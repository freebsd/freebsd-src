/* Generated automatically by the program `genopinit'
from the machine description file `md'.  */

#include "config.h"
#include "rtl.h"
#include "flags.h"
#include "insn-flags.h"
#include "insn-codes.h"
#include "insn-config.h"
#include "recog.h"
#include "expr.h"
#include "reload.h"

void
init_all_optabs ()
{
  tst_optab->handlers[(int) SImode].insn_code = CODE_FOR_tstsi;
  tst_optab->handlers[(int) HImode].insn_code = CODE_FOR_tsthi;
  tst_optab->handlers[(int) QImode].insn_code = CODE_FOR_tstqi;
  if (HAVE_tstsf)
    tst_optab->handlers[(int) SFmode].insn_code = CODE_FOR_tstsf;
  if (HAVE_tstdf)
    tst_optab->handlers[(int) DFmode].insn_code = CODE_FOR_tstdf;
  cmp_optab->handlers[(int) SImode].insn_code = CODE_FOR_cmpsi;
  cmp_optab->handlers[(int) HImode].insn_code = CODE_FOR_cmphi;
  cmp_optab->handlers[(int) QImode].insn_code = CODE_FOR_cmpqi;
  if (HAVE_cmpdf)
    cmp_optab->handlers[(int) DFmode].insn_code = CODE_FOR_cmpdf;
  if (HAVE_cmpsf)
    cmp_optab->handlers[(int) SFmode].insn_code = CODE_FOR_cmpsf;
  mov_optab->handlers[(int) SImode].insn_code = CODE_FOR_movsi;
  mov_optab->handlers[(int) HImode].insn_code = CODE_FOR_movhi;
  movstrict_optab->handlers[(int) HImode].insn_code = CODE_FOR_movstricthi;
  mov_optab->handlers[(int) QImode].insn_code = CODE_FOR_movqi;
  movstrict_optab->handlers[(int) QImode].insn_code = CODE_FOR_movstrictqi;
  mov_optab->handlers[(int) SFmode].insn_code = CODE_FOR_movsf;
  mov_optab->handlers[(int) DFmode].insn_code = CODE_FOR_movdf;
  mov_optab->handlers[(int) DImode].insn_code = CODE_FOR_movdi;
  extendtab[(int) SImode][(int) HImode][1] = CODE_FOR_zero_extendhisi2;
  extendtab[(int) HImode][(int) QImode][1] = CODE_FOR_zero_extendqihi2;
  extendtab[(int) SImode][(int) QImode][1] = CODE_FOR_zero_extendqisi2;
  extendtab[(int) DImode][(int) SImode][1] = CODE_FOR_zero_extendsidi2;
  extendtab[(int) DImode][(int) SImode][0] = CODE_FOR_extendsidi2;
  extendtab[(int) SImode][(int) HImode][0] = CODE_FOR_extendhisi2;
  extendtab[(int) HImode][(int) QImode][0] = CODE_FOR_extendqihi2;
  extendtab[(int) SImode][(int) QImode][0] = CODE_FOR_extendqisi2;
  if (HAVE_extendsfdf2)
    extendtab[(int) DFmode][(int) SFmode][0] = CODE_FOR_extendsfdf2;
  if (HAVE_fixuns_truncdfsi2)
    fixtrunctab[(int) DFmode][(int) SImode][1] = CODE_FOR_fixuns_truncdfsi2;
  if (HAVE_fixuns_truncsfsi2)
    fixtrunctab[(int) SFmode][(int) SImode][1] = CODE_FOR_fixuns_truncsfsi2;
  if (HAVE_fix_truncdfdi2)
    fixtrunctab[(int) DFmode][(int) DImode][0] = CODE_FOR_fix_truncdfdi2;
  if (HAVE_fix_truncsfdi2)
    fixtrunctab[(int) SFmode][(int) DImode][0] = CODE_FOR_fix_truncsfdi2;
  if (HAVE_fix_truncdfsi2)
    fixtrunctab[(int) DFmode][(int) SImode][0] = CODE_FOR_fix_truncdfsi2;
  if (HAVE_fix_truncsfsi2)
    fixtrunctab[(int) SFmode][(int) SImode][0] = CODE_FOR_fix_truncsfsi2;
  if (HAVE_floatsisf2)
    floattab[(int) SFmode][(int) SImode][0] = CODE_FOR_floatsisf2;
  if (HAVE_floatdisf2)
    floattab[(int) SFmode][(int) DImode][0] = CODE_FOR_floatdisf2;
  if (HAVE_floatsidf2)
    floattab[(int) DFmode][(int) SImode][0] = CODE_FOR_floatsidf2;
  if (HAVE_floatdidf2)
    floattab[(int) DFmode][(int) DImode][0] = CODE_FOR_floatdidf2;
  add_optab->handlers[(int) DImode].insn_code = CODE_FOR_adddi3;
  add_optab->handlers[(int) SImode].insn_code = CODE_FOR_addsi3;
  add_optab->handlers[(int) HImode].insn_code = CODE_FOR_addhi3;
  add_optab->handlers[(int) QImode].insn_code = CODE_FOR_addqi3;
  if (HAVE_adddf3)
    add_optab->handlers[(int) DFmode].insn_code = CODE_FOR_adddf3;
  if (HAVE_addsf3)
    add_optab->handlers[(int) SFmode].insn_code = CODE_FOR_addsf3;
  sub_optab->handlers[(int) DImode].insn_code = CODE_FOR_subdi3;
  sub_optab->handlers[(int) SImode].insn_code = CODE_FOR_subsi3;
  sub_optab->handlers[(int) HImode].insn_code = CODE_FOR_subhi3;
  sub_optab->handlers[(int) QImode].insn_code = CODE_FOR_subqi3;
  if (HAVE_subdf3)
    sub_optab->handlers[(int) DFmode].insn_code = CODE_FOR_subdf3;
  if (HAVE_subsf3)
    sub_optab->handlers[(int) SFmode].insn_code = CODE_FOR_subsf3;
  smul_optab->handlers[(int) HImode].insn_code = CODE_FOR_mulhi3;
  smul_optab->handlers[(int) SImode].insn_code = CODE_FOR_mulsi3;
  if (HAVE_muldf3)
    smul_optab->handlers[(int) DFmode].insn_code = CODE_FOR_muldf3;
  if (HAVE_mulsf3)
    smul_optab->handlers[(int) SFmode].insn_code = CODE_FOR_mulsf3;
  sdiv_optab->handlers[(int) QImode].insn_code = CODE_FOR_divqi3;
  udiv_optab->handlers[(int) QImode].insn_code = CODE_FOR_udivqi3;
  if (HAVE_divdf3)
    flodiv_optab->handlers[(int) DFmode].insn_code = CODE_FOR_divdf3;
  if (HAVE_divsf3)
    flodiv_optab->handlers[(int) SFmode].insn_code = CODE_FOR_divsf3;
  sdivmod_optab->handlers[(int) SImode].insn_code = CODE_FOR_divmodsi4;
  sdivmod_optab->handlers[(int) HImode].insn_code = CODE_FOR_divmodhi4;
  udivmod_optab->handlers[(int) SImode].insn_code = CODE_FOR_udivmodsi4;
  udivmod_optab->handlers[(int) HImode].insn_code = CODE_FOR_udivmodhi4;
  and_optab->handlers[(int) SImode].insn_code = CODE_FOR_andsi3;
  and_optab->handlers[(int) HImode].insn_code = CODE_FOR_andhi3;
  and_optab->handlers[(int) QImode].insn_code = CODE_FOR_andqi3;
  ior_optab->handlers[(int) SImode].insn_code = CODE_FOR_iorsi3;
  ior_optab->handlers[(int) HImode].insn_code = CODE_FOR_iorhi3;
  ior_optab->handlers[(int) QImode].insn_code = CODE_FOR_iorqi3;
  xor_optab->handlers[(int) SImode].insn_code = CODE_FOR_xorsi3;
  xor_optab->handlers[(int) HImode].insn_code = CODE_FOR_xorhi3;
  xor_optab->handlers[(int) QImode].insn_code = CODE_FOR_xorqi3;
  neg_optab->handlers[(int) DImode].insn_code = CODE_FOR_negdi2;
  neg_optab->handlers[(int) SImode].insn_code = CODE_FOR_negsi2;
  neg_optab->handlers[(int) HImode].insn_code = CODE_FOR_neghi2;
  neg_optab->handlers[(int) QImode].insn_code = CODE_FOR_negqi2;
  if (HAVE_negsf2)
    neg_optab->handlers[(int) SFmode].insn_code = CODE_FOR_negsf2;
  if (HAVE_negdf2)
    neg_optab->handlers[(int) DFmode].insn_code = CODE_FOR_negdf2;
  if (HAVE_abssf2)
    abs_optab->handlers[(int) SFmode].insn_code = CODE_FOR_abssf2;
  if (HAVE_absdf2)
    abs_optab->handlers[(int) DFmode].insn_code = CODE_FOR_absdf2;
  if (HAVE_sqrtsf2)
    sqrt_optab->handlers[(int) SFmode].insn_code = CODE_FOR_sqrtsf2;
  if (HAVE_sqrtdf2)
    sqrt_optab->handlers[(int) DFmode].insn_code = CODE_FOR_sqrtdf2;
  if (HAVE_sindf2)
    sin_optab->handlers[(int) DFmode].insn_code = CODE_FOR_sindf2;
  if (HAVE_sinsf2)
    sin_optab->handlers[(int) SFmode].insn_code = CODE_FOR_sinsf2;
  if (HAVE_cosdf2)
    cos_optab->handlers[(int) DFmode].insn_code = CODE_FOR_cosdf2;
  if (HAVE_cossf2)
    cos_optab->handlers[(int) SFmode].insn_code = CODE_FOR_cossf2;
  one_cmpl_optab->handlers[(int) SImode].insn_code = CODE_FOR_one_cmplsi2;
  one_cmpl_optab->handlers[(int) HImode].insn_code = CODE_FOR_one_cmplhi2;
  one_cmpl_optab->handlers[(int) QImode].insn_code = CODE_FOR_one_cmplqi2;
  ashl_optab->handlers[(int) DImode].insn_code = CODE_FOR_ashldi3;
  ashl_optab->handlers[(int) SImode].insn_code = CODE_FOR_ashlsi3;
  ashl_optab->handlers[(int) HImode].insn_code = CODE_FOR_ashlhi3;
  ashl_optab->handlers[(int) QImode].insn_code = CODE_FOR_ashlqi3;
  ashr_optab->handlers[(int) DImode].insn_code = CODE_FOR_ashrdi3;
  ashr_optab->handlers[(int) SImode].insn_code = CODE_FOR_ashrsi3;
  ashr_optab->handlers[(int) HImode].insn_code = CODE_FOR_ashrhi3;
  ashr_optab->handlers[(int) QImode].insn_code = CODE_FOR_ashrqi3;
  lshr_optab->handlers[(int) DImode].insn_code = CODE_FOR_lshrdi3;
  lshr_optab->handlers[(int) SImode].insn_code = CODE_FOR_lshrsi3;
  lshr_optab->handlers[(int) HImode].insn_code = CODE_FOR_lshrhi3;
  lshr_optab->handlers[(int) QImode].insn_code = CODE_FOR_lshrqi3;
  rotl_optab->handlers[(int) SImode].insn_code = CODE_FOR_rotlsi3;
  rotl_optab->handlers[(int) HImode].insn_code = CODE_FOR_rotlhi3;
  rotl_optab->handlers[(int) QImode].insn_code = CODE_FOR_rotlqi3;
  rotr_optab->handlers[(int) SImode].insn_code = CODE_FOR_rotrsi3;
  rotr_optab->handlers[(int) HImode].insn_code = CODE_FOR_rotrhi3;
  rotr_optab->handlers[(int) QImode].insn_code = CODE_FOR_rotrqi3;
  setcc_gen_code[(int) EQ] = CODE_FOR_seq;
  setcc_gen_code[(int) NE] = CODE_FOR_sne;
  setcc_gen_code[(int) GT] = CODE_FOR_sgt;
  setcc_gen_code[(int) GTU] = CODE_FOR_sgtu;
  setcc_gen_code[(int) LT] = CODE_FOR_slt;
  setcc_gen_code[(int) LTU] = CODE_FOR_sltu;
  setcc_gen_code[(int) GE] = CODE_FOR_sge;
  setcc_gen_code[(int) GEU] = CODE_FOR_sgeu;
  setcc_gen_code[(int) LE] = CODE_FOR_sle;
  setcc_gen_code[(int) LEU] = CODE_FOR_sleu;
  bcc_gen_fctn[(int) EQ] = gen_beq;
  bcc_gen_fctn[(int) NE] = gen_bne;
  bcc_gen_fctn[(int) GT] = gen_bgt;
  bcc_gen_fctn[(int) GTU] = gen_bgtu;
  bcc_gen_fctn[(int) LT] = gen_blt;
  bcc_gen_fctn[(int) LTU] = gen_bltu;
  bcc_gen_fctn[(int) GE] = gen_bge;
  bcc_gen_fctn[(int) GEU] = gen_bgeu;
  bcc_gen_fctn[(int) LE] = gen_ble;
  bcc_gen_fctn[(int) LEU] = gen_bleu;
  movstr_optab[(int) SImode] = CODE_FOR_movstrsi;
  ffs_optab->handlers[(int) SImode].insn_code = CODE_FOR_ffssi2;
  ffs_optab->handlers[(int) HImode].insn_code = CODE_FOR_ffshi2;
  strlen_optab->handlers[(int) SImode].insn_code = CODE_FOR_strlensi;
}
