#include "integrator.h"
#include <stdlib.h>

/**
 * Инициализация параметров вычисления шага интегрирования
 * @param control структура TimeStepControl в которую запишется результат
 */
void time_step_control_default(TimeStepControl *control) {
    control->dt = 60.0;         // базовый шаг
    control->dt_max = 300.0;    // максимальный шаг
    control->cfl_factor = 0.5;  // минимальный шаг
    control->min_energy = 1.0e-10;
}

/**
 * @brief Вычисление адаптивного шага на основе максимальной частоты
 *
 * @param spec структура Spectrum
 * @param control структура TimeStepControl
 * @return шаг интегрирования по времени
 */
double compute_adaptive_dt(const Spectrum *spec, const TimeStepControl *control) {
    double f_max = spec->freq[spec->nfreq - 1];
    double dt_cfl = control->cfl_factor / f_max; // 0.5/0.93
    //printf("f_max: %.2f\n", f_max);

    if (dt_cfl > control->dt_max) {
        return control->dt_max;
    }
    return dt_cfl;
}

/**
 * @brief Реализация метода Эйлера: поиск функции через шаги по времени,
 * используя ее производную в начальной точке
 * @details dy/dt = f(t,y), y(t0) = y0,
 * y_(n+1) = y_n +dt * f(t_n, y_n)
 * При этом следующим шагом по времени n+1 будет вычисляемый, обновленный спектр
 *
 * @param spec Структура спектра
 * @param wind Параметры ветра
 * @param params Эмпирические коэффициенты и константы
 * @param dt шаг интегрирования
 */
static void euler_step(Spectrum *spec, const WindForcing *wind,
                       const ST6Params *params, double dt) {
    // TODO: добавить проверку на выделение dS[i][j]
    double **dS = malloc(spec->nfreq * sizeof(double*)); // сумма source terms, м^2/(Гц*рад*c)
    for (int i = 0; i < spec->nfreq; i++) {
        dS[i] = (double*)malloc(spec->ndir * sizeof(double));
    }

    source_term_total(spec, wind, params, dS);

    for (int i = 0; i < spec->nfreq; i++) {
        for (int j = 0; j < spec->ndir; j++) {
            // y_(n+1) = y_n +dt * f(t_n, y_n)
            double new_energy = spec->energy[i][j] + dS[i][j]*dt; // м^2/(Гц*рад)
            if (new_energy < 0.0) new_energy = 0.0;
            spec->energy[i][j] = new_energy;
        }
    }

    for (int i = 0; i < spec->nfreq; i++) free(dS[i]);
    free(dS);
}

double integrator_step(Spectrum *spec, const WindForcing *wind,
                       const ST6Params *params, IntegratorType type,
                       const double dt) {
    // double dt = compute_adaptive_dt(spec, control)

    switch (type) {
    case EULER_FORWARD:
        euler_step(spec, wind, params, dt);
        break;
    default:
        euler_step(spec, wind, params, dt);
        break;
    }

    spectrum_update_peak(spec);
    return dt;
}
