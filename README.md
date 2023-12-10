# unPNG - Uncompressed PNG (WIP)

A backward-compatible subset of the PNG file format, for storing *uncompressed* bitmaps. unPNG files can be decoded with a simple zero-copy parser.

## Why?

Parsing image file formats is [hard](https://binarly.io/posts/The_Far_Reaching_Consequences_of_LogoFAIL/). Most image file formats in common usage are rather flexible, and PNG is no exception. Flexibility breeds bugs, and this is an experiment in cutting it down to the bare minimum.

All unPNGs are fully valid PNGs, and can be viewed in standard image viewers.

However, most PNGs are not valid unPNGs - you need to use a special encoder (like the one in this repo).

## What?

This repo contains an encoder and a decoder program. The encoder is writte in Python, and the decoder is a single-header C library.

Notably, the decoder **never copies any buffers**. It simply verifies the structure of the buffer you give it, and returns the metadata you need to view/process it - the width, height, pixel format, stride.

The decoder also doesn't bother verifying checksums, for simplicity - but it could if it wanted to.

## How?

The non-compression is achieved using uncompressed DEFLATE blocks. The maximum length of an uncompressed DEFLATE block is 0xffff bytes - smaller than most image files. To avoid having to parse and re-assemble blocks, each row of the image is encapsulated within its own DEFLATE block. This gives a theoretical maximum horizontal resolution of 16383 pixels (assuming `RGBA8888` pixel format), although unPNG further limits itself to 8192px in width and height, just to stay on the safe side.

Each row is also encapsulated within its own PNG IDAT chunk. This is technically unnecessary (since a 8192x8192px image would be able to fit in a single chunk), but it makes the encode/decode logic ever so slightly simpler (I might change my mind about this!).

PNG supports ancilliary chunks and other sources of flexibility. unPNG does not - we only support a specific chunk layout. The unPNG decoder never attempts to "parse" chunks (or parse zlib, or parse DEFLATE), it simply treats the expected values as magic byte sequences.

Until I write proper docs/specs, looking at `encoder.py` is perhaps the easiest way to understand the details.

# TODO

- Write a spec!

- Document `unpng.h`

- Set up fuzzing.
