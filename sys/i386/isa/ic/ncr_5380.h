/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@login.dkuug.dk> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 *
 * $Id$
 *
 * Definitions for 5380 SCSI-controller chip.
 *
 * Derived from "NCR 53C80 Family SCSI Protocol Controller Data Manual"
 */

#ifndef  _IC_NCR_5380_H_ 
#define  _IC_NCR_5380_H_ 

#if 0 /* XXX */
/*
 * Register map
 */

typedef struct {
    volatile u_char sci_data;	/* r: Current data */
#define	sci_odata sci_data	/* w: Out data */

    volatile u_char sci_icmd;	/* rw:Initiator command */

    volatile u_char sci_mode;	/* rw:Mode */

    volatile u_char sci_tcmd;	/* rw:Target command */

    volatile u_char sci_bus_csr;/* r: Bus Status */
#define	sci_sel_enb sci_bus_csr	/* w: Select enable */

    volatile u_char sci_csr;	/* r: Status */
#define	sci_dma_send sci_csr	/* w: Start dma send data */

    volatile u_char sci_idata;	/* r: Input data */
#define	sci_trecv sci_idata	/* w: Start dma recv, target */

    volatile u_char sci_iack;	/* r: Interrupt Acknowledge  */
#define	sci_irecv sci_iack	/* w: Start dma recv, initiator */
} sci_regmap_t;


/*
 * Initiator command register
 */

#define SCI_ICMD_DATA		0x01	/* rw:Assert data bus   */
#define SCI_ICMD_ATN		0x02	/* rw:Assert ATN signal */
#define SCI_ICMD_SEL		0x04	/* rw:Assert SEL signal */
#define SCI_ICMD_BSY		0x08	/* rw:Assert BSY signal */
#define SCI_ICMD_ACK		0x10	/* rw:Assert ACK signal */
#define SCI_ICMD_LST		0x20	/* r: Lost arbitration */
#define SCI_ICMD_DIFF	SCI_ICMD_LST	/* w: Differential cable */
#define SCI_ICMD_AIP		0x40	/* r: Arbitration in progress */
#define SCI_ICMD_TEST	SCI_ICMD_AIP	/* w: Test mode */
#define SCI_ICMD_RST		0x80	/* rw:Assert RST signal */


/*
 * Mode register
 */

#define SCI_MODE_ARB		0x01	/* rw: Start arbitration */
#define SCI_MODE_DMA		0x02	/* rw: Enable DMA xfers */
#define SCI_MODE_MONBSY		0x04	/* rw: Monitor BSY signal */
#define SCI_MODE_DMA_IE		0x08	/* rw: Enable DMA complete interrupt */
#define SCI_MODE_PERR_IE	0x10	/* rw: Interrupt on parity errors */
#define SCI_MODE_PAR_CHK	0x20	/* rw: Check parity */
#define SCI_MODE_TARGET		0x40	/* rw: Target mode (Initiator if 0) */
#define SCI_MODE_BLOCKDMA	0x80	/* rw: Block-mode DMA handshake (MBZ) */


/*
 * Target command register
 */

#define SCI_TCMD_IO		0x01	/* rw: Assert I/O signal */
#define SCI_TCMD_CD		0x02	/* rw: Assert C/D signal */
#define SCI_TCMD_MSG		0x04	/* rw: Assert MSG signal */
#define SCI_TCMD_PHASE_MASK	0x07	/* r:  Mask for current bus phase */
#define SCI_TCMD_REQ		0x08	/* rw: Assert REQ signal */
#define	SCI_TCMD_LAST_SENT	0x80	/* ro: Last byte was xferred
					 *     (not on 5380/1) */

#define	SCI_PHASE(x)		SCSI_PHASE(x)

/*
 * Current (SCSI) Bus status
 */

#define SCI_BUS_DBP		0x01	/* r:  Data Bus parity */
#define SCI_BUS_SEL		0x02	/* r:  SEL signal */
#define SCI_BUS_IO		0x04	/* r:  I/O signal */
#define SCI_BUS_CD		0x08	/* r:  C/D signal */
#define SCI_BUS_MSG		0x10	/* r:  MSG signal */
#define SCI_BUS_REQ		0x20	/* r:  REQ signal */
#define SCI_BUS_BSY		0x40	/* r:  BSY signal */
#define SCI_BUS_RST		0x80	/* r:  RST signal */

#define	SCI_CUR_PHASE(x)	SCSI_PHASE((x)>>2)

/*
 * Bus and Status register
 */

#define SCI_CSR_ACK		0x01	/* r:  ACK signal */
#define SCI_CSR_ATN		0x02	/* r:  ATN signal */
#define SCI_CSR_DISC		0x04	/* r:  Disconnected (BSY==0) */
#define SCI_CSR_PHASE_MATCH	0x08	/* r:  Bus and SCI_TCMD match */
#define SCI_CSR_INT		0x10	/* r:  Interrupt request */
#define SCI_CSR_PERR		0x20	/* r:  Parity error */
#define SCI_CSR_DREQ		0x40	/* r:  DMA request */
#define SCI_CSR_DONE		0x80	/* r:  DMA count is zero */

#endif /* XXX */

#define R_CSDR	0		/* R   Current SCSI Data Reg.		*/
#define W_ODR	0		/* W   Output Data Reg.			*/

#define R_ICR	1		/* R   Initiator Command Reg.		*/
#define R_ICR_ASSERT_RST		0x80
#define R_ICR_ARBITRATION_IN_PROGRESS	0x40
#define R_ICR_LOST_ARBITRATION		0x20
#define R_ICR_ASSERT_ACK		0x10
#define R_ICR_ASSERT_BSY		0x08
#define R_ICR_ASSERT_SEL		0x04
#define R_ICR_ASSERT_ATN		0x02
#define R_ICR_ASSERT_DATA_BUS		0x01

#define W_ICR	1		/* W   Initiator Command Reg.		*/
#define W_ICR_ASSERT_RST		0x80
#define W_ICR_TRI_STATE_MODE		0x40
#define W_ICR_DIFF_ENABLE		0x20
#define W_ICR_ASSERT_ACK		0x10
#define W_ICR_ASSERT_BSY		0x08
#define W_ICR_ASSERT_SEL		0x04
#define W_ICR_ASSERT_ATN		0x02
#define W_ICR_ASSERT_DATA_BUS		0x01

/* 
 * The mask to use when doing read_modify_write on ICR.
 */
#define RW_ICR_MASK	(~(W_ICR_DIFF_ENABLE|W_ICR_TRI_STATE_MODE))

#define RW_MR	2		/* RW  Mode Reg.			*/
#define RW_MR_BLOCK_MODE_DMA		0x80
#define RW_MR_TARGET_MODE		0x40
#define RW_MR_ENABLE_PARITY_CHECKING	0x20
#define RW_MR_ENABLE_PARITY_INTERRUPT	0x10
#define RW_MR_ENABLE_EOP_INTERRUPT	0x08
#define RW_MR_MONITOR_BUSY		0x04
#define RW_MR_DMA_MODE			0x02
#define RW_MR_ARBITRATE			0x01

#define R_TCR	3		/* R   Target Command Reg.		*/
#define R_TCR_LAST_BYTE_SENT		0x80
/*      R_TCR_RESERVED			0x40 */
/*      R_TCR_RESERVED			0x20 */
/*      R_TCR_RESERVED			0x10 */
#define R_TCR_ASSERT_REQ		0x08
#define R_TCR_ASSERT_MSG		0x04
#define R_TCR_ASSERT_CD			0x02
#define R_TCR_ASSERT_IO			0x01

#define W_TCR	3		/* W   Target Command Reg.		*/
/*      W_TCR_RESERVED			0x80 */
/*      W_TCR_RESERVED			0x40 */
/*      W_TCR_RESERVED			0x20 */
/*      W_TCR_RESERVED			0x10 */
#define W_TCR_ASSERT_REQ		0x08
#define W_TCR_ASSERT_MSG		0x04
#define W_TCR_ASSERT_CD			0x02
#define W_TCR_ASSERT_IO			0x01

#define R_CSCR	4		/* R   Current SCSI Bus Status Reg.	*/
#define R_CSCR_RST			0x80
#define R_CSCR_BSY			0x40
#define R_CSCR_REQ			0x20
#define R_CSCR_MSG			0x10
#define R_CSCR_CD			0x08
#define R_CSCR_IO			0x04
#define R_CSCR_SEL			0x02
#define R_CSCR_ACK			0x01

#define W_SER	4		/* W   Select Enable Reg.		*/
#define W_SER_SCSI_ID_7			0x80
#define W_SER_SCSI_ID_6			0x40
#define W_SER_SCSI_ID_5			0x20
#define W_SER_SCSI_ID_4			0x10
#define W_SER_SCSI_ID_3			0x08
#define W_SER_SCSI_ID_2			0x04
#define W_SER_SCSI_ID_1			0x02
#define W_SER_SCSI_ID_0			0x01

#define R_BSR	5		/* R   Bus and Status Reg.		*/
#define R_BSR_END_OF_DMA_XFER		0x80
#define R_BSR_DMA_REQUEST		0x40
#define R_BSR_PARITY_ERROR		0x20
#define R_BSR_INTERRUPT_REQUEST_ACTIVE	0x10
#define R_BSR_PHASE_MISMATCH		0x08
#define R_BSR_BUSY_ERROR		0x04
#define R_BSR_ATN			0x02
#define R_BSR_ACK			0x01

#define W_SDSR	5		/* W   Start DMA Send Reg.		*/
#define R_IDR	6		/* R   Input Data Reg.			*/
#define W_SDTR	6		/* W   Start DMA Target Receive Reg.	*/
#define R_RPIR	7		/* R   Reset Parity/Interrupt Reg.	*/
#define W_SDIR	7		/* W  Start DMA Initiator Receive Reg.	*/

#endif /* _IC_NCR_5380_H_ */
