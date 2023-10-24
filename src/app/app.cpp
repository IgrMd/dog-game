#include "app.h"

namespace app {

model::GameSession& Player::GetGameSession() const noexcept {
    return *session_;
}

// PlayerTokens
const Token& PlayerTokens::AddPlayer(const Player& player) {
    Player* player_ptr = const_cast<Player*>(&player);
    auto [it, inserted] = token_to_player_.emplace(Token{get_token_()}, player_ptr);
    while (!inserted) {
        std:tie(it, inserted) = token_to_player_.emplace(Token{get_token_()}, player_ptr);
    }
    player_to_token_.emplace(player_ptr, &it->first);
    return it->first;
}

void PlayerTokens::AddPlayer(const Player& player, Token token) {
    Player* player_ptr = const_cast<Player*>(&player);
    auto [it, inserted] = token_to_player_.emplace(std::move(token), player_ptr);
    if (!inserted) {
        throw std::runtime_error("Player already exists");
    }
    player_to_token_.emplace(player_ptr, &it->first);
}

void PlayerTokens::ErasePlayer(const Player* player) {
    const Token* token = player_to_token_.at(player);
    token_to_player_.erase(*token);
    player_to_token_.erase(player);
}

Player* PlayerTokens::FindPlayerByToken(const Token& token) const {
    if (token_to_player_.contains(token)) {
        return token_to_player_.at(token);
    }
    return nullptr;
}

PlayersState PlayerTokens::GetPlayersState() const {
    PlayersState content;
    for (auto [token, player] : token_to_player_) {
        content.emplace_back(
            std::move(token),
            player->GetGameSession().GetMap().GetId(),
            player->GetGameSession().GetId(),
            player->GetDog().GetId()
        );
    }
    return content;
}

// Players
Player& Players::AddPlayer(model::Dog* dog, model::GameSession* session) {
    auto [it, inserted] = players_.emplace(
        std::make_pair(dog->GetId(), session->GetMap().GetId()),
        Player{dog, session}
    );
    if (!inserted) {
        throw std::runtime_error("Dog already exists");
    }
    return it->second;
}

Player* Players::FindByDogIdAndMapId(model::Dog::Id dog_id, const model::Map::Id& map_id) {
    if (auto it = players_.find({dog_id, map_id}); it != players_.end()) {
        return &it->second;
    }
    return nullptr;
}

void Players::ErasePlayer(model::Dog::Id dog_id, const model::Map::Id& map_id) {
    players_.erase({dog_id, map_id});
}

size_t Players::Hasher::operator()(const std::pair<model::Dog::Id, model::Map::Id>& item) const {
    return util::TaggedHasher<model::Dog::Id>{}(item.first) ^ util::TaggedHasher<model::Map::Id>{}(item.second);
}

// UseCaseBase
UseCaseBase::UseCaseBase(Application* app)
    : app_{app} {}

model::Game& UseCaseBase::GetGame() const noexcept{
    return app_->game_;
}

Players& UseCaseBase::GetPlayers() const noexcept{
    return app_->players_;
}

PlayerTokens& UseCaseBase::GetPlayerTokens() const noexcept{
    return app_->player_tokens_;
}

bool UseCaseBase::TimeTickerUsed() {
    return app_->time_ticker_used_;
}

postgres::UnitOfWorkFactoryImpl& UseCaseBase::GetUnitOfWorkFactory() {
    return app_->db_.GetUnitOfWorkFactory();
}

//Use Cases
UseCaseJoinPlayer::Result UseCaseJoinPlayer::operator()(const model::Map::Id& map_id, std::string dog_name) {
    model::GameSession* session = GetGame().GetGameSessionByMapId(map_id);
    if (!session) {
        return std::nullopt;
    }
    model::Dog* dog = session->NewDog(std::move(dog_name));
    Player& player = GetPlayers().AddPlayer(dog, session);
    Token token = GetPlayerTokens().AddPlayer(player);
    return std::make_pair(std::move(token), player.GetDog().GetId());
}

UseCaseGetPlayers::Result UseCaseGetPlayers::operator()(const Token& player_token) {
    Result result = std::nullopt;
    if (Player* player = GetPlayerTokens().FindPlayerByToken(player_token)) {
        const auto& dogs = player->GetGameSession().GetDogs();
        result.emplace().reserve(dogs.size());
        for (const auto& dog : dogs) {
            result->emplace_back(dog.GetId(), dog.GetName());
        }
    }
    return result;
}

UseCaseGetGameState::Result UseCaseGetGameState::operator()(const Token& player_token) {
    Result result = std::nullopt;
    if (Player* player = GetPlayerTokens().FindPlayerByToken(player_token)) {
        const auto& dogs = player->GetGameSession().GetDogs();
        result.emplace().players.reserve(dogs.size());
        for (const auto& dog : dogs) {
            UseCaseGetGameState::PlayerState::Bag player_bag;
            for (const auto& loot_item : dog.GetBagpack()) {
                player_bag.emplace_back(*loot_item.GetId(), loot_item.GetType());
            }
            result->players.emplace_back(
                dog.GetId(),
                dog.GetCoorginates(),
                dog.GetSpeed(),
                dog.GetDirection(),
                std::move(player_bag),
                dog.GetScore()
            );
        }
        const auto& session = player->GetGameSession();
        const auto& loot_oblects = session.GetLootObjects();
        result->loot_objects.reserve(loot_oblects.size());
        for (const auto& [obj_id, obj] : loot_oblects ) {
            result->loot_objects.emplace_back(obj_id, obj.GetType(), session.GetLootCoordsById(obj_id));
        }
    }
    return result;
}

UseCaseMovePlayer::Result UseCaseMovePlayer::operator()(const Token& player_token, model::Dog::Direction dir) {
    Result result = false;
    if (Player* player = GetPlayerTokens().FindPlayerByToken(player_token)) {
        const auto speed = player->GetGameSession().GetMap().GetDogSpeed();
        player->GetDog().SetDirection(dir);
        player->GetDog().SetSpeed(speed);
        result = true;
    }
    return result;
}

UseCaseStopPlayer::Result UseCaseStopPlayer::operator()(const Token& player_token) {
    Result result = false;
    if (Player* player = GetPlayerTokens().FindPlayerByToken(player_token)) {
        player->GetDog().Stop();
        result = true;
    }
    return result;
}

bool UseCaseTimeTick::operator()(std::chrono::milliseconds time_delta) {
    if (TimeTickerUsed()) {
        return false;
    }
    app_->Tick(time_delta);
    return true;
}

bool UseCaseDogRetire::operator()(model::Dog::Id dog_id, const model::Map::Id& map_id) {
    using namespace std::chrono;
    auto unit = GetUnitOfWorkFactory().CreateUnitOfWork();
    Player* player = GetPlayers().FindByDogIdAndMapId(dog_id, map_id);
    if (!player) {
        return true;
    }
    model::Dog& dog = player->GetDog();
    unit->PlayerRepository().Save({RetiredPlayerId::New(), dog.GetName(), dog.GetScore(), dog.GetTimeInGame()});
    unit->Commit();
    GetPlayers().ErasePlayer(dog_id, map_id);
    GetPlayerTokens().ErasePlayer(player);
    return true;
}

std::vector<RetiredPlayer> UseCaseRecords::operator()(int offset, int limit) {
    auto unit = GetUnitOfWorkFactory().CreateUnitOfWork();
    return unit->PlayerRepository().GetSavedRetiredPlayers(offset, limit);
}

// Application
Application::Application(model::Game& game, const AppConfig& config)
    : game_{game}
    , db_{config.num_threads, config.db_url}
    , JoinPlayer{this}
    , GetPlayers{this}
    , GetGameState{this}
    , MovePlayer{this}
    , StopPlayer{this}
    , TimeTick{this}
    , DogRetire{this}
    , Records{this} {
    game_.SetRetireListener([this](model::Dog::Id dog, const model::Map::Id& map) {this->DogRetire(dog, map);});
    }

const model::Game::Maps& Application::GetMaps() const noexcept {
    return game_.GetMaps();
}

const model::Map* Application::FindMap(const model::Map::Id& id) const noexcept {
    return game_.FindMap(id);
}

void Application::Tick(std::chrono::milliseconds time_delta) {
    game_.OnTick(time_delta);
    if (listener_) {
        listener_->OnTick(time_delta);
    }
}

void Application::TimeTickerUsed() {
    time_ticker_used_ = true;
}

PlayersState Application::GetPlayersState() const {
    return player_tokens_.GetPlayersState();
}

void Application::AddPlayer(Token token, const model::Map::Id& map_id,
    model::GameSession::Id session_id, model::Dog::Id dog_id) {
    model::GameSession* session =  game_.GetGameSessionByMapId(map_id);
    if (!session || session->GetId() != session_id) {
        throw std::runtime_error("Game session not found");
    }
    model::Dog* dog = session->GetDogById(dog_id);
    if (!dog) {
        throw std::runtime_error("Dog not found");
    }
    Player& player = players_.AddPlayer(dog, session);
    player_tokens_.AddPlayer(player, std::move(token));
}

void Application::AddListener(std::unique_ptr<ApplicationListener> listener) {
    listener_ = std::move(listener);
}

} //namespace app