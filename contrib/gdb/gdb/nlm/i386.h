/* Register values.  All of these values *MUST* agree with tm.h */
#define SP_REGNUM 4		/* Contains address of top of stack */
#define PC_REGNUM 8		/* Contains program counter */
#define FP_REGNUM 5		/* Virtual frame pointer */
#define NUM_REGS 16		/* Number of machine registers */
#define REGISTER_BYTES (NUM_REGS * 4) /* Total size of registers array */

#define ExceptionPC ExceptionEIP
#define DECR_PC_AFTER_BREAK 1	/* int 3 leaves PC pointing after insn */
#define BREAKPOINT {0xcc}
#define BREAKPOINT_SIZE (sizeof breakpoint_insn)

#define StackFrame T_TSS_StackFrame
