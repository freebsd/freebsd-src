/******************************************************************************
 *
 * Filename: env_vars.h
 *
 * Definition of environment variables, structures, and other globals.
 *
 * Revision information:
 *
 * 20AUG2004	kb_admin	initial creation
 *
 * BEGIN_KBDD_BLOCK
 * No warranty, expressed or implied, is included with this software.  It is
 * provided "AS IS" and no warranty of any kind including statutory or aspects
 * relating to merchantability or fitness for any purpose is provided.  All
 * intellectual property rights of others is maintained with the respective
 * owners.  This software is not copyrighted and is intended for reference
 * only.
 * END_BLOCK
 *
 * $FreeBSD: src/sys/boot/arm/at91/bootspi/env_vars.h,v 1.1 2006/08/16 23:39:58 imp Exp $
 *****************************************************************************/

#ifndef _ENV_VARS_H_
#define _ENV_VARS_H_

/* each environment variable is a string following the standard command	*/
/* definition used by the interactive loader in the following format:	*/
/*  <command> <parm1> <parm2> ...					*/
/* all environment variables (or commands) are stored in a string	*/
/* format: NULL-terminated.						*/
/* this implies that commands can never utilize 0-values: actual 0, not	*/
/* the string '0'.  this is not an issue as the string '0' is handled	*/
/* by the command parse routine.					*/

/* the following defines the maximum size of the environment for 	*/
/* including variables.							*/
/* this value must match that declared in the low-level file that	*/
/* actually reserves the space for the non-volatile environment.	*/
#define	MAX_ENV_SIZE_BYTES	0x100

#define MAX_BOOT_COMMANDS	10

/* C-style reference section						*/
#ifndef __ASSEMBLY__

extern void	WriteCommandTable(void);
extern void	SetBootCommand(int index, char *command);
extern void	DumpBootCommands(void);
extern void	LoadBootCommands(void);
extern void	ExecuteEnvironmentFunctions(void);

#endif /* !__ASSEMBLY__ */

#endif /* _ENV_VARS_H_ */
