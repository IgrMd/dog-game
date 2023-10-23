#define _USE_MATH_DEFINES
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_container_properties.hpp>
#include <catch2/matchers/catch_matchers_templated.hpp>

#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "../src/collision/collision_detector.h"

#include <cmath>
#include <functional>
#include <sstream>
#include <tuple>

using namespace collision_detector;
using namespace geom;
using namespace Catch::Matchers;
using namespace std::literals;

static constexpr double DOG_WIDTH = 0.6;
static constexpr double ITEM_WIDTH = 0.1;

static constexpr double EPSILON = 1e-10;

namespace Catch {
template<>
struct StringMaker<GatheringEvent> {
  static std::string convert(GatheringEvent const& value) {
      std::ostringstream tmp;
      tmp << "(" << value.item_id << "," << value.gatherer_id << "," << value.sq_distance << "," << value.time << ")";

      return tmp.str();
  }
};
}  // namespace Catch

struct EventComparator {
    bool operator()(const GatheringEvent& lhs, const GatheringEvent& rhs) const {
        if (std::tie(lhs.gatherer_id, lhs.item_id) != std::tie(rhs.gatherer_id, rhs.item_id)) {
            return false;
        }
        if (std::abs(lhs.sq_distance - rhs.sq_distance) > EPSILON) {
            return false;
        }
        if (std::abs(lhs.time - rhs.time) > EPSILON) {
            return false;
        }
        return true;
    }
};

bool operator==(const GatheringEvent& lhs, const GatheringEvent& rhs) {
    return EventComparator{}(lhs, rhs);
}

template <typename Range, typename Comparator>
struct IsEqualRangeMatcher : Catch::Matchers::MatcherGenericBase {
    IsEqualRangeMatcher(const Range& range, Comparator cmp)
        : range_{range}
        , cmp_{cmp} {
    }
    IsEqualRangeMatcher(IsEqualRangeMatcher&&) = default;

    template <typename OtherRange>
    bool match(const OtherRange& other) const {
        using std::begin;
        using std::end;

        return std::equal(begin(range_), end(range_), begin(other), end(other), cmp_);
    }

    std::string describe() const override {
        // Описание свойства, проверяемого матчером:
        return "Is equal range of: "s + Catch::rangeToString(range_);
    }

private:
    const Range& range_;
    Comparator cmp_;
};

template<typename Range, typename Comparator>
IsEqualRangeMatcher<Range, Comparator> IsEqualRange(const Range& range, Comparator cmp) {
    return IsEqualRangeMatcher<Range, Comparator>{range, cmp};
}

SCENARIO("Collision detection") {
    GIVEN("ItemGathererProvider") {
        ItemGathererProvider gatherer_provider;
        WHEN("No items added") {
            gatherer_provider.AddGatherer(Gatherer{{}, {0, 2}, DOG_WIDTH});
            gatherer_provider.AddGatherer(Gatherer{{0, 1}, {0, 2}, DOG_WIDTH});
            gatherer_provider.AddGatherer(Gatherer{{0, 0}, {5, 0}, DOG_WIDTH});
            THEN("No events expected") {
                auto events = FindGatherEvents(gatherer_provider);
                CHECK_THAT(events, IsEmpty());
            }
        }

        WHEN("No gatherers added") {
            gatherer_provider.AddItem(Item{{0, 0}, ITEM_WIDTH});
            gatherer_provider.AddItem(Item{{0, 1}, ITEM_WIDTH});
            gatherer_provider.AddItem(Item{{5, 0}, ITEM_WIDTH});
            THEN("No events expected") {
                auto events = FindGatherEvents(gatherer_provider);
                CHECK_THAT(events, IsEmpty());
            }
        }

        WHEN("Single gatherer collects single item") {
            gatherer_provider.AddGatherer(Gatherer{{}, {0, 2}, DOG_WIDTH});
            gatherer_provider.AddItem(Item{{0.2, 1}, ITEM_WIDTH});
            THEN("Item must be collected") {
                auto events = FindGatherEvents(gatherer_provider);
                REQUIRE_THAT(events, SizeIs(1));
                AND_THEN("Event params must be correct") {
                    auto event = events.front();
                    CHECK(event == GatheringEvent{0, 0, 0.2*0.2, 0.5});
                }
            }
        }

        WHEN("Single gatherer collects one of many items") {
            gatherer_provider.AddGatherer(Gatherer{{}, {0, 2}, DOG_WIDTH});
            gatherer_provider.AddItem(Item{{  5, 1}, ITEM_WIDTH});
            gatherer_provider.AddItem(Item{{  0, 3}, ITEM_WIDTH});
            gatherer_provider.AddItem(Item{{0.2, 1}, 1});
            THEN("Only one item must be collected") {
                auto events = FindGatherEvents(gatherer_provider);
                REQUIRE_THAT(events, SizeIs(1));
                AND_THEN("Event params must be correct") {
                    auto event = events.front();
                    CHECK(event == GatheringEvent{2, 0, 0.2*0.2, 0.5});
                }
            }
        }

        WHEN("Single gatherer collects items according to widths") {
            gatherer_provider.AddGatherer(Gatherer{{}, {0, 2}, DOG_WIDTH});
            AND_WHEN("Item width not zero"){
                gatherer_provider.AddItem(Item{{0.65, 1}, ITEM_WIDTH});
                THEN("Item must be collected") {
                    auto events = FindGatherEvents(gatherer_provider);
                    REQUIRE_THAT(events, SizeIs(1));
                    AND_THEN("Event params must be correct") {
                        auto event = events.front();
                        CHECK(event == GatheringEvent{0, 0, 0.65*0.65, 0.5});
                    }
                }
            }
            AND_WHEN("Item width is zero"){
                gatherer_provider.AddItem(Item{{0.65, 1}, 0});
                THEN("Item must not be collected") {
                    auto events = FindGatherEvents(gatherer_provider);
                    REQUIRE_THAT(events, IsEmpty());
                }
            }
        }

        WHEN("Single gatherer collects some items in a row") {
            gatherer_provider.AddGatherer(Gatherer{{0, 0}, {0, 5}, DOG_WIDTH});
            gatherer_provider.AddItem(Item{{  0, -1}, ITEM_WIDTH});
            gatherer_provider.AddItem(Item{{   0, 3}, ITEM_WIDTH});
            gatherer_provider.AddItem(Item{{ 0.1, 2}, ITEM_WIDTH});
            gatherer_provider.AddItem(Item{{-0.2, 1}, ITEM_WIDTH});
            THEN("3 of 4 items must be collected") {
                auto events = FindGatherEvents(gatherer_provider);
                REQUIRE_THAT(events, SizeIs(3));
                AND_THEN("Items must be collected in order 3-2-1") {
                    CHECK_THAT(events, IsEqualRange(
                        std::vector{
                            GatheringEvent{3, 0,-0.2 *-0.2, 1. / 5},
                            GatheringEvent{2, 0, 0.1 * 0.1, 2. / 5},
                            GatheringEvent{1, 0, 0.0 * 0.0, 3. / 5}
                        },
                        EventComparator()
                    ));
                }
            }
        }

        WHEN("Two gatherers collects 1 item crossing") {
            gatherer_provider.AddGatherer(Gatherer{{ 2.0, 4.0}, { 10., 4.0}, DOG_WIDTH});
            gatherer_provider.AddGatherer(Gatherer{{ 8.0, 6.0}, { 8.0, 2.0}, DOG_WIDTH});
            gatherer_provider.AddItem(Item{{ 8.5, 3.5}, ITEM_WIDTH});
            THEN("Item must be twice") {
                auto events = FindGatherEvents(gatherer_provider);
                CHECK_THAT(events, IsEqualRange(
                    std::vector{
                        GatheringEvent{0, 1, 0.5 * 0.5, (3.5 - 6.) / (2.0 - 6.)},
                        GatheringEvent{0, 0, 0.5 * 0.5, (8.5 - 2.) / (10. - 2.)}
                    },
                    EventComparator()
                ));
            }
        }

         WHEN("Gatherer walks diagonal") {
            gatherer_provider.AddGatherer(Gatherer{{ 1.0, 1.0}, { 5.0, 5.0}, DOG_WIDTH});
            gatherer_provider.AddItem(Item{{ 3.0, 3.0}, ITEM_WIDTH});
            THEN("Item must be collected") {
                auto events = FindGatherEvents(gatherer_provider);
                REQUIRE_THAT(events, SizeIs(1));
                CHECK(events.front() == GatheringEvent{0, 0, 0.0, 0.5});
            }
        }

        WHEN("Two gatherers collects 4 items") {
            gatherer_provider.AddGatherer(Gatherer{{ 0.0, 0.0}, { 0.0, 10.}, DOG_WIDTH});
            gatherer_provider.AddGatherer(Gatherer{{ -0.1, 20.}, { -0.1, 0.1}, DOG_WIDTH});
            gatherer_provider.AddItem(Item{{ -0.2, 1}, ITEM_WIDTH});
            gatherer_provider.AddItem(Item{{  0.2, 2}, ITEM_WIDTH});
            gatherer_provider.AddItem(Item{{-0.61, 3}, ITEM_WIDTH});
            gatherer_provider.AddItem(Item{{  0.5,19}, ITEM_WIDTH});
            THEN("All of items must be collected") {
                auto events = FindGatherEvents(gatherer_provider);
                REQUIRE_THAT(events, SizeIs(7));
                AND_THEN("Items must be collected in order") {
                    CHECK_THAT(events, IsEqualRange(
                        std::vector{
                            GatheringEvent{3, 1, 0.6 * 0.6, (20. - 19.) / (20. - 0.1)},
                            GatheringEvent{0, 0,-0.2 *-0.2,  1. / 10},
                            GatheringEvent{1, 0, 0.2 * 0.2,  2. / 10},
                            GatheringEvent{2, 0,-0.61*-0.61, 3. / 10},
                            GatheringEvent{2, 1,-0.51*-0.51, (20. - 3.) / (20. - 0.1)},
                            GatheringEvent{1, 1, 0.3 * 0.3,  (20. - 2.) / (20. - 0.1)},
                            GatheringEvent{0, 1,-0.1 *-0.1,  (20. - 1.) / (20. - 0.1)}
                        },
                        EventComparator()
                    ));
                }
            }
        }
    }
}
