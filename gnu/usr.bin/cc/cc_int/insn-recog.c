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
	  goto L474;
	case SIGN_EXTEND:
	  goto L494;
	case PLUS:
	  goto L715;
	case MINUS:
	  goto L746;
	case MULT:
	  goto L780;
	case AND:
	  goto L913;
	case IOR:
	  goto L928;
	case XOR:
	  goto L943;
	case NEG:
	  goto L961;
	case NOT:
	  goto L1070;
	case ASHIFT:
	  goto L1096;
	case ASHIFTRT:
	  goto L1124;
	case LSHIFTRT:
	  goto L1152;
	case ROTATE:
	  goto L1167;
	case ROTATERT:
	  goto L1182;
	}
    }
  if (general_operand (x1, HImode))
    {
      ro[1] = x1;
      if ((!TARGET_MOVE || GET_CODE (operands[0]) != MEM) || (GET_CODE (operands[1]) != MEM))
	return 55;
      }
  goto ret0;

  L474:
  x2 = XEXP (x1, 0);
  if (nonimmediate_operand (x2, QImode))
    {
      ro[1] = x2;
      return 86;
    }
  goto ret0;

  L494:
  x2 = XEXP (x1, 0);
  if (nonimmediate_operand (x2, QImode))
    {
      ro[1] = x2;
      return 91;
    }
  goto ret0;

  L715:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, HImode))
    {
      ro[1] = x2;
      goto L716;
    }
  goto ret0;

  L716:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, HImode))
    {
      ro[2] = x2;
      return 129;
    }
  goto ret0;

  L746:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, HImode))
    {
      ro[1] = x2;
      goto L747;
    }
  goto ret0;

  L747:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, HImode))
    {
      ro[2] = x2;
      return 137;
    }
  goto ret0;

  L780:
  x2 = XEXP (x1, 0);
  switch (GET_MODE (x2))
    {
    case HImode:
      switch (GET_CODE (x2))
	{
	case ZERO_EXTEND:
	  goto L781;
	case SIGN_EXTEND:
	  goto L788;
	}
    }
  if (general_operand (x2, HImode))
    {
      ro[1] = x2;
      goto L757;
    }
  goto ret0;

  L781:
  x3 = XEXP (x2, 0);
  if (nonimmediate_operand (x3, QImode))
    {
      ro[1] = x3;
      goto L782;
    }
  goto ret0;

  L782:
  x2 = XEXP (x1, 1);
  if (GET_MODE (x2) == HImode && GET_CODE (x2) == ZERO_EXTEND && 1)
    goto L783;
  goto ret0;

  L783:
  x3 = XEXP (x2, 0);
  if (nonimmediate_operand (x3, QImode))
    {
      ro[2] = x3;
      return 146;
    }
  goto ret0;

  L788:
  x3 = XEXP (x2, 0);
  if (nonimmediate_operand (x3, QImode))
    {
      ro[1] = x3;
      goto L789;
    }
  goto ret0;

  L789:
  x2 = XEXP (x1, 1);
  if (GET_MODE (x2) == HImode && GET_CODE (x2) == SIGN_EXTEND && 1)
    goto L790;
  goto ret0;

  L790:
  x3 = XEXP (x2, 0);
  if (nonimmediate_operand (x3, QImode))
    {
      ro[2] = x3;
      return 147;
    }
  goto ret0;

  L757:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, HImode))
    goto L763;
  goto ret0;

  L763:
  ro[2] = x2;
  if (GET_CODE (operands[2]) == CONST_INT && INTVAL (operands[2]) == 0x80)
    return 142;
  L764:
  ro[2] = x2;
  return 143;

  L913:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, HImode))
    {
      ro[1] = x2;
      goto L914;
    }
  goto ret0;

  L914:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, HImode))
    {
      ro[2] = x2;
      return 165;
    }
  goto ret0;

  L928:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, HImode))
    {
      ro[1] = x2;
      goto L929;
    }
  goto ret0;

  L929:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, HImode))
    {
      ro[2] = x2;
      return 168;
    }
  goto ret0;

  L943:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, HImode))
    {
      ro[1] = x2;
      goto L944;
    }
  goto ret0;

  L944:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, HImode))
    {
      ro[2] = x2;
      return 171;
    }
  goto ret0;

  L961:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, HImode))
    {
      ro[1] = x2;
      return 175;
    }
  goto ret0;

  L1070:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, HImode))
    {
      ro[1] = x2;
      return 200;
    }
  goto ret0;

  L1096:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, HImode))
    {
      ro[1] = x2;
      goto L1097;
    }
  goto ret0;

  L1097:
  x2 = XEXP (x1, 1);
  if (nonmemory_operand (x2, HImode))
    {
      ro[2] = x2;
      return 206;
    }
  goto ret0;

  L1124:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, HImode))
    {
      ro[1] = x2;
      goto L1125;
    }
  goto ret0;

  L1125:
  x2 = XEXP (x1, 1);
  if (nonmemory_operand (x2, HImode))
    {
      ro[2] = x2;
      return 212;
    }
  goto ret0;

  L1152:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, HImode))
    {
      ro[1] = x2;
      goto L1153;
    }
  goto ret0;

  L1153:
  x2 = XEXP (x1, 1);
  if (nonmemory_operand (x2, HImode))
    {
      ro[2] = x2;
      return 218;
    }
  goto ret0;

  L1167:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, HImode))
    {
      ro[1] = x2;
      goto L1168;
    }
  goto ret0;

  L1168:
  x2 = XEXP (x1, 1);
  if (nonmemory_operand (x2, HImode))
    {
      ro[2] = x2;
      return 221;
    }
  goto ret0;

  L1182:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, HImode))
    {
      ro[1] = x2;
      goto L1183;
    }
  goto ret0;

  L1183:
  x2 = XEXP (x1, 1);
  if (nonmemory_operand (x2, HImode))
    {
      ro[2] = x2;
      return 224;
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
      goto L1212;
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

  L1212:
  x2 = XEXP (x1, 0);
  switch (GET_MODE (x2))
    {
    case SImode:
      if (register_operand (x2, SImode))
	{
	  ro[0] = x2;
	  goto L1213;
	}
      break;
    case QImode:
      if (general_operand (x2, QImode))
	{
	  ro[0] = x2;
	  goto L1225;
	}
    }
  goto L61;

  L1213:
  x2 = XEXP (x1, 1);
  if (GET_CODE (x2) != CONST_INT)
    {
    goto L61;
    }
  if (XWINT (x2, 0) == 1 && 1)
    goto L1214;
  L1219:
  ro[1] = x2;
  goto L1220;

  L1214:
  x2 = XEXP (x1, 2);
  if (general_operand (x2, SImode))
    {
      ro[1] = x2;
      if (GET_CODE (operands[1]) != CONST_INT)
	return 229;
      }
  x2 = XEXP (x1, 1);
  goto L1219;

  L1220:
  x2 = XEXP (x1, 2);
  if (GET_CODE (x2) == CONST_INT && 1)
    {
      ro[2] = x2;
      return 230;
    }
  goto L61;

  L1225:
  x2 = XEXP (x1, 1);
  if (GET_CODE (x2) == CONST_INT && 1)
    {
      ro[1] = x2;
      goto L1226;
    }
  goto L61;

  L1226:
  x2 = XEXP (x1, 2);
  if (GET_CODE (x2) == CONST_INT && 1)
    {
      ro[2] = x2;
      if (GET_CODE (operands[0]) != MEM || ! MEM_VOLATILE_P (operands[0]))
	return 231;
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
      goto L1281;
    case NE:
      goto L1290;
    case GT:
      goto L1299;
    case GTU:
      goto L1308;
    case LT:
      goto L1317;
    case LTU:
      goto L1326;
    case GE:
      goto L1335;
    case GEU:
      goto L1344;
    case LE:
      goto L1353;
    case LEU:
      goto L1362;
    }
  goto ret0;

  L1281:
  x3 = XEXP (x2, 0);
  if (GET_CODE (x3) == CC0 && 1)
    goto L1282;
  goto ret0;

  L1282:
  x3 = XEXP (x2, 1);
  if (GET_CODE (x3) == CONST_INT && XWINT (x3, 0) == 0 && 1)
    goto L1283;
  goto ret0;

  L1283:
  x2 = XEXP (x1, 1);
  switch (GET_CODE (x2))
    {
    case LABEL_REF:
      goto L1284;
    case PC:
      goto L1374;
    }
  goto ret0;

  L1284:
  x3 = XEXP (x2, 0);
  ro[0] = x3;
  goto L1285;

  L1285:
  x2 = XEXP (x1, 2);
  if (GET_CODE (x2) == PC && 1)
    return 253;
  goto ret0;

  L1374:
  x2 = XEXP (x1, 2);
  if (GET_CODE (x2) == LABEL_REF && 1)
    goto L1375;
  goto ret0;

  L1375:
  x3 = XEXP (x2, 0);
  ro[0] = x3;
  return 272;

  L1290:
  x3 = XEXP (x2, 0);
  if (GET_CODE (x3) == CC0 && 1)
    goto L1291;
  goto ret0;

  L1291:
  x3 = XEXP (x2, 1);
  if (GET_CODE (x3) == CONST_INT && XWINT (x3, 0) == 0 && 1)
    goto L1292;
  goto ret0;

  L1292:
  x2 = XEXP (x1, 1);
  switch (GET_CODE (x2))
    {
    case LABEL_REF:
      goto L1293;
    case PC:
      goto L1383;
    }
  goto ret0;

  L1293:
  x3 = XEXP (x2, 0);
  ro[0] = x3;
  goto L1294;

  L1294:
  x2 = XEXP (x1, 2);
  if (GET_CODE (x2) == PC && 1)
    return 255;
  goto ret0;

  L1383:
  x2 = XEXP (x1, 2);
  if (GET_CODE (x2) == LABEL_REF && 1)
    goto L1384;
  goto ret0;

  L1384:
  x3 = XEXP (x2, 0);
  ro[0] = x3;
  return 273;

  L1299:
  x3 = XEXP (x2, 0);
  if (GET_CODE (x3) == CC0 && 1)
    goto L1300;
  goto ret0;

  L1300:
  x3 = XEXP (x2, 1);
  if (GET_CODE (x3) == CONST_INT && XWINT (x3, 0) == 0 && 1)
    goto L1301;
  goto ret0;

  L1301:
  x2 = XEXP (x1, 1);
  switch (GET_CODE (x2))
    {
    case LABEL_REF:
      goto L1302;
    case PC:
      goto L1392;
    }
  goto ret0;

  L1302:
  x3 = XEXP (x2, 0);
  ro[0] = x3;
  goto L1303;

  L1303:
  x2 = XEXP (x1, 2);
  if (GET_CODE (x2) == PC && 1)
    return 257;
  goto ret0;

  L1392:
  x2 = XEXP (x1, 2);
  if (GET_CODE (x2) == LABEL_REF && 1)
    goto L1393;
  goto ret0;

  L1393:
  x3 = XEXP (x2, 0);
  ro[0] = x3;
  return 274;

  L1308:
  x3 = XEXP (x2, 0);
  if (GET_CODE (x3) == CC0 && 1)
    goto L1309;
  goto ret0;

  L1309:
  x3 = XEXP (x2, 1);
  if (GET_CODE (x3) == CONST_INT && XWINT (x3, 0) == 0 && 1)
    goto L1310;
  goto ret0;

  L1310:
  x2 = XEXP (x1, 1);
  switch (GET_CODE (x2))
    {
    case LABEL_REF:
      goto L1311;
    case PC:
      goto L1401;
    }
  goto ret0;

  L1311:
  x3 = XEXP (x2, 0);
  ro[0] = x3;
  goto L1312;

  L1312:
  x2 = XEXP (x1, 2);
  if (GET_CODE (x2) == PC && 1)
    return 259;
  goto ret0;

  L1401:
  x2 = XEXP (x1, 2);
  if (GET_CODE (x2) == LABEL_REF && 1)
    goto L1402;
  goto ret0;

  L1402:
  x3 = XEXP (x2, 0);
  ro[0] = x3;
  return 275;

  L1317:
  x3 = XEXP (x2, 0);
  if (GET_CODE (x3) == CC0 && 1)
    goto L1318;
  goto ret0;

  L1318:
  x3 = XEXP (x2, 1);
  if (GET_CODE (x3) == CONST_INT && XWINT (x3, 0) == 0 && 1)
    goto L1319;
  goto ret0;

  L1319:
  x2 = XEXP (x1, 1);
  switch (GET_CODE (x2))
    {
    case LABEL_REF:
      goto L1320;
    case PC:
      goto L1410;
    }
  goto ret0;

  L1320:
  x3 = XEXP (x2, 0);
  ro[0] = x3;
  goto L1321;

  L1321:
  x2 = XEXP (x1, 2);
  if (GET_CODE (x2) == PC && 1)
    return 261;
  goto ret0;

  L1410:
  x2 = XEXP (x1, 2);
  if (GET_CODE (x2) == LABEL_REF && 1)
    goto L1411;
  goto ret0;

  L1411:
  x3 = XEXP (x2, 0);
  ro[0] = x3;
  return 276;

  L1326:
  x3 = XEXP (x2, 0);
  if (GET_CODE (x3) == CC0 && 1)
    goto L1327;
  goto ret0;

  L1327:
  x3 = XEXP (x2, 1);
  if (GET_CODE (x3) == CONST_INT && XWINT (x3, 0) == 0 && 1)
    goto L1328;
  goto ret0;

  L1328:
  x2 = XEXP (x1, 1);
  switch (GET_CODE (x2))
    {
    case LABEL_REF:
      goto L1329;
    case PC:
      goto L1419;
    }
  goto ret0;

  L1329:
  x3 = XEXP (x2, 0);
  ro[0] = x3;
  goto L1330;

  L1330:
  x2 = XEXP (x1, 2);
  if (GET_CODE (x2) == PC && 1)
    return 263;
  goto ret0;

  L1419:
  x2 = XEXP (x1, 2);
  if (GET_CODE (x2) == LABEL_REF && 1)
    goto L1420;
  goto ret0;

  L1420:
  x3 = XEXP (x2, 0);
  ro[0] = x3;
  return 277;

  L1335:
  x3 = XEXP (x2, 0);
  if (GET_CODE (x3) == CC0 && 1)
    goto L1336;
  goto ret0;

  L1336:
  x3 = XEXP (x2, 1);
  if (GET_CODE (x3) == CONST_INT && XWINT (x3, 0) == 0 && 1)
    goto L1337;
  goto ret0;

  L1337:
  x2 = XEXP (x1, 1);
  switch (GET_CODE (x2))
    {
    case LABEL_REF:
      goto L1338;
    case PC:
      goto L1428;
    }
  goto ret0;

  L1338:
  x3 = XEXP (x2, 0);
  ro[0] = x3;
  goto L1339;

  L1339:
  x2 = XEXP (x1, 2);
  if (GET_CODE (x2) == PC && 1)
    return 265;
  goto ret0;

  L1428:
  x2 = XEXP (x1, 2);
  if (GET_CODE (x2) == LABEL_REF && 1)
    goto L1429;
  goto ret0;

  L1429:
  x3 = XEXP (x2, 0);
  ro[0] = x3;
  return 278;

  L1344:
  x3 = XEXP (x2, 0);
  if (GET_CODE (x3) == CC0 && 1)
    goto L1345;
  goto ret0;

  L1345:
  x3 = XEXP (x2, 1);
  if (GET_CODE (x3) == CONST_INT && XWINT (x3, 0) == 0 && 1)
    goto L1346;
  goto ret0;

  L1346:
  x2 = XEXP (x1, 1);
  switch (GET_CODE (x2))
    {
    case LABEL_REF:
      goto L1347;
    case PC:
      goto L1437;
    }
  goto ret0;

  L1347:
  x3 = XEXP (x2, 0);
  ro[0] = x3;
  goto L1348;

  L1348:
  x2 = XEXP (x1, 2);
  if (GET_CODE (x2) == PC && 1)
    return 267;
  goto ret0;

  L1437:
  x2 = XEXP (x1, 2);
  if (GET_CODE (x2) == LABEL_REF && 1)
    goto L1438;
  goto ret0;

  L1438:
  x3 = XEXP (x2, 0);
  ro[0] = x3;
  return 279;

  L1353:
  x3 = XEXP (x2, 0);
  if (GET_CODE (x3) == CC0 && 1)
    goto L1354;
  goto ret0;

  L1354:
  x3 = XEXP (x2, 1);
  if (GET_CODE (x3) == CONST_INT && XWINT (x3, 0) == 0 && 1)
    goto L1355;
  goto ret0;

  L1355:
  x2 = XEXP (x1, 1);
  switch (GET_CODE (x2))
    {
    case LABEL_REF:
      goto L1356;
    case PC:
      goto L1446;
    }
  goto ret0;

  L1356:
  x3 = XEXP (x2, 0);
  ro[0] = x3;
  goto L1357;

  L1357:
  x2 = XEXP (x1, 2);
  if (GET_CODE (x2) == PC && 1)
    return 269;
  goto ret0;

  L1446:
  x2 = XEXP (x1, 2);
  if (GET_CODE (x2) == LABEL_REF && 1)
    goto L1447;
  goto ret0;

  L1447:
  x3 = XEXP (x2, 0);
  ro[0] = x3;
  return 280;

  L1362:
  x3 = XEXP (x2, 0);
  if (GET_CODE (x3) == CC0 && 1)
    goto L1363;
  goto ret0;

  L1363:
  x3 = XEXP (x2, 1);
  if (GET_CODE (x3) == CONST_INT && XWINT (x3, 0) == 0 && 1)
    goto L1364;
  goto ret0;

  L1364:
  x2 = XEXP (x1, 1);
  switch (GET_CODE (x2))
    {
    case LABEL_REF:
      goto L1365;
    case PC:
      goto L1455;
    }
  goto ret0;

  L1365:
  x3 = XEXP (x2, 0);
  ro[0] = x3;
  goto L1366;

  L1366:
  x2 = XEXP (x1, 2);
  if (GET_CODE (x2) == PC && 1)
    return 271;
  goto ret0;

  L1455:
  x2 = XEXP (x1, 2);
  if (GET_CODE (x2) == LABEL_REF && 1)
    goto L1456;
  goto ret0;

  L1456:
  x3 = XEXP (x2, 0);
  ro[0] = x3;
  return 281;
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
	  goto L1191;
	}
    L304:
      if (general_operand (x1, SImode))
	{
	  ro[0] = x1;
	  goto L469;
	}
    L723:
      if (register_operand (x1, SImode))
	{
	  ro[0] = x1;
	  goto L820;
	}
    L739:
      if (general_operand (x1, SImode))
	{
	  ro[0] = x1;
	  goto L740;
	}
      break;
    case HImode:
      if (GET_CODE (x1) == MEM && push_operand (x1, HImode))
	{
	  ro[0] = x1;
	  goto L308;
	}
    L316:
      if (general_operand (x1, HImode))
	{
	  ro[0] = x1;
	  goto L473;
	}
      break;
    case QImode:
      if (GET_CODE (x1) == MEM && push_operand (x1, QImode))
	{
	  ro[0] = x1;
	  goto L327;
	}
    L332:
      if (general_operand (x1, QImode))
	{
	  ro[0] = x1;
	  goto L719;
	}
    L1228:
      if (register_operand (x1, QImode))
	{
	  ro[0] = x1;
	  goto L1229;
	}
      break;
    case SFmode:
      if (GET_CODE (x1) == MEM && push_operand (x1, SFmode))
	{
	  ro[0] = x1;
	  goto L340;
	}
    L359:
      if (memory_operand (x1, SFmode))
	{
	  ro[0] = x1;
	  goto L360;
	}
    L362:
      if (general_operand (x1, SFmode))
	{
	  ro[0] = x1;
	  goto L520;
	}
    L679:
      if (register_operand (x1, SFmode))
	{
	  ro[0] = x1;
	  goto L680;
	}
      break;
    case DFmode:
      if (GET_CODE (x1) == MEM && push_operand (x1, DFmode))
	{
	  ro[0] = x1;
	  goto L373;
	}
    L396:
      if (memory_operand (x1, DFmode))
	{
	  ro[0] = x1;
	  goto L397;
	}
    L399:
      if (general_operand (x1, DFmode))
	{
	  ro[0] = x1;
	  goto L501;
	}
    L675:
      if (register_operand (x1, DFmode))
	{
	  ro[0] = x1;
	  goto L676;
	}
      break;
    case XFmode:
      if (GET_CODE (x1) == MEM && push_operand (x1, XFmode))
	{
	  ro[0] = x1;
	  goto L410;
	}
    L433:
      if (memory_operand (x1, XFmode))
	{
	  ro[0] = x1;
	  goto L434;
	}
    L436:
      if (general_operand (x1, XFmode))
	{
	  ro[0] = x1;
	  goto L505;
	}
    L671:
      if (register_operand (x1, XFmode))
	{
	  ro[0] = x1;
	  goto L672;
	}
      break;
    case DImode:
      if (GET_CODE (x1) == MEM && push_operand (x1, DImode))
	{
	  ro[0] = x1;
	  goto L455;
	}
    L465:
      if (general_operand (x1, DImode))
	{
	  ro[0] = x1;
	  goto L704;
	}
    L480:
      if (register_operand (x1, DImode))
	{
	  ro[0] = x1;
	  goto L481;
	}
    }
  switch (GET_CODE (x1))
    {
    case CC0:
      goto L2;
    case STRICT_LOW_PART:
      goto L320;
    case PC:
      goto L1480;
    }
  L1546:
  ro[0] = x1;
  goto L1547;
  L1613:
  switch (GET_MODE (x1))
    {
    case SImode:
      if (general_operand (x1, SImode))
	{
	  ro[0] = x1;
	  goto L1614;
	}
      break;
    case HImode:
      if (general_operand (x1, HImode))
	{
	  ro[0] = x1;
	  goto L1620;
	}
      break;
    case DFmode:
      if (register_operand (x1, DFmode))
	{
	  ro[0] = x1;
	  goto L1626;
	}
      break;
    case XFmode:
      if (register_operand (x1, XFmode))
	{
	  ro[0] = x1;
	  goto L1637;
	}
      break;
    case SFmode:
      if (register_operand (x1, SFmode))
	{
	  ro[0] = x1;
	  goto L1684;
	}
    }
  goto ret0;

  L296:
  x1 = XEXP (x0, 1);
  if (general_operand (x1, SImode))
    {
      ro[1] = x1;
      if (TARGET_386)
	return 46;
      }
  L299:
  if (nonmemory_operand (x1, SImode))
    {
      ro[1] = x1;
      if (!TARGET_386 && TARGET_MOVE)
	return 47;
      }
  L302:
  if (general_operand (x1, SImode))
    {
      ro[1] = x1;
      if (!TARGET_386 && !TARGET_MOVE)
	return 48;
      }
  x1 = XEXP (x0, 0);
  goto L304;

  L1191:
  x2 = XEXP (x1, 0);
  if (GET_MODE (x2) == SImode && general_operand (x2, SImode))
    {
      ro[0] = x2;
      goto L1192;
    }
  goto L1546;

  L1192:
  x2 = XEXP (x1, 1);
  if (GET_CODE (x2) == CONST_INT && XWINT (x2, 0) == 1 && 1)
    goto L1193;
  goto L1546;

  L1193:
  x2 = XEXP (x1, 2);
  if (general_operand (x2, SImode))
    {
      ro[2] = x2;
      goto L1194;
    }
  goto L1546;

  L1194:
  x1 = XEXP (x0, 1);
  if (GET_CODE (x1) == CONST_INT && 1)
    {
      ro[3] = x1;
      if (TARGET_386 && GET_CODE (operands[2]) != CONST_INT)
	return 226;
      }
  x1 = XEXP (x0, 0);
  goto L1546;

  L469:
  x1 = XEXP (x0, 1);
  switch (GET_MODE (x1))
    {
    case SImode:
      switch (GET_CODE (x1))
	{
	case ZERO_EXTEND:
	  goto L470;
	case SIGN_EXTEND:
	  goto L490;
	case PLUS:
	  goto L710;
	}
    }
  if (general_operand (x1, SImode))
    {
      ro[1] = x1;
      if ((!TARGET_MOVE || GET_CODE (operands[0]) != MEM) || (GET_CODE (operands[1]) != MEM))
	return 50;
      }
  x1 = XEXP (x0, 0);
  goto L723;

  L470:
  x2 = XEXP (x1, 0);
  switch (GET_MODE (x2))
    {
    case HImode:
      if (nonimmediate_operand (x2, HImode))
	{
	  ro[1] = x2;
	  return 85;
	}
      break;
    case QImode:
      if (nonimmediate_operand (x2, QImode))
	{
	  ro[1] = x2;
	  return 87;
	}
    }
  x1 = XEXP (x0, 0);
  goto L723;

  L490:
  x2 = XEXP (x1, 0);
  switch (GET_MODE (x2))
    {
    case HImode:
      if (nonimmediate_operand (x2, HImode))
	{
	  ro[1] = x2;
	  return 90;
	}
      break;
    case QImode:
      if (nonimmediate_operand (x2, QImode))
	{
	  ro[1] = x2;
	  return 92;
	}
    }
  x1 = XEXP (x0, 0);
  goto L723;

  L710:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, SImode))
    {
      ro[1] = x2;
      goto L711;
    }
  x1 = XEXP (x0, 0);
  goto L723;

  L711:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, SImode))
    {
      ro[2] = x2;
      return 128;
    }
  x1 = XEXP (x0, 0);
  goto L723;

  L820:
  x1 = XEXP (x0, 1);
  if (GET_MODE (x1) == SImode && GET_CODE (x1) == TRUNCATE && 1)
    goto L821;
  if (address_operand (x1, QImode))
    {
      ro[1] = x1;
      return 131;
    }
  x1 = XEXP (x0, 0);
  goto L739;

  L821:
  x2 = XEXP (x1, 0);
  if (GET_MODE (x2) == DImode && GET_CODE (x2) == LSHIFTRT && 1)
    goto L822;
  x1 = XEXP (x0, 0);
  goto L739;

  L822:
  x3 = XEXP (x2, 0);
  if (GET_MODE (x3) == DImode && GET_CODE (x3) == MULT && 1)
    goto L823;
  x1 = XEXP (x0, 0);
  goto L739;

  L823:
  x4 = XEXP (x3, 0);
  if (GET_MODE (x4) != DImode)
    {
      x1 = XEXP (x0, 0);
      goto L739;
    }
  switch (GET_CODE (x4))
    {
    case ZERO_EXTEND:
      goto L824;
    case SIGN_EXTEND:
      goto L847;
    }
  x1 = XEXP (x0, 0);
  goto L739;

  L824:
  x5 = XEXP (x4, 0);
  if (register_operand (x5, SImode))
    {
      ro[1] = x5;
      goto L825;
    }
  x1 = XEXP (x0, 0);
  goto L739;

  L825:
  x4 = XEXP (x3, 1);
  if (GET_MODE (x4) == DImode && GET_CODE (x4) == ZERO_EXTEND && 1)
    goto L826;
  x1 = XEXP (x0, 0);
  goto L739;

  L826:
  x5 = XEXP (x4, 0);
  if (nonimmediate_operand (x5, SImode))
    {
      ro[2] = x5;
      goto L827;
    }
  x1 = XEXP (x0, 0);
  goto L739;

  L827:
  x3 = XEXP (x2, 1);
  if (GET_CODE (x3) == CONST_INT && XWINT (x3, 0) == 32 && pnum_clobbers != 0 && 1)
    if (TARGET_WIDE_MULTIPLY)
      {
	*pnum_clobbers = 1;
	return 150;
      }
  x1 = XEXP (x0, 0);
  goto L739;

  L847:
  x5 = XEXP (x4, 0);
  if (register_operand (x5, SImode))
    {
      ro[1] = x5;
      goto L848;
    }
  x1 = XEXP (x0, 0);
  goto L739;

  L848:
  x4 = XEXP (x3, 1);
  if (GET_MODE (x4) == DImode && GET_CODE (x4) == SIGN_EXTEND && 1)
    goto L849;
  x1 = XEXP (x0, 0);
  goto L739;

  L849:
  x5 = XEXP (x4, 0);
  if (nonimmediate_operand (x5, SImode))
    {
      ro[2] = x5;
      goto L850;
    }
  x1 = XEXP (x0, 0);
  goto L739;

  L850:
  x3 = XEXP (x2, 1);
  if (GET_CODE (x3) == CONST_INT && XWINT (x3, 0) == 32 && pnum_clobbers != 0 && 1)
    if (TARGET_WIDE_MULTIPLY)
      {
	*pnum_clobbers = 1;
	return 151;
      }
  x1 = XEXP (x0, 0);
  goto L739;

  L740:
  x1 = XEXP (x0, 1);
  if (GET_MODE (x1) != SImode)
    {
      x1 = XEXP (x0, 0);
      goto L1546;
    }
  switch (GET_CODE (x1))
    {
    case MINUS:
      goto L741;
    case MULT:
      goto L768;
    case AND:
      goto L908;
    case IOR:
      goto L923;
    case XOR:
      goto L1198;
    case NEG:
      goto L957;
    case NOT:
      goto L1066;
    case ASHIFT:
      goto L1091;
    case ASHIFTRT:
      goto L1119;
    case LSHIFTRT:
      goto L1147;
    case ROTATE:
      goto L1162;
    case ROTATERT:
      goto L1177;
    }
  x1 = XEXP (x0, 0);
  goto L1546;

  L741:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, SImode))
    {
      ro[1] = x2;
      goto L742;
    }
  x1 = XEXP (x0, 0);
  goto L1546;

  L742:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, SImode))
    {
      ro[2] = x2;
      return 136;
    }
  x1 = XEXP (x0, 0);
  goto L1546;

  L768:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, SImode))
    {
      ro[1] = x2;
      goto L769;
    }
  x1 = XEXP (x0, 0);
  goto L1546;

  L769:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, SImode))
    goto L775;
  x1 = XEXP (x0, 0);
  goto L1546;

  L775:
  ro[2] = x2;
  if (GET_CODE (operands[2]) == CONST_INT && INTVAL (operands[2]) == 0x80)
    return 144;
  L776:
  ro[2] = x2;
  return 145;

  L908:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, SImode))
    {
      ro[1] = x2;
      goto L909;
    }
  x1 = XEXP (x0, 0);
  goto L1546;

  L909:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, SImode))
    {
      ro[2] = x2;
      return 164;
    }
  x1 = XEXP (x0, 0);
  goto L1546;

  L923:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, SImode))
    {
      ro[1] = x2;
      goto L924;
    }
  x1 = XEXP (x0, 0);
  goto L1546;

  L924:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, SImode))
    {
      ro[2] = x2;
      return 167;
    }
  x1 = XEXP (x0, 0);
  goto L1546;

  L1198:
  x2 = XEXP (x1, 0);
  if (GET_MODE (x2) == SImode && GET_CODE (x2) == ASHIFT && 1)
    goto L1199;
  if (general_operand (x2, SImode))
    {
      ro[1] = x2;
      goto L1206;
    }
  x1 = XEXP (x0, 0);
  goto L1546;

  L1199:
  x3 = XEXP (x2, 0);
  if (GET_CODE (x3) == CONST_INT && XWINT (x3, 0) == 1 && 1)
    goto L1200;
  x1 = XEXP (x0, 0);
  goto L1546;

  L1200:
  x3 = XEXP (x2, 1);
  if (general_operand (x3, SImode))
    {
      ro[1] = x3;
      goto L1201;
    }
  x1 = XEXP (x0, 0);
  goto L1546;

  L1201:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, SImode))
    {
      ro[2] = x2;
      if (TARGET_386 && GET_CODE (operands[1]) != CONST_INT)
	return 227;
      }
  x1 = XEXP (x0, 0);
  goto L1546;

  L1206:
  x2 = XEXP (x1, 1);
  if (GET_MODE (x2) == SImode && GET_CODE (x2) == ASHIFT && 1)
    goto L1207;
  if (general_operand (x2, SImode))
    {
      ro[2] = x2;
      return 170;
    }
  x1 = XEXP (x0, 0);
  goto L1546;

  L1207:
  x3 = XEXP (x2, 0);
  if (GET_CODE (x3) == CONST_INT && XWINT (x3, 0) == 1 && 1)
    goto L1208;
  x1 = XEXP (x0, 0);
  goto L1546;

  L1208:
  x3 = XEXP (x2, 1);
  if (general_operand (x3, SImode))
    {
      ro[2] = x3;
      if (TARGET_386 && GET_CODE (operands[2]) != CONST_INT)
	return 228;
      }
  x1 = XEXP (x0, 0);
  goto L1546;

  L957:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, SImode))
    {
      ro[1] = x2;
      return 174;
    }
  x1 = XEXP (x0, 0);
  goto L1546;

  L1066:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, SImode))
    {
      ro[1] = x2;
      return 199;
    }
  x1 = XEXP (x0, 0);
  goto L1546;

  L1091:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, SImode))
    {
      ro[1] = x2;
      goto L1092;
    }
  x1 = XEXP (x0, 0);
  goto L1546;

  L1092:
  x2 = XEXP (x1, 1);
  if (nonmemory_operand (x2, SImode))
    {
      ro[2] = x2;
      return 205;
    }
  x1 = XEXP (x0, 0);
  goto L1546;

  L1119:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, SImode))
    {
      ro[1] = x2;
      goto L1120;
    }
  x1 = XEXP (x0, 0);
  goto L1546;

  L1120:
  x2 = XEXP (x1, 1);
  if (nonmemory_operand (x2, SImode))
    {
      ro[2] = x2;
      return 211;
    }
  x1 = XEXP (x0, 0);
  goto L1546;

  L1147:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, SImode))
    {
      ro[1] = x2;
      goto L1148;
    }
  x1 = XEXP (x0, 0);
  goto L1546;

  L1148:
  x2 = XEXP (x1, 1);
  if (nonmemory_operand (x2, SImode))
    {
      ro[2] = x2;
      return 217;
    }
  x1 = XEXP (x0, 0);
  goto L1546;

  L1162:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, SImode))
    {
      ro[1] = x2;
      goto L1163;
    }
  x1 = XEXP (x0, 0);
  goto L1546;

  L1163:
  x2 = XEXP (x1, 1);
  if (nonmemory_operand (x2, SImode))
    {
      ro[2] = x2;
      return 220;
    }
  x1 = XEXP (x0, 0);
  goto L1546;

  L1177:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, SImode))
    {
      ro[1] = x2;
      goto L1178;
    }
  x1 = XEXP (x0, 0);
  goto L1546;

  L1178:
  x2 = XEXP (x1, 1);
  if (nonmemory_operand (x2, SImode))
    {
      ro[2] = x2;
      return 223;
    }
  x1 = XEXP (x0, 0);
  goto L1546;

  L308:
  x1 = XEXP (x0, 1);
  if (general_operand (x1, HImode))
    {
      ro[1] = x1;
      if (TARGET_386)
	return 51;
      }
  L311:
  if (nonmemory_operand (x1, HImode))
    {
      ro[1] = x1;
      if (!TARGET_386 && TARGET_MOVE)
	return 52;
      }
  L314:
  if (general_operand (x1, HImode))
    {
      ro[1] = x1;
      if (!TARGET_386 && !TARGET_MOVE)
	return 53;
      }
  x1 = XEXP (x0, 0);
  goto L316;
 L473:
  tem = recog_1 (x0, insn, pnum_clobbers);
  if (tem >= 0) return tem;
  x1 = XEXP (x0, 0);
  goto L1546;

  L327:
  x1 = XEXP (x0, 1);
  switch (GET_MODE (x1))
    {
    case QImode:
      if (nonimmediate_operand (x1, QImode))
	{
	  ro[1] = x1;
	  if (!TARGET_MOVE)
	    return 59;
	  }
    L330:
      if (register_operand (x1, QImode))
	{
	  ro[1] = x1;
	  if (TARGET_MOVE)
	    return 60;
	  }
    }
  if (immediate_operand (x1, QImode))
    {
      ro[1] = x1;
      return 58;
    }
  x1 = XEXP (x0, 0);
  goto L332;

  L719:
  x1 = XEXP (x0, 1);
  switch (GET_MODE (x1))
    {
    case QImode:
      switch (GET_CODE (x1))
	{
	case PLUS:
	  goto L720;
	case MINUS:
	  goto L751;
	case DIV:
	  goto L854;
	case UDIV:
	  goto L859;
	case AND:
	  goto L918;
	case IOR:
	  goto L933;
	case XOR:
	  goto L948;
	case NEG:
	  goto L965;
	case NOT:
	  goto L1074;
	case ASHIFT:
	  goto L1101;
	case ASHIFTRT:
	  goto L1129;
	case LSHIFTRT:
	  goto L1157;
	case ROTATE:
	  goto L1172;
	case ROTATERT:
	  goto L1187;
	}
    }
  if (general_operand (x1, QImode))
    {
      ro[1] = x1;
      if ((!TARGET_MOVE || GET_CODE (operands[0]) != MEM) || (GET_CODE (operands[1]) != MEM))
	return 62;
      }
  x1 = XEXP (x0, 0);
  goto L1228;

  L720:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, QImode))
    {
      ro[1] = x2;
      goto L721;
    }
  x1 = XEXP (x0, 0);
  goto L1228;

  L721:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, QImode))
    {
      ro[2] = x2;
      return 130;
    }
  x1 = XEXP (x0, 0);
  goto L1228;

  L751:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, QImode))
    {
      ro[1] = x2;
      goto L752;
    }
  x1 = XEXP (x0, 0);
  goto L1228;

  L752:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, QImode))
    {
      ro[2] = x2;
      return 138;
    }
  x1 = XEXP (x0, 0);
  goto L1228;

  L854:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, HImode))
    {
      ro[1] = x2;
      goto L855;
    }
  x1 = XEXP (x0, 0);
  goto L1228;

  L855:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, QImode))
    {
      ro[2] = x2;
      return 155;
    }
  x1 = XEXP (x0, 0);
  goto L1228;

  L859:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, HImode))
    {
      ro[1] = x2;
      goto L860;
    }
  x1 = XEXP (x0, 0);
  goto L1228;

  L860:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, QImode))
    {
      ro[2] = x2;
      return 156;
    }
  x1 = XEXP (x0, 0);
  goto L1228;

  L918:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, QImode))
    {
      ro[1] = x2;
      goto L919;
    }
  x1 = XEXP (x0, 0);
  goto L1228;

  L919:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, QImode))
    {
      ro[2] = x2;
      return 166;
    }
  x1 = XEXP (x0, 0);
  goto L1228;

  L933:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, QImode))
    {
      ro[1] = x2;
      goto L934;
    }
  x1 = XEXP (x0, 0);
  goto L1228;

  L934:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, QImode))
    {
      ro[2] = x2;
      return 169;
    }
  x1 = XEXP (x0, 0);
  goto L1228;

  L948:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, QImode))
    {
      ro[1] = x2;
      goto L949;
    }
  x1 = XEXP (x0, 0);
  goto L1228;

  L949:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, QImode))
    {
      ro[2] = x2;
      return 172;
    }
  x1 = XEXP (x0, 0);
  goto L1228;

  L965:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, QImode))
    {
      ro[1] = x2;
      return 176;
    }
  x1 = XEXP (x0, 0);
  goto L1228;

  L1074:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, QImode))
    {
      ro[1] = x2;
      return 201;
    }
  x1 = XEXP (x0, 0);
  goto L1228;

  L1101:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, QImode))
    {
      ro[1] = x2;
      goto L1102;
    }
  x1 = XEXP (x0, 0);
  goto L1228;

  L1102:
  x2 = XEXP (x1, 1);
  if (nonmemory_operand (x2, QImode))
    {
      ro[2] = x2;
      return 207;
    }
  x1 = XEXP (x0, 0);
  goto L1228;

  L1129:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, QImode))
    {
      ro[1] = x2;
      goto L1130;
    }
  x1 = XEXP (x0, 0);
  goto L1228;

  L1130:
  x2 = XEXP (x1, 1);
  if (nonmemory_operand (x2, QImode))
    {
      ro[2] = x2;
      return 213;
    }
  x1 = XEXP (x0, 0);
  goto L1228;

  L1157:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, QImode))
    {
      ro[1] = x2;
      goto L1158;
    }
  x1 = XEXP (x0, 0);
  goto L1228;

  L1158:
  x2 = XEXP (x1, 1);
  if (nonmemory_operand (x2, QImode))
    {
      ro[2] = x2;
      return 219;
    }
  x1 = XEXP (x0, 0);
  goto L1228;

  L1172:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, QImode))
    {
      ro[1] = x2;
      goto L1173;
    }
  x1 = XEXP (x0, 0);
  goto L1228;

  L1173:
  x2 = XEXP (x1, 1);
  if (nonmemory_operand (x2, QImode))
    {
      ro[2] = x2;
      return 222;
    }
  x1 = XEXP (x0, 0);
  goto L1228;

  L1187:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, QImode))
    {
      ro[1] = x2;
      goto L1188;
    }
  x1 = XEXP (x0, 0);
  goto L1228;

  L1188:
  x2 = XEXP (x1, 1);
  if (nonmemory_operand (x2, QImode))
    {
      ro[2] = x2;
      return 225;
    }
  x1 = XEXP (x0, 0);
  goto L1228;

  L1229:
  x1 = XEXP (x0, 1);
  if (GET_MODE (x1) != QImode)
    {
      x1 = XEXP (x0, 0);
      goto L1546;
    }
  switch (GET_CODE (x1))
    {
    case EQ:
      goto L1230;
    case NE:
      goto L1235;
    case GT:
      goto L1240;
    case GTU:
      goto L1245;
    case LT:
      goto L1250;
    case LTU:
      goto L1255;
    case GE:
      goto L1260;
    case GEU:
      goto L1265;
    case LE:
      goto L1270;
    case LEU:
      goto L1275;
    }
  x1 = XEXP (x0, 0);
  goto L1546;

  L1230:
  x2 = XEXP (x1, 0);
  if (GET_CODE (x2) == CC0 && 1)
    goto L1231;
  x1 = XEXP (x0, 0);
  goto L1546;

  L1231:
  x2 = XEXP (x1, 1);
  if (GET_CODE (x2) == CONST_INT && XWINT (x2, 0) == 0 && 1)
    return 233;
  x1 = XEXP (x0, 0);
  goto L1546;

  L1235:
  x2 = XEXP (x1, 0);
  if (GET_CODE (x2) == CC0 && 1)
    goto L1236;
  x1 = XEXP (x0, 0);
  goto L1546;

  L1236:
  x2 = XEXP (x1, 1);
  if (GET_CODE (x2) == CONST_INT && XWINT (x2, 0) == 0 && 1)
    return 235;
  x1 = XEXP (x0, 0);
  goto L1546;

  L1240:
  x2 = XEXP (x1, 0);
  if (GET_CODE (x2) == CC0 && 1)
    goto L1241;
  x1 = XEXP (x0, 0);
  goto L1546;

  L1241:
  x2 = XEXP (x1, 1);
  if (GET_CODE (x2) == CONST_INT && XWINT (x2, 0) == 0 && 1)
    return 237;
  x1 = XEXP (x0, 0);
  goto L1546;

  L1245:
  x2 = XEXP (x1, 0);
  if (GET_CODE (x2) == CC0 && 1)
    goto L1246;
  x1 = XEXP (x0, 0);
  goto L1546;

  L1246:
  x2 = XEXP (x1, 1);
  if (GET_CODE (x2) == CONST_INT && XWINT (x2, 0) == 0 && 1)
    return 239;
  x1 = XEXP (x0, 0);
  goto L1546;

  L1250:
  x2 = XEXP (x1, 0);
  if (GET_CODE (x2) == CC0 && 1)
    goto L1251;
  x1 = XEXP (x0, 0);
  goto L1546;

  L1251:
  x2 = XEXP (x1, 1);
  if (GET_CODE (x2) == CONST_INT && XWINT (x2, 0) == 0 && 1)
    return 241;
  x1 = XEXP (x0, 0);
  goto L1546;

  L1255:
  x2 = XEXP (x1, 0);
  if (GET_CODE (x2) == CC0 && 1)
    goto L1256;
  x1 = XEXP (x0, 0);
  goto L1546;

  L1256:
  x2 = XEXP (x1, 1);
  if (GET_CODE (x2) == CONST_INT && XWINT (x2, 0) == 0 && 1)
    return 243;
  x1 = XEXP (x0, 0);
  goto L1546;

  L1260:
  x2 = XEXP (x1, 0);
  if (GET_CODE (x2) == CC0 && 1)
    goto L1261;
  x1 = XEXP (x0, 0);
  goto L1546;

  L1261:
  x2 = XEXP (x1, 1);
  if (GET_CODE (x2) == CONST_INT && XWINT (x2, 0) == 0 && 1)
    return 245;
  x1 = XEXP (x0, 0);
  goto L1546;

  L1265:
  x2 = XEXP (x1, 0);
  if (GET_CODE (x2) == CC0 && 1)
    goto L1266;
  x1 = XEXP (x0, 0);
  goto L1546;

  L1266:
  x2 = XEXP (x1, 1);
  if (GET_CODE (x2) == CONST_INT && XWINT (x2, 0) == 0 && 1)
    return 247;
  x1 = XEXP (x0, 0);
  goto L1546;

  L1270:
  x2 = XEXP (x1, 0);
  if (GET_CODE (x2) == CC0 && 1)
    goto L1271;
  x1 = XEXP (x0, 0);
  goto L1546;

  L1271:
  x2 = XEXP (x1, 1);
  if (GET_CODE (x2) == CONST_INT && XWINT (x2, 0) == 0 && 1)
    return 249;
  x1 = XEXP (x0, 0);
  goto L1546;

  L1275:
  x2 = XEXP (x1, 0);
  if (GET_CODE (x2) == CC0 && 1)
    goto L1276;
  x1 = XEXP (x0, 0);
  goto L1546;

  L1276:
  x2 = XEXP (x1, 1);
  if (GET_CODE (x2) == CONST_INT && XWINT (x2, 0) == 0 && 1)
    return 251;
  x1 = XEXP (x0, 0);
  goto L1546;

  L340:
  x1 = XEXP (x0, 1);
  if (general_operand (x1, SFmode))
    goto L350;
  x1 = XEXP (x0, 0);
  goto L359;

  L350:
  ro[1] = x1;
  if (!TARGET_MOVE)
    return 66;
  L351:
  if (pnum_clobbers != 0 && 1)
    {
      ro[1] = x1;
      *pnum_clobbers = 1;
      return 67;
    }
  x1 = XEXP (x0, 0);
  goto L359;

  L360:
  x1 = XEXP (x0, 1);
  if (pnum_clobbers != 0 && memory_operand (x1, SFmode))
    {
      ro[1] = x1;
      *pnum_clobbers = 1;
      return 68;
    }
  x1 = XEXP (x0, 0);
  goto L362;

  L520:
  x1 = XEXP (x0, 1);
  if (GET_MODE (x1) == SFmode && GET_CODE (x1) == FLOAT_TRUNCATE && 1)
    goto L521;
  if (general_operand (x1, SFmode))
    {
      ro[1] = x1;
      if ((!TARGET_MOVE || GET_CODE (operands[0]) != MEM) || (GET_CODE (operands[1]) != MEM))
	return 69;
      }
  x1 = XEXP (x0, 0);
  goto L679;

  L521:
  x2 = XEXP (x1, 0);
  if (register_operand (x2, XFmode))
    {
      ro[1] = x2;
      if (TARGET_80387)
	return 98;
      }
  x1 = XEXP (x0, 0);
  goto L679;

  L680:
  x1 = XEXP (x0, 1);
  if (GET_MODE (x1) != SFmode)
    {
      x1 = XEXP (x0, 0);
      goto L1546;
    }
  switch (GET_CODE (x1))
    {
    case FLOAT:
      goto L681;
    case NEG:
      goto L969;
    case ABS:
      goto L991;
    case SQRT:
      goto L1013;
    case UNSPEC:
      if (XINT (x1, 1) == 1 && XVECLEN (x1, 0) == 1 && 1)
	goto L1044;
      if (XINT (x1, 1) == 2 && XVECLEN (x1, 0) == 1 && 1)
	goto L1057;
    }
  x1 = XEXP (x0, 0);
  goto L1546;

  L681:
  x2 = XEXP (x1, 0);
  switch (GET_MODE (x2))
    {
    case DImode:
      if (nonimmediate_operand (x2, DImode))
	{
	  ro[1] = x2;
	  if (TARGET_80387)
	    return 123;
	  }
      break;
    case SImode:
      if (nonimmediate_operand (x2, SImode))
	{
	  ro[1] = x2;
	  if (TARGET_80387)
	    return 126;
	  }
    }
  x1 = XEXP (x0, 0);
  goto L1546;

  L969:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, SFmode))
    {
      ro[1] = x2;
      if (TARGET_80387)
	return 177;
      }
  x1 = XEXP (x0, 0);
  goto L1546;

  L991:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, SFmode))
    {
      ro[1] = x2;
      if (TARGET_80387)
	return 182;
      }
  x1 = XEXP (x0, 0);
  goto L1546;

  L1013:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, SFmode))
    {
      ro[1] = x2;
      if (! TARGET_NO_FANCY_MATH_387 && TARGET_80387
   && (TARGET_IEEE_FP || flag_fast_math) )
	return 187;
      }
  x1 = XEXP (x0, 0);
  goto L1546;

  L1044:
  x2 = XVECEXP (x1, 0, 0);
  if (register_operand (x2, SFmode))
    {
      ro[1] = x2;
      if (! TARGET_NO_FANCY_MATH_387 && TARGET_80387
   && (TARGET_IEEE_FP || flag_fast_math) )
	return 194;
      }
  x1 = XEXP (x0, 0);
  goto L1546;

  L1057:
  x2 = XVECEXP (x1, 0, 0);
  if (register_operand (x2, SFmode))
    {
      ro[1] = x2;
      if (! TARGET_NO_FANCY_MATH_387 && TARGET_80387
   && (TARGET_IEEE_FP || flag_fast_math) )
	return 197;
      }
  x1 = XEXP (x0, 0);
  goto L1546;

  L373:
  x1 = XEXP (x0, 1);
  if (general_operand (x1, DFmode))
    goto L385;
  x1 = XEXP (x0, 0);
  goto L396;

  L385:
  ro[1] = x1;
  if (!TARGET_MOVE)
    return 72;
  L386:
  if (pnum_clobbers != 0 && 1)
    {
      ro[1] = x1;
      *pnum_clobbers = 2;
      return 73;
    }
  x1 = XEXP (x0, 0);
  goto L396;

  L397:
  x1 = XEXP (x0, 1);
  if (pnum_clobbers != 0 && memory_operand (x1, DFmode))
    {
      ro[1] = x1;
      *pnum_clobbers = 2;
      return 74;
    }
  x1 = XEXP (x0, 0);
  goto L399;

  L501:
  x1 = XEXP (x0, 1);
  switch (GET_MODE (x1))
    {
    case DFmode:
      switch (GET_CODE (x1))
	{
	case FLOAT_EXTEND:
	  goto L502;
	case FLOAT_TRUNCATE:
	  goto L525;
	}
    }
  if (general_operand (x1, DFmode))
    {
      ro[1] = x1;
      if ((!TARGET_MOVE || GET_CODE (operands[0]) != MEM) || (GET_CODE (operands[1]) != MEM))
	return 75;
      }
  x1 = XEXP (x0, 0);
  goto L675;

  L502:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, SFmode))
    {
      ro[1] = x2;
      if (TARGET_80387)
	return 93;
      }
  x1 = XEXP (x0, 0);
  goto L675;

  L525:
  x2 = XEXP (x1, 0);
  if (register_operand (x2, XFmode))
    {
      ro[1] = x2;
      if (TARGET_80387)
	return 99;
      }
  x1 = XEXP (x0, 0);
  goto L675;

  L676:
  x1 = XEXP (x0, 1);
  if (GET_MODE (x1) != DFmode)
    {
      x1 = XEXP (x0, 0);
      goto L1546;
    }
  switch (GET_CODE (x1))
    {
    case FLOAT:
      goto L677;
    case NEG:
      goto L977;
    case ABS:
      goto L999;
    case SQRT:
      goto L1021;
    case UNSPEC:
      if (XINT (x1, 1) == 1 && XVECLEN (x1, 0) == 1 && 1)
	goto L1048;
      if (XINT (x1, 1) == 2 && XVECLEN (x1, 0) == 1 && 1)
	goto L1061;
    }
  x1 = XEXP (x0, 0);
  goto L1546;

  L677:
  x2 = XEXP (x1, 0);
  switch (GET_MODE (x2))
    {
    case DImode:
      if (nonimmediate_operand (x2, DImode))
	{
	  ro[1] = x2;
	  if (TARGET_80387)
	    return 122;
	  }
      break;
    case SImode:
      if (nonimmediate_operand (x2, SImode))
	{
	  ro[1] = x2;
	  if (TARGET_80387)
	    return 124;
	  }
    }
  x1 = XEXP (x0, 0);
  goto L1546;

  L977:
  x2 = XEXP (x1, 0);
  if (GET_MODE (x2) == DFmode && GET_CODE (x2) == FLOAT_EXTEND && 1)
    goto L978;
  if (general_operand (x2, DFmode))
    {
      ro[1] = x2;
      if (TARGET_80387)
	return 178;
      }
  x1 = XEXP (x0, 0);
  goto L1546;

  L978:
  x3 = XEXP (x2, 0);
  if (general_operand (x3, SFmode))
    {
      ro[1] = x3;
      if (TARGET_80387)
	return 179;
      }
  x1 = XEXP (x0, 0);
  goto L1546;

  L999:
  x2 = XEXP (x1, 0);
  if (GET_MODE (x2) == DFmode && GET_CODE (x2) == FLOAT_EXTEND && 1)
    goto L1000;
  if (general_operand (x2, DFmode))
    {
      ro[1] = x2;
      if (TARGET_80387)
	return 183;
      }
  x1 = XEXP (x0, 0);
  goto L1546;

  L1000:
  x3 = XEXP (x2, 0);
  if (general_operand (x3, SFmode))
    {
      ro[1] = x3;
      if (TARGET_80387)
	return 184;
      }
  x1 = XEXP (x0, 0);
  goto L1546;

  L1021:
  x2 = XEXP (x1, 0);
  if (GET_MODE (x2) == DFmode && GET_CODE (x2) == FLOAT_EXTEND && 1)
    goto L1022;
  if (general_operand (x2, DFmode))
    {
      ro[1] = x2;
      if (! TARGET_NO_FANCY_MATH_387 && TARGET_80387
   && (TARGET_IEEE_FP || flag_fast_math) )
	return 188;
      }
  x1 = XEXP (x0, 0);
  goto L1546;

  L1022:
  x3 = XEXP (x2, 0);
  if (general_operand (x3, SFmode))
    {
      ro[1] = x3;
      if (! TARGET_NO_FANCY_MATH_387 && TARGET_80387
   && (TARGET_IEEE_FP || flag_fast_math) )
	return 189;
      }
  x1 = XEXP (x0, 0);
  goto L1546;

  L1048:
  x2 = XVECEXP (x1, 0, 0);
  if (GET_MODE (x2) != DFmode)
    {
      x1 = XEXP (x0, 0);
      goto L1546;
    }
  if (GET_CODE (x2) == FLOAT_EXTEND && 1)
    goto L1049;
  if (register_operand (x2, DFmode))
    {
      ro[1] = x2;
      if (! TARGET_NO_FANCY_MATH_387 && TARGET_80387
   && (TARGET_IEEE_FP || flag_fast_math) )
	return 193;
      }
  x1 = XEXP (x0, 0);
  goto L1546;

  L1049:
  x3 = XEXP (x2, 0);
  if (register_operand (x3, SFmode))
    {
      ro[1] = x3;
      if (! TARGET_NO_FANCY_MATH_387 && TARGET_80387
   && (TARGET_IEEE_FP || flag_fast_math) )
	return 195;
      }
  x1 = XEXP (x0, 0);
  goto L1546;

  L1061:
  x2 = XVECEXP (x1, 0, 0);
  if (GET_MODE (x2) != DFmode)
    {
      x1 = XEXP (x0, 0);
      goto L1546;
    }
  if (GET_CODE (x2) == FLOAT_EXTEND && 1)
    goto L1062;
  if (register_operand (x2, DFmode))
    {
      ro[1] = x2;
      if (! TARGET_NO_FANCY_MATH_387 && TARGET_80387
   && (TARGET_IEEE_FP || flag_fast_math) )
	return 196;
      }
  x1 = XEXP (x0, 0);
  goto L1546;

  L1062:
  x3 = XEXP (x2, 0);
  if (register_operand (x3, SFmode))
    {
      ro[1] = x3;
      if (! TARGET_NO_FANCY_MATH_387 && TARGET_80387
   && (TARGET_IEEE_FP || flag_fast_math) )
	return 198;
      }
  x1 = XEXP (x0, 0);
  goto L1546;

  L410:
  x1 = XEXP (x0, 1);
  if (general_operand (x1, XFmode))
    goto L422;
  x1 = XEXP (x0, 0);
  goto L433;

  L422:
  ro[1] = x1;
  if (!TARGET_MOVE)
    return 78;
  L423:
  if (pnum_clobbers != 0 && 1)
    {
      ro[1] = x1;
      *pnum_clobbers = 2;
      return 79;
    }
  x1 = XEXP (x0, 0);
  goto L433;

  L434:
  x1 = XEXP (x0, 1);
  if (pnum_clobbers != 0 && memory_operand (x1, XFmode))
    {
      ro[1] = x1;
      *pnum_clobbers = 2;
      return 80;
    }
  x1 = XEXP (x0, 0);
  goto L436;

  L505:
  x1 = XEXP (x0, 1);
  if (GET_MODE (x1) == XFmode && GET_CODE (x1) == FLOAT_EXTEND && 1)
    goto L506;
  if (general_operand (x1, XFmode))
    {
      ro[1] = x1;
      if ((!TARGET_MOVE || GET_CODE (operands[0]) != MEM) || (GET_CODE (operands[1]) != MEM))
	return 81;
      }
  x1 = XEXP (x0, 0);
  goto L671;

  L506:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, DFmode))
    {
      ro[1] = x2;
      if (TARGET_80387)
	return 94;
      }
  L510:
  if (general_operand (x2, SFmode))
    {
      ro[1] = x2;
      if (TARGET_80387)
	return 95;
      }
  x1 = XEXP (x0, 0);
  goto L671;

  L672:
  x1 = XEXP (x0, 1);
  if (GET_MODE (x1) != XFmode)
    {
      x1 = XEXP (x0, 0);
      goto L1546;
    }
  switch (GET_CODE (x1))
    {
    case FLOAT:
      goto L673;
    case NEG:
      goto L986;
    case ABS:
      goto L1008;
    case SQRT:
      goto L1030;
    }
  x1 = XEXP (x0, 0);
  goto L1546;

  L673:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, DImode))
    {
      ro[1] = x2;
      if (TARGET_80387)
	return 121;
      }
  L689:
  if (general_operand (x2, SImode))
    {
      ro[1] = x2;
      if (TARGET_80387)
	return 125;
      }
  x1 = XEXP (x0, 0);
  goto L1546;

  L986:
  x2 = XEXP (x1, 0);
  if (GET_MODE (x2) == XFmode && GET_CODE (x2) == FLOAT_EXTEND && 1)
    goto L987;
  if (general_operand (x2, XFmode))
    {
      ro[1] = x2;
      if (TARGET_80387)
	return 180;
      }
  x1 = XEXP (x0, 0);
  goto L1546;

  L987:
  x3 = XEXP (x2, 0);
  if (general_operand (x3, DFmode))
    {
      ro[1] = x3;
      if (TARGET_80387)
	return 181;
      }
  x1 = XEXP (x0, 0);
  goto L1546;

  L1008:
  x2 = XEXP (x1, 0);
  if (GET_MODE (x2) == XFmode && GET_CODE (x2) == FLOAT_EXTEND && 1)
    goto L1009;
  if (general_operand (x2, XFmode))
    {
      ro[1] = x2;
      if (TARGET_80387)
	return 185;
      }
  x1 = XEXP (x0, 0);
  goto L1546;

  L1009:
  x3 = XEXP (x2, 0);
  if (general_operand (x3, DFmode))
    {
      ro[1] = x3;
      if (TARGET_80387)
	return 186;
      }
  x1 = XEXP (x0, 0);
  goto L1546;

  L1030:
  x2 = XEXP (x1, 0);
  if (GET_MODE (x2) == XFmode && GET_CODE (x2) == FLOAT_EXTEND && 1)
    goto L1031;
  if (general_operand (x2, XFmode))
    {
      ro[1] = x2;
      if (! TARGET_NO_FANCY_MATH_387 && TARGET_80387
   && (TARGET_IEEE_FP || flag_fast_math) )
	return 190;
      }
  x1 = XEXP (x0, 0);
  goto L1546;

  L1031:
  x3 = XEXP (x2, 0);
  if (general_operand (x3, DFmode))
    {
      ro[1] = x3;
      if (! TARGET_NO_FANCY_MATH_387 && TARGET_80387
   && (TARGET_IEEE_FP || flag_fast_math) )
	return 191;
      }
  L1036:
  if (general_operand (x3, SFmode))
    {
      ro[1] = x3;
      if (! TARGET_NO_FANCY_MATH_387 && TARGET_80387
   && (TARGET_IEEE_FP || flag_fast_math) )
	return 192;
      }
  x1 = XEXP (x0, 0);
  goto L1546;

  L455:
  x1 = XEXP (x0, 1);
  if (pnum_clobbers != 0 && general_operand (x1, DImode))
    {
      ro[1] = x1;
      *pnum_clobbers = 2;
      return 83;
    }
  x1 = XEXP (x0, 0);
  goto L465;

  L704:
  x1 = XEXP (x0, 1);
  switch (GET_MODE (x1))
    {
    case DImode:
      switch (GET_CODE (x1))
	{
	case PLUS:
	  goto L705;
	case MINUS:
	  goto L736;
	case NEG:
	  goto L953;
	}
    }
  if (pnum_clobbers != 0 && general_operand (x1, DImode))
    {
      ro[1] = x1;
      *pnum_clobbers = 2;
      return 84;
    }
  x1 = XEXP (x0, 0);
  goto L480;

  L705:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, DImode))
    {
      ro[1] = x2;
      goto L706;
    }
  x1 = XEXP (x0, 0);
  goto L480;

  L706:
  x2 = XEXP (x1, 1);
  if (pnum_clobbers != 0 && general_operand (x2, DImode))
    {
      ro[2] = x2;
      *pnum_clobbers = 1;
      return 127;
    }
  x1 = XEXP (x0, 0);
  goto L480;

  L736:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, DImode))
    {
      ro[1] = x2;
      goto L737;
    }
  x1 = XEXP (x0, 0);
  goto L480;

  L737:
  x2 = XEXP (x1, 1);
  if (pnum_clobbers != 0 && general_operand (x2, DImode))
    {
      ro[2] = x2;
      *pnum_clobbers = 1;
      return 135;
    }
  x1 = XEXP (x0, 0);
  goto L480;

  L953:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, DImode))
    {
      ro[1] = x2;
      return 173;
    }
  x1 = XEXP (x0, 0);
  goto L480;

  L481:
  x1 = XEXP (x0, 1);
  if (GET_MODE (x1) != DImode)
    {
      x1 = XEXP (x0, 0);
      goto L1546;
    }
  switch (GET_CODE (x1))
    {
    case ZERO_EXTEND:
      goto L482;
    case SIGN_EXTEND:
      goto L486;
    case MULT:
      goto L794;
    case ASHIFT:
      goto L1078;
    case ASHIFTRT:
      goto L1106;
    case LSHIFTRT:
      goto L1134;
    }
  x1 = XEXP (x0, 0);
  goto L1546;

  L482:
  x2 = XEXP (x1, 0);
  if (register_operand (x2, SImode))
    {
      ro[1] = x2;
      return 88;
    }
  x1 = XEXP (x0, 0);
  goto L1546;

  L486:
  x2 = XEXP (x1, 0);
  if (register_operand (x2, SImode))
    {
      ro[1] = x2;
      return 89;
    }
  x1 = XEXP (x0, 0);
  goto L1546;

  L794:
  x2 = XEXP (x1, 0);
  if (GET_MODE (x2) != DImode)
    {
      x1 = XEXP (x0, 0);
      goto L1546;
    }
  switch (GET_CODE (x2))
    {
    case ZERO_EXTEND:
      goto L795;
    case SIGN_EXTEND:
      goto L802;
    }
  x1 = XEXP (x0, 0);
  goto L1546;

  L795:
  x3 = XEXP (x2, 0);
  if (register_operand (x3, SImode))
    {
      ro[1] = x3;
      goto L796;
    }
  x1 = XEXP (x0, 0);
  goto L1546;

  L796:
  x2 = XEXP (x1, 1);
  if (GET_MODE (x2) == DImode && GET_CODE (x2) == ZERO_EXTEND && 1)
    goto L797;
  x1 = XEXP (x0, 0);
  goto L1546;

  L797:
  x3 = XEXP (x2, 0);
  if (nonimmediate_operand (x3, SImode))
    {
      ro[2] = x3;
      if (TARGET_WIDE_MULTIPLY)
	return 148;
      }
  x1 = XEXP (x0, 0);
  goto L1546;

  L802:
  x3 = XEXP (x2, 0);
  if (register_operand (x3, SImode))
    {
      ro[1] = x3;
      goto L803;
    }
  x1 = XEXP (x0, 0);
  goto L1546;

  L803:
  x2 = XEXP (x1, 1);
  if (GET_MODE (x2) == DImode && GET_CODE (x2) == SIGN_EXTEND && 1)
    goto L804;
  x1 = XEXP (x0, 0);
  goto L1546;

  L804:
  x3 = XEXP (x2, 0);
  if (nonimmediate_operand (x3, SImode))
    {
      ro[2] = x3;
      if (TARGET_WIDE_MULTIPLY)
	return 149;
      }
  x1 = XEXP (x0, 0);
  goto L1546;

  L1078:
  x2 = XEXP (x1, 0);
  if (register_operand (x2, DImode))
    {
      ro[1] = x2;
      goto L1079;
    }
  x1 = XEXP (x0, 0);
  goto L1546;

  L1079:
  x2 = XEXP (x1, 1);
  if (GET_CODE (x2) == CONST_INT && 1)
    {
      ro[2] = x2;
      return 203;
    }
  x1 = XEXP (x0, 0);
  goto L1546;

  L1106:
  x2 = XEXP (x1, 0);
  if (register_operand (x2, DImode))
    {
      ro[1] = x2;
      goto L1107;
    }
  x1 = XEXP (x0, 0);
  goto L1546;

  L1107:
  x2 = XEXP (x1, 1);
  if (GET_CODE (x2) == CONST_INT && 1)
    {
      ro[2] = x2;
      return 209;
    }
  x1 = XEXP (x0, 0);
  goto L1546;

  L1134:
  x2 = XEXP (x1, 0);
  if (register_operand (x2, DImode))
    {
      ro[1] = x2;
      goto L1135;
    }
  x1 = XEXP (x0, 0);
  goto L1546;

  L1135:
  x2 = XEXP (x1, 1);
  if (GET_CODE (x2) == CONST_INT && 1)
    {
      ro[2] = x2;
      return 215;
    }
  x1 = XEXP (x0, 0);
  goto L1546;
 L2:
  tem = recog_2 (x0, insn, pnum_clobbers);
  if (tem >= 0) return tem;
  x1 = XEXP (x0, 0);
  goto L1546;

  L320:
  x2 = XEXP (x1, 0);
  switch (GET_MODE (x2))
    {
    case HImode:
      if (general_operand (x2, HImode))
	{
	  ro[0] = x2;
	  goto L321;
	}
      break;
    case QImode:
      if (general_operand (x2, QImode))
	{
	  ro[0] = x2;
	  goto L337;
	}
    }
  goto L1546;

  L321:
  x1 = XEXP (x0, 1);
  if (general_operand (x1, HImode))
    {
      ro[1] = x1;
      if ((!TARGET_MOVE || GET_CODE (operands[0]) != MEM) || (GET_CODE (operands[1]) != MEM))
	return 57;
      }
  x1 = XEXP (x0, 0);
  goto L1546;

  L337:
  x1 = XEXP (x0, 1);
  if (general_operand (x1, QImode))
    {
      ro[1] = x1;
      if ((!TARGET_MOVE || GET_CODE (operands[0]) != MEM) || (GET_CODE (operands[1]) != MEM))
	return 64;
      }
  x1 = XEXP (x0, 0);
  goto L1546;

  L1480:
  x1 = XEXP (x0, 1);
  switch (GET_CODE (x1))
    {
    case MINUS:
      if (GET_MODE (x1) == SImode && 1)
	goto L1481;
      break;
    case IF_THEN_ELSE:
      goto L1280;
    case LABEL_REF:
      goto L1460;
    }
  L1463:
  if (general_operand (x1, SImode))
    {
      ro[0] = x1;
      return 283;
    }
  x1 = XEXP (x0, 0);
  goto L1546;

  L1481:
  x2 = XEXP (x1, 0);
  if (GET_MODE (x2) == SImode && GET_CODE (x2) == REG && XINT (x2, 0) == 3 && 1)
    goto L1482;
  x1 = XEXP (x0, 0);
  goto L1546;

  L1482:
  x2 = XEXP (x1, 1);
  if (GET_MODE (x2) == SImode && GET_CODE (x2) == MEM && 1)
    goto L1483;
  x1 = XEXP (x0, 0);
  goto L1546;

  L1483:
  x3 = XEXP (x2, 0);
  if (GET_MODE (x3) == SImode && GET_CODE (x3) == PLUS && 1)
    goto L1484;
  x1 = XEXP (x0, 0);
  goto L1546;

  L1484:
  x4 = XEXP (x3, 0);
  if (GET_MODE (x4) == SImode && GET_CODE (x4) == MULT && 1)
    goto L1485;
  x1 = XEXP (x0, 0);
  goto L1546;

  L1485:
  x5 = XEXP (x4, 0);
  if (register_operand (x5, SImode))
    {
      ro[0] = x5;
      goto L1486;
    }
  x1 = XEXP (x0, 0);
  goto L1546;

  L1486:
  x5 = XEXP (x4, 1);
  if (GET_CODE (x5) == CONST_INT && XWINT (x5, 0) == 4 && 1)
    goto L1487;
  x1 = XEXP (x0, 0);
  goto L1546;

  L1487:
  x4 = XEXP (x3, 1);
  if (GET_CODE (x4) == LABEL_REF && 1)
    goto L1488;
  x1 = XEXP (x0, 0);
  goto L1546;

  L1488:
  x5 = XEXP (x4, 0);
  if (pnum_clobbers != 0 && 1)
    {
      ro[1] = x5;
      *pnum_clobbers = 1;
      return 285;
    }
  x1 = XEXP (x0, 0);
  goto L1546;
 L1280:
  tem = recog_3 (x0, insn, pnum_clobbers);
  if (tem >= 0) return tem;
  x1 = XEXP (x0, 0);
  goto L1546;

  L1460:
  x2 = XEXP (x1, 0);
  ro[0] = x2;
  return 282;

  L1547:
  x1 = XEXP (x0, 1);
  if (GET_CODE (x1) == CALL && 1)
    goto L1548;
  x1 = XEXP (x0, 0);
  goto L1613;

  L1548:
  x2 = XEXP (x1, 0);
  if (call_insn_operand (x2, QImode))
    {
      ro[1] = x2;
      goto L1549;
    }
  L1553:
  if (GET_MODE (x2) == QImode && GET_CODE (x2) == MEM && 1)
    goto L1554;
  x1 = XEXP (x0, 0);
  goto L1613;

  L1549:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, SImode))
    {
      ro[2] = x2;
      return 297;
    }
  x2 = XEXP (x1, 0);
  goto L1553;

  L1554:
  x3 = XEXP (x2, 0);
  if (symbolic_operand (x3, SImode))
    {
      ro[1] = x3;
      goto L1555;
    }
  x1 = XEXP (x0, 0);
  goto L1613;

  L1555:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, SImode))
    {
      ro[2] = x2;
      if (!HALF_PIC_P ())
	return 298;
      }
  x1 = XEXP (x0, 0);
  goto L1613;

  L1614:
  x1 = XEXP (x0, 1);
  if (GET_MODE (x1) == SImode && GET_CODE (x1) == PLUS && 1)
    goto L1615;
  goto ret0;

  L1615:
  x2 = XEXP (x1, 0);
  if (GET_MODE (x2) == SImode && GET_CODE (x2) == FFS && 1)
    goto L1616;
  goto ret0;

  L1616:
  x3 = XEXP (x2, 0);
  if (general_operand (x3, SImode))
    {
      ro[1] = x3;
      goto L1617;
    }
  goto ret0;

  L1617:
  x2 = XEXP (x1, 1);
  if (GET_CODE (x2) == CONST_INT && XWINT (x2, 0) == -1 && 1)
    return 309;
  goto ret0;

  L1620:
  x1 = XEXP (x0, 1);
  if (GET_MODE (x1) == HImode && GET_CODE (x1) == PLUS && 1)
    goto L1621;
  goto ret0;

  L1621:
  x2 = XEXP (x1, 0);
  if (GET_MODE (x2) == HImode && GET_CODE (x2) == FFS && 1)
    goto L1622;
  goto ret0;

  L1622:
  x3 = XEXP (x2, 0);
  if (general_operand (x3, SImode))
    {
      ro[1] = x3;
      goto L1623;
    }
  goto ret0;

  L1623:
  x2 = XEXP (x1, 1);
  if (GET_CODE (x2) == CONST_INT && XWINT (x2, 0) == -1 && 1)
    return 311;
  goto ret0;

  L1626:
  x1 = XEXP (x0, 1);
  if (binary_387_op (x1, DFmode))
    {
      ro[3] = x1;
      goto L1632;
    }
  goto ret0;

  L1632:
  x2 = XEXP (x1, 0);
  switch (GET_MODE (x2))
    {
    case DFmode:
      switch (GET_CODE (x2))
	{
	case FLOAT:
	  goto L1633;
	case FLOAT_EXTEND:
	  goto L1668;
	case SUBREG:
	case REG:
	case MEM:
	  if (nonimmediate_operand (x2, DFmode))
	    {
	      ro[1] = x2;
	      goto L1628;
	    }
	}
    }
  L1673:
  if (general_operand (x2, DFmode))
    {
      ro[1] = x2;
      goto L1674;
    }
  goto ret0;

  L1633:
  x3 = XEXP (x2, 0);
  if (general_operand (x3, SImode))
    {
      ro[1] = x3;
      goto L1634;
    }
  goto ret0;

  L1634:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, DFmode))
    {
      ro[2] = x2;
      if (TARGET_80387)
	return 313;
      }
  goto ret0;

  L1668:
  x3 = XEXP (x2, 0);
  if (general_operand (x3, SFmode))
    {
      ro[1] = x3;
      goto L1669;
    }
  goto ret0;

  L1669:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, DFmode))
    {
      ro[2] = x2;
      if (TARGET_80387)
	return 319;
      }
  goto ret0;

  L1628:
  x2 = XEXP (x1, 1);
  if (nonimmediate_operand (x2, DFmode))
    {
      ro[2] = x2;
      if (TARGET_80387)
	return 312;
      }
  x2 = XEXP (x1, 0);
  goto L1673;

  L1674:
  x2 = XEXP (x1, 1);
  if (GET_MODE (x2) != DFmode)
    goto ret0;
  switch (GET_CODE (x2))
    {
    case FLOAT:
      goto L1675;
    case FLOAT_EXTEND:
      goto L1681;
    }
  goto ret0;

  L1675:
  x3 = XEXP (x2, 0);
  if (general_operand (x3, SImode))
    {
      ro[2] = x3;
      if (TARGET_80387)
	return 320;
      }
  goto ret0;

  L1681:
  x3 = XEXP (x2, 0);
  if (general_operand (x3, SFmode))
    {
      ro[2] = x3;
      if (TARGET_80387)
	return 321;
      }
  goto ret0;

  L1637:
  x1 = XEXP (x0, 1);
  if (binary_387_op (x1, XFmode))
    {
      ro[3] = x1;
      goto L1643;
    }
  goto ret0;

  L1643:
  x2 = XEXP (x1, 0);
  switch (GET_MODE (x2))
    {
    case XFmode:
      switch (GET_CODE (x2))
	{
	case FLOAT:
	  goto L1644;
	case FLOAT_EXTEND:
	  goto L1650;
	case SUBREG:
	case REG:
	case MEM:
	  if (nonimmediate_operand (x2, XFmode))
	    {
	      ro[1] = x2;
	      goto L1639;
	    }
	}
    }
  L1655:
  if (general_operand (x2, XFmode))
    {
      ro[1] = x2;
      goto L1656;
    }
  goto ret0;

  L1644:
  x3 = XEXP (x2, 0);
  if (general_operand (x3, SImode))
    {
      ro[1] = x3;
      goto L1645;
    }
  goto ret0;

  L1645:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, XFmode))
    {
      ro[2] = x2;
      if (TARGET_80387)
	return 315;
      }
  goto ret0;

  L1650:
  x3 = XEXP (x2, 0);
  if (general_operand (x3, SFmode))
    {
      ro[1] = x3;
      goto L1651;
    }
  goto ret0;

  L1651:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, XFmode))
    {
      ro[2] = x2;
      if (TARGET_80387)
	return 316;
      }
  goto ret0;

  L1639:
  x2 = XEXP (x1, 1);
  if (nonimmediate_operand (x2, XFmode))
    {
      ro[2] = x2;
      if (TARGET_80387)
	return 314;
      }
  x2 = XEXP (x1, 0);
  goto L1655;

  L1656:
  x2 = XEXP (x1, 1);
  if (GET_MODE (x2) != XFmode)
    goto ret0;
  switch (GET_CODE (x2))
    {
    case FLOAT:
      goto L1657;
    case FLOAT_EXTEND:
      goto L1663;
    }
  goto ret0;

  L1657:
  x3 = XEXP (x2, 0);
  if (general_operand (x3, SImode))
    {
      ro[2] = x3;
      if (TARGET_80387)
	return 317;
      }
  goto ret0;

  L1663:
  x3 = XEXP (x2, 0);
  if (general_operand (x3, SFmode))
    {
      ro[2] = x3;
      if (TARGET_80387)
	return 318;
      }
  goto ret0;

  L1684:
  x1 = XEXP (x0, 1);
  if (binary_387_op (x1, SFmode))
    {
      ro[3] = x1;
      goto L1690;
    }
  goto ret0;

  L1690:
  x2 = XEXP (x1, 0);
  switch (GET_MODE (x2))
    {
    case SFmode:
      if (GET_CODE (x2) == FLOAT && 1)
	goto L1691;
      if (nonimmediate_operand (x2, SFmode))
	{
	  ro[1] = x2;
	  goto L1686;
	}
    }
  L1696:
  if (general_operand (x2, SFmode))
    {
      ro[1] = x2;
      goto L1697;
    }
  goto ret0;

  L1691:
  x3 = XEXP (x2, 0);
  if (general_operand (x3, SImode))
    {
      ro[1] = x3;
      goto L1692;
    }
  goto ret0;

  L1692:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, SFmode))
    {
      ro[2] = x2;
      if (TARGET_80387)
	return 323;
      }
  goto ret0;

  L1686:
  x2 = XEXP (x1, 1);
  if (nonimmediate_operand (x2, SFmode))
    {
      ro[2] = x2;
      if (TARGET_80387)
	return 322;
      }
  x2 = XEXP (x1, 0);
  goto L1696;

  L1697:
  x2 = XEXP (x1, 1);
  if (GET_MODE (x2) == SFmode && GET_CODE (x2) == FLOAT && 1)
    goto L1698;
  goto ret0;

  L1698:
  x3 = XEXP (x2, 0);
  if (general_operand (x3, SImode))
    {
      ro[2] = x3;
      if (TARGET_80387)
	return 324;
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
    case SFmode:
      if (GET_CODE (x2) == MEM && push_operand (x2, SFmode))
	{
	  ro[0] = x2;
	  goto L344;
	}
    L354:
      if (memory_operand (x2, SFmode))
	{
	  ro[0] = x2;
	  goto L355;
	}
    L366:
      if (register_operand (x2, SFmode))
	{
	  ro[0] = x2;
	  goto L367;
	}
    L513:
      if (nonimmediate_operand (x2, SFmode))
	{
	  ro[0] = x2;
	  goto L514;
	}
      break;
    case DFmode:
      if (register_operand (x2, DFmode))
	{
	  ro[0] = x2;
	  goto L404;
	}
      break;
    case XFmode:
      if (register_operand (x2, XFmode))
	{
	  ro[0] = x2;
	  goto L441;
	}
      break;
    case DImode:
      if (general_operand (x2, DImode))
	{
	  ro[0] = x2;
	  goto L697;
	}
    L1082:
      if (register_operand (x2, DImode))
	{
	  ro[0] = x2;
	  goto L1083;
	}
      break;
    case SImode:
      if (register_operand (x2, SImode))
	{
	  ro[0] = x2;
	  goto L808;
	}
      break;
    case HImode:
      if (register_operand (x2, HImode))
	{
	  ro[0] = x2;
	  goto L875;
	}
    }
  switch (GET_CODE (x2))
    {
    case CC0:
      goto L12;
    case PC:
      goto L1467;
    }
  L1524:
  ro[0] = x2;
  goto L1525;
  L1701:
  if (register_operand (x2, SImode))
    {
      ro[0] = x2;
      goto L1702;
    }
  goto ret0;

  L344:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, SFmode))
    {
      ro[1] = x2;
      goto L345;
    }
  x2 = XEXP (x1, 0);
  goto L354;

  L345:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L346;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L354;

  L346:
  x2 = XEXP (x1, 0);
  if (scratch_operand (x2, SImode))
    {
      ro[2] = x2;
      return 67;
    }
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L354;

  L355:
  x2 = XEXP (x1, 1);
  if (memory_operand (x2, SFmode))
    {
      ro[1] = x2;
      goto L356;
    }
  x2 = XEXP (x1, 0);
  goto L366;

  L356:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L357;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L366;

  L357:
  x2 = XEXP (x1, 0);
  if (scratch_operand (x2, SImode))
    {
      ro[2] = x2;
      return 68;
    }
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L366;

  L367:
  x2 = XEXP (x1, 1);
  if (register_operand (x2, SFmode))
    {
      ro[1] = x2;
      goto L368;
    }
  x2 = XEXP (x1, 0);
  goto L513;

  L368:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == SET && 1)
    goto L369;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L513;

  L369:
  x2 = XEXP (x1, 0);
  if (rtx_equal_p (x2, ro[1]) && 1)
    goto L370;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L513;

  L370:
  x2 = XEXP (x1, 1);
  if (rtx_equal_p (x2, ro[0]) && 1)
    return 70;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L513;

  L514:
  x2 = XEXP (x1, 1);
  if (GET_MODE (x2) == SFmode && GET_CODE (x2) == FLOAT_TRUNCATE && 1)
    goto L515;
  x2 = XEXP (x1, 0);
  goto L1524;

  L515:
  x3 = XEXP (x2, 0);
  if (register_operand (x3, DFmode))
    {
      ro[1] = x3;
      goto L516;
    }
  x2 = XEXP (x1, 0);
  goto L1524;

  L516:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L517;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1524;

  L517:
  x2 = XEXP (x1, 0);
  if (memory_operand (x2, SFmode))
    {
      ro[2] = x2;
      if (TARGET_80387)
	return 97;
      }
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1524;

  L404:
  x2 = XEXP (x1, 1);
  if (register_operand (x2, DFmode))
    {
      ro[1] = x2;
      goto L405;
    }
  x2 = XEXP (x1, 0);
  goto L1524;

  L405:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == SET && 1)
    goto L406;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1524;

  L406:
  x2 = XEXP (x1, 0);
  if (rtx_equal_p (x2, ro[1]) && 1)
    goto L407;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1524;

  L407:
  x2 = XEXP (x1, 1);
  if (rtx_equal_p (x2, ro[0]) && 1)
    return 76;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1524;

  L441:
  x2 = XEXP (x1, 1);
  if (register_operand (x2, XFmode))
    {
      ro[1] = x2;
      goto L442;
    }
  x2 = XEXP (x1, 0);
  goto L1524;

  L442:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == SET && 1)
    goto L443;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1524;

  L443:
  x2 = XEXP (x1, 0);
  if (rtx_equal_p (x2, ro[1]) && 1)
    goto L444;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1524;

  L444:
  x2 = XEXP (x1, 1);
  if (rtx_equal_p (x2, ro[0]) && 1)
    return 82;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1524;

  L697:
  x2 = XEXP (x1, 1);
  if (GET_MODE (x2) != DImode)
    {
      x2 = XEXP (x1, 0);
      goto L1082;
    }
  switch (GET_CODE (x2))
    {
    case PLUS:
      goto L698;
    case MINUS:
      goto L729;
    }
  x2 = XEXP (x1, 0);
  goto L1082;

  L698:
  x3 = XEXP (x2, 0);
  if (general_operand (x3, DImode))
    {
      ro[1] = x3;
      goto L699;
    }
  x2 = XEXP (x1, 0);
  goto L1082;

  L699:
  x3 = XEXP (x2, 1);
  if (general_operand (x3, DImode))
    {
      ro[2] = x3;
      goto L700;
    }
  x2 = XEXP (x1, 0);
  goto L1082;

  L700:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L701;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1082;

  L701:
  x2 = XEXP (x1, 0);
  if (scratch_operand (x2, SImode))
    {
      ro[3] = x2;
      return 127;
    }
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1082;

  L729:
  x3 = XEXP (x2, 0);
  if (general_operand (x3, DImode))
    {
      ro[1] = x3;
      goto L730;
    }
  x2 = XEXP (x1, 0);
  goto L1082;

  L730:
  x3 = XEXP (x2, 1);
  if (general_operand (x3, DImode))
    {
      ro[2] = x3;
      goto L731;
    }
  x2 = XEXP (x1, 0);
  goto L1082;

  L731:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L732;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1082;

  L732:
  x2 = XEXP (x1, 0);
  if (scratch_operand (x2, SImode))
    {
      ro[3] = x2;
      return 135;
    }
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1082;

  L1083:
  x2 = XEXP (x1, 1);
  if (GET_MODE (x2) != DImode)
    {
      x2 = XEXP (x1, 0);
      goto L1524;
    }
  switch (GET_CODE (x2))
    {
    case ASHIFT:
      goto L1084;
    case ASHIFTRT:
      goto L1112;
    case LSHIFTRT:
      goto L1140;
    }
  x2 = XEXP (x1, 0);
  goto L1524;

  L1084:
  x3 = XEXP (x2, 0);
  if (register_operand (x3, DImode))
    {
      ro[1] = x3;
      goto L1085;
    }
  x2 = XEXP (x1, 0);
  goto L1524;

  L1085:
  x3 = XEXP (x2, 1);
  if (register_operand (x3, QImode))
    {
      ro[2] = x3;
      goto L1086;
    }
  x2 = XEXP (x1, 0);
  goto L1524;

  L1086:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L1087;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1524;

  L1087:
  x2 = XEXP (x1, 0);
  if (rtx_equal_p (x2, ro[2]) && 1)
    return 204;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1524;

  L1112:
  x3 = XEXP (x2, 0);
  if (register_operand (x3, DImode))
    {
      ro[1] = x3;
      goto L1113;
    }
  x2 = XEXP (x1, 0);
  goto L1524;

  L1113:
  x3 = XEXP (x2, 1);
  if (register_operand (x3, QImode))
    {
      ro[2] = x3;
      goto L1114;
    }
  x2 = XEXP (x1, 0);
  goto L1524;

  L1114:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L1115;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1524;

  L1115:
  x2 = XEXP (x1, 0);
  if (rtx_equal_p (x2, ro[2]) && 1)
    return 210;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1524;

  L1140:
  x3 = XEXP (x2, 0);
  if (register_operand (x3, DImode))
    {
      ro[1] = x3;
      goto L1141;
    }
  x2 = XEXP (x1, 0);
  goto L1524;

  L1141:
  x3 = XEXP (x2, 1);
  if (register_operand (x3, QImode))
    {
      ro[2] = x3;
      goto L1142;
    }
  x2 = XEXP (x1, 0);
  goto L1524;

  L1142:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L1143;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1524;

  L1143:
  x2 = XEXP (x1, 0);
  if (rtx_equal_p (x2, ro[2]) && 1)
    return 216;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1524;

  L808:
  x2 = XEXP (x1, 1);
  if (GET_MODE (x2) != SImode)
    {
      x2 = XEXP (x1, 0);
      goto L1524;
    }
  switch (GET_CODE (x2))
    {
    case TRUNCATE:
      goto L809;
    case DIV:
      goto L865;
    case UDIV:
      goto L887;
    }
  x2 = XEXP (x1, 0);
  goto L1524;

  L809:
  x3 = XEXP (x2, 0);
  if (GET_MODE (x3) == DImode && GET_CODE (x3) == LSHIFTRT && 1)
    goto L810;
  x2 = XEXP (x1, 0);
  goto L1524;

  L810:
  x4 = XEXP (x3, 0);
  if (GET_MODE (x4) == DImode && GET_CODE (x4) == MULT && 1)
    goto L811;
  x2 = XEXP (x1, 0);
  goto L1524;

  L811:
  x5 = XEXP (x4, 0);
  if (GET_MODE (x5) != DImode)
    {
      x2 = XEXP (x1, 0);
      goto L1524;
    }
  switch (GET_CODE (x5))
    {
    case ZERO_EXTEND:
      goto L812;
    case SIGN_EXTEND:
      goto L835;
    }
  x2 = XEXP (x1, 0);
  goto L1524;

  L812:
  x6 = XEXP (x5, 0);
  if (register_operand (x6, SImode))
    {
      ro[1] = x6;
      goto L813;
    }
  x2 = XEXP (x1, 0);
  goto L1524;

  L813:
  x5 = XEXP (x4, 1);
  if (GET_MODE (x5) == DImode && GET_CODE (x5) == ZERO_EXTEND && 1)
    goto L814;
  x2 = XEXP (x1, 0);
  goto L1524;

  L814:
  x6 = XEXP (x5, 0);
  if (nonimmediate_operand (x6, SImode))
    {
      ro[2] = x6;
      goto L815;
    }
  x2 = XEXP (x1, 0);
  goto L1524;

  L815:
  x4 = XEXP (x3, 1);
  if (GET_CODE (x4) == CONST_INT && XWINT (x4, 0) == 32 && 1)
    goto L816;
  x2 = XEXP (x1, 0);
  goto L1524;

  L816:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L817;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1524;

  L817:
  x2 = XEXP (x1, 0);
  if (scratch_operand (x2, SImode))
    {
      ro[3] = x2;
      if (TARGET_WIDE_MULTIPLY)
	return 150;
      }
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1524;

  L835:
  x6 = XEXP (x5, 0);
  if (register_operand (x6, SImode))
    {
      ro[1] = x6;
      goto L836;
    }
  x2 = XEXP (x1, 0);
  goto L1524;

  L836:
  x5 = XEXP (x4, 1);
  if (GET_MODE (x5) == DImode && GET_CODE (x5) == SIGN_EXTEND && 1)
    goto L837;
  x2 = XEXP (x1, 0);
  goto L1524;

  L837:
  x6 = XEXP (x5, 0);
  if (nonimmediate_operand (x6, SImode))
    {
      ro[2] = x6;
      goto L838;
    }
  x2 = XEXP (x1, 0);
  goto L1524;

  L838:
  x4 = XEXP (x3, 1);
  if (GET_CODE (x4) == CONST_INT && XWINT (x4, 0) == 32 && 1)
    goto L839;
  x2 = XEXP (x1, 0);
  goto L1524;

  L839:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L840;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1524;

  L840:
  x2 = XEXP (x1, 0);
  if (scratch_operand (x2, SImode))
    {
      ro[3] = x2;
      if (TARGET_WIDE_MULTIPLY)
	return 151;
      }
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1524;

  L865:
  x3 = XEXP (x2, 0);
  if (register_operand (x3, SImode))
    {
      ro[1] = x3;
      goto L866;
    }
  x2 = XEXP (x1, 0);
  goto L1524;

  L866:
  x3 = XEXP (x2, 1);
  if (general_operand (x3, SImode))
    {
      ro[2] = x3;
      goto L867;
    }
  x2 = XEXP (x1, 0);
  goto L1524;

  L867:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == SET && 1)
    goto L868;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1524;

  L868:
  x2 = XEXP (x1, 0);
  if (register_operand (x2, SImode))
    {
      ro[3] = x2;
      goto L869;
    }
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1524;

  L869:
  x2 = XEXP (x1, 1);
  if (GET_MODE (x2) == SImode && GET_CODE (x2) == MOD && 1)
    goto L870;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1524;

  L870:
  x3 = XEXP (x2, 0);
  if (rtx_equal_p (x3, ro[1]) && 1)
    goto L871;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1524;

  L871:
  x3 = XEXP (x2, 1);
  if (rtx_equal_p (x3, ro[2]) && 1)
    return 160;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1524;

  L887:
  x3 = XEXP (x2, 0);
  if (register_operand (x3, SImode))
    {
      ro[1] = x3;
      goto L888;
    }
  x2 = XEXP (x1, 0);
  goto L1524;

  L888:
  x3 = XEXP (x2, 1);
  if (general_operand (x3, SImode))
    {
      ro[2] = x3;
      goto L889;
    }
  x2 = XEXP (x1, 0);
  goto L1524;

  L889:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == SET && 1)
    goto L890;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1524;

  L890:
  x2 = XEXP (x1, 0);
  if (register_operand (x2, SImode))
    {
      ro[3] = x2;
      goto L891;
    }
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1524;

  L891:
  x2 = XEXP (x1, 1);
  if (GET_MODE (x2) == SImode && GET_CODE (x2) == UMOD && 1)
    goto L892;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1524;

  L892:
  x3 = XEXP (x2, 0);
  if (rtx_equal_p (x3, ro[1]) && 1)
    goto L893;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1524;

  L893:
  x3 = XEXP (x2, 1);
  if (rtx_equal_p (x3, ro[2]) && 1)
    return 162;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1524;

  L875:
  x2 = XEXP (x1, 1);
  if (GET_MODE (x2) != HImode)
    {
      x2 = XEXP (x1, 0);
      goto L1524;
    }
  switch (GET_CODE (x2))
    {
    case DIV:
      goto L876;
    case UDIV:
      goto L898;
    }
  x2 = XEXP (x1, 0);
  goto L1524;

  L876:
  x3 = XEXP (x2, 0);
  if (register_operand (x3, HImode))
    {
      ro[1] = x3;
      goto L877;
    }
  x2 = XEXP (x1, 0);
  goto L1524;

  L877:
  x3 = XEXP (x2, 1);
  if (general_operand (x3, HImode))
    {
      ro[2] = x3;
      goto L878;
    }
  x2 = XEXP (x1, 0);
  goto L1524;

  L878:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == SET && 1)
    goto L879;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1524;

  L879:
  x2 = XEXP (x1, 0);
  if (register_operand (x2, HImode))
    {
      ro[3] = x2;
      goto L880;
    }
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1524;

  L880:
  x2 = XEXP (x1, 1);
  if (GET_MODE (x2) == HImode && GET_CODE (x2) == MOD && 1)
    goto L881;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1524;

  L881:
  x3 = XEXP (x2, 0);
  if (rtx_equal_p (x3, ro[1]) && 1)
    goto L882;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1524;

  L882:
  x3 = XEXP (x2, 1);
  if (rtx_equal_p (x3, ro[2]) && 1)
    return 161;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1524;

  L898:
  x3 = XEXP (x2, 0);
  if (register_operand (x3, HImode))
    {
      ro[1] = x3;
      goto L899;
    }
  x2 = XEXP (x1, 0);
  goto L1524;

  L899:
  x3 = XEXP (x2, 1);
  if (general_operand (x3, HImode))
    {
      ro[2] = x3;
      goto L900;
    }
  x2 = XEXP (x1, 0);
  goto L1524;

  L900:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == SET && 1)
    goto L901;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1524;

  L901:
  x2 = XEXP (x1, 0);
  if (register_operand (x2, HImode))
    {
      ro[3] = x2;
      goto L902;
    }
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1524;

  L902:
  x2 = XEXP (x1, 1);
  if (GET_MODE (x2) == HImode && GET_CODE (x2) == UMOD && 1)
    goto L903;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1524;

  L903:
  x3 = XEXP (x2, 0);
  if (rtx_equal_p (x3, ro[1]) && 1)
    goto L904;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1524;

  L904:
  x3 = XEXP (x2, 1);
  if (rtx_equal_p (x3, ro[2]) && 1)
    return 163;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1524;

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
  goto L1524;

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
  goto L1524;

  L129:
  x3 = XEXP (x2, 1);
  if (register_operand (x3, XFmode))
    {
      ro[1] = x3;
      goto L130;
    }
  x2 = XEXP (x1, 0);
  goto L1524;

  L130:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L131;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1524;

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
  goto L1524;

  L215:
  x3 = XEXP (x2, 1);
  if (register_operand (x3, DFmode))
    {
      ro[1] = x3;
      goto L216;
    }
  x2 = XEXP (x1, 0);
  goto L1524;

  L216:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L217;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1524;

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
  goto L1524;

  L271:
  x3 = XEXP (x2, 1);
  if (register_operand (x3, SFmode))
    {
      ro[1] = x3;
      goto L272;
    }
  x2 = XEXP (x1, 0);
  goto L1524;

  L272:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L273;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1524;

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
  goto L1524;

  L1467:
  x2 = XEXP (x1, 1);
  if (GET_MODE (x2) == SImode && GET_CODE (x2) == MINUS && 1)
    goto L1468;
  if (general_operand (x2, SImode))
    {
      ro[0] = x2;
      goto L1493;
    }
  x2 = XEXP (x1, 0);
  goto L1524;

  L1468:
  x3 = XEXP (x2, 0);
  if (GET_MODE (x3) == SImode && GET_CODE (x3) == REG && XINT (x3, 0) == 3 && 1)
    goto L1469;
  x2 = XEXP (x1, 0);
  goto L1524;

  L1469:
  x3 = XEXP (x2, 1);
  if (GET_MODE (x3) == SImode && GET_CODE (x3) == MEM && 1)
    goto L1470;
  x2 = XEXP (x1, 0);
  goto L1524;

  L1470:
  x4 = XEXP (x3, 0);
  if (GET_MODE (x4) == SImode && GET_CODE (x4) == PLUS && 1)
    goto L1471;
  x2 = XEXP (x1, 0);
  goto L1524;

  L1471:
  x5 = XEXP (x4, 0);
  if (GET_MODE (x5) == SImode && GET_CODE (x5) == MULT && 1)
    goto L1472;
  x2 = XEXP (x1, 0);
  goto L1524;

  L1472:
  x6 = XEXP (x5, 0);
  if (register_operand (x6, SImode))
    {
      ro[0] = x6;
      goto L1473;
    }
  x2 = XEXP (x1, 0);
  goto L1524;

  L1473:
  x6 = XEXP (x5, 1);
  if (GET_CODE (x6) == CONST_INT && XWINT (x6, 0) == 4 && 1)
    goto L1474;
  x2 = XEXP (x1, 0);
  goto L1524;

  L1474:
  x5 = XEXP (x4, 1);
  if (GET_CODE (x5) == LABEL_REF && 1)
    goto L1475;
  x2 = XEXP (x1, 0);
  goto L1524;

  L1475:
  x6 = XEXP (x5, 0);
  ro[1] = x6;
  goto L1476;

  L1476:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L1477;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1524;

  L1477:
  x2 = XEXP (x1, 0);
  if (scratch_operand (x2, SImode))
    {
      ro[2] = x2;
      return 285;
    }
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1524;

  L1493:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == USE && 1)
    goto L1494;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1524;

  L1494:
  x2 = XEXP (x1, 0);
  if (GET_CODE (x2) == LABEL_REF && 1)
    goto L1495;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1524;

  L1495:
  x3 = XEXP (x2, 0);
  ro[1] = x3;
  return 286;

  L1525:
  x2 = XEXP (x1, 1);
  if (GET_CODE (x2) == CALL && 1)
    goto L1537;
  x2 = XEXP (x1, 0);
  goto L1701;

  L1537:
  x3 = XEXP (x2, 0);
  if (GET_MODE (x3) == QImode && GET_CODE (x3) == MEM && 1)
    goto L1538;
  L1526:
  if (call_insn_operand (x3, QImode))
    {
      ro[1] = x3;
      goto L1527;
    }
  x2 = XEXP (x1, 0);
  goto L1701;

  L1538:
  x4 = XEXP (x3, 0);
  if (symbolic_operand (x4, SImode))
    {
      ro[1] = x4;
      goto L1539;
    }
  goto L1526;

  L1539:
  x3 = XEXP (x2, 1);
  if (general_operand (x3, SImode))
    {
      ro[2] = x3;
      goto L1540;
    }
  x3 = XEXP (x2, 0);
  goto L1526;

  L1540:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == SET && 1)
    goto L1541;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 1);
  x3 = XEXP (x2, 0);
  goto L1526;

  L1541:
  x2 = XEXP (x1, 0);
  if (GET_MODE (x2) == SImode && GET_CODE (x2) == REG && XINT (x2, 0) == 7 && 1)
    goto L1542;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 1);
  x3 = XEXP (x2, 0);
  goto L1526;

  L1542:
  x2 = XEXP (x1, 1);
  if (GET_MODE (x2) == SImode && GET_CODE (x2) == PLUS && 1)
    goto L1543;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 1);
  x3 = XEXP (x2, 0);
  goto L1526;

  L1543:
  x3 = XEXP (x2, 0);
  if (GET_MODE (x3) == SImode && GET_CODE (x3) == REG && XINT (x3, 0) == 7 && 1)
    goto L1544;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 1);
  x3 = XEXP (x2, 0);
  goto L1526;

  L1544:
  x3 = XEXP (x2, 1);
  if (immediate_operand (x3, SImode))
    {
      ro[4] = x3;
      if (!HALF_PIC_P ())
	return 295;
      }
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 1);
  x3 = XEXP (x2, 0);
  goto L1526;

  L1527:
  x3 = XEXP (x2, 1);
  if (general_operand (x3, SImode))
    {
      ro[2] = x3;
      goto L1528;
    }
  x2 = XEXP (x1, 0);
  goto L1701;

  L1528:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == SET && 1)
    goto L1529;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1701;

  L1529:
  x2 = XEXP (x1, 0);
  if (GET_MODE (x2) == SImode && GET_CODE (x2) == REG && XINT (x2, 0) == 7 && 1)
    goto L1530;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1701;

  L1530:
  x2 = XEXP (x1, 1);
  if (GET_MODE (x2) == SImode && GET_CODE (x2) == PLUS && 1)
    goto L1531;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1701;

  L1531:
  x3 = XEXP (x2, 0);
  if (GET_MODE (x3) == SImode && GET_CODE (x3) == REG && XINT (x3, 0) == 7 && 1)
    goto L1532;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1701;

  L1532:
  x3 = XEXP (x2, 1);
  if (immediate_operand (x3, SImode))
    {
      ro[4] = x3;
      return 294;
    }
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1701;

  L1702:
  x2 = XEXP (x1, 1);
  if (GET_MODE (x2) == SImode && GET_CODE (x2) == UNSPEC && XINT (x2, 1) == 0 && XVECLEN (x2, 0) == 3 && 1)
    goto L1703;
  goto ret0;

  L1703:
  x3 = XVECEXP (x2, 0, 0);
  if (GET_MODE (x3) == BLKmode && GET_CODE (x3) == MEM && 1)
    goto L1704;
  goto ret0;

  L1704:
  x4 = XEXP (x3, 0);
  if (address_operand (x4, SImode))
    {
      ro[1] = x4;
      goto L1705;
    }
  goto ret0;

  L1705:
  x3 = XVECEXP (x2, 0, 1);
  if (register_operand (x3, QImode))
    {
      ro[2] = x3;
      goto L1706;
    }
  goto ret0;

  L1706:
  x3 = XVECEXP (x2, 0, 2);
  if (immediate_operand (x3, SImode))
    {
      ro[3] = x3;
      goto L1707;
    }
  goto ret0;

  L1707:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L1708;
  goto ret0;

  L1708:
  x2 = XEXP (x1, 0);
  if (rtx_equal_p (x2, ro[1]) && 1)
    return 326;
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
    case DFmode:
      if (GET_CODE (x2) == MEM && push_operand (x2, DFmode))
	{
	  ro[0] = x2;
	  goto L377;
	}
    L389:
      if (memory_operand (x2, DFmode))
	{
	  ro[0] = x2;
	  goto L390;
	}
      break;
    case XFmode:
      if (GET_CODE (x2) == MEM && push_operand (x2, XFmode))
	{
	  ro[0] = x2;
	  goto L414;
	}
    L426:
      if (memory_operand (x2, XFmode))
	{
	  ro[0] = x2;
	  goto L427;
	}
      break;
    case DImode:
      if (GET_CODE (x2) == MEM && push_operand (x2, DImode))
	{
	  ro[0] = x2;
	  goto L448;
	}
    L458:
      if (general_operand (x2, DImode))
	{
	  ro[0] = x2;
	  goto L459;
	}
      break;
    case SImode:
      if (general_operand (x2, SImode))
	{
	  ro[0] = x2;
	  goto L619;
	}
    }
  goto ret0;

  L377:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, DFmode))
    {
      ro[1] = x2;
      goto L378;
    }
  x2 = XEXP (x1, 0);
  goto L389;

  L378:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L379;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L389;

  L379:
  x2 = XEXP (x1, 0);
  if (scratch_operand (x2, SImode))
    {
      ro[2] = x2;
      goto L380;
    }
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L389;

  L380:
  x1 = XVECEXP (x0, 0, 2);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L381;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L389;

  L381:
  x2 = XEXP (x1, 0);
  if (scratch_operand (x2, SImode))
    {
      ro[3] = x2;
      return 73;
    }
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L389;

  L390:
  x2 = XEXP (x1, 1);
  if (memory_operand (x2, DFmode))
    {
      ro[1] = x2;
      goto L391;
    }
  goto ret0;

  L391:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L392;
  goto ret0;

  L392:
  x2 = XEXP (x1, 0);
  if (scratch_operand (x2, SImode))
    {
      ro[2] = x2;
      goto L393;
    }
  goto ret0;

  L393:
  x1 = XVECEXP (x0, 0, 2);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L394;
  goto ret0;

  L394:
  x2 = XEXP (x1, 0);
  if (scratch_operand (x2, SImode))
    {
      ro[3] = x2;
      return 74;
    }
  goto ret0;

  L414:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, XFmode))
    {
      ro[1] = x2;
      goto L415;
    }
  x2 = XEXP (x1, 0);
  goto L426;

  L415:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L416;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L426;

  L416:
  x2 = XEXP (x1, 0);
  if (scratch_operand (x2, SImode))
    {
      ro[2] = x2;
      goto L417;
    }
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L426;

  L417:
  x1 = XVECEXP (x0, 0, 2);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L418;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L426;

  L418:
  x2 = XEXP (x1, 0);
  if (scratch_operand (x2, SImode))
    {
      ro[3] = x2;
      return 79;
    }
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L426;

  L427:
  x2 = XEXP (x1, 1);
  if (memory_operand (x2, XFmode))
    {
      ro[1] = x2;
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
  if (scratch_operand (x2, SImode))
    {
      ro[2] = x2;
      goto L430;
    }
  goto ret0;

  L430:
  x1 = XVECEXP (x0, 0, 2);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L431;
  goto ret0;

  L431:
  x2 = XEXP (x1, 0);
  if (scratch_operand (x2, SImode))
    {
      ro[3] = x2;
      return 80;
    }
  goto ret0;

  L448:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, DImode))
    {
      ro[1] = x2;
      goto L449;
    }
  x2 = XEXP (x1, 0);
  goto L458;

  L449:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L450;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L458;

  L450:
  x2 = XEXP (x1, 0);
  if (scratch_operand (x2, SImode))
    {
      ro[2] = x2;
      goto L451;
    }
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L458;

  L451:
  x1 = XVECEXP (x0, 0, 2);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L452;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L458;

  L452:
  x2 = XEXP (x1, 0);
  if (scratch_operand (x2, SImode))
    {
      ro[3] = x2;
      return 83;
    }
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L458;

  L459:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, DImode))
    {
      ro[1] = x2;
      goto L460;
    }
  goto ret0;

  L460:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L461;
  goto ret0;

  L461:
  x2 = XEXP (x1, 0);
  if (scratch_operand (x2, SImode))
    {
      ro[2] = x2;
      goto L462;
    }
  goto ret0;

  L462:
  x1 = XVECEXP (x0, 0, 2);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L463;
  goto ret0;

  L463:
  x2 = XEXP (x1, 0);
  if (scratch_operand (x2, SImode))
    {
      ro[3] = x2;
      return 84;
    }
  goto ret0;

  L619:
  x2 = XEXP (x1, 1);
  if (GET_MODE (x2) == SImode && GET_CODE (x2) == FIX && 1)
    goto L620;
  goto ret0;

  L620:
  x3 = XEXP (x2, 0);
  if (GET_CODE (x3) != FIX)
    goto ret0;
  switch (GET_MODE (x3))
    {
    case XFmode:
      goto L621;
    case DFmode:
      goto L643;
    case SFmode:
      goto L665;
    }
  goto ret0;

  L621:
  x4 = XEXP (x3, 0);
  if (register_operand (x4, XFmode))
    {
      ro[1] = x4;
      goto L622;
    }
  goto ret0;

  L622:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L623;
  goto ret0;

  L623:
  x2 = XEXP (x1, 0);
  if (memory_operand (x2, SImode))
    {
      ro[2] = x2;
      goto L624;
    }
  goto ret0;

  L624:
  x1 = XVECEXP (x0, 0, 2);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L625;
  goto ret0;

  L625:
  x2 = XEXP (x1, 0);
  if (pnum_clobbers != 0 && memory_operand (x2, SImode))
    {
      ro[3] = x2;
      if (TARGET_80387)
	{
	  *pnum_clobbers = 1;
	  return 112;
	}
      }
  goto ret0;

  L643:
  x4 = XEXP (x3, 0);
  if (register_operand (x4, DFmode))
    {
      ro[1] = x4;
      goto L644;
    }
  goto ret0;

  L644:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L645;
  goto ret0;

  L645:
  x2 = XEXP (x1, 0);
  if (memory_operand (x2, SImode))
    {
      ro[2] = x2;
      goto L646;
    }
  goto ret0;

  L646:
  x1 = XVECEXP (x0, 0, 2);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L647;
  goto ret0;

  L647:
  x2 = XEXP (x1, 0);
  if (pnum_clobbers != 0 && memory_operand (x2, SImode))
    {
      ro[3] = x2;
      if (TARGET_80387)
	{
	  *pnum_clobbers = 1;
	  return 113;
	}
      }
  goto ret0;

  L665:
  x4 = XEXP (x3, 0);
  if (register_operand (x4, SFmode))
    {
      ro[1] = x4;
      goto L666;
    }
  goto ret0;

  L666:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L667;
  goto ret0;

  L667:
  x2 = XEXP (x1, 0);
  if (memory_operand (x2, SImode))
    {
      ro[2] = x2;
      goto L668;
    }
  goto ret0;

  L668:
  x1 = XVECEXP (x0, 0, 2);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L669;
  goto ret0;

  L669:
  x2 = XEXP (x1, 0);
  if (pnum_clobbers != 0 && memory_operand (x2, SImode))
    {
      ro[3] = x2;
      if (TARGET_80387)
	{
	  *pnum_clobbers = 1;
	  return 114;
	}
      }
  goto ret0;
 ret0: return -1;
}

int
recog_8 (x0, insn, pnum_clobbers)
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
	  goto L543;
	}
      break;
    case SImode:
      if (general_operand (x2, SImode))
	{
	  ro[0] = x2;
	  goto L607;
	}
    }
  goto ret0;

  L543:
  x2 = XEXP (x1, 1);
  if (GET_MODE (x2) == DImode && GET_CODE (x2) == FIX && 1)
    goto L544;
  goto ret0;

  L544:
  x3 = XEXP (x2, 0);
  if (GET_CODE (x3) != FIX)
    goto ret0;
  switch (GET_MODE (x3))
    {
    case XFmode:
      goto L545;
    case DFmode:
      goto L571;
    case SFmode:
      goto L597;
    }
  goto ret0;

  L545:
  x4 = XEXP (x3, 0);
  if (register_operand (x4, XFmode))
    {
      ro[1] = x4;
      goto L546;
    }
  goto ret0;

  L546:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L547;
  goto ret0;

  L547:
  x2 = XEXP (x1, 0);
  if (rtx_equal_p (x2, ro[1]) && 1)
    goto L548;
  goto ret0;

  L548:
  x1 = XVECEXP (x0, 0, 2);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L549;
  goto ret0;

  L549:
  x2 = XEXP (x1, 0);
  if (memory_operand (x2, SImode))
    {
      ro[2] = x2;
      goto L550;
    }
  goto ret0;

  L550:
  x1 = XVECEXP (x0, 0, 3);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L551;
  goto ret0;

  L551:
  x2 = XEXP (x1, 0);
  if (pnum_clobbers != 0 && memory_operand (x2, SImode))
    {
      ro[3] = x2;
      if (TARGET_80387)
	{
	  *pnum_clobbers = 1;
	  return 106;
	}
      }
  goto ret0;

  L571:
  x4 = XEXP (x3, 0);
  if (register_operand (x4, DFmode))
    {
      ro[1] = x4;
      goto L572;
    }
  goto ret0;

  L572:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L573;
  goto ret0;

  L573:
  x2 = XEXP (x1, 0);
  if (rtx_equal_p (x2, ro[1]) && 1)
    goto L574;
  goto ret0;

  L574:
  x1 = XVECEXP (x0, 0, 2);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L575;
  goto ret0;

  L575:
  x2 = XEXP (x1, 0);
  if (memory_operand (x2, SImode))
    {
      ro[2] = x2;
      goto L576;
    }
  goto ret0;

  L576:
  x1 = XVECEXP (x0, 0, 3);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L577;
  goto ret0;

  L577:
  x2 = XEXP (x1, 0);
  if (pnum_clobbers != 0 && memory_operand (x2, SImode))
    {
      ro[3] = x2;
      if (TARGET_80387)
	{
	  *pnum_clobbers = 1;
	  return 107;
	}
      }
  goto ret0;

  L597:
  x4 = XEXP (x3, 0);
  if (register_operand (x4, SFmode))
    {
      ro[1] = x4;
      goto L598;
    }
  goto ret0;

  L598:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L599;
  goto ret0;

  L599:
  x2 = XEXP (x1, 0);
  if (rtx_equal_p (x2, ro[1]) && 1)
    goto L600;
  goto ret0;

  L600:
  x1 = XVECEXP (x0, 0, 2);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L601;
  goto ret0;

  L601:
  x2 = XEXP (x1, 0);
  if (memory_operand (x2, SImode))
    {
      ro[2] = x2;
      goto L602;
    }
  goto ret0;

  L602:
  x1 = XVECEXP (x0, 0, 3);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L603;
  goto ret0;

  L603:
  x2 = XEXP (x1, 0);
  if (pnum_clobbers != 0 && memory_operand (x2, SImode))
    {
      ro[3] = x2;
      if (TARGET_80387)
	{
	  *pnum_clobbers = 1;
	  return 108;
	}
      }
  goto ret0;

  L607:
  x2 = XEXP (x1, 1);
  if (GET_MODE (x2) == SImode && GET_CODE (x2) == FIX && 1)
    goto L608;
  goto ret0;

  L608:
  x3 = XEXP (x2, 0);
  if (GET_CODE (x3) != FIX)
    goto ret0;
  switch (GET_MODE (x3))
    {
    case XFmode:
      goto L609;
    case DFmode:
      goto L631;
    case SFmode:
      goto L653;
    }
  goto ret0;

  L609:
  x4 = XEXP (x3, 0);
  if (register_operand (x4, XFmode))
    {
      ro[1] = x4;
      goto L610;
    }
  goto ret0;

  L610:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L611;
  goto ret0;

  L611:
  x2 = XEXP (x1, 0);
  if (memory_operand (x2, SImode))
    {
      ro[2] = x2;
      goto L612;
    }
  goto ret0;

  L612:
  x1 = XVECEXP (x0, 0, 2);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L613;
  goto ret0;

  L613:
  x2 = XEXP (x1, 0);
  if (memory_operand (x2, SImode))
    {
      ro[3] = x2;
      goto L614;
    }
  goto ret0;

  L614:
  x1 = XVECEXP (x0, 0, 3);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L615;
  goto ret0;

  L615:
  x2 = XEXP (x1, 0);
  if (scratch_operand (x2, SImode))
    {
      ro[4] = x2;
      if (TARGET_80387)
	return 112;
      }
  goto ret0;

  L631:
  x4 = XEXP (x3, 0);
  if (register_operand (x4, DFmode))
    {
      ro[1] = x4;
      goto L632;
    }
  goto ret0;

  L632:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L633;
  goto ret0;

  L633:
  x2 = XEXP (x1, 0);
  if (memory_operand (x2, SImode))
    {
      ro[2] = x2;
      goto L634;
    }
  goto ret0;

  L634:
  x1 = XVECEXP (x0, 0, 2);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L635;
  goto ret0;

  L635:
  x2 = XEXP (x1, 0);
  if (memory_operand (x2, SImode))
    {
      ro[3] = x2;
      goto L636;
    }
  goto ret0;

  L636:
  x1 = XVECEXP (x0, 0, 3);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L637;
  goto ret0;

  L637:
  x2 = XEXP (x1, 0);
  if (scratch_operand (x2, SImode))
    {
      ro[4] = x2;
      if (TARGET_80387)
	return 113;
      }
  goto ret0;

  L653:
  x4 = XEXP (x3, 0);
  if (register_operand (x4, SFmode))
    {
      ro[1] = x4;
      goto L654;
    }
  goto ret0;

  L654:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L655;
  goto ret0;

  L655:
  x2 = XEXP (x1, 0);
  if (memory_operand (x2, SImode))
    {
      ro[2] = x2;
      goto L656;
    }
  goto ret0;

  L656:
  x1 = XVECEXP (x0, 0, 2);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L657;
  goto ret0;

  L657:
  x2 = XEXP (x1, 0);
  if (memory_operand (x2, SImode))
    {
      ro[3] = x2;
      goto L658;
    }
  goto ret0;

  L658:
  x1 = XVECEXP (x0, 0, 3);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L659;
  goto ret0;

  L659:
  x2 = XEXP (x1, 0);
  if (scratch_operand (x2, SImode))
    {
      ro[4] = x2;
      if (TARGET_80387)
	return 114;
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

  L0:
  switch (GET_CODE (x0))
    {
    case SET:
      goto L295;
    case PARALLEL:
      if (XVECLEN (x0, 0) == 2 && 1)
	goto L10;
      if (XVECLEN (x0, 0) == 3 && 1)
	goto L375;
      if (XVECLEN (x0, 0) == 5 && 1)
	goto L527;
      if (XVECLEN (x0, 0) == 4 && 1)
	goto L541;
      if (XVECLEN (x0, 0) == 6 && 1)
	goto L1561;
      break;
    case CALL:
      goto L1516;
    case UNSPEC_VOLATILE:
      if (XINT (x0, 1) == 0 && XVECLEN (x0, 0) == 1 && 1)
	goto L1557;
      break;
    case RETURN:
      if (simple_386_epilogue ())
	return 301;
      break;
    case CONST_INT:
      if (XWINT (x0, 0) == 0 && 1)
	return 302;
    }
  goto ret0;
 L295:
  return recog_4 (x0, insn, pnum_clobbers);

  L10:
  x1 = XVECEXP (x0, 0, 0);
  switch (GET_CODE (x1))
    {
    case SET:
      goto L343;
    case CALL:
      goto L1507;
    }
  goto ret0;
 L343:
  return recog_6 (x0, insn, pnum_clobbers);

  L1507:
  x2 = XEXP (x1, 0);
  if (GET_MODE (x2) == QImode && GET_CODE (x2) == MEM && 1)
    goto L1508;
  L1498:
  if (call_insn_operand (x2, QImode))
    {
      ro[0] = x2;
      goto L1499;
    }
  goto ret0;

  L1508:
  x3 = XEXP (x2, 0);
  if (symbolic_operand (x3, SImode))
    {
      ro[0] = x3;
      goto L1509;
    }
  goto L1498;

  L1509:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, SImode))
    {
      ro[1] = x2;
      goto L1510;
    }
  x2 = XEXP (x1, 0);
  goto L1498;

  L1510:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == SET && 1)
    goto L1511;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1498;

  L1511:
  x2 = XEXP (x1, 0);
  if (GET_MODE (x2) == SImode && GET_CODE (x2) == REG && XINT (x2, 0) == 7 && 1)
    goto L1512;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1498;

  L1512:
  x2 = XEXP (x1, 1);
  if (GET_MODE (x2) == SImode && GET_CODE (x2) == PLUS && 1)
    goto L1513;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1498;

  L1513:
  x3 = XEXP (x2, 0);
  if (GET_MODE (x3) == SImode && GET_CODE (x3) == REG && XINT (x3, 0) == 7 && 1)
    goto L1514;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1498;

  L1514:
  x3 = XEXP (x2, 1);
  if (immediate_operand (x3, SImode))
    {
      ro[3] = x3;
      if (!HALF_PIC_P ())
	return 289;
      }
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1498;

  L1499:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, SImode))
    {
      ro[1] = x2;
      goto L1500;
    }
  goto ret0;

  L1500:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == SET && 1)
    goto L1501;
  goto ret0;

  L1501:
  x2 = XEXP (x1, 0);
  if (GET_MODE (x2) == SImode && GET_CODE (x2) == REG && XINT (x2, 0) == 7 && 1)
    goto L1502;
  goto ret0;

  L1502:
  x2 = XEXP (x1, 1);
  if (GET_MODE (x2) == SImode && GET_CODE (x2) == PLUS && 1)
    goto L1503;
  goto ret0;

  L1503:
  x3 = XEXP (x2, 0);
  if (GET_MODE (x3) == SImode && GET_CODE (x3) == REG && XINT (x3, 0) == 7 && 1)
    goto L1504;
  goto ret0;

  L1504:
  x3 = XEXP (x2, 1);
  if (immediate_operand (x3, SImode))
    {
      ro[3] = x3;
      return 288;
    }
  goto ret0;

  L375:
  x1 = XVECEXP (x0, 0, 0);
  if (GET_CODE (x1) == SET && 1)
    goto L376;
  goto ret0;
 L376:
  return recog_7 (x0, insn, pnum_clobbers);

  L527:
  x1 = XVECEXP (x0, 0, 0);
  if (GET_CODE (x1) == SET && 1)
    goto L528;
  goto ret0;

  L528:
  x2 = XEXP (x1, 0);
  if (GET_MODE (x2) == DImode && general_operand (x2, DImode))
    {
      ro[0] = x2;
      goto L529;
    }
  goto ret0;

  L529:
  x2 = XEXP (x1, 1);
  if (GET_MODE (x2) == DImode && GET_CODE (x2) == FIX && 1)
    goto L530;
  goto ret0;

  L530:
  x3 = XEXP (x2, 0);
  if (GET_CODE (x3) != FIX)
    goto ret0;
  switch (GET_MODE (x3))
    {
    case XFmode:
      goto L531;
    case DFmode:
      goto L557;
    case SFmode:
      goto L583;
    }
  goto ret0;

  L531:
  x4 = XEXP (x3, 0);
  if (register_operand (x4, XFmode))
    {
      ro[1] = x4;
      goto L532;
    }
  goto ret0;

  L532:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L533;
  goto ret0;

  L533:
  x2 = XEXP (x1, 0);
  if (rtx_equal_p (x2, ro[1]) && 1)
    goto L534;
  goto ret0;

  L534:
  x1 = XVECEXP (x0, 0, 2);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L535;
  goto ret0;

  L535:
  x2 = XEXP (x1, 0);
  if (memory_operand (x2, SImode))
    {
      ro[2] = x2;
      goto L536;
    }
  goto ret0;

  L536:
  x1 = XVECEXP (x0, 0, 3);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L537;
  goto ret0;

  L537:
  x2 = XEXP (x1, 0);
  if (memory_operand (x2, SImode))
    {
      ro[3] = x2;
      goto L538;
    }
  goto ret0;

  L538:
  x1 = XVECEXP (x0, 0, 4);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L539;
  goto ret0;

  L539:
  x2 = XEXP (x1, 0);
  if (scratch_operand (x2, SImode))
    {
      ro[4] = x2;
      if (TARGET_80387)
	return 106;
      }
  goto ret0;

  L557:
  x4 = XEXP (x3, 0);
  if (register_operand (x4, DFmode))
    {
      ro[1] = x4;
      goto L558;
    }
  goto ret0;

  L558:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L559;
  goto ret0;

  L559:
  x2 = XEXP (x1, 0);
  if (rtx_equal_p (x2, ro[1]) && 1)
    goto L560;
  goto ret0;

  L560:
  x1 = XVECEXP (x0, 0, 2);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L561;
  goto ret0;

  L561:
  x2 = XEXP (x1, 0);
  if (memory_operand (x2, SImode))
    {
      ro[2] = x2;
      goto L562;
    }
  goto ret0;

  L562:
  x1 = XVECEXP (x0, 0, 3);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L563;
  goto ret0;

  L563:
  x2 = XEXP (x1, 0);
  if (memory_operand (x2, SImode))
    {
      ro[3] = x2;
      goto L564;
    }
  goto ret0;

  L564:
  x1 = XVECEXP (x0, 0, 4);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L565;
  goto ret0;

  L565:
  x2 = XEXP (x1, 0);
  if (scratch_operand (x2, SImode))
    {
      ro[4] = x2;
      if (TARGET_80387)
	return 107;
      }
  goto ret0;

  L583:
  x4 = XEXP (x3, 0);
  if (register_operand (x4, SFmode))
    {
      ro[1] = x4;
      goto L584;
    }
  goto ret0;

  L584:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L585;
  goto ret0;

  L585:
  x2 = XEXP (x1, 0);
  if (rtx_equal_p (x2, ro[1]) && 1)
    goto L586;
  goto ret0;

  L586:
  x1 = XVECEXP (x0, 0, 2);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L587;
  goto ret0;

  L587:
  x2 = XEXP (x1, 0);
  if (memory_operand (x2, SImode))
    {
      ro[2] = x2;
      goto L588;
    }
  goto ret0;

  L588:
  x1 = XVECEXP (x0, 0, 3);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L589;
  goto ret0;

  L589:
  x2 = XEXP (x1, 0);
  if (memory_operand (x2, SImode))
    {
      ro[3] = x2;
      goto L590;
    }
  goto ret0;

  L590:
  x1 = XVECEXP (x0, 0, 4);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L591;
  goto ret0;

  L591:
  x2 = XEXP (x1, 0);
  if (scratch_operand (x2, SImode))
    {
      ro[4] = x2;
      if (TARGET_80387)
	return 108;
      }
  goto ret0;

  L541:
  x1 = XVECEXP (x0, 0, 0);
  if (GET_CODE (x1) == SET && 1)
    goto L542;
  goto ret0;
 L542:
  return recog_8 (x0, insn, pnum_clobbers);

  L1561:
  x1 = XVECEXP (x0, 0, 0);
  if (GET_CODE (x1) == SET && 1)
    goto L1562;
  goto ret0;

  L1562:
  x2 = XEXP (x1, 0);
  switch (GET_MODE (x2))
    {
    case BLKmode:
      if (GET_CODE (x2) == MEM && 1)
	goto L1563;
      break;
    case SImode:
      if (general_operand (x2, SImode))
	{
	  ro[0] = x2;
	  goto L1579;
	}
    }
  if (GET_CODE (x2) == CC0 && 1)
    goto L1597;
  goto ret0;

  L1563:
  x3 = XEXP (x2, 0);
  if (address_operand (x3, SImode))
    {
      ro[0] = x3;
      goto L1564;
    }
  goto ret0;

  L1564:
  x2 = XEXP (x1, 1);
  if (GET_MODE (x2) == BLKmode && GET_CODE (x2) == MEM && 1)
    goto L1565;
  goto ret0;

  L1565:
  x3 = XEXP (x2, 0);
  if (address_operand (x3, SImode))
    {
      ro[1] = x3;
      goto L1566;
    }
  goto ret0;

  L1566:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == USE && 1)
    goto L1567;
  goto ret0;

  L1567:
  x2 = XEXP (x1, 0);
  if (GET_CODE (x2) == CONST_INT && 1)
    {
      ro[2] = x2;
      goto L1568;
    }
  goto ret0;

  L1568:
  x1 = XVECEXP (x0, 0, 2);
  if (GET_CODE (x1) == USE && 1)
    goto L1569;
  goto ret0;

  L1569:
  x2 = XEXP (x1, 0);
  if (immediate_operand (x2, SImode))
    {
      ro[3] = x2;
      goto L1570;
    }
  goto ret0;

  L1570:
  x1 = XVECEXP (x0, 0, 3);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L1571;
  goto ret0;

  L1571:
  x2 = XEXP (x1, 0);
  if (scratch_operand (x2, SImode))
    {
      ro[4] = x2;
      goto L1572;
    }
  goto ret0;

  L1572:
  x1 = XVECEXP (x0, 0, 4);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L1573;
  goto ret0;

  L1573:
  x2 = XEXP (x1, 0);
  if (rtx_equal_p (x2, ro[0]) && 1)
    goto L1574;
  goto ret0;

  L1574:
  x1 = XVECEXP (x0, 0, 5);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L1575;
  goto ret0;

  L1575:
  x2 = XEXP (x1, 0);
  if (rtx_equal_p (x2, ro[1]) && 1)
    return 304;
  goto ret0;

  L1579:
  x2 = XEXP (x1, 1);
  if (GET_MODE (x2) == SImode && GET_CODE (x2) == COMPARE && 1)
    goto L1580;
  goto ret0;

  L1580:
  x3 = XEXP (x2, 0);
  if (GET_MODE (x3) == BLKmode && GET_CODE (x3) == MEM && 1)
    goto L1581;
  goto ret0;

  L1581:
  x4 = XEXP (x3, 0);
  if (address_operand (x4, SImode))
    {
      ro[1] = x4;
      goto L1582;
    }
  goto ret0;

  L1582:
  x3 = XEXP (x2, 1);
  if (GET_MODE (x3) == BLKmode && GET_CODE (x3) == MEM && 1)
    goto L1583;
  goto ret0;

  L1583:
  x4 = XEXP (x3, 0);
  if (address_operand (x4, SImode))
    {
      ro[2] = x4;
      goto L1584;
    }
  goto ret0;

  L1584:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == USE && 1)
    goto L1585;
  goto ret0;

  L1585:
  x2 = XEXP (x1, 0);
  if (register_operand (x2, SImode))
    {
      ro[3] = x2;
      goto L1586;
    }
  goto ret0;

  L1586:
  x1 = XVECEXP (x0, 0, 2);
  if (GET_CODE (x1) == USE && 1)
    goto L1587;
  goto ret0;

  L1587:
  x2 = XEXP (x1, 0);
  if (immediate_operand (x2, SImode))
    {
      ro[4] = x2;
      goto L1588;
    }
  goto ret0;

  L1588:
  x1 = XVECEXP (x0, 0, 3);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L1589;
  goto ret0;

  L1589:
  x2 = XEXP (x1, 0);
  if (rtx_equal_p (x2, ro[1]) && 1)
    goto L1590;
  goto ret0;

  L1590:
  x1 = XVECEXP (x0, 0, 4);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L1591;
  goto ret0;

  L1591:
  x2 = XEXP (x1, 0);
  if (rtx_equal_p (x2, ro[2]) && 1)
    goto L1592;
  goto ret0;

  L1592:
  x1 = XVECEXP (x0, 0, 5);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L1593;
  goto ret0;

  L1593:
  x2 = XEXP (x1, 0);
  if (rtx_equal_p (x2, ro[3]) && 1)
    return 306;
  goto ret0;

  L1597:
  x2 = XEXP (x1, 1);
  if (GET_MODE (x2) == SImode && GET_CODE (x2) == COMPARE && 1)
    goto L1598;
  goto ret0;

  L1598:
  x3 = XEXP (x2, 0);
  if (GET_MODE (x3) == BLKmode && GET_CODE (x3) == MEM && 1)
    goto L1599;
  goto ret0;

  L1599:
  x4 = XEXP (x3, 0);
  if (address_operand (x4, SImode))
    {
      ro[0] = x4;
      goto L1600;
    }
  goto ret0;

  L1600:
  x3 = XEXP (x2, 1);
  if (GET_MODE (x3) == BLKmode && GET_CODE (x3) == MEM && 1)
    goto L1601;
  goto ret0;

  L1601:
  x4 = XEXP (x3, 0);
  if (address_operand (x4, SImode))
    {
      ro[1] = x4;
      goto L1602;
    }
  goto ret0;

  L1602:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == USE && 1)
    goto L1603;
  goto ret0;

  L1603:
  x2 = XEXP (x1, 0);
  if (register_operand (x2, SImode))
    {
      ro[2] = x2;
      goto L1604;
    }
  goto ret0;

  L1604:
  x1 = XVECEXP (x0, 0, 2);
  if (GET_CODE (x1) == USE && 1)
    goto L1605;
  goto ret0;

  L1605:
  x2 = XEXP (x1, 0);
  if (immediate_operand (x2, SImode))
    {
      ro[3] = x2;
      goto L1606;
    }
  goto ret0;

  L1606:
  x1 = XVECEXP (x0, 0, 3);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L1607;
  goto ret0;

  L1607:
  x2 = XEXP (x1, 0);
  if (rtx_equal_p (x2, ro[0]) && 1)
    goto L1608;
  goto ret0;

  L1608:
  x1 = XVECEXP (x0, 0, 4);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L1609;
  goto ret0;

  L1609:
  x2 = XEXP (x1, 0);
  if (rtx_equal_p (x2, ro[1]) && 1)
    goto L1610;
  goto ret0;

  L1610:
  x1 = XVECEXP (x0, 0, 5);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L1611;
  goto ret0;

  L1611:
  x2 = XEXP (x1, 0);
  if (rtx_equal_p (x2, ro[2]) && 1)
    return 307;
  goto ret0;

  L1516:
  x1 = XEXP (x0, 0);
  if (call_insn_operand (x1, QImode))
    {
      ro[0] = x1;
      goto L1517;
    }
  L1519:
  if (GET_MODE (x1) == QImode && GET_CODE (x1) == MEM && 1)
    goto L1520;
  goto ret0;

  L1517:
  x1 = XEXP (x0, 1);
  if (general_operand (x1, SImode))
    {
      ro[1] = x1;
      return 291;
    }
  x1 = XEXP (x0, 0);
  goto L1519;

  L1520:
  x2 = XEXP (x1, 0);
  if (symbolic_operand (x2, SImode))
    {
      ro[0] = x2;
      goto L1521;
    }
  goto ret0;

  L1521:
  x1 = XEXP (x0, 1);
  if (general_operand (x1, SImode))
    {
      ro[1] = x1;
      if (!HALF_PIC_P ())
	return 292;
      }
  goto ret0;

  L1557:
  x1 = XVECEXP (x0, 0, 0);
  if (GET_CODE (x1) == CONST_INT && XWINT (x1, 0) == 0 && 1)
    return 300;
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

