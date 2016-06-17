/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *
 * Copyright (C) 2002 Shane Nay (shane@minirl.com)
 *
 */

/**************************************************************************
 * * Copyright © ARM Limited 1998.  All rights reserved.
 * ***********************************************************************/
/* ************************************************************************
 *
 *   MX1 address map
 *
 * ***********************************************************************/


/* ========================================================================
 *  MX1 definitions
 * ========================================================================
 * ------------------------------------------------------------------------
 *  Memory definitions
 * ------------------------------------------------------------------------
 *  MX1 memory map
 */


#ifndef __MX1ADS_PLATFORM_H__
#define __MX1ADS_PLATFORM_H__


#define MX1ADS_SRAM_BASE           0x00300000
#define MX1ADS_SRAM_SIZE           SZ_128K

#define MX1ADS_SFLASH_BASE         0x0C000000
#define MX1ADS_SFLASH_SIZE         SZ_16M

#define MX1ADS_IO_BASE             0x00200000
#define MX1ADS_IO_SIZE             SZ_256K

#define MX1ADS_VID_BASE            0x00300000
#define MX1ADS_VID_SIZE            0x26000

#define MX1ADS_VID_START           IO_ADDRESS(MX1ADS_VID_BASE)


/* ------------------------------------------------------------------------
 *  Motorola MX1 system registers
 * ------------------------------------------------------------------------
 *
 */

/*
 *  Register offests.
 *
 */

#define MX1ADS_AIPI1_OFFSET             0x00000
#define MX1ADS_WDT_OFFSET               0x01000
#define MX1ADS_TIM1_OFFSET              0x02000
#define MX1ADS_TIM2_OFFSET              0x03000
#define MX1ADS_RTC_OFFSET               0x04000
#define MX1ADS_LCDC_OFFSET              0x05000
#define MX1ADS_UART1_OFFSET             0x06000
#define MX1ADS_UART2_OFFSET             0x07000
#define MX1ADS_PWM_OFFSET               0x08000
#define MX1ADS_DMAC_OFFSET              0x09000
#define MX1ADS_AIPI2_OFFSET             0x10000
#define MX1ADS_SIM_OFFSET               0x11000
#define MX1ADS_USBD_OFFSET              0x12000
#define MX1ADS_SPI1_OFFSET              0x13000
#define MX1ADS_MMC_OFFSET               0x14000
#define MX1ADS_ASP_OFFSET               0x15000
#define MX1ADS_BTA_OFFSET               0x16000
#define MX1ADS_I2C_OFFSET               0x17000
#define MX1ADS_SSI_OFFSET               0x18000
#define MX1ADS_SPI2_OFFSET              0x19000
#define MX1ADS_MSHC_OFFSET              0x1A000
#define MX1ADS_PLL_OFFSET               0x1B000
#define MX1ADS_GPIO_OFFSET              0x1C000
#define MX1ADS_EIM_OFFSET               0x20000
#define MX1ADS_SDRAMC_OFFSET            0x21000
#define MX1ADS_MMA_OFFSET               0x22000
#define MX1ADS_AITC_OFFSET              0x23000
#define MX1ADS_CSI_OFFSET               0x24000


/*
 *  Register BASEs, based on OFFSETs
 *
 */

#define MX1ADS_AIPI1_BASE             (MX1ADS_AIPI1_OFFSET + MX1ADS_IO_BASE)
#define MX1ADS_WDT_BASE               (MX1ADS_WDT_OFFSET + MX1ADS_IO_BASE)
#define MX1ADS_TIM1_BASE              (MX1ADS_TIM1_OFFSET + MX1ADS_IO_BASE)
#define MX1ADS_TIM2_BASE              (MX1ADS_TIM2_OFFSET + MX1ADS_IO_BASE)
#define MX1ADS_RTC_BASE               (MX1ADS_RTC_OFFSET + MX1ADS_IO_BASE)
#define MX1ADS_LCDC_BASE              (MX1ADS_LCDC_OFFSET + MX1ADS_IO_BASE)
#define MX1ADS_UART1_BASE             (MX1ADS_UART1_OFFSET + MX1ADS_IO_BASE)
#define MX1ADS_UART2_BASE             (MX1ADS_UART2_OFFSET + MX1ADS_IO_BASE)
#define MX1ADS_PWM_BASE               (MX1ADS_PWM_OFFSET + MX1ADS_IO_BASE)
#define MX1ADS_DMAC_BASE              (MX1ADS_DMAC_OFFSET + MX1ADS_IO_BASE)
#define MX1ADS_AIPI2_BASE             (MX1ADS_AIPI2_OFFSET + MX1ADS_IO_BASE)
#define MX1ADS_SIM_BASE               (MX1ADS_SIM_OFFSET + MX1ADS_IO_BASE)
#define MX1ADS_USBD_BASE              (MX1ADS_USBD_OFFSET + MX1ADS_IO_BASE)
#define MX1ADS_SPI1_BASE              (MX1ADS_SPI1_OFFSET + MX1ADS_IO_BASE)
#define MX1ADS_MMC_BASE               (MX1ADS_MMC_OFFSET + MX1ADS_IO_BASE)
#define MX1ADS_ASP_BASE               (MX1ADS_ASP_OFFSET + MX1ADS_IO_BASE)
#define MX1ADS_BTA_BASE               (MX1ADS_BTA_OFFSET + MX1ADS_IO_BASE)
#define MX1ADS_I2C_BASE               (MX1ADS_I2C_OFFSET + MX1ADS_IO_BASE)
#define MX1ADS_SSI_BASE               (MX1ADS_SSI_OFFSET + MX1ADS_IO_BASE)
#define MX1ADS_SPI2_BASE              (MX1ADS_SPI2_OFFSET + MX1ADS_IO_BASE)
#define MX1ADS_MSHC_BASE              (MX1ADS_MSHC_OFFSET + MX1ADS_IO_BASE)
#define MX1ADS_PLL_BASE               (MX1ADS_PLL_OFFSET + MX1ADS_IO_BASE)
#define MX1ADS_GPIO_BASE              (MX1ADS_GPIO_OFFSET + MX1ADS_IO_BASE)
#define MX1ADS_EIM_BASE               (MX1ADS_EIM_OFFSET + MX1ADS_IO_BASE)
#define MX1ADS_SDRAMC_BASE            (MX1ADS_SDRAMC_OFFSET + MX1ADS_IO_BASE)
#define MX1ADS_MMA_BASE               (MX1ADS_MMA_OFFSET + MX1ADS_IO_BASE)
#define MX1ADS_AITC_BASE              (MX1ADS_AITC_OFFSET + MX1ADS_IO_BASE)
#define MX1ADS_CSI_BASE               (MX1ADS_CSI_OFFSET + MX1ADS_IO_BASE)


/*
 *  MX1 Interrupt numbers
 *
 */
#define INT_SOFTINT                 0
#define CSI_INT                     6
#define DSPA_MAC_INT                7
#define DSPA_INT                    8
#define COMP_INT                    9
#define MSHC_XINT                   10
#define GPIO_INT_PORTA              11
#define GPIO_INT_PORTB              12
#define GPIO_INT_PORTC              13
#define LCDC_INT                    14
#define SIM_INT                     15
#define SIM_DATA_INT                16
#define RTC_INT                     17
#define RTC_SAMINT                  18
#define UART2_MINT_PFERR            19
#define UART2_MINT_RTS              20
#define UART2_MINT_DTR              21
#define UART2_MINT_UARTC            22
#define UART2_MINT_TX               23
#define UART2_MINT_RX               24
#define UART1_MINT_PFERR            25
#define UART1_MINT_RTS              26
#define UART1_MINT_DTR              27
#define UART1_MINT_UARTC            28
#define UART1_MINT_TX               29
#define UART1_MINT_RX               30
#define VOICE_DAC_INT               31
#define VOICE_ADC_INT               32
#define PEN_DATA_INT                33
#define PWM_INT                     34
#define SDHC_INT                    35
#define I2C_INT                     39
#define CSPI_INT                    41
#define SSI_TX_INT                  42
#define SSI_TX_ERR_INT              43
#define SSI_RX_INT                  44
#define SSI_RX_ERR_INT              45
#define TOUCH_INT                   46
#define USBD_INT0                   47
#define USBD_INT1                   48
#define USBD_INT2                   49
#define USBD_INT3                   50
#define USBD_INT4                   51
#define USBD_INT5                   52
#define USBD_INT6                   53
#define BTSYS_INT                   55
#define BTTIM_INT                   56
#define BTWUI_INT                   57
#define TIM2_INT                    58
#define TIM1_INT                    59
#define DMA_ERR                     60
#define DMA_INT                     61
#define GPIO_INT_PORTD              62




#define MAXIRQNUM                       62
#define MAXFIQNUM                       62
#define MAXSWINUM                       62





#define TICKS_PER_uSEC                  24

/*
 *  These are useconds NOT ticks.
 *
 */
#define mSEC_1                          1000
#define mSEC_5                          (mSEC_1 * 5)
#define mSEC_10                         (mSEC_1 * 10)
#define mSEC_25                         (mSEC_1 * 25)
#define SEC_1                           (mSEC_1 * 1000)




/* 	END */
#endif
