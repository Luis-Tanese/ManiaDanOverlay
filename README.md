# ManiaDanOverlay

This project is a native desktop overlay for 4K osu mania. It reads live gameplay state from Tosu, parses the active beatmap, calculates Reform or LN course estimates, shows MinaCalc pattern data and renders the result in a silly compact resizable HUD (Massive inspiration from Daniel by TheBagelOfMan).

The project is written primarily in C11. MinaCalc is linked through a small C++17 bridge. The release builds are designed to ship as a single executable on Windows and Linux X11.

## Platform support

| Platform          | Status                  |
| ----------------- | ----------------------- |
| Windows 10/11 x64 | Supported               |
| Linux X11 x64     | Supported               |
| Linux Wayland     | Not currently supported |
| macOS             | Not supported           |

The overlay currently supports 4K osu mania only. I perhaps _may_ make it work for 7K, but it is far from my expertise.

## Runtime requirements

ManiaDanOverlay does not read osu memory directly. Live state is provided by [Tosu](https://github.com/tosuapp/tosu).

For normal use:

1. Start osu!stable or osu!lazer.
2. Start Tosu.
3. Start ManiaDanOverlay

The overlay connects only to Tosu on the local machine at `127.0.0.1:24050`.

It uses two Tosu endpoints:

```js
GET / json / v2;
GET / files / beatmap / file;
```

`/json/v2` provides the current client state, play state, beatmap metadata, checksum, mod information, clock rate, song position, and song length.

`/files/beatmap/file` provides the active `.osu` file so the overlay can parse the chart locally.

ManiaDanOverlay does not require an internet connection for its own analysis and does not upload chart or gameplay data.

## Controls

The controls will only work when you have the screen focused on the program itself.

| Key       | Action                                           |
| --------- | ------------------------------------------------ |
| `F2`      | Open or close Settings                           |
| `F3`      | Toggle diagnostics (More for debugging purposes) |
| `F10`     | Toggle OBS Hide mode                             |
| `G`       | Switch between Overview and Focus density modes  |
| `-`       | Use a smaller Focus time span                    |
| `+` / `=` | Use a larger Focus time span                     |

Focus span presets are:

```text
15, 30, 45, 60, 90, 120, 180 seconds
```

## User Interface

### HUD

HUD is the default view. It is inspired by [Daniel](https://github.com/TheBagelOfMan/Daniel) because it has all you need in a minimal footprint.

For rice charts, the HUD shows:

```
Est. Dan: GAMMA MID (13.4)
Stamina, Jumpstream 32.69 MSD
```

For LN Course charts, the HUD also keeps the LN profile description and MinaCalc head-pattern information visible.

The supporting HUD content can be adjusted from Settings. Mod/rate, the 4K label, graph mode, Tosu client information, and MinaCalc pattern/MSD output can be shown or hidden independently. The density graph and current Dan or LN Course result remain the core HUD elements.

### Extra Info

Extra Info keeps the full analysis layout. It has:

- artist, title, and difficulty
- active mod and rate
- current song position
- 4K object count and BPM
- Reform or LN Course result
- pattern and MSD information
- LN profile information when applicable
- density graph

## Density graph

The density graph is built from chart density samples at 250 ms intervals.

### Overview mode

Overview mode displays the entire chart at once. The graph remains fixed while the current play position moves across it.

Display-only smoothing is applied in Overview mode to keep long charts visually clean.

### Focus mode

Focus mode acts like a scrolling camera. The current play position stays near the left-middle portion of the graph while the chart moves through the selected time window.

Focus uses the detailed density data without the Overview smoothing pass.

The graph fill and outline use the current Dan or LN rank theme color.

# Boring technical stuff

## Beatmap parsing

The parser reads the active `.osu` file locally and extracts the data needed by the analysis engine.

For osu mania it reads:

- `CircleSize` for key count
- artist, title, creator, and difficulty metadata
- timing points and BPM
- tap notes
- hold-note start times
- hold-note release times
- note columns

For 4K, object X positions are converted into columns using the osu!mania playfield width and clamped to the valid four-column range.

Hold releases are preserved in the native beatmap representation because LN analysis depends on occupancy, overlap, duration, and release behavior.

## Clock-rate handling

The analysis pipeline uses the actual numeric clock rate instead of assuming fixed 1.50x DT or 0.75x HT values.

Supported rate-changing mods are:

- DT
- NC
- HT
- DC

The current rate is read from Tosu's mod rate data, with the selected mod's `speed_change` setting available as a fallback.

When a rate is applied, note heads and LN releases are converted to rate-adjusted timing before timing-sensitive analysis. This allows custom lazer rates such as 1.20x, 1.35x, or 1.80x to be evaluated directly.

## Analysis pipeline

The normal runtime flow is:

```
Tosu
  |
  | live state + active beatmap
  v
Beatmap parser
  |
  v
Rate-adjusted analysis map
  |
  +--> chart feature extraction
  |
  +--> Sunny SR
  |
  +--> MinaCalc
  |
  v
Reform / LN Course routing
  |
  v
HUD or Extra Info view
```

Map-dependent work is cached. A map is rebuilt when the active checksum changes. Rate-dependent analysis is refreshed when the numeric clock rate changes.

### Sunny SR

`src/engine/sunny_sr.c` is a native C implementation of the Sunny calculation used by the Dan analysis path.

The implementation follows the behavior used by the reference Dan-Overlay calculation, including its J, P, X, and A difficulty components, high-percentile aggregation, weighted combination, and high-SR rescaling behavior.

For calibration compat, the current Dan path analyzes note heads with OD 9 behavior matching the reference implementation.

Sunny is an internal input to the rank estimators so it is only shown in the `F3` diagnostics and not on the normal HUD.

### Reform ranking

Rice and hybrid charts use the Reform ranking path.

The available ranks are:

```
1st  2nd  3rd  4th  5th
6th  7th  8th  9th  10th
Alpha  Beta  Gamma  Delta  Epsilon
Zeta   Eta   Theta  Iota   Kappa
```

Reform uses five calibration rulers:

- General
- Jack
- Speed
- Stamina
- Tech

The ruler tables currently used by the native implementation are:

| Rank    | General |  Jack | Speed | Stamina |  Tech |
| ------- | ------: | ----: | ----: | ------: | ----: |
| 1st     |    2.94 |  2.34 |  2.94 |    3.38 |  2.84 |
| 2nd     |    3.23 |  2.35 |  3.50 |    3.48 |  3.08 |
| 3rd     |    3.51 |  3.17 |  3.78 |    3.79 |  3.09 |
| 4th     |    4.16 |  3.48 |  4.16 |    4.69 |  3.90 |
| 5th     |    4.71 |  3.98 |  4.86 |    5.23 |  4.18 |
| 6th     |    5.12 |  4.75 |  5.29 |    5.65 |  4.50 |
| 7th     |    5.36 |  4.97 |  5.36 |    5.75 |  5.43 |
| 8th     |    5.83 |  5.84 |  5.71 |    6.15 |  5.69 |
| 9th     |    6.15 |  5.85 |  5.98 |    6.26 |  6.31 |
| 10th    |    6.55 |  6.50 |  6.22 |    6.41 |  6.46 |
| Alpha   |    6.56 |  6.66 |  6.58 |    6.70 |  6.63 |
| Beta    |    6.94 |  6.90 |  6.92 |    7.04 |  7.00 |
| Gamma   |    7.41 |  7.28 |  7.23 |    7.37 |  7.31 |
| Delta   |    7.91 |  7.91 |  7.91 |    8.04 |  7.99 |
| Epsilon |    9.03 |  9.13 |  9.25 |    9.32 |  9.22 |
| Zeta    |    9.40 |  9.35 |  9.55 |    9.60 |  9.60 |
| Eta     |   10.13 | 10.38 | 10.05 |    9.96 | 10.25 |
| Theta   |   10.74 | 10.96 | 10.67 |   10.81 | 10.64 |
| Iota    |   11.68 | 12.27 | 11.16 |   11.66 | 11.69 |
| Kappa   |   12.25 | 13.13 | 12.05 |   12.41 | 12.10 |

Tier boundaries are placed at the midpoint between adjacent means. Progress inside the selected tier is divided into sublevels for Low, Mid, and High output and the numeric DP value.

#### Ruler selection

I chose the ruler selection to be conservative.

For lower and mid-level charts, the native rhythm profile is used to reproduce the reference low-tier pattern classification behavior.

For higher-SR charts, structural features are used to classify the chart as Jack, Speed, Stamina, Tech, Stream, or Hybrid. A skill-specific ruler is only used when the evidence is strong enough. Otherwise the General ruler is retained.

MinaCalc is also used as a conservative second opinion. It may support a skill ruler or veto a weak selection back to General, but it does not freely switch one skill ruler into another.

The internal classifier and ruler decision are diagnostic tools, so they aren't present in the normal HUD.

### MinaCalc and MSD

[MinaCalc Standalone](https://github.com/kangalio/minacalc-standalone) is included as a Git submodule and linked directly into ManiaDanOverlay as a static library.

The bridge exposes the standard MinaCalc skillsets:

- Stream
- Jumpstream
- Handstream
- Stamina
- JackSpeed
- Chordjack
- Technical

The HUD displays the strongest relevant pattern names and overall MSD. Extra Info can show the individual values in more detail.

MinaCalc receives note rows represented as four-column bitmasks and row timestamps. MinaCalc does not model LN releases directly, so LN MSD describes the chart's note-head pattern content rather than the full hold-release structure. The dedicated LN profile and LN Course estimator handle the hold-specific information separately.

Arbitrary gameplay rates are sampled from the cached MinaCalc rate table by interpolation. The low-rate DC path also supports values below the normal table range through controlled extrapolation.

### LN Course

LN-heavy charts use a dedicated LN Course path instead of the rice Reform path.

Current routing is:

```
LN ratio <= 30%       Reform
LN ratio 30% to 45%   Reform hybrid range
LN ratio > 45%        LN Course
```

LN Course uses a global 16-stage ladder:

```
1st  2nd  3rd  4th  5th
6th  7th  8th  9th  10th
Yoake  Yuugure  Yoru  Yami  Yume  Yokaze
```

The estimator combines the Sunny base value with LN-specific structural information, including hold occupancy, simultaneous holds, release density, LN duration variation, and the interaction between base difficulty and occupancy.

The LN family label is descriptive only. It does not select a different LN rating ruler.

Current player-facing LN descriptions are:

- All-round LN
- Jack / Technical LN
- Inverse / Wall LN
- Speed / Density LN

These descriptions are based on competing structural signals and are separate from the LN Course DP calculation.

### Theme and rank colors

The UI uses a dark neutral palette with a Dan-dependent accent color.

Base colors:

```
Background      #151719
Panel           #1D2022
Primary text    #E8E5DF
Muted text      #8B8B86
Rules           #343638
```

Rank colors are modeled after the osu!lazer's difficulty-color progression. The dark 8th through 10th range receives additional contrast treatment so text and graph elements remain readable on a dark overlay. Alpha and above use the high-difficulty color progression with a near-black badge background.

### Settings

Settings are stored as JSON and are created automatically on first run.

Linux:

```
$XDG_CONFIG_HOME/ManiaDanOverlay/settings.json
```

If `XDG_CONFIG_HOME` is not set:

```
~/.config/ManiaDanOverlay/settings.json
```

Windows:

```
%APPDATA%\ManiaDanOverlay\settings.json
```

Current defaults:

```json
{
  "version": 5,
  "view_mode": "hud",
  "graph_mode": "overview",
  "focus_span_seconds": 30,
  "always_on_top": true,
  "hud_content": {
    "mod_rate": true,
    "key_mode": true,
    "graph_mode": true,
    "client": false,
    "msd": true
  },
  "window": {
    "remember_position": true,
    "remember_size": true,
    "has_position": true,
    "x": 1920,
    "y": 381,
    "hud": {
      "width": 503,
      "height": 240
    },
    "extra_info": {
      "width": 900,
      "height": 600
    }
  }
}
```

The stored window position is checked against the connected monitor layout before it is restored, so a saved position should not intentionally leave the window inaccessible after a monitor configuration change.

Settings are written through a temporary file and replaced atomically. Window movement and resizing are debounced to avoid writing the settings file continuously while the user drags the window (had some issues with it soooo that's the best solution).

F3 diagnostics and F10 OBS Hide are temporary runtime states and do not overwrite the user's normal saved window size or position.

The Settings panel also includes an About section with the application version, build type, platform, active calibration revision, and a button that opens the ManiaDanOverlay GitHub repository. Opening the repository is the only part of this section that requires internet access.

### OBS behavior

The application continues to update its render loop while the window is not focused.

On Linux X11, some desktop environments unmap a truly minimized window. OBS Window Capture can stop receiving fresh frames in that state even if the application continues running.

`F10` provides OBS Hide mode for this case. The normal window position is preserved, the window remains mapped and rendering, and the overlay is moved almost entirely off-screen until F10 is pressed again (lowkey this feature is not that good and half works but it's already in so meh).

### Dependencies

#### Runtime

| Dependency                              | Purpose                                              |
| --------------------------------------- | ---------------------------------------------------- |
| osu!lazer or osu!stable                 | Game client                                          |
| Tosu                                    | Local gameplay telemetry and active beatmap access   |
| Windows system APIs or Linux X11/OpenGL | Windowing, rendering, sockets, and platform services |

#### Built into the executable

| Dependency                                                             | Purpose                                                      |
| ---------------------------------------------------------------------- | ------------------------------------------------------------ |
| [raylib](https://github.com/raysan5/raylib)                            | Native rendering, input, font loading, and window management |
| [yyjson](https://github.com/ibireme/yyjson)                            | Tosu JSON parsing and settings serialization                 |
| [MinaCalc Standalone](https://github.com/kangalio/minacalc-standalone) | MSD and skillset calculation                                 |

libcurl is not used. Tosu communication is handled by `src/net/local_http.c`, a small HTTP/1.1 client written specifically for localhost use.

The local HTTP implementation supports the response framing needed by Tosu, including normal `Content-Length` responses and chunked transfer encoding (I was able to shrink file size even lower with this >:]).

### Source layout

```
ManiaDanOverlay/
|-- assets/
|   `-- fonts/
|-- cmake/
|   |-- EmbedFonts.cmake
|   |-- StageLinuxX11Release.cmake
|   |-- StageWindowsRelease.cmake
|   `-- toolchains/
|       `-- mingw64.cmake
|-- scripts/
|   |-- build-linux-x11-release.sh
|   `-- build-windows-mingw64.sh
|-- src/
|   |-- app/
|   |-- beatmap/
|   |-- engine/
|   |-- net/
|   |-- platform/
|   |-- settings/
|   |-- tosu/
|   |-- ui/
|   `-- main.c
|-- vendor/
|   |-- minacalc/
|   |-- raylib/
|   `-- yyjson/
|-- .gitmodules
`-- CMakeLists.txt
```

### Building from source

Clone the repository with its submodules:

```bash
git clone --recurse-submodules <repository-url>
cd ManiaDanOverlay
```

If the repository was cloned without submodules:

```bash
git submodule update --init --recursive
```

#### Linux X11

The supported Linux release target is X11 only. Wayland is explicitly disabled in the release configuration because I can't be bothered to make it, only if it gets requested.

The build requires a C11/C++17 toolchain, CMake 3.20 or newer, Git, and the X11/OpenGL development packages required by raylib's bundled GLFW.

On Debian, Ubuntu, and Linux Mint, a typical development setup is:

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

Build the release:

```bash
./scripts/build-linux-x11-release.sh --clean
```

Output:

```
dist/ManiaDanOverlay
```

The release staging step verifies that `dist/` contains only the final executable and checks that libcurl, Wayland, external GLFW, raylib, yyjson, and MinaCalc have not appeared as accidental dynamic dependencies.

Normal Linux system libraries such as glibc, X11, and OpenGL remain dynamic system dependencies because... it's obvious.

#### Windows x64

The current release pipeline cross-compiles the Windows executable from Linux with MinGW-w64.

Install MinGW-w64 on Linux:

```bash
sudo apt update
sudo apt install mingw-w64
```

Build:

```bash
./scripts/build-windows-mingw64.sh --clean
```

Output:

```
dist-windows/ManiaDanOverlay.exe
```

The Windows release uses the GUI subsystem, embeds DPI-aware application metadata, and statically links the MinGW support runtimes used by the project. The release verifier rejects accidental dependencies on libcurl, libgcc, libstdc++, libwinpthread, raylib, GLFW, yyjson, or MinaCalc DLLs.

Windows system DLLs such as `KERNEL32.dll`, `USER32.dll`, `GDI32.dll`, `OPENGL32.dll`, and `WS2_32.dll` are normal operating-system dependencies and are not distributed with the app.

### Single-file release model

The intended user-facing downloads are:

```
Linux X11:  ManiaDanOverlay
Windows:    ManiaDanOverlay.exe
```

No runtime `assets` directory, font files, MinaCalc helper executable, Python environment, Wine environment, libcurl DLL, or project source tree is required beside the release executable.

The settings JSON created in the user's configuration directory is normal per-user application data and is not part of the distributed program package.

### Reference projects and calibration sources

ManiaDanOverlay is a native rewrite and extension of ideas and calibration behavior from existing osu mania overlays and analysis projects.

Primary references include:

- [Dan-Overlay](https://github.com/acarranzao1a-png/Dan-Overlay), used as a reference for Reform calibration, rhythm-profile behavior, LN Course behavior, and several compatibility decisions.
- [Daniel](https://github.com/TheBagelOfMan/Daniel) by TheBagelOfMan, used as a reference for the original overlay presentation and Dan calculation behavior.
- [MinaCalc Standalone](https://github.com/kangalio/minacalc-standalone), used for MSD and skillset analysis.
- [Tosu](https://github.com/tosuapp/tosu), used as the local telemetry source.
- [raylib](https://github.com/raysan5/raylib), used for the native desktop UI.
- [yyjson](https://github.com/ibireme/yyjson), used for JSON handling.

The native implementation is not a Python wrapper around the original overlay. Parsing, feature extraction, Sunny calculation, Reform evaluation, LN Course evaluation, density rendering, settings, and Tosu networking are implemented inside the native application. MinaCalc remains C++ internally and is accessed through a C-facing bridge.

### Known limitations

- Only 4K osu mania is supported.
- Tosu must be running for live gameplay data.
- Linux support is X11 only at this time.
- Wayland is out of scope for the current release.
- LN profile family labels are descriptive and may be adjusted independently of LN Course rating calibration.
- MinaCalc does not model LN releases, so LN MSD should be interpreted as note-head pattern difficulty rather than a complete LN difficulty measurement.
- Reform and LN Course results are estimates produced by the project's calibration models. They are not official osu rankings or anything.

### Development policy

The normal player-facing UI is kept separate from diagnostics. Internal classifiers, Sunny components, ruler decisions, and feature values are available through F3 when debugging is needed, while the normal HUD is intentionally limited to information a player is likely to care about during play.

The current release targets are intentionally limited because the project has already gone through a substantial development cycle and the priority now is keeping the main build stable and maintainable:

```
Windows x64
Linux X11 x64
4K osu!mania
```

Additional platforms, key modes, and calibration changes are welcome as long as they fit the existing architecture, remain cleanly separated where necessary, and do not regress the current Windows/Linux X11 4K experience.
