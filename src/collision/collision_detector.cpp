#include "collision_detector.h"
#include <cassert>

namespace collision_detector {

CollectionResult TryCollectPoint(geom::PointDouble a, geom::PointDouble b, geom::PointDouble c) {
    const double u_x = c.x - a.x;
    const double u_y = c.y - a.y;
    const double v_x = b.x - a.x;
    const double v_y = b.y - a.y;
    const double u_dot_v = u_x * v_x + u_y * v_y;
    const double u_len2 = u_x * u_x + u_y * u_y;
    const double v_len2 = v_x * v_x + v_y * v_y;
    const double proj_ratio = u_dot_v / v_len2;
    const double sq_distance = u_len2 - (u_dot_v * u_dot_v) / v_len2;

    return CollectionResult(sq_distance, proj_ratio);
}

std::vector<GatheringEvent> FindGatherEvents(
    const ItemGathererProvider& provider) {
    std::vector<GatheringEvent> detected_events;

    static auto is_points_equal = [](geom::PointDouble p1, geom::PointDouble p2) {
        return p1.x == p2.x && p1.y == p2.y;
    };

    for (size_t g = 0; g < provider.GatherersCount(); ++g) {
        Gatherer gatherer = provider.GetGatherer(g);
        if (is_points_equal(gatherer.start_pos, gatherer.end_pos)) {
            continue;
        }
        for (size_t i = 0; i < provider.ItemsCount(); ++i) {
            Item item = provider.GetItem(i);
            auto collect_result = TryCollectPoint(gatherer.start_pos, gatherer.end_pos, item.position);
            if (collect_result.IsCollected(gatherer.raduis + item.radius)) {
                GatheringEvent event{.item_id = i,
                                   .gatherer_id = g,
                                   .sq_distance = collect_result.sq_distance,
                                   .time = collect_result.proj_ratio};
                detected_events.push_back(event);
            }
        }
    }

    std::sort(detected_events.begin(), detected_events.end(),
              [](const GatheringEvent& lhs, const GatheringEvent& rhs) {
                  return lhs.time < rhs.time;
              });

    return detected_events;
}

}  // namespace collision_detector