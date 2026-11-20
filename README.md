# Dashcam motion detection

I built a three-camera security dashcam for my senior design project. This repo 
is where I'm rebuilding the motion detector in C++ and 
testing it on recorded footage before putting it back on hardware.

GStreamer sends each frame down two branches. One writes an
H.264 recording.  The other gives my C++ code a grayscale
frame to capture with the previous one.

## Build and run

Requires CMake, a C++ compiler, and the GStreamer core, app, and video development
packages. The recording pipeline uses 'x264enc', 'h264parse', and 'mp4mux'.

```bash
curl -fL -o parking-lot.mp4 \
  https://raw.githubusercontent.com/intel-iot-devkit/sample-videos/master/person-bicycle-car-detection.mp4
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build --parallel
./build/dashcam
```

Run from the project root. The program writes `recording.mp4` and a raw
changed-pixel mask, `changes.gray`

## What I measured

The test clip has 647 frames at 12 fps. I added a 120 ms delay to the analysis
branch, longer than the 83 ms between frames. With a blocking queue, processing
all 647 frames took 79.2 seconds. With a three-frame queue set to 
`leaky=downstream`, the run finished in 54.4 seconds. The detector processed
442 frames, and I verified with `ffprobe` that the recording still contained
all 647.

[Test footage from Intel IoT DevKit]
(https://github.com/intel-iot-devkit/sample-videos)
