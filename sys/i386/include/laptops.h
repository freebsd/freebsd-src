/*
 * Machine-depend parameters for laptop machines
 *
 * Copyright (c) 1996, HOSOKAWA, Tatsumi <hosokawa@mt.cs.keio.ac.jp>
 */

/*
 * Laptop machines has more incompatibilities and machine-specific 
 * parameters than the desktop machines.
 */

#ifndef	_LAPTOPS_H_
#define	_LAPTOPS_H_

#ifdef	LAPTOP

#ifdef	HINOTE		/* Digital Hinote */
#ifndef	COMPAT_APM10
#define	COMPAT_APM10
#endif	/* COMPAT_APM10 */
#ifndef	SIO_IRQ_BUG
#define	SIO_IRQ_BUG
#endif	/* SIO_IRQ_BUG */
#endif	/* HINOTE */

#ifdef	DHULTRA		/* Digital Hinote Ultra */
#ifndef	FORCE_APM10
#define	FORCE_APM10
#endif	/* FORCE_APM10 */
#ifndef	SIO_IRQ_BUG
#define	SIO_IRQ_BUG
#endif	/* SIO_IRQ_BUG */
#endif	/* DHULTRA */

#ifdef	TP230		/* IBM ThinkPad 230 Series */
#ifndef	SIO_IRQ_BUG2
#define	SIO_IRQ_BUG2
#endif	/* SIO_IRQ_BUG2 */
#ifndef	FORCE_APM10
#define	FORCE_APM10
#endif	/* FORCE_APM10 */
#ifndef	PCIC_NOCLRREGS
#define	PCIC_NOCLRREGS
#endif	/* PCIC_NOCLRREGS */
#endif	/* TP230 */

#ifdef	TP230FBW	/* IBM ThinkPad 230 FBW Series */
#ifndef	SIO_IRQ_BUG2
#define	SIO_IRQ_BUG2
#endif	/* SIO_IRQ_BUG2 */
#ifndef	PCIC_NOCLRREGS
#define	PCIC_NOCLRREGS
#endif	/* PCIC_NOCLRREGS */
#endif	/* TP230FBW */

#ifdef	TP530		/* IBM ThinkPad 530 Series */
#ifndef	APM_DSVALUE_BUG
#define	APM_DSVALUE_BUG
#endif	/* APM_DSVALUE_BUG */
#endif	/* TP530 */

#ifdef	WINBOOKPRO	/* Sotec WinbookPro */
#ifndef	FORCE_APM10
#define	FORCE_APM10
#endif	/* FORCE_APM10 */
#ifndef	APM_NO_ENGAGE
#define	APM_NO_ENGAGE
#endif	/* APM_NO_ENGAGE */
#ifndef	APM_SUSPEND_POSTPONE
#define	APM_SUSPEND_POSTPONE
#endif	/* APM_SUSPEND_POSTPONE */
#ifndef	APM_DISABLE_BUG
#define	APM_DISABLE_BUG
#endif	/* APM_DISABLE_BUG */
#endif	/* WINBOOKPRO */

#ifdef	GW2KLIBERTY	/* Gateway 2K Liberty */
#ifndef FORCE_APM10
#define FORCE_APM10
#endif	/* FORCE_APM10 */
#endif	/* GW2KLIBERTY */

#ifdef	JETMINI		/* Panasonic Pronote Jet Mini */
#ifndef FORCE_APM10
#define FORCE_APM10
#endif	/* FORCE_APM10 */
#endif	/* JETMINI */

#ifdef	CONTURA		/* COMPAQ CONTURA Series */
#ifndef	SIO_IRQ_BUG
#define	SIO_IRQ_BUG
#endif	/* SIO_IRQ_BUG */
#ifndef	PCIC_NOCLRREGS
#define	PCIC_NOCLRREGS
#endif	/* PCIC_NOCLRREGS */
#ifndef	APM_SUSPEND_DELAY
#define	APM_SUSPEND_DELAY
#endif	/* APM_SUSPEND_DELAY */
#endif	/* CONTURA */


#endif	/* LAPTOP */

#endif	/* _LAPTOPS_H_ */
