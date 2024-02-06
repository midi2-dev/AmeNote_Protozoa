<img src="images/AmeNoteHoriz.png"
     alt="AmeNote Logo"
     style="center; margin-right: 100px;" />

# ProtoZOA(TM) Developer Guide

**This document is in development and in need of a lot of inputs from other contributors as well. When making contributions to the ProtoZOA project, please consider how the Developer Guide can benefit with more details regarding contribution.**

## Developing on Raspberry Pi Pico

There are several resources in regards to developing on the Raspberry Pi Pico. To setup your development environment, see the documentation supplied by Raspberry for the Pico [here](https://www.raspberrypi.com/documentation/microcontrollers/raspberry-pi-pico.html).

A really good resource is a series of documents and videos through [Digikey](https://www.digikey.ca/en/maker/projects/raspberry-pi-pico-and-rp2040-cc-part-1-blink-and-vs-code/7102fb8bca95452e9df6150f39ae8422).

There is also a good video from [Gary Explains](https://www.youtube.com/watch?v=NCaL6tXAF0c)

Please use these resources to become familiar with development on Raspberry Pico plus also in configuring your development environment.

## About the Code
The ProtoZOA code is currently available in a private MIDI Association repository. Eventually the code will be made public as indicated in the license and contribution agreement.

The repository is located at: [https://github.com/midi2-dev/Amenote_Protozoa](https://github.com/midi2-dev/Amenote_Protozoa).

To get the ProtoZOA code including all documentation, you need to clone the Amenote_Protozoa to your development machine. It is assumed that you have already installed and configured your Raspberry Pi Pico development environment and SDK. If not, please refer to the documentation provided by Raspberry Pi for the Pico [here](https://www.raspberrypi.com/documentation/microcontrollers/raspberry-pi-pico.html). In particular, check out the [Getting Started Guide](https://datasheets.raspberrypi.com/pico/getting-started-with-pico.pdf).

The ProtoZOA code base is C / C++. There are many resources available online if your are not familiar with the C / C++ syntax. The Pico SDK is the C/C++ SDK which you can read more about [here](https://datasheets.raspberrypi.com/pico/raspberry-pi-pico-c-sdk.pdf).

### Getting the Code
The best way to get the code is to clone the repository to your local development machine. With git installed, you can go to your Pico directory (same place as pico-sdk directory) and exeucte the following command:

> git clone https://github.com/midi2-dev/Amenote_Protozoa.git --recursive

The --recursive command will ensure all submodules are also fetched into your local repository. If you did not fetch repository with the recursive command, you can change directory into your local repository and execute the following commands:

> git submodule init

> git submodule update --recursive

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
