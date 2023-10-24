#pragma once

#include "../collision/collision_detector.h"
#include "../util/tagged.h"

#include "geom.h"
#include "loot_generator.h"

#include <cassert>
#include <chrono>
#include <cmath>
#include <functional>
#include <list>
#include <memory>
#include <mutex>
#include <random>
#include <ranges>
#include <string>
#include <unordered_map>
#include <vector>
#include <utility>

namespace model {

using namespace std::literals;

namespace rng = std::ranges;
namespace views = rng::views;

static constexpr size_t TIME_FACTOR = 1'000;
static constexpr double ROAD_SIDE = 0.4;
static constexpr double DOG_WIDTH = 0.6;
static constexpr double OFFICE_WIDTH = 0.5;
static constexpr double LOOT_WIDTH = 0.;

struct Size {
    geom::Dimension width, height;
};

struct Rectangle {
    geom::PointInt position;
    Size size;
};

struct Offset {
    geom::Dimension dx, dy;
};

class Road {
    struct HorizontalTag {
        HorizontalTag() = default;
    };

    struct VerticalTag {
        VerticalTag() = default;
    };

public:
    constexpr static HorizontalTag HORIZONTAL{};
    constexpr static VerticalTag VERTICAL{};

    Road(HorizontalTag, geom::PointInt start, geom::Coord end_x) noexcept
        : start_{start}
        , end_{end_x, start.y}
        , x_range_{std::min(start.x, end_x), std::max(start.x, end_x)}
        , y_range_{start.y, start.y} {
        CalculateDimentions();
    }

    Road(VerticalTag, geom::PointInt start, geom::Coord end_y) noexcept
        : start_{start}
        , end_{start.x, end_y}
        , x_range_{start.x, start.x}
        , y_range_{std::min(start.y, end_y), std::max(start.y, end_y)} {
        CalculateDimentions();
    }

    bool IsHorizontal() const noexcept {
        return start_.y == end_.y;
    }

    bool IsVertical() const noexcept {
        return start_.x == end_.x;
    }

    geom::PointInt GetStart() const noexcept {
        return start_;
    }

    geom::PointInt GetEnd() const noexcept {
        return end_;
    }

    using Range = std::pair<geom::Dimension, geom::Dimension>;
    const Range& GetRangeX() const noexcept {
        return x_range_;
    }

    const Range& GetRangeY() const noexcept {
        return y_range_;
    }

    struct Rect{geom::PointDouble p1, p2;};
    const Rect& GetAbsDimentions() const noexcept {
        return abs_dimentions_;
    }

private:
    void CalculateDimentions() {
        abs_dimentions_.p1.x = x_range_.first - ROAD_SIDE;
        abs_dimentions_.p1.y = y_range_.first - ROAD_SIDE;
        abs_dimentions_.p2.x = x_range_.second + ROAD_SIDE;
        abs_dimentions_.p2.y = y_range_.second + ROAD_SIDE;
    }
    geom::PointInt start_;
    geom::PointInt end_;
    Range x_range_;
    Range y_range_;
    Rect abs_dimentions_;
};

class Building {
public:
    explicit Building(Rectangle bounds) noexcept
        : bounds_{bounds} {
    }

    const Rectangle& GetBounds() const noexcept {
        return bounds_;
    }

private:
    Rectangle bounds_;
};

class Office {
public:
    static constexpr double COLLISION_RADIUS = OFFICE_WIDTH / 2;
    using Id = util::Tagged<std::string, Office>;

    Office(Id id, geom::PointInt position, Offset offset) noexcept
        : id_{std::move(id)}
        , position_{position}
        , offset_{offset} {
    }

    const Id& GetId() const noexcept {
        return id_;
    }

    geom::PointInt GetPosition() const noexcept {
        return position_;
    }

    Offset GetOffset() const noexcept {
        return offset_;
    }

private:
    Id id_;
    geom::PointInt position_;
    Offset offset_;
};

class Map {
public:
    using Id = util::Tagged<std::string, Map>;
    using Roads = std::vector<Road>;
    using Buildings = std::vector<Building>;
    using Offices = std::vector<Office>;

    Map(Id id, std::string name) noexcept;

    const Id& GetId() const noexcept;

    const std::string& GetName() const noexcept;

    const Buildings& GetBuildings() const noexcept;

    const Roads& GetRoads() const noexcept;

    const Offices& GetOffices() const noexcept;

    void AddRoad(const Road& road);

    void AddBuilding(const Building& building);

    template<typename Office_t>
    void AddOffice(Office_t&& office);

    Map& SetDogSpeed(double value);

    void AddLootTypeWorth(size_t value);

    Map& SetDogBagCapacity(size_t value);

    double GetDogSpeed() const noexcept;

    size_t GetDogBagCapacity() const noexcept;

    size_t GetLootTypeCount() const noexcept;

    size_t GetLootWorth(size_t loot_type) const noexcept;


private:
    using OfficeIdToIndex = std::unordered_map<Office::Id, size_t, util::TaggedHasher<Office::Id>>;

    Id id_;
    std::string name_;
    Roads roads_;
    Buildings buildings_;
    OfficeIdToIndex warehouse_id_to_index_;
    Offices offices_;
    double dog_speed_;
    size_t bag_capacity_;
    std::vector<size_t> loot_types_worth_;
};

// LootObject
class LootObject {
public:
    static constexpr double COLLISION_RADIUS = LOOT_WIDTH / 2;

    using Id = util::Tagged<size_t, LootObject>;

    LootObject() = default;
    LootObject(Id id, size_t type, size_t worth_) noexcept;

    const Id& GetId() const noexcept;

    size_t GetType() const noexcept;

    size_t GetWorth() const noexcept;

private:
    Id id_{0};
    size_t type_;
    size_t worth_;
};

// Dog
class Dog {
public:
    enum class Direction {
        NORTH,
        SOUTH,
        WEST,
        EAST,
    };
    static constexpr double COLLISION_RADIUS = DOG_WIDTH / 2;

public:
    using Id = util::Tagged<size_t, Dog>;
    Dog() = default;
    Dog(const Dog&) = default;
    Dog(Dog&&) = default;

    Dog(Dog::Id id, std::string name, geom::PointDouble coords,
        Direction direction = Direction::NORTH, geom::PointDouble speed = {}) noexcept;

    Dog& operator=(const Dog&) = delete;
    Dog& operator=(Dog&&) = default;

    const Id& GetId() const noexcept;

    const std::string& GetName() const noexcept;

    Direction GetDirection() const noexcept;

    const geom::PointDouble& GetCoorginates() const noexcept;

    const geom::PointDouble& GetPrevCoorginates() const noexcept;

    const geom::PointDouble& GetSpeed() const noexcept;

    size_t GetScore() const noexcept;

    using Bag = std::vector<LootObject>;
    const Bag& GetBagpack() const;

    size_t LootCountInBagpack() const noexcept;

    void AddLootObjectToBagpack(LootObject obj);

    void AddScore(size_t score);

    void DropBagpackContent();

    void SetCoorginates(geom::PointDouble move_to);

    void SetSpeed(double speed);

    void SetDirection(Direction dir);

    void Stop();

    bool IsStoped() const noexcept;

    void AddTick(size_t tick);

    size_t GetHoldingPeriod() const noexcept;

    size_t GetTimeInGame() const noexcept;

private:
    Id id_{0u};
    std::string name_;
    Direction direction_;
    geom::PointDouble coords_;
    geom::PointDouble speed_;
    geom::PointDouble prev_coords_;
    Bag bagpack_;
    size_t score_ = 0;
    size_t holding_time_ = 0;
    size_t time_in_game_ = 0;
};


class GameSession {
public:
    using Id = util::Tagged<size_t, GameSession>;

    struct StateContent {
        Map::Id map_id{""};
        GameSession::Id session_id{0u};
        std::list<Dog> dogs;
        using LootObjects = std::vector<std::pair<LootObject, geom::PointDouble>>;
        LootObjects loot_objects;
        size_t dogs_join;
        size_t objects_spawned;
    };

    GameSession(const Map* map, size_t index, bool random_spawn,
        const loot_gen::LootGeneratorParams& loot_gen_params,
        size_t dog_retirement_time,
        std::function<void(Dog::Id dog, const Map::Id&)> do_on_retire,
        size_t dog_start_id = 0, size_t loot_object_start_id = 0);

    Dog* GetDogById(Dog::Id id) const;

    geom::PointDouble GetLootCoordsById(LootObject::Id id) const;

    const Id& GetId() const noexcept;

    Dog* NewDog(std::string name);

    void AddLootObject(LootObject obj, geom::PointDouble coords);

    Dog* AddDog(Dog dog);

    const Map& GetMap() const;

    const std::list<Dog>& GetDogs() const;

    using LootObjectIdToObject = std::unordered_map<LootObject::Id, LootObject, util::TaggedHasher<LootObject::Id>>;
    const LootObjectIdToObject& GetLootObjects() const;

    StateContent GetSessionStateContent() const;

    void OnTick(std::chrono::milliseconds tick);

    bool IsRandomSpawn() const noexcept;

    void DoOnRetire(std::function<void(Dog::Id dog, const Map::Id&)> do_on_retire) {
        do_on_retire_ = std::move(do_on_retire);
    }

private:
    size_t GetNewDogIndex();

    geom::PointDouble GetDogSpawnPoint() const;

    geom::PointDouble GetRandomPointOnRandomRoad() const;

    void PrepareRoads();

    void Move(Dog& dog, std::chrono::milliseconds delta_t) const;

    void SpawnLoot(std::chrono::milliseconds tick);

    void SpawnLootObject();

    void HandleCollisions();

    void HandleLootColletc(Dog* dog, LootObject::Id);

    void HandleLootDrop(Dog* dog);

    std::optional<LootObject> ExtractLootObject(LootObject::Id id);

    void RetireDogs();

    const Map* map_;
    Id id_;
    bool random_spawn_;
    loot_gen::LootGenerator loot_generator_;
    size_t road_count_;
    size_t dog_retirement_time_;
    std::function<void(Dog::Id dog, const Map::Id&)> do_on_retire_;
    size_t dogs_join_;
    size_t objects_spawned_;

    std::vector<Dog::Id> dogs_to_retire_;

    using DogIdToDog = std::unordered_map<Dog::Id, std::list<Dog>::iterator, util::TaggedHasher<Dog::Id>>;
    DogIdToDog dog_id_to_dog_;
    std::list<Dog> dogs_;

    using CoordsToRoad = std::unordered_map<geom::Dimension, std::unordered_multimap<geom::Dimension, const model::Road*>>;
    CoordsToRoad coords_to_roads_;

    LootObjectIdToObject loot_obj_id_to_obj_;

    using LootObjectIdToCoords = std::unordered_map<LootObject::Id, geom::PointDouble, util::TaggedHasher<LootObject::Id>>;
    LootObjectIdToCoords loot_obj_id_to_coords_;

};

class Game {
public:
    using Maps = std::vector<Map>;

    template<typename Map_t>
    void AddMap(Map_t&& map);

    const Maps& GetMaps() const noexcept;

    const Map* FindMap(const Map::Id& id) const noexcept;

    GameSession* GetGameSessionByMapId(const Map::Id& id);

    GameSession* AddGameSession(const Map::Id& id, size_t index,
        size_t dog_start_id = 0, size_t loot_object_start_id = 0);

    void OnTick(std::chrono::milliseconds tick);

    void SetRandomSpawn(bool value);

    void SetLootGeneratorParams(double period, double probability);

    void SetDogRetirementTime(size_t dog_retirement_time);

    using GameState = std::vector<GameSession::StateContent>;
    GameState GetGameState() const;

    void SetRetireListener(std::function<void(Dog::Id dog, const Map::Id&)> do_on_retire);

private:
    using MapIdHasher = util::TaggedHasher<Map::Id>;
    using MapIdToIndex = std::unordered_map<Map::Id, size_t, MapIdHasher>;
    using MapIndexToSession = std::unordered_map<Map::Id, GameSession, MapIdHasher>;
    std::vector<Map> maps_;
    MapIdToIndex map_id_to_index_;
    MapIndexToSession map_id_to_session_;
    size_t last_session_index_ = 0;
    bool random_spawn_ = false;
    loot_gen::LootGeneratorParams loot_generator_params_;
    size_t dog_retirement_time_;
    std::function<void(Dog::Id dog, const Map::Id&)> do_on_retire_;

};

template<typename Map_t>
void Game::AddMap(Map_t&& map) {
    const size_t index = maps_.size();
    if (auto [it, inserted] = map_id_to_index_.emplace(map.GetId(), index); !inserted) {
        throw std::invalid_argument("Map with id "s + *map.GetId() + " already exists"s);
    } else {
        try {
            maps_.emplace_back(std::forward<Map_t>(map));
        } catch (...) {
            map_id_to_index_.erase(it);
            throw;
        }
    }
}

template<typename Office_t>
void Map::AddOffice(Office_t&& office) {
    if (warehouse_id_to_index_.contains(office.GetId())) {
        throw std::invalid_argument("Duplicate warehouse");
    }

    const size_t index = offices_.size();
    Office& o = offices_.emplace_back(std::forward<Office_t>(office));
    try {
        warehouse_id_to_index_.emplace(o.GetId(), index);
    } catch (...) {
        offices_.pop_back();
        throw;
    }
}

}  // namespace model
