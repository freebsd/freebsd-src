#ifndef SABER
#ifndef lint
static char Xrcsid[] = "$XConsortium: Dvi.c,v 1.9 89/12/10 16:12:25 rws Exp $";
#endif /* lint */
#endif /* SABER */

/*
 * Dvi.c - Dvi display widget
 *
 */

#define XtStrlen(s)	((s) ? strlen(s) : 0)

  /* The following are defined for the reader's convenience.  Any
     Xt..Field macro in this code just refers to some field in
     one of the substructures of the WidgetRec.  */

#include <X11/IntrinsicP.h>
#include <X11/StringDefs.h>
#include <X11/Xmu/Converters.h>
#include <stdio.h>
#include <ctype.h>
#include "DviP.h"

/****************************************************************
 *
 * Full class record constant
 *
 ****************************************************************/

/* Private Data */

static char default_font_map_1[] = "\
TR	-adobe-times-medium-r-normal--*-100-*-*-*-*-iso8859-1\n\
TI	-adobe-times-medium-i-normal--*-100-*-*-*-*-iso8859-1\n\
TB	-adobe-times-bold-r-normal--*-100-*-*-*-*-iso8859-1\n\
TBI	-adobe-times-bold-i-normal--*-100-*-*-*-*-iso8859-1\n\
CR	-adobe-courier-medium-r-normal--*-100-*-*-*-*-iso8859-1\n\
CI	-adobe-courier-medium-o-normal--*-100-*-*-*-*-iso8859-1\n\
CB	-adobe-courier-bold-r-normal--*-100-*-*-*-*-iso8859-1\n\
CBI	-adobe-courier-bold-o-normal--*-100-*-*-*-*-iso8859-1\n\
";
static char default_font_map_2[] = "\
HR	-adobe-helvetica-medium-r-normal--*-100-*-*-*-*-iso8859-1\n\
HI	-adobe-helvetica-medium-o-normal--*-100-*-*-*-*-iso8859-1\n\
HB	-adobe-helvetica-bold-r-normal--*-100-*-*-*-*-iso8859-1\n\
HBI	-adobe-helvetica-bold-o-normal--*-100-*-*-*-*-iso8859-1\n\
";
static char default_font_map_3[] = "\
NR	-adobe-new century schoolbook-medium-r-normal--*-100-*-*-*-*-iso8859-1\n\
NI	-adobe-new century schoolbook-medium-i-normal--*-100-*-*-*-*-iso8859-1\n\
NB	-adobe-new century schoolbook-bold-r-normal--*-100-*-*-*-*-iso8859-1\n\
NBI	-adobe-new century schoolbook-bold-i-normal--*-100-*-*-*-*-iso8859-1\n\
S	-adobe-symbol-medium-r-normal--*-100-*-*-*-*-adobe-fontspecific\n\
SS	-adobe-symbol-medium-r-normal--*-100-*-*-*-*-adobe-fontspecific\n\
";

#define offset(field) XtOffset(DviWidget, field)

#define MY_WIDTH(dw) ((int)(dw->dvi.paperwidth * dw->dvi.scale_factor + .5))
#define MY_HEIGHT(dw) ((int)(dw->dvi.paperlength * dw->dvi.scale_factor + .5))

static XtResource resources[] = { 
	{(String)XtNfontMap, (String)XtCFontMap, (String)XtRString,
	 sizeof (char *), offset(dvi.font_map_string),
	 (String)XtRString, NULL /* set in code */},
	{(String)XtNforeground, (String)XtCForeground, (String)XtRPixel,
	 sizeof (unsigned long), offset(dvi.foreground),
	 (String)XtRString, (XtPointer)"XtDefaultForeground"},
	{(String)XtNbackground, (String)XtCBackground, (String)XtRPixel,
	 sizeof (unsigned long), offset(dvi.background),
	 (String)XtRString, (XtPointer)"XtDefaultBackground"},
	{(String)XtNpageNumber, (String)XtCPageNumber, (String)XtRInt,
	 sizeof (int), offset(dvi.requested_page),
	 (String)XtRString, (XtPointer)"1"},
	{(String)XtNlastPageNumber, (String)XtCLastPageNumber, (String)XtRInt,
	 sizeof (int), offset (dvi.last_page),
	 (String)XtRString, (XtPointer)"0"},
	{(String)XtNfile, (String)XtCFile, (String)XtRFile,
	 sizeof (FILE *), offset (dvi.file),
	 (String)XtRFile, (XtPointer)0},
	{(String)XtNseek, (String)XtCSeek, (String)XtRBoolean,
	 sizeof (Boolean), offset(dvi.seek),
	 (String)XtRString, (XtPointer)"false"},
	{(String)XtNfont, (String)XtCFont, (String)XtRFontStruct,
	 sizeof (XFontStruct *), offset(dvi.default_font),
	 (String)XtRString, (XtPointer)"xtdefaultfont"},
	{(String)XtNbackingStore, (String)XtCBackingStore, (String)XtRBackingStore,
	 sizeof (int), offset(dvi.backing_store),
	 (String)XtRString, (XtPointer)"default"},
	{(String)XtNnoPolyText, (String)XtCNoPolyText, (String)XtRBoolean,
	 sizeof (Boolean), offset(dvi.noPolyText),
	 (String)XtRString, (XtPointer)"false"},
	{(String)XtNresolution, (String)XtCResolution, (String)XtRInt,
	 sizeof(int), offset(dvi.default_resolution),
	 (String)XtRString, (XtPointer)"75"},
};

#undef offset

static void		ClassInitialize (void);
static void		ClassPartInitialize(WidgetClass);
static void		Initialize(Widget, Widget, ArgList, Cardinal *);
static void		Realize (Widget, XtValueMask *, XSetWindowAttributes *);
static void		Destroy (Widget);
static void		Redisplay (Widget, XEvent *, Region);
static Boolean		SetValues (Widget, Widget, Widget,
				   ArgList, Cardinal *);
static Boolean		SetValuesHook (Widget, ArgList, Cardinal *);
static XtGeometryResult	QueryGeometry (Widget, XtWidgetGeometry *,
				       XtWidgetGeometry *);
static void		ShowDvi (DviWidget);
static void		CloseFile (DviWidget);
static void		OpenFile (DviWidget);
static void		FindPage (DviWidget);

static void		SaveToFile (Widget, FILE *);

/* font.c */
extern void ParseFontMap(DviWidget);
extern void DestroyFontMap(DviFontMap *);
extern void ForgetFonts(DviWidget);

/* page.c */
extern void DestroyFileMap(DviFileMap *);
extern long SearchPagePosition(DviWidget, int);
extern void FileSeek(DviWidget, long);
extern void ForgetPagePositions(DviWidget);

/* parse.c */
extern int ParseInput(register DviWidget);

DviClassRec dviClassRec = {
{
	&widgetClassRec,		/* superclass		  */	
	(String)"Dvi",			/* class_name		  */
	sizeof(DviRec),			/* size			  */
	ClassInitialize,		/* class_initialize	  */
	ClassPartInitialize,		/* class_part_initialize  */
	FALSE,				/* class_inited		  */
	Initialize,			/* initialize		  */
	NULL,				/* initialize_hook	  */
	Realize,			/* realize		  */
	NULL,				/* actions		  */
	0,				/* num_actions		  */
	resources,			/* resources		  */
	XtNumber(resources),		/* resource_count	  */
	NULLQUARK,			/* xrm_class		  */
	FALSE,				/* compress_motion	  */
	TRUE,				/* compress_exposure	  */
	TRUE,				/* compress_enterleave    */
	FALSE,				/* visible_interest	  */
	Destroy,			/* destroy		  */
	NULL,				/* resize		  */
	Redisplay,			/* expose		  */
	SetValues,			/* set_values		  */
	SetValuesHook,			/* set_values_hook	  */
	NULL,				/* set_values_almost	  */
	NULL,				/* get_values_hook	  */
	NULL,				/* accept_focus		  */
	XtVersion,			/* version		  */
	NULL,				/* callback_private	  */
	0,				/* tm_table		  */
	QueryGeometry,			/* query_geometry	  */
	NULL,				/* display_accelerator	  */
	NULL				/* extension		  */
},{
	SaveToFile,			/* save    */
},
};

WidgetClass dviWidgetClass = (WidgetClass) &dviClassRec;

static void ClassInitialize (void)
{
	int len1 = strlen(default_font_map_1);
	int len2 = strlen(default_font_map_2);
	int len3 = strlen(default_font_map_3);
	char *dfm = XtMalloc(len1 + len2 + len3 + 1);
	char *ptr = dfm;
	strcpy(ptr, default_font_map_1); ptr += len1;
	strcpy(ptr, default_font_map_2); ptr += len2;
	strcpy(ptr, default_font_map_3);
	resources[0].default_addr = dfm;

	XtAddConverter( XtRString, XtRBackingStore, XmuCvtStringToBackingStore,
			NULL, 0 );
}

/****************************************************************
 *
 * Private Procedures
 *
 ****************************************************************/

/* ARGSUSED */
static void Initialize(Widget request, Widget new_wd,
		       ArgList args, Cardinal *num_args)
{
	DviWidget	dw = (DviWidget) new_wd;

	dw->dvi.current_page = 0;
	dw->dvi.font_map = 0;
	dw->dvi.cache.index = 0;
	dw->dvi.text_x_width = 0;
	dw->dvi.text_device_width = 0;
	dw->dvi.word_flag = 0;
	dw->dvi.file = 0;
	dw->dvi.tmpFile = 0;
	dw->dvi.state = 0;
	dw->dvi.readingTmp = 0;
	dw->dvi.cache.char_index = 0;
	dw->dvi.cache.font_size = -1;
	dw->dvi.cache.font_number = -1;
	dw->dvi.cache.adjustable[0] = 0;
	dw->dvi.file_map = 0;
	dw->dvi.fonts = 0;
	dw->dvi.seek = False;
	dw->dvi.device_resolution = dw->dvi.default_resolution;
	dw->dvi.display_resolution = dw->dvi.default_resolution;
	dw->dvi.paperlength = dw->dvi.default_resolution*11;
	dw->dvi.paperwidth = (dw->dvi.default_resolution*8
			      + dw->dvi.default_resolution/2);
	dw->dvi.scale_factor = 1.0;
	dw->dvi.sizescale = 1;
	dw->dvi.line_thickness = -1;
	dw->dvi.line_width = 1;
	dw->dvi.fill = DVI_FILL_MAX;
	dw->dvi.device_font = 0;
	dw->dvi.device_font_number = -1;
	dw->dvi.device = 0;
	dw->dvi.native = 0;

	request = request;	/* unused; suppress compiler warning */
	args = args;
	num_args = num_args;
}

#include "gray1.bm"
#include "gray2.bm"
#include "gray3.bm"
#include "gray4.bm"
#include "gray5.bm"
#include "gray6.bm"
#include "gray7.bm"
#include "gray8.bm"

static void
Realize (Widget w, XtValueMask *valueMask, XSetWindowAttributes *attrs)
{
	DviWidget	dw = (DviWidget) w;
	XGCValues	values;

	if (dw->dvi.backing_store != Always + WhenMapped + NotUseful) {
		attrs->backing_store = dw->dvi.backing_store;
		*valueMask |= CWBackingStore;
	}
	XtCreateWindow (w, (unsigned)InputOutput, (Visual *) CopyFromParent,
			*valueMask, attrs);
	values.foreground = dw->dvi.foreground;
	values.cap_style = CapRound;
	values.join_style = JoinRound;
	values.line_width = dw->dvi.line_width;
	dw->dvi.normal_GC = XCreateGC (XtDisplay (w), XtWindow (w),
				       GCForeground|GCCapStyle|GCJoinStyle
				       |GCLineWidth,
				       &values);
	dw->dvi.gray[0] = XCreateBitmapFromData(XtDisplay (w), XtWindow (w),
					     gray1_bits,
					     gray1_width, gray1_height);
	dw->dvi.gray[1] = XCreateBitmapFromData(XtDisplay (w), XtWindow (w),
					     gray2_bits,
					     gray2_width, gray2_height);
	dw->dvi.gray[2] = XCreateBitmapFromData(XtDisplay (w), XtWindow (w),
					     gray3_bits,
					     gray3_width, gray3_height);
	dw->dvi.gray[3] = XCreateBitmapFromData(XtDisplay (w), XtWindow (w),
					     gray4_bits,
					     gray4_width, gray4_height);
	dw->dvi.gray[4] = XCreateBitmapFromData(XtDisplay (w), XtWindow (w),
					     gray5_bits,
					     gray5_width, gray5_height);
	dw->dvi.gray[5] = XCreateBitmapFromData(XtDisplay (w), XtWindow (w),
					     gray6_bits,
					     gray6_width, gray6_height);
	dw->dvi.gray[6] = XCreateBitmapFromData(XtDisplay (w), XtWindow (w),
					     gray7_bits,
					     gray7_width, gray7_height);
	dw->dvi.gray[7] = XCreateBitmapFromData(XtDisplay (w), XtWindow (w),
					     gray8_bits,
					     gray8_width, gray8_height);
	values.background = dw->dvi.background;
	values.stipple = dw->dvi.gray[5];
	dw->dvi.fill_GC = XCreateGC (XtDisplay (w), XtWindow (w),
				     GCForeground|GCBackground|GCStipple,
				     &values);

	dw->dvi.fill_type = 9;

	if (dw->dvi.file)
		OpenFile (dw);
	ParseFontMap (dw);
}

static void
Destroy(Widget w)
{
	DviWidget	dw = (DviWidget) w;

	XFreeGC (XtDisplay (w), dw->dvi.normal_GC);
	XFreeGC (XtDisplay (w), dw->dvi.fill_GC);
	XFreePixmap (XtDisplay (w), dw->dvi.gray[0]);
	XFreePixmap (XtDisplay (w), dw->dvi.gray[1]);
	XFreePixmap (XtDisplay (w), dw->dvi.gray[2]);
	XFreePixmap (XtDisplay (w), dw->dvi.gray[3]);
	XFreePixmap (XtDisplay (w), dw->dvi.gray[4]);
	XFreePixmap (XtDisplay (w), dw->dvi.gray[5]);
	XFreePixmap (XtDisplay (w), dw->dvi.gray[6]);
	XFreePixmap (XtDisplay (w), dw->dvi.gray[7]);
	DestroyFontMap (dw->dvi.font_map);
	DestroyFileMap (dw->dvi.file_map);
	device_destroy (dw->dvi.device);
}

/*
 * Repaint the widget window
 */

/* ARGSUSED */
static void
Redisplay(Widget w, XEvent *event, Region region)
{
	DviWidget	dw = (DviWidget) w;
	XRectangle	extents;
	
	XClipBox (region, &extents);
	dw->dvi.extents.x1 = extents.x;
	dw->dvi.extents.y1 = extents.y;
	dw->dvi.extents.x2 = extents.x + extents.width;
	dw->dvi.extents.y2 = extents.y + extents.height;
	ShowDvi (dw);

	event = event;		/* unused; suppress compiler warning */
}

/*
 * Set specified arguments into widget
 */
/* ARGSUSED */
static Boolean
SetValues (Widget wcurrent, Widget wrequest, Widget wnew,
	   ArgList args, Cardinal *num_args)
{
	Boolean		redisplay = FALSE;
	char		*new_map;
	int		cur, req;
	DviWidget	current = (DviWidget)wcurrent;
	DviWidget	request = (DviWidget)wrequest;
	DviWidget	new_wd = (DviWidget)wnew;

	if (current->dvi.font_map_string != request->dvi.font_map_string) {
		new_map = XtMalloc (strlen (request->dvi.font_map_string) + 1);
		if (new_map) {
			redisplay = TRUE;
			strcpy (new_map, request->dvi.font_map_string);
			new_wd->dvi.font_map_string = new_map;
			if (current->dvi.font_map_string)
				XtFree (current->dvi.font_map_string);
			current->dvi.font_map_string = 0;
			ParseFontMap (new_wd);
		}
	}

	req = request->dvi.requested_page;
	cur = current->dvi.requested_page;
	if (cur != req) {
		if (!request->dvi.file)
		    req = 0;
		else {
		    if (req < 1)
			    req = 1;
		    if (current->dvi.last_page != 0 &&
			req > current->dvi.last_page)
			    req = current->dvi.last_page;
		}
		if (cur != req)
	    	    redisplay = TRUE;
		new_wd->dvi.requested_page = req;
		if (current->dvi.last_page == 0 && req > cur)
			FindPage (new_wd);
	}

	args = args;		/* unused; suppress compiler warning */
	num_args = num_args;

	return redisplay;
}

/*
 * use the set_values_hook entry to check when
 * the file is set
 */

static Boolean
SetValuesHook (Widget wdw, ArgList args, Cardinal *num_argsp)
{
	Cardinal	i;
	DviWidget	dw = (DviWidget)wdw;

	for (i = 0; i < *num_argsp; i++) {
		if (!strcmp (args[i].name, XtNfile)) {
			CloseFile (dw);
			OpenFile (dw);
			return TRUE;
		}
	}
	return FALSE;
}

static void CloseFile (DviWidget dw)
{
	if (dw->dvi.tmpFile)
		fclose (dw->dvi.tmpFile);
	ForgetPagePositions (dw);
}

static void OpenFile (DviWidget dw)
{
	dw->dvi.tmpFile = 0;
	if (!dw->dvi.seek)
		dw->dvi.tmpFile = tmpfile();
	dw->dvi.requested_page = 1;
	dw->dvi.last_page = 0;
}

static XtGeometryResult
QueryGeometry (Widget w, XtWidgetGeometry *request,
	       XtWidgetGeometry *geometry_return)
{
	XtGeometryResult	ret;
	DviWidget		dw = (DviWidget) w;

	ret = XtGeometryYes;
	if (((request->request_mode & CWWidth)
	     && request->width < MY_WIDTH(dw))
	    || ((request->request_mode & CWHeight)
		&& request->height < MY_HEIGHT(dw)))
		ret = XtGeometryAlmost;
	geometry_return->width = MY_WIDTH(dw);
	geometry_return->height = MY_HEIGHT(dw);
	geometry_return->request_mode = CWWidth|CWHeight;
	return ret;
}

void
SetDevice (DviWidget dw, const char *name)
{
	XtWidgetGeometry	request, reply;
	XtGeometryResult ret;

	ForgetFonts (dw);
	dw->dvi.device = device_load (name);
	if (!dw->dvi.device)
		return;
	dw->dvi.sizescale = dw->dvi.device->sizescale;
	dw->dvi.device_resolution = dw->dvi.device->res;
	dw->dvi.native = dw->dvi.device->X11;
	dw->dvi.paperlength = dw->dvi.device->paperlength;
	dw->dvi.paperwidth = dw->dvi.device->paperwidth;
	if (dw->dvi.native) {
		dw->dvi.display_resolution = dw->dvi.device_resolution;
		dw->dvi.scale_factor = 1.0;
	}
	else {
		dw->dvi.display_resolution = dw->dvi.default_resolution;
		dw->dvi.scale_factor = ((double)dw->dvi.display_resolution
					/ dw->dvi.device_resolution);
	}
	request.request_mode = CWWidth|CWHeight;
	request.width = MY_WIDTH(dw);
	request.height = MY_HEIGHT(dw);
	ret = XtMakeGeometryRequest ((Widget)dw, &request, &reply);
	if (ret == XtGeometryAlmost
	    && reply.height >= request.height
	    && reply.width >= request.width) {
		request.width = reply.width;
		request.height = reply.height;
		XtMakeGeometryRequest ((Widget)dw, &request, &reply);
	}
}

static void
ShowDvi (DviWidget dw)
{
	if (!dw->dvi.file) {
		static char Error[] = "No file selected";

		XSetFont (XtDisplay(dw), dw->dvi.normal_GC,
			  dw->dvi.default_font->fid);
		XDrawString (XtDisplay (dw), XtWindow (dw), dw->dvi.normal_GC,
			     20, 20, Error, strlen (Error));
		return;
	}

	FindPage (dw);
	
	dw->dvi.display_enable = 1;
	ParseInput (dw);
	if (dw->dvi.last_page && dw->dvi.requested_page > dw->dvi.last_page)
		dw->dvi.requested_page = dw->dvi.last_page;
}

static void
FindPage (DviWidget dw)
{
	int	i;
	long	file_position;

	if (dw->dvi.requested_page < 1)
		dw->dvi.requested_page = 1;

	if (dw->dvi.last_page != 0 && dw->dvi.requested_page > dw->dvi.last_page)
		dw->dvi.requested_page = dw->dvi.last_page;

	file_position = SearchPagePosition (dw, dw->dvi.requested_page);
	if (file_position != -1) {
		FileSeek(dw, file_position);
		dw->dvi.current_page = dw->dvi.requested_page;
	} else {
		for (i=dw->dvi.requested_page; i > 0; i--) {
			file_position = SearchPagePosition (dw, i);
			if (file_position != -1)
				break;
		}
		if (file_position == -1)
			file_position = 0;
		FileSeek (dw, file_position);

		dw->dvi.current_page = i;
		
		dw->dvi.display_enable = 0;
		while (dw->dvi.current_page != dw->dvi.requested_page) {
			dw->dvi.current_page = ParseInput (dw);
			/*
			 * at EOF, seek back to the beginning of this page.
			 */
			if (!dw->dvi.readingTmp && feof (dw->dvi.file)) {
				file_position = SearchPagePosition (dw,
						dw->dvi.current_page);
				if (file_position != -1)
					FileSeek (dw, file_position);
				dw->dvi.requested_page = dw->dvi.current_page;
				break;
			}
		}
	}
}

void DviSaveToFile(Widget w, FILE *fp)
{
	XtCheckSubclass(w, dviWidgetClass, NULL);
	(*((DviWidgetClass) XtClass(w))->command_class.save)(w, fp);
}

static
void SaveToFile(Widget w, FILE *fp)
{
	DviWidget dw = (DviWidget)w;
	long pos;
	int c;

	if (dw->dvi.tmpFile) {
		pos = ftell(dw->dvi.tmpFile);
		if (dw->dvi.ungot) {
			pos--;
			dw->dvi.ungot = 0;
			/* The ungot character is in the tmpFile, so we don't
			   want to read it from file. */
			(void)getc(dw->dvi.file);
		}
	}
	else
		pos = ftell(dw->dvi.file);
	FileSeek(dw, 0L);
	while (DviGetC(dw, &c) != EOF)
		if (putc(c, fp) == EOF) {
			/* XXX print error message */
			break;
		}
	FileSeek(dw, pos);
}

static
void ClassPartInitialize(WidgetClass widget_class)
{
	DviWidgetClass wc = (DviWidgetClass)widget_class;
	DviWidgetClass super = (DviWidgetClass) wc->core_class.superclass;
	if (wc->command_class.save == InheritSaveToFile)
		wc->command_class.save = super->command_class.save;
}
	
/*
Local Variables:
c-indent-level: 8
c-continued-statement-offset: 8
c-brace-offset: -8
c-argdecl-indent: 8
c-label-offset: -8
c-tab-always-indent: nil
End:
*/
