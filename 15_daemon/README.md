## filesizer - демон для отслеживания размера файла

### Сборка
Для сборки должна быть установлена библиотека libyaml или libyaml-dev
```bash
cmake .
cmake --build .
```

### Конфигурация

Конфигурация демона ведется через YAML-файл, в котором указано две опции:
```yaml
file_path: /tmp/testfile # путь к отслеживаему файлу
socket_path: /tmp/filesizer.sock # путь к сокету
```
Демон при запуске может принимать аргументы командной строки:

| Аргумент | Значение                                                  |
|----------|-----------------------------------------------------------|
| -d       | Демонизировать сервер. По умолчанию сервер работает в fg  |
| -с       | Путь к файлу конфигурации (по умолчанию: ./config.yaml)   |


### Использование
Запуск демона:
```bash
./filesizer -d
```
Запуск произойдет без лишних сообщений и демон можно понаблюдать в списке процессов:
```bash
 % ps aux | grep filesizer
cutter           98044   0.0  0.0 435300176   1376 s001  S+    1:16PM   0:00.00 grep filesizer
cutter           97940   0.0  0.0 435296000   1104   ??  S     1:15PM   0:00.00 ./filesizer -d
```
Для тестирования можно использовать net cat:
```bash
% cd /tmp
 /tmp % ls -l
total 8
srwxr-xr-x@ 1 cutter  wheel    0 Apr  7 13:15 filesizer.sock
-rw-r--r--@ 1 cutter  wheel   35 Apr  7 12:50 testfile
 /tmp % cat testfile 
bla bla
lsdkjf
sldjkfslkdjfsldkjfs
 /tmp % ls -lh testfile 
-rw-r--r--@ 1 cutter  wheel    35B Apr  7 12:50 testfile
 /tmp % nc -U /tmp/filesizer.sock 
35
 /tmp % echo "bla bla bla" >> testfile 
 /tmp % nc -U /tmp/filesizer.sock     
47
```
В случае отсутствия файла testfile демон запустится, но при обращении к сокету будет в сокет отдавать строку "error: No such file or directory"
