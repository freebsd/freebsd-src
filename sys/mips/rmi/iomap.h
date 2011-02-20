/*-
 * Copyright (c) 2003-2009 RMI Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of RMI Corporation, nor the names of its contributors,
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
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
 *
 * RMI_BSD
 * $FreeBSD$
 */
#ifndef _RMI_IOMAP_H_
#define _RMI_IOMAP_H_

#include <machine/endian.h>
#define XLR_DEVICE_REGISTER_BASE	0x1EF00000
#define DEFAULT_XLR_IO_BASE 0xffffffffbef00000ULL
#define XLR_IO_SIZE                   0x1000

#define XLR_IO_BRIDGE_OFFSET          0x00000

#define XLR_IO_DDR2_CHN0_OFFSET       0x01000
#define XLR_IO_DDR2_CHN1_OFFSET       0x02000
#define XLR_IO_DDR2_CHN2_OFFSET       0x03000
#define XLR_IO_DDR2_CHN3_OFFSET       0x04000

#define XLR_IO_RLD2_CHN0_OFFSET       0x05000
#define XLR_IO_RLD2_CHN1_OFFSET       0x06000

#define XLR_IO_SRAM_OFFSET            0x07000

#define XLR_IO_PIC_OFFSET             0x08000
#define XLR_IO_PCIX_OFFSET            0x09000
#define XLR_IO_HT_OFFSET              0x0A000

#define XLR_IO_SECURITY_OFFSET        0x0B000

#define XLR_IO_GMAC_0_OFFSET          0x0C000
#define XLR_IO_GMAC_1_OFFSET          0x0D000
#define XLR_IO_GMAC_2_OFFSET          0x0E000
#define XLR_IO_GMAC_3_OFFSET          0x0F000

#define XLR_IO_SPI4_0_OFFSET          0x10000
#define XLR_IO_XGMAC_0_OFFSET         0x11000
#define XLR_IO_SPI4_1_OFFSET          0x12000
#define XLR_IO_XGMAC_1_OFFSET         0x13000

#define XLR_IO_UART_0_OFFSET          0x14000
#define XLR_IO_UART_1_OFFSET          0x15000
#define XLR_UART0ADDR                 (XLR_IO_UART_0_OFFSET+XLR_DEVICE_REGISTER_BASE)



#define XLR_IO_I2C_0_OFFSET           0x16000
#define XLR_IO_I2C_1_OFFSET           0x17000

#define XLR_IO_GPIO_OFFSET            0x18000

#define XLR_IO_FLASH_OFFSET           0x19000

#define XLR_IO_TB_OFFSET           	  0x1C000

#define XLR_IO_GMAC_4_OFFSET          0x20000
#define XLR_IO_GMAC_5_OFFSET          0x21000
#define XLR_IO_GMAC_6_OFFSET          0x22000
#define XLR_IO_GMAC_7_OFFSET          0x23000

#define XLR_IO_PCIE_0_OFFSET          0x1E000
#define XLR_IO_PCIE_1_OFFSET          0x1F000

#define XLR_IO_USB_0_OFFSET           0x24000
#define XLR_IO_USB_1_OFFSET           0x25000

#define XLR_IO_COMP_OFFSET            0x1d000

/* Base Address (Virtual) of the PCI Config address space
 * For now, choose 256M phys in kseg1 = 0xA0000000 + (1<<28)
 * Config space spans 256 (num of buses) * 256 (num functions) * 256 bytes
 * ie 1<<24 = 16M
 */
#define DEFAULT_PCI_CONFIG_BASE         0x18000000
#define DEFAULT_HT_TYPE0_CFG_BASE       0x16000000
#define DEFAULT_HT_TYPE1_CFG_BASE       0x17000000

typedef volatile __uint32_t xlr_reg_t;
extern unsigned long xlr_io_base;

#define xlr_io_mmio(offset) ((xlr_reg_t *)(xlr_io_base+(offset)))

#define xlr_read_reg(base, offset) (__ntohl((base)[(offset)]))
#define xlr_write_reg(base, offset, value) ((base)[(offset)] = __htonl((value)))

extern void on_chip_init(void);

#endif				/* _RMI_IOMAP_H_ */
