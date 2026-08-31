# Contributing to sferic

sferic is a clean-room, MIT-licensed firmware. **Do not paste code from Bruce
or any other GPL/AGPL project** — Bruce is a behavioural reference only
([ADR-0001](docs/adr/0001-onboard-rf-and-tool-capabilities.md)). Datasheets,
protocol specs, and your own implementation are fine.

## Ground rules

- **Scope of use.** Capture / replay / decode / scan for authorized testing
  and education. No untargeted-DoS features as headliners; dual-use
  primitives stay behind the on-device arm/confirm guard.
- **No secrets.** Wi-Fi creds, tokens, keys — never committed. `*.example`
  files only.
- **Design changes go through an ADR.** Anything that changes architecture,
  a wire format, or the tool contract: add `docs/adr/NNNN-*.md` in the same
  PR. Change records live in the PR, not in the repo history docs.

## Build & test

```sh
pio run -e sferic            # default build (must pass)
pio run -e sferic-usbhid     # HID variant (must pass)
pio test -e native           # host unit tests (must pass)
```

CI runs all three on every push and PR; keep it green. The `native` env
builds only dependency-free sources (`test_build_src` + `build_src_filter`
in `platformio.ini`) — keep hardware-independent logic that way and add a
test for it in `test/test_main.cpp`.

## Adding a tool

One `registry.add({...})` line in `src/menu_tree.cpp` + a `Screen`
subclass in `src/<tool>.{h,cpp}`. Give it a headless `run()` when it has a
non-interactive shape and doesn't need a radio the SoftAP is using
([`docs/mcp/headless.md`](docs/mcp/headless.md)). Emitting tools
(IR/RF/keystrokes) must go through `src/confirm.h`.

## Pull requests

- Branch off `main`, open a PR — no direct pushes.
- Keep commits focused; present-tense subject line.
- Note on-hardware verification status in the PR description (which envs
  build, what you flashed and saw).

## Releases

Tag `vX.Y.Z` on `main` (matching `SFERIC_VERSION` in `src/version.h`). CI
builds both envs and publishes a GitHub Release with `sferic-*.bin` and
auto-generated notes.
