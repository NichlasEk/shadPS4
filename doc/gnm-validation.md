# GNM submission validation for homebrew

Enable with `SHADPS4_GNM_VALIDATION=1`, or use:

```sh
scripts/run-gnm-validation.sh --fullscreen false /absolute/path/eboot.bin
```

The launcher defaults to `build-validation/shadps4`; override with `SHADPS4_BINARY`.
The normal AppImage is not replaced. Structural argument and flip-buffer safety
checks are always active. Deeper PM4/mapped GPU-memory checks are opt-in.

## Coverage and returned errors

| Rejected condition | Existing GNM error |
| --- | --- |
| Empty batch, unreadable arrays/buffers, non-dword alignment, invalid size | `0x80D11000` INVALID_ARGUMENT |
| Missing/truncated prepareFlip tail | `0x80D11080` INVALID_COMMAND_BUFFER |
| Truncated PM4, empty register write, register bank overflow | `0x80D13013` VALIDATION_DCB |
| Reserved packet type 1 | `0x80D13016` BAD_OP_CODE |
| Invalid WAIT/WRITE_DATA memory | `0x80D13003` VALIDATION_RESOURCE |
| Invalid EOP/prepareFlip write destination | `0x80D13005` WRITE_EVENT_OP |
| Invalid known index address/range/capacity | `0x80D13006` INDEX_BUFFER |
| Reserved index element size | `0x80D1300A` INDEX_SIZE |

Rejections report DCB/CCB number, word offset, reason and hexadecimal error.
The entire batch is checked before any command is queued or flip is patched.
Mapped ranges must have the appropriate read/write permissions, with no holes,
reserved-only mappings or address overflow. Strict command buffers must fit the
40-bit GPU address space. VideoOut currently exposes a host allocation as firmware
labels; only that exact owned allocation is exempted from the GPU address limit.

These diagnostic returns deliberately catch some errors that physical hardware
may instead hang on; they are not a claim that retail firmware returns exactly
these errors for every malformed packet.

## Evidence, 2026-10-04

- Release shadps4 build succeeds on Linux.
- `bash scripts/test-gnm-validation.sh`: standalone ASan/UBSan protocol tests pass.
- OpenOrbis guest `tests/gnm_submission_guest.c`: fourteen API checks and final
  summary pass through the real emulator entry points. Invalid second DCB does
  not execute the valid first DCB's write. Valid NOP submission succeeds.
- Physical UT99 0.29 capture rejects the empty PS-input register write at word428
  with `0x80D13013`. The two-word regression is included in the source tests.
- Physical UT99 0.73 capture passes structural validation despite its hardware
  timeout: this is a state/cache-sensitive case, not a proven malformed packet.
- UT99 0.79 runs intro -> character creation -> DM-Tutorial under strict mode,
  visibly rendering the tutorial after login, without GNM validation rejection.
  This is emulator evidence, not physical PS4 proof.

Build the guest with `scripts/build-gnm-probe.sh` using a local OpenOrbis SDK
(`OO_PS4_TOOLCHAIN`, optionally `OO_CREATE_FSELF`). Its libc.prx is copied locally;
SDK binaries are not committed. Create a fresh `XDG_DATA_HOME` and the emulator's
`shadPS4/home/1000..1003/{savedata,inputs,trophy}` directories before a headless
run. Launch the absolute guest eboot path in a dedicated Xvfb session with strict
validation enabled. Read `shadPS4/data/gnm-validation-probe.log`; require all PASS,
including summary zero. Guest exit is currently logged by the emulator as an
unreachable-code diagnostic even for status0; use the probe results, not that
message alone. Captured game command buffers stay outside Git.

## Limits

This is submission preflight, not GPU cache/timing emulation. Unknown opcodes,
nested indirect-buffer contents and inherited index state are not guessed at.
There is no forced per-draw index rebind rule: absence of a rebind has not been
proven universally invalid. Shader flat-input semantics and resource contents
are not validated. It does not detect engine-side stale UObject pointers such
as the separate UT99 map-transition audio failure. Concurrent guest mutation
between preflight and execution is not prevented. Physical-console testing
remains necessary.
