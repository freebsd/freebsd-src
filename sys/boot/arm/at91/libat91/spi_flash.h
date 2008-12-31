/*******************************************************************************
 *
 * Filename: spi_flash.h
 *
 * Definition of flash control routines supporting AT45DB161B
 *
 * Revision information:
 *
 * 17JAN2005	kb_admin	initial creation
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
 * $FreeBSD: src/sys/boot/arm/at91/libat91/spi_flash.h,v 1.2.6.1 2008/11/25 02:59:29 kensmith Exp $
 ******************************************************************************/

#ifndef _SPI_FLASH_H_
#define _SPI_FLASH_H_

typedef struct {
	char		*tx_cmd;
	unsigned	tx_cmd_size;
	char		*rx_cmd;
	unsigned	rx_cmd_size;
	char		*tx_data;
	unsigned	tx_data_size;
	char		*rx_data;
	unsigned	rx_data_size;
} spiCommand_t;

void SPI_ReadFlash(unsigned flash_addr, char *dest_addr, unsigned size);
void SPI_WriteFlash(unsigned flash_addr, char *dest_addr, unsigned size);
void SPI_InitFlash(void);

void SPI_GetId(unsigned *id);

#ifdef BOOT_BWCT
#define FLASH_PAGE_SIZE	528
#else
#define FLASH_PAGE_SIZE	1056
#endif

// Flash commands

#define CONTINUOUS_ARRAY_READ		0xE8
#define CONTINUOUS_ARRAY_READ_HF	0x0B
#define CONTINUOUS_ARRAY_READ_LF	0x03
#define STATUS_REGISTER_READ		0xD7
#define PROGRAM_THROUGH_BUFFER		0x82
#define MANUFACTURER_ID			0x9F

#endif
