// todo: make this more portable, don't #include anything

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#ifdef UNPNG_DEBUG
#include <assert.h>
#include <stdio.h> // only used for debug printfs
#endif

#define BUF_CMP(buffer, target) memcmp(buffer, target, sizeof(target))

enum unpng_pixfmt {
	UNPNG_PIXFMT_ANY = 0, // currently unused
	UNPNG_PIXFMT_INVALID = 1, // currently unused
	UNPNG_PIXFMT_RGB888 = 2,
	UNPNG_PIXFMT_RGBA8888 = 3,
};

enum unpng_err {
	UNPNG_OK = 0, 
	UNPNG_ERR_MAGIC = -1, // bad magic bytes
	UNPNG_ERR_LENGTH = -2, // bad buffer length
	UNPNG_ERR_SIZE = -3, // invalid image dimensions
};

struct unpng
{
	uint32_t width;
	uint32_t height;
	uint32_t stride;
	enum unpng_pixfmt pixfmt;
	const uint8_t *buffer;
};

static const uint8_t UNPNG_MAGIC0[] = {
	0x89, 'P',  'N',  'G',  0x0d, 0x0a, 0x1a, 0x0a,
	0x00, 0x00, 0x00, 0x0d,
	'I',  'H',  'D',  'R'
};

static const uint8_t UNPNG_MAGIC1_RGB888[] = {
	0x08, 0x02, 0x00, 0x00, 0x00
};
static const uint8_t UNPNG_MAGIC1_RGBA8888[] = {
	0x08, 0x06, 0x00, 0x00, 0x00
};

static const uint8_t UNPNG_MAGIC2[] = {
	0x00, 0x00, 0x00, 0x01,
	'u',  'n',  'P',  'n', 'G',
	0x53, 0x93, 0xa4, 0x38
};

static const uint8_t UNPNG_MAGIC_ROWEND[] = {
	0x03, 0x00 // zlib final block
};
static const uint8_t UNPNG_MAGIC_IEND[] = {
	0x00, 0x00, 0x00, 0x00,
	'I',  'E',  'N',  'D',
	0xae, 0x42, 0x60, 0x82
};

static const uint32_t UNPNG_MAGIC1_OFF = sizeof(UNPNG_MAGIC0) + 8; // +8 for png width and height fields
static const uint32_t UNPNG_MAGIC2_OFF = UNPNG_MAGIC1_OFF + sizeof(UNPNG_MAGIC1_RGB888) + 4; // +4 for the IHDR crc
static const uint32_t UNPNG_IDAT_OFF = UNPNG_MAGIC2_OFF + sizeof(UNPNG_MAGIC2);
static const uint32_t UNPNG_HEADER_SIZE = UNPNG_IDAT_OFF + 10; // 10 is sizeof(idat_magic)
static const uint32_t UNPNG_OVERHEAD = UNPNG_HEADER_SIZE + sizeof(UNPNG_MAGIC_ROWEND) + 8 + sizeof(UNPNG_MAGIC_IEND); // +8 for adler32 + IDAT crc
static const uint32_t UNPNG_ROW_OVERHEAD = 8; // sizeof(row_magic)
static const uint32_t UNPNG_MAX_RES = 0x2000;

// this is really the only thing we do that resembles traditional parsing
static uint32_t unpng_unpack_be32(const uint8_t data[4])
{
	return (data[0] << 24) | \
	       (data[1] << 16) | \
	       (data[2] <<  8) | \
	       (data[3] <<  0);
}

static int unpng_parse(struct unpng *img, const uint8_t *data, size_t length)
{
	// make sure the file is at least as long as the headers we're about to read
	if (length < UNPNG_HEADER_SIZE) {
		return UNPNG_ERR_LENGTH;
	}

	// check the header magic
	if (BUF_CMP(data, UNPNG_MAGIC0) != 0) {
		return UNPNG_ERR_MAGIC;
	}

	img->width = unpng_unpack_be32(data + sizeof(UNPNG_MAGIC0));
	img->height = unpng_unpack_be32(data + sizeof(UNPNG_MAGIC0) + 4);

	if (img->width == 0 || img->height == 0 || img->width > UNPNG_MAX_RES || img->height > UNPNG_MAX_RES) {
		return UNPNG_ERR_SIZE; // 8k ought to be enough for anyone!
	}

	// Figure out the pixel format by comparing against all possible magic byte values
	if (BUF_CMP(data + UNPNG_MAGIC1_OFF, UNPNG_MAGIC1_RGB888) == 0) {
		img->pixfmt = UNPNG_PIXFMT_RGB888;
		img->stride = img->width * 3 + UNPNG_ROW_OVERHEAD;
	} else if (BUF_CMP(data + UNPNG_MAGIC1_OFF, UNPNG_MAGIC1_RGBA8888) == 0) {
		img->pixfmt = UNPNG_PIXFMT_RGBA8888;
		img->stride = img->width * 4 + UNPNG_ROW_OVERHEAD;
	} else {
		return UNPNG_ERR_MAGIC;
	}

	// check for "unPnG" magic chunk
	if (BUF_CMP(data + UNPNG_MAGIC2_OFF, UNPNG_MAGIC2) != 0) {
		return UNPNG_ERR_MAGIC;
	}

	// make sure the buffer is precisely the expected length
	if (img->stride * img->height + UNPNG_OVERHEAD != length) {
		return UNPNG_ERR_LENGTH;
	}

	img->buffer = data + UNPNG_HEADER_SIZE + UNPNG_ROW_OVERHEAD;

#if UNPNG_DEBUG
	fprintf(stderr, "width=%u\n", img->width);
	fprintf(stderr, "height=%u\n", img->height);
	fprintf(stderr, "stride=%u\n", img->stride);
	fprintf(stderr, "pixfmt=%u\n", img->pixfmt);

	fprintf(stderr, "UNPNG_HEADER_SIZE=%u\n", UNPNG_HEADER_SIZE);

	assert(UNPNG_HEADER_SIZE + UNPNG_ROW_OVERHEAD + img->stride * img->height < length);
#endif

	/*
	At this point, all the actual parsing is already complete, but we still
	want to check various constrants to ensure that the input
	was a valid PNG. We don't check checksums, though.
	*/

	const uint32_t idat_len = img->stride * img->height + 8; // due to constraints on width and height, this can never overflow
	const unsigned char idat_magic[] = {
		(idat_len >> 24) & 0xff,
		(idat_len >> 16) & 0xff,
		(idat_len >>  8) & 0xff,
		(idat_len >>  0) & 0xff,
		'I',  'D',  'A',  'T',
		0x78, 0x01 // zlib magic
	};
#if UNPNG_DEBUG
	fprintf(stderr, "DEBUG: idat_magic = ");
	for (size_t i=0; i<sizeof(idat_magic); i++) {
		fprintf(stderr, "%02hhx ", idat_magic[i]);
	}
	fprintf(stderr, "\n");
#endif
	if (BUF_CMP(data + UNPNG_IDAT_OFF, idat_magic) != 0) {
		return UNPNG_ERR_MAGIC;
	}

	const uint32_t row_len = img->stride - UNPNG_ROW_OVERHEAD + 1;
	const unsigned char row_magic[] = {
		0x02, 0x08, 0x00, // deflate padding, start non-final uncompressed deflate block
		row_len & 0xff, (row_len >> 8) & 0xff,
		(row_len & 0xff) ^ 0xff, ((row_len >> 8) & 0xff) ^ 0xff,
		0x00 // png row filter byte
	};

#if UNPNG_DEBUG
	fprintf(stderr, "DEBUG: row_magic = ");
	for (size_t i=0; i<sizeof(row_magic); i++) {
		fprintf(stderr, "%02hhx ", row_magic[i]);
	}
	fprintf(stderr, "\n");
#endif

	for (uint32_t y=0; y < img->height; y++) {
		if (BUF_CMP(data + UNPNG_HEADER_SIZE + y * img->stride, row_magic) != 0) {
			return UNPNG_ERR_MAGIC;
		}
	}

	if (BUF_CMP(data + UNPNG_HEADER_SIZE + img->height * img->stride, UNPNG_MAGIC_ROWEND) != 0) {
		return UNPNG_ERR_MAGIC;
	}

	if (BUF_CMP(data + UNPNG_HEADER_SIZE + img->height * img->stride + sizeof(UNPNG_MAGIC_ROWEND) + 8, UNPNG_MAGIC_IEND) != 0) {
		return UNPNG_ERR_MAGIC;
	}

	return UNPNG_OK;
}
