/* Generated automatically by the program `genrecog'
from the machine description file `md'.  */

#include "config.h"
#include "rtl.h"
#include "insn-config.h"
#include "recog.h"
#include "real.h"
#include "output.h"
#include "flags.h"


/* `recog' contains a decision tree
   that recognizes whether the rtx X0 is a valid instruction.

   recog returns -1 if the rtx is not valid.
   If the rtx is valid, recog returns a nonnegative number
   which is the insn code number for the pattern that matched.
   This is the same as the order in the machine description of
   the entry that matched.  This number can be used as an index into
   entry that matched.  This number can be used as an index into various
   insn_* tables, such as insn_templates, insn_outfun, and insn_n_operands
   (found in insn-output.c).

   The third argument to recog is an optional pointer to an int.
   If present, recog will accept a pattern if it matches except for
   missing CLOBBER expressions at the end.  In that case, the value
   pointed to by the optional pointer will be set to the number of
   CLOBBERs that need to be added (it should be initialized to zero by
   the caller).  If it is set nonzero, the caller should allocate a
   PARALLEL of the appropriate size, copy the initial entries, and call
   add_clobbers (found in insn-emit.c) to fill in the CLOBBERs.*/

rtx recog_operand[MAX_RECOG_OPERANDS];

rtx *recog_operand_loc[MAX_RECOG_OPERANDS];

rtx *recog_dup_loc[MAX_DUP_OPERANDS];

char recog_dup_num[MAX_DUP_OPERANDS];

#define operands recog_operand

int
recog_1 (x0, insn, pnum_clobbers)
     register rtx x0;
     rtx insn;
     int *pnum_clobbers;
{
  register rtx *ro = &recog_operand[0];
  register rtx x1, x2, x3, x4, x5, x6;
  int tem;

  x1 = XEXP (x0, 1);
  switch (GET_MODE (x1))
    {
    case HImode:
      switch (GET_CODE (x1))
	{
	case ZERO_EXTEND:
	  goto L370;
	case SIGN_EXTEND:
	  goto L390;
	case PLUS:
	  goto L603;
	case MINUS:
	  goto L626;
	case MULT:
	  goto L660;
	case AND:
	  goto L747;
	case IOR:
	  goto L762;
	case XOR:
	  goto L777;
	case NEG:
	  goto L795;
	case NOT:
	  goto L904;
	case ASHIFT:
	  goto L930;
	case ASHIFTRT:
	  goto L958;
	case LSHIFTRT:
	  goto L986;
	case ROTATE:
	  goto L1001;
	case ROTATERT:
	  goto L1016;
	}
    }
  if (general_operand (x1, HImode))
    {
      ro[1] = x1;
      return 51;
    }
  goto ret0;

  L370:
  x2 = XEXP (x1, 0);
  if (nonimmediate_operand (x2, QImode))
    {
      ro[1] = x2;
      return 67;
    }
  goto ret0;

  L390:
  x2 = XEXP (x1, 0);
  if (nonimmediate_operand (x2, QImode))
    {
      ro[1] = x2;
      return 72;
    }
  goto ret0;

  L603:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, HImode))
    {
      ro[1] = x2;
      goto L604;
    }
  goto ret0;

  L604:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, HImode))
    {
      ro[2] = x2;
      return 110;
    }
  goto ret0;

  L626:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, HImode))
    {
      ro[1] = x2;
      goto L627;
    }
  goto ret0;

  L627:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, HImode))
    {
      ro[2] = x2;
      return 118;
    }
  goto ret0;

  L660:
  x2 = XEXP (x1, 0);
  switch (GET_MODE (x2))
    {
    case HImode:
      switch (GET_CODE (x2))
	{
	case ZERO_EXTEND:
	  goto L661;
	case SIGN_EXTEND:
	  goto L668;
	}
    }
  if (general_operand (x2, HImode))
    {
      ro[1] = x2;
      goto L637;
    }
  goto ret0;

  L661:
  x3 = XEXP (x2, 0);
  if (nonimmediate_operand (x3, QImode))
    {
      ro[1] = x3;
      goto L662;
    }
  goto ret0;

  L662:
  x2 = XEXP (x1, 1);
  if (GET_MODE (x2) == HImode && GET_CODE (x2) == ZERO_EXTEND && 1)
    goto L663;
  goto ret0;

  L663:
  x3 = XEXP (x2, 0);
  if (nonimmediate_operand (x3, QImode))
    {
      ro[2] = x3;
      return 127;
    }
  goto ret0;

  L668:
  x3 = XEXP (x2, 0);
  if (nonimmediate_operand (x3, QImode))
    {
      ro[1] = x3;
      goto L669;
    }
  goto ret0;

  L669:
  x2 = XEXP (x1, 1);
  if (GET_MODE (x2) == HImode && GET_CODE (x2) == SIGN_EXTEND && 1)
    goto L670;
  goto ret0;

  L670:
  x3 = XEXP (x2, 0);
  if (nonimmediate_operand (x3, QImode))
    {
      ro[2] = x3;
      return 128;
    }
  goto ret0;

  L637:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, HImode))
    goto L643;
  goto ret0;

  L643:
  ro[2] = x2;
  if (GET_CODE (operands[2]) == CONST_INT && INTVAL (operands[2]) == 0x80)
    return 123;
  L644:
  ro[2] = x2;
  return 124;

  L747:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, HImode))
    {
      ro[1] = x2;
      goto L748;
    }
  goto ret0;

  L748:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, HImode))
    {
      ro[2] = x2;
      return 144;
    }
  goto ret0;

  L762:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, HImode))
    {
      ro[1] = x2;
      goto L763;
    }
  goto ret0;

  L763:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, HImode))
    {
      ro[2] = x2;
      return 147;
    }
  goto ret0;

  L777:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, HImode))
    {
      ro[1] = x2;
      goto L778;
    }
  goto ret0;

  L778:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, HImode))
    {
      ro[2] = x2;
      return 150;
    }
  goto ret0;

  L795:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, HImode))
    {
      ro[1] = x2;
      return 154;
    }
  goto ret0;

  L904:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, HImode))
    {
      ro[1] = x2;
      return 179;
    }
  goto ret0;

  L930:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, HImode))
    {
      ro[1] = x2;
      goto L931;
    }
  goto ret0;

  L931:
  x2 = XEXP (x1, 1);
  if (nonmemory_operand (x2, HImode))
    {
      ro[2] = x2;
      return 185;
    }
  goto ret0;

  L958:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, HImode))
    {
      ro[1] = x2;
      goto L959;
    }
  goto ret0;

  L959:
  x2 = XEXP (x1, 1);
  if (nonmemory_operand (x2, HImode))
    {
      ro[2] = x2;
      return 191;
    }
  goto ret0;

  L986:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, HImode))
    {
      ro[1] = x2;
      goto L987;
    }
  goto ret0;

  L987:
  x2 = XEXP (x1, 1);
  if (nonmemory_operand (x2, HImode))
    {
      ro[2] = x2;
      return 197;
    }
  goto ret0;

  L1001:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, HImode))
    {
      ro[1] = x2;
      goto L1002;
    }
  goto ret0;

  L1002:
  x2 = XEXP (x1, 1);
  if (nonmemory_operand (x2, HImode))
    {
      ro[2] = x2;
      return 200;
    }
  goto ret0;

  L1016:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, HImode))
    {
      ro[1] = x2;
      goto L1017;
    }
  goto ret0;

  L1017:
  x2 = XEXP (x1, 1);
  if (nonmemory_operand (x2, HImode))
    {
      ro[2] = x2;
      return 203;
    }
  goto ret0;
 ret0: return -1;
}

int
recog_2 (x0, insn, pnum_clobbers)
     register rtx x0;
     rtx insn;
     int *pnum_clobbers;
{
  register rtx *ro = &recog_operand[0];
  register rtx x1, x2, x3, x4, x5, x6;
  int tem;

  x1 = XEXP (x0, 1);
  switch (GET_MODE (x1))
    {
    case SImode:
      if (nonimmediate_operand (x1, SImode))
	{
	  ro[0] = x1;
	  return 0;
	}
      break;
    case HImode:
      if (nonimmediate_operand (x1, HImode))
	{
	  ro[0] = x1;
	  return 2;
	}
      break;
    case QImode:
      if (nonimmediate_operand (x1, QImode))
	{
	  ro[0] = x1;
	  return 4;
	}
      break;
    case SFmode:
      if (pnum_clobbers != 0 && register_operand (x1, SFmode))
	{
	  ro[0] = x1;
	  if (TARGET_80387 && ! TARGET_IEEE_FP)
	    {
	      *pnum_clobbers = 1;
	      return 6;
	    }
	  }
      break;
    case DFmode:
      if (pnum_clobbers != 0 && register_operand (x1, DFmode))
	{
	  ro[0] = x1;
	  if (TARGET_80387 && ! TARGET_IEEE_FP)
	    {
	      *pnum_clobbers = 1;
	      return 8;
	    }
	  }
      break;
    case XFmode:
      if (pnum_clobbers != 0 && register_operand (x1, XFmode))
	{
	  ro[0] = x1;
	  if (TARGET_80387 && ! TARGET_IEEE_FP)
	    {
	      *pnum_clobbers = 1;
	      return 10;
	    }
	  }
    }
  switch (GET_CODE (x1))
    {
    case COMPARE:
      goto L39;
    case ZERO_EXTRACT:
      goto L1046;
    }
  L61:
  if (VOIDmode_compare_op (x1, VOIDmode))
    {
      ro[2] = x1;
      goto L91;
    }
  L134:
  switch (GET_MODE (x1))
    {
    case CCFPEQmode:
      switch (GET_CODE (x1))
	{
	case COMPARE:
	  goto L135;
	}
      break;
    case SImode:
      switch (GET_CODE (x1))
	{
	case AND:
	  goto L282;
	}
      break;
    case HImode:
      switch (GET_CODE (x1))
	{
	case AND:
	  goto L287;
	}
      break;
    case QImode:
      if (GET_CODE (x1) == AND && 1)
	goto L292;
    }
  goto ret0;

  L39:
  x2 = XEXP (x1, 0);
  switch (GET_MODE (x2))
    {
    case SImode:
      if (nonimmediate_operand (x2, SImode))
	{
	  ro[0] = x2;
	  goto L40;
	}
      break;
    case HImode:
      if (nonimmediate_operand (x2, HImode))
	{
	  ro[0] = x2;
	  goto L45;
	}
      break;
    case QImode:
      if (nonimmediate_operand (x2, QImode))
	{
	  ro[0] = x2;
	  goto L50;
	}
    }
  goto L61;

  L40:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, SImode))
    {
      ro[1] = x2;
      if (GET_CODE (operands[0]) != MEM || GET_CODE (operands[1]) != MEM)
	return 12;
      }
  goto L61;

  L45:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, HImode))
    {
      ro[1] = x2;
      if (GET_CODE (operands[0]) != MEM || GET_CODE (operands[1]) != MEM)
	return 14;
      }
  goto L61;

  L50:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, QImode))
    {
      ro[1] = x2;
      if (GET_CODE (operands[0]) != MEM || GET_CODE (operands[1]) != MEM)
	return 16;
      }
  goto L61;

  L1046:
  x2 = XEXP (x1, 0);
  switch (GET_MODE (x2))
    {
    case SImode:
      if (register_operand (x2, SImode))
	{
	  ro[0] = x2;
	  goto L1047;
	}
      break;
    case QImode:
      if (general_operand (x2, QImode))
	{
	  ro[0] = x2;
	  goto L1059;
	}
    }
  goto L61;

  L1047:
  x2 = XEXP (x1, 1);
  if (GET_CODE (x2) != CONST_INT)
    {
    goto L61;
    }
  if (XWINT (x2, 0) == 1 && 1)
    goto L1048;
  L1053:
  ro[1] = x2;
  goto L1054;

  L1048:
  x2 = XEXP (x1, 2);
  if (general_operand (x2, SImode))
    {
      ro[1] = x2;
      if (GET_CODE (operands[1]) != CONST_INT)
	return 208;
      }
  x2 = XEXP (x1, 1);
  goto L1053;

  L1054:
  x2 = XEXP (x1, 2);
  if (GET_CODE (x2) == CONST_INT && 1)
    {
      ro[2] = x2;
      return 209;
    }
  goto L61;

  L1059:
  x2 = XEXP (x1, 1);
  if (GET_CODE (x2) == CONST_INT && 1)
    {
      ro[1] = x2;
      goto L1060;
    }
  goto L61;

  L1060:
  x2 = XEXP (x1, 2);
  if (GET_CODE (x2) == CONST_INT && 1)
    {
      ro[2] = x2;
      if (GET_CODE (operands[0]) != MEM || ! MEM_VOLATILE_P (operands[0]))
	return 210;
      }
  goto L61;

  L91:
  x2 = XEXP (x1, 0);
  switch (GET_MODE (x2))
    {
    case XFmode:
      if (GET_CODE (x2) == FLOAT && 1)
	goto L92;
      if (nonimmediate_operand (x2, XFmode))
	{
	  ro[0] = x2;
	  goto L63;
	}
    L76:
      if (register_operand (x2, XFmode))
	{
	  ro[0] = x2;
	  goto L77;
	}
      break;
    case DFmode:
      switch (GET_CODE (x2))
	{
	case FLOAT:
	  goto L178;
	case FLOAT_EXTEND:
	  goto L208;
	case SUBREG:
	case REG:
	case MEM:
	  if (nonimmediate_operand (x2, DFmode))
	    {
	      ro[0] = x2;
	      goto L149;
	    }
	}
    L162:
      if (register_operand (x2, DFmode))
	{
	  ro[0] = x2;
	  goto L163;
	}
      break;
    case SFmode:
      if (GET_CODE (x2) == FLOAT && 1)
	goto L264;
      if (nonimmediate_operand (x2, SFmode))
	{
	  ro[0] = x2;
	  goto L235;
	}
    L248:
      if (register_operand (x2, SFmode))
	{
	  ro[0] = x2;
	  goto L249;
	}
    }
  goto L134;

  L92:
  x3 = XEXP (x2, 0);
  if (nonimmediate_operand (x3, SImode))
    {
      ro[0] = x3;
      goto L93;
    }
  goto L134;

  L93:
  x2 = XEXP (x1, 1);
  if (pnum_clobbers != 0 && register_operand (x2, XFmode))
    {
      ro[1] = x2;
      if (TARGET_80387)
	{
	  *pnum_clobbers = 1;
	  return 20;
	}
      }
  goto L134;

  L63:
  x2 = XEXP (x1, 1);
  if (pnum_clobbers != 0 && nonimmediate_operand (x2, XFmode))
    {
      ro[1] = x2;
      if (TARGET_80387
   && (GET_CODE (operands[0]) != MEM || GET_CODE (operands[1]) != MEM))
	{
	  *pnum_clobbers = 1;
	  return 18;
	}
      }
  x2 = XEXP (x1, 0);
  goto L76;

  L77:
  x2 = XEXP (x1, 1);
  if (GET_MODE (x2) != XFmode)
    {
      goto L134;
    }
  switch (GET_CODE (x2))
    {
    case FLOAT:
      goto L78;
    case FLOAT_EXTEND:
      goto L108;
    }
  goto L134;

  L78:
  x3 = XEXP (x2, 0);
  if (pnum_clobbers != 0 && nonimmediate_operand (x3, SImode))
    {
      ro[1] = x3;
      if (TARGET_80387)
	{
	  *pnum_clobbers = 1;
	  return 19;
	}
      }
  goto L134;

  L108:
  x3 = XEXP (x2, 0);
  switch (GET_MODE (x3))
    {
    case DFmode:
      if (pnum_clobbers != 0 && nonimmediate_operand (x3, DFmode))
	{
	  ro[1] = x3;
	  if (TARGET_80387)
	    {
	      *pnum_clobbers = 1;
	      return 21;
	    }
	  }
      break;
    case SFmode:
      if (pnum_clobbers != 0 && nonimmediate_operand (x3, SFmode))
	{
	  ro[1] = x3;
	  if (TARGET_80387)
	    {
	      *pnum_clobbers = 1;
	      return 22;
	    }
	  }
    }
  goto L134;

  L178:
  x3 = XEXP (x2, 0);
  if (nonimmediate_operand (x3, SImode))
    {
      ro[0] = x3;
      goto L179;
    }
  goto L134;

  L179:
  x2 = XEXP (x1, 1);
  if (pnum_clobbers != 0 && register_operand (x2, DFmode))
    {
      ro[1] = x2;
      if (TARGET_80387)
	{
	  *pnum_clobbers = 1;
	  return 26;
	}
      }
  goto L134;

  L208:
  x3 = XEXP (x2, 0);
  if (nonimmediate_operand (x3, SFmode))
    {
      ro[0] = x3;
      goto L209;
    }
  goto L134;

  L209:
  x2 = XEXP (x1, 1);
  if (pnum_clobbers != 0 && register_operand (x2, DFmode))
    {
      ro[1] = x2;
      if (TARGET_80387)
	{
	  *pnum_clobbers = 1;
	  return 28;
	}
      }
  goto L134;

  L149:
  x2 = XEXP (x1, 1);
  if (pnum_clobbers != 0 && nonimmediate_operand (x2, DFmode))
    {
      ro[1] = x2;
      if (TARGET_80387
   && (GET_CODE (operands[0]) != MEM || GET_CODE (operands[1]) != MEM))
	{
	  *pnum_clobbers = 1;
	  return 24;
	}
      }
  x2 = XEXP (x1, 0);
  goto L162;

  L163:
  x2 = XEXP (x1, 1);
  if (GET_MODE (x2) != DFmode)
    {
      goto L134;
    }
  switch (GET_CODE (x2))
    {
    case FLOAT:
      goto L164;
    case FLOAT_EXTEND:
      goto L194;
    }
  goto L134;

  L164:
  x3 = XEXP (x2, 0);
  if (pnum_clobbers != 0 && nonimmediate_operand (x3, SImode))
    {
      ro[1] = x3;
      if (TARGET_80387)
	{
	  *pnum_clobbers = 1;
	  return 25;
	}
      }
  goto L134;

  L194:
  x3 = XEXP (x2, 0);
  if (pnum_clobbers != 0 && nonimmediate_operand (x3, SFmode))
    {
      ro[1] = x3;
      if (TARGET_80387)
	{
	  *pnum_clobbers = 1;
	  return 27;
	}
      }
  goto L134;

  L264:
  x3 = XEXP (x2, 0);
  if (nonimmediate_operand (x3, SImode))
    {
      ro[0] = x3;
      goto L265;
    }
  goto L134;

  L265:
  x2 = XEXP (x1, 1);
  if (pnum_clobbers != 0 && register_operand (x2, SFmode))
    {
      ro[1] = x2;
      if (TARGET_80387)
	{
	  *pnum_clobbers = 1;
	  return 32;
	}
      }
  goto L134;

  L235:
  x2 = XEXP (x1, 1);
  if (pnum_clobbers != 0 && nonimmediate_operand (x2, SFmode))
    {
      ro[1] = x2;
      if (TARGET_80387
   && (GET_CODE (operands[0]) != MEM || GET_CODE (operands[1]) != MEM))
	{
	  *pnum_clobbers = 1;
	  return 30;
	}
      }
  x2 = XEXP (x1, 0);
  goto L248;

  L249:
  x2 = XEXP (x1, 1);
  if (GET_MODE (x2) == SFmode && GET_CODE (x2) == FLOAT && 1)
    goto L250;
  goto L134;

  L250:
  x3 = XEXP (x2, 0);
  if (pnum_clobbers != 0 && nonimmediate_operand (x3, SImode))
    {
      ro[1] = x3;
      if (TARGET_80387)
	{
	  *pnum_clobbers = 1;
	  return 31;
	}
      }
  goto L134;

  L135:
  x2 = XEXP (x1, 0);
  switch (GET_MODE (x2))
    {
    case XFmode:
      if (register_operand (x2, XFmode))
	{
	  ro[0] = x2;
	  goto L136;
	}
      break;
    case DFmode:
      if (register_operand (x2, DFmode))
	{
	  ro[0] = x2;
	  goto L222;
	}
      break;
    case SFmode:
      if (register_operand (x2, SFmode))
	{
	  ro[0] = x2;
	  goto L278;
	}
    }
  goto ret0;

  L136:
  x2 = XEXP (x1, 1);
  if (pnum_clobbers != 0 && register_operand (x2, XFmode))
    {
      ro[1] = x2;
      if (TARGET_80387)
	{
	  *pnum_clobbers = 1;
	  return 23;
	}
      }
  goto ret0;

  L222:
  x2 = XEXP (x1, 1);
  if (pnum_clobbers != 0 && register_operand (x2, DFmode))
    {
      ro[1] = x2;
      if (TARGET_80387)
	{
	  *pnum_clobbers = 1;
	  return 29;
	}
      }
  goto ret0;

  L278:
  x2 = XEXP (x1, 1);
  if (pnum_clobbers != 0 && register_operand (x2, SFmode))
    {
      ro[1] = x2;
      if (TARGET_80387)
	{
	  *pnum_clobbers = 1;
	  return 33;
	}
      }
  goto ret0;

  L282:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, SImode))
    {
      ro[0] = x2;
      goto L283;
    }
  goto ret0;

  L283:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, SImode))
    {
      ro[1] = x2;
      return 43;
    }
  goto ret0;

  L287:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, HImode))
    {
      ro[0] = x2;
      goto L288;
    }
  goto ret0;

  L288:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, HImode))
    {
      ro[1] = x2;
      return 44;
    }
  goto ret0;

  L292:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, QImode))
    {
      ro[0] = x2;
      goto L293;
    }
  goto ret0;

  L293:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, QImode))
    {
      ro[1] = x2;
      return 45;
    }
  goto ret0;
 ret0: return -1;
}

int
recog_3 (x0, insn, pnum_clobbers)
     register rtx x0;
     rtx insn;
     int *pnum_clobbers;
{
  register rtx *ro = &recog_operand[0];
  register rtx x1, x2, x3, x4, x5, x6;
  int tem;

  x1 = XEXP (x0, 1);
  x2 = XEXP (x1, 0);
  switch (GET_CODE (x2))
    {
    case EQ:
      goto L1115;
    case NE:
      goto L1124;
    case GT:
      goto L1133;
    case GTU:
      goto L1142;
    case LT:
      goto L1151;
    case LTU:
      goto L1160;
    case GE:
      goto L1169;
    case GEU:
      goto L1178;
    case LE:
      goto L1187;
    case LEU:
      goto L1196;
    }
  goto ret0;

  L1115:
  x3 = XEXP (x2, 0);
  if (GET_CODE (x3) == CC0 && 1)
    goto L1116;
  goto ret0;

  L1116:
  x3 = XEXP (x2, 1);
  if (GET_CODE (x3) == CONST_INT && XWINT (x3, 0) == 0 && 1)
    goto L1117;
  goto ret0;

  L1117:
  x2 = XEXP (x1, 1);
  switch (GET_CODE (x2))
    {
    case LABEL_REF:
      goto L1118;
    case PC:
      goto L1208;
    }
  goto ret0;

  L1118:
  x3 = XEXP (x2, 0);
  ro[0] = x3;
  goto L1119;

  L1119:
  x2 = XEXP (x1, 2);
  if (GET_CODE (x2) == PC && 1)
    return 232;
  goto ret0;

  L1208:
  x2 = XEXP (x1, 2);
  if (GET_CODE (x2) == LABEL_REF && 1)
    goto L1209;
  goto ret0;

  L1209:
  x3 = XEXP (x2, 0);
  ro[0] = x3;
  return 251;

  L1124:
  x3 = XEXP (x2, 0);
  if (GET_CODE (x3) == CC0 && 1)
    goto L1125;
  goto ret0;

  L1125:
  x3 = XEXP (x2, 1);
  if (GET_CODE (x3) == CONST_INT && XWINT (x3, 0) == 0 && 1)
    goto L1126;
  goto ret0;

  L1126:
  x2 = XEXP (x1, 1);
  switch (GET_CODE (x2))
    {
    case LABEL_REF:
      goto L1127;
    case PC:
      goto L1217;
    }
  goto ret0;

  L1127:
  x3 = XEXP (x2, 0);
  ro[0] = x3;
  goto L1128;

  L1128:
  x2 = XEXP (x1, 2);
  if (GET_CODE (x2) == PC && 1)
    return 234;
  goto ret0;

  L1217:
  x2 = XEXP (x1, 2);
  if (GET_CODE (x2) == LABEL_REF && 1)
    goto L1218;
  goto ret0;

  L1218:
  x3 = XEXP (x2, 0);
  ro[0] = x3;
  return 252;

  L1133:
  x3 = XEXP (x2, 0);
  if (GET_CODE (x3) == CC0 && 1)
    goto L1134;
  goto ret0;

  L1134:
  x3 = XEXP (x2, 1);
  if (GET_CODE (x3) == CONST_INT && XWINT (x3, 0) == 0 && 1)
    goto L1135;
  goto ret0;

  L1135:
  x2 = XEXP (x1, 1);
  switch (GET_CODE (x2))
    {
    case LABEL_REF:
      goto L1136;
    case PC:
      goto L1226;
    }
  goto ret0;

  L1136:
  x3 = XEXP (x2, 0);
  ro[0] = x3;
  goto L1137;

  L1137:
  x2 = XEXP (x1, 2);
  if (GET_CODE (x2) == PC && 1)
    return 236;
  goto ret0;

  L1226:
  x2 = XEXP (x1, 2);
  if (GET_CODE (x2) == LABEL_REF && 1)
    goto L1227;
  goto ret0;

  L1227:
  x3 = XEXP (x2, 0);
  ro[0] = x3;
  return 253;

  L1142:
  x3 = XEXP (x2, 0);
  if (GET_CODE (x3) == CC0 && 1)
    goto L1143;
  goto ret0;

  L1143:
  x3 = XEXP (x2, 1);
  if (GET_CODE (x3) == CONST_INT && XWINT (x3, 0) == 0 && 1)
    goto L1144;
  goto ret0;

  L1144:
  x2 = XEXP (x1, 1);
  switch (GET_CODE (x2))
    {
    case LABEL_REF:
      goto L1145;
    case PC:
      goto L1235;
    }
  goto ret0;

  L1145:
  x3 = XEXP (x2, 0);
  ro[0] = x3;
  goto L1146;

  L1146:
  x2 = XEXP (x1, 2);
  if (GET_CODE (x2) == PC && 1)
    return 238;
  goto ret0;

  L1235:
  x2 = XEXP (x1, 2);
  if (GET_CODE (x2) == LABEL_REF && 1)
    goto L1236;
  goto ret0;

  L1236:
  x3 = XEXP (x2, 0);
  ro[0] = x3;
  return 254;

  L1151:
  x3 = XEXP (x2, 0);
  if (GET_CODE (x3) == CC0 && 1)
    goto L1152;
  goto ret0;

  L1152:
  x3 = XEXP (x2, 1);
  if (GET_CODE (x3) == CONST_INT && XWINT (x3, 0) == 0 && 1)
    goto L1153;
  goto ret0;

  L1153:
  x2 = XEXP (x1, 1);
  switch (GET_CODE (x2))
    {
    case LABEL_REF:
      goto L1154;
    case PC:
      goto L1244;
    }
  goto ret0;

  L1154:
  x3 = XEXP (x2, 0);
  ro[0] = x3;
  goto L1155;

  L1155:
  x2 = XEXP (x1, 2);
  if (GET_CODE (x2) == PC && 1)
    return 240;
  goto ret0;

  L1244:
  x2 = XEXP (x1, 2);
  if (GET_CODE (x2) == LABEL_REF && 1)
    goto L1245;
  goto ret0;

  L1245:
  x3 = XEXP (x2, 0);
  ro[0] = x3;
  return 255;

  L1160:
  x3 = XEXP (x2, 0);
  if (GET_CODE (x3) == CC0 && 1)
    goto L1161;
  goto ret0;

  L1161:
  x3 = XEXP (x2, 1);
  if (GET_CODE (x3) == CONST_INT && XWINT (x3, 0) == 0 && 1)
    goto L1162;
  goto ret0;

  L1162:
  x2 = XEXP (x1, 1);
  switch (GET_CODE (x2))
    {
    case LABEL_REF:
      goto L1163;
    case PC:
      goto L1253;
    }
  goto ret0;

  L1163:
  x3 = XEXP (x2, 0);
  ro[0] = x3;
  goto L1164;

  L1164:
  x2 = XEXP (x1, 2);
  if (GET_CODE (x2) == PC && 1)
    return 242;
  goto ret0;

  L1253:
  x2 = XEXP (x1, 2);
  if (GET_CODE (x2) == LABEL_REF && 1)
    goto L1254;
  goto ret0;

  L1254:
  x3 = XEXP (x2, 0);
  ro[0] = x3;
  return 256;

  L1169:
  x3 = XEXP (x2, 0);
  if (GET_CODE (x3) == CC0 && 1)
    goto L1170;
  goto ret0;

  L1170:
  x3 = XEXP (x2, 1);
  if (GET_CODE (x3) == CONST_INT && XWINT (x3, 0) == 0 && 1)
    goto L1171;
  goto ret0;

  L1171:
  x2 = XEXP (x1, 1);
  switch (GET_CODE (x2))
    {
    case LABEL_REF:
      goto L1172;
    case PC:
      goto L1262;
    }
  goto ret0;

  L1172:
  x3 = XEXP (x2, 0);
  ro[0] = x3;
  goto L1173;

  L1173:
  x2 = XEXP (x1, 2);
  if (GET_CODE (x2) == PC && 1)
    return 244;
  goto ret0;

  L1262:
  x2 = XEXP (x1, 2);
  if (GET_CODE (x2) == LABEL_REF && 1)
    goto L1263;
  goto ret0;

  L1263:
  x3 = XEXP (x2, 0);
  ro[0] = x3;
  return 257;

  L1178:
  x3 = XEXP (x2, 0);
  if (GET_CODE (x3) == CC0 && 1)
    goto L1179;
  goto ret0;

  L1179:
  x3 = XEXP (x2, 1);
  if (GET_CODE (x3) == CONST_INT && XWINT (x3, 0) == 0 && 1)
    goto L1180;
  goto ret0;

  L1180:
  x2 = XEXP (x1, 1);
  switch (GET_CODE (x2))
    {
    case LABEL_REF:
      goto L1181;
    case PC:
      goto L1271;
    }
  goto ret0;

  L1181:
  x3 = XEXP (x2, 0);
  ro[0] = x3;
  goto L1182;

  L1182:
  x2 = XEXP (x1, 2);
  if (GET_CODE (x2) == PC && 1)
    return 246;
  goto ret0;

  L1271:
  x2 = XEXP (x1, 2);
  if (GET_CODE (x2) == LABEL_REF && 1)
    goto L1272;
  goto ret0;

  L1272:
  x3 = XEXP (x2, 0);
  ro[0] = x3;
  return 258;

  L1187:
  x3 = XEXP (x2, 0);
  if (GET_CODE (x3) == CC0 && 1)
    goto L1188;
  goto ret0;

  L1188:
  x3 = XEXP (x2, 1);
  if (GET_CODE (x3) == CONST_INT && XWINT (x3, 0) == 0 && 1)
    goto L1189;
  goto ret0;

  L1189:
  x2 = XEXP (x1, 1);
  switch (GET_CODE (x2))
    {
    case LABEL_REF:
      goto L1190;
    case PC:
      goto L1280;
    }
  goto ret0;

  L1190:
  x3 = XEXP (x2, 0);
  ro[0] = x3;
  goto L1191;

  L1191:
  x2 = XEXP (x1, 2);
  if (GET_CODE (x2) == PC && 1)
    return 248;
  goto ret0;

  L1280:
  x2 = XEXP (x1, 2);
  if (GET_CODE (x2) == LABEL_REF && 1)
    goto L1281;
  goto ret0;

  L1281:
  x3 = XEXP (x2, 0);
  ro[0] = x3;
  return 259;

  L1196:
  x3 = XEXP (x2, 0);
  if (GET_CODE (x3) == CC0 && 1)
    goto L1197;
  goto ret0;

  L1197:
  x3 = XEXP (x2, 1);
  if (GET_CODE (x3) == CONST_INT && XWINT (x3, 0) == 0 && 1)
    goto L1198;
  goto ret0;

  L1198:
  x2 = XEXP (x1, 1);
  switch (GET_CODE (x2))
    {
    case LABEL_REF:
      goto L1199;
    case PC:
      goto L1289;
    }
  goto ret0;

  L1199:
  x3 = XEXP (x2, 0);
  ro[0] = x3;
  goto L1200;

  L1200:
  x2 = XEXP (x1, 2);
  if (GET_CODE (x2) == PC && 1)
    return 250;
  goto ret0;

  L1289:
  x2 = XEXP (x1, 2);
  if (GET_CODE (x2) == LABEL_REF && 1)
    goto L1290;
  goto ret0;

  L1290:
  x3 = XEXP (x2, 0);
  ro[0] = x3;
  return 260;
 ret0: return -1;
}

int
recog_4 (x0, insn, pnum_clobbers)
     register rtx x0;
     rtx insn;
     int *pnum_clobbers;
{
  register rtx *ro = &recog_operand[0];
  register rtx x1, x2, x3, x4, x5, x6;
  int tem;

  x1 = XEXP (x0, 0);
  switch (GET_MODE (x1))
    {
    case SImode:
      switch (GET_CODE (x1))
	{
	case MEM:
	  if (push_operand (x1, SImode))
	    {
	      ro[0] = x1;
	      goto L296;
	    }
	  break;
	case ZERO_EXTRACT:
	  goto L1025;
	}
    L303:
      if (general_operand (x1, SImode))
	{
	  ro[0] = x1;
	  goto L365;
	}
    L611:
      if (register_operand (x1, SImode))
	{
	  ro[0] = x1;
	  goto L612;
	}
    L619:
      if (general_operand (x1, SImode))
	{
	  ro[0] = x1;
	  goto L620;
	}
      break;
    case HImode:
      if (GET_CODE (x1) == MEM && push_operand (x1, HImode))
	{
	  ro[0] = x1;
	  goto L307;
	}
    L309:
      if (general_operand (x1, HImode))
	{
	  ro[0] = x1;
	  goto L369;
	}
      break;
    case QImode:
      if (GET_CODE (x1) == MEM && push_operand (x1, QImode))
	{
	  ro[0] = x1;
	  goto L317;
	}
    L319:
      if (general_operand (x1, QImode))
	{
	  ro[0] = x1;
	  goto L607;
	}
    L1062:
      if (register_operand (x1, QImode))
	{
	  ro[0] = x1;
	  goto L1063;
	}
      break;
    case SFmode:
      if (GET_CODE (x1) == MEM && push_operand (x1, SFmode))
	{
	  ro[0] = x1;
	  goto L327;
	}
    L329:
      if (general_operand (x1, SFmode))
	{
	  ro[0] = x1;
	  goto L416;
	}
    L575:
      if (register_operand (x1, SFmode))
	{
	  ro[0] = x1;
	  goto L576;
	}
      break;
    case DFmode:
      if (GET_CODE (x1) == MEM && push_operand (x1, DFmode))
	{
	  ro[0] = x1;
	  goto L333;
	}
    L342:
      if (general_operand (x1, DFmode))
	{
	  ro[0] = x1;
	  goto L397;
	}
    L571:
      if (register_operand (x1, DFmode))
	{
	  ro[0] = x1;
	  goto L572;
	}
      break;
    case XFmode:
      if (GET_CODE (x1) == MEM && push_operand (x1, XFmode))
	{
	  ro[0] = x1;
	  goto L346;
	}
    L355:
      if (general_operand (x1, XFmode))
	{
	  ro[0] = x1;
	  goto L401;
	}
    L567:
      if (register_operand (x1, XFmode))
	{
	  ro[0] = x1;
	  goto L568;
	}
      break;
    case DImode:
      if (GET_CODE (x1) == MEM && push_operand (x1, DImode))
	{
	  ro[0] = x1;
	  goto L359;
	}
    L361:
      if (general_operand (x1, DImode))
	{
	  ro[0] = x1;
	  goto L592;
	}
    L376:
      if (register_operand (x1, DImode))
	{
	  ro[0] = x1;
	  goto L377;
	}
    }
  switch (GET_CODE (x1))
    {
    case CC0:
      goto L2;
    case STRICT_LOW_PART:
      goto L313;
    case PC:
      goto L1314;
    }
  L1380:
  ro[0] = x1;
  goto L1381;
  L1460:
  switch (GET_MODE (x1))
    {
    case SImode:
      if (general_operand (x1, SImode))
	{
	  ro[0] = x1;
	  goto L1461;
	}
      break;
    case HImode:
      if (general_operand (x1, HImode))
	{
	  ro[0] = x1;
	  goto L1467;
	}
      break;
    case DFmode:
      if (register_operand (x1, DFmode))
	{
	  ro[0] = x1;
	  goto L1473;
	}
      break;
    case XFmode:
      if (register_operand (x1, XFmode))
	{
	  ro[0] = x1;
	  goto L1484;
	}
      break;
    case SFmode:
      if (register_operand (x1, SFmode))
	{
	  ro[0] = x1;
	  goto L1531;
	}
    }
  goto ret0;

  L296:
  x1 = XEXP (x0, 1);
  if (general_operand (x1, SImode))
    goto L300;
  x1 = XEXP (x0, 0);
  goto L303;

  L300:
  ro[1] = x1;
  if (! TARGET_486)
    return 46;
  L301:
  ro[1] = x1;
  if (TARGET_486)
    return 47;
  x1 = XEXP (x0, 0);
  goto L303;

  L1025:
  x2 = XEXP (x1, 0);
  if (GET_MODE (x2) == SImode && general_operand (x2, SImode))
    {
      ro[0] = x2;
      goto L1026;
    }
  goto L1380;

  L1026:
  x2 = XEXP (x1, 1);
  if (GET_CODE (x2) == CONST_INT && XWINT (x2, 0) == 1 && 1)
    goto L1027;
  goto L1380;

  L1027:
  x2 = XEXP (x1, 2);
  if (general_operand (x2, SImode))
    {
      ro[2] = x2;
      goto L1028;
    }
  goto L1380;

  L1028:
  x1 = XEXP (x0, 1);
  if (GET_CODE (x1) == CONST_INT && 1)
    {
      ro[3] = x1;
      if (! TARGET_486 && GET_CODE (operands[2]) != CONST_INT)
	return 205;
      }
  x1 = XEXP (x0, 0);
  goto L1380;

  L365:
  x1 = XEXP (x0, 1);
  switch (GET_MODE (x1))
    {
    case SImode:
      switch (GET_CODE (x1))
	{
	case ZERO_EXTEND:
	  goto L366;
	case SIGN_EXTEND:
	  goto L386;
	case PLUS:
	  goto L598;
	}
    }
  if (general_operand (x1, SImode))
    {
      ro[1] = x1;
      return 49;
    }
  x1 = XEXP (x0, 0);
  goto L611;

  L366:
  x2 = XEXP (x1, 0);
  switch (GET_MODE (x2))
    {
    case HImode:
      if (nonimmediate_operand (x2, HImode))
	{
	  ro[1] = x2;
	  return 66;
	}
      break;
    case QImode:
      if (nonimmediate_operand (x2, QImode))
	{
	  ro[1] = x2;
	  return 68;
	}
    }
  x1 = XEXP (x0, 0);
  goto L611;

  L386:
  x2 = XEXP (x1, 0);
  switch (GET_MODE (x2))
    {
    case HImode:
      if (nonimmediate_operand (x2, HImode))
	{
	  ro[1] = x2;
	  return 71;
	}
      break;
    case QImode:
      if (nonimmediate_operand (x2, QImode))
	{
	  ro[1] = x2;
	  return 73;
	}
    }
  x1 = XEXP (x0, 0);
  goto L611;

  L598:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, SImode))
    {
      ro[1] = x2;
      goto L599;
    }
  x1 = XEXP (x0, 0);
  goto L611;

  L599:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, SImode))
    {
      ro[2] = x2;
      return 109;
    }
  x1 = XEXP (x0, 0);
  goto L611;

  L612:
  x1 = XEXP (x0, 1);
  if (address_operand (x1, QImode))
    {
      ro[1] = x1;
      return 112;
    }
  x1 = XEXP (x0, 0);
  goto L619;

  L620:
  x1 = XEXP (x0, 1);
  if (GET_MODE (x1) != SImode)
    {
      x1 = XEXP (x0, 0);
      goto L1380;
    }
  switch (GET_CODE (x1))
    {
    case MINUS:
      goto L621;
    case MULT:
      goto L648;
    case AND:
      goto L742;
    case IOR:
      goto L757;
    case XOR:
      goto L1032;
    case NEG:
      goto L791;
    case NOT:
      goto L900;
    case ASHIFT:
      goto L925;
    case ASHIFTRT:
      goto L953;
    case LSHIFTRT:
      goto L981;
    case ROTATE:
      goto L996;
    case ROTATERT:
      goto L1011;
    }
  x1 = XEXP (x0, 0);
  goto L1380;

  L621:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, SImode))
    {
      ro[1] = x2;
      goto L622;
    }
  x1 = XEXP (x0, 0);
  goto L1380;

  L622:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, SImode))
    {
      ro[2] = x2;
      return 117;
    }
  x1 = XEXP (x0, 0);
  goto L1380;

  L648:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, SImode))
    {
      ro[1] = x2;
      goto L649;
    }
  x1 = XEXP (x0, 0);
  goto L1380;

  L649:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, SImode))
    goto L655;
  x1 = XEXP (x0, 0);
  goto L1380;

  L655:
  ro[2] = x2;
  if (GET_CODE (operands[2]) == CONST_INT && INTVAL (operands[2]) == 0x80)
    return 125;
  L656:
  ro[2] = x2;
  return 126;

  L742:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, SImode))
    {
      ro[1] = x2;
      goto L743;
    }
  x1 = XEXP (x0, 0);
  goto L1380;

  L743:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, SImode))
    {
      ro[2] = x2;
      return 143;
    }
  x1 = XEXP (x0, 0);
  goto L1380;

  L757:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, SImode))
    {
      ro[1] = x2;
      goto L758;
    }
  x1 = XEXP (x0, 0);
  goto L1380;

  L758:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, SImode))
    {
      ro[2] = x2;
      return 146;
    }
  x1 = XEXP (x0, 0);
  goto L1380;

  L1032:
  x2 = XEXP (x1, 0);
  if (GET_MODE (x2) == SImode && GET_CODE (x2) == ASHIFT && 1)
    goto L1033;
  if (general_operand (x2, SImode))
    {
      ro[1] = x2;
      goto L1040;
    }
  x1 = XEXP (x0, 0);
  goto L1380;

  L1033:
  x3 = XEXP (x2, 0);
  if (GET_CODE (x3) == CONST_INT && XWINT (x3, 0) == 1 && 1)
    goto L1034;
  x1 = XEXP (x0, 0);
  goto L1380;

  L1034:
  x3 = XEXP (x2, 1);
  if (general_operand (x3, SImode))
    {
      ro[1] = x3;
      goto L1035;
    }
  x1 = XEXP (x0, 0);
  goto L1380;

  L1035:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, SImode))
    {
      ro[2] = x2;
      if (! TARGET_486 && GET_CODE (operands[1]) != CONST_INT)
	return 206;
      }
  x1 = XEXP (x0, 0);
  goto L1380;

  L1040:
  x2 = XEXP (x1, 1);
  if (GET_MODE (x2) == SImode && GET_CODE (x2) == ASHIFT && 1)
    goto L1041;
  if (general_operand (x2, SImode))
    {
      ro[2] = x2;
      return 149;
    }
  x1 = XEXP (x0, 0);
  goto L1380;

  L1041:
  x3 = XEXP (x2, 0);
  if (GET_CODE (x3) == CONST_INT && XWINT (x3, 0) == 1 && 1)
    goto L1042;
  x1 = XEXP (x0, 0);
  goto L1380;

  L1042:
  x3 = XEXP (x2, 1);
  if (general_operand (x3, SImode))
    {
      ro[2] = x3;
      if (! TARGET_486 && GET_CODE (operands[2]) != CONST_INT)
	return 207;
      }
  x1 = XEXP (x0, 0);
  goto L1380;

  L791:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, SImode))
    {
      ro[1] = x2;
      return 153;
    }
  x1 = XEXP (x0, 0);
  goto L1380;

  L900:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, SImode))
    {
      ro[1] = x2;
      return 178;
    }
  x1 = XEXP (x0, 0);
  goto L1380;

  L925:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, SImode))
    {
      ro[1] = x2;
      goto L926;
    }
  x1 = XEXP (x0, 0);
  goto L1380;

  L926:
  x2 = XEXP (x1, 1);
  if (nonmemory_operand (x2, SImode))
    {
      ro[2] = x2;
      return 184;
    }
  x1 = XEXP (x0, 0);
  goto L1380;

  L953:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, SImode))
    {
      ro[1] = x2;
      goto L954;
    }
  x1 = XEXP (x0, 0);
  goto L1380;

  L954:
  x2 = XEXP (x1, 1);
  if (nonmemory_operand (x2, SImode))
    {
      ro[2] = x2;
      return 190;
    }
  x1 = XEXP (x0, 0);
  goto L1380;

  L981:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, SImode))
    {
      ro[1] = x2;
      goto L982;
    }
  x1 = XEXP (x0, 0);
  goto L1380;

  L982:
  x2 = XEXP (x1, 1);
  if (nonmemory_operand (x2, SImode))
    {
      ro[2] = x2;
      return 196;
    }
  x1 = XEXP (x0, 0);
  goto L1380;

  L996:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, SImode))
    {
      ro[1] = x2;
      goto L997;
    }
  x1 = XEXP (x0, 0);
  goto L1380;

  L997:
  x2 = XEXP (x1, 1);
  if (nonmemory_operand (x2, SImode))
    {
      ro[2] = x2;
      return 199;
    }
  x1 = XEXP (x0, 0);
  goto L1380;

  L1011:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, SImode))
    {
      ro[1] = x2;
      goto L1012;
    }
  x1 = XEXP (x0, 0);
  goto L1380;

  L1012:
  x2 = XEXP (x1, 1);
  if (nonmemory_operand (x2, SImode))
    {
      ro[2] = x2;
      return 202;
    }
  x1 = XEXP (x0, 0);
  goto L1380;

  L307:
  x1 = XEXP (x0, 1);
  if (general_operand (x1, HImode))
    {
      ro[1] = x1;
      return 50;
    }
  x1 = XEXP (x0, 0);
  goto L309;
 L369:
  tem = recog_1 (x0, insn, pnum_clobbers);
  if (tem >= 0) return tem;
  x1 = XEXP (x0, 0);
  goto L1380;

  L317:
  x1 = XEXP (x0, 1);
  if (general_operand (x1, QImode))
    {
      ro[1] = x1;
      return 53;
    }
  x1 = XEXP (x0, 0);
  goto L319;

  L607:
  x1 = XEXP (x0, 1);
  switch (GET_MODE (x1))
    {
    case QImode:
      switch (GET_CODE (x1))
	{
	case PLUS:
	  goto L608;
	case MINUS:
	  goto L631;
	case DIV:
	  goto L688;
	case UDIV:
	  goto L693;
	case AND:
	  goto L752;
	case IOR:
	  goto L767;
	case XOR:
	  goto L782;
	case NEG:
	  goto L799;
	case NOT:
	  goto L908;
	case ASHIFT:
	  goto L935;
	case ASHIFTRT:
	  goto L963;
	case LSHIFTRT:
	  goto L991;
	case ROTATE:
	  goto L1006;
	case ROTATERT:
	  goto L1021;
	}
    }
  if (general_operand (x1, QImode))
    {
      ro[1] = x1;
      return 54;
    }
  x1 = XEXP (x0, 0);
  goto L1062;

  L608:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, QImode))
    {
      ro[1] = x2;
      goto L609;
    }
  x1 = XEXP (x0, 0);
  goto L1062;

  L609:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, QImode))
    {
      ro[2] = x2;
      return 111;
    }
  x1 = XEXP (x0, 0);
  goto L1062;

  L631:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, QImode))
    {
      ro[1] = x2;
      goto L632;
    }
  x1 = XEXP (x0, 0);
  goto L1062;

  L632:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, QImode))
    {
      ro[2] = x2;
      return 119;
    }
  x1 = XEXP (x0, 0);
  goto L1062;

  L688:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, HImode))
    {
      ro[1] = x2;
      goto L689;
    }
  x1 = XEXP (x0, 0);
  goto L1062;

  L689:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, QImode))
    {
      ro[2] = x2;
      return 134;
    }
  x1 = XEXP (x0, 0);
  goto L1062;

  L693:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, HImode))
    {
      ro[1] = x2;
      goto L694;
    }
  x1 = XEXP (x0, 0);
  goto L1062;

  L694:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, QImode))
    {
      ro[2] = x2;
      return 135;
    }
  x1 = XEXP (x0, 0);
  goto L1062;

  L752:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, QImode))
    {
      ro[1] = x2;
      goto L753;
    }
  x1 = XEXP (x0, 0);
  goto L1062;

  L753:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, QImode))
    {
      ro[2] = x2;
      return 145;
    }
  x1 = XEXP (x0, 0);
  goto L1062;

  L767:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, QImode))
    {
      ro[1] = x2;
      goto L768;
    }
  x1 = XEXP (x0, 0);
  goto L1062;

  L768:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, QImode))
    {
      ro[2] = x2;
      return 148;
    }
  x1 = XEXP (x0, 0);
  goto L1062;

  L782:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, QImode))
    {
      ro[1] = x2;
      goto L783;
    }
  x1 = XEXP (x0, 0);
  goto L1062;

  L783:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, QImode))
    {
      ro[2] = x2;
      return 151;
    }
  x1 = XEXP (x0, 0);
  goto L1062;

  L799:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, QImode))
    {
      ro[1] = x2;
      return 155;
    }
  x1 = XEXP (x0, 0);
  goto L1062;

  L908:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, QImode))
    {
      ro[1] = x2;
      return 180;
    }
  x1 = XEXP (x0, 0);
  goto L1062;

  L935:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, QImode))
    {
      ro[1] = x2;
      goto L936;
    }
  x1 = XEXP (x0, 0);
  goto L1062;

  L936:
  x2 = XEXP (x1, 1);
  if (nonmemory_operand (x2, QImode))
    {
      ro[2] = x2;
      return 186;
    }
  x1 = XEXP (x0, 0);
  goto L1062;

  L963:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, QImode))
    {
      ro[1] = x2;
      goto L964;
    }
  x1 = XEXP (x0, 0);
  goto L1062;

  L964:
  x2 = XEXP (x1, 1);
  if (nonmemory_operand (x2, QImode))
    {
      ro[2] = x2;
      return 192;
    }
  x1 = XEXP (x0, 0);
  goto L1062;

  L991:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, QImode))
    {
      ro[1] = x2;
      goto L992;
    }
  x1 = XEXP (x0, 0);
  goto L1062;

  L992:
  x2 = XEXP (x1, 1);
  if (nonmemory_operand (x2, QImode))
    {
      ro[2] = x2;
      return 198;
    }
  x1 = XEXP (x0, 0);
  goto L1062;

  L1006:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, QImode))
    {
      ro[1] = x2;
      goto L1007;
    }
  x1 = XEXP (x0, 0);
  goto L1062;

  L1007:
  x2 = XEXP (x1, 1);
  if (nonmemory_operand (x2, QImode))
    {
      ro[2] = x2;
      return 201;
    }
  x1 = XEXP (x0, 0);
  goto L1062;

  L1021:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, QImode))
    {
      ro[1] = x2;
      goto L1022;
    }
  x1 = XEXP (x0, 0);
  goto L1062;

  L1022:
  x2 = XEXP (x1, 1);
  if (nonmemory_operand (x2, QImode))
    {
      ro[2] = x2;
      return 204;
    }
  x1 = XEXP (x0, 0);
  goto L1062;

  L1063:
  x1 = XEXP (x0, 1);
  if (GET_MODE (x1) != QImode)
    {
      x1 = XEXP (x0, 0);
      goto L1380;
    }
  switch (GET_CODE (x1))
    {
    case EQ:
      goto L1064;
    case NE:
      goto L1069;
    case GT:
      goto L1074;
    case GTU:
      goto L1079;
    case LT:
      goto L1084;
    case LTU:
      goto L1089;
    case GE:
      goto L1094;
    case GEU:
      goto L1099;
    case LE:
      goto L1104;
    case LEU:
      goto L1109;
    }
  x1 = XEXP (x0, 0);
  goto L1380;

  L1064:
  x2 = XEXP (x1, 0);
  if (GET_CODE (x2) == CC0 && 1)
    goto L1065;
  x1 = XEXP (x0, 0);
  goto L1380;

  L1065:
  x2 = XEXP (x1, 1);
  if (GET_CODE (x2) == CONST_INT && XWINT (x2, 0) == 0 && 1)
    return 212;
  x1 = XEXP (x0, 0);
  goto L1380;

  L1069:
  x2 = XEXP (x1, 0);
  if (GET_CODE (x2) == CC0 && 1)
    goto L1070;
  x1 = XEXP (x0, 0);
  goto L1380;

  L1070:
  x2 = XEXP (x1, 1);
  if (GET_CODE (x2) == CONST_INT && XWINT (x2, 0) == 0 && 1)
    return 214;
  x1 = XEXP (x0, 0);
  goto L1380;

  L1074:
  x2 = XEXP (x1, 0);
  if (GET_CODE (x2) == CC0 && 1)
    goto L1075;
  x1 = XEXP (x0, 0);
  goto L1380;

  L1075:
  x2 = XEXP (x1, 1);
  if (GET_CODE (x2) == CONST_INT && XWINT (x2, 0) == 0 && 1)
    return 216;
  x1 = XEXP (x0, 0);
  goto L1380;

  L1079:
  x2 = XEXP (x1, 0);
  if (GET_CODE (x2) == CC0 && 1)
    goto L1080;
  x1 = XEXP (x0, 0);
  goto L1380;

  L1080:
  x2 = XEXP (x1, 1);
  if (GET_CODE (x2) == CONST_INT && XWINT (x2, 0) == 0 && 1)
    return 218;
  x1 = XEXP (x0, 0);
  goto L1380;

  L1084:
  x2 = XEXP (x1, 0);
  if (GET_CODE (x2) == CC0 && 1)
    goto L1085;
  x1 = XEXP (x0, 0);
  goto L1380;

  L1085:
  x2 = XEXP (x1, 1);
  if (GET_CODE (x2) == CONST_INT && XWINT (x2, 0) == 0 && 1)
    return 220;
  x1 = XEXP (x0, 0);
  goto L1380;

  L1089:
  x2 = XEXP (x1, 0);
  if (GET_CODE (x2) == CC0 && 1)
    goto L1090;
  x1 = XEXP (x0, 0);
  goto L1380;

  L1090:
  x2 = XEXP (x1, 1);
  if (GET_CODE (x2) == CONST_INT && XWINT (x2, 0) == 0 && 1)
    return 222;
  x1 = XEXP (x0, 0);
  goto L1380;

  L1094:
  x2 = XEXP (x1, 0);
  if (GET_CODE (x2) == CC0 && 1)
    goto L1095;
  x1 = XEXP (x0, 0);
  goto L1380;

  L1095:
  x2 = XEXP (x1, 1);
  if (GET_CODE (x2) == CONST_INT && XWINT (x2, 0) == 0 && 1)
    return 224;
  x1 = XEXP (x0, 0);
  goto L1380;

  L1099:
  x2 = XEXP (x1, 0);
  if (GET_CODE (x2) == CC0 && 1)
    goto L1100;
  x1 = XEXP (x0, 0);
  goto L1380;

  L1100:
  x2 = XEXP (x1, 1);
  if (GET_CODE (x2) == CONST_INT && XWINT (x2, 0) == 0 && 1)
    return 226;
  x1 = XEXP (x0, 0);
  goto L1380;

  L1104:
  x2 = XEXP (x1, 0);
  if (GET_CODE (x2) == CC0 && 1)
    goto L1105;
  x1 = XEXP (x0, 0);
  goto L1380;

  L1105:
  x2 = XEXP (x1, 1);
  if (GET_CODE (x2) == CONST_INT && XWINT (x2, 0) == 0 && 1)
    return 228;
  x1 = XEXP (x0, 0);
  goto L1380;

  L1109:
  x2 = XEXP (x1, 0);
  if (GET_CODE (x2) == CC0 && 1)
    goto L1110;
  x1 = XEXP (x0, 0);
  goto L1380;

  L1110:
  x2 = XEXP (x1, 1);
  if (GET_CODE (x2) == CONST_INT && XWINT (x2, 0) == 0 && 1)
    return 230;
  x1 = XEXP (x0, 0);
  goto L1380;

  L327:
  x1 = XEXP (x0, 1);
  if (general_operand (x1, SFmode))
    {
      ro[1] = x1;
      return 56;
    }
  x1 = XEXP (x0, 0);
  goto L329;

  L416:
  x1 = XEXP (x0, 1);
  if (GET_MODE (x1) == SFmode && GET_CODE (x1) == FLOAT_TRUNCATE && 1)
    goto L417;
  if (general_operand (x1, SFmode))
    {
      ro[1] = x1;
      return 57;
    }
  x1 = XEXP (x0, 0);
  goto L575;

  L417:
  x2 = XEXP (x1, 0);
  if (register_operand (x2, XFmode))
    {
      ro[1] = x2;
      if (TARGET_80387)
	return 79;
      }
  x1 = XEXP (x0, 0);
  goto L575;

  L576:
  x1 = XEXP (x0, 1);
  if (GET_MODE (x1) != SFmode)
    {
      x1 = XEXP (x0, 0);
      goto L1380;
    }
  switch (GET_CODE (x1))
    {
    case FLOAT:
      goto L577;
    case NEG:
      goto L803;
    case ABS:
      goto L825;
    case SQRT:
      goto L847;
    case UNSPEC:
      if (XINT (x1, 1) == 1 && XVECLEN (x1, 0) == 1 && 1)
	goto L878;
      if (XINT (x1, 1) == 2 && XVECLEN (x1, 0) == 1 && 1)
	goto L891;
    }
  x1 = XEXP (x0, 0);
  goto L1380;

  L577:
  x2 = XEXP (x1, 0);
  switch (GET_MODE (x2))
    {
    case DImode:
      if (nonimmediate_operand (x2, DImode))
	{
	  ro[1] = x2;
	  if (TARGET_80387)
	    return 104;
	  }
      break;
    case SImode:
      if (nonimmediate_operand (x2, SImode))
	{
	  ro[1] = x2;
	  if (TARGET_80387)
	    return 107;
	  }
    }
  x1 = XEXP (x0, 0);
  goto L1380;

  L803:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, SFmode))
    {
      ro[1] = x2;
      if (TARGET_80387)
	return 156;
      }
  x1 = XEXP (x0, 0);
  goto L1380;

  L825:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, SFmode))
    {
      ro[1] = x2;
      if (TARGET_80387)
	return 161;
      }
  x1 = XEXP (x0, 0);
  goto L1380;

  L847:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, SFmode))
    {
      ro[1] = x2;
      if (! TARGET_NO_FANCY_MATH_387 && TARGET_80387
   && (TARGET_IEEE_FP || flag_fast_math) )
	return 166;
      }
  x1 = XEXP (x0, 0);
  goto L1380;

  L878:
  x2 = XVECEXP (x1, 0, 0);
  if (register_operand (x2, SFmode))
    {
      ro[1] = x2;
      if (! TARGET_NO_FANCY_MATH_387 && TARGET_80387 
   && (TARGET_IEEE_FP || flag_fast_math) )
	return 173;
      }
  x1 = XEXP (x0, 0);
  goto L1380;

  L891:
  x2 = XVECEXP (x1, 0, 0);
  if (register_operand (x2, SFmode))
    {
      ro[1] = x2;
      if (! TARGET_NO_FANCY_MATH_387 && TARGET_80387 
   && (TARGET_IEEE_FP || flag_fast_math) )
	return 176;
      }
  x1 = XEXP (x0, 0);
  goto L1380;

  L333:
  x1 = XEXP (x0, 1);
  if (general_operand (x1, DFmode))
    {
      ro[1] = x1;
      return 58;
    }
  x1 = XEXP (x0, 0);
  goto L342;

  L397:
  x1 = XEXP (x0, 1);
  switch (GET_MODE (x1))
    {
    case DFmode:
      switch (GET_CODE (x1))
	{
	case FLOAT_EXTEND:
	  goto L398;
	case FLOAT_TRUNCATE:
	  goto L421;
	}
    }
  if (general_operand (x1, DFmode))
    {
      ro[1] = x1;
      return 60;
    }
  x1 = XEXP (x0, 0);
  goto L571;

  L398:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, SFmode))
    {
      ro[1] = x2;
      if (TARGET_80387)
	return 74;
      }
  x1 = XEXP (x0, 0);
  goto L571;

  L421:
  x2 = XEXP (x1, 0);
  if (register_operand (x2, XFmode))
    {
      ro[1] = x2;
      if (TARGET_80387)
	return 80;
      }
  x1 = XEXP (x0, 0);
  goto L571;

  L572:
  x1 = XEXP (x0, 1);
  if (GET_MODE (x1) != DFmode)
    {
      x1 = XEXP (x0, 0);
      goto L1380;
    }
  switch (GET_CODE (x1))
    {
    case FLOAT:
      goto L573;
    case NEG:
      goto L811;
    case ABS:
      goto L833;
    case SQRT:
      goto L855;
    case UNSPEC:
      if (XINT (x1, 1) == 1 && XVECLEN (x1, 0) == 1 && 1)
	goto L882;
      if (XINT (x1, 1) == 2 && XVECLEN (x1, 0) == 1 && 1)
	goto L895;
    }
  x1 = XEXP (x0, 0);
  goto L1380;

  L573:
  x2 = XEXP (x1, 0);
  switch (GET_MODE (x2))
    {
    case DImode:
      if (nonimmediate_operand (x2, DImode))
	{
	  ro[1] = x2;
	  if (TARGET_80387)
	    return 103;
	  }
      break;
    case SImode:
      if (nonimmediate_operand (x2, SImode))
	{
	  ro[1] = x2;
	  if (TARGET_80387)
	    return 105;
	  }
    }
  x1 = XEXP (x0, 0);
  goto L1380;

  L811:
  x2 = XEXP (x1, 0);
  if (GET_MODE (x2) == DFmode && GET_CODE (x2) == FLOAT_EXTEND && 1)
    goto L812;
  if (general_operand (x2, DFmode))
    {
      ro[1] = x2;
      if (TARGET_80387)
	return 157;
      }
  x1 = XEXP (x0, 0);
  goto L1380;

  L812:
  x3 = XEXP (x2, 0);
  if (general_operand (x3, SFmode))
    {
      ro[1] = x3;
      if (TARGET_80387)
	return 158;
      }
  x1 = XEXP (x0, 0);
  goto L1380;

  L833:
  x2 = XEXP (x1, 0);
  if (GET_MODE (x2) == DFmode && GET_CODE (x2) == FLOAT_EXTEND && 1)
    goto L834;
  if (general_operand (x2, DFmode))
    {
      ro[1] = x2;
      if (TARGET_80387)
	return 162;
      }
  x1 = XEXP (x0, 0);
  goto L1380;

  L834:
  x3 = XEXP (x2, 0);
  if (general_operand (x3, SFmode))
    {
      ro[1] = x3;
      if (TARGET_80387)
	return 163;
      }
  x1 = XEXP (x0, 0);
  goto L1380;

  L855:
  x2 = XEXP (x1, 0);
  if (GET_MODE (x2) == DFmode && GET_CODE (x2) == FLOAT_EXTEND && 1)
    goto L856;
  if (general_operand (x2, DFmode))
    {
      ro[1] = x2;
      if (! TARGET_NO_FANCY_MATH_387 && TARGET_80387
   && (TARGET_IEEE_FP || flag_fast_math) )
	return 167;
      }
  x1 = XEXP (x0, 0);
  goto L1380;

  L856:
  x3 = XEXP (x2, 0);
  if (general_operand (x3, SFmode))
    {
      ro[1] = x3;
      if (! TARGET_NO_FANCY_MATH_387 && TARGET_80387 
   && (TARGET_IEEE_FP || flag_fast_math) )
	return 168;
      }
  x1 = XEXP (x0, 0);
  goto L1380;

  L882:
  x2 = XVECEXP (x1, 0, 0);
  if (GET_MODE (x2) != DFmode)
    {
      x1 = XEXP (x0, 0);
      goto L1380;
    }
  if (GET_CODE (x2) == FLOAT_EXTEND && 1)
    goto L883;
  if (register_operand (x2, DFmode))
    {
      ro[1] = x2;
      if (! TARGET_NO_FANCY_MATH_387 && TARGET_80387 
   && (TARGET_IEEE_FP || flag_fast_math) )
	return 172;
      }
  x1 = XEXP (x0, 0);
  goto L1380;

  L883:
  x3 = XEXP (x2, 0);
  if (register_operand (x3, SFmode))
    {
      ro[1] = x3;
      if (! TARGET_NO_FANCY_MATH_387 && TARGET_80387 
   && (TARGET_IEEE_FP || flag_fast_math) )
	return 174;
      }
  x1 = XEXP (x0, 0);
  goto L1380;

  L895:
  x2 = XVECEXP (x1, 0, 0);
  if (GET_MODE (x2) != DFmode)
    {
      x1 = XEXP (x0, 0);
      goto L1380;
    }
  if (GET_CODE (x2) == FLOAT_EXTEND && 1)
    goto L896;
  if (register_operand (x2, DFmode))
    {
      ro[1] = x2;
      if (! TARGET_NO_FANCY_MATH_387 && TARGET_80387 
   && (TARGET_IEEE_FP || flag_fast_math) )
	return 175;
      }
  x1 = XEXP (x0, 0);
  goto L1380;

  L896:
  x3 = XEXP (x2, 0);
  if (register_operand (x3, SFmode))
    {
      ro[1] = x3;
      if (! TARGET_NO_FANCY_MATH_387 && TARGET_80387 
   && (TARGET_IEEE_FP || flag_fast_math) )
	return 177;
      }
  x1 = XEXP (x0, 0);
  goto L1380;

  L346:
  x1 = XEXP (x0, 1);
  if (general_operand (x1, XFmode))
    {
      ro[1] = x1;
      return 61;
    }
  x1 = XEXP (x0, 0);
  goto L355;

  L401:
  x1 = XEXP (x0, 1);
  if (GET_MODE (x1) == XFmode && GET_CODE (x1) == FLOAT_EXTEND && 1)
    goto L402;
  if (general_operand (x1, XFmode))
    {
      ro[1] = x1;
      return 63;
    }
  x1 = XEXP (x0, 0);
  goto L567;

  L402:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, DFmode))
    {
      ro[1] = x2;
      if (TARGET_80387)
	return 75;
      }
  L406:
  if (general_operand (x2, SFmode))
    {
      ro[1] = x2;
      if (TARGET_80387)
	return 76;
      }
  x1 = XEXP (x0, 0);
  goto L567;

  L568:
  x1 = XEXP (x0, 1);
  if (GET_MODE (x1) != XFmode)
    {
      x1 = XEXP (x0, 0);
      goto L1380;
    }
  switch (GET_CODE (x1))
    {
    case FLOAT:
      goto L569;
    case NEG:
      goto L820;
    case ABS:
      goto L842;
    case SQRT:
      goto L864;
    }
  x1 = XEXP (x0, 0);
  goto L1380;

  L569:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, DImode))
    {
      ro[1] = x2;
      if (TARGET_80387)
	return 102;
      }
  L585:
  if (general_operand (x2, SImode))
    {
      ro[1] = x2;
      if (TARGET_80387)
	return 106;
      }
  x1 = XEXP (x0, 0);
  goto L1380;

  L820:
  x2 = XEXP (x1, 0);
  if (GET_MODE (x2) == XFmode && GET_CODE (x2) == FLOAT_EXTEND && 1)
    goto L821;
  if (general_operand (x2, XFmode))
    {
      ro[1] = x2;
      if (TARGET_80387)
	return 159;
      }
  x1 = XEXP (x0, 0);
  goto L1380;

  L821:
  x3 = XEXP (x2, 0);
  if (general_operand (x3, DFmode))
    {
      ro[1] = x3;
      if (TARGET_80387)
	return 160;
      }
  x1 = XEXP (x0, 0);
  goto L1380;

  L842:
  x2 = XEXP (x1, 0);
  if (GET_MODE (x2) == XFmode && GET_CODE (x2) == FLOAT_EXTEND && 1)
    goto L843;
  if (general_operand (x2, XFmode))
    {
      ro[1] = x2;
      if (TARGET_80387)
	return 164;
      }
  x1 = XEXP (x0, 0);
  goto L1380;

  L843:
  x3 = XEXP (x2, 0);
  if (general_operand (x3, DFmode))
    {
      ro[1] = x3;
      if (TARGET_80387)
	return 165;
      }
  x1 = XEXP (x0, 0);
  goto L1380;

  L864:
  x2 = XEXP (x1, 0);
  if (GET_MODE (x2) == XFmode && GET_CODE (x2) == FLOAT_EXTEND && 1)
    goto L865;
  if (general_operand (x2, XFmode))
    {
      ro[1] = x2;
      if (! TARGET_NO_FANCY_MATH_387 && TARGET_80387 
   && (TARGET_IEEE_FP || flag_fast_math) )
	return 169;
      }
  x1 = XEXP (x0, 0);
  goto L1380;

  L865:
  x3 = XEXP (x2, 0);
  if (general_operand (x3, DFmode))
    {
      ro[1] = x3;
      if (! TARGET_NO_FANCY_MATH_387 && TARGET_80387 
   && (TARGET_IEEE_FP || flag_fast_math) )
	return 170;
      }
  L870:
  if (general_operand (x3, SFmode))
    {
      ro[1] = x3;
      if (! TARGET_NO_FANCY_MATH_387 && TARGET_80387 
   && (TARGET_IEEE_FP || flag_fast_math) )
	return 171;
      }
  x1 = XEXP (x0, 0);
  goto L1380;

  L359:
  x1 = XEXP (x0, 1);
  if (general_operand (x1, DImode))
    {
      ro[1] = x1;
      return 64;
    }
  x1 = XEXP (x0, 0);
  goto L361;

  L592:
  x1 = XEXP (x0, 1);
  switch (GET_MODE (x1))
    {
    case DImode:
      switch (GET_CODE (x1))
	{
	case PLUS:
	  goto L593;
	case MINUS:
	  goto L616;
	case NEG:
	  goto L787;
	}
    }
  if (general_operand (x1, DImode))
    {
      ro[1] = x1;
      return 65;
    }
  x1 = XEXP (x0, 0);
  goto L376;

  L593:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, DImode))
    {
      ro[1] = x2;
      goto L594;
    }
  x1 = XEXP (x0, 0);
  goto L376;

  L594:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, DImode))
    {
      ro[2] = x2;
      return 108;
    }
  x1 = XEXP (x0, 0);
  goto L376;

  L616:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, DImode))
    {
      ro[1] = x2;
      goto L617;
    }
  x1 = XEXP (x0, 0);
  goto L376;

  L617:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, DImode))
    {
      ro[2] = x2;
      return 116;
    }
  x1 = XEXP (x0, 0);
  goto L376;

  L787:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, DImode))
    {
      ro[1] = x2;
      return 152;
    }
  x1 = XEXP (x0, 0);
  goto L376;

  L377:
  x1 = XEXP (x0, 1);
  if (GET_MODE (x1) != DImode)
    {
      x1 = XEXP (x0, 0);
      goto L1380;
    }
  switch (GET_CODE (x1))
    {
    case ZERO_EXTEND:
      goto L378;
    case SIGN_EXTEND:
      goto L382;
    case MULT:
      goto L674;
    case ASHIFT:
      goto L912;
    case ASHIFTRT:
      goto L940;
    case LSHIFTRT:
      goto L968;
    }
  x1 = XEXP (x0, 0);
  goto L1380;

  L378:
  x2 = XEXP (x1, 0);
  if (register_operand (x2, SImode))
    {
      ro[1] = x2;
      return 69;
    }
  x1 = XEXP (x0, 0);
  goto L1380;

  L382:
  x2 = XEXP (x1, 0);
  if (register_operand (x2, SImode))
    {
      ro[1] = x2;
      return 70;
    }
  x1 = XEXP (x0, 0);
  goto L1380;

  L674:
  x2 = XEXP (x1, 0);
  if (GET_MODE (x2) != DImode)
    {
      x1 = XEXP (x0, 0);
      goto L1380;
    }
  switch (GET_CODE (x2))
    {
    case ZERO_EXTEND:
      goto L675;
    case SIGN_EXTEND:
      goto L682;
    }
  x1 = XEXP (x0, 0);
  goto L1380;

  L675:
  x3 = XEXP (x2, 0);
  if (register_operand (x3, SImode))
    {
      ro[1] = x3;
      goto L676;
    }
  x1 = XEXP (x0, 0);
  goto L1380;

  L676:
  x2 = XEXP (x1, 1);
  if (GET_MODE (x2) == DImode && GET_CODE (x2) == ZERO_EXTEND && 1)
    goto L677;
  x1 = XEXP (x0, 0);
  goto L1380;

  L677:
  x3 = XEXP (x2, 0);
  if (nonimmediate_operand (x3, SImode))
    {
      ro[2] = x3;
      return 129;
    }
  x1 = XEXP (x0, 0);
  goto L1380;

  L682:
  x3 = XEXP (x2, 0);
  if (register_operand (x3, SImode))
    {
      ro[1] = x3;
      goto L683;
    }
  x1 = XEXP (x0, 0);
  goto L1380;

  L683:
  x2 = XEXP (x1, 1);
  if (GET_MODE (x2) == DImode && GET_CODE (x2) == SIGN_EXTEND && 1)
    goto L684;
  x1 = XEXP (x0, 0);
  goto L1380;

  L684:
  x3 = XEXP (x2, 0);
  if (nonimmediate_operand (x3, SImode))
    {
      ro[2] = x3;
      return 130;
    }
  x1 = XEXP (x0, 0);
  goto L1380;

  L912:
  x2 = XEXP (x1, 0);
  if (register_operand (x2, DImode))
    {
      ro[1] = x2;
      goto L913;
    }
  x1 = XEXP (x0, 0);
  goto L1380;

  L913:
  x2 = XEXP (x1, 1);
  if (GET_CODE (x2) == CONST_INT && 1)
    {
      ro[2] = x2;
      return 182;
    }
  x1 = XEXP (x0, 0);
  goto L1380;

  L940:
  x2 = XEXP (x1, 0);
  if (register_operand (x2, DImode))
    {
      ro[1] = x2;
      goto L941;
    }
  x1 = XEXP (x0, 0);
  goto L1380;

  L941:
  x2 = XEXP (x1, 1);
  if (GET_CODE (x2) == CONST_INT && 1)
    {
      ro[2] = x2;
      return 188;
    }
  x1 = XEXP (x0, 0);
  goto L1380;

  L968:
  x2 = XEXP (x1, 0);
  if (register_operand (x2, DImode))
    {
      ro[1] = x2;
      goto L969;
    }
  x1 = XEXP (x0, 0);
  goto L1380;

  L969:
  x2 = XEXP (x1, 1);
  if (GET_CODE (x2) == CONST_INT && 1)
    {
      ro[2] = x2;
      return 194;
    }
  x1 = XEXP (x0, 0);
  goto L1380;
 L2:
  tem = recog_2 (x0, insn, pnum_clobbers);
  if (tem >= 0) return tem;
  x1 = XEXP (x0, 0);
  goto L1380;

  L313:
  x2 = XEXP (x1, 0);
  switch (GET_MODE (x2))
    {
    case HImode:
      if (general_operand (x2, HImode))
	{
	  ro[0] = x2;
	  goto L314;
	}
      break;
    case QImode:
      if (general_operand (x2, QImode))
	{
	  ro[0] = x2;
	  goto L324;
	}
    }
  goto L1380;

  L314:
  x1 = XEXP (x0, 1);
  if (general_operand (x1, HImode))
    {
      ro[1] = x1;
      return 52;
    }
  x1 = XEXP (x0, 0);
  goto L1380;

  L324:
  x1 = XEXP (x0, 1);
  if (general_operand (x1, QImode))
    {
      ro[1] = x1;
      return 55;
    }
  x1 = XEXP (x0, 0);
  goto L1380;

  L1314:
  x1 = XEXP (x0, 1);
  switch (GET_CODE (x1))
    {
    case MINUS:
      if (GET_MODE (x1) == SImode && 1)
	goto L1315;
      break;
    case IF_THEN_ELSE:
      goto L1114;
    case LABEL_REF:
      goto L1294;
    }
  L1297:
  if (general_operand (x1, SImode))
    {
      ro[0] = x1;
      return 262;
    }
  x1 = XEXP (x0, 0);
  goto L1380;

  L1315:
  x2 = XEXP (x1, 0);
  if (GET_MODE (x2) == SImode && GET_CODE (x2) == REG && XINT (x2, 0) == 3 && 1)
    goto L1316;
  x1 = XEXP (x0, 0);
  goto L1380;

  L1316:
  x2 = XEXP (x1, 1);
  if (GET_MODE (x2) == SImode && GET_CODE (x2) == MEM && 1)
    goto L1317;
  x1 = XEXP (x0, 0);
  goto L1380;

  L1317:
  x3 = XEXP (x2, 0);
  if (GET_MODE (x3) == SImode && GET_CODE (x3) == PLUS && 1)
    goto L1318;
  x1 = XEXP (x0, 0);
  goto L1380;

  L1318:
  x4 = XEXP (x3, 0);
  if (GET_MODE (x4) == SImode && GET_CODE (x4) == MULT && 1)
    goto L1319;
  x1 = XEXP (x0, 0);
  goto L1380;

  L1319:
  x5 = XEXP (x4, 0);
  if (register_operand (x5, SImode))
    {
      ro[0] = x5;
      goto L1320;
    }
  x1 = XEXP (x0, 0);
  goto L1380;

  L1320:
  x5 = XEXP (x4, 1);
  if (GET_CODE (x5) == CONST_INT && XWINT (x5, 0) == 4 && 1)
    goto L1321;
  x1 = XEXP (x0, 0);
  goto L1380;

  L1321:
  x4 = XEXP (x3, 1);
  if (GET_CODE (x4) == LABEL_REF && 1)
    goto L1322;
  x1 = XEXP (x0, 0);
  goto L1380;

  L1322:
  x5 = XEXP (x4, 0);
  if (pnum_clobbers != 0 && 1)
    {
      ro[1] = x5;
      *pnum_clobbers = 1;
      return 264;
    }
  x1 = XEXP (x0, 0);
  goto L1380;
 L1114:
  tem = recog_3 (x0, insn, pnum_clobbers);
  if (tem >= 0) return tem;
  x1 = XEXP (x0, 0);
  goto L1380;

  L1294:
  x2 = XEXP (x1, 0);
  ro[0] = x2;
  return 261;

  L1381:
  x1 = XEXP (x0, 1);
  if (GET_CODE (x1) == CALL && 1)
    goto L1382;
  x1 = XEXP (x0, 0);
  goto L1460;

  L1382:
  x2 = XEXP (x1, 0);
  if (call_insn_operand (x2, QImode))
    {
      ro[1] = x2;
      goto L1383;
    }
  L1387:
  if (GET_MODE (x2) == QImode && GET_CODE (x2) == MEM && 1)
    goto L1388;
  x1 = XEXP (x0, 0);
  goto L1460;

  L1383:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, SImode))
    {
      ro[2] = x2;
      return 276;
    }
  x2 = XEXP (x1, 0);
  goto L1387;

  L1388:
  x3 = XEXP (x2, 0);
  if (symbolic_operand (x3, SImode))
    {
      ro[1] = x3;
      goto L1389;
    }
  x1 = XEXP (x0, 0);
  goto L1460;

  L1389:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, SImode))
    {
      ro[2] = x2;
      if (!HALF_PIC_P ())
	return 277;
      }
  x1 = XEXP (x0, 0);
  goto L1460;

  L1461:
  x1 = XEXP (x0, 1);
  if (GET_MODE (x1) == SImode && GET_CODE (x1) == PLUS && 1)
    goto L1462;
  goto ret0;

  L1462:
  x2 = XEXP (x1, 0);
  if (GET_MODE (x2) == SImode && GET_CODE (x2) == FFS && 1)
    goto L1463;
  goto ret0;

  L1463:
  x3 = XEXP (x2, 0);
  if (general_operand (x3, SImode))
    {
      ro[1] = x3;
      goto L1464;
    }
  goto ret0;

  L1464:
  x2 = XEXP (x1, 1);
  if (GET_CODE (x2) == CONST_INT && XWINT (x2, 0) == -1 && 1)
    return 291;
  goto ret0;

  L1467:
  x1 = XEXP (x0, 1);
  if (GET_MODE (x1) == HImode && GET_CODE (x1) == PLUS && 1)
    goto L1468;
  goto ret0;

  L1468:
  x2 = XEXP (x1, 0);
  if (GET_MODE (x2) == HImode && GET_CODE (x2) == FFS && 1)
    goto L1469;
  goto ret0;

  L1469:
  x3 = XEXP (x2, 0);
  if (general_operand (x3, SImode))
    {
      ro[1] = x3;
      goto L1470;
    }
  goto ret0;

  L1470:
  x2 = XEXP (x1, 1);
  if (GET_CODE (x2) == CONST_INT && XWINT (x2, 0) == -1 && 1)
    return 293;
  goto ret0;

  L1473:
  x1 = XEXP (x0, 1);
  if (binary_387_op (x1, DFmode))
    {
      ro[3] = x1;
      goto L1479;
    }
  goto ret0;

  L1479:
  x2 = XEXP (x1, 0);
  switch (GET_MODE (x2))
    {
    case DFmode:
      switch (GET_CODE (x2))
	{
	case FLOAT:
	  goto L1480;
	case FLOAT_EXTEND:
	  goto L1515;
	case SUBREG:
	case REG:
	case MEM:
	  if (nonimmediate_operand (x2, DFmode))
	    {
	      ro[1] = x2;
	      goto L1475;
	    }
	}
    }
  L1520:
  if (general_operand (x2, DFmode))
    {
      ro[1] = x2;
      goto L1521;
    }
  goto ret0;

  L1480:
  x3 = XEXP (x2, 0);
  if (general_operand (x3, SImode))
    {
      ro[1] = x3;
      goto L1481;
    }
  goto ret0;

  L1481:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, DFmode))
    {
      ro[2] = x2;
      if (TARGET_80387)
	return 295;
      }
  goto ret0;

  L1515:
  x3 = XEXP (x2, 0);
  if (general_operand (x3, SFmode))
    {
      ro[1] = x3;
      goto L1516;
    }
  goto ret0;

  L1516:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, DFmode))
    {
      ro[2] = x2;
      if (TARGET_80387)
	return 301;
      }
  goto ret0;

  L1475:
  x2 = XEXP (x1, 1);
  if (nonimmediate_operand (x2, DFmode))
    {
      ro[2] = x2;
      if (TARGET_80387)
	return 294;
      }
  x2 = XEXP (x1, 0);
  goto L1520;

  L1521:
  x2 = XEXP (x1, 1);
  if (GET_MODE (x2) != DFmode)
    goto ret0;
  switch (GET_CODE (x2))
    {
    case FLOAT:
      goto L1522;
    case FLOAT_EXTEND:
      goto L1528;
    }
  goto ret0;

  L1522:
  x3 = XEXP (x2, 0);
  if (general_operand (x3, SImode))
    {
      ro[2] = x3;
      if (TARGET_80387)
	return 302;
      }
  goto ret0;

  L1528:
  x3 = XEXP (x2, 0);
  if (general_operand (x3, SFmode))
    {
      ro[2] = x3;
      if (TARGET_80387)
	return 303;
      }
  goto ret0;

  L1484:
  x1 = XEXP (x0, 1);
  if (binary_387_op (x1, XFmode))
    {
      ro[3] = x1;
      goto L1490;
    }
  goto ret0;

  L1490:
  x2 = XEXP (x1, 0);
  switch (GET_MODE (x2))
    {
    case XFmode:
      switch (GET_CODE (x2))
	{
	case FLOAT:
	  goto L1491;
	case FLOAT_EXTEND:
	  goto L1497;
	case SUBREG:
	case REG:
	case MEM:
	  if (nonimmediate_operand (x2, XFmode))
	    {
	      ro[1] = x2;
	      goto L1486;
	    }
	}
    }
  L1502:
  if (general_operand (x2, XFmode))
    {
      ro[1] = x2;
      goto L1503;
    }
  goto ret0;

  L1491:
  x3 = XEXP (x2, 0);
  if (general_operand (x3, SImode))
    {
      ro[1] = x3;
      goto L1492;
    }
  goto ret0;

  L1492:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, XFmode))
    {
      ro[2] = x2;
      if (TARGET_80387)
	return 297;
      }
  goto ret0;

  L1497:
  x3 = XEXP (x2, 0);
  if (general_operand (x3, SFmode))
    {
      ro[1] = x3;
      goto L1498;
    }
  goto ret0;

  L1498:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, XFmode))
    {
      ro[2] = x2;
      if (TARGET_80387)
	return 298;
      }
  goto ret0;

  L1486:
  x2 = XEXP (x1, 1);
  if (nonimmediate_operand (x2, XFmode))
    {
      ro[2] = x2;
      if (TARGET_80387)
	return 296;
      }
  x2 = XEXP (x1, 0);
  goto L1502;

  L1503:
  x2 = XEXP (x1, 1);
  if (GET_MODE (x2) != XFmode)
    goto ret0;
  switch (GET_CODE (x2))
    {
    case FLOAT:
      goto L1504;
    case FLOAT_EXTEND:
      goto L1510;
    }
  goto ret0;

  L1504:
  x3 = XEXP (x2, 0);
  if (general_operand (x3, SImode))
    {
      ro[2] = x3;
      if (TARGET_80387)
	return 299;
      }
  goto ret0;

  L1510:
  x3 = XEXP (x2, 0);
  if (general_operand (x3, SFmode))
    {
      ro[2] = x3;
      if (TARGET_80387)
	return 300;
      }
  goto ret0;

  L1531:
  x1 = XEXP (x0, 1);
  if (binary_387_op (x1, SFmode))
    {
      ro[3] = x1;
      goto L1537;
    }
  goto ret0;

  L1537:
  x2 = XEXP (x1, 0);
  switch (GET_MODE (x2))
    {
    case SFmode:
      if (GET_CODE (x2) == FLOAT && 1)
	goto L1538;
      if (nonimmediate_operand (x2, SFmode))
	{
	  ro[1] = x2;
	  goto L1533;
	}
    }
  L1543:
  if (general_operand (x2, SFmode))
    {
      ro[1] = x2;
      goto L1544;
    }
  goto ret0;

  L1538:
  x3 = XEXP (x2, 0);
  if (general_operand (x3, SImode))
    {
      ro[1] = x3;
      goto L1539;
    }
  goto ret0;

  L1539:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, SFmode))
    {
      ro[2] = x2;
      if (TARGET_80387)
	return 305;
      }
  goto ret0;

  L1533:
  x2 = XEXP (x1, 1);
  if (nonimmediate_operand (x2, SFmode))
    {
      ro[2] = x2;
      if (TARGET_80387)
	return 304;
      }
  x2 = XEXP (x1, 0);
  goto L1543;

  L1544:
  x2 = XEXP (x1, 1);
  if (GET_MODE (x2) == SFmode && GET_CODE (x2) == FLOAT && 1)
    goto L1545;
  goto ret0;

  L1545:
  x3 = XEXP (x2, 0);
  if (general_operand (x3, SImode))
    {
      ro[2] = x3;
      if (TARGET_80387)
	return 306;
      }
  goto ret0;
 ret0: return -1;
}

int
recog_5 (x0, insn, pnum_clobbers)
     register rtx x0;
     rtx insn;
     int *pnum_clobbers;
{
  register rtx *ro = &recog_operand[0];
  register rtx x1, x2, x3, x4, x5, x6;
  int tem;

  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 1);
  x3 = XEXP (x2, 0);
  switch (GET_MODE (x3))
    {
    case XFmode:
      if (GET_CODE (x3) == FLOAT && 1)
	goto L84;
      if (nonimmediate_operand (x3, XFmode))
	{
	  ro[0] = x3;
	  goto L56;
	}
    L68:
      if (register_operand (x3, XFmode))
	{
	  ro[0] = x3;
	  goto L69;
	}
      break;
    case DFmode:
      switch (GET_CODE (x3))
	{
	case FLOAT:
	  goto L170;
	case FLOAT_EXTEND:
	  goto L200;
	case SUBREG:
	case REG:
	case MEM:
	  if (nonimmediate_operand (x3, DFmode))
	    {
	      ro[0] = x3;
	      goto L142;
	    }
	}
    L154:
      if (register_operand (x3, DFmode))
	{
	  ro[0] = x3;
	  goto L155;
	}
      break;
    case SFmode:
      if (GET_CODE (x3) == FLOAT && 1)
	goto L256;
      if (nonimmediate_operand (x3, SFmode))
	{
	  ro[0] = x3;
	  goto L228;
	}
    L240:
      if (register_operand (x3, SFmode))
	{
	  ro[0] = x3;
	  goto L241;
	}
    }
  goto ret0;

  L84:
  x4 = XEXP (x3, 0);
  if (nonimmediate_operand (x4, SImode))
    {
      ro[0] = x4;
      goto L85;
    }
  goto ret0;

  L85:
  x3 = XEXP (x2, 1);
  if (register_operand (x3, XFmode))
    {
      ro[1] = x3;
      goto L86;
    }
  goto ret0;

  L86:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L87;
  goto ret0;

  L87:
  x2 = XEXP (x1, 0);
  if (scratch_operand (x2, HImode))
    {
      ro[3] = x2;
      if (TARGET_80387)
	return 20;
      }
  goto ret0;

  L56:
  x3 = XEXP (x2, 1);
  if (nonimmediate_operand (x3, XFmode))
    {
      ro[1] = x3;
      goto L57;
    }
  x3 = XEXP (x2, 0);
  goto L68;

  L57:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L58;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 1);
  x3 = XEXP (x2, 0);
  goto L68;

  L58:
  x2 = XEXP (x1, 0);
  if (scratch_operand (x2, HImode))
    {
      ro[3] = x2;
      if (TARGET_80387
   && (GET_CODE (operands[0]) != MEM || GET_CODE (operands[1]) != MEM))
	return 18;
      }
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 1);
  x3 = XEXP (x2, 0);
  goto L68;

  L69:
  x3 = XEXP (x2, 1);
  if (GET_MODE (x3) != XFmode)
    goto ret0;
  switch (GET_CODE (x3))
    {
    case FLOAT:
      goto L70;
    case FLOAT_EXTEND:
      goto L100;
    }
  goto ret0;

  L70:
  x4 = XEXP (x3, 0);
  if (nonimmediate_operand (x4, SImode))
    {
      ro[1] = x4;
      goto L71;
    }
  goto ret0;

  L71:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L72;
  goto ret0;

  L72:
  x2 = XEXP (x1, 0);
  if (scratch_operand (x2, HImode))
    {
      ro[3] = x2;
      if (TARGET_80387)
	return 19;
      }
  goto ret0;

  L100:
  x4 = XEXP (x3, 0);
  switch (GET_MODE (x4))
    {
    case DFmode:
      if (nonimmediate_operand (x4, DFmode))
	{
	  ro[1] = x4;
	  goto L101;
	}
      break;
    case SFmode:
      if (nonimmediate_operand (x4, SFmode))
	{
	  ro[1] = x4;
	  goto L116;
	}
    }
  goto ret0;

  L101:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L102;
  goto ret0;

  L102:
  x2 = XEXP (x1, 0);
  if (scratch_operand (x2, HImode))
    {
      ro[3] = x2;
      if (TARGET_80387)
	return 21;
      }
  goto ret0;

  L116:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L117;
  goto ret0;

  L117:
  x2 = XEXP (x1, 0);
  if (scratch_operand (x2, HImode))
    {
      ro[3] = x2;
      if (TARGET_80387)
	return 22;
      }
  goto ret0;

  L170:
  x4 = XEXP (x3, 0);
  if (nonimmediate_operand (x4, SImode))
    {
      ro[0] = x4;
      goto L171;
    }
  goto ret0;

  L171:
  x3 = XEXP (x2, 1);
  if (register_operand (x3, DFmode))
    {
      ro[1] = x3;
      goto L172;
    }
  goto ret0;

  L172:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L173;
  goto ret0;

  L173:
  x2 = XEXP (x1, 0);
  if (scratch_operand (x2, HImode))
    {
      ro[3] = x2;
      if (TARGET_80387)
	return 26;
      }
  goto ret0;

  L200:
  x4 = XEXP (x3, 0);
  if (nonimmediate_operand (x4, SFmode))
    {
      ro[0] = x4;
      goto L201;
    }
  goto ret0;

  L201:
  x3 = XEXP (x2, 1);
  if (register_operand (x3, DFmode))
    {
      ro[1] = x3;
      goto L202;
    }
  goto ret0;

  L202:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L203;
  goto ret0;

  L203:
  x2 = XEXP (x1, 0);
  if (scratch_operand (x2, HImode))
    {
      ro[3] = x2;
      if (TARGET_80387)
	return 28;
      }
  goto ret0;

  L142:
  x3 = XEXP (x2, 1);
  if (nonimmediate_operand (x3, DFmode))
    {
      ro[1] = x3;
      goto L143;
    }
  x3 = XEXP (x2, 0);
  goto L154;

  L143:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L144;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 1);
  x3 = XEXP (x2, 0);
  goto L154;

  L144:
  x2 = XEXP (x1, 0);
  if (scratch_operand (x2, HImode))
    {
      ro[3] = x2;
      if (TARGET_80387
   && (GET_CODE (operands[0]) != MEM || GET_CODE (operands[1]) != MEM))
	return 24;
      }
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 1);
  x3 = XEXP (x2, 0);
  goto L154;

  L155:
  x3 = XEXP (x2, 1);
  if (GET_MODE (x3) != DFmode)
    goto ret0;
  switch (GET_CODE (x3))
    {
    case FLOAT:
      goto L156;
    case FLOAT_EXTEND:
      goto L186;
    }
  goto ret0;

  L156:
  x4 = XEXP (x3, 0);
  if (nonimmediate_operand (x4, SImode))
    {
      ro[1] = x4;
      goto L157;
    }
  goto ret0;

  L157:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L158;
  goto ret0;

  L158:
  x2 = XEXP (x1, 0);
  if (scratch_operand (x2, HImode))
    {
      ro[3] = x2;
      if (TARGET_80387)
	return 25;
      }
  goto ret0;

  L186:
  x4 = XEXP (x3, 0);
  if (nonimmediate_operand (x4, SFmode))
    {
      ro[1] = x4;
      goto L187;
    }
  goto ret0;

  L187:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L188;
  goto ret0;

  L188:
  x2 = XEXP (x1, 0);
  if (scratch_operand (x2, HImode))
    {
      ro[3] = x2;
      if (TARGET_80387)
	return 27;
      }
  goto ret0;

  L256:
  x4 = XEXP (x3, 0);
  if (nonimmediate_operand (x4, SImode))
    {
      ro[0] = x4;
      goto L257;
    }
  goto ret0;

  L257:
  x3 = XEXP (x2, 1);
  if (register_operand (x3, SFmode))
    {
      ro[1] = x3;
      goto L258;
    }
  goto ret0;

  L258:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L259;
  goto ret0;

  L259:
  x2 = XEXP (x1, 0);
  if (scratch_operand (x2, HImode))
    {
      ro[3] = x2;
      if (TARGET_80387)
	return 32;
      }
  goto ret0;

  L228:
  x3 = XEXP (x2, 1);
  if (nonimmediate_operand (x3, SFmode))
    {
      ro[1] = x3;
      goto L229;
    }
  x3 = XEXP (x2, 0);
  goto L240;

  L229:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L230;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 1);
  x3 = XEXP (x2, 0);
  goto L240;

  L230:
  x2 = XEXP (x1, 0);
  if (scratch_operand (x2, HImode))
    {
      ro[3] = x2;
      if (TARGET_80387
   && (GET_CODE (operands[0]) != MEM || GET_CODE (operands[1]) != MEM))
	return 30;
      }
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 1);
  x3 = XEXP (x2, 0);
  goto L240;

  L241:
  x3 = XEXP (x2, 1);
  if (GET_MODE (x3) == SFmode && GET_CODE (x3) == FLOAT && 1)
    goto L242;
  goto ret0;

  L242:
  x4 = XEXP (x3, 0);
  if (nonimmediate_operand (x4, SImode))
    {
      ro[1] = x4;
      goto L243;
    }
  goto ret0;

  L243:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L244;
  goto ret0;

  L244:
  x2 = XEXP (x1, 0);
  if (scratch_operand (x2, HImode))
    {
      ro[3] = x2;
      if (TARGET_80387)
	return 31;
      }
  goto ret0;
 ret0: return -1;
}

int
recog_6 (x0, insn, pnum_clobbers)
     register rtx x0;
     rtx insn;
     int *pnum_clobbers;
{
  register rtx *ro = &recog_operand[0];
  register rtx x1, x2, x3, x4, x5, x6;
  int tem;

  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  switch (GET_MODE (x2))
    {
    case DFmode:
      if (register_operand (x2, DFmode))
	{
	  ro[0] = x2;
	  goto L337;
	}
      break;
    case XFmode:
      if (register_operand (x2, XFmode))
	{
	  ro[0] = x2;
	  goto L350;
	}
      break;
    case SFmode:
      if (nonimmediate_operand (x2, SFmode))
	{
	  ro[0] = x2;
	  goto L410;
	}
      break;
    case SImode:
      if (register_operand (x2, SImode))
	{
	  ro[0] = x2;
	  goto L698;
	}
      break;
    case HImode:
      if (register_operand (x2, HImode))
	{
	  ro[0] = x2;
	  goto L709;
	}
      break;
    case DImode:
      if (register_operand (x2, DImode))
	{
	  ro[0] = x2;
	  goto L917;
	}
    }
  switch (GET_CODE (x2))
    {
    case CC0:
      goto L12;
    case PC:
      goto L1301;
    }
  L1358:
  ro[0] = x2;
  goto L1359;
  L1548:
  if (register_operand (x2, SImode))
    {
      ro[0] = x2;
      goto L1549;
    }
  goto ret0;

  L337:
  x2 = XEXP (x1, 1);
  if (register_operand (x2, DFmode))
    {
      ro[1] = x2;
      goto L338;
    }
  x2 = XEXP (x1, 0);
  goto L1358;

  L338:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == SET && 1)
    goto L339;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1358;

  L339:
  x2 = XEXP (x1, 0);
  if (rtx_equal_p (x2, ro[1]) && 1)
    goto L340;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1358;

  L340:
  x2 = XEXP (x1, 1);
  if (rtx_equal_p (x2, ro[0]) && 1)
    return 59;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1358;

  L350:
  x2 = XEXP (x1, 1);
  if (register_operand (x2, XFmode))
    {
      ro[1] = x2;
      goto L351;
    }
  x2 = XEXP (x1, 0);
  goto L1358;

  L351:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == SET && 1)
    goto L352;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1358;

  L352:
  x2 = XEXP (x1, 0);
  if (rtx_equal_p (x2, ro[1]) && 1)
    goto L353;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1358;

  L353:
  x2 = XEXP (x1, 1);
  if (rtx_equal_p (x2, ro[0]) && 1)
    return 62;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1358;

  L410:
  x2 = XEXP (x1, 1);
  if (GET_MODE (x2) == SFmode && GET_CODE (x2) == FLOAT_TRUNCATE && 1)
    goto L411;
  x2 = XEXP (x1, 0);
  goto L1358;

  L411:
  x3 = XEXP (x2, 0);
  if (register_operand (x3, DFmode))
    {
      ro[1] = x3;
      goto L412;
    }
  x2 = XEXP (x1, 0);
  goto L1358;

  L412:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L413;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1358;

  L413:
  x2 = XEXP (x1, 0);
  if (memory_operand (x2, SFmode))
    {
      ro[2] = x2;
      if (TARGET_80387)
	return 78;
      }
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1358;

  L698:
  x2 = XEXP (x1, 1);
  if (GET_MODE (x2) != SImode)
    {
      x2 = XEXP (x1, 0);
      goto L1358;
    }
  switch (GET_CODE (x2))
    {
    case DIV:
      goto L699;
    case UDIV:
      goto L721;
    }
  x2 = XEXP (x1, 0);
  goto L1358;

  L699:
  x3 = XEXP (x2, 0);
  if (register_operand (x3, SImode))
    {
      ro[1] = x3;
      goto L700;
    }
  x2 = XEXP (x1, 0);
  goto L1358;

  L700:
  x3 = XEXP (x2, 1);
  if (general_operand (x3, SImode))
    {
      ro[2] = x3;
      goto L701;
    }
  x2 = XEXP (x1, 0);
  goto L1358;

  L701:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == SET && 1)
    goto L702;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1358;

  L702:
  x2 = XEXP (x1, 0);
  if (register_operand (x2, SImode))
    {
      ro[3] = x2;
      goto L703;
    }
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1358;

  L703:
  x2 = XEXP (x1, 1);
  if (GET_MODE (x2) == SImode && GET_CODE (x2) == MOD && 1)
    goto L704;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1358;

  L704:
  x3 = XEXP (x2, 0);
  if (rtx_equal_p (x3, ro[1]) && 1)
    goto L705;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1358;

  L705:
  x3 = XEXP (x2, 1);
  if (rtx_equal_p (x3, ro[2]) && 1)
    return 139;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1358;

  L721:
  x3 = XEXP (x2, 0);
  if (register_operand (x3, SImode))
    {
      ro[1] = x3;
      goto L722;
    }
  x2 = XEXP (x1, 0);
  goto L1358;

  L722:
  x3 = XEXP (x2, 1);
  if (general_operand (x3, SImode))
    {
      ro[2] = x3;
      goto L723;
    }
  x2 = XEXP (x1, 0);
  goto L1358;

  L723:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == SET && 1)
    goto L724;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1358;

  L724:
  x2 = XEXP (x1, 0);
  if (register_operand (x2, SImode))
    {
      ro[3] = x2;
      goto L725;
    }
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1358;

  L725:
  x2 = XEXP (x1, 1);
  if (GET_MODE (x2) == SImode && GET_CODE (x2) == UMOD && 1)
    goto L726;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1358;

  L726:
  x3 = XEXP (x2, 0);
  if (rtx_equal_p (x3, ro[1]) && 1)
    goto L727;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1358;

  L727:
  x3 = XEXP (x2, 1);
  if (rtx_equal_p (x3, ro[2]) && 1)
    return 141;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1358;

  L709:
  x2 = XEXP (x1, 1);
  if (GET_MODE (x2) != HImode)
    {
      x2 = XEXP (x1, 0);
      goto L1358;
    }
  switch (GET_CODE (x2))
    {
    case DIV:
      goto L710;
    case UDIV:
      goto L732;
    }
  x2 = XEXP (x1, 0);
  goto L1358;

  L710:
  x3 = XEXP (x2, 0);
  if (register_operand (x3, HImode))
    {
      ro[1] = x3;
      goto L711;
    }
  x2 = XEXP (x1, 0);
  goto L1358;

  L711:
  x3 = XEXP (x2, 1);
  if (general_operand (x3, HImode))
    {
      ro[2] = x3;
      goto L712;
    }
  x2 = XEXP (x1, 0);
  goto L1358;

  L712:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == SET && 1)
    goto L713;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1358;

  L713:
  x2 = XEXP (x1, 0);
  if (register_operand (x2, HImode))
    {
      ro[3] = x2;
      goto L714;
    }
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1358;

  L714:
  x2 = XEXP (x1, 1);
  if (GET_MODE (x2) == HImode && GET_CODE (x2) == MOD && 1)
    goto L715;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1358;

  L715:
  x3 = XEXP (x2, 0);
  if (rtx_equal_p (x3, ro[1]) && 1)
    goto L716;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1358;

  L716:
  x3 = XEXP (x2, 1);
  if (rtx_equal_p (x3, ro[2]) && 1)
    return 140;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1358;

  L732:
  x3 = XEXP (x2, 0);
  if (register_operand (x3, HImode))
    {
      ro[1] = x3;
      goto L733;
    }
  x2 = XEXP (x1, 0);
  goto L1358;

  L733:
  x3 = XEXP (x2, 1);
  if (general_operand (x3, HImode))
    {
      ro[2] = x3;
      goto L734;
    }
  x2 = XEXP (x1, 0);
  goto L1358;

  L734:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == SET && 1)
    goto L735;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1358;

  L735:
  x2 = XEXP (x1, 0);
  if (register_operand (x2, HImode))
    {
      ro[3] = x2;
      goto L736;
    }
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1358;

  L736:
  x2 = XEXP (x1, 1);
  if (GET_MODE (x2) == HImode && GET_CODE (x2) == UMOD && 1)
    goto L737;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1358;

  L737:
  x3 = XEXP (x2, 0);
  if (rtx_equal_p (x3, ro[1]) && 1)
    goto L738;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1358;

  L738:
  x3 = XEXP (x2, 1);
  if (rtx_equal_p (x3, ro[2]) && 1)
    return 142;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1358;

  L917:
  x2 = XEXP (x1, 1);
  if (GET_MODE (x2) != DImode)
    {
      x2 = XEXP (x1, 0);
      goto L1358;
    }
  switch (GET_CODE (x2))
    {
    case ASHIFT:
      goto L918;
    case ASHIFTRT:
      goto L946;
    case LSHIFTRT:
      goto L974;
    }
  x2 = XEXP (x1, 0);
  goto L1358;

  L918:
  x3 = XEXP (x2, 0);
  if (register_operand (x3, DImode))
    {
      ro[1] = x3;
      goto L919;
    }
  x2 = XEXP (x1, 0);
  goto L1358;

  L919:
  x3 = XEXP (x2, 1);
  if (register_operand (x3, QImode))
    {
      ro[2] = x3;
      goto L920;
    }
  x2 = XEXP (x1, 0);
  goto L1358;

  L920:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L921;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1358;

  L921:
  x2 = XEXP (x1, 0);
  if (rtx_equal_p (x2, ro[2]) && 1)
    return 183;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1358;

  L946:
  x3 = XEXP (x2, 0);
  if (register_operand (x3, DImode))
    {
      ro[1] = x3;
      goto L947;
    }
  x2 = XEXP (x1, 0);
  goto L1358;

  L947:
  x3 = XEXP (x2, 1);
  if (register_operand (x3, QImode))
    {
      ro[2] = x3;
      goto L948;
    }
  x2 = XEXP (x1, 0);
  goto L1358;

  L948:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L949;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1358;

  L949:
  x2 = XEXP (x1, 0);
  if (rtx_equal_p (x2, ro[2]) && 1)
    return 189;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1358;

  L974:
  x3 = XEXP (x2, 0);
  if (register_operand (x3, DImode))
    {
      ro[1] = x3;
      goto L975;
    }
  x2 = XEXP (x1, 0);
  goto L1358;

  L975:
  x3 = XEXP (x2, 1);
  if (register_operand (x3, QImode))
    {
      ro[2] = x3;
      goto L976;
    }
  x2 = XEXP (x1, 0);
  goto L1358;

  L976:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L977;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1358;

  L977:
  x2 = XEXP (x1, 0);
  if (rtx_equal_p (x2, ro[2]) && 1)
    return 195;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1358;

  L12:
  x2 = XEXP (x1, 1);
  switch (GET_MODE (x2))
    {
    case SFmode:
      if (register_operand (x2, SFmode))
	{
	  ro[0] = x2;
	  goto L13;
	}
      break;
    case DFmode:
      if (register_operand (x2, DFmode))
	{
	  ro[0] = x2;
	  goto L22;
	}
      break;
    case XFmode:
      if (register_operand (x2, XFmode))
	{
	  ro[0] = x2;
	  goto L31;
	}
    }
  L54:
  if (VOIDmode_compare_op (x2, VOIDmode))
    {
      ro[2] = x2;
      goto L83;
    }
  L127:
  if (GET_MODE (x2) == CCFPEQmode && GET_CODE (x2) == COMPARE && 1)
    goto L128;
  x2 = XEXP (x1, 0);
  goto L1358;

  L13:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L14;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 1);
  goto L54;

  L14:
  x2 = XEXP (x1, 0);
  if (scratch_operand (x2, HImode))
    {
      ro[1] = x2;
      if (TARGET_80387 && ! TARGET_IEEE_FP)
	return 6;
      }
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 1);
  goto L54;

  L22:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L23;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 1);
  goto L54;

  L23:
  x2 = XEXP (x1, 0);
  if (scratch_operand (x2, HImode))
    {
      ro[1] = x2;
      if (TARGET_80387 && ! TARGET_IEEE_FP)
	return 8;
      }
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 1);
  goto L54;

  L31:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L32;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 1);
  goto L54;

  L32:
  x2 = XEXP (x1, 0);
  if (scratch_operand (x2, HImode))
    {
      ro[1] = x2;
      if (TARGET_80387 && ! TARGET_IEEE_FP)
	return 10;
      }
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 1);
  goto L54;
 L83:
  tem = recog_5 (x0, insn, pnum_clobbers);
  if (tem >= 0) return tem;
  goto L127;

  L128:
  x3 = XEXP (x2, 0);
  switch (GET_MODE (x3))
    {
    case XFmode:
      if (register_operand (x3, XFmode))
	{
	  ro[0] = x3;
	  goto L129;
	}
      break;
    case DFmode:
      if (register_operand (x3, DFmode))
	{
	  ro[0] = x3;
	  goto L215;
	}
      break;
    case SFmode:
      if (register_operand (x3, SFmode))
	{
	  ro[0] = x3;
	  goto L271;
	}
    }
  x2 = XEXP (x1, 0);
  goto L1358;

  L129:
  x3 = XEXP (x2, 1);
  if (register_operand (x3, XFmode))
    {
      ro[1] = x3;
      goto L130;
    }
  x2 = XEXP (x1, 0);
  goto L1358;

  L130:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L131;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1358;

  L131:
  x2 = XEXP (x1, 0);
  if (scratch_operand (x2, HImode))
    {
      ro[2] = x2;
      if (TARGET_80387)
	return 23;
      }
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1358;

  L215:
  x3 = XEXP (x2, 1);
  if (register_operand (x3, DFmode))
    {
      ro[1] = x3;
      goto L216;
    }
  x2 = XEXP (x1, 0);
  goto L1358;

  L216:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L217;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1358;

  L217:
  x2 = XEXP (x1, 0);
  if (scratch_operand (x2, HImode))
    {
      ro[2] = x2;
      if (TARGET_80387)
	return 29;
      }
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1358;

  L271:
  x3 = XEXP (x2, 1);
  if (register_operand (x3, SFmode))
    {
      ro[1] = x3;
      goto L272;
    }
  x2 = XEXP (x1, 0);
  goto L1358;

  L272:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L273;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1358;

  L273:
  x2 = XEXP (x1, 0);
  if (scratch_operand (x2, HImode))
    {
      ro[2] = x2;
      if (TARGET_80387)
	return 33;
      }
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1358;

  L1301:
  x2 = XEXP (x1, 1);
  if (GET_MODE (x2) == SImode && GET_CODE (x2) == MINUS && 1)
    goto L1302;
  if (general_operand (x2, SImode))
    {
      ro[0] = x2;
      goto L1327;
    }
  x2 = XEXP (x1, 0);
  goto L1358;

  L1302:
  x3 = XEXP (x2, 0);
  if (GET_MODE (x3) == SImode && GET_CODE (x3) == REG && XINT (x3, 0) == 3 && 1)
    goto L1303;
  x2 = XEXP (x1, 0);
  goto L1358;

  L1303:
  x3 = XEXP (x2, 1);
  if (GET_MODE (x3) == SImode && GET_CODE (x3) == MEM && 1)
    goto L1304;
  x2 = XEXP (x1, 0);
  goto L1358;

  L1304:
  x4 = XEXP (x3, 0);
  if (GET_MODE (x4) == SImode && GET_CODE (x4) == PLUS && 1)
    goto L1305;
  x2 = XEXP (x1, 0);
  goto L1358;

  L1305:
  x5 = XEXP (x4, 0);
  if (GET_MODE (x5) == SImode && GET_CODE (x5) == MULT && 1)
    goto L1306;
  x2 = XEXP (x1, 0);
  goto L1358;

  L1306:
  x6 = XEXP (x5, 0);
  if (register_operand (x6, SImode))
    {
      ro[0] = x6;
      goto L1307;
    }
  x2 = XEXP (x1, 0);
  goto L1358;

  L1307:
  x6 = XEXP (x5, 1);
  if (GET_CODE (x6) == CONST_INT && XWINT (x6, 0) == 4 && 1)
    goto L1308;
  x2 = XEXP (x1, 0);
  goto L1358;

  L1308:
  x5 = XEXP (x4, 1);
  if (GET_CODE (x5) == LABEL_REF && 1)
    goto L1309;
  x2 = XEXP (x1, 0);
  goto L1358;

  L1309:
  x6 = XEXP (x5, 0);
  ro[1] = x6;
  goto L1310;

  L1310:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L1311;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1358;

  L1311:
  x2 = XEXP (x1, 0);
  if (scratch_operand (x2, SImode))
    {
      ro[2] = x2;
      return 264;
    }
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1358;

  L1327:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == USE && 1)
    goto L1328;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1358;

  L1328:
  x2 = XEXP (x1, 0);
  if (GET_CODE (x2) == LABEL_REF && 1)
    goto L1329;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1358;

  L1329:
  x3 = XEXP (x2, 0);
  ro[1] = x3;
  return 265;

  L1359:
  x2 = XEXP (x1, 1);
  if (GET_CODE (x2) == CALL && 1)
    goto L1371;
  x2 = XEXP (x1, 0);
  goto L1548;

  L1371:
  x3 = XEXP (x2, 0);
  if (GET_MODE (x3) == QImode && GET_CODE (x3) == MEM && 1)
    goto L1372;
  L1360:
  if (call_insn_operand (x3, QImode))
    {
      ro[1] = x3;
      goto L1361;
    }
  x2 = XEXP (x1, 0);
  goto L1548;

  L1372:
  x4 = XEXP (x3, 0);
  if (symbolic_operand (x4, SImode))
    {
      ro[1] = x4;
      goto L1373;
    }
  goto L1360;

  L1373:
  x3 = XEXP (x2, 1);
  if (general_operand (x3, SImode))
    {
      ro[2] = x3;
      goto L1374;
    }
  x3 = XEXP (x2, 0);
  goto L1360;

  L1374:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == SET && 1)
    goto L1375;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 1);
  x3 = XEXP (x2, 0);
  goto L1360;

  L1375:
  x2 = XEXP (x1, 0);
  if (GET_MODE (x2) == SImode && GET_CODE (x2) == REG && XINT (x2, 0) == 7 && 1)
    goto L1376;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 1);
  x3 = XEXP (x2, 0);
  goto L1360;

  L1376:
  x2 = XEXP (x1, 1);
  if (GET_MODE (x2) == SImode && GET_CODE (x2) == PLUS && 1)
    goto L1377;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 1);
  x3 = XEXP (x2, 0);
  goto L1360;

  L1377:
  x3 = XEXP (x2, 0);
  if (GET_MODE (x3) == SImode && GET_CODE (x3) == REG && XINT (x3, 0) == 7 && 1)
    goto L1378;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 1);
  x3 = XEXP (x2, 0);
  goto L1360;

  L1378:
  x3 = XEXP (x2, 1);
  if (immediate_operand (x3, SImode))
    {
      ro[4] = x3;
      if (!HALF_PIC_P ())
	return 274;
      }
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 1);
  x3 = XEXP (x2, 0);
  goto L1360;

  L1361:
  x3 = XEXP (x2, 1);
  if (general_operand (x3, SImode))
    {
      ro[2] = x3;
      goto L1362;
    }
  x2 = XEXP (x1, 0);
  goto L1548;

  L1362:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == SET && 1)
    goto L1363;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1548;

  L1363:
  x2 = XEXP (x1, 0);
  if (GET_MODE (x2) == SImode && GET_CODE (x2) == REG && XINT (x2, 0) == 7 && 1)
    goto L1364;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1548;

  L1364:
  x2 = XEXP (x1, 1);
  if (GET_MODE (x2) == SImode && GET_CODE (x2) == PLUS && 1)
    goto L1365;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1548;

  L1365:
  x3 = XEXP (x2, 0);
  if (GET_MODE (x3) == SImode && GET_CODE (x3) == REG && XINT (x3, 0) == 7 && 1)
    goto L1366;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1548;

  L1366:
  x3 = XEXP (x2, 1);
  if (immediate_operand (x3, SImode))
    {
      ro[4] = x3;
      return 273;
    }
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1548;

  L1549:
  x2 = XEXP (x1, 1);
  if (GET_MODE (x2) == SImode && GET_CODE (x2) == UNSPEC && XINT (x2, 1) == 0 && XVECLEN (x2, 0) == 3 && 1)
    goto L1550;
  goto ret0;

  L1550:
  x3 = XVECEXP (x2, 0, 0);
  if (GET_MODE (x3) == BLKmode && GET_CODE (x3) == MEM && 1)
    goto L1551;
  goto ret0;

  L1551:
  x4 = XEXP (x3, 0);
  if (address_operand (x4, SImode))
    {
      ro[1] = x4;
      goto L1552;
    }
  goto ret0;

  L1552:
  x3 = XVECEXP (x2, 0, 1);
  if (register_operand (x3, QImode))
    {
      ro[2] = x3;
      goto L1553;
    }
  goto ret0;

  L1553:
  x3 = XVECEXP (x2, 0, 2);
  if (immediate_operand (x3, SImode))
    {
      ro[3] = x3;
      goto L1554;
    }
  goto ret0;

  L1554:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L1555;
  goto ret0;

  L1555:
  x2 = XEXP (x1, 0);
  if (rtx_equal_p (x2, ro[1]) && 1)
    return 308;
  goto ret0;
 ret0: return -1;
}

int
recog_7 (x0, insn, pnum_clobbers)
     register rtx x0;
     rtx insn;
     int *pnum_clobbers;
{
  register rtx *ro = &recog_operand[0];
  register rtx x1, x2, x3, x4, x5, x6;
  int tem;

  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  switch (GET_MODE (x2))
    {
    case DImode:
      if (general_operand (x2, DImode))
	{
	  ro[0] = x2;
	  goto L439;
	}
      break;
    case SImode:
      if (general_operand (x2, SImode))
	{
	  ro[0] = x2;
	  goto L503;
	}
    }
  goto ret0;

  L439:
  x2 = XEXP (x1, 1);
  if (GET_MODE (x2) == DImode && GET_CODE (x2) == FIX && 1)
    goto L440;
  goto ret0;

  L440:
  x3 = XEXP (x2, 0);
  if (GET_CODE (x3) != FIX)
    goto ret0;
  switch (GET_MODE (x3))
    {
    case XFmode:
      goto L441;
    case DFmode:
      goto L467;
    case SFmode:
      goto L493;
    }
  goto ret0;

  L441:
  x4 = XEXP (x3, 0);
  if (register_operand (x4, XFmode))
    {
      ro[1] = x4;
      goto L442;
    }
  goto ret0;

  L442:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L443;
  goto ret0;

  L443:
  x2 = XEXP (x1, 0);
  if (rtx_equal_p (x2, ro[1]) && 1)
    goto L444;
  goto ret0;

  L444:
  x1 = XVECEXP (x0, 0, 2);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L445;
  goto ret0;

  L445:
  x2 = XEXP (x1, 0);
  if (memory_operand (x2, SImode))
    {
      ro[2] = x2;
      goto L446;
    }
  goto ret0;

  L446:
  x1 = XVECEXP (x0, 0, 3);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L447;
  goto ret0;

  L447:
  x2 = XEXP (x1, 0);
  if (pnum_clobbers != 0 && memory_operand (x2, SImode))
    {
      ro[3] = x2;
      if (TARGET_80387)
	{
	  *pnum_clobbers = 1;
	  return 87;
	}
      }
  goto ret0;

  L467:
  x4 = XEXP (x3, 0);
  if (register_operand (x4, DFmode))
    {
      ro[1] = x4;
      goto L468;
    }
  goto ret0;

  L468:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L469;
  goto ret0;

  L469:
  x2 = XEXP (x1, 0);
  if (rtx_equal_p (x2, ro[1]) && 1)
    goto L470;
  goto ret0;

  L470:
  x1 = XVECEXP (x0, 0, 2);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L471;
  goto ret0;

  L471:
  x2 = XEXP (x1, 0);
  if (memory_operand (x2, SImode))
    {
      ro[2] = x2;
      goto L472;
    }
  goto ret0;

  L472:
  x1 = XVECEXP (x0, 0, 3);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L473;
  goto ret0;

  L473:
  x2 = XEXP (x1, 0);
  if (pnum_clobbers != 0 && memory_operand (x2, SImode))
    {
      ro[3] = x2;
      if (TARGET_80387)
	{
	  *pnum_clobbers = 1;
	  return 88;
	}
      }
  goto ret0;

  L493:
  x4 = XEXP (x3, 0);
  if (register_operand (x4, SFmode))
    {
      ro[1] = x4;
      goto L494;
    }
  goto ret0;

  L494:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L495;
  goto ret0;

  L495:
  x2 = XEXP (x1, 0);
  if (rtx_equal_p (x2, ro[1]) && 1)
    goto L496;
  goto ret0;

  L496:
  x1 = XVECEXP (x0, 0, 2);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L497;
  goto ret0;

  L497:
  x2 = XEXP (x1, 0);
  if (memory_operand (x2, SImode))
    {
      ro[2] = x2;
      goto L498;
    }
  goto ret0;

  L498:
  x1 = XVECEXP (x0, 0, 3);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L499;
  goto ret0;

  L499:
  x2 = XEXP (x1, 0);
  if (pnum_clobbers != 0 && memory_operand (x2, SImode))
    {
      ro[3] = x2;
      if (TARGET_80387)
	{
	  *pnum_clobbers = 1;
	  return 89;
	}
      }
  goto ret0;

  L503:
  x2 = XEXP (x1, 1);
  if (GET_MODE (x2) == SImode && GET_CODE (x2) == FIX && 1)
    goto L504;
  goto ret0;

  L504:
  x3 = XEXP (x2, 0);
  if (GET_CODE (x3) != FIX)
    goto ret0;
  switch (GET_MODE (x3))
    {
    case XFmode:
      goto L505;
    case DFmode:
      goto L527;
    case SFmode:
      goto L549;
    }
  goto ret0;

  L505:
  x4 = XEXP (x3, 0);
  if (register_operand (x4, XFmode))
    {
      ro[1] = x4;
      goto L506;
    }
  goto ret0;

  L506:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L507;
  goto ret0;

  L507:
  x2 = XEXP (x1, 0);
  if (memory_operand (x2, SImode))
    {
      ro[2] = x2;
      goto L508;
    }
  goto ret0;

  L508:
  x1 = XVECEXP (x0, 0, 2);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L509;
  goto ret0;

  L509:
  x2 = XEXP (x1, 0);
  if (memory_operand (x2, SImode))
    {
      ro[3] = x2;
      goto L510;
    }
  goto ret0;

  L510:
  x1 = XVECEXP (x0, 0, 3);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L511;
  goto ret0;

  L511:
  x2 = XEXP (x1, 0);
  if (scratch_operand (x2, SImode))
    {
      ro[4] = x2;
      if (TARGET_80387)
	return 93;
      }
  goto ret0;

  L527:
  x4 = XEXP (x3, 0);
  if (register_operand (x4, DFmode))
    {
      ro[1] = x4;
      goto L528;
    }
  goto ret0;

  L528:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L529;
  goto ret0;

  L529:
  x2 = XEXP (x1, 0);
  if (memory_operand (x2, SImode))
    {
      ro[2] = x2;
      goto L530;
    }
  goto ret0;

  L530:
  x1 = XVECEXP (x0, 0, 2);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L531;
  goto ret0;

  L531:
  x2 = XEXP (x1, 0);
  if (memory_operand (x2, SImode))
    {
      ro[3] = x2;
      goto L532;
    }
  goto ret0;

  L532:
  x1 = XVECEXP (x0, 0, 3);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L533;
  goto ret0;

  L533:
  x2 = XEXP (x1, 0);
  if (scratch_operand (x2, SImode))
    {
      ro[4] = x2;
      if (TARGET_80387)
	return 94;
      }
  goto ret0;

  L549:
  x4 = XEXP (x3, 0);
  if (register_operand (x4, SFmode))
    {
      ro[1] = x4;
      goto L550;
    }
  goto ret0;

  L550:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L551;
  goto ret0;

  L551:
  x2 = XEXP (x1, 0);
  if (memory_operand (x2, SImode))
    {
      ro[2] = x2;
      goto L552;
    }
  goto ret0;

  L552:
  x1 = XVECEXP (x0, 0, 2);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L553;
  goto ret0;

  L553:
  x2 = XEXP (x1, 0);
  if (memory_operand (x2, SImode))
    {
      ro[3] = x2;
      goto L554;
    }
  goto ret0;

  L554:
  x1 = XVECEXP (x0, 0, 3);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L555;
  goto ret0;

  L555:
  x2 = XEXP (x1, 0);
  if (scratch_operand (x2, SImode))
    {
      ro[4] = x2;
      if (TARGET_80387)
	return 95;
      }
  goto ret0;
 ret0: return -1;
}

int
recog (x0, insn, pnum_clobbers)
     register rtx x0;
     rtx insn;
     int *pnum_clobbers;
{
  register rtx *ro = &recog_operand[0];
  register rtx x1, x2, x3, x4, x5, x6;
  int tem;

  L1403:
  switch (GET_CODE (x0))
    {
    case UNSPEC:
      if (GET_MODE (x0) == SImode && XINT (x0, 1) == 0 && XVECLEN (x0, 0) == 1 && 1)
	goto L1404;
      break;
    case SET:
      goto L295;
    case PARALLEL:
      if (XVECLEN (x0, 0) == 2 && 1)
	goto L10;
      if (XVECLEN (x0, 0) == 5 && 1)
	goto L423;
      if (XVECLEN (x0, 0) == 4 && 1)
	goto L437;
      if (XVECLEN (x0, 0) == 3 && 1)
	goto L513;
      if (XVECLEN (x0, 0) == 6 && 1)
	goto L1408;
      break;
    case CALL:
      goto L1350;
    case RETURN:
      if (simple_386_epilogue ())
	return 283;
      break;
    case CONST_INT:
      if (XWINT (x0, 0) == 0 && 1)
	return 284;
    }
  goto ret0;

  L1404:
  x1 = XVECEXP (x0, 0, 0);
  if (memory_operand (x1, SImode))
    {
      ro[0] = x1;
      return 282;
    }
  goto ret0;
 L295:
  return recog_4 (x0, insn, pnum_clobbers);

  L10:
  x1 = XVECEXP (x0, 0, 0);
  switch (GET_CODE (x1))
    {
    case SET:
      goto L336;
    case CALL:
      goto L1341;
    }
  goto ret0;
 L336:
  return recog_6 (x0, insn, pnum_clobbers);

  L1341:
  x2 = XEXP (x1, 0);
  if (GET_MODE (x2) == QImode && GET_CODE (x2) == MEM && 1)
    goto L1342;
  L1332:
  if (call_insn_operand (x2, QImode))
    {
      ro[0] = x2;
      goto L1333;
    }
  goto ret0;

  L1342:
  x3 = XEXP (x2, 0);
  if (symbolic_operand (x3, SImode))
    {
      ro[0] = x3;
      goto L1343;
    }
  goto L1332;

  L1343:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, SImode))
    {
      ro[1] = x2;
      goto L1344;
    }
  x2 = XEXP (x1, 0);
  goto L1332;

  L1344:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == SET && 1)
    goto L1345;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1332;

  L1345:
  x2 = XEXP (x1, 0);
  if (GET_MODE (x2) == SImode && GET_CODE (x2) == REG && XINT (x2, 0) == 7 && 1)
    goto L1346;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1332;

  L1346:
  x2 = XEXP (x1, 1);
  if (GET_MODE (x2) == SImode && GET_CODE (x2) == PLUS && 1)
    goto L1347;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1332;

  L1347:
  x3 = XEXP (x2, 0);
  if (GET_MODE (x3) == SImode && GET_CODE (x3) == REG && XINT (x3, 0) == 7 && 1)
    goto L1348;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1332;

  L1348:
  x3 = XEXP (x2, 1);
  if (immediate_operand (x3, SImode))
    {
      ro[3] = x3;
      if (!HALF_PIC_P ())
	return 268;
      }
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1332;

  L1333:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, SImode))
    {
      ro[1] = x2;
      goto L1334;
    }
  goto ret0;

  L1334:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == SET && 1)
    goto L1335;
  goto ret0;

  L1335:
  x2 = XEXP (x1, 0);
  if (GET_MODE (x2) == SImode && GET_CODE (x2) == REG && XINT (x2, 0) == 7 && 1)
    goto L1336;
  goto ret0;

  L1336:
  x2 = XEXP (x1, 1);
  if (GET_MODE (x2) == SImode && GET_CODE (x2) == PLUS && 1)
    goto L1337;
  goto ret0;

  L1337:
  x3 = XEXP (x2, 0);
  if (GET_MODE (x3) == SImode && GET_CODE (x3) == REG && XINT (x3, 0) == 7 && 1)
    goto L1338;
  goto ret0;

  L1338:
  x3 = XEXP (x2, 1);
  if (immediate_operand (x3, SImode))
    {
      ro[3] = x3;
      return 267;
    }
  goto ret0;

  L423:
  x1 = XVECEXP (x0, 0, 0);
  if (GET_CODE (x1) == SET && 1)
    goto L424;
  goto ret0;

  L424:
  x2 = XEXP (x1, 0);
  if (GET_MODE (x2) == DImode && general_operand (x2, DImode))
    {
      ro[0] = x2;
      goto L425;
    }
  goto ret0;

  L425:
  x2 = XEXP (x1, 1);
  if (GET_MODE (x2) == DImode && GET_CODE (x2) == FIX && 1)
    goto L426;
  goto ret0;

  L426:
  x3 = XEXP (x2, 0);
  if (GET_CODE (x3) != FIX)
    goto ret0;
  switch (GET_MODE (x3))
    {
    case XFmode:
      goto L427;
    case DFmode:
      goto L453;
    case SFmode:
      goto L479;
    }
  goto ret0;

  L427:
  x4 = XEXP (x3, 0);
  if (register_operand (x4, XFmode))
    {
      ro[1] = x4;
      goto L428;
    }
  goto ret0;

  L428:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L429;
  goto ret0;

  L429:
  x2 = XEXP (x1, 0);
  if (rtx_equal_p (x2, ro[1]) && 1)
    goto L430;
  goto ret0;

  L430:
  x1 = XVECEXP (x0, 0, 2);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L431;
  goto ret0;

  L431:
  x2 = XEXP (x1, 0);
  if (memory_operand (x2, SImode))
    {
      ro[2] = x2;
      goto L432;
    }
  goto ret0;

  L432:
  x1 = XVECEXP (x0, 0, 3);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L433;
  goto ret0;

  L433:
  x2 = XEXP (x1, 0);
  if (memory_operand (x2, SImode))
    {
      ro[3] = x2;
      goto L434;
    }
  goto ret0;

  L434:
  x1 = XVECEXP (x0, 0, 4);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L435;
  goto ret0;

  L435:
  x2 = XEXP (x1, 0);
  if (scratch_operand (x2, SImode))
    {
      ro[4] = x2;
      if (TARGET_80387)
	return 87;
      }
  goto ret0;

  L453:
  x4 = XEXP (x3, 0);
  if (register_operand (x4, DFmode))
    {
      ro[1] = x4;
      goto L454;
    }
  goto ret0;

  L454:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L455;
  goto ret0;

  L455:
  x2 = XEXP (x1, 0);
  if (rtx_equal_p (x2, ro[1]) && 1)
    goto L456;
  goto ret0;

  L456:
  x1 = XVECEXP (x0, 0, 2);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L457;
  goto ret0;

  L457:
  x2 = XEXP (x1, 0);
  if (memory_operand (x2, SImode))
    {
      ro[2] = x2;
      goto L458;
    }
  goto ret0;

  L458:
  x1 = XVECEXP (x0, 0, 3);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L459;
  goto ret0;

  L459:
  x2 = XEXP (x1, 0);
  if (memory_operand (x2, SImode))
    {
      ro[3] = x2;
      goto L460;
    }
  goto ret0;

  L460:
  x1 = XVECEXP (x0, 0, 4);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L461;
  goto ret0;

  L461:
  x2 = XEXP (x1, 0);
  if (scratch_operand (x2, SImode))
    {
      ro[4] = x2;
      if (TARGET_80387)
	return 88;
      }
  goto ret0;

  L479:
  x4 = XEXP (x3, 0);
  if (register_operand (x4, SFmode))
    {
      ro[1] = x4;
      goto L480;
    }
  goto ret0;

  L480:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L481;
  goto ret0;

  L481:
  x2 = XEXP (x1, 0);
  if (rtx_equal_p (x2, ro[1]) && 1)
    goto L482;
  goto ret0;

  L482:
  x1 = XVECEXP (x0, 0, 2);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L483;
  goto ret0;

  L483:
  x2 = XEXP (x1, 0);
  if (memory_operand (x2, SImode))
    {
      ro[2] = x2;
      goto L484;
    }
  goto ret0;

  L484:
  x1 = XVECEXP (x0, 0, 3);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L485;
  goto ret0;

  L485:
  x2 = XEXP (x1, 0);
  if (memory_operand (x2, SImode))
    {
      ro[3] = x2;
      goto L486;
    }
  goto ret0;

  L486:
  x1 = XVECEXP (x0, 0, 4);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L487;
  goto ret0;

  L487:
  x2 = XEXP (x1, 0);
  if (scratch_operand (x2, SImode))
    {
      ro[4] = x2;
      if (TARGET_80387)
	return 89;
      }
  goto ret0;

  L437:
  x1 = XVECEXP (x0, 0, 0);
  if (GET_CODE (x1) == SET && 1)
    goto L438;
  goto ret0;
 L438:
  return recog_7 (x0, insn, pnum_clobbers);

  L513:
  x1 = XVECEXP (x0, 0, 0);
  switch (GET_CODE (x1))
    {
    case SET:
      goto L514;
    case CALL:
      goto L1398;
    }
  goto ret0;

  L514:
  x2 = XEXP (x1, 0);
  if (GET_MODE (x2) == SImode && general_operand (x2, SImode))
    {
      ro[0] = x2;
      goto L515;
    }
  goto ret0;

  L515:
  x2 = XEXP (x1, 1);
  if (GET_MODE (x2) == SImode && GET_CODE (x2) == FIX && 1)
    goto L516;
  goto ret0;

  L516:
  x3 = XEXP (x2, 0);
  if (GET_CODE (x3) != FIX)
    goto ret0;
  switch (GET_MODE (x3))
    {
    case XFmode:
      goto L517;
    case DFmode:
      goto L539;
    case SFmode:
      goto L561;
    }
  goto ret0;

  L517:
  x4 = XEXP (x3, 0);
  if (register_operand (x4, XFmode))
    {
      ro[1] = x4;
      goto L518;
    }
  goto ret0;

  L518:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L519;
  goto ret0;

  L519:
  x2 = XEXP (x1, 0);
  if (memory_operand (x2, SImode))
    {
      ro[2] = x2;
      goto L520;
    }
  goto ret0;

  L520:
  x1 = XVECEXP (x0, 0, 2);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L521;
  goto ret0;

  L521:
  x2 = XEXP (x1, 0);
  if (pnum_clobbers != 0 && memory_operand (x2, SImode))
    {
      ro[3] = x2;
      if (TARGET_80387)
	{
	  *pnum_clobbers = 1;
	  return 93;
	}
      }
  goto ret0;

  L539:
  x4 = XEXP (x3, 0);
  if (register_operand (x4, DFmode))
    {
      ro[1] = x4;
      goto L540;
    }
  goto ret0;

  L540:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L541;
  goto ret0;

  L541:
  x2 = XEXP (x1, 0);
  if (memory_operand (x2, SImode))
    {
      ro[2] = x2;
      goto L542;
    }
  goto ret0;

  L542:
  x1 = XVECEXP (x0, 0, 2);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L543;
  goto ret0;

  L543:
  x2 = XEXP (x1, 0);
  if (pnum_clobbers != 0 && memory_operand (x2, SImode))
    {
      ro[3] = x2;
      if (TARGET_80387)
	{
	  *pnum_clobbers = 1;
	  return 94;
	}
      }
  goto ret0;

  L561:
  x4 = XEXP (x3, 0);
  if (register_operand (x4, SFmode))
    {
      ro[1] = x4;
      goto L562;
    }
  goto ret0;

  L562:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L563;
  goto ret0;

  L563:
  x2 = XEXP (x1, 0);
  if (memory_operand (x2, SImode))
    {
      ro[2] = x2;
      goto L564;
    }
  goto ret0;

  L564:
  x1 = XVECEXP (x0, 0, 2);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L565;
  goto ret0;

  L565:
  x2 = XEXP (x1, 0);
  if (pnum_clobbers != 0 && memory_operand (x2, SImode))
    {
      ro[3] = x2;
      if (TARGET_80387)
	{
	  *pnum_clobbers = 1;
	  return 95;
	}
      }
  goto ret0;

  L1398:
  x2 = XEXP (x1, 0);
  if (GET_MODE (x2) == QImode && GET_CODE (x2) == MEM && 1)
    goto L1399;
  L1392:
  if (call_insn_operand (x2, QImode))
    {
      ro[0] = x2;
      goto L1393;
    }
  goto ret0;

  L1399:
  x3 = XEXP (x2, 0);
  if (symbolic_operand (x3, SImode))
    {
      ro[0] = x3;
      goto L1400;
    }
  goto L1392;

  L1400:
  x2 = XEXP (x1, 1);
  if (GET_CODE (x2) == CONST_INT && XWINT (x2, 0) == 0 && 1)
    goto L1401;
  x2 = XEXP (x1, 0);
  goto L1392;

  L1401:
  x1 = XVECEXP (x0, 0, 1);
  if (memory_operand (x1, DImode))
    {
      ro[1] = x1;
      goto L1402;
    }
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1392;

  L1402:
  x1 = XVECEXP (x0, 0, 2);
  ro[2] = x1;
  if (!HALF_PIC_P ())
    return 280;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1392;

  L1393:
  x2 = XEXP (x1, 1);
  if (GET_CODE (x2) == CONST_INT && XWINT (x2, 0) == 0 && 1)
    goto L1394;
  goto ret0;

  L1394:
  x1 = XVECEXP (x0, 0, 1);
  if (memory_operand (x1, DImode))
    {
      ro[1] = x1;
      goto L1395;
    }
  goto ret0;

  L1395:
  x1 = XVECEXP (x0, 0, 2);
  ro[2] = x1;
  return 279;

  L1408:
  x1 = XVECEXP (x0, 0, 0);
  if (GET_CODE (x1) == SET && 1)
    goto L1409;
  goto ret0;

  L1409:
  x2 = XEXP (x1, 0);
  switch (GET_MODE (x2))
    {
    case BLKmode:
      if (GET_CODE (x2) == MEM && 1)
	goto L1410;
      break;
    case SImode:
      if (general_operand (x2, SImode))
	{
	  ro[0] = x2;
	  goto L1426;
	}
    }
  if (GET_CODE (x2) == CC0 && 1)
    goto L1444;
  goto ret0;

  L1410:
  x3 = XEXP (x2, 0);
  if (address_operand (x3, SImode))
    {
      ro[0] = x3;
      goto L1411;
    }
  goto ret0;

  L1411:
  x2 = XEXP (x1, 1);
  if (GET_MODE (x2) == BLKmode && GET_CODE (x2) == MEM && 1)
    goto L1412;
  goto ret0;

  L1412:
  x3 = XEXP (x2, 0);
  if (address_operand (x3, SImode))
    {
      ro[1] = x3;
      goto L1413;
    }
  goto ret0;

  L1413:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == USE && 1)
    goto L1414;
  goto ret0;

  L1414:
  x2 = XEXP (x1, 0);
  if (GET_CODE (x2) == CONST_INT && 1)
    {
      ro[2] = x2;
      goto L1415;
    }
  goto ret0;

  L1415:
  x1 = XVECEXP (x0, 0, 2);
  if (GET_CODE (x1) == USE && 1)
    goto L1416;
  goto ret0;

  L1416:
  x2 = XEXP (x1, 0);
  if (immediate_operand (x2, SImode))
    {
      ro[3] = x2;
      goto L1417;
    }
  goto ret0;

  L1417:
  x1 = XVECEXP (x0, 0, 3);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L1418;
  goto ret0;

  L1418:
  x2 = XEXP (x1, 0);
  if (scratch_operand (x2, SImode))
    {
      ro[4] = x2;
      goto L1419;
    }
  goto ret0;

  L1419:
  x1 = XVECEXP (x0, 0, 4);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L1420;
  goto ret0;

  L1420:
  x2 = XEXP (x1, 0);
  if (rtx_equal_p (x2, ro[0]) && 1)
    goto L1421;
  goto ret0;

  L1421:
  x1 = XVECEXP (x0, 0, 5);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L1422;
  goto ret0;

  L1422:
  x2 = XEXP (x1, 0);
  if (rtx_equal_p (x2, ro[1]) && 1)
    return 286;
  goto ret0;

  L1426:
  x2 = XEXP (x1, 1);
  if (GET_MODE (x2) == SImode && GET_CODE (x2) == COMPARE && 1)
    goto L1427;
  goto ret0;

  L1427:
  x3 = XEXP (x2, 0);
  if (GET_MODE (x3) == BLKmode && GET_CODE (x3) == MEM && 1)
    goto L1428;
  goto ret0;

  L1428:
  x4 = XEXP (x3, 0);
  if (address_operand (x4, SImode))
    {
      ro[1] = x4;
      goto L1429;
    }
  goto ret0;

  L1429:
  x3 = XEXP (x2, 1);
  if (GET_MODE (x3) == BLKmode && GET_CODE (x3) == MEM && 1)
    goto L1430;
  goto ret0;

  L1430:
  x4 = XEXP (x3, 0);
  if (address_operand (x4, SImode))
    {
      ro[2] = x4;
      goto L1431;
    }
  goto ret0;

  L1431:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == USE && 1)
    goto L1432;
  goto ret0;

  L1432:
  x2 = XEXP (x1, 0);
  if (register_operand (x2, SImode))
    {
      ro[3] = x2;
      goto L1433;
    }
  goto ret0;

  L1433:
  x1 = XVECEXP (x0, 0, 2);
  if (GET_CODE (x1) == USE && 1)
    goto L1434;
  goto ret0;

  L1434:
  x2 = XEXP (x1, 0);
  if (immediate_operand (x2, SImode))
    {
      ro[4] = x2;
      goto L1435;
    }
  goto ret0;

  L1435:
  x1 = XVECEXP (x0, 0, 3);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L1436;
  goto ret0;

  L1436:
  x2 = XEXP (x1, 0);
  if (rtx_equal_p (x2, ro[1]) && 1)
    goto L1437;
  goto ret0;

  L1437:
  x1 = XVECEXP (x0, 0, 4);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L1438;
  goto ret0;

  L1438:
  x2 = XEXP (x1, 0);
  if (rtx_equal_p (x2, ro[2]) && 1)
    goto L1439;
  goto ret0;

  L1439:
  x1 = XVECEXP (x0, 0, 5);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L1440;
  goto ret0;

  L1440:
  x2 = XEXP (x1, 0);
  if (rtx_equal_p (x2, ro[3]) && 1)
    return 288;
  goto ret0;

  L1444:
  x2 = XEXP (x1, 1);
  if (GET_MODE (x2) == SImode && GET_CODE (x2) == COMPARE && 1)
    goto L1445;
  goto ret0;

  L1445:
  x3 = XEXP (x2, 0);
  if (GET_MODE (x3) == BLKmode && GET_CODE (x3) == MEM && 1)
    goto L1446;
  goto ret0;

  L1446:
  x4 = XEXP (x3, 0);
  if (address_operand (x4, SImode))
    {
      ro[0] = x4;
      goto L1447;
    }
  goto ret0;

  L1447:
  x3 = XEXP (x2, 1);
  if (GET_MODE (x3) == BLKmode && GET_CODE (x3) == MEM && 1)
    goto L1448;
  goto ret0;

  L1448:
  x4 = XEXP (x3, 0);
  if (address_operand (x4, SImode))
    {
      ro[1] = x4;
      goto L1449;
    }
  goto ret0;

  L1449:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == USE && 1)
    goto L1450;
  goto ret0;

  L1450:
  x2 = XEXP (x1, 0);
  if (register_operand (x2, SImode))
    {
      ro[2] = x2;
      goto L1451;
    }
  goto ret0;

  L1451:
  x1 = XVECEXP (x0, 0, 2);
  if (GET_CODE (x1) == USE && 1)
    goto L1452;
  goto ret0;

  L1452:
  x2 = XEXP (x1, 0);
  if (immediate_operand (x2, SImode))
    {
      ro[3] = x2;
      goto L1453;
    }
  goto ret0;

  L1453:
  x1 = XVECEXP (x0, 0, 3);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L1454;
  goto ret0;

  L1454:
  x2 = XEXP (x1, 0);
  if (rtx_equal_p (x2, ro[0]) && 1)
    goto L1455;
  goto ret0;

  L1455:
  x1 = XVECEXP (x0, 0, 4);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L1456;
  goto ret0;

  L1456:
  x2 = XEXP (x1, 0);
  if (rtx_equal_p (x2, ro[1]) && 1)
    goto L1457;
  goto ret0;

  L1457:
  x1 = XVECEXP (x0, 0, 5);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L1458;
  goto ret0;

  L1458:
  x2 = XEXP (x1, 0);
  if (rtx_equal_p (x2, ro[2]) && 1)
    return 289;
  goto ret0;

  L1350:
  x1 = XEXP (x0, 0);
  if (call_insn_operand (x1, QImode))
    {
      ro[0] = x1;
      goto L1351;
    }
  L1353:
  if (GET_MODE (x1) == QImode && GET_CODE (x1) == MEM && 1)
    goto L1354;
  goto ret0;

  L1351:
  x1 = XEXP (x0, 1);
  if (general_operand (x1, SImode))
    {
      ro[1] = x1;
      return 270;
    }
  x1 = XEXP (x0, 0);
  goto L1353;

  L1354:
  x2 = XEXP (x1, 0);
  if (symbolic_operand (x2, SImode))
    {
      ro[0] = x2;
      goto L1355;
    }
  goto ret0;

  L1355:
  x1 = XEXP (x0, 1);
  if (general_operand (x1, SImode))
    {
      ro[1] = x1;
      if (!HALF_PIC_P ())
	return 271;
      }
  goto ret0;
 ret0: return -1;
}

rtx
split_insns (x0, insn)
     register rtx x0;
     rtx insn;
{
  register rtx *ro = &recog_operand[0];
  register rtx x1, x2, x3, x4, x5, x6;
  rtx tem;

  goto ret0;
 ret0: return 0;
}

