/* Print VAX instructions.
   Copyright 1995, 1998, 2000, 2001, 2002 Free Software Foundation, Inc.
   Contributed by Pauline Middelink <middelin@polyware.iaf.nl>

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
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#include "sysdep.h"
#include "opcode/vax.h"
#include "dis-asm.h"

/* Local function prototypes */
static int fetch_data PARAMS ((struct disassemble_info *, bfd_byte *));
static int print_insn_arg
  PARAMS ((const char *, unsigned char *, bfd_vma, disassemble_info *));
static int print_insn_mode
  PARAMS ((const char *, int, unsigned char *, bfd_vma, disassemble_info *));


static char *reg_names[] =
{
  "r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7",
  "r8", "r9", "r10", "r11", "ap", "fp", "sp", "pc"
};

/* Sign-extend an (unsigned char). */
#if __STDC__ == 1
#define COERCE_SIGNED_CHAR(ch) ((signed char)(ch))
#else
#define COERCE_SIGNED_CHAR(ch) ((int)(((ch) ^ 0x80) & 0xFF) - 128)
#endif

/* Get a 1 byte signed integer.  */
#define NEXTBYTE(p)  \
  (p += 1, FETCH_DATA (info, p), \
  COERCE_SIGNED_CHAR(p[-1]))

/* Get a 2 byte signed integer.  */
#define COERCE16(x) ((int) (((x) ^ 0x8000) - 0x8000))
#define NEXTWORD(p)  \
  (p += 2, FETCH_DATA (info, p), \
   COERCE16 ((p[-1] << 8) + p[-2]))

/* Get a 4 byte signed integer.  */
#define COERCE32(x) ((int) (((x) ^ 0x80000000) - 0x80000000))
#define NEXTLONG(p)  \
  (p += 4, FETCH_DATA (info, p), \
   (COERCE32 ((((((p[-1] << 8) + p[-2]) << 8) + p[-3]) << 8) + p[-4])))

/* Maximum length of an instruction.  */
#define MAXLEN 25

#include <setjmp.h>

struct private
{
  /* Points to first byte not fetched.  */
  bfd_byte *max_fetched;
  bfd_byte the_buffer[MAXLEN];
  bfd_vma insn_start;
  jmp_buf bailout;
};

/* Make sure that bytes from INFO->PRIVATE_DATA->BUFFER (inclusive)
   to ADDR (exclusive) are valid.  Returns 1 for success, longjmps
   on error.  */
#define FETCH_DATA(info, addr) \
  ((addr) <= ((struct private *)(info->private_data))->max_fetched \
   ? 1 : fetch_data ((info), (addr)))

static int
fetch_data (info, addr)
     struct disassemble_info *info;
     bfd_byte *addr;
{
  int status;
  struct private *priv = (struct private *) info->private_data;
  bfd_vma start = priv->insn_start + (priv->max_fetched - priv->the_buffer);

  status = (*info->read_memory_func) (start,
				      priv->max_fetched,
				      addr - priv->max_fetched,
				      info);
  if (status != 0)
    {
      (*info->memory_error_func) (status, start, info);
      longjmp (priv->bailout, 1);
    }
  else
    priv->max_fetched = addr;

  return 1;
}

/* Print the vax instruction at address MEMADDR in debugged memory,
   on INFO->STREAM.  Returns length of the instruction, in bytes.  */

int
print_insn_vax (memaddr, info)
     bfd_vma memaddr;
     disassemble_info *info;
{
  const struct vot *votp;
  const char *argp;
  unsigned char *arg;
  struct private priv;
  bfd_byte *buffer = priv.the_buffer;

  info->private_data = (PTR) &priv;
  priv.max_fetched = priv.the_buffer;
  priv.insn_start = memaddr;

  if (setjmp (priv.bailout) != 0)
    {
      /* Error return.  */
      return -1;
    }

  argp = NULL;
  /* Check if the info buffer has more than one byte left since
     the last opcode might be a single byte with no argument data.  */
  if (info->buffer_length - (memaddr - info->buffer_vma) > 1)
    {
      FETCH_DATA (info, buffer + 2);
    }
  else
    {
      FETCH_DATA (info, buffer + 1);
      buffer[1] = 0;
    }

  for (votp = &votstrs[0]; votp->name[0]; votp++)
    {
      register vax_opcodeT opcode = votp->detail.code;

      /* 2 byte codes match 2 buffer pos. */
      if ((bfd_byte) opcode == buffer[0]
	  && (opcode >> 8 == 0 || opcode >> 8 == buffer[1]))
	{
	  argp = votp->detail.args;
	  break;
	}
    }
  if (argp == NULL)
    {
      /* Handle undefined instructions. */
      (*info->fprintf_func) (info->stream, ".word 0x%x",
			     (buffer[0] << 8) + buffer[1]);
      return 2;
    }

  /* Point at first byte of argument data, and at descriptor for first
     argument.  */
  arg = buffer + ((votp->detail.code >> 8) ? 2 : 1);

  /* Make sure we have it in mem */
  FETCH_DATA (info, arg);

  (*info->fprintf_func) (info->stream, "%s", votp->name);
  if (*argp)
    (*info->fprintf_func) (info->stream, " ");

  while (*argp)
    {
      arg += print_insn_arg (argp, arg, memaddr + arg - buffer, info);
      argp += 2;
      if (*argp)
	(*info->fprintf_func) (info->stream, ",");
    }

  return arg - buffer;
}

/* Returns number of bytes "eaten" by the operand, or return -1 if an
   invalid operand was found, or -2 if an opcode tabel error was
   found. */

static int
print_insn_arg (d, p0, addr, info)
     const char *d;
     unsigned char *p0;
     bfd_vma addr;		/* PC for this arg to be relative to */
     disassemble_info *info;
{
  int arg_len;

  /* check validity of addressing length */
  switch (d[1])
    {
    case 'b' : arg_len = 1;	break;
    case 'd' : arg_len = 8;	break;
    case 'f' : arg_len = 4;	break;
    case 'g' : arg_len = 8;	break;
    case 'h' : arg_len = 16;	break;
    case 'l' : arg_len = 4;	break;
    case 'o' : arg_len = 16;	break;
    case 'w' : arg_len = 2;	break;
    case 'q' : arg_len = 8;	break;
    default  : abort();
    }

  /* branches have no mode byte */
  if (d[0] == 'b')
    {
      unsigned char *p = p0;

      if (arg_len == 1)
	(*info->print_address_func) (addr + 1 + NEXTBYTE (p), info);
      else
	(*info->print_address_func) (addr + 2 + NEXTWORD (p), info);

      return p - p0;
    }

  return print_insn_mode (d, arg_len, p0, addr, info);
}

static int
print_insn_mode (d, size, p0, addr, info)
     const char *d;
     int size;
     unsigned char *p0;
     bfd_vma addr;		/* PC for this arg to be relative to */
     disassemble_info *info;
{
  unsigned char *p = p0;
  unsigned char mode, reg;

  /* fetch and interpret mode byte */
  mode = (unsigned char) NEXTBYTE (p);
  reg = mode & 0xF;
  switch (mode & 0xF0)
    {
    case 0x00:
    case 0x10:
    case 0x20:
    case 0x30: /* literal mode			$number */
      if (d[1] == 'd' || d[1] == 'f' || d[1] == 'g' || d[1] == 'h')
	(*info->fprintf_func) (info->stream, "$0x%x [%c-float]", mode, d[1]);
      else
        (*info->fprintf_func) (info->stream, "$0x%x", mode);
      break;
    case 0x40: /* index:			base-addr[Rn] */
      p += print_insn_mode (d, size, p0 + 1, addr + 1, info);
      (*info->fprintf_func) (info->stream, "[%s]", reg_names[reg]);
      break;
    case 0x50: /* register:			Rn */
      (*info->fprintf_func) (info->stream, "%s", reg_names[reg]);
      break;
    case 0x60: /* register deferred:		(Rn) */
      (*info->fprintf_func) (info->stream, "(%s)", reg_names[reg]);
      break;
    case 0x70: /* autodecrement:		-(Rn) */
      (*info->fprintf_func) (info->stream, "-(%s)", reg_names[reg]);
      break;
    case 0x80: /* autoincrement:		(Rn)+ */
      if (reg == 0xF)
	{	/* immediate? */
	  int i;

	  FETCH_DATA (info, p + size);
	  (*info->fprintf_func) (info->stream, "$0x");
	  if (d[1] == 'd' || d[1] == 'f' || d[1] == 'g' || d[1] == 'h')
	    {
	      int float_word;

	      float_word = p[0] | (p[1] << 8);
	      if ((d[1] == 'd' || d[1] == 'f')
		  && (float_word & 0xff80) == 0x8000)
		{
		  (*info->fprintf_func) (info->stream, "[invalid %c-float]",
					 d[1]);
		}
	      else
		{
	          for (i = 0; i < size; i++)
		    (*info->fprintf_func) (info->stream, "%02x",
		                           p[size - i - 1]);
	          (*info->fprintf_func) (info->stream, " [%c-float]", d[1]);
		}
	    }
	  else
	    {
	      for (i = 0; i < size; i++)
	        (*info->fprintf_func) (info->stream, "%02x", p[size - i - 1]);
	    }
	  p += size;
	}
      else
	(*info->fprintf_func) (info->stream, "(%s)+", reg_names[reg]);
      break;
    case 0x90: /* autoincrement deferred:	@(Rn)+ */
      if (reg == 0xF)
	(*info->fprintf_func) (info->stream, "*0x%x", NEXTLONG (p));
      else
	(*info->fprintf_func) (info->stream, "@(%s)+", reg_names[reg]);
      break;
    case 0xB0: /* displacement byte deferred:	*displ(Rn) */
      (*info->fprintf_func) (info->stream, "*");
    case 0xA0: /* displacement byte:		displ(Rn) */
      if (reg == 0xF)
	(*info->print_address_func) (addr + 2 + NEXTBYTE (p), info);
      else
	(*info->fprintf_func) (info->stream, "0x%x(%s)", NEXTBYTE (p),
			       reg_names[reg]);
      break;
    case 0xD0: /* displacement word deferred:	*displ(Rn) */
      (*info->fprintf_func) (info->stream, "*");
    case 0xC0: /* displacement word:		displ(Rn) */
      if (reg == 0xF)
	(*info->print_address_func) (addr + 3 + NEXTWORD (p), info);
      else
	(*info->fprintf_func) (info->stream, "0x%x(%s)", NEXTWORD (p),
			       reg_names[reg]);
      break;
    case 0xF0: /* displacement long deferred:	*displ(Rn) */
      (*info->fprintf_func) (info->stream, "*");
    case 0xE0: /* displacement long:		displ(Rn) */
      if (reg == 0xF)
	(*info->print_address_func) (addr + 5 + NEXTLONG (p), info);
      else
	(*info->fprintf_func) (info->stream, "0x%x(%s)", NEXTLONG (p),
			       reg_names[reg]);
      break;
    }

  return p - p0;
}
