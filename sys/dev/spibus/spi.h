/*-
 * Copyright (c) 2006 M. Warner Losh <imp@FreeBSD.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

struct spi_command {
	void	*tx_cmd;
	uint32_t tx_cmd_sz;
	void	*rx_cmd;
	uint32_t rx_cmd_sz;
	void	*tx_data;
	uint32_t tx_data_sz;
	void	*rx_data;
	uint32_t rx_data_sz;
	uint32_t flags;
};
#define	SPI_COMMAND_INITIALIZER	{ 0 }

#define	SPI_FLAG_KEEP_CS	0x1		/* Keep chip select asserted */
#define	SPI_FLAG_NO_SLEEP	0x2		/* Prevent driver from sleeping (use polling) */

#define	SPI_CHIP_SELECT_HIGH	0x1		/* Chip select high (else low) */

#ifdef FDT
#define	SPIBUS_FDT_PNP_INFO(t)	FDTCOMPAT_PNP_INFO(t, spibus)
#else
#define	SPIBUS_FDT_PNP_INFO(t)
#endif

#ifdef DEV_ACPI
#define	SPIBUS_ACPI_PNP_INFO(t)	ACPICOMPAT_PNP_INFO(t, spibus)
#else
#define	SPIBUS_ACPI_PNP_INFO(t)
#endif
