// -*- C++ -*-
/* Copyright (C) 1989, 1990, 1991, 1992 Free Software Foundation, Inc.
     Written by James Clark (jjc@jclark.com)

This file is part of groff.

groff is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free
Software Foundation; either version 2, or (at your option) any later
version.

groff is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License along
with groff; see the file COPYING.  If not, write to the Free Software
Foundation, 675 Mass Ave, Cambridge, MA 02139, USA. */

#include "eqn.h"
#include "pbox.h"

enum left_or_right_t { LEFT_DELIM = 01, RIGHT_DELIM = 02 };

// Small must be none-zero and must exist in each device.
// Small will be put in the roman font, others are assumed to be
// on the special font (so no font change will be necessary.)

struct delimiter {
  const char *name;
  int flags;
  const char *small;
  const char *chain_format;
  const char *ext;
  const char *top;
  const char *mid;
  const char *bot;
} delim_table[] = {
  {
    "(", LEFT_DELIM|RIGHT_DELIM, "(", "\\[parenleft%s]",
    "\\[parenleftex]",
    "\\[parenlefttp]",
    0,
    "\\[parenleftbt]",
  },
  {
    ")", LEFT_DELIM|RIGHT_DELIM, ")", "\\[parenright%s]",
    "\\[parenrightex]",
    "\\[parenrighttp]",
    0,
    "\\[parenrightbt]",
  },
  {
    "[", LEFT_DELIM|RIGHT_DELIM, "[", "\\[bracketleft%s]",
    "\\[bracketleftex]",
    "\\[bracketlefttp]",
    0,
    "\\[bracketleftbt]",
  },
  {
    "]", LEFT_DELIM|RIGHT_DELIM, "]", "\\[bracketright%s]",
    "\\[bracketrightex]",
    "\\[bracketrighttp]",
    0,
    "\\[bracketrightbt]",
  },
  {
    "{", LEFT_DELIM|RIGHT_DELIM, "{", "\\[braceleft%s]",
    "\\[braceleftex]",
    "\\[bracelefttp]",
    "\\[braceleftmid]",
    "\\[braceleftbt]",
  },
  {
    "}", LEFT_DELIM|RIGHT_DELIM, "}", "\\[braceright%s]",
    "\\[bracerightex]",
    "\\[bracerighttp]",
    "\\[bracerightmid]",
    "\\[bracerightbt]",
  },
  {
    "|", LEFT_DELIM|RIGHT_DELIM, "|", "\\[bar%s]",
    "\\[barex]",
  },
  {
    "floor", LEFT_DELIM, "\\(lf", "\\[floorleft%s]",
    "\\[bracketleftex]",
    0,
    0,
    "\\[bracketleftbt]",
  },
  {
    "floor", RIGHT_DELIM, "\\(rf", "\\[floorright%s]",
    "\\[bracketrightex]",
    0,
    0,
    "\\[bracketrightbt]",
  },
  {
    "ceiling", LEFT_DELIM, "\\(lc", "\\[ceilingleft%s]",
    "\\[bracketleftex]",
    "\\[bracketlefttp]",
  },
  {
    "ceiling", RIGHT_DELIM, "\\(rc", "\\[ceilingright%s]",
    "\\[bracketrightex]",
    "\\[bracketrighttp]",
  },
  {
    "||", LEFT_DELIM|RIGHT_DELIM, "|", "\\[bar%s]",
    "\\[bardblex]",
  },
  {
    "<", LEFT_DELIM|RIGHT_DELIM, "\\(la", "\\[angleleft%s]",
  },
  {
    ">", LEFT_DELIM|RIGHT_DELIM, "\\(ra", "\\[angleright%s]",
  },
  {
    "uparrow", LEFT_DELIM|RIGHT_DELIM, "\\(ua", "\\[arrowup%s]",
    "\\[arrowvertex]",
    "\\[arrowverttp]",
  },
  {
    "downarrow", LEFT_DELIM|RIGHT_DELIM, "\\(da", "\\[arrowdown%s]",
    "\\[arrowvertex]",
    0,
    0,
    "\\[arrowvertbt]",
  },
  {
    "updownarrow", LEFT_DELIM|RIGHT_DELIM, "\\(va", "\\[arrowupdown%s]",
    "\\[arrowvertex]",
    "\\[arrowverttp]",
    0,
    "\\[arrowvertbt]",
  },
};

const int DELIM_TABLE_SIZE = int(sizeof(delim_table)/sizeof(delim_table[0]));

class delim_box : public box {
private:
  char *left;
  char *right;
  box *p;
public:
  delim_box(char *, box *, char *);
  ~delim_box();
  int compute_metrics(int);
  void output();
  void check_tabs(int);
  void debug_print();
};

box *make_delim_box(char *l, box *pp, char *r)
{
  if (l != 0 && *l == '\0') {
    a_delete l;
    l = 0;
  }
  if (r != 0 && *r == '\0') {
    a_delete r;
    r = 0;
  }
  return new delim_box(l, pp, r);
}

delim_box::delim_box(char *l, box *pp, char *r)
: left(l), right(r), p(pp)
{
}

delim_box::~delim_box()
{
  a_delete left;
  a_delete right;
  delete p;
}

static void build_extensible(const char *ext, const char *top, const char *mid,
			     const char *bot)
{
  assert(ext != 0);
  printf(".nr " DELIM_WIDTH_REG " 0\\w" DELIMITER_CHAR "%s" DELIMITER_CHAR "\n",
	 ext);
  printf(".nr " EXT_HEIGHT_REG " 0\\n[rst]\n");
  printf(".nr " EXT_DEPTH_REG " 0-\\n[rsb]\n");
  if (top) {
    printf(".nr " DELIM_WIDTH_REG " 0\\n[" DELIM_WIDTH_REG "]"
	   ">?\\w" DELIMITER_CHAR "%s" DELIMITER_CHAR "\n",
	   top);
    printf(".nr " TOP_HEIGHT_REG " 0\\n[rst]\n");
    printf(".nr " TOP_DEPTH_REG " 0-\\n[rsb]\n");
  }
  if (mid) {
    printf(".nr " DELIM_WIDTH_REG " 0\\n[" DELIM_WIDTH_REG "]"
	   ">?\\w" DELIMITER_CHAR "%s" DELIMITER_CHAR "\n",
	   mid);
    printf(".nr " MID_HEIGHT_REG " 0\\n[rst]\n");
    printf(".nr " MID_DEPTH_REG " 0-\\n[rsb]\n");
  }
  if (bot) {
    printf(".nr " DELIM_WIDTH_REG " 0\\n[" DELIM_WIDTH_REG "]"
	   ">?\\w" DELIMITER_CHAR "%s" DELIMITER_CHAR "\n",
	   bot);
    printf(".nr " BOT_HEIGHT_REG " 0\\n[rst]\n");
    printf(".nr " BOT_DEPTH_REG " 0-\\n[rsb]\n");
  }
  printf(".nr " TOTAL_HEIGHT_REG " 0");
  if (top)
    printf("+\\n[" TOP_HEIGHT_REG "]+\\n[" TOP_DEPTH_REG "]");
  if (bot)
    printf("+\\n[" BOT_HEIGHT_REG "]+\\n[" BOT_DEPTH_REG "]");
  if (mid)
    printf("+\\n[" MID_HEIGHT_REG "]+\\n[" MID_DEPTH_REG "]");
  printf("\n");
  // determine how many extensible characters we need
  printf(".nr " TEMP_REG " \\n[" DELTA_REG "]-\\n[" TOTAL_HEIGHT_REG "]");
  if (mid)
    printf("/2");
  printf(">?0+\\n[" EXT_HEIGHT_REG "]+\\n[" EXT_DEPTH_REG "]-1/(\\n["
	 EXT_HEIGHT_REG "]+\\n[" EXT_DEPTH_REG "])\n");
  
  printf(".nr " TOTAL_HEIGHT_REG " +(\\n[" EXT_HEIGHT_REG "]+\\n["
	 EXT_DEPTH_REG "]*\\n[" TEMP_REG "]");
  if (mid)
    printf("*2");
  printf(")\n");
  printf(".ds " DELIM_STRING " \\Z" DELIMITER_CHAR
	 "\\v'-%dM-(\\n[" TOTAL_HEIGHT_REG "]u/2u)'\n",
	 axis_height);
  if (top)
    printf(".as " DELIM_STRING " \\v'\\n[" TOP_HEIGHT_REG "]u'"
	   "\\Z" DELIMITER_CHAR "%s" DELIMITER_CHAR
	   "\\v'\\n[" TOP_DEPTH_REG "]u'\n",
	   top);

  // this macro appends $2 copies of $3 to string $1
  printf(".de " REPEAT_APPEND_STRING_MACRO "\n"
	 ".if \\\\$2 \\{.as \\\\$1 \"\\\\$3\n"
	 "." REPEAT_APPEND_STRING_MACRO " \\\\$1 \\\\$2-1 \"\\\\$3\"\n"
	 ".\\}\n"
	 "..\n");

  printf("." REPEAT_APPEND_STRING_MACRO " " DELIM_STRING " \\n[" TEMP_REG "] "
	 "\\v'\\n[" EXT_HEIGHT_REG "]u'"
	 "\\Z" DELIMITER_CHAR "%s" DELIMITER_CHAR 
	 "\\v'\\n[" EXT_DEPTH_REG "]u'\n",
	 ext);

  if (mid) {
    printf(".as " DELIM_STRING " \\v'\\n[" MID_HEIGHT_REG "]u'"
	   "\\Z" DELIMITER_CHAR "%s" DELIMITER_CHAR
	   "\\v'\\n[" MID_DEPTH_REG "]u'\n",
	   mid);
    printf("." REPEAT_APPEND_STRING_MACRO " " DELIM_STRING 
	   " \\n[" TEMP_REG "] "
	   "\\v'\\n[" EXT_HEIGHT_REG "]u'"
	   "\\Z" DELIMITER_CHAR "%s" DELIMITER_CHAR
	   "\\v'\\n[" EXT_DEPTH_REG "]u'\n",
	   ext);
  }
  if (bot)
    printf(".as " DELIM_STRING " \\v'\\n[" BOT_HEIGHT_REG "]u'"
	   "\\Z" DELIMITER_CHAR "%s" DELIMITER_CHAR
	   "\\v'\\n[" BOT_DEPTH_REG "]u'\n",
	   bot);
  printf(".as " DELIM_STRING " " DELIMITER_CHAR "\n");
}

static void define_extensible_string(char *delim, int uid,
				     left_or_right_t left_or_right)
{
  printf(".ds " DELIM_STRING "\n");
  delimiter *d = delim_table;
  int delim_len = strlen(delim);
  for (int i = 0; i < DELIM_TABLE_SIZE; i++, d++)
    if (strncmp(delim, d->name, delim_len) == 0 
	&& (left_or_right & d->flags) != 0)
      break;
  if (i >= DELIM_TABLE_SIZE) {
    error("there is no `%1' delimiter", delim);
    printf(".nr " DELIM_WIDTH_REG " 0\n");
    return;
  }

  printf(".nr " DELIM_WIDTH_REG " 0\\w" DELIMITER_CHAR "\\f[%s]%s\\fP" DELIMITER_CHAR "\n"
	 ".ds " DELIM_STRING " \\Z" DELIMITER_CHAR
	   "\\v'\\n[rsb]u+\\n[rst]u/2u-%dM'\\f[%s]%s\\fP" DELIMITER_CHAR "\n"
	 ".nr " TOTAL_HEIGHT_REG " \\n[rst]-\\n[rsb]\n"
	 ".if \\n[" TOTAL_HEIGHT_REG "]<\\n[" DELTA_REG "] "
	 "\\{",
	 current_roman_font, d->small, axis_height,
	 current_roman_font, d->small);
	 
  char buf[256];
  sprintf(buf, d->chain_format, "\\\\n[" INDEX_REG "]");
  printf(".nr " INDEX_REG " 0\n"
	 ".de " TEMP_MACRO "\n"
	 ".ie c%s \\{\\\n"
	 ".nr " DELIM_WIDTH_REG " 0\\w" DELIMITER_CHAR "%s" DELIMITER_CHAR "\n"
	 ".ds " DELIM_STRING " \\Z" DELIMITER_CHAR
	   "\\v'\\\\n[rsb]u+\\\\n[rst]u/2u-%dM'%s" DELIMITER_CHAR "\n"
	 ".nr " TOTAL_HEIGHT_REG " \\\\n[rst]-\\\\n[rsb]\n"
	 ".if \\\\n[" TOTAL_HEIGHT_REG "]<\\n[" DELTA_REG "] "
	 "\\{.nr " INDEX_REG " +1\n"
	 "." TEMP_MACRO "\n"
	 ".\\}\\}\n"
	 ".el .nr " INDEX_REG " 0-1\n"
	 "..\n"
	 "." TEMP_MACRO "\n",
	 buf, buf, axis_height, buf);
  if (d->ext) {
    printf(".if \\n[" INDEX_REG "]<0 \\{.if c%s \\{\\\n", d->ext);
    build_extensible(d->ext, d->top, d->mid, d->bot);
    printf(".\\}\\}\n");
  }
  printf(".\\}\n");
  printf(".as " DELIM_STRING " \\h'\\n[" DELIM_WIDTH_REG "]u'\n");
  printf(".nr " WIDTH_FORMAT " +\\n[" DELIM_WIDTH_REG "]\n", uid);
  printf(".nr " HEIGHT_FORMAT " \\n[" HEIGHT_FORMAT "]"
	 ">?(\\n[" TOTAL_HEIGHT_REG "]/2+%dM)\n",
	 uid, uid, axis_height);
  printf(".nr " DEPTH_FORMAT " \\n[" DEPTH_FORMAT "]"
	 ">?(\\n[" TOTAL_HEIGHT_REG "]/2-%dM)\n",
	 uid, uid, axis_height);
}

int delim_box::compute_metrics(int style)
{
  int r = p->compute_metrics(style);
  printf(".nr " WIDTH_FORMAT " 0\\n[" WIDTH_FORMAT "]\n", uid, p->uid);
  printf(".nr " HEIGHT_FORMAT " \\n[" HEIGHT_FORMAT "]\n", uid, p->uid);
  printf(".nr " DEPTH_FORMAT " \\n[" DEPTH_FORMAT "]\n", uid, p->uid);
  printf(".nr " DELTA_REG " \\n[" HEIGHT_FORMAT "]-%dM"
	 ">?(\\n[" DEPTH_FORMAT "]+%dM)\n",
	 p->uid, axis_height, p->uid, axis_height);
  printf(".nr " DELTA_REG " 0\\n[" DELTA_REG "]*%d/500"
	 ">?(\\n[" DELTA_REG "]*2-%dM)\n",
	 delimiter_factor, delimiter_shortfall);
  if (left) {
    define_extensible_string(left, uid, LEFT_DELIM);
    printf(".rn " DELIM_STRING " " LEFT_DELIM_STRING_FORMAT "\n",
	   uid);
  }
  if (r)
    printf(".nr " MARK_REG " +\\n[" DELIM_WIDTH_REG "]\n");
  if (right) {
    define_extensible_string(right, uid, RIGHT_DELIM);
    printf(".rn " DELIM_STRING " " RIGHT_DELIM_STRING_FORMAT "\n",
	   uid);
  }
  return r;
}

void delim_box::output()
{
  if (left)
    printf("\\*[" LEFT_DELIM_STRING_FORMAT "]", uid);
  p->output();
  if (right)
    printf("\\*[" RIGHT_DELIM_STRING_FORMAT "]", uid);
}

void delim_box::check_tabs(int level)
{
  p->check_tabs(level);
}

void delim_box::debug_print()
{
  fprintf(stderr, "left \"%s\" { ", left ? left : "");
  p->debug_print();
  fprintf(stderr, " }");
  if (right)
    fprintf(stderr, " right \"%s\"", right);
}

