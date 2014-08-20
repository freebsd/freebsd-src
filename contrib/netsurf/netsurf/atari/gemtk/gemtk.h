#ifndef GEMTK_H_INCLUDED
#define GEMTK_H_INCLUDED

#include <stdint.h>
#include <stdbool.h>

#include <mint/osbind.h>
#include <mint/cookie.h>

#include <gem.h>
#include <cflib.h>


/* -------------------------------------------------------------------------- */
/* SYSTEM UTILS                                                               */
/* -------------------------------------------------------------------------- */

/* System type detection added by [GS]  */
/* detect the system type, AES + kernel */
#define SYS_TOS    0x0001
#define SYS_MAGIC  0x0002
#define SYS_MINT   0x0004
#define SYS_GENEVA 0x0010
#define SYS_NAES   0x0020
#define SYS_XAAES  0x0040
#define sys_type()    (_systype_v ? _systype_v : _systype())
#define sys_MAGIC()   ((sys_type() & SYS_MAGIC) != 0)
#define sys_NAES()    ((sys_type() & SYS_NAES)  != 0)
#define sys_XAAES()   ((sys_type() & SYS_XAAES) != 0)

#define TOS4VER 0x03300 /* this is assumed to be the last single tasking OS */

extern unsigned short _systype_v;
unsigned short _systype (void);

/* GEMTK Utils API: */

#define GEMTK_DBG_GRECT(s,g) 				\
	printf("%s", s);							\
	printf("\tx0: %d, \n", (g)->g_x); 			\
	printf("\ty0: %d, \n", (g)->g_y);			\
	printf("\tx1: %d, \n", (g)->g_x+(g)->g_w); 	\
	printf("\ty1: %d, \n", (g)->g_y+(g)->g_h);		\
	printf("\tw:  %d, \n", (g)->g_w);			\
	printf("\th:  %d  \n", (g)->g_h);			\

/*
* Chech for GRECT intersection without modifiend the src rectangles
* return true when the  GRECT's intersect, fals otherwise.
*/
bool gemtk_rc_intersect_ro(GRECT *a, GRECT *b);

/*
* Convert keycode returned by evnt_multi to ascii value
*/
int gemtk_keybd2ascii( int keybd, int shift);

/** set VDI clip area by passing an GRECT */
void gemtk_clip_grect(VdiHdl vh, GRECT *rect);

void gemtk_wind_get_str(short aes_handle, short mode, char *str, int len);

/* send application message */
void gemtk_send_msg(short msg_type, short data2, short data3, short data4,
                     short data5, short data6, short data7);


#ifndef POINT_WITHIN
# define POINT_WITHIN(_x,_y, r) ((_x >= r.g_x) && (_x <= r.g_x + r.g_w ) \
		&& (_y >= r.g_y) && (_y <= r.g_y + r.g_h))
#endif

#ifndef RC_WITHIN
# define RC_WITHIN(a,b) \
    (((a)->g_x >= (b)->g_x) \
        && (((a)->g_x + (a)->g_w) <= ((b)->g_x + (b)->g_w))) \
            && (((a)->g_y >= (b)->g_y) \
                && (((a)->g_y + (a)->g_h) <= ((b)->g_y + (b)->g_h)))
#endif

#ifndef MAX
# define MAX(_a,_b) ((_a>_b) ? _a : _b)
#endif

#ifndef MIN
# define MIN(_a,_b) ((_a<_b) ? _a : _b)
#endif

#ifndef SET_BIT
# define SET_BIT(field,bit,val) field = (val)?((field)|(bit)):((field) & ~(bit))
#endif

/* -------------------------------------------------------------------------- */
/* MultiTOS Drag & Drop                                                       */
/* -------------------------------------------------------------------------- */
short gemtk_dd_create(short *pipe);
short gemtk_dd_message(short apid, short fd, short winid, short mx, short my, short kstate, short pipename);
short gemtk_dd_rexts(short fd, char *exts);
short gemtk_dd_stry(short fd, char *ext, char *text, char *name, long size);
void gemtk_dd_close(short fd);
void gemtk_dd_getsig(long *oldsig);
void gemtk_dd_setsig(long oldsig);
short gemtk_dd_open(short ddnam, char ddmsg);
short gemtk_dd_sexts(short fd, char *exts);
short gemtk_dd_rtry(short fd, char *name, char *file, char *whichext, long *size);
short gemtk_dd_reply(short fd, char ack);

/* -------------------------------------------------------------------------- */
/* AV/VA Protocol Module                                                      */
/* -------------------------------------------------------------------------- */
int gemtk_av_init(const char *appname);
void gemtk_av_exit(void);
bool gemtk_av_send (short message, const char * data1, const char * data2);
bool gemtk_av_dispatch (short msg[8]);

/* -------------------------------------------------------------------------- */
/* Message Box module                                                         */
/* -------------------------------------------------------------------------- */
#define GEMTK_MSG_BOX_ALERT	1
#define GEMTK_MSG_BOX_CONFIRM	2

short gemtk_msg_box_show(short type, const char * msg);

/* -------------------------------------------------------------------------- */
/* GUIWIN Module                                                              */
/* -------------------------------------------------------------------------- */
#define GEMTK_WM_FLAG_PREPROC_WM		0x01	// let guiwin API handle some events
#define GEMTK_WM_FLAG_RECV_PREPROC_WM	0x02	// get notified even when pre-processed
#define GEMTK_WM_FLAG_HAS_VTOOLBAR		0x04	// the attached toolbar is vertical
#define GEMTK_WM_FLAG_CUSTOM_TOOLBAR	0x08	// no internal toolbar handling
												// (Except considering it's size)
#define GEMTK_WM_FLAG_CUSTOM_SCROLLING	0x20	// no internal scroller handling

#define GEMTK_WM_FLAG_DEFAULTS  \
	(GEMTK_WM_FLAG_PREPROC_WM | GEMTK_WM_FLAG_RECV_PREPROC_WM)

#define GEMTK_WM_STATUS_ICONIFIED		0x01
#define GEMTK_WM_STATUS_SHADED			0x02

#define GEMTK_WM_VSLIDER 				0x01
#define GEMTK_WM_HSLIDER 				0x02
#define GEMTK_WM_VH_SLIDER 				0x03

/*
	Message sent to the client application when an AES object is
	clicked in an window which contains an form.

	Message Parameters:
	msg[4] = Clicked Object.
	msg[5] = Number of clicks.
	msg[6] = Modifier keys.
*/
#define GEMTK_WM_WM_FORM_CLICK			1001
#define GEMTK_WM_WM_FORM_KEY			1002

struct gemtk_window_s;

/** list struct for managing AES windows */
typedef struct gemtk_window_s GUIWIN;

/** GUIWIN event handler */
typedef short (*gemtk_wm_event_handler_f)(GUIWIN *gw,
		EVMULT_OUT *ev_out, short msg[8]);

typedef void (*gemtk_wm_redraw_f)(GUIWIN *win, uint16_t msg, GRECT *clip);

struct gemtk_wm_scroll_info_s {

	/** Definition of a content unit (horizontal) measured in pixel  */
	int x_unit_px;

	/** Definition of content unit (vertical) measured in pixel */
	int y_unit_px;

	/** Current scroll position (in content units) */
	int x_pos;

	/** Current scroll position (in content units) */
	int y_pos;

	/** Size of content (horizontal) measured in content units */
	int x_units;

	/** Size of content (vertical) measured in content units */
	int y_units;
};

/** Well known areas inside the window */
enum guwin_area_e {
	GEMTK_WM_AREA_WORK = 0,
	GEMTK_WM_AREA_TOOLBAR,
	GEMTK_WM_AREA_CONTENT
};

/* -------------------------------------------------------------------------- */
/* GUIWIN functions (document in guiwin.c)                                    */
/* -------------------------------------------------------------------------- */

short
gemtk_wm_init(void);

void gemtk_wm_exit(void);

GUIWIN * gemtk_wm_add(short handle, uint32_t flags,
		gemtk_wm_event_handler_f handler);

GUIWIN * gemtk_wm_find(short handle);

void gemtk_wm_dump_window_info(GUIWIN *win);

short gemtk_wm_remove(GUIWIN *win);

GUIWIN * gemtk_wm_validate_ptr(GUIWIN *win);

GUIWIN *gemtk_wm_link(GUIWIN *win);

GUIWIN *gemtk_wm_unlink(GUIWIN *win);

short gemtk_wm_dispatch_event(EVMULT_IN *ev_in, EVMULT_OUT *ev_out, short msg[8]);

void gemtk_wm_get_grect(GUIWIN *win, enum guwin_area_e mode, GRECT *dest);

short gemtk_wm_get_toolbar_edit_obj(GUIWIN *win);

short gemtk_wm_get_handle(GUIWIN *win);

uint32_t gemtk_wm_get_state(GUIWIN *win);

void gemtk_wm_set_toolbar(GUIWIN *win, OBJECT *toolbar, short idx,
		uint32_t flags);

void gemtk_wm_set_event_handler(GUIWIN *win,gemtk_wm_event_handler_f cb);

void gemtk_wm_set_user_data(GUIWIN *win, void *data);

void * gemtk_wm_get_user_data(GUIWIN *win);

struct gemtk_wm_scroll_info_s * gemtk_wm_get_scroll_info(GUIWIN *win);

void gemtk_wm_set_scroll_grid(GUIWIN * win, short x, short y);

void gemtk_wm_set_content_units(GUIWIN * win, short x, short y);

void gemtk_wm_set_form(GUIWIN *win, OBJECT *tree, short index);

void gemtk_wm_set_toolbar_size(GUIWIN *win, uint16_t s);

void gemtk_wm_set_toolbar_edit_obj(GUIWIN *win, uint16_t obj, short kreturn);

void gemtk_wm_set_toolbar_redraw_func(GUIWIN *win, gemtk_wm_redraw_f func);

bool gemtk_wm_update_slider(GUIWIN *win, short mode);

void gemtk_wm_scroll(GUIWIN *gw, short orientation, int units, bool refresh);

void gemtk_wm_send_msg(GUIWIN *win, short msgtype, short a, short b, short c,
		short d);

short gemtk_wm_exec_msg(GUIWIN *win, short msg_type, short a, short b, short c,
		short d);

void gemtk_wm_exec_redraw(GUIWIN *win, GRECT *area);

VdiHdl gemtk_wm_get_vdi_handle(GUIWIN *win);

short getm_wm_get_toolbar_edit_obj(GUIWIN *win);

bool gemtk_wm_has_intersection(GUIWIN *win, GRECT *work);

void gemtk_wm_toolbar_redraw(GUIWIN *win, uint16_t msg, GRECT *clip);

void gemtk_wm_form_redraw(GUIWIN *gw, GRECT *clip);

void gemtk_wm_clear(GUIWIN *win);

/* -------------------------------------------------------------------------- */
/* AES SCROLLER MODULE                                                        */
/* -------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------- */
/* AES TABS MODULE                                                            */
/* -------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------- */
/* AES OBJECT TREE TOOLS                                                      */
/* -------------------------------------------------------------------------- */
char gemtk_obj_set_str_safe(OBJECT * tree, short idx, const char *txt);
char *gemtk_obj_get_text(OBJECT * tree, short idx);
GRECT * gemtk_obj_screen_rect(OBJECT * tree, short obj);
bool gemtk_obj_is_inside(OBJECT * tree, short obj, GRECT *area);
OBJECT *gemtk_obj_get_tree(int idx);
void gemtk_obj_mouse_sprite(OBJECT *tree, int index);
OBJECT *gemtk_obj_tree_copy(OBJECT *tree);
OBJECT * gemtk_obj_create_popup_tree(const char **items, int nitems,
                                     char * selected, bool horizontal,
                                     int max_width, int max_height);
void gemtk_obj_destroy_popup_tree(OBJECT * popup);
#endif // GEMTK_H_INCLUDED
