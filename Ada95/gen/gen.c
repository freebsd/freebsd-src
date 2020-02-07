/****************************************************************************
 * Copyright (c) 1998-2014,2016 Free Software Foundation, Inc.              *
 *                                                                          *
 * Permission is hereby granted, free of charge, to any person obtaining a  *
 * copy of this software and associated documentation files (the            *
 * "Software"), to deal in the Software without restriction, including      *
 * without limitation the rights to use, copy, modify, merge, publish,      *
 * distribute, distribute with modifications, sublicense, and/or sell       *
 * copies of the Software, and to permit persons to whom the Software is    *
 * furnished to do so, subject to the following conditions:                 *
 *                                                                          *
 * The above copyright notice and this permission notice shall be included  *
 * in all copies or substantial portions of the Software.                   *
 *                                                                          *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS  *
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF               *
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.   *
 * IN NO EVENT SHALL THE ABOVE COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,   *
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR    *
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR    *
 * THE USE OR OTHER DEALINGS IN THE SOFTWARE.                               *
 *                                                                          *
 * Except as contained in this notice, the name(s) of the above copyright   *
 * holders shall not be used in advertising or otherwise to promote the     *
 * sale, use or other dealings in this Software without prior written       *
 * authorization.                                                           *
 ****************************************************************************/

/****************************************************************************
 *   Author:  Juergen Pfeifer, 1996                                         *
 ****************************************************************************/

/*
    Version Control
    $Id: gen.c,v 1.70 2016/02/13 22:00:22 tom Exp $
  --------------------------------------------------------------------------*/
/*
  This program prints on its standard output the source for the
  Terminal_Interface.Curses_Constants Ada package specification. This pure
  package only exports C constants to the Ada compiler.
 */

#ifdef HAVE_CONFIG_H
#include <ncurses_cfg.h>
#else
#include <ncurses.h>
#endif

#include <stdlib.h>
#include <string.h>

#include <menu.h>
#include <form.h>

#undef UCHAR
#undef UINT

typedef unsigned char UCHAR;
typedef unsigned int UINT;

/* These global variables will be set by main () */
static int little_endian;
static const char *my_program_invocation_name = NULL;

static void
my_error(const char *message)
{
  fprintf(stderr, "%s: %s\n", my_program_invocation_name, message);
  exit(EXIT_FAILURE);
}

static void
print_constant(const char *name,
	       long value)
{
  printf("   %-28s : constant := %ld;\n", name, value);
}

#define PRINT_NAMED_CONSTANT(name) \
  print_constant (#name, name)

static void
print_comment(const char *message)
{
  printf("\n   --  %s\n\n", message);
}

/*
 * Make sure that KEY_MIN and KEY_MAX are defined.
 * main () will protest if KEY_MIN == 256
 */
#ifndef KEY_MAX
#  define KEY_MAX 0777
#endif
#ifndef KEY_MIN
#  define KEY_MIN 0401
#endif

static UCHAR
bit_is_set(const UCHAR * const data,
	   const UINT offset)
{
  const UCHAR byte = data[offset >> 3];
  UINT bit;

  if (little_endian)
    bit = offset;		/* offset */
  else				/* or */
    bit = ~offset;		/* 7 - offset */
  bit &= 7;			/* modulo 8 */
  return (UCHAR) (byte & (1 << bit));
}

/* Find lowest and highest used offset in a byte array. */
/* Returns 0 if and only if all bits are unset. */
static int
find_pos(const UCHAR * const data,
	 const UINT sizeof_data,
	 UINT * const low,
	 UINT * const high)
{
  const UINT last = (sizeof_data << 3) - 1;
  UINT offset;

  for (offset = last; !bit_is_set(data, offset); offset--)
    if (!offset)		/* All bits are 0. */
      return 0;
  *high = offset;

  for (offset = 0; !bit_is_set(data, offset); offset++)
    {
    }
  *low = offset;

  return -1;
}

#define PRINT_BITMASK(c_type, ada_name, mask_macro)                     \
  {                                                                     \
    UINT first, last;                                                   \
    c_type mask = (mask_macro);                                         \
    if (!find_pos ((UCHAR *)&mask, sizeof (mask), &first, &last))       \
      my_error ("failed to locate " ada_name);                          \
    print_constant (ada_name "_First", first);                          \
    print_constant (ada_name "_Last", last);                            \
  }

#define PRINT_NAMED_BITMASK(c_type, mask_macro)         \
  PRINT_BITMASK (c_type, #mask_macro, mask_macro)

#define STRUCT_OFFSET(record, field)                                    \
  {                                                                     \
    UINT first, last;                                                   \
    record mask;                                                        \
    memset (&mask, 0, sizeof (mask));                                   \
    memset (&mask.field, 0xff, sizeof(mask.field));                     \
    if (!find_pos ((UCHAR *)&mask, sizeof (mask), &first, &last))       \
      my_error ("failed to locate" #record "_" #field);                 \
    print_constant (#record "_" #field "_First", first);                \
    print_constant (#record "_" #field "_Last", last);                  \
  }

/*--------------------*/
/*  Start of main (). */
/*--------------------*/

int
main(int argc, const char *argv[])
{
  const int x = 0x12345678;

  little_endian = (*((const char *)&x) == 0x78);

  my_program_invocation_name = argv[0];

  if (KEY_MIN == 256)
    my_error("unexpected value for KEY_MIN: 256");

  if (argc != 2)
    my_error("Only one argument expected (DFT_ARG_SUFFIX)");

  printf("--  Generated by the C program %s (source " __FILE__ ").\n",
	 my_program_invocation_name);
  printf("--  Do not edit this file directly.\n");
  printf("--  The values provided here may vary on your system.\n");
  printf("\n");
  printf("with System;\n");
  printf("package Terminal_Interface.Curses_Constants is\n");
  printf("   pragma Pure;\n");
  printf("\n");

  printf("   DFT_ARG_SUFFIX : constant String := \"%s\";\n", argv[1]);
  printf("   Bit_Order : constant System.Bit_Order := System.%s_Order_First;\n",
	 little_endian ? "Low" : "High");
  print_constant("Sizeof_Bool", 8 * sizeof(bool));

  PRINT_NAMED_CONSTANT(OK);
  PRINT_NAMED_CONSTANT(ERR);
  printf("   pragma Warnings (Off); -- redefinition of Standard.True and False\n");
  PRINT_NAMED_CONSTANT(TRUE);
  PRINT_NAMED_CONSTANT(FALSE);
  printf("   pragma Warnings (On);\n");

  print_comment("Version of the ncurses library from extensions(3NCURSES)");
  PRINT_NAMED_CONSTANT(NCURSES_VERSION_MAJOR);
  PRINT_NAMED_CONSTANT(NCURSES_VERSION_MINOR);
  printf("   Version : constant String := \"%d.%d\";\n",
	 NCURSES_VERSION_MAJOR, NCURSES_VERSION_MINOR);

  print_comment("Character non-color attributes from attr(3NCURSES)");
  printf("   --  attr_t and chtype may be signed in C.\n");
  printf("   type attr_t is mod 2 ** %lu;\n", (long unsigned)(8 * sizeof(attr_t)));
  PRINT_NAMED_BITMASK(attr_t, A_CHARTEXT);
  PRINT_NAMED_BITMASK(attr_t, A_COLOR);
  PRINT_BITMASK(attr_t, "Attr", A_ATTRIBUTES & ~A_COLOR);
  PRINT_NAMED_BITMASK(attr_t, A_STANDOUT);
  PRINT_NAMED_BITMASK(attr_t, A_UNDERLINE);
  PRINT_NAMED_BITMASK(attr_t, A_REVERSE);
  PRINT_NAMED_BITMASK(attr_t, A_BLINK);
  PRINT_NAMED_BITMASK(attr_t, A_DIM);
  PRINT_NAMED_BITMASK(attr_t, A_BOLD);
  PRINT_NAMED_BITMASK(attr_t, A_PROTECT);
  PRINT_NAMED_BITMASK(attr_t, A_INVIS);
  PRINT_NAMED_BITMASK(attr_t, A_ALTCHARSET);
  PRINT_NAMED_BITMASK(attr_t, A_HORIZONTAL);
  PRINT_NAMED_BITMASK(attr_t, A_LEFT);
  PRINT_NAMED_BITMASK(attr_t, A_LOW);
  PRINT_NAMED_BITMASK(attr_t, A_RIGHT);
  PRINT_NAMED_BITMASK(attr_t, A_TOP);
  PRINT_NAMED_BITMASK(attr_t, A_VERTICAL);
  print_constant("chtype_Size", 8 * sizeof(chtype));

  print_comment("predefined color numbers from color(3NCURSES)");
  PRINT_NAMED_CONSTANT(COLOR_BLACK);
  PRINT_NAMED_CONSTANT(COLOR_RED);
  PRINT_NAMED_CONSTANT(COLOR_GREEN);
  PRINT_NAMED_CONSTANT(COLOR_YELLOW);
  PRINT_NAMED_CONSTANT(COLOR_BLUE);
  PRINT_NAMED_CONSTANT(COLOR_MAGENTA);
  PRINT_NAMED_CONSTANT(COLOR_CYAN);
  PRINT_NAMED_CONSTANT(COLOR_WHITE);

  print_comment("ETI return codes from ncurses.h");
  PRINT_NAMED_CONSTANT(E_OK);
  PRINT_NAMED_CONSTANT(E_SYSTEM_ERROR);
  PRINT_NAMED_CONSTANT(E_BAD_ARGUMENT);
  PRINT_NAMED_CONSTANT(E_POSTED);
  PRINT_NAMED_CONSTANT(E_CONNECTED);
  PRINT_NAMED_CONSTANT(E_BAD_STATE);
  PRINT_NAMED_CONSTANT(E_NO_ROOM);
  PRINT_NAMED_CONSTANT(E_NOT_POSTED);
  PRINT_NAMED_CONSTANT(E_UNKNOWN_COMMAND);
  PRINT_NAMED_CONSTANT(E_NO_MATCH);
  PRINT_NAMED_CONSTANT(E_NOT_SELECTABLE);
  PRINT_NAMED_CONSTANT(E_NOT_CONNECTED);
  PRINT_NAMED_CONSTANT(E_REQUEST_DENIED);
  PRINT_NAMED_CONSTANT(E_INVALID_FIELD);
  PRINT_NAMED_CONSTANT(E_CURRENT);

  print_comment("Input key codes not defined in any ncurses manpage");
  PRINT_NAMED_CONSTANT(KEY_MIN);
  PRINT_NAMED_CONSTANT(KEY_MAX);
#ifdef KEY_CODE_YES
  PRINT_NAMED_CONSTANT(KEY_CODE_YES);
#endif

  print_comment("Input key codes from getch(3NCURSES)");
  PRINT_NAMED_CONSTANT(KEY_BREAK);
  PRINT_NAMED_CONSTANT(KEY_DOWN);
  PRINT_NAMED_CONSTANT(KEY_UP);
  PRINT_NAMED_CONSTANT(KEY_LEFT);
  PRINT_NAMED_CONSTANT(KEY_RIGHT);
  PRINT_NAMED_CONSTANT(KEY_HOME);
  PRINT_NAMED_CONSTANT(KEY_BACKSPACE);
  PRINT_NAMED_CONSTANT(KEY_F0);
  print_constant("KEY_F1", KEY_F(1));
  print_constant("KEY_F2", KEY_F(2));
  print_constant("KEY_F3", KEY_F(3));
  print_constant("KEY_F4", KEY_F(4));
  print_constant("KEY_F5", KEY_F(5));
  print_constant("KEY_F6", KEY_F(6));
  print_constant("KEY_F7", KEY_F(7));
  print_constant("KEY_F8", KEY_F(8));
  print_constant("KEY_F9", KEY_F(9));
  print_constant("KEY_F10", KEY_F(10));
  print_constant("KEY_F11", KEY_F(11));
  print_constant("KEY_F12", KEY_F(12));
  print_constant("KEY_F13", KEY_F(13));
  print_constant("KEY_F14", KEY_F(14));
  print_constant("KEY_F15", KEY_F(15));
  print_constant("KEY_F16", KEY_F(16));
  print_constant("KEY_F17", KEY_F(17));
  print_constant("KEY_F18", KEY_F(18));
  print_constant("KEY_F19", KEY_F(19));
  print_constant("KEY_F20", KEY_F(20));
  print_constant("KEY_F21", KEY_F(21));
  print_constant("KEY_F22", KEY_F(22));
  print_constant("KEY_F23", KEY_F(23));
  print_constant("KEY_F24", KEY_F(24));
  PRINT_NAMED_CONSTANT(KEY_DL);
  PRINT_NAMED_CONSTANT(KEY_IL);
  PRINT_NAMED_CONSTANT(KEY_DC);
  PRINT_NAMED_CONSTANT(KEY_IC);
  PRINT_NAMED_CONSTANT(KEY_EIC);
  PRINT_NAMED_CONSTANT(KEY_CLEAR);
  PRINT_NAMED_CONSTANT(KEY_EOS);
  PRINT_NAMED_CONSTANT(KEY_EOL);
  PRINT_NAMED_CONSTANT(KEY_SF);
  PRINT_NAMED_CONSTANT(KEY_SR);
  PRINT_NAMED_CONSTANT(KEY_NPAGE);
  PRINT_NAMED_CONSTANT(KEY_PPAGE);
  PRINT_NAMED_CONSTANT(KEY_STAB);
  PRINT_NAMED_CONSTANT(KEY_CTAB);
  PRINT_NAMED_CONSTANT(KEY_CATAB);
  PRINT_NAMED_CONSTANT(KEY_ENTER);
  PRINT_NAMED_CONSTANT(KEY_SRESET);
  PRINT_NAMED_CONSTANT(KEY_RESET);
  PRINT_NAMED_CONSTANT(KEY_PRINT);
  PRINT_NAMED_CONSTANT(KEY_LL);
  PRINT_NAMED_CONSTANT(KEY_A1);
  PRINT_NAMED_CONSTANT(KEY_A3);
  PRINT_NAMED_CONSTANT(KEY_B2);
  PRINT_NAMED_CONSTANT(KEY_C1);
  PRINT_NAMED_CONSTANT(KEY_C3);
  PRINT_NAMED_CONSTANT(KEY_BTAB);
  PRINT_NAMED_CONSTANT(KEY_BEG);
  PRINT_NAMED_CONSTANT(KEY_CANCEL);
  PRINT_NAMED_CONSTANT(KEY_CLOSE);
  PRINT_NAMED_CONSTANT(KEY_COMMAND);
  PRINT_NAMED_CONSTANT(KEY_COPY);
  PRINT_NAMED_CONSTANT(KEY_CREATE);
  PRINT_NAMED_CONSTANT(KEY_END);
  PRINT_NAMED_CONSTANT(KEY_EXIT);
  PRINT_NAMED_CONSTANT(KEY_FIND);
  PRINT_NAMED_CONSTANT(KEY_HELP);
  PRINT_NAMED_CONSTANT(KEY_MARK);
  PRINT_NAMED_CONSTANT(KEY_MESSAGE);
  PRINT_NAMED_CONSTANT(KEY_MOVE);
  PRINT_NAMED_CONSTANT(KEY_NEXT);
  PRINT_NAMED_CONSTANT(KEY_OPEN);
  PRINT_NAMED_CONSTANT(KEY_OPTIONS);
  PRINT_NAMED_CONSTANT(KEY_PREVIOUS);
  PRINT_NAMED_CONSTANT(KEY_REDO);
  PRINT_NAMED_CONSTANT(KEY_REFERENCE);
  PRINT_NAMED_CONSTANT(KEY_REFRESH);
  PRINT_NAMED_CONSTANT(KEY_REPLACE);
  PRINT_NAMED_CONSTANT(KEY_RESTART);
  PRINT_NAMED_CONSTANT(KEY_RESUME);
  PRINT_NAMED_CONSTANT(KEY_SAVE);
  PRINT_NAMED_CONSTANT(KEY_SBEG);
  PRINT_NAMED_CONSTANT(KEY_SCANCEL);
  PRINT_NAMED_CONSTANT(KEY_SCOMMAND);
  PRINT_NAMED_CONSTANT(KEY_SCOPY);
  PRINT_NAMED_CONSTANT(KEY_SCREATE);
  PRINT_NAMED_CONSTANT(KEY_SDC);
  PRINT_NAMED_CONSTANT(KEY_SDL);
  PRINT_NAMED_CONSTANT(KEY_SELECT);
  PRINT_NAMED_CONSTANT(KEY_SEND);
  PRINT_NAMED_CONSTANT(KEY_SEOL);
  PRINT_NAMED_CONSTANT(KEY_SEXIT);
  PRINT_NAMED_CONSTANT(KEY_SFIND);
  PRINT_NAMED_CONSTANT(KEY_SHELP);
  PRINT_NAMED_CONSTANT(KEY_SHOME);
  PRINT_NAMED_CONSTANT(KEY_SIC);
  PRINT_NAMED_CONSTANT(KEY_SLEFT);
  PRINT_NAMED_CONSTANT(KEY_SMESSAGE);
  PRINT_NAMED_CONSTANT(KEY_SMOVE);
  PRINT_NAMED_CONSTANT(KEY_SNEXT);
  PRINT_NAMED_CONSTANT(KEY_SOPTIONS);
  PRINT_NAMED_CONSTANT(KEY_SPREVIOUS);
  PRINT_NAMED_CONSTANT(KEY_SPRINT);
  PRINT_NAMED_CONSTANT(KEY_SREDO);
  PRINT_NAMED_CONSTANT(KEY_SREPLACE);
  PRINT_NAMED_CONSTANT(KEY_SRIGHT);
  PRINT_NAMED_CONSTANT(KEY_SRSUME);
  PRINT_NAMED_CONSTANT(KEY_SSAVE);
  PRINT_NAMED_CONSTANT(KEY_SSUSPEND);
  PRINT_NAMED_CONSTANT(KEY_SUNDO);
  PRINT_NAMED_CONSTANT(KEY_SUSPEND);
  PRINT_NAMED_CONSTANT(KEY_UNDO);
  PRINT_NAMED_CONSTANT(KEY_MOUSE);
  PRINT_NAMED_CONSTANT(KEY_RESIZE);

  print_comment("alternate character codes (ACS) from addch(3NCURSES)");
#define PRINT_ACS(name) print_constant (#name, &name - &acs_map[0])
  PRINT_ACS(ACS_ULCORNER);
  PRINT_ACS(ACS_LLCORNER);
  PRINT_ACS(ACS_URCORNER);
  PRINT_ACS(ACS_LRCORNER);
  PRINT_ACS(ACS_LTEE);
  PRINT_ACS(ACS_RTEE);
  PRINT_ACS(ACS_BTEE);
  PRINT_ACS(ACS_TTEE);
  PRINT_ACS(ACS_HLINE);
  PRINT_ACS(ACS_VLINE);
  PRINT_ACS(ACS_PLUS);
  PRINT_ACS(ACS_S1);
  PRINT_ACS(ACS_S9);
  PRINT_ACS(ACS_DIAMOND);
  PRINT_ACS(ACS_CKBOARD);
  PRINT_ACS(ACS_DEGREE);
  PRINT_ACS(ACS_PLMINUS);
  PRINT_ACS(ACS_BULLET);
  PRINT_ACS(ACS_LARROW);
  PRINT_ACS(ACS_RARROW);
  PRINT_ACS(ACS_DARROW);
  PRINT_ACS(ACS_UARROW);
  PRINT_ACS(ACS_BOARD);
  PRINT_ACS(ACS_LANTERN);
  PRINT_ACS(ACS_BLOCK);
  PRINT_ACS(ACS_S3);
  PRINT_ACS(ACS_S7);
  PRINT_ACS(ACS_LEQUAL);
  PRINT_ACS(ACS_GEQUAL);
  PRINT_ACS(ACS_PI);
  PRINT_ACS(ACS_NEQUAL);
  PRINT_ACS(ACS_STERLING);

  print_comment("Menu_Options from opts(3MENU)");
  PRINT_NAMED_BITMASK(Menu_Options, O_ONEVALUE);
  PRINT_NAMED_BITMASK(Menu_Options, O_SHOWDESC);
  PRINT_NAMED_BITMASK(Menu_Options, O_ROWMAJOR);
  PRINT_NAMED_BITMASK(Menu_Options, O_IGNORECASE);
  PRINT_NAMED_BITMASK(Menu_Options, O_SHOWMATCH);
  PRINT_NAMED_BITMASK(Menu_Options, O_NONCYCLIC);
  print_constant("Menu_Options_Size", 8 * sizeof(Menu_Options));

  print_comment("Item_Options from menu_opts(3MENU)");
  PRINT_NAMED_BITMASK(Item_Options, O_SELECTABLE);
  print_constant("Item_Options_Size", 8 * sizeof(Item_Options));

  print_comment("Field_Options from field_opts(3FORM)");
  PRINT_NAMED_BITMASK(Field_Options, O_VISIBLE);
  PRINT_NAMED_BITMASK(Field_Options, O_ACTIVE);
  PRINT_NAMED_BITMASK(Field_Options, O_PUBLIC);
  PRINT_NAMED_BITMASK(Field_Options, O_EDIT);
  PRINT_NAMED_BITMASK(Field_Options, O_WRAP);
  PRINT_NAMED_BITMASK(Field_Options, O_BLANK);
  PRINT_NAMED_BITMASK(Field_Options, O_AUTOSKIP);
  PRINT_NAMED_BITMASK(Field_Options, O_NULLOK);
  PRINT_NAMED_BITMASK(Field_Options, O_PASSOK);
  PRINT_NAMED_BITMASK(Field_Options, O_STATIC);
  print_constant("Field_Options_Size", 8 * sizeof(Field_Options));

  print_comment("Field_Options from opts(3FORM)");
  PRINT_NAMED_BITMASK(Field_Options, O_NL_OVERLOAD);
  PRINT_NAMED_BITMASK(Field_Options, O_BS_OVERLOAD);
  /*  Field_Options_Size is defined below */

  print_comment("MEVENT structure from mouse(3NCURSES)");
  STRUCT_OFFSET(MEVENT, id);
  STRUCT_OFFSET(MEVENT, x);
  STRUCT_OFFSET(MEVENT, y);
  STRUCT_OFFSET(MEVENT, z);
  STRUCT_OFFSET(MEVENT, bstate);
  print_constant("MEVENT_Size", 8 * sizeof(MEVENT));

  print_comment("mouse events from mouse(3NCURSES)");
  {
    mmask_t all_events;

#define PRINT_MOUSE_EVENT(event)                \
    print_constant (#event, event);             \
    all_events |= event

    all_events = 0;
    PRINT_MOUSE_EVENT(BUTTON1_RELEASED);
    PRINT_MOUSE_EVENT(BUTTON1_PRESSED);
    PRINT_MOUSE_EVENT(BUTTON1_CLICKED);
    PRINT_MOUSE_EVENT(BUTTON1_DOUBLE_CLICKED);
    PRINT_MOUSE_EVENT(BUTTON1_TRIPLE_CLICKED);
#ifdef BUTTON1_RESERVED_EVENT
    PRINT_MOUSE_EVENT(BUTTON1_RESERVED_EVENT);
#endif
    print_constant("all_events_button_1", (long)all_events);

    all_events = 0;
    PRINT_MOUSE_EVENT(BUTTON2_RELEASED);
    PRINT_MOUSE_EVENT(BUTTON2_PRESSED);
    PRINT_MOUSE_EVENT(BUTTON2_CLICKED);
    PRINT_MOUSE_EVENT(BUTTON2_DOUBLE_CLICKED);
    PRINT_MOUSE_EVENT(BUTTON2_TRIPLE_CLICKED);
#ifdef BUTTON2_RESERVED_EVENT
    PRINT_MOUSE_EVENT(BUTTON2_RESERVED_EVENT);
#endif
    print_constant("all_events_button_2", (long)all_events);

    all_events = 0;
    PRINT_MOUSE_EVENT(BUTTON3_RELEASED);
    PRINT_MOUSE_EVENT(BUTTON3_PRESSED);
    PRINT_MOUSE_EVENT(BUTTON3_CLICKED);
    PRINT_MOUSE_EVENT(BUTTON3_DOUBLE_CLICKED);
    PRINT_MOUSE_EVENT(BUTTON3_TRIPLE_CLICKED);
#ifdef BUTTON3_RESERVED_EVENT
    PRINT_MOUSE_EVENT(BUTTON3_RESERVED_EVENT);
#endif
    print_constant("all_events_button_3", (long)all_events);

    all_events = 0;
    PRINT_MOUSE_EVENT(BUTTON4_RELEASED);
    PRINT_MOUSE_EVENT(BUTTON4_PRESSED);
    PRINT_MOUSE_EVENT(BUTTON4_CLICKED);
    PRINT_MOUSE_EVENT(BUTTON4_DOUBLE_CLICKED);
    PRINT_MOUSE_EVENT(BUTTON4_TRIPLE_CLICKED);
#ifdef BUTTON4_RESERVED_EVENT
    PRINT_MOUSE_EVENT(BUTTON4_RESERVED_EVENT);
#endif
    print_constant("all_events_button_4", (long)all_events);
  }
  PRINT_NAMED_CONSTANT(BUTTON_CTRL);
  PRINT_NAMED_CONSTANT(BUTTON_SHIFT);
  PRINT_NAMED_CONSTANT(BUTTON_ALT);
  PRINT_NAMED_CONSTANT(REPORT_MOUSE_POSITION);
  PRINT_NAMED_CONSTANT(ALL_MOUSE_EVENTS);

  print_comment("trace selection from trace(3NCURSES)");
  PRINT_NAMED_BITMASK(UINT, TRACE_TIMES);
  PRINT_NAMED_BITMASK(UINT, TRACE_TPUTS);
  PRINT_NAMED_BITMASK(UINT, TRACE_UPDATE);
  PRINT_NAMED_BITMASK(UINT, TRACE_MOVE);
  PRINT_NAMED_BITMASK(UINT, TRACE_CHARPUT);
  PRINT_NAMED_BITMASK(UINT, TRACE_CALLS);
  PRINT_NAMED_BITMASK(UINT, TRACE_VIRTPUT);
  PRINT_NAMED_BITMASK(UINT, TRACE_IEVENT);
  PRINT_NAMED_BITMASK(UINT, TRACE_BITS);
  PRINT_NAMED_BITMASK(UINT, TRACE_ICALLS);
  PRINT_NAMED_BITMASK(UINT, TRACE_CCALLS);
  PRINT_NAMED_BITMASK(UINT, TRACE_DATABASE);
  PRINT_NAMED_BITMASK(UINT, TRACE_ATTRS);
  print_constant("Trace_Size", 8 * sizeof(UINT));

  printf("end Terminal_Interface.Curses_Constants;\n");
  exit(EXIT_SUCCESS);
}
