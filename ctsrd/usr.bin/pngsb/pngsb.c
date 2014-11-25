/*-
 * Copyright (c) 2012 SRI International
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract (FA8750-10-C-0237)
 * ("CTSRD"), as part of the DARPA CRASH research programme.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/types.h>

#include <sys/endian.h>

#include <terasic_mtl.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <imagebox.h>
#include <png.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#if 0
static void read_row_callback(png_structp, png_uint_32, int);
static void read_png_from_fd(png_structp, png_bytep, png_size_t);

struct pngstate {
	uint32_t		 width;
	uint32_t		 height;
	volatile uint32_t	 valid_rows;
	volatile uint32_t	 passes_remaining;
	volatile uint32_t	 error;
	volatile uint32_t	*buffer;

	void		*private;
};

struct pthr_decode_state
{
	int		 fd;
	struct pngstate	*ps;
};

struct pthr_decode_private
{
	pthread_t	pthr;
};

static void *
pthr_decode_png(void *arg)
{
	int bit_depth, color_type, interlace_type;
	png_uint_32 r, width, height;
	struct pthr_decode_state *pds = arg;
	png_structp png_ptr;
	png_infop info_ptr;
	png_infop end_info;
	png_bytep *rows;

	if ((png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING,
	    NULL, NULL, NULL)) == NULL) {
		pds->ps->error = 1;
		close(pds->fd);
		free(pds);
		pthread_exit(NULL);
	}
	if ((info_ptr = png_create_info_struct(png_ptr)) == NULL) {
		png_destroy_read_struct(&png_ptr, NULL, NULL);
		pds->ps->error = 1;
		close(pds->fd);
		free(pds);
		pthread_exit(NULL);
	}
	if ((end_info = png_create_info_struct(png_ptr)) == NULL) {
		png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
		pds->ps->error = 1;
		close(pds->fd);
		free(pds);
		pthread_exit(NULL);
	}

	if (setjmp(png_jmpbuf(png_ptr))) {
		png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
		pds->ps->error = 1;
		close(pds->fd);
		free(pds);
		pthread_exit(NULL);
	}

#if 0
	/* XXX Insert back door function here */
	png_set_read_user_chunk_fn(png_ptr, user_chunk_ptr,
	    read_chunk_callback);
#endif

	png_set_read_status_fn(png_ptr, read_row_callback);

	/*
	 * Reject the image if the parser finds a different size than
	 * our manual parsing did.
	 */
#if 0
	png_set_user_limits(png_ptr, width, height);
#endif

	png_set_read_fn(png_ptr, pds, read_png_from_fd);

	png_read_info(png_ptr, info_ptr);

	png_get_IHDR(png_ptr, info_ptr, &width, &height, &bit_depth,
	    &color_type, &interlace_type, NULL, NULL);

	printf("bit_depth = %d, color_type = %d\n", bit_depth, color_type);

	if (width != pds->ps->width || height != pds->ps->height) {
		png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
		pds->ps->error = 1;
		close(pds->fd);
		free(pds);
		pthread_exit(NULL);
	}

	png_set_gray_to_rgb(png_ptr);
	png_set_bgr(png_ptr);
	png_set_filler(png_ptr, 0, PNG_FILLER_AFTER);
	pds->ps->passes_remaining = png_set_interlace_handling(png_ptr);
	png_read_update_info(png_ptr, info_ptr);

	if ((rows = malloc(height*sizeof(png_bytep))) == NULL)
		png_error(png_ptr, "failed to malloc row array");
	for (r = 0; r < height; r++)
		rows[r] = __DEVOLATILE(png_bytep,
		     pds->ps->buffer + (width * r));

	png_read_rows(png_ptr, rows, NULL, height);

	png_read_end(png_ptr, end_info);
	png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
	free(rows);

	pthread_exit(NULL);
}

/*
 * Begin decoding a stream containing a PNG image.  Reads will proceed
 * in the background.  The file descriptor will be under the control of
 * the png_read code and will be closed when decoding is complete.
 */
static struct pngstate*
png_read_start(int pfd, uint32_t maxw, uint32_t maxh)
{
	uint32_t header[9];
	char *cheader = (char *)header;
	char ihdr[] = {0x00, 0x00, 0x00, 0x0d, 'I', 'H', 'D', 'R'};
	struct pngstate			*ps;
	struct pthr_decode_state	*pds;
	struct pthr_decode_private	*pdp;
	
	if (read(pfd, header, sizeof(header)) != sizeof(header)) {
		close(pfd);
		return (NULL);
	}
	/*
	 * XXX: Should store data in a struct to be retrieved in
	 * read_png_from_fd() to support non-seekable streams.
	 */
	if (lseek(pfd, 0, SEEK_SET) != 0) {
		close(pfd);
		return (NULL);
	}

	if (png_sig_cmp(cheader, 0, 8) != 0) {
		errno = EINVAL;
		return (NULL);
	}
	if (memcmp(header + 2, ihdr, sizeof(ihdr)) != 0)
		errx(1, "First chunk is not an IHDR");

	if ((ps = malloc(sizeof(struct pngstate))) == NULL) {
		close(pfd);
		return (NULL);
	}
	memset(ps, 0, sizeof(struct pngstate));
	ps->width = be32toh(*(header + 4));
	ps->height = be32toh(*(header + 5));
	if (ps->width > maxw || ps->height > maxh) {
		close(pfd);
		free(ps);
		return (NULL);
	}
	ps->passes_remaining = UINT32_MAX;

	if ((ps->buffer = malloc(ps->width * ps->height * sizeof(*ps->buffer)))
	    == NULL) {
		close(pfd);
		free(ps);
		return (NULL);
	}

	if ((pds = malloc(sizeof(struct pthr_decode_state))) == NULL) {
		close(pfd);
		free(__DEVOLATILE(void*, ps->buffer));
		free(ps);
		return (NULL);
	}
	pds->ps = ps;
	pds->fd = pfd;

	if ((pdp = malloc(sizeof(struct pthr_decode_private))) == NULL) {
		close(pfd);
		free(__DEVOLATILE(void*, ps->buffer));
		free(ps);
		free(pds);
	}
	ps->private = pdp;
	
	if (pthread_create(&(pdp->pthr), NULL, pthr_decode_png, pds) != 0) {
		close(pfd);
		free(__DEVOLATILE(void*, ps->buffer));
		free(ps);
		free(pds);
		free(pdp);
	}

	return (ps);
}

/*
 * Return when the png has finished decoding.
 */
static int
png_read_finish(struct pngstate *ps)
{
	int error;
	struct pthr_decode_private *pdp = ps->private;

	error = pthread_join(pdp->pthr, NULL);
	free(pdp);
	ps->private = NULL;
	return (error);
}

static void
pngstate_free(struct pngstate *ps)
{

	if (ps->private != NULL)
		png_read_finish(ps);
	free(__DEVOLATILE(void*, ps->buffer));
	free(ps);
}
#endif

static void
usage(void)
{
	errx(1, "usage: pngsb <file>");
}

#if 0
static void
read_row_callback(png_structp png_ptr, png_uint_32 row, int pass __unused)
{
	struct pthr_decode_state *pds;

	pds = png_get_io_ptr(png_ptr);
	if (pds->ps->valid_rows < row)
		pds->ps->valid_rows = row;
	if (row == pds->ps->height)
		pds->ps->passes_remaining--;
}

static void
read_png_from_fd(png_structp png_ptr, png_bytep data, png_size_t length)
{
	struct pthr_decode_state *pds;
	ssize_t rlen;

	pds = png_get_io_ptr(png_ptr);
	rlen = read(pds->fd, data, length);
	if (rlen < 0 || (png_size_t)rlen != length)
		png_error(png_ptr, "read error");
}
#endif

int
main(int argc, char **argv)
{	
	int pfd;
	uint32_t i, last_row = 0;
	struct iboxstate *ps;

	if (argc != 2)
		usage();

	fb_init();

	if ((pfd = open(argv[1], O_RDONLY)) < -1)
		err(1, "open(%s)", argv[1]);

	if ((ps = png_read_start(pfd, 800, 480, SB_CHERI)) == NULL)
		err(1, "failed to initialize read of %s", argv[1]);

	/* XXX: do something with the valid parts of the image as it decodes. */
	while(ps->valid_rows < ps->height ) {
		if (last_row != ps->valid_rows) {
			for (i = last_row; i < ps->valid_rows; i++)
				memcpy(__DEVOLATILE(void*,
				     pfbp + (i * fb_width)),
				    __DEVOLATILE(void *,
				    ps->buffer + (i * ps->width)),
				    sizeof(uint32_t) * ps->width);
			last_row = ps->valid_rows;
			printf("valid_rows = %d\n", ps->valid_rows);
		}
#if 0
		pthread_yield();
#endif
	}
	if (last_row != ps->valid_rows) {
		for (i = last_row; i < ps->valid_rows; i++)
			memcpy(__DEVOLATILE(void*,
			     pfbp + (i * fb_width)),
			    __DEVOLATILE(void *,
			    ps->buffer + (i * ps->width)),
			    sizeof(uint32_t) * ps->width);
		last_row = ps->valid_rows;
	}
	printf("valid_rows = %d\n", ps->valid_rows);

	if (png_read_finish(ps) != 0)
		errx(1, "png_read_finish failed");

	iboxstate_free(ps);

	fb_fini();

	return(0);
}
