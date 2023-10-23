#pragma once

#include "../model/geom.h"

#include <algorithm>
#include <vector>

namespace collision_detector {

struct CollectionResult {
    bool IsCollected(double collect_radius) const {
        return proj_ratio >= 0 && proj_ratio <= 1 && sq_distance <= collect_radius * collect_radius;
    }

    double sq_distance;
    double proj_ratio;
};

CollectionResult TryCollectPoint(geom::PointDouble a, geom::PointDouble b, geom::PointDouble c);

struct Item {
    geom::PointDouble position;
    double radius;
};

struct Gatherer {
    geom::PointDouble start_pos;
    geom::PointDouble end_pos;
    double raduis;
};

class ItemGathererProvider {
public:
    size_t ItemsCount() const {
        return items_.size();
    }

    const Item& GetItem(size_t idx) const {
        return items_.at(idx);
    }

    size_t GatherersCount() const {
        return gatherers_.size();
    }

    const Gatherer& GetGatherer(size_t idx) const {
        return gatherers_.at(idx);
    }

    template<class... Args>
    size_t AddItem(Args&&... args) {
        items_.emplace_back(std::forward<Args>(args)...);
        return items_.size() - 1;
    }

    template<class... Args>
    size_t AddGatherer(Args&&... args) {
        gatherers_.emplace_back(std::forward<Args>(args)...);
        return gatherers_.size() - 1;
    }

    void ReserveGatherers(size_t size) {
        gatherers_.reserve(size);
    }

    void ReserveItems(size_t size) {
        items_.reserve(size);
    }

private:
    std::vector<Item> items_;
    std::vector<Gatherer> gatherers_;
};

struct GatheringEvent {
    size_t item_id;
    size_t gatherer_id;
    double sq_distance;
    double time;
};

std::vector<GatheringEvent> FindGatherEvents(const ItemGathererProvider& provider);

}  // namespace collision_detector