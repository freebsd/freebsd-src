/*
 * Copyright 2005 Richard Wilson <info@tinct.net>
 *
 * This file is part of NetSurf, http://www.netsurf-browser.org/
 *
 * NetSurf is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * NetSurf is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Complete details on using Tinct are available from http://www.tinct.net.
 */

/** \file
 * Tinct SWI numbers and flags for version 0.11
 */

#ifndef _NETSURF_RISCOS_TINCT_H_
#define _NETSURF_RISCOS_TINCT_H_


/**
 * Plots an alpha-blended sprite at the specified coordinates.
 *
 * ->	R2	Sprite pointer
 *	R3	X coordinate
 *	R4	Y coordinate
 *	R7	Flag word
*/
#define Tinct_PlotAlpha 0x57240


/**
 * Plots a scaled alpha-blended sprite at the specified coordinates.
 *
 * ->	R2	Sprite pointer
 *	R3	X coordinate
 *	R4	Y coordinate
 *	R5	Scaled sprite width
 *	R6	Scaled sprite height
 *	R7	Flag word
 */
#define Tinct_PlotScaledAlpha 0x57241


/**
 * Plots a sprite at the specified coordinates with a constant 0xff value for
 * the alpha channel, ie without a mask.
 *
 * ->	R2	Sprite pointer
 *	R3	X coordinate
 *	R4	Y coordinate
 *	R7	Flag word
 */
#define Tinct_Plot 0x57242

/**
 * Plots a scaled sprite at the specified coordinates with a constant 0xff value
 * for the alpha channel, ie without a mask.
 *
 * ->	R2	Sprite pointer
 *	R3	X coordinate
 *	R4	Y coordinate
 *	R5	Scaled sprite width
 *	R6	Scaled sprite height
 *	R7	Flag word
 */
#define Tinct_PlotScaled 0x57243


/**
 * Converts a paletted sprite into its 32bpp equivalent. Sufficient memory must
 * have previously been allocated for the sprite (44 + width * height * 4).
 * As sprites with 16bpp or 32bpp do not have palettes, conversion cannot be
 * performed on these variants. All sprites must be supplied with a full palette,
 * eg 8bpp must have 256 palette entries.
 *
 * ->	R2	Source sprite pointer
 *	R3	Destination sprite pointer
 */
#define Tinct_ConvertSprite 0x57244


/**
 * Returns the features available to the caller by specifying bits in the flag
 * word. The features available are unique for each mode, although the current
 * version of Tinct supports the same subset of features for all modes.
 *
 * ->	R0	Feature to test for, or 0 for all features
 * <-	R0	Features available
 */
#define Tinct_AvailableFeatures 0x57245


/**
 * Compresses an image using a fast algorithm. Sufficient memory must have been
 * previously allocated for the maximum possible compressed size. This value is
 * equal to 28 + (width * height * 4) * 33 / 32.
 *
 * ->	R0	Source sprite pointer
 *	R2	Output data buffer
 *	R3	Output bytes available
 *	R7	Flag word (currently 0)
 * <-	R0	Size of compressed data
 */
#define Tinct_Compress 0x57246


/**
 * Decompresses an image previously compressed. Sufficient memory must have been
 * previously allocated for the decompressed data (44 + width * height * 4) where
 * width and height are available at +0 and +4 of the compressed data respectively.
 *
 * ->	R0	Input data buffer
 *	R2	Output data buffer
 *	R7	Flag word (currently 0)
 * <-	R0	Size of decompressed data
 */
#define Tinct_Decompress 0x57247


/*	Plotting flags
*/
#define tinct_READ_SCREEN_BASE	  0x01	/** <-- Use when hardware scrolling */
#define tinct_BILINEAR_FILTER	  0x02	/** <-- Perform bi-linear filtering */
#define tinct_DITHER		  0x04	/** <-- Perform dithering */
#define tinct_ERROR_DIFFUSE	  0x08	/** <-- Perform error diffusion */
#define tinct_DITHER_INVERTED	  0x0C	/** <-- Perform dithering with inverted pattern */
#define tinct_FILL_HORIZONTALLY	  0x10	/** <-- Horizontally fill clipping region with image */
#define tinct_FILL_VERTICALLY	  0x20	/** <-- Vertically fill clipping region with image */
#define tinct_FORCE_PALETTE_READ  0x40	/** <-- Use after a palette change when out of the desktop */
#define tinct_USE_OS_SPRITE_OP	  0x80	/** <-- Use when printing */

/*	Compression flags
*/
#define tinct_OPAQUE_IMAGE	  0x01	/** <-- Image is opaque, compress further */

/*	Shifts
*/
#define tinct_BACKGROUND_SHIFT	  0x08

/*	Sprite mode
*/
#define tinct_SPRITE_MODE	  (os_mode)0x301680b5
#endif
