#ifndef INTEGRATOR_H
#define INTEGRATOR_H

#include "spectrum.h"
#include "source_terms.h"

typedef enum {
    EULER_FORWARD,
    RK4
} IntegratorType;

typedef struct {
    double dt;
    double dt_max;
    double cfl_factor;
    double min_energy;
} TimeStepControl;

void time_step_control_default(TimeStepControl *control);

double compute_adaptive_dt(const Spectrum *spec, const TimeStepControl *control);

double integrator_step(Spectrum *spec, const WindForcing *wind,
                       const ST6Params *params, IntegratorType type, double dt);

#endif /* INTEGRATOR_H */
