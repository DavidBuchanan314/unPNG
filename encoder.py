import zlib
from PIL import Image
from enum import Enum

class UNPNG_PIXFMT(Enum):
	RGB888 = 2
	RGBA8888 = 3

# https://www.w3.org/TR/2022/WD-png-3-20221025/#5PNG-file-signature
PNG_SIGNATURE = b'\x89PNG\r\n\x1a\n'

# https://www.w3.org/TR/2022/WD-png-3-20221025/#dfn-png-four-byte-unsigned-integer
# Helper function to pack an int into a "PNG 4-byte unsigned integer"
def encode_png_uint31(value):
	if value > 2**31 - 1:  # This is unlikely to ever happen!
		raise ValueError("Too big!")
	return value.to_bytes(4, "big")

def write_png_chunk(stream, chunk_type, chunk_data):
	# https://www.w3.org/TR/2022/WD-png-3-20221025/#5CRC-algorithm
	# Fortunately, zlib's CRC32 implementation is compatible with PNG's spec:
	crc = zlib.crc32(chunk_type + chunk_data)

	stream.write(encode_png_uint31(len(chunk_data)))
	stream.write(chunk_type)
	stream.write(chunk_data)
	stream.write(crc.to_bytes(4, "big"))


def encode_png_ihdr(
		width,
		height,
		bit_depth=8,           # bits per sample
		colour_type=2,         # 2 = "Truecolour" (RGB)
		compression_method=0,  # 0 = zlib/DEFLATE (only specified value)
		filter_method=0,       # 0 = "adaptive filtering" (only specified value)
		interlace_method=0):   # 0 = no interlacing (1 = Adam7 interlacing)

	ihdr = b""
	ihdr += encode_png_uint31(width)
	ihdr += encode_png_uint31(height)
	ihdr += bytes([
		bit_depth,
		colour_type,
		compression_method,
		filter_method,
		interlace_method
	])

	return ihdr

PIL_MODES = {
	UNPNG_PIXFMT.RGB888:   "RGB",
	UNPNG_PIXFMT.RGBA8888: "RGBA",
}

PNG_COLOUR_TYPES = {
	UNPNG_PIXFMT.RGB888:   2,
	UNPNG_PIXFMT.RGBA8888: 6,
}

UNPNG_MAX_RES = 0x2000

def unpng_encode(out, pixfmt: UNPNG_PIXFMT, im: Image):
	width, height = im.size
	if width > UNPNG_MAX_RES or height > UNPNG_MAX_RES:
		raise Exception("image too big")
	
	imdata = b"".join(bytes(x) for x in im.convert(PIL_MODES[pixfmt]).getdata())

	out.write(PNG_SIGNATURE)
	write_png_chunk(out, b"IHDR", encode_png_ihdr(width, height, colour_type=PNG_COLOUR_TYPES[pixfmt]))
	write_png_chunk(out, b"unPn", b"G")

	idat = b"\x78\x01"

	if pixfmt == UNPNG_PIXFMT.RGB888:
		base_stride = 3 * width
	elif pixfmt == UNPNG_PIXFMT.RGBA8888:
		base_stride = 4 * width
	else:
		raise Exception("bad pixfmt")
	
	row_magic_len = base_stride + 1
	row_magic = (
		b"\x02\x08\x00" + # non-final uncompressed deflate block
		row_magic_len.to_bytes(2, "little") +
		(row_magic_len ^ 0xffff).to_bytes(2, "little") +
		b"\x00" # png row filter
	)

	adlersum = 1
	for y in range(height):
		row_data = imdata[y*base_stride:(y+1)*base_stride]
		adlersum = zlib.adler32(b"\x00", adlersum) # account for filter byte
		adlersum = zlib.adler32(row_data, adlersum)
		idat += row_magic + row_data

	# final deflate block of length zero, plus adler32
	idat += b"\x03\x00" + adlersum.to_bytes(4, "big")

	write_png_chunk(out, b"IDAT", idat)
	write_png_chunk(out, b"IEND", b"")

if __name__ == "__main__":
	import argparse

	parser = argparse.ArgumentParser(
		prog="unPNG encoder",
		description="Convert any format PIL can open into an unPNG file.",
	)

	parser.add_argument("input", help="input file path")
	parser.add_argument("output", help="output file path")
	parser.add_argument("--no-alpha", help="whether to output an alpha channel", default=False, required=False, action="store_true")

	args = parser.parse_args()

	im = Image.open(args.input)
	with open(args.output, "wb") as out:
		unpng_encode(out, UNPNG_PIXFMT.RGB888 if args.no_alpha else UNPNG_PIXFMT.RGBA8888, im)
