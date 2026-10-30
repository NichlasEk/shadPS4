# Page-watch readonly-hole regression

Linux x86-64, C++20, Boost headers and pthreads are required. Run against a source
checkout containing the verifier's `src/video_core/page_manager.cpp`:

```
python3 run.py /path/to/source /tmp/page-watch-after
```

For the uncorrected source, require the specific failure instead:

```
python3 run.py /path/to/uncorrected-source /tmp/page-watch-before --expect-eacces
```

This compiles the real PageManager translation unit, page state/counting,
range batching, region masks and locks. It does not copy the protection
algorithm into the test. Its dependencies are deliberately isolated:

- Rasterizer is a test double containing the GPU interval registry. It does not
  run Vulkan, buffers, rendering, or concurrent GPU submission.
- AddressSpace calls real Linux `mprotect` and converts failure into an exception
  instead of terminating the test at the emulator's assertion.
- Access-violation registration and logging are inert; no guest fault is handled.
  Child processes test real write faults, with core dumps disabled.

Three contiguous pages contain writable anonymous memory, an O_RDONLY shared
file, then writable anonymous memory. Only the outside pages are registered as
GPU memory. Two overlapping watchers and their removals exercise protection
batching across the hole. The corrected code restores writes on both outside
pages while the file stays readonly and retains its marker. The control requires
EACCES on final watcher removal, the same kernel failure as the captured emulator
assertion. An unexpectedly passing old-code control fails the test.

This is component evidence for clipping protections, not a complete guest/GPU
reproduction of the scheduling that produced that assertion. It does not test
the userfaultfd backend or solve the separate Roblox worker null-pointer fault.
