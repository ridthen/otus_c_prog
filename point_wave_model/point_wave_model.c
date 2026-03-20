#include <stdio.h>
#include <math.h>

#include "spectrum.h"
#include "source_terms.h"
#include "integrator.h"
#include "netcdf_output.h"

#define AIR_DENSITY 1.225     // плотность воздуха
#define WATER_DENSITY 1025.0  // плотность воды
#define XFR 1.1               // параметр логарифмической сетки
#define GAMMA 3.3             // пиковый фактор JONSWAP

void run_duration_limited_test(Spectrum *spec, WindForcing *wind,
                                ST6Params *params, double duration);
void print_progress(double t, double duration, double Hs, double fp);

int main(void) {
    // сетка
    const double F_MIN = 0.04;       // минимальная частота [Гц]
    const double F_MAX = 1.00;       // максимальная частота [Гц]
    const int NDIR = 36;             // число направлений
    // начальные условия
    double Hs0 = 0.5;                // начальная значительная высота [м]
    double fp0 = 0.2;                // начальная пиковая частота [Гц]
    // ветровой форсинг
    WindForcing wind;
    wind.u10 = 15.0;                 // скорость ветра 15, [м/с]
    wind.dir = 0.0;                  // направление ветра, [рад]
    wind.rho_air = AIR_DENSITY;
    wind.rho_water = WATER_DENSITY;
    // продолжительность теста
    double duration = 3600.0 * 24;   // продолжительность теста, [с]

    int NFREQ = (int)floor(log(F_MAX / F_MIN) / log(XFR)) + 1; // 34

    Spectrum *spec = spectrum_create(NFREQ, NDIR, F_MIN);
    // TODO: унифицировать
    if (!spec) {
        fprintf(stderr, "Ошибка создания спектра\n");
        return 1;
    }
    spectrum_init_jonswap(spec, Hs0, fp0, GAMMA);

    printf("Инициализация спектра:\n");
    printf("  Начальная Hs: %.2f м\n", Hs0);
    printf("  Начальная fp: %.3f Гц\n", fp0);
    printf("  Сетка: %d частот x %d направлений\n", spec->nfreq, NDIR);

    printf("\nВетровой форсинг:\n");
    printf("  Скорость ветра U10: %.1f м/с\n", wind.u10);
    printf("  Направление ветра: %.1f град\n", wind.dir * 180.0 / M_PI);

    ST6Params params;
    st6_params_default(&params);

    printf("\nЗапуск duration-limited теста на %.1f часов...\n\n",
           duration / 3600.0);

    run_duration_limited_test(spec, &wind, &params, duration);

    // высота волны и пиковая частота в конце симуляции
    double Hs_final = spectrum_Hs(spec);
    double fp_final = spec->peak_freq;

    printf("\n============================================================\n");
    printf("Результаты после %.1f часов:\n", duration / 3600.0);
    printf("  Значительная высота Hs: %.2f м (рост в %.1f раз)\n",
           Hs_final, Hs_final / Hs0);
    printf("  Пиковая частота fp: %.3f Гц (сдвиг на %.1f%%)\n",
           fp_final, (fp0 - fp_final) / fp0 * 100);
    printf("============================================================\n");

    spectrum_destroy(spec);

    return 0;
}

void run_duration_limited_test(Spectrum *spec, WindForcing *wind,
                                ST6Params *params, double duration) {

    const double output_interval = 1800.0;
    double t = 0.0;

    // dt
    TimeStepControl control;
    time_step_control_default(&control);
    const double dt = compute_adaptive_dt(spec, &control);
    printf("Шаг интегрирования dt: %.2f\n", dt);

    // Запись начального спектра
    int ret = write_spectrum_to_netcdf("output.nc", spec, t, 1);
    if (ret != 0) {
        fprintf(stderr, "Ошибка записи начального спектра в NetCDF\n");
    }

    printf("  t = %6.1f s | Hs = %6.3f m | fp = %6.3f Hz\n",
           t, spectrum_Hs(spec), spec->peak_freq);

    while (t < duration) {
        double dt_used = integrator_step(spec, wind, params, EULER_FORWARD, dt);
        t += dt_used;

        if (fmod(t, output_interval) < dt_used) {
            // запись спектра каждые 30 минут
            ret = write_spectrum_to_netcdf("output.nc", spec, t, 0);
            if (ret != 0) {
                fprintf(stderr, "Ошибка записи спектра в NetCDF при t = %.2f\n", t);
            }
            print_progress(t, duration, spectrum_Hs(spec), spec->peak_freq);
        }
    }
}

void print_progress(double t, double duration, double Hs, double fp) {
    int percent = (int)(100.0 * t / duration);
    printf("\r  [%3d%%] t = %6.1f s | Hs = %6.3f m | fp = %6.3f Hz",
           percent, t, Hs, fp);
    fflush(stdout);
}
