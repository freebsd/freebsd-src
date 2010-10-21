/* Instruction printing code for the ARM
   Copyright 1994, 1995, 1996, 1997, 1998, 1999, 2000, 2001, 2002, 2003, 2004
   Free Software Foundation, Inc.
   Contributed by Richard Earnshaw (rwe@pegasus.esprit.ec.org)
   Modification by James G. Smith (jsmith@cygnus.co.uk)

   This file is part of libopcodes.

   This program is free software; you can redistribute it and/or modify it under
   the terms of the GNU General Public License as published by the Free
   Software Foundation; either version 2 of the License, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful, but WITHOUT
   ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
   FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
   more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street - Fifth Floor, Boston, MA 02110-1301, USA.  */

#include "sysdep.h"

#include "dis-asm.h"
#include "opcode/arm.h"
#include "opintl.h"
#include "safe-ctype.h"

/* FIXME: This shouldn't be done here.  */
#include "coff/internal.h"
#include "libcoff.h"
#include "elf-bfd.h"
#include "elf/internal.h"
#include "elf/arm.h"

/* FIXME: Belongs in global header.  */
#ifndef strneq
#define strneq(a,b,n)	(strncmp ((a), (b), (n)) == 0)
#endif

#ifndef NUM_ELEM
#define NUM_ELEM(a)     (sizeof (a) / sizeof (a)[0])
#endif

struct opcode32
{
  unsigned long arch;		/* Architecture defining this insn.  */
  unsigned long value, mask;	/* Recognise insn if (op&mask)==value.  */
  const char *assembler;	/* How to disassemble this insn.  */
};

struct opcode16
{
  unsigned long arch;		/* Architecture defining this insn.  */
  unsigned short value, mask;	/* Recognise insn if (op&mask)==value.  */
  const char *assembler;	/* How to disassemble this insn.  */
};

/* print_insn_coprocessor recognizes the following format control codes:

   %%			%

   %c			print condition code (always bits 28-31)
   %A			print address for ldc/stc/ldf/stf instruction
   %I                   print cirrus signed shift immediate: bits 0..3|4..6
   %F			print the COUNT field of a LFM/SFM instruction.
   %P			print floating point precision in arithmetic insn
   %Q			print floating point precision in ldf/stf insn
   %R			print floating point rounding mode

   %<bitfield>r		print as an ARM register
   %<bitfield>d		print the bitfield in decimal
   %<bitfield>x		print the bitfield in hex
   %<bitfield>X		print the bitfield as 1 hex digit without leading "0x"
   %<bitfield>f		print a floating point constant if >7 else a
			floating point register
   %<bitfield>w         print as an iWMMXt width field - [bhwd]ss/us
   %<bitfield>g         print as an iWMMXt 64-bit register
   %<bitfield>G         print as an iWMMXt general purpose or control register

   %<code>y		print a single precision VFP reg.
			  Codes: 0=>Sm, 1=>Sd, 2=>Sn, 3=>multi-list, 4=>Sm pair
   %<code>z		print a double precision VFP reg
			  Codes: 0=>Dm, 1=>Dd, 2=>Dn, 3=>multi-list
   %<bitnum>'c		print specified char iff bit is one
   %<bitnum>`c		print specified char iff bit is zero
   %<bitnum>?ab		print a if bit is one else print b

   %L			print as an iWMMXt N/M width field.
   %Z			print the Immediate of a WSHUFH instruction.
   %l			like 'A' except use byte offsets for 'B' & 'H'
			versions.  */

/* Common coprocessor opcodes shared between Arm and Thumb-2.  */

static const struct opcode32 coprocessor_opcodes[] =
{
  /* XScale instructions.  */
  {ARM_CEXT_XSCALE, 0x0e200010, 0x0fff0ff0, "mia%c\tacc0, %0-3r, %12-15r"},
  {ARM_CEXT_XSCALE, 0x0e280010, 0x0fff0ff0, "miaph%c\tacc0, %0-3r, %12-15r"},
  {ARM_CEXT_XSCALE, 0x0e2c0010, 0x0ffc0ff0, "mia%17'T%17`B%16'T%16`B%c\tacc0, %0-3r, %12-15r"},
  {ARM_CEXT_XSCALE, 0x0c400000, 0x0ff00fff, "mar%c\tacc0, %12-15r, %16-19r"},
  {ARM_CEXT_XSCALE, 0x0c500000, 0x0ff00fff, "mra%c\t%12-15r, %16-19r, acc0"},
    
  /* Intel Wireless MMX technology instructions.  */
#define FIRST_IWMMXT_INSN 0x0e130130
#define IWMMXT_INSN_COUNT 47
  {ARM_CEXT_IWMMXT, 0x0e130130, 0x0f3f0fff, "tandc%22-23w%c\t%12-15r"},
  {ARM_CEXT_XSCALE, 0x0e400010, 0x0ff00f3f, "tbcst%6-7w%c\t%16-19g, %12-15r"},
  {ARM_CEXT_XSCALE, 0x0e130170, 0x0f3f0ff8, "textrc%22-23w%c\t%12-15r, #%0-2d"},
  {ARM_CEXT_XSCALE, 0x0e100070, 0x0f300ff0, "textrm%3?su%22-23w%c\t%12-15r, %16-19g, #%0-2d"},
  {ARM_CEXT_XSCALE, 0x0e600010, 0x0ff00f38, "tinsr%6-7w%c\t%16-19g, %12-15r, #%0-2d"},
  {ARM_CEXT_XSCALE, 0x0e000110, 0x0ff00fff, "tmcr%c\t%16-19G, %12-15r"},
  {ARM_CEXT_XSCALE, 0x0c400000, 0x0ff00ff0, "tmcrr%c\t%0-3g, %12-15r, %16-19r"},
  {ARM_CEXT_XSCALE, 0x0e2c0010, 0x0ffc0e10, "tmia%17?tb%16?tb%c\t%5-8g, %0-3r, %12-15r"},
  {ARM_CEXT_XSCALE, 0x0e200010, 0x0fff0e10, "tmia%c\t%5-8g, %0-3r, %12-15r"},
  {ARM_CEXT_XSCALE, 0x0e280010, 0x0fff0e10, "tmiaph%c\t%5-8g, %0-3r, %12-15r"},
  {ARM_CEXT_XSCALE, 0x0e100030, 0x0f300fff, "tmovmsk%22-23w%c\t%12-15r, %16-19g"},
  {ARM_CEXT_XSCALE, 0x0e100110, 0x0ff00ff0, "tmrc%c\t%12-15r, %16-19G"},
  {ARM_CEXT_XSCALE, 0x0c500000, 0x0ff00ff0, "tmrrc%c\t%12-15r, %16-19r, %0-3g"},
  {ARM_CEXT_XSCALE, 0x0e130150, 0x0f3f0fff, "torc%22-23w%c\t%12-15r"},
  {ARM_CEXT_XSCALE, 0x0e0001c0, 0x0f300fff, "wacc%22-23w%c\t%12-15g, %16-19g"},
  {ARM_CEXT_XSCALE, 0x0e000180, 0x0f000ff0, "wadd%20-23w%c\t%12-15g, %16-19g, %0-3g"},
  {ARM_CEXT_XSCALE, 0x0e000020, 0x0f800ff0, "waligni%c\t%12-15g, %16-19g, %0-3g, #%20-22d"},
  {ARM_CEXT_XSCALE, 0x0e800020, 0x0fc00ff0, "walignr%20-21d%c\t%12-15g, %16-19g, %0-3g"},
  {ARM_CEXT_XSCALE, 0x0e200000, 0x0fe00ff0, "wand%20'n%c\t%12-15g, %16-19g, %0-3g"},
  {ARM_CEXT_XSCALE, 0x0e800000, 0x0fa00ff0, "wavg2%22?hb%20'r%c\t%12-15g, %16-19g, %0-3g"},
  {ARM_CEXT_XSCALE, 0x0e000060, 0x0f300ff0, "wcmpeq%22-23w%c\t%12-15g, %16-19g, %0-3g"},
  {ARM_CEXT_XSCALE, 0x0e100060, 0x0f100ff0, "wcmpgt%21?su%22-23w%c\t%12-15g, %16-19g, %0-3g"},
  {ARM_CEXT_XSCALE, 0xfc100100, 0xfe500f00, "wldrw\t%12-15G, %A"},
  {ARM_CEXT_XSCALE, 0x0c100000, 0x0e100e00, "wldr%L%c\t%12-15g, %l"},
  {ARM_CEXT_XSCALE, 0x0e400100, 0x0fc00ff0, "wmac%21?su%20'z%c\t%12-15g, %16-19g, %0-3g"},
  {ARM_CEXT_XSCALE, 0x0e800100, 0x0fd00ff0, "wmadd%21?su%c\t%12-15g, %16-19g, %0-3g"},
  {ARM_CEXT_XSCALE, 0x0e000160, 0x0f100ff0, "wmax%21?su%22-23w%c\t%12-15g, %16-19g, %0-3g"},
  {ARM_CEXT_XSCALE, 0x0e100160, 0x0f100ff0, "wmin%21?su%22-23w%c\t%12-15g, %16-19g, %0-3g"},
  {ARM_CEXT_XSCALE, 0x0e000100, 0x0fc00ff0, "wmul%21?su%20?ml%c\t%12-15g, %16-19g, %0-3g"},
  {ARM_CEXT_XSCALE, 0x0e000000, 0x0ff00ff0, "wor%c\t%12-15g, %16-19g, %0-3g"},
  {ARM_CEXT_XSCALE, 0x0e000080, 0x0f000ff0, "wpack%20-23w%c\t%12-15g, %16-19g, %0-3g"},
  {ARM_CEXT_XSCALE, 0x0e300040, 0x0f300ff0, "wror%22-23w%8'g%c\t%12-15g, %16-19g, %0-3g"},
  {ARM_CEXT_XSCALE, 0x0e300148, 0x0f300ffc, "wror%22-23w%8'g%c\t%12-15g, %16-19g, %0-3G"},
  {ARM_CEXT_XSCALE, 0x0e000120, 0x0fa00ff0, "wsad%22?hb%20'z%c\t%12-15g, %16-19g, %0-3g"},
  {ARM_CEXT_XSCALE, 0x0e0001e0, 0x0f000ff0, "wshufh%c\t%12-15g, %16-19g, #%Z"},
  {ARM_CEXT_XSCALE, 0x0e100040, 0x0f300ff0, "wsll%22-23w%8'g%c\t%12-15g, %16-19g, %0-3g"},
  {ARM_CEXT_XSCALE, 0x0e100148, 0x0f300ffc, "wsll%22-23w%8'g%c\t%12-15g, %16-19g, %0-3G"},
  {ARM_CEXT_XSCALE, 0x0e000040, 0x0f300ff0, "wsra%22-23w%8'g%c\t%12-15g, %16-19g, %0-3g"},
  {ARM_CEXT_XSCALE, 0x0e000148, 0x0f300ffc, "wsra%22-23w%8'g%c\t%12-15g, %16-19g, %0-3G"},
  {ARM_CEXT_XSCALE, 0x0e200040, 0x0f300ff0, "wsrl%22-23w%8'g%c\t%12-15g, %16-19g, %0-3g"},
  {ARM_CEXT_XSCALE, 0x0e200148, 0x0f300ffc, "wsrl%22-23w%8'g%c\t%12-15g, %16-19g, %0-3G"},
  {ARM_CEXT_XSCALE, 0xfc000100, 0xfe500f00, "wstrw\t%12-15G, %A"},
  {ARM_CEXT_XSCALE, 0x0c000000, 0x0e100e00, "wstr%L%c\t%12-15g, %l"},
  {ARM_CEXT_XSCALE, 0x0e0001a0, 0x0f000ff0, "wsub%20-23w%c\t%12-15g, %16-19g, %0-3g"},
  {ARM_CEXT_XSCALE, 0x0e0000c0, 0x0f100fff, "wunpckeh%21?su%22-23w%c\t%12-15g, %16-19g"},
  {ARM_CEXT_XSCALE, 0x0e0000e0, 0x0f100fff, "wunpckel%21?su%22-23w%c\t%12-15g, %16-19g"},
  {ARM_CEXT_XSCALE, 0x0e1000c0, 0x0f300ff0, "wunpckih%22-23w%c\t%12-15g, %16-19g, %0-3g"},
  {ARM_CEXT_XSCALE, 0x0e1000e0, 0x0f300ff0, "wunpckil%22-23w%c\t%12-15g, %16-19g, %0-3g"},
  {ARM_CEXT_XSCALE, 0x0e100000, 0x0ff00ff0, "wxor%c\t%12-15g, %16-19g, %0-3g"},

  /* Floating point coprocessor (FPA) instructions */
  {FPU_FPA_EXT_V1, 0x0e000100, 0x0ff08f10, "adf%c%P%R\t%12-14f, %16-18f, %0-3f"},
  {FPU_FPA_EXT_V1, 0x0e100100, 0x0ff08f10, "muf%c%P%R\t%12-14f, %16-18f, %0-3f"},
  {FPU_FPA_EXT_V1, 0x0e200100, 0x0ff08f10, "suf%c%P%R\t%12-14f, %16-18f, %0-3f"},
  {FPU_FPA_EXT_V1, 0x0e300100, 0x0ff08f10, "rsf%c%P%R\t%12-14f, %16-18f, %0-3f"},
  {FPU_FPA_EXT_V1, 0x0e400100, 0x0ff08f10, "dvf%c%P%R\t%12-14f, %16-18f, %0-3f"},
  {FPU_FPA_EXT_V1, 0x0e500100, 0x0ff08f10, "rdf%c%P%R\t%12-14f, %16-18f, %0-3f"},
  {FPU_FPA_EXT_V1, 0x0e600100, 0x0ff08f10, "pow%c%P%R\t%12-14f, %16-18f, %0-3f"},
  {FPU_FPA_EXT_V1, 0x0e700100, 0x0ff08f10, "rpw%c%P%R\t%12-14f, %16-18f, %0-3f"},
  {FPU_FPA_EXT_V1, 0x0e800100, 0x0ff08f10, "rmf%c%P%R\t%12-14f, %16-18f, %0-3f"},
  {FPU_FPA_EXT_V1, 0x0e900100, 0x0ff08f10, "fml%c%P%R\t%12-14f, %16-18f, %0-3f"},
  {FPU_FPA_EXT_V1, 0x0ea00100, 0x0ff08f10, "fdv%c%P%R\t%12-14f, %16-18f, %0-3f"},
  {FPU_FPA_EXT_V1, 0x0eb00100, 0x0ff08f10, "frd%c%P%R\t%12-14f, %16-18f, %0-3f"},
  {FPU_FPA_EXT_V1, 0x0ec00100, 0x0ff08f10, "pol%c%P%R\t%12-14f, %16-18f, %0-3f"},
  {FPU_FPA_EXT_V1, 0x0e008100, 0x0ff08f10, "mvf%c%P%R\t%12-14f, %0-3f"},
  {FPU_FPA_EXT_V1, 0x0e108100, 0x0ff08f10, "mnf%c%P%R\t%12-14f, %0-3f"},
  {FPU_FPA_EXT_V1, 0x0e208100, 0x0ff08f10, "abs%c%P%R\t%12-14f, %0-3f"},
  {FPU_FPA_EXT_V1, 0x0e308100, 0x0ff08f10, "rnd%c%P%R\t%12-14f, %0-3f"},
  {FPU_FPA_EXT_V1, 0x0e408100, 0x0ff08f10, "sqt%c%P%R\t%12-14f, %0-3f"},
  {FPU_FPA_EXT_V1, 0x0e508100, 0x0ff08f10, "log%c%P%R\t%12-14f, %0-3f"},
  {FPU_FPA_EXT_V1, 0x0e608100, 0x0ff08f10, "lgn%c%P%R\t%12-14f, %0-3f"},
  {FPU_FPA_EXT_V1, 0x0e708100, 0x0ff08f10, "exp%c%P%R\t%12-14f, %0-3f"},
  {FPU_FPA_EXT_V1, 0x0e808100, 0x0ff08f10, "sin%c%P%R\t%12-14f, %0-3f"},
  {FPU_FPA_EXT_V1, 0x0e908100, 0x0ff08f10, "cos%c%P%R\t%12-14f, %0-3f"},
  {FPU_FPA_EXT_V1, 0x0ea08100, 0x0ff08f10, "tan%c%P%R\t%12-14f, %0-3f"},
  {FPU_FPA_EXT_V1, 0x0eb08100, 0x0ff08f10, "asn%c%P%R\t%12-14f, %0-3f"},
  {FPU_FPA_EXT_V1, 0x0ec08100, 0x0ff08f10, "acs%c%P%R\t%12-14f, %0-3f"},
  {FPU_FPA_EXT_V1, 0x0ed08100, 0x0ff08f10, "atn%c%P%R\t%12-14f, %0-3f"},
  {FPU_FPA_EXT_V1, 0x0ee08100, 0x0ff08f10, "urd%c%P%R\t%12-14f, %0-3f"},
  {FPU_FPA_EXT_V1, 0x0ef08100, 0x0ff08f10, "nrm%c%P%R\t%12-14f, %0-3f"},
  {FPU_FPA_EXT_V1, 0x0e000110, 0x0ff00f1f, "flt%c%P%R\t%16-18f, %12-15r"},
  {FPU_FPA_EXT_V1, 0x0e100110, 0x0fff0f98, "fix%c%R\t%12-15r, %0-2f"},
  {FPU_FPA_EXT_V1, 0x0e200110, 0x0fff0fff, "wfs%c\t%12-15r"},
  {FPU_FPA_EXT_V1, 0x0e300110, 0x0fff0fff, "rfs%c\t%12-15r"},
  {FPU_FPA_EXT_V1, 0x0e400110, 0x0fff0fff, "wfc%c\t%12-15r"},
  {FPU_FPA_EXT_V1, 0x0e500110, 0x0fff0fff, "rfc%c\t%12-15r"},
  {FPU_FPA_EXT_V1, 0x0e90f110, 0x0ff8fff0, "cmf%c\t%16-18f, %0-3f"},
  {FPU_FPA_EXT_V1, 0x0eb0f110, 0x0ff8fff0, "cnf%c\t%16-18f, %0-3f"},
  {FPU_FPA_EXT_V1, 0x0ed0f110, 0x0ff8fff0, "cmfe%c\t%16-18f, %0-3f"},
  {FPU_FPA_EXT_V1, 0x0ef0f110, 0x0ff8fff0, "cnfe%c\t%16-18f, %0-3f"},
  {FPU_FPA_EXT_V1, 0x0c000100, 0x0e100f00, "stf%c%Q\t%12-14f, %A"},
  {FPU_FPA_EXT_V1, 0x0c100100, 0x0e100f00, "ldf%c%Q\t%12-14f, %A"},
  {FPU_FPA_EXT_V2, 0x0c000200, 0x0e100f00, "sfm%c\t%12-14f, %F, %A"},
  {FPU_FPA_EXT_V2, 0x0c100200, 0x0e100f00, "lfm%c\t%12-14f, %F, %A"},

  /* Floating point coprocessor (VFP) instructions */
  {FPU_VFP_EXT_V1, 0x0eb00bc0, 0x0fff0ff0, "fabsd%c\t%1z, %0z"},
  {FPU_VFP_EXT_V1xD, 0x0eb00ac0, 0x0fbf0fd0, "fabss%c\t%1y, %0y"},
  {FPU_VFP_EXT_V1, 0x0e300b00, 0x0ff00ff0, "faddd%c\t%1z, %2z, %0z"},
  {FPU_VFP_EXT_V1xD, 0x0e300a00, 0x0fb00f50, "fadds%c\t%1y, %2y, %0y"},
  {FPU_VFP_EXT_V1, 0x0eb40b40, 0x0fff0f70, "fcmp%7'ed%c\t%1z, %0z"},
  {FPU_VFP_EXT_V1xD, 0x0eb40a40, 0x0fbf0f50, "fcmp%7'es%c\t%1y, %0y"},
  {FPU_VFP_EXT_V1, 0x0eb50b40, 0x0fff0f70, "fcmp%7'ezd%c\t%1z"},
  {FPU_VFP_EXT_V1xD, 0x0eb50a40, 0x0fbf0f70, "fcmp%7'ezs%c\t%1y"},
  {FPU_VFP_EXT_V1, 0x0eb00b40, 0x0fff0ff0, "fcpyd%c\t%1z, %0z"},
  {FPU_VFP_EXT_V1xD, 0x0eb00a40, 0x0fbf0fd0, "fcpys%c\t%1y, %0y"},
  {FPU_VFP_EXT_V1, 0x0eb70ac0, 0x0fff0fd0, "fcvtds%c\t%1z, %0y"},
  {FPU_VFP_EXT_V1, 0x0eb70bc0, 0x0fbf0ff0, "fcvtsd%c\t%1y, %0z"},
  {FPU_VFP_EXT_V1, 0x0e800b00, 0x0ff00ff0, "fdivd%c\t%1z, %2z, %0z"},
  {FPU_VFP_EXT_V1xD, 0x0e800a00, 0x0fb00f50, "fdivs%c\t%1y, %2y, %0y"},
  {FPU_VFP_EXT_V1, 0x0d100b00, 0x0f700f00, "fldd%c\t%1z, %A"},
  {FPU_VFP_EXT_V1xD, 0x0c900b00, 0x0fd00f00, "fldmia%0?xd%c\t%16-19r%21'!, %3z"},
  {FPU_VFP_EXT_V1xD, 0x0d300b00, 0x0ff00f00, "fldmdb%0?xd%c\t%16-19r!, %3z"},
  {FPU_VFP_EXT_V1xD, 0x0d100a00, 0x0f300f00, "flds%c\t%1y, %A"},
  {FPU_VFP_EXT_V1xD, 0x0c900a00, 0x0f900f00, "fldmias%c\t%16-19r%21'!, %3y"},
  {FPU_VFP_EXT_V1xD, 0x0d300a00, 0x0fb00f00, "fldmdbs%c\t%16-19r!, %3y"},
  {FPU_VFP_EXT_V1, 0x0e000b00, 0x0ff00ff0, "fmacd%c\t%1z, %2z, %0z"},
  {FPU_VFP_EXT_V1xD, 0x0e000a00, 0x0fb00f50, "fmacs%c\t%1y, %2y, %0y"},
  {FPU_VFP_EXT_V1, 0x0e200b10, 0x0ff00fff, "fmdhr%c\t%2z, %12-15r"},
  {FPU_VFP_EXT_V1, 0x0e000b10, 0x0ff00fff, "fmdlr%c\t%2z, %12-15r"},
  {FPU_VFP_EXT_V2, 0x0c400b10, 0x0ff00ff0, "fmdrr%c\t%0z, %12-15r, %16-19r"},
  {FPU_VFP_EXT_V1, 0x0e300b10, 0x0ff00fff, "fmrdh%c\t%12-15r, %2z"},
  {FPU_VFP_EXT_V1, 0x0e100b10, 0x0ff00fff, "fmrdl%c\t%12-15r, %2z"},
  {FPU_VFP_EXT_V1, 0x0c500b10, 0x0ff00ff0, "fmrrd%c\t%12-15r, %16-19r, %0z"},
  {FPU_VFP_EXT_V2, 0x0c500a10, 0x0ff00fd0, "fmrrs%c\t%12-15r, %16-19r, %4y"},
  {FPU_VFP_EXT_V1xD, 0x0e100a10, 0x0ff00f7f, "fmrs%c\t%12-15r, %2y"},
  {FPU_VFP_EXT_V1xD, 0x0ef1fa10, 0x0fffffff, "fmstat%c"},
  {FPU_VFP_EXT_V1xD, 0x0ef00a10, 0x0fff0fff, "fmrx%c\t%12-15r, fpsid"},
  {FPU_VFP_EXT_V1xD, 0x0ef10a10, 0x0fff0fff, "fmrx%c\t%12-15r, fpscr"},
  {FPU_VFP_EXT_V1xD, 0x0ef80a10, 0x0fff0fff, "fmrx%c\t%12-15r, fpexc"},
  {FPU_VFP_EXT_V1xD, 0x0ef90a10, 0x0fff0fff, "fmrx%c\t%12-15r, fpinst\t@ Impl def"},
  {FPU_VFP_EXT_V1xD, 0x0efa0a10, 0x0fff0fff, "fmrx%c\t%12-15r, fpinst2\t@ Impl def"},
  {FPU_VFP_EXT_V1xD, 0x0ef00a10, 0x0ff00fff, "fmrx%c\t%12-15r, <impl def 0x%16-19x>"},
  {FPU_VFP_EXT_V1, 0x0e100b00, 0x0ff00ff0, "fmscd%c\t%1z, %2z, %0z"},
  {FPU_VFP_EXT_V1xD, 0x0e100a00, 0x0fb00f50, "fmscs%c\t%1y, %2y, %0y"},
  {FPU_VFP_EXT_V1xD, 0x0e000a10, 0x0ff00f7f, "fmsr%c\t%2y, %12-15r"},
  {FPU_VFP_EXT_V2, 0x0c400a10, 0x0ff00fd0, "fmsrr%c\t%12-15r, %16-19r, %4y"},
  {FPU_VFP_EXT_V1, 0x0e200b00, 0x0ff00ff0, "fmuld%c\t%1z, %2z, %0z"},
  {FPU_VFP_EXT_V1xD, 0x0e200a00, 0x0fb00f50, "fmuls%c\t%1y, %2y, %0y"},
  {FPU_VFP_EXT_V1xD, 0x0ee00a10, 0x0fff0fff, "fmxr%c\tfpsid, %12-15r"},
  {FPU_VFP_EXT_V1xD, 0x0ee10a10, 0x0fff0fff, "fmxr%c\tfpscr, %12-15r"},
  {FPU_VFP_EXT_V1xD, 0x0ee80a10, 0x0fff0fff, "fmxr%c\tfpexc, %12-15r"},
  {FPU_VFP_EXT_V1xD, 0x0ee90a10, 0x0fff0fff, "fmxr%c\tfpinst, %12-15r\t@ Impl def"},
  {FPU_VFP_EXT_V1xD, 0x0eea0a10, 0x0fff0fff, "fmxr%c\tfpinst2, %12-15r\t@ Impl def"},
  {FPU_VFP_EXT_V1xD, 0x0ee00a10, 0x0ff00fff, "fmxr%c\t<impl def 0x%16-19x>, %12-15r"},
  {FPU_VFP_EXT_V1, 0x0eb10b40, 0x0fff0ff0, "fnegd%c\t%1z, %0z"},
  {FPU_VFP_EXT_V1xD, 0x0eb10a40, 0x0fbf0fd0, "fnegs%c\t%1y, %0y"},
  {FPU_VFP_EXT_V1, 0x0e000b40, 0x0ff00ff0, "fnmacd%c\t%1z, %2z, %0z"},
  {FPU_VFP_EXT_V1xD, 0x0e000a40, 0x0fb00f50, "fnmacs%c\t%1y, %2y, %0y"},
  {FPU_VFP_EXT_V1, 0x0e100b40, 0x0ff00ff0, "fnmscd%c\t%1z, %2z, %0z"},
  {FPU_VFP_EXT_V1xD, 0x0e100a40, 0x0fb00f50, "fnmscs%c\t%1y, %2y, %0y"},
  {FPU_VFP_EXT_V1, 0x0e200b40, 0x0ff00ff0, "fnmuld%c\t%1z, %2z, %0z"},
  {FPU_VFP_EXT_V1xD, 0x0e200a40, 0x0fb00f50, "fnmuls%c\t%1y, %2y, %0y"},
  {FPU_VFP_EXT_V1, 0x0eb80bc0, 0x0fff0fd0, "fsitod%c\t%1z, %0y"},
  {FPU_VFP_EXT_V1xD, 0x0eb80ac0, 0x0fbf0fd0, "fsitos%c\t%1y, %0y"},
  {FPU_VFP_EXT_V1, 0x0eb10bc0, 0x0fff0ff0, "fsqrtd%c\t%1z, %0z"},
  {FPU_VFP_EXT_V1xD, 0x0eb10ac0, 0x0fbf0fd0, "fsqrts%c\t%1y, %0y"},
  {FPU_VFP_EXT_V1, 0x0d000b00, 0x0f700f00, "fstd%c\t%1z, %A"},
  {FPU_VFP_EXT_V1xD, 0x0c800b00, 0x0fd00f00, "fstmia%0?xd%c\t%16-19r%21'!, %3z"},
  {FPU_VFP_EXT_V1xD, 0x0d200b00, 0x0ff00f00, "fstmdb%0?xd%c\t%16-19r!, %3z"},
  {FPU_VFP_EXT_V1xD, 0x0d000a00, 0x0f300f00, "fsts%c\t%1y, %A"},
  {FPU_VFP_EXT_V1xD, 0x0c800a00, 0x0f900f00, "fstmias%c\t%16-19r%21'!, %3y"},
  {FPU_VFP_EXT_V1xD, 0x0d200a00, 0x0fb00f00, "fstmdbs%c\t%16-19r!, %3y"},
  {FPU_VFP_EXT_V1, 0x0e300b40, 0x0ff00ff0, "fsubd%c\t%1z, %2z, %0z"},
  {FPU_VFP_EXT_V1xD, 0x0e300a40, 0x0fb00f50, "fsubs%c\t%1y, %2y, %0y"},
  {FPU_VFP_EXT_V1, 0x0ebc0b40, 0x0fbe0f70, "fto%16?sui%7'zd%c\t%1y, %0z"},
  {FPU_VFP_EXT_V1xD, 0x0ebc0a40, 0x0fbe0f50, "fto%16?sui%7'zs%c\t%1y, %0y"},
  {FPU_VFP_EXT_V1, 0x0eb80b40, 0x0fff0fd0, "fuitod%c\t%1z, %0y"},
  {FPU_VFP_EXT_V1xD, 0x0eb80a40, 0x0fbf0fd0, "fuitos%c\t%1y, %0y"},

  /* Cirrus coprocessor instructions.  */
  {ARM_CEXT_MAVERICK, 0x0d100400, 0x0f500f00, "cfldrs%c\tmvf%12-15d, %A"},
  {ARM_CEXT_MAVERICK, 0x0c100400, 0x0f500f00, "cfldrs%c\tmvf%12-15d, %A"},
  {ARM_CEXT_MAVERICK, 0x0d500400, 0x0f500f00, "cfldrd%c\tmvd%12-15d, %A"},
  {ARM_CEXT_MAVERICK, 0x0c500400, 0x0f500f00, "cfldrd%c\tmvd%12-15d, %A"}, 
  {ARM_CEXT_MAVERICK, 0x0d100500, 0x0f500f00, "cfldr32%c\tmvfx%12-15d, %A"},
  {ARM_CEXT_MAVERICK, 0x0c100500, 0x0f500f00, "cfldr32%c\tmvfx%12-15d, %A"},
  {ARM_CEXT_MAVERICK, 0x0d500500, 0x0f500f00, "cfldr64%c\tmvdx%12-15d, %A"},
  {ARM_CEXT_MAVERICK, 0x0c500500, 0x0f500f00, "cfldr64%c\tmvdx%12-15d, %A"},
  {ARM_CEXT_MAVERICK, 0x0d000400, 0x0f500f00, "cfstrs%c\tmvf%12-15d, %A"},
  {ARM_CEXT_MAVERICK, 0x0c000400, 0x0f500f00, "cfstrs%c\tmvf%12-15d, %A"},
  {ARM_CEXT_MAVERICK, 0x0d400400, 0x0f500f00, "cfstrd%c\tmvd%12-15d, %A"},
  {ARM_CEXT_MAVERICK, 0x0c400400, 0x0f500f00, "cfstrd%c\tmvd%12-15d, %A"},
  {ARM_CEXT_MAVERICK, 0x0d000500, 0x0f500f00, "cfstr32%c\tmvfx%12-15d, %A"},
  {ARM_CEXT_MAVERICK, 0x0c000500, 0x0f500f00, "cfstr32%c\tmvfx%12-15d, %A"},
  {ARM_CEXT_MAVERICK, 0x0d400500, 0x0f500f00, "cfstr64%c\tmvdx%12-15d, %A"},
  {ARM_CEXT_MAVERICK, 0x0c400500, 0x0f500f00, "cfstr64%c\tmvdx%12-15d, %A"},
  {ARM_CEXT_MAVERICK, 0x0e000450, 0x0ff00ff0, "cfmvsr%c\tmvf%16-19d, %12-15r"},
  {ARM_CEXT_MAVERICK, 0x0e100450, 0x0ff00ff0, "cfmvrs%c\t%12-15r, mvf%16-19d"},
  {ARM_CEXT_MAVERICK, 0x0e000410, 0x0ff00ff0, "cfmvdlr%c\tmvd%16-19d, %12-15r"},
  {ARM_CEXT_MAVERICK, 0x0e100410, 0x0ff00ff0, "cfmvrdl%c\t%12-15r, mvd%16-19d"},
  {ARM_CEXT_MAVERICK, 0x0e000430, 0x0ff00ff0, "cfmvdhr%c\tmvd%16-19d, %12-15r"},
  {ARM_CEXT_MAVERICK, 0x0e100430, 0x0ff00fff, "cfmvrdh%c\t%12-15r, mvd%16-19d"},
  {ARM_CEXT_MAVERICK, 0x0e000510, 0x0ff00fff, "cfmv64lr%c\tmvdx%16-19d, %12-15r"},
  {ARM_CEXT_MAVERICK, 0x0e100510, 0x0ff00fff, "cfmvr64l%c\t%12-15r, mvdx%16-19d"},
  {ARM_CEXT_MAVERICK, 0x0e000530, 0x0ff00fff, "cfmv64hr%c\tmvdx%16-19d, %12-15r"},
  {ARM_CEXT_MAVERICK, 0x0e100530, 0x0ff00fff, "cfmvr64h%c\t%12-15r, mvdx%16-19d"},
  {ARM_CEXT_MAVERICK, 0x0e200440, 0x0ff00fff, "cfmval32%c\tmvax%12-15d, mvfx%16-19d"},
  {ARM_CEXT_MAVERICK, 0x0e100440, 0x0ff00fff, "cfmv32al%c\tmvfx%12-15d, mvax%16-19d"},
  {ARM_CEXT_MAVERICK, 0x0e200460, 0x0ff00fff, "cfmvam32%c\tmvax%12-15d, mvfx%16-19d"},
  {ARM_CEXT_MAVERICK, 0x0e100460, 0x0ff00fff, "cfmv32am%c\tmvfx%12-15d, mvax%16-19d"},
  {ARM_CEXT_MAVERICK, 0x0e200480, 0x0ff00fff, "cfmvah32%c\tmvax%12-15d, mvfx%16-19d"},
  {ARM_CEXT_MAVERICK, 0x0e100480, 0x0ff00fff, "cfmv32ah%c\tmvfx%12-15d, mvax%16-19d"},
  {ARM_CEXT_MAVERICK, 0x0e2004a0, 0x0ff00fff, "cfmva32%c\tmvax%12-15d, mvfx%16-19d"},
  {ARM_CEXT_MAVERICK, 0x0e1004a0, 0x0ff00fff, "cfmv32a%c\tmvfx%12-15d, mvax%16-19d"},
  {ARM_CEXT_MAVERICK, 0x0e2004c0, 0x0ff00fff, "cfmva64%c\tmvax%12-15d, mvdx%16-19d"},
  {ARM_CEXT_MAVERICK, 0x0e1004c0, 0x0ff00fff, "cfmv64a%c\tmvdx%12-15d, mvax%16-19d"},
  {ARM_CEXT_MAVERICK, 0x0e2004e0, 0x0fff0fff, "cfmvsc32%c\tdspsc, mvdx%12-15d"},
  {ARM_CEXT_MAVERICK, 0x0e1004e0, 0x0fff0fff, "cfmv32sc%c\tmvdx%12-15d, dspsc"},
  {ARM_CEXT_MAVERICK, 0x0e000400, 0x0ff00fff, "cfcpys%c\tmvf%12-15d, mvf%16-19d"},
  {ARM_CEXT_MAVERICK, 0x0e000420, 0x0ff00fff, "cfcpyd%c\tmvd%12-15d, mvd%16-19d"},
  {ARM_CEXT_MAVERICK, 0x0e000460, 0x0ff00fff, "cfcvtsd%c\tmvd%12-15d, mvf%16-19d"},
  {ARM_CEXT_MAVERICK, 0x0e000440, 0x0ff00fff, "cfcvtds%c\tmvf%12-15d, mvd%16-19d"},
  {ARM_CEXT_MAVERICK, 0x0e000480, 0x0ff00fff, "cfcvt32s%c\tmvf%12-15d, mvfx%16-19d"},
  {ARM_CEXT_MAVERICK, 0x0e0004a0, 0x0ff00fff, "cfcvt32d%c\tmvd%12-15d, mvfx%16-19d"},
  {ARM_CEXT_MAVERICK, 0x0e0004c0, 0x0ff00fff, "cfcvt64s%c\tmvf%12-15d, mvdx%16-19d"},
  {ARM_CEXT_MAVERICK, 0x0e0004e0, 0x0ff00fff, "cfcvt64d%c\tmvd%12-15d, mvdx%16-19d"},
  {ARM_CEXT_MAVERICK, 0x0e100580, 0x0ff00fff, "cfcvts32%c\tmvfx%12-15d, mvf%16-19d"},
  {ARM_CEXT_MAVERICK, 0x0e1005a0, 0x0ff00fff, "cfcvtd32%c\tmvfx%12-15d, mvd%16-19d"},
  {ARM_CEXT_MAVERICK, 0x0e1005c0, 0x0ff00fff, "cftruncs32%c\tmvfx%12-15d, mvf%16-19d"},
  {ARM_CEXT_MAVERICK, 0x0e1005e0, 0x0ff00fff, "cftruncd32%c\tmvfx%12-15d, mvd%16-19d"},
  {ARM_CEXT_MAVERICK, 0x0e000550, 0x0ff00ff0, "cfrshl32%c\tmvfx%16-19d, mvfx%0-3d, %12-15r"},
  {ARM_CEXT_MAVERICK, 0x0e000570, 0x0ff00ff0, "cfrshl64%c\tmvdx%16-19d, mvdx%0-3d, %12-15r"},
  {ARM_CEXT_MAVERICK, 0x0e000500, 0x0ff00f10, "cfsh32%c\tmvfx%12-15d, mvfx%16-19d, #%I"},
  {ARM_CEXT_MAVERICK, 0x0e200500, 0x0ff00f10, "cfsh64%c\tmvdx%12-15d, mvdx%16-19d, #%I"},
  {ARM_CEXT_MAVERICK, 0x0e100490, 0x0ff00ff0, "cfcmps%c\t%12-15r, mvf%16-19d, mvf%0-3d"},
  {ARM_CEXT_MAVERICK, 0x0e1004b0, 0x0ff00ff0, "cfcmpd%c\t%12-15r, mvd%16-19d, mvd%0-3d"},
  {ARM_CEXT_MAVERICK, 0x0e100590, 0x0ff00ff0, "cfcmp32%c\t%12-15r, mvfx%16-19d, mvfx%0-3d"},
  {ARM_CEXT_MAVERICK, 0x0e1005b0, 0x0ff00ff0, "cfcmp64%c\t%12-15r, mvdx%16-19d, mvdx%0-3d"},
  {ARM_CEXT_MAVERICK, 0x0e300400, 0x0ff00fff, "cfabss%c\tmvf%12-15d, mvf%16-19d"},
  {ARM_CEXT_MAVERICK, 0x0e300420, 0x0ff00fff, "cfabsd%c\tmvd%12-15d, mvd%16-19d"},
  {ARM_CEXT_MAVERICK, 0x0e300440, 0x0ff00fff, "cfnegs%c\tmvf%12-15d, mvf%16-19d"},
  {ARM_CEXT_MAVERICK, 0x0e300460, 0x0ff00fff, "cfnegd%c\tmvd%12-15d, mvd%16-19d"},
  {ARM_CEXT_MAVERICK, 0x0e300480, 0x0ff00ff0, "cfadds%c\tmvf%12-15d, mvf%16-19d, mvf%0-3d"},
  {ARM_CEXT_MAVERICK, 0x0e3004a0, 0x0ff00ff0, "cfaddd%c\tmvd%12-15d, mvd%16-19d, mvd%0-3d"},
  {ARM_CEXT_MAVERICK, 0x0e3004c0, 0x0ff00ff0, "cfsubs%c\tmvf%12-15d, mvf%16-19d, mvf%0-3d"},
  {ARM_CEXT_MAVERICK, 0x0e3004e0, 0x0ff00ff0, "cfsubd%c\tmvd%12-15d, mvd%16-19d, mvd%0-3d"},
  {ARM_CEXT_MAVERICK, 0x0e100400, 0x0ff00ff0, "cfmuls%c\tmvf%12-15d, mvf%16-19d, mvf%0-3d"},
  {ARM_CEXT_MAVERICK, 0x0e100420, 0x0ff00ff0, "cfmuld%c\tmvd%12-15d, mvd%16-19d, mvd%0-3d"},
  {ARM_CEXT_MAVERICK, 0x0e300500, 0x0ff00fff, "cfabs32%c\tmvfx%12-15d, mvfx%16-19d"},
  {ARM_CEXT_MAVERICK, 0x0e300520, 0x0ff00fff, "cfabs64%c\tmvdx%12-15d, mvdx%16-19d"},
  {ARM_CEXT_MAVERICK, 0x0e300540, 0x0ff00fff, "cfneg32%c\tmvfx%12-15d, mvfx%16-19d"},
  {ARM_CEXT_MAVERICK, 0x0e300560, 0x0ff00fff, "cfneg64%c\tmvdx%12-15d, mvdx%16-19d"},
  {ARM_CEXT_MAVERICK, 0x0e300580, 0x0ff00ff0, "cfadd32%c\tmvfx%12-15d, mvfx%16-19d, mvfx%0-3d"},
  {ARM_CEXT_MAVERICK, 0x0e3005a0, 0x0ff00ff0, "cfadd64%c\tmvdx%12-15d, mvdx%16-19d, mvdx%0-3d"},
  {ARM_CEXT_MAVERICK, 0x0e3005c0, 0x0ff00ff0, "cfsub32%c\tmvfx%12-15d, mvfx%16-19d, mvfx%0-3d"},
  {ARM_CEXT_MAVERICK, 0x0e3005e0, 0x0ff00ff0, "cfsub64%c\tmvdx%12-15d, mvdx%16-19d, mvdx%0-3d"},
  {ARM_CEXT_MAVERICK, 0x0e100500, 0x0ff00ff0, "cfmul32%c\tmvfx%12-15d, mvfx%16-19d, mvfx%0-3d"},
  {ARM_CEXT_MAVERICK, 0x0e100520, 0x0ff00ff0, "cfmul64%c\tmvdx%12-15d, mvdx%16-19d, mvdx%0-3d"},
  {ARM_CEXT_MAVERICK, 0x0e100540, 0x0ff00ff0, "cfmac32%c\tmvfx%12-15d, mvfx%16-19d, mvfx%0-3d"},
  {ARM_CEXT_MAVERICK, 0x0e100560, 0x0ff00ff0, "cfmsc32%c\tmvfx%12-15d, mvfx%16-19d, mvfx%0-3d"},
  {ARM_CEXT_MAVERICK, 0x0e000600, 0x0ff00f10, "cfmadd32%c\tmvax%5-7d, mvfx%12-15d, mvfx%16-19d, mvfx%0-3d"},
  {ARM_CEXT_MAVERICK, 0x0e100600, 0x0ff00f10, "cfmsub32%c\tmvax%5-7d, mvfx%12-15d, mvfx%16-19d, mvfx%0-3d"},
  {ARM_CEXT_MAVERICK, 0x0e200600, 0x0ff00f10, "cfmadda32%c\tmvax%5-7d, mvax%12-15d, mvfx%16-19d, mvfx%0-3d"},
  {ARM_CEXT_MAVERICK, 0x0e300600, 0x0ff00f10, "cfmsuba32%c\tmvax%5-7d, mvax%12-15d, mvfx%16-19d, mvfx%0-3d"},

  /* Generic coprocessor instructions */
  {ARM_EXT_V2, 0x0c400000, 0x0ff00000, "mcrr%c\t%8-11d, %4-7d, %12-15r, %16-19r, cr%0-3d"},
  {ARM_EXT_V2, 0x0c500000, 0x0ff00000, "mrrc%c\t%8-11d, %4-7d, %12-15r, %16-19r, cr%0-3d"},
  {ARM_EXT_V2, 0x0e000000, 0x0f000010, "cdp%c\t%8-11d, %20-23d, cr%12-15d, cr%16-19d, cr%0-3d, {%5-7d}"},
  {ARM_EXT_V2, 0x0e100010, 0x0f100010, "mrc%c\t%8-11d, %21-23d, %12-15r, cr%16-19d, cr%0-3d, {%5-7d}"},
  {ARM_EXT_V2, 0x0e000010, 0x0f100010, "mcr%c\t%8-11d, %21-23d, %12-15r, cr%16-19d, cr%0-3d, {%5-7d}"},
  {ARM_EXT_V2, 0x0c000000, 0x0e100000, "stc%c%22'l\t%8-11d, cr%12-15d, %A"},
  {ARM_EXT_V2, 0x0c100000, 0x0e100000, "ldc%c%22'l\t%8-11d, cr%12-15d, %A"},

  /* V6 coprocessor instructions */
  {ARM_EXT_V6, 0xfc500000, 0xfff00000, "mrrc2\t%8-11d, %4-7d, %12-15r, %16-19r, cr%0-3d"},
  {ARM_EXT_V6, 0xfc400000, 0xfff00000, "mcrr2\t%8-11d, %4-7d, %12-15r, %16-19r, cr%0-3d"},

  /* V5 coprocessor instructions */
  {ARM_EXT_V5, 0xfc100000, 0xfe100000, "ldc2%22'l\t%8-11d, cr%12-15d, %A"},
  {ARM_EXT_V5, 0xfc000000, 0xfe100000, "stc2%22'l\t%8-11d, cr%12-15d, %A"},
  {ARM_EXT_V5, 0xfe000000, 0xff000010, "cdp2\t%8-11d, %20-23d, cr%12-15d, cr%16-19d, cr%0-3d, {%5-7d}"},
  {ARM_EXT_V5, 0xfe000010, 0xff100010, "mcr2\t%8-11d, %21-23d, %12-15r, cr%16-19d, cr%0-3d, {%5-7d}"},
  {ARM_EXT_V5, 0xfe100010, 0xff100010, "mrc2\t%8-11d, %21-23d, %12-15r, cr%16-19d, cr%0-3d, {%5-7d}"},
  {0, 0, 0, 0}
};

/* Opcode tables: ARM, 16-bit Thumb, 32-bit Thumb.  All three are partially
   ordered: they must be searched linearly from the top to obtain a correct
   match.  */

/* print_insn_arm recognizes the following format control codes:

   %%			%

   %a			print address for ldr/str instruction
   %s                   print address for ldr/str halfword/signextend instruction
   %b			print branch destination
   %c			print condition code (always bits 28-31)
   %m			print register mask for ldm/stm instruction
   %o			print operand2 (immediate or register + shift)
   %p			print 'p' iff bits 12-15 are 15
   %t			print 't' iff bit 21 set and bit 24 clear
   %B			print arm BLX(1) destination
   %C			print the PSR sub type.
   %U			print barrier type.
   %P			print address for pli instruction.

   %<bitfield>r		print as an ARM register
   %<bitfield>d		print the bitfield in decimal
   %<bitfield>W         print the bitfield plus one in decimal 
   %<bitfield>x		print the bitfield in hex
   %<bitfield>X		print the bitfield as 1 hex digit without leading "0x"

   %<bitnum>'c		print specified char iff bit is one
   %<bitnum>`c		print specified char iff bit is zero
   %<bitnum>?ab		print a if bit is one else print b

   %e                   print arm SMI operand (bits 0..7,8..19).
   %E			print the LSB and WIDTH fields of a BFI or BFC instruction.
   %V                   print the 16-bit immediate field of a MOVT or MOVW instruction.  */

static const struct opcode32 arm_opcodes[] =
{
  /* ARM instructions.  */
  {ARM_EXT_V1, 0xe1a00000, 0xffffffff, "nop\t\t\t(mov r0,r0)"},
  {ARM_EXT_V4T | ARM_EXT_V5, 0x012FFF10, 0x0ffffff0, "bx%c\t%0-3r"},
  {ARM_EXT_V2, 0x00000090, 0x0fe000f0, "mul%c%20's\t%16-19r, %0-3r, %8-11r"},
  {ARM_EXT_V2, 0x00200090, 0x0fe000f0, "mla%c%20's\t%16-19r, %0-3r, %8-11r, %12-15r"},
  {ARM_EXT_V2S, 0x01000090, 0x0fb00ff0, "swp%c%22'b\t%12-15r, %0-3r, [%16-19r]"},
  {ARM_EXT_V3M, 0x00800090, 0x0fa000f0, "%22?sumull%c%20's\t%12-15r, %16-19r, %0-3r, %8-11r"},
  {ARM_EXT_V3M, 0x00a00090, 0x0fa000f0, "%22?sumlal%c%20's\t%12-15r, %16-19r, %0-3r, %8-11r"},

  /* V7 instructions.  */
  {ARM_EXT_V7, 0xf450f000, 0xfd70f000, "pli\t%P"},
  {ARM_EXT_V7, 0x0320f0f0, 0x0ffffff0, "dbg%c\t#%0-3d"},
  {ARM_EXT_V7, 0xf57ff050, 0xfffffff0, "dmb\t%U"},
  {ARM_EXT_V7, 0xf57ff040, 0xfffffff0, "dsb\t%U"},
  {ARM_EXT_V7, 0xf57ff060, 0xfffffff0, "isb\t%U"},

  /* ARM V6T2 instructions.  */
  {ARM_EXT_V6T2, 0x07c0001f, 0x0fe0007f, "bfc%c\t%12-15r, %E"},
  {ARM_EXT_V6T2, 0x07c00010, 0x0fe00070, "bfi%c\t%12-15r, %0-3r, %E"},
  {ARM_EXT_V6T2, 0x00600090, 0x0ff000f0, "mls%c\t%16-19r, %0-3r, %8-11r, %12-15r"},
  {ARM_EXT_V6T2, 0x006000b0, 0x0f7000f0, "str%cht\t%12-15r, %s"},
  {ARM_EXT_V6T2, 0x00300090, 0x0f300090, "ldr%c%6's%5?hbt\t%12-15r, %s"},
  {ARM_EXT_V6T2, 0x03000000, 0x0ff00000, "movw%c\t%12-15r, %V"},
  {ARM_EXT_V6T2, 0x03400000, 0x0ff00000, "movt%c\t%12-15r, %V"},
  {ARM_EXT_V6T2, 0x03ff0f30, 0x0fff0ff0, "rbit%c\t%12-15r, %0-3r"},
  {ARM_EXT_V6T2, 0x07a00050, 0x0fa00070, "%22?usbfx%c\t%12-15r, %0-3r, #%7-11d, #%16-20W"},

  /* ARM V6Z instructions.  */
  {ARM_EXT_V6Z, 0x01600070, 0x0ff000f0, "smc%c\t%e"},

  /* ARM V6K instructions.  */
  {ARM_EXT_V6K, 0xf57ff01f, 0xffffffff, "clrex"},
  {ARM_EXT_V6K, 0x01d00f9f, 0x0ff00fff, "ldrexb%c\t%12-15r, [%16-19r]"},
  {ARM_EXT_V6K, 0x01b00f9f, 0x0ff00fff, "ldrexd%c\t%12-15r, [%16-19r]"},
  {ARM_EXT_V6K, 0x01f00f9f, 0x0ff00fff, "ldrexh%c\t%12-15r, [%16-19r]"},
  {ARM_EXT_V6K, 0x01c00f90, 0x0ff00ff0, "strexb%c\t%12-15r, %0-3r, [%16-19r]"},
  {ARM_EXT_V6K, 0x01a00f90, 0x0ff00ff0, "strexd%c\t%12-15r, %0-3r, [%16-19r]"},
  {ARM_EXT_V6K, 0x01e00f90, 0x0ff00ff0, "strexh%c\t%12-15r, %0-3r, [%16-19r]"},

  /* ARM V6K NOP hints.  */
  {ARM_EXT_V6K, 0x0320f001, 0x0fffffff, "yield%c"},
  {ARM_EXT_V6K, 0x0320f002, 0x0fffffff, "wfe%c"},
  {ARM_EXT_V6K, 0x0320f003, 0x0fffffff, "wfi%c"},
  {ARM_EXT_V6K, 0x0320f004, 0x0fffffff, "sev%c"},
  {ARM_EXT_V6K, 0x0320f000, 0x0fffff00, "nop%c\t{%0-7d}"},

  /* ARM V6 instructions. */
  {ARM_EXT_V6, 0xf1080000, 0xfffdfe3f, "cpsie\t%8'a%7'i%6'f"},
  {ARM_EXT_V6, 0xf1080000, 0xfffdfe20, "cpsie\t%8'a%7'i%6'f,#%0-4d"},
  {ARM_EXT_V6, 0xf10C0000, 0xfffdfe3f, "cpsid\t%8'a%7'i%6'f"},
  {ARM_EXT_V6, 0xf10C0000, 0xfffdfe20, "cpsid\t%8'a%7'i%6'f,#%0-4d"},
  {ARM_EXT_V6, 0xf1000000, 0xfff1fe20, "cps\t#%0-4d"},
  {ARM_EXT_V6, 0x06800010, 0x0ff00ff0, "pkhbt%c\t%12-15r, %16-19r, %0-3r"},
  {ARM_EXT_V6, 0x06800010, 0x0ff00070, "pkhbt%c\t%12-15r, %16-19r, %0-3r, LSL #%7-11d"},
  {ARM_EXT_V6, 0x06800050, 0x0ff00ff0, "pkhtb%c\t%12-15r, %16-19r, %0-3r, ASR #32"},
  {ARM_EXT_V6, 0x06800050, 0x0ff00070, "pkhtb%c\t%12-15r, %16-19r, %0-3r, ASR #%7-11d"},
  {ARM_EXT_V6, 0x01900f9f, 0x0ff00fff, "ldrex%c\tr%12-15d, [%16-19r]"},
  {ARM_EXT_V6, 0x06200f10, 0x0ff00ff0, "qadd16%c\t%12-15r, %16-19r, %0-3r"},
  {ARM_EXT_V6, 0x06200f90, 0x0ff00ff0, "qadd8%c\t%12-15r, %16-19r, %0-3r"},
  {ARM_EXT_V6, 0x06200f30, 0x0ff00ff0, "qaddsubx%c\t%12-15r, %16-19r, %0-3r"},
  {ARM_EXT_V6, 0x06200f70, 0x0ff00ff0, "qsub16%c\t%12-15r, %16-19r, %0-3r"},
  {ARM_EXT_V6, 0x06200ff0, 0x0ff00ff0, "qsub8%c\t%12-15r, %16-19r, %0-3r"},
  {ARM_EXT_V6, 0x06200f50, 0x0ff00ff0, "qsubaddx%c\t%12-15r, %16-19r, %0-3r"},
  {ARM_EXT_V6, 0x06100f10, 0x0ff00ff0, "sadd16%c\t%12-15r, %16-19r, %0-3r"},
  {ARM_EXT_V6, 0x06100f90, 0x0ff00ff0, "sadd8%c\t%12-15r, %16-19r, %0-3r"},
  {ARM_EXT_V6, 0x06100f30, 0x0ff00ff0, "saddaddx%c\t%12-15r, %16-19r, %0-3r"},
  {ARM_EXT_V6, 0x06300f10, 0x0ff00ff0, "shadd16%c\t%12-15r, %16-19r, %0-3r"},
  {ARM_EXT_V6, 0x06300f90, 0x0ff00ff0, "shadd8%c\t%12-15r, %16-19r, %0-3r"},
  {ARM_EXT_V6, 0x06300f30, 0x0ff00ff0, "shaddsubx%c\t%12-15r, %16-19r, %0-3r"},
  {ARM_EXT_V6, 0x06300f70, 0x0ff00ff0, "shsub16%c\t%12-15r, %16-19r, %0-3r"},
  {ARM_EXT_V6, 0x06300ff0, 0x0ff00ff0, "shsub8%c\t%12-15r, %16-19r, %0-3r"},
  {ARM_EXT_V6, 0x06300f50, 0x0ff00ff0, "shsubaddx%c\t%12-15r, %16-19r, %0-3r"},
  {ARM_EXT_V6, 0x06100f70, 0x0ff00ff0, "ssub16%c\t%12-15r, %16-19r, %0-3r"},
  {ARM_EXT_V6, 0x06100ff0, 0x0ff00ff0, "ssub8%c\t%12-15r, %16-19r, %0-3r"},
  {ARM_EXT_V6, 0x06100f50, 0x0ff00ff0, "ssubaddx%c\t%12-15r, %16-19r, %0-3r"},
  {ARM_EXT_V6, 0x06500f10, 0x0ff00ff0, "uadd16%c\t%12-15r, %16-19r, %0-3r"},
  {ARM_EXT_V6, 0x06500f90, 0x0ff00ff0, "uadd8%c\t%12-15r, %16-19r, %0-3r"},
  {ARM_EXT_V6, 0x06500f30, 0x0ff00ff0, "uaddsubx%c\t%12-15r, %16-19r, %0-3r"},
  {ARM_EXT_V6, 0x06700f10, 0x0ff00ff0, "uhadd16%c\t%12-15r, %16-19r, %0-3r"},
  {ARM_EXT_V6, 0x06700f90, 0x0ff00ff0, "uhadd8%c\t%12-15r, %16-19r, %0-3r"},
  {ARM_EXT_V6, 0x06700f30, 0x0ff00ff0, "uhaddsubx%c\t%12-15r, %16-19r, %0-3r"},
  {ARM_EXT_V6, 0x06700f70, 0x0ff00ff0, "uhsub16%c\t%12-15r, %16-19r, %0-3r"},
  {ARM_EXT_V6, 0x06700ff0, 0x0ff00ff0, "uhsub8%c\t%12-15r, %16-19r, %0-3r"},
  {ARM_EXT_V6, 0x06700f50, 0x0ff00ff0, "uhsubaddx%c\t%12-15r, %16-19r, %0-3r"},
  {ARM_EXT_V6, 0x06600f10, 0x0ff00ff0, "uqadd16%c\t%12-15r, %16-19r, %0-3r"},
  {ARM_EXT_V6, 0x06600f90, 0x0ff00ff0, "uqadd8%c\t%12-15r, %16-19r, %0-3r"},
  {ARM_EXT_V6, 0x06600f30, 0x0ff00ff0, "uqaddsubx%c\t%12-15r, %16-19r, %0-3r"},
  {ARM_EXT_V6, 0x06600f70, 0x0ff00ff0, "uqsub16%c\t%12-15r, %16-19r, %0-3r"},
  {ARM_EXT_V6, 0x06600ff0, 0x0ff00ff0, "uqsub8%c\t%12-15r, %16-19r, %0-3r"},
  {ARM_EXT_V6, 0x06600f50, 0x0ff00ff0, "uqsubaddx%c\t%12-15r, %16-19r, %0-3r"},
  {ARM_EXT_V6, 0x06500f70, 0x0ff00ff0, "usub16%c\t%12-15r, %16-19r, %0-3r"},
  {ARM_EXT_V6, 0x06500ff0, 0x0ff00ff0, "usub8%c\t%12-15r, %16-19r, %0-3r"},
  {ARM_EXT_V6, 0x06500f50, 0x0ff00ff0, "usubaddx%c\t%12-15r, %16-19r, %0-3r"},
  {ARM_EXT_V6, 0x06bf0f30, 0x0fff0ff0, "rev%c\t\%12-15r, %0-3r"},
  {ARM_EXT_V6, 0x06bf0fb0, 0x0fff0ff0, "rev16%c\t\%12-15r, %0-3r"},
  {ARM_EXT_V6, 0x06ff0fb0, 0x0fff0ff0, "revsh%c\t\%12-15r, %0-3r"},
  {ARM_EXT_V6, 0xf8100a00, 0xfe50ffff, "rfe%23?id%24?ba\t\%16-19r%21'!"},
  {ARM_EXT_V6, 0x06bf0070, 0x0fff0ff0, "sxth%c %12-15r,%0-3r"},
  {ARM_EXT_V6, 0x06bf0470, 0x0fff0ff0, "sxth%c %12-15r,%0-3r, ROR #8"},
  {ARM_EXT_V6, 0x06bf0870, 0x0fff0ff0, "sxth%c %12-15r,%0-3r, ROR #16"},
  {ARM_EXT_V6, 0x06bf0c70, 0x0fff0ff0, "sxth%c %12-15r,%0-3r, ROR #24"},
  {ARM_EXT_V6, 0x068f0070, 0x0fff0ff0, "sxtb16%c %12-15r,%0-3r"},
  {ARM_EXT_V6, 0x068f0470, 0x0fff0ff0, "sxtb16%c %12-15r,%0-3r, ROR #8"},
  {ARM_EXT_V6, 0x068f0870, 0x0fff0ff0, "sxtb16%c %12-15r,%0-3r, ROR #16"},
  {ARM_EXT_V6, 0x068f0c70, 0x0fff0ff0, "sxtb16%c %12-15r,%0-3r, ROR #24"},
  {ARM_EXT_V6, 0x06af0070, 0x0fff0ff0, "sxtb%c %12-15r,%0-3r"},
  {ARM_EXT_V6, 0x06af0470, 0x0fff0ff0, "sxtb%c %12-15r,%0-3r, ROR #8"},
  {ARM_EXT_V6, 0x06af0870, 0x0fff0ff0, "sxtb%c %12-15r,%0-3r, ROR #16"},
  {ARM_EXT_V6, 0x06af0c70, 0x0fff0ff0, "sxtb%c %12-15r,%0-3r, ROR #24"},
  {ARM_EXT_V6, 0x06ff0070, 0x0fff0ff0, "uxth%c %12-15r,%0-3r"},
  {ARM_EXT_V6, 0x06ff0470, 0x0fff0ff0, "uxth%c %12-15r,%0-3r, ROR #8"},
  {ARM_EXT_V6, 0x06ff0870, 0x0fff0ff0, "uxth%c %12-15r,%0-3r, ROR #16"},
  {ARM_EXT_V6, 0x06ff0c70, 0x0fff0ff0, "uxth%c %12-15r,%0-3r, ROR #24"},
  {ARM_EXT_V6, 0x06cf0070, 0x0fff0ff0, "uxtb16%c %12-15r,%0-3r"},
  {ARM_EXT_V6, 0x06cf0470, 0x0fff0ff0, "uxtb16%c %12-15r,%0-3r, ROR #8"},
  {ARM_EXT_V6, 0x06cf0870, 0x0fff0ff0, "uxtb16%c %12-15r,%0-3r, ROR #16"},
  {ARM_EXT_V6, 0x06cf0c70, 0x0fff0ff0, "uxtb16%c %12-15r,%0-3r, ROR #24"},
  {ARM_EXT_V6, 0x06ef0070, 0x0fff0ff0, "uxtb%c %12-15r,%0-3r"},
  {ARM_EXT_V6, 0x06ef0470, 0x0fff0ff0, "uxtb%c %12-15r,%0-3r, ROR #8"},
  {ARM_EXT_V6, 0x06ef0870, 0x0fff0ff0, "uxtb%c %12-15r,%0-3r, ROR #16"},
  {ARM_EXT_V6, 0x06ef0c70, 0x0fff0ff0, "uxtb%c %12-15r,%0-3r, ROR #24"},
  {ARM_EXT_V6, 0x06b00070, 0x0ff00ff0, "sxtah%c\t%12-15r, %16-19r, %0-3r"},
  {ARM_EXT_V6, 0x06b00470, 0x0ff00ff0, "sxtah%c\t%12-15r, %16-19r, %0-3r, ROR #8"},
  {ARM_EXT_V6, 0x06b00870, 0x0ff00ff0, "sxtah%c\t%12-15r, %16-19r, %0-3r, ROR #16"},
  {ARM_EXT_V6, 0x06b00c70, 0x0ff00ff0, "sxtah%c\t%12-15r, %16-19r, %0-3r, ROR #24"},
  {ARM_EXT_V6, 0x06800070, 0x0ff00ff0, "sxtab16%c\t%12-15r, %16-19r, %0-3r"},
  {ARM_EXT_V6, 0x06800470, 0x0ff00ff0, "sxtab16%c\t%12-15r, %16-19r, %0-3r, ROR #8"},
  {ARM_EXT_V6, 0x06800870, 0x0ff00ff0, "sxtab16%c\t%12-15r, %16-19r, %0-3r, ROR #16"},
  {ARM_EXT_V6, 0x06800c70, 0x0ff00ff0, "sxtab16%c\t%12-15r, %16-19r, %0-3r, ROR #24"},
  {ARM_EXT_V6, 0x06a00070, 0x0ff00ff0, "sxtab%c\t%12-15r, %16-19r, %0-3r"},
  {ARM_EXT_V6, 0x06a00470, 0x0ff00ff0, "sxtab%c\t%12-15r, %16-19r, %0-3r, ROR #8"},
  {ARM_EXT_V6, 0x06a00870, 0x0ff00ff0, "sxtab%c\t%12-15r, %16-19r, %0-3r, ROR #16"},
  {ARM_EXT_V6, 0x06a00c70, 0x0ff00ff0, "sxtab%c\t%12-15r, %16-19r, %0-3r, ROR #24"},
  {ARM_EXT_V6, 0x06f00070, 0x0ff00ff0, "uxtah%c\t%12-15r, %16-19r, %0-3r"},
  {ARM_EXT_V6, 0x06f00470, 0x0ff00ff0, "uxtah%c\t%12-15r, %16-19r, %0-3r, ROR #8"},
  {ARM_EXT_V6, 0x06f00870, 0x0ff00ff0, "uxtah%c\t%12-15r, %16-19r, %0-3r, ROR #16"},
  {ARM_EXT_V6, 0x06f00c70, 0x0ff00ff0, "uxtah%c\t%12-15r, %16-19r, %0-3r, ROR #24"},
  {ARM_EXT_V6, 0x06c00070, 0x0ff00ff0, "uxtab16%c\t%12-15r, %16-19r, %0-3r"},
  {ARM_EXT_V6, 0x06c00470, 0x0ff00ff0, "uxtab16%c\t%12-15r, %16-19r, %0-3r, ROR #8"},
  {ARM_EXT_V6, 0x06c00870, 0x0ff00ff0, "uxtab16%c\t%12-15r, %16-19r, %0-3r, ROR #16"},
  {ARM_EXT_V6, 0x06c00c70, 0x0ff00ff0, "uxtab16%c\t%12-15r, %16-19r, %0-3r, ROR #24"},
  {ARM_EXT_V6, 0x06e00070, 0x0ff00ff0, "uxtab%c\t%12-15r, %16-19r, %0-3r"},
  {ARM_EXT_V6, 0x06e00470, 0x0ff00ff0, "uxtab%c\t%12-15r, %16-19r, %0-3r, ROR #8"},
  {ARM_EXT_V6, 0x06e00870, 0x0ff00ff0, "uxtab%c\t%12-15r, %16-19r, %0-3r, ROR #16"},
  {ARM_EXT_V6, 0x06e00c70, 0x0ff00ff0, "uxtab%c\t%12-15r, %16-19r, %0-3r, ROR #24"},
  {ARM_EXT_V6, 0x06800fb0, 0x0ff00ff0, "sel%c\t%12-15r, %16-19r, %0-3r"},
  {ARM_EXT_V6, 0xf1010000, 0xfffffc00, "setend\t%9?ble"},
  {ARM_EXT_V6, 0x0700f010, 0x0ff0f0d0, "smuad%5'x%c\t%16-19r, %0-3r, %8-11r"},
  {ARM_EXT_V6, 0x0700f050, 0x0ff0f0d0, "smusd%5'x%c\t%16-19r, %0-3r, %8-11r"},
  {ARM_EXT_V6, 0x07000010, 0x0ff000d0, "smlad%5'x%c\t%16-19r, %0-3r, %8-11r, %12-15r"},
  {ARM_EXT_V6, 0x07400010, 0x0ff000d0, "smlald%5'x%c\t%12-15r, %16-19r, %0-3r, %8-11r"},
  {ARM_EXT_V6, 0x07000050, 0x0ff000d0, "smlsd%5'x%c\t%16-19r, %0-3r, %8-11r, %12-15r"},
  {ARM_EXT_V6, 0x07400050, 0x0ff000d0, "smlsld%5'x%c\t%12-15r, %16-19r, %0-3r, %8-11r"},
  {ARM_EXT_V6, 0x0750f010, 0x0ff0f0d0, "smmul%5'r%c\t%16-19r, %0-3r, %8-11r"},
  {ARM_EXT_V6, 0x07500010, 0x0ff000d0, "smmla%5'r%c\t%16-19r, %0-3r, %8-11r, %12-15r"},
  {ARM_EXT_V6, 0x075000d0, 0x0ff000d0, "smmls%5'r%c\t%16-19r, %0-3r, %8-11r, %12-15r"},
  {ARM_EXT_V6, 0xf84d0500, 0xfe5fffe0, "srs%23?id%24?ba\t#%0-4d%21'!"},
  {ARM_EXT_V6, 0x06a00010, 0x0fe00ff0, "ssat%c\t%12-15r, #%16-20W, %0-3r"},
  {ARM_EXT_V6, 0x06a00010, 0x0fe00070, "ssat%c\t%12-15r, #%16-20W, %0-3r, LSL #%7-11d"},
  {ARM_EXT_V6, 0x06a00050, 0x0fe00070, "ssat%c\t%12-15r, #%16-20W, %0-3r, ASR #%7-11d"},
  {ARM_EXT_V6, 0x06a00f30, 0x0ff00ff0, "ssat16%c\t%12-15r, #%16-19W, %0-3r"},
  {ARM_EXT_V6, 0x01800f90, 0x0ff00ff0, "strex%c\t%12-15r, %0-3r, [%16-19r]"},
  {ARM_EXT_V6, 0x00400090, 0x0ff000f0, "umaal%c\t%12-15r, %16-19r, %0-3r, %8-11r"},
  {ARM_EXT_V6, 0x0780f010, 0x0ff0f0f0, "usad8%c\t%16-19r, %0-3r, %8-11r"},
  {ARM_EXT_V6, 0x07800010, 0x0ff000f0, "usada8%c\t%16-19r, %0-3r, %8-11r, %12-15r"},
  {ARM_EXT_V6, 0x06e00010, 0x0fe00ff0, "usat%c\t%12-15r, #%16-20d, %0-3r"},
  {ARM_EXT_V6, 0x06e00010, 0x0fe00070, "usat%c\t%12-15r, #%16-20d, %0-3r, LSL #%7-11d"},
  {ARM_EXT_V6, 0x06e00050, 0x0fe00070, "usat%c\t%12-15r, #%16-20d, %0-3r, ASR #%7-11d"},
  {ARM_EXT_V6, 0x06e00f30, 0x0ff00ff0, "usat16%c\t%12-15r, #%16-19d, %0-3r"},

  /* V5J instruction.  */
  {ARM_EXT_V5J, 0x012fff20, 0x0ffffff0, "bxj%c\t%0-3r"},

  /* V5 Instructions.  */
  {ARM_EXT_V5, 0xe1200070, 0xfff000f0, "bkpt\t0x%16-19X%12-15X%8-11X%0-3X"},
  {ARM_EXT_V5, 0xfa000000, 0xfe000000, "blx\t%B"},
  {ARM_EXT_V5, 0x012fff30, 0x0ffffff0, "blx%c\t%0-3r"},
  {ARM_EXT_V5, 0x016f0f10, 0x0fff0ff0, "clz%c\t%12-15r, %0-3r"},

  /* V5E "El Segundo" Instructions.  */    
  {ARM_EXT_V5E, 0x000000d0, 0x0e1000f0, "ldr%cd\t%12-15r, %s"},
  {ARM_EXT_V5E, 0x000000f0, 0x0e1000f0, "str%cd\t%12-15r, %s"},
  {ARM_EXT_V5E, 0xf450f000, 0xfc70f000, "pld\t%a"},
  {ARM_EXT_V5ExP, 0x01000080, 0x0ff000f0, "smlabb%c\t%16-19r, %0-3r, %8-11r, %12-15r"},
  {ARM_EXT_V5ExP, 0x010000a0, 0x0ff000f0, "smlatb%c\t%16-19r, %0-3r, %8-11r, %12-15r"},
  {ARM_EXT_V5ExP, 0x010000c0, 0x0ff000f0, "smlabt%c\t%16-19r, %0-3r, %8-11r, %12-15r"},
  {ARM_EXT_V5ExP, 0x010000e0, 0x0ff000f0, "smlatt%c\t%16-19r, %0-3r, %8-11r, %12-15r"},

  {ARM_EXT_V5ExP, 0x01200080, 0x0ff000f0, "smlawb%c\t%16-19r, %0-3r, %8-11r, %12-15r"},
  {ARM_EXT_V5ExP, 0x012000c0, 0x0ff000f0, "smlawt%c\t%16-19r, %0-3r, %8-11r, %12-15r"},

  {ARM_EXT_V5ExP, 0x01400080, 0x0ff000f0, "smlalbb%c\t%12-15r, %16-19r, %0-3r, %8-11r"},
  {ARM_EXT_V5ExP, 0x014000a0, 0x0ff000f0, "smlaltb%c\t%12-15r, %16-19r, %0-3r, %8-11r"},
  {ARM_EXT_V5ExP, 0x014000c0, 0x0ff000f0, "smlalbt%c\t%12-15r, %16-19r, %0-3r, %8-11r"},
  {ARM_EXT_V5ExP, 0x014000e0, 0x0ff000f0, "smlaltt%c\t%12-15r, %16-19r, %0-3r, %8-11r"},

  {ARM_EXT_V5ExP, 0x01600080, 0x0ff0f0f0, "smulbb%c\t%16-19r, %0-3r, %8-11r"},
  {ARM_EXT_V5ExP, 0x016000a0, 0x0ff0f0f0, "smultb%c\t%16-19r, %0-3r, %8-11r"},
  {ARM_EXT_V5ExP, 0x016000c0, 0x0ff0f0f0, "smulbt%c\t%16-19r, %0-3r, %8-11r"},
  {ARM_EXT_V5ExP, 0x016000e0, 0x0ff0f0f0, "smultt%c\t%16-19r, %0-3r, %8-11r"},

  {ARM_EXT_V5ExP, 0x012000a0, 0x0ff0f0f0, "smulwb%c\t%16-19r, %0-3r, %8-11r"},
  {ARM_EXT_V5ExP, 0x012000e0, 0x0ff0f0f0, "smulwt%c\t%16-19r, %0-3r, %8-11r"},

  {ARM_EXT_V5ExP, 0x01000050, 0x0ff00ff0,  "qadd%c\t%12-15r, %0-3r, %16-19r"},
  {ARM_EXT_V5ExP, 0x01400050, 0x0ff00ff0, "qdadd%c\t%12-15r, %0-3r, %16-19r"},
  {ARM_EXT_V5ExP, 0x01200050, 0x0ff00ff0,  "qsub%c\t%12-15r, %0-3r, %16-19r"},
  {ARM_EXT_V5ExP, 0x01600050, 0x0ff00ff0, "qdsub%c\t%12-15r, %0-3r, %16-19r"},

  /* ARM Instructions.  */
  {ARM_EXT_V1, 0x00000090, 0x0e100090, "str%c%6's%5?hb\t%12-15r, %s"},
  {ARM_EXT_V1, 0x00100090, 0x0e100090, "ldr%c%6's%5?hb\t%12-15r, %s"},
  {ARM_EXT_V1, 0x00000000, 0x0de00000, "and%c%20's\t%12-15r, %16-19r, %o"},
  {ARM_EXT_V1, 0x00200000, 0x0de00000, "eor%c%20's\t%12-15r, %16-19r, %o"},
  {ARM_EXT_V1, 0x00400000, 0x0de00000, "sub%c%20's\t%12-15r, %16-19r, %o"},
  {ARM_EXT_V1, 0x00600000, 0x0de00000, "rsb%c%20's\t%12-15r, %16-19r, %o"},
  {ARM_EXT_V1, 0x00800000, 0x0de00000, "add%c%20's\t%12-15r, %16-19r, %o"},
  {ARM_EXT_V1, 0x00a00000, 0x0de00000, "adc%c%20's\t%12-15r, %16-19r, %o"},
  {ARM_EXT_V1, 0x00c00000, 0x0de00000, "sbc%c%20's\t%12-15r, %16-19r, %o"},
  {ARM_EXT_V1, 0x00e00000, 0x0de00000, "rsc%c%20's\t%12-15r, %16-19r, %o"},
  {ARM_EXT_V3, 0x0120f000, 0x0db0f000, "msr%c\t%22?SCPSR%C, %o"},
  {ARM_EXT_V3, 0x010f0000, 0x0fbf0fff, "mrs%c\t%12-15r, %22?SCPSR"},
  {ARM_EXT_V1, 0x01000000, 0x0de00000, "tst%c%p\t%16-19r, %o"},
  {ARM_EXT_V1, 0x01200000, 0x0de00000, "teq%c%p\t%16-19r, %o"},
  {ARM_EXT_V1, 0x01400000, 0x0de00000, "cmp%c%p\t%16-19r, %o"},
  {ARM_EXT_V1, 0x01600000, 0x0de00000, "cmn%c%p\t%16-19r, %o"},
  {ARM_EXT_V1, 0x01800000, 0x0de00000, "orr%c%20's\t%12-15r, %16-19r, %o"},
  {ARM_EXT_V1, 0x01a00000, 0x0de00000, "mov%c%20's\t%12-15r, %o"},
  {ARM_EXT_V1, 0x01c00000, 0x0de00000, "bic%c%20's\t%12-15r, %16-19r, %o"},
  {ARM_EXT_V1, 0x01e00000, 0x0de00000, "mvn%c%20's\t%12-15r, %o"},
  {ARM_EXT_V1, 0x04000000, 0x0e100000, "str%c%22'b%t\t%12-15r, %a"},
  {ARM_EXT_V1, 0x06000000, 0x0e100ff0, "str%c%22'b%t\t%12-15r, %a"},
  {ARM_EXT_V1, 0x04000000, 0x0c100010, "str%c%22'b%t\t%12-15r, %a"},
  {ARM_EXT_V1, 0x06000010, 0x0e000010, "undefined"},
  {ARM_EXT_V1, 0x04100000, 0x0c100000, "ldr%c%22'b%t\t%12-15r, %a"},
  {ARM_EXT_V1, 0x08000000, 0x0e100000, "stm%c%23?id%24?ba\t%16-19r%21'!, %m%22'^"},
  {ARM_EXT_V1, 0x08100000, 0x0e100000, "ldm%c%23?id%24?ba\t%16-19r%21'!, %m%22'^"},
  {ARM_EXT_V1, 0x0a000000, 0x0e000000, "b%24'l%c\t%b"},
  {ARM_EXT_V1, 0x0f000000, 0x0f000000, "svc%c\t%0-23x"},

  /* The rest.  */
  {ARM_EXT_V1, 0x00000000, 0x00000000, "undefined instruction %0-31x"},
  {0, 0x00000000, 0x00000000, 0}
};

/* print_insn_thumb16 recognizes the following format control codes:

   %S                   print Thumb register (bits 3..5 as high number if bit 6 set)
   %D                   print Thumb register (bits 0..2 as high number if bit 7 set)
   %<bitfield>I         print bitfield as a signed decimal
   				(top bit of range being the sign bit)
   %N                   print Thumb register mask (with LR)
   %O                   print Thumb register mask (with PC)
   %M                   print Thumb register mask
   %b			print CZB's 6-bit unsigned branch destination
   %s			print Thumb right-shift immediate (6..10; 0 == 32).
   %<bitfield>r		print bitfield as an ARM register
   %<bitfield>d		print bitfield as a decimal
   %<bitfield>H         print (bitfield * 2) as a decimal
   %<bitfield>W         print (bitfield * 4) as a decimal
   %<bitfield>a         print (bitfield * 4) as a pc-rel offset + decoded symbol
   %<bitfield>B         print Thumb branch destination (signed displacement)
   %<bitfield>c         print bitfield as a condition code
   %<bitnum>'c		print specified char iff bit is one
   %<bitnum>?ab		print a if bit is one else print b.  */

static const struct opcode16 thumb_opcodes[] =
{
  /* Thumb instructions.  */

  /* ARM V6K no-argument instructions.  */
  {ARM_EXT_V6K, 0xbf00, 0xffff, "nop"},
  {ARM_EXT_V6K, 0xbf10, 0xffff, "yield"},
  {ARM_EXT_V6K, 0xbf20, 0xffff, "wfe"},
  {ARM_EXT_V6K, 0xbf30, 0xffff, "wfi"},
  {ARM_EXT_V6K, 0xbf40, 0xffff, "sev"},
  {ARM_EXT_V6K, 0xbf00, 0xff0f, "nop\t{%4-7d}"},

  /* ARM V6T2 instructions.  */
  {ARM_EXT_V6T2, 0xb900, 0xfd00, "cbnz\t%0-2r, %b"},
  {ARM_EXT_V6T2, 0xb100, 0xfd00, "cbz\t%0-2r, %b"},
  {ARM_EXT_V6T2, 0xbf08, 0xff0f, "it\t%4-7c"},
  {ARM_EXT_V6T2, 0xbf14, 0xff17, "it%3?te\t%4-7c"},
  {ARM_EXT_V6T2, 0xbf04, 0xff17, "it%3?et\t%4-7c"},
  {ARM_EXT_V6T2, 0xbf12, 0xff13, "it%3?te%2?te\t%4-7c"},
  {ARM_EXT_V6T2, 0xbf02, 0xff13, "it%3?et%2?et\t%4-7c"},
  {ARM_EXT_V6T2, 0xbf11, 0xff11, "it%3?te%2?te%1?te\t%4-7c"},
  {ARM_EXT_V6T2, 0xbf01, 0xff11, "it%3?et%2?et%1?et\t%4-7c"},

  /* ARM V6.  */
  {ARM_EXT_V6, 0xb660, 0xfff8, "cpsie\t%2'a%1'i%0'f"},
  {ARM_EXT_V6, 0xb670, 0xfff8, "cpsid\t%2'a%1'i%0'f"},
  {ARM_EXT_V6, 0x4600, 0xffc0, "mov\t%0-2r, %3-5r"},
  {ARM_EXT_V6, 0xba00, 0xffc0, "rev\t%0-2r, %3-5r"},
  {ARM_EXT_V6, 0xba40, 0xffc0, "rev16\t%0-2r, %3-5r"},
  {ARM_EXT_V6, 0xbac0, 0xffc0, "revsh\t%0-2r, %3-5r"},
  {ARM_EXT_V6, 0xb650, 0xfff7, "setend\t%3?ble"},
  {ARM_EXT_V6, 0xb200, 0xffc0, "sxth\t%0-2r, %3-5r"},
  {ARM_EXT_V6, 0xb240, 0xffc0, "sxtb\t%0-2r, %3-5r"},
  {ARM_EXT_V6, 0xb280, 0xffc0, "uxth\t%0-2r, %3-5r"},
  {ARM_EXT_V6, 0xb2c0, 0xffc0, "uxtb\t%0-2r, %3-5r"},

  /* ARM V5 ISA extends Thumb.  */
  {ARM_EXT_V5T, 0xbe00, 0xff00, "bkpt\t%0-7x"},
  /* This is BLX(2).  BLX(1) is a 32-bit instruction.  */
  {ARM_EXT_V5T, 0x4780, 0xff87, "blx\t%3-6r"},	/* note: 4 bit register number.  */
  /* ARM V4T ISA (Thumb v1).  */
  {ARM_EXT_V4T, 0x46C0, 0xFFFF, "nop\t\t\t(mov r8, r8)"},
  /* Format 4.  */
  {ARM_EXT_V4T, 0x4000, 0xFFC0, "ands\t%0-2r, %3-5r"},
  {ARM_EXT_V4T, 0x4040, 0xFFC0, "eors\t%0-2r, %3-5r"},
  {ARM_EXT_V4T, 0x4080, 0xFFC0, "lsls\t%0-2r, %3-5r"},
  {ARM_EXT_V4T, 0x40C0, 0xFFC0, "lsrs\t%0-2r, %3-5r"},
  {ARM_EXT_V4T, 0x4100, 0xFFC0, "asrs\t%0-2r, %3-5r"},
  {ARM_EXT_V4T, 0x4140, 0xFFC0, "adcs\t%0-2r, %3-5r"},
  {ARM_EXT_V4T, 0x4180, 0xFFC0, "sbcs\t%0-2r, %3-5r"},
  {ARM_EXT_V4T, 0x41C0, 0xFFC0, "rors\t%0-2r, %3-5r"},
  {ARM_EXT_V4T, 0x4200, 0xFFC0, "tst\t%0-2r, %3-5r"},
  {ARM_EXT_V4T, 0x4240, 0xFFC0, "negs\t%0-2r, %3-5r"},
  {ARM_EXT_V4T, 0x4280, 0xFFC0, "cmp\t%0-2r, %3-5r"},
  {ARM_EXT_V4T, 0x42C0, 0xFFC0, "cmn\t%0-2r, %3-5r"},
  {ARM_EXT_V4T, 0x4300, 0xFFC0, "orrs\t%0-2r, %3-5r"},
  {ARM_EXT_V4T, 0x4340, 0xFFC0, "muls\t%0-2r, %3-5r"},
  {ARM_EXT_V4T, 0x4380, 0xFFC0, "bics\t%0-2r, %3-5r"},
  {ARM_EXT_V4T, 0x43C0, 0xFFC0, "mvns\t%0-2r, %3-5r"},
  /* format 13 */
  {ARM_EXT_V4T, 0xB000, 0xFF80, "add\tsp, #%0-6W"},
  {ARM_EXT_V4T, 0xB080, 0xFF80, "sub\tsp, #%0-6W"},
  /* format 5 */
  {ARM_EXT_V4T, 0x4700, 0xFF80, "bx\t%S"},
  {ARM_EXT_V4T, 0x4400, 0xFF00, "add\t%D, %S"},
  {ARM_EXT_V4T, 0x4500, 0xFF00, "cmp\t%D, %S"},
  {ARM_EXT_V4T, 0x4600, 0xFF00, "mov\t%D, %S"},
  /* format 14 */
  {ARM_EXT_V4T, 0xB400, 0xFE00, "push\t%N"},
  {ARM_EXT_V4T, 0xBC00, 0xFE00, "pop\t%O"},
  /* format 2 */
  {ARM_EXT_V4T, 0x1800, 0xFE00, "adds\t%0-2r, %3-5r, %6-8r"},
  {ARM_EXT_V4T, 0x1A00, 0xFE00, "subs\t%0-2r, %3-5r, %6-8r"},
  {ARM_EXT_V4T, 0x1C00, 0xFE00, "adds\t%0-2r, %3-5r, #%6-8d"},
  {ARM_EXT_V4T, 0x1E00, 0xFE00, "subs\t%0-2r, %3-5r, #%6-8d"},
  /* format 8 */
  {ARM_EXT_V4T, 0x5200, 0xFE00, "strh\t%0-2r, [%3-5r, %6-8r]"},
  {ARM_EXT_V4T, 0x5A00, 0xFE00, "ldrh\t%0-2r, [%3-5r, %6-8r]"},
  {ARM_EXT_V4T, 0x5600, 0xF600, "ldrs%11?hb\t%0-2r, [%3-5r, %6-8r]"},
  /* format 7 */
  {ARM_EXT_V4T, 0x5000, 0xFA00, "str%10'b\t%0-2r, [%3-5r, %6-8r]"},
  {ARM_EXT_V4T, 0x5800, 0xFA00, "ldr%10'b\t%0-2r, [%3-5r, %6-8r]"},
  /* format 1 */
  {ARM_EXT_V4T, 0x0000, 0xF800, "lsls\t%0-2r, %3-5r, #%6-10d"},
  {ARM_EXT_V4T, 0x0800, 0xF800, "lsrs\t%0-2r, %3-5r, %s"},
  {ARM_EXT_V4T, 0x1000, 0xF800, "asrs\t%0-2r, %3-5r, %s"},
  /* format 3 */
  {ARM_EXT_V4T, 0x2000, 0xF800, "movs\t%8-10r, #%0-7d"},
  {ARM_EXT_V4T, 0x2800, 0xF800, "cmp\t%8-10r, #%0-7d"},
  {ARM_EXT_V4T, 0x3000, 0xF800, "adds\t%8-10r, #%0-7d"},
  {ARM_EXT_V4T, 0x3800, 0xF800, "subs\t%8-10r, #%0-7d"},
  /* format 6 */
  {ARM_EXT_V4T, 0x4800, 0xF800, "ldr\t%8-10r, [pc, #%0-7W]\t(%0-7a)"},  /* TODO: Disassemble PC relative "LDR rD,=<symbolic>" */
  /* format 9 */
  {ARM_EXT_V4T, 0x6000, 0xF800, "str\t%0-2r, [%3-5r, #%6-10W]"},
  {ARM_EXT_V4T, 0x6800, 0xF800, "ldr\t%0-2r, [%3-5r, #%6-10W]"},
  {ARM_EXT_V4T, 0x7000, 0xF800, "strb\t%0-2r, [%3-5r, #%6-10d]"},
  {ARM_EXT_V4T, 0x7800, 0xF800, "ldrb\t%0-2r, [%3-5r, #%6-10d]"},
  /* format 10 */
  {ARM_EXT_V4T, 0x8000, 0xF800, "strh\t%0-2r, [%3-5r, #%6-10H]"},
  {ARM_EXT_V4T, 0x8800, 0xF800, "ldrh\t%0-2r, [%3-5r, #%6-10H]"},
  /* format 11 */
  {ARM_EXT_V4T, 0x9000, 0xF800, "str\t%8-10r, [sp, #%0-7W]"},
  {ARM_EXT_V4T, 0x9800, 0xF800, "ldr\t%8-10r, [sp, #%0-7W]"},
  /* format 12 */
  {ARM_EXT_V4T, 0xA000, 0xF800, "add\t%8-10r, pc, #%0-7W\t(adr %8-10r,%0-7a)"},
  {ARM_EXT_V4T, 0xA800, 0xF800, "add\t%8-10r, sp, #%0-7W"},
  /* format 15 */
  {ARM_EXT_V4T, 0xC000, 0xF800, "stmia\t%8-10r!, %M"},
  {ARM_EXT_V4T, 0xC800, 0xF800, "ldmia\t%8-10r!, %M"},
  /* format 17 */
  {ARM_EXT_V4T, 0xDF00, 0xFF00, "svc\t%0-7d"},
  /* format 16 */
  {ARM_EXT_V4T, 0xD000, 0xF000, "b%8-11c.n\t%0-7B"},
  /* format 18 */
  {ARM_EXT_V4T, 0xE000, 0xF800, "b.n\t%0-10B"},

  /* The E800 .. FFFF range is unconditionally redirected to the
     32-bit table, because even in pre-V6T2 ISAs, BL and BLX(1) pairs
     are processed via that table.  Thus, we can never encounter a
     bare "second half of BL/BLX(1)" instruction here.  */
  {ARM_EXT_V1,  0x0000, 0x0000, "undefined"},
  {0, 0, 0, 0}
};

/* Thumb32 opcodes use the same table structure as the ARM opcodes.
   We adopt the convention that hw1 is the high 16 bits of .value and
   .mask, hw2 the low 16 bits.

   print_insn_thumb32 recognizes the following format control codes:

       %%		%

       %I		print a 12-bit immediate from hw1[10],hw2[14:12,7:0]
       %M		print a modified 12-bit immediate (same location)
       %J		print a 16-bit immediate from hw1[3:0,10],hw2[14:12,7:0]
       %K		print a 16-bit immediate from hw2[3:0],hw1[3:0],hw2[11:4]
       %S		print a possibly-shifted Rm

       %a		print the address of a plain load/store
       %w		print the width and signedness of a core load/store
       %m		print register mask for ldm/stm

       %E		print the lsb and width fields of a bfc/bfi instruction
       %F		print the lsb and width fields of a sbfx/ubfx instruction
       %b		print a conditional branch offset
       %B		print an unconditional branch offset
       %s		print the shift field of an SSAT instruction
       %R		print the rotation field of an SXT instruction
       %U		print barrier type.
       %P		print address for pli instruction.

       %<bitfield>d	print bitfield in decimal
       %<bitfield>W	print bitfield*4 in decimal
       %<bitfield>r	print bitfield as an ARM register
       %<bitfield>c	print bitfield as a condition code

       %<bitnum>'c	print "c" iff bit is one
       %<bitnum>`c	print "c" iff bit is zero
       %<bitnum>?ab	print "a" if bit is one, else "b"

   With one exception at the bottom (done because BL and BLX(1) need
   to come dead last), this table was machine-sorted first in
   decreasing order of number of bits set in the mask, then in
   increasing numeric order of mask, then in increasing numeric order
   of opcode.  This order is not the clearest for a human reader, but
   is guaranteed never to catch a special-case bit pattern with a more
   general mask, which is important, because this instruction encoding
   makes heavy use of special-case bit patterns.  */
static const struct opcode32 thumb32_opcodes[] =
{
  /* V7 instructions.  */
  {ARM_EXT_V7, 0xf910f000, 0xff70f000, "pli\t%a"},
  {ARM_EXT_V7, 0xf3af80f0, 0xfffffff0, "dbg\t#%0-3d"},
  {ARM_EXT_V7, 0xf3bf8f50, 0xfffffff0, "dmb\t%U"},
  {ARM_EXT_V7, 0xf3bf8f40, 0xfffffff0, "dsb\t%U"},
  {ARM_EXT_V7, 0xf3bf8f60, 0xfffffff0, "isb\t%U"},
  {ARM_EXT_DIV, 0xfb90f0f0, 0xfff0f0f0, "sdiv\t%8-11r, %16-19r, %0-3r"},
  {ARM_EXT_DIV, 0xfbb0f0f0, 0xfff0f0f0, "udiv\t%8-11r, %16-19r, %0-3r"},

  /* Instructions defined in the basic V6T2 set.  */
  {ARM_EXT_V6T2, 0xf3af8000, 0xffffffff, "nop.w"},
  {ARM_EXT_V6T2, 0xf3af8001, 0xffffffff, "yield.w"},
  {ARM_EXT_V6T2, 0xf3af8002, 0xffffffff, "wfe.w"},
  {ARM_EXT_V6T2, 0xf3af8003, 0xffffffff, "wfi.w"},
  {ARM_EXT_V6T2, 0xf3af9004, 0xffffffff, "sev.w"},
  {ARM_EXT_V6T2, 0xf3af8000, 0xffffff00, "nop.w\t{%0-7d}"},

  {ARM_EXT_V6T2, 0xf3bf8f2f, 0xffffffff, "clrex"},
  {ARM_EXT_V6T2, 0xf3af8400, 0xffffff1f, "cpsie.w\t%7'a%6'i%5'f"},
  {ARM_EXT_V6T2, 0xf3af8600, 0xffffff1f, "cpsid.w\t%7'a%6'i%5'f"},
  {ARM_EXT_V6T2, 0xf3c08f00, 0xfff0ffff, "bxj\t%16-19r"},
  {ARM_EXT_V6T2, 0xe810c000, 0xffd0ffff, "rfedb\t%16-19r%21'!"},
  {ARM_EXT_V6T2, 0xe990c000, 0xffd0ffff, "rfeia\t%16-19r%21'!"},
  {ARM_EXT_V6T2, 0xf3ef8000, 0xffeff000, "mrs\t%8-11r, %D"},
  {ARM_EXT_V6T2, 0xf3af8100, 0xffffffe0, "cps\t#%0-4d"},
  {ARM_EXT_V6T2, 0xe8d0f000, 0xfff0fff0, "tbb\t[%16-19r, %0-3r]"},
  {ARM_EXT_V6T2, 0xe8d0f010, 0xfff0fff0, "tbh\t[%16-19r, %0-3r, lsl #1]"},
  {ARM_EXT_V6T2, 0xf3af8500, 0xffffff00, "cpsie\t%7'a%6'i%5'f, #%0-4d"},
  {ARM_EXT_V6T2, 0xf3af8700, 0xffffff00, "cpsid\t%7'a%6'i%5'f, #%0-4d"},
  {ARM_EXT_V6T2, 0xf3de8f00, 0xffffff00, "subs\tpc, lr, #%0-7d"},
  {ARM_EXT_V6T2, 0xf3808000, 0xffe0f000, "msr\t%C, %16-19r"},
  {ARM_EXT_V6T2, 0xe8500f00, 0xfff00fff, "ldrex\t%12-15r, [%16-19r]"},
  {ARM_EXT_V6T2, 0xe8d00f4f, 0xfff00fef, "ldrex%4?hb\t%12-15r, [%16-19r]"},
  {ARM_EXT_V6T2, 0xe800c000, 0xffd0ffe0, "srsdb\t#%0-4d%21'!"},
  {ARM_EXT_V6T2, 0xe980c000, 0xffd0ffe0, "srsia\t#%0-4d%21'!"},
  {ARM_EXT_V6T2, 0xfa0ff080, 0xfffff0c0, "sxth.w\t%8-11r, %0-3r%R"},
  {ARM_EXT_V6T2, 0xfa1ff080, 0xfffff0c0, "uxth.w\t%8-11r, %0-3r%R"},
  {ARM_EXT_V6T2, 0xfa2ff080, 0xfffff0c0, "sxtb16\t%8-11r, %0-3r%R"},
  {ARM_EXT_V6T2, 0xfa3ff080, 0xfffff0c0, "uxtb16\t%8-11r, %0-3r%R"},
  {ARM_EXT_V6T2, 0xfa4ff080, 0xfffff0c0, "sxtb.w\t%8-11r, %0-3r%R"},
  {ARM_EXT_V6T2, 0xfa5ff080, 0xfffff0c0, "uxtb.w\t%8-11r, %0-3r%R"},
  {ARM_EXT_V6T2, 0xe8400000, 0xfff000ff, "strex\t%8-11r, %12-15r, [%16-19r]"},
  {ARM_EXT_V6T2, 0xe8d0007f, 0xfff000ff, "ldrexd\t%12-15r, %8-11r, [%16-19r]"},
  {ARM_EXT_V6T2, 0xfa80f000, 0xfff0f0f0, "sadd8\t%8-11r, %16-19r, %0-3r"},
  {ARM_EXT_V6T2, 0xfa80f010, 0xfff0f0f0, "qadd8\t%8-11r, %16-19r, %0-3r"},
  {ARM_EXT_V6T2, 0xfa80f020, 0xfff0f0f0, "shadd8\t%8-11r, %16-19r, %0-3r"},
  {ARM_EXT_V6T2, 0xfa80f040, 0xfff0f0f0, "uadd8\t%8-11r, %16-19r, %0-3r"},
  {ARM_EXT_V6T2, 0xfa80f050, 0xfff0f0f0, "uqadd8\t%8-11r, %16-19r, %0-3r"},
  {ARM_EXT_V6T2, 0xfa80f060, 0xfff0f0f0, "uhadd8\t%8-11r, %16-19r, %0-3r"},
  {ARM_EXT_V6T2, 0xfa80f080, 0xfff0f0f0, "qadd\t%8-11r, %0-3r, %16-19r"},
  {ARM_EXT_V6T2, 0xfa80f090, 0xfff0f0f0, "qdadd\t%8-11r, %0-3r, %16-19r"},
  {ARM_EXT_V6T2, 0xfa80f0a0, 0xfff0f0f0, "qsub\t%8-11r, %0-3r, %16-19r"},
  {ARM_EXT_V6T2, 0xfa80f0b0, 0xfff0f0f0, "qdsub\t%8-11r, %0-3r, %16-19r"},
  {ARM_EXT_V6T2, 0xfa90f000, 0xfff0f0f0, "sadd16\t%8-11r, %16-19r, %0-3r"},
  {ARM_EXT_V6T2, 0xfa90f010, 0xfff0f0f0, "qadd16\t%8-11r, %16-19r, %0-3r"},
  {ARM_EXT_V6T2, 0xfa90f020, 0xfff0f0f0, "shadd16\t%8-11r, %16-19r, %0-3r"},
  {ARM_EXT_V6T2, 0xfa90f040, 0xfff0f0f0, "uadd16\t%8-11r, %16-19r, %0-3r"},
  {ARM_EXT_V6T2, 0xfa90f050, 0xfff0f0f0, "uqadd16\t%8-11r, %16-19r, %0-3r"},
  {ARM_EXT_V6T2, 0xfa90f060, 0xfff0f0f0, "uhadd16\t%8-11r, %16-19r, %0-3r"},
  {ARM_EXT_V6T2, 0xfa90f080, 0xfff0f0f0, "rev.w\t%8-11r, %16-19r"},
  {ARM_EXT_V6T2, 0xfa90f090, 0xfff0f0f0, "rev16.w\t%8-11r, %16-19r"},
  {ARM_EXT_V6T2, 0xfa90f0a0, 0xfff0f0f0, "rbit\t%8-11r, %16-19r"},
  {ARM_EXT_V6T2, 0xfa90f0b0, 0xfff0f0f0, "revsh.w\t%8-11r, %16-19r"},
  {ARM_EXT_V6T2, 0xfaa0f000, 0xfff0f0f0, "saddsubx\t%8-11r, %16-19r, %0-3r"},
  {ARM_EXT_V6T2, 0xfaa0f010, 0xfff0f0f0, "qaddsubx\t%8-11r, %16-19r, %0-3r"},
  {ARM_EXT_V6T2, 0xfaa0f020, 0xfff0f0f0, "shaddsubx\t%8-11r, %16-19r, %0-3r"},
  {ARM_EXT_V6T2, 0xfaa0f040, 0xfff0f0f0, "uaddsubx\t%8-11r, %16-19r, %0-3r"},
  {ARM_EXT_V6T2, 0xfaa0f050, 0xfff0f0f0, "uqaddsubx\t%8-11r, %16-19r, %0-3r"},
  {ARM_EXT_V6T2, 0xfaa0f060, 0xfff0f0f0, "uhaddsubx\t%8-11r, %16-19r, %0-3r"},
  {ARM_EXT_V6T2, 0xfaa0f080, 0xfff0f0f0, "sel\t%8-11r, %16-19r, %0-3r"},
  {ARM_EXT_V6T2, 0xfab0f080, 0xfff0f0f0, "clz\t%8-11r, %16-19r"},
  {ARM_EXT_V6T2, 0xfac0f000, 0xfff0f0f0, "ssub8\t%8-11r, %16-19r, %0-3r"},
  {ARM_EXT_V6T2, 0xfac0f010, 0xfff0f0f0, "qsub8\t%8-11r, %16-19r, %0-3r"},
  {ARM_EXT_V6T2, 0xfac0f020, 0xfff0f0f0, "shsub8\t%8-11r, %16-19r, %0-3r"},
  {ARM_EXT_V6T2, 0xfac0f040, 0xfff0f0f0, "usub8\t%8-11r, %16-19r, %0-3r"},
  {ARM_EXT_V6T2, 0xfac0f050, 0xfff0f0f0, "uqsub8\t%8-11r, %16-19r, %0-3r"},
  {ARM_EXT_V6T2, 0xfac0f060, 0xfff0f0f0, "uhsub8\t%8-11r, %16-19r, %0-3r"},
  {ARM_EXT_V6T2, 0xfad0f000, 0xfff0f0f0, "ssub16\t%8-11r, %16-19r, %0-3r"},
  {ARM_EXT_V6T2, 0xfad0f010, 0xfff0f0f0, "qsub16\t%8-11r, %16-19r, %0-3r"},
  {ARM_EXT_V6T2, 0xfad0f020, 0xfff0f0f0, "shsub16\t%8-11r, %16-19r, %0-3r"},
  {ARM_EXT_V6T2, 0xfad0f040, 0xfff0f0f0, "usub16\t%8-11r, %16-19r, %0-3r"},
  {ARM_EXT_V6T2, 0xfad0f050, 0xfff0f0f0, "uqsub16\t%8-11r, %16-19r, %0-3r"},
  {ARM_EXT_V6T2, 0xfad0f060, 0xfff0f0f0, "uhsub16\t%8-11r, %16-19r, %0-3r"},
  {ARM_EXT_V6T2, 0xfae0f000, 0xfff0f0f0, "ssubaddx\t%8-11r, %16-19r, %0-3r"},
  {ARM_EXT_V6T2, 0xfae0f010, 0xfff0f0f0, "qsubaddx\t%8-11r, %16-19r, %0-3r"},
  {ARM_EXT_V6T2, 0xfae0f020, 0xfff0f0f0, "shsubaddx\t%8-11r, %16-19r, %0-3r"},
  {ARM_EXT_V6T2, 0xfae0f040, 0xfff0f0f0, "usubaddx\t%8-11r, %16-19r, %0-3r"},
  {ARM_EXT_V6T2, 0xfae0f050, 0xfff0f0f0, "uqsubaddx\t%8-11r, %16-19r, %0-3r"},
  {ARM_EXT_V6T2, 0xfae0f060, 0xfff0f0f0, "uhsubaddx\t%8-11r, %16-19r, %0-3r"},
  {ARM_EXT_V6T2, 0xfb00f000, 0xfff0f0f0, "mul.w\t%8-11r, %16-19r, %0-3r"},
  {ARM_EXT_V6T2, 0xfb70f000, 0xfff0f0f0, "usad8\t%8-11r, %16-19r, %0-3r"},
  {ARM_EXT_V6T2, 0xfa00f000, 0xffe0f0f0, "lsl%20's.w\t%8-11r, %16-19r, %0-3r"},
  {ARM_EXT_V6T2, 0xfa20f000, 0xffe0f0f0, "lsr%20's.w\t%8-11r, %16-19r, %0-3r"},
  {ARM_EXT_V6T2, 0xfa40f000, 0xffe0f0f0, "asr%20's.w\t%8-11r, %16-19r, %0-3r"},
  {ARM_EXT_V6T2, 0xfa60f000, 0xffe0f0f0, "ror%20's.w\t%8-11r, %16-19r, %0-3r"},
  {ARM_EXT_V6T2, 0xe8c00f40, 0xfff00fe0, "strex%4?hb\t%0-3r, %12-15r, [%16-19r]"},
  {ARM_EXT_V6T2, 0xf3200000, 0xfff0f0e0, "ssat16\t%8-11r, #%0-4d, %16-19r"},
  {ARM_EXT_V6T2, 0xf3a00000, 0xfff0f0e0, "usat16\t%8-11r, #%0-4d, %16-19r"},
  {ARM_EXT_V6T2, 0xfb20f000, 0xfff0f0e0, "smuad%4'x\t%8-11r, %16-19r, %0-3r"},
  {ARM_EXT_V6T2, 0xfb30f000, 0xfff0f0e0, "smulw%4?tb\t%8-11r, %16-19r, %0-3r"},
  {ARM_EXT_V6T2, 0xfb40f000, 0xfff0f0e0, "smusd%4'x\t%8-11r, %16-19r, %0-3r"},
  {ARM_EXT_V6T2, 0xfb50f000, 0xfff0f0e0, "smmul%4'r\t%8-11r, %16-19r, %0-3r"},
  {ARM_EXT_V6T2, 0xfa00f080, 0xfff0f0c0, "sxtah\t%8-11r, %16-19r, %0-3r%R"},
  {ARM_EXT_V6T2, 0xfa10f080, 0xfff0f0c0, "uxtah\t%8-11r, %16-19r, %0-3r%R"},
  {ARM_EXT_V6T2, 0xfa20f080, 0xfff0f0c0, "sxtab16\t%8-11r, %16-19r, %0-3r%R"},
  {ARM_EXT_V6T2, 0xfa30f080, 0xfff0f0c0, "uxtab16\t%8-11r, %16-19r, %0-3r%R"},
  {ARM_EXT_V6T2, 0xfa40f080, 0xfff0f0c0, "sxtab\t%8-11r, %16-19r, %0-3r%R"},
  {ARM_EXT_V6T2, 0xfa50f080, 0xfff0f0c0, "uxtab\t%8-11r, %16-19r, %0-3r%R"},
  {ARM_EXT_V6T2, 0xfb10f000, 0xfff0f0c0, "smul%5?tb%4?tb\t%8-11r, %16-19r, %0-3r"},
  {ARM_EXT_V6T2, 0xf36f0000, 0xffff8020, "bfc\t%8-11r, %E"},
  {ARM_EXT_V6T2, 0xea100f00, 0xfff08f00, "tst.w\t%16-19r, %S"},
  {ARM_EXT_V6T2, 0xea900f00, 0xfff08f00, "teq\t%16-19r, %S"},
  {ARM_EXT_V6T2, 0xeb100f00, 0xfff08f00, "cmn.w\t%16-19r, %S"},
  {ARM_EXT_V6T2, 0xebb00f00, 0xfff08f00, "cmp.w\t%16-19r, %S"},
  {ARM_EXT_V6T2, 0xf0100f00, 0xfbf08f00, "tst.w\t%16-19r, %M"},
  {ARM_EXT_V6T2, 0xf0900f00, 0xfbf08f00, "teq\t%16-19r, %M"},
  {ARM_EXT_V6T2, 0xf1100f00, 0xfbf08f00, "cmn.w\t%16-19r, %M"},
  {ARM_EXT_V6T2, 0xf1b00f00, 0xfbf08f00, "cmp.w\t%16-19r, %M"},
  {ARM_EXT_V6T2, 0xea4f0000, 0xffef8000, "mov%20's.w\t%8-11r, %S"},
  {ARM_EXT_V6T2, 0xea6f0000, 0xffef8000, "mvn%20's.w\t%8-11r, %S"},
  {ARM_EXT_V6T2, 0xe8c00070, 0xfff000f0, "strexd\t%0-3r, %12-15r, %8-11r, [%16-19r]"},
  {ARM_EXT_V6T2, 0xfb000000, 0xfff000f0, "mla\t%8-11r, %16-19r, %0-3r, %12-15r"},
  {ARM_EXT_V6T2, 0xfb000010, 0xfff000f0, "mls\t%8-11r, %16-19r, %0-3r, %12-15r"},
  {ARM_EXT_V6T2, 0xfb700000, 0xfff000f0, "usada8\t%8-11r, %16-19r, %0-3r, %12-15r"},
  {ARM_EXT_V6T2, 0xfb800000, 0xfff000f0, "smull\t%12-15r, %8-11r, %16-19r, %0-3r"},
  {ARM_EXT_V6T2, 0xfba00000, 0xfff000f0, "umull\t%12-15r, %8-11r, %16-19r, %0-3r"},
  {ARM_EXT_V6T2, 0xfbc00000, 0xfff000f0, "smlal\t%12-15r, %8-11r, %16-19r, %0-3r"},
  {ARM_EXT_V6T2, 0xfbe00000, 0xfff000f0, "umlal\t%12-15r, %8-11r, %16-19r, %0-3r"},
  {ARM_EXT_V6T2, 0xfbe00060, 0xfff000f0, "umaal\t%12-15r, %8-11r, %16-19r, %0-3r"},
  {ARM_EXT_V6T2, 0xe8500f00, 0xfff00f00, "ldrex\t%12-15r, [%16-19r, #%0-7W]"},
  {ARM_EXT_V6T2, 0xf7f08000, 0xfff0f000, "smc\t%K"},
  {ARM_EXT_V6T2, 0xf04f0000, 0xfbef8000, "mov%20's.w\t%8-11r, %M"},
  {ARM_EXT_V6T2, 0xf06f0000, 0xfbef8000, "mvn%20's.w\t%8-11r, %M"},
  {ARM_EXT_V6T2, 0xf810f000, 0xff70f000, "pld\t%a"},
  {ARM_EXT_V6T2, 0xfb200000, 0xfff000e0, "smlad%4'x\t%8-11r, %16-19r, %0-3r, %12-15r"},
  {ARM_EXT_V6T2, 0xfb300000, 0xfff000e0, "smlaw%4?tb\t%8-11r, %16-19r, %0-3r, %12-15r"},
  {ARM_EXT_V6T2, 0xfb400000, 0xfff000e0, "smlsd%4'x\t%8-11r, %16-19r, %0-3r, %12-15r"},
  {ARM_EXT_V6T2, 0xfb500000, 0xfff000e0, "smmla%4'r\t%8-11r, %16-19r, %0-3r, %12-15r"},
  {ARM_EXT_V6T2, 0xfb600000, 0xfff000e0, "smmls%4'r\t%8-11r, %16-19r, %0-3r, %12-15r"},
  {ARM_EXT_V6T2, 0xfbc000c0, 0xfff000e0, "smlald%4'x\t%12-15r, %8-11r, %16-19r, %0-3r"},
  {ARM_EXT_V6T2, 0xfbd000c0, 0xfff000e0, "smlsld%4'x\t%12-15r, %8-11r, %16-19r, %0-3r"},
  {ARM_EXT_V6T2, 0xeac00000, 0xfff08030, "pkhbt\t%8-11r, %16-19r, %S"},
  {ARM_EXT_V6T2, 0xeac00020, 0xfff08030, "pkhtb\t%8-11r, %16-19r, %S"},
  {ARM_EXT_V6T2, 0xf3400000, 0xfff08020, "sbfx\t%8-11r, %16-19r, %F"},
  {ARM_EXT_V6T2, 0xf3c00000, 0xfff08020, "ubfx\t%8-11r, %16-19r, %F"},
  {ARM_EXT_V6T2, 0xf8000e00, 0xff900f00, "str%wt\t%12-15r, %a"},
  {ARM_EXT_V6T2, 0xfb100000, 0xfff000c0, "smla%5?tb%4?tb\t%8-11r, %16-19r, %0-3r, %12-15r"},
  {ARM_EXT_V6T2, 0xfbc00080, 0xfff000c0, "smlal%5?tb%4?tb\t%12-15r, %8-11r, %16-19r, %0-3r"},
  {ARM_EXT_V6T2, 0xf3600000, 0xfff08020, "bfi\t%8-11r, %16-19r, %E"},
  {ARM_EXT_V6T2, 0xf8100e00, 0xfe900f00, "ldr%wt\t%12-15r, %a"},
  {ARM_EXT_V6T2, 0xf3000000, 0xffd08020, "ssat\t%8-11r, #%0-4d, %16-19r%s"},
  {ARM_EXT_V6T2, 0xf3800000, 0xffd08020, "usat\t%8-11r, #%0-4d, %16-19r%s"},
  {ARM_EXT_V6T2, 0xf2000000, 0xfbf08000, "addw\t%8-11r, %16-19r, %I"},
  {ARM_EXT_V6T2, 0xf2400000, 0xfbf08000, "movw\t%8-11r, %J"},
  {ARM_EXT_V6T2, 0xf2a00000, 0xfbf08000, "subw\t%8-11r, %16-19r, %I"},
  {ARM_EXT_V6T2, 0xf2c00000, 0xfbf08000, "movt\t%8-11r, %J"},
  {ARM_EXT_V6T2, 0xea000000, 0xffe08000, "and%20's.w\t%8-11r, %16-19r, %S"},
  {ARM_EXT_V6T2, 0xea200000, 0xffe08000, "bic%20's.w\t%8-11r, %16-19r, %S"},
  {ARM_EXT_V6T2, 0xea400000, 0xffe08000, "orr%20's.w\t%8-11r, %16-19r, %S"},
  {ARM_EXT_V6T2, 0xea600000, 0xffe08000, "orn%20's\t%8-11r, %16-19r, %S"},
  {ARM_EXT_V6T2, 0xea800000, 0xffe08000, "eor%20's.w\t%8-11r, %16-19r, %S"},
  {ARM_EXT_V6T2, 0xeb000000, 0xffe08000, "add%20's.w\t%8-11r, %16-19r, %S"},
  {ARM_EXT_V6T2, 0xeb400000, 0xffe08000, "adc%20's.w\t%8-11r, %16-19r, %S"},
  {ARM_EXT_V6T2, 0xeb600000, 0xffe08000, "sbc%20's.w\t%8-11r, %16-19r, %S"},
  {ARM_EXT_V6T2, 0xeba00000, 0xffe08000, "sub%20's.w\t%8-11r, %16-19r, %S"},
  {ARM_EXT_V6T2, 0xebc00000, 0xffe08000, "rsb%20's\t%8-11r, %16-19r, %S"},
  {ARM_EXT_V6T2, 0xe8400000, 0xfff00000, "strex\t%8-11r, %12-15r, [%16-19r, #%0-7W]"},
  {ARM_EXT_V6T2, 0xf0000000, 0xfbe08000, "and%20's.w\t%8-11r, %16-19r, %M"},
  {ARM_EXT_V6T2, 0xf0200000, 0xfbe08000, "bic%20's.w\t%8-11r, %16-19r, %M"},
  {ARM_EXT_V6T2, 0xf0400000, 0xfbe08000, "orr%20's.w\t%8-11r, %16-19r, %M"},
  {ARM_EXT_V6T2, 0xf0600000, 0xfbe08000, "orn%20's\t%8-11r, %16-19r, %M"},
  {ARM_EXT_V6T2, 0xf0800000, 0xfbe08000, "eor%20's.w\t%8-11r, %16-19r, %M"},
  {ARM_EXT_V6T2, 0xf1000000, 0xfbe08000, "add%20's.w\t%8-11r, %16-19r, %M"},
  {ARM_EXT_V6T2, 0xf1400000, 0xfbe08000, "adc%20's.w\t%8-11r, %16-19r, %M"},
  {ARM_EXT_V6T2, 0xf1600000, 0xfbe08000, "sbc%20's.w\t%8-11r, %16-19r, %M"},
  {ARM_EXT_V6T2, 0xf1a00000, 0xfbe08000, "sub%20's.w\t%8-11r, %16-19r, %M"},
  {ARM_EXT_V6T2, 0xf1c00000, 0xfbe08000, "rsb%20's\t%8-11r, %16-19r, %M"},
  {ARM_EXT_V6T2, 0xe8800000, 0xffd00000, "stmia.w\t%16-19r%21'!, %m"},
  {ARM_EXT_V6T2, 0xe8900000, 0xffd00000, "ldmia.w\t%16-19r%21'!, %m"},
  {ARM_EXT_V6T2, 0xe9000000, 0xffd00000, "stmdb\t%16-19r%21'!, %m"},
  {ARM_EXT_V6T2, 0xe9100000, 0xffd00000, "ldmdb\t%16-19r%21'!, %m"},
  {ARM_EXT_V6T2, 0xe9c00000, 0xffd000ff, "strd\t%12-15r, %8-11r, [%16-19r]"},
  {ARM_EXT_V6T2, 0xe9d00000, 0xffd000ff, "ldrd\t%12-15r, %8-11r, [%16-19r]"},
  {ARM_EXT_V6T2, 0xe9400000, 0xff500000, "strd\t%12-15r, %8-11r, [%16-19r, #%23`-%0-7W]"},
  {ARM_EXT_V6T2, 0xe9500000, 0xff500000, "ldrd\t%12-15r, %8-11r, [%16-19r, #%23`-%0-7W]"},
  {ARM_EXT_V6T2, 0xf8000000, 0xff100000, "str%w.w\t%12-15r, %a"},
  {ARM_EXT_V6T2, 0xf8100000, 0xfe100000, "ldr%w.w\t%12-15r, %a"},

  /* Filter out Bcc with cond=E or F, which are used for other instructions.  */
  {ARM_EXT_V6T2, 0xf3c08000, 0xfbc0d000, "undefined (bcc, cond=0xF)"},
  {ARM_EXT_V6T2, 0xf3808000, 0xfbc0d000, "undefined (bcc, cond=0xE)"},
  {ARM_EXT_V6T2, 0xf0008000, 0xf800d000, "b%22-25c.w\t%b"},
  {ARM_EXT_V6T2, 0xf0009000, 0xf800d000, "b.w\t%B"},

  /* These have been 32-bit since the invention of Thumb.  */
  {ARM_EXT_V4T,  0xf000c000, 0xf800d000, "blx\t%B"},
  {ARM_EXT_V4T,  0xf000d000, 0xf800d000, "bl\t%B"},

  /* Fallback.  */
  {ARM_EXT_V1,   0x00000000, 0x00000000, "undefined"},
  {0, 0, 0, 0}
};
   
static const char *const arm_conditional[] =
{"eq", "ne", "cs", "cc", "mi", "pl", "vs", "vc",
 "hi", "ls", "ge", "lt", "gt", "le", "", "<und>"};

static const char *const arm_fp_const[] =
{"0.0", "1.0", "2.0", "3.0", "4.0", "5.0", "0.5", "10.0"};

static const char *const arm_shift[] =
{"lsl", "lsr", "asr", "ror"};

typedef struct
{
  const char *name;
  const char *description;
  const char *reg_names[16];
}
arm_regname;

static const arm_regname regnames[] =
{
  { "raw" , "Select raw register names",
    { "r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7", "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15"}},
  { "gcc",  "Select register names used by GCC",
    { "r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7", "r8", "r9", "sl",  "fp",  "ip",  "sp",  "lr",  "pc" }},
  { "std",  "Select register names used in ARM's ISA documentation",
    { "r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7", "r8", "r9", "r10", "r11", "r12", "sp",  "lr",  "pc" }},
  { "apcs", "Select register names used in the APCS",
    { "a1", "a2", "a3", "a4", "v1", "v2", "v3", "v4", "v5", "v6", "sl",  "fp",  "ip",  "sp",  "lr",  "pc" }},
  { "atpcs", "Select register names used in the ATPCS",
    { "a1", "a2", "a3", "a4", "v1", "v2", "v3", "v4", "v5", "v6", "v7",  "v8",  "IP",  "SP",  "LR",  "PC" }},
  { "special-atpcs", "Select special register names used in the ATPCS",
    { "a1", "a2", "a3", "a4", "v1", "v2", "v3", "WR", "v5", "SB", "SL",  "FP",  "IP",  "SP",  "LR",  "PC" }},
};

static const char *const iwmmxt_wwnames[] =
{"b", "h", "w", "d"};

static const char *const iwmmxt_wwssnames[] =
{"b", "bus", "b", "bss",
 "h", "hus", "h", "hss",
 "w", "wus", "w", "wss",
 "d", "dus", "d", "dss"
};

static const char *const iwmmxt_regnames[] =
{ "wr0", "wr1", "wr2", "wr3", "wr4", "wr5", "wr6", "wr7",
  "wr8", "wr9", "wr10", "wr11", "wr12", "wr13", "wr14", "wr15"
};

static const char *const iwmmxt_cregnames[] =
{ "wcid", "wcon", "wcssf", "wcasf", "reserved", "reserved", "reserved", "reserved",
  "wcgr0", "wcgr1", "wcgr2", "wcgr3", "reserved", "reserved", "reserved", "reserved"
};

/* Default to GCC register name set.  */
static unsigned int regname_selected = 1;

#define NUM_ARM_REGNAMES  NUM_ELEM (regnames)
#define arm_regnames      regnames[regname_selected].reg_names

static bfd_boolean force_thumb = FALSE;


/* Functions.  */
int
get_arm_regname_num_options (void)
{
  return NUM_ARM_REGNAMES;
}

int
set_arm_regname_option (int option)
{
  int old = regname_selected;
  regname_selected = option;
  return old;
}

int
get_arm_regnames (int option, const char **setname, const char **setdescription,
		  const char *const **register_names)
{
  *setname = regnames[option].name;
  *setdescription = regnames[option].description;
  *register_names = regnames[option].reg_names;
  return 16;
}

static void
arm_decode_shift (long given, fprintf_ftype func, void *stream)
{
  func (stream, "%s", arm_regnames[given & 0xf]);

  if ((given & 0xff0) != 0)
    {
      if ((given & 0x10) == 0)
	{
	  int amount = (given & 0xf80) >> 7;
	  int shift = (given & 0x60) >> 5;

	  if (amount == 0)
	    {
	      if (shift == 3)
		{
		  func (stream, ", rrx");
		  return;
		}

	      amount = 32;
	    }

	  func (stream, ", %s #%d", arm_shift[shift], amount);
	}
      else
	func (stream, ", %s %s", arm_shift[(given & 0x60) >> 5],
	      arm_regnames[(given & 0xf00) >> 8]);
    }
}

/* Print one coprocessor instruction on INFO->STREAM.
   Return TRUE if the instuction matched, FALSE if this is not a
   recognised coprocessor instruction.  */

static bfd_boolean
print_insn_coprocessor (struct disassemble_info *info, long given,
			bfd_boolean thumb)
{
  const struct opcode32 *insn;
  void *stream = info->stream;
  fprintf_ftype func = info->fprintf_func;
  unsigned long mask;
  unsigned long value;

  for (insn = coprocessor_opcodes; insn->assembler; insn++)
    {
      if (insn->value == FIRST_IWMMXT_INSN
	  && info->mach != bfd_mach_arm_XScale
	  && info->mach != bfd_mach_arm_iWMMXt)
	insn = insn + IWMMXT_INSN_COUNT;

      mask = insn->mask;
      value = insn->value;
      if (thumb)
	{
	  /* The high 4 bits are 0xe for Arm conditional instructions, and
	     0xe for arm unconditional instructions.  The rest of the
	     encoding is the same.  */
	  mask |= 0xf0000000;
	  value |= 0xe0000000;
	}
      else
	{
	  /* Only match unconditional instuctions against unconditional
	     patterns.  */
	  if ((given & 0xf0000000) == 0xf0000000)
	    mask |= 0xf0000000;
	}
      if ((given & mask) == value)
	{
	  const char *c;

	  for (c = insn->assembler; *c; c++)
	    {
	      if (*c == '%')
		{
		  switch (*++c)
		    {
		    case '%':
		      func (stream, "%%");
		      break;

		    case 'A':
		      func (stream, "[%s", arm_regnames [(given >> 16) & 0xf]);

		      if ((given & (1 << 24)) != 0)
			{
			  int offset = given & 0xff;

			  if (offset)
			    func (stream, ", #%s%d]%s",
				  ((given & 0x00800000) == 0 ? "-" : ""),
				  offset * 4,
				  ((given & 0x00200000) != 0 ? "!" : ""));
			  else
			    func (stream, "]");
			}
		      else
			{
			  int offset = given & 0xff;

			  func (stream, "]");

			  if (given & (1 << 21))
			    {
			      if (offset)
				func (stream, ", #%s%d",
				      ((given & 0x00800000) == 0 ? "-" : ""),
				      offset * 4);
			    }
			  else
			    func (stream, ", {%d}", offset);
			}
		      break;

		    case 'c':
		      func (stream, "%s",
			    arm_conditional [(given >> 28) & 0xf]);
		      break;

		    case 'I':
		      /* Print a Cirrus/DSP shift immediate.  */
		      /* Immediates are 7bit signed ints with bits 0..3 in
			 bits 0..3 of opcode and bits 4..6 in bits 5..7
			 of opcode.  */
		      {
			int imm;

			imm = (given & 0xf) | ((given & 0xe0) >> 1);

			/* Is ``imm'' a negative number?  */
			if (imm & 0x40)
			  imm |= (-1 << 7);

			func (stream, "%d", imm);
		      }

		      break;

		    case 'F':
		      switch (given & 0x00408000)
			{
			case 0:
			  func (stream, "4");
			  break;
			case 0x8000:
			  func (stream, "1");
			  break;
			case 0x00400000:
			  func (stream, "2");
			  break;
			default:
			  func (stream, "3");
			}
		      break;

		    case 'P':
		      switch (given & 0x00080080)
			{
			case 0:
			  func (stream, "s");
			  break;
			case 0x80:
			  func (stream, "d");
			  break;
			case 0x00080000:
			  func (stream, "e");
			  break;
			default:
			  func (stream, _("<illegal precision>"));
			  break;
			}
		      break;
		    case 'Q':
		      switch (given & 0x00408000)
			{
			case 0:
			  func (stream, "s");
			  break;
			case 0x8000:
			  func (stream, "d");
			  break;
			case 0x00400000:
			  func (stream, "e");
			  break;
			default:
			  func (stream, "p");
			  break;
			}
		      break;
		    case 'R':
		      switch (given & 0x60)
			{
			case 0:
			  break;
			case 0x20:
			  func (stream, "p");
			  break;
			case 0x40:
			  func (stream, "m");
			  break;
			default:
			  func (stream, "z");
			  break;
			}
		      break;

		    case '0': case '1': case '2': case '3': case '4':
		    case '5': case '6': case '7': case '8': case '9':
		      {
			int bitstart = *c++ - '0';
			int bitend = 0;
			while (*c >= '0' && *c <= '9')
			  bitstart = (bitstart * 10) + *c++ - '0';

			switch (*c)
			  {
			  case '-':
			    c++;

			    while (*c >= '0' && *c <= '9')
			      bitend = (bitend * 10) + *c++ - '0';

			    if (!bitend)
			      abort ();

			    switch (*c)
			      {
			      case 'r':
				{
				  long reg;

				  reg = given >> bitstart;
				  reg &= (2 << (bitend - bitstart)) - 1;

				  func (stream, "%s", arm_regnames[reg]);
				}
				break;
			      case 'd':
				{
				  long reg;

				  reg = given >> bitstart;
				  reg &= (2 << (bitend - bitstart)) - 1;

				  func (stream, "%ld", reg);
				}
				break;
			      case 'f':
				{
				  long reg;

				  reg = given >> bitstart;
				  reg &= (2 << (bitend - bitstart)) - 1;

				  if (reg > 7)
				    func (stream, "#%s",
					  arm_fp_const[reg & 7]);
				  else
				    func (stream, "f%ld", reg);
				}
				break;

			      case 'w':
				{
				  long reg;

				  if (bitstart != bitend)
				    {
				      reg = given >> bitstart;
				      reg &= (2 << (bitend - bitstart)) - 1;
				      if (bitend - bitstart == 1)
					func (stream, "%s", iwmmxt_wwnames[reg]);
				      else
					func (stream, "%s", iwmmxt_wwssnames[reg]);
				    }
				  else
				    {
				      reg = (((given >> 8)  & 0x1) |
					     ((given >> 22) & 0x1));
				      func (stream, "%s", iwmmxt_wwnames[reg]);
				    }
				}
				break;

			      case 'g':
				{
				  long reg;
				  reg = given >> bitstart;
				  reg &= (2 << (bitend - bitstart)) - 1;
				  func (stream, "%s", iwmmxt_regnames[reg]);
				}
				break;

			      case 'G':
				{
				  long reg;
				  reg = given >> bitstart;
				  reg &= (2 << (bitend - bitstart)) - 1;
				  func (stream, "%s", iwmmxt_cregnames[reg]);
				}
				break;

			      default:
				abort ();
			      }
			    break;

			  case 'y':
			  case 'z':
			    {
			      int single = *c == 'y';
			      int regno;

			      switch (bitstart)
				{
				case 4: /* Sm pair */
				  func (stream, "{");
				  /* Fall through.  */
				case 0: /* Sm, Dm */
				  regno = given & 0x0000000f;
				  if (single)
				    {
				      regno <<= 1;
				      regno += (given >> 5) & 1;
				    }
				  break;

				case 1: /* Sd, Dd */
				  regno = (given >> 12) & 0x0000000f;
				  if (single)
				    {
				      regno <<= 1;
				      regno += (given >> 22) & 1;
				    }
				  break;

				case 2: /* Sn, Dn */
				  regno = (given >> 16) & 0x0000000f;
				  if (single)
				    {
				      regno <<= 1;
				      regno += (given >> 7) & 1;
				    }
				  break;

				case 3: /* List */
				  func (stream, "{");
				  regno = (given >> 12) & 0x0000000f;
				  if (single)
				    {
				      regno <<= 1;
				      regno += (given >> 22) & 1;
				    }
				  break;


				default:
				  abort ();
				}

			      func (stream, "%c%d", single ? 's' : 'd', regno);

			      if (bitstart == 3)
				{
				  int count = given & 0xff;

				  if (single == 0)
				    count >>= 1;

				  if (--count)
				    {
				      func (stream, "-%c%d",
					    single ? 's' : 'd',
					    regno + count);
				    }

				  func (stream, "}");
				}
			      else if (bitstart == 4)
				func (stream, ", %c%d}", single ? 's' : 'd',
				      regno + 1);

			      break;
			    }

			    break;

			  case '`':
			    c++;
			    if ((given & (1 << bitstart)) == 0)
			      func (stream, "%c", *c);
			    break;
			  case '\'':
			    c++;
			    if ((given & (1 << bitstart)) != 0)
			      func (stream, "%c", *c);
			    break;
			  case '?':
			    ++c;
			    if ((given & (1 << bitstart)) != 0)
			      func (stream, "%c", *c++);
			    else
			      func (stream, "%c", *++c);
			    break;
			  default:
			    abort ();
			  }
			break;

		      case 'L':
			switch (given & 0x00400100)
			  {
			  case 0x00000000: func (stream, "b"); break;
			  case 0x00400000: func (stream, "h"); break;
			  case 0x00000100: func (stream, "w"); break;
			  case 0x00400100: func (stream, "d"); break;
			  default:
			    break;
			  }
			break;

		      case 'Z':
			{
			  int value;
			  /* given (20, 23) | given (0, 3) */
			  value = ((given >> 16) & 0xf0) | (given & 0xf);
			  func (stream, "%d", value);
			}
			break;

		      case 'l':
			/* This is like the 'A' operator, except that if
			   the width field "M" is zero, then the offset is
			   *not* multiplied by four.  */
			{
			  int offset = given & 0xff;
			  int multiplier = (given & 0x00000100) ? 4 : 1;

			  func (stream, "[%s", arm_regnames [(given >> 16) & 0xf]);

			  if (offset)
			    {
			      if ((given & 0x01000000) != 0)
				func (stream, ", #%s%d]%s",
				      ((given & 0x00800000) == 0 ? "-" : ""),
				      offset * multiplier,
				      ((given & 0x00200000) != 0 ? "!" : ""));
			      else
				func (stream, "], #%s%d",
				      ((given & 0x00800000) == 0 ? "-" : ""),
				      offset * multiplier);
			    }
			  else
			    func (stream, "]");
			}
			break;

		      default:
			abort ();
		      }
		    }
		}
	      else
		func (stream, "%c", *c);
	    }
	  return TRUE;
	}
    }
  return FALSE;
}

static void
print_arm_address (bfd_vma pc, struct disassemble_info *info, long given)
{
  void *stream = info->stream;
  fprintf_ftype func = info->fprintf_func;

  if (((given & 0x000f0000) == 0x000f0000)
      && ((given & 0x02000000) == 0))
    {
      int offset = given & 0xfff;

      func (stream, "[pc");

      if (given & 0x01000000)
	{
	  if ((given & 0x00800000) == 0)
	    offset = - offset;

	  /* Pre-indexed.  */
	  func (stream, ", #%d]", offset);

	  offset += pc + 8;

	  /* Cope with the possibility of write-back
	     being used.  Probably a very dangerous thing
	     for the programmer to do, but who are we to
	     argue ?  */
	  if (given & 0x00200000)
	    func (stream, "!");
	}
      else
	{
	  /* Post indexed.  */
	  func (stream, "], #%d", offset);

	  /* ie ignore the offset.  */
	  offset = pc + 8;
	}

      func (stream, "\t; ");
      info->print_address_func (offset, info);
    }
  else
    {
      func (stream, "[%s",
	    arm_regnames[(given >> 16) & 0xf]);
      if ((given & 0x01000000) != 0)
	{
	  if ((given & 0x02000000) == 0)
	    {
	      int offset = given & 0xfff;
	      if (offset)
		func (stream, ", #%s%d",
		      (((given & 0x00800000) == 0)
		       ? "-" : ""), offset);
	    }
	  else
	    {
	      func (stream, ", %s",
		    (((given & 0x00800000) == 0)
		     ? "-" : ""));
	      arm_decode_shift (given, func, stream);
	    }

	  func (stream, "]%s",
		((given & 0x00200000) != 0) ? "!" : "");
	}
      else
	{
	  if ((given & 0x02000000) == 0)
	    {
	      int offset = given & 0xfff;
	      if (offset)
		func (stream, "], #%s%d",
		      (((given & 0x00800000) == 0)
		       ? "-" : ""), offset);
	      else
		func (stream, "]");
	    }
	  else
	    {
	      func (stream, "], %s",
		    (((given & 0x00800000) == 0)
		     ? "-" : ""));
	      arm_decode_shift (given, func, stream);
	    }
	}
    }
}

/* Print one ARM instruction from PC on INFO->STREAM.  */

static void
print_insn_arm (bfd_vma pc, struct disassemble_info *info, long given)
{
  const struct opcode32 *insn;
  void *stream = info->stream;
  fprintf_ftype func = info->fprintf_func;

  if (print_insn_coprocessor (info, given, FALSE))
    return;

  for (insn = arm_opcodes; insn->assembler; insn++)
    {
      if (insn->value == FIRST_IWMMXT_INSN
	  && info->mach != bfd_mach_arm_XScale
	  && info->mach != bfd_mach_arm_iWMMXt)
	insn = insn + IWMMXT_INSN_COUNT;

      if ((given & insn->mask) == insn->value
	  /* Special case: an instruction with all bits set in the condition field
	     (0xFnnn_nnnn) is only matched if all those bits are set in insn->mask,
	     or by the catchall at the end of the table.  */
	  && ((given & 0xF0000000) != 0xF0000000
	      || (insn->mask & 0xF0000000) == 0xF0000000
	      || (insn->mask == 0 && insn->value == 0)))
	{
	  const char *c;

	  for (c = insn->assembler; *c; c++)
	    {
	      if (*c == '%')
		{
		  switch (*++c)
		    {
		    case '%':
		      func (stream, "%%");
		      break;

		    case 'a':
		      print_arm_address (pc, info, given);
		      break;

		    case 'P':
		      /* Set P address bit and use normal address
			 printing routine.  */
		      print_arm_address (pc, info, given | (1 << 24));
		      break;

		    case 's':
                      if ((given & 0x004f0000) == 0x004f0000)
			{
                          /* PC relative with immediate offset.  */
			  int offset = ((given & 0xf00) >> 4) | (given & 0xf);

			  if ((given & 0x00800000) == 0)
			    offset = -offset;

			  func (stream, "[pc, #%d]\t; ", offset);
			  info->print_address_func (offset + pc + 8, info);
			}
		      else
			{
			  func (stream, "[%s",
				arm_regnames[(given >> 16) & 0xf]);
			  if ((given & 0x01000000) != 0)
			    {
                              /* Pre-indexed.  */
			      if ((given & 0x00400000) == 0x00400000)
				{
                                  /* Immediate.  */
                                  int offset = ((given & 0xf00) >> 4) | (given & 0xf);
				  if (offset)
				    func (stream, ", #%s%d",
					  (((given & 0x00800000) == 0)
					   ? "-" : ""), offset);
				}
			      else
				{
                                  /* Register.  */
				  func (stream, ", %s%s",
					(((given & 0x00800000) == 0)
					 ? "-" : ""),
                                        arm_regnames[given & 0xf]);
				}

			      func (stream, "]%s",
				    ((given & 0x00200000) != 0) ? "!" : "");
			    }
			  else
			    {
                              /* Post-indexed.  */
			      if ((given & 0x00400000) == 0x00400000)
				{
                                  /* Immediate.  */
                                  int offset = ((given & 0xf00) >> 4) | (given & 0xf);
				  if (offset)
				    func (stream, "], #%s%d",
					  (((given & 0x00800000) == 0)
					   ? "-" : ""), offset);
				  else
				    func (stream, "]");
				}
			      else
				{
                                  /* Register.  */
				  func (stream, "], %s%s",
					(((given & 0x00800000) == 0)
					 ? "-" : ""),
                                        arm_regnames[given & 0xf]);
				}
			    }
			}
		      break;

		    case 'b':
		      {
			int disp = (((given & 0xffffff) ^ 0x800000) - 0x800000);
			info->print_address_func (disp*4 + pc + 8, info);
		      }
		      break;

		    case 'c':
		      func (stream, "%s",
			    arm_conditional [(given >> 28) & 0xf]);
		      break;

		    case 'm':
		      {
			int started = 0;
			int reg;

			func (stream, "{");
			for (reg = 0; reg < 16; reg++)
			  if ((given & (1 << reg)) != 0)
			    {
			      if (started)
				func (stream, ", ");
			      started = 1;
			      func (stream, "%s", arm_regnames[reg]);
			    }
			func (stream, "}");
		      }
		      break;

		    case 'o':
		      if ((given & 0x02000000) != 0)
			{
			  int rotate = (given & 0xf00) >> 7;
			  int immed = (given & 0xff);
			  immed = (((immed << (32 - rotate))
				    | (immed >> rotate)) & 0xffffffff);
			  func (stream, "#%d\t; 0x%x", immed, immed);
			}
		      else
			arm_decode_shift (given, func, stream);
		      break;

		    case 'p':
		      if ((given & 0x0000f000) == 0x0000f000)
			func (stream, "p");
		      break;

		    case 't':
		      if ((given & 0x01200000) == 0x00200000)
			func (stream, "t");
		      break;

		    case 'A':
		      func (stream, "[%s", arm_regnames [(given >> 16) & 0xf]);

		      if ((given & (1 << 24)) != 0)
			{
			  int offset = given & 0xff;

			  if (offset)
			    func (stream, ", #%s%d]%s",
				  ((given & 0x00800000) == 0 ? "-" : ""),
				  offset * 4,
				  ((given & 0x00200000) != 0 ? "!" : ""));
			  else
			    func (stream, "]");
			}
		      else
			{
			  int offset = given & 0xff;

			  func (stream, "]");

			  if (given & (1 << 21))
			    {
			      if (offset)
				func (stream, ", #%s%d",
				      ((given & 0x00800000) == 0 ? "-" : ""),
				      offset * 4);
			    }
			  else
			    func (stream, ", {%d}", offset);
			}
		      break;

		    case 'B':
		      /* Print ARM V5 BLX(1) address: pc+25 bits.  */
		      {
			bfd_vma address;
			bfd_vma offset = 0;

			if (given & 0x00800000)
			  /* Is signed, hi bits should be ones.  */
			  offset = (-1) ^ 0x00ffffff;

			/* Offset is (SignExtend(offset field)<<2).  */
			offset += given & 0x00ffffff;
			offset <<= 2;
			address = offset + pc + 8;

			if (given & 0x01000000)
			  /* H bit allows addressing to 2-byte boundaries.  */
			  address += 2;

		        info->print_address_func (address, info);
		      }
		      break;

		    case 'C':
		      func (stream, "_");
		      if (given & 0x80000)
			func (stream, "f");
		      if (given & 0x40000)
			func (stream, "s");
		      if (given & 0x20000)
			func (stream, "x");
		      if (given & 0x10000)
			func (stream, "c");
		      break;

		    case 'U':
		      switch (given & 0xf)
			{
			case 0xf: func(stream, "sy"); break;
			case 0x7: func(stream, "un"); break;
			case 0xe: func(stream, "st"); break;
			case 0x6: func(stream, "unst"); break;
			default:
			  func(stream, "#%d", (int)given & 0xf);
			  break;
			}
		      break;

		    case '0': case '1': case '2': case '3': case '4':
		    case '5': case '6': case '7': case '8': case '9':
		      {
			int bitstart = *c++ - '0';
			int bitend = 0;
			while (*c >= '0' && *c <= '9')
			  bitstart = (bitstart * 10) + *c++ - '0';

			switch (*c)
			  {
			  case '-':
			    c++;

			    while (*c >= '0' && *c <= '9')
			      bitend = (bitend * 10) + *c++ - '0';

			    if (!bitend)
			      abort ();

			    switch (*c)
			      {
			      case 'r':
				{
				  long reg;

				  reg = given >> bitstart;
				  reg &= (2 << (bitend - bitstart)) - 1;

				  func (stream, "%s", arm_regnames[reg]);
				}
				break;
			      case 'd':
				{
				  long reg;

				  reg = given >> bitstart;
				  reg &= (2 << (bitend - bitstart)) - 1;

				  func (stream, "%ld", reg);
				}
				break;
			      case 'W':
				{
				  long reg;
				  
				  reg = given >> bitstart;
				  reg &= (2 << (bitend - bitstart)) - 1;
				  
				  func (stream, "%ld", reg + 1);
				}
				break;
			      case 'x':
				{
				  long reg;

				  reg = given >> bitstart;
				  reg &= (2 << (bitend - bitstart)) - 1;

				  func (stream, "0x%08lx", reg);

				  /* Some SWI instructions have special
				     meanings.  */
				  if ((given & 0x0fffffff) == 0x0FF00000)
				    func (stream, "\t; IMB");
				  else if ((given & 0x0fffffff) == 0x0FF00001)
				    func (stream, "\t; IMBRange");
				}
				break;
			      case 'X':
				{
				  long reg;

				  reg = given >> bitstart;
				  reg &= (2 << (bitend - bitstart)) - 1;

				  func (stream, "%01lx", reg & 0xf);
				}
				break;
			      default:
				abort ();
			      }
			    break;

			  case '`':
			    c++;
			    if ((given & (1 << bitstart)) == 0)
			      func (stream, "%c", *c);
			    break;
			  case '\'':
			    c++;
			    if ((given & (1 << bitstart)) != 0)
			      func (stream, "%c", *c);
			    break;
			  case '?':
			    ++c;
			    if ((given & (1 << bitstart)) != 0)
			      func (stream, "%c", *c++);
			    else
			      func (stream, "%c", *++c);
			    break;
			  default:
			    abort ();
			  }
			break;

		      case 'e':
			{
			  int imm;

			  imm = (given & 0xf) | ((given & 0xfff00) >> 4);
			  func (stream, "%d", imm);
			}
			break;

		      case 'E':
			/* LSB and WIDTH fields of BFI or BFC.  The machine-
			   language instruction encodes LSB and MSB.  */
			{
			  long msb = (given & 0x001f0000) >> 16;
			  long lsb = (given & 0x00000f80) >> 7;

			  long width = msb - lsb + 1;
			  if (width > 0)
			    func (stream, "#%lu, #%lu", lsb, width);
			  else
			    func (stream, "(invalid: %lu:%lu)", lsb, msb);
			}
			break;

		      case 'V':
			/* 16-bit unsigned immediate from a MOVT or MOVW
			   instruction, encoded in bits 0:11 and 15:19.  */
			{
			  long hi = (given & 0x000f0000) >> 4;
			  long lo = (given & 0x00000fff);
			  long imm16 = hi | lo;
			  func (stream, "#%lu\t; 0x%lx", imm16, imm16);
			}
			break;

		      default:
			abort ();
		      }
		    }
		}
	      else
		func (stream, "%c", *c);
	    }
	  return;
	}
    }
  abort ();
}

/* Print one 16-bit Thumb instruction from PC on INFO->STREAM.  */

static void
print_insn_thumb16 (bfd_vma pc, struct disassemble_info *info, long given)
{
  const struct opcode16 *insn;
  void *stream = info->stream;
  fprintf_ftype func = info->fprintf_func;

  for (insn = thumb_opcodes; insn->assembler; insn++)
    if ((given & insn->mask) == insn->value)
      {
	const char *c = insn->assembler;
	for (; *c; c++)
	  {
	    int domaskpc = 0;
	    int domasklr = 0;

	    if (*c != '%')
	      {
		func (stream, "%c", *c);
		continue;
	      }

	    switch (*++c)
	      {
	      case '%':
		func (stream, "%%");
		break;

	      case 'S':
		{
		  long reg;

		  reg = (given >> 3) & 0x7;
		  if (given & (1 << 6))
		    reg += 8;

		  func (stream, "%s", arm_regnames[reg]);
		}
		break;

	      case 'D':
		{
		  long reg;

		  reg = given & 0x7;
		  if (given & (1 << 7))
		    reg += 8;

		  func (stream, "%s", arm_regnames[reg]);
		}
		break;

	      case 'N':
		if (given & (1 << 8))
		  domasklr = 1;
		/* Fall through.  */
	      case 'O':
		if (*c == 'O' && (given & (1 << 8)))
		  domaskpc = 1;
		/* Fall through.  */
	      case 'M':
		{
		  int started = 0;
		  int reg;

		  func (stream, "{");

		  /* It would be nice if we could spot
		     ranges, and generate the rS-rE format: */
		  for (reg = 0; (reg < 8); reg++)
		    if ((given & (1 << reg)) != 0)
		      {
			if (started)
			  func (stream, ", ");
			started = 1;
			func (stream, "%s", arm_regnames[reg]);
		      }

		  if (domasklr)
		    {
		      if (started)
			func (stream, ", ");
		      started = 1;
		      func (stream, arm_regnames[14] /* "lr" */);
		    }

		  if (domaskpc)
		    {
		      if (started)
			func (stream, ", ");
		      func (stream, arm_regnames[15] /* "pc" */);
		    }

		  func (stream, "}");
		}
		break;

	      case 'b':
		/* Print ARM V6T2 CZB address: pc+4+6 bits.  */
		{
		  bfd_vma address = (pc + 4
				     + ((given & 0x00f8) >> 2)
				     + ((given & 0x0200) >> 3));
		  info->print_address_func (address, info);
		}
		break;

	      case 's':
		/* Right shift immediate -- bits 6..10; 1-31 print
		   as themselves, 0 prints as 32.  */
		{
		  long imm = (given & 0x07c0) >> 6;
		  if (imm == 0)
		    imm = 32;
		  func (stream, "#%ld", imm);
		}
		break;

	      case '0': case '1': case '2': case '3': case '4':
	      case '5': case '6': case '7': case '8': case '9':
		{
		  int bitstart = *c++ - '0';
		  int bitend = 0;

		  while (*c >= '0' && *c <= '9')
		    bitstart = (bitstart * 10) + *c++ - '0';

		  switch (*c)
		    {
		    case '-':
		      {
			long reg;

			c++;
			while (*c >= '0' && *c <= '9')
			  bitend = (bitend * 10) + *c++ - '0';
			if (!bitend)
			  abort ();
			reg = given >> bitstart;
			reg &= (2 << (bitend - bitstart)) - 1;
			switch (*c)
			  {
			  case 'r':
			    func (stream, "%s", arm_regnames[reg]);
			    break;

			  case 'd':
			    func (stream, "%ld", reg);
			    break;

			  case 'H':
			    func (stream, "%ld", reg << 1);
			    break;

			  case 'W':
			    func (stream, "%ld", reg << 2);
			    break;

			  case 'a':
			    /* PC-relative address -- the bottom two
			       bits of the address are dropped
			       before the calculation.  */
			    info->print_address_func
			      (((pc + 4) & ~3) + (reg << 2), info);
			    break;

			  case 'x':
			    func (stream, "0x%04lx", reg);
			    break;

			  case 'B':
			    reg = ((reg ^ (1 << bitend)) - (1 << bitend));
			    info->print_address_func (reg * 2 + pc + 4, info);
			    break;

			  case 'c':
			    {
			      /* Must print 0xE as 'al' to distinguish
				 unconditional B from conditional BAL.  */
			      if (reg == 0xE)
				func (stream, "al");
			      else
				func (stream, "%s", arm_conditional [reg]);
			    }
			    break;

			  default:
			    abort ();
			  }
		      }
		      break;

		    case '\'':
		      c++;
		      if ((given & (1 << bitstart)) != 0)
			func (stream, "%c", *c);
		      break;

		    case '?':
		      ++c;
		      if ((given & (1 << bitstart)) != 0)
			func (stream, "%c", *c++);
		      else
			func (stream, "%c", *++c);
		      break;

		    default:
		      abort ();
		    }
		}
		break;

	      default:
		abort ();
	      }
	  }
	return;
      }

  /* No match.  */
  abort ();
}

/* Return the name of an V7M special register.  */
static const char *
psr_name (int regno)
{
  switch (regno)
    {
    case 0: return "APSR";
    case 1: return "IAPSR";
    case 2: return "EAPSR";
    case 3: return "PSR";
    case 5: return "IPSR";
    case 6: return "EPSR";
    case 7: return "IEPSR";
    case 8: return "MSP";
    case 9: return "PSP";
    case 16: return "PRIMASK";
    case 17: return "BASEPRI";
    case 18: return "BASEPRI_MASK";
    case 19: return "FAULTMASK";
    case 20: return "CONTROL";
    default: return "<unknown>";
    }
}

/* Print one 32-bit Thumb instruction from PC on INFO->STREAM.  */

static void
print_insn_thumb32 (bfd_vma pc, struct disassemble_info *info, long given)
{
  const struct opcode32 *insn;
  void *stream = info->stream;
  fprintf_ftype func = info->fprintf_func;

  if (print_insn_coprocessor (info, given, TRUE))
    return;

  for (insn = thumb32_opcodes; insn->assembler; insn++)
    if ((given & insn->mask) == insn->value)
      {
	const char *c = insn->assembler;
	for (; *c; c++)
	  {
	    if (*c != '%')
	      {
		func (stream, "%c", *c);
		continue;
	      }

	    switch (*++c)
	      {
	      case '%':
		func (stream, "%%");
		break;

	      case 'I':
		{
		  unsigned int imm12 = 0;
		  imm12 |= (given & 0x000000ffu);
		  imm12 |= (given & 0x00007000u) >> 4;
		  imm12 |= (given & 0x04000000u) >> 15;
		  func (stream, "#%u\t; 0x%x", imm12, imm12);
		}
		break;

	      case 'M':
		{
		  unsigned int bits = 0, imm, imm8, mod;
		  bits |= (given & 0x000000ffu);
		  bits |= (given & 0x00007000u) >> 4;
		  bits |= (given & 0x04000000u) >> 15;
		  imm8 = (bits & 0x0ff);
		  mod = (bits & 0xf00) >> 8;
		  switch (mod)
		    {
		    case 0: imm = imm8; break;
		    case 1: imm = ((imm8<<16) | imm8); break;
		    case 2: imm = ((imm8<<24) | (imm8 << 8)); break;
		    case 3: imm = ((imm8<<24) | (imm8 << 16) | (imm8 << 8) | imm8); break;
		    default:
		      mod  = (bits & 0xf80) >> 7;
		      imm8 = (bits & 0x07f) | 0x80;
		      imm  = (((imm8 << (32 - mod)) | (imm8 >> mod)) & 0xffffffff);
		    }
		  func (stream, "#%u\t; 0x%x", imm, imm);
		}
		break;
		  
	      case 'J':
		{
		  unsigned int imm = 0;
		  imm |= (given & 0x000000ffu);
		  imm |= (given & 0x00007000u) >> 4;
		  imm |= (given & 0x04000000u) >> 15;
		  imm |= (given & 0x000f0000u) >> 4;
		  func (stream, "#%u\t; 0x%x", imm, imm);
		}
		break;

	      case 'K':
		{
		  unsigned int imm = 0;
		  imm |= (given & 0x000f0000u) >> 16;
		  imm |= (given & 0x00000ff0u) >> 0;
		  imm |= (given & 0x0000000fu) << 12;
		  func (stream, "#%u\t; 0x%x", imm, imm);
		}
		break;

	      case 'S':
		{
		  unsigned int reg = (given & 0x0000000fu);
		  unsigned int stp = (given & 0x00000030u) >> 4;
		  unsigned int imm = 0;
		  imm |= (given & 0x000000c0u) >> 6;
		  imm |= (given & 0x00007000u) >> 10;

		  func (stream, "%s", arm_regnames[reg]);
		  switch (stp)
		    {
		    case 0:
		      if (imm > 0)
			func (stream, ", lsl #%u", imm);
		      break;

		    case 1:
		      if (imm == 0)
			imm = 32;
		      func (stream, ", lsr #%u", imm);
		      break;

		    case 2:
		      if (imm == 0)
			imm = 32;
		      func (stream, ", asr #%u", imm);
		      break;

		    case 3:
		      if (imm == 0)
			func (stream, ", rrx");
		      else
			func (stream, ", ror #%u", imm);
		    }
		}
		break;

	      case 'a':
		{
		  unsigned int Rn  = (given & 0x000f0000) >> 16;
		  unsigned int U   = (given & 0x00800000) >> 23;
		  unsigned int op  = (given & 0x00000f00) >> 8;
		  unsigned int i12 = (given & 0x00000fff);
		  unsigned int i8  = (given & 0x000000ff);
		  bfd_boolean writeback = FALSE, postind = FALSE;
		  int offset = 0;

		  func (stream, "[%s", arm_regnames[Rn]);
		  if (U) /* 12-bit positive immediate offset */
		    offset = i12;
		  else if (Rn == 15) /* 12-bit negative immediate offset */
		    offset = -(int)i12;
		  else if (op == 0x0) /* shifted register offset */
		    {
		      unsigned int Rm = (i8 & 0x0f);
		      unsigned int sh = (i8 & 0x30) >> 4;
		      func (stream, ", %s", arm_regnames[Rm]);
		      if (sh)
			func (stream, ", lsl #%u", sh);
		      func (stream, "]");
		      break;
		    }
		  else switch (op)
		    {
		    case 0xE:  /* 8-bit positive immediate offset */
		      offset = i8;
		      break;

		    case 0xC:  /* 8-bit negative immediate offset */
		      offset = -i8;
		      break;

		    case 0xF:  /* 8-bit + preindex with wb */
		      offset = i8;
		      writeback = TRUE;
		      break;

		    case 0xD:  /* 8-bit - preindex with wb */
		      offset = -i8;
		      writeback = TRUE;
		      break;

		    case 0xB:  /* 8-bit + postindex */
		      offset = i8;
		      postind = TRUE;
		      break;

		    case 0x9:  /* 8-bit - postindex */
		      offset = -i8;
		      postind = TRUE;
		      break;

		    default:
		      func (stream, ", <undefined>]");
		      goto skip;
		    }

		  if (postind)
		    func (stream, "], #%d", offset);
		  else
		    {
		      if (offset)
			func (stream, ", #%d", offset);
		      func (stream, writeback ? "]!" : "]");
		    }

		  if (Rn == 15)
		    {
		      func (stream, "\t; ");
		      info->print_address_func (((pc + 4) & ~3) + offset, info);
		    }
		}
	      skip:
		break;

	      case 'A':
		{
		  unsigned int P   = (given & 0x01000000) >> 24;
		  unsigned int U   = (given & 0x00800000) >> 23;
		  unsigned int W   = (given & 0x00400000) >> 21;
		  unsigned int Rn  = (given & 0x000f0000) >> 16;
		  unsigned int off = (given & 0x000000ff);

		  func (stream, "[%s", arm_regnames[Rn]);
		  if (P)
		    {
		      if (off || !U)
			func (stream, ", #%c%u", U ? '+' : '-', off * 4);
		      func (stream, "]");
		      if (W)
			func (stream, "!");
		    }
		  else
		    {
		      func (stream, "], ");
		      if (W)
			func (stream, "#%c%u", U ? '+' : '-', off * 4);
		      else
			func (stream, "{%u}", off);
		    }
		}
		break;

	      case 'w':
		{
		  unsigned int Sbit = (given & 0x01000000) >> 24;
		  unsigned int type = (given & 0x00600000) >> 21;
		  switch (type)
		    {
		    case 0: func (stream, Sbit ? "sb" : "b"); break;
		    case 1: func (stream, Sbit ? "sh" : "h"); break;
		    case 2:
		      if (Sbit)
			func (stream, "??");
		      break;
		    case 3:
		      func (stream, "??");
		      break;
		    }
		}
		break;

	      case 'm':
		{
		  int started = 0;
		  int reg;

		  func (stream, "{");
		  for (reg = 0; reg < 16; reg++)
		    if ((given & (1 << reg)) != 0)
		      {
			if (started)
			  func (stream, ", ");
			started = 1;
			func (stream, "%s", arm_regnames[reg]);
		      }
		  func (stream, "}");
		}
		break;

	      case 'E':
		{
		  unsigned int msb = (given & 0x0000001f);
		  unsigned int lsb = 0;
		  lsb |= (given & 0x000000c0u) >> 6;
		  lsb |= (given & 0x00007000u) >> 10;
		  func (stream, "#%u, #%u", lsb, msb - lsb + 1);
		}
		break;

	      case 'F':
		{
		  unsigned int width = (given & 0x0000001f) + 1;
		  unsigned int lsb = 0;
		  lsb |= (given & 0x000000c0u) >> 6;
		  lsb |= (given & 0x00007000u) >> 10;
		  func (stream, "#%u, #%u", lsb, width);
		}
		break;

	      case 'b':
		{
		  unsigned int S = (given & 0x04000000u) >> 26;
		  unsigned int J1 = (given & 0x00002000u) >> 13;
		  unsigned int J2 = (given & 0x00000800u) >> 11;
		  int offset = 0;

		  offset |= !S << 20;
		  offset |= J2 << 19;
		  offset |= J1 << 18;
		  offset |= (given & 0x003f0000) >> 4;
		  offset |= (given & 0x000007ff) << 1;
		  offset -= (1 << 20);

		  info->print_address_func (pc + 4 + offset, info);
		}
		break;

	      case 'B':
		{
		  unsigned int S = (given & 0x04000000u) >> 26;
		  unsigned int I1 = (given & 0x00002000u) >> 13;
		  unsigned int I2 = (given & 0x00000800u) >> 11;
		  int offset = 0;

		  offset |= !S << 24;
		  offset |= !(I1 ^ S) << 23;
		  offset |= !(I2 ^ S) << 22;
		  offset |= (given & 0x03ff0000u) >> 4;
		  offset |= (given & 0x000007ffu) << 1;
		  offset -= (1 << 24);
		  offset += pc + 4;

		  /* BLX target addresses are always word aligned.  */
		  if ((given & 0x00001000u) == 0)
		      offset &= ~2u;

		  info->print_address_func (offset, info);
		}
		break;

	      case 's':
		{
		  unsigned int shift = 0;
		  shift |= (given & 0x000000c0u) >> 6;
		  shift |= (given & 0x00007000u) >> 10;
		  if (given & 0x00200000u)
		    func (stream, ", asr #%u", shift);
		  else if (shift)
		    func (stream, ", lsl #%u", shift);
		  /* else print nothing - lsl #0 */
		}
		break;

	      case 'R':
		{
		  unsigned int rot = (given & 0x00000030) >> 4;
		  if (rot)
		    func (stream, ", ror #%u", rot * 8);
		}
		break;

	      case 'U':
		switch (given & 0xf)
		  {
		  case 0xf: func(stream, "sy"); break;
		  case 0x7: func(stream, "un"); break;
		  case 0xe: func(stream, "st"); break;
		  case 0x6: func(stream, "unst"); break;
		  default:
		    func(stream, "#%d", (int)given & 0xf);
		    break;
		  }
		break;

	      case 'C':
		if ((given & 0xff) == 0)
		  {
		    func (stream, "%cPSR_", (given & 0x100000) ? 'S' : 'C');
		    if (given & 0x800)
		      func (stream, "f");
		    if (given & 0x400)
		      func (stream, "s");
		    if (given & 0x200)
		      func (stream, "x");
		    if (given & 0x100)
		      func (stream, "c");
		  }
		else
		  {
		    func (stream, psr_name (given & 0xff));
		  }
		break;

	      case 'D':
		if ((given & 0xff) == 0)
		  func (stream, "%cPSR", (given & 0x100000) ? 'S' : 'C');
		else
		  func (stream, psr_name (given & 0xff));
		break;

	      case '0': case '1': case '2': case '3': case '4':
	      case '5': case '6': case '7': case '8': case '9':
		{
		  int bitstart = *c++ - '0';
		  int bitend = 0;
		  unsigned int val;
		  while (*c >= '0' && *c <= '9')
		    bitstart = (bitstart * 10) + *c++ - '0';

		  if (*c == '-')
		    {
		      c++;
		      while (*c >= '0' && *c <= '9')
			bitend = (bitend * 10) + *c++ - '0';
		      if (!bitend)
			abort ();

		      val = given >> bitstart;
		      val &= (2 << (bitend - bitstart)) - 1;
		    }
		  else
		    val = (given >> bitstart) & 1;

		  switch (*c)
		    {
		    case 'd': func (stream, "%u", val); break;
		    case 'W': func (stream, "%u", val * 4); break;
		    case 'r': func (stream, "%s", arm_regnames[val]); break;

		    case 'c':
		      if (val == 0xE)
			func (stream, "al");
		      else
			func (stream, "%s", arm_conditional[val]);
		      break;

		    case '\'':
		      if (val)
			func (stream, "%c", c[1]);
		      c++;
		      break;
		      
		    case '`':
		      if (!val)
			func (stream, "%c", c[1]);
		      c++;
		      break;

		    case '?':
		      func (stream, "%c", val ? c[1] : c[2]);
		      c += 2;
		      break;

		    default:
		      abort ();
		    }
		}
		break;

	      default:
		abort ();
	      }
	  }
	return;
      }

  /* No match.  */
  abort ();
}

/* Disallow mapping symbols ($a, $b, $d, $t etc) from
   being displayed in symbol relative addresses.  */

bfd_boolean
arm_symbol_is_valid (asymbol * sym,
		     struct disassemble_info * info ATTRIBUTE_UNUSED)
{
  const char * name;
  
  if (sym == NULL)
    return FALSE;

  name = bfd_asymbol_name (sym);

  return (name && *name != '$');
}

/* Parse an individual disassembler option.  */

void
parse_arm_disassembler_option (char *option)
{
  if (option == NULL)
    return;

  if (strneq (option, "reg-names-", 10))
    {
      int i;

      option += 10;

      for (i = NUM_ARM_REGNAMES; i--;)
	if (strneq (option, regnames[i].name, strlen (regnames[i].name)))
	  {
	    regname_selected = i;
	    break;
	  }

      if (i < 0)
	/* XXX - should break 'option' at following delimiter.  */
	fprintf (stderr, _("Unrecognised register name set: %s\n"), option);
    }
  else if (strneq (option, "force-thumb", 11))
    force_thumb = 1;
  else if (strneq (option, "no-force-thumb", 14))
    force_thumb = 0;
  else
    /* XXX - should break 'option' at following delimiter.  */
    fprintf (stderr, _("Unrecognised disassembler option: %s\n"), option);

  return;
}

/* Parse the string of disassembler options, spliting it at whitespaces
   or commas.  (Whitespace separators supported for backwards compatibility).  */

static void
parse_disassembler_options (char *options)
{
  if (options == NULL)
    return;

  while (*options)
    {
      parse_arm_disassembler_option (options);

      /* Skip forward to next seperator.  */
      while ((*options) && (! ISSPACE (*options)) && (*options != ','))
	++ options;
      /* Skip forward past seperators.  */
      while (ISSPACE (*options) || (*options == ','))
	++ options;      
    }
}

/* NOTE: There are no checks in these routines that
   the relevant number of data bytes exist.  */

static int
print_insn (bfd_vma pc, struct disassemble_info *info, bfd_boolean little)
{
  unsigned char b[4];
  long		given;
  int           status;
  int           is_thumb;
  int		size;
  void	 	(*printer) (bfd_vma, struct disassemble_info *, long);

  if (info->disassembler_options)
    {
      parse_disassembler_options (info->disassembler_options);

      /* To avoid repeated parsing of these options, we remove them here.  */
      info->disassembler_options = NULL;
    }

  is_thumb = force_thumb;

  if (!is_thumb && info->symbols != NULL)
    {
      if (bfd_asymbol_flavour (*info->symbols) == bfd_target_coff_flavour)
	{
	  coff_symbol_type * cs;

	  cs = coffsymbol (*info->symbols);
	  is_thumb = (   cs->native->u.syment.n_sclass == C_THUMBEXT
		      || cs->native->u.syment.n_sclass == C_THUMBSTAT
		      || cs->native->u.syment.n_sclass == C_THUMBLABEL
		      || cs->native->u.syment.n_sclass == C_THUMBEXTFUNC
		      || cs->native->u.syment.n_sclass == C_THUMBSTATFUNC);
	}
      else if (bfd_asymbol_flavour (*info->symbols) == bfd_target_elf_flavour)
	{
	  elf_symbol_type *  es;
	  unsigned int       type;

	  es = *(elf_symbol_type **)(info->symbols);
	  type = ELF_ST_TYPE (es->internal_elf_sym.st_info);

	  is_thumb = (type == STT_ARM_TFUNC) || (type == STT_ARM_16BIT);
	}
    }

  info->display_endian  = little ? BFD_ENDIAN_LITTLE : BFD_ENDIAN_BIG;
  info->bytes_per_line = 4;

  if (!is_thumb)
    {
      /* In ARM mode endianness is a straightforward issue: the instruction
	 is four bytes long and is either ordered 0123 or 3210.  */
      printer = print_insn_arm;
      info->bytes_per_chunk = 4;
      size = 4;

      status = info->read_memory_func (pc, (bfd_byte *)b, 4, info);
      if (little)
	given = (b[0]) | (b[1] << 8) | (b[2] << 16) | (b[3] << 24);
      else
	given = (b[3]) | (b[2] << 8) | (b[1] << 16) | (b[0] << 24);
    }
  else
    {
      /* In Thumb mode we have the additional wrinkle of two
	 instruction lengths.  Fortunately, the bits that determine
	 the length of the current instruction are always to be found
	 in the first two bytes.  */
      printer = print_insn_thumb16;
      info->bytes_per_chunk = 2;
      size = 2;

      status = info->read_memory_func (pc, (bfd_byte *)b, 2, info);
      if (little)
	given = (b[0]) | (b[1] << 8);
      else
	given = (b[1]) | (b[0] << 8);

      if (!status)
	{
	  /* These bit patterns signal a four-byte Thumb
	     instruction.  */
	  if ((given & 0xF800) == 0xF800
	      || (given & 0xF800) == 0xF000
	      || (given & 0xF800) == 0xE800)
	    {
	      status = info->read_memory_func (pc + 2, (bfd_byte *)b, 2, info);
	      if (little)
		given = (b[0]) | (b[1] << 8) | (given << 16);
	      else
		given = (b[1]) | (b[0] << 8) | (given << 16);

	      printer = print_insn_thumb32;
	      size = 4;
	    }
	}
    }

  if (status)
    {
      info->memory_error_func (status, pc, info);
      return -1;
    }
  if (info->flags & INSN_HAS_RELOC)
    /* If the instruction has a reloc associated with it, then
       the offset field in the instruction will actually be the
       addend for the reloc.  (We are using REL type relocs).
       In such cases, we can ignore the pc when computing
       addresses, since the addend is not currently pc-relative.  */
    pc = 0;

  printer (pc, info, given);
  return size;
}

int
print_insn_big_arm (bfd_vma pc, struct disassemble_info *info)
{
  return print_insn (pc, info, FALSE);
}

int
print_insn_little_arm (bfd_vma pc, struct disassemble_info *info)
{
  return print_insn (pc, info, TRUE);
}

void
print_arm_disassembler_options (FILE *stream)
{
  int i;

  fprintf (stream, _("\n\
The following ARM specific disassembler options are supported for use with\n\
the -M switch:\n"));

  for (i = NUM_ARM_REGNAMES; i--;)
    fprintf (stream, "  reg-names-%s %*c%s\n",
	     regnames[i].name,
	     (int)(14 - strlen (regnames[i].name)), ' ',
	     regnames[i].description);

  fprintf (stream, "  force-thumb              Assume all insns are Thumb insns\n");
  fprintf (stream, "  no-force-thumb           Examine preceeding label to determine an insn's type\n\n");
}
