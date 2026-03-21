#include "source_terms.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

// TODO: вынести все параметры и константы в одно место
#define G 9.81
#define VON_KARMAN 0.41
#define AIR_DENSITY 1.225
#define WATER_DENSITY 1025.0
#define FLT_MIN 1.0e-38

void st6_params_default(ST6Params *params) {
    params->sin6ws = 32.0;
    params->a1 = 5.7e-7;      // для UL4M4
    params->a2 = 8.0e-6;      // для UL4M4
    params->L = 4.0;
    params->M = 4.0;
    params->Bnt = 0.001225;   // (0.035)^2
    params->norm_type = 1;     // 1 = нормировка по E_T
    params->z0 = 0.0001;
}

/**
 * Расчет фазовой скорости волны для большой глубины
 * @param f Частота, [Гц]
 * @return c фазовая скорость, [м/c]
 */
static double phase_speed(double f) {
    return G / (2.0 * M_PI * f);
}

/**
 * Расчет скорости трения
 * @param u10 Скорость ветра на высоте 10м, [м/с]
 * @param z0 Параметр шероховатости, [м]
 * @return u* скорость трения, [м/с]
 */
static double friction_velocity(double u10, double z0) {
    return u10 * VON_KARMAN / log(10.0 / z0);
}


/**
 * @brief Вспомогательная функция для интегрирования вклада в напряжение Rogers et. al 2012 (12)
 * @param S_arr массив значений (например, S_x или S_y)
 * @param cinv массив обратной фазовой скорости 1/C
 * @param dsii массив ширины частотных интервалов
 * @param nk10 размер массивов
 * @param rho_water плотность воды
 * @param g ускорение свободного падения
 * @return вычисленное напряжение
 */
static double integrate_stress(const double *S_arr, const double *cinv,
                               const double *dsii, int nk10,
                               double rho_water, double g) {
    double sum = 0.0;
    for (int i = 0; i < nk10; i++) {
        sum += S_arr[i] * cinv[i] * dsii[i];
    }
    return sum * rho_water * g;
}

static double norm_stress_excess(const double *S_in_x, const double *S_in_y, const double *cinv,
                               const double *dsii, int nk10, double rho_water, double g, const double tau_v_x,
                               const double tau_v_y, const double tau_tot) {
    double tau_wx, tau_wy;     // (tau_norm) init wave supported stress (x,y-comp.) Rogers et. al 2012 (12)
    double tau_x, tau_y;       // Начальные tau_wx+tau_v_x, tau_wy+tau_v_y
    double tau_norm;           // (tau_norm) Начальный total stress (input) без редукции
    
    tau_wx = integrate_stress(S_in_x, cinv, dsii, nk10, rho_water, g);
    tau_wy = integrate_stress(S_in_y, cinv, dsii, nk10, rho_water, g);
    tau_x = tau_v_x + tau_wx;
    tau_y = tau_v_y + tau_wy;
    tau_norm = sqrt(tau_x * tau_x + tau_y * tau_y);
    // Доля излишка tau_norm в напряжении
    return (tau_norm - tau_tot) / tau_tot;
}

/**
 * @brief Применение физического ограничения на нормальное напряжение
 * @param spec спектр (частоты, шаги, количество)
 * @param wind параметры ветра (u10, rho_air, rho_water, ustar, wind_dir)
 * @param params параметры ST6 (sin6ws для UPROXY)
 * @param dS_in двумерный массив предварительного ветрового входа
 *
 * @details Реализация следует алгоритму LFACTOR из WW3 (Rogers et al. 2012):
 *          - расширение спектра до 10 Гц с экстраполяцией S_in ~ f^(-2)
 *          - вычисление компонент напряжения по направлениям
 *          - итеративный подбор параметра R_tau мультипликативным методом
 *          - применение редукции L(f) = min(1, exp((1 - UPROXY/C) * R_tau))
 */
static void apply_stress_constraint2(const Spectrum *spec, const WindForcing *wind,
                                    const ST6Params *params, double **dS_in) {
    double u10 = wind->u10;
    double ustar = friction_velocity(u10, params->z0);
    double wind_dir = wind->dir;
    double g = 9.81;
    double rho_air = wind->rho_air;      // Плотность воздуха
    double rho_water = wind->rho_water;  // Плотность воды
    double xfr = 1.1;                    // Коэффициент увеличения частоты

    int nk = spec->nfreq;                // Число частот в расчётной сетке
    double *freq = spec->freq;           // Частоты из спектра[Гц]

    int i, j;
    int nk10;                            // Количество частот в расширенной до 10Гц сетке
    double f_max_tail = 10.0;            // Upper freq. limit to extrapolate to.
    double tau_v;                        // Viscous stress
    double cv;                           // Коэффициент для viscous stress. Rogers et. al 2012 (15)
    double tau_v_x, tau_v_y;             // Компонент viscous stress
    double tau_tot;                      // (tau_tot) Total stress. Rogers et. al 2012 (13)
    double uproxy;                       // Scaled friction velocity
    double err_init;                     // Доля излишка tau_norm в напряжении (tau_norm - tau_tot) / tau_tot

    // расчет ненаправленного S_in и его компонентов x, y
    double *S_omni = calloc(nk, sizeof(double)); // ненаправленный S_in
    double *S_x    = calloc(nk, sizeof(double)); // x-компонента S_in
    double *S_y    = calloc(nk, sizeof(double)); // y-компонента S_in

    if (!S_omni || !S_x || !S_y) {
        printf("Ошибка выделения памяти: S_omni, S_x, S_y\n");
        if (S_omni) free(S_omni);
        if (S_x) free(S_x);
        if (S_y) free(S_y);
        return;
    }

    for (i = 0; i < nk; i++) {
        double sum = 0.0, sum_x = 0.0, sum_y = 0.0;
        for (j = 0; j < spec->ndir; j++) {
            double theta = spec->dir[j];
            double val = dS_in[i][j];
            sum   += val;
            sum_x += val * cos(theta);
            sum_y += val * sin(theta);
        }
        S_omni[i] = sum * spec->dtheta;         // интеграл S_in dtheta
        S_x[i]    = sum_x * spec->dtheta;       // интеграл S_in cos(theta) dtheta
        S_y[i]    = sum_y * spec->dtheta;       // интеграл S_in sin(theta) dtheta
    }

    // Подготовка расширенной сетки до 10 Гц
    nk10 = (int)ceil(log(f_max_tail / freq[0]) / log(xfr)) + 1; // ceil(log(10.0/0.04) / log(1.1)) + 1
    // printf("nk10: %i\n", nk10);
    if (nk10 < nk) nk10 = nk;

    double *freq10   = malloc(nk10 * sizeof(double)); // Частоты до 10Гц
    double *cinv10   = malloc(nk10 * sizeof(double)); // Inverse phase speed 1/C(sigma)
    double *dsii10   = malloc(nk10 * sizeof(double)); // Frequency bandwiths [in rad.]
    double *S_omni10 = malloc(nk10 * sizeof(double)); // Ненаправленный расширенный до 10Гц S_in
    double *S_x10    = malloc(nk10 * sizeof(double)); // x-компонента S_omni10
    double *S_y10    = malloc(nk10 * sizeof(double)); // y-компонента S_omni10
    double *ucinv10  = malloc(nk10 * sizeof(double)); // (1-U/C) на частотах до 10Гц
    double *lfact10  = malloc(nk10 * sizeof(double)); // correction factor

    if (!freq10 || !cinv10 || !dsii10 || !S_omni10 || !S_x10 || !S_y10 || !ucinv10 || !lfact10) {
        printf("Ошибка выделения памяти: freq10, cinv10, dsii10, S_omni10, S_x10, S_y10, ucinv10, lfact10\n");
        free(S_omni); free(S_x); free(S_y);
        if (freq10) free(freq10);
        if (cinv10) free(cinv10);
        if (dsii10) free(dsii10);
        if (S_omni10) free(S_omni10);
        if (S_x10) free(S_x10);
        if (S_y10) free(S_y10);
        if (ucinv10) free(ucinv10);
        if (lfact10) free(lfact10);
        return;
    }

    for (i = 0; i < nk10; i++) {
        freq10[i] = freq[0] * pow(xfr, i);
    }

    // Исходные значения для первых nk частот
    for (i = 0; i < nk; i++) {
        S_omni10[i] = S_omni[i];
        S_x10[i]    = S_x[i];
        S_y10[i]    = S_y[i];
        double c = g / (2.0 * M_PI * freq10[i]);
        cinv10[i] = 1.0 / c;
    }

    // Экстраполяция спектра пропорционально f^(-2) до 10 Гц
    // https://github.com/NOAA-EMC/WW3/blob/57f0705566b97aafcf7ab10c3f698514c641f450/model/src/w3src6md.F90#L929
    for (i = nk; i < nk10; i++) {
        double ratio = freq10[nk-1] / freq10[i]; // f_nk/f_i -> 0
        double ratio2 = ratio * ratio;
        S_omni10[i] = S_omni[nk-1] * ratio2;
        S_x10[i]    = S_x[nk-1]    * ratio2;
        S_y10[i]    = S_y[nk-1]    * ratio2;
        double c = g / (2.0 * M_PI * freq10[i]);
        cinv10[i] = 1.0 / c;
    }

    // Ширина полосы для логарифмической сетки (DSII frequency bandwidths)
    for (i = 0; i < nk10; i++) {
        if (i == 0) { // первое значение
            dsii10[i] = 0.5 * (freq10[1] - freq10[0]);
        } else if (i == nk10 - 1) { // последнее
            dsii10[i] = 0.5 * (freq10[i] - freq10[i-1]);
        } else {
            dsii10[i] = 0.5 * (freq10[i+1] - freq10[i-1]);
        }
    }

    // Расчёт напряжений
    uproxy = params->sin6ws * ustar;
    for (i = 0; i < nk10; i++) {
        ucinv10[i] = 1.0 - uproxy * cinv10[i]; // Rogers et. al 2012 (18)
    }

    tau_tot = ustar * ustar * rho_air; // Rogers et. al 2012 (13)

    cv = -5e-5 * u10 + 1.1e-3; // Rogers et. al 2012 (15)
    if (cv < 0.0) cv = 0.0;
    tau_v = cv * u10 * u10 * rho_air; // Rogers et. al 2012 (14)
    if (tau_v > 0.95 * tau_tot) tau_v = 0.95 * tau_tot;
    tau_v_x = tau_v * cos(wind_dir);
    tau_v_y = tau_v * sin(wind_dir);

    // Начальные напряжения без редукции
    err_init = norm_stress_excess(S_x10, S_y10,
        cinv10, dsii10, nk10, rho_water, g, tau_v_x, tau_v_y, tau_tot);

    // Инициализация lfact10
    for (i = 0; i < nk10; i++) lfact10[i] = 1.0;

    // We only modify the wind-input term if tau_norm > tau_total - tau_viscous
    if (err_init > 0) {
        double err;                         // доля излишка tau_norm
        double Rtau, dRtau;                 // параметр редукции
        double L;                           // коэффициент редукции
        int sign_old, sign_new, overshot, iter;
        double *S_x_red, *S_y_red;          // Редуцированные компоненты x,y S_in
        const int ITER_MAX = 80;

        Rtau = err_init / 90.0;
        dRtau = 2.0;
        sign_old = (err_init > 0) ? 1 : -1;
        overshot = 0;

        for (iter = 0; iter < ITER_MAX; iter++) {
            // факторы редукции для всей сетки
            for (i = 0; i < nk10; i++) {
                L = exp(ucinv10[i] * Rtau); // ucinv10 = 1-U/C; Rogers et. al 2012 (18)
                if (L > 1.0) L = 1.0; // Rogers et. al 2012 (17)
                lfact10[i] = L;
            }

            // память для редуцированных S_in
            S_x_red = (double*)malloc(nk10 * sizeof(double));
            S_y_red = (double*)malloc(nk10 * sizeof(double));
            if (!S_x_red || !S_y_red) {
                printf("Ошибка выделения памяти S_x_red, S_y_red\n");
                if (S_x_red) free(S_x_red);
                if (S_y_red) free(S_y_red);
                break;
            }

            // найденный L для компонентов спектральной функции S_in
            for (i = 0; i < nk10; i++) {
                S_x_red[i] = S_x10[i] * lfact10[i];
                S_y_red[i] = S_y10[i] * lfact10[i];
            }

            // tau_norm для редуцированных S_in
            err = norm_stress_excess(S_x_red, S_y_red, cinv10, dsii10, nk10,
                rho_water, g, tau_v_x, tau_v_y, tau_tot);

            free(S_x_red);
            free(S_y_red);

            sign_new = (err > 0) ? 1 : -1;
            if (sign_new != sign_old) overshot = 1;
            if (overshot) {
                dRtau = 0.5 * (1.0 + dRtau);
                if (dRtau < 1.0001) dRtau = 1.0001;
            }

            Rtau = Rtau * pow(dRtau, sign_new); // умножить или разделить на dRtau
            sign_old = sign_new;

            if (fabs(err) < 1.5e-4) break;
        }

        // финальные lfact10 к исходным S_in по частотам и направлениям
        for (i = 0; i < nk; i++) {
            L = lfact10[i];
            // printf("L: %.2f\n", L);
            for (j = 0; j < spec->ndir; j++) {
                dS_in[i][j] *= L;
            }
        }
    }

    free(S_omni);
    free(S_x);
    free(S_y);
    free(freq10);
    free(cinv10);
    free(dsii10);
    free(S_omni10);
    free(S_x10);
    free(S_y10);
    free(ucinv10);
    free(lfact10);
}

/**
 * @brief Source term for wind input
 *
 * @param spec Спектр (используются freq, nfreq, df, ndir, dtheta)
 * @param wind Параметры ветра (u10, rho_air, rho_water)
 * @param params Эмпирические коэффициенты и константы
 * @param dS_in dS_in двумерный массив предварительного ветрового входа
 *
 * @details
 * References:
 * - Rogers, W. E., A. V. Babanin, and D. W. Wang, 2012: Observation-Consistent Input and Whitecapping Dissipation
 *   in a Model for Wind-Generated Surface Waves: Description and Simple Calculations. J. Atmos. Oceanic Technol.,
 *   29, 1329–1346, https://doi.org/10.1175/JTECH-D-11-00092.1.
 * - https://github.com/NOAA-EMC/WW3/blob/57f0705566b97aafcf7ab10c3f698514c641f450/model/src/w3src6md.F90#L289
 */
void source_term_wind_input(Spectrum *spec, const WindForcing *wind,
                            const ST6Params *params, double **dS_in) {
    double wind_dir = wind->dir;                          // угол направление ветра
    double rho_ratio = wind->rho_air / wind->rho_water;   // отношение плотностей воздух/вода
    double u10 = wind->u10;                               // скорость ветра
    double u_star = friction_velocity(u10, params->z0);   // скорость трения
    double u_scaled = u_star * params->sin6ws;            // proxy wind speed

    for (int i = 0; i < spec->nfreq; i++) {
        double f = spec->freq[i];               // частота из спектра
        double sigma = 2.0 * M_PI * f;          // угловая частота, рад/с
        double c = phase_speed(f);              // фазовая скорость
        double k = sigma * sigma / G;           // wave number
        double Cg = 0.5 * c;                    // group speed

        if (c < 0.1) {
            for (int j = 0; j < spec->ndir; j++) dS_in[i][j] = 0.0;
            continue;
        }

        // maximum value of the spectrum at that frequency
        // (9) Rogers et al. 2012
        double maxE = -1.0;
        double E_omni = 0.0;
        for (int j = 0; j < spec->ndir; j++) {
            if (spec->energy[i][j] > maxE) maxE = spec->energy[i][j];
            E_omni += spec->energy[i][j];
        }
        if (maxE <= FLT_MIN) {
            for (int j = 0; j < spec->ndir; j++) dS_in[i][j] = 0.0;
            continue;
        }

        // A(f) spectral narrowness (spreading function)
        // (8, 10) Rogers et al. 2012
        double A_f_int = 0.0;
        for (int j = 0; j < spec->ndir; j++) {
            double En = spec->energy[i][j] / maxE;
            A_f_int += En * spec->dtheta;
        }
        double A = 1.0 / A_f_int; // spectral narrowness

        // omnidirectional spectrum Ef
        E_omni *= spec->dtheta;

        // the spectral saturation
        // (7) Rogers et al. 2012
        double Bn = (1.0/(2.0*M_PI)) * E_omni * k*k*k * Cg;
        double Bn_prime = A * Bn; // spectral saturation
        double sqrtBn = sqrt(Bn_prime);
        if (sqrtBn < 1e-10) sqrtBn = 1e-10;

        for (int j = 0; j < spec->ndir; j++) {
            double theta = spec->dir[j];
            double cos_diff = cos(theta - wind_dir);
            if (cos_diff <= FLT_MIN) {
                dS_in[i][j] = 0.0;
                continue;
            }

            // W(f,theta) the wind forcing parameter
            // (6) Rogers et al. 2012
            double arg = u_scaled / c * cos_diff - 1.0;
            double W = (arg > 0.0) ? arg*arg : 0.0;

            // the sheltering coefficient, which accounts for the reduction of atmosphere-to-wave momentum transfer
            // resulting from full airflow separation (degree of flow separation)
            // (5) Rogers et al. 2012
            double shelter = 2.8 - (1.0 + tanh(10.0 * sqrtBn * W - 11.0));

            // (4) Rogers et al. 2012
            double gamma = shelter * sqrtBn * W;
            // (3) Rogers et al. 2012
            double Bterm = gamma * sigma * rho_ratio;
            // (2) Rogers et al. 2012
            dS_in[i][j] = Bterm * spec->energy[i][j];
        }
    }

    // int nf = spec->nfreq;
    // double *S_in_omni = calloc(nf, sizeof(double));
    // for (int i = 0; i < nf; i++) {
    //     double sum = 0.0;
    //     for (int j = 0; j < spec->ndir; j++) {
    //         sum += dS_in[i][j];
    //     }
    //     S_in_omni[i] = sum * spec->dtheta;
    // }

    apply_stress_constraint2(spec, wind, params, dS_in);

    // free(S_in_omni);
}

/**
 * @brief Рассчитывает сток энергии при разрушении волн (whitecapping dissipation)
 * @param spec Указатель на структуру Spectrum
 * @param params Параметры, использованные в ST6 WW3
 * @param dS_ds Массив для результатов
 */
void source_term_dissipation(Spectrum *spec, const ST6Params *params, double **dS_ds) {
    double g = 9.81;
    double B_nt = params->Bnt;          // эмпирическая константа (0.035^2)
    int nf = spec->nfreq;
    int ndir = spec->ndir;
    double dtheta = spec->dtheta;

    double *E_omni = malloc(nf * sizeof(double));
    double *E_T    = malloc(nf * sizeof(double));
    double *ratio  = malloc(nf * sizeof(double));
    double *accum  = calloc(nf, sizeof(double));

    // TODO: унифицировать
    if (!E_omni || !E_T || !ratio || !accum) {
        printf("Ошибка выделения памяти в source_term_dissipation\n");
        free(E_omni); free(E_T); free(ratio); free(accum);
        return;
    }

    // одномерный спектр E_omni(f) и порог E_T(f)
    for (int i = 0; i < nf; i++) {
        double f = spec->freq[i];
        double omega = 2.0 * M_PI * f;
        double k = omega * omega / g;         // волновое число
        double Cg = 0.5 * g / omega;          // group velocity

        double sum = 0.0;
        for (int j = 0; j < ndir; j++) {
            sum += spec->energy[i][j];
        }
        E_omni[i] = sum * dtheta; // интеграл по dtheta

        // пороговая плотность (19) с A=1
        E_T[i] = (2.0 * M_PI * B_nt) / (Cg * k * k * k);
    }

    // максимальное значение E_omni для защиты при нормировке
    double maxE = 0.0;
    for (int i = 0; i < nf; i++) {
        if (E_omni[i] > maxE) maxE = E_omni[i];
    }
    double eps = 1e-5 * maxE;

    // превышение и нормированное значение
    for (int i = 0; i < nf; i++) {
        double Delta = E_omni[i] - E_T[i];
        if (Delta < 0.0) Delta = 0.0;

        if (params->norm_type == 0) {        // нормировка по E(f)
            if (E_omni[i] > eps) {
                ratio[i] = Delta / E_omni[i];
            } else {
                ratio[i] = 0.0;
            }
        } else {                              // нормировка по E_T(f)
            if (E_T[i] > 0.0) {
                ratio[i] = Delta / E_T[i];
            } else {
                ratio[i] = 0.0;
            }
        }
    }

    // накопленный интеграл для T2
    for (int i = 0; i < nf; i++) {
        double integrand = pow(ratio[i], params->M);
        if (i == 0) {
            accum[i] = integrand * spec->dsii[i];
        } else {
            accum[i] = accum[i-1] + integrand * spec->dsii[i];
        }
    }

    for (int i = 0; i < nf; i++) {
        double f = spec->freq[i];
        double T1_coef = params->a1 * f * pow(fmax(ratio[i], 0.0), params->L);
        double T2_coef = params->a2 * accum[i];

        for (int j = 0; j < ndir; j++) {
            double E = spec->energy[i][j]; // спектральная плотность энергии м^2/(Гц*рад)
            double T1 = T1_coef * E;
            double T2 = T2_coef * E;
            dS_ds[i][j] = -(T1 + T2); // м^2/(Гц*рад)
        }
    }

    free(E_omni);
    free(E_T);
    free(ratio);
    free(accum);
}


/**
 * @brief Суммирование источников и стоков энергии
 * @param spec Структура спектра
 * @param wind Параметры ветра
 * @param params Эмпирические коэффициенты и константы
 * @param dS_total Сумма источников и стоков, м^2/(Гц*рад*c)
 */
void source_term_total(Spectrum *spec, const WindForcing *wind,
                       const ST6Params *params, double **dS_total) {

    // использовать заранее выделенные массивы в point_wave_model.c
    // TODO: добавить проверку на выделение dS_in, dS_ds
    double **dS_in = (double**)malloc(spec->nfreq * sizeof(double*));
    double **dS_ds = (double**)malloc(spec->nfreq * sizeof(double*));
    
    for (int i = 0; i < spec->nfreq; i++) {
        dS_in[i] = (double*)calloc(spec->ndir, sizeof(double));
        dS_ds[i] = (double*)calloc(spec->ndir, sizeof(double));
    }
    
    // ветровой вход и обрушение
    source_term_wind_input(spec, wind, params, dS_in);
    source_term_dissipation(spec, params, dS_ds);
    
    // суммирование источников и стоков
    for (int i = 0; i < spec->nfreq; i++) {
        for (int j = 0; j < spec->ndir; j++) {
            dS_total[i][j] = dS_in[i][j] + dS_ds[i][j]; // м^2/(Гц*рад*c)
        }
    }

    for (int i = 0; i < spec->nfreq; i++) {
        free(dS_in[i]);
        free(dS_ds[i]);
    }
    free(dS_in);
    free(dS_ds);
}
