# point_wave_model

## Упрощенная исследовательская спектральная модель прогноза морского волнения

Существует несколько используемых и широко распространенных спектральных моделей прогноза
морского волнения: WaveWatch III, SWAN, WAM6. Каждая из них разрабатывается с начала 1980-x,
реализована на Fortran и имеет сложную и очень развитую структуру, множество схем расчета, 
выработанных за десятилетия. Запуск этих моделей предполагается через OpenMP, MPICH на 
распределенных HPC кластерах с сотнями и тысячами процессорных ядер. Модификация таких 
моделей для проведения численных экспериментов, добавления расчета и проверки новых схем 
физических процессов выглядит большой и сложной задачей. Целью данного проекта является 
разработка упрощенной модели прогноза морского волнения для одной точки, без расчета по сетке,
без использования множества источников и стоков  энергии (source terms).

В качестве основы для этого проекта будет использована статья Rogers et. Al 2012 и реализация
на Fortran в пакете ST6 модели WAVEWATCH III:

1. Rogers, W. E., A. V. Babanin, and D. W. Wang, 2012: Observation-Consistent Input and Whitecapping Dissipation in a Model for Wind-Generated Surface Waves: Description and Simple Calculations. J. Atmos. Oceanic Technol., 29, 1329–1346,
2. https://doi.org/10.1175/JTECH-D-11-00092.1. https://github.com/NOAA-EMC/WW3/blob/57f0705566b97aafcf7ab10c3f698514c641f450/model/src/w3src6md.F90#L289

### Зависимости:
* Библиотека netcdf

В MacOS: `brew install netcdf`

В Debian GNU/Linux: `sudo apt install libnetcdf22 libnetcdf-dev`

### Сборка:
```
cmake .
cmake --build .
make
```

### Запуск
```
./point_wave_model
```
