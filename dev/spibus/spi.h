/* $FreeBSD$ */

struct spi_command {
	void	*tx_cmd;
	uint32_t tx_cmd_sz;
	void	*rx_cmd;
	uint32_t rx_cmd_sz;
	void	*tx_data;
	uint32_t tx_data_sz;
	void	*rx_data;
	uint32_t rx_data_sz;
};

#define	SPI_CHIP_SELECT_HIGH	0x1		/* Chip select high (else low) */
