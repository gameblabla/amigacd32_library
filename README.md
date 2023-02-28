# Amiga CD32 Homebrew small library (cd32.c)

This comes with a small library called cd32.c that works with the recent GCC-based AmigaOS m68k cross compiler.
The goal is to be self contained and be relatively easy to use.

You will need a file called "CD32.TM" otherwise the building process will fail.
You also need python2 to run the provided script.

# Currently supported

- Displaying to the 8bpp framebuffer (a buffer is made available for this purpose)
- Playing PCM tracks on all 4 channels (Either ptplayer or NDK)
- MOD support through ptplayer (requires building with VASM, if you don't like that, there's another audio codepath without it)
- CDDA tracks
- Really basic CDXL support (comes from Amiga Developer CD)
- Basic input support (Actually provided by the AmigaOS SDK as they are quite basic enough)

# Not (currently) supported
- Hardware sprites (Something i would like to have partial support for at least)
- HAM color modes
- Amiga Copper (I'm not sure i'll ever come to this)

Check out DOCUMENTATION.md if you want to know more about it.

# Many thanks to

- The amiga people for keeping the scene alive
- ChatGPT for assisting me with trimming down some of the functions
- TheFakeMontyOnTheRun for his audio code. (https://gist.github.com/TheFakeMontyOnTheRun/5ac2ea1f0d491714f945ba5e6b7bec2f)
