/*
 *  pnglite.h - Interface for pnglite library
 *	Copyright (c) 2007 Daniel Karling
 *
 *	This software is provided 'as-is', without any express or implied
 *	warranty. In no event will the authors be held liable for any damages
 *	arising from the use of this software.
 *
 *	Permission is granted to anyone to use this software for any purpose,
 *	including commercial applications, and to alter it and redistribute it
 *	freely, subject to the following restrictions:
 *
 *	1. The origin of this software must not be misrepresented; you must not
 *	   claim that you wrote the original software. If you use this software
 *	   in a product, an acknowledgment in the product documentation would be
 *	   appreciated but is not required.
 *
 *	2. Altered source versions must be plainly marked as such, and must not
 *	   be misrepresented as being the original software.
 *
 *	3. This notice may not be removed or altered from any source
 *	   distribution.
 *
 *	Daniel Karling
 *	daniel.karling@gmail.com
 */


#ifndef _PNGLITE_H_
#define	_PNGLITE_H_

#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 *	Enumerations for pnglite.
 *	Negative numbers are error codes and 0 and up are okay responses.
 */

enum {
	PNG_DONE			= 1,
	PNG_NO_ERROR			= 0,
	PNG_FILE_ERROR			= -1,
	PNG_HEADER_ERROR		= -2,
	PNG_IO_ERROR			= -3,
	PNG_EOF_ERROR			= -4,
	PNG_CRC_ERROR			= -5,
	PNG_MEMORY_ERROR		= -6,
	PNG_ZLIB_ERROR			= -7,
	PNG_UNKNOWN_FILTER		= -8,
	PNG_NOT_SUPPORTED		= -9,
	PNG_WRONG_ARGUMENTS		= -10
};

/*
 *	The five different kinds of color storage in PNG files.
 */

enum {
	PNG_GREYSCALE			= 0,
	PNG_TRUECOLOR			= 2,
	PNG_INDEXED			= 3,
	PNG_GREYSCALE_ALPHA		= 4,
	PNG_TRUECOLOR_ALPHA		= 6
};

typedef struct {
	void			*zs;		/* pointer to z_stream */
	int			fd;
	unsigned char		*image;

	unsigned char		*png_data;
	unsigned		png_datalen;

	unsigned		width;
	unsigned		height;
	unsigned char		depth;
	unsigned char		color_type;
	unsigned char		compression_method;
	unsigned char		filter_method;
	unsigned char		interlace_method;
	unsigned char		bpp;

	unsigned char		*readbuf;
	unsigned		readbuflen;
} png_t;


/*
 *	Function: png_open
 *
 *	This function is used to open a png file with the internal file
 *	IO system.
 *
 *	Parameters:
 *		png - Empty png_t struct.
 *		filename - Filename of the file to be opened.
 *
 *	Returns:
 *		PNG_NO_ERROR on success, otherwise an error code.
 */

int png_open(png_t *png, const char *filename);

/*
 *	Function: png_print_info
 *
 *	This function prints some info about the opened png file to stdout.
 *
 *	Parameters:
 *		png - png struct to get info from.
 */

void png_print_info(png_t *png);

/*
 *	Function: png_error_string
 *
 *	This function translates an error code to a human readable string.
 *
 *	Parameters:
 *		error - Error code.
 *
 *	Returns:
 *		Pointer to string.
 */

char *png_error_string(int error);

/*
 *	Function: png_get_data
 *
 *	This function decodes the opened png file and stores the result in data.
 *	data should be big enough to hold the decoded png.
 *	Required size will be:
 *
 *	> width*height*(bytes per pixel)
 *
 *	Parameters:
 *		data - Where to store result.
 *
 *	Returns:
 *		PNG_NO_ERROR on success, otherwise an error code.
 */

int png_get_data(png_t *png, uint8_t *data);

/*
 *	Function: png_close
 *
 *	Closes an open png file pointer.
 *
 *	Parameters:
 *		png - png to close.
 *
 *	Returns:
 *		PNG_NO_ERROR
 */

int png_close(png_t *png);

#ifdef __cplusplus
}
#endif

#endif /* _PNGLITE_H_ */
