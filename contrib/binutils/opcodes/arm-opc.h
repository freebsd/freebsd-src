/* Opcode table for the ARM.

   Copyright 1994, 1995, 1996, 1997, 1998, 1999, 2000, 2003
   Free Software Foundation, Inc.
   
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
   %<bitfield>w         print the bitfield plus one in decimal 
   %<bitfield>r		print as an ARM register
   %<bitfield>f		print a floating point constant if >7 else a
			floating point register
   %<code>y		print a single precision VFP reg.
			  Codes: 0=>Sm, 1=>Sd, 2=>Sn, 3=>multi-list, 4=>Sm pair
   %<code>z		print a double precision VFP reg
			  Codes: 0=>Dm, 1=>Dd, 2=>Dn, 3=>multi-list
   %c			print condition code (always bits 28-31)
   %P			print floating point precision in arithmetic insn
   %Q			print floating point precision in ldf/stf insn
   %R			print floating point rounding mode
   %<bitnum>'c		print specified char iff bit is one
   %<bitnum>`c		print specified char iff bit is zero
   %<bitnum>?ab		print a if bit is one else print b
   %p			print 'p' iff bits 12-15 are 15
   %t			print 't' iff bit 21 set and bit 24 clear
   %o			print operand2 (immediate or register + shift)
   %a			print address for ldr/str instruction
   %s                   print address for ldr/str halfword/signextend instruction
   %b			print branch destination
   %B			print arm BLX(1) destination
   %A			print address for ldc/stc/ldf/stf instruction
   %m			print register mask for ldm/stm instruction
   %C			print the PSR sub type.
   %F			print the COUNT field of a LFM/SFM instruction.
IWMMXT specific format options:
   %<bitfield>g         print as an iWMMXt 64-bit register
   %<bitfield>G         print as an iWMMXt general purpose or control register
   %<bitfield>w         print as an iWMMXt width field - [bhwd]ss/us
   %Z			print the Immediate of a WSHUFH instruction.
   %L			print as an iWMMXt N/M width field.
   %l			like 'A' except use byte offsets for 'B' & 'H' versions
Thumb specific format options:
   %D                   print Thumb register (bits 0..2 as high number if bit 7 set)
   %S                   print Thumb register (bits 3..5 as high number if bit 6 set)
   %<bitfield>I         print bitfield as a signed decimal
   				(top bit of range being the sign bit)
   %M                   print Thumb register mask
   %N                   print Thumb register mask (with LR)
   %O                   print Thumb register mask (with PC)
   %T                   print Thumb condition code (always bits 8-11)
   %I                   print cirrus signed shift immediate: bits 0..3|4..6
   %<bitfield>B         print Thumb branch destination (signed displacement)
   %<bitfield>W         print (bitfield * 4) as a decimal
   %<bitfield>H         print (bitfield * 2) as a decimal
   %<bitfield>a         print (bitfield * 4) as a pc-rel offset + decoded symbol
*/

/* Note: There is a partial ordering in this table - it must be searched from
   the top to obtain a correct match. */

static const struct arm_opcode arm_opcodes[] =
{
    /* ARM instructions.  */
    {0xe1a00000, 0xffffffff, "nop\t\t\t(mov r0,r0)"},
    {0x012FFF10, 0x0ffffff0, "bx%c\t%0-3r"},
    {0x00000090, 0x0fe000f0, "mul%c%20's\t%16-19r, %0-3r, %8-11r"},
    {0x00200090, 0x0fe000f0, "mla%c%20's\t%16-19r, %0-3r, %8-11r, %12-15r"},
    {0x01000090, 0x0fb00ff0, "swp%c%22'b\t%12-15r, %0-3r, [%16-19r]"},
    {0x00800090, 0x0fa000f0, "%22?sumull%c%20's\t%12-15r, %16-19r, %0-3r, %8-11r"},
    {0x00a00090, 0x0fa000f0, "%22?sumlal%c%20's\t%12-15r, %16-19r, %0-3r, %8-11r"},

    /* ARM V6 instructions. */
    {0xfc500000, 0xfff00000, "mrrc2\t%8-11d, %4-7d, %12-15r, %16-19r, cr%0-3d"},
    {0xfc400000, 0xfff00000, "mcrr2\t%8-11d, %4-7d, %12-15r, %16-19r, cr%0-3d"},
    {0xf1080000, 0xfffdfe3f, "cpsie\t%8'a%7'i%6'f"},
    {0xf1080000, 0xfffdfe20, "cpsie\t%8'a%7'i%6'f,#%0-4d"},
    {0xf10C0000, 0xfffdfe3f, "cpsid\t%8'a%7'i%6'f"},
    {0xf10C0000, 0xfffdfe20, "cpsid\t%8'a%7'i%6'f,#%0-4d"},
    {0xf1000000, 0xfff1fe20, "cps\t#%0-4d"},
    {0x06800010, 0x0ff00ff0, "pkhbt%c\t%12-15r, %16-19r, %0-3r"},
    {0x06800010, 0x0ff00070, "pkhbt%c\t%12-15r, %16-19r, %0-3r, LSL #%7-11d"},
    {0x06800050, 0x0ff00ff0, "pkhtb%c\t%12-15r, %16-19r, %0-3r, ASR #32"},
    {0x06800050, 0x0ff00070, "pkhtb%c\t%12-15r, %16-19r, %0-3r, ASR #%7-11d"},
    {0x01900f9f, 0x0ff00fff, "ldrex%c\tr%12-15d, [%16-19r]"},
    {0x06200f10, 0x0ff00ff0, "qadd16%c\t%12-15r, %16-19r, %0-3r"},
    {0x06200f90, 0x0ff00ff0, "qadd8%c\t%12-15r, %16-19r, %0-3r"},
    {0x06200f30, 0x0ff00ff0, "qaddsubx%c\t%12-15r, %16-19r, %0-3r"},
    {0x06200f70, 0x0ff00ff0, "qsub16%c\t%12-15r, %16-19r, %0-3r"},
    {0x06200ff0, 0x0ff00ff0, "qsub8%c\t%12-15r, %16-19r, %0-3r"},
    {0x06200f50, 0x0ff00ff0, "qsubaddx%c\t%12-15r, %16-19r, %0-3r"},
    {0x06100f10, 0x0ff00ff0, "sadd16%c\t%12-15r, %16-19r, %0-3r"},
    {0x06100f90, 0x0ff00ff0, "sadd8%c\t%12-15r, %16-19r, %0-3r"},
    {0x06100f30, 0x0ff00ff0, "saddaddx%c\t%12-15r, %16-19r, %0-3r"},
    {0x06300f10, 0x0ff00ff0, "shadd16%c\t%12-15r, %16-19r, %0-3r"},
    {0x06300f90, 0x0ff00ff0, "shadd8%c\t%12-15r, %16-19r, %0-3r"},
    {0x06300f30, 0x0ff00ff0, "shaddsubx%c\t%12-15r, %16-19r, %0-3r"},
    {0x06300f70, 0x0ff00ff0, "shsub16%c\t%12-15r, %16-19r, %0-3r"},
    {0x06300ff0, 0x0ff00ff0, "shsub8%c\t%12-15r, %16-19r, %0-3r"},
    {0x06300f50, 0x0ff00ff0, "shsubaddx%c\t%12-15r, %16-19r, %0-3r"},
    {0x06100f70, 0x0ff00ff0, "ssub16%c\t%12-15r, %16-19r, %0-3r"},
    {0x06100ff0, 0x0ff00ff0, "ssub8%c\t%12-15r, %16-19r, %0-3r"},
    {0x06100f50, 0x0ff00ff0, "ssubaddx%c\t%12-15r, %16-19r, %0-3r"},
    {0x06500f10, 0x0ff00ff0, "uadd16%c\t%12-15r, %16-19r, %0-3r"},
    {0x06500f90, 0x0ff00ff0, "uadd8%c\t%12-15r, %16-19r, %0-3r"},
    {0x06500f30, 0x0ff00ff0, "uaddsubx%c\t%12-15r, %16-19r, %0-3r"},
    {0x06700f10, 0x0ff00ff0, "uhadd16%c\t%12-15r, %16-19r, %0-3r"},
    {0x06700f90, 0x0ff00ff0, "uhadd8%c\t%12-15r, %16-19r, %0-3r"},
    {0x06700f30, 0x0ff00ff0, "uhaddsubx%c\t%12-15r, %16-19r, %0-3r"},
    {0x06700f70, 0x0ff00ff0, "uhsub16%c\t%12-15r, %16-19r, %0-3r"},
    {0x06700ff0, 0x0ff00ff0, "uhsub8%c\t%12-15r, %16-19r, %0-3r"},
    {0x06700f50, 0x0ff00ff0, "uhsubaddx%c\t%12-15r, %16-19r, %0-3r"},
    {0x06600f10, 0x0ff00ff0, "uqadd16%c\t%12-15r, %16-19r, %0-3r"},
    {0x06600f90, 0x0ff00ff0, "uqadd8%c\t%12-15r, %16-19r, %0-3r"},
    {0x06600f30, 0x0ff00ff0, "uqaddsubx%c\t%12-15r, %16-19r, %0-3r"},
    {0x06600f70, 0x0ff00ff0, "uqsub16%c\t%12-15r, %16-19r, %0-3r"},
    {0x06600ff0, 0x0ff00ff0, "uqsub8%c\t%12-15r, %16-19r, %0-3r"},
    {0x06600f50, 0x0ff00ff0, "uqsubaddx%c\t%12-15r, %16-19r, %0-3r"},
    {0x06500f70, 0x0ff00ff0, "usub16%c\t%12-15r, %16-19r, %0-3r"},
    {0x06500ff0, 0x0ff00ff0, "usub8%c\t%12-15r, %16-19r, %0-3r"},
    {0x06500f50, 0x0ff00ff0, "usubaddx%c\t%12-15r, %16-19r, %0-3r"},
    {0x06bf0f30, 0x0fff0ff0, "rev%c\t\%12-15r, %0-3r"},
    {0x06bf0fb0, 0x0fff0ff0, "rev16%c\t\%12-15r, %0-3r"},
    {0x06ff0fb0, 0x0fff0ff0, "revsh%c\t\%12-15r, %0-3r"},
    {0xf8100a00, 0xfe50ffff, "rfe%23?id%24?ba\t\%16-19r%21'!"},
    {0x06bf0070, 0x0fff0ff0, "sxth%c %12-15r,%0-3r"},
    {0x06bf0470, 0x0fff0ff0, "sxth%c %12-15r,%0-3r, ROR #8"},
    {0x06bf0870, 0x0fff0ff0, "sxth%c %12-15r,%0-3r, ROR #16"},
    {0x06bf0c70, 0x0fff0ff0, "sxth%c %12-15r,%0-3r, ROR #24"},
    {0x068f0070, 0x0fff0ff0, "sxtb16%c %12-15r,%0-3r"},
    {0x068f0470, 0x0fff0ff0, "sxtb16%c %12-15r,%0-3r, ROR #8"},
    {0x068f0870, 0x0fff0ff0, "sxtb16%c %12-15r,%0-3r, ROR #16"},
    {0x068f0c70, 0x0fff0ff0, "sxtb16%c %12-15r,%0-3r, ROR #24"},
    {0x06af0070, 0x0fff0ff0, "sxtb%c %12-15r,%0-3r"},
    {0x06af0470, 0x0fff0ff0, "sxtb%c %12-15r,%0-3r, ROR #8"},
    {0x06af0870, 0x0fff0ff0, "sxtb%c %12-15r,%0-3r, ROR #16"},
    {0x06af0c70, 0x0fff0ff0, "sxtb%c %12-15r,%0-3r, ROR #24"},
    {0x06ff0070, 0x0fff0ff0, "uxth%c %12-15r,%0-3r"},
    {0x06ff0470, 0x0fff0ff0, "uxth%c %12-15r,%0-3r, ROR #8"},
    {0x06ff0870, 0x0fff0ff0, "uxth%c %12-15r,%0-3r, ROR #16"},
    {0x06ff0c70, 0x0fff0ff0, "uxth%c %12-15r,%0-3r, ROR #24"},
    {0x06cf0070, 0x0fff0ff0, "uxtb16%c %12-15r,%0-3r"},
    {0x06cf0470, 0x0fff0ff0, "uxtb16%c %12-15r,%0-3r, ROR #8"},
    {0x06cf0870, 0x0fff0ff0, "uxtb16%c %12-15r,%0-3r, ROR #16"},
    {0x06cf0c70, 0x0fff0ff0, "uxtb16%c %12-15r,%0-3r, ROR #24"},
    {0x06ef0070, 0x0fff0ff0, "uxtb%c %12-15r,%0-3r"},
    {0x06ef0470, 0x0fff0ff0, "uxtb%c %12-15r,%0-3r, ROR #8"},
    {0x06ef0870, 0x0fff0ff0, "uxtb%c %12-15r,%0-3r, ROR #16"},
    {0x06ef0c70, 0x0fff0ff0, "uxtb%c %12-15r,%0-3r, ROR #24"},
    {0x06b00070, 0x0ff00ff0, "sxtah%c\t%12-15r, %16-19r, %0-3r"},
    {0x06b00470, 0x0ff00ff0, "sxtah%c\t%12-15r, %16-19r, %0-3r, ROR #8"},
    {0x06b00870, 0x0ff00ff0, "sxtah%c\t%12-15r, %16-19r, %0-3r, ROR #16"},
    {0x06b00c70, 0x0ff00ff0, "sxtah%c\t%12-15r, %16-19r, %0-3r, ROR #24"},
    {0x06800070, 0x0ff00ff0, "sxtab16%c\t%12-15r, %16-19r, %0-3r"},
    {0x06800470, 0x0ff00ff0, "sxtab16%c\t%12-15r, %16-19r, %0-3r, ROR #8"},
    {0x06800870, 0x0ff00ff0, "sxtab16%c\t%12-15r, %16-19r, %0-3r, ROR #16"},
    {0x06800c70, 0x0ff00ff0, "sxtab16%c\t%12-15r, %16-19r, %0-3r, ROR #24"},
    {0x06a00070, 0x0ff00ff0, "sxtab%c\t%12-15r, %16-19r, %0-3r"},
    {0x06a00470, 0x0ff00ff0, "sxtab%c\t%12-15r, %16-19r, %0-3r, ROR #8"},
    {0x06a00870, 0x0ff00ff0, "sxtab%c\t%12-15r, %16-19r, %0-3r, ROR #16"},
    {0x06a00c70, 0x0ff00ff0, "sxtab%c\t%12-15r, %16-19r, %0-3r, ROR #24"},
    {0x06f00070, 0x0ff00ff0, "uxtah%c\t%12-15r, %16-19r, %0-3r"},
    {0x06f00470, 0x0ff00ff0, "uxtah%c\t%12-15r, %16-19r, %0-3r, ROR #8"},
    {0x06f00870, 0x0ff00ff0, "uxtah%c\t%12-15r, %16-19r, %0-3r, ROR #16"},
    {0x06f00c70, 0x0ff00ff0, "uxtah%c\t%12-15r, %16-19r, %0-3r, ROR #24"},
    {0x06c00070, 0x0ff00ff0, "uxtab16%c\t%12-15r, %16-19r, %0-3r"},
    {0x06c00470, 0x0ff00ff0, "uxtab16%c\t%12-15r, %16-19r, %0-3r, ROR #8"},
    {0x06c00870, 0x0ff00ff0, "uxtab16%c\t%12-15r, %16-19r, %0-3r, ROR #16"},
    {0x06c00c70, 0x0ff00ff0, "uxtab16%c\t%12-15r, %16-19r, %0-3r, ROR #24"},
    {0x06e00070, 0x0ff00ff0, "uxtab%c\t%12-15r, %16-19r, %0-3r"},
    {0x06e00470, 0x0ff00ff0, "uxtab%c\t%12-15r, %16-19r, %0-3r, ROR #8"},
    {0x06e00870, 0x0ff00ff0, "uxtab%c\t%12-15r, %16-19r, %0-3r, ROR #16"},
    {0x06e00c70, 0x0ff00ff0, "uxtab%c\t%12-15r, %16-19r, %0-3r, ROR #24"},
    {0x068000b0, 0x0ff00ff0, "sel%c\t%12-15r, %16-19r, %0-3r"},
    {0xf1010000, 0xfffffc00, "setend\t%9?ble"},
    {0x0700f010, 0x0ff0f0d0, "smuad%5'x%c\t%16-19r, %0-3r, %8-11r"},
    {0x0700f050, 0x0ff0f0d0, "smusd%5'x%c\t%16-19r, %0-3r, %8-11r"},
    {0x07000010, 0x0ff000d0, "smlad%5'x%c\t%16-19r, %0-3r, %8-11r, %12-15r"},
    {0x07400010, 0x0ff000d0, "smlald%5'x%c\t%12-15r, %16-19r, %0-3r, %8-11r"},
    {0x07000050, 0x0ff000d0, "smlsd%5'x%c\t%16-19r, %0-3r, %8-11r, %12-15r"},
    {0x07400050, 0x0ff000d0, "smlsld%5'x%c\t%12-15r, %16-19r, %0-3r, %8-11r"},
    {0x0750f010, 0x0ff0f0d0, "smmul%5'r%c\t%16-19r, %0-3r, %8-11r"},
    {0x07500010, 0x0ff000d0, "smmla%5'r%c\t%16-19r, %0-3r, %8-11r, %12-15r"},
    {0x075000d0, 0x0ff000d0, "smmls%5'r%c\t%16-19r, %0-3r, %8-11r, %12-15r"},
    {0xf84d0500, 0xfe5fffe0, "srs%23?id%24?ba\t#%0-4d%21'!"},
    {0x06a00010, 0x0fe00ff0, "ssat%c\t%12-15r, #%16-20W, %0-3r"},
    {0x06a00010, 0x0fe00070, "ssat%c\t%12-15r, #%16-20W, %0-3r, LSL #%7-11d"},
    {0x06a00050, 0x0fe00070, "ssat%c\t%12-15r, #%16-20W, %0-3r, ASR #%7-11d"},
    {0x06a00f30, 0x0ff00ff0, "ssat16%c\t%12-15r, #%16-19W, %0-3r"},
    {0x01800f90, 0x0ff00ff0, "strex%c\t%12-15r, %0-3r, [%16-19r]"},
    {0x00400090, 0x0ff000f0, "umaal%c\t%12-15r, %16-19r, %0-3r, %8-11r"},
    {0x0780f010, 0x0ff0f0f0, "usad8%c\t%16-19r, %0-3r, %8-11r"},
    {0x07800010, 0x0ff000f0, "usada8%c\t%16-19r, %0-3r, %8-11r, %12-15r"},
    {0x06e00010, 0x0fe00ff0, "usat%c\t%12-15r, #%16-20d, %0-3r"},
    {0x06e00010, 0x0fe00070, "usat%c\t%12-15r, #%16-20d, %0-3r, LSL #%7-11d"},
    {0x06e00050, 0x0fe00070, "usat%c\t%12-15r, #%16-20d, %0-3r, ASR #%7-11d"},
    {0x06e00f30, 0x0ff00ff0, "usat16%c\t%12-15r, #%16-19d, %0-3r"},

    /* V5J instruction.  */
    {0x012fff20, 0x0ffffff0, "bxj%c\t%0-3r"},

    /* XScale instructions.  */
    {0x0e200010, 0x0fff0ff0, "mia%c\tacc0, %0-3r, %12-15r"},
    {0x0e280010, 0x0fff0ff0, "miaph%c\tacc0, %0-3r, %12-15r"},
    {0x0e2c0010, 0x0ffc0ff0, "mia%17'T%17`B%16'T%16`B%c\tacc0, %0-3r, %12-15r"},
    {0x0c400000, 0x0ff00fff, "mar%c\tacc0, %12-15r, %16-19r"},
    {0x0c500000, 0x0ff00fff, "mra%c\t%12-15r, %16-19r, acc0"},
    {0xf450f000, 0xfc70f000, "pld\t%a"},
    
    /* Intel Wireless MMX technology instructions.  */
#define FIRST_IWMMXT_INSN 0x0e130130
#define IWMMXT_INSN_COUNT 47
    {0x0e130130, 0x0f3f0fff, "tandc%22-23w%c\t%12-15r"},
    {0x0e400010, 0x0ff00f3f, "tbcst%6-7w%c\t%16-19g, %12-15r"},
    {0x0e130170, 0x0f3f0ff8, "textrc%22-23w%c\t%12-15r, #%0-2d"},
    {0x0e100070, 0x0f300ff0, "textrm%3?su%22-23w%c\t%12-15r, %16-19g, #%0-2d"},
    {0x0e600010, 0x0ff00f38, "tinsr%6-7w%c\t%16-19g, %12-15r, #%0-2d"},
    {0x0e000110, 0x0ff00fff, "tmcr%c\t%16-19G, %12-15r"},
    {0x0c400000, 0x0ff00ff0, "tmcrr%c\t%0-3g, %12-15r, %16-19r"},
    {0x0e2c0010, 0x0ffc0e10, "tmia%17?tb%16?tb%c\t%5-8g, %0-3r, %12-15r"},
    {0x0e200010, 0x0fff0e10, "tmia%c\t%5-8g, %0-3r, %12-15r"},
    {0x0e280010, 0x0fff0e10, "tmiaph%c\t%5-8g, %0-3r, %12-15r"},
    {0x0e100030, 0x0f300fff, "tmovmsk%22-23w%c\t%12-15r, %16-19g"},
    {0x0e100110, 0x0ff00ff0, "tmrc%c\t%12-15r, %16-19G"},
    {0x0c500000, 0x0ff00ff0, "tmrrc%c\t%12-15r, %16-19r, %0-3g"},
    {0x0e130150, 0x0f3f0fff, "torc%22-23w%c\t%12-15r"},
    {0x0e0001c0, 0x0f300fff, "wacc%22-23w%c\t%12-15g, %16-19g"},
    {0x0e000180, 0x0f000ff0, "wadd%20-23w%c\t%12-15g, %16-19g, %0-3g"},
    {0x0e000020, 0x0f800ff0, "waligni%c\t%12-15g, %16-19g, %0-3g, #%20-22d"},
    {0x0e800020, 0x0fc00ff0, "walignr%20-21d%c\t%12-15g, %16-19g, %0-3g"},
    {0x0e200000, 0x0fe00ff0, "wand%20'n%c\t%12-15g, %16-19g, %0-3g"},
    {0x0e800000, 0x0fa00ff0, "wavg2%22?hb%20'r%c\t%12-15g, %16-19g, %0-3g"},
    {0x0e000060, 0x0f300ff0, "wcmpeq%22-23w%c\t%12-15g, %16-19g, %0-3g"},
    {0x0e100060, 0x0f100ff0, "wcmpgt%21?su%22-23w%c\t%12-15g, %16-19g, %0-3g"},
    {0xfc100100, 0xfe500f00, "wldrw\t%12-15G, %A"},
    {0x0c100000, 0x0e100e00, "wldr%L%c\t%12-15g, %l"},
    {0x0e400100, 0x0fc00ff0, "wmac%21?su%20'z%c\t%12-15g, %16-19g, %0-3g"},
    {0x0e800100, 0x0fd00ff0, "wmadd%21?su%c\t%12-15g, %16-19g, %0-3g"},
    {0x0e000160, 0x0f100ff0, "wmax%21?su%22-23w%c\t%12-15g, %16-19g, %0-3g"},
    {0x0e100160, 0x0f100ff0, "wmin%21?su%22-23w%c\t%12-15g, %16-19g, %0-3g"},
    {0x0e000100, 0x0fc00ff0, "wmul%21?su%20?ml%c\t%12-15g, %16-19g, %0-3g"},
    {0x0e000000, 0x0ff00ff0, "wor%c\t%12-15g, %16-19g, %0-3g"},
    {0x0e000080, 0x0f000ff0, "wpack%20-23w%c\t%12-15g, %16-19g, %0-3g"},
    {0x0e300040, 0x0f300ff0, "wror%22-23w%8'g%c\t%12-15g, %16-19g, %0-3g"},
    {0x0e300148, 0x0f300ffc, "wror%22-23w%8'g%c\t%12-15g, %16-19g, %0-3G"},
    {0x0e000120, 0x0fa00ff0, "wsad%22?hb%20'z%c\t%12-15g, %16-19g, %0-3g"},
    {0x0e0001e0, 0x0f000ff0, "wshufh%c\t%12-15g, %16-19g, #%Z"},
    {0x0e100040, 0x0f300ff0, "wsll%22-23w%8'g%c\t%12-15g, %16-19g, %0-3g"},
    {0x0e100148, 0x0f300ffc, "wsll%22-23w%8'g%c\t%12-15g, %16-19g, %0-3G"},
    {0x0e000040, 0x0f300ff0, "wsra%22-23w%8'g%c\t%12-15g, %16-19g, %0-3g"},
    {0x0e000148, 0x0f300ffc, "wsra%22-23w%8'g%c\t%12-15g, %16-19g, %0-3G"},
    {0x0e200040, 0x0f300ff0, "wsrl%22-23w%8'g%c\t%12-15g, %16-19g, %0-3g"},
    {0x0e200148, 0x0f300ffc, "wsrl%22-23w%8'g%c\t%12-15g, %16-19g, %0-3G"},
    {0xfc000100, 0xfe500f00, "wstrw\t%12-15G, %A"},
    {0x0c000000, 0x0e100e00, "wstr%L%c\t%12-15g, %l"},
    {0x0e0001a0, 0x0f000ff0, "wsub%20-23w%c\t%12-15g, %16-19g, %0-3g"},
    {0x0e0000c0, 0x0f100fff, "wunpckeh%21?su%22-23w%c\t%12-15g, %16-19g"},
    {0x0e0000e0, 0x0f100fff, "wunpckel%21?su%22-23w%c\t%12-15g, %16-19g"},
    {0x0e1000c0, 0x0f300ff0, "wunpckih%22-23w%c\t%12-15g, %16-19g, %0-3g"},
    {0x0e1000e0, 0x0f300ff0, "wunpckil%22-23w%c\t%12-15g, %16-19g, %0-3g"},
    {0x0e100000, 0x0ff00ff0, "wxor%c\t%12-15g, %16-19g, %0-3g"},

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

    /* ARM Instructions.  */
    {0x00000090, 0x0e100090, "str%c%6's%5?hb\t%12-15r, %s"},
    {0x00100090, 0x0e100090, "ldr%c%6's%5?hb\t%12-15r, %s"},
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

    /* Floating point coprocessor (FPA) instructions */
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

    /* Floating point coprocessor (VFP) instructions */
    {0x0eb00bc0, 0x0fff0ff0, "fabsd%c\t%1z, %0z"},
    {0x0eb00ac0, 0x0fbf0fd0, "fabss%c\t%1y, %0y"},
    {0x0e300b00, 0x0ff00ff0, "faddd%c\t%1z, %2z, %0z"},
    {0x0e300a00, 0x0fb00f50, "fadds%c\t%1y, %2y, %1y"},
    {0x0eb40b40, 0x0fff0f70, "fcmp%7'ed%c\t%1z, %0z"},
    {0x0eb40a40, 0x0fbf0f50, "fcmp%7'es%c\t%1y, %0y"},
    {0x0eb50b40, 0x0fff0f70, "fcmp%7'ezd%c\t%1z"},
    {0x0eb50a40, 0x0fbf0f70, "fcmp%7'ezs%c\t%1y"},
    {0x0eb00b40, 0x0fff0ff0, "fcpyd%c\t%1z, %0z"},
    {0x0eb00a40, 0x0fbf0fd0, "fcpys%c\t%1y, %0y"},
    {0x0eb70ac0, 0x0fff0fd0, "fcvtds%c\t%1z, %0y"},
    {0x0eb70bc0, 0x0fbf0ff0, "fcvtsd%c\t%1y, %0z"},
    {0x0e800b00, 0x0ff00ff0, "fdivd%c\t%1z, %2z, %0z"},
    {0x0e800a00, 0x0fb00f50, "fdivs%c\t%1y, %2y, %0y"},
    {0x0d100b00, 0x0f700f00, "fldd%c\t%1z, %A"},
    {0x0c900b00, 0x0fd00f00, "fldmia%0?xd%c\t%16-19r%21'!, %3z"},
    {0x0d300b00, 0x0ff00f00, "fldmdb%0?xd%c\t%16-19r!, %3z"},
    {0x0d100a00, 0x0f300f00, "flds%c\t%1y, %A"},
    {0x0c900a00, 0x0f900f00, "fldmias%c\t%16-19r%21'!, %3y"},
    {0x0d300a00, 0x0fb00f00, "fldmdbs%c\t%16-19r!, %3y"},
    {0x0e000b00, 0x0ff00ff0, "fmacd%c\t%1z, %2z, %0z"},
    {0x0e000a00, 0x0fb00f50, "fmacs%c\t%1y, %2y, %0y"},
    {0x0e200b10, 0x0ff00fff, "fmdhr%c\t%2z, %12-15r"},
    {0x0e000b10, 0x0ff00fff, "fmdlr%c\t%2z, %12-15r"},
    {0x0c400b10, 0x0ff00ff0, "fmdrr%c\t%0z, %12-15r, %16-19r"},
    {0x0e300b10, 0x0ff00fff, "fmrdh%c\t%12-15r, %2z"},
    {0x0e100b10, 0x0ff00fff, "fmrdl%c\t%12-15r, %2z"},
    {0x0c500b10, 0x0ff00ff0, "fmrrd%c\t%12-15r, %16-19r, %0z"},
    {0x0c500a10, 0x0ff00fd0, "fmrrs%c\t%12-15r, %16-19r, %4y"},
    {0x0e100a10, 0x0ff00f7f, "fmrs%c\t%12-15r, %2y"},
    {0x0ef1fa10, 0x0fffffff, "fmstat%c"},
    {0x0ef00a10, 0x0fff0fff, "fmrx%c\t%12-15r, fpsid"},
    {0x0ef10a10, 0x0fff0fff, "fmrx%c\t%12-15r, fpscr"},
    {0x0ef80a10, 0x0fff0fff, "fmrx%c\t%12-15r, fpexc"},
    {0x0ef90a10, 0x0fff0fff, "fmrx%c\t%12-15r, fpinst\t@ Impl def"},
    {0x0efa0a10, 0x0fff0fff, "fmrx%c\t%12-15r, fpinst2\t@ Impl def"},
    {0x0ef00a10, 0x0ff00fff, "fmrx%c\t%12-15r, <impl def 0x%16-19x>"},
    {0x0e100b00, 0x0ff00ff0, "fmscd%c\t%1z, %2z, %0z"},
    {0x0e100a00, 0x0fb00f50, "fmscs%c\t%1y, %2y, %0y"},
    {0x0e000a10, 0x0ff00f7f, "fmsr%c\t%2y, %12-15r"},
    {0x0c400a10, 0x0ff00fd0, "fmsrr%c\t%12-15r, %16-19r, %4y"},
    {0x0e200b00, 0x0ff00ff0, "fmuld%c\t%1z, %2z, %0z"},
    {0x0e200a00, 0x0fb00f50, "fmuls%c\t%1y, %2y, %0y"},
    {0x0ee00a10, 0x0fff0fff, "fmxr%c\tfpsid, %12-15r"},
    {0x0ee10a10, 0x0fff0fff, "fmxr%c\tfpscr, %12-15r"},
    {0x0ee80a10, 0x0fff0fff, "fmxr%c\tfpexc, %12-15r"},
    {0x0ee90a10, 0x0fff0fff, "fmxr%c\tfpinst, %12-15r\t@ Impl def"},
    {0x0eea0a10, 0x0fff0fff, "fmxr%c\tfpinst2, %12-15r\t@ Impl def"},
    {0x0ee00a10, 0x0ff00fff, "fmxr%c\t<impl def 0x%16-19x>, %12-15r"},
    {0x0eb10b40, 0x0fff0ff0, "fnegd%c\t%1z, %0z"},
    {0x0eb10a40, 0x0fbf0fd0, "fnegs%c\t%1y, %0y"},
    {0x0e000b40, 0x0ff00ff0, "fnmacd%c\t%1z, %2z, %0z"},
    {0x0e000a40, 0x0fb00f50, "fnmacs%c\t%1y, %2y, %0y"},
    {0x0e100b40, 0x0ff00ff0, "fnmscd%c\t%1z, %2z, %0z"},
    {0x0e100a40, 0x0fb00f50, "fnmscs%c\t%1y, %2y, %0y"},
    {0x0e200b40, 0x0ff00ff0, "fnmuld%c\t%1z, %2z, %0z"},
    {0x0e200a40, 0x0fb00f50, "fnmuls%c\t%1y, %2y, %0y"},
    {0x0eb80bc0, 0x0fff0fd0, "fsitod%c\t%1z, %0y"},
    {0x0eb80ac0, 0x0fbf0fd0, "fsitos%c\t%1y, %0y"},
    {0x0eb10bc0, 0x0fff0ff0, "fsqrtd%c\t%1z, %0z"},
    {0x0eb10ac0, 0x0fbf0fd0, "fsqrts%c\t%1y, %0y"},
    {0x0d000b00, 0x0f700f00, "fstd%c\t%1z, %A"},
    {0x0c800b00, 0x0fd00f00, "fstmia%0?xd%c\t%16-19r%21'!, %3z"},
    {0x0d200b00, 0x0ff00f00, "fstmdb%0?xd%c\t%16-19r!, %3z"},
    {0x0d000a00, 0x0f300f00, "fsts%c\t%1y, %A"},
    {0x0c800a00, 0x0f900f00, "fstmias%c\t%16-19r%21'!, %3y"},
    {0x0d200a00, 0x0fb00f00, "fstmdbs%c\t%16-19r!, %3y"},
    {0x0e300b40, 0x0ff00ff0, "fsubd%c\t%1z, %2z, %0z"},
    {0x0e300a40, 0x0fb00f50, "fsubs%c\t%1y, %2y, %0y"},
    {0x0ebc0b40, 0x0fbe0f70, "fto%16?sui%7'zd%c\t%1y, %0z"},
    {0x0ebc0a40, 0x0fbe0f50, "fto%16?sui%7'zs%c\t%1y, %0y"},
    {0x0eb80b40, 0x0fff0fd0, "fuitod%c\t%1z, %0y"},
    {0x0eb80a40, 0x0fbf0fd0, "fuitos%c\t%1y, %0y"},

    /* Cirrus coprocessor instructions.  */
    {0x0d100400, 0x0f500f00, "cfldrs%c\tmvf%12-15d, %A"},
    {0x0c100400, 0x0f500f00, "cfldrs%c\tmvf%12-15d, %A"},
    {0x0d500400, 0x0f500f00, "cfldrd%c\tmvd%12-15d, %A"},
    {0x0c500400, 0x0f500f00, "cfldrd%c\tmvd%12-15d, %A"}, 
    {0x0d100500, 0x0f500f00, "cfldr32%c\tmvfx%12-15d, %A"},
    {0x0c100500, 0x0f500f00, "cfldr32%c\tmvfx%12-15d, %A"},
    {0x0d500500, 0x0f500f00, "cfldr64%c\tmvdx%12-15d, %A"},
    {0x0c500500, 0x0f500f00, "cfldr64%c\tmvdx%12-15d, %A"},
    {0x0d000400, 0x0f500f00, "cfstrs%c\tmvf%12-15d, %A"},
    {0x0c000400, 0x0f500f00, "cfstrs%c\tmvf%12-15d, %A"},
    {0x0d400400, 0x0f500f00, "cfstrd%c\tmvd%12-15d, %A"},
    {0x0c400400, 0x0f500f00, "cfstrd%c\tmvd%12-15d, %A"},
    {0x0d000500, 0x0f500f00, "cfstr32%c\tmvfx%12-15d, %A"},
    {0x0c000500, 0x0f500f00, "cfstr32%c\tmvfx%12-15d, %A"},
    {0x0d400500, 0x0f500f00, "cfstr64%c\tmvdx%12-15d, %A"},
    {0x0c400500, 0x0f500f00, "cfstr64%c\tmvdx%12-15d, %A"},
    {0x0e000450, 0x0ff00ff0, "cfmvsr%c\tmvf%16-19d, %12-15r"},
    {0x0e100450, 0x0ff00ff0, "cfmvrs%c\t%12-15r, mvf%16-19d"},
    {0x0e000410, 0x0ff00ff0, "cfmvdlr%c\tmvd%16-19d, %12-15r"},
    {0x0e100410, 0x0ff00ff0, "cfmvrdl%c\t%12-15r, mvd%16-19d"},
    {0x0e000430, 0x0ff00ff0, "cfmvdhr%c\tmvd%16-19d, %12-15r"},
    {0x0e100430, 0x0ff00fff, "cfmvrdh%c\t%12-15r, mvd%16-19d"},
    {0x0e000510, 0x0ff00fff, "cfmv64lr%c\tmvdx%16-19d, %12-15r"},
    {0x0e100510, 0x0ff00fff, "cfmvr64l%c\t%12-15r, mvdx%16-19d"},
    {0x0e000530, 0x0ff00fff, "cfmv64hr%c\tmvdx%16-19d, %12-15r"},
    {0x0e100530, 0x0ff00fff, "cfmvr64h%c\t%12-15r, mvdx%16-19d"},
    {0x0e200440, 0x0ff00fff, "cfmval32%c\tmvax%12-15d, mvfx%16-19d"},
    {0x0e100440, 0x0ff00fff, "cfmv32al%c\tmvfx%12-15d, mvax%16-19d"},
    {0x0e200460, 0x0ff00fff, "cfmvam32%c\tmvax%12-15d, mvfx%16-19d"},
    {0x0e100460, 0x0ff00fff, "cfmv32am%c\tmvfx%12-15d, mvax%16-19d"},
    {0x0e200480, 0x0ff00fff, "cfmvah32%c\tmvax%12-15d, mvfx%16-19d"},
    {0x0e100480, 0x0ff00fff, "cfmv32ah%c\tmvfx%12-15d, mvax%16-19d"},
    {0x0e2004a0, 0x0ff00fff, "cfmva32%c\tmvax%12-15d, mvfx%16-19d"},
    {0x0e1004a0, 0x0ff00fff, "cfmv32a%c\tmvfx%12-15d, mvax%16-19d"},
    {0x0e2004c0, 0x0ff00fff, "cfmva64%c\tmvax%12-15d, mvdx%16-19d"},
    {0x0e1004c0, 0x0ff00fff, "cfmv64a%c\tmvdx%12-15d, mvax%16-19d"},
    {0x0e2004e0, 0x0fff0fff, "cfmvsc32%c\tdspsc, mvdx%12-15d"},
    {0x0e1004e0, 0x0fff0fff, "cfmv32sc%c\tmvdx%12-15d, dspsc"},
    {0x0e000400, 0x0ff00fff, "cfcpys%c\tmvf%12-15d, mvf%16-19d"},
    {0x0e000420, 0x0ff00fff, "cfcpyd%c\tmvd%12-15d, mvd%16-19d"},
    {0x0e000460, 0x0ff00fff, "cfcvtsd%c\tmvd%12-15d, mvf%16-19d"},
    {0x0e000440, 0x0ff00fff, "cfcvtds%c\tmvf%12-15d, mvd%16-19d"},
    {0x0e000480, 0x0ff00fff, "cfcvt32s%c\tmvf%12-15d, mvfx%16-19d"},
    {0x0e0004a0, 0x0ff00fff, "cfcvt32d%c\tmvd%12-15d, mvfx%16-19d"},
    {0x0e0004c0, 0x0ff00fff, "cfcvt64s%c\tmvf%12-15d, mvdx%16-19d"},
    {0x0e0004e0, 0x0ff00fff, "cfcvt64d%c\tmvd%12-15d, mvdx%16-19d"},
    {0x0e100580, 0x0ff00fff, "cfcvts32%c\tmvfx%12-15d, mvf%16-19d"},
    {0x0e1005a0, 0x0ff00fff, "cfcvtd32%c\tmvfx%12-15d, mvd%16-19d"},
    {0x0e1005c0, 0x0ff00fff, "cftruncs32%c\tmvfx%12-15d, mvf%16-19d"},
    {0x0e1005e0, 0x0ff00fff, "cftruncd32%c\tmvfx%12-15d, mvd%16-19d"},
    {0x0e000550, 0x0ff00ff0, "cfrshl32%c\tmvfx%16-19d, mvfx%0-3d, %12-15r"},
    {0x0e000570, 0x0ff00ff0, "cfrshl64%c\tmvdx%16-19d, mvdx%0-3d, %12-15r"},
    {0x0e000500, 0x0ff00f00, "cfsh32%c\tmvfx%12-15d, mvfx%16-19d, #%I"},
    {0x0e200500, 0x0ff00f00, "cfsh64%c\tmvdx%12-15d, mvdx%16-19d, #%I"},
    {0x0e100490, 0x0ff00ff0, "cfcmps%c\t%12-15r, mvf%16-19d, mvf%0-3d"},
    {0x0e1004b0, 0x0ff00ff0, "cfcmpd%c\t%12-15r, mvd%16-19d, mvd%0-3d"},
    {0x0e100590, 0x0ff00ff0, "cfcmp32%c\t%12-15r, mvfx%16-19d, mvfx%0-3d"},
    {0x0e1005b0, 0x0ff00ff0, "cfcmp64%c\t%12-15r, mvdx%16-19d, mvdx%0-3d"},
    {0x0e300400, 0x0ff00fff, "cfabss%c\tmvf%12-15d, mvf%16-19d"},
    {0x0e300420, 0x0ff00fff, "cfabsd%c\tmvd%12-15d, mvd%16-19d"},
    {0x0e300440, 0x0ff00fff, "cfnegs%c\tmvf%12-15d, mvf%16-19d"},
    {0x0e300460, 0x0ff00fff, "cfnegd%c\tmvd%12-15d, mvd%16-19d"},
    {0x0e300480, 0x0ff00ff0, "cfadds%c\tmvf%12-15d, mvf%16-19d, mvf%0-3d"},
    {0x0e3004a0, 0x0ff00ff0, "cfaddd%c\tmvd%12-15d, mvd%16-19d, mvd%0-3d"},
    {0x0e3004c0, 0x0ff00ff0, "cfsubs%c\tmvf%12-15d, mvf%16-19d, mvf%0-3d"},
    {0x0e3004e0, 0x0ff00ff0, "cfsubd%c\tmvd%12-15d, mvd%16-19d, mvd%0-3d"},
    {0x0e100400, 0x0ff00ff0, "cfmuls%c\tmvf%12-15d, mvf%16-19d, mvf%0-3d"},
    {0x0e100420, 0x0ff00ff0, "cfmuld%c\tmvd%12-15d, mvd%16-19d, mvd%0-3d"},
    {0x0e300500, 0x0ff00fff, "cfabs32%c\tmvfx%12-15d, mvfx%16-19d"},
    {0x0e300520, 0x0ff00fff, "cfabs64%c\tmvdx%12-15d, mvdx%16-19d"},
    {0x0e300540, 0x0ff00fff, "cfneg32%c\tmvfx%12-15d, mvfx%16-19d"},
    {0x0e300560, 0x0ff00fff, "cfneg64%c\tmvdx%12-15d, mvdx%16-19d"},
    {0x0e300580, 0x0ff00ff0, "cfadd32%c\tmvfx%12-15d, mvfx%16-19d, mvfx%0-3d"},
    {0x0e3005a0, 0x0ff00ff0, "cfadd64%c\tmvdx%12-15d, mvdx%16-19d, mvdx%0-3d"},
    {0x0e3005c0, 0x0ff00ff0, "cfsub32%c\tmvfx%12-15d, mvfx%16-19d, mvfx%0-3d"},
    {0x0e3005e0, 0x0ff00ff0, "cfsub64%c\tmvdx%12-15d, mvdx%16-19d, mvdx%0-3d"},
    {0x0e100500, 0x0ff00ff0, "cfmul32%c\tmvfx%12-15d, mvfx%16-19d, mvfx%0-3d"},
    {0x0e100520, 0x0ff00ff0, "cfmul64%c\tmvdx%12-15d, mvdx%16-19d, mvdx%0-3d"},
    {0x0e100540, 0x0ff00ff0, "cfmac32%c\tmvfx%12-15d, mvfx%16-19d, mvfx%0-3d"},
    {0x0e100560, 0x0ff00ff0, "cfmsc32%c\tmvfx%12-15d, mvfx%16-19d, mvfx%0-3d"},
    {0x0e000600, 0x0ff00f00, "cfmadd32%c\tmvax%5-7d, mvfx%12-15d, mvfx%16-19d, mvfx%0-3d"},
    {0x0e100600, 0x0ff00f00, "cfmsub32%c\tmvax%5-7d, mvfx%12-15d, mvfx%16-19d, mvfx%0-3d"},
    {0x0e200600, 0x0ff00f00, "cfmadda32%c\tmvax%5-7d, mvax%12-15d, mvfx%16-19d, mvfx%0-3d"},
    {0x0e300600, 0x0ff00f00, "cfmsuba32%c\tmvax%5-7d, mvax%12-15d, mvfx%16-19d, mvfx%0-3d"},

    /* Generic coprocessor instructions */
    {0x0c400000, 0x0ff00000, "mcrr%c\t%8-11d, %4-7d, %12-15r, %16-19r, cr%0-3d"},
    {0x0c500000, 0x0ff00000, "mrrc%c\t%8-11d, %4-7d, %12-15r, %16-19r, cr%0-3d"},
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

static const struct thumb_opcode thumb_opcodes[] =
{
  /* Thumb instructions.  */

  /* ARM V6.  */
  {0xb660, 0xfff8, "cpsie\t%2'a%1'i%0'f"},
  {0xb670, 0xfff8, "cpsid\t%2'a%1'i%0'f"},
  {0x4600, 0xffc0, "cpy\t%0-2r, %3-5r"},
  {0xba00, 0xffc0, "rev\t%0-2r, %3-5r"},
  {0xba40, 0xffc0, "rev16\t%0-2r, %3-5r"},
  {0xbac0, 0xffc0, "revsh\t%0-2r, %3-5r"},
  {0xb650, 0xfff7, "setend\t%3?ble\t"},
  {0xb200, 0xffc0, "sxth\t%0-2r, %3-5r"},
  {0xb240, 0xffc0, "sxtb\t%0-2r, %3-5r"},
  {0xb280, 0xffc0, "uxth\t%0-2r, %3-5r"},
  {0xb2c0, 0xffc0, "uxtb\t%0-2r, %3-5r"},

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

