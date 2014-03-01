/****************************************************************************
 * Copyright (c) 1998,2010,2011 Free Software Foundation, Inc.              *
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
    $Id: gen.c,v 1.59 2011/03/31 23:50:24 tom Exp $
  --------------------------------------------------------------------------*/
/*
  This program generates various record structures and constants from the
  ncurses header file for the Ada95 packages. Essentially it produces
  Ada95 source on stdout, which is then merged using m4 into a template
  to produce the real source.
  */

#ifdef HAVE_CONFIG_H
#include <ncurses_cfg.h>
#else
#include <ncurses.h>
#define HAVE_USE_DEFAULT_COLORS 1
#endif

#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>

#include <menu.h>
#include <form.h>

#define UChar(c)	((unsigned char)(c))
#define RES_NAME "Reserved"

static const char *model = "";
static int little_endian = 0;

typedef struct
  {
    const char *name;
    unsigned long attr;
  }
name_attribute_pair;

static int
find_pos(char *s, unsigned len, int *low, int *high)
{
  unsigned int i, j;
  int l = 0;

  *high = -1;
  *low = (int)(8 * len);

  for (i = 0; i < len; i++, s++)
    {
      if (*s)
	{
	  for (j = 0; j < 8 * sizeof(char); j++)

	    {
	      if (((little_endian && ((*s) & 0x01)) ||
		   (!little_endian && ((*s) & 0x80))))
		{
		  if (l > *high)
		    *high = l;
		  if (l < *low)
		    *low = l;
		}
	      l++;
	      if (little_endian)
		{
		  *s >>= 1;
		}
	      else
		{
		  *s = (char)(*s << 1);
		}
	    }
	}
      else
	l += 8;
    }
  return (*high >= 0 && (*low <= *high)) ? *low : -1;
}

/*
 * This helper routine generates a representation clause for a
 * record type defined in the binding.
 * We are only dealing with record types which are of 32 or 16
 * bit size, i.e. they fit into an (u)int or a (u)short.
 */
static void
gen_reps(
	  const name_attribute_pair * nap,	/* array of name_attribute_pair records */
	  const char *name,	/* name of the represented record type  */
	  int len,		/* size of the record in bytes          */
	  int bias)
{
  const char *unused_name = "Unused";
  int long_bits = (8 * (int)sizeof(unsigned long));
  int len_bits = (8 * len);
  int i, j, n, l, cnt = 0, low, high;
  int width = strlen(RES_NAME) + 3;
  unsigned long a;
  unsigned long mask = 0;

  assert(nap != NULL);

  for (i = 0; nap[i].name != (char *)0; i++)
    {
      cnt++;
      l = (int)strlen(nap[i].name);
      if (l > width)
	width = l;
    }
  assert(width > 0);

  printf("   type %s is\n", name);
  printf("      record\n");
  for (i = 0; nap[i].name != (char *)0; i++)
    {
      mask |= nap[i].attr;
      printf("         %-*s : Boolean;\n", width, nap[i].name);
    }

  /*
   * Compute a mask for the unused bits in this target.
   */
  mask = ~mask;
  /*
   * Bits in the biased area are unused by the target.
   */
  for (j = 0; j < bias; ++j)
    {
      mask &= (unsigned long)(~(1L << j));
    }
  /*
   * Bits past the target's size are really unused.
   */
  for (j = len_bits + bias; j < long_bits; ++j)
    {
      mask &= (unsigned long)(~(1L << j));
    }
  if (mask != 0)
    {
      printf("         %-*s : Boolean;\n", width, unused_name);
    }
  printf("      end record;\n");
  printf("   pragma Convention (C, %s);\n\n", name);

  printf("   for %s use\n", name);
  printf("      record\n");

  for (i = 0; nap[i].name != (char *)0; i++)
    {
      a = nap[i].attr;
      l = find_pos((char *)&a, sizeof(a), &low, &high);
      if (l >= 0)
	printf("         %-*s at 0 range %2d .. %2d;\n", width, nap[i].name,
	       low - bias, high - bias);
    }
  if (mask != 0)
    {
      l = find_pos((char *)&mask, sizeof(mask), &low, &high);
      if (l >= 0)
	printf("         %-*s at 0 range %2d .. %2d;\n", width, unused_name,
	       low - bias, high - bias);
    }
  i = 1;
  n = cnt;
  printf("      end record;\n");
  printf("   for %s'Size use %d;\n", name, len_bits);
  printf("   --  Please note: this rep. clause is generated and may be\n");
  printf("   --               different on your system.");
}

static void
chtype_rep(const char *name, attr_t mask)
{
  attr_t x = (attr_t)-1;
  attr_t t = x & mask;
  int low, high;
  int l = find_pos((char *)&t, sizeof(t), &low, &high);

  if (l >= 0)
    printf("         %-5s at 0 range %2d .. %2d;\n", name, low, high);
}

static void
gen_chtype_rep(const char *name)
{
  printf("   for %s use\n      record\n", name);
  chtype_rep("Ch", A_CHARTEXT);
  chtype_rep("Color", A_COLOR);
  chtype_rep("Attr", (A_ATTRIBUTES & ~A_COLOR));
  printf("      end record;\n   for %s'Size use %ld;\n",
	 name, (long)(8 * sizeof(chtype)));

  printf("      --  Please note: this rep. clause is generated and may be\n");
  printf("      --               different on your system.\n");
}

static void
mrep_rep(const char *name, void *rec)
{
  int low, high;
  int l = find_pos((char *)rec, sizeof(MEVENT), &low, &high);

  if (l >= 0)
    printf("         %-7s at 0 range %3d .. %3d;\n", name, low, high);
}

static void
gen_mrep_rep(const char *name)
{
  MEVENT x;

  printf("   for %s use\n      record\n", name);

  memset(&x, 0, sizeof(x));
  x.id = -1;
  mrep_rep("Id", &x);

  memset(&x, 0, sizeof(x));
  x.x = -1;
  mrep_rep("X", &x);

  memset(&x, 0, sizeof(x));
  x.y = -1;
  mrep_rep("Y", &x);

  memset(&x, 0, sizeof(x));
  x.z = -1;
  mrep_rep("Z", &x);

  memset(&x, 0, sizeof(x));
  x.bstate = (mmask_t) - 1;
  mrep_rep("Bstate", &x);

  printf("      end record;\n");
  printf("      --  Please note: this rep. clause is generated and may be\n");
  printf("      --               different on your system.\n");
}

static void
gen_attr_set(const char *name)
{
  /* All of the A_xxx symbols are defined in ncurses, but not all are nonzero
   * if "configure --enable-widec" is not specified.  Originally (in
   * 1999-2000), the ifdef's also were needed since the proposed bit-layout
   * for wide characters allocated 16-bits for A_CHARTEXT, leaving too few
   * bits for a few of the A_xxx symbols.
   */
  static const name_attribute_pair nap[] =
  {
#if A_STANDOUT
    {"Stand_Out", A_STANDOUT},
#endif
#if A_UNDERLINE
    {"Under_Line", A_UNDERLINE},
#endif
#if A_REVERSE
    {"Reverse_Video", A_REVERSE},
#endif
#if A_BLINK
    {"Blink", A_BLINK},
#endif
#if A_DIM
    {"Dim_Character", A_DIM},
#endif
#if A_BOLD
    {"Bold_Character", A_BOLD},
#endif
#if A_ALTCHARSET
    {"Alternate_Character_Set", A_ALTCHARSET},
#endif
#if A_INVIS
    {"Invisible_Character", A_INVIS},
#endif
#if A_PROTECT
    {"Protected_Character", A_PROTECT},
#endif
#if A_HORIZONTAL
    {"Horizontal", A_HORIZONTAL},
#endif
#if A_LEFT
    {"Left", A_LEFT},
#endif
#if A_LOW
    {"Low", A_LOW},
#endif
#if A_RIGHT
    {"Right", A_RIGHT},
#endif
#if A_TOP
    {"Top", A_TOP},
#endif
#if A_VERTICAL
    {"Vertical", A_VERTICAL},
#endif
    {(char *)0, 0}
  };
  chtype attr = A_ATTRIBUTES & ~A_COLOR;
  int start = -1;
  int len = 0;
  int i;
  chtype set;
  for (i = 0; i < (int)(8 * sizeof(chtype)); i++)

    {
      set = (attr & 1);
      if (set)
	{
	  if (start < 0)
	    start = i;
	  if (start >= 0)
	    {
	      len++;
	    }
	}
      attr = attr >> 1;
    }
  gen_reps(nap, name, (len + 7) / 8, little_endian ? start : 0);
}

static void
gen_trace(const char *name)
{
  static const name_attribute_pair nap[] =
  {
    {"Times", TRACE_TIMES},
    {"Tputs", TRACE_TPUTS},
    {"Update", TRACE_UPDATE},
    {"Cursor_Move", TRACE_MOVE},
    {"Character_Output", TRACE_CHARPUT},
    {"Calls", TRACE_CALLS},
    {"Virtual_Puts", TRACE_VIRTPUT},
    {"Input_Events", TRACE_IEVENT},
    {"TTY_State", TRACE_BITS},
    {"Internal_Calls", TRACE_ICALLS},
    {"Character_Calls", TRACE_CCALLS},
    {"Termcap_TermInfo", TRACE_DATABASE},
    {"Attributes_And_Colors", TRACE_ATTRS},
    {(char *)0, 0}
  };
  gen_reps(nap, name, sizeof(int), 0);
}

static void
gen_menu_opt_rep(const char *name)
{
  static const name_attribute_pair nap[] =
  {
#ifdef O_ONEVALUE
    {"One_Valued", O_ONEVALUE},
#endif
#ifdef O_SHOWDESC
    {"Show_Descriptions", O_SHOWDESC},
#endif
#ifdef O_ROWMAJOR
    {"Row_Major_Order", O_ROWMAJOR},
#endif
#ifdef O_IGNORECASE
    {"Ignore_Case", O_IGNORECASE},
#endif
#ifdef O_SHOWMATCH
    {"Show_Matches", O_SHOWMATCH},
#endif
#ifdef O_NONCYCLIC
    {"Non_Cyclic", O_NONCYCLIC},
#endif
    {(char *)0, 0}
  };
  gen_reps(nap, name, sizeof(int), 0);
}

static void
gen_item_opt_rep(const char *name)
{
  static const name_attribute_pair nap[] =
  {
#ifdef O_SELECTABLE
    {"Selectable", O_SELECTABLE},
#endif
    {(char *)0, 0}
  };
  gen_reps(nap, name, sizeof(int), 0);
}

static void
gen_form_opt_rep(const char *name)
{
  static const name_attribute_pair nap[] =
  {
#ifdef O_NL_OVERLOAD
    {"NL_Overload", O_NL_OVERLOAD},
#endif
#ifdef O_BS_OVERLOAD
    {"BS_Overload", O_BS_OVERLOAD},
#endif
    {(char *)0, 0}
  };
  gen_reps(nap, name, sizeof(int), 0);
}

/*
 * Generate the representation clause for the Field_Option_Set record
 */
static void
gen_field_opt_rep(const char *name)
{
  static const name_attribute_pair nap[] =
  {
#ifdef O_VISIBLE
    {"Visible", O_VISIBLE},
#endif
#ifdef O_ACTIVE
    {"Active", O_ACTIVE},
#endif
#ifdef O_PUBLIC
    {"Public", O_PUBLIC},
#endif
#ifdef O_EDIT
    {"Edit", O_EDIT},
#endif
#ifdef O_WRAP
    {"Wrap", O_WRAP},
#endif
#ifdef O_BLANK
    {"Blank", O_BLANK},
#endif
#ifdef O_AUTOSKIP
    {"Auto_Skip", O_AUTOSKIP},
#endif
#ifdef O_NULLOK
    {"Null_Ok", O_NULLOK},
#endif
#ifdef O_PASSOK
    {"Pass_Ok", O_PASSOK},
#endif
#ifdef O_STATIC
    {"Static", O_STATIC},
#endif
    {(char *)0, 0}
  };
  gen_reps(nap, name, sizeof(int), 0);
}

/*
 * Generate a single key code constant definition.
 */
static void
keydef(const char *name, const char *old_name, int value, int mode)
{
  if (mode == 0)		/* Generate the new name */
    printf("   %-30s : constant Special_Key_Code := 8#%3o#;\n", name, value);
  else
    {
      const char *s = old_name;
      const char *t = name;

      /* generate the old name, but only if it doesn't conflict with the old
       * name (Ada95 isn't case sensitive!)
       */
      while (*s && *t && (toupper(UChar(*s++)) == toupper(UChar(*t++))));
      if (*s || *t)
	printf("   %-16s : Special_Key_Code renames %s;\n", old_name, name);
    }
}

/*
 * Generate constants for the key codes. When called with mode==0, a
 * complete list with nice constant names in proper casing style will
 * be generated. Otherwise a list of old (i.e. C-style) names will be
 * generated, given that the name wasn't already defined in the "nice"
 * list.
 */
static void
gen_keydefs(int mode)
{
  char buf[16];
  char obuf[16];
  int i;

#ifdef KEY_CODE_YES
  keydef("Key_Code_Yes", "KEY_CODE_YES", KEY_CODE_YES, mode);
#endif
#ifdef KEY_MIN
  keydef("Key_Min", "KEY_MIN", KEY_MIN, mode);
#endif
#ifdef KEY_BREAK
  keydef("Key_Break", "KEY_BREAK", KEY_BREAK, mode);
#endif
#ifdef KEY_DOWN
  keydef("Key_Cursor_Down", "KEY_DOWN", KEY_DOWN, mode);
#endif
#ifdef KEY_UP
  keydef("Key_Cursor_Up", "KEY_UP", KEY_UP, mode);
#endif
#ifdef KEY_LEFT
  keydef("Key_Cursor_Left", "KEY_LEFT", KEY_LEFT, mode);
#endif
#ifdef KEY_RIGHT
  keydef("Key_Cursor_Right", "KEY_RIGHT", KEY_RIGHT, mode);
#endif
#ifdef KEY_HOME
  keydef("Key_Home", "KEY_HOME", KEY_HOME, mode);
#endif
#ifdef KEY_BACKSPACE
  keydef("Key_Backspace", "KEY_BACKSPACE", KEY_BACKSPACE, mode);
#endif
#ifdef KEY_F0
  keydef("Key_F0", "KEY_F0", KEY_F0, mode);
#endif
#ifdef KEY_F
  for (i = 1; i <= 24; i++)
    {
      sprintf(buf, "Key_F%d", i);
      sprintf(obuf, "KEY_F%d", i);
      keydef(buf, obuf, KEY_F(i), mode);
    }
#endif
#ifdef KEY_DL
  keydef("Key_Delete_Line", "KEY_DL", KEY_DL, mode);
#endif
#ifdef KEY_IL
  keydef("Key_Insert_Line", "KEY_IL", KEY_IL, mode);
#endif
#ifdef KEY_DC
  keydef("Key_Delete_Char", "KEY_DC", KEY_DC, mode);
#endif
#ifdef KEY_IC
  keydef("Key_Insert_Char", "KEY_IC", KEY_IC, mode);
#endif
#ifdef KEY_EIC
  keydef("Key_Exit_Insert_Mode", "KEY_EIC", KEY_EIC, mode);
#endif
#ifdef KEY_CLEAR
  keydef("Key_Clear_Screen", "KEY_CLEAR", KEY_CLEAR, mode);
#endif
#ifdef KEY_EOS
  keydef("Key_Clear_End_Of_Screen", "KEY_EOS", KEY_EOS, mode);
#endif
#ifdef KEY_EOL
  keydef("Key_Clear_End_Of_Line", "KEY_EOL", KEY_EOL, mode);
#endif
#ifdef KEY_SF
  keydef("Key_Scroll_1_Forward", "KEY_SF", KEY_SF, mode);
#endif
#ifdef KEY_SR
  keydef("Key_Scroll_1_Backward", "KEY_SR", KEY_SR, mode);
#endif
#ifdef KEY_NPAGE
  keydef("Key_Next_Page", "KEY_NPAGE", KEY_NPAGE, mode);
#endif
#ifdef KEY_PPAGE
  keydef("Key_Previous_Page", "KEY_PPAGE", KEY_PPAGE, mode);
#endif
#ifdef KEY_STAB
  keydef("Key_Set_Tab", "KEY_STAB", KEY_STAB, mode);
#endif
#ifdef KEY_CTAB
  keydef("Key_Clear_Tab", "KEY_CTAB", KEY_CTAB, mode);
#endif
#ifdef KEY_CATAB
  keydef("Key_Clear_All_Tabs", "KEY_CATAB", KEY_CATAB, mode);
#endif
#ifdef KEY_ENTER
  keydef("Key_Enter_Or_Send", "KEY_ENTER", KEY_ENTER, mode);
#endif
#ifdef KEY_SRESET
  keydef("Key_Soft_Reset", "KEY_SRESET", KEY_SRESET, mode);
#endif
#ifdef KEY_RESET
  keydef("Key_Reset", "KEY_RESET", KEY_RESET, mode);
#endif
#ifdef KEY_PRINT
  keydef("Key_Print", "KEY_PRINT", KEY_PRINT, mode);
#endif
#ifdef KEY_LL
  keydef("Key_Bottom", "KEY_LL", KEY_LL, mode);
#endif
#ifdef KEY_A1
  keydef("Key_Upper_Left_Of_Keypad", "KEY_A1", KEY_A1, mode);
#endif
#ifdef KEY_A3
  keydef("Key_Upper_Right_Of_Keypad", "KEY_A3", KEY_A3, mode);
#endif
#ifdef KEY_B2
  keydef("Key_Center_Of_Keypad", "KEY_B2", KEY_B2, mode);
#endif
#ifdef KEY_C1
  keydef("Key_Lower_Left_Of_Keypad", "KEY_C1", KEY_C1, mode);
#endif
#ifdef KEY_C3
  keydef("Key_Lower_Right_Of_Keypad", "KEY_C3", KEY_C3, mode);
#endif
#ifdef KEY_BTAB
  keydef("Key_Back_Tab", "KEY_BTAB", KEY_BTAB, mode);
#endif
#ifdef KEY_BEG
  keydef("Key_Beginning", "KEY_BEG", KEY_BEG, mode);
#endif
#ifdef KEY_CANCEL
  keydef("Key_Cancel", "KEY_CANCEL", KEY_CANCEL, mode);
#endif
#ifdef KEY_CLOSE
  keydef("Key_Close", "KEY_CLOSE", KEY_CLOSE, mode);
#endif
#ifdef KEY_COMMAND
  keydef("Key_Command", "KEY_COMMAND", KEY_COMMAND, mode);
#endif
#ifdef KEY_COPY
  keydef("Key_Copy", "KEY_COPY", KEY_COPY, mode);
#endif
#ifdef KEY_CREATE
  keydef("Key_Create", "KEY_CREATE", KEY_CREATE, mode);
#endif
#ifdef KEY_END
  keydef("Key_End", "KEY_END", KEY_END, mode);
#endif
#ifdef KEY_EXIT
  keydef("Key_Exit", "KEY_EXIT", KEY_EXIT, mode);
#endif
#ifdef KEY_FIND
  keydef("Key_Find", "KEY_FIND", KEY_FIND, mode);
#endif
#ifdef KEY_HELP
  keydef("Key_Help", "KEY_HELP", KEY_HELP, mode);
#endif
#ifdef KEY_MARK
  keydef("Key_Mark", "KEY_MARK", KEY_MARK, mode);
#endif
#ifdef KEY_MESSAGE
  keydef("Key_Message", "KEY_MESSAGE", KEY_MESSAGE, mode);
#endif
#ifdef KEY_MOVE
  keydef("Key_Move", "KEY_MOVE", KEY_MOVE, mode);
#endif
#ifdef KEY_NEXT
  keydef("Key_Next", "KEY_NEXT", KEY_NEXT, mode);
#endif
#ifdef KEY_OPEN
  keydef("Key_Open", "KEY_OPEN", KEY_OPEN, mode);
#endif
#ifdef KEY_OPTIONS
  keydef("Key_Options", "KEY_OPTIONS", KEY_OPTIONS, mode);
#endif
#ifdef KEY_PREVIOUS
  keydef("Key_Previous", "KEY_PREVIOUS", KEY_PREVIOUS, mode);
#endif
#ifdef KEY_REDO
  keydef("Key_Redo", "KEY_REDO", KEY_REDO, mode);
#endif
#ifdef KEY_REFERENCE
  keydef("Key_Reference", "KEY_REFERENCE", KEY_REFERENCE, mode);
#endif
#ifdef KEY_REFRESH
  keydef("Key_Refresh", "KEY_REFRESH", KEY_REFRESH, mode);
#endif
#ifdef KEY_REPLACE
  keydef("Key_Replace", "KEY_REPLACE", KEY_REPLACE, mode);
#endif
#ifdef KEY_RESTART
  keydef("Key_Restart", "KEY_RESTART", KEY_RESTART, mode);
#endif
#ifdef KEY_RESUME
  keydef("Key_Resume", "KEY_RESUME", KEY_RESUME, mode);
#endif
#ifdef KEY_SAVE
  keydef("Key_Save", "KEY_SAVE", KEY_SAVE, mode);
#endif
#ifdef KEY_SBEG
  keydef("Key_Shift_Begin", "KEY_SBEG", KEY_SBEG, mode);
#endif
#ifdef KEY_SCANCEL
  keydef("Key_Shift_Cancel", "KEY_SCANCEL", KEY_SCANCEL, mode);
#endif
#ifdef KEY_SCOMMAND
  keydef("Key_Shift_Command", "KEY_SCOMMAND", KEY_SCOMMAND, mode);
#endif
#ifdef KEY_SCOPY
  keydef("Key_Shift_Copy", "KEY_SCOPY", KEY_SCOPY, mode);
#endif
#ifdef KEY_SCREATE
  keydef("Key_Shift_Create", "KEY_SCREATE", KEY_SCREATE, mode);
#endif
#ifdef KEY_SDC
  keydef("Key_Shift_Delete_Char", "KEY_SDC", KEY_SDC, mode);
#endif
#ifdef KEY_SDL
  keydef("Key_Shift_Delete_Line", "KEY_SDL", KEY_SDL, mode);
#endif
#ifdef KEY_SELECT
  keydef("Key_Select", "KEY_SELECT", KEY_SELECT, mode);
#endif
#ifdef KEY_SEND
  keydef("Key_Shift_End", "KEY_SEND", KEY_SEND, mode);
#endif
#ifdef KEY_SEOL
  keydef("Key_Shift_Clear_End_Of_Line", "KEY_SEOL", KEY_SEOL, mode);
#endif
#ifdef KEY_SEXIT
  keydef("Key_Shift_Exit", "KEY_SEXIT", KEY_SEXIT, mode);
#endif
#ifdef KEY_SFIND
  keydef("Key_Shift_Find", "KEY_SFIND", KEY_SFIND, mode);
#endif
#ifdef KEY_SHELP
  keydef("Key_Shift_Help", "KEY_SHELP", KEY_SHELP, mode);
#endif
#ifdef KEY_SHOME
  keydef("Key_Shift_Home", "KEY_SHOME", KEY_SHOME, mode);
#endif
#ifdef KEY_SIC
  keydef("Key_Shift_Insert_Char", "KEY_SIC", KEY_SIC, mode);
#endif
#ifdef KEY_SLEFT
  keydef("Key_Shift_Cursor_Left", "KEY_SLEFT", KEY_SLEFT, mode);
#endif
#ifdef KEY_SMESSAGE
  keydef("Key_Shift_Message", "KEY_SMESSAGE", KEY_SMESSAGE, mode);
#endif
#ifdef KEY_SMOVE
  keydef("Key_Shift_Move", "KEY_SMOVE", KEY_SMOVE, mode);
#endif
#ifdef KEY_SNEXT
  keydef("Key_Shift_Next_Page", "KEY_SNEXT", KEY_SNEXT, mode);
#endif
#ifdef KEY_SOPTIONS
  keydef("Key_Shift_Options", "KEY_SOPTIONS", KEY_SOPTIONS, mode);
#endif
#ifdef KEY_SPREVIOUS
  keydef("Key_Shift_Previous_Page", "KEY_SPREVIOUS", KEY_SPREVIOUS, mode);
#endif
#ifdef KEY_SPRINT
  keydef("Key_Shift_Print", "KEY_SPRINT", KEY_SPRINT, mode);
#endif
#ifdef KEY_SREDO
  keydef("Key_Shift_Redo", "KEY_SREDO", KEY_SREDO, mode);
#endif
#ifdef KEY_SREPLACE
  keydef("Key_Shift_Replace", "KEY_SREPLACE", KEY_SREPLACE, mode);
#endif
#ifdef KEY_SRIGHT
  keydef("Key_Shift_Cursor_Right", "KEY_SRIGHT", KEY_SRIGHT, mode);
#endif
#ifdef KEY_SRSUME
  keydef("Key_Shift_Resume", "KEY_SRSUME", KEY_SRSUME, mode);
#endif
#ifdef KEY_SSAVE
  keydef("Key_Shift_Save", "KEY_SSAVE", KEY_SSAVE, mode);
#endif
#ifdef KEY_SSUSPEND
  keydef("Key_Shift_Suspend", "KEY_SSUSPEND", KEY_SSUSPEND, mode);
#endif
#ifdef KEY_SUNDO
  keydef("Key_Shift_Undo", "KEY_SUNDO", KEY_SUNDO, mode);
#endif
#ifdef KEY_SUSPEND
  keydef("Key_Suspend", "KEY_SUSPEND", KEY_SUSPEND, mode);
#endif
#ifdef KEY_UNDO
  keydef("Key_Undo", "KEY_UNDO", KEY_UNDO, mode);
#endif
#ifdef KEY_MOUSE
  keydef("Key_Mouse", "KEY_MOUSE", KEY_MOUSE, mode);
#endif
#ifdef KEY_RESIZE
  keydef("Key_Resize", "KEY_RESIZE", KEY_RESIZE, mode);
#endif
}

/*
 * Generate a constant with the given name. The second parameter
 * is a reference to the ACS character in the acs_map[] array and
 * will be translated into an index.
 */
static void
acs_def(const char *name, chtype *a)
{
  int c = (int)(a - &acs_map[0]);

  printf("   %-24s : constant Character := ", name);
  if (isprint(UChar(c)) && (c != '`'))
    printf("'%c';\n", c);
  else
    printf("Character'Val (%d);\n", c);
}

/*
 * Generate the constants for the ACS characters
 */
static void
gen_acs(void)
{
  printf("   type C_ACS_Map is array (Character'Val (0) .. Character'Val (127))\n");
  printf("        of Attributed_Character;\n");
#if USE_REENTRANT || BROKEN_LINKER
  printf("   type C_ACS_Ptr is access C_ACS_Map;\n");
  printf("   function ACS_Map return C_ACS_Ptr;\n");
  printf("   pragma Import (C, ACS_Map, \""
	 NCURSES_WRAP_PREFIX
	 "acs_map\");\n");
#else
  printf("   ACS_Map : C_ACS_Map;\n");
  printf("   pragma Import (C, ACS_Map, \"acs_map\");\n");
#endif
  printf("   --\n");
  printf("   --\n");
  printf("   --  Constants for several characters from the Alternate Character Set\n");
  printf("   --  You must use these constants as indices into the ACS_Map array\n");
  printf("   --  to get the corresponding attributed character at runtime.\n");
  printf("   --\n");

#ifdef ACS_ULCORNER
  acs_def("ACS_Upper_Left_Corner", &ACS_ULCORNER);
#endif
#ifdef ACS_LLCORNER
  acs_def("ACS_Lower_Left_Corner", &ACS_LLCORNER);
#endif
#ifdef ACS_URCORNER
  acs_def("ACS_Upper_Right_Corner", &ACS_URCORNER);
#endif
#ifdef ACS_LRCORNER
  acs_def("ACS_Lower_Right_Corner", &ACS_LRCORNER);
#endif
#ifdef ACS_LTEE
  acs_def("ACS_Left_Tee", &ACS_LTEE);
#endif
#ifdef ACS_RTEE
  acs_def("ACS_Right_Tee", &ACS_RTEE);
#endif
#ifdef ACS_BTEE
  acs_def("ACS_Bottom_Tee", &ACS_BTEE);
#endif
#ifdef ACS_TTEE
  acs_def("ACS_Top_Tee", &ACS_TTEE);
#endif
#ifdef ACS_HLINE
  acs_def("ACS_Horizontal_Line", &ACS_HLINE);
#endif
#ifdef ACS_VLINE
  acs_def("ACS_Vertical_Line", &ACS_VLINE);
#endif
#ifdef ACS_PLUS
  acs_def("ACS_Plus_Symbol", &ACS_PLUS);
#endif
#ifdef ACS_S1
  acs_def("ACS_Scan_Line_1", &ACS_S1);
#endif
#ifdef ACS_S9
  acs_def("ACS_Scan_Line_9", &ACS_S9);
#endif
#ifdef ACS_DIAMOND
  acs_def("ACS_Diamond", &ACS_DIAMOND);
#endif
#ifdef ACS_CKBOARD
  acs_def("ACS_Checker_Board", &ACS_CKBOARD);
#endif
#ifdef ACS_DEGREE
  acs_def("ACS_Degree", &ACS_DEGREE);
#endif
#ifdef ACS_PLMINUS
  acs_def("ACS_Plus_Minus", &ACS_PLMINUS);
#endif
#ifdef ACS_BULLET
  acs_def("ACS_Bullet", &ACS_BULLET);
#endif
#ifdef ACS_LARROW
  acs_def("ACS_Left_Arrow", &ACS_LARROW);
#endif
#ifdef ACS_RARROW
  acs_def("ACS_Right_Arrow", &ACS_RARROW);
#endif
#ifdef ACS_DARROW
  acs_def("ACS_Down_Arrow", &ACS_DARROW);
#endif
#ifdef ACS_UARROW
  acs_def("ACS_Up_Arrow", &ACS_UARROW);
#endif
#ifdef ACS_BOARD
  acs_def("ACS_Board_Of_Squares", &ACS_BOARD);
#endif
#ifdef ACS_LANTERN
  acs_def("ACS_Lantern", &ACS_LANTERN);
#endif
#ifdef ACS_BLOCK
  acs_def("ACS_Solid_Block", &ACS_BLOCK);
#endif
#ifdef ACS_S3
  acs_def("ACS_Scan_Line_3", &ACS_S3);
#endif
#ifdef ACS_S7
  acs_def("ACS_Scan_Line_7", &ACS_S7);
#endif
#ifdef ACS_LEQUAL
  acs_def("ACS_Less_Or_Equal", &ACS_LEQUAL);
#endif
#ifdef ACS_GEQUAL
  acs_def("ACS_Greater_Or_Equal", &ACS_GEQUAL);
#endif
#ifdef ACS_PI
  acs_def("ACS_PI", &ACS_PI);
#endif
#ifdef ACS_NEQUAL
  acs_def("ACS_Not_Equal", &ACS_NEQUAL);
#endif
#ifdef ACS_STERLING
  acs_def("ACS_Sterling", &ACS_STERLING);
#endif
}

#define GEN_EVENT(name,value) \
   printf("   %-25s : constant Event_Mask := 8#%011lo#;\n", \
          #name, value)

#define GEN_MEVENT(name) \
   printf("   %-25s : constant Event_Mask := 8#%011lo#;\n", \
          #name, name)

static void
gen_mouse_events(void)
{
  mmask_t all1 = 0;
  mmask_t all2 = 0;
  mmask_t all3 = 0;
  mmask_t all4 = 0;

#ifdef BUTTON1_RELEASED
  GEN_MEVENT(BUTTON1_RELEASED);
  all1 |= BUTTON1_RELEASED;
#endif
#ifdef BUTTON1_PRESSED
  GEN_MEVENT(BUTTON1_PRESSED);
  all1 |= BUTTON1_PRESSED;
#endif
#ifdef BUTTON1_CLICKED
  GEN_MEVENT(BUTTON1_CLICKED);
  all1 |= BUTTON1_CLICKED;
#endif
#ifdef BUTTON1_DOUBLE_CLICKED
  GEN_MEVENT(BUTTON1_DOUBLE_CLICKED);
  all1 |= BUTTON1_DOUBLE_CLICKED;
#endif
#ifdef BUTTON1_TRIPLE_CLICKED
  GEN_MEVENT(BUTTON1_TRIPLE_CLICKED);
  all1 |= BUTTON1_TRIPLE_CLICKED;
#endif
#ifdef BUTTON1_RESERVED_EVENT
  GEN_MEVENT(BUTTON1_RESERVED_EVENT);
  all1 |= BUTTON1_RESERVED_EVENT;
#endif
#ifdef BUTTON2_RELEASED
  GEN_MEVENT(BUTTON2_RELEASED);
  all2 |= BUTTON2_RELEASED;
#endif
#ifdef BUTTON2_PRESSED
  GEN_MEVENT(BUTTON2_PRESSED);
  all2 |= BUTTON2_PRESSED;
#endif
#ifdef BUTTON2_CLICKED
  GEN_MEVENT(BUTTON2_CLICKED);
  all2 |= BUTTON2_CLICKED;
#endif
#ifdef BUTTON2_DOUBLE_CLICKED
  GEN_MEVENT(BUTTON2_DOUBLE_CLICKED);
  all2 |= BUTTON2_DOUBLE_CLICKED;
#endif
#ifdef BUTTON2_TRIPLE_CLICKED
  GEN_MEVENT(BUTTON2_TRIPLE_CLICKED);
  all2 |= BUTTON2_TRIPLE_CLICKED;
#endif
#ifdef BUTTON2_RESERVED_EVENT
  GEN_MEVENT(BUTTON2_RESERVED_EVENT);
  all2 |= BUTTON2_RESERVED_EVENT;
#endif
#ifdef BUTTON3_RELEASED
  GEN_MEVENT(BUTTON3_RELEASED);
  all3 |= BUTTON3_RELEASED;
#endif
#ifdef BUTTON3_PRESSED
  GEN_MEVENT(BUTTON3_PRESSED);
  all3 |= BUTTON3_PRESSED;
#endif
#ifdef BUTTON3_CLICKED
  GEN_MEVENT(BUTTON3_CLICKED);
  all3 |= BUTTON3_CLICKED;
#endif
#ifdef BUTTON3_DOUBLE_CLICKED
  GEN_MEVENT(BUTTON3_DOUBLE_CLICKED);
  all3 |= BUTTON3_DOUBLE_CLICKED;
#endif
#ifdef BUTTON3_TRIPLE_CLICKED
  GEN_MEVENT(BUTTON3_TRIPLE_CLICKED);
  all3 |= BUTTON3_TRIPLE_CLICKED;
#endif
#ifdef BUTTON3_RESERVED_EVENT
  GEN_MEVENT(BUTTON3_RESERVED_EVENT);
  all3 |= BUTTON3_RESERVED_EVENT;
#endif
#ifdef BUTTON4_RELEASED
  GEN_MEVENT(BUTTON4_RELEASED);
  all4 |= BUTTON4_RELEASED;
#endif
#ifdef BUTTON4_PRESSED
  GEN_MEVENT(BUTTON4_PRESSED);
  all4 |= BUTTON4_PRESSED;
#endif
#ifdef BUTTON4_CLICKED
  GEN_MEVENT(BUTTON4_CLICKED);
  all4 |= BUTTON4_CLICKED;
#endif
#ifdef BUTTON4_DOUBLE_CLICKED
  GEN_MEVENT(BUTTON4_DOUBLE_CLICKED);
  all4 |= BUTTON4_DOUBLE_CLICKED;
#endif
#ifdef BUTTON4_TRIPLE_CLICKED
  GEN_MEVENT(BUTTON4_TRIPLE_CLICKED);
  all4 |= BUTTON4_TRIPLE_CLICKED;
#endif
#ifdef BUTTON4_RESERVED_EVENT
  GEN_MEVENT(BUTTON4_RESERVED_EVENT);
  all4 |= BUTTON4_RESERVED_EVENT;
#endif
#ifdef BUTTON_CTRL
  GEN_MEVENT(BUTTON_CTRL);
#endif
#ifdef BUTTON_SHIFT
  GEN_MEVENT(BUTTON_SHIFT);
#endif
#ifdef BUTTON_ALT
  GEN_MEVENT(BUTTON_ALT);
#endif
#ifdef REPORT_MOUSE_POSITION
  GEN_MEVENT(REPORT_MOUSE_POSITION);
#endif
#ifdef ALL_MOUSE_EVENTS
  GEN_MEVENT(ALL_MOUSE_EVENTS);
#endif

  GEN_EVENT(BUTTON1_EVENTS, all1);
  GEN_EVENT(BUTTON2_EVENTS, all2);
  GEN_EVENT(BUTTON3_EVENTS, all3);
  GEN_EVENT(BUTTON4_EVENTS, all4);
}

static void
wrap_one_var(const char *c_var,
	     const char *c_type,
	     const char *ada_func,
	     const char *ada_type)
{
#if USE_REENTRANT
  /* must wrap variables */
  printf("\n");
  printf("   function %s return %s\n", ada_func, ada_type);
  printf("   is\n");
  printf("      function Result return %s;\n", c_type);
  printf("      pragma Import (C, Result, \"" NCURSES_WRAP_PREFIX "%s\");\n", c_var);
  printf("   begin\n");
  if (strcmp(c_type, ada_type))
    printf("      return %s (Result);\n", ada_type);
  else
    printf("      return Result;\n");
  printf("   end %s;\n", ada_func);
#else
  /* global variables are really global */
  printf("\n");
  printf("   function %s return %s\n", ada_func, ada_type);
  printf("   is\n");
  printf("      Result : %s;\n", c_type);
  printf("      pragma Import (C, Result, \"%s\");\n", c_var);
  printf("   begin\n");
  if (strcmp(c_type, ada_type))
    printf("      return %s (Result);\n", ada_type);
  else
    printf("      return Result;\n");
  printf("   end %s;\n", ada_func);
#endif
}

#define GEN_PUBLIC_VAR(c_var, c_type, ada_func, ada_type) \
	wrap_one_var(#c_var, #c_type, #ada_func, #ada_type)

static void
gen_public_vars(void)
{
  GEN_PUBLIC_VAR(stdscr, Window, Standard_Window, Window);
  GEN_PUBLIC_VAR(curscr, Window, Current_Window, Window);
  GEN_PUBLIC_VAR(LINES, C_Int, Lines, Line_Count);
  GEN_PUBLIC_VAR(COLS, C_Int, Columns, Column_Count);
  GEN_PUBLIC_VAR(TABSIZE, C_Int, Tab_Size, Natural);
  GEN_PUBLIC_VAR(COLORS, C_Int, Number_Of_Colors, Natural);
  GEN_PUBLIC_VAR(COLOR_PAIRS, C_Int, Number_Of_Color_Pairs, Natural);
}

/*
 * Output some comment lines indicating that the file is generated.
 * The name parameter is the name of the facility to be used in
 * the comment.
 */
static void
prologue(const char *name)
{
  printf("--  %s binding.\n", name);
  printf("--  This module is generated. Please don't change it manually!\n");
  printf("--  Run the generator instead.\n--  |");

  printf("define(`M4_BIT_ORDER',`%s_Order_First')",
	 little_endian ? "Low" : "High");
}

/*
 * Write the prologue for the curses facility and make sure that
 * KEY_MIN and KEY_MAX are defined for the rest of this source.
 */
static void
basedefs(void)
{
  prologue("curses");
#ifndef KEY_MAX
#  define KEY_MAX 0777
#endif
  printf("define(`M4_KEY_MAX',`8#%o#')", KEY_MAX);
#ifndef KEY_MIN
#  define KEY_MIN 0401
#endif
  if (KEY_MIN == 256)
    {
      fprintf(stderr, "Unexpected value for KEY_MIN: %d\n", KEY_MIN);
      exit(1);
    }
  printf("define(`M4_SPECIAL_FIRST',`8#%o#')", KEY_MIN - 1);
}

/*
 * Write out the comment lines for the menu facility
 */
static void
menu_basedefs(void)
{
  prologue("menu");
}

/*
 * Write out the comment lines for the form facility
 */
static void
form_basedefs(void)
{
  prologue("form");
}

/*
 * Write out the comment lines for the mouse facility
 */
static void
mouse_basedefs(void)
{
  prologue("mouse");
}

/*
 * Write the definition of a single color
 */
static void
color_def(const char *name, int value)
{
  printf("   %-16s : constant Color_Number := %d;\n", name, value);
}

/*
 * Generate all color definitions
 */
static void
gen_color(void)
{
#if HAVE_USE_DEFAULT_COLORS
  color_def("Default_Color", -1);
#endif
#ifdef COLOR_BLACK
  color_def("Black", COLOR_BLACK);
#endif
#ifdef COLOR_RED
  color_def("Red", COLOR_RED);
#endif
#ifdef COLOR_GREEN
  color_def("Green", COLOR_GREEN);
#endif
#ifdef COLOR_YELLOW
  color_def("Yellow", COLOR_YELLOW);
#endif
#ifdef COLOR_BLUE
  color_def("Blue", COLOR_BLUE);
#endif
#ifdef COLOR_MAGENTA
  color_def("Magenta", COLOR_MAGENTA);
#endif
#ifdef COLOR_CYAN
  color_def("Cyan", COLOR_CYAN);
#endif
#ifdef COLOR_WHITE
  color_def("White", COLOR_WHITE);
#endif
}

/*
 * Generate the linker options for the base facility
 */
static void
gen_linkopts(void)
{
  printf("   pragma Linker_Options (\"-lncurses%s\");\n", model);
}

/*
 * Generate the linker options for the menu facility
 */
static void
gen_menu_linkopts(void)
{
  printf("   pragma Linker_Options (\"-lmenu%s\");\n", model);
}

/*
 * Generate the linker options for the form facility
 */
static void
gen_form_linkopts(void)
{
  printf("   pragma Linker_Options (\"-lform%s\");\n", model);
}

/*
 * Generate the linker options for the panel facility
 */
static void
gen_panel_linkopts(void)
{
  printf("   pragma Linker_Options (\"-lpanel%s\");\n", model);
}

static void
gen_version_info(void)
{
  static const char *v1 =
  "   NC_Major_Version : constant := %d; --  Major version of the library\n";
  static const char *v2 =
  "   NC_Minor_Version : constant := %d; --  Minor version of the library\n";
  static const char *v3 =
  "   NC_Version : constant String := %c%d.%d%c;  --  Version of library\n";

  printf(v1, NCURSES_VERSION_MAJOR);
  printf(v2, NCURSES_VERSION_MINOR);
  printf(v3, '"', NCURSES_VERSION_MAJOR, NCURSES_VERSION_MINOR, '"');
}

static int
eti_gen(char *buf, int code, const char *name, int *etimin, int *etimax)
{
  sprintf(buf, "   E_%-16s : constant Eti_Error := %d;\n", name, code);
  if (code < *etimin)
    *etimin = code;
  if (code > *etimax)
    *etimax = code;
  return (int)strlen(buf);
}

static void
gen_offsets(void)
{
  const char *s_bool = "";

  if (sizeof(bool) == sizeof(char))
    {
      s_bool = "char";
    }
  else if (sizeof(bool) == sizeof(short))
    {
      s_bool = "short";
    }
  else if (sizeof(bool) == sizeof(int))
    {
      s_bool = "int";
    }
  printf("   Sizeof%-*s : constant Natural := %2ld; --  %s\n",
	 12, "_bool", (long)sizeof(bool), "bool");

  printf("   type Curses_Bool is mod 2 ** Interfaces.C.%s'Size;\n", s_bool);
}

/*
 * main() expects two arguments on the commandline, both single characters.
 * The first character denotes the facility for which we generate output.
 * Possible values are
 *   B - Base
 *   M - Menus
 *   F - Forms
 *   P - Pointer Device (Mouse)
 *   E - ETI base definitions
 *
 * The second character then denotes the specific output that should be
 * generated for the selected facility.
 */
int
main(int argc, char *argv[])
{
  int x = 0x12345678;
  char *s = (char *)&x;

  if (*s == 0x78)
    little_endian = 1;

  if (argc != 4)
    exit(1);
  model = *++argv;

  switch (argv[1][0])
    {
      /* --------------------------------------------------------------- */
    case 'B':			/* The Base facility */
      switch (argv[2][0])
	{
	case 'A':		/* chtype translation into Ada95 record type */
	  gen_attr_set("Character_Attribute_Set");
	  break;
	case 'B':		/* write some initial comment lines */
	  basedefs();
	  break;
	case 'C':		/* generate color constants */
	  gen_color();
	  break;
	case 'D':		/* generate displacements of fields in WINDOW struct. */
	  gen_offsets();
	  break;
	case 'E':		/* generate Mouse Event codes */
	  gen_mouse_events();
	  break;
	case 'K':		/* translation of keycodes */
	  gen_keydefs(0);
	  break;
	case 'L':		/* generate the Linker_Options pragma */
	  gen_linkopts();
	  break;
	case 'M':		/* generate constants for the ACS characters */
	  gen_acs();
	  break;
	case 'O':		/* generate definitions of the old key code names */
	  gen_keydefs(1);
	  break;
	case 'P':		/* generate definitions of the public variables */
	  gen_public_vars();
	  break;
	case 'R':		/* generate representation clause for Attributed character */
	  gen_chtype_rep("Attributed_Character");
	  break;
	case 'T':		/* generate the Trace info */
	  gen_trace("Trace_Attribute_Set");
	  break;
	case 'V':		/* generate version info */
	  gen_version_info();
	  break;
	default:
	  break;
	}
      break;
      /* --------------------------------------------------------------- */
    case 'M':			/* The Menu facility */
      switch (argv[2][0])
	{
	case 'R':		/* generate representation clause for Menu_Option_Set */
	  gen_menu_opt_rep("Menu_Option_Set");
	  break;
	case 'B':		/* write some initial comment lines */
	  menu_basedefs();
	  break;
	case 'L':		/* generate the Linker_Options pragma */
	  gen_menu_linkopts();
	  break;
	case 'I':		/* generate representation clause for Item_Option_Set */
	  gen_item_opt_rep("Item_Option_Set");
	  break;
	default:
	  break;
	}
      break;
      /* --------------------------------------------------------------- */
    case 'F':			/* The Form facility */
      switch (argv[2][0])
	{
	case 'R':		/* generate representation clause for Form_Option_Set */
	  gen_form_opt_rep("Form_Option_Set");
	  break;
	case 'B':		/* write some initial comment lines */
	  form_basedefs();
	  break;
	case 'L':		/* generate the Linker_Options pragma */
	  gen_form_linkopts();
	  break;
	case 'I':		/* generate representation clause for Field_Option_Set */
	  gen_field_opt_rep("Field_Option_Set");
	  break;
	default:
	  break;
	}
      break;
      /* --------------------------------------------------------------- */
    case 'P':			/* The Pointer(=Mouse) facility */
      switch (argv[2][0])
	{
	case 'B':		/* write some initial comment lines */
	  mouse_basedefs();
	  break;
	case 'M':		/* generate representation clause for Mouse_Event */
	  gen_mrep_rep("Mouse_Event");
	  break;
	case 'L':		/* generate the Linker_Options pragma */
	  gen_panel_linkopts();
	  break;
	default:
	  break;
	}
      break;
      /* --------------------------------------------------------------- */
    case 'E':			/* chtype size detection */
      switch (argv[2][0])
	{
	case 'C':
	  {
	    const char *fmt = "   type    C_Chtype   is new %s;\n";
	    const char *afmt = "   type    C_AttrType is new %s;\n";

	    if (sizeof(chtype) == sizeof(int))
	      {
		if (sizeof(int) == sizeof(long))
		    printf(fmt, "C_ULong");

		else
		  printf(fmt, "C_UInt");
	      }
	    else if (sizeof(chtype) == sizeof(long))
	      {
		printf(fmt, "C_ULong");
	      }
	    else
	      printf("Error\n");

	    if (sizeof(attr_t) == sizeof(int))
	      {
		if (sizeof(int) == sizeof(long))
		    printf(afmt, "C_ULong");

		else
		  printf(afmt, "C_UInt");
	      }
	    else if (sizeof(attr_t) == sizeof(long))
	      {
		printf(afmt, "C_ULong");
	      }
	    else
	      printf("Error\n");

	    printf("define(`CF_CURSES_OK',`%d')", OK);
	    printf("define(`CF_CURSES_ERR',`%d')", ERR);
	    printf("define(`CF_CURSES_TRUE',`%d')", TRUE);
	    printf("define(`CF_CURSES_FALSE',`%d')", FALSE);
	  }
	  break;
	case 'E':
	  {
	    char *buf = (char *)malloc(2048);
	    char *p = buf;
	    int etimin = E_OK;
	    int etimax = E_OK;

	    if (p)
	      {
		p += eti_gen(p, E_OK, "Ok", &etimin, &etimax);
		p += eti_gen(p, E_SYSTEM_ERROR, "System_Error", &etimin, &etimax);
		p += eti_gen(p, E_BAD_ARGUMENT, "Bad_Argument", &etimin, &etimax);
		p += eti_gen(p, E_POSTED, "Posted", &etimin, &etimax);
		p += eti_gen(p, E_CONNECTED, "Connected", &etimin, &etimax);
		p += eti_gen(p, E_BAD_STATE, "Bad_State", &etimin, &etimax);
		p += eti_gen(p, E_NO_ROOM, "No_Room", &etimin, &etimax);
		p += eti_gen(p, E_NOT_POSTED, "Not_Posted", &etimin, &etimax);
		p += eti_gen(p, E_UNKNOWN_COMMAND,
			     "Unknown_Command", &etimin, &etimax);
		p += eti_gen(p, E_NO_MATCH, "No_Match", &etimin, &etimax);
		p += eti_gen(p, E_NOT_SELECTABLE,
			     "Not_Selectable", &etimin, &etimax);
		p += eti_gen(p, E_NOT_CONNECTED,
			     "Not_Connected", &etimin, &etimax);
		p += eti_gen(p, E_REQUEST_DENIED,
			     "Request_Denied", &etimin, &etimax);
		p += eti_gen(p, E_INVALID_FIELD,
			     "Invalid_Field", &etimin, &etimax);
		p += eti_gen(p, E_CURRENT,
			     "Current", &etimin, &etimax);
	      }
	    printf("   subtype Eti_Error is C_Int range %d .. %d;\n\n",
		   etimin, etimax);
	    printf("%s", buf);
	  }
	  break;
	default:
	  break;
	}
      break;
      /* --------------------------------------------------------------- */
    case 'V':			/* plain version dump */
      {
	switch (argv[2][0])
	  {
	  case '1':		/* major version */
#ifdef NCURSES_VERSION_MAJOR
	    printf("%d", NCURSES_VERSION_MAJOR);
#endif
	    break;
	  case '2':		/* minor version */
#ifdef NCURSES_VERSION_MINOR
	    printf("%d", NCURSES_VERSION_MINOR);
#endif
	    break;
	  case '3':		/* patch level */
#ifdef NCURSES_VERSION_PATCH
	    printf("%d", NCURSES_VERSION_PATCH);
#endif
	    break;
	  default:
	    break;
	  }
      }
      break;
      /* --------------------------------------------------------------- */
    default:
      break;
    }
  return 0;
}
