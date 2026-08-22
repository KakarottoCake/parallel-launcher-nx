# ParaLLEl Launcher NX

**ParaLLEl Launcher NX** is a native Nintendo Switch port and frontend inspired by [ParaLLEl Launcher](https://gitlab.com/parallel-launcher/parallel-launcher) and [ParaLLEl-Lite](https://github.com/KakarottoCake/ParaLLEl-Lite). It provides a full-featured ROM hack manager and launcher designed specifically to play *Super Mario 64* ROM hacks directly from [romhacking.com](https://romhacking.com) on the Switch.

---

## Features

- **Horizon OS-Style UI**: Built using the [Borealis](https://github.com/XITRIX/borealis) UI framework with smooth 60 FPS navigation, light/dark themes, and controller-first interaction.
- **Romhacking.com Integration**:
  - **Catalog Browser**: Search, filter by category (Featured, Trending, Top Rated, Kaizo, SM64 Non-Stop, Legacy OGRE), and view metadata (stars, ratings, difficulty, screenshots).
  - **User Account & Playlists**: Authenticate with romhacking.com to view star power stats and sync favorite hack playlists.
- **On-Device BPS & IPS Patching Engine**:
  - Automatically verifies base *Super Mario 64 (USA)* `z64` ROM integrity (SHA-1: `8a20a45e36a5c71a33350e937b7702046dd9ce82`).
  - Converts between `n64`, `v64`, and `z64` byte orders on the fly.
  - Native BPS and IPS patch applicator with streaming verification.
- **Multi-Engine Graphics Support & Auto-Selection**:
  - **ParaLLEl-RDP**: Pixel and cycle-accurate Vulkan N64 RDP renderer (via [nxvk](https://github.com/PalindromicBreadLoaf/nxvk) / NVK).
  - **GLideN64**: Fast OpenGL high-compatibility renderer.
  - **OGRE**: Legacy HLE plugin for classic SM64-Editor based hacks.
  - Automatically selects the author-recommended graphics plugin based on hack metadata.
- **Comprehensive Controller Support**:
  - Nintendo Switch Pro Controller
  - Joy-Con (Handheld and Dual Grip modes)
  - GameCube Controller (USB 4-Port Adapter)
  - Configurable analog stick deadzones and in-game quick menu mapping.

---

## Project Structure

```
├── CMakeLists.txt              # Root Switch CMake build file
├── build.sh                    # Build script targeting Switch NRO (Ninja + devkitA64)
├── resources/                  # RomFS resources (fonts, icons, themes, XML layouts)
│   ├── font/                   # Switch system fonts
│   ├── icon/                   # App icons
│   └── xml/                    # Borealis XML activity and tab definitions
├── src/
│   ├── core/                   # Core logic (checksum, ROM handling, BPS/IPS patcher, catalog, settings)
│   ├── emu/                    # Emulator runner & controller input abstraction
│   ├── ui/                     # Borealis UI views, activities, tabs (Library, Browse, Account, Settings)
│   ├── types.hpp               # Core enums and constants
│   └── main.cpp                # App entry point
├── player/                     # Standalone player process wrapper
├── cores/                      # Core build scripts and libnx/vulkan patches
│   ├── build_cores.sh          # Fetches & builds libretro cores with Switch patches
│   ├── build_nxvk.sh           # Builds nxvk / Mesa Vulkan driver for Switch
│   ├── patches/                # Custom patches for libnx compatibility
│   └── toolchain/              # Horizon OS toolchain patches & Dockerfile
└── tests/                      # Host-side C++ unit test suite
    ├── run_tests.sh            # Runs 42+ unit test checks on host
    └── test_core.cpp           # Test suite for SHA-1, CRC-32, BPS/IPS patcher, byte orders
```

---

## Building and Running

### Prerequisites
- [devkitPro](https://devkitpro.org/) with `devkitA64` and `libnx`
- `cmake` (>= 3.13) and `ninja`
- Nintendo Switch running Atmosphere (or [Ryujinx](https://ryujinx.org/) for local testing)

### Build the Switch NRO
```bash
./build.sh
```
The compiled homebrew application will be at `build_switch/parallel_launcher.nro`.

### Run Unit Tests (Host Machine)
```bash
./tests/run_tests.sh
```
Or optionally test against a real Super Mario 64 base ROM:
```bash
./tests/run_tests.sh /path/to/Super\ Mario\ 64\ \(USA\).z64
```

### Local Emulation Testing
You can launch the `.nro` in Ryujinx directly:
```bash
/Applications/Ryujinx.app/Contents/MacOS/Ryujinx build_switch/parallel_launcher.nro
```

---

## License

Inspired by the original ParaLLEl Launcher and ParaLLEl-Lite projects.
Borealis UI is licensed under the Apache 2.0 License.
