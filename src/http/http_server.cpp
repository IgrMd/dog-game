#include "http_server.h"

#include <boost/asio/dispatch.hpp>
#include <iostream>

namespace http_server {

void ReportError(beast::error_code ec, std::string_view where) {
    server_logging::LogError(ec, where);

}

void SessionBase::Run() {
    net::dispatch(
        stream_.get_executor(),
        beast::bind_front_handler(&SessionBase::Read, GetSharedThis())
    );
}

SessionBase::SessionBase(tcp::socket&& socket)
    : stream_(std::move(socket)) {}

void SessionBase::Read() {
    request_ = {};
    stream_.expires_after(120s);
    http::async_read(stream_, buffer_, request_,
        beast::bind_front_handler(&SessionBase::OnRead, GetSharedThis())
    );
}

void SessionBase::OnRead(beast::error_code ec, [[maybe_unused]] std::size_t bytes_read) {
    if (ec == http::error::end_of_stream) {
        return Close();
    }
    if (ec) {
        return ReportError(ec, server_logging::LogMsg::READ);
    }
    HandleRequest(std::move(request_));
}

void SessionBase::OnWrite(bool close, beast::error_code ec, [[maybe_unused]] std::size_t bytes_written) {
    if (ec) {
        return ReportError(ec, server_logging::LogMsg::WRITE);
    }
    if (close) {
        return Close();
    }
    Read();
}

void SessionBase::Close() {
    stream_.socket().shutdown(tcp::socket::shutdown_send);
}

}  // namespace http_server
