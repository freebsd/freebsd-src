/* Stack unwinding code based on dwarf2 frame info for GDB, the GNU debugger.
   Copyright 2001, 2002 Free Software Foundation, Inc.
   Contributed by Jiri Smid, SuSE Labs.
   Based on code written by Daniel Berlin (dan@dberlin.org).

   This file is part of GDB.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

#include "defs.h"
#include "symtab.h"
#include "symfile.h"
#include "objfiles.h"
#include "target.h"
#include "elf/dwarf2.h"
#include "inferior.h"
#include "regcache.h"
#include "dwarf2cfi.h"

/* Common Information Entry - holds information that is shared among many
   Frame Descriptors.  */
struct cie_unit
{
  /* Offset of this unit in .debug_frame or .eh_frame.  */
  ULONGEST offset;

  /* A null-terminated string that identifies the augmentation to this CIE or
     to the FDEs that use it.  */
  char *augmentation;

  /* A constant that is factored out of all advance location instructions.  */
  unsigned int code_align;

  /* A constant that is factored out of all offset instructions.  */
  int data_align;

  /* A constant that indicates which regiter represents the return address
     of a function.  */
  unsigned char ra;

  /* Indicates how addresses are encoded.  */
  unsigned char addr_encoding;

  /* Pointer and length of the cie program.  */
  char *data;
  unsigned int data_length;

  struct objfile *objfile;

  /* Next in chain.  */
  struct cie_unit *next;
};

/* Frame Description Entry.  */
struct fde_unit
{
  /* Address of the first location associated with this entry.  */
  CORE_ADDR initial_location;

  /* Length of program section described by this entry.  */
  CORE_ADDR address_range;

  /* Pointer to asociated CIE.  */
  struct cie_unit *cie_ptr;

  /* Pointer and length of the cie program.  */
  char *data;
  unsigned int data_length;
};

struct fde_array
{
  struct fde_unit **array;
  int elems;
  int array_size;
};

struct context_reg
{
  union
  {
    unsigned int reg;
    long offset;
    CORE_ADDR addr;
  }
  loc;
  enum
  {
    REG_CTX_UNSAVED,
    REG_CTX_SAVED_OFFSET,
    REG_CTX_SAVED_REG,
    REG_CTX_SAVED_ADDR,
    REG_CTX_VALUE,
  }
  how;
};

/* This is the register and unwind state for a particular frame.  */
struct context
{
  struct context_reg *reg;

  CORE_ADDR cfa;
  CORE_ADDR ra;
  void *lsda;
  int args_size;
};

struct frame_state_reg
{
  union
  {
    unsigned int reg;
    long offset;
    unsigned char *exp;
  }
  loc;
  enum
  {
    REG_UNSAVED,
    REG_SAVED_OFFSET,
    REG_SAVED_REG,
    REG_SAVED_EXP,
  }
  how;
};

struct frame_state
{
  /* Each register save state can be described in terms of a CFA slot,
     another register, or a location expression.  */
  struct frame_state_regs
  {
    struct frame_state_reg *reg;

    /* Used to implement DW_CFA_remember_state.  */
    struct frame_state_regs *prev;
  }
  regs;

  /* The CFA can be described in terms of a reg+offset or a
     location expression.  */
  long cfa_offset;
  int cfa_reg;
  unsigned char *cfa_exp;
  enum
  {
    CFA_UNSET,
    CFA_REG_OFFSET,
    CFA_EXP,
  }
  cfa_how;

  /* The PC described by the current frame state.  */
  CORE_ADDR pc;

  /* The information we care about from the CIE/FDE.  */
  int data_align;
  unsigned int code_align;
  unsigned char retaddr_column;
  unsigned char addr_encoding;

  struct objfile *objfile;
};

enum ptr_encoding
{
  PE_absptr = DW_EH_PE_absptr,
  PE_pcrel = DW_EH_PE_pcrel,
  PE_textrel = DW_EH_PE_textrel,
  PE_datarel = DW_EH_PE_datarel,
  PE_funcrel = DW_EH_PE_funcrel
};

#define UNWIND_CONTEXT(fi) ((struct context *) (fi->context))


static struct cie_unit *cie_chunks;
static struct fde_array fde_chunks;
/* Obstack for allocating temporary storage used during unwind operations.  */
static struct obstack unwind_tmp_obstack;

extern file_ptr dwarf_frame_offset;
extern unsigned int dwarf_frame_size;
extern file_ptr dwarf_eh_frame_offset;
extern unsigned int dwarf_eh_frame_size;


extern char *dwarf2_read_section (struct objfile *objfile, file_ptr offset,
				  unsigned int size);

static struct fde_unit *fde_unit_alloc (void);
static struct cie_unit *cie_unit_alloc (void);
static void fde_chunks_need_space ();

static struct context *context_alloc ();
static struct frame_state *frame_state_alloc ();
static void unwind_tmp_obstack_init ();
static void unwind_tmp_obstack_free ();
static void context_cpy (struct context *dst, struct context *src);

static unsigned int read_1u (bfd * abfd, char **p);
static int read_1s (bfd * abfd, char **p);
static unsigned int read_2u (bfd * abfd, char **p);
static int read_2s (bfd * abfd, char **p);
static unsigned int read_4u (bfd * abfd, char **p);
static int read_4s (bfd * abfd, char **p);
static ULONGEST read_8u (bfd * abfd, char **p);
static LONGEST read_8s (bfd * abfd, char **p);

static ULONGEST read_uleb128 (bfd * abfd, char **p);
static LONGEST read_sleb128 (bfd * abfd, char **p);
static CORE_ADDR read_pointer (bfd * abfd, char **p);
static CORE_ADDR read_encoded_pointer (bfd * abfd, char **p,
				       unsigned char encoding);
static enum ptr_encoding pointer_encoding (unsigned char encoding);

static LONGEST read_initial_length (bfd * abfd, char *buf, int *bytes_read);
static ULONGEST read_length (bfd * abfd, char *buf, int *bytes_read,
			     int dwarf64);

static int is_cie (ULONGEST cie_id, int dwarf64);
static int compare_fde_unit (const void *a, const void *b);
void dwarf2_build_frame_info (struct objfile *objfile);

static void execute_cfa_program (struct objfile *objfile, char *insn_ptr,
				 char *insn_end, struct context *context,
				 struct frame_state *fs);
static struct fde_unit *get_fde_for_addr (CORE_ADDR pc);
static void frame_state_for (struct context *context, struct frame_state *fs);
static void get_reg (char *reg, struct context *context, int regnum);
static CORE_ADDR execute_stack_op (struct objfile *objfile,
				   char *op_ptr, char *op_end,
				   struct context *context,
				   CORE_ADDR initial);
static void update_context (struct context *context, struct frame_state *fs,
			    int chain);


/* Memory allocation functions.  */
static struct fde_unit *
fde_unit_alloc (void)
{
  struct fde_unit *fde;

  fde = (struct fde_unit *) xmalloc (sizeof (struct fde_unit));
  memset (fde, 0, sizeof (struct fde_unit));
  return fde;
}

static struct cie_unit *
cie_unit_alloc (void)
{
  struct cie_unit *cie;

  cie = (struct cie_unit *) xmalloc (sizeof (struct cie_unit));
  memset (cie, 0, sizeof (struct cie_unit));
  return cie;
}

static void
fde_chunks_need_space ()
{
  if (fde_chunks.elems < fde_chunks.array_size)
    return;
  fde_chunks.array_size =
    fde_chunks.array_size ? 2 * fde_chunks.array_size : 1024;
  fde_chunks.array =
    xrealloc (fde_chunks.array,
	      sizeof (struct fde_unit) * fde_chunks.array_size);
}

/* Alocate a new `struct context' on temporary obstack.  */
static struct context *
context_alloc ()
{
  struct context *context;

  int regs_size = sizeof (struct context_reg) * NUM_REGS;

  context = (struct context *) obstack_alloc (&unwind_tmp_obstack,
					      sizeof (struct context));
  memset (context, 0, sizeof (struct context));
  context->reg = (struct context_reg *) obstack_alloc (&unwind_tmp_obstack,
						       regs_size);
  memset (context->reg, 0, regs_size);
  return context;
}

/* Alocate a new `struct frame_state' on temporary obstack.  */
static struct frame_state *
frame_state_alloc ()
{
  struct frame_state *fs;

  int regs_size = sizeof (struct frame_state_reg) * NUM_REGS;

  fs = (struct frame_state *) obstack_alloc (&unwind_tmp_obstack,
					     sizeof (struct frame_state));
  memset (fs, 0, sizeof (struct frame_state));
  fs->regs.reg =
    (struct frame_state_reg *) obstack_alloc (&unwind_tmp_obstack, regs_size);
  memset (fs->regs.reg, 0, regs_size);
  return fs;
}

static void
unwind_tmp_obstack_init ()
{
  obstack_init (&unwind_tmp_obstack);
}

static void
unwind_tmp_obstack_free ()
{
  obstack_free (&unwind_tmp_obstack, NULL);
  unwind_tmp_obstack_init ();
}

static void
context_cpy (struct context *dst, struct context *src)
{
  int regs_size = sizeof (struct context_reg) * NUM_REGS;
  struct context_reg *dreg;

  /* Structure dst contains a pointer to an array of
   * registers of a given frame as well as src does. This
   * array was already allocated before dst was passed to
   * context_cpy but the pointer to it was overriden by
   * '*dst = *src' and the array was lost. This led to the
   * situation, that we've had a copy of src placed in dst,
   * but both of them pointed to the same regs array and
   * thus we've sometimes blindly rewritten it.  Now we save
   * the pointer before copying src to dst, return it back
   * after that and copy the registers into their new place
   * finally.   ---   mludvig@suse.cz  */
  dreg = dst->reg;
  *dst = *src;
  dst->reg = dreg;

  memcpy (dst->reg, src->reg, regs_size);
}

static unsigned int
read_1u (bfd * abfd, char **p)
{
  unsigned ret;

  ret = bfd_get_8 (abfd, (bfd_byte *) * p);
  (*p)++;
  return ret;
}

static int
read_1s (bfd * abfd, char **p)
{
  int ret;

  ret = bfd_get_signed_8 (abfd, (bfd_byte *) * p);
  (*p)++;
  return ret;
}

static unsigned int
read_2u (bfd * abfd, char **p)
{
  unsigned ret;

  ret = bfd_get_16 (abfd, (bfd_byte *) * p);
  (*p)++;
  return ret;
}

static int
read_2s (bfd * abfd, char **p)
{
  int ret;

  ret = bfd_get_signed_16 (abfd, (bfd_byte *) * p);
  (*p) += 2;
  return ret;
}

static unsigned int
read_4u (bfd * abfd, char **p)
{
  unsigned int ret;

  ret = bfd_get_32 (abfd, (bfd_byte *) * p);
  (*p) += 4;
  return ret;
}

static int
read_4s (bfd * abfd, char **p)
{
  int ret;

  ret = bfd_get_signed_32 (abfd, (bfd_byte *) * p);
  (*p) += 4;
  return ret;
}

static ULONGEST
read_8u (bfd * abfd, char **p)
{
  ULONGEST ret;

  ret = bfd_get_64 (abfd, (bfd_byte *) * p);
  (*p) += 8;
  return ret;
}

static LONGEST
read_8s (bfd * abfd, char **p)
{
  LONGEST ret;

  ret = bfd_get_signed_64 (abfd, (bfd_byte *) * p);
  (*p) += 8;
  return ret;
}

static ULONGEST
read_uleb128 (bfd * abfd, char **p)
{
  ULONGEST ret;
  int i, shift;
  unsigned char byte;

  ret = 0;
  shift = 0;
  i = 0;
  while (1)
    {
      byte = bfd_get_8 (abfd, (bfd_byte *) * p);
      (*p)++;
      ret |= ((unsigned long) (byte & 127) << shift);
      if ((byte & 128) == 0)
	{
	  break;
	}
      shift += 7;
    }
  return ret;
}

static LONGEST
read_sleb128 (bfd * abfd, char **p)
{
  LONGEST ret;
  int i, shift, size, num_read;
  unsigned char byte;

  ret = 0;
  shift = 0;
  size = 32;
  num_read = 0;
  i = 0;
  while (1)
    {
      byte = bfd_get_8 (abfd, (bfd_byte *) * p);
      (*p)++;
      ret |= ((long) (byte & 127) << shift);
      shift += 7;
      if ((byte & 128) == 0)
	{
	  break;
	}
    }
  if ((shift < size) && (byte & 0x40))
    {
      ret |= -(1 << shift);
    }
  return ret;
}

static CORE_ADDR
read_pointer (bfd * abfd, char **p)
{
  switch (TARGET_ADDR_BIT / TARGET_CHAR_BIT)
    {
    case 4:
      return read_4u (abfd, p);
    case 8:
      return read_8u (abfd, p);
    default:
      error ("dwarf cfi error: unsupported target address length.");
    }
}

/* This functions only reads appropriate amount of data from *p 
 * and returns the resulting value. Calling function must handle
 * different encoding possibilities itself!  */
static CORE_ADDR
read_encoded_pointer (bfd * abfd, char **p, unsigned char encoding)
{
  CORE_ADDR ret;

  switch (encoding & 0x0f)
    {
    case DW_EH_PE_absptr:
      ret = read_pointer (abfd, p);
      break;

    case DW_EH_PE_uleb128:
      ret = read_uleb128 (abfd, p);
      break;
    case DW_EH_PE_sleb128:
      ret = read_sleb128 (abfd, p);
      break;

    case DW_EH_PE_udata2:
      ret = read_2u (abfd, p);
      break;
    case DW_EH_PE_udata4:
      ret = read_4u (abfd, p);
      break;
    case DW_EH_PE_udata8:
      ret = read_8u (abfd, p);
      break;

    case DW_EH_PE_sdata2:
      ret = read_2s (abfd, p);
      break;
    case DW_EH_PE_sdata4:
      ret = read_4s (abfd, p);
      break;
    case DW_EH_PE_sdata8:
      ret = read_8s (abfd, p);
      break;

    default:
      internal_error (__FILE__, __LINE__,
		      "read_encoded_pointer: unknown pointer encoding");
    }

  return ret;
}

/* Variable 'encoding' carries 3 different flags:
 * - encoding & 0x0f : size of the address (handled in read_encoded_pointer())
 * - encoding & 0x70 : type (absolute, relative, ...)
 * - encoding & 0x80 : indirect flag (DW_EH_PE_indirect == 0x80).  */
enum ptr_encoding
pointer_encoding (unsigned char encoding)
{
  int ret;

  if (encoding & DW_EH_PE_indirect)
    warning ("CFI: Unsupported pointer encoding: DW_EH_PE_indirect");

  switch (encoding & 0x70)
    {
    case DW_EH_PE_absptr:
    case DW_EH_PE_pcrel:
    case DW_EH_PE_textrel:
    case DW_EH_PE_datarel:
    case DW_EH_PE_funcrel:
      ret = encoding & 0x70;
      break;
    default:
      internal_error (__FILE__, __LINE__, "CFI: unknown pointer encoding");
    }
  return ret;
}

static LONGEST
read_initial_length (bfd * abfd, char *buf, int *bytes_read)
{
  LONGEST ret = 0;

  ret = bfd_get_32 (abfd, (bfd_byte *) buf);

  if (ret == 0xffffffff)
    {
      ret = bfd_get_64 (abfd, (bfd_byte *) buf + 4);
      *bytes_read = 12;
    }
  else
    {
      *bytes_read = 4;
    }

  return ret;
}

static ULONGEST
read_length (bfd * abfd, char *buf, int *bytes_read, int dwarf64)
{
  if (dwarf64)
    {
      *bytes_read = 8;
      return read_8u (abfd, &buf);
    }
  else
    {
      *bytes_read = 4;
      return read_4u (abfd, &buf);
    }
}

static void
execute_cfa_program (struct objfile *objfile, char *insn_ptr, char *insn_end,
		     struct context *context, struct frame_state *fs)
{
  struct frame_state_regs *unused_rs = NULL;

  /* Don't allow remember/restore between CIE and FDE programs.  */
  fs->regs.prev = NULL;

  while (insn_ptr < insn_end && fs->pc < context->ra)
    {
      unsigned char insn = *insn_ptr++;
      ULONGEST reg, uoffset;
      LONGEST offset;

      if (insn & DW_CFA_advance_loc)
	fs->pc += (insn & 0x3f) * fs->code_align;
      else if (insn & DW_CFA_offset)
	{
	  reg = insn & 0x3f;
	  uoffset = read_uleb128 (objfile->obfd, &insn_ptr);
	  offset = (long) uoffset *fs->data_align;
	  fs->regs.reg[reg].how = REG_SAVED_OFFSET;
	  fs->regs.reg[reg].loc.offset = offset;
	}
      else if (insn & DW_CFA_restore)
	{
	  reg = insn & 0x3f;
	  fs->regs.reg[reg].how = REG_UNSAVED;
	}
      else
	switch (insn)
	  {
	  case DW_CFA_set_loc:
	    fs->pc = read_encoded_pointer (objfile->obfd, &insn_ptr,
					   fs->addr_encoding);

	    if (pointer_encoding (fs->addr_encoding) != PE_absptr)
	      warning ("CFI: DW_CFA_set_loc uses relative addressing");

	    break;

	  case DW_CFA_advance_loc1:
	    fs->pc += read_1u (objfile->obfd, &insn_ptr);
	    break;
	  case DW_CFA_advance_loc2:
	    fs->pc += read_2u (objfile->obfd, &insn_ptr);
	    break;
	  case DW_CFA_advance_loc4:
	    fs->pc += read_4u (objfile->obfd, &insn_ptr);
	    break;

	  case DW_CFA_offset_extended:
	    reg = read_uleb128 (objfile->obfd, &insn_ptr);
	    uoffset = read_uleb128 (objfile->obfd, &insn_ptr);
	    offset = (long) uoffset *fs->data_align;
	    fs->regs.reg[reg].how = REG_SAVED_OFFSET;
	    fs->regs.reg[reg].loc.offset = offset;
	    break;

	  case DW_CFA_restore_extended:
	    reg = read_uleb128 (objfile->obfd, &insn_ptr);
	    fs->regs.reg[reg].how = REG_UNSAVED;
	    break;

	  case DW_CFA_undefined:
	  case DW_CFA_same_value:
	  case DW_CFA_nop:
	    break;

	  case DW_CFA_register:
	    {
	      ULONGEST reg2;
	      reg = read_uleb128 (objfile->obfd, &insn_ptr);
	      reg2 = read_uleb128 (objfile->obfd, &insn_ptr);
	      fs->regs.reg[reg].how = REG_SAVED_REG;
	      fs->regs.reg[reg].loc.reg = reg2;
	    }
	    break;

	  case DW_CFA_remember_state:
	    {
	      struct frame_state_regs *new_rs;
	      if (unused_rs)
		{
		  new_rs = unused_rs;
		  unused_rs = unused_rs->prev;
		}
	      else
		new_rs = xmalloc (sizeof (struct frame_state_regs));

	      *new_rs = fs->regs;
	      fs->regs.prev = new_rs;
	    }
	    break;

	  case DW_CFA_restore_state:
	    {
	      struct frame_state_regs *old_rs = fs->regs.prev;
	      fs->regs = *old_rs;
	      old_rs->prev = unused_rs;
	      unused_rs = old_rs;
	    }
	    break;

	  case DW_CFA_def_cfa:
	    reg = read_uleb128 (objfile->obfd, &insn_ptr);
	    uoffset = read_uleb128 (objfile->obfd, &insn_ptr);
	    fs->cfa_reg = reg;
	    fs->cfa_offset = uoffset;
	    fs->cfa_how = CFA_REG_OFFSET;
	    break;

	  case DW_CFA_def_cfa_register:
	    reg = read_uleb128 (objfile->obfd, &insn_ptr);
	    fs->cfa_reg = reg;
	    fs->cfa_how = CFA_REG_OFFSET;
	    break;

	  case DW_CFA_def_cfa_offset:
	    uoffset = read_uleb128 (objfile->obfd, &insn_ptr);
	    fs->cfa_offset = uoffset;
	    break;

	  case DW_CFA_def_cfa_expression:
	    uoffset = read_uleb128 (objfile->obfd, &insn_ptr);
	    fs->cfa_exp = insn_ptr;
	    fs->cfa_how = CFA_EXP;
	    insn_ptr += uoffset;
	    break;

	  case DW_CFA_expression:
	    reg = read_uleb128 (objfile->obfd, &insn_ptr);
	    uoffset = read_uleb128 (objfile->obfd, &insn_ptr);
	    fs->regs.reg[reg].how = REG_SAVED_EXP;
	    fs->regs.reg[reg].loc.exp = insn_ptr;
	    insn_ptr += uoffset;
	    break;

	    /* From the 2.1 draft.  */
	  case DW_CFA_offset_extended_sf:
	    reg = read_uleb128 (objfile->obfd, &insn_ptr);
	    offset = read_sleb128 (objfile->obfd, &insn_ptr);
	    offset *= fs->data_align;
	    fs->regs.reg[reg].how = REG_SAVED_OFFSET;
	    fs->regs.reg[reg].loc.offset = offset;
	    break;

	  case DW_CFA_def_cfa_sf:
	    reg = read_uleb128 (objfile->obfd, &insn_ptr);
	    offset = read_sleb128 (objfile->obfd, &insn_ptr);
	    fs->cfa_offset = offset;
	    fs->cfa_reg = reg;
	    fs->cfa_how = CFA_REG_OFFSET;
	    break;

	  case DW_CFA_def_cfa_offset_sf:
	    uoffset = read_uleb128 (objfile->obfd, &insn_ptr);
	    fs->cfa_offset = uoffset;
	    /* cfa_how deliberately not set.  */
	    break;

	  case DW_CFA_GNU_window_save:
	    /* ??? Hardcoded for SPARC register window configuration.  */
	    for (reg = 16; reg < 32; ++reg)
	      {
		fs->regs.reg[reg].how = REG_SAVED_OFFSET;
		fs->regs.reg[reg].loc.offset = (reg - 16) * sizeof (void *);
	      }
	    break;

	  case DW_CFA_GNU_args_size:
	    uoffset = read_uleb128 (objfile->obfd, &insn_ptr);
	    context->args_size = uoffset;
	    break;

	  case DW_CFA_GNU_negative_offset_extended:
	    /* Obsoleted by DW_CFA_offset_extended_sf, but used by
	       older PowerPC code.  */
	    reg = read_uleb128 (objfile->obfd, &insn_ptr);
	    uoffset = read_uleb128 (objfile->obfd, &insn_ptr);
	    offset = (long) uoffset *fs->data_align;
	    fs->regs.reg[reg].how = REG_SAVED_OFFSET;
	    fs->regs.reg[reg].loc.offset = -offset;
	    break;

	  default:
	    error ("dwarf cfi error: unknown cfa instruction %d.", insn);
	  }
    }
}

static struct fde_unit *
get_fde_for_addr (CORE_ADDR pc)
{
  size_t lo, hi;
  struct fde_unit *fde = NULL;
  lo = 0;
  hi = fde_chunks.elems;

  while (lo < hi)
    {
      size_t i = (lo + hi) / 2;
      fde = fde_chunks.array[i];
      if (pc < fde->initial_location)
	hi = i;
      else if (pc >= fde->initial_location + fde->address_range)
	lo = i + 1;
      else
	return fde;
    }
  return 0;
}

static void
frame_state_for (struct context *context, struct frame_state *fs)
{
  struct fde_unit *fde;
  struct cie_unit *cie;

  context->args_size = 0;
  context->lsda = 0;

  fde = get_fde_for_addr (context->ra - 1);

  if (fde == NULL)
    return;

  fs->pc = fde->initial_location;

  if (fde->cie_ptr)
    {
      cie = fde->cie_ptr;

      fs->code_align = cie->code_align;
      fs->data_align = cie->data_align;
      fs->retaddr_column = cie->ra;
      fs->addr_encoding = cie->addr_encoding;
      fs->objfile = cie->objfile;

      execute_cfa_program (cie->objfile, cie->data,
			   cie->data + cie->data_length, context, fs);
      execute_cfa_program (cie->objfile, fde->data,
			   fde->data + fde->data_length, context, fs);
    }
  else
    internal_error (__FILE__, __LINE__,
		    "%s(): Internal error: fde->cie_ptr==NULL !", "?func?");
}

static void
get_reg (char *reg, struct context *context, int regnum)
{
  switch (context->reg[regnum].how)
    {
    case REG_CTX_UNSAVED:
      read_register_gen (regnum, reg);
      break;
    case REG_CTX_SAVED_OFFSET:
      target_read_memory (context->cfa + context->reg[regnum].loc.offset,
			  reg, REGISTER_RAW_SIZE (regnum));
      break;
    case REG_CTX_SAVED_REG:
      read_register_gen (context->reg[regnum].loc.reg, reg);
      break;
    case REG_CTX_SAVED_ADDR:
      target_read_memory (context->reg[regnum].loc.addr,
			  reg, REGISTER_RAW_SIZE (regnum));
      break;
    case REG_CTX_VALUE:
      memcpy (reg, &context->reg[regnum].loc.addr,
	      REGISTER_RAW_SIZE (regnum));
      break;
    default:
      internal_error (__FILE__, __LINE__, "get_reg: unknown register rule");
    }
}

/* Decode a DW_OP stack program.  Return the top of stack.  Push INITIAL
   onto the stack to start.  */
static CORE_ADDR
execute_stack_op (struct objfile *objfile,
		  char *op_ptr, char *op_end, struct context *context,
		  CORE_ADDR initial)
{
  CORE_ADDR stack[64];		/* ??? Assume this is enough. */
  int stack_elt;

  stack[0] = initial;
  stack_elt = 1;

  while (op_ptr < op_end)
    {
      enum dwarf_location_atom op = *op_ptr++;
      CORE_ADDR result;
      ULONGEST reg;
      LONGEST offset;

      switch (op)
	{
	case DW_OP_lit0:
	case DW_OP_lit1:
	case DW_OP_lit2:
	case DW_OP_lit3:
	case DW_OP_lit4:
	case DW_OP_lit5:
	case DW_OP_lit6:
	case DW_OP_lit7:
	case DW_OP_lit8:
	case DW_OP_lit9:
	case DW_OP_lit10:
	case DW_OP_lit11:
	case DW_OP_lit12:
	case DW_OP_lit13:
	case DW_OP_lit14:
	case DW_OP_lit15:
	case DW_OP_lit16:
	case DW_OP_lit17:
	case DW_OP_lit18:
	case DW_OP_lit19:
	case DW_OP_lit20:
	case DW_OP_lit21:
	case DW_OP_lit22:
	case DW_OP_lit23:
	case DW_OP_lit24:
	case DW_OP_lit25:
	case DW_OP_lit26:
	case DW_OP_lit27:
	case DW_OP_lit28:
	case DW_OP_lit29:
	case DW_OP_lit30:
	case DW_OP_lit31:
	  result = op - DW_OP_lit0;
	  break;

	case DW_OP_addr:
	  result = read_pointer (objfile->obfd, &op_ptr);
	  break;

	case DW_OP_const1u:
	  result = read_1u (objfile->obfd, &op_ptr);
	  break;
	case DW_OP_const1s:
	  result = read_1s (objfile->obfd, &op_ptr);
	  break;
	case DW_OP_const2u:
	  result = read_2u (objfile->obfd, &op_ptr);
	  break;
	case DW_OP_const2s:
	  result = read_2s (objfile->obfd, &op_ptr);
	  break;
	case DW_OP_const4u:
	  result = read_4u (objfile->obfd, &op_ptr);
	  break;
	case DW_OP_const4s:
	  result = read_4s (objfile->obfd, &op_ptr);
	  break;
	case DW_OP_const8u:
	  result = read_8u (objfile->obfd, &op_ptr);
	  break;
	case DW_OP_const8s:
	  result = read_8s (objfile->obfd, &op_ptr);
	  break;
	case DW_OP_constu:
	  result = read_uleb128 (objfile->obfd, &op_ptr);
	  break;
	case DW_OP_consts:
	  result = read_sleb128 (objfile->obfd, &op_ptr);
	  break;

	case DW_OP_reg0:
	case DW_OP_reg1:
	case DW_OP_reg2:
	case DW_OP_reg3:
	case DW_OP_reg4:
	case DW_OP_reg5:
	case DW_OP_reg6:
	case DW_OP_reg7:
	case DW_OP_reg8:
	case DW_OP_reg9:
	case DW_OP_reg10:
	case DW_OP_reg11:
	case DW_OP_reg12:
	case DW_OP_reg13:
	case DW_OP_reg14:
	case DW_OP_reg15:
	case DW_OP_reg16:
	case DW_OP_reg17:
	case DW_OP_reg18:
	case DW_OP_reg19:
	case DW_OP_reg20:
	case DW_OP_reg21:
	case DW_OP_reg22:
	case DW_OP_reg23:
	case DW_OP_reg24:
	case DW_OP_reg25:
	case DW_OP_reg26:
	case DW_OP_reg27:
	case DW_OP_reg28:
	case DW_OP_reg29:
	case DW_OP_reg30:
	case DW_OP_reg31:
	  get_reg ((char *) &result, context, op - DW_OP_reg0);
	  break;
	case DW_OP_regx:
	  reg = read_uleb128 (objfile->obfd, &op_ptr);
	  get_reg ((char *) &result, context, reg);
	  break;

	case DW_OP_breg0:
	case DW_OP_breg1:
	case DW_OP_breg2:
	case DW_OP_breg3:
	case DW_OP_breg4:
	case DW_OP_breg5:
	case DW_OP_breg6:
	case DW_OP_breg7:
	case DW_OP_breg8:
	case DW_OP_breg9:
	case DW_OP_breg10:
	case DW_OP_breg11:
	case DW_OP_breg12:
	case DW_OP_breg13:
	case DW_OP_breg14:
	case DW_OP_breg15:
	case DW_OP_breg16:
	case DW_OP_breg17:
	case DW_OP_breg18:
	case DW_OP_breg19:
	case DW_OP_breg20:
	case DW_OP_breg21:
	case DW_OP_breg22:
	case DW_OP_breg23:
	case DW_OP_breg24:
	case DW_OP_breg25:
	case DW_OP_breg26:
	case DW_OP_breg27:
	case DW_OP_breg28:
	case DW_OP_breg29:
	case DW_OP_breg30:
	case DW_OP_breg31:
	  offset = read_sleb128 (objfile->obfd, &op_ptr);
	  get_reg ((char *) &result, context, op - DW_OP_breg0);
	  result += offset;
	  break;
	case DW_OP_bregx:
	  reg = read_uleb128 (objfile->obfd, &op_ptr);
	  offset = read_sleb128 (objfile->obfd, &op_ptr);
	  get_reg ((char *) &result, context, reg);
	  result += offset;
	  break;

	case DW_OP_dup:
	  if (stack_elt < 1)
	    internal_error (__FILE__, __LINE__, "execute_stack_op error");
	  result = stack[stack_elt - 1];
	  break;

	case DW_OP_drop:
	  if (--stack_elt < 0)
	    internal_error (__FILE__, __LINE__, "execute_stack_op error");
	  goto no_push;

	case DW_OP_pick:
	  offset = *op_ptr++;
	  if (offset >= stack_elt - 1)
	    internal_error (__FILE__, __LINE__, "execute_stack_op error");
	  result = stack[stack_elt - 1 - offset];
	  break;

	case DW_OP_over:
	  if (stack_elt < 2)
	    internal_error (__FILE__, __LINE__, "execute_stack_op error");
	  result = stack[stack_elt - 2];
	  break;

	case DW_OP_rot:
	  {
	    CORE_ADDR t1, t2, t3;

	    if (stack_elt < 3)
	      internal_error (__FILE__, __LINE__, "execute_stack_op error");
	    t1 = stack[stack_elt - 1];
	    t2 = stack[stack_elt - 2];
	    t3 = stack[stack_elt - 3];
	    stack[stack_elt - 1] = t2;
	    stack[stack_elt - 2] = t3;
	    stack[stack_elt - 3] = t1;
	    goto no_push;
	  }

	case DW_OP_deref:
	case DW_OP_deref_size:
	case DW_OP_abs:
	case DW_OP_neg:
	case DW_OP_not:
	case DW_OP_plus_uconst:
	  /* Unary operations.  */
	  if (--stack_elt < 0)
	    internal_error (__FILE__, __LINE__, "execute_stack_op error");
	  result = stack[stack_elt];

	  switch (op)
	    {
	    case DW_OP_deref:
	      {
		char *ptr = (char *) result;
		result = read_pointer (objfile->obfd, &ptr);
	      }
	      break;

	    case DW_OP_deref_size:
	      {
		char *ptr = (char *) result;
		switch (*op_ptr++)
		  {
		  case 1:
		    result = read_1u (objfile->obfd, &ptr);
		    break;
		  case 2:
		    result = read_2u (objfile->obfd, &ptr);
		    break;
		  case 4:
		    result = read_4u (objfile->obfd, &ptr);
		    break;
		  case 8:
		    result = read_8u (objfile->obfd, &ptr);
		    break;
		  default:
		    internal_error (__FILE__, __LINE__,
				    "execute_stack_op error");
		  }
	      }
	      break;

	    case DW_OP_abs:
	      if (result < 0)
		result = -result;
	      break;
	    case DW_OP_neg:
	      result = -result;
	      break;
	    case DW_OP_not:
	      result = ~result;
	      break;
	    case DW_OP_plus_uconst:
	      result += read_uleb128 (objfile->obfd, &op_ptr);
	      break;
	    default:
	      break;
	    }
	  break;

	case DW_OP_and:
	case DW_OP_div:
	case DW_OP_minus:
	case DW_OP_mod:
	case DW_OP_mul:
	case DW_OP_or:
	case DW_OP_plus:
	case DW_OP_le:
	case DW_OP_ge:
	case DW_OP_eq:
	case DW_OP_lt:
	case DW_OP_gt:
	case DW_OP_ne:
	  {
	    /* Binary operations.  */
	    CORE_ADDR first, second;
	    if ((stack_elt -= 2) < 0)
	      internal_error (__FILE__, __LINE__, "execute_stack_op error");
	    second = stack[stack_elt];
	    first = stack[stack_elt + 1];

	    switch (op)
	      {
	      case DW_OP_and:
		result = second & first;
		break;
	      case DW_OP_div:
		result = (LONGEST) second / (LONGEST) first;
		break;
	      case DW_OP_minus:
		result = second - first;
		break;
	      case DW_OP_mod:
		result = (LONGEST) second % (LONGEST) first;
		break;
	      case DW_OP_mul:
		result = second * first;
		break;
	      case DW_OP_or:
		result = second | first;
		break;
	      case DW_OP_plus:
		result = second + first;
		break;
	      case DW_OP_shl:
		result = second << first;
		break;
	      case DW_OP_shr:
		result = second >> first;
		break;
	      case DW_OP_shra:
		result = (LONGEST) second >> first;
		break;
	      case DW_OP_xor:
		result = second ^ first;
		break;
	      case DW_OP_le:
		result = (LONGEST) first <= (LONGEST) second;
		break;
	      case DW_OP_ge:
		result = (LONGEST) first >= (LONGEST) second;
		break;
	      case DW_OP_eq:
		result = (LONGEST) first == (LONGEST) second;
		break;
	      case DW_OP_lt:
		result = (LONGEST) first < (LONGEST) second;
		break;
	      case DW_OP_gt:
		result = (LONGEST) first > (LONGEST) second;
		break;
	      case DW_OP_ne:
		result = (LONGEST) first != (LONGEST) second;
		break;
	      default:		/* This label is here just to avoid warning.  */
		break;
	      }
	  }
	  break;

	case DW_OP_skip:
	  offset = read_2s (objfile->obfd, &op_ptr);
	  op_ptr += offset;
	  goto no_push;

	case DW_OP_bra:
	  if (--stack_elt < 0)
	    internal_error (__FILE__, __LINE__, "execute_stack_op error");
	  offset = read_2s (objfile->obfd, &op_ptr);
	  if (stack[stack_elt] != 0)
	    op_ptr += offset;
	  goto no_push;

	case DW_OP_nop:
	  goto no_push;

	default:
	  internal_error (__FILE__, __LINE__, "execute_stack_op error");
	}

      /* Most things push a result value.  */
      if ((size_t) stack_elt >= sizeof (stack) / sizeof (*stack))
	internal_error (__FILE__, __LINE__, "execute_stack_op error");
      stack[++stack_elt] = result;
    no_push:;
    }

  /* We were executing this program to get a value.  It should be
     at top of stack.  */
  if (--stack_elt < 0)
    internal_error (__FILE__, __LINE__, "execute_stack_op error");
  return stack[stack_elt];
}

static void
update_context (struct context *context, struct frame_state *fs, int chain)
{
  struct context *orig_context;
  CORE_ADDR cfa;
  long i;

  unwind_tmp_obstack_init ();

  orig_context = context_alloc ();
  context_cpy (orig_context, context);

  /* Compute this frame's CFA.  */
  switch (fs->cfa_how)
    {
    case CFA_REG_OFFSET:
      get_reg ((char *) &cfa, context, fs->cfa_reg);
      cfa += fs->cfa_offset;
      break;

    case CFA_EXP:
      /* ??? No way of knowing what register number is the stack pointer
         to do the same sort of handling as above.  Assume that if the
         CFA calculation is so complicated as to require a stack program
         that this will not be a problem.  */
      {
	char *exp = fs->cfa_exp;
	ULONGEST len;

	len = read_uleb128 (fs->objfile->obfd, &exp);
	cfa = (CORE_ADDR) execute_stack_op (fs->objfile, exp,
					    exp + len, context, 0);
	break;
      }
    default:
      break;
    }
  context->cfa = cfa;

  if (!chain)
    orig_context->cfa = cfa;

  /* Compute the addresses of all registers saved in this frame.  */
  for (i = 0; i < NUM_REGS; ++i)
    switch (fs->regs.reg[i].how)
      {
      case REG_UNSAVED:
	if (i == SP_REGNUM)
	  {
	    context->reg[i].how = REG_CTX_VALUE;
	    context->reg[i].loc.addr = cfa;
	  }
	else
	  context->reg[i].how = REG_CTX_UNSAVED;
	break;
      case REG_SAVED_OFFSET:
	context->reg[i].how = REG_CTX_SAVED_OFFSET;
	context->reg[i].loc.offset = fs->regs.reg[i].loc.offset;
	break;
      case REG_SAVED_REG:
	switch (orig_context->reg[fs->regs.reg[i].loc.reg].how)
	  {
	  case REG_CTX_UNSAVED:
	    context->reg[i].how = REG_CTX_UNSAVED;
	    break;
	  case REG_CTX_SAVED_OFFSET:
	    context->reg[i].how = REG_CTX_SAVED_OFFSET;
	    context->reg[i].loc.offset = orig_context->cfa - context->cfa +
	      orig_context->reg[fs->regs.reg[i].loc.reg].loc.offset;
	    break;
	  case REG_CTX_SAVED_REG:
	    context->reg[i].how = REG_CTX_SAVED_REG;
	    context->reg[i].loc.reg =
	      orig_context->reg[fs->regs.reg[i].loc.reg].loc.reg;
	    break;
	  case REG_CTX_SAVED_ADDR:
	    context->reg[i].how = REG_CTX_SAVED_ADDR;
	    context->reg[i].loc.addr =
	      orig_context->reg[fs->regs.reg[i].loc.reg].loc.addr;
	  default:
	    internal_error (__FILE__, __LINE__,
			    "%s: unknown register rule", "?func?");
	  }
	break;
      case REG_SAVED_EXP:
	{
	  char *exp = fs->regs.reg[i].loc.exp;
	  ULONGEST len;
	  CORE_ADDR val;

	  len = read_uleb128 (fs->objfile->obfd, &exp);
	  val = execute_stack_op (fs->objfile, exp, exp + len,
				  orig_context, cfa);
	  context->reg[i].how = REG_CTX_SAVED_ADDR;
	  context->reg[i].loc.addr = val;
	}
	break;
      default:
	internal_error (__FILE__, __LINE__,
			"%s: unknown register rule", "?func?");
      }
  get_reg ((char *) &context->ra, context, fs->retaddr_column);
  unwind_tmp_obstack_free ();
}

static int
is_cie (ULONGEST cie_id, int dwarf64)
{
  return dwarf64 ? (cie_id == 0xffffffffffffffff) : (cie_id == 0xffffffff);
}

static int
compare_fde_unit (const void *a, const void *b)
{
  struct fde_unit **first, **second;
  first = (struct fde_unit **) a;
  second = (struct fde_unit **) b;
  if ((*first)->initial_location > (*second)->initial_location)
    return 1;
  else if ((*first)->initial_location < (*second)->initial_location)
    return -1;
  else
    return 0;
}

/*  Build the cie_chunks and fde_chunks tables from informations
    found in .debug_frame and .eh_frame sections.  */
/* We can handle both of these sections almost in the same way, however there
   are some exceptions:
   - CIE ID is -1 in debug_frame, but 0 in eh_frame
   - eh_frame may contain some more information that are used only by gcc 
     (eg. personality pointer, LSDA pointer, ...). Most of them we can ignore.
   - In debug_frame FDE's item cie_id contains offset of it's parent CIE.
     In eh_frame FDE's item cie_id is a relative pointer to the parent CIE.
     Anyway we don't need to bother with this, because we are smart enough 
     to keep the pointer to the parent CIE of oncomming FDEs in 'last_cie'.
   - Although debug_frame items can contain Augmentation as well as 
     eh_frame ones, I have never seen them non-empty. Thus only in eh_frame 
     we can encounter for example non-absolute pointers (Aug. 'R').  
                                                              -- mludvig  */
static void
parse_frame_info (struct objfile *objfile, file_ptr frame_offset,
		  unsigned int frame_size, int eh_frame)
{
  bfd *abfd = objfile->obfd;
  asection *curr_section_ptr;
  char *start = NULL;
  char *end = NULL;
  char *frame_buffer = NULL;
  char *curr_section_name, *aug_data;
  struct cie_unit *last_cie = NULL;
  int last_dup_fde = 0;
  int aug_len, i;
  CORE_ADDR curr_section_vma = 0;

  unwind_tmp_obstack_init ();

  frame_buffer = dwarf2_read_section (objfile, frame_offset, frame_size);

  start = frame_buffer;
  end = frame_buffer + frame_size;

  curr_section_name = eh_frame ? ".eh_frame" : ".debug_frame";
  curr_section_ptr = bfd_get_section_by_name (abfd, curr_section_name);
  if (curr_section_ptr)
    curr_section_vma = curr_section_ptr->vma;

  if (start)
    {
      while (start < end)
	{
	  unsigned long length;
	  ULONGEST cie_id;
	  ULONGEST unit_offset = start - frame_buffer;
	  int bytes_read, dwarf64;
	  char *block_end;

	  length = read_initial_length (abfd, start, &bytes_read);
	  start += bytes_read;
	  dwarf64 = (bytes_read == 12);
	  block_end = start + length;

	  if (length == 0)
	    {
	      start = block_end;
	      continue;
	    }

	  cie_id = read_length (abfd, start, &bytes_read, dwarf64);
	  start += bytes_read;

	  if ((eh_frame && cie_id == 0) || is_cie (cie_id, dwarf64))
	    {
	      struct cie_unit *cie = cie_unit_alloc ();
	      char *aug;

	      cie->objfile = objfile;
	      cie->next = cie_chunks;
	      cie_chunks = cie;

	      cie->objfile = objfile;

	      cie->offset = unit_offset;

	      start++;		/* version */

	      cie->augmentation = aug = start;
	      while (*start++);	/* Skips last NULL as well */

	      cie->code_align = read_uleb128 (abfd, &start);
	      cie->data_align = read_sleb128 (abfd, &start);
	      cie->ra = read_1u (abfd, &start);

	      /* Augmentation:
	         z      Indicates that a uleb128 is present to size the
	         augmentation section.
	         L      Indicates the encoding (and thus presence) of
	         an LSDA pointer in the FDE augmentation.
	         R      Indicates a non-default pointer encoding for
	         FDE code pointers.
	         P      Indicates the presence of an encoding + language
	         personality routine in the CIE augmentation.

	         [This info comes from GCC's dwarf2out.c]
	       */
	      if (*aug == 'z')
		{
		  aug_len = read_uleb128 (abfd, &start);
		  aug_data = start;
		  start += aug_len;
		  ++aug;
		}

	      cie->data = start;
	      cie->data_length = block_end - cie->data;

	      while (*aug != '\0')
		{
		  if (aug[0] == 'e' && aug[1] == 'h')
		    {
		      aug_data += sizeof (void *);
		      aug++;
		    }
		  else if (aug[0] == 'R')
		    cie->addr_encoding = *aug_data++;
		  else if (aug[0] == 'P')
		    {
		      CORE_ADDR pers_addr;
		      int pers_addr_enc;

		      pers_addr_enc = *aug_data++;
		      /* We don't need pers_addr value and so we 
		         don't care about it's encoding.  */
		      pers_addr = read_encoded_pointer (abfd, &aug_data,
							pers_addr_enc);
		    }
		  else if (aug[0] == 'L' && eh_frame)
		    {
		      int lsda_addr_enc;

		      /* Perhaps we should save this to CIE for later use?
		         Do we need it for something in GDB?  */
		      lsda_addr_enc = *aug_data++;
		    }
		  else
		    warning ("CFI warning: unknown augmentation \"%c\""
			     " in \"%s\" of\n"
			     "\t%s", aug[0], curr_section_name,
			     objfile->name);
		  aug++;
		}

	      last_cie = cie;
	    }
	  else
	    {
	      struct fde_unit *fde;
	      struct cie_unit *cie;
	      int dup = 0;
	      CORE_ADDR init_loc;

	      /* We assume that debug_frame is in order 
	         CIE,FDE,CIE,FDE,FDE,...  and thus the CIE for this FDE
	         should be stored in last_cie pointer. If not, we'll 
	         try to find it by the older way.  */
	      if (last_cie)
		cie = last_cie;
	      else
		{
		  warning ("CFI: last_cie == NULL. "
			   "Perhaps a malformed %s section in '%s'...?\n",
			   curr_section_name, objfile->name);

		  cie = cie_chunks;
		  while (cie)
		    {
		      if (cie->objfile == objfile)
			{
			  if (eh_frame &&
			      (cie->offset ==
			       (unit_offset + bytes_read - cie_id)))
			    break;
			  if (!eh_frame && (cie->offset == cie_id))
			    break;
			}

		      cie = cie->next;
		    }
		  if (!cie)
		    error ("CFI: can't find CIE pointer");
		}

	      init_loc = read_encoded_pointer (abfd, &start,
					       cie->addr_encoding);

	      switch (pointer_encoding (cie->addr_encoding))
		{
		case PE_absptr:
		  break;
		case PE_pcrel:
		  /* start-frame_buffer gives offset from 
		     the beginning of actual section.  */
		  init_loc += curr_section_vma + start - frame_buffer;
		  break;
		default:
		  warning ("CFI: Unsupported pointer encoding\n");
		}

	      /* For relocatable objects we must add an offset telling
	         where the section is actually mapped in the memory.  */
	      init_loc += ANOFFSET (objfile->section_offsets,
				    SECT_OFF_TEXT (objfile));

	      /* If we have both .debug_frame and .eh_frame present in 
	         a file, we must eliminate duplicate FDEs. For now we'll 
	         run through all entries in fde_chunks and check it one 
	         by one. Perhaps in the future we can implement a faster 
	         searching algorithm.  */
	      /* eh_frame==2 indicates, that this file has an already 
	         parsed .debug_frame too. When eh_frame==1 it means, that no
	         .debug_frame is present and thus we don't need to check for
	         duplicities. eh_frame==0 means, that we parse .debug_frame
	         and don't need to care about duplicate FDEs, because
	         .debug_frame is parsed first.  */
	      if (eh_frame == 2)
		for (i = 0; eh_frame == 2 && i < fde_chunks.elems; i++)
		  {
		    /* We assume that FDEs in .debug_frame and .eh_frame 
		       have the same order (if they are present, of course).
		       If we find a duplicate entry for one FDE and save
		       it's index to last_dup_fde it's very likely, that 
		       we'll find an entry for the following FDE right after 
		       the previous one. Thus in many cases we'll run this 
		       loop only once.  */
		    last_dup_fde = (last_dup_fde + i) % fde_chunks.elems;
		    if (fde_chunks.array[last_dup_fde]->initial_location
			== init_loc)
		      {
			dup = 1;
			break;
		      }
		  }

	      /* Allocate a new entry only if this FDE isn't a duplicate of
	         something we have already seen.   */
	      if (!dup)
		{
		  fde_chunks_need_space ();
		  fde = fde_unit_alloc ();

		  fde_chunks.array[fde_chunks.elems++] = fde;

		  fde->initial_location = init_loc;
		  fde->address_range = read_encoded_pointer (abfd, &start,
							     cie->
							     addr_encoding);

		  fde->cie_ptr = cie;

		  /* Here we intentionally ignore augmentation data
		     from FDE, because we don't need them.  */
		  if (cie->augmentation[0] == 'z')
		    start += read_uleb128 (abfd, &start);

		  fde->data = start;
		  fde->data_length = block_end - start;
		}
	    }
	  start = block_end;
	}
      qsort (fde_chunks.array, fde_chunks.elems,
	     sizeof (struct fde_unit *), compare_fde_unit);
    }
}

/* We must parse both .debug_frame section and .eh_frame because 
 * not all frames must be present in both of these sections. */
void
dwarf2_build_frame_info (struct objfile *objfile)
{
  int after_debug_frame = 0;

  /* If we have .debug_frame then the parser is called with 
     eh_frame==0 for .debug_frame and eh_frame==2 for .eh_frame, 
     otherwise it's only called once for .eh_frame with argument 
     eh_frame==1.  */

  if (dwarf_frame_offset)
    {
      parse_frame_info (objfile, dwarf_frame_offset,
			dwarf_frame_size, 0 /* = debug_frame */ );
      after_debug_frame = 1;
    }

  if (dwarf_eh_frame_offset)
    parse_frame_info (objfile, dwarf_eh_frame_offset, dwarf_eh_frame_size,
		      1 /* = eh_frame */  + after_debug_frame);
}

/* Return the frame address.  */
CORE_ADDR
cfi_read_fp ()
{
  struct context *context;
  struct frame_state *fs;
  CORE_ADDR cfa;

  unwind_tmp_obstack_init ();

  context = context_alloc ();
  fs = frame_state_alloc ();

  context->ra = read_pc () + 1;

  frame_state_for (context, fs);
  update_context (context, fs, 0);

  cfa = context->cfa;

  unwind_tmp_obstack_free ();

  return cfa;
}

/* Store the frame address.  This function is not used.  */

void
cfi_write_fp (CORE_ADDR val)
{
  struct context *context;
  struct frame_state *fs;

  unwind_tmp_obstack_init ();

  context = context_alloc ();
  fs = frame_state_alloc ();

  context->ra = read_pc () + 1;

  frame_state_for (context, fs);

  if (fs->cfa_how == CFA_REG_OFFSET)
    {
      val -= fs->cfa_offset;
      write_register_gen (fs->cfa_reg, (char *) &val);
    }
  else
    warning ("Can't write fp.");

  unwind_tmp_obstack_free ();
}

/* Restore the machine to the state it had before the current frame
   was created.  */
void
cfi_pop_frame (struct frame_info *fi)
{
  char regbuf[MAX_REGISTER_RAW_SIZE];
  int regnum;

  fi = get_current_frame ();

  for (regnum = 0; regnum < NUM_REGS; regnum++)
    {
      get_reg (regbuf, UNWIND_CONTEXT (fi), regnum);
      write_register_bytes (REGISTER_BYTE (regnum), regbuf,
			    REGISTER_RAW_SIZE (regnum));
    }
  write_register (PC_REGNUM, UNWIND_CONTEXT (fi)->ra);

  flush_cached_frames ();
}

/* Determine the address of the calling function's frame.  */
CORE_ADDR
cfi_frame_chain (struct frame_info *fi)
{
  struct context *context;
  struct frame_state *fs;
  CORE_ADDR cfa;

  unwind_tmp_obstack_init ();

  context = context_alloc ();
  fs = frame_state_alloc ();
  context_cpy (context, UNWIND_CONTEXT (fi));

  /* outermost frame */
  if (context->ra == 0)
    {
      unwind_tmp_obstack_free ();
      return 0;
    }

  frame_state_for (context, fs);
  update_context (context, fs, 1);

  cfa = context->cfa;
  unwind_tmp_obstack_free ();

  return cfa;
}

/* Sets the pc of the frame.  */
void
cfi_init_frame_pc (int fromleaf, struct frame_info *fi)
{
  if (fi->next)
    get_reg ((char *) &(fi->pc), UNWIND_CONTEXT (fi->next), PC_REGNUM);
  else
    fi->pc = read_pc ();
}

/* Initialize unwind context informations of the frame.  */
void
cfi_init_extra_frame_info (int fromleaf, struct frame_info *fi)
{
  struct frame_state *fs;

  unwind_tmp_obstack_init ();

  fs = frame_state_alloc ();
  fi->context = frame_obstack_alloc (sizeof (struct context));
  UNWIND_CONTEXT (fi)->reg =
    frame_obstack_alloc (sizeof (struct context_reg) * NUM_REGS);
  memset (UNWIND_CONTEXT (fi)->reg, 0,
	  sizeof (struct context_reg) * NUM_REGS);

  if (fi->next)
    {
      context_cpy (UNWIND_CONTEXT (fi), UNWIND_CONTEXT (fi->next));
      frame_state_for (UNWIND_CONTEXT (fi), fs);
      update_context (UNWIND_CONTEXT (fi), fs, 1);
    }
  else
    {
      UNWIND_CONTEXT (fi)->ra = fi->pc + 1;
      frame_state_for (UNWIND_CONTEXT (fi), fs);
      update_context (UNWIND_CONTEXT (fi), fs, 0);
    }

  unwind_tmp_obstack_free ();
}

/* Obtain return address of the frame.  */
CORE_ADDR
cfi_get_ra (struct frame_info *fi)
{
  return UNWIND_CONTEXT (fi)->ra;
}

/* Find register number REGNUM relative to FRAME and put its
   (raw) contents in *RAW_BUFFER.  Set *OPTIMIZED if the variable
   was optimized out (and thus can't be fetched).  If the variable
   was fetched from memory, set *ADDRP to where it was fetched from,
   otherwise it was fetched from a register.

   The argument RAW_BUFFER must point to aligned memory.  */
void
cfi_get_saved_register (char *raw_buffer,
			int *optimized,
			CORE_ADDR *addrp,
			struct frame_info *frame,
			int regnum, enum lval_type *lval)
{
  if (!target_has_registers)
    error ("No registers.");

  /* Normal systems don't optimize out things with register numbers.  */
  if (optimized != NULL)
    *optimized = 0;

  if (addrp)			/* default assumption: not found in memory */
    *addrp = 0;

  if (!frame->next)
    {
      read_register_gen (regnum, raw_buffer);
      if (lval != NULL)
	*lval = lval_register;
      if (addrp != NULL)
	*addrp = REGISTER_BYTE (regnum);
    }
  else
    {
      frame = frame->next;
      switch (UNWIND_CONTEXT (frame)->reg[regnum].how)
	{
	case REG_CTX_UNSAVED:
	  read_register_gen (regnum, raw_buffer);
	  if (lval != NULL)
	    *lval = not_lval;
	  if (optimized != NULL)
	    *optimized = 1;
	  break;
	case REG_CTX_SAVED_OFFSET:
	  target_read_memory (UNWIND_CONTEXT (frame)->cfa +
			      UNWIND_CONTEXT (frame)->reg[regnum].loc.offset,
			      raw_buffer, REGISTER_RAW_SIZE (regnum));
	  if (lval != NULL)
	    *lval = lval_memory;
	  if (addrp != NULL)
	    *addrp =
	      UNWIND_CONTEXT (frame)->cfa +
	      UNWIND_CONTEXT (frame)->reg[regnum].loc.offset;
	  break;
	case REG_CTX_SAVED_REG:
	  read_register_gen (UNWIND_CONTEXT (frame)->reg[regnum].loc.reg,
			     raw_buffer);
	  if (lval != NULL)
	    *lval = lval_register;
	  if (addrp != NULL)
	    *addrp =
	      REGISTER_BYTE (UNWIND_CONTEXT (frame)->reg[regnum].loc.reg);
	  break;
	case REG_CTX_SAVED_ADDR:
	  target_read_memory (UNWIND_CONTEXT (frame)->reg[regnum].loc.addr,
			      raw_buffer, REGISTER_RAW_SIZE (regnum));
	  if (lval != NULL)
	    *lval = lval_memory;
	  if (addrp != NULL)
	    *addrp = UNWIND_CONTEXT (frame)->reg[regnum].loc.addr;
	  break;
	case REG_CTX_VALUE:
	  memcpy (raw_buffer, &UNWIND_CONTEXT (frame)->reg[regnum].loc.addr,
		  REGISTER_RAW_SIZE (regnum));
	  if (lval != NULL)
	    *lval = not_lval;
	  if (optimized != NULL)
	    *optimized = 0;
	  break;
	default:
	  internal_error (__FILE__, __LINE__,
			  "cfi_get_saved_register: unknown register rule");
	}
    }
}

/*  Return the register that the function uses for a frame pointer,
    plus any necessary offset to be applied to the register before
    any frame pointer offsets.  */
void
cfi_virtual_frame_pointer (CORE_ADDR pc, int *frame_reg,
			   LONGEST * frame_offset)
{
  struct context *context;
  struct frame_state *fs;

  unwind_tmp_obstack_init ();

  context = context_alloc ();
  fs = frame_state_alloc ();

  context->ra = read_pc () + 1;

  frame_state_for (context, fs);

  if (fs->cfa_how == CFA_REG_OFFSET)
    {
      *frame_reg = fs->cfa_reg;
      *frame_offset = fs->cfa_offset;
    }
  else
    error ("dwarf cfi error: CFA is not defined as CFA_REG_OFFSET");

  unwind_tmp_obstack_free ();
}
