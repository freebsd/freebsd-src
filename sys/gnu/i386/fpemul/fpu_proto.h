/*
 *
 *    $Id: fpu_proto.h,v 1.4 1995/12/14 09:50:23 phk Exp $
 *
 */


/* errors.c */
extern void Un_impl(void);
extern void emu_printall(void);
extern void exception(int n);
extern void real_2op_NaN(FPU_REG * a, FPU_REG * b, FPU_REG * dest);
extern void arith_invalid(FPU_REG * dest);
extern void divide_by_zero(int sign, FPU_REG * dest);
extern void set_precision_flag_up(void);
extern void set_precision_flag_down(void);
extern int denormal_operand(void);
extern void arith_overflow(FPU_REG * dest);
extern void arith_underflow(FPU_REG * dest);
extern void stack_overflow(void);
extern void stack_underflow(void);
extern void stack_underflow_i(int i);
extern void stack_underflow_pop(int i);
/* fpu_arith.c */
extern void fadd__(void);
extern void fmul__(void);
extern void fsub__(void);
extern void fsubr_(void);
extern void fdiv__(void);
extern void fdivr_(void);
extern void fadd_i(void);
extern void fmul_i(void);
extern void fsubri(void);
extern void fsub_i(void);
extern void fdivri(void);
extern void fdiv_i(void);
extern void faddp_(void);
extern void fmulp_(void);
extern void fsubrp(void);
extern void fsubp_(void);
extern void fdivrp(void);
extern void fdivp_(void);
/* fpu_aux.c */
extern void finit(void);
extern void finit_(void);
extern void fstsw_(void);
extern void fp_nop(void);
extern void fld_i_(void);
extern void fxch_i(void);
extern void ffree_(void);
extern void ffreep(void);
extern void fst_i_(void);
extern void fstp_i(void);
/* fpu_entry.c */
#if 0
extern int math_emulate(struct trapframe * info);
#endif
/* fpu_etc.c */
extern void fp_etc(void);
/* fpu_trig.c */
extern void trig_a(void);
extern void trig_b(void);
/* get_address.c */
extern void get_address(unsigned char FPU_modrm);
/* load_store.c */
extern void load_store_instr(char type);
/* poly_2xm1.c */
extern int poly_2xm1(FPU_REG * arg, FPU_REG * result);
/* poly_atan.c */
extern void poly_atan(FPU_REG * arg);
/* poly_l2.c */
extern void poly_l2(FPU_REG * arg, FPU_REG * result);
extern int poly_l2p1(FPU_REG * arg, FPU_REG * result);
/* poly_sin.c */
extern void poly_sine(FPU_REG * arg, FPU_REG * result);
/* poly_tan.c */
extern void poly_tan(FPU_REG * arg, FPU_REG * y_reg);
/* reg_add_sub.c */
extern void reg_add(FPU_REG * a, FPU_REG * b, FPU_REG * dest, int control_w);
extern void reg_sub(FPU_REG * a, FPU_REG * b, FPU_REG * dest, int control_w);
/* reg_compare.c */
extern int compare(FPU_REG * b);
extern int compare_st_data(void);
extern void fcom_st(void);
extern void fcompst(void);
extern void fcompp(void);
extern void fucom_(void);
extern void fucomp(void);
extern void fucompp(void);
/* reg_constant.c */
extern void fconst(void);
/* reg_ld_str.c */
extern void reg_load_extended(void);
extern void reg_load_double(void);
extern void reg_load_single(void);
extern void reg_load_int64(void);
extern void reg_load_int32(void);
extern void reg_load_int16(void);
extern void reg_load_bcd(void);
extern int reg_store_extended(void);
extern int reg_store_double(void);
extern int reg_store_single(void);
extern int reg_store_int64(void);
extern int reg_store_int32(void);
extern int reg_store_int16(void);
extern int reg_store_bcd(void);
extern int round_to_int(FPU_REG * r);
extern char *fldenv(void);
extern void frstor(void);
extern unsigned short tag_word(void);
extern char *fstenv(void);
extern void fsave(void);
/* reg_mul.c */
extern void reg_mul(FPU_REG * a, FPU_REG * b, FPU_REG * dest, unsigned int control_w);
