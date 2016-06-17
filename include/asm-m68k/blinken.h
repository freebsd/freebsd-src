/*
** asm/blinken.h -- m68k blinkenlights support (currently hp300 only)
**
** (c) 1998 Phil Blundell <philb@gnu.org>
**
** This file is subject to the terms and conditions of the GNU General Public
** License.  See the file COPYING in the main directory of this archive
** for more details.
**
*/

#ifndef _M68K_BLINKEN_H
#define _M68K_BLINKEN_H

#include <asm/setup.h>

#define HP300_LEDS		0xf001ffff

static __inline__ void blinken_leds(int x)
{
  if (MACH_IS_HP300)
  {
    *((volatile unsigned char *)HP300_LEDS) = (x);
  }
}

#endif
