/* dwarf2dbg.c - DWARF2 debug support
   Copyright (C) 1999, 2000 Free Software Foundation, Inc.
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
# define DWARF2_LINE_MIN_INSN_LENGTH	4
#endif

/* Flag that indicates the initial value of the is_stmt_start flag.
   In the present implementation, we do not mark any lines as
   the beginning of a source statement, because that information
   is not made available by the GCC front-end.  */
#define	DWARF2_LINE_DEFAULT_IS_STMT	1

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

#define INITIAL_STATE						\
  /* Initialize as per DWARF2.0 standard.  */			\
  0,					/* address */		\
  1,					/* file */		\
  1,					/* line */		\
  0,					/* column */		\
  DWARF2_LINE_DEFAULT_IS_STMT,		/* is_stmt */		\
  0,					/* basic_block */	\
  1					/* empty_sequence */

static struct
  {
    /* state machine state as per DWARF2 manual: */
    struct dwarf2_sm
      {
	addressT addr;
	unsigned int filenum;
	unsigned int line;
	unsigned int column;
	unsigned int
	  is_stmt : 1,
	  basic_block : 1,
	  empty_sequence : 1;		/* current code sequence has no DWARF2 directives? */
      }
    sm;

    unsigned int
      any_dwarf2_directives : 1;	/* did we emit any DWARF2 line debug directives? */

    fragS * frag;	/* frag that "addr" is relative to */
    segT text_seg;	/* text segment "addr" is relative to */
    subsegT text_subseg;
    segT line_seg;	/* ".debug_line" segment */
    int last_filename;	/* index of last filename that was used */
    int num_filenames;	/* index of last filename in use */
    int filename_len;	/* length of the filename array */
    struct
      {
	int dir;	/* valid after gen_dir_list() only */
	char *name; /* full path before gen_dir_list(), filename afterwards */
      }
    *file;

    struct dwarf2_line_info current;	/* current source info: */

    /* counters for statistical purposes: */
    unsigned int num_line_entries;
    unsigned int opcode_hist[256];	/* histogram of opcode frequencies */
  }
ls =
  {
    {
      INITIAL_STATE
    },
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    NULL,
    { NULL, 0, 0, 0, 0 },
    0,
    {
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
    }
  };


/* Function prototypes: */
static void out_uleb128 PARAMS ((addressT));
static void out_sleb128 PARAMS ((offsetT));
static void gen_addr_line PARAMS ((int, addressT));
static void reset_state_machine PARAMS ((void));
static void out_set_addr PARAMS ((addressT));
static void out_end_sequence PARAMS ((void));
static int get_filenum PARAMS ((int, char *));
static void gen_dir_list PARAMS ((void));
static void gen_file_list PARAMS ((void));
static void print_stats PARAMS ((unsigned long));


#define out_byte(byte)	FRAG_APPEND_1_CHAR(byte)
#define out_opcode(opc)	(out_byte ((opc)), ++ls.opcode_hist[(opc) & 0xff])

/* Output an unsigned "little-endian base 128" number.  */
static void
out_uleb128 (value)
     addressT value;
{
  unsigned char byte, more = 0x80;

  do
    {
      byte = value & 0x7f;
      value >>= 7;
      if (value == 0)
	more = 0;
      out_byte (more | byte);
    }
  while (more);
}

/* Output a signed "little-endian base 128" number.  */
static void
out_sleb128 (value)
     offsetT value;
{
  unsigned char byte, more = 0x80;

  do
    {
      byte = value & 0x7f;
      value >>= 7;
      if (((value == 0) && ((byte & 0x40) == 0))
	  || ((value == -1) && ((byte & 0x40) != 0)))
	more = 0;
      out_byte (more | byte);
    }
  while (more);
}

/* Encode a pair of line and address skips as efficiently as possible.
   Note that the line skip is signed, whereas the address skip is
   unsigned.  */
static void
gen_addr_line (line_delta, addr_delta)
     int line_delta;
     addressT addr_delta;
{
  unsigned int tmp, opcode;

  tmp = line_delta - DWARF2_LINE_BASE;

  if (tmp >= DWARF2_LINE_RANGE)
    {
      out_opcode (DW_LNS_advance_line);
      out_sleb128 (line_delta);
      tmp = 0 - DWARF2_LINE_BASE;
      line_delta = 0;
    }

  tmp += DWARF2_LINE_OPCODE_BASE;

  /* try using a special opcode: */
  opcode = tmp + addr_delta*DWARF2_LINE_RANGE;
  if (opcode <= 255)
    {
      out_opcode (opcode);
      return;
    }

  /* try using DW_LNS_const_add_pc followed by special op: */
  opcode = tmp + (addr_delta - MAX_SPECIAL_ADDR_DELTA)*DWARF2_LINE_RANGE;
  if (opcode <= 255)
    {
      out_opcode (DW_LNS_const_add_pc);
      out_opcode (opcode);
      return;
    }

  out_opcode (DW_LNS_advance_pc);
  out_uleb128 (addr_delta);

  if (line_delta)
    out_opcode (tmp);		/* output line-delta */
  else
    out_opcode (DW_LNS_copy);	/* append new row with current info */
}

static void
reset_state_machine ()
{
  static const struct dwarf2_sm initial_state = { INITIAL_STATE };

  ls.sm = initial_state;
}

/* Set an absolute address (may results in a relocation entry): */
static void
out_set_addr (addr)
     addressT addr;
{
  subsegT saved_subseg;
  segT saved_seg;
  expressionS expr;
  symbolS *sym;
  int bytes_per_address;

  saved_seg = now_seg;
  saved_subseg = now_subseg;

  subseg_set (ls.text_seg, ls.text_subseg);
  sym = symbol_new (".L0\001", now_seg, addr, frag_now);

  subseg_set (saved_seg, saved_subseg);

#ifdef BFD_ASSEMBLER
  bytes_per_address = bfd_arch_bits_per_address (stdoutput) / 8;
#else
  /* FIXME.  */
  bytes_per_address = 4;
#endif

  out_opcode (DW_LNS_extended_op);
  out_uleb128 (bytes_per_address + 1);

  out_opcode (DW_LNE_set_address);
  expr.X_op = O_symbol;
  expr.X_add_symbol = sym;
  expr.X_add_number = 0;
  emit_expr (&expr, bytes_per_address);
}

/* Emit DW_LNS_end_sequence and reset state machine.  Does not
   preserve the current segment/sub-segment!  */
static void
out_end_sequence ()
{
  addressT addr, delta;
  fragS *text_frag;

  if (ls.text_seg)
    {
      subseg_set (ls.text_seg, ls.text_subseg);
#ifdef md_current_text_addr
      addr = md_current_text_addr ();
#else
      addr = frag_now_fix ();
#endif
      text_frag = frag_now;
      subseg_set (ls.line_seg, DL_BODY);
      if (text_frag != ls.frag)
	{
	  out_set_addr (addr);
	  ls.sm.addr = addr;
	  ls.frag = text_frag;
	}
      else
	{
	  delta = (addr - ls.sm.addr) / DWARF2_LINE_MIN_INSN_LENGTH;
	  if (delta > 0)
	    {
	      /* Advance address without updating the line-debug
		 matrix---the end_sequence entry is used only to tell
		 the debugger the end of the sequence.*/
	      out_opcode (DW_LNS_advance_pc);
	      out_uleb128 (delta);
	    }
	}
    }
  else
    subseg_set (ls.line_seg, DL_BODY);

  out_opcode (DW_LNS_extended_op);
  out_uleb128 (1);
  out_byte (DW_LNE_end_sequence);

  reset_state_machine ();
}

/* Look up a filenumber either by filename or by filenumber.  If both
   a filenumber and a filename are specified, lookup by filename takes
   precedence.  If the filename cannot be found, it is added to the
   filetable and the filenumber for the new entry is returned.  */
static int
get_filenum (filenum, file)
     int filenum;
     char *file;
{
  int i, last = filenum - 1;
  char char0 = file[0];

  /* If filenum is out of range of the filename table, then try using the
     table entry returned from the previous call.  */
  if (last >= ls.num_filenames || last < 0)
    last = ls.last_filename;

  /* Do a quick check against the specified or previously used filenum.  */
  if (ls.num_filenames > 0 && ls.file[last].name[0] == char0
      && strcmp (ls.file[last].name + 1, file + 1) == 0)
    return last + 1;

  /* no match, fall back to simple linear scan: */
  for (i = 0; i < ls.num_filenames; ++i)
    {
      if (ls.file[i].name[0] == char0
	  && strcmp (ls.file[i].name + 1, file + 1) == 0)
	{
	  ls.last_filename = i;
	  return i + 1;
	}
    }

  /* no match: enter new filename */
  if (ls.num_filenames >= ls.filename_len)
    {
      ls.filename_len += 13;
      ls.file = xrealloc (ls.file, ls.filename_len * sizeof (ls.file[0]));
    }
  ls.file[ls.num_filenames].dir = 0;
  ls.file[ls.num_filenames].name = file;
  ls.last_filename = ls.num_filenames;
  return ++ls.num_filenames;
}

/* Emit an entry in the line number table if the address or line has changed.
   ADDR is relative to the current frag in the text section.  */

void
dwarf2_gen_line_info (addr, l)
     addressT addr;
     struct dwarf2_line_info *l;
{
  unsigned int filenum = l->filenum;
  unsigned int any_output = 0;
  subsegT saved_subseg;
  segT saved_seg;
  fragS *saved_frag;

  if (flag_debug)
    fprintf (stderr, "line: addr %lx file `%s' line %u col %u flags %x\n",
	     (unsigned long) addr, l->filename, l->line, l->column, l->flags);

  if (filenum > 0 && !l->filename)
    {
      if (filenum >= (unsigned int) ls.num_filenames)
	{
	  as_warn ("Encountered bad file number in line number debug info!");
	  return;
	}
    }
  else if (l->filename)
    filenum = get_filenum (filenum, l->filename);
  else
    return;	/* no filename, no filnum => no play */

  /* Must save these before the subseg_new call, as that call will change
     them.  */
  saved_seg = now_seg;
  saved_subseg = now_subseg;
  saved_frag = frag_now;

  if (!ls.line_seg)
    {
#ifdef BFD_ASSEMBLER
      symbolS *secsym;
#endif

      ls.line_seg = subseg_new (".debug_line", 0);

#ifdef BFD_ASSEMBLER
      bfd_set_section_flags (stdoutput, ls.line_seg, SEC_READONLY);

      /* We're going to need this symbol.  */
      secsym = symbol_find (".debug_line");
      if (secsym != NULL)
        symbol_set_bfdsym (secsym, ls.line_seg->symbol);
      else
        symbol_table_insert (section_symbol (ls.line_seg));
#endif
    }

  subseg_set (ls.line_seg, DL_BODY);

  if (ls.text_seg != saved_seg || ls.text_subseg != saved_subseg)
    {
      if (!ls.sm.empty_sequence)
	{
	  out_end_sequence ();		/* terminate previous sequence */
	  ls.sm.empty_sequence = 1;
	}
      any_output = 1;
      ls.text_seg = saved_seg;
      ls.text_subseg = saved_subseg;
      out_set_addr (addr);
      ls.sm.addr = addr;
      ls.frag = saved_frag;
    }

  if (ls.sm.filenum != filenum)
    {
      any_output = 1;
      out_opcode (DW_LNS_set_file);
      out_uleb128 (filenum);
      ls.sm.filenum = filenum;
    }

  if (ls.sm.column != l->column)
    {
      any_output = 1;
      out_opcode (DW_LNS_set_column);
      out_uleb128 (l->column);
      ls.sm.column = l->column;
    }

  if (((l->flags & DWARF2_FLAG_BEGIN_STMT) != 0) != ls.sm.is_stmt)
    {
      any_output = 1;
      out_opcode (DW_LNS_negate_stmt);
    }

  if (l->flags & DWARF2_FLAG_BEGIN_BLOCK)
    {
      any_output = 1;
      out_opcode (DW_LNS_set_basic_block);
    }

  if (ls.sm.line != l->line)
    {
      any_output = 1;
      if (saved_frag != ls.frag)
	{
	  /* If a new frag got allocated (for whatever reason), then
	     deal with it by generating a reference symbol.  Note: no
	     end_sequence needs to be generated because the address did
	     not really decrease (only the reference point changed).  */
	  out_set_addr (addr);
	  ls.sm.addr = addr;
	  ls.frag = saved_frag;
	}
      gen_addr_line (l->line - ls.sm.line,
		     (addr - ls.sm.addr) / DWARF2_LINE_MIN_INSN_LENGTH);
      ls.sm.basic_block = 0;
      ls.sm.line = l->line;
      ls.sm.addr = addr;
    }

  subseg_set (saved_seg, saved_subseg);

  ls.num_line_entries += any_output;
  if (any_output)
    ls.sm.empty_sequence = 0;
}

static void
gen_dir_list ()
{
  char *str, *slash, *dir_list, *dp, *cp;
  int i, j, num_dirs;

  dir_list = frag_more (0);
  num_dirs = 0;

  for (i = 0; i < ls.num_filenames; ++i)
    {
      str = ls.file[i].name;
      slash = strrchr (str, '/');
      if (slash)
	{
	  *slash = '\0';
	  for (j = 0, dp = dir_list; j < num_dirs; ++j)
	    {
	      if (strcmp (str, dp) == 0)
		{
		  ls.file[i].dir = j + 1;
		  break;
		}
	      dp += strlen (dp);
	    }
	  if (j >= num_dirs)
	    {
	      /* didn't find this directory: append it to the list */
	      size_t size = strlen (str) + 1;
	      cp = frag_more (size);
	      memcpy (cp, str, size);
	      ls.file[i].dir = ++num_dirs;
	    }
	  *slash = '/';
	  ls.file[i].name = slash + 1;
	}
    }
  out_byte ('\0');	/* terminate directory list */
}

static void
gen_file_list ()
{
  size_t size;
  char *cp;
  int i;

  for (i = 0; i < ls.num_filenames; ++i)
    {
      size = strlen (ls.file[i].name) + 1;
      cp = frag_more (size);
      memcpy (cp, ls.file[i].name, size);

      out_uleb128 (ls.file[i].dir);	/* directory number */
      out_uleb128 (0);			/* last modification timestamp */
      out_uleb128 (0);			/* filesize */
    }
  out_byte (0);		/* terminate filename list */
}

static void
print_stats (total_size)
     unsigned long total_size;
{
  static const char *opc_name[] =
    {
      "extended", "copy", "advance_pc", "advance_line", "set_file",
      "set_column", "negate_stmt", "set_basic_block", "const_add_pc",
      "fixed_advance_pc"
    };
  size_t i;
  int j;

  fprintf (stderr, "Average size: %g bytes/line\n",
	   total_size / (double) ls.num_line_entries);

  fprintf (stderr, "\nStandard opcode histogram:\n");

  for (i = 0; i < sizeof (opc_name)/sizeof (opc_name[0]); ++i)
    {
      fprintf (stderr, "%s", opc_name[i]);
      for (j = strlen (opc_name[i]); j < 16; ++j)
	fprintf (stderr, " ");
      fprintf (stderr, ": %u\n", ls.opcode_hist[i]);
    }

  fprintf (stderr, "\nSpecial opcodes:\naddr\t\t\t\tline skip\n");

  fprintf (stderr, "skip: ");
  for (j = DWARF2_LINE_BASE; j < DWARF2_LINE_BASE + DWARF2_LINE_RANGE; ++j)
    fprintf (stderr, "%3d", j);
  fprintf (stderr, "\n-----");

  for (; i < 256; ++i)
    {
      j = SPECIAL_LINE (i);
      if (j == DWARF2_LINE_BASE)
	fprintf (stderr, "\n%4u: ",
		 ((unsigned int)
		  DWARF2_LINE_MIN_INSN_LENGTH * SPECIAL_ADDR (i)));
      fprintf (stderr, " %2u", ls.opcode_hist[i]);
    }
  fprintf (stderr, "\n");
}

void
dwarf2_finish ()
{
  addressT body_size, total_size, prolog_size;
  subsegT saved_subseg;
  segT saved_seg;
  char *cp;

  if (!ls.line_seg)
    /* no .debug_line segment, no work to do... */
    return;

  saved_seg = now_seg;
  saved_subseg = now_subseg;

  if (!ls.sm.empty_sequence)
    out_end_sequence ();
  total_size = body_size = frag_now_fix ();

  /* now generate the directory and file lists: */
  subseg_set (ls.line_seg, DL_FILES);
  gen_dir_list ();
  gen_file_list ();
  total_size += frag_now_fix ();

  /* and now the header ("statement program prolog", in DWARF2 lingo...) */
  subseg_set (ls.line_seg, DL_PROLOG);

  cp = frag_more (15 + DWARF2_LINE_OPCODE_BASE - 1);

  total_size += frag_now_fix ();
  prolog_size = total_size - body_size - 10;

# define STUFF(val,size)	md_number_to_chars (cp, val, size); cp += size;
  STUFF (total_size - 4, 4);	/* length */
  STUFF (2, 2);			/* version */
  STUFF (prolog_size, 4);	/* prologue_length */
  STUFF (DWARF2_LINE_MIN_INSN_LENGTH, 1);
  STUFF (DWARF2_LINE_DEFAULT_IS_STMT, 1);
  STUFF (DWARF2_LINE_BASE, 1);
  STUFF (DWARF2_LINE_RANGE, 1);
  STUFF (DWARF2_LINE_OPCODE_BASE, 1);

  /* standard_opcode_lengths: */
  STUFF (0, 1);			/* DW_LNS_copy */
  STUFF (1, 1);			/* DW_LNS_advance_pc */
  STUFF (1, 1);			/* DW_LNS_advance_line */
  STUFF (1, 1);			/* DW_LNS_set_file */
  STUFF (1, 1);			/* DW_LNS_set_column */
  STUFF (0, 1);			/* DW_LNS_negate_stmt */
  STUFF (0, 1);			/* DW_LNS_set_basic_block */
  STUFF (0, 1);			/* DW_LNS_const_add_pc */
  STUFF (1, 1);			/* DW_LNS_fixed_advance_pc */

  subseg_set (saved_seg, saved_subseg);

  if (flag_debug)
    print_stats (total_size);
}

void
dwarf2_directive_file (dummy)
     int dummy ATTRIBUTE_UNUSED;
{
  int len;

  /* Continue to accept a bare string and pass it off.  */
  SKIP_WHITESPACE ();
  if (*input_line_pointer == '"')
    {
      s_app_file (0);
      return;
    }

  ls.any_dwarf2_directives = 1;

  if (debug_type == DEBUG_NONE)
    /* Automatically turn on DWARF2 debug info unless something else
       has been selected.  */
    debug_type = DEBUG_DWARF2;

  ls.current.filenum = get_absolute_expression ();
  ls.current.filename = demand_copy_C_string (&len);

  demand_empty_rest_of_line ();
}

void
dwarf2_directive_loc (dummy)
     int dummy ATTRIBUTE_UNUSED;
{
  ls.any_dwarf2_directives = 1;

  ls.current.filenum = get_absolute_expression ();
  SKIP_WHITESPACE ();
  ls.current.line = get_absolute_expression ();
  SKIP_WHITESPACE ();
  ls.current.column = get_absolute_expression ();
  demand_empty_rest_of_line ();

  ls.current.flags = DWARF2_FLAG_BEGIN_STMT;

#ifndef NO_LISTING
  if (listing)
    listing_source_line (ls.current.line);
#endif
}

void
dwarf2_where (line)
     struct dwarf2_line_info *line;
{
  if (ls.any_dwarf2_directives)
    *line = ls.current;
  else
    {
      as_where (&line->filename, &line->line);
      line->filenum = 0;
      line->column = 0;
      line->flags = DWARF2_FLAG_BEGIN_STMT;
    }
}
