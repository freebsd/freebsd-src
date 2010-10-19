/* vms-tir.c -- BFD back-end for VAX (openVMS/VAX) and
   EVAX (openVMS/Alpha) files.
   Copyright 1996, 1997, 1998, 1999, 2000, 2001, 2002, 2004, 2005
   Free Software Foundation, Inc.

   TIR record handling functions
   ETIR record handling functions

   go and read the openVMS linker manual (esp. appendix B)
   if you don't know what's going on here :-)

   Written by Klaus K"ampf (kkaempf@rmi.de)

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
   Foundation, Inc., 51 Franklin Street - Fifth Floor, Boston, MA 02110-1301, USA.  */

/* The following type abbreviations are used:

	cs	counted string (ascii string with length byte)
	by	byte (1 byte)
	sh	short (2 byte, 16 bit)
	lw	longword (4 byte, 32 bit)
	qw	quadword (8 byte, 64 bit)
	da	data stream  */

#include "bfd.h"
#include "sysdep.h"
#include "bfdlink.h"
#include "libbfd.h"
#include "vms.h"

static int
check_section (bfd * abfd, int size)
{
  bfd_size_type offset;

  offset = PRIV (image_ptr) - PRIV (image_section)->contents;
  if (offset + size > PRIV (image_section)->size)
    {
      PRIV (image_section)->contents
	= bfd_realloc (PRIV (image_section)->contents, offset + size);
      if (PRIV (image_section)->contents == 0)
	{
	  (*_bfd_error_handler) (_("No Mem !"));
	  return -1;
	}
      PRIV (image_section)->size = offset + size;
      PRIV (image_ptr) = PRIV (image_section)->contents + offset;
    }

  return 0;
}

/* Routines to fill sections contents during tir/etir read.  */

/* Initialize image buffer pointer to be filled.  */

static void
image_set_ptr (bfd * abfd, int psect, uquad offset)
{
#if VMS_DEBUG
  _bfd_vms_debug (4, "image_set_ptr (%d=%s, %d)\n",
		  psect, PRIV (sections)[psect]->name, offset);
#endif

  PRIV (image_ptr) = PRIV (sections)[psect]->contents + offset;
  PRIV (image_section) = PRIV (sections)[psect];
}

/* Increment image buffer pointer by offset.  */

static void
image_inc_ptr (bfd * abfd, uquad offset)
{
#if VMS_DEBUG
  _bfd_vms_debug (4, "image_inc_ptr (%d)\n", offset);
#endif

  PRIV (image_ptr) += offset;
}

/* Dump multiple bytes to section image.  */

static void
image_dump (bfd * abfd,
	    unsigned char *ptr,
	    int size,
	    int offset ATTRIBUTE_UNUSED)
{
#if VMS_DEBUG
  _bfd_vms_debug (8, "image_dump from (%p, %d) to (%p)\n", ptr, size,
		  PRIV (image_ptr));
  _bfd_hexdump (9, ptr, size, offset);
#endif

  if (PRIV (is_vax) && check_section (abfd, size))
    return;

  while (size-- > 0)
    *PRIV (image_ptr)++ = *ptr++;
}

/* Write byte to section image.  */

static void
image_write_b (bfd * abfd, unsigned int value)
{
#if VMS_DEBUG
  _bfd_vms_debug (6, "image_write_b (%02x)\n", (int) value);
#endif

  if (PRIV (is_vax) && check_section (abfd, 1))
    return;

  *PRIV (image_ptr)++ = (value & 0xff);
}

/* Write 2-byte word to image.  */

static void
image_write_w (bfd * abfd, unsigned int value)
{
#if VMS_DEBUG
  _bfd_vms_debug (6, "image_write_w (%04x)\n", (int) value);
#endif

  if (PRIV (is_vax) && check_section (abfd, 2))
    return;

  bfd_putl16 ((bfd_vma) value, PRIV (image_ptr));
  PRIV (image_ptr) += 2;
}

/* Write 4-byte long to image.  */

static void
image_write_l (bfd * abfd, unsigned long value)
{
#if VMS_DEBUG
  _bfd_vms_debug (6, "image_write_l (%08lx)\n", value);
#endif

  if (PRIV (is_vax) && check_section (abfd, 4))
    return;

  bfd_putl32 ((bfd_vma) value, PRIV (image_ptr));
  PRIV (image_ptr) += 4;
}

/* Write 8-byte quad to image.  */

static void
image_write_q (bfd * abfd, uquad value)
{
#if VMS_DEBUG
  _bfd_vms_debug (6, "image_write_q (%016lx)\n", value);
#endif

  if (PRIV (is_vax) && check_section (abfd, 8))
    return;

  bfd_putl64 (value, PRIV (image_ptr));
  PRIV (image_ptr) += 8;
}

static const char *
cmd_name (int cmd)
{
  switch (cmd)
    {
    case ETIR_S_C_STA_GBL: return "ETIR_S_C_STA_GBL";
    case ETIR_S_C_STA_PQ: return "ETIR_S_C_STA_PQ";
    case ETIR_S_C_STA_LI: return "ETIR_S_C_STA_LI";
    case ETIR_S_C_STA_MOD: return "ETIR_S_C_STA_MOD";
    case ETIR_S_C_STA_CKARG: return "ETIR_S_C_STA_CKARG";
    case ETIR_S_C_STO_B: return "ETIR_S_C_STO_B";
    case ETIR_S_C_STO_W: return "ETIR_S_C_STO_W";
    case ETIR_S_C_STO_GBL: return "ETIR_S_C_STO_GBL";
    case ETIR_S_C_STO_CA: return "ETIR_S_C_STO_CA";
    case ETIR_S_C_STO_RB: return "ETIR_S_C_STO_RB";
    case ETIR_S_C_STO_AB: return "ETIR_S_C_STO_AB";
    case ETIR_S_C_STO_GBL_LW: return "ETIR_S_C_STO_GBL_LW";
    case ETIR_S_C_STO_LP_PSB: return "ETIR_S_C_STO_LP_PSB";
    case ETIR_S_C_STO_HINT_GBL: return "ETIR_S_C_STO_HINT_GBL";
    case ETIR_S_C_STO_HINT_PS: return "ETIR_S_C_STO_HINT_PS";
    case ETIR_S_C_OPR_INSV: return "ETIR_S_C_OPR_INSV";
    case ETIR_S_C_OPR_USH: return "ETIR_S_C_OPR_USH";
    case ETIR_S_C_OPR_ROT: return "ETIR_S_C_OPR_ROT";
    case ETIR_S_C_OPR_REDEF: return "ETIR_S_C_OPR_REDEF";
    case ETIR_S_C_OPR_DFLIT: return "ETIR_S_C_OPR_DFLIT";
    case ETIR_S_C_STC_LP: return "ETIR_S_C_STC_LP";
    case ETIR_S_C_STC_GBL: return "ETIR_S_C_STC_GBL";
    case ETIR_S_C_STC_GCA: return "ETIR_S_C_STC_GCA";
    case ETIR_S_C_STC_PS: return "ETIR_S_C_STC_PS";
    case ETIR_S_C_STC_NBH_PS: return "ETIR_S_C_STC_NBH_PS";
    case ETIR_S_C_STC_NOP_GBL: return "ETIR_S_C_STC_NOP_GBL";
    case ETIR_S_C_STC_NOP_PS: return "ETIR_S_C_STC_NOP_PS";
    case ETIR_S_C_STC_BSR_GBL: return "ETIR_S_C_STC_BSR_GBL";
    case ETIR_S_C_STC_BSR_PS: return "ETIR_S_C_STC_BSR_PS";
    case ETIR_S_C_STC_LDA_GBL: return "ETIR_S_C_STC_LDA_GBL";
    case ETIR_S_C_STC_LDA_PS: return "ETIR_S_C_STC_LDA_PS";
    case ETIR_S_C_STC_BOH_GBL: return "ETIR_S_C_STC_BOH_GBL";
    case ETIR_S_C_STC_BOH_PS: return "ETIR_S_C_STC_BOH_PS";
    case ETIR_S_C_STC_NBH_GBL: return "ETIR_S_C_STC_NBH_GBL";

    default:
      /* These names have not yet been added to this switch statement.  */
      abort ();
    }
}
#define HIGHBIT(op) ((op & 0x80000000L) == 0x80000000L)

/* etir_sta

   vms stack commands

   handle sta_xxx commands in etir section
   ptr points to data area in record

   see table B-8 of the openVMS linker manual.  */

static bfd_boolean
etir_sta (bfd * abfd, int cmd, unsigned char *ptr)
{
#if VMS_DEBUG
  _bfd_vms_debug (5, "etir_sta %d/%x\n", cmd, cmd);
  _bfd_hexdump (8, ptr, 16, (int) ptr);
#endif

  switch (cmd)
    {
      /* stack global
	 arg: cs	symbol name

	 stack 32 bit value of symbol (high bits set to 0).  */
    case ETIR_S_C_STA_GBL:
      {
	char *name;
	vms_symbol_entry *entry;

	name = _bfd_vms_save_counted_string (ptr);
	entry = (vms_symbol_entry *)
	  bfd_hash_lookup (PRIV (vms_symbol_table), name, FALSE, FALSE);
	if (entry == NULL)
	  {
#if VMS_DEBUG
	    _bfd_vms_debug (3, "%s: no symbol \"%s\"\n",
			    cmd_name (cmd), name);
#endif
	    _bfd_vms_push (abfd, (uquad) 0, -1);
	  }
	else
	  _bfd_vms_push (abfd, (uquad) (entry->symbol->value), -1);
      }
      break;

      /* stack longword
	 arg: lw	value

	 stack 32 bit value, sign extend to 64 bit.  */
    case ETIR_S_C_STA_LW:
      _bfd_vms_push (abfd, (uquad) bfd_getl32 (ptr), -1);
      break;

      /* stack global
	 arg: qw	value

	 stack 64 bit value of symbol.  */
    case ETIR_S_C_STA_QW:
      _bfd_vms_push (abfd, (uquad) bfd_getl64 (ptr), -1);
      break;

      /* stack psect base plus quadword offset
	 arg: lw	section index
	 qw	signed quadword offset (low 32 bits)

	 stack qw argument and section index
	 (see ETIR_S_C_STO_OFF, ETIR_S_C_CTL_SETRB).  */
    case ETIR_S_C_STA_PQ:
      {
	uquad dummy;
	unsigned int psect;

	psect = bfd_getl32 (ptr);
	if (psect >= PRIV (section_count))
	  {
	    (*_bfd_error_handler) (_("bad section index in %s"),
				   cmd_name (cmd));
	    bfd_set_error (bfd_error_bad_value);
	    return FALSE;
	  }
	dummy = bfd_getl64 (ptr + 4);
	_bfd_vms_push (abfd, dummy, (int) psect);
      }
      break;

    case ETIR_S_C_STA_LI:
    case ETIR_S_C_STA_MOD:
    case ETIR_S_C_STA_CKARG:
      (*_bfd_error_handler) (_("unsupported STA cmd %s"), cmd_name (cmd));
      return FALSE;
      break;

    default:
      (*_bfd_error_handler) (_("reserved STA cmd %d"), cmd);
      return FALSE;
      break;
    }
#if VMS_DEBUG
  _bfd_vms_debug (5, "etir_sta true\n");
#endif
  return TRUE;
}

/* etir_sto

   vms store commands

   handle sto_xxx commands in etir section
   ptr points to data area in record

   see table B-9 of the openVMS linker manual.  */

static bfd_boolean
etir_sto (bfd * abfd, int cmd, unsigned char *ptr)
{
  uquad dummy;
  int psect;

#if VMS_DEBUG
  _bfd_vms_debug (5, "etir_sto %d/%x\n", cmd, cmd);
  _bfd_hexdump (8, ptr, 16, (int) ptr);
#endif

  switch (cmd)
    {
      /* Store byte: pop stack, write byte
	 arg: -.  */
    case ETIR_S_C_STO_B:
      dummy = _bfd_vms_pop (abfd, &psect);
      /* FIXME: check top bits.  */
      image_write_b (abfd, (unsigned int) dummy & 0xff);
      break;

      /* Store word: pop stack, write word
	 arg: -.  */
    case ETIR_S_C_STO_W:
      dummy = _bfd_vms_pop (abfd, &psect);
      /* FIXME: check top bits */
      image_write_w (abfd, (unsigned int) dummy & 0xffff);
      break;

      /* Store longword: pop stack, write longword
	 arg: -.  */
    case ETIR_S_C_STO_LW:
      dummy = _bfd_vms_pop (abfd, &psect);
      dummy += (PRIV (sections)[psect])->vma;
      /* FIXME: check top bits.  */
      image_write_l (abfd, (unsigned int) dummy & 0xffffffff);
      break;

      /* Store quadword: pop stack, write quadword
	 arg: -.  */
    case ETIR_S_C_STO_QW:
      dummy = _bfd_vms_pop (abfd, &psect);
      dummy += (PRIV (sections)[psect])->vma;
      /* FIXME: check top bits.  */
      image_write_q (abfd, dummy);
      break;

      /* Store immediate repeated: pop stack for repeat count
	 arg: lw	byte count
	 da	data.  */
    case ETIR_S_C_STO_IMMR:
      {
	int size;

	size = bfd_getl32 (ptr);
	dummy = (unsigned long) _bfd_vms_pop (abfd, NULL);
	while (dummy-- > 0)
	  image_dump (abfd, ptr+4, size, 0);
      }
      break;

      /* Store global: write symbol value
	 arg: cs	global symbol name.  */
    case ETIR_S_C_STO_GBL:
      {
	vms_symbol_entry *entry;
	char *name;

	name = _bfd_vms_save_counted_string (ptr);
	entry = (vms_symbol_entry *) bfd_hash_lookup (PRIV (vms_symbol_table),
						      name, FALSE, FALSE);
	if (entry == NULL)
	  {
	    (*_bfd_error_handler) (_("%s: no symbol \"%s\""),
				   cmd_name (cmd), name);
	    return FALSE;
	  }
	else
	  /* FIXME, reloc.  */
	  image_write_q (abfd, (uquad) (entry->symbol->value));
      }
      break;

      /* Store code address: write address of entry point
	 arg: cs	global symbol name (procedure).  */
    case ETIR_S_C_STO_CA:
      {
	vms_symbol_entry *entry;
	char *name;

	name = _bfd_vms_save_counted_string (ptr);
	entry = (vms_symbol_entry *) bfd_hash_lookup (PRIV (vms_symbol_table),
						      name, FALSE, FALSE);
	if (entry == NULL)
	  {
	    (*_bfd_error_handler) (_("%s: no symbol \"%s\""),
				   cmd_name (cmd), name);
	    return FALSE;
	  }
	else
	  /* FIXME, reloc.  */
	  image_write_q (abfd, (uquad) (entry->symbol->value));
      }
      break;

      /* Store offset to psect: pop stack, add low 32 bits to base of psect
	 arg: none.  */
    case ETIR_S_C_STO_OFF:
      {
	uquad q;
	int psect1;

	q = _bfd_vms_pop (abfd, & psect1);
	q += (PRIV (sections)[psect1])->vma;
	image_write_q (abfd, q);
      }
      break;

      /* Store immediate
	 arg: lw	count of bytes
	      da	data.  */
    case ETIR_S_C_STO_IMM:
      {
	int size;

	size = bfd_getl32 (ptr);
	image_dump (abfd, ptr+4, size, 0);
      }
      break;

      /* This code is 'reserved to digital' according to the openVMS
	 linker manual, however it is generated by the DEC C compiler
	 and defined in the include file.
	 FIXME, since the following is just a guess
	 store global longword: store 32bit value of symbol
	 arg: cs	symbol name.  */
    case ETIR_S_C_STO_GBL_LW:
      {
	vms_symbol_entry *entry;
	char *name;

	name = _bfd_vms_save_counted_string (ptr);
	entry = (vms_symbol_entry *) bfd_hash_lookup (PRIV (vms_symbol_table),
						      name, FALSE, FALSE);
	if (entry == NULL)
	  {
#if VMS_DEBUG
	    _bfd_vms_debug (3, "%s: no symbol \"%s\"\n", cmd_name (cmd), name);
#endif
	    image_write_l (abfd, (unsigned long) 0);	/* FIXME, reloc */
	  }
	else
	  /* FIXME, reloc.  */
	  image_write_l (abfd, (unsigned long) (entry->symbol->value));
      }
      break;

    case ETIR_S_C_STO_RB:
    case ETIR_S_C_STO_AB:
    case ETIR_S_C_STO_LP_PSB:
      (*_bfd_error_handler) (_("%s: not supported"), cmd_name (cmd));
      break;

    case ETIR_S_C_STO_HINT_GBL:
    case ETIR_S_C_STO_HINT_PS:
      (*_bfd_error_handler) (_("%s: not implemented"), cmd_name (cmd));
      break;

    default:
      (*_bfd_error_handler) (_("reserved STO cmd %d"), cmd);
      break;
    }

  return TRUE;
}

/* Stack operator commands
   all 32 bit signed arithmetic
   all word just like a stack calculator
   arguments are popped from stack, results are pushed on stack

   see table B-10 of the openVMS linker manual.  */

static bfd_boolean
etir_opr (bfd * abfd, int cmd, unsigned char *ptr ATTRIBUTE_UNUSED)
{
  long op1, op2;

#if VMS_DEBUG
  _bfd_vms_debug (5, "etir_opr %d/%x\n", cmd, cmd);
  _bfd_hexdump (8, ptr, 16, (int) ptr);
#endif

  switch (cmd)
    {
    case ETIR_S_C_OPR_NOP:      /* No-op.  */
      break;

    case ETIR_S_C_OPR_ADD:      /* Add.  */
      op1 = (long) _bfd_vms_pop (abfd, NULL);
      op2 = (long) _bfd_vms_pop (abfd, NULL);
      _bfd_vms_push (abfd, (uquad) (op1 + op2), -1);
      break;

    case ETIR_S_C_OPR_SUB:      /* Subtract.  */
      op1 = (long) _bfd_vms_pop (abfd, NULL);
      op2 = (long) _bfd_vms_pop (abfd, NULL);
      _bfd_vms_push (abfd, (uquad) (op2 - op1), -1);
      break;

    case ETIR_S_C_OPR_MUL:      /* Multiply.  */
      op1 = (long) _bfd_vms_pop (abfd, NULL);
      op2 = (long) _bfd_vms_pop (abfd, NULL);
      _bfd_vms_push (abfd, (uquad) (op1 * op2), -1);
      break;

    case ETIR_S_C_OPR_DIV:      /* Divide.  */
      op1 = (long) _bfd_vms_pop (abfd, NULL);
      op2 = (long) _bfd_vms_pop (abfd, NULL);
      if (op2 == 0)
	_bfd_vms_push (abfd, (uquad) 0, -1);
      else
	_bfd_vms_push (abfd, (uquad) (op2 / op1), -1);
      break;

    case ETIR_S_C_OPR_AND:      /* Logical AND.  */
      op1 = (long) _bfd_vms_pop (abfd, NULL);
      op2 = (long) _bfd_vms_pop (abfd, NULL);
      _bfd_vms_push (abfd, (uquad) (op1 & op2), -1);
      break;

    case ETIR_S_C_OPR_IOR:      /* Logical inclusive OR.  */
      op1 = (long) _bfd_vms_pop (abfd, NULL);
      op2 = (long) _bfd_vms_pop (abfd, NULL);
      _bfd_vms_push (abfd, (uquad) (op1 | op2), -1);
      break;

    case ETIR_S_C_OPR_EOR:      /* Logical exclusive OR.  */
      op1 = (long) _bfd_vms_pop (abfd, NULL);
      op2 = (long) _bfd_vms_pop (abfd, NULL);
      _bfd_vms_push (abfd, (uquad) (op1 ^ op2), -1);
      break;

    case ETIR_S_C_OPR_NEG:      /* Negate.  */
      op1 = (long) _bfd_vms_pop (abfd, NULL);
      _bfd_vms_push (abfd, (uquad) (-op1), -1);
      break;

    case ETIR_S_C_OPR_COM:      /* Complement.  */
      op1 = (long) _bfd_vms_pop (abfd, NULL);
      _bfd_vms_push (abfd, (uquad) (op1 ^ -1L), -1);
      break;

    case ETIR_S_C_OPR_ASH:      /* Arithmetic shift.  */
      op1 = (long) _bfd_vms_pop (abfd, NULL);
      op2 = (long) _bfd_vms_pop (abfd, NULL);
      if (op2 < 0)		/* Shift right.  */
	op1 >>= -op2;
      else			/* Shift left.  */
	op1 <<= op2;
      _bfd_vms_push (abfd, (uquad) op1, -1);
      break;

    case ETIR_S_C_OPR_INSV:      /* Insert field.   */
      (void) _bfd_vms_pop (abfd, NULL);
    case ETIR_S_C_OPR_USH:       /* Unsigned shift.   */
    case ETIR_S_C_OPR_ROT:       /* Rotate.  */
    case ETIR_S_C_OPR_REDEF:     /* Redefine symbol to current location.  */
    case ETIR_S_C_OPR_DFLIT:     /* Define a literal.  */
      (*_bfd_error_handler) (_("%s: not supported"), cmd_name (cmd));
      break;

    case ETIR_S_C_OPR_SEL:      /* Select.  */
      if ((long) _bfd_vms_pop (abfd, NULL) & 0x01L)
	(void) _bfd_vms_pop (abfd, NULL);
      else
	{
	  op1 = (long) _bfd_vms_pop (abfd, NULL);
	  (void) _bfd_vms_pop (abfd, NULL);
	  _bfd_vms_push (abfd, (uquad) op1, -1);
	}
      break;

    default:
      (*_bfd_error_handler) (_("reserved OPR cmd %d"), cmd);
      break;
    }

  return TRUE;
}

/* Control commands.

   See table B-11 of the openVMS linker manual.  */

static bfd_boolean
etir_ctl (bfd * abfd, int cmd, unsigned char *ptr)
{
  uquad	 dummy;
  int psect;

#if VMS_DEBUG
  _bfd_vms_debug (5, "etir_ctl %d/%x\n", cmd, cmd);
  _bfd_hexdump (8, ptr, 16, (int) ptr);
#endif

  switch (cmd)
    {
      /* Det relocation base: pop stack, set image location counter
	 arg: none.  */
    case ETIR_S_C_CTL_SETRB:
      dummy = _bfd_vms_pop (abfd, &psect);
      image_set_ptr (abfd, psect, dummy);
      break;

      /* Augment relocation base: increment image location counter by offset
	 arg: lw	offset value.  */
    case ETIR_S_C_CTL_AUGRB:
      dummy = bfd_getl32 (ptr);
      image_inc_ptr (abfd, dummy);
      break;

      /* Define location: pop index, save location counter under index
	 arg: none.  */
    case ETIR_S_C_CTL_DFLOC:
      dummy = _bfd_vms_pop (abfd, NULL);
      /* FIXME */
      break;

      /* Set location: pop index, restore location counter from index
	 arg: none.  */
    case ETIR_S_C_CTL_STLOC:
      dummy = _bfd_vms_pop (abfd, &psect);
      /* FIXME */
      break;

      /* Stack defined location: pop index, push location counter from index
	 arg: none.  */
    case ETIR_S_C_CTL_STKDL:
      dummy = _bfd_vms_pop (abfd, &psect);
      /* FIXME.  */
      break;

    default:
      (*_bfd_error_handler) (_("reserved CTL cmd %d"), cmd);
      break;
    }
  return TRUE;
}

/* Store conditional commands

   See table B-12 and B-13 of the openVMS linker manual.  */

static bfd_boolean
etir_stc (bfd * abfd, int cmd, unsigned char *ptr ATTRIBUTE_UNUSED)
{
#if VMS_DEBUG
  _bfd_vms_debug (5, "etir_stc %d/%x\n", cmd, cmd);
  _bfd_hexdump (8, ptr, 16, (int) ptr);
#endif

  switch (cmd)
    {
      /* 200 Store-conditional Linkage Pair
	 arg: none.  */
    case ETIR_S_C_STC_LP:
      (*_bfd_error_handler) (_("%s: not supported"), cmd_name (cmd));
      break;

      /* 201 Store-conditional Linkage Pair with Procedure Signature
	 arg:	lw	linkage index
		cs	procedure name
		by	signature length
		da	signature.  */
    case ETIR_S_C_STC_LP_PSB:
      image_inc_ptr (abfd, (uquad) 16);	/* skip entry,procval */
      break;

      /* 202 Store-conditional Address at global address
	 arg:	lw	linkage index
		cs	global name.  */

    case ETIR_S_C_STC_GBL:
      (*_bfd_error_handler) (_("%s: not supported"), cmd_name (cmd));
      break;

      /* 203 Store-conditional Code Address at global address
	 arg:	lw	linkage index
		cs	procedure name.  */
    case ETIR_S_C_STC_GCA:
      (*_bfd_error_handler) (_("%s: not supported"), cmd_name (cmd));
      break;

      /* 204 Store-conditional Address at psect + offset
	 arg:	lw	linkage index
		lw	psect index
		qw	offset.  */
    case ETIR_S_C_STC_PS:
      (*_bfd_error_handler) (_("%s: not supported"), cmd_name (cmd));
      break;

      /* 205 Store-conditional NOP at address of global
	 arg: none.  */
    case ETIR_S_C_STC_NOP_GBL:

      /* 206 Store-conditional NOP at pect + offset
	 arg: none.  */
    case ETIR_S_C_STC_NOP_PS:

      /* 207 Store-conditional BSR at global address
	 arg: none.  */
    case ETIR_S_C_STC_BSR_GBL:

      /* 208 Store-conditional BSR at pect + offset
	 arg: none.  */
    case ETIR_S_C_STC_BSR_PS:

      /* 209 Store-conditional LDA at global address
	 arg: none.  */
    case ETIR_S_C_STC_LDA_GBL:

      /* 210 Store-conditional LDA at psect + offset
	 arg: none.  */
    case ETIR_S_C_STC_LDA_PS:

      /* 211 Store-conditional BSR or Hint at global address
	 arg: none.  */
    case ETIR_S_C_STC_BOH_GBL:

      /* 212 Store-conditional BSR or Hint at pect + offset
	 arg: none.  */
    case ETIR_S_C_STC_BOH_PS:

      /* 213 Store-conditional NOP,BSR or HINT at global address
	 arg: none.  */
    case ETIR_S_C_STC_NBH_GBL:

      /* 214 Store-conditional NOP,BSR or HINT at psect + offset
	 arg: none.  */
    case ETIR_S_C_STC_NBH_PS:
      /* FIXME */
      break;

    default:
#if VMS_DEBUG
      _bfd_vms_debug (3,  "reserved STC cmd %d", cmd);
#endif
      break;
    }
  return TRUE;
}

static asection *
new_section (bfd * abfd ATTRIBUTE_UNUSED, int idx)
{
  asection *section;
  char sname[16];
  char *name;

#if VMS_DEBUG
  _bfd_vms_debug (5, "new_section %d\n", idx);
#endif
  sprintf (sname, SECTION_NAME_TEMPLATE, idx);

  name = bfd_malloc ((bfd_size_type) strlen (sname) + 1);
  if (name == 0)
    return NULL;
  strcpy (name, sname);

  section = bfd_malloc ((bfd_size_type) sizeof (asection));
  if (section == 0)
    {
#if VMS_DEBUG
      _bfd_vms_debug (6,  "bfd_make_section (%s) failed", name);
#endif
      return NULL;
    }

  section->size = 0;
  section->vma = 0;
  section->contents = 0;
  section->name = name;
  section->index = idx;

  return section;
}

static int
alloc_section (bfd * abfd, unsigned int idx)
{
  bfd_size_type amt;

#if VMS_DEBUG
  _bfd_vms_debug (4, "alloc_section %d\n", idx);
#endif

  amt = idx + 1;
  amt *= sizeof (asection *);
  PRIV (sections) = bfd_realloc (PRIV (sections), amt);
  if (PRIV (sections) == 0)
    return -1;

  while (PRIV (section_count) <= idx)
    {
      PRIV (sections)[PRIV (section_count)]
	= new_section (abfd, (int) PRIV (section_count));
      if (PRIV (sections)[PRIV (section_count)] == 0)
	return -1;
      PRIV (section_count)++;
    }

  return 0;
}

/* tir_sta

   vax stack commands

   Handle sta_xxx commands in tir section
   ptr points to data area in record

   See table 7-3 of the VAX/VMS linker manual.  */

static unsigned char *
tir_sta (bfd * abfd, unsigned char *ptr)
{
  int cmd = *ptr++;

#if VMS_DEBUG
  _bfd_vms_debug (5, "tir_sta %d\n", cmd);
#endif

  switch (cmd)
    {
      /* stack */
    case TIR_S_C_STA_GBL:
      /* stack global
	 arg: cs	symbol name

	 stack 32 bit value of symbol (high bits set to 0).  */
      {
	char *name;
	vms_symbol_entry *entry;

	name = _bfd_vms_save_counted_string (ptr);

	entry = _bfd_vms_enter_symbol (abfd, name);
	if (entry == NULL)
	  return NULL;

	_bfd_vms_push (abfd, (uquad) (entry->symbol->value), -1);
	ptr += *ptr + 1;
      }
      break;

    case TIR_S_C_STA_SB:
      /* stack signed byte
	 arg: by	value

	 stack byte value, sign extend to 32 bit.  */
      _bfd_vms_push (abfd, (uquad) *ptr++, -1);
      break;

    case TIR_S_C_STA_SW:
      /* stack signed short word
	 arg: sh	value

	 stack 16 bit value, sign extend to 32 bit.  */
      _bfd_vms_push (abfd, (uquad) bfd_getl16 (ptr), -1);
      ptr += 2;
      break;

    case TIR_S_C_STA_LW:
      /* stack signed longword
	 arg: lw	value

	 stack 32 bit value.  */
      _bfd_vms_push (abfd, (uquad) bfd_getl32 (ptr), -1);
      ptr += 4;
      break;

    case TIR_S_C_STA_PB:
    case TIR_S_C_STA_WPB:
      /* stack psect base plus byte offset (word index)
	 arg: by	section index
		(sh	section index)
		by	signed byte offset.  */
      {
	unsigned long dummy;
	unsigned int psect;

	if (cmd == TIR_S_C_STA_PB)
	  psect = *ptr++;
	else
	  {
	    psect = bfd_getl16 (ptr);
	    ptr += 2;
	  }

	if (psect >= PRIV (section_count))
	  alloc_section (abfd, psect);

	dummy = (long) *ptr++;
	dummy += (PRIV (sections)[psect])->vma;
	_bfd_vms_push (abfd, (uquad) dummy, (int) psect);
      }
      break;

    case TIR_S_C_STA_PW:
    case TIR_S_C_STA_WPW:
      /* stack psect base plus word offset (word index)
	 arg: by	section index
		(sh	section index)
		sh	signed short offset.  */
      {
	unsigned long dummy;
	unsigned int psect;

	if (cmd == TIR_S_C_STA_PW)
	  psect = *ptr++;
	else
	  {
	    psect = bfd_getl16 (ptr);
	    ptr += 2;
	  }

	if (psect >= PRIV (section_count))
	  alloc_section (abfd, psect);

	dummy = bfd_getl16 (ptr); ptr+=2;
	dummy += (PRIV (sections)[psect])->vma;
	_bfd_vms_push (abfd, (uquad) dummy, (int) psect);
      }
      break;

    case TIR_S_C_STA_PL:
    case TIR_S_C_STA_WPL:
      /* stack psect base plus long offset (word index)
	 arg: by	section index
		(sh	section index)
		lw	signed longword offset.	 */
      {
	unsigned long dummy;
	unsigned int psect;

	if (cmd == TIR_S_C_STA_PL)
	  psect = *ptr++;
	else
	  {
	    psect = bfd_getl16 (ptr);
	    ptr += 2;
	  }

	if (psect >= PRIV (section_count))
	  alloc_section (abfd, psect);

	dummy = bfd_getl32 (ptr); ptr += 4;
	dummy += (PRIV (sections)[psect])->vma;
	_bfd_vms_push (abfd, (uquad) dummy, (int) psect);
      }
      break;

    case TIR_S_C_STA_UB:
      /* stack unsigned byte
	 arg: by	value

	 stack byte value.  */
      _bfd_vms_push (abfd, (uquad) *ptr++, -1);
      break;

    case TIR_S_C_STA_UW:
      /* stack unsigned short word
	 arg: sh	value

	 stack 16 bit value.  */
      _bfd_vms_push (abfd, (uquad) bfd_getl16 (ptr), -1);
      ptr += 2;
      break;

    case TIR_S_C_STA_BFI:
      /* stack byte from image
	 arg: none.  */
      /* FALLTHRU  */
    case TIR_S_C_STA_WFI:
      /* stack byte from image
	 arg: none.  */
      /* FALLTHRU */
    case TIR_S_C_STA_LFI:
      /* stack byte from image
	 arg: none.  */
      (*_bfd_error_handler) (_("stack-from-image not implemented"));
      return NULL;

    case TIR_S_C_STA_EPM:
      /* stack entry point mask
	 arg: cs	symbol name

	 stack (unsigned) entry point mask of symbol
	 err if symbol is no entry point.  */
      {
	char *name;
	vms_symbol_entry *entry;

	name = _bfd_vms_save_counted_string (ptr);
	entry = _bfd_vms_enter_symbol (abfd, name);
	if (entry == NULL)
	  return NULL;

	(*_bfd_error_handler) (_("stack-entry-mask not fully implemented"));
	_bfd_vms_push (abfd, (uquad) 0, -1);
	ptr += *ptr + 1;
      }
      break;

    case TIR_S_C_STA_CKARG:
      /* compare procedure argument
	 arg: cs	symbol name
		by	argument index
		da	argument descriptor

	 compare argument descriptor with symbol argument (ARG$V_PASSMECH)
	 and stack TRUE (args match) or FALSE (args dont match) value.  */
      (*_bfd_error_handler) (_("PASSMECH not fully implemented"));
      _bfd_vms_push (abfd, (uquad) 1, -1);
      break;

    case TIR_S_C_STA_LSY:
      /* stack local symbol value
	 arg:	sh	environment index
		cs	symbol name.  */
      {
	int envidx;
	char *name;
	vms_symbol_entry *entry;

	envidx = bfd_getl16 (ptr);
	ptr += 2;
	name = _bfd_vms_save_counted_string (ptr);
	entry = _bfd_vms_enter_symbol (abfd, name);
	if (entry == NULL)
	  return NULL;
	(*_bfd_error_handler) (_("stack-local-symbol not fully implemented"));
	_bfd_vms_push (abfd, (uquad) 0, -1);
	ptr += *ptr + 1;
      }
      break;

    case TIR_S_C_STA_LIT:
      /* stack literal
	 arg:	by	literal index

	 stack literal.  */
      ptr++;
      _bfd_vms_push (abfd, (uquad) 0, -1);
      (*_bfd_error_handler) (_("stack-literal not fully implemented"));
      break;

    case TIR_S_C_STA_LEPM:
      /* stack local symbol entry point mask
	 arg:	sh	environment index
		cs	symbol name

	 stack (unsigned) entry point mask of symbol
	 err if symbol is no entry point.  */
      {
	int envidx;
	char *name;
	vms_symbol_entry *entry;

	envidx = bfd_getl16 (ptr);
	ptr += 2;
	name = _bfd_vms_save_counted_string (ptr);
	entry = _bfd_vms_enter_symbol (abfd, name);
	if (entry == NULL)
	  return NULL;
	(*_bfd_error_handler) (_("stack-local-symbol-entry-point-mask not fully implemented"));
	_bfd_vms_push (abfd, (uquad) 0, -1);
	ptr += *ptr + 1;
      }
      break;

    default:
      (*_bfd_error_handler) (_("reserved STA cmd %d"), ptr[-1]);
      return NULL;
      break;
    }

  return ptr;
}

static const char *
tir_cmd_name (int cmd)
{
  switch (cmd)
    {
    case TIR_S_C_STO_RSB: return "TIR_S_C_STO_RSB";
    case TIR_S_C_STO_RSW: return "TIR_S_C_STO_RSW";
    case TIR_S_C_STO_RL: return "TIR_S_C_STO_RL";
    case TIR_S_C_STO_VPS: return "TIR_S_C_STO_VPS";
    case TIR_S_C_STO_USB: return "TIR_S_C_STO_USB";
    case TIR_S_C_STO_USW: return "TIR_S_C_STO_USW";
    case TIR_S_C_STO_RUB: return "TIR_S_C_STO_RUB";
    case TIR_S_C_STO_RUW: return "TIR_S_C_STO_RUW";
    case TIR_S_C_STO_PIRR: return "TIR_S_C_STO_PIRR";
    case TIR_S_C_OPR_INSV: return "TIR_S_C_OPR_INSV";
    case TIR_S_C_OPR_DFLIT: return "TIR_S_C_OPR_DFLIT";
    case TIR_S_C_OPR_REDEF: return "TIR_S_C_OPR_REDEF";
    case TIR_S_C_OPR_ROT: return "TIR_S_C_OPR_ROT";
    case TIR_S_C_OPR_USH: return "TIR_S_C_OPR_USH";
    case TIR_S_C_OPR_ASH: return "TIR_S_C_OPR_ASH";
    case TIR_S_C_CTL_DFLOC: return "TIR_S_C_CTL_DFLOC";
    case TIR_S_C_CTL_STLOC: return "TIR_S_C_CTL_STLOC";
    case TIR_S_C_CTL_STKDL: return "TIR_S_C_CTL_STKDL";

    default:
      /* These strings have not been added yet.  */
      abort ();
    }
}

/* tir_sto

   vax store commands

   handle sto_xxx commands in tir section
   ptr points to data area in record

   See table 7-4 of the VAX/VMS linker manual.  */

static unsigned char *
tir_sto (bfd * abfd, unsigned char *ptr)
{
  unsigned long dummy;
  int size;
  int psect;

#if VMS_DEBUG
  _bfd_vms_debug (5, "tir_sto %d\n", *ptr);
#endif

  switch (*ptr++)
    {
    case TIR_S_C_STO_SB:
      /* Store signed byte: pop stack, write byte
	 arg: none.  */
      dummy = _bfd_vms_pop (abfd, &psect);
      image_write_b (abfd, dummy & 0xff);	/* FIXME: check top bits */
      break;

    case TIR_S_C_STO_SW:
      /* Store signed word: pop stack, write word
	 arg: none.  */
      dummy = _bfd_vms_pop (abfd, &psect);
      image_write_w (abfd, dummy & 0xffff);	/* FIXME: check top bits */
      break;

    case TIR_S_C_STO_LW:
      /* Store longword: pop stack, write longword
	 arg: none.  */
      dummy = _bfd_vms_pop (abfd, &psect);
      image_write_l (abfd, dummy & 0xffffffff);	/* FIXME: check top bits */
      break;

    case TIR_S_C_STO_BD:
      /* Store byte displaced: pop stack, sub lc+1, write byte
	 arg: none.  */
      dummy = _bfd_vms_pop (abfd, &psect);
      dummy -= ((PRIV (sections)[psect])->vma + 1);
      image_write_b (abfd, dummy & 0xff);/* FIXME: check top bits */
      break;

    case TIR_S_C_STO_WD:
      /* Store word displaced: pop stack, sub lc+2, write word
	 arg: none.  */
      dummy = _bfd_vms_pop (abfd, &psect);
      dummy -= ((PRIV (sections)[psect])->vma + 2);
      image_write_w (abfd, dummy & 0xffff);/* FIXME: check top bits */
      break;

    case TIR_S_C_STO_LD:
      /* Store long displaced: pop stack, sub lc+4, write long
	 arg: none.  */
      dummy = _bfd_vms_pop (abfd, &psect);
      dummy -= ((PRIV (sections)[psect])->vma + 4);
      image_write_l (abfd, dummy & 0xffffffff);/* FIXME: check top bits */
      break;

    case TIR_S_C_STO_LI:
      /* Store short literal: pop stack, write byte
	 arg: none.  */
      dummy = _bfd_vms_pop (abfd, &psect);
      image_write_b (abfd, dummy & 0xff);/* FIXME: check top bits */
      break;

    case TIR_S_C_STO_PIDR:
      /* Store position independent data reference: pop stack, write longword
	 arg: none.
	 FIXME: incomplete !  */
      dummy = _bfd_vms_pop (abfd, &psect);
      image_write_l (abfd, dummy & 0xffffffff);
      break;

    case TIR_S_C_STO_PICR:
      /* Store position independent code reference: pop stack, write longword
	 arg: none.
	 FIXME: incomplete !  */
      dummy = _bfd_vms_pop (abfd, &psect);
      image_write_b (abfd, 0x9f);
      image_write_l (abfd, dummy & 0xffffffff);
      break;

    case TIR_S_C_STO_RIVB:
      /* Store repeated immediate variable bytes
	 1-byte count n field followed by n bytes of data
	 pop stack, write n bytes <stack> times.  */
      size = *ptr++;
      dummy = (unsigned long) _bfd_vms_pop (abfd, NULL);
      while (dummy-- > 0L)
	image_dump (abfd, ptr, size, 0);
      ptr += size;
      break;

    case TIR_S_C_STO_B:
      /* Store byte from top longword.  */
      dummy = (unsigned long) _bfd_vms_pop (abfd, NULL);
      image_write_b (abfd, dummy & 0xff);
      break;

    case TIR_S_C_STO_W:
      /* Store word from top longword.  */
      dummy = (unsigned long) _bfd_vms_pop (abfd, NULL);
      image_write_w (abfd, dummy & 0xffff);
      break;

    case TIR_S_C_STO_RB:
      /* Store repeated byte from top longword.  */
      size = (unsigned long) _bfd_vms_pop (abfd, NULL);
      dummy = (unsigned long) _bfd_vms_pop (abfd, NULL);
      while (size-- > 0)
	image_write_b (abfd, dummy & 0xff);
      break;

    case TIR_S_C_STO_RW:
      /* Store repeated word from top longword.  */
      size = (unsigned long) _bfd_vms_pop (abfd, NULL);
      dummy = (unsigned long) _bfd_vms_pop (abfd, NULL);
      while (size-- > 0)
	image_write_w (abfd, dummy & 0xffff);
      break;

    case TIR_S_C_STO_RSB:
    case TIR_S_C_STO_RSW:
    case TIR_S_C_STO_RL:
    case TIR_S_C_STO_VPS:
    case TIR_S_C_STO_USB:
    case TIR_S_C_STO_USW:
    case TIR_S_C_STO_RUB:
    case TIR_S_C_STO_RUW:
    case TIR_S_C_STO_PIRR:
      (*_bfd_error_handler) (_("%s: not implemented"), tir_cmd_name (ptr[-1]));
      break;

    default:
      (*_bfd_error_handler) (_("reserved STO cmd %d"), ptr[-1]);
      break;
    }

  return ptr;
}

/* Stack operator commands
   All 32 bit signed arithmetic
   All word just like a stack calculator
   Arguments are popped from stack, results are pushed on stack

   See table 7-5 of the VAX/VMS linker manual.  */

static unsigned char *
tir_opr (bfd * abfd, unsigned char *ptr)
{
  long op1, op2;

#if VMS_DEBUG
  _bfd_vms_debug (5, "tir_opr %d\n", *ptr);
#endif

  /* Operation.  */
  switch (*ptr++)
    {
    case TIR_S_C_OPR_NOP: /* No-op.  */
      break;

    case TIR_S_C_OPR_ADD: /* Add.  */
      op1 = (long) _bfd_vms_pop (abfd, NULL);
      op2 = (long) _bfd_vms_pop (abfd, NULL);
      _bfd_vms_push (abfd, (uquad) (op1 + op2), -1);
      break;

    case TIR_S_C_OPR_SUB: /* Subtract.  */
      op1 = (long) _bfd_vms_pop (abfd, NULL);
      op2 = (long) _bfd_vms_pop (abfd, NULL);
      _bfd_vms_push (abfd, (uquad) (op2 - op1), -1);
      break;

    case TIR_S_C_OPR_MUL: /* Multiply.  */
      op1 = (long) _bfd_vms_pop (abfd, NULL);
      op2 = (long) _bfd_vms_pop (abfd, NULL);
      _bfd_vms_push (abfd, (uquad) (op1 * op2), -1);
      break;

    case TIR_S_C_OPR_DIV: /* Divide.  */
      op1 = (long) _bfd_vms_pop (abfd, NULL);
      op2 = (long) _bfd_vms_pop (abfd, NULL);
      if (op2 == 0)
	_bfd_vms_push (abfd, (uquad) 0, -1);
      else
	_bfd_vms_push (abfd, (uquad) (op2 / op1), -1);
      break;

    case TIR_S_C_OPR_AND: /* Logical AND.  */
      op1 = (long) _bfd_vms_pop (abfd, NULL);
      op2 = (long) _bfd_vms_pop (abfd, NULL);
      _bfd_vms_push (abfd, (uquad) (op1 & op2), -1);
      break;

    case TIR_S_C_OPR_IOR: /* Logical inclusive OR.  */
      op1 = (long) _bfd_vms_pop (abfd, NULL);
      op2 = (long) _bfd_vms_pop (abfd, NULL);
      _bfd_vms_push (abfd, (uquad) (op1 | op2), -1);
      break;

    case TIR_S_C_OPR_EOR: /* Logical exclusive OR.  */
      op1 = (long) _bfd_vms_pop (abfd, NULL);
      op2 = (long) _bfd_vms_pop (abfd, NULL);
      _bfd_vms_push (abfd, (uquad) (op1 ^ op2), -1);
      break;

    case TIR_S_C_OPR_NEG: /* Negate.  */
      op1 = (long) _bfd_vms_pop (abfd, NULL);
      _bfd_vms_push (abfd, (uquad) (-op1), -1);
      break;

    case TIR_S_C_OPR_COM: /* Complement.  */
      op1 = (long) _bfd_vms_pop (abfd, NULL);
      _bfd_vms_push (abfd, (uquad) (op1 ^ -1L), -1);
      break;

    case TIR_S_C_OPR_INSV: /* Insert field.  */
      (void) _bfd_vms_pop (abfd, NULL);
      (*_bfd_error_handler)  (_("%s: not fully implemented"),
			      tir_cmd_name (ptr[-1]));
      break;

    case TIR_S_C_OPR_ASH: /* Arithmetic shift.  */
      op1 = (long) _bfd_vms_pop (abfd, NULL);
      op2 = (long) _bfd_vms_pop (abfd, NULL);
      if (HIGHBIT (op1))	/* Shift right.  */
	op2 >>= op1;
      else			/* Shift left.  */
	op2 <<= op1;
      _bfd_vms_push (abfd, (uquad) op2, -1);
      (*_bfd_error_handler)  (_("%s: not fully implemented"),
			      tir_cmd_name (ptr[-1]));
      break;

    case TIR_S_C_OPR_USH: /* Unsigned shift.  */
      op1 = (long) _bfd_vms_pop (abfd, NULL);
      op2 = (long) _bfd_vms_pop (abfd, NULL);
      if (HIGHBIT (op1))	/* Shift right.  */
	op2 >>= op1;
      else			/* Shift left.  */
	op2 <<= op1;
      _bfd_vms_push (abfd, (uquad) op2, -1);
      (*_bfd_error_handler)  (_("%s: not fully implemented"),
			      tir_cmd_name (ptr[-1]));
      break;

    case TIR_S_C_OPR_ROT: /* Rotate.  */
      op1 = (long) _bfd_vms_pop (abfd, NULL);
      op2 = (long) _bfd_vms_pop (abfd, NULL);
      if (HIGHBIT (0))	/* Shift right.  */
	op2 >>= op1;
      else		/* Shift left.  */
	op2 <<= op1;
      _bfd_vms_push (abfd, (uquad) op2, -1);
      (*_bfd_error_handler)  (_("%s: not fully implemented"),
			      tir_cmd_name (ptr[-1]));
      break;

    case TIR_S_C_OPR_SEL: /* Select.  */
      if ((long) _bfd_vms_pop (abfd, NULL) & 0x01L)
	(void) _bfd_vms_pop (abfd, NULL);
      else
	{
	  op1 = (long) _bfd_vms_pop (abfd, NULL);
	  (void) _bfd_vms_pop (abfd, NULL);
	  _bfd_vms_push (abfd, (uquad) op1, -1);
	}
      break;

    case TIR_S_C_OPR_REDEF: /* Redefine symbol to current location.  */
    case TIR_S_C_OPR_DFLIT: /* Define a literal.  */
      (*_bfd_error_handler) (_("%s: not supported"),
			     tir_cmd_name (ptr[-1]));
      break;

    default:
      (*_bfd_error_handler) (_("reserved OPR cmd %d"), ptr[-1]);
      break;
    }

  return ptr;
}

/* Control commands

   See table 7-6 of the VAX/VMS linker manual.  */

static unsigned char *
tir_ctl (bfd * abfd, unsigned char *ptr)
{
  unsigned long dummy;
  unsigned int psect;

#if VMS_DEBUG
  _bfd_vms_debug (5, "tir_ctl %d\n", *ptr);
#endif

  switch (*ptr++)
    {
    case TIR_S_C_CTL_SETRB:
      /* Set relocation base: pop stack, set image location counter
	 arg: none.  */
      dummy = _bfd_vms_pop (abfd, (int *) &psect);
      if (psect >= PRIV (section_count))
	alloc_section (abfd, psect);
      image_set_ptr (abfd, (int) psect, (uquad) dummy);
      break;

    case TIR_S_C_CTL_AUGRB:
      /* Augment relocation base: increment image location counter by offset
	 arg: lw	offset value.  */
      dummy = bfd_getl32 (ptr);
      image_inc_ptr (abfd, (uquad) dummy);
      break;

    case TIR_S_C_CTL_DFLOC:
      /* Define location: pop index, save location counter under index
	 arg: none.  */
      dummy = _bfd_vms_pop (abfd, NULL);
      (*_bfd_error_handler) (_("%s: not fully implemented"),
			     tir_cmd_name (ptr[-1]));
      break;

    case TIR_S_C_CTL_STLOC:
      /* Set location: pop index, restore location counter from index
	 arg: none.  */
      dummy = _bfd_vms_pop (abfd, (int *) &psect);
      (*_bfd_error_handler) (_("%s: not fully implemented"),
			     tir_cmd_name (ptr[-1]));
      break;

    case TIR_S_C_CTL_STKDL:
      /* Stack defined location: pop index, push location counter from index
	 arg: none.  */
      dummy = _bfd_vms_pop (abfd, (int *) &psect);
      (*_bfd_error_handler) (_("%s: not fully implemented"),
			     tir_cmd_name (ptr[-1]));
      break;

    default:
      (*_bfd_error_handler) (_("reserved CTL cmd %d"), ptr[-1]);
      break;
    }
  return ptr;
}

/* Handle command from TIR section.  */

static unsigned char *
tir_cmd (bfd * abfd, unsigned char *ptr)
{
  struct
  {
    int mincod;
    int maxcod;
    unsigned char * (*explain) (bfd *, unsigned char *);
  }
  tir_table[] =
  {
    { 0,		 TIR_S_C_MAXSTACOD, tir_sta },
    { TIR_S_C_MINSTOCOD, TIR_S_C_MAXSTOCOD, tir_sto },
    { TIR_S_C_MINOPRCOD, TIR_S_C_MAXOPRCOD, tir_opr },
    { TIR_S_C_MINCTLCOD, TIR_S_C_MAXCTLCOD, tir_ctl },
    { -1, -1, NULL }
  };
  int i = 0;

#if VMS_DEBUG
  _bfd_vms_debug (4, "tir_cmd %d/%x\n", *ptr, *ptr);
  _bfd_hexdump (8, ptr, 16, (int) ptr);
#endif

  if (*ptr & 0x80)
    {
      /* Store immediate.  */
      i = 128 - (*ptr++ & 0x7f);
      image_dump (abfd, ptr, i, 0);
      ptr += i;
    }
  else
    {
      while (tir_table[i].mincod >= 0)
	{
	  if ( (tir_table[i].mincod <= *ptr)
	       && (*ptr <= tir_table[i].maxcod))
	    {
	      ptr = tir_table[i].explain (abfd, ptr);
	      break;
	    }
	  i++;
	}
      if (tir_table[i].mincod < 0)
	{
	  (*_bfd_error_handler) (_("obj code %d not found"), *ptr);
	  ptr = 0;
	}
    }

  return ptr;
}

/* Handle command from ETIR section.  */

static int
etir_cmd (bfd * abfd, int cmd, unsigned char *ptr)
{
  static struct
  {
    int mincod;
    int maxcod;
    bfd_boolean (*explain) (bfd *, int, unsigned char *);
  }
  etir_table[] =
  {
    { ETIR_S_C_MINSTACOD, ETIR_S_C_MAXSTACOD, etir_sta },
    { ETIR_S_C_MINSTOCOD, ETIR_S_C_MAXSTOCOD, etir_sto },
    { ETIR_S_C_MINOPRCOD, ETIR_S_C_MAXOPRCOD, etir_opr },
    { ETIR_S_C_MINCTLCOD, ETIR_S_C_MAXCTLCOD, etir_ctl },
    { ETIR_S_C_MINSTCCOD, ETIR_S_C_MAXSTCCOD, etir_stc },
    { -1, -1, NULL }
  };

  int i = 0;

#if VMS_DEBUG
  _bfd_vms_debug (4, "etir_cmd %d/%x\n", cmd, cmd);
  _bfd_hexdump (8, ptr, 16, (int) ptr);
#endif

  while (etir_table[i].mincod >= 0)
    {
      if ( (etir_table[i].mincod <= cmd)
	   && (cmd <= etir_table[i].maxcod))
	{
	  if (!etir_table[i].explain (abfd, cmd, ptr))
	    return -1;
	  break;
	}
      i++;
    }

#if VMS_DEBUG
  _bfd_vms_debug (4, "etir_cmd: = 0\n");
#endif
  return 0;
}

/* Text Information and Relocation Records (OBJ$C_TIR)
   handle tir record.  */

static int
analyze_tir (bfd * abfd, unsigned char *ptr, unsigned int length)
{
  unsigned char *maxptr;

#if VMS_DEBUG
  _bfd_vms_debug (3, "analyze_tir: %d bytes\n", length);
#endif

  maxptr = ptr + length;

  while (ptr < maxptr)
    {
      ptr = tir_cmd (abfd, ptr);
      if (ptr == 0)
	return -1;
    }

  return 0;
}

/* Text Information and Relocation Records (EOBJ$C_ETIR)
   handle etir record.  */

static int
analyze_etir (bfd * abfd, unsigned char *ptr, unsigned int length)
{
  int cmd;
  unsigned char *maxptr;
  int result = 0;

#if VMS_DEBUG
  _bfd_vms_debug (3, "analyze_etir: %d bytes\n", length);
#endif

  maxptr = ptr + length;

  while (ptr < maxptr)
    {
      cmd = bfd_getl16 (ptr);
      length = bfd_getl16 (ptr + 2);
      result = etir_cmd (abfd, cmd, ptr+4);
      if (result != 0)
	break;
      ptr += length;
    }

#if VMS_DEBUG
  _bfd_vms_debug (3, "analyze_etir: = %d\n", result);
#endif

  return result;
}

/* Process ETIR record
   Return 0 on success, -1 on error.  */

int
_bfd_vms_slurp_tir (bfd * abfd, int objtype)
{
  int result;

#if VMS_DEBUG
  _bfd_vms_debug (2, "TIR/ETIR\n");
#endif

  switch (objtype)
    {
    case EOBJ_S_C_ETIR:
      PRIV (vms_rec) += 4;	/* Skip type, size.  */
      PRIV (rec_size) -= 4;
      result = analyze_etir (abfd, PRIV (vms_rec), (unsigned) PRIV (rec_size));
      break;
    case OBJ_S_C_TIR:
      PRIV (vms_rec) += 1;	/* Skip type.  */
      PRIV (rec_size) -= 1;
      result = analyze_tir (abfd, PRIV (vms_rec), (unsigned) PRIV (rec_size));
      break;
    default:
      result = -1;
      break;
    }

  return result;
}

/* Process EDBG record
   Return 0 on success, -1 on error

   Not implemented yet.  */

int
_bfd_vms_slurp_dbg (bfd * abfd, int objtype ATTRIBUTE_UNUSED)
{
#if VMS_DEBUG
  _bfd_vms_debug (2, "DBG/EDBG\n");
#endif

  abfd->flags |= (HAS_DEBUG | HAS_LINENO);
  return 0;
}

/* Process ETBT record
   Return 0 on success, -1 on error

   Not implemented yet.  */

int
_bfd_vms_slurp_tbt (bfd * abfd ATTRIBUTE_UNUSED,
		    int objtype ATTRIBUTE_UNUSED)
{
#if VMS_DEBUG
  _bfd_vms_debug (2, "TBT/ETBT\n");
#endif

  return 0;
}

/* Process LNK record
   Return 0 on success, -1 on error

   Not implemented yet.  */

int
_bfd_vms_slurp_lnk (bfd * abfd ATTRIBUTE_UNUSED,
		    int objtype ATTRIBUTE_UNUSED)
{
#if VMS_DEBUG
  _bfd_vms_debug (2, "LNK\n");
#endif

  return 0;
}

/* Start ETIR record for section #index at virtual addr offset.  */

static void
start_etir_record (bfd * abfd, int index, uquad offset, bfd_boolean justoffset)
{
  if (!justoffset)
    {
      /* One ETIR per section.  */
      _bfd_vms_output_begin (abfd, EOBJ_S_C_ETIR, -1);
      _bfd_vms_output_push (abfd);
    }

  /* Push start offset.  */
  _bfd_vms_output_begin (abfd, ETIR_S_C_STA_PQ, -1);
  _bfd_vms_output_long (abfd, (unsigned long) index);
  _bfd_vms_output_quad (abfd, (uquad) offset);
  _bfd_vms_output_flush (abfd);

  /* Start = pop ().  */
  _bfd_vms_output_begin (abfd, ETIR_S_C_CTL_SETRB, -1);
  _bfd_vms_output_flush (abfd);
}

static void
end_etir_record (bfd * abfd)
{
  _bfd_vms_output_pop (abfd);
  _bfd_vms_output_end (abfd);
}

/* WRITE ETIR SECTION

   This is still under construction and therefore not documented.  */

static void
sto_imm (bfd * abfd, vms_section *sptr, bfd_vma vaddr, int index)
{
  int size;
  int ssize;
  unsigned char *cptr;

#if VMS_DEBUG
  _bfd_vms_debug (8, "sto_imm %d bytes\n", sptr->size);
  _bfd_hexdump (9, sptr->contents, (int) sptr->size, (int) vaddr);
#endif

  ssize = sptr->size;
  cptr = sptr->contents;

  while (ssize > 0)
    {
      /* Try all the rest.  */
      size = ssize;

      if (_bfd_vms_output_check (abfd, size) < 0)
	{
	  /* Doesn't fit, split !  */
	  end_etir_record (abfd);
	  start_etir_record (abfd, index, vaddr, FALSE);
	  /* Get max size.  */
	  size = _bfd_vms_output_check (abfd, 0);
	  /* More than what's left ?  */
	  if (size > ssize)
	    size = ssize;
	}

      _bfd_vms_output_begin (abfd, ETIR_S_C_STO_IMM, -1);
      _bfd_vms_output_long (abfd, (unsigned long) (size));
      _bfd_vms_output_dump (abfd, cptr, size);
      _bfd_vms_output_flush (abfd);

#if VMS_DEBUG
      _bfd_vms_debug (10, "dumped %d bytes\n", size);
      _bfd_hexdump (10, cptr, (int) size, (int) vaddr);
#endif

      vaddr += size;
      ssize -= size;
      cptr += size;
    }
}

/* Write section contents for bfd abfd.  */

int
_bfd_vms_write_tir (bfd * abfd, int objtype ATTRIBUTE_UNUSED)
{
  asection *section;
  vms_section *sptr;
  int nextoffset;

#if VMS_DEBUG
  _bfd_vms_debug (2, "vms_write_tir (%p, %d)\n", abfd, objtype);
#endif

  _bfd_vms_output_alignment (abfd, 4);

  nextoffset = 0;
  PRIV (vms_linkage_index) = 1;

  /* Dump all other sections.  */
  section = abfd->sections;

  while (section != NULL)
    {

#if VMS_DEBUG
      _bfd_vms_debug (4, "writing %d. section '%s' (%d bytes)\n",
		      section->index, section->name,
		      (int) (section->size));
#endif

      if (section->flags & SEC_RELOC)
	{
	  int i;

	  if ((i = section->reloc_count) <= 0)
	    (*_bfd_error_handler) (_("SEC_RELOC with no relocs in section %s"),
				   section->name);
#if VMS_DEBUG
	  else
	    {
	      arelent **rptr;
	      _bfd_vms_debug (4, "%d relocations:\n", i);
	      rptr = section->orelocation;
	      while (i-- > 0)
		{
		  _bfd_vms_debug (4, "sym %s in sec %s, value %08lx, addr %08lx, off %08lx, len %d: %s\n",
				  (*(*rptr)->sym_ptr_ptr)->name,
				  (*(*rptr)->sym_ptr_ptr)->section->name,
				  (long) (*(*rptr)->sym_ptr_ptr)->value,
				  (*rptr)->address, (*rptr)->addend,
				  bfd_get_reloc_size ((*rptr)->howto),
				  (*rptr)->howto->name);
		  rptr++;
		}
	    }
#endif
	}

      if ((section->flags & SEC_HAS_CONTENTS)
	  && (! bfd_is_com_section (section)))
	{
	  /* Virtual addr in section.  */
	  bfd_vma vaddr;

	  sptr = _bfd_get_vms_section (abfd, section->index);
	  if (sptr == NULL)
	    {
	      bfd_set_error (bfd_error_no_contents);
	      return -1;
	    }

	  vaddr = (bfd_vma) (sptr->offset);

	  start_etir_record (abfd, section->index, (uquad) sptr->offset,
			     FALSE);

	  while (sptr != NULL)
	    {
	      /* One STA_PQ, CTL_SETRB per vms_section.  */
	      if (section->flags & SEC_RELOC)
		{
		  /* Check for relocs.  */
		  arelent **rptr = section->orelocation;
		  int i = section->reloc_count;

		  for (;;)
		    {
		      bfd_size_type addr = (*rptr)->address;
		      bfd_size_type len = bfd_get_reloc_size ((*rptr)->howto);
		      if (sptr->offset < addr)
			{
			  /* Sptr starts before reloc.  */
			  bfd_size_type before = addr - sptr->offset;
			  if (sptr->size <= before)
			    {
			      /* Complete before.  */
			      sto_imm (abfd, sptr, vaddr, section->index);
			      vaddr += sptr->size;
			      break;
			    }
			  else
			    {
			      /* Partly before.  */
			      int after = sptr->size - before;

			      sptr->size = before;
			      sto_imm (abfd, sptr, vaddr, section->index);
			      vaddr += sptr->size;
			      sptr->contents += before;
			      sptr->offset += before;
			      sptr->size = after;
			    }
			}
		      else if (sptr->offset == addr)
			{
			  /* Sptr starts at reloc.  */
			  asymbol *sym = *(*rptr)->sym_ptr_ptr;
			  asection *sec = sym->section;

			  switch ((*rptr)->howto->type)
			    {
			    case ALPHA_R_IGNORE:
			      break;

			    case ALPHA_R_REFLONG:
			      {
				if (bfd_is_und_section (sym->section))
				  {
				    int slen = strlen ((char *) sym->name);
				    char *hash;

				    if (_bfd_vms_output_check (abfd, slen) < 0)
				      {
					end_etir_record (abfd);
					start_etir_record (abfd,
							   section->index,
							   vaddr, FALSE);
				      }
				    _bfd_vms_output_begin (abfd,
							   ETIR_S_C_STO_GBL_LW,
							   -1);
				    hash = (_bfd_vms_length_hash_symbol
					    (abfd, sym->name, EOBJ_S_C_SYMSIZ));
				    _bfd_vms_output_counted (abfd, hash);
				    _bfd_vms_output_flush (abfd);
				  }
				else if (bfd_is_abs_section (sym->section))
				  {
				    if (_bfd_vms_output_check (abfd, 16) < 0)
				      {
					end_etir_record (abfd);
					start_etir_record (abfd,
							   section->index,
							   vaddr, FALSE);
				      }
				    _bfd_vms_output_begin (abfd,
							   ETIR_S_C_STA_LW,
							   -1);
				    _bfd_vms_output_quad (abfd,
							  (uquad) sym->value);
				    _bfd_vms_output_flush (abfd);
				    _bfd_vms_output_begin (abfd,
							   ETIR_S_C_STO_LW,
							   -1);
				    _bfd_vms_output_flush (abfd);
				  }
				else
				  {
				    if (_bfd_vms_output_check (abfd, 32) < 0)
				      {
					end_etir_record (abfd);
					start_etir_record (abfd,
							   section->index,
							   vaddr, FALSE);
				      }
				    _bfd_vms_output_begin (abfd,
							   ETIR_S_C_STA_PQ,
							   -1);
				    _bfd_vms_output_long (abfd,
							  (unsigned long) (sec->index));
				    _bfd_vms_output_quad (abfd,
							  ((uquad) (*rptr)->addend
							   + (uquad) sym->value));
				    _bfd_vms_output_flush (abfd);
				    _bfd_vms_output_begin (abfd,
							   ETIR_S_C_STO_LW,
							   -1);
				    _bfd_vms_output_flush (abfd);
				  }
			      }
			      break;

			    case ALPHA_R_REFQUAD:
			      {
				if (bfd_is_und_section (sym->section))
				  {
				    int slen = strlen ((char *) sym->name);
				    char *hash;

				    if (_bfd_vms_output_check (abfd, slen) < 0)
				      {
					end_etir_record (abfd);
					start_etir_record (abfd,
							   section->index,
							   vaddr, FALSE);
				      }
				    _bfd_vms_output_begin (abfd,
							   ETIR_S_C_STO_GBL,
							   -1);
				    hash = (_bfd_vms_length_hash_symbol
					    (abfd, sym->name, EOBJ_S_C_SYMSIZ));
				    _bfd_vms_output_counted (abfd, hash);
				    _bfd_vms_output_flush (abfd);
				  }
				else if (bfd_is_abs_section (sym->section))
				  {
				    if (_bfd_vms_output_check (abfd, 16) < 0)
				      {
					end_etir_record (abfd);
					start_etir_record (abfd,
							   section->index,
							   vaddr, FALSE);
				      }
				    _bfd_vms_output_begin (abfd,
							   ETIR_S_C_STA_QW,
							   -1);
				    _bfd_vms_output_quad (abfd,
							  (uquad) sym->value);
				    _bfd_vms_output_flush (abfd);
				    _bfd_vms_output_begin (abfd,
							   ETIR_S_C_STO_QW,
							   -1);
				    _bfd_vms_output_flush (abfd);
				  }
				else
				  {
				    if (_bfd_vms_output_check (abfd, 32) < 0)
				      {
					end_etir_record (abfd);
					start_etir_record (abfd,
							   section->index,
							   vaddr, FALSE);
				      }
				    _bfd_vms_output_begin (abfd,
							   ETIR_S_C_STA_PQ,
							   -1);
				    _bfd_vms_output_long (abfd,
							  (unsigned long) (sec->index));
				    _bfd_vms_output_quad (abfd,
							  ((uquad) (*rptr)->addend
							   + (uquad) sym->value));
				    _bfd_vms_output_flush (abfd);
				    _bfd_vms_output_begin (abfd,
							   ETIR_S_C_STO_OFF,
							   -1);
				    _bfd_vms_output_flush (abfd);
				  }
			      }
			      break;

			    case ALPHA_R_HINT:
			      {
				int hint_size;
				char *hash ATTRIBUTE_UNUSED;

				hint_size = sptr->size;
				sptr->size = len;
				sto_imm (abfd, sptr, vaddr, section->index);
				sptr->size = hint_size;
			      }
			      break;
			    case ALPHA_R_LINKAGE:
			      {
				char *hash;

				if (_bfd_vms_output_check (abfd, 64) < 0)
				  {
				    end_etir_record (abfd);
				    start_etir_record (abfd, section->index,
						       vaddr, FALSE);
				  }
				_bfd_vms_output_begin (abfd,
						       ETIR_S_C_STC_LP_PSB,
						       -1);
				_bfd_vms_output_long (abfd,
						      (unsigned long) PRIV (vms_linkage_index));
				PRIV (vms_linkage_index) += 2;
				hash = (_bfd_vms_length_hash_symbol
					(abfd, sym->name, EOBJ_S_C_SYMSIZ));
				_bfd_vms_output_counted (abfd, hash);
				_bfd_vms_output_byte (abfd, 0);
				_bfd_vms_output_flush (abfd);
			      }
			      break;

			    case ALPHA_R_CODEADDR:
			      {
				int slen = strlen ((char *) sym->name);
				char *hash;
				if (_bfd_vms_output_check (abfd, slen) < 0)
				  {
				    end_etir_record (abfd);
				    start_etir_record (abfd,
						       section->index,
						       vaddr, FALSE);
				  }
				_bfd_vms_output_begin (abfd,
						       ETIR_S_C_STO_CA,
						       -1);
				hash = (_bfd_vms_length_hash_symbol
					(abfd, sym->name, EOBJ_S_C_SYMSIZ));
				_bfd_vms_output_counted (abfd, hash);
				_bfd_vms_output_flush (abfd);
			      }
			      break;

			    default:
			      (*_bfd_error_handler) (_("Unhandled relocation %s"),
						     (*rptr)->howto->name);
			      break;
			    }

			  vaddr += len;

			  if (len == sptr->size)
			    {
			      break;
			    }
			  else
			    {
			      sptr->contents += len;
			      sptr->offset += len;
			      sptr->size -= len;
			      i--;
			      rptr++;
			    }
			}
		      else
			{
			  /* Sptr starts after reloc.  */
			  i--;
			  /* Check next reloc.  */
			  rptr++;
			}

		      if (i == 0)
			{
			  /* All reloc checked.  */
			  if (sptr->size > 0)
			    {
			      /* Dump rest.  */
			      sto_imm (abfd, sptr, vaddr, section->index);
			      vaddr += sptr->size;
			    }
			  break;
			}
		    }
		}
	      else
		{
		  /* No relocs, just dump.  */
		  sto_imm (abfd, sptr, vaddr, section->index);
		  vaddr += sptr->size;
		}

	      sptr = sptr->next;
	    }

	  end_etir_record (abfd);
	}

      section = section->next;
    }

  _bfd_vms_output_alignment (abfd, 2);
  return 0;
}

/* Write traceback data for bfd abfd.  */

int
_bfd_vms_write_tbt (bfd * abfd ATTRIBUTE_UNUSED,
		    int objtype ATTRIBUTE_UNUSED)
{
#if VMS_DEBUG
  _bfd_vms_debug (2, "vms_write_tbt (%p, %d)\n", abfd, objtype);
#endif

  return 0;
}

/* Write debug info for bfd abfd.  */

int
_bfd_vms_write_dbg (bfd * abfd ATTRIBUTE_UNUSED,
		    int objtype ATTRIBUTE_UNUSED)
{
#if VMS_DEBUG
  _bfd_vms_debug (2, "vms_write_dbg (%p, objtype)\n", abfd, objtype);
#endif

  return 0;
}
