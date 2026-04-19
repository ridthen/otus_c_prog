## 29_dbstat - статистика таблицы базы данных MariaDB

Запуск базы данных, компиляция и запуск клиента происходит в Docker.

Контейнер базы данных при запуске инициализируется из дампа `db_dumps/oscar.
sql`.

Для контейнера клиента используется такой же контейнер, как и для базы 
данных, но в него доустанавливаются библиотеки с заголовками mariadb и 
компилятор.

```bash
docker-compose up
[+] Building 59.7s (11/11) FINISHED
 => [internal] load local bake definitions                                                                                                                                                                          0.0s
 => => reading from stdin 522B                                                                                                                                                                                      0.0s
 => [internal] load build definition from Dockerfile                                                                                                                                                                0.0s
 => => transferring dockerfile: 308B                                                                                                                                                                                0.0s
 => [internal] load metadata for docker.io/library/mariadb:10.11                                                                                                                                                    0.0s
 => [internal] load .dockerignore                                                                                                                                                                                   0.0s
 => => transferring context: 2B                                                                                                                                                                                     0.0s
 => [1/4] FROM docker.io/library/mariadb:10.11@sha256:407fccb51e710f34752c1a3ef9b936d1f55f38d4ac7fa043b3759742d266fd9a                                                                                              0.0s
 => => resolve docker.io/library/mariadb:10.11@sha256:407fccb51e710f34752c1a3ef9b936d1f55f38d4ac7fa043b3759742d266fd9a                                                                                              0.0s
 => [internal] load build context                                                                                                                                                                                   0.0s
 => => transferring context: 5.70kB                                                                                                                                                                                 0.0s
 => [2/4] RUN apt-get update &&     apt-get -y install     libmariadb-dev libmariadb-dev-compat build-essential cmake                                                                                              51.8s
 => [3/4] COPY main.c CMakeLists.txt /app/                                                                                                                                                                          0.0s
 => [4/4] RUN echo "Сборка dbstat"     && cd /app     && cmake .     && cmake --build .                                                                                                                             0.2s
 => exporting to image                                                                                                                                                                                              7.4s
 => => exporting layers                                                                                                                                                                                             6.1s
 => => exporting manifest sha256:e7be587c3c1d57690d5f863137925184028ac0035f4cbf2a5631f702a77368ab                                                                                                                   0.0s
 => => exporting config sha256:59af718c88c36b635a3f1d8aea031977575e20f5a6a7b4cce40b9818cc531e7f                                                                                                                     0.0s
 => => exporting attestation manifest sha256:965a6ca64bec045d017af80a20b262ffdc093a99aa75e33363b0cc8625639bbd                                                                                                       0.0s
 => => exporting manifest list sha256:fc8654a366833ab7de9a3a981c8e4b4165e1d22fe94caf7bf9c84b56d1d00f7c                                                                                                              0.0s
 => => naming to docker.io/library/29_dbstat-db_client:latest                                                                                                                                                           0.0s
 => => unpacking to docker.io/library/29_dbstat-db_client:latest                                                                                                                                                        1.3s
 => resolving provenance for metadata file                                                                                                                                                                          0.0s
[+] up 4/4
 ✔ Image 29_dbstat-db_client    Built                                                                                                                                                                                   59.8s
 ✔ Network 29_db_default    Created                                                                                                                                                                                 0.0s
 ✔ Container mariadb_server Created                                                                                                                                                                                 0.3s
 ✔ Container db_client      Created                                                                                                                                                                                 0.0s
Attaching to db_client, mariadb_server
mariadb_server  | 2026-04-19 10:18:49+00:00 [Note] [Entrypoint]: Entrypoint script for MariaDB Server 1:10.11.16+maria~ubu2204 started.
mariadb_server  | 2026-04-19 10:18:49+00:00 [Note] [Entrypoint]: MariaDB upgrade not required
mariadb_server  | 2026-04-19 10:18:49 0 [Warning] Setting lower_case_table_names=2 because file system for /var/lib/mysql/ is case insensitive
mariadb_server  | 2026-04-19 10:18:49 0 [Note] Starting MariaDB 10.11.16-MariaDB-ubu2204 source revision 3218602d3100db9ce7a875511a591cddc173cc16 server_uid wMxWawHn14vMbW6iBRMTeOhXS+s= as process 1
mariadb_server  | 2026-04-19 10:18:49 0 [Note] InnoDB: Compressed tables use zlib 1.2.11
mariadb_server  | 2026-04-19 10:18:49 0 [Note] InnoDB: Number of transaction pools: 1
mariadb_server  | 2026-04-19 10:18:49 0 [Note] InnoDB: Using ARMv8 crc32 + pmull instructions
mariadb_server  | 2026-04-19 10:18:49 0 [Warning] mariadbd: io_uring_queue_init() failed with EPERM: sysctl kernel.io_uring_disabled has the value 2, or 1 and the user of the process is not a member of sysctl kernel.io_uring_group. (see man 2 io_uring_setup).
mariadb_server  | create_uring failed: falling back to libaio
mariadb_server  | 2026-04-19 10:18:49 0 [Note] InnoDB: Using Linux native AIO
mariadb_server  | 2026-04-19 10:18:49 0 [Note] InnoDB: innodb_buffer_pool_size_max=128m, innodb_buffer_pool_size=128m
mariadb_server  | 2026-04-19 10:18:49 0 [Note] InnoDB: Completed initialization of buffer pool
mariadb_server  | 2026-04-19 10:18:49 0 [Note] InnoDB: Buffered log writes (block size=512 bytes)
mariadb_server  | 2026-04-19 10:18:49 0 [Note] InnoDB: End of log at LSN=81384
mariadb_server  | 2026-04-19 10:18:49 0 [Note] InnoDB: 128 rollback segments are active.
mariadb_server  | 2026-04-19 10:18:49 0 [Note] InnoDB: Setting file './ibtmp1' size to 12.000MiB. Physically writing the file full; Please wait ...
mariadb_server  | 2026-04-19 10:18:49 0 [Note] InnoDB: File './ibtmp1' size is now 12.000MiB.
mariadb_server  | 2026-04-19 10:18:49 0 [Note] InnoDB: log sequence number 81384; transaction id 201
mariadb_server  | 2026-04-19 10:18:49 0 [Note] Plugin 'FEEDBACK' is disabled.
mariadb_server  | 2026-04-19 10:18:49 0 [Note] InnoDB: Loading buffer pool(s) from /var/lib/mysql/ib_buffer_pool
mariadb_server  | 2026-04-19 10:18:49 0 [Warning] 'innodb-file-format' was removed. It does nothing now and exists only for compatibility with old my.cnf files.
mariadb_server  | 2026-04-19 10:18:49 0 [Warning] 'innodb-large-prefix' was removed. It does nothing now and exists only for compatibility with old my.cnf files.
mariadb_server  | 2026-04-19 10:18:49 0 [Warning] You need to use --log-bin to make --expire-logs-days or --binlog-expire-logs-seconds work.
mariadb_server  | 2026-04-19 10:18:49 0 [Note] Server socket created on IP: '0.0.0.0', port: '3306'.
mariadb_server  | 2026-04-19 10:18:49 0 [Note] Server socket created on IP: '::', port: '3306'.
mariadb_server  | 2026-04-19 10:18:49 0 [Note] InnoDB: Buffer pool(s) load completed at 260419 10:18:49
mariadb_server  | 2026-04-19 10:18:49 0 [Note] mariadbd: ready for connections.
mariadb_server  | Version: '10.11.16-MariaDB-ubu2204'  socket: '/run/mysqld/mysqld.sock'  port: 3306  mariadb.org binary distribution
db_client       | Количество обработанных числовых значений: 89
db_client       | Сумма:    3215
db_client       | Среднее:  36.1236
db_client       | Минимум:  21
db_client       | Максимум: 80
db_client       | Дисперсия: 136.4
db_client exited with code 0
Gracefully Stopping... press Ctrl+C again to force
Container db_client Stopping
Container db_client Stopped
Container mariadb_server Stopping
mariadb_server  | 2026-04-19 10:19:00 0 [Note] mariadbd (initiated by: unknown): Normal shutdown
mariadb_server  | 2026-04-19 10:19:00 0 [Note] InnoDB: FTS optimize thread exiting.
mariadb_server  | 2026-04-19 10:19:00 0 [Note] InnoDB: Starting shutdown...
mariadb_server  | 2026-04-19 10:19:00 0 [Note] InnoDB: Dumping buffer pool(s) to /var/lib/mysql/ib_buffer_pool
mariadb_server  | 2026-04-19 10:19:00 0 [Note] InnoDB: Buffer pool(s) dump completed at 260419 10:19:00
mariadb_server  | 2026-04-19 10:19:01 0 [Note] InnoDB: Removed temporary tablespace data file: "./ibtmp1"
mariadb_server  | 2026-04-19 10:19:01 0 [Note] InnoDB: Shutdown completed; log sequence number 81384; transaction id 203
mariadb_server  | 2026-04-19 10:19:01 0 [Note] mariadbd: Shutdown complete
Container mariadb_server Stopped
mariadb_server exited with code 0
```

Для подключения к серверу и инспекции базы данных можно использовать:
```bash
% docker-compose exec -it db_server mysql -u root -p
```

Опции клиента
```bash
./dbstat -d база -t таблица -c колонка [-h хост] [-u пользователь] [-p пароль]
```

Запуск запроса
```bash
./29_dbstat -h 127.0.0.1 -u ap -p appasswd -d movies -t oscar -c age
```
Внутри контейнера с клиентом работает внутренний DNS, поэтому в команде 
запуска в docker-compose.yml указан хост mariadb_server. 
