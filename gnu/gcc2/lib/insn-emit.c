/* Generated automatically by the program `genemit'
from the machine description file `md'.  */

#include "config.h"
#include "rtl.h"
#include "expr.h"
#include "real.h"
#include "output.h"
#include "insn-config.h"

#include "insn-flags.h"

#include "insn-codes.h"

extern char *insn_operand_constraint[][MAX_RECOG_OPERANDS];

extern rtx recog_operand[];
#define operands emit_operand

#define FAIL goto _fail

#define DONE goto _done

rtx
gen_tstsi_1 (operand0)
     rtx operand0;
{
  return gen_rtx (SET, VOIDmode, cc0_rtx, operand0);
}

rtx
gen_tstsi (operand0)
     rtx operand0;
{
  rtx operands[1];
  rtx _val = 0;
  start_sequence ();
  operands[0] = operand0;

{
  i386_compare_gen = gen_tstsi_1;
  i386_compare_op0 = operands[0];
  DONE;
}
  operand0 = operands[0];
  emit_insn (gen_rtx (SET, VOIDmode, cc0_rtx, operand0));
 _done:
  _val = gen_sequence ();
 _fail:
  end_sequence ();
  return _val;
}

rtx
gen_tsthi_1 (operand0)
     rtx operand0;
{
  return gen_rtx (SET, VOIDmode, cc0_rtx, operand0);
}

rtx
gen_tsthi (operand0)
     rtx operand0;
{
  rtx operands[1];
  rtx _val = 0;
  start_sequence ();
  operands[0] = operand0;

{
  i386_compare_gen = gen_tsthi_1;
  i386_compare_op0 = operands[0];
  DONE;
}
  operand0 = operands[0];
  emit_insn (gen_rtx (SET, VOIDmode, cc0_rtx, operand0));
 _done:
  _val = gen_sequence ();
 _fail:
  end_sequence ();
  return _val;
}

rtx
gen_tstqi_1 (operand0)
     rtx operand0;
{
  return gen_rtx (SET, VOIDmode, cc0_rtx, operand0);
}

rtx
gen_tstqi (operand0)
     rtx operand0;
{
  rtx operands[1];
  rtx _val = 0;
  start_sequence ();
  operands[0] = operand0;

{
  i386_compare_gen = gen_tstqi_1;
  i386_compare_op0 = operands[0];
  DONE;
}
  operand0 = operands[0];
  emit_insn (gen_rtx (SET, VOIDmode, cc0_rtx, operand0));
 _done:
  _val = gen_sequence ();
 _fail:
  end_sequence ();
  return _val;
}

rtx
gen_tstsf_cc (operand0)
     rtx operand0;
{
  return gen_rtx (PARALLEL, VOIDmode, gen_rtvec (2,
		gen_rtx (SET, VOIDmode, cc0_rtx, operand0),
		gen_rtx (CLOBBER, VOIDmode, gen_rtx (SCRATCH, HImode, 0))));
}

rtx
gen_tstsf (operand0)
     rtx operand0;
{
  rtx operands[1];
  rtx _val = 0;
  start_sequence ();
  operands[0] = operand0;

{
  i386_compare_gen = gen_tstsf_cc;
  i386_compare_op0 = operands[0];
  DONE;
}
  operand0 = operands[0];
  emit (gen_rtx (PARALLEL, VOIDmode, gen_rtvec (2,
		gen_rtx (SET, VOIDmode, cc0_rtx, operand0),
		gen_rtx (CLOBBER, VOIDmode, gen_rtx (SCRATCH, HImode, 0)))));
 _done:
  _val = gen_sequence ();
 _fail:
  end_sequence ();
  return _val;
}

rtx
gen_tstdf_cc (operand0)
     rtx operand0;
{
  return gen_rtx (PARALLEL, VOIDmode, gen_rtvec (2,
		gen_rtx (SET, VOIDmode, cc0_rtx, operand0),
		gen_rtx (CLOBBER, VOIDmode, gen_rtx (SCRATCH, HImode, 0))));
}

rtx
gen_tstdf (operand0)
     rtx operand0;
{
  rtx operands[1];
  rtx _val = 0;
  start_sequence ();
  operands[0] = operand0;

{
  i386_compare_gen = gen_tstdf_cc;
  i386_compare_op0 = operands[0];
  DONE;
}
  operand0 = operands[0];
  emit (gen_rtx (PARALLEL, VOIDmode, gen_rtvec (2,
		gen_rtx (SET, VOIDmode, cc0_rtx, operand0),
		gen_rtx (CLOBBER, VOIDmode, gen_rtx (SCRATCH, HImode, 0)))));
 _done:
  _val = gen_sequence ();
 _fail:
  end_sequence ();
  return _val;
}

rtx
gen_cmpsi_1 (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  return gen_rtx (SET, VOIDmode, cc0_rtx, gen_rtx (COMPARE, VOIDmode, operand0, operand1));
}

rtx
gen_cmpsi (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  rtx operands[2];
  rtx _val = 0;
  start_sequence ();
  operands[0] = operand0;
  operands[1] = operand1;

{
  if (GET_CODE (operands[0]) == MEM && GET_CODE (operands[1]) == MEM)
    operands[0] = force_reg (SImode, operands[0]);

  i386_compare_gen = gen_cmpsi_1;
  i386_compare_op0 = operands[0];
  i386_compare_op1 = operands[1];
  DONE;
}
  operand0 = operands[0];
  operand1 = operands[1];
  emit_insn (gen_rtx (SET, VOIDmode, cc0_rtx, gen_rtx (COMPARE, VOIDmode, operand0, operand1)));
 _done:
  _val = gen_sequence ();
 _fail:
  end_sequence ();
  return _val;
}

rtx
gen_cmphi_1 (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  return gen_rtx (SET, VOIDmode, cc0_rtx, gen_rtx (COMPARE, VOIDmode, operand0, operand1));
}

rtx
gen_cmphi (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  rtx operands[2];
  rtx _val = 0;
  start_sequence ();
  operands[0] = operand0;
  operands[1] = operand1;

{
  if (GET_CODE (operands[0]) == MEM && GET_CODE (operands[1]) == MEM)
    operands[0] = force_reg (HImode, operands[0]);

  i386_compare_gen = gen_cmphi_1;
  i386_compare_op0 = operands[0];
  i386_compare_op1 = operands[1];
  DONE;
}
  operand0 = operands[0];
  operand1 = operands[1];
  emit_insn (gen_rtx (SET, VOIDmode, cc0_rtx, gen_rtx (COMPARE, VOIDmode, operand0, operand1)));
 _done:
  _val = gen_sequence ();
 _fail:
  end_sequence ();
  return _val;
}

rtx
gen_cmpqi_1 (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  return gen_rtx (SET, VOIDmode, cc0_rtx, gen_rtx (COMPARE, VOIDmode, operand0, operand1));
}

rtx
gen_cmpqi (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  rtx operands[2];
  rtx _val = 0;
  start_sequence ();
  operands[0] = operand0;
  operands[1] = operand1;

{
  if (GET_CODE (operands[0]) == MEM && GET_CODE (operands[1]) == MEM)
    operands[0] = force_reg (QImode, operands[0]);

  i386_compare_gen = gen_cmpqi_1;
  i386_compare_op0 = operands[0];
  i386_compare_op1 = operands[1];
  DONE;
}
  operand0 = operands[0];
  operand1 = operands[1];
  emit_insn (gen_rtx (SET, VOIDmode, cc0_rtx, gen_rtx (COMPARE, VOIDmode, operand0, operand1)));
 _done:
  _val = gen_sequence ();
 _fail:
  end_sequence ();
  return _val;
}

rtx
gen_cmpsf_cc_1 (operand0, operand1, operand2)
     rtx operand0;
     rtx operand1;
     rtx operand2;
{
  return gen_rtx (PARALLEL, VOIDmode, gen_rtvec (2,
		gen_rtx (SET, VOIDmode, cc0_rtx, gen_rtx (GET_CODE (operand2), VOIDmode,
		operand0,
		operand1)),
		gen_rtx (CLOBBER, VOIDmode, gen_rtx (SCRATCH, HImode, 0))));
}

rtx
gen_cmpdf (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  rtx operands[2];
  rtx _val = 0;
  start_sequence ();
  operands[0] = operand0;
  operands[1] = operand1;

{
  i386_compare_gen = gen_cmpdf_cc;
  i386_compare_gen_eq = gen_cmpdf_ccfpeq;
  i386_compare_op0 = operands[0];
  i386_compare_op1 = operands[1];
  DONE;
}
  operand0 = operands[0];
  operand1 = operands[1];
  emit_insn (gen_rtx (SET, VOIDmode, cc0_rtx, gen_rtx (COMPARE, VOIDmode, operand0, operand1)));
 _done:
  _val = gen_sequence ();
 _fail:
  end_sequence ();
  return _val;
}

rtx
gen_cmpsf (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  rtx operands[2];
  rtx _val = 0;
  start_sequence ();
  operands[0] = operand0;
  operands[1] = operand1;

{
  i386_compare_gen = gen_cmpsf_cc;
  i386_compare_gen_eq = gen_cmpsf_ccfpeq;
  i386_compare_op0 = operands[0];
  i386_compare_op1 = operands[1];
  DONE;
}
  operand0 = operands[0];
  operand1 = operands[1];
  emit_insn (gen_rtx (SET, VOIDmode, cc0_rtx, gen_rtx (COMPARE, VOIDmode, operand0, operand1)));
 _done:
  _val = gen_sequence ();
 _fail:
  end_sequence ();
  return _val;
}

rtx
gen_cmpdf_cc (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  return gen_rtx (PARALLEL, VOIDmode, gen_rtvec (2,
		gen_rtx (SET, VOIDmode, cc0_rtx, gen_rtx (COMPARE, VOIDmode, operand0, operand1)),
		gen_rtx (CLOBBER, VOIDmode, gen_rtx (SCRATCH, HImode, 0))));
}

rtx
gen_cmpdf_ccfpeq (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  rtx operands[2];
  rtx _val = 0;
  start_sequence ();
  operands[0] = operand0;
  operands[1] = operand1;

{
  if (! register_operand (operands[1], DFmode))
    operands[1] = copy_to_mode_reg (DFmode, operands[1]);
}
  operand0 = operands[0];
  operand1 = operands[1];
  emit (gen_rtx (PARALLEL, VOIDmode, gen_rtvec (2,
		gen_rtx (SET, VOIDmode, cc0_rtx, gen_rtx (COMPARE, CCFPEQmode, operand0, operand1)),
		gen_rtx (CLOBBER, VOIDmode, gen_rtx (SCRATCH, HImode, 0)))));
 _done:
  _val = gen_sequence ();
 _fail:
  end_sequence ();
  return _val;
}

rtx
gen_cmpsf_cc (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  return gen_rtx (PARALLEL, VOIDmode, gen_rtvec (2,
		gen_rtx (SET, VOIDmode, cc0_rtx, gen_rtx (COMPARE, VOIDmode, operand0, operand1)),
		gen_rtx (CLOBBER, VOIDmode, gen_rtx (SCRATCH, HImode, 0))));
}

rtx
gen_cmpsf_ccfpeq (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  rtx operands[2];
  rtx _val = 0;
  start_sequence ();
  operands[0] = operand0;
  operands[1] = operand1;

{
  if (! register_operand (operands[1], SFmode))
    operands[1] = copy_to_mode_reg (SFmode, operands[1]);
}
  operand0 = operands[0];
  operand1 = operands[1];
  emit (gen_rtx (PARALLEL, VOIDmode, gen_rtvec (2,
		gen_rtx (SET, VOIDmode, cc0_rtx, gen_rtx (COMPARE, CCFPEQmode, operand0, operand1)),
		gen_rtx (CLOBBER, VOIDmode, gen_rtx (SCRATCH, HImode, 0)))));
 _done:
  _val = gen_sequence ();
 _fail:
  end_sequence ();
  return _val;
}

rtx
gen_movsi (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  rtx operands[2];
  rtx _val = 0;
  start_sequence ();
  operands[0] = operand0;
  operands[1] = operand1;

{
  extern int flag_pic;

  if (flag_pic && SYMBOLIC_CONST (operands[1]))
    emit_pic_move (operands, SImode);
}
  operand0 = operands[0];
  operand1 = operands[1];
  emit_insn (gen_rtx (SET, VOIDmode, operand0, operand1));
 _done:
  _val = gen_sequence ();
 _fail:
  end_sequence ();
  return _val;
}

rtx
gen_movhi (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  return gen_rtx (SET, VOIDmode, operand0, operand1);
}

rtx
gen_movstricthi (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  return gen_rtx (SET, VOIDmode, gen_rtx (STRICT_LOW_PART, VOIDmode, operand0), operand1);
}

rtx
gen_movqi (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  return gen_rtx (SET, VOIDmode, operand0, operand1);
}

rtx
gen_movstrictqi (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  return gen_rtx (SET, VOIDmode, gen_rtx (STRICT_LOW_PART, VOIDmode, operand0), operand1);
}

rtx
gen_movsf (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  return gen_rtx (SET, VOIDmode, operand0, operand1);
}

rtx
gen_swapdf (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  return gen_rtx (PARALLEL, VOIDmode, gen_rtvec (2,
		gen_rtx (SET, VOIDmode, operand0, operand1),
		gen_rtx (SET, VOIDmode, operand1, operand0)));
}

rtx
gen_movdf (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  return gen_rtx (SET, VOIDmode, operand0, operand1);
}

rtx
gen_movdi (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  return gen_rtx (SET, VOIDmode, operand0, operand1);
}

rtx
gen_zero_extendhisi2 (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (ZERO_EXTEND, SImode, operand1));
}

rtx
gen_zero_extendqihi2 (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (ZERO_EXTEND, HImode, operand1));
}

rtx
gen_zero_extendqisi2 (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (ZERO_EXTEND, SImode, operand1));
}

rtx
gen_zero_extendsidi2 (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (ZERO_EXTEND, DImode, operand1));
}

rtx
gen_extendsidi2 (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (SIGN_EXTEND, DImode, operand1));
}

rtx
gen_extendhisi2 (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (SIGN_EXTEND, SImode, operand1));
}

rtx
gen_extendqihi2 (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (SIGN_EXTEND, HImode, operand1));
}

rtx
gen_extendqisi2 (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (SIGN_EXTEND, SImode, operand1));
}

rtx
gen_extendsfdf2 (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (FLOAT_EXTEND, DFmode, operand1));
}

rtx
gen_truncdfsf2 (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  rtx operand2;
  rtx operands[3];
  rtx _val = 0;
  start_sequence ();
  operands[0] = operand0;
  operands[1] = operand1;

{
  operands[2] = (rtx) assign_386_stack_local (SFmode, 0);
}
  operand0 = operands[0];
  operand1 = operands[1];
  operand2 = operands[2];
  emit (gen_rtx (PARALLEL, VOIDmode, gen_rtvec (2,
		gen_rtx (SET, VOIDmode, operand0, gen_rtx (FLOAT_TRUNCATE, SFmode, operand1)),
		gen_rtx (CLOBBER, VOIDmode, operand2))));
 _done:
  _val = gen_sequence ();
 _fail:
  end_sequence ();
  return _val;
}

rtx
gen_fixuns_truncdfsi2 (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  rtx operand2;
  rtx operand3;
  rtx operand4;
  rtx operand5;
  rtx operand6;
  rtx operands[7];
  rtx _val = 0;
  start_sequence ();
  operands[0] = operand0;
  operands[1] = operand1;

{
  operands[2] = gen_reg_rtx (DImode);
  operands[3] = gen_lowpart (SImode, operands[2]);
  operands[4] = gen_reg_rtx (DFmode);
  operands[5] = (rtx) assign_386_stack_local (SImode, 0);
  operands[6] = (rtx) assign_386_stack_local (SImode, 1);
}
  operand0 = operands[0];
  operand1 = operands[1];
  operand2 = operands[2];
  operand3 = operands[3];
  operand4 = operands[4];
  operand5 = operands[5];
  operand6 = operands[6];
  emit_insn (gen_rtx (SET, VOIDmode, operand4, operand1));
  emit (gen_rtx (PARALLEL, VOIDmode, gen_rtvec (5,
		gen_rtx (SET, VOIDmode, operand2, gen_rtx (FIX, DImode, gen_rtx (FIX, DFmode, operand4))),
		gen_rtx (CLOBBER, VOIDmode, operand4),
		gen_rtx (CLOBBER, VOIDmode, operand5),
		gen_rtx (CLOBBER, VOIDmode, operand6),
		gen_rtx (CLOBBER, VOIDmode, gen_rtx (SCRATCH, SImode, 0)))));
  emit_insn (gen_rtx (SET, VOIDmode, operand0, operand3));
 _done:
  _val = gen_sequence ();
 _fail:
  end_sequence ();
  return _val;
}

rtx
gen_fixuns_truncsfsi2 (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  rtx operand2;
  rtx operand3;
  rtx operand4;
  rtx operand5;
  rtx operand6;
  rtx operands[7];
  rtx _val = 0;
  start_sequence ();
  operands[0] = operand0;
  operands[1] = operand1;

{
  operands[2] = gen_reg_rtx (DImode);
  operands[3] = gen_lowpart (SImode, operands[2]);
  operands[4] = gen_reg_rtx (SFmode);
  operands[5] = (rtx) assign_386_stack_local (SImode, 0);
  operands[6] = (rtx) assign_386_stack_local (SImode, 1);
}
  operand0 = operands[0];
  operand1 = operands[1];
  operand2 = operands[2];
  operand3 = operands[3];
  operand4 = operands[4];
  operand5 = operands[5];
  operand6 = operands[6];
  emit_insn (gen_rtx (SET, VOIDmode, operand4, operand1));
  emit (gen_rtx (PARALLEL, VOIDmode, gen_rtvec (5,
		gen_rtx (SET, VOIDmode, operand2, gen_rtx (FIX, DImode, gen_rtx (FIX, SFmode, operand4))),
		gen_rtx (CLOBBER, VOIDmode, operand4),
		gen_rtx (CLOBBER, VOIDmode, operand5),
		gen_rtx (CLOBBER, VOIDmode, operand6),
		gen_rtx (CLOBBER, VOIDmode, gen_rtx (SCRATCH, SImode, 0)))));
  emit_insn (gen_rtx (SET, VOIDmode, operand0, operand3));
 _done:
  _val = gen_sequence ();
 _fail:
  end_sequence ();
  return _val;
}

rtx
gen_fix_truncdfdi2 (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  rtx operand2;
  rtx operand3;
  rtx operand4;
  rtx operands[5];
  rtx _val = 0;
  start_sequence ();
  operands[0] = operand0;
  operands[1] = operand1;

{
  operands[1] = copy_to_mode_reg (DFmode, operands[1]);
  operands[2] = gen_reg_rtx (DFmode);
  operands[3] = (rtx) assign_386_stack_local (SImode, 0);
  operands[4] = (rtx) assign_386_stack_local (SImode, 1);
}
  operand0 = operands[0];
  operand1 = operands[1];
  operand2 = operands[2];
  operand3 = operands[3];
  operand4 = operands[4];
  emit_insn (gen_rtx (SET, VOIDmode, operand2, operand1));
  emit (gen_rtx (PARALLEL, VOIDmode, gen_rtvec (5,
		gen_rtx (SET, VOIDmode, operand0, gen_rtx (FIX, DImode, gen_rtx (FIX, DFmode, operand2))),
		gen_rtx (CLOBBER, VOIDmode, operand2),
		gen_rtx (CLOBBER, VOIDmode, operand3),
		gen_rtx (CLOBBER, VOIDmode, operand4),
		gen_rtx (CLOBBER, VOIDmode, gen_rtx (SCRATCH, SImode, 0)))));
 _done:
  _val = gen_sequence ();
 _fail:
  end_sequence ();
  return _val;
}

rtx
gen_fix_truncsfdi2 (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  rtx operand2;
  rtx operand3;
  rtx operand4;
  rtx operands[5];
  rtx _val = 0;
  start_sequence ();
  operands[0] = operand0;
  operands[1] = operand1;

{
  operands[1] = copy_to_mode_reg (SFmode, operands[1]);
  operands[2] = gen_reg_rtx (SFmode);
  operands[3] = (rtx) assign_386_stack_local (SImode, 0);
  operands[4] = (rtx) assign_386_stack_local (SImode, 1);
}
  operand0 = operands[0];
  operand1 = operands[1];
  operand2 = operands[2];
  operand3 = operands[3];
  operand4 = operands[4];
  emit_insn (gen_rtx (SET, VOIDmode, operand2, operand1));
  emit (gen_rtx (PARALLEL, VOIDmode, gen_rtvec (5,
		gen_rtx (SET, VOIDmode, operand0, gen_rtx (FIX, DImode, gen_rtx (FIX, SFmode, operand2))),
		gen_rtx (CLOBBER, VOIDmode, operand2),
		gen_rtx (CLOBBER, VOIDmode, operand3),
		gen_rtx (CLOBBER, VOIDmode, operand4),
		gen_rtx (CLOBBER, VOIDmode, gen_rtx (SCRATCH, SImode, 0)))));
 _done:
  _val = gen_sequence ();
 _fail:
  end_sequence ();
  return _val;
}

rtx
gen_fix_truncdfsi2 (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  rtx operand2;
  rtx operand3;
  rtx operands[4];
  rtx _val = 0;
  start_sequence ();
  operands[0] = operand0;
  operands[1] = operand1;

{
  operands[2] = (rtx) assign_386_stack_local (SImode, 0);
  operands[3] = (rtx) assign_386_stack_local (SImode, 1);
}
  operand0 = operands[0];
  operand1 = operands[1];
  operand2 = operands[2];
  operand3 = operands[3];
  emit (gen_rtx (PARALLEL, VOIDmode, gen_rtvec (4,
		gen_rtx (SET, VOIDmode, operand0, gen_rtx (FIX, SImode, gen_rtx (FIX, DFmode, operand1))),
		gen_rtx (CLOBBER, VOIDmode, operand2),
		gen_rtx (CLOBBER, VOIDmode, operand3),
		gen_rtx (CLOBBER, VOIDmode, gen_rtx (SCRATCH, SImode, 0)))));
 _done:
  _val = gen_sequence ();
 _fail:
  end_sequence ();
  return _val;
}

rtx
gen_fix_truncsfsi2 (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  rtx operand2;
  rtx operand3;
  rtx operands[4];
  rtx _val = 0;
  start_sequence ();
  operands[0] = operand0;
  operands[1] = operand1;

{
  operands[2] = (rtx) assign_386_stack_local (SImode, 0);
  operands[3] = (rtx) assign_386_stack_local (SImode, 1);
}
  operand0 = operands[0];
  operand1 = operands[1];
  operand2 = operands[2];
  operand3 = operands[3];
  emit (gen_rtx (PARALLEL, VOIDmode, gen_rtvec (4,
		gen_rtx (SET, VOIDmode, operand0, gen_rtx (FIX, SImode, gen_rtx (FIX, SFmode, operand1))),
		gen_rtx (CLOBBER, VOIDmode, operand2),
		gen_rtx (CLOBBER, VOIDmode, operand3),
		gen_rtx (CLOBBER, VOIDmode, gen_rtx (SCRATCH, SImode, 0)))));
 _done:
  _val = gen_sequence ();
 _fail:
  end_sequence ();
  return _val;
}

rtx
gen_floatsisf2 (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (FLOAT, SFmode, operand1));
}

rtx
gen_floatdisf2 (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (FLOAT, SFmode, operand1));
}

rtx
gen_floatsidf2 (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (FLOAT, DFmode, operand1));
}

rtx
gen_floatdidf2 (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (FLOAT, DFmode, operand1));
}

rtx
gen_adddi3 (operand0, operand1, operand2)
     rtx operand0;
     rtx operand1;
     rtx operand2;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (PLUS, DImode, operand1, operand2));
}

rtx
gen_addsi3 (operand0, operand1, operand2)
     rtx operand0;
     rtx operand1;
     rtx operand2;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (PLUS, SImode, operand1, operand2));
}

rtx
gen_addhi3 (operand0, operand1, operand2)
     rtx operand0;
     rtx operand1;
     rtx operand2;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (PLUS, HImode, operand1, operand2));
}

rtx
gen_addqi3 (operand0, operand1, operand2)
     rtx operand0;
     rtx operand1;
     rtx operand2;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (PLUS, QImode, operand1, operand2));
}

rtx
gen_adddf3 (operand0, operand1, operand2)
     rtx operand0;
     rtx operand1;
     rtx operand2;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (PLUS, DFmode, operand1, operand2));
}

rtx
gen_addsf3 (operand0, operand1, operand2)
     rtx operand0;
     rtx operand1;
     rtx operand2;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (PLUS, SFmode, operand1, operand2));
}

rtx
gen_subdi3 (operand0, operand1, operand2)
     rtx operand0;
     rtx operand1;
     rtx operand2;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (MINUS, DImode, operand1, operand2));
}

rtx
gen_subsi3 (operand0, operand1, operand2)
     rtx operand0;
     rtx operand1;
     rtx operand2;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (MINUS, SImode, operand1, operand2));
}

rtx
gen_subhi3 (operand0, operand1, operand2)
     rtx operand0;
     rtx operand1;
     rtx operand2;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (MINUS, HImode, operand1, operand2));
}

rtx
gen_subqi3 (operand0, operand1, operand2)
     rtx operand0;
     rtx operand1;
     rtx operand2;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (MINUS, QImode, operand1, operand2));
}

rtx
gen_subdf3 (operand0, operand1, operand2)
     rtx operand0;
     rtx operand1;
     rtx operand2;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (MINUS, DFmode, operand1, operand2));
}

rtx
gen_subsf3 (operand0, operand1, operand2)
     rtx operand0;
     rtx operand1;
     rtx operand2;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (MINUS, SFmode, operand1, operand2));
}

rtx
gen_mulhi3 (operand0, operand1, operand2)
     rtx operand0;
     rtx operand1;
     rtx operand2;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (MULT, SImode, operand1, operand2));
}

rtx
gen_mulsi3 (operand0, operand1, operand2)
     rtx operand0;
     rtx operand1;
     rtx operand2;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (MULT, SImode, operand1, operand2));
}

rtx
gen_muldf3 (operand0, operand1, operand2)
     rtx operand0;
     rtx operand1;
     rtx operand2;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (MULT, DFmode, operand1, operand2));
}

rtx
gen_mulsf3 (operand0, operand1, operand2)
     rtx operand0;
     rtx operand1;
     rtx operand2;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (MULT, SFmode, operand1, operand2));
}

rtx
gen_divqi3 (operand0, operand1, operand2)
     rtx operand0;
     rtx operand1;
     rtx operand2;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (DIV, QImode, operand1, operand2));
}

rtx
gen_udivqi3 (operand0, operand1, operand2)
     rtx operand0;
     rtx operand1;
     rtx operand2;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (UDIV, QImode, operand1, operand2));
}

rtx
gen_divdf3 (operand0, operand1, operand2)
     rtx operand0;
     rtx operand1;
     rtx operand2;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (DIV, DFmode, operand1, operand2));
}

rtx
gen_divsf3 (operand0, operand1, operand2)
     rtx operand0;
     rtx operand1;
     rtx operand2;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (DIV, SFmode, operand1, operand2));
}

rtx
gen_divmodsi4 (operand0, operand1, operand2, operand3)
     rtx operand0;
     rtx operand1;
     rtx operand2;
     rtx operand3;
{
  return gen_rtx (PARALLEL, VOIDmode, gen_rtvec (2,
		gen_rtx (SET, VOIDmode, operand0, gen_rtx (DIV, SImode, operand1, operand2)),
		gen_rtx (SET, VOIDmode, operand3, gen_rtx (MOD, SImode, operand1, operand2))));
}

rtx
gen_divmodhi4 (operand0, operand1, operand2, operand3)
     rtx operand0;
     rtx operand1;
     rtx operand2;
     rtx operand3;
{
  return gen_rtx (PARALLEL, VOIDmode, gen_rtvec (2,
		gen_rtx (SET, VOIDmode, operand0, gen_rtx (DIV, HImode, operand1, operand2)),
		gen_rtx (SET, VOIDmode, operand3, gen_rtx (MOD, HImode, operand1, operand2))));
}

rtx
gen_udivmodsi4 (operand0, operand1, operand2, operand3)
     rtx operand0;
     rtx operand1;
     rtx operand2;
     rtx operand3;
{
  return gen_rtx (PARALLEL, VOIDmode, gen_rtvec (2,
		gen_rtx (SET, VOIDmode, operand0, gen_rtx (UDIV, SImode, operand1, operand2)),
		gen_rtx (SET, VOIDmode, operand3, gen_rtx (UMOD, SImode, operand1, operand2))));
}

rtx
gen_udivmodhi4 (operand0, operand1, operand2, operand3)
     rtx operand0;
     rtx operand1;
     rtx operand2;
     rtx operand3;
{
  return gen_rtx (PARALLEL, VOIDmode, gen_rtvec (2,
		gen_rtx (SET, VOIDmode, operand0, gen_rtx (UDIV, HImode, operand1, operand2)),
		gen_rtx (SET, VOIDmode, operand3, gen_rtx (UMOD, HImode, operand1, operand2))));
}

rtx
gen_andsi3 (operand0, operand1, operand2)
     rtx operand0;
     rtx operand1;
     rtx operand2;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (AND, SImode, operand1, operand2));
}

rtx
gen_andhi3 (operand0, operand1, operand2)
     rtx operand0;
     rtx operand1;
     rtx operand2;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (AND, HImode, operand1, operand2));
}

rtx
gen_andqi3 (operand0, operand1, operand2)
     rtx operand0;
     rtx operand1;
     rtx operand2;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (AND, QImode, operand1, operand2));
}

rtx
gen_iorsi3 (operand0, operand1, operand2)
     rtx operand0;
     rtx operand1;
     rtx operand2;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (IOR, SImode, operand1, operand2));
}

rtx
gen_iorhi3 (operand0, operand1, operand2)
     rtx operand0;
     rtx operand1;
     rtx operand2;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (IOR, HImode, operand1, operand2));
}

rtx
gen_iorqi3 (operand0, operand1, operand2)
     rtx operand0;
     rtx operand1;
     rtx operand2;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (IOR, QImode, operand1, operand2));
}

rtx
gen_xorsi3 (operand0, operand1, operand2)
     rtx operand0;
     rtx operand1;
     rtx operand2;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (XOR, SImode, operand1, operand2));
}

rtx
gen_xorhi3 (operand0, operand1, operand2)
     rtx operand0;
     rtx operand1;
     rtx operand2;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (XOR, HImode, operand1, operand2));
}

rtx
gen_xorqi3 (operand0, operand1, operand2)
     rtx operand0;
     rtx operand1;
     rtx operand2;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (XOR, QImode, operand1, operand2));
}

rtx
gen_negdi2 (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (NEG, DImode, operand1));
}

rtx
gen_negsi2 (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (NEG, SImode, operand1));
}

rtx
gen_neghi2 (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (NEG, HImode, operand1));
}

rtx
gen_negqi2 (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (NEG, QImode, operand1));
}

rtx
gen_negsf2 (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (NEG, SFmode, operand1));
}

rtx
gen_negdf2 (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (NEG, DFmode, operand1));
}

rtx
gen_abssf2 (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (ABS, SFmode, operand1));
}

rtx
gen_absdf2 (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (ABS, DFmode, operand1));
}

rtx
gen_sqrtsf2 (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (SQRT, SFmode, operand1));
}

rtx
gen_sqrtdf2 (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (SQRT, DFmode, operand1));
}

rtx
gen_sindf2 (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (UNSPEC, DFmode, gen_rtvec (1,
		operand1), 1));
}

rtx
gen_sinsf2 (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (UNSPEC, SFmode, gen_rtvec (1,
		operand1), 1));
}

rtx
gen_cosdf2 (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (UNSPEC, DFmode, gen_rtvec (1,
		operand1), 2));
}

rtx
gen_cossf2 (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (UNSPEC, SFmode, gen_rtvec (1,
		operand1), 2));
}

rtx
gen_one_cmplsi2 (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (NOT, SImode, operand1));
}

rtx
gen_one_cmplhi2 (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (NOT, HImode, operand1));
}

rtx
gen_one_cmplqi2 (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (NOT, QImode, operand1));
}

rtx
gen_ashldi3 (operand0, operand1, operand2)
     rtx operand0;
     rtx operand1;
     rtx operand2;
{
  rtx operands[3];
  rtx _val = 0;
  start_sequence ();
  operands[0] = operand0;
  operands[1] = operand1;
  operands[2] = operand2;

{
  if (GET_CODE (operands[2]) != CONST_INT
      || ! CONST_OK_FOR_LETTER_P (INTVAL (operands[2]), 'J'))
    {
      operands[2] = copy_to_mode_reg (QImode, operands[2]);
      emit_insn (gen_ashldi3_non_const_int (operands[0], operands[1],
					    operands[2]));
    }
  else
    emit_insn (gen_ashldi3_const_int (operands[0], operands[1], operands[2]));

  DONE;
}
  operand0 = operands[0];
  operand1 = operands[1];
  operand2 = operands[2];
  emit_insn (gen_rtx (SET, VOIDmode, operand0, gen_rtx (ASHIFT, DImode, operand1, operand2)));
 _done:
  _val = gen_sequence ();
 _fail:
  end_sequence ();
  return _val;
}

rtx
gen_ashldi3_const_int (operand0, operand1, operand2)
     rtx operand0;
     rtx operand1;
     rtx operand2;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (ASHIFT, DImode, operand1, operand2));
}

rtx
gen_ashldi3_non_const_int (operand0, operand1, operand2)
     rtx operand0;
     rtx operand1;
     rtx operand2;
{
  return gen_rtx (PARALLEL, VOIDmode, gen_rtvec (2,
		gen_rtx (SET, VOIDmode, operand0, gen_rtx (ASHIFT, DImode, operand1, operand2)),
		gen_rtx (CLOBBER, VOIDmode, operand2)));
}

rtx
gen_ashlsi3 (operand0, operand1, operand2)
     rtx operand0;
     rtx operand1;
     rtx operand2;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (ASHIFT, SImode, operand1, operand2));
}

rtx
gen_ashlhi3 (operand0, operand1, operand2)
     rtx operand0;
     rtx operand1;
     rtx operand2;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (ASHIFT, HImode, operand1, operand2));
}

rtx
gen_ashlqi3 (operand0, operand1, operand2)
     rtx operand0;
     rtx operand1;
     rtx operand2;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (ASHIFT, QImode, operand1, operand2));
}

rtx
gen_ashrdi3 (operand0, operand1, operand2)
     rtx operand0;
     rtx operand1;
     rtx operand2;
{
  rtx operands[3];
  rtx _val = 0;
  start_sequence ();
  operands[0] = operand0;
  operands[1] = operand1;
  operands[2] = operand2;

{
  if (GET_CODE (operands[2]) != CONST_INT
      || ! CONST_OK_FOR_LETTER_P (INTVAL (operands[2]), 'J'))
    {
      operands[2] = copy_to_mode_reg (QImode, operands[2]);
      emit_insn (gen_ashrdi3_non_const_int (operands[0], operands[1],
					    operands[2]));
    }
  else
    emit_insn (gen_ashrdi3_const_int (operands[0], operands[1], operands[2]));

  DONE;
}
  operand0 = operands[0];
  operand1 = operands[1];
  operand2 = operands[2];
  emit_insn (gen_rtx (SET, VOIDmode, operand0, gen_rtx (ASHIFTRT, DImode, operand1, operand2)));
 _done:
  _val = gen_sequence ();
 _fail:
  end_sequence ();
  return _val;
}

rtx
gen_ashrdi3_const_int (operand0, operand1, operand2)
     rtx operand0;
     rtx operand1;
     rtx operand2;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (ASHIFTRT, DImode, operand1, operand2));
}

rtx
gen_ashrdi3_non_const_int (operand0, operand1, operand2)
     rtx operand0;
     rtx operand1;
     rtx operand2;
{
  return gen_rtx (PARALLEL, VOIDmode, gen_rtvec (2,
		gen_rtx (SET, VOIDmode, operand0, gen_rtx (ASHIFTRT, DImode, operand1, operand2)),
		gen_rtx (CLOBBER, VOIDmode, operand2)));
}

rtx
gen_ashrsi3 (operand0, operand1, operand2)
     rtx operand0;
     rtx operand1;
     rtx operand2;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (ASHIFTRT, SImode, operand1, operand2));
}

rtx
gen_ashrhi3 (operand0, operand1, operand2)
     rtx operand0;
     rtx operand1;
     rtx operand2;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (ASHIFTRT, HImode, operand1, operand2));
}

rtx
gen_ashrqi3 (operand0, operand1, operand2)
     rtx operand0;
     rtx operand1;
     rtx operand2;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (ASHIFTRT, QImode, operand1, operand2));
}

rtx
gen_lshrdi3 (operand0, operand1, operand2)
     rtx operand0;
     rtx operand1;
     rtx operand2;
{
  rtx operands[3];
  rtx _val = 0;
  start_sequence ();
  operands[0] = operand0;
  operands[1] = operand1;
  operands[2] = operand2;

{
  if (GET_CODE (operands[2]) != CONST_INT
      || ! CONST_OK_FOR_LETTER_P (INTVAL (operands[2]), 'J'))
    {
      operands[2] = copy_to_mode_reg (QImode, operands[2]);
      emit_insn (gen_lshrdi3_non_const_int (operands[0], operands[1],
					    operands[2]));
    }
  else
    emit_insn (gen_lshrdi3_const_int (operands[0], operands[1], operands[2]));

  DONE;
}
  operand0 = operands[0];
  operand1 = operands[1];
  operand2 = operands[2];
  emit_insn (gen_rtx (SET, VOIDmode, operand0, gen_rtx (LSHIFTRT, DImode, operand1, operand2)));
 _done:
  _val = gen_sequence ();
 _fail:
  end_sequence ();
  return _val;
}

rtx
gen_lshrdi3_const_int (operand0, operand1, operand2)
     rtx operand0;
     rtx operand1;
     rtx operand2;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (LSHIFTRT, DImode, operand1, operand2));
}

rtx
gen_lshrdi3_non_const_int (operand0, operand1, operand2)
     rtx operand0;
     rtx operand1;
     rtx operand2;
{
  return gen_rtx (PARALLEL, VOIDmode, gen_rtvec (2,
		gen_rtx (SET, VOIDmode, operand0, gen_rtx (LSHIFTRT, DImode, operand1, operand2)),
		gen_rtx (CLOBBER, VOIDmode, operand2)));
}

rtx
gen_lshrsi3 (operand0, operand1, operand2)
     rtx operand0;
     rtx operand1;
     rtx operand2;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (LSHIFTRT, SImode, operand1, operand2));
}

rtx
gen_lshrhi3 (operand0, operand1, operand2)
     rtx operand0;
     rtx operand1;
     rtx operand2;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (LSHIFTRT, HImode, operand1, operand2));
}

rtx
gen_lshrqi3 (operand0, operand1, operand2)
     rtx operand0;
     rtx operand1;
     rtx operand2;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (LSHIFTRT, QImode, operand1, operand2));
}

rtx
gen_rotlsi3 (operand0, operand1, operand2)
     rtx operand0;
     rtx operand1;
     rtx operand2;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (ROTATE, SImode, operand1, operand2));
}

rtx
gen_rotlhi3 (operand0, operand1, operand2)
     rtx operand0;
     rtx operand1;
     rtx operand2;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (ROTATE, HImode, operand1, operand2));
}

rtx
gen_rotlqi3 (operand0, operand1, operand2)
     rtx operand0;
     rtx operand1;
     rtx operand2;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (ROTATE, QImode, operand1, operand2));
}

rtx
gen_rotrsi3 (operand0, operand1, operand2)
     rtx operand0;
     rtx operand1;
     rtx operand2;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (ROTATERT, SImode, operand1, operand2));
}

rtx
gen_rotrhi3 (operand0, operand1, operand2)
     rtx operand0;
     rtx operand1;
     rtx operand2;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (ROTATERT, HImode, operand1, operand2));
}

rtx
gen_rotrqi3 (operand0, operand1, operand2)
     rtx operand0;
     rtx operand1;
     rtx operand2;
{
  return gen_rtx (SET, VOIDmode, operand0, gen_rtx (ROTATERT, QImode, operand1, operand2));
}

rtx
gen_seq (operand0)
     rtx operand0;
{
  rtx operand1;
  rtx operands[2];
  rtx _val = 0;
  start_sequence ();
  operands[0] = operand0;

{
  if (TARGET_IEEE_FP
      && GET_MODE_CLASS (GET_MODE (i386_compare_op0)) == MODE_FLOAT)
    operands[1] = (*i386_compare_gen_eq)(i386_compare_op0, i386_compare_op1);
  else
    operands[1] = (*i386_compare_gen)(i386_compare_op0, i386_compare_op1);
}
  operand0 = operands[0];
  operand1 = operands[1];
  emit (operand1);
  emit_insn (gen_rtx (SET, VOIDmode, operand0, gen_rtx (EQ, QImode, cc0_rtx, const0_rtx)));
 _done:
  _val = gen_sequence ();
 _fail:
  end_sequence ();
  return _val;
}

rtx
gen_sne (operand0)
     rtx operand0;
{
  rtx operand1;
  rtx operands[2];
  rtx _val = 0;
  start_sequence ();
  operands[0] = operand0;

{
  if (TARGET_IEEE_FP
      && GET_MODE_CLASS (GET_MODE (i386_compare_op0)) == MODE_FLOAT)
    operands[1] = (*i386_compare_gen_eq)(i386_compare_op0, i386_compare_op1);
  else
    operands[1] = (*i386_compare_gen)(i386_compare_op0, i386_compare_op1);
}
  operand0 = operands[0];
  operand1 = operands[1];
  emit (operand1);
  emit_insn (gen_rtx (SET, VOIDmode, operand0, gen_rtx (NE, QImode, cc0_rtx, const0_rtx)));
 _done:
  _val = gen_sequence ();
 _fail:
  end_sequence ();
  return _val;
}

rtx
gen_sgt (operand0)
     rtx operand0;
{
  rtx operand1;
  rtx operands[2];
  rtx _val = 0;
  start_sequence ();
  operands[0] = operand0;
operands[1] = (*i386_compare_gen)(i386_compare_op0, i386_compare_op1);
  operand0 = operands[0];
  operand1 = operands[1];
  emit (operand1);
  emit_insn (gen_rtx (SET, VOIDmode, operand0, gen_rtx (GT, QImode, cc0_rtx, const0_rtx)));
 _done:
  _val = gen_sequence ();
 _fail:
  end_sequence ();
  return _val;
}

rtx
gen_sgtu (operand0)
     rtx operand0;
{
  rtx operand1;
  rtx operands[2];
  rtx _val = 0;
  start_sequence ();
  operands[0] = operand0;
operands[1] = (*i386_compare_gen)(i386_compare_op0, i386_compare_op1);
  operand0 = operands[0];
  operand1 = operands[1];
  emit (operand1);
  emit_insn (gen_rtx (SET, VOIDmode, operand0, gen_rtx (GTU, QImode, cc0_rtx, const0_rtx)));
 _done:
  _val = gen_sequence ();
 _fail:
  end_sequence ();
  return _val;
}

rtx
gen_slt (operand0)
     rtx operand0;
{
  rtx operand1;
  rtx operands[2];
  rtx _val = 0;
  start_sequence ();
  operands[0] = operand0;
operands[1] = (*i386_compare_gen)(i386_compare_op0, i386_compare_op1);
  operand0 = operands[0];
  operand1 = operands[1];
  emit (operand1);
  emit_insn (gen_rtx (SET, VOIDmode, operand0, gen_rtx (LT, QImode, cc0_rtx, const0_rtx)));
 _done:
  _val = gen_sequence ();
 _fail:
  end_sequence ();
  return _val;
}

rtx
gen_sltu (operand0)
     rtx operand0;
{
  rtx operand1;
  rtx operands[2];
  rtx _val = 0;
  start_sequence ();
  operands[0] = operand0;
operands[1] = (*i386_compare_gen)(i386_compare_op0, i386_compare_op1);
  operand0 = operands[0];
  operand1 = operands[1];
  emit (operand1);
  emit_insn (gen_rtx (SET, VOIDmode, operand0, gen_rtx (LTU, QImode, cc0_rtx, const0_rtx)));
 _done:
  _val = gen_sequence ();
 _fail:
  end_sequence ();
  return _val;
}

rtx
gen_sge (operand0)
     rtx operand0;
{
  rtx operand1;
  rtx operands[2];
  rtx _val = 0;
  start_sequence ();
  operands[0] = operand0;
operands[1] = (*i386_compare_gen)(i386_compare_op0, i386_compare_op1);
  operand0 = operands[0];
  operand1 = operands[1];
  emit (operand1);
  emit_insn (gen_rtx (SET, VOIDmode, operand0, gen_rtx (GE, QImode, cc0_rtx, const0_rtx)));
 _done:
  _val = gen_sequence ();
 _fail:
  end_sequence ();
  return _val;
}

rtx
gen_sgeu (operand0)
     rtx operand0;
{
  rtx operand1;
  rtx operands[2];
  rtx _val = 0;
  start_sequence ();
  operands[0] = operand0;
operands[1] = (*i386_compare_gen)(i386_compare_op0, i386_compare_op1);
  operand0 = operands[0];
  operand1 = operands[1];
  emit (operand1);
  emit_insn (gen_rtx (SET, VOIDmode, operand0, gen_rtx (GEU, QImode, cc0_rtx, const0_rtx)));
 _done:
  _val = gen_sequence ();
 _fail:
  end_sequence ();
  return _val;
}

rtx
gen_sle (operand0)
     rtx operand0;
{
  rtx operand1;
  rtx operands[2];
  rtx _val = 0;
  start_sequence ();
  operands[0] = operand0;
operands[1] = (*i386_compare_gen)(i386_compare_op0, i386_compare_op1);
  operand0 = operands[0];
  operand1 = operands[1];
  emit (operand1);
  emit_insn (gen_rtx (SET, VOIDmode, operand0, gen_rtx (LE, QImode, cc0_rtx, const0_rtx)));
 _done:
  _val = gen_sequence ();
 _fail:
  end_sequence ();
  return _val;
}

rtx
gen_sleu (operand0)
     rtx operand0;
{
  rtx operand1;
  rtx operands[2];
  rtx _val = 0;
  start_sequence ();
  operands[0] = operand0;
operands[1] = (*i386_compare_gen)(i386_compare_op0, i386_compare_op1);
  operand0 = operands[0];
  operand1 = operands[1];
  emit (operand1);
  emit_insn (gen_rtx (SET, VOIDmode, operand0, gen_rtx (LEU, QImode, cc0_rtx, const0_rtx)));
 _done:
  _val = gen_sequence ();
 _fail:
  end_sequence ();
  return _val;
}

rtx
gen_beq (operand0)
     rtx operand0;
{
  rtx operand1;
  rtx operands[2];
  rtx _val = 0;
  start_sequence ();
  operands[0] = operand0;

{
  if (TARGET_IEEE_FP
      && GET_MODE_CLASS (GET_MODE (i386_compare_op0)) == MODE_FLOAT)
    operands[1] = (*i386_compare_gen_eq)(i386_compare_op0, i386_compare_op1);
  else
    operands[1] = (*i386_compare_gen)(i386_compare_op0, i386_compare_op1);
}
  operand0 = operands[0];
  operand1 = operands[1];
  emit (operand1);
  emit_jump_insn (gen_rtx (SET, VOIDmode, pc_rtx, gen_rtx (IF_THEN_ELSE, VOIDmode, gen_rtx (EQ, VOIDmode, cc0_rtx, const0_rtx), gen_rtx (LABEL_REF, VOIDmode, operand0), pc_rtx)));
 _done:
  _val = gen_sequence ();
 _fail:
  end_sequence ();
  return _val;
}

rtx
gen_bne (operand0)
     rtx operand0;
{
  rtx operand1;
  rtx operands[2];
  rtx _val = 0;
  start_sequence ();
  operands[0] = operand0;

{
  if (TARGET_IEEE_FP
      && GET_MODE_CLASS (GET_MODE (i386_compare_op0)) == MODE_FLOAT)
    operands[1] = (*i386_compare_gen_eq)(i386_compare_op0, i386_compare_op1);
  else
    operands[1] = (*i386_compare_gen)(i386_compare_op0, i386_compare_op1);
}
  operand0 = operands[0];
  operand1 = operands[1];
  emit (operand1);
  emit_jump_insn (gen_rtx (SET, VOIDmode, pc_rtx, gen_rtx (IF_THEN_ELSE, VOIDmode, gen_rtx (NE, VOIDmode, cc0_rtx, const0_rtx), gen_rtx (LABEL_REF, VOIDmode, operand0), pc_rtx)));
 _done:
  _val = gen_sequence ();
 _fail:
  end_sequence ();
  return _val;
}

rtx
gen_bgt (operand0)
     rtx operand0;
{
  rtx operand1;
  rtx operands[2];
  rtx _val = 0;
  start_sequence ();
  operands[0] = operand0;
operands[1] = (*i386_compare_gen)(i386_compare_op0, i386_compare_op1);
  operand0 = operands[0];
  operand1 = operands[1];
  emit (operand1);
  emit_jump_insn (gen_rtx (SET, VOIDmode, pc_rtx, gen_rtx (IF_THEN_ELSE, VOIDmode, gen_rtx (GT, VOIDmode, cc0_rtx, const0_rtx), gen_rtx (LABEL_REF, VOIDmode, operand0), pc_rtx)));
 _done:
  _val = gen_sequence ();
 _fail:
  end_sequence ();
  return _val;
}

rtx
gen_bgtu (operand0)
     rtx operand0;
{
  rtx operand1;
  rtx operands[2];
  rtx _val = 0;
  start_sequence ();
  operands[0] = operand0;
operands[1] = (*i386_compare_gen)(i386_compare_op0, i386_compare_op1);
  operand0 = operands[0];
  operand1 = operands[1];
  emit (operand1);
  emit_jump_insn (gen_rtx (SET, VOIDmode, pc_rtx, gen_rtx (IF_THEN_ELSE, VOIDmode, gen_rtx (GTU, VOIDmode, cc0_rtx, const0_rtx), gen_rtx (LABEL_REF, VOIDmode, operand0), pc_rtx)));
 _done:
  _val = gen_sequence ();
 _fail:
  end_sequence ();
  return _val;
}

rtx
gen_blt (operand0)
     rtx operand0;
{
  rtx operand1;
  rtx operands[2];
  rtx _val = 0;
  start_sequence ();
  operands[0] = operand0;
operands[1] = (*i386_compare_gen)(i386_compare_op0, i386_compare_op1);
  operand0 = operands[0];
  operand1 = operands[1];
  emit (operand1);
  emit_jump_insn (gen_rtx (SET, VOIDmode, pc_rtx, gen_rtx (IF_THEN_ELSE, VOIDmode, gen_rtx (LT, VOIDmode, cc0_rtx, const0_rtx), gen_rtx (LABEL_REF, VOIDmode, operand0), pc_rtx)));
 _done:
  _val = gen_sequence ();
 _fail:
  end_sequence ();
  return _val;
}

rtx
gen_bltu (operand0)
     rtx operand0;
{
  rtx operand1;
  rtx operands[2];
  rtx _val = 0;
  start_sequence ();
  operands[0] = operand0;
operands[1] = (*i386_compare_gen)(i386_compare_op0, i386_compare_op1);
  operand0 = operands[0];
  operand1 = operands[1];
  emit (operand1);
  emit_jump_insn (gen_rtx (SET, VOIDmode, pc_rtx, gen_rtx (IF_THEN_ELSE, VOIDmode, gen_rtx (LTU, VOIDmode, cc0_rtx, const0_rtx), gen_rtx (LABEL_REF, VOIDmode, operand0), pc_rtx)));
 _done:
  _val = gen_sequence ();
 _fail:
  end_sequence ();
  return _val;
}

rtx
gen_bge (operand0)
     rtx operand0;
{
  rtx operand1;
  rtx operands[2];
  rtx _val = 0;
  start_sequence ();
  operands[0] = operand0;
operands[1] = (*i386_compare_gen)(i386_compare_op0, i386_compare_op1);
  operand0 = operands[0];
  operand1 = operands[1];
  emit (operand1);
  emit_jump_insn (gen_rtx (SET, VOIDmode, pc_rtx, gen_rtx (IF_THEN_ELSE, VOIDmode, gen_rtx (GE, VOIDmode, cc0_rtx, const0_rtx), gen_rtx (LABEL_REF, VOIDmode, operand0), pc_rtx)));
 _done:
  _val = gen_sequence ();
 _fail:
  end_sequence ();
  return _val;
}

rtx
gen_bgeu (operand0)
     rtx operand0;
{
  rtx operand1;
  rtx operands[2];
  rtx _val = 0;
  start_sequence ();
  operands[0] = operand0;
operands[1] = (*i386_compare_gen)(i386_compare_op0, i386_compare_op1);
  operand0 = operands[0];
  operand1 = operands[1];
  emit (operand1);
  emit_jump_insn (gen_rtx (SET, VOIDmode, pc_rtx, gen_rtx (IF_THEN_ELSE, VOIDmode, gen_rtx (GEU, VOIDmode, cc0_rtx, const0_rtx), gen_rtx (LABEL_REF, VOIDmode, operand0), pc_rtx)));
 _done:
  _val = gen_sequence ();
 _fail:
  end_sequence ();
  return _val;
}

rtx
gen_ble (operand0)
     rtx operand0;
{
  rtx operand1;
  rtx operands[2];
  rtx _val = 0;
  start_sequence ();
  operands[0] = operand0;
operands[1] = (*i386_compare_gen)(i386_compare_op0, i386_compare_op1);
  operand0 = operands[0];
  operand1 = operands[1];
  emit (operand1);
  emit_jump_insn (gen_rtx (SET, VOIDmode, pc_rtx, gen_rtx (IF_THEN_ELSE, VOIDmode, gen_rtx (LE, VOIDmode, cc0_rtx, const0_rtx), gen_rtx (LABEL_REF, VOIDmode, operand0), pc_rtx)));
 _done:
  _val = gen_sequence ();
 _fail:
  end_sequence ();
  return _val;
}

rtx
gen_bleu (operand0)
     rtx operand0;
{
  rtx operand1;
  rtx operands[2];
  rtx _val = 0;
  start_sequence ();
  operands[0] = operand0;
operands[1] = (*i386_compare_gen)(i386_compare_op0, i386_compare_op1);
  operand0 = operands[0];
  operand1 = operands[1];
  emit (operand1);
  emit_jump_insn (gen_rtx (SET, VOIDmode, pc_rtx, gen_rtx (IF_THEN_ELSE, VOIDmode, gen_rtx (LEU, VOIDmode, cc0_rtx, const0_rtx), gen_rtx (LABEL_REF, VOIDmode, operand0), pc_rtx)));
 _done:
  _val = gen_sequence ();
 _fail:
  end_sequence ();
  return _val;
}

rtx
gen_jump (operand0)
     rtx operand0;
{
  return gen_rtx (SET, VOIDmode, pc_rtx, gen_rtx (LABEL_REF, VOIDmode, operand0));
}

rtx
gen_indirect_jump (operand0)
     rtx operand0;
{
  return gen_rtx (SET, VOIDmode, pc_rtx, operand0);
}

rtx
gen_casesi (operand0, operand1, operand2, operand3, operand4)
     rtx operand0;
     rtx operand1;
     rtx operand2;
     rtx operand3;
     rtx operand4;
{
  rtx operand5;
  rtx operands[6];
  rtx _val = 0;
  start_sequence ();
  operands[0] = operand0;
  operands[1] = operand1;
  operands[2] = operand2;
  operands[3] = operand3;
  operands[4] = operand4;

{
  operands[5] = gen_reg_rtx (SImode);
  current_function_uses_pic_offset_table = 1;
}
  operand0 = operands[0];
  operand1 = operands[1];
  operand2 = operands[2];
  operand3 = operands[3];
  operand4 = operands[4];
  operand5 = operands[5];
  emit_insn (gen_rtx (SET, VOIDmode, operand5, gen_rtx (MINUS, SImode, operand0, operand1)));
  emit_insn (gen_rtx (SET, VOIDmode, cc0_rtx, gen_rtx (COMPARE, CCmode, operand5, operand2)));
  emit_jump_insn (gen_rtx (SET, VOIDmode, pc_rtx, gen_rtx (IF_THEN_ELSE, VOIDmode, gen_rtx (GTU, VOIDmode, cc0_rtx, const0_rtx), gen_rtx (LABEL_REF, VOIDmode, operand4), pc_rtx)));
  emit_jump_insn (gen_rtx (PARALLEL, VOIDmode, gen_rtvec (2,
		gen_rtx (SET, VOIDmode, pc_rtx, gen_rtx (MINUS, SImode, gen_rtx (REG, SImode, 3), gen_rtx (MEM, SImode, gen_rtx (PLUS, SImode, gen_rtx (MULT, SImode, operand5, GEN_INT (4)), gen_rtx (LABEL_REF, VOIDmode, operand3))))),
		gen_rtx (CLOBBER, VOIDmode, gen_rtx (SCRATCH, SImode, 0)))));
 _done:
  _val = gen_sequence ();
 _fail:
  end_sequence ();
  return _val;
}

rtx
gen_tablejump (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  return gen_rtx (PARALLEL, VOIDmode, gen_rtvec (2,
		gen_rtx (SET, VOIDmode, pc_rtx, operand0),
		gen_rtx (USE, VOIDmode, gen_rtx (LABEL_REF, VOIDmode, operand1))));
}

rtx
gen_call_pop (operand0, operand1, operand2, operand3)
     rtx operand0;
     rtx operand1;
     rtx operand2;
     rtx operand3;
{
  rtx operands[4];
  rtx _val = 0;
  start_sequence ();
  operands[0] = operand0;
  operands[1] = operand1;
  operands[2] = operand2;
  operands[3] = operand3;

{
  rtx addr;

  if (flag_pic)
    current_function_uses_pic_offset_table = 1;

  /* With half-pic, force the address into a register.  */
  addr = XEXP (operands[0], 0);
  if (GET_CODE (addr) != REG && HALF_PIC_P () && !CONSTANT_ADDRESS_P (addr))
    XEXP (operands[0], 0) = force_reg (Pmode, addr);

  if (! expander_call_insn_operand (operands[0], QImode))
    operands[0]
      = change_address (operands[0], VOIDmode,
			copy_to_mode_reg (Pmode, XEXP (operands[0], 0)));
}
  operand0 = operands[0];
  operand1 = operands[1];
  operand2 = operands[2];
  operand3 = operands[3];
  emit_call_insn (gen_rtx (PARALLEL, VOIDmode, gen_rtvec (2,
		gen_rtx (CALL, VOIDmode, operand0, operand1),
		gen_rtx (SET, VOIDmode, gen_rtx (REG, SImode, 7), gen_rtx (PLUS, SImode, gen_rtx (REG, SImode, 7), operand3)))));
 _done:
  _val = gen_sequence ();
 _fail:
  end_sequence ();
  return _val;
}

rtx
gen_call (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  rtx operands[2];
  rtx _val = 0;
  start_sequence ();
  operands[0] = operand0;
  operands[1] = operand1;

{
  rtx addr;

  if (flag_pic)
    current_function_uses_pic_offset_table = 1;

  /* With half-pic, force the address into a register.  */
  addr = XEXP (operands[0], 0);
  if (GET_CODE (addr) != REG && HALF_PIC_P () && !CONSTANT_ADDRESS_P (addr))
    XEXP (operands[0], 0) = force_reg (Pmode, addr);

  if (! expander_call_insn_operand (operands[0], QImode))
    operands[0]
      = change_address (operands[0], VOIDmode,
			copy_to_mode_reg (Pmode, XEXP (operands[0], 0)));
}
  operand0 = operands[0];
  operand1 = operands[1];
  emit_call_insn (gen_rtx (CALL, VOIDmode, operand0, operand1));
 _done:
  _val = gen_sequence ();
 _fail:
  end_sequence ();
  return _val;
}

rtx
gen_call_value_pop (operand0, operand1, operand2, operand3, operand4)
     rtx operand0;
     rtx operand1;
     rtx operand2;
     rtx operand3;
     rtx operand4;
{
  rtx operands[5];
  rtx _val = 0;
  start_sequence ();
  operands[0] = operand0;
  operands[1] = operand1;
  operands[2] = operand2;
  operands[3] = operand3;
  operands[4] = operand4;

{
  rtx addr;

  if (flag_pic)
    current_function_uses_pic_offset_table = 1;

  /* With half-pic, force the address into a register.  */
  addr = XEXP (operands[1], 0);
  if (GET_CODE (addr) != REG && HALF_PIC_P () && !CONSTANT_ADDRESS_P (addr))
    XEXP (operands[1], 0) = force_reg (Pmode, addr);

  if (! expander_call_insn_operand (operands[1], QImode))
    operands[1]
      = change_address (operands[1], VOIDmode,
			copy_to_mode_reg (Pmode, XEXP (operands[1], 0)));
}
  operand0 = operands[0];
  operand1 = operands[1];
  operand2 = operands[2];
  operand3 = operands[3];
  operand4 = operands[4];
  emit_call_insn (gen_rtx (PARALLEL, VOIDmode, gen_rtvec (2,
		gen_rtx (SET, VOIDmode, operand0, gen_rtx (CALL, VOIDmode, operand1, operand2)),
		gen_rtx (SET, VOIDmode, gen_rtx (REG, SImode, 7), gen_rtx (PLUS, SImode, gen_rtx (REG, SImode, 7), operand4)))));
 _done:
  _val = gen_sequence ();
 _fail:
  end_sequence ();
  return _val;
}

rtx
gen_call_value (operand0, operand1, operand2)
     rtx operand0;
     rtx operand1;
     rtx operand2;
{
  rtx operands[3];
  rtx _val = 0;
  start_sequence ();
  operands[0] = operand0;
  operands[1] = operand1;
  operands[2] = operand2;

{
  rtx addr;

  if (flag_pic)
    current_function_uses_pic_offset_table = 1;

  /* With half-pic, force the address into a register.  */
  addr = XEXP (operands[1], 0);
  if (GET_CODE (addr) != REG && HALF_PIC_P () && !CONSTANT_ADDRESS_P (addr))
    XEXP (operands[1], 0) = force_reg (Pmode, addr);

  if (! expander_call_insn_operand (operands[1], QImode))
    operands[1]
      = change_address (operands[1], VOIDmode,
			copy_to_mode_reg (Pmode, XEXP (operands[1], 0)));
}
  operand0 = operands[0];
  operand1 = operands[1];
  operand2 = operands[2];
  emit_call_insn (gen_rtx (SET, VOIDmode, operand0, gen_rtx (CALL, VOIDmode, operand1, operand2)));
 _done:
  _val = gen_sequence ();
 _fail:
  end_sequence ();
  return _val;
}

rtx
gen_untyped_call (operand0, operand1, operand2)
     rtx operand0;
     rtx operand1;
     rtx operand2;
{
  rtx operands[3];
  rtx _val = 0;
  start_sequence ();
  operands[0] = operand0;
  operands[1] = operand1;
  operands[2] = operand2;

{
  rtx addr;

  if (flag_pic)
    current_function_uses_pic_offset_table = 1;

  /* With half-pic, force the address into a register.  */
  addr = XEXP (operands[0], 0);
  if (GET_CODE (addr) != REG && HALF_PIC_P () && !CONSTANT_ADDRESS_P (addr))
    XEXP (operands[0], 0) = force_reg (Pmode, addr);

  operands[1] = change_address (operands[1], DImode, XEXP (operands[1], 0));
  if (! expander_call_insn_operand (operands[1], QImode))
    operands[1]
      = change_address (operands[1], VOIDmode,
			copy_to_mode_reg (Pmode, XEXP (operands[1], 0)));
}
  operand0 = operands[0];
  operand1 = operands[1];
  operand2 = operands[2];
  emit_call_insn (gen_rtx (PARALLEL, VOIDmode, gen_rtvec (3,
		gen_rtx (CALL, VOIDmode, operand0, const0_rtx),
		operand1,
		operand2)));
 _done:
  _val = gen_sequence ();
 _fail:
  end_sequence ();
  return _val;
}

rtx
gen_untyped_return (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  rtx operands[2];
  rtx _val = 0;
  start_sequence ();
  operands[0] = operand0;
  operands[1] = operand1;

{
  rtx valreg1 = gen_rtx (REG, SImode, 0);
  rtx valreg2 = gen_rtx (REG, SImode, 1);
  rtx result = operands[0];

  /* Restore the FPU state.  */
  emit_insn (gen_update_return (change_address (result, SImode,
						plus_constant (XEXP (result, 0),
							       8))));

  /* Reload the function value registers.  */
  emit_move_insn (valreg1, change_address (result, SImode, XEXP (result, 0)));
  emit_move_insn (valreg2,
		  change_address (result, SImode,
				  plus_constant (XEXP (result, 0), 4)));

  /* Put USE insns before the return.  */
  emit_insn (gen_rtx (USE, VOIDmode, valreg1));
  emit_insn (gen_rtx (USE, VOIDmode, valreg2));

  /* Construct the return.  */
  expand_null_return ();

  DONE;
}
  operand0 = operands[0];
  operand1 = operands[1];
  emit (operand0);
  emit (operand1);
 _done:
  _val = gen_sequence ();
 _fail:
  end_sequence ();
  return _val;
}

rtx
gen_update_return (operand0)
     rtx operand0;
{
  return gen_rtx (UNSPEC, SImode, gen_rtvec (1,
		operand0), 0);
}

rtx
gen_return ()
{
  return gen_rtx (RETURN, VOIDmode);
}

rtx
gen_nop ()
{
  return const0_rtx;
}

rtx
gen_movstrsi (operand0, operand1, operand2, operand3)
     rtx operand0;
     rtx operand1;
     rtx operand2;
     rtx operand3;
{
  rtx operand4;
  rtx operand5;
  rtx operand6;
  rtx operands[7];
  rtx _val = 0;
  start_sequence ();
  operands[0] = operand0;
  operands[1] = operand1;
  operands[2] = operand2;
  operands[3] = operand3;

{
  rtx addr0, addr1;

  if (GET_CODE (operands[2]) != CONST_INT)
    FAIL;

  addr0 = copy_to_mode_reg (Pmode, XEXP (operands[0], 0));
  addr1 = copy_to_mode_reg (Pmode, XEXP (operands[1], 0));

  operands[5] = addr0;
  operands[6] = addr1;

  operands[0] = gen_rtx (MEM, BLKmode, addr0);
  operands[1] = gen_rtx (MEM, BLKmode, addr1);
}
  operand0 = operands[0];
  operand1 = operands[1];
  operand2 = operands[2];
  operand3 = operands[3];
  operand4 = operands[4];
  operand5 = operands[5];
  operand6 = operands[6];
  emit (gen_rtx (PARALLEL, VOIDmode, gen_rtvec (6,
		gen_rtx (SET, VOIDmode, operand0, operand1),
		gen_rtx (USE, VOIDmode, operand2),
		gen_rtx (USE, VOIDmode, operand3),
		gen_rtx (CLOBBER, VOIDmode, gen_rtx (SCRATCH, SImode, 0)),
		gen_rtx (CLOBBER, VOIDmode, operand5),
		gen_rtx (CLOBBER, VOIDmode, operand6))));
 _done:
  _val = gen_sequence ();
 _fail:
  end_sequence ();
  return _val;
}

rtx
gen_cmpstrsi (operand0, operand1, operand2, operand3, operand4)
     rtx operand0;
     rtx operand1;
     rtx operand2;
     rtx operand3;
     rtx operand4;
{
  rtx operand5;
  rtx operand6;
  rtx operands[7];
  rtx _val = 0;
  start_sequence ();
  operands[0] = operand0;
  operands[1] = operand1;
  operands[2] = operand2;
  operands[3] = operand3;
  operands[4] = operand4;

{
  rtx addr1, addr2;

  addr1 = copy_to_mode_reg (Pmode, XEXP (operands[1], 0));
  addr2 = copy_to_mode_reg (Pmode, XEXP (operands[2], 0));
  operands[3] = copy_to_mode_reg (SImode, operands[3]);

  operands[5] = addr1;
  operands[6] = addr2;

  operands[1] = gen_rtx (MEM, BLKmode, addr1);
  operands[2] = gen_rtx (MEM, BLKmode, addr2);

}
  operand0 = operands[0];
  operand1 = operands[1];
  operand2 = operands[2];
  operand3 = operands[3];
  operand4 = operands[4];
  operand5 = operands[5];
  operand6 = operands[6];
  emit (gen_rtx (PARALLEL, VOIDmode, gen_rtvec (6,
		gen_rtx (SET, VOIDmode, operand0, gen_rtx (COMPARE, SImode, operand1, operand2)),
		gen_rtx (USE, VOIDmode, operand3),
		gen_rtx (USE, VOIDmode, operand4),
		gen_rtx (CLOBBER, VOIDmode, operand5),
		gen_rtx (CLOBBER, VOIDmode, operand6),
		gen_rtx (CLOBBER, VOIDmode, operand3))));
 _done:
  _val = gen_sequence ();
 _fail:
  end_sequence ();
  return _val;
}

rtx
gen_ffssi2 (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  rtx operand2;
  rtx operand3;
  rtx operands[4];
  rtx _val = 0;
  start_sequence ();
  operands[0] = operand0;
  operands[1] = operand1;
operands[3] = gen_reg_rtx (SImode);
  operand0 = operands[0];
  operand1 = operands[1];
  operand2 = operands[2];
  operand3 = operands[3];
  emit (gen_rtx (PARALLEL, VOIDmode, gen_rtvec (2,
		gen_rtx (SET, VOIDmode, operand3, gen_rtx (PLUS, SImode, gen_rtx (FFS, SImode, operand1), constm1_rtx)),
		gen_rtx (CLOBBER, VOIDmode, gen_rtx (SCRATCH, SImode, 0)))));
  emit_insn (gen_rtx (SET, VOIDmode, operand0, gen_rtx (PLUS, SImode, operand3, const1_rtx)));
 _done:
  _val = gen_sequence ();
 _fail:
  end_sequence ();
  return _val;
}

rtx
gen_ffshi2 (operand0, operand1)
     rtx operand0;
     rtx operand1;
{
  rtx operand2;
  rtx operand3;
  rtx operands[4];
  rtx _val = 0;
  start_sequence ();
  operands[0] = operand0;
  operands[1] = operand1;
operands[3] = gen_reg_rtx (HImode);
  operand0 = operands[0];
  operand1 = operands[1];
  operand2 = operands[2];
  operand3 = operands[3];
  emit (gen_rtx (PARALLEL, VOIDmode, gen_rtvec (2,
		gen_rtx (SET, VOIDmode, operand3, gen_rtx (PLUS, HImode, gen_rtx (FFS, HImode, operand1), constm1_rtx)),
		gen_rtx (CLOBBER, VOIDmode, gen_rtx (SCRATCH, HImode, 0)))));
  emit_insn (gen_rtx (SET, VOIDmode, operand0, gen_rtx (PLUS, HImode, operand3, const1_rtx)));
 _done:
  _val = gen_sequence ();
 _fail:
  end_sequence ();
  return _val;
}

rtx
gen_strlensi (operand0, operand1, operand2, operand3)
     rtx operand0;
     rtx operand1;
     rtx operand2;
     rtx operand3;
{
  rtx operand4;
  rtx operand5;
  rtx operands[6];
  rtx _val = 0;
  start_sequence ();
  operands[0] = operand0;
  operands[1] = operand1;
  operands[2] = operand2;
  operands[3] = operand3;

{
  operands[1] = copy_to_mode_reg (SImode, XEXP (operands[1], 0));
  operands[4] = gen_reg_rtx (SImode);
  operands[5] = gen_reg_rtx (SImode);
}
  operand0 = operands[0];
  operand1 = operands[1];
  operand2 = operands[2];
  operand3 = operands[3];
  operand4 = operands[4];
  operand5 = operands[5];
  emit (gen_rtx (PARALLEL, VOIDmode, gen_rtvec (2,
		gen_rtx (SET, VOIDmode, operand4, gen_rtx (UNSPEC, SImode, gen_rtvec (3,
		gen_rtx (MEM, BLKmode, operand1),
		operand2,
		operand3), 0)),
		gen_rtx (CLOBBER, VOIDmode, operand1))));
  emit_insn (gen_rtx (SET, VOIDmode, operand5, gen_rtx (NOT, SImode, operand4)));
  emit_insn (gen_rtx (SET, VOIDmode, operand0, gen_rtx (MINUS, SImode, operand5, const1_rtx)));
 _done:
  _val = gen_sequence ();
 _fail:
  end_sequence ();
  return _val;
}



void
add_clobbers (pattern, insn_code_number)
     rtx pattern;
     int insn_code_number;
{
  int i;

  switch (insn_code_number)
    {
    case 250:
    case 223:
      XVECEXP (pattern, 0, 1) = gen_rtx (CLOBBER, VOIDmode, gen_rtx (SCRATCH, SImode, 0));
      break;

    case 72:
    case 71:
      XVECEXP (pattern, 0, 3) = gen_rtx (CLOBBER, VOIDmode, gen_rtx (SCRATCH, SImode, 0));
      break;

    case 68:
    case 67:
      XVECEXP (pattern, 0, 4) = gen_rtx (CLOBBER, VOIDmode, gen_rtx (SCRATCH, SImode, 0));
      break;

    case 252:
    case 25:
    case 24:
    case 23:
    case 22:
    case 21:
    case 20:
    case 19:
    case 18:
    case 17:
    case 16:
    case 8:
    case 6:
      XVECEXP (pattern, 0, 1) = gen_rtx (CLOBBER, VOIDmode, gen_rtx (SCRATCH, HImode, 0));
      break;

    default:
      abort ();
    }
}

void
init_mov_optab ()
{
#ifdef HAVE_movccfpeq
  if (HAVE_movccfpeq)
    mov_optab->handlers[(int) CCFPEQmode].insn_code = CODE_FOR_movccfpeq;
#endif
}
