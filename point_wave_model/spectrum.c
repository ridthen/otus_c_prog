#include "spectrum.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#define G 9.81
#define XFR 1.1

/**
 * @brief Создание и инициализация спектра с логарифмической шкалой
 * @param nfreq Количество частот в логарифмической сетке
 * @param ndir Количество бинов по направлениям
 * @param f_min Минимальная частота
 * @return
 */
Spectrum* spectrum_create(int nfreq, int ndir, double f_min) {
    // int nk = (int)floor(log(f_max / f_min) / log(XFR)) + 1;
    // TODO: добавить проверки на полученные значения nfreq, ndir
    int nk = nfreq;
    if (nk < 2) nk = 2;  // минимум две частоты

    Spectrum *spec = (Spectrum*)malloc(sizeof(Spectrum));
    if (!spec) return NULL;

    spec->nfreq = nk;
    spec->ndir = ndir;
    spec->dtheta = 2.0 * M_PI / ndir;
    spec->peak_freq = f_min;

    // TODO: проверки на выделение памяти
    spec->freq = (double*)malloc(nk * sizeof(double));
    spec->dir = (double*)malloc(ndir * sizeof(double));
    spec->dsii = (double*)malloc(nk * sizeof(double));
    spec->energy = (double**)malloc(nk * sizeof(double*));
    for (int i = 0; i < nk; i++) {
        spec->energy[i] = (double*)calloc(ndir, sizeof(double));
    }

    // Заполнение частот (логарифмическая шкала)
    for (int i = 0; i < nk; i++) {
        spec->freq[i] = f_min * pow(XFR, i);
    }

    // Вычисление ширины интервалов dsii [Гц]
    for (int i = 0; i < nk; i++) {
        if (i == 0) { // первое
            spec->dsii[i] = 0.5 * (spec->freq[1] - spec->freq[0]);
        } else if (i == nk - 1) { // последнее
            spec->dsii[i] = 0.5 * (spec->freq[i] - spec->freq[i-1]);
        } else { // все остальные
            spec->dsii[i] = 0.5 * (spec->freq[i+1] - spec->freq[i-1]);
        }
    }

    // Направления от -PI до PI
    for (int j = 0; j < ndir; j++) {
        spec->dir[j] = -M_PI + j * spec->dtheta;
    }

    return spec;
}

/**
 * Удаляет структуру Spectrum
 * @param spec
 */
void spectrum_destroy(Spectrum *spec) {
    if (!spec) return;
    for (int i = 0; i < spec->nfreq; i++) {
        free(spec->energy[i]);
    }
    free(spec->energy);
    free(spec->freq);
    free(spec->dir);
    free(spec->dsii);
    free(spec);
}

/**
 * @brief Инициализация спектра по параметризации JONSWAP
 * @details
 * Используется параметризация спектра JONSWAP в форме (2.4.1):
 * <br>
 *   E(f) = alpha g^2 (2PI)^{-4} f^{-5} exp[ -5/4 (f/fp)^{-4} ] gamma^{exp[ -(f-fp)^2/(2sigma^2 fp^2) ]}
 * <br>,
 * где:<br>
 * alpha - постоянная Филлипса; (0.0081),<br>
 * gamma - пиковый фактор (peak enhancement factor); (3.3),<br>
 * sigma = sigma_a for f <= fp, sigma_b for f > fp; (0.07, 0.09 соответственно) - ширина пика слева и справа<br>
 * Средние значения alpha, gamma, sigma: (2.4.4, 2.4.5)<br>
 * References:
 *  - Hasselman K. et. al. Measurements of Wind-Wave Growth and Swell Decay during the Joint North Sea Wave Project"
 *  Dt. Hydrogr. Z., Reihe A (8), Nr. 12, 1973.
 *
 * @param spec Подготовленная структура Spectrum
 * @param Hs Значительная высота волны
 * @param fp Пиковая частота
 * @param gamma Эмпирический параметр
 */
void spectrum_init_jonswap(Spectrum *spec, double Hs, double fp, double gamma) {
    // TODO: вынести
    const double alpha_phillips = 0.0081;  // постоянная Филлипса
    const double sigma_a = 0.07;           // левая полуширина пика
    const double sigma_b = 0.09;           // правая полуширина пика
    double g = 9.81;                       // гравитационная постоянная

    // ненормированный спектр (только форма)
    for (int i = 0; i < spec->nfreq; i++) {
        double f = spec->freq[i];
        if (f <= 0.0) continue;

        double r = f / fp;
        double sigma = (f <= fp) ? sigma_a : sigma_b;
        // Hasselman K. et. al. 1973 (2.4.1)
        double exp_arg = - (r - 1.0) * (r - 1.0) / (2.0 * sigma * sigma);
        double gamma_factor = pow(gamma, exp(exp_arg));

        // E(f) = alpha g^2 (2PI)^{-4} f^{-5} exp[ -5/4 (f/fp)^{-4} ] gamma^{exp[ -(f-fp)^2/(2sigma^2 fp^2) ]}
        // Hasselman K. et. al. 1973 (2.4.1)
        double E_f = alpha_phillips * g * g * pow(2.0 * M_PI, -4.0) * pow(f, -5.0) *
                     exp(-1.25 * pow(r, -4.0)) * gamma_factor;

        // Угловое распределение E_f
        // Since most of the wave energy is concentrated near the peak we have assumed for simplicity
        // a frequency independent cos^2(theta) spreading function in computing net source  functions,
        // nonlinear transfer rates, etc. Hasselman K. et. al. 1973 p. 39
        for (int j = 0; j < spec->ndir; j++)
        {
            double theta = spec->dir[j];
            double dir_dist = 0.0;
            if (fabs(theta) <= M_PI / 2.0)
            {
                dir_dist = (2.0 / M_PI) * pow(cos(theta), 2.0);
            }
            spec->energy[i][j] = E_f * dir_dist;
        }
    }

    // интегрирование полной энергии спектра по частоте и направлениям, чтобы потом сравнить с полученной из Hs
    double E_total = 0.0;
    for (int i = 0; i < spec->nfreq; i++) {
        double sum_dir = 0.0;
        for (int j = 0; j < spec->ndir; j++) {
            sum_dir += spec->energy[i][j];
        }
        E_total += sum_dir * spec->dtheta * spec->dsii[i];
    }

    // масштабирование созданного спектра E(f, theta), чтобы достичь нужной Hs
    // Hs = 4 * sqrt(E_total)
    double target_E = (Hs / 4.0) * (Hs / 4.0);
    double scale = target_E / E_total;

    for (int i = 0; i < spec->nfreq; i++) {
        for (int j = 0; j < spec->ndir; j++) {
            spec->energy[i][j] *= scale;
        }
    }

    spectrum_update_peak(spec);
}

/**
 * @brief Расчет значительной высоты волны через спектр энергии
 * @param spec Указатель на структуру Spectrum
 * @return
 */
double spectrum_Hs(const Spectrum *spec) {
    double E_total = 0.0;
    for (int i = 0; i < spec->nfreq; i++) {
        double sum_dir = 0.0;
        for (int j = 0; j < spec->ndir; j++) {
            sum_dir += spec->energy[i][j];
        }
        E_total += sum_dir * spec->dtheta * spec->dsii[i];
    }
    return 4.0 * sqrt(E_total);
}

/**
 * Поиск и обновление пиковой частоты в Spectrum
 * @param spec указатель на структуру Spectrum
 */
void spectrum_update_peak(Spectrum *spec) {
    double max_E = 0.0;
    double f_peak = spec->freq[0];
    for (int i = 0; i < spec->nfreq; i++) {
        double E_omni = 0.0;
        for (int j = 0; j < spec->ndir; j++) {
            E_omni += spec->energy[i][j];
        }
        E_omni *= spec->dtheta;  // плотность по частоте (интеграл по направлениям)
        if (E_omni > max_E) {
            max_E = E_omni;
            f_peak = spec->freq[i];
        }
    }
    spec->peak_freq = f_peak;
}
