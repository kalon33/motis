#include "icc/journey_to_response.h"

#include "nigiri/routing/journey.h"
#include "nigiri/rt/frun.h"
#include "nigiri/timetable.h"
#include "nigiri/types.h"

#include "utl/enumerate.h"

namespace n = nigiri;

namespace icc {

std::int64_t to_ms(n::unixtime_t const t) {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             t.time_since_epoch())
      .count();
}

std::int64_t to_seconds(n::i32_minutes const t) {
  return std::chrono::duration_cast<std::chrono::seconds>(t).count();
}

std::int64_t to_ms(n::i32_minutes const t) {
  return std::chrono::duration_cast<std::chrono::milliseconds>(t).count();
}

api::Place to_place(n::timetable const& tt, n::location_idx_t const l) {
  auto const pos = tt.locations_.coordinates_[l];
  return {.name_ = std::string{tt.locations_.names_[l].view()},
          .lat_ = pos.lat_,
          .lon_ = pos.lng_};
}

std::string to_str(n::color_t const c) {
  return fmt::format("{:06x}", to_idx(c) & 0x00ffffff);
}

api::Itinerary journey_to_response(n::timetable const& tt,
                                   n::rt_timetable const* rtt,
                                   n::routing::journey const& j) {
  auto itinerary = api::Itinerary{
      .duration_ = to_seconds(j.arrival_time() - j.departure_time()),
      .startTime_ = to_ms(j.departure_time()),
      .endTime_ = to_ms(j.arrival_time()),
      .transfers_ = std::max(
          0L, utl::count_if(j.legs_, [](auto&& leg) {
                return holds_alternative<n::routing::journey::run_enter_exit>(
                    leg.uses_);
              }) - 1)};

  for (auto const [leg_i, j_leg] : utl::enumerate(j.legs_)) {
    auto const write_leg = [&](auto&& x,
                               api::ModeEnum const mode) -> api::Leg& {
      auto& leg = itinerary.legs_.emplace_back();
      leg.mode_ = mode;
      leg.from_ = to_place(tt, j_leg.from_);
      leg.to_ = to_place(tt, j_leg.to_);
      leg.from_.departure_ = leg.startTime_ = to_ms(j_leg.dep_time_);
      leg.to_.arrival_ = leg.endTime_ = to_ms(j_leg.arr_time_);
      leg.duration_ = to_seconds(j_leg.arr_time_ - j_leg.dep_time_);
      return leg;
    };

    std::visit(
        utl::overloaded{
            [&](n::routing::journey::run_enter_exit const& t) {
              // TODO interlining
              auto const fr = n::rt::frun{tt, rtt, t.r_};
              auto const enter_stop = fr[t.stop_range_.from_];
              auto const exit_stop = fr[t.stop_range_.to_ - 1U];
              auto const color =
                  enter_stop.get_route_color(n::event_type::kDep);
              auto const agency = enter_stop.get_provider(n::event_type::kDep);

              auto& leg = write_leg(t, api::ModeEnum::TRANSIT);
              leg.headsign_ = enter_stop.direction(n::event_type::kDep);
              leg.routeColor_ = to_str(color.color_);
              leg.routeTextColor_ = to_str(color.text_color_);
              leg.realTime_ = fr.is_rt();
              leg.tripId_ = fr.id().id_;  // TODO source_idx
              leg.agencyName_ = agency.long_name_;
              leg.agencyId_ = agency.short_name_;
              leg.departureDelay_ =
                  to_ms(enter_stop.delay(n::event_type::kDep));
              leg.arrivalDelay_ = to_ms(exit_stop.delay(n::event_type::kArr));

              for (auto i = t.stop_range_.from_ + 1U;
                   i < t.stop_range_.to_ - 2U; ++i) {
                auto const stop = fr[t.stop_range_.from_];
                auto& p = leg.intermediateStops_.emplace_back(
                    to_place(tt, stop.get_location_idx()));
                p.departure_ = to_ms(stop.time(n::event_type::kDep));
                p.departureDelay_ = to_ms(stop.delay(n::event_type::kDep));
                p.arrival_ = to_ms(stop.time(n::event_type::kDep));
                p.arrivalDelay_ = to_ms(stop.delay(n::event_type::kDep));
              }
            },
            [&](n::footpath const fp) { write_leg(fp, api::ModeEnum::WALK); },
            [&](n::routing::offset const x) {
              write_leg(x, api::ModeEnum{x.transport_mode_id_});
            }},
        j_leg.uses_);
  }

  return itinerary;
}

}  // namespace icc