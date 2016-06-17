/*
 * vac.h: Various VIC controller defines.  The VIC is a VME controller
 *        used in Baget/MIPS series.
 *
 * Copyright (C) 1998 Gleb Raiko & Vladimir Roganov
 */
#ifndef _ASM_VAC_H
#define _ASM_VAC_H

#define VAC_SLSEL1_MASK      0x000
#define VAC_SLSEL1_BASE      0x100
#define VAC_SLSEL0_MASK      0x200
#define VAC_SLSEL0_BASE      0x300
#define VAC_ICFSEL_BASE      0x400
#define VAC_ICFSEL_GLOBAL_VAL(x) (((x)>>8)&0xff)
#define VAC_ICFSEL_MODULE_VAL(x) ((x)&0xff)
#define VAC_DRAM_MASK        0x500
#define VAC_BNDR2            0x600
#define VAC_BNDR3            0x700
#define VAC_A24_BASE         0x800
#define    VAC_A24_MASK          (0x3f<<9)
#define    VAC_A24_D32_ENABLE    (1<<8)
#define    VAC_A24_A24_CACHINH   (1<<7)
#define    VAC_A24_A16D32_ENABLE (1<<6)
#define    VAC_A24_A16D32        (1<<5)
#define    VAC_A24_DATAPATH      (1<<4)
#define    VAC_A24_IO_CACHINH    (1<<3)
#define VAC_REG1             0x900
#define VAC_REG2             0xA00
#define VAC_REG3             0xB00
#define    VAC_REG_WORD      (1<<15)
#define    VAC_REG_ASIZ1     (1<<14)
#define    VAC_REG_ASIZ0     (1<<13)
#define    VAC_REG_ASIZ_VAL(x) (((x)>>13)&3)
#define    VAC_REG_CACHINH   (1<<12)
#define    VAC_REG_INACTIVE  (0<<10)
#define    VAC_REG_SHARED    (1<<10)
#define    VAC_REG_VSB       (2<<10)
#define    VAC_REG_MWB       (3<<10)
#define    VAC_REG_MASK      (3<<10)
#define    VAC_REG_MODE(x)   (((x)>>10)&3)
#define VAC_IOSEL4_CTRL      0xC00
#define VAC_IOSEL5_CTRL      0xD00
#define VAC_SHRCS_CTRL       0xE00
#define VAC_EPROMCS_CTRL     0xF00
#define VAC_IOSEL0_CTRL      0x1000
#define VAC_IOSEL1_CTRL      0x1100
#define VAC_IOSEL2_CTRL      0x1200
#define VAC_IOSEL3_CTRL      0x1300
#define    VAC_CTRL_IOWR               (1<<0)
#define    VAC_CTRL_IORD               (1<<1)
#define    VAC_CTRL_DELAY_IOSELI(x)    (((x)&3)<<2)
#define    VAC_CTRL_DELAY_IOSELI_VAL(x) (((x)>>2)&3)
#define    VAC_CTRL_DELAY_IOWR(x)      (((x)&3)<<4)
#define    VAC_CTRL_DELAY_IOWR_VAL(x)  (((x)>>4)&3)
#define    VAC_CTRL_DELAY_IORD(x)      (((x)&3)<<6)
#define    VAC_CTRL_DELAY_IORD_VAL(x)  (((x)>>6)&3)
#define    VAC_CTRL_RECOVERY_IOSELI(x) ((((x)-1)&7)<<8)
#define    VAC_CTRL_RECOVERY_IOSELI_VAL(x) ((((x)>>8)&7)+1)
#define    VAC_CTRL_DSACK0             (1<<11)
#define    VAC_CTRL_DSACK1             (1<<12)
#define    VAC_CTRL_DELAY_DSACKI(x)    ((((x)-1)&7)<<13)
#define    VAC_CTRL_DELAY_DSACKI_VAL(x) ((((x)>>13)&7)+1)
#define VAC_DECODE_CTRL      0x1400
#define    VAC_DECODE_FPUCS   (1<<0)
#define    VAC_DECODE_CPUCLK(x)  (((x)&3)<<1)
#define    VAC_DECODE_CPUCLK_VAL(x) (((x)>>1)&3)
#define    VAC_DECODE_RDR_SLSEL0 (1<<3)
#define    VAC_DECODE_RDR_SLSEL1 (1<<4)
#define    VAC_DECODE_DSACK   (1<<5)
#define    VAC_DECODE_QFY_BNDR    (1<<6)
#define    VAC_DECODE_QFY_ICFSEL  (1<<7)
#define    VAC_DECODE_QFY_SLSEL1  (1<<8)
#define    VAC_DECODE_QFY_SLSEL0  (1<<9)
#define    VAC_DECODE_CMP_SLSEL1_LO  (1<<10)
#define    VAC_DECODE_CMP_SLSEL1_HI  (1<<11)
#define    VAC_DECODE_CMP_SLSEL1_VAL(x) (((x)>>10)&3)
#define    VAC_DECODE_DRAMCS  (3<<12)
#define    VAC_DECODE_SHRCS   (2<<12)
#define    VAC_DECODE_VSBSEL  (1<<12)
#define    VAC_DECODE_EPROMCS (0<<12)
#define    VAC_DECODE_MODE_VAL(x) (((x)>>12)&3)
#define    VAC_DECODE_QFY_DRAMCS  (1<<14)
#define    VAC_DECODE_DSACKI  (1<<15)
#define VAC_INT_STATUS       0x1500
#define VAC_INT_CTRL         0x1600
#define    VAC_INT_CTRL_TIMER_PIO11    (3<<0)
#define    VAC_INT_CTRL_TIMER_PIO10    (2<<0)
#define    VAC_INT_CTRL_TIMER_PIO7     (1<<0)
#define    VAC_INT_CTRL_TIMER_DISABLE  (0<<0)
#define    VAC_INT_CTRL_TIMER_MASK     (3<<0)
#define    VAC_INT_CTRL_UART_B_PIO11   (3<<2)
#define    VAC_INT_CTRL_UART_B_PIO10   (2<<2)
#define    VAC_INT_CTRL_UART_B_PIO7    (1<<2)
#define    VAC_INT_CTRL_UART_B_DISABLE (0<<2)
#define    VAC_INT_CTRL_UART_A_PIO11   (3<<4)
#define    VAC_INT_CTRL_UART_A_PIO10   (2<<4)
#define    VAC_INT_CTRL_UART_A_PIO7    (1<<4)
#define    VAC_INT_CTRL_UART_A_DISABLE (0<<4)
#define    VAC_INT_CTRL_MBOX_PIO11     (3<<6)
#define    VAC_INT_CTRL_MBOX_PIO10     (2<<6)
#define    VAC_INT_CTRL_MBOX_PIO7      (1<<6)
#define    VAC_INT_CTRL_MBOX_DISABLE   (0<<6)
#define    VAC_INT_CTRL_PIO4_PIO11     (3<<8)
#define    VAC_INT_CTRL_PIO4_PIO10     (2<<8)
#define    VAC_INT_CTRL_PIO4_PIO7      (1<<8)
#define    VAC_INT_CTRL_PIO4_DISABLE   (0<<8)
#define    VAC_INT_CTRL_PIO7_PIO11     (3<<10)
#define    VAC_INT_CTRL_PIO7_PIO10     (2<<10)
#define    VAC_INT_CTRL_PIO7_PIO7      (1<<10)
#define    VAC_INT_CTRL_PIO7_DISABLE   (0<<10)
#define    VAC_INT_CTRL_PIO8_PIO11     (3<<12)
#define    VAC_INT_CTRL_PIO8_PIO10     (2<<12)
#define    VAC_INT_CTRL_PIO8_PIO7      (1<<12)
#define    VAC_INT_CTRL_PIO8_DISABLE   (0<<12)
#define    VAC_INT_CTRL_PIO9_PIO11     (3<<14)
#define    VAC_INT_CTRL_PIO9_PIO10     (2<<14)
#define    VAC_INT_CTRL_PIO9_PIO7      (1<<14)
#define    VAC_INT_CTRL_PIO9_DISABLE   (0<<14)
#define VAC_DEV_LOC          0x1700
#define    VAC_DEV_LOC_IOSEL(x)   (1<<(x))
#define VAC_PIO_DATA_OUT     0x1800
#define VAC_PIO_PIN          0x1900
#define VAC_PIO_DIRECTION    0x1A00
#define    VAC_PIO_DIR_OUT(x)     (1<<(x))
#define    VAC_PIO_DIR_IN(x)      (0<<(x))
#define    VAC_PIO_DIR_FCIACK     (1<<14)
#define VAC_PIO_FUNC         0x1B00
#define    VAC_PIO_FUNC_UART_A_TX (1<<0)
#define    VAC_PIO_FUNC_UART_A_RX (1<<1)
#define    VAC_PIO_FUNC_UART_B_TX (1<<2)
#define    VAC_PIO_FUNC_UART_B_RX (1<<3)
#define    VAC_PIO_FUNC_IORD      (1<<4)
#define    VAC_PIO_FUNC_IOWR      (1<<5)
#define    VAC_PIO_FUNC_IOSEL3    (1<<6)
#define    VAC_PIO_FUNC_IRQ7      (1<<7)
#define    VAC_PIO_FUNC_IOSEL4    (1<<8)
#define    VAC_PIO_FUNC_IOSEL5    (1<<9)
#define    VAC_PIO_FUNC_IRQ10     (1<<10)
#define    VAC_PIO_FUNC_IRQ11     (1<<11)
#define    VAC_PIO_FUNC_OUT       (1<<12)
#define    VAC_PIO_FUNC_IOSEL2    (1<<13)
#define    VAC_PIO_FUNC_DELAY     (1<<14)
#define    VAC_PIO_FUNC_FCIACK    (1<<15)
#define VAC_CPU_CLK_DIV      0x1C00
#define VAC_UART_A_MODE      0x1D00
#define    VAC_UART_MODE_PARITY_ENABLE  (1<<15) /* Inversed in manual ? */
#define    VAC_UART_MODE_PARITY_ODD     (1<<14) /* Inversed in manual ? */
#define    VAC_UART_MODE_8BIT_CHAR      (1<<13)
#define    VAC_UART_MODE_BAUD(x)        (((x)&7)<<10)
#define    VAC_UART_MODE_CHAR_RX_ENABLE (1<<9)
#define    VAC_UART_MODE_CHAR_TX_ENABLE (1<<8)
#define    VAC_UART_MODE_TX_ENABLE      (1<<7)
#define    VAC_UART_MODE_RX_ENABLE      (1<<6)
#define    VAC_UART_MODE_SEND_BREAK     (1<<5)
#define    VAC_UART_MODE_LOOPBACK       (1<<4)
#define    VAC_UART_MODE_INITIAL        (VAC_UART_MODE_8BIT_CHAR | \
                                         VAC_UART_MODE_TX_ENABLE | \
                                         VAC_UART_MODE_RX_ENABLE | \
                                         VAC_UART_MODE_CHAR_TX_ENABLE | \
                                         VAC_UART_MODE_CHAR_RX_ENABLE | \
                                         VAC_UART_MODE_BAUD(5)) /* 9600/4 */
#define VAC_UART_A_TX        0x1E00
#define VAC_UART_B_MODE      0x1F00
#define VAC_UART_A_RX        0x2000
#define    VAC_UART_RX_ERR_BREAK        (1<<10)
#define    VAC_UART_RX_ERR_FRAME        (1<<9)
#define    VAC_UART_RX_ERR_PARITY       (1<<8)
#define    VAC_UART_RX_DATA_MASK        (0xff)
#define VAC_UART_B_RX        0x2100
#define VAC_UART_B_TX        0x2200
#define VAC_UART_A_INT_MASK  0x2300
#define    VAC_UART_INT_RX_READY        (1<<15)
#define    VAC_UART_INT_RX_FULL         (1<<14)
#define    VAC_UART_INT_RX_BREAK_CHANGE (1<<13)
#define    VAC_UART_INT_RX_ERRS         (1<<12)
#define    VAC_UART_INT_TX_READY        (1<<11)
#define    VAC_UART_INT_TX_EMPTY        (1<<10)
#define VAC_UART_B_INT_MASK  0x2400
#define VAC_UART_A_INT_STATUS  0x2500
#define    VAC_UART_STATUS_RX_READY        (1<<15)
#define    VAC_UART_STATUS_RX_FULL         (1<<14)
#define    VAC_UART_STATUS_RX_BREAK_CHANGE (1<<13)
#define    VAC_UART_STATUS_RX_ERR_PARITY   (1<<12)
#define    VAC_UART_STATUS_RX_ERR_FRAME    (1<<11)
#define    VAC_UART_STATUS_RX_ERR_OVERRUN  (1<<10)
#define    VAC_UART_STATUS_TX_READY        (1<<9)
#define    VAC_UART_STATUS_TX_EMPTY        (1<<8)
#define    VAC_UART_STATUS_INTS            (0xff<<8)
#define VAC_UART_B_INT_STATUS  0x2600
#define VAC_TIMER_DATA       0x2700
#define VAC_TIMER_CTRL       0x2800
#define    VAC_TIMER_ONCE      (1<<15)
#define    VAC_TIMER_ENABLE    (1<<14)
#define    VAC_TIMER_PRESCALE(x) (((x)&0x3F)<<8)
#define VAC_ID               0x2900


#ifndef __ASSEMBLY__

#define vac_inb(p)    (*(volatile unsigned char *)(VAC_BASE + (p)))
#define vac_outb(v,p) (*((volatile unsigned char *)(VAC_BASE + (p))) = v)
#define vac_inw(p)    (*(volatile unsigned short*)(VAC_BASE + (p)))
#define vac_outw(v,p) (*((volatile unsigned short*)(VAC_BASE + (p))) = v)

#endif /* !__ASSEMBLY__ */

#endif /* _ASM_VAC_H */
