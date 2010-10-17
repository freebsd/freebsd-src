/* Opcode table for the Atmel AVR micro controllers.

   Copyright 2000 Free Software Foundation, Inc.
   Contributed by Denis Chertykov <denisc@overta.ru>
   
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

#define AVR_ISA_1200  0x0001 /* in the beginning there was ... */
#define AVR_ISA_LPM   0x0002 /* device has LPM */
#define AVR_ISA_LPMX  0x0004 /* device has LPM Rd,Z[+] */
#define AVR_ISA_SRAM  0x0008 /* device has SRAM (LD, ST, PUSH, POP, ...) */
#define AVR_ISA_MEGA  0x0020 /* device has >8K program memory (JMP and CALL
				supported, no 8K wrap on RJMP and RCALL) */
#define AVR_ISA_MUL   0x0040 /* device has new core (MUL, MOVW, ...) */
#define AVR_ISA_ELPM  0x0080 /* device has >64K program memory (ELPM) */
#define AVR_ISA_ELPMX 0x0100 /* device has ELPM Rd,Z[+] */
#define AVR_ISA_SPM   0x0200 /* device can program itself */
#define AVR_ISA_BRK   0x0400 /* device has BREAK (on-chip debug) */
#define AVR_ISA_EIND  0x0800 /* device has >128K program memory (none yet) */

#define AVR_ISA_TINY1 (AVR_ISA_1200 | AVR_ISA_LPM)
#define AVR_ISA_2xxx (AVR_ISA_TINY1 | AVR_ISA_SRAM)
#define AVR_ISA_M8   (AVR_ISA_2xxx | AVR_ISA_MUL | AVR_ISA_LPMX | AVR_ISA_SPM)
#define AVR_ISA_M603 (AVR_ISA_2xxx | AVR_ISA_MEGA)
#define AVR_ISA_M103 (AVR_ISA_M603 | AVR_ISA_ELPM)
#define AVR_ISA_M161 (AVR_ISA_M603 | AVR_ISA_MUL | AVR_ISA_LPMX | AVR_ISA_SPM)
#define AVR_ISA_94K  (AVR_ISA_M603 | AVR_ISA_MUL | AVR_ISA_LPMX)
#define AVR_ISA_M323 (AVR_ISA_M161 | AVR_ISA_BRK)
#define AVR_ISA_M128 (AVR_ISA_M323 | AVR_ISA_ELPM | AVR_ISA_ELPMX)

#define AVR_ISA_ALL   0xFFFF

#define REGISTER_P(x) ((x) == 'r'		\
		       || (x) == 'd'		\
		       || (x) == 'w'		\
		       || (x) == 'a'		\
		       || (x) == 'v')

/* Undefined combination of operands - does the register
   operand overlap with pre-decremented or post-incremented
   pointer register (like ld r31,Z+)?  */
#define AVR_UNDEF_P(x) (((x) & 0xFFED) == 0x91E5 ||		\
  ((x) & 0xFDEF) == 0x91AD || ((x) & 0xFDEF) == 0x91AE ||	\
  ((x) & 0xFDEF) == 0x91C9 || ((x) & 0xFDEF) == 0x91CA ||	\
  ((x) & 0xFDEF) == 0x91E1 || ((x) & 0xFDEF) == 0x91E2)

/* Is this a skip instruction {cpse,sbic,sbis,sbrc,sbrs}?  */
#define AVR_SKIP_P(x) (((x) & 0xFC00) == 0x1000 ||		\
  ((x) & 0xFD00) == 0x9900 || ((x) & 0xFC08) == 0xFC00)

/* Is this `ldd r,b+0' or `std b+0,r' (b={Y,Z}, disassembled as
   `ld r,b' or `st b,r' respectively - next opcode entry)?  */
#define AVR_DISP0_P(x) (((x) & 0xFC07) == 0x8000)

/* constraint letters
   r - any register
   d - `ldi' register (r16-r31)
   v - `movw' even register (r0, r2, ..., r28, r30)
   a - `fmul' register (r16-r23)
   w - `adiw' register (r24,r26,r28,r30)
   e - pointer registers (X,Y,Z)
   b - base pointer register and displacement ([YZ]+disp)
   z - Z pointer register (for [e]lpm Rd,Z[+])
   M - immediate value from 0 to 255
   n - immediate value from 0 to 255 ( n = ~M ). Relocation impossible
   s - immediate value from 0 to 7
   P - Port address value from 0 to 63. (in, out)
   p - Port address value from 0 to 31. (cbi, sbi, sbic, sbis)
   K - immediate value from 0 to 63 (used in `adiw', `sbiw')
   i - immediate value
   l - signed pc relative offset from -64 to 63
   L - signed pc relative offset from -2048 to 2047
   h - absolute code address (call, jmp)
   S - immediate value from 0 to 7 (S = s << 4)
   ? - use this opcode entry if no parameters, else use next opcode entry

   Order is important - some binary opcodes have more than one name,
   the disassembler will only see the first match.

   Remaining undefined opcodes (1699 total - some of them might work
   as normal instructions if not all of the bits are decoded):

    0x0001...0x00ff    (255) (known to be decoded as `nop' by the old core)
   "100100xxxxxxx011"  (128) 0x9[0-3][0-9a-f][3b]
   "100100xxxxxx1000"   (64) 0x9[0-3][0-9a-f]8
   "1001001xxxxx01xx"  (128) 0x9[23][0-9a-f][4-7]
   "1001010xxxxx0100"   (32) 0x9[45][0-9a-f]4
   "1001010x001x1001"    (4) 0x9[45][23]9
   "1001010x01xx1001"    (8) 0x9[45][4-7]9
   "1001010x1xxx1001"   (16) 0x9[45][8-9a-f]9
   "1001010xxxxx1011"   (32) 0x9[45][0-9a-f]b
   "10010101001x1000"    (2) 0x95[23]8
   "1001010101xx1000"    (4) 0x95[4-7]8
   "1001010110111000"    (1) 0x95b8
   "1001010111111000"    (1) 0x95f8 (`espm' removed in databook update)
   "11111xxxxxxx1xxx" (1024) 0xf[8-9a-f][0-9a-f][8-9a-f]
 */

AVR_INSN (clc,  "",    "1001010010001000", 1, AVR_ISA_1200, 0x9488)
AVR_INSN (clh,  "",    "1001010011011000", 1, AVR_ISA_1200, 0x94d8)
AVR_INSN (cli,  "",    "1001010011111000", 1, AVR_ISA_1200, 0x94f8)
AVR_INSN (cln,  "",    "1001010010101000", 1, AVR_ISA_1200, 0x94a8)
AVR_INSN (cls,  "",    "1001010011001000", 1, AVR_ISA_1200, 0x94c8)
AVR_INSN (clt,  "",    "1001010011101000", 1, AVR_ISA_1200, 0x94e8)
AVR_INSN (clv,  "",    "1001010010111000", 1, AVR_ISA_1200, 0x94b8)
AVR_INSN (clz,  "",    "1001010010011000", 1, AVR_ISA_1200, 0x9498)

AVR_INSN (sec,  "",    "1001010000001000", 1, AVR_ISA_1200, 0x9408)
AVR_INSN (seh,  "",    "1001010001011000", 1, AVR_ISA_1200, 0x9458)
AVR_INSN (sei,  "",    "1001010001111000", 1, AVR_ISA_1200, 0x9478)
AVR_INSN (sen,  "",    "1001010000101000", 1, AVR_ISA_1200, 0x9428)
AVR_INSN (ses,  "",    "1001010001001000", 1, AVR_ISA_1200, 0x9448)
AVR_INSN (set,  "",    "1001010001101000", 1, AVR_ISA_1200, 0x9468)
AVR_INSN (sev,  "",    "1001010000111000", 1, AVR_ISA_1200, 0x9438)
AVR_INSN (sez,  "",    "1001010000011000", 1, AVR_ISA_1200, 0x9418)

   /* Same as {cl,se}[chinstvz] above.  */
AVR_INSN (bclr, "S",   "100101001SSS1000", 1, AVR_ISA_1200, 0x9488)
AVR_INSN (bset, "S",   "100101000SSS1000", 1, AVR_ISA_1200, 0x9408)

AVR_INSN (icall,"",    "1001010100001001", 1, AVR_ISA_2xxx, 0x9509)
AVR_INSN (ijmp, "",    "1001010000001001", 1, AVR_ISA_2xxx, 0x9409)

AVR_INSN (lpm,  "?",   "1001010111001000", 1, AVR_ISA_TINY1,0x95c8)
AVR_INSN (lpm,  "r,z", "1001000ddddd010+", 1, AVR_ISA_LPMX, 0x9004)
AVR_INSN (elpm, "?",   "1001010111011000", 1, AVR_ISA_ELPM, 0x95d8)
AVR_INSN (elpm, "r,z", "1001000ddddd011+", 1, AVR_ISA_ELPMX,0x9006)

AVR_INSN (nop,  "",    "0000000000000000", 1, AVR_ISA_1200, 0x0000)
AVR_INSN (ret,  "",    "1001010100001000", 1, AVR_ISA_1200, 0x9508)
AVR_INSN (reti, "",    "1001010100011000", 1, AVR_ISA_1200, 0x9518)
AVR_INSN (sleep,"",    "1001010110001000", 1, AVR_ISA_1200, 0x9588)
AVR_INSN (break,"",    "1001010110011000", 1, AVR_ISA_BRK,  0x9598)
AVR_INSN (wdr,  "",    "1001010110101000", 1, AVR_ISA_1200, 0x95a8)
AVR_INSN (spm,  "",    "1001010111101000", 1, AVR_ISA_SPM,  0x95e8)

AVR_INSN (adc,  "r,r", "000111rdddddrrrr", 1, AVR_ISA_1200, 0x1c00)
AVR_INSN (add,  "r,r", "000011rdddddrrrr", 1, AVR_ISA_1200, 0x0c00)
AVR_INSN (and,  "r,r", "001000rdddddrrrr", 1, AVR_ISA_1200, 0x2000)
AVR_INSN (cp,   "r,r", "000101rdddddrrrr", 1, AVR_ISA_1200, 0x1400)
AVR_INSN (cpc,  "r,r", "000001rdddddrrrr", 1, AVR_ISA_1200, 0x0400)
AVR_INSN (cpse, "r,r", "000100rdddddrrrr", 1, AVR_ISA_1200, 0x1000)
AVR_INSN (eor,  "r,r", "001001rdddddrrrr", 1, AVR_ISA_1200, 0x2400)
AVR_INSN (mov,  "r,r", "001011rdddddrrrr", 1, AVR_ISA_1200, 0x2c00)
AVR_INSN (mul,  "r,r", "100111rdddddrrrr", 1, AVR_ISA_MUL,  0x9c00)
AVR_INSN (or,   "r,r", "001010rdddddrrrr", 1, AVR_ISA_1200, 0x2800)
AVR_INSN (sbc,  "r,r", "000010rdddddrrrr", 1, AVR_ISA_1200, 0x0800)
AVR_INSN (sub,  "r,r", "000110rdddddrrrr", 1, AVR_ISA_1200, 0x1800)

   /* Shorthand for {eor,add,adc,and} r,r above.  */
AVR_INSN (clr,  "r=r", "001001rdddddrrrr", 1, AVR_ISA_1200, 0x2400)
AVR_INSN (lsl,  "r=r", "000011rdddddrrrr", 1, AVR_ISA_1200, 0x0c00)
AVR_INSN (rol,  "r=r", "000111rdddddrrrr", 1, AVR_ISA_1200, 0x1c00)
AVR_INSN (tst,  "r=r", "001000rdddddrrrr", 1, AVR_ISA_1200, 0x2000)

AVR_INSN (andi, "d,M", "0111KKKKddddKKKK", 1, AVR_ISA_1200, 0x7000)
  /*XXX special case*/
AVR_INSN (cbr,  "d,n", "0111KKKKddddKKKK", 1, AVR_ISA_1200, 0x7000)

AVR_INSN (ldi,  "d,M", "1110KKKKddddKKKK", 1, AVR_ISA_1200, 0xe000)
AVR_INSN (ser,  "d",   "11101111dddd1111", 1, AVR_ISA_1200, 0xef0f)

AVR_INSN (ori,  "d,M", "0110KKKKddddKKKK", 1, AVR_ISA_1200, 0x6000)
AVR_INSN (sbr,  "d,M", "0110KKKKddddKKKK", 1, AVR_ISA_1200, 0x6000)

AVR_INSN (cpi,  "d,M", "0011KKKKddddKKKK", 1, AVR_ISA_1200, 0x3000)
AVR_INSN (sbci, "d,M", "0100KKKKddddKKKK", 1, AVR_ISA_1200, 0x4000)
AVR_INSN (subi, "d,M", "0101KKKKddddKKKK", 1, AVR_ISA_1200, 0x5000)

AVR_INSN (sbrc, "r,s", "1111110rrrrr0sss", 1, AVR_ISA_1200, 0xfc00)
AVR_INSN (sbrs, "r,s", "1111111rrrrr0sss", 1, AVR_ISA_1200, 0xfe00)
AVR_INSN (bld,  "r,s", "1111100ddddd0sss", 1, AVR_ISA_1200, 0xf800)
AVR_INSN (bst,  "r,s", "1111101ddddd0sss", 1, AVR_ISA_1200, 0xfa00)

AVR_INSN (in,   "r,P", "10110PPdddddPPPP", 1, AVR_ISA_1200, 0xb000)
AVR_INSN (out,  "P,r", "10111PPrrrrrPPPP", 1, AVR_ISA_1200, 0xb800)

AVR_INSN (adiw, "w,K", "10010110KKddKKKK", 1, AVR_ISA_2xxx, 0x9600)
AVR_INSN (sbiw, "w,K", "10010111KKddKKKK", 1, AVR_ISA_2xxx, 0x9700)

AVR_INSN (cbi,  "p,s", "10011000pppppsss", 1, AVR_ISA_1200, 0x9800)
AVR_INSN (sbi,  "p,s", "10011010pppppsss", 1, AVR_ISA_1200, 0x9a00)
AVR_INSN (sbic, "p,s", "10011001pppppsss", 1, AVR_ISA_1200, 0x9900)
AVR_INSN (sbis, "p,s", "10011011pppppsss", 1, AVR_ISA_1200, 0x9b00)

AVR_INSN (brcc, "l",   "111101lllllll000", 1, AVR_ISA_1200, 0xf400)
AVR_INSN (brcs, "l",   "111100lllllll000", 1, AVR_ISA_1200, 0xf000)
AVR_INSN (breq, "l",   "111100lllllll001", 1, AVR_ISA_1200, 0xf001)
AVR_INSN (brge, "l",   "111101lllllll100", 1, AVR_ISA_1200, 0xf404)
AVR_INSN (brhc, "l",   "111101lllllll101", 1, AVR_ISA_1200, 0xf405)
AVR_INSN (brhs, "l",   "111100lllllll101", 1, AVR_ISA_1200, 0xf005)
AVR_INSN (brid, "l",   "111101lllllll111", 1, AVR_ISA_1200, 0xf407)
AVR_INSN (brie, "l",   "111100lllllll111", 1, AVR_ISA_1200, 0xf007)
AVR_INSN (brlo, "l",   "111100lllllll000", 1, AVR_ISA_1200, 0xf000)
AVR_INSN (brlt, "l",   "111100lllllll100", 1, AVR_ISA_1200, 0xf004)
AVR_INSN (brmi, "l",   "111100lllllll010", 1, AVR_ISA_1200, 0xf002)
AVR_INSN (brne, "l",   "111101lllllll001", 1, AVR_ISA_1200, 0xf401)
AVR_INSN (brpl, "l",   "111101lllllll010", 1, AVR_ISA_1200, 0xf402)
AVR_INSN (brsh, "l",   "111101lllllll000", 1, AVR_ISA_1200, 0xf400)
AVR_INSN (brtc, "l",   "111101lllllll110", 1, AVR_ISA_1200, 0xf406)
AVR_INSN (brts, "l",   "111100lllllll110", 1, AVR_ISA_1200, 0xf006)
AVR_INSN (brvc, "l",   "111101lllllll011", 1, AVR_ISA_1200, 0xf403)
AVR_INSN (brvs, "l",   "111100lllllll011", 1, AVR_ISA_1200, 0xf003)

   /* Same as br?? above.  */
AVR_INSN (brbc, "s,l", "111101lllllllsss", 1, AVR_ISA_1200, 0xf400)
AVR_INSN (brbs, "s,l", "111100lllllllsss", 1, AVR_ISA_1200, 0xf000)

AVR_INSN (rcall, "L",  "1101LLLLLLLLLLLL", 1, AVR_ISA_1200, 0xd000)
AVR_INSN (rjmp,  "L",  "1100LLLLLLLLLLLL", 1, AVR_ISA_1200, 0xc000)

AVR_INSN (call, "h",   "1001010hhhhh111h", 2, AVR_ISA_MEGA, 0x940e)
AVR_INSN (jmp,  "h",   "1001010hhhhh110h", 2, AVR_ISA_MEGA, 0x940c)

AVR_INSN (asr,  "r",   "1001010rrrrr0101", 1, AVR_ISA_1200, 0x9405)
AVR_INSN (com,  "r",   "1001010rrrrr0000", 1, AVR_ISA_1200, 0x9400)
AVR_INSN (dec,  "r",   "1001010rrrrr1010", 1, AVR_ISA_1200, 0x940a)
AVR_INSN (inc,  "r",   "1001010rrrrr0011", 1, AVR_ISA_1200, 0x9403)
AVR_INSN (lsr,  "r",   "1001010rrrrr0110", 1, AVR_ISA_1200, 0x9406)
AVR_INSN (neg,  "r",   "1001010rrrrr0001", 1, AVR_ISA_1200, 0x9401)
AVR_INSN (pop,  "r",   "1001000rrrrr1111", 1, AVR_ISA_2xxx, 0x900f)
AVR_INSN (push, "r",   "1001001rrrrr1111", 1, AVR_ISA_2xxx, 0x920f)
AVR_INSN (ror,  "r",   "1001010rrrrr0111", 1, AVR_ISA_1200, 0x9407)
AVR_INSN (swap, "r",   "1001010rrrrr0010", 1, AVR_ISA_1200, 0x9402)

   /* Known to be decoded as `nop' by the old core.  */
AVR_INSN (movw, "v,v", "00000001ddddrrrr", 1, AVR_ISA_MUL,  0x0100)
AVR_INSN (muls, "d,d", "00000010ddddrrrr", 1, AVR_ISA_MUL,  0x0200)
AVR_INSN (mulsu,"a,a", "000000110ddd0rrr", 1, AVR_ISA_MUL,  0x0300)
AVR_INSN (fmul, "a,a", "000000110ddd1rrr", 1, AVR_ISA_MUL,  0x0308)
AVR_INSN (fmuls,"a,a", "000000111ddd0rrr", 1, AVR_ISA_MUL,  0x0380)
AVR_INSN (fmulsu,"a,a","000000111ddd1rrr", 1, AVR_ISA_MUL,  0x0388)

AVR_INSN (sts,  "i,r", "1001001ddddd0000", 2, AVR_ISA_2xxx, 0x9200)
AVR_INSN (lds,  "r,i", "1001000ddddd0000", 2, AVR_ISA_2xxx, 0x9000)

   /* Special case for b+0, `e' must be next entry after `b',
      b={Y=1,Z=0}, ee={X=11,Y=10,Z=00}, !=1 if -e or e+ or X.  */
AVR_INSN (ldd,  "r,b", "10o0oo0dddddbooo", 1, AVR_ISA_2xxx, 0x8000)
AVR_INSN (ld,   "r,e", "100!000dddddee-+", 1, AVR_ISA_1200, 0x8000)
AVR_INSN (std,  "b,r", "10o0oo1rrrrrbooo", 1, AVR_ISA_2xxx, 0x8200)
AVR_INSN (st,   "e,r", "100!001rrrrree-+", 1, AVR_ISA_1200, 0x8200)

   /* These are for devices that don't exist yet
      (>128K program memory, PC = EIND:Z).  */
AVR_INSN (eicall, "",  "1001010100011001", 1, AVR_ISA_EIND, 0x9519)
AVR_INSN (eijmp, "",   "1001010000011001", 1, AVR_ISA_EIND, 0x9419)

