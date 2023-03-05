# TODO list

# Implement faster functions, particulary for OCS/ECS
Blitter functions are infamous for being awful on Kickstart 1.3.

Hoping to use this as a reference but he did not implement
both bobs and tilemap at the same time :
https://github.com/weiju/amiga_hardware_in_c

#  HAM6/HAM8 modes
So far, only supported by the semi broken CDXL player.

#  Improve CDXL player or converter
It kind of sucks rn... Hoping to find a better solution.

#  Fast PRNG generator

This might be useful in some cases.

# Support compression for images

LZ4W and Aplib are good for 68000.

For 68020, there's also lz4-68k :
https://github.com/arnaud-carre/lz4-68k

lz4_normal.asm version is said to be suitable for 68020, ideal for AGA/CD32.
He also has a 68000 version as well for CPUs without instruction cache.

Compression is especially useful on CDTV/CD32 as the drives are very slow
but it can also be very useful on floppy disks as well.

# Provide fast Memcpy/memset

On the Atari Falcon, they had some fast ones that i did use for Evil Australians.
These will also work for the Amiga CD32/AGA.

On OCS/ECS+, SGDK also provides great alternatives on the Megadrive.

# Bypass the OS eventually.

- For CDDA (CDTV)
https://eab.abime.net/showthread.php?t=106131&highlight=cdtv.device
 
- CD32
Not sure.

- Loading files off the CD without OS
I believe no one knows how to. 
We can take over the system after the files are loaded into memory however.

