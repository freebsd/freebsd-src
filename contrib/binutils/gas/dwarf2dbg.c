/* dwarf2dbg.c - DWARF2 debug support
   Copyright 1999, 2000, 2001 Free Software Foundation, Inc.
   Contributed by David Mosberger-Tang <davidm@hpl.hp.com>

   This file is part of GAS, the GNU Assembler.

   GAS is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   GAS is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with GAS; see the file COPYING.  If not, write to the Free
   Software Foundation, 59 Temple Place - Suite 330, Boston, MA
   02111-1307, USA.  */

/* Logical line numbers can be controlled by the compiler via the
   following two directives:

	.file FILENO "file.c"
	.loc  FILENO LINENO [COLUMN]

   FILENO is the filenumber.  */

#include "ansidecl.h"
#include "as.h"

#ifdef HAVE_LIMITS_H
#include <limits.h>
#else
#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif
#ifndef INT_MAX
#define INT_MAX (int) (((unsigned) (-1)) >> 1)
#endif
#endif

#ifdef BFD_ASSEMBLER

#include "dwarf2dbg.h"
#include "subsegs.h"

#include "elf/dwarf2.h"

/* Since we can't generate the prolog until the body is complete, we
   use three different subsegments for .debug_line: one holding the
   prolog, one for the directory and filename info, and one for the
   body ("statement program").  */
#define DL_PROLOG	0
#define DL_FILES	1
#define DL_BODY		2

/* First special line opcde - leave room for the standard opcodes.
   Note: If you want to change this, you'll have to update the
   "standard_opcode_lengths" table that is emitted below in
   dwarf2_finish().  */
#define DWARF2_LINE_OPCODE_BASE		10

#ifndef DWARF2_LINE_BASE
  /* Minimum line offset in a special line info. opcode.  This value
     was chosen to give a reasonable range of values.  */
# define DWARF2_LINE_BASE		-5
#endif

/* Range of line offsets in a special line info. opcode.  */
#ifndef DWARF2_LINE_RANGE
# define DWARF2_LINE_RANGE		14
#endif

#ifndef DWARF2_LINE_MIN_INSN_LENGTH
  /* Define the architecture-dependent minimum instruction length (in
     bytes).  This value should be rather too small than too big.  */
# define DWARF2_LINE_MIN_INSN_LENGTH	1
#endif

/* Flag that indicates the initial value of the is_stmt_start flag.
   In the present implementation, we do not mark any lines as
   the beginning of a source statement, because that information
   is not made available by the GCC front-end.  */
#define	DWARF2_LINE_DEFAULT_IS_STMT	1

/* Given a special op, return the line skip amount.  */
#define SPECIAL_LINE(op) \
	(((op) - DWARF2_LINE_OPCODE_BASE)%DWARF2_LINE_RANGE + DWARF2_LINE_BASE)

/* Given a special op, return the address skip amount (in units of
   DWARF2_LINE_MIN_INSN_LENGTH.  */
#define SPECIAL_ADDR(op) (((op) - DWARF2_LINE_OPCODE_BASE)/DWARF2_LINE_RANGE)

/* The maximum address skip amount that can be encoded with a special op.  */
#define MAX_SPECIAL_ADDR_DELTA		SPECIAL_ADDR(255)

struct line_entry {
  struct line_entry *next;
  fragS *frag;
  addressT frag_ofs;
  struct dwarf2_line_info loc;
};

struct line_subseg {
  struct line_subseg *next;
  subsegT subseg;
  struct line_entry *head;
  struct line_entry **ptail;
};

struct line_seg {
  struct line_seg *next;
  segT seg;
  struct line_subseg *head;
  symbolS *text_start;
  symbolS *text_end;
};

/* Collects data for all line table entries during assembly.  */
static struct line_seg *all_segs;

struct file_entry {
  char *filename;
  unsigned int dir;
};

/* Table of files used by .debug_line.  */
static struct file_entry *files;
static unsigned int files_in_use;
static unsigned int files_allocated;

/* True when we've seen a .loc directive recently.  Used to avoid
   doing work when there's nothing to do.  */
static boolean loc_directive_seen;

/* Current location as indicated by the most recent .loc directive.  */
static struct dwarf2_line_info current;

/* Fake label name.  */
static char const fake_label_name[] = ".L0\001";

/* The size of an address on the target.  */
static unsigned int sizeof_address;

static struct line_subseg *get_line_subseg PARAMS ((segT, subsegT));
static unsigned int get_filenum PARAMS ((const char *));
static struct frag *first_frag_for_seg PARAMS ((segT));
static struct frag *last_frag_for_seg PARAMS ((segT));
static void out_byte PARAMS ((int));
static void out_opcode PARAMS ((int));
static void out_two PARAMS ((int));
static void out_four PARAMS ((int));
static void out_abbrev PARAMS ((int, int));
static void out_uleb128 PARAMS ((addressT));
static symbolS *symbol_new_now PARAMS ((void));
static void set_symbol_value_now PARAMS ((symbolS *));
static offsetT get_frag_fix PARAMS ((fragS *));
static void out_set_addr PARAMS ((segT, fragS *, addressT));
static int size_inc_line_addr PARAMS ((int, addressT));
static void emit_inc_line_addr PARAMS ((int, addressT, char *, int));
static void out_inc_line_addr PARAMS ((int, addressT));
static void relax_inc_line_addr PARAMS ((int, segT, fragS *, addressT,
					 fragS *, addressT));
static void process_entries PARAMS ((segT, struct line_entry *));
static void out_file_list PARAMS ((void));
static void out_debug_line PARAMS ((segT));
static void out_debug_aranges PARAMS ((segT, segT));
static void out_debug_abbrev PARAMS ((segT));
static void out_debug_info PARAMS ((segT, segT, segT));

/* Find or create an entry for SEG+SUBSEG in ALL_SEGS.  */

static struct line_subseg *
get_line_subseg (seg, subseg)
     segT seg;
     subsegT subseg;
{
  static segT last_seg;
  static subsegT last_subseg;
  static struct line_subseg *last_line_subseg;

  struct line_seg *s;
  struct line_subseg **pss, *ss;

  if (seg == last_seg && subseg == last_subseg)
    return last_line_subseg;

  for (s = all_segs; s; s = s->next)
    if (s->seg == seg)
      goto found_seg;

  s = (struct line_seg *) xmalloc (sizeof (*s));
  s->next = all_segs;
  s->seg = seg;
  s->head = NULL;
  all_segs = s;

 found_seg:
  for (pss = &s->head; (ss = *pss) != NULL ; pss = &ss->next)
    {
      if (ss->subseg == subseg)
	goto found_subseg;
      if (ss->subseg > subseg)
	break;
    }

  ss = (struct line_subseg *) xmalloc (sizeof (*ss));
  ss->next = *pss;
  ss->subseg = subseg;
  ss->head = NULL;
  ss->ptail = &ss->head;
  *pss = ss;

 found_subseg:
  last_seg = seg;
  last_subseg = subseg;
  last_line_subseg = ss;

  return ss;
}

/* Record an entry for LOC ocurring at OFS within the current fragment.  */

void
dwarf2_gen_line_info (ofs, loc)
     addressT ofs;
     struct dwarf2_line_info *loc;
{
  struct line_subseg *ss;
  struct line_entry *e;
  static unsigned int line = -1;
  static unsigned int filenum = -1;

  /* Early out for as-yet incomplete location information.  */
  if (loc->filenum == 0 || loc->line == 0)
    return;

  /* Don't emit sequences of line symbols for the same line when the
     symbols apply to assembler code.  It is necessary to emit
     duplicate line symbols when a compiler asks for them, because GDB
     uses them to determine the end of the prologue.  */
  if (debug_type == DEBUG_DWARF2
      && line == loc->line && filenum == loc->filenum)
    return;

  line = loc->line;
  filenum = loc->filenum;

  e = (struct line_entry *) xmalloc (sizeof (*e));
  e->next = NULL;
  e->frag = frag_now;
  e->frag_ofs = ofs;
  e->loc = *loc;

  ss = get_line_subseg (now_seg, now_subseg);
  *ss->ptail = e;
  ss->ptail = &e->next;
}

void
dwarf2_where (line)
     struct dwarf2_line_info *line;
{
  if (debug_type == DEBUG_DWARF2)
    {
      char *filename;
      as_where (&filename, &line->line);
      line->filenum = get_filenum (filename);
      line->column = 0;
      line->flags = DWARF2_FLAG_BEGIN_STMT;
    }
  else
    *line = current;
}

/* Called for each machine instruction, or relatively atomic group of
   machine instructions (ie built-in macro).  The instruction or group
   is SIZE bytes in length.  If dwarf2 line number generation is called
   for, emit a line statement appropriately.  */

void
dwarf2_emit_insn (size)
     int size;
{
  struct dwarf2_line_info loc;

  if (loc_directive_seen)
    {
      /* Use the last location established by a .loc directive, not
	 the value returned by dwarf2_where().  That calls as_where()
	 which will return either the logical input file name (foo.c)
	or the physical input file name (foo.s) and not the file name
	specified in the most recent .loc directive (eg foo.h).  */
      loc = current;

      /* Unless we generate DWARF2 debugging information for each
	 assembler line, we only emit one line symbol for one LOC.  */
      if (debug_type != DEBUG_DWARF2)
	loc_directive_seen = false;
    }
  else if (debug_type != DEBUG_DWARF2)
    return;
  else
    dwarf2_where (& loc);

  dwarf2_gen_line_info (frag_now_fix () - size, &loc);
}

/* Get a .debug_line file number for FILENAME.  */

static unsigned int
get_filenum (filename)
     const char *filename;
{
  static unsigned int last_used;
  unsigned int i;

  if (last_used)
    if (strcmp (filename, files[last_used].filename) == 0)
      return last_used;

  for (i = 1; i < files_in_use; ++i)
    if (strcmp (filename, files[i].filename) == 0)
      return i;

  if (i >= files_allocated)
    {
      unsigned int old = files_allocated;

      files_allocated = i + 32;
      files = (struct file_entry *)
	xrealloc (files, (i + 32) * sizeof (struct file_entry));

      memset (files + old, 0, (i + 32 - old) * sizeof (struct file_entry));
    }

  files[i].filename = xstrdup (filename);
  files[i].dir = 0;
  files_in_use = i + 1;
  last_used = i;

  return i;
}

/* Handle two forms of .file directive:
   - Pass .file "source.c" to s_app_file
   - Handle .file 1 "source.c" by adding an entry to the DWARF-2 file table

   If an entry is added to the file table, return a pointer to the filename. */

char *
dwarf2_directive_file (dummy)
     int dummy ATTRIBUTE_UNUSED;
{
  offsetT num;
  char *filename;
  int filename_len;

  /* Continue to accept a bare string and pass it off.  */
  SKIP_WHITESPACE ();
  if (*input_line_pointer == '"')
    {
      s_app_file (0);
      return NULL;
    }

  num = get_absolute_expression ();
  filename = demand_copy_C_string (&filename_len);
  demand_empty_rest_of_line ();

  if (num < 1)
    {
      as_bad (_("file number less than one"));
      return NULL;
    }

  if (num < (int) files_in_use && files[num].filename != 0)
    {
      as_bad (_("file number %ld already allocated"), (long) num);
      return NULL;
    }

  if (num >= (int) files_allocated)
    {
      unsigned int old = files_allocated;

      files_allocated = num + 16;
      files = (struct file_entry *)
	xrealloc (files, (num + 16) * sizeof (struct file_entry));

      /* Zero the new memory.  */
      memset (files + old, 0, (num + 16 - old) * sizeof (struct file_entry));
    }

  files[num].filename = filename;
  files[num].dir = 0;
  files_in_use = num + 1;

  return filename;
}

void
dwarf2_directive_loc (dummy)
     int dummy ATTRIBUTE_UNUSED;
{
  offsetT filenum, line, column;

  filenum = get_absolute_expression ();
  SKIP_WHITESPACE ();
  line = get_absolute_expression ();
  SKIP_WHITESPACE ();
  column = get_absolute_expression ();
  demand_empty_rest_of_line ();

  if (filenum < 1)
    {
      as_bad (_("file number less than one"));
      return;
    }
  if (filenum >= (int) files_in_use || files[filenum].filename == 0)
    {
      as_bad (_("unassigned file number %ld"), (long) filenum);
      return;
    }

  current.filenum = filenum;
  current.line = line;
  current.column = column;
  current.flags = DWARF2_FLAG_BEGIN_STMT;

  loc_directive_seen = true;

#ifndef NO_LISTING
  if (listing)
    {
      listing_source_file (files[filenum].filename);
      listing_source_line (line);
    }
#endif
}

static struct frag *
first_frag_for_seg (seg)
     segT seg;
{
  frchainS *f, *first = NULL;

  for (f = frchain_root; f; f = f->frch_next)
    if (f->frch_seg == seg
	&& (! first || first->frch_subseg > f->frch_subseg))
      first = f;

  return first ? first->frch_root : NULL;
}

static struct frag *
last_frag_for_seg (seg)
     segT seg;
{
  frchainS *f, *last = NULL;

  for (f = frchain_root; f; f = f->frch_next)
    if (f->frch_seg == seg
	&& (! last || last->frch_subseg < f->frch_subseg))
      last= f;

  return last ? last->frch_last : NULL;
}

/* Emit a single byte into the current segment.  */

static inline void
out_byte (byte)
     int byte;
{
  FRAG_APPEND_1_CHAR (byte);
}

/* Emit a statement program opcode into the current segment.  */

static inline void
out_opcode (opc)
     int opc;
{
  out_byte (opc);
}

/* Emit a two-byte word into the current segment.  */

static inline void
out_two (data)
     int data;
{
  md_number_to_chars (frag_more (2), data, 2);
}

/* Emit a four byte word into the current segment.  */

static inline void
out_four (data)
     int data;
{
  md_number_to_chars (frag_more (4), data, 4);
}

/* Emit an unsigned "little-endian base 128" number.  */

static void
out_uleb128 (value)
     addressT value;
{
  output_leb128 (frag_more (sizeof_leb128 (value, 0)), value, 0);
}

/* Emit a tuple for .debug_abbrev.  */

static inline void
out_abbrev (name, form)
     int name, form;
{
  out_uleb128 (name);
  out_uleb128 (form);
}

/* Create a new fake symbol whose value is the current position.  */

static symbolS *
symbol_new_now ()
{
  return symbol_new (fake_label_name, now_seg, frag_now_fix (), frag_now);
}

/* Set the value of SYM to the current position in the current segment.  */

static void
set_symbol_value_now (sym)
     symbolS *sym;
{
  S_SET_SEGMENT (sym, now_seg);
  S_SET_VALUE (sym, frag_now_fix ());
  symbol_set_frag (sym, frag_now);
}

/* Get the size of a fragment.  */

static offsetT
get_frag_fix (frag)
     fragS *frag;
{
  frchainS *fr;

  if (frag->fr_next)
    return frag->fr_fix;

  /* If a fragment is the last in the chain, special measures must be
     taken to find its size before relaxation, since it may be pending
     on some subsegment chain.  */
  for (fr = frchain_root; fr; fr = fr->frch_next)
    if (fr->frch_last == frag)
      {
	long align_mask = -1 << get_recorded_alignment (fr->frch_seg);
	return (((char *) obstack_next_free (&fr->frch_obstack)
		 - frag->fr_literal) + ~align_mask) & align_mask;
      }

  abort ();
}

/* Set an absolute address (may result in a relocation entry).  */

static void
out_set_addr (seg, frag, ofs)
     segT seg;
     fragS *frag;
     addressT ofs;
{
  expressionS expr;
  symbolS *sym;

  sym = symbol_new (fake_label_name, seg, ofs, frag);

  out_opcode (DW_LNS_extended_op);
  out_uleb128 (sizeof_address + 1);

  out_opcode (DW_LNE_set_address);
  expr.X_op = O_symbol;
  expr.X_add_symbol = sym;
  expr.X_add_number = 0;
  emit_expr (&expr, sizeof_address);
}

/* Encode a pair of line and address skips as efficiently as possible.
   Note that the line skip is signed, whereas the address skip is unsigned.

   The following two routines *must* be kept in sync.  This is
   enforced by making emit_inc_line_addr abort if we do not emit
   exactly the expected number of bytes.  */

static int
size_inc_line_addr (line_delta, addr_delta)
     int line_delta;
     addressT addr_delta;
{
  unsigned int tmp, opcode;
  int len = 0;

  /* Scale the address delta by the minimum instruction length.  */
#if DWARF2_LINE_MIN_INSN_LENGTH > 1
  assert (addr_delta % DWARF2_LINE_MIN_INSN_LENGTH == 0);
  addr_delta /= DWARF2_LINE_MIN_INSN_LENGTH;
#endif

  /* INT_MAX is a signal that this is actually a DW_LNE_end_sequence.
     We cannot use special opcodes here, since we want the end_sequence
     to emit the matrix entry.  */
  if (line_delta == INT_MAX)
    {
      if (addr_delta == MAX_SPECIAL_ADDR_DELTA)
	len = 1;
      else
	len = 1 + sizeof_leb128 (addr_delta, 0);
      return len + 3;
    }

  /* Bias the line delta by the base.  */
  tmp = line_delta - DWARF2_LINE_BASE;

  /* If the line increment is out of range of a special opcode, we
     must encode it with DW_LNS_advance_line.  */
  if (tmp >= DWARF2_LINE_RANGE)
    {
      len = 1 + sizeof_leb128 (line_delta, 1);
      line_delta = 0;
      tmp = 0 - DWARF2_LINE_BASE;
    }

  /* Bias the opcode by the special opcode base.  */
  tmp += DWARF2_LINE_OPCODE_BASE;

  /* Avoid overflow when addr_delta is large.  */
  if (addr_delta < 256 + MAX_SPECIAL_ADDR_DELTA)
    {
      /* Try using a special opcode.  */
      opcode = tmp + addr_delta * DWARF2_LINE_RANGE;
      if (opcode <= 255)
	return len + 1;

      /* Try using DW_LNS_const_add_pc followed by special op.  */
      opcode = tmp + (addr_delta - MAX_SPECIAL_ADDR_DELTA) * DWARF2_LINE_RANGE;
      if (opcode <= 255)
	return len + 2;
    }

  /* Otherwise use DW_LNS_advance_pc.  */
  len += 1 + sizeof_leb128 (addr_delta, 0);

  /* DW_LNS_copy or special opcode.  */
  len += 1;

  return len;
}

static void
emit_inc_line_addr (line_delta, addr_delta, p, len)
     int line_delta;
     addressT addr_delta;
     char *p;
     int len;
{
  unsigned int tmp, opcode;
  int need_copy = 0;
  char *end = p + len;

#if DWARF2_LINE_MIN_INSN_LENGTH > 1
  /* Scale the address delta by the minimum instruction length.  */
  assert (addr_delta % DWARF2_LINE_MIN_INSN_LENGTH == 0);
  addr_delta /= DWARF2_LINE_MIN_INSN_LENGTH;
#endif
  /* INT_MAX is a signal that this is actually a DW_LNE_end_sequence.
     We cannot use special opcodes here, since we want the end_sequence
     to emit the matrix entry.  */
  if (line_delta == INT_MAX)
    {
      if (addr_delta == MAX_SPECIAL_ADDR_DELTA)
	*p++ = DW_LNS_const_add_pc;
      else
	{
	  *p++ = DW_LNS_advance_pc;
	  p += output_leb128 (p, addr_delta, 0);
	}

      *p++ = DW_LNS_extended_op;
      *p++ = 1;
      *p++ = DW_LNE_end_sequence;
      goto done;
    }

  /* Bias the line delta by the base.  */
  tmp = line_delta - DWARF2_LINE_BASE;

  /* If the line increment is out of range of a special opcode, we
     must encode it with DW_LNS_advance_line.  */
  if (tmp >= DWARF2_LINE_RANGE)
    {
      *p++ = DW_LNS_advance_line;
      p += output_leb128 (p, line_delta, 1);

      /* Prettier, I think, to use DW_LNS_copy instead of a
	 "line +0, addr +0" special opcode.  */
      if (addr_delta == 0)
	{
	  *p++ = DW_LNS_copy;
	  goto done;
	}

      line_delta = 0;
      tmp = 0 - DWARF2_LINE_BASE;
      need_copy = 1;
    }

  /* Bias the opcode by the special opcode base.  */
  tmp += DWARF2_LINE_OPCODE_BASE;

  /* Avoid overflow when addr_delta is large.  */
  if (addr_delta < 256 + MAX_SPECIAL_ADDR_DELTA)
    {
      /* Try using a special opcode.  */
      opcode = tmp + addr_delta * DWARF2_LINE_RANGE;
      if (opcode <= 255)
	{
	  *p++ = opcode;
	  goto done;
	}

      /* Try using DW_LNS_const_add_pc followed by special op.  */
      opcode = tmp + (addr_delta - MAX_SPECIAL_ADDR_DELTA) * DWARF2_LINE_RANGE;
      if (opcode <= 255)
	{
	  *p++ = DW_LNS_const_add_pc;
	  *p++ = opcode;
	  goto done;
	}
    }

  /* Otherwise use DW_LNS_advance_pc.  */
  *p++ = DW_LNS_advance_pc;
  p += output_leb128 (p, addr_delta, 0);

  if (need_copy)
    *p++ = DW_LNS_copy;
  else
    *p++ = tmp;

 done:
  assert (p == end);
}

/* Handy routine to combine calls to the above two routines.  */

static void
out_inc_line_addr (line_delta, addr_delta)
     int line_delta;
     addressT addr_delta;
{
  int len = size_inc_line_addr (line_delta, addr_delta);
  emit_inc_line_addr (line_delta, addr_delta, frag_more (len), len);
}

/* Generate a variant frag that we can use to relax address/line
   increments between fragments of the target segment.  */

static void
relax_inc_line_addr (line_delta, seg, to_frag, to_ofs, from_frag, from_ofs)
     int line_delta;
     segT seg;
     fragS *to_frag, *from_frag;
     addressT to_ofs, from_ofs;
{
  symbolS *to_sym, *from_sym;
  expressionS expr;
  int max_chars;

  to_sym = symbol_new (fake_label_name, seg, to_ofs, to_frag);
  from_sym = symbol_new (fake_label_name, seg, from_ofs, from_frag);

  expr.X_op = O_subtract;
  expr.X_add_symbol = to_sym;
  expr.X_op_symbol = from_sym;
  expr.X_add_number = 0;

  /* The maximum size of the frag is the line delta with a maximum
     sized address delta.  */
  max_chars = size_inc_line_addr (line_delta, -DWARF2_LINE_MIN_INSN_LENGTH);

  frag_var (rs_dwarf2dbg, max_chars, max_chars, 1,
	    make_expr_symbol (&expr), line_delta, NULL);
}

/* The function estimates the size of a rs_dwarf2dbg variant frag
   based on the current values of the symbols.  It is called before
   the relaxation loop.  We set fr_subtype to the expected length.  */

int
dwarf2dbg_estimate_size_before_relax (frag)
     fragS *frag;
{
  offsetT addr_delta;
  int size;

  addr_delta = resolve_symbol_value (frag->fr_symbol);
  size = size_inc_line_addr (frag->fr_offset, addr_delta);

  frag->fr_subtype = size;

  return size;
}

/* This function relaxes a rs_dwarf2dbg variant frag based on the
   current values of the symbols.  fr_subtype is the current length
   of the frag.  This returns the change in frag length.  */

int
dwarf2dbg_relax_frag (frag)
     fragS *frag;
{
  int old_size, new_size;

  old_size = frag->fr_subtype;
  new_size = dwarf2dbg_estimate_size_before_relax (frag);

  return new_size - old_size;
}

/* This function converts a rs_dwarf2dbg variant frag into a normal
   fill frag.  This is called after all relaxation has been done.
   fr_subtype will be the desired length of the frag.  */

void
dwarf2dbg_convert_frag (frag)
     fragS *frag;
{
  offsetT addr_diff;

  addr_diff = resolve_symbol_value (frag->fr_symbol);

  /* fr_var carries the max_chars that we created the fragment with.
     fr_subtype carries the current expected length.  We must, of
     course, have allocated enough memory earlier.  */
  assert (frag->fr_var >= (int) frag->fr_subtype);

  emit_inc_line_addr (frag->fr_offset, addr_diff,
		      frag->fr_literal + frag->fr_fix, frag->fr_subtype);

  frag->fr_fix += frag->fr_subtype;
  frag->fr_type = rs_fill;
  frag->fr_var = 0;
  frag->fr_offset = 0;
}

/* Generate .debug_line content for the chain of line number entries
   beginning at E, for segment SEG.  */

static void
process_entries (seg, e)
     segT seg;
     struct line_entry *e;
{
  unsigned filenum = 1;
  unsigned line = 1;
  unsigned column = 0;
  unsigned flags = DWARF2_LINE_DEFAULT_IS_STMT ? DWARF2_FLAG_BEGIN_STMT : 0;
  fragS *frag = NULL;
  fragS *last_frag;
  addressT frag_ofs = 0;
  addressT last_frag_ofs;
  struct line_entry *next;

  while (e)
    {
      int changed = 0;

      if (filenum != e->loc.filenum)
	{
	  filenum = e->loc.filenum;
	  out_opcode (DW_LNS_set_file);
	  out_uleb128 (filenum);
	  changed = 1;
	}

      if (column != e->loc.column)
	{
	  column = e->loc.column;
	  out_opcode (DW_LNS_set_column);
	  out_uleb128 (column);
	  changed = 1;
	}

      if ((e->loc.flags ^ flags) & DWARF2_FLAG_BEGIN_STMT)
	{
	  flags = e->loc.flags;
	  out_opcode (DW_LNS_negate_stmt);
	  changed = 1;
	}

      if (e->loc.flags & DWARF2_FLAG_BEGIN_BLOCK)
	{
	  out_opcode (DW_LNS_set_basic_block);
	  changed = 1;
	}

      /* Don't try to optimize away redundant entries; gdb wants two
	 entries for a function where the code starts on the same line as
	 the {, and there's no way to identify that case here.  Trust gcc
	 to optimize appropriately.  */
      if (1 /* line != e->loc.line || changed */)
	{
	  int line_delta = e->loc.line - line;
	  if (frag == NULL)
	    {
	      out_set_addr (seg, e->frag, e->frag_ofs);
	      out_inc_line_addr (line_delta, 0);
	    }
	  else if (frag == e->frag)
	    out_inc_line_addr (line_delta, e->frag_ofs - frag_ofs);
	  else
	    relax_inc_line_addr (line_delta, seg, e->frag, e->frag_ofs,
				 frag, frag_ofs);

	  frag = e->frag;
	  frag_ofs = e->frag_ofs;
	  line = e->loc.line;
	}
      else if (frag == NULL)
	{
	  out_set_addr (seg, e->frag, e->frag_ofs);
	  frag = e->frag;
	  frag_ofs = e->frag_ofs;
	}

      next = e->next;
      free (e);
      e = next;
    }

  /* Emit a DW_LNE_end_sequence for the end of the section.  */
  last_frag = last_frag_for_seg (seg);
  last_frag_ofs = get_frag_fix (last_frag);
  if (frag == last_frag)
    out_inc_line_addr (INT_MAX, last_frag_ofs - frag_ofs);
  else
    relax_inc_line_addr (INT_MAX, seg, last_frag, last_frag_ofs,
			 frag, frag_ofs);
}

/* Emit the directory and file tables for .debug_line.  */

static void
out_file_list ()
{
  size_t size;
  char *cp;
  unsigned int i;

  /* Terminate directory list.  */
  out_byte ('\0');

  for (i = 1; i < files_in_use; ++i)
    {
      if (files[i].filename == NULL)
	{
	  as_bad (_("unassigned file number %ld"), (long) i);
	  continue;
	}

      size = strlen (files[i].filename) + 1;
      cp = frag_more (size);
      memcpy (cp, files[i].filename, size);

      out_uleb128 (files[i].dir);	/* directory number */
      out_uleb128 (0);			/* last modification timestamp */
      out_uleb128 (0);			/* filesize */
    }

  /* Terminate filename list.  */
  out_byte (0);
}

/* Emit the collected .debug_line data.  */

static void
out_debug_line (line_seg)
     segT line_seg;
{
  expressionS expr;
  symbolS *line_start;
  symbolS *prologue_end;
  symbolS *line_end;
  struct line_seg *s;

  subseg_set (line_seg, 0);

  line_start = symbol_new_now ();
  prologue_end = symbol_make (fake_label_name);
  line_end = symbol_make (fake_label_name);

  /* Total length of the information for this compilation unit.  */
  expr.X_op = O_subtract;
  expr.X_add_symbol = line_end;
  expr.X_op_symbol = line_start;
  expr.X_add_number = -4;
  emit_expr (&expr, 4);

  /* Version.  */
  out_two (2);

  /* Length of the prologue following this length.  */
  expr.X_op = O_subtract;
  expr.X_add_symbol = prologue_end;
  expr.X_op_symbol = line_start;
  expr.X_add_number = - (4 + 2 + 4);
  emit_expr (&expr, 4);

  /* Parameters of the state machine.  */
  out_byte (DWARF2_LINE_MIN_INSN_LENGTH);
  out_byte (DWARF2_LINE_DEFAULT_IS_STMT);
  out_byte (DWARF2_LINE_BASE);
  out_byte (DWARF2_LINE_RANGE);
  out_byte (DWARF2_LINE_OPCODE_BASE);

  /* Standard opcode lengths.  */
  out_byte (0);			/* DW_LNS_copy */
  out_byte (1);			/* DW_LNS_advance_pc */
  out_byte (1);			/* DW_LNS_advance_line */
  out_byte (1);			/* DW_LNS_set_file */
  out_byte (1);			/* DW_LNS_set_column */
  out_byte (0);			/* DW_LNS_negate_stmt */
  out_byte (0);			/* DW_LNS_set_basic_block */
  out_byte (0);			/* DW_LNS_const_add_pc */
  out_byte (1);			/* DW_LNS_fixed_advance_pc */

  out_file_list ();

  set_symbol_value_now (prologue_end);

  /* For each section, emit a statement program.  */
  for (s = all_segs; s; s = s->next)
    process_entries (s->seg, s->head->head);

  set_symbol_value_now (line_end);
}

/* Emit data for .debug_aranges.  */

static void
out_debug_aranges (aranges_seg, info_seg)
     segT aranges_seg;
     segT info_seg;
{
  unsigned int addr_size = sizeof_address;
  addressT size, skip;
  struct line_seg *s;
  expressionS expr;
  char *p;

  size = 4 + 2 + 4 + 1 + 1;

  skip = 2 * addr_size - (size & (2 * addr_size - 1));
  if (skip == 2 * addr_size)
    skip = 0;
  size += skip;

  for (s = all_segs; s; s = s->next)
    size += 2 * addr_size;

  size += 2 * addr_size;

  subseg_set (aranges_seg, 0);

  /* Length of the compilation unit.  */
  out_four (size - 4);

  /* Version.  */
  out_two (2);

  /* Offset to .debug_info.  */
  expr.X_op = O_symbol;
  expr.X_add_symbol = section_symbol (info_seg);
  expr.X_add_number = 0;
  emit_expr (&expr, 4);

  /* Size of an address (offset portion).  */
  out_byte (addr_size);

  /* Size of a segment descriptor.  */
  out_byte (0);

  /* Align the header.  */
  if (skip)
    frag_align (ffs (2 * addr_size) - 1, 0, 0);

  for (s = all_segs; s; s = s->next)
    {
      fragS *frag;
      symbolS *beg, *end;

      frag = first_frag_for_seg (s->seg);
      beg = symbol_new (fake_label_name, s->seg, 0, frag);
      s->text_start = beg;

      frag = last_frag_for_seg (s->seg);
      end = symbol_new (fake_label_name, s->seg, get_frag_fix (frag), frag);
      s->text_end = end;

      expr.X_op = O_symbol;
      expr.X_add_symbol = beg;
      expr.X_add_number = 0;
      emit_expr (&expr, addr_size);

      expr.X_op = O_subtract;
      expr.X_add_symbol = end;
      expr.X_op_symbol = beg;
      expr.X_add_number = 0;
      emit_expr (&expr, addr_size);
    }

  p = frag_more (2 * addr_size);
  md_number_to_chars (p, 0, addr_size);
  md_number_to_chars (p + addr_size, 0, addr_size);
}

/* Emit data for .debug_abbrev.  Note that this must be kept in
   sync with out_debug_info below.  */

static void
out_debug_abbrev (abbrev_seg)
     segT abbrev_seg;
{
  subseg_set (abbrev_seg, 0);

  out_uleb128 (1);
  out_uleb128 (DW_TAG_compile_unit);
  out_byte (DW_CHILDREN_no);
  out_abbrev (DW_AT_stmt_list, DW_FORM_data4);
  if (all_segs->next == NULL)
    {
      out_abbrev (DW_AT_low_pc, DW_FORM_addr);
      out_abbrev (DW_AT_high_pc, DW_FORM_addr);
    }
  out_abbrev (DW_AT_name, DW_FORM_string);
  out_abbrev (DW_AT_comp_dir, DW_FORM_string);
  out_abbrev (DW_AT_producer, DW_FORM_string);
  out_abbrev (DW_AT_language, DW_FORM_data2);
  out_abbrev (0, 0);

  /* Terminate the abbreviations for this compilation unit.  */
  out_byte (0);
}

/* Emit a description of this compilation unit for .debug_info.  */

static void
out_debug_info (info_seg, abbrev_seg, line_seg)
     segT info_seg;
     segT abbrev_seg;
     segT line_seg;
{
  char producer[128];
  char *comp_dir;
  expressionS expr;
  symbolS *info_start;
  symbolS *info_end;
  char *p;
  int len;

  subseg_set (info_seg, 0);

  info_start = symbol_new_now ();
  info_end = symbol_make (fake_label_name);

  /* Compilation Unit length.  */
  expr.X_op = O_subtract;
  expr.X_add_symbol = info_end;
  expr.X_op_symbol = info_start;
  expr.X_add_number = -4;
  emit_expr (&expr, 4);

  /* DWARF version.  */
  out_two (2);

  /* .debug_abbrev offset */
  expr.X_op = O_symbol;
  expr.X_add_symbol = section_symbol (abbrev_seg);
  expr.X_add_number = 0;
  emit_expr (&expr, 4);

  /* Target address size.  */
  out_byte (sizeof_address);

  /* DW_TAG_compile_unit DIE abbrev */
  out_uleb128 (1);

  /* DW_AT_stmt_list */
  expr.X_op = O_symbol;
  expr.X_add_symbol = section_symbol (line_seg);
  expr.X_add_number = 0;
  emit_expr (&expr, 4);

  /* These two attributes may only be emitted if all of the code is
     contiguous.  Multiple sections are not that.  */
  if (all_segs->next == NULL)
    {
      /* DW_AT_low_pc */
      expr.X_op = O_symbol;
      expr.X_add_symbol = all_segs->text_start;
      expr.X_add_number = 0;
      emit_expr (&expr, sizeof_address);

      /* DW_AT_high_pc */
      expr.X_op = O_symbol;
      expr.X_add_symbol = all_segs->text_end;
      expr.X_add_number = 0;
      emit_expr (&expr, sizeof_address);
    }

  /* DW_AT_name.  We don't have the actual file name that was present
     on the command line, so assume files[1] is the main input file.
     We're not supposed to get called unless at least one line number
     entry was emitted, so this should always be defined.  */
  if (!files || files_in_use < 1)
    abort ();
  len = strlen (files[1].filename) + 1;
  p = frag_more (len);
  memcpy (p, files[1].filename, len);

  /* DW_AT_comp_dir */
  comp_dir = getpwd ();
  len = strlen (comp_dir) + 1;
  p = frag_more (len);
  memcpy (p, comp_dir, len);

  /* DW_AT_producer */
  sprintf (producer, "GNU AS %s", VERSION);
  len = strlen (producer) + 1;
  p = frag_more (len);
  memcpy (p, producer, len);

  /* DW_AT_language.  Yes, this is probably not really MIPS, but the
     dwarf2 draft has no standard code for assembler.  */
  out_two (DW_LANG_Mips_Assembler);

  set_symbol_value_now (info_end);
}

void
dwarf2_finish ()
{
  segT line_seg;
  struct line_seg *s;

  /* We don't need to do anything unless:
     - Some debug information was recorded via .file/.loc
     - or, we are generating DWARF2 information ourself (--gdwarf2)
     - or, there is a user-provided .debug_info section which could
       reference the file table in the .debug_line section we generate
       below.  */
  if (all_segs == NULL
      && debug_type != DEBUG_DWARF2
      && bfd_get_section_by_name (stdoutput, ".debug_info") == NULL)
    return;

  /* Calculate the size of an address for the target machine.  */
  sizeof_address = bfd_arch_bits_per_address (stdoutput) / 8;

  /* Create and switch to the line number section.  */
  line_seg = subseg_new (".debug_line", 0);
  bfd_set_section_flags (stdoutput, line_seg, SEC_READONLY);

  /* For each subsection, chain the debug entries together.  */
  for (s = all_segs; s; s = s->next)
    {
      struct line_subseg *ss = s->head;
      struct line_entry **ptail = ss->ptail;

      while ((ss = ss->next) != NULL)
	{
	  *ptail = ss->head;
	  ptail = ss->ptail;
	}
    }

  out_debug_line (line_seg);

  /* If this is assembler generated line info, we need .debug_info
     and .debug_abbrev sections as well.  */
  if (all_segs != NULL && debug_type == DEBUG_DWARF2)
    {
      segT abbrev_seg;
      segT info_seg;
      segT aranges_seg;

      info_seg = subseg_new (".debug_info", 0);
      abbrev_seg = subseg_new (".debug_abbrev", 0);
      aranges_seg = subseg_new (".debug_aranges", 0);

      bfd_set_section_flags (stdoutput, info_seg, SEC_READONLY);
      bfd_set_section_flags (stdoutput, abbrev_seg, SEC_READONLY);
      bfd_set_section_flags (stdoutput, aranges_seg, SEC_READONLY);

      record_alignment (aranges_seg, ffs (2 * sizeof_address) - 1);

      out_debug_aranges (aranges_seg, info_seg);
      out_debug_abbrev (abbrev_seg);
      out_debug_info (info_seg, abbrev_seg, line_seg);
    }
}

#else
void
dwarf2_finish ()
{
}

int
dwarf2dbg_estimate_size_before_relax (frag)
     fragS *frag ATTRIBUTE_UNUSED;
{
  as_fatal (_("dwarf2 is not supported for this object file format"));
  return 0;
}

int
dwarf2dbg_relax_frag (frag)
     fragS *frag ATTRIBUTE_UNUSED;
{
  as_fatal (_("dwarf2 is not supported for this object file format"));
  return 0;
}

void
dwarf2dbg_convert_frag (frag)
     fragS *frag ATTRIBUTE_UNUSED;
{
  as_fatal (_("dwarf2 is not supported for this object file format"));
}

void
dwarf2_emit_insn (size)
     int size ATTRIBUTE_UNUSED;
{
}

void
dwarf2_directive_file (dummy)
     int dummy ATTRIBUTE_UNUSED;
{
  s_app_file (0);
}

void
dwarf2_directive_loc (dummy)
     int dummy ATTRIBUTE_UNUSED;
{
  as_fatal (_("dwarf2 is not supported for this object file format"));
}
#endif /* BFD_ASSEMBLER */
