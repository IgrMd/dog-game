#pragma once

#include "../model/model.h"
#include "../db/postgres.h"

#include "player.h"
#include "unit_of_work.h"

#include <chrono>
#include <optional>
#include <random>
#include <unordered_map>

namespace app {

namespace detail {

}  // namespace detail

class ApplicationListener {
public:
    virtual void OnTick(std::chrono::milliseconds) = 0;
    virtual ~ApplicationListener() = default;
};

class PlayerTokens {
public:
    const Token& AddPlayer(const Player& player);

    void AddPlayer(const Player& player, Token token);

    Player* FindPlayerByToken(const Token& token) const;

    PlayersState GetPlayersState() const;

    void ErasePlayer(const Player* player);

private:

    std::unordered_map<Token, Player*, util::TaggedHasher<Token>> token_to_player_;
    std::unordered_map<const Player*, const Token*> player_to_token_;
    detail::TokenGenerator get_token_;
};

// Players
class Players {
public:

    Player& AddPlayer(model::Dog* dog, model::GameSession* session);

    Player* FindByDogIdAndMapId(model::Dog::Id dog_id, const model::Map::Id& map_id);

    void ErasePlayer(model::Dog::Id dog_id, const model::Map::Id& map_id);

private:

    struct Hasher{size_t operator()(const std::pair<model::Dog::Id, model::Map::Id>& item) const;};

    using DogIdAndMapIdToPlayer = std::unordered_map<std::pair<model::Dog::Id, model::Map::Id>, Player, Hasher>;
    DogIdAndMapIdToPlayer players_;
};

// Use Cases
class Application;
class UseCaseBase {
public:
    explicit UseCaseBase(Application* app);

protected:
    model::Game& GetGame() const noexcept;
    Players& GetPlayers() const noexcept ;
    PlayerTokens& GetPlayerTokens() const noexcept ;
    bool TimeTickerUsed();
    Application* app_;
    postgres::UnitOfWorkFactoryImpl& GetUnitOfWorkFactory();
};

class UseCaseJoinPlayer : public UseCaseBase {
public:
    using UseCaseBase::UseCaseBase;

    using Result = std::optional<std::pair<Token, model::Dog::Id>>;
    Result operator()(const model::Map::Id& map_id, std::string dog_name);
};

class UseCaseGetPlayers : public UseCaseBase {
public:
    using UseCaseBase::UseCaseBase;

    using Result = std::optional<std::vector<std::pair<model::Dog::Id, std::string_view>>>;
    Result operator()(const Token& player_token);
};

class UseCaseGetGameState : public UseCaseBase {
public:
    using UseCaseBase::UseCaseBase;
    struct PlayerState {
        struct LootObj {
            size_t id, type;
        };
        model::Dog::Id id;
        geom::PointDouble pos;
        geom::PointDouble speed;
        model::Dog::Direction dir;
        using Bag = std::vector<LootObj>;
        Bag bag;
        size_t score;
    };
    struct LootObjectState {
        model::LootObject::Id id;
        size_t type;
        geom::PointDouble pos;
    };
    struct GameState {
        std::vector<PlayerState> players;
        std::vector<LootObjectState> loot_objects;
    };
    using Result = std::optional<GameState>;
    Result operator()(const Token& player_token);
};

class UseCaseMovePlayer : public UseCaseBase {
public:
    using UseCaseBase::UseCaseBase;
    using Result = bool;
    Result operator()(const Token& player_token, model::Dog::Direction dir);
};

class UseCaseStopPlayer : public UseCaseBase {
public:
    using UseCaseBase::UseCaseBase;
    using Result = bool;
    Result operator()(const Token& player_token);
};

class UseCaseTimeTick : public UseCaseBase {
public:
    using UseCaseBase::UseCaseBase;
    bool operator()(std::chrono::milliseconds time_delta);
};

class UseCaseDogRetire : public UseCaseBase {
public:
    using UseCaseBase::UseCaseBase;
    bool operator()(model::Dog::Id dog, const model::Map::Id&);
};

class UseCaseRecords : public UseCaseBase {
public:
    using UseCaseBase::UseCaseBase;
    std::vector<RetiredPlayer> operator()(int offset, int limit);
};

struct AppConfig {
    std::string db_url;
    unsigned num_threads = 1;
};

// Application
class Application {
    friend UseCaseBase;
public:
    explicit Application(model::Game& game, const AppConfig& config);

    const model::Game::Maps& GetMaps() const noexcept;

    const model::Map* FindMap(const model::Map::Id& id) const noexcept;

    void Tick(std::chrono::milliseconds time_delta);

    void TimeTickerUsed();

    PlayersState GetPlayersState() const;

    void AddPlayer(Token token, const model::Map::Id& map_id,
        model::GameSession::Id session_id, model::Dog::Id dog_id);

    void AddListener(std::unique_ptr<ApplicationListener> listener);

    UseCaseGetGameState GetGameState;
    UseCaseGetPlayers GetPlayers;
    UseCaseJoinPlayer JoinPlayer;
    UseCaseMovePlayer MovePlayer;
    UseCaseStopPlayer StopPlayer;
    UseCaseTimeTick TimeTick;
    UseCaseDogRetire DogRetire;
    UseCaseRecords Records;

private:
    model::Game& game_;
    Players players_;
    PlayerTokens player_tokens_;
    bool time_ticker_used_ = false;
    postgres::Database db_;
    std::unique_ptr<ApplicationListener> listener_;
};

} //namespace app