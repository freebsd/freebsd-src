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
  
enum machine_mode {
  VOIDmode , 
  QImode , 		 
  HImode , 
  PSImode , 
  SImode , 
  PDImode , 
  DImode , 
  TImode , 
  OImode , 
  QFmode , 
  HFmode , 
  SFmode , 
  DFmode , 
  XFmode ,     
  TFmode , 
  SCmode , 
  DCmode , 
  XCmode , 
  TCmode , 
  CQImode , 
  CHImode , 
  CSImode , 
  CDImode , 
  CTImode , 
  COImode , 
  BLKmode , 
  CCmode , 
  CCFPEQmode ,
MAX_MACHINE_MODE };
extern char *mode_name[];
enum mode_class { MODE_RANDOM, MODE_INT, MODE_FLOAT, MODE_PARTIAL_INT, MODE_CC,
		  MODE_COMPLEX_INT, MODE_COMPLEX_FLOAT, MAX_MODE_CLASS};
extern enum mode_class mode_class[];
extern int mode_size[];
extern int mode_unit_size[];
extern enum machine_mode mode_wider_mode[];
extern enum machine_mode mode_for_size  (unsigned int, enum mode_class, int)  ;
extern enum machine_mode get_best_mode  (int, int, int, enum machine_mode, int)  ;
extern enum machine_mode class_narrowest_mode[];
extern enum machine_mode byte_mode;
extern enum machine_mode word_mode;
typedef int ptrdiff_t;
typedef unsigned int size_t;
typedef short unsigned int wchar_t;
typedef unsigned int UQItype	__attribute__ ((mode (QI)));
typedef 	 int SItype	__attribute__ ((mode (SI)));
typedef unsigned int USItype	__attribute__ ((mode (SI)));
typedef		 int DItype	__attribute__ ((mode (DI)));
typedef unsigned int UDItype	__attribute__ ((mode (DI)));
typedef 	float SFtype	__attribute__ ((mode (SF)));
typedef		float DFtype	__attribute__ ((mode (DF)));
typedef int word_type __attribute__ ((mode (SI)));
  struct DIstruct {SItype low, high;};
typedef union
{
  struct DIstruct s;
  DItype ll;
} DIunion;
extern DItype __fixunssfdi (SFtype a);
extern DItype __fixunsdfdi (DFtype a);
USItype
__udiv_w_sdiv (rp, a1, a0, d)
     USItype *rp, a1, a0, d;
{
  USItype q, r;
  USItype c0, c1, b1;
  if ((SItype) d >= 0)
    {
      if (a1 < d - a1 - (a0 >> ((sizeof (SItype) * 8 )  - 1)))
	{
	   
	  sdiv_qrnnd (q, r, a1, a0, d);
	}
      else
	{
	   
	  __asm__ ("subl %5,%1
	sbbl %3,%0"	: "=r" ((USItype)( c1 )),	"=&r" ((USItype)(  c0 ))	: "0" ((USItype)(  a1 )),	"g" ((USItype)(  d >> 1 )),	"1" ((USItype)(  a0 )),	"g" ((USItype)(  d << ((sizeof (SItype) * 8 )  - 1) ))) ;
	   
	  sdiv_qrnnd (q, r, c1, c0, d);
	   
	  q += (USItype) 1 << ((sizeof (SItype) * 8 )  - 1);
	}
    }
  else
    {
      b1 = d >> 1;			 
      c1 = a1 >> 1;			 
      c0 = (a1 << ((sizeof (SItype) * 8 )  - 1)) + (a0 >> 1);
      if (a1 < b1)			 
	{
	  sdiv_qrnnd (q, r, c1, c0, b1);  
	  r = 2*r + (a0 & 1);		 
	  if ((d & 1) != 0)
	    {
	      if (r >= q)
		r = r - q;
	      else if (q - r <= d)
		{
		  r = r - q + d;
		  q--;
		}
	      else
		{
		  r = r - q + 2*d;
		  q -= 2;
		}
	    }
	}
      else if (c1 < b1)			 
	{
	  c1 = (b1 - 1) - c1;
	  c0 = ~c0;			 
	  sdiv_qrnnd (q, r, c1, c0, b1);  
	  q = ~q;			 
	  r = (b1 - 1) - r;
	  r = 2*r + (a0 & 1);		 
	  if ((d & 1) != 0)
	    {
	      if (r >= q)
		r = r - q;
	      else if (q - r <= d)
		{
		  r = r - q + d;
		  q--;
		}
	      else
		{
		  r = r - q + 2*d;
		  q -= 2;
		}
	    }
	}
      else				 
	{				 
	  if (a0 >= -d)
	    {
	      q = -1;
	      r = a0 + d;
	    }
	  else
	    {
	      q = -2;
	      r = a0 + 2*d;
	    }
	}
    }
  *rp = r;
  return q;
}
