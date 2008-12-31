/******************************************************************************
 *
 * Filename: spi_flash.c
 *
 * Instantiation of SPI flash control routines supporting AT45DB161B
 *
 * Revision information:
 *
 * 17JAN2005	kb_admin	initial creation
 *				adapted from external sources
 *				tested for basic operation only!!!
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
 * $FreeBSD: src/sys/boot/arm/at91/libat91/spi_flash.c,v 1.4.6.1 2008/11/25 02:59:29 kensmith Exp $
 *****************************************************************************/

#include "at91rm9200.h"
#include "spi_flash.h"
#include "lib.h"

/*********************** PRIVATE FUNCTIONS/DATA ******************************/


static spiCommand_t	spi_command;
static char		tx_commandBuffer[8], rx_commandBuffer[8];

/*
 * .KB_C_FN_DEFINITION_START
 * void SendCommand(spiCommand_t *pCommand)
 *  Private function sends 8-bit value to the device and returns the 8-bit
 * value in response.
 * .KB_C_FN_DEFINITION_END
 */
static void
SendCommand(spiCommand_t *pCommand)
{
	AT91PS_SPI	pSPI = AT91C_BASE_SPI;

	pSPI->SPI_PTCR = AT91C_PDC_TXTDIS | AT91C_PDC_RXTDIS;

	pSPI->SPI_RPR = (unsigned)pCommand->rx_cmd;
	pSPI->SPI_RCR = pCommand->rx_cmd_size;
	pSPI->SPI_TPR = (unsigned)pCommand->tx_cmd;
	pSPI->SPI_TCR = pCommand->tx_cmd_size;

	pSPI->SPI_TNPR = (unsigned)pCommand->tx_data;
	pSPI->SPI_TNCR = pCommand->tx_data_size;
	pSPI->SPI_RNPR = (unsigned)pCommand->rx_data;
	pSPI->SPI_RNCR = pCommand->rx_data_size;

	pSPI->SPI_PTCR = AT91C_PDC_TXTEN | AT91C_PDC_RXTEN;

	// wait for completion
	while (!(pSPI->SPI_SR & AT91C_SPI_SPENDRX))
		Delay(700);
}


/*
 * .KB_C_FN_DEFINITION_START
 * char GetFlashStatus(void)
 *  Private function to return device status.
 * .KB_C_FN_DEFINITION_END
 */
static char
GetFlashStatus(void)
{
	p_memset((char *)&spi_command, 0, sizeof(spi_command));
	p_memset(tx_commandBuffer, 0, 8);
	tx_commandBuffer[0] = STATUS_REGISTER_READ;
	p_memset(rx_commandBuffer, 0, 8);
	spi_command.tx_cmd = tx_commandBuffer;
	spi_command.rx_cmd = rx_commandBuffer;
	spi_command.rx_cmd_size = 2;
	spi_command.tx_cmd_size = 2;
	SendCommand(&spi_command);
	return (rx_commandBuffer[1]);
}

/*
 * .KB_C_FN_DEFINITION_START
 * void WaitForDeviceReady(void)
 *  Private function to poll until the device is ready for next operation.
 * .KB_C_FN_DEFINITION_END
 */
static void
WaitForDeviceReady(void)
{
	while (!(GetFlashStatus() & 0x80)) ;
}

/*************************** GLOBAL FUNCTIONS ********************************/


/*
 * .KB_C_FN_DEFINITION_START
 * void SPI_ReadFlash(unsigned flash_addr, unsigned dest_addr, unsigned size)
 *  Global function to read the SPI flash device using the continuous read
 * array command.
 * .KB_C_FN_DEFINITION_END
 */
void
SPI_ReadFlash(unsigned flash_addr, char *dest_addr, unsigned size)
{
	unsigned	pageAddress, byteAddress;

	// determine page address
	pageAddress = flash_addr / FLASH_PAGE_SIZE;

	// determine byte address
	byteAddress = flash_addr % FLASH_PAGE_SIZE;

	p_memset(tx_commandBuffer, 0, 8);
#ifdef BOOT_BWCT
	tx_commandBuffer[0] = 0xd2;
	tx_commandBuffer[1] = ((pageAddress >> 6) & 0xFF);
	tx_commandBuffer[2] = ((pageAddress << 2) & 0xFC) |
				((byteAddress >> 8) & 0x3);
	tx_commandBuffer[3] = byteAddress & 0xFF;
	spi_command.tx_cmd = tx_commandBuffer;
	spi_command.tx_cmd_size = 8;
	spi_command.tx_data_size = size;
	spi_command.tx_data = dest_addr;

	p_memset(rx_commandBuffer, 0, 8);
	spi_command.rx_cmd = rx_commandBuffer;
	spi_command.rx_cmd_size = 8;
	spi_command.rx_data_size = size;
	spi_command.rx_data = dest_addr;
#else
	tx_commandBuffer[0] = CONTINUOUS_ARRAY_READ_HF;
	tx_commandBuffer[1] = ((pageAddress >> 5) & 0xFF);
	tx_commandBuffer[2] = ((pageAddress << 3) & 0xF8) |
				((byteAddress >> 8) & 0x7);
	tx_commandBuffer[3] = byteAddress & 0xFF;
	spi_command.tx_cmd = tx_commandBuffer;
	spi_command.tx_cmd_size = 5;
	spi_command.tx_data_size = size;
	spi_command.tx_data = dest_addr;

	p_memset(rx_commandBuffer, 0, 8);
	spi_command.rx_cmd = rx_commandBuffer;
	spi_command.rx_cmd_size = 5;
	spi_command.rx_data_size = size;
	spi_command.rx_data = dest_addr;
#endif

	SendCommand(&spi_command);
}


/*
 * .KB_C_FN_DEFINITION_START
 * void SPI_WriteFlash(unsigned flash_addr, unsigned src_addr, unsigned size)
 *  Global function to program the SPI flash device.  Notice the warning
 * provided in lower-level functions regarding corruption of data in non-
 * page aligned write operations.
 * .KB_C_FN_DEFINITION_END
 */
void
SPI_WriteFlash(unsigned flash_addr, char *src_addr, unsigned size)
{
	unsigned	pageAddress, byteAddress;

	// determine page address
	pageAddress = flash_addr / FLASH_PAGE_SIZE;

	// determine byte address
	byteAddress = flash_addr % FLASH_PAGE_SIZE;

	p_memset(tx_commandBuffer, 0, 8);
#ifdef BOOT_BWCT
	tx_commandBuffer[0] = 0x82;
	tx_commandBuffer[1] = ((pageAddress >> 6) & 0xFF);
	tx_commandBuffer[2] = ((pageAddress << 2) & 0xFC) |
				((byteAddress >> 8) & 0x3);
	tx_commandBuffer[3] = (byteAddress & 0xFF);
#else
	tx_commandBuffer[0] = PROGRAM_THROUGH_BUFFER;
	tx_commandBuffer[1] = ((pageAddress >> 5) & 0xFF);
	tx_commandBuffer[2] = ((pageAddress << 3) & 0xF8) |
				((byteAddress >> 8) & 0x7);
	tx_commandBuffer[3] = (byteAddress & 0xFF);
#endif

	p_memset(rx_commandBuffer, 0, 8);

	spi_command.tx_cmd = tx_commandBuffer;
	spi_command.rx_cmd = rx_commandBuffer;
	spi_command.rx_cmd_size = 4;
	spi_command.tx_cmd_size = 4;

	spi_command.tx_data_size = size;
	spi_command.tx_data = src_addr;
	spi_command.rx_data_size = size;
	spi_command.rx_data = src_addr;

	SendCommand(&spi_command);

	WaitForDeviceReady();
}

/*
 * .KB_C_FN_DEFINITION_START
 * void SPI_InitFlash(void)
 *  Global function to initialize the SPI flash device/accessor functions.
 * .KB_C_FN_DEFINITION_END
 */
void
SPI_InitFlash(void)
{
	AT91PS_PIO	pPio;
	AT91PS_SPI	pSPI = AT91C_BASE_SPI;
	unsigned	value;

	// enable CS0, CLK, MOSI, MISO
	pPio = (AT91PS_PIO)AT91C_BASE_PIOA;
	pPio->PIO_ASR = AT91C_PA3_NPCS0 | AT91C_PA1_MOSI | AT91C_PA0_MISO |
	    AT91C_PA2_SPCK;
	pPio->PIO_PDR = AT91C_PA3_NPCS0 | AT91C_PA1_MOSI | AT91C_PA0_MISO |
	    AT91C_PA2_SPCK;

	// enable clocks to SPI
	AT91C_BASE_PMC->PMC_PCER = 1u << AT91C_ID_SPI;

	// reset the SPI
	pSPI->SPI_CR = AT91C_SPI_SWRST;

	pSPI->SPI_MR = (0xf << 24) | AT91C_SPI_MSTR | AT91C_SPI_MODFDIS |
	    (0xE << 16);

	pSPI->SPI_CSR[0] = AT91C_SPI_CPOL | (4 << 16) | (2 << 8);
	pSPI->SPI_CR = AT91C_SPI_SPIEN;

	pSPI->SPI_PTCR = AT91C_PDC_TXTDIS;
	pSPI->SPI_PTCR = AT91C_PDC_RXTDIS;
	pSPI->SPI_RNPR = 0;
	pSPI->SPI_RNCR = 0;
	pSPI->SPI_TNPR = 0;
	pSPI->SPI_TNCR = 0;
	pSPI->SPI_RPR = 0;
	pSPI->SPI_RCR = 0;
	pSPI->SPI_TPR = 0;
	pSPI->SPI_TCR = 0;
	pSPI->SPI_PTCR = AT91C_PDC_RXTEN;
	pSPI->SPI_PTCR = AT91C_PDC_TXTEN;

	value = pSPI->SPI_RDR;
	value = pSPI->SPI_SR;

#ifdef BOOT_BWCT
	if (((value = GetFlashStatus()) & 0xFC) != 0xB4)
		printf(" Bad SPI status: 0x%x\n", value);
#else
	if (((value = GetFlashStatus()) & 0xFC) != 0xBC)
		printf(" Bad SPI status: 0x%x\n", value);
#endif
}
