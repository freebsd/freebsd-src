/* Opcode table for the ARM.

   Copyright 1994, 1995, 1996, 1997, 2000 Free Software Foundation, Inc.
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */


struct arm_opcode {
    unsigned long value, mask;	/* recognise instruction if (op&mask)==value */
    char *assembler;		/* how to disassemble this instruction */
};

struct thumb_opcode
{
    unsigned short value, mask;	/* recognise instruction if (op&mask)==value */
    char * assembler;		/* how to disassemble this instruction */
};

/* format of the assembler string :
   
   %%			%
   %<bitfield>d		print the bitfield in decimal
   %<bitfield>x		print the bitfield in hex
   %<bitfield>X		print the bitfield as 1 hex digit without leading "0x"
   %<bitfield>r		print as an ARM register
   %<bitfield>f		print a floating point constant if >7 else a
			floating point register
   %c			print condition code (always bits 28-31)
   %P			print floating point precision in arithmetic insn
   %Q			print floating point precision in ldf/stf insn
   %R			print floating point rounding mode
   %<bitnum>'c		print specified char iff bit is one
   %<bitnum>`c		print specified char iff bit is zero
   %<bitnum>?ab		print a if bit is one else print b
   %p			print 'p' iff bits 12-15 are 15
   %t			print 't' iff bit 21 set and bit 24 clear
   %h                   print 'h' iff bit 5 set, else print 'b'
   %o			print operand2 (immediate or register + shift)
   %a			print address for ldr/str instruction
   %s                   print address for ldr/str halfword/signextend instruction
   %b			print branch destination
   %B			print arm BLX(1) destination
   %A			print address for ldc/stc/ldf/stf instruction
   %m			print register mask for ldm/stm instruction
   %C			print the PSR sub type.
   %F			print the COUNT field of a LFM/SFM instruction.
Thumb specific format options:
   %D                   print Thumb register (bits 0..2 as high number if bit 7 set)
   %S                   print Thumb register (bits 3..5 as high number if bit 6 set)
   %<bitfield>I         print bitfield as a signed decimal
   				(top bit of range being the sign bit)
   %M                   print Thumb register mask
   %N                   print Thumb register mask (with LR)
   %O                   print Thumb register mask (with PC)
   %T                   print Thumb condition code (always bits 8-11)
   %<bitfield>B         print Thumb branch destination (signed displacement)
   %<bitfield>W         print (bitfield * 4) as a decimal
   %<bitfield>H         print (bitfield * 2) as a decimal
   %<bitfield>a         print (bitfield * 4) as a pc-rel offset + decoded symbol
*/

/* Note: There is a partial ordering in this table - it must be searched from
   the top to obtain a correct match. */

static struct arm_opcode arm_opcodes[] =
{
    /* ARM instructions.  */
    {0xe1a00000, 0xffffffff, "nop\t\t\t(mov r0,r0)"},
    {0x012FFF10, 0x0ffffff0, "bx%c\t%0-3r"},
    {0x00000090, 0x0fe000f0, "mul%c%20's\t%16-19r, %0-3r, %8-11r"},
    {0x00200090, 0x0fe000f0, "mla%c%20's\t%16-19r, %0-3r, %8-11r, %12-15r"},
    {0x01000090, 0x0fb00ff0, "swp%c%22'b\t%12-15r, %0-3r, [%16-19r]"},
    {0x00800090, 0x0fa000f0, "%22?sumull%c%20's\t%12-15r, %16-19r, %0-3r, %8-11r"},
    {0x00a00090, 0x0fa000f0, "%22?sumlal%c%20's\t%12-15r, %16-19r, %0-3r, %8-11r"},

    /* XScale instructions.  */
    {0x0e200010, 0x0fff0ff0, "mia%c\tacc0, %0-3r, %12-15r"},
    {0x0e280010, 0x0fff0ff0, "miaph%c\tacc0, %0-3r, %12-15r"},
    {0x0e2c0010, 0x0ffc0ff0, "mia%17'T%17`B%16'T%16`B%c\tacc0, %0-3r, %12-15r"},
    {0x0c400000, 0x0ff00fff, "mar%c\tacc0, %12-15r, %16-19r"},
    {0x0c500000, 0x0ff00fff, "mra%c\t%12-15r, %16-19r, acc0"},
    {0xf450f000, 0xfc70f000, "pld\t%a"},
    
    /* V5 Instructions.  */
    {0xe1200070, 0xfff000f0, "bkpt\t0x%16-19X%12-15X%8-11X%0-3X"},
    {0xfa000000, 0xfe000000, "blx\t%B"},
    {0x012fff30, 0x0ffffff0, "blx%c\t%0-3r"},
    {0x016f0f10, 0x0fff0ff0, "clz%c\t%12-15r, %0-3r"},
    {0xfc100000, 0xfe100000, "ldc2%22'l\t%8-11d, cr%12-15d, %A"},
    {0xfc000000, 0xfe100000, "stc2%22'l\t%8-11d, cr%12-15d, %A"},
    {0xfe000000, 0xff000010, "cdp2\t%8-11d, %20-23d, cr%12-15d, cr%16-19d, cr%0-3d, {%5-7d}"},
    {0xfe000010, 0xff100010, "mcr2\t%8-11d, %21-23d, %12-15r, cr%16-19d, cr%0-3d, {%5-7d}"},
    {0xfe100010, 0xff100010, "mrc2\t%8-11d, %21-23d, %12-15r, cr%16-19d, cr%0-3d, {%5-7d}"},

    /* V5E "El Segundo" Instructions.  */    
    {0x000000d0, 0x0e1000f0, "ldr%cd\t%12-15r, %s"},
    {0x000000f0, 0x0e1000f0, "str%cd\t%12-15r, %s"},
    {0x01000080, 0x0ff000f0, "smlabb%c\t%16-19r, %0-3r, %8-11r, %12-15r"},
    {0x010000a0, 0x0ff000f0, "smlatb%c\t%16-19r, %0-3r, %8-11r, %12-15r"},
    {0x010000c0, 0x0ff000f0, "smlabt%c\t%16-19r, %0-3r, %8-11r, %12-15r"},
    {0x010000e0, 0x0ff000f0, "smlatt%c\t%16-19r, %0-3r, %8-11r, %12-15r"},

    {0x01200080, 0x0ff000f0, "smlawb%c\t%16-19r, %0-3r, %8-11r, %12-15r"},
    {0x012000c0, 0x0ff000f0, "smlawt%c\t%16-19r, %0-3r, %8-11r, %12-15r"},

    {0x01400080, 0x0ff000f0, "smlalbb%c\t%12-15r, %16-19r, %0-3r, %8-11r"},
    {0x014000a0, 0x0ff000f0, "smlaltb%c\t%12-15r, %16-19r, %0-3r, %8-11r"},
    {0x014000c0, 0x0ff000f0, "smlalbt%c\t%12-15r, %16-19r, %0-3r, %8-11r"},
    {0x014000e0, 0x0ff000f0, "smlaltt%c\t%12-15r, %16-19r, %0-3r, %8-11r"},

    {0x01600080, 0x0ff0f0f0, "smulbb%c\t%16-19r, %0-3r, %8-11r"},
    {0x016000a0, 0x0ff0f0f0, "smultb%c\t%16-19r, %0-3r, %8-11r"},
    {0x016000c0, 0x0ff0f0f0, "smulbt%c\t%16-19r, %0-3r, %8-11r"},
    {0x016000e0, 0x0ff0f0f0, "smultt%c\t%16-19r, %0-3r, %8-11r"},

    {0x012000a0, 0x0ff0f0f0, "smulwb%c\t%16-19r, %0-3r, %8-11r"},
    {0x012000e0, 0x0ff0f0f0, "smulwt%c\t%16-19r, %0-3r, %8-11r"},

    {0x01000050, 0x0ff00ff0,  "qadd%c\t%12-15r, %0-3r, %16-19r"},
    {0x01400050, 0x0ff00ff0, "qdadd%c\t%12-15r, %0-3r, %16-19r"},
    {0x01200050, 0x0ff00ff0,  "qsub%c\t%12-15r, %0-3r, %16-19r"},
    {0x01600050, 0x0ff00ff0, "qdsub%c\t%12-15r, %0-3r, %16-19r"},

    {0x0c400000, 0x0ff00000, "mcrr%c\t%8-11d, %4-7d, %12-15r, %16-19r, cr%0-3d"},
    {0x0c500000, 0x0ff00000, "mrrc%c\t%8-11d, %4-7d, %12-15r, %16-19r, cr%0-3d"},

    /* ARM Instructions.  */
    {0x00000090, 0x0e100090, "str%c%6's%h\t%12-15r, %s"},
    {0x00100090, 0x0e100090, "ldr%c%6's%h\t%12-15r, %s"},
    {0x00000000, 0x0de00000, "and%c%20's\t%12-15r, %16-19r, %o"},
    {0x00200000, 0x0de00000, "eor%c%20's\t%12-15r, %16-19r, %o"},
    {0x00400000, 0x0de00000, "sub%c%20's\t%12-15r, %16-19r, %o"},
    {0x00600000, 0x0de00000, "rsb%c%20's\t%12-15r, %16-19r, %o"},
    {0x00800000, 0x0de00000, "add%c%20's\t%12-15r, %16-19r, %o"},
    {0x00a00000, 0x0de00000, "adc%c%20's\t%12-15r, %16-19r, %o"},
    {0x00c00000, 0x0de00000, "sbc%c%20's\t%12-15r, %16-19r, %o"},
    {0x00e00000, 0x0de00000, "rsc%c%20's\t%12-15r, %16-19r, %o"},
    {0x0120f000, 0x0db0f000, "msr%c\t%22?SCPSR%C, %o"},
    {0x010f0000, 0x0fbf0fff, "mrs%c\t%12-15r, %22?SCPSR"},
    {0x01000000, 0x0de00000, "tst%c%p\t%16-19r, %o"},
    {0x01200000, 0x0de00000, "teq%c%p\t%16-19r, %o"},
    {0x01400000, 0x0de00000, "cmp%c%p\t%16-19r, %o"},
    {0x01600000, 0x0de00000, "cmn%c%p\t%16-19r, %o"},
    {0x01800000, 0x0de00000, "orr%c%20's\t%12-15r, %16-19r, %o"},
    {0x01a00000, 0x0de00000, "mov%c%20's\t%12-15r, %o"},
    {0x01c00000, 0x0de00000, "bic%c%20's\t%12-15r, %16-19r, %o"},
    {0x01e00000, 0x0de00000, "mvn%c%20's\t%12-15r, %o"},
    {0x04000000, 0x0e100000, "str%c%22'b%t\t%12-15r, %a"},
    {0x06000000, 0x0e100ff0, "str%c%22'b%t\t%12-15r, %a"},
    {0x04000000, 0x0c100010, "str%c%22'b%t\t%12-15r, %a"},
    {0x06000010, 0x0e000010, "undefined"},
    {0x04100000, 0x0c100000, "ldr%c%22'b%t\t%12-15r, %a"},
    {0x08000000, 0x0e100000, "stm%c%23?id%24?ba\t%16-19r%21'!, %m%22'^"},
    {0x08100000, 0x0e100000, "ldm%c%23?id%24?ba\t%16-19r%21'!, %m%22'^"},
    {0x0a000000, 0x0e000000, "b%24'l%c\t%b"},
    {0x0f000000, 0x0f000000, "swi%c\t%0-23x"},

    /* Floating point coprocessor instructions */
    {0x0e000100, 0x0ff08f10, "adf%c%P%R\t%12-14f, %16-18f, %0-3f"},
    {0x0e100100, 0x0ff08f10, "muf%c%P%R\t%12-14f, %16-18f, %0-3f"},
    {0x0e200100, 0x0ff08f10, "suf%c%P%R\t%12-14f, %16-18f, %0-3f"},
    {0x0e300100, 0x0ff08f10, "rsf%c%P%R\t%12-14f, %16-18f, %0-3f"},
    {0x0e400100, 0x0ff08f10, "dvf%c%P%R\t%12-14f, %16-18f, %0-3f"},
    {0x0e500100, 0x0ff08f10, "rdf%c%P%R\t%12-14f, %16-18f, %0-3f"},
    {0x0e600100, 0x0ff08f10, "pow%c%P%R\t%12-14f, %16-18f, %0-3f"},
    {0x0e700100, 0x0ff08f10, "rpw%c%P%R\t%12-14f, %16-18f, %0-3f"},
    {0x0e800100, 0x0ff08f10, "rmf%c%P%R\t%12-14f, %16-18f, %0-3f"},
    {0x0e900100, 0x0ff08f10, "fml%c%P%R\t%12-14f, %16-18f, %0-3f"},
    {0x0ea00100, 0x0ff08f10, "fdv%c%P%R\t%12-14f, %16-18f, %0-3f"},
    {0x0eb00100, 0x0ff08f10, "frd%c%P%R\t%12-14f, %16-18f, %0-3f"},
    {0x0ec00100, 0x0ff08f10, "pol%c%P%R\t%12-14f, %16-18f, %0-3f"},
    {0x0e008100, 0x0ff08f10, "mvf%c%P%R\t%12-14f, %0-3f"},
    {0x0e108100, 0x0ff08f10, "mnf%c%P%R\t%12-14f, %0-3f"},
    {0x0e208100, 0x0ff08f10, "abs%c%P%R\t%12-14f, %0-3f"},
    {0x0e308100, 0x0ff08f10, "rnd%c%P%R\t%12-14f, %0-3f"},
    {0x0e408100, 0x0ff08f10, "sqt%c%P%R\t%12-14f, %0-3f"},
    {0x0e508100, 0x0ff08f10, "log%c%P%R\t%12-14f, %0-3f"},
    {0x0e608100, 0x0ff08f10, "lgn%c%P%R\t%12-14f, %0-3f"},
    {0x0e708100, 0x0ff08f10, "exp%c%P%R\t%12-14f, %0-3f"},
    {0x0e808100, 0x0ff08f10, "sin%c%P%R\t%12-14f, %0-3f"},
    {0x0e908100, 0x0ff08f10, "cos%c%P%R\t%12-14f, %0-3f"},
    {0x0ea08100, 0x0ff08f10, "tan%c%P%R\t%12-14f, %0-3f"},
    {0x0eb08100, 0x0ff08f10, "asn%c%P%R\t%12-14f, %0-3f"},
    {0x0ec08100, 0x0ff08f10, "acs%c%P%R\t%12-14f, %0-3f"},
    {0x0ed08100, 0x0ff08f10, "atn%c%P%R\t%12-14f, %0-3f"},
    {0x0ee08100, 0x0ff08f10, "urd%c%P%R\t%12-14f, %0-3f"},
    {0x0ef08100, 0x0ff08f10, "nrm%c%P%R\t%12-14f, %0-3f"},
    {0x0e000110, 0x0ff00f1f, "flt%c%P%R\t%16-18f, %12-15r"},
    {0x0e100110, 0x0fff0f98, "fix%c%R\t%12-15r, %0-2f"},
    {0x0e200110, 0x0fff0fff, "wfs%c\t%12-15r"},
    {0x0e300110, 0x0fff0fff, "rfs%c\t%12-15r"},
    {0x0e400110, 0x0fff0fff, "wfc%c\t%12-15r"},
    {0x0e500110, 0x0fff0fff, "rfc%c\t%12-15r"},
    {0x0e90f110, 0x0ff8fff0, "cmf%c\t%16-18f, %0-3f"},
    {0x0eb0f110, 0x0ff8fff0, "cnf%c\t%16-18f, %0-3f"},
    {0x0ed0f110, 0x0ff8fff0, "cmfe%c\t%16-18f, %0-3f"},
    {0x0ef0f110, 0x0ff8fff0, "cnfe%c\t%16-18f, %0-3f"},
    {0x0c000100, 0x0e100f00, "stf%c%Q\t%12-14f, %A"},
    {0x0c100100, 0x0e100f00, "ldf%c%Q\t%12-14f, %A"},
    {0x0c000200, 0x0e100f00, "sfm%c\t%12-14f, %F, %A"},
    {0x0c100200, 0x0e100f00, "lfm%c\t%12-14f, %F, %A"},

    /* Generic coprocessor instructions */
    {0x0e000000, 0x0f000010, "cdp%c\t%8-11d, %20-23d, cr%12-15d, cr%16-19d, cr%0-3d, {%5-7d}"},
    {0x0e100010, 0x0f100010, "mrc%c\t%8-11d, %21-23d, %12-15r, cr%16-19d, cr%0-3d, {%5-7d}"},
    {0x0e000010, 0x0f100010, "mcr%c\t%8-11d, %21-23d, %12-15r, cr%16-19d, cr%0-3d, {%5-7d}"},
    {0x0c000000, 0x0e100000, "stc%c%22'l\t%8-11d, cr%12-15d, %A"},
    {0x0c100000, 0x0e100000, "ldc%c%22'l\t%8-11d, cr%12-15d, %A"},

    /* The rest.  */
    {0x00000000, 0x00000000, "undefined instruction %0-31x"},
    {0x00000000, 0x00000000, 0}
};

#define BDISP(x) ((((x) & 0xffffff) ^ 0x800000) - 0x800000) /* 26 bit */

static struct thumb_opcode thumb_opcodes[] =
{
  /* Thumb instructions.  */

  /* ARM V5 ISA extends Thumb.  */
  {0xbe00, 0xff00, "bkpt\t%0-7x"},
  {0x4780, 0xff87, "blx\t%3-6r"},	/* note: 4 bit register number.  */
  /* Note: this is BLX(2).  BLX(1) is done in arm-dis.c/print_insn_thumb()
     as an extension of the special processing there for Thumb BL.
     BL and BLX(1) involve 2 successive 16-bit instructions, which must
     always appear together in the correct order.  So, the empty
     string is put in this table, and the string interpreter takes <empty>
     to mean it has a pair of BL-ish instructions.  */
  {0x46C0, 0xFFFF, "nop\t\t\t(mov r8, r8)"},
  /* Format 5 instructions do not update the PSR.  */
  {0x1C00, 0xFFC0, "mov\t%0-2r, %3-5r\t\t(add %0-2r, %3-5r, #%6-8d)"},
  /* Format 4.  */
  {0x4000, 0xFFC0, "and\t%0-2r, %3-5r"},
  {0x4040, 0xFFC0, "eor\t%0-2r, %3-5r"},
  {0x4080, 0xFFC0, "lsl\t%0-2r, %3-5r"},
  {0x40C0, 0xFFC0, "lsr\t%0-2r, %3-5r"},
  {0x4100, 0xFFC0, "asr\t%0-2r, %3-5r"},
  {0x4140, 0xFFC0, "adc\t%0-2r, %3-5r"},
  {0x4180, 0xFFC0, "sbc\t%0-2r, %3-5r"},
  {0x41C0, 0xFFC0, "ror\t%0-2r, %3-5r"},
  {0x4200, 0xFFC0, "tst\t%0-2r, %3-5r"},
  {0x4240, 0xFFC0, "neg\t%0-2r, %3-5r"},
  {0x4280, 0xFFC0, "cmp\t%0-2r, %3-5r"},
  {0x42C0, 0xFFC0, "cmn\t%0-2r, %3-5r"},
  {0x4300, 0xFFC0, "orr\t%0-2r, %3-5r"},
  {0x4340, 0xFFC0, "mul\t%0-2r, %3-5r"},
  {0x4380, 0xFFC0, "bic\t%0-2r, %3-5r"},
  {0x43C0, 0xFFC0, "mvn\t%0-2r, %3-5r"},
  /* format 13 */
  {0xB000, 0xFF80, "add\tsp, #%0-6W"},
  {0xB080, 0xFF80, "sub\tsp, #%0-6W"},
  /* format 5 */
  {0x4700, 0xFF80, "bx\t%S"},
  {0x4400, 0xFF00, "add\t%D, %S"},
  {0x4500, 0xFF00, "cmp\t%D, %S"},
  {0x4600, 0xFF00, "mov\t%D, %S"},
  /* format 14 */
  {0xB400, 0xFE00, "push\t%N"},
  {0xBC00, 0xFE00, "pop\t%O"},
  /* format 2 */
  {0x1800, 0xFE00, "add\t%0-2r, %3-5r, %6-8r"},
  {0x1A00, 0xFE00, "sub\t%0-2r, %3-5r, %6-8r"},
  {0x1C00, 0xFE00, "add\t%0-2r, %3-5r, #%6-8d"},
  {0x1E00, 0xFE00, "sub\t%0-2r, %3-5r, #%6-8d"},
  /* format 8 */
  {0x5200, 0xFE00, "strh\t%0-2r, [%3-5r, %6-8r]"},
  {0x5A00, 0xFE00, "ldrh\t%0-2r, [%3-5r, %6-8r]"},
  {0x5600, 0xF600, "ldrs%11?hb\t%0-2r, [%3-5r, %6-8r]"},
  /* format 7 */
  {0x5000, 0xFA00, "str%10'b\t%0-2r, [%3-5r, %6-8r]"},
  {0x5800, 0xFA00, "ldr%10'b\t%0-2r, [%3-5r, %6-8r]"},
  /* format 1 */
  {0x0000, 0xF800, "lsl\t%0-2r, %3-5r, #%6-10d"},
  {0x0800, 0xF800, "lsr\t%0-2r, %3-5r, #%6-10d"},
  {0x1000, 0xF800, "asr\t%0-2r, %3-5r, #%6-10d"},
  /* format 3 */
  {0x2000, 0xF800, "mov\t%8-10r, #%0-7d"},
  {0x2800, 0xF800, "cmp\t%8-10r, #%0-7d"},
  {0x3000, 0xF800, "add\t%8-10r, #%0-7d"},
  {0x3800, 0xF800, "sub\t%8-10r, #%0-7d"},
  /* format 6 */
  {0x4800, 0xF800, "ldr\t%8-10r, [pc, #%0-7W]\t(%0-7a)"},  /* TODO: Disassemble PC relative "LDR rD,=<symbolic>" */
  /* format 9 */
  {0x6000, 0xF800, "str\t%0-2r, [%3-5r, #%6-10W]"},
  {0x6800, 0xF800, "ldr\t%0-2r, [%3-5r, #%6-10W]"},
  {0x7000, 0xF800, "strb\t%0-2r, [%3-5r, #%6-10d]"},
  {0x7800, 0xF800, "ldrb\t%0-2r, [%3-5r, #%6-10d]"},
  /* format 10 */
  {0x8000, 0xF800, "strh\t%0-2r, [%3-5r, #%6-10H]"},
  {0x8800, 0xF800, "ldrh\t%0-2r, [%3-5r, #%6-10H]"},
  /* format 11 */
  {0x9000, 0xF800, "str\t%8-10r, [sp, #%0-7W]"},
  {0x9800, 0xF800, "ldr\t%8-10r, [sp, #%0-7W]"},
  /* format 12 */
  {0xA000, 0xF800, "add\t%8-10r, pc, #%0-7W\t(adr %8-10r,%0-7a)"},
  {0xA800, 0xF800, "add\t%8-10r, sp, #%0-7W"},
  /* format 15 */
  {0xC000, 0xF800, "stmia\t%8-10r!,%M"},
  {0xC800, 0xF800, "ldmia\t%8-10r!,%M"},
  /* format 18 */
  {0xE000, 0xF800, "b\t%0-10B"},
  {0xE800, 0xF800, "undefined"},
  /* format 19 */
  {0xF000, 0xF800, ""}, /* special processing required in disassembler */
  {0xF800, 0xF800, "second half of BL instruction %0-15x"},
  /* format 16 */
  {0xD000, 0xFF00, "beq\t%0-7B"},
  {0xD100, 0xFF00, "bne\t%0-7B"},
  {0xD200, 0xFF00, "bcs\t%0-7B"},
  {0xD300, 0xFF00, "bcc\t%0-7B"},
  {0xD400, 0xFF00, "bmi\t%0-7B"},
  {0xD500, 0xFF00, "bpl\t%0-7B"},
  {0xD600, 0xFF00, "bvs\t%0-7B"},
  {0xD700, 0xFF00, "bvc\t%0-7B"},
  {0xD800, 0xFF00, "bhi\t%0-7B"},
  {0xD900, 0xFF00, "bls\t%0-7B"},
  {0xDA00, 0xFF00, "bge\t%0-7B"},
  {0xDB00, 0xFF00, "blt\t%0-7B"},
  {0xDC00, 0xFF00, "bgt\t%0-7B"},
  {0xDD00, 0xFF00, "ble\t%0-7B"},
  /* format 17 */
  {0xDE00, 0xFF00, "bal\t%0-7B"},
  {0xDF00, 0xFF00, "swi\t%0-7d"},
  /* format 9 */
  {0x6000, 0xF800, "str\t%0-2r, [%3-5r, #%6-10W]"},
  {0x6800, 0xF800, "ldr\t%0-2r, [%3-5r, #%6-10W]"},
  {0x7000, 0xF800, "strb\t%0-2r, [%3-5r, #%6-10d]"},
  {0x7800, 0xF800, "ldrb\t%0-2r, [%3-5r, #%6-10d]"},
  /* the rest */
  {0x0000, 0x0000, "undefined instruction %0-15x"},
  {0x0000, 0x0000, 0}
};

#define BDISP23(x) ((((((x) & 0x07ff) << 11) | (((x) & 0x07ff0000) >> 16)) \
                     ^ 0x200000) - 0x200000) /* 23bit */

