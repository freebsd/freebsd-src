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
 * $FreeBSD: src/sys/boot/arm/at91/bootspi/loader_prompt.c,v 1.4.6.1 2008/11/25 02:59:29 kensmith Exp $
 *****************************************************************************/

#include "at91rm9200_lowlevel.h"
#include "at91rm9200.h"
#include "emac.h"
#include "loader_prompt.h"
#include "env_vars.h"
#include "lib.h"
#include "spi_flash.h"
#include "ee.h"

/******************************* GLOBALS *************************************/


/*********************** PRIVATE FUNCTIONS/DATA ******************************/

static char	inputBuffer[MAX_INPUT_SIZE];
static int	buffCount;

// argv pointer are either NULL or point to locations in inputBuffer
static char	*argv[MAX_COMMAND_PARAMS];

#define FLASH_OFFSET (0 * FLASH_PAGE_SIZE)
#define KERNEL_OFFSET (220 * FLASH_PAGE_SIZE)
#define KERNEL_LEN (6 * 1024 * FLASH_PAGE_SIZE)
static const char *backspaceString = "\010 \010";

static const command_entry_t	CommandTable[] = {
	{COMMAND_DUMP, "d"},
	{COMMAND_EXEC, "e"},
	{COMMAND_LOCAL_IP, "ip"},
	{COMMAND_MAC, "m"},
	{COMMAND_SERVER_IP, "server_ip"},
	{COMMAND_TFTP, "tftp"},
	{COMMAND_XMODEM, "x"},
	{COMMAND_RESET, "R"},
	{COMMAND_LOAD_SPI_KERNEL, "k"},
	{COMMAND_REPLACE_KERNEL_VIA_XMODEM, "K"},
	{COMMAND_REPLACE_FLASH_VIA_XMODEM, "I"},
	{COMMAND_REPLACE_ID_EEPROM, "E"},
	{COMMAND_FINAL_FLAG, 0}
};

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

#if 0
static void
UpdateEEProm(int eeaddr)
{
	char *addr = (char *)SDRAM_BASE + (1 << 20); /* Load to base + 1MB */
	int len;

	while ((len = xmodem_rx(addr)) == -1)
		continue;
	printf("\nDownloaded %u bytes.\n", len);
	WriteEEPROM(eeaddr, 0, addr, len);
}
#endif

static void
UpdateFlash(int offset)
{
	char *addr = (char *)SDRAM_BASE + (1 << 20); /* Load to base + 1MB */
	int len, i, off;

	while ((len = xmodem_rx(addr)) == -1)
		continue;
	printf("\nDownloaded %u bytes.\n", len);
	for (i = 0; i < len; i+= FLASH_PAGE_SIZE) {
		off = i + offset;
		SPI_WriteFlash(off, addr + i, FLASH_PAGE_SIZE);
	}
}

static void
LoadKernelFromSpi(char *addr)
{
	int i, off;

	for (i = 0; i < KERNEL_LEN; i+= FLASH_PAGE_SIZE) {
		off = i + KERNEL_OFFSET;
		SPI_ReadFlash(off, addr + i, FLASH_PAGE_SIZE);
	}
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
	case COMMAND_DUMP:
		// display boot commands
		DumpBootCommands();
		break;

	case COMMAND_EXEC:
	{
		// "e <address>"
		// execute at address
		void (*execAddr)(unsigned, unsigned);

		if (argc > 1) {
			/* in future, include machtypes (MACH_KB9200 = 612) */
			execAddr = (void (*)(unsigned, unsigned))
			    p_ASCIIToHex(argv[1]);
			(*execAddr)(0, 612);
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

	case COMMAND_LOAD_SPI_KERNEL:
		// "k <address>"
		if (argc > 1)
			LoadKernelFromSpi((char *)p_ASCIIToHex(argv[1]));
		break;

	case COMMAND_XMODEM:
		// "x <address>"
		// download X-modem record at address
		if (argc > 1)
			xmodem_rx((char *)p_ASCIIToHex(argv[1]));
		break;

	case COMMAND_RESET:
		printf("Reset\n");
		reset();
		while (1) continue;
		break;

	case COMMAND_REPLACE_KERNEL_VIA_XMODEM:
		printf("Updating KERNEL image\n");
		UpdateFlash(KERNEL_OFFSET);
		break;
	case COMMAND_REPLACE_FLASH_VIA_XMODEM: 
		printf("Updating FLASH image\n");
		UpdateFlash(FLASH_OFFSET);
		break;

	case COMMAND_REPLACE_ID_EEPROM: 
	{
	    char buf[25];
		printf("Testing Config EEPROM\n");
		EEWrite(0, "This is a test", 15);
		EERead(0, buf, 15);
		printf("Found '%s'\n", buf);
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

	printf("\n>");

	while (1)
		if ((ch = ((*inputFunction)(0))) > 0)
			ServicePrompt(ch);
}
