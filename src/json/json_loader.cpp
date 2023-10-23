#include "../model/geom.h"

#include "json_loader.h"

#include <chrono>
#include <string>
#include <fstream>

namespace json_loader {
namespace fs = std::filesystem;
namespace json = boost::json;
using namespace std::literals;

static std::string ReadFile(const std::ifstream& file) {
    if (!file.is_open()) {
        return {};
    }
    std::stringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

static std::string GetObjectFieldAsString(const json::value& obj, std::string_view field) {
    return obj.at(field).as_string().c_str();
}

static geom::Dimension GetObjectFieldAsDimension(const json::value& obj, std::string_view field) {
    return static_cast<geom::Dimension>(obj.at(field).as_int64());
}

static void AddRoad(const json::value& json_road, model::Map& map) {
    bool has_x1 = json_road.as_object().contains(RoadFields::x1);
    bool has_y1 = json_road.as_object().contains(RoadFields::y1);
    if ((has_x1 && has_y1) || (!has_x1 && !has_y1)) {
        std::ostringstream os;
        os << "Map ["sv << *map.GetId() <<"] \'"sv << map.GetName() << "\'. Invalid road"sv;
        throw std::invalid_argument("");
    }
    geom::PointInt start{.x = GetObjectFieldAsDimension(json_road, RoadFields::x0),
                       .y = GetObjectFieldAsDimension(json_road, RoadFields::y0)};
    if (has_x1) {
        map.AddRoad({ model::Road::HORIZONTAL, start, GetObjectFieldAsDimension(json_road, RoadFields::x1)});
    }
    if (has_y1) {
        map.AddRoad({ model::Road::VERTICAL, start, GetObjectFieldAsDimension(json_road, RoadFields::y1)});
    }
}

static void AddBuilding(const json::value& json_building, model::Map& map) {
    geom::PointInt position{.x = GetObjectFieldAsDimension(json_building, BuildingFields::x),
                          .y = GetObjectFieldAsDimension(json_building, BuildingFields::y)};
    model::Size size{.width = GetObjectFieldAsDimension(json_building, BuildingFields::w),
                     .height = GetObjectFieldAsDimension(json_building, BuildingFields::h)};
    map.AddBuilding(model::Building{ model::Rectangle{ .position = position,
                                                       .size = size } });
}

static void AddOffice(const json::value& json_office, model::Map& map) {
    std::string id = GetObjectFieldAsString(json_office, OfficeFields::id);
    geom::PointInt position{.x = GetObjectFieldAsDimension(json_office, OfficeFields::x),
                          .y = GetObjectFieldAsDimension(json_office, OfficeFields::y)};
    model::Offset offset{.dx = GetObjectFieldAsDimension(json_office, OfficeFields::offsetX),
                         .dy = GetObjectFieldAsDimension(json_office, OfficeFields::offsetY)};
    map.AddOffice(model::Office{model::Office::Id{std::move(id)}, position, offset});
}

json::value LoadJsonData(const std::filesystem::path& json_path) {
    // Загружаем модель игры из файла
    std::error_code ec;
    if (!fs::exists(json_path, ec) || ec) {
        std::ostringstream message;
        message << "File \"" << json_path.string() << "\" not found: "sv << ec.message();
        throw std::filesystem::filesystem_error(message.str(), ec);
    }
    // Загружаем содержимое файла json_path, в виде строки
    std::ifstream config_file(json_path);
    std::string json_string = ReadFile(config_file);
    // Парсим строку как JSON
    return json::parse(json_string);
}

std::pair<model::Game, extra_data::ExtraData> LoadGame(const json::value& input_json) {
    const double default_dog_speed = input_json.as_object().contains(Fields::defaultDogSpeed)
        ? input_json.at(Fields::defaultDogSpeed).as_double()
        : 1.;
    const double dog_retirement_time = input_json.as_object().contains(Fields::dogRetirementTime)
        ? input_json.at(Fields::dogRetirementTime).as_double()
        : 60.;
    const int default_bag_capacity = input_json.as_object().contains(Fields::defaultBagCapacity)
        ? input_json.at(Fields::defaultBagCapacity).as_int64()
        : 3;
    model::Game game;
    game.SetDogRetirementTime(
        static_cast<size_t>(dog_retirement_time * 1000)
    );
    game.SetLootGeneratorParams(
        input_json.at(Fields::lootGeneratorConfig).at(LootGeneratorFields::period).as_double(),
        input_json.at(Fields::lootGeneratorConfig).at(LootGeneratorFields::probability).as_double()
    );
    extra_data::ExtraData data;
    for (const auto& json_map : input_json.at(Fields::maps).as_array()) {
        model::Map::Id id(GetObjectFieldAsString(json_map, MapFields::id));
        std::string name = GetObjectFieldAsString(json_map, MapFields::name);
        const double dog_speed = json_map.as_object().contains(MapFields::dogSpeed)
            ? json_map.at(MapFields::dogSpeed).as_double()
            : default_dog_speed;
        const int bag_capacity = json_map.as_object().contains(MapFields::bagCapacity)
            ? json_map.at(MapFields::bagCapacity).as_double()
            : default_bag_capacity;
        model::Map map(std::move(id), std::move(name));
        map.SetDogSpeed(dog_speed).SetDogBagCapacity(bag_capacity);
        auto loot_types = json_map.at(Fields::lootTypes).as_array();
        for (const auto& loot_item : loot_types) {
            map.AddLootTypeWorth(loot_item.at(LootTypesFields::value).as_int64());
        }
        data.map_id_to_loot_types[map.GetId()] = std::move(loot_types);
        for (const auto& json_road : json_map.at(MapFields::roads).as_array()) {
            AddRoad(json_road, map);
        }
        for (const auto& json_building : json_map.at(MapFields::buildings).as_array()) {
            AddBuilding(json_building, map);
        }
        for (const auto& json_office : json_map.at(MapFields::offices).as_array()) {
            AddOffice(json_office, map);
        }
        game.AddMap(std::move(map));
    }
    return {std::move(game), std::move(data)};
}

}  // namespace json_loader
