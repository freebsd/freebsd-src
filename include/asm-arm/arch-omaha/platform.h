
/* DO NOT EDIT!! - this file automatically generated
 *                 from .s file by awk -f s2h.awk
 */
/***********************************************************************
 *    Copyright ARM Limited 1998 - 2001.  All rights reserved.
 * ************************************************************************
 * 
 *   Omaha address map
 * 
 * 	NOTE: This is a multi-hosted header file for use with uHAL and
 * 	      supported debuggers.
 * 
 * 	$Id: platform.h,v 1.12 2002/08/22 15:49:56 ahaigh Exp $
 * 
 *     Everything is defined in terms of PLAT_* and then the key values
 *     are redefined as required for uHAL.
 * 
 *     NOTE: If things are defined in terms of a BASE and lots of OFFSETs,
 * 	the code will be more portable and amenable to being used in
 * 	different (physical vs virtual) memory maps - just change the
 * 	base and the rest will work!
 * 
 * ***********************************************************************/

#ifndef __address_h
#define __address_h                     1

#include "bits.h" 
	
#define PLATFORM_ID                     0x00000800	 /*  TBD */

/*  Common modules for uHAL can be included or excluded by changing these
 *  definitions. These can be over-ridden by the makefile/ARM project file
 *  provided the .h file is rebuilt.
 */

#ifndef USE_C_LIBRARY
#define USE_C_LIBRARY                   0
#endif
#ifndef uHAL_HEAP
#define uHAL_HEAP                       1
#endif
#ifndef uHAL_PCI
#define uHAL_PCI                        0
#endif


/* -----------------------------------------------------------------------
 * 
 *  uHAL always has RAM at 0 and, when a MMU is used, maps everything else
 *  1-1 physical to virtual. 
 */

/* =======================================================================
 *  Omaha
 * =======================================================================
 * -----------------------------------------------------------------------
 *  Memory definitions
 * -----------------------------------------------------------------------
 */

/*  New world memory....
 */

/*  We have eight banks of 32MB, covering the first 256Mb of address space
 */

/*  Bank		Function
 *  0		Flash
 *  1		TBD / ROM
 *  2		TBD / ROM
 *  3		FPGA / sub-chip-selects
 *  4		TBD / ROM
 *  5		TBD / ROM
 *  6		SDRAM Bank 0
 *  7		SDRAM Bank 1
 */


/*  SDRAM	: 64Mbytes. 
 * 
 *  Two banks of 32-bit SDRAM, each 32Mbytes in size.
 *  Bank 0 is on nGCS6, bank 1 is on nGCS7
 */

/*  Physical address
 */
#define PLAT_SDRAM_PHYS0                0x0C000000	 /*  @ 192Mb */
#define PLAT_SDRAM_PHYS1                0x0E000000	 /*  @ 224Mb */

/*  Size of one SDRAM bank
 */
#define PLAT_SDRAM_BANKSIZE             SZ_32M

/*  Virtual address
 */
#define PLAT_SDRAM_BASE                 0x00000000
#define PLAT_SDRAM_SIZE                 SZ_64M

/*  Put page tables in top 1MB of memory; don't let the user access this area.
 */
#define PLAT_PAGETABLE_BASE             0x0FFF0000
#define PLAT_USER_SDRAM_SIZE            PLAT_SDRAM_SIZE - SZ_1M

/*  Flash : 0.5MBytes
 * 
 *  One bank of 8-bit Flash on nGCS0 (SST39VF040-90-4C-NH)
 */

/*  Physical flash device base address
 */
#define PLAT_FLASH_PHYS                 0x00000000	 /*  nCS0 */

/*  Logical address for flash
 */
#define PLAT_FLASH_BASE                 SZ_64M
#define PLAT_FLASH_UNCACHED             SZ_128M
#define PLAT_FLASH_DEVICE_SIZE          SZ_512K		
#define PLAT_FLASH_SIZE                 SZ_512K

/*  Notes
 *  We do not map physical devices at their physical
 *  addresses, because this would overlap the 64Mb of RAM
 *  at the bottom of memory. Instead, they are mostly mapped
 *  at 0x20000000 + Physical address instead
 *  (CPU internal registers live at 0x10000000 + something)
 */

/*  FPGA space (all of nGCS 3)
 */
#define PLAT_FPGA_PHYS                  0x06000000
#define PLAT_FPGA_BASE                  0x26000000
#define PLAT_FPGA_SIZE                  SZ_32M	

/*  PLD live in PLD space in nGCS1
 */
#define PLAT_PLD_PHYS                   0x02800000
#define PLAT_PLD_BASE                   0x22800000
#define PLAT_PLD_SIZE                   SZ_8M

/*  USB2 space
 */
#define PLAT_USB2_PHYS                  0x04000000
#define PLAT_USB2_BASE                  0x24000000
#define PLAT_USB2_SIZE                  0x00200000
			       
/*  Ethernet space
 */
#define PLAT_ETHERNET_PHYS              0x02200000
#define PLAT_ETHERNET_BASE              0x22200000
#define PLAT_ETHERNET_SIZE              0x00200000

/*  TAP Controller space
 */
#define PLAT_TAP_PHYS                   PLAT_FPGA_PHYS
#define PLAT_TAP_BASE                   PLAT_FPGA_BASE
#define PLAT_TAP_SIZE                   SZ_32M

/*  CompactFlash address space (top half on nGCS1)
 */

/*  Notes about the CompactFlash implementation (rev b):
 * 
 *  Since we only need CF ATA cards to work, we
 *  use TrueIDE mode for simplicity.
 * 
 *  However, we need 8-bit access to the Command
 *  Registers, and 16-bit access to the Data-Register.
 *  So we have to access the CF card in two chip select
 *  regions: nCS0 and nCS2.
 * 
 *  In the case of TrueIDE mode, we get the following
 *  (physical) address usage:
 * 
 *  0x01C00000 - 0x01C00007 	- Command Registers (8-bit r/w)
 *  0x01E00006 - 0x01E00006	- Control Register (8-bit r/w)
 *  0x05C00000 - 0x05C00001	- Data Register (16-bit r/w)
 *  
 */

/*  CF 16-bit access in nCS2
 */
#define PLAT_PCMCIA_PHYS                0x05000000
#define PLAT_PCMCIA_BASE                0x25000000
#define PLAT_PCMCIA_SIZE                0x01000000

/*  CF 8-bit access in nCS0
 */
#define PLAT_PCMCIA_8_PHYS              0x01000000
#define PLAT_PCMCIA_8_BASE              0x21000000
#define PLAT_PCMCIA_8_SIZE              0x01000000

/*  Offsets into CF region (applies to both CS regions)
 */
#define PLAT_PCMCIA_ATTR                0x00800000
#define PLAT_PCMCIA_MEM                 0x00A00000
#define PLAT_PCMCIA_IO                  0x00C00000	 /*  CE1 in TrueIDE */
#define PLAT_PCMCIA_IO_ALT              0x00E00000	 /*  CE2 in TrueIDE */

/*  Expansion Bus memory region (nGCS4 and nGCS5)
 */
#define PLAT_BUS_PHYS                   0x08000000
#define PLAT_BUS_BASE                   0x28000000
#define PLAT_BUS_SIZE                   0x04000000

/*  Location of high vectors used by operating system
 *  If we may this, then we can seamlessly debug
 *  from a uHAL program into an OS using high-vectors,
 *  since we are always going to need exception handlers,
 *  and they can only be in two places (0x0 and 0xFFFF0000)!
 */
#define PLAT_HIVECS_PHYS                0xFFFF0000
#define PLAT_HIVECS_BASE                0xFFFF0000
#define PLAT_HIVECS_SIZE                SZ_1M

/*  Remaining chip-select regions
 *  Leave these un-mapped for the moment
 */


/*  ----------------------------------------------------------------------
 *  S3C2400X01 CPU peripherals
 * -----------------------------------------------------------------------
 */

/*  AHB peripherals appear in the lower 16Mb, APB in the upper 16Mb
 */

/*  All peripherals are offsets from PLAT_PERIPHERAL_BASE
 */

#define PLAT_PERIPHERAL_BASE            0x14000000
#define PLAT_PERIPHERAL_SIZE            SZ_32M

/*  Memory Controller
 */
#define OMAHA_BWSCON                    0x00	 /*  Bus width and wait state control */
#define OMAHA_BANKCON0                  0x04	 /*  Boot ROM control (Bank 0) */
#define OMAHA_BANKCON1                  0x08	 /*  Bank 1 control */
#define OMAHA_BANKCON2                  0x0C	 /*  Bank 2 control */
#define OMAHA_BANKCON3                  0x10	 /*  Bank 3 control */
#define OMAHA_BANKCON4                  0x14	 /*  Bank 4 control */
#define OMAHA_BANKCON5                  0x18	 /*  Bank 5 control */
#define OMAHA_BANKCON6                  0x1C	 /*  Bank 6 control */
#define OMAHA_BANKCON7                  0x20	 /*  Bank 7 control */

#define OMAHA_REFRESH                   0x24	 /*  SDRAM refresh control */
#define OMAHA_BANKSIZE                  0x28	 /*  Flexible bank size */
#define OMAHA_MRSRB6                    0x2C	 /*  Mode register set for SDRAM Bank 0 (nGCS6) */
#define OMAHA_MRSRB7                    0x30	 /*  Mode register set for SDRAM Bank 1 (nGCS7) */

/*  Interrupt controller
 */
#define OMAHA_SRCPND                    0x400000	 /*  Interrupt sources pending */
#define OMAHA_INTMOD                    0x400004	 /*  Interrupt mode control */
#define OMAHA_INTMSK                    0x400008	 /*  Interrupt mask control */
#define OMAHA_PRIORITY                  0x40000C	 /*  Int. priority control */
#define OMAHA_INTPND                    0x400010	 /*  Interrupts pending */
#define OMAHA_INTOFFSET                 0x400014	 /*  IRQ source */

/*  Clock / Power management
 */
#define OMAHA_LOCKTIME                  0x800000	 /*  PLL Lock time counter */
#define OMAHA_MPLLCON                   0x800004	 /*  MPLL Control */
#define OMAHA_UPLLCON                   0x800008	 /*  UPLL Control */
#define OMAHA_CLKCON                    0x80000C	 /*  Clock control */
#define OMAHA_CLKSLOW                   0x800010	 /*  Slow clock control */
#define OMAHA_CLKDIVN                   0x800014	 /*  Clock divider control */

/*  LCD Control
 */

/*  UARTs
 */
#define OMAHA_ULCON0                    0x1000000	 /*  UART 0 Line control */
#define OMAHA_ULCON1                    0x1004000	 /*  UART 1 Line control */

/*  PWM Timers
 */
#define OMAHA_TCFG0                     0x1100000	 /*  Timer 0 config */
#define OMAHA_TCFG1                     0x1100004	 /*  Timer 1 config */
#define OMAHA_TCON                      0x1100008	 /*  Timer control */
#define OMAHA_TCNTB0                    0x110000C	 /*  Timer count buffer 0 */
#define OMAHA_TCMPB0                    0x1100010	 /*  Timer Compare buffer 0 */
#define OMAHA_TCNTO0                    0x1100014	 /*  Timer count observation 0 */
#define OMAHA_TCNTB1                    0x1100018	 /*  Timer count buffer 1 */
#define OMAHA_TCMPB1                    0x110001C	 /*  Timer compare buffer 1 */
#define OMAHA_TCNTO1                    0x1100020	 /*  Timer count observation 1 */
#define OMAHA_TCNTB2                    0x1100024	 /*  Timer count buffer 2 */
#define OMAHA_TCMPB2                    0x1100028	 /*  Timer compare buffer 2 */
#define OMAHA_TCNTO2                    0x110002C	 /*  Timer	count observation 2 */
#define OMAHA_TCNTB3                    0x1100030	 /*  Timer count buffer 3 */
#define OMAHA_TCMPB3                    0x1100034	 /*  Timer compare buffer 3 */
#define OMAHA_TCNTO3                    0x1100038	 /*  Timer	count observation 3 */
#define OMAHA_TCNTB4                    0x110003C	 /*  Timer count buffer 2 */
#define OMAHA_TCNTO4                    0x1100040	 /*  Timer	count observation 2 */

/*  DMA
 */
#define OMAHA_DMA_CON                   0x12001C0	 /*  DMA Interface control */
#define OMAHA_DMA_UNIT                  0x12001C4	 /*  DMA Transfer unit counter */
#define OMAHA_DMA_FIFO                  0x12001C8	 /*  DMA Transfer FIFO counter */
#define OMAHA_DMA_TX                    0x12001CC	 /*  DMA Total transfer counter */

/*  Watchdog
 */
#define OMAHA_WTCON                     0x1300000	 /*  Watchdog control register */
#define OMAHA_WTDAT                     0x1300004	 /*  Watchdog data */
#define OMAHA_WTCNT                     0x1300008	 /*  Watchdog count */
#define OMAHA_WT_DEF                    0x0		 /*  Disable the watchdog */

/*  IIC
 */
#define OMAHA_IICCON                    0x1400000	 /*  IIC Control */
#define OMAHA_IICSTAT                   0x1400004	 /*  IIC Status */
#define OMAHA_IICADD                    0x1400008	 /*  IIC address */
#define OMAHA_IICDS                     0x140000C	 /*  IIC Data shift */

/*  IIS
 */
#define OMAHA_IISCON                    0x1508000	 /*  IIS Control */
#define OMAHA_IISMOD                    0x1508004	 /*  IIS Mode */
#define OMAHA_IISPSR                    0x1508008	 /*  IIS Prescaler */
#define OMAHA_IISFIFOCON                0x150800C	 /*  IIS FIFO control */
#define OMAHA_IISFIF                    0x1508010	 /*  IIS Fifo entry */

/*  I/O Ports (GPIO's)
 */
#define OMAHA_PACON                     0x1600000	 /*  Port A control */
#define OMAHA_PADAT                     0x1600004	 /*  Port A data */
#define OMAHA_PBCON                     0x1600008	 /*  Port B control */
#define OMAHA_PBDAT                     0x160000C	 /*  Port B data */
#define OMAHA_PBUP                      0x1600010	 /*  Port B pull-up control */
#define OMAHA_PCCON                     0x1600014	 /*  Port C control */
#define OMAHA_PCDAT                     0x1600018	 /*  Port C data */
#define OMAHA_PCUP                      0x160001C	 /*  Port C pull-up */
#define OMAHA_PDCON                     0x1600020	 /*  Port D control */
#define OMAHA_PDDAT                     0x1600024	 /*  Port D data */
#define OMAHA_PDUP                      0x1600028	 /*  Port D pull-up */
#define OMAHA_PECON                     0x160002C	 /*  Port E control */
#define OMAHA_PEDAT                     0x1600030	 /*  Port E data */
#define OMAHA_PEUP                      0x1600034	 /*  Port E pull-up */
#define OMAHA_PFCON                     0x1600038	 /*  Port F control */
#define OMAHA_PFDAT                     0x160003C	 /*  Port F data */
#define OMAHA_PFUP                      0x1600040	 /*  Port F pull-up */
#define OMAHA_PGCON                     0x1600044	 /*  Port G control */
#define OMAHA_PGDAT                     0x1600048	 /*  Port G data */
#define OMAHA_PGUP                      0x160004C	 /*  Port G pull-up */
#define OMAHA_OPENCR                    0x1600050	 /*  Open Drain enable */
#define OMAHA_MISCCR                    0x1600054	 /*  Misc. control */
#define OMAHA_EXTINT                    0x1600058	 /*  External interrupt control */

/*  RTC
 */
#define OMAHA_RTCCON                    0x1700040	 /*  RTC Control */
#define OMAHA_TICINT                    0x1700044	 /*  Tick time count */
#define OMAHA_RTCALM                    0x1700050	 /*  RTC Alarm control */
#define OMAHA_ALMSEC                    0x1700054	 /*  Alarm Second */
#define OMAHA_ALMMIN                    0x1700058	 /*  Alarm Minute */
#define OMAHA_ALMHOUR                   0x170005C	 /*  Alarm Hour */
#define OMAHA_ALMDAY                    0x1700060	 /*  Alarm Day */
#define OMAHA_ALMMON                    0x1700064	 /*  Alarm Month */
#define OMAHA_ALMYEAR                   0x1700068	 /*  Alarm Year */
#define OMAHA_RTCRST                    0x170006C	 /*  RTC Round Reset */
#define OMAHA_BCDSEC                    0x1700070	 /*  BCD Second */
#define OMAHA_BCDMIN                    0x1700074	 /*  BCD Minute */
#define OMAHA_BCDHOUR                   0x1700078	 /*  BCD Hour */
#define OMAHA_BCDDAY                    0x170007C	 /*  BCD Day */
#define OMAHA_BCDDATE                   0x1700080	 /*  BCD Date */
#define OMAHA_BCDMON                    0x1700084	 /*  BCD Month */
#define OMAHA_BCDYEAR                   0x1700088	 /*  BCD Year */

/*  ADC
 */
#define OMAHA_ADCCON                    0x1800000	 /*  ADC control */
#define OMAHA_ADCDAT                    0x1800004	 /*  ADC data */

/*  SPI
 */
#define OMAHA_SPCON                     0x1900000	 /*  SPI Control */
#define OMAHA_SPSTA                     0x1900004	 /*  SPI status */
#define OMAHA_SPPIN                     0x1900008	 /*  SPI pin control */
#define OMAHA_SPPRE                     0x190000C	 /*  Baud rate prescaler */
#define OMAHA_SPTDAT                    0x1900010	 /*  SPI Tx data */
#define OMAHA_SPRDAT                    0x1900014	 /*  SPI Rx data */

/*  MMC
 */

/*  Memory timings
 */

/*  nGCS0: 8-bit r/o, no-wait. boot flash
 *  nGCS1: 32-bit, r/w, no-wait. PLD (inc. ethernet)
 *  nGCS2: 16-bit, r/w, wait. CompactFlash+USB2
 *  nGCS3: 32-bit, r/w, no-wait. FPGA
 *  nGCS4: 32-bit, r/w, no-wait. Expansion Bus
 *  nGCS5: 32-bit, r/w, no-wait. Expansion Bus
 *  nGCS6: 32-bit, r/w, no-wait. SDRAM bank 0
 *  nGCS7: 32-bit, r/w, no-wait. SDRAM bank 1
 */

#define OMAHA_BWSCON_DEF                0x222221A0	 /*  All 32-bit, r/w, no-wait */

/*  Functions:
 *  CS0	- Flash bank 0
 *  CS1	- PLD
 *  CS2	- FPGA
 *  CS3	- FPGA
 *  CS4	- PCMCIA0 (Unused)
 *  CS5	- PCMCIA1 (Unused)
 */
	
/*  CS0 Intel flash devices: 
 */
#define OMAHA_BANKCON0_DEF              0x00007FFC	 /*  Maximum clocks/function */
#define OMAHA_BANKCON1_DEF              0x00007FFC	 /*  Maximum clocks/function */
#define OMAHA_BANKCON2_DEF              0x00007FFC	 /*  Maximum clocks/function */
#define OMAHA_BANKCON3_DEF              0x00002400	 /*  tacs=1, tacc = 6clks */
#define OMAHA_BANKCON4_DEF              0x00007FFC	 /*  Maximum clocks/function */
#define OMAHA_BANKCON5_DEF              0x00007FFC	 /*  Maximum clocks/function */
	  
/*  CS6 SDRAM0 
 */
#define OMAHA_BANKCON6_DEF              0x00018001	 /*  9-bit, 2clks */

/*  CS7 SDRAM1
 */
#define OMAHA_BANKCON7_DEF              0x00018001	 /*  9-bit, 2clks */

/*  refresh (Assumes HCLK = 66MHz)
 *  refresh period = 64msecs (from datasheet) for 8K cycles (8usecs each)
 *  @66MHz busclock, this is 533 cycles...
 */
#define OMAHA_REFRESH_DEF               0x00A405EC	 /*  Refresh enabled, max clk. */

#define OMAHA_BANKSIZE_DEF              0x00000000	 /*  32Mb/32Mb */

/*  mode register	(CL = 3)
 */
#define OMAHA_MRSRB6_DEF                0x00000030
#define OMAHA_MRSRB7_DEF                0x00000030

/* -----------------------------------------------------------------------
 *  CPU Clocking
 * -----------------------------------------------------------------------
 */

/*  There are three important clock domains
 *  FCLK - CPU clock
 *  HCLK - AHB clock
 *  PCLK - APB clock
 */

/*  All clocks are derived from a 12MHz Xtal fed through
 *  PLL's and dividers.
 * 
 *  Note:
 *  HCLK = FCLK / 2
 *  PCLK = HCLK / 2
 * 
 *  Eg. for FCLK = 133MHz, HCLK = 66MHz, PCLK = 33MHz
 */

#define OMAHA_LOCKTIME_DEF              0xFFFFFF	 /*  PLL synchronization time */

/*  Nearest values (Samsung recommended from 12MHz xtal)
 */
#define OMAHA_CLK_33M                   0x25003		 /* ; 33.75 MHz */
#define OMAHA_CLK_66M                   0x25002		 /*   67.50 MHz */
#define OMAHA_CLK_100M                  0x2B011		 /*  102.00 MHz */
#define OMAHA_CLK_133M                  0x51021		 /*  133.50 MHz */

/*  Full speed ahead!
 */
#define OMAHA_CLK_DEFAULT               OMAHA_CLK_133M	

/*  Don't trust the PLL, use SLOW mode (HCLK = 12MHz direct)
 * OMAHA_SLOW		EQU	1
 */

/* -----------------------------------------------------------------------
 *  From PrimeCell UART (PL010) Block Specification (ARM-DDI-0139B)
 * -----------------------------------------------------------------------
 *  UART Base register absolute address
 */
#define OMAHA_UART0_BASE                0x15000000	 /*  Uart 0 base */
#define OMAHA_UART1_BASE                0x15004000	 /*  Uart 1 base */

/*  Offsets into registers of each UART controller
 */
#define OMAHA_ULCON                     0x00		 /*  Line control */
#define OMAHA_UCON                      0x04		 /*  Control */
#define OMAHA_UFCON                     0x08		 /*  FIFO control */
#define OMAHA_UMCON                     0x0C		 /*  Modem control */
#define OMAHA_UTRSTAT                   0x10		 /*  Rx/Tx status */
#define OMAHA_UERSTAT                   0x14		 /*  Rx Error Status */
#define OMAHA_UFSTAT                    0x18		 /*  FIFO status */
#define OMAHA_UMSTAT                    0x1C		 /*  Modem status */
#define OMAHA_UTXH                      0x20		 /*  Transmission Hold (byte wide) */
#define OMAHA_URXH                      0x24		 /*  Receive buffer (byte wide) */
#define OMAHA_UBRDIV                    0x28		 /*  Baud rate divisor */

/*  UART status flags in OMAHA_UTRSTAT
 */
#define OMAHA_URX_FULL                  0x1		 /*  Receive buffer has valid data */
#define OMAHA_UTX_EMPTY                 0x2		 /*  Transmitter has finished */

/* Baud rates supported on the uart. */

#define ARM_BAUD_460800                 460800
#define ARM_BAUD_230400                 230400
#define ARM_BAUD_115200                 115200
#define ARM_BAUD_57600                  57600
#define ARM_BAUD_38400                  38400
#define ARM_BAUD_19200                  19200
#define ARM_BAUD_9600                   9600

/*  LEDs
 *  These are connected to GPIO Port C
 */
#define PLAT_DBG_LEDS                   (PLAT_PERIPHERAL_BASE + OMAHA_PCDAT)	

/* -----------------------------------------------------------------------
 *  Interrupts
 * -----------------------------------------------------------------------
 */

/*  Interrupt numbers
 */
#define OMAHA_INT_EINT0                 0	 /*  FPGA */
#define OMAHA_INT_EINT1                 1	 /*  PLD */
#define OMAHA_INT_EINT2                 2	 /*  Expansion Bus */
#define OMAHA_INT_EINT3                 3	 /*  Ethernet */
#define OMAHA_INT_EINT4                 4	 /*  USB2 */
#define OMAHA_INT_EINT5                 5	 /*  Fan */
#define OMAHA_INT_EINT6                 6	 /*  unused */
#define OMAHA_INT_EINT7                 7	 /*  unused */
#define OMAHA_INT_TICK                  8
#define OMAHA_INT_WDT                   9
#define OMAHA_INT_TIMER0                10
#define OMAHA_INT_TIMER1                11
#define OMAHA_INT_TIMER2                12
#define OMAHA_INT_TIMER3                13
#define OMAHA_INT_TIMER4                14
#define OMAHA_INT_UERR                  15
/*  16 Unused
 */
#define OMAHA_INT_DMA0                  17
#define OMAHA_INT_DMA1                  18
#define OMAHA_INT_DMA2                  19
#define OMAHA_INT_DMA3                  20
#define OMAHA_INT_MMC                   21
#define OMAHA_INT_SPI                   22
#define OMAHA_INT_URXD0                 23
#define OMAHA_INT_URXD1                 24
#define OMAHA_INT_USBD                  25
#define OMAHA_INT_USBH                  26
#define OMAHA_INT_IIC                   27
#define OMAHA_INT_UTXD0                 28
#define OMAHA_INT_UTXD1                 29
#define OMAHA_INT_RTC                   30
#define OMAHA_INT_ADC                   31

/* -----------------------------------------------------------------------
 *  PLD Control registers (offset from PLAT_PLD_BASE)
 * -----------------------------------------------------------------------
 */

/*  register offsets from PLAT_PLD_BASE
 */
#define PLD_FPGA_ID                     0x00	 /*  8-bit FPGA ID number (ro) */
#define PLD_INT_CTRL                    0x04	 /*  Interrupt control */
#define PLD_INT_STATUS                  0x08	 /*  Interrupt status */
#define PLD_WAIT_STATUS                 0x0C	 /*  IO device wait status */
#define PLD_ID                          0x10	 /*  PCB revision number */
#define PLD_BUS                         0x14	 /*  Expansion bus control register */
#define PLD_TEST                        0x18	 /*  8-bit test register (rw) */
#define PLD_CF                          0x1C	 /*  CompactFlash control */
#define PLD_DMA_CTRL                    0x20	 /*  DMA Control register */

/*  PLD Register bitdefs
 */

/*  PLD_FPGA_ID		; contains FPGA Iden
 */

/*  INT_CTRL bit-defs
 */
#define PLD_INT_CTRL_USB                BIT0	 /*  USB */
#define PLD_INT_CTRL_LAN                BIT1	 /*  Ethernet */

/*  INT_STATUS bit-defs
 */
#define PLD_INT_STATUS_CF_IDE           BIT0	 /*  CF True-IDE mode (r/w) */
#define PLD_INT_STATUS_nDATACS          BIT1	 /*  SMC91C111 nDATACS control (r/w) */
#define PLD_INT_STATUS_CF_RDY           BIT2	 /*  CompactFlash ready (r/o) */
#define PLD_INT_STATUS_nWP              BIT3	 /*  Flash write protect (r/w) */

/*  WAIT_STATUS bit-defs
 */
#define PLD_WAIT_STATUS_USB             BIT0	 /*  USB wait signal active */
#define PLD_WAIT_STATUS_LAN             BIT1	 /*  Ethernet wait signal active */
#define PLD_WAIT_STATUS_CF              BIT2	 /*  CompactFlash wait status */

/*  ID		; Contains PLD Id
 */

/*  BOARD_ADDR
 */

#define PLD_BUS_ADDR                    0x0F	 /*  4-bit expansion bus address */
#define PLD_BUS_nACK                    BIT4	 /*  BUS nACK */
#define PLD_BUS_nCLR                    BIT5	 /*  BUS nCLR */
#define PLD_BUS_nSTR                    BIT6	 /*  BUS nSTR */
#define PLD_BUS_DONE                    BIT7	 /*  BUS Done */

/*  TEST		; 8-bit r/w any value.
 */

/*  CF status bitdefs
 */
#define PLD_CF_WP                       BIT0	 /*  r/w */
#define PLD_CF_RDY                      BIT1	 /*  CF_RDY r/o */
#define PLD_CF_VS1                      BIT2	 /*  Voltage Sense 1. r/o */
#define PLD_CF_VS2                      BIT3	 /*  Voltage Sense 2. r/o */
#define PLD_CF_RESET                    BIT4	 /*  Reset. r/w (TrueIDE active low/else high) */
#define PLD_CF_CD                       BIT5	 /*  Card Detect (1 AND 2). r/o */
#define PLD_CF_BVD                      BIT6	 /*  Batt. Voltage Detect (1 AND 2). r/o */
#define PLD_CF_nINPACK                  BIT7	 /*  Input Acknowlegde. r/o */

/*  DMA_CTRL
 */
#define PLD_DMA_BUS                     BIT4	 /*  Bus DMA request. r/w */
#define PLD_DMA_USB                     BIT5	 /*  USB DMA request. r/w */
#define PLD_DMA_FPGA                    BIT6	 /*  FPGA DMA request. r/w */

/* =======================================================================
 *  Start of uHAL definitions
 * =======================================================================
 */

#define MAXIRQNUM                       31
 
#define NR_IRQS                         (MAXIRQNUM + 1)

/* -----------------------------------------------------------------------
 *  LEDs - One available
 * -----------------------------------------------------------------------
 * 
 */
#define uHAL_LED_ON                     1
#define uHAL_LED_OFF                    0
#define uHAL_NUM_OF_LEDS                4

/*  Colours may not match reality...
 */
#define GREEN_LED                       BIT8
#define YELLOW_LED                      BIT9
#define RED_LED                         BIT10
#define RED_LED_1                       BIT11
#define ALL_LEDS                        (RED_LED | RED_LED_1 | YELLOW_LED | GREEN_LED)						    

#define LED_BANK                        PLAT_DBG_LEDS

/*  LED definitions.		
 *  The bit patterns & base addresses of the individual LEDs
 */
#define uHAL_LED_MASKS		{0, GREEN_LED, YELLOW_LED, RED_LED, RED_LED_1}
#define uHAL_LED_OFFSETS	{0, (void *)LED_BANK, (void *)LED_BANK, (void *)LED_BANK}


/* -----------------------------------------------------------------------
 *  Memory definitions - run uHAL out of SDRAM.  Reserve top 64K for MMU.
 */
#define uHAL_MEMORY_SIZE                PLAT_USER_SDRAM_SIZE


/*  Application Flash
 */
#define FLASH_BASE                      PLAT_FLASH_BASE
#define WFLASH_BASE                     PLAT_FLASH_UNCACHED
#define FLASH_SIZE                      PLAT_FLASH_SIZE
#define FLASH_END                       (FLASH_BASE + FLASH_SIZE - 1)
#define FLASH_BLOCK_SIZE                SZ_4K

/*  Reserve the first sector of flash for the boot switcher.
 *  Note: Changes to PLAT_BOOT_ROM_HI will have to be reflected in
 *  FlashLibrary/Boards/P920/flashMap.h
 */
#define PLAT_BOOT_ROM_HI                FLASH_BASE
#define PLAT_BOOT_ROM_SIZE              (FLASH_BLOCK_SIZE*0)

/*  Boot Flash
 */
#define EPROM_BASE                      PLAT_BOOT_ROM_HI
#define EPROM_SIZE                      PLAT_BOOT_ROM_SIZE
#define EPROM_END                       (EPROM_BASE + EPROM_SIZE - 1)


/*  Clean base - an area of memory (usually fast access) which can be read
 *  to ensure the data caches are flushed. 
 */
#define CLEAN_BASE                      EPROM_BASE

/* -----------------------------------------------------------------------
 *  UART definitions
 */

/*  Which com port can the OS use?
 * 
 */
/* Default port to talk to host (via debugger) */
#define HOST_COMPORT                    OMAHA_UART0_BASE
#define HOST_IRQBIT_NUMBER              OMAHA_INT_URXD0
#define HOST_IRQBIT                     (1 << HOST_IRQBIT_NUMBER)

/* Default port for use by Operating System or program */
#define OS_COMPORT                      OMAHA_UART0_BASE
#define OS_IRQBIT_NUMBER                OMAHA_INT_URXD0
#define OS_IRQBIT                       (1 << OS_IRQBIT_NUMBER)

#define DEBUG_COMPORT                   OS_COMPORT
#define DEBUG_IRQBIT                    OS_IRQBIT

/* Values to set given baud rates */
#define DEFAULT_HOST_BAUD               ARM_BAUD_9600
#define DEFAULT_OS_BAUD                 ARM_BAUD_38400

/* 'C' macros to access comports */
#define GET_STATUS(p)		(IO_READ((p) + OMAHA_UTRSTAT))
#define GET_CHAR(p)		((IO_READ((p) + OMAHA_URXH)) & 0xFF)
#define PUT_CHAR(p, c)		(IO_WRITE(((p) + OMAHA_UTXH), (c)))
#define IO_READ(p)             (*(volatile unsigned int *)(p))
#define IO_WRITE(p, c)         (*(unsigned int *)(p) = (c))
#define IO_MASK_WRITE(p, m, c) IO_WRITE(p, (IO_READ(p) & ~(m)) | (c))
#define IO_SET(p, c)	        IO_WRITE(p, (IO_READ(p) | (c)))
#define IO_CLEAR(p, c)         IO_WRITE(p, (IO_READ(p) & ~(c)))
#define RX_DATA(s)		(((s) & OMAHA_URX_FULL))
#define TX_READY(s)		(((s) & OMAHA_UTX_EMPTY))
#define TX_EMPTY(p)		((GET_STATUS(p) & OMAHA_UTX_EMPTY) != 0)


/* -----------------------------------------------------------------------
 *  Timer definitions
 * 
 *  There are 5 16-bit countdown timers on-chip.
 *  These are all driven by PCLK, running at whatever Mhz.
 *  For now we shall clock all the timers at 1MHz (arranged by uHAL)
 */

#define PLAT_TIMER1_BASE                (PLAT_PERIPHERAL_BASE + OMAHA_TCNTB0)
#define PLAT_TIMER2_BASE                (PLAT_PERIPHERAL_BASE + OMAHA_TCNTB1)
#define PLAT_TIMER3_BASE                (PLAT_PERIPHERAL_BASE + OMAHA_TCNTB2)
#define PLAT_TIMER4_BASE                (PLAT_PERIPHERAL_BASE + OMAHA_TCNTB3)
#define PLAT_TIMER5_BASE                (PLAT_PERIPHERAL_BASE + OMAHA_TCNTB4)


#define MAX_TIMER                       5
/*  Maximum time interval we can handle (in microseconds)
 *  = max_ticks / ticks_per_us ( = 65535 / 0.2)
 */

/*  Maximum period in uSecs
 */
#define MAX_PERIOD                      131072
			  	
#define PLAT_uS_TO_TICK(t) (t*1)

/*  These are useconds NOT ticks.  
 */
#define mSEC_1                          1000
#define mSEC_5                          (mSEC_1 * 5)
#define mSEC_10                         (mSEC_1 * 10)
#define mSEC_25                         (mSEC_1 * 25)
#define SEC_1                           (mSEC_1 * 1000)

/*  SEMIHOSTED Debugger doesn't use a timer by default. If it requires a
 *  timer (eg for profiling), set HOST_TIMER to MAX_TIMER
 */
#define HOST_TIMER                      0
#define OS_TIMER                        1
#define OS_TIMERINT                     OMAHA_INT_TIMER1


/*  Timer definitions.		
 *  The irq numbers & base addresses of the individual timers
 */
#define TIMER_VECTORS	{0, OMAHA_INT_TIMER0, OMAHA_INT_TIMER1, OMAHA_INT_TIMER2, OMAHA_INT_TIMER3, OMAHA_INT_TIMER4}
#define TIMER_BASES	{0, (void *)PLAT_TIMER1_BASE, (void *)PLAT_TIMER2_BASE, (void *)PLAT_TIMER3_BASE, (void *)PLAT_TIMER4_BASE, (void *)PLAT_TIMER5_BASE}


/* -----------------------------------------------------------------------
 *  Number of Level2 table entries in uHAL_AddressTable
 *  Each entry contains 256 32-bit descriptors
 */
#define L2_TABLE_ENTRIES                0


/* macros to map from PCI memory/IO addresses to local bus addresses
 */
#define _MapAddress(a)		(a)
#define _MapIOAddress(a)	(a)
#define _MapMemAddress(a)	(a)


#define ALLBITS                         0xffffffff


#ifdef uHAL_HEAP
#if USE_C_LIBRARY != 0
#define uHAL_HEAP_BASE                  (PLAT_SDRAM_BASE + SZ_1M)
#define uHAL_HEAP_SIZE                  (SZ_16M - SZ_1M - SZ_1M - 4)
#define uHAL_STACK_BASE                 SZ_16M
#define uHAL_STACK_SIZE                 SZ_1M
#else
#define uHAL_HEAP_BASE                  (PLAT_SDRAM_BASE + PLAT_SDRAM_SIZE - SZ_64K)
#define uHAL_HEAP_SIZE                  SZ_16K
#define uHAL_STACK_SIZE                 SZ_16K
#define uHAL_STACK_BASE                 (uHAL_HEAP_BASE + SZ32K)
#endif
#endif


#endif

/* 	END */

