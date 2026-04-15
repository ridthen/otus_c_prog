## 23_http_server - http-сервер на базе kqueue 

### Сборка
```bash
cmake .
cmake --build .
```

### Использование

Сервер принимает в качестве аргументов командной строки:
* -d - директория, файлы из которой будут доступны для чтения по http
* -l - адрес сокета, куда привяжется сервер

```bash
./23_http_server -d /Users/cutter/otus_c_prog/23_http_server/files -l 127.0.0.1:8080
```

### Тестирование

 ```bash
 % brew install wrk
 % wrk -t4 -c100 -d30s http://localhost:8080/Chalikov2022.pdf
Running 30s test @ http://localhost:8080/Chalikov2022.pdf
  4 threads and 100 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency   214.51us  609.68us  30.50ms   99.79%
    Req/Sec     4.36k   411.54     4.91k    97.04%
  131908 requests in 30.10s, 46.21GB read
  Socket errors: connect 0, read 65, write 0, timeout 0
Requests/sec:   4382.21
Transfer/sec:      1.54GB
 ```
