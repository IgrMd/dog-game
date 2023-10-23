#include "../model/geom.h"

#include "request_handler.h"

#include <boost/json/parse.hpp>

#include <string>
#include <tuple>

bool IsSubPath(const fs::path& path, const fs::path& base) {
    auto canonical_path = fs::weakly_canonical(path);
    auto canonical_base = fs::weakly_canonical(base);

    for (auto b = canonical_base.begin(), p = canonical_path.begin(); b != canonical_base.end(); ++b, ++p) {
        if (p == path.end() || *p != *b) {
            return false;
        }
    }
    return true;
}

std::queue<std::string_view> SplitIntoTokens(std::string_view str, char delimeter) {
    std::queue<std::string_view> words;
    size_t pos = str.find_first_not_of(delimeter);
    const size_t pos_end = str.npos;
    while (pos != pos_end) {
        size_t space = str.find(delimeter, pos);
        words.push(space == pos_end ? str.substr(pos) : str.substr(pos, space - pos));
        pos = str.find_first_not_of(delimeter, space);
    }
    return words;
}

namespace http_handler {

const fs::path ApiTokens::api_root = fs::path{"/"} / ApiTokens::API;
const std::unordered_map<std::string_view, std::string_view> ContentType::ext_to_content =
{
    {".htm"sv,  TEXT_HTML},
    {".html"sv, TEXT_HTML},
    {".css"sv,  TEXT_CSS},
    {".txt"sv,  TEXT_PLAIN},
    {".js"sv,   TEXT_JS},
    {".json"sv, APPLICATION_JSON},
    {".xml"sv,  APPLICATION_XML},
    {".png"sv,  IMAGE_PNG},
    {".jpg"sv,  IMAGE_JPG},
    {".jpe"sv,  IMAGE_JPG},
    {".jpeg"sv, IMAGE_JPG},
    {".gif"sv,  IMAGE_GIF},
    {".bmp"sv,  IMAGE_BMP},
    {".ico"sv,  IMAGE_ICO},
    {".tiff"sv, IMAGE_TIFF},
    {".tif"sv,  IMAGE_TIFF},
    {".svg"sv,  IMAGE_SVG},
    {".svgz"sv, IMAGE_SVG},
    {".mp3"sv,  AUDIO_MP3},
};

const std::unordered_map<http::verb, std::string_view> Methods::method_to_str =
{
    {http::verb::get,  GET},
    {http::verb::head, HEAD},
    {http::verb::post, POST},
};


StringResponse MakeStringResponse(http::status status, std::string_view body, const RequestData& req_data,
                                  std::string_view content_type) {
    StringResponse response(status, req_data.http_version);
    response.set(http::field::content_type, content_type);
    response.set(http::field::cache_control, Constants::NO_CACHE);
    if (body.empty() && content_type == ContentType::APPLICATION_JSON) {
        response.body() = Constants::EMPTY_JSON;
        response.content_length(Constants::EMPTY_JSON.size());
    } else {
        response.body() = body;
        response.content_length(body.size());
    }
    response.keep_alive(req_data.keep_alive);
    return response;
}

std::optional<std::string> DecodeURI(std::string_view encoded) {
    static const std::string prefix = "0x"s;
    std::string decoded;
    decoded.reserve(encoded.size());
    int i = 0;
    while (i < encoded.size()) {
        if (encoded[i] == '%') {
            if (i + 2 >= encoded.size()) {
                return std::nullopt;
            }
            size_t count, c;
            try {
                c = std::stoul(std::string{encoded.substr(++i, 2)}, &count, 16);
            } catch (...) {
                return std::nullopt;
            }
            if (count != 2 || c > CHAR_MAX) {
                return std::nullopt;
            }
            decoded.push_back(static_cast<char>(c));
            i += 2;
            continue;
        }
        if (encoded[i] == '+') {
            decoded.push_back(' ');
        } else {
            decoded.push_back(encoded[i]);
        }
        ++i;
    }
    return decoded;
}

// ApiHandler
ApiHandler::ApiHandler(app::Application& app, const extra_data::ExtraData& extra_data)
    : app_{app}
    , extra_data_{extra_data} {}

StringResponse ApiHandler::HandleSingleMapRequest(std::string_view version) const {
    std::string_view map_id = req_tokens_.front(); req_tokens_.pop();
    if (!req_tokens_.empty()) {
        return ResponseApiError(ErrorCode::BadRequest);
    }
    model::Map::Id id{std::string{map_id}};
    const model::Map* map = app_.FindMap(id);
    if (!map) {
        return ResponseApiError(ErrorCode::MapNotFound);
    }
    if (req_data_.method == http::verb::head) {
        return MakeStringResponse(http::status::ok, {}, req_data_, ContentType::APPLICATION_JSON);
    }
    auto body = json::serialize(MapAsJsonObject(*map));
    return MakeStringResponse(http::status::ok, body, req_data_, ContentType::APPLICATION_JSON);
}

StringResponse ApiHandler::HandleAllMapsRequest(std::string_view version) const {
    if (req_data_.method == http::verb::head) {
        return MakeStringResponse(http::status::ok, {}, req_data_, ContentType::APPLICATION_JSON);
    }
    const bool short_info = true;
    json::array json_maps;
    for (const auto& map : app_.GetMaps()) {
        json_maps.push_back(std::move(MapAsJsonObject(map, short_info)));
    }
    auto body = json::serialize(json_maps);
    return MakeStringResponse(http::status::ok, body, req_data_, ContentType::APPLICATION_JSON);
}

StringResponse ApiHandler::HandleMapsRequest(std::string_view version) const {
    if (version != ApiTokens::V1){
        return ErrorBuilder::MakeErrorResponse(ErrorCode::BadRequest, req_data_);
    }
    auto action = [this, version]() {
        return req_tokens_.empty()
            ? HandleAllMapsRequest(version)
            : HandleSingleMapRequest(version);
    };
    return ExecuteAllowedMethods(std::move(action), http::verb::get, http::verb::head);
}

StringResponse ApiHandler::HandleGameRequest(std::string_view version) const {
    if (version != ApiTokens::V1){
        return ResponseApiError(ErrorCode::BadRequest);
    }
    if (req_tokens_.empty()) {
        return ResponseApiError(ErrorCode::BadRequest);
    }
    auto api_token = req_tokens_.front(); req_tokens_.pop();
    if (api_token == ApiTokens::JOIN && req_tokens_.empty()) {
        return HandlePlayerJoin(version);
    }
    if (api_token == ApiTokens::PLAYERS && req_tokens_.empty()) {
        return HandlePlayersRequest(version);
    }
    if (api_token == ApiTokens::STATE && req_tokens_.empty()) {
        return HandleGameStateRequest(version);
    }
    if (api_token == ApiTokens::PLAYER) {
        api_token = req_tokens_.front(); req_tokens_.pop();
        if (api_token == ApiTokens::ACTION && req_tokens_.empty()) {
            return HandlePlayerActionRequest(version);
        }
    }
    if (api_token == ApiTokens::TICK && req_tokens_.empty()) {
        return HandleTickRequest(version);
    }
    if (api_token.starts_with(ApiTokens::RECORDS)) {
        return HandleRecordsRequest(api_token, version);
    }
    return ResponseApiError(ErrorCode::BadRequest);
}

StringResponse ApiHandler::HandlePlayerJoin(std::string_view version) const {
    auto action = [this]() {
        if (req_data_.content_type != ContentType::APPLICATION_JSON) {
            return ResponseApiError(ErrorCode::BadRequest);
        }
        std::error_code ec;
        json::value content = json::parse(req_data_.body.value(), ec);
        if (ec) {
            return ResponseApiError(ErrorCode::JoinGameParse);
        }
        if (!content.is_object() ||
            !content.as_object().contains(Constants::USER_NAME) ||
            !content.as_object().contains(Constants::MAP_ID) ||
            !content.as_object().at(Constants::USER_NAME).is_string() ||
            !content.as_object().at(Constants::MAP_ID).is_string() ||
            content.as_object().at(Constants::USER_NAME).as_string().empty()) {
            return ResponseApiError(ErrorCode::JoinGameParse);
        }
        std::string dog_name = content.as_object().at(Constants::USER_NAME).as_string().c_str();
        std::string map_id = content.as_object().at(Constants::MAP_ID).as_string().c_str();
        auto result = app_.JoinPlayer(model::Map::Id{map_id}, dog_name);
        if (!result.has_value()) {
            return ResponseApiError(ErrorCode::MapNotFound);
        }
        json::object player;
        player.emplace(Constants::AUTH_TOKEN, *result->first);
        player.emplace(Constants::PLAYER_ID, *result->second);
        auto body = json::serialize(player);
        return MakeStringResponse(http::status::ok, body, req_data_, ContentType::APPLICATION_JSON);
    };
    return ExecuteAllowedMethods(std::move(action), http::verb::post);
}

StringResponse ApiHandler::HandlePlayersRequest(std::string_view version) const {
    auto action = [this]() {
        return ExecuteAuthorized([this](const app::Token& token) {
            auto players = app_.GetPlayers(token);
            if (!players.has_value()) {
                return ResponseApiError(ErrorCode::PlayerTokenNotFound);
            }
            if (req_data_.method == http::verb::head) {
                return MakeStringResponse(http::status::ok, {}, req_data_, ContentType::APPLICATION_JSON);
            }
            json::object json_players;
            for (const auto [id, name] : players.value()) {
                json::object player;
                player.emplace(Constants::NAME, name);
                json_players.emplace(std::to_string(*id), player);
            }
            auto body = json::serialize(json_players);
            return MakeStringResponse(http::status::ok, body, req_data_, ContentType::APPLICATION_JSON);
        });
    };
    return ExecuteAllowedMethods(std::move(action), http::verb::get, http::verb::head);
}

StringResponse ApiHandler::HandleGameStateRequest(std::string_view version) const {
    auto action = [this]() {
        return ExecuteAuthorized([this](const app::Token& token) {
            auto state = app_.GetGameState(app::Token{token});
            if (!state.has_value()) {
                return ResponseApiError(ErrorCode::PlayerTokenNotFound);
            }
            if (req_data_.method == http::verb::head) {
                return MakeStringResponse(http::status::ok, {}, req_data_, ContentType::APPLICATION_JSON);
            }
            static const std::unordered_map<model::Dog::Direction, std::string_view> direction_map{
                {model::Dog::Direction::NORTH, "U"sv},
                {model::Dog::Direction::SOUTH, "D"sv},
                {model::Dog::Direction::WEST,  "L"sv},
                {model::Dog::Direction::EAST,  "R"sv}
            };

            json::object json_players_state;
            for (const auto& player : state->players) {
                json::object json_player;
                json_player.emplace(Constants::POSITION, json::array{player.pos.x, player.pos.y});
                json_player.emplace(Constants::SPEED, json::array{player.speed.x, player.speed.y});
                json_player.emplace(Constants::DIRECTION, direction_map.at(player.dir));
                json::array json_bag;
                for (auto loot_item : player.bag) {
                    json::object json_loot_item;
                    json_loot_item.emplace(Constants::ID, loot_item.id);
                    json_loot_item.emplace(Constants::TYPE, loot_item.type);
                    json_bag.emplace_back(std::move(json_loot_item));
                }
                json_player.emplace(Constants::BAG, std::move(json_bag));
                json_player.emplace(Constants::SCORE, player.score);
                json_players_state.emplace(std::to_string(*player.id), std::move(json_player));
            }

            json::object json_loot_objects_state;
            for (const auto& loot_object : state->loot_objects) {
                json::object json_loot_object;
                json_loot_object.emplace(Constants::TYPE, loot_object.type);
                json_loot_object.emplace(Constants::POSITION, json::array{loot_object.pos.x, loot_object.pos.y});
                json_loot_objects_state.emplace(std::to_string(*loot_object.id), json_loot_object);
            }

            json::object json_game_state;
            json_game_state.emplace(Constants::PLAYERS, std::move(json_players_state));
            json_game_state.emplace(Constants::LOST_OBJECTS, std::move(json_loot_objects_state));
            return MakeStringResponse(http::status::ok, json::serialize(json_game_state), req_data_, ContentType::APPLICATION_JSON);
        });
    };
    return ExecuteAllowedMethods(std::move(action), http::verb::get, http::verb::head);
}

StringResponse ApiHandler::HandlePlayerActionRequest(std::string_view version) const {
    const auto action = [this](const app::Token& token){
        if (req_data_.content_type != ContentType::APPLICATION_JSON) {
            return ResponseApiError(ErrorCode::BadRequest);
        }
        std::error_code ec;
        json::value content = json::parse(req_data_.body.value(), ec);
        if (ec) {
            return ResponseApiError(ErrorCode::ActionParse);
        }
        if (!content.is_object() ||
            !content.as_object().contains(Constants::MOVE) ||
            !content.as_object().at(Constants::MOVE).is_string()) {
            return ResponseApiError(ErrorCode::ActionParse);
        }
        static const std::unordered_map<std::string_view, model::Dog::Direction> direction_map{
            {"U"sv, model::Dog::Direction::NORTH},
            {"D"sv, model::Dog::Direction::SOUTH},
            {"L"sv, model::Dog::Direction::WEST},
            {"R"sv, model::Dog::Direction::EAST},
        };
        std::string dir = content.as_object().at(Constants::MOVE).as_string().c_str();
        if (dir.empty()) {
            if (!app_.StopPlayer(token)) {
                return ResponseApiError(ErrorCode::PlayerTokenNotFound);
            }
        } else {
            if (!direction_map.contains(dir)) {
                return ResponseApiError(ErrorCode::JoinGameParse);
            }
            if (!app_.MovePlayer(token, direction_map.at(dir))) {
                return ResponseApiError(ErrorCode::PlayerTokenNotFound);
            }
        }
        return MakeStringResponse(http::status::ok, {}, req_data_, ContentType::APPLICATION_JSON);
    };

    return ExecuteAllowedMethods([this, &action](){
        return ExecuteAuthorized([&action](const app::Token& token) {
            return action(token);
        });
    }, http::verb::post);
}

StringResponse ApiHandler::HandleTickRequest(std::string_view version) const {
    const auto action = [this](){
        if (req_data_.content_type != ContentType::APPLICATION_JSON) {
            return ResponseApiError(ErrorCode::BadRequest);
        }
        std::error_code ec;
        json::value content = json::parse(req_data_.body.value(), ec);
        if (ec) {
            return ResponseApiError(ErrorCode::TickParse);
        }
        if (!content.is_object() ||
            !content.as_object().contains(Constants::TIME_DELTA) ||
            !content.as_object().at(Constants::TIME_DELTA).is_int64()) {
            return ResponseApiError(ErrorCode::TickParse);
        }
        size_t time_delta = content.as_object().at(Constants::TIME_DELTA).as_int64();
        if (time_delta < 0) {
            return ResponseApiError(ErrorCode::TickParse);
        }
        auto tick = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::duration<size_t, std::milli>(time_delta));
        if (app_.TimeTick(tick)) {
            return MakeStringResponse(http::status::ok, {}, req_data_, ContentType::APPLICATION_JSON);
        }
        return ResponseApiError(ErrorCode::TickFail);
    };
    return ExecuteAllowedMethods([this, &action](){
        return action();
    }, http::verb::post);
}

int ExtractParameterValue(std::string_view api_token, std::string_view parameter) {
    size_t start = api_token.find(parameter);
    if (start == std::string::npos) {
        return 0;
    }
    start += parameter.size();
    if (start == api_token.size() || api_token[start] != '=') {
        throw std::runtime_error("Value not found");
    }
    size_t end = api_token.find_first_of('&', start);
    if (end == std::string::npos) {
        end = api_token.size();
    }
    return std::stoi(std::string{api_token.substr(start + 1, end - start)});
}

static std::tuple<int, int> ParseRecordEndpoint(std::string_view api_token) {
    return {
        ExtractParameterValue(api_token, Constants::START),
        ExtractParameterValue(api_token, Constants::MAX_ITEMS)
    };
}

StringResponse ApiHandler::HandleRecordsRequest(std::string_view api_token, std::string_view version) const {
    const auto action = [this, api_token](){
        int start, max_items;
        try {
            std::tie(start, max_items) = ParseRecordEndpoint(api_token);
        } catch (...) {
            return ResponseApiError(ErrorCode::BadRequest);
        }
        if (max_items > 100) {
            return ResponseApiError(ErrorCode::BadRequest);
        }
        if (max_items == 0) {
            max_items = 100;
        }
        auto players = app_.Records(start, max_items);
        json::array json_players;
        for (const auto& player : players) {
            json::object json_player;
            json_player.emplace(Constants::NAME, player.GetName());
            json_player.emplace(Constants::SCORE, player.GetScore());
            json_player.emplace(Constants::PLAY_TIME, player.PlayTime()*1./1000);
            json_players.push_back(std::move(json_player));
        }
        return MakeStringResponse(http::status::ok, json::serialize(json_players), req_data_, ContentType::APPLICATION_JSON);
    };

    return ExecuteAllowedMethods([this, &action](){
        return action();
    }, http::verb::get, http::verb::head);
}

static void JsonifyMainMapInfo(const model::Map& map, json::object& json_map) {
    json_map.emplace(json_loader::MapFields::id, *map.GetId());
    json_map.emplace(json_loader::MapFields::name, map.GetName());
}

static void JsonifyRoads(const model::Map& map, json::object& json_map) {
    json::array json_roads;
    for (const auto& road : map.GetRoads()) {
        json::object json_road;
        geom::PointInt start = road.GetStart();
        json_road.emplace(json_loader::RoadFields::x0, start.x);
        json_road.emplace(json_loader::RoadFields::y0, start.y);
        geom::PointInt end = road.GetEnd();
        if (road.IsHorizontal()) {
            json_road.emplace(json_loader::RoadFields::x1, end.x);
        } else {
            json_road.emplace(json_loader::RoadFields::y1, end.y);
        }
        json_roads.push_back(std::move(json_road));
    }
    json_map.emplace(json_loader::MapFields::roads, json_roads);
}

static void JsonifyBuildings(const model::Map& map, json::object& json_map) {
    json::array json_offices;
    for (const auto& office : map.GetOffices()) {
        json::object json_office;
        json_office.emplace(json_loader::OfficeFields::id, *office.GetId());
        geom::PointInt position = office.GetPosition();
        json_office.emplace(json_loader::OfficeFields::x, position.x);
        json_office.emplace(json_loader::OfficeFields::y, position.y);
        model::Offset offset = office.GetOffset();
        json_office.emplace(json_loader::OfficeFields::offsetX, offset.dx);
        json_office.emplace(json_loader::OfficeFields::offsetY, offset.dy);
        json_offices.push_back(std::move(json_office));
    }
    json_map.emplace(json_loader::MapFields::offices, json_offices);
}

static void JsonifyOffices(const model::Map& map, json::object& json_map) {
    json::array json_buildings;
    for (const auto& building : map.GetBuildings()) {
        json::object json_building;
        model::Rectangle rect = building.GetBounds();
        json_building.emplace(json_loader::BuildingFields::x, rect.position.x);
        json_building.emplace(json_loader::BuildingFields::y, rect.position.y);
        json_building.emplace(json_loader::BuildingFields::w, rect.size.width);
        json_building.emplace(json_loader::BuildingFields::h, rect.size.height);
        json_buildings.push_back(std::move(json_building));
    }
    json_map.emplace(json_loader::MapFields::buildings, json_buildings);
}

json::object ApiHandler::MapAsJsonObject(const model::Map& map, bool short_info /* = false */) const {
    json::object json_map;
    JsonifyMainMapInfo(map, json_map);
    if (short_info) {
        return json_map;
    }
    json_map.emplace(json_loader::Fields::lootTypes, extra_data_.map_id_to_loot_types.at(map.GetId()));
    JsonifyRoads(map, json_map);
    JsonifyOffices(map, json_map);
    JsonifyBuildings(map, json_map);
    return json_map;
}

StringResponse ApiHandler::ResponseApiError(ErrorCode ec) const {
    return ErrorBuilder::MakeErrorResponse(ec, req_data_);
}

}  // namespace http_handler
