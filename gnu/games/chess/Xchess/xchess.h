
/* This file contains code for X-CHESS.
   Copyright (C) 1986 Free Software Foundation, Inc.

This file is part of X-CHESS.

X-CHESS is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY.  No author or distributor
accepts responsibility to anyone for the consequences of using it
or for whether it serves any particular purpose or works at all,
unless he says so in writing.  Refer to the X-CHESS General Public
License for full details.

Everyone is granted permission to copy, modify and redistribute
X-CHESS, but only under the conditions described in the
X-CHESS General Public License.   A copy of this license is
supposed to have been given to you along with X-CHESS so you
can know your rights and responsibilities.  It should be in a
file named COPYING.  Among other things, the copyright notice
and this notice must be preserved on all copies.  */


/* RCS Info: $Revision: 1.5 $ on $Date: 86/11/26 12:11:39 $
 *           $Source: /users/faustus/xchess/RCS/xchess.h,v $
 * Copyright (c) 1986 Wayne A. Christopher, U. C. Berkeley CAD Group
 *	Permission is granted to do anything with this code except sell it
 *	or remove this message.
 *
 * Definitions for the X chess program.
 */

#include "std.h"
#include <X11/Xlib.h>
#include "scrollText/scrollText.h"

#define SIZE	8

typedef enum piecetype { PAWN, ROOK, KNIGHT, BISHOP, QUEEN, KING } piecetype;
typedef enum movetype { MOVE, QCASTLE, KCASTLE, CAPTURE } movetype;
typedef enum color { WHITE, BLACK, NONE } color;

typedef struct piece {
	enum piecetype type;
	enum color color;
} piece;

/* The board has y=0 and black at the top...  This probably isn't the best
 * place to keep track of who can castle, but it's part of the game state...
 */

typedef struct board {
	piece square[SIZE][SIZE];
	bool white_cant_castle_k;
	bool white_cant_castle_q;
	bool black_cant_castle_k;
	bool black_cant_castle_q;
} board;

typedef struct move {
	movetype type;
	piece piece;
	piece taken;
	int fromx, fromy;
	int tox, toy;
	struct move *next;
	bool enpassant;
	bool check;
} move;

#define iswhite(win, i, j)	(!(((i) + (j)) % 2))

/* Stuff for the display. */

typedef struct windata {
	Display *display;
	Window basewin;
	Window boardwin;
	Window recwin;
	Window wclockwin;
	Window bclockwin;
	Window messagewin;
	Window buttonwin;
	Window jailwin;
	Window icon;
	Pixmap iconpixmap;
	XColor blackpiece;
	XColor whitepiece;
	XColor blacksquare;
	XColor whitesquare;
	XColor border;
	XColor textcolor;
	XColor textback;
	XColor errortext;
	XColor playertext;
	XColor cursorcolor;
	XFontStruct *small;
	XFontStruct *medium;
	XFontStruct *large;
	bool bnw;
	color color;
	bool flipped;
	double whitehands[3];
	double blackhands[3];
	char *txtassoc;
} windata;

#define SMALL_FONT	"6x10"
#define MEDIUM_FONT	"8x13"
#define LARGE_FONT	"9x15"
#define JAIL_FONT	"6x10"

#define SQUARE_WIDTH	80
#define SQUARE_HEIGHT	80

#define BORDER_WIDTH	3

#define BOARD_WIDTH	8 * SQUARE_WIDTH + 7 * BORDER_WIDTH
#define BOARD_HEIGHT	8 * SQUARE_HEIGHT + 7 * BORDER_WIDTH
#define BOARD_XPOS	0
#define BOARD_YPOS	0

#define RECORD_WIDTH	265	/* 40 chars * 6 pixels / character. */
#define RECORD_HEIGHT	433
#define RECORD_XPOS	BOARD_WIDTH + BORDER_WIDTH
#define RECORD_YPOS	0

#define JAIL_WIDTH	RECORD_WIDTH
#define JAIL_HEIGHT	163
#define JAIL_XPOS	RECORD_XPOS
#define JAIL_YPOS	RECORD_YPOS + RECORD_HEIGHT + BORDER_WIDTH

#define CLOCK_WIDTH	131
#define CLOCK_HEIGHT	131 + BORDER_WIDTH + 20
#define WCLOCK_XPOS	RECORD_XPOS
#define WCLOCK_YPOS	RECORD_HEIGHT + JAIL_HEIGHT + BORDER_WIDTH * 2
#define BCLOCK_XPOS	WCLOCK_XPOS + CLOCK_WIDTH + BORDER_WIDTH
#define BCLOCK_YPOS	WCLOCK_YPOS

#define MESS_WIDTH	329
#define MESS_HEIGHT	92
#define MESS_XPOS	0
#define MESS_YPOS	BOARD_HEIGHT + BORDER_WIDTH

#define BUTTON_WIDTH	MESS_WIDTH
#define BUTTON_HEIGHT	MESS_HEIGHT
#define BUTTON_XPOS	MESS_WIDTH + BORDER_WIDTH
#define BUTTON_YPOS	MESS_YPOS

#define BASE_WIDTH	BOARD_WIDTH + RECORD_WIDTH + BORDER_WIDTH * 3
#define BASE_HEIGHT	BOARD_HEIGHT + MESS_HEIGHT + BORDER_WIDTH * 3

#define BASE_XPOS	50
#define BASE_YPOS	50

#define BLACK_PIECE_COLOR	"#202020"
#define WHITE_PIECE_COLOR	"#FFFFCC"
#define BLACK_SQUARE_COLOR	"#77A26D"
#define WHITE_SQUARE_COLOR	"#C8C365"
#define BORDER_COLOR		"#902E39"
#define TEXT_COLOR		"#006D6D"
#define TEXT_BACK		"#FFFFDD"
#define ERROR_TEXT		"Red"
#define PLAYER_TEXT		"Blue"
#define CURSOR_COLOR		"#FF606F"

#define DEF_RECORD_FILE		"xchess.game"

#define NUM_FLASHES		5
#define FLASH_SIZE		10

/* xchess.c */

extern void main();
extern bool debug;
extern char *progname;
extern char *proghost;
extern char *piecenames[];
extern char *colornames[];
extern char *movetypenames[];
extern char *dispname1, *dispname2;
extern bool oneboard;
extern bool bnwflag;
extern bool progflag;
extern bool blackflag;
extern bool quickflag;
extern int num_flashes;
extern int flash_size;
extern char *black_piece_color;
extern char *white_piece_color;
extern char *black_square_color;
extern char *white_square_color;
extern char *border_color;
extern char *text_color;
extern char *text_back;
extern char *error_text;
extern char *player_text;
extern char *cursor_color;

/* board.c */

extern void board_setup();
extern void board_drawall();
extern void board_move();
extern board *chessboard;
extern void board_init();

/* window.c */

extern bool win_setup();
extern void win_redraw();
extern void win_restart();
extern void win_drawboard();
extern void win_drawpiece();
extern void win_erasepiece();
extern void win_process();
extern void win_flash();
extern windata *win1, *win2;
extern bool win_flashmove;

/* control.c */

extern void button_pressed();
extern void button_released();
extern void move_piece();
extern void prog_move();
extern move *moves;
extern move *foremoves;
extern color nexttomove;
extern void replay();
extern void forward();
extern void cleanup();
extern void restart();
extern bool noisyflag;

/* valid.c */

extern bool valid_move();

/* record.c */

extern void record_move();
extern void record_reset();
extern void record_save();
extern void record_back();
extern void record_init();
extern void record_end();
extern bool record_english;
extern char *record_file;
extern int movenum;
extern bool saveflag;

/* message.c */

extern void message_init();
extern void message_add();
extern void message_send();

/* clock.c */

extern void clock_init();
extern void clock_draw();
extern void clock_update();
extern void clock_switch();
extern bool clock_started;
extern int movesperunit;
extern int timeunit;
extern int whiteseconds;
extern int blackseconds;

/* button.c */

extern void button_draw();
extern void button_service();

/* jail.c */

extern void jail_init();
extern void jail_draw();
extern void jail_add();
extern void jail_remove();

/* program.c */
extern bool program_init();
extern void program_end();
extern void program_send();
extern void program_undo();
extern move *program_get();

/* parse.c */

extern void load_game();
extern move *parse_file();
extern move *parse_move();
extern move *parse_imove();
extern bool loading_flag;
extern bool loading_paused;

/* popup.c */

extern bool pop_question();

