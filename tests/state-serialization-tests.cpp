#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_container_properties.hpp>
#include <sstream>
#include <tuple>

#include "../src/model/model.h"
#include "../src/model/model_serialization.h"

using namespace model;
using namespace app;
using namespace std::literals;
using namespace geom;
using namespace Catch::Matchers;

namespace {

using InputArchive = boost::archive::text_iarchive;
using OutputArchive = boost::archive::text_oarchive;

struct Fixture {
    std::stringstream strm;
    OutputArchive output_archive{strm};
};

}  // namespace
namespace model{
    bool operator==(const model::LootObject& lhs, const model::LootObject& rhs) {
        return *lhs.GetId() == *rhs.GetId() && lhs.GetType() == rhs.GetType() &&  lhs.GetWorth() == rhs.GetWorth();
    }
}

namespace Catch {
template<>
struct StringMaker<model::LootObject> {
  static std::string convert(model::LootObject const& value) {
      std::ostringstream tmp;
      tmp << "{Id: " << *value.GetId() << ", type: " << value.GetType() << ", worth: " << value.GetWorth() << "}";

      return tmp.str();
  }
};


}  // namespace Catch

bool CheckDogs(const model::Dog& lhs, const model::Dog& rhs) {
    CHECK(lhs.GetId()                 == rhs.GetId());
    CHECK(lhs.GetName()               == rhs.GetName());
    CHECK(lhs.GetDirection()          == rhs.GetDirection());
    CHECK(lhs.GetCoorginates()        == rhs.GetCoorginates());
    CHECK(lhs.GetSpeed()              == rhs.GetSpeed());
    CHECK(lhs.GetPrevCoorginates()    == rhs.GetPrevCoorginates());
    CHECK(lhs.GetBagpack()            == rhs.GetBagpack());
    CHECK(lhs.GetScore()              == rhs.GetScore());
    return true;
}

SCENARIO_METHOD(Fixture, "Serialization") {
    SECTION("Point serialization") {
        GIVEN("A point") {
            const geom::PointDouble p{10, 20};
            WHEN("point is serialized") {
                output_archive << p;
                THEN("it is equal to point after serialization") {
                    InputArchive input_archive{strm};
                    geom::PointDouble restored_point;
                    input_archive >> restored_point;
                    CHECK(p == restored_point);
                }
            }
        }
    }
    SECTION ("Loot obj serialization") {
        GIVEN("A loot object") {
            const model::LootObject obj{LootObject::Id{666u}, 13u, 42u};
            WHEN("Loot obj serialized") {
                {
                    serialization::LootObjRepr repr{obj};
                    output_archive << repr;
                }
                THEN("it is equal to loot obj after serialization") {
                    InputArchive input_archive{strm};
                    serialization::LootObjRepr repr;
                    input_archive >> repr;
                    auto restored_obj = repr.Restore();
                    CHECK(restored_obj == obj);
                }
            }
        }
    }

    SECTION("Dog Serialization") {
        GIVEN("a dog") {
            const auto dog = [] {
                model::Dog dog{Dog::Id{42}, "Pluto"s, {42.2, 12.5}};
                dog.AddScore(42);
                dog.AddLootObjectToBagpack({LootObject::Id{10}, 2u, 15u});
                dog.AddLootObjectToBagpack({LootObject::Id{4}, 5u, 6u});
                dog.SetDirection(model::Dog::Direction::EAST);
                dog.SetSpeed(2.3);
                dog.SetCoorginates({2.2, 2.5});
                return dog;
            }();
            SECTION("Dog serialization with DogRepr"){
                WHEN("dog is serialized") {
                    {
                        serialization::DogRepr repr{dog};
                        output_archive << repr;
                    }

                    THEN("it can be deserialized") {
                        InputArchive input_archive{strm};
                        serialization::DogRepr repr;
                        input_archive >> repr;
                        const auto restored = repr.Restore();
                        CheckDogs(dog, restored);
                    }
                }
            }
            SECTION("Dog serialization with function"){
                WHEN("dog is serialized") {
                    output_archive << dog;

                    THEN("it can be deserialized") {
                        InputArchive input_archive{strm};
                        model::Dog restored;
                        input_archive >> restored;
                        CheckDogs(dog, restored);
                    }
                }
            }
        }
    }
    SECTION("GameSession params Serialization") {
        GIVEN("GameSession with static params") {
            model::Map map(model::Map::Id{"MapId"}, "MapName");
            size_t index = 42;
            size_t dog_start_id = 4;
            size_t loot_object_start_id = 5;
            model::GameSession session(&map, index, true, {1s, 0.5}, 1000, [](model::Dog::Id, const model::Map::Id&){}, dog_start_id, loot_object_start_id);
            model::Dog dog{Dog::Id{42}, "Pluto"s, {42.2, 12.5}};
            dog.AddScore(42);
            dog.AddLootObjectToBagpack({LootObject::Id{10}, 2u, 15u});
            dog.AddLootObjectToBagpack({LootObject::Id{4}, 5u, 6u});
            dog.SetDirection(model::Dog::Direction::EAST);
            dog.SetSpeed(2.3);
            dog.SetCoorginates({2.2, 2.5});
            session.AddDog(model::Dog{dog});
            LootObject loot_obj(LootObject::Id{7}, 6, 7);
            PointDouble loot_obj_coords(20.0, 15.5);
            session.AddLootObject(loot_obj, loot_obj_coords);

            WHEN("Params are serialized") {
                auto content = session.GetSessionStateContent();
                output_archive << content;

                THEN("it can be deserialized") {
                    InputArchive input_archive{strm};
                    model::GameSession::StateContent restored;
                    input_archive >> restored;
                    CHECK(*restored.map_id == *map.GetId());
                    CHECK(*restored.session_id == index);
                    CHECK(restored.dogs_join == dog_start_id);
                    CHECK(restored.objects_spawned == loot_object_start_id);
                    REQUIRE_THAT(restored.dogs, SizeIs(1));
                    CheckDogs(restored.dogs.front(), dog);
                    REQUIRE_THAT(restored.loot_objects, SizeIs(1));
                    CHECK(restored.loot_objects.front().first == loot_obj);
                    CHECK(restored.loot_objects.front().second == loot_obj_coords);
                }
            }
        }
    }

    SECTION("Players state Serialization") {
        GIVEN("Some players") {
            app::Players players;
            app::PlayerTokens tokens;
            model::Map map1(model::Map::Id{"Map1"}, "Map1Name");
            model::GameSession session1(&map1, 1, true, {1s, 0.5}, 1000, 0, 0);
            model::Dog dog1{Dog::Id{42}, ""s, {}};
            auto& pl1 = players.AddPlayer(&dog1, &session1);
            auto pl1_token = tokens.AddPlayer(pl1);

            model::Map map2(model::Map::Id{"Map2"}, "Map2Name");
            model::GameSession session2(&map2, 1, true, {}, 1000, [](model::Dog::Id, const model::Map::Id&){}, 0, 0);
            model::Dog dog2{Dog::Id{13}, ""s, {}};
            auto& pl2 = players.AddPlayer(&dog2, &session1);
            auto pl2_token = tokens.AddPlayer(pl2);

            WHEN("Players Params are serialized") {
                auto content = tokens.GetPlayersState();
                output_archive << content;
                std::unordered_map<Token, Player*, util::TaggedHasher<Token>> given = {{pl1_token, &pl1}, {pl2_token, &pl2}};
                THEN("it can be deserialized") {
                    InputArchive input_archive{strm};
                    app::PlayersState restored;
                    input_archive >> restored;
                    REQUIRE_THAT(restored, SizeIs(given.size()));
                    for (auto& target : restored) {
                        REQUIRE(given.contains(target.token));
                        Player* player = given.at(target.token);
                        CHECK(*player->GetGameSession().GetMap().GetId() == *target.map_id);
                        CHECK(*player->GetGameSession().GetId() == *target.session_id);
                        CHECK(*player->GetDog().GetId() == *target.dog_id);
                    }
                }
            }
        }
    }




}