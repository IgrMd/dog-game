#include "logger.h"

namespace server_logging {

namespace json = boost::json;
namespace net = boost::asio;

static void JsonLogFormatter(logging::record_view const& rec, logging::formatting_ostream& strm) {
    json::object log;
    auto ts = rec[timestamp];
    log.emplace(LogField::TIMESTAMP, to_iso_extended_string(*ts));
    auto data = rec[additional_data];
    if (data) {
        log.emplace(LogField::DATA, *data);
    }
    log.emplace(LogField::MESSAGE, *rec[logging::expressions::smessage]);
    strm << json::serialize(log);
}

void InitLogger() {
    logging::add_common_attributes();
    logging::add_console_log(
        std::cout,
        keywords::format = &JsonLogFormatter,
        keywords::auto_flush = true
    );
}

void LogStart(net::ip::port_type port, const net::ip::address& address) {
    boost::json::object data;
    data.emplace(LogField::PORT, port);
    data.emplace(LogField::ADDRESS, address.to_string());
    BOOST_LOG_TRIVIAL(info) << logging::add_value(additional_data, data)
                            << LogMsg::SERVER_START;
}

void LogStop(const boost::json::object& data) {
    BOOST_LOG_TRIVIAL(info) << logging::add_value(additional_data, data)
                            << LogMsg::SERVER_STOP;
}

void LogStop(int code) {
    boost::json::object data;
    if (code == 0) {
        data.emplace(LogField::CODE, code);
    } else {
        data.emplace(LogField::CODE, LogMsg::EXITFAILURE);
    }
    LogStop(data);
}

void LogStop(const std::exception& e) {
    boost::json::object data;
    data.emplace(LogField::EXCEPTION, e.what());
    LogStop(data);
}

void LogError(beast::error_code ec, std::string_view where) {
    boost::json::object data;
    data.emplace(LogField::CODE, ec.value());
    data.emplace(LogField::TEXT, ec.message());
    data.emplace(LogField::WHERE, where);
    BOOST_LOG_TRIVIAL(info) << logging::add_value(additional_data, data)
                            << LogMsg::ERROR;
}

} //namespace server_logging