/******************************************************************************
 *
 * Filename: env_vars.c
 *
 * Instantiation of environment variables, structures, and other globals.
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
 * $FreeBSD$
 *****************************************************************************/

#include "env_vars.h"
#include "loader_prompt.h"
#include "lib.h"

/******************************* GLOBALS *************************************/
char	boot_commands[MAX_BOOT_COMMANDS][MAX_INPUT_SIZE];

char	env_table[MAX_ENV_SIZE_BYTES];

extern char	BootCommandSection;

/************************** PRIVATE FUNCTIONS ********************************/


static int	currentIndex;
static int	currentOffset;


/*
 * .KB_C_FN_DEFINITION_START
 * int ReadCharFromEnvironment(char *)
 *  This private function reads characters from the enviroment variables
 * to service the command prompt during auto-boot or just to setup the
 * default environment.  Returns positive value if valid character was
 * set in the pointer.  Returns negative value to signal input stream
 * terminated.  Returns 0 to indicate _wait_ condition.
 * .KB_C_FN_DEFINITION_END
 */
static int
ReadCharFromEnvironment(int timeout)
{
	int ch;

	if (currentIndex < MAX_BOOT_COMMANDS) {
		ch = boot_commands[currentIndex][currentOffset++];
		if (ch == '\0' || (currentOffset >= MAX_INPUT_SIZE)) {
			currentOffset = 0;
			++currentIndex;
			ch = '\r';
		}
		return (ch);
	}

	return (-1);
}


/*************************** GLOBAL FUNCTIONS ********************************/


/*
 * .KB_C_FN_DEFINITION_START
 * void DumpBootCommands(void)
 *  This global function displays the current boot commands.
 * .KB_C_FN_DEFINITION_END
 */
void
DumpBootCommands(void)
{
	int	i;

	for (i = 0; boot_commands[i][0]; i++)
		printf("0x%x : %s[E]\r\n", i, boot_commands[i]);
}


/*
 * .KB_C_FN_DEFINITION_START
 * void LoadBootCommands(void)
 *  This global function loads the existing boot commands from raw format and
 * coverts it to the standard, command-index format.  Notice, the processed
 * boot command table has much more space allocated than the actual table
 * stored in non-volatile memory.  This is because the processed table
 * exists in RAM which is larger than the non-volatile space.
 * .KB_C_FN_DEFINITION_END
 */
void
LoadBootCommands(void)
{
	int	index, j;
	char	*cptr;

	p_memset((char*)boot_commands, 0, sizeof(boot_commands));
	cptr = &BootCommandSection;
	for (index = 0; *cptr; index++) {
		for (j = 0; *cptr; j++)
			boot_commands[index][j] = *cptr++;
		cptr++;
	}
}


/*
 * .KB_C_FN_DEFINITION_START
 * void ExecuteEnvironmentFunctions(void)
 *  This global function executes applicable entries in the environment.
 * .KB_C_FN_DEFINITION_END
 */
void
ExecuteEnvironmentFunctions(void)
{
	currentIndex = 0;
	currentOffset = 0;

	DumpBootCommands();
	printf("Autoboot...\r\n");
	Bootloader(ReadCharFromEnvironment);
}
