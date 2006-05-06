/*-
 * Copyright (c) 1997, 2002 Hellmuth Michaelis. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*---------------------------------------------------------------------------
 *
 *	i4b_global.h - i4b global include file
 *	--------------------------------------
 *
 * $FreeBSD: src/sys/i4b/include/i4b_global.h,v 1.13 2005/01/06 22:18:18 imp Exp $
 *
 *	last edit-date: [Sun Mar 17 09:55:26 2002]
 *
 *---------------------------------------------------------------------------*/

#ifndef _I4B_GLOBAL_H_
#define _I4B_GLOBAL_H_

/*---------------------------------------------------------------------------*
 *	hiding OS differences in the kernel
 *---------------------------------------------------------------------------*/ 

#if defined(__FreeBSD__) && __FreeBSD__ >= 5
/*
 * Deprecated LKM interface.
 */
#include <sys/module.h>
#define	PSEUDO_SET(sym, name) \
	static int name ## _modevent(module_t mod, int type, void *data) \
	{ \
		void (*initfunc)(void *) = (void (*)(void *))data; \
		switch (type) { \
		case MOD_LOAD: \
			/* printf(#name " module load\n"); */ \
			initfunc(NULL); \
			break; \
		case MOD_UNLOAD: \
			printf(#name " module unload - not possible for this module type\n"); \
			return EINVAL; \
		default: \
			return EOPNOTSUPP; \
		} \
		return 0; \
	} \
	static moduledata_t name ## _mod = { \
		#name, \
		name ## _modevent, \
		(void *)sym \
	}; \
	DECLARE_MODULE(name, name ## _mod, SI_SUB_PSEUDO, SI_ORDER_ANY)
#endif

/*---------------*/
/* time handling */
/*---------------*/

#include <sys/param.h>

#define TIMEOUT_FUNC_T	timeout_t *
#define SECOND		time_second
#define MICROTIME(x)	getmicrotime(&(x))

/*----------------*/
/* timer handling */
/*----------------*/

#define START_TIMER(XHANDLE, XF, XSC, XTIME) XHANDLE = timeout((TIMEOUT_FUNC_T)XF, (void*)XSC, XTIME)
#define	STOP_TIMER(XHANDLE, XF, XSC)	untimeout((TIMEOUT_FUNC_T)XF, (void*)XSC, XHANDLE)

/*---------------------------------------------------------------------------*
 *	misc globally used things in the kernel
 *---------------------------------------------------------------------------*/ 

/* timer states */

#define TIMER_IDLE	1		/* a timer is running	*/
#define TIMER_ACTIVE	2		/* a timer is idle	*/

/* priority */
#define I4BPRI		(PSOCK + 1)

/* i4b's spl */

#define	SPLI4B()	splimp()	/* spl for i4b		*/

/* critial code region handling macros */

#define CRIT_VAR	int _svd_spl_	/* declare variable	*/
#define CRIT_BEG	_svd_spl_ = SPLI4B()	/* save spl	*/
#define CRIT_END	splx(_svd_spl_)	/* restore spl		*/

/* definitions for the STATUS indications L1 -> L2 -> L3 */

#define	STI_ATTACH	0	/* attach at boot time			*/
#define	STI_L1STAT	1	/* layer 1 status			*/
#define	STI_L2STAT	2	/* layer 2 status			*/
#define	STI_TEIASG	3	/* TEI assignments			*/
#define	STI_PDEACT	4	/* Layer 1 T4 expired = persistent deactivation */
#define STI_NOL1ACC	5	/* no outgoing L1 access possible	*/

/* definitions for the COMMAND requests L3 -> L2 -> L1 */

#define CMR_DOPEN	0	/* daemon opened /dev/i4b		*/
#define CMR_DCLOSE	1	/* daemon closed /dev/i4b		*/
#define CMR_SETTRACE	2	/* set D-channel and B-channel trace	*/
#define CMR_GCST	3	/* get chipset statistics		*/
#define CMR_CCST	4	/* clear chipset statistics		*/

#endif /* _I4B_GLOBAL_H_ */
