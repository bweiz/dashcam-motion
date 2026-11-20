# Dashcam motion detection

I built a three-camera security dashcam for my senior design project. This repo 
is where I'm rebuilding the motion detector in C++ and 
testing it on recorded footage before putting it back on hardware.

GStreamer sends each frame down two branches. One writes an
H.264 recording.  The other gives my C++ code a grayscale
frame to capture with the previous one.
