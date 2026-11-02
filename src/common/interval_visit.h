// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <boost/icl/interval_set.hpp>

namespace Common {
// Caller holds the interval set lock. The callback must not mutate the set.
// Visit only intersections, without allocating/copying the entire mapping set.
template <typename Set, typename Func>
void VisitIntervalIntersections(const Set& ranges, const typename Set::interval_type& query,
                               Func&& func) {
    if (boost::icl::is_empty(query)) {
        return;
    }
    const auto [first, last] = ranges.equal_range(query);
    for (auto it = first; it != last; ++it) {
        func(*it & query);
    }
}
} // namespace Common
