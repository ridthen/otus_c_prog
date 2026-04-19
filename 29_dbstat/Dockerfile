FROM mariadb:10.11

USER root
RUN apt-get update && \
    apt-get -y install \
    libmariadb-dev libmariadb-dev-compat build-essential cmake

COPY main.c CMakeLists.txt /app/

RUN echo "Сборка dbstat" \
    && cd /app \
    && cmake . \
    && cmake --build . \
