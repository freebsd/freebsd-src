/* vms.c -- Write out a VAX/VMS object file
   Copyright (C) 1987, 1988, 1992 Free Software Foundation, Inc.

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
the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.  */

/* Written by David L. Kashtan */
/* Modified by Eric Youngdale to write VMS debug records for program
   variables */
#include "as.h"
#include "subsegs.h"
#include "obstack.h"

/* What we do if there is a goof. */
#define error as_fatal

#ifdef HO_VMS			/* These are of no use if we are cross assembling. */
#include <fab.h>		/* Define File Access Block	  */
#include <nam.h>		/* Define NAM Block		  */
#include <xab.h>		/* Define XAB - all different types*/
#endif
/*
 *	Version string of the compiler that produced the code we are
 *	assembling.  (And this assembler, if we do not have compiler info.)
 */
extern const char version_string[];
char *compiler_version_string;

/* Flag that determines how we map names.  This takes several values, and
 * is set with the -h switch.  A value of zero implies names should be
 * upper case, and the presence of the -h switch inhibits the case hack.
 * No -h switch at all sets vms_name_mapping to 0, and allows case hacking.
 * A value of 2 (set with -h2) implies names should be
 * all lower case, with no case hack.  A value of 3 (set with -h3) implies
 * that case should be preserved.  */

/* If the -+ switch is given, then the hash is appended to any name that is
 * longer than 31 characters, irregardless of the setting of the -h switch.
 */

char vms_name_mapping = 0;


extern char *strchr ();
extern char *myname;
static symbolS *Entry_Point_Symbol = 0;	/* Pointer to "_main" */

/*
 *	We augment the "gas" symbol structure with this
 */
struct VMS_Symbol
{
  struct VMS_Symbol *Next;
  struct symbol *Symbol;
  int Size;
  int Psect_Index;
  int Psect_Offset;
};
struct VMS_Symbol *VMS_Symbols = 0;

/* We need this to keep track of the various input files, so that we can
 * give the debugger the correct source line.
 */

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


static struct input_file *find_file (symbolS *);

/*
 * This enum is used to keep track of the various types of variables that
 * may be present.
 */

enum advanced_type
{
  BASIC, POINTER, ARRAY, ENUM, STRUCT, UNION, FUNCTION, VOID, UNKNOWN
};

/*
 * This structure contains the information from the stabs directives, and the
 * information is filled in by VMS_typedef_parse.  Everything that is needed
 * to generate the debugging record for a given symbol is present here.
 * This could be done more efficiently, using nested struct/unions, but for now
 * I am happy that it works.
 */
struct VMS_DBG_Symbol
{
  struct VMS_DBG_Symbol *next;
  enum advanced_type advanced;	/* description of what this is */
  int dbx_type;			/* this record is for this type */
  int type2;			/* For advanced types this is the type referred to.
					i.e. the type a pointer points to, or the type
					of object that makes up an array */
  int VMS_type;			/* Use this type when generating a variable def */
  int index_min;		/* used for arrays - this will be present for all */
  int index_max;		/* entries, but will be meaningless for non-arrays */
  int data_size;		/* size in bytes of the data type.  For an array, this
				   is the size of one element in the array */
  int struc_numb;		/* Number of the structure/union/enum - used for ref */
};

struct VMS_DBG_Symbol *VMS_Symbol_type_list =
{(struct VMS_DBG_Symbol *) NULL};

/*
 * We need this structure to keep track of forward references to
 * struct/union/enum that have not been defined yet.  When they are ultimately
 * defined, then we can go back and generate the TIR commands to make a back
 * reference.
 */

struct forward_ref
{
  struct forward_ref *next;
  int dbx_type;
  int struc_numb;
  char resolved;
};

struct forward_ref *f_ref_root =
{(struct forward_ref *) NULL};

/*
 * This routine is used to compare the names of certain types to various
 * fixed types that are known by the debugger.
 */
#define type_check(x)  !strcmp( symbol_name , x )

/*
 * This variable is used to keep track of the name of the symbol we are
 * working on while we are parsing the stabs directives.
 */
static char *symbol_name;

/* We use this counter to assign numbers to all of the structures, unions
 * and enums that we define.  When we actually declare a variable to the
 * debugger, we can simply do it by number, rather than describing the
 * whole thing each time.
 */

static structure_count = 0;

/* This variable is used to keep track of the current structure number
 * for a given variable.  If this is < 0, that means that the structure
 * has not yet been defined to the debugger.  This is still cool, since
 * the VMS object language has ways of fixing things up after the fact,
 * so we just make a note of this, and generate fixups at the end.
 */
static int struct_number;


/*
 * Variable descriptors are used tell the debugger the data types of certain
 * more complicated variables (basically anything involving a structure,
 * union, enum, array or pointer).  Some non-pointer variables of the
 * basic types that the debugger knows about do not require a variable
 * descriptor.
 *
 * Since it is impossible to have a variable descriptor longer than 128
 * bytes by virtue of the way that the VMS object language is set up,
 * it makes not sense to make the arrays any longer than this, or worrying
 * about dynamic sizing of the array.
 *
 * These are the arrays and counters that we use to build a variable
 * descriptor.
 */

#define MAX_DEBUG_RECORD 128
static char Local[MAX_DEBUG_RECORD];	/* buffer for variable descriptor */
static char Asuffix[MAX_DEBUG_RECORD];	/* buffer for array descriptor */
static int Lpnt;		/* index into Local */
static int Apoint;		/* index into Asuffix */
static char overflow;		/* flag to indicate we have written too much*/
static int total_len;		/* used to calculate the total length of variable
				descriptor plus array descriptor - used for len byte*/

/* Flag if we have told user about finding global constants in the text
   section. */
static gave_compiler_message = 0;

/* A pointer to the current routine that we are working on.  */

static symbolS *Current_Routine;

/* The psect number for $code a.k.a. the text section. */

static int Text_Psect;


/*
 *	Global data (Object records limited to 512 bytes by VAX-11 "C" runtime)
 */
static int VMS_Object_File_FD;	/* File Descriptor for object file */
static char Object_Record_Buffer[512];	/* Buffer for object file records  */
static int Object_Record_Offset;/* Offset to end of data	   */
static int Current_Object_Record_Type;	/* Type of record in above	   */

/*
 *	Macros for placing data into the object record buffer
 */

#define	PUT_LONG(val) \
{ md_number_to_chars(Object_Record_Buffer + \
		     Object_Record_Offset, val, 4); \
			 Object_Record_Offset += 4; }

#define	PUT_SHORT(val) \
{ md_number_to_chars(Object_Record_Buffer + \
		     Object_Record_Offset, val, 2); \
			 Object_Record_Offset += 2; }

#define	PUT_CHAR(val)	Object_Record_Buffer[Object_Record_Offset++] = val

#define	PUT_COUNTED_STRING(cp) {\
			register char *p = cp; \
			PUT_CHAR(strlen(p)); \
			while (*p) PUT_CHAR(*p++);}

/*
 *	Macro for determining if a Name has psect attributes attached
 *	to it.
 */
#define	PSECT_ATTRIBUTES_STRING		"$$PsectAttributes_"
#define	PSECT_ATTRIBUTES_STRING_LENGTH	18

#define	HAS_PSECT_ATTRIBUTES(Name) \
		(strncmp((Name[0] == '_' ? Name + 1 : Name), \
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
  N_UNDF,			/* absent */
  N_UNDF,			/* pass1 */
  N_UNDF,			/* error */
  N_UNDF,			/* bignum/flonum */
  N_UNDF,			/* difference */
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
 *  use with VMS.
 */

char const_flag = 0;

void
s_const ()
{
  register int temp;

  temp = get_absolute_expression ();
  subseg_new (SEG_DATA, (subsegT) temp);
  const_flag = 1;
  demand_empty_rest_of_line ();
}

/*
 *			stab()
 *
 * Handle .stabX directives, which used to be open-coded.
 * So much creeping featurism overloaded the semantics that we decided
 * to put all .stabX thinking in one place. Here.
 *
 * We try to make any .stabX directive legal. Other people's AS will often
 * do assembly-time consistency checks: eg assigning meaning to n_type bits
 * and "protecting" you from setting them to certain values. (They also zero
 * certain bits before emitting symbols. Tut tut.)
 *
 * If an expression is not absolute we either gripe or use the relocation
 * information. Other people's assemblers silently forget information they
 * don't need and invent information they need that you didn't supply.
 *
 * .stabX directives always make a symbol table entry. It may be junk if
 * the rest of your .stabX directive is malformed.
 */
static void
obj_aout_stab (what)
     int what;
{
  register symbolS *symbolP = 0;
  register char *string;
  int saved_type = 0;
  int length;
  int goof;			/* TRUE if we have aborted. */
  long longint;

/*
 * Enter with input_line_pointer pointing past .stabX and any following
 * whitespace.
 */
  goof = 0;			/* JF who forgot this?? */
  if (what == 's')
    {
      string = demand_copy_C_string (&length);
      SKIP_WHITESPACE ();
      if (*input_line_pointer == ',')
	input_line_pointer++;
      else
	{
	  as_bad ("I need a comma after symbol's name");
	  goof = 1;
	}
    }
  else
    string = "";

/*
 * Input_line_pointer->after ','.  String->symbol name.
 */
  if (!goof)
    {
      symbolP = symbol_new (string,
			    SEG_UNKNOWN,
			    0,
			    (struct frag *) 0);
      switch (what)
	{
	case 'd':
	  S_SET_NAME (symbolP, NULL);	/* .stabd feature. */
	  S_SET_VALUE (symbolP, obstack_next_free (&frags) - frag_now->fr_literal);
	  symbolP->sy_frag = frag_now;
	  break;

	case 'n':
	  symbolP->sy_frag = &zero_address_frag;
	  break;

	case 's':
	  symbolP->sy_frag = &zero_address_frag;
	  break;

	default:
	  BAD_CASE (what);
	  break;
	}

      if (get_absolute_expression_and_terminator (&longint) == ',')
	symbolP->sy_symbol.n_type = saved_type = longint;
      else
	{
	  as_bad ("I want a comma after the n_type expression");
	  goof = 1;
	  input_line_pointer--;	/* Backup over a non-',' char. */
	}
    }

  if (!goof)
    {
      if (get_absolute_expression_and_terminator (&longint) == ',')
	S_SET_OTHER (symbolP, longint);
      else
	{
	  as_bad ("I want a comma after the n_other expression");
	  goof = 1;
	  input_line_pointer--;	/* Backup over a non-',' char. */
	}
    }

  if (!goof)
    {
      S_SET_DESC (symbolP, get_absolute_expression ());
      if (what == 's' || what == 'n')
	{
	  if (*input_line_pointer != ',')
	    {
	      as_bad ("I want a comma after the n_desc expression");
	      goof = 1;
	    }
	  else
	    {
	      input_line_pointer++;
	    }
	}
    }

  if ((!goof) && (what == 's' || what == 'n'))
    {
      pseudo_set (symbolP);
      symbolP->sy_symbol.n_type = saved_type;
    }

  if (goof)
    ignore_rest_of_line ();
  else
    demand_empty_rest_of_line ();
}				/* obj_aout_stab() */

const pseudo_typeS obj_pseudo_table[] =
{
  {"stabd", obj_aout_stab, 'd'},/* stabs */
  {"stabn", obj_aout_stab, 'n'},/* stabs */
  {"stabs", obj_aout_stab, 's'},/* stabs */
  {"const", s_const, 0},
  {0, 0, 0},

};				/* obj_pseudo_table */

void
obj_read_begin_hook ()
{
  return;
}				/* obj_read_begin_hook() */

void
obj_crawl_symbol_chain (headers)
     object_headers *headers;
{
  symbolS *symbolP;
  symbolS **symbolPP;
  int symbol_number = 0;

  /* JF deal with forward references first... */
  for (symbolP = symbol_rootP; symbolP; symbolP = symbol_next (symbolP))
    {
      if (symbolP->sy_forward)
	{
	  S_SET_VALUE (symbolP, S_GET_VALUE (symbolP)
		       + S_GET_VALUE (symbolP->sy_forward)
		       + symbolP->sy_forward->sy_frag->fr_address);
	  symbolP->sy_forward = 0;
	}			/* if it has a forward reference */
    }				/* walk the symbol chain */

  {				/* crawl symbol table */
    register int symbol_number = 0;

    {
      symbolPP = &symbol_rootP;	/* -> last symbol chain link. */
      while ((symbolP = *symbolPP) != NULL)
	{
	  S_GET_VALUE (symbolP) += symbolP->sy_frag->fr_address;

	  /* OK, here is how we decide which symbols go out into the
	     brave new symtab.  Symbols that do are:

	     * symbols with no name (stabd's?)
	     * symbols with debug info in their N_TYPE

	     Symbols that don't are:
	     * symbols that are registers
	     * symbols with \1 as their 3rd character (numeric labels)
	     * "local labels" as defined by S_LOCAL_NAME(name)
	     if the -L switch was passed to gas.

	     All other symbols are output.  We complain if a deleted
	     symbol was marked external.  */


	  if (!S_IS_REGISTER (symbolP))
	    {
	      symbolP->sy_name_offset = 0;
	      symbolPP = &(symbol_next (symbolP));
	    }
	  else
	    {
	      if (S_IS_EXTERNAL (symbolP) || !S_IS_DEFINED (symbolP))
		{
		  as_bad ("Local symbol %s never defined", S_GET_NAME (symbolP));
		}		/* oops. */

	    }			/* if this symbol should be in the output */
	}			/* for each symbol */
    }
    H_SET_STRING_SIZE (headers, string_byte_count);
    H_SET_SYMBOL_TABLE_SIZE (headers, symbol_number);
  }				/* crawl symbol table */

}				/* obj_crawl_symbol_chain() */


 /****** VMS OBJECT FILE HACKING ROUTINES *******/


/*
 *	Create the VMS object file
 */
static
Create_VMS_Object_File ()
{
#if	defined(eunice) || !defined(HO_VMS)
  VMS_Object_File_FD = creat (out_file_name, 0777, "var");
#else	/* eunice */
  VMS_Object_File_FD = creat (out_file_name, 0, "rfm=var",
			     "mbc=16", "deq=64", "fop=tef", "shr=nil");
#endif	/* eunice */
  /*
   *	Deal with errors
   */
  if (VMS_Object_File_FD < 0)
    {
      char Error_Line[256];

      sprintf (Error_Line, "Couldn't create VMS object file \"%s\"",
	       out_file_name);
      error (Error_Line);
    }
  /*
   *	Initialize object file hacking variables
   */
  Object_Record_Offset = 0;
  Current_Object_Record_Type = -1;
}


/*
 *	Flush the object record buffer to the object file
 */
static
Flush_VMS_Object_Record_Buffer ()
{
  int i;
  short int zero;
  /*
   *	If the buffer is empty, we are done
   */
  if (Object_Record_Offset == 0)
    return;
  /*
   *	Write the data to the file
   */
#ifndef HO_VMS			/* For cross-assembly purposes. */
  i = write (VMS_Object_File_FD, &Object_Record_Offset, 2);
#endif /* not HO_VMS */
  i = write (VMS_Object_File_FD,
	     Object_Record_Buffer,
	     Object_Record_Offset);
  if (i != Object_Record_Offset)
    error ("I/O error writing VMS object file");
#ifndef HO_VMS			/* When cross-assembling, we need to pad the record to an even
						number of bytes. */
  /* pad it if needed */
  zero = 0;
  if (Object_Record_Offset & 1 != 0)
    write (VMS_Object_File_FD, &zero, 1);
#endif /* not HO_VMS */
  /*
   *	The buffer is now empty
   */
  Object_Record_Offset = 0;
}


/*
 *	Declare a particular type of object file record
 */
static
Set_VMS_Object_File_Record (Type)
     int Type;
{
  /*
   *	If the type matches, we are done
   */
  if (Type == Current_Object_Record_Type)
    return;
  /*
   *	Otherwise: flush the buffer
   */
  Flush_VMS_Object_Record_Buffer ();
  /*
   *	Set the new type
   */
  Current_Object_Record_Type = Type;
}



/*
 *	Close the VMS Object file
 */
static
Close_VMS_Object_File ()
{
  short int m_one = -1;
#ifndef HO_VMS			/* For cross-assembly purposes. */
/* Write a 0xffff into the file, which means "End of File" */
  write (VMS_Object_File_FD, &m_one, 2);
#endif /* not HO_VMS */
  close (VMS_Object_File_FD);
}


/*
 *	Store immediate data in current Psect
 */
static
VMS_Store_Immediate_Data (Pointer, Size, Record_Type)
     register char *Pointer;
     int Size;
     int Record_Type;
{
  register int i;

  /*
   *	We are writing a "Record_Type" record
   */
  Set_VMS_Object_File_Record (Record_Type);
  /*
   *	We can only store 128 bytes at a time
   */
  while (Size > 0)
    {
      /*
       *	Store a maximum of 128 bytes
       */
      i = (Size > 128) ? 128 : Size;
      Size -= i;
      /*
       *	If we cannot accommodate this record, flush the
       *	buffer.
       */
      if ((Object_Record_Offset + i + 1) >=
	  sizeof (Object_Record_Buffer))
	Flush_VMS_Object_Record_Buffer ();
      /*
       *	If the buffer is empty we must insert record type
       */
      if (Object_Record_Offset == 0)
	PUT_CHAR (Record_Type);
      /*
       *	Store the count
       */
      PUT_CHAR (-i & 0xff);
      /*
       *	Store the data
       */
      while (--i >= 0)
	PUT_CHAR (*Pointer++);
      /*
       *	Flush the buffer if it is more than 75% full
       */
      if (Object_Record_Offset >
	  (sizeof (Object_Record_Buffer) * 3 / 4))
	Flush_VMS_Object_Record_Buffer ();
    }
}

/*
 *	Make a data reference
 */
static
VMS_Set_Data (Psect_Index, Offset, Record_Type, Force)
     int Psect_Index;
     int Offset;
     int Record_Type;
     int Force;
{
  /*
   *	We are writing a "Record_Type" record
   */
  Set_VMS_Object_File_Record (Record_Type);
  /*
   *	If the buffer is empty we must insert the record type
   */
  if (Object_Record_Offset == 0)
    PUT_CHAR (Record_Type);
  /*
   *	Stack the Psect base + Longword Offset
   */
  if (Force == 1)
    {
      if (Psect_Index > 127)
	{
	  PUT_CHAR (TIR_S_C_STA_WPL);
	  PUT_SHORT (Psect_Index);
	  PUT_LONG (Offset);
	}
      else
	{
	  PUT_CHAR (TIR_S_C_STA_PL);
	  PUT_CHAR (Psect_Index);
	  PUT_LONG (Offset);
	}
    }
  else
    {
      if (Offset > 32767)
	{
	  PUT_CHAR (TIR_S_C_STA_WPL);
	  PUT_SHORT (Psect_Index);
	  PUT_LONG (Offset);
	}
      else if (Offset > 127)
	{
	  PUT_CHAR (TIR_S_C_STA_WPW);
	  PUT_SHORT (Psect_Index);
	  PUT_SHORT (Offset);
	}
      else
	{
	  PUT_CHAR (TIR_S_C_STA_WPB);
	  PUT_SHORT (Psect_Index);
	  PUT_CHAR (Offset);
	};
    };
  /*
   *	Set relocation base
   */
  PUT_CHAR (TIR_S_C_STO_PIDR);
  /*
   *	Flush the buffer if it is more than 75% full
   */
  if (Object_Record_Offset >
      (sizeof (Object_Record_Buffer) * 3 / 4))
    Flush_VMS_Object_Record_Buffer ();
}

/*
 *	Make a debugger reference to a struct, union or enum.
 */
static
VMS_Store_Struct (int Struct_Index)
{
  /*
   *	We are writing a "OBJ_S_C_DBG" record
   */
  Set_VMS_Object_File_Record (OBJ_S_C_DBG);
  /*
   *	If the buffer is empty we must insert the record type
   */
  if (Object_Record_Offset == 0)
    PUT_CHAR (OBJ_S_C_DBG);
  PUT_CHAR (TIR_S_C_STA_UW);
  PUT_SHORT (Struct_Index);
  PUT_CHAR (TIR_S_C_CTL_STKDL);
  PUT_CHAR (TIR_S_C_STO_L);
  /*
   *	Flush the buffer if it is more than 75% full
   */
  if (Object_Record_Offset >
      (sizeof (Object_Record_Buffer) * 3 / 4))
    Flush_VMS_Object_Record_Buffer ();
}

/*
 *	Make a debugger reference to partially define a struct, union or enum.
 */
static
VMS_Def_Struct (int Struct_Index)
{
  /*
   *	We are writing a "OBJ_S_C_DBG" record
   */
  Set_VMS_Object_File_Record (OBJ_S_C_DBG);
  /*
   *	If the buffer is empty we must insert the record type
   */
  if (Object_Record_Offset == 0)
    PUT_CHAR (OBJ_S_C_DBG);
  PUT_CHAR (TIR_S_C_STA_UW);
  PUT_SHORT (Struct_Index);
  PUT_CHAR (TIR_S_C_CTL_DFLOC);
  /*
   *	Flush the buffer if it is more than 75% full
   */
  if (Object_Record_Offset >
      (sizeof (Object_Record_Buffer) * 3 / 4))
    Flush_VMS_Object_Record_Buffer ();
}

static
VMS_Set_Struct (int Struct_Index)
{				/* see previous functions for comments */
  Set_VMS_Object_File_Record (OBJ_S_C_DBG);
  if (Object_Record_Offset == 0)
    PUT_CHAR (OBJ_S_C_DBG);
  PUT_CHAR (TIR_S_C_STA_UW);
  PUT_SHORT (Struct_Index);
  PUT_CHAR (TIR_S_C_CTL_STLOC);
  if (Object_Record_Offset >
      (sizeof (Object_Record_Buffer) * 3 / 4))
    Flush_VMS_Object_Record_Buffer ();
}

/*
 *	Write the Traceback Module Begin record
 */
static
VMS_TBT_Module_Begin ()
{
  register char *cp, *cp1;
  int Size;
  char Module_Name[256];
  char Local[256];

  /*
   *	Get module name (the FILENAME part of the object file)
   */
  cp = out_file_name;
  cp1 = Module_Name;
  while (*cp)
    {
      if ((*cp == ']') || (*cp == '>') ||
	  (*cp == ':') || (*cp == '/'))
	{
	  cp1 = Module_Name;
	  cp++;
	  continue;
	}
      *cp1++ = islower (*cp) ? toupper (*cp++) : *cp++;
    }
  *cp1 = 0;
  /*
   *	Limit it to 31 characters
   */
  while (--cp1 >= Module_Name)
    if (*cp1 == '.')
      *cp1 = 0;
  if (strlen (Module_Name) > 31)
    {
      if (flagseen['+'])
	printf ("%s: Module name truncated: %s\n", myname, Module_Name);
      Module_Name[31] = 0;
    }
  /*
   *	Arrange to store the data locally (leave room for size byte)
   */
  cp = Local + 1;
  /*
   *	Begin module
   */
  *cp++ = DST_S_C_MODBEG;
  /*
   *	Unused
   */
  *cp++ = 0;
  /*
   *	Language type == "C"
   */
  *(long *) cp = DST_S_C_C;
  cp += sizeof (long);
  /*
   *	Store the module name
   */
  *cp++ = strlen (Module_Name);
  cp1 = Module_Name;
  while (*cp1)
    *cp++ = *cp1++;
  /*
   *	Now we can store the record size
   */
  Size = (cp - Local);
  Local[0] = Size - 1;
  /*
   *	Put it into the object record
   */
  VMS_Store_Immediate_Data (Local, Size, OBJ_S_C_TBT);
}


/*
 *	Write the Traceback Module End record
*/
static
VMS_TBT_Module_End ()
{
  char Local[2];

  /*
   *	End module
   */
  Local[0] = 1;
  Local[1] = DST_S_C_MODEND;
  /*
   *	Put it into the object record
   */
  VMS_Store_Immediate_Data (Local, 2, OBJ_S_C_TBT);
}


/*
 *	Write the Traceback Routine Begin record
 */
static
VMS_TBT_Routine_Begin (symbolP, Psect)
     struct symbol *symbolP;
     int Psect;
{
  register char *cp, *cp1;
  char *Name;
  int Offset;
  int Size;
  char Local[512];

  /*
   *	Strip the leading "_" from the name
   */
  Name = S_GET_NAME (symbolP);
  if (*Name == '_')
    Name++;
  /*
   *	Get the text psect offset
   */
  Offset = S_GET_VALUE (symbolP);
  /*
   *	Calculate the record size
   */
  Size = 1 + 1 + 4 + 1 + strlen (Name);
  /*
   *	Record Size
   */
  Local[0] = Size;
  /*
   *	Begin Routine
   */
  Local[1] = DST_S_C_RTNBEG;
  /*
   *	Uses CallS/CallG
   */
  Local[2] = 0;
  /*
   *	Store the data so far
   */
  VMS_Store_Immediate_Data (Local, 3, OBJ_S_C_TBT);
  /*
   *	Make sure we are still generating a OBJ_S_C_TBT record
   */
  if (Object_Record_Offset == 0)
    PUT_CHAR (OBJ_S_C_TBT);
  /*
   *	Now get the symbol address
   */
  PUT_CHAR (TIR_S_C_STA_WPL);
  PUT_SHORT (Psect);
  PUT_LONG (Offset);
  /*
   *	Store the data reference
   */
  PUT_CHAR (TIR_S_C_STO_PIDR);
  /*
   *	Store the counted string as data
   */
  cp = Local;
  cp1 = Name;
  Size = strlen (cp1) + 1;
  *cp++ = Size - 1;
  while (*cp1)
    *cp++ = *cp1++;
  VMS_Store_Immediate_Data (Local, Size, OBJ_S_C_TBT);
}


/*
 *	Write the Traceback Routine End record
 * 	We *must* search the symbol table to find the next routine, since
 * 	the assember has a way of reassembling the symbol table OUT OF ORDER
 * 	Thus the next routine in the symbol list is not necessarily the
 *	next one in memory.  For debugging to work correctly we must know the
 *	size of the routine.
 */
static
VMS_TBT_Routine_End (Max_Size, sp)
     int Max_Size;
     symbolS *sp;
{
  symbolS *symbolP;
  int Size = 0x7fffffff;
  char Local[16];


  for (symbolP = symbol_rootP; symbolP; symbolP = symbol_next (symbolP))
    {
      if (!S_IS_DEBUG (symbolP) && S_GET_TYPE (symbolP) == N_TEXT)
	{
	  if (*S_GET_NAME (symbolP) == 'L')
	    continue;
	  if ((S_GET_VALUE (symbolP) > S_GET_VALUE (sp)) &&
	      (S_GET_VALUE (symbolP) < Size))
	    Size = S_GET_VALUE (symbolP);
	  /* check if gcc_compiled. has size of zero */
	  if ((S_GET_VALUE (symbolP) == S_GET_VALUE (sp)) &&
	      sp != symbolP &&
	      (!strcmp (S_GET_NAME (sp), "gcc_compiled.") ||
	       !strcmp (S_GET_NAME (sp), "gcc2_compiled.")))
	    Size = S_GET_VALUE (symbolP);

	};
    };
  if (Size == 0x7fffffff)
    Size = Max_Size;
  Size -= S_GET_VALUE (sp);	/* and get the size of the routine */
  /*
   *	Record Size
   */
  Local[0] = 6;
  /*
   *	End of Routine
   */
  Local[1] = DST_S_C_RTNEND;
  /*
   *	Unused
   */
  Local[2] = 0;
  /*
   *	Size of routine
   */
  *((long *) (Local + 3)) = Size;
  /*
   *	Store the record
   */
  VMS_Store_Immediate_Data (Local, 7, OBJ_S_C_TBT);
}

/*
 *	Write the Traceback Block End record
 */
static
VMS_TBT_Block_Begin (symbolP, Psect, Name)
     struct symbol *symbolP;
     int Psect;
     char *Name;
{
  register char *cp, *cp1;
  int Offset;
  int Size;
  char Local[512];
  /*
   *	Begin block
   */
  Size = 1 + 1 + 4 + 1 + strlen (Name);
  /*
   *	Record Size
   */
  Local[0] = Size;
  /*
   *	Begin Block - We simulate with a phony routine
   */
  Local[1] = DST_S_C_BLKBEG;
  /*
   *	Uses CallS/CallG
   */
  Local[2] = 0;
  /*
   *	Store the data so far
   */
  VMS_Store_Immediate_Data (Local, 3, OBJ_S_C_DBG);
  /*
   *	Make sure we are still generating a OBJ_S_C_DBG record
   */
  if (Object_Record_Offset == 0)
    PUT_CHAR (OBJ_S_C_DBG);
  /*
   *	Now get the symbol address
   */
  PUT_CHAR (TIR_S_C_STA_WPL);
  PUT_SHORT (Psect);
  /*
   *	Get the text psect offset
   */
  Offset = S_GET_VALUE (symbolP);
  PUT_LONG (Offset);
  /*
   *	Store the data reference
   */
  PUT_CHAR (TIR_S_C_STO_PIDR);
  /*
   *	Store the counted string as data
   */
  cp = Local;
  cp1 = Name;
  Size = strlen (cp1) + 1;
  *cp++ = Size - 1;
  while (*cp1)
    *cp++ = *cp1++;
  VMS_Store_Immediate_Data (Local, Size, OBJ_S_C_DBG);
}


/*
 *	Write the Traceback Block End record
 */
static
VMS_TBT_Block_End (int Size)
{
  char Local[16];

  /*
   *	End block - simulate with a phony end routine
   */
  Local[0] = 6;
  Local[1] = DST_S_C_BLKEND;
  *((long *) (Local + 3)) = Size;
  /*
   *	Unused
   */
  Local[2] = 0;
  VMS_Store_Immediate_Data (Local, 7, OBJ_S_C_DBG);
}



/*
 *	Write a Line number / PC correlation record
 */
static
VMS_TBT_Line_PC_Correlation (Line_Number, Offset, Psect, Do_Delta)
     int Line_Number;
     int Offset;
     int Psect;
     int Do_Delta;
{
  register char *cp;
  char Local[64];

  /*
*	If not delta, set our PC/Line number correlation
*/
  if (Do_Delta == 0)
    {
      /*
       *	Size
       */
      Local[0] = 1 + 1 + 2 + 1 + 4;
      /*
       *	Line Number/PC correlation
       */
      Local[1] = DST_S_C_LINE_NUM;
      /*
       *	Set Line number
       */
      Local[2] = DST_S_C_SET_LINE_NUM;
      *((unsigned short *) (Local + 3)) = Line_Number - 1;
      /*
       *	Set PC
       */
      Local[5] = DST_S_C_SET_ABS_PC;
      VMS_Store_Immediate_Data (Local, 6, OBJ_S_C_TBT);
      /*
       *	Make sure we are still generating a OBJ_S_C_TBT record
       */
      if (Object_Record_Offset == 0)
	PUT_CHAR (OBJ_S_C_TBT);
      if (Psect < 255)
	{
	  PUT_CHAR (TIR_S_C_STA_PL);
	  PUT_CHAR (Psect);
	}
      else
	{
	  PUT_CHAR (TIR_S_C_STA_WPL);
	  PUT_SHORT (Psect);
	}
      PUT_LONG (Offset);
      PUT_CHAR (TIR_S_C_STO_PIDR);
      /*
       *	Do a PC offset of 0 to register the line number
       */
      Local[0] = 2;
      Local[1] = DST_S_C_LINE_NUM;
      Local[2] = 0;		/* Increment PC by 0 and register line # */
      VMS_Store_Immediate_Data (Local, 3, OBJ_S_C_TBT);
    }
  else
    {
      /*
       *	If Delta is negative, terminate the line numbers
       */
      if (Do_Delta < 0)
	{
	  Local[0] = 1 + 1 + 4;
	  Local[1] = DST_S_C_LINE_NUM;
	  Local[2] = DST_S_C_TERM_L;
	  *((long *) (Local + 3)) = Offset;
	  VMS_Store_Immediate_Data (Local, 7, OBJ_S_C_TBT);
	  /*
	   *	Done
	   */
	  return;
	}
      /*
       *	Do a PC/Line delta
       */
      cp = Local + 1;
      *cp++ = DST_S_C_LINE_NUM;
      if (Line_Number > 1)
	{
	  /*
	   *	We need to increment the line number
	   */
	  if (Line_Number - 1 <= 255)
	    {
	      *cp++ = DST_S_C_INCR_LINUM;
	      *cp++ = Line_Number - 1;
	    }
	  else
	    {
	      *cp++ = DST_S_C_INCR_LINUM_W;
	      *(short *) cp = Line_Number - 1;
	      cp += sizeof (short);
	    }
	}
      /*
       *	Increment the PC
       */
      if (Offset <= 128)
	{
	  *cp++ = -Offset;
	}
      else
	{
	  if (Offset < 0x10000)
	    {
	      *cp++ = DST_S_C_DELTA_PC_W;
	      *(short *) cp = Offset;
	      cp += sizeof (short);
	    }
	  else
	    {
	      *cp++ = DST_S_C_DELTA_PC_L;
	      *(long *) cp = Offset;
	      cp += sizeof (long);
	    }
	}
      Local[0] = cp - (Local + 1);
      VMS_Store_Immediate_Data (Local, cp - Local, OBJ_S_C_TBT);
    }
}


/*
 *	Describe a source file to the debugger
 */
static
VMS_TBT_Source_File (Filename, ID_Number)
     char *Filename;
     int ID_Number;
{
  register char *cp, *cp1;
  int Status, i;
  char Local[512];
#ifndef HO_VMS			/* Used for cross-assembly */
  i = strlen (Filename);
#else /* HO_VMS */
  static struct FAB Fab;
  static struct NAM Nam;
  static struct XABDAT Date_Xab;
  static struct XABFHC File_Header_Xab;
  char Es_String[255], Rs_String[255];

  /*
   *	Setup the Fab
   */
  Fab.fab$b_bid = FAB$C_BID;
  Fab.fab$b_bln = sizeof (Fab);
  Fab.fab$l_nam = (&Nam);
  Fab.fab$l_xab = (char *) &Date_Xab;
  /*
   *	Setup the Nam block so we can find out the FULL name
   *	of the source file.
   */
  Nam.nam$b_bid = NAM$C_BID;
  Nam.nam$b_bln = sizeof (Nam);
  Nam.nam$l_rsa = Rs_String;
  Nam.nam$b_rss = sizeof (Rs_String);
  Nam.nam$l_esa = Es_String;
  Nam.nam$b_ess = sizeof (Es_String);
  /*
   *	Setup the Date and File Header Xabs
   */
  Date_Xab.xab$b_cod = XAB$C_DAT;
  Date_Xab.xab$b_bln = sizeof (Date_Xab);
  Date_Xab.xab$l_nxt = (char *) &File_Header_Xab;
  File_Header_Xab.xab$b_cod = XAB$C_FHC;
  File_Header_Xab.xab$b_bln = sizeof (File_Header_Xab);
  /*
   *	Get the file information
   */
  Fab.fab$l_fna = Filename;
  Fab.fab$b_fns = strlen (Filename);
  Status = sys$open (&Fab);
  if (!(Status & 1))
    {
      printf ("gas: Couldn't find source file \"%s\", Error = %%X%x\n",
	      Filename, Status);
      return (0);
    }
  sys$close (&Fab);
  /*
   *	Calculate the size of the resultant string
   */
  i = Nam.nam$b_rsl;
#endif /* HO_VMS */
  /*
   *	Size of record
   */
  Local[0] = 1 + 1 + 1 + 1 + 1 + 2 + 8 + 4 + 2 + 1 + 1 + i + 1;
  /*
   *	Source declaration
   */
  Local[1] = DST_S_C_SOURCE;
  /*
   *	Make formfeeds count as source records
   */
  Local[2] = DST_S_C_SRC_FORMFEED;
  /*
   *	Declare source file
   */
  Local[3] = DST_S_C_SRC_DECLFILE;
  Local[4] = 1 + 2 + 8 + 4 + 2 + 1 + 1 + i + 1;
  cp = Local + 5;
  /*
   *	Flags
   */
  *cp++ = 0;
  /*
   *	File ID
   */
  *(short *) cp = ID_Number;
  cp += sizeof (short);
#ifndef HO_VMS
  /*
   *	Creation Date.  Unknown, so we fill with zeroes.
   */
  *(long *) cp = 0;
  cp += sizeof (long);
  *(long *) cp = 0;
  cp += sizeof (long);
  /*
   *	End of file block
   */
  *(long *) cp = 0;
  cp += sizeof (long);
  /*
   *	First free byte
   */
  *(short *) cp = 0;
  cp += sizeof (short);
  /*
   *	Record format
   */
  *cp++ = 0;
  /*
   *	Filename
   */
  *cp++ = i;
  cp1 = Filename;
#else /* Use this code when assembling for VMS on a VMS system */
  /*
   *	Creation Date
   */
  *(long *) cp = ((long *) &Date_Xab.xab$q_cdt)[0];
  cp += sizeof (long);
  *(long *) cp = ((long *) &Date_Xab.xab$q_cdt)[1];
  cp += sizeof (long);
  /*
   *	End of file block
   */
  *(long *) cp = File_Header_Xab.xab$l_ebk;
  cp += sizeof (long);
  /*
   *	First free byte
   */
  *(short *) cp = File_Header_Xab.xab$w_ffb;
  cp += sizeof (short);
  /*
   *	Record format
   */
  *cp++ = File_Header_Xab.xab$b_rfo;
  /*
   *	Filename
   */
  *cp++ = i;
  cp1 = Rs_String;
#endif /* HO_VMS */
  while (--i >= 0)
    *cp++ = *cp1++;
  /*
   *	Library module name (none)
   */
  *cp++ = 0;
  /*
   *	Done
   */
  VMS_Store_Immediate_Data (Local, cp - Local, OBJ_S_C_TBT);
  return 1;
}


/*
 *	Give the number of source lines to the debugger
 */
static
VMS_TBT_Source_Lines (ID_Number, Starting_Line_Number, Number_Of_Lines)
     int ID_Number;
     int Starting_Line_Number;
     int Number_Of_Lines;
{
  char *cp, *cp1;
  char Local[16];

  /*
   *	Size of record
   */
  Local[0] = 1 + 1 + 2 + 1 + 4 + 1 + 2;
  /*
   *	Source declaration
   */
  Local[1] = DST_S_C_SOURCE;
  /*
   *	Set Source File
   */
  cp = Local + 2;
  *cp++ = DST_S_C_SRC_SETFILE;
  /*
   *	File ID Number
   */
  *(short *) cp = ID_Number;
  cp += sizeof (short);
  /*
   *	Set record number
   */
  *cp++ = DST_S_C_SRC_SETREC_L;
  *(long *) cp = Starting_Line_Number;
  cp += sizeof (long);
  /*
   *	Define lines
   */
  *cp++ = DST_S_C_SRC_DEFLINES_W;
  *(short *) cp = Number_Of_Lines;
  cp += sizeof (short);
  /*
   *	Done
   */
  VMS_Store_Immediate_Data (Local, cp - Local, OBJ_S_C_TBT);
}




/* This routine locates a file in the list of files.  If an entry does not
 * exist, one is created.  For include files, a new entry is always created
 * such that inline functions can be properly debugged. */
static struct input_file *
find_file (sp)
     symbolS *sp;
{
  struct input_file *same_file;
  struct input_file *fpnt;
  same_file = (struct input_file *) NULL;
  for (fpnt = file_root; fpnt; fpnt = fpnt->next)
    {
      if (fpnt == (struct input_file *) NULL)
	break;
      if (fpnt->spnt == sp)
	return fpnt;
    };
  for (fpnt = file_root; fpnt; fpnt = fpnt->next)
    {
      if (fpnt == (struct input_file *) NULL)
	break;
      if (strcmp (S_GET_NAME (sp), fpnt->name) == 0)
	{
	  if (fpnt->flag == 1)
	    return fpnt;
	  same_file = fpnt;
	  break;
	};
    };
  fpnt = (struct input_file *) malloc (sizeof (struct input_file));
  if (file_root == (struct input_file *) NULL)
    file_root = fpnt;
  else
    {
      struct input_file *fpnt1;
      for (fpnt1 = file_root; fpnt1->next; fpnt1 = fpnt1->next) ;
      fpnt1->next = fpnt;
    };
  fpnt->next = (struct input_file *) NULL;
  fpnt->name = S_GET_NAME (sp);
  fpnt->min_line = 0x7fffffff;
  fpnt->max_line = 0;
  fpnt->offset = 0;
  fpnt->flag = 0;
  fpnt->file_number = 0;
  fpnt->spnt = sp;
  fpnt->same_file_fpnt = same_file;
  return fpnt;
}

/*
 * The following functions and definitions are used to generate object records
 * that will describe program variables to the VMS debugger.
 *
 * This file contains many of the routines needed to output debugging info into
 * the object file that the VMS debugger needs to understand symbols.  These
 * routines are called very late in the assembly process, and thus we can be
 * fairly lax about changing things, since the GSD and the TIR sections have
 * already been output.
 */


/* This routine converts a number string into an integer, and stops when it
 * sees an invalid character the return value is the address of the character
 * just past the last character read.  No error is generated.
 */
static char *
cvt_integer (str, rtn)
     char *str;
     int *rtn;
{
  int ival, neg;
  neg = *str == '-' ? ++str, -1 : 1;
  ival = 0;			/* first get the number of the type for dbx */
  while ((*str <= '9') && (*str >= '0'))
    ival = 10 * ival + *str++ - '0';
  *rtn = neg * ival;
  return str;
}

/* this routine fixes the names that are generated by C++, ".this" is a good
 * example.  The period does not work for the debugger, since it looks like
 * the syntax for a structure element, and thus it gets mightily confused
 *
 * We also use this to strip the PsectAttribute hack from the name before we
 * write a debugger record */

static char *
fix_name (pnt)
     char *pnt;
{
  char *pnt1;
  /*
   *	Kill any leading "_"
   */
  if (*pnt == '_')
    pnt++;
  /*
   *	Is there a Psect Attribute to skip??
   */
  if (HAS_PSECT_ATTRIBUTES (pnt))
    {
      /*
       *	Yes: Skip it
       */
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
/* Here we fix the .this -> $this conversion */
  for (pnt1 = pnt; *pnt1 != 0; pnt1++)
    {
      if (*pnt1 == '.')
	*pnt1 = '$';
    };
  return pnt;
}

/* When defining a structure, this routine is called to find the name of
 * the actual structure.  It is assumed that str points to the equal sign
 * in the definition, and it moves backward until it finds the start of the
 * name.  If it finds a 0, then it knows that this structure def is in the
 * outermost level, and thus symbol_name points to the symbol name.
 */
static char *
get_struct_name (str)
     char *str;
{
  char *pnt;
  pnt = str;
  while ((*pnt != ':') && (*pnt != '\0'))
    pnt--;
  if (*pnt == '\0')
    return symbol_name;
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

/* search symbol list for type number dbx_type.  Return a pointer to struct */
static struct VMS_DBG_Symbol *
find_symbol (dbx_type)
     int dbx_type;
{
  struct VMS_DBG_Symbol *spnt;
  spnt = VMS_Symbol_type_list;
  while (spnt != (struct VMS_DBG_Symbol *) NULL)
    {
      if (spnt->dbx_type == dbx_type)
	break;
      spnt = spnt->next;
    };
  if (spnt == (struct VMS_DBG_Symbol *) NULL)
    return 0;			/*Dunno what this is*/
  return spnt;
}


/* this routine puts info into either Local or Asuffix, depending on the sign
 * of size.  The reason is that it is easier to build the variable descriptor
 * backwards, while the array descriptor is best built forwards.  In the end
 * they get put together, if there is not a struct/union/enum along the way
 */
static
push (value, size)
     int value, size;
{
  char *pnt;
  int i;
  int size1;
  long int val;
  val = value;
  pnt = (char *) &val;
  size1 = size;
  if (size < 0)
    {
      size1 = -size;
      pnt += size1 - 1;
    };
  if (size < 0)
    for (i = 0; i < size1; i++)
      {
	Local[Lpnt--] = *pnt--;
	if (Lpnt < 0)
	  {
	    overflow = 1;
	    Lpnt = 1;
	  };
      }
  else
    for (i = 0; i < size1; i++)
      {
	Asuffix[Apoint++] = *pnt++;
	if (Apoint >= MAX_DEBUG_RECORD)
	  {
	    overflow = 1;
	    Apoint = MAX_DEBUG_RECORD - 1;
	  };
      }
}

/* this routine generates the array descriptor for a given array */
static
array_suffix (spnt2)
     struct VMS_DBG_Symbol *spnt2;
{
  struct VMS_DBG_Symbol *spnt;
  struct VMS_DBG_Symbol *spnt1;
  int rank;
  int total_size;
  int i;
  rank = 0;
  spnt = spnt2;
  while (spnt->advanced != ARRAY)
    {
      spnt = find_symbol (spnt->type2);
      if (spnt == (struct VMS_DBG_Symbol *) NULL)
	return;
    };
  spnt1 = spnt;
  spnt1 = spnt;
  total_size = 1;
  while (spnt1->advanced == ARRAY)
    {
      rank++;
      total_size *= (spnt1->index_max - spnt1->index_min + 1);
      spnt1 = find_symbol (spnt1->type2);
    };
  total_size = total_size * spnt1->data_size;
  push (spnt1->data_size, 2);
  if (spnt1->VMS_type == 0xa3)
    push (0, 1);
  else
    push (spnt1->VMS_type, 1);
  push (4, 1);
  for (i = 0; i < 6; i++)
    push (0, 1);
  push (0xc0, 1);
  push (rank, 1);
  push (total_size, 4);
  push (0, 4);
  spnt1 = spnt;
  while (spnt1->advanced == ARRAY)
    {
      push (spnt1->index_max - spnt1->index_min + 1, 4);
      spnt1 = find_symbol (spnt1->type2);
    };
  spnt1 = spnt;
  while (spnt1->advanced == ARRAY)
    {
      push (spnt1->index_min, 4);
      push (spnt1->index_max, 4);
      spnt1 = find_symbol (spnt1->type2);
    };
}

/* this routine generates the start of a variable descriptor based upon
 * a struct/union/enum that has yet to be defined.  We define this spot as
 * a new location, and save four bytes for the address.  When the struct is
 * finally defined, then we can go back and plug in the correct address
*/
static
new_forward_ref (dbx_type)
     int dbx_type;
{
  struct forward_ref *fpnt;
  fpnt = (struct forward_ref *) malloc (sizeof (struct forward_ref));
  fpnt->next = f_ref_root;
  f_ref_root = fpnt;
  fpnt->dbx_type = dbx_type;
  fpnt->struc_numb = ++structure_count;
  fpnt->resolved = 'N';
  push (3, -1);
  total_len = 5;
  push (total_len, -2);
  struct_number = -fpnt->struc_numb;
}

/* this routine generates the variable descriptor used to describe non-basic
 * variables.  It calls itself recursively until it gets to the bottom of it
 * all, and then builds the descriptor backwards.  It is easiest to do it this
 *way since we must periodically write length bytes, and it is easiest if we know
 *the value when it is time to write it.
 */
static int
gen1 (spnt, array_suffix_len)
     struct VMS_DBG_Symbol *spnt;
     int array_suffix_len;
{
  struct VMS_DBG_Symbol *spnt1;
  int i;
  switch (spnt->advanced)
    {
    case VOID:
      push (DBG_S_C_VOID, -1);
      total_len += 1;
      push (total_len, -2);
      return 0;
    case BASIC:
    case FUNCTION:
      if (array_suffix_len == 0)
	{
	  push (spnt->VMS_type, -1);
	  push (DBG_S_C_BASIC, -1);
	  total_len = 2;
	  push (total_len, -2);
	  return 1;
	};
      push (0, -4);
      push (0xfa02, -2);
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
      push (DBG_S_C_STRUCT, -1);
      total_len = 5;
      push (total_len, -2);
      return 1;
    case POINTER:
      spnt1 = find_symbol (spnt->type2);
      i = 1;
      if (spnt1 == (struct VMS_DBG_Symbol *) NULL)
	new_forward_ref (spnt->type2);
      else
	i = gen1 (spnt1, 0);
      if (i)
	{			/* (*void) is a special case, do not put pointer suffix*/
	  push (DBG_S_C_POINTER, -1);
	  total_len += 3;
	  push (total_len, -2);
	};
      return 1;
    case ARRAY:
      spnt1 = spnt;
      while (spnt1->advanced == ARRAY)
	{
	  spnt1 = find_symbol (spnt1->type2);
	  if (spnt1 == (struct VMS_DBG_Symbol *) NULL)
	    {
	      printf ("gcc-as warning(debugger output):");
	      printf ("Forward reference error, dbx type %d\n",
		      spnt->type2);
	      return;
	    }
	};
/* It is too late to generate forward references, so the user gets a message.
 * This should only happen on a compiler error */
      i = gen1 (spnt1, 1);
      i = Apoint;
      array_suffix (spnt);
      array_suffix_len = Apoint - i;
      switch (spnt1->advanced)
	{
	case BASIC:
	case FUNCTION:
	  break;
	default:
	  push (0, -2);
	  total_len += 2;
	  push (total_len, -2);
	  push (0xfa, -1);
	  push (0x0101, -2);
	  push (DBG_S_C_COMPLEX_ARRAY, -1);
	};
      total_len += array_suffix_len + 8;
      push (total_len, -2);
    };
}

/* This generates a suffix for a variable.  If it is not a defined type yet,
 * then dbx_type contains the type we are expecting so we can generate a
 * forward reference.  This calls gen1 to build most of the descriptor, and
 * then it puts the icing on at the end.  It then dumps whatever is needed
 * to get a complete descriptor (i.e. struct reference, array suffix ).
 */
static
generate_suffix (spnt, dbx_type)
     struct VMS_DBG_Symbol *spnt;
     int dbx_type;
{
  int ilen;
  int i;
  char pvoid[6] =
  {5, 0xaf, 0, 1, 0, 5};
  struct VMS_DBG_Symbol *spnt1;
  Apoint = 0;
  Lpnt = MAX_DEBUG_RECORD - 1;
  total_len = 0;
  struct_number = 0;
  overflow = 0;
  if (spnt == (struct VMS_DBG_Symbol *) NULL)
    new_forward_ref (dbx_type);
  else
    {
      if (spnt->VMS_type != 0xa3)
	return 0;		/* no suffix needed */
      gen1 (spnt, 0);
    };
  push (0x00af, -2);
  total_len += 4;
  push (total_len, -1);
/* if the variable descriptor overflows the record, output a descriptor for
 * a pointer to void.
 */
  if ((total_len >= MAX_DEBUG_RECORD) || overflow)
    {
      printf (" Variable descriptor %d too complicated. Defined as *void ", spnt->dbx_type);
      VMS_Store_Immediate_Data (pvoid, 6, OBJ_S_C_DBG);
      return;
    };
  i = 0;
  while (Lpnt < MAX_DEBUG_RECORD - 1)
    Local[i++] = Local[++Lpnt];
  Lpnt = i;
/* we use this for a reference to a structure that has already been defined */
  if (struct_number > 0)
    {
      VMS_Store_Immediate_Data (Local, Lpnt, OBJ_S_C_DBG);
      Lpnt = 0;
      VMS_Store_Struct (struct_number);
    };
/* we use this for a forward reference to a structure that has yet to be
*defined.  We store four bytes of zero to make room for the actual address once
* it is known
*/
  if (struct_number < 0)
    {
      struct_number = -struct_number;
      VMS_Store_Immediate_Data (Local, Lpnt, OBJ_S_C_DBG);
      Lpnt = 0;
      VMS_Def_Struct (struct_number);
      for (i = 0; i < 4; i++)
	Local[Lpnt++] = 0;
      VMS_Store_Immediate_Data (Local, Lpnt, OBJ_S_C_DBG);
      Lpnt = 0;
    };
  i = 0;
  while (i < Apoint)
    Local[Lpnt++] = Asuffix[i++];
  if (Lpnt != 0)
    VMS_Store_Immediate_Data (Local, Lpnt, OBJ_S_C_DBG);
  Lpnt = 0;
}

/* This routine generates a symbol definition for a C sybmol for the debugger.
 * It takes a psect and offset for global symbols - if psect < 0, then this is
 * a local variable and the offset is relative to FP.  In this case it can
 * be either a variable (Offset < 0) or a parameter (Offset > 0).
 */
static
VMS_DBG_record (spnt, Psect, Offset, Name)
     struct VMS_DBG_Symbol *spnt;
     int Psect;
     int Offset;
     char *Name;
{
  char *pnt;
  char *Name_pnt;
  int j;
  int maxlen;
  int i = 0;
  Name_pnt = fix_name (Name);	/* if there are bad characters in name, convert them */
  if (Psect < 0)
    {				/* this is a local variable, referenced to SP */
      maxlen = 7 + strlen (Name_pnt);
      Local[i++] = maxlen;
      Local[i++] = spnt->VMS_type;
      if (Offset > 0)
	Local[i++] = DBG_S_C_FUNCTION_PARAMETER;
      else
	Local[i++] = DBG_S_C_LOCAL_SYM;
      pnt = (char *) &Offset;
      for (j = 0; j < 4; j++)
	Local[i++] = *pnt++;	/* copy the offset */
    }
  else
    {
      maxlen = 7 + strlen (Name_pnt);	/* symbols fixed in memory */
      Local[i++] = 7 + strlen (Name_pnt);
      Local[i++] = spnt->VMS_type;
      Local[i++] = 1;
      VMS_Store_Immediate_Data (Local, i, OBJ_S_C_DBG);
      i = 0;
      VMS_Set_Data (Psect, Offset, OBJ_S_C_DBG, 0);
    }
  Local[i++] = strlen (Name_pnt);
  while (*Name_pnt != '\0')
    Local[i++] = *Name_pnt++;
  VMS_Store_Immediate_Data (Local, i, OBJ_S_C_DBG);
  if (spnt->VMS_type == DBG_S_C_ADVANCED_TYPE)
    generate_suffix (spnt, 0);
}


/* This routine parses the stabs entries in order to make the definition
 * for the debugger of local symbols and function parameters
 */
static int
VMS_local_stab_Parse (sp)
     symbolS *sp;
{
  char *pnt;
  char *pnt1;
  char *str;
  struct VMS_DBG_Symbol *spnt;
  struct VMS_Symbol *vsp;
  int dbx_type;
  int VMS_type;
  dbx_type = 0;
  str = S_GET_NAME (sp);
  pnt = (char *) strchr (str, ':');
  if (pnt == (char *) NULL)
    return;			/* no colon present */
  pnt1 = pnt++;			/* save this for later, and skip colon */
  if (*pnt == 'c')
    return 0;			/* ignore static constants */
/* there is one little catch that we must be aware of.  Sometimes function
 * parameters are optimized into registers, and the compiler, in its infiite
 * wisdom outputs stabs records for *both*.  In general we want to use the
 * register if it is present, so we must search the rest of the symbols for
 * this function to see if this parameter is assigned to a register.
 */
  {
    char *str1;
    char *pnt2;
    symbolS *sp1;
    if (*pnt == 'p')
      {
	for (sp1 = symbol_next (sp); sp1; sp1 = symbol_next (sp1))
	  {
	    if (!S_IS_DEBUG (sp1))
	      continue;
	    if (S_GET_RAW_TYPE (sp1) == N_FUN)
	      {
		char * pnt3=(char*) strchr (S_GET_NAME (sp1), ':') + 1;
		if (*pnt3 == 'F' || *pnt3 == 'f') break;
	      };
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
	      };
	    if ((*str1 != ':') || (*pnt2 != ':'))
	      continue;
	    return;		/* they are the same!  lets skip this one */
	  };			/* for */
/* first find the dbx symbol type from list, and then find VMS type */
	pnt++;			/* skip p in case no register */
      };			/* if */
  };				/* p block */
  pnt = cvt_integer (pnt, &dbx_type);
  spnt = find_symbol (dbx_type);
  if (spnt == (struct VMS_DBG_Symbol *) NULL)
    return 0;			/*Dunno what this is*/
  *pnt1 = '\0';
  VMS_DBG_record (spnt, -1, S_GET_VALUE (sp), str);
  *pnt1 = ':';			/* and restore the string */
  return 1;
}

/* This routine parses a stabs entry to find the information required to define
 * a variable.  It is used for global and static variables.
 * Basically we need to know the address of the symbol.  With older versions
 * of the compiler, const symbols are
 * treated differently, in that if they are global they are written into the
 * text psect.  The global symbol entry for such a const is actually written
 * as a program entry point (Yuk!!), so if we cannot find a symbol in the list
 * of psects, we must search the entry points as well.  static consts are even
 * harder, since they are never assigned a memory address.  The compiler passes
 * a stab to tell us the value, but I am not sure what to do with it.
 */

static
VMS_stab_parse (sp, expected_type, type1, type2, Text_Psect)
     symbolS *sp;
     char expected_type;
     int type1, type2, Text_Psect;
{
  char *pnt;
  char *pnt1;
  char *str;
  symbolS *sp1;
  struct VMS_DBG_Symbol *spnt;
  struct VMS_Symbol *vsp;
  int dbx_type;
  int VMS_type;
  dbx_type = 0;
  str = S_GET_NAME (sp);
  pnt = (char *) strchr (str, ':');
  if (pnt == (char *) NULL)
    return;			/* no colon present */
  pnt1 = pnt;			/* save this for later*/
  pnt++;
  if (*pnt == expected_type)
    {
      pnt = cvt_integer (pnt + 1, &dbx_type);
      spnt = find_symbol (dbx_type);
      if (spnt == (struct VMS_DBG_Symbol *) NULL)
	return 0;		/*Dunno what this is*/
/* now we need to search the symbol table to find the psect and offset for
 * this variable.
 */
      *pnt1 = '\0';
      vsp = VMS_Symbols;
      while (vsp != (struct VMS_Symbol *) NULL)
	{
	  pnt = S_GET_NAME (vsp->Symbol);
	  if (pnt != (char *) NULL)
	    if (*pnt++ == '_')
/* make sure name is the same, and make sure correct symbol type */
	      if ((strlen (pnt) == strlen (str)) && (strcmp (pnt, str) == 0)
		  && ((S_GET_RAW_TYPE (vsp->Symbol) == type1) ||
		      (S_GET_RAW_TYPE (vsp->Symbol) == type2)))
		break;
	  vsp = vsp->Next;
	};
      if (vsp != (struct VMS_Symbol *) NULL)
	{
	  VMS_DBG_record (spnt, vsp->Psect_Index, vsp->Psect_Offset, str);
	  *pnt1 = ':';		/* and restore the string */
	  return 1;
	};
/* the symbol was not in the symbol list, but it may be an "entry point"
   if it was a constant */
      for (sp1 = symbol_rootP; sp1; sp1 = symbol_next (sp1))
	{
	  /*
	   *	Dispatch on STAB type
	   */
	  if (S_IS_DEBUG (sp1) || (S_GET_TYPE (sp1) != N_TEXT))
	    continue;
	  pnt = S_GET_NAME (sp1);
	  if (*pnt == '_')
	    pnt++;
	  if (strcmp (pnt, str) == 0)
	    {
	      if (!gave_compiler_message && expected_type == 'G')
		{
		  printf ("***Warning - the assembly code generated by the compiler has placed\n");
		  printf ("global constant(s) in the text psect.  These will not be available to\n");
		  printf ("other modules, since this is not the correct way to handle this. You\n");
		  printf ("have two options: 1) get a patched compiler that does not put global\n");
		  printf ("constants in the text psect, or 2) remove the 'const' keyword from\n");
		  printf ("definitions of global variables in your source module(s).  Don't say\n");
		  printf ("I didn't warn you!");
		  gave_compiler_message = 1;
		};
	      VMS_DBG_record (spnt,
			      Text_Psect,
			      S_GET_VALUE (sp1),
			      str);
	      *pnt1 = ':';
	      *S_GET_NAME (sp1) = 'L';
	      /* fool assembler to not output this
	       * as a routine in the TBT */
	      return 1;
	    };
	};
    };
  *pnt1 = ':';			/* and restore the string */
  return 0;
}

static
VMS_GSYM_Parse (sp, Text_Psect)
     symbolS *sp;
     int Text_Psect;
{				/* Global variables */
  VMS_stab_parse (sp, 'G', (N_UNDF | N_EXT), (N_DATA | N_EXT), Text_Psect);
}


static
VMS_LCSYM_Parse (sp, Text_Psect)
     symbolS *sp;
     int Text_Psect;
{				/* Static symbols - uninitialized */
  VMS_stab_parse (sp, 'S', N_BSS, -1, Text_Psect);
}

static
VMS_STSYM_Parse (sp, Text_Psect)
     symbolS *sp;
     int Text_Psect;
{				/* Static symbols - initialized */
  VMS_stab_parse (sp, 'S', N_DATA, -1, Text_Psect);
}


/* for register symbols, we must figure out what range of addresses within the
 * psect are valid. We will use the brackets in the stab directives to give us
 * guidance as to the PC range that this variable is in scope.  I am still not
 * completely comfortable with this but as I learn more, I seem to get a better
 * handle on what is going on.
 * Caveat Emptor.
 */
static
VMS_RSYM_Parse (sp, Current_Routine, Text_Psect)
     symbolS *sp, *Current_Routine;
     int Text_Psect;
{
  char *pnt;
  char *pnt1;
  char *str;
  int dbx_type;
  struct VMS_DBG_Symbol *spnt;
  int j;
  int maxlen;
  int i = 0;
  int bcnt = 0;
  int Min_Offset = -1;		/* min PC of validity */
  int Max_Offset = 0;		/* max PC of validity */
  symbolS *symbolP;
  for (symbolP = sp; symbolP; symbolP = symbol_next (symbolP))
    {
      /*
       *	Dispatch on STAB type
       */
      switch (S_GET_RAW_TYPE (symbolP))
	{
	case N_LBRAC:
	  if (bcnt++ == 0)
	    Min_Offset = S_GET_VALUE (symbolP);
	  break;
	case N_RBRAC:
	  if (--bcnt == 0)
	    Max_Offset =
	      S_GET_VALUE (symbolP) - 1;
	  break;
	}
      if ((Min_Offset != -1) && (bcnt == 0))
	break;
      if (S_GET_RAW_TYPE (symbolP) == N_FUN)
	{
	  pnt=(char*) strchr (S_GET_NAME (symbolP), ':') + 1;
	  if (*pnt == 'F' || *pnt == 'f') break;
	};
    }
/* check to see that the addresses were defined.  If not, then there were no
 * brackets in the function, and we must try to search for the next function
 * Since functions can be in any order, we should search all of the symbol list
 * to find the correct ending address. */
  if (Min_Offset == -1)
    {
      int Max_Source_Offset;
      int This_Offset;
      Min_Offset = S_GET_VALUE (sp);
      for (symbolP = symbol_rootP; symbolP; symbolP = symbol_next (symbolP))
	{
	  /*
	   *	Dispatch on STAB type
	   */
	  This_Offset = S_GET_VALUE (symbolP);
	  switch (S_GET_RAW_TYPE (symbolP))
	    {
	    case N_TEXT | N_EXT:
	      if ((This_Offset > Min_Offset) && (This_Offset < Max_Offset))
		Max_Offset = This_Offset;
	      break;
	    case N_SLINE:
	      if (This_Offset > Max_Source_Offset)
		Max_Source_Offset = This_Offset;
	    }
	}
/* if this is the last routine, then we use the PC of the last source line
 * as a marker of the max PC for which this reg is valid */
      if (Max_Offset == 0x7fffffff)
	Max_Offset = Max_Source_Offset;
    };
  dbx_type = 0;
  str = S_GET_NAME (sp);
  pnt = (char *) strchr (str, ':');
  if (pnt == (char *) NULL)
    return;			/* no colon present */
  pnt1 = pnt;			/* save this for later*/
  pnt++;
  if (*pnt != 'r')
    return 0;
  pnt = cvt_integer (pnt + 1, &dbx_type);
  spnt = find_symbol (dbx_type);
  if (spnt == (struct VMS_DBG_Symbol *) NULL)
    return 0;			/*Dunno what this is yet*/
  *pnt1 = '\0';
  pnt = fix_name (S_GET_NAME (sp));	/* if there are bad characters in name, convert them */
  maxlen = 25 + strlen (pnt);
  Local[i++] = maxlen;
  Local[i++] = spnt->VMS_type;
  Local[i++] = 0xfb;
  Local[i++] = strlen (pnt) + 1;
  Local[i++] = 0x00;
  Local[i++] = 0x00;
  Local[i++] = 0x00;
  Local[i++] = strlen (pnt);
  while (*pnt != '\0')
    Local[i++] = *pnt++;
  Local[i++] = 0xfd;
  Local[i++] = 0x0f;
  Local[i++] = 0x00;
  Local[i++] = 0x03;
  Local[i++] = 0x01;
  VMS_Store_Immediate_Data (Local, i, OBJ_S_C_DBG);
  i = 0;
  VMS_Set_Data (Text_Psect, Min_Offset, OBJ_S_C_DBG, 1);
  VMS_Set_Data (Text_Psect, Max_Offset, OBJ_S_C_DBG, 1);
  Local[i++] = 0x03;
  Local[i++] = S_GET_VALUE (sp);
  Local[i++] = 0x00;
  Local[i++] = 0x00;
  Local[i++] = 0x00;
  VMS_Store_Immediate_Data (Local, i, OBJ_S_C_DBG);
  *pnt1 = ':';
  if (spnt->VMS_type == DBG_S_C_ADVANCED_TYPE)
    generate_suffix (spnt, 0);
}

/* this function examines a structure definition, checking all of the elements
 * to make sure that all of them are fully defined.  The only thing that we
 * kick out are arrays of undefined structs, since we do not know how big
 * they are.  All others we can handle with a normal forward reference.
 */
static int
forward_reference (pnt)
     char *pnt;
{
  int i;
  struct VMS_DBG_Symbol *spnt;
  struct VMS_DBG_Symbol *spnt1;
  pnt = cvt_integer (pnt + 1, &i);
  if (*pnt == ';')
    return 0;			/* no forward references */
  do
    {
      pnt = (char *) strchr (pnt, ':');
      pnt = cvt_integer (pnt + 1, &i);
      spnt = find_symbol (i);
      if (spnt == (struct VMS_DBG_Symbol *) NULL)
	return 0;
      while ((spnt->advanced == POINTER) || (spnt->advanced == ARRAY))
	{
	  i = spnt->type2;
	  spnt1 = find_symbol (spnt->type2);
	  if ((spnt->advanced == ARRAY) &&
	      (spnt1 == (struct VMS_DBG_Symbol *) NULL))
	    return 1;
	  if (spnt1 == (struct VMS_DBG_Symbol *) NULL)
	    break;
	  spnt = spnt1;
	};
      pnt = cvt_integer (pnt + 1, &i);
      pnt = cvt_integer (pnt + 1, &i);
  } while (*++pnt != ';');
  return 0;			/* no forward refences found */
}

/* This routine parses the stabs directives to find any definitions of dbx type
 * numbers.  It makes a note of all of them, creating a structure element
 * of VMS_DBG_Symbol that describes it.  This also generates the info for the
 * debugger that describes the struct/union/enum, so that further references
 * to these data types will be by number
 * 	We have to process pointers right away, since there can be references
 * to them later in the same stabs directive.  We cannot have forward
 * references to pointers, (but we can have a forward reference to a pointer to
 * a structure/enum/union) and this is why we process them immediately.
 * After we process the pointer, then we search for defs that are nested even
 * deeper.
 */
static int
VMS_typedef_parse (str)
     char *str;
{
  char *pnt;
  char *pnt1;
  char *pnt2;
  int i;
  int dtype;
  struct forward_ref *fpnt;
  int i1, i2, i3;
  int convert_integer;
  struct VMS_DBG_Symbol *spnt;
  struct VMS_DBG_Symbol *spnt1;
/* check for any nested def's */
  pnt = (char *) strchr (str + 1, '=');
  if ((pnt != (char *) NULL) && (*(str + 1) != '*'))
    if (VMS_typedef_parse (pnt) == 1)
      return 1;
/* now find dbx_type of entry */
  pnt = str - 1;
  if (*pnt == 'c')
    {				/* check for static constants */
      *str = '\0';		/* for now we ignore them */
      return 0;
    };
  while ((*pnt <= '9') && (*pnt >= '0'))
    pnt--;
  pnt++;			/* and get back to the number */
  cvt_integer (pnt, &i1);
  spnt = find_symbol (i1);
/* first we see if this has been defined already, due to a forward reference*/
  if (spnt == (struct VMS_DBG_Symbol *) NULL)
    {
      if (VMS_Symbol_type_list == (struct VMS_DBG_Symbol *) NULL)
	{
	  spnt = (struct VMS_DBG_Symbol *) malloc (sizeof (struct VMS_DBG_Symbol));
	  spnt->next = (struct VMS_DBG_Symbol *) NULL;
	  VMS_Symbol_type_list = spnt;
	}
      else
	{
	  spnt = (struct VMS_DBG_Symbol *) malloc (sizeof (struct VMS_DBG_Symbol));
	  spnt->next = VMS_Symbol_type_list;
	  VMS_Symbol_type_list = spnt;
	};
      spnt->dbx_type = i1;	/* and save the type */
    };
/* for structs and unions, do a partial parse, otherwise we sometimes get
 * circular definitions that are impossible to resolve. We read enough info
 * so that any reference to this type has enough info to be resolved
 */
  pnt = str + 1;		/* point to character past equal sign */
  if ((*pnt == 'u') || (*pnt == 's'))
    {
    };
  if ((*pnt <= '9') && (*pnt >= '0'))
    {
      if (type_check ("void"))
	{			/* this is the void symbol */
	  *str = '\0';
	  spnt->advanced = VOID;
	  return 0;
	};
      if (type_check ("unknown type"))
	{			/* this is the void symbol */
	  *str = '\0';
	  spnt->advanced = UNKNOWN;
	  return 0;
	};
      printf ("gcc-as warning(debugger output):");
      printf (" %d is an unknown untyped variable.\n", spnt->dbx_type);
      return 1;			/* do not know what this is */
    };
/* now define this module*/
  pnt = str + 1;		/* point to character past equal sign */
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
	  spnt->VMS_type = DBG_S_C_REAL8;
	  spnt->data_size = 8;
	}
      pnt1 = (char *) strchr (str, ';') + 1;
      break;
    case 's':
    case 'u':
      if (*pnt == 's')
	spnt->advanced = STRUCT;
      else
	spnt->advanced = UNION;
      spnt->VMS_type = DBG_S_C_ADVANCED_TYPE;
      pnt1 = cvt_integer (pnt + 1, &spnt->data_size);
      if (forward_reference (pnt))
	{
	  spnt->struc_numb = -1;
	  return 1;
	}
      spnt->struc_numb = ++structure_count;
      pnt1--;
      pnt = get_struct_name (str);
      VMS_Def_Struct (spnt->struc_numb);
      fpnt = f_ref_root;
      while (fpnt != (struct forward_ref *) NULL)
	{
	  if (fpnt->dbx_type == spnt->dbx_type)
	    {
	      fpnt->resolved = 'Y';
	      VMS_Set_Struct (fpnt->struc_numb);
	      VMS_Store_Struct (spnt->struc_numb);
	    };
	  fpnt = fpnt->next;
	};
      VMS_Set_Struct (spnt->struc_numb);
      i = 0;
      Local[i++] = 11 + strlen (pnt);
      Local[i++] = DBG_S_C_STRUCT_START;
      Local[i++] = 0x80;
      for (i1 = 0; i1 < 4; i1++)
	Local[i++] = 0x00;
      Local[i++] = strlen (pnt);
      pnt2 = pnt;
      while (*pnt2 != '\0')
	Local[i++] = *pnt2++;
      i2 = spnt->data_size * 8;	/* number of bits */
      pnt2 = (char *) &i2;
      for (i1 = 0; i1 < 4; i1++)
	Local[i++] = *pnt2++;
      VMS_Store_Immediate_Data (Local, i, OBJ_S_C_DBG);
      i = 0;
      if (pnt != symbol_name)
	{
	  pnt += strlen (pnt);
	  *pnt = ':';
	};			/* replace colon for later */
      while (*++pnt1 != ';')
	{
	  pnt = (char *) strchr (pnt1, ':');
	  *pnt = '\0';
	  pnt2 = pnt1;
	  pnt1 = cvt_integer (pnt + 1, &dtype);
	  pnt1 = cvt_integer (pnt1 + 1, &i2);
	  pnt1 = cvt_integer (pnt1 + 1, &i3);
	  if ((dtype == 1) && (i3 != 32))
	    {			/* bitfield */
	      Apoint = 0;
	      push (19 + strlen (pnt2), 1);
	      push (0xfa22, 2);
	      push (1 + strlen (pnt2), 4);
	      push (strlen (pnt2), 1);
	      while (*pnt2 != '\0')
		push (*pnt2++, 1);
	      push (i3, 2);	/* size of bitfield */
	      push (0x0d22, 2);
	      push (0x00, 4);
	      push (i2, 4);	/* start position */
	      VMS_Store_Immediate_Data (Asuffix, Apoint, OBJ_S_C_DBG);
	      Apoint = 0;
	    }
	  else
	    {
	      Local[i++] = 7 + strlen (pnt2);
	      spnt1 = find_symbol (dtype);
	      /* check if this is a forward reference */
	      if (spnt1 != (struct VMS_DBG_Symbol *) NULL)
		Local[i++] = spnt1->VMS_type;
	      else
		Local[i++] = DBG_S_C_ADVANCED_TYPE;
	      Local[i++] = DBG_S_C_STRUCT_ITEM;
	      pnt = (char *) &i2;
	      for (i1 = 0; i1 < 4; i1++)
		Local[i++] = *pnt++;
	      Local[i++] = strlen (pnt2);
	      while (*pnt2 != '\0')
		Local[i++] = *pnt2++;
	      VMS_Store_Immediate_Data (Local, i, OBJ_S_C_DBG);
	      i = 0;
	      if (spnt1 == (struct VMS_DBG_Symbol *) NULL)
		generate_suffix (spnt1, dtype);
	      else if (spnt1->VMS_type == DBG_S_C_ADVANCED_TYPE)
		generate_suffix (spnt1, 0);
	    };
	};
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
      fpnt = f_ref_root;
      while (fpnt != (struct forward_ref *) NULL)
	{
	  if (fpnt->dbx_type == spnt->dbx_type)
	    {
	      fpnt->resolved = 'Y';
	      VMS_Set_Struct (fpnt->struc_numb);
	      VMS_Store_Struct (spnt->struc_numb);
	    };
	  fpnt = fpnt->next;
	};
      VMS_Set_Struct (spnt->struc_numb);
      i = 0;
      Local[i++] = 3 + strlen (symbol_name);
      Local[i++] = DBG_S_C_ENUM_START;
      Local[i++] = 0x20;
      Local[i++] = strlen (symbol_name);
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
	  Local[i++] = 7 + strlen (pnt);
	  Local[i++] = DBG_S_C_ENUM_ITEM;
	  Local[i++] = 0x00;
	  pnt2 = (char *) &i1;
	  for (i2 = 0; i2 < 4; i2++)
	    Local[i++] = *pnt2++;
	  Local[i++] = strlen (pnt);
	  pnt2 = pnt;
	  while (*pnt != '\0')
	    Local[i++] = *pnt++;
	  VMS_Store_Immediate_Data (Local, i, OBJ_S_C_DBG);
	  i = 0;
	  pnt = pnt1;		/* Skip final semicolon */
	};
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
      if (pnt == (char *) NULL)
	return 1;
      pnt1 = cvt_integer (pnt + 1, &spnt->index_min);
      pnt1 = cvt_integer (pnt1 + 1, &spnt->index_max);
      pnt1 = cvt_integer (pnt1 + 1, &spnt->type2);
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
      if ((pnt != (char *) NULL))
	if (VMS_typedef_parse (pnt) == 1)
	  return 1;
      break;
    default:
      spnt->advanced = UNKNOWN;
      spnt->VMS_type = 0;
      printf ("gcc-as warning(debugger output):");
      printf (" %d is an unknown type of variable.\n", spnt->dbx_type);
      return 1;			/* unable to decipher */
    };
/* this removes the evidence of the definition so that the outer levels of
parsing do not have to worry about it */
  pnt = str;
  while (*pnt1 != '\0')
    *pnt++ = *pnt1++;
  *pnt = '\0';
  return 0;
}


/*
 * This is the root routine that parses the stabs entries for definitions.
 * it calls VMS_typedef_parse, which can in turn call itself.
 * We need to be careful, since sometimes there are forward references to
 * other symbol types, and these cannot be resolved until we have completed
 * the parse.
 */
static int
VMS_LSYM_Parse ()
{
  char *pnt;
  char *pnt1;
  char *pnt2;
  char *str;
  char fixit[10];
  int incomplete, i, pass, incom1;
  struct VMS_DBG_Symbol *spnt;
  struct VMS_Symbol *vsp;
  struct forward_ref *fpnt;
  symbolS *sp;
  pass = 0;
  incomplete = 0;
  do
    {
      incom1 = incomplete;
      incomplete = 0;
      for (sp = symbol_rootP; sp; sp = symbol_next (sp))
	{
	  /*
	   *	Deal with STAB symbols
	   */
	  if (S_IS_DEBUG (sp))
	    {
	      /*
	       *	Dispatch on STAB type
	       */
	      switch (S_GET_RAW_TYPE (sp))
		{
		case N_GSYM:
		case N_LCSYM:
		case N_STSYM:
		case N_PSYM:
		case N_RSYM:
		case N_LSYM:
		case N_FUN:	/*sometimes these contain typedefs*/
		  str = S_GET_NAME (sp);
		  symbol_name = str;
		  pnt = (char *) strchr (str, ':');
		  if (pnt == (char *) NULL)
		    break;
		  *pnt = '\0';
		  pnt1 = pnt + 1;
		  pnt2 = (char *) strchr (pnt1, '=');
		  if (pnt2 == (char *) NULL)
		    {
		      *pnt = ':';	/* replace colon */
		      break;
		    };		/* no symbol here */
		  incomplete += VMS_typedef_parse (pnt2);
		  *pnt = ':';	/* put back colon so variable def code finds dbx_type*/
		  break;
		}		/*switch*/
	    }			/* if */
	}			/*for*/
      pass++;
  } while ((incomplete != 0) && (incomplete != incom1));
  /* repeat until all refs resolved if possible */
/*	if (pass > 1) printf(" Required %d passes\n",pass);*/
  if (incomplete != 0)
    {
      printf ("gcc-as warning(debugger output):");
      printf ("Unable to resolve %d circular references.\n", incomplete);
    };
  fpnt = f_ref_root;
  symbol_name = "\0";
  while (fpnt != (struct forward_ref *) NULL)
    {
      if (fpnt->resolved != 'Y')
	{
	  if (find_symbol (fpnt->dbx_type) !=
	      (struct VMS_DBG_Symbol *) NULL)
	    {
	      printf ("gcc-as warning(debugger output):");
	      printf ("Forward reference error, dbx type %d\n",
		      fpnt->dbx_type);
	      break;
	    };
	  fixit[0] = 0;
	  sprintf (&fixit[1], "%d=s4;", fpnt->dbx_type);
	  pnt2 = (char *) strchr (&fixit[1], '=');
	  VMS_typedef_parse (pnt2);
	};
      fpnt = fpnt->next;
    };
}

static
Define_Local_Symbols (s1, s2)
     symbolS *s1, *s2;
{
  symbolS *symbolP1;
  for (symbolP1 = symbol_next (s1); symbolP1 != s2; symbolP1 = symbol_next (symbolP1))
    {
      if (symbolP1 == (symbolS *) NULL)
	return;
      if (S_GET_RAW_TYPE (symbolP1) == N_FUN)
	{
	  char * pnt=(char*) strchr (S_GET_NAME (symbolP1), ':') + 1;
	  if (*pnt == 'F' || *pnt == 'f') break;
	};
      /*
       *	Deal with STAB symbols
       */
      if (S_IS_DEBUG (symbolP1))
	{
	  /*
	   *	Dispatch on STAB type
	   */
	  switch (S_GET_RAW_TYPE (symbolP1))
	    {
	    case N_LSYM:
	    case N_PSYM:
	      VMS_local_stab_Parse (symbolP1);
	      break;
	    case N_RSYM:
	      VMS_RSYM_Parse (symbolP1, Current_Routine, Text_Psect);
	      break;
	    }			/*switch*/
	}			/* if */
    }				/* for */
}


/* This function crawls the symbol chain searching for local symbols that need
 * to be described to the debugger.  When we enter a new scope with a "{", it
 * creates a new "block", which helps the debugger keep track of which scope
 * we are currently in.
 */

static symbolS *
Define_Routine (symbolP, Level)
     symbolS *symbolP;
     int Level;
{
  symbolS *sstart;
  symbolS *symbolP1;
  char str[10];
  int rcount = 0;
  int Offset;
  sstart = symbolP;
  for (symbolP1 = symbol_next (symbolP); symbolP1; symbolP1 = symbol_next (symbolP1))
    {
      if (S_GET_RAW_TYPE (symbolP1) == N_FUN)
	{
	  char * pnt=(char*) strchr (S_GET_NAME (symbolP1), ':') + 1;
	  if (*pnt == 'F' || *pnt == 'f') break;
	};
      /*
       *	Deal with STAB symbols
       */
      if (S_IS_DEBUG (symbolP1))
	{
	  /*
	   *	Dispatch on STAB type
	   */
	  switch (S_GET_RAW_TYPE (symbolP1))
	    {
	    case N_LBRAC:
	      if (Level != 0)
		{
		  sprintf (str, "$%d", rcount++);
		  VMS_TBT_Block_Begin (symbolP1, Text_Psect, str);
		};
	      Offset = S_GET_VALUE (symbolP1);
	      Define_Local_Symbols (sstart, symbolP1);
	      symbolP1 =
		Define_Routine (symbolP1, Level + 1);
	      if (Level != 0)
		VMS_TBT_Block_End (S_GET_VALUE (symbolP1) -
				   Offset);
	      sstart = symbolP1;
	      break;
	    case N_RBRAC:
	      return symbolP1;
	    }			/*switch*/
	}			/* if */
    }				/* for */
  /* we end up here if there were no brackets in this function. Define
everything */
  Define_Local_Symbols (sstart, (symbolS *) 0);
  return symbolP1;
}


static
VMS_DBG_Define_Routine (symbolP, Curr_Routine, Txt_Psect)
     symbolS *symbolP;
     symbolS *Curr_Routine;
     int Txt_Psect;
{
  Current_Routine = Curr_Routine;
  Text_Psect = Txt_Psect;
  Define_Routine (symbolP, 0);
}




#ifndef HO_VMS
#include <sys/types.h>
#include <time.h>

/* Manufacure a VMS like time on a unix based system. */
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

#endif /* not HO_VMS */
/*
 *	Write the MHD (Module Header) records
 */
static
Write_VMS_MHD_Records ()
{
  register char *cp, *cp1;
  register int i;
  struct
  {
    int Size;
    char *Ptr;
  } Descriptor;
  char Module_Name[256];
  char Now[18];

  /*
   *	We are writing a module header record
   */
  Set_VMS_Object_File_Record (OBJ_S_C_HDR);
  /*
   *	***************************
   *	*MAIN MODULE HEADER RECORD*
   *	***************************
   *
   *	Store record type and header type
   */
  PUT_CHAR (OBJ_S_C_HDR);
  PUT_CHAR (MHD_S_C_MHD);
  /*
   *	Structure level is 0
   */
  PUT_CHAR (OBJ_S_C_STRLVL);
  /*
   *	Maximum record size is size of the object record buffer
   */
  PUT_SHORT (sizeof (Object_Record_Buffer));
  /*
   *	Get module name (the FILENAME part of the object file)
   */
  cp = out_file_name;
  cp1 = Module_Name;
  while (*cp)
    {
      if ((*cp == ']') || (*cp == '>') ||
	  (*cp == ':') || (*cp == '/'))
	{
	  cp1 = Module_Name;
	  cp++;
	  continue;
	}
      *cp1++ = islower (*cp) ? toupper (*cp++) : *cp++;
    }
  *cp1 = 0;
  /*
   *	Limit it to 31 characters and store in the object record
   */
  while (--cp1 >= Module_Name)
    if (*cp1 == '.')
      *cp1 = 0;
  if (strlen (Module_Name) > 31)
    {
      if (flagseen['+'])
	printf ("%s: Module name truncated: %s\n", myname, Module_Name);
      Module_Name[31] = 0;
    }
  PUT_COUNTED_STRING (Module_Name);
  /*
   *	Module Version is "V1.0"
   */
  PUT_COUNTED_STRING ("V1.0");
  /*
   *	Creation time is "now" (17 chars of time string)
   */
#ifndef HO_VMS
  get_VMS_time_on_unix (&Now[0]);
#else /* HO_VMS */
  Descriptor.Size = 17;
  Descriptor.Ptr = Now;
  sys$asctim (0, &Descriptor, 0, 0);
#endif /* HO_VMS */
  for (i = 0; i < 17; i++)
    PUT_CHAR (Now[i]);
  /*
   *	Patch time is "never" (17 zeros)
   */
  for (i = 0; i < 17; i++)
    PUT_CHAR (0);
  /*
   *	Flush the record
   */
  Flush_VMS_Object_Record_Buffer ();
  /*
   *	*************************
   *	*LANGUAGE PROCESSOR NAME*
   *	*************************
   *
   *	Store record type and header type
   */
  PUT_CHAR (OBJ_S_C_HDR);
  PUT_CHAR (MHD_S_C_LNM);
  /*
   *	Store language processor name and version
   *	(not a counted string!)
   */
  cp = compiler_version_string;
  if (cp == 0)
    {
      cp = "GNU AS  V";
      while (*cp)
	PUT_CHAR (*cp++);
      cp = strchr (&version_string, '.');
      while (*cp != ' ')
	cp--;
      cp++;
    };
  while (*cp >= 32)
    PUT_CHAR (*cp++);
  /*
   *	Flush the record
   */
  Flush_VMS_Object_Record_Buffer ();
}


/*
 *	Write the EOM (End Of Module) record
 */
static
Write_VMS_EOM_Record (Psect, Offset)
     int Psect;
     int Offset;
{
  /*
   *	We are writing an end-of-module record
   */
  Set_VMS_Object_File_Record (OBJ_S_C_EOM);
  /*
   *	Store record Type
   */
  PUT_CHAR (OBJ_S_C_EOM);
  /*
   *	Store the error severity (0)
   */
  PUT_CHAR (0);
  /*
   *	Store the entry point, if it exists
   */
  if (Psect >= 0)
    {
      /*
       *	Store the entry point Psect
       */
      PUT_CHAR (Psect);
      /*
       *	Store the entry point Psect offset
       */
      PUT_LONG (Offset);
    }
  /*
   *	Flush the record
   */
  Flush_VMS_Object_Record_Buffer ();
}


/* this hash routine borrowed from GNU-EMACS, and strengthened slightly  ERY*/

static int
hash_string (ptr)
     unsigned char *ptr;
{
  register unsigned char *p = ptr;
  register unsigned char *end = p + strlen (ptr);
  register unsigned char c;
  register int hash = 0;

  while (p != end)
    {
      c = *p++;
      hash = ((hash << 3) + (hash << 15) + (hash >> 28) + c);
    }
  return hash;
}

/*
 *	Generate a Case-Hacked VMS symbol name (limited to 31 chars)
 */
static
VMS_Case_Hack_Symbol (In, Out)
     register char *In;
     register char *Out;
{
  long int init = 0;
  long int result;
  char *pnt;
  char *new_name;
  char *old_name;
  register int i;
  int destructor = 0;		/*hack to allow for case sens in a destructor*/
  int truncate = 0;
  int Case_Hack_Bits = 0;
  int Saw_Dollar = 0;
  static char Hex_Table[16] =
  {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};

  /*
   *	Kill any leading "_"
   */
  if ((In[0] == '_') && ((In[1] > '9') || (In[1] < '0')))
    In++;

  new_name = Out;		/* save this for later*/

#if barfoo			/* Dead code */
  if ((In[0] == '_') && (In[1] == '$') && (In[2] == '_'))
    destructor = 1;
#endif

  /* We may need to truncate the symbol, save the hash for later*/
  if (strlen (In) > 23)
    result = hash_string (In);
  /*
   *	Is there a Psect Attribute to skip??
   */
  if (HAS_PSECT_ATTRIBUTES (In))
    {
      /*
       *	Yes: Skip it
       */
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
/*	if (strlen(In) > 31 && flagseen['+'])
		printf("%s: Symbol name truncated: %s\n",myname,In);*/
  /*
   *	Do the case conversion
   */
  i = 23;			/* Maximum of 23 chars */
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
	  if (isupper(*In)) {
	    *Out++ = *In++;
	    Case_Hack_Bits |= 1;
	  } else {
	    *Out++ = islower(*In) ? toupper(*In++) : *In++;
	  }
	  break;
	case 3: *Out++ = *In++;
	  break;
	case 2:
	  if (islower(*In)) {
	    *Out++ = *In++;
	  } else {
	    *Out++ = isupper(*In) ? tolower(*In++) : *In++;
	  }
	  break;
	};
    }
  /*
   *	If we saw a dollar sign, we don't do case hacking
   */
  if (flagseen['h'] || Saw_Dollar)
    Case_Hack_Bits = 0;

  /*
   *	If we have more than 23 characters and everything is lowercase
   *	we can insert the full 31 characters
   */
  if (*In)
    {
      /*
       *	We  have more than 23 characters
       * If we must add the case hack, then we have truncated the str
       */
      pnt = Out;
      truncate = 1;
      if (Case_Hack_Bits == 0)
	{
	  /*
	   *	And so far they are all lower case:
	   *		Check up to 8 more characters
	   *		and ensure that they are lowercase
	   */
	  for (i = 0; (In[i] != 0) && (i < 8); i++)
	    if (isupper(In[i]) && !Saw_Dollar && !flagseen['h'])
	      break;

	  if (In[i] == 0)
	    truncate = 0;

	  if ((i == 8) || (In[i] == 0))
	    {
	      /*
	       *	They are:  Copy up to 31 characters
	       *			to the output string
	       */
	      i = 8;
	      while ((--i >= 0) && (*In))
		switch (vms_name_mapping){
		case 0: *Out++ = islower(*In) ?
		  toupper (*In++) :
		    *In++;
		  break;
		case 3: *Out++ = *In++;
		  break;
		case 2: *Out++ = isupper(*In) ?
		  tolower(*In++) :
		    *In++;
		  break;
		};
	    }
	}
    }
  /*
   *	If there were any uppercase characters in the name we
   *	take on the case hacking string
   */

  /* Old behavior for regular GNU-C compiler */
  if (!flagseen['+'])
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
	  Out = pnt;		/*Cut back to 23 characters maximum */
	  *Out++ = '_';
	  for (i = 0; i < 7; i++)
	    {
	      init = result & 0x01f;
	      if (init < 10)
		*Out++ = '0' + init;
	      else
		*Out++ = 'A' + init - 10;
	      result = result >> 5;
	    }
	}
    }				/*Case Hack */
  /*
   *	Done
   */
  *Out = 0;
  if (truncate == 1 && flagseen['+'] && flagseen['H'])
    printf ("%s: Symbol %s replaced by %s\n", myname, old_name, new_name);
}


/*
 *	Scan a symbol name for a psect attribute specification
 */
#define GLOBALSYMBOL_BIT	0x10000
#define GLOBALVALUE_BIT		0x20000


static
VMS_Modify_Psect_Attributes (Name, Attribute_Pointer)
     char *Name;
     int *Attribute_Pointer;
{
  register int i;
  register char *cp;
  int Negate;
  static struct
  {
    char *Name;
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

  /*
   *	Kill leading "_"
   */
  if (*Name == '_')
    Name++;
  /*
   *	Check for a PSECT attribute list
   */
  if (!HAS_PSECT_ATTRIBUTES (Name))
    return;			/* If not, return */
  /*
   *	Skip the attribute list indicator
   */
  Name += PSECT_ATTRIBUTES_STRING_LENGTH;
  /*
   *	Process the attributes ("_" separated, "$" terminated)
   */
  while (*Name != '$')
    {
      /*
       *	Assume not negating
       */
      Negate = 0;
      /*
       *	Check for "NO"
       */
      if ((Name[0] == 'N') && (Name[1] == 'O'))
	{
	  /*
	   *	We are negating (and skip the NO)
	   */
	  Negate = 1;
	  Name += 2;
	}
      /*
       *	Find the token delimiter
       */
      cp = Name;
      while (*cp && (*cp != '_') && (*cp != '$'))
	cp++;
      /*
       *	Look for the token in the attribute list
       */
      for (i = 0; Attributes[i].Name; i++)
	{
	  /*
	   *	If the strings match, set/clear the attr.
	   */
	  if (strncmp (Name, Attributes[i].Name, cp - Name) == 0)
	    {
	      /*
	       *	Set or clear
	       */
	      if (Negate)
		*Attribute_Pointer &=
		  ~Attributes[i].Value;
	      else
		*Attribute_Pointer |=
		  Attributes[i].Value;
	      /*
	       *	Done
	       */
	      break;
	    }
	}
      /*
       *	Now skip the attribute
       */
      Name = cp;
      if (*Name == '_')
	Name++;
    }
  /*
   *	Done
   */
  return;
}


/*
 *	Define a global symbol
 */
static
VMS_Global_Symbol_Spec (Name, Psect_Number, Psect_Offset, Defined)
     char *Name;
     int Psect_Number;
     int Psect_Offset;
{
  char Local[32];

  /*
   *	We are writing a GSD record
   */
  Set_VMS_Object_File_Record (OBJ_S_C_GSD);
  /*
   *	If the buffer is empty we must insert the GSD record type
   */
  if (Object_Record_Offset == 0)
    PUT_CHAR (OBJ_S_C_GSD);
  /*
   *	We are writing a Global symbol definition subrecord
   */
  if (Psect_Number <= 255)
    {
      PUT_CHAR (GSD_S_C_SYM);
    }
  else
    {
      PUT_CHAR (GSD_S_C_SYMW);
    }
  /*
   *	Data type is undefined
   */
  PUT_CHAR (0);
  /*
   *	Switch on Definition/Reference
   */
  if ((Defined & 1) != 0)
    {
      /*
       *	Definition:
       *	Flags = "RELOCATABLE" and "DEFINED" for regular symbol
       *	      = "DEFINED" for globalvalue (Defined & 2 == 1)
       */
      if ((Defined & 2) == 0)
	{
	  PUT_SHORT (GSY_S_M_DEF | GSY_S_M_REL);
	}
      else
	{
	  PUT_SHORT (GSY_S_M_DEF);
	};
      /*
       *	Psect Number
       */
      if (Psect_Number <= 255)
	{
	  PUT_CHAR (Psect_Number);
	}
      else
	{
	  PUT_SHORT (Psect_Number);
	}
      /*
       *	Offset
       */
      PUT_LONG (Psect_Offset);
    }
  else
    {
      /*
       *	Reference:
       *	Flags = "RELOCATABLE" for regular symbol,
       *	      = "" for globalvalue (Defined & 2 == 1)
       */
      if ((Defined & 2) == 0)
	{
	  PUT_SHORT (GSY_S_M_REL);
	}
      else
	{
	  PUT_SHORT (0);
	};
    }
  /*
   *	Finally, the global symbol name
   */
  VMS_Case_Hack_Symbol (Name, Local);
  PUT_COUNTED_STRING (Local);
  /*
   *	Flush the buffer if it is more than 75% full
   */
  if (Object_Record_Offset >
      (sizeof (Object_Record_Buffer) * 3 / 4))
    Flush_VMS_Object_Record_Buffer ();
}


/*
 *	Define a psect
 */
static int
VMS_Psect_Spec (Name, Size, Type, vsp)
     char *Name;
     int Size;
     char *Type;
     struct VMS_Symbol *vsp;
{
  char Local[32];
  int Psect_Attributes;

  /*
   *	Generate the appropriate PSECT flags given the PSECT type
   */
  if (strcmp (Type, "COMMON") == 0)
    {
      /*
       *	Common block psects are:  PIC,OVR,REL,GBL,SHR,RD,WRT
       */
      Psect_Attributes = (GPS_S_M_PIC | GPS_S_M_OVR | GPS_S_M_REL | GPS_S_M_GBL |
			  GPS_S_M_SHR | GPS_S_M_RD | GPS_S_M_WRT);
    }
  else if (strcmp (Type, "CONST") == 0)
    {
      /*
       *	Common block psects are:  PIC,OVR,REL,GBL,SHR,RD
       */
      Psect_Attributes = (GPS_S_M_PIC | GPS_S_M_OVR | GPS_S_M_REL | GPS_S_M_GBL |
			  GPS_S_M_SHR | GPS_S_M_RD);
    }
  else if (strcmp (Type, "DATA") == 0)
    {
      /*
       *	The Data psects are PIC,REL,RD,WRT
       */
      Psect_Attributes =
	(GPS_S_M_PIC | GPS_S_M_REL | GPS_S_M_RD | GPS_S_M_WRT);
    }
  else if (strcmp (Type, "TEXT") == 0)
    {
      /*
       *	The Text psects are PIC,REL,SHR,EXE,RD
       */
      Psect_Attributes =
	(GPS_S_M_PIC | GPS_S_M_REL | GPS_S_M_SHR |
	 GPS_S_M_EXE | GPS_S_M_RD);
    }
  else
    {
      /*
       *	Error: Unknown psect type
       */
      error ("Unknown VMS psect type");
    }
  /*
   *	Modify the psect attributes according to any attribute string
   */
  if (HAS_PSECT_ATTRIBUTES (Name))
    VMS_Modify_Psect_Attributes (Name, &Psect_Attributes);
  /*
   *	Check for globalref/def/val.
   */
  if ((Psect_Attributes & GLOBALVALUE_BIT) != 0)
    {
      /*
       * globalvalue symbols were generated before. This code
       * prevents unsightly psect buildup, and makes sure that
       * fixup references are emitted correctly.
       */
      vsp->Psect_Index = -1;	/* to catch errors */
      S_GET_RAW_TYPE (vsp->Symbol) = N_UNDF;	/* make refs work */
      return 1;			/* decrement psect counter */
    };

  if ((Psect_Attributes & GLOBALSYMBOL_BIT) != 0)
    {
      switch (S_GET_RAW_TYPE (vsp->Symbol))
	{
	case N_UNDF | N_EXT:
	  VMS_Global_Symbol_Spec (Name, vsp->Psect_Index,
				  vsp->Psect_Offset, 0);
	  vsp->Psect_Index = -1;
	  S_GET_RAW_TYPE (vsp->Symbol) = N_UNDF;
	  return 1;		/* return and indicate no psect */
	case N_DATA | N_EXT:
	  VMS_Global_Symbol_Spec (Name, vsp->Psect_Index,
				  vsp->Psect_Offset, 1);
	  /* In this case we still generate the psect */
	  break;
	default:
	  {
	    char Error_Line[256];
	    sprintf (Error_Line, "Globalsymbol attribute for"
		     " symbol %s was unexpected.\n", Name);
	    error (Error_Line);
	    break;
	  };
	};			/* switch */
    };

  Psect_Attributes &= 0xffff;	/* clear out the globalref/def stuff */
  /*
   *	We are writing a GSD record
   */
  Set_VMS_Object_File_Record (OBJ_S_C_GSD);
  /*
   *	If the buffer is empty we must insert the GSD record type
   */
  if (Object_Record_Offset == 0)
    PUT_CHAR (OBJ_S_C_GSD);
  /*
   *	We are writing a PSECT definition subrecord
   */
  PUT_CHAR (GSD_S_C_PSC);
  /*
   *	Psects are always LONGWORD aligned
   */
  PUT_CHAR (2);
  /*
   *	Specify the psect attributes
   */
  PUT_SHORT (Psect_Attributes);
  /*
   *	Specify the allocation
   */
  PUT_LONG (Size);
  /*
   *	Finally, the psect name
   */
  VMS_Case_Hack_Symbol (Name, Local);
  PUT_COUNTED_STRING (Local);
  /*
   *	Flush the buffer if it is more than 75% full
   */
  if (Object_Record_Offset >
      (sizeof (Object_Record_Buffer) * 3 / 4))
    Flush_VMS_Object_Record_Buffer ();
  return 0;
}


/*
 *	Given the pointer to a symbol we calculate how big the data at the
 *	symbol is.  We do this by looking for the next symbol (local or
 *	global) which will indicate the start of another datum.
 */
static int
VMS_Initialized_Data_Size (sp, End_Of_Data)
     register struct symbol *sp;
     int End_Of_Data;
{
  register struct symbol *sp1, *Next_Symbol;

  /*
   *	Find the next symbol
   *	it delimits this datum
   */
  Next_Symbol = 0;
  for (sp1 = symbol_rootP; sp1; sp1 = symbol_next (sp1))
    {
      /*
       *	The data type must match
       */
      if (S_GET_TYPE (sp1) != N_DATA)
	continue;
      /*
       *	The symbol must be AFTER this symbol
       */
      if (S_GET_VALUE (sp1) <= S_GET_VALUE (sp))
	continue;
      /*
       *	We ignore THIS symbol
       */
      if (sp1 == sp)
	continue;
      /*
       *	If there is already a candidate selected for the
       *	next symbol, see if we are a better candidate
       */
      if (Next_Symbol)
	{
	  /*
	   *	We are a better candidate if we are "closer"
	   *	to the symbol
	   */
	  if (S_GET_VALUE (sp1) >
	      S_GET_VALUE (Next_Symbol))
	    continue;
	  /*
	   *	Win:  Make this the candidate
	   */
	  Next_Symbol = sp1;
	}
      else
	{
	  /*
	   *	This is the 1st candidate
	   */
	  Next_Symbol = sp1;
	}
    }
  /*
   *	Calculate its size
   */
  return (Next_Symbol ?
	  (S_GET_VALUE (Next_Symbol) -
	   S_GET_VALUE (sp)) :
	  (End_Of_Data - S_GET_VALUE (sp)));
}

/*
 *	Check symbol names for the Psect hack with a globalvalue, and then
 *	generate globalvalues for those that have it.
 */
static
VMS_Emit_Globalvalues (text_siz, data_siz, Data_Segment)
     unsigned text_siz;
     unsigned data_siz;
     char *Data_Segment;
{
  register symbolS *sp;
  char *stripped_name, *Name;
  int Size;
  int Psect_Attributes;
  int globalvalue;

  /*
   * Scan the symbol table for globalvalues, and emit def/ref when
   * required.  These will be caught again later and converted to
   * N_UNDF
   */
  for (sp = symbol_rootP; sp; sp = sp->sy_next)
    {
      /*
       *	See if this is something we want to look at.
       */
      if ((S_GET_RAW_TYPE (sp) != (N_DATA | N_EXT)) &&
	  (S_GET_RAW_TYPE (sp) != (N_UNDF | N_EXT)))
	continue;
      /*
       *	See if this has globalvalue specification.
       */
      Name = S_GET_NAME (sp);

      if (!HAS_PSECT_ATTRIBUTES (Name))
	continue;

      stripped_name = (char *) malloc (strlen (Name) + 1);
      strcpy (stripped_name, Name);
      Psect_Attributes = 0;
      VMS_Modify_Psect_Attributes (stripped_name, &Psect_Attributes);

      if ((Psect_Attributes & GLOBALVALUE_BIT) != 0)
	{
	  switch (S_GET_RAW_TYPE (sp))
	    {
	    case N_UNDF | N_EXT:
	      VMS_Global_Symbol_Spec (stripped_name, 0, 0, 2);
	      break;
	    case N_DATA | N_EXT:
	      Size = VMS_Initialized_Data_Size (sp, text_siz + data_siz);
	      if (Size > 4)
		error ("Invalid data type for globalvalue");
	      globalvalue = 0;

	      memcpy (&globalvalue, Data_Segment + S_GET_VALUE (sp) -
		     text_siz, Size);
	      /* Three times for good luck.  The linker seems to get confused
	         if there are fewer than three */
	      VMS_Global_Symbol_Spec (stripped_name, 0, 0, 2);
	      VMS_Global_Symbol_Spec (stripped_name, 0, globalvalue, 3);
	      VMS_Global_Symbol_Spec (stripped_name, 0, globalvalue, 3);
	      break;
	    default:
	      printf (" Invalid globalvalue of %s\n", stripped_name);
	      break;
	    };			/* switch */
	};			/* if */
      free (stripped_name);	/* clean up */
    };				/* for */

}


/*
 *	Define a procedure entry pt/mask
 */
static
VMS_Procedure_Entry_Pt (Name, Psect_Number, Psect_Offset, Entry_Mask)
     char *Name;
     int Psect_Number;
     int Psect_Offset;
     int Entry_Mask;
{
  char Local[32];

  /*
   *	We are writing a GSD record
   */
  Set_VMS_Object_File_Record (OBJ_S_C_GSD);
  /*
   *	If the buffer is empty we must insert the GSD record type
   */
  if (Object_Record_Offset == 0)
    PUT_CHAR (OBJ_S_C_GSD);
  /*
   *	We are writing a Procedure Entry Pt/Mask subrecord
   */
  if (Psect_Number <= 255)
    {
      PUT_CHAR (GSD_S_C_EPM);
    }
  else
    {
      PUT_CHAR (GSD_S_C_EPMW);
    }
  /*
   *	Data type is undefined
   */
  PUT_CHAR (0);
  /*
   *	Flags = "RELOCATABLE" and "DEFINED"
   */
  PUT_SHORT (GSY_S_M_DEF | GSY_S_M_REL);
  /*
   *	Psect Number
   */
  if (Psect_Number <= 255)
    {
      PUT_CHAR (Psect_Number);
    }
  else
    {
      PUT_SHORT (Psect_Number);
    }
  /*
   *	Offset
   */
  PUT_LONG (Psect_Offset);
  /*
   *	Entry mask
   */
  PUT_SHORT (Entry_Mask);
  /*
   *	Finally, the global symbol name
   */
  VMS_Case_Hack_Symbol (Name, Local);
  PUT_COUNTED_STRING (Local);
  /*
   *	Flush the buffer if it is more than 75% full
   */
  if (Object_Record_Offset >
      (sizeof (Object_Record_Buffer) * 3 / 4))
    Flush_VMS_Object_Record_Buffer ();
}


/*
 *	Set the current location counter to a particular Psect and Offset
 */
static
VMS_Set_Psect (Psect_Index, Offset, Record_Type)
     int Psect_Index;
     int Offset;
     int Record_Type;
{
  /*
   *	We are writing a "Record_Type" record
   */
  Set_VMS_Object_File_Record (Record_Type);
  /*
   *	If the buffer is empty we must insert the record type
   */
  if (Object_Record_Offset == 0)
    PUT_CHAR (Record_Type);
  /*
   *	Stack the Psect base + Longword Offset
   */
  if (Psect_Index < 255)
    {
      PUT_CHAR (TIR_S_C_STA_PL);
      PUT_CHAR (Psect_Index);
    }
  else
    {
      PUT_CHAR (TIR_S_C_STA_WPL);
      PUT_SHORT (Psect_Index);
    }
  PUT_LONG (Offset);
  /*
   *	Set relocation base
   */
  PUT_CHAR (TIR_S_C_CTL_SETRB);
  /*
   *	Flush the buffer if it is more than 75% full
   */
  if (Object_Record_Offset >
      (sizeof (Object_Record_Buffer) * 3 / 4))
    Flush_VMS_Object_Record_Buffer ();
}


/*
 *	Store repeated immediate data in current Psect
 */
static
VMS_Store_Repeated_Data (Repeat_Count, Pointer, Size, Record_Type)
     int Repeat_Count;
     register char *Pointer;
     int Size;
     int Record_Type;
{

  /*
   *	Ignore zero bytes/words/longwords
   */
  if ((Size == sizeof (char)) && (*Pointer == 0))
    return;
  if ((Size == sizeof (short)) && (*(short *) Pointer == 0))
    return;
  if ((Size == sizeof (long)) && (*(long *) Pointer == 0))
    return;
  /*
   *	If the data is too big for a TIR_S_C_STO_RIVB sub-record
   *	then we do it manually
   */
  if (Size > 255)
    {
      while (--Repeat_Count >= 0)
	VMS_Store_Immediate_Data (Pointer, Size, Record_Type);
      return;
    }
  /*
   *	We are writing a "Record_Type" record
   */
  Set_VMS_Object_File_Record (Record_Type);
  /*
   *	If the buffer is empty we must insert record type
   */
  if (Object_Record_Offset == 0)
    PUT_CHAR (Record_Type);
  /*
   *	Stack the repeat count
   */
  PUT_CHAR (TIR_S_C_STA_LW);
  PUT_LONG (Repeat_Count);
  /*
   *	And now the command and its data
   */
  PUT_CHAR (TIR_S_C_STO_RIVB);
  PUT_CHAR (Size);
  while (--Size >= 0)
    PUT_CHAR (*Pointer++);
  /*
   *	Flush the buffer if it is more than 75% full
   */
  if (Object_Record_Offset >
      (sizeof (Object_Record_Buffer) * 3 / 4))
    Flush_VMS_Object_Record_Buffer ();
}


/*
 *	Store a Position Independent Reference
 */
static
VMS_Store_PIC_Symbol_Reference (Symbol, Offset, PC_Relative,
				Psect, Psect_Offset, Record_Type)
     struct symbol *Symbol;
     int Offset;
     int PC_Relative;
     int Psect;
     int Psect_Offset;
     int Record_Type;
{
  register struct VMS_Symbol *vsp =
  (struct VMS_Symbol *) (Symbol->sy_number);
  char Local[32];

  /*
   *	We are writing a "Record_Type" record
   */
  Set_VMS_Object_File_Record (Record_Type);
  /*
   *	If the buffer is empty we must insert record type
   */
  if (Object_Record_Offset == 0)
    PUT_CHAR (Record_Type);
  /*
   *	Set to the appropriate offset in the Psect
   */
  if (PC_Relative)
    {
      /*
       *	For a Code reference we need to fix the operand
       *	specifier as well (so back up 1 byte)
       */
      VMS_Set_Psect (Psect, Psect_Offset - 1, Record_Type);
    }
  else
    {
      /*
       *	For a Data reference we just store HERE
       */
      VMS_Set_Psect (Psect, Psect_Offset, Record_Type);
    }
  /*
   *	Make sure we are still generating a "Record Type" record
   */
  if (Object_Record_Offset == 0)
    PUT_CHAR (Record_Type);
  /*
   *	Dispatch on symbol type (so we can stack its value)
   */
  switch (S_GET_RAW_TYPE (Symbol))
    {
      /*
       *	Global symbol
       */
#ifdef	NOT_VAX_11_C_COMPATIBLE
    case N_UNDF | N_EXT:
    case N_DATA | N_EXT:
#endif	/* NOT_VAX_11_C_COMPATIBLE */
    case N_UNDF:
    case N_TEXT | N_EXT:
      /*
       *	Get the symbol name (case hacked)
       */
      VMS_Case_Hack_Symbol (S_GET_NAME (Symbol), Local);
      /*
       *	Stack the global symbol value
       */
      PUT_CHAR (TIR_S_C_STA_GBL);
      PUT_COUNTED_STRING (Local);
      if (Offset)
	{
	  /*
	   *	Stack the longword offset
	   */
	  PUT_CHAR (TIR_S_C_STA_LW);
	  PUT_LONG (Offset);
	  /*
	   *	Add the two, leaving the result on the stack
	   */
	  PUT_CHAR (TIR_S_C_OPR_ADD);
	}
      break;
      /*
       *	Uninitialized local data
       */
    case N_BSS:
      /*
       *	Stack the Psect (+offset)
       */
      if (vsp->Psect_Index < 255)
	{
	  PUT_CHAR (TIR_S_C_STA_PL);
	  PUT_CHAR (vsp->Psect_Index);
	}
      else
	{
	  PUT_CHAR (TIR_S_C_STA_WPL);
	  PUT_SHORT (vsp->Psect_Index);
	}
      PUT_LONG (vsp->Psect_Offset + Offset);
      break;
      /*
       *	Local text
       */
    case N_TEXT:
      /*
       *	Stack the Psect (+offset)
       */
      if (vsp->Psect_Index < 255)
	{
	  PUT_CHAR (TIR_S_C_STA_PL);
	  PUT_CHAR (vsp->Psect_Index);
	}
      else
	{
	  PUT_CHAR (TIR_S_C_STA_WPL);
	  PUT_SHORT (vsp->Psect_Index);
	}
      PUT_LONG (S_GET_VALUE (Symbol) + Offset);
      break;
      /*
       *	Initialized local or global data
       */
    case N_DATA:
#ifndef	NOT_VAX_11_C_COMPATIBLE
    case N_UNDF | N_EXT:
    case N_DATA | N_EXT:
#endif	/* NOT_VAX_11_C_COMPATIBLE */
      /*
       *	Stack the Psect (+offset)
       */
      if (vsp->Psect_Index < 255)
	{
	  PUT_CHAR (TIR_S_C_STA_PL);
	  PUT_CHAR (vsp->Psect_Index);
	}
      else
	{
	  PUT_CHAR (TIR_S_C_STA_WPL);
	  PUT_SHORT (vsp->Psect_Index);
	}
      PUT_LONG (vsp->Psect_Offset + Offset);
      break;
    }
  /*
   *	Store either a code or data reference
   */
  PUT_CHAR (PC_Relative ? TIR_S_C_STO_PICR : TIR_S_C_STO_PIDR);
  /*
   *	Flush the buffer if it is more than 75% full
   */
  if (Object_Record_Offset >
      (sizeof (Object_Record_Buffer) * 3 / 4))
    Flush_VMS_Object_Record_Buffer ();
}


/*
 *	Check in the text area for an indirect pc-relative reference
 *	and fix it up with addressing mode 0xff [PC indirect]
 *
 *	THIS SHOULD BE REPLACED BY THE USE OF TIR_S_C_STO_PIRR IN THE
 *	PIC CODE GENERATING FIXUP ROUTINE.
 */
static
VMS_Fix_Indirect_Reference (Text_Psect, Offset, fragP, text_frag_root)
     int Text_Psect;
     int Offset;
     register fragS *fragP;
     struct frag *text_frag_root;
{
  /*
   *	The addressing mode byte is 1 byte before the address
   */
  Offset--;
  /*
   *	Is it in THIS frag??
   */
  if ((Offset < fragP->fr_address) ||
      (Offset >= (fragP->fr_address + fragP->fr_fix)))
    {
      /*
       *	We need to search for the fragment containing this
       *	Offset
       */
      for (fragP = text_frag_root; fragP; fragP = fragP->fr_next)
	{
	  if ((Offset >= fragP->fr_address) &&
	      (Offset < (fragP->fr_address + fragP->fr_fix)))
	    break;
	}
      /*
       *	If we couldn't find the frag, things are BAD!!
       */
      if (fragP == 0)
	error ("Couldn't find fixup fragment when checking for indirect reference");
    }
  /*
   *	Check for indirect PC relative addressing mode
   */
  if (fragP->fr_literal[Offset - fragP->fr_address] == (char) 0xff)
    {
      static char Address_Mode = 0xff;

      /*
       *	Yes: Store the indirect mode back into the image
       *	     to fix up the damage done by STO_PICR
       */
      VMS_Set_Psect (Text_Psect, Offset, OBJ_S_C_TIR);
      VMS_Store_Immediate_Data (&Address_Mode, 1, OBJ_S_C_TIR);
    }
}



/*
 *	This is a hacked _doprnt() for VAX-11 "C".  It understands that
 *	it is ONLY called by as_fatal(Format, Args) with a pointer to the
 *	"Args" argument.  From this we can make it all work right!
 */
#if	!defined(eunice) && defined(HO_VMS)
_doprnt (Format, a, f)
     char *Format;
     FILE *f;
     char **a;
{
  int Nargs = ((int *) a)[-2];	/* This understands as_fatal() */

  switch (Nargs)
    {
    default:
      fprintf (f, "_doprnt error on \"%s\"!!", Format);
      break;
    case 1:
      fprintf (f, Format);
      break;
    case 2:
      fprintf (f, Format, a[0]);
      break;
    case 3:
      fprintf (f, Format, a[0], a[1]);
      break;
    case 4:
      fprintf (f, Format, a[0], a[1], a[2]);
      break;
    case 5:
      fprintf (f, Format, a[0], a[1], a[2], a[3]);
      break;
    case 6:
      fprintf (f, Format, a[0], a[1], a[2], a[3], a[4]);
      break;
    case 7:
      fprintf (f, Format, a[0], a[1], a[2], a[3], a[4], a[5]);
      break;
    case 8:
      fprintf (f, Format, a[0], a[1], a[2], a[3], a[4], a[5], a[6]);
      break;
    case 9:
      fprintf (f, Format, a[0], a[1], a[2], a[3], a[4], a[5], a[6], a[7]);
      break;
    case 10:
      fprintf (f, Format, a[0], a[1], a[2], a[3], a[4], a[5], a[6], a[7], a[8]);
      break;
    }
}

#endif /* eunice */


/*
 *	If the procedure "main()" exists we have to add the instruction
 *	"jsb c$main_args" at the beginning to be compatible with VAX-11 "C".
 */
VMS_Check_For_Main ()
{
  register symbolS *symbolP;
#ifdef	HACK_DEC_C_STARTUP	/* JF */
  register struct frchain *frchainP;
  register fragS *fragP;
  register fragS **prev_fragPP;
  register struct fix *fixP;
  register fragS *New_Frag;
  int i;
#endif	/* HACK_DEC_C_STARTUP */

  symbolP = (struct symbol *) symbol_find ("_main");
  if (symbolP && !S_IS_DEBUG (symbolP) &&
      S_IS_EXTERNAL (symbolP) && (S_GET_TYPE (symbolP) == N_TEXT))
    {
#ifdef	HACK_DEC_C_STARTUP
      if (!flagseen['+'])
	{
#endif
	  /*
	   *	Remember the entry point symbol
	   */
	  Entry_Point_Symbol = symbolP;
#ifdef HACK_DEC_C_STARTUP
	}
      else
	{
	  /*
	   *	Scan all the fragment chains for the one with "_main"
	   *	(Actually we know the fragment from the symbol, but we need
	   *	 the previous fragment so we can change its pointer)
	   */
	  frchainP = frchain_root;
	  while (frchainP)
	    {
	      /*
	       *	Scan all the fragments in this chain, remembering
	       *	the "previous fragment"
	       */
	      prev_fragPP = &frchainP->frch_root;
	      fragP = frchainP->frch_root;
	      while (fragP && (fragP != frchainP->frch_last))
		{
		  /*
		   *	Is this the fragment?
		   */
		  if (fragP == symbolP->sy_frag)
		    {
		      /*
		       *	Yes: Modify the fragment by replacing
		       *	     it with a new fragment.
		       */
		      New_Frag = (fragS *)
			xmalloc (sizeof (*New_Frag) +
				 fragP->fr_fix +
				 fragP->fr_var +
				 5);
		      /*
		       *	The fragments are the same except
		       *	that the "fixed" area is larger
		       */
		      *New_Frag = *fragP;
		      New_Frag->fr_fix += 6;
		      /*
		       *	Copy the literal data opening a hole
		       *	2 bytes after "_main" (i.e. just after
		       *	the entry mask).  Into which we place
		       *	the JSB instruction.
		       */
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
		      /*
		       *	Now replace the old fragment with the
		       *	newly generated one.
		       */
		      *prev_fragPP = New_Frag;
		      /*
		       *	Remember the entry point symbol
		       */
		      Entry_Point_Symbol = symbolP;
		      /*
		       *	Scan the text area fixup structures
		       *	as offsets in the fragment may have
		       *	changed
		       */
		      for (fixP = text_fix_root; fixP; fixP = fixP->fx_next)
			{
			  /*
			   *	Look for references to this
			   *	fragment.
			   */
			  if (fixP->fx_frag == fragP)
			    {
			      /*
			       *	Change the fragment
			       *	pointer
			       */
			      fixP->fx_frag = New_Frag;
			      /*
			       *	If the offset is after
			       *	the entry mask we need
			       *	to account for the JSB
			       *	instruction we just
			       *	inserted.
			       */
			      if (fixP->fx_where >= 2)
				fixP->fx_where += 6;
			    }
			}
		      /*
		       *	Scan the symbols as offsets in the
		       *	fragment may have changed
		       */
		      for (symbolP = symbol_rootP;
			   symbolP;
			   symbolP = symbol_next (symbolP))
			{
			  /*
			   *	Look for references to this
			   *	fragment.
			   */
			  if (symbolP->sy_frag == fragP)
			    {
			      /*
			       *	Change the fragment
			       *	pointer
			       */
			      symbolP->sy_frag = New_Frag;
			      /*
			       *	If the offset is after
			       *	the entry mask we need
			       *	to account for the JSB
			       *	instruction we just
			       *	inserted.
			       */
			      if (S_GET_VALUE (symbolP) >= 2)
				S_GET_VALUE (symbolP) += 6;
			    }
			}
		      /*
		       *	Make a symbol reference to
		       *	"_c$main_args" so we can get
		       *	its address inserted into the
		       *	JSB instruction.
		       */
		      symbolP = (symbolS *) xmalloc (sizeof (*symbolP));
		      S_GET_NAME (symbolP) = "_c$main_args";
		      S_SET_TYPE (symbolP, N_UNDF);
		      S_GET_OTHER (symbolP) = 0;
		      S_GET_DESC (symbolP) = 0;
		      S_GET_VALUE (symbolP) = 0;
		      symbolP->sy_name_offset = 0;
		      symbolP->sy_number = 0;
		      symbolP->sy_frag = New_Frag;
		      symbolP->sy_forward = 0;
		      /* this actually inserts at the beginning of the list */
		      symbol_append (symbol_rootP, symbolP, &symbol_rootP, &symbol_lastP);

		      symbol_rootP = symbolP;
		      /*
		       *	Generate a text fixup structure
		       *	to get "_c$main_args" stored into the
		       *	JSB instruction.
		       */
		      fixP = (struct fix *) xmalloc (sizeof (*fixP));
		      fixP->fx_frag = New_Frag;
		      fixP->fx_where = 4;
		      fixP->fx_addsy = symbolP;
		      fixP->fx_subsy = 0;
		      fixP->fx_offset = 0;
		      fixP->fx_size = sizeof (long);
		      fixP->fx_pcrel = 1;
		      fixP->fx_next = text_fix_root;
		      text_fix_root = fixP;
		      /*
		       *	Now make sure we exit from the loop
		       */
		      frchainP = 0;
		      break;
		    }
		  /*
		   *	Try the next fragment
		   */
		  prev_fragPP = &fragP->fr_next;
		  fragP = fragP->fr_next;
		}
	      /*
	       *	Try the next fragment chain
	       */
	      if (frchainP)
		frchainP = frchainP->frch_next;
	    }
	}
#endif /* HACK_DEC_C_STARTUP */
    }
}

/*
 *	Write a VAX/VMS object file (everything else has been done!)
 */
VMS_write_object_file (text_siz, data_siz, text_frag_root, data_frag_root)
     unsigned text_siz;
     unsigned data_siz;
     struct frag *text_frag_root;
     struct frag *data_frag_root;
{
  register fragS *fragP;
  register symbolS *symbolP;
  register symbolS *sp;
  register struct fix *fixP;
  register struct VMS_Symbol *vsp;
  char *Data_Segment;
  int Local_Initialized_Data_Size = 0;
  int Globalref;
  int Psect_Number = 0;		/* Psect Index Number */
  int Text_Psect = -1;		/* Text Psect Index   */
  int Data_Psect = -2;		/* Data Psect Index   JF: Was -1 */
  int Bss_Psect = -3;		/* Bss Psect Index    JF: Was -1 */

  /*
   *	Create the VMS object file
   */
  Create_VMS_Object_File ();
  /*
   *	Write the module header records
   */
  Write_VMS_MHD_Records ();

  /*
   *	Store the Data segment:
   *
   *	Since this is REALLY hard to do any other way,
   *	we actually manufacture the data segment and
   *	the store the appropriate values out of it.
   *	We need to generate this early, so that globalvalues
   *	can be properly emitted.
   */
  if (data_siz > 0)
    {
      /*
       *	Allocate the data segment
       */
      Data_Segment = (char *) xmalloc (data_siz);
      /*
       *	Run through the data fragments, filling in the segment
       */
      for (fragP = data_frag_root; fragP; fragP = fragP->fr_next)
	{
	  register long int count;
	  register char *fill_literal;
	  register long int fill_size;
	  int i;

	  i = fragP->fr_address - text_siz;
	  if (fragP->fr_fix)
	    memcpy (Data_Segment + i,
		    fragP->fr_literal,
		    fragP->fr_fix);
	  i += fragP->fr_fix;

	  fill_literal = fragP->fr_literal + fragP->fr_fix;
	  fill_size = fragP->fr_var;
	  for (count = fragP->fr_offset; count; count--)
	    {
	      if (fill_size)
		memcpy (Data_Segment + i, fill_literal, fill_size);
	      i += fill_size;
	    }
	}
    }


  /*
   *	Generate the VMS object file records
   *	1st GSD then TIR records
   */

  /*******       Global Symbol Dictionary       *******/
  /*
   * Emit globalvalues now.  We must do this before the text psect
   * is defined, or we will get linker warnings about multiply defined
   * symbols.  All of the globalvalues "reference" psect 0, although
   * it really does not have anything to do with it.
   */
  VMS_Emit_Globalvalues (text_siz, data_siz, Data_Segment);
  /*
   *	Define the Text Psect
   */
  Text_Psect = Psect_Number++;
  VMS_Psect_Spec ("$code", text_siz, "TEXT", 0);
  /*
   *	Define the BSS Psect
   */
  if (local_bss_counter > 0)
    {
      Bss_Psect = Psect_Number++;
      VMS_Psect_Spec ("$uninitialized_data", local_bss_counter, "DATA",
		      0);
    }
#ifndef gxx_bug_fixed
  /*
   * The g++ compiler does not write out external references to vtables
   * correctly.  Check for this and holler if we see it happening.
   * If that compiler bug is ever fixed we can remove this.
   */
  for (sp = symbol_rootP; sp; sp = symbol_next (sp))
    {
      /*
       *	Dispatch on symbol type
       */
      switch (S_GET_RAW_TYPE (sp)) {
	/*
	 *	Global Reference
	 */
      case N_UNDF:
	/*
	 *	Make a GSD global symbol reference
	 *	record.
	 */
	if (strncmp (S_GET_NAME (sp),"__vt.",5) == 0)
	  {
	    S_GET_RAW_TYPE (sp) = N_UNDF | N_EXT;
	    as_warn("g++ wrote an extern reference to %s as a routine.",
		    S_GET_NAME (sp));
	    as_warn("I will fix it, but I hope that it was not really a routine");
	  };
	break;
      default:
	break;
      }
    }
#endif /* gxx_bug_fixed */
  /*
   *	Now scan the symbols and emit the appropriate GSD records
   */
  for (sp = symbol_rootP; sp; sp = symbol_next (sp))
    {
      /*
       *	Dispatch on symbol type
       */
      switch (S_GET_RAW_TYPE (sp))
	{
	  /*
	   *	Global uninitialized data
	   */
	case N_UNDF | N_EXT:
	  /*
	   *	Make a VMS data symbol entry
	   */
	  vsp = (struct VMS_Symbol *)
	    xmalloc (sizeof (*vsp));
	  vsp->Symbol = sp;
	  vsp->Size = S_GET_VALUE (sp);
	  vsp->Psect_Index = Psect_Number++;
	  vsp->Psect_Offset = 0;
	  vsp->Next = VMS_Symbols;
	  VMS_Symbols = vsp;
	  sp->sy_number = (int) vsp;
	  /*
	   *	Make the psect for this data
	   */
	  if (S_GET_OTHER (sp))
	    Globalref = VMS_Psect_Spec (
					 S_GET_NAME (sp),
					 vsp->Size,
					 "CONST",
					 vsp);
	  else
	    Globalref = VMS_Psect_Spec (
					 S_GET_NAME (sp),
					 vsp->Size,
					 "COMMON",
					 vsp);
	  if (Globalref)
	    Psect_Number--;
#ifdef	NOT_VAX_11_C_COMPATIBLE
	  /*
	   *	Place a global symbol at the
	   *	beginning of the Psect
	   */
	  VMS_Global_Symbol_Spec (S_GET_NAME (sp),
				  vsp->Psect_Index,
				  0,
				  1);
#endif	/* NOT_VAX_11_C_COMPATIBLE */
	  break;
	  /*
	   *	Local uninitialized data
	   */
	case N_BSS:
	  /*
	   *	Make a VMS data symbol entry
	   */
	  vsp = (struct VMS_Symbol *)
	    xmalloc (sizeof (*vsp));
	  vsp->Symbol = sp;
	  vsp->Size = 0;
	  vsp->Psect_Index = Bss_Psect;
	  vsp->Psect_Offset =
	    S_GET_VALUE (sp) -
	    bss_address_frag.fr_address;
	  vsp->Next = VMS_Symbols;
	  VMS_Symbols = vsp;
	  sp->sy_number = (int) vsp;
	  break;
	  /*
	   *	Global initialized data
	   */
	case N_DATA | N_EXT:
	  /*
	   *	Make a VMS data symbol entry
	   */
	  vsp = (struct VMS_Symbol *)
	    xmalloc (sizeof (*vsp));
	  vsp->Symbol = sp;
	  vsp->Size = VMS_Initialized_Data_Size (sp,
						 text_siz + data_siz);
	  vsp->Psect_Index = Psect_Number++;
	  vsp->Psect_Offset = 0;
	  vsp->Next = VMS_Symbols;
	  VMS_Symbols = vsp;
	  sp->sy_number = (int) vsp;
	  /*
	   *	Make its psect
	   */
	  if (S_GET_OTHER (sp))
	    Globalref = VMS_Psect_Spec (
					 S_GET_NAME (sp),
					 vsp->Size,
					 "CONST",
					 vsp);
	  else
	    Globalref = VMS_Psect_Spec (
					 S_GET_NAME (sp),
					 vsp->Size,
					 "COMMON",
					 vsp);
	  if (Globalref)
	    Psect_Number--;
#ifdef	NOT_VAX_11_C_COMPATIBLE
	  /*
	   *	Place a global symbol at the
	   *	beginning of the Psect
	   */
	  VMS_Global_Symbol_Spec (S_GET_NAME (sp),
				  vsp->Psect_Index,
				  0,
				  1);
#endif	/* NOT_VAX_11_C_COMPATIBLE */
	  break;
	  /*
	   *	Local initialized data
	   */
	case N_DATA:
	  /*
	   *	Make a VMS data symbol entry
	   */
	  vsp = (struct VMS_Symbol *)
	    xmalloc (sizeof (*vsp));
	  vsp->Symbol = sp;
	  vsp->Size =
	    VMS_Initialized_Data_Size (sp,
				       text_siz + data_siz);
	  vsp->Psect_Index = Data_Psect;
	  vsp->Psect_Offset =
	    Local_Initialized_Data_Size;
	  Local_Initialized_Data_Size += vsp->Size;
	  vsp->Next = VMS_Symbols;
	  VMS_Symbols = vsp;
	  sp->sy_number = (int) vsp;
	  break;
	  /*
	   *	Global Text definition
	   */
	case N_TEXT | N_EXT:
	  {
	    unsigned short Entry_Mask;

	    /*
	     *	Get the entry mask
	     */
	    fragP = sp->sy_frag;
	    Entry_Mask = (fragP->fr_literal[0] & 0xff) +
	      ((fragP->fr_literal[1] & 0xff)
	       << 8);
	    /*
	     *	Define the Procedure entry pt.
	     */
	    VMS_Procedure_Entry_Pt (S_GET_NAME (sp),
				    Text_Psect,
				    S_GET_VALUE (sp),
				    Entry_Mask);
	    break;
	  }
	  /*
	   *	Local Text definition
	   */
	case N_TEXT:
	  /*
	   *	Make a VMS data symbol entry
	   */
	  if (Text_Psect != -1)
	    {
	      vsp = (struct VMS_Symbol *)
		xmalloc (sizeof (*vsp));
	      vsp->Symbol = sp;
	      vsp->Size = 0;
	      vsp->Psect_Index = Text_Psect;
	      vsp->Psect_Offset = S_GET_VALUE (sp);
	      vsp->Next = VMS_Symbols;
	      VMS_Symbols = vsp;
	      sp->sy_number = (int) vsp;
	    }
	  break;
	  /*
	   *	Global Reference
	   */
	case N_UNDF:
	  /*
	   *	Make a GSD global symbol reference
	   *	record.
	   */
	  VMS_Global_Symbol_Spec (S_GET_NAME (sp),
				  0,
				  0,
				  0);
	  break;
	  /*
	   *	Anything else
	   */
	default:
	  /*
	   *	Ignore STAB symbols
	   *	Including .stabs emitted by g++
	   */
	  if (S_IS_DEBUG (sp) || (S_GET_TYPE (sp) == 22))
	    break;
	  /*
	   *	Error
	   */
	  if (S_GET_TYPE (sp) != 22)
	    printf (" ERROR, unknown type (%d)\n",
		    S_GET_TYPE (sp));
	  break;
	}
    }
  /*
   *	Define the Data Psect
   */
  if ((data_siz > 0) && (Local_Initialized_Data_Size > 0))
    {
      /*
       *	Do it
       */
      Data_Psect = Psect_Number++;
      VMS_Psect_Spec ("$data",
		      Local_Initialized_Data_Size,
		      "DATA", 0);
      /*
       *	Scan the VMS symbols and fill in the data psect
       */
      for (vsp = VMS_Symbols; vsp; vsp = vsp->Next)
	{
	  /*
	   *	Only look for undefined psects
	   */
	  if (vsp->Psect_Index < 0)
	    {
	      /*
	       *	And only initialized data
	       */
	      if ((S_GET_TYPE (vsp->Symbol) == N_DATA) && !S_IS_EXTERNAL (vsp->Symbol))
		vsp->Psect_Index = Data_Psect;
	    }
	}
    }

  /*******  Text Information and Relocation Records  *******/
  /*
   *	Write the text segment data
   */
  if (text_siz > 0)
    {
      /*
       *	Scan the text fragments
       */
      for (fragP = text_frag_root; fragP; fragP = fragP->fr_next)
	{
	  /*
	   *	Stop if we get to the data fragments
	   */
	  if (fragP == data_frag_root)
	    break;
	  /*
	   *	Ignore fragments with no data
	   */
	  if ((fragP->fr_fix == 0) && (fragP->fr_var == 0))
	    continue;
	  /*
	   *	Go the the appropriate offset in the
	   *	Text Psect.
	   */
	  VMS_Set_Psect (Text_Psect, fragP->fr_address, OBJ_S_C_TIR);
	  /*
	   *	Store the "fixed" part
	   */
	  if (fragP->fr_fix)
	    VMS_Store_Immediate_Data (fragP->fr_literal,
				      fragP->fr_fix,
				      OBJ_S_C_TIR);
	  /*
	   *	Store the "variable" part
	   */
	  if (fragP->fr_var && fragP->fr_offset)
	    VMS_Store_Repeated_Data (fragP->fr_offset,
				     fragP->fr_literal +
				     fragP->fr_fix,
				     fragP->fr_var,
				     OBJ_S_C_TIR);
	}
      /*
       *	Now we go through the text segment fixups and
       *	generate TIR records to fix up addresses within
       *	the Text Psect
       */
      for (fixP = text_fix_root; fixP; fixP = fixP->fx_next)
	{
	  /*
	   *	We DO handle the case of "Symbol - Symbol" as
	   *	long as it is in the same segment.
	   */
	  if (fixP->fx_subsy && fixP->fx_addsy)
	    {
	      int i;

	      /*
	       *	They need to be in the same segment
	       */
	      if (S_GET_RAW_TYPE (fixP->fx_subsy) !=
		  S_GET_RAW_TYPE (fixP->fx_addsy))
		error ("Fixup data addsy and subsy didn't have the same type");
	      /*
	       *	And they need to be in one that we
	       *	can check the psect on
	       */
	      if ((S_GET_TYPE (fixP->fx_addsy) != N_DATA) &&
		  (S_GET_TYPE (fixP->fx_addsy) != N_TEXT))
		error ("Fixup data addsy and subsy didn't have an appropriate type");
	      /*
	       *	This had better not be PC relative!
	       */
	      if (fixP->fx_pcrel)
		error ("Fixup data was erroneously \"pcrel\"");
	      /*
	       *	Subtract their values to get the
	       *	difference.
	       */
	      i = S_GET_VALUE (fixP->fx_addsy) -
		S_GET_VALUE (fixP->fx_subsy);
	      /*
	       *	Now generate the fixup object records
	       *	Set the psect and store the data
	       */
	      VMS_Set_Psect (Text_Psect,
			     fixP->fx_where +
			     fixP->fx_frag->fr_address,
			     OBJ_S_C_TIR);
	      VMS_Store_Immediate_Data (&i,
					fixP->fx_size,
					OBJ_S_C_TIR);
	      /*
	       *	Done
	       */
	      continue;
	    }
	  /*
	   *	Size will HAVE to be "long"
	   */
	  if (fixP->fx_size != sizeof (long))
	    error ("Fixup datum was not a longword");
	  /*
	   *	Symbol must be "added" (if it is ever
	   *				subtracted we can
	   *				fix this assumption)
	   */
	  if (fixP->fx_addsy == 0)
	    error ("Fixup datum was not \"fixP->fx_addsy\"");
	  /*
	   *	Store the symbol value in a PIC fashion
	   */
	  VMS_Store_PIC_Symbol_Reference (fixP->fx_addsy,
					  fixP->fx_offset,
					  fixP->fx_pcrel,
					  Text_Psect,
					  fixP->fx_where +
					  fixP->fx_frag->fr_address,
					  OBJ_S_C_TIR);
	  /*
	   *	Check for indirect address reference,
	   *	which has to be fixed up (as the linker
	   *	will screw it up with TIR_S_C_STO_PICR).
	   */
	  if (fixP->fx_pcrel)
	    VMS_Fix_Indirect_Reference (Text_Psect,
					fixP->fx_where +
					fixP->fx_frag->fr_address,
					fixP->fx_frag,
					text_frag_root);
	}
    }
  /*
   *	Store the Data segment:
   *
   *	Since this is REALLY hard to do any other way,
   *	we actually manufacture the data segment and
   *	the store the appropriate values out of it.
   *	The segment was manufactured before, now we just
   *	dump it into the appropriate psects.
   */
  if (data_siz > 0)
    {

      /*
       *	Now we can run through all the data symbols
       *	and store the data
       */
      for (vsp = VMS_Symbols; vsp; vsp = vsp->Next)
	{
	  /*
	   *	Ignore anything other than data symbols
	   */
	  if (S_GET_TYPE (vsp->Symbol) != N_DATA)
	    continue;
	  /*
	   *	Set the Psect + Offset
	   */
	  VMS_Set_Psect (vsp->Psect_Index,
			 vsp->Psect_Offset,
			 OBJ_S_C_TIR);
	  /*
	   *	Store the data
	   */
	  VMS_Store_Immediate_Data (Data_Segment +
				    S_GET_VALUE (vsp->Symbol) -
				    text_siz,
				    vsp->Size,
				    OBJ_S_C_TIR);
	}
      /*
       *	Now we go through the data segment fixups and
       *	generate TIR records to fix up addresses within
       *	the Data Psects
       */
      for (fixP = data_fix_root; fixP; fixP = fixP->fx_next)
	{
	  /*
	   *	Find the symbol for the containing datum
	   */
	  for (vsp = VMS_Symbols; vsp; vsp = vsp->Next)
	    {
	      /*
	       *	Only bother with Data symbols
	       */
	      sp = vsp->Symbol;
	      if (S_GET_TYPE (sp) != N_DATA)
		continue;
	      /*
	       *	Ignore symbol if After fixup
	       */
	      if (S_GET_VALUE (sp) >
		  (fixP->fx_where +
		   fixP->fx_frag->fr_address))
		continue;
	      /*
	       *	See if the datum is here
	       */
	      if ((S_GET_VALUE (sp) + vsp->Size) <=
		  (fixP->fx_where +
		   fixP->fx_frag->fr_address))
		continue;
	      /*
	       *	We DO handle the case of "Symbol - Symbol" as
	       *	long as it is in the same segment.
	       */
	      if (fixP->fx_subsy && fixP->fx_addsy)
		{
		  int i;

		  /*
		   *	They need to be in the same segment
		   */
		  if (S_GET_RAW_TYPE (fixP->fx_subsy) !=
		      S_GET_RAW_TYPE (fixP->fx_addsy))
		    error ("Fixup data addsy and subsy didn't have the same type");
		  /*
		   *	And they need to be in one that we
		   *	can check the psect on
		   */
		  if ((S_GET_TYPE (fixP->fx_addsy) != N_DATA) &&
		      (S_GET_TYPE (fixP->fx_addsy) != N_TEXT))
		    error ("Fixup data addsy and subsy didn't have an appropriate type");
		  /*
		   *	This had better not be PC relative!
		   */
		  if (fixP->fx_pcrel)
		    error ("Fixup data was erroneously \"pcrel\"");
		  /*
		   *	Subtract their values to get the
		   *	difference.
		   */
		  i = S_GET_VALUE (fixP->fx_addsy) -
		    S_GET_VALUE (fixP->fx_subsy);
		  /*
		   *	Now generate the fixup object records
		   *	Set the psect and store the data
		   */
		  VMS_Set_Psect (vsp->Psect_Index,
				 fixP->fx_frag->fr_address +
				 fixP->fx_where -
				 S_GET_VALUE (vsp->Symbol) +
				 vsp->Psect_Offset,
				 OBJ_S_C_TIR);
		  VMS_Store_Immediate_Data (&i,
					    fixP->fx_size,
					    OBJ_S_C_TIR);
		  /*
		   *	Done
		   */
		  break;
		}
	      /*
	       *	Size will HAVE to be "long"
	       */
	      if (fixP->fx_size != sizeof (long))
		error ("Fixup datum was not a longword");
	      /*
	       *	Symbol must be "added" (if it is ever
	       *				subtracted we can
	       *				fix this assumption)
	       */
	      if (fixP->fx_addsy == 0)
		error ("Fixup datum was not \"fixP->fx_addsy\"");
	      /*
	       *	Store the symbol value in a PIC fashion
	       */
	      VMS_Store_PIC_Symbol_Reference (
					       fixP->fx_addsy,
					       fixP->fx_offset,
					       fixP->fx_pcrel,
					       vsp->Psect_Index,
					       fixP->fx_frag->fr_address +
					       fixP->fx_where -
					       S_GET_VALUE (vsp->Symbol) +
					       vsp->Psect_Offset,
					       OBJ_S_C_TIR);
	      /*
	       *	Done
	       */
	      break;
	    }

	}
    }

  /*
   *	Write the Traceback Begin Module record
   */
  VMS_TBT_Module_Begin ();
  /*
   *	Scan the symbols and write out the routines
   *	(this makes the assumption that symbols are in
   *	 order of ascending text segment offset)
   */
  {
    struct symbol *Current_Routine = 0;
    int Current_Line_Number = 0;
    int Current_Offset = -1;
    struct input_file *Current_File;

/* Output debugging info for global variables and static variables that are not
 * specific to one routine. We also need to examine all stabs directives, to
 * find the definitions to all of the advanced data types, and this is done by
 * VMS_LSYM_Parse.  This needs to be done before any definitions are output to
 * the object file, since there can be forward references in the stabs
 * directives. When through with parsing, the text of the stabs directive
 * is altered, with the definitions removed, so that later passes will see
 * directives as they would be written if the type were already defined.
 *
 * We also look for files and include files, and make a list of them.  We
 * examine the source file numbers to establish the actual lines that code was
 * generated from, and then generate offsets.
 */
    VMS_LSYM_Parse ();
    for (symbolP = symbol_rootP; symbolP; symbolP = symbol_next (symbolP))
      {
	/*
	 *	Deal with STAB symbols
	 */
	if (S_IS_DEBUG (symbolP))
	  {
	    /*
	     *	Dispatch on STAB type
	     */
	    switch ((unsigned char) S_GET_RAW_TYPE (symbolP))
	      {
	      case N_SLINE:
		if (S_GET_DESC (symbolP) > Current_File->max_line)
		  Current_File->max_line = S_GET_DESC (symbolP);
		if (S_GET_DESC (symbolP) < Current_File->min_line)
		  Current_File->min_line = S_GET_DESC (symbolP);
		break;
	      case N_SO:
		Current_File = find_file (symbolP);
		Current_File->flag = 1;
		Current_File->min_line = 1;
		break;
	      case N_SOL:
		Current_File = find_file (symbolP);
		break;
	      case N_GSYM:
		VMS_GSYM_Parse (symbolP, Text_Psect);
		break;
	      case N_LCSYM:
		VMS_LCSYM_Parse (symbolP, Text_Psect);
		break;
	      case N_FUN:	/* For static constant symbols */
	      case N_STSYM:
		VMS_STSYM_Parse (symbolP, Text_Psect);
		break;
	      }
	  }
      }

    /* now we take a quick sweep through the files and assign offsets
    to each one.  This will essentially be the starting line number to the
   debugger for each file.  Output the info for the debugger to specify the
   files, and then tell it how many lines to use */
    {
      int File_Number = 0;
      int Debugger_Offset = 0;
      int file_available;
      Current_File = file_root;
      for (Current_File = file_root; Current_File; Current_File = Current_File->next)
	{
	  if (Current_File == (struct input_file *) NULL)
	    break;
	  if (Current_File->max_line == 0)
	    continue;
	  if ((strncmp (Current_File->name, "GNU_GXX_INCLUDE:", 16) == 0) &&
	      !flagseen['D'])
	    continue;
	  if ((strncmp (Current_File->name, "GNU_CC_INCLUDE:", 15) == 0) &&
	      !flagseen['D'])
	    continue;
/* show a few extra lines at the start of the region selected */
	  if (Current_File->min_line > 2)
	    Current_File->min_line -= 2;
	  Current_File->offset = Debugger_Offset - Current_File->min_line + 1;
	  Debugger_Offset += Current_File->max_line - Current_File->min_line + 1;
	  if (Current_File->same_file_fpnt != (struct input_file *) NULL)
	    Current_File->file_number = Current_File->same_file_fpnt->file_number;
	  else
	    {
	      Current_File->file_number = ++File_Number;
	      file_available = VMS_TBT_Source_File (Current_File->name,
						 Current_File->file_number);
	      if (!file_available)
		{
		  Current_File->file_number = 0;
		  File_Number--;
		  continue;
		};
	    };
	  VMS_TBT_Source_Lines (Current_File->file_number,
				Current_File->min_line,
		       Current_File->max_line - Current_File->min_line + 1);
	};			/* for */
    };				/* scope */
    Current_File = (struct input_file *) NULL;

    for (symbolP = symbol_rootP; symbolP; symbolP = symbol_next (symbolP))
      {
	/*
	 *	Deal with text symbols
	 */
	if (!S_IS_DEBUG (symbolP) && (S_GET_TYPE (symbolP) == N_TEXT))
	  {
	    /*
	     *	Ignore symbols starting with "L",
	     *	as they are local symbols
	     */
	    if (*S_GET_NAME (symbolP) == 'L')
	      continue;
	    /*
	     *	If there is a routine start defined,
	     *	terminate it.
	     */
	    if (Current_Routine)
	      {
		/*
		 *	End the routine
		 */
		VMS_TBT_Routine_End (text_siz, Current_Routine);
	      }
	    /*
	     *	Store the routine begin traceback info
	     */
	    if (Text_Psect != -1)
	      {
		VMS_TBT_Routine_Begin (symbolP, Text_Psect);
		Current_Routine = symbolP;
	      }
/* Output local symbols, i.e. all symbols that are associated with a specific
 * routine.  We output them now so the debugger recognizes them as local to
 * this routine.
 */
	    {
	      symbolS *symbolP1;
	      char *pnt;
	      char *pnt1;
	      for (symbolP1 = Current_Routine; symbolP1; symbolP1 = symbol_next (symbolP1))
		{
		  if (!S_IS_DEBUG (symbolP1))
		    continue;
		  if (S_GET_RAW_TYPE (symbolP1) != N_FUN)
		    continue;
		  pnt = S_GET_NAME (symbolP);
		  pnt1 = S_GET_NAME (symbolP1);
		  if (*pnt++ != '_')
		    continue;
		  while (*pnt++ == *pnt1++)
		    {
		    };
		  if (*pnt1 != 'F' && *pnt1 != 'f') continue;
		  if ((*(--pnt) == '\0') && (*(--pnt1) == ':'))
		    break;
		};
	      if (symbolP1 != (symbolS *) NULL)
		VMS_DBG_Define_Routine (symbolP1, Current_Routine, Text_Psect);
	    }			/* local symbol block */
	    /*
	     *	Done
	     */
	    continue;
	  }
	/*
	 *	Deal with STAB symbols
	 */
	if (S_IS_DEBUG (symbolP))
	  {
	    /*
	     *	Dispatch on STAB type
	     */
	    switch ((unsigned char) S_GET_RAW_TYPE (symbolP))
	      {
		/*
		 *	Line number
		 */
	      case N_SLINE:
		/* Offset the line into the correct portion
		 * of the file */
		if (Current_File->file_number == 0)
		  break;
		/* Sometimes the same offset gets several source
		 * lines assigned to it.
		 * We should be selective about which lines
		 * we allow, we should prefer lines that are
		 * in the main source file when debugging
		 * inline functions. */
		if ((Current_File->file_number != 1) &&
		    S_GET_VALUE (symbolP) ==
		    Current_Offset)
		  break;
		/* calculate actual debugger source line */
		S_GET_DESC (symbolP)
		  += Current_File->offset;
		/*
		 *	If this is the 1st N_SLINE, setup
		 *	PC/Line correlation.  Otherwise
		 *	do the delta PC/Line.  If the offset
		 *	for the line number is not +ve we need
		 *	to do another PC/Line correlation
		 *	setup
		 */
		if (Current_Offset == -1)
		  {
		    VMS_TBT_Line_PC_Correlation (
						  S_GET_DESC (symbolP),
						  S_GET_VALUE (symbolP),
						  Text_Psect,
						  0);
		  }
		else
		  {
		    if ((S_GET_DESC (symbolP) -
			 Current_Line_Number) <= 0)
		      {
			/*
			 *	Line delta is not +ve, we
			 *	need to close the line and
			 *	start a new PC/Line
			 *	correlation.
			 */
			VMS_TBT_Line_PC_Correlation (0,
						     S_GET_VALUE (symbolP) -
						     Current_Offset,
						     0,
						     -1);
			VMS_TBT_Line_PC_Correlation (
						      S_GET_DESC (symbolP),
						      S_GET_VALUE (symbolP),
						      Text_Psect,
						      0);
		      }
		    else
		      {
			/*
			 *	Line delta is +ve, all is well
			 */
			VMS_TBT_Line_PC_Correlation (
						      S_GET_DESC (symbolP) -
						      Current_Line_Number,
						      S_GET_VALUE (symbolP) -
						      Current_Offset,
						      0,
						      1);
		      }
		  }
		/*
		 *	Update the current line/PC
		 */
		Current_Line_Number = S_GET_DESC (symbolP);
		Current_Offset = S_GET_VALUE (symbolP);
		/*
		 *	Done
		 */
		break;
		/*
		 *	Source file
		 */
	      case N_SO:
		/*
		 *	Remember that we had a source file
		 *	and emit the source file debugger
		 *	record
		 */
		Current_File =
		  find_file (symbolP);
		break;
/* We need to make sure that we are really in the actual source file when
 * we compute the maximum line number.  Otherwise the debugger gets really
 * confused */
	      case N_SOL:
		Current_File =
		  find_file (symbolP);
		break;
	      }
	  }
      }
    /*
     *	If there is a routine start defined,
     *	terminate it (and the line numbers)
     */
    if (Current_Routine)
      {
	/*
	 *	Terminate the line numbers
	 */
	VMS_TBT_Line_PC_Correlation (0,
				   text_siz - S_GET_VALUE (Current_Routine),
				     0,
				     -1);
	/*
	 *	Terminate the routine
	 */
	VMS_TBT_Routine_End (text_siz, Current_Routine);
      }
  }
  /*
   *	Write the Traceback End Module TBT record
   */
  VMS_TBT_Module_End ();

  /*
   *	Write the End Of Module record
   */
  if (Entry_Point_Symbol == 0)
    Write_VMS_EOM_Record (-1, 0);
  else
    Write_VMS_EOM_Record (Text_Psect,
			  S_GET_VALUE (Entry_Point_Symbol));

  /*
   *	All done, close the object file
   */
  Close_VMS_Object_File ();
}

/* end of obj-vms.c */
