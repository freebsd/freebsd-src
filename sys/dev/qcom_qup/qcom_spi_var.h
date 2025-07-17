/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2021 Adrian Chadd <adrian@FreeBSD.org>
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

#ifndef	__QCOM_SPI_VAR_H__
#define	__QCOM_SPI_VAR_H__

typedef enum {
	QCOM_SPI_HW_QPI_V1_1 = 1,
	QCOM_SPI_HW_QPI_V2_1 = 2,
	QCOM_SPI_HW_QPI_V2_2 = 3,
} qcom_spi_hw_version_t;

#define	CS_MAX				4

struct qcom_spi_softc {
	device_t		sc_dev;
	device_t		spibus;

	uint32_t		sc_debug;

	struct resource		*sc_mem_res;
	struct resource		*sc_irq_res;
	void			*sc_irq_h;

	struct mtx		sc_mtx;
	bool			sc_busy; /* an SPI transfer (cmd+data)
					  * is active */

	qcom_spi_hw_version_t	hw_version;

	clk_t			clk_core;	/* QUP/SPI core */
	clk_t			clk_iface;	/* SPI interface */

	/* For GPIO chip selects .. */
	gpio_pin_t		cs_pins[CS_MAX];

	struct {
		/*
		 * FIFO size / block size in bytes.
		 *
		 * The FIFO slots are DWORD sized, not byte sized.
		 * So if the transfer size is set to 8 bits per
		 * word (which is what we'll support initially)
		 * the effective available FIFO is
		 * fifo_size / sizeof(uint32_t).
		 */
		uint32_t input_block_size;
		uint32_t output_block_size;
		uint32_t input_fifo_size;
		uint32_t output_fifo_size;

		uint32_t cs_select;
		uint32_t num_cs;
		uint32_t max_frequency;
	} config;

	struct {
		uint32_t transfer_mode; /* QUP_IO_M_MODE_* */
		uint32_t transfer_word_size; /* how many bytes in a transfer word */
		uint32_t frequency;
		bool cs_high; /* true if CS is high for active */
	} state;

	struct {
		bool tx_dma_done;
		bool rx_dma_done;
		bool done;
		bool do_tx;
		bool do_rx;
		bool error;
	} intr;

	struct {
		bool active; /* a (sub) transfer is active */
		uint32_t num_words; /* number of word_size words to transfer */
		const char *tx_buf;
		int tx_len;
		int tx_offset;
		char *rx_buf;
		int rx_len;
		int rx_offset;
		bool done;
	} transfer;
};

#define	QCOM_SPI_QUP_VERSION_V1(sc)	\
	    ((sc)->hw_version == QCOM_SPI_HW_QPI_V1_1)

#define	QCOM_SPI_LOCK(sc)		mtx_lock(&(sc)->sc_mtx)
#define	QCOM_SPI_UNLOCK(sc)		mtx_unlock(&(sc)->sc_mtx)
#define	QCOM_SPI_ASSERT_LOCKED(sc)	mtx_assert(&(sc)->sc_mtx, MA_OWNED)
#define	QCOM_SPI_READ_4(sc, reg)	bus_read_4((sc)->sc_mem_res, (reg))
#define	QCOM_SPI_WRITE_4(sc, reg, val)	bus_write_4((sc)->sc_mem_res, \
	    (reg), (val))

/* XXX TODO: the region size should be in the tag or softc */
#define	QCOM_SPI_BARRIER_WRITE(sc)	bus_barrier((sc)->sc_mem_res,	\
	    0, 0x600, BUS_SPACE_BARRIER_WRITE)
#define	QCOM_SPI_BARRIER_READ(sc)	bus_barrier((sc)->sc_mem_res,	\
	    0, 0x600, BUS_SPACE_BARRIER_READ)
#define	QCOM_SPI_BARRIER_RW(sc)		bus_barrier((sc)->sc_mem_res,	\
	     0, 0x600, BUS_SPACE_BARRIER_READ | BUS_SPACE_BARRIER_WRITE)

extern	int qcom_spi_hw_read_controller_transfer_sizes(
	    struct qcom_spi_softc *sc);
extern	int qcom_spi_hw_qup_set_state_locked(struct qcom_spi_softc *sc,
	    uint32_t state);
extern	int qcom_spi_hw_qup_init_locked(struct qcom_spi_softc *sc);
extern	int qcom_spi_hw_spi_init_locked(struct qcom_spi_softc *sc);
extern	int qcom_spi_hw_spi_cs_force(struct qcom_spi_softc *sc, int cs,
	    bool enable);
extern	int qcom_spi_hw_interrupt_handle(struct qcom_spi_softc *sc);
extern	int qcom_spi_hw_setup_transfer_selection(struct qcom_spi_softc *sc,
	    uint32_t len);
extern	int qcom_spi_hw_complete_transfer(struct qcom_spi_softc *sc);

extern	int qcom_spi_hw_setup_current_transfer(struct qcom_spi_softc *sc);
extern	int qcom_spi_hw_setup_pio_transfer_cnt(struct qcom_spi_softc *sc);
extern	int qcom_spi_hw_setup_block_transfer_cnt(struct qcom_spi_softc *sc);
extern	int qcom_spi_hw_setup_io_modes(struct qcom_spi_softc *sc);
extern	int qcom_spi_hw_setup_spi_io_clock_polarity(
	    struct qcom_spi_softc *sc, bool cpol);
extern	int qcom_spi_hw_setup_spi_config(struct qcom_spi_softc *sc,
	    uint32_t clock_val, bool cpha);
extern	int qcom_spi_hw_setup_qup_config(struct qcom_spi_softc *sc,
	    bool is_tx, bool is_rx);
extern	int qcom_spi_hw_setup_operational_mask(struct qcom_spi_softc *sc);
extern	int qcom_spi_hw_ack_write_pio_fifo(struct qcom_spi_softc *sc);
extern	int qcom_spi_hw_ack_opmode(struct qcom_spi_softc *sc);
extern	int qcom_spi_hw_write_pio_fifo(struct qcom_spi_softc *sc);
extern	int qcom_spi_hw_write_pio_block(struct qcom_spi_softc *sc);
extern	int qcom_spi_hw_read_pio_fifo(struct qcom_spi_softc *sc);
extern	int qcom_spi_hw_read_pio_block(struct qcom_spi_softc *sc);
extern	int qcom_spi_hw_do_full_reset(struct qcom_spi_softc *sc);

#endif	/* __QCOM_SPI_VAR_H__ */
