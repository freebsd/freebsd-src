/* $FreeBSD: src/sys/dev/spibus/spi.h,v 1.2.10.1.6.1 2010/12/21 17:09:25 kensmith Exp $ */

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
