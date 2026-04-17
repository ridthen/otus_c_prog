## ringbuf_driver - драйвер символьного устройства

### Сборка
Сборка драйвера
```bash
make
```
Сборка тестового приложения
```bash
gcc -Wall -Wextra -Wpedantic -std=c11 -o test_ringbuf test_ringbuf.c
```
### Загрузка модуля

```bash
sudo insmod ringbuf_driver.ko
```

### Тестирование драйвера
Проверка существования устройства
```bash
$ sudo dmesg | tail -1
[ 4537.161523] Ring buffer driver loaded (size=16)
$ ls -lh /dev/ringbuf 
crw------- 1 root root 237, 0 Apr 17 13:50 /dev/ringbuf
```
Запуск теста

Тестовое приложение сначала пишет строки "Hello" и ", ring buffer world!" в 
символьное устройство, затем читает кольцевой буфер, в котором оказывается последние
записанные 16 байт: "ng buffer world!". 

Потом три раза пытается записать строку "1_3_5_7_9_11_14_16_18". Из буфера считывается
16 байт. Затем тестовое приложение увеличивает размер буфера до 54 байт и опять три
раза отправляет последнюю строку и считывает уже 54 байта. Также тестовое приложение
читает dmesg и видит в нём информацию об изменении размера буфера.
```bash
$ sudo ./test_ringbuf 
[OK] Устройство подключено
[OK] Запись двух строк
Отправлено: Hello
Отправлено: , ring buffer world!
Прочитано 16 байт: "ng buffer world!"
[OK] Записано 21 * 3 байт
Прочитано 16 байт: "_7_9_11_14_16_18"
[OK] Размер буфера изменен через sysfs
[ 1837.642787] Ring buffer driver loaded (size=16)
[ 2561.459218] Buffer resized to 512 bytes
[ 2995.662268] Buffer resized to 512 bytes
[ 3063.747898] Buffer resized to 512 bytes
[ 3218.276350] Buffer resized to 16 bytes
[ 3287.204250] Buffer resized to 18 bytes
[ 3965.490159] Buffer resized to 54 bytes
[ 4520.279728] Ring buffer driver unloaded
[ 4537.161523] Ring buffer driver loaded (size=16)
[ 5429.185184] Buffer resized to 54 bytes
[OK] Записано 21 * 3 байт
Прочитано 54 байт: "_11_14_16_181_3_5_7_9_11_14_16_181_3_5_7_9_11_14_16_18"
```
Выгрузка драйвера
```bash
sudo rmmod ringbuf_driver
$ sudo dmesg | tail -1
[ 5494.468173] Ring buffer driver unloaded
```