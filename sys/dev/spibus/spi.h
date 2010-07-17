/* $FreeBSD: src/sys/dev/spibus/spi.h,v 1.2.10.1.4.1 2010/06/14 02:09:06 kensmith Exp $ */

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
