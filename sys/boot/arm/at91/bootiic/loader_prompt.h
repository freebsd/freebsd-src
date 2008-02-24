/******************************************************************************
 *
 * Filename: loader_prompt.h
 *
 * Definition of the interactive loader functions.
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
 * $FreeBSD: src/sys/boot/arm/at91/bootiic/loader_prompt.h,v 1.1 2006/08/10 19:55:52 imp Exp $
 *****************************************************************************/

#ifndef _LOADER_PROMPT_H_
#define _LOADER_PROMPT_H_

#define MAX_INPUT_SIZE		256
#define MAX_COMMAND_PARAMS	10

enum {
	COMMAND_INVALID	= 0,
	COMMAND_COPY,
	COMMAND_DUMP,
	COMMAND_EXEC,
	COMMAND_HELP,
	COMMAND_LOCAL_IP,
	COMMAND_MAC,
	COMMAND_SERVER_IP,
	COMMAND_SET,
	COMMAND_TAG,
	COMMAND_TFTP,
	COMMAND_WRITE,
	COMMAND_XMODEM,
	COMMAND_FINAL_FLAG
} e_cmd_t;


typedef struct {
	int		command;
	const char	*c_string;
} command_entry_t;

void EnterInteractiveBootloader(int(*inputFunction)(int));
void Bootloader(int(*inputFunction)(int));

#endif /* _LOADER_PROMPT_H_ */
