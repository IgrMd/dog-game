cmake_minimum_required(VERSION 3.11)

project(game_server CXX)
set(CMAKE_CXX_STANDARD 20)

include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup(TARGETS)

find_package(Boost 1.83.0 REQUIRED)

# find_package(Libpqxx 7.7.4 REQUIRED)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

# === Статические библиотеки
# Библиотека обработки коллизий
add_library(collision_detection_lib STATIC
    src/collision/collision_detector.cpp
    src/collision/collision_detector.h
)

# Библиотека модели
add_library(model_lib STATIC
    src/model/geom.h
    src/model/loot_generator.cpp
    src/model/loot_generator.h
    src/model/model.cpp
    src/model/model.h
    src/model/model_serialization.cpp
    src/model/model_serialization.h)

target_include_directories(model_lib PUBLIC
    CONAN_PKG::boost
    Threads::Threads)

target_link_libraries(model_lib
    CONAN_PKG::boost
    Threads::Threads
    collision_detection_lib
    postgres_lib)

# Библиотека приложения
add_library(application_lib STATIC
    src/app/app.cpp
    src/app/app.h
    src/app/player.cpp
    src/app/player.h
    src/app/unit_of_work.h
)

# Библиотека БД
add_library(postgres_lib STATIC
    src/util/tagged_uuid.cpp
    src/util/tagged_uuid.h
    src/db/postgres.cpp
    src/db/postgres.h
    src/db/connection_pool.h)

target_include_directories(postgres_lib PUBLIC
    CONAN_PKG::boost
    CONAN_PKG::libpq
    CONAN_PKG::libpqxx)

target_link_libraries(postgres_lib
    application_lib
    CONAN_PKG::boost
    CONAN_PKG::libpq
    CONAN_PKG::libpqxx)

target_link_libraries(application_lib
    model_lib)

# === Испоняемые файлы ===
# game_server
add_executable(game_server
    src/http/http_server.cpp
    src/http/http_server.h
    src/http/request_handler.cpp
    src/http/request_handler.h
    src/tools/cmd_parser.h
    src/tools/logger.cpp
    src/tools/logger.h
    src/tools/ticker.h
    src/json/extra_data.h
    src/json/boost_json.cpp
    src/json/json_loader.h
    src/json/json_loader.cpp
    src/util/tagged.h
    src/main.cpp
    src/sdk.h
)

# game_server_tests
add_executable(game_server_tests
    tests/model-tests.cpp
    tests/loot_generator_tests.cpp
)

# collision_detection_tests
add_executable(collision_detection_tests
    tests/collision-detector-tests.cpp
)

# state_serialization_tests
add_executable(state_serialization_tests
    tests/state-serialization-tests.cpp
)

# Зависимости целей от статических библиотек.
target_link_libraries(game_server
    model_lib
    application_lib
    collision_detection_lib)


target_link_libraries(game_server_tests
    CONAN_PKG::catch2
    model_lib)

target_link_libraries(collision_detection_tests
    CONAN_PKG::catch2
    collision_detection_lib)

target_link_libraries(state_serialization_tests
    CONAN_PKG::catch2
    model_lib
    application_lib)

# Подключаем CTest
include(CTest)
include(${CONAN_BUILD_DIRS_CATCH2}/Catch.cmake)
catch_discover_tests(game_server_tests)
catch_discover_tests(collision_detection_tests)
catch_discover_tests(state_serialization_tests)