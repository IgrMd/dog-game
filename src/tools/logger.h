#pragma once

#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/http.hpp>
#include <boost/date_time.hpp>
#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/utility/manipulators/add_value.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/json.hpp>

#include <string_view>

namespace server_logging {

namespace beast = boost::beast;
namespace http = beast::http;
namespace json = boost::json;
namespace logging = boost::log;
namespace net = boost::asio;
namespace keywords = boost::log::keywords;
namespace sys = boost::system;

using namespace std::literals;

struct LogField {
    LogField() = delete;
    static constexpr std::string_view ADDRESS   = "address"sv;
    static constexpr std::string_view CODE      = "code"sv;
    static constexpr std::string_view CONT_TYPE = "content_type"sv;
    static constexpr std::string_view DATA      = "data"sv;
    static constexpr std::string_view EXCEPTION = "exception"sv;
    static constexpr std::string_view IP        = "ip"sv;
    static constexpr std::string_view MESSAGE   = "message"sv;
    static constexpr std::string_view METHOD    = "method"sv;
    static constexpr std::string_view PORT      = "port"sv;
    static constexpr std::string_view RESP_TIME = "response_time"sv;
    static constexpr std::string_view TEXT      = "text"sv;
    static constexpr std::string_view TIMESTAMP = "timestamp"sv;
    static constexpr std::string_view URI       = "URI"sv;
    static constexpr std::string_view WHERE     = "where"sv;
};

struct LogMsg {
    LogMsg() = delete;
    static constexpr std::string_view EXITFAILURE   = "EXIT_FAILURE"sv;
    static constexpr std::string_view SERVER_START  = "server started"sv;
    static constexpr std::string_view SERVER_STOP   = "server exited"sv;
    static constexpr std::string_view REQ_RECEIVED  = "request received"sv;
    static constexpr std::string_view RESP_SENT     = "response sent"sv;
    static constexpr std::string_view READ          = "read"sv;
    static constexpr std::string_view WRITE         = "write"sv;
    static constexpr std::string_view ACCEPT        = "accept"sv;
    static constexpr std::string_view ERROR         = "error"sv;
};

BOOST_LOG_ATTRIBUTE_KEYWORD(additional_data, "AdditionalData", json::value)
BOOST_LOG_ATTRIBUTE_KEYWORD(timestamp, "TimeStamp", boost::posix_time::ptime)

void InitLogger();

void LogStart(net::ip::port_type port, const net::ip::address& address);

void LogStop(const boost::json::object& data);

void LogStop(int code);

void LogStop(const std::exception& e);

void LogError(beast::error_code ec, std::string_view where);

template<class RequestHandler>
class LoggingRequestHandler {
public:
    explicit LoggingRequestHandler(RequestHandler&& handler)
        : decorated_{handler} {}

    template <typename Body, typename Allocator, typename Send>
    void operator() (http::request<Body, http::basic_fields<Allocator>>&& req,
        const net::ip::tcp::endpoint& endpoint, Send&& send) {
        LogRequest(req, endpoint);
        auto start = std::chrono::steady_clock::now();
        decorated_(std::move(req), [send, start, &endpoint](auto&& response){
            LogResponse(response, start, endpoint);
            send(response);
        });
    }

private:
    template <typename Body, typename Allocator>
    static void LogRequest(const http::request<Body, http::basic_fields<Allocator>>& req,
        const net::ip::tcp::endpoint& endpoint) {
        boost::json::object data;
        data.emplace(LogField::IP, endpoint.address().to_string());
        data.emplace(LogField::URI, req.target());
        data.emplace(LogField::METHOD, req.method_string());
        BOOST_LOG_TRIVIAL(info) << logging::add_value(additional_data, data)
                                << LogMsg::REQ_RECEIVED;
     }

    template <typename Body>
    static void LogResponse(http::response<Body>& response,
        std::chrono::steady_clock::time_point s, const net::ip::tcp::endpoint& endpoint) {
        boost::json::object data;
        data.emplace(LogField::IP, endpoint.address().to_string());
        data.emplace(
            LogField::RESP_TIME,
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - s).count()
        );
        data.emplace(LogField::CODE, response.result_int());
        if (response.count(http::field::content_type)) {
            data.emplace(LogField::CONT_TYPE, response[http::field::content_type]);
        } else {
            data.emplace(LogField::CONT_TYPE, json::value{});
        }
        data.emplace(LogField::CODE, response.result_int());
        BOOST_LOG_TRIVIAL(info) << logging::add_value(additional_data, data)
                                << LogMsg::RESP_SENT;
    }

private:
    RequestHandler decorated_;
};


} //namespace server_logging