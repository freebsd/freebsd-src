/* Intel 387 floating point stuff.
   Copyright 1988, 1989, 1991, 1992, 1993, 1994, 1998, 1999, 2000, 2001
   Free Software Foundation, Inc.

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
#include "frame.h"
#include "inferior.h"
#include "language.h"
#include "value.h"
#include "gdbcore.h"
#include "floatformat.h"
#include "regcache.h"
#include "gdb_assert.h"
#include "doublest.h"

#include "i386-tdep.h"

/* FIXME: Eliminate the next two functions when we have the time to
   change all the callers.  */

void i387_to_double (char *from, char *to);
void double_to_i387 (char *from, char *to);

void
i387_to_double (char *from, char *to)
{
  floatformat_to_double (&floatformat_i387_ext, from, (double *) to);
}

void
double_to_i387 (char *from, char *to)
{
  floatformat_from_double (&floatformat_i387_ext, (double *) from, to);
}


/* FIXME: The functions on this page are used by the old `info float'
   implementations that a few of the i386 targets provide.  These
   functions should be removed if all of these have been converted to
   use the generic implementation based on the new register file
   layout.  */

static void print_387_control_bits (unsigned int control);
static void print_387_status_bits (unsigned int status);

static void
print_387_control_bits (unsigned int control)
{
  switch ((control >> 8) & 3)
    {
    case 0:
      puts_unfiltered (" 24 bit; ");
      break;
    case 1:
      puts_unfiltered (" (bad); ");
      break;
    case 2:
      puts_unfiltered (" 53 bit; ");
      break;
    case 3:
      puts_unfiltered (" 64 bit; ");
      break;
    }
  switch ((control >> 10) & 3)
    {
    case 0:
      puts_unfiltered ("NEAR; ");
      break;
    case 1:
      puts_unfiltered ("DOWN; ");
      break;
    case 2:
      puts_unfiltered ("UP; ");
      break;
    case 3:
      puts_unfiltered ("CHOP; ");
      break;
    }
  if (control & 0x3f)
    {
      puts_unfiltered ("mask");
      if (control & 0x0001)
	puts_unfiltered (" INVAL");
      if (control & 0x0002)
	puts_unfiltered (" DENOR");
      if (control & 0x0004)
	puts_unfiltered (" DIVZ");
      if (control & 0x0008)
	puts_unfiltered (" OVERF");
      if (control & 0x0010)
	puts_unfiltered (" UNDER");
      if (control & 0x0020)
	puts_unfiltered (" LOS");
      puts_unfiltered (";");
    }

  if (control & 0xe080)
    warning ("\nreserved bits on: %s",
	     local_hex_string (control & 0xe080));
}

void
print_387_control_word (unsigned int control)
{
  printf_filtered ("control %s:", local_hex_string(control & 0xffff));
  print_387_control_bits (control);
  puts_unfiltered ("\n");
}

static void
print_387_status_bits (unsigned int status)
{
  printf_unfiltered (" flags %d%d%d%d; ",
		     (status & 0x4000) != 0,
		     (status & 0x0400) != 0,
		     (status & 0x0200) != 0,
		     (status & 0x0100) != 0);
  printf_unfiltered ("top %d; ", (status >> 11) & 7);
  if (status & 0xff) 
    {
      puts_unfiltered ("excep");
      if (status & 0x0001) puts_unfiltered (" INVAL");
      if (status & 0x0002) puts_unfiltered (" DENOR");
      if (status & 0x0004) puts_unfiltered (" DIVZ");
      if (status & 0x0008) puts_unfiltered (" OVERF");
      if (status & 0x0010) puts_unfiltered (" UNDER");
      if (status & 0x0020) puts_unfiltered (" LOS");
      if (status & 0x0040) puts_unfiltered (" STACK");
    }
}

void
print_387_status_word (unsigned int status)
{
  printf_filtered ("status %s:", local_hex_string (status & 0xffff));
  print_387_status_bits (status);
  puts_unfiltered ("\n");
}


/* Implement the `info float' layout based on the register definitions
   in `tm-i386.h'.  */

/* Print the floating point number specified by RAW.  */
static void
print_i387_value (char *raw)
{
  DOUBLEST value;

  /* Using extract_typed_floating here might affect the representation
     of certain numbers such as NaNs, even if GDB is running natively.
     This is fine since our caller already detects such special
     numbers and we print the hexadecimal representation anyway.  */
  value = extract_typed_floating (raw, builtin_type_i387_ext);

  /* We try to print 19 digits.  The last digit may or may not contain
     garbage, but we'd better print one too many.  We need enough room
     to print the value, 1 position for the sign, 1 for the decimal
     point, 19 for the digits and 6 for the exponent adds up to 27.  */
#ifdef PRINTF_HAS_LONG_DOUBLE
  printf_filtered (" %-+27.19Lg", (long double) value);
#else
  printf_filtered (" %-+27.19g", (double) value);
#endif
}

/* Print the classification for the register contents RAW.  */
static void
print_i387_ext (unsigned char *raw)
{
  int sign;
  int integer;
  unsigned int exponent;
  unsigned long fraction[2];

  sign = raw[9] & 0x80;
  integer = raw[7] & 0x80;
  exponent = (((raw[9] & 0x7f) << 8) | raw[8]);
  fraction[0] = ((raw[3] << 24) | (raw[2] << 16) | (raw[1] << 8) | raw[0]);
  fraction[1] = (((raw[7] & 0x7f) << 24) | (raw[6] << 16)
		 | (raw[5] << 8) | raw[4]);

  if (exponent == 0x7fff && integer)
    {
      if (fraction[0] == 0x00000000 && fraction[1] == 0x00000000)
	/* Infinity.  */
	printf_filtered (" %cInf", (sign ? '-' : '+'));
      else if (sign && fraction[0] == 0x00000000 && fraction[1] == 0x40000000)
	/* Real Indefinite (QNaN).  */
	puts_unfiltered (" Real Indefinite (QNaN)");
      else if (fraction[1] & 0x40000000)
	/* QNaN.  */
	puts_filtered (" QNaN");
      else
	/* SNaN.  */
	puts_filtered (" SNaN");
    }
  else if (exponent < 0x7fff && exponent > 0x0000 && integer)
    /* Normal.  */
    print_i387_value (raw);
  else if (exponent == 0x0000)
    {
      /* Denormal or zero.  */
      print_i387_value (raw);
      
      if (integer)
	/* Pseudo-denormal.  */
	puts_filtered (" Pseudo-denormal");
      else if (fraction[0] || fraction[1])
	/* Denormal.  */
	puts_filtered (" Denormal");
    }
  else
    /* Unsupported.  */
    puts_filtered (" Unsupported");
}

/* Print the status word STATUS.  */
static void
print_i387_status_word (unsigned int status)
{
  printf_filtered ("Status Word:         %s",
		   local_hex_string_custom (status, "04"));
  puts_filtered ("  ");
  printf_filtered (" %s", (status & 0x0001) ? "IE" : "  ");
  printf_filtered (" %s", (status & 0x0002) ? "DE" : "  ");
  printf_filtered (" %s", (status & 0x0004) ? "ZE" : "  ");
  printf_filtered (" %s", (status & 0x0008) ? "OE" : "  ");
  printf_filtered (" %s", (status & 0x0010) ? "UE" : "  ");
  printf_filtered (" %s", (status & 0x0020) ? "PE" : "  ");
  puts_filtered ("  ");
  printf_filtered (" %s", (status & 0x0080) ? "ES" : "  ");
  puts_filtered ("  ");
  printf_filtered (" %s", (status & 0x0040) ? "SF" : "  ");
  puts_filtered ("  ");
  printf_filtered (" %s", (status & 0x0100) ? "C0" : "  ");
  printf_filtered (" %s", (status & 0x0200) ? "C1" : "  ");
  printf_filtered (" %s", (status & 0x0400) ? "C2" : "  ");
  printf_filtered (" %s", (status & 0x4000) ? "C3" : "  ");

  puts_filtered ("\n");

  printf_filtered ("                       TOP: %d\n", ((status >> 11) & 7));
}

/* Print the control word CONTROL.  */
static void
print_i387_control_word (unsigned int control)
{
  printf_filtered ("Control Word:        %s",
		   local_hex_string_custom (control, "04"));
  puts_filtered ("  ");
  printf_filtered (" %s", (control & 0x0001) ? "IM" : "  ");
  printf_filtered (" %s", (control & 0x0002) ? "DM" : "  ");
  printf_filtered (" %s", (control & 0x0004) ? "ZM" : "  ");
  printf_filtered (" %s", (control & 0x0008) ? "OM" : "  ");
  printf_filtered (" %s", (control & 0x0010) ? "UM" : "  ");
  printf_filtered (" %s", (control & 0x0020) ? "PM" : "  ");

  puts_filtered ("\n");

  puts_filtered ("                       PC: ");
  switch ((control >> 8) & 3)
    {
    case 0:
      puts_filtered ("Single Precision (24-bits)\n");
      break;
    case 1:
      puts_filtered ("Reserved\n");
      break;
    case 2:
      puts_filtered ("Double Precision (53-bits)\n");
      break;
    case 3:
      puts_filtered ("Extended Precision (64-bits)\n");
      break;
    }
      
  puts_filtered ("                       RC: ");
  switch ((control >> 10) & 3)
    {
    case 0:
      puts_filtered ("Round to nearest\n");
      break;
    case 1:
      puts_filtered ("Round down\n");
      break;
    case 2:
      puts_filtered ("Round up\n");
      break;
    case 3:
      puts_filtered ("Round toward zero\n");
      break;
    }
}

/* Print out the i387 floating poin state.  */
void
i387_float_info (void)
{
  unsigned int fctrl;
  unsigned int fstat;
  unsigned int ftag;
  unsigned int fiseg;
  unsigned int fioff;
  unsigned int foseg;
  unsigned int fooff;
  unsigned int fop;
  int fpreg;
  int top;

  fctrl = read_register (FCTRL_REGNUM);
  fstat = read_register (FSTAT_REGNUM);
  ftag  = read_register (FTAG_REGNUM);
  fiseg = read_register (FCS_REGNUM);
  fioff = read_register (FCOFF_REGNUM);
  foseg = read_register (FDS_REGNUM);
  fooff = read_register (FDOFF_REGNUM);
  fop   = read_register (FOP_REGNUM);
  
  top = ((fstat >> 11) & 7);

  for (fpreg = 7; fpreg >= 0; fpreg--)
    {
      unsigned char raw[FPU_REG_RAW_SIZE];
      int tag = (ftag >> (fpreg * 2)) & 3;
      int i;

      printf_filtered ("%sR%d: ", fpreg == top ? "=>" : "  ", fpreg);

      switch (tag)
	{
	case 0:
	  puts_filtered ("Valid   ");
	  break;
	case 1:
	  puts_filtered ("Zero    ");
	  break;
	case 2:
	  puts_filtered ("Special ");
	  break;
	case 3:
	  puts_filtered ("Empty   ");
	  break;
	}

      read_register_gen ((fpreg + 8 - top) % 8 + FP0_REGNUM, raw);

      puts_filtered ("0x");
      for (i = 9; i >= 0; i--)
	printf_filtered ("%02x", raw[i]);

      if (tag != 3)
	print_i387_ext (raw);

      puts_filtered ("\n");
    }

  puts_filtered ("\n");

  print_i387_status_word (fstat);
  print_i387_control_word (fctrl);
  printf_filtered ("Tag Word:            %s\n",
		   local_hex_string_custom (ftag, "04"));
  printf_filtered ("Instruction Pointer: %s:",
		   local_hex_string_custom (fiseg, "02"));
  printf_filtered ("%s\n", local_hex_string_custom (fioff, "08"));
  printf_filtered ("Operand Pointer:     %s:",
		   local_hex_string_custom (foseg, "02"));
  printf_filtered ("%s\n", local_hex_string_custom (fooff, "08"));
  printf_filtered ("Opcode:              %s\n",
		   local_hex_string_custom (fop ? (fop | 0xd800) : 0, "04"));
}
