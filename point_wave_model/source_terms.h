#ifndef SOURCE_TERMS_H
#define SOURCE_TERMS_H

#include "spectrum.h"

typedef struct {
    double u10;        // скорость ветра на высоте 10 м [м/с]
    double dir;        // направление ветра [рад]
    double rho_air;    // плотность воздуха [кг/м^3]
    double rho_water;  // плотность воды [кг/м^3]
} WindForcing;

typedef struct {
    // Параметры ветрового воздействия
    double sin6ws;     // масштаб для пересчёта u* в прокси-скорость

    // Параметры диссипации (по Rogers et al. 2012)
    double a1;         // коэффициент локальной диссипации T1
    double a2;         // коэффициент кумулятивной диссипации T2
    double L;          // показатель степени для T1
    double M;          // показатель степени для T2
    double Bnt;        // квадрат порогового параметра Bnt
    int norm_type;     // тип нормировки: 0 – по E(f), 1 – по E_T(f)

    double z0;         // шероховатость поверхности
} ST6Params;

void st6_params_default(ST6Params *params);

void source_term_wind_input(Spectrum *spec, const WindForcing *wind,
                            const ST6Params *params, double **dS_in);

void source_term_dissipation(Spectrum *spec,
                             const ST6Params *params, double **dS_ds);

void source_term_total(Spectrum *spec, const WindForcing *wind,
                       const ST6Params *params, double **dS_total);

#endif /* SOURCE_TERMS_H */
