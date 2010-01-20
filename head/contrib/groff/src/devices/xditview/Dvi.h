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

#define XtNfontMap	(String)"fontMap"
#define XtNpageNumber	(String)"pageNumber"
#define XtNlastPageNumber   (String)"lastPageNumber"
#define XtNnoPolyText	(String)"noPolyText"
#define XtNseek		(String)"seek"
#define XtNresolution	(String)"resolution"

#define XtCFontMap	(String)"FontMap"
#define XtCPageNumber	(String)"PageNumber"
#define XtCLastPageNumber   (String)"LastPageNumber"
#define XtCNoPolyText	(String)"NoPolyText"
#define XtCSeek		(String)"Seek"
#define XtCResolution	(String)"Resolution"

typedef struct _DviRec *DviWidget;  /* completely defined in DviP.h */
typedef struct _DviClassRec *DviWidgetClass;    /* completely defined in DviP.h */

extern WidgetClass dviWidgetClass;

void DviSaveToFile(Widget, FILE *);

#endif /* _XtDvi_h */
/* DON'T ADD STUFF AFTER THIS #endif */
