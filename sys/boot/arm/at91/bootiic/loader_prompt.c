/******************************************************************************
 *
 * Filename: loader_prompt.c
 *
 * Instantiation of the interactive loader functions.
 *
 * Revision information:
 *
 * 20AUG2004	kb_admin	initial creation
 * 12JAN2005	kb_admin	massive changes for tftp, strings, and more
 * 05JUL2005	kb_admin	save tag address, and set registers on boot
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
 * $FreeBSD: src/sys/boot/arm/at91/bootiic/loader_prompt.c,v 1.3 2006/10/21 22:43:39 imp Exp $
 *****************************************************************************/

#include "at91rm9200_lowlevel.h"
#ifdef SUPPORT_TAG_LIST
#include "tag_list.h"
#endif
#include "emac.h"
#include "loader_prompt.h"
#include "env_vars.h"
#include "lib.h"


/******************************* GLOBALS *************************************/


/*********************** PRIVATE FUNCTIONS/DATA ******************************/

static char	inputBuffer[MAX_INPUT_SIZE];
static int	buffCount;

// argv pointer are either NULL or point to locations in inputBuffer
static char	*argv[MAX_COMMAND_PARAMS];

static const char *backspaceString = "\010 \010";

static const command_entry_t	CommandTable[] = {
	{COMMAND_COPY, "c"},
	{COMMAND_DUMP, "d"},
	{COMMAND_EXEC, "e"},
	{COMMAND_HELP, "?"},
	{COMMAND_LOCAL_IP, "ip"},
	{COMMAND_MAC, "m"},
	{COMMAND_SERVER_IP, "server_ip"},
	{COMMAND_SET, "s"},
#ifdef SUPPORT_TAG_LIST
	{COMMAND_TAG, "t"},
#endif
	{COMMAND_TFTP, "tftp"},
	{COMMAND_WRITE, "w"},
	{COMMAND_XMODEM, "x"},
	{COMMAND_FINAL_FLAG, 0}
};

static unsigned tagAddress;

/*
 * .KB_C_FN_DEFINITION_START
 * unsigned BuildIP(void)
 *  This private function packs the test IP info to an unsigned value.
 * .KB_C_FN_DEFINITION_END
 */
static unsigned
BuildIP(void)
{
	return ((p_ASCIIToDec(argv[1]) << 24) |
	    (p_ASCIIToDec(argv[2]) << 16) |
	    (p_ASCIIToDec(argv[3]) << 8) |
	    p_ASCIIToDec(argv[4]));
}


/*
 * .KB_C_FN_DEFINITION_START
 * int StringToCommand(char *cPtr)
 *  This private function converts a command string to a command code.
 * .KB_C_FN_DEFINITION_END
 */
static int
StringToCommand(char *cPtr)
{
	int	i;

	for (i = 0; CommandTable[i].command != COMMAND_FINAL_FLAG; ++i)
		if (!strcmp(CommandTable[i].c_string, cPtr))
			return (CommandTable[i].command);

	return (COMMAND_INVALID);
}


/*
 * .KB_C_FN_DEFINITION_START
 * void RestoreSpace(int)
 *  This private function restores NULL characters to spaces in order to
 * process the remaining args as a string.  The number passed is the argc
 * of the first entry to begin restoring space in the inputBuffer.
 * .KB_C_FN_DEFINITION_END
 */
static void
RestoreSpace(int startArgc)
{
	char	*cPtr;

	for (startArgc++; startArgc < MAX_COMMAND_PARAMS; startArgc++) {
		if ((cPtr = argv[startArgc]))
			*(cPtr - 1) = ' ';
	}
}


/*
 * .KB_C_FN_DEFINITION_START
 * int BreakCommand(char *)
 *  This private function splits the buffer into separate strings as pointed
 * by argv and returns the number of parameters (< 0 on failure).
 * .KB_C_FN_DEFINITION_END
 */
static int
BreakCommand(char *buffer)
{
	int	pCount, cCount, state;

	state = pCount = 0;
	p_memset((char*)argv, 0, sizeof(argv));

	for (cCount = 0; cCount < MAX_INPUT_SIZE; ++cCount) {

		if (!state) {
			/* look for next command */
			if (!p_IsWhiteSpace(buffer[cCount])) {
				argv[pCount++] = &buffer[cCount];
				state = 1;
			} else {
				buffer[cCount] = 0;
			}
		} else {
			/* in command, find next white space */
			if (p_IsWhiteSpace(buffer[cCount])) {
				buffer[cCount] = 0;
				state = 0;
			}
		}

		if (pCount >= MAX_COMMAND_PARAMS) {
			return (-1);
		}
	}

	return (pCount);
}


/*
 * .KB_C_FN_DEFINITION_START
 * void ParseCommand(char *)
 *  This private function executes matching functions.
 * .KB_C_FN_DEFINITION_END
 */
static void
ParseCommand(char *buffer)
{
	int		argc, i;

	if ((argc = BreakCommand(buffer)) < 1)
		return;

	switch (StringToCommand(argv[0])) {
	case COMMAND_COPY:
	{
		// "c <to> <from> <size in bytes>"
		// copy memory
		char		*to, *from;
		unsigned	size;

		if (argc > 3) {
			to = (char *)p_ASCIIToHex(argv[1]);
			from = (char *)p_ASCIIToHex(argv[2]);
			size = p_ASCIIToHex(argv[3]);
			memcpy(to, from, size);
		}
		break;
	}

	case COMMAND_DUMP:
		// display boot commands
		DumpBootCommands();
		break;

	case COMMAND_EXEC:
	{
		// "e <address>"
		// execute at address
		void (*execAddr)(unsigned, unsigned, unsigned);

		if (argc > 1) {
			/* in future, include machtypes (MACH_KB9200 = 612) */
			execAddr = (void (*)(unsigned, unsigned, unsigned))
			  p_ASCIIToHex(argv[1]);
			(*execAddr)(0, 612, tagAddress);
		}
		break;
	}

	case COMMAND_TFTP:
	{
		// "tftp <local_dest_addr filename>"
		//  tftp download
		unsigned address = 0;

		if (argc > 2)
			address = p_ASCIIToHex(argv[1]);
		TFTP_Download(address, argv[2]);
		break;
	}

	case COMMAND_SERVER_IP:
		// "server_ip <server IP 192 200 1 20>"
		// set download server address
		if (argc > 4)
			SetServerIPAddress(BuildIP());
		break;

	case COMMAND_HELP:
		// dump command info
		printf("Commands:\n"
		"\tc\n"
		"\td\n"
		"\te\n"
		"\tip\n"
		"\tserver_ip\n"
		"\tm\n"
		"\ttftp\n"
		"\ts\n"
#ifdef SUPPORT_TAG_LIST
		"\tt\n"
#endif
		"\tw\n"
		"\tx\n");
		break;

	case COMMAND_LOCAL_IP:
		// "local_ip <local IP 192 200 1 21>
		// set ip of this module
		if (argc > 4)
			SetLocalIPAddress(BuildIP());
		break;

	case COMMAND_MAC:
	{
		// "m <mac address 12 34 56 78 9a bc>
		// set mac address using 6 byte values
		unsigned char mac[6];

		if (argc > 6) {
			for (i = 0; i < 6; i++)
				mac[i] = p_ASCIIToHex(argv[i + 1]);
			EMAC_SetMACAddress(mac);
		}
		break;
	}

	case COMMAND_SET:
	{
		// s <index> <new boot command>
		// set the boot command at index (0-based)
		unsigned	index;

		if (argc > 1) {
			RestoreSpace(2);
			index = p_ASCIIToHex(argv[1]);
			SetBootCommand(index, argv[2]);
		}
		break;
	}

#ifdef SUPPORT_TAG_LIST
	case COMMAND_TAG:
		// t <address> <boot command line>
		// create tag-list for linux boot
		if (argc > 2) {
			RestoreSpace(2);
			tagAddress = p_ASCIIToHex(argv[1]);
			InitTagList(argv[2], (void*)tagAddress);
		}
		break;
#endif

	case COMMAND_WRITE:
		// write the command table to non-volatile
		WriteCommandTable();
		break;

	case COMMAND_XMODEM:
	{
		// "x <address>"
		// download X-modem record at address
		if (argc > 1)
			xmodem_rx((char *)p_ASCIIToHex(argv[1]));
		break;
	}

	default:
		break;
	}

	printf("\n");
}


/*
 * .KB_C_FN_DEFINITION_START
 * void ServicePrompt(char)
 *  This private function process each character checking for valid commands.
 * This function is only executed if the character is considered valid.
 * Each command is terminated with NULL (0) or ''.
 * .KB_C_FN_DEFINITION_END
 */
static void
ServicePrompt(char p_char)
{
	if (p_char == '\r')
		p_char = 0;

	if (p_char == '\010') {
		if (buffCount) {
			/* handle backspace BS */
			inputBuffer[--buffCount] = 0;
			printf(backspaceString);
		}
		return;
	}
	if (buffCount < MAX_INPUT_SIZE - 1) {
		inputBuffer[buffCount++] = p_char;
		putchar(p_char);
	}
	if (!p_char) {
		printf("\n");
		ParseCommand(inputBuffer);
		p_memset(inputBuffer, 0, MAX_INPUT_SIZE);
		buffCount = 0;
		printf("\n>");
	}
}


/* ************************** GLOBAL FUNCTIONS ********************************/


/*
 * .KB_C_FN_DEFINITION_START
 * void Bootloader(void *inputFunction)
 *  This global function is the entry point for the bootloader.  If the
 * inputFunction pointer is NULL, the loader input will be serviced from
 * the uart.  Otherwise, inputFunction is called to get characters which
 * the loader will parse.
 * .KB_C_FN_DEFINITION_END
 */
void
Bootloader(int(*inputFunction)(int))
{
	int	ch = 0;

	p_memset((void*)inputBuffer, 0, sizeof(inputBuffer));

	buffCount = 0;
	if (!inputFunction) {
		inputFunction = getc;
	}

	printf("\n>");

	while (1)
		if ((ch = ((*inputFunction)(0))) > 0)
			ServicePrompt(ch);
}
