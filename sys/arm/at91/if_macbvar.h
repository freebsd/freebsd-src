/*
 * $FreeBSD$
 */

#ifndef	_IF_MACB_H
#define	_IF_MACB_H

#define	MACB_MAX_TX_BUFFERS	64
#define	MACB_MAX_RX_BUFFERS	256

#define MAX_FRAGMENT		20
#define DATA_SIZE		128

#define	MACB_DESC_INC(x, y)	((x) = ((x) + 1) % (y))

#define MACB_TIMEOUT		1000

struct eth_tx_desc {
	uint32_t		addr;
	uint32_t		flags;
#define TD_OWN		(1U << 31)
#define TD_LAST		(1 << 15)
#define	TD_WRAP_MASK		(1 << 30)
};

struct eth_rx_desc {
	uint32_t		addr;
#define	RD_LEN_MASK		0x7ff
#define	RD_WRAP_MASK		0x00000002
#define	RD_OWN			0x00000001

	uint32_t		flags;
#define RD_BROADCAST		(1U << 31)
#define RD_MULTICAST		(1 << 30)
#define RD_UNICAST		(1 << 29)
#define RD_EXTERNAL		(1 << 28)
#define RD_TYPE_ID		(1 << 22)
#define RD_PRIORITY		(1 << 20)
#define RD_VLAN		(1 << 21)
#define RD_CONCAT		(1 << 16)
#define RD_EOF		(1 << 15)
#define RD_SOF		(1 << 14)
#define RD_OFFSET_MASK		(1 << 13)|(1 << 12)
#define RD_LENGTH_MASK		(0x00000FFF)

};


struct rx_desc_info {
	struct mbuf *buff;
	bus_dmamap_t dmamap;
};

struct tx_desc_info {
	struct mbuf *buff;
	bus_dmamap_t dmamap;
};


struct macb_chain_data{	
	struct mbuf		*rxhead;
	struct mbuf		*rxtail;
};

struct macb_softc
{
	struct ifnet *ifp;		/* ifnet pointer */
	struct mtx sc_mtx;		/* global mutex */

	bus_dma_tag_t	sc_parent_tag;	/* parent bus DMA tag */

	device_t dev;			/* Myself */
	device_t miibus;		/* My child miibus */
	void *intrhand;			/* Interrupt handle */
	void *intrhand_qf;		/* queue full */
	void *intrhand_tx;		/* tx complete */
	void *intrhand_status;		/* error status */

	struct resource *irq_res;	/* transmit */
	struct resource *irq_res_rec;	/* receive */
	struct resource *irq_res_qf;	/* queue full */
	struct resource *irq_res_status; /* status */

	struct resource	*mem_res;	/* Memory resource */

	struct callout tick_ch;		/* Tick callout */

	struct taskqueue *sc_tq;
	struct task	sc_intr_task;
	struct task	sc_tx_task;
	struct task	sc_link_task;

	bus_dmamap_t	dmamap_ring_tx;
	bus_dmamap_t	dmamap_ring_rx;

	/*dma tag for ring*/
	bus_dma_tag_t	dmatag_ring_tx;
	bus_dma_tag_t	dmatag_ring_rx;

	/*dma tag for data*/
	bus_dma_tag_t	dmatag_data_tx;
	bus_dma_tag_t	dmatag_data_rx;

	/*the ring*/
	struct eth_tx_desc	*desc_tx;
	struct eth_rx_desc	*desc_rx;

	/*ring physical address*/
	bus_addr_t	ring_paddr_tx;
	bus_addr_t	ring_paddr_rx;

	/*index of last received descriptor*/
	int		rx_cons;
	struct rx_desc_info rx_desc[MACB_MAX_RX_BUFFERS];

	/* tx producer index */
	uint32_t tx_prod;
	/* tx consumer index */
	uint32_t tx_cons;
	int	tx_cnt;

	struct tx_desc_info tx_desc[MACB_MAX_TX_BUFFERS];

	int macb_watchdog_timer;

#define	MACB_FLAG_LINK		0x0001

	int flags;
	int if_flags;
	struct at91_pmc_clock *clk;

	struct macb_chain_data	macb_cdata;
	int clock;

	uint32_t use_rmii; /* 0 or USRIO_RMII */
};

#endif
