extern int target_flags;
  
enum reg_class
{
  NO_REGS,
  AREG, DREG, CREG, BREG,
  Q_REGS,			 
  SIREG, DIREG,
  INDEX_REGS,			 
  GENERAL_REGS,			 
  FP_TOP_REG, FP_SECOND_REG,	 
  FLOAT_REGS,
  ALL_REGS, LIM_REG_CLASSES
};
extern enum reg_class regclass_map[17 ];
  
   
extern struct rtx_def *i386_compare_op0, *i386_compare_op1;
extern struct rtx_def *(*i386_compare_gen)(), *(*i386_compare_gen_eq)();
extern char *hi_reg_name[];
extern char *qi_reg_name[];
extern char *qi_high_reg_name[];
  
union flt_or_value { float  i; float f; };
union flt_or_int { int i; float f; };
double
__floatsidf (a)
     long int  a;
{
  return (double)  a  ;
}
