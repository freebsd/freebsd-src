/*
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

/* Object status values */
#define O_VISIBLE	0x0001
#define O_ACTIVE	0x0002

/* Standard attribute commands */
typedef enum {
	ATTR_BOX,
	ATTR_CENTER,
	ATTR_RIGHT,
	ATTR_SHADOW,
	ATTR_UNKNOWN
} AttrType;

struct attr_cmnd {
	char *attr_name;
	AttrType attr_type;
};

/* Ncurses color pairs */
typedef struct color_pair {
	int no;
	int fg;
	int bg;
} COLPAIR;

extern struct attr_cmnd attr_cmnds[];

extern hash_table  *root_table, *cbind;
extern DISPLAY *cdisp;
extern int lineno;

/* Private function declarations */
int display_tuples(char *, void *, void *);
int refresh_displays(char *, void *, void *);
int copy_object_tree(char *, void *, void *);
void process_tuple(OBJECT *);
void process_object(OBJECT *);
void process_input_object(OBJECT *);
void process_menu_object(OBJECT *);
void process_text_object(OBJECT *);

DISPLAY *default_open(DISPLAY *);
DISPLAY *ncurses_open(DISPLAY *);

int ncurses_print_string(OBJECT *, char *);
void ncurses_print_status(char *);
int ncurses_bind_key(OBJECT *, unsigned int);
void ncurses_display_action(OBJECT *);
void ncurses_display_compound(OBJECT *);
void ncurses_display_function(OBJECT *);
void ncurses_display_input(OBJECT *);
void ncurses_display_menu(OBJECT *);
void ncurses_display_text(OBJECT *);
void ncurses_process_action(OBJECT *);
void ncurses_process_input(OBJECT *);
void ncurses_process_menu(OBJECT *);
void ncurses_process_text(OBJECT *);
