#pragma once

#include "../app/app.h"
#include "../json/extra_data.h"
#include "../json/json_loader.h"
#include "../tools/logger.h"

#include "http_server.h"

#include <boost/algorithm/string/case_conv.hpp>
#include <boost/asio/dispatch.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/json.hpp>

#include <chrono>
#include <deque>
#include <filesystem>
#include <memory>
#include <sstream>
#include <queue>
#include <ranges>
#include <regex>
#include <system_error>
#include <tuple>
#include <unordered_map>

namespace rng = std::ranges;
namespace views = rng::views;
namespace fs = std::filesystem;

bool IsSubPath(const fs::path& path, const fs::path& base);

std::queue<std::string_view> SplitIntoTokens(std::string_view str, char delimeter);

namespace http_handler {

namespace beast = boost::beast;
namespace net = boost::asio;
namespace http = beast::http;
namespace json = boost::json;
namespace sys = boost::system;

using StringRequest = http::request<http::string_body>;
using StringResponse = http::response<http::string_body>;
using FileResponse = http::response<http::file_body>;
using namespace std::literals;

struct RequestData;

StringResponse MakeStringResponse(http::status status, std::string_view body, const RequestData& req_data,
                                  std::string_view content_type);

std::optional<std::string> DecodeURI(std::string_view encoded);


enum class ErrorCode {
    Ok,
    ActionParse,
    BadRequest,
    FileNotFound,
    InvalidAuthHeader,
    InvalidMethod,
    InvalidURI,
    JoinGameParse,
    MapNotFound,
    PlayerTokenNotFound,
    ServerError,
    TickParse,
    TickFail,
};

struct ApiTokens {
    ApiTokens() = delete;
    static constexpr std::string_view API       = "api"sv;
    static constexpr std::string_view V1        = "v1"sv;
    static constexpr std::string_view MAPS      = "maps"sv;
    static constexpr std::string_view GAME      = "game"sv;
    static constexpr std::string_view JOIN      = "join"sv;
    static constexpr std::string_view PLAYER    = "player"sv;
    static constexpr std::string_view PLAYERS   = "players"sv;
    static constexpr std::string_view STATE     = "state"sv;
    static constexpr std::string_view ACTION    = "action"sv;
    static constexpr std::string_view TICK      = "tick"sv;
    static constexpr std::string_view RECORDS   = "records"sv;
    static const fs::path api_root;
};

struct Constants {
    Constants() = delete;
    static constexpr std::string_view NO_CACHE      = "no-cache"sv;
    static constexpr std::string_view INDEX_HTML    = "index.html"sv;
    static constexpr std::string_view USER_NAME     = "userName"sv;
    static constexpr std::string_view MAP_ID        = "mapId"sv;
    static constexpr std::string_view AUTH_TOKEN    = "authToken"sv;
    static constexpr std::string_view PLAYER        = "player"sv;
    static constexpr std::string_view PLAYERS       = "players"sv;
    static constexpr std::string_view PLAYER_ID     = "playerId"sv;
    static constexpr std::string_view NAME          = "name"sv;
    static constexpr std::string_view POSITION      = "pos"sv;
    static constexpr std::string_view SPEED         = "speed"sv;
    static constexpr std::string_view DIRECTION     = "dir"sv;
    static constexpr std::string_view MOVE          = "move"sv;
    static constexpr std::string_view TIME_DELTA    = "timeDelta"sv;
    static constexpr std::string_view EMPTY_JSON    = "{}"sv;
    static constexpr std::string_view LOST_OBJECTS  = "lostObjects"sv;
    static constexpr std::string_view TYPE          = "type"sv;
    static constexpr std::string_view BAG           = "bag"sv;
    static constexpr std::string_view ID            = "id"sv;
    static constexpr std::string_view SCORE         = "score"sv;
    static constexpr std::string_view MAX_ITEMS     = "maxItems"sv;
    static constexpr std::string_view START         = "start"sv;
    static constexpr std::string_view PLAY_TIME     = "playTime"sv;
};

struct Methods {
    Methods() = delete;
    static constexpr std::string_view GET = "GET"sv;
    static constexpr std::string_view HEAD = "HEAD"sv;
    static constexpr std::string_view POST = "POST"sv;
    static const std::unordered_map<http::verb, std::string_view> method_to_str;
};

struct ContentType {
    ContentType() = delete;
    static constexpr std::string_view TEXT_HTML         = "text/html"sv;
    static constexpr std::string_view TEXT_CSS          = "text/html"sv;
    static constexpr std::string_view TEXT_PLAIN        = "text/plain"sv;
    static constexpr std::string_view TEXT_JS           = "text/javascript"sv;
    static constexpr std::string_view APPLICATION_JSON  = "application/json"sv;
    static constexpr std::string_view APPLICATION_XML   = "text/xml"sv;
    static constexpr std::string_view IMAGE_PNG         = "image/png"sv;
    static constexpr std::string_view IMAGE_JPG         = "image/jpeg"sv;
    static constexpr std::string_view IMAGE_GIF         = "image/gif"sv;
    static constexpr std::string_view IMAGE_BMP         = "image/bmp"sv;
    static constexpr std::string_view IMAGE_ICO         = "image/vnd.microsoft.icon"sv;
    static constexpr std::string_view IMAGE_TIFF        = "image/tiff"sv;
    static constexpr std::string_view IMAGE_SVG         = "image/svg+xml"sv;
    static constexpr std::string_view AUDIO_MP3         = "audio/mpeg"sv;
    static constexpr std::string_view APPLICATION_OCTED = "application/octet-stream"sv;

    static std::string_view FromFileExt(std::string_view ext) {
        if (ext_to_content.contains(ext)) {
            return ext_to_content.at(ext);
        }
        return APPLICATION_OCTED;
    }
private:
    static const std::unordered_map<std::string_view, std::string_view> ext_to_content;
};

struct RequestData {
    RequestData() = default;

    template <typename Body, typename Allocator>
    RequestData(const http::request<Body, http::basic_fields<Allocator>>& req) {
        Construct(req);
    }

    template <typename Body, typename Allocator>
    void Construct(const http::request<Body, http::basic_fields<Allocator>>& req) {
        http_version = req.version();
        keep_alive = req.keep_alive();
        method = req.method();
        raw_uri = req.target();
        decoded_uri = DecodeURI(req.target());
        if (method == http::verb::post) {
            body = req.body();
            content_type = req[http::field::content_type];
        } else {
            body.reset();
            content_type.reset();
        }
        if (req.count(http::field::authorization)) {
            auth_token = ParseAuthToken(req);
        }
    }

    unsigned http_version{};
    bool keep_alive{};
    http::verb method{};
    std::string_view raw_uri;
    std::optional<std::string> decoded_uri;
    std::optional<std::string_view> body;
    std::optional<std::string_view> content_type;
    std::optional<app::Token> auth_token;

private:
    template <typename Body, typename Allocator>
    std::optional<app::Token> ParseAuthToken(const http::request<Body, http::basic_fields<Allocator>>& req) {
        const std::string req_field = req[http::field::authorization];
        static const std::string auth_token_pattern = "[0-9a-fA-F]{32}"s;
        static const auto full_r = std::regex("^Bearer "s.append(auth_token_pattern).append("$"));
        if (!std::regex_match(req_field, full_r)) {
            return std::nullopt;
        }
        static const auto token_r = std::regex(auth_token_pattern);
        std::sregex_iterator begin{ req_field.cbegin(), req_field.cend(), token_r };
        std::sregex_iterator end{};
        if (std::distance(begin, end) != 1) {
            return std::nullopt;
        }
        return app::Token{std::move(begin->str())};
    }
};

struct ErrorBuilder {
    ErrorBuilder() = delete;

    static StringResponse MakeErrorResponse(ErrorCode ec, const RequestData& req_data, std::optional<std::string_view> param = std::nullopt) {
        auto [status, body, content_type] = Error(ec, param);
        return MakeStringResponse(status, body, req_data, content_type);
    }

private:
    static std::string SerializeError(std::string_view code, std::string_view msg) {
        static constexpr std::string_view code_key = "code"sv;
        static constexpr std::string_view msg_key = "message"sv;
        json::object obj;
        obj.emplace(code_key, code);
        obj.emplace(msg_key, msg);
        return json::serialize(obj);
    }

    static std::tuple<http::status, std::string, std::string_view> Error(ErrorCode ec, std::optional<std::string_view> param) {
        switch (ec) {
        case ErrorCode::MapNotFound:
            return {http::status::not_found, SerializeError(Codes::MapNotFound, Messages::MapNotFound), ContentType::APPLICATION_JSON};
        case ErrorCode::BadRequest:
            return {http::status::bad_request, SerializeError(Codes::BadRequest, Messages::BadRequest), ContentType::APPLICATION_JSON};
        case ErrorCode::InvalidURI:
            return {http::status::bad_request, SerializeError(Codes::BadRequest, Messages::InvalidURI), ContentType::APPLICATION_JSON};
        case ErrorCode::InvalidMethod: {
            std::ostringstream message;
            message << Messages::InvalidMethod;
            if (param.has_value()) {
                message << ". Expected methods: "sv << param.value();
            }
            return {http::status::method_not_allowed, SerializeError(Codes::InvalidMethod, message.str()), ContentType::APPLICATION_JSON};
        }
        case ErrorCode::JoinGameParse:
            return {http::status::bad_request, SerializeError(Codes::InvalidArgument, Messages::JoinGameParse), ContentType::APPLICATION_JSON};
        case ErrorCode::ActionParse:
            return {http::status::bad_request, SerializeError(Codes::InvalidArgument, Messages::ActionParse), ContentType::APPLICATION_JSON};
        case ErrorCode::TickParse:
            return {http::status::bad_request, SerializeError(Codes::InvalidArgument, Messages::TickParse), ContentType::APPLICATION_JSON};
        case ErrorCode::FileNotFound:
            return {http::status::not_found, std::string{Messages::FileNotFound}, ContentType::TEXT_PLAIN};
        case ErrorCode::ServerError:
            return {http::status::internal_server_error, std::string{Messages::FileNotFound}, ContentType::TEXT_PLAIN};
        case ErrorCode::InvalidAuthHeader:
            return {http::status::unauthorized, SerializeError(Codes::InvalidToken, Messages::AuthHeader), ContentType::APPLICATION_JSON};
        case ErrorCode::PlayerTokenNotFound:
            return {http::status::unauthorized, SerializeError(Codes::UnknownToken, Messages::PlayerToken), ContentType::APPLICATION_JSON};
        case ErrorCode::TickFail:
            return {http::status::bad_request, SerializeError(Codes::BadRequest, Messages::InvalidEndpoint), ContentType::APPLICATION_JSON};
        default:
            return {};
        }
    }

    struct Codes {
        Codes() = delete;
        static constexpr std::string_view MapNotFound = "mapNotFound"sv;
        static constexpr std::string_view BadRequest = "badRequest"sv;
        static constexpr std::string_view FileNotFound = "fileNotFound"sv;
        static constexpr std::string_view InvalidMethod = "invalidMethod"sv;
        static constexpr std::string_view InvalidArgument = "invalidArgument"sv;
        static constexpr std::string_view InvalidToken = "invalidToken"sv;
        static constexpr std::string_view UnknownToken = "unknownToken"sv;
    };

    struct Messages {
        Messages() = delete;
        static constexpr std::string_view MapNotFound       = "Map not found"sv;
        static constexpr std::string_view BadRequest        = "Bad request"sv;
        static constexpr std::string_view InvalidURI        = "Invalid URI"sv;
        static constexpr std::string_view InvalidMethod     = "Invalid method"sv;
        static constexpr std::string_view JoinGameParse     = "Join game request parse error"sv;
        static constexpr std::string_view ActionParse       = "Failed to parse action"sv;
        static constexpr std::string_view TickParse         = "Failed to parse tick request JSON"sv;
        static constexpr std::string_view FileNotFound      = "File Not Found: "sv;
        static constexpr std::string_view ServerError       = "Iternal server error"sv;
        static constexpr std::string_view AuthHeader        = "Authorization header is missing"sv;
        static constexpr std::string_view PlayerToken       = "Player token has not been found"sv;
        static constexpr std::string_view InvalidEndpoint   = "Invalid endpoint"sv;
    };

};

class ApiHandler {

public:
    explicit ApiHandler(app::Application& app, const extra_data::ExtraData& extra_data);

    template <typename Body, typename Allocator>
    StringResponse HandleRequest(const http::request<Body, http::basic_fields<Allocator>>& req);

private:
    StringResponse HandleMapsRequest(std::string_view version) const;

    StringResponse HandleAllMapsRequest(std::string_view version) const;

    StringResponse HandleSingleMapRequest(std::string_view version) const;

    StringResponse HandleGameRequest(std::string_view version) const;

    StringResponse HandlePlayerJoin(std::string_view version) const;

    StringResponse HandlePlayersRequest(std::string_view version) const;

    StringResponse HandleGameStateRequest(std::string_view version) const;

    StringResponse HandlePlayerActionRequest(std::string_view version) const;

    StringResponse HandleTickRequest(std::string_view version) const;

    StringResponse HandleRecordsRequest(std::string_view api_token, std::string_view version) const;

    json::object MapAsJsonObject(const model::Map& map, bool short_info = false) const;

    StringResponse ResponseApiError(ErrorCode ec) const;

    template <typename Arg, typename... Args>
    bool IsMethodOneOfAllowed(const Arg& arg, const Args&... args) const {
        if (req_data_.method == arg)
            return true;
        if constexpr (sizeof...(args) != 0) {
            return IsMethodOneOfAllowed(args...);
        }
        return false;
    }

    template <typename Fn, typename... Args>
    StringResponse ExecuteAllowedMethods(Fn&& action, const Args&... allowed_methods) const {
        if (!IsMethodOneOfAllowed(allowed_methods...)) {
            return MakeInvalidMethodResponse(allowed_methods...);
        }
        return action();
    }

    template <typename Fn>
    StringResponse ExecuteAuthorized(Fn&& action) const {
        if (!req_data_.auth_token.has_value()) {
            return ResponseApiError(ErrorCode::InvalidAuthHeader);
        }
        return action(req_data_.auth_token.value());
    }

    template <typename Arg, typename... Args>
    void PrintMethods(std::ostream& out, const Arg& arg, const Args&... args) const {
        out << Methods::method_to_str.at(arg);
        if constexpr (sizeof...(args) != 0) {
            out << ", "sv;
            PrintMethods(out, args...);
        }
    }

    template <typename... Args>
    StringResponse MakeInvalidMethodResponse(const Args&... args) const {
        std::ostringstream methods;
        PrintMethods(methods, args...);
        auto response = ErrorBuilder::MakeErrorResponse(ErrorCode::InvalidMethod, req_data_, methods.str());
        response.set(http::field::allow, methods.str());
        return response;
    }

private:
    RequestData req_data_;
    app::Application& app_;
    const extra_data::ExtraData& extra_data_;
    mutable std::queue<std::string_view> req_tokens_;
};

template <typename Body, typename Allocator>
StringResponse ApiHandler::HandleRequest(const http::request<Body, http::basic_fields<Allocator>>& req) {
    req_data_.Construct(req);
    if (!req_data_.decoded_uri.has_value()) {
        return ResponseApiError(ErrorCode::InvalidURI);
    }
    req_tokens_ = SplitIntoTokens(*req_data_.decoded_uri, '/');
    if (req_tokens_.empty() || req_tokens_.front() != ApiTokens::API) {
        throw std::runtime_error("Not API request");
    }
    req_tokens_.pop();
    if (req_tokens_.empty()) {
        return ResponseApiError(ErrorCode::BadRequest);
    }
    auto version = req_tokens_.front(); req_tokens_.pop();
    if (version != ApiTokens::V1) {
        return ResponseApiError(ErrorCode::BadRequest);
    }
    if (req_tokens_.empty()) {
        return ResponseApiError(ErrorCode::BadRequest);
    }
    auto token = req_tokens_.front(); req_tokens_.pop();
    if (token == ApiTokens::MAPS) {
        return HandleMapsRequest(version);
    }
    if (token == ApiTokens::GAME) {
        return HandleGameRequest(version);
    }
    return ResponseApiError(ErrorCode::BadRequest);
}

class RequestHandler : public std::enable_shared_from_this<RequestHandler>{
    using Strand = net::strand<net::io_context::executor_type>;

public:
    explicit RequestHandler(ApiHandler& api_handler, fs::path&& root, Strand api_strand)
        : api_handler_{api_handler}
        , root_(std::move(root))
        , api_strand_{api_strand} {
        std::error_code ec;
        if (!fs::exists(root_, ec) || ec) {
            std::ostringstream message;
            message << "File \"" << root_.string() << "\" not found: "sv << ec.message();
            throw std::filesystem::filesystem_error(message.str(), ec);
        }
        root_ = fs::canonical(root_);
    }

    RequestHandler(const RequestHandler&) = delete;
    RequestHandler& operator=(const RequestHandler&) = delete;

    template <typename Body, typename Allocator, typename Send>
    void operator()(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
        try {
            HandleRequest(std::forward<decltype(req)>(req), std::forward<decltype(send)>(send));
        } catch (...) {
            RequestData data(req);
            send(ErrorBuilder::MakeErrorResponse(ErrorCode::ServerError, data));
        }
    }

private:
    template <typename Body, typename Allocator, typename Send>
    void HandleRequest(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
        if (IsApiRequest(req)) {
            auto handle = [self = shared_from_this(), send, req = std::forward<decltype(req)>(req)]() {
                try {
                    send(self->api_handler_.HandleRequest(std::forward<decltype(req)>(req)));
                } catch (...) {
                    RequestData data(req);
                    send(ErrorBuilder::MakeErrorResponse(ErrorCode::ServerError, data));
                }
            };
            return net::dispatch(api_strand_, handle);
        }
        return std::visit(
            [&send](auto&& result) {
                send(std::forward<decltype(result)>(result));
            },
            HandleFileRequest(std::forward<decltype(req)>(req))
        );
    }

    template <typename Body, typename Allocator>
    std::variant<StringResponse, FileResponse> HandleFileRequest(http::request<Body, http::basic_fields<Allocator>>&& req) const {
        RequestData data(req);
        if (!data.decoded_uri.has_value()) {
            return ErrorBuilder::MakeErrorResponse(ErrorCode::InvalidURI, data);
        }
        fs::path uri(data.decoded_uri.value());
        uri = fs::weakly_canonical(root_ / uri.relative_path());
        if (!IsSubPath(uri, root_)) {
            return ErrorBuilder::MakeErrorResponse(ErrorCode::BadRequest, data);
        }
        if (data.method != http::verb::get) {
            auto response = ErrorBuilder::MakeErrorResponse(ErrorCode::InvalidMethod, data);
            response.set(http::field::allow, Methods::GET);
            return response;
        }
        if (uri == root_) {
            uri /= Constants::INDEX_HTML;
        }
        std::string_view content = ContentType::FromFileExt(
            boost::algorithm::to_lower_copy(uri.extension().string())
        );
        FileResponse response;
        response.version(data.http_version);
        response.result(http::status::ok);
        response.insert(http::field::content_type, content);
        http::file_body::value_type file;
        if (sys::error_code ec; file.open(uri.c_str(), beast::file_mode::read, ec), ec) {
            StringResponse error = ErrorBuilder::MakeErrorResponse(ErrorCode::FileNotFound, data);
            error.body().append(data.decoded_uri.value());
            error.content_length(error.body().size());
            return error;
        }
        response.body() = std::move(file);
        response.prepare_payload();
        return response;
    }

private:
    template <typename Body, typename Allocator>
    bool IsApiRequest(const http::request<Body, http::basic_fields<Allocator>>& req) const {
        auto decoded_uri = DecodeURI(req.target());
        if (!decoded_uri.has_value()) {
            return false;
        }
        fs::path uri(decoded_uri.value());
        return IsSubPath(uri, ApiTokens::api_root);
    }

    fs::path root_;
    Strand api_strand_;
    ApiHandler& api_handler_;
};

}  // namespace http_handler
