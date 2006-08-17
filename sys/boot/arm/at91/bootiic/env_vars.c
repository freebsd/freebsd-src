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
 * void WriteCommandTable(void)
 *  This global function write the current command table to the non-volatile
 * memory.
 * .KB_C_FN_DEFINITION_END
 */
void
WriteCommandTable(void)
{
	int	i, size = MAX_ENV_SIZE_BYTES, copySize;
	char	*cPtr = env_table;

	p_memset(env_table, 0, sizeof(env_table));

	for (i = 0; i < MAX_BOOT_COMMANDS; ++i) {

		copySize = p_strlen(boot_commands[i]);
		size -= copySize + 1;

		if (size < 0) {
			continue;
		}
		p_memcpy(cPtr, boot_commands[i], copySize);
		cPtr += copySize;
		*cPtr++ = 0;
	}

	/* We're executing in low RAM so addr in ram == offset in eeprom */
	WriteEEPROM((unsigned)&BootCommandSection, env_table,
	    sizeof(env_table));
}


/*
 * .KB_C_FN_DEFINITION_START
 * void SetBootCommand(int index, char *command)
 *  This global function replaces the specified index with the string residing
 * at command.  Execute this function with a NULL string to clear the
 * associated command index.
 * .KB_C_FN_DEFINITION_END
 */
void
SetBootCommand(int index, char *command)
{
	int 	i;

	if ((unsigned)index < MAX_BOOT_COMMANDS) {

		p_memset(boot_commands[index], 0, MAX_INPUT_SIZE);

		if (!command)
			return ;

		for (i = 0; i < MAX_INPUT_SIZE; ++i) {
			boot_commands[index][i] = command[i];
			if (!(boot_commands[index][i]))
				return;
		}
	}
}


/*
 * .KB_C_FN_DEFINITION_START
 * void DumpBootCommands(void)
 *  This global function displays the current boot commands.
 * .KB_C_FN_DEFINITION_END
 */
void
DumpBootCommands(void)
{
	int	i, j;

	for (i = 0; i < MAX_BOOT_COMMANDS; ++i) {
		printf("0x%x : ", i);
		for (j = 0; j < MAX_INPUT_SIZE; ++j) {
			putchar(boot_commands[i][j]);
			if (!(boot_commands[i][j]))
				break;
		}
		printf("[E]\n\r");
	}
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
	int	index, j, size;
	char	*cPtr;

	p_memset((char*)boot_commands, 0, sizeof(boot_commands));

	cPtr = &BootCommandSection;

	size = MAX_ENV_SIZE_BYTES;

	for (index = 0; (index < MAX_BOOT_COMMANDS) && size; ++index) {
		for (j = 0; (j < MAX_INPUT_SIZE) && size; ++j) {
			size--;
			boot_commands[index][j] = *cPtr++;
			if (!(boot_commands[index][j])) {
				break;
			}
		}
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
	Bootloader(ReadCharFromEnvironment);
}
