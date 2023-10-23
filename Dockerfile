# Создать образ на основе базового слоя gcc (там будет ОС и сам компилятор).
# 11.4 — используемая версия gcc.
FROM gcc:11.4 as build

# Выполнить установку зависимостей внутри контейнера.
RUN apt update && \
    apt install -y \
      python3-pip \
      cmake \
    && \
    pip3 install conan==1.*

# копируем conanfile.txt в контейнер и запускаем conan install
COPY conanfile.txt /app/
RUN mkdir /app/build && cd /app/build && \
    conan install .. -s compiler.libcxx=libstdc++11 -s build_type=Release \
     --build=missing

# только после этого копируем остальные иходники
COPY ./src /app/src
COPY ./tests /app/tests
COPY CMakeLists.txt /app/

# новая команда для сборки сервера:
RUN cd /app/build && \
    cmake -DCMAKE_BUILD_TYPE=Release .. && \
    cmake --build .

# Второй контейнер в том же докерфайле
FROM ubuntu:22.04 as run

# Создадим пользователя www
RUN groupadd -r www && useradd -r -g www www
USER www

COPY --from=build /app/build/bin/game_server /app/
COPY ./data /app/data
COPY ./static /app/static

# Запускаем игровой сервер
ENTRYPOINT ["/app/game_server",\
            "-c", "app/data/config.json",\
            "-w", "app/static/"]