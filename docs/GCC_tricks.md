# Amiga APIs

A bunch of generic functions like LoadFile_tobuffer and LoadPalette_fromfile are provided to you for simple stuff.
The NDK provides its own set of native functions like Open/Seek/Read/Close that are pretty similar to their POSIX equivalents
but not directly compatible (much like TOS).
Use them if possible to avoid a dependency on the c library and having to link to ixemul as a result.

# Multiply and Divisions

Multiply and Divisions should be avoided whenever possible as they do have a higher performance penalty and are more expensive than bitshifting.

y / 2
vs
y << 1

And so on... 

ChatGPT is also good at giving you a bitshift equivalent to your Mul/DIV operations if you just need quick help for constants.
Note that if the code in question is only being run once, then you may not worry too much about it.
This advice is also true for other processors like the 6502, 6809 etc...

# Optimization flags

Most people recommend sticking with -Os on the Motorola 68000+ cpus.
Under some circumstances -Ofast may be ideal too so it mostly a matter of strategy.
(You can also use -Os -ffast-math and get close to that)

Here are some other essential ones:
-mxgot is perhaps the most important and the one with the most performance impact, you want to have this enabled by default.

-fipa-pta has been noticed to have a small performance improvement on MIPS32 and ARM.

-fno-exceptions & -fno-rtti for C++ projects. If the compiler complains, you may have to remove the exceptions.

-fno-common is not turned on by default in GCC6.

-noixemul will prevent the linker from linking to ixemul. Note that if your game no longer boots, it might need it especially if your game relies on libc.

-flto for LTO. Make sure to pass your CFLAGS opts to the linker too with GCC.

Do not use
-malign-int is useful as the M68k will be slower on unaligned access but this may cause breakage.

However, it causes the palette to break. (I will fix that hopefully)

# Switch statement

For a switch with an unreachable default, you can use
```c
default:
__builtin_unreachable(); 
```
instead of
```c
default: break
```

...to avoid GCC generating extras cmp & bhi.

Note that doing so will may undefined behaviour so be warned !
VBCC also does not support this.

