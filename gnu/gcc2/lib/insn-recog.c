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
    }
  switch (GET_CODE (x1))
    {
    case COMPARE:
      goto L30;
    case ZERO_EXTRACT:
      goto L813;
    }
  L52:
  if (VOIDmode_compare_op (x1, VOIDmode))
    {
      ro[2] = x1;
      goto L82;
    }
  L125:
  switch (GET_MODE (x1))
    {
    case CCFPEQmode:
      switch (GET_CODE (x1))
	{
	case COMPARE:
	  goto L126;
	}
      break;
    case SImode:
      switch (GET_CODE (x1))
	{
	case AND:
	  goto L187;
	}
      break;
    case HImode:
      switch (GET_CODE (x1))
	{
	case AND:
	  goto L192;
	}
      break;
    case QImode:
      if (GET_CODE (x1) == AND && 1)
	goto L197;
    }
  goto ret0;

  L30:
  x2 = XEXP (x1, 0);
  switch (GET_MODE (x2))
    {
    case SImode:
      if (nonimmediate_operand (x2, SImode))
	{
	  ro[0] = x2;
	  goto L31;
	}
      break;
    case HImode:
      if (nonimmediate_operand (x2, HImode))
	{
	  ro[0] = x2;
	  goto L36;
	}
      break;
    case QImode:
      if (nonimmediate_operand (x2, QImode))
	{
	  ro[0] = x2;
	  goto L41;
	}
    }
  goto L52;

  L31:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, SImode))
    {
      ro[1] = x2;
      if (GET_CODE (operands[0]) != MEM || GET_CODE (operands[1]) != MEM)
	return 10;
      }
  goto L52;

  L36:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, HImode))
    {
      ro[1] = x2;
      if (GET_CODE (operands[0]) != MEM || GET_CODE (operands[1]) != MEM)
	return 12;
      }
  goto L52;

  L41:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, QImode))
    {
      ro[1] = x2;
      if (GET_CODE (operands[0]) != MEM || GET_CODE (operands[1]) != MEM)
	return 14;
      }
  goto L52;

  L813:
  x2 = XEXP (x1, 0);
  switch (GET_MODE (x2))
    {
    case SImode:
      if (register_operand (x2, SImode))
	{
	  ro[0] = x2;
	  goto L814;
	}
      break;
    case QImode:
      if (general_operand (x2, QImode))
	{
	  ro[0] = x2;
	  goto L826;
	}
    }
  goto L52;

  L814:
  x2 = XEXP (x1, 1);
  if (GET_CODE (x2) != CONST_INT)
    {
    goto L52;
    }
  if (XWINT (x2, 0) == 1 && 1)
    goto L815;
  L820:
  ro[1] = x2;
  goto L821;

  L815:
  x2 = XEXP (x1, 2);
  if (general_operand (x2, SImode))
    {
      ro[1] = x2;
      if (GET_CODE (operands[1]) != CONST_INT)
	return 167;
      }
  x2 = XEXP (x1, 1);
  goto L820;

  L821:
  x2 = XEXP (x1, 2);
  if (GET_CODE (x2) == CONST_INT && 1)
    {
      ro[2] = x2;
      return 168;
    }
  goto L52;

  L826:
  x2 = XEXP (x1, 1);
  if (GET_CODE (x2) == CONST_INT && 1)
    {
      ro[1] = x2;
      goto L827;
    }
  goto L52;

  L827:
  x2 = XEXP (x1, 2);
  if (GET_CODE (x2) == CONST_INT && 1)
    {
      ro[2] = x2;
      if (GET_CODE (operands[0]) != MEM || ! MEM_VOLATILE_P (operands[0]))
	return 169;
      }
  goto L52;

  L82:
  x2 = XEXP (x1, 0);
  switch (GET_MODE (x2))
    {
    case DFmode:
      switch (GET_CODE (x2))
	{
	case FLOAT:
	  goto L83;
	case FLOAT_EXTEND:
	  goto L113;
	case SUBREG:
	case REG:
	case MEM:
	  if (nonimmediate_operand (x2, DFmode))
	    {
	      ro[0] = x2;
	      goto L54;
	    }
	}
    L67:
      if (register_operand (x2, DFmode))
	{
	  ro[0] = x2;
	  goto L68;
	}
      break;
    case SFmode:
      if (GET_CODE (x2) == FLOAT && 1)
	goto L169;
      if (nonimmediate_operand (x2, SFmode))
	{
	  ro[0] = x2;
	  goto L140;
	}
    L153:
      if (register_operand (x2, SFmode))
	{
	  ro[0] = x2;
	  goto L154;
	}
    }
  goto L125;

  L83:
  x3 = XEXP (x2, 0);
  if (nonimmediate_operand (x3, SImode))
    {
      ro[0] = x3;
      goto L84;
    }
  goto L125;

  L84:
  x2 = XEXP (x1, 1);
  if (pnum_clobbers != 0 && register_operand (x2, DFmode))
    {
      ro[1] = x2;
      if (TARGET_80387)
	{
	  *pnum_clobbers = 1;
	  return 18;
	}
      }
  goto L125;

  L113:
  x3 = XEXP (x2, 0);
  if (nonimmediate_operand (x3, SFmode))
    {
      ro[0] = x3;
      goto L114;
    }
  goto L125;

  L114:
  x2 = XEXP (x1, 1);
  if (pnum_clobbers != 0 && register_operand (x2, DFmode))
    {
      ro[1] = x2;
      if (TARGET_80387)
	{
	  *pnum_clobbers = 1;
	  return 20;
	}
      }
  goto L125;

  L54:
  x2 = XEXP (x1, 1);
  if (pnum_clobbers != 0 && nonimmediate_operand (x2, DFmode))
    {
      ro[1] = x2;
      if (TARGET_80387
   && (GET_CODE (operands[0]) != MEM || GET_CODE (operands[1]) != MEM))
	{
	  *pnum_clobbers = 1;
	  return 16;
	}
      }
  x2 = XEXP (x1, 0);
  goto L67;

  L68:
  x2 = XEXP (x1, 1);
  if (GET_MODE (x2) != DFmode)
    {
      goto L125;
    }
  switch (GET_CODE (x2))
    {
    case FLOAT:
      goto L69;
    case FLOAT_EXTEND:
      goto L99;
    }
  goto L125;

  L69:
  x3 = XEXP (x2, 0);
  if (pnum_clobbers != 0 && nonimmediate_operand (x3, SImode))
    {
      ro[1] = x3;
      if (TARGET_80387)
	{
	  *pnum_clobbers = 1;
	  return 17;
	}
      }
  goto L125;

  L99:
  x3 = XEXP (x2, 0);
  if (pnum_clobbers != 0 && nonimmediate_operand (x3, SFmode))
    {
      ro[1] = x3;
      if (TARGET_80387)
	{
	  *pnum_clobbers = 1;
	  return 19;
	}
      }
  goto L125;

  L169:
  x3 = XEXP (x2, 0);
  if (nonimmediate_operand (x3, SImode))
    {
      ro[0] = x3;
      goto L170;
    }
  goto L125;

  L170:
  x2 = XEXP (x1, 1);
  if (pnum_clobbers != 0 && register_operand (x2, SFmode))
    {
      ro[1] = x2;
      if (TARGET_80387)
	{
	  *pnum_clobbers = 1;
	  return 24;
	}
      }
  goto L125;

  L140:
  x2 = XEXP (x1, 1);
  if (pnum_clobbers != 0 && nonimmediate_operand (x2, SFmode))
    {
      ro[1] = x2;
      if (TARGET_80387
   && (GET_CODE (operands[0]) != MEM || GET_CODE (operands[1]) != MEM))
	{
	  *pnum_clobbers = 1;
	  return 22;
	}
      }
  x2 = XEXP (x1, 0);
  goto L153;

  L154:
  x2 = XEXP (x1, 1);
  if (GET_MODE (x2) == SFmode && GET_CODE (x2) == FLOAT && 1)
    goto L155;
  goto L125;

  L155:
  x3 = XEXP (x2, 0);
  if (pnum_clobbers != 0 && nonimmediate_operand (x3, SImode))
    {
      ro[1] = x3;
      if (TARGET_80387)
	{
	  *pnum_clobbers = 1;
	  return 23;
	}
      }
  goto L125;

  L126:
  x2 = XEXP (x1, 0);
  switch (GET_MODE (x2))
    {
    case DFmode:
      if (register_operand (x2, DFmode))
	{
	  ro[0] = x2;
	  goto L127;
	}
      break;
    case SFmode:
      if (register_operand (x2, SFmode))
	{
	  ro[0] = x2;
	  goto L183;
	}
    }
  goto ret0;

  L127:
  x2 = XEXP (x1, 1);
  if (pnum_clobbers != 0 && register_operand (x2, DFmode))
    {
      ro[1] = x2;
      if (TARGET_80387)
	{
	  *pnum_clobbers = 1;
	  return 21;
	}
      }
  goto ret0;

  L183:
  x2 = XEXP (x1, 1);
  if (pnum_clobbers != 0 && register_operand (x2, SFmode))
    {
      ro[1] = x2;
      if (TARGET_80387)
	{
	  *pnum_clobbers = 1;
	  return 25;
	}
      }
  goto ret0;

  L187:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, SImode))
    {
      ro[0] = x2;
      goto L188;
    }
  goto ret0;

  L188:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, SImode))
    {
      ro[1] = x2;
      return 32;
    }
  goto ret0;

  L192:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, HImode))
    {
      ro[0] = x2;
      goto L193;
    }
  goto ret0;

  L193:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, HImode))
    {
      ro[1] = x2;
      return 33;
    }
  goto ret0;

  L197:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, QImode))
    {
      ro[0] = x2;
      goto L198;
    }
  goto ret0;

  L198:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, QImode))
    {
      ro[1] = x2;
      return 34;
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
  x2 = XEXP (x1, 0);
  switch (GET_CODE (x2))
    {
    case EQ:
      goto L882;
    case NE:
      goto L891;
    case GT:
      goto L900;
    case GTU:
      goto L909;
    case LT:
      goto L918;
    case LTU:
      goto L927;
    case GE:
      goto L936;
    case GEU:
      goto L945;
    case LE:
      goto L954;
    case LEU:
      goto L963;
    }
  goto ret0;

  L882:
  x3 = XEXP (x2, 0);
  if (GET_CODE (x3) == CC0 && 1)
    goto L883;
  goto ret0;

  L883:
  x3 = XEXP (x2, 1);
  if (GET_CODE (x3) == CONST_INT && XWINT (x3, 0) == 0 && 1)
    goto L884;
  goto ret0;

  L884:
  x2 = XEXP (x1, 1);
  switch (GET_CODE (x2))
    {
    case LABEL_REF:
      goto L885;
    case PC:
      goto L975;
    }
  goto ret0;

  L885:
  x3 = XEXP (x2, 0);
  ro[0] = x3;
  goto L886;

  L886:
  x2 = XEXP (x1, 2);
  if (GET_CODE (x2) == PC && 1)
    return 191;
  goto ret0;

  L975:
  x2 = XEXP (x1, 2);
  if (GET_CODE (x2) == LABEL_REF && 1)
    goto L976;
  goto ret0;

  L976:
  x3 = XEXP (x2, 0);
  ro[0] = x3;
  return 210;

  L891:
  x3 = XEXP (x2, 0);
  if (GET_CODE (x3) == CC0 && 1)
    goto L892;
  goto ret0;

  L892:
  x3 = XEXP (x2, 1);
  if (GET_CODE (x3) == CONST_INT && XWINT (x3, 0) == 0 && 1)
    goto L893;
  goto ret0;

  L893:
  x2 = XEXP (x1, 1);
  switch (GET_CODE (x2))
    {
    case LABEL_REF:
      goto L894;
    case PC:
      goto L984;
    }
  goto ret0;

  L894:
  x3 = XEXP (x2, 0);
  ro[0] = x3;
  goto L895;

  L895:
  x2 = XEXP (x1, 2);
  if (GET_CODE (x2) == PC && 1)
    return 193;
  goto ret0;

  L984:
  x2 = XEXP (x1, 2);
  if (GET_CODE (x2) == LABEL_REF && 1)
    goto L985;
  goto ret0;

  L985:
  x3 = XEXP (x2, 0);
  ro[0] = x3;
  return 211;

  L900:
  x3 = XEXP (x2, 0);
  if (GET_CODE (x3) == CC0 && 1)
    goto L901;
  goto ret0;

  L901:
  x3 = XEXP (x2, 1);
  if (GET_CODE (x3) == CONST_INT && XWINT (x3, 0) == 0 && 1)
    goto L902;
  goto ret0;

  L902:
  x2 = XEXP (x1, 1);
  switch (GET_CODE (x2))
    {
    case LABEL_REF:
      goto L903;
    case PC:
      goto L993;
    }
  goto ret0;

  L903:
  x3 = XEXP (x2, 0);
  ro[0] = x3;
  goto L904;

  L904:
  x2 = XEXP (x1, 2);
  if (GET_CODE (x2) == PC && 1)
    return 195;
  goto ret0;

  L993:
  x2 = XEXP (x1, 2);
  if (GET_CODE (x2) == LABEL_REF && 1)
    goto L994;
  goto ret0;

  L994:
  x3 = XEXP (x2, 0);
  ro[0] = x3;
  return 212;

  L909:
  x3 = XEXP (x2, 0);
  if (GET_CODE (x3) == CC0 && 1)
    goto L910;
  goto ret0;

  L910:
  x3 = XEXP (x2, 1);
  if (GET_CODE (x3) == CONST_INT && XWINT (x3, 0) == 0 && 1)
    goto L911;
  goto ret0;

  L911:
  x2 = XEXP (x1, 1);
  switch (GET_CODE (x2))
    {
    case LABEL_REF:
      goto L912;
    case PC:
      goto L1002;
    }
  goto ret0;

  L912:
  x3 = XEXP (x2, 0);
  ro[0] = x3;
  goto L913;

  L913:
  x2 = XEXP (x1, 2);
  if (GET_CODE (x2) == PC && 1)
    return 197;
  goto ret0;

  L1002:
  x2 = XEXP (x1, 2);
  if (GET_CODE (x2) == LABEL_REF && 1)
    goto L1003;
  goto ret0;

  L1003:
  x3 = XEXP (x2, 0);
  ro[0] = x3;
  return 213;

  L918:
  x3 = XEXP (x2, 0);
  if (GET_CODE (x3) == CC0 && 1)
    goto L919;
  goto ret0;

  L919:
  x3 = XEXP (x2, 1);
  if (GET_CODE (x3) == CONST_INT && XWINT (x3, 0) == 0 && 1)
    goto L920;
  goto ret0;

  L920:
  x2 = XEXP (x1, 1);
  switch (GET_CODE (x2))
    {
    case LABEL_REF:
      goto L921;
    case PC:
      goto L1011;
    }
  goto ret0;

  L921:
  x3 = XEXP (x2, 0);
  ro[0] = x3;
  goto L922;

  L922:
  x2 = XEXP (x1, 2);
  if (GET_CODE (x2) == PC && 1)
    return 199;
  goto ret0;

  L1011:
  x2 = XEXP (x1, 2);
  if (GET_CODE (x2) == LABEL_REF && 1)
    goto L1012;
  goto ret0;

  L1012:
  x3 = XEXP (x2, 0);
  ro[0] = x3;
  return 214;

  L927:
  x3 = XEXP (x2, 0);
  if (GET_CODE (x3) == CC0 && 1)
    goto L928;
  goto ret0;

  L928:
  x3 = XEXP (x2, 1);
  if (GET_CODE (x3) == CONST_INT && XWINT (x3, 0) == 0 && 1)
    goto L929;
  goto ret0;

  L929:
  x2 = XEXP (x1, 1);
  switch (GET_CODE (x2))
    {
    case LABEL_REF:
      goto L930;
    case PC:
      goto L1020;
    }
  goto ret0;

  L930:
  x3 = XEXP (x2, 0);
  ro[0] = x3;
  goto L931;

  L931:
  x2 = XEXP (x1, 2);
  if (GET_CODE (x2) == PC && 1)
    return 201;
  goto ret0;

  L1020:
  x2 = XEXP (x1, 2);
  if (GET_CODE (x2) == LABEL_REF && 1)
    goto L1021;
  goto ret0;

  L1021:
  x3 = XEXP (x2, 0);
  ro[0] = x3;
  return 215;

  L936:
  x3 = XEXP (x2, 0);
  if (GET_CODE (x3) == CC0 && 1)
    goto L937;
  goto ret0;

  L937:
  x3 = XEXP (x2, 1);
  if (GET_CODE (x3) == CONST_INT && XWINT (x3, 0) == 0 && 1)
    goto L938;
  goto ret0;

  L938:
  x2 = XEXP (x1, 1);
  switch (GET_CODE (x2))
    {
    case LABEL_REF:
      goto L939;
    case PC:
      goto L1029;
    }
  goto ret0;

  L939:
  x3 = XEXP (x2, 0);
  ro[0] = x3;
  goto L940;

  L940:
  x2 = XEXP (x1, 2);
  if (GET_CODE (x2) == PC && 1)
    return 203;
  goto ret0;

  L1029:
  x2 = XEXP (x1, 2);
  if (GET_CODE (x2) == LABEL_REF && 1)
    goto L1030;
  goto ret0;

  L1030:
  x3 = XEXP (x2, 0);
  ro[0] = x3;
  return 216;

  L945:
  x3 = XEXP (x2, 0);
  if (GET_CODE (x3) == CC0 && 1)
    goto L946;
  goto ret0;

  L946:
  x3 = XEXP (x2, 1);
  if (GET_CODE (x3) == CONST_INT && XWINT (x3, 0) == 0 && 1)
    goto L947;
  goto ret0;

  L947:
  x2 = XEXP (x1, 1);
  switch (GET_CODE (x2))
    {
    case LABEL_REF:
      goto L948;
    case PC:
      goto L1038;
    }
  goto ret0;

  L948:
  x3 = XEXP (x2, 0);
  ro[0] = x3;
  goto L949;

  L949:
  x2 = XEXP (x1, 2);
  if (GET_CODE (x2) == PC && 1)
    return 205;
  goto ret0;

  L1038:
  x2 = XEXP (x1, 2);
  if (GET_CODE (x2) == LABEL_REF && 1)
    goto L1039;
  goto ret0;

  L1039:
  x3 = XEXP (x2, 0);
  ro[0] = x3;
  return 217;

  L954:
  x3 = XEXP (x2, 0);
  if (GET_CODE (x3) == CC0 && 1)
    goto L955;
  goto ret0;

  L955:
  x3 = XEXP (x2, 1);
  if (GET_CODE (x3) == CONST_INT && XWINT (x3, 0) == 0 && 1)
    goto L956;
  goto ret0;

  L956:
  x2 = XEXP (x1, 1);
  switch (GET_CODE (x2))
    {
    case LABEL_REF:
      goto L957;
    case PC:
      goto L1047;
    }
  goto ret0;

  L957:
  x3 = XEXP (x2, 0);
  ro[0] = x3;
  goto L958;

  L958:
  x2 = XEXP (x1, 2);
  if (GET_CODE (x2) == PC && 1)
    return 207;
  goto ret0;

  L1047:
  x2 = XEXP (x1, 2);
  if (GET_CODE (x2) == LABEL_REF && 1)
    goto L1048;
  goto ret0;

  L1048:
  x3 = XEXP (x2, 0);
  ro[0] = x3;
  return 218;

  L963:
  x3 = XEXP (x2, 0);
  if (GET_CODE (x3) == CC0 && 1)
    goto L964;
  goto ret0;

  L964:
  x3 = XEXP (x2, 1);
  if (GET_CODE (x3) == CONST_INT && XWINT (x3, 0) == 0 && 1)
    goto L965;
  goto ret0;

  L965:
  x2 = XEXP (x1, 1);
  switch (GET_CODE (x2))
    {
    case LABEL_REF:
      goto L966;
    case PC:
      goto L1056;
    }
  goto ret0;

  L966:
  x3 = XEXP (x2, 0);
  ro[0] = x3;
  goto L967;

  L967:
  x2 = XEXP (x1, 2);
  if (GET_CODE (x2) == PC && 1)
    return 209;
  goto ret0;

  L1056:
  x2 = XEXP (x1, 2);
  if (GET_CODE (x2) == LABEL_REF && 1)
    goto L1057;
  goto ret0;

  L1057:
  x3 = XEXP (x2, 0);
  ro[0] = x3;
  return 219;
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
	      goto L201;
	    }
	  break;
	case ZERO_EXTRACT:
	  goto L792;
	}
    L208:
      if (general_operand (x1, SImode))
	{
	  ro[0] = x1;
	  goto L257;
	}
    L431:
      if (register_operand (x1, SImode))
	{
	  ro[0] = x1;
	  goto L432;
	}
    L439:
      if (general_operand (x1, SImode))
	{
	  ro[0] = x1;
	  goto L440;
	}
      break;
    case HImode:
      if (GET_CODE (x1) == MEM && push_operand (x1, HImode))
	{
	  ro[0] = x1;
	  goto L212;
	}
    L214:
      if (general_operand (x1, HImode))
	{
	  ro[0] = x1;
	  goto L261;
	}
      break;
    case QImode:
      if (GET_CODE (x1) == MEM && push_operand (x1, QImode))
	{
	  ro[0] = x1;
	  goto L222;
	}
    L224:
      if (general_operand (x1, QImode))
	{
	  ro[0] = x1;
	  goto L427;
	}
    L829:
      if (register_operand (x1, QImode))
	{
	  ro[0] = x1;
	  goto L830;
	}
      break;
    case SFmode:
      if (GET_CODE (x1) == MEM && push_operand (x1, SFmode))
	{
	  ro[0] = x1;
	  goto L232;
	}
    L234:
      if (general_operand (x1, SFmode))
	{
	  ro[0] = x1;
	  goto L235;
	}
    L399:
      if (register_operand (x1, SFmode))
	{
	  ro[0] = x1;
	  goto L400;
	}
      break;
    case DFmode:
      if (GET_CODE (x1) == MEM && push_operand (x1, DFmode))
	{
	  ro[0] = x1;
	  goto L238;
	}
    L247:
      if (general_operand (x1, DFmode))
	{
	  ro[0] = x1;
	  goto L289;
	}
    L395:
      if (register_operand (x1, DFmode))
	{
	  ro[0] = x1;
	  goto L396;
	}
      break;
    case DImode:
      if (GET_CODE (x1) == MEM && push_operand (x1, DImode))
	{
	  ro[0] = x1;
	  goto L251;
	}
    L253:
      if (general_operand (x1, DImode))
	{
	  ro[0] = x1;
	  goto L412;
	}
    L268:
      if (register_operand (x1, DImode))
	{
	  ro[0] = x1;
	  goto L269;
	}
    }
  switch (GET_CODE (x1))
    {
    case CC0:
      goto L2;
    case STRICT_LOW_PART:
      goto L218;
    case PC:
      goto L1081;
    }
  L1147:
  ro[0] = x1;
  goto L1148;
  L1236:
  switch (GET_MODE (x1))
    {
    case SImode:
      if (register_operand (x1, SImode))
	{
	  ro[0] = x1;
	  goto L1237;
	}
      break;
    case HImode:
      if (register_operand (x1, HImode))
	{
	  ro[0] = x1;
	  goto L1252;
	}
      break;
    case DFmode:
      if (register_operand (x1, DFmode))
	{
	  ro[0] = x1;
	  goto L1258;
	}
      break;
    case SFmode:
      if (register_operand (x1, SFmode))
	{
	  ro[0] = x1;
	  goto L1287;
	}
    }
  goto ret0;

  L201:
  x1 = XEXP (x0, 1);
  if (general_operand (x1, SImode))
    goto L205;
  x1 = XEXP (x0, 0);
  goto L208;

  L205:
  ro[1] = x1;
  if (! TARGET_486)
    return 35;
  L206:
  ro[1] = x1;
  if (TARGET_486)
    return 36;
  x1 = XEXP (x0, 0);
  goto L208;

  L792:
  x2 = XEXP (x1, 0);
  if (GET_MODE (x2) == SImode && general_operand (x2, SImode))
    {
      ro[0] = x2;
      goto L793;
    }
  goto L1147;

  L793:
  x2 = XEXP (x1, 1);
  if (GET_CODE (x2) == CONST_INT && XWINT (x2, 0) == 1 && 1)
    goto L794;
  goto L1147;

  L794:
  x2 = XEXP (x1, 2);
  if (general_operand (x2, SImode))
    {
      ro[2] = x2;
      goto L795;
    }
  goto L1147;

  L795:
  x1 = XEXP (x0, 1);
  if (GET_CODE (x1) == CONST_INT && 1)
    {
      ro[3] = x1;
      if (! TARGET_486 && GET_CODE (operands[2]) != CONST_INT)
	return 164;
      }
  x1 = XEXP (x0, 0);
  goto L1147;

  L257:
  x1 = XEXP (x0, 1);
  switch (GET_MODE (x1))
    {
    case SImode:
      switch (GET_CODE (x1))
	{
	case ZERO_EXTEND:
	  goto L258;
	case SIGN_EXTEND:
	  goto L278;
	case PLUS:
	  goto L418;
	}
    }
  if (general_operand (x1, SImode))
    {
      ro[1] = x1;
      return 38;
    }
  x1 = XEXP (x0, 0);
  goto L431;

  L258:
  x2 = XEXP (x1, 0);
  switch (GET_MODE (x2))
    {
    case HImode:
      if (nonimmediate_operand (x2, HImode))
	{
	  ro[1] = x2;
	  return 52;
	}
      break;
    case QImode:
      if (nonimmediate_operand (x2, QImode))
	{
	  ro[1] = x2;
	  return 54;
	}
    }
  x1 = XEXP (x0, 0);
  goto L431;

  L278:
  x2 = XEXP (x1, 0);
  switch (GET_MODE (x2))
    {
    case HImode:
      if (nonimmediate_operand (x2, HImode))
	{
	  ro[1] = x2;
	  return 57;
	}
      break;
    case QImode:
      if (nonimmediate_operand (x2, QImode))
	{
	  ro[1] = x2;
	  return 59;
	}
    }
  x1 = XEXP (x0, 0);
  goto L431;

  L418:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, SImode))
    {
      ro[1] = x2;
      goto L419;
    }
  x1 = XEXP (x0, 0);
  goto L431;

  L419:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, SImode))
    {
      ro[2] = x2;
      return 82;
    }
  x1 = XEXP (x0, 0);
  goto L431;

  L432:
  x1 = XEXP (x0, 1);
  if (address_operand (x1, QImode))
    {
      ro[1] = x1;
      return 85;
    }
  x1 = XEXP (x0, 0);
  goto L439;

  L440:
  x1 = XEXP (x0, 1);
  if (GET_MODE (x1) != SImode)
    {
      x1 = XEXP (x0, 0);
      goto L1147;
    }
  switch (GET_CODE (x1))
    {
    case MINUS:
      goto L441;
    case MULT:
      goto L468;
    case AND:
      goto L541;
    case IOR:
      goto L556;
    case XOR:
      goto L799;
    case NEG:
      goto L590;
    case NOT:
      goto L667;
    case ASHIFT:
      goto L692;
    case ASHIFTRT:
      goto L720;
    case LSHIFTRT:
      goto L748;
    case ROTATE:
      goto L763;
    case ROTATERT:
      goto L778;
    }
  x1 = XEXP (x0, 0);
  goto L1147;

  L441:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, SImode))
    {
      ro[1] = x2;
      goto L442;
    }
  x1 = XEXP (x0, 0);
  goto L1147;

  L442:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, SImode))
    {
      ro[2] = x2;
      return 89;
    }
  x1 = XEXP (x0, 0);
  goto L1147;

  L468:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, SImode))
    {
      ro[1] = x2;
      goto L469;
    }
  x1 = XEXP (x0, 0);
  goto L1147;

  L469:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, SImode))
    goto L475;
  x1 = XEXP (x0, 0);
  goto L1147;

  L475:
  ro[2] = x2;
  if (GET_CODE (operands[2]) == CONST_INT && INTVAL (operands[2]) == 0x80)
    return 96;
  L476:
  ro[2] = x2;
  return 97;

  L541:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, SImode))
    {
      ro[1] = x2;
      goto L542;
    }
  x1 = XEXP (x0, 0);
  goto L1147;

  L542:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, SImode))
    {
      ro[2] = x2;
      return 109;
    }
  x1 = XEXP (x0, 0);
  goto L1147;

  L556:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, SImode))
    {
      ro[1] = x2;
      goto L557;
    }
  x1 = XEXP (x0, 0);
  goto L1147;

  L557:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, SImode))
    {
      ro[2] = x2;
      return 112;
    }
  x1 = XEXP (x0, 0);
  goto L1147;

  L799:
  x2 = XEXP (x1, 0);
  if (GET_MODE (x2) == SImode && GET_CODE (x2) == ASHIFT && 1)
    goto L800;
  if (general_operand (x2, SImode))
    {
      ro[1] = x2;
      goto L807;
    }
  x1 = XEXP (x0, 0);
  goto L1147;

  L800:
  x3 = XEXP (x2, 0);
  if (GET_CODE (x3) == CONST_INT && XWINT (x3, 0) == 1 && 1)
    goto L801;
  x1 = XEXP (x0, 0);
  goto L1147;

  L801:
  x3 = XEXP (x2, 1);
  if (general_operand (x3, SImode))
    {
      ro[1] = x3;
      goto L802;
    }
  x1 = XEXP (x0, 0);
  goto L1147;

  L802:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, SImode))
    {
      ro[2] = x2;
      if (! TARGET_486 && GET_CODE (operands[1]) != CONST_INT)
	return 165;
      }
  x1 = XEXP (x0, 0);
  goto L1147;

  L807:
  x2 = XEXP (x1, 1);
  if (GET_MODE (x2) == SImode && GET_CODE (x2) == ASHIFT && 1)
    goto L808;
  if (general_operand (x2, SImode))
    {
      ro[2] = x2;
      return 115;
    }
  x1 = XEXP (x0, 0);
  goto L1147;

  L808:
  x3 = XEXP (x2, 0);
  if (GET_CODE (x3) == CONST_INT && XWINT (x3, 0) == 1 && 1)
    goto L809;
  x1 = XEXP (x0, 0);
  goto L1147;

  L809:
  x3 = XEXP (x2, 1);
  if (general_operand (x3, SImode))
    {
      ro[2] = x3;
      if (! TARGET_486 && GET_CODE (operands[2]) != CONST_INT)
	return 166;
      }
  x1 = XEXP (x0, 0);
  goto L1147;

  L590:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, SImode))
    {
      ro[1] = x2;
      return 119;
    }
  x1 = XEXP (x0, 0);
  goto L1147;

  L667:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, SImode))
    {
      ro[1] = x2;
      return 137;
    }
  x1 = XEXP (x0, 0);
  goto L1147;

  L692:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, SImode))
    {
      ro[1] = x2;
      goto L693;
    }
  x1 = XEXP (x0, 0);
  goto L1147;

  L693:
  x2 = XEXP (x1, 1);
  if (nonmemory_operand (x2, SImode))
    {
      ro[2] = x2;
      return 143;
    }
  x1 = XEXP (x0, 0);
  goto L1147;

  L720:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, SImode))
    {
      ro[1] = x2;
      goto L721;
    }
  x1 = XEXP (x0, 0);
  goto L1147;

  L721:
  x2 = XEXP (x1, 1);
  if (nonmemory_operand (x2, SImode))
    {
      ro[2] = x2;
      return 149;
    }
  x1 = XEXP (x0, 0);
  goto L1147;

  L748:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, SImode))
    {
      ro[1] = x2;
      goto L749;
    }
  x1 = XEXP (x0, 0);
  goto L1147;

  L749:
  x2 = XEXP (x1, 1);
  if (nonmemory_operand (x2, SImode))
    {
      ro[2] = x2;
      return 155;
    }
  x1 = XEXP (x0, 0);
  goto L1147;

  L763:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, SImode))
    {
      ro[1] = x2;
      goto L764;
    }
  x1 = XEXP (x0, 0);
  goto L1147;

  L764:
  x2 = XEXP (x1, 1);
  if (nonmemory_operand (x2, SImode))
    {
      ro[2] = x2;
      return 158;
    }
  x1 = XEXP (x0, 0);
  goto L1147;

  L778:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, SImode))
    {
      ro[1] = x2;
      goto L779;
    }
  x1 = XEXP (x0, 0);
  goto L1147;

  L779:
  x2 = XEXP (x1, 1);
  if (nonmemory_operand (x2, SImode))
    {
      ro[2] = x2;
      return 161;
    }
  x1 = XEXP (x0, 0);
  goto L1147;

  L212:
  x1 = XEXP (x0, 1);
  if (general_operand (x1, HImode))
    {
      ro[1] = x1;
      return 39;
    }
  x1 = XEXP (x0, 0);
  goto L214;

  L261:
  x1 = XEXP (x0, 1);
  switch (GET_MODE (x1))
    {
    case HImode:
      switch (GET_CODE (x1))
	{
	case ZERO_EXTEND:
	  goto L262;
	case SIGN_EXTEND:
	  goto L282;
	case PLUS:
	  goto L423;
	case MINUS:
	  goto L446;
	case AND:
	  goto L546;
	case IOR:
	  goto L561;
	case XOR:
	  goto L576;
	case NEG:
	  goto L594;
	case NOT:
	  goto L671;
	case ASHIFT:
	  goto L697;
	case ASHIFTRT:
	  goto L725;
	case LSHIFTRT:
	  goto L753;
	case ROTATE:
	  goto L768;
	case ROTATERT:
	  goto L783;
	}
      break;
    case SImode:
      if (GET_CODE (x1) == MULT && 1)
	goto L480;
    }
  if (general_operand (x1, HImode))
    {
      ro[1] = x1;
      return 40;
    }
  x1 = XEXP (x0, 0);
  goto L1147;

  L262:
  x2 = XEXP (x1, 0);
  if (nonimmediate_operand (x2, QImode))
    {
      ro[1] = x2;
      return 53;
    }
  x1 = XEXP (x0, 0);
  goto L1147;

  L282:
  x2 = XEXP (x1, 0);
  if (nonimmediate_operand (x2, QImode))
    {
      ro[1] = x2;
      return 58;
    }
  x1 = XEXP (x0, 0);
  goto L1147;

  L423:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, HImode))
    {
      ro[1] = x2;
      goto L424;
    }
  x1 = XEXP (x0, 0);
  goto L1147;

  L424:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, HImode))
    {
      ro[2] = x2;
      return 83;
    }
  x1 = XEXP (x0, 0);
  goto L1147;

  L446:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, HImode))
    {
      ro[1] = x2;
      goto L447;
    }
  x1 = XEXP (x0, 0);
  goto L1147;

  L447:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, HImode))
    {
      ro[2] = x2;
      return 90;
    }
  x1 = XEXP (x0, 0);
  goto L1147;

  L546:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, HImode))
    {
      ro[1] = x2;
      goto L547;
    }
  x1 = XEXP (x0, 0);
  goto L1147;

  L547:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, HImode))
    {
      ro[2] = x2;
      return 110;
    }
  x1 = XEXP (x0, 0);
  goto L1147;

  L561:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, HImode))
    {
      ro[1] = x2;
      goto L562;
    }
  x1 = XEXP (x0, 0);
  goto L1147;

  L562:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, HImode))
    {
      ro[2] = x2;
      return 113;
    }
  x1 = XEXP (x0, 0);
  goto L1147;

  L576:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, HImode))
    {
      ro[1] = x2;
      goto L577;
    }
  x1 = XEXP (x0, 0);
  goto L1147;

  L577:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, HImode))
    {
      ro[2] = x2;
      return 116;
    }
  x1 = XEXP (x0, 0);
  goto L1147;

  L594:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, HImode))
    {
      ro[1] = x2;
      return 120;
    }
  x1 = XEXP (x0, 0);
  goto L1147;

  L671:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, HImode))
    {
      ro[1] = x2;
      return 138;
    }
  x1 = XEXP (x0, 0);
  goto L1147;

  L697:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, HImode))
    {
      ro[1] = x2;
      goto L698;
    }
  x1 = XEXP (x0, 0);
  goto L1147;

  L698:
  x2 = XEXP (x1, 1);
  if (nonmemory_operand (x2, HImode))
    {
      ro[2] = x2;
      return 144;
    }
  x1 = XEXP (x0, 0);
  goto L1147;

  L725:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, HImode))
    {
      ro[1] = x2;
      goto L726;
    }
  x1 = XEXP (x0, 0);
  goto L1147;

  L726:
  x2 = XEXP (x1, 1);
  if (nonmemory_operand (x2, HImode))
    {
      ro[2] = x2;
      return 150;
    }
  x1 = XEXP (x0, 0);
  goto L1147;

  L753:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, HImode))
    {
      ro[1] = x2;
      goto L754;
    }
  x1 = XEXP (x0, 0);
  goto L1147;

  L754:
  x2 = XEXP (x1, 1);
  if (nonmemory_operand (x2, HImode))
    {
      ro[2] = x2;
      return 156;
    }
  x1 = XEXP (x0, 0);
  goto L1147;

  L768:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, HImode))
    {
      ro[1] = x2;
      goto L769;
    }
  x1 = XEXP (x0, 0);
  goto L1147;

  L769:
  x2 = XEXP (x1, 1);
  if (nonmemory_operand (x2, HImode))
    {
      ro[2] = x2;
      return 159;
    }
  x1 = XEXP (x0, 0);
  goto L1147;

  L783:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, HImode))
    {
      ro[1] = x2;
      goto L784;
    }
  x1 = XEXP (x0, 0);
  goto L1147;

  L784:
  x2 = XEXP (x1, 1);
  if (nonmemory_operand (x2, HImode))
    {
      ro[2] = x2;
      return 162;
    }
  x1 = XEXP (x0, 0);
  goto L1147;

  L480:
  x2 = XEXP (x1, 0);
  if (GET_MODE (x2) == HImode && GET_CODE (x2) == ZERO_EXTEND && 1)
    goto L481;
  if (general_operand (x2, HImode))
    {
      ro[1] = x2;
      goto L457;
    }
  x1 = XEXP (x0, 0);
  goto L1147;

  L481:
  x3 = XEXP (x2, 0);
  if (nonimmediate_operand (x3, QImode))
    {
      ro[1] = x3;
      goto L482;
    }
  x1 = XEXP (x0, 0);
  goto L1147;

  L482:
  x2 = XEXP (x1, 1);
  if (GET_MODE (x2) == HImode && GET_CODE (x2) == ZERO_EXTEND && 1)
    goto L483;
  x1 = XEXP (x0, 0);
  goto L1147;

  L483:
  x3 = XEXP (x2, 0);
  if (nonimmediate_operand (x3, QImode))
    {
      ro[2] = x3;
      return 98;
    }
  x1 = XEXP (x0, 0);
  goto L1147;

  L457:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, HImode))
    goto L463;
  x1 = XEXP (x0, 0);
  goto L1147;

  L463:
  ro[2] = x2;
  if (GET_CODE (operands[2]) == CONST_INT && INTVAL (operands[2]) == 0x80)
    return 94;
  L464:
  ro[2] = x2;
  return 95;

  L222:
  x1 = XEXP (x0, 1);
  if (general_operand (x1, QImode))
    {
      ro[1] = x1;
      return 42;
    }
  x1 = XEXP (x0, 0);
  goto L224;

  L427:
  x1 = XEXP (x0, 1);
  switch (GET_MODE (x1))
    {
    case QImode:
      switch (GET_CODE (x1))
	{
	case PLUS:
	  goto L428;
	case MINUS:
	  goto L451;
	case DIV:
	  goto L487;
	case UDIV:
	  goto L492;
	case AND:
	  goto L551;
	case IOR:
	  goto L566;
	case XOR:
	  goto L581;
	case NEG:
	  goto L598;
	case NOT:
	  goto L675;
	case ASHIFT:
	  goto L702;
	case ASHIFTRT:
	  goto L730;
	case LSHIFTRT:
	  goto L758;
	case ROTATE:
	  goto L773;
	case ROTATERT:
	  goto L788;
	}
    }
  if (general_operand (x1, QImode))
    {
      ro[1] = x1;
      return 43;
    }
  x1 = XEXP (x0, 0);
  goto L829;

  L428:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, QImode))
    {
      ro[1] = x2;
      goto L429;
    }
  x1 = XEXP (x0, 0);
  goto L829;

  L429:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, QImode))
    {
      ro[2] = x2;
      return 84;
    }
  x1 = XEXP (x0, 0);
  goto L829;

  L451:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, QImode))
    {
      ro[1] = x2;
      goto L452;
    }
  x1 = XEXP (x0, 0);
  goto L829;

  L452:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, QImode))
    {
      ro[2] = x2;
      return 91;
    }
  x1 = XEXP (x0, 0);
  goto L829;

  L487:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, HImode))
    {
      ro[1] = x2;
      goto L488;
    }
  x1 = XEXP (x0, 0);
  goto L829;

  L488:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, QImode))
    {
      ro[2] = x2;
      return 101;
    }
  x1 = XEXP (x0, 0);
  goto L829;

  L492:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, HImode))
    {
      ro[1] = x2;
      goto L493;
    }
  x1 = XEXP (x0, 0);
  goto L829;

  L493:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, QImode))
    {
      ro[2] = x2;
      return 102;
    }
  x1 = XEXP (x0, 0);
  goto L829;

  L551:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, QImode))
    {
      ro[1] = x2;
      goto L552;
    }
  x1 = XEXP (x0, 0);
  goto L829;

  L552:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, QImode))
    {
      ro[2] = x2;
      return 111;
    }
  x1 = XEXP (x0, 0);
  goto L829;

  L566:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, QImode))
    {
      ro[1] = x2;
      goto L567;
    }
  x1 = XEXP (x0, 0);
  goto L829;

  L567:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, QImode))
    {
      ro[2] = x2;
      return 114;
    }
  x1 = XEXP (x0, 0);
  goto L829;

  L581:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, QImode))
    {
      ro[1] = x2;
      goto L582;
    }
  x1 = XEXP (x0, 0);
  goto L829;

  L582:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, QImode))
    {
      ro[2] = x2;
      return 117;
    }
  x1 = XEXP (x0, 0);
  goto L829;

  L598:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, QImode))
    {
      ro[1] = x2;
      return 121;
    }
  x1 = XEXP (x0, 0);
  goto L829;

  L675:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, QImode))
    {
      ro[1] = x2;
      return 139;
    }
  x1 = XEXP (x0, 0);
  goto L829;

  L702:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, QImode))
    {
      ro[1] = x2;
      goto L703;
    }
  x1 = XEXP (x0, 0);
  goto L829;

  L703:
  x2 = XEXP (x1, 1);
  if (nonmemory_operand (x2, QImode))
    {
      ro[2] = x2;
      return 145;
    }
  x1 = XEXP (x0, 0);
  goto L829;

  L730:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, QImode))
    {
      ro[1] = x2;
      goto L731;
    }
  x1 = XEXP (x0, 0);
  goto L829;

  L731:
  x2 = XEXP (x1, 1);
  if (nonmemory_operand (x2, QImode))
    {
      ro[2] = x2;
      return 151;
    }
  x1 = XEXP (x0, 0);
  goto L829;

  L758:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, QImode))
    {
      ro[1] = x2;
      goto L759;
    }
  x1 = XEXP (x0, 0);
  goto L829;

  L759:
  x2 = XEXP (x1, 1);
  if (nonmemory_operand (x2, QImode))
    {
      ro[2] = x2;
      return 157;
    }
  x1 = XEXP (x0, 0);
  goto L829;

  L773:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, QImode))
    {
      ro[1] = x2;
      goto L774;
    }
  x1 = XEXP (x0, 0);
  goto L829;

  L774:
  x2 = XEXP (x1, 1);
  if (nonmemory_operand (x2, QImode))
    {
      ro[2] = x2;
      return 160;
    }
  x1 = XEXP (x0, 0);
  goto L829;

  L788:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, QImode))
    {
      ro[1] = x2;
      goto L789;
    }
  x1 = XEXP (x0, 0);
  goto L829;

  L789:
  x2 = XEXP (x1, 1);
  if (nonmemory_operand (x2, QImode))
    {
      ro[2] = x2;
      return 163;
    }
  x1 = XEXP (x0, 0);
  goto L829;

  L830:
  x1 = XEXP (x0, 1);
  if (GET_MODE (x1) != QImode)
    {
      x1 = XEXP (x0, 0);
      goto L1147;
    }
  switch (GET_CODE (x1))
    {
    case EQ:
      goto L831;
    case NE:
      goto L836;
    case GT:
      goto L841;
    case GTU:
      goto L846;
    case LT:
      goto L851;
    case LTU:
      goto L856;
    case GE:
      goto L861;
    case GEU:
      goto L866;
    case LE:
      goto L871;
    case LEU:
      goto L876;
    }
  x1 = XEXP (x0, 0);
  goto L1147;

  L831:
  x2 = XEXP (x1, 0);
  if (GET_CODE (x2) == CC0 && 1)
    goto L832;
  x1 = XEXP (x0, 0);
  goto L1147;

  L832:
  x2 = XEXP (x1, 1);
  if (GET_CODE (x2) == CONST_INT && XWINT (x2, 0) == 0 && 1)
    return 171;
  x1 = XEXP (x0, 0);
  goto L1147;

  L836:
  x2 = XEXP (x1, 0);
  if (GET_CODE (x2) == CC0 && 1)
    goto L837;
  x1 = XEXP (x0, 0);
  goto L1147;

  L837:
  x2 = XEXP (x1, 1);
  if (GET_CODE (x2) == CONST_INT && XWINT (x2, 0) == 0 && 1)
    return 173;
  x1 = XEXP (x0, 0);
  goto L1147;

  L841:
  x2 = XEXP (x1, 0);
  if (GET_CODE (x2) == CC0 && 1)
    goto L842;
  x1 = XEXP (x0, 0);
  goto L1147;

  L842:
  x2 = XEXP (x1, 1);
  if (GET_CODE (x2) == CONST_INT && XWINT (x2, 0) == 0 && 1)
    return 175;
  x1 = XEXP (x0, 0);
  goto L1147;

  L846:
  x2 = XEXP (x1, 0);
  if (GET_CODE (x2) == CC0 && 1)
    goto L847;
  x1 = XEXP (x0, 0);
  goto L1147;

  L847:
  x2 = XEXP (x1, 1);
  if (GET_CODE (x2) == CONST_INT && XWINT (x2, 0) == 0 && 1)
    return 177;
  x1 = XEXP (x0, 0);
  goto L1147;

  L851:
  x2 = XEXP (x1, 0);
  if (GET_CODE (x2) == CC0 && 1)
    goto L852;
  x1 = XEXP (x0, 0);
  goto L1147;

  L852:
  x2 = XEXP (x1, 1);
  if (GET_CODE (x2) == CONST_INT && XWINT (x2, 0) == 0 && 1)
    return 179;
  x1 = XEXP (x0, 0);
  goto L1147;

  L856:
  x2 = XEXP (x1, 0);
  if (GET_CODE (x2) == CC0 && 1)
    goto L857;
  x1 = XEXP (x0, 0);
  goto L1147;

  L857:
  x2 = XEXP (x1, 1);
  if (GET_CODE (x2) == CONST_INT && XWINT (x2, 0) == 0 && 1)
    return 181;
  x1 = XEXP (x0, 0);
  goto L1147;

  L861:
  x2 = XEXP (x1, 0);
  if (GET_CODE (x2) == CC0 && 1)
    goto L862;
  x1 = XEXP (x0, 0);
  goto L1147;

  L862:
  x2 = XEXP (x1, 1);
  if (GET_CODE (x2) == CONST_INT && XWINT (x2, 0) == 0 && 1)
    return 183;
  x1 = XEXP (x0, 0);
  goto L1147;

  L866:
  x2 = XEXP (x1, 0);
  if (GET_CODE (x2) == CC0 && 1)
    goto L867;
  x1 = XEXP (x0, 0);
  goto L1147;

  L867:
  x2 = XEXP (x1, 1);
  if (GET_CODE (x2) == CONST_INT && XWINT (x2, 0) == 0 && 1)
    return 185;
  x1 = XEXP (x0, 0);
  goto L1147;

  L871:
  x2 = XEXP (x1, 0);
  if (GET_CODE (x2) == CC0 && 1)
    goto L872;
  x1 = XEXP (x0, 0);
  goto L1147;

  L872:
  x2 = XEXP (x1, 1);
  if (GET_CODE (x2) == CONST_INT && XWINT (x2, 0) == 0 && 1)
    return 187;
  x1 = XEXP (x0, 0);
  goto L1147;

  L876:
  x2 = XEXP (x1, 0);
  if (GET_CODE (x2) == CC0 && 1)
    goto L877;
  x1 = XEXP (x0, 0);
  goto L1147;

  L877:
  x2 = XEXP (x1, 1);
  if (GET_CODE (x2) == CONST_INT && XWINT (x2, 0) == 0 && 1)
    return 189;
  x1 = XEXP (x0, 0);
  goto L1147;

  L232:
  x1 = XEXP (x0, 1);
  if (general_operand (x1, SFmode))
    {
      ro[1] = x1;
      return 45;
    }
  x1 = XEXP (x0, 0);
  goto L234;

  L235:
  x1 = XEXP (x0, 1);
  if (general_operand (x1, SFmode))
    {
      ro[1] = x1;
      return 46;
    }
  x1 = XEXP (x0, 0);
  goto L399;

  L400:
  x1 = XEXP (x0, 1);
  if (GET_MODE (x1) != SFmode)
    {
      x1 = XEXP (x0, 0);
      goto L1147;
    }
  switch (GET_CODE (x1))
    {
    case FLOAT:
      goto L401;
    case NEG:
      goto L602;
    case ABS:
      goto L615;
    case SQRT:
      goto L628;
    case UNSPEC:
      if (XINT (x1, 1) == 1 && XVECLEN (x1, 0) == 1 && 1)
	goto L645;
      if (XINT (x1, 1) == 2 && XVECLEN (x1, 0) == 1 && 1)
	goto L658;
    }
  x1 = XEXP (x0, 0);
  goto L1147;

  L401:
  x2 = XEXP (x1, 0);
  switch (GET_MODE (x2))
    {
    case DImode:
      if (nonimmediate_operand (x2, DImode))
	{
	  ro[1] = x2;
	  if (TARGET_80387)
	    return 78;
	  }
      break;
    case SImode:
      if (nonimmediate_operand (x2, SImode))
	{
	  ro[1] = x2;
	  if (TARGET_80387)
	    return 80;
	  }
    }
  x1 = XEXP (x0, 0);
  goto L1147;

  L602:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, SFmode))
    {
      ro[1] = x2;
      if (TARGET_80387)
	return 122;
      }
  x1 = XEXP (x0, 0);
  goto L1147;

  L615:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, SFmode))
    {
      ro[1] = x2;
      if (TARGET_80387)
	return 125;
      }
  x1 = XEXP (x0, 0);
  goto L1147;

  L628:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, SFmode))
    {
      ro[1] = x2;
      if (TARGET_80387 && (TARGET_IEEE_FP || flag_fast_math))
	return 128;
      }
  x1 = XEXP (x0, 0);
  goto L1147;

  L645:
  x2 = XVECEXP (x1, 0, 0);
  if (register_operand (x2, SFmode))
    {
      ro[1] = x2;
      if (TARGET_80387 && (TARGET_IEEE_FP || flag_fast_math))
	return 132;
      }
  x1 = XEXP (x0, 0);
  goto L1147;

  L658:
  x2 = XVECEXP (x1, 0, 0);
  if (register_operand (x2, SFmode))
    {
      ro[1] = x2;
      if (TARGET_80387 && (TARGET_IEEE_FP || flag_fast_math))
	return 135;
      }
  x1 = XEXP (x0, 0);
  goto L1147;

  L238:
  x1 = XEXP (x0, 1);
  if (general_operand (x1, DFmode))
    {
      ro[1] = x1;
      return 47;
    }
  x1 = XEXP (x0, 0);
  goto L247;

  L289:
  x1 = XEXP (x0, 1);
  if (GET_MODE (x1) == DFmode && GET_CODE (x1) == FLOAT_EXTEND && 1)
    goto L290;
  if (general_operand (x1, DFmode))
    {
      ro[1] = x1;
      return 49;
    }
  x1 = XEXP (x0, 0);
  goto L395;

  L290:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, SFmode))
    {
      ro[1] = x2;
      if (TARGET_80387)
	return 60;
      }
  x1 = XEXP (x0, 0);
  goto L395;

  L396:
  x1 = XEXP (x0, 1);
  if (GET_MODE (x1) != DFmode)
    {
      x1 = XEXP (x0, 0);
      goto L1147;
    }
  switch (GET_CODE (x1))
    {
    case FLOAT:
      goto L397;
    case NEG:
      goto L610;
    case ABS:
      goto L623;
    case SQRT:
      goto L636;
    case UNSPEC:
      if (XINT (x1, 1) == 1 && XVECLEN (x1, 0) == 1 && 1)
	goto L649;
      if (XINT (x1, 1) == 2 && XVECLEN (x1, 0) == 1 && 1)
	goto L662;
    }
  x1 = XEXP (x0, 0);
  goto L1147;

  L397:
  x2 = XEXP (x1, 0);
  switch (GET_MODE (x2))
    {
    case DImode:
      if (nonimmediate_operand (x2, DImode))
	{
	  ro[1] = x2;
	  if (TARGET_80387)
	    return 77;
	  }
      break;
    case SImode:
      if (nonimmediate_operand (x2, SImode))
	{
	  ro[1] = x2;
	  if (TARGET_80387)
	    return 79;
	  }
    }
  x1 = XEXP (x0, 0);
  goto L1147;

  L610:
  x2 = XEXP (x1, 0);
  if (GET_MODE (x2) == DFmode && GET_CODE (x2) == FLOAT_EXTEND && 1)
    goto L611;
  if (general_operand (x2, DFmode))
    {
      ro[1] = x2;
      if (TARGET_80387)
	return 123;
      }
  x1 = XEXP (x0, 0);
  goto L1147;

  L611:
  x3 = XEXP (x2, 0);
  if (general_operand (x3, SFmode))
    {
      ro[1] = x3;
      if (TARGET_80387)
	return 124;
      }
  x1 = XEXP (x0, 0);
  goto L1147;

  L623:
  x2 = XEXP (x1, 0);
  if (GET_MODE (x2) == DFmode && GET_CODE (x2) == FLOAT_EXTEND && 1)
    goto L624;
  if (general_operand (x2, DFmode))
    {
      ro[1] = x2;
      if (TARGET_80387)
	return 126;
      }
  x1 = XEXP (x0, 0);
  goto L1147;

  L624:
  x3 = XEXP (x2, 0);
  if (general_operand (x3, SFmode))
    {
      ro[1] = x3;
      if (TARGET_80387)
	return 127;
      }
  x1 = XEXP (x0, 0);
  goto L1147;

  L636:
  x2 = XEXP (x1, 0);
  if (GET_MODE (x2) == DFmode && GET_CODE (x2) == FLOAT_EXTEND && 1)
    goto L637;
  if (general_operand (x2, DFmode))
    {
      ro[1] = x2;
      if (TARGET_80387 && (TARGET_IEEE_FP || flag_fast_math))
	return 129;
      }
  x1 = XEXP (x0, 0);
  goto L1147;

  L637:
  x3 = XEXP (x2, 0);
  if (general_operand (x3, SFmode))
    {
      ro[1] = x3;
      if (TARGET_80387 && (TARGET_IEEE_FP || flag_fast_math))
	return 130;
      }
  x1 = XEXP (x0, 0);
  goto L1147;

  L649:
  x2 = XVECEXP (x1, 0, 0);
  if (GET_MODE (x2) != DFmode)
    {
      x1 = XEXP (x0, 0);
      goto L1147;
    }
  if (GET_CODE (x2) == FLOAT_EXTEND && 1)
    goto L650;
  if (register_operand (x2, DFmode))
    {
      ro[1] = x2;
      if (TARGET_80387 && (TARGET_IEEE_FP || flag_fast_math))
	return 131;
      }
  x1 = XEXP (x0, 0);
  goto L1147;

  L650:
  x3 = XEXP (x2, 0);
  if (register_operand (x3, SFmode))
    {
      ro[1] = x3;
      if (TARGET_80387 && (TARGET_IEEE_FP || flag_fast_math))
	return 133;
      }
  x1 = XEXP (x0, 0);
  goto L1147;

  L662:
  x2 = XVECEXP (x1, 0, 0);
  if (GET_MODE (x2) != DFmode)
    {
      x1 = XEXP (x0, 0);
      goto L1147;
    }
  if (GET_CODE (x2) == FLOAT_EXTEND && 1)
    goto L663;
  if (register_operand (x2, DFmode))
    {
      ro[1] = x2;
      if (TARGET_80387 && (TARGET_IEEE_FP || flag_fast_math))
	return 134;
      }
  x1 = XEXP (x0, 0);
  goto L1147;

  L663:
  x3 = XEXP (x2, 0);
  if (register_operand (x3, SFmode))
    {
      ro[1] = x3;
      if (TARGET_80387 && (TARGET_IEEE_FP || flag_fast_math))
	return 136;
      }
  x1 = XEXP (x0, 0);
  goto L1147;

  L251:
  x1 = XEXP (x0, 1);
  if (general_operand (x1, DImode))
    {
      ro[1] = x1;
      return 50;
    }
  x1 = XEXP (x0, 0);
  goto L253;

  L412:
  x1 = XEXP (x0, 1);
  switch (GET_MODE (x1))
    {
    case DImode:
      switch (GET_CODE (x1))
	{
	case PLUS:
	  goto L413;
	case MINUS:
	  goto L436;
	case NEG:
	  goto L586;
	}
    }
  if (general_operand (x1, DImode))
    {
      ro[1] = x1;
      return 51;
    }
  x1 = XEXP (x0, 0);
  goto L268;

  L413:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, DImode))
    {
      ro[1] = x2;
      goto L414;
    }
  x1 = XEXP (x0, 0);
  goto L268;

  L414:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, DImode))
    {
      ro[2] = x2;
      return 81;
    }
  x1 = XEXP (x0, 0);
  goto L268;

  L436:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, DImode))
    {
      ro[1] = x2;
      goto L437;
    }
  x1 = XEXP (x0, 0);
  goto L268;

  L437:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, DImode))
    {
      ro[2] = x2;
      return 88;
    }
  x1 = XEXP (x0, 0);
  goto L268;

  L586:
  x2 = XEXP (x1, 0);
  if (general_operand (x2, DImode))
    {
      ro[1] = x2;
      return 118;
    }
  x1 = XEXP (x0, 0);
  goto L268;

  L269:
  x1 = XEXP (x0, 1);
  if (GET_MODE (x1) != DImode)
    {
      x1 = XEXP (x0, 0);
      goto L1147;
    }
  switch (GET_CODE (x1))
    {
    case ZERO_EXTEND:
      goto L270;
    case SIGN_EXTEND:
      goto L274;
    case ASHIFT:
      goto L679;
    case ASHIFTRT:
      goto L707;
    case LSHIFTRT:
      goto L735;
    }
  x1 = XEXP (x0, 0);
  goto L1147;

  L270:
  x2 = XEXP (x1, 0);
  if (register_operand (x2, SImode))
    {
      ro[1] = x2;
      return 55;
    }
  x1 = XEXP (x0, 0);
  goto L1147;

  L274:
  x2 = XEXP (x1, 0);
  if (register_operand (x2, SImode))
    {
      ro[1] = x2;
      return 56;
    }
  x1 = XEXP (x0, 0);
  goto L1147;

  L679:
  x2 = XEXP (x1, 0);
  if (register_operand (x2, DImode))
    {
      ro[1] = x2;
      goto L680;
    }
  x1 = XEXP (x0, 0);
  goto L1147;

  L680:
  x2 = XEXP (x1, 1);
  if (GET_CODE (x2) == CONST_INT && 1)
    {
      ro[2] = x2;
      return 141;
    }
  x1 = XEXP (x0, 0);
  goto L1147;

  L707:
  x2 = XEXP (x1, 0);
  if (register_operand (x2, DImode))
    {
      ro[1] = x2;
      goto L708;
    }
  x1 = XEXP (x0, 0);
  goto L1147;

  L708:
  x2 = XEXP (x1, 1);
  if (GET_CODE (x2) == CONST_INT && 1)
    {
      ro[2] = x2;
      return 147;
    }
  x1 = XEXP (x0, 0);
  goto L1147;

  L735:
  x2 = XEXP (x1, 0);
  if (register_operand (x2, DImode))
    {
      ro[1] = x2;
      goto L736;
    }
  x1 = XEXP (x0, 0);
  goto L1147;

  L736:
  x2 = XEXP (x1, 1);
  if (GET_CODE (x2) == CONST_INT && 1)
    {
      ro[2] = x2;
      return 153;
    }
  x1 = XEXP (x0, 0);
  goto L1147;
 L2:
  tem = recog_1 (x0, insn, pnum_clobbers);
  if (tem >= 0) return tem;
  x1 = XEXP (x0, 0);
  goto L1147;

  L218:
  x2 = XEXP (x1, 0);
  switch (GET_MODE (x2))
    {
    case HImode:
      if (general_operand (x2, HImode))
	{
	  ro[0] = x2;
	  goto L219;
	}
      break;
    case QImode:
      if (general_operand (x2, QImode))
	{
	  ro[0] = x2;
	  goto L229;
	}
    }
  goto L1147;

  L219:
  x1 = XEXP (x0, 1);
  if (general_operand (x1, HImode))
    {
      ro[1] = x1;
      return 41;
    }
  x1 = XEXP (x0, 0);
  goto L1147;

  L229:
  x1 = XEXP (x0, 1);
  if (general_operand (x1, QImode))
    {
      ro[1] = x1;
      return 44;
    }
  x1 = XEXP (x0, 0);
  goto L1147;

  L1081:
  x1 = XEXP (x0, 1);
  switch (GET_CODE (x1))
    {
    case MINUS:
      if (GET_MODE (x1) == SImode && 1)
	goto L1082;
      break;
    case IF_THEN_ELSE:
      goto L881;
    case LABEL_REF:
      goto L1061;
    }
  L1064:
  if (general_operand (x1, SImode))
    {
      ro[0] = x1;
      return 221;
    }
  x1 = XEXP (x0, 0);
  goto L1147;

  L1082:
  x2 = XEXP (x1, 0);
  if (GET_MODE (x2) == SImode && GET_CODE (x2) == REG && XINT (x2, 0) == 3 && 1)
    goto L1083;
  x1 = XEXP (x0, 0);
  goto L1147;

  L1083:
  x2 = XEXP (x1, 1);
  if (GET_MODE (x2) == SImode && GET_CODE (x2) == MEM && 1)
    goto L1084;
  x1 = XEXP (x0, 0);
  goto L1147;

  L1084:
  x3 = XEXP (x2, 0);
  if (GET_MODE (x3) == SImode && GET_CODE (x3) == PLUS && 1)
    goto L1085;
  x1 = XEXP (x0, 0);
  goto L1147;

  L1085:
  x4 = XEXP (x3, 0);
  if (GET_MODE (x4) == SImode && GET_CODE (x4) == MULT && 1)
    goto L1086;
  x1 = XEXP (x0, 0);
  goto L1147;

  L1086:
  x5 = XEXP (x4, 0);
  if (register_operand (x5, SImode))
    {
      ro[0] = x5;
      goto L1087;
    }
  x1 = XEXP (x0, 0);
  goto L1147;

  L1087:
  x5 = XEXP (x4, 1);
  if (GET_CODE (x5) == CONST_INT && XWINT (x5, 0) == 4 && 1)
    goto L1088;
  x1 = XEXP (x0, 0);
  goto L1147;

  L1088:
  x4 = XEXP (x3, 1);
  if (GET_CODE (x4) == LABEL_REF && 1)
    goto L1089;
  x1 = XEXP (x0, 0);
  goto L1147;

  L1089:
  x5 = XEXP (x4, 0);
  if (pnum_clobbers != 0 && 1)
    {
      ro[1] = x5;
      *pnum_clobbers = 1;
      return 223;
    }
  x1 = XEXP (x0, 0);
  goto L1147;
 L881:
  tem = recog_2 (x0, insn, pnum_clobbers);
  if (tem >= 0) return tem;
  x1 = XEXP (x0, 0);
  goto L1147;

  L1061:
  x2 = XEXP (x1, 0);
  ro[0] = x2;
  return 220;

  L1148:
  x1 = XEXP (x0, 1);
  if (GET_CODE (x1) == CALL && 1)
    goto L1149;
  x1 = XEXP (x0, 0);
  goto L1236;

  L1149:
  x2 = XEXP (x1, 0);
  if (call_insn_operand (x2, QImode))
    {
      ro[1] = x2;
      goto L1150;
    }
  L1154:
  if (GET_MODE (x2) == QImode && GET_CODE (x2) == MEM && 1)
    goto L1155;
  x1 = XEXP (x0, 0);
  goto L1236;

  L1150:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, SImode))
    {
      ro[2] = x2;
      return 235;
    }
  x2 = XEXP (x1, 0);
  goto L1154;

  L1155:
  x3 = XEXP (x2, 0);
  if (symbolic_operand (x3, SImode))
    {
      ro[1] = x3;
      goto L1156;
    }
  x1 = XEXP (x0, 0);
  goto L1236;

  L1156:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, SImode))
    {
      ro[2] = x2;
      if (!HALF_PIC_P ())
	return 236;
      }
  x1 = XEXP (x0, 0);
  goto L1236;

  L1237:
  x1 = XEXP (x0, 1);
  if (GET_MODE (x1) == SImode && GET_CODE (x1) == PLUS && 1)
    goto L1238;
  goto ret0;

  L1238:
  x2 = XEXP (x1, 0);
  if (GET_MODE (x2) == SImode && GET_CODE (x2) == FFS && 1)
    goto L1239;
  goto ret0;

  L1239:
  x3 = XEXP (x2, 0);
  if (general_operand (x3, SImode))
    {
      ro[1] = x3;
      goto L1240;
    }
  goto ret0;

  L1240:
  x2 = XEXP (x1, 1);
  if (GET_CODE (x2) == CONST_INT && XWINT (x2, 0) == -1 && pnum_clobbers != 0 && 1)
    {
      *pnum_clobbers = 1;
      return 250;
    }
  goto ret0;

  L1252:
  x1 = XEXP (x0, 1);
  if (GET_MODE (x1) == HImode && GET_CODE (x1) == PLUS && 1)
    goto L1253;
  goto ret0;

  L1253:
  x2 = XEXP (x1, 0);
  if (GET_MODE (x2) == HImode && GET_CODE (x2) == FFS && 1)
    goto L1254;
  goto ret0;

  L1254:
  x3 = XEXP (x2, 0);
  if (general_operand (x3, HImode))
    {
      ro[1] = x3;
      goto L1255;
    }
  goto ret0;

  L1255:
  x2 = XEXP (x1, 1);
  if (GET_CODE (x2) == CONST_INT && XWINT (x2, 0) == -1 && pnum_clobbers != 0 && 1)
    {
      *pnum_clobbers = 1;
      return 252;
    }
  goto ret0;

  L1258:
  x1 = XEXP (x0, 1);
  if (binary_387_op (x1, DFmode))
    {
      ro[3] = x1;
      goto L1264;
    }
  goto ret0;

  L1264:
  x2 = XEXP (x1, 0);
  switch (GET_MODE (x2))
    {
    case DFmode:
      switch (GET_CODE (x2))
	{
	case FLOAT:
	  goto L1265;
	case FLOAT_EXTEND:
	  goto L1271;
	case SUBREG:
	case REG:
	case MEM:
	  if (nonimmediate_operand (x2, DFmode))
	    {
	      ro[1] = x2;
	      goto L1260;
	    }
	}
    }
  L1276:
  if (general_operand (x2, DFmode))
    {
      ro[1] = x2;
      goto L1277;
    }
  goto ret0;

  L1265:
  x3 = XEXP (x2, 0);
  if (general_operand (x3, SImode))
    {
      ro[1] = x3;
      goto L1266;
    }
  goto ret0;

  L1266:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, DFmode))
    {
      ro[2] = x2;
      if (TARGET_80387)
	return 254;
      }
  goto ret0;

  L1271:
  x3 = XEXP (x2, 0);
  if (general_operand (x3, SFmode))
    {
      ro[1] = x3;
      goto L1272;
    }
  goto ret0;

  L1272:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, DFmode))
    {
      ro[2] = x2;
      if (TARGET_80387)
	return 255;
      }
  goto ret0;

  L1260:
  x2 = XEXP (x1, 1);
  if (nonimmediate_operand (x2, DFmode))
    {
      ro[2] = x2;
      if (TARGET_80387)
	return 253;
      }
  x2 = XEXP (x1, 0);
  goto L1276;

  L1277:
  x2 = XEXP (x1, 1);
  if (GET_MODE (x2) != DFmode)
    goto ret0;
  switch (GET_CODE (x2))
    {
    case FLOAT:
      goto L1278;
    case FLOAT_EXTEND:
      goto L1284;
    }
  goto ret0;

  L1278:
  x3 = XEXP (x2, 0);
  if (general_operand (x3, SImode))
    {
      ro[2] = x3;
      if (TARGET_80387)
	return 256;
      }
  goto ret0;

  L1284:
  x3 = XEXP (x2, 0);
  if (general_operand (x3, SFmode))
    {
      ro[2] = x3;
      if (TARGET_80387)
	return 257;
      }
  goto ret0;

  L1287:
  x1 = XEXP (x0, 1);
  if (binary_387_op (x1, SFmode))
    {
      ro[3] = x1;
      goto L1293;
    }
  goto ret0;

  L1293:
  x2 = XEXP (x1, 0);
  switch (GET_MODE (x2))
    {
    case SFmode:
      if (GET_CODE (x2) == FLOAT && 1)
	goto L1294;
      if (nonimmediate_operand (x2, SFmode))
	{
	  ro[1] = x2;
	  goto L1289;
	}
    }
  L1299:
  if (general_operand (x2, SFmode))
    {
      ro[1] = x2;
      goto L1300;
    }
  goto ret0;

  L1294:
  x3 = XEXP (x2, 0);
  if (general_operand (x3, SImode))
    {
      ro[1] = x3;
      goto L1295;
    }
  goto ret0;

  L1295:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, SFmode))
    {
      ro[2] = x2;
      if (TARGET_80387)
	return 259;
      }
  goto ret0;

  L1289:
  x2 = XEXP (x1, 1);
  if (nonimmediate_operand (x2, SFmode))
    {
      ro[2] = x2;
      if (TARGET_80387)
	return 258;
      }
  x2 = XEXP (x1, 0);
  goto L1299;

  L1300:
  x2 = XEXP (x1, 1);
  if (GET_MODE (x2) == SFmode && GET_CODE (x2) == FLOAT && 1)
    goto L1301;
  goto ret0;

  L1301:
  x3 = XEXP (x2, 0);
  if (general_operand (x3, SImode))
    {
      ro[2] = x3;
      if (TARGET_80387)
	return 260;
      }
  goto ret0;
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

  x1 = XVECEXP (x0, 0, 0);
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
    }
  L45:
  if (VOIDmode_compare_op (x2, VOIDmode))
    {
      ro[2] = x2;
      goto L74;
    }
  L118:
  if (GET_MODE (x2) == CCFPEQmode && GET_CODE (x2) == COMPARE && 1)
    goto L119;
  goto ret0;

  L13:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L14;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 1);
  goto L45;

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
  goto L45;

  L22:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L23;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 1);
  goto L45;

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
  goto L45;

  L74:
  x3 = XEXP (x2, 0);
  switch (GET_MODE (x3))
    {
    case DFmode:
      switch (GET_CODE (x3))
	{
	case FLOAT:
	  goto L75;
	case FLOAT_EXTEND:
	  goto L105;
	case SUBREG:
	case REG:
	case MEM:
	  if (nonimmediate_operand (x3, DFmode))
	    {
	      ro[0] = x3;
	      goto L47;
	    }
	}
    L59:
      if (register_operand (x3, DFmode))
	{
	  ro[0] = x3;
	  goto L60;
	}
      break;
    case SFmode:
      if (GET_CODE (x3) == FLOAT && 1)
	goto L161;
      if (nonimmediate_operand (x3, SFmode))
	{
	  ro[0] = x3;
	  goto L133;
	}
    L145:
      if (register_operand (x3, SFmode))
	{
	  ro[0] = x3;
	  goto L146;
	}
    }
  goto L118;

  L75:
  x4 = XEXP (x3, 0);
  if (nonimmediate_operand (x4, SImode))
    {
      ro[0] = x4;
      goto L76;
    }
  goto L118;

  L76:
  x3 = XEXP (x2, 1);
  if (register_operand (x3, DFmode))
    {
      ro[1] = x3;
      goto L77;
    }
  goto L118;

  L77:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L78;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 1);
  goto L118;

  L78:
  x2 = XEXP (x1, 0);
  if (scratch_operand (x2, HImode))
    {
      ro[3] = x2;
      if (TARGET_80387)
	return 18;
      }
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 1);
  goto L118;

  L105:
  x4 = XEXP (x3, 0);
  if (nonimmediate_operand (x4, SFmode))
    {
      ro[0] = x4;
      goto L106;
    }
  goto L118;

  L106:
  x3 = XEXP (x2, 1);
  if (register_operand (x3, DFmode))
    {
      ro[1] = x3;
      goto L107;
    }
  goto L118;

  L107:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L108;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 1);
  goto L118;

  L108:
  x2 = XEXP (x1, 0);
  if (scratch_operand (x2, HImode))
    {
      ro[3] = x2;
      if (TARGET_80387)
	return 20;
      }
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 1);
  goto L118;

  L47:
  x3 = XEXP (x2, 1);
  if (nonimmediate_operand (x3, DFmode))
    {
      ro[1] = x3;
      goto L48;
    }
  x3 = XEXP (x2, 0);
  goto L59;

  L48:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L49;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 1);
  x3 = XEXP (x2, 0);
  goto L59;

  L49:
  x2 = XEXP (x1, 0);
  if (scratch_operand (x2, HImode))
    {
      ro[3] = x2;
      if (TARGET_80387
   && (GET_CODE (operands[0]) != MEM || GET_CODE (operands[1]) != MEM))
	return 16;
      }
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 1);
  x3 = XEXP (x2, 0);
  goto L59;

  L60:
  x3 = XEXP (x2, 1);
  if (GET_MODE (x3) != DFmode)
    {
      goto L118;
    }
  switch (GET_CODE (x3))
    {
    case FLOAT:
      goto L61;
    case FLOAT_EXTEND:
      goto L91;
    }
  goto L118;

  L61:
  x4 = XEXP (x3, 0);
  if (nonimmediate_operand (x4, SImode))
    {
      ro[1] = x4;
      goto L62;
    }
  goto L118;

  L62:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L63;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 1);
  goto L118;

  L63:
  x2 = XEXP (x1, 0);
  if (scratch_operand (x2, HImode))
    {
      ro[3] = x2;
      if (TARGET_80387)
	return 17;
      }
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 1);
  goto L118;

  L91:
  x4 = XEXP (x3, 0);
  if (nonimmediate_operand (x4, SFmode))
    {
      ro[1] = x4;
      goto L92;
    }
  goto L118;

  L92:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L93;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 1);
  goto L118;

  L93:
  x2 = XEXP (x1, 0);
  if (scratch_operand (x2, HImode))
    {
      ro[3] = x2;
      if (TARGET_80387)
	return 19;
      }
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 1);
  goto L118;

  L161:
  x4 = XEXP (x3, 0);
  if (nonimmediate_operand (x4, SImode))
    {
      ro[0] = x4;
      goto L162;
    }
  goto L118;

  L162:
  x3 = XEXP (x2, 1);
  if (register_operand (x3, SFmode))
    {
      ro[1] = x3;
      goto L163;
    }
  goto L118;

  L163:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L164;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 1);
  goto L118;

  L164:
  x2 = XEXP (x1, 0);
  if (scratch_operand (x2, HImode))
    {
      ro[3] = x2;
      if (TARGET_80387)
	return 24;
      }
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 1);
  goto L118;

  L133:
  x3 = XEXP (x2, 1);
  if (nonimmediate_operand (x3, SFmode))
    {
      ro[1] = x3;
      goto L134;
    }
  x3 = XEXP (x2, 0);
  goto L145;

  L134:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L135;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 1);
  x3 = XEXP (x2, 0);
  goto L145;

  L135:
  x2 = XEXP (x1, 0);
  if (scratch_operand (x2, HImode))
    {
      ro[3] = x2;
      if (TARGET_80387
   && (GET_CODE (operands[0]) != MEM || GET_CODE (operands[1]) != MEM))
	return 22;
      }
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 1);
  x3 = XEXP (x2, 0);
  goto L145;

  L146:
  x3 = XEXP (x2, 1);
  if (GET_MODE (x3) == SFmode && GET_CODE (x3) == FLOAT && 1)
    goto L147;
  goto L118;

  L147:
  x4 = XEXP (x3, 0);
  if (nonimmediate_operand (x4, SImode))
    {
      ro[1] = x4;
      goto L148;
    }
  goto L118;

  L148:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L149;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 1);
  goto L118;

  L149:
  x2 = XEXP (x1, 0);
  if (scratch_operand (x2, HImode))
    {
      ro[3] = x2;
      if (TARGET_80387)
	return 23;
      }
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 1);
  goto L118;

  L119:
  x3 = XEXP (x2, 0);
  switch (GET_MODE (x3))
    {
    case DFmode:
      if (register_operand (x3, DFmode))
	{
	  ro[0] = x3;
	  goto L120;
	}
      break;
    case SFmode:
      if (register_operand (x3, SFmode))
	{
	  ro[0] = x3;
	  goto L176;
	}
    }
  goto ret0;

  L120:
  x3 = XEXP (x2, 1);
  if (register_operand (x3, DFmode))
    {
      ro[1] = x3;
      goto L121;
    }
  goto ret0;

  L121:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L122;
  goto ret0;

  L122:
  x2 = XEXP (x1, 0);
  if (scratch_operand (x2, HImode))
    {
      ro[2] = x2;
      if (TARGET_80387)
	return 21;
      }
  goto ret0;

  L176:
  x3 = XEXP (x2, 1);
  if (register_operand (x3, SFmode))
    {
      ro[1] = x3;
      goto L177;
    }
  goto ret0;

  L177:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L178;
  goto ret0;

  L178:
  x2 = XEXP (x1, 0);
  if (scratch_operand (x2, HImode))
    {
      ro[2] = x2;
      if (TARGET_80387)
	return 25;
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
  x2 = XEXP (x1, 0);
  switch (GET_MODE (x2))
    {
    case DFmode:
      if (register_operand (x2, DFmode))
	{
	  ro[0] = x2;
	  goto L242;
	}
      break;
    case SFmode:
      if (nonimmediate_operand (x2, SFmode))
	{
	  ro[0] = x2;
	  goto L294;
	}
      break;
    case SImode:
      if (register_operand (x2, SImode))
	{
	  ro[0] = x2;
	  goto L497;
	}
      break;
    case HImode:
      if (register_operand (x2, HImode))
	{
	  ro[0] = x2;
	  goto L508;
	}
      break;
    case DImode:
      if (register_operand (x2, DImode))
	{
	  ro[0] = x2;
	  goto L684;
	}
    }
  switch (GET_CODE (x2))
    {
    case CC0:
      goto L12;
    case PC:
      goto L1068;
    }
  L1125:
  ro[0] = x2;
  goto L1126;
  L1228:
  switch (GET_MODE (x2))
    {
    case SImode:
      if (register_operand (x2, SImode))
	{
	  ro[0] = x2;
	  goto L1229;
	}
      break;
    case HImode:
      if (register_operand (x2, HImode))
	{
	  ro[0] = x2;
	  goto L1244;
	}
    }
  goto ret0;

  L242:
  x2 = XEXP (x1, 1);
  if (register_operand (x2, DFmode))
    {
      ro[1] = x2;
      goto L243;
    }
  x2 = XEXP (x1, 0);
  goto L1125;

  L243:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == SET && 1)
    goto L244;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1125;

  L244:
  x2 = XEXP (x1, 0);
  if (rtx_equal_p (x2, ro[1]) && 1)
    goto L245;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1125;

  L245:
  x2 = XEXP (x1, 1);
  if (rtx_equal_p (x2, ro[0]) && 1)
    return 48;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1125;

  L294:
  x2 = XEXP (x1, 1);
  if (GET_MODE (x2) == SFmode && GET_CODE (x2) == FLOAT_TRUNCATE && 1)
    goto L295;
  x2 = XEXP (x1, 0);
  goto L1125;

  L295:
  x3 = XEXP (x2, 0);
  if (register_operand (x3, DFmode))
    {
      ro[1] = x3;
      goto L296;
    }
  x2 = XEXP (x1, 0);
  goto L1125;

  L296:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L297;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1125;

  L297:
  x2 = XEXP (x1, 0);
  if (memory_operand (x2, SFmode))
    {
      ro[2] = x2;
      if (TARGET_80387)
	return 62;
      }
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1125;

  L497:
  x2 = XEXP (x1, 1);
  if (GET_MODE (x2) != SImode)
    {
      x2 = XEXP (x1, 0);
      goto L1125;
    }
  switch (GET_CODE (x2))
    {
    case DIV:
      goto L498;
    case UDIV:
      goto L520;
    }
  x2 = XEXP (x1, 0);
  goto L1125;

  L498:
  x3 = XEXP (x2, 0);
  if (register_operand (x3, SImode))
    {
      ro[1] = x3;
      goto L499;
    }
  x2 = XEXP (x1, 0);
  goto L1125;

  L499:
  x3 = XEXP (x2, 1);
  if (general_operand (x3, SImode))
    {
      ro[2] = x3;
      goto L500;
    }
  x2 = XEXP (x1, 0);
  goto L1125;

  L500:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == SET && 1)
    goto L501;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1125;

  L501:
  x2 = XEXP (x1, 0);
  if (register_operand (x2, SImode))
    {
      ro[3] = x2;
      goto L502;
    }
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1125;

  L502:
  x2 = XEXP (x1, 1);
  if (GET_MODE (x2) == SImode && GET_CODE (x2) == MOD && 1)
    goto L503;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1125;

  L503:
  x3 = XEXP (x2, 0);
  if (rtx_equal_p (x3, ro[1]) && 1)
    goto L504;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1125;

  L504:
  x3 = XEXP (x2, 1);
  if (rtx_equal_p (x3, ro[2]) && 1)
    return 105;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1125;

  L520:
  x3 = XEXP (x2, 0);
  if (register_operand (x3, SImode))
    {
      ro[1] = x3;
      goto L521;
    }
  x2 = XEXP (x1, 0);
  goto L1125;

  L521:
  x3 = XEXP (x2, 1);
  if (general_operand (x3, SImode))
    {
      ro[2] = x3;
      goto L522;
    }
  x2 = XEXP (x1, 0);
  goto L1125;

  L522:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == SET && 1)
    goto L523;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1125;

  L523:
  x2 = XEXP (x1, 0);
  if (register_operand (x2, SImode))
    {
      ro[3] = x2;
      goto L524;
    }
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1125;

  L524:
  x2 = XEXP (x1, 1);
  if (GET_MODE (x2) == SImode && GET_CODE (x2) == UMOD && 1)
    goto L525;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1125;

  L525:
  x3 = XEXP (x2, 0);
  if (rtx_equal_p (x3, ro[1]) && 1)
    goto L526;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1125;

  L526:
  x3 = XEXP (x2, 1);
  if (rtx_equal_p (x3, ro[2]) && 1)
    return 107;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1125;

  L508:
  x2 = XEXP (x1, 1);
  if (GET_MODE (x2) != HImode)
    {
      x2 = XEXP (x1, 0);
      goto L1125;
    }
  switch (GET_CODE (x2))
    {
    case DIV:
      goto L509;
    case UDIV:
      goto L531;
    }
  x2 = XEXP (x1, 0);
  goto L1125;

  L509:
  x3 = XEXP (x2, 0);
  if (register_operand (x3, HImode))
    {
      ro[1] = x3;
      goto L510;
    }
  x2 = XEXP (x1, 0);
  goto L1125;

  L510:
  x3 = XEXP (x2, 1);
  if (general_operand (x3, HImode))
    {
      ro[2] = x3;
      goto L511;
    }
  x2 = XEXP (x1, 0);
  goto L1125;

  L511:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == SET && 1)
    goto L512;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1125;

  L512:
  x2 = XEXP (x1, 0);
  if (register_operand (x2, HImode))
    {
      ro[3] = x2;
      goto L513;
    }
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1125;

  L513:
  x2 = XEXP (x1, 1);
  if (GET_MODE (x2) == HImode && GET_CODE (x2) == MOD && 1)
    goto L514;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1125;

  L514:
  x3 = XEXP (x2, 0);
  if (rtx_equal_p (x3, ro[1]) && 1)
    goto L515;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1125;

  L515:
  x3 = XEXP (x2, 1);
  if (rtx_equal_p (x3, ro[2]) && 1)
    return 106;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1125;

  L531:
  x3 = XEXP (x2, 0);
  if (register_operand (x3, HImode))
    {
      ro[1] = x3;
      goto L532;
    }
  x2 = XEXP (x1, 0);
  goto L1125;

  L532:
  x3 = XEXP (x2, 1);
  if (general_operand (x3, HImode))
    {
      ro[2] = x3;
      goto L533;
    }
  x2 = XEXP (x1, 0);
  goto L1125;

  L533:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == SET && 1)
    goto L534;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1125;

  L534:
  x2 = XEXP (x1, 0);
  if (register_operand (x2, HImode))
    {
      ro[3] = x2;
      goto L535;
    }
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1125;

  L535:
  x2 = XEXP (x1, 1);
  if (GET_MODE (x2) == HImode && GET_CODE (x2) == UMOD && 1)
    goto L536;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1125;

  L536:
  x3 = XEXP (x2, 0);
  if (rtx_equal_p (x3, ro[1]) && 1)
    goto L537;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1125;

  L537:
  x3 = XEXP (x2, 1);
  if (rtx_equal_p (x3, ro[2]) && 1)
    return 108;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1125;

  L684:
  x2 = XEXP (x1, 1);
  if (GET_MODE (x2) != DImode)
    {
      x2 = XEXP (x1, 0);
      goto L1125;
    }
  switch (GET_CODE (x2))
    {
    case ASHIFT:
      goto L685;
    case ASHIFTRT:
      goto L713;
    case LSHIFTRT:
      goto L741;
    }
  x2 = XEXP (x1, 0);
  goto L1125;

  L685:
  x3 = XEXP (x2, 0);
  if (register_operand (x3, DImode))
    {
      ro[1] = x3;
      goto L686;
    }
  x2 = XEXP (x1, 0);
  goto L1125;

  L686:
  x3 = XEXP (x2, 1);
  if (register_operand (x3, QImode))
    {
      ro[2] = x3;
      goto L687;
    }
  x2 = XEXP (x1, 0);
  goto L1125;

  L687:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L688;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1125;

  L688:
  x2 = XEXP (x1, 0);
  if (rtx_equal_p (x2, ro[2]) && 1)
    return 142;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1125;

  L713:
  x3 = XEXP (x2, 0);
  if (register_operand (x3, DImode))
    {
      ro[1] = x3;
      goto L714;
    }
  x2 = XEXP (x1, 0);
  goto L1125;

  L714:
  x3 = XEXP (x2, 1);
  if (register_operand (x3, QImode))
    {
      ro[2] = x3;
      goto L715;
    }
  x2 = XEXP (x1, 0);
  goto L1125;

  L715:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L716;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1125;

  L716:
  x2 = XEXP (x1, 0);
  if (rtx_equal_p (x2, ro[2]) && 1)
    return 148;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1125;

  L741:
  x3 = XEXP (x2, 0);
  if (register_operand (x3, DImode))
    {
      ro[1] = x3;
      goto L742;
    }
  x2 = XEXP (x1, 0);
  goto L1125;

  L742:
  x3 = XEXP (x2, 1);
  if (register_operand (x3, QImode))
    {
      ro[2] = x3;
      goto L743;
    }
  x2 = XEXP (x1, 0);
  goto L1125;

  L743:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L744;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1125;

  L744:
  x2 = XEXP (x1, 0);
  if (rtx_equal_p (x2, ro[2]) && 1)
    return 154;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1125;
 L12:
  tem = recog_4 (x0, insn, pnum_clobbers);
  if (tem >= 0) return tem;
  x2 = XEXP (x1, 0);
  goto L1125;

  L1068:
  x2 = XEXP (x1, 1);
  if (GET_MODE (x2) == SImode && GET_CODE (x2) == MINUS && 1)
    goto L1069;
  if (general_operand (x2, SImode))
    {
      ro[0] = x2;
      goto L1094;
    }
  x2 = XEXP (x1, 0);
  goto L1125;

  L1069:
  x3 = XEXP (x2, 0);
  if (GET_MODE (x3) == SImode && GET_CODE (x3) == REG && XINT (x3, 0) == 3 && 1)
    goto L1070;
  x2 = XEXP (x1, 0);
  goto L1125;

  L1070:
  x3 = XEXP (x2, 1);
  if (GET_MODE (x3) == SImode && GET_CODE (x3) == MEM && 1)
    goto L1071;
  x2 = XEXP (x1, 0);
  goto L1125;

  L1071:
  x4 = XEXP (x3, 0);
  if (GET_MODE (x4) == SImode && GET_CODE (x4) == PLUS && 1)
    goto L1072;
  x2 = XEXP (x1, 0);
  goto L1125;

  L1072:
  x5 = XEXP (x4, 0);
  if (GET_MODE (x5) == SImode && GET_CODE (x5) == MULT && 1)
    goto L1073;
  x2 = XEXP (x1, 0);
  goto L1125;

  L1073:
  x6 = XEXP (x5, 0);
  if (register_operand (x6, SImode))
    {
      ro[0] = x6;
      goto L1074;
    }
  x2 = XEXP (x1, 0);
  goto L1125;

  L1074:
  x6 = XEXP (x5, 1);
  if (GET_CODE (x6) == CONST_INT && XWINT (x6, 0) == 4 && 1)
    goto L1075;
  x2 = XEXP (x1, 0);
  goto L1125;

  L1075:
  x5 = XEXP (x4, 1);
  if (GET_CODE (x5) == LABEL_REF && 1)
    goto L1076;
  x2 = XEXP (x1, 0);
  goto L1125;

  L1076:
  x6 = XEXP (x5, 0);
  ro[1] = x6;
  goto L1077;

  L1077:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L1078;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1125;

  L1078:
  x2 = XEXP (x1, 0);
  if (scratch_operand (x2, SImode))
    {
      ro[2] = x2;
      return 223;
    }
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1125;

  L1094:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == USE && 1)
    goto L1095;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1125;

  L1095:
  x2 = XEXP (x1, 0);
  if (GET_CODE (x2) == LABEL_REF && 1)
    goto L1096;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1125;

  L1096:
  x3 = XEXP (x2, 0);
  ro[1] = x3;
  return 224;

  L1126:
  x2 = XEXP (x1, 1);
  if (GET_CODE (x2) == CALL && 1)
    goto L1138;
  x2 = XEXP (x1, 0);
  goto L1228;

  L1138:
  x3 = XEXP (x2, 0);
  if (GET_MODE (x3) == QImode && GET_CODE (x3) == MEM && 1)
    goto L1139;
  L1127:
  if (call_insn_operand (x3, QImode))
    {
      ro[1] = x3;
      goto L1128;
    }
  x2 = XEXP (x1, 0);
  goto L1228;

  L1139:
  x4 = XEXP (x3, 0);
  if (symbolic_operand (x4, SImode))
    {
      ro[1] = x4;
      goto L1140;
    }
  goto L1127;

  L1140:
  x3 = XEXP (x2, 1);
  if (general_operand (x3, SImode))
    {
      ro[2] = x3;
      goto L1141;
    }
  x3 = XEXP (x2, 0);
  goto L1127;

  L1141:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == SET && 1)
    goto L1142;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 1);
  x3 = XEXP (x2, 0);
  goto L1127;

  L1142:
  x2 = XEXP (x1, 0);
  if (GET_MODE (x2) == SImode && GET_CODE (x2) == REG && XINT (x2, 0) == 7 && 1)
    goto L1143;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 1);
  x3 = XEXP (x2, 0);
  goto L1127;

  L1143:
  x2 = XEXP (x1, 1);
  if (GET_MODE (x2) == SImode && GET_CODE (x2) == PLUS && 1)
    goto L1144;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 1);
  x3 = XEXP (x2, 0);
  goto L1127;

  L1144:
  x3 = XEXP (x2, 0);
  if (GET_MODE (x3) == SImode && GET_CODE (x3) == REG && XINT (x3, 0) == 7 && 1)
    goto L1145;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 1);
  x3 = XEXP (x2, 0);
  goto L1127;

  L1145:
  x3 = XEXP (x2, 1);
  if (immediate_operand (x3, SImode))
    {
      ro[4] = x3;
      if (!HALF_PIC_P ())
	return 233;
      }
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 1);
  x3 = XEXP (x2, 0);
  goto L1127;

  L1128:
  x3 = XEXP (x2, 1);
  if (general_operand (x3, SImode))
    {
      ro[2] = x3;
      goto L1129;
    }
  x2 = XEXP (x1, 0);
  goto L1228;

  L1129:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == SET && 1)
    goto L1130;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1228;

  L1130:
  x2 = XEXP (x1, 0);
  if (GET_MODE (x2) == SImode && GET_CODE (x2) == REG && XINT (x2, 0) == 7 && 1)
    goto L1131;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1228;

  L1131:
  x2 = XEXP (x1, 1);
  if (GET_MODE (x2) == SImode && GET_CODE (x2) == PLUS && 1)
    goto L1132;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1228;

  L1132:
  x3 = XEXP (x2, 0);
  if (GET_MODE (x3) == SImode && GET_CODE (x3) == REG && XINT (x3, 0) == 7 && 1)
    goto L1133;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1228;

  L1133:
  x3 = XEXP (x2, 1);
  if (immediate_operand (x3, SImode))
    {
      ro[4] = x3;
      return 232;
    }
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1228;

  L1229:
  x2 = XEXP (x1, 1);
  if (GET_MODE (x2) != SImode)
    goto ret0;
  switch (GET_CODE (x2))
    {
    case PLUS:
      goto L1230;
    case UNSPEC:
      if (XINT (x2, 1) == 0 && XVECLEN (x2, 0) == 3 && 1)
	goto L1306;
    }
  goto ret0;

  L1230:
  x3 = XEXP (x2, 0);
  if (GET_MODE (x3) == SImode && GET_CODE (x3) == FFS && 1)
    goto L1231;
  goto ret0;

  L1231:
  x4 = XEXP (x3, 0);
  if (general_operand (x4, SImode))
    {
      ro[1] = x4;
      goto L1232;
    }
  goto ret0;

  L1232:
  x3 = XEXP (x2, 1);
  if (GET_CODE (x3) == CONST_INT && XWINT (x3, 0) == -1 && 1)
    goto L1233;
  goto ret0;

  L1233:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L1234;
  goto ret0;

  L1234:
  x2 = XEXP (x1, 0);
  if (scratch_operand (x2, SImode))
    {
      ro[2] = x2;
      return 250;
    }
  goto ret0;

  L1306:
  x3 = XVECEXP (x2, 0, 0);
  if (GET_MODE (x3) == BLKmode && GET_CODE (x3) == MEM && 1)
    goto L1307;
  goto ret0;

  L1307:
  x4 = XEXP (x3, 0);
  if (address_operand (x4, SImode))
    {
      ro[1] = x4;
      goto L1308;
    }
  goto ret0;

  L1308:
  x3 = XVECEXP (x2, 0, 1);
  if (register_operand (x3, QImode))
    {
      ro[2] = x3;
      goto L1309;
    }
  goto ret0;

  L1309:
  x3 = XVECEXP (x2, 0, 2);
  if (immediate_operand (x3, SImode))
    {
      ro[3] = x3;
      goto L1310;
    }
  goto ret0;

  L1310:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L1311;
  goto ret0;

  L1311:
  x2 = XEXP (x1, 0);
  if (rtx_equal_p (x2, ro[1]) && 1)
    return 262;
  goto ret0;

  L1244:
  x2 = XEXP (x1, 1);
  if (GET_MODE (x2) == HImode && GET_CODE (x2) == PLUS && 1)
    goto L1245;
  goto ret0;

  L1245:
  x3 = XEXP (x2, 0);
  if (GET_MODE (x3) == HImode && GET_CODE (x3) == FFS && 1)
    goto L1246;
  goto ret0;

  L1246:
  x4 = XEXP (x3, 0);
  if (general_operand (x4, HImode))
    {
      ro[1] = x4;
      goto L1247;
    }
  goto ret0;

  L1247:
  x3 = XEXP (x2, 1);
  if (GET_CODE (x3) == CONST_INT && XWINT (x3, 0) == -1 && 1)
    goto L1248;
  goto ret0;

  L1248:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L1249;
  goto ret0;

  L1249:
  x2 = XEXP (x1, 0);
  if (scratch_operand (x2, HImode))
    {
      ro[2] = x2;
      return 252;
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

  L1170:
  switch (GET_CODE (x0))
    {
    case UNSPEC:
      if (GET_MODE (x0) == SImode && XINT (x0, 1) == 0 && XVECLEN (x0, 0) == 1 && 1)
	goto L1171;
      break;
    case SET:
      goto L200;
    case PARALLEL:
      if (XVECLEN (x0, 0) == 2 && 1)
	goto L10;
      if (XVECLEN (x0, 0) == 5 && 1)
	goto L299;
      if (XVECLEN (x0, 0) == 4 && 1)
	goto L313;
      if (XVECLEN (x0, 0) == 3 && 1)
	goto L363;
      if (XVECLEN (x0, 0) == 6 && 1)
	goto L1175;
      break;
    case CALL:
      goto L1117;
    case RETURN:
      if (simple_386_epilogue ())
	return 242;
      break;
    case CONST_INT:
      if (XWINT (x0, 0) == 0 && 1)
	return 243;
    }
  goto ret0;

  L1171:
  x1 = XVECEXP (x0, 0, 0);
  if (memory_operand (x1, SImode))
    {
      ro[0] = x1;
      return 241;
    }
  goto ret0;
 L200:
  return recog_3 (x0, insn, pnum_clobbers);

  L10:
  x1 = XVECEXP (x0, 0, 0);
  switch (GET_CODE (x1))
    {
    case SET:
      goto L241;
    case CALL:
      goto L1108;
    }
  goto ret0;
 L241:
  return recog_5 (x0, insn, pnum_clobbers);

  L1108:
  x2 = XEXP (x1, 0);
  if (GET_MODE (x2) == QImode && GET_CODE (x2) == MEM && 1)
    goto L1109;
  L1099:
  if (call_insn_operand (x2, QImode))
    {
      ro[0] = x2;
      goto L1100;
    }
  goto ret0;

  L1109:
  x3 = XEXP (x2, 0);
  if (symbolic_operand (x3, SImode))
    {
      ro[0] = x3;
      goto L1110;
    }
  goto L1099;

  L1110:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, SImode))
    {
      ro[1] = x2;
      goto L1111;
    }
  x2 = XEXP (x1, 0);
  goto L1099;

  L1111:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == SET && 1)
    goto L1112;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1099;

  L1112:
  x2 = XEXP (x1, 0);
  if (GET_MODE (x2) == SImode && GET_CODE (x2) == REG && XINT (x2, 0) == 7 && 1)
    goto L1113;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1099;

  L1113:
  x2 = XEXP (x1, 1);
  if (GET_MODE (x2) == SImode && GET_CODE (x2) == PLUS && 1)
    goto L1114;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1099;

  L1114:
  x3 = XEXP (x2, 0);
  if (GET_MODE (x3) == SImode && GET_CODE (x3) == REG && XINT (x3, 0) == 7 && 1)
    goto L1115;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1099;

  L1115:
  x3 = XEXP (x2, 1);
  if (immediate_operand (x3, SImode))
    {
      ro[3] = x3;
      if (!HALF_PIC_P ())
	return 227;
      }
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1099;

  L1100:
  x2 = XEXP (x1, 1);
  if (general_operand (x2, SImode))
    {
      ro[1] = x2;
      goto L1101;
    }
  goto ret0;

  L1101:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == SET && 1)
    goto L1102;
  goto ret0;

  L1102:
  x2 = XEXP (x1, 0);
  if (GET_MODE (x2) == SImode && GET_CODE (x2) == REG && XINT (x2, 0) == 7 && 1)
    goto L1103;
  goto ret0;

  L1103:
  x2 = XEXP (x1, 1);
  if (GET_MODE (x2) == SImode && GET_CODE (x2) == PLUS && 1)
    goto L1104;
  goto ret0;

  L1104:
  x3 = XEXP (x2, 0);
  if (GET_MODE (x3) == SImode && GET_CODE (x3) == REG && XINT (x3, 0) == 7 && 1)
    goto L1105;
  goto ret0;

  L1105:
  x3 = XEXP (x2, 1);
  if (immediate_operand (x3, SImode))
    {
      ro[3] = x3;
      return 226;
    }
  goto ret0;

  L299:
  x1 = XVECEXP (x0, 0, 0);
  if (GET_CODE (x1) == SET && 1)
    goto L300;
  goto ret0;

  L300:
  x2 = XEXP (x1, 0);
  if (GET_MODE (x2) == DImode && general_operand (x2, DImode))
    {
      ro[0] = x2;
      goto L301;
    }
  goto ret0;

  L301:
  x2 = XEXP (x1, 1);
  if (GET_MODE (x2) == DImode && GET_CODE (x2) == FIX && 1)
    goto L302;
  goto ret0;

  L302:
  x3 = XEXP (x2, 0);
  if (GET_CODE (x3) != FIX)
    goto ret0;
  switch (GET_MODE (x3))
    {
    case DFmode:
      goto L303;
    case SFmode:
      goto L329;
    }
  goto ret0;

  L303:
  x4 = XEXP (x3, 0);
  if (register_operand (x4, DFmode))
    {
      ro[1] = x4;
      goto L304;
    }
  goto ret0;

  L304:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L305;
  goto ret0;

  L305:
  x2 = XEXP (x1, 0);
  if (rtx_equal_p (x2, ro[1]) && 1)
    goto L306;
  goto ret0;

  L306:
  x1 = XVECEXP (x0, 0, 2);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L307;
  goto ret0;

  L307:
  x2 = XEXP (x1, 0);
  if (memory_operand (x2, SImode))
    {
      ro[2] = x2;
      goto L308;
    }
  goto ret0;

  L308:
  x1 = XVECEXP (x0, 0, 3);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L309;
  goto ret0;

  L309:
  x2 = XEXP (x1, 0);
  if (memory_operand (x2, SImode))
    {
      ro[3] = x2;
      goto L310;
    }
  goto ret0;

  L310:
  x1 = XVECEXP (x0, 0, 4);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L311;
  goto ret0;

  L311:
  x2 = XEXP (x1, 0);
  if (scratch_operand (x2, SImode))
    {
      ro[4] = x2;
      if (TARGET_80387)
	return 67;
      }
  goto ret0;

  L329:
  x4 = XEXP (x3, 0);
  if (register_operand (x4, SFmode))
    {
      ro[1] = x4;
      goto L330;
    }
  goto ret0;

  L330:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L331;
  goto ret0;

  L331:
  x2 = XEXP (x1, 0);
  if (rtx_equal_p (x2, ro[1]) && 1)
    goto L332;
  goto ret0;

  L332:
  x1 = XVECEXP (x0, 0, 2);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L333;
  goto ret0;

  L333:
  x2 = XEXP (x1, 0);
  if (memory_operand (x2, SImode))
    {
      ro[2] = x2;
      goto L334;
    }
  goto ret0;

  L334:
  x1 = XVECEXP (x0, 0, 3);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L335;
  goto ret0;

  L335:
  x2 = XEXP (x1, 0);
  if (memory_operand (x2, SImode))
    {
      ro[3] = x2;
      goto L336;
    }
  goto ret0;

  L336:
  x1 = XVECEXP (x0, 0, 4);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L337;
  goto ret0;

  L337:
  x2 = XEXP (x1, 0);
  if (scratch_operand (x2, SImode))
    {
      ro[4] = x2;
      if (TARGET_80387)
	return 68;
      }
  goto ret0;

  L313:
  x1 = XVECEXP (x0, 0, 0);
  if (GET_CODE (x1) == SET && 1)
    goto L314;
  goto ret0;

  L314:
  x2 = XEXP (x1, 0);
  switch (GET_MODE (x2))
    {
    case DImode:
      if (general_operand (x2, DImode))
	{
	  ro[0] = x2;
	  goto L315;
	}
      break;
    case SImode:
      if (general_operand (x2, SImode))
	{
	  ro[0] = x2;
	  goto L353;
	}
    }
  goto ret0;

  L315:
  x2 = XEXP (x1, 1);
  if (GET_MODE (x2) == DImode && GET_CODE (x2) == FIX && 1)
    goto L316;
  goto ret0;

  L316:
  x3 = XEXP (x2, 0);
  if (GET_CODE (x3) != FIX)
    goto ret0;
  switch (GET_MODE (x3))
    {
    case DFmode:
      goto L317;
    case SFmode:
      goto L343;
    }
  goto ret0;

  L317:
  x4 = XEXP (x3, 0);
  if (register_operand (x4, DFmode))
    {
      ro[1] = x4;
      goto L318;
    }
  goto ret0;

  L318:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L319;
  goto ret0;

  L319:
  x2 = XEXP (x1, 0);
  if (rtx_equal_p (x2, ro[1]) && 1)
    goto L320;
  goto ret0;

  L320:
  x1 = XVECEXP (x0, 0, 2);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L321;
  goto ret0;

  L321:
  x2 = XEXP (x1, 0);
  if (memory_operand (x2, SImode))
    {
      ro[2] = x2;
      goto L322;
    }
  goto ret0;

  L322:
  x1 = XVECEXP (x0, 0, 3);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L323;
  goto ret0;

  L323:
  x2 = XEXP (x1, 0);
  if (pnum_clobbers != 0 && memory_operand (x2, SImode))
    {
      ro[3] = x2;
      if (TARGET_80387)
	{
	  *pnum_clobbers = 1;
	  return 67;
	}
      }
  goto ret0;

  L343:
  x4 = XEXP (x3, 0);
  if (register_operand (x4, SFmode))
    {
      ro[1] = x4;
      goto L344;
    }
  goto ret0;

  L344:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L345;
  goto ret0;

  L345:
  x2 = XEXP (x1, 0);
  if (rtx_equal_p (x2, ro[1]) && 1)
    goto L346;
  goto ret0;

  L346:
  x1 = XVECEXP (x0, 0, 2);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L347;
  goto ret0;

  L347:
  x2 = XEXP (x1, 0);
  if (memory_operand (x2, SImode))
    {
      ro[2] = x2;
      goto L348;
    }
  goto ret0;

  L348:
  x1 = XVECEXP (x0, 0, 3);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L349;
  goto ret0;

  L349:
  x2 = XEXP (x1, 0);
  if (pnum_clobbers != 0 && memory_operand (x2, SImode))
    {
      ro[3] = x2;
      if (TARGET_80387)
	{
	  *pnum_clobbers = 1;
	  return 68;
	}
      }
  goto ret0;

  L353:
  x2 = XEXP (x1, 1);
  if (GET_MODE (x2) == SImode && GET_CODE (x2) == FIX && 1)
    goto L354;
  goto ret0;

  L354:
  x3 = XEXP (x2, 0);
  if (GET_CODE (x3) != FIX)
    goto ret0;
  switch (GET_MODE (x3))
    {
    case DFmode:
      goto L355;
    case SFmode:
      goto L377;
    }
  goto ret0;

  L355:
  x4 = XEXP (x3, 0);
  if (register_operand (x4, DFmode))
    {
      ro[1] = x4;
      goto L356;
    }
  goto ret0;

  L356:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L357;
  goto ret0;

  L357:
  x2 = XEXP (x1, 0);
  if (memory_operand (x2, SImode))
    {
      ro[2] = x2;
      goto L358;
    }
  goto ret0;

  L358:
  x1 = XVECEXP (x0, 0, 2);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L359;
  goto ret0;

  L359:
  x2 = XEXP (x1, 0);
  if (memory_operand (x2, SImode))
    {
      ro[3] = x2;
      goto L360;
    }
  goto ret0;

  L360:
  x1 = XVECEXP (x0, 0, 3);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L361;
  goto ret0;

  L361:
  x2 = XEXP (x1, 0);
  if (scratch_operand (x2, SImode))
    {
      ro[4] = x2;
      if (TARGET_80387)
	return 71;
      }
  goto ret0;

  L377:
  x4 = XEXP (x3, 0);
  if (register_operand (x4, SFmode))
    {
      ro[1] = x4;
      goto L378;
    }
  goto ret0;

  L378:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L379;
  goto ret0;

  L379:
  x2 = XEXP (x1, 0);
  if (memory_operand (x2, SImode))
    {
      ro[2] = x2;
      goto L380;
    }
  goto ret0;

  L380:
  x1 = XVECEXP (x0, 0, 2);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L381;
  goto ret0;

  L381:
  x2 = XEXP (x1, 0);
  if (memory_operand (x2, SImode))
    {
      ro[3] = x2;
      goto L382;
    }
  goto ret0;

  L382:
  x1 = XVECEXP (x0, 0, 3);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L383;
  goto ret0;

  L383:
  x2 = XEXP (x1, 0);
  if (scratch_operand (x2, SImode))
    {
      ro[4] = x2;
      if (TARGET_80387)
	return 72;
      }
  goto ret0;

  L363:
  x1 = XVECEXP (x0, 0, 0);
  switch (GET_CODE (x1))
    {
    case SET:
      goto L364;
    case CALL:
      goto L1165;
    }
  goto ret0;

  L364:
  x2 = XEXP (x1, 0);
  if (GET_MODE (x2) == SImode && general_operand (x2, SImode))
    {
      ro[0] = x2;
      goto L365;
    }
  goto ret0;

  L365:
  x2 = XEXP (x1, 1);
  if (GET_MODE (x2) == SImode && GET_CODE (x2) == FIX && 1)
    goto L366;
  goto ret0;

  L366:
  x3 = XEXP (x2, 0);
  if (GET_CODE (x3) != FIX)
    goto ret0;
  switch (GET_MODE (x3))
    {
    case DFmode:
      goto L367;
    case SFmode:
      goto L389;
    }
  goto ret0;

  L367:
  x4 = XEXP (x3, 0);
  if (register_operand (x4, DFmode))
    {
      ro[1] = x4;
      goto L368;
    }
  goto ret0;

  L368:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L369;
  goto ret0;

  L369:
  x2 = XEXP (x1, 0);
  if (memory_operand (x2, SImode))
    {
      ro[2] = x2;
      goto L370;
    }
  goto ret0;

  L370:
  x1 = XVECEXP (x0, 0, 2);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L371;
  goto ret0;

  L371:
  x2 = XEXP (x1, 0);
  if (pnum_clobbers != 0 && memory_operand (x2, SImode))
    {
      ro[3] = x2;
      if (TARGET_80387)
	{
	  *pnum_clobbers = 1;
	  return 71;
	}
      }
  goto ret0;

  L389:
  x4 = XEXP (x3, 0);
  if (register_operand (x4, SFmode))
    {
      ro[1] = x4;
      goto L390;
    }
  goto ret0;

  L390:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L391;
  goto ret0;

  L391:
  x2 = XEXP (x1, 0);
  if (memory_operand (x2, SImode))
    {
      ro[2] = x2;
      goto L392;
    }
  goto ret0;

  L392:
  x1 = XVECEXP (x0, 0, 2);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L393;
  goto ret0;

  L393:
  x2 = XEXP (x1, 0);
  if (pnum_clobbers != 0 && memory_operand (x2, SImode))
    {
      ro[3] = x2;
      if (TARGET_80387)
	{
	  *pnum_clobbers = 1;
	  return 72;
	}
      }
  goto ret0;

  L1165:
  x2 = XEXP (x1, 0);
  if (GET_MODE (x2) == QImode && GET_CODE (x2) == MEM && 1)
    goto L1166;
  L1159:
  if (call_insn_operand (x2, QImode))
    {
      ro[0] = x2;
      goto L1160;
    }
  goto ret0;

  L1166:
  x3 = XEXP (x2, 0);
  if (symbolic_operand (x3, SImode))
    {
      ro[0] = x3;
      goto L1167;
    }
  goto L1159;

  L1167:
  x2 = XEXP (x1, 1);
  if (GET_CODE (x2) == CONST_INT && XWINT (x2, 0) == 0 && 1)
    goto L1168;
  x2 = XEXP (x1, 0);
  goto L1159;

  L1168:
  x1 = XVECEXP (x0, 0, 1);
  if (memory_operand (x1, DImode))
    {
      ro[1] = x1;
      goto L1169;
    }
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1159;

  L1169:
  x1 = XVECEXP (x0, 0, 2);
  ro[2] = x1;
  if (!HALF_PIC_P ())
    return 239;
  x1 = XVECEXP (x0, 0, 0);
  x2 = XEXP (x1, 0);
  goto L1159;

  L1160:
  x2 = XEXP (x1, 1);
  if (GET_CODE (x2) == CONST_INT && XWINT (x2, 0) == 0 && 1)
    goto L1161;
  goto ret0;

  L1161:
  x1 = XVECEXP (x0, 0, 1);
  if (memory_operand (x1, DImode))
    {
      ro[1] = x1;
      goto L1162;
    }
  goto ret0;

  L1162:
  x1 = XVECEXP (x0, 0, 2);
  ro[2] = x1;
  return 238;

  L1175:
  x1 = XVECEXP (x0, 0, 0);
  if (GET_CODE (x1) == SET && 1)
    goto L1176;
  goto ret0;

  L1176:
  x2 = XEXP (x1, 0);
  switch (GET_MODE (x2))
    {
    case BLKmode:
      if (GET_CODE (x2) == MEM && 1)
	goto L1177;
      break;
    case SImode:
      if (general_operand (x2, SImode))
	{
	  ro[0] = x2;
	  goto L1193;
	}
    }
  if (GET_CODE (x2) == CC0 && 1)
    goto L1211;
  goto ret0;

  L1177:
  x3 = XEXP (x2, 0);
  if (address_operand (x3, SImode))
    {
      ro[0] = x3;
      goto L1178;
    }
  goto ret0;

  L1178:
  x2 = XEXP (x1, 1);
  if (GET_MODE (x2) == BLKmode && GET_CODE (x2) == MEM && 1)
    goto L1179;
  goto ret0;

  L1179:
  x3 = XEXP (x2, 0);
  if (address_operand (x3, SImode))
    {
      ro[1] = x3;
      goto L1180;
    }
  goto ret0;

  L1180:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == USE && 1)
    goto L1181;
  goto ret0;

  L1181:
  x2 = XEXP (x1, 0);
  if (GET_CODE (x2) == CONST_INT && 1)
    {
      ro[2] = x2;
      goto L1182;
    }
  goto ret0;

  L1182:
  x1 = XVECEXP (x0, 0, 2);
  if (GET_CODE (x1) == USE && 1)
    goto L1183;
  goto ret0;

  L1183:
  x2 = XEXP (x1, 0);
  if (immediate_operand (x2, SImode))
    {
      ro[3] = x2;
      goto L1184;
    }
  goto ret0;

  L1184:
  x1 = XVECEXP (x0, 0, 3);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L1185;
  goto ret0;

  L1185:
  x2 = XEXP (x1, 0);
  if (scratch_operand (x2, SImode))
    {
      ro[4] = x2;
      goto L1186;
    }
  goto ret0;

  L1186:
  x1 = XVECEXP (x0, 0, 4);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L1187;
  goto ret0;

  L1187:
  x2 = XEXP (x1, 0);
  if (rtx_equal_p (x2, ro[0]) && 1)
    goto L1188;
  goto ret0;

  L1188:
  x1 = XVECEXP (x0, 0, 5);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L1189;
  goto ret0;

  L1189:
  x2 = XEXP (x1, 0);
  if (rtx_equal_p (x2, ro[1]) && 1)
    return 245;
  goto ret0;

  L1193:
  x2 = XEXP (x1, 1);
  if (GET_MODE (x2) == SImode && GET_CODE (x2) == COMPARE && 1)
    goto L1194;
  goto ret0;

  L1194:
  x3 = XEXP (x2, 0);
  if (GET_MODE (x3) == BLKmode && GET_CODE (x3) == MEM && 1)
    goto L1195;
  goto ret0;

  L1195:
  x4 = XEXP (x3, 0);
  if (address_operand (x4, SImode))
    {
      ro[1] = x4;
      goto L1196;
    }
  goto ret0;

  L1196:
  x3 = XEXP (x2, 1);
  if (GET_MODE (x3) == BLKmode && GET_CODE (x3) == MEM && 1)
    goto L1197;
  goto ret0;

  L1197:
  x4 = XEXP (x3, 0);
  if (address_operand (x4, SImode))
    {
      ro[2] = x4;
      goto L1198;
    }
  goto ret0;

  L1198:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == USE && 1)
    goto L1199;
  goto ret0;

  L1199:
  x2 = XEXP (x1, 0);
  if (register_operand (x2, SImode))
    {
      ro[3] = x2;
      goto L1200;
    }
  goto ret0;

  L1200:
  x1 = XVECEXP (x0, 0, 2);
  if (GET_CODE (x1) == USE && 1)
    goto L1201;
  goto ret0;

  L1201:
  x2 = XEXP (x1, 0);
  if (immediate_operand (x2, SImode))
    {
      ro[4] = x2;
      goto L1202;
    }
  goto ret0;

  L1202:
  x1 = XVECEXP (x0, 0, 3);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L1203;
  goto ret0;

  L1203:
  x2 = XEXP (x1, 0);
  if (rtx_equal_p (x2, ro[1]) && 1)
    goto L1204;
  goto ret0;

  L1204:
  x1 = XVECEXP (x0, 0, 4);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L1205;
  goto ret0;

  L1205:
  x2 = XEXP (x1, 0);
  if (rtx_equal_p (x2, ro[2]) && 1)
    goto L1206;
  goto ret0;

  L1206:
  x1 = XVECEXP (x0, 0, 5);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L1207;
  goto ret0;

  L1207:
  x2 = XEXP (x1, 0);
  if (rtx_equal_p (x2, ro[3]) && 1)
    return 247;
  goto ret0;

  L1211:
  x2 = XEXP (x1, 1);
  if (GET_MODE (x2) == SImode && GET_CODE (x2) == COMPARE && 1)
    goto L1212;
  goto ret0;

  L1212:
  x3 = XEXP (x2, 0);
  if (GET_MODE (x3) == BLKmode && GET_CODE (x3) == MEM && 1)
    goto L1213;
  goto ret0;

  L1213:
  x4 = XEXP (x3, 0);
  if (address_operand (x4, SImode))
    {
      ro[0] = x4;
      goto L1214;
    }
  goto ret0;

  L1214:
  x3 = XEXP (x2, 1);
  if (GET_MODE (x3) == BLKmode && GET_CODE (x3) == MEM && 1)
    goto L1215;
  goto ret0;

  L1215:
  x4 = XEXP (x3, 0);
  if (address_operand (x4, SImode))
    {
      ro[1] = x4;
      goto L1216;
    }
  goto ret0;

  L1216:
  x1 = XVECEXP (x0, 0, 1);
  if (GET_CODE (x1) == USE && 1)
    goto L1217;
  goto ret0;

  L1217:
  x2 = XEXP (x1, 0);
  if (register_operand (x2, SImode))
    {
      ro[2] = x2;
      goto L1218;
    }
  goto ret0;

  L1218:
  x1 = XVECEXP (x0, 0, 2);
  if (GET_CODE (x1) == USE && 1)
    goto L1219;
  goto ret0;

  L1219:
  x2 = XEXP (x1, 0);
  if (immediate_operand (x2, SImode))
    {
      ro[3] = x2;
      goto L1220;
    }
  goto ret0;

  L1220:
  x1 = XVECEXP (x0, 0, 3);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L1221;
  goto ret0;

  L1221:
  x2 = XEXP (x1, 0);
  if (rtx_equal_p (x2, ro[0]) && 1)
    goto L1222;
  goto ret0;

  L1222:
  x1 = XVECEXP (x0, 0, 4);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L1223;
  goto ret0;

  L1223:
  x2 = XEXP (x1, 0);
  if (rtx_equal_p (x2, ro[1]) && 1)
    goto L1224;
  goto ret0;

  L1224:
  x1 = XVECEXP (x0, 0, 5);
  if (GET_CODE (x1) == CLOBBER && 1)
    goto L1225;
  goto ret0;

  L1225:
  x2 = XEXP (x1, 0);
  if (rtx_equal_p (x2, ro[2]) && 1)
    return 248;
  goto ret0;

  L1117:
  x1 = XEXP (x0, 0);
  if (call_insn_operand (x1, QImode))
    {
      ro[0] = x1;
      goto L1118;
    }
  L1120:
  if (GET_MODE (x1) == QImode && GET_CODE (x1) == MEM && 1)
    goto L1121;
  goto ret0;

  L1118:
  x1 = XEXP (x0, 1);
  if (general_operand (x1, SImode))
    {
      ro[1] = x1;
      return 229;
    }
  x1 = XEXP (x0, 0);
  goto L1120;

  L1121:
  x2 = XEXP (x1, 0);
  if (symbolic_operand (x2, SImode))
    {
      ro[0] = x2;
      goto L1122;
    }
  goto ret0;

  L1122:
  x1 = XEXP (x0, 1);
  if (general_operand (x1, SImode))
    {
      ro[1] = x1;
      if (!HALF_PIC_P ())
	return 230;
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

