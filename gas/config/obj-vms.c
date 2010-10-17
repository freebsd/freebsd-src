/* vms.c -- Write out a VAX/VMS object file
   Copyright 1987, 1988, 1992, 1993, 1994, 1995, 1997, 1998, 2000, 2001,
   2002, 2003
   Free Software Foundation, Inc.

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

/* Written by David L. Kashtan */
/* Modified by Eric Youngdale to write VMS debug records for program
   variables */

/* Want all of obj-vms.h (as obj-format.h, via targ-env.h, via as.h).  */
#define WANT_VMS_OBJ_DEFS

#include "as.h"
#include "config.h"
#include "safe-ctype.h"
#include "subsegs.h"
#include "obstack.h"
#include <fcntl.h>

/* What we do if there is a goof.  */
#define error as_fatal

#ifdef VMS			/* These are of no use if we are cross assembling.  */
#include <fab.h>		/* Define File Access Block.  */
#include <nam.h>		/* Define NAM Block.  */
#include <xab.h>		/* Define XAB - all different types.  */
extern int sys$open(), sys$close(), sys$asctim();
#endif

/* Version string of the compiler that produced the code we are
   assembling.  (And this assembler, if we do not have compiler info).  */
char *compiler_version_string;

extern int flag_hash_long_names;	/* -+ */
extern int flag_one;			/* -1; compatibility with gcc 1.x */
extern int flag_show_after_trunc;	/* -H */
extern int flag_no_hash_mixed_case;	/* -h NUM */

/* Flag that determines how we map names.  This takes several values, and
   is set with the -h switch.  A value of zero implies names should be
   upper case, and the presence of the -h switch inhibits the case hack.
   No -h switch at all sets vms_name_mapping to 0, and allows case hacking.
   A value of 2 (set with -h2) implies names should be
   all lower case, with no case hack.  A value of 3 (set with -h3) implies
   that case should be preserved.  */

/* If the -+ switch is given, then the hash is appended to any name that is
   longer than 31 characters, regardless of the setting of the -h switch.  */

char vms_name_mapping = 0;

static symbolS *Entry_Point_Symbol = 0;	/* Pointer to "_main" */

/* We augment the "gas" symbol structure with this.  */

struct VMS_Symbol
{
  struct VMS_Symbol *Next;
  symbolS *Symbol;
  int Size;
  int Psect_Index;
  int Psect_Offset;
};

struct VMS_Symbol *VMS_Symbols = 0;
struct VMS_Symbol *Ctors_Symbols = 0;
struct VMS_Symbol *Dtors_Symbols = 0;

/* We need this to keep track of the various input files, so that we can
   give the debugger the correct source line.  */

struct input_file
{
  struct input_file *next;
  struct input_file *same_file_fpnt;
  int file_number;
  int max_line;
  int min_line;
  int offset;
  char flag;
  char *name;
  symbolS *spnt;
};

static struct input_file *file_root = (struct input_file *) NULL;

/* Styles of PSECTS (program sections) that we generate; just shorthand
   to avoid lists of section attributes.  Used by VMS_Psect_Spec().  */
enum ps_type
{
  ps_TEXT, ps_DATA, ps_COMMON, ps_CONST, ps_CTORS, ps_DTORS
};

/* This enum is used to keep track of the various types of variables that
   may be present.  */

enum advanced_type
{
  BASIC, POINTER, ARRAY, ENUM, STRUCT, UNION, FUNCTION, VOID, ALIAS, UNKNOWN
};

/* This structure contains the information from the stabs directives, and the
   information is filled in by VMS_typedef_parse.  Everything that is needed
   to generate the debugging record for a given symbol is present here.
   This could be done more efficiently, using nested struct/unions, but for
   now I am happy that it works.  */

struct VMS_DBG_Symbol
{
  struct VMS_DBG_Symbol *next;
  /* Description of what this is.  */
  enum advanced_type advanced;
  /* This record is for this type.  */
  int dbx_type;
  /* For advanced types this is the type referred to.  I.e., the type
     a pointer points to, or the type of object that makes up an
     array.  */
  int type2;
  /* Use this type when generating a variable def.  */
  int VMS_type;
  /* Used for arrays - this will be present for all.  */
  int index_min;
  /* Entries, but will be meaningless for non-arrays.  */
  int index_max;
  /* Size in bytes of the data type.  For an array, this is the size
     of one element in the array.  */
  int data_size;
  /* Number of the structure/union/enum - used for ref.  */
  int struc_numb;
};

#define SYMTYPLST_SIZE (1<<4)	/* 16; Must be power of two.  */
#define SYMTYP_HASH(x) ((unsigned) (x) & (SYMTYPLST_SIZE - 1))

struct VMS_DBG_Symbol *VMS_Symbol_type_list[SYMTYPLST_SIZE];

/* We need this structure to keep track of forward references to
   struct/union/enum that have not been defined yet.  When they are
   ultimately defined, then we can go back and generate the TIR
   commands to make a back reference.  */

struct forward_ref
{
  struct forward_ref *next;
  int dbx_type;
  int struc_numb;
  char resolved;
};

struct forward_ref *f_ref_root = (struct forward_ref *) NULL;

/* This routine is used to compare the names of certain types to various
   fixed types that are known by the debugger.  */

#define type_check(X)  !strcmp (symbol_name, X)

/* This variable is used to keep track of the name of the symbol we are
   working on while we are parsing the stabs directives.  */

static const char *symbol_name;

/* We use this counter to assign numbers to all of the structures, unions
   and enums that we define.  When we actually declare a variable to the
   debugger, we can simply do it by number, rather than describing the
   whole thing each time.  */

static int structure_count = 0;

/* This variable is used to indicate that we are making the last attempt to
   parse the stabs, and that we should define as much as we can, and ignore
   the rest.  */

static int final_pass;

/* This variable is used to keep track of the current structure number
   for a given variable.  If this is < 0, that means that the structure
   has not yet been defined to the debugger.  This is still cool, since
   the VMS object language has ways of fixing things up after the fact,
   so we just make a note of this, and generate fixups at the end.  */

static int struct_number;

/* This is used to distinguish between D_float and G_float for telling
   the debugger about doubles.  gcc outputs the same .stabs regardless
   of whether -mg is used to select alternate doubles.  */

static int vax_g_doubles = 0;

/* Local symbol references (used to handle N_ABS symbols; gcc does not
   generate those, but they're possible with hand-coded assembler input)
   are always made relative to some particular environment.  If the current
   input has any such symbols, then we expect this to get incremented
   exactly once and end up having all of them be in environment #0.  */

static int Current_Environment = -1;

/* Every object file must specify an module name, which is also used by
   traceback records.  Set in Write_VMS_MHD_Records().  */

static char Module_Name[255+1];

/* Variable descriptors are used tell the debugger the data types of certain
   more complicated variables (basically anything involving a structure,
   union, enum, array or pointer).  Some non-pointer variables of the
   basic types that the debugger knows about do not require a variable
   descriptor.

   Since it is impossible to have a variable descriptor longer than 128
   bytes by virtue of the way that the VMS object language is set up,
   it makes not sense to make the arrays any longer than this, or worrying
   about dynamic sizing of the array.

   These are the arrays and counters that we use to build a variable
   descriptor.  */

#define MAX_DEBUG_RECORD 128
static char Local[MAX_DEBUG_RECORD];	/* Buffer for variable descriptor.  */
static char Asuffix[MAX_DEBUG_RECORD];	/* Buffer for array descriptor.  */
static int Lpnt;		/* Index into Local.  */
static int Apoint;		/* Index into Asuffix.  */
static char overflow;		/* Flag to indicate we have written too much.  */
static int total_len;		/* Used to calculate the total length of
				   variable descriptor plus array descriptor
				   - used for len byte.  */

/* Flag if we have told user about finding global constants in the text
   section.  */
static int gave_compiler_message = 0;

/* Global data (Object records limited to 512 bytes by VAX-11 "C" runtime).  */

static int VMS_Object_File_FD;		/* File Descriptor for object file.  */
static char Object_Record_Buffer[512];	/* Buffer for object file records.  */
static size_t Object_Record_Offset;	/* Offset to end of data.  */
static int Current_Object_Record_Type;	/* Type of record in above.  */

/* Macros for moving data around.  Must work on big-endian systems.  */

#ifdef VMS  /* These are more efficient for VMS->VMS systems.  */
#define COPY_LONG(dest,val)	( *(long *) (dest) = (val) )
#define COPY_SHORT(dest,val)	( *(short *) (dest) = (val) )
#else
#define COPY_LONG(dest,val)	md_number_to_chars ((dest), (val), 4)
#define COPY_SHORT(dest,val)	md_number_to_chars ((dest), (val), 2)
#endif

/* Macros for placing data into the object record buffer.  */

#define PUT_LONG(val) \
	( COPY_LONG (&Object_Record_Buffer[Object_Record_Offset], (val)), \
	  Object_Record_Offset += 4 )

#define PUT_SHORT(val) \
	( COPY_SHORT (&Object_Record_Buffer[Object_Record_Offset], (val)), \
	  Object_Record_Offset += 2 )

#define PUT_CHAR(val) (Object_Record_Buffer[Object_Record_Offset++] = (val))

#define PUT_COUNTED_STRING(cp)			\
  do 						\
    { 						\
      const char *p = (cp);			\
      						\
      PUT_CHAR ((char) strlen (p)); 		\
      while (*p)				\
	PUT_CHAR (*p++);			\
    }						\
  while (0)

/* Macro for determining if a Name has psect attributes attached
   to it.   */

#define PSECT_ATTRIBUTES_STRING		"$$PsectAttributes_"
#define PSECT_ATTRIBUTES_STRING_LENGTH	18

#define HAS_PSECT_ATTRIBUTES(Name) \
		(strncmp ((*Name == '_' ? Name + 1 : Name), \
			  PSECT_ATTRIBUTES_STRING, \
			  PSECT_ATTRIBUTES_STRING_LENGTH) == 0)


 /* in: segT   out: N_TYPE bits */
const short seg_N_TYPE[] =
{
  N_ABS,
  N_TEXT,
  N_DATA,
  N_BSS,
  N_UNDF,			/* unknown */
  N_UNDF,			/* error */
  N_UNDF,			/* expression */
  N_UNDF,			/* debug */
  N_UNDF,			/* ntv */
  N_UNDF,			/* ptv */
  N_REGISTER,			/* register */
};

const segT N_TYPE_seg[N_TYPE + 2] =
{				/* N_TYPE == 0x1E = 32-2 */
  SEG_UNKNOWN,			/* N_UNDF == 0 */
  SEG_GOOF,
  SEG_ABSOLUTE,			/* N_ABS == 2 */
  SEG_GOOF,
  SEG_TEXT,			/* N_TEXT == 4 */
  SEG_GOOF,
  SEG_DATA,			/* N_DATA == 6 */
  SEG_GOOF,
  SEG_BSS,			/* N_BSS == 8 */
  SEG_GOOF,
  SEG_GOOF, SEG_GOOF, SEG_GOOF, SEG_GOOF, SEG_GOOF, SEG_GOOF, SEG_GOOF, SEG_GOOF,
  SEG_GOOF, SEG_GOOF, SEG_GOOF, SEG_GOOF, SEG_GOOF, SEG_GOOF, SEG_GOOF, SEG_GOOF,
  SEG_GOOF, SEG_GOOF, SEG_GOOF, SEG_GOOF,
  SEG_REGISTER,			/* dummy N_REGISTER for regs = 30 */
  SEG_GOOF,
};


/* The following code defines the special types of pseudo-ops that we
   use with VMS.  */

unsigned char const_flag = IN_DEFAULT_SECTION;

static void
s_const (int arg)
{
  /* Since we don't need `arg', use it as our scratch variable so that
     we won't get any "not used" warnings about it.  */
  arg = get_absolute_expression ();
  subseg_set (SEG_DATA, (subsegT) arg);
  const_flag = 1;
  demand_empty_rest_of_line ();
}

const pseudo_typeS obj_pseudo_table[] =
{
  {"const", s_const, 0},
  {0, 0, 0},
};				/* obj_pseudo_table */

/* Routine to perform RESOLVE_SYMBOL_REDEFINITION().  */

int
vms_resolve_symbol_redef (symbolS *sym)
{
  /* If the new symbol is .comm AND it has a size of zero,
     we ignore it (i.e. the old symbol overrides it).  */
  if (SEGMENT_TO_SYMBOL_TYPE ((int) now_seg) == (N_UNDF | N_EXT)
      && frag_now_fix () == 0)
    {
      as_warn (_("compiler emitted zero-size common symbol `%s' already defined"),
	       S_GET_NAME (sym));
      return 1;
    }
  /* If the old symbol is .comm and it has a size of zero,
     we override it with the new symbol value.  */
  if (S_IS_EXTERNAL (sym) && S_IS_DEFINED (sym) && S_GET_VALUE (sym) == 0)
    {
      as_warn (_("compiler redefined zero-size common symbol `%s'"),
	       S_GET_NAME (sym));
      sym->sy_frag  = frag_now;
      S_SET_OTHER (sym, const_flag);
      S_SET_VALUE (sym, frag_now_fix ());
      /* Keep N_EXT bit.  */
      sym->sy_symbol.n_type |= SEGMENT_TO_SYMBOL_TYPE ((int) now_seg);
      return 1;
    }

  return 0;
}

/* `tc_frob_label' handler for colon(symbols.c), used to examine the
   dummy label(s) gcc inserts at the beginning of each file it generates.
   gcc 1.x put "gcc_compiled."; gcc 2.x (as of 2.7) puts "gcc2_compiled."
   and "__gnu_language_<name>" and possibly "__vax_<type>_doubles".  */

void
vms_check_for_special_label (symbolS *symbolP)
{
  /* Special labels only occur prior to explicit section directives.  */
  if ((const_flag & IN_DEFAULT_SECTION) != 0)
    {
      char *sym_name = S_GET_NAME (symbolP);

      if (*sym_name == '_')
	++sym_name;

      if (!strcmp (sym_name, "__vax_g_doubles"))
	vax_g_doubles = 1;
#if 0	/* not necessary */
      else if (!strcmp (sym_name, "__vax_d_doubles"))
	vax_g_doubles = 0;
#endif
#if 0	/* These are potential alternatives to tc-vax.c's md_parse_options().  */
      else if (!strcmp (sym_name, "gcc_compiled."))
	flag_one = 1;
      else if (!strcmp (sym_name, "__gnu_language_cplusplus"))
	flag_hash_long_names = 1;
#endif
    }
}

void
obj_read_begin_hook (void)
{
}

void
obj_crawl_symbol_chain (object_headers *headers)
{
  symbolS *symbolP;
  symbolS **symbolPP;
  int symbol_number = 0;

  symbolPP = &symbol_rootP;	/* -> last symbol chain link.  */
  while ((symbolP = *symbolPP) != NULL)
    {
      resolve_symbol_value (symbolP);

     /* OK, here is how we decide which symbols go out into the
	brave new symtab.  Symbols that do are:

	* symbols with no name (stabd's?)
	* symbols with debug info in their N_TYPE
	* symbols with \1 as their 3rd character (numeric labels)
	* "local labels" needed for PIC fixups

	Symbols that don't are:
	* symbols that are registers

	All other symbols are output.  We complain if a deleted
	symbol was marked external.  */

      if (!S_IS_REGISTER (symbolP))
	{
	  symbolP->sy_number = symbol_number++;
	  symbolP->sy_name_offset = 0;
	  symbolPP = &symbolP->sy_next;
	}
      else
	{
	  if (S_IS_EXTERNAL (symbolP) || !S_IS_DEFINED (symbolP))
	    as_bad (_("Local symbol %s never defined"),
		    S_GET_NAME (symbolP));

	  /* Unhook it from the chain.  */
	  *symbolPP = symbol_next (symbolP);
	}
    }

  H_SET_STRING_SIZE (headers, string_byte_count);
  H_SET_SYMBOL_TABLE_SIZE (headers, symbol_number);
}


/* VMS OBJECT FILE HACKING ROUTINES.  */

/* Create the VMS object file.  */

static void
Create_VMS_Object_File (void)
{
#ifdef eunice
  VMS_Object_File_FD = creat (out_file_name, 0777, "var");
#else
#ifndef VMS
  VMS_Object_File_FD = creat (out_file_name, 0777);
#else	/* VMS */
  VMS_Object_File_FD = creat (out_file_name, 0, "rfm=var",
			      "ctx=bin", "mbc=16", "deq=64", "fop=tef",
			      "shr=nil");
#endif	/* !VMS */
#endif	/* !eunice */
  /* Deal with errors.  */
  if (VMS_Object_File_FD < 0)
    as_fatal (_("Couldn't create VMS object file \"%s\""), out_file_name);
  /* Initialize object file hacking variables.  */
  Object_Record_Offset = 0;
  Current_Object_Record_Type = -1;
}

/* Flush the object record buffer to the object file.  */

static void
Flush_VMS_Object_Record_Buffer (void)
{
  /* If the buffer is empty, there's nothing to do.  */
  if (Object_Record_Offset == 0)
    return;

#ifndef VMS			/* For cross-assembly purposes.  */
  {
    char RecLen[2];

    /* "Variable-length record" files have a two byte length field
       prepended to each record.  It's normally out-of-band, and native
       VMS output will insert it automatically for this type of file.
       When cross-assembling, we must write it explicitly.  */
    md_number_to_chars (RecLen, Object_Record_Offset, 2);
    if (write (VMS_Object_File_FD, RecLen, 2) != 2)
      error (_("I/O error writing VMS object file (length prefix)"));
    /* We also need to force the actual record to be an even number of
       bytes.  For native output, that's automatic; when cross-assembling,
       pad with a NUL byte if length is odd.  Do so _after_ writing the
       pre-padded length.  Since our buffer is defined with even size,
       an odd offset implies that it has some room left.  */
    if ((Object_Record_Offset & 1) != 0)
      Object_Record_Buffer[Object_Record_Offset++] = '\0';
  }
#endif /* not VMS */

  /* Write the data to the file.  */
  if ((size_t) write (VMS_Object_File_FD, Object_Record_Buffer,
		      Object_Record_Offset) != Object_Record_Offset)
    error (_("I/O error writing VMS object file"));

  /* The buffer is now empty.  */
  Object_Record_Offset = 0;
}

/* Declare a particular type of object file record.  */

static void
Set_VMS_Object_File_Record (int Type)
{
  /* If the type matches, we are done.  */
  if (Type == Current_Object_Record_Type)
    return;
  /* Otherwise: flush the buffer.  */
  Flush_VMS_Object_Record_Buffer ();
  /* Remember the new type.  */
  Current_Object_Record_Type = Type;
}

/* Close the VMS Object file.  */

static void
Close_VMS_Object_File (void)
{
  /* Flush (should never be necessary) and reset saved record-type context.  */
  Set_VMS_Object_File_Record (-1);

#ifndef VMS			/* For cross-assembly purposes.  */
  {
    char RecLen[2];
    int minus_one = -1;

    /* Write a 2 byte record-length field of -1 into the file, which
       means end-of-block when read, hence end-of-file when occurring
       in the file's last block.  It is only needed for variable-length
       record files transferred to VMS as fixed-length record files
       (typical for binary FTP; NFS shouldn't need it, but it won't hurt).  */
    md_number_to_chars (RecLen, minus_one, 2);
    write (VMS_Object_File_FD, RecLen, 2);
  }
#else
    /* When written on a VMS system, the file header (cf inode) will record
       the actual end-of-file position and no inline marker is needed.  */
#endif

  close (VMS_Object_File_FD);
}

/* Text Information and Relocation routines. */

/* Stack Psect base followed by signed, varying-sized offset.
   Common to several object records.  */

static void
vms_tir_stack_psect (int Psect_Index, int Offset, int Force)
{
  int psect_width, offset_width;

  psect_width = ((unsigned) Psect_Index > 255) ? 2 : 1;
  offset_width = (Force || Offset > 32767 || Offset < -32768) ? 4
		 : (Offset > 127 || Offset < -128) ? 2 : 1;
#define Sta_P(p,o) (((o)<<1) | ((p)-1))
  /* Byte or word psect; byte, word, or longword offset.  */
  switch (Sta_P(psect_width,offset_width))
    {
      case Sta_P(1,1):	PUT_CHAR (TIR_S_C_STA_PB);
			PUT_CHAR ((char) (unsigned char) Psect_Index);
			PUT_CHAR ((char) Offset);
			break;
      case Sta_P(1,2):	PUT_CHAR (TIR_S_C_STA_PW);
			PUT_CHAR ((char) (unsigned char) Psect_Index);
			PUT_SHORT (Offset);
			break;
      case Sta_P(1,4):	PUT_CHAR (TIR_S_C_STA_PL);
			PUT_CHAR ((char) (unsigned char) Psect_Index);
			PUT_LONG (Offset);
			break;
      case Sta_P(2,1):	PUT_CHAR (TIR_S_C_STA_WPB);
			PUT_SHORT (Psect_Index);
			PUT_CHAR ((char) Offset);
			break;
      case Sta_P(2,2):	PUT_CHAR (TIR_S_C_STA_WPW);
			PUT_SHORT (Psect_Index);
			PUT_SHORT (Offset);
			break;
      case Sta_P(2,4):	PUT_CHAR (TIR_S_C_STA_WPL);
			PUT_SHORT (Psect_Index);
			PUT_LONG (Offset);
			break;
    }
#undef Sta_P
}

/* Store immediate data in current Psect.  */

static void
VMS_Store_Immediate_Data (const char *Pointer, int Size, int Record_Type)
{
  int i;

  Set_VMS_Object_File_Record (Record_Type);
  /* We can only store as most 128 bytes at a time due to the way that
     TIR commands are encoded.  */
  while (Size > 0)
    {
      i = (Size > 128) ? 128 : Size;
      Size -= i;
      /* If we cannot accommodate this record, flush the buffer.  */
      if ((Object_Record_Offset + i + 1) >= sizeof Object_Record_Buffer)
	Flush_VMS_Object_Record_Buffer ();
      /* If the buffer is empty we must insert record type.  */
      if (Object_Record_Offset == 0)
	PUT_CHAR (Record_Type);
      /* Store the count.  The Store Immediate TIR command is implied by
         a negative command byte, and the length of the immediate data
         is abs(command_byte).  So, we write the negated length value.  */
      PUT_CHAR ((char) (-i & 0xff));
      /* Now store the data.  */
      while (--i >= 0)
	PUT_CHAR (*Pointer++);
    }
  /* Flush the buffer if it is more than 75% full.  */
  if (Object_Record_Offset > (sizeof (Object_Record_Buffer) * 3 / 4))
    Flush_VMS_Object_Record_Buffer ();
}

/* Make a data reference.  */

static void
VMS_Set_Data (int Psect_Index, int Offset, int Record_Type, int Force)
{
  Set_VMS_Object_File_Record (Record_Type);
  /* If the buffer is empty we must insert the record type.  */
  if (Object_Record_Offset == 0)
    PUT_CHAR (Record_Type);
  /* Stack the Psect base with its offset.  */
  vms_tir_stack_psect (Psect_Index, Offset, Force);
  /* Set relocation base.  */
  PUT_CHAR (TIR_S_C_STO_PIDR);
  /* Flush the buffer if it is more than 75% full.  */
  if (Object_Record_Offset > (sizeof (Object_Record_Buffer) * 3 / 4))
    Flush_VMS_Object_Record_Buffer ();
}

/* Make a debugger reference to a struct, union or enum.  */

static void
VMS_Store_Struct (int Struct_Index)
{
  /* We are writing a debug record.  */
  Set_VMS_Object_File_Record (OBJ_S_C_DBG);
  /* If the buffer is empty we must insert the record type.  */
  if (Object_Record_Offset == 0)
    PUT_CHAR (OBJ_S_C_DBG);
  PUT_CHAR (TIR_S_C_STA_UW);
  PUT_SHORT (Struct_Index);
  PUT_CHAR (TIR_S_C_CTL_STKDL);
  PUT_CHAR (TIR_S_C_STO_L);
  /* Flush the buffer if it is more than 75% full.  */
  if (Object_Record_Offset > (sizeof (Object_Record_Buffer) * 3 / 4))
    Flush_VMS_Object_Record_Buffer ();
}

/* Make a debugger reference to partially define a struct, union or enum.  */

static void
VMS_Def_Struct (int Struct_Index)
{
  /* We are writing a debug record.  */
  Set_VMS_Object_File_Record (OBJ_S_C_DBG);
  /* If the buffer is empty we must insert the record type.  */
  if (Object_Record_Offset == 0)
    PUT_CHAR (OBJ_S_C_DBG);
  PUT_CHAR (TIR_S_C_STA_UW);
  PUT_SHORT (Struct_Index);
  PUT_CHAR (TIR_S_C_CTL_DFLOC);
  /* Flush the buffer if it is more than 75% full.  */
  if (Object_Record_Offset > (sizeof (Object_Record_Buffer) * 3 / 4))
    Flush_VMS_Object_Record_Buffer ();
}

static void
VMS_Set_Struct (int Struct_Index)
{
  Set_VMS_Object_File_Record (OBJ_S_C_DBG);
  if (Object_Record_Offset == 0)
    PUT_CHAR (OBJ_S_C_DBG);
  PUT_CHAR (TIR_S_C_STA_UW);
  PUT_SHORT (Struct_Index);
  PUT_CHAR (TIR_S_C_CTL_STLOC);
  if (Object_Record_Offset > (sizeof (Object_Record_Buffer) * 3 / 4))
    Flush_VMS_Object_Record_Buffer ();
}

/* Traceback Information routines.  */

/* Write the Traceback Module Begin record.  */

static void
VMS_TBT_Module_Begin (void)
{
  char *cp, *cp1;
  int Size;
  char Local[256];

  /* Arrange to store the data locally (leave room for size byte).  */
  cp = &Local[1];
  /* Begin module.  */
  *cp++ = DST_S_C_MODBEG;
  *cp++ = 0;		/* flags; not used */
  /* Language type == "C"
    (FIXME:  this should be based on the input...)  */
  COPY_LONG (cp, DST_S_C_C);
  cp += 4;
  /* Store the module name.  */
  *cp++ = (char) strlen (Module_Name);
  cp1 = Module_Name;
  while (*cp1)
    *cp++ = *cp1++;
  /* Now we can store the record size.  */
  Size = (cp - Local);
  Local[0] = Size - 1;
  /* Put it into the object record.  */
  VMS_Store_Immediate_Data (Local, Size, OBJ_S_C_TBT);
}

/* Write the Traceback Module End record.  */

static void
VMS_TBT_Module_End (void)
{
  char Local[2];

  /* End module.  */
  Local[0] = 1;
  Local[1] = DST_S_C_MODEND;
  /* Put it into the object record.  */
  VMS_Store_Immediate_Data (Local, 2, OBJ_S_C_TBT);
}

/* Write a Traceback Routine Begin record.  */

static void
VMS_TBT_Routine_Begin (symbolS *symbolP, int Psect)
{
  char *cp, *cp1;
  char *Name;
  int Offset;
  int Size;
  char Local[512];

  /* Strip the leading "_" from the name.  */
  Name = S_GET_NAME (symbolP);
  if (*Name == '_')
    Name++;
  /* Get the text psect offset.  */
  Offset = S_GET_VALUE (symbolP);
  /* Set the record size.  */
  Size = 1 + 1 + 4 + 1 + strlen (Name);
  Local[0] = Size;
  /* DST type "routine begin".  */
  Local[1] = DST_S_C_RTNBEG;
  /* Uses CallS/CallG.  */
  Local[2] = 0;
  /* Store the data so far.  */
  VMS_Store_Immediate_Data (Local, 3, OBJ_S_C_TBT);
  /* Make sure we are still generating a OBJ_S_C_TBT record.  */
  if (Object_Record_Offset == 0)
    PUT_CHAR (OBJ_S_C_TBT);
  /* Stack the address.  */
  vms_tir_stack_psect (Psect, Offset, 0);
  /* Store the data reference.  */
  PUT_CHAR (TIR_S_C_STO_PIDR);
  /* Store the counted string as data.  */
  cp = Local;
  cp1 = Name;
  Size = strlen (cp1) + 1;
  *cp++ = Size - 1;
  while (*cp1)
    *cp++ = *cp1++;
  VMS_Store_Immediate_Data (Local, Size, OBJ_S_C_TBT);
}

/* Write a Traceback Routine End record.

   We *must* search the symbol table to find the next routine, since the
   assembler has a way of reassembling the symbol table OUT OF ORDER Thus
   the next routine in the symbol list is not necessarily the next one in
   memory.  For debugging to work correctly we must know the size of the
   routine.  */

static void
VMS_TBT_Routine_End (int Max_Size, symbolS *sp)
{
  symbolS *symbolP;
  unsigned long Size = 0x7fffffff;
  char Local[16];
  valueT sym_value, sp_value = S_GET_VALUE (sp);

  for (symbolP = symbol_rootP; symbolP; symbolP = symbol_next (symbolP))
    {
      if (!S_IS_DEBUG (symbolP) && S_GET_TYPE (symbolP) == N_TEXT)
	{
	  if (*S_GET_NAME (symbolP) == 'L')
	    continue;
	  sym_value = S_GET_VALUE (symbolP);
	  if (sym_value > sp_value && sym_value < Size)
	    Size = sym_value;

	  /* Dummy labels like "gcc_compiled." should no longer reach here.  */
#if 0
	  else
	    /* Check if gcc_compiled. has size of zero.  */
	    if (sym_value == sp_value &&
		sp != symbolP &&
		(!strcmp (S_GET_NAME (sp), "gcc_compiled.") ||
		 !strcmp (S_GET_NAME (sp), "gcc2_compiled.")))
	      Size = sym_value;
#endif
	}
    }
  if (Size == 0x7fffffff)
    Size = Max_Size;
  Size -= sp_value;		/* and get the size of the routine */
  /* Record Size.  */
  Local[0] = 6;
  /* DST type is "routine end".  */
  Local[1] = DST_S_C_RTNEND;
  Local[2] = 0;		/* unused */
  /* Size of routine.  */
  COPY_LONG (&Local[3], Size);
  /* Store the record.  */
  VMS_Store_Immediate_Data (Local, 7, OBJ_S_C_TBT);
}

/* Write a Traceback Block Begin record.  */

static void
VMS_TBT_Block_Begin (symbolS *symbolP, int Psect, char *Name)
{
  char *cp, *cp1;
  int Offset;
  int Size;
  char Local[512];

  /* Set the record size.  */
  Size = 1 + 1 + 4 + 1 + strlen (Name);
  Local[0] = Size;
  /* DST type is "begin block"; we simulate with a phony routine.  */
  Local[1] = DST_S_C_BLKBEG;
  /* Uses CallS/CallG.  */
  Local[2] = 0;
  /* Store the data so far.  */
  VMS_Store_Immediate_Data (Local, 3, OBJ_S_C_DBG);
  /* Make sure we are still generating a debug record.  */
  if (Object_Record_Offset == 0)
    PUT_CHAR (OBJ_S_C_DBG);
  /* Now get the symbol address.  */
  PUT_CHAR (TIR_S_C_STA_WPL);
  PUT_SHORT (Psect);
  /* Get the text psect offset.  */
  Offset = S_GET_VALUE (symbolP);
  PUT_LONG (Offset);
  /* Store the data reference.  */
  PUT_CHAR (TIR_S_C_STO_PIDR);
  /* Store the counted string as data.  */
  cp = Local;
  cp1 = Name;
  Size = strlen (cp1) + 1;
  *cp++ = Size - 1;
  while (*cp1)
    *cp++ = *cp1++;
  VMS_Store_Immediate_Data (Local, Size, OBJ_S_C_DBG);
}

/* Write a Traceback Block End record.  */

static void
VMS_TBT_Block_End (valueT Size)
{
  char Local[16];

  Local[0] = 6;		/* record length */
  /* DST type is "block end"; simulate with a phony end routine.  */
  Local[1] = DST_S_C_BLKEND;
  Local[2] = 0;		/* unused, must be zero */
  COPY_LONG (&Local[3], Size);
  VMS_Store_Immediate_Data (Local, 7, OBJ_S_C_DBG);
}


/* Write a Line number <-> Program Counter correlation record.  */

static void
VMS_TBT_Line_PC_Correlation (int Line_Number, int Offset,
			     int Psect, int Do_Delta)
{
  char *cp;
  char Local[64];

  if (Do_Delta == 0)
    {
      /* If not delta, set our PC/Line number correlation.  */
      cp = &Local[1];	/* Put size in Local[0] later.  */
      /* DST type is "Line Number/PC correlation".  */
      *cp++ = DST_S_C_LINE_NUM;
      /* Set Line number.  */
      if (Line_Number - 1 <= 255)
	{
	  *cp++ = DST_S_C_SET_LINUM_B;
	  *cp++ = (char) (Line_Number - 1);
	}
      else if (Line_Number - 1 <= 65535)
	{
	  *cp++ = DST_S_C_SET_LINE_NUM;
	  COPY_SHORT (cp, Line_Number - 1),  cp += 2;
	}
      else
	{
	  *cp++ = DST_S_C_SET_LINUM_L;
	  COPY_LONG (cp, Line_Number - 1),  cp += 4;
	}
      /* Set PC.  */
      *cp++ = DST_S_C_SET_ABS_PC;
      /* Store size now that we know it, then output the data.  */
      Local[0] = cp - &Local[1];
	/* Account for the space that TIR_S_C_STO_PIDR will use for the PC.  */
	Local[0] += 4;		/* size includes length of another longword */
      VMS_Store_Immediate_Data (Local, cp - Local, OBJ_S_C_TBT);
      /* Make sure we are still generating a OBJ_S_C_TBT record.  */
      if (Object_Record_Offset == 0)
	PUT_CHAR (OBJ_S_C_TBT);
      vms_tir_stack_psect (Psect, Offset, 0);
      PUT_CHAR (TIR_S_C_STO_PIDR);
      /* Do a PC offset of 0 to register the line number.  */
      Local[0] = 2;
      Local[1] = DST_S_C_LINE_NUM;
      Local[2] = 0;		/* Increment PC by 0 and register line # */
      VMS_Store_Immediate_Data (Local, 3, OBJ_S_C_TBT);
    }
  else
    {
      if (Do_Delta < 0)
	{
	  /* When delta is negative, terminate the line numbers.  */
	  Local[0] = 1 + 1 + 4;
	  Local[1] = DST_S_C_LINE_NUM;
	  Local[2] = DST_S_C_TERM_L;
	  COPY_LONG (&Local[3], Offset);
	  VMS_Store_Immediate_Data (Local, 7, OBJ_S_C_TBT);
	  return;
	}
      /* Do a PC/Line delta.  */
      cp = &Local[1];
      *cp++ = DST_S_C_LINE_NUM;
      if (Line_Number > 1)
	{
	  /* We need to increment the line number.  */
	  if (Line_Number - 1 <= 255)
	    {
	      *cp++ = DST_S_C_INCR_LINUM;
	      *cp++ = Line_Number - 1;
	    }
	  else if (Line_Number - 1 <= 65535)
	    {
	      *cp++ = DST_S_C_INCR_LINUM_W;
	      COPY_SHORT (cp, Line_Number - 1),  cp += 2;
	    }
	  else
	    {
	      *cp++ = DST_S_C_INCR_LINUM_L;
	      COPY_LONG (cp, Line_Number - 1),  cp += 4;
	    }
	}
      /* Increment the PC.  */
      if (Offset <= 128)
	{
	  /* Small offsets are encoded as negative numbers, rather than the
	     usual non-negative type code followed by another data field.  */
	  *cp++ = (char) -Offset;
	}
      else if (Offset <= 65535)
	{
	  *cp++ = DST_S_C_DELTA_PC_W;
	  COPY_SHORT (cp, Offset),  cp += 2;
	}
      else
	{
	  *cp++ = DST_S_C_DELTA_PC_L;
	  COPY_LONG (cp, Offset),  cp += 4;
	}
      /* Set size now that be know it, then output the data.  */
      Local[0] = cp - &Local[1];
      VMS_Store_Immediate_Data (Local, cp - Local, OBJ_S_C_TBT);
    }
}


/* Describe a source file to the debugger.  */

static int
VMS_TBT_Source_File (char *Filename, int ID_Number)
{
  char *cp;
  int len, rfo, ffb, ebk;
  char cdt[8];
  char Local[512];
#ifdef VMS			/* Used for native assembly */
  unsigned Status;
  struct FAB fab;		/* RMS file access block */
  struct NAM nam;		/* file name information */
  struct XABDAT xabdat;		/* date+time fields */
  struct XABFHC xabfhc;		/* file header characteristics */
  char resultant_string_buffer[255 + 1];

  /* Set up RMS structures:  */
  /* FAB -- file access block */
  memset ((char *) &fab, 0, sizeof fab);
  fab.fab$b_bid = FAB$C_BID;
  fab.fab$b_bln = (unsigned char) sizeof fab;
  fab.fab$l_fna = Filename;
  fab.fab$b_fns = (unsigned char) strlen (Filename);
  fab.fab$l_nam = (char *) &nam;
  fab.fab$l_xab = (char *) &xabdat;
  /* NAM -- file name block.  */
  memset ((char *) &nam, 0, sizeof nam);
  nam.nam$b_bid = NAM$C_BID;
  nam.nam$b_bln = (unsigned char) sizeof nam;
  nam.nam$l_rsa = resultant_string_buffer;
  nam.nam$b_rss = (unsigned char) (sizeof resultant_string_buffer - 1);
  /* XABs -- extended attributes blocks.  */
  memset ((char *) &xabdat, 0, sizeof xabdat);
  xabdat.xab$b_cod = XAB$C_DAT;
  xabdat.xab$b_bln = (unsigned char) sizeof xabdat;
  xabdat.xab$l_nxt = (char *) &xabfhc;
  memset ((char *) &xabfhc, 0, sizeof xabfhc);
  xabfhc.xab$b_cod = XAB$C_FHC;
  xabfhc.xab$b_bln = (unsigned char) sizeof xabfhc;
  xabfhc.xab$l_nxt = 0;

  /* Get the file information.  */
  Status = sys$open (&fab);
  if (!(Status & 1))
    {
      as_tsktsk (_("Couldn't find source file \"%s\", status=%%X%x"),
		 Filename, Status);
      return 0;
    }
  sys$close (&fab);
  /* Now extract fields of interest.  */
  memcpy (cdt, (char *) &xabdat.xab$q_cdt, 8);	/* creation date */
  ebk = xabfhc.xab$l_ebk;		/* end-of-file block */
  ffb = xabfhc.xab$w_ffb;		/* first free byte of last block */
  rfo = xabfhc.xab$b_rfo;		/* record format */
  len = nam.nam$b_rsl;			/* length of Filename */
  resultant_string_buffer[len] = '\0';
  Filename = resultant_string_buffer;	/* full filename */
#else				/* Cross-assembly */
  /* [Perhaps we ought to use actual values derived from stat() here?]  */
  memset (cdt, 0, 8);			/* null VMS quadword binary time */
  ebk = ffb = rfo = 0;
  len = strlen (Filename);
  if (len > 255)	/* a single byte is used as count prefix */
    {
      Filename += (len - 255);		/* tail end is more significant */
      len = 255;
    }
#endif /* VMS */

  cp = &Local[1];			/* fill in record length later */
  *cp++ = DST_S_C_SOURCE;		/* DST type is "source file" */
  *cp++ = DST_S_C_SRC_FORMFEED;		/* formfeeds count as source records */
  *cp++ = DST_S_C_SRC_DECLFILE;		/* declare source file */
  know (cp == &Local[4]);
  *cp++ = 0;				/* fill in this length below */
  *cp++ = 0;				/* flags; must be zero */
  COPY_SHORT (cp, ID_Number),  cp += 2;	/* file ID number */
  memcpy (cp, cdt, 8),  cp += 8;	/* creation date+time */
  COPY_LONG (cp, ebk),  cp += 4;	/* end-of-file block */
  COPY_SHORT (cp, ffb),  cp += 2;	/* first free byte of last block */
  *cp++ = (char) rfo;			/* RMS record format */
  /* Filename.  */
  *cp++ = (char) len;
  while (--len >= 0)
    *cp++ = *Filename++;
  /* Library module name (none).  */
  *cp++ = 0;
  /* Now that size is known, fill it in and write out the record.  */
  Local[4] = cp - &Local[5];		/* source file declaration size */
  Local[0] = cp - &Local[1];		/* TBT record size */
  VMS_Store_Immediate_Data (Local, cp - Local, OBJ_S_C_TBT);
  return 1;
}

/* Traceback information is described in terms of lines from compiler
   listing files, not lines from source files.  We need to set up the
   correlation between listing line numbers and source line numbers.
   Since gcc's .stabn directives refer to the source lines, we just
   need to describe a one-to-one correspondence.  */

static void
VMS_TBT_Source_Lines (int ID_Number, int Starting_Line_Number,
		      int Number_Of_Lines)
{
  char *cp;
  int chunk_limit;
  char Local[128];	/* room enough to describe 1310700 lines...  */

  cp = &Local[1];	/* Put size in Local[0] later.  */
  *cp++ = DST_S_C_SOURCE;		/* DST type is "source file".  */
  *cp++ = DST_S_C_SRC_SETFILE;		/* Set Source File.  */
  COPY_SHORT (cp, ID_Number),  cp += 2;	/* File ID Number.  */
  /* Set record number and define lines.  Since no longword form of
     SRC_DEFLINES is available, we need to be able to cope with any huge
     files a chunk at a time.  It doesn't matter for tracebacks, since
     unspecified lines are mapped one-to-one and work out right, but it
     does matter within the debugger.  Without this explicit mapping,
     it will complain about lines not existing in the module.  */
  chunk_limit = (sizeof Local - 5) / 6;
  if (Number_Of_Lines > 65535 * chunk_limit)	/* avoid buffer overflow */
    Number_Of_Lines = 65535 * chunk_limit;
  while (Number_Of_Lines > 65535)
    {
      *cp++ = DST_S_C_SRC_SETREC_L;
      COPY_LONG (cp, Starting_Line_Number),  cp += 4;
      *cp++ = DST_S_C_SRC_DEFLINES_W;
      COPY_SHORT (cp, 65535),  cp += 2;
      Starting_Line_Number += 65535;
      Number_Of_Lines -= 65535;
    }
  /* Set record number and define lines, normal case.  */
  if (Starting_Line_Number <= 65535)
    {
      *cp++ = DST_S_C_SRC_SETREC_W;
      COPY_SHORT (cp, Starting_Line_Number),  cp += 2;
    }
  else
    {
      *cp++ = DST_S_C_SRC_SETREC_L;
      COPY_LONG (cp, Starting_Line_Number),  cp += 4;
    }
  *cp++ = DST_S_C_SRC_DEFLINES_W;
  COPY_SHORT (cp, Number_Of_Lines),  cp += 2;
  /* Set size now that be know it, then output the data.  */
  Local[0] = cp - &Local[1];
  VMS_Store_Immediate_Data (Local, cp - Local, OBJ_S_C_TBT);
}


/* Debugger Information support routines. */

/* This routine locates a file in the list of files.  If an entry does
   not exist, one is created.  For include files, a new entry is always
   created such that inline functions can be properly debugged.  */

static struct input_file *
find_file (symbolS *sp)
{
  struct input_file *same_file = 0;
  struct input_file *fpnt, *last = 0;
  char *sp_name;

  for (fpnt = file_root; fpnt; fpnt = fpnt->next)
    {
      if (fpnt->spnt == sp)
	return fpnt;
      last = fpnt;
    }
  sp_name = S_GET_NAME (sp);
  for (fpnt = file_root; fpnt; fpnt = fpnt->next)
    {
      if (strcmp (sp_name, fpnt->name) == 0)
	{
	  if (fpnt->flag == 1)
	    return fpnt;
	  same_file = fpnt;
	  break;
	}
    }
  fpnt = xmalloc (sizeof (struct input_file));
  if (!file_root)
    file_root = fpnt;
  else
    last->next = fpnt;
  fpnt->next = 0;
  fpnt->name = sp_name;
  fpnt->min_line = 0x7fffffff;
  fpnt->max_line = 0;
  fpnt->offset = 0;
  fpnt->flag = 0;
  fpnt->file_number = 0;
  fpnt->spnt = sp;
  fpnt->same_file_fpnt = same_file;
  return fpnt;
}

/* This routine converts a number string into an integer, and stops when
   it sees an invalid character.  The return value is the address of the
   character just past the last character read.  No error is generated.  */

static char *
cvt_integer (char *str, int *rtn)
{
  int ival = 0, sgn = 1;

  if (*str == '-')
    sgn = -1,  ++str;
  while (*str >= '0' && *str <= '9')
    ival = 10 * ival + *str++ - '0';
  *rtn = sgn * ival;
  return str;
}


/* The following functions and definitions are used to generate object
   records that will describe program variables to the VMS debugger.

   This file contains many of the routines needed to output debugging info
   into the object file that the VMS debugger needs to understand symbols.
   These routines are called very late in the assembly process, and thus
   we can be fairly lax about changing things, since the GSD and the TIR
   sections have already been output.  */

/* This routine fixes the names that are generated by C++, ".this" is a good
   example.  The period does not work for the debugger, since it looks like
   the syntax for a structure element, and thus it gets mightily confused.

   We also use this to strip the PsectAttribute hack from the name before we
   write a debugger record.  */

static char *
fix_name (char *pnt)
{
  char *pnt1;

  /* Kill any leading "_".  */
  if (*pnt == '_')
    pnt++;

  /* Is there a Psect Attribute to skip??  */
  if (HAS_PSECT_ATTRIBUTES (pnt))
    {
      /* Yes: Skip it.  */
      pnt += PSECT_ATTRIBUTES_STRING_LENGTH;
      while (*pnt)
	{
	  if ((pnt[0] == '$') && (pnt[1] == '$'))
	    {
	      pnt += 2;
	      break;
	    }
	  pnt++;
	}
    }

  /* Here we fix the .this -> $this conversion.  */
  for (pnt1 = pnt; *pnt1 != 0; pnt1++)
    if (*pnt1 == '.')
      *pnt1 = '$';

  return pnt;
}

/* When defining a structure, this routine is called to find the name of
   the actual structure.  It is assumed that str points to the equal sign
   in the definition, and it moves backward until it finds the start of the
   name.  If it finds a 0, then it knows that this structure def is in the
   outermost level, and thus symbol_name points to the symbol name.  */

static char *
get_struct_name (char *str)
{
  char *pnt;
  pnt = str;
  while ((*pnt != ':') && (*pnt != '\0'))
    pnt--;
  if (*pnt == '\0')
    return (char *) symbol_name;
  *pnt-- = '\0';
  while ((*pnt != ';') && (*pnt != '='))
    pnt--;
  if (*pnt == ';')
    return pnt + 1;
  while ((*pnt < '0') || (*pnt > '9'))
    pnt++;
  while ((*pnt >= '0') && (*pnt <= '9'))
    pnt++;
  return pnt;
}

/* Search symbol list for type number dbx_type.
   Return a pointer to struct.  */

static struct VMS_DBG_Symbol *
find_symbol (int dbx_type)
{
  struct VMS_DBG_Symbol *spnt;

  spnt = VMS_Symbol_type_list[SYMTYP_HASH (dbx_type)];
  while (spnt)
    {
      if (spnt->dbx_type == dbx_type)
	break;
      spnt = spnt->next;
    }
  if (!spnt || spnt->advanced != ALIAS)
    return spnt;
  return find_symbol (spnt->type2);
}

#if 0		/* obsolete */
/* This routine puts info into either Local or Asuffix, depending on the sign
   of size.  The reason is that it is easier to build the variable descriptor
   backwards, while the array descriptor is best built forwards.  In the end
   they get put together, if there is not a struct/union/enum along the way.  */

static void
push (int value, int size1)
{
  if (size1 < 0)
    {
      size1 = -size1;
      if (Lpnt < size1)
	{
	  overflow = 1;
	  Lpnt = 1;
	  return;
	}
      Lpnt -= size1;
      md_number_to_chars (&Local[Lpnt + 1], value, size1);
    }
  else
    {
      if (Apoint + size1 >= MAX_DEBUG_RECORD)
	{
	  overflow = 1;
	  Apoint = MAX_DEBUG_RECORD - 1;
	  return;
	}
      md_number_to_chars (&Asuffix[Apoint], value, size1);
      Apoint += size1;
    }
}
#endif

static void
fpush (int value, int size)
{
  if (Apoint + size >= MAX_DEBUG_RECORD)
    {
      overflow = 1;
      Apoint = MAX_DEBUG_RECORD - 1;
      return;
    }
  if (size == 1)
    Asuffix[Apoint++] = (char) value;
  else
    {
      md_number_to_chars (&Asuffix[Apoint], value, size);
      Apoint += size;
    }
}

static void
rpush (int value, int size)
{
  if (Lpnt < size)
    {
      overflow = 1;
      Lpnt = 1;
      return;
    }
  if (size == 1)
      Local[Lpnt--] = (char) value;
  else
    {
      Lpnt -= size;
      md_number_to_chars (&Local[Lpnt + 1], value, size);
    }
}

/* This routine generates the array descriptor for a given array.  */

static void
array_suffix (struct VMS_DBG_Symbol *spnt2)
{
  struct VMS_DBG_Symbol *spnt;
  struct VMS_DBG_Symbol *spnt1;
  int rank;
  int total_size;

  rank = 0;
  spnt = spnt2;
  while (spnt->advanced != ARRAY)
    {
      spnt = find_symbol (spnt->type2);
      if (!spnt)
	return;
    }
  spnt1 = spnt;
  total_size = 1;
  while (spnt1->advanced == ARRAY)
    {
      rank++;
      total_size *= (spnt1->index_max - spnt1->index_min + 1);
      spnt1 = find_symbol (spnt1->type2);
    }
  total_size = total_size * spnt1->data_size;
  fpush (spnt1->data_size, 2);	/* element size */
  if (spnt1->VMS_type == DBG_S_C_ADVANCED_TYPE)
    fpush (0, 1);
  else
    fpush (spnt1->VMS_type, 1);	/* element type */
  fpush (DSC_K_CLASS_A, 1);	/* descriptor class */
  fpush (0, 4);			/* base address */
  fpush (0, 1);			/* scale factor -- not applicable */
  fpush (0, 1);			/* digit count -- not applicable */
  fpush (0xc0, 1);		/* flags: multiplier block & bounds present */
  fpush (rank, 1);		/* number of dimensions */
  fpush (total_size, 4);
  fpush (0, 4);			/* pointer to element [0][0]...[0] */
  spnt1 = spnt;
  while (spnt1->advanced == ARRAY)
    {
      fpush (spnt1->index_max - spnt1->index_min + 1, 4);
      spnt1 = find_symbol (spnt1->type2);
    }
  spnt1 = spnt;
  while (spnt1->advanced == ARRAY)
    {
      fpush (spnt1->index_min, 4);
      fpush (spnt1->index_max, 4);
      spnt1 = find_symbol (spnt1->type2);
    }
}

/* This routine generates the start of a variable descriptor based upon
   a struct/union/enum that has yet to be defined.  We define this spot as
   a new location, and save four bytes for the address.  When the struct is
   finally defined, then we can go back and plug in the correct address.  */

static void
new_forward_ref (int dbx_type)
{
  struct forward_ref *fpnt;

  fpnt = xmalloc (sizeof (struct forward_ref));
  fpnt->next = f_ref_root;
  f_ref_root = fpnt;
  fpnt->dbx_type = dbx_type;
  fpnt->struc_numb = ++structure_count;
  fpnt->resolved = 'N';
  rpush (DST_K_TS_IND, 1);	/* indirect type specification */
  total_len = 5;
  rpush (total_len, 2);
  struct_number = -fpnt->struc_numb;
}

/* This routine generates the variable descriptor used to describe non-basic
   variables.  It calls itself recursively until it gets to the bottom of it
   all, and then builds the descriptor backwards.  It is easiest to do it
   this way since we must periodically write length bytes, and it is easiest
   if we know the value when it is time to write it.  */

static int
gen1 (struct VMS_DBG_Symbol *spnt, int array_suffix_len)
{
  struct VMS_DBG_Symbol *spnt1;
  int i;

  switch (spnt->advanced)
    {
    case VOID:
      rpush (DBG_S_C_VOID, 1);
      total_len += 1;
      rpush (total_len, 2);
      return 0;
    case BASIC:
    case FUNCTION:
      if (array_suffix_len == 0)
	{
	  rpush (spnt->VMS_type, 1);
	  rpush (DBG_S_C_BASIC, 1);
	  total_len = 2;
	  rpush (total_len, 2);
	  return 1;
	}
      rpush (0, 4);
      rpush (DST_K_VFLAGS_DSC, 1);
      rpush (DST_K_TS_DSC, 1);	/* Descriptor type specification.  */
      total_len = -2;
      return 1;
    case STRUCT:
    case UNION:
    case ENUM:
      struct_number = spnt->struc_numb;
      if (struct_number < 0)
	{
	  new_forward_ref (spnt->dbx_type);
	  return 1;
	}
      rpush (DBG_S_C_STRUCT, 1);
      total_len = 5;
      rpush (total_len, 2);
      return 1;
    case POINTER:
      spnt1 = find_symbol (spnt->type2);
      i = 1;
      if (!spnt1)
	new_forward_ref (spnt->type2);
      else
	i = gen1 (spnt1, 0);
      if (i)
	{
	  /* (*void) is a special case, do not put pointer suffix.  */
	  rpush (DBG_S_C_POINTER, 1);
	  total_len += 3;
	  rpush (total_len, 2);
	}
      return 1;
    case ARRAY:
      spnt1 = spnt;
      while (spnt1->advanced == ARRAY)
	{
	  spnt1 = find_symbol (spnt1->type2);
	  if (!spnt1)
	    {
	      as_tsktsk (_("debugger forward reference error, dbx type %d"),
			 spnt->type2);
	      return 0;
	    }
	}
      /* It is too late to generate forward references, so the user
	 gets a message.  This should only happen on a compiler error.  */
      (void) gen1 (spnt1, 1);
      i = Apoint;
      array_suffix (spnt);
      array_suffix_len = Apoint - i;
      switch (spnt1->advanced)
	{
	case BASIC:
	case FUNCTION:
	  break;
	default:
	  rpush (0, 2);
	  total_len += 2;
	  rpush (total_len, 2);
	  rpush (DST_K_VFLAGS_DSC, 1);
	  rpush (1, 1);		/* Flags: element value spec included.  */
	  rpush (1, 1);		/* One dimension.  */
	  rpush (DBG_S_C_COMPLEX_ARRAY, 1);
	}
      total_len += array_suffix_len + 8;
      rpush (total_len, 2);
      break;
    default:
      break;
    }
  return 0;
}

/* This generates a suffix for a variable.  If it is not a defined type yet,
   then dbx_type contains the type we are expecting so we can generate a
   forward reference.  This calls gen1 to build most of the descriptor, and
   then it puts the icing on at the end.  It then dumps whatever is needed
   to get a complete descriptor (i.e. struct reference, array suffix).  */

static void
generate_suffix (struct VMS_DBG_Symbol *spnt, int dbx_type)
{
  static const char pvoid[6] =
    {
      5,		/* record.length == 5 */
      DST_K_TYPSPEC,	/* record.type == 1 (type specification) */
      0,		/* name.length == 0, no name follows */
      1, 0,		/* type.length == 1 {2 bytes, little endian} */
      DBG_S_C_VOID	/* type.type == 5 (pointer to unspecified) */
    };
  int i;

  Apoint = 0;
  Lpnt = MAX_DEBUG_RECORD - 1;
  total_len = 0;
  struct_number = 0;
  overflow = 0;
  if (!spnt)
    new_forward_ref (dbx_type);
  else
    {
      if (spnt->VMS_type != DBG_S_C_ADVANCED_TYPE)
	return;		/* no suffix needed */
      gen1 (spnt, 0);
    }
  rpush (0, 1);		/* no name (len==0) */
  rpush (DST_K_TYPSPEC, 1);
  total_len += 4;
  rpush (total_len, 1);
  /* If the variable descriptor overflows the record, output a descriptor
     for a pointer to void.  */
  if ((total_len >= MAX_DEBUG_RECORD) || overflow)
    {
      as_warn (_("Variable descriptor %d too complicated.  Defined as `void *'."),
		spnt->dbx_type);
      VMS_Store_Immediate_Data (pvoid, 6, OBJ_S_C_DBG);
      return;
    }
  i = 0;
  while (Lpnt < MAX_DEBUG_RECORD - 1)
    Local[i++] = Local[++Lpnt];
  Lpnt = i;
  /* We use this for reference to structure that has already been defined.  */
  if (struct_number > 0)
    {
      VMS_Store_Immediate_Data (Local, Lpnt, OBJ_S_C_DBG);
      Lpnt = 0;
      VMS_Store_Struct (struct_number);
    }
  /* We use this for a forward reference to a structure that has yet to
     be defined.  We store four bytes of zero to make room for the actual
     address once it is known.  */
  if (struct_number < 0)
    {
      struct_number = -struct_number;
      VMS_Store_Immediate_Data (Local, Lpnt, OBJ_S_C_DBG);
      Lpnt = 0;
      VMS_Def_Struct (struct_number);
      COPY_LONG (&Local[Lpnt], 0L);
      Lpnt += 4;
      VMS_Store_Immediate_Data (Local, Lpnt, OBJ_S_C_DBG);
      Lpnt = 0;
    }
  i = 0;
  while (i < Apoint)
    Local[Lpnt++] = Asuffix[i++];
  if (Lpnt != 0)
    VMS_Store_Immediate_Data (Local, Lpnt, OBJ_S_C_DBG);
  Lpnt = 0;
}

/* "novel length" type doesn't work for simple atomic types.  */
#define USE_BITSTRING_DESCRIPTOR(t) ((t)->advanced == BASIC)
#undef SETUP_BASIC_TYPES

/* This routine generates a type description for a bitfield.  */

static void
bitfield_suffix (struct VMS_DBG_Symbol *spnt, int width)
{
  Local[Lpnt++] = 13;			/* rec.len==13 */
  Local[Lpnt++] = DST_K_TYPSPEC;	/* a type specification record */
  Local[Lpnt++] = 0;			/* not named */
  COPY_SHORT (&Local[Lpnt], 9);		/* typ.len==9 */
  Lpnt += 2;
  Local[Lpnt++] = DST_K_TS_NOV_LENG;	/* This type is a "novel length"
					   incarnation of some other type.  */
  COPY_LONG (&Local[Lpnt], width);	/* size in bits == novel length */
  Lpnt += 4;
  VMS_Store_Immediate_Data (Local, Lpnt, OBJ_S_C_DBG);
  Lpnt = 0;
  /* assert( spnt->struc_numb > 0 ); */
  VMS_Store_Struct (spnt->struc_numb);	/* output 4 more bytes */
}

/* Formally define a builtin type, so that it can serve as the target of
   an indirect reference.  It makes bitfield_suffix() easier by avoiding
   the need to use a forward reference for the first occurrence of each
   type used in a bitfield.  */

static void
setup_basic_type (struct VMS_DBG_Symbol *spnt ATTRIBUTE_UNUSED)
{
#ifdef SETUP_BASIC_TYPES
  /* This would be very useful if "novel length" fields actually worked
     with basic types like they do with enumerated types.  However,
     they do not, so this isn't worth doing just so that you can use
     EXAMINE/TYPE=(__long_long_int) instead of EXAMINE/QUAD.  */
  char *p;
#ifndef SETUP_SYNONYM_TYPES
  /* This determines whether compatible things like `int' and `long int'
     ought to have distinct type records rather than sharing one.  */
  struct VMS_DBG_Symbol *spnt2;

  /* First check whether this type has already been seen by another name.  */
  for (spnt2 = VMS_Symbol_type_list[SYMTYP_HASH (spnt->VMS_type)];
       spnt2;
       spnt2 = spnt2->next)
    if (spnt2 != spnt && spnt2->VMS_type == spnt->VMS_type)
      {
	spnt->struc_numb = spnt2->struc_numb;
	return;
      }
#endif

  /* `structure number' doesn't really mean `structure'; it means an index
     into a linker maintained set of saved locations which can be referenced
     again later.  */
  spnt->struc_numb = ++structure_count;
  VMS_Def_Struct (spnt->struc_numb);	/* remember where this type lives */
  /* define the simple scalar type */
  Local[Lpnt++] = 6 + strlen (symbol_name) + 2;	/* rec.len */
  Local[Lpnt++] = DST_K_TYPSPEC;	/* rec.typ==type specification */
  Local[Lpnt++] = strlen (symbol_name) + 2;
  Local[Lpnt++] = '_';			/* prefix name with "__" */
  Local[Lpnt++] = '_';
  for (p = symbol_name; *p; p++)
    Local[Lpnt++] = *p == ' ' ? '_' : *p;
  COPY_SHORT (&Local[Lpnt], 2);		/* typ.len==2 */
  Lpnt += 2;
  Local[Lpnt++] = DST_K_TS_ATOM;	/* typ.kind is simple type */
  Local[Lpnt++] = spnt->VMS_type;	/* typ.type */
  VMS_Store_Immediate_Data (Local, Lpnt, OBJ_S_C_DBG);
  Lpnt = 0;
#endif	/* SETUP_BASIC_TYPES */
}

/* This routine generates a symbol definition for a C symbol for the
   debugger.  It takes a psect and offset for global symbols; if psect < 0,
   then this is a local variable and the offset is relative to FP.  In this
   case it can be either a variable (Offset < 0) or a parameter (Offset > 0).  */

static void
VMS_DBG_record (struct VMS_DBG_Symbol *spnt, int Psect,
		int Offset, char *Name)
{
  char *Name_pnt;
  int len;
  int i = 0;

  /* If there are bad characters in name, convert them.  */
  Name_pnt = fix_name (Name);

  len = strlen (Name_pnt);
  if (Psect < 0)
    {
      /* This is a local variable, referenced to SP.  */
      Local[i++] = 7 + len;
      Local[i++] = spnt->VMS_type;
      Local[i++] = (Offset > 0) ? DBG_C_FUNCTION_PARAM : DBG_C_LOCAL_SYM;
      COPY_LONG (&Local[i], Offset);
      i += 4;
    }
  else
    {
      Local[i++] = 7 + len;
      Local[i++] = spnt->VMS_type;
      Local[i++] = DST_K_VALKIND_ADDR;
      VMS_Store_Immediate_Data (Local, i, OBJ_S_C_DBG);
      i = 0;
      VMS_Set_Data (Psect, Offset, OBJ_S_C_DBG, 0);
    }
  Local[i++] = len;
  while (*Name_pnt != '\0')
    Local[i++] = *Name_pnt++;
  VMS_Store_Immediate_Data (Local, i, OBJ_S_C_DBG);
  if (spnt->VMS_type == DBG_S_C_ADVANCED_TYPE)
    generate_suffix (spnt, 0);
}

/* This routine parses the stabs entries in order to make the definition
   for the debugger of local symbols and function parameters.  */

static void
VMS_local_stab_Parse (symbolS *sp)
{
  struct VMS_DBG_Symbol *spnt;
  char *pnt;
  char *pnt1;
  char *str;
  int dbx_type;

  dbx_type = 0;
  str = S_GET_NAME (sp);
  pnt = (char *) strchr (str, ':');
  if (!pnt)
    return;
  
  /* Save this for later, and skip colon.  */
  pnt1 = pnt++;

  /* Ignore static constants.  */
  if (*pnt == 'c')
    return;
  
  /* There is one little catch that we must be aware of.  Sometimes function
     parameters are optimized into registers, and the compiler, in its
     infiite wisdom outputs stabs records for *both*.  In general we want to
     use the register if it is present, so we must search the rest of the
     symbols for this function to see if this parameter is assigned to a
     register.  */
  {
    symbolS *sp1;
    char *str1;
    char *pnt2;

    if (*pnt == 'p')
      {
	for (sp1 = symbol_next (sp); sp1; sp1 = symbol_next (sp1))
	  {
	    if (!S_IS_DEBUG (sp1))
	      continue;
	    if (S_GET_RAW_TYPE (sp1) == N_FUN)
	      {
		pnt2 = (char *) strchr (S_GET_NAME (sp1), ':') + 1;
		if (*pnt2 == 'F' || *pnt2 == 'f')
		  break;
	      }
	    if (S_GET_RAW_TYPE (sp1) != N_RSYM)
	      continue;
	    str1 = S_GET_NAME (sp1);	/* and get the name */
	    pnt2 = str;
	    while (*pnt2 != ':')
	      {
		if (*pnt2 != *str1)
		  break;
		pnt2++;
		str1++;
	      }
	    if (*str1 == ':' && *pnt2 == ':')
	      return;	/* They are the same!  Let's skip this one.  */
	  }

	/* Skip p in case no register.  */
	pnt++;
      }
  }

  pnt = cvt_integer (pnt, &dbx_type);

  spnt = find_symbol (dbx_type);
  if (!spnt)
    /* Dunno what this is.  */
    return;
  
  *pnt1 = '\0';
  VMS_DBG_record (spnt, -1, S_GET_VALUE (sp), str);

  /* ...and restore the string.  */
  *pnt1 = ':';
}

/* This routine parses a stabs entry to find the information required
   to define a variable.  It is used for global and static variables.
   Basically we need to know the address of the symbol.  With older
   versions of the compiler, const symbols are treated differently, in
   that if they are global they are written into the text psect.  The
   global symbol entry for such a const is actually written as a program
   entry point (Yuk!!), so if we cannot find a symbol in the list of
   psects, we must search the entry points as well.  static consts are
   even harder, since they are never assigned a memory address.  The
   compiler passes a stab to tell us the value, but I am not sure what
   to do with it.  */

static void
VMS_stab_parse (symbolS *sp, int expected_type,
		int type1, int type2, int Text_Psect)
{
  char *pnt;
  char *pnt1;
  char *str;
  symbolS *sp1;
  struct VMS_DBG_Symbol *spnt;
  struct VMS_Symbol *vsp;
  int dbx_type;

  dbx_type = 0;
  str = S_GET_NAME (sp);

  pnt = (char *) strchr (str, ':');
  if (!pnt)
    /* No colon present.  */
    return;
  
  /* Save this for later. */
  pnt1 = pnt;
  pnt++;
  if (*pnt == expected_type)
    {
      pnt = cvt_integer (pnt + 1, &dbx_type);
      spnt = find_symbol (dbx_type);
      if (!spnt)
	return;		/*Dunno what this is*/
      /* Now we need to search the symbol table to find the psect and
         offset for this variable.  */
      *pnt1 = '\0';
      vsp = VMS_Symbols;
      while (vsp)
	{
	  pnt = S_GET_NAME (vsp->Symbol);
	  if (pnt && *pnt++ == '_'
	      /* make sure name is the same and symbol type matches */
	      && strcmp (pnt, str) == 0
	      && (S_GET_RAW_TYPE (vsp->Symbol) == type1
		  || S_GET_RAW_TYPE (vsp->Symbol) == type2))
	    break;
	  vsp = vsp->Next;
	}
      if (vsp)
	{
	  VMS_DBG_record (spnt, vsp->Psect_Index, vsp->Psect_Offset, str);
	  *pnt1 = ':';		/* and restore the string */
	  return;
	}
      /* The symbol was not in the symbol list, but it may be an
         "entry point" if it was a constant.  */
      for (sp1 = symbol_rootP; sp1; sp1 = symbol_next (sp1))
	{
	  /* Dispatch on STAB type.  */
	  if (S_IS_DEBUG (sp1) || (S_GET_TYPE (sp1) != N_TEXT))
	    continue;
	  pnt = S_GET_NAME (sp1);
	  if (*pnt == '_')
	    pnt++;
	  if (strcmp (pnt, str) == 0)
	    {
	      if (!gave_compiler_message && expected_type == 'G')
		{
		  char *long_const_msg = _("\
***Warning - the assembly code generated by the compiler has placed \n\
 global constant(s) in the text psect.  These will not be available to \n\
 other modules, since this is not the correct way to handle this. You \n\
 have two options: 1) get a patched compiler that does not put global \n\
 constants in the text psect, or 2) remove the 'const' keyword from \n\
 definitions of global variables in your source module(s).  Don't say \n\
 I didn't warn you! \n");

		  as_tsktsk (long_const_msg);
		  gave_compiler_message = 1;
		}
	      VMS_DBG_record (spnt,
			      Text_Psect,
			      S_GET_VALUE (sp1),
			      str);
	      *pnt1 = ':';
	      /* Fool assembler to not output this as a routine in the TBT.  */
	      pnt1 = S_GET_NAME (sp1);
	      *pnt1 = 'L';
	      S_SET_NAME (sp1, pnt1);
	      return;
	    }
	}
    }

  /* ...and restore the string.  */
  *pnt1 = ':';
}

/* Simpler interfaces into VMS_stab_parse().  */

static void
VMS_GSYM_Parse (symbolS *sp, int Text_Psect)
{				/* Global variables */
  VMS_stab_parse (sp, 'G', (N_UNDF | N_EXT), (N_DATA | N_EXT), Text_Psect);
}

static void
VMS_LCSYM_Parse (symbolS *sp, int Text_Psect)
{
  VMS_stab_parse (sp, 'S', N_BSS, -1, Text_Psect);
}

static void
VMS_STSYM_Parse (symbolS *sp, int Text_Psect)
{
  VMS_stab_parse (sp, 'S', N_DATA, -1, Text_Psect);
}

/* For register symbols, we must figure out what range of addresses
   within the psect are valid.  We will use the brackets in the stab
   directives to give us guidance as to the PC range that this variable
   is in scope.  I am still not completely comfortable with this but
   as I learn more, I seem to get a better handle on what is going on.
   Caveat Emptor.  */

static void
VMS_RSYM_Parse (symbolS *sp, symbolS *Current_Routine ATTRIBUTE_UNUSED,
		int Text_Psect)
{
  symbolS *symbolP;
  struct VMS_DBG_Symbol *spnt;
  char *pnt;
  char *pnt1;
  char *str;
  int dbx_type;
  int len;
  int i = 0;
  int bcnt = 0;
  int Min_Offset = -1;		/* min PC of validity */
  int Max_Offset = 0;		/* max PC of validity */

  for (symbolP = sp; symbolP; symbolP = symbol_next (symbolP))
    {
      /* Dispatch on STAB type.  */
      switch (S_GET_RAW_TYPE (symbolP))
	{
	case N_LBRAC:
	  if (bcnt++ == 0)
	    Min_Offset = S_GET_VALUE (symbolP);
	  break;
	case N_RBRAC:
	  if (--bcnt == 0)
	    Max_Offset = S_GET_VALUE (symbolP) - 1;
	  break;
	}
      if ((Min_Offset != -1) && (bcnt == 0))
	break;
      if (S_GET_RAW_TYPE (symbolP) == N_FUN)
	{
	  pnt = (char *) strchr (S_GET_NAME (symbolP), ':') + 1;
	  if (*pnt == 'F' || *pnt == 'f') break;
	}
    }

  /* Check to see that the addresses were defined.  If not, then there
     were no brackets in the function, and we must try to search for
     the next function.  Since functions can be in any order, we should
     search all of the symbol list to find the correct ending address.  */
  if (Min_Offset == -1)
    {
      int Max_Source_Offset;
      int This_Offset;

      Min_Offset = S_GET_VALUE (sp);
      Max_Source_Offset = Min_Offset;	/* just in case no N_SLINEs found */
      for (symbolP = symbol_rootP; symbolP; symbolP = symbol_next (symbolP))
	switch (S_GET_RAW_TYPE (symbolP))
	  {
	  case N_TEXT | N_EXT:
	    This_Offset = S_GET_VALUE (symbolP);
	    if (This_Offset > Min_Offset && This_Offset < Max_Offset)
	      Max_Offset = This_Offset;
	    break;
	  case N_SLINE:
	    This_Offset = S_GET_VALUE (symbolP);
	    if (This_Offset > Max_Source_Offset)
	      Max_Source_Offset = This_Offset;
	    break;
	  }
      /* If this is the last routine, then we use the PC of the last source
         line as a marker of the max PC for which this reg is valid.  */
      if (Max_Offset == 0x7fffffff)
	Max_Offset = Max_Source_Offset;
    }

  dbx_type = 0;
  str = S_GET_NAME (sp);
  if ((pnt = (char *) strchr (str, ':')) == 0)
    return;			/* no colon present */
  pnt1 = pnt;			/* save this for later*/
  pnt++;
  if (*pnt != 'r')
    return;
  pnt = cvt_integer (pnt + 1, &dbx_type);
  spnt = find_symbol (dbx_type);
  if (!spnt)
    return;			/*Dunno what this is yet*/
  *pnt1 = '\0';
  pnt = fix_name (S_GET_NAME (sp));	/* if there are bad characters in name, convert them */
  len = strlen (pnt);
  Local[i++] = 25 + len;
  Local[i++] = spnt->VMS_type;
  Local[i++] = DST_K_VFLAGS_TVS;	/* trailing value specified */
  COPY_LONG (&Local[i], 1 + len);	/* relative offset, beyond name */
  i += 4;
  Local[i++] = len;			/* name length (ascic prefix) */
  while (*pnt != '\0')
    Local[i++] = *pnt++;
  Local[i++] = DST_K_VS_FOLLOWS;	/* value specification follows */
  COPY_SHORT (&Local[i], 15);		/* length of rest of record */
  i += 2;
  Local[i++] = DST_K_VS_ALLOC_SPLIT;	/* split lifetime */
  Local[i++] = 1;			/* one binding follows */
  VMS_Store_Immediate_Data (Local, i, OBJ_S_C_DBG);
  i = 0;
  VMS_Set_Data (Text_Psect, Min_Offset, OBJ_S_C_DBG, 1);
  VMS_Set_Data (Text_Psect, Max_Offset, OBJ_S_C_DBG, 1);
  Local[i++] = DST_K_VALKIND_REG;		/* nested value spec */
  COPY_LONG (&Local[i], S_GET_VALUE (sp));
  i += 4;
  VMS_Store_Immediate_Data (Local, i, OBJ_S_C_DBG);
  *pnt1 = ':';
  if (spnt->VMS_type == DBG_S_C_ADVANCED_TYPE)
    generate_suffix (spnt, 0);
}

/* This function examines a structure definition, checking all of the elements
   to make sure that all of them are fully defined.  The only thing that we
   kick out are arrays of undefined structs, since we do not know how big
   they are.  All others we can handle with a normal forward reference.  */

static int
forward_reference (char *pnt)
{
  struct VMS_DBG_Symbol *spnt, *spnt1;
  int i;

  pnt = cvt_integer (pnt + 1, &i);
  if (*pnt == ';')
    return 0;			/* no forward references */
  do
    {
      pnt = (char *) strchr (pnt, ':');
      pnt = cvt_integer (pnt + 1, &i);
      spnt = find_symbol (i);
      while (spnt && (spnt->advanced == POINTER || spnt->advanced == ARRAY))
	{
	  spnt1 = find_symbol (spnt->type2);
	  if (spnt->advanced == ARRAY && !spnt1)
	    return 1;
	  spnt = spnt1;
	}
      pnt = cvt_integer (pnt + 1, &i);
      pnt = cvt_integer (pnt + 1, &i);
    } while (*++pnt != ';');
  return 0;			/* no forward references found */
}

/* Used to check a single element of a structure on the final pass.  */

static int
final_forward_reference (struct VMS_DBG_Symbol *spnt)
{
  struct VMS_DBG_Symbol *spnt1;

  while (spnt && (spnt->advanced == POINTER || spnt->advanced == ARRAY))
    {
      spnt1 = find_symbol (spnt->type2);
      if (spnt->advanced == ARRAY && !spnt1)
	return 1;
      spnt = spnt1;
    }
  return 0;	/* no forward references found */
}

/* This routine parses the stabs directives to find any definitions of dbx
   type numbers.  It makes a note of all of them, creating a structure
   element of VMS_DBG_Symbol that describes it.  This also generates the
   info for the debugger that describes the struct/union/enum, so that
   further references to these data types will be by number

   We have to process pointers right away, since there can be references
   to them later in the same stabs directive.  We cannot have forward
   references to pointers, (but we can have a forward reference to a
   pointer to a structure/enum/union) and this is why we process them
   immediately.  After we process the pointer, then we search for defs
   that are nested even deeper.

   8/15/92: We have to process arrays right away too, because there can
   be multiple references to identical array types in one structure
   definition, and only the first one has the definition.  */

static int
VMS_typedef_parse (char *str)
{
  char *pnt;
  char *pnt1;
  const char *pnt2;
  int i;
  int dtype;
  struct forward_ref *fpnt;
  int i1, i2, i3, len;
  struct VMS_DBG_Symbol *spnt;
  struct VMS_DBG_Symbol *spnt1;

  /* check for any nested def's */
  pnt = (char *) strchr (str + 1, '=');
  if (pnt && str[1] != '*' && (str[1] != 'a' || str[2] != 'r')
      && VMS_typedef_parse (pnt) == 1)
    return 1;
  /* now find dbx_type of entry */
  pnt = str - 1;
  if (*pnt == 'c')
    {				/* check for static constants */
      *str = '\0';		/* for now we ignore them */
      return 0;
    }
  while ((*pnt <= '9') && (*pnt >= '0'))
    pnt--;
  pnt++;			/* and get back to the number */
  cvt_integer (pnt, &i1);
  spnt = find_symbol (i1);
  /* First see if this has been defined already, due to forward reference.  */
  if (!spnt)
    {
      i2 = SYMTYP_HASH (i1);
      spnt = xmalloc (sizeof (struct VMS_DBG_Symbol));
      spnt->next = VMS_Symbol_type_list[i2];
      VMS_Symbol_type_list[i2] = spnt;
      spnt->dbx_type = i1;	/* and save the type */
      spnt->type2 = spnt->VMS_type = spnt->data_size = 0;
      spnt->index_min = spnt->index_max = spnt->struc_numb = 0;
    }

  /* For structs and unions, do a partial parse, otherwise we sometimes get
     circular definitions that are impossible to resolve.  We read enough
     info so that any reference to this type has enough info to be resolved.  */

  /* Point to character past equal sign.  */
  pnt = str + 1;

  if (*pnt >= '0' && *pnt <= '9')
    {
      if (type_check ("void"))
	{			/* this is the void symbol */
	  *str = '\0';
	  spnt->advanced = VOID;
	  return 0;
	}
      if (type_check ("unknown type"))
	{
	  *str = '\0';
	  spnt->advanced = UNKNOWN;
	  return 0;
	}
      pnt1 = cvt_integer (pnt, &i1);
      if (i1 != spnt->dbx_type)
	{
	  spnt->advanced = ALIAS;
	  spnt->type2 = i1;
	  strcpy (str, pnt1);
	  return 0;
	}
      as_tsktsk (_("debugginer output: %d is an unknown untyped variable."),
		 spnt->dbx_type);
      return 1;			/* do not know what this is */
    }

  /* Point to character past equal sign.  */
  pnt = str + 1;

  switch (*pnt)
    {
    case 'r':
      spnt->advanced = BASIC;
      if (type_check ("int"))
	{
	  spnt->VMS_type = DBG_S_C_SLINT;
	  spnt->data_size = 4;
	}
      else if (type_check ("long int"))
	{
	  spnt->VMS_type = DBG_S_C_SLINT;
	  spnt->data_size = 4;
	}
      else if (type_check ("unsigned int"))
	{
	  spnt->VMS_type = DBG_S_C_ULINT;
	  spnt->data_size = 4;
	}
      else if (type_check ("long unsigned int"))
	{
	  spnt->VMS_type = DBG_S_C_ULINT;
	  spnt->data_size = 4;
	}
      else if (type_check ("short int"))
	{
	  spnt->VMS_type = DBG_S_C_SSINT;
	  spnt->data_size = 2;
	}
      else if (type_check ("short unsigned int"))
	{
	  spnt->VMS_type = DBG_S_C_USINT;
	  spnt->data_size = 2;
	}
      else if (type_check ("char"))
	{
	  spnt->VMS_type = DBG_S_C_SCHAR;
	  spnt->data_size = 1;
	}
      else if (type_check ("signed char"))
	{
	  spnt->VMS_type = DBG_S_C_SCHAR;
	  spnt->data_size = 1;
	}
      else if (type_check ("unsigned char"))
	{
	  spnt->VMS_type = DBG_S_C_UCHAR;
	  spnt->data_size = 1;
	}
      else if (type_check ("float"))
	{
	  spnt->VMS_type = DBG_S_C_REAL4;
	  spnt->data_size = 4;
	}
      else if (type_check ("double"))
	{
	  spnt->VMS_type = vax_g_doubles ? DBG_S_C_REAL8_G : DBG_S_C_REAL8;
	  spnt->data_size = 8;
	}
      else if (type_check ("long double"))
	{
	  /* same as double, at least for now */
	  spnt->VMS_type = vax_g_doubles ? DBG_S_C_REAL8_G : DBG_S_C_REAL8;
	  spnt->data_size = 8;
	}
      else if (type_check ("long long int"))
	{
	  spnt->VMS_type = DBG_S_C_SQUAD;	/* signed quadword */
	  spnt->data_size = 8;
	}
      else if (type_check ("long long unsigned int"))
	{
	  spnt->VMS_type = DBG_S_C_UQUAD;	/* unsigned quadword */
	  spnt->data_size = 8;
	}
      else if (type_check ("complex float"))
	{
	  spnt->VMS_type = DBG_S_C_COMPLX4;
	  spnt->data_size = 2 * 4;
	}
      else if (type_check ("complex double"))
	{
	  spnt->VMS_type = vax_g_doubles ? DBG_S_C_COMPLX8_G : DBG_S_C_COMPLX8;
	  spnt->data_size = 2 * 8;
	}
      else if (type_check ("complex long double"))
	{
	  /* same as complex double, at least for now */
	  spnt->VMS_type = vax_g_doubles ? DBG_S_C_COMPLX8_G : DBG_S_C_COMPLX8;
	  spnt->data_size = 2 * 8;
	}
      else
	{
	  /* Shouldn't get here, but if we do, something
	     more substantial ought to be done...  */
	  spnt->VMS_type = 0;
	  spnt->data_size = 0;
	}
      if (spnt->VMS_type != 0)
	setup_basic_type (spnt);
      pnt1 = (char *) strchr (str, ';') + 1;
      break;
    case 's':
    case 'u':
      spnt->advanced = (*pnt == 's') ? STRUCT : UNION;
      spnt->VMS_type = DBG_S_C_ADVANCED_TYPE;
      pnt1 = cvt_integer (pnt + 1, &spnt->data_size);
      if (!final_pass && forward_reference (pnt))
	{
	  spnt->struc_numb = -1;
	  return 1;
	}
      spnt->struc_numb = ++structure_count;
      pnt1--;
      pnt = get_struct_name (str);
      VMS_Def_Struct (spnt->struc_numb);
      i = 0;
      for (fpnt = f_ref_root; fpnt; fpnt = fpnt->next)
	if (fpnt->dbx_type == spnt->dbx_type)
	  {
	    fpnt->resolved = 'Y';
	    VMS_Set_Struct (fpnt->struc_numb);
	    VMS_Store_Struct (spnt->struc_numb);
	    i++;
	  }
      if (i > 0)
	VMS_Set_Struct (spnt->struc_numb);
      i = 0;
      Local[i++] = 11 + strlen (pnt);
      Local[i++] = DBG_S_C_STRUCT_START;
      Local[i++] = DST_K_VFLAGS_NOVAL;	/* structure definition only */
      COPY_LONG (&Local[i], 0L);	/* hence value is unused */
      i += 4;
      Local[i++] = strlen (pnt);
      pnt2 = pnt;
      while (*pnt2 != '\0')
	Local[i++] = *pnt2++;
      i2 = spnt->data_size * 8;	/* number of bits */
      COPY_LONG (&Local[i], i2);
      i += 4;
      VMS_Store_Immediate_Data (Local, i, OBJ_S_C_DBG);
      i = 0;
      if (pnt != symbol_name)
	{
	  pnt += strlen (pnt);
	  /* Replace colon for later.  */
	  *pnt = ':';
	}

      while (*++pnt1 != ';')
	{
	  pnt = (char *) strchr (pnt1, ':');
	  *pnt = '\0';
	  pnt2 = pnt1;
	  pnt1 = cvt_integer (pnt + 1, &dtype);
	  pnt1 = cvt_integer (pnt1 + 1, &i2);
	  pnt1 = cvt_integer (pnt1 + 1, &i3);
	  spnt1 = find_symbol (dtype);
	  len = strlen (pnt2);
	  if (spnt1 && (spnt1->advanced == BASIC || spnt1->advanced == ENUM)
	      && ((i3 != spnt1->data_size * 8) || (i2 % 8 != 0)))
	    {			/* bitfield */
	      if (USE_BITSTRING_DESCRIPTOR (spnt1))
		{
		  /* This uses a type descriptor, which doesn't work if
		     the enclosing structure has been placed in a register.
		     Also, enum bitfields degenerate to simple integers.  */
		  int unsigned_type = (spnt1->VMS_type == DBG_S_C_ULINT
				    || spnt1->VMS_type == DBG_S_C_USINT
				    || spnt1->VMS_type == DBG_S_C_UCHAR
				    || spnt1->VMS_type == DBG_S_C_UQUAD
				    || spnt1->advanced == ENUM);
		  Apoint = 0;
		  fpush (19 + len, 1);
		  fpush (unsigned_type ? DBG_S_C_UBITU : DBG_S_C_SBITU, 1);
		  fpush (DST_K_VFLAGS_DSC, 1);	/* specified by descriptor */
		  fpush (1 + len, 4);	/* relative offset to descriptor */
		  fpush (len, 1);		/* length byte (ascic prefix) */
		  while (*pnt2 != '\0')	/* name bytes */
		    fpush (*pnt2++, 1);
		  fpush (i3, 2);	/* dsc length == size of bitfield */
					/* dsc type == un?signed bitfield */
		  fpush (unsigned_type ? DBG_S_C_UBITU : DBG_S_C_SBITU, 1);
		  fpush (DSC_K_CLASS_UBS, 1);	/* dsc class == unaligned bitstring */
		  fpush (0x00, 4);		/* dsc pointer == zeroes */
		  fpush (i2, 4);	/* start position */
		  VMS_Store_Immediate_Data (Asuffix, Apoint, OBJ_S_C_DBG);
		  Apoint = 0;
		}
	      else
		{
		  /* Use a "novel length" type specification, which works
		     right for register structures and for enum bitfields
		     but results in larger object modules.  */
		  Local[i++] = 7 + len;
		  Local[i++] = DBG_S_C_ADVANCED_TYPE;	/* type spec follows */
		  Local[i++] = DBG_S_C_STRUCT_ITEM;	/* value is a bit offset */
		  COPY_LONG (&Local[i], i2);		/* bit offset */
		  i += 4;
		  Local[i++] = strlen (pnt2);
		  while (*pnt2 != '\0')
		    Local[i++] = *pnt2++;
		  VMS_Store_Immediate_Data (Local, i, OBJ_S_C_DBG);
		  i = 0;
		  bitfield_suffix (spnt1, i3);
	     }
	    }
	  else /* Not a bitfield.  */
	    {
	      /* Check if this is a forward reference.  */
	      if (final_pass && final_forward_reference (spnt1))
		{
		  as_tsktsk (_("debugger output: structure element `%s' has undefined type"),
			   pnt2);
		  continue;
		}
	      Local[i++] = 7 + len;
	      Local[i++] = spnt1 ? spnt1->VMS_type : DBG_S_C_ADVANCED_TYPE;
	      Local[i++] = DBG_S_C_STRUCT_ITEM;
	      COPY_LONG (&Local[i], i2);		/* bit offset */
	      i += 4;
	      Local[i++] = strlen (pnt2);
	      while (*pnt2 != '\0')
		Local[i++] = *pnt2++;
	      VMS_Store_Immediate_Data (Local, i, OBJ_S_C_DBG);
	      i = 0;
	      if (!spnt1)
		generate_suffix (spnt1, dtype);
	      else if (spnt1->VMS_type == DBG_S_C_ADVANCED_TYPE)
		generate_suffix (spnt1, 0);
	    }
	}
      pnt1++;
      Local[i++] = 0x01;	/* length byte */
      Local[i++] = DBG_S_C_STRUCT_END;
      VMS_Store_Immediate_Data (Local, i, OBJ_S_C_DBG);
      i = 0;
      break;
    case 'e':
      spnt->advanced = ENUM;
      spnt->VMS_type = DBG_S_C_ADVANCED_TYPE;
      spnt->struc_numb = ++structure_count;
      spnt->data_size = 4;
      VMS_Def_Struct (spnt->struc_numb);
      i = 0;
      for (fpnt = f_ref_root; fpnt; fpnt = fpnt->next)
	if (fpnt->dbx_type == spnt->dbx_type)
	  {
	    fpnt->resolved = 'Y';
	    VMS_Set_Struct (fpnt->struc_numb);
	    VMS_Store_Struct (spnt->struc_numb);
	    i++;
	  }
      if (i > 0)
	VMS_Set_Struct (spnt->struc_numb);
      i = 0;
      len = strlen (symbol_name);
      Local[i++] = 3 + len;
      Local[i++] = DBG_S_C_ENUM_START;
      Local[i++] = 4 * 8;		/* enum values are 32 bits */
      Local[i++] = len;
      pnt2 = symbol_name;
      while (*pnt2 != '\0')
	Local[i++] = *pnt2++;
      VMS_Store_Immediate_Data (Local, i, OBJ_S_C_DBG);
      i = 0;
      while (*++pnt != ';')
	{
	  pnt1 = (char *) strchr (pnt, ':');
	  *pnt1++ = '\0';
	  pnt1 = cvt_integer (pnt1, &i1);
	  len = strlen (pnt);
	  Local[i++] = 7 + len;
	  Local[i++] = DBG_S_C_ENUM_ITEM;
	  Local[i++] = DST_K_VALKIND_LITERAL;
	  COPY_LONG (&Local[i], i1);
	  i += 4;
	  Local[i++] = len;
	  pnt2 = pnt;
	  while (*pnt != '\0')
	    Local[i++] = *pnt++;
	  VMS_Store_Immediate_Data (Local, i, OBJ_S_C_DBG);
	  i = 0;
	  pnt = pnt1;		/* Skip final semicolon */
	}
      Local[i++] = 0x01;	/* len byte */
      Local[i++] = DBG_S_C_ENUM_END;
      VMS_Store_Immediate_Data (Local, i, OBJ_S_C_DBG);
      i = 0;
      pnt1 = pnt + 1;
      break;
    case 'a':
      spnt->advanced = ARRAY;
      spnt->VMS_type = DBG_S_C_ADVANCED_TYPE;
      pnt = (char *) strchr (pnt, ';');
      if (!pnt)
	return 1;
      pnt1 = cvt_integer (pnt + 1, &spnt->index_min);
      pnt1 = cvt_integer (pnt1 + 1, &spnt->index_max);
      pnt1 = cvt_integer (pnt1 + 1, &spnt->type2);
      pnt = (char *) strchr (str + 1, '=');
      if (pnt && VMS_typedef_parse (pnt) == 1)
	return 1;
      break;
    case 'f':
      spnt->advanced = FUNCTION;
      spnt->VMS_type = DBG_S_C_FUNCTION_ADDR;
      /* this masquerades as a basic type*/
      spnt->data_size = 4;
      pnt1 = cvt_integer (pnt + 1, &spnt->type2);
      break;
    case '*':
      spnt->advanced = POINTER;
      spnt->VMS_type = DBG_S_C_ADVANCED_TYPE;
      spnt->data_size = 4;
      pnt1 = cvt_integer (pnt + 1, &spnt->type2);
      pnt = (char *) strchr (str + 1, '=');
      if (pnt && VMS_typedef_parse (pnt) == 1)
	return 1;
      break;
    default:
      spnt->advanced = UNKNOWN;
      spnt->VMS_type = 0;
      as_tsktsk (_("debugger output: %d is an unknown type of variable."),
		 spnt->dbx_type);
      return 1;			/* unable to decipher */
    }
  /* This removes the evidence of the definition so that the outer levels
     of parsing do not have to worry about it.  */
  pnt = str;
  while (*pnt1 != '\0')
    *pnt++ = *pnt1++;
  *pnt = '\0';
  return 0;
}

/* This is the root routine that parses the stabs entries for definitions.
   it calls VMS_typedef_parse, which can in turn call itself.  We need to
   be careful, since sometimes there are forward references to other symbol
   types, and these cannot be resolved until we have completed the parse.

   Also check and see if we are using continuation stabs, if we are, then
   paste together the entire contents of the stab before we pass it to
   VMS_typedef_parse.  */

static void
VMS_LSYM_Parse (void)
{
  char *pnt;
  char *pnt1;
  char *pnt2;
  char *str;
  char *parse_buffer = 0;
  char fixit[10];
  int incomplete, pass, incom1;
  struct forward_ref *fpnt;
  symbolS *sp;

  pass = 0;
  final_pass = 0;
  incomplete = 0;
  do
    {
      incom1 = incomplete;
      incomplete = 0;
      for (sp = symbol_rootP; sp; sp = symbol_next (sp))
	{
	  /* Deal with STAB symbols.  */
	  if (S_IS_DEBUG (sp))
	    {
	      /* Dispatch on STAB type.  */
	      switch (S_GET_RAW_TYPE (sp))
		{
		case N_GSYM:
		case N_LCSYM:
		case N_STSYM:
		case N_PSYM:
		case N_RSYM:
		case N_LSYM:
		case N_FUN:	/* Sometimes these contain typedefs. */
		  str = S_GET_NAME (sp);
		  symbol_name = str;
		  pnt = str + strlen (str) - 1;
		  if (*pnt == '?')  /* Continuation stab.  */
		    {
		      symbolS *spnext;
		      int tlen = 0;

		      spnext = sp;
		      do
			{
			  tlen += strlen (str) - 1;
			  spnext = symbol_next (spnext);
			  str = S_GET_NAME (spnext);
			  pnt = str + strlen (str) - 1;
			}
		      while (*pnt == '?');

		      tlen += strlen (str);
		      parse_buffer = xmalloc (tlen + 1);
		      strcpy (parse_buffer, S_GET_NAME (sp));
		      pnt2 = parse_buffer + strlen (parse_buffer) - 1;
		      *pnt2 = '\0';
		      spnext = sp;

		      do
			{
			  spnext = symbol_next (spnext);
			  str = S_GET_NAME (spnext);
			  strcat (pnt2, str);
			  pnt2 +=  strlen (str) - 1;
			  *str = '\0';  /* Erase this string  */
			  /* S_SET_NAME (spnext, str); */
			  if (*pnt2 != '?') break;
			  *pnt2 = '\0';
			}
		      while (1);

		      str = parse_buffer;
		      symbol_name = str;
		    }

		  if ((pnt = (char *) strchr (str, ':')) != 0)
		    {
		      *pnt = '\0';
		      pnt1 = pnt + 1;
		      if ((pnt2 = (char *) strchr (pnt1, '=')) != 0)
			incomplete += VMS_typedef_parse (pnt2);
		      if (parse_buffer)
			{
			  /*  At this point the parse buffer should just
			      contain name:nn.  If it does not, then we
			      are in real trouble.  Anyway, this is always
			      shorter than the original line.  */
			  pnt2 = S_GET_NAME (sp);
			  strcpy (pnt2, parse_buffer);
			  /* S_SET_NAME (sp, pnt2); */
			  free (parse_buffer),  parse_buffer = 0;
			}
		      /* Put back colon to restore dbx_type.  */
		      *pnt = ':';
		    }
		  break;
		}
	    }
	}
      pass++;

      /* Make one last pass, if needed, and define whatever we can
         that is left.  */
      if (final_pass == 0 && incomplete == incom1)
	{
	  final_pass = 1;
	  incom1++;	/* Force one last pass through.  */
	}
    }
  while (incomplete != 0 && incomplete != incom1);

  if (incomplete != 0)
    as_tsktsk (_("debugger output: Unable to resolve %d circular references."),
	       incomplete);

  fpnt = f_ref_root;
  symbol_name = "\0";
  while (fpnt)
    {
      if (fpnt->resolved != 'Y')
	{
	  if (find_symbol (fpnt->dbx_type))
	    {
	      as_tsktsk (_("debugger forward reference error, dbx type %d"),
			 fpnt->dbx_type);
	      break;
	    }
	  fixit[0] = 0;
	  sprintf (&fixit[1], "%d=s4;", fpnt->dbx_type);
	  pnt2 = (char *) strchr (&fixit[1], '=');
	  VMS_typedef_parse (pnt2);
	}
      fpnt = fpnt->next;
    }
}

static void
Define_Local_Symbols (symbolS *s0P, symbolS *s2P, symbolS *Current_Routine,
		      int Text_Psect)
{
  symbolS *s1P;		/* Each symbol from s0P .. s2P (exclusive).  */

  for (s1P = symbol_next (s0P); s1P != s2P; s1P = symbol_next (s1P))
    {
      if (!s1P)
	break;		/* and return */
      if (S_GET_RAW_TYPE (s1P) == N_FUN)
	{
	  char *pnt = (char *) strchr (S_GET_NAME (s1P), ':') + 1;
	  if (*pnt == 'F' || *pnt == 'f') break;
	}
      if (!S_IS_DEBUG (s1P))
	continue;
      /* Dispatch on STAB type.  */
      switch (S_GET_RAW_TYPE (s1P))
	{
	default:
	  /* Not left or right brace.  */
	  continue;

	case N_LSYM:
	case N_PSYM:
	  VMS_local_stab_Parse (s1P);
	  break;

	case N_RSYM:
	  VMS_RSYM_Parse (s1P, Current_Routine, Text_Psect);
	  break;
	}
    }
}

/* This function crawls the symbol chain searching for local symbols that
   need to be described to the debugger.  When we enter a new scope with
   a "{", it creates a new "block", which helps the debugger keep track
   of which scope we are currently in.  */

static symbolS *
Define_Routine (symbolS *s0P, int Level, symbolS *Current_Routine,
		int Text_Psect)
{
  symbolS *s1P;
  valueT Offset;
  int rcount = 0;

  for (s1P = symbol_next (s0P); s1P != 0; s1P = symbol_next (s1P))
    {
      if (S_GET_RAW_TYPE (s1P) == N_FUN)
	{
	  char *pnt = (char *) strchr (S_GET_NAME (s1P), ':') + 1;
	  if (*pnt == 'F' || *pnt == 'f') break;
	}
      if (!S_IS_DEBUG (s1P))
	continue;
      /* Dispatch on STAB type.  */
      switch (S_GET_RAW_TYPE (s1P))
	{
	default:
	  continue;

	case N_LBRAC:
	  if (Level != 0)
	    {
	      char str[10];
	      sprintf (str, "$%d", rcount++);
	      VMS_TBT_Block_Begin (s1P, Text_Psect, str);
	    }
	  /* Side-effect: fully resolve symbol.  */
	  Offset = S_GET_VALUE (s1P);
	  Define_Local_Symbols (s0P, s1P, Current_Routine, Text_Psect);
	  s1P = Define_Routine (s1P, Level + 1, Current_Routine, Text_Psect);
	  if (Level != 0)
	    VMS_TBT_Block_End (S_GET_VALUE (s1P) - Offset);
	  s0P = s1P;
	  break;

	case N_RBRAC:
	  return s1P;
	}
    }

  /* We end up here if there were no brackets in this function.
     Define everything.  */
  Define_Local_Symbols (s0P, (symbolS *)0, Current_Routine, Text_Psect);
  return s1P;
}


#ifndef VMS
#include <sys/types.h>
#include <time.h>
static void get_VMS_time_on_unix (char *);

/* Manufacture a VMS-like time string on a Unix based system.  */
static void
get_VMS_time_on_unix (char *Now)
{
  char *pnt;
  time_t timeb;

  time (&timeb);
  pnt = ctime (&timeb);
  pnt[3] = 0;
  pnt[7] = 0;
  pnt[10] = 0;
  pnt[16] = 0;
  pnt[24] = 0;
  sprintf (Now, "%2s-%3s-%s %s", pnt + 8, pnt + 4, pnt + 20, pnt + 11);
}
#endif /* not VMS */

/* Write the MHD (Module Header) records.  */

static void
Write_VMS_MHD_Records (void)
{
  const char *cp;
  char *cp1;
  int i;
#ifdef VMS
  struct { unsigned short len, mbz; char *ptr; } Descriptor;
#endif
  char Now[17+1];

  /* We are writing a module header record.  */
  Set_VMS_Object_File_Record (OBJ_S_C_HDR);
  /* MAIN MODULE HEADER RECORD.  */
  /* Store record type and header type.  */
  PUT_CHAR (OBJ_S_C_HDR);
  PUT_CHAR (MHD_S_C_MHD);
  /* Structure level is 0.  */
  PUT_CHAR (OBJ_S_C_STRLVL);
  /* Maximum record size is size of the object record buffer.  */
  PUT_SHORT (sizeof (Object_Record_Buffer));

  /* FIXME:  module name and version should be user
	     specifiable via `.ident' and/or `#pragma ident'.  */

  /* Get module name (the FILENAME part of the object file).  */
  cp = out_file_name;
  cp1 = Module_Name;
  while (*cp)
    {
      if (*cp == ']' || *cp == '>' || *cp == ':' || *cp == '/')
	{
	  cp1 = Module_Name;
	  cp++;
	  continue;
	}
      *cp1++ = TOUPPER (*cp++);
    }
  *cp1 = '\0';

  /* Limit it to 31 characters and store in the object record.  */
  while (--cp1 >= Module_Name)
    if (*cp1 == '.')
      *cp1 = '\0';
  if (strlen (Module_Name) > 31)
    {
      if (flag_hash_long_names)
	as_tsktsk (_("Module name truncated: %s\n"), Module_Name);
      Module_Name[31] = '\0';
    }
  PUT_COUNTED_STRING (Module_Name);
  /* Module Version is "V1.0".  */
  PUT_COUNTED_STRING ("V1.0");
  /* Creation time is "now" (17 chars of time string): "dd-MMM-yyyy hh:mm".  */
#ifndef VMS
  get_VMS_time_on_unix (Now);
#else /* VMS */
  Descriptor.len = sizeof Now - 1;
  Descriptor.mbz = 0;		/* type & class unspecified */
  Descriptor.ptr = Now;
  (void) sys$asctim ((unsigned short *)0, &Descriptor, (long *)0, 0);
#endif /* VMS */
  for (i = 0; i < 17; i++)
    PUT_CHAR (Now[i]);
  /* Patch time is "never" (17 zeros).  */
  for (i = 0; i < 17; i++)
    PUT_CHAR (0);
  /* Force this to be a separate output record.  */
  Flush_VMS_Object_Record_Buffer ();

  /* LANGUAGE PROCESSOR NAME.  */

  /* Store record type and header type.  */
  PUT_CHAR (OBJ_S_C_HDR);
  PUT_CHAR (MHD_S_C_LNM);

  /* Store language processor name and version (not a counted string!).
     This is normally supplied by the gcc driver for the command line
     which invokes gas.  If absent, we fall back to gas's version.  */
  
  cp = compiler_version_string;
  if (cp == 0)
    {
      cp = "GNU AS  V";
      while (*cp)
	PUT_CHAR (*cp++);
      cp = VERSION;
    }
  while (*cp >= ' ')
    PUT_CHAR (*cp++);
  /* Force this to be a separate output record.  */
  Flush_VMS_Object_Record_Buffer ();
}

/* Write the EOM (End Of Module) record.  */

static void
Write_VMS_EOM_Record (int Psect, valueT Offset)
{
  /* We are writing an end-of-module record
     (this assumes that the entry point will always be in a psect
     represented by a single byte, which is the case for code in
     Text_Psect==0).  */
  
  Set_VMS_Object_File_Record (OBJ_S_C_EOM);
  PUT_CHAR (OBJ_S_C_EOM);	/* Record type.  */
  PUT_CHAR (0);			/* Error severity level (we ignore it).  */
  /* Store the entry point, if it exists.  */
  if (Psect >= 0)
    {
      PUT_CHAR (Psect);
      PUT_LONG (Offset);
    }
  /* Flush the record; this will be our final output.  */
  Flush_VMS_Object_Record_Buffer ();
}


/* This hash routine borrowed from GNU-EMACS, and strengthened slightly
   ERY.  */

static int
hash_string (const char *ptr)
{
  const unsigned char *p = (unsigned char *) ptr;
  const unsigned char *end = p + strlen (ptr);
  unsigned char c;
  int hash = 0;

  while (p != end)
    {
      c = *p++;
      hash = ((hash << 3) + (hash << 15) + (hash >> 28) + c);
    }
  return hash;
}

/* Generate a Case-Hacked VMS symbol name (limited to 31 chars).  */

static void
VMS_Case_Hack_Symbol (const char *In, char *Out)
{
  long int init;
  long int result;
  char *pnt = 0;
  char *new_name;
  const char *old_name;
  int i;
  int destructor = 0;		/* Hack to allow for case sens in a destructor.  */
  int truncate = 0;
  int Case_Hack_Bits = 0;
  int Saw_Dollar = 0;
  static char Hex_Table[16] =
  {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};

  /* Kill any leading "_".  */
  if ((In[0] == '_') && ((In[1] > '9') || (In[1] < '0')))
    In++;

  new_name = Out;		/* Save this for later.  */

#if 0
  if ((In[0] == '_') && (In[1] == '$') && (In[2] == '_'))
    destructor = 1;
#endif

  /* We may need to truncate the symbol, save the hash for later.  */
  result = (strlen (In) > 23) ? hash_string (In) : 0;
  /* Is there a Psect Attribute to skip?  */
  if (HAS_PSECT_ATTRIBUTES (In))
    {
      /* Yes: Skip it.  */
      In += PSECT_ATTRIBUTES_STRING_LENGTH;
      while (*In)
	{
	  if ((In[0] == '$') && (In[1] == '$'))
	    {
	      In += 2;
	      break;
	    }
	  In++;
	}
    }

  old_name = In;
#if 0
  if (strlen (In) > 31 && flag_hash_long_names)
    as_tsktsk ("Symbol name truncated: %s\n", In);
#endif
  /* Do the case conversion.  */
  /* Maximum of 23 chars */
  i = 23;
  while (*In && (--i >= 0))
    {
      Case_Hack_Bits <<= 1;
      if (*In == '$')
	Saw_Dollar = 1;
      if ((destructor == 1) && (i == 21))
	Saw_Dollar = 0;

      switch (vms_name_mapping)
	{
	case 0:
	  if (ISUPPER (*In))
	    {
	      *Out++ = *In++;
	      Case_Hack_Bits |= 1;
	    }
	  else
	    *Out++ = TOUPPER (*In++);
	  break;

	case 3:
	  *Out++ = *In++;
	  break;

	case 2:
	  if (ISLOWER (*In))
	    *Out++ = *In++;
	  else
	    *Out++ = TOLOWER (*In++);
	  break;
	}
    }
  /* If we saw a dollar sign, we don't do case hacking.  */
  if (flag_no_hash_mixed_case || Saw_Dollar)
    Case_Hack_Bits = 0;

  /* If we have more than 23 characters and everything is lowercase
     we can insert the full 31 characters.  */
  if (*In)
    {
      /* We have more than 23 characters
         If we must add the case hack, then we have truncated the str.  */
      pnt = Out;
      truncate = 1;
      if (Case_Hack_Bits == 0)
	{
	  /* And so far they are all lower case:
	     Check up to 8 more characters
	     and ensure that they are lowercase.  */
	  for (i = 0; (In[i] != 0) && (i < 8); i++)
	    if (ISUPPER (In[i]) && !Saw_Dollar && !flag_no_hash_mixed_case)
	      break;

	  if (In[i] == 0)
	    truncate = 0;

	  if ((i == 8) || (In[i] == 0))
	    {
	      /* They are:  Copy up to 31 characters
	         to the output string.  */
	      i = 8;
	      while ((--i >= 0) && (*In))
		switch (vms_name_mapping){
		case 0: *Out++ = TOUPPER (*In++);
		  break;
		case 3: *Out++ = *In++;
		  break;
		case 2: *Out++ = TOLOWER (*In++);
		  break;
		}
	    }
	}
    }
  /* If there were any uppercase characters in the name we
     take on the case hacking string.  */

  /* Old behavior for regular GNU-C compiler.  */
  if (!flag_hash_long_names)
    truncate = 0;
  if ((Case_Hack_Bits != 0) || (truncate == 1))
    {
      if (truncate == 0)
	{
	  *Out++ = '_';
	  for (i = 0; i < 6; i++)
	    {
	      *Out++ = Hex_Table[Case_Hack_Bits & 0xf];
	      Case_Hack_Bits >>= 4;
	    }
	  *Out++ = 'X';
	}
      else
	{
	  Out = pnt;		/* Cut back to 23 characters maximum.  */
	  *Out++ = '_';
	  for (i = 0; i < 7; i++)
	    {
	      init = result & 0x01f;
	      *Out++ = (init < 10) ? ('0' + init) : ('A' + init - 10);
	      result = result >> 5;
	    }
	}
    }
  /* Done.  */
  *Out = 0;
  if (truncate == 1 && flag_hash_long_names && flag_show_after_trunc)
    as_tsktsk (_("Symbol %s replaced by %s\n"), old_name, new_name);
}


/* Scan a symbol name for a psect attribute specification.  */

#define GLOBALSYMBOL_BIT	0x10000
#define GLOBALVALUE_BIT		0x20000

static void
VMS_Modify_Psect_Attributes (const char *Name, int *Attribute_Pointer)
{
  int i;
  const char *cp;
  int Negate;
  static const struct
  {
    const char *Name;
    int Value;
  } Attributes[] =
  {
    {"PIC", GPS_S_M_PIC},
    {"LIB", GPS_S_M_LIB},
    {"OVR", GPS_S_M_OVR},
    {"REL", GPS_S_M_REL},
    {"GBL", GPS_S_M_GBL},
    {"SHR", GPS_S_M_SHR},
    {"EXE", GPS_S_M_EXE},
    {"RD", GPS_S_M_RD},
    {"WRT", GPS_S_M_WRT},
    {"VEC", GPS_S_M_VEC},
    {"GLOBALSYMBOL", GLOBALSYMBOL_BIT},
    {"GLOBALVALUE", GLOBALVALUE_BIT},
    {0, 0}
  };

  /* Kill leading "_".  */
  if (*Name == '_')
    Name++;
  /* Check for a PSECT attribute list.  */
  if (!HAS_PSECT_ATTRIBUTES (Name))
    return;
  /* Skip the attribute list indicator.  */
  Name += PSECT_ATTRIBUTES_STRING_LENGTH;
  /* Process the attributes ("_" separated, "$" terminated).  */
  while (*Name != '$')
    {
      /* Assume not negating.  */
      Negate = 0;
      /* Check for "NO".  */
      if ((Name[0] == 'N') && (Name[1] == 'O'))
	{
	  /* We are negating (and skip the NO).  */
	  Negate = 1;
	  Name += 2;
	}
      /* Find the token delimiter.  */
      cp = Name;
      while (*cp && (*cp != '_') && (*cp != '$'))
	cp++;
      /* Look for the token in the attribute list.  */
      for (i = 0; Attributes[i].Name; i++)
	{
	  /* If the strings match, set/clear the attr.  */
	  if (strncmp (Name, Attributes[i].Name, cp - Name) == 0)
	    {
	      /* Set or clear.  */
	      if (Negate)
		*Attribute_Pointer &=
		  ~Attributes[i].Value;
	      else
		*Attribute_Pointer |=
		  Attributes[i].Value;
	      /* Done.  */
	      break;
	    }
	}
      /* Now skip the attribute.  */
      Name = cp;
      if (*Name == '_')
	Name++;
    }
}


#define GBLSYM_REF 0
#define GBLSYM_DEF 1
#define GBLSYM_VAL 2
#define GBLSYM_LCL 4	/* not GBL after all...  */
#define GBLSYM_WEAK 8

/* Define a global symbol (or possibly a local one).  */

static void
VMS_Global_Symbol_Spec (const char *Name, int Psect_Number, int Psect_Offset, int Flags)
{
  char Local[32];

  /* We are writing a GSD record.  */
  Set_VMS_Object_File_Record (OBJ_S_C_GSD);

  /* If the buffer is empty we must insert the GSD record type.  */
  if (Object_Record_Offset == 0)
    PUT_CHAR (OBJ_S_C_GSD);

  /* We are writing a Global (or local) symbol definition subrecord.  */
  PUT_CHAR ((Flags & GBLSYM_LCL) != 0 ? GSD_S_C_LSY :
	    ((unsigned) Psect_Number <= 255) ? GSD_S_C_SYM : GSD_S_C_SYMW);

  /* Data type is undefined.  */
  PUT_CHAR (0);

  /* Switch on Definition/Reference.  */
  if ((Flags & GBLSYM_DEF) == 0)
    {
      /* Reference.  */
      PUT_SHORT (((Flags & GBLSYM_VAL) == 0) ? GSY_S_M_REL : 0);
      if ((Flags & GBLSYM_LCL) != 0)	/* local symbols have extra field */
	PUT_SHORT (Current_Environment);
    }
  else
    {
      int sym_flags;

      /* Definition
         [ assert (LSY_S_M_DEF == GSY_S_M_DEF && LSY_S_M_REL == GSY_S_M_REL); ].  */
      sym_flags = GSY_S_M_DEF;
      if (Flags & GBLSYM_WEAK)
	sym_flags |= GSY_S_M_WEAK;
      if ((Flags & GBLSYM_VAL) == 0)
	sym_flags |= GSY_S_M_REL;
      PUT_SHORT (sym_flags);
      if ((Flags & GBLSYM_LCL) != 0)	/* local symbols have extra field */
	PUT_SHORT (Current_Environment);

      /* Psect Number.  */
      if ((Flags & GBLSYM_LCL) == 0 && (unsigned) Psect_Number <= 255)
	PUT_CHAR (Psect_Number);
      else
	PUT_SHORT (Psect_Number);

      /* Offset.  */
      PUT_LONG (Psect_Offset);
    }

  /* Finally, the global symbol name.  */
  VMS_Case_Hack_Symbol (Name, Local);
  PUT_COUNTED_STRING (Local);

  /* Flush the buffer if it is more than 75% full.  */
  if (Object_Record_Offset > (sizeof (Object_Record_Buffer) * 3 / 4))
    Flush_VMS_Object_Record_Buffer ();
}

/* Define an environment to support local symbol references.
   This is just to mollify the linker; we don't actually do
   anything useful with it.  */

static void
VMS_Local_Environment_Setup (const char *Env_Name)
{
  /* We are writing a GSD record.  */
  Set_VMS_Object_File_Record (OBJ_S_C_GSD);
  /* If the buffer is empty we must insert the GSD record type.  */
  if (Object_Record_Offset == 0)
    PUT_CHAR (OBJ_S_C_GSD);
  /* We are writing an ENV subrecord.  */
  PUT_CHAR (GSD_S_C_ENV);

  ++Current_Environment;	/* index of environment being defined */

  /* ENV$W_FLAGS:  we are defining the next environment.  It's not nested.  */
  PUT_SHORT (ENV_S_M_DEF);
  /* ENV$W_ENVINDX:  index is always 0 for non-nested definitions.  */
  PUT_SHORT (0);

  /* ENV$B_NAMLNG + ENV$T_NAME:  environment name in ASCIC format.  */
  if (!Env_Name) Env_Name = "";
  PUT_COUNTED_STRING ((char *)Env_Name);

  /* Flush the buffer if it is more than 75% full.  */
  if (Object_Record_Offset > (sizeof (Object_Record_Buffer) * 3 / 4))
    Flush_VMS_Object_Record_Buffer ();
}


/* Define a psect.  */

static int
VMS_Psect_Spec (const char *Name, int Size, enum ps_type Type, struct VMS_Symbol *vsp)
{
  char Local[32];
  int Psect_Attributes;

  /* Generate the appropriate PSECT flags given the PSECT type.  */
  switch (Type)
    {
    case ps_TEXT:
      /* Text psects are PIC,noOVR,REL,noGBL,SHR,EXE,RD,noWRT.  */
      Psect_Attributes = (GPS_S_M_PIC|GPS_S_M_REL|GPS_S_M_SHR|GPS_S_M_EXE
			  |GPS_S_M_RD);
      break;
    case ps_DATA:
      /* Data psects are PIC,noOVR,REL,noGBL,noSHR,noEXE,RD,WRT.  */
      Psect_Attributes = (GPS_S_M_PIC|GPS_S_M_REL|GPS_S_M_RD|GPS_S_M_WRT);
      break;
    case ps_COMMON:
      /* Common block psects are:  PIC,OVR,REL,GBL,noSHR,noEXE,RD,WRT.  */
      Psect_Attributes = (GPS_S_M_PIC|GPS_S_M_OVR|GPS_S_M_REL|GPS_S_M_GBL
			  |GPS_S_M_RD|GPS_S_M_WRT);
      break;
    case ps_CONST:
      /* Const data psects are:  PIC,OVR,REL,GBL,noSHR,noEXE,RD,noWRT.  */
      Psect_Attributes = (GPS_S_M_PIC|GPS_S_M_OVR|GPS_S_M_REL|GPS_S_M_GBL
			  |GPS_S_M_RD);
      break;
    case ps_CTORS:
      /* Ctor psects are PIC,noOVR,REL,GBL,noSHR,noEXE,RD,noWRT.  */
      Psect_Attributes = (GPS_S_M_PIC|GPS_S_M_REL|GPS_S_M_GBL|GPS_S_M_RD);
      break;
    case ps_DTORS:
      /* Dtor psects are PIC,noOVR,REL,GBL,noSHR,noEXE,RD,noWRT.  */
      Psect_Attributes = (GPS_S_M_PIC|GPS_S_M_REL|GPS_S_M_GBL|GPS_S_M_RD);
      break;
    default:
      /* impossible */
      error (_("Unknown VMS psect type (%ld)"), (long) Type);
      break;
    }
  /* Modify the psect attributes according to any attribute string.  */
  if (vsp && S_GET_TYPE (vsp->Symbol) == N_ABS)
    Psect_Attributes |= GLOBALVALUE_BIT;
  else if (HAS_PSECT_ATTRIBUTES (Name))
    VMS_Modify_Psect_Attributes (Name, &Psect_Attributes);
  /* Check for globalref/def/val.  */
  if ((Psect_Attributes & GLOBALVALUE_BIT) != 0)
    {
      /* globalvalue symbols were generated before. This code
         prevents unsightly psect buildup, and makes sure that
         fixup references are emitted correctly.  */
      vsp->Psect_Index = -1;	/* to catch errors */
      S_SET_TYPE (vsp->Symbol, N_UNDF);		/* make refs work */
      return 1;			/* decrement psect counter */
    }

  if ((Psect_Attributes & GLOBALSYMBOL_BIT) != 0)
    {
      switch (S_GET_RAW_TYPE (vsp->Symbol))
	{
	case N_UNDF | N_EXT:
	  VMS_Global_Symbol_Spec (Name, vsp->Psect_Index,
				  vsp->Psect_Offset, GBLSYM_REF);
	  vsp->Psect_Index = -1;
	  S_SET_TYPE (vsp->Symbol, N_UNDF);
	  /* Return and indicate no psect.  */
	  return 1;
	  
	case N_DATA | N_EXT:
	  VMS_Global_Symbol_Spec (Name, vsp->Psect_Index,
				  vsp->Psect_Offset, GBLSYM_DEF);
	  /* In this case we still generate the psect. */
	  break;
	  
	default:
	  as_fatal (_("Globalsymbol attribute for symbol %s was unexpected."),
		    Name);
	  break;
	}
    }

  /* Clear out the globalref/def stuff.  */
  Psect_Attributes &= 0xffff;
  /* We are writing a GSD record.  */
  Set_VMS_Object_File_Record (OBJ_S_C_GSD);
  /* If the buffer is empty we must insert the GSD record type.  */
  if (Object_Record_Offset == 0)
    PUT_CHAR (OBJ_S_C_GSD);
  /* We are writing a PSECT definition subrecord.  */
  PUT_CHAR (GSD_S_C_PSC);
  /* Psects are always LONGWORD aligned.  */
  PUT_CHAR (2);
  /* Specify the psect attributes.  */
  PUT_SHORT (Psect_Attributes);
  /* Specify the allocation.  */
  PUT_LONG (Size);
  /* Finally, the psect name.  */
  VMS_Case_Hack_Symbol (Name, Local);
  PUT_COUNTED_STRING (Local);
  /* Flush the buffer if it is more than 75% full.  */
  if (Object_Record_Offset > (sizeof (Object_Record_Buffer) * 3 / 4))
    Flush_VMS_Object_Record_Buffer ();
  return 0;
}


/* Given the pointer to a symbol we calculate how big the data at the
   symbol is.  We do this by looking for the next symbol (local or global)
   which will indicate the start of another datum.  */

static offsetT
VMS_Initialized_Data_Size (symbolS *s0P, unsigned End_Of_Data)
{
  symbolS *s1P;
  valueT s0P_val = S_GET_VALUE (s0P), s1P_val,
	 nearest_val = (valueT) End_Of_Data;

  /* Find the nearest symbol what follows this one.  */
  for (s1P = symbol_rootP; s1P; s1P = symbol_next (s1P))
    {
      /* The data type must match.  */
      if (S_GET_TYPE (s1P) != N_DATA)
	continue;
      s1P_val = S_GET_VALUE (s1P);
      if (s1P_val > s0P_val && s1P_val < nearest_val)
	nearest_val = s1P_val;
    }
  /* Calculate its size.  */
  return (offsetT) (nearest_val - s0P_val);
}

/* Check symbol names for the Psect hack with a globalvalue, and then
   generate globalvalues for those that have it.  */

static void
VMS_Emit_Globalvalues (unsigned text_siz, unsigned data_siz,
		       char *Data_Segment)
{
  symbolS *sp;
  char *stripped_name, *Name;
  int Size;
  int Psect_Attributes;
  int globalvalue;
  int typ, abstyp;

  /* Scan the symbol table for globalvalues, and emit def/ref when
     required.  These will be caught again later and converted to
     N_UNDF.  */
  for (sp = symbol_rootP; sp; sp = sp->sy_next)
    {
      typ = S_GET_RAW_TYPE (sp);
      abstyp = ((typ & ~N_EXT) == N_ABS);
      /* See if this is something we want to look at.  */
      if (!abstyp &&
	  typ != (N_DATA | N_EXT) &&
	  typ != (N_UNDF | N_EXT))
	continue;
      /* See if this has globalvalue specification.  */
      Name = S_GET_NAME (sp);

      if (abstyp)
	{
	  stripped_name = 0;
	  Psect_Attributes = GLOBALVALUE_BIT;
	}
      else if (HAS_PSECT_ATTRIBUTES (Name))
	{
	  stripped_name = xmalloc (strlen (Name) + 1);
	  strcpy (stripped_name, Name);
	  Psect_Attributes = 0;
	  VMS_Modify_Psect_Attributes (stripped_name, &Psect_Attributes);
	}
      else
	continue;

      if ((Psect_Attributes & GLOBALVALUE_BIT) != 0)
	{
	  switch (typ)
	    {
	    case N_ABS:
	      /* Local symbol references will want
		 to have an environment defined.  */
	      if (Current_Environment < 0)
		VMS_Local_Environment_Setup (".N_ABS");
	      VMS_Global_Symbol_Spec (Name, 0,
				      S_GET_VALUE (sp),
				      GBLSYM_DEF|GBLSYM_VAL|GBLSYM_LCL);
	      break;
	    case N_ABS | N_EXT:
	      VMS_Global_Symbol_Spec (Name, 0,
				      S_GET_VALUE (sp),
				      GBLSYM_DEF|GBLSYM_VAL);
	      break;
	    case N_UNDF | N_EXT:
	      VMS_Global_Symbol_Spec (stripped_name, 0, 0, GBLSYM_VAL);
	      break;
	    case N_DATA | N_EXT:
	      Size = VMS_Initialized_Data_Size (sp, text_siz + data_siz);
	      if (Size > 4)
		error (_("Invalid data type for globalvalue"));
	      globalvalue = md_chars_to_number (Data_Segment +
		     S_GET_VALUE (sp) - text_siz , Size);
	      /* Three times for good luck.  The linker seems to get confused
	         if there are fewer than three */
	      VMS_Global_Symbol_Spec (stripped_name, 0, 0, GBLSYM_VAL);
	      VMS_Global_Symbol_Spec (stripped_name, 0, globalvalue,
				      GBLSYM_DEF|GBLSYM_VAL);
	      VMS_Global_Symbol_Spec (stripped_name, 0, globalvalue,
				      GBLSYM_DEF|GBLSYM_VAL);
	      break;
	    default:
	      as_warn (_("Invalid globalvalue of %s"), stripped_name);
	      break;
	    }
	}

      if (stripped_name)
	free (stripped_name);
    }

}


/* Define a procedure entry pt/mask.  */

static void
VMS_Procedure_Entry_Pt (char *Name, int Psect_Number, int Psect_Offset,
			int Entry_Mask)
{
  char Local[32];

  /* We are writing a GSD record.  */
  Set_VMS_Object_File_Record (OBJ_S_C_GSD);
  /* If the buffer is empty we must insert the GSD record type.  */
  if (Object_Record_Offset == 0)
    PUT_CHAR (OBJ_S_C_GSD);
  /* We are writing a Procedure Entry Pt/Mask subrecord.  */
  PUT_CHAR (((unsigned) Psect_Number <= 255) ? GSD_S_C_EPM : GSD_S_C_EPMW);
  /* Data type is undefined.  */
  PUT_CHAR (0);
  /* Flags = "RELOCATABLE" and "DEFINED".  */
  PUT_SHORT (GSY_S_M_DEF | GSY_S_M_REL);
  /* Psect Number.  */
  if ((unsigned) Psect_Number <= 255)
    PUT_CHAR (Psect_Number);
  else
    PUT_SHORT (Psect_Number);
  /* Offset.  */
  PUT_LONG (Psect_Offset);
  /* Entry mask.  */
  PUT_SHORT (Entry_Mask);
  /* Finally, the global symbol name.  */
  VMS_Case_Hack_Symbol (Name, Local);
  PUT_COUNTED_STRING (Local);
  /* Flush the buffer if it is more than 75% full.  */
  if (Object_Record_Offset > (sizeof (Object_Record_Buffer) * 3 / 4))
    Flush_VMS_Object_Record_Buffer ();
}


/* Set the current location counter to a particular Psect and Offset.  */

static void
VMS_Set_Psect (int Psect_Index, int Offset, int Record_Type)
{
  /* We are writing a "Record_Type" record.  */
  Set_VMS_Object_File_Record (Record_Type);
  /* If the buffer is empty we must insert the record type.  */
  if (Object_Record_Offset == 0)
    PUT_CHAR (Record_Type);
  /* Stack the Psect base + Offset.  */
  vms_tir_stack_psect (Psect_Index, Offset, 0);
  /* Set relocation base.  */
  PUT_CHAR (TIR_S_C_CTL_SETRB);
  /* Flush the buffer if it is more than 75% full.  */
  if (Object_Record_Offset > (sizeof (Object_Record_Buffer) * 3 / 4))
    Flush_VMS_Object_Record_Buffer ();
}


/* Store repeated immediate data in current Psect.  */

static void
VMS_Store_Repeated_Data (int Repeat_Count, char *Pointer, int Size,
			 int Record_Type)
{
  /* Ignore zero bytes/words/longwords.  */
  switch (Size)
    {
    case 4:
      if (Pointer[3] != 0 || Pointer[2] != 0) break;
      /* else FALLTHRU */
    case 2:
      if (Pointer[1] != 0) break;
      /* else FALLTHRU */
    case 1:
      if (Pointer[0] != 0) break;
      /* zero value */
      return;
    default:
      break;
    }
  /* If the data is too big for a TIR_S_C_STO_RIVB sub-record
     then we do it manually.  */
  if (Size > 255)
    {
      while (--Repeat_Count >= 0)
	VMS_Store_Immediate_Data (Pointer, Size, Record_Type);
      return;
    }
  /* We are writing a "Record_Type" record.  */
  Set_VMS_Object_File_Record (Record_Type);
  /* If the buffer is empty we must insert record type.  */
  if (Object_Record_Offset == 0)
    PUT_CHAR (Record_Type);
  /* Stack the repeat count.  */
  PUT_CHAR (TIR_S_C_STA_LW);
  PUT_LONG (Repeat_Count);
  /* And now the command and its data.  */
  PUT_CHAR (TIR_S_C_STO_RIVB);
  PUT_CHAR (Size);
  while (--Size >= 0)
    PUT_CHAR (*Pointer++);
  /* Flush the buffer if it is more than 75% full.  */
  if (Object_Record_Offset > (sizeof (Object_Record_Buffer) * 3 / 4))
    Flush_VMS_Object_Record_Buffer ();
}


/* Store a Position Independent Reference.  */

static void
VMS_Store_PIC_Symbol_Reference (symbolS *Symbol, int Offset, int PC_Relative,
				int Psect, int Psect_Offset, int Record_Type)
{
  struct VMS_Symbol *vsp = Symbol->sy_obj;
  char Local[32];
  int local_sym = 0;

  /* We are writing a "Record_Type" record.  */
  Set_VMS_Object_File_Record (Record_Type);
  /* If the buffer is empty we must insert record type.  */
  if (Object_Record_Offset == 0)
    PUT_CHAR (Record_Type);
  /* Set to the appropriate offset in the Psect.
     For a Code reference we need to fix the operand
     specifier as well, so back up 1 byte;
     for a Data reference we just store HERE.  */
  VMS_Set_Psect (Psect,
		 PC_Relative ? Psect_Offset - 1 : Psect_Offset,
		 Record_Type);
  /* Make sure we are still generating a "Record Type" record.  */
  if (Object_Record_Offset == 0)
    PUT_CHAR (Record_Type);
  /* Dispatch on symbol type (so we can stack its value).  */
  switch (S_GET_RAW_TYPE (Symbol))
    {
      /* Global symbol.  */
    case N_ABS:
      local_sym = 1;
      /*FALLTHRU*/
    case N_ABS | N_EXT:
#ifdef	NOT_VAX_11_C_COMPATIBLE
    case N_UNDF | N_EXT:
    case N_DATA | N_EXT:
#endif	/* NOT_VAX_11_C_COMPATIBLE */
    case N_UNDF:
    case N_TEXT | N_EXT:
      /* Get the symbol name (case hacked).  */
      VMS_Case_Hack_Symbol (S_GET_NAME (Symbol), Local);
      /* Stack the global symbol value.  */
      if (!local_sym)
	{
	  PUT_CHAR (TIR_S_C_STA_GBL);
	}
      else
	{
	  /* Local symbols have an extra field.  */
	  PUT_CHAR (TIR_S_C_STA_LSY);
	  PUT_SHORT (Current_Environment);
	}
      PUT_COUNTED_STRING (Local);
      if (Offset)
	{
	  /* Stack the longword offset.  */
	  PUT_CHAR (TIR_S_C_STA_LW);
	  PUT_LONG (Offset);
	  /* Add the two, leaving the result on the stack.  */
	  PUT_CHAR (TIR_S_C_OPR_ADD);
	}
      break;
      /* Uninitialized local data.  */
    case N_BSS:
      /* Stack the Psect (+offset).  */
      vms_tir_stack_psect (vsp->Psect_Index,
			   vsp->Psect_Offset + Offset,
			   0);
      break;
      /* Local text.  */
    case N_TEXT:
      /* Stack the Psect (+offset).  */
      vms_tir_stack_psect (vsp->Psect_Index,
			   S_GET_VALUE (Symbol) + Offset,
			   0);
      break;
      /* Initialized local or global data.  */
    case N_DATA:
#ifndef	NOT_VAX_11_C_COMPATIBLE
    case N_UNDF | N_EXT:
    case N_DATA | N_EXT:
#endif	/* NOT_VAX_11_C_COMPATIBLE */
      /* Stack the Psect (+offset).  */
      vms_tir_stack_psect (vsp->Psect_Index,
			   vsp->Psect_Offset + Offset,
			   0);
      break;
    }
  /* Store either a code or data reference.  */
  PUT_CHAR (PC_Relative ? TIR_S_C_STO_PICR : TIR_S_C_STO_PIDR);
  /* Flush the buffer if it is more than 75% full.  */
  if (Object_Record_Offset > (sizeof (Object_Record_Buffer) * 3 / 4))
    Flush_VMS_Object_Record_Buffer ();
}


/* Check in the text area for an indirect pc-relative reference
   and fix it up with addressing mode 0xff [PC indirect]

   THIS SHOULD BE REPLACED BY THE USE OF TIR_S_C_STO_PIRR IN THE
   PIC CODE GENERATING FIXUP ROUTINE.  */

static void
VMS_Fix_Indirect_Reference (int Text_Psect, addressT Offset,
			    fragS *fragP, fragS *text_frag_root)
{
  /* The addressing mode byte is 1 byte before the address.  */
  Offset--;
  /* Is it in THIS frag?  */
  if ((Offset < fragP->fr_address) ||
      (Offset >= (fragP->fr_address + fragP->fr_fix)))
    {
      /* We need to search for the fragment containing this
         Offset.  */
      for (fragP = text_frag_root; fragP; fragP = fragP->fr_next)
	{
	  if ((Offset >= fragP->fr_address) &&
	      (Offset < (fragP->fr_address + fragP->fr_fix)))
	    break;
	}
      /* If we couldn't find the frag, things are BAD!  */
      if (fragP == 0)
	error (_("Couldn't find fixup fragment when checking for indirect reference"));
    }
  /* Check for indirect PC relative addressing mode.  */
  if (fragP->fr_literal[Offset - fragP->fr_address] == (char) 0xff)
    {
      static char Address_Mode = (char) 0xff;

      /* Yes: Store the indirect mode back into the image
         to fix up the damage done by STO_PICR.  */
      VMS_Set_Psect (Text_Psect, Offset, OBJ_S_C_TIR);
      VMS_Store_Immediate_Data (&Address_Mode, 1, OBJ_S_C_TIR);
    }
}


/* If the procedure "main()" exists we have to add the instruction
   "jsb c$main_args" at the beginning to be compatible with VAX-11 "C".

   FIXME:  the macro name `HACK_DEC_C_STARTUP' should be renamed
	   to `HACK_VAXCRTL_STARTUP' because Digital's compiler
 	   named "DEC C" uses run-time library "DECC$SHR", but this
 	   startup code is for "VAXCRTL", the library for Digital's
 	   older "VAX C".  Also, this extra code isn't needed for
 	   supporting gcc because it already generates the VAXCRTL
 	   startup call when compiling main().  The reference to
 	   `flag_hash_long_names' looks very suspicious too;
 	   probably an old-style command line option was inadvertently
 	   overloaded here, then blindly converted into the new one.  */
void
vms_check_for_main (void)
{
  symbolS *symbolP;
#ifdef	HACK_DEC_C_STARTUP	/* JF */
  struct frchain *frchainP;
  fragS *fragP;
  fragS **prev_fragPP;
  struct fix *fixP;
  fragS *New_Frag;
  int i;
#endif	/* HACK_DEC_C_STARTUP */

  symbolP = (symbolS *) symbol_find ("_main");
  if (symbolP && !S_IS_DEBUG (symbolP) &&
      S_IS_EXTERNAL (symbolP) && (S_GET_TYPE (symbolP) == N_TEXT))
    {
#ifdef	HACK_DEC_C_STARTUP
      if (!flag_hash_long_names)
	{
#endif
	  /* Remember the entry point symbol.  */
	  Entry_Point_Symbol = symbolP;
#ifdef HACK_DEC_C_STARTUP
	}
      else
	{
	  /* Scan all the fragment chains for the one with "_main"
	     (Actually we know the fragment from the symbol, but we need
	     the previous fragment so we can change its pointer).  */
	  frchainP = frchain_root;
	  while (frchainP)
	    {
	      /* Scan all the fragments in this chain, remembering
	         the "previous fragment".  */
	      prev_fragPP = &frchainP->frch_root;
	      fragP = frchainP->frch_root;
	      while (fragP && (fragP != frchainP->frch_last))
		{
		  /* Is this the fragment ?  */
		  if (fragP == symbolP->sy_frag)
		    {
		      /* Yes: Modify the fragment by replacing
		         it with a new fragment.  */
		      New_Frag =
			xmalloc (sizeof (*New_Frag) +
				 fragP->fr_fix +
				 fragP->fr_var +
				 5);
		      /* The fragments are the same except
		        	that the "fixed" area is larger.  */
		      *New_Frag = *fragP;
		      New_Frag->fr_fix += 6;
		      /* Copy the literal data opening a hole
		         2 bytes after "_main" (i.e. just after
		         the entry mask).  Into which we place
		         the JSB instruction.  */
		      New_Frag->fr_literal[0] = fragP->fr_literal[0];
		      New_Frag->fr_literal[1] = fragP->fr_literal[1];
		      New_Frag->fr_literal[2] = 0x16;	/* Jsb */
		      New_Frag->fr_literal[3] = 0xef;
		      New_Frag->fr_literal[4] = 0;
		      New_Frag->fr_literal[5] = 0;
		      New_Frag->fr_literal[6] = 0;
		      New_Frag->fr_literal[7] = 0;
		      for (i = 2; i < fragP->fr_fix + fragP->fr_var; i++)
			New_Frag->fr_literal[i + 6] =
			  fragP->fr_literal[i];
		      /* Now replace the old fragment with the
		         newly generated one.  */
		      *prev_fragPP = New_Frag;
		      /* Remember the entry point symbol.  */
		      Entry_Point_Symbol = symbolP;
		      /* Scan the text area fixup structures
		         as offsets in the fragment may have changed.  */
		      for (fixP = text_fix_root; fixP; fixP = fixP->fx_next)
			{
			  /* Look for references to this fragment.  */
			  if (fixP->fx_frag == fragP)
			    {
			      /* Change the fragment pointer.  */
			      fixP->fx_frag = New_Frag;
			      /* If the offset is after	the entry mask we need
			         to account for the JSB	instruction we just
			         inserted.  */
			      if (fixP->fx_where >= 2)
				fixP->fx_where += 6;
			    }
			}
		      /* Scan the symbols as offsets in the
		        fragment may have changed.  */
		      for (symbolP = symbol_rootP;
			   symbolP;
			   symbolP = symbol_next (symbolP))
			{
			  /* Look for references to this fragment.  */
			  if (symbolP->sy_frag == fragP)
			    {
			      /* Change the fragment pointer.  */
			      symbolP->sy_frag = New_Frag;
			      /* If the offset is after	the entry mask we need
			         to account for the JSB	instruction we just
			         inserted.  */
			      if (S_GET_VALUE (symbolP) >= 2)
				S_SET_VALUE (symbolP,
					     S_GET_VALUE (symbolP) + 6);
			    }
			}
		      /*  Make a symbol reference to "_c$main_args" so we
			  can get its address inserted into the	JSB
			  instruction.  */
		      symbolP = xmalloc (sizeof (*symbolP));
		      S_SET_NAME (symbolP, "_C$MAIN_ARGS");
		      S_SET_TYPE (symbolP, N_UNDF);
		      S_SET_OTHER (symbolP, 0);
		      S_SET_DESC (symbolP, 0);
		      S_SET_VALUE (symbolP, 0);
		      symbolP->sy_name_offset = 0;
		      symbolP->sy_number = 0;
		      symbolP->sy_obj = 0;
		      symbolP->sy_frag = New_Frag;
		      symbolP->sy_resolved = 0;
		      symbolP->sy_resolving = 0;
		      /* This actually inserts at the beginning of the list.  */
		      symbol_append (symbol_rootP, symbolP,
				     &symbol_rootP, &symbol_lastP);

		      symbol_rootP = symbolP;
		      /* Generate a text fixup structure
		         to get "_c$main_args" stored into the
		         JSB instruction.  */
		      fixP = xmalloc (sizeof (*fixP));
		      fixP->fx_frag = New_Frag;
		      fixP->fx_where = 4;
		      fixP->fx_addsy = symbolP;
		      fixP->fx_subsy = 0;
		      fixP->fx_offset = 0;
		      fixP->fx_size = 4;
		      fixP->fx_pcrel = 1;
		      fixP->fx_next = text_fix_root;
		      text_fix_root = fixP;
		      /* Now make sure we exit from the loop.  */
		      frchainP = 0;
		      break;
		    }
		  /* Try the next fragment.  */
		  prev_fragPP = &fragP->fr_next;
		  fragP = fragP->fr_next;
		}
	      /* Try the next fragment chain.  */
	      if (frchainP)
		frchainP = frchainP->frch_next;
	    }
	}
#endif /* HACK_DEC_C_STARTUP */
    }
}


/* Beginning of vms_write_object_file().  */

static
struct vms_obj_state
{
  /* Next program section index to use.  */
  int	psect_number;

  /* Psect index for code.  Always ends up #0.  */
  int	text_psect;

  /* Psect index for initialized static variables.  */
  int	data_psect;

  /* Psect index for uninitialized static variables.  */
  int	bss_psect;

  /* Psect index for static constructors.  */
  int	ctors_psect;

  /* Psect index for static destructors.  */
  int	dtors_psect;

  /* Number of bytes used for local symbol data.  */
  int	local_initd_data_size;

  /* Dynamic buffer for initialized data.  */
  char *data_segment;

} vms_obj_state;

#define Psect_Number		vms_obj_state.psect_number
#define Text_Psect		vms_obj_state.text_psect
#define Data_Psect		vms_obj_state.data_psect
#define Bss_Psect		vms_obj_state.bss_psect
#define Ctors_Psect		vms_obj_state.ctors_psect
#define Dtors_Psect		vms_obj_state.dtors_psect
#define Local_Initd_Data_Size	vms_obj_state.local_initd_data_size
#define Data_Segment		vms_obj_state.data_segment

#define IS_GXX_VTABLE(symP) (strncmp (S_GET_NAME (symP), "__vt.", 5) == 0)
#define IS_GXX_XTOR(symP) (strncmp (S_GET_NAME (symP), "__GLOBAL_.", 10) == 0)
#define XTOR_SIZE 4


/* Perform text segment fixups.  */

static void
vms_fixup_text_section (unsigned text_siz ATTRIBUTE_UNUSED,
			struct frag *text_frag_root,
			struct frag *data_frag_root)
{
  fragS *fragP;
  struct fix *fixP;
  offsetT dif;

  /* Scan the text fragments.  */
  for (fragP = text_frag_root; fragP; fragP = fragP->fr_next)
    {
      /* Stop if we get to the data fragments.  */
      if (fragP == data_frag_root)
	break;
      /* Ignore fragments with no data.  */
      if ((fragP->fr_fix == 0) && (fragP->fr_var == 0))
	continue;
      /* Go to the appropriate offset in the Text Psect.  */
      VMS_Set_Psect (Text_Psect, fragP->fr_address, OBJ_S_C_TIR);
      /* Store the "fixed" part.  */
      if (fragP->fr_fix)
	VMS_Store_Immediate_Data (fragP->fr_literal,
				  fragP->fr_fix,
				  OBJ_S_C_TIR);
      /* Store the "variable" part.  */
      if (fragP->fr_var && fragP->fr_offset)
	VMS_Store_Repeated_Data (fragP->fr_offset,
				 fragP->fr_literal + fragP->fr_fix,
				 fragP->fr_var,
				 OBJ_S_C_TIR);
    }

  /* Now we go through the text segment fixups and generate
     TIR records to fix up addresses within the Text Psect.  */
  for (fixP = text_fix_root; fixP; fixP = fixP->fx_next)
    {
      /* We DO handle the case of "Symbol - Symbol" as
	 long as it is in the same segment.  */
      if (fixP->fx_subsy && fixP->fx_addsy)
	{
	  /* They need to be in the same segment.  */
	  if (S_GET_RAW_TYPE (fixP->fx_subsy) !=
	      S_GET_RAW_TYPE (fixP->fx_addsy))
	    error (_("Fixup data addsy and subsy don't have the same type"));
	  /* And they need to be in one that we can check the psect on.  */
	  if ((S_GET_TYPE (fixP->fx_addsy) != N_DATA) &&
		    (S_GET_TYPE (fixP->fx_addsy) != N_TEXT))
	    error (_("Fixup data addsy and subsy don't have an appropriate type"));
	  /* This had better not be PC relative!  */
	  if (fixP->fx_pcrel)
	    error (_("Fixup data is erroneously \"pcrel\""));
	  /* Subtract their values to get the difference.  */
	  dif = S_GET_VALUE (fixP->fx_addsy) - S_GET_VALUE (fixP->fx_subsy);
	  md_number_to_chars (Local, (valueT)dif, fixP->fx_size);
	  /* Now generate the fixup object records;
	     set the psect and store the data.  */
	  VMS_Set_Psect (Text_Psect,
			 fixP->fx_where + fixP->fx_frag->fr_address,
			 OBJ_S_C_TIR);
	  VMS_Store_Immediate_Data (Local,
				    fixP->fx_size,
				    OBJ_S_C_TIR);
	  continue;
	}
      /* Size will HAVE to be "long".  */
      if (fixP->fx_size != 4)
	error (_("Fixup datum is not a longword"));
      /* Symbol must be "added" (if it is ever
	 subtracted we can fix this assumption).  */
      if (fixP->fx_addsy == 0)
	error (_("Fixup datum is not \"fixP->fx_addsy\""));
      /* Store the symbol value in a PIC fashion.  */
      VMS_Store_PIC_Symbol_Reference (fixP->fx_addsy,
				      fixP->fx_offset,
				      fixP->fx_pcrel,
				      Text_Psect,
				    fixP->fx_where + fixP->fx_frag->fr_address,
				      OBJ_S_C_TIR);
	  /* Check for indirect address reference, which has to be fixed up
	     (as the linker will screw it up with TIR_S_C_STO_PICR).  */
      if (fixP->fx_pcrel)
	VMS_Fix_Indirect_Reference (Text_Psect,
				    fixP->fx_where + fixP->fx_frag->fr_address,
				    fixP->fx_frag,
				    text_frag_root);
    }
}


/* Create a buffer holding the data segment.  */

static void
synthesize_data_segment (unsigned data_siz, unsigned text_siz,
			 struct frag *data_frag_root)
{
  fragS *fragP;
  char *fill_literal;
  long fill_size, count, i;

  /* Allocate the data segment.  */
  Data_Segment = xmalloc (data_siz);
  
  /* Run through the data fragments, filling in the segment.  */
  for (fragP = data_frag_root; fragP; fragP = fragP->fr_next)
    {
      i = fragP->fr_address - text_siz;
      if (fragP->fr_fix)
	memcpy (Data_Segment + i, fragP->fr_literal, fragP->fr_fix);
      i += fragP->fr_fix;

      if ((fill_size = fragP->fr_var) != 0)
	{
	  fill_literal = fragP->fr_literal + fragP->fr_fix;
	  for (count = fragP->fr_offset; count; count--)
	    {
	      memcpy (Data_Segment + i, fill_literal, fill_size);
	      i += fill_size;
	    }
	}
    }
}

/* Perform data segment fixups.  */

static void
vms_fixup_data_section (unsigned int data_siz ATTRIBUTE_UNUSED,
			unsigned int text_siz)
{
  struct VMS_Symbol *vsp;
  struct fix *fixP;
  symbolS *sp;
  addressT fr_address;
  offsetT dif;
  valueT val;

  /* Run through all the data symbols and store the data.  */
  for (vsp = VMS_Symbols; vsp; vsp = vsp->Next)
    {
      /* Ignore anything other than data symbols.  */
      if (S_GET_TYPE (vsp->Symbol) != N_DATA)
	continue;
      /* Set the Psect + Offset.  */
      VMS_Set_Psect (vsp->Psect_Index,
		       vsp->Psect_Offset,
		       OBJ_S_C_TIR);
      /* Store the data.  */
      val = S_GET_VALUE (vsp->Symbol);
      VMS_Store_Immediate_Data (Data_Segment + val - text_siz,
				vsp->Size,
				OBJ_S_C_TIR);
    }			/* N_DATA symbol loop */

  /* Now we go through the data segment fixups and generate
     TIR records to fix up addresses within the Data Psects.  */
  for (fixP = data_fix_root; fixP; fixP = fixP->fx_next)
    {
      /* Find the symbol for the containing datum.  */
      for (vsp = VMS_Symbols; vsp; vsp = vsp->Next)
	{
	  /* Only bother with Data symbols.  */
	  sp = vsp->Symbol;
	  if (S_GET_TYPE (sp) != N_DATA)
	    continue;
	  /* Ignore symbol if After fixup.  */
	  val = S_GET_VALUE (sp);
	  fr_address = fixP->fx_frag->fr_address;
	  if (val > fixP->fx_where + fr_address)
	    continue;
	  /* See if the datum is here.  */
	  if (val + vsp->Size <= fixP->fx_where + fr_address)
	    continue;
	  /* We DO handle the case of "Symbol - Symbol" as
	     long as it is in the same segment.  */
	  if (fixP->fx_subsy && fixP->fx_addsy)
	    {
	      /* They need to be in the same segment.  */
	      if (S_GET_RAW_TYPE (fixP->fx_subsy) !=
		  S_GET_RAW_TYPE (fixP->fx_addsy))
		error (_("Fixup data addsy and subsy don't have the same type"));
	      /* And they need to be in one that we can check the psect on.  */
	      if ((S_GET_TYPE (fixP->fx_addsy) != N_DATA) &&
		  (S_GET_TYPE (fixP->fx_addsy) != N_TEXT))
		error (_("Fixup data addsy and subsy don't have an appropriate type"));
	      /* This had better not be PC relative!  */
	      if (fixP->fx_pcrel)
		error (_("Fixup data is erroneously \"pcrel\""));
	      /* Subtract their values to get the difference.  */
	      dif = S_GET_VALUE (fixP->fx_addsy) - S_GET_VALUE (fixP->fx_subsy);
	      md_number_to_chars (Local, (valueT)dif, fixP->fx_size);
	      /* Now generate the fixup object records;
	         set the psect and store the data.  */
	      VMS_Set_Psect (vsp->Psect_Index,
			     fr_address + fixP->fx_where
				 - val + vsp->Psect_Offset,
			     OBJ_S_C_TIR);
	      VMS_Store_Immediate_Data (Local,
					fixP->fx_size,
					OBJ_S_C_TIR);
		  break;	/* done with this fixup */
		}
	  /* Size will HAVE to be "long".  */
	  if (fixP->fx_size != 4)
	    error (_("Fixup datum is not a longword"));
	  /* Symbol must be "added" (if it is ever
	     subtracted we can fix this assumption).  */
	  if (fixP->fx_addsy == 0)
	    error (_("Fixup datum is not \"fixP->fx_addsy\""));
	  /* Store the symbol value in a PIC fashion.  */
	  VMS_Store_PIC_Symbol_Reference (fixP->fx_addsy,
					  fixP->fx_offset,
					  fixP->fx_pcrel,
					  vsp->Psect_Index,
					  fr_address + fixP->fx_where
					      - val + vsp->Psect_Offset,
					  OBJ_S_C_TIR);
	  /* Done with this fixup.  */
	  break;
	}
    }
}

/* Perform ctors/dtors segment fixups.  */

static void
vms_fixup_xtors_section (struct VMS_Symbol *symbols,
			 int sect_no ATTRIBUTE_UNUSED)
{
  struct VMS_Symbol *vsp;

  /* Run through all the symbols and store the data.  */
  for (vsp = symbols; vsp; vsp = vsp->Next)
    {
      symbolS *sp;

      /* Set relocation base.  */
      VMS_Set_Psect (vsp->Psect_Index, vsp->Psect_Offset, OBJ_S_C_TIR);

      sp = vsp->Symbol;
      /* Stack the Psect base with its offset.  */
      VMS_Set_Data (Text_Psect, S_GET_VALUE (sp), OBJ_S_C_TIR, 0);
    }
  /* Flush the buffer if it is more than 75% full.  */
  if (Object_Record_Offset > (sizeof (Object_Record_Buffer) * 3 / 4))
    Flush_VMS_Object_Record_Buffer ();
}


/* Define symbols for the linker.  */

static void
global_symbol_directory (unsigned text_siz, unsigned data_siz)
{
  fragS *fragP;
  symbolS *sp;
  struct VMS_Symbol *vsp;
  int Globalref, define_as_global_symbol;

#if 0
  /* The g++ compiler does not write out external references to
     vtables correctly.  Check for this and holler if we see it
     happening.  If that compiler bug is ever fixed we can remove
     this.

     (Jun'95: gcc 2.7.0's cc1plus still exhibits this behavior.)

     This was reportedly fixed as of June 2, 1998.   */

  for (sp = symbol_rootP; sp; sp = symbol_next (sp))
    if (S_GET_RAW_TYPE (sp) == N_UNDF && IS_GXX_VTABLE (sp))
      {
	S_SET_TYPE (sp, N_UNDF | N_EXT);
	S_SET_OTHER (sp, 1);
	as_warn (_("g++ wrote an extern reference to `%s' as a routine.\nI will fix it, but I hope that it was note really a routine."),
		 S_GET_NAME (sp));
      }
#endif

  /* Now scan the symbols and emit the appropriate GSD records.  */
  for (sp = symbol_rootP; sp; sp = symbol_next (sp))
    {
      define_as_global_symbol = 0;
      vsp = 0;
      /* Dispatch on symbol type.  */
      switch (S_GET_RAW_TYPE (sp))
	{

	/* Global uninitialized data.  */
	case N_UNDF | N_EXT:
	  /* Make a VMS data symbol entry.  */
	  vsp = xmalloc (sizeof *vsp);
	  vsp->Symbol = sp;
	  vsp->Size = S_GET_VALUE (sp);
	  vsp->Psect_Index = Psect_Number++;
	  vsp->Psect_Offset = 0;
	  vsp->Next = VMS_Symbols;
	  VMS_Symbols = vsp;
	  sp->sy_obj = vsp;
	  /* Make the psect for this data.  */
	  Globalref = VMS_Psect_Spec (S_GET_NAME (sp),
				      vsp->Size,
				      S_GET_OTHER (sp) ? ps_CONST : ps_COMMON,
				      vsp);
	  if (Globalref)
	    Psect_Number--;
#ifdef	NOT_VAX_11_C_COMPATIBLE
	  define_as_global_symbol = 1;
#else
	  /* See if this is an external vtable.  We want to help the
	     linker find these things in libraries, so we make a symbol
	     reference.  This is not compatible with VAX-C usage for
	     variables, but since vtables are only used internally by
	     g++, we can get away with this hack.  */
	  define_as_global_symbol = IS_GXX_VTABLE (sp);
#endif
	  break;

	/* Local uninitialized data.  */
	case N_BSS:
	  /* Make a VMS data symbol entry.  */
	  vsp = xmalloc (sizeof *vsp);
	  vsp->Symbol = sp;
	  vsp->Size = 0;
	  vsp->Psect_Index = Bss_Psect;
	  vsp->Psect_Offset = S_GET_VALUE (sp) - bss_address_frag.fr_address;
	  vsp->Next = VMS_Symbols;
	  VMS_Symbols = vsp;
	  sp->sy_obj = vsp;
	  break;

	/* Global initialized data.  */
	case N_DATA | N_EXT:
	  /* Make a VMS data symbol entry.  */
	  vsp = xmalloc (sizeof *vsp);
	  vsp->Symbol = sp;
	  vsp->Size = VMS_Initialized_Data_Size (sp, text_siz + data_siz);
	  vsp->Psect_Index = Psect_Number++;
	  vsp->Psect_Offset = 0;
	  vsp->Next = VMS_Symbols;
	  VMS_Symbols = vsp;
	  sp->sy_obj = vsp;
	  /* Make its psect.  */
	  Globalref = VMS_Psect_Spec (S_GET_NAME (sp),
				      vsp->Size,
				      S_GET_OTHER (sp) ? ps_CONST : ps_COMMON,
				      vsp);
	  if (Globalref)
	    Psect_Number--;
#ifdef	NOT_VAX_11_C_COMPATIBLE
	  define_as_global_symbol = 1;
#else
	  /* See N_UNDF|N_EXT above for explanation.  */
	  define_as_global_symbol = IS_GXX_VTABLE (sp);
#endif
	  break;

	/* Local initialized data.  */
	case N_DATA:
	  {
	    char *sym_name = S_GET_NAME (sp);

	    /* Always suppress local numeric labels.  */
	    if (sym_name && strcmp (sym_name, FAKE_LABEL_NAME) == 0)
	      break;

	    /* Make a VMS data symbol entry.  */
	    vsp = xmalloc (sizeof *vsp);
	    vsp->Symbol = sp;
	    vsp->Size = VMS_Initialized_Data_Size (sp, text_siz + data_siz);
	    vsp->Psect_Index = Data_Psect;
	    vsp->Psect_Offset = Local_Initd_Data_Size;
	    Local_Initd_Data_Size += vsp->Size;
	    vsp->Next = VMS_Symbols;
	    VMS_Symbols = vsp;
	    sp->sy_obj = vsp;
	  }
	  break;

	/* Global Text definition.  */
	case N_TEXT | N_EXT:
	  {

	    if (IS_GXX_XTOR (sp))
	      {
		vsp = xmalloc (sizeof *vsp);
		vsp->Symbol = sp;
		vsp->Size = XTOR_SIZE;
		sp->sy_obj = vsp;
		switch ((S_GET_NAME (sp))[10])
		  {
		    case 'I':
		      vsp->Psect_Index = Ctors_Psect;
		      vsp->Psect_Offset = (Ctors_Symbols==0)?0:(Ctors_Symbols->Psect_Offset+XTOR_SIZE);
		      vsp->Next = Ctors_Symbols;
		      Ctors_Symbols = vsp;
		      break;
		    case 'D':
		      vsp->Psect_Index = Dtors_Psect;
		      vsp->Psect_Offset = (Dtors_Symbols==0)?0:(Dtors_Symbols->Psect_Offset+XTOR_SIZE);
		      vsp->Next = Dtors_Symbols;
		      Dtors_Symbols = vsp;
		      break;
		    case 'G':
		      as_warn (_("Can't handle global xtors symbols yet."));
		      break;
		    default:
		      as_warn (_("Unknown %s"), S_GET_NAME (sp));
		      break;
		  }
	      }
	    else
	      {
		unsigned short Entry_Mask;

		/* Get the entry mask.  */
		fragP = sp->sy_frag;
		/* First frag might be empty if we're generating listings.
		   So skip empty rs_fill frags.  */
		while (fragP && fragP->fr_type == rs_fill && fragP->fr_fix == 0)
		  fragP = fragP->fr_next;

		/* If first frag doesn't contain the data, what do we do?
		   If it's possibly smaller than two bytes, that would
		   imply that the entry mask is not stored where we're
		   expecting it.

		   If you can find a test case that triggers this, report
		   it (and tell me what the entry mask field ought to be),
		   and I'll try to fix it.  KR */
		if (fragP->fr_fix < 2)
		  abort ();

		Entry_Mask = (fragP->fr_literal[0] & 0x00ff) |
			     ((fragP->fr_literal[1] & 0x00ff) << 8);
		/* Define the procedure entry point.  */
		VMS_Procedure_Entry_Pt (S_GET_NAME (sp),
				    Text_Psect,
				    S_GET_VALUE (sp),
				    Entry_Mask);
	      }
	    break;
	  }

	/* Local Text definition.  */
	case N_TEXT:
	  /* Make a VMS data symbol entry.  */
	  if (Text_Psect != -1)
	    {
	      vsp = xmalloc (sizeof *vsp);
	      vsp->Symbol = sp;
	      vsp->Size = 0;
	      vsp->Psect_Index = Text_Psect;
	      vsp->Psect_Offset = S_GET_VALUE (sp);
	      vsp->Next = VMS_Symbols;
	      VMS_Symbols = vsp;
	      sp->sy_obj = vsp;
	    }
	  break;

	/* Global Reference.  */
	case N_UNDF:
	  /* Make a GSD global symbol reference record.  */
	  VMS_Global_Symbol_Spec (S_GET_NAME (sp),
				  0,
				  0,
				  GBLSYM_REF);
	  break;

	/* Absolute symbol.  */
	case N_ABS:
	case N_ABS | N_EXT:
	  /* gcc doesn't generate these;
	     VMS_Emit_Globalvalue handles them though.	*/
	  vsp = xmalloc (sizeof *vsp);
	  vsp->Symbol = sp;
	  vsp->Size = 4;		/* always assume 32 bits */
	  vsp->Psect_Index = 0;
	  vsp->Psect_Offset = S_GET_VALUE (sp);
	  vsp->Next = VMS_Symbols;
	  VMS_Symbols = vsp;
	  sp->sy_obj = vsp;
	  break;

	/* Anything else.  */
	default:
	  /* Ignore STAB symbols, including .stabs emitted by g++.  */
	  if (S_IS_DEBUG (sp) || (S_GET_TYPE (sp) == 22))
	    break;
	  /*
	   *	Error otherwise.
	   */
	  as_tsktsk (_("unhandled stab type %d"), S_GET_TYPE (sp));
	  break;
	}

      /* Global symbols have different linkage than external variables.  */
      if (define_as_global_symbol)
	VMS_Global_Symbol_Spec (S_GET_NAME (sp),
				vsp->Psect_Index,
				0,
				GBLSYM_DEF);
    }
}


/* Output debugger symbol table information for symbols which
   are local to a specific routine.  */

static void
local_symbols_DST (symbolS *s0P, symbolS *Current_Routine)
{
  symbolS *s1P;
  char *s0P_name, *pnt0, *pnt1;

  s0P_name = S_GET_NAME (s0P);
  if (*s0P_name++ != '_')
    return;

  for (s1P = Current_Routine; s1P; s1P = symbol_next (s1P))
    {
#if 0		/* redundant; RAW_TYPE != N_FUN suffices */
      if (!S_IS_DEBUG (s1P))
	continue;
#endif
      if (S_GET_RAW_TYPE (s1P) != N_FUN)
	continue;
      pnt0 = s0P_name;
      pnt1 = S_GET_NAME (s1P);
      /* We assume the two strings are never exactly equal...  */
      while (*pnt0++ == *pnt1++)
	{
	}
      /* Found it if s0P name is exhausted and s1P name has ":F" or ":f" next.
	 Note:  both pointers have advanced one past the non-matching char.  */
      if ((*pnt1 == 'F' || *pnt1 == 'f') && *--pnt1 == ':' && *--pnt0 == '\0')
	{
	  Define_Routine (s1P, 0, Current_Routine, Text_Psect);
	  return;
	}
    }
}

/* Construct and output the debug symbol table.  */

static void
vms_build_DST (unsigned text_siz)
{
  symbolS *symbolP;
  symbolS *Current_Routine = 0;
  struct input_file *Cur_File = 0;
  offsetT Cur_Offset = -1;
  int Cur_Line_Number = 0;
  int File_Number = 0;
  int Debugger_Offset = 0;
  int file_available;
  int dsc;
  offsetT val;

  /* Write the Traceback Begin Module record.  */
  VMS_TBT_Module_Begin ();

  /* Output debugging info for global variables and static variables
     that are not specific to one routine.  We also need to examine
     all stabs directives, to find the definitions to all of the
     advanced data types, and this is done by VMS_LSYM_Parse.  This
     needs to be done before any definitions are output to the object
     file, since there can be forward references in the stabs
     directives.  When through with parsing, the text of the stabs
     directive is altered, with the definitions removed, so that later
     passes will see directives as they would be written if the type
     were already defined.

     We also look for files and include files, and make a list of
     them.  We examine the source file numbers to establish the actual
     lines that code was generated from, and then generate offsets.  */
  VMS_LSYM_Parse ();
  for (symbolP = symbol_rootP; symbolP; symbolP = symbol_next (symbolP))
    {
      /* Only deal with STAB symbols here.  */
      if (!S_IS_DEBUG (symbolP))
	continue;
      /* Dispatch on STAB type.  */
      switch (S_GET_RAW_TYPE (symbolP))
	{
	case N_SLINE:
	  dsc = S_GET_DESC (symbolP);
	  if (dsc > Cur_File->max_line)
	    Cur_File->max_line = dsc;
	  if (dsc < Cur_File->min_line)
	    Cur_File->min_line = dsc;
	  break;
	case N_SO:
	  Cur_File = find_file (symbolP);
	  Cur_File->flag = 1;
	  Cur_File->min_line = 1;
	  break;
	case N_SOL:
	  Cur_File = find_file (symbolP);
	  break;
	case N_GSYM:
	  VMS_GSYM_Parse (symbolP, Text_Psect);
	  break;
	case N_LCSYM:
	  VMS_LCSYM_Parse (symbolP, Text_Psect);
	  break;
	case N_FUN:		/* For static constant symbols */
	case N_STSYM:
	  VMS_STSYM_Parse (symbolP, Text_Psect);
	  break;
	default:
	  break;
	}
    }

  /* Now we take a quick sweep through the files and assign offsets
     to each one.  This will essentially be the starting line number to
     the debugger for each file.  Output the info for the debugger to
     specify the files, and then tell it how many lines to use.  */
  for (Cur_File = file_root; Cur_File; Cur_File = Cur_File->next)
    {
      if (Cur_File->max_line == 0)
	continue;
      if ((strncmp (Cur_File->name, "GNU_GXX_INCLUDE:", 16) == 0) &&
	  !flag_debug)
	continue;
      if ((strncmp (Cur_File->name, "GNU_CC_INCLUDE:", 15) == 0) &&
	  !flag_debug)
	continue;
      /* show a few extra lines at the start of the region selected */
      if (Cur_File->min_line > 2)
	Cur_File->min_line -= 2;
      Cur_File->offset = Debugger_Offset - Cur_File->min_line + 1;
      Debugger_Offset += Cur_File->max_line - Cur_File->min_line + 1;
      if (Cur_File->same_file_fpnt)
	{
	  Cur_File->file_number = Cur_File->same_file_fpnt->file_number;
	}
      else
	{
	  Cur_File->file_number = ++File_Number;
	  file_available = VMS_TBT_Source_File (Cur_File->name,
						Cur_File->file_number);
	  if (!file_available)
	    {
	      Cur_File->file_number = 0;
	      File_Number--;
	      continue;
	    }
	}
      VMS_TBT_Source_Lines (Cur_File->file_number,
			    Cur_File->min_line,
			    Cur_File->max_line - Cur_File->min_line + 1);
  }			/* for */
  Cur_File = (struct input_file *) NULL;

  /* Scan the symbols and write out the routines
     (this makes the assumption that symbols are in
     order of ascending text segment offset).  */
  for (symbolP = symbol_rootP; symbolP; symbolP = symbol_next (symbolP))
    {
      /* Deal with text symbols.  */
      if (!S_IS_DEBUG (symbolP) && S_GET_TYPE (symbolP) == N_TEXT)
	{
	  /* Ignore symbols starting with "L", as they are local symbols.  */
	  if (*S_GET_NAME (symbolP) == 'L')
	    continue;
	  /* If there is a routine start defined, terminate it.  */
	  if (Current_Routine)
	    VMS_TBT_Routine_End (text_siz, Current_Routine);

	  /* Check for & skip dummy labels like "gcc_compiled.".
	   * They're identified by the IN_DEFAULT_SECTION flag.  */
	  if ((S_GET_OTHER (symbolP) & IN_DEFAULT_SECTION) != 0 &&
	      S_GET_VALUE (symbolP) == 0)
	    continue;
	  /* Store the routine begin traceback info.  */
	  VMS_TBT_Routine_Begin (symbolP, Text_Psect);
	  Current_Routine = symbolP;
	  /* Define symbols local to this routine.  */
	  local_symbols_DST (symbolP, Current_Routine);
	  /* Done.  */
	  continue;

	}
      /* Deal with STAB symbols.  */
      else if (S_IS_DEBUG (symbolP))
	{
	  /* Dispatch on STAB type.  */
	  switch (S_GET_RAW_TYPE (symbolP))
	    {
	      /* Line number.  */
	    case N_SLINE:
	      /* Offset the line into the correct portion of the file.  */
	      if (Cur_File->file_number == 0)
		break;
	      val = S_GET_VALUE (symbolP);
	      /* Sometimes the same offset gets several source lines
		 assigned to it.  We should be selective about which
		 lines we allow, we should prefer lines that are in
		 the main source file when debugging inline functions.  */
	      if (val == Cur_Offset && Cur_File->file_number != 1)
		break;

	      /* Calculate actual debugger source line.  */
	      dsc = S_GET_DESC (symbolP) + Cur_File->offset;
	      S_SET_DESC (symbolP, dsc);
	      /* Define PC/Line correlation.  */
	      if (Cur_Offset == -1)
		{
		  /* First N_SLINE; set up initial correlation.  */
		  VMS_TBT_Line_PC_Correlation (dsc,
					       val,
					       Text_Psect,
					       0);
		}
	      else if ((dsc - Cur_Line_Number) <= 0)
		{
		  /* Line delta is not +ve, we need to close the line and
		     start a new PC/Line correlation.  */
		  VMS_TBT_Line_PC_Correlation (0,
					       val - Cur_Offset,
					       0,
					       -1);
		  VMS_TBT_Line_PC_Correlation (dsc,
					       val,
					       Text_Psect,
					       0);
		}
	      else
		{
		  /* Line delta is +ve, all is well.  */
		  VMS_TBT_Line_PC_Correlation (dsc - Cur_Line_Number,
					       val - Cur_Offset,
					       0,
					       1);
		}
	      /* Update the current line/PC info.  */
	      Cur_Line_Number = dsc;
	      Cur_Offset = val;
	      break;

		/* Source file.  */
	    case N_SO:
	      /* Remember that we had a source file and emit
		 the source file debugger record.  */
	      Cur_File = find_file (symbolP);
	      break;

	    case N_SOL:
	      /* We need to make sure that we are really in the actual
		 source file when we compute the maximum line number.
		 Otherwise the debugger gets really confused.  */
	      Cur_File = find_file (symbolP);
	      break;

	    default:
	      break;
	    }
	}
    }

    /* If there is a routine start defined, terminate it
       (and the line numbers).  */
    if (Current_Routine)
      {
	/* Terminate the line numbers.  */
	VMS_TBT_Line_PC_Correlation (0,
				     text_siz - S_GET_VALUE (Current_Routine),
				     0,
				     -1);
	/* Terminate the routine.  */
	VMS_TBT_Routine_End (text_siz, Current_Routine);
      }

  /* Write the Traceback End Module TBT record.  */
  VMS_TBT_Module_End ();
}


/* Write a VAX/VMS object file (everything else has been done!).  */

void
vms_write_object_file (unsigned text_siz, unsigned data_siz, unsigned bss_siz,
		       fragS *text_frag_root, fragS *data_frag_root)
{
  struct VMS_Symbol *vsp;

  /* Initialize program section indices; values get updated later.  */
  Psect_Number = 0;		/* next Psect Index to use */
  Text_Psect = -1;		/* Text Psect Index   */
  Data_Psect = -2;		/* Data Psect Index   JF: Was -1 */
  Bss_Psect = -3;		/* Bss Psect Index    JF: Was -1 */
  Ctors_Psect = -4;		/* Ctors Psect Index  */
  Dtors_Psect = -5;		/* Dtors Psect Index  */
  /* Initialize other state variables.  */
  Data_Segment = 0;
  Local_Initd_Data_Size = 0;

  /* Create the actual output file and populate it with required
     "module header" information.  */
  Create_VMS_Object_File ();
  Write_VMS_MHD_Records ();

  /* Create the Data segment:

     Since this is REALLY hard to do any other way,
     we actually manufacture the data segment and
     then store the appropriate values out of it.
     We need to generate this early, so that globalvalues
     can be properly emitted.  */
  if (data_siz > 0)
    synthesize_data_segment (data_siz, text_siz, data_frag_root);

  /* Global Symbol Directory.  */

  /* Emit globalvalues now.  We must do this before the text psect is
     defined, or we will get linker warnings about multiply defined
     symbols.  All of the globalvalues "reference" psect 0, although
     it really does not have anything to do with it.  */
  VMS_Emit_Globalvalues (text_siz, data_siz, Data_Segment);
  /* Define the Text Psect.  */
  Text_Psect = Psect_Number++;
  VMS_Psect_Spec ("$code", text_siz, ps_TEXT, 0);
  /* Define the BSS Psect.  */
  if (bss_siz > 0)
    {
      Bss_Psect = Psect_Number++;
      VMS_Psect_Spec ("$uninitialized_data", bss_siz, ps_DATA, 0);
    }
  /* Define symbols to the linker.  */
  global_symbol_directory (text_siz, data_siz);
  /* Define the Data Psect.  */
  if (data_siz > 0 && Local_Initd_Data_Size > 0)
    {
      Data_Psect = Psect_Number++;
      VMS_Psect_Spec ("$data", Local_Initd_Data_Size, ps_DATA, 0);
      /* Local initialized data (N_DATA) symbols need to be updated to the
         proper value of Data_Psect now that it's actually been defined.
         (A dummy value was used in global_symbol_directory() above.)  */
      for (vsp = VMS_Symbols; vsp; vsp = vsp->Next)
	if (vsp->Psect_Index < 0 && S_GET_RAW_TYPE (vsp->Symbol) == N_DATA)
	  vsp->Psect_Index = Data_Psect;
    }

  if (Ctors_Symbols != 0)
    {
      char *ps_name = "$ctors";
      Ctors_Psect = Psect_Number++;
      VMS_Psect_Spec (ps_name, Ctors_Symbols->Psect_Offset + XTOR_SIZE,
		      ps_CTORS, 0);
      VMS_Global_Symbol_Spec (ps_name, Ctors_Psect,
				  0, GBLSYM_DEF|GBLSYM_WEAK);
      for (vsp = Ctors_Symbols; vsp; vsp = vsp->Next)
	vsp->Psect_Index = Ctors_Psect;
    }

  if (Dtors_Symbols != 0)
    {
      char *ps_name = "$dtors";
      Dtors_Psect = Psect_Number++;
      VMS_Psect_Spec (ps_name, Dtors_Symbols->Psect_Offset + XTOR_SIZE,
		      ps_DTORS, 0);
      VMS_Global_Symbol_Spec (ps_name, Dtors_Psect,
				  0, GBLSYM_DEF|GBLSYM_WEAK);
      for (vsp = Dtors_Symbols; vsp; vsp = vsp->Next)
	vsp->Psect_Index = Dtors_Psect;
    }

  /* Text Information and Relocation Records.  */

  /* Write the text segment data.  */
  if (text_siz > 0)
    vms_fixup_text_section (text_siz, text_frag_root, data_frag_root);
  /* Write the data segment data, then discard it.  */
  if (data_siz > 0)
    {
      vms_fixup_data_section (data_siz, text_siz);
      free (Data_Segment),  Data_Segment = 0;
    }

  if (Ctors_Symbols != 0)
    vms_fixup_xtors_section (Ctors_Symbols, Ctors_Psect);

  if (Dtors_Symbols != 0)
    vms_fixup_xtors_section (Dtors_Symbols, Dtors_Psect);

  /* Debugger Symbol Table Records.  */

  vms_build_DST (text_siz);

  /* Wrap things up.  */

  /* Write the End Of Module record.  */
  if (Entry_Point_Symbol)
    Write_VMS_EOM_Record (Text_Psect, S_GET_VALUE (Entry_Point_Symbol));
  else
    Write_VMS_EOM_Record (-1, (valueT) 0);

  /* All done, close the object file.  */
  Close_VMS_Object_File ();
}
