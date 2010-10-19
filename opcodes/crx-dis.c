/* Disassembler code for CRX.
   Copyright 2004, 2005 Free Software Foundation, Inc.
   Contributed by Tomer Levi, NSC, Israel.
   Written by Tomer Levi.

   This file is part of the GNU binutils and GDB, the GNU debugger.

   This program is free software; you can redistribute it and/or modify it under
   the terms of the GNU General Public License as published by the Free
   Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful, but WITHOUT
   ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
   FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
   more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street - Fifth Floor, Boston, MA 02110-1301, USA.  */

#include "dis-asm.h"
#include "sysdep.h"
#include "opcode/crx.h"

/* String to print when opcode was not matched.  */
#define ILLEGAL	"illegal"
  /* Escape to 16-bit immediate.  */
#define ESCAPE_16_BIT  0xE

/* Extract 'n_bits' from 'a' starting from offset 'offs'.  */
#define EXTRACT(a, offs, n_bits)	    \
  (n_bits == 32 ? (((a) >> (offs)) & 0xffffffffL)   \
  : (((a) >> (offs)) & ((1 << (n_bits)) -1)))

/* Set Bit Mask - a mask to set all bits starting from offset 'offs'.  */
#define SBM(offs)  ((((1 << (32 - offs)) -1) << (offs)))

typedef unsigned long dwordU;
typedef unsigned short wordU;

typedef struct
{
  dwordU val;
  int nbits;
} parameter;

/* Structure to hold valid 'cinv' instruction options.  */

typedef struct
  {
    /* Cinv printed string.  */
    char *str;
    /* Value corresponding to the string.  */
    unsigned int value;
  }
cinv_entry;

/* CRX 'cinv' options.  */
const cinv_entry crx_cinvs[] =
{
  {"[i]", 2}, {"[i,u]", 3}, {"[d]", 4}, {"[d,u]", 5}, 
  {"[d,i]", 6}, {"[d,i,u]", 7}, {"[b]", 8}, 
  {"[b,i]", 10}, {"[b,i,u]", 11}, {"[b,d]", 12}, 
  {"[b,d,u]", 13}, {"[b,d,i]", 14}, {"[b,d,i,u]", 15}
};

/* Enum to distinguish different registers argument types.  */
typedef enum REG_ARG_TYPE
  {
    /* General purpose register (r<N>).  */
    REG_ARG = 0,
    /* User register (u<N>).  */
    USER_REG_ARG,
    /* CO-Processor register (c<N>).  */
    COP_ARG,
    /* CO-Processor special register (cs<N>).  */
    COPS_ARG 
  }
REG_ARG_TYPE;

/* Number of valid 'cinv' instruction options.  */
int NUMCINVS = ((sizeof crx_cinvs)/(sizeof crx_cinvs[0]));
/* Current opcode table entry we're disassembling.  */
const inst *instruction;
/* Current instruction we're disassembling.  */
ins currInsn;
/* The current instruction is read into 3 consecutive words.  */
wordU words[3];
/* Contains all words in appropriate order.  */
ULONGLONG allWords;
/* Holds the current processed argument number.  */
int processing_argument_number;
/* Nonzero means a CST4 instruction.  */
int cst4flag;
/* Nonzero means the instruction's original size is
   incremented (escape sequence is used).  */
int size_changed;

static int get_number_of_operands (void);
static argtype getargtype     (operand_type);
static int getbits	      (operand_type);
static char *getregname	      (reg);
static char *getcopregname    (copreg, reg_type);
static char * getprocregname  (int);
static char *gettrapstring    (unsigned);
static char *getcinvstring    (unsigned);
static void getregliststring  (int, char *, enum REG_ARG_TYPE);
static wordU get_word_at_PC   (bfd_vma, struct disassemble_info *);
static void get_words_at_PC   (bfd_vma, struct disassemble_info *);
static unsigned long build_mask (void);
static int powerof2	      (int);
static int match_opcode	      (void);
static void make_instruction  (void);
static void print_arguments   (ins *, bfd_vma, struct disassemble_info *);
static void print_arg	      (argument *, bfd_vma, struct disassemble_info *);

/* Retrieve the number of operands for the current assembled instruction.  */

static int
get_number_of_operands (void)
{
  int i;

  for (i = 0; instruction->operands[i].op_type && i < MAX_OPERANDS; i++)
    ;

  return i;
}

/* Return the bit size for a given operand.  */

static int
getbits (operand_type op)
{
  if (op < MAX_OPRD)
    return crx_optab[op].bit_size;
  else
    return 0;
}

/* Return the argument type of a given operand.  */

static argtype
getargtype (operand_type op)
{
  if (op < MAX_OPRD)
    return crx_optab[op].arg_type;
  else
    return nullargs;
}

/* Given the trap index in dispatch table, return its name.
   This routine is used when disassembling the 'excp' instruction.  */

static char *
gettrapstring (unsigned int index)
{
  const trap_entry *trap;

  for (trap = crx_traps; trap < crx_traps + NUMTRAPS; trap++)
    if (trap->entry == index)
      return trap->name;

  return ILLEGAL;
}

/* Given a 'cinv' instruction constant operand, return its corresponding string.
   This routine is used when disassembling the 'cinv' instruction.  */

static char *
getcinvstring (unsigned int num)
{
  const cinv_entry *cinv;

  for (cinv = crx_cinvs; cinv < (crx_cinvs + NUMCINVS); cinv++)
    if (cinv->value == num)
      return cinv->str;

  return ILLEGAL;
}

/* Given a register enum value, retrieve its name.  */

char *
getregname (reg r)
{
  const reg_entry *reg = &crx_regtab[r];

  if (reg->type != CRX_R_REGTYPE)
    return ILLEGAL;
  else
    return reg->name;
}

/* Given a coprocessor register enum value, retrieve its name.  */

char *
getcopregname (copreg r, reg_type type)
{
  const reg_entry *reg;

  if (type == CRX_C_REGTYPE)
    reg = &crx_copregtab[r];
  else if (type == CRX_CS_REGTYPE)
    reg = &crx_copregtab[r+(cs0-c0)];
  else
    return ILLEGAL;

  return reg->name;
}


/* Getting a processor register name.  */

static char *
getprocregname (int index)
{
  const reg_entry *r;

  for (r = crx_regtab; r < crx_regtab + NUMREGS; r++)
    if (r->image == index)
      return r->name;

  return "ILLEGAL REGISTER";
}

/* Get the power of two for a given integer.  */

static int
powerof2 (int x)
{
  int product, i;

  for (i = 0, product = 1; i < x; i++)
    product *= 2;

  return product;
}

/* Transform a register bit mask to a register list.  */

void
getregliststring (int mask, char *string, enum REG_ARG_TYPE core_cop)
{
  char temp_string[5];
  int i;

  string[0] = '{';
  string[1] = '\0';


  /* A zero mask means HI/LO registers.  */
  if (mask == 0)
    {
      if (core_cop == USER_REG_ARG)
	strcat (string, "ulo,uhi");
      else
	strcat (string, "lo,hi");
    }
  else
    {
      for (i = 0; i < 16; i++)
	{
	  if (mask & 0x1)
	    {
	      switch (core_cop)
	      {
	      case REG_ARG:
		sprintf (temp_string, "r%d", i);
		break;
	      case USER_REG_ARG:
		sprintf (temp_string, "u%d", i);
		break;
	      case COP_ARG:
		sprintf (temp_string, "c%d", i);
		break;
	      case COPS_ARG:
		sprintf (temp_string, "cs%d", i);
		break;
	      default:
		break;
	      }
	      strcat (string, temp_string);
	      if (mask & 0xfffe)
		strcat (string, ",");
	    }
	  mask >>= 1;
	}
    }

  strcat (string, "}");
}

/* START and END are relating 'allWords' struct, which is 48 bits size.

			  START|--------|END
	    +---------+---------+---------+---------+
	    |	      |	   V    |     A	  |   L	    |
	    +---------+---------+---------+---------+
	    	      0		16	  32	    48
    words		  [0]	    [1]	      [2]	*/

static parameter
makelongparameter (ULONGLONG val, int start, int end)
{
  parameter p;

  p.val = (dwordU) EXTRACT(val, 48 - end, end - start);
  p.nbits = end - start;
  return p;
}

/* Build a mask of the instruction's 'constant' opcode,
   based on the instruction's printing flags.  */

static unsigned long
build_mask (void)
{
  unsigned int print_flags;
  unsigned long mask;

  print_flags = instruction->flags & FMT_CRX;
  switch (print_flags)
    {
      case FMT_1:
	mask = 0xF0F00000;
	break;
      case FMT_2:
	mask = 0xFFF0FF00;
	break;
      case FMT_3:
	mask = 0xFFF00F00;
	break;
      case FMT_4:
	mask = 0xFFF0F000;
	break;
      case FMT_5:
	mask = 0xFFF0FFF0;
	break;
      default:
	mask = SBM(instruction->match_bits);
	break;
    }

  return mask;
}

/* Search for a matching opcode. Return 1 for success, 0 for failure.  */

static int
match_opcode (void)
{
  unsigned long mask;

  /* The instruction 'constant' opcode doewsn't exceed 32 bits.  */
  unsigned long doubleWord = words[1] + (words[0] << 16);

  /* Start searching from end of instruction table.  */
  instruction = &crx_instruction[NUMOPCODES - 2];

  /* Loop over instruction table until a full match is found.  */
  while (instruction >= crx_instruction)
    {
      mask = build_mask ();
      if ((doubleWord & mask) == BIN(instruction->match, instruction->match_bits))
	return 1;
      else
	instruction--;
    }
  return 0;
}

/* Set the proper parameter value for different type of arguments.  */

static void
make_argument (argument * a, int start_bits)
{
  int inst_bit_size, total_size;
  parameter p;

  if ((instruction->size == 3) && a->size >= 16)
    inst_bit_size = 48;
  else
    inst_bit_size = 32;

  switch (a->type)
    {
    case arg_copr:
    case arg_copsr:
      p = makelongparameter (allWords, inst_bit_size - (start_bits + a->size),
			     inst_bit_size - start_bits);
      a->cr = p.val;
      break;

    case arg_r:
      p = makelongparameter (allWords, inst_bit_size - (start_bits + a->size),
			     inst_bit_size - start_bits);
      a->r = p.val;
      break;

    case arg_ic:
      p = makelongparameter (allWords, inst_bit_size - (start_bits + a->size),
			     inst_bit_size - start_bits);

      if ((p.nbits == 4) && cst4flag)
        {
	  if (IS_INSN_TYPE (CMPBR_INS) && (p.val == ESCAPE_16_BIT))
	    {
	      /* A special case, where the value is actually stored
		 in the last 4 bits.  */
	      p = makelongparameter (allWords, 44, 48);
	      /* The size of the instruction should be incremented.  */
	      size_changed = 1;
	    }

          if (p.val == 6)
            p.val = -1;
          else if (p.val == 13)
            p.val = 48;
          else if (p.val == 5)
            p.val = -4;
          else if (p.val == 10)
            p.val = 32;
          else if (p.val == 11)
            p.val = 20;
          else if (p.val == 9)
            p.val = 16;
        }

      a->constant = p.val;
      break;

    case arg_idxr:
      a->scale = 0;
      total_size = a->size + 10;  /* sizeof(rbase + ridx + scl2) = 10.  */
      p = makelongparameter (allWords, inst_bit_size - total_size,
			     inst_bit_size - (total_size - 4));
      a->r = p.val;
      p = makelongparameter (allWords, inst_bit_size - (total_size - 4),
			     inst_bit_size - (total_size - 8));
      a->i_r = p.val;
      p = makelongparameter (allWords, inst_bit_size - (total_size - 8),
			     inst_bit_size - (total_size - 10));
      a->scale = p.val;
      p = makelongparameter (allWords, inst_bit_size - (total_size - 10),
			     inst_bit_size);
      a->constant = p.val;
      break;

    case arg_rbase:
      p = makelongparameter (allWords, inst_bit_size - (start_bits + 4),
			     inst_bit_size - start_bits);
      a->r = p.val;
      break;

    case arg_cr:
      if (a->size <= 8)
        {
          p = makelongparameter (allWords, inst_bit_size - (start_bits + 4),
				 inst_bit_size - start_bits);
          a->r = p.val;
          /* Case for opc4 r dispu rbase.  */
          p = makelongparameter (allWords, inst_bit_size - (start_bits + 8),
				 inst_bit_size - (start_bits + 4));
        }
      else
        {
	  /* The 'rbase' start_bits is always relative to a 32-bit data type.  */
          p = makelongparameter (allWords, 32 - (start_bits + 4),
				 32 - start_bits);
          a->r = p.val;
          p = makelongparameter (allWords, 32 - start_bits,
				 inst_bit_size);
        }
      if ((p.nbits == 4) && cst4flag)
        {
          if (instruction->flags & DISPUW4)
	    p.val *= 2;
          else if (instruction->flags & DISPUD4)
	    p.val *= 4;
        }
      a->constant = p.val;
      break;

    case arg_c:
      p = makelongparameter (allWords, inst_bit_size - (start_bits + a->size),
			     inst_bit_size - start_bits);
      a->constant = p.val;
      break;
    default:
      break;
    }
}

/*  Print a single argument.  */

static void
print_arg (argument *a, bfd_vma memaddr, struct disassemble_info *info)
{
  LONGLONG longdisp, mask;
  int sign_flag = 0;
  int relative = 0;
  bfd_vma number;
  int op_index = 0;
  char string[200];
  PTR stream = info->stream;
  fprintf_ftype func = info->fprintf_func;

  switch (a->type)
    {
    case arg_copr:
      func (stream, "%s", getcopregname (a->cr, CRX_C_REGTYPE));
      break;

    case arg_copsr:
      func (stream, "%s", getcopregname (a->cr, CRX_CS_REGTYPE));
      break;

    case arg_r:
      if (IS_INSN_MNEMONIC ("mtpr") || IS_INSN_MNEMONIC ("mfpr"))
	func (stream, "%s", getprocregname (a->r));
      else
	func (stream, "%s", getregname (a->r));
      break;

    case arg_ic:
      if (IS_INSN_MNEMONIC ("excp"))
	func (stream, "%s", gettrapstring (a->constant));

      else if (IS_INSN_MNEMONIC ("cinv"))
	func (stream, "%s", getcinvstring (a->constant));

      else if (INST_HAS_REG_LIST)
        {
	  REG_ARG_TYPE reg_arg_type = IS_INSN_TYPE (COP_REG_INS) ? 
				 COP_ARG : IS_INSN_TYPE (COPS_REG_INS) ? 
				 COPS_ARG : (instruction->flags & USER_REG) ?
				 USER_REG_ARG : REG_ARG;

          if ((reg_arg_type == COP_ARG) || (reg_arg_type == COPS_ARG))
	    {
		/*  Check for proper argument number.  */
		if (processing_argument_number == 2)
		  {
		    getregliststring (a->constant, string, reg_arg_type);
		    func (stream, "%s", string);
		  }
		else
		  func (stream, "$0x%lx", a->constant);
	    }
	  else
            {
              getregliststring (a->constant, string, reg_arg_type);
              func (stream, "%s", string);
            }
        }
      else
	func (stream, "$0x%lx", a->constant);
      break;

    case arg_idxr:
      func (stream, "0x%lx(%s,%s,%d)", a->constant, getregname (a->r),
	    getregname (a->i_r), powerof2 (a->scale));
      break;

    case arg_rbase:
      func (stream, "(%s)", getregname (a->r));
      break;

    case arg_cr:
      func (stream, "0x%lx(%s)", a->constant, getregname (a->r));

      if (IS_INSN_TYPE (LD_STOR_INS_INC))
	func (stream, "+");
      break;

    case arg_c:
      /* Removed the *2 part as because implicit zeros are no more required.
	 Have to fix this as this needs a bit of extension in terms of branchins.
	 Have to add support for cmp and branch instructions.  */
      if (IS_INSN_TYPE (BRANCH_INS) || IS_INSN_MNEMONIC ("bal")
	  || IS_INSN_TYPE (CMPBR_INS) || IS_INSN_TYPE (DCR_BRANCH_INS)
	  || IS_INSN_TYPE (COP_BRANCH_INS))
        {
	  relative = 1;
          longdisp = a->constant;
          longdisp <<= 1;

          switch (a->size)
            {
            case 8:
	    case 16:
	    case 24:
	    case 32:
	      mask = ((LONGLONG)1 << a->size) - 1;
              if (longdisp & ((LONGLONG)1 << a->size))
                {
                  sign_flag = 1;
                  longdisp = ~(longdisp) + 1;
                }
              a->constant = (unsigned long int) (longdisp & mask);
              break;
            default:
	      func (stream,
		    "Wrong offset used in branch/bal instruction");
              break;
            }

        }
      /* For branch Neq instruction it is 2*offset + 2.  */
      else if (IS_INSN_TYPE (BRANCH_NEQ_INS))
	a->constant = 2 * a->constant + 2;
      else if (IS_INSN_TYPE (LD_STOR_INS_INC)
	  || IS_INSN_TYPE (LD_STOR_INS)
	  || IS_INSN_TYPE (STOR_IMM_INS)
	  || IS_INSN_TYPE (CSTBIT_INS))
        {
          op_index = instruction->flags & REVERSE_MATCH ? 0 : 1;
          if (instruction->operands[op_index].op_type == abs16)
	    a->constant |= 0xFFFF0000;
        }
      func (stream, "%s", "0x");
      number = (relative ? memaddr : 0)
	       + (sign_flag ? -a->constant : a->constant);
      (*info->print_address_func) (number, info);
      break;
    default:
      break;
    }
}

/* Print all the arguments of CURRINSN instruction.  */

static void
print_arguments (ins *currInsn, bfd_vma memaddr, struct disassemble_info *info)
{
  int i;

  for (i = 0; i < currInsn->nargs; i++)
    {
      processing_argument_number = i;

      print_arg (&currInsn->arg[i], memaddr, info);

      if (i != currInsn->nargs - 1)
	info->fprintf_func (info->stream, ", ");
    }
}

/* Build the instruction's arguments.  */

static void
make_instruction (void)
{
  int i;
  unsigned int shift;

  for (i = 0; i < currInsn.nargs; i++)
    {
      argument a;

      memset (&a, 0, sizeof (a));
      a.type = getargtype (instruction->operands[i].op_type);
      if (instruction->operands[i].op_type == cst4
	  || instruction->operands[i].op_type == rbase_dispu4)
	cst4flag = 1;
      a.size = getbits (instruction->operands[i].op_type);
      shift = instruction->operands[i].shift;

      make_argument (&a, shift);
      currInsn.arg[i] = a;
    }

  /* Calculate instruction size (in bytes).  */
  currInsn.size = instruction->size + (size_changed ? 1 : 0);
  /* Now in bits.  */
  currInsn.size *= 2;
}

/* Retrieve a single word from a given memory address.  */

static wordU
get_word_at_PC (bfd_vma memaddr, struct disassemble_info *info)
{
  bfd_byte buffer[4];
  int status;
  wordU insn = 0;

  status = info->read_memory_func (memaddr, buffer, 2, info);

  if (status == 0)
    insn = (wordU) bfd_getl16 (buffer);

  return insn;
}

/* Retrieve multiple words (3) from a given memory address.  */

static void
get_words_at_PC (bfd_vma memaddr, struct disassemble_info *info)
{
  int i;
  bfd_vma mem;

  for (i = 0, mem = memaddr; i < 3; i++, mem += 2)
    words[i] = get_word_at_PC (mem, info);

  allWords =
    ((ULONGLONG) words[0] << 32) + ((unsigned long) words[1] << 16) + words[2];
}

/* Prints the instruction by calling print_arguments after proper matching.  */

int
print_insn_crx (memaddr, info)
     bfd_vma memaddr;
     struct disassemble_info *info;
{
  int is_decoded;     /* Nonzero means instruction has a match.  */

  /* Initialize global variables.  */
  cst4flag = 0;
  size_changed = 0;

  /* Retrieve the encoding from current memory location.  */
  get_words_at_PC (memaddr, info);
  /* Find a matching opcode in table.  */
  is_decoded = match_opcode ();
  /* If found, print the instruction's mnemonic and arguments.  */
  if (is_decoded > 0 && (words[0] << 16 || words[1]) != 0)
    {
      info->fprintf_func (info->stream, "%s", instruction->mnemonic);
      if ((currInsn.nargs = get_number_of_operands ()) != 0)
	info->fprintf_func (info->stream, "\t");
      make_instruction ();
      print_arguments (&currInsn, memaddr, info);
      return currInsn.size;
    }

  /* No match found.  */
  info->fprintf_func (info->stream,"%s ",ILLEGAL);
  return 2;
}
