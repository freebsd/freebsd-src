/*-
 * Copyright (c) 1982, 1988, 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)systm.h	8.7 (Berkeley) 3/29/95
 * $FreeBSD$
 */

#ifndef _SYS_SYSTM_H_
#define	_SYS_SYSTM_H_

#include <machine/atomic.h>
#include <machine/cpufunc.h>
#include <sys/callout.h>
#include <sys/cdefs.h>
#include <sys/queue.h>
#include <sys/stdint.h>		/* for people using printf mainly */

extern int cold;		/* nonzero if we are doing a cold boot */
extern int rebooting;		/* kern_reboot() has been called. */
extern const char *panicstr;	/* panic message */
extern char version[];		/* system version */
extern char copyright[];	/* system copyright */
extern int kstack_pages;	/* number of kernel stack pages */

extern u_long pagesizes[];	/* supported page sizes */
extern long physmem;		/* physical memory */
extern long realmem;		/* 'real' memory */

extern char *rootdevnames[2];	/* names of possible root devices */

extern int boothowto;		/* reboot flags, from console subsystem */
extern int bootverbose;		/* nonzero to print verbose messages */

extern int maxusers;		/* system tune hint */
extern int ngroups_max;		/* max # of supplemental groups */
extern int vm_guest;		/* Running as virtual machine guest? */

/*
 * Detected virtual machine guest types. The intention is to expand
 * and/or add to the VM_GUEST_VM type if specific VM functionality is
 * ever implemented (e.g. vendor-specific paravirtualization features).
 */
enum VM_GUEST { VM_GUEST_NO = 0, VM_GUEST_VM, VM_GUEST_XEN };

#ifdef	INVARIANTS		/* The option is always available */
#define	KASSERT(exp,msg) do {						\
	if (__predict_false(!(exp)))					\
		panic msg;						\
} while (0)
#define	VNASSERT(exp, vp, msg) do {					\
	if (__predict_false(!(exp))) {					\
		vn_printf(vp, "VNASSERT failed\n");			\
		panic msg;						\
	}								\
} while (0)
#else
#define	KASSERT(exp,msg) do { \
} while (0)

#define	VNASSERT(exp, vp, msg) do { \
} while (0)
#endif

#ifndef CTASSERT		/* Allow lint to override */
#define	CTASSERT(x)		_CTASSERT(x, __LINE__)
#define	_CTASSERT(x, y)		__CTASSERT(x, y)
#define	__CTASSERT(x, y)	typedef char __assert ## y[(x) ? 1 : -1]
#endif

/*
 * Assert that a pointer can be loaded from memory atomically.
 *
 * This assertion enforces stronger alignment than necessary.  For example,
 * on some architectures, atomicity for unaligned loads will depend on
 * whether or not the load spans multiple cache lines.
 */
#define	ASSERT_ATOMIC_LOAD_PTR(var, msg)				\
	KASSERT(sizeof(var) == sizeof(void *) &&			\
	    ((uintptr_t)&(var) & (sizeof(void *) - 1)) == 0, msg)

/*
 * XXX the hints declarations are even more misplaced than most declarations
 * in this file, since they are needed in one file (per arch) and only used
 * in two files.
 * XXX most of these variables should be const.
 */
extern int osreldate;
extern int envmode;
extern int hintmode;		/* 0 = off. 1 = config, 2 = fallback */
extern int dynamic_kenv;
extern struct mtx kenv_lock;
extern char *kern_envp;
extern char static_env[];
extern char static_hints[];	/* by config for now */

extern char **kenvp;

extern const void *zero_region;	/* address space maps to a zeroed page	*/

/*
 * General function declarations.
 */

struct inpcb;
struct lock_object;
struct malloc_type;
struct mtx;
struct proc;
struct socket;
struct thread;
struct tty;
struct ucred;
struct uio;
struct _jmp_buf;
struct trapframe;

int	setjmp(struct _jmp_buf *);
void	longjmp(struct _jmp_buf *, int) __dead2;
int	dumpstatus(vm_offset_t addr, off_t count);
int	nullop(void);
int	eopnotsupp(void);
int	ureadc(int, struct uio *);
void	hashdestroy(void *, struct malloc_type *, u_long);
void	*hashinit(int count, struct malloc_type *type, u_long *hashmask);
void	*hashinit_flags(int count, struct malloc_type *type,
    u_long *hashmask, int flags);
#define	HASH_NOWAIT	0x00000001
#define	HASH_WAITOK	0x00000002

void	*phashinit(int count, struct malloc_type *type, u_long *nentries);
void	g_waitidle(void);

void	panic(const char *, ...) __dead2 __printflike(1, 2);

void	cpu_boot(int);
void	cpu_flush_dcache(void *, size_t);
void	cpu_rootconf(void);
void	critical_enter(void);
void	critical_exit(void);
void	init_param1(void);
void	init_param2(long physpages);
void	init_static_kenv(char *, size_t);
void	tablefull(const char *);
int	kvprintf(char const *, void (*)(int, void*), void *, int,
	    __va_list) __printflike(1, 0);
void	log(int, const char *, ...) __printflike(2, 3);
void	log_console(struct uio *);
int	printf(const char *, ...) __printflike(1, 2);
int	snprintf(char *, size_t, const char *, ...) __printflike(3, 4);
int	sprintf(char *buf, const char *, ...) __printflike(2, 3);
int	uprintf(const char *, ...) __printflike(1, 2);
int	vprintf(const char *, __va_list) __printflike(1, 0);
int	vsnprintf(char *, size_t, const char *, __va_list) __printflike(3, 0);
int	vsnrprintf(char *, size_t, int, const char *, __va_list) __printflike(4, 0);
int	vsprintf(char *buf, const char *, __va_list) __printflike(2, 0);
int	ttyprintf(struct tty *, const char *, ...) __printflike(2, 3);
int	sscanf(const char *, char const *, ...) __nonnull(1) __nonnull(2);
int	vsscanf(const char *, char const *, __va_list) __nonnull(1) __nonnull(2);
long	strtol(const char *, char **, int) __nonnull(1);
u_long	strtoul(const char *, char **, int) __nonnull(1);
quad_t	strtoq(const char *, char **, int) __nonnull(1);
u_quad_t strtouq(const char *, char **, int) __nonnull(1);
void	tprintf(struct proc *p, int pri, const char *, ...) __printflike(3, 4);
void	hexdump(const void *ptr, int length, const char *hdr, int flags);
#define	HD_COLUMN_MASK	0xff
#define	HD_DELIM_MASK	0xff00
#define	HD_OMIT_COUNT	(1 << 16)
#define	HD_OMIT_HEX	(1 << 17)
#define	HD_OMIT_CHARS	(1 << 18)

#define ovbcopy(f, t, l) bcopy((f), (t), (l))
void	bcopy(const void *from, void *to, size_t len) __nonnull(1) __nonnull(2);
void	bzero(void *buf, size_t len) __nonnull(1);

void	*memcpy(void *to, const void *from, size_t len) __nonnull(1) __nonnull(2);
void	*memmove(void *dest, const void *src, size_t n) __nonnull(1) __nonnull(2);

int	copystr(const void * __restrict kfaddr, void * __restrict kdaddr,
	    size_t len, size_t * __restrict lencopied)
	    __nonnull(1) __nonnull(2);
int	copyinstr(const void * __restrict udaddr, void * __restrict kaddr,
	    size_t len, size_t * __restrict lencopied)
	    __nonnull(1) __nonnull(2);
int	copyin(const void * __restrict udaddr, void * __restrict kaddr,
	    size_t len) __nonnull(1) __nonnull(2);
int	copyin_nofault(const void * __restrict udaddr, void * __restrict kaddr,
	    size_t len) __nonnull(1) __nonnull(2);
int	copyout(const void * __restrict kaddr, void * __restrict udaddr,
	    size_t len) __nonnull(1) __nonnull(2);
int	copyout_nofault(const void * __restrict kaddr, void * __restrict udaddr,
	    size_t len) __nonnull(1) __nonnull(2);

int	fubyte(const void *base);
long	fuword(const void *base);
int	fuword16(void *base);
int32_t	fuword32(const void *base);
int64_t	fuword64(const void *base);
int	subyte(void *base, int byte);
int	suword(void *base, long word);
int	suword16(void *base, int word);
int	suword32(void *base, int32_t word);
int	suword64(void *base, int64_t word);
uint32_t casuword32(volatile uint32_t *base, uint32_t oldval, uint32_t newval);
u_long	 casuword(volatile u_long *p, u_long oldval, u_long newval);

void	realitexpire(void *);

int	sysbeep(int hertz, int period);

void	hardclock(int usermode, uintfptr_t pc);
void	hardclock_anycpu(int cnt, int usermode);
void	hardclock_cpu(int usermode);
void	hardclock_sync(int cpu);
void	softclock(void *);
void	statclock(int usermode);
void	profclock(int usermode, uintfptr_t pc);

int	hardclockintr(void);

void	startprofclock(struct proc *);
void	stopprofclock(struct proc *);
void	cpu_startprofclock(void);
void	cpu_stopprofclock(void);
void	cpu_idleclock(void);
void	cpu_activeclock(void);
extern int	cpu_can_deep_sleep;
extern int	cpu_disable_deep_sleep;

int	cr_cansee(struct ucred *u1, struct ucred *u2);
int	cr_canseesocket(struct ucred *cred, struct socket *so);
int	cr_canseeinpcb(struct ucred *cred, struct inpcb *inp);

char	*getenv(const char *name);
void	freeenv(char *env);
int	getenv_int(const char *name, int *data);
int	getenv_uint(const char *name, unsigned int *data);
int	getenv_long(const char *name, long *data);
int	getenv_ulong(const char *name, unsigned long *data);
int	getenv_string(const char *name, char *data, int size);
int	getenv_quad(const char *name, quad_t *data);
int	setenv(const char *name, const char *value);
int	unsetenv(const char *name);
int	testenv(const char *name);

typedef uint64_t (cpu_tick_f)(void);
void set_cputicker(cpu_tick_f *func, uint64_t freq, unsigned var);
extern cpu_tick_f *cpu_ticks;
uint64_t cpu_tickrate(void);
uint64_t cputick2usec(uint64_t tick);

#ifdef APM_FIXUP_CALLTODO
struct timeval;
void	adjust_timeout_calltodo(struct timeval *time_change);
#endif /* APM_FIXUP_CALLTODO */

#include <sys/libkern.h>

/* Initialize the world */
void	consinit(void);
void	cpu_initclocks(void);
void	cpu_initclocks_bsp(void);
void	cpu_initclocks_ap(void);
void	usrinfoinit(void);

/* Finalize the world */
void	kern_reboot(int) __dead2;
void	shutdown_nice(int);

/* Timeouts */
typedef void timeout_t(void *);	/* timeout function type */
#define CALLOUT_HANDLE_INITIALIZER(handle)	\
	{ NULL }

void	callout_handle_init(struct callout_handle *);
struct	callout_handle timeout(timeout_t *, void *, int);
void	untimeout(timeout_t *, void *, struct callout_handle);
caddr_t	kern_timeout_callwheel_alloc(caddr_t v);
void	kern_timeout_callwheel_init(void);

/* Stubs for obsolete functions that used to be for interrupt management */
static __inline void		spl0(void)		{ return; }
static __inline intrmask_t	splbio(void)		{ return 0; }
static __inline intrmask_t	splcam(void)		{ return 0; }
static __inline intrmask_t	splclock(void)		{ return 0; }
static __inline intrmask_t	splhigh(void)		{ return 0; }
static __inline intrmask_t	splimp(void)		{ return 0; }
static __inline intrmask_t	splnet(void)		{ return 0; }
static __inline intrmask_t	splsoftcam(void)	{ return 0; }
static __inline intrmask_t	splsoftclock(void)	{ return 0; }
static __inline intrmask_t	splsofttty(void)	{ return 0; }
static __inline intrmask_t	splsoftvm(void)		{ return 0; }
static __inline intrmask_t	splsofttq(void)		{ return 0; }
static __inline intrmask_t	splstatclock(void)	{ return 0; }
static __inline intrmask_t	spltty(void)		{ return 0; }
static __inline intrmask_t	splvm(void)		{ return 0; }
static __inline void		splx(intrmask_t ipl __unused)	{ return; }

/*
 * Common `proc' functions are declared here so that proc.h can be included
 * less often.
 */
int	_sleep(void *chan, struct lock_object *lock, int pri, const char *wmesg,
	    int timo) __nonnull(1);
#define	msleep(chan, mtx, pri, wmesg, timo)				\
	_sleep((chan), &(mtx)->lock_object, (pri), (wmesg), (timo))
int	msleep_spin(void *chan, struct mtx *mtx, const char *wmesg, int timo)
	    __nonnull(1);
int	pause(const char *wmesg, int timo);
#define	tsleep(chan, pri, wmesg, timo)					\
	_sleep((chan), NULL, (pri), (wmesg), (timo))
void	wakeup(void *chan) __nonnull(1);
void	wakeup_one(void *chan) __nonnull(1);

/*
 * Common `struct cdev *' stuff are declared here to avoid #include poisoning
 */

struct cdev;
dev_t dev2udev(struct cdev *x);
const char *devtoname(struct cdev *cdev);

int poll_no_poll(int events);

/* XXX: Should be void nanodelay(u_int nsec); */
void	DELAY(int usec);

/* Root mount holdback API */
struct root_hold_token;

struct root_hold_token *root_mount_hold(const char *identifier);
void root_mount_rel(struct root_hold_token *h);
void root_mount_wait(void);
int root_mounted(void);


/*
 * Unit number allocation API. (kern/subr_unit.c)
 */
struct unrhdr;
struct unrhdr *new_unrhdr(int low, int high, struct mtx *mutex);
void delete_unrhdr(struct unrhdr *uh);
void clean_unrhdr(struct unrhdr *uh);
void clean_unrhdrl(struct unrhdr *uh);
int alloc_unr(struct unrhdr *uh);
int alloc_unr_specific(struct unrhdr *uh, u_int item);
int alloc_unrl(struct unrhdr *uh);
void free_unr(struct unrhdr *uh, u_int item);

/*
 * Population count algorithm using SWAR approach
 * - "SIMD Within A Register".
 */
static __inline uint32_t
bitcount32(uint32_t x)
{

	x = (x & 0x55555555) + ((x & 0xaaaaaaaa) >> 1);
	x = (x & 0x33333333) + ((x & 0xcccccccc) >> 2);
	x = (x + (x >> 4)) & 0x0f0f0f0f;
	x = (x + (x >> 8));
	x = (x + (x >> 16)) & 0x000000ff;
	return (x);
}

static __inline uint16_t
bitcount16(uint32_t x)
{

	x = (x & 0x5555) + ((x & 0xaaaa) >> 1);
	x = (x & 0x3333) + ((x & 0xcccc) >> 2);
	x = (x + (x >> 4)) & 0x0f0f;
	x = (x + (x >> 8)) & 0x00ff;
	return (x);
}

#endif /* !_SYS_SYSTM_H_ */
