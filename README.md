# L-Packer v0.99

L-Packer is an executable compressor designed for 64K-style demos. Since I couldn’t choose between my two favorite platforms, L-Packer works on both **Atari** and **Amiga** systems!

## The Idea

If you’re targeting a 4K demo, nothing beats Shrinkler (Amiga) or STrinkler (Atari).

However, for 40K or 64K demos, Shrinkler’s decompression speed becomes a problem — it can take up to 30 seconds before your demo even starts. For that size range, Atari users often rely on UPX, while Amiga users use Cranker. Unfortunately, both offer relatively poor compression ratios.

So there was a gap to fill — something in between: a packer with good decompression speed without sacrificing too much compression efficiency.
That’s where the idea for L-Packer came from!

## Key Features

* Balances good compression ratio with decent decompression speed
* Automatically try up to 3 compression algorithms on your exe (deflate, zx0, LZ4)
* Chooses the best algorithm for your target size (selecting the fastest depacker when possible)
* Supports both Atari and Amiga executables with a single tool
* Uses multi-threading to compress hunks with several algorithms in parallel for maximum speed
* Ability to pad final executable with random bytes
* also supports raw -data mode

## Use Case

If you can accept about 5% less compression than Shrinkler, L-Packer is perfect for you!
It consistently beats UPX and Cranker in compression ratio, and can decompress a 64K demo in about 3–5 seconds.

![image info](./pics/sizes.png)

## Usage

````
L-Packer v0.99(beta) by Leonard/Oxygene
Atari & Amiga executable cruncher

Usage:
        L-Packer <src> <dst> [-options]
Options:
        -t<x> : target size limit in KiB (ex -t64)
        -pad : add random bytes to pad up to the target size
        -noflash : remove color flash when depacking
        -data : raw data mode (no executable)
        -deflate : include DEFLATE algorithm (by default)
        -zx0 : include ZX0 algorithm (by default)
        -lz4 : include LZ4 algorithm (by default)
        -v : verbose mode
````

## Exemple
Pack foo.exe as best as you can
````
  L-Packer foo.exe foo_packed.exe
````

Pack Colombia.exe, aiming 64KiB demo category, and pad to 64KiB using random bytes
````
  L-Packer Colombia.exe Colombia_packed.exe -t64 -pad
````

Pack raw data mydata.bin, including both LZ4 and ZX0 algorithms and save the best result
````
  L-Packer -data -lz4 -zx0 mydata.bin mydata.lpk
````


## Already in production

Few demos are already using L-Packer, [check the list!]( https://www.pouet.net/lists.php?which=326 )

## Raw data mode

L-Packer is made to compress Amiga or Atari executables. But it can also used to pack raw data. It will also select the best algorithm and save a raw packed file.

## Source Code

The full source code will be released with version 1.0.
I’d like to gather community feedback first to improve and debug the tool.

## Credits

* Code & concept by Leonard / Oxygene
* Zopfli efficient Deflate compressor by Google
* Inflate 68k depacker by Keir Fraser
* ZX0 “Salvador” compressor & 68k decompressor by Emmanuel Marty
