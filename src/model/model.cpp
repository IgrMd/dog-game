#include "model.h"
#include <stdexcept>

namespace model {

// Map::
Map::Map(Id id, std::string name) noexcept
    : id_(std::move(id))
    , name_(std::move(name)) {
}

const Map::Id& Map::GetId() const noexcept {
    return id_;
}

const std::string& Map::GetName() const noexcept {
    return name_;
}

const Map::Buildings& Map::GetBuildings() const noexcept {
    return buildings_;
}

const Map::Roads& Map::GetRoads() const noexcept {
    return roads_;
}

const Map::Offices& Map::GetOffices() const noexcept {
    return offices_;
}

void Map::AddRoad(const Road& road) {
    roads_.emplace_back(road);
}

void Map::AddBuilding(const Building& building) {
    buildings_.emplace_back(building);
}

Map& Map::SetDogSpeed(double value) {
    dog_speed_ = value;
    return *this;
}
void Map::AddLootTypeWorth(size_t value) {
    loot_types_worth_.push_back(value);
}

Map& Map::SetDogBagCapacity(size_t value) {
    bag_capacity_ = value;
    return *this;
}

double Map::GetDogSpeed() const noexcept {
    return dog_speed_;
}

size_t Map::GetLootTypeCount() const noexcept {
    return loot_types_worth_.size();
}

size_t Map::GetLootWorth(size_t loot_type) const noexcept {
    return loot_types_worth_.at(loot_type);
}

size_t Map::GetDogBagCapacity() const noexcept {
    return bag_capacity_;
}

// LootObject::
LootObject::LootObject(Id id, size_t type, size_t worth) noexcept
    : id_{id}
    , type_{type}
    , worth_(worth) {
}

const LootObject::Id& LootObject::GetId() const noexcept {
    return id_;
}

size_t LootObject::GetType() const noexcept {
    return type_;
}

size_t LootObject::GetWorth() const noexcept {
    return worth_;
}

// Dog::
Dog::Dog(Dog::Id id, std::string name, geom::PointDouble coords,
    Direction direction /* = Direction::NORTH */, geom::PointDouble speed /* = {} */) noexcept
    : id_(id)
    , name_(std::move(name))
    , coords_{coords}
    , prev_coords_{coords}
    , direction_{direction}
    , speed_(speed) {
}

const Dog::Id& Dog::GetId() const noexcept {
    return id_;
}

const std::string& Dog::GetName() const noexcept {
    return name_;
}

Dog::Direction Dog::GetDirection() const noexcept {
    return direction_;
}

const geom::PointDouble& Dog::GetCoorginates() const noexcept {
    return coords_;
}

const geom::PointDouble& Dog::GetPrevCoorginates() const noexcept {
    return prev_coords_;
}

const geom::PointDouble& Dog::GetSpeed() const noexcept {
    return speed_;
}

size_t Dog::GetScore() const noexcept {
    return score_;
}

const Dog::Bag& Dog::GetBagpack() const {
    return bagpack_;
}

size_t Dog::LootCountInBagpack() const noexcept {
    return bagpack_.size();
}

void Dog::AddLootObjectToBagpack(LootObject obj) {
    bagpack_.emplace_back(std::move(obj));
}

void Dog::AddScore(size_t score) {
    score_ += score;
}

void Dog::DropBagpackContent() {
    score_ = std::accumulate(bagpack_.begin(), bagpack_.end(), score_,
        [this](size_t lhs, const LootObject& rhs) {
            return lhs + rhs.GetWorth();
        }
    );
    bagpack_.clear();
}

void Dog::SetCoorginates(geom::PointDouble move_to) {
    prev_coords_ = coords_;
    coords_ = move_to;
}

void Dog::SetSpeed(double speed) {
    speed_.x = (direction_ == Direction::NORTH || direction_ == Direction::SOUTH )
                ? 0.
                : (direction_ == Direction::EAST ? speed : -speed);
    speed_.y = (direction_ == Direction::WEST || direction_ == Direction::EAST )
                ? 0.
                : (direction_ == Direction::SOUTH ? speed : -speed);
}

void Dog::SetDirection(Direction dir) {
    direction_ = dir;
}


void Dog::Stop() {
    speed_.x = speed_.y = 0.;
    holding_time_ = 0;
}

bool Dog::IsStoped() const noexcept {
    return speed_.x == 0. && speed_.y == 0.;
}

void Dog::AddTick(size_t tick) {
    time_in_game_ += tick;
    if (IsStoped()) {
        holding_time_ += tick;
    }
}

size_t Dog::GetHoldingPeriod() const noexcept {
    return holding_time_;
}

size_t Dog::GetTimeInGame() const noexcept {
    return time_in_game_;
}

// GameSession::
GameSession::GameSession(const Map* map, size_t index, bool random_spawn,
    const loot_gen::LootGeneratorParams& loot_gen_params,
    size_t dog_retirement_time,
    std::function<void(Dog::Id dog, const Map::Id&)> do_on_retire,
    size_t dog_start_id /* = 0 */, size_t loot_object_start_id /* = 0 */)
    : map_{map}
    , id_{index}
    , random_spawn_{random_spawn}
    , loot_generator_{loot_gen_params.period, loot_gen_params.probability}
    , dog_retirement_time_{dog_retirement_time}
    , do_on_retire_{std::move(do_on_retire)}
    , road_count_{map->GetRoads().size()}
    , dogs_join_{dog_start_id}
    , objects_spawned_{loot_object_start_id} {
        PrepareRoads();
    }

Dog* GameSession::GetDogById(Dog::Id id) const {
    if (auto it = dog_id_to_dog_.find(id); it != dog_id_to_dog_.end()) {
        return &(*it->second);
    }
    return nullptr;
}

geom::PointDouble GameSession::GetLootCoordsById(LootObject::Id id) const {
    return loot_obj_id_to_coords_.at(id);
}

const GameSession::Id& GameSession::GetId() const noexcept {
    return id_;
}

Dog* GameSession::NewDog(std::string name) {
    size_t index = GetNewDogIndex();
    return AddDog({Dog::Id{index}, std::move(name), GetDogSpawnPoint()});
}

void GameSession::AddLootObject(LootObject obj, geom::PointDouble coords) {
    if (loot_obj_id_to_obj_.contains(obj.GetId())) {
        throw std::runtime_error("Loot object already exists");
    }
    loot_obj_id_to_coords_.emplace(obj.GetId(), coords);
    loot_obj_id_to_obj_.emplace(obj.GetId(), obj);
}

Dog* GameSession::AddDog(Dog dog) {
    auto dog_it = dogs_.emplace(dogs_.end(), std::move(dog));
    auto [it, inserted] = dog_id_to_dog_.emplace(dog_it->GetId(), dog_it);
    if (!inserted) {
        throw std::runtime_error("Dog already exists");
    }
    return &(*dog_it);
}

const Map& GameSession::GetMap() const {
    return *map_;
}

const std::list<Dog>& GameSession::GetDogs() const {
    return dogs_;
}

const GameSession::LootObjectIdToObject& GameSession::GetLootObjects() const {
    return loot_obj_id_to_obj_;
}

GameSession::StateContent GameSession::GetSessionStateContent() const {
    StateContent::LootObjects loot_objects;
    loot_objects.reserve(loot_obj_id_to_obj_.size());
    for (const auto& [_, obj] : loot_obj_id_to_obj_) {
        loot_objects.emplace_back(obj, loot_obj_id_to_coords_.at(obj.GetId()));
    }
    return StateContent{
        .map_id = map_->GetId(),
        .session_id = id_,
        .dogs = dogs_,
        .loot_objects = std::move(loot_objects),
        .dogs_join = dogs_join_,
        .objects_spawned = objects_spawned_
    };
}

size_t GameSession::GetNewDogIndex() {
    return dogs_join_++;
}

geom::PointDouble GameSession::GetRandomPointOnRandomRoad() const {
    static std::random_device rd;
    std::uniform_int_distribution<size_t> road_d(0, road_count_ - 1);
    size_t road_index = road_d(rd);
    if(!(road_index >= 0 && road_index < road_count_)) {
        road_index = 0;
    }
    const auto& road = map_->GetRoads().at(road_index);
    double x = std::uniform_real_distribution<double>{road.GetAbsDimentions().p1.x, road.GetAbsDimentions().p2.x}(rd);
    double y = std::uniform_real_distribution<double>{road.GetAbsDimentions().p1.y, road.GetAbsDimentions().p2.y}(rd);
    return geom::PointDouble{.x = x, .y = y};
}

geom::PointDouble GameSession::GetDogSpawnPoint() const {
    if (random_spawn_) {
        return GetRandomPointOnRandomRoad();
    } else {
        const auto& road = map_->GetRoads().front();
        double x = road.GetStart().x;
        double y = road.GetStart().y;
        return geom::PointDouble{.x = x, .y = y};
    }
}

void GameSession::SpawnLootObject() {
    size_t index = objects_spawned_++;
    static std::random_device rd;
    size_t type = std::uniform_int_distribution<size_t>{0, map_->GetLootTypeCount() - 1}(rd);
    auto [it, inserted] = loot_obj_id_to_obj_.emplace(
        LootObject::Id{index},
        LootObject(LootObject::Id{index}, type, map_->GetLootWorth(type))
    );
    if (!inserted) {
        throw std::runtime_error("Loot object already exists");
    }
    loot_obj_id_to_coords_[it->first] = GetRandomPointOnRandomRoad();
}

void GameSession::SpawnLoot(std::chrono::milliseconds tick) {
    int objects_count = loot_generator_.Generate(
        tick,
        loot_obj_id_to_obj_.size(),
        dogs_.size()
    );
    while (objects_count--) {
        SpawnLootObject();
    }
}

void GameSession::OnTick(std::chrono::milliseconds tick) {
    for (Dog& dog : dogs_) {
        Move(dog, tick);
        if (dog.IsStoped() && dog.GetHoldingPeriod() >= dog_retirement_time_) {
            dogs_to_retire_.push_back(dog.GetId());
        }
    }
    RetireDogs();
    HandleCollisions();
    SpawnLoot(tick);
}

void GameSession::RetireDogs() {
    std::lock_guard g(retire_mutex_);
    for (Dog::Id dog_id : dogs_to_retire_) {
        do_on_retire_(dog_id, map_->GetId());
        auto nh = dog_id_to_dog_.extract(dog_id);
        dogs_.erase(nh.mapped());
    }
    dogs_to_retire_.clear();
}

bool GameSession::IsRandomSpawn() const noexcept {
    return random_spawn_;
}

void GameSession::HandleCollisions() {
    using namespace collision_detector;

    ItemGathererProvider g_provider;

    g_provider.ReserveGatherers(dogs_.size());
    std::unordered_map<size_t, model::Dog*> gatherer_id_to_dog;
    for (auto& dog : dogs_) {
        size_t id =  g_provider.AddGatherer(dog.GetPrevCoorginates(), dog.GetCoorginates(), Dog::COLLISION_RADIUS);
        gatherer_id_to_dog[id] = &dog;
    }

    g_provider.ReserveItems(loot_obj_id_to_obj_.size() + map_->GetOffices().size());
    enum class ItemType {
        LOOT,
        OFFICE
     };
    std::unordered_map<size_t, ItemType> item_id_to_type;
    std::unordered_map<size_t, LootObject::Id> item_id_to_loot_id;
    for (auto& [_, loot_obj] : loot_obj_id_to_obj_) {
        size_t id = g_provider.AddItem(loot_obj_id_to_coords_.at(loot_obj.GetId()), LootObject::COLLISION_RADIUS);
        item_id_to_type[id] = ItemType::LOOT;
        item_id_to_loot_id.emplace(id, loot_obj.GetId());
    }
    for (const auto& office : map_->GetOffices()) {
        geom::PointDouble pos(
            static_cast<double>(office.GetPosition().x),
            static_cast<double>(office.GetPosition().y)
        );
        size_t id = g_provider.AddItem(pos, Office::COLLISION_RADIUS);
        item_id_to_type[id] = ItemType::OFFICE;
    }
    auto events = FindGatherEvents(g_provider);
    for (const GatheringEvent& event : events) {
        if (item_id_to_type.at(event.item_id) == ItemType::LOOT) {
            HandleLootColletc(gatherer_id_to_dog.at(event.gatherer_id), item_id_to_loot_id.at(event.item_id));
        } else if (item_id_to_type.at(event.item_id) == ItemType::OFFICE) {
            HandleLootDrop(gatherer_id_to_dog.at(event.gatherer_id));
        }
    }
}

void GameSession::HandleLootColletc(Dog* dog, LootObject::Id id) {
    if (dog->LootCountInBagpack() >= map_->GetDogBagCapacity()) {
        return;
    }
    if (auto loot_obj = ExtractLootObject(id)) {
        dog->AddLootObjectToBagpack(std::move(*loot_obj));
    }
}

void GameSession::HandleLootDrop(Dog* dog) {
    dog->DropBagpackContent();
}

std::optional<LootObject> GameSession::ExtractLootObject(LootObject::Id id) {
    if (!loot_obj_id_to_obj_.contains(id)) {
        return std::nullopt;
    }
    auto nh = loot_obj_id_to_obj_.extract(id);
    loot_obj_id_to_coords_.erase(id);
    return std::move(nh.mapped());
}

void GameSession::PrepareRoads() {
    for (const auto& road : map_->GetRoads()) {
        for (int x = road.GetRangeX().first; x <= road.GetRangeX().second; ++x) {
            for (int y = road.GetRangeY().first; y <= road.GetRangeY().second; ++y) {
                coords_to_roads_[x].emplace(y, &road);
            }
        }
    }
}

static double PossibleMoveDist(const geom::PointDouble& from, const Road& road, Dog::Direction dir) {
    switch (dir) {
    case Dog::Direction::NORTH:
        return road.GetAbsDimentions().p1.y - from.y;
    case Dog::Direction::SOUTH:
        return road.GetAbsDimentions().p2.y - from.y;
    case Dog::Direction::WEST:
        return road.GetAbsDimentions().p1.x - from.x;
    case Dog::Direction::EAST:
        return road.GetAbsDimentions().p2.x - from.x;
    default:
        return {};
    }
}

static geom::Dimension RoundRoadCoord(double coord) {
    static constexpr double mid = 0.5;
    return (coord - std::floor(coord) < mid) ? std::floor(coord) : std::ceil(coord);
}

static geom::PointInt RoundRoadCoords(const geom::PointDouble& coords) {
    return geom::PointInt{
        .x = RoundRoadCoord(coords.x),
        .y = RoundRoadCoord(coords.y)};
}

void GameSession::Move(Dog& dog, std::chrono::milliseconds delta_t) const {
    static constexpr geom::PointDouble zero_speed(0., 0.);
    size_t tick = delta_t.count();
    dog.AddTick(tick);
    if (dog.IsStoped()) {
        return;
    }
    geom::PointInt road_coords = RoundRoadCoords(dog.GetCoorginates());
    const auto direction = dog.GetDirection();
    auto roads = coords_to_roads_.at(road_coords.x).equal_range(road_coords.y);
    geom::PointDouble dp{
        .x = dog.GetSpeed().x * tick / TIME_FACTOR,
        .y = dog.GetSpeed().y * tick / TIME_FACTOR
    };
    auto best_road = roads.first;
    double best_dist = 0;
    for (auto it = roads.first; it != roads.second; ++it) {
        double dist = PossibleMoveDist(dog.GetCoorginates(), *it->second, direction);
        if (std::abs(dist) > std::abs(best_dist)) {
            best_dist = dist;
            best_road = it;
        }
    }
    bool border = (direction == Dog::Direction::WEST || direction == Dog::Direction::EAST)
        ? std::abs(best_dist) <= std::abs(dp.x)
        : std::abs(best_dist) <= std::abs(dp.y);
    if (border) {
        if (direction == Dog::Direction::WEST || direction == Dog::Direction::EAST) {
            dp.x = best_dist;
        }
        if (direction == Dog::Direction::NORTH || direction == Dog::Direction::SOUTH) {
            dp.y = best_dist;
        }
        dog.Stop();
    }
    dog.SetCoorginates(dog.GetCoorginates() + dp);
}

// Game::
const Game::Maps& Game::GetMaps() const noexcept {
    return maps_;
}

const Map* Game::FindMap(const Map::Id& id) const noexcept {
    if (auto it = map_id_to_index_.find(id); it != map_id_to_index_.end()) {
        return &maps_.at(it->second);
    }
    return nullptr;
}

GameSession* Game::GetGameSessionByMapId(const Map::Id& id) {
    if (auto session = map_id_to_session_.find(id); session != map_id_to_session_.end()) {
        return &session->second;
    }
    if (auto map = map_id_to_index_.find(id); map != map_id_to_index_.end()) {
        GameSession session(
            &maps_[map->second],
            last_session_index_++,
            random_spawn_,
            loot_generator_params_,
            dog_retirement_time_,
            do_on_retire_
        );
        return &(map_id_to_session_.emplace(id, std::move(session)).first->second);
    }
    return nullptr;
}

GameSession* Game::AddGameSession(const Map::Id& id, size_t index,
    size_t dog_start_id /* = 0 */, size_t loot_object_start_id /* = 0 */) {
    if (auto map = map_id_to_index_.find(id); map != map_id_to_index_.end()) {
        GameSession session(
            &maps_[map->second],
            index, random_spawn_,
            loot_generator_params_,
            dog_retirement_time_,
            do_on_retire_,
            dog_start_id,
            loot_object_start_id
        );
        return &(map_id_to_session_.emplace(id, std::move(session)).first->second);
    } else {
        throw std::runtime_error("Map not found");
    }
    return nullptr;
}

void Game::OnTick(std::chrono::milliseconds tick) {
    for (auto& [map, session] : map_id_to_session_) {
        session.OnTick(tick);
    }
}

void Game::SetRandomSpawn(bool value) {
    random_spawn_ = value;
}

void Game::SetLootGeneratorParams(double period, double probability) {
    loot_generator_params_.period = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::duration<double>(period));
    loot_generator_params_.probability = probability;
}

void Game::SetDogRetirementTime(size_t dog_retirement_time) {
    dog_retirement_time_ = dog_retirement_time;
}

Game::GameState Game::GetGameState() const {
    GameState state;
    state.reserve(map_id_to_session_.size());
    for (const auto& [id, session] : map_id_to_session_) {
        state.emplace_back(session.GetSessionStateContent());
    }
    return state;
}

void Game::SetRetireListener(std::function<void(Dog::Id dog, const Map::Id&)> do_on_retire) {
    do_on_retire_ = std::move(do_on_retire);
}

}  // namespace model
