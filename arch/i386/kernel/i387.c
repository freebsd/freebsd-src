/*
 *  linux/arch/i386/kernel/i387.c
 *
 *  Copyright (C) 1994 Linus Torvalds
 *
 *  Pentium III FXSR, SSE support
 *  General FPU state handling cleanups
 *	Gareth Hughes <gareth@valinux.com>, May 2000
 */

#include <linux/config.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <asm/processor.h>
#include <asm/i387.h>
#include <asm/math_emu.h>
#include <asm/sigcontext.h>
#include <asm/user.h>
#include <asm/ptrace.h>
#include <asm/uaccess.h>

#ifdef CONFIG_MATH_EMULATION
#define HAVE_HWFP (boot_cpu_data.hard_math)
#else
#define HAVE_HWFP 1
#endif

static union i387_union empty_fpu_state;

void __init boot_init_fpu(void)
{
	memset(&empty_fpu_state, 0, sizeof(union i387_union));

	if (!cpu_has_fxsr) {
		empty_fpu_state.fsave.cwd = 0xffff037f;
		empty_fpu_state.fsave.swd = 0xffff0000;
		empty_fpu_state.fsave.twd = 0xffffffff;
		empty_fpu_state.fsave.fos = 0xffff0000;
	} else {
		empty_fpu_state.fxsave.cwd = 0x37f;
		if (cpu_has_xmm)
			empty_fpu_state.fxsave.mxcsr = 0x1f80;
	}
}

void load_empty_fpu(struct task_struct * tsk)
{
	memcpy(&tsk->thread.i387, &empty_fpu_state, sizeof(union i387_union));
}

/*
 * The _current_ task is using the FPU for the first time
 * so initialize it and set the mxcsr to its default
 * value at reset if we support XMM instructions and then
 * remeber the current task has used the FPU.
 */
void init_fpu(void)
{
	if (cpu_has_fxsr)
		asm volatile("fxrstor %0" : : "m" (empty_fpu_state.fxsave));
	else
		__asm__("fninit");
	current->used_math = 1;
}

/*
 * FPU lazy state save handling.
 */

static inline void __save_init_fpu( struct task_struct *tsk )
{
	if ( cpu_has_fxsr ) {
		asm volatile( "fxsave %0 ; fnclex"
			      : "=m" (tsk->thread.i387.fxsave) );
	} else {
		asm volatile( "fnsave %0 ; fwait"
			      : "=m" (tsk->thread.i387.fsave) );
	}
	tsk->flags &= ~PF_USEDFPU;
}

void save_init_fpu( struct task_struct *tsk )
{
	__save_init_fpu(tsk);
	stts();
}

void kernel_fpu_begin(void)
{
	struct task_struct *tsk = current;

	if (tsk->flags & PF_USEDFPU) {
		__save_init_fpu(tsk);
		return;
	}
	clts();
}

void restore_fpu( struct task_struct *tsk )
{
	if ( cpu_has_fxsr ) {
		asm volatile( "fxrstor %0"
			      : : "m" (tsk->thread.i387.fxsave) );
	} else {
		asm volatile( "frstor %0"
			      : : "m" (tsk->thread.i387.fsave) );
	}
}

/*
 * FPU tag word conversions.
 */

static inline unsigned short twd_i387_to_fxsr( unsigned short twd )
{
	unsigned int tmp; /* to avoid 16 bit prefixes in the code */
 
	/* Transform each pair of bits into 01 (valid) or 00 (empty) */
        tmp = ~twd;
        tmp = (tmp | (tmp>>1)) & 0x5555; /* 0V0V0V0V0V0V0V0V */
        /* and move the valid bits to the lower byte. */
        tmp = (tmp | (tmp >> 1)) & 0x3333; /* 00VV00VV00VV00VV */
        tmp = (tmp | (tmp >> 2)) & 0x0f0f; /* 0000VVVV0000VVVV */
        tmp = (tmp | (tmp >> 4)) & 0x00ff; /* 00000000VVVVVVVV */
        return tmp;
}

static inline unsigned long twd_fxsr_to_i387( struct i387_fxsave_struct *fxsave )
{
	struct _fpxreg *st = NULL;
	unsigned long twd = (unsigned long) fxsave->twd;
	unsigned long tag;
	unsigned long ret = 0xffff0000;
	int i;

#define FPREG_ADDR(f, n)	((char *)&(f)->st_space + (n) * 16);

	for ( i = 0 ; i < 8 ; i++ ) {
		if ( twd & 0x1 ) {
			st = (struct _fpxreg *) FPREG_ADDR( fxsave, i );

			switch ( st->exponent & 0x7fff ) {
			case 0x7fff:
				tag = 2;		/* Special */
				break;
			case 0x0000:
				if ( !st->significand[0] &&
				     !st->significand[1] &&
				     !st->significand[2] &&
				     !st->significand[3] ) {
					tag = 1;	/* Zero */
				} else {
					tag = 2;	/* Special */
				}
				break;
			default:
				if ( st->significand[3] & 0x8000 ) {
					tag = 0;	/* Valid */
				} else {
					tag = 2;	/* Special */
				}
				break;
			}
		} else {
			tag = 3;			/* Empty */
		}
		ret |= (tag << (2 * i));
		twd = twd >> 1;
	}
	return ret;
}

/*
 * FPU state interaction.
 */

unsigned short get_fpu_cwd( struct task_struct *tsk )
{
	if ( cpu_has_fxsr ) {
		return tsk->thread.i387.fxsave.cwd;
	} else {
		return (unsigned short)tsk->thread.i387.fsave.cwd;
	}
}

unsigned short get_fpu_swd( struct task_struct *tsk )
{
	if ( cpu_has_fxsr ) {
		return tsk->thread.i387.fxsave.swd;
	} else {
		return (unsigned short)tsk->thread.i387.fsave.swd;
	}
}

unsigned short get_fpu_twd( struct task_struct *tsk )
{
	if ( cpu_has_fxsr ) {
		return tsk->thread.i387.fxsave.twd;
	} else {
		return (unsigned short)tsk->thread.i387.fsave.twd;
	}
}

unsigned short get_fpu_mxcsr( struct task_struct *tsk )
{
	if ( cpu_has_xmm ) {
		return tsk->thread.i387.fxsave.mxcsr;
	} else {
		return 0x1f80;
	}
}

void set_fpu_cwd( struct task_struct *tsk, unsigned short cwd )
{
	if ( cpu_has_fxsr ) {
		tsk->thread.i387.fxsave.cwd = cwd;
	} else {
		tsk->thread.i387.fsave.cwd = ((long)cwd | 0xffff0000);
	}
}

void set_fpu_swd( struct task_struct *tsk, unsigned short swd )
{
	if ( cpu_has_fxsr ) {
		tsk->thread.i387.fxsave.swd = swd;
	} else {
		tsk->thread.i387.fsave.swd = ((long)swd | 0xffff0000);
	}
}

void set_fpu_twd( struct task_struct *tsk, unsigned short twd )
{
	if ( cpu_has_fxsr ) {
		tsk->thread.i387.fxsave.twd = twd_i387_to_fxsr(twd);
	} else {
		tsk->thread.i387.fsave.twd = ((long)twd | 0xffff0000);
	}
}

void set_fpu_mxcsr( struct task_struct *tsk, unsigned short mxcsr )
{
	if ( cpu_has_xmm ) {
		tsk->thread.i387.fxsave.mxcsr = (mxcsr & 0xffbf);
	}
}

/*
 * FXSR floating point environment conversions.
 */

static inline int convert_fxsr_to_user( struct _fpstate *buf,
					struct i387_fxsave_struct *fxsave )
{
	unsigned long env[7];
	struct _fpreg *to;
	struct _fpxreg *from;
	int i;

	env[0] = (unsigned long)fxsave->cwd | 0xffff0000;
	env[1] = (unsigned long)fxsave->swd | 0xffff0000;
	env[2] = twd_fxsr_to_i387(fxsave);
	env[3] = fxsave->fip;
	env[4] = fxsave->fcs | ((unsigned long)fxsave->fop << 16);
	env[5] = fxsave->foo;
	env[6] = fxsave->fos;

	if ( __copy_to_user( buf, env, 7 * sizeof(unsigned long) ) )
		return 1;

	to = &buf->_st[0];
	from = (struct _fpxreg *) &fxsave->st_space[0];
	for ( i = 0 ; i < 8 ; i++, to++, from++ ) {
		if ( __copy_to_user( to, from, sizeof(*to) ) )
			return 1;
	}
	return 0;
}

static inline int convert_fxsr_from_user( struct i387_fxsave_struct *fxsave,
					  struct _fpstate *buf )
{
	unsigned long env[7];
	struct _fpxreg *to;
	struct _fpreg *from;
	int i;

	if ( __copy_from_user( env, buf, 7 * sizeof(long) ) )
		return 1;

	fxsave->cwd = (unsigned short)(env[0] & 0xffff);
	fxsave->swd = (unsigned short)(env[1] & 0xffff);
	fxsave->twd = twd_i387_to_fxsr((unsigned short)(env[2] & 0xffff));
	fxsave->fip = env[3];
	fxsave->fop = (unsigned short)((env[4] & 0xffff0000) >> 16);
	fxsave->fcs = (env[4] & 0xffff);
	fxsave->foo = env[5];
	fxsave->fos = env[6];

	to = (struct _fpxreg *) &fxsave->st_space[0];
	from = &buf->_st[0];
	for ( i = 0 ; i < 8 ; i++, to++, from++ ) {
		if ( __copy_from_user( to, from, sizeof(*from) ) )
			return 1;
	}
	return 0;
}

/*
 * Signal frame handlers.
 */

static inline int save_i387_fsave( struct _fpstate *buf )
{
	struct task_struct *tsk = current;

	unlazy_fpu( tsk );
	tsk->thread.i387.fsave.status = tsk->thread.i387.fsave.swd;
	if ( __copy_to_user( buf, &tsk->thread.i387.fsave,
			     sizeof(struct i387_fsave_struct) ) )
		return -1;
	return 1;
}

static inline int save_i387_fxsave( struct _fpstate *buf )
{
	struct task_struct *tsk = current;
	int err = 0;

	unlazy_fpu( tsk );

	if ( convert_fxsr_to_user( buf, &tsk->thread.i387.fxsave ) )
		return -1;

	err |= __put_user( tsk->thread.i387.fxsave.swd, &buf->status );
	err |= __put_user( X86_FXSR_MAGIC, &buf->magic );
	if ( err )
		return -1;

	if ( __copy_to_user( &buf->_fxsr_env[0], &tsk->thread.i387.fxsave,
			     sizeof(struct i387_fxsave_struct) ) )
		return -1;
	return 1;
}

int save_i387( struct _fpstate *buf )
{
	if ( !current->used_math )
		return 0;

	/* This will cause a "finit" to be triggered by the next
	 * attempted FPU operation by the 'current' process.
	 */
	current->used_math = 0;

	if ( HAVE_HWFP ) {
		if ( cpu_has_fxsr ) {
			return save_i387_fxsave( buf );
		} else {
			return save_i387_fsave( buf );
		}
	} else {
		return save_i387_soft( &current->thread.i387.soft, buf );
	}
}

static inline int restore_i387_fsave( struct _fpstate *buf )
{
	struct task_struct *tsk = current;
	clear_fpu( tsk );
	return __copy_from_user( &tsk->thread.i387.fsave, buf,
				 sizeof(struct i387_fsave_struct) );
}

static inline int restore_i387_fxsave( struct _fpstate *buf )
{
	int err;
	struct task_struct *tsk = current;
	clear_fpu( tsk );
	err = __copy_from_user( &tsk->thread.i387.fxsave, &buf->_fxsr_env[0],
				sizeof(struct i387_fxsave_struct) );
	/* mxcsr bit 6 and 31-16 must be zero for security reasons */
	tsk->thread.i387.fxsave.mxcsr &= 0xffbf;
	return err ? 1 : convert_fxsr_from_user( &tsk->thread.i387.fxsave, buf );
}

int restore_i387( struct _fpstate *buf )
{
	int err;

	if ( HAVE_HWFP ) {
		if ( cpu_has_fxsr ) {
			err =  restore_i387_fxsave( buf );
		} else {
			err = restore_i387_fsave( buf );
		}
	} else {
		err = restore_i387_soft( &current->thread.i387.soft, buf );
	}
	current->used_math = 1;
	return err;
}

/*
 * ptrace request handlers.
 */

static inline int get_fpregs_fsave( struct user_i387_struct *buf,
				    struct task_struct *tsk )
{
	return __copy_to_user( buf, &tsk->thread.i387.fsave,
			       sizeof(struct user_i387_struct) );
}

static inline int get_fpregs_fxsave( struct user_i387_struct *buf,
				     struct task_struct *tsk )
{
	return convert_fxsr_to_user( (struct _fpstate *)buf,
				     &tsk->thread.i387.fxsave );
}

int get_fpregs( struct user_i387_struct *buf, struct task_struct *tsk )
{
	if ( HAVE_HWFP ) {
		if ( cpu_has_fxsr ) {
			return get_fpregs_fxsave( buf, tsk );
		} else {
			return get_fpregs_fsave( buf, tsk );
		}
	} else {
		return save_i387_soft( &tsk->thread.i387.soft,
				       (struct _fpstate *)buf );
	}
}

static inline int set_fpregs_fsave( struct task_struct *tsk,
				    struct user_i387_struct *buf )
{
	return __copy_from_user( &tsk->thread.i387.fsave, buf,
				 sizeof(struct user_i387_struct) );
}

static inline int set_fpregs_fxsave( struct task_struct *tsk,
				     struct user_i387_struct *buf )
{
	return convert_fxsr_from_user( &tsk->thread.i387.fxsave,
				       (struct _fpstate *)buf );
}

int set_fpregs( struct task_struct *tsk, struct user_i387_struct *buf )
{
	if ( HAVE_HWFP ) {
		if ( cpu_has_fxsr ) {
			return set_fpregs_fxsave( tsk, buf );
		} else {
			return set_fpregs_fsave( tsk, buf );
		}
	} else {
		return restore_i387_soft( &tsk->thread.i387.soft,
					  (struct _fpstate *)buf );
	}
}

int get_fpxregs( struct user_fxsr_struct *buf, struct task_struct *tsk )
{
	if ( cpu_has_fxsr ) {
		if (__copy_to_user( (void *)buf, &tsk->thread.i387.fxsave,
				    sizeof(struct user_fxsr_struct) ))
			return -EFAULT;
		return 0;
	} else {
		return -EIO;
	}
}

int set_fpxregs( struct task_struct *tsk, struct user_fxsr_struct *buf )
{
	if ( cpu_has_fxsr ) {
		__copy_from_user( &tsk->thread.i387.fxsave, (void *)buf,
				  sizeof(struct user_fxsr_struct) );
		/* mxcsr bit 6 and 31-16 must be zero for security reasons */
		tsk->thread.i387.fxsave.mxcsr &= 0xffbf;
		return 0;
	} else {
		return -EIO;
	}
}

/*
 * FPU state for core dumps.
 */

static inline void copy_fpu_fsave( struct task_struct *tsk,
				   struct user_i387_struct *fpu )
{
	memcpy( fpu, &tsk->thread.i387.fsave,
		sizeof(struct user_i387_struct) );
}

static inline void copy_fpu_fxsave( struct task_struct *tsk,
				   struct user_i387_struct *fpu )
{
	unsigned short *to;
	unsigned short *from;
	int i;

	memcpy( fpu, &tsk->thread.i387.fxsave, 7 * sizeof(long) );

	to = (unsigned short *)&fpu->st_space[0];
	from = (unsigned short *)&tsk->thread.i387.fxsave.st_space[0];
	for ( i = 0 ; i < 8 ; i++, to += 5, from += 8 ) {
		memcpy( to, from, 5 * sizeof(unsigned short) );
	}
}

int dump_fpu( struct pt_regs *regs, struct user_i387_struct *fpu )
{
	int fpvalid;
	struct task_struct *tsk = current;

	fpvalid = tsk->used_math;
	if ( fpvalid ) {
		unlazy_fpu( tsk );
		if ( cpu_has_fxsr ) {
			copy_fpu_fxsave( tsk, fpu );
		} else {
			copy_fpu_fsave( tsk, fpu );
		}
	}

	return fpvalid;
}

int dump_extended_fpu( struct pt_regs *regs, struct user_fxsr_struct *fpu )
{
	int fpvalid;
	struct task_struct *tsk = current;

	fpvalid = tsk->used_math && cpu_has_fxsr;
	if ( fpvalid ) {
		unlazy_fpu( tsk );
		memcpy( fpu, &tsk->thread.i387.fxsave,
			sizeof(struct user_fxsr_struct) );
	}

	return fpvalid;
}
