# Mapping intersection traversal

CPU write tracking used a temporary interval-set intersection to clip page
protection to live GPU mappings. This copied the registry on every query.
VisitIntervalIntersections uses equal_range and clips overlapping intervals
without allocating. The existing shared lock remains held for callbacks.
Callbacks must not mutate the registry. Mapping boundaries, permissions and
GPU writeback behavior are unchanged.

Build with ENABLE_TESTS and run ctest -R mapped_interval_visit, or run the
standalone test directly with system Boost headers:

```
c++ -std=c++20 -O2 -I src tests/test_interval_visit.cpp -o /tmp/interval-visit-test
/tmp/interval-visit-test
```

The standalone test compares 25,604 queries with the previous algorithm and
asserts allocation-free traversal. A 4,096-interval/2,000-query microbenchmark
counts allocations; its timing is not a game-performance claim.

The same helper/renderer change was built and tested in the independent e3ce810
native-runtime verifier used by RobduxPs4. Its real PageManager regression test
preserved a read-only file hole between GPU mappings, and the complete native
GPU probe passed with Vulkan validation (run-c4f_8paq). This fork branch has the
standalone helper test; a full build of this branch was not performed.

Live Roblox measurements on the verifier with upload-page reuse enabled showed
about 85–87 ms vertex packing versus 206–210 ms in the earlier verifier run. Those
are different sessions/workloads and do not establish a fixed FPS improvement.
Reuse still performed worse than releasing upload pages, so the port keeps
reuse opt-in. This change does not claim to solve the separate intermittent
command-pool teardown race or prove physical-console behavior.
