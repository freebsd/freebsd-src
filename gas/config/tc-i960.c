/* tc-i960.c - All the i80960-specific stuff
   Copyright 1989, 1990, 1991, 1992, 1993, 1994, 1995, 1996, 1997, 1998,
   1999, 2000, 2001, 2002, 2003
   Free Software Foundation, Inc.

   This file is part of GAS.

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

/* See comment on md_parse_option for 80960-specific invocation options.  */

/* There are 4 different lengths of (potentially) symbol-based displacements
   in the 80960 instruction set, each of which could require address fix-ups
   and (in the case of external symbols) emission of relocation directives:

   32-bit (MEMB)
        This is a standard length for the base assembler and requires no
        special action.

   13-bit (COBR)
        This is a non-standard length, but the base assembler has a
        hook for bit field address fixups: the fixS structure can
        point to a descriptor of the field, in which case our
        md_number_to_field() routine gets called to process it.

        I made the hook a little cleaner by having fix_new() (in the base
        assembler) return a pointer to the fixS in question.  And I made it a
        little simpler by storing the field size (in this case 13) instead of
        of a pointer to another structure:  80960 displacements are ALWAYS
        stored in the low-order bits of a 4-byte word.

        Since the target of a COBR cannot be external, no relocation
        directives for this size displacement have to be generated.
        But the base assembler had to be modified to issue error
        messages if the symbol did turn out to be external.

   24-bit (CTRL)
        Fixups are handled as for the 13-bit case (except that 24 is stored
        in the fixS).

        The relocation directive generated is the same as that for the 32-bit
        displacement, except that it's PC-relative (the 32-bit displacement
        never is).   The i80960 version of the linker needs a mod to
        distinguish and handle the 24-bit case.

   12-bit (MEMA)
        MEMA formats are always promoted to MEMB (32-bit) if the displacement
        is based on a symbol, because it could be relocated at link time.
        The only time we use the 12-bit format is if an absolute value of
        less than 4096 is specified, in which case we need neither a fixup nor
        a relocation directive.  */

#include <stdio.h>

#include "as.h"

#include "safe-ctype.h"
#include "obstack.h"

#include "opcode/i960.h"

#if defined (OBJ_AOUT) || defined (OBJ_BOUT)

#define TC_S_IS_SYSPROC(s)	((1<=S_GET_OTHER(s)) && (S_GET_OTHER(s)<=32))
#define TC_S_IS_BALNAME(s)	(S_GET_OTHER(s) == N_BALNAME)
#define TC_S_IS_CALLNAME(s)	(S_GET_OTHER(s) == N_CALLNAME)
#define TC_S_IS_BADPROC(s)	((S_GET_OTHER(s) != 0) && !TC_S_IS_CALLNAME(s) && !TC_S_IS_BALNAME(s) && !TC_S_IS_SYSPROC(s))

#define TC_S_SET_SYSPROC(s, p)	(S_SET_OTHER((s), (p)+1))
#define TC_S_GET_SYSPROC(s)	(S_GET_OTHER(s)-1)

#define TC_S_FORCE_TO_BALNAME(s)	(S_SET_OTHER((s), N_BALNAME))
#define TC_S_FORCE_TO_CALLNAME(s)	(S_SET_OTHER((s), N_CALLNAME))
#define TC_S_FORCE_TO_SYSPROC(s)	{;}

#else /* ! OBJ_A/BOUT */
#ifdef OBJ_COFF

#define TC_S_IS_SYSPROC(s)	(S_GET_STORAGE_CLASS(s) == C_SCALL)
#define TC_S_IS_BALNAME(s)	(SF_GET_BALNAME(s))
#define TC_S_IS_CALLNAME(s)	(SF_GET_CALLNAME(s))
#define TC_S_IS_BADPROC(s)	(TC_S_IS_SYSPROC(s) && TC_S_GET_SYSPROC(s) < 0 && 31 < TC_S_GET_SYSPROC(s))

#define TC_S_SET_SYSPROC(s, p)	((s)->sy_symbol.ost_auxent[1].x_sc.x_stindx = (p))
#define TC_S_GET_SYSPROC(s)	((s)->sy_symbol.ost_auxent[1].x_sc.x_stindx)

#define TC_S_FORCE_TO_BALNAME(s)	(SF_SET_BALNAME(s))
#define TC_S_FORCE_TO_CALLNAME(s)	(SF_SET_CALLNAME(s))
#define TC_S_FORCE_TO_SYSPROC(s)	(S_SET_STORAGE_CLASS((s), C_SCALL))

#else /* ! OBJ_COFF */
#ifdef OBJ_ELF
#define TC_S_IS_SYSPROC(s)	0

#define TC_S_IS_BALNAME(s)	0
#define TC_S_IS_CALLNAME(s)	0
#define TC_S_IS_BADPROC(s)	0

#define TC_S_SET_SYSPROC(s, p)
#define TC_S_GET_SYSPROC(s) 0

#define TC_S_FORCE_TO_BALNAME(s)
#define TC_S_FORCE_TO_CALLNAME(s)
#define TC_S_FORCE_TO_SYSPROC(s)
#else
 #error COFF, a.out, b.out, and ELF are the only supported formats.
#endif /* ! OBJ_ELF */
#endif /* ! OBJ_COFF */
#endif /* ! OBJ_A/BOUT */

extern char *input_line_pointer;

#if !defined (BFD_ASSEMBLER) && !defined (BFD)
#ifdef OBJ_COFF
const int md_reloc_size = sizeof (struct reloc);
#else /* OBJ_COFF */
const int md_reloc_size = sizeof (struct relocation_info);
#endif /* OBJ_COFF */
#endif

/* Local i80960 routines.  */
struct memS;
struct regop;

/* Emit branch-prediction instrumentation code */
static void brcnt_emit PARAMS ((void));
/* Return next branch local label */
static char *brlab_next PARAMS ((void));
/* Generate COBR instruction */
static void cobr_fmt PARAMS ((char *[], long, struct i960_opcode *));
/* Generate CTRL instruction */
static void ctrl_fmt PARAMS ((char *, long, int));
/* Emit (internally) binary */
static char *emit PARAMS ((long));
/* Break arguments out of comma-separated list */
static int get_args PARAMS ((char *, char *[]));
/* Handle COBR or CTRL displacement */
static void get_cdisp PARAMS ((char *, char *, long, int, int, int));
/* Find index specification string */
static char *get_ispec PARAMS ((char *));
/* Translate text to register number */
static int get_regnum PARAMS ((char *));
/* Lexical scan of instruction source */
static int i_scan PARAMS ((char *, char *[]));
/* Generate MEMA or MEMB instruction */
static void mem_fmt PARAMS ((char *[], struct i960_opcode *, int));
/* Convert MEMA instruction to MEMB format */
static void mema_to_memb PARAMS ((char *));
/* Parse an expression */
static void parse_expr PARAMS ((char *, expressionS *));
/* Parse and replace a 'ldconst' pseudo-op */
static int parse_ldconst PARAMS ((char *[]));
/* Parse a memory operand */
static void parse_memop PARAMS ((struct memS *, char *, int));
/* Parse machine-dependent pseudo-op */
static void parse_po PARAMS ((int));
/* Parse a register operand */
static void parse_regop PARAMS ((struct regop *, char *, char));
/* Generate a REG format instruction */
static void reg_fmt PARAMS ((char *[], struct i960_opcode *));
/* "De-optimize" cobr into compare/branch */
static void relax_cobr PARAMS ((fragS *));
/* Process '.leafproc' pseudo-op */
static void s_leafproc PARAMS ((int, char *[]));
/* Process '.sysproc' pseudo-op */
static void s_sysproc PARAMS ((int, char *[]));
/* Will a 'shlo' substitute for a 'ldconst'? */
static int shift_ok PARAMS ((int));
/* Give syntax error */
static void syntax PARAMS ((void));
/* Target chip supports spec-func register? */
static int targ_has_sfr PARAMS ((int));
/* Target chip supports instruction set? */
static int targ_has_iclass PARAMS ((int));

/* See md_parse_option() for meanings of these options */
static char norelax;		/* True if -norelax switch seen */
static char instrument_branches;	/* True if -b switch seen */

/* Characters that always start a comment.
   If the pre-processor is disabled, these aren't very useful.
 */
const char comment_chars[] = "#";

/* Characters that only start a comment at the beginning of
   a line.  If the line seems to have the form '# 123 filename'
   .line and .file directives will appear in the pre-processed output.

   Note that input_file.c hand checks for '#' at the beginning of the
   first line of the input file.  This is because the compiler outputs
   #NO_APP at the beginning of its output.
 */

/* Also note that comments started like this one will always work.  */

const char line_comment_chars[] = "#";

const char line_separator_chars[] = ";";

/* Chars that can be used to separate mant from exp in floating point nums */
const char EXP_CHARS[] = "eE";

/* Chars that mean this number is a floating point constant,
   as in 0f12.456 or 0d1.2345e12
 */
const char FLT_CHARS[] = "fFdDtT";

/* Table used by base assembler to relax addresses based on varying length
   instructions.  The fields are:
     1) most positive reach of this state,
     2) most negative reach of this state,
     3) how many bytes this mode will add to the size of the current frag
     4) which index into the table to try if we can't fit into this one.

   For i80960, the only application is the (de-)optimization of cobr
   instructions into separate compare and branch instructions when a 13-bit
   displacement won't hack it.
 */
const relax_typeS md_relax_table[] =
{
  {0, 0, 0, 0},			/* State 0 => no more relaxation possible */
  {4088, -4096, 0, 2},		/* State 1: conditional branch (cobr) */
  {0x800000 - 8, -0x800000, 4, 0},	/* State 2: compare (reg) & branch (ctrl) */
};

static void s_endian PARAMS ((int));

/* These are the machine dependent pseudo-ops.

   This table describes all the machine specific pseudo-ops the assembler
   has to support.  The fields are:
        pseudo-op name without dot
        function to call to execute this pseudo-op
        integer arg to pass to the function
 */
#define S_LEAFPROC	1
#define S_SYSPROC	2

const pseudo_typeS md_pseudo_table[] =
{
  {"bss", s_lcomm, 1},
  {"endian", s_endian, 0},
  {"extended", float_cons, 't'},
  {"leafproc", parse_po, S_LEAFPROC},
  {"sysproc", parse_po, S_SYSPROC},

  {"word", cons, 4},
  {"quad", cons, 16},

  {0, 0, 0}
};

/* Macros to extract info from an 'expressionS' structure 'e' */
#define adds(e)	e.X_add_symbol
#define offs(e)	e.X_add_number

/* Branch-prediction bits for CTRL/COBR format opcodes */
#define BP_MASK		0x00000002	/* Mask for branch-prediction bit */
#define BP_TAKEN	0x00000000	/* Value to OR in to predict branch */
#define BP_NOT_TAKEN	0x00000002	/* Value to OR in to predict no branch */

/* Some instruction opcodes that we need explicitly */
#define BE	0x12000000
#define BG	0x11000000
#define BGE	0x13000000
#define BL	0x14000000
#define BLE	0x16000000
#define BNE	0x15000000
#define BNO	0x10000000
#define BO	0x17000000
#define CHKBIT	0x5a002700
#define CMPI	0x5a002080
#define CMPO	0x5a002000

#define B	0x08000000
#define BAL	0x0b000000
#define CALL	0x09000000
#define CALLS	0x66003800
#define RET	0x0a000000

/* These masks are used to build up a set of MEMB mode bits.  */
#define	A_BIT		0x0400
#define	I_BIT		0x0800
#define MEMB_BIT	0x1000
#define	D_BIT		0x2000

/* Mask for the only mode bit in a MEMA instruction (if set, abase reg is
   used).  */
#define MEMA_ABASE	0x2000

/* Info from which a MEMA or MEMB format instruction can be generated */
typedef struct memS
  {
    /* (First) 32 bits of instruction */
    long opcode;
    /* 0-(none), 12- or, 32-bit displacement needed */
    int disp;
    /* The expression in the source instruction from which the
       displacement should be determined.  */
    char *e;
  }
memS;

/* The two pieces of info we need to generate a register operand */
struct regop
  {
    int mode;			/* 0 =>local/global/spec reg; 1=> literal or fp reg */
    int special;		/* 0 =>not a sfr;  1=> is a sfr (not valid w/mode=0) */
    int n;			/* Register number or literal value */
  };

/* Number and assembler mnemonic for all registers that can appear in
   operands.  */
static const struct
  {
    char *reg_name;
    int reg_num;
  }
regnames[] =
{
  { "pfp", 0 },
  { "sp", 1 },
  { "rip", 2 },
  { "r3", 3 },
  { "r4", 4 },
  { "r5", 5 },
  { "r6", 6 },
  { "r7", 7 },
  { "r8", 8 },
  { "r9", 9 },
  { "r10", 10 },
  { "r11", 11 },
  { "r12", 12 },
  { "r13", 13 },
  { "r14", 14 },
  { "r15", 15 },
  { "g0", 16 },
  { "g1", 17 },
  { "g2", 18 },
  { "g3", 19 },
  { "g4", 20 },
  { "g5", 21 },
  { "g6", 22 },
  { "g7", 23 },
  { "g8", 24 },
  { "g9", 25 },
  { "g10", 26 },
  { "g11", 27 },
  { "g12", 28 },
  { "g13", 29 },
  { "g14", 30 },
  { "fp", 31 },

  /* Numbers for special-function registers are for assembler internal
     use only: they are scaled back to range [0-31] for binary output.  */
#define SF0	32

  { "sf0", 32 },
  { "sf1", 33 },
  { "sf2", 34 },
  { "sf3", 35 },
  { "sf4", 36 },
  { "sf5", 37 },
  { "sf6", 38 },
  { "sf7", 39 },
  { "sf8", 40 },
  { "sf9", 41 },
  { "sf10", 42 },
  { "sf11", 43 },
  { "sf12", 44 },
  { "sf13", 45 },
  { "sf14", 46 },
  { "sf15", 47 },
  { "sf16", 48 },
  { "sf17", 49 },
  { "sf18", 50 },
  { "sf19", 51 },
  { "sf20", 52 },
  { "sf21", 53 },
  { "sf22", 54 },
  { "sf23", 55 },
  { "sf24", 56 },
  { "sf25", 57 },
  { "sf26", 58 },
  { "sf27", 59 },
  { "sf28", 60 },
  { "sf29", 61 },
  { "sf30", 62 },
  { "sf31", 63 },

  /* Numbers for floating point registers are for assembler internal
     use only: they are scaled back to [0-3] for binary output.  */
#define FP0	64

  { "fp0", 64 },
  { "fp1", 65 },
  { "fp2", 66 },
  { "fp3", 67 },

  { NULL, 0 },				/* END OF LIST */
};

#define	IS_RG_REG(n)	((0 <= (n)) && ((n) < SF0))
#define	IS_SF_REG(n)	((SF0 <= (n)) && ((n) < FP0))
#define	IS_FP_REG(n)	((n) >= FP0)

/* Number and assembler mnemonic for all registers that can appear as
   'abase' (indirect addressing) registers.  */
static const struct
  {
    char *areg_name;
    int areg_num;
  }
aregs[] =
{
  { "(pfp)", 0 },
  { "(sp)", 1 },
  { "(rip)", 2 },
  { "(r3)", 3 },
  { "(r4)", 4 },
  { "(r5)", 5 },
  { "(r6)", 6 },
  { "(r7)", 7 },
  { "(r8)", 8 },
  { "(r9)", 9 },
  { "(r10)", 10 },
  { "(r11)", 11 },
  { "(r12)", 12 },
  { "(r13)", 13 },
  { "(r14)", 14 },
  { "(r15)", 15 },
  { "(g0)", 16 },
  { "(g1)", 17 },
  { "(g2)", 18 },
  { "(g3)", 19 },
  { "(g4)", 20 },
  { "(g5)", 21 },
  { "(g6)", 22 },
  { "(g7)", 23 },
  { "(g8)", 24 },
  { "(g9)", 25 },
  { "(g10)", 26 },
  { "(g11)", 27 },
  { "(g12)", 28 },
  { "(g13)", 29 },
  { "(g14)", 30 },
  { "(fp)", 31 },

#define IPREL	32
  /* For assembler internal use only: this number never appears in binary
     output.  */
  { "(ip)", IPREL },

  { NULL, 0 },				/* END OF LIST */
};

/* Hash tables */
static struct hash_control *op_hash;	/* Opcode mnemonics */
static struct hash_control *reg_hash;	/* Register name hash table */
static struct hash_control *areg_hash;	/* Abase register hash table */

/* Architecture for which we are assembling */
#define ARCH_ANY	0	/* Default: no architecture checking done */
#define ARCH_KA		1
#define ARCH_KB		2
#define ARCH_MC		3
#define ARCH_CA		4
#define ARCH_JX		5
#define ARCH_HX		6
int architecture = ARCH_ANY;	/* Architecture requested on invocation line */
int iclasses_seen;		/* OR of instruction classes (I_* constants)
				 *    for which we've actually assembled
				 *      instructions.
				 */

/* BRANCH-PREDICTION INSTRUMENTATION

        The following supports generation of branch-prediction instrumentation
        (turned on by -b switch).  The instrumentation collects counts
        of branches taken/not-taken for later input to a utility that will
        set the branch prediction bits of the instructions in accordance with
        the behavior observed.  (Note that the KX series does not have
        brach-prediction.)

        The instrumentation consists of:

        (1) before and after each conditional branch, a call to an external
            routine that increments and steps over an inline counter.  The
            counter itself, initialized to 0, immediately follows the call
            instruction.  For each branch, the counter following the branch
            is the number of times the branch was not taken, and the difference
            between the counters is the number of times it was taken.  An
            example of an instrumented conditional branch:

                                call    BR_CNT_FUNC
                                .word   0
                LBRANCH23:      be      label
                                call    BR_CNT_FUNC
                                .word   0

        (2) a table of pointers to the instrumented branches, so that an
            external postprocessing routine can locate all of the counters.
            the table begins with a 2-word header: a pointer to the next in
            a linked list of such tables (initialized to 0);  and a count
            of the number of entries in the table (exclusive of the header.

            Note that input source code is expected to already contain calls
            an external routine that will link the branch local table into a
            list of such tables.
 */

/* Number of branches instrumented so far.  Also used to generate
   unique local labels for each instrumented branch.  */
static int br_cnt;

#define BR_LABEL_BASE	"LBRANCH"
/* Basename of local labels on instrumented branches, to avoid
   conflict with compiler- generated local labels.  */

#define BR_CNT_FUNC	"__inc_branch"
/* Name of the external routine that will increment (and step over) an
   inline counter.  */

#define BR_TAB_NAME	"__BRANCH_TABLE__"
/* Name of the table of pointers to branches.  A local (i.e.,
   non-external) symbol.  */

/*****************************************************************************
   md_begin:  One-time initialization.

  	Set up hash tables.

  *************************************************************************** */
void
md_begin ()
{
  int i;			/* Loop counter */
  const struct i960_opcode *oP;	/* Pointer into opcode table */
  const char *retval;		/* Value returned by hash functions */

  op_hash = hash_new ();
  reg_hash = hash_new ();
  areg_hash = hash_new ();

  /* For some reason, the base assembler uses an empty string for "no
     error message", instead of a NULL pointer.  */
  retval = 0;

  for (oP = i960_opcodes; oP->name && !retval; oP++)
    retval = hash_insert (op_hash, oP->name, (PTR) oP);

  for (i = 0; regnames[i].reg_name && !retval; i++)
    retval = hash_insert (reg_hash, regnames[i].reg_name,
			  (char *) &regnames[i].reg_num);

  for (i = 0; aregs[i].areg_name && !retval; i++)
    retval = hash_insert (areg_hash, aregs[i].areg_name,
			  (char *) &aregs[i].areg_num);

  if (retval)
    as_fatal (_("Hashing returned \"%s\"."), retval);
}

/*****************************************************************************
   md_assemble:  Assemble an instruction

   Assumptions about the passed-in text:
  	- all comments, labels removed
  	- text is an instruction
  	- all white space compressed to single blanks
  	- all character constants have been replaced with decimal

  *************************************************************************** */
void
md_assemble (textP)
     char *textP;		/* Source text of instruction */
{
  /* Parsed instruction text, containing NO whitespace: arg[0]->opcode
     mnemonic arg[1-3]->operands, with char constants replaced by
     decimal numbers.  */
  char *args[4];

  int n_ops;			/* Number of instruction operands */
  /* Pointer to instruction description */
  struct i960_opcode *oP;
  /* TRUE iff opcode mnemonic included branch-prediction suffix (".f"
     or ".t").  */
  int branch_predict;
  /* Setting of branch-prediction bit(s) to be OR'd into instruction
     opcode of CTRL/COBR format instructions.  */
  long bp_bits;

  int n;			/* Offset of last character in opcode mnemonic */

  const char *bp_error_msg = _("branch prediction invalid on this opcode");

  /* Parse instruction into opcode and operands */
  memset (args, '\0', sizeof (args));
  n_ops = i_scan (textP, args);
  if (n_ops == -1)
    {
      return;			/* Error message already issued */
    }

  /* Do "macro substitution" (sort of) on 'ldconst' pseudo-instruction */
  if (!strcmp (args[0], "ldconst"))
    {
      n_ops = parse_ldconst (args);
      if (n_ops == -1)
	{
	  return;
	}
    }

  /* Check for branch-prediction suffix on opcode mnemonic, strip it off */
  n = strlen (args[0]) - 1;
  branch_predict = 0;
  bp_bits = 0;
  if (args[0][n - 1] == '.' && (args[0][n] == 't' || args[0][n] == 'f'))
    {
      /* We could check here to see if the target architecture
	 supports branch prediction, but why bother?  The bit will
	 just be ignored by processors that don't use it.  */
      branch_predict = 1;
      bp_bits = (args[0][n] == 't') ? BP_TAKEN : BP_NOT_TAKEN;
      args[0][n - 1] = '\0';	/* Strip suffix from opcode mnemonic */
    }

  /* Look up opcode mnemonic in table and check number of operands.
     Check that opcode is legal for the target architecture.  If all
     looks good, assemble instruction.  */
  oP = (struct i960_opcode *) hash_find (op_hash, args[0]);
  if (!oP || !targ_has_iclass (oP->iclass))
    {
      as_bad (_("invalid opcode, \"%s\"."), args[0]);

    }
  else if (n_ops != oP->num_ops)
    {
      as_bad (_("improper number of operands.  expecting %d, got %d"),
	      oP->num_ops, n_ops);
    }
  else
    {
      switch (oP->format)
	{
	case FBRA:
	case CTRL:
	  ctrl_fmt (args[1], oP->opcode | bp_bits, oP->num_ops);
	  if (oP->format == FBRA)
	    {
	      /* Now generate a 'bno' to same arg */
	      ctrl_fmt (args[1], BNO | bp_bits, 1);
	    }
	  break;
	case COBR:
	case COJ:
	  cobr_fmt (args, oP->opcode | bp_bits, oP);
	  break;
	case REG:
	  if (branch_predict)
	    {
	      as_warn (bp_error_msg);
	    }
	  reg_fmt (args, oP);
	  break;
	case MEM1:
	  if (args[0][0] == 'c' && args[0][1] == 'a')
	    {
	      if (branch_predict)
		{
		  as_warn (bp_error_msg);
		}
	      mem_fmt (args, oP, 1);
	      break;
	    }
	case MEM2:
	case MEM4:
	case MEM8:
	case MEM12:
	case MEM16:
	  if (branch_predict)
	    {
	      as_warn (bp_error_msg);
	    }
	  mem_fmt (args, oP, 0);
	  break;
	case CALLJ:
	  if (branch_predict)
	    {
	      as_warn (bp_error_msg);
	    }
	  /* Output opcode & set up "fixup" (relocation); flag
	     relocation as 'callj' type.  */
	  know (oP->num_ops == 1);
	  get_cdisp (args[1], "CTRL", oP->opcode, 24, 0, 1);
	  break;
	default:
	  BAD_CASE (oP->format);
	  break;
	}
    }
}				/* md_assemble() */

/*****************************************************************************
   md_number_to_chars:  convert a number to target byte order

  *************************************************************************** */
void
md_number_to_chars (buf, value, n)
     char *buf;
     valueT value;
     int n;
{
  number_to_chars_littleendian (buf, value, n);
}

/*****************************************************************************
   md_chars_to_number:  convert from target byte order to host byte order.

  *************************************************************************** */
static int md_chars_to_number PARAMS ((unsigned char *, int));

static int
md_chars_to_number (val, n)
     unsigned char *val;	/* Value in target byte order */
     int n;			/* Number of bytes in the input */
{
  int retval;

  for (retval = 0; n--;)
    {
      retval <<= 8;
      retval |= val[n];
    }
  return retval;
}

#define MAX_LITTLENUMS	6
#define LNUM_SIZE	sizeof (LITTLENUM_TYPE)

/*****************************************************************************
   md_atof:	convert ascii to floating point

   Turn a string at input_line_pointer into a floating point constant of type
   'type', and store the appropriate bytes at *litP.  The number of LITTLENUMS
   emitted is returned at 'sizeP'.  An error message is returned, or a pointer
   to an empty message if OK.

   Note we call the i386 floating point routine, rather than complicating
   things with more files or symbolic links.

  *************************************************************************** */
char *
md_atof (type, litP, sizeP)
     int type;
     char *litP;
     int *sizeP;
{
  LITTLENUM_TYPE words[MAX_LITTLENUMS];
  LITTLENUM_TYPE *wordP;
  int prec;
  char *t;

  switch (type)
    {
    case 'f':
    case 'F':
      prec = 2;
      break;

    case 'd':
    case 'D':
      prec = 4;
      break;

    case 't':
    case 'T':
      prec = 5;
      type = 'x';		/* That's what atof_ieee() understands */
      break;

    default:
      *sizeP = 0;
      return _("Bad call to md_atof()");
    }

  t = atof_ieee (input_line_pointer, type, words);
  if (t)
    {
      input_line_pointer = t;
    }

  *sizeP = prec * LNUM_SIZE;

  /* Output the LITTLENUMs in REVERSE order in accord with i80960
     word-order.  (Dunno why atof_ieee doesn't do it in the right
     order in the first place -- probably because it's a hack of
     atof_m68k.)  */

  for (wordP = words + prec - 1; prec--;)
    {
      md_number_to_chars (litP, (long) (*wordP--), LNUM_SIZE);
      litP += sizeof (LITTLENUM_TYPE);
    }

  return 0;
}

/*****************************************************************************
   md_number_to_imm

  *************************************************************************** */
static void md_number_to_imm PARAMS ((char *, long, int));

static void
md_number_to_imm (buf, val, n)
     char *buf;
     long val;
     int n;
{
  md_number_to_chars (buf, val, n);
}

/*****************************************************************************
   md_number_to_field:

  	Stick a value (an address fixup) into a bit field of
  	previously-generated instruction.

  *************************************************************************** */
static void md_number_to_field PARAMS ((char *, long, bit_fixS *));

static void
md_number_to_field (instrP, val, bfixP)
     char *instrP;		/* Pointer to instruction to be fixed */
     long val;			/* Address fixup value */
     bit_fixS *bfixP;		/* Description of bit field to be fixed up */
{
  int numbits;			/* Length of bit field to be fixed */
  long instr;			/* 32-bit instruction to be fixed-up */
  long sign;			/* 0 or -1, according to sign bit of 'val' */

  /* Convert instruction back to host byte order.  */
  instr = md_chars_to_number (instrP, 4);

  /* Surprise! -- we stored the number of bits to be modified rather
     than a pointer to a structure.  */
  numbits = (int) bfixP;
  if (numbits == 1)
    {
      /* This is a no-op, stuck here by reloc_callj() */
      return;
    }

  know ((numbits == 13) || (numbits == 24));

  /* Propagate sign bit of 'val' for the given number of bits.  Result
     should be all 0 or all 1.  */
  sign = val >> ((int) numbits - 1);
  if (((val < 0) && (sign != -1))
      || ((val > 0) && (sign != 0)))
    {
      as_bad (_("Fixup of %ld too large for field width of %d"),
	      val, numbits);
    }
  else
    {
      /* Put bit field into instruction and write back in target
         * byte order.
       */
      val &= ~(-1 << (int) numbits);	/* Clear unused sign bits */
      instr |= val;
      md_number_to_chars (instrP, instr, 4);
    }
}				/* md_number_to_field() */


/*****************************************************************************
   md_parse_option
  	Invocation line includes a switch not recognized by the base assembler.
  	See if it's a processor-specific option.  For the 960, these are:

  	-norelax:
  		Conditional branch instructions that require displacements
  		greater than 13 bits (or that have external targets) should
  		generate errors.  The default is to replace each such
  		instruction with the corresponding compare (or chkbit) and
  		branch instructions.  Note that the Intel "j" cobr directives
  		are ALWAYS "de-optimized" in this way when necessary,
  		regardless of the setting of this option.

  	-b:
  		Add code to collect information about branches taken, for
  		later optimization of branch prediction bits by a separate
  		tool.  COBR and CNTL format instructions have branch
  		prediction bits (in the CX architecture);  if "BR" represents
  		an instruction in one of these classes, the following rep-
  		resents the code generated by the assembler:

  			call	<increment routine>
  			.word	0	# pre-counter
  		Label:  BR
  			call	<increment routine>
  			.word	0	# post-counter

  		A table of all such "Labels" is also generated.

  	-AKA, -AKB, -AKC, -ASA, -ASB, -AMC, -ACA:
  		Select the 80960 architecture.  Instructions or features not
  		supported by the selected architecture cause fatal errors.
  		The default is to generate code for any instruction or feature
  		that is supported by SOME version of the 960 (even if this
  		means mixing architectures!).

  ****************************************************************************/

const char *md_shortopts = "A:b";
struct option md_longopts[] =
{
#define OPTION_LINKRELAX (OPTION_MD_BASE)
  {"linkrelax", no_argument, NULL, OPTION_LINKRELAX},
  {"link-relax", no_argument, NULL, OPTION_LINKRELAX},
#define OPTION_NORELAX (OPTION_MD_BASE + 1)
  {"norelax", no_argument, NULL, OPTION_NORELAX},
  {"no-relax", no_argument, NULL, OPTION_NORELAX},
  {NULL, no_argument, NULL, 0}
};
size_t md_longopts_size = sizeof (md_longopts);

struct tabentry
  {
    char *flag;
    int arch;
  };
static const struct tabentry arch_tab[] =
{
  {"KA", ARCH_KA},
  {"KB", ARCH_KB},
  {"SA", ARCH_KA},		/* Synonym for KA */
  {"SB", ARCH_KB},		/* Synonym for KB */
  {"KC", ARCH_MC},		/* Synonym for MC */
  {"MC", ARCH_MC},
  {"CA", ARCH_CA},
  {"JX", ARCH_JX},
  {"HX", ARCH_HX},
  {NULL, 0}
};

int
md_parse_option (c, arg)
     int c;
     char *arg;
{
  switch (c)
    {
    case OPTION_LINKRELAX:
      linkrelax = 1;
      flag_keep_locals = 1;
      break;

    case OPTION_NORELAX:
      norelax = 1;
      break;

    case 'b':
      instrument_branches = 1;
      break;

    case 'A':
      {
	const struct tabentry *tp;
	char *p = arg;

	for (tp = arch_tab; tp->flag != NULL; tp++)
	  if (!strcmp (p, tp->flag))
	    break;

	if (tp->flag == NULL)
	  {
	    as_bad (_("invalid architecture %s"), p);
	    return 0;
	  }
	else
	  architecture = tp->arch;
      }
      break;

    default:
      return 0;
    }

  return 1;
}

void
md_show_usage (stream)
     FILE *stream;
{
  int i;
  fprintf (stream, _("I960 options:\n"));
  for (i = 0; arch_tab[i].flag; i++)
    fprintf (stream, "%s-A%s", i ? " | " : "", arch_tab[i].flag);
  fprintf (stream, _("\n\
			specify variant of 960 architecture\n\
-b			add code to collect statistics about branches taken\n\
-link-relax		preserve individual alignment directives so linker\n\
			can do relaxing (b.out format only)\n\
-no-relax		don't alter compare-and-branch instructions for\n\
			long displacements\n"));
}


/*****************************************************************************
   md_convert_frag:
  	Called by base assembler after address relaxation is finished:  modify
  	variable fragments according to how much relaxation was done.

  	If the fragment substate is still 1, a 13-bit displacement was enough
  	to reach the symbol in question.  Set up an address fixup, but otherwise
  	leave the cobr instruction alone.

  	If the fragment substate is 2, a 13-bit displacement was not enough.
  	Replace the cobr with a two instructions (a compare and a branch).

  *************************************************************************** */
#ifndef BFD_ASSEMBLER
void
md_convert_frag (headers, seg, fragP)
     object_headers *headers ATTRIBUTE_UNUSED;
     segT seg ATTRIBUTE_UNUSED;
     fragS *fragP;
#else
void
md_convert_frag (abfd, sec, fragP)
     bfd *abfd ATTRIBUTE_UNUSED;
     segT sec ATTRIBUTE_UNUSED;
     fragS *fragP;
#endif
{
  fixS *fixP;			/* Structure describing needed address fix */

  switch (fragP->fr_subtype)
    {
    case 1:
      /* LEAVE SINGLE COBR INSTRUCTION */
      fixP = fix_new (fragP,
		      fragP->fr_opcode - fragP->fr_literal,
		      4,
		      fragP->fr_symbol,
		      fragP->fr_offset,
		      1,
		      NO_RELOC);

      fixP->fx_bit_fixP = (bit_fixS *) 13;	/* size of bit field */
      break;
    case 2:
      /* REPLACE COBR WITH COMPARE/BRANCH INSTRUCTIONS */
      relax_cobr (fragP);
      break;
    default:
      BAD_CASE (fragP->fr_subtype);
      break;
    }
}

/*****************************************************************************
   md_estimate_size_before_relax:  How much does it look like *fragP will grow?

  	Called by base assembler just before address relaxation.
  	Return the amount by which the fragment will grow.

  	Any symbol that is now undefined will not become defined; cobr's
  	based on undefined symbols will have to be replaced with a compare
  	instruction and a branch instruction, and the code fragment will grow
  	by 4 bytes.

  *************************************************************************** */
int
md_estimate_size_before_relax (fragP, segment_type)
     register fragS *fragP;
     register segT segment_type;
{
  /* If symbol is undefined in this segment, go to "relaxed" state
     (compare and branch instructions instead of cobr) right now.  */
  if (S_GET_SEGMENT (fragP->fr_symbol) != segment_type)
    {
      relax_cobr (fragP);
      return 4;
    }

  return md_relax_table[fragP->fr_subtype].rlx_length;
}				/* md_estimate_size_before_relax() */

#if defined(OBJ_AOUT) | defined(OBJ_BOUT)

/*****************************************************************************
   md_ri_to_chars:
  	This routine exists in order to overcome machine byte-order problems
  	when dealing with bit-field entries in the relocation_info struct.

  	But relocation info will be used on the host machine only (only
  	executable code is actually downloaded to the i80960).  Therefore,
  	we leave it in host byte order.

  	The above comment is no longer true.  This routine now really
  	does do the reordering (Ian Taylor 28 Aug 92).

  *************************************************************************** */
static void md_ri_to_chars PARAMS ((char *, struct relocation_info *));

static void
md_ri_to_chars (where, ri)
     char *where;
     struct relocation_info *ri;
{
  md_number_to_chars (where, ri->r_address,
		      sizeof (ri->r_address));
  where[4] = ri->r_index & 0x0ff;
  where[5] = (ri->r_index >> 8) & 0x0ff;
  where[6] = (ri->r_index >> 16) & 0x0ff;
  where[7] = ((ri->r_pcrel << 0)
	      | (ri->r_length << 1)
	      | (ri->r_extern << 3)
	      | (ri->r_bsr << 4)
	      | (ri->r_disp << 5)
	      | (ri->r_callj << 6));
}

#endif /* defined(OBJ_AOUT) | defined(OBJ_BOUT) */


/* FOLLOWING ARE THE LOCAL ROUTINES, IN ALPHABETICAL ORDER  */

/*****************************************************************************
   brcnt_emit:	Emit code to increment inline branch counter.

  	See the comments above the declaration of 'br_cnt' for details on
  	branch-prediction instrumentation.
  *************************************************************************** */
static void
brcnt_emit ()
{
  ctrl_fmt (BR_CNT_FUNC, CALL, 1);	/* Emit call to "increment" routine */
  emit (0);			/* Emit inline counter to be incremented */
}

/*****************************************************************************
   brlab_next:	generate the next branch local label

  	See the comments above the declaration of 'br_cnt' for details on
  	branch-prediction instrumentation.
  *************************************************************************** */
static char *
brlab_next ()
{
  static char buf[20];

  sprintf (buf, "%s%d", BR_LABEL_BASE, br_cnt++);
  return buf;
}

/*****************************************************************************
   brtab_emit:	generate the fetch-prediction branch table.

  	See the comments above the declaration of 'br_cnt' for details on
  	branch-prediction instrumentation.

  	The code emitted here would be functionally equivalent to the following
  	example assembler source.

  			.data
  			.align	2
  	   BR_TAB_NAME:
  			.word	0		# link to next table
  			.word	3		# length of table
  			.word	LBRANCH0	# 1st entry in table proper
  			.word	LBRANCH1
  			.word	LBRANCH2
  **************************************************************************** */
void
brtab_emit ()
{
  int i;
  char buf[20];
  char *p;			/* Where the binary was output to */
  /* Pointer to description of deferred address fixup.  */
  fixS *fixP;

  if (!instrument_branches)
    {
      return;
    }

  subseg_set (data_section, 0);	/*      .data */
  frag_align (2, 0, 0);		/*      .align 2 */
  record_alignment (now_seg, 2);
  colon (BR_TAB_NAME);		/* BR_TAB_NAME: */
  emit (0);			/*      .word 0 #link to next table */
  emit (br_cnt);		/*      .word n #length of table */

  for (i = 0; i < br_cnt; i++)
    {
      sprintf (buf, "%s%d", BR_LABEL_BASE, i);
      p = emit (0);
      fixP = fix_new (frag_now,
		      p - frag_now->fr_literal,
		      4,
		      symbol_find (buf),
		      0,
		      0,
		      NO_RELOC);
    }
}

/*****************************************************************************
   cobr_fmt:	generate a COBR-format instruction

  *************************************************************************** */
static void
cobr_fmt (arg, opcode, oP)
     /* arg[0]->opcode mnemonic, arg[1-3]->operands (ascii) */
     char *arg[];
     /* Opcode, with branch-prediction bits already set if necessary.  */
     long opcode;
     /* Pointer to description of instruction.  */
     struct i960_opcode *oP;
{
  long instr;			/* 32-bit instruction */
  struct regop regop;		/* Description of register operand */
  int n;			/* Number of operands */
  int var_frag;			/* 1 if varying length code fragment should
				 *    be emitted;  0 if an address fix
				 *      should be emitted.
				 */

  instr = opcode;
  n = oP->num_ops;

  if (n >= 1)
    {
      /* First operand (if any) of a COBR is always a register
	 operand.  Parse it.  */
      parse_regop (&regop, arg[1], oP->operand[0]);
      instr |= (regop.n << 19) | (regop.mode << 13);
    }
  if (n >= 2)
    {
      /* Second operand (if any) of a COBR is always a register
	 operand.  Parse it.  */
    parse_regop (&regop, arg[2], oP->operand[1]);
      instr |= (regop.n << 14) | regop.special;
    }

  if (n < 3)
    {
      emit (instr);

    }
  else
    {
      if (instrument_branches)
	{
	  brcnt_emit ();
	  colon (brlab_next ());
	}

      /* A third operand to a COBR is always a displacement.  Parse
         it; if it's relaxable (a cobr "j" directive, or any cobr
         other than bbs/bbc when the "-norelax" option is not in use)
         set up a variable code fragment; otherwise set up an address
         fix.  */
      var_frag = !norelax || (oP->format == COJ);	/* TRUE or FALSE */
      get_cdisp (arg[3], "COBR", instr, 13, var_frag, 0);

      if (instrument_branches)
	{
	  brcnt_emit ();
	}
    }
}				/* cobr_fmt() */

/*****************************************************************************
   ctrl_fmt:	generate a CTRL-format instruction

  *************************************************************************** */
static void
ctrl_fmt (targP, opcode, num_ops)
     char *targP;		/* Pointer to text of lone operand (if any) */
     long opcode;		/* Template of instruction */
     int num_ops;		/* Number of operands */
{
  int instrument;		/* TRUE iff we should add instrumentation to track
				   * how often the branch is taken
				 */

  if (num_ops == 0)
    {
      emit (opcode);		/* Output opcode */
    }
  else
    {

      instrument = instrument_branches && (opcode != CALL)
	&& (opcode != B) && (opcode != RET) && (opcode != BAL);

      if (instrument)
	{
	  brcnt_emit ();
	  colon (brlab_next ());
	}

      /* The operand MUST be an ip-relative displacement. Parse it
         * and set up address fix for the instruction we just output.
       */
      get_cdisp (targP, "CTRL", opcode, 24, 0, 0);

      if (instrument)
	{
	  brcnt_emit ();
	}
    }

}

/*****************************************************************************
   emit:	output instruction binary

  	Output instruction binary, in target byte order, 4 bytes at a time.
  	Return pointer to where it was placed.

  *************************************************************************** */
static char *
emit (instr)
     long instr;		/* Word to be output, host byte order */
{
  char *toP;			/* Where to output it */

  toP = frag_more (4);		/* Allocate storage */
  md_number_to_chars (toP, instr, 4);	/* Convert to target byte order */
  return toP;
}

/*****************************************************************************
   get_args:	break individual arguments out of comma-separated list

   Input assumptions:
  	- all comments and labels have been removed
  	- all strings of whitespace have been collapsed to a single blank.
  	- all character constants ('x') have been replaced with decimal

   Output:
  	args[0] is untouched. args[1] points to first operand, etc. All args:
  	- are NULL-terminated
  	- contain no whitespace

   Return value:
  	Number of operands (0,1,2, or 3) or -1 on error.

  *************************************************************************** */
static int
get_args (p, args)
     /* Pointer to comma-separated operands; MUCKED BY US */
     register char *p;
     /* Output arg: pointers to operands placed in args[1-3].  MUST
        ACCOMMODATE 4 ENTRIES (args[0-3]).  */
     char *args[];
{
  register int n;		/* Number of operands */
  register char *to;

  /* Skip lead white space */
  while (*p == ' ')
    {
      p++;
    }

  if (*p == '\0')
    {
      return 0;
    }

  n = 1;
  args[1] = p;

  /* Squeze blanks out by moving non-blanks toward start of string.
     * Isolate operands, whenever comma is found.
   */
  to = p;
  while (*p != '\0')
    {

      if (*p == ' '
	  && (! ISALNUM (p[1])
	      || ! ISALNUM (p[-1])))
	{
	  p++;

	}
      else if (*p == ',')
	{

	  /* Start of operand */
	  if (n == 3)
	    {
	      as_bad (_("too many operands"));
	      return -1;
	    }
	  *to++ = '\0';		/* Terminate argument */
	  args[++n] = to;	/* Start next argument */
	  p++;

	}
      else
	{
	  *to++ = *p++;
	}
    }
  *to = '\0';
  return n;
}

/*****************************************************************************
   get_cdisp:	handle displacement for a COBR or CTRL instruction.

  	Parse displacement for a COBR or CTRL instruction.

  	If successful, output the instruction opcode and set up for it,
  	depending on the arg 'var_frag', either:
  	    o an address fixup to be done when all symbol values are known, or
  	    o a varying length code fragment, with address fixup info.  This
  		will be done for cobr instructions that may have to be relaxed
  		in to compare/branch instructions (8 bytes) if the final
  		address displacement is greater than 13 bits.

  ****************************************************************************/
static void
get_cdisp (dispP, ifmtP, instr, numbits, var_frag, callj)
     /* displacement as specified in source instruction */
     char *dispP;
     /* "COBR" or "CTRL" (for use in error message) */
     char *ifmtP;
     /* Instruction needing the displacement */
     long instr;
     /* # bits of displacement (13 for COBR, 24 for CTRL) */
     int numbits;
     /* 1 if varying length code fragment should be emitted;
      *       0 if an address fix should be emitted.
      */
     int var_frag;
     /* 1 if callj relocation should be done; else 0 */
     int callj;
{
  expressionS e;		/* Parsed expression */
  fixS *fixP;			/* Structure describing needed address fix */
  char *outP;			/* Where instruction binary is output to */

  fixP = NULL;

  parse_expr (dispP, &e);
  switch (e.X_op)
    {
    case O_illegal:
      as_bad (_("expression syntax error"));

    case O_symbol:
      if (S_GET_SEGMENT (e.X_add_symbol) == now_seg
	  || S_GET_SEGMENT (e.X_add_symbol) == undefined_section)
	{
	  if (var_frag)
	    {
	      outP = frag_more (8);	/* Allocate worst-case storage */
	      md_number_to_chars (outP, instr, 4);
	      frag_variant (rs_machine_dependent, 4, 4, 1,
			    adds (e), offs (e), outP);
	    }
	  else
	    {
	      /* Set up a new fix structure, so address can be updated
	       * when all symbol values are known.
	       */
	      outP = emit (instr);
	      fixP = fix_new (frag_now,
			      outP - frag_now->fr_literal,
			      4,
			      adds (e),
			      offs (e),
			      1,
			      NO_RELOC);

	      fixP->fx_tcbit = callj;

	      /* We want to modify a bit field when the address is
	       * known.  But we don't need all the garbage in the
	       * bit_fix structure.  So we're going to lie and store
	       * the number of bits affected instead of a pointer.
	       */
	      fixP->fx_bit_fixP = (bit_fixS *) numbits;
	    }
	}
      else
	as_bad (_("attempt to branch into different segment"));
      break;

    default:
      as_bad (_("target of %s instruction must be a label"), ifmtP);
      break;
    }
}

/*****************************************************************************
   get_ispec:	parse a memory operand for an index specification

  	Here, an "index specification" is taken to be anything surrounded
  	by square brackets and NOT followed by anything else.

  	If it's found, detach it from the input string, remove the surrounding
  	square brackets, and return a pointer to it.  Otherwise, return NULL.

  *************************************************************************** */
static char *
get_ispec (textP)
     /* Pointer to memory operand from source instruction, no white space.  */
     char *textP;
{
  /* Points to start of index specification.  */
  char *start;
  /* Points to end of index specification.  */
  char *end;

  /* Find opening square bracket, if any.  */
  start = strchr (textP, '[');

  if (start != NULL)
    {

      /* Eliminate '[', detach from rest of operand */
      *start++ = '\0';

      end = strchr (start, ']');

      if (end == NULL)
	{
	  as_bad (_("unmatched '['"));

	}
      else
	{
	  /* Eliminate ']' and make sure it was the last thing
	     * in the string.
	   */
	  *end = '\0';
	  if (*(end + 1) != '\0')
	    {
	      as_bad (_("garbage after index spec ignored"));
	    }
	}
    }
  return start;
}

/*****************************************************************************
   get_regnum:

  	Look up a (suspected) register name in the register table and return the
  	associated register number (or -1 if not found).

  *************************************************************************** */
static int
get_regnum (regname)
     char *regname;		/* Suspected register name */
{
  int *rP;

  rP = (int *) hash_find (reg_hash, regname);
  return (rP == NULL) ? -1 : *rP;
}

/*****************************************************************************
   i_scan:	perform lexical scan of ascii assembler instruction.

   Input assumptions:
  	- input string is an i80960 instruction (not a pseudo-op)
  	- all comments and labels have been removed
  	- all strings of whitespace have been collapsed to a single blank.

   Output:
  	args[0] points to opcode, other entries point to operands. All strings:
  	- are NULL-terminated
  	- contain no whitespace
  	- have character constants ('x') replaced with a decimal number

   Return value:
  	Number of operands (0,1,2, or 3) or -1 on error.

  *************************************************************************** */
static int
i_scan (iP, args)
     /* Pointer to ascii instruction;  MUCKED BY US.  */
     register char *iP;
     /* Output arg: pointers to opcode and operands placed here.  MUST
	ACCOMMODATE 4 ENTRIES.  */
     char *args[];
{

  /* Isolate opcode */
  if (*(iP) == ' ')
    {
      iP++;
    }				/* Skip lead space, if any */
  args[0] = iP;
  for (; *iP != ' '; iP++)
    {
      if (*iP == '\0')
	{
	  /* There are no operands */
	  if (args[0] == iP)
	    {
	      /* We never moved: there was no opcode either! */
	      as_bad (_("missing opcode"));
	      return -1;
	    }
	  return 0;
	}
    }
  *iP++ = '\0';			/* Terminate opcode */
  return (get_args (iP, args));
}				/* i_scan() */

/*****************************************************************************
   mem_fmt:	generate a MEMA- or MEMB-format instruction

  *************************************************************************** */
static void
mem_fmt (args, oP, callx)
     char *args[];		/* args[0]->opcode mnemonic, args[1-3]->operands */
     struct i960_opcode *oP;	/* Pointer to description of instruction */
     int callx;			/* Is this a callx opcode */
{
  int i;			/* Loop counter */
  struct regop regop;		/* Description of register operand */
  char opdesc;			/* Operand descriptor byte */
  memS instr;			/* Description of binary to be output */
  char *outP;			/* Where the binary was output to */
  expressionS expr;		/* Parsed expression */
  /* ->description of deferred address fixup */
  fixS *fixP;

#ifdef OBJ_COFF
  /* COFF support isn't in place yet for callx relaxing.  */
  callx = 0;
#endif

  memset (&instr, '\0', sizeof (memS));
  instr.opcode = oP->opcode;

  /* Process operands.  */
  for (i = 1; i <= oP->num_ops; i++)
    {
      opdesc = oP->operand[i - 1];

      if (MEMOP (opdesc))
	{
	  parse_memop (&instr, args[i], oP->format);
	}
      else
	{
	  parse_regop (&regop, args[i], opdesc);
	  instr.opcode |= regop.n << 19;
	}
    }

  /* Parse the displacement; this must be done before emitting the
     opcode, in case it is an expression using `.'.  */
  parse_expr (instr.e, &expr);

  /* Output opcode */
  outP = emit (instr.opcode);

  if (instr.disp == 0)
    {
      return;
    }

  /* Process the displacement */
  switch (expr.X_op)
    {
    case O_illegal:
      as_bad (_("expression syntax error"));
      break;

    case O_constant:
      if (instr.disp == 32)
	{
	  (void) emit (offs (expr));	/* Output displacement */
	}
      else
	{
	  /* 12-bit displacement */
	  if (offs (expr) & ~0xfff)
	    {
	      /* Won't fit in 12 bits: convert already-output
	       * instruction to MEMB format, output
	       * displacement.
	       */
	      mema_to_memb (outP);
	      (void) emit (offs (expr));
	    }
	  else
	    {
	      /* WILL fit in 12 bits:  OR into opcode and
	       * overwrite the binary we already put out
	       */
	      instr.opcode |= offs (expr);
	      md_number_to_chars (outP, instr.opcode, 4);
	    }
	}
      break;

    default:
      if (instr.disp == 12)
	{
	  /* Displacement is dependent on a symbol, whose value
	   * may change at link time.  We HAVE to reserve 32 bits.
	   * Convert already-output opcode to MEMB format.
	   */
	  mema_to_memb (outP);
	}

      /* Output 0 displacement and set up address fixup for when
       * this symbol's value becomes known.
       */
      outP = emit ((long) 0);
      fixP = fix_new_exp (frag_now,
			  outP - frag_now->fr_literal,
			  4,
			  &expr,
			  0,
			  NO_RELOC);
      /* Steve's linker relaxing hack.  Mark this 32-bit relocation as
         being in the instruction stream, specifically as part of a callx
         instruction.  */
      fixP->fx_bsr = callx;
      break;
    }
}				/* memfmt() */

/*****************************************************************************
   mema_to_memb:	convert a MEMA-format opcode to a MEMB-format opcode.

   There are 2 possible MEMA formats:
  	- displacement only
  	- displacement + abase

   They are distinguished by the setting of the MEMA_ABASE bit.

  *************************************************************************** */
static void
mema_to_memb (opcodeP)
     char *opcodeP;		/* Where to find the opcode, in target byte order */
{
  long opcode;			/* Opcode in host byte order */
  long mode;			/* Mode bits for MEMB instruction */

  opcode = md_chars_to_number (opcodeP, 4);
  know (!(opcode & MEMB_BIT));

  mode = MEMB_BIT | D_BIT;
  if (opcode & MEMA_ABASE)
    {
      mode |= A_BIT;
    }

  opcode &= 0xffffc000;		/* Clear MEMA offset and mode bits */
  opcode |= mode;		/* Set MEMB mode bits */

  md_number_to_chars (opcodeP, opcode, 4);
}				/* mema_to_memb() */

/*****************************************************************************
   parse_expr:		parse an expression

  	Use base assembler's expression parser to parse an expression.
  	It, unfortunately, runs off a global which we have to save/restore
  	in order to make it work for us.

  	An empty expression string is treated as an absolute 0.

  	Sets O_illegal regardless of expression evaluation if entire input
  	string is not consumed in the evaluation -- tolerate no dangling junk!

  *************************************************************************** */
static void
parse_expr (textP, expP)
     char *textP;		/* Text of expression to be parsed */
     expressionS *expP;		/* Where to put the results of parsing */
{
  char *save_in;		/* Save global here */
  symbolS *symP;

  know (textP);

  if (*textP == '\0')
    {
      /* Treat empty string as absolute 0 */
      expP->X_add_symbol = expP->X_op_symbol = NULL;
      expP->X_add_number = 0;
      expP->X_op = O_constant;
    }
  else
    {
      save_in = input_line_pointer;	/* Save global */
      input_line_pointer = textP;	/* Make parser work for us */

      (void) expression (expP);
      if ((size_t) (input_line_pointer - textP) != strlen (textP))
	{
	  /* Did not consume all of the input */
	  expP->X_op = O_illegal;
	}
      symP = expP->X_add_symbol;
      if (symP && (hash_find (reg_hash, S_GET_NAME (symP))))
	{
	  /* Register name in an expression */
	  /* FIXME: this isn't much of a check any more.  */
	  expP->X_op = O_illegal;
	}

      input_line_pointer = save_in;	/* Restore global */
    }
}

/*****************************************************************************
   parse_ldcont:
  	Parse and replace a 'ldconst' pseudo-instruction with an appropriate
  	i80960 instruction.

  	Assumes the input consists of:
  		arg[0]	opcode mnemonic ('ldconst')
  		arg[1]  first operand (constant)
  		arg[2]	name of register to be loaded

  	Replaces opcode and/or operands as appropriate.

  	Returns the new number of arguments, or -1 on failure.

  *************************************************************************** */
static int
parse_ldconst (arg)
     char *arg[];		/* See above */
{
  int n;			/* Constant to be loaded */
  int shift;			/* Shift count for "shlo" instruction */
  static char buf[5];		/* Literal for first operand */
  static char buf2[5];		/* Literal for second operand */
  expressionS e;		/* Parsed expression */

  arg[3] = NULL;		/* So we can tell at the end if it got used or not */

  parse_expr (arg[1], &e);
  switch (e.X_op)
    {
    default:
      /* We're dependent on one or more symbols -- use "lda" */
      arg[0] = "lda";
      break;

    case O_constant:
      /* Try the following mappings:
       *      ldconst 0,<reg>  ->mov  0,<reg>
       *        ldconst 31,<reg> ->mov  31,<reg>
       *        ldconst 32,<reg> ->addo 1,31,<reg>
       *        ldconst 62,<reg> ->addo 31,31,<reg>
       *        ldconst 64,<reg> ->shlo 8,3,<reg>
       *        ldconst -1,<reg> ->subo 1,0,<reg>
       *        ldconst -31,<reg>->subo 31,0,<reg>
       *
       * anything else becomes:
       *        lda xxx,<reg>
       */
      n = offs (e);
      if ((0 <= n) && (n <= 31))
	{
	  arg[0] = "mov";

	}
      else if ((-31 <= n) && (n <= -1))
	{
	  arg[0] = "subo";
	  arg[3] = arg[2];
	  sprintf (buf, "%d", -n);
	  arg[1] = buf;
	  arg[2] = "0";

	}
      else if ((32 <= n) && (n <= 62))
	{
	  arg[0] = "addo";
	  arg[3] = arg[2];
	  arg[1] = "31";
	  sprintf (buf, "%d", n - 31);
	  arg[2] = buf;

	}
      else if ((shift = shift_ok (n)) != 0)
	{
	  arg[0] = "shlo";
	  arg[3] = arg[2];
	  sprintf (buf, "%d", shift);
	  arg[1] = buf;
	  sprintf (buf2, "%d", n >> shift);
	  arg[2] = buf2;

	}
      else
	{
	  arg[0] = "lda";
	}
      break;

    case O_illegal:
      as_bad (_("invalid constant"));
      return -1;
      break;
    }
  return (arg[3] == 0) ? 2 : 3;
}

/*****************************************************************************
   parse_memop:	parse a memory operand

  	This routine is based on the observation that the 4 mode bits of the
  	MEMB format, taken individually, have fairly consistent meaning:

  		 M3 (bit 13): 1 if displacement is present (D_BIT)
  		 M2 (bit 12): 1 for MEMB instructions (MEMB_BIT)
  		 M1 (bit 11): 1 if index is present (I_BIT)
  		 M0 (bit 10): 1 if abase is present (A_BIT)

  	So we parse the memory operand and set bits in the mode as we find
  	things.  Then at the end, if we go to MEMB format, we need only set
  	the MEMB bit (M2) and our mode is built for us.

  	Unfortunately, I said "fairly consistent".  The exceptions:

  		 DBIA
  		 0100	Would seem illegal, but means "abase-only".

  		 0101	Would seem to mean "abase-only" -- it means IP-relative.
  			Must be converted to 0100.

  		 0110	Would seem to mean "index-only", but is reserved.
  			We turn on the D bit and provide a 0 displacement.

  	The other thing to observe is that we parse from the right, peeling
  	things * off as we go:  first any index spec, then any abase, then
  	the displacement.

  *************************************************************************** */
static void
parse_memop (memP, argP, optype)
     memS *memP;		/* Where to put the results */
     char *argP;		/* Text of the operand to be parsed */
     int optype;		/* MEM1, MEM2, MEM4, MEM8, MEM12, or MEM16 */
{
  char *indexP;			/* Pointer to index specification with "[]" removed */
  char *p;			/* Temp char pointer */
  char iprel_flag;		/* True if this is an IP-relative operand */
  int regnum;			/* Register number */
  /* Scale factor: 1,2,4,8, or 16.  Later converted to internal format
     (0,1,2,3,4 respectively).  */
  int scale;
  int mode;			/* MEMB mode bits */
  int *intP;			/* Pointer to register number */

  /* The following table contains the default scale factors for each
     type of memory instruction.  It is accessed using (optype-MEM1)
     as an index -- thus it assumes the 'optype' constants are
     assigned consecutive values, in the order they appear in this
     table.  */
  static const int def_scale[] =
  {
    1,				/* MEM1 */
    2,				/* MEM2 */
    4,				/* MEM4 */
    8,				/* MEM8 */
    -1,				/* MEM12 -- no valid default */
    16				/* MEM16 */
  };

  iprel_flag = mode = 0;

  /* Any index present? */
  indexP = get_ispec (argP);
  if (indexP)
    {
      p = strchr (indexP, '*');
      if (p == NULL)
	{
	  /* No explicit scale -- use default for this instruction
	     type and assembler mode.  */
	  if (flag_mri)
	    scale = 1;
	  else
	    /* GNU960 compatibility */
	    scale = def_scale[optype - MEM1];
	}
      else
	{
	  *p++ = '\0';		/* Eliminate '*' */

	  /* Now indexP->a '\0'-terminated register name,
	     * and p->a scale factor.
	   */

	  if (!strcmp (p, "16"))
	    {
	      scale = 16;
	    }
	  else if (strchr ("1248", *p) && (p[1] == '\0'))
	    {
	      scale = *p - '0';
	    }
	  else
	    {
	      scale = -1;
	    }
	}

      regnum = get_regnum (indexP);	/* Get index reg. # */
      if (!IS_RG_REG (regnum))
	{
	  as_bad (_("invalid index register"));
	  return;
	}

      /* Convert scale to its binary encoding */
      switch (scale)
	{
	case 1:
	  scale = 0 << 7;
	  break;
	case 2:
	  scale = 1 << 7;
	  break;
	case 4:
	  scale = 2 << 7;
	  break;
	case 8:
	  scale = 3 << 7;
	  break;
	case 16:
	  scale = 4 << 7;
	  break;
	default:
	  as_bad (_("invalid scale factor"));
	  return;
	};

      memP->opcode |= scale | regnum;	/* Set index bits in opcode */
      mode |= I_BIT;		/* Found a valid index spec */
    }

  /* Any abase (Register Indirect) specification present? */
  if ((p = strrchr (argP, '(')) != NULL)
    {
      /* "(" is there -- does it start a legal abase spec?  If not, it
         could be part of a displacement expression.  */
      intP = (int *) hash_find (areg_hash, p);
      if (intP != NULL)
	{
	  /* Got an abase here */
	  regnum = *intP;
	  *p = '\0';		/* discard register spec */
	  if (regnum == IPREL)
	    {
	      /* We have to specialcase ip-rel mode */
	      iprel_flag = 1;
	    }
	  else
	    {
	      memP->opcode |= regnum << 14;
	      mode |= A_BIT;
	    }
	}
    }

  /* Any expression present? */
  memP->e = argP;
  if (*argP != '\0')
    {
      mode |= D_BIT;
    }

  /* Special-case ip-relative addressing */
  if (iprel_flag)
    {
      if (mode & I_BIT)
	{
	  syntax ();
	}
      else
	{
	  memP->opcode |= 5 << 10;	/* IP-relative mode */
	  memP->disp = 32;
	}
      return;
    }

  /* Handle all other modes */
  switch (mode)
    {
    case D_BIT | A_BIT:
      /* Go with MEMA instruction format for now (grow to MEMB later
         if 12 bits is not enough for the displacement).  MEMA format
         has a single mode bit: set it to indicate that abase is
         present.  */
      memP->opcode |= MEMA_ABASE;
      memP->disp = 12;
      break;

    case D_BIT:
      /* Go with MEMA instruction format for now (grow to MEMB later
         if 12 bits is not enough for the displacement).  */
      memP->disp = 12;
      break;

    case A_BIT:
      /* For some reason, the bit string for this mode is not
         consistent: it should be 0 (exclusive of the MEMB bit), so we
         set it "by hand" here.  */
      memP->opcode |= MEMB_BIT;
      break;

    case A_BIT | I_BIT:
      /* set MEMB bit in mode, and OR in mode bits */
      memP->opcode |= mode | MEMB_BIT;
      break;

    case I_BIT:
      /* Treat missing displacement as displacement of 0.  */
      mode |= D_BIT;
      /* Fall into next case.  */
    case D_BIT | A_BIT | I_BIT:
    case D_BIT | I_BIT:
      /* set MEMB bit in mode, and OR in mode bits */
      memP->opcode |= mode | MEMB_BIT;
      memP->disp = 32;
      break;

    default:
      syntax ();
      break;
    }
}

/*****************************************************************************
   parse_po:	parse machine-dependent pseudo-op

  	This is a top-level routine for machine-dependent pseudo-ops.  It slurps
  	up the rest of the input line, breaks out the individual arguments,
  	and dispatches them to the correct handler.
  *************************************************************************** */
static void
parse_po (po_num)
     int po_num;		/* Pseudo-op number:  currently S_LEAFPROC or S_SYSPROC */
{
  /* Pointers operands, with no embedded whitespace.
     arg[0] unused, arg[1-3]->operands */
  char *args[4];
  int n_ops;			/* Number of operands */
  char *p;			/* Pointer to beginning of unparsed argument string */
  char eol;			/* Character that indicated end of line */

  extern char is_end_of_line[];

  /* Advance input pointer to end of line.  */
  p = input_line_pointer;
  while (!is_end_of_line[(unsigned char) *input_line_pointer])
    {
      input_line_pointer++;
    }
  eol = *input_line_pointer;	/* Save end-of-line char */
  *input_line_pointer = '\0';	/* Terminate argument list */

  /* Parse out operands */
  n_ops = get_args (p, args);
  if (n_ops == -1)
    {
      return;
    }

  /* Dispatch to correct handler */
  switch (po_num)
    {
    case S_SYSPROC:
      s_sysproc (n_ops, args);
      break;
    case S_LEAFPROC:
      s_leafproc (n_ops, args);
      break;
    default:
      BAD_CASE (po_num);
      break;
    }

  /* Restore eol, so line numbers get updated correctly.  Base
     assembler assumes we leave input pointer pointing at char
     following the eol.  */
  *input_line_pointer++ = eol;
}

/*****************************************************************************
   parse_regop: parse a register operand.

  	In case of illegal operand, issue a message and return some valid
  	information so instruction processing can continue.
  *************************************************************************** */
static void
parse_regop (regopP, optext, opdesc)
     struct regop *regopP;	/* Where to put description of register operand */
     char *optext;		/* Text of operand */
     char opdesc;		/* Descriptor byte:  what's legal for this operand */
{
  int n;			/* Register number */
  expressionS e;		/* Parsed expression */

  /* See if operand is a register */
  n = get_regnum (optext);
  if (n >= 0)
    {
      if (IS_RG_REG (n))
	{
	  /* global or local register */
	  if (!REG_ALIGN (opdesc, n))
	    {
	      as_bad (_("unaligned register"));
	    }
	  regopP->n = n;
	  regopP->mode = 0;
	  regopP->special = 0;
	  return;
	}
      else if (IS_FP_REG (n) && FP_OK (opdesc))
	{
	  /* Floating point register, and it's allowed */
	  regopP->n = n - FP0;
	  regopP->mode = 1;
	  regopP->special = 0;
	  return;
	}
      else if (IS_SF_REG (n) && SFR_OK (opdesc))
	{
	  /* Special-function register, and it's allowed */
	  regopP->n = n - SF0;
	  regopP->mode = 0;
	  regopP->special = 1;
	  if (!targ_has_sfr (regopP->n))
	    {
	      as_bad (_("no such sfr in this architecture"));
	    }
	  return;
	}
    }
  else if (LIT_OK (opdesc))
    {
      /* How about a literal?  */
      regopP->mode = 1;
      regopP->special = 0;
      if (FP_OK (opdesc))
	{			/* floating point literal acceptable */
	  /* Skip over 0f, 0d, or 0e prefix */
	  if ((optext[0] == '0')
	      && (optext[1] >= 'd')
	      && (optext[1] <= 'f'))
	    {
	      optext += 2;
	    }

	  if (!strcmp (optext, "0.0") || !strcmp (optext, "0"))
	    {
	      regopP->n = 0x10;
	      return;
	    }
	  if (!strcmp (optext, "1.0") || !strcmp (optext, "1"))
	    {
	      regopP->n = 0x16;
	      return;
	    }

	}
      else
	{			/* fixed point literal acceptable */
	  parse_expr (optext, &e);
	  if (e.X_op != O_constant
	      || (offs (e) < 0) || (offs (e) > 31))
	    {
	      as_bad (_("illegal literal"));
	      offs (e) = 0;
	    }
	  regopP->n = offs (e);
	  return;
	}
    }

  /* Nothing worked */
  syntax ();
  regopP->mode = 0;		/* Register r0 is always a good one */
  regopP->n = 0;
  regopP->special = 0;
}				/* parse_regop() */

/*****************************************************************************
   reg_fmt:	generate a REG-format instruction

  *************************************************************************** */
static void
reg_fmt (args, oP)
     char *args[];		/* args[0]->opcode mnemonic, args[1-3]->operands */
     struct i960_opcode *oP;	/* Pointer to description of instruction */
{
  long instr;			/* Binary to be output */
  struct regop regop;		/* Description of register operand */
  int n_ops;			/* Number of operands */

  instr = oP->opcode;
  n_ops = oP->num_ops;

  if (n_ops >= 1)
    {
      parse_regop (&regop, args[1], oP->operand[0]);

      if ((n_ops == 1) && !(instr & M3))
	{
	  /* 1-operand instruction in which the dst field should
	     * be used (instead of src1).
	   */
	  regop.n <<= 19;
	  if (regop.special)
	    {
	      regop.mode = regop.special;
	    }
	  regop.mode <<= 13;
	  regop.special = 0;
	}
      else
	{
	  /* regop.n goes in bit 0, needs no shifting */
	  regop.mode <<= 11;
	  regop.special <<= 5;
	}
      instr |= regop.n | regop.mode | regop.special;
    }

  if (n_ops >= 2)
    {
      parse_regop (&regop, args[2], oP->operand[1]);

      if ((n_ops == 2) && !(instr & M3))
	{
	  /* 2-operand instruction in which the dst field should
	     * be used instead of src2).
	   */
	  regop.n <<= 19;
	  if (regop.special)
	    {
	      regop.mode = regop.special;
	    }
	  regop.mode <<= 13;
	  regop.special = 0;
	}
      else
	{
	  regop.n <<= 14;
	  regop.mode <<= 12;
	  regop.special <<= 6;
	}
      instr |= regop.n | regop.mode | regop.special;
    }
  if (n_ops == 3)
    {
      parse_regop (&regop, args[3], oP->operand[2]);
      if (regop.special)
	{
	  regop.mode = regop.special;
	}
      instr |= (regop.n <<= 19) | (regop.mode <<= 13);
    }
  emit (instr);
}

/*****************************************************************************
   relax_cobr:
  	Replace cobr instruction in a code fragment with equivalent branch and
  	compare instructions, so it can reach beyond a 13-bit displacement.
  	Set up an address fix/relocation for the new branch instruction.

  *************************************************************************** */

/* This "conditional jump" table maps cobr instructions into
   equivalent compare and branch opcodes.  */
static const
struct
{
  long compare;
  long branch;
}

coj[] =
{				/* COBR OPCODE: */
  { CHKBIT, BNO },		/*      0x30 - bbc */
  { CMPO, BG },			/*      0x31 - cmpobg */
  { CMPO, BE },			/*      0x32 - cmpobe */
  { CMPO, BGE },		/*      0x33 - cmpobge */
  { CMPO, BL },			/*      0x34 - cmpobl */
  { CMPO, BNE },		/*      0x35 - cmpobne */
  { CMPO, BLE },		/*      0x36 - cmpoble */
  { CHKBIT, BO },		/*      0x37 - bbs */
  { CMPI, BNO },		/*      0x38 - cmpibno */
  { CMPI, BG },			/*      0x39 - cmpibg */
  { CMPI, BE },			/*      0x3a - cmpibe */
  { CMPI, BGE },		/*      0x3b - cmpibge */
  { CMPI, BL },			/*      0x3c - cmpibl */
  { CMPI, BNE },		/*      0x3d - cmpibne */
  { CMPI, BLE },		/*      0x3e - cmpible */
  { CMPI, BO },			/*      0x3f - cmpibo */
};

static void
relax_cobr (fragP)
     register fragS *fragP;	/* fragP->fr_opcode is assumed to point to
				 * the cobr instruction, which comes at the
				 * end of the code fragment.
				 */
{
  int opcode, src1, src2, m1, s2;
  /* Bit fields from cobr instruction */
  long bp_bits;			/* Branch prediction bits from cobr instruction */
  long instr;			/* A single i960 instruction */
  /* ->instruction to be replaced */
  char *iP;
  fixS *fixP;			/* Relocation that can be done at assembly time */

  /* PICK UP & PARSE COBR INSTRUCTION */
  iP = fragP->fr_opcode;
  instr = md_chars_to_number (iP, 4);
  opcode = ((instr >> 24) & 0xff) - 0x30;	/* "-0x30" for table index */
  src1 = (instr >> 19) & 0x1f;
  m1 = (instr >> 13) & 1;
  s2 = instr & 1;
  src2 = (instr >> 14) & 0x1f;
  bp_bits = instr & BP_MASK;

  /* GENERATE AND OUTPUT COMPARE INSTRUCTION */
  instr = coj[opcode].compare
    | src1 | (m1 << 11) | (s2 << 6) | (src2 << 14);
  md_number_to_chars (iP, instr, 4);

  /* OUTPUT BRANCH INSTRUCTION */
  md_number_to_chars (iP + 4, coj[opcode].branch | bp_bits, 4);

  /* SET UP ADDRESS FIXUP/RELOCATION */
  fixP = fix_new (fragP,
		  iP + 4 - fragP->fr_literal,
		  4,
		  fragP->fr_symbol,
		  fragP->fr_offset,
		  1,
		  NO_RELOC);

  fixP->fx_bit_fixP = (bit_fixS *) 24;	/* Store size of bit field */

  fragP->fr_fix += 4;
  frag_wane (fragP);
}

/*****************************************************************************
   reloc_callj:	Relocate a 'callj' instruction

  	This is a "non-(GNU)-standard" machine-dependent hook.  The base
  	assembler calls it when it decides it can relocate an address at
  	assembly time instead of emitting a relocation directive.

  	Check to see if the relocation involves a 'callj' instruction to a:
  	    sysproc:	Replace the default 'call' instruction with a 'calls'
  	    leafproc:	Replace the default 'call' instruction with a 'bal'.
  	    other proc:	Do nothing.

  	See b.out.h for details on the 'n_other' field in a symbol structure.

   IMPORTANT!:
  	Assumes the caller has already figured out, in the case of a leafproc,
  	to use the 'bal' entry point, and has substituted that symbol into the
  	passed fixup structure.

  *************************************************************************** */
int
reloc_callj (fixP)
     /* Relocation that can be done at assembly time */
     fixS *fixP;
{
  /* Points to the binary for the instruction being relocated.  */
  char *where;

  if (!fixP->fx_tcbit)
    {
      /* This wasn't a callj instruction in the first place */
      return 0;
    }

  where = fixP->fx_frag->fr_literal + fixP->fx_where;

  if (TC_S_IS_SYSPROC (fixP->fx_addsy))
    {
      /* Symbol is a .sysproc: replace 'call' with 'calls'.  System
         procedure number is (other-1).  */
      md_number_to_chars (where, CALLS | TC_S_GET_SYSPROC (fixP->fx_addsy), 4);

      /* Nothing else needs to be done for this instruction.  Make
         sure 'md_number_to_field()' will perform a no-op.  */
      fixP->fx_bit_fixP = (bit_fixS *) 1;
    }
  else if (TC_S_IS_CALLNAME (fixP->fx_addsy))
    {
      /* Should not happen: see block comment above */
      as_fatal (_("Trying to 'bal' to %s"), S_GET_NAME (fixP->fx_addsy));
    }
  else if (TC_S_IS_BALNAME (fixP->fx_addsy))
    {
      /* Replace 'call' with 'bal'; both instructions have the same
         format, so calling code should complete relocation as if
         nothing happened here.  */
      md_number_to_chars (where, BAL, 4);
    }
  else if (TC_S_IS_BADPROC (fixP->fx_addsy))
    {
      as_bad (_("Looks like a proc, but can't tell what kind.\n"));
    }				/* switch on proc type */

  /* else Symbol is neither a sysproc nor a leafproc */
  return 0;
}

/*****************************************************************************
   s_leafproc:	process .leafproc pseudo-op

  	.leafproc takes two arguments, the second one is optional:
  		arg[1]: name of 'call' entry point to leaf procedure
  		arg[2]: name of 'bal' entry point to leaf procedure

  	If the two arguments are identical, or if the second one is missing,
  	the first argument is taken to be the 'bal' entry point.

  	If there are 2 distinct arguments, we must make sure that the 'bal'
  	entry point immediately follows the 'call' entry point in the linked
  	list of symbols.

  *************************************************************************** */
static void
s_leafproc (n_ops, args)
     int n_ops;			/* Number of operands */
     char *args[];		/* args[1]->1st operand, args[2]->2nd operand */
{
  symbolS *callP;		/* Pointer to leafproc 'call' entry point symbol */
  symbolS *balP;		/* Pointer to leafproc 'bal' entry point symbol */

  if ((n_ops != 1) && (n_ops != 2))
    {
      as_bad (_("should have 1 or 2 operands"));
      return;
    }				/* Check number of arguments */

  /* Find or create symbol for 'call' entry point.  */
  callP = symbol_find_or_make (args[1]);

  if (TC_S_IS_CALLNAME (callP))
    {
      as_warn (_("Redefining leafproc %s"), S_GET_NAME (callP));
    }				/* is leafproc */

  /* If that was the only argument, use it as the 'bal' entry point.
     * Otherwise, mark it as the 'call' entry point and find or create
     * another symbol for the 'bal' entry point.
   */
  if ((n_ops == 1) || !strcmp (args[1], args[2]))
    {
      TC_S_FORCE_TO_BALNAME (callP);

    }
  else
    {
      TC_S_FORCE_TO_CALLNAME (callP);

      balP = symbol_find_or_make (args[2]);
      if (TC_S_IS_CALLNAME (balP))
	{
	  as_warn (_("Redefining leafproc %s"), S_GET_NAME (balP));
	}
      TC_S_FORCE_TO_BALNAME (balP);

#ifndef OBJ_ELF
      tc_set_bal_of_call (callP, balP);
#endif
    }				/* if only one arg, or the args are the same */
}

/*
   s_sysproc: process .sysproc pseudo-op

        .sysproc takes two arguments:
                arg[1]: name of entry point to system procedure
                arg[2]: 'entry_num' (index) of system procedure in the range
                        [0,31] inclusive.

        For [ab].out, we store the 'entrynum' in the 'n_other' field of
        the symbol.  Since that entry is normally 0, we bias 'entrynum'
        by adding 1 to it.  It must be unbiased before it is used.  */
static void
s_sysproc (n_ops, args)
     int n_ops;			/* Number of operands */
     char *args[];		/* args[1]->1st operand, args[2]->2nd operand */
{
  expressionS exp;
  symbolS *symP;

  if (n_ops != 2)
    {
      as_bad (_("should have two operands"));
      return;
    }				/* bad arg count */

  /* Parse "entry_num" argument and check it for validity.  */
  parse_expr (args[2], &exp);
  if (exp.X_op != O_constant
      || (offs (exp) < 0)
      || (offs (exp) > 31))
    {
      as_bad (_("'entry_num' must be absolute number in [0,31]"));
      return;
    }

  /* Find/make symbol and stick entry number (biased by +1) into it */
  symP = symbol_find_or_make (args[1]);

  if (TC_S_IS_SYSPROC (symP))
    {
      as_warn (_("Redefining entrynum for sysproc %s"), S_GET_NAME (symP));
    }				/* redefining */

  TC_S_SET_SYSPROC (symP, offs (exp));	/* encode entry number */
  TC_S_FORCE_TO_SYSPROC (symP);
}

/*****************************************************************************
   shift_ok:
  	Determine if a "shlo" instruction can be used to implement a "ldconst".
  	This means that some number X < 32 can be shifted left to produce the
  	constant of interest.

  	Return the shift count, or 0 if we can't do it.
  	Caller calculates X by shifting original constant right 'shift' places.

  *************************************************************************** */
static int
shift_ok (n)
     int n;			/* The constant of interest */
{
  int shift;			/* The shift count */

  if (n <= 0)
    {
      /* Can't do it for negative numbers */
      return 0;
    }

  /* Shift 'n' right until a 1 is about to be lost */
  for (shift = 0; (n & 1) == 0; shift++)
    {
      n >>= 1;
    }

  if (n >= 32)
    {
      return 0;
    }
  return shift;
}

/* syntax: issue syntax error */

static void
syntax ()
{
  as_bad (_("syntax error"));
}				/* syntax() */

/* targ_has_sfr:

   Return TRUE iff the target architecture supports the specified
   special-function register (sfr).  */

static int
targ_has_sfr (n)
     int n;			/* Number (0-31) of sfr */
{
  switch (architecture)
    {
    case ARCH_KA:
    case ARCH_KB:
    case ARCH_MC:
    case ARCH_JX:
      return 0;
    case ARCH_HX:
      return ((0 <= n) && (n <= 4));
    case ARCH_CA:
    default:
      return ((0 <= n) && (n <= 2));
    }
}

/* targ_has_iclass:

   Return TRUE iff the target architecture supports the indicated
   class of instructions.  */
static int
targ_has_iclass (ic)
     /* Instruction class;  one of:
        I_BASE, I_CX, I_DEC, I_KX, I_FP, I_MIL, I_CASIM, I_CX2, I_HX, I_HX2
      */
     int ic;
{
  iclasses_seen |= ic;
  switch (architecture)
    {
    case ARCH_KA:
      return ic & (I_BASE | I_KX);
    case ARCH_KB:
      return ic & (I_BASE | I_KX | I_FP | I_DEC);
    case ARCH_MC:
      return ic & (I_BASE | I_KX | I_FP | I_DEC | I_MIL);
    case ARCH_CA:
      return ic & (I_BASE | I_CX | I_CX2 | I_CASIM);
    case ARCH_JX:
      return ic & (I_BASE | I_CX2 | I_JX);
    case ARCH_HX:
      return ic & (I_BASE | I_CX2 | I_JX | I_HX);
    default:
      if ((iclasses_seen & (I_KX | I_FP | I_DEC | I_MIL))
	  && (iclasses_seen & (I_CX | I_CX2)))
	{
	  as_warn (_("architecture of opcode conflicts with that of earlier instruction(s)"));
	  iclasses_seen &= ~ic;
	}
      return 1;
    }
}

/* Handle the MRI .endian pseudo-op.  */

static void
s_endian (ignore)
     int ignore ATTRIBUTE_UNUSED;
{
  char *name;
  char c;

  name = input_line_pointer;
  c = get_symbol_end ();
  if (strcasecmp (name, "little") == 0)
    ;
  else if (strcasecmp (name, "big") == 0)
    as_bad (_("big endian mode is not supported"));
  else
    as_warn (_("ignoring unrecognized .endian type `%s'"), name);

  *input_line_pointer = c;

  demand_empty_rest_of_line ();
}

/* We have no need to default values of symbols.  */

symbolS *
md_undefined_symbol (name)
     char *name ATTRIBUTE_UNUSED;
{
  return 0;
}

/* Exactly what point is a PC-relative offset relative TO?
   On the i960, they're relative to the address of the instruction,
   which we have set up as the address of the fixup too.  */
long
md_pcrel_from (fixP)
     fixS *fixP;
{
  return fixP->fx_where + fixP->fx_frag->fr_address;
}

void
md_apply_fix3 (fixP, valP, seg)
     fixS *fixP;
     valueT *valP;
     segT seg ATTRIBUTE_UNUSED;
{
  long val = *valP;
  char *place = fixP->fx_where + fixP->fx_frag->fr_literal;

  if (!fixP->fx_bit_fixP)
    {
#ifndef BFD_ASSEMBLER
      /* For callx, we always want to write out zero, and emit a
	 symbolic relocation.  */
      if (fixP->fx_bsr)
	val = 0;

      fixP->fx_addnumber = val;
#endif

      md_number_to_imm (place, val, fixP->fx_size);
    }
  else if ((int) fixP->fx_bit_fixP == 13
	   && fixP->fx_addsy != NULL
	   && S_GET_SEGMENT (fixP->fx_addsy) == undefined_section)
    {
      /* This is a COBR instruction.  They have only a
	 13-bit displacement and are only to be used
	 for local branches: flag as error, don't generate
	 relocation.  */
      as_bad_where (fixP->fx_file, fixP->fx_line,
		    _("can't use COBR format with external label"));
      fixP->fx_addsy = NULL;
    }
  else
    md_number_to_field (place, val, fixP->fx_bit_fixP);

  if (fixP->fx_addsy == NULL)
    fixP->fx_done = 1;
}

#if defined(OBJ_AOUT) | defined(OBJ_BOUT)
void
tc_bout_fix_to_chars (where, fixP, segment_address_in_file)
     char *where;
     fixS *fixP;
     relax_addressT segment_address_in_file;
{
  static const unsigned char nbytes_r_length[] = {42, 0, 1, 42, 2};
  struct relocation_info ri;
  symbolS *symbolP;

  memset ((char *) &ri, '\0', sizeof (ri));
  symbolP = fixP->fx_addsy;
  know (symbolP != 0 || fixP->fx_r_type != NO_RELOC);
  ri.r_bsr = fixP->fx_bsr;	/*SAC LD RELAX HACK */
  /* These two 'cuz of NS32K */
  ri.r_callj = fixP->fx_tcbit;
  if (fixP->fx_bit_fixP)
    ri.r_length = 2;
  else
    ri.r_length = nbytes_r_length[fixP->fx_size];
  ri.r_pcrel = fixP->fx_pcrel;
  ri.r_address = fixP->fx_frag->fr_address + fixP->fx_where - segment_address_in_file;

  if (fixP->fx_r_type != NO_RELOC)
    {
      switch (fixP->fx_r_type)
	{
	case rs_align:
	  ri.r_index = -2;
	  ri.r_pcrel = 1;
	  ri.r_length = fixP->fx_size - 1;
	  break;
	case rs_org:
	  ri.r_index = -2;
	  ri.r_pcrel = 0;
	  break;
	case rs_fill:
	  ri.r_index = -1;
	  break;
	default:
	  abort ();
	}
      ri.r_extern = 0;
    }
  else if (linkrelax || !S_IS_DEFINED (symbolP) || fixP->fx_bsr)
    {
      ri.r_extern = 1;
      ri.r_index = symbolP->sy_number;
    }
  else
    {
      ri.r_extern = 0;
      ri.r_index = S_GET_TYPE (symbolP);
    }

  /* Output the relocation information in machine-dependent form.  */
  md_ri_to_chars (where, &ri);
}

#endif /* OBJ_AOUT or OBJ_BOUT */

#if defined (OBJ_COFF) && defined (BFD)
short
tc_coff_fix2rtype (fixP)
     fixS *fixP;
{
  if (fixP->fx_bsr)
    abort ();

  if (fixP->fx_pcrel == 0 && fixP->fx_size == 4)
    return R_RELLONG;

  if (fixP->fx_pcrel != 0 && fixP->fx_size == 4)
    return R_IPRMED;

  abort ();
  return 0;
}

int
tc_coff_sizemachdep (frag)
     fragS *frag;
{
  if (frag->fr_next)
    return frag->fr_next->fr_address - frag->fr_address;
  else
    return 0;
}
#endif

/* Align an address by rounding it up to the specified boundary.  */
valueT
md_section_align (seg, addr)
     segT seg;
     valueT addr;		/* Address to be rounded up */
{
  int align;
#ifdef BFD_ASSEMBLER
  align = bfd_get_section_alignment (stdoutput, seg);
#else
  align = section_alignment[(int) seg];
#endif
  return (addr + (1 << align) - 1) & (-1 << align);
}

extern int coff_flags;

#ifdef OBJ_COFF
void
tc_headers_hook (headers)
     object_headers *headers;
{
  switch (architecture)
    {
    case ARCH_KA:
      coff_flags |= F_I960KA;
      break;

    case ARCH_KB:
      coff_flags |= F_I960KB;
      break;

    case ARCH_MC:
      coff_flags |= F_I960MC;
      break;

    case ARCH_CA:
      coff_flags |= F_I960CA;
      break;

    case ARCH_JX:
      coff_flags |= F_I960JX;
      break;

    case ARCH_HX:
      coff_flags |= F_I960HX;
      break;

    default:
      if (iclasses_seen == I_BASE)
	coff_flags |= F_I960CORE;
      else if (iclasses_seen & I_CX)
	coff_flags |= F_I960CA;
      else if (iclasses_seen & I_HX)
	coff_flags |= F_I960HX;
      else if (iclasses_seen & I_JX)
	coff_flags |= F_I960JX;
      else if (iclasses_seen & I_CX2)
	coff_flags |= F_I960CA;
      else if (iclasses_seen & I_MIL)
	coff_flags |= F_I960MC;
      else if (iclasses_seen & (I_DEC | I_FP))
	coff_flags |= F_I960KB;
      else
	coff_flags |= F_I960KA;
      break;
    }

  if (flag_readonly_data_in_text)
    {
      headers->filehdr.f_magic = I960RWMAGIC;
      headers->aouthdr.magic = OMAGIC;
    }
  else
    {
      headers->filehdr.f_magic = I960ROMAGIC;
      headers->aouthdr.magic = NMAGIC;
    }				/* set magic numbers */
}

#endif /* OBJ_COFF */

#ifndef BFD_ASSEMBLER

/* Things going on here:

   For bout, We need to assure a couple of simplifying
   assumptions about leafprocs for the linker: the leafproc
   entry symbols will be defined in the same assembly in
   which they're declared with the '.leafproc' directive;
   and if a leafproc has both 'call' and 'bal' entry points
   they are both global or both local.

   For coff, the call symbol has a second aux entry that
   contains the bal entry point.  The bal symbol becomes a
   label.

   For coff representation, the call symbol has a second aux entry that
   contains the bal entry point.  The bal symbol becomes a label.  */

void
tc_crawl_symbol_chain (headers)
     object_headers *headers ATTRIBUTE_UNUSED;
{
  symbolS *symbolP;

  for (symbolP = symbol_rootP; symbolP; symbolP = symbol_next (symbolP))
    {
#ifdef OBJ_COFF
      if (TC_S_IS_SYSPROC (symbolP))
	{
	  /* second aux entry already contains the sysproc number */
	  S_SET_NUMBER_AUXILIARY (symbolP, 2);
	  S_SET_STORAGE_CLASS (symbolP, C_SCALL);
	  S_SET_DATA_TYPE (symbolP, S_GET_DATA_TYPE (symbolP) | (DT_FCN << N_BTSHFT));
	  continue;
	}			/* rewrite sysproc */
#endif /* OBJ_COFF */

      if (!TC_S_IS_BALNAME (symbolP) && !TC_S_IS_CALLNAME (symbolP))
	{
	  continue;
	}			/* Not a leafproc symbol */

      if (!S_IS_DEFINED (symbolP))
	{
	  as_bad (_("leafproc symbol '%s' undefined"), S_GET_NAME (symbolP));
	}			/* undefined leaf */

      if (TC_S_IS_CALLNAME (symbolP))
	{
	  symbolS *balP = tc_get_bal_of_call (symbolP);
	  if (S_IS_EXTERNAL (symbolP) != S_IS_EXTERNAL (balP))
	    {
	      S_SET_EXTERNAL (symbolP);
	      S_SET_EXTERNAL (balP);
	      as_warn (_("Warning: making leafproc entries %s and %s both global\n"),
		       S_GET_NAME (symbolP), S_GET_NAME (balP));
	    }			/* externality mismatch */
	}			/* if callname */
    }				/* walk the symbol chain */
}

#endif /* ! BFD_ASSEMBLER */

/* For aout or bout, the bal immediately follows the call.

   For coff, we cheat and store a pointer to the bal symbol in the
   second aux entry of the call.  */

#undef OBJ_ABOUT
#ifdef OBJ_AOUT
#define OBJ_ABOUT
#endif
#ifdef OBJ_BOUT
#define OBJ_ABOUT
#endif

void
tc_set_bal_of_call (callP, balP)
     symbolS *callP ATTRIBUTE_UNUSED;
     symbolS *balP ATTRIBUTE_UNUSED;
{
  know (TC_S_IS_CALLNAME (callP));
  know (TC_S_IS_BALNAME (balP));

#ifdef OBJ_COFF

  callP->sy_tc = balP;
  S_SET_NUMBER_AUXILIARY (callP, 2);

#else /* ! OBJ_COFF */
#ifdef OBJ_ABOUT

  /* If the 'bal' entry doesn't immediately follow the 'call'
     * symbol, unlink it from the symbol list and re-insert it.
   */
  if (symbol_next (callP) != balP)
    {
      symbol_remove (balP, &symbol_rootP, &symbol_lastP);
      symbol_append (balP, callP, &symbol_rootP, &symbol_lastP);
    }				/* if not in order */

#else /* ! OBJ_ABOUT */
  as_fatal ("Only supported for a.out, b.out, or COFF");
#endif /* ! OBJ_ABOUT */
#endif /* ! OBJ_COFF */
}

symbolS *
tc_get_bal_of_call (callP)
     symbolS *callP ATTRIBUTE_UNUSED;
{
  symbolS *retval;

  know (TC_S_IS_CALLNAME (callP));

#ifdef OBJ_COFF
  retval = callP->sy_tc;
#else
#ifdef OBJ_ABOUT
  retval = symbol_next (callP);
#else
  as_fatal ("Only supported for a.out, b.out, or COFF");
#endif /* ! OBJ_ABOUT */
#endif /* ! OBJ_COFF */

  know (TC_S_IS_BALNAME (retval));
  return retval;
}				/* _tc_get_bal_of_call() */

#ifdef OBJ_COFF
void
tc_coff_symbol_emit_hook (symbolP)
     symbolS *symbolP ATTRIBUTE_UNUSED;
{
  if (TC_S_IS_CALLNAME (symbolP))
    {
      symbolS *balP = tc_get_bal_of_call (symbolP);

#if 0
      /* second aux entry contains the bal entry point */
      S_SET_NUMBER_AUXILIARY (symbolP, 2);
#endif
      symbolP->sy_symbol.ost_auxent[1].x_bal.x_balntry = S_GET_VALUE (balP);
      if (S_GET_STORAGE_CLASS (symbolP) == C_EXT)
	S_SET_STORAGE_CLASS (symbolP, C_LEAFEXT);
      else
	S_SET_STORAGE_CLASS (symbolP, C_LEAFSTAT);
      S_SET_DATA_TYPE (symbolP, S_GET_DATA_TYPE (symbolP) | (DT_FCN << N_BTSHFT));
      /* fix up the bal symbol */
      S_SET_STORAGE_CLASS (balP, C_LABEL);
    }				/* only on calls */
}
#endif /* OBJ_COFF */

void
i960_handle_align (fragp)
     fragS *fragp ATTRIBUTE_UNUSED;
{
  if (!linkrelax)
    return;

#ifndef OBJ_BOUT

  as_bad (_("option --link-relax is only supported in b.out format"));
  linkrelax = 0;
  return;

#else

  /* The text section "ends" with another alignment reloc, to which we
     aren't adding padding.  */
  if (fragp->fr_next == text_last_frag
      || fragp->fr_next == data_last_frag)
    return;

  /* alignment directive */
  fix_new (fragp, fragp->fr_fix, fragp->fr_offset, 0, 0, 0,
	   (int) fragp->fr_type);
#endif /* OBJ_BOUT */
}

int
i960_validate_fix (fixP, this_segment_type)
     fixS *fixP;
     segT this_segment_type ATTRIBUTE_UNUSED;
{
  if (fixP->fx_tcbit && TC_S_IS_CALLNAME (fixP->fx_addsy))
    {
      /* Relocation should be done via the associated 'bal'
         entry point symbol.  */

      if (!TC_S_IS_BALNAME (tc_get_bal_of_call (fixP->fx_addsy)))
	{
	  as_bad_where (fixP->fx_file, fixP->fx_line,
			_("No 'bal' entry point for leafproc %s"),
			S_GET_NAME (fixP->fx_addsy));
	  return 0;
	}
      fixP->fx_addsy = tc_get_bal_of_call (fixP->fx_addsy);
    }

  return 1;
}

#ifdef BFD_ASSEMBLER

/* From cgen.c:  */

static short tc_bfd_fix2rtype PARAMS ((fixS *));

static short
tc_bfd_fix2rtype (fixP)
     fixS *fixP;
{
#if 0
  if (fixP->fx_bsr)
    abort ();
#endif

  if (fixP->fx_pcrel == 0 && fixP->fx_size == 4)
    return BFD_RELOC_32;

  if (fixP->fx_pcrel != 0 && fixP->fx_size == 4)
    return BFD_RELOC_24_PCREL;

  abort ();
  return 0;
}

/* Translate internal representation of relocation info to BFD target
   format.

   FIXME: To what extent can we get all relevant targets to use this?  */

arelent *
tc_gen_reloc (section, fixP)
     asection *section ATTRIBUTE_UNUSED;
     fixS *fixP;
{
  arelent * reloc;

  reloc = (arelent *) xmalloc (sizeof (arelent));

  /* HACK: Is this right? */
  fixP->fx_r_type = tc_bfd_fix2rtype (fixP);

  reloc->howto = bfd_reloc_type_lookup (stdoutput, fixP->fx_r_type);
  if (reloc->howto == (reloc_howto_type *) NULL)
    {
      as_bad_where (fixP->fx_file, fixP->fx_line,
		    "internal error: can't export reloc type %d (`%s')",
		    fixP->fx_r_type,
		    bfd_get_reloc_code_name (fixP->fx_r_type));
      return NULL;
    }

  assert (!fixP->fx_pcrel == !reloc->howto->pc_relative);

  reloc->sym_ptr_ptr = (asymbol **) xmalloc (sizeof (asymbol *));
  *reloc->sym_ptr_ptr = symbol_get_bfdsym (fixP->fx_addsy);
  reloc->address = fixP->fx_frag->fr_address + fixP->fx_where;
  reloc->addend = fixP->fx_addnumber;

  return reloc;
}

/* end from cgen.c */

#endif /* BFD_ASSEMBLER */

/* end of tc-i960.c */
