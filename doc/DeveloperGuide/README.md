<img src="images/AmeNoteHoriz.png"
     alt="AmeNote Logo"
     style="center; margin-right: 100px;" />

# ProtoZOA(TM) Developer Guide

**This document is in development and in need of a lot of inputs from other contributors as well. When making contributions to the ProtoZOA project, please consider how the Developer Guide can benefit with more details regarding contribution.**

## Developing on Raspberry Pi Pico

This project targets **Pico SDK 2.3.0** with **ARM GNU Toolchain 15_2_Rel1**. There are two supported ways to set up your development environment:

### Option A: VS Code (recommended for most contributors)

1. Install [VS Code](https://code.visualstudio.com/) and the official [Raspberry Pi Pico VS Code extension](https://marketplace.visualstudio.com/items?itemName=raspberry-pi.raspberry-pi-pico).
2. Open this repository's folder in VS Code. Use the extension's "Import Project" / SDK version picker to install SDK 2.3.0 and toolchain 15_2_Rel1 if you don't already have them (it installs them under `~/.pico-sdk`, not system-wide).
3. Use the extension's Compile / Run / Debug commands as normal. The top-level `CMakeLists.txt` has a header block (`# == DO NOT EDIT ... ==`) that the extension reads to auto-locate the right SDK/toolchain/picotool versions -- leave that block alone.

### Option B: CLI or another IDE (CLion, etc.)

1. Install the [Pico SDK](https://github.com/raspberrypi/pico-sdk) (tag `2.3.0`) and [ARM GNU Toolchain](https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads) `15.2.Rel1` yourself, anywhere on disk.
2. Set `PICO_SDK_PATH` to your SDK checkout, and `PICO_TOOLCHAIN_PATH` to your toolchain install (unless its `bin/` is already on `PATH`).
3. **Install CMake 4.x.** This is a hard requirement, not just a suggestion: SDK 2.3.0's linker-script system uses a `target_link_options(... "LINKER:-L...")` mechanism that CMake 3.x (including current Homebrew/apt-packaged CMake as of this writing) fails to translate correctly for `copy_to_ram` binaries -- `ProtoZOA_Main` and `picoprobe` will fail to link with `cannot open linker script file section_platform_end.incl` on CMake 3.x. If you don't want to touch your system CMake, download the [CMake 4.3.4 release](https://github.com/Kitware/CMake/releases/tag/v4.3.4) tarball and point directly at that binary.
4. From the repo root:
   ```
   cmake --preset release
   cmake --build --preset release
   ```
   See [CMakePresets.json](../../CMakePresets.json) for the available presets.

Raspberry Pi's own [Pico documentation](https://www.raspberrypi.com/documentation/microcontrollers/raspberry-pi-pico.html) and [C/C++ SDK guide](https://datasheets.raspberrypi.com/pico/raspberry-pi-pico-c-sdk.pdf) are good general background reading, independent of the ProtoZOA-specific steps above.

## About the Code
The ProtoZOA code is currently available in a private MIDI Association repository. Eventually the code will be made public as indicated in the license and contribution agreement.

The repository is located at: [https://github.com/midi2-dev/Amenote_Protozoa](https://github.com/midi2-dev/Amenote_Protozoa).

To get the ProtoZOA code including all documentation, you need to clone the Amenote_Protozoa to your development machine. It is assumed that you have already installed and configured your Raspberry Pi Pico development environment and SDK. If not, please refer to the documentation provided by Raspberry Pi for the Pico [here](https://www.raspberrypi.com/documentation/microcontrollers/raspberry-pi-pico.html). In particular, check out the [Getting Started Guide](https://datasheets.raspberrypi.com/pico/getting-started-with-pico.pdf).

The ProtoZOA code base is C / C++. There are many resources available online if your are not familiar with the C / C++ syntax. The Pico SDK is the C/C++ SDK which you can read more about [here](https://datasheets.raspberrypi.com/pico/raspberry-pi-pico-c-sdk.pdf).

### Getting the Code
The best way to get the code is to clone the repository to your local development machine. With git installed, you can go to your Pico directory (same place as pico-sdk directory) and exeucte the following command:

> git clone git@github.com:midi2-dev/AmeNote_Protozoa.git --recursive

**Note:** this project's submodules (`lib/FreeRTOS-Kernel`, `lib/ni-midi2`, `lib/AM_MIDI2.0Lib`, `lib/tusb_ump`, `lib/CMSIS_5`) are all fetched over SSH (`git@github.com:...`), so you need an SSH key [added to your GitHub account](https://docs.github.com/en/authentication/connecting-to-github-with-ssh) before cloning -- an HTTPS clone of the main repo will still fail to fetch submodules without one.

The --recursive command will ensure all submodules are also fetched into your local repository. If you did not fetch repository with the recursive command, you can change directory into your local repository and execute the following commands:

> git submodule init

> git submodule update --recursive

`lib/CMSIS_5` is only used for the CMSIS-DAP debug-probe firmware (`ProtoZOA_Main` and `ProtoZOA_PicoProbe` pull in `CMSIS/DAP/Firmware/{Source,Include}` and `CMSIS/Core/Include` directly) -- it's pinned to upstream tag `5.9.0`, not the full CMSIS_5 feature set.

### Contributing

Please refer to the contribution guidelines published by [MIDI2.dev](https://githuyb.com/midi2-dev).

#### ProtoZOA
For contributions to the ProtoZOA code base, please create yourself a branch or switch to an existing branch (please check with branch owner). Do not attempt to check any work into the main branch, these will not be accepted in pull requests.

Work on your contribution, making sure to check regularly with the main branch to sync to your branch to avoid issues in future. After completing your effort and testing, submit a pull request. Be sure to add AmeNote-Michael (Michael Loh) as a reviewer plus anyone else you feel can add comment to review your pull request. Michael Loh may add additional reviewers.

The pull request may be approved, commented for further suggested development or recommended changes indicated. If approved, the code will be merged into main creating the next release.

## Issues
If you discover an issue with existing code bases, be sure to review current Issues. Please add more details to any existing Issue if you discover any. If a new Issue, please be sure to create new Issue with enough detail to aid all in recreation to help solve the issue.

**Note that Issue reporting is not to request feature or capability. For this, please see below.**

## Suggested Improvements / Additions

Please use the Issues feature of Git to submit any suggested improvements or additions. Please review any current open issues to see if there is a similar or same issue already posted and either add or agree to that thread.

##### AmeNote, AmeNote Logo and ProtoZOA are trademarks of AmeNote, Inc. (2022).
