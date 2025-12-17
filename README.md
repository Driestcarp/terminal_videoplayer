# Terminal Video Player

A video player that renders video directly in the terminal.

The player is known to work with **.gif** and **.mp4** files.

## Features
- Play videos directly in the terminal
- Adjustable FPS (including uncapped mode)
- Loop playback
- Playback statistics
- Custom terminal size


## Requirements

This project depends on **OpenCV**.

You must have OpenCV installed before compiling, since the project uses:

#include <opencv2/opencv.hpp>

### Linux (Debian / Ubuntu)
sudo apt update
sudo apt install libopencv-dev

### Arch Linux
sudo pacman -S opencv

### Fedora
sudo dnf install opencv-devel

### macOS (Homebrew)
brew install opencv

## Build

### Compile
make term_vid

## Usage

### Run
./termvid FILENAME [flags]

## Flags

| Flag                | Description |
| ------------------  | --------------------------------------------- |
| -loop               | Loops the video |
| -fps <value>        | Sets max FPS (-1 for uncapped) |
| -stats              | Shows playback statistics |
| -size <cols> <rows> | Sets terminal window size |

## Supported Formats
The video player is confirmed to work with:
- .gif
- .mp4

Other formats may work depending on your system and build configuration.

## Example
./termvid demo.mp4 -fps 30 -loop -stats

## Requirements
- Unix-like operating system
- Make
- C compiler (GCC or Clang)
- A terminal with sufficient color support

## Notes
- Performance depends heavily on terminal speed and size
- Best results are achieved with smaller resolutions

## License
MIT
