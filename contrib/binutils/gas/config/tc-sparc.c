/* tc-sparc.c -- Assemble for the SPARC
   Copyright (C) 1989, 90-96, 97, 1998 Free Software Foundation, Inc.
   This file is part of GAS, the GNU Assembler.

   GAS is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   GAS is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public
   License along with GAS; see the file COPYING.  If not, write
   to the Free Software Foundation, 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA. */

#include <stdio.h>
#include <ctype.h>

#include "as.h"
#include "subsegs.h"

#include "opcode/sparc.h"

#ifdef OBJ_ELF
#include "elf/sparc.h"
#endif

static struct sparc_arch *lookup_arch PARAMS ((char *));
static void init_default_arch PARAMS ((void));
static void sparc_ip PARAMS ((char *, const struct sparc_opcode **));
static int in_signed_range PARAMS ((bfd_signed_vma, bfd_signed_vma));
static int in_unsigned_range PARAMS ((bfd_vma, bfd_vma));
static int in_bitfield_range PARAMS ((bfd_signed_vma, bfd_signed_vma));
static int sparc_ffs PARAMS ((unsigned int));
static bfd_vma BSR PARAMS ((bfd_vma, int));
static int cmp_reg_entry PARAMS ((const PTR, const PTR));
static int parse_keyword_arg PARAMS ((int (*) (const char *), char **, int *));
static int parse_const_expr_arg PARAMS ((char **, int *));
static int get_expression PARAMS ((char *str));

/* Default architecture.  */
/* ??? The default value should be V8, but sparclite support was added
   by making it the default.  GCC now passes -Asparclite, so maybe sometime in
   the future we can set this to V8.  */
#ifndef DEFAULT_ARCH
#define DEFAULT_ARCH "sparclite"
#endif
static char *default_arch = DEFAULT_ARCH;

/* Non-zero if the initial values of `max_architecture' and `sparc_arch_size'
   have been set.  */
static int default_init_p;

/* Current architecture.  We don't bump up unless necessary.  */
static enum sparc_opcode_arch_val current_architecture = SPARC_OPCODE_ARCH_V6;

/* The maximum architecture level we can bump up to.
   In a 32 bit environment, don't allow bumping up to v9 by default.
   The native assembler works this way.  The user is required to pass
   an explicit argument before we'll create v9 object files.  However, if
   we don't see any v9 insns, a v8plus object file is not created.  */
static enum sparc_opcode_arch_val max_architecture;

/* Either 32 or 64, selects file format.  */
static int sparc_arch_size;
/* Initial (default) value, recorded separately in case a user option
   changes the value before md_show_usage is called.  */
static int default_arch_size;

#ifdef OBJ_ELF
/* The currently selected v9 memory model.  Currently only used for
   ELF.  */
static enum { MM_TSO, MM_PSO, MM_RMO } sparc_memory_model = MM_RMO;
#endif

static int architecture_requested;
static int warn_on_bump;

/* If warn_on_bump and the needed architecture is higher than this
   architecture, issue a warning.  */
static enum sparc_opcode_arch_val warn_after_architecture;

/* Non-zero if we are generating PIC code.  */
int sparc_pic_code;

/* Non-zero if we should give an error when misaligned data is seen.  */
static int enforce_aligned_data;

extern int target_big_endian;

/* V9 has big and little endian data, but instructions are always big endian.
   The sparclet has bi-endian support but both data and insns have the same
   endianness.  Global `target_big_endian' is used for data.  The following
   macro is used for instructions.  */
#define INSN_BIG_ENDIAN (target_big_endian \
			 || SPARC_OPCODE_ARCH_V9_P (max_architecture))

/* handle of the OPCODE hash table */
static struct hash_control *op_hash;

static void s_data1 PARAMS ((void));
static void s_seg PARAMS ((int));
static void s_proc PARAMS ((int));
static void s_reserve PARAMS ((int));
static void s_common PARAMS ((int));
static void s_empty PARAMS ((int));
static void s_uacons PARAMS ((int));

const pseudo_typeS md_pseudo_table[] =
{
  {"align", s_align_bytes, 0},	/* Defaulting is invalid (0) */
  {"common", s_common, 0},
  {"empty", s_empty, 0},
  {"global", s_globl, 0},
  {"half", cons, 2},
  {"optim", s_ignore, 0},
  {"proc", s_proc, 0},
  {"reserve", s_reserve, 0},
  {"seg", s_seg, 0},
  {"skip", s_space, 0},
  {"word", cons, 4},
  {"xword", cons, 8},
  {"uahalf", s_uacons, 2},
  {"uaword", s_uacons, 4},
  {"uaxword", s_uacons, 8},
#ifdef OBJ_ELF
  /* these are specific to sparc/svr4 */
  {"pushsection", obj_elf_section, 0},
  {"popsection", obj_elf_previous, 0},
  {"2byte", s_uacons, 2},
  {"4byte", s_uacons, 4},
  {"8byte", s_uacons, 8},
#endif
  {NULL, 0, 0},
};

const int md_reloc_size = 12;	/* Size of relocation record */

/* This array holds the chars that always start a comment.  If the
   pre-processor is disabled, these aren't very useful */
const char comment_chars[] = "!";	/* JF removed '|' from comment_chars */

/* This array holds the chars that only start a comment at the beginning of
   a line.  If the line seems to have the form '# 123 filename'
   .line and .file directives will appear in the pre-processed output */
/* Note that input_file.c hand checks for '#' at the beginning of the
   first line of the input file.  This is because the compiler outputs
   #NO_APP at the beginning of its output. */
/* Also note that comments started like this one will always
   work if '/' isn't otherwise defined. */
const char line_comment_chars[] = "#";

const char line_separator_chars[] = "";

/* Chars that can be used to separate mant from exp in floating point nums */
const char EXP_CHARS[] = "eE";

/* Chars that mean this number is a floating point constant */
/* As in 0f12.456 */
/* or    0d1.2345e12 */
const char FLT_CHARS[] = "rRsSfFdDxXpP";

/* Also be aware that MAXIMUM_NUMBER_OF_CHARS_FOR_FLOAT may have to be
   changed in read.c.  Ideally it shouldn't have to know about it at all,
   but nothing is ideal around here.  */

static unsigned char octal[256];
#define isoctal(c)  octal[(unsigned char) (c)]
static unsigned char toHex[256];

struct sparc_it
  {
    char *error;
    unsigned long opcode;
    struct nlist *nlistp;
    expressionS exp;
    int pcrel;
    bfd_reloc_code_real_type reloc;
  };

struct sparc_it the_insn, set_insn;

static void output_insn
  PARAMS ((const struct sparc_opcode *, struct sparc_it *));

/* Table of arguments to -A.
   The sparc_opcode_arch table in sparc-opc.c is insufficient and incorrect
   for this use.  That table is for opcodes only.  This table is for opcodes
   and file formats.  */

static struct sparc_arch {
  char *name;
  char *opcode_arch;
  /* Default word size, as specified during configuration.
     A value of zero means can't be used to specify default architecture.  */
  int default_arch_size;
  /* Allowable arg to -A?  */
  int user_option_p;
} sparc_arch_table[] = {
  { "v6", "v6", 0, 1 },
  { "v7", "v7", 0, 1 },
  { "v8", "v8", 32, 1 },
  { "sparclet", "sparclet", 32, 1 },
  { "sparclite", "sparclite", 32, 1 },
  { "v8plus", "v9", 0, 1 },
  { "v8plusa", "v9a", 0, 1 },
  { "v9", "v9", 0, 1 },
  { "v9a", "v9a", 0, 1 },
  /* This exists to allow configure.in/Makefile.in to pass one
     value to specify both the default machine and default word size.  */
  { "v9-64", "v9", 64, 0 },
  { NULL, NULL, 0 }
};

static struct sparc_arch *
lookup_arch (name)
     char *name;
{
  struct sparc_arch *sa;

  for (sa = &sparc_arch_table[0]; sa->name != NULL; sa++)
    if (strcmp (sa->name, name) == 0)
      break;
  if (sa->name == NULL)
    return NULL;
  return sa;
}

/* Initialize the default opcode arch and word size from the default
   architecture name.  */

static void
init_default_arch ()
{
  struct sparc_arch *sa = lookup_arch (default_arch);

  if (sa == NULL
      || sa->default_arch_size == 0)
    as_fatal ("Invalid default architecture, broken assembler.");

  max_architecture = sparc_opcode_lookup_arch (sa->opcode_arch);
  if (max_architecture == SPARC_OPCODE_ARCH_BAD)
    as_fatal ("Bad opcode table, broken assembler.");
  default_arch_size = sparc_arch_size = sa->default_arch_size;
  default_init_p = 1;
}

/* Called by TARGET_FORMAT.  */

const char *
sparc_target_format ()
{
  /* We don't get a chance to initialize anything before we're called,
     so handle that now.  */
  if (! default_init_p)
    init_default_arch ();

#ifdef OBJ_AOUT
#ifdef TE_NetBSD
  return "a.out-sparc-netbsd";
#else
#ifdef TE_SPARCAOUT
  return target_big_endian ? "a.out-sunos-big" : "a.out-sparc-little";
#else
  return "a.out-sunos-big";
#endif
#endif
#endif

#ifdef OBJ_BOUT
  return "b.out.big";
#endif

#ifdef OBJ_COFF
#ifdef TE_LYNX
  return "coff-sparc-lynx";
#else
  return "coff-sparc";
#endif
#endif

#ifdef OBJ_ELF
  return sparc_arch_size == 64 ? "elf64-sparc" : "elf32-sparc";
#endif

  abort ();
}

/*
 * md_parse_option
 *	Invocation line includes a switch not recognized by the base assembler.
 *	See if it's a processor-specific option.  These are:
 *
 *	-bump
 *		Warn on architecture bumps.  See also -A.
 *
 *	-Av6, -Av7, -Av8, -Asparclite, -Asparclet
 *		Standard 32 bit architectures.
 *	-Av8plus, -Av8plusa
 *		Sparc64 in a 32 bit world.
 *	-Av9, -Av9a
 *		Sparc64 in either a 32 or 64 bit world (-32/-64 says which).
 *		This used to only mean 64 bits, but properly specifying it
 *		complicated gcc's ASM_SPECs, so now opcode selection is
 *		specified orthogonally to word size (except when specifying
 *		the default, but that is an internal implementation detail).
 *	-xarch=v8plus, -xarch=v8plusa
 *		Same as -Av8plus{,a}, for compatibility with Sun's assembler.
 *
 *		Select the architecture and possibly the file format.
 *		Instructions or features not supported by the selected
 *		architecture cause fatal errors.
 *
 *		The default is to start at v6, and bump the architecture up
 *		whenever an instruction is seen at a higher level.  In 32 bit
 *		environments, v9 is not bumped up to, the user must pass
 * 		-Av8plus{,a}.
 *
 *		If -bump is specified, a warning is printing when bumping to
 *		higher levels.
 *
 *		If an architecture is specified, all instructions must match
 *		that architecture.  Any higher level instructions are flagged
 *		as errors.  Note that in the 32 bit environment specifying
 *		-Av8plus does not automatically create a v8plus object file, a
 *		v9 insn must be seen.
 *
 *		If both an architecture and -bump are specified, the
 *		architecture starts at the specified level, but bumps are
 *		warnings.  Note that we can't set `current_architecture' to
 *		the requested level in this case: in the 32 bit environment,
 *		we still must avoid creating v8plus object files unless v9
 * 		insns are seen.
 *
 * Note:
 *		Bumping between incompatible architectures is always an
 *		error.  For example, from sparclite to v9.
 */

#ifdef OBJ_ELF
CONST char *md_shortopts = "A:K:VQ:sq";
#else
#ifdef OBJ_AOUT
CONST char *md_shortopts = "A:k";
#else
CONST char *md_shortopts = "A:";
#endif
#endif
struct option md_longopts[] = {
#define OPTION_BUMP (OPTION_MD_BASE)
  {"bump", no_argument, NULL, OPTION_BUMP},
#define OPTION_SPARC (OPTION_MD_BASE + 1)
  {"sparc", no_argument, NULL, OPTION_SPARC},
#define OPTION_XARCH (OPTION_MD_BASE + 2)
  {"xarch", required_argument, NULL, OPTION_XARCH},
#ifdef OBJ_ELF
#define OPTION_32 (OPTION_MD_BASE + 3)
  {"32", no_argument, NULL, OPTION_32},
#define OPTION_64 (OPTION_MD_BASE + 4)
  {"64", no_argument, NULL, OPTION_64},
#define OPTION_TSO (OPTION_MD_BASE + 5)
  {"TSO", no_argument, NULL, OPTION_TSO},
#define OPTION_PSO (OPTION_MD_BASE + 6)
  {"PSO", no_argument, NULL, OPTION_PSO},
#define OPTION_RMO (OPTION_MD_BASE + 7)
  {"RMO", no_argument, NULL, OPTION_RMO},
#endif
#ifdef SPARC_BIENDIAN
#define OPTION_LITTLE_ENDIAN (OPTION_MD_BASE + 8)
  {"EL", no_argument, NULL, OPTION_LITTLE_ENDIAN},
#define OPTION_BIG_ENDIAN (OPTION_MD_BASE + 9)
  {"EB", no_argument, NULL, OPTION_BIG_ENDIAN},
#endif
#define OPTION_ENFORCE_ALIGNED_DATA (OPTION_MD_BASE + 10)
  {"enforce-aligned-data", no_argument, NULL, OPTION_ENFORCE_ALIGNED_DATA},
  {NULL, no_argument, NULL, 0}
};
size_t md_longopts_size = sizeof(md_longopts);

int
md_parse_option (c, arg)
     int c;
     char *arg;
{
  /* We don't get a chance to initialize anything before we're called,
     so handle that now.  */
  if (! default_init_p)
    init_default_arch ();

  switch (c)
    {
    case OPTION_BUMP:
      warn_on_bump = 1;
      warn_after_architecture = SPARC_OPCODE_ARCH_V6;
      break;

    case OPTION_XARCH:
      /* This is for compatibility with Sun's assembler.  */
      if (strcmp (arg, "v8plus") != 0
	  && strcmp (arg, "v8plusa") != 0)
	{
	  as_bad ("invalid architecture -xarch=%s", arg);
	  return 0;
	}

      /* fall through */

    case 'A':
      {
	struct sparc_arch *sa;
	enum sparc_opcode_arch_val opcode_arch;

	sa = lookup_arch (arg);
	if (sa == NULL
	    || ! sa->user_option_p)
	  {
	    as_bad ("invalid architecture -A%s", arg);
	    return 0;
	  }

	opcode_arch = sparc_opcode_lookup_arch (sa->opcode_arch);
	if (opcode_arch == SPARC_OPCODE_ARCH_BAD)
	  as_fatal ("Bad opcode table, broken assembler.");

	max_architecture = opcode_arch;
	architecture_requested = 1;
      }
      break;

    case OPTION_SPARC:
      /* Ignore -sparc, used by SunOS make default .s.o rule.  */
      break;

    case OPTION_ENFORCE_ALIGNED_DATA:
      enforce_aligned_data = 1;
      break;

#ifdef SPARC_BIENDIAN
    case OPTION_LITTLE_ENDIAN:
      target_big_endian = 0;
      break;
    case OPTION_BIG_ENDIAN:
      target_big_endian = 1;
      break;
#endif

#ifdef OBJ_AOUT
    case 'k':
      sparc_pic_code = 1;
      break;
#endif

#ifdef OBJ_ELF
    case OPTION_32:
    case OPTION_64:
      {
	const char **list, **l;

	sparc_arch_size = c == OPTION_32 ? 32 : 64;
	list = bfd_target_list ();
	for (l = list; *l != NULL; l++)
	  {
	    if (sparc_arch_size == 32)
	      {
		if (strcmp (*l, "elf32-sparc") == 0)
		  break;
	      }
	    else
	      {
		if (strcmp (*l, "elf64-sparc") == 0)
		  break;
	      }
	  }
	if (*l == NULL)
	  as_fatal ("No compiled in support for %d bit object file format",
		    sparc_arch_size);
	free (list);
      }
      break;

    case OPTION_TSO:
      sparc_memory_model = MM_TSO;
      break;

    case OPTION_PSO:
      sparc_memory_model = MM_PSO;
      break;

    case OPTION_RMO:
      sparc_memory_model = MM_RMO;
      break;

    case 'V':
      print_version_id ();
      break;

    case 'Q':
      /* Qy - do emit .comment
	 Qn - do not emit .comment */
      break;

    case 's':
      /* use .stab instead of .stab.excl */
      break;

    case 'q':
      /* quick -- native assembler does fewer checks */
      break;

    case 'K':
      if (strcmp (arg, "PIC") != 0)
	as_warn ("Unrecognized option following -K");
      else
	sparc_pic_code = 1;
      break;
#endif

    default:
      return 0;
    }

  return 1;
}

void
md_show_usage (stream)
     FILE *stream;
{
  const struct sparc_arch *arch;

  /* We don't get a chance to initialize anything before we're called,
     so handle that now.  */
  if (! default_init_p)
    init_default_arch ();

  fprintf(stream, "SPARC options:\n");
  for (arch = &sparc_arch_table[0]; arch->name; arch++)
    {
      if (arch != &sparc_arch_table[0])
	fprintf (stream, " | ");
      if (arch->user_option_p)
	fprintf (stream, "-A%s", arch->name);
    }
  fprintf (stream, "\n-xarch=v8plus | -xarch=v8plusa\n");
  fprintf (stream, "\
			specify variant of SPARC architecture\n\
-bump			warn when assembler switches architectures\n\
-sparc			ignored\n\
--enforce-aligned-data	force .long, etc., to be aligned correctly\n");
#ifdef OBJ_AOUT
  fprintf (stream, "\
-k			generate PIC\n");
#endif
#ifdef OBJ_ELF
  fprintf (stream, "\
-32			create 32 bit object file\n\
-64			create 64 bit object file\n");
  fprintf (stream, "\
			[default is %d]\n", default_arch_size);
  fprintf (stream, "\
-TSO			use Total Store Ordering\n\
-PSO			use Partial Store Ordering\n\
-RMO			use Relaxed Memory Ordering\n");
  fprintf (stream, "\
			[default is %s]\n", (default_arch_size == 64) ? "RMO" : "TSO");
  fprintf (stream, "\
-KPIC			generate PIC\n\
-V			print assembler version number\n\
-q			ignored\n\
-Qy, -Qn		ignored\n\
-s			ignored\n");
#endif
#ifdef SPARC_BIENDIAN
  fprintf (stream, "\
-EL			generate code for a little endian machine\n\
-EB			generate code for a big endian machine\n");
#endif
}

/* sparc64 priviledged registers */

struct priv_reg_entry
  {
    char *name;
    int regnum;
  };

struct priv_reg_entry priv_reg_table[] =
{
  {"tpc", 0},
  {"tnpc", 1},
  {"tstate", 2},
  {"tt", 3},
  {"tick", 4},
  {"tba", 5},
  {"pstate", 6},
  {"tl", 7},
  {"pil", 8},
  {"cwp", 9},
  {"cansave", 10},
  {"canrestore", 11},
  {"cleanwin", 12},
  {"otherwin", 13},
  {"wstate", 14},
  {"fq", 15},
  {"ver", 31},
  {"", -1},			/* end marker */
};

/* v9a specific asrs */

struct priv_reg_entry v9a_asr_table[] =
{
  {"tick_cmpr", 23},
  {"softint", 22},
  {"set_softint", 20},
  {"pic", 17},
  {"pcr", 16},
  {"gsr", 19},
  {"dcr", 18},
  {"clear_softint", 21},
  {"", -1},			/* end marker */
};

static int
cmp_reg_entry (parg, qarg)
     const PTR parg;
     const PTR qarg;
{
  const struct priv_reg_entry *p = (const struct priv_reg_entry *) parg;
  const struct priv_reg_entry *q = (const struct priv_reg_entry *) qarg;

  return strcmp (q->name, p->name);
}

/* This function is called once, at assembler startup time.  It should
   set up all the tables, etc. that the MD part of the assembler will need. */

void
md_begin ()
{
  register const char *retval = NULL;
  int lose = 0;
  register unsigned int i = 0;

  /* We don't get a chance to initialize anything before md_parse_option
     is called, and it may not be called, so handle default initialization
     now if not already done.  */
  if (! default_init_p)
    init_default_arch ();

  op_hash = hash_new ();

  while (i < (unsigned int) sparc_num_opcodes)
    {
      const char *name = sparc_opcodes[i].name;
      retval = hash_insert (op_hash, name, (PTR) &sparc_opcodes[i]);
      if (retval != NULL)
	{
	  fprintf (stderr, "internal error: can't hash `%s': %s\n",
		   sparc_opcodes[i].name, retval);
	  lose = 1;
	}
      do
	{
	  if (sparc_opcodes[i].match & sparc_opcodes[i].lose)
	    {
	      fprintf (stderr, "internal error: losing opcode: `%s' \"%s\"\n",
		       sparc_opcodes[i].name, sparc_opcodes[i].args);
	      lose = 1;
	    }
	  ++i;
	}
      while (i < (unsigned int) sparc_num_opcodes
	     && !strcmp (sparc_opcodes[i].name, name));
    }

  if (lose)
    as_fatal ("Broken assembler.  No assembly attempted.");

  for (i = '0'; i < '8'; ++i)
    octal[i] = 1;
  for (i = '0'; i <= '9'; ++i)
    toHex[i] = i - '0';
  for (i = 'a'; i <= 'f'; ++i)
    toHex[i] = i + 10 - 'a';
  for (i = 'A'; i <= 'F'; ++i)
    toHex[i] = i + 10 - 'A';

  qsort (priv_reg_table, sizeof (priv_reg_table) / sizeof (priv_reg_table[0]),
	 sizeof (priv_reg_table[0]), cmp_reg_entry);

  /* If -bump, record the architecture level at which we start issuing
     warnings.  The behaviour is different depending upon whether an
     architecture was explicitly specified.  If it wasn't, we issue warnings
     for all upwards bumps.  If it was, we don't start issuing warnings until
     we need to bump beyond the requested architecture or when we bump between
     conflicting architectures.  */

  if (warn_on_bump
      && architecture_requested)
    {
      /* `max_architecture' records the requested architecture.
	 Issue warnings if we go above it.  */
      warn_after_architecture = max_architecture;

      /* Find the highest architecture level that doesn't conflict with
	 the requested one.  */
      for (max_architecture = SPARC_OPCODE_ARCH_MAX;
	   max_architecture > warn_after_architecture;
	   --max_architecture)
	if (! SPARC_OPCODE_CONFLICT_P (max_architecture,
				       warn_after_architecture))
	  break;
    }
}

/* Called after all assembly has been done.  */

void
sparc_md_end ()
{
  if (sparc_arch_size == 64)
    {
      if (current_architecture == SPARC_OPCODE_ARCH_V9A)
	bfd_set_arch_mach (stdoutput, bfd_arch_sparc, bfd_mach_sparc_v9a);
      else
	bfd_set_arch_mach (stdoutput, bfd_arch_sparc, bfd_mach_sparc_v9);
    }
  else
    {
      if (current_architecture == SPARC_OPCODE_ARCH_V9)
	bfd_set_arch_mach (stdoutput, bfd_arch_sparc, bfd_mach_sparc_v8plus);
      else if (current_architecture == SPARC_OPCODE_ARCH_V9A)
	bfd_set_arch_mach (stdoutput, bfd_arch_sparc, bfd_mach_sparc_v8plusa);
      else if (current_architecture == SPARC_OPCODE_ARCH_SPARCLET)
	bfd_set_arch_mach (stdoutput, bfd_arch_sparc, bfd_mach_sparc_sparclet);
      else
	{
	  /* The sparclite is treated like a normal sparc.  Perhaps it shouldn't
	     be but for now it is (since that's the way it's always been
	     treated).  */
	  bfd_set_arch_mach (stdoutput, bfd_arch_sparc, bfd_mach_sparc);
	}
    }
}

/* Return non-zero if VAL is in the range -(MAX+1) to MAX.  */

static INLINE int
in_signed_range (val, max)
     bfd_signed_vma val, max;
{
  if (max <= 0)
    abort ();
  if (val > max)
    return 0;
  if (val < ~max)
    return 0;
  return 1;
}

/* Return non-zero if VAL is in the range 0 to MAX.  */

static INLINE int
in_unsigned_range (val, max)
     bfd_vma val, max;
{
  if (val > max)
    return 0;
  return 1;
}

/* Return non-zero if VAL is in the range -(MAX/2+1) to MAX.
   (e.g. -15 to +31).  */

static INLINE int
in_bitfield_range (val, max)
     bfd_signed_vma val, max;
{
  if (max <= 0)
    abort ();
  if (val > max)
    return 0;
  if (val < ~(max >> 1))
    return 0;
  return 1;
}

static int
sparc_ffs (mask)
     unsigned int mask;
{
  int i;

  if (mask == 0)
    return -1;

  for (i = 0; (mask & 1) == 0; ++i)
    mask >>= 1;
  return i;
}

/* Implement big shift right.  */
static bfd_vma
BSR (val, amount)
     bfd_vma val;
     int amount;
{
  if (sizeof (bfd_vma) <= 4 && amount >= 32)
    as_fatal ("Support for 64-bit arithmetic not compiled in.");
  return val >> amount;
}

/* For communication between sparc_ip and get_expression.  */
static char *expr_end;

/* For communication between md_assemble and sparc_ip.  */
static int special_case;

/* Values for `special_case'.
   Instructions that require wierd handling because they're longer than
   4 bytes.  */
#define SPECIAL_CASE_NONE	0
#define	SPECIAL_CASE_SET	1
#define SPECIAL_CASE_SETSW	2
#define SPECIAL_CASE_SETX	3
/* FIXME: sparc-opc.c doesn't have necessary "S" trigger to enable this.  */
#define	SPECIAL_CASE_FDIV	4

/* Bit masks of various insns.  */
#define NOP_INSN 0x01000000
#define OR_INSN 0x80100000
#define FMOVS_INSN 0x81A00020
#define SETHI_INSN 0x01000000
#define SLLX_INSN 0x81281000
#define SRA_INSN 0x81380000

/* The last instruction to be assembled.  */
static const struct sparc_opcode *last_insn;
/* The assembled opcode of `last_insn'.  */
static unsigned long last_opcode;

/* Main entry point to assemble one instruction.  */

void
md_assemble (str)
     char *str;
{
  const struct sparc_opcode *insn;

  know (str);
  special_case = SPECIAL_CASE_NONE;
  sparc_ip (str, &insn);

  /* We warn about attempts to put a floating point branch in a delay slot,
     unless the delay slot has been annulled.  */
  if (insn != NULL
      && last_insn != NULL
      && (insn->flags & F_FBR) != 0
      && (last_insn->flags & F_DELAYED) != 0
      /* ??? This test isn't completely accurate.  We assume anything with
	 F_{UNBR,CONDBR,FBR} set is annullable.  */
      && ((last_insn->flags & (F_UNBR | F_CONDBR | F_FBR)) == 0
	  || (last_opcode & ANNUL) == 0))
    as_warn ("FP branch in delay slot");

  /* SPARC before v9 requires a nop instruction between a floating
     point instruction and a floating point branch.  We insert one
     automatically, with a warning.  */
  if (max_architecture < SPARC_OPCODE_ARCH_V9
      && insn != NULL
      && last_insn != NULL
      && (insn->flags & F_FBR) != 0
      && (last_insn->flags & F_FLOAT) != 0)
    {
      struct sparc_it nop_insn;

      nop_insn.opcode = NOP_INSN;
      nop_insn.reloc = BFD_RELOC_NONE;
      output_insn (insn, &nop_insn);
      as_warn ("FP branch preceded by FP instruction; NOP inserted");
    }

  switch (special_case)
    {
    case SPECIAL_CASE_NONE:
      /* normal insn */
      output_insn (insn, &the_insn);
      break;

    case SPECIAL_CASE_SET:
      {
	int need_hi22_p = 0;

	/* "set" is not defined for negative numbers in v9: it doesn't yield
	   what you expect it to.  */
	if (SPARC_OPCODE_ARCH_V9_P (max_architecture)
	    && the_insn.exp.X_op == O_constant)
	  {
	    if (the_insn.exp.X_add_number < 0)
	      as_warn ("set: used with negative number");
	    else if (the_insn.exp.X_add_number > (offsetT) 0xffffffff)
	      as_warn ("set: number larger than 4294967295");
	  }

	/* See if operand is absolute and small; skip sethi if so.  */
	if (the_insn.exp.X_op != O_constant
	    || the_insn.exp.X_add_number >= (1 << 12)
	    || the_insn.exp.X_add_number < -(1 << 12))
	  {
	    output_insn (insn, &the_insn);
	    need_hi22_p = 1;
	  }
	/* See if operand has no low-order bits; skip OR if so.  */
	if (the_insn.exp.X_op != O_constant
	    || (need_hi22_p && (the_insn.exp.X_add_number & 0x3FF) != 0)
	    || ! need_hi22_p)
	  {
	    int rd = (the_insn.opcode & RD (~0)) >> 25;
	    the_insn.opcode = (OR_INSN | (need_hi22_p ? RS1 (rd) : 0)
			       | RD (rd)
			       | IMMED
			       | (the_insn.exp.X_add_number
				  & (need_hi22_p ? 0x3ff : 0x1fff)));
	    the_insn.reloc = (the_insn.exp.X_op != O_constant
			      ? BFD_RELOC_LO10
			      : BFD_RELOC_NONE);
	    output_insn (insn, &the_insn);
	  }
	break;
      }

    case SPECIAL_CASE_SETSW:
      {
	/* FIXME: Not finished.  */
	break;
      }

    case SPECIAL_CASE_SETX:
      {
#define SIGNEXT32(x) ((((x) & 0xffffffff) ^ 0x80000000) - 0x80000000)
	int upper32 = SIGNEXT32 (BSR (the_insn.exp.X_add_number, 32));
	int lower32 = SIGNEXT32 (the_insn.exp.X_add_number);
#undef SIGNEXT32
	int tmpreg = (the_insn.opcode & RS1 (~0)) >> 14;
	int dstreg = (the_insn.opcode & RD (~0)) >> 25;
	/* Output directly to dst reg if lower 32 bits are all zero.  */
	int upper_dstreg = (the_insn.exp.X_op == O_constant
			    && lower32 == 0) ? dstreg : tmpreg;
	int need_hh22_p = 0, need_hm10_p = 0, need_hi22_p = 0, need_lo10_p = 0;

	/* The tmp reg should not be the dst reg.  */
	if (tmpreg == dstreg)
	  as_warn ("setx: temporary register same as destination register");

	/* Reset X_add_number, we've extracted it as upper32/lower32.
	   Otherwise fixup_segment will complain about not being able to
	   write an 8 byte number in a 4 byte field.  */
	the_insn.exp.X_add_number = 0;

	/* ??? Obviously there are other optimizations we can do
	   (e.g. sethi+shift for 0x1f0000000) and perhaps we shouldn't be
	   doing some of these.  Later.  If you do change things, try to
	   change all of this to be table driven as well.  */

	/* What to output depends on the number if it's constant.
	   Compute that first, then output what we've decided upon.  */
	if (the_insn.exp.X_op != O_constant)
	  need_hh22_p = need_hm10_p = need_hi22_p = need_lo10_p = 1;
	else
	  {
	    /* Only need hh22 if `or' insn can't handle constant.  */
	    if (upper32 < -(1 << 12) || upper32 >= (1 << 12))
	      need_hh22_p = 1;

	    /* Does bottom part (after sethi) have bits?  */
	    if ((need_hh22_p && (upper32 & 0x3ff) != 0)
		/* No hh22, but does upper32 still have bits we can't set
		   from lower32?  */
		|| (! need_hh22_p
		    && upper32 != 0
		    && (upper32 != -1 || lower32 >= 0)))
	      need_hm10_p = 1;

	    /* If the lower half is all zero, we build the upper half directly
	       into the dst reg.  */
	    if (lower32 != 0
		/* Need lower half if number is zero.  */
		|| (! need_hh22_p && ! need_hm10_p))
	      {
		/* No need for sethi if `or' insn can handle constant.  */
		if (lower32 < -(1 << 12) || lower32 >= (1 << 12)
		    /* Note that we can't use a negative constant in the `or'
		       insn unless the upper 32 bits are all ones.  */
		    || (lower32 < 0 && upper32 != -1))
		  need_hi22_p = 1;

		/* Does bottom part (after sethi) have bits?  */
		if ((need_hi22_p && (lower32 & 0x3ff) != 0)
		    /* No sethi.  */
		    || (! need_hi22_p && (lower32 & 0x1fff) != 0)
		    /* Need `or' if we didn't set anything else.  */
		    || (! need_hi22_p && ! need_hh22_p && ! need_hm10_p))
		  need_lo10_p = 1;
	      }
	  }

	if (need_hh22_p)
	  {
	    the_insn.opcode = (SETHI_INSN | RD (upper_dstreg)
			       | ((upper32 >> 10) & 0x3fffff));
	    the_insn.reloc = (the_insn.exp.X_op != O_constant
			      ? BFD_RELOC_SPARC_HH22 : BFD_RELOC_NONE);
	    output_insn (insn, &the_insn);
	  }

	if (need_hm10_p)
	  {
	    the_insn.opcode = (OR_INSN
			       | (need_hh22_p ? RS1 (upper_dstreg) : 0)
			       | RD (upper_dstreg)
			       | IMMED
			       | (upper32
				  & (need_hh22_p ? 0x3ff : 0x1fff)));
	    the_insn.reloc = (the_insn.exp.X_op != O_constant
			      ? BFD_RELOC_SPARC_HM10 : BFD_RELOC_NONE);
	    output_insn (insn, &the_insn);
	  }

	if (need_hi22_p)
	  {
	    the_insn.opcode = (SETHI_INSN | RD (dstreg)
			       | ((lower32 >> 10) & 0x3fffff));
	    the_insn.reloc = BFD_RELOC_HI22;
	    output_insn (insn, &the_insn);
	  }

	if (need_lo10_p)
	  {
	    /* FIXME: One nice optimization to do here is to OR the low part
	       with the highpart if hi22 isn't needed and the low part is
	       positive.  */
	    the_insn.opcode = (OR_INSN | (need_hi22_p ? RS1 (dstreg) : 0)
			       | RD (dstreg)
			       | IMMED
			       | (lower32
				  & (need_hi22_p ? 0x3ff : 0x1fff)));
	    the_insn.reloc = BFD_RELOC_LO10;
	    output_insn (insn, &the_insn);
	  }

	/* If we needed to build the upper part, shift it into place.  */
	if (need_hh22_p || need_hm10_p)
	  {
	    the_insn.opcode = (SLLX_INSN | RS1 (upper_dstreg) | RD (upper_dstreg)
			       | IMMED | 32);
	    the_insn.reloc = BFD_RELOC_NONE;
	    output_insn (insn, &the_insn);
	  }

	/* If we needed to build both upper and lower parts, OR them together.  */
	if ((need_hh22_p || need_hm10_p)
	    && (need_hi22_p || need_lo10_p))
	  {
	    the_insn.opcode = (OR_INSN | RS1 (dstreg) | RS2 (upper_dstreg)
			       | RD (dstreg));
	    the_insn.reloc = BFD_RELOC_NONE;
	    output_insn (insn, &the_insn);
	  }
	/* We didn't need both regs, but we may have to sign extend lower32.  */
	else if (need_hi22_p && upper32 == -1)
	  {
	    the_insn.opcode = (SRA_INSN | RS1 (dstreg) | RD (dstreg)
			       | IMMED | 0);
	    the_insn.reloc = BFD_RELOC_NONE;
	    output_insn (insn, &the_insn);
	  }
	break;
      }

    case SPECIAL_CASE_FDIV:
      {
	int rd = (the_insn.opcode >> 25) & 0x1f;

	output_insn (insn, &the_insn);

	/* According to information leaked from Sun, the "fdiv" instructions
	   on early SPARC machines would produce incorrect results sometimes.
	   The workaround is to add an fmovs of the destination register to
	   itself just after the instruction.  This was true on machines
	   with Weitek 1165 float chips, such as the Sun-4/260 and /280. */
	assert (the_insn.reloc == BFD_RELOC_NONE);
	the_insn.opcode = FMOVS_INSN | rd | RD (rd);
	output_insn (insn, &the_insn);
	break;
      }

    default:
      as_fatal ("failed special case insn sanity check");
    }
}

/* Subroutine of md_assemble to do the actual parsing.  */

static void
sparc_ip (str, pinsn)
     char *str;
     const struct sparc_opcode **pinsn;
{
  char *error_message = "";
  char *s;
  const char *args;
  char c;
  const struct sparc_opcode *insn;
  char *argsStart;
  unsigned long opcode;
  unsigned int mask = 0;
  int match = 0;
  int comma = 0;
  int v9_arg_p;

  for (s = str; islower ((unsigned char) *s) || (*s >= '0' && *s <= '3'); ++s)
    ;

  switch (*s)
    {
    case '\0':
      break;

    case ',':
      comma = 1;

      /*FALLTHROUGH */

    case ' ':
      *s++ = '\0';
      break;

    default:
      as_fatal ("Unknown opcode: `%s'", str);
    }
  insn = (struct sparc_opcode *) hash_find (op_hash, str);
  *pinsn = insn;
  if (insn == NULL)
    {
      as_bad ("Unknown opcode: `%s'", str);
      return;
    }
  if (comma)
    {
      *--s = ',';
    }

  argsStart = s;
  for (;;)
    {
      opcode = insn->match;
      memset (&the_insn, '\0', sizeof (the_insn));
      the_insn.reloc = BFD_RELOC_NONE;
      v9_arg_p = 0;

      /*
       * Build the opcode, checking as we go to make
       * sure that the operands match
       */
      for (args = insn->args;; ++args)
	{
	  switch (*args)
	    {
	    case 'K':
	      {
		int kmask = 0;

		/* Parse a series of masks.  */
		if (*s == '#')
		  {
		    while (*s == '#')
		      {
			int mask;

			if (! parse_keyword_arg (sparc_encode_membar, &s,
						 &mask))
			  {
			    error_message = ": invalid membar mask name";
			    goto error;
			  }
			kmask |= mask;
			while (*s == ' ') { ++s; continue; }
			if (*s == '|' || *s == '+')
			  ++s;
			while (*s == ' ') { ++s; continue; }
		      }
		  }
		else
		  {
		    if (! parse_const_expr_arg (&s, &kmask))
		      {
			error_message = ": invalid membar mask expression";
			goto error;
		      }
		    if (kmask < 0 || kmask > 127)
		      {
			error_message = ": invalid membar mask number";
			goto error;
		      }
		  }

		opcode |= MEMBAR (kmask);
		continue;
	      }

	    case '*':
	      {
		int fcn = 0;

		/* Parse a prefetch function.  */
		if (*s == '#')
		  {
		    if (! parse_keyword_arg (sparc_encode_prefetch, &s, &fcn))
		      {
			error_message = ": invalid prefetch function name";
			goto error;
		      }
		  }
		else
		  {
		    if (! parse_const_expr_arg (&s, &fcn))
		      {
			error_message = ": invalid prefetch function expression";
			goto error;
		      }
		    if (fcn < 0 || fcn > 31)
		      {
			error_message = ": invalid prefetch function number";
			goto error;
		      }
		  }
		opcode |= RD (fcn);
		continue;
	      }

	    case '!':
	    case '?':
	      /* Parse a sparc64 privileged register.  */
	      if (*s == '%')
		{
		  struct priv_reg_entry *p = priv_reg_table;
		  unsigned int len = 9999999; /* init to make gcc happy */

		  s += 1;
		  while (p->name[0] > s[0])
		    p++;
		  while (p->name[0] == s[0])
		    {
		      len = strlen (p->name);
		      if (strncmp (p->name, s, len) == 0)
			break;
		      p++;
		    }
		  if (p->name[0] != s[0])
		    {
		      error_message = ": unrecognizable privileged register";
		      goto error;
		    }
		  if (*args == '?')
		    opcode |= (p->regnum << 14);
		  else
		    opcode |= (p->regnum << 25);
		  s += len;
		  continue;
		}
	      else
		{
		  error_message = ": unrecognizable privileged register";
		  goto error;
		}

	    case '_':
	    case '/':
	      /* Parse a v9a ancillary state register.  */
	      if (*s == '%')
		{
		  struct priv_reg_entry *p = v9a_asr_table;
		  unsigned int len = 9999999; /* init to make gcc happy */

		  s += 1;
		  while (p->name[0] > s[0])
		    p++;
		  while (p->name[0] == s[0])
		    {
		      len = strlen (p->name);
		      if (strncmp (p->name, s, len) == 0)
			break;
		      p++;
		    }
		  if (p->name[0] != s[0])
		    {
		      error_message = ": unrecognizable v9a ancillary state register";
		      goto error;
		    }
		  if (*args == '/' && (p->regnum == 20 || p->regnum == 21))
		    {
		      error_message = ": rd on write only ancillary state register";
		      goto error;
		    }		      
		  if (*args == '/')
		    opcode |= (p->regnum << 14);
		  else
		    opcode |= (p->regnum << 25);
		  s += len;
		  continue;
		}
	      else
		{
		  error_message = ": unrecognizable v9a ancillary state register";
		  goto error;
		}

	    case 'M':
	    case 'm':
	      if (strncmp (s, "%asr", 4) == 0)
		{
		  s += 4;

		  if (isdigit ((unsigned char) *s))
		    {
		      long num = 0;

		      while (isdigit ((unsigned char) *s))
			{
			  num = num * 10 + *s - '0';
			  ++s;
			}

		      if (current_architecture >= SPARC_OPCODE_ARCH_V9)
			{
			  if (num < 16 || 31 < num)
			    {
			      error_message = ": asr number must be between 16 and 31";
			      goto error;
			    }
			}
		      else
			{
			  if (num < 0 || 31 < num)
			    {
			      error_message = ": asr number must be between 0 and 31";
			      goto error;
			    }
			}

		      opcode |= (*args == 'M' ? RS1 (num) : RD (num));
		      continue;
		    }
		  else
		    {
		      error_message = ": expecting %asrN";
		      goto error;
		    }
		} /* if %asr */
	      break;

	    case 'I':
	      the_insn.reloc = BFD_RELOC_SPARC_11;
	      goto immediate;

	    case 'j':
	      the_insn.reloc = BFD_RELOC_SPARC_10;
	      goto immediate;

	    case 'X':
	      /* V8 systems don't understand BFD_RELOC_SPARC_5.  */
	      if (SPARC_OPCODE_ARCH_V9_P (max_architecture))
		the_insn.reloc = BFD_RELOC_SPARC_5;
	      else
		the_insn.reloc = BFD_RELOC_SPARC13;
	      /* These fields are unsigned, but for upward compatibility,
		 allow negative values as well.  */
	      goto immediate;

	    case 'Y':
	      /* V8 systems don't understand BFD_RELOC_SPARC_6.  */
	      if (SPARC_OPCODE_ARCH_V9_P (max_architecture))
		the_insn.reloc = BFD_RELOC_SPARC_6;
	      else
		the_insn.reloc = BFD_RELOC_SPARC13;
	      /* These fields are unsigned, but for upward compatibility,
		 allow negative values as well.  */
	      goto immediate;

	    case 'k':
	      the_insn.reloc = /* RELOC_WDISP2_14 */ BFD_RELOC_SPARC_WDISP16;
	      the_insn.pcrel = 1;
	      goto immediate;

	    case 'G':
	      the_insn.reloc = BFD_RELOC_SPARC_WDISP19;
	      the_insn.pcrel = 1;
	      goto immediate;

	    case 'N':
	      if (*s == 'p' && s[1] == 'n')
		{
		  s += 2;
		  continue;
		}
	      break;

	    case 'T':
	      if (*s == 'p' && s[1] == 't')
		{
		  s += 2;
		  continue;
		}
	      break;

	    case 'z':
	      if (*s == ' ')
		{
		  ++s;
		}
	      if (strncmp (s, "%icc", 4) == 0)
		{
		  s += 4;
		  continue;
		}
	      break;

	    case 'Z':
	      if (*s == ' ')
		{
		  ++s;
		}
	      if (strncmp (s, "%xcc", 4) == 0)
		{
		  s += 4;
		  continue;
		}
	      break;

	    case '6':
	      if (*s == ' ')
		{
		  ++s;
		}
	      if (strncmp (s, "%fcc0", 5) == 0)
		{
		  s += 5;
		  continue;
		}
	      break;

	    case '7':
	      if (*s == ' ')
		{
		  ++s;
		}
	      if (strncmp (s, "%fcc1", 5) == 0)
		{
		  s += 5;
		  continue;
		}
	      break;

	    case '8':
	      if (*s == ' ')
		{
		  ++s;
		}
	      if (strncmp (s, "%fcc2", 5) == 0)
		{
		  s += 5;
		  continue;
		}
	      break;

	    case '9':
	      if (*s == ' ')
		{
		  ++s;
		}
	      if (strncmp (s, "%fcc3", 5) == 0)
		{
		  s += 5;
		  continue;
		}
	      break;

	    case 'P':
	      if (strncmp (s, "%pc", 3) == 0)
		{
		  s += 3;
		  continue;
		}
	      break;

	    case 'W':
	      if (strncmp (s, "%tick", 5) == 0)
		{
		  s += 5;
		  continue;
		}
	      break;

	    case '\0':		/* end of args */
	      if (*s == '\0')
		{
		  match = 1;
		}
	      break;

	    case '+':
	      if (*s == '+')
		{
		  ++s;
		  continue;
		}
	      if (*s == '-')
		{
		  continue;
		}
	      break;

	    case '[':		/* these must match exactly */
	    case ']':
	    case ',':
	    case ' ':
	      if (*s++ == *args)
		continue;
	      break;

	    case '#':		/* must be at least one digit */
	      if (isdigit ((unsigned char) *s++))
		{
		  while (isdigit ((unsigned char) *s))
		    {
		      ++s;
		    }
		  continue;
		}
	      break;

	    case 'C':		/* coprocessor state register */
	      if (strncmp (s, "%csr", 4) == 0)
		{
		  s += 4;
		  continue;
		}
	      break;

	    case 'b':		/* next operand is a coprocessor register */
	    case 'c':
	    case 'D':
	      if (*s++ == '%' && *s++ == 'c' && isdigit ((unsigned char) *s))
		{
		  mask = *s++;
		  if (isdigit ((unsigned char) *s))
		    {
		      mask = 10 * (mask - '0') + (*s++ - '0');
		      if (mask >= 32)
			{
			  break;
			}
		    }
		  else
		    {
		      mask -= '0';
		    }
		  switch (*args)
		    {

		    case 'b':
		      opcode |= mask << 14;
		      continue;

		    case 'c':
		      opcode |= mask;
		      continue;

		    case 'D':
		      opcode |= mask << 25;
		      continue;
		    }
		}
	      break;

	    case 'r':		/* next operand must be a register */
	    case 'O':
	    case '1':
	    case '2':
	    case 'd':
	      if (*s++ == '%')
		{
		  switch (c = *s++)
		    {

		    case 'f':	/* frame pointer */
		      if (*s++ == 'p')
			{
			  mask = 0x1e;
			  break;
			}
		      goto error;

		    case 'g':	/* global register */
		      if (isoctal (c = *s++))
			{
			  mask = c - '0';
			  break;
			}
		      goto error;

		    case 'i':	/* in register */
		      if (isoctal (c = *s++))
			{
			  mask = c - '0' + 24;
			  break;
			}
		      goto error;

		    case 'l':	/* local register */
		      if (isoctal (c = *s++))
			{
			  mask = (c - '0' + 16);
			  break;
			}
		      goto error;

		    case 'o':	/* out register */
		      if (isoctal (c = *s++))
			{
			  mask = (c - '0' + 8);
			  break;
			}
		      goto error;

		    case 's':	/* stack pointer */
		      if (*s++ == 'p')
			{
			  mask = 0xe;
			  break;
			}
		      goto error;

		    case 'r':	/* any register */
		      if (!isdigit ((unsigned char) (c = *s++)))
			{
			  goto error;
			}
		      /* FALLTHROUGH */
		    case '0':
		    case '1':
		    case '2':
		    case '3':
		    case '4':
		    case '5':
		    case '6':
		    case '7':
		    case '8':
		    case '9':
		      if (isdigit ((unsigned char) *s))
			{
			  if ((c = 10 * (c - '0') + (*s++ - '0')) >= 32)
			    {
			      goto error;
			    }
			}
		      else
			{
			  c -= '0';
			}
		      mask = c;
		      break;

		    default:
		      goto error;
		    }

		  /* Got the register, now figure out where
		     it goes in the opcode.  */
		  switch (*args)
		    {
		    case '1':
		      opcode |= mask << 14;
		      continue;

		    case '2':
		      opcode |= mask;
		      continue;

		    case 'd':
		      opcode |= mask << 25;
		      continue;

		    case 'r':
		      opcode |= (mask << 25) | (mask << 14);
		      continue;

		    case 'O':
		      opcode |= (mask << 25) | (mask << 0);
		      continue;
		    }
		}
	      break;

	    case 'e':		/* next operand is a floating point register */
	    case 'v':
	    case 'V':

	    case 'f':
	    case 'B':
	    case 'R':

	    case 'g':
	    case 'H':
	    case 'J':
	      {
		char format;

		if (*s++ == '%'
		    && ((format = *s) == 'f')
		    && isdigit ((unsigned char) *++s))
		  {
		    for (mask = 0; isdigit ((unsigned char) *s); ++s)
		      {
			mask = 10 * mask + (*s - '0');
		      }		/* read the number */

		    if ((*args == 'v'
			 || *args == 'B'
			 || *args == 'H')
			&& (mask & 1))
		      {
			break;
		      }		/* register must be even numbered */

		    if ((*args == 'V'
			 || *args == 'R'
			 || *args == 'J')
			&& (mask & 3))
		      {
			break;
		      }		/* register must be multiple of 4 */

		    if (mask >= 64)
		      {
			if (SPARC_OPCODE_ARCH_V9_P (max_architecture))
			  error_message = ": There are only 64 f registers; [0-63]";
			else
			  error_message = ": There are only 32 f registers; [0-31]";
			goto error;
		      }	/* on error */
		    else if (mask >= 32)
		      {
			if (SPARC_OPCODE_ARCH_V9_P (max_architecture))
			  {
			    v9_arg_p = 1;
			    mask -= 31;	/* wrap high bit */
			  }
			else
			  {
			    error_message = ": There are only 32 f registers; [0-31]";
			    goto error;
			  }
		      }
		  }
		else
		  {
		    break;
		  }	/* if not an 'f' register. */

		switch (*args)
		  {
		  case 'v':
		  case 'V':
		  case 'e':
		    opcode |= RS1 (mask);
		    continue;


		  case 'f':
		  case 'B':
		  case 'R':
		    opcode |= RS2 (mask);
		    continue;

		  case 'g':
		  case 'H':
		  case 'J':
		    opcode |= RD (mask);
		    continue;
		  }		/* pack it in. */

		know (0);
		break;
	      }			/* float arg */

	    case 'F':
	      if (strncmp (s, "%fsr", 4) == 0)
		{
		  s += 4;
		  continue;
		}
	      break;

	    case '0':		/* 64 bit immediate (setx insn) */
	      the_insn.reloc = BFD_RELOC_NONE; /* reloc handled elsewhere */
	      goto immediate;

	    case 'h':		/* high 22 bits */
	      the_insn.reloc = BFD_RELOC_HI22;
	      goto immediate;

	    case 'l':		/* 22 bit PC relative immediate */
	      the_insn.reloc = BFD_RELOC_SPARC_WDISP22;
	      the_insn.pcrel = 1;
	      goto immediate;

	    case 'L':		/* 30 bit immediate */
	      the_insn.reloc = BFD_RELOC_32_PCREL_S2;
	      the_insn.pcrel = 1;
	      goto immediate;

	    case 'n':		/* 22 bit immediate */
	      the_insn.reloc = BFD_RELOC_SPARC22;
	      goto immediate;

	    case 'i':		/* 13 bit immediate */
	      the_insn.reloc = BFD_RELOC_SPARC13;

	      /* fallthrough */

	    immediate:
	      if (*s == ' ')
		s++;

	      /* Check for %hi, etc.  */
	      if (*s == '%')
		{
		  static struct ops {
		    /* The name as it appears in assembler.  */
		    char *name;
		    /* strlen (name), precomputed for speed */
		    int len;
		    /* The reloc this pseudo-op translates to.  */
		    int reloc;
		    /* Non-zero if for v9 only.  */
		    int v9_p;
		    /* Non-zero if can be used in pc-relative contexts.  */
		    int pcrel_p;/*FIXME:wip*/
		  } ops[] = {
		    /* hix/lox must appear before hi/lo so %hix won't be
		       mistaken for %hi.  */
		    { "hix", 3, BFD_RELOC_SPARC_HIX22, 1, 0 },
		    { "lox", 3, BFD_RELOC_SPARC_LOX10, 1, 0 },
		    { "hi", 2, BFD_RELOC_HI22, 0, 1 },
		    { "lo", 2, BFD_RELOC_LO10, 0, 1 },
		    { "hh", 2, BFD_RELOC_SPARC_HH22, 1, 1 },
		    { "hm", 2, BFD_RELOC_SPARC_HM10, 1, 1 },
		    { "lm", 2, BFD_RELOC_SPARC_LM22, 1, 1 },
		    { "h44", 3, BFD_RELOC_SPARC_H44, 1, 0 },
		    { "m44", 3, BFD_RELOC_SPARC_M44, 1, 0 },
		    { "l44", 3, BFD_RELOC_SPARC_L44, 1, 0 },
		    { "uhi", 3, BFD_RELOC_SPARC_HH22, 1, 0 },
		    { "ulo", 3, BFD_RELOC_SPARC_HM10, 1, 0 },
		    { NULL }
		  };
		  struct ops *o;

		  for (o = ops; o->name; o++)
		    if (strncmp (s + 1, o->name, o->len) == 0)
		      break;
		  if (o->name == NULL)
		    break;

		  the_insn.reloc = o->reloc;
		  s += o->len + 1;
		  v9_arg_p = o->v9_p;
		}

	      /* Note that if the get_expression() fails, we will still
		 have created U entries in the symbol table for the
		 'symbols' in the input string.  Try not to create U
		 symbols for registers, etc.  */
	      {
		/* This stuff checks to see if the expression ends in
		   +%reg.  If it does, it removes the register from
		   the expression, and re-sets 's' to point to the
		   right place.  */

		char *s1;

		for (s1 = s; *s1 && *s1 != ',' && *s1 != ']'; s1++) ;

		if (s1 != s && isdigit ((unsigned char) s1[-1]))
		  {
		    if (s1[-2] == '%' && s1[-3] == '+')
		      {
			s1 -= 3;
			*s1 = '\0';
			(void) get_expression (s);
			*s1 = '+';
			s = s1;
			continue;
		      }
		    else if (strchr ("goli0123456789", s1[-2]) && s1[-3] == '%' && s1[-4] == '+')
		      {
			s1 -= 4;
			*s1 = '\0';
			(void) get_expression (s);
			*s1 = '+';
			s = s1;
			continue;
		      }
		  }
	      }
	      (void) get_expression (s);
	      s = expr_end;

	      /* Check for constants that don't require emitting a reloc.  */
	      if (the_insn.exp.X_op == O_constant
		  && the_insn.exp.X_add_symbol == 0
		  && the_insn.exp.X_op_symbol == 0)
		{
		  /* For pc-relative call instructions, we reject
		     constants to get better code.  */
		  if (the_insn.pcrel
		      && the_insn.reloc == BFD_RELOC_32_PCREL_S2
		      && in_signed_range (the_insn.exp.X_add_number, 0x3fff))
		    {
		      error_message = ": PC-relative operand can't be a constant";
		      goto error;
		    }

		  /* Constants that won't fit are checked in md_apply_fix3
		     and bfd_install_relocation.
		     ??? It would be preferable to install the constants
		     into the insn here and save having to create a fixS
		     for each one.  There already exists code to handle
		     all the various cases (e.g. in md_apply_fix3 and
		     bfd_install_relocation) so duplicating all that code
		     here isn't right.  */
		}

	      continue;

	    case 'a':
	      if (*s++ == 'a')
		{
		  opcode |= ANNUL;
		  continue;
		}
	      break;

	    case 'A':
	      {
		int asi = 0;

		/* Parse an asi.  */
		if (*s == '#')
		  {
		    if (! parse_keyword_arg (sparc_encode_asi, &s, &asi))
		      {
			error_message = ": invalid ASI name";
			goto error;
		      }
		  }
		else
		  {
		    if (! parse_const_expr_arg (&s, &asi))
		      {
			error_message = ": invalid ASI expression";
			goto error;
		      }
		    if (asi < 0 || asi > 255)
		      {
			error_message = ": invalid ASI number";
			goto error;
		      }
		  }
		opcode |= ASI (asi);
		continue;
	      }			/* alternate space */

	    case 'p':
	      if (strncmp (s, "%psr", 4) == 0)
		{
		  s += 4;
		  continue;
		}
	      break;

	    case 'q':		/* floating point queue */
	      if (strncmp (s, "%fq", 3) == 0)
		{
		  s += 3;
		  continue;
		}
	      break;

	    case 'Q':		/* coprocessor queue */
	      if (strncmp (s, "%cq", 3) == 0)
		{
		  s += 3;
		  continue;
		}
	      break;

	    case 'S':
	      if (strcmp (str, "set") == 0
		  || strcmp (str, "setuw") == 0)
		{
		  special_case = SPECIAL_CASE_SET;
		  continue;
		}
	      else if (strcmp (str, "setsw") == 0)
		{
		  special_case = SPECIAL_CASE_SETSW;
		  continue;
		}
	      else if (strcmp (str, "setx") == 0)
		{
		  special_case = SPECIAL_CASE_SETX;
		  continue;
		}
	      else if (strncmp (str, "fdiv", 4) == 0)
		{
		  special_case = SPECIAL_CASE_FDIV;
		  continue;
		}
	      break;

	    case 'o':
	      if (strncmp (s, "%asi", 4) != 0)
		break;
	      s += 4;
	      continue;

	    case 's':
	      if (strncmp (s, "%fprs", 5) != 0)
		break;
	      s += 5;
	      continue;

	    case 'E':
	      if (strncmp (s, "%ccr", 4) != 0)
		break;
	      s += 4;
	      continue;

	    case 't':
	      if (strncmp (s, "%tbr", 4) != 0)
		break;
	      s += 4;
	      continue;

	    case 'w':
	      if (strncmp (s, "%wim", 4) != 0)
		break;
	      s += 4;
	      continue;

	    case 'x':
	      {
		char *push = input_line_pointer;
		expressionS e;

		input_line_pointer = s;
		expression (&e);
		if (e.X_op == O_constant)
		  {
		    int n = e.X_add_number;
		    if (n != e.X_add_number || (n & ~0x1ff) != 0)
		      as_bad ("OPF immediate operand out of range (0-0x1ff)");
		    else
		      opcode |= e.X_add_number << 5;
		  }
		else
		  as_bad ("non-immediate OPF operand, ignored");
		s = input_line_pointer;
		input_line_pointer = push;
		continue;
	      }

	    case 'y':
	      if (strncmp (s, "%y", 2) != 0)
		break;
	      s += 2;
	      continue;

	    case 'u':
	    case 'U':
	      {
		/* Parse a sparclet cpreg.  */
		int cpreg;
		if (! parse_keyword_arg (sparc_encode_sparclet_cpreg, &s, &cpreg))
		  {
		    error_message = ": invalid cpreg name";
		    goto error;
		  }
		opcode |= (*args == 'U' ? RS1 (cpreg) : RD (cpreg));
		continue;
	      }

	    default:
	      as_fatal ("failed sanity check.");
	    }			/* switch on arg code */

	  /* Break out of for() loop.  */
	  break;
	}			/* for each arg that we expect */

    error:
      if (match == 0)
	{
	  /* Args don't match. */
	  if (&insn[1] - sparc_opcodes < sparc_num_opcodes
	      && (insn->name == insn[1].name
		  || !strcmp (insn->name, insn[1].name)))
	    {
	      ++insn;
	      s = argsStart;
	      continue;
	    }
	  else
	    {
	      as_bad ("Illegal operands%s", error_message);
	      return;
	    }
	}
      else
	{
	  /* We have a match.  Now see if the architecture is ok.  */
	  int needed_arch_mask = insn->architecture;

	  if (v9_arg_p)
	    {
	      needed_arch_mask &= ~ ((1 << SPARC_OPCODE_ARCH_V9)
				     | (1 << SPARC_OPCODE_ARCH_V9A));
	      needed_arch_mask |= (1 << SPARC_OPCODE_ARCH_V9);
	    }

	  if (needed_arch_mask & SPARC_OPCODE_SUPPORTED (current_architecture))
	    ; /* ok */
	  /* Can we bump up the architecture?  */
	  else if (needed_arch_mask & SPARC_OPCODE_SUPPORTED (max_architecture))
	    {
	      enum sparc_opcode_arch_val needed_architecture =
		sparc_ffs (SPARC_OPCODE_SUPPORTED (max_architecture)
			   & needed_arch_mask);

	      assert (needed_architecture <= SPARC_OPCODE_ARCH_MAX);
	      if (warn_on_bump
		  && needed_architecture > warn_after_architecture)
		{
		  as_warn ("architecture bumped from \"%s\" to \"%s\" on \"%s\"",
			   sparc_opcode_archs[current_architecture].name,
			   sparc_opcode_archs[needed_architecture].name,
			   str);
		  warn_after_architecture = needed_architecture;
		}
	      current_architecture = needed_architecture;
	    }
	  /* Conflict.  */
	  /* ??? This seems to be a bit fragile.  What if the next entry in
	     the opcode table is the one we want and it is supported?
	     It is possible to arrange the table today so that this can't
	     happen but what about tomorrow?  */
	  else
	    {
	      int arch,printed_one_p = 0;
	      char *p;
	      char required_archs[SPARC_OPCODE_ARCH_MAX * 16];

	      /* Create a list of the architectures that support the insn.  */
	      needed_arch_mask &= ~ SPARC_OPCODE_SUPPORTED (max_architecture);
	      p = required_archs;
	      arch = sparc_ffs (needed_arch_mask);
	      while ((1 << arch) <= needed_arch_mask)
		{
		  if ((1 << arch) & needed_arch_mask)
		    {
		      if (printed_one_p)
			*p++ = '|';
		      strcpy (p, sparc_opcode_archs[arch].name);
		      p += strlen (p);
		      printed_one_p = 1;
		    }
		  ++arch;
		}

	      as_bad ("Architecture mismatch on \"%s\".", str);
	      as_tsktsk (" (Requires %s; requested architecture is %s.)",
			 required_archs,
			 sparc_opcode_archs[max_architecture].name);
	      return;
	    }
	} /* if no match */

      break;
    } /* forever looking for a match */

  the_insn.opcode = opcode;
}

/* Parse an argument that can be expressed as a keyword.
   (eg: #StoreStore or %ccfr).
   The result is a boolean indicating success.
   If successful, INPUT_POINTER is updated.  */

static int
parse_keyword_arg (lookup_fn, input_pointerP, valueP)
     int (*lookup_fn) PARAMS ((const char *));
     char **input_pointerP;
     int *valueP;
{
  int value;
  char c, *p, *q;

  p = *input_pointerP;
  for (q = p + (*p == '#' || *p == '%');
       isalnum ((unsigned char) *q) || *q == '_';
       ++q)
    continue;
  c = *q;
  *q = 0;
  value = (*lookup_fn) (p);
  *q = c;
  if (value == -1)
    return 0;
  *valueP = value;
  *input_pointerP = q;
  return 1;
}

/* Parse an argument that is a constant expression.
   The result is a boolean indicating success.  */

static int
parse_const_expr_arg (input_pointerP, valueP)
     char **input_pointerP;
     int *valueP;
{
  char *save = input_line_pointer;
  expressionS exp;

  input_line_pointer = *input_pointerP;
  /* The next expression may be something other than a constant
     (say if we're not processing the right variant of the insn).
     Don't call expression unless we're sure it will succeed as it will
     signal an error (which we want to defer until later).  */
  /* FIXME: It might be better to define md_operand and have it recognize
     things like %asi, etc. but continuing that route through to the end
     is a lot of work.  */
  if (*input_line_pointer == '%')
    {
      input_line_pointer = save;
      return 0;
    }
  expression (&exp);
  *input_pointerP = input_line_pointer;
  input_line_pointer = save;
  if (exp.X_op != O_constant)
    return 0;
  *valueP = exp.X_add_number;
  return 1;
}

/* Subroutine of sparc_ip to parse an expression.  */

static int
get_expression (str)
     char *str;
{
  char *save_in;
  segT seg;

  save_in = input_line_pointer;
  input_line_pointer = str;
  seg = expression (&the_insn.exp);
  if (seg != absolute_section
      && seg != text_section
      && seg != data_section
      && seg != bss_section
      && seg != undefined_section)
    {
      the_insn.error = "bad segment";
      expr_end = input_line_pointer;
      input_line_pointer = save_in;
      return 1;
    }
  expr_end = input_line_pointer;
  input_line_pointer = save_in;
  return 0;
}

/* Subroutine of md_assemble to output one insn.  */

static void
output_insn (insn, the_insn)
     const struct sparc_opcode *insn;
     struct sparc_it *the_insn;
{
  char *toP = frag_more (4);

  /* put out the opcode */
  if (INSN_BIG_ENDIAN)
    number_to_chars_bigendian (toP, (valueT) the_insn->opcode, 4);
  else
    number_to_chars_littleendian (toP, (valueT) the_insn->opcode, 4);

  /* put out the symbol-dependent stuff */
  if (the_insn->reloc != BFD_RELOC_NONE)
    {
      fixS *fixP =  fix_new_exp (frag_now,	/* which frag */
				 (toP - frag_now->fr_literal),	/* where */
				 4,		/* size */
				 &the_insn->exp,
				 the_insn->pcrel,
				 the_insn->reloc);
      /* Turn off overflow checking in fixup_segment.  We'll do our
	 own overflow checking in md_apply_fix3.  This is necessary because
	 the insn size is 4 and fixup_segment will signal an overflow for
	 large 8 byte quantities.  */
      fixP->fx_no_overflow = 1;
    }

  last_insn = insn;
  last_opcode = the_insn->opcode;
}

/*
  This is identical to the md_atof in m68k.c.  I think this is right,
  but I'm not sure.

  Turn a string in input_line_pointer into a floating point constant of type
  type, and store the appropriate bytes in *litP.  The number of LITTLENUMS
  emitted is stored in *sizeP .  An error message is returned, or NULL on OK.
  */

/* Equal to MAX_PRECISION in atof-ieee.c */
#define MAX_LITTLENUMS 6

char *
md_atof (type, litP, sizeP)
     char type;
     char *litP;
     int *sizeP;
{
  int i,prec;
  LITTLENUM_TYPE words[MAX_LITTLENUMS];
  char *t;

  switch (type)
    {
    case 'f':
    case 'F':
    case 's':
    case 'S':
      prec = 2;
      break;

    case 'd':
    case 'D':
    case 'r':
    case 'R':
      prec = 4;
      break;

    case 'x':
    case 'X':
      prec = 6;
      break;

    case 'p':
    case 'P':
      prec = 6;
      break;

    default:
      *sizeP = 0;
      return "Bad call to MD_ATOF()";
    }

  t = atof_ieee (input_line_pointer, type, words);
  if (t)
    input_line_pointer = t;
  *sizeP = prec * sizeof (LITTLENUM_TYPE);

  if (target_big_endian)
    {
      for (i = 0; i < prec; i++)
	{
	  md_number_to_chars (litP, (valueT) words[i], sizeof (LITTLENUM_TYPE));
	  litP += sizeof (LITTLENUM_TYPE);
	}
    }
  else
    {
      for (i = prec - 1; i >= 0; i--)
	{
	  md_number_to_chars (litP, (valueT) words[i], sizeof (LITTLENUM_TYPE));
	  litP += sizeof (LITTLENUM_TYPE);
	}
    }
     
  return 0;
}

/* Write a value out to the object file, using the appropriate
   endianness.  */

void
md_number_to_chars (buf, val, n)
     char *buf;
     valueT val;
     int n;
{
  if (target_big_endian)
    number_to_chars_bigendian (buf, val, n);
  else
    number_to_chars_littleendian (buf, val, n);
}

/* Apply a fixS to the frags, now that we know the value it ought to
   hold. */

int
md_apply_fix3 (fixP, value, segment)
     fixS *fixP;
     valueT *value;
     segT segment;
{
  char *buf = fixP->fx_where + fixP->fx_frag->fr_literal;
  offsetT val;
  long insn;

  val = *value;

  assert (fixP->fx_r_type < BFD_RELOC_UNUSED);

  fixP->fx_addnumber = val;	/* Remember value for emit_reloc */

#ifdef OBJ_ELF
  /* FIXME: SPARC ELF relocations don't use an addend in the data
     field itself.  This whole approach should be somehow combined
     with the calls to bfd_install_relocation.  Also, the value passed
     in by fixup_segment includes the value of a defined symbol.  We
     don't want to include the value of an externally visible symbol.  */
  if (fixP->fx_addsy != NULL)
    {
      if (fixP->fx_addsy->sy_used_in_reloc
	  && (S_IS_EXTERNAL (fixP->fx_addsy)
	      || S_IS_WEAK (fixP->fx_addsy)
	      || (sparc_pic_code && ! fixP->fx_pcrel)
	      || (S_GET_SEGMENT (fixP->fx_addsy) != segment
		  && ((bfd_get_section_flags (stdoutput,
					      S_GET_SEGMENT (fixP->fx_addsy))
		       & SEC_LINK_ONCE) != 0
		      || strncmp (segment_name (S_GET_SEGMENT (fixP->fx_addsy)),
				  ".gnu.linkonce",
				  sizeof ".gnu.linkonce" - 1) == 0)))
	  && S_GET_SEGMENT (fixP->fx_addsy) != absolute_section
	  && S_GET_SEGMENT (fixP->fx_addsy) != undefined_section
	  && ! bfd_is_com_section (S_GET_SEGMENT (fixP->fx_addsy)))
	fixP->fx_addnumber -= S_GET_VALUE (fixP->fx_addsy);
      return 1;
    }
#endif

  /* This is a hack.  There should be a better way to
     handle this.  Probably in terms of howto fields, once
     we can look at these fixups in terms of howtos.  */
  if (fixP->fx_r_type == BFD_RELOC_32_PCREL_S2 && fixP->fx_addsy)
    val += fixP->fx_where + fixP->fx_frag->fr_address;

#ifdef OBJ_AOUT
  /* FIXME: More ridiculous gas reloc hacking.  If we are going to
     generate a reloc, then we just want to let the reloc addend set
     the value.  We do not want to also stuff the addend into the
     object file.  Including the addend in the object file works when
     doing a static link, because the linker will ignore the object
     file contents.  However, the dynamic linker does not ignore the
     object file contents.  */
  if (fixP->fx_addsy != NULL
      && fixP->fx_r_type != BFD_RELOC_32_PCREL_S2)
    val = 0;

  /* When generating PIC code, we do not want an addend for a reloc
     against a local symbol.  We adjust fx_addnumber to cancel out the
     value already included in val, and to also cancel out the
     adjustment which bfd_install_relocation will create.  */
  if (sparc_pic_code
      && fixP->fx_r_type != BFD_RELOC_32_PCREL_S2
      && fixP->fx_addsy != NULL
      && ! S_IS_COMMON (fixP->fx_addsy)
      && (fixP->fx_addsy->bsym->flags & BSF_SECTION_SYM) == 0)
    fixP->fx_addnumber -= 2 * S_GET_VALUE (fixP->fx_addsy);
#endif

  /* If this is a data relocation, just output VAL.  */

  if (fixP->fx_r_type == BFD_RELOC_16)
    {
      md_number_to_chars (buf, val, 2);
    }
  else if (fixP->fx_r_type == BFD_RELOC_32)
    {
      md_number_to_chars (buf, val, 4);
    }
  else if (fixP->fx_r_type == BFD_RELOC_64)
    {
      md_number_to_chars (buf, val, 8);
    }
  else
    {
      /* It's a relocation against an instruction.  */

      if (INSN_BIG_ENDIAN)
	insn = bfd_getb32 ((unsigned char *) buf);
      else
	insn = bfd_getl32 ((unsigned char *) buf);
    
      switch (fixP->fx_r_type)
	{
	case BFD_RELOC_32_PCREL_S2:
	  val = val >> 2;
	  /* FIXME: This increment-by-one deserves a comment of why it's
	     being done!  */
	  if (! sparc_pic_code
	      || fixP->fx_addsy == NULL
	      || (fixP->fx_addsy->bsym->flags & BSF_SECTION_SYM) != 0)
	    ++val;
	  insn |= val & 0x3fffffff;
	  break;

	case BFD_RELOC_SPARC_11:
	  if (! in_signed_range (val, 0x7ff))
	    as_bad_where (fixP->fx_file, fixP->fx_line, "relocation overflow");
	  insn |= val & 0x7ff;
	  break;

	case BFD_RELOC_SPARC_10:
	  if (! in_signed_range (val, 0x3ff))
	    as_bad_where (fixP->fx_file, fixP->fx_line, "relocation overflow");
	  insn |= val & 0x3ff;
	  break;

	case BFD_RELOC_SPARC_7:
	  if (! in_bitfield_range (val, 0x7f))
	    as_bad_where (fixP->fx_file, fixP->fx_line, "relocation overflow");
	  insn |= val & 0x7f;
	  break;

	case BFD_RELOC_SPARC_6:
	  if (! in_bitfield_range (val, 0x3f))
	    as_bad_where (fixP->fx_file, fixP->fx_line, "relocation overflow");
	  insn |= val & 0x3f;
	  break;

	case BFD_RELOC_SPARC_5:
	  if (! in_bitfield_range (val, 0x1f))
	    as_bad_where (fixP->fx_file, fixP->fx_line, "relocation overflow");
	  insn |= val & 0x1f;
	  break;

	case BFD_RELOC_SPARC_WDISP16:
	  /* FIXME: simplify */
	  if (((val > 0) && (val & ~0x3fffc))
	      || ((val < 0) && (~(val - 1) & ~0x3fffc)))
	    as_bad_where (fixP->fx_file, fixP->fx_line, "relocation overflow");
	  /* FIXME: The +1 deserves a comment.  */
	  val = (val >> 2) + 1;
	  insn |= ((val & 0xc000) << 6) | (val & 0x3fff);
	  break;

	case BFD_RELOC_SPARC_WDISP19:
	  /* FIXME: simplify */
	  if (((val > 0) && (val & ~0x1ffffc))
	      || ((val < 0) && (~(val - 1) & ~0x1ffffc)))
	    as_bad_where (fixP->fx_file, fixP->fx_line, "relocation overflow");
	  /* FIXME: The +1 deserves a comment.  */
	  val = (val >> 2) + 1;
	  insn |= val & 0x7ffff;
	  break;

	case BFD_RELOC_SPARC_HH22:
	  val = BSR (val, 32);
	  /* intentional fallthrough */

	case BFD_RELOC_SPARC_LM22:
	case BFD_RELOC_HI22:
	  if (!fixP->fx_addsy)
	    {
	      insn |= (val >> 10) & 0x3fffff;
	    }
	  else
	    {
	      /* FIXME: Need comment explaining why we do this.  */
	      insn &= ~0xffff;
	    }
	  break;

	case BFD_RELOC_SPARC22:
	  if (val & ~0x003fffff)
	    as_bad_where (fixP->fx_file, fixP->fx_line, "relocation overflow");
	  insn |= (val & 0x3fffff);
	  break;

	case BFD_RELOC_SPARC_HM10:
	  val = BSR (val, 32);
	  /* intentional fallthrough */

	case BFD_RELOC_LO10:
	  if (!fixP->fx_addsy)
	    {
	      insn |= val & 0x3ff;
	    }
	  else
	    {
	      /* FIXME: Need comment explaining why we do this.  */
	      insn &= ~0xff;
	    }
	  break;

	case BFD_RELOC_SPARC13:
	  if (! in_signed_range (val, 0x1fff))
	    as_bad_where (fixP->fx_file, fixP->fx_line, "relocation overflow");
	  insn |= val & 0x1fff;
	  break;

	case BFD_RELOC_SPARC_WDISP22:
	  val = (val >> 2) + 1;
	  /* FALLTHROUGH */
	case BFD_RELOC_SPARC_BASE22:
	  insn |= val & 0x3fffff;
	  break;

	case BFD_RELOC_SPARC_H44:
	  if (!fixP->fx_addsy)
	    {
	      bfd_vma tval = val;
	      tval >>= 22;
	      insn |= tval & 0x3fffff;
	    }
	  break;

	case BFD_RELOC_SPARC_M44:
	  if (!fixP->fx_addsy)
	    insn |= (val >> 12) & 0x3ff;
	  break;

	case BFD_RELOC_SPARC_L44:
	  if (!fixP->fx_addsy)
	    insn |= val & 0xfff;
	  break;

	case BFD_RELOC_SPARC_HIX22:
	  if (!fixP->fx_addsy)
	    {
	      val ^= ~ (offsetT) 0;
	      insn |= (val >> 10) & 0x3fffff;
	    }
	  break;

	case BFD_RELOC_SPARC_LOX10:
	  if (!fixP->fx_addsy)
	    insn |= 0x1c00 | (val & 0x3ff);
	  break;

	case BFD_RELOC_NONE:
	default:
	  as_bad_where (fixP->fx_file, fixP->fx_line,
			"bad or unhandled relocation type: 0x%02x",
			fixP->fx_r_type);
	  break;
	}

      if (INSN_BIG_ENDIAN)
	bfd_putb32 (insn, (unsigned char *) buf);
      else
	bfd_putl32 (insn, (unsigned char *) buf);
    }

  /* Are we finished with this relocation now?  */
  if (fixP->fx_addsy == 0 && !fixP->fx_pcrel)
    fixP->fx_done = 1;

  return 1;
}

/* Translate internal representation of relocation info to BFD target
   format.  */
arelent *
tc_gen_reloc (section, fixp)
     asection *section;
     fixS *fixp;
{
  arelent *reloc;
  bfd_reloc_code_real_type code;

  reloc = (arelent *) xmalloc (sizeof (arelent));

  reloc->sym_ptr_ptr = &fixp->fx_addsy->bsym;
  reloc->address = fixp->fx_frag->fr_address + fixp->fx_where;

  switch (fixp->fx_r_type)
    {
    case BFD_RELOC_16:
    case BFD_RELOC_32:
    case BFD_RELOC_HI22:
    case BFD_RELOC_LO10:
    case BFD_RELOC_32_PCREL_S2:
    case BFD_RELOC_SPARC13:
    case BFD_RELOC_SPARC_BASE13:
    case BFD_RELOC_SPARC_WDISP16:
    case BFD_RELOC_SPARC_WDISP19:
    case BFD_RELOC_SPARC_WDISP22:
    case BFD_RELOC_64:
    case BFD_RELOC_SPARC_5:
    case BFD_RELOC_SPARC_6:
    case BFD_RELOC_SPARC_7:
    case BFD_RELOC_SPARC_10:
    case BFD_RELOC_SPARC_11:
    case BFD_RELOC_SPARC_HH22:
    case BFD_RELOC_SPARC_HM10:
    case BFD_RELOC_SPARC_LM22:
    case BFD_RELOC_SPARC_PC_HH22:
    case BFD_RELOC_SPARC_PC_HM10:
    case BFD_RELOC_SPARC_PC_LM22:
    case BFD_RELOC_SPARC_H44:
    case BFD_RELOC_SPARC_M44:
    case BFD_RELOC_SPARC_L44:
    case BFD_RELOC_SPARC_HIX22:
    case BFD_RELOC_SPARC_LOX10:
      code = fixp->fx_r_type;
      break;
    default:
      abort ();
      return NULL;
    }

#if defined (OBJ_ELF) || defined (OBJ_AOUT)
  /* If we are generating PIC code, we need to generate a different
     set of relocs.  */

#ifdef OBJ_ELF
#define GOT_NAME "_GLOBAL_OFFSET_TABLE_"
#else
#define GOT_NAME "__GLOBAL_OFFSET_TABLE_"
#endif

  if (sparc_pic_code)
    {
      switch (code)
	{
	case BFD_RELOC_32_PCREL_S2:
	  if (! S_IS_DEFINED (fixp->fx_addsy)
	      || S_IS_COMMON (fixp->fx_addsy)
	      || S_IS_EXTERNAL (fixp->fx_addsy)
	      || S_IS_WEAK (fixp->fx_addsy))
	    code = BFD_RELOC_SPARC_WPLT30;
	  break;
	case BFD_RELOC_HI22:
	  if (fixp->fx_addsy != NULL
	      && strcmp (S_GET_NAME (fixp->fx_addsy), GOT_NAME) == 0)
	    code = BFD_RELOC_SPARC_PC22;
	  else
	    code = BFD_RELOC_SPARC_GOT22;
	  break;
	case BFD_RELOC_LO10:
	  if (fixp->fx_addsy != NULL
	      && strcmp (S_GET_NAME (fixp->fx_addsy), GOT_NAME) == 0)
	    code = BFD_RELOC_SPARC_PC10;
	  else
	    code = BFD_RELOC_SPARC_GOT10;
	  break;
	case BFD_RELOC_SPARC13:
	  code = BFD_RELOC_SPARC_GOT13;
	  break;
	default:
	  break;
	}
    }
#endif /* defined (OBJ_ELF) || defined (OBJ_AOUT) */

  reloc->howto = bfd_reloc_type_lookup (stdoutput, code);
  if (reloc->howto == 0)
    {
      as_bad_where (fixp->fx_file, fixp->fx_line,
		    "internal error: can't export reloc type %d (`%s')",
		    fixp->fx_r_type, bfd_get_reloc_code_name (code));
      return 0;
    }

  /* @@ Why fx_addnumber sometimes and fx_offset other times?  */
#ifdef OBJ_AOUT

  if (reloc->howto->pc_relative == 0
      || code == BFD_RELOC_SPARC_PC10
      || code == BFD_RELOC_SPARC_PC22)
    reloc->addend = fixp->fx_addnumber;
  else
    reloc->addend = fixp->fx_offset - reloc->address;

#else /* elf or coff */

  if (reloc->howto->pc_relative == 0
      || code == BFD_RELOC_SPARC_PC10
      || code == BFD_RELOC_SPARC_PC22)
    reloc->addend = fixp->fx_addnumber;
  else if ((fixp->fx_addsy->bsym->flags & BSF_SECTION_SYM) != 0)
    reloc->addend = (section->vma
		     + fixp->fx_addnumber
		     + md_pcrel_from (fixp));
  else
    reloc->addend = fixp->fx_offset;
#endif

  return reloc;
}

/* We have no need to default values of symbols. */

/* ARGSUSED */
symbolS *
md_undefined_symbol (name)
     char *name;
{
  return 0;
}				/* md_undefined_symbol() */

/* Round up a section size to the appropriate boundary. */
valueT
md_section_align (segment, size)
     segT segment;
     valueT size;
{
#ifndef OBJ_ELF
  /* This is not right for ELF; a.out wants it, and COFF will force
     the alignment anyways.  */
  valueT align = ((valueT) 1
		  << (valueT) bfd_get_section_alignment (stdoutput, segment));
  valueT newsize;
  /* turn alignment value into a mask */
  align--;
  newsize = (size + align) & ~align;
  return newsize;
#else
  return size;
#endif
}

/* Exactly what point is a PC-relative offset relative TO?
   On the sparc, they're relative to the address of the offset, plus
   its size.  This gets us to the following instruction.
   (??? Is this right?  FIXME-SOON) */
long 
md_pcrel_from (fixP)
     fixS *fixP;
{
  long ret;

  ret = fixP->fx_where + fixP->fx_frag->fr_address;
  if (! sparc_pic_code
      || fixP->fx_addsy == NULL
      || (fixP->fx_addsy->bsym->flags & BSF_SECTION_SYM) != 0)
    ret += fixP->fx_size;
  return ret;
}

/*
 * sort of like s_lcomm
 */

#ifndef OBJ_ELF
static int max_alignment = 15;
#endif

static void
s_reserve (ignore)
     int ignore;
{
  char *name;
  char *p;
  char c;
  int align;
  int size;
  int temp;
  symbolS *symbolP;

  name = input_line_pointer;
  c = get_symbol_end ();
  p = input_line_pointer;
  *p = c;
  SKIP_WHITESPACE ();

  if (*input_line_pointer != ',')
    {
      as_bad ("Expected comma after name");
      ignore_rest_of_line ();
      return;
    }

  ++input_line_pointer;

  if ((size = get_absolute_expression ()) < 0)
    {
      as_bad ("BSS length (%d.) <0! Ignored.", size);
      ignore_rest_of_line ();
      return;
    }				/* bad length */

  *p = 0;
  symbolP = symbol_find_or_make (name);
  *p = c;

  if (strncmp (input_line_pointer, ",\"bss\"", 6) != 0
      && strncmp (input_line_pointer, ",\".bss\"", 7) != 0)
    {
      as_bad ("bad .reserve segment -- expected BSS segment");
      return;
    }

  if (input_line_pointer[2] == '.')
    input_line_pointer += 7;
  else
    input_line_pointer += 6;
  SKIP_WHITESPACE ();

  if (*input_line_pointer == ',')
    {
      ++input_line_pointer;

      SKIP_WHITESPACE ();
      if (*input_line_pointer == '\n')
	{
	  as_bad ("Missing alignment");
	  return;
	}

      align = get_absolute_expression ();
#ifndef OBJ_ELF
      if (align > max_alignment)
	{
	  align = max_alignment;
	  as_warn ("Alignment too large: %d. assumed.", align);
	}
#endif
      if (align < 0)
	{
	  align = 0;
	  as_warn ("Alignment negative. 0 assumed.");
	}

      record_alignment (bss_section, align);

      /* convert to a power of 2 alignment */
      for (temp = 0; (align & 1) == 0; align >>= 1, ++temp);;

      if (align != 1)
	{
	  as_bad ("Alignment not a power of 2");
	  ignore_rest_of_line ();
	  return;
	}			/* not a power of two */

      align = temp;
    }				/* if has optional alignment */
  else
    align = 0;

  if (!S_IS_DEFINED (symbolP)
#ifdef OBJ_AOUT
      && S_GET_OTHER (symbolP) == 0
      && S_GET_DESC (symbolP) == 0
#endif
      )
    {
      if (! need_pass_2)
	{
	  char *pfrag;
	  segT current_seg = now_seg;
	  subsegT current_subseg = now_subseg;

	  subseg_set (bss_section, 1); /* switch to bss */

	  if (align)
	    frag_align (align, 0, 0); /* do alignment */

	  /* detach from old frag */
	  if (S_GET_SEGMENT(symbolP) == bss_section)
	    symbolP->sy_frag->fr_symbol = NULL;

	  symbolP->sy_frag = frag_now;
	  pfrag = frag_var (rs_org, 1, 1, (relax_substateT)0, symbolP,
			    (offsetT) size, (char *)0);
	  *pfrag = 0;

	  S_SET_SEGMENT (symbolP, bss_section);

	  subseg_set (current_seg, current_subseg);
	}
    }
  else
    {
      as_warn("Ignoring attempt to re-define symbol %s",
	      S_GET_NAME (symbolP));
    }				/* if not redefining */

  demand_empty_rest_of_line ();
}

static void
s_common (ignore)
     int ignore;
{
  char *name;
  char c;
  char *p;
  int temp, size;
  symbolS *symbolP;

  name = input_line_pointer;
  c = get_symbol_end ();
  /* just after name is now '\0' */
  p = input_line_pointer;
  *p = c;
  SKIP_WHITESPACE ();
  if (*input_line_pointer != ',')
    {
      as_bad ("Expected comma after symbol-name");
      ignore_rest_of_line ();
      return;
    }
  input_line_pointer++;		/* skip ',' */
  if ((temp = get_absolute_expression ()) < 0)
    {
      as_bad (".COMMon length (%d.) <0! Ignored.", temp);
      ignore_rest_of_line ();
      return;
    }
  size = temp;
  *p = 0;
  symbolP = symbol_find_or_make (name);
  *p = c;
  if (S_IS_DEFINED (symbolP) && ! S_IS_COMMON (symbolP))
    {
      as_bad ("Ignoring attempt to re-define symbol");
      ignore_rest_of_line ();
      return;
    }
  if (S_GET_VALUE (symbolP) != 0)
    {
      if (S_GET_VALUE (symbolP) != (valueT) size)
	{
	  as_warn ("Length of .comm \"%s\" is already %ld. Not changed to %d.",
		   S_GET_NAME (symbolP), (long) S_GET_VALUE (symbolP), size);
	}
    }
  else
    {
#ifndef OBJ_ELF
      S_SET_VALUE (symbolP, (valueT) size);
      S_SET_EXTERNAL (symbolP);
#endif
    }
  know (symbolP->sy_frag == &zero_address_frag);
  if (*input_line_pointer != ',')
    {
      as_bad ("Expected comma after common length");
      ignore_rest_of_line ();
      return;
    }
  input_line_pointer++;
  SKIP_WHITESPACE ();
  if (*input_line_pointer != '"')
    {
      temp = get_absolute_expression ();
#ifndef OBJ_ELF
      if (temp > max_alignment)
	{
	  temp = max_alignment;
	  as_warn ("Common alignment too large: %d. assumed", temp);
	}
#endif
      if (temp < 0)
	{
	  temp = 0;
	  as_warn ("Common alignment negative; 0 assumed");
	}
#ifdef OBJ_ELF
      if (symbolP->local)
	{
	  segT old_sec;
	  int old_subsec;
	  char *p;
	  int align;

	  old_sec = now_seg;
	  old_subsec = now_subseg;
	  align = temp;
	  record_alignment (bss_section, align);
	  subseg_set (bss_section, 0);
	  if (align)
	    frag_align (align, 0, 0);
	  if (S_GET_SEGMENT (symbolP) == bss_section)
	    symbolP->sy_frag->fr_symbol = 0;
	  symbolP->sy_frag = frag_now;
	  p = frag_var (rs_org, 1, 1, (relax_substateT) 0, symbolP,
			(offsetT) size, (char *) 0);
	  *p = 0;
	  S_SET_SEGMENT (symbolP, bss_section);
	  S_CLEAR_EXTERNAL (symbolP);
	  subseg_set (old_sec, old_subsec);
	}
      else
#endif
	{
	allocate_common:
	  S_SET_VALUE (symbolP, (valueT) size);
#ifdef OBJ_ELF
	  S_SET_ALIGN (symbolP, temp);
#endif
	  S_SET_EXTERNAL (symbolP);
	  S_SET_SEGMENT (symbolP, bfd_com_section_ptr);
	}
    }
  else
    {
      input_line_pointer++;
      /* @@ Some use the dot, some don't.  Can we get some consistency??  */
      if (*input_line_pointer == '.')
	input_line_pointer++;
      /* @@ Some say data, some say bss.  */
      if (strncmp (input_line_pointer, "bss\"", 4)
	  && strncmp (input_line_pointer, "data\"", 5))
	{
	  while (*--input_line_pointer != '"')
	    ;
	  input_line_pointer--;
	  goto bad_common_segment;
	}
      while (*input_line_pointer++ != '"')
	;
      goto allocate_common;
    }

#ifdef BFD_ASSEMBLER
  symbolP->bsym->flags |= BSF_OBJECT;
#endif

  demand_empty_rest_of_line ();
  return;

  {
  bad_common_segment:
    p = input_line_pointer;
    while (*p && *p != '\n')
      p++;
    c = *p;
    *p = '\0';
    as_bad ("bad .common segment %s", input_line_pointer + 1);
    *p = c;
    input_line_pointer = p;
    ignore_rest_of_line ();
    return;
  }
}

/* Handle the .empty pseudo-op.  This supresses the warnings about
   invalid delay slot usage.  */

static void
s_empty (ignore)
     int ignore;
{
  /* The easy way to implement is to just forget about the last
     instruction.  */
  last_insn = NULL;
}

static void
s_seg (ignore)
     int ignore;
{

  if (strncmp (input_line_pointer, "\"text\"", 6) == 0)
    {
      input_line_pointer += 6;
      s_text (0);
      return;
    }
  if (strncmp (input_line_pointer, "\"data\"", 6) == 0)
    {
      input_line_pointer += 6;
      s_data (0);
      return;
    }
  if (strncmp (input_line_pointer, "\"data1\"", 7) == 0)
    {
      input_line_pointer += 7;
      s_data1 ();
      return;
    }
  if (strncmp (input_line_pointer, "\"bss\"", 5) == 0)
    {
      input_line_pointer += 5;
      /* We only support 2 segments -- text and data -- for now, so
	 things in the "bss segment" will have to go into data for now.
	 You can still allocate SEG_BSS stuff with .lcomm or .reserve. */
      subseg_set (data_section, 255);	/* FIXME-SOMEDAY */
      return;
    }
  as_bad ("Unknown segment type");
  demand_empty_rest_of_line ();
}

static void
s_data1 ()
{
  subseg_set (data_section, 1);
  demand_empty_rest_of_line ();
}

static void
s_proc (ignore)
     int ignore;
{
  while (!is_end_of_line[(unsigned char) *input_line_pointer])
    {
      ++input_line_pointer;
    }
  ++input_line_pointer;
}

/* This static variable is set by s_uacons to tell sparc_cons_align
   that the expession does not need to be aligned.  */

static int sparc_no_align_cons = 0;

/* This handles the unaligned space allocation pseudo-ops, such as
   .uaword.  .uaword is just like .word, but the value does not need
   to be aligned.  */

static void
s_uacons (bytes)
     int bytes;
{
  /* Tell sparc_cons_align not to align this value.  */
  sparc_no_align_cons = 1;
  cons (bytes);
}

/* If the --enforce-aligned-data option is used, we require .word,
   et. al., to be aligned correctly.  We do it by setting up an
   rs_align_code frag, and checking in HANDLE_ALIGN to make sure that
   no unexpected alignment was introduced.

   The SunOS and Solaris native assemblers enforce aligned data by
   default.  We don't want to do that, because gcc can deliberately
   generate misaligned data if the packed attribute is used.  Instead,
   we permit misaligned data by default, and permit the user to set an
   option to check for it.  */

void
sparc_cons_align (nbytes)
     int nbytes;
{
  int nalign;
  char *p;

  /* Only do this if we are enforcing aligned data.  */
  if (! enforce_aligned_data)
    return;

  if (sparc_no_align_cons)
    {
      /* This is an unaligned pseudo-op.  */
      sparc_no_align_cons = 0;
      return;
    }

  nalign = 0;
  while ((nbytes & 1) == 0)
    {
      ++nalign;
      nbytes >>= 1;
    }

  if (nalign == 0)
    return;

  if (now_seg == absolute_section)
    {
      if ((abs_section_offset & ((1 << nalign) - 1)) != 0)
	as_bad ("misaligned data");
      return;
    }

  p = frag_var (rs_align_code, 1, 1, (relax_substateT) 0,
		(symbolS *) NULL, (offsetT) nalign, (char *) NULL);

  record_alignment (now_seg, nalign);
}

/* This is where we do the unexpected alignment check.
   This is called from HANDLE_ALIGN in tc-sparc.h.  */

void
sparc_handle_align (fragp)
     fragS *fragp;
{
  if (fragp->fr_type == rs_align_code && !fragp->fr_subtype
      && fragp->fr_next->fr_address - fragp->fr_address - fragp->fr_fix != 0)
    as_bad_where (fragp->fr_file, fragp->fr_line, "misaligned data");
  if (fragp->fr_type == rs_align_code && fragp->fr_subtype == 1024)
    {
      int count = fragp->fr_next->fr_address - fragp->fr_address - fragp->fr_fix;
      
      if (count >= 4 && !(count & 3) && count <= 1024 && !((long)(fragp->fr_literal + fragp->fr_fix) & 3))
        {
          unsigned *p = (unsigned *)(fragp->fr_literal + fragp->fr_fix);
          int i;
          
          for (i = 0; i < count; i += 4)
            *p++ = 0x01000000; /* nop */
          if (SPARC_OPCODE_ARCH_V9_P (max_architecture) && count > 8)
            *(unsigned *)(fragp->fr_literal + fragp->fr_fix) =
              0x30680000 | (count >> 2); /* ba,a,pt %xcc, 1f */
          fragp->fr_var = count;
        }
    }
}

#ifdef OBJ_ELF
/* Some special processing for a Sparc ELF file.  */

void
sparc_elf_final_processing ()
{
  /* Set the Sparc ELF flag bits.  FIXME: There should probably be some
     sort of BFD interface for this.  */
  if (sparc_arch_size == 64)
    switch (sparc_memory_model)
      {
      case MM_RMO:
        elf_elfheader (stdoutput)->e_flags |= EF_SPARCV9_RMO;
        break;
      case MM_PSO:
        elf_elfheader (stdoutput)->e_flags |= EF_SPARCV9_PSO;
        break;
      }
  else if (current_architecture >= SPARC_OPCODE_ARCH_V9)
    elf_elfheader (stdoutput)->e_flags |= EF_SPARC_32PLUS;
  if (current_architecture == SPARC_OPCODE_ARCH_V9A)
    elf_elfheader (stdoutput)->e_flags |= EF_SPARC_SUN_US1;
}
#endif
