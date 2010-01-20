/*
Copyright (c) 2003 Hewlett-Packard Development Company, L.P.
Permission is hereby granted, free of charge, to any person
obtaining a copy of this software and associated documentation
files (the "Software"), to deal in the Software without
restriction, including without limitation the rights to use,
copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following
conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.
*/

#ifndef __UWX_INCLUDED
#define __UWX_INCLUDED 1

#ifndef _KERNEL
#include <stdlib.h>
#include <inttypes.h>
#else
#include <sys/param.h>
#include <sys/systm.h>
#endif

#if defined(__cplusplus)
#define __EXTERN_C extern "C"
#else
#define __EXTERN_C extern
#endif

#define UWX_VERSION 1		/* Version id for callback interfaces */

/* Unwind environment structure (opaque) */
struct uwx_env;

/* Allocate and free callbacks */
typedef void *(*alloc_cb)(size_t size);
typedef void (*free_cb)(void *ptr);
__EXTERN_C int uwx_register_alloc_cb(alloc_cb alloc, free_cb free);

/* Allocate and initialize an unwind environment */
__EXTERN_C struct uwx_env *uwx_init(void);

/* Free an unwind environment */
__EXTERN_C int uwx_free(struct uwx_env *env);

/* Put unwind express into cross-process mode */
__EXTERN_C int uwx_set_remote(struct uwx_env *env, int is_big_endian_target);

/* Copy-in callback */
typedef int (*copyin_cb)(
    int request,		/* request code (see below) */
    char *loc,			/* local (destination) address */
    uint64_t rem,		/* remote (source) address */
    int len,			/* number of bytes to copy */
    intptr_t tok);		/* callback token */

/* Lookup IP callback */
typedef int (*lookupip_cb)(
    int request,		/* request code (see below) */
    uint64_t ip,		/* IP of current frame */
    intptr_t tok,		/* callback token */
    uint64_t **vecp);		/* parameter vector (in/out) */

/* Register copy-in and lookup IP callbacks */
__EXTERN_C int uwx_register_callbacks(
    struct uwx_env *env,	/* unwind environment */
    intptr_t tok,		/* callback token */
    copyin_cb copyin,		/* copy-in callback */
    lookupip_cb lookupip);	/* lookup IP callback */

/* Initialize a context with the basic info needed to start an unwind */
__EXTERN_C int uwx_init_context(
    struct uwx_env *env,	/* unwind environment */
    uint64_t ip,		/* IP (instruction pointer) */
    uint64_t sp,		/* SP (stack pointer) */
    uint64_t bsp,		/* BSP (backing store pointer) */
    uint64_t cfm);		/* CFM (current frame marker) */

/* Set the value of a specific register in the current context (non fp) */
__EXTERN_C int uwx_set_reg(
    struct uwx_env *env,	/* unwind environment */
    int regid,			/* register id (see below) */
    uint64_t val);		/* register value */

/* Set the value of a floating-point register in the current context */
__EXTERN_C int uwx_set_fr(
    struct uwx_env *env,	/* unwind environment */
    int regid,			/* register id (see below) */
    uint64_t *val);		/* register value (ptr to 16 bytes) */
				/*   (memory spill format) */

/* Initialize the unwind history */
__EXTERN_C int uwx_init_history(struct uwx_env *env);

/* Step one frame */
__EXTERN_C int uwx_step(struct uwx_env *env);

/* Get symbol information, if available, for current frame */
__EXTERN_C int uwx_get_sym_info(
    struct uwx_env *env,	/* unwind environment */
    char **modp,		/* load module name (out)  */
    char **symp,		/* function name (out)  */
    uint64_t *offsetp);		/* offset from start of function (out)  */

/* Get the value of a register from the current context */
__EXTERN_C int uwx_get_reg(
    struct uwx_env *env,	/* unwind environment */
    int regid,			/* register id (see below) */
    uint64_t *valp);		/* register value (out) */

/* Get the NaT bit of a GR from the current context */
__EXTERN_C int uwx_get_nat(
    struct uwx_env *env,	/* unwind environment */
    int regid,			/* register id (see below) */
    int *natp);			/* NaT value (out: 0 or 1) */

/* Get the spill location for a register in the current context */
__EXTERN_C int uwx_get_spill_loc(
    struct uwx_env *env,	/* unwind environment */
    int regid,			/* register id (see below) */
    uint64_t *dispp);		/* disposition code (see below) (out) */

/* Get the ABI context code (if uwx_step returned UWX_ABI_FRAME) */
__EXTERN_C int uwx_get_abi_context_code(struct uwx_env *env);

/* Return status codes for uwx_ APIs */
#define UWX_OK			0
#define UWX_BOTTOM		1	/* Hit bottom of stack */
#define UWX_ABI_FRAME		2	/* Hit ABI-dependent frame */
#define UWX_ERR_NOENV		(-1)	/* No uwx_env allocated */
#define UWX_ERR_IPNOTFOUND	(-2)	/* Lookup IP c/b returned NOTFOUND */
#define UWX_ERR_LOOKUPERR	(-3)	/* Lookup IP c/b returned ERR */
#define UWX_ERR_BADKEY		(-4)	/* Bad result vector key */
#define UWX_ERR_COPYIN_UTBL	(-5)	/* Error reading unwind table */
#define UWX_ERR_COPYIN_UINFO	(-6)	/* Error reading unwind info */
#define UWX_ERR_COPYIN_MSTK	(-7)	/* Error reading memory stack */
#define UWX_ERR_COPYIN_RSTK	(-8)	/* Error reading register stack */
#define UWX_ERR_COPYIN_REG	(-9)	/* Error reading context register */
#define UWX_ERR_NOUENTRY	(-10)	/* No unwind table entry for ip */
#define UWX_ERR_NOUDESC		(-11)	/* No unwind descriptor covers ip */
#define UWX_ERR_BADUDESC	(-12)	/* Bad unwind descriptor */
#define UWX_ERR_NOMEM		(-13)	/* Out of memory */
#define UWX_ERR_PROLOG_UF	(-14)	/* Prologue underflow */
#define UWX_ERR_UNDEFLABEL	(-15)	/* Undefined label in copy_state */
#define UWX_ERR_BADREGID	(-16)	/* Bad register identifier */
#define UWX_ERR_CANTUNWIND	(-17)	/* Can't unwind */
#define UWX_ERR_NOCALLBACKS	(-18)	/* No callbacks registered */
#define UWX_ERR_NOCONTEXT	(-19)	/* Context not initialized */

/* Request codes for copyin callback */
#define UWX_COPYIN_UINFO	1	/* Reading unwind info */
#define UWX_COPYIN_MSTACK	2	/* Reading memory stack */
#define UWX_COPYIN_RSTACK	3	/* Reading RSE backing store */
#define UWX_COPYIN_REG		4	/* Reading initial register state */

/* Request codes for lookup IP callback */
#define UWX_LKUP_LOOKUP		1	/* Lookup IP */
#define UWX_LKUP_FREE		2	/* Free result vector */
#define UWX_LKUP_SYMBOLS	3	/* Lookup symbolic information */

/* Return status codes for lookup IP callback */
#define UWX_LKUP_NOTFOUND	0	/* IP not found */
#define UWX_LKUP_ERR		1	/* Other error */
#define UWX_LKUP_UTABLE		2	/* Returned ref to unwind table */
#define UWX_LKUP_FDESC		3	/* Returned frame description */
#define UWX_LKUP_SYMINFO	4	/* Returned symbolic information */
#define UWX_LKUP_REMAP		5	/* Returned remapped IP */
#define UWX_LKUP_UINFO		6	/* Returned unw info block ptr */

/* The lookup IP callback receives a parameter vector, and returns */
/* one on success. This vector is a series of key/value pairs; each */
/* even-numbered slot is a key, and each odd-numbered slot is a */
/* corresponding value. The vector is terminated by a pair whose */
/* key is 0. */
#define UWX_KEY_END		0	/* End of vector */

/* Keys passed to lookup IP callback */
#define UWX_KEY_PREDS		1	/* Predicate registers */
#define UWX_KEY_VERSION		2	/* Version id of unwind engine */
/* UWX_KEY_FUNCSTART (below) may also be passed with the UWX_LKUP_SYMINFO */
/* request. */

/* Keys returned with UWX_LKUP_UTABLE */
/* These key/value pairs describe the unwind table corresponding */
/* to the load module in which the current IP resides. */
#define UWX_KEY_TBASE		1	/* Base address of text seg */
#define UWX_KEY_UFLAGS		2	/* Unwind flags (optional) */
#define UWX_KEY_USTART		3	/* Base of unwind tbl */
#define UWX_KEY_UEND		4	/* End of unwind tbl */

/* Keys returned with UWX_LKUP_FDESC */
/* These key/value pairs describe the state of the frame at the */
/* given IP. They are typically used for dynamically-generated code. */
/* If UWX_KEY_CONTEXT is returned, it must be the only key returned. */
#define UWX_KEY_FSIZE		1			/* Frame size */
#define UWX_KEY_SPILL(reg_id)	(2 | ((reg_id) << 4))	/* Reg spilled */
#define UWX_KEY_CONTEXT		3 			/* ABI-dep. context */

/* Keys returned with UWX_LKUP_REMAP */
#define UWX_KEY_NEWIP		5	/* Remapped IP */

/* Keys returned with UWX_LKUP_UINFO */
/* Use UWX_KEY_FUNCSTART for the start address of the function */
/* Use UWX_KEY_UFLAGS for the unwind flags (optional) */
#define UWX_KEY_UINFO 		6	/* Address of unwind info block */

/* Keys returned with UWX_LKUP_SYMINFO */
/* These keys may be returned with UWX_LKUP_FDESC or UWX_LKUP_UINFO, */
/* if the information is cheap to obtain. */
#define UWX_KEY_MODULE		17	/* Name of load module */
#define UWX_KEY_FUNC		18	/* Name of function */
#define UWX_KEY_FUNCSTART	19	/* Address of start of function */

/* Register identifiers */
/* For use in UWX_LKUP_FDESC result vectors and context access APIs. */
/* "no spill info": These regs aren't spilled directly, so */
/*    result vectors must not describe these registers. */
/*    The result vector must describe the related register or */
/*    pseudo register instead (ip:rp, sp:psp, bsp/cfm:pfs). */
/* "pseudo register": Not a machine register, but treated as */
/*    one for unwind purposes. */
#define UWX_REG_IP		0	/* ip (no spill info) */
#define UWX_REG_SP		1	/* sp (no spill info) */
#define UWX_REG_BSP		2	/* ar.bsp (no spill info) */
#define UWX_REG_CFM		3	/* cfm (no spill info) */
#define UWX_REG_RP		4	/* rp (pseudo-register) */
#define UWX_REG_PSP		5	/* psp (pseudo-register) */
#define UWX_REG_PFS		6	/* pfs (pseudo-register) */
#define UWX_REG_PREDS		7	/* p0 - p63 */
#define UWX_REG_PRIUNAT		8	/* primary unat (pseudo-register) */
#define UWX_REG_AR_BSPSTORE	9	/* ar.bspstore */
#define UWX_REG_AR_RNAT		10	/* ar.rnat */
#define UWX_REG_AR_UNAT		11	/* ar.unat */
#define UWX_REG_AR_FPSR		12	/* ar.fpsr */
#define UWX_REG_AR_LC		13	/* ar.lc */
#define UWX_REG_AR_PFS		14	/* ar.pfs */
#define UWX_REG_GR(gr)		(0x100 | (gr))
#define UWX_REG_FR(fr)		(0x200 | (fr))
#define UWX_REG_BR(br)		(0x300 | (br))

/* for backwards compatibility with previous releases... */
#define UWX_REG_BSPSTORE	UWX_REG_AR_BSPSTORE
#define UWX_REG_RNAT		UWX_REG_AR_RNAT
#define UWX_REG_UNAT		UWX_REG_AR_UNAT
#define UWX_REG_FPSR		UWX_REG_AR_FPSR
#define UWX_REG_LC		UWX_REG_AR_LC

/* Values corresponding to UWX_KEY_SPILL keys indicate the disposition */
/* of the spilled register -- either in the memory stack or in another */
/* register. The PSP register may also have a disposition of "SPPLUS", */
/* indicating that its value is SP plus a fixed constant. */
#define UWX_DISP_NONE		0		/* Not spilled */
#define UWX_DISP_SPPLUS(k)	(1 | (k))	/* PSP = SP+constant */
#define UWX_DISP_SPREL(disp)	(2 | (disp))	/* Spilled at [SP+disp] */
#define UWX_DISP_PSPREL(disp)	(3 | (disp))	/* Spilled at [PSP+16-disp] */
#define UWX_DISP_REG(reg)	(4 | ((reg) << 4)) /* Saved to another reg. */

/* The uwx_get_spill_loc() routine returns a spill location for a */
/* given register in the current context. It will return a disposition */
/* code of UWX_DISP_NONE, UWX_DISP_REG(reg), or one of the following */
/* to indicate that the spilled value can be found in the memory */
/* stack or the register stack backing store. */
#define UWX_DISP_MSTK(addr)	(5 | (addr))	/* Spilled in mem. stack */
#define UWX_DISP_RSTK(addr)	(6 | (addr))	/* Spilled in reg. stack */

/* Extract the disposition code, offset, address, or register id */
/* from a disposition returned from uwx_get_spill_loc(). */
/* Compare the extracted disp code against UWX_DISP_REG(0), etc. */
#define UWX_GET_DISP_CODE(disp)		((int)(disp) & 0x07)
#define UWX_GET_DISP_OFFSET(disp)	((disp) & ~(uint64_t)0x07)
#define UWX_GET_DISP_ADDR(disp)		((disp) & ~(uint64_t)0x07)
#define UWX_GET_DISP_REGID(disp)	((int)(disp) >> 4)

#undef __EXTERN_C

#if defined(__cplusplus)

class UnwindExpress {

public:

    UnwindExpress() {
	env = uwx_init();
    }

    ~UnwindExpress() {
	if (env != 0)
	    uwx_free(env);
	env = 0;
    }

    int init_context(uint64_t ip, uint64_t sp, uint64_t bsp, uint64_t cfm) { 
	return uwx_init_context(env, ip, sp, bsp, cfm);
    }

    int init_history() {
	return uwx_init_history(env);
    }

    int set_reg(int regid, uint64_t val) {
	return uwx_set_reg(env, regid, val);
    }

    int set_fr(int regid, uint64_t *valp) {
	return uwx_set_fr(env, regid, valp);
    }

    int step() {
	return uwx_step(env);
    }

    int get_sym_info(char **modp, char **symp, uint64_t *offsetp) {
	return uwx_get_sym_info(env, modp, symp, offsetp);
    }

    int get_reg(int regid, uint64_t *valp) {
	return uwx_get_reg(env, regid, valp);
    }

    int get_nat(int regid, int *natp) {
	return uwx_get_nat(env, regid, natp);
    }

    int get_spill_loc(int regid, uint64_t *dispp) {
	return uwx_get_spill_loc(env, regid, dispp);
    }

    int get_abi_context_code() {
	return uwx_get_abi_context_code(env);
    }

    struct uwx_env *get_env() {
	return env;
    }

protected:

    struct uwx_env *env;

};

#endif /* __cplusplus */

#endif /* __UWX_INCLUDED */
