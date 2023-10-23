#pragma once

#include "../model/model.h"
#include "../util/tagged_uuid.h"


namespace app {

namespace detail {

struct TokenTag {};

class TokenGenerator {
public:
    std::string operator()();
private:
    std::random_device random_device_;
    std::mt19937_64 generator1_{[this] {
        std::uniform_int_distribution<std::mt19937_64::result_type> dist;
        return dist(random_device_);
    }()};
    std::mt19937_64 generator2_{[this] {
        std::uniform_int_distribution<std::mt19937_64::result_type> dist;
        return dist(random_device_);
    }()};
};

struct RetiredPlayerTag {};

}  // namespace detail


using Token = util::Tagged<std::string, detail::TokenTag>;

class Player {
public:
    explicit Player(model::Dog* dog, model::GameSession* session);

    model::Dog& GetDog() const noexcept;

    model::GameSession& GetGameSession() const noexcept;

private:
    model::Dog* dog_ = nullptr;
    model::GameSession* session_ = nullptr;
};

struct PlayerStateContent {
    Token token{""};
    model::Map::Id map_id{""};
    model::GameSession::Id session_id{0u};
    model::Dog::Id dog_id{0u};
};

using PlayersState = std::vector<PlayerStateContent>;

using RetiredPlayerId = util::TaggedUUID<detail::RetiredPlayerTag>;

class RetiredPlayer {
public:
    RetiredPlayer(RetiredPlayerId id, std::string name, size_t score, size_t play_time);

    const RetiredPlayerId& GetId() const noexcept;

    const std::string& GetName() const noexcept;

    size_t GetScore() const noexcept;

    size_t PlayTime() const noexcept;

private:
    RetiredPlayerId id_;
    std::string name_;
    size_t score_;
    size_t play_time_;
};

class RetiredPlayerRepository {
public:
    virtual void Save(const RetiredPlayer& player) = 0;

    virtual std::vector<RetiredPlayer> GetSavedRetiredPlayers(int offset, int limit) = 0;

protected:
    ~RetiredPlayerRepository() = default;
};


} // namespace app