# Не просто создаём образ, но даём ему имя build
FROM gcc:11.3 as build

RUN apt update && \
    apt install -y \
      python3-pip \
      cmake \
    && \
    pip3 install conan==1.59.0

# Запуск conan
COPY conanfile.txt /app/
RUN mkdir /app/build && cd /app/build && \
    conan install .. --build=missing -s compiler.libcxx=libstdc++11 -s build_type=Release

# Копируем все необходимые компоненты
COPY ./client /app/client
COPY ./monitor /app/monitor
COPY ./server /app/server
COPY ./logger /app/logger
COPY CMakeLists.txt /app/

RUN cd /app/build && \
    cmake -DCMAKE_BUILD_TYPE=Release .. && \
    cmake --build . -j12

# Второй контейнер в том же докерфайле
FROM ubuntu:22.04 as run

# Создадим пользователя admin
RUN groupadd -r admin && useradd -mrg admin admin
USER admin

# Скопируем приложение со сборочного контейнера в директорию /app, а также все библиотеки
COPY --from=build /app/build/lib/* /app/
COPY --from=build /app/build/bin/monitor /app/
# COPY --from=build /app/build/bin/client /app/ (нужно будет раскомментировать)
# COPY --from=build /app/build/bin/server /app/ (нужно будет раскомментировать)

# добавим переменную окружения, чтобы библиотеки были видны
ENV LD_LIBRARY_PATH=/app/

# Запускаем мониторинг
ENTRYPOINT ["/app/monitor"]
