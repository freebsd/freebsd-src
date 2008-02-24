/*******************************************************************************
 *
 * Filename: main.c
 *
 * Basic entry points for top-level functions
 *
 * Revision information:
 *
 * 20AUG2004	kb_admin	initial creation
 * 12JAN2005	kb_admin	cosmetic changes
 * 29APR2005	kb_admin	modified boot delay
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
 * $FreeBSD: src/sys/boot/arm/at91/bootiic/main.c,v 1.3 2006/08/10 19:55:52 imp Exp $
 ******************************************************************************/

#include "env_vars.h"
#include "at91rm9200_lowlevel.h"
#include "loader_prompt.h"
#include "emac.h"
#include "lib.h"

/*
 * .KB_C_FN_DEFINITION_START
 * int main(void)
 *  This global function waits at least one second, but not more than two 
 * seconds, for input from the serial port.  If no response is recognized,
 * it acts according to the parameters specified by the environment.  For 
 * example, the function might boot an operating system.  Do not return
 * from this function.
 * .KB_C_FN_DEFINITION_END
 */
int
main(void)
{
	InitEEPROM();
	EMAC_Init();
	LoadBootCommands();
	printf("\n\rKB9202(www.kwikbyte.com)\n\rAuto boot..\n\r");
	if (getc(1) == -1)
		ExecuteEnvironmentFunctions();
	Bootloader(0);

	return (1);
}
