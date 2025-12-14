# WS2812b Christmas Lights

> Turns a string of WS2812b addressable LEDs into a string of Chrismas lights. 

![Static Badge](https://img.shields.io/badge/GNU_C-11.4.0-purple?style=flat&logo=gnu&logoColor=white&logoSize=auto)
![Static Badge](https://img.shields.io/badge/CMake-3.18-green?style=flat&logo=cmake&logoColor=orange&logoSize=auto)
![Static Badge](https://img.shields.io/badge/PicoSDK-2.2.0-orange?style=flat)
![Static Badge](https://img.shields.io/badge/VSCode-1.107%2B-blue?style=flat)

## Introduction

This project as inspired by the Pico SDK PIO project named **ws2812** which addresses a string of LEDs and demostrated a small number of pattern variations. This project built upon that example and provides 6 different modes of LED behaviour, indended for use as a Christmas decoration.

## Description

A string of WS2812b "addressable" LEDs are controlled by sending one 24bit word for each LED in the string. 

The term "addressable" implies that you can write data to a specific LED but this is not accurate. If the string consist of 10 LEDs you must send a sequence of 10 x 24bits to the string so that each LED is addressed.

Originally put together as a fun way to demonstrate the use of addressable LED strings, the prototype now decorates my Chrismas Tree.

The project relies upon a PIO program which sends a bit sequence to the pin connected to the addressable LED string.

# Addressable LED types

This project was build using a string of WS2812b LEDs, which use RGB data. Other types use RGBW, which requried 32 bits of data. The software should be changed to match the LED type and the number of LEDs in the string. I will release an update of this software which allows selection between various RGB and RGBW types at a later date.

# Installation

Ensure that you have VsCode installed with the extensions listed below.

Then clone this repository, ensure the selected board type matches your physical board. Then run a clean configuration in cmake, followed by a build. Once the build is complete write the binary to your pico.

## Development setup

For this project I used VsCode on any *nix environment (including WSL2 on Windows). The extensions requried are as follows:

| Name| Manufactuer | Description |
| ----------- | ----------- | ----------- |
| CMake Tools | Microsoft | Extended CMake support in Visual Studio Code |
| C/C++ Extension Pack| Microsoft | Popular extensions for C++ development in Visual Studio Code |
| CMake (3.18+) | Kitware | Open source toolkit for building, testing and packaging software |
| GCC (11.4.0+) | GNU Software Foundataion | C & C++ compiler |
| Raspberry Pi Pico | Raspberry Pi | Raspberry Pi Pico C SDK (2.2.0+) |

## Release History

* 0.1.0.
    * The first release.

## Meta

Distributed under the MIT license. See [LICENCE](LICENCE) for more information.

[https://github.com/markdlehane](https://github.com/markdlehane/pico-ws2812b)

