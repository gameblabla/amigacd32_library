# Amiga CD32 Homebrew small library (cd32.c)

This comes with a small library called cd32.c that works with the recent GCC-based AmigaOS m68k cross compiler.
The goal is to be self contained and be relatively easy to use.

You will need a file called "CD32.TM" otherwise the building process will fail.
You also need python2 to run the provided script.

# Currently supported

- Displaying to the 8bpp framebuffer (a buffer is made available for this purpose)
- Playing PCM tracks on all 4 channels
- CDDA tracks
- Basic input support (Actually provided by the AmigaOS SDK as they are quite basic enough)

The idea is to have enough working to port simple framebuffer based games like Wolfenstein 3D.

# Not (currently) supported
- Hardware sprites (Something i would like to have partial support for at least)
- HAM color modes
- Amiga Copper (I'm not sure i'll ever come to this)
- MOD/XM support (I need to integrate this :https://github.com/keirf/amiga-stuff/tree/master/testkit)
- CDTV support

Beyond that, the fact it uses the system libraries means it's not as optimal as it can be speed wise.

# Framebuffer format

Thanks to the Akiko/system functions, this is pretty much like VGA.
To be more precise, the resolution is 320x256 at 8bpp.
You can simply export the image with GIMP as a raw format by reducing the numbers of colors to 256
and GIMP will export both the RAW file and the PAL file as well.

The palette expects R, G, B and should be 768 bytes wide.

# Sound format

The expected sound format for the RAW samples is
Signed 8-bits RAW MONO.

The frequency can be configured at runtime but i would recommend something like 11025 or 22050.

# CDXL

There's a CDXL player in there but it's extremely finicky and doesn't work properly anyway...
It seems to work properly only with HAM and low resolution videos.
If you just need a video for an intro, i would recommend an external video player program instead.

# Example program

This shows off what the small CD32.c library supports :
- PCM sound samples
- CDDA tracks
- Loading an image from CD (as well as its palette file) and displaying it onscreen

Feel free to modify it according to your needs.

# Many thanks to

- The amiga people for keeping the scene alive
- ChatGPT for assisting me with trimming down some of the functions
- TheFakeMontyOnTheRun for his audio code. (https://gist.github.com/TheFakeMontyOnTheRun/5ac2ea1f0d491714f945ba5e6b7bec2f)
