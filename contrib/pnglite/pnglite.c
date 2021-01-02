/*
 * pnglite.c - pnglite library
 * For conditions of distribution and use, see copyright notice in pnglite.h
 */

/*
 * Note: this source is updated to enable build for FreeBSD boot loader.
 */

#ifdef _STANDALONE
#include <sys/cdefs.h>
#include <stand.h>
#else
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#endif
#include <zlib.h>
#include "pnglite.h"

#ifndef abs
#define	abs(x)	((x) < 0? -(x):(x))
#endif

#define	PNG_32b(b, s) ((uint32_t)(b) << (s))
#define	PNG_U32(b1, b2, b3, b4) \
	(PNG_32b(b1, 24) | PNG_32b(b2, 16) | PNG_32b(b3, 8) | PNG_32b(b4, 0))

#define	png_IDAT PNG_U32(73,  68,  65,  84)
#define	png_IEND PNG_U32(73,  69,  78,  68)

static ssize_t
file_read(png_t *png, void *out, size_t size, size_t numel)
{
	ssize_t result;
	off_t offset = (off_t)(size * numel);

	if (offset < 0)
		return (PNG_FILE_ERROR);

	if (!out) {
		result = lseek(png->fd, offset, SEEK_CUR);
	} else {
		result = read(png->fd, out, size * numel);
	}

	return (result);
}

static int
file_read_ul(png_t *png, unsigned *out)
{
	uint8_t buf[4];

	if (file_read(png, buf, 1, 4) != 4)
		return (PNG_FILE_ERROR);

	*out = (buf[0]<<24) | (buf[1]<<16) | (buf[2]<<8) | buf[3];

	return (PNG_NO_ERROR);
}

static unsigned
get_ul(uint8_t *buf)
{
	unsigned result;
	uint8_t foo[4];

	memcpy(foo, buf, 4);

	result = (foo[0]<<24) | (foo[1]<<16) | (foo[2]<<8) | foo[3];

	return (result);
}

static int
png_get_bpp(png_t *png)
{
	int bpp;

	switch (png->color_type) {
	case PNG_GREYSCALE:
		bpp = 1; break;
	case PNG_TRUECOLOR:
		bpp = 3; break;
	case PNG_INDEXED:
		bpp = 1; break;
	case PNG_GREYSCALE_ALPHA:
		bpp = 2; break;
	case PNG_TRUECOLOR_ALPHA:
		bpp = 4; break;
	default:
		return (PNG_FILE_ERROR);
	}

	bpp *= png->depth / 8;

	return (bpp);
}

static int
png_read_ihdr(png_t *png)
{
	unsigned length = 0;
	unsigned orig_crc;
	unsigned calc_crc;
	uint8_t ihdr[13+4]; /* length should be 13, make room for type (IHDR) */

	if (file_read_ul(png, &length) != PNG_NO_ERROR)
		return (PNG_FILE_ERROR);

	if (length != 13)
		return (PNG_CRC_ERROR);

	if (file_read(png, ihdr, 1, 13+4) != 13+4)
		return (PNG_EOF_ERROR);

	if (file_read_ul(png, &orig_crc) != PNG_NO_ERROR)
		return (PNG_FILE_ERROR);

	calc_crc = crc32(0L, Z_NULL, 0);
	calc_crc = crc32(calc_crc, ihdr, 13+4);

	if (orig_crc != calc_crc) {
		return (PNG_CRC_ERROR);
	}

	png->width = get_ul(ihdr+4);
	png->height = get_ul(ihdr+8);
	png->depth = ihdr[12];
	png->color_type = ihdr[13];
	png->compression_method = ihdr[14];
	png->filter_method = ihdr[15];
	png->interlace_method = ihdr[16];

	if (png->color_type == PNG_INDEXED)
		return (PNG_NOT_SUPPORTED);

	if (png->depth != 8 && png->depth != 16)
		return (PNG_NOT_SUPPORTED);

	if (png->interlace_method)
		return (PNG_NOT_SUPPORTED);

	return (PNG_NO_ERROR);
}

void
png_print_info(png_t *png)
{
	printf("PNG INFO:\n");
	printf("\twidth:\t\t%d\n", png->width);
	printf("\theight:\t\t%d\n", png->height);
	printf("\tdepth:\t\t%d\n", png->depth);
	printf("\tcolor:\t\t");

	switch (png->color_type) {
	case PNG_GREYSCALE:
		printf("greyscale\n"); break;
	case PNG_TRUECOLOR:
		printf("truecolor\n"); break;
	case PNG_INDEXED:
		printf("palette\n"); break;
	case PNG_GREYSCALE_ALPHA:
		printf("greyscale with alpha\n"); break;
	case PNG_TRUECOLOR_ALPHA:
		printf("truecolor with alpha\n"); break;
	default:
		printf("unknown, this is not good\n"); break;
	}

	printf("\tcompression:\t%s\n",
	    png->compression_method?
	    "unknown, this is not good":"inflate/deflate");
	printf("\tfilter:\t\t%s\n",
	    png->filter_method? "unknown, this is not good":"adaptive");
	printf("\tinterlace:\t%s\n",
	    png->interlace_method? "interlace":"no interlace");
}

int
png_open(png_t *png, const char *filename)
{
	char header[8];
	int result;

	png->image = NULL;
	png->fd = open(filename, O_RDONLY);
	if (png->fd == -1)
		return (PNG_FILE_ERROR);

	if (file_read(png, header, 1, 8) != 8) {
		result = PNG_EOF_ERROR;
		goto done;
	}

	if (memcmp(header, "\x89\x50\x4E\x47\x0D\x0A\x1A\x0A", 8) != 0) {
		result = PNG_HEADER_ERROR;
		goto done;
	}

	result = png_read_ihdr(png);
	if (result == PNG_NO_ERROR) {
		result = png_get_bpp(png);
		if (result > 0) {
			png->bpp = (uint8_t)result;
			result = PNG_NO_ERROR;
		}
	}

done:
	if (result == PNG_NO_ERROR) {
		uint64_t size = png->width * png->height * png->bpp;

		if (size < UINT_MAX)
			png->image = malloc(size);
		if (png->image == NULL)
			result = PNG_MEMORY_ERROR;
	}

	if (result == PNG_NO_ERROR)
		result = png_get_data(png, png->image);

	if (result != PNG_NO_ERROR) {
		free(png->image);
		(void) close(png->fd);
		png->fd = -1;
		return (result);
	}

	return (result);
}

int
png_close(png_t *png)
{
	(void) close(png->fd);
	png->fd = -1;
	free(png->image);
	png->image = NULL;

	return (PNG_NO_ERROR);
}

static int
png_init_inflate(png_t *png)
{
	z_stream *stream;
	png->zs = calloc(1, sizeof (z_stream));

	stream = png->zs;

	if (!stream)
		return (PNG_MEMORY_ERROR);

	if (inflateInit(stream) != Z_OK) {
		free(png->zs);
		png->zs = NULL;
		return (PNG_ZLIB_ERROR);
	}

	stream->next_out = png->png_data;
	stream->avail_out = png->png_datalen;

	return (PNG_NO_ERROR);
}

static int
png_end_inflate(png_t *png)
{
	z_stream *stream = png->zs;
	int rc = PNG_NO_ERROR;

	if (!stream)
		return (PNG_MEMORY_ERROR);

	if (inflateEnd(stream) != Z_OK) {
		printf("ZLIB says: %s\n", stream->msg);
		rc = PNG_ZLIB_ERROR;
	}

	free(png->zs);
	png->zs = NULL;

	return (rc);
}

static int
png_inflate(png_t *png, uint8_t *data, int len)
{
	int result;
	z_stream *stream = png->zs;

	if (!stream)
		return (PNG_MEMORY_ERROR);

	stream->next_in = data;
	stream->avail_in = len;

	result = inflate(stream, Z_SYNC_FLUSH);

	if (result != Z_STREAM_END && result != Z_OK) {
		printf("%s\n", stream->msg);
		return (PNG_ZLIB_ERROR);
	}

	if (stream->avail_in != 0)
		return (PNG_ZLIB_ERROR);

	return (PNG_NO_ERROR);
}

static int
png_read_idat(png_t *png, unsigned length)
{
	unsigned orig_crc;
	unsigned calc_crc;
	ssize_t len = length;

	if (!png->readbuf || png->readbuflen < length) {
		png->readbuf = realloc(png->readbuf, length);
		png->readbuflen = length;
	}

	if (!png->readbuf)
		return (PNG_MEMORY_ERROR);

	if (file_read(png, png->readbuf, 1, length) != len)
		return (PNG_FILE_ERROR);

	calc_crc = crc32(0L, Z_NULL, 0);
	calc_crc = crc32(calc_crc, (uint8_t *)"IDAT", 4);
	calc_crc = crc32(calc_crc, (uint8_t *)png->readbuf, length);

	if (file_read_ul(png, &orig_crc) != PNG_NO_ERROR)
		return (PNG_FILE_ERROR);

	if (orig_crc != calc_crc)
		return (PNG_CRC_ERROR);

	return (png_inflate(png, png->readbuf, length));
}

static int
png_process_chunk(png_t *png)
{
	int result = PNG_NO_ERROR;
	unsigned type;
	unsigned length;

	if (file_read_ul(png, &length) != PNG_NO_ERROR)
		return (PNG_FILE_ERROR);

	if (file_read_ul(png, &type) != PNG_NO_ERROR)
		return (PNG_FILE_ERROR);

	/*
	 * if we found an idat, all other idats should be followed with no
	 * other chunks in between
	 */
	if (type == png_IDAT) {
		if (!png->png_data) {	/* first IDAT */
			png->png_datalen = png->width * png->height *
			    png->bpp + png->height;
			png->png_data = malloc(png->png_datalen);
		}

		if (!png->png_data)
			return (PNG_MEMORY_ERROR);

		if (!png->zs) {
			result = png_init_inflate(png);
			if (result != PNG_NO_ERROR)
				return (result);
		}

		return (png_read_idat(png, length));
	} else if (type == png_IEND)
		return (PNG_DONE);
	else
		(void) file_read(png, 0, 1, length + 4); /* unknown chunk */

	return (result);
}

static void
png_filter_sub(unsigned stride, uint8_t *in, uint8_t *out, unsigned len)
{
	unsigned i;
	uint8_t a = 0;

	for (i = 0; i < len; i++) {
		if (i >= stride)
			a = out[i - stride];

		out[i] = in[i] + a;
	}
}

static void
png_filter_up(unsigned stride __unused, uint8_t *in, uint8_t *out,
    uint8_t *prev_line, unsigned len)
{
	unsigned i;

	if (prev_line) {
		for (i = 0; i < len; i++)
			out[i] = in[i] + prev_line[i];
	} else
		memcpy(out, in, len);
}

static void
png_filter_average(unsigned stride, uint8_t *in, uint8_t *out,
    uint8_t *prev_line, unsigned len)
{
	unsigned int i;
	uint8_t a = 0;
	uint8_t b = 0;
	unsigned int sum = 0;

	for (i = 0; i < len; i++) {
		if (prev_line)
			b = prev_line[i];

		if (i >= stride)
			a = out[i - stride];

		sum = a;
		sum += b;

		out[i] = in[i] + sum/2;
	}
}

static uint8_t
png_paeth(uint8_t a, uint8_t b, uint8_t c)
{
	int p = (int)a + b - c;
	int pa = abs(p - a);
	int pb = abs(p - b);
	int pc = abs(p - c);

	int pr;

	if (pa <= pb && pa <= pc)
		pr = a;
	else if (pb <= pc)
		pr = b;
	else
		pr = c;

	return (pr);
}

static void
png_filter_paeth(unsigned stride, uint8_t *in, uint8_t *out, uint8_t *prev_line,
    unsigned len)
{
	unsigned i;
	uint8_t a;
	uint8_t b;
	uint8_t c;

	for (i = 0; i < len; i++) {
		if (prev_line && i >= stride) {
			a = out[i - stride];
			b = prev_line[i];
			c = prev_line[i - stride];
		} else {
			if (prev_line)
				b = prev_line[i];
			else
				b = 0;

			if (i >= stride)
				a = out[i - stride];
			else
				a = 0;

			c = 0;
		}

		out[i] = in[i] + png_paeth(a, b, c);
	}
}

static int
png_unfilter(png_t *png, uint8_t *data)
{
	unsigned i;
	unsigned pos = 0;
	unsigned outpos = 0;
	uint8_t *filtered = png->png_data;
	unsigned stride = png->bpp;

	while (pos < png->png_datalen) {
		uint8_t filter = filtered[pos];

		pos++;

		if (png->depth == 16) {
			for (i = 0; i < png->width * stride; i += 2) {
				*(short *)(filtered+pos+i) =
				    (filtered[pos+i] << 8) | filtered[pos+i+1];
			}
		}

		switch (filter) {
		case 0: /* none */
			memcpy(data+outpos, filtered+pos, png->width * stride);
			break;
		case 1: /* sub */
			png_filter_sub(stride, filtered+pos, data+outpos,
			    png->width * stride);
			break;
		case 2: /* up */
			if (outpos) {
				png_filter_up(stride, filtered+pos, data+outpos,
				    data + outpos - (png->width*stride),
				    png->width*stride);
			} else {
				png_filter_up(stride, filtered+pos, data+outpos,
				    0, png->width*stride);
			}
			break;
		case 3: /* average */
			if (outpos) {
				png_filter_average(stride, filtered+pos,
				    data+outpos,
				    data + outpos - (png->width*stride),
				    png->width*stride);
			} else {
				png_filter_average(stride, filtered+pos,
				    data+outpos, 0, png->width*stride);
			}
			break;
		case 4: /* paeth */
			if (outpos) {
				png_filter_paeth(stride, filtered+pos,
				    data+outpos,
				    data + outpos - (png->width*stride),
				    png->width*stride);
			} else {
				png_filter_paeth(stride, filtered+pos,
				    data+outpos, 0, png->width*stride);
			}
			break;
		default:
			return (PNG_UNKNOWN_FILTER);
		}

		outpos += png->width * stride;
		pos += png->width * stride;
	}

	return (PNG_NO_ERROR);
}

int
png_get_data(png_t *png, uint8_t *data)
{
	int result = PNG_NO_ERROR;

	png->zs = NULL;
	png->png_datalen = 0;
	png->png_data = NULL;
	png->readbuf = NULL;
	png->readbuflen = 0;

	while (result == PNG_NO_ERROR)
		result = png_process_chunk(png);

	if (png->readbuf) {
		free(png->readbuf);
		png->readbuflen = 0;
	}
	if (png->zs)
		(void) png_end_inflate(png);

	if (result != PNG_DONE) {
		free(png->png_data);
		return (result);
	}

	result = png_unfilter(png, data);

	free(png->png_data);

	return (result);
}

char *
png_error_string(int error)
{
	switch (error) {
	case PNG_NO_ERROR:
		return ("No error");
	case PNG_FILE_ERROR:
		return ("Unknown file error.");
	case PNG_HEADER_ERROR:
		return ("No PNG header found. Are you sure this is a PNG?");
	case PNG_IO_ERROR:
		return ("Failure while reading file.");
	case PNG_EOF_ERROR:
		return ("Reached end of file.");
	case PNG_CRC_ERROR:
		return ("CRC or chunk length error.");
	case PNG_MEMORY_ERROR:
		return ("Could not allocate memory.");
	case PNG_ZLIB_ERROR:
		return ("zlib reported an error.");
	case PNG_UNKNOWN_FILTER:
		return ("Unknown filter method used in scanline.");
	case PNG_DONE:
		return ("PNG done");
	case PNG_NOT_SUPPORTED:
		return ("The PNG is unsupported by pnglite, too bad for you!");
	case PNG_WRONG_ARGUMENTS:
		return ("Wrong combination of arguments passed to png_open. "
		    "You must use either a read_function or supply a file "
		    "pointer to use.");
	default:
		return ("Unknown error.");
	};
}
