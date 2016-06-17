/*
 * arch/mips/lib/console.c
 *
 * Copyright (C) 1994 by Waldorf Electronic,
 * written by Ralf Baechle and Andreas Busse
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
 * FIXME: This file is hacked to be hardwired for the Deskstation
 *        Only thought as a debugging console output.  It's as inefficient
 *        as a piece of code can be but probably a good piece of code to
 *        implement a preliminary console for a new target.
 */

#include <linux/tty.h>
#include <asm/bootinfo.h>

static unsigned int size_x;
static unsigned int size_y;
static unsigned short cursor_x;
static unsigned short cursor_y;
static volatile unsigned short *vram_addr;
static int console_needs_init = 1;

extern struct screen_info screen_info;

/* ----------------------------------------------------------------------
 * init_console()
 * ---------------------------------------------------------------------- */

void init_console(void)
{
  size_x = 80;
  size_y = 25;
  cursor_x = 0;
  cursor_y = 0;

  vram_addr = (unsigned short *)0xb00b8000;

  console_needs_init = 0;
}

void
set_size_x(unsigned int x)
{
  size_x = x;
}

void
set_size_y(unsigned int y)
{
  size_y = y;
}

void
set_vram(unsigned short *vram)
{
  vram_addr = vram;
}

void
set_crsr(unsigned int x, unsigned int y)
{
  cursor_x = x;
  cursor_y = y;
}

void
print_char(unsigned int x, unsigned int y, unsigned char c)
{
  volatile unsigned short *caddr;

  caddr = vram_addr + (y * size_x) + x;
  *caddr = (*caddr & 0xff00) | 0x0f00 | (unsigned short) c;
}

static void
scroll(void)
{
  volatile unsigned short *caddr;
  register int i;

  caddr = vram_addr;
  for(i=0; i<size_x * (size_y-1); i++)
    *(caddr++) = *(caddr + size_x);

  /* blank last line */

  caddr = vram_addr + (size_x * (size_y-1));
  for(i=0; i<size_x; i++)
    *(caddr++) = (*caddr & 0xff00) | (unsigned short) ' ';
}

void print_string(const unsigned char *str)
{
  unsigned char c;

  if (console_needs_init)
    init_console();

  while((c = *str++))
    switch(c)
      {
      case '\n':
	cursor_x = 0;
	cursor_y++;
	if(cursor_y == size_y)
	  {
	    scroll();
	    cursor_y = size_y - 1;
	  }
	break;

      default:
	print_char(cursor_x, cursor_y, c);
	cursor_x++;
	if(cursor_x == size_x)
	  {
	    cursor_x = 0;
	    cursor_y++;
	    if(cursor_y == size_y)
	      {
		scroll();
		cursor_y = size_y - 1;
	      }
	  }
	break;
      }
}

/* end of file */
