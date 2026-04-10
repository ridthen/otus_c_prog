## log_reader статистика логов веб-сервера

Читает каждый лог-файл отдельным потоком. Для подсчета байт и количества рефералов используются структуры HashMap.

### Сборка
```bash
cmake .
cmake --build .
```

### Использование

Приложение принимает в качестве аргументов командной строки путь к директории с логами и количество потоков обработки.

```bash
% ls ~/Downloads/19_threads_homework-12926-1514cc
19-threads-homework.pdf access.log.2            access.log.3            access.log.4            access.log.5            access.log.6            access.log.7            access.log.8            access.log.9            logs.7z
% ./17_log_reader ~/Downloads/19_threads_homework-12926-1514cc 10
Всего байт: 279204971448

Топ 10 URI по трафику:
42504752962 /sitemap.xml
2133096622 /best/
1776085244 /
1153080316 /new/
1078666002 /стоят-пассажиры-в-аэропорту-посадочный-досмотр/?p=4
853984744 /да-алё-да-да-ну-как-там-с-деньгами/
763750570 /женщина-и-её-любовник-находились-в-доме-пока-муж/
648695810 /встречались-с-девушкой-уже-2-года-был-секс-и/
464625796 /крутой-джазовый-бар-проводит-конкурс/
458865158 /-девушка-ваши-родители-случайно-не-охотники-нет/

Топ 10 рефералов:
9332970 -
1200038 https://www.google.com/
414808 https://yandex.ru/
246342 https://www.google.ru/
194708 https://baneks.site/best/
100254 https://baneks.site/new/
84240 http://yandex.ru/searchapp?text=
74830 https://baneks.site/
64942 https://baneks.site/best/?p=2
49456 https://www.google.com.ua/
```