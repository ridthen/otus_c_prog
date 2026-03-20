#ifndef SPECTRUM_H
#define SPECTRUM_H

typedef struct {
    int nfreq;           // число частотных бинов
    int ndir;            // число направлений
    double *freq;        // массив частот [Гц] (логарифмическая шкала)
    double *dir;         // массив направлений [рад]
    double **energy;     // 2D массив энергии [nfreq][ndir]
    double *dsii;        // ширина частотных интервалов [Гц]
    double dtheta;       // шаг по направлению
    double peak_freq;    // пиковая частота
} Spectrum;

Spectrum* spectrum_create(int nfreq, int ndir, double f_min);

void spectrum_destroy(Spectrum *spec);

void spectrum_init_jonswap(Spectrum *spec, double Hs, double fp, double gamma);

double spectrum_Hs(const Spectrum *spec);

void spectrum_update_peak(Spectrum *spec);

#endif /* SPECTRUM_H */
