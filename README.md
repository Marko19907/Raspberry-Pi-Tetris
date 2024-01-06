# Raspberry Pi Tetris

This repository contains the code for the Tetris game that was made for the Raspberry Pi and the Sense HAT display.
The game was made as a part of the Low-Level Programming course TDT4258 at NTNU.

This is the 3rd and final lab in the course. The first two labs can be found in the [Low-Level Programming labs](https://github.com/Marko19907/Low-level-programming-labs) repository.

## Requirements

* [x] Build a simplified version of Tetris for the Raspberry Pi 4 and the Sense HAT display using C.
* [x] The state of the game must be displayed on both the Sense HAT display and the console.
* [x] The game must be controlled using the joystick on the Sense HAT or the keyboard.
* [x] Controls:
  * [x] Arrow keys to move the block left and right.
  * [x] Down arrow key to move the block down.
  * [x] Pressing `Enter` or center pressing the joystick should exit the game.
* [x] You are not allowed to use any external libraries apart from what is available in C and Linux.
* [x] Display the entire game state on the 8x8 Sense HAT display.
  * [x] You are not allowed to display anything else other than the tiles, so no score, level, etc.
* [x] One must memory map the Sense HAT display.
  * [x] You cannot assume that the display or the joystick always has the same path in Linux.
  * [x] Output an error message if you cannot find the display (`RPi-Sense FB`) or the joystick (`Raspberry Pi Sense HAT Joystick`).
* [x] The tile must move continuously downwards even if the player does not press any buttons.
* [x] Holding a button down should move the tile continuously in that direction.
* [x] Add colors to the tiles and persist the colors for the entire duration of the game.
  * [x] The colors must be distinguishable from each other.

## Running the game

Connect the Sense HAT to the Raspberry Pi and boot up the Pi with the Sense HAT attached in Raspbian.
The OS should include the necessary libraries to run the game, so no additional setup should be required.

There is a make file included in the repository, so the game can be compiled by running `make` in the root directory of the repository.

## Video

Short video of the game running on the Raspberry Pi Sense HAT display:

https://github.com/Marko19907/Raspberry-Pi-Tetris/assets/22809973/87c48bbf-6889-4846-b0d0-d6d87cdecf82

