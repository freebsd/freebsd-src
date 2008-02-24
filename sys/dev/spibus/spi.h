/* $FreeBSD: src/sys/dev/spibus/spi.h,v 1.2 2006/07/17 21:18:03 cognet Exp $ */

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
