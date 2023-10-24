#include "sdk.h"
//
#include "./json/extra_data.h"
#include "./json/json_loader.h"
#include "./http/request_handler.h"
#include "./model/model_serialization.h"
#include "./tools/cmd_parser.h"
#include "./tools/logger.h"
#include "./tools/ticker.h"

#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>

#include <cstdlib>
#include <iostream>
#include <syncstream>
#include <thread>

namespace {

using namespace std::literals;
namespace net = boost::asio;
namespace sys = boost::system;


template <typename Fn>
void RunWorkers(unsigned thread_count, const Fn& fn) {
    thread_count = std::max(1u, thread_count);
    std::vector<std::jthread> workers;
    workers.reserve(thread_count - 1);
    while (--thread_count) {
        workers.emplace_back(fn);
    }
    fn();
}

struct POSIXSignal {
    POSIXSignal() = delete;
    static std::string_view ToStr(int signal_number) {
        switch (signal_number) {
        case SIGINT:
            return SIGINT_;
        case SIGTERM:
            return SIGTERM_;
        default:
            break;
        }
        return {};
    }

private:
    static constexpr std::string_view SIGINT_ = "SIGINT"sv;
    static constexpr std::string_view SIGTERM_ = "SIGTERM"sv;
};

void AddSignalsHandler(net::io_context& ioc, net::signal_set& signals){
    signals.async_wait([&ioc](const sys::error_code& ec, [[maybe_unused]] int signal_number) {
        if (!ec) {
            std::osyncstream os{std::cout};
            server_logging::LogStop(0);
            ioc.stop();
        }
    });
}
constexpr const char DB_URL_ENV_NAME[]{"GAME_DB_URL"};

std::string GetDbURLFromEnv() {
    if (const auto* url = std::getenv(DB_URL_ENV_NAME)) {
        return std::string{url};
    } else {
        throw std::runtime_error(DB_URL_ENV_NAME + " environment variable not found"s);
    }
    return {};
}

void StartServer(const cmd_parser::Args& args) {
    // 1. Загружаем карту из файла и строим модель игры
    auto input_json = json_loader::LoadJsonData(args.config_path);
    auto [game, extra_data] = json_loader::LoadGame(input_json);
    game.SetRandomSpawn(args.randomize_spawn_points);
    app::AppConfig conf {
        .db_url = GetDbURLFromEnv(),
        .num_threads = std::thread::hardware_concurrency()
        // .num_threads = 2 // in cloud we have only 2 threads
        // .num_threads = 1
    };
    app::Application app(game, conf);

    // 1.1 загружаем сохраненное состояние игры
    serialization::AppSerializator app_serializator(app, game, args.state_file_path, args.has_state_file_path);
    app_serializator.Restore();

    // 1.2 Добавляем обработчик автосохранения
    if (args.has_save_state_period) {
        auto save_period = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::duration<size_t, std::milli>(args.save_state_period));
        app.AddListener(std::make_unique<serialization::SerializingListener>(app_serializator, save_period));
    }

    // 2. Инициализируем io_context
    net::io_context ioc(conf.num_threads);

    // 3. Добавляем асинхронный обработчик сигналов SIGINT и SIGTERM
    net::signal_set signals(ioc, SIGINT, SIGTERM);
    AddSignalsHandler(ioc, signals);

    // 4. Создаём обработчик HTTP-запросов и связываем его с моделью игры
    // Обработчик API
    http_handler::ApiHandler api_handler(app, extra_data);
    // strand для выполнения запросов к API
    auto api_strand = net::make_strand(ioc);
    //запуск таймера
    if (args.has_tick_period) {
        app.TimeTickerUsed();
        auto ticker = std::make_shared<Ticker>(api_strand, std::chrono::milliseconds{args.tick_period},
            [&app](std::chrono::milliseconds delta) { app.Tick(delta); }
        );
        ticker->Start();
    }
    // Создаём обработчик запросов в куче, управляемый shared_ptr
    auto handler = std::make_shared<http_handler::RequestHandler>(api_handler, args.root_path, api_strand);
    // Оборачиваем его в логирующий декоратор
    server_logging::LoggingRequestHandler logging_handler(
        [handler](auto&& req, auto&& send) {
            // Обрабатываем запрос
            (*handler)(
                std::forward<decltype(req)>(req),
                std::forward<decltype(send)>(send));
            }
    );

    // 5. Запустить обработчик HTTP-запросов, делегируя их обработчику запросов
    const auto address = net::ip::make_address("0.0.0.0");
    constexpr net::ip::port_type port = 8080;
    http_server::ServeHttp(ioc, {address, port}, logging_handler);

    // 6. Логгируем старт
    server_logging::LogStart(port, address);

    // 7. Запускаем обработку асинхронных операций
    RunWorkers(std::max(1u, conf.num_threads), [&ioc] {
        ioc.run();
    });

    // 8. Сохраняем состояние сервера при получении сигналов SIGINT, SIGTERM
    app_serializator.Serialize();
}

}  // namespace

int main(int argc, const char* argv[]) {
    server_logging::InitLogger();
    try {
        auto args = cmd_parser::ParseCommandLine(argc, argv);
        StartServer(args.value());
    } catch (const std::exception& ex) {
        server_logging::LogStop(ex);
        return EXIT_FAILURE;
    }
}
