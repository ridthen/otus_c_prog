#ifndef NETCDF_OUTPUT_H
#define NETCDF_OUTPUT_H

#include "spectrum.h"

/**
 * @brief Записывает структуру Spectrum в существующий или создает новый netcdf файл
 * @param filename Строка с именем файла
 * @param spec Структура Spectrum
 * @param time_seconds Временная отметка от начала эксперимента
 * @param is_first Устанавливается 1, если это первая запись в файл и его нужно создать
 * @return
 */
int write_spectrum_to_netcdf(const char *filename, const Spectrum *spec, double time_seconds, int is_first);

#endif
