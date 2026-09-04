# Contributing to ManiaDanOverlay

Thanks for taking an interest in ManiaDanOverlay.

ManiaDanOverlay currently targets 4K osu mania on Windows and Linux X11, with a focus on compact presentation, predictable behavior, low runtime overhead, and single-file release builds.

Those targets are the current supported baseline, not a permanent limit on the project. Contributions that add new platforms, key modes, calibration sets, or related analysis support are welcome when they preserve the existing architecture and do not regress currently supported behavior.

## Project scope

The current first-class supported targets are:

```
Windows x64
Linux X11 x64
4K osu!mania
```

The project is open to broader support. Contributions for additional desktop platforms, other osu!mania key modes, new calibration data, or alternative calibration sets are welcome when they fit the existing design.

Expansion work should follow these rules:

- existing Windows and Linux X11 behavior must continue to work
- existing 4K behavior and calibration must not be broken as an accidental side effect
- platform-specific code should remain behind small platform interfaces
- key-mode-specific parsing, features, and calibration should be explicit rather than hidden behind 4K assumptions
- new calibration should document what data or methodology it is based on
- shared engine and UI code should remain shared where practical
- release builds should preserve the single-executable distribution model

Wayland, macOS, additional key modes, and other platforms are not currently shipped as official targets, but they are not rejected solely because they are outside the current V1 support matrix.

A contribution does not need to solve every platform or key mode at once. It does need to avoid making shared code depend on behavior that exists only on one target.

## Before making a change

For small fixes, documentation changes, and straightforward platform work, a pull request can be opened directly.

For larger changes, open an issue first if the change would affect any of the following:

- Reform or LN Course rating behavior
- rank or stage calibration
- Sunny calculation behavior
- chart feature extraction
- Tosu protocol handling
- application settings format
- major UI layout changes
- new runtime dependencies
- release packaging
- a new operating-system target
- a new mania key mode
- a new or substantially revised calibration set

This helps coordinate larger changes before significant work is done and makes it easier to preserve compatibility with the current architecture.

## Development setup

Clone the repository with submodules:

```bash
git clone --recurse-submodules <repository-url>
cd ManiaDanOverlay
```

If the repository was cloned without submodules:

```bash
git submodule update --init --recursive
```

The project uses:

- C11 for the main application
- C++17 for the MinaCalc bridge and MinaCalc itself
- CMake 3.20 or newer
- raylib
- yyjson
- MinaCalc Standalone

Tosu is required for live runtime testing.

## Linux X11 development

On Debian, Ubuntu, or Linux Mint, a typical development setup is:

```bash
sudo apt update
sudo apt install \
  build-essential \
  cmake \
  git \
  libx11-dev \
  libxrandr-dev \
  libxi-dev \
  libxcursor-dev \
  libxinerama-dev \
  libgl1-mesa-dev
```

A normal CMake debug build can be used during development.

For the supported Linux release build:

```bash
./scripts/build-linux-x11-release.sh --clean
```

The release output is expected at:

```
dist/ManiaDanOverlay
```

The current official Linux release build uses X11. A Wayland contribution is welcome if it is implemented as a clean platform addition and does not weaken or complicate the existing X11 path unnecessarily.

## Windows development

The current release pipeline cross-compiles the Windows build from Linux with MinGW-w64.

On Linux:

```bash
sudo apt update
sudo apt install mingw-w64
```

Build with:

```bash
./scripts/build-windows-mingw64.sh --clean
```

The release output is expected at:

```
dist-windows/ManiaDanOverlay.exe
```

Windows-specific changes should be tested on a real Windows 10 or Windows 11 system before they are considered complete.

## Repository layout

The main source tree is organized by responsibility:

```
src/app/        application identity and shared application definitions
src/beatmap/    beatmap representation, parsing, and active map fetching
src/engine/     Sunny, Reform, LN Course, MinaCalc bridge, and chart analysis
src/net/        native localhost HTTP client
src/platform/   operating-system-specific code
src/settings/   persistent application settings
src/tosu/       Tosu state and telemetry handling
src/ui/         rendering, themes, fonts, HUD, settings, and diagnostics
src/main.c      main application loop
```

Keep platform-independent logic out of `src/platform/` unless it genuinely needs operating-system APIs.

## Code style

The existing style should be followed rather than introducing a second style into the same module.

General rules:

- Use C11 for new application code unless C++ is required by an existing C++ dependency.
- Keep functions focused and reasonably small.
- Prefer explicit data flow over hidden global state.
- Check allocation and I/O failures where failure is realistic.
- Clamp and validate external values before using them.
- Avoid unnecessary abstractions for code that has only one real implementation.
- Do not add a dependency when a small native implementation already solves the problem cleanly.
- Keep comments useful and specific. Do not narrate obvious code.
- Use descriptive names instead of single-letter identifiers outside short local loops or formulas.
- Do not reformat unrelated files as part of a functional change.

Warnings introduced by a change should be fixed before submitting it.

## Analysis and calibration changes

The Dan and LN analysis code is one of the most sensitive parts of the project.

Calibration work is welcome, including new calibration sets for additional key modes or well-supported revisions to existing behavior. Changes should have a clear structural or data-driven reason and should be checked against several relevant charts rather than being tuned around a single map.

When changing Reform, LN Course, Sunny, chart features, or ruler selection, include the following in the pull request description:

- what behavior was wrong
- what part of the algorithm changed
- why the new behavior is more appropriate
- example maps or chart types used for testing
- relevant rates used during testing
- whether the change affects displayed rating values or only descriptive labels

Player-facing LN profile labels are separate from LN Course rating calibration. If a change only affects labels such as `Jack / Technical LN` or `Inverse / Wall LN`, keep that logic separate from the LN Course DP calculation.

When adding calibration for another key mode or rating family, keep its data and assumptions explicit. Do not silently reinterpret the existing 4K calibration tables for a different key mode. Shared formulas are fine when they are genuinely shared, but calibration data should remain identifiable and maintainable.

MinaCalc data should remain descriptive evidence. It should not silently replace Reform or LN Course calibration.

## Beatmap and rate handling

The current release supports 4K osu mania. Support for additional mania key modes is welcome when it is added without weakening the existing 4K path or spreading fixed four-key assumptions through shared code.

For the existing 4K path, beatmap changes must preserve:

- tap note timing
- LN start timing
- LN release timing
- existing four-column mapping
- timing-point information needed by analysis
- checksum-based map refresh behavior

Clock-rate handling uses the numeric rate supplied by Tosu. Do not replace this with fixed assumptions for DT, NC, HT, or DC.

Supported rate-changing mods are currently:

```
DT
NC
HT
DC
```

Wind Up and Wind Down are intentionally ignored unless a good use of them is made.

## Tosu and networking

ManiaDanOverlay communicates with Tosu only over localhost:

```
127.0.0.1:24050
```

The current endpoints are:

```js
GET / json / v2;
GET / files / beatmap / file;
```

The native HTTP client exists specifically to avoid a libcurl runtime dependency.

Do not replace it with a general-purpose networking dependency unless there is a strong technical reason and the release-size and packaging consequences have been discussed first.

Networking changes should preserve handling for both normal `Content-Length` responses and chunked transfer encoding.

## User interface changes

The default HUD is intentionally compact.

Its priorities are:

1. density graph
2. current Reform or LN Course result
3. pattern or LN profile information
4. small supporting status information

The HUD should not gradually turn back into the Extra Info view.

Song title, artist, mapper, difficulty name, BPM, object count, and similar metadata belong in Extra Info unless there is a strong reason to change the product direction.

UI changes should preserve:

- the dark neutral application palette
- Dan-dependent accent colors
- readable text at compact window sizes
- arbitrary window resizing
- HUD and Extra Info modes
- Overview and Focus density modes
- the full-height density playhead
- settings usability at small overlay sizes

Avoid adding decorative effects that make the overlay harder to read or visually inconsistent with the existing design.

## Settings

Settings are stored in JSON and currently use schema version 3.

Linux:

```
$XDG_CONFIG_HOME/ManiaDanOverlay/settings.json
```

or:

```
~/.config/ManiaDanOverlay/settings.json
```

Windows:

```
%APPDATA%\ManiaDanOverlay\settings.json
```

When adding a setting:

- provide a sensible default
- validate the value when loading it
- preserve compatibility with existing settings files when practical
- update the settings UI if the value is user-facing
- update the README if the setting affects normal operation

Temporary states such as diagnostics and OBS Hide should not overwrite the user's normal saved window position or size.

## Platform-specific code

Platform-specific code should be kept behind a small shared interface where practical.

Examples include:

- socket initialization and cleanup
- config-directory discovery
- Windows application entry behavior
- X11-specific window behavior
- future platform-specific OBS workarounds

Do not duplicate engine or UI logic into separate Windows and Linux copies. The same rule applies to future platform ports.

New platform support should normally extend the shared platform abstraction rather than fork the application. The goal is one shared application with small platform layers, regardless of how many operating systems are supported.

## Dependencies

The current built-in dependencies are:

- raylib
- yyjson
- MinaCalc Standalone

Keep dependencies minimal.

Before adding a new library, consider:

- whether the feature can be implemented clearly in a small amount of native code
- binary-size impact
- Windows and Linux support
- static-linking support
- license compatibility
- whether it introduces runtime files or DLLs

A new dependency should not be added only for convenience if it compromises the single-file release model.

## Fonts and binary assets

Release builds embed the UI font data into the executable at build time.

Do not change the application back to requiring a runtime font directory.

Avoid committing generated build artifacts or generated font arrays unless the repository explicitly adopts them as source files.

## Single-file release requirement

This is a hard project requirement.

The intended downloads are:

```
Linux X11: ManiaDanOverlay
Windows:   ManiaDanOverlay.exe
```

A release contribution must not introduce required companion files such as:

- DLLs
- shared libraries shipped beside the executable
- runtime asset folders
- Python environments
- helper executables
- external MinaCalc processes
- libcurl

Normal operating-system libraries are not considered companion files.

Per-user settings files created after launch are also not part of the distributed application package.

## Testing a change

At minimum, test the area you changed and make sure the application still starts correctly.

For general application changes, a useful smoke test is:

1. Start Tosu and osu!lazer.
2. Start ManiaDanOverlay.
3. Load a 4K rice map.
4. Confirm the HUD updates.
5. Switch between HUD and Extra Info.
6. Switch between Overview and Focus.
7. Change the Focus span.
8. Resize the window smaller, wider, and taller.
9. Open F2 settings at a compact window size.
10. Load an LN-heavy map.
11. Confirm LN Course and LN profile information appear correctly.
12. Change the clock rate if the chart supports it.
13. Restart the application and confirm settings persist.

If the change affects Linux window behavior, test X11 specifically.

If the change affects Windows, test the actual `.exe` on Windows rather than relying only on a successful cross-compile.

If the change adds a new platform or key mode, test that new target directly and also confirm that the existing Windows/Linux X11 4K path still builds and behaves normally. Calibration changes should include representative examples for every calibration set they modify.

## Release checks

Before a release-related pull request is considered complete, verify the appropriate release target.

Linux:

```bash
./scripts/build-linux-x11-release.sh --clean
```

Expected output:

```
dist/ManiaDanOverlay
```

Windows:

```bash
./scripts/build-windows-mingw64.sh --clean
```

Expected output:

```
dist-windows/ManiaDanOverlay.exe
```

The release directories should contain only the intended executable.

Do not disable the release dependency checks simply to make a build pass.

## Pull requests

Keep pull requests focused. A bug fix should not also rename unrelated modules, replace the formatting style, and add a new feature.

A useful pull request description should include:

- a short explanation of the problem
- what was changed
- which platform or subsystem is affected
- how the change was tested
- screenshots for visible UI changes
- example maps or rates for analysis changes when relevant

If there is a known limitation or unresolved edge case, state it directly.

## Commit messages

There is no strict commit-message format, but messages should describe the change clearly.

Good examples:

```
Fix density fill gaps during responsive resize
Add Windows release staging checks
Preserve normal window position during OBS hide
Adjust LN profile wall classification
```

Avoid messages such as:

```
fix
stuff
update
changes
```

## Bug reports

A useful bug report includes:

- operating system
- ManiaDanOverlay version or commit
- osu lazer or osu stable version if relevant
- Tosu version if relevant
- map or chart information when the issue is analysis-related
- active mod and clock rate
- what happened
- what was expected
- screenshot or console output when useful

For Linux windowing issues, include the desktop environment and confirm that the session is X11 rather than Wayland.

## Documentation

Update documentation when a contribution changes user-visible behavior, controls, build steps, supported platforms, dependencies, or settings.

Documentation should be direct and technical. Avoid promotional language, unnecessary formatting, and undocumented claims about how an algorithm behaves.
