/*
 * Copyright 2009 Chris Young <chris@unsatisfactorysoftware.co.uk>
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

#ifdef WITH_NS_SVG
#ifndef AMIGA_IFF_DR2D_H
#define AMIGA_IFF_DR2D_H
#include <proto/iffparse.h>
#include <datatypes/pictureclass.h>
#include <stdbool.h>
#ifndef AMIGA_DR2D_STANDALONE
#include "content/content.h"
#include "amiga/download.h"
#endif

#define ID_DR2D MAKE_ID('D','R','2','D')
#define ID_DRHD MAKE_ID('D','R','H','D')
#define ID_ATTR MAKE_ID('A','T','T','R')
#define ID_CPLY MAKE_ID('C','P','L','Y')
#define ID_OPLY MAKE_ID('O','P','L','Y')
#define ID_STXT MAKE_ID('S','T','X','T')
#define ID_DASH MAKE_ID('D','A','S','H')
//#define ID_CMAP MAKE_ID('C','M','A','P')  in dt/pictureclass
//#define ID_NAME MAKE_ID('N','A','M','E')  in dt/datatypes
#define ID_ANNO MAKE_ID('A','N','N','O')
#define ID_FONS MAKE_ID('F','O','N','S')

struct drhd_struct {
	float	XLeft, YTop, XRight, YBot;
};

struct poly_struct {
    USHORT	NumPoints;
//	    float	PolyPoints[]; // 2*numpoints
};

#define INDICATOR  0xFFFFFFFF
#define IND_SPLINE 0x00000001
#define IND_MOVETO 0x00000002
#define IND_CURVE  0x00000001

struct fons_struct {
    UBYTE	FontID;	/* ID the font is referenced by */
    UBYTE	Pad1;          	/* Always 0 */
    UBYTE	Proportional;	/* Is it proportional? */
    UBYTE	Serif;	/* does it have serifs? */
};

struct stxt_struct {
    UBYTE	Pad0;	/* Always 0 (for future expansion) */
    UBYTE	WhichFont;      /* Which font to use */
    float	CharW, CharH,  	/* W/H of an individual char */
	BaseX, BaseY,  	/* Start of baseline */
	Rotation;      	/* Angle of text (in degrees) */
    uint16	NumChars;
    //char	TextChars[NumChars];
};

/* Various fill types */
#define FT_NONE	0    /* No fill	*/
#define FT_COLOR    	1    /* Fill with color from palette */
#define FT_OBJECTS	2    /* Fill with tiled objects	*/

struct attr_struct {
    UBYTE	FillType;    /* One of FT_*, above	*/
    UBYTE	JoinType;    /* One of JT_*, below	*/
    UBYTE	DashPattern; /* ID of edge dash pattern */
    UBYTE	ArrowHead;   /* ID of arrowhead to use  */
    USHORT	FillValue;   /* Color or object with which to fill */
    USHORT	EdgeValue;   /* Edge color index	*/
    USHORT	WhichLayer;  /* ID of layer it's in	*/
    float	EdgeThick;   /* Line width	*/
};

/* Join types */
#define JT_NONE	   0    	/* Don't do line joins */
#define JT_MITER  	1    	/* Mitered join */
#define JT_BEVEL  	2    	/* Beveled join */
#define JT_ROUND  	3    	/* Round join */

struct dash_struct {
    USHORT	DashID;	/* ID of the dash pattern */
    USHORT	NumDashes; 	/* Should always be even */
//	    IEEE	Dashes[NumDashes];  	/* On-off pattern */
};

bool ami_svg_to_dr2d(struct IFFHandle *iffh, const char *buffer,
		uint32 size, const char *url);
#ifndef AMIGA_DR2D_STANDALONE
bool ami_save_svg(struct hlcache_handle *c, char *filename);
#endif
#endif // AMIGA_IFF_DR2D_H
#endif // WITH_NS_SVG
