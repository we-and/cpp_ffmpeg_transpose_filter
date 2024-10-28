# FFmpeg Transpose Filter in C++
This project implements a simple transpose filter for video files using FFmpeg in C++. The filter rotates video frames 90 degrees clockwise and outputs a transposed video file. It demonstrates how to use FFmpeg's libraries (libavcodec, libavformat, libswscale, and libavutil) to read, process, and write video frames programmatically in C++.

### Features
Transposes video frames by rotating 90 degrees clockwise.
Reads from an input video file and writes to an output file.
Demonstrates the use of FFmpeg libraries in a C++ environment.

### Requirements
FFmpeg libraries: libavcodec, libavformat, libswscale, libavutil.
C++ Compiler: A C++11-compatible compiler (e.g., g++).
macOS (other Unix-like systems should work with slight modifications).

### Installation
Step 1: Install FFmpeg Libraries
If you haven’t installed FFmpeg libraries yet, you can use Homebrew to install them on macOS:

```
brew install ffmpeg
```
This will install the FFmpeg libraries including libavcodec, libavformat, libswscale, and libavutil.

Step 2: Clone the Repository
Clone this repository to your local machine:

Step 3: Compile the Program
Use the following command to compile the code, specifying the paths to the FFmpeg libraries (Homebrew path shown here):
```
g++ main.cpp -I/usr/local/opt/ffmpeg/include -L/usr/local/opt/ffmpeg/lib -lavformat -lavcodec -lavutil -lswscale -o transpose
```
If you’re on an Apple Silicon Mac, your paths might be under /opt/homebrew/ instead of /usr/local/.

### Usage
Once compiled, you can use the program to transpose a video file. Here’s how:
```
./transpose input.mp4 output.mp4
```
This command reads input.mp4, rotates each frame 90 degrees clockwise, and saves the result to output.mp4.

### Code Overview
main.cpp: The main source file containing the transpose filter logic.
VideoTranspose: A class where the 'process' function rotates each video frame by 90 degrees clockwise.
