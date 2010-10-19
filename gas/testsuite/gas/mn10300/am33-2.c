/* Copyright (C) 2000, 2002 Free Software Foundation
 * Contributed by Alexandre Oliva <aoliva@redhat.com>
 *
 * This file is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street - Fifth Floor, Boston, MA 02110-1301, USA.
 */

/* Generator of tests for insns introduced in AM33 2.0.  */

#define INSN_REPEAT 11

/* See the following file for usage and documentation.  */
#include "../all/test-gen.c"

/* These are the AM33 registers.  */
const char *am33_regs[] = {
  /* These are the canonical names, i.e., those printed by the
   * disassembler.  */
  "r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7",
  "a0", "a1", "a2", "a3", "d0", "d1", "d2", "d3",
  /* These are aliases that the assembler should also recognize.  */
  "e0", "e1", "e2", "e3", "e4", "e5", "e6", "e7",
  "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15"
};

/* Signed constants of the given sizes.  */
#define  d8(shift) signed_constant( 8, shift, 1)
#define d16(shift) signed_constant(16, shift, 1)
#define d24(shift) signed_constant(24, shift, 1)
#define d32(shift) signed_constant(32, shift, 1)
#define  u8(shift) unsigned_constant( 8, shift, 1)
#define u24(shift) unsigned_constant(24, shift, 1)
#define a16(shift) absolute_address(16, shift, 1)

/* Emit an AM33 register shifted by these many words. */
#define amreg(shift) reg_r (am33_regs, shift, 15, mk_get_bits (5u))
#define spreg literal ("sp")
#define fcreg literal ("fpcr")

/* Emit an AM33-2 FP single-precision register, with the 4 least
 * significant bits shifted by shiftlow and the most significant bit
 * shifted by shifthigh.  */
int
freg (func_arg *arg, insn_data *data)
#define freg(shiftlow, shifthigh) { freg, { i1: shiftlow, i2: shifthigh } }
{
  unsigned val = get_bits (5u);

  data->as_in = data->dis_out = (char*)malloc (3 + ulen (val, 10));
  sprintf (data->as_in, "fs%u", val);
  data->bits = val;
  data->bits = ((data->bits & 15) << arg->i1) | ((data->bits >> 4) << arg->i2);

  return 0;
}

/* Emit an AM33-2 FP single-precision register in the ``accumulator''
 * range, with the 2 least significant bits shifted by shiftlow and
 * the most significant bit shifted by shifthigh. */
int
areg (func_arg *arg, insn_data *data)
#define areg(shiftlow, shifthigh) { areg, { i1: shiftlow, i2: shifthigh } }
{
  unsigned val = get_bits (3u);

  data->as_in = data->dis_out = (char*)malloc (4);
  sprintf (data->as_in, "fs%u", val);
  data->bits = val;
  data->bits = ((data->bits & 3) << arg->i1) | ((data->bits >> 2) << arg->i2);

  return 0;
}

/* Emit an AM33-2 FP double-precision register, with the 4 least
 * significant bits shifted by shiftlow and the most significant bit
 * shifted by shifthigh. */
int
dreg (func_arg *arg, insn_data *data)
#define dreg(shiftlow, shifthigh) { dreg, { i1: shiftlow, i2: shifthigh } }
{
  unsigned val = 2 * get_bits (4u);

  data->as_in = data->dis_out = (char*)malloc (3 + ulen (val, 10));
  sprintf (data->as_in, "fd%u", val);
  data->bits = val;
  data->bits = ((data->bits & 15) << arg->i1) | ((data->bits >> 4) << arg->i2);

  return 0;
}

/* Emit a signed 8-bit PC-relative offset from the current insn to the
 * last emitted label.  */
int
d8pcoff (func_arg *arg, insn_data *data)
#define  d8pcoff(shift) { d8pcoff, { p1: shift } }
{
  int diff = insn_size - arg->i1/8 - 1;
  int displacement = current_offset - last_label_offset;
  char *current_address = malloc (strlen (last_label_name) + 4
				  + ulen (displacement, 16) + 1);

  /* Make sure we're not too far from the target.  */
  if (displacement > 128)
    abort ();

  data->as_in = strdup (last_label_name);

  /* Calculate the address that will be printed by the disassembler as
     the target of the jump.  Since it won't take relocations into
     account, it will be the insn's own address.  */
  if (current_offset == last_label_offset)
    strcpy (current_address, last_label_name);
  else
    sprintf (current_address, "%s\\+0x%x", last_label_name, displacement);

  /* Compute the complete label, including the relocation message
     printed as an additional message.  The relocation will point us
     to the intended target label plus an offset equal to the offset
     of the displacement within the current insn.  We do not account
     for the case in which this displacement is zero, since it doesn't
     come up on this platform. */
  data->dis_out = malloc (8 + 2 + strlen (current_address) + 2
			  + 3 + ulen (current_offset + diff, 16) + 19
			  + strlen (last_label_name) + 4
			  + ulen (diff, 16) + 1);
  sprintf (data->dis_out, "0*%x <%s>\n"
	   "\t\t\t%x: R_MN10300_PCREL8\t%s\\+0x%x",
	   current_offset, current_address,
	   current_offset + diff, last_label_name, diff);

  free (current_address);

  return 0;
}

/* Emit a signed 8-bit PC-relative offset from the current insn to the
 * current section.  */
int
d8pcsec (func_arg *arg, insn_data *data)
#define  d8pcsec(shift) { d8pcsec, { p1: shift } }
{
  int diff = insn_size - arg->i1/8 - 1;
  int displacement = current_offset - last_label_offset;
  char *current_address = malloc (strlen (last_label_name) + 4
				  + ulen (displacement, 16) + 1);

  /* Make sure we're not too far from the target.  */
  if (displacement > 128)
    abort ();

  data->as_in = strdup (last_label_name);

  /* Calculate the address that will be printed by the disassembler as
     the target of the jump.  Since it won't take relocations into
     account, it will be the insn's own address.  */

  if (current_offset == last_label_offset)
    strcpy (current_address, last_label_name);
  else
    sprintf (current_address, "%s\\+0x%x", last_label_name, displacement);


  /* Compute the complete label, including the relocation message
     printed as an additional message.  The relocation will point us
     to the intended target label plus an offset equal to the offset
     of the displacement within the current insn.  We do not account
     for the case in which this displacement is zero, since it doesn't
     come up on this platform. */
  data->dis_out = malloc (8 + 2 + strlen (current_address) + 2
			  + 3 + ulen (current_offset + diff, 16) + 33);
  sprintf (data->dis_out, "0*%x <%s>\n"
	   "\t\t\t%x: R_MN10300_PCREL8\tcondjmp\\+0x2",
	   current_offset, current_address,
	   current_offset + diff);

  free (current_address);

  return 0;
}

/* Convenience wrapper to define_insn.  */
#define def_am_insn(insname, variant, size, word, funcs...) \
  define_insn(insname ## _ ## variant, \
	      insn_size_bits (insname, size, \
			      ((unsigned long long)word) << 8*(size-2)), \
	      tab, \
	      ## funcs)
#define am_insn(insname, variant) insn (insname ## _ ## variant)

#define def_bit_insn(insname, word) \
  def_am_insn (insname, i8a16, 5, word, \
	       u8(0), comma, lparen, a16 (8), rparen, tick_random);
#define bit_insn(insname) insn (insname ## _ ## i8a16)

/* Data cache pre-fetch insns.  */
def_am_insn (dcpf, r,    3, 0xf9a6, lparen, amreg (4), rparen);
def_am_insn (dcpf, sp,   3, 0xf9a7, lparen, spreg, rparen);
def_am_insn (dcpf, rr,   4, 0xfba6,
	     lparen, amreg(12), comma, amreg (8), rparen, tick_random);
def_am_insn (dcpf, d8r,  4, 0xfba7,
	     lparen, d8 (0), comma, amreg (12), rparen, tick_random);
def_am_insn (dcpf, d24r, 6, 0xfda7,
	     lparen, d24(0), comma, amreg (28), rparen, tick_random);
def_am_insn (dcpf, d32r, 7, 0xfe46,
	     lparen, d32(0), comma, amreg (36), rparen, tick_random);

/* Define the group of data cache pre-fetch insns.  */
func *dcpf_insns[] = {
  am_insn (dcpf, r),
  am_insn (dcpf, sp),
  am_insn (dcpf, rr),
  am_insn (dcpf, d8r),
  am_insn (dcpf, d24r),
  am_insn (dcpf, d32r),
  0
};

/* Bit operations.  */
def_bit_insn (bset, 0xfe80);
def_bit_insn (bclr, 0xfe81);
def_bit_insn (btst, 0xfe82);

/* Define the group of bit insns.  */
func *bit_insns[] = {
  bit_insn (bset),
  bit_insn (bclr),
  bit_insn (btst),
  0
};

/* Define the single-precision FP move insns.  */
def_am_insn (fmov, irfs, 3, 0xf920,
	     lparen, amreg (4), rparen, comma,
	     freg (0, 8), tick_random);
def_am_insn (fmov, rpfs, 3, 0xf922,
	     lparen, amreg (4), plus, rparen, comma,
	     freg (0, 8), tick_random);
def_am_insn (fmov, spfs, 3, 0xf924,
	     lparen, spreg, rparen, comma, freg (0, 8));
def_am_insn (fmov, vrfs, 3, 0xf926,
	     amreg (4), comma, freg (0, 8), tick_random);
def_am_insn (fmov, fsir, 3, 0xf930,
	     freg (4, 9), comma, lparen, amreg (0), rparen, tick_random);
def_am_insn (fmov, fsrp, 3, 0xf931,
	     freg (4, 9), comma, lparen, amreg (0), plus, rparen, tick_random);
def_am_insn (fmov, fssp, 3, 0xf934,
	     freg (4, 9), comma, lparen, spreg, rparen);
def_am_insn (fmov, fsvr, 3, 0xf935,
	     freg (4, 9), comma, amreg (0), tick_random);
def_am_insn (fmov, fsfs, 3, 0xf940,
	     freg (4, 9), comma, freg (0, 8), tick_random);
def_am_insn (fmov, d8rfs, 4, 0xfb20,
	     lparen, d8 (0), comma, amreg (12), rparen, comma,
	     freg (8, 16));
def_am_insn (fmov, rpi8fs, 4, 0xfb22,
	     lparen, amreg (12), plus, comma, d8 (0), rparen, comma,
	     freg (8, 16));
def_am_insn (fmov, d8spfs, 4, 0xfb24,
	     lparen, u8 (0), comma, spreg, rparen, comma, freg (8, 16),
	     tick_random);
def_am_insn (fmov, irrfs, 4, 0xfb27,
	     lparen, amreg (12), comma, amreg (8), rparen, comma,
	     freg (4, 1));
def_am_insn (fmov, fsd8r, 4, 0xfb30,
	     freg (12, 17), comma, lparen, d8 (0), comma, amreg (8), rparen);
def_am_insn (fmov, fsrpi8, 4, 0xfb31,
	     freg (12, 17), comma,
	     lparen, amreg (8), plus, comma, d8 (0), rparen);
def_am_insn (fmov, fsd8sp, 4, 0xfb34,
	     freg (12, 17), comma,
	     lparen, u8 (0), comma, spreg, rparen, tick_random);
def_am_insn (fmov, fsirr, 4, 0xfb37,
	     freg (4, 1), comma,
	     lparen, amreg (12), comma, amreg (8), rparen);
def_am_insn (fmov, d24rfs, 6, 0xfd20,
	     lparen, d24 (0), comma, amreg (28), rparen, comma, freg (24, 32));
def_am_insn (fmov, rpi24fs, 6, 0xfd22,
	     lparen, amreg (28), plus, comma, d24 (0), rparen, comma,
	     freg (24, 32));
def_am_insn (fmov, d24spfs, 6, 0xfd24,
	     lparen, u24 (0), comma, spreg, rparen, comma,
	     freg (24, 32), tick_random);
def_am_insn (fmov, fsd24r, 6, 0xfd30,
	     freg (28, 33), comma, lparen, d24 (0), comma, amreg (24), rparen);
def_am_insn (fmov, fsrpi24, 6, 0xfd31,
	     freg (28, 33), comma,
	     lparen, amreg (24), plus, comma, d24 (0), rparen);
def_am_insn (fmov, fsd24sp, 6, 0xfd34,
	     freg (28, 33), comma,
	     lparen, u24 (0), comma, spreg, rparen, tick_random);
def_am_insn (fmov, d32rfs, 7, 0xfe20,
	     lparen, d32 (0), comma, amreg (36), rparen, comma, freg (32, 40));
def_am_insn (fmov, rpi32fs, 7, 0xfe22,
	     lparen, amreg (36), plus, comma, d32 (0), rparen, comma,
	     freg (32, 40));
def_am_insn (fmov, d32spfs, 7, 0xfe24,
	     lparen, d32 (0), comma, spreg, rparen, comma,
	     freg (32, 40), tick_random);
def_am_insn (fmov, i32fs, 7, 0xfe26,
	     d32 (0), comma, freg (32, 40), tick_random);
def_am_insn (fmov, fsd32r, 7, 0xfe30,
	     freg (36, 41), comma, lparen, d32 (0), comma, amreg (32), rparen);
def_am_insn (fmov, fsrpi32, 7, 0xfe31,
	     freg (36, 41), comma,
	     lparen, amreg (32), plus, comma, d32 (0), rparen);
def_am_insn (fmov, fsd32sp, 7, 0xfe34,
	     freg (36, 41), comma,
	     lparen, d32 (0), comma, spreg, rparen, tick_random);

/* Define the group of single-precision FP move insns.  */
func *fmovs_insns[] = {
  am_insn (fmov, irfs),
  am_insn (fmov, rpfs),
  am_insn (fmov, spfs),
  am_insn (fmov, vrfs),
  am_insn (fmov, fsir),
  am_insn (fmov, fsrp),
  am_insn (fmov, fssp),
  am_insn (fmov, fsvr),
  am_insn (fmov, fsfs),
  am_insn (fmov, d8rfs),
  am_insn (fmov, rpi8fs),
  am_insn (fmov, d8spfs),
  am_insn (fmov, irrfs),
  am_insn (fmov, fsd8r),
  am_insn (fmov, fsrpi8),
  am_insn (fmov, fsd8sp),
  am_insn (fmov, fsirr),
  am_insn (fmov, d24rfs),
  am_insn (fmov, rpi24fs),
  am_insn (fmov, d24spfs),
  am_insn (fmov, fsd24r),
  am_insn (fmov, fsrpi24),
  am_insn (fmov, fsd24sp),
  am_insn (fmov, d32rfs),
  am_insn (fmov, rpi32fs),
  am_insn (fmov, d32spfs),
  am_insn (fmov, i32fs),
  am_insn (fmov, fsd32r),
  am_insn (fmov, fsrpi32),
  am_insn (fmov, fsd32sp),
  0
};

/* Define the double-precision FP move insns.  */
def_am_insn (fmov, irfd, 3, 0xf9a0,
	     lparen, amreg (4), rparen, comma, dreg (0, 8), tick_random);
def_am_insn (fmov, rpfd, 3, 0xf9a2,
	     lparen, amreg (4), plus, rparen, comma, dreg (0, 8), tick_random);
def_am_insn (fmov, spfd, 3, 0xf9a4,
	     lparen, spreg, rparen, comma, dreg (0, 8));
def_am_insn (fmov, fdir, 3, 0xf9b0,
	     dreg (4, 9), comma, lparen, amreg (0), rparen, tick_random);
def_am_insn (fmov, fdrp, 3, 0xf9b1,
	     dreg (4, 9), comma, lparen, amreg (0), plus, rparen, tick_random);
def_am_insn (fmov, fdsp, 3, 0xf9b4,
	     dreg (4, 9), comma, lparen, spreg, rparen);
def_am_insn (fmov, fdfd, 3, 0xf9c0,
	     dreg (4, 9), comma, dreg (0, 8), tick_random);
def_am_insn (fmov, irrfd, 4, 0xfb47,
	     lparen, amreg (12), comma, amreg (8), rparen, comma, dreg (4, 1));
def_am_insn (fmov, fdirr, 4, 0xfb57,
	     dreg (4, 1), comma, lparen, amreg (12), comma, amreg (8), rparen);
def_am_insn (fmov, d8rfd, 4, 0xfba0,
	     lparen, d8 (0), comma, amreg (12), rparen, comma, dreg (8, 16));
def_am_insn (fmov, rpi8fd, 4, 0xfba2,
	     lparen, amreg (12), plus, comma, d8 (0), rparen, comma,
	     dreg (8, 16));
def_am_insn (fmov, d8spfd, 4, 0xfba4,
	     lparen, u8 (0), comma, spreg, rparen, comma,
	     dreg (8, 16), tick_random);
def_am_insn (fmov, fdd8r, 4, 0xfbb0,
	     dreg (12, 17), comma, lparen, d8 (0), comma, amreg (8), rparen);
def_am_insn (fmov, fdrpi8, 4, 0xfbb1,
	     dreg (12, 17), comma,
	     lparen, amreg (8), plus, comma, d8 (0), rparen);
def_am_insn (fmov, fdi8sp, 4, 0xfbb4,
	     dreg (12, 17), comma,
	     lparen, u8 (0), comma, spreg, rparen, tick_random);
def_am_insn (fmov, d24rfd, 6, 0xfda0,
	     lparen, d24 (0), comma, amreg (28), rparen, comma, dreg (24, 32));
def_am_insn (fmov, rpi24fd, 6, 0xfda2,
	     lparen, amreg (28), plus, comma, d24 (0), rparen, comma,
	     dreg (24, 32));
def_am_insn (fmov, d24spfd, 6, 0xfda4,
	     lparen, u24 (0), comma, spreg, rparen, comma,
	     dreg (24, 32), tick_random);
def_am_insn (fmov, fdd24r, 6, 0xfdb0,
	     dreg (28, 33), comma,
	     lparen, d24 (0), comma, amreg (24), rparen);
def_am_insn (fmov, fdrpi24, 6, 0xfdb1,
	     dreg (28, 33), comma,
	     lparen, amreg (24), plus, comma, d24 (0), rparen);
def_am_insn (fmov, fdd24sp, 6, 0xfdb4,
	     dreg (28, 33), comma,
	     lparen, u24 (0), comma, spreg, rparen, tick_random);
def_am_insn (fmov, d32rfd, 7, 0xfe40,
	     lparen, d32 (0), comma, amreg (36), rparen, comma, dreg (32, 40));
def_am_insn (fmov, rpi32fd, 7, 0xfe42,
	     lparen, amreg (36), plus, comma, d32 (0), rparen, comma,
	     dreg (32, 40));
def_am_insn (fmov, d32spfd, 7, 0xfe44,
	     lparen, d32 (0), comma, spreg, rparen, comma,
	     dreg (32, 40), tick_random);
def_am_insn (fmov, fdd32r, 7, 0xfe50,
	     dreg (36, 41), comma,
	     lparen, d32 (0), comma, amreg (32), rparen);
def_am_insn (fmov, fdrpi32, 7, 0xfe51,
	     dreg (36, 41), comma,
	     lparen, amreg (32), plus, comma, d32 (0), rparen);
def_am_insn (fmov, fdd32sp, 7, 0xfe54,
	     dreg (36, 41), comma,
	     lparen, d32 (0), comma, spreg, rparen, tick_random);

/* Define the group of double-precision FP move insns.  */
func *fmovd_insns[] = {
  am_insn (fmov, irfd),
  am_insn (fmov, rpfd),
  am_insn (fmov, spfd),
  am_insn (fmov, fdir),
  am_insn (fmov, fdrp),
  am_insn (fmov, fdsp),
  am_insn (fmov, fdfd),
  am_insn (fmov, irrfd),
  am_insn (fmov, fdirr),
  am_insn (fmov, d8rfd),
  am_insn (fmov, rpi8fd),
  am_insn (fmov, d8spfd),
  am_insn (fmov, fdd8r),
  am_insn (fmov, fdrpi8),
  am_insn (fmov, fdi8sp),
  am_insn (fmov, d24rfd),
  am_insn (fmov, rpi24fd),
  am_insn (fmov, d24spfd),
  am_insn (fmov, fdd24r),
  am_insn (fmov, fdrpi24),
  am_insn (fmov, fdd24sp),
  am_insn (fmov, d32rfd),
  am_insn (fmov, rpi32fd),
  am_insn (fmov, d32spfd),
  am_insn (fmov, fdd32r),
  am_insn (fmov, fdrpi32),
  am_insn (fmov, fdd32sp),
  0
};

/* Define fmov FPCR insns.  */
def_am_insn (fmov, vrfc, 3, 0xf9b5,
	     amreg (4), comma, fcreg);
def_am_insn (fmov, fcvr, 3, 0xf9b7,
	     fcreg, comma, amreg (0));
def_am_insn (fmov, i32fc, 6, 0xfdb5,
	     d32 (0), comma, fcreg);

/* Define the group of FPCR move insns.  */
func *fmovc_insns[] = {
  am_insn (fmov, vrfc),
  am_insn (fmov, fcvr),
  am_insn (fmov, i32fc),
  0
};

/* Define single-precision floating-point arithmetic insns.  */
def_am_insn (fabs, fs, 3, 0xf944, freg (0, 8));
def_am_insn (fabs, fsfs, 4, 0xfb44,
	     freg (12, 3), comma, freg (4, 1), tick_random);
def_am_insn (fneg, fs, 3, 0xf946, freg (0, 8));
def_am_insn (fneg, fsfs, 4, 0xfb46,
	     freg (12, 3), comma, freg (4, 1), tick_random);
def_am_insn (frsqrt, fs, 3, 0xf950, freg (0, 8));
def_am_insn (frsqrt, fsfs, 4, 0xfb50,
	     freg (12, 3), comma, freg (4, 1), tick_random);
def_am_insn (fsqrt, fs, 3, 0xf952, freg (0, 8));
def_am_insn (fsqrt, fsfs, 4, 0xfb54,
	     freg (12, 3), comma, freg (4, 1), tick_random);
def_am_insn (fcmp, fsfs, 3, 0xf954,
	     freg (4, 9), comma, freg (0, 8), tick_random);
def_am_insn (fcmp, i32fs, 7, 0xfe35,
	     d32 (0), comma, freg (36, 41), tick_random);
def_am_insn (fadd, fsfs, 3, 0xf960,
	     freg (4, 9), comma, freg (0, 8), tick_random);
def_am_insn (fadd, fsfsfs, 4, 0xfb60,
	     freg (12, 3), comma, freg (8, 2), comma, freg (4, 1));
def_am_insn (fadd, i32fsfs, 7, 0xfe60,
	     d32 (0), comma, freg (36, 41), comma, freg (32, 40));
def_am_insn (fsub, fsfs, 3, 0xf964,
	     freg (4, 9), comma, freg (0, 8), tick_random);
def_am_insn (fsub, fsfsfs, 4, 0xfb64,
	     freg (12, 3), comma, freg (8, 2), comma, freg (4, 1));
def_am_insn (fsub, i32fsfs, 7, 0xfe64,
	     d32 (0), comma, freg (36, 41), comma, freg (32, 40));
def_am_insn (fmul, fsfs, 3, 0xf970,
	     freg (4, 9), comma, freg (0, 8), tick_random);
def_am_insn (fmul, fsfsfs, 4, 0xfb70,
	     freg (12, 3), comma, freg (8, 2), comma, freg (4, 1));
def_am_insn (fmul, i32fsfs, 7, 0xfe70,
	     d32 (0), comma, freg (36, 41), comma, freg (32, 40));
def_am_insn (fdiv, fsfs, 3, 0xf974,
	     freg (4, 9), comma, freg (0, 8), tick_random);
def_am_insn (fdiv, fsfsfs, 4, 0xfb74,
	     freg (12, 3), comma, freg (8, 2), comma, freg (4, 1));
def_am_insn (fdiv, i32fsfs, 7, 0xfe74,
	     d32 (0), comma, freg (36, 41), comma, freg (32, 40));

/* Define the group of single-precision floating-point arithmetic insns.  */
func *sfparith_insns[] = {
  am_insn (fabs, fs),
  am_insn (fabs, fsfs),
  am_insn (fneg, fs),
  am_insn (fneg, fsfs),
  am_insn (frsqrt, fs),
  am_insn (frsqrt, fsfs),
  am_insn (fsqrt, fs),
  am_insn (fsqrt, fsfs),
  am_insn (fcmp, fsfs),
  am_insn (fcmp, i32fs),
  am_insn (fadd, fsfs),
  am_insn (fadd, fsfsfs),
  am_insn (fadd, i32fsfs),
  am_insn (fsub, fsfs),
  am_insn (fsub, fsfsfs),
  am_insn (fsub, i32fsfs),
  am_insn (fmul, fsfs),
  am_insn (fmul, fsfsfs),
  am_insn (fmul, i32fsfs),
  am_insn (fdiv, fsfs),
  am_insn (fdiv, fsfsfs),
  am_insn (fdiv, i32fsfs),
  0
};

/* Define floating-point accumulator arithmetic insns.  */
def_am_insn (fmadd, , 4, 0xfb80,
	     freg (12, 3), comma, freg (8, 2), comma,
	     freg (4, 1), comma, areg (16, 0), tick_random);
def_am_insn (fmsub, , 4, 0xfb84,
	     freg (12, 3), comma, freg (8, 2), comma,
	     freg (4, 1), comma, areg (16, 0), tick_random);
def_am_insn (fnmadd, , 4, 0xfb90,
	     freg (12, 3), comma, freg (8, 2), comma,
	     freg (4, 1), comma, areg (16, 0), tick_random);
def_am_insn (fnmsub, , 4, 0xfb94,
	     freg (12, 3), comma, freg (8, 2), comma,
	     freg (4, 1), comma, areg (16, 0), tick_random);

/* Define the group of floating-point accumulator arithmetic insns.  */
func *fpacc_insns[] = {
  am_insn (fmadd, ),
  am_insn (fmsub, ),
  am_insn (fnmadd, ),
  am_insn (fnmsub, ),
  0
};

/* Define double-precision floating-point arithmetic insns.  */
def_am_insn (fabs, fd, 3, 0xf9c4, dreg (0, 8));
def_am_insn (fabs, fdfd, 4, 0xfbc4,
	     dreg (12, 3), comma, dreg (4, 1), tick_random);
def_am_insn (fneg, fd, 3, 0xf9c6, dreg (0, 8));
def_am_insn (fneg, fdfd, 4, 0xfbc6,
	     dreg (12, 3), comma, dreg (4, 1), tick_random);
def_am_insn (frsqrt, fd, 3, 0xf9d0, dreg (0, 8));
def_am_insn (frsqrt, fdfd, 4, 0xfbd0,
	     dreg (12, 3), comma, dreg (4, 1), tick_random);
def_am_insn (fsqrt, fd, 3, 0xf9d2, dreg (0, 8));
def_am_insn (fsqrt, fdfd, 4, 0xfbd4,
	     dreg (12, 3), comma, dreg (4, 1), tick_random);
def_am_insn (fcmp, fdfd, 3, 0xf9d4,
	     dreg (4, 9), comma, dreg (0, 8), tick_random);
def_am_insn (fadd, fdfd, 3, 0xf9e0,
	     dreg (4, 9), comma, dreg (0, 8), tick_random);
def_am_insn (fadd, fdfdfd, 4, 0xfbe0,
	     dreg (12, 3), comma, dreg (8, 2), comma, dreg (4, 1));
def_am_insn (fsub, fdfd, 3, 0xf9e4,
	     dreg (4, 9), comma, dreg (0, 8), tick_random);
def_am_insn (fsub, fdfdfd, 4, 0xfbe4,
	     dreg (12, 3), comma, dreg (8, 2), comma, dreg (4, 1));
def_am_insn (fmul, fdfd, 3, 0xf9f0,
	     dreg (4, 9), comma, dreg (0, 8), tick_random);
def_am_insn (fmul, fdfdfd, 4, 0xfbf0,
	     dreg (12, 3), comma, dreg (8, 2), comma, dreg (4, 1));
def_am_insn (fdiv, fdfd, 3, 0xf9f4,
	     dreg (4, 9), comma, dreg (0, 8), tick_random);
def_am_insn (fdiv, fdfdfd, 4, 0xfbf4,
	     dreg (12, 3), comma, dreg (8, 2), comma, dreg (4, 1));

/* Define the group of double-precision floating-point arithmetic insns.  */
func *dfparith_insns[] = {
  am_insn (fabs, fd),
  am_insn (fabs, fdfd),
  am_insn (fneg, fd),
  am_insn (fneg, fdfd),
  am_insn (frsqrt, fd),
  am_insn (frsqrt, fdfd),
  am_insn (fsqrt, fd),
  am_insn (fsqrt, fdfd),
  am_insn (fcmp, fdfd),
  am_insn (fadd, fdfd),
  am_insn (fadd, fdfdfd),
  am_insn (fsub, fdfd),
  am_insn (fsub, fdfdfd),
  am_insn (fmul, fdfd),
  am_insn (fmul, fdfdfd),
  am_insn (fdiv, fdfd),
  am_insn (fdiv, fdfdfd),
  0
};

/* Define floating-point conversion insns.  */
def_am_insn (ftoi, fsfs, 4, 0xfb40,
	     freg (12, 3), comma, freg (4, 1), tick_random);
def_am_insn (itof, fsfs, 4, 0xfb42,
	     freg (12, 3), comma, freg (4, 1), tick_random);
def_am_insn (ftod, fsfd, 4, 0xfb52,
	     freg (12, 3), comma, dreg (4, 1), tick_random);
def_am_insn (dtof, fdfs, 4, 0xfb56,
	     dreg (12, 3), comma, freg (4, 1), tick_random);

/* Define the group of floating-point conversion insns.  */
func *fpconv_insns[] = {
  am_insn (ftoi, fsfs),
  am_insn (itof, fsfs),
  am_insn (ftod, fsfd),
  am_insn (dtof, fdfs),
  0
};

/* Define conditional jump insns.  */
def_am_insn (fbeq, , 3, 0xf8d0, d8pcsec (0));
def_am_insn (fbne, , 3, 0xf8d1, d8pcsec (0));
def_am_insn (fbgt, , 3, 0xf8d2, d8pcsec (0));
def_am_insn (fbge, , 3, 0xf8d3, d8pcsec (0));
def_am_insn (fblt, , 3, 0xf8d4, d8pcsec (0));
def_am_insn (fble, , 3, 0xf8d5, d8pcsec (0));
def_am_insn (fbuo, , 3, 0xf8d6, d8pcsec (0));
def_am_insn (fblg, , 3, 0xf8d7, d8pcsec (0));
def_am_insn (fbleg,, 3, 0xf8d8, d8pcsec (0));
def_am_insn (fbug, , 3, 0xf8d9, d8pcsec (0));
def_am_insn (fbuge,, 3, 0xf8da, d8pcsec (0));
def_am_insn (fbul, , 3, 0xf8db, d8pcsec (0));
def_am_insn (fbule,, 3, 0xf8dc, d8pcsec (0));
def_am_insn (fbue, , 3, 0xf8dd, d8pcsec (0));
def_am_insn (fleq, , 2, 0xf0d0, nothing);
def_am_insn (flne, , 2, 0xf0d1, nothing);
def_am_insn (flgt, , 2, 0xf0d2, nothing);
def_am_insn (flge, , 2, 0xf0d3, nothing);
def_am_insn (fllt, , 2, 0xf0d4, nothing);
def_am_insn (flle, , 2, 0xf0d5, nothing);
def_am_insn (fluo, , 2, 0xf0d6, nothing);
def_am_insn (fllg, , 2, 0xf0d7, nothing);
def_am_insn (flleg,, 2, 0xf0d8, nothing);
def_am_insn (flug, , 2, 0xf0d9, nothing);
def_am_insn (fluge,, 2, 0xf0da, nothing);
def_am_insn (flul, , 2, 0xf0db, nothing);
def_am_insn (flule,, 2, 0xf0dc, nothing);
def_am_insn (flue, , 2, 0xf0dd, nothing);

/* Define the group of conditional jump insns.  */
func *condjmp_insns[] = {
  am_insn (fbeq, ),
  am_insn (fbne, ),
  am_insn (fbgt, ),
  am_insn (fbge, ),
  am_insn (fblt, ),
  am_insn (fble, ),
  am_insn (fbuo, ),
  am_insn (fblg, ),
  am_insn (fbleg, ),
  am_insn (fbug, ),
  am_insn (fbuge, ),
  am_insn (fbul, ),
  am_insn (fbule, ),
  am_insn (fbue, ),
  am_insn (fleq, ),
  am_insn (flne, ),
  am_insn (flgt, ),
  am_insn (flge, ),
  am_insn (fllt, ),
  am_insn (flle, ),
  am_insn (fluo, ),
  am_insn (fllg, ),
  am_insn (flleg, ),
  am_insn (flug, ),
  am_insn (fluge, ),
  am_insn (flul, ),
  am_insn (flule, ),
  am_insn (flue, ),
  0
};

/* Define the set of all groups.  */
group_t
groups[] = {
  { "dcpf", dcpf_insns },
  { "bit", bit_insns },
  { "fmovs", fmovs_insns },
  { "fmovd", fmovd_insns },
  { "fmovc", fmovc_insns },
  { "sfparith", sfparith_insns },
  { "fpacc", fpacc_insns },
  { "dfparith", dfparith_insns },
  { "fpconv", fpconv_insns },
  { "condjmp", condjmp_insns },
  { 0 }
};

int
main(int argc, char *argv[])
{
  FILE *as_in = stdout, *dis_out = stderr;

  /* Check whether we're filtering insns.  */
  if (argc > 1)
    skip_list = argv + 1;

  /* Output assembler header.  */
  fputs ("\t.text\n"
	 "\t.am33_2\n",
	 as_in);
  /* Output comments for the testsuite-driver and the initial
   * disassembler output. */
  fputs ("#objdump: -dr --prefix-address --show-raw-insn\n"
	 "#name: AM33/2.0\n"
	 "\n"
	 ".*: +file format.*elf32-mn10300.*\n"
	 "\n"
	 "Disassembly of section .text:\n",
	 dis_out);

  /* Now emit all (selected) insns.  */
  output_groups (groups, as_in, dis_out);

  exit (0);
}
