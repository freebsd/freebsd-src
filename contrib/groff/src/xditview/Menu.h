/*
 * $XConsortium: Menu.h,v 1.2 89/07/21 14:22:10 jim Exp $
 */

#ifndef _XtMenu_h
#define _XtMenu_h

/***********************************************************************
 *
 * Menu Widget
 *
 ***********************************************************************/

/* Parameters:

 Name		     Class		RepType		Default Value
 ----		     -----		-------		-------------
 background	     Background		pixel		White
 border		     BorderColor	pixel		Black
 borderWidth	     BorderWidth	int		1
 height		     Height		int		120
 mappedWhenManaged   MappedWhenManaged	Boolean		True
 reverseVideo	     ReverseVideo	Boolean		False
 width		     Width		int		120
 x		     Position		int		0
 y		     Position		int		0

*/

#define XtNmenuEntries		"menuEntries"
#define XtNhorizontalPadding	"horizontalPadding"
#define XtNverticalPadding	"verticalPadding"
#define XtNselection		"Selection"

#define XtCMenuEntries		"MenuEntries"
#define XtCPadding		"Padding"
#define XtCSelection		"Selection"

typedef struct _MenuRec *MenuWidget;  /* completely defined in MenuPrivate.h */
typedef struct _MenuClassRec *MenuWidgetClass;    /* completely defined in MenuPrivate.h */

extern WidgetClass menuWidgetClass;

extern Widget	XawMenuCreate ();
#endif /* _XtMenu_h */
/* DON'T ADD STUFF AFTER THIS #endif */
