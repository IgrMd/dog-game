#pragma once

#include "../app/app.h"
#include "model.h"

#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/serialization/list.hpp>
#include <boost/serialization/unordered_map.hpp>

#include <filesystem>
#include <fstream>

namespace {

using boost::archive::text_oarchive;
using boost::archive::text_iarchive;

}

namespace geom {

template <typename Archive>
void serialize(Archive& ar, PointDouble& point, [[maybe_unused]] const unsigned version) {
    ar & point.x;
    ar & point.y;
}


}  // namespace geom

namespace serialization {

class LootObjRepr;

}

namespace model {

void serialize(text_oarchive& ar, LootObject& obj, [[maybe_unused]] const unsigned version);

void serialize(text_iarchive& ar, LootObject& obj, [[maybe_unused]] const unsigned version);

void serialize(text_oarchive& ar, Dog& dog, [[maybe_unused]] const unsigned version);

void serialize(text_iarchive& ar, Dog& dog, [[maybe_unused]] const unsigned version);

template <typename Archive>
void serialize(Archive& ar, GameSession::StateContent& content, [[maybe_unused]] const unsigned version) {
    ar & *content.map_id;
    ar & *content.session_id;
    ar & content.dogs;
    ar & content.loot_objects;
    ar & content.dogs_join;
    ar & content.objects_spawned;
}

}  // namespace model

namespace app{

template <typename Archive>
void serialize(Archive& ar, app::PlayerStateContent& content, [[maybe_unused]] const unsigned version) {
    ar & *content.token;
    ar & *content.map_id;
    ar & *content.session_id;
    ar & *content.dog_id;
}

} //namespace app

namespace serialization {

// LootObjRepr
class LootObjRepr {
public:
    LootObjRepr() = default;
    explicit LootObjRepr(const model::LootObject& obj);

    [[nodiscard]] model::LootObject Restore() const;

    template <typename Archive>
    void serialize(Archive& ar, [[maybe_unused]] const unsigned version) {
        ar & *id_;
        ar & type_;
        ar & worth_;
    }

private:
    model::LootObject::Id id_{0u};
    size_t type_{};
    size_t worth_{};
};

// DogRepr (DogRepresentation) - сериализованное представление класса Dog
class DogRepr {
public:
    DogRepr() = default;

    explicit DogRepr(const model::Dog& dog);
    [[nodiscard]] model::Dog Restore() const;

    template <typename Archive>
    void serialize(Archive& ar, [[maybe_unused]] const unsigned version) {
        ar & *id_;
        ar & name_;
        ar & direction_;
        ar & coords_;
        ar & speed_;
        ar & prev_coords_;
        ar & bagpack_;
        ar & score_;
    }

private:
    model::Dog::Id id_ = model::Dog::Id{0u};
    std::string name_;
    model::Dog::Direction direction_;
    geom::PointDouble coords_;
    geom::PointDouble speed_;
    geom::PointDouble prev_coords_;
    model::Dog::Bag bagpack_;
    size_t score_ = 0;
};


// ApplicationSerializator
class AppSerializator {
public:
    AppSerializator(app::Application& app, model::Game& game, const std::string path, bool save_require);

    void Serialize() const;

    void Restore();

private:
    using ApplicationState = std::pair<app::PlayersState, model::Game::GameState>;
    app::Application& app_;
    model::Game& game_;
    std::filesystem::path target_file_path_;
    std::filesystem::path buf_file_path_;
    bool has_file_;
};

class SerializingListener : public app::ApplicationListener {
public:
    SerializingListener(AppSerializator& serializator, std::chrono::milliseconds save_period)
        : serializator_{serializator}
        , save_period_{save_period} {}

    void OnTick(std::chrono::milliseconds tick) override {
        counter += tick;
        if (counter >= save_period_) {
            counter = counter - save_period_;
            serializator_.Serialize();
        }
    }

private:
    AppSerializator& serializator_;
    std::chrono::milliseconds counter{0};
    std::chrono::milliseconds save_period_;
};

}  // namespace serialization
