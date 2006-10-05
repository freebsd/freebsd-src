
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/* FPop1 */
#define FMOVQ    0x0003
#define FNEGQ    0x0007
#define FADDQ    0x0403
#define FADDQ    0x0407
#define FQTOI    0x0D03
#define FABSQ    0x000B
#define FSQRTS   0x0209
#define FSQRTD   0x020A
#define FSQRTQ   0x020B
#define FMULQ    0x040B
#define FDIVQ    0x040F
#define FDMULQ   0x060E
#define FXTOQ    0x080C
#define FQTOD    0x0C0B
#define FITOQ    0x0C0D
#define FSTOQ    0x0C0E

/* FPop2 */
#define FMOVQZ    0x027
#define FMOVQLE   0x047
#define FCMPQ     0x053
#define FCMPEQ    0x057
#define FMOVQLZ   0x067
#define FMOVQNZ   0x0A7
#define FMOVQ0    0x003
#define FMOVQ1    0x043
#define FMOVQ2    0x083
#define FMOVQ3    0x0C3
#define FMOVQI    0x103
#define FMOVQX    0x183

#define REGINFO(ftt, rd, rdu, rs2, rs2u, rs1, rs1u) \
     reginfo = (rs1u << 2) | (rs1 << 0) | (rs2u << 5) | (rs2 << 3) \
            | (rdu << 8) | (rd << 6) | (ftt << 9)

void fpemu(struct trapframe *tf, uint64_t type, uint64_t fsr)
{

	uint32_t insn;

	if (tf->tf_tstate & TSTATE_PRIV)
		panic("unimplemented FPop in kernel");
	if (copyin(tf->tf_tpc, &insn) != EFAULT) {
		if ((insn & FPOP_MASK) == FPOP1) {
			switch ((insn >> 5) & 0x1ff) {
			case FMOVQ:
			case FNEGQ:
			case FABSQ: REGINFO(3,3,0,3,0,0,0); break;
			case FSQRTQ: REGINFO(3,3,1,3,1,0,0); break;
			case FADDQ:
			case FSUBQ:
			case FMULQ:
			case FDIVQ: REGINFO(3,3,1,3,1,3,1); break;
			case FDMULQ: REGINFO(3,3,1,2,1,2,1); break;
			case FQTOX: REGINFO(3,2,0,3,1,0,0); break;
			case FXTOQ: REGINFO(3,3,1,2,0,0,0); break;
			case FQTOS: REGINFO(3,1,1,3,1,0,0); break;
			case FQTOD: REGINFO(3,2,1,3,1,0,0); break;
			case FITOQ: REGINFO(3,3,1,1,0,0,0); break;
			case FSTOQ: REGINFO(3,3,1,1,1,0,0); break;
			case FDTOQ: REGINFO(3,3,1,2,1,0,0); break;
			case FQTOI: REGINFO(3,1,0,3,1,0,0); break;
				/* SUBNORMAL - ftt == 2 */
			case FSQRTS: REGINFO(2,1,1,1,1,0,0); break;
			case FSQRTD: REGINFO(2,2,1,2,1,0,0); break;
			case FADDD:
			case FSUBD:
			case FMULD:
			case FDIVD: REGINFO(2,2,1,2,1,2,1); break;
			case FADDS:
			case FSUBS:
			case FMULS:
			case FDIVS: REGINFO(2,1,1,1,1,1,1); break;
			case FSMULD: REGINFO(2,2,1,1,1,1,1); break;
			case FSTOX: REGINFO(2,2,0,1,1,0,0); break;
			case FDTOX: REGINFO(2,2,0,2,1,0,0); break;
			case FDTOS: REGINFO(2,1,1,2,1,0,0); break;
			case FSTOD: REGINFO(2,2,1,1,1,0,0); break;
			case FSTOI: REGINFO(2,1,0,1,1,0,0); break;
			case FDTOI: REGINFO(2,1,0,2,1,0,0); break;
			}

		} else if ((insn & FPOP_MASK) == FPOP2) {
			IR = 2;
			switch ((insn >> 5) & 0x1ff) {
			case FCMPQ: TYPE(3,0,0,3,1,3,1); break;
			case FCMPEQ: TYPE(3,0,0,3,1,3,1); break;
				/* Now the conditional fmovq support */
			case FMOVQ0:
			case FMOVQ1:
			case FMOVQ2:
			case FMOVQ3:
				/* fmovq %fccX, %fY, %fZ */
				if (!((insn >> 11) & 3))
					XR = tf->tf_fsr >> 10;
				else
					XR = tf->tf_fsr >> (30 + ((insn >> 10) & 0x6));
				XR &= 3;
				IR = 0;
				switch ((insn >> 14) & 0x7) {
					/* case 0: IR = 0; break; *//* Never */
				case 1: if (XR) IR = 1; break;/* Not Equal */
				case 2: if (XR == 1 || XR == 2) IR = 1; break;/* Less or Greater */
				case 3: if (XR & 1) IR = 1; break;/* Unordered or Less */
				case 4: if (XR == 1) IR = 1; break;/* Less */
				case 5: if (XR & 2) IR = 1; break;/* Unordered or Greater */
				case 6: if (XR == 2) IR = 1; break;/* Greater */
				case 7: if (XR == 3) IR = 1; break;/* Unordered */
				}
				if ((insn >> 14) & 8)
					IR ^= 1;
				break;
			case FMOVQI:
			case FMOVQX:
				/* fmovq %[ix]cc, %fY, %fZ */
				XR = tf->tf_tstate >> 32;
				if ((insn >> 5) & 0x80)
					XR >>= 4;
				XR &= 0xf;
				IR = 0;
				freg = ((XR >> 2) ^ XR) & 2;
				switch ((insn >> 14) & 0x7) {
					/* case 0: IR = 0; break; *//* Never */
				case 1: if (XR & 4) IR = 1; break;/* Equal */
				case 2: if ((XR & 4) || freg) IR = 1; break;/* Less or Equal */
				case 3: if (freg) IR = 1; break;/* Less */
				case 4: if (XR & 5) IR = 1; break;/* Less or Equal Unsigned */
				case 5: if (XR & 1) IR = 1; break;/* Carry Set */
				case 6: if (XR & 8) IR = 1; break;/* Negative */
				case 7: if (XR & 2) IR = 1; break;/* Overflow Set */
				}
				if ((insn >> 14) & 8)
					IR ^= 1;
				break;
			case FMOVQZ:
			case FMOVQLE:
			case FMOVQLZ:
			case FMOVQNZ:
			case FMOVQGZ:
			case FMOVQGE:
				freg = (insn >> 14) & 0x1f;
				KASSERT(freg < 16, ("freg too large"));

				if (!freg)
					XR = 0;
				else if (freg < 16)
					XR = regs->u_regs[freg];

#if 0 
				else if (test_thread_flag(TIF_32BIT)) {
					struct reg_window32 __user *win32;
					flushw_user ();
					win32 = (struct reg_window32 __user *)((unsigned long)((u32)regs->u_regs[UREG_FP]));
					get_user(XR, &win32->locals[freg - 16]);
				} else {
					struct reg_window __user *win;
					flushw_user ();
					win = (struct reg_window __user *)(regs->u_regs[UREG_FP] + STACK_BIAS);
					get_user(XR, &win->locals[freg - 16]);
				}
#endif 
				IR = 0;
				switch ((insn >> 10) & 3) {
				case 1: if (!XR) IR = 1; break;/* Register Zero */
				case 2: if (XR <= 0) IR = 1; break;/* Register Less Than or Equal to Zero */
				case 3: if (XR < 0) IR = 1; break;/* Register Less Than Zero */
				}
				if ((insn >> 10) & 4)
					IR ^= 1;
				break;
			}
			if (IR == 0) {
				/* The fmov test was false. Do a nop instead */
				tf->tf_fsr &= ~(FSR_CEXC_MASK);
				tf->tf_tpc = tf->tf_tnpc;
				tf->tf_tnpc += 4;
				return 1;
			} else if (IR == 1) {
				/* Change the instruction into plain fmovq */
				insn = (insn & 0x3e00001f) | 0x81a00060;
				REGINFO(3,3,0,3,0,0,0); 
			}
		}
	}

}
