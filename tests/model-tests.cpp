#include <cmath>
#include <catch2/catch_test_macros.hpp>

#include "../src/model/geom.h"
#include "../src/model/model.h"

using namespace std::literals;
using namespace  model;
using namespace  loot_gen;

SCENARIO("Loot spawn") {
    GIVEN("Game session with 1 map with 1 road") {
        Map map(Map::Id{"id"s}, "name"s);
        map.SetDogSpeed(3).SetDogBagCapacity(3);
        map.AddLootTypeWorth(1);
        Road road(Road::HORIZONTAL, {0 ,0}, 10);
        map.AddRoad(road);
        GameSession session(&map, 0, false, {5s, 1.0}, 1000, {});
        WHEN("Playerd joined and enough time passed") {
            auto* dog = session.NewDog("dog"s);
            session.OnTick(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::duration<size_t, std::milli>(5000)));
            THEN("Loot object must be spawned") {
                REQUIRE(session.GetLootObjects().size() == 1);
            }
        }
    }
}