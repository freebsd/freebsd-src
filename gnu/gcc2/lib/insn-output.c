/* Generated automatically by the program `genoutput'
from the machine description file `md'.  */

#include "config.h"
#include "rtl.h"
#include "regs.h"
#include "hard-reg-set.h"
#include "real.h"
#include "insn-config.h"

#include "conditions.h"
#include "insn-flags.h"
#include "insn-attr.h"

#include "insn-codes.h"

#include "recog.h"

#include <stdio.h>
#include "output.h"

static char *
output_0 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (REG_P (operands[0]))
    return AS2 (test%L0,%0,%0);

  operands[1] = const0_rtx;
  return AS2 (cmp%L0,%1,%0);
}
}

static char *
output_2 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (REG_P (operands[0]))
    return AS2 (test%W0,%0,%0);

  operands[1] = const0_rtx;
  return AS2 (cmp%W0,%1,%0);
}
}

static char *
output_4 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (REG_P (operands[0]))
    return AS2 (test%B0,%0,%0);

  operands[1] = const0_rtx;
  return AS2 (cmp%B0,%1,%0);
}
}

static char *
output_6 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (! STACK_TOP_P (operands[0]))
    abort ();

  output_asm_insn ("ftst", operands);

  if (find_regno_note (insn, REG_DEAD, FIRST_STACK_REG))
    output_asm_insn (AS1 (fstp,%y0), operands);

  return (char *) output_fp_cc0_set (insn);
}
}

static char *
output_8 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (! STACK_TOP_P (operands[0]))
    abort ();

  output_asm_insn ("ftst", operands);

  if (find_regno_note (insn, REG_DEAD, FIRST_STACK_REG))
    output_asm_insn (AS1 (fstp,%y0), operands);

  return (char *) output_fp_cc0_set (insn);
}
}

static char *
output_10 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (CONSTANT_P (operands[0]) || GET_CODE (operands[1]) == MEM)
    {
      cc_status.flags |= CC_REVERSED;
      return AS2 (cmp%L0,%0,%1);
    }
  return AS2 (cmp%L0,%1,%0);
}
}

static char *
output_12 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (CONSTANT_P (operands[0]) || GET_CODE (operands[1]) == MEM)
    {
      cc_status.flags |= CC_REVERSED;
      return AS2 (cmp%W0,%0,%1);
    }
  return AS2 (cmp%W0,%1,%0);
}
}

static char *
output_14 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (CONSTANT_P (operands[0]) || GET_CODE (operands[1]) == MEM)
    {
      cc_status.flags |= CC_REVERSED;
      return AS2 (cmp%B0,%0,%1);
    }
  return AS2 (cmp%B0,%1,%0);
}
}

static char *
output_16 (operands, insn)
     rtx *operands;
     rtx insn;
{
 return (char *) output_float_compare (insn, operands);
}

static char *
output_17 (operands, insn)
     rtx *operands;
     rtx insn;
{
 return (char *) output_float_compare (insn, operands);
}

static char *
output_18 (operands, insn)
     rtx *operands;
     rtx insn;
{
 return (char *) output_float_compare (insn, operands);
}

static char *
output_19 (operands, insn)
     rtx *operands;
     rtx insn;
{
 return (char *) output_float_compare (insn, operands);
}

static char *
output_20 (operands, insn)
     rtx *operands;
     rtx insn;
{
 return (char *) output_float_compare (insn, operands);
}

static char *
output_21 (operands, insn)
     rtx *operands;
     rtx insn;
{
 return (char *) output_float_compare (insn, operands);
}

static char *
output_22 (operands, insn)
     rtx *operands;
     rtx insn;
{
 return (char *) output_float_compare (insn, operands);
}

static char *
output_23 (operands, insn)
     rtx *operands;
     rtx insn;
{
 return (char *) output_float_compare (insn, operands);
}

static char *
output_24 (operands, insn)
     rtx *operands;
     rtx insn;
{
 return (char *) output_float_compare (insn, operands);
}

static char *
output_25 (operands, insn)
     rtx *operands;
     rtx insn;
{
 return (char *) output_float_compare (insn, operands);
}

static char *
output_32 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  /* For small integers, we may actually use testb. */
  if (GET_CODE (operands[1]) == CONST_INT
      && ! (GET_CODE (operands[0]) == MEM && MEM_VOLATILE_P (operands[0]))
      && (! REG_P (operands[0]) || QI_REG_P (operands[0])))
    {
      /* We may set the sign bit spuriously.  */

      if ((INTVAL (operands[1]) & ~0xff) == 0)
        {
	  cc_status.flags |= CC_NOT_NEGATIVE;
	  return AS2 (test%B0,%1,%b0);
	}

      if ((INTVAL (operands[1]) & ~0xff00) == 0)
        {
	  cc_status.flags |= CC_NOT_NEGATIVE;
	  operands[1] = GEN_INT (INTVAL (operands[1]) >> 8);

	  if (QI_REG_P (operands[0]))
	    return AS2 (test%B0,%1,%h0);
	  else
	    {
	      operands[0] = adj_offsettable_operand (operands[0], 1);
	      return AS2 (test%B0,%1,%b0);
	    }
	}

      if (GET_CODE (operands[0]) == MEM
	  && (INTVAL (operands[1]) & ~0xff0000) == 0)
        {
	  cc_status.flags |= CC_NOT_NEGATIVE;
	  operands[1] = GEN_INT (INTVAL (operands[1]) >> 16);
	  operands[0] = adj_offsettable_operand (operands[0], 2);
	  return AS2 (test%B0,%1,%b0);
	}

      if (GET_CODE (operands[0]) == MEM
	  && (INTVAL (operands[1]) & ~0xff000000) == 0)
        {
	  operands[1] = GEN_INT ((INTVAL (operands[1]) >> 24) & 0xff);
	  operands[0] = adj_offsettable_operand (operands[0], 3);
	  return AS2 (test%B0,%1,%b0);
	}
    }

  if (CONSTANT_P (operands[1]) || GET_CODE (operands[0]) == MEM)
    return AS2 (test%L0,%1,%0);

  return AS2 (test%L1,%0,%1);
}
}

static char *
output_33 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (GET_CODE (operands[1]) == CONST_INT
      && ! (GET_CODE (operands[0]) == MEM && MEM_VOLATILE_P (operands[0]))
      && (! REG_P (operands[0]) || QI_REG_P (operands[0])))
    {
      if ((INTVAL (operands[1]) & 0xff00) == 0)
	{
	  /* ??? This might not be necessary. */
	  if (INTVAL (operands[1]) & 0xffff0000)
	    operands[1] = GEN_INT (INTVAL (operands[1]) & 0xff);

	  /* We may set the sign bit spuriously.  */
	  cc_status.flags |= CC_NOT_NEGATIVE;
	  return AS2 (test%B0,%1,%b0);
	}

      if ((INTVAL (operands[1]) & 0xff) == 0)
        {
	  operands[1] = GEN_INT ((INTVAL (operands[1]) >> 8) & 0xff);

	  if (QI_REG_P (operands[0]))
	    return AS2 (test%B0,%1,%h0);
	  else
	    {
	      operands[0] = adj_offsettable_operand (operands[0], 1);
	      return AS2 (test%B0,%1,%b0);
	    }
	}
    }

  if (CONSTANT_P (operands[1]) || GET_CODE (operands[0]) == MEM)
    return AS2 (test%W0,%1,%0);

  return AS2 (test%W1,%0,%1);
}
}

static char *
output_34 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (CONSTANT_P (operands[1]) || GET_CODE (operands[0]) == MEM)
    return AS2 (test%B0,%1,%0);

  return AS2 (test%B1,%0,%1);
}
}

static char *
output_38 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  rtx link;
  if (operands[1] == const0_rtx && REG_P (operands[0]))
    return AS2 (xor%L0,%0,%0);

  if (operands[1] == const1_rtx
      && (link = find_reg_note (insn, REG_WAS_0, 0))
      /* Make sure the insn that stored the 0 is still present.  */
      && ! INSN_DELETED_P (XEXP (link, 0))
      && GET_CODE (XEXP (link, 0)) != NOTE
      /* Make sure cross jumping didn't happen here.  */
      && no_labels_between_p (XEXP (link, 0), insn)
      /* Make sure the reg hasn't been clobbered.  */
      && ! reg_set_between_p (operands[0], XEXP (link, 0), insn))
    /* Fastest way to change a 0 to a 1.  */
    return AS1 (inc%L0,%0);

  return AS2 (mov%L0,%1,%0);
}
}

static char *
output_40 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  rtx link;
  if (REG_P (operands[0]) && operands[1] == const0_rtx)
    return AS2 (xor%L0,%k0,%k0);

  if (REG_P (operands[0]) && operands[1] == const1_rtx 
      && (link = find_reg_note (insn, REG_WAS_0, 0))
      /* Make sure the insn that stored the 0 is still present.  */
      && ! INSN_DELETED_P (XEXP (link, 0))
      && GET_CODE (XEXP (link, 0)) != NOTE
      /* Make sure cross jumping didn't happen here.  */
      && no_labels_between_p (XEXP (link, 0), insn)
      /* Make sure the reg hasn't been clobbered.  */
      && ! reg_set_between_p (operands[0], XEXP (link, 0), insn))
    /* Fastest way to change a 0 to a 1.  */
    return AS1 (inc%L0,%k0);

  if (REG_P (operands[0]))
    {
      if (REG_P (operands[1]))
	return AS2 (mov%L0,%k1,%k0);
      else if (CONSTANT_P (operands[1]))
	return AS2 (mov%L0,%1,%k0);
    }

  return AS2 (mov%W0,%1,%0);
}
}

static char *
output_41 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  rtx link;
  if (operands[1] == const0_rtx && REG_P (operands[0]))
    return AS2 (xor%W0,%0,%0);

  if (operands[1] == const1_rtx
      && (link = find_reg_note (insn, REG_WAS_0, 0))
      /* Make sure the insn that stored the 0 is still present.  */
      && ! INSN_DELETED_P (XEXP (link, 0))
      && GET_CODE (XEXP (link, 0)) != NOTE
      /* Make sure cross jumping didn't happen here.  */
      && no_labels_between_p (XEXP (link, 0), insn)
      /* Make sure the reg hasn't been clobbered.  */
      && ! reg_set_between_p (operands[0], XEXP (link, 0), insn))
    /* Fastest way to change a 0 to a 1.  */
    return AS1 (inc%W0,%0);

  return AS2 (mov%W0,%1,%0);
}
}

static char *
output_42 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  operands[1] = gen_rtx (REG, HImode, REGNO (operands[1]));
  return AS1 (push%W0,%1);
}
}

static char *
output_43 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  rtx link;
  if (operands[1] == const0_rtx && REG_P (operands[0]))
    return AS2 (xor%B0,%0,%0);

  if (operands[1] == const1_rtx
      && (link = find_reg_note (insn, REG_WAS_0, 0))
      /* Make sure the insn that stored the 0 is still present.  */
      && ! INSN_DELETED_P (XEXP (link, 0))
      && GET_CODE (XEXP (link, 0)) != NOTE
      /* Make sure cross jumping didn't happen here.  */
      && no_labels_between_p (XEXP (link, 0), insn)
      /* Make sure the reg hasn't been clobbered.  */
      && ! reg_set_between_p (operands[0], XEXP (link, 0), insn))
    /* Fastest way to change a 0 to a 1.  */
    return AS1 (inc%B0,%0);

  /* If mov%B0 isn't allowed for one of these regs, use mov%L0.  */
  if (NON_QI_REG_P (operands[0]) || NON_QI_REG_P (operands[1]))
    return (AS2 (mov%L0,%k1,%k0));

  return (AS2 (mov%B0,%1,%0));
}
}

static char *
output_44 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  rtx link;
  if (operands[1] == const0_rtx && REG_P (operands[0]))
    return AS2 (xor%B0,%0,%0);

  if (operands[1] == const1_rtx
      && (link = find_reg_note (insn, REG_WAS_0, 0))
      /* Make sure the insn that stored the 0 is still present.  */
      && ! INSN_DELETED_P (XEXP (link, 0))
      && GET_CODE (XEXP (link, 0)) != NOTE
      /* Make sure cross jumping didn't happen here.  */
      && no_labels_between_p (XEXP (link, 0), insn)
      /* Make sure the reg hasn't been clobbered.  */
      && ! reg_set_between_p (operands[0], XEXP (link, 0), insn))
    /* Fastest way to change a 0 to a 1.  */
    return AS1 (inc%B0,%0);

  /* If mov%B0 isn't allowed for one of these regs, use mov%W0.  */
  if (NON_QI_REG_P (operands[0]) || NON_QI_REG_P (operands[1]))
    {
      abort ();
      return (AS2 (mov%L0,%k1,%k0));
    }

  return AS2 (mov%B0,%1,%0);
}
}

static char *
output_45 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (STACK_REG_P (operands[1]))
    {
      rtx xops[3];

      if (! STACK_TOP_P (operands[1]))
        abort ();

      xops[0] = AT_SP (SFmode);
      xops[1] = GEN_INT (4);
      xops[2] = stack_pointer_rtx;

      output_asm_insn (AS2 (sub%L2,%1,%2), xops);

      if (find_regno_note (insn, REG_DEAD, FIRST_STACK_REG))
        output_asm_insn (AS1 (fstp%S0,%0), xops);
      else
        output_asm_insn (AS1 (fst%S0,%0), xops);
      RET;
    }
  return AS1 (push%L1,%1);
}
}

static char *
output_46 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  int stack_top_dies = find_regno_note (insn, REG_DEAD, FIRST_STACK_REG) != 0;

  /* First handle a `pop' insn or a `fld %st(0)' */

  if (STACK_TOP_P (operands[0]) && STACK_TOP_P (operands[1]))
    {
      if (stack_top_dies)
	return AS1 (fstp,%y0);
      else
        return AS1 (fld,%y0);
    }

  /* Handle a transfer between the 387 and a 386 register */

  if (STACK_TOP_P (operands[0]) && NON_STACK_REG_P (operands[1]))
    {
      output_op_from_reg (operands[1], AS1 (fld%z0,%y1));
      RET;
    }

  if (STACK_TOP_P (operands[1]) && NON_STACK_REG_P (operands[0]))
    {
      output_to_reg (operands[0], stack_top_dies);
      RET;
    }

  /* Handle other kinds of writes from the 387 */

  if (STACK_TOP_P (operands[1]))
    {
      if (stack_top_dies)
	return AS1 (fstp%z0,%y0);
      else
        return AS1 (fst%z0,%y0);
    }

  /* Handle other kinds of reads to the 387 */

  if (STACK_TOP_P (operands[0]) && GET_CODE (operands[1]) == CONST_DOUBLE)
    return (char *) output_move_const_single (operands);

  if (STACK_TOP_P (operands[0]))
    return AS1 (fld%z1,%y1);

  /* Handle all SFmode moves not involving the 387 */

  return (char *) singlemove_string (operands);
}
}

static char *
output_47 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (STACK_REG_P (operands[1]))
    {
      rtx xops[3];

      xops[0] = AT_SP (SFmode);
      xops[1] = GEN_INT (8);
      xops[2] = stack_pointer_rtx;

      output_asm_insn (AS2 (sub%L2,%1,%2), xops);

      if (find_regno_note (insn, REG_DEAD, FIRST_STACK_REG))
        output_asm_insn (AS1 (fstp%Q0,%0), xops);
      else
        output_asm_insn (AS1 (fst%Q0,%0), xops);

      RET;
    }
  else
    return (char *) output_move_double (operands);
}
}

static char *
output_48 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (STACK_TOP_P (operands[0]))
    return AS1 (fxch,%1);
  else
    return AS1 (fxch,%0);
}
}

static char *
output_49 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  int stack_top_dies = find_regno_note (insn, REG_DEAD, FIRST_STACK_REG) != 0;

  /* First handle a `pop' insn or a `fld %st(0)' */

  if (STACK_TOP_P (operands[0]) && STACK_TOP_P (operands[1]))
    {
      if (stack_top_dies)
	return AS1 (fstp,%y0);
      else
        return AS1 (fld,%y0);
    }

  /* Handle a transfer between the 387 and a 386 register */

  if (STACK_TOP_P (operands[0]) && NON_STACK_REG_P (operands[1]))
    {
      output_op_from_reg (operands[1], AS1 (fld%z0,%y1));
      RET;
    }

  if (STACK_TOP_P (operands[1]) && NON_STACK_REG_P (operands[0]))
    {
      output_to_reg (operands[0], stack_top_dies);
      RET;
    }

  /* Handle other kinds of writes from the 387 */

  if (STACK_TOP_P (operands[1]))
    {
      if (stack_top_dies)
	return AS1 (fstp%z0,%y0);
      else
        return AS1 (fst%z0,%y0);
    }

  /* Handle other kinds of reads to the 387 */

  if (STACK_TOP_P (operands[0]) && GET_CODE (operands[1]) == CONST_DOUBLE)
    return (char *) output_move_const_single (operands);

  if (STACK_TOP_P (operands[0]))
    return AS1 (fld%z1,%y1);

  /* Handle all DFmode moves not involving the 387 */

  return (char *) output_move_double (operands);
}
}

static char *
output_50 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  return (char *) output_move_double (operands);
}
}

static char *
output_51 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  return (char *) output_move_double (operands);
}
}

static char *
output_52 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if ((TARGET_486 || REGNO (operands[0]) == 0)
      && REG_P (operands[1]) && REGNO (operands[0]) == REGNO (operands[1]))
    {
      rtx xops[2];
      xops[0] = operands[0];
      xops[1] = GEN_INT (0xffff);
      output_asm_insn (AS2 (and%L0,%1,%k0), xops);
      RET;
    }

#ifdef INTEL_SYNTAX
  return AS2 (movzx,%1,%0);
#else
  return AS2 (movz%W0%L0,%1,%0);
#endif
}
}

static char *
output_53 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if ((TARGET_486 || REGNO (operands[0]) == 0)
      && REG_P (operands[1]) && REGNO (operands[0]) == REGNO (operands[1]))
    {
      rtx xops[2];
      xops[0] = operands[0];
      xops[1] = GEN_INT (0xff);
      output_asm_insn (AS2 (and%L0,%1,%k0), xops);
      RET;
    }

#ifdef INTEL_SYNTAX
  return AS2 (movzx,%1,%0);
#else
  return AS2 (movz%B0%W0,%1,%0);
#endif
}
}

static char *
output_54 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if ((TARGET_486 || REGNO (operands[0]) == 0)
      && REG_P (operands[1]) && REGNO (operands[0]) == REGNO (operands[1]))
    {
      rtx xops[2];
      xops[0] = operands[0];
      xops[1] = GEN_INT (0xff);
      output_asm_insn (AS2 (and%L0,%1,%k0), xops);
      RET;
    }

#ifdef INTEL_SYNTAX
  return AS2 (movzx,%1,%0);
#else
  return AS2 (movz%B0%L0,%1,%0);
#endif
}
}

static char *
output_55 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  operands[0] = gen_rtx (REG, SImode, REGNO (operands[0]) + 1);
  return AS2 (xor%L0,%0,%0);
}
}

static char *
output_56 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (REGNO (operands[0]) == 0)
    {
      /* This used to be cwtl, but that extends HI to SI somehow.  */
#ifdef INTEL_SYNTAX
      return "cdq";
#else
      return "cltd";
#endif
    }

  operands[1] = gen_rtx (REG, SImode, REGNO (operands[0]) + 1);
  output_asm_insn (AS2 (mov%L0,%0,%1), operands);

  operands[0] = GEN_INT (31);
  return AS2 (sar%L1,%0,%1);
}
}

static char *
output_57 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (REGNO (operands[0]) == 0
      && REG_P (operands[1]) && REGNO (operands[1]) == 0)
#ifdef INTEL_SYNTAX
    return "cwde";
#else
    return "cwtl";
#endif

#ifdef INTEL_SYNTAX
  return AS2 (movsx,%1,%0);
#else
  return AS2 (movs%W0%L0,%1,%0);
#endif
}
}

static char *
output_58 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (REGNO (operands[0]) == 0
      && REG_P (operands[1]) && REGNO (operands[1]) == 0)
    return "cbtw";

#ifdef INTEL_SYNTAX
  return AS2 (movsx,%1,%0);
#else
  return AS2 (movs%B0%W0,%1,%0);
#endif
}
}

static char *
output_59 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
#ifdef INTEL_SYNTAX
  return AS2 (movsx,%1,%0);
#else
  return AS2 (movs%B0%L0,%1,%0);
#endif
}
}

static char *
output_60 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  int stack_top_dies = find_regno_note (insn, REG_DEAD, FIRST_STACK_REG) != 0;

  if (NON_STACK_REG_P (operands[1]))
    {
      output_op_from_reg (operands[1], AS1 (fld%z0,%y1));
      RET;
    }

  if (NON_STACK_REG_P (operands[0]))
    {
      output_to_reg (operands[0], stack_top_dies);
      RET;
    }

  if (STACK_TOP_P (operands[0]))
    return AS1 (fld%z1,%y1);

  if (GET_CODE (operands[0]) == MEM)
    {
      if (stack_top_dies)
	return AS1 (fstp%z0,%y0);
      else
        return AS1 (fst%z0,%y0);
    }

  abort ();
}
}

static char *
output_62 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  int stack_top_dies = find_regno_note (insn, REG_DEAD, FIRST_STACK_REG) != 0;

  if (GET_CODE (operands[0]) == MEM)
    {
      if (stack_top_dies)
	return AS1 (fstp%z0,%0);
      else
        return AS1 (fst%z0,%0);
    }
  else if (STACK_TOP_P (operands[0]))
    {
      output_asm_insn (AS1 (fstp%z2,%y2), operands);
      return AS1 (fld%z2,%y2);
    }
  else
    abort ();
}
}

static char *
output_67 (operands, insn)
     rtx *operands;
     rtx insn;
{
 return (char *) output_fix_trunc (insn, operands);
}

static char *
output_68 (operands, insn)
     rtx *operands;
     rtx insn;
{
 return (char *) output_fix_trunc (insn, operands);
}

static char *
output_71 (operands, insn)
     rtx *operands;
     rtx insn;
{
 return (char *) output_fix_trunc (insn, operands);
}

static char *
output_72 (operands, insn)
     rtx *operands;
     rtx insn;
{
 return (char *) output_fix_trunc (insn, operands);
}

static char *
output_77 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (NON_STACK_REG_P (operands[1]))
    {
      output_op_from_reg (operands[1], AS1 (fild%z0,%1));
      RET;
    }
  else if (GET_CODE (operands[1]) == MEM)
    return AS1 (fild%z1,%1);
  else
    abort ();
}
}

static char *
output_78 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (NON_STACK_REG_P (operands[1]))
    {
      output_op_from_reg (operands[1], AS1 (fild%z0,%1));
      RET;
    }
  else if (GET_CODE (operands[1]) == MEM)
    return AS1 (fild%z1,%1);
  else
    abort ();
}
}

static char *
output_79 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (NON_STACK_REG_P (operands[1]))
    {
      output_op_from_reg (operands[1], AS1 (fild%z0,%1));
      RET;
    }
  else if (GET_CODE (operands[1]) == MEM)
    return AS1 (fild%z1,%1);
  else
    abort ();
}
}

static char *
output_80 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (NON_STACK_REG_P (operands[1]))
    {
      output_op_from_reg (operands[1], AS1 (fild%z0,%1));
      RET;
    }
  else if (GET_CODE (operands[1]) == MEM)
    return AS1 (fild%z1,%1);
  else
    abort ();
}
}

static char *
output_81 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  rtx low[3], high[3];

  CC_STATUS_INIT;

  split_di (operands, 3, low, high);

  if (GET_CODE (low[2]) != CONST_INT || INTVAL (low[2]) != 0)
    {
      output_asm_insn (AS2 (add%L0,%2,%0), low);
      output_asm_insn (AS2 (adc%L0,%2,%0), high);
    }
  else
    output_asm_insn (AS2 (add%L0,%2,%0), high);
  RET;
}
}

static char *
output_82 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (REG_P (operands[0]) && REGNO (operands[0]) != REGNO (operands[1]))
    {
      if (REG_P (operands[2]) && REGNO (operands[0]) == REGNO (operands[2]))
	return AS2 (add%L0,%1,%0);

      if (! TARGET_486 || ! REG_P (operands[2]))
        {
	  CC_STATUS_INIT;

	  if (operands[2] == stack_pointer_rtx)
	    {
	      rtx temp;

	      temp = operands[1];
	      operands[1] = operands[2];
	      operands[2] = temp;
	    }
	  if (operands[2] != stack_pointer_rtx)
	    {
	      operands[1] = SET_SRC (PATTERN (insn));
	      return AS2 (lea%L0,%a1,%0);
	    }
	}

      output_asm_insn (AS2 (mov%L0,%1,%0), operands);
    }

  if (operands[2] == const1_rtx)
    return AS1 (inc%L0,%0);

  if (operands[2] == constm1_rtx)
    return AS1 (dec%L0,%0);

  return AS2 (add%L0,%2,%0);
}
}

static char *
output_83 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (operands[2] == const1_rtx)
    return AS1 (inc%W0,%0);

  if (operands[2] == constm1_rtx)
    return AS1 (dec%W0,%0);

  return AS2 (add%W0,%2,%0);
}
}

static char *
output_84 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (operands[2] == const1_rtx)
    return AS1 (inc%B0,%0);

  if (operands[2] == constm1_rtx)
    return AS1 (dec%B0,%0);

  return AS2 (add%B0,%2,%0);
}
}

static char *
output_85 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  CC_STATUS_INIT;
  /* Adding a constant to a register is faster with an add.  */
  /* ??? can this ever happen? */
  if (GET_CODE (operands[1]) == PLUS
      && GET_CODE (XEXP (operands[1], 1)) == CONST_INT
      && rtx_equal_p (operands[0], XEXP (operands[1], 0)))
    {
      operands[1] = XEXP (operands[1], 1);

      if (operands[1] == const1_rtx)
        return AS1 (inc%L0,%0);

      if (operands[1] == constm1_rtx)
        return AS1 (dec%L0,%0);

      return AS2 (add%L0,%1,%0);
    }
  return AS2 (lea%L0,%a1,%0);
}
}

static char *
output_88 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  rtx low[3], high[3];

  CC_STATUS_INIT;

  split_di (operands, 3, low, high);

  if (GET_CODE (low[2]) != CONST_INT || INTVAL (low[2]) != 0)
    {
      output_asm_insn (AS2 (sub%L0,%2,%0), low);
      output_asm_insn (AS2 (sbb%L0,%2,%0), high);
    }
  else
    output_asm_insn (AS2 (sub%L0,%2,%0), high);

  RET;
}
}

static char *
output_89 (operands, insn)
     rtx *operands;
     rtx insn;
{
 return AS2 (sub%L0,%2,%0);
}

static char *
output_90 (operands, insn)
     rtx *operands;
     rtx insn;
{
 return AS2 (sub%W0,%2,%0);
}

static char *
output_91 (operands, insn)
     rtx *operands;
     rtx insn;
{
 return AS2 (sub%B0,%2,%0);
}

static char *
output_94 (operands, insn)
     rtx *operands;
     rtx insn;
{
 return AS2 (imul%W0,%2,%0);
}

static char *
output_95 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (GET_CODE (operands[1]) == REG
      && REGNO (operands[1]) == REGNO (operands[0])
      && (GET_CODE (operands[2]) == MEM || GET_CODE (operands[2]) == REG))
    /* Assembler has weird restrictions.  */
    return AS2 (imul%W0,%2,%0);
  return AS3 (imul%W0,%2,%1,%0);
}
}

static char *
output_96 (operands, insn)
     rtx *operands;
     rtx insn;
{
 return AS2 (imul%L0,%2,%0);
}

static char *
output_97 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (GET_CODE (operands[1]) == REG
      && REGNO (operands[1]) == REGNO (operands[0])
      && (GET_CODE (operands[2]) == MEM || GET_CODE (operands[2]) == REG))
    /* Assembler has weird restrictions.  */
    return AS2 (imul%L0,%2,%0);
  return AS3 (imul%L0,%2,%1,%0);
}
}

static char *
output_105 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
#ifdef INTEL_SYNTAX
  output_asm_insn ("cdq", operands);
#else
  output_asm_insn ("cltd", operands);
#endif
  return AS1 (idiv%L0,%2);
}
}

static char *
output_107 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  output_asm_insn (AS2 (xor%L3,%3,%3), operands);
  return AS1 (div%L0,%2);
}
}

static char *
output_108 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  output_asm_insn (AS2 (xor%W0,%3,%3), operands);
  return AS1 (div%W0,%2);
}
}

static char *
output_109 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (GET_CODE (operands[2]) == CONST_INT
      && ! (GET_CODE (operands[0]) == MEM && MEM_VOLATILE_P (operands[0])))
    {
      if (INTVAL (operands[2]) == 0xffff && REG_P (operands[0])
	  && (! REG_P (operands[1])
	      || REGNO (operands[0]) != 0 || REGNO (operands[1]) != 0)
	  && (! TARGET_486 || ! rtx_equal_p (operands[0], operands[1])))
	{
	  /* ??? tege: Should forget CC_STATUS only if we clobber a
	     remembered operand.  Fix that later.  */
	  CC_STATUS_INIT;
#ifdef INTEL_SYNTAX
	  return AS2 (movzx,%w1,%0);
#else
	  return AS2 (movz%W0%L0,%w1,%0);
#endif
	}

      if (INTVAL (operands[2]) == 0xff && REG_P (operands[0])
	  && !(REG_P (operands[1]) && NON_QI_REG_P (operands[1]))
	  && (! REG_P (operands[1])
	      || REGNO (operands[0]) != 0 || REGNO (operands[1]) != 0)
	  && (! TARGET_486 || ! rtx_equal_p (operands[0], operands[1])))
	{
	  /* ??? tege: Should forget CC_STATUS only if we clobber a
	     remembered operand.  Fix that later.  */
	  CC_STATUS_INIT;
#ifdef INTEL_SYNTAX
	  return AS2 (movzx,%b1,%0);
#else
	  return AS2 (movz%B0%L0,%b1,%0);
#endif
	}

      if (QI_REG_P (operands[0]) && ~(INTVAL (operands[2]) | 0xff) == 0)
	{
	  CC_STATUS_INIT;

	  if (INTVAL (operands[2]) == 0xffffff00)
	    {
	      operands[2] = const0_rtx;
	      return AS2 (mov%B0,%2,%b0);
	    }

	  operands[2] = GEN_INT (INTVAL (operands[2]) & 0xff);
	  return AS2 (and%B0,%2,%b0);
	}

      if (QI_REG_P (operands[0]) && ~(INTVAL (operands[2]) | 0xff00) == 0)
	{
	  CC_STATUS_INIT;

	  if (INTVAL (operands[2]) == 0xffff00ff)
	    {
	      operands[2] = const0_rtx;
	      return AS2 (mov%B0,%2,%h0);
	    }

	  operands[2] = GEN_INT ((INTVAL (operands[2]) >> 8) & 0xff);
	  return AS2 (and%B0,%2,%h0);
	}

      if (GET_CODE (operands[0]) == MEM && INTVAL (operands[2]) == 0xffff0000)
        {
	  operands[2] = const0_rtx;
	  return AS2 (mov%W0,%2,%w0);
	}
    }

  return AS2 (and%L0,%2,%0);
}
}

static char *
output_110 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (GET_CODE (operands[2]) == CONST_INT
      && ! (GET_CODE (operands[0]) == MEM && MEM_VOLATILE_P (operands[0])))
    {
      /* Can we ignore the upper byte? */
      if ((! REG_P (operands[0]) || QI_REG_P (operands[0]))
	  && (INTVAL (operands[2]) & 0xff00) == 0xff00)
	{
	  CC_STATUS_INIT;

	  if ((INTVAL (operands[2]) & 0xff) == 0)
	    {
	      operands[2] = const0_rtx;
	      return AS2 (mov%B0,%2,%b0);
	    }

	  operands[2] = GEN_INT (INTVAL (operands[2]) & 0xff);
	  return AS2 (and%B0,%2,%b0);
	}

      /* Can we ignore the lower byte? */
      /* ??? what about offsettable memory references? */
      if (QI_REG_P (operands[0]) && (INTVAL (operands[2]) & 0xff) == 0xff)
	{
	  CC_STATUS_INIT;

	  if ((INTVAL (operands[2]) & 0xff00) == 0)
	    {
	      operands[2] = const0_rtx;
	      return AS2 (mov%B0,%2,%h0);
	    }

	  operands[2] = GEN_INT ((INTVAL (operands[2]) >> 8) & 0xff);
	  return AS2 (and%B0,%2,%h0);
	}
    }

  return AS2 (and%W0,%2,%0);
}
}

static char *
output_111 (operands, insn)
     rtx *operands;
     rtx insn;
{
 return AS2 (and%B0,%2,%0);
}

static char *
output_112 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (GET_CODE (operands[2]) == CONST_INT
      && ! (GET_CODE (operands[0]) == MEM && MEM_VOLATILE_P (operands[0])))
    {
      if ((! REG_P (operands[0]) || QI_REG_P (operands[0]))
	  && (INTVAL (operands[2]) & ~0xff) == 0)
	{
	  CC_STATUS_INIT;

	  if (INTVAL (operands[2]) == 0xff)
	    return AS2 (mov%B0,%2,%b0);

	  return AS2 (or%B0,%2,%b0);
	}

      if (QI_REG_P (operands[0]) && (INTVAL (operands[2]) & ~0xff00) == 0)
	{
	  CC_STATUS_INIT;
	  operands[2] = GEN_INT (INTVAL (operands[2]) >> 8);

	  if (INTVAL (operands[2]) == 0xff)
	    return AS2 (mov%B0,%2,%h0);

	  return AS2 (or%B0,%2,%h0);
	}
    }

  return AS2 (or%L0,%2,%0);
}
}

static char *
output_113 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (GET_CODE (operands[2]) == CONST_INT
      && ! (GET_CODE (operands[0]) == MEM && MEM_VOLATILE_P (operands[0])))
    {
      /* Can we ignore the upper byte? */
      if ((! REG_P (operands[0]) || QI_REG_P (operands[0]))
	  && (INTVAL (operands[2]) & 0xff00) == 0)
	{
	  CC_STATUS_INIT;
	  if (INTVAL (operands[2]) & 0xffff0000)
	    operands[2] = GEN_INT (INTVAL (operands[2]) & 0xffff);

	  if (INTVAL (operands[2]) == 0xff)
	    return AS2 (mov%B0,%2,%b0);

	  return AS2 (or%B0,%2,%b0);
	}

      /* Can we ignore the lower byte? */
      /* ??? what about offsettable memory references? */
      if (QI_REG_P (operands[0])
	  && (INTVAL (operands[2]) & 0xff) == 0)
	{
	  CC_STATUS_INIT;
	  operands[2] = GEN_INT ((INTVAL (operands[2]) >> 8) & 0xff);

	  if (INTVAL (operands[2]) == 0xff)
	    return AS2 (mov%B0,%2,%h0);

	  return AS2 (or%B0,%2,%h0);
	}
    }

  return AS2 (or%W0,%2,%0);
}
}

static char *
output_114 (operands, insn)
     rtx *operands;
     rtx insn;
{
 return AS2 (or%B0,%2,%0);
}

static char *
output_115 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (GET_CODE (operands[2]) == CONST_INT
      && ! (GET_CODE (operands[0]) == MEM && MEM_VOLATILE_P (operands[0])))
    {
      if ((! REG_P (operands[0]) || QI_REG_P (operands[0]))
	  && (INTVAL (operands[2]) & ~0xff) == 0)
	{
	  CC_STATUS_INIT;

	  if (INTVAL (operands[2]) == 0xff)
	    return AS1 (not%B0,%b0);

	  return AS2 (xor%B0,%2,%b0);
	}

      if (QI_REG_P (operands[0]) && (INTVAL (operands[2]) & ~0xff00) == 0)
	{
	  CC_STATUS_INIT;
	  operands[2] = GEN_INT (INTVAL (operands[2]) >> 8);

	  if (INTVAL (operands[2]) == 0xff)
	    return AS1 (not%B0,%h0);

	  return AS2 (xor%B0,%2,%h0);
	}
    }

  return AS2 (xor%L0,%2,%0);
}
}

static char *
output_116 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (GET_CODE (operands[2]) == CONST_INT
      && ! (GET_CODE (operands[0]) == MEM && MEM_VOLATILE_P (operands[0])))
    {
      /* Can we ignore the upper byte? */
      if ((! REG_P (operands[0]) || QI_REG_P (operands[0]))
	  && (INTVAL (operands[2]) & 0xff00) == 0)
	{
	  CC_STATUS_INIT;
	  if (INTVAL (operands[2]) & 0xffff0000)
	    operands[2] = GEN_INT (INTVAL (operands[2]) & 0xffff);

	  if (INTVAL (operands[2]) == 0xff)
	    return AS1 (not%B0,%b0);

	  return AS2 (xor%B0,%2,%b0);
	}

      /* Can we ignore the lower byte? */
      /* ??? what about offsettable memory references? */
      if (QI_REG_P (operands[0])
	  && (INTVAL (operands[2]) & 0xff) == 0)
	{
	  CC_STATUS_INIT;
	  operands[2] = GEN_INT ((INTVAL (operands[2]) >> 8) & 0xff);

	  if (INTVAL (operands[2]) == 0xff)
	    return AS1 (not%B0,%h0);

	  return AS2 (xor%B0,%2,%h0);
	}
    }

  return AS2 (xor%W0,%2,%0);
}
}

static char *
output_117 (operands, insn)
     rtx *operands;
     rtx insn;
{
 return AS2 (xor%B0,%2,%0);
}

static char *
output_118 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  rtx xops[2], low[1], high[1];

  CC_STATUS_INIT;

  split_di (operands, 1, low, high);
  xops[0] = const0_rtx;
  xops[1] = high[0];

  output_asm_insn (AS1 (neg%L0,%0), low);
  output_asm_insn (AS2 (adc%L1,%0,%1), xops);
  output_asm_insn (AS1 (neg%L0,%0), high);
  RET;
}
}

static char *
output_141 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  rtx xops[4], low[1], high[1];

  CC_STATUS_INIT;

  split_di (operands, 1, low, high);
  xops[0] = operands[2];
  xops[1] = const1_rtx;
  xops[2] = low[0];
  xops[3] = high[0];

  if (INTVAL (xops[0]) > 31)
    {
      output_asm_insn (AS2 (mov%L3,%2,%3), xops);	/* Fast shift by 32 */
      output_asm_insn (AS2 (xor%L2,%2,%2), xops);

      if (INTVAL (xops[0]) > 32)
        {
	  xops[0] = GEN_INT (INTVAL (xops[0]) - 32);
	  output_asm_insn (AS2 (sal%L3,%0,%3), xops); /* Remaining shift */
	}
    }
  else
    {
      output_asm_insn (AS3 (shld%L3,%0,%2,%3), xops);
      output_asm_insn (AS2 (sal%L2,%0,%2), xops);
    }
  RET;
}
}

static char *
output_142 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  rtx xops[4], low[1], high[1];

  CC_STATUS_INIT;

  split_di (operands, 1, low, high);
  xops[0] = operands[2];
  xops[1] = const1_rtx;
  xops[2] = low[0];
  xops[3] = high[0];

  output_asm_insn (AS2 (ror%B0,%1,%0), xops);	/* shift count / 2 */

  output_asm_insn (AS3_SHIFT_DOUBLE (shld%L3,%0,%2,%3), xops);
  output_asm_insn (AS2 (sal%L2,%0,%2), xops);
  output_asm_insn (AS3_SHIFT_DOUBLE (shld%L3,%0,%2,%3), xops);
  output_asm_insn (AS2 (sal%L2,%0,%2), xops);

  xops[1] = GEN_INT (7);			/* shift count & 1 */

  output_asm_insn (AS2 (shr%B0,%1,%0), xops);

  output_asm_insn (AS3_SHIFT_DOUBLE (shld%L3,%0,%2,%3), xops);
  output_asm_insn (AS2 (sal%L2,%0,%2), xops);

  RET;
}
}

static char *
output_143 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (REG_P (operands[0]) && REGNO (operands[0]) != REGNO (operands[1]))
    {
      if (TARGET_486 && INTVAL (operands[2]) == 1)
	{
	  output_asm_insn (AS2 (mov%L0,%1,%0), operands);
	  return AS2 (add%L0,%1,%0);
	}
      else
        {
          CC_STATUS_INIT;

	  if (operands[1] == stack_pointer_rtx)
	    {
	      output_asm_insn (AS2 (mov%L0,%1,%0), operands);
	      operands[1] = operands[0];
	    }
          operands[1] = gen_rtx (MULT, SImode, operands[1],
				 GEN_INT (1 << INTVAL (operands[2])));
	  return AS2 (lea%L0,%a1,%0);
	}
    }

  if (REG_P (operands[2]))
    return AS2 (sal%L0,%b2,%0);

  if (REG_P (operands[0]) && operands[2] == const1_rtx)
    return AS2 (add%L0,%0,%0);

  return AS2 (sal%L0,%2,%0);
}
}

static char *
output_144 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (REG_P (operands[2]))
    return AS2 (sal%W0,%b2,%0);

  if (REG_P (operands[0]) && operands[2] == const1_rtx)
    return AS2 (add%W0,%0,%0);

  return AS2 (sal%W0,%2,%0);
}
}

static char *
output_145 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (REG_P (operands[2]))
    return AS2 (sal%B0,%b2,%0);

  if (REG_P (operands[0]) && operands[2] == const1_rtx)
    return AS2 (add%B0,%0,%0);

  return AS2 (sal%B0,%2,%0);
}
}

static char *
output_147 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  rtx xops[4], low[1], high[1];

  CC_STATUS_INIT;

  split_di (operands, 1, low, high);
  xops[0] = operands[2];
  xops[1] = const1_rtx;
  xops[2] = low[0];
  xops[3] = high[0];

  if (INTVAL (xops[0]) > 31)
    {
      xops[1] = GEN_INT (31);
      output_asm_insn (AS2 (mov%L2,%3,%2), xops);
      output_asm_insn (AS2 (sar%L3,%1,%3), xops);	/* shift by 32 */

      if (INTVAL (xops[0]) > 32)
        {
	  xops[0] = GEN_INT (INTVAL (xops[0]) - 32);
	  output_asm_insn (AS2 (sar%L2,%0,%2), xops); /* Remaining shift */
	}
    }
  else
    {
      output_asm_insn (AS3 (shrd%L2,%0,%3,%2), xops);
      output_asm_insn (AS2 (sar%L3,%0,%3), xops);
    }

  RET;
}
}

static char *
output_148 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  rtx xops[4], low[1], high[1];

  CC_STATUS_INIT;

  split_di (operands, 1, low, high);
  xops[0] = operands[2];
  xops[1] = const1_rtx;
  xops[2] = low[0];
  xops[3] = high[0];

  output_asm_insn (AS2 (ror%B0,%1,%0), xops);	/* shift count / 2 */

  output_asm_insn (AS3_SHIFT_DOUBLE (shrd%L2,%0,%3,%2), xops);
  output_asm_insn (AS2 (sar%L3,%0,%3), xops);
  output_asm_insn (AS3_SHIFT_DOUBLE (shrd%L2,%0,%3,%2), xops);
  output_asm_insn (AS2 (sar%L3,%0,%3), xops);

  xops[1] = GEN_INT (7);			/* shift count & 1 */

  output_asm_insn (AS2 (shr%B0,%1,%0), xops);

  output_asm_insn (AS3_SHIFT_DOUBLE (shrd%L2,%0,%3,%2), xops);
  output_asm_insn (AS2 (sar%L3,%0,%3), xops);

  RET;
}
}

static char *
output_149 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (REG_P (operands[2]))
    return AS2 (sar%L0,%b2,%0);
  else
    return AS2 (sar%L0,%2,%0);
}
}

static char *
output_150 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (REG_P (operands[2]))
    return AS2 (sar%W0,%b2,%0);
  else
    return AS2 (sar%W0,%2,%0);
}
}

static char *
output_151 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (REG_P (operands[2]))
    return AS2 (sar%B0,%b2,%0);
  else
    return AS2 (sar%B0,%2,%0);
}
}

static char *
output_153 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  rtx xops[4], low[1], high[1];

  CC_STATUS_INIT;

  split_di (operands, 1, low, high);
  xops[0] = operands[2];
  xops[1] = const1_rtx;
  xops[2] = low[0];
  xops[3] = high[0];

  if (INTVAL (xops[0]) > 31)
    {
      output_asm_insn (AS2 (mov%L2,%3,%2), xops);	/* Fast shift by 32 */
      output_asm_insn (AS2 (xor%L3,%3,%3), xops);

      if (INTVAL (xops[0]) > 32)
        {
	  xops[0] = GEN_INT (INTVAL (xops[0]) - 32);
	  output_asm_insn (AS2 (shr%L2,%0,%2), xops); /* Remaining shift */
	}
    }
  else
    {
      output_asm_insn (AS3 (shrd%L2,%0,%3,%2), xops);
      output_asm_insn (AS2 (shr%L3,%0,%3), xops);
    }

  RET;
}
}

static char *
output_154 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  rtx xops[4], low[1], high[1];

  CC_STATUS_INIT;

  split_di (operands, 1, low, high);
  xops[0] = operands[2];
  xops[1] = const1_rtx;
  xops[2] = low[0];
  xops[3] = high[0];

  output_asm_insn (AS2 (ror%B0,%1,%0), xops);	/* shift count / 2 */

  output_asm_insn (AS3_SHIFT_DOUBLE (shrd%L2,%0,%3,%2), xops);
  output_asm_insn (AS2 (shr%L3,%0,%3), xops);
  output_asm_insn (AS3_SHIFT_DOUBLE (shrd%L2,%0,%3,%2), xops);
  output_asm_insn (AS2 (shr%L3,%0,%3), xops);

  xops[1] = GEN_INT (7);			/* shift count & 1 */

  output_asm_insn (AS2 (shr%B0,%1,%0), xops);

  output_asm_insn (AS3_SHIFT_DOUBLE (shrd%L2,%0,%3,%2), xops);
  output_asm_insn (AS2 (shr%L3,%0,%3), xops);

  RET;
}
}

static char *
output_155 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (REG_P (operands[2]))
    return AS2 (shr%L0,%b2,%0);
  else
    return AS2 (shr%L0,%2,%1);
}
}

static char *
output_156 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (REG_P (operands[2]))
    return AS2 (shr%W0,%b2,%0);
  else
    return AS2 (shr%W0,%2,%0);
}
}

static char *
output_157 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (REG_P (operands[2]))
    return AS2 (shr%B0,%b2,%0);
  else
    return AS2 (shr%B0,%2,%0);
}
}

static char *
output_158 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (REG_P (operands[2]))
    return AS2 (rol%L0,%b2,%0);
  else
    return AS2 (rol%L0,%2,%0);
}
}

static char *
output_159 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (REG_P (operands[2]))
    return AS2 (rol%W0,%b2,%0);
  else
    return AS2 (rol%W0,%2,%0);
}
}

static char *
output_160 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (REG_P (operands[2]))
    return AS2 (rol%B0,%b2,%0);
  else
    return AS2 (rol%B0,%2,%0);
}
}

static char *
output_161 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (REG_P (operands[2]))
    return AS2 (ror%L0,%b2,%0);
  else
    return AS2 (ror%L0,%2,%0);
}
}

static char *
output_162 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (REG_P (operands[2]))
    return AS2 (ror%W0,%b2,%0);
  else
    return AS2 (ror%W0,%2,%0);
}
}

static char *
output_163 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (REG_P (operands[2]))
    return AS2 (ror%B0,%b2,%0);
  else
    return AS2 (ror%B0,%2,%0);
}
}

static char *
output_164 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  CC_STATUS_INIT;

  if (INTVAL (operands[3]) == 1)
    return AS2 (bts%L0,%2,%0);
  else
    return AS2 (btr%L0,%2,%0);
}
}

static char *
output_165 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  CC_STATUS_INIT;

  return AS2 (btc%L0,%1,%0);
}
}

static char *
output_166 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  CC_STATUS_INIT;

  return AS2 (btc%L0,%2,%0);
}
}

static char *
output_167 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  cc_status.flags |= CC_Z_IN_NOT_C;
  return AS2 (bt%L0,%1,%0);
}
}

static char *
output_168 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  unsigned int mask;

  mask = ((1 << INTVAL (operands[1])) - 1) << INTVAL (operands[2]);
  operands[1] = GEN_INT (mask);

  if (QI_REG_P (operands[0]))
    {
      if ((mask & ~0xff) == 0)
        {
	  cc_status.flags |= CC_NOT_NEGATIVE;
	  return AS2 (test%B0,%1,%b0);
	}

      if ((mask & ~0xff00) == 0)
        {
	  cc_status.flags |= CC_NOT_NEGATIVE;
	  operands[1] = GEN_INT (mask >> 8);
	  return AS2 (test%B0,%1,%h0);
	}
    }

  return AS2 (test%L0,%1,%0);
}
}

static char *
output_169 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  unsigned int mask;

  mask = ((1 << INTVAL (operands[1])) - 1) << INTVAL (operands[2]);
  operands[1] = GEN_INT (mask);

  if (! REG_P (operands[0]) || QI_REG_P (operands[0]))
    {
      if ((mask & ~0xff) == 0)
        {
	  cc_status.flags |= CC_NOT_NEGATIVE;
	  return AS2 (test%B0,%1,%b0);
	}

      if ((mask & ~0xff00) == 0)
        {
	  cc_status.flags |= CC_NOT_NEGATIVE;
	  operands[1] = GEN_INT (mask >> 8);

	  if (QI_REG_P (operands[0]))
	    return AS2 (test%B0,%1,%h0);
	  else
	    {
	      operands[0] = adj_offsettable_operand (operands[0], 1);
	      return AS2 (test%B0,%1,%b0);
	    }
	}

      if (GET_CODE (operands[0]) == MEM && (mask & ~0xff0000) == 0)
        {
	  cc_status.flags |= CC_NOT_NEGATIVE;
	  operands[1] = GEN_INT (mask >> 16);
	  operands[0] = adj_offsettable_operand (operands[0], 2);
	  return AS2 (test%B0,%1,%b0);
	}

      if (GET_CODE (operands[0]) == MEM && (mask & ~0xff000000) == 0)
        {
	  cc_status.flags |= CC_NOT_NEGATIVE;
	  operands[1] = GEN_INT (mask >> 24);
	  operands[0] = adj_offsettable_operand (operands[0], 3);
	  return AS2 (test%B0,%1,%b0);
	}
    }

  if (CONSTANT_P (operands[1]) || GET_CODE (operands[0]) == MEM)
    return AS2 (test%L0,%1,%0);

  return AS2 (test%L1,%0,%1);
}
}

static char *
output_171 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (cc_prev_status.flags & CC_Z_IN_NOT_C)
    return AS1 (setnb,%0);
  else
    return AS1 (sete,%0);
}
}

static char *
output_173 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (cc_prev_status.flags & CC_Z_IN_NOT_C)
    return AS1 (setb,%0);
  else
    return AS1 (setne,%0);
}

}

static char *
output_175 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (TARGET_IEEE_FP && (cc_prev_status.flags & CC_IN_80387))
    return AS1 (sete,%0);

  OUTPUT_JUMP ("setg %0", "seta %0", NULL_PTR);
}
}

static char *
output_177 (operands, insn)
     rtx *operands;
     rtx insn;
{
 return "seta %0"; 
}

static char *
output_179 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (TARGET_IEEE_FP && (cc_prev_status.flags & CC_IN_80387))
    return AS1 (sete,%0);

  OUTPUT_JUMP ("setl %0", "setb %0", "sets %0");
}
}

static char *
output_181 (operands, insn)
     rtx *operands;
     rtx insn;
{
 return "setb %0"; 
}

static char *
output_183 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (TARGET_IEEE_FP && (cc_prev_status.flags & CC_IN_80387))
    return AS1 (sete,%0);

  OUTPUT_JUMP ("setge %0", "setae %0", "setns %0");
}
}

static char *
output_185 (operands, insn)
     rtx *operands;
     rtx insn;
{
 return "setae %0"; 
}

static char *
output_187 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (TARGET_IEEE_FP && (cc_prev_status.flags & CC_IN_80387))
    return AS1 (setb,%0);

  OUTPUT_JUMP ("setle %0", "setbe %0", NULL_PTR);
}
}

static char *
output_189 (operands, insn)
     rtx *operands;
     rtx insn;
{
 return "setbe %0"; 
}

static char *
output_191 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (cc_prev_status.flags & CC_Z_IN_NOT_C)
    return "jnc %l0";
  else
    return "je %l0";
}
}

static char *
output_193 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (cc_prev_status.flags & CC_Z_IN_NOT_C)
    return "jc %l0";
  else
    return "jne %l0";
}
}

static char *
output_195 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (TARGET_IEEE_FP && (cc_prev_status.flags & CC_IN_80387))
    return AS1 (je,%l0);

  OUTPUT_JUMP ("jg %l0", "ja %l0", NULL_PTR);
}
}

static char *
output_199 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (TARGET_IEEE_FP && (cc_prev_status.flags & CC_IN_80387))
    return AS1 (je,%l0);

  OUTPUT_JUMP ("jl %l0", "jb %l0", "js %l0");
}
}

static char *
output_203 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (TARGET_IEEE_FP && (cc_prev_status.flags & CC_IN_80387))
    return AS1 (je,%l0);

  OUTPUT_JUMP ("jge %l0", "jae %l0", "jns %l0");
}
}

static char *
output_207 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (TARGET_IEEE_FP && (cc_prev_status.flags & CC_IN_80387))
    return AS1 (jb,%l0);

  OUTPUT_JUMP ("jle %l0", "jbe %l0", NULL_PTR);
}
}

static char *
output_210 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (cc_prev_status.flags & CC_Z_IN_NOT_C)
    return "jc %l0";
  else
    return "jne %l0";
}
}

static char *
output_211 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (cc_prev_status.flags & CC_Z_IN_NOT_C)
    return "jnc %l0";
  else
    return "je %l0";
}
}

static char *
output_212 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (TARGET_IEEE_FP && (cc_prev_status.flags & CC_IN_80387))
    return AS1 (jne,%l0);

  OUTPUT_JUMP ("jle %l0", "jbe %l0", NULL_PTR);
}
}

static char *
output_214 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (TARGET_IEEE_FP && (cc_prev_status.flags & CC_IN_80387))
    return AS1 (jne,%l0);

  OUTPUT_JUMP ("jge %l0", "jae %l0", "jns %l0");
}
}

static char *
output_216 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (TARGET_IEEE_FP && (cc_prev_status.flags & CC_IN_80387))
    return AS1 (jne,%l0);

  OUTPUT_JUMP ("jl %l0", "jb %l0", "js %l0");
}
}

static char *
output_218 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (TARGET_IEEE_FP && (cc_prev_status.flags & CC_IN_80387))
    return AS1 (jae,%l0);

  OUTPUT_JUMP ("jg %l0", "ja %l0", NULL_PTR);
}
}

static char *
output_221 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  CC_STATUS_INIT;

  return AS1 (jmp,%*%0);
}
}

static char *
output_223 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  rtx xops[4];

  xops[0] = operands[0];
  xops[1] = operands[1];
  xops[2] = operands[2];
  xops[3] = pic_offset_table_rtx;

  output_asm_insn (AS2 (mov%L2,%3,%2), xops);
  output_asm_insn ("sub%L2 %l1@GOTOFF(%3,%0,4),%2", xops);
  output_asm_insn (AS1 (jmp,%*%2), xops);
  ASM_OUTPUT_ALIGN_CODE (asm_out_file);
  RET;
}
}

static char *
output_224 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  CC_STATUS_INIT;

  return AS1 (jmp,%*%0);
}
}

static char *
output_226 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (GET_CODE (operands[0]) == MEM
      && ! CONSTANT_ADDRESS_P (XEXP (operands[0], 0)))
    {
      operands[0] = XEXP (operands[0], 0);
      return AS1 (call,%*%0);
    }
  else
    return AS1 (call,%P0);
}
}

static char *
output_229 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (GET_CODE (operands[0]) == MEM
      && ! CONSTANT_ADDRESS_P (XEXP (operands[0], 0)))
    {
      operands[0] = XEXP (operands[0], 0);
      return AS1 (call,%*%0);
    }
  else
    return AS1 (call,%P0);
}
}

static char *
output_232 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (GET_CODE (operands[1]) == MEM
      && ! CONSTANT_ADDRESS_P (XEXP (operands[1], 0)))
    {
      operands[1] = XEXP (operands[1], 0);
      output_asm_insn (AS1 (call,%*%1), operands);
    }
  else
    output_asm_insn (AS1 (call,%P1), operands);

  RET;
}
}

static char *
output_235 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  if (GET_CODE (operands[1]) == MEM
      && ! CONSTANT_ADDRESS_P (XEXP (operands[1], 0)))
    {
      operands[1] = XEXP (operands[1], 0);
      output_asm_insn (AS1 (call,%*%1), operands);
    }
  else
    output_asm_insn (AS1 (call,%P1), operands);

  RET;
}
}

static char *
output_238 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  rtx addr = operands[1];

  if (GET_CODE (operands[0]) == MEM
      && ! CONSTANT_ADDRESS_P (XEXP (operands[0], 0)))
    {
      operands[0] = XEXP (operands[0], 0);
      output_asm_insn (AS1 (call,%*%0), operands);
    }
  else
    output_asm_insn (AS1 (call,%P0), operands);

  operands[2] = gen_rtx (REG, SImode, 0);
  output_asm_insn (AS2 (mov%L2,%2,%1), operands);

  operands[2] = gen_rtx (REG, SImode, 1);
  operands[1] = adj_offsettable_operand (addr, 4);
  output_asm_insn (AS2 (mov%L2,%2,%1), operands);

  operands[1] = adj_offsettable_operand (addr, 8);
  return AS1 (fnsave,%1);
}
}

static char *
output_239 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  rtx addr = operands[1];

  output_asm_insn (AS1 (call,%P0), operands);

  operands[2] = gen_rtx (REG, SImode, 0);
  output_asm_insn (AS2 (mov%L2,%2,%1), operands);

  operands[2] = gen_rtx (REG, SImode, 1);
  operands[1] = adj_offsettable_operand (addr, 4);
  output_asm_insn (AS2 (mov%L2,%2,%1), operands);

  operands[1] = adj_offsettable_operand (addr, 8);
  return AS1 (fnsave,%1);
}
}

static char *
output_242 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  function_epilogue (asm_out_file, get_frame_size ());
  RET;
}
}

static char *
output_245 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  rtx xops[2];

  output_asm_insn ("cld", operands);
  if (GET_CODE (operands[2]) == CONST_INT)
    {
      if (INTVAL (operands[2]) & ~0x03)
	{
	  xops[0] = GEN_INT ((INTVAL (operands[2]) >> 2) & 0x3fffffff);
	  xops[1] = operands[4];

	  output_asm_insn (AS2 (mov%L1,%0,%1), xops);
#ifdef INTEL_SYNTAX
	  output_asm_insn ("rep movsd", xops);
#else
	  output_asm_insn ("rep\n\tmovsl", xops);
#endif
	}
      if (INTVAL (operands[2]) & 0x02)
	output_asm_insn ("movsw", operands);
      if (INTVAL (operands[2]) & 0x01)
	output_asm_insn ("movsb", operands);
    }
  else
    abort ();
  RET;
}
}

static char *
output_247 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  rtx xops[4], label;

  label = gen_label_rtx ();

  output_asm_insn ("cld", operands);
  output_asm_insn (AS2 (xor%L0,%0,%0), operands);
  output_asm_insn ("repz\n\tcmps%B2", operands);
  output_asm_insn ("je %l0", &label);

  xops[0] = operands[0];
  xops[1] = gen_rtx (MEM, QImode,
		     gen_rtx (PLUS, SImode, operands[1], constm1_rtx));
  xops[2] = gen_rtx (MEM, QImode,
		     gen_rtx (PLUS, SImode, operands[2], constm1_rtx));
  xops[3] = operands[3];

  output_asm_insn (AS2 (movz%B1%L0,%1,%0), xops);
  output_asm_insn (AS2 (movz%B2%L3,%2,%3), xops);

  output_asm_insn (AS2 (sub%L0,%3,%0), xops);
  ASM_OUTPUT_INTERNAL_LABEL (asm_out_file, "L", CODE_LABEL_NUMBER (label));
  RET;
}
}

static char *
output_248 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  rtx xops[2];

  cc_status.flags |= CC_NOT_SIGNED;

  xops[0] = gen_rtx (REG, QImode, 0);
  xops[1] = CONST0_RTX (QImode);

  output_asm_insn ("cld", operands);
  output_asm_insn (AS2 (test%B0,%1,%0), xops);
  return "repz\n\tcmps%B2";
}
}

static char *
output_250 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  rtx xops[2];

  xops[0] = const1_rtx;
  xops[1] = operands[1];
  output_asm_insn (AS2 (bsf%L1,%1,%0), operands);
  output_asm_insn (AS2 (cmp%L1,%0,%1), xops);
  output_asm_insn (AS2 (sbb%L0,%2,%2), operands);
  output_asm_insn (AS2 (or%L0,%2,%0), operands);
  return "";
}
}

static char *
output_252 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  rtx xops[2];

  xops[0] = const1_rtx;
  xops[1] = operands[1];
  output_asm_insn (AS2 (bsf%W1,%1,%0), operands);
  output_asm_insn (AS2 (cmp%W1,%0,%1), xops);
  output_asm_insn (AS2 (sbb%W0,%2,%2), operands);
  output_asm_insn (AS2 (or%W0,%2,%0), operands);
  return "";
}
}

static char *
output_253 (operands, insn)
     rtx *operands;
     rtx insn;
{
 return (char *) output_387_binary_op (insn, operands);
}

static char *
output_254 (operands, insn)
     rtx *operands;
     rtx insn;
{
 return (char *) output_387_binary_op (insn, operands);
}

static char *
output_255 (operands, insn)
     rtx *operands;
     rtx insn;
{
 return (char *) output_387_binary_op (insn, operands);
}

static char *
output_256 (operands, insn)
     rtx *operands;
     rtx insn;
{
 return (char *) output_387_binary_op (insn, operands);
}

static char *
output_257 (operands, insn)
     rtx *operands;
     rtx insn;
{
 return (char *) output_387_binary_op (insn, operands);
}

static char *
output_258 (operands, insn)
     rtx *operands;
     rtx insn;
{
 return (char *) output_387_binary_op (insn, operands);
}

static char *
output_259 (operands, insn)
     rtx *operands;
     rtx insn;
{
 return (char *) output_387_binary_op (insn, operands);
}

static char *
output_260 (operands, insn)
     rtx *operands;
     rtx insn;
{
 return (char *) output_387_binary_op (insn, operands);
}

static char *
output_262 (operands, insn)
     rtx *operands;
     rtx insn;
{

{
  rtx xops[2];

  xops[0] = operands[0];
  xops[1] = constm1_rtx;
  output_asm_insn ("cld", operands);
  output_asm_insn (AS2 (mov%L0,%1,%0), xops);
  return "repnz\n\tscas%B2";
}
}

char * const insn_template[] =
  {
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    "push%L0 %1",
    "push%L0 %1",
    0,
    0,
    "push%W0 %1",
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    "mul%B0 %2",
    0,
    0,
    "idiv%B0 %2",
    "div%B0 %2",
    0,
    0,
    0,
    "cwtd\n\tidiv%W0 %2",
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    "neg%L0 %0",
    "neg%W0 %0",
    "neg%B0 %0",
    "fchs",
    "fchs",
    "fchs",
    "fabs",
    "fabs",
    "fabs",
    "fsqrt",
    "fsqrt",
    "fsqrt",
    "fsin",
    "fsin",
    "fsin",
    "fcos",
    "fcos",
    "fcos",
    "not%L0 %0",
    "not%W0 %0",
    "not%B0 %0",
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    "ja %l0",
    0,
    0,
    0,
    "jb %l0",
    0,
    0,
    0,
    "jae %l0",
    0,
    0,
    0,
    "jbe %l0",
    0,
    0,
    0,
    "jbe %l0",
    0,
    "jae %l0",
    0,
    "jb %l0",
    0,
    "ja %l0",
    "jmp %l0",
    0,
    0,
    0,
    0,
    0,
    0,
    "call %P0",
    0,
    0,
    "call %P0",
    0,
    0,
    "call %P1",
    0,
    0,
    "call %P1",
    0,
    0,
    0,
    0,
    "frstor %0",
    0,
    "nop",
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
  };

char *(*const insn_outfun[])() =
  {
    output_0,
    0,
    output_2,
    0,
    output_4,
    0,
    output_6,
    0,
    output_8,
    0,
    output_10,
    0,
    output_12,
    0,
    output_14,
    0,
    output_16,
    output_17,
    output_18,
    output_19,
    output_20,
    output_21,
    output_22,
    output_23,
    output_24,
    output_25,
    0,
    0,
    0,
    0,
    0,
    0,
    output_32,
    output_33,
    output_34,
    0,
    0,
    0,
    output_38,
    0,
    output_40,
    output_41,
    output_42,
    output_43,
    output_44,
    output_45,
    output_46,
    output_47,
    output_48,
    output_49,
    output_50,
    output_51,
    output_52,
    output_53,
    output_54,
    output_55,
    output_56,
    output_57,
    output_58,
    output_59,
    output_60,
    0,
    output_62,
    0,
    0,
    0,
    0,
    output_67,
    output_68,
    0,
    0,
    output_71,
    output_72,
    0,
    0,
    0,
    0,
    output_77,
    output_78,
    output_79,
    output_80,
    output_81,
    output_82,
    output_83,
    output_84,
    output_85,
    0,
    0,
    output_88,
    output_89,
    output_90,
    output_91,
    0,
    0,
    output_94,
    output_95,
    output_96,
    output_97,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    output_105,
    0,
    output_107,
    output_108,
    output_109,
    output_110,
    output_111,
    output_112,
    output_113,
    output_114,
    output_115,
    output_116,
    output_117,
    output_118,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    output_141,
    output_142,
    output_143,
    output_144,
    output_145,
    0,
    output_147,
    output_148,
    output_149,
    output_150,
    output_151,
    0,
    output_153,
    output_154,
    output_155,
    output_156,
    output_157,
    output_158,
    output_159,
    output_160,
    output_161,
    output_162,
    output_163,
    output_164,
    output_165,
    output_166,
    output_167,
    output_168,
    output_169,
    0,
    output_171,
    0,
    output_173,
    0,
    output_175,
    0,
    output_177,
    0,
    output_179,
    0,
    output_181,
    0,
    output_183,
    0,
    output_185,
    0,
    output_187,
    0,
    output_189,
    0,
    output_191,
    0,
    output_193,
    0,
    output_195,
    0,
    0,
    0,
    output_199,
    0,
    0,
    0,
    output_203,
    0,
    0,
    0,
    output_207,
    0,
    0,
    output_210,
    output_211,
    output_212,
    0,
    output_214,
    0,
    output_216,
    0,
    output_218,
    0,
    0,
    output_221,
    0,
    output_223,
    output_224,
    0,
    output_226,
    0,
    0,
    output_229,
    0,
    0,
    output_232,
    0,
    0,
    output_235,
    0,
    0,
    output_238,
    output_239,
    0,
    0,
    output_242,
    0,
    0,
    output_245,
    0,
    output_247,
    output_248,
    0,
    output_250,
    0,
    output_252,
    output_253,
    output_254,
    output_255,
    output_256,
    output_257,
    output_258,
    output_259,
    output_260,
    0,
    output_262,
  };

rtx (*const insn_gen_function[]) () =
  {
    gen_tstsi_1,
    gen_tstsi,
    gen_tsthi_1,
    gen_tsthi,
    gen_tstqi_1,
    gen_tstqi,
    gen_tstsf_cc,
    gen_tstsf,
    gen_tstdf_cc,
    gen_tstdf,
    gen_cmpsi_1,
    gen_cmpsi,
    gen_cmphi_1,
    gen_cmphi,
    gen_cmpqi_1,
    gen_cmpqi,
    0,
    0,
    0,
    0,
    0,
    0,
    gen_cmpsf_cc_1,
    0,
    0,
    0,
    gen_cmpdf,
    gen_cmpsf,
    gen_cmpdf_cc,
    gen_cmpdf_ccfpeq,
    gen_cmpsf_cc,
    gen_cmpsf_ccfpeq,
    0,
    0,
    0,
    0,
    0,
    gen_movsi,
    0,
    0,
    gen_movhi,
    gen_movstricthi,
    0,
    gen_movqi,
    gen_movstrictqi,
    0,
    gen_movsf,
    0,
    gen_swapdf,
    gen_movdf,
    0,
    gen_movdi,
    gen_zero_extendhisi2,
    gen_zero_extendqihi2,
    gen_zero_extendqisi2,
    gen_zero_extendsidi2,
    gen_extendsidi2,
    gen_extendhisi2,
    gen_extendqihi2,
    gen_extendqisi2,
    gen_extendsfdf2,
    gen_truncdfsf2,
    0,
    gen_fixuns_truncdfsi2,
    gen_fixuns_truncsfsi2,
    gen_fix_truncdfdi2,
    gen_fix_truncsfdi2,
    0,
    0,
    gen_fix_truncdfsi2,
    gen_fix_truncsfsi2,
    0,
    0,
    gen_floatsisf2,
    gen_floatdisf2,
    gen_floatsidf2,
    gen_floatdidf2,
    0,
    0,
    0,
    0,
    gen_adddi3,
    gen_addsi3,
    gen_addhi3,
    gen_addqi3,
    0,
    gen_adddf3,
    gen_addsf3,
    gen_subdi3,
    gen_subsi3,
    gen_subhi3,
    gen_subqi3,
    gen_subdf3,
    gen_subsf3,
    0,
    gen_mulhi3,
    0,
    gen_mulsi3,
    0,
    gen_muldf3,
    gen_mulsf3,
    gen_divqi3,
    gen_udivqi3,
    gen_divdf3,
    gen_divsf3,
    gen_divmodsi4,
    gen_divmodhi4,
    gen_udivmodsi4,
    gen_udivmodhi4,
    gen_andsi3,
    gen_andhi3,
    gen_andqi3,
    gen_iorsi3,
    gen_iorhi3,
    gen_iorqi3,
    gen_xorsi3,
    gen_xorhi3,
    gen_xorqi3,
    gen_negdi2,
    gen_negsi2,
    gen_neghi2,
    gen_negqi2,
    gen_negsf2,
    gen_negdf2,
    0,
    gen_abssf2,
    gen_absdf2,
    0,
    gen_sqrtsf2,
    gen_sqrtdf2,
    0,
    gen_sindf2,
    gen_sinsf2,
    0,
    gen_cosdf2,
    gen_cossf2,
    0,
    gen_one_cmplsi2,
    gen_one_cmplhi2,
    gen_one_cmplqi2,
    gen_ashldi3,
    gen_ashldi3_const_int,
    gen_ashldi3_non_const_int,
    gen_ashlsi3,
    gen_ashlhi3,
    gen_ashlqi3,
    gen_ashrdi3,
    gen_ashrdi3_const_int,
    gen_ashrdi3_non_const_int,
    gen_ashrsi3,
    gen_ashrhi3,
    gen_ashrqi3,
    gen_lshrdi3,
    gen_lshrdi3_const_int,
    gen_lshrdi3_non_const_int,
    gen_lshrsi3,
    gen_lshrhi3,
    gen_lshrqi3,
    gen_rotlsi3,
    gen_rotlhi3,
    gen_rotlqi3,
    gen_rotrsi3,
    gen_rotrhi3,
    gen_rotrqi3,
    0,
    0,
    0,
    0,
    0,
    0,
    gen_seq,
    0,
    gen_sne,
    0,
    gen_sgt,
    0,
    gen_sgtu,
    0,
    gen_slt,
    0,
    gen_sltu,
    0,
    gen_sge,
    0,
    gen_sgeu,
    0,
    gen_sle,
    0,
    gen_sleu,
    0,
    gen_beq,
    0,
    gen_bne,
    0,
    gen_bgt,
    0,
    gen_bgtu,
    0,
    gen_blt,
    0,
    gen_bltu,
    0,
    gen_bge,
    0,
    gen_bgeu,
    0,
    gen_ble,
    0,
    gen_bleu,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    gen_jump,
    gen_indirect_jump,
    gen_casesi,
    0,
    gen_tablejump,
    gen_call_pop,
    0,
    0,
    gen_call,
    0,
    0,
    gen_call_value_pop,
    0,
    0,
    gen_call_value,
    0,
    0,
    gen_untyped_call,
    0,
    0,
    gen_untyped_return,
    gen_update_return,
    gen_return,
    gen_nop,
    gen_movstrsi,
    0,
    gen_cmpstrsi,
    0,
    0,
    gen_ffssi2,
    0,
    gen_ffshi2,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    gen_strlensi,
    0,
  };

char *insn_name[] =
  {
    "tstsi_1",
    "tstsi",
    "tsthi_1",
    "tsthi",
    "tstqi_1",
    "tstqi",
    "tstsf_cc",
    "tstsf",
    "tstdf_cc",
    "tstdf",
    "cmpsi_1",
    "cmpsi",
    "cmphi_1",
    "cmphi",
    "cmpqi_1",
    "cmpqi",
    "cmpqi+1",
    "cmpqi+2",
    "cmpqi+3",
    "cmpsf_cc_1-3",
    "cmpsf_cc_1-2",
    "cmpsf_cc_1-1",
    "cmpsf_cc_1",
    "cmpsf_cc_1+1",
    "cmpsf_cc_1+2",
    "cmpdf-1",
    "cmpdf",
    "cmpsf",
    "cmpdf_cc",
    "cmpdf_ccfpeq",
    "cmpsf_cc",
    "cmpsf_ccfpeq",
    "cmpsf_ccfpeq+1",
    "cmpsf_ccfpeq+2",
    "cmpsf_ccfpeq+3",
    "movsi-2",
    "movsi-1",
    "movsi",
    "movsi+1",
    "movhi-1",
    "movhi",
    "movstricthi",
    "movstricthi+1",
    "movqi",
    "movstrictqi",
    "movstrictqi+1",
    "movsf",
    "movsf+1",
    "swapdf",
    "movdf",
    "movdf+1",
    "movdi",
    "zero_extendhisi2",
    "zero_extendqihi2",
    "zero_extendqisi2",
    "zero_extendsidi2",
    "extendsidi2",
    "extendhisi2",
    "extendqihi2",
    "extendqisi2",
    "extendsfdf2",
    "truncdfsf2",
    "truncdfsf2+1",
    "fixuns_truncdfsi2",
    "fixuns_truncsfsi2",
    "fix_truncdfdi2",
    "fix_truncsfdi2",
    "fix_truncsfdi2+1",
    "fix_truncdfsi2-1",
    "fix_truncdfsi2",
    "fix_truncsfsi2",
    "fix_truncsfsi2+1",
    "floatsisf2-1",
    "floatsisf2",
    "floatdisf2",
    "floatsidf2",
    "floatdidf2",
    "floatdidf2+1",
    "floatdidf2+2",
    "adddi3-2",
    "adddi3-1",
    "adddi3",
    "addsi3",
    "addhi3",
    "addqi3",
    "addqi3+1",
    "adddf3",
    "addsf3",
    "subdi3",
    "subsi3",
    "subhi3",
    "subqi3",
    "subdf3",
    "subsf3",
    "subsf3+1",
    "mulhi3",
    "mulhi3+1",
    "mulsi3",
    "mulsi3+1",
    "muldf3",
    "mulsf3",
    "divqi3",
    "udivqi3",
    "divdf3",
    "divsf3",
    "divmodsi4",
    "divmodhi4",
    "udivmodsi4",
    "udivmodhi4",
    "andsi3",
    "andhi3",
    "andqi3",
    "iorsi3",
    "iorhi3",
    "iorqi3",
    "xorsi3",
    "xorhi3",
    "xorqi3",
    "negdi2",
    "negsi2",
    "neghi2",
    "negqi2",
    "negsf2",
    "negdf2",
    "negdf2+1",
    "abssf2",
    "absdf2",
    "absdf2+1",
    "sqrtsf2",
    "sqrtdf2",
    "sqrtdf2+1",
    "sindf2",
    "sinsf2",
    "sinsf2+1",
    "cosdf2",
    "cossf2",
    "cossf2+1",
    "one_cmplsi2",
    "one_cmplhi2",
    "one_cmplqi2",
    "ashldi3",
    "ashldi3_const_int",
    "ashldi3_non_const_int",
    "ashlsi3",
    "ashlhi3",
    "ashlqi3",
    "ashrdi3",
    "ashrdi3_const_int",
    "ashrdi3_non_const_int",
    "ashrsi3",
    "ashrhi3",
    "ashrqi3",
    "lshrdi3",
    "lshrdi3_const_int",
    "lshrdi3_non_const_int",
    "lshrsi3",
    "lshrhi3",
    "lshrqi3",
    "rotlsi3",
    "rotlhi3",
    "rotlqi3",
    "rotrsi3",
    "rotrhi3",
    "rotrqi3",
    "rotrqi3+1",
    "rotrqi3+2",
    "rotrqi3+3",
    "seq-3",
    "seq-2",
    "seq-1",
    "seq",
    "seq+1",
    "sne",
    "sne+1",
    "sgt",
    "sgt+1",
    "sgtu",
    "sgtu+1",
    "slt",
    "slt+1",
    "sltu",
    "sltu+1",
    "sge",
    "sge+1",
    "sgeu",
    "sgeu+1",
    "sle",
    "sle+1",
    "sleu",
    "sleu+1",
    "beq",
    "beq+1",
    "bne",
    "bne+1",
    "bgt",
    "bgt+1",
    "bgtu",
    "bgtu+1",
    "blt",
    "blt+1",
    "bltu",
    "bltu+1",
    "bge",
    "bge+1",
    "bgeu",
    "bgeu+1",
    "ble",
    "ble+1",
    "bleu",
    "bleu+1",
    "bleu+2",
    "bleu+3",
    "bleu+4",
    "bleu+5",
    "bleu+6",
    "jump-5",
    "jump-4",
    "jump-3",
    "jump-2",
    "jump-1",
    "jump",
    "indirect_jump",
    "casesi",
    "casesi+1",
    "tablejump",
    "call_pop",
    "call_pop+1",
    "call-1",
    "call",
    "call+1",
    "call_value_pop-1",
    "call_value_pop",
    "call_value_pop+1",
    "call_value-1",
    "call_value",
    "call_value+1",
    "untyped_call-1",
    "untyped_call",
    "untyped_call+1",
    "untyped_return-1",
    "untyped_return",
    "update_return",
    "return",
    "nop",
    "movstrsi",
    "movstrsi+1",
    "cmpstrsi",
    "cmpstrsi+1",
    "ffssi2-1",
    "ffssi2",
    "ffssi2+1",
    "ffshi2",
    "ffshi2+1",
    "ffshi2+2",
    "ffshi2+3",
    "ffshi2+4",
    "ffshi2+5",
    "strlensi-4",
    "strlensi-3",
    "strlensi-2",
    "strlensi-1",
    "strlensi",
    "strlensi+1",
  };
char **insn_name_ptr = insn_name;

const int insn_n_operands[] =
  {
    1,
    1,
    1,
    1,
    1,
    1,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    4,
    4,
    4,
    4,
    4,
    3,
    4,
    4,
    4,
    3,
    2,
    2,
    3,
    3,
    3,
    3,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    3,
    8,
    8,
    6,
    6,
    5,
    5,
    5,
    5,
    5,
    5,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    3,
    3,
    3,
    3,
    2,
    3,
    3,
    3,
    3,
    3,
    3,
    3,
    3,
    3,
    3,
    3,
    3,
    3,
    3,
    3,
    3,
    3,
    3,
    3,
    4,
    4,
    4,
    4,
    3,
    3,
    3,
    3,
    3,
    3,
    3,
    3,
    3,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    3,
    3,
    3,
    3,
    3,
    3,
    3,
    3,
    3,
    3,
    3,
    3,
    3,
    3,
    3,
    3,
    3,
    3,
    3,
    3,
    3,
    3,
    3,
    3,
    4,
    3,
    3,
    2,
    3,
    3,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    1,
    7,
    3,
    1,
    4,
    4,
    4,
    2,
    2,
    2,
    5,
    5,
    5,
    3,
    3,
    3,
    3,
    3,
    3,
    2,
    1,
    0,
    0,
    5,
    5,
    5,
    5,
    4,
    3,
    3,
    3,
    3,
    4,
    4,
    4,
    4,
    4,
    4,
    4,
    4,
    4,
    4,
  };

const int insn_n_dups[] =
  {
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    2,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    1,
    0,
    7,
    7,
    5,
    5,
    1,
    1,
    2,
    2,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    2,
    2,
    2,
    2,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    1,
    0,
    0,
    0,
    0,
    0,
    1,
    0,
    0,
    0,
    0,
    0,
    1,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    1,
    0,
    1,
    0,
    1,
    0,
    1,
    0,
    1,
    0,
    1,
    0,
    1,
    0,
    1,
    0,
    1,
    0,
    1,
    0,
    1,
    0,
    1,
    0,
    1,
    0,
    1,
    0,
    1,
    0,
    1,
    0,
    1,
    0,
    1,
    0,
    1,
    0,
    1,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    3,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    2,
    2,
    3,
    3,
    3,
    2,
    0,
    2,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    5,
    1,
  };

char *const insn_operand_constraint[][MAX_RECOG_OPERANDS] =
  {
    { "rm", },
    { "", },
    { "rm", },
    { "", },
    { "qm", },
    { "", },
    { "f", "=a", },
    { "", "", },
    { "f", "=a", },
    { "", "", },
    { "mr,r", "ri,mr", },
    { "", "", },
    { "mr,r", "ri,mr", },
    { "", "", },
    { "q,mq", "qm,nq", },
    { "", "", },
    { "f,fm", "fm,f", "", "=a,a", },
    { "f", "rm", "", "=a", },
    { "rm", "f", "", "=a", },
    { "f", "fm", "", "=a", },
    { "fm", "f", "", "=a", },
    { "f", "f", "=a", },
    { "f,fm", "fm,f", "", "=a,a", },
    { "f", "rm", "", "=a", },
    { "rm", "f", "", "=a", },
    { "f", "f", "=a", },
    { "", "", },
    { "", "", },
    { "", "", "", },
    { "", "", "", },
    { "", "", "", },
    { "", "", "", },
    { "%ro", "ri", },
    { "%ro", "ri", },
    { "%qm", "qi", },
    { "=<", "g", },
    { "=<", "ri", },
    { "", "", },
    { "=g,r", "ri,m", },
    { "=<", "g", },
    { "=g,r", "ri,m", },
    { "+g,r", "ri,m", },
    { "=<", "q", },
    { "=q,*r,qm", "*g,q,qn", },
    { "+q,qm", "*g,qn", },
    { "=<,<", "gF,f", },
    { "=*rfm,*rf,f,!*rm", "*rf,*rfm,fG,fF", },
    { "=<,<", "gF,f", },
    { "f", "f", },
    { "=*rfm,*rf,f,!*rm", "*rf,*rfm,fG,fF", },
    { "=<", "roiF", },
    { "=r,rm", "m,riF", },
    { "=r", "rm", },
    { "=r", "qm", },
    { "=r", "qm", },
    { "=r", "0", },
    { "=r", "0", },
    { "=r", "rm", },
    { "=r", "qm", },
    { "=r", "qm", },
    { "=fm,f", "f,fm", },
    { "", "", },
    { "=f,m", "0,f", "m,m", },
    { "", "", "", "", "", "", "", "", },
    { "", "", "", "", "", "", "", "", },
    { "", "", "", "", "", "", },
    { "", "", "", "", "", "", },
    { "=rm", "f", "m", "m", "=&q", },
    { "=rm", "f", "m", "m", "=&q", },
    { "", "", "", "", "", },
    { "", "", "", "", "", },
    { "=rm", "f", "m", "m", "=&q", },
    { "=rm", "f", "m", "m", "=&q", },
    { "", "", },
    { "", "", },
    { "", "", },
    { "", "", },
    { "=f", "rm", },
    { "=f", "rm", },
    { "=f", "rm", },
    { "=f", "rm", },
    { "=&r,ro", "%0,0", "o,riF", },
    { "=?r,rm,r", "%r,0,0", "ri,ri,rm", },
    { "=rm,r", "%0,0", "ri,rm", },
    { "=qm,q", "%0,0", "qn,qmn", },
    { "=r", "p", },
    { "", "", "", },
    { "", "", "", },
    { "=&r,ro", "0,0", "o,riF", },
    { "=rm,r", "0,0", "ri,rm", },
    { "=rm,r", "0,0", "ri,rm", },
    { "=qm,q", "0,0", "qn,qmn", },
    { "", "", "", },
    { "", "", "", },
    { "=r", "%0", "r", },
    { "=r,r", "%0,rm", "g,i", },
    { "=r", "%0", "r", },
    { "=r,r", "%0,rm", "g,i", },
    { "=a", "%0", "qm", },
    { "", "", "", },
    { "", "", "", },
    { "=a", "0", "qm", },
    { "=a", "0", "qm", },
    { "", "", "", },
    { "", "", "", },
    { "=a", "0", "rm", "=&d", },
    { "=a", "0", "rm", "=&d", },
    { "=a", "0", "rm", "=&d", },
    { "=a", "0", "rm", "=&d", },
    { "=r,r,rm,r", "%rm,qm,0,0", "L,K,ri,rm", },
    { "=rm,r", "%0,0", "ri,rm", },
    { "=qm,q", "%0,0", "qn,qmn", },
    { "=rm,r", "%0,0", "ri,rm", },
    { "=rm,r", "%0,0", "ri,rm", },
    { "=qm,q", "%0,0", "qn,qmn", },
    { "=rm,r", "%0,0", "ri,rm", },
    { "=rm,r", "%0,0", "ri,rm", },
    { "=qm,q", "%0,0", "qn,qm", },
    { "=&ro", "0", },
    { "=rm", "0", },
    { "=rm", "0", },
    { "=qm", "0", },
    { "=f", "0", },
    { "=f", "0", },
    { "=f", "0", },
    { "=f", "0", },
    { "=f", "0", },
    { "=f", "0", },
    { "=f", "0", },
    { "=f", "0", },
    { "=f", "0", },
    { "=f", "0", },
    { "=f", "0", },
    { "=f", "0", },
    { "=f", "0", },
    { "=f", "0", },
    { "=f", "0", },
    { "=rm", "0", },
    { "=rm", "0", },
    { "=qm", "0", },
    { "", "", "", },
    { "=&r", "0", "J", },
    { "=&r", "0", "c", },
    { "=r,rm", "r,0", "M,cI", },
    { "=rm", "0", "cI", },
    { "=qm", "0", "cI", },
    { "", "", "", },
    { "=&r", "0", "J", },
    { "=&r", "0", "c", },
    { "=rm", "0", "cI", },
    { "=rm", "0", "cI", },
    { "=qm", "0", "cI", },
    { "", "", "", },
    { "=&r", "0", "J", },
    { "=&r", "0", "c", },
    { "=rm", "0", "cI", },
    { "=rm", "0", "cI", },
    { "=qm", "0", "cI", },
    { "=rm", "0", "cI", },
    { "=rm", "0", "cI", },
    { "=qm", "0", "cI", },
    { "=rm", "0", "cI", },
    { "=rm", "0", "cI", },
    { "=qm", "0", "cI", },
    { "+rm", "", "r", "n", },
    { "=rm", "r", "0", },
    { "=rm", "0", "r", },
    { "r", "r", },
    { "r", "n", "n", },
    { "rm", "n", "n", },
    { "", },
    { "=q", },
    { "", },
    { "=q", },
    { "", },
    { "=q", },
    { "", },
    { "=q", },
    { "", },
    { "=q", },
    { "", },
    { "=q", },
    { "", },
    { "=q", },
    { "", },
    { "=q", },
    { "", },
    { "=q", },
    { "", },
    { "=q", },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { "rm", },
    { "", "", "", "", "", "", "", },
    { "r", "", "=&r", },
    { "rm", },
    { "", "", "", "", },
    { "m", "g", "", "i", },
    { "", "g", "", "i", },
    { "", "", },
    { "m", "g", },
    { "", "g", },
    { "", "", "", "", "", },
    { "=rf", "m", "g", "", "i", },
    { "=rf", "", "g", "", "i", },
    { "", "", "", },
    { "=rf", "m", "g", },
    { "=rf", "", "g", },
    { "", "", "", },
    { "m", "o", "", },
    { "", "o", "", },
    { "", "", },
    { "m", },
    { 0 },
    { 0 },
    { "", "", "", "", "", },
    { "D", "S", "n", "i", "=&c", },
    { "", "", "", "", "", },
    { "=&r", "S", "D", "c", "i", },
    { "S", "D", "c", "i", },
    { "", "", "", },
    { "=&r", "rm", "=r", },
    { "", "", "", },
    { "=&r", "rm", "=r", },
    { "=f,f", "0,fm", "fm,0", "", },
    { "=f", "rm", "0", "", },
    { "=f,f", "fm,0", "0,f", "", },
    { "=f", "0", "rm", "", },
    { "=f,f", "0,f", "fm,0", "", },
    { "=f,f", "0,fm", "fm,0", "", },
    { "=f", "rm", "0", "", },
    { "=f", "0", "rm", "", },
    { "", "", "", "", },
    { "=&c", "D", "a", "i", },
  };

const enum machine_mode insn_operand_mode[][MAX_RECOG_OPERANDS] =
  {
    { SImode, },
    { SImode, },
    { HImode, },
    { HImode, },
    { QImode, },
    { QImode, },
    { SFmode, HImode, },
    { SFmode, HImode, },
    { DFmode, HImode, },
    { DFmode, HImode, },
    { SImode, SImode, },
    { SImode, SImode, },
    { HImode, HImode, },
    { HImode, HImode, },
    { QImode, QImode, },
    { QImode, QImode, },
    { DFmode, DFmode, VOIDmode, HImode, },
    { DFmode, SImode, VOIDmode, HImode, },
    { SImode, DFmode, VOIDmode, HImode, },
    { DFmode, SFmode, VOIDmode, HImode, },
    { SFmode, DFmode, VOIDmode, HImode, },
    { DFmode, DFmode, HImode, },
    { SFmode, SFmode, VOIDmode, HImode, },
    { SFmode, SImode, VOIDmode, HImode, },
    { SImode, SFmode, VOIDmode, HImode, },
    { SFmode, SFmode, HImode, },
    { DFmode, DFmode, },
    { SFmode, SFmode, },
    { DFmode, DFmode, HImode, },
    { DFmode, DFmode, HImode, },
    { SFmode, SFmode, HImode, },
    { SFmode, SFmode, HImode, },
    { SImode, SImode, },
    { HImode, HImode, },
    { QImode, QImode, },
    { SImode, SImode, },
    { SImode, SImode, },
    { SImode, SImode, },
    { SImode, SImode, },
    { HImode, HImode, },
    { HImode, HImode, },
    { HImode, HImode, },
    { QImode, QImode, },
    { QImode, QImode, },
    { QImode, QImode, },
    { SFmode, SFmode, },
    { SFmode, SFmode, },
    { DFmode, DFmode, },
    { DFmode, DFmode, },
    { DFmode, DFmode, },
    { DImode, DImode, },
    { DImode, DImode, },
    { SImode, HImode, },
    { HImode, QImode, },
    { SImode, QImode, },
    { DImode, SImode, },
    { DImode, SImode, },
    { SImode, HImode, },
    { HImode, QImode, },
    { SImode, QImode, },
    { DFmode, SFmode, },
    { SFmode, DFmode, },
    { SFmode, DFmode, SFmode, },
    { SImode, DFmode, VOIDmode, VOIDmode, VOIDmode, VOIDmode, VOIDmode, SImode, },
    { SImode, SFmode, VOIDmode, VOIDmode, VOIDmode, VOIDmode, VOIDmode, SImode, },
    { DImode, DFmode, VOIDmode, VOIDmode, VOIDmode, SImode, },
    { DImode, SFmode, VOIDmode, VOIDmode, VOIDmode, SImode, },
    { DImode, DFmode, SImode, SImode, SImode, },
    { DImode, SFmode, SImode, SImode, SImode, },
    { SImode, DFmode, VOIDmode, VOIDmode, SImode, },
    { SImode, SFmode, VOIDmode, VOIDmode, SImode, },
    { SImode, DFmode, SImode, SImode, SImode, },
    { SImode, SFmode, SImode, SImode, SImode, },
    { SFmode, SImode, },
    { SFmode, DImode, },
    { DFmode, SImode, },
    { DFmode, DImode, },
    { DFmode, DImode, },
    { SFmode, DImode, },
    { DFmode, SImode, },
    { SFmode, SImode, },
    { DImode, DImode, DImode, },
    { SImode, SImode, SImode, },
    { HImode, HImode, HImode, },
    { QImode, QImode, QImode, },
    { SImode, QImode, },
    { DFmode, DFmode, DFmode, },
    { SFmode, SFmode, SFmode, },
    { DImode, DImode, DImode, },
    { SImode, SImode, SImode, },
    { HImode, HImode, HImode, },
    { QImode, QImode, QImode, },
    { DFmode, DFmode, DFmode, },
    { SFmode, SFmode, SFmode, },
    { HImode, HImode, HImode, },
    { HImode, HImode, HImode, },
    { SImode, SImode, SImode, },
    { SImode, SImode, SImode, },
    { HImode, QImode, QImode, },
    { DFmode, DFmode, DFmode, },
    { SFmode, SFmode, SFmode, },
    { QImode, HImode, QImode, },
    { QImode, HImode, QImode, },
    { DFmode, DFmode, DFmode, },
    { SFmode, SFmode, SFmode, },
    { SImode, SImode, SImode, SImode, },
    { HImode, HImode, HImode, HImode, },
    { SImode, SImode, SImode, SImode, },
    { HImode, HImode, HImode, HImode, },
    { SImode, SImode, SImode, },
    { HImode, HImode, HImode, },
    { QImode, QImode, QImode, },
    { SImode, SImode, SImode, },
    { HImode, HImode, HImode, },
    { QImode, QImode, QImode, },
    { SImode, SImode, SImode, },
    { HImode, HImode, HImode, },
    { QImode, QImode, QImode, },
    { DImode, DImode, },
    { SImode, SImode, },
    { HImode, HImode, },
    { QImode, QImode, },
    { SFmode, SFmode, },
    { DFmode, DFmode, },
    { DFmode, SFmode, },
    { SFmode, SFmode, },
    { DFmode, DFmode, },
    { DFmode, SFmode, },
    { SFmode, SFmode, },
    { DFmode, DFmode, },
    { DFmode, SFmode, },
    { DFmode, DFmode, },
    { SFmode, SFmode, },
    { DFmode, SFmode, },
    { DFmode, DFmode, },
    { SFmode, SFmode, },
    { DFmode, SFmode, },
    { SImode, SImode, },
    { HImode, HImode, },
    { QImode, QImode, },
    { DImode, DImode, QImode, },
    { DImode, DImode, QImode, },
    { DImode, DImode, QImode, },
    { SImode, SImode, SImode, },
    { HImode, HImode, HImode, },
    { QImode, QImode, QImode, },
    { DImode, DImode, QImode, },
    { DImode, DImode, QImode, },
    { DImode, DImode, QImode, },
    { SImode, SImode, SImode, },
    { HImode, HImode, HImode, },
    { QImode, QImode, QImode, },
    { DImode, DImode, QImode, },
    { DImode, DImode, QImode, },
    { DImode, DImode, QImode, },
    { SImode, SImode, SImode, },
    { HImode, HImode, HImode, },
    { QImode, QImode, QImode, },
    { SImode, SImode, SImode, },
    { HImode, HImode, HImode, },
    { QImode, QImode, QImode, },
    { SImode, SImode, SImode, },
    { HImode, HImode, HImode, },
    { QImode, QImode, QImode, },
    { SImode, VOIDmode, SImode, SImode, },
    { SImode, SImode, SImode, },
    { SImode, SImode, SImode, },
    { SImode, SImode, },
    { SImode, SImode, SImode, },
    { QImode, SImode, SImode, },
    { QImode, },
    { QImode, },
    { QImode, },
    { QImode, },
    { QImode, },
    { QImode, },
    { QImode, },
    { QImode, },
    { QImode, },
    { QImode, },
    { QImode, },
    { QImode, },
    { QImode, },
    { QImode, },
    { QImode, },
    { QImode, },
    { QImode, },
    { QImode, },
    { QImode, },
    { QImode, },
    { VOIDmode },
    { VOIDmode },
    { VOIDmode },
    { VOIDmode },
    { VOIDmode },
    { VOIDmode },
    { VOIDmode },
    { VOIDmode },
    { VOIDmode },
    { VOIDmode },
    { VOIDmode },
    { VOIDmode },
    { VOIDmode },
    { VOIDmode },
    { VOIDmode },
    { VOIDmode },
    { VOIDmode },
    { VOIDmode },
    { VOIDmode },
    { VOIDmode },
    { VOIDmode },
    { VOIDmode },
    { VOIDmode },
    { VOIDmode },
    { VOIDmode },
    { VOIDmode },
    { VOIDmode },
    { VOIDmode },
    { VOIDmode },
    { VOIDmode },
    { VOIDmode },
    { SImode, },
    { SImode, SImode, SImode, VOIDmode, VOIDmode, VOIDmode, SImode, },
    { SImode, VOIDmode, SImode, },
    { SImode, },
    { QImode, SImode, VOIDmode, SImode, },
    { QImode, SImode, VOIDmode, SImode, },
    { SImode, SImode, VOIDmode, SImode, },
    { QImode, SImode, },
    { QImode, SImode, },
    { SImode, SImode, },
    { VOIDmode, QImode, SImode, VOIDmode, SImode, },
    { VOIDmode, QImode, SImode, VOIDmode, SImode, },
    { VOIDmode, SImode, SImode, VOIDmode, SImode, },
    { VOIDmode, QImode, SImode, },
    { VOIDmode, QImode, SImode, },
    { VOIDmode, SImode, SImode, },
    { QImode, BLKmode, VOIDmode, },
    { QImode, DImode, VOIDmode, },
    { SImode, DImode, VOIDmode, },
    { BLKmode, VOIDmode, },
    { SImode, },
    { VOIDmode },
    { VOIDmode },
    { BLKmode, BLKmode, SImode, SImode, SImode, },
    { SImode, SImode, SImode, SImode, SImode, },
    { SImode, BLKmode, BLKmode, SImode, SImode, },
    { SImode, SImode, SImode, SImode, SImode, },
    { SImode, SImode, SImode, SImode, },
    { SImode, SImode, SImode, },
    { SImode, SImode, SImode, },
    { HImode, HImode, HImode, },
    { HImode, HImode, HImode, },
    { DFmode, DFmode, DFmode, DFmode, },
    { DFmode, SImode, DFmode, DFmode, },
    { DFmode, SFmode, DFmode, DFmode, },
    { DFmode, DFmode, SImode, DFmode, },
    { DFmode, DFmode, SFmode, DFmode, },
    { SFmode, SFmode, SFmode, SFmode, },
    { SFmode, SImode, SFmode, SFmode, },
    { SFmode, SFmode, SImode, SFmode, },
    { SImode, BLKmode, QImode, SImode, },
    { SImode, SImode, QImode, SImode, },
  };

const char insn_operand_strict_low[][MAX_RECOG_OPERANDS] =
  {
    { 0, },
    { 0, },
    { 0, },
    { 0, },
    { 0, },
    { 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, 0, 0, },
    { 0, 0, 0, 0, },
    { 0, 0, 0, 0, },
    { 0, 0, 0, 0, },
    { 0, 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, 0, },
    { 0, 0, 0, 0, },
    { 0, 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 1, 0, },
    { 0, 0, },
    { 0, 0, },
    { 1, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, 0, 0, 0, 0, 0, },
    { 0, 0, 0, 0, 0, 0, 0, 0, },
    { 0, 0, 0, 0, 0, 0, },
    { 0, 0, 0, 0, 0, 0, },
    { 0, 0, 0, 0, 0, },
    { 0, 0, 0, 0, 0, },
    { 0, 0, 0, 0, 0, },
    { 0, 0, 0, 0, 0, },
    { 0, 0, 0, 0, 0, },
    { 0, 0, 0, 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, 0, },
    { 0, 0, 0, 0, },
    { 0, 0, 0, 0, },
    { 0, 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, },
    { 0, },
    { 0, },
    { 0, },
    { 0, },
    { 0, },
    { 0, },
    { 0, },
    { 0, },
    { 0, },
    { 0, },
    { 0, },
    { 0, },
    { 0, },
    { 0, },
    { 0, },
    { 0, },
    { 0, },
    { 0, },
    { 0, },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0, },
    { 0, 0, 0, 0, 0, 0, 0, },
    { 0, 0, 0, },
    { 0, },
    { 0, 0, 0, 0, },
    { 0, 0, 0, 0, },
    { 0, 0, 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, },
    { 0, 0, 0, 0, 0, },
    { 0, 0, 0, 0, 0, },
    { 0, 0, 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, },
    { 0, },
    { 0 },
    { 0 },
    { 0, 0, 0, 0, 0, },
    { 0, 0, 0, 0, 0, },
    { 0, 0, 0, 0, 0, },
    { 0, 0, 0, 0, 0, },
    { 0, 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, },
    { 0, 0, 0, 0, },
    { 0, 0, 0, 0, },
    { 0, 0, 0, 0, },
    { 0, 0, 0, 0, },
    { 0, 0, 0, 0, },
    { 0, 0, 0, 0, },
    { 0, 0, 0, 0, },
    { 0, 0, 0, 0, },
    { 0, 0, 0, 0, },
    { 0, 0, 0, 0, },
  };

extern int nonimmediate_operand ();
extern int register_operand ();
extern int scratch_operand ();
extern int general_operand ();
extern int VOIDmode_compare_op ();
extern int push_operand ();
extern int memory_operand ();
extern int address_operand ();
extern int nonmemory_operand ();
extern int const_int_operand ();
extern int indirect_operand ();
extern int immediate_operand ();
extern int call_insn_operand ();
extern int symbolic_operand ();
extern int binary_387_op ();

int (*const insn_operand_predicate[][MAX_RECOG_OPERANDS])() =
  {
    { nonimmediate_operand, },
    { nonimmediate_operand, },
    { nonimmediate_operand, },
    { nonimmediate_operand, },
    { nonimmediate_operand, },
    { nonimmediate_operand, },
    { register_operand, scratch_operand, },
    { register_operand, scratch_operand, },
    { register_operand, scratch_operand, },
    { register_operand, scratch_operand, },
    { nonimmediate_operand, general_operand, },
    { nonimmediate_operand, general_operand, },
    { nonimmediate_operand, general_operand, },
    { nonimmediate_operand, general_operand, },
    { nonimmediate_operand, general_operand, },
    { nonimmediate_operand, general_operand, },
    { nonimmediate_operand, nonimmediate_operand, VOIDmode_compare_op, scratch_operand, },
    { register_operand, nonimmediate_operand, VOIDmode_compare_op, scratch_operand, },
    { nonimmediate_operand, register_operand, VOIDmode_compare_op, scratch_operand, },
    { register_operand, nonimmediate_operand, VOIDmode_compare_op, scratch_operand, },
    { nonimmediate_operand, register_operand, VOIDmode_compare_op, scratch_operand, },
    { register_operand, register_operand, scratch_operand, },
    { nonimmediate_operand, nonimmediate_operand, VOIDmode_compare_op, scratch_operand, },
    { register_operand, nonimmediate_operand, VOIDmode_compare_op, scratch_operand, },
    { nonimmediate_operand, register_operand, VOIDmode_compare_op, scratch_operand, },
    { register_operand, register_operand, scratch_operand, },
    { register_operand, nonimmediate_operand, },
    { register_operand, nonimmediate_operand, },
    { register_operand, register_operand, scratch_operand, },
    { register_operand, register_operand, scratch_operand, },
    { register_operand, register_operand, scratch_operand, },
    { register_operand, register_operand, scratch_operand, },
    { general_operand, general_operand, },
    { general_operand, general_operand, },
    { general_operand, general_operand, },
    { push_operand, general_operand, },
    { push_operand, general_operand, },
    { general_operand, general_operand, },
    { general_operand, general_operand, },
    { push_operand, general_operand, },
    { general_operand, general_operand, },
    { general_operand, general_operand, },
    { push_operand, general_operand, },
    { general_operand, general_operand, },
    { general_operand, general_operand, },
    { push_operand, general_operand, },
    { general_operand, general_operand, },
    { push_operand, general_operand, },
    { register_operand, register_operand, },
    { general_operand, general_operand, },
    { push_operand, general_operand, },
    { general_operand, general_operand, },
    { general_operand, nonimmediate_operand, },
    { general_operand, nonimmediate_operand, },
    { general_operand, nonimmediate_operand, },
    { register_operand, register_operand, },
    { register_operand, register_operand, },
    { general_operand, nonimmediate_operand, },
    { general_operand, nonimmediate_operand, },
    { general_operand, nonimmediate_operand, },
    { general_operand, general_operand, },
    { nonimmediate_operand, register_operand, },
    { nonimmediate_operand, register_operand, memory_operand, },
    { general_operand, register_operand, 0, 0, 0, 0, 0, scratch_operand, },
    { general_operand, register_operand, 0, 0, 0, 0, 0, scratch_operand, },
    { general_operand, register_operand, 0, 0, 0, scratch_operand, },
    { general_operand, register_operand, 0, 0, 0, scratch_operand, },
    { general_operand, register_operand, memory_operand, memory_operand, scratch_operand, },
    { general_operand, register_operand, memory_operand, memory_operand, scratch_operand, },
    { general_operand, register_operand, 0, 0, scratch_operand, },
    { general_operand, register_operand, 0, 0, scratch_operand, },
    { general_operand, register_operand, memory_operand, memory_operand, scratch_operand, },
    { general_operand, register_operand, memory_operand, memory_operand, scratch_operand, },
    { register_operand, nonimmediate_operand, },
    { register_operand, nonimmediate_operand, },
    { register_operand, nonimmediate_operand, },
    { register_operand, nonimmediate_operand, },
    { register_operand, nonimmediate_operand, },
    { register_operand, nonimmediate_operand, },
    { register_operand, nonimmediate_operand, },
    { register_operand, nonimmediate_operand, },
    { general_operand, general_operand, general_operand, },
    { general_operand, general_operand, general_operand, },
    { general_operand, general_operand, general_operand, },
    { general_operand, general_operand, general_operand, },
    { register_operand, address_operand, },
    { register_operand, nonimmediate_operand, nonimmediate_operand, },
    { register_operand, nonimmediate_operand, nonimmediate_operand, },
    { general_operand, general_operand, general_operand, },
    { general_operand, general_operand, general_operand, },
    { general_operand, general_operand, general_operand, },
    { general_operand, general_operand, general_operand, },
    { register_operand, nonimmediate_operand, nonimmediate_operand, },
    { register_operand, nonimmediate_operand, nonimmediate_operand, },
    { general_operand, general_operand, general_operand, },
    { general_operand, general_operand, general_operand, },
    { general_operand, general_operand, general_operand, },
    { general_operand, general_operand, general_operand, },
    { general_operand, nonimmediate_operand, nonimmediate_operand, },
    { register_operand, nonimmediate_operand, nonimmediate_operand, },
    { register_operand, nonimmediate_operand, nonimmediate_operand, },
    { general_operand, general_operand, general_operand, },
    { general_operand, general_operand, general_operand, },
    { register_operand, nonimmediate_operand, nonimmediate_operand, },
    { register_operand, nonimmediate_operand, nonimmediate_operand, },
    { register_operand, register_operand, general_operand, register_operand, },
    { register_operand, register_operand, general_operand, register_operand, },
    { register_operand, register_operand, general_operand, register_operand, },
    { register_operand, register_operand, general_operand, register_operand, },
    { general_operand, general_operand, general_operand, },
    { general_operand, general_operand, general_operand, },
    { general_operand, general_operand, general_operand, },
    { general_operand, general_operand, general_operand, },
    { general_operand, general_operand, general_operand, },
    { general_operand, general_operand, general_operand, },
    { general_operand, general_operand, general_operand, },
    { general_operand, general_operand, general_operand, },
    { general_operand, general_operand, general_operand, },
    { general_operand, general_operand, },
    { general_operand, general_operand, },
    { general_operand, general_operand, },
    { general_operand, general_operand, },
    { register_operand, general_operand, },
    { register_operand, general_operand, },
    { register_operand, general_operand, },
    { register_operand, general_operand, },
    { register_operand, general_operand, },
    { register_operand, general_operand, },
    { register_operand, general_operand, },
    { register_operand, general_operand, },
    { register_operand, general_operand, },
    { register_operand, register_operand, },
    { register_operand, register_operand, },
    { register_operand, register_operand, },
    { register_operand, register_operand, },
    { register_operand, register_operand, },
    { register_operand, register_operand, },
    { general_operand, general_operand, },
    { general_operand, general_operand, },
    { general_operand, general_operand, },
    { register_operand, register_operand, nonmemory_operand, },
    { register_operand, register_operand, const_int_operand, },
    { register_operand, register_operand, register_operand, },
    { general_operand, general_operand, nonmemory_operand, },
    { general_operand, general_operand, nonmemory_operand, },
    { general_operand, general_operand, nonmemory_operand, },
    { register_operand, register_operand, nonmemory_operand, },
    { register_operand, register_operand, const_int_operand, },
    { register_operand, register_operand, register_operand, },
    { general_operand, general_operand, nonmemory_operand, },
    { general_operand, general_operand, nonmemory_operand, },
    { general_operand, general_operand, nonmemory_operand, },
    { register_operand, register_operand, nonmemory_operand, },
    { register_operand, register_operand, const_int_operand, },
    { register_operand, register_operand, register_operand, },
    { general_operand, general_operand, nonmemory_operand, },
    { general_operand, general_operand, nonmemory_operand, },
    { general_operand, general_operand, nonmemory_operand, },
    { general_operand, general_operand, nonmemory_operand, },
    { general_operand, general_operand, nonmemory_operand, },
    { general_operand, general_operand, nonmemory_operand, },
    { general_operand, general_operand, nonmemory_operand, },
    { general_operand, general_operand, nonmemory_operand, },
    { general_operand, general_operand, nonmemory_operand, },
    { general_operand, 0, general_operand, const_int_operand, },
    { general_operand, general_operand, general_operand, },
    { general_operand, general_operand, general_operand, },
    { register_operand, general_operand, },
    { register_operand, const_int_operand, const_int_operand, },
    { general_operand, const_int_operand, const_int_operand, },
    { register_operand, },
    { register_operand, },
    { register_operand, },
    { register_operand, },
    { register_operand, },
    { register_operand, },
    { register_operand, },
    { register_operand, },
    { register_operand, },
    { register_operand, },
    { register_operand, },
    { register_operand, },
    { register_operand, },
    { register_operand, },
    { register_operand, },
    { register_operand, },
    { register_operand, },
    { register_operand, },
    { register_operand, },
    { register_operand, },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { 0 },
    { general_operand, },
    { general_operand, general_operand, general_operand, 0, 0, 0, scratch_operand, },
    { register_operand, 0, scratch_operand, },
    { general_operand, },
    { indirect_operand, general_operand, 0, immediate_operand, },
    { call_insn_operand, general_operand, 0, immediate_operand, },
    { symbolic_operand, general_operand, 0, immediate_operand, },
    { indirect_operand, general_operand, },
    { call_insn_operand, general_operand, },
    { symbolic_operand, general_operand, },
    { 0, indirect_operand, general_operand, 0, immediate_operand, },
    { 0, call_insn_operand, general_operand, 0, immediate_operand, },
    { 0, symbolic_operand, general_operand, 0, immediate_operand, },
    { 0, indirect_operand, general_operand, },
    { 0, call_insn_operand, general_operand, },
    { 0, symbolic_operand, general_operand, },
    { indirect_operand, memory_operand, 0, },
    { call_insn_operand, memory_operand, 0, },
    { symbolic_operand, memory_operand, 0, },
    { memory_operand, 0, },
    { memory_operand, },
    { 0 },
    { 0 },
    { memory_operand, memory_operand, const_int_operand, const_int_operand, scratch_operand, },
    { address_operand, address_operand, const_int_operand, immediate_operand, scratch_operand, },
    { general_operand, general_operand, general_operand, general_operand, immediate_operand, },
    { general_operand, address_operand, address_operand, register_operand, immediate_operand, },
    { address_operand, address_operand, register_operand, immediate_operand, },
    { general_operand, general_operand, scratch_operand, },
    { register_operand, general_operand, scratch_operand, },
    { general_operand, general_operand, scratch_operand, },
    { register_operand, general_operand, scratch_operand, },
    { register_operand, nonimmediate_operand, nonimmediate_operand, binary_387_op, },
    { register_operand, general_operand, general_operand, binary_387_op, },
    { register_operand, general_operand, general_operand, binary_387_op, },
    { register_operand, general_operand, general_operand, binary_387_op, },
    { register_operand, general_operand, general_operand, binary_387_op, },
    { register_operand, nonimmediate_operand, nonimmediate_operand, binary_387_op, },
    { register_operand, general_operand, general_operand, binary_387_op, },
    { register_operand, general_operand, general_operand, binary_387_op, },
    { register_operand, general_operand, register_operand, immediate_operand, },
    { register_operand, address_operand, register_operand, immediate_operand, },
  };

const int insn_n_alternatives[] =
  {
    1,
    0,
    1,
    0,
    1,
    0,
    1,
    0,
    1,
    0,
    2,
    0,
    2,
    0,
    2,
    0,
    2,
    1,
    1,
    1,
    1,
    1,
    2,
    1,
    1,
    1,
    0,
    0,
    0,
    0,
    0,
    0,
    1,
    1,
    1,
    1,
    1,
    0,
    2,
    1,
    2,
    2,
    1,
    3,
    2,
    2,
    4,
    2,
    1,
    4,
    1,
    2,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    2,
    0,
    2,
    0,
    0,
    0,
    0,
    1,
    1,
    0,
    0,
    1,
    1,
    0,
    0,
    0,
    0,
    1,
    1,
    1,
    1,
    2,
    3,
    2,
    2,
    1,
    0,
    0,
    2,
    2,
    2,
    2,
    0,
    0,
    1,
    2,
    1,
    2,
    1,
    0,
    0,
    1,
    1,
    0,
    0,
    1,
    1,
    1,
    1,
    4,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    0,
    1,
    1,
    2,
    1,
    1,
    0,
    1,
    1,
    1,
    1,
    1,
    0,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    0,
    1,
    0,
    1,
    0,
    1,
    0,
    1,
    0,
    1,
    0,
    1,
    0,
    1,
    0,
    1,
    0,
    1,
    0,
    1,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    1,
    0,
    1,
    1,
    0,
    1,
    1,
    0,
    1,
    1,
    0,
    1,
    1,
    0,
    1,
    1,
    0,
    1,
    1,
    0,
    1,
    0,
    0,
    0,
    1,
    0,
    1,
    1,
    0,
    1,
    0,
    1,
    2,
    1,
    2,
    1,
    2,
    2,
    1,
    1,
    0,
    1,
  };
