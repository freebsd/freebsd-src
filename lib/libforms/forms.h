/*-
 * Copyright (c) 1995
 *	Paul Richards.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer,
 *    verbatim and that no modifications are made prior to this
 *    point in the file.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Paul Richards.
 * 4. The name Paul Richards may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY PAUL RICHARDS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL PAUL RICHARDS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#ifndef _FORMS_H_
#define _FORMS_H_

#include <ncurses.h>
#include <strhash.h>

#define F_DEFATTR 0
#define F_SELATTR A_REVERSE

/* Status values */

#define ST_ERROR	-1
#define ST_OK		0
#define ST_DONE		1
#define ST_CANCEL	2
#define ST_NOBIND	3
#define ST_RUNNING  4

typedef enum {
	TT_ANY,
	TT_OBJ_INST,
	TT_OBJ_DEF,
	TT_FUNC,
	TT_DISPLAY,
	TT_ATTR
} TupleType;

typedef enum {
	DT_ANY,
	DT_NCURSES,
	DT_X,
	DT_VGA
} DisplayType;

typedef enum {
	OT_ACTION,
	OT_COMPOUND,
	OT_FUNCTION,
	OT_INPUT,
	OT_MENU,
	OT_SHADOW,
	OT_TEXT
} ObjectType;

#define FUNCP void(*)(void *)

typedef struct Tuple {
	char *name;
	int type;
	void (*addr)(void *);
} TUPLE;

typedef struct NcursesDevice {
	char *ttyname;
	char *input;
	char *output;
	SCREEN *screen;
} NCURSDEV;

typedef struct NcursesWindow {
	WINDOW *win;
} NCURSES_WINDOW;

typedef struct Display {
	DisplayType type;
	int height;
	int width;
	int virt_height;
	int virt_width;
	union {
		NCURSDEV *ncurses;
	} device;
	hash_table *bind;
} DISPLAY;

typedef struct ActionObject {
	char *text;
	char *action;
} ACTION_OBJECT;

typedef struct CompoundObject {
	char *defobj;
} COMPOUND_OBJECT;

typedef struct FunctionObject {
	char *fn;
} FUNCTION_OBJECT;

typedef struct InputObject {
	int lbl_flag;
	char *label;
	char *input;
	int limit;
} INPUT_OBJECT;

typedef struct MenuObject {
	int selected;
	int no_options;
	char **options;
} MENU_OBJECT;

typedef struct TextObject {
	char *text;
} TEXT_OBJECT;

typedef union {
	NCURSES_WINDOW *ncurses;
} WIN;

typedef union {
		ACTION_OBJECT *action;
		COMPOUND_OBJECT *compound;
		FUNCTION_OBJECT *function;
		INPUT_OBJECT  *input;
		MENU_OBJECT   *menu;
		TEXT_OBJECT   *text;
} OBJ_TYPE;

typedef struct Object {
	ObjectType type;
	int status;
	struct Object *parent;
	int y;
	int x;
	int height;
	int width;
	char *attributes;
	char *highlight;
	char *lnext;
	char *lup;
	char *ldown;
	char *lleft;
	char *lright;
	char *UserDrawFunc;
	char *UserProcFunc;
	char *OnEntry;
	char *OnExit;
	OBJ_TYPE object;
	hash_table *bind;
	struct Display *display;
	WIN window;
} OBJECT;

/* Externally visible variables */
extern hash_table *root_table;

/* Function declarations */
__inline struct Tuple *get_tuple(hash_table *, char *, TupleType);
TUPLE *tuple_search(OBJECT *, char *, TupleType);
int bind_tuple(hash_table *, char *, TupleType, void(*fn)());
int add_menu_option(MENU_OBJECT *, char *);
void draw_box(OBJECT *);
void draw_shadow(OBJECT *);

#endif /* _FORMS_H_ */
