#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#define DEV_PATH "/dev/ringbuf"

int main(void)
{
	int fd;
	char buf[256];
	const char *data1 = "Hello";
	const char *data2 = ", ring buffer world!";
	const char *data3 = "1_3_5_7_9_11_14_16_18";
	ssize_t n;

	fd = open(DEV_PATH, O_RDWR);
	if (fd < 0) {
		perror("open");
		return EXIT_FAILURE;
	}
	printf("[OK] Устройство подключено\n");

	/* Запись данных */
	if (write(fd, data1, strlen(data1)) < 0) {
		perror("write1");
		goto err;
	}
	if (write(fd, data2, strlen(data2)) < 0) {
		perror("write2");
		goto err;
	}
	printf("[OK] Запись двух строк\n");

	printf("Отправлено: %s\n", data1);
	printf("Отправлено: %s\n", data2);

	/* Чтение всех данных */
	memset(buf, 0, sizeof(buf));
	lseek(fd, 0, SEEK_SET);
	n = read(fd, buf, sizeof(buf) - 1);
	if (n < 0) {
		perror("read");
		goto err;
	}
	buf[n] = '\0';
	printf("Прочитано %zd байт: \"%s\"\n", n, buf);

	/* Проверка кольцевого поведения */
	lseek(fd, 0, SEEK_SET);
	for (int i = 0; i < 3; i++) {
		if (write(fd, data3, strlen(data3)) < 0) {
			perror("write in loop");
			goto err;
		}
	}
	printf("[OK] Записано %zd * 3 байт\n", strlen(data3));

	lseek(fd, 0, SEEK_SET);
	memset(buf, 0, sizeof(buf));
	n = read(fd, buf, sizeof(buf) - 1);
	if (n < 0) {
		perror("read after overflow");
		goto err;
	}
	buf[n] = '\0';
	printf("Прочитано %zd байт: \"%s\"\n", n, buf);

	/* Изменение размера буфера */
	system("echo 54 | sudo tee /sys/module/ringbuf_driver/parameters/buf_size > /dev/null");
	printf("[OK] Размер буфера изменен через sysfs\n");
	system("sudo dmesg | tail -10");

	/* Вновь проверка кольцевого поведения */
	lseek(fd, 0, SEEK_SET);
	for (int i = 0; i < 3; i++) {
		if (write(fd, data3, strlen(data3)) < 0) {
			perror("write in loop");
			goto err;
		}
	}
	printf("[OK] Записано %zd * 3 байт\n", strlen(data3));

	lseek(fd, 0, SEEK_SET);
	memset(buf, 0, sizeof(buf));
	n = read(fd, buf, sizeof(buf) - 1);
	if (n < 0) {
		perror("read after overflow");
		goto err;
	}
	buf[n] = '\0';
	printf("Прочитано %zd байт: \"%s\"\n", n, buf);

	close(fd);
	return EXIT_SUCCESS;

err:
	close(fd);
	return EXIT_FAILURE;
}

