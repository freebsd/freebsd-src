/*
 * $XConsortium: DviP.h,v 1.5 89/07/22 19:44:08 keith Exp $
 */

/* 
 * DviP.h - Private definitions for Dvi widget
 */

#ifndef _XtDviP_h
#define _XtDviP_h

#include "Dvi.h"
#include "DviChar.h"
#include "device.h"

/***********************************************************************
 *
 * Dvi Widget Private Data
 *
 ***********************************************************************/

/************************************
 *
 *  Class structure
 *
 ***********************************/

/* Type for save method. */

typedef void (*DviSaveProc)();

/*
 * New fields for the Dvi widget class record
 */


typedef struct _DviClass {
      DviSaveProc	save;
} DviClassPart;

/*
 * Full class record declaration
 */

typedef struct _DviClassRec {
	CoreClassPart	core_class;
	DviClassPart	command_class;
} DviClassRec;

extern DviClassRec dviClassRec;

/***************************************
 *
 *  Instance (widget) structure 
 *
 **************************************/

/*
 * a list of fonts we've used for this widget
 */

typedef struct _dviFontSizeList {
	struct _dviFontSizeList	*next;
	int			size;
	char			*x_name;
	XFontStruct		*font;
	int			doesnt_exist;
} DviFontSizeList;

typedef struct _dviFontList {
	struct _dviFontList	*next;
	char			*dvi_name;
	char			*x_name;
	int			dvi_number;
	Boolean			initialized;
	Boolean			scalable;
	DviFontSizeList		*sizes;
	DviCharNameMap		*char_map;
	DeviceFont		*device_font;
} DviFontList;

typedef struct _dviFontMap {
	struct _dviFontMap	*next;
	char			*dvi_name;
	char			*x_name;
} DviFontMap;

#define DVI_TEXT_CACHE_SIZE	256
#define DVI_CHAR_CACHE_SIZE	1024

typedef struct _dviCharCache {
	XTextItem	cache[DVI_TEXT_CACHE_SIZE];
	char		adjustable[DVI_TEXT_CACHE_SIZE];
	char		char_cache[DVI_CHAR_CACHE_SIZE];
	int		index;
	int		max;
	int		char_index;
	int		font_size;
	int		font_number;
	XFontStruct	*font;
	int		start_x, start_y;
	int		x, y;
} DviCharCache;

typedef struct _dviState {
	struct _dviState	*next;
	int			font_size;
	int			font_number;
	int			x;
	int			y;
} DviState;

typedef struct _dviFileMap {
	struct _dviFileMap	*next;
	long			position;
	int			page_number;
} DviFileMap;

/*
 * New fields for the Dvi widget record
 */

typedef struct {
	/*
	 * resource specifiable items
	 */
	char		*font_map_string;
	unsigned long	foreground;
	unsigned long	background;
	int		requested_page;
	int		last_page;
	XFontStruct	*default_font;
	FILE		*file;
	Boolean		noPolyText;
	Boolean		seek;		/* file is "seekable" */
	int		default_resolution;
	/*
 	 * private state
 	 */
	FILE		*tmpFile;	/* used when reading stdin */
	char		readingTmp;	/* reading now from tmp */
	char		ungot;		/* have ungetc'd a char */
	GC		normal_GC;
	GC		fill_GC;
	DviFileMap	*file_map;
	DviFontList	*fonts;
	DviFontMap	*font_map;
	int		current_page;
	int		font_size;
	int		font_number;
	DeviceFont	*device_font;
	int		device_font_number;
	Device		*device;
	int		native;
	int		device_resolution;
	int		display_resolution;
	int		paperlength;
	int		paperwidth;
	double		scale_factor;	/* display res / device res */
	int		sizescale;
	int		line_thickness;
	int		line_width;

#define DVI_FILL_MAX 1000

	int		fill;
#define DVI_FILL_WHITE 0
#define DVI_FILL_GRAY 1
#define DVI_FILL_BLACK 2
	int		fill_type;
	Pixmap		gray;
	int		backing_store;
	XFontStruct	*font;
	int		display_enable;
	struct ExposedExtents {
	    int x1, y1, x2, y2;
	}		extents;
	DviState	*state;
	DviCharCache	cache;
	int		text_x_width;
	int		text_device_width;
	int		word_flag;
} DviPart;

#define DviGetIn(dw,cp)\
    (dw->dvi.tmpFile ? (\
	DviGetAndPut (dw, cp) \
    ) :\
	(*cp = getc (dw->dvi.file))\
)

#define DviGetC(dw, cp)\
    (dw->dvi.readingTmp ? (\
	((*cp = getc (dw->dvi.tmpFile)) == EOF) ? (\
	    fseek (dw->dvi.tmpFile, 0l, 2),\
	    (dw->dvi.readingTmp = 0),\
	    DviGetIn (dw,cp)\
	) : (\
	    *cp\
	)\
    ) : (\
	DviGetIn(dw,cp)\
    )\
)

#define DviUngetC(dw, c)\
    (dw->dvi.readingTmp ? (\
	ungetc (c, dw->dvi.tmpFile)\
    ) : ( \
	(dw->dvi.ungot = 1),\
	ungetc (c, dw->dvi.file)))

/*
 * Full widget declaration
 */

typedef struct _DviRec {
	CorePart	core;
	DviPart		dvi;
} DviRec;

#define InheritSaveToFile ((DviSaveProc)_XtInherit)

extern XFontStruct	*QueryFont ();

extern DviCharNameMap	*QueryFontMap ();

extern DeviceFont	*QueryDeviceFont ();

extern char *GetWord(), *GetLine();
#endif /* _XtDviP_h */


