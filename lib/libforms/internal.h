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

#define FK_UP		f_keymap[0]
#define FK_DOWN		f_keymap[1]
#define FK_RIGHT	f_keymap[2]
#define FK_LEFT		f_keymap[3]
#define FK_NEXT		f_keymap[4]
#define FK_CLEFT	f_keymap[5]
#define FK_CRIGHT	f_keymap[6]
#define FK_CHOME	f_keymap[7]
#define FK_CEND		f_keymap[8]
#define FK_CBS		f_keymap[9]
#define FK_CDEL		f_keymap[10]
#define FK_ACCEPT	f_keymap[11]

extern unsigned int f_keymap[];

/* Private function declarations */
void display_field(WINDOW *, struct Field *);
void display_text(WINDOW *, struct Field *);
void display_input(WINDOW *, struct Field *);
void display_menu(WINDOW *, struct Field *);
void display_action(WINDOW *, struct Field *);
int print_string(WINDOW *, int, int, int, int, char *);
unsigned int do_key_bind(struct Form *, unsigned int);
int do_action(struct Form *);
int do_menu(struct Form *);
int do_input(struct Form *);
int init_field(char *, void *, void *);
int calc_string_width(char *);
void calc_field_height(struct Field *, char *);

#ifdef not
static void show_form(struct form *);
static void disp_text(struct form *);
static void disp_menu(struct form *);
static void disp_action(struct form *);
static void disp_input(struct form *);
static void field_menu(struct form *);
static void field_input(struct form *);
static void field_action(struct form *);
static int print_string(WINDOW *, int, int, int, int, char *);
static int next_field(struct form *form, int);
#endif
