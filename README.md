# unPNG - Uncompressed PNG (WIP)

A backward-compatible subset of the PNG file format, for storing *uncompressed* bitmaps. unPNG files can be decoded with a simple zero-copy parser.

![annotated hexdump of example file](https://github.com/DavidBuchanan314/unPNG/assets/13520633/e8c0fbf0-5dec-431b-8c91-e70510ba886c)
*^ Annotated hexdump of an 8x8 8-bit greyscale unPNG file*

## Why?

Parsing image file formats is [hard](https://binarly.io/posts/The_Far_Reaching_Consequences_of_LogoFAIL/). Most image file formats in common usage are rather flexible, and PNG is no exception. Flexibility breeds bugs, and this is an experiment in cutting it down to the bare minimum.

All unPNGs are valid PNGs, and can be viewed in standard image viewers.

However, most PNGs are not valid unPNGs - you need to use a special encoder (like the one in this repo).

## Why not?

- There's 8 bytes of overhead per row. If you're dealing with very small images, that's a lot.

- BMP already exists - although it's a rather crufty format. It would be interesting to compare unPNG to a minimal BMP encoder/decoder.

- libpng (and others) are well documented and battle-tested.

- Parsing itself is not the only source of unsafety when dealing with image formats.

## What's in this repo?

This repo contains an encoder and a decoder program. The encoder is written in Python, and the decoder is a single-header C library.

Notably, the decoder **never copies any buffers**. It simply verifies the structure of the buffer you give it, and returns the metadata you need to view/process it - the width, height, pixel format, stride (aka pitch).

The decoder also doesn't bother verifying checksums, for simplicity - but it could if it wanted to. The *en*coder on the other hand emits files with valid checksums, passing checks from `pngfix` et al.

`demo.c` is a simple demo program that hooks up `unpng.h` to SDL2, allowing you to view decoded images.

Although unPNG is designed to be implemented securely, **this implementation is prototype-quality, mostly untested, and should not be especially trusted**. I may also make breaking changes to the spec.

## How?

The non-compression is achieved using uncompressed DEFLATE blocks. The maximum length of an uncompressed DEFLATE block is 0xffff bytes - smaller than most image files. To avoid having to parse and re-assemble blocks, each row of the image is encapsulated within its own DEFLATE block. This gives a theoretical maximum horizontal resolution of 16383 pixels (assuming `RGBA8888` pixel format), although unPNG further limits itself to 8192px in width and height, just to stay on the safe side. These limits also ensure that the whole image can always fit in a single IDAT chunk.

Always using a single IDAT chunk negates the need for chunk reassembly etc.

PNG supports ancilliary chunks and other sources of flexibility. unPNG does not - we only support a specific chunk layout. The unPNG decoder never attempts to "parse" chunks (or parse zlib, or parse DEFLATE), it simply treats the expected values as magic byte sequences, and bails out if they don't match.

That specific chunk layout is:

```
IHDR
unPn
IDAT
IEND
```

The `unPn` chunk contains a single byte - ascii `G`, meaning that if you inspect the file in a hexeditor you'll see the string `unPnG`. This chunk has two purposes - firstly it makes the file more easily identifiable as an unPNG file (to humans and machines alike), and secondly it acts as padding, ensuring that the first byte of pixel data occurs `0x40` bytes into the file. Alignment is nice to have!

*If* I end up adding support for palette modes, there'll be a PLTE chunk too.

Until I write proper docs/specs, looking at the source is perhaps the easiest way to understand the details.

# TODO

- Implement pixel formats other than RGB888, RGBA8888

- Write a spec!

- Document `unpng.h`

- Make `unpng.h` more portable.

- Set up tests and fuzzing.

- Consider also writing an encoder in C, and a decoder in Python.

## Some other things you may be interested in:

- https://www.da.vidbuchanan.co.uk/blog/hello-png.html - An introduction to the PNG file format itself.

- http://public-domain.advel.cz/ - Some minimal zlib and PNG implementations.

- https://en.wikipedia.org/wiki/Netpbm - A minimal bitmap file format.

- https://richg42.blogspot.com/2023/12/announcing-cpng-compatible-network.html - CPNG, a backwards compatible fork of PNG. While unPNG is focused on minimalism, CPNG focuses on enhancements. In some sense they're opposites!

- https://qoiformat.org/ - QOI, "The Quite OK Image Format for
Fast, Lossless Compression"

- https://tools.suckless.org/farbfeld/ - "farbfeld is a lossless image format which is easy to parse, pipe and compress"
