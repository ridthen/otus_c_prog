#include "netcdf_output.h"
#include <netcdf.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#define HANDLE_ERROR(err) \
    if (err != NC_NOERR) { fprintf(stderr, "NetCDF error: %s\n", nc_strerror(err)); return err; }

/**
 * @brief Записывает структуру Spectrum в существующий или создает новый netcdf файл
 * @param filename Строка с именем файла
 * @param spec Структура Spectrum
 * @param time_seconds Временная отметка от начала эксперимента
 * @param is_first Устанавливается 1, если это первая запись в файл и его нужно создать
 * @return
 */
int write_spectrum_to_netcdf(const char *filename, const Spectrum *spec, double time_seconds, int is_first) {
    int ncid;
    int dimid_freq, dimid_dir, dimid_time;
    int varid_freq, varid_dir, varid_time, varid_espec, varid_fullspec;
    size_t time_len;
    int ret;

    // если это первая запись в файл
    if (is_first) {
        ret = nc_create(filename, NC_CLOBBER, &ncid);
        HANDLE_ERROR(ret);

        // Dimensions
        ret = nc_def_dim(ncid, "FullWaveDirectionalSpectra_Frequency", spec->nfreq, &dimid_freq);
        HANDLE_ERROR(ret);
        ret = nc_def_dim(ncid, "FullWaveDirectionalSpectra_Direction", spec->ndir, &dimid_dir);
        HANDLE_ERROR(ret);
        ret = nc_def_dim(ncid, "time", NC_UNLIMITED, &dimid_time);
        HANDLE_ERROR(ret);

        // Coordinates
        ret = nc_def_var(ncid, "FullWaveDirectionalSpectra_Frequency", NC_DOUBLE, 1, &dimid_freq, &varid_freq);
        HANDLE_ERROR(ret);
        ret = nc_def_var(ncid, "FullWaveDirectionalSpectra_Direction", NC_DOUBLE, 1, &dimid_dir, &varid_dir);
        HANDLE_ERROR(ret);
        ret = nc_put_att_text(ncid, varid_dir, "units", 7, "degrees");
        HANDLE_ERROR(ret);

        // Time
        ret = nc_def_var(ncid, "time", NC_DOUBLE, 1, &dimid_time, &varid_time);
        HANDLE_ERROR(ret);
        ret = nc_put_att_text(ncid, varid_time, "units", 30, "seconds since 1970-01-01 00:00:00");
        HANDLE_ERROR(ret);

        // Всенаправленный спектр EnergySpectra (time, frequency)
        int dims_espec[2] = {dimid_time, dimid_freq};
        ret = nc_def_var(ncid, "EnergySpectra", NC_DOUBLE, 2, dims_espec, &varid_espec);
        HANDLE_ERROR(ret);

        // Направленный спектр FullWaveDirectionalSpectra_Energy (time, direction, frequency)
        int dims_full[3] = {dimid_time, dimid_dir, dimid_freq};
        ret = nc_def_var(ncid, "FullWaveDirectionalSpectra_Energy", NC_DOUBLE, 3, dims_full, &varid_fullspec);
        HANDLE_ERROR(ret);

        ret = nc_enddef(ncid);
        HANDLE_ERROR(ret);

        // Запись сетки
        ret = nc_put_var_double(ncid, varid_freq, spec->freq);
        HANDLE_ERROR(ret);

        double *dirs_deg = malloc(spec->ndir * sizeof(double));
        if (!dirs_deg) return NC_ENOMEM;
        for (int j = 0; j < spec->ndir; j++) {
            dirs_deg[j] = spec->dir[j] * 180.0 / M_PI; // радианы в градусы
        }
        ret = nc_put_var_double(ncid, varid_dir, dirs_deg);
        free(dirs_deg);
        HANDLE_ERROR(ret);

        ret = nc_close(ncid);
        HANDLE_ERROR(ret);
    }

    // Если нужно дописать в файл
    ret = nc_open(filename, NC_WRITE, &ncid);
    HANDLE_ERROR(ret);

    ret = nc_inq_dimid(ncid, "time", &dimid_time);
    HANDLE_ERROR(ret);
    ret = nc_inq_dimlen(ncid, dimid_time, &time_len);
    HANDLE_ERROR(ret);
    size_t time_idx = time_len;

    ret = nc_inq_varid(ncid, "time", &varid_time);
    HANDLE_ERROR(ret);
    ret = nc_inq_varid(ncid, "EnergySpectra", &varid_espec);
    HANDLE_ERROR(ret);
    ret = nc_inq_varid(ncid, "FullWaveDirectionalSpectra_Energy", &varid_fullspec);
    HANDLE_ERROR(ret);

    ret = nc_put_var1_double(ncid, varid_time, &time_idx, &time_seconds);
    HANDLE_ERROR(ret);

    // Вычисление всенаправленного спектра
    double *energy_omni = malloc(spec->nfreq * sizeof(double));
    if (!energy_omni) return NC_ENOMEM;
    for (int i = 0; i < spec->nfreq; i++) {
        double sum = 0.0;
        for (int j = 0; j < spec->ndir; j++) {
            sum += spec->energy[i][j];
        }
        energy_omni[i] = sum * spec->dtheta; // интеграл по dtheta
    }

    // Запись среза EnergySpectra (time, freq)
    size_t start_espec[2] = {time_idx, 0};
    size_t count_espec[2] = {1, (size_t)spec->nfreq};
    ret = nc_put_vara_double(ncid, varid_espec, start_espec, count_espec, energy_omni);
    free(energy_omni);
    HANDLE_ERROR(ret);

    // Подготовка буфера для направленного спектра в порядке (time, dir, freq) с быстрым изменением freq
    double *full_buf = malloc(spec->ndir * spec->nfreq * sizeof(double));
    if (!full_buf) return NC_ENOMEM;
    for (int j = 0; j < spec->ndir; j++) {
        for (int i = 0; i < spec->nfreq; i++) {
            full_buf[j * spec->nfreq + i] = spec->energy[i][j];
        }
    }

    // Запись среза FullWaveDirectionalSpectra_Energy (time, dir, freq)
    size_t start_full[3] = {time_idx, 0, 0};
    size_t count_full[3] = {1, (size_t)spec->ndir, (size_t)spec->nfreq};
    ret = nc_put_vara_double(ncid, varid_fullspec, start_full, count_full, full_buf);
    free(full_buf);
    HANDLE_ERROR(ret);

    ret = nc_close(ncid);
    HANDLE_ERROR(ret);

    return 0;
}
