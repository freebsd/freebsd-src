typedef long Long;

/* The following enum is used to access the special registers in 
   the saved machine state.  */

typedef enum
{
  kDc_SavedPC =	0,		/* really SRR0 */
  kDc_SavedMSR = 1,		/* really SRR1 */
  kDc_SavedCR =	2,
  kDc_SavedLR =	3,
  kDc_SavedDSISR = 4,
  kDc_SavedDAR = 5,
  kDc_SavedXER = 6,
  kDc_SavedCTR = 7,
  kDc_SavedSDR1 = 8,
  kDc_SavedRTCU = 9,
  kDc_SavedRTCL = 10,
  kDc_SavedDEC = 11,
  kDc_SavedSR00 = 12,		/* The Segement Registers are consecutive */
  kDc_SavedSR01 = 13,		/* kDc_SavedSR00 + n is supported */
  kDc_SavedSR02 = 14,
  kDc_SavedSR03 = 15,
  kDc_SavedSR04 = 16,
  kDc_SavedSR05 = 17,
  kDc_SavedSR06 = 18,
  kDc_SavedSR07 = 19,
  kDc_SavedSR08 = 20,
  kDc_SavedSR09 = 21,
  kDc_SavedSR10 = 22,
  kDc_SavedSR11 = 23,
  kDc_SavedSR12 = 24,
  kDc_SavedSR13 = 25,
  kDc_SavedSR14 = 26,
  kDc_SavedSR15 = 27,
  kDc_SavedFPSCR = 29,
  kDc_SavedMQ = 30,
  kDc_SavedBAT0U = 31,
  kDc_SavedBAT0L = 32,
  kDc_SavedBAT1U = 33,
  kDc_SavedBAT1L = 34,
  kDc_SavedBAT2U = 35,
  kDc_SavedBAT2L = 36,
  kDc_SavedBAT3U = 37,
  kDc_SavedBAT3L = 38,

  kNumberSpecialRegisters = 39
} Dc_SavedRegisterName;

/* Access to floating points is not very easy.  This allows the number to be
   accessed both as a floating number and as a pair of Longs.  */

typedef union
{
  double asfloat;		/* access the variable as a floating number */
  struct
    {
      Long high;
      Long low;
    }
  asLONG;			/* access the variable as two Longs */
} FloatingPoints;

/* The following is the standard record for Saving a machine state */

struct SavedMachineState
{
  FloatingPoints CSavedFPRegs[32]; /* The floating point registers [0->31] */
				/* ***32bit assumption*** */
  Long CsavedRegs[32];		/* space to save the General Registers */
				/* These are saved 0->31 */
  Long CexReason;
  Long SavedDomainID;
  union
    {				/* must be 8-byte aligned, so doubleFPSCR is 8-byte aligned */
      struct
	{
	  Long CsavedSRR0;	/* Index 0 - The saved PC */
	  Long CsavedSRR1;	/* 1 saved MSR */
	  Long CsavedCR;	/* 2 */
	  Long CsavedLR;	/* 3 */
	  Long CsavedDSISR;	/* 4 */
	  Long CsavedDAR;	/* 5 */

	  Long CsavedXER;	/* 6 */
	  Long CsavedCTR;	/* 7 */
	  Long CsavedSDR1;	/* 8 */
	  Long CsavedRTCU;	/* 9 */
	  Long CsavedRTCL;	/* 10 */
	  Long CsavedDEC;	/* 11 */
	  Long CsavedSR0;	/* 12 */
	  Long CsavedSR1;	/* 13 */
	  Long CsavedSR2;	/* 14 */
	  Long CsavedSR3;	/* 15 */
	  Long CsavedSR4;	/* 16 */
	  Long CsavedSR5;	/* 17 */
	  Long CsavedSR6;	/* 18 */
	  Long CsavedSR7;	/* 19 */
	  Long CsavedSR8;	/* 20 */
	  Long CsavedSR9;	/* 21 */
	  Long CsavedSR10;	/* 22 */
	  Long CsavedSR11;	/* 23 */
	  Long CsavedSR12;	/* 24 */
	  Long CsavedSR13;	/* 25 */
	  Long CsavedSR14;	/* 26 */
	  Long CsavedSR15;	/* 27 */
				/* CdoubleFPSCR must be double word aligned */
	  Long CdoubleFPSCR;	/* 28 this is the upper part of the store and has
				      no meaning */
	  Long CsavedFPSCR;	/* 29 */
	  Long CsavedMQ;	/* 30 */
	  Long CsavedBAT0U;	/* 31 */
	  Long CsavedBAT0L;	/* 32 */
	  Long CsavedBAT1U;	/* 33 */
	  Long CsavedBAT1L;	/* 34 */
	  Long CsavedBAT2U;	/* 35 */
	  Long CsavedBAT2L;	/* 36 */
	  Long CsavedBAT3U;	/* 37 */
	  Long CsavedBAT3L;	/* 38 */
	}
      SpecialRegistersEnumerated;

      Long SpecialRegistersIndexed[kNumberSpecialRegisters];
    } u;

  Long Padding[3];		/* Needed for quad-word alignment */
};

struct StackFrame
{
  LONG *ExceptionDomainID;
  /*ProcessorStructure*/ int *ExceptionProcessorID;
  BYTE *ExceptionDescription;
  LONG ExceptionFlags;
  LONG ExceptionErrorCode;
  LONG ExceptionNumber;
  struct SavedMachineState ExceptionState;
};

/* Register values.  All of these values *MUST* agree with tm.h */
#define	GP0_REGNUM 0		/* GPR register 0 */
#define SP_REGNUM 1		/* Contains address of top of stack */
#define FP0_REGNUM 32		/* FPR (Floating point) register 0 */
#define PC_REGNUM 64		/* Contains program counter */
#define PS_REGNUM 65		/* Processor (or machine) status (%msr) */
#define	CR_REGNUM 66		/* Condition register */
#define	LR_REGNUM 67		/* Link register */
#define	CTR_REGNUM 68		/* Count register */
#define	XER_REGNUM 69		/* Fixed point exception registers */
#define	MQ_REGNUM 70		/* Multiply/quotient register */
#define NUM_REGS 71		/* Number of machine registers */
#define REGISTER_BYTES (420)	/* Total size of registers array */

#define ExceptionPC ExceptionState.u.SpecialRegistersEnumerated.CsavedSRR0
#define DECR_PC_AFTER_BREAK 0	/* PPCs get this right! */
#define BREAKPOINT {0x7d, 0x82, 0x10, 0x08}
extern unsigned char breakpoint_insn[];
#define BREAKPOINT_SIZE 4

#if 0
#define ALTERNATE_MEM_FUNCS	/* We need our own get_char/set_char */
#endif

extern int get_char (char *addr);
extern void set_char (char *addr, int val);
