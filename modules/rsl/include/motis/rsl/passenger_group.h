#pragma once

#include <cstdint>
#include <vector>

#include "motis/core/journey/journey.h"

#include "motis/rsl/compact_journey.h"

namespace motis::rsl {

struct edge;

struct passenger_group {
  compact_journey compact_planned_journey_;
  std::uint16_t passengers_{};
  std::uint64_t id_{};
  std::uint64_t sub_id_{};
  bool ok_{true};
  std::vector<edge*> edges_{};
};

}  // namespace motis::rsl