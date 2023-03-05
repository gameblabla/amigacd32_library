
# Framebuffer format

- AGA / CD32

Thanks to the Akiko/system functions, this is pretty much like VGA.

To be more precise, the resolution is 320x256 at 8bpp.
If you wish a resolution that is lower, 320x200 and 320x224 are also available.
You can also set a lower internal framebuffer and only partially update that.

You can simply export the image with GIMP as a raw format by reducing the numbers of colors to 256
and GIMP will export both the RAW file and the PAL file as well.
The palette expects R, G, B and should be 768 bytes wide.

This is pretty much exposed like a VGA framebuffer would be.
Please note that there is, at this moment, no proper CPU fallback for C2P,
and the results will be quite slow than usual on those computers.

Blitter functions are also supported and interleaved frames must be used.
This should be decent enough for simple static games, at least on a stock AGA Amiga anyway.

Color depth is forced to 8bpp so if that's a problem, please use the OCS version
(but with the newer NDK functions turned on, COMPILE time only).

Please use the blitter functions whenever possible.

- OCS/ECS

There is no Akiko hardware on OCS/ECS, requiring the use of the Blitter.
A custom solution will be used in the future but for now, it relies on the OS call "BltBitMap".

Unfortunately, BltBitMap is only decently good on the CD32 and it's Kickstart 3.1.
On the CDTV however, they use Kickstart 1.3 and some models have 2.3+ expanded ROMs.
Still, it's not good enough for our purpose :/...

Earlier kickstart also only supports non-interleaved frames unlike the newer OS.

# Sound format

The expected sound format for the RAW samples is Signed 8-bits RAW MONO.
The frequency can be configured at runtime (up to 44100) but i would recommend something like 11025 or 22050.
You can eithse use SoX or Audacity for this purpose.
For music, you can either playback MOD files (4 channels) or with CDDA, play CD Stereo files too. (as CDDA tracks, bin/cue, mds/mdf etc...)

# CDXL

There's a CDXL player in there but it's extremely finicky...
This was directly taken from the Amiga Developer CD 2.1 but it would appear they had issues with LoadRGB4 being slow,
resulting in visible palette switching and making it look really ugly as a result.
You also need to tweak the playback speed and this varies between on your settings with your CDXL so it's going to be a bit of trial and error.

For best results, stick with a low framerate and only stick with one palette accross the whole thing.
Do not use ocs5, as it will look very poor : Either use HAM6 or HAM8 in compatible mode.

# Example program

This shows off what the small CD32.c library supports :
- PCM sound samples (Either via NDK or ptplayer. Note that using ptplayer requires building with VASM, and VASM has a non commercial license)
- MOD playback (Enable with MOD_PLAYER. Note that you can still use CDDA tracks at the same time but the example code will disable them)
- CDDA tracks (either via cdtv.service OR cd.service, the former will work on a CDTV and A570 and the latter on CD32)
- CDXL playback (Enable with VIDEO_PLAYBACK)
- Loading an image from CD (as well as its palette file) and displaying it onscreen

Feel free to modify it according to your needs.
