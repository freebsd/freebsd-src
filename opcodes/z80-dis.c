/* Print Z80 and R800 instructions
   Copyright 2005 Free Software Foundation, Inc.
   Contributed by Arnold Metselaar <arnold_m@operamail.com>

   This file is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street - Fifth Floor, Boston,
   MA 02110-1301, USA.  */

#include "sysdep.h"
#include "dis-asm.h"
#include <stdio.h>

struct buffer
{
  bfd_vma base;
  int n_fetch;
  int n_used;
  signed char data[4];
} ;

typedef int (*func)(struct buffer *, disassemble_info *, char *);

struct tab_elt
{
  unsigned char val;
  unsigned char mask;
  func          fp;
  char *        text;
} ;

#define TXTSIZ 24
/* Names of 16-bit registers.  */
static char * rr_str[] = { "bc", "de", "hl", "sp" };
/* Names of 8-bit registers.  */
static char * r_str[]  = { "b", "c", "d", "e", "h", "l", "(hl)", "a" };
/* Texts for condition codes.  */
static char * cc_str[] = { "nz", "z", "nc", "c", "po", "pe", "p", "m" };
/* Instruction names for 8-bit arithmetic, operand "a" is often implicit */
static char * arit_str[] =
{
  "add a,", "adc a,", "sub ", "sbc a,", "and ", "xor ", "or ", "cp "
} ;

static int
fetch_data (struct buffer *buf, disassemble_info * info, int n)
{
  int r;

  if (buf->n_fetch + n > 4)
    abort ();

  r = info->read_memory_func (buf->base + buf->n_fetch,
			      (unsigned char*) buf->data + buf->n_fetch,
			      n, info);
  if (r == 0)
    buf->n_fetch += n;
  return !r;
}

static int
prt (struct buffer *buf, disassemble_info * info, char *txt)
{
  info->fprintf_func (info->stream, "%s", txt);
  buf->n_used = buf->n_fetch;
  return 1;
}

static int
prt_e (struct buffer *buf, disassemble_info * info, char *txt)
{
  char e;
  int target_addr;

  if (fetch_data (buf, info, 1))
    {
      e = buf->data[1];
      target_addr = (buf->base + 2 + e) & 0xffff;
      buf->n_used = buf->n_fetch;
      info->fprintf_func (info->stream, "%s0x%04x", txt, target_addr);
    }
  else
    buf->n_used = -1;

  return buf->n_used;
}

static int
jr_cc (struct buffer *buf, disassemble_info * info, char *txt)
{
  char mytxt[TXTSIZ];

  snprintf (mytxt, TXTSIZ, txt, cc_str[(buf->data[0] >> 3) & 3]);
  return prt_e (buf, info, mytxt);
}

static int
prt_nn (struct buffer *buf, disassemble_info * info, char *txt)
{
  int nn;
  unsigned char *p;

  p = (unsigned char*) buf->data + buf->n_fetch;
  if (fetch_data (buf, info, 2))
    {
      nn = p[0] + (p[1] << 8);
      info->fprintf_func (info->stream, txt, nn);
      buf->n_used = buf->n_fetch;
    }
  else
    buf->n_used = -1;
  return buf->n_used;
}

static int
prt_rr_nn (struct buffer *buf, disassemble_info * info, char *txt)
{
  char mytxt[TXTSIZ];

  snprintf (mytxt, TXTSIZ, txt, rr_str[(buf->data[0] >> 4) & 3]);
  return prt_nn (buf, info, mytxt);
}

static int
prt_rr (struct buffer *buf, disassemble_info * info, char *txt)
{
  info->fprintf_func (info->stream, "%s%s", txt,
		      rr_str[(buf->data[buf->n_fetch - 1] >> 4) & 3]);
  buf->n_used = buf->n_fetch;
  return buf->n_used;
}

static int
prt_n (struct buffer *buf, disassemble_info * info, char *txt)
{
  int n;
  unsigned char *p;

  p = (unsigned char*) buf->data + buf->n_fetch;

  if (fetch_data (buf, info, 1))
    {
      n = p[0];
      info->fprintf_func (info->stream, txt, n);
      buf->n_used = buf->n_fetch;
    }
  else
    buf->n_used = -1;

  return buf->n_used;
}

static int
ld_r_n (struct buffer *buf, disassemble_info * info, char *txt)
{
  char mytxt[TXTSIZ];

  snprintf (mytxt, TXTSIZ, txt, r_str[(buf->data[0] >> 3) & 7]);
  return prt_n (buf, info, mytxt);
}

static int
prt_r (struct buffer *buf, disassemble_info * info, char *txt)
{
  info->fprintf_func (info->stream, txt,
		      r_str[(buf->data[buf->n_fetch - 1] >> 3) & 7]);
  buf->n_used = buf->n_fetch;
  return buf->n_used;
}

static int
ld_r_r (struct buffer *buf, disassemble_info * info, char *txt)
{
  info->fprintf_func (info->stream, txt,
		      r_str[(buf->data[buf->n_fetch - 1] >> 3) & 7],
		      r_str[buf->data[buf->n_fetch - 1] & 7]);
  buf->n_used = buf->n_fetch;
  return buf->n_used;
}

static int
arit_r (struct buffer *buf, disassemble_info * info, char *txt)
{
  info->fprintf_func (info->stream, txt,
		      arit_str[(buf->data[buf->n_fetch - 1] >> 3) & 7],
		      r_str[buf->data[buf->n_fetch - 1] & 7]);
  buf->n_used = buf->n_fetch;
  return buf->n_used;
}

static int
prt_cc (struct buffer *buf, disassemble_info * info, char *txt)
{
  info->fprintf_func (info->stream, "%s%s", txt,
		      cc_str[(buf->data[0] >> 3) & 7]);
  buf->n_used = buf->n_fetch;
  return buf->n_used;
}

static int
pop_rr (struct buffer *buf, disassemble_info * info, char *txt)
{
  static char *rr_stack[] = { "bc","de","hl","af"};

  info->fprintf_func (info->stream, "%s %s", txt,
		      rr_stack[(buf->data[0] >> 4) & 3]);
  buf->n_used = buf->n_fetch;
  return buf->n_used;
}


static int
jp_cc_nn (struct buffer *buf, disassemble_info * info, char *txt)
{
  char mytxt[TXTSIZ];

  snprintf (mytxt,TXTSIZ,
	    "%s%s,0x%%04x", txt, cc_str[(buf->data[0] >> 3) & 7]);
  return prt_nn (buf, info, mytxt);
}

static int
arit_n (struct buffer *buf, disassemble_info * info, char *txt)
{
  char mytxt[TXTSIZ];

  snprintf (mytxt,TXTSIZ, txt, arit_str[(buf->data[0] >> 3) & 7]);
  return prt_n (buf, info, mytxt);
}

static int
rst (struct buffer *buf, disassemble_info * info, char *txt)
{
  info->fprintf_func (info->stream, txt, buf->data[0] & 0x38);
  buf->n_used = buf->n_fetch;
  return buf->n_used;
}


static int
cis (struct buffer *buf, disassemble_info * info, char *txt ATTRIBUTE_UNUSED)
{
  static char * opar[] = { "ld", "cp", "in", "out" };
  char * op;
  char c;

  c = buf->data[1];
  op = ((0x13 & c) == 0x13) ? "ot" : (opar[c & 3]);
  info->fprintf_func (info->stream,
		      "%s%c%s", op,
		      (c & 0x08) ? 'd' : 'i',
		      (c & 0x10) ? "r" : "");
  buf->n_used = 2;
  return buf->n_used;
}

static int
dump (struct buffer *buf, disassemble_info * info, char *txt)
{
  int i;

  info->fprintf_func (info->stream, "defb ");
  for (i = 0; txt[i]; ++i)
    info->fprintf_func (info->stream, i ? ", 0x%02x" : "0x%02x",
			(unsigned char) buf->data[i]);
  buf->n_used = i;
  return buf->n_used;
}

/* Table to disassemble machine codes with prefix 0xED.  */
struct tab_elt opc_ed[] =
{
  { 0x70, 0xFF, prt, "in f,(c)" },
  { 0x70, 0xFF, dump, "xx" },
  { 0x40, 0xC7, prt_r, "in %s,(c)" },
  { 0x71, 0xFF, prt, "out (c),0" },
  { 0x70, 0xFF, dump, "xx" },
  { 0x41, 0xC7, prt_r, "out (c),%s" },
  { 0x42, 0xCF, prt_rr, "sbc hl," },
  { 0x43, 0xCF, prt_rr_nn, "ld (0x%%04x),%s" },
  { 0x44, 0xFF, prt, "neg" },
  { 0x45, 0xFF, prt, "retn" },
  { 0x46, 0xFF, prt, "im 0" },
  { 0x47, 0xFF, prt, "ld i,a" },
  { 0x4A, 0xCF, prt_rr, "adc hl," },
  { 0x4B, 0xCF, prt_rr_nn, "ld %s,(0x%%04x)" },
  { 0x4D, 0xFF, prt, "reti" },
  { 0x56, 0xFF, prt, "im 1" },
  { 0x57, 0xFF, prt, "ld a,i" },
  { 0x5E, 0xFF, prt, "im 2" },
  { 0x67, 0xFF, prt, "rrd" },
  { 0x6F, 0xFF, prt, "rld" },
  { 0xA0, 0xE4, cis, "" },
  { 0xC3, 0xFF, prt, "muluw hl,bc" },
  { 0xC5, 0xE7, prt_r, "mulub a,%s" },
  { 0xF3, 0xFF, prt, "muluw hl,sp" },
  { 0x00, 0x00, dump, "xx" }
};

static int
pref_ed (struct buffer * buf, disassemble_info * info, 
	 char* txt ATTRIBUTE_UNUSED)
{
  struct tab_elt *p;

  if (fetch_data(buf, info, 1))
    {
      for (p = opc_ed; p->val != (buf->data[1] & p->mask); ++p)
	;
      p->fp (buf, info, p->text);
    }
  else
    buf->n_used = -1;

  return buf->n_used;
}

/* Instruction names for the instructions addressing single bits.  */
static char *cb1_str[] = { "", "bit", "res", "set"};
/* Instruction names for shifts and rotates.  */
static char *cb2_str[] =
{
  "rlc", "rrc", "rl", "rr", "sla", "sra", "sli", "srl"
};

static int
pref_cb (struct buffer * buf, disassemble_info * info,
	 char* txt ATTRIBUTE_UNUSED)
{
  if (fetch_data (buf, info, 1))
    {
      buf->n_used = 2;
      if ((buf->data[1] & 0xc0) == 0)
	info->fprintf_func (info->stream, "%s %s",
			    cb2_str[(buf->data[1] >> 3) & 7],
			    r_str[buf->data[1] & 7]);
      else
	info->fprintf_func (info->stream, "%s %d,%s",
			    cb1_str[(buf->data[1] >> 6) & 3],
			    (buf->data[1] >> 3) & 7,
			    r_str[buf->data[1] & 7]);
    }
  else
    buf->n_used = -1;

  return buf->n_used;
}

static int
addvv (struct buffer * buf, disassemble_info * info, char* txt)
{
  info->fprintf_func (info->stream, "add %s,%s", txt, txt);

  return buf->n_used = buf->n_fetch;
}

static int
ld_v_v (struct buffer * buf, disassemble_info * info, char* txt)
{
  char mytxt[TXTSIZ];

  snprintf (mytxt, TXTSIZ, "ld %s%%s,%s%%s", txt, txt);
  return ld_r_r (buf, info, mytxt);
}

static int
prt_d (struct buffer *buf, disassemble_info * info, char *txt)
{
  int d;
  signed char *p;

  p = buf->data + buf->n_fetch;

  if (fetch_data (buf, info, 1))
    {
      d = p[0];
      info->fprintf_func (info->stream, txt, d);
      buf->n_used = buf->n_fetch;
    }
  else
    buf->n_used = -1;

  return buf->n_used;
}

static int
prt_d_n (struct buffer *buf, disassemble_info * info, char *txt)
{
  char mytxt[TXTSIZ];
  int d;
  signed char *p;

  p = buf->data + buf->n_fetch;

  if (fetch_data (buf, info, 1))
    {
      d = p[0];
      snprintf (mytxt, TXTSIZ, txt, d);
      return prt_n (buf, info, mytxt);
    }
  else
    buf->n_used = -1;

  return buf->n_used;
}

static int
arit_d (struct buffer *buf, disassemble_info * info, char *txt)
{
  char mytxt[TXTSIZ];
  signed char c;

  c = buf->data[buf->n_fetch - 1];
  snprintf (mytxt, TXTSIZ, txt, arit_str[(c >> 3) & 7]);
  return prt_d (buf, info, mytxt);
}

static int
ld_r_d (struct buffer *buf, disassemble_info * info, char *txt)
{
  char mytxt[TXTSIZ];
  signed char c;

  c = buf->data[buf->n_fetch - 1];
  snprintf (mytxt, TXTSIZ, txt, r_str[(c >> 3) & 7]);
  return prt_d (buf, info, mytxt);
}

static int
ld_d_r(struct buffer *buf, disassemble_info * info, char *txt)
{
  char mytxt[TXTSIZ];
  signed char c;

  c = buf->data[buf->n_fetch - 1];
  snprintf (mytxt, TXTSIZ, txt, r_str[c & 7]);
  return prt_d (buf, info, mytxt);
}

static int
pref_xd_cb (struct buffer * buf, disassemble_info * info, char* txt)
{
  if (fetch_data (buf, info, 2))
    {
      int d;
      char arg[TXTSIZ];
      signed char *p;

      buf->n_used = 4;
      p = buf->data;
      d = p[2];

      if (((p[3] & 0xC0) == 0x40) || ((p[3] & 7) == 0x06))
	snprintf (arg, TXTSIZ, "(%s%+d)", txt, d);
      else
	snprintf (arg, TXTSIZ, "(%s%+d),%s", txt, d, r_str[p[3] & 7]);

      if ((p[3] & 0xc0) == 0)
	info->fprintf_func (info->stream, "%s %s",
			    cb2_str[(buf->data[3] >> 3) & 7],
			    arg);
      else
	info->fprintf_func (info->stream, "%s %d,%s",
			    cb1_str[(buf->data[3] >> 6) & 3],
			    (buf->data[3] >> 3) & 7,
			    arg);
    }
  else
    buf->n_used = -1;

  return buf->n_used;
}

/* Table to disassemble machine codes with prefix 0xDD or 0xFD.  */
static struct tab_elt opc_ind[] =
{
  { 0x24, 0xF7, prt_r, "inc %s%%s" },
  { 0x25, 0xF7, prt_r, "dec %s%%s" },
  { 0x26, 0xF7, ld_r_n, "ld %s%%s,0x%%%%02x" },
  { 0x21, 0xFF, prt_nn, "ld %s,0x%%04x" },
  { 0x22, 0xFF, prt_nn, "ld (0x%%04x),%s" },
  { 0x2A, 0xFF, prt_nn, "ld %s,(0x%%04x)" },
  { 0x23, 0xFF, prt, "inc %s" },
  { 0x2B, 0xFF, prt, "dec %s" },
  { 0x29, 0xFF, addvv, "%s" },
  { 0x09, 0xCF, prt_rr, "add %s," },
  { 0x34, 0xFF, prt_d, "inc (%s%%+d)" },
  { 0x35, 0xFF, prt_d, "dec (%s%%+d)" },
  { 0x36, 0xFF, prt_d_n, "ld (%s%%+d),0x%%%%02x" },

  { 0x76, 0xFF, dump, "h" },
  { 0x46, 0xC7, ld_r_d, "ld %%s,(%s%%%%+d)" },
  { 0x70, 0xF8, ld_d_r, "ld (%s%%%%+d),%%s" },
  { 0x64, 0xF6, ld_v_v, "%s" },
  { 0x60, 0xF0, ld_r_r, "ld %s%%s,%%s" },
  { 0x44, 0xC6, ld_r_r, "ld %%s,%s%%s" },

  { 0x86, 0xC7, arit_d, "%%s(%s%%%%+d)" },
  { 0x84, 0xC6, arit_r, "%%s%s%%s" },

  { 0xE1, 0xFF, prt, "pop %s" },
  { 0xE5, 0xFF, prt, "push %s" },
  { 0xCB, 0xFF, pref_xd_cb, "%s" },
  { 0xE3, 0xFF, prt, "ex (sp),%s" },
  { 0xE9, 0xFF, prt, "jp (%s)" },
  { 0xF9, 0xFF, prt, "ld sp,%s" },
  { 0x00, 0x00, dump, "?" },
} ;

static int
pref_ind (struct buffer * buf, disassemble_info * info, char* txt)
{
  if (fetch_data (buf, info, 1))
    {
      char mytxt[TXTSIZ];
      struct tab_elt *p;

      for (p = opc_ind; p->val != (buf->data[1] & p->mask); ++p)
	;
      snprintf (mytxt, TXTSIZ, p->text, txt);
      p->fp (buf, info, mytxt);
    }
  else
    buf->n_used = -1;

  return buf->n_used;
}

/* Table to disassemble machine codes without prefix.  */
static struct tab_elt opc_main[] =
{
  { 0x00, 0xFF, prt, "nop" },
  { 0x01, 0xCF, prt_rr_nn, "ld %s,0x%%04x" },
  { 0x02, 0xFF, prt, "ld (bc),a" },
  { 0x03, 0xCF, prt_rr, "inc " },
  { 0x04, 0xC7, prt_r, "inc %s" },
  { 0x05, 0xC7, prt_r, "dec %s" },
  { 0x06, 0xC7, ld_r_n, "ld %s,0x%%02x" },
  { 0x07, 0xFF, prt, "rlca" },
  { 0x08, 0xFF, prt, "ex af,af'" },
  { 0x09, 0xCF, prt_rr, "add hl," },
  { 0x0A, 0xFF, prt, "ld a,(bc)" },
  { 0x0B, 0xCF, prt_rr, "dec " },
  { 0x0F, 0xFF, prt, "rrca" },
  { 0x10, 0xFF, prt_e, "djnz " },
  { 0x12, 0xFF, prt, "ld (de),a" },
  { 0x17, 0xFF, prt, "rla" },
  { 0x18, 0xFF, prt_e, "jr "},
  { 0x1A, 0xFF, prt, "ld a,(de)" },
  { 0x1F, 0xFF, prt, "rra" },
  { 0x20, 0xE7, jr_cc, "jr %s,"},
  { 0x22, 0xFF, prt_nn, "ld (0x%04x),hl" },
  { 0x27, 0xFF, prt, "daa"},
  { 0x2A, 0xFF, prt_nn, "ld hl,(0x%04x)" },
  { 0x2F, 0xFF, prt, "cpl" },
  { 0x32, 0xFF, prt_nn, "ld (0x%04x),a" },
  { 0x37, 0xFF, prt, "scf" },
  { 0x3A, 0xFF, prt_nn, "ld a,(0x%04x)" },
  { 0x3F, 0xFF, prt, "ccf" },

  { 0x76, 0xFF, prt, "halt" },
  { 0x40, 0xC0, ld_r_r, "ld %s,%s"},

  { 0x80, 0xC0, arit_r, "%s%s" },

  { 0xC0, 0xC7, prt_cc, "ret " },
  { 0xC1, 0xCF, pop_rr, "pop" },
  { 0xC2, 0xC7, jp_cc_nn, "jp " },
  { 0xC3, 0xFF, prt_nn, "jp 0x%04x" },
  { 0xC4, 0xC7, jp_cc_nn, "call " },
  { 0xC5, 0xCF, pop_rr, "push" },
  { 0xC6, 0xC7, arit_n, "%s0x%%02x" },
  { 0xC7, 0xC7, rst, "rst 0x%02x" },
  { 0xC9, 0xFF, prt, "ret" },
  { 0xCB, 0xFF, pref_cb, "" },
  { 0xCD, 0xFF, prt_nn, "call 0x%04x" },
  { 0xD3, 0xFF, prt_n, "out (0x%02x),a" },
  { 0xD9, 0xFF, prt, "exx" },
  { 0xDB, 0xFF, prt_n, "in a,(0x%02x)" },
  { 0xDD, 0xFF, pref_ind, "ix" },
  { 0xE3, 0xFF, prt, "ex (sp),hl" },
  { 0xE9, 0xFF, prt, "jp (hl)" },
  { 0xEB, 0xFF, prt, "ex de,hl" },
  { 0xED, 0xFF, pref_ed, ""},
  { 0xF3, 0xFF, prt, "di" },
  { 0xF9, 0xFF, prt, "ld sp,hl" },
  { 0xFB, 0xFF, prt, "ei" },
  { 0xFD, 0xFF, pref_ind, "iy" },
  { 0x00, 0x00, prt, "????" },
} ;

int
print_insn_z80 (bfd_vma addr, disassemble_info * info)
{
  struct buffer buf;
  struct tab_elt *p;

  buf.base = addr;
  buf.n_fetch = 0;
  buf.n_used = 0;

  if (! fetch_data (& buf, info, 1))
    return -1;

  for (p = opc_main; p->val != (buf.data[0] & p->mask); ++p)
    ;
  p->fp (& buf, info, p->text);

  return buf.n_used;
}
