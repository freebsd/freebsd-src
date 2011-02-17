/*
 * $FreeBSD$
 */

#ifndef MACB_REG_H
#define MACB_REG_H

#define EMAC_NCR		0x00
#define EMAC_NCFGR		0x04
#define EMAC_TSR		0x14
#define EMAC_RSR		0x20
#define EMAC_ISR		0x24
#define EMAC_IER		0x28
#define EMAC_IDR		0x2C
#define EMAC_IMR		0x30



#define EMAC_RBQP		0x18
#define EMAC_TBQP		0x1C

#define EMAC_HRB		0x90
#define EMAC_HRT		0x94

#define EMAC_SA1B		0x98
#define EMAC_SA1T		0x9C

#define EMAC_USRIO		0xC0

#define EMAC_MAN		0x34		/* EMAC PHY Maintenance Register */
#define EMAC_SR		0x08		/* EMAC STatus Register */
#define EMAC_SR_LINK	(1U << 0)	/* Reserved! */
#define EMAC_SR_MDIO	(1U << 1)	/* MDIO pin status */
#define EMAC_SR_IDLE	(1U << 2)	/* IDLE (PHY logic) */

#define RX_ENABLE		(1 << 2)
#define TX_ENABLE		(1 << 3)
#define MPE_ENABLE		(1 << 4)


/* EMAC_MAN */
#define EMAC_MAN_BITS	0x40020000	/* HIGH and CODE bits */
#define EMAC_MAN_READ	(2U << 28)
#define EMAC_MAN_WRITE	(1U << 28)
#define EMAC_MAN_PHYA_BIT 23
#define EMAC_MAN_REGA_BIT 18
#define EMAC_MAN_VALUE_MASK	0xffffU
#define EMAC_MAN_REG_WR(phy, reg, val) \
		(EMAC_MAN_BITS | EMAC_MAN_WRITE | ((phy) << EMAC_MAN_PHYA_BIT) | \
		((reg) << EMAC_MAN_REGA_BIT) | ((val) & EMAC_MAN_VALUE_MASK))

#define EMAC_MAN_REG_RD(phy, reg) \
		(EMAC_MAN_BITS | EMAC_MAN_READ | ((phy) << EMAC_MAN_PHYA_BIT) | \
		((reg) << EMAC_MAN_REGA_BIT))

#define RCOMP_INTERRUPT		(1 << 1)
#define RXUBR_INTERRUPT		(1 << 2)
#define TUBR_INTERRUPT		(1 << 3)
#define TUND_INTERRUPT		(1 << 4)
#define RLE_INTERRUPT		(1 << 5)
#define TXERR_INTERRUPT		(1 << 6)
#define ROVR_INTERRUPT		(1 << 10)
#define HRESP_INTERRUPT		(1 << 11)
#define TCOMP_INTERRUPT		(1 << 7)

#define CLEAR_STAT		(1 << 5)

#define TRANSMIT_START		(1 << 9)
#define TRANSMIT_STOP		(1 << 10)

/*Transmit status register flags*/
#define	TSR_UND			(1 << 6)
#define	TSR_COMP		(1 << 5)
#define	TSR_BEX			(1 << 4)
#define	TSR_TGO			(1 << 3)
#define	TSR_RLE			(1 << 2)
#define	TSR_COL			(1 << 1)
#define	TSR_UBR			(1 << 0)

#define	CFG_SPD		(1 << 0)
#define	CFG_FD		(1 << 1)
#define	CFG_CAF		(1 << 4)
#define	CFG_NBC		(1 << 5)
#define	CFG_MTI		(1 << 6)
#define	CFG_UNI		(1 << 7)
#define	CFG_BIG		(1 << 8)

#define	CFG_CLK_8		(0)
#define	CFG_CLK_16		(1)
#define	CFG_CLK_32		(2)
#define	CFG_CLK_64		(3)

#define	CFG_PAE		(1 << 13)

#define CFG_RBOF_0	(0 << 14)
#define CFG_RBOF_1	(1 << 14)
#define CFG_RBOF_2	(2 << 14)
#define CFG_RBOF_3	(3 << 14)

#define	CFG_DRFCS	(1 << 17)

#define USRIO_CLOCK	(1 << 1)



#endif
