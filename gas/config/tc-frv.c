/* tc-frv.c -- Assembler for the Fujitsu FRV.
   Copyright 2002, 2003 Free Software Foundation.

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
   along with GAS; see the file COPYING.  If not, write to
   the Free Software Foundation, 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

#include <stdio.h>
#include "as.h"
#include "subsegs.h"     
#include "symcat.h"
#include "opcodes/frv-desc.h"
#include "opcodes/frv-opc.h"
#include "cgen.h"
#include "libbfd.h"
#include "elf/common.h"
#include "elf/frv.h"

/* Structure to hold all of the different components describing
   an individual instruction.  */
typedef struct
{
  const CGEN_INSN *	insn;
  const CGEN_INSN *	orig_insn;
  CGEN_FIELDS		fields;
#if CGEN_INT_INSN_P
  CGEN_INSN_INT         buffer [1];
#define INSN_VALUE(buf) (*(buf))
#else
  unsigned char         buffer [CGEN_MAX_INSN_SIZE];
#define INSN_VALUE(buf) (buf)
#endif
  char *		addr;
  fragS *		frag;
  int                   num_fixups;
  fixS *                fixups [GAS_CGEN_MAX_FIXUPS];
  int                   indices [MAX_OPERAND_INSTANCES];
}
frv_insn;

enum vliw_insn_type
{
  VLIW_GENERIC_TYPE,		/* Don't care about this insn.  */
  VLIW_BRANCH_TYPE,		/* A Branch.  */
  VLIW_LABEL_TYPE,		/* A Label.  */
  VLIW_NOP_TYPE,		/* A NOP.  */
  VLIW_BRANCH_HAS_NOPS		/* A Branch that requires NOPS.  */
};

/* We're going to use these in the fr_subtype field to mark 
   whether to keep inserted nops.  */

#define NOP_KEEP 1		/* Keep these NOPS.  */
#define NOP_DELETE 2		/* Delete these NOPS.  */

#define DO_COUNT    TRUE
#define DONT_COUNT  FALSE

/* A list of insns within a VLIW insn.  */
struct vliw_insn_list
{
  /*  The type of this insn.  */
  enum vliw_insn_type	type;

  /*  The corresponding gas insn information.  */
  const CGEN_INSN	*insn;

  /*  For branches and labels, the symbol that is referenced.  */
  symbolS		*sym;

  /*  For branches, the frag containing the single nop that was generated.  */
  fragS			*snop_frag;

  /*  For branches, the frag containing the double nop that was generated.  */
  fragS			*dnop_frag;

  /*  Pointer to raw data for this insn.  */
  char			*address;

  /* Next insn in list.  */
  struct vliw_insn_list *next;
};

static struct vliw_insn_list single_nop_insn = {
   VLIW_NOP_TYPE, NULL, NULL, NULL, NULL, NULL, NULL };

static struct vliw_insn_list double_nop_insn = {
   VLIW_NOP_TYPE, NULL, NULL, NULL, NULL, NULL, NULL };

struct vliw_chain
{
  int			num;
  int			insn_count;
  struct vliw_insn_list *insn_list;
  struct vliw_chain     *next;
};

static struct vliw_chain	*vliw_chain_top;
static struct vliw_chain	*current_vliw_chain;
static struct vliw_chain	*previous_vliw_chain;
static struct vliw_insn_list	*current_vliw_insn;

const char comment_chars[]        = ";";
const char line_comment_chars[]   = "#";
const char line_separator_chars[] = "!"; 
const char EXP_CHARS[]            = "eE";
const char FLT_CHARS[]            = "dD";

static FRV_VLIW vliw;

/* Default machine */

#ifdef  DEFAULT_CPU_FRV
#define DEFAULT_MACHINE bfd_mach_frv
#define DEFAULT_FLAGS	EF_FRV_CPU_GENERIC

#else
#ifdef  DEFAULT_CPU_FR300
#define DEFAULT_MACHINE	bfd_mach_fr300
#define DEFAULT_FLAGS	EF_FRV_CPU_FR300

#else
#ifdef	DEFAULT_CPU_SIMPLE
#define	DEFAULT_MACHINE bfd_mach_frvsimple
#define DEFAULT_FLAGS	EF_FRV_CPU_SIMPLE

#else
#ifdef	DEFAULT_CPU_TOMCAT
#define	DEFAULT_MACHINE bfd_mach_frvtomcat
#define DEFAULT_FLAGS	EF_FRV_CPU_TOMCAT

#else
#ifdef  DEFAULT_CPU_FR400
#define DEFAULT_MACHINE	bfd_mach_fr400
#define DEFAULT_FLAGS	EF_FRV_CPU_FR400

#else
#ifdef  DEFAULT_CPU_FR550
#define DEFAULT_MACHINE	bfd_mach_fr550
#define DEFAULT_FLAGS	EF_FRV_CPU_FR550

#else
#define DEFAULT_MACHINE	bfd_mach_fr500
#define DEFAULT_FLAGS	EF_FRV_CPU_FR500
#endif
#endif
#endif
#endif
#endif
#endif

#ifdef TE_LINUX
# define DEFAULT_FDPIC	EF_FRV_FDPIC
#else
# define DEFAULT_FDPIC	0
#endif

static unsigned long frv_mach = bfd_mach_frv;

/* Flags to set in the elf header */
static flagword frv_flags = DEFAULT_FLAGS | DEFAULT_FDPIC;

static int frv_user_set_flags_p = 0;
static int frv_pic_p = 0;
static const char *frv_pic_flag = DEFAULT_FDPIC ? "-mfdpic" : (const char *)0;

/* Print tomcat-specific debugging info.  */
static int tomcat_debug = 0;

/* Tomcat-specific NOP statistics.  */
static int tomcat_stats = 0;
static int tomcat_doubles = 0;
static int tomcat_singles = 0;

/* Forward reference to static functions */
static void frv_set_flags		PARAMS ((int));
static void frv_pic_ptr			PARAMS ((int));
static void frv_frob_file_section	PARAMS ((bfd *, asection *, PTR));

/* The target specific pseudo-ops which we support.  */
const pseudo_typeS md_pseudo_table[] =
{
  { "eflags",	frv_set_flags,		0 },
  { "word",	cons,			4 },
  { "picptr",	frv_pic_ptr,		4 },
  { NULL, 	NULL,			0 }
};


#define FRV_SHORTOPTS "G:"
const char * md_shortopts = FRV_SHORTOPTS;

#define OPTION_GPR_32		(OPTION_MD_BASE)
#define OPTION_GPR_64		(OPTION_MD_BASE + 1)
#define OPTION_FPR_32		(OPTION_MD_BASE + 2)
#define OPTION_FPR_64		(OPTION_MD_BASE + 3)
#define OPTION_SOFT_FLOAT	(OPTION_MD_BASE + 4)
#define OPTION_DWORD_YES	(OPTION_MD_BASE + 5)
#define OPTION_DWORD_NO		(OPTION_MD_BASE + 6)
#define OPTION_DOUBLE		(OPTION_MD_BASE + 7)
#define OPTION_NO_DOUBLE	(OPTION_MD_BASE + 8)
#define OPTION_MEDIA		(OPTION_MD_BASE + 9)
#define OPTION_NO_MEDIA		(OPTION_MD_BASE + 10)
#define OPTION_CPU		(OPTION_MD_BASE + 11)
#define OPTION_PIC		(OPTION_MD_BASE + 12)
#define OPTION_BIGPIC		(OPTION_MD_BASE + 13)
#define OPTION_LIBPIC		(OPTION_MD_BASE + 14)
#define OPTION_MULADD		(OPTION_MD_BASE + 15)
#define OPTION_NO_MULADD	(OPTION_MD_BASE + 16)
#define OPTION_TOMCAT_DEBUG	(OPTION_MD_BASE + 17)
#define OPTION_TOMCAT_STATS	(OPTION_MD_BASE + 18)
#define OPTION_PACK	        (OPTION_MD_BASE + 19)
#define OPTION_NO_PACK	        (OPTION_MD_BASE + 20)
#define OPTION_FDPIC		(OPTION_MD_BASE + 21)
#define OPTION_NOPIC		(OPTION_MD_BASE + 22)

struct option md_longopts[] =
{
  { "mgpr-32",		no_argument,		NULL, OPTION_GPR_32        },
  { "mgpr-64",		no_argument,		NULL, OPTION_GPR_64        },
  { "mfpr-32",		no_argument,		NULL, OPTION_FPR_32        },
  { "mfpr-64",		no_argument,		NULL, OPTION_FPR_64        },
  { "mhard-float",	no_argument,		NULL, OPTION_FPR_64        },
  { "msoft-float",	no_argument,		NULL, OPTION_SOFT_FLOAT    },
  { "mdword",		no_argument,		NULL, OPTION_DWORD_YES     },
  { "mno-dword",	no_argument,		NULL, OPTION_DWORD_NO      },
  { "mdouble",		no_argument,		NULL, OPTION_DOUBLE        },
  { "mno-double",	no_argument,		NULL, OPTION_NO_DOUBLE     },
  { "mmedia",		no_argument,		NULL, OPTION_MEDIA         },
  { "mno-media",	no_argument,		NULL, OPTION_NO_MEDIA      },
  { "mcpu",		required_argument,	NULL, OPTION_CPU           },
  { "mpic",		no_argument,		NULL, OPTION_PIC           },
  { "mPIC",		no_argument,		NULL, OPTION_BIGPIC        },
  { "mlibrary-pic",	no_argument,		NULL, OPTION_LIBPIC        },
  { "mmuladd",		no_argument,		NULL, OPTION_MULADD        },
  { "mno-muladd",	no_argument,		NULL, OPTION_NO_MULADD     },
  { "mtomcat-debug",    no_argument,            NULL, OPTION_TOMCAT_DEBUG  },
  { "mtomcat-stats",	no_argument,		NULL, OPTION_TOMCAT_STATS  },
  { "mpack",        	no_argument,		NULL, OPTION_PACK          },
  { "mno-pack",        	no_argument,		NULL, OPTION_NO_PACK       },
  { "mfdpic",		no_argument,		NULL, OPTION_FDPIC	   },
  { "mnopic",		no_argument,		NULL, OPTION_NOPIC	   },
  { NULL,		no_argument,		NULL, 0                 },
};

size_t md_longopts_size = sizeof (md_longopts);

/* What value to give to bfd_set_gp_size.  */
static int g_switch_value = 8;

int
md_parse_option (c, arg)
     int    c;
     char * arg;
{
  switch (c)
    {
    default:
      return 0;

    case 'G':
      g_switch_value = atoi (arg);
      if (! g_switch_value)
	frv_flags |= EF_FRV_G0;
      break;

    case OPTION_GPR_32:
      frv_flags = (frv_flags & ~EF_FRV_GPR_MASK) | EF_FRV_GPR_32;
      break;

    case OPTION_GPR_64:
      frv_flags = (frv_flags & ~EF_FRV_GPR_MASK) | EF_FRV_GPR_64;
      break;

    case OPTION_FPR_32:
      frv_flags = (frv_flags & ~EF_FRV_FPR_MASK) | EF_FRV_FPR_32;
      break;

    case OPTION_FPR_64:
      frv_flags = (frv_flags & ~EF_FRV_FPR_MASK) | EF_FRV_FPR_64;
      break;

    case OPTION_SOFT_FLOAT:
      frv_flags = (frv_flags & ~EF_FRV_FPR_MASK) | EF_FRV_FPR_NONE;
      break;

    case OPTION_DWORD_YES:
      frv_flags = (frv_flags & ~EF_FRV_DWORD_MASK) | EF_FRV_DWORD_YES;
      break;

    case OPTION_DWORD_NO:
      frv_flags = (frv_flags & ~EF_FRV_DWORD_MASK) | EF_FRV_DWORD_NO;
      break;

    case OPTION_DOUBLE:
      frv_flags |= EF_FRV_DOUBLE;
      break;

    case OPTION_NO_DOUBLE:
      frv_flags &= ~EF_FRV_DOUBLE;
      break;

    case OPTION_MEDIA:
      frv_flags |= EF_FRV_MEDIA;
      break;

    case OPTION_NO_MEDIA:
      frv_flags &= ~EF_FRV_MEDIA;
      break;

    case OPTION_MULADD:
      frv_flags |= EF_FRV_MULADD;
      break;

    case OPTION_NO_MULADD:
      frv_flags &= ~EF_FRV_MULADD;
      break;

    case OPTION_PACK:
      frv_flags &= ~EF_FRV_NOPACK;
      break;

    case OPTION_NO_PACK:
      frv_flags |= EF_FRV_NOPACK;
      break;

    case OPTION_CPU:
      {
	char *p;
	int cpu_flags = EF_FRV_CPU_GENERIC;

	/* Identify the processor type */
	p = arg;
	if (strcmp (p, "frv") == 0)
	  {
	    cpu_flags = EF_FRV_CPU_GENERIC;
	    frv_mach = bfd_mach_frv;
	  }

	else if (strcmp (p, "fr500") == 0)
	  {
	    cpu_flags = EF_FRV_CPU_FR500;
	    frv_mach = bfd_mach_fr500;
	  }

	else if (strcmp (p, "fr550") == 0)
	  {
	    cpu_flags = EF_FRV_CPU_FR550;
	    frv_mach = bfd_mach_fr550;
	  }

	else if (strcmp (p, "fr400") == 0)
	  {
	    cpu_flags = EF_FRV_CPU_FR400;
	    frv_mach = bfd_mach_fr400;
	  }

	else if (strcmp (p, "fr300") == 0)
	  {
	    cpu_flags = EF_FRV_CPU_FR300;
	    frv_mach = bfd_mach_fr300;
	  }

	else if (strcmp (p, "simple") == 0)
	  {
	    cpu_flags = EF_FRV_CPU_SIMPLE;
	    frv_mach = bfd_mach_frvsimple;
	    frv_flags |= EF_FRV_NOPACK;
	  }

        else if (strcmp (p, "tomcat") == 0)
          {
            cpu_flags = EF_FRV_CPU_TOMCAT;
            frv_mach = bfd_mach_frvtomcat;
          }

	else
	  {
	    as_fatal ("Unknown cpu -mcpu=%s", arg);
	    return 0;
	  }

	frv_flags = (frv_flags & ~EF_FRV_CPU_MASK) | cpu_flags;
      }
      break;

    case OPTION_PIC:
      frv_flags |= EF_FRV_PIC;
      frv_pic_p = 1;
      frv_pic_flag = "-fpic";
      break;

    case OPTION_BIGPIC:
      frv_flags |= EF_FRV_BIGPIC;
      frv_pic_p = 1;
      frv_pic_flag = "-fPIC";
      break;

    case OPTION_LIBPIC:
      frv_flags |= (EF_FRV_LIBPIC | EF_FRV_G0);
      frv_pic_p = 1;
      frv_pic_flag = "-mlibrary-pic";
      g_switch_value = 0;
      break;

    case OPTION_FDPIC:
      frv_flags |= EF_FRV_FDPIC;
      frv_pic_flag = "-mfdpic";
      break;

    case OPTION_NOPIC:
      frv_flags &= ~(EF_FRV_FDPIC | EF_FRV_PIC
		     | EF_FRV_BIGPIC | EF_FRV_LIBPIC);
      frv_pic_flag = 0;
      break;

    case OPTION_TOMCAT_DEBUG:
      tomcat_debug = 1;
      break;

    case OPTION_TOMCAT_STATS:
      tomcat_stats = 1;
      break;
    }

  return 1;
}

void
md_show_usage (stream)
  FILE * stream;
{
  fprintf (stream, _("FRV specific command line options:\n"));
  fprintf (stream, _("-G n         Data >= n bytes is in small data area\n"));
  fprintf (stream, _("-mgpr-32     Note 32 gprs are used\n"));
  fprintf (stream, _("-mgpr-64     Note 64 gprs are used\n"));
  fprintf (stream, _("-mfpr-32     Note 32 fprs are used\n"));
  fprintf (stream, _("-mfpr-64     Note 64 fprs are used\n"));
  fprintf (stream, _("-msoft-float Note software fp is used\n"));
  fprintf (stream, _("-mdword      Note stack is aligned to a 8 byte boundary\n"));
  fprintf (stream, _("-mno-dword   Note stack is aligned to a 4 byte boundary\n"));
  fprintf (stream, _("-mdouble     Note fp double insns are used\n"));
  fprintf (stream, _("-mmedia      Note media insns are used\n"));
  fprintf (stream, _("-mmuladd     Note multiply add/subtract insns are used\n"));
  fprintf (stream, _("-mpack       Note instructions are packed\n"));
  fprintf (stream, _("-mno-pack    Do not allow instructions to be packed\n"));
  fprintf (stream, _("-mpic        Note small position independent code\n"));
  fprintf (stream, _("-mPIC        Note large position independent code\n"));
  fprintf (stream, _("-mlibrary-pic Compile library for large position indepedent code\n"));
  fprintf (stream, _("-mfdpic      Assemble for the FDPIC ABI\n"));
  fprintf (stream, _("-mnopic      Disable -mpic, -mPIC, -mlibrary-pic and -mfdpic\n"));
  fprintf (stream, _("-mcpu={fr500|fr550|fr400|fr300|frv|simple|tomcat}\n"));
  fprintf (stream, _("             Record the cpu type\n"));
  fprintf (stream, _("-mtomcat-stats Print out stats for tomcat workarounds\n"));
  fprintf (stream, _("-mtomcat-debug Debug tomcat workarounds\n"));
} 


void
md_begin ()
{
  /* Initialize the `cgen' interface.  */
  
  /* Set the machine number and endian.  */
  gas_cgen_cpu_desc = frv_cgen_cpu_open (CGEN_CPU_OPEN_MACHS, 0,
					 CGEN_CPU_OPEN_ENDIAN,
					 CGEN_ENDIAN_BIG,
					 CGEN_CPU_OPEN_END);
  frv_cgen_init_asm (gas_cgen_cpu_desc);

  /* This is a callback from cgen to gas to parse operands.  */
  cgen_set_parse_operand_fn (gas_cgen_cpu_desc, gas_cgen_parse_operand);

  /* Set the ELF flags if desired. */
  if (frv_flags)
    bfd_set_private_flags (stdoutput, frv_flags);

  /* Set the machine type */
  bfd_default_set_arch_mach (stdoutput, bfd_arch_frv, frv_mach);

  /* Set up gp size so we can put local common items in .sbss */
  bfd_set_gp_size (stdoutput, g_switch_value);

  frv_vliw_reset (& vliw, frv_mach, frv_flags);
}

bfd_boolean
frv_md_fdpic_enabled (void)
{
  return (frv_flags & EF_FRV_FDPIC) != 0;
}

int chain_num = 0;

struct vliw_insn_list *frv_insert_vliw_insn PARAMS ((bfd_boolean));

struct vliw_insn_list *
frv_insert_vliw_insn (count)
      bfd_boolean count;
{
  struct vliw_insn_list *vliw_insn_list_entry;
  struct vliw_chain     *vliw_chain_entry;

  if (current_vliw_chain == NULL)
    {
      vliw_chain_entry = (struct vliw_chain *) xmalloc (sizeof (struct vliw_chain));
      vliw_chain_entry->insn_count = 0;
      vliw_chain_entry->insn_list  = NULL;
      vliw_chain_entry->next       = NULL;
      vliw_chain_entry->num        = chain_num++;

      if (!vliw_chain_top)
	vliw_chain_top = vliw_chain_entry;
      current_vliw_chain = vliw_chain_entry;
      if (previous_vliw_chain)
	previous_vliw_chain->next = vliw_chain_entry;
    }

  vliw_insn_list_entry = (struct vliw_insn_list *) xmalloc (sizeof (struct vliw_insn_list));
  vliw_insn_list_entry->type      = VLIW_GENERIC_TYPE;
  vliw_insn_list_entry->insn      = NULL;
  vliw_insn_list_entry->sym       = NULL;
  vliw_insn_list_entry->snop_frag = NULL;
  vliw_insn_list_entry->dnop_frag = NULL;
  vliw_insn_list_entry->next      = NULL;

  if (count)
    current_vliw_chain->insn_count++;

  if (current_vliw_insn)
    current_vliw_insn->next = vliw_insn_list_entry;
  current_vliw_insn = vliw_insn_list_entry;

  if (!current_vliw_chain->insn_list)
    current_vliw_chain->insn_list = current_vliw_insn;

  return vliw_insn_list_entry;
}

  /* Identify the following cases:
 
     1) A VLIW insn that contains both a branch and the branch destination.
        This requires the insertion of two vliw instructions before the
        branch.  The first consists of two nops.  The second consists of
        a single nop.
 
     2) A single instruction VLIW insn which is the destination of a branch
        that is in the next VLIW insn.  This requires the insertion of a vliw
        insn containing two nops before the branch.
 
     3) A double instruction VLIW insn which contains the destination of a
        branch that is in the next VLIW insn.  This requires the insertion of
        a VLIW insn containing a single nop before the branch.
 
     4) A single instruction VLIW insn which contains branch destination (x),
        followed by a single instruction VLIW insn which does not contain
        the branch to (x), followed by a VLIW insn which does contain the branch
        to (x).  This requires the insertion of a VLIW insn containing a single
        nop before the VLIW instruction containing the branch.
 
  */
#define FRV_IS_NOP(insn) (insn.buffer[0] == FRV_NOP_PACK || insn.buffer[0] == FRV_NOP_NOPACK)
#define FRV_NOP_PACK   0x00880000  /* ori.p  gr0,0,gr0 */
#define FRV_NOP_NOPACK 0x80880000  /* ori    gr0,0,gr0 */

/* Check a vliw insn for an insn of type containing the sym passed in label_sym.  */

static struct vliw_insn_list *frv_find_in_vliw
  PARAMS ((enum vliw_insn_type, struct vliw_chain *, symbolS *));

static struct vliw_insn_list *
frv_find_in_vliw (vliw_insn_type, this_chain, label_sym)
    enum vliw_insn_type vliw_insn_type;
    struct vliw_chain *this_chain;
    symbolS *label_sym;
{

  struct vliw_insn_list *the_insn;

  if (!this_chain)
    return NULL;

  for (the_insn = this_chain->insn_list; the_insn; the_insn = the_insn->next)
    {
      if (the_insn->type == vliw_insn_type
	  && the_insn->sym == label_sym)
	return the_insn;
    }

  return NULL;
}

enum vliw_nop_type
{
  /* A Vliw insn containing a single nop insn.  */
  VLIW_SINGLE_NOP,
  
  /* A Vliw insn containing two nop insns.  */
  VLIW_DOUBLE_NOP,

  /* Two vliw insns.  The first containing two nop insns.  
     The second contain a single nop insn.  */
  VLIW_DOUBLE_THEN_SINGLE_NOP
};

static void frv_debug_tomcat PARAMS ((struct vliw_chain *));

static void
frv_debug_tomcat (start_chain)
   struct vliw_chain *start_chain;
{
   struct vliw_chain *this_chain;
   struct vliw_insn_list *this_insn;
   int i = 1;

  for (this_chain = start_chain; this_chain; this_chain = this_chain->next, i++)
    {
      fprintf (stderr, "\nVliw Insn #%d, #insns: %d\n", i, this_chain->insn_count);

      for (this_insn = this_chain->insn_list; this_insn; this_insn = this_insn->next)
	{
	  if (this_insn->type == VLIW_LABEL_TYPE)
	    fprintf (stderr, "Label Value: %d\n", (int) this_insn->sym);
	  else if (this_insn->type == VLIW_BRANCH_TYPE)
	    fprintf (stderr, "%s to %d\n", this_insn->insn->base->name, (int) this_insn->sym);
	  else if (this_insn->type == VLIW_BRANCH_HAS_NOPS)
	    fprintf (stderr, "nop'd %s to %d\n", this_insn->insn->base->name, (int) this_insn->sym);
	  else if (this_insn->type == VLIW_NOP_TYPE)
	    fprintf (stderr, "Nop\n");
	  else
	    fprintf (stderr, "	%s\n", this_insn->insn->base->name);
	}
   }
}

static void frv_adjust_vliw_count PARAMS ((struct vliw_chain *));

static void
frv_adjust_vliw_count (this_chain)
    struct vliw_chain *this_chain;
{
  struct vliw_insn_list *this_insn;

  this_chain->insn_count = 0;

  for (this_insn = this_chain->insn_list;
       this_insn;
       this_insn = this_insn->next)
    {
      if (this_insn->type != VLIW_LABEL_TYPE)
  	this_chain->insn_count++;
    }

}

/* Insert the desired nop combination in the vliw chain before insert_before_insn.
   Rechain the vliw insn.  */

static struct vliw_chain *frv_tomcat_shuffle
  PARAMS ((enum vliw_nop_type, struct vliw_chain *, struct vliw_insn_list *));

static struct vliw_chain *
frv_tomcat_shuffle (this_nop_type, vliw_to_split, insert_before_insn)
   enum vliw_nop_type    this_nop_type;
   struct vliw_chain     *vliw_to_split;
   struct vliw_insn_list *insert_before_insn;
{

  bfd_boolean pack_prev = FALSE;
  struct vliw_chain *return_me = NULL;
  struct vliw_insn_list *prev_insn = NULL;
  struct vliw_insn_list *curr_insn = vliw_to_split->insn_list;

  struct vliw_chain *double_nop = (struct vliw_chain *) xmalloc (sizeof (struct vliw_chain));
  struct vliw_chain *single_nop = (struct vliw_chain *) xmalloc (sizeof (struct vliw_chain));
  struct vliw_chain *second_part = (struct vliw_chain *) xmalloc (sizeof (struct vliw_chain));
  struct vliw_chain *curr_vliw = vliw_chain_top;
  struct vliw_chain *prev_vliw = NULL;

  while (curr_insn && curr_insn != insert_before_insn)
    {
      /* We can't set the packing bit on a label.  If we have the case
	 label 1:
	 label 2:
	 label 3:
	   branch that needs nops
	Then don't set pack bit later.  */

      if (curr_insn->type != VLIW_LABEL_TYPE)
	pack_prev = TRUE;
      prev_insn = curr_insn;
      curr_insn = curr_insn->next;
    } 

  while (curr_vliw && curr_vliw != vliw_to_split)
    {
      prev_vliw = curr_vliw;
      curr_vliw = curr_vliw->next;
    }

  switch (this_nop_type)
    {
    case VLIW_SINGLE_NOP:
      if (!prev_insn)
	{
	/* Branch is first,  Insert the NOP prior to this vliw insn.  */
	if (prev_vliw)
	  prev_vliw->next = single_nop;
	else
	  vliw_chain_top = single_nop;
	single_nop->next = vliw_to_split;
	vliw_to_split->insn_list->type = VLIW_BRANCH_HAS_NOPS;
	return_me = vliw_to_split;
	}
      else
	{
	  /* Set the packing bit on the previous insn.  */
	  if (pack_prev)
	    {
	      unsigned char *buffer = prev_insn->address;
	      buffer[0] |= 0x80;
	    }
	  /* The branch is in the middle.  Split this vliw insn into first
	     and second parts.  Insert the NOP inbetween.  */

          second_part->insn_list = insert_before_insn;
	  second_part->insn_list->type = VLIW_BRANCH_HAS_NOPS;
          second_part->next      = vliw_to_split->next;
 	  frv_adjust_vliw_count (second_part);

          single_nop->next       = second_part;
 
          vliw_to_split->next    = single_nop;
          prev_insn->next        = NULL;
 
          return_me = second_part;
	  frv_adjust_vliw_count (vliw_to_split);
	}
      break;

    case VLIW_DOUBLE_NOP:
      if (!prev_insn)
	{
	/* Branch is first,  Insert the NOP prior to this vliw insn.  */
        if (prev_vliw)
          prev_vliw->next = double_nop;
        else
          vliw_chain_top = double_nop;

	double_nop->next = vliw_to_split;
	return_me = vliw_to_split;
	vliw_to_split->insn_list->type = VLIW_BRANCH_HAS_NOPS;
	}
      else
	{
	  /* Set the packing bit on the previous insn.  */
	  if (pack_prev)
	    {
	      unsigned char *buffer = prev_insn->address;
	      buffer[0] |= 0x80;
	    }

	/* The branch is in the middle.  Split this vliw insn into first
	   and second parts.  Insert the NOP inbetween.  */
          second_part->insn_list = insert_before_insn;
	  second_part->insn_list->type = VLIW_BRANCH_HAS_NOPS;
          second_part->next      = vliw_to_split->next;
 	  frv_adjust_vliw_count (second_part);
 
          double_nop->next       = second_part;
 
          vliw_to_split->next    = single_nop;
          prev_insn->next        = NULL;
 	  frv_adjust_vliw_count (vliw_to_split);
 
          return_me = second_part;
	}
      break;

    case VLIW_DOUBLE_THEN_SINGLE_NOP:
      double_nop->next = single_nop;
      double_nop->insn_count = 2;
      double_nop->insn_list = &double_nop_insn;
      single_nop->insn_count = 1;
      single_nop->insn_list = &single_nop_insn;

      if (!prev_insn)
	{
	  /* The branch is the first insn in this vliw.  Don't split the vliw.  Insert
	     the nops prior to this vliw.  */
          if (prev_vliw)
            prev_vliw->next = double_nop;
          else
            vliw_chain_top = double_nop;
 
	  single_nop->next = vliw_to_split;
	  return_me = vliw_to_split;
	  vliw_to_split->insn_list->type = VLIW_BRANCH_HAS_NOPS;
	}
      else
	{
	  /* Set the packing bit on the previous insn.  */
	  if (pack_prev)
	    {
	      unsigned char *buffer = prev_insn->address;
	      buffer[0] |= 0x80;
	    }

	  /* The branch is in the middle of this vliw insn.  Split into first and
	     second parts.  Insert the nop vliws in between.  */  
	  second_part->insn_list = insert_before_insn;
	  second_part->insn_list->type = VLIW_BRANCH_HAS_NOPS;
	  second_part->next      = vliw_to_split->next;
 	  frv_adjust_vliw_count (second_part);

	  single_nop->next       = second_part;

	  vliw_to_split->next	 = double_nop;
	  prev_insn->next	 = NULL;
 	  frv_adjust_vliw_count (vliw_to_split);

	  return_me = second_part;
	}
      break;
    }

  return return_me;
}

static void frv_tomcat_analyze_vliw_chains PARAMS ((void));

static void
frv_tomcat_analyze_vliw_chains ()
{
  struct vliw_chain *vliw1 = NULL;
  struct vliw_chain *vliw2 = NULL;
  struct vliw_chain *vliw3 = NULL;

  struct vliw_insn_list *this_insn = NULL;
  struct vliw_insn_list *temp_insn = NULL;

  /* We potentially need to look at three VLIW insns to determine if the
     workaround is required.  Set them up.  Ignore existing nops during analysis. */

#define FRV_SET_VLIW_WINDOW(VLIW1, VLIW2, VLIW3) \
  if (VLIW1 && VLIW1->next)			 \
    VLIW2 = VLIW1->next;			 \
  else						 \
    VLIW2 = NULL;				 \
  if (VLIW2 && VLIW2->next)			 \
    VLIW3 = VLIW2->next;			 \
  else						 \
    VLIW3 = NULL

  vliw1 = vliw_chain_top;

workaround_top:

  FRV_SET_VLIW_WINDOW (vliw1, vliw2, vliw3);

  if (!vliw1)
    return;

  if (vliw1->insn_count == 1)
    {
      /* check vliw1 for a label. */
      if (vliw1->insn_list->type == VLIW_LABEL_TYPE)
	{
	  temp_insn = frv_find_in_vliw (VLIW_BRANCH_TYPE, vliw2, vliw1->insn_list->sym);
	  if (temp_insn)
	    {
	      vliw1 = frv_tomcat_shuffle (VLIW_DOUBLE_NOP, vliw2, vliw1->insn_list);
	      temp_insn->dnop_frag->fr_subtype = NOP_KEEP;
	      vliw1 = vliw1->next;
	      if (tomcat_stats)
		tomcat_doubles++;
	      goto workaround_top;
	    }
	  else if (vliw2 
		   && vliw2->insn_count == 1
		   && (temp_insn = frv_find_in_vliw (VLIW_BRANCH_TYPE, vliw3, vliw1->insn_list->sym)) != NULL)
	    {
	      temp_insn->snop_frag->fr_subtype = NOP_KEEP;
	      vliw1 = frv_tomcat_shuffle (VLIW_SINGLE_NOP, vliw3, vliw3->insn_list);
	      if (tomcat_stats)
		tomcat_singles++;
	      goto workaround_top;
	    }
	}
    }

  if (vliw1->insn_count == 2)
    {
      struct vliw_insn_list *this_insn;
 
      /* check vliw1 for a label. */
      for (this_insn = vliw1->insn_list; this_insn; this_insn = this_insn->next)
	{
	  if (this_insn->type == VLIW_LABEL_TYPE)
	    {
	      if ((temp_insn = frv_find_in_vliw (VLIW_BRANCH_TYPE, vliw2, this_insn->sym)) != NULL)
		{
		  temp_insn->snop_frag->fr_subtype = NOP_KEEP;
		  vliw1 = frv_tomcat_shuffle (VLIW_SINGLE_NOP, vliw2, this_insn);
		  if (tomcat_stats)
		    tomcat_singles++;
		}
	      else
		vliw1 = vliw1->next;
              goto workaround_top;
            }
	}
    }
  /* Examine each insn in this VLIW.  Look for the workaround criteria.  */
  for (this_insn = vliw1->insn_list; this_insn; this_insn = this_insn->next)
    {
      /* Don't look at labels or nops.  */
      while (this_insn
	     && (this_insn->type == VLIW_LABEL_TYPE
                 || this_insn->type == VLIW_NOP_TYPE
		 || this_insn->type == VLIW_BRANCH_HAS_NOPS))
	this_insn = this_insn->next;

      if (!this_insn)
        {
	  vliw1 = vliw2;
	  goto workaround_top;
	}

      if (frv_is_branch_insn (this_insn->insn))
	{
	  if ((temp_insn = frv_find_in_vliw (VLIW_LABEL_TYPE, vliw1, this_insn->sym)) != NULL)
	    {
	      /* Insert [nop/nop] [nop] before branch.  */
	      this_insn->snop_frag->fr_subtype = NOP_KEEP;
	      this_insn->dnop_frag->fr_subtype = NOP_KEEP;
	      vliw1 = frv_tomcat_shuffle (VLIW_DOUBLE_THEN_SINGLE_NOP, vliw1, this_insn);
	      goto workaround_top;
	    }
	}


    }
  /* This vliw insn checks out okay.  Take a look at the next one.  */
  vliw1 = vliw1->next;
  goto workaround_top;
}

void
frv_tomcat_workaround ()
{
  if (frv_mach != bfd_mach_frvtomcat)
    return;

  if (tomcat_debug)
    frv_debug_tomcat (vliw_chain_top);

  frv_tomcat_analyze_vliw_chains ();

  if (tomcat_stats)
    {
      fprintf (stderr, "Inserted %d Single Nops\n", tomcat_singles);
      fprintf (stderr, "Inserted %d Double Nops\n", tomcat_doubles);
    }
}

static int
fr550_check_insn_acc_range (frv_insn *insn, int low, int hi)
{
  int acc;
  switch (CGEN_INSN_NUM (insn->insn))
    {
    case FRV_INSN_MADDACCS:
    case FRV_INSN_MSUBACCS:
    case FRV_INSN_MDADDACCS:
    case FRV_INSN_MDSUBACCS:
    case FRV_INSN_MASACCS:
    case FRV_INSN_MDASACCS:
      acc = insn->fields.f_ACC40Si;
      if (acc < low || acc > hi)
	return 1; /* out of range */
      acc = insn->fields.f_ACC40Sk;
      if (acc < low || acc > hi)
	return 1; /* out of range */
      break;
    case FRV_INSN_MMULHS:
    case FRV_INSN_MMULHU:
    case FRV_INSN_MMULXHS:
    case FRV_INSN_MMULXHU:
    case FRV_INSN_CMMULHS:
    case FRV_INSN_CMMULHU:
    case FRV_INSN_MQMULHS:
    case FRV_INSN_MQMULHU:
    case FRV_INSN_MQMULXHS:
    case FRV_INSN_MQMULXHU:
    case FRV_INSN_CMQMULHS:
    case FRV_INSN_CMQMULHU:
    case FRV_INSN_MMACHS:
    case FRV_INSN_MMRDHS:
    case FRV_INSN_CMMACHS: 
    case FRV_INSN_MQMACHS:
    case FRV_INSN_CMQMACHS:
    case FRV_INSN_MQXMACHS:
    case FRV_INSN_MQXMACXHS:
    case FRV_INSN_MQMACXHS:
    case FRV_INSN_MCPXRS:
    case FRV_INSN_MCPXIS:
    case FRV_INSN_CMCPXRS:
    case FRV_INSN_CMCPXIS:
    case FRV_INSN_MQCPXRS:
    case FRV_INSN_MQCPXIS:
     acc = insn->fields.f_ACC40Sk;
      if (acc < low || acc > hi)
	return 1; /* out of range */
      break;
    case FRV_INSN_MMACHU:
    case FRV_INSN_MMRDHU:
    case FRV_INSN_CMMACHU:
    case FRV_INSN_MQMACHU:
    case FRV_INSN_CMQMACHU:
    case FRV_INSN_MCPXRU:
    case FRV_INSN_MCPXIU:
    case FRV_INSN_CMCPXRU:
    case FRV_INSN_CMCPXIU:
    case FRV_INSN_MQCPXRU:
    case FRV_INSN_MQCPXIU:
      acc = insn->fields.f_ACC40Uk;
      if (acc < low || acc > hi)
	return 1; /* out of range */
      break;
    default:
      break;
    }
  return 0; /* all is ok */
}

static int
fr550_check_acc_range (FRV_VLIW *vliw, frv_insn *insn)
{
  switch ((*vliw->current_vliw)[vliw->next_slot - 1])
    {
    case UNIT_FM0:
    case UNIT_FM2:
      return fr550_check_insn_acc_range (insn, 0, 3);
    case UNIT_FM1:
    case UNIT_FM3:
      return fr550_check_insn_acc_range (insn, 4, 7);
    default:
      break;
    }
  return 0; /* all is ok */
}

void
md_assemble (str)
     char * str;
{
  frv_insn insn;
  char *errmsg;
  int packing_constraint;
  finished_insnS  finished_insn;
  fragS *double_nop_frag = NULL;
  fragS *single_nop_frag = NULL;
  struct vliw_insn_list *vliw_insn_list_entry = NULL;

  /* Initialize GAS's cgen interface for a new instruction.  */
  gas_cgen_init_parse ();

  memset (&insn, 0, sizeof (insn));

  insn.insn = frv_cgen_assemble_insn
    (gas_cgen_cpu_desc, str, & insn.fields, insn.buffer, &errmsg);
  
  if (!insn.insn)
    {
      as_bad (errmsg);
      return;
    }
  
  /* If the cpu is tomcat, then we need to insert nops to workaround
     hardware limitations.  We need to keep track of each vliw unit
     and examine the length of the unit and the individual insns
     within the unit to determine the number and location of the
     required nops.  */
  if (frv_mach == bfd_mach_frvtomcat)
    {
      /* If we've just finished a VLIW insn OR this is a branch,
	 then start up a new frag.  Fill it with nops.  We will get rid
	 of those that are not required after we've seen all of the 
	 instructions but before we start resolving fixups.  */
      if ( !FRV_IS_NOP (insn)
	  && (frv_is_branch_insn (insn.insn) || insn.fields.f_pack))
	{
	  char *buffer;

	  frag_wane (frag_now);
	  frag_new (0);
	  double_nop_frag = frag_now;
	  buffer = frag_var (rs_machine_dependent, 8, 8, NOP_DELETE, NULL, 0, 0);
	  md_number_to_chars (buffer, FRV_NOP_PACK, 4);
	  md_number_to_chars (buffer+4, FRV_NOP_NOPACK, 4);

	  frag_wane (frag_now);
	  frag_new (0);
	  single_nop_frag = frag_now;
	  buffer = frag_var (rs_machine_dependent, 4, 4, NOP_DELETE, NULL, 0, 0);
	  md_number_to_chars (buffer, FRV_NOP_NOPACK, 4);
	}

      vliw_insn_list_entry = frv_insert_vliw_insn (DO_COUNT);
      vliw_insn_list_entry->insn   = insn.insn;
      if (frv_is_branch_insn (insn.insn))
	vliw_insn_list_entry->type = VLIW_BRANCH_TYPE;

      if ( !FRV_IS_NOP (insn)
	  && (frv_is_branch_insn (insn.insn) || insn.fields.f_pack))
	{
	  vliw_insn_list_entry->snop_frag = single_nop_frag;
	  vliw_insn_list_entry->dnop_frag = double_nop_frag;
	}
    }

  /* Make sure that this insn does not violate the VLIW packing constraints.  */
  /* -mno-pack disallows any packing whatsoever.  */
  if (frv_flags & EF_FRV_NOPACK)
    {
      if (! insn.fields.f_pack)
	{
	  as_bad (_("VLIW packing used for -mno-pack"));
	  return;
	}
    }
  /* -mcpu=FRV is an idealized FR-V implementation that supports all of the
     instructions, don't do vliw checking.  */
  else if (frv_mach != bfd_mach_frv)
    {
      packing_constraint = frv_vliw_add_insn (& vliw, insn.insn);
      if (frv_mach == bfd_mach_fr550 && ! packing_constraint)
	packing_constraint = fr550_check_acc_range (& vliw, & insn);
      if (insn.fields.f_pack)
	frv_vliw_reset (& vliw, frv_mach, frv_flags);
      if (packing_constraint)
	{
	  as_bad (_("VLIW packing constraint violation"));
	  return;
	}
    }

  /* Doesn't really matter what we pass for RELAX_P here.  */
  gas_cgen_finish_insn (insn.insn, insn.buffer,
			CGEN_FIELDS_BITSIZE (& insn.fields), 1, &finished_insn);


  /* If the cpu is tomcat, then we need to insert nops to workaround
     hardware limitations.  We need to keep track of each vliw unit
     and examine the length of the unit and the individual insns
     within the unit to determine the number and location of the
     required nops.  */
  if (frv_mach == bfd_mach_frvtomcat)
    {
      if (vliw_insn_list_entry)
        vliw_insn_list_entry->address = finished_insn.addr;
      else
	abort();

      if (insn.fields.f_pack)
	{
	  /* We've completed a VLIW insn.  */
	  previous_vliw_chain = current_vliw_chain;
	  current_vliw_chain = NULL;
	  current_vliw_insn  = NULL;
        } 
    }
}

/* The syntax in the manual says constants begin with '#'.
   We just ignore it.  */

void 
md_operand (expressionP)
     expressionS * expressionP;
{
  if (* input_line_pointer == '#')
    {
      input_line_pointer ++;
      expression (expressionP);
    }
}

valueT
md_section_align (segment, size)
     segT   segment;
     valueT size;
{
  int align = bfd_get_section_alignment (stdoutput, segment);
  return ((size + (1 << align) - 1) & (-1 << align));
}

symbolS *
md_undefined_symbol (name)
  char * name ATTRIBUTE_UNUSED;
{
  return 0;
}

/* Interface to relax_segment.  */

/* FIXME: Build table by hand, get it working, then machine generate.  */
const relax_typeS md_relax_table[] =
{
  {1, 1, 0, 0},
  {511 - 2 - 2, -512 - 2 + 2, 0, 2 },
  {0x2000000 - 1 - 2, -0x2000000 - 2, 2, 0 },
  {0x2000000 - 1 - 2, -0x2000000 - 2, 4, 0 }
};

long
frv_relax_frag (fragP, stretch)
     fragS   *fragP ATTRIBUTE_UNUSED;
     long    stretch ATTRIBUTE_UNUSED;
{
  return 0;
}

/* Return an initial guess of the length by which a fragment must grow to
   hold a branch to reach its destination.
   Also updates fr_type/fr_subtype as necessary.

   Called just before doing relaxation.
   Any symbol that is now undefined will not become defined.
   The guess for fr_var is ACTUALLY the growth beyond fr_fix.
   Whatever we do to grow fr_fix or fr_var contributes to our returned value.
   Although it may not be explicit in the frag, pretend fr_var starts with a
   0 value.  */

int
md_estimate_size_before_relax (fragP, segment)
     fragS * fragP;
     segT    segment ATTRIBUTE_UNUSED;
{
  switch (fragP->fr_subtype)
    {
    case NOP_KEEP:
      return fragP->fr_var;

    default:
    case NOP_DELETE:
      return 0;
    }     
} 

/* *fragP has been relaxed to its final size, and now needs to have
   the bytes inside it modified to conform to the new size.

   Called after relaxation is finished.
   fragP->fr_type == rs_machine_dependent.
   fragP->fr_subtype is the subtype of what the address relaxed to.  */

void
md_convert_frag (abfd, sec, fragP)
  bfd *   abfd ATTRIBUTE_UNUSED;
  segT    sec ATTRIBUTE_UNUSED;
  fragS * fragP;
{
  switch (fragP->fr_subtype)
    {
    default:
    case NOP_DELETE:
      return;

    case NOP_KEEP:
      fragP->fr_fix = fragP->fr_var;
      fragP->fr_var = 0;
      return;   
    }
}

/* Functions concerning relocs.  */

/* The location from which a PC relative jump should be calculated,
   given a PC relative reloc.  */

long
md_pcrel_from_section (fixP, sec)
     fixS * fixP;
     segT   sec;
{
  if (TC_FORCE_RELOCATION (fixP)
      || (fixP->fx_addsy != (symbolS *) NULL
	  && S_GET_SEGMENT (fixP->fx_addsy) != sec))
    {
      /* If we can't adjust this relocation, or if it references a
	 local symbol in a different section (which
	 TC_FORCE_RELOCATION can't check), let the linker figure it
	 out.  */
      return 0;
    }

  return (fixP->fx_frag->fr_address + fixP->fx_where) & ~1;
}

/* Return the bfd reloc type for OPERAND of INSN at fixup FIXP.
   Returns BFD_RELOC_NONE if no reloc type can be found.
   *FIXP may be modified if desired.  */

bfd_reloc_code_real_type
md_cgen_lookup_reloc (insn, operand, fixP)
     const CGEN_INSN *    insn ATTRIBUTE_UNUSED;
     const CGEN_OPERAND * operand;
     fixS *               fixP;
{
  switch (operand->type)
    {
    case FRV_OPERAND_LABEL16:
      fixP->fx_pcrel = TRUE;
      return BFD_RELOC_FRV_LABEL16;

    case FRV_OPERAND_LABEL24:
      fixP->fx_pcrel = TRUE;
      return BFD_RELOC_FRV_LABEL24;

    case FRV_OPERAND_UHI16:
    case FRV_OPERAND_ULO16:
    case FRV_OPERAND_SLO16:

      /* The relocation type should be recorded in opinfo */
      if (fixP->fx_cgen.opinfo != 0)
        return fixP->fx_cgen.opinfo;
      break;

    case FRV_OPERAND_D12:
    case FRV_OPERAND_S12:
      if (fixP->fx_cgen.opinfo != 0)
	return fixP->fx_cgen.opinfo;

      return BFD_RELOC_FRV_GPREL12;

    case FRV_OPERAND_U12:
      return BFD_RELOC_FRV_GPRELU12;

    default: 
      break;
    }
  return BFD_RELOC_NONE;
}


/* See whether we need to force a relocation into the output file.
   This is used to force out switch and PC relative relocations when
   relaxing.  */

int
frv_force_relocation (fix)
     fixS * fix;
{
  if (fix->fx_r_type == BFD_RELOC_FRV_GPREL12
      || fix->fx_r_type == BFD_RELOC_FRV_GPRELU12)
    return 1;

  return generic_force_reloc (fix);
}

/* Apply a fixup that could be resolved within the assembler.  */

void
md_apply_fix3 (fixP, valP, seg)
     fixS *   fixP;
     valueT * valP;
     segT     seg;
{
  if (fixP->fx_addsy == 0)
    switch (fixP->fx_cgen.opinfo)
      {
      case BFD_RELOC_FRV_HI16:
	*valP >>= 16;
	/* Fall through.  */
      case BFD_RELOC_FRV_LO16:
	*valP &= 0xffff;
	break;
      }

  gas_cgen_md_apply_fix3 (fixP, valP, seg);
  return;
}


/* Write a value out to the object file, using the appropriate endianness.  */

void
frv_md_number_to_chars (buf, val, n)
     char * buf;
     valueT val;
     int    n;
{
  number_to_chars_bigendian (buf, val, n);
}

/* Turn a string in input_line_pointer into a floating point constant of type
   type, and store the appropriate bytes in *litP.  The number of LITTLENUMS
   emitted is stored in *sizeP .  An error message is returned, or NULL on OK.
*/

/* Equal to MAX_PRECISION in atof-ieee.c */
#define MAX_LITTLENUMS 6

char *
md_atof (type, litP, sizeP)
     char   type;
     char * litP;
     int *  sizeP;
{
  int              i;
  int              prec;
  LITTLENUM_TYPE   words [MAX_LITTLENUMS];
  char *           t;

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

   /* FIXME: Some targets allow other format chars for bigger sizes here.  */

    default:
      * sizeP = 0;
      return _("Bad call to md_atof()");
    }

  t = atof_ieee (input_line_pointer, type, words);
  if (t)
    input_line_pointer = t;
  * sizeP = prec * sizeof (LITTLENUM_TYPE);

  for (i = 0; i < prec; i++)
    {
      md_number_to_chars (litP, (valueT) words[i],
			  sizeof (LITTLENUM_TYPE));
      litP += sizeof (LITTLENUM_TYPE);
    }
     
  return 0;
}

bfd_boolean
frv_fix_adjustable (fixP)
   fixS * fixP;
{
  bfd_reloc_code_real_type reloc_type;

  if ((int) fixP->fx_r_type >= (int) BFD_RELOC_UNUSED)
    {
      const CGEN_INSN *insn = NULL;
      int opindex = (int) fixP->fx_r_type - (int) BFD_RELOC_UNUSED;
      const CGEN_OPERAND *operand = cgen_operand_lookup_by_num(gas_cgen_cpu_desc, opindex);
      reloc_type = md_cgen_lookup_reloc (insn, operand, fixP);
    }
  else
    reloc_type = fixP->fx_r_type;

  /* We need the symbol name for the VTABLE entries */
  if (   reloc_type == BFD_RELOC_VTABLE_INHERIT
      || reloc_type == BFD_RELOC_VTABLE_ENTRY
      || reloc_type == BFD_RELOC_FRV_GPREL12
      || reloc_type == BFD_RELOC_FRV_GPRELU12)
    return 0;

  return 1;
}

/* Allow user to set flags bits.  */
void
frv_set_flags (arg)
     int arg ATTRIBUTE_UNUSED;
{
  flagword new_flags = get_absolute_expression ();
  flagword new_mask = ~ (flagword)0;

  frv_user_set_flags_p = 1;
  if (*input_line_pointer == ',')
    {
      ++input_line_pointer;
      new_mask = get_absolute_expression ();
    }

  frv_flags = (frv_flags & ~new_mask) | (new_flags & new_mask);
  bfd_set_private_flags (stdoutput, frv_flags);
}

/* Frv specific function to handle 4 byte initializations for pointers that are
   considered 'safe' for use with pic support.  Until frv_frob_file{,_section}
   is run, we encode it a BFD_RELOC_CTOR, and it is turned back into a normal
   BFD_RELOC_32 at that time.  */

void
frv_pic_ptr (nbytes)
     int nbytes;
{
  expressionS exp;
  char *p;

  if (nbytes != 4)
    abort ();

#ifdef md_flush_pending_output
  md_flush_pending_output ();
#endif

  if (is_it_end_of_statement ())
    {
      demand_empty_rest_of_line ();
      return;
    }

#ifdef md_cons_align
  md_cons_align (nbytes);
#endif

  do
    {
      bfd_reloc_code_real_type reloc_type = BFD_RELOC_CTOR;
      
      if (strncasecmp (input_line_pointer, "funcdesc(", 9) == 0)
	{
	  input_line_pointer += 9;
	  expression (&exp);
	  if (*input_line_pointer == ')')
	    input_line_pointer++;
	  else
	    as_bad ("missing ')'");
	  reloc_type = BFD_RELOC_FRV_FUNCDESC;
	}
      else
	expression (&exp);

      p = frag_more (4);
      memset (p, 0, 4);
      fix_new_exp (frag_now, p - frag_now->fr_literal, 4, &exp, 0,
		   reloc_type);
    }
  while (*input_line_pointer++ == ',');

  input_line_pointer--;			/* Put terminator back into stream. */
  demand_empty_rest_of_line ();
}



#ifdef DEBUG
#define DPRINTF1(A)	fprintf (stderr, A)
#define DPRINTF2(A,B)	fprintf (stderr, A, B)
#define DPRINTF3(A,B,C)	fprintf (stderr, A, B, C)

#else
#define DPRINTF1(A)
#define DPRINTF2(A,B)
#define DPRINTF3(A,B,C)
#endif

/* Go through a the sections looking for relocations that are problematical for
   pic.  If not pic, just note that this object can't be linked with pic.  If
   it is pic, see if it needs to be marked so that it will be fixed up, or if
   not possible, issue an error.  */

static void
frv_frob_file_section (abfd, sec, ptr)
     bfd *abfd;
     asection *sec;
     PTR ptr ATTRIBUTE_UNUSED;
{
  segment_info_type *seginfo = seg_info (sec);
  fixS *fixp;
  CGEN_CPU_DESC cd = gas_cgen_cpu_desc;
  flagword flags = bfd_get_section_flags (abfd, sec);

  /* Skip relocations in known sections (.ctors, .dtors, and .gcc_except_table)
     since we can fix those up by hand.  */
  int known_section_p = (sec->name
			 && sec->name[0] == '.'
			 && ((sec->name[1] == 'c'
			      && strcmp (sec->name, ".ctor") == 0)
			     || (sec->name[1] == 'd'
				 && strcmp (sec->name, ".dtor") == 0)
			     || (sec->name[1] == 'g'
				 && strcmp (sec->name, ".gcc_except_table") == 0)));

  DPRINTF3 ("\nFrv section %s%s\n", sec->name, (known_section_p) ? ", known section" : "");
  if ((flags & SEC_ALLOC) == 0)
    {
      DPRINTF1 ("\tSkipping non-loaded section\n");
      return;
    }

  for (fixp = seginfo->fix_root; fixp; fixp = fixp->fx_next)
    {
      symbolS *s = fixp->fx_addsy;
      bfd_reloc_code_real_type reloc;
      int non_pic_p;
      int opindex;
      const CGEN_OPERAND *operand;
      const CGEN_INSN *insn = fixp->fx_cgen.insn;

      if (fixp->fx_done)
	{
	  DPRINTF1 ("\tSkipping reloc that has already been done\n");
	  continue;
	}

      if (fixp->fx_pcrel)
	{
	  DPRINTF1 ("\tSkipping reloc that is PC relative\n");
	  continue;
	}

      if (! s)
	{
	  DPRINTF1 ("\tSkipping reloc without symbol\n");
	  continue;
	}

      if (fixp->fx_r_type < BFD_RELOC_UNUSED)
	{
	  opindex = -1;
	  reloc = fixp->fx_r_type;
	}
      else
	{
	  opindex = (int) fixp->fx_r_type - (int) BFD_RELOC_UNUSED;
	  operand = cgen_operand_lookup_by_num (cd, opindex);
	  reloc = md_cgen_lookup_reloc (insn, operand, fixp);
	}

      DPRINTF3 ("\treloc %s\t%s", bfd_get_reloc_code_name (reloc), S_GET_NAME (s));

      non_pic_p = 0;
      switch (reloc)
	{
	default:
	  break;

	case BFD_RELOC_32:
	  /* Skip relocations in known sections (.ctors, .dtors, and
	     .gcc_except_table) since we can fix those up by hand.  Also
	     skip forward references to constants.  Also skip a difference
	     of two symbols, which still uses the BFD_RELOC_32 at this
	     point.  */
	  if (! known_section_p
	      && S_GET_SEGMENT (s) != absolute_section
	      && !fixp->fx_subsy
	      && (flags & (SEC_READONLY | SEC_CODE)) == 0)
	    {
	      non_pic_p = 1;
	    }
	  break;

	  /* FIXME -- should determine if any of the GP relocation really uses
	     gr16 (which is not pic safe) or not.  Right now, assume if we
	     aren't being compiled with -mpic, the usage is non pic safe, but
	     is safe with -mpic.  */
	case BFD_RELOC_FRV_GPREL12:
	case BFD_RELOC_FRV_GPRELU12:
	case BFD_RELOC_FRV_GPREL32:
	case BFD_RELOC_FRV_GPRELHI:
	case BFD_RELOC_FRV_GPRELLO:
	  non_pic_p = ! frv_pic_p;
	  break;

	case BFD_RELOC_FRV_LO16:
	case BFD_RELOC_FRV_HI16:
	  if (S_GET_SEGMENT (s) != absolute_section)
	    non_pic_p = 1;
	  break;

	case BFD_RELOC_VTABLE_INHERIT:
	case BFD_RELOC_VTABLE_ENTRY:
	  non_pic_p = 1;
	  break;

	  /* If this is a blessed BFD_RELOC_32, convert it back to the normal
             relocation.  */
	case BFD_RELOC_CTOR:
	  fixp->fx_r_type = BFD_RELOC_32;
	  break;
	}

      if (non_pic_p)
	{
	  DPRINTF1 (" (Non-pic relocation)\n");
	  if (frv_pic_p)
	    as_warn_where (fixp->fx_file, fixp->fx_line,
			   _("Relocation %s is not safe for %s"),
			   bfd_get_reloc_code_name (reloc), frv_pic_flag);

	  else if ((frv_flags & EF_FRV_NON_PIC_RELOCS) == 0)
	    {
	      frv_flags |= EF_FRV_NON_PIC_RELOCS;
	      bfd_set_private_flags (abfd, frv_flags);
	    }
	}
#ifdef DEBUG
      else
	DPRINTF1 ("\n");
#endif
    }
}

/* After all of the symbols have been adjusted, go over the file looking
   for any relocations that pic won't support.  */

void
frv_frob_file ()
{
  bfd_map_over_sections (stdoutput, frv_frob_file_section, (PTR)0);
}

void
frv_frob_label (this_label)
    symbolS *this_label;
{
  struct vliw_insn_list *vliw_insn_list_entry;

  if (frv_mach != bfd_mach_frvtomcat)
    return;

  if (now_seg != text_section)
    return;

  vliw_insn_list_entry = frv_insert_vliw_insn(DONT_COUNT);
  vliw_insn_list_entry->type = VLIW_LABEL_TYPE;
  vliw_insn_list_entry->sym  = this_label; 
}

fixS *
frv_cgen_record_fixup_exp (frag, where, insn, length, operand, opinfo, exp)
     fragS *              frag;
     int                  where;
     const CGEN_INSN *    insn;
     int                  length;
     const CGEN_OPERAND * operand;
     int                  opinfo;
     expressionS *        exp;
{
  fixS * fixP = gas_cgen_record_fixup_exp (frag, where, insn, length,
                                           operand, opinfo, exp);

  if (frv_mach == bfd_mach_frvtomcat
      && current_vliw_insn
      && current_vliw_insn->type == VLIW_BRANCH_TYPE
      && exp != NULL)
    current_vliw_insn->sym = exp->X_add_symbol;
    
  return fixP;
}
