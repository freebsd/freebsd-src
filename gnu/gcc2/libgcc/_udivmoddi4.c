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
static const UQItype __clz_tab[] =
{
  0,1,2,2,3,3,3,3,4,4,4,4,4,4,4,4,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,
  6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
  7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
  7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
  8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
  8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
  8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
  8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
};
UDItype
__udivmoddi4 (n, d, rp)
     UDItype n, d;
     UDItype *rp;
{
  DIunion ww;
  DIunion nn, dd;
  DIunion rr;
  USItype d0, d1, n0, n1, n2;
  USItype q0, q1;
  USItype b, bm;
  nn.ll = n;
  dd.ll = d;
  d0 = dd.s.low;
  d1 = dd.s.high;
  n0 = nn.s.low;
  n1 = nn.s.high;
  if (d1 == 0)
    {
      if (d0 > n1)
	{
	   
	  __asm__ ("divl %4"	: "=a" ((USItype)( q0 )),	"=d" ((USItype)(  n0 ))	: "0" ((USItype)(  n0 )),	"1" ((USItype)(  n1 )),	"rm" ((USItype)(  d0 ))) ;
	  q1 = 0;
	   
	}
      else
	{
	   
	  if (d0 == 0)
	    d0 = 1 / d0;	 
	  __asm__ ("divl %4"	: "=a" ((USItype)( q1 )),	"=d" ((USItype)(  n1 ))	: "0" ((USItype)(  n1 )),	"1" ((USItype)(  0 )),	"rm" ((USItype)(  d0 ))) ;
	  __asm__ ("divl %4"	: "=a" ((USItype)( q0 )),	"=d" ((USItype)(  n0 ))	: "0" ((USItype)(  n0 )),	"1" ((USItype)(  n1 )),	"rm" ((USItype)(  d0 ))) ;
	   
	}
      if (rp != 0)
	{
	  rr.s.low = n0;
	  rr.s.high = 0;
	  *rp = rr.ll;
	}
    }
  else
    {
      if (d1 > n1)
	{
	   
	  q0 = 0;
	  q1 = 0;
	   
	  if (rp != 0)
	    {
	      rr.s.low = n0;
	      rr.s.high = n1;
	      *rp = rr.ll;
	    }
	}
      else
	{
	   
	  do {	USItype __cbtmp;	__asm__ ("bsrl %1,%0"	: "=r" (__cbtmp) : "rm" ((USItype)(  d1 )));	( bm ) = __cbtmp ^ 31;	} while (0) ;
	  if (bm == 0)
	    {
	       
	       
	      if (n1 > d1 || n0 >= d0)
		{
		  q0 = 1;
		  __asm__ ("subl %5,%1
	sbbl %3,%0"	: "=r" ((USItype)( n1 )),	"=&r" ((USItype)(  n0 ))	: "0" ((USItype)(  n1 )),	"g" ((USItype)(  d1 )),	"1" ((USItype)(  n0 )),	"g" ((USItype)(  d0 ))) ;
		}
	      else
		q0 = 0;
	      q1 = 0;
	      if (rp != 0)
		{
		  rr.s.low = n0;
		  rr.s.high = n1;
		  *rp = rr.ll;
		}
	    }
	  else
	    {
	      USItype m1, m0;
	       
	      b = (sizeof (SItype) * 8 )  - bm;
	      d1 = (d1 << bm) | (d0 >> b);
	      d0 = d0 << bm;
	      n2 = n1 >> b;
	      n1 = (n1 << bm) | (n0 >> b);
	      n0 = n0 << bm;
	      __asm__ ("divl %4"	: "=a" ((USItype)( q0 )),	"=d" ((USItype)(  n1 ))	: "0" ((USItype)(  n1 )),	"1" ((USItype)(  n2 )),	"rm" ((USItype)(  d1 ))) ;
	      __asm__ ("mull %3"	: "=a" ((USItype)(  m0 )),	"=d" ((USItype)( m1 ))	: "%0" ((USItype)(  q0 )),	"rm" ((USItype)(  d0 ))) ;
	      if (m1 > n1 || (m1 == n1 && m0 > n0))
		{
		  q0--;
		  __asm__ ("subl %5,%1
	sbbl %3,%0"	: "=r" ((USItype)( m1 )),	"=&r" ((USItype)(  m0 ))	: "0" ((USItype)(  m1 )),	"g" ((USItype)(  d1 )),	"1" ((USItype)(  m0 )),	"g" ((USItype)(  d0 ))) ;
		}
	      q1 = 0;
	       
	      if (rp != 0)
		{
		  __asm__ ("subl %5,%1
	sbbl %3,%0"	: "=r" ((USItype)( n1 )),	"=&r" ((USItype)(  n0 ))	: "0" ((USItype)(  n1 )),	"g" ((USItype)(  m1 )),	"1" ((USItype)(  n0 )),	"g" ((USItype)(  m0 ))) ;
		  rr.s.low = (n1 << b) | (n0 >> bm);
		  rr.s.high = n1 >> bm;
		  *rp = rr.ll;
		}
	    }
	}
    }
  ww.s.low = q0;
  ww.s.high = q1;
  return ww.ll;
}
