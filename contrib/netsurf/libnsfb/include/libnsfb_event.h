/*
 * Copyright 2009 Vincent Sanders <vince@simtec.co.uk>
 *
 * This file is part of libnsfb, http://www.netsurf-browser.org/
 * Licenced under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 *
 * This is the exported interface for the libnsfb graphics library. 
 */

#ifndef _LIBNSFB_EVENT_H
#define _LIBNSFB_EVENT_H 1

enum nsfb_event_type_e {
    NSFB_EVENT_NONE,
    NSFB_EVENT_CONTROL,
    NSFB_EVENT_KEY_DOWN,
    NSFB_EVENT_KEY_UP,
    NSFB_EVENT_MOVE_RELATIVE,
    NSFB_EVENT_MOVE_ABSOLUTE,
};


/** keycodes which mostly map to ascii chars */
enum nsfb_key_code_e {
    NSFB_KEY_UNKNOWN		= 0,
    NSFB_KEY_BACKSPACE		= 8,
    NSFB_KEY_TAB		= 9,
    NSFB_KEY_LF                 = 10,
    NSFB_KEY_CLEAR		= 12,
    NSFB_KEY_RETURN		= 13,
    NSFB_KEY_PAUSE		= 19,
    NSFB_KEY_ESCAPE		= 27,
    NSFB_KEY_SPACE		= 32,
    NSFB_KEY_EXCLAIM		= 33,
    NSFB_KEY_QUOTEDBL		= 34,
    NSFB_KEY_HASH		= 35,
    NSFB_KEY_DOLLAR		= 36,
    NSFB_KEY_AMPERSAND		= 38,
    NSFB_KEY_QUOTE		= 39,
    NSFB_KEY_LEFTPAREN		= 40,
    NSFB_KEY_RIGHTPAREN		= 41,
    NSFB_KEY_ASTERISK		= 42,
    NSFB_KEY_PLUS		= 43,
    NSFB_KEY_COMMA		= 44,
    NSFB_KEY_MINUS		= 45,
    NSFB_KEY_PERIOD		= 46,
    NSFB_KEY_SLASH		= 47,
    NSFB_KEY_0			= 48,
    NSFB_KEY_1			= 49,
    NSFB_KEY_2			= 50,
    NSFB_KEY_3			= 51,
    NSFB_KEY_4			= 52,
    NSFB_KEY_5			= 53,
    NSFB_KEY_6			= 54,
    NSFB_KEY_7			= 55,
    NSFB_KEY_8			= 56,
    NSFB_KEY_9			= 57,
    NSFB_KEY_COLON		= 58,
    NSFB_KEY_SEMICOLON		= 59,
    NSFB_KEY_LESS		= 60,
    NSFB_KEY_EQUALS		= 61,
    NSFB_KEY_GREATER		= 62,
    NSFB_KEY_QUESTION		= 63,
    NSFB_KEY_AT			= 64,
    NSFB_KEY_LEFTBRACKET	= 91,
    NSFB_KEY_BACKSLASH		= 92,
    NSFB_KEY_RIGHTBRACKET	= 93,
    NSFB_KEY_CARET		= 94,
    NSFB_KEY_UNDERSCORE		= 95,
    NSFB_KEY_BACKQUOTE		= 96,
    NSFB_KEY_a			= 97,
    NSFB_KEY_b			= 98,
    NSFB_KEY_c			= 99,
    NSFB_KEY_d			= 100,
    NSFB_KEY_e			= 101,
    NSFB_KEY_f			= 102,
    NSFB_KEY_g			= 103,
    NSFB_KEY_h			= 104,
    NSFB_KEY_i			= 105,
    NSFB_KEY_j			= 106,
    NSFB_KEY_k			= 107,
    NSFB_KEY_l			= 108,
    NSFB_KEY_m			= 109,
    NSFB_KEY_n			= 110,
    NSFB_KEY_o			= 111,
    NSFB_KEY_p			= 112,
    NSFB_KEY_q			= 113,
    NSFB_KEY_r			= 114,
    NSFB_KEY_s			= 115,
    NSFB_KEY_t			= 116,
    NSFB_KEY_u			= 117,
    NSFB_KEY_v			= 118,
    NSFB_KEY_w			= 119,
    NSFB_KEY_x			= 120,
    NSFB_KEY_y			= 121,
    NSFB_KEY_z			= 122,
    NSFB_KEY_DELETE		= 127,

    NSFB_KEY_KP0		= 256,
    NSFB_KEY_KP1		= 257,
    NSFB_KEY_KP2		= 258,
    NSFB_KEY_KP3		= 259,
    NSFB_KEY_KP4		= 260,
    NSFB_KEY_KP5		= 261,
    NSFB_KEY_KP6		= 262,
    NSFB_KEY_KP7		= 263,
    NSFB_KEY_KP8		= 264,
    NSFB_KEY_KP9		= 265,
    NSFB_KEY_KP_PERIOD		= 266,
    NSFB_KEY_KP_DIVIDE		= 267,
    NSFB_KEY_KP_MULTIPLY	= 268,
    NSFB_KEY_KP_MINUS		= 269,
    NSFB_KEY_KP_PLUS		= 270,
    NSFB_KEY_KP_ENTER		= 271,
    NSFB_KEY_KP_EQUALS		= 272,

    NSFB_KEY_UP			= 273,
    NSFB_KEY_DOWN		= 274,
    NSFB_KEY_RIGHT		= 275,
    NSFB_KEY_LEFT		= 276,
    NSFB_KEY_INSERT		= 277,
    NSFB_KEY_HOME		= 278,
    NSFB_KEY_END		= 279,
    NSFB_KEY_PAGEUP		= 280,
    NSFB_KEY_PAGEDOWN		= 281,

    /* Function keys */
    NSFB_KEY_F1			= 282,
    NSFB_KEY_F2			= 283,
    NSFB_KEY_F3			= 284,
    NSFB_KEY_F4			= 285,
    NSFB_KEY_F5			= 286,
    NSFB_KEY_F6			= 287,
    NSFB_KEY_F7			= 288,
    NSFB_KEY_F8			= 289,
    NSFB_KEY_F9			= 290,
    NSFB_KEY_F10		= 291,
    NSFB_KEY_F11		= 292,
    NSFB_KEY_F12		= 293,
    NSFB_KEY_F13		= 294,
    NSFB_KEY_F14		= 295,
    NSFB_KEY_F15		= 296,

    /* Key state modifier keys */
    NSFB_KEY_NUMLOCK		= 300,
    NSFB_KEY_CAPSLOCK		= 301,
    NSFB_KEY_SCROLLOCK		= 302,
    NSFB_KEY_RSHIFT		= 303,
    NSFB_KEY_LSHIFT		= 304,
    NSFB_KEY_RCTRL		= 305,
    NSFB_KEY_LCTRL		= 306,
    NSFB_KEY_RALT		= 307,
    NSFB_KEY_LALT		= 308,
    NSFB_KEY_RMETA		= 309,
    NSFB_KEY_LMETA		= 310,
    NSFB_KEY_LSUPER		= 311,
    NSFB_KEY_RSUPER		= 312,
    NSFB_KEY_MODE		= 313,
    NSFB_KEY_COMPOSE		= 314,

    /* Miscellaneous function keys */
    NSFB_KEY_HELP		= 315,
    NSFB_KEY_PRINT		= 316,
    NSFB_KEY_SYSREQ		= 317,
    NSFB_KEY_BREAK		= 318,
    NSFB_KEY_MENU		= 319,
    NSFB_KEY_POWER		= 320,
    NSFB_KEY_EURO		= 321,
    NSFB_KEY_UNDO		= 322,

    /* mouse buttons */
    NSFB_KEY_MOUSE_1 = 401,
    NSFB_KEY_MOUSE_2 = 402,
    NSFB_KEY_MOUSE_3 = 403,
    NSFB_KEY_MOUSE_4 = 404,
    NSFB_KEY_MOUSE_5 = 405,

};

enum nsfb_control_e {
    NSFB_CONTROL_NONE,
    NSFB_CONTROL_TIMEOUT, /* timeout event */
    NSFB_CONTROL_QUIT, /* surface handler quit event */
};

struct nsfb_event_s {
    enum nsfb_event_type_e type;
    union {
        enum nsfb_key_code_e keycode;
        enum nsfb_control_e controlcode;
        struct {
            int x;
            int y;
            int z;
        } vector;
    } value;
};

/** Process input events.
 *
 * Gather events from a frontend.
 *
 * @param nsfb The library handle.
 * @param event The event structure to fill.
 * @param timeout The number of milliseconds to wait for input, -1 is wait
 * forever, 0 returns immediately.
 * @return If the /a event structure is updated true else false.
 */
bool nsfb_event(nsfb_t *nsfb, nsfb_event_t *event, int timeout);

#endif

/*
 * Local variables:
 *  c-basic-offset: 4
 *  tab-width: 8
 * End:
 */
