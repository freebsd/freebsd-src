/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@login.dkuug.dk> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 * Modified by Serge Vakulenko (vak@cronyx.ru)
 *
 * ncr_5380.h,v 1.2 1994/09/11 20:29:18 phk Exp
 *
 * Definitions for 5380 SCSI-controller chip.
 *
 * Derived from "NCR 53C80 Family SCSI Protocol Controller Data Manual"
 */

#ifndef  _IC_NCR_5380_H_
#define  _IC_NCR_5380_H_

#define C80_CSDR                0       /* ro - Current SCSI Data Reg. */
#define C80_ODR                 0       /* wo - Output Data Reg. */

#define C80_ICR                 1       /* rw - Initiator Command Reg. */
#define ICR_ASSERT_RST                  0x80
#define ICR_ARBITRATION_IN_PROGRESS     0x40 /* read only */
#define ICR_TRI_STATE_MODE              0x40 /* write only */
#define ICR_LOST_ARBITRATION            0x20 /* read only */
#define ICR_DIFF_ENABLE                 0x20 /* write only */
#define ICR_ASSERT_ACK                  0x10
#define ICR_ASSERT_BSY                  0x08
#define ICR_ASSERT_SEL                  0x04
#define ICR_ASSERT_ATN                  0x02
#define ICR_ASSERT_DATA_BUS             0x01
#define ICR_BITS "\20\1dbus\2atn\3sel\4bsy\5ack\6arblost\7arb\10rst"

/*
 * The mask to use when doing read_modify_write on ICR.
 */
#define ICR_MASK (~(ICR_DIFF_ENABLE | ICR_TRI_STATE_MODE))

#define C80_MR                  2       /* rw - Mode Reg. */
#define MR_BLOCK_MODE_DMA               0x80
#define MR_TARGET_MODE                  0x40
#define MR_ENABLE_PARITY_CHECKING       0x20
#define MR_ENABLE_PARITY_INTERRUPT      0x10
#define MR_ENABLE_EOP_INTERRUPT         0x08
#define MR_MONITOR_BUSY                 0x04
#define MR_DMA_MODE                     0x02
#define MR_ARBITRATE                    0x01
#define MR_BITS "\20\1arb\2dma\3mbusy\4eopintr\5parintr\6pcheck\7targ\10blk"

#define C80_TCR                 3       /* rw - Target Command Reg. */
#define TCR_LAST_BYTE_SENT              0x80 /* read only */
#define TCR_ASSERT_REQ                  0x08
#define TCR_ASSERT_MSG                  0x04
#define TCR_ASSERT_CD                   0x02
#define TCR_ASSERT_IO                   0x01
#define TCR_BITS "\20\1i/o\2c/d\3msg\4req\10lastbyte"

#define C80_CSBR                4       /* ro - Current SCSI Bus Status Reg. */
#define CSBR_RST                        0x80
#define CSBR_BSY                        0x40
#define CSBR_REQ                        0x20
#define CSBR_MSG                        0x10
#define CSBR_CD                         0x08
#define CSBR_IO                         0x04
#define CSBR_SEL                        0x02
#define CSBR_ACK                        0x01
#define CSBR_BITS "\20\1ack\2sel\3i/o\4c/d\5msg\6req\7bsy\10rst"

#define C80_SER                 4       /* wo - Select Enable Reg. */

#define C80_BSR                 5       /* ro - Bus and Status Reg. */
#define BSR_END_OF_DMA_XFER             0x80
#define BSR_DMA_REQUEST                 0x40
#define BSR_PARITY_ERROR                0x20
#define BSR_INTERRUPT_REQUEST_ACTIVE    0x10
#define BSR_PHASE_MISMATCH              0x08
#define BSR_BUSY_ERROR                  0x04
#define BSR_ATN                         0x02
#define BSR_ACK                         0x01
#define BSR_BITS "\20\1ack\2atn\3busyerr\4pherr\5irq\6parerr\7drq\10dend"

#define C80_SDSR                5       /* wo - Start DMA Send Reg. */
#define C80_IDR                 6       /* ro - Input Data Reg. */
#define C80_SDTR                6       /* wo - Start DMA Target Receive Reg. */
#define C80_RPIR                7       /* ro - Reset Parity/Interrupt Reg. */
#define C80_SDIR                7       /* wo - Start DMA Initiator Receive Reg. */

#endif /* _IC_NCR_5380_H_ */
