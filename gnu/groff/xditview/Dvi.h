/*
* $XConsortium: Dvi.h,v 1.4 89/07/21 14:22:06 jim Exp $
*/

#ifndef _XtDvi_h
#define _XtDvi_h

/***********************************************************************
 *
 * Dvi Widget
 *
 ***********************************************************************/

/* Parameters:

 Name		     Class		RepType		Default Value
 ----		     -----		-------		-------------
 background	     Background		pixel		White
 foreground	     Foreground		Pixel		Black
 fontMap	     FontMap		char *		...
 pageNumber	     PageNumber		int		1
*/

#define XtNfontMap	"fontMap"
#define XtNpageNumber	"pageNumber"
#define XtNlastPageNumber   "lastPageNumber"
#define XtNnoPolyText	"noPolyText"
#define XtNseek		"seek"
#define XtNresolution	"resolution"

#define XtCFontMap	"FontMap"
#define XtCPageNumber	"PageNumber"
#define XtCLastPageNumber   "LastPageNumber"
#define XtCNoPolyText	"NoPolyText"
#define XtCSeek		"Seek"
#define XtCResolution	"Resolution"

typedef struct _DviRec *DviWidget;  /* completely defined in DviPrivate.h */
typedef struct _DviClassRec *DviWidgetClass;    /* completely defined in DviPrivate.h */

extern WidgetClass dviWidgetClass;

extern void DviSaveToFile();

#endif /* _XtDvi_h */
/* DON'T ADD STUFF AFTER THIS #endif */
