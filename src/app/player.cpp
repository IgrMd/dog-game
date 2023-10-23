#include "player.h"

#include <sstream>
#include <iomanip>

namespace app {

namespace detail {

// TokenGenerator
std::string TokenGenerator::operator()() {
    std::ostringstream ss;
    ss << std::hex << std::setw(16) << std::setfill('f') << generator1_()
        << std::setw(16) << std::setfill('a') << generator2_();
    return ss.str();
}

} //namespace detail

// Player
Player::Player(model::Dog* dog, model::GameSession* session)
    : dog_{dog}
    , session_{session} {}

model::Dog& Player::GetDog() const noexcept {
    return *dog_;
}

RetiredPlayer::RetiredPlayer(RetiredPlayerId id, std::string name, size_t score, size_t play_time)
    : id_(std::move(id))
    , name_(std::move(name))
    , score_(score)
    , play_time_(play_time) {
}

const RetiredPlayerId& RetiredPlayer::GetId() const noexcept {
    return id_;
}

const std::string& RetiredPlayer::GetName() const noexcept {
    return name_;
}

size_t RetiredPlayer::GetScore() const noexcept {
    return score_;
}

size_t RetiredPlayer::PlayTime() const noexcept {
    return play_time_;
}

} //namespace app