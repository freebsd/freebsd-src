/* Disassemble SH instructions.
   Copyright (C) 1993, 1995 Free Software Foundation, Inc.

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

#include <stdio.h>
#define STATIC_TABLE
#define DEFINE_TABLE

#include "sh-opc.h"
#include "dis-asm.h"

#define LITTLE_BIT 2

static int 
print_insn_shx(memaddr, info)
     bfd_vma memaddr;
     struct disassemble_info *info;
{
  fprintf_ftype fprintf = info->fprintf_func;
  void *stream = info->stream;
  unsigned  char insn[2];
  unsigned  char nibs[4];
  int status;
  int relmask = ~0;
  sh_opcode_info *op;
  
  status = info->read_memory_func(memaddr, insn, 2, info);

  if (status != 0) 
    {
      info->memory_error_func(status, memaddr, info);
      return -1;
    }



  if (info->flags & LITTLE_BIT) 
    {
      nibs[0] = (insn[1] >> 4) & 0xf;
      nibs[1] = insn[1] & 0xf;

      nibs[2] = (insn[0] >> 4) & 0xf;
      nibs[3] = insn[0] & 0xf;
    }
  else 
    {
      nibs[0] = (insn[0] >> 4) & 0xf;
      nibs[1] = insn[0] & 0xf;

      nibs[2] = (insn[1] >> 4) & 0xf;
      nibs[3] = insn[1] & 0xf;
    }

  for (op = sh_table; op->name; op++) 
    {
      int n;
      int imm;
      int rn;
      int rm;
      int rb;

      for (n = 0; n < 4; n++) {
	int i = op->nibbles[n];
	if (i < 16) 
	  {
	    if (nibs[n] == i) continue;
	    goto fail;
	  }
	switch (i)
	  {
	  case BRANCH_8:
	    imm = (nibs[2] << 4) | (nibs[3]);	  
	    if (imm & 0x80)
	      imm |= ~0xff;
	    imm = ((char)imm) * 2 + 4 ;
	    goto ok;

	  case BRANCH_12:
	    imm = ((nibs[1]) << 8) | (nibs[2] << 4) | (nibs[3]);
	    if (imm & 0x800)
	      imm |= ~0xfff;
	    imm = imm * 2 + 4;
	    goto ok;
	  case IMM_4:
	    imm = nibs[3];
	    goto ok;
	  case IMM_4BY2:
	    imm = nibs[3] <<1;
	    goto ok;
	  case IMM_4BY4:
	    imm = nibs[3] <<2;
	    goto ok;

	    
	  case IMM_8:
	    imm = (nibs[2] << 4) | nibs[3];
	    goto ok;
	  case PCRELIMM_8BY2:
	    imm = ((nibs[2] << 4) | nibs[3]) <<1;
	    relmask  = ~1;
	    
	    goto ok;

	  case PCRELIMM_8BY4:
	    imm = ((nibs[2] << 4) | nibs[3]) <<2;
	    relmask  = ~3;	    
	    goto ok;
	    
	  case IMM_8BY2:
	    imm = ((nibs[2] << 4) | nibs[3]) <<1;
	    goto ok;
	  case IMM_8BY4:
	    imm = ((nibs[2] << 4) | nibs[3]) <<2;
	    goto ok;
	  case DISP_8:
	    imm = (nibs[2] << 4) | (nibs[3]);	  
	    goto ok;
	  case DISP_4:
	    imm = nibs[3];
	    goto ok;
	  case REG_N:
	    rn = nibs[n];
	    break;
	  case REG_M:
	    rm = nibs[n];
	    break;
          case REG_B:
            rb = nibs[n] & 0x07;
            break;	
	  default:
	    abort();
	  }

      }
    ok:
      fprintf(stream,"%s\t", op->name);
      for (n = 0; n < 3 && op->arg[n] != A_END; n++) 
	{
	  if (n && op->arg[1] != A_END)
	    fprintf(stream,",");
	  switch (op->arg[n]) 
	    {
	    case A_IMM:
	      fprintf(stream,"#%d", (char)(imm));
	      break;
	    case A_R0:
	      fprintf(stream,"r0");
	      break;
	    case A_REG_N:
	      fprintf(stream,"r%d", rn);
	      break;
	    case A_INC_N:
	      fprintf(stream,"@r%d+", rn);	
	      break;
	    case A_DEC_N:
	      fprintf(stream,"@-r%d", rn);	
	      break;
	    case A_IND_N:
	      fprintf(stream,"@r%d", rn);	
	      break;
	    case A_DISP_REG_N:
	      fprintf(stream,"@(%d,r%d)",imm, rn);	
	      break;
	    case A_REG_M:
	      fprintf(stream,"r%d", rm);
	      break;
	    case A_INC_M:
	      fprintf(stream,"@r%d+", rm);	
	      break;
	    case A_DEC_M:
	      fprintf(stream,"@-r%d", rm);	
	      break;
	    case A_IND_M:
	      fprintf(stream,"@r%d", rm);	
	      break;
	    case A_DISP_REG_M:
	      fprintf(stream,"@(%d,r%d)",imm, rm);	
	      break;
            case A_REG_B:
              fprintf(stream,"r%d_bank", rb);
	      break;
	    case A_DISP_PC:
	      fprintf(stream,"0x%0x", imm+ 4+(memaddr&relmask));
	      break;
	    case A_IND_R0_REG_N:
	      fprintf(stream,"@(r0,r%d)", rn);
	      break; 
	    case A_IND_R0_REG_M:
	      fprintf(stream,"@(r0,r%d)", rm);
	      break; 
	    case A_DISP_GBR:
	      fprintf(stream,"@(%d,gbr)",imm);
	      break;
	    case A_R0_GBR:
	      fprintf(stream,"@(r0,gbr)");
	      break;
	    case A_BDISP12:
	    case A_BDISP8:
	      (*info->print_address_func) (imm + memaddr, info);
	      break;
	    case A_SR:
	      fprintf(stream,"sr");
	      break;
	    case A_GBR:
	      fprintf(stream,"gbr");
	      break;
	    case A_VBR:
	      fprintf(stream,"vbr");
	      break;
	    case A_SSR:
	      fprintf(stream,"ssr");
	      break;
	    case A_SPC:
	      fprintf(stream,"spc");
	      break;
	    case A_MACH:
	      fprintf(stream,"mach");
	      break;
	    case A_MACL:
	      fprintf(stream,"macl");
	      break;
	    case A_PR:
	      fprintf(stream,"pr");
	      break;
	    case F_REG_N:
	      fprintf(stream,"fr%d", rn);
	      break;
	    case F_REG_M:
	      fprintf(stream,"fr%d", rm);
	      break;
	    case FPSCR_M:
	    case FPSCR_N:
	      fprintf(stream,"fpscr");
	      break;
	    case FPUL_M:
	    case FPUL_N:
	      fprintf(stream,"fpul");
	      break;
	    case F_FR0:
	      fprintf(stream,"fr0");
	      break;
	    default:
	      abort();
	    }
	
	}
      if (!(info->flags & 1)
	  && (op->name[0] == 'j'
	      || (op->name[0] == 'b'
		  && (op->name[1] == 'r' 
		      || op->name[1] == 's'))
	      || (op->name[0] == 'r' && op->name[1] == 't')
	      || (op->name[0] == 'b' && op->name[2] == '.')))
	{
	  info->flags |= 1;
	  fprintf(stream,"\t(slot ");  print_insn_shx(memaddr +2, info);
	  info->flags &= ~1;
	  fprintf(stream,")");
	  return 4;
	}
      
      return 2;
    fail:
      ;

    }
  fprintf(stream,".word 0x%x%x%x%x", nibs[0], nibs[1], nibs[2], nibs[3]);
  return 2;
}


int 
print_insn_shl(memaddr, info)
     bfd_vma memaddr;
     struct disassemble_info *info;
{
  int r;
  info->flags = LITTLE_BIT;
  r =print_insn_shx (memaddr, info);
  return r;
}

int 
print_insn_sh(memaddr, info)
     bfd_vma memaddr;
     struct disassemble_info *info;
{
  int r;
  info->flags = 0;
  r =print_insn_shx (memaddr, info);
  return r;
}
