// ----------------------------------------------------------------------------
//          ATMEL Microcontroller Software Support  -  ROUSSET  -
// ----------------------------------------------------------------------------
//  The software is delivered "AS IS" without warranty or condition of any
//  kind, either express, implied or statutory. This includes without
//  limitation any warranty or condition with respect to merchantability or
//  fitness for any particular purpose, or against the infringements of
//  intellectual property rights of others.
// ----------------------------------------------------------------------------
// $FreeBSD$
//
// File Name           : AT91RM9200.h
// Object              : AT91RM9200 definitions
// Generated           : AT91 SW Application Group  07/04/2003 (11:05:04)
// 
// CVS Reference       : /AT91RM9200.pl/1.16/Fri Feb 07 09:29:50 2003//
// CVS Reference       : /SYS_AT91RM9200.pl/1.2/Fri Jan 17 11:44:36 2003//
// CVS Reference       : /MC_1760A.pl/1.1/Fri Aug 23 13:38:22 2002//
// CVS Reference       : /AIC_1796B.pl/1.1.1.1/Fri Jun 28 08:36:46 2002//
// CVS Reference       : /PMC_2636A.pl/1.1.1.1/Fri Jun 28 08:36:48 2002//
// CVS Reference       : /ST_1763B.pl/1.1/Fri Aug 23 13:41:42 2002//
// CVS Reference       : /RTC_1245D.pl/1.2/Fri Jan 31 11:19:06 2003//
// CVS Reference       : /PIO_1725D.pl/1.1.1.1/Fri Jun 28 08:36:46 2002//
// CVS Reference       : /DBGU_1754A.pl/1.4/Fri Jan 31 11:18:24 2003//
// CVS Reference       : /UDP_1765B.pl/1.3/Fri Aug 02 13:45:38 2002//
// CVS Reference       : /MCI_1764A.pl/1.2/Thu Nov 14 16:48:24 2002//
// CVS Reference       : /US_1739C.pl/1.2/Fri Jul 12 06:49:24 2002//
// CVS Reference       : /SPI_AT91RMxxxx.pl/1.3/Tue Nov 26 09:20:28 2002//
// CVS Reference       : /SSC_1762A.pl/1.2/Fri Nov 08 12:26:38 2002//
// CVS Reference       : /TC_1753B.pl/1.2/Fri Jan 31 11:19:54 2003//
// CVS Reference       : /TWI_1761B.pl/1.4/Fri Feb 07 09:30:06 2003//
// CVS Reference       : /PDC_1734B.pl/1.2/Thu Nov 21 15:38:22 2002//
// CVS Reference       : /UHP_xxxxA.pl/1.1/Mon Jul 22 11:21:58 2002//
// CVS Reference       : /EMAC_1794A.pl/1.4/Fri Jan 17 11:11:54 2003//
// CVS Reference       : /EBI_1759B.pl/1.10/Fri Jan 17 11:44:28 2003//
// CVS Reference       : /SMC_1783A.pl/1.3/Thu Oct 31 13:38:16 2002//
// CVS Reference       : /SDRC_1758B.pl/1.2/Thu Oct 03 12:04:40 2002//
// CVS Reference       : /BFC_1757B.pl/1.3/Thu Oct 31 13:38:00 2002//
// ----------------------------------------------------------------------------

#ifndef AT91RM9200_H
#define	AT91RM9200_H

typedef volatile unsigned int AT91_REG;// Hardware register definition

// *****************************************************************************
//              SOFTWARE API DEFINITION  FOR System Peripherals
// *****************************************************************************
typedef struct _AT91S_SYS {
	AT91_REG	 AIC_SMR[32]; 	// Source Mode Register
	AT91_REG	 AIC_SVR[32]; 	// Source Vector Register
	AT91_REG	 AIC_IVR; 	// IRQ Vector Register
	AT91_REG	 AIC_FVR; 	// FIQ Vector Register
	AT91_REG	 AIC_ISR; 	// Interrupt Status Register
	AT91_REG	 AIC_IPR; 	// Interrupt Pending Register
	AT91_REG	 AIC_IMR; 	// Interrupt Mask Register
	AT91_REG	 AIC_CISR; 	// Core Interrupt Status Register
	AT91_REG	 Reserved0[2]; 	// 
	AT91_REG	 AIC_IECR; 	// Interrupt Enable Command Register
	AT91_REG	 AIC_IDCR; 	// Interrupt Disable Command Register
	AT91_REG	 AIC_ICCR; 	// Interrupt Clear Command Register
	AT91_REG	 AIC_ISCR; 	// Interrupt Set Command Register
	AT91_REG	 AIC_EOICR; 	// End of Interrupt Command Register
	AT91_REG	 AIC_SPU; 	// Spurious Vector Register
	AT91_REG	 AIC_DCR; 	// Debug Control Register (Protect)
	AT91_REG	 Reserved1[1]; 	// 
	AT91_REG	 AIC_FFER; 	// Fast Forcing Enable Register
	AT91_REG	 AIC_FFDR; 	// Fast Forcing Disable Register
	AT91_REG	 AIC_FFSR; 	// Fast Forcing Status Register
	AT91_REG	 Reserved2[45]; 	// 
	AT91_REG	 DBGU_CR; 	// Control Register
	AT91_REG	 DBGU_MR; 	// Mode Register
	AT91_REG	 DBGU_IER; 	// Interrupt Enable Register
	AT91_REG	 DBGU_IDR; 	// Interrupt Disable Register
	AT91_REG	 DBGU_IMR; 	// Interrupt Mask Register
	AT91_REG	 DBGU_CSR; 	// Channel Status Register
	AT91_REG	 DBGU_RHR; 	// Receiver Holding Register
	AT91_REG	 DBGU_THR; 	// Transmitter Holding Register
	AT91_REG	 DBGU_BRGR; 	// Baud Rate Generator Register
	AT91_REG	 Reserved3[7]; 	// 
	AT91_REG	 DBGU_C1R; 	// Chip ID1 Register
	AT91_REG	 DBGU_C2R; 	// Chip ID2 Register
	AT91_REG	 DBGU_FNTR; 	// Force NTRST Register
	AT91_REG	 Reserved4[45]; 	// 
	AT91_REG	 DBGU_RPR; 	// Receive Pointer Register
	AT91_REG	 DBGU_RCR; 	// Receive Counter Register
	AT91_REG	 DBGU_TPR; 	// Transmit Pointer Register
	AT91_REG	 DBGU_TCR; 	// Transmit Counter Register
	AT91_REG	 DBGU_RNPR; 	// Receive Next Pointer Register
	AT91_REG	 DBGU_RNCR; 	// Receive Next Counter Register
	AT91_REG	 DBGU_TNPR; 	// Transmit Next Pointer Register
	AT91_REG	 DBGU_TNCR; 	// Transmit Next Counter Register
	AT91_REG	 DBGU_PTCR; 	// PDC Transfer Control Register
	AT91_REG	 DBGU_PTSR; 	// PDC Transfer Status Register
	AT91_REG	 Reserved5[54]; 	// 
	AT91_REG	 PIOA_PER; 	// PIO Enable Register
	AT91_REG	 PIOA_PDR; 	// PIO Disable Register
	AT91_REG	 PIOA_PSR; 	// PIO Status Register
	AT91_REG	 Reserved6[1]; 	// 
	AT91_REG	 PIOA_OER; 	// Output Enable Register
	AT91_REG	 PIOA_ODR; 	// Output Disable Registerr
	AT91_REG	 PIOA_OSR; 	// Output Status Register
	AT91_REG	 Reserved7[1]; 	// 
	AT91_REG	 PIOA_IFER; 	// Input Filter Enable Register
	AT91_REG	 PIOA_IFDR; 	// Input Filter Disable Register
	AT91_REG	 PIOA_IFSR; 	// Input Filter Status Register
	AT91_REG	 Reserved8[1]; 	// 
	AT91_REG	 PIOA_SODR; 	// Set Output Data Register
	AT91_REG	 PIOA_CODR; 	// Clear Output Data Register
	AT91_REG	 PIOA_ODSR; 	// Output Data Status Register
	AT91_REG	 PIOA_PDSR; 	// Pin Data Status Register
	AT91_REG	 PIOA_IER; 	// Interrupt Enable Register
	AT91_REG	 PIOA_IDR; 	// Interrupt Disable Register
	AT91_REG	 PIOA_IMR; 	// Interrupt Mask Register
	AT91_REG	 PIOA_ISR; 	// Interrupt Status Register
	AT91_REG	 PIOA_MDER; 	// Multi-driver Enable Register
	AT91_REG	 PIOA_MDDR; 	// Multi-driver Disable Register
	AT91_REG	 PIOA_MDSR; 	// Multi-driver Status Register
	AT91_REG	 Reserved9[1]; 	// 
	AT91_REG	 PIOA_PPUDR; 	// Pull-up Disable Register
	AT91_REG	 PIOA_PPUER; 	// Pull-up Enable Register
	AT91_REG	 PIOA_PPUSR; 	// Pad Pull-up Status Register
	AT91_REG	 Reserved10[1]; 	// 
	AT91_REG	 PIOA_ASR; 	// Select A Register
	AT91_REG	 PIOA_BSR; 	// Select B Register
	AT91_REG	 PIOA_ABSR; 	// AB Select Status Register
	AT91_REG	 Reserved11[9]; 	// 
	AT91_REG	 PIOA_OWER; 	// Output Write Enable Register
	AT91_REG	 PIOA_OWDR; 	// Output Write Disable Register
	AT91_REG	 PIOA_OWSR; 	// Output Write Status Register
	AT91_REG	 Reserved12[85]; 	// 
	AT91_REG	 PIOB_PER; 	// PIO Enable Register
	AT91_REG	 PIOB_PDR; 	// PIO Disable Register
	AT91_REG	 PIOB_PSR; 	// PIO Status Register
	AT91_REG	 Reserved13[1]; 	// 
	AT91_REG	 PIOB_OER; 	// Output Enable Register
	AT91_REG	 PIOB_ODR; 	// Output Disable Registerr
	AT91_REG	 PIOB_OSR; 	// Output Status Register
	AT91_REG	 Reserved14[1]; 	// 
	AT91_REG	 PIOB_IFER; 	// Input Filter Enable Register
	AT91_REG	 PIOB_IFDR; 	// Input Filter Disable Register
	AT91_REG	 PIOB_IFSR; 	// Input Filter Status Register
	AT91_REG	 Reserved15[1]; 	// 
	AT91_REG	 PIOB_SODR; 	// Set Output Data Register
	AT91_REG	 PIOB_CODR; 	// Clear Output Data Register
	AT91_REG	 PIOB_ODSR; 	// Output Data Status Register
	AT91_REG	 PIOB_PDSR; 	// Pin Data Status Register
	AT91_REG	 PIOB_IER; 	// Interrupt Enable Register
	AT91_REG	 PIOB_IDR; 	// Interrupt Disable Register
	AT91_REG	 PIOB_IMR; 	// Interrupt Mask Register
	AT91_REG	 PIOB_ISR; 	// Interrupt Status Register
	AT91_REG	 PIOB_MDER; 	// Multi-driver Enable Register
	AT91_REG	 PIOB_MDDR; 	// Multi-driver Disable Register
	AT91_REG	 PIOB_MDSR; 	// Multi-driver Status Register
	AT91_REG	 Reserved16[1]; 	// 
	AT91_REG	 PIOB_PPUDR; 	// Pull-up Disable Register
	AT91_REG	 PIOB_PPUER; 	// Pull-up Enable Register
	AT91_REG	 PIOB_PPUSR; 	// Pad Pull-up Status Register
	AT91_REG	 Reserved17[1]; 	// 
	AT91_REG	 PIOB_ASR; 	// Select A Register
	AT91_REG	 PIOB_BSR; 	// Select B Register
	AT91_REG	 PIOB_ABSR; 	// AB Select Status Register
	AT91_REG	 Reserved18[9]; 	// 
	AT91_REG	 PIOB_OWER; 	// Output Write Enable Register
	AT91_REG	 PIOB_OWDR; 	// Output Write Disable Register
	AT91_REG	 PIOB_OWSR; 	// Output Write Status Register
	AT91_REG	 Reserved19[85]; 	// 
	AT91_REG	 PIOC_PER; 	// PIO Enable Register
	AT91_REG	 PIOC_PDR; 	// PIO Disable Register
	AT91_REG	 PIOC_PSR; 	// PIO Status Register
	AT91_REG	 Reserved20[1]; 	// 
	AT91_REG	 PIOC_OER; 	// Output Enable Register
	AT91_REG	 PIOC_ODR; 	// Output Disable Registerr
	AT91_REG	 PIOC_OSR; 	// Output Status Register
	AT91_REG	 Reserved21[1]; 	// 
	AT91_REG	 PIOC_IFER; 	// Input Filter Enable Register
	AT91_REG	 PIOC_IFDR; 	// Input Filter Disable Register
	AT91_REG	 PIOC_IFSR; 	// Input Filter Status Register
	AT91_REG	 Reserved22[1]; 	// 
	AT91_REG	 PIOC_SODR; 	// Set Output Data Register
	AT91_REG	 PIOC_CODR; 	// Clear Output Data Register
	AT91_REG	 PIOC_ODSR; 	// Output Data Status Register
	AT91_REG	 PIOC_PDSR; 	// Pin Data Status Register
	AT91_REG	 PIOC_IER; 	// Interrupt Enable Register
	AT91_REG	 PIOC_IDR; 	// Interrupt Disable Register
	AT91_REG	 PIOC_IMR; 	// Interrupt Mask Register
	AT91_REG	 PIOC_ISR; 	// Interrupt Status Register
	AT91_REG	 PIOC_MDER; 	// Multi-driver Enable Register
	AT91_REG	 PIOC_MDDR; 	// Multi-driver Disable Register
	AT91_REG	 PIOC_MDSR; 	// Multi-driver Status Register
	AT91_REG	 Reserved23[1]; 	// 
	AT91_REG	 PIOC_PPUDR; 	// Pull-up Disable Register
	AT91_REG	 PIOC_PPUER; 	// Pull-up Enable Register
	AT91_REG	 PIOC_PPUSR; 	// Pad Pull-up Status Register
	AT91_REG	 Reserved24[1]; 	// 
	AT91_REG	 PIOC_ASR; 	// Select A Register
	AT91_REG	 PIOC_BSR; 	// Select B Register
	AT91_REG	 PIOC_ABSR; 	// AB Select Status Register
	AT91_REG	 Reserved25[9]; 	// 
	AT91_REG	 PIOC_OWER; 	// Output Write Enable Register
	AT91_REG	 PIOC_OWDR; 	// Output Write Disable Register
	AT91_REG	 PIOC_OWSR; 	// Output Write Status Register
	AT91_REG	 Reserved26[85]; 	// 
	AT91_REG	 PIOD_PER; 	// PIO Enable Register
	AT91_REG	 PIOD_PDR; 	// PIO Disable Register
	AT91_REG	 PIOD_PSR; 	// PIO Status Register
	AT91_REG	 Reserved27[1]; 	// 
	AT91_REG	 PIOD_OER; 	// Output Enable Register
	AT91_REG	 PIOD_ODR; 	// Output Disable Registerr
	AT91_REG	 PIOD_OSR; 	// Output Status Register
	AT91_REG	 Reserved28[1]; 	// 
	AT91_REG	 PIOD_IFER; 	// Input Filter Enable Register
	AT91_REG	 PIOD_IFDR; 	// Input Filter Disable Register
	AT91_REG	 PIOD_IFSR; 	// Input Filter Status Register
	AT91_REG	 Reserved29[1]; 	// 
	AT91_REG	 PIOD_SODR; 	// Set Output Data Register
	AT91_REG	 PIOD_CODR; 	// Clear Output Data Register
	AT91_REG	 PIOD_ODSR; 	// Output Data Status Register
	AT91_REG	 PIOD_PDSR; 	// Pin Data Status Register
	AT91_REG	 PIOD_IER; 	// Interrupt Enable Register
	AT91_REG	 PIOD_IDR; 	// Interrupt Disable Register
	AT91_REG	 PIOD_IMR; 	// Interrupt Mask Register
	AT91_REG	 PIOD_ISR; 	// Interrupt Status Register
	AT91_REG	 PIOD_MDER; 	// Multi-driver Enable Register
	AT91_REG	 PIOD_MDDR; 	// Multi-driver Disable Register
	AT91_REG	 PIOD_MDSR; 	// Multi-driver Status Register
	AT91_REG	 Reserved30[1]; 	// 
	AT91_REG	 PIOD_PPUDR; 	// Pull-up Disable Register
	AT91_REG	 PIOD_PPUER; 	// Pull-up Enable Register
	AT91_REG	 PIOD_PPUSR; 	// Pad Pull-up Status Register
	AT91_REG	 Reserved31[1]; 	// 
	AT91_REG	 PIOD_ASR; 	// Select A Register
	AT91_REG	 PIOD_BSR; 	// Select B Register
	AT91_REG	 PIOD_ABSR; 	// AB Select Status Register
	AT91_REG	 Reserved32[9]; 	// 
	AT91_REG	 PIOD_OWER; 	// Output Write Enable Register
	AT91_REG	 PIOD_OWDR; 	// Output Write Disable Register
	AT91_REG	 PIOD_OWSR; 	// Output Write Status Register
	AT91_REG	 Reserved33[85]; 	// 
	AT91_REG	 PMC_SCER; 	// System Clock Enable Register
	AT91_REG	 PMC_SCDR; 	// System Clock Disable Register
	AT91_REG	 PMC_SCSR; 	// System Clock Status Register
	AT91_REG	 Reserved34[1]; 	// 
	AT91_REG	 PMC_PCER; 	// Peripheral Clock Enable Register
	AT91_REG	 PMC_PCDR; 	// Peripheral Clock Disable Register
	AT91_REG	 PMC_PCSR; 	// Peripheral Clock Status Register
	AT91_REG	 Reserved35[1]; 	// 
	AT91_REG	 CKGR_MOR; 	// Main Oscillator Register
	AT91_REG	 CKGR_MCFR; 	// Main Clock  Frequency Register
	AT91_REG	 CKGR_PLLAR; 	// PLL A Register
	AT91_REG	 CKGR_PLLBR; 	// PLL B Register
	AT91_REG	 PMC_MCKR; 	// Master Clock Register
	AT91_REG	 Reserved36[3]; 	// 
	AT91_REG	 PMC_PCKR[8]; 	// Programmable Clock Register
	AT91_REG	 PMC_IER; 	// Interrupt Enable Register
	AT91_REG	 PMC_IDR; 	// Interrupt Disable Register
	AT91_REG	 PMC_SR; 	// Status Register
	AT91_REG	 PMC_IMR; 	// Interrupt Mask Register
	AT91_REG	 Reserved37[36]; 	// 
	AT91_REG	 ST_CR; 	// Control Register
	AT91_REG	 ST_PIMR; 	// Period Interval Mode Register
	AT91_REG	 ST_WDMR; 	// Watchdog Mode Register
	AT91_REG	 ST_RTMR; 	// Real-time Mode Register
	AT91_REG	 ST_SR; 	// Status Register
	AT91_REG	 ST_IER; 	// Interrupt Enable Register
	AT91_REG	 ST_IDR; 	// Interrupt Disable Register
	AT91_REG	 ST_IMR; 	// Interrupt Mask Register
	AT91_REG	 ST_RTAR; 	// Real-time Alarm Register
	AT91_REG	 ST_CRTR; 	// Current Real-time Register
	AT91_REG	 Reserved38[54]; 	// 
	AT91_REG	 RTC_CR; 	// Control Register
	AT91_REG	 RTC_MR; 	// Mode Register
	AT91_REG	 RTC_TIMR; 	// Time Register
	AT91_REG	 RTC_CALR; 	// Calendar Register
	AT91_REG	 RTC_TIMALR; 	// Time Alarm Register
	AT91_REG	 RTC_CALALR; 	// Calendar Alarm Register
	AT91_REG	 RTC_SR; 	// Status Register
	AT91_REG	 RTC_SCCR; 	// Status Clear Command Register
	AT91_REG	 RTC_IER; 	// Interrupt Enable Register
	AT91_REG	 RTC_IDR; 	// Interrupt Disable Register
	AT91_REG	 RTC_IMR; 	// Interrupt Mask Register
	AT91_REG	 RTC_VER; 	// Valid Entry Register
	AT91_REG	 Reserved39[52]; 	// 
	AT91_REG	 MC_RCR; 	// MC Remap Control Register
	AT91_REG	 MC_ASR; 	// MC Abort Status Register
	AT91_REG	 MC_AASR; 	// MC Abort Address Status Register
	AT91_REG	 Reserved40[1]; 	// 
	AT91_REG	 MC_PUIA[16]; 	// MC Protection Unit Area
	AT91_REG	 MC_PUP; 	// MC Protection Unit Peripherals
	AT91_REG	 MC_PUER; 	// MC Protection Unit Enable Register
	AT91_REG	 Reserved41[2]; 	// 
	AT91_REG	 EBI_CSA; 	// Chip Select Assignment Register
	AT91_REG	 EBI_CFGR; 	// Configuration Register
	AT91_REG	 Reserved42[2]; 	// 
	AT91_REG	 EBI_SMC2_CSR[8]; 	// SMC2 Chip Select Register
	AT91_REG	 EBI_SDRC_MR; 	// SDRAM Controller Mode Register
	AT91_REG	 EBI_SDRC_TR; 	// SDRAM Controller Refresh Timer Register
	AT91_REG	 EBI_SDRC_CR; 	// SDRAM Controller Configuration Register
	AT91_REG	 EBI_SDRC_SRR; 	// SDRAM Controller Self Refresh Register
	AT91_REG	 EBI_SDRC_LPR; 	// SDRAM Controller Low Power Register
	AT91_REG	 EBI_SDRC_IER; 	// SDRAM Controller Interrupt Enable Register
	AT91_REG	 EBI_SDRC_IDR; 	// SDRAM Controller Interrupt Disable Register
	AT91_REG	 EBI_SDRC_IMR; 	// SDRAM Controller Interrupt Mask Register
	AT91_REG	 EBI_SDRC_ISR; 	// SDRAM Controller Interrupt Mask Register
	AT91_REG	 Reserved43[3]; 	// 
	AT91_REG	 EBI_BFC_MR; 	// BFC Mode Register
} AT91S_SYS, *AT91PS_SYS;


// *****************************************************************************
//              SOFTWARE API DEFINITION  FOR Memory Controller Interface
// *****************************************************************************
typedef struct _AT91S_MC {
	AT91_REG	 MC_RCR; 	// MC Remap Control Register
	AT91_REG	 MC_ASR; 	// MC Abort Status Register
	AT91_REG	 MC_AASR; 	// MC Abort Address Status Register
	AT91_REG	 Reserved0[1]; 	// 
	AT91_REG	 MC_PUIA[16]; 	// MC Protection Unit Area
	AT91_REG	 MC_PUP; 	// MC Protection Unit Peripherals
	AT91_REG	 MC_PUER; 	// MC Protection Unit Enable Register
} AT91S_MC, *AT91PS_MC;

// -------- MC_RCR : (MC Offset: 0x0) MC Remap Control Register -------- 
#define	AT91C_MC_RCB          (0x1u <<  0) // (MC) Remap Command Bit
// -------- MC_ASR : (MC Offset: 0x4) MC Abort Status Register -------- 
#define	AT91C_MC_UNDADD       (0x1u <<  0) // (MC) Undefined Addess Abort Status
#define	AT91C_MC_MISADD       (0x1u <<  1) // (MC) Misaligned Addess Abort Status
#define	AT91C_MC_MPU          (0x1u <<  2) // (MC) Memory protection Unit Abort Status
#define	AT91C_MC_ABTSZ        (0x3u <<  8) // (MC) Abort Size Status
#define		AT91C_MC_ABTSZ_BYTE                 (0x0u <<  8) // (MC) Byte
#define		AT91C_MC_ABTSZ_HWORD                (0x1u <<  8) // (MC) Half-word
#define		AT91C_MC_ABTSZ_WORD                 (0x2u <<  8) // (MC) Word
#define	AT91C_MC_ABTTYP       (0x3u << 10) // (MC) Abort Type Status
#define		AT91C_MC_ABTTYP_DATAR                (0x0u << 10) // (MC) Data Read
#define		AT91C_MC_ABTTYP_DATAW                (0x1u << 10) // (MC) Data Write
#define		AT91C_MC_ABTTYP_FETCH                (0x2u << 10) // (MC) Code Fetch
#define	AT91C_MC_MST0         (0x1u << 16) // (MC) Master 0 Abort Source
#define	AT91C_MC_MST1         (0x1u << 17) // (MC) Master 1 Abort Source
#define	AT91C_MC_SVMST0       (0x1u << 24) // (MC) Saved Master 0 Abort Source
#define	AT91C_MC_SVMST1       (0x1u << 25) // (MC) Saved Master 1 Abort Source
// -------- MC_PUIA : (MC Offset: 0x10) MC Protection Unit Area -------- 
#define	AT91C_MC_PROT         (0x3u <<  0) // (MC) Protection
#define		AT91C_MC_PROT_PNAUNA               0x0u // (MC) Privilege: No Access, User: No Access
#define		AT91C_MC_PROT_PRWUNA               0x1u // (MC) Privilege: Read/Write, User: No Access
#define		AT91C_MC_PROT_PRWURO               0x2u // (MC) Privilege: Read/Write, User: Read Only
#define		AT91C_MC_PROT_PRWURW               0x3u // (MC) Privilege: Read/Write, User: Read/Write
#define	AT91C_MC_SIZE         (0xFu <<  4) // (MC) Internal Area Size
#define		AT91C_MC_SIZE_1KB                  (0x0u <<  4) // (MC) Area size 1KByte
#define		AT91C_MC_SIZE_2KB                  (0x1u <<  4) // (MC) Area size 2KByte
#define		AT91C_MC_SIZE_4KB                  (0x2u <<  4) // (MC) Area size 4KByte
#define		AT91C_MC_SIZE_8KB                  (0x3u <<  4) // (MC) Area size 8KByte
#define		AT91C_MC_SIZE_16KB                 (0x4u <<  4) // (MC) Area size 16KByte
#define		AT91C_MC_SIZE_32KB                 (0x5u <<  4) // (MC) Area size 32KByte
#define		AT91C_MC_SIZE_64KB                 (0x6u <<  4) // (MC) Area size 64KByte
#define		AT91C_MC_SIZE_128KB                (0x7u <<  4) // (MC) Area size 128KByte
#define		AT91C_MC_SIZE_256KB                (0x8u <<  4) // (MC) Area size 256KByte
#define		AT91C_MC_SIZE_512KB                (0x9u <<  4) // (MC) Area size 512KByte
#define		AT91C_MC_SIZE_1MB                  (0xAu <<  4) // (MC) Area size 1MByte
#define		AT91C_MC_SIZE_2MB                  (0xBu <<  4) // (MC) Area size 2MByte
#define		AT91C_MC_SIZE_4MB                  (0xCu <<  4) // (MC) Area size 4MByte
#define		AT91C_MC_SIZE_8MB                  (0xDu <<  4) // (MC) Area size 8MByte
#define		AT91C_MC_SIZE_16MB                 (0xEu <<  4) // (MC) Area size 16MByte
#define		AT91C_MC_SIZE_64MB                 (0xFu <<  4) // (MC) Area size 64MByte
#define	AT91C_MC_BA           (0x3FFFFu << 10) // (MC) Internal Area Base Address
// -------- MC_PUP : (MC Offset: 0x50) MC Protection Unit Peripheral -------- 
// -------- MC_PUER : (MC Offset: 0x54) MC Protection Unit Area -------- 
#define	AT91C_MC_PUEB         (0x1u <<  0) // (MC) Protection Unit enable Bit

// *****************************************************************************
//              SOFTWARE API DEFINITION  FOR Real-time Clock Alarm and Parallel Load Interface
// *****************************************************************************
typedef struct _AT91S_RTC {
	AT91_REG	 RTC_CR; 	// Control Register
	AT91_REG	 RTC_MR; 	// Mode Register
	AT91_REG	 RTC_TIMR; 	// Time Register
	AT91_REG	 RTC_CALR; 	// Calendar Register
	AT91_REG	 RTC_TIMALR; 	// Time Alarm Register
	AT91_REG	 RTC_CALALR; 	// Calendar Alarm Register
	AT91_REG	 RTC_SR; 	// Status Register
	AT91_REG	 RTC_SCCR; 	// Status Clear Command Register
	AT91_REG	 RTC_IER; 	// Interrupt Enable Register
	AT91_REG	 RTC_IDR; 	// Interrupt Disable Register
	AT91_REG	 RTC_IMR; 	// Interrupt Mask Register
	AT91_REG	 RTC_VER; 	// Valid Entry Register
} AT91S_RTC, *AT91PS_RTC;

// -------- RTC_CR : (RTC Offset: 0x0) RTC Control Register -------- 
#define	AT91C_RTC_UPDTIM      (0x1u <<  0) // (RTC) Update Request Time Register
#define	AT91C_RTC_UPDCAL      (0x1u <<  1) // (RTC) Update Request Calendar Register
#define	AT91C_RTC_TIMEVSEL    (0x3u <<  8) // (RTC) Time Event Selection
#define		AT91C_RTC_TIMEVSEL_MINUTE               (0x0u <<  8) // (RTC) Minute change.
#define		AT91C_RTC_TIMEVSEL_HOUR                 (0x1u <<  8) // (RTC) Hour change.
#define		AT91C_RTC_TIMEVSEL_DAY24                (0x2u <<  8) // (RTC) Every day at midnight.
#define		AT91C_RTC_TIMEVSEL_DAY12                (0x3u <<  8) // (RTC) Every day at noon.
#define	AT91C_RTC_CALEVSEL    (0x3u << 16) // (RTC) Calendar Event Selection
#define		AT91C_RTC_CALEVSEL_WEEK                 (0x0u << 16) // (RTC) Week change (every Monday at time 00:00:00).
#define		AT91C_RTC_CALEVSEL_MONTH                (0x1u << 16) // (RTC) Month change (every 01 of each month at time 00:00:00).
#define		AT91C_RTC_CALEVSEL_YEAR                 (0x2u << 16) // (RTC) Year change (every January 1 at time 00:00:00).
// -------- RTC_MR : (RTC Offset: 0x4) RTC Mode Register -------- 
#define	AT91C_RTC_HRMOD       (0x1u <<  0) // (RTC) 12-24 hour Mode
// -------- RTC_TIMR : (RTC Offset: 0x8) RTC Time Register -------- 
#define	AT91C_RTC_SEC         (0x7Fu <<  0) // (RTC) Current Second
#define	AT91C_RTC_MIN         (0x7Fu <<  8) // (RTC) Current Minute
#define	AT91C_RTC_HOUR        (0x1Fu << 16) // (RTC) Current Hour
#define	AT91C_RTC_AMPM        (0x1u << 22) // (RTC) Ante Meridiem, Post Meridiem Indicator
// -------- RTC_CALR : (RTC Offset: 0xc) RTC Calendar Register -------- 
#define	AT91C_RTC_CENT        (0x3Fu <<  0) // (RTC) Current Century
#define	AT91C_RTC_YEAR        (0xFFu <<  8) // (RTC) Current Year
#define	AT91C_RTC_MONTH       (0x1Fu << 16) // (RTC) Current Month
#define	AT91C_RTC_DAY         (0x7u << 21) // (RTC) Current Day
#define	AT91C_RTC_DATE        (0x3Fu << 24) // (RTC) Current Date
// -------- RTC_TIMALR : (RTC Offset: 0x10) RTC Time Alarm Register -------- 
#define	AT91C_RTC_SECEN       (0x1u <<  7) // (RTC) Second Alarm Enable
#define	AT91C_RTC_MINEN       (0x1u << 15) // (RTC) Minute Alarm
#define	AT91C_RTC_HOUREN      (0x1u << 23) // (RTC) Current Hour
// -------- RTC_CALALR : (RTC Offset: 0x14) RTC Calendar Alarm Register -------- 
#define	AT91C_RTC_MONTHEN     (0x1u << 23) // (RTC) Month Alarm Enable
#define	AT91C_RTC_DATEEN      (0x1u << 31) // (RTC) Date Alarm Enable
// -------- RTC_SR : (RTC Offset: 0x18) RTC Status Register -------- 
#define	AT91C_RTC_ACKUPD      (0x1u <<  0) // (RTC) Acknowledge for Update
#define	AT91C_RTC_ALARM       (0x1u <<  1) // (RTC) Alarm Flag
#define	AT91C_RTC_SECEV       (0x1u <<  2) // (RTC) Second Event
#define	AT91C_RTC_TIMEV       (0x1u <<  3) // (RTC) Time Event
#define	AT91C_RTC_CALEV       (0x1u <<  4) // (RTC) Calendar event
// -------- RTC_SCCR : (RTC Offset: 0x1c) RTC Status Clear Command Register -------- 
// -------- RTC_IER : (RTC Offset: 0x20) RTC Interrupt Enable Register -------- 
// -------- RTC_IDR : (RTC Offset: 0x24) RTC Interrupt Disable Register -------- 
// -------- RTC_IMR : (RTC Offset: 0x28) RTC Interrupt Mask Register -------- 
// -------- RTC_VER : (RTC Offset: 0x2c) RTC Valid Entry Register -------- 
#define	AT91C_RTC_NVTIM       (0x1u <<  0) // (RTC) Non valid Time
#define	AT91C_RTC_NVCAL       (0x1u <<  1) // (RTC) Non valid Calendar
#define	AT91C_RTC_NVTIMALR    (0x1u <<  2) // (RTC) Non valid time Alarm
#define	AT91C_RTC_NVCALALR    (0x1u <<  3) // (RTC) Nonvalid Calendar Alarm

// *****************************************************************************
//              SOFTWARE API DEFINITION  FOR System Timer Interface
// *****************************************************************************
typedef struct _AT91S_ST {
	AT91_REG	 ST_CR; 	// Control Register
	AT91_REG	 ST_PIMR; 	// Period Interval Mode Register
	AT91_REG	 ST_WDMR; 	// Watchdog Mode Register
	AT91_REG	 ST_RTMR; 	// Real-time Mode Register
	AT91_REG	 ST_SR; 	// Status Register
	AT91_REG	 ST_IER; 	// Interrupt Enable Register
	AT91_REG	 ST_IDR; 	// Interrupt Disable Register
	AT91_REG	 ST_IMR; 	// Interrupt Mask Register
	AT91_REG	 ST_RTAR; 	// Real-time Alarm Register
	AT91_REG	 ST_CRTR; 	// Current Real-time Register
} AT91S_ST, *AT91PS_ST;

// -------- ST_CR : (ST Offset: 0x0) System Timer Control Register -------- 
#define	AT91C_ST_WDRST        (0x1u <<  0) // (ST) Watchdog Timer Restart
// -------- ST_PIMR : (ST Offset: 0x4) System Timer Period Interval Mode Register -------- 
#define	AT91C_ST_PIV          (0xFFFFu <<  0) // (ST) Watchdog Timer Restart
// -------- ST_WDMR : (ST Offset: 0x8) System Timer Watchdog Mode Register -------- 
#define	AT91C_ST_WDV          (0xFFFFu <<  0) // (ST) Watchdog Timer Restart
#define	AT91C_ST_RSTEN        (0x1u << 16) // (ST) Reset Enable
#define	AT91C_ST_EXTEN        (0x1u << 17) // (ST) External Signal Assertion Enable
// -------- ST_RTMR : (ST Offset: 0xc) System Timer Real-time Mode Register -------- 
#define	AT91C_ST_RTPRES       (0xFFFFu <<  0) // (ST) Real-time Timer Prescaler Value
// -------- ST_SR : (ST Offset: 0x10) System Timer Status Register -------- 
#define	AT91C_ST_PITS         (0x1u <<  0) // (ST) Period Interval Timer Interrupt
#define	AT91C_ST_WDOVF        (0x1u <<  1) // (ST) Watchdog Overflow
#define	AT91C_ST_RTTINC       (0x1u <<  2) // (ST) Real-time Timer Increment
#define	AT91C_ST_ALMS         (0x1u <<  3) // (ST) Alarm Status
// -------- ST_IER : (ST Offset: 0x14) System Timer Interrupt Enable Register -------- 
// -------- ST_IDR : (ST Offset: 0x18) System Timer Interrupt Disable Register -------- 
// -------- ST_IMR : (ST Offset: 0x1c) System Timer Interrupt Mask Register -------- 
// -------- ST_RTAR : (ST Offset: 0x20) System Timer Real-time Alarm Register -------- 
#define	AT91C_ST_ALMV         (0xFFFFFu <<  0) // (ST) Alarm Value Value
// -------- ST_CRTR : (ST Offset: 0x24) System Timer Current Real-time Register -------- 
#define	AT91C_ST_CRTV         (0xFFFFFu <<  0) // (ST) Current Real-time Value

// *****************************************************************************
//              SOFTWARE API DEFINITION  FOR Power Management Controler
// *****************************************************************************
typedef struct _AT91S_PMC {
	AT91_REG	 PMC_SCER; 	// System Clock Enable Register
	AT91_REG	 PMC_SCDR; 	// System Clock Disable Register
	AT91_REG	 PMC_SCSR; 	// System Clock Status Register
	AT91_REG	 Reserved0[1]; 	// 
	AT91_REG	 PMC_PCER; 	// Peripheral Clock Enable Register
	AT91_REG	 PMC_PCDR; 	// Peripheral Clock Disable Register
	AT91_REG	 PMC_PCSR; 	// Peripheral Clock Status Register
	AT91_REG	 Reserved1[5]; 	// 
	AT91_REG	 PMC_MCKR; 	// Master Clock Register
	AT91_REG	 Reserved2[3]; 	// 
	AT91_REG	 PMC_PCKR[8]; 	// Programmable Clock Register
	AT91_REG	 PMC_IER; 	// Interrupt Enable Register
	AT91_REG	 PMC_IDR; 	// Interrupt Disable Register
	AT91_REG	 PMC_SR; 	// Status Register
	AT91_REG	 PMC_IMR; 	// Interrupt Mask Register
} AT91S_PMC, *AT91PS_PMC;

// -------- PMC_SCER : (PMC Offset: 0x0) System Clock Enable Register -------- 
#define	AT91C_PMC_PCK         (0x1u <<  0) // (PMC) Processor Clock
#define	AT91C_PMC_UDP         (0x1u <<  1) // (PMC) USB Device Port Clock
#define	AT91C_PMC_MCKUDP      (0x1u <<  2) // (PMC) USB Device Port Master Clock Automatic Disable on Suspend
#define	AT91C_PMC_UHP         (0x1u <<  4) // (PMC) USB Host Port Clock
#define	AT91C_PMC_PCK0        (0x1u <<  8) // (PMC) Programmable Clock Output
#define	AT91C_PMC_PCK1        (0x1u <<  9) // (PMC) Programmable Clock Output
#define	AT91C_PMC_PCK2        (0x1u << 10) // (PMC) Programmable Clock Output
#define	AT91C_PMC_PCK3        (0x1u << 11) // (PMC) Programmable Clock Output
#define	AT91C_PMC_PCK4        (0x1u << 12) // (PMC) Programmable Clock Output
#define	AT91C_PMC_PCK5        (0x1u << 13) // (PMC) Programmable Clock Output
#define	AT91C_PMC_PCK6        (0x1u << 14) // (PMC) Programmable Clock Output
#define	AT91C_PMC_PCK7        (0x1u << 15) // (PMC) Programmable Clock Output
// -------- PMC_SCDR : (PMC Offset: 0x4) System Clock Disable Register -------- 
// -------- PMC_SCSR : (PMC Offset: 0x8) System Clock Status Register -------- 
// -------- PMC_MCKR : (PMC Offset: 0x30) Master Clock Register -------- 
#define	AT91C_PMC_CSS         (0x3u <<  0) // (PMC) Programmable Clock Selection
#define		AT91C_PMC_CSS_SLOW_CLK             0x0u // (PMC) Slow Clock is selected
#define		AT91C_PMC_CSS_MAIN_CLK             0x1u // (PMC) Main Clock is selected
#define		AT91C_PMC_CSS_PLLA_CLK             0x2u // (PMC) Clock from PLL A is selected
#define		AT91C_PMC_CSS_PLLB_CLK             0x3u // (PMC) Clock from PLL B is selected
#define	AT91C_PMC_PRES        (0x7u <<  2) // (PMC) Programmable Clock Prescaler
#define		AT91C_PMC_PRES_CLK                  (0x0u <<  2) // (PMC) Selected clock
#define		AT91C_PMC_PRES_CLK_2                (0x1u <<  2) // (PMC) Selected clock divided by 2
#define		AT91C_PMC_PRES_CLK_4                (0x2u <<  2) // (PMC) Selected clock divided by 4
#define		AT91C_PMC_PRES_CLK_8                (0x3u <<  2) // (PMC) Selected clock divided by 8
#define		AT91C_PMC_PRES_CLK_16               (0x4u <<  2) // (PMC) Selected clock divided by 16
#define		AT91C_PMC_PRES_CLK_32               (0x5u <<  2) // (PMC) Selected clock divided by 32
#define		AT91C_PMC_PRES_CLK_64               (0x6u <<  2) // (PMC) Selected clock divided by 64
#define	AT91C_PMC_MDIV        (0x3u <<  8) // (PMC) Master Clock Division
#define		AT91C_PMC_MDIV_1                    (0x0u <<  8) // (PMC) The master clock and the processor clock are the same
#define		AT91C_PMC_MDIV_2                    (0x1u <<  8) // (PMC) The processor clock is twice as fast as the master clock
#define		AT91C_PMC_MDIV_3                    (0x2u <<  8) // (PMC) The processor clock is three times faster than the master clock
#define		AT91C_PMC_MDIV_4                    (0x3u <<  8) // (PMC) The processor clock is four times faster than the master clock
// -------- PMC_PCKR : (PMC Offset: 0x40) Programmable Clock Register -------- 
// -------- PMC_IER : (PMC Offset: 0x60) PMC Interrupt Enable Register -------- 
#define	AT91C_PMC_MOSCS       (0x1u <<  0) // (PMC) MOSC Status/Enable/Disable/Mask
#define	AT91C_PMC_LOCKA       (0x1u <<  1) // (PMC) PLL A Status/Enable/Disable/Mask
#define	AT91C_PMC_LOCKB       (0x1u <<  2) // (PMC) PLL B Status/Enable/Disable/Mask
#define	AT91C_PMC_MCKRDY      (0x1u <<  3) // (PMC) MCK_RDY Status/Enable/Disable/Mask
#define	AT91C_PMC_PCK0RDY     (0x1u <<  8) // (PMC) PCK0_RDY Status/Enable/Disable/Mask
#define	AT91C_PMC_PCK1RDY     (0x1u <<  9) // (PMC) PCK1_RDY Status/Enable/Disable/Mask
#define	AT91C_PMC_PCK2RDY     (0x1u << 10) // (PMC) PCK2_RDY Status/Enable/Disable/Mask
#define	AT91C_PMC_PCK3RDY     (0x1u << 11) // (PMC) PCK3_RDY Status/Enable/Disable/Mask
#define	AT91C_PMC_PCK4RDY     (0x1u << 12) // (PMC) PCK4_RDY Status/Enable/Disable/Mask
#define	AT91C_PMC_PCK5RDY     (0x1u << 13) // (PMC) PCK5_RDY Status/Enable/Disable/Mask
#define	AT91C_PMC_PCK6RDY     (0x1u << 14) // (PMC) PCK6_RDY Status/Enable/Disable/Mask
#define	AT91C_PMC_PCK7RDY     (0x1u << 15) // (PMC) PCK7_RDY Status/Enable/Disable/Mask
// -------- PMC_IDR : (PMC Offset: 0x64) PMC Interrupt Disable Register -------- 
// -------- PMC_SR : (PMC Offset: 0x68) PMC Status Register -------- 
// -------- PMC_IMR : (PMC Offset: 0x6c) PMC Interrupt Mask Register -------- 

// *****************************************************************************
//              SOFTWARE API DEFINITION  FOR Clock Generator Controler
// *****************************************************************************
typedef struct _AT91S_CKGR {
	AT91_REG	 CKGR_MOR; 	// Main Oscillator Register
	AT91_REG	 CKGR_MCFR; 	// Main Clock  Frequency Register
	AT91_REG	 CKGR_PLLAR; 	// PLL A Register
	AT91_REG	 CKGR_PLLBR; 	// PLL B Register
} AT91S_CKGR, *AT91PS_CKGR;

// -------- CKGR_MOR : (CKGR Offset: 0x0) Main Oscillator Register -------- 
#define	AT91C_CKGR_MOSCEN     (0x1u <<  0) // (CKGR) Main Oscillator Enable
#define	AT91C_CKGR_OSCTEST    (0x1u <<  1) // (CKGR) Oscillator Test
#define	AT91C_CKGR_OSCOUNT    (0xFFu <<  8) // (CKGR) Main Oscillator Start-up Time
// -------- CKGR_MCFR : (CKGR Offset: 0x4) Main Clock Frequency Register -------- 
#define	AT91C_CKGR_MAINF      (0xFFFFu <<  0) // (CKGR) Main Clock Frequency
#define	AT91C_CKGR_MAINRDY    (0x1u << 16) // (CKGR) Main Clock Ready
// -------- CKGR_PLLAR : (CKGR Offset: 0x8) PLL A Register -------- 
#define	AT91C_CKGR_DIVA       (0xFFu <<  0) // (CKGR) Divider Selected
#define		AT91C_CKGR_DIVA_0                    0x0u // (CKGR) Divider output is 0
#define		AT91C_CKGR_DIVA_BYPASS               0x1u // (CKGR) Divider is bypassed
#define	AT91C_CKGR_PLLACOUNT  (0x3Fu <<  8) // (CKGR) PLL A Counter
#define	AT91C_CKGR_OUTA       (0x3u << 14) // (CKGR) PLL A Output Frequency Range
#define		AT91C_CKGR_OUTA_0                    (0x0u << 14) // (CKGR) Please refer to the PLLA datasheet
#define		AT91C_CKGR_OUTA_1                    (0x1u << 14) // (CKGR) Please refer to the PLLA datasheet
#define		AT91C_CKGR_OUTA_2                    (0x2u << 14) // (CKGR) Please refer to the PLLA datasheet
#define		AT91C_CKGR_OUTA_3                    (0x3u << 14) // (CKGR) Please refer to the PLLA datasheet
#define	AT91C_CKGR_MULA       (0x7FFu << 16) // (CKGR) PLL A Multiplier
#define	AT91C_CKGR_SRCA       (0x1u << 29) // (CKGR) PLL A Source
// -------- CKGR_PLLBR : (CKGR Offset: 0xc) PLL B Register -------- 
#define	AT91C_CKGR_DIVB       (0xFFu <<  0) // (CKGR) Divider Selected
#define		AT91C_CKGR_DIVB_0                    0x0u // (CKGR) Divider output is 0
#define		AT91C_CKGR_DIVB_BYPASS               0x1u // (CKGR) Divider is bypassed
#define	AT91C_CKGR_PLLBCOUNT  (0x3Fu <<  8) // (CKGR) PLL B Counter
#define	AT91C_CKGR_OUTB       (0x3u << 14) // (CKGR) PLL B Output Frequency Range
#define		AT91C_CKGR_OUTB_0                    (0x0u << 14) // (CKGR) Please refer to the PLLB datasheet
#define		AT91C_CKGR_OUTB_1                    (0x1u << 14) // (CKGR) Please refer to the PLLB datasheet
#define		AT91C_CKGR_OUTB_2                    (0x2u << 14) // (CKGR) Please refer to the PLLB datasheet
#define		AT91C_CKGR_OUTB_3                    (0x3u << 14) // (CKGR) Please refer to the PLLB datasheet
#define	AT91C_CKGR_MULB       (0x7FFu << 16) // (CKGR) PLL B Multiplier
#define	AT91C_CKGR_USB_96M    (0x1u << 28) // (CKGR) Divider for USB Ports
#define	AT91C_CKGR_USB_PLL    (0x1u << 29) // (CKGR) PLL Use

// *****************************************************************************
//              SOFTWARE API DEFINITION  FOR Parallel Input Output Controler
// *****************************************************************************
typedef struct _AT91S_PIO {
	AT91_REG	 PIO_PER; 	// PIO Enable Register
	AT91_REG	 PIO_PDR; 	// PIO Disable Register
	AT91_REG	 PIO_PSR; 	// PIO Status Register
	AT91_REG	 Reserved0[1]; 	// 
	AT91_REG	 PIO_OER; 	// Output Enable Register
	AT91_REG	 PIO_ODR; 	// Output Disable Registerr
	AT91_REG	 PIO_OSR; 	// Output Status Register
	AT91_REG	 Reserved1[1]; 	// 
	AT91_REG	 PIO_IFER; 	// Input Filter Enable Register
	AT91_REG	 PIO_IFDR; 	// Input Filter Disable Register
	AT91_REG	 PIO_IFSR; 	// Input Filter Status Register
	AT91_REG	 Reserved2[1]; 	// 
	AT91_REG	 PIO_SODR; 	// Set Output Data Register
	AT91_REG	 PIO_CODR; 	// Clear Output Data Register
	AT91_REG	 PIO_ODSR; 	// Output Data Status Register
	AT91_REG	 PIO_PDSR; 	// Pin Data Status Register
	AT91_REG	 PIO_IER; 	// Interrupt Enable Register
	AT91_REG	 PIO_IDR; 	// Interrupt Disable Register
	AT91_REG	 PIO_IMR; 	// Interrupt Mask Register
	AT91_REG	 PIO_ISR; 	// Interrupt Status Register
	AT91_REG	 PIO_MDER; 	// Multi-driver Enable Register
	AT91_REG	 PIO_MDDR; 	// Multi-driver Disable Register
	AT91_REG	 PIO_MDSR; 	// Multi-driver Status Register
	AT91_REG	 Reserved3[1]; 	// 
	AT91_REG	 PIO_PPUDR; 	// Pull-up Disable Register
	AT91_REG	 PIO_PPUER; 	// Pull-up Enable Register
	AT91_REG	 PIO_PPUSR; 	// Pad Pull-up Status Register
	AT91_REG	 Reserved4[1]; 	// 
	AT91_REG	 PIO_ASR; 	// Select A Register
	AT91_REG	 PIO_BSR; 	// Select B Register
	AT91_REG	 PIO_ABSR; 	// AB Select Status Register
	AT91_REG	 Reserved5[9]; 	// 
	AT91_REG	 PIO_OWER; 	// Output Write Enable Register
	AT91_REG	 PIO_OWDR; 	// Output Write Disable Register
	AT91_REG	 PIO_OWSR; 	// Output Write Status Register
} AT91S_PIO, *AT91PS_PIO;


// *****************************************************************************
//              SOFTWARE API DEFINITION  FOR Debug Unit
// *****************************************************************************
typedef struct _AT91S_DBGU {
	AT91_REG	 DBGU_CR; 	// Control Register
	AT91_REG	 DBGU_MR; 	// Mode Register
	AT91_REG	 DBGU_IER; 	// Interrupt Enable Register
	AT91_REG	 DBGU_IDR; 	// Interrupt Disable Register
	AT91_REG	 DBGU_IMR; 	// Interrupt Mask Register
	AT91_REG	 DBGU_CSR; 	// Channel Status Register
	AT91_REG	 DBGU_RHR; 	// Receiver Holding Register
	AT91_REG	 DBGU_THR; 	// Transmitter Holding Register
	AT91_REG	 DBGU_BRGR; 	// Baud Rate Generator Register
	AT91_REG	 Reserved0[7]; 	// 
	AT91_REG	 DBGU_C1R; 	// Chip ID1 Register
	AT91_REG	 DBGU_C2R; 	// Chip ID2 Register
	AT91_REG	 DBGU_FNTR; 	// Force NTRST Register
	AT91_REG	 Reserved1[45]; 	// 
	AT91_REG	 DBGU_RPR; 	// Receive Pointer Register
	AT91_REG	 DBGU_RCR; 	// Receive Counter Register
	AT91_REG	 DBGU_TPR; 	// Transmit Pointer Register
	AT91_REG	 DBGU_TCR; 	// Transmit Counter Register
	AT91_REG	 DBGU_RNPR; 	// Receive Next Pointer Register
	AT91_REG	 DBGU_RNCR; 	// Receive Next Counter Register
	AT91_REG	 DBGU_TNPR; 	// Transmit Next Pointer Register
	AT91_REG	 DBGU_TNCR; 	// Transmit Next Counter Register
	AT91_REG	 DBGU_PTCR; 	// PDC Transfer Control Register
	AT91_REG	 DBGU_PTSR; 	// PDC Transfer Status Register
} AT91S_DBGU, *AT91PS_DBGU;

// -------- DBGU_CR : (DBGU Offset: 0x0) Debug Unit Control Register -------- 
#define	AT91C_US_RSTRX        (0x1u <<  2) // (DBGU) Reset Receiver
#define	AT91C_US_RSTTX        (0x1u <<  3) // (DBGU) Reset Transmitter
#define	AT91C_US_RXEN         (0x1u <<  4) // (DBGU) Receiver Enable
#define	AT91C_US_RXDIS        (0x1u <<  5) // (DBGU) Receiver Disable
#define	AT91C_US_TXEN         (0x1u <<  6) // (DBGU) Transmitter Enable
#define	AT91C_US_TXDIS        (0x1u <<  7) // (DBGU) Transmitter Disable
// -------- DBGU_MR : (DBGU Offset: 0x4) Debug Unit Mode Register -------- 
#define	AT91C_US_PAR          (0x7u <<  9) // (DBGU) Parity type
#define		AT91C_US_PAR_EVEN                 (0x0u <<  9) // (DBGU) Even Parity
#define		AT91C_US_PAR_ODD                  (0x1u <<  9) // (DBGU) Odd Parity
#define		AT91C_US_PAR_SPACE                (0x2u <<  9) // (DBGU) Parity forced to 0 (Space)
#define		AT91C_US_PAR_MARK                 (0x3u <<  9) // (DBGU) Parity forced to 1 (Mark)
#define		AT91C_US_PAR_NONE                 (0x4u <<  9) // (DBGU) No Parity
#define		AT91C_US_PAR_MULTI_DROP           (0x6u <<  9) // (DBGU) Multi-drop mode
#define	AT91C_US_CHMODE       (0x3u << 14) // (DBGU) Channel Mode
#define		AT91C_US_CHMODE_NORMAL               (0x0u << 14) // (DBGU) Normal Mode: The USART channel operates as an RX/TX USART.
#define		AT91C_US_CHMODE_AUTO                 (0x1u << 14) // (DBGU) Automatic Echo: Receiver Data Input is connected to the TXD pin.
#define		AT91C_US_CHMODE_LOCAL                (0x2u << 14) // (DBGU) Local Loopback: Transmitter Output Signal is connected to Receiver Input Signal.
#define		AT91C_US_CHMODE_REMOTE               (0x3u << 14) // (DBGU) Remote Loopback: RXD pin is internally connected to TXD pin.
// -------- DBGU_IER : (DBGU Offset: 0x8) Debug Unit Interrupt Enable Register -------- 
#define	AT91C_US_RXRDY        (0x1u <<  0) // (DBGU) RXRDY Interrupt
#define	AT91C_US_TXRDY        (0x1u <<  1) // (DBGU) TXRDY Interrupt
#define	AT91C_US_ENDRX        (0x1u <<  3) // (DBGU) End of Receive Transfer Interrupt
#define	AT91C_US_ENDTX        (0x1u <<  4) // (DBGU) End of Transmit Interrupt
#define	AT91C_US_OVRE         (0x1u <<  5) // (DBGU) Overrun Interrupt
#define	AT91C_US_FRAME        (0x1u <<  6) // (DBGU) Framing Error Interrupt
#define	AT91C_US_PARE         (0x1u <<  7) // (DBGU) Parity Error Interrupt
#define	AT91C_US_TXEMPTY      (0x1u <<  9) // (DBGU) TXEMPTY Interrupt
#define	AT91C_US_TXBUFE       (0x1u << 11) // (DBGU) TXBUFE Interrupt
#define	AT91C_US_RXBUFF       (0x1u << 12) // (DBGU) RXBUFF Interrupt
#define	AT91C_US_COMM_TX      (0x1u << 30) // (DBGU) COMM_TX Interrupt
#define	AT91C_US_COMM_RX      (0x1u << 31) // (DBGU) COMM_RX Interrupt
// -------- DBGU_IDR : (DBGU Offset: 0xc) Debug Unit Interrupt Disable Register -------- 
// -------- DBGU_IMR : (DBGU Offset: 0x10) Debug Unit Interrupt Mask Register -------- 
// -------- DBGU_CSR : (DBGU Offset: 0x14) Debug Unit Channel Status Register -------- 
// -------- DBGU_FNTR : (DBGU Offset: 0x48) Debug Unit FORCE_NTRST Register -------- 
#define	AT91C_US_FORCE_NTRST  (0x1u <<  0) // (DBGU) Force NTRST in JTAG

// *****************************************************************************
//              SOFTWARE API DEFINITION  FOR Peripheral Data Controller
// *****************************************************************************
typedef struct _AT91S_PDC {
	AT91_REG	 PDC_RPR; 	// Receive Pointer Register
	AT91_REG	 PDC_RCR; 	// Receive Counter Register
	AT91_REG	 PDC_TPR; 	// Transmit Pointer Register
	AT91_REG	 PDC_TCR; 	// Transmit Counter Register
	AT91_REG	 PDC_RNPR; 	// Receive Next Pointer Register
	AT91_REG	 PDC_RNCR; 	// Receive Next Counter Register
	AT91_REG	 PDC_TNPR; 	// Transmit Next Pointer Register
	AT91_REG	 PDC_TNCR; 	// Transmit Next Counter Register
	AT91_REG	 PDC_PTCR; 	// PDC Transfer Control Register
	AT91_REG	 PDC_PTSR; 	// PDC Transfer Status Register
} AT91S_PDC, *AT91PS_PDC;

// -------- PDC_PTCR : (PDC Offset: 0x20) PDC Transfer Control Register -------- 
#define	AT91C_PDC_RXTEN       (0x1u <<  0) // (PDC) Receiver Transfer Enable
#define	AT91C_PDC_RXTDIS      (0x1u <<  1) // (PDC) Receiver Transfer Disable
#define	AT91C_PDC_TXTEN       (0x1u <<  8) // (PDC) Transmitter Transfer Enable
#define	AT91C_PDC_TXTDIS      (0x1u <<  9) // (PDC) Transmitter Transfer Disable
// -------- PDC_PTSR : (PDC Offset: 0x24) PDC Transfer Status Register -------- 

// *****************************************************************************
//              SOFTWARE API DEFINITION  FOR Advanced Interrupt Controller
// *****************************************************************************
typedef struct _AT91S_AIC {
	AT91_REG	 AIC_SMR[32]; 	// Source Mode Register
	AT91_REG	 AIC_SVR[32]; 	// Source Vector Register
	AT91_REG	 AIC_IVR; 	// IRQ Vector Register
	AT91_REG	 AIC_FVR; 	// FIQ Vector Register
	AT91_REG	 AIC_ISR; 	// Interrupt Status Register
	AT91_REG	 AIC_IPR; 	// Interrupt Pending Register
	AT91_REG	 AIC_IMR; 	// Interrupt Mask Register
	AT91_REG	 AIC_CISR; 	// Core Interrupt Status Register
	AT91_REG	 Reserved0[2]; 	// 
	AT91_REG	 AIC_IECR; 	// Interrupt Enable Command Register
	AT91_REG	 AIC_IDCR; 	// Interrupt Disable Command Register
	AT91_REG	 AIC_ICCR; 	// Interrupt Clear Command Register
	AT91_REG	 AIC_ISCR; 	// Interrupt Set Command Register
	AT91_REG	 AIC_EOICR; 	// End of Interrupt Command Register
	AT91_REG	 AIC_SPU; 	// Spurious Vector Register
	AT91_REG	 AIC_DCR; 	// Debug Control Register (Protect)
	AT91_REG	 Reserved1[1]; 	// 
	AT91_REG	 AIC_FFER; 	// Fast Forcing Enable Register
	AT91_REG	 AIC_FFDR; 	// Fast Forcing Disable Register
	AT91_REG	 AIC_FFSR; 	// Fast Forcing Status Register
} AT91S_AIC, *AT91PS_AIC;

// -------- AIC_SMR : (AIC Offset: 0x0) Control Register -------- 
#define	AT91C_AIC_PRIOR       (0x7u <<  0) // (AIC) Priority Level
#define		AT91C_AIC_PRIOR_LOWEST               0x0u // (AIC) Lowest priority level
#define		AT91C_AIC_PRIOR_HIGHEST              0x7u // (AIC) Highest priority level
#define	AT91C_AIC_SRCTYPE     (0x3u <<  5) // (AIC) Interrupt Source Type
#define		AT91C_AIC_SRCTYPE_INT_LEVEL_SENSITIVE  (0x0u <<  5) // (AIC) Internal Sources Code Label Level Sensitive
#define		AT91C_AIC_SRCTYPE_INT_EDGE_TRIGGERED   (0x1u <<  5) // (AIC) Internal Sources Code Label Edge triggered
#define		AT91C_AIC_SRCTYPE_EXT_HIGH_LEVEL       (0x2u <<  5) // (AIC) External Sources Code Label High-level Sensitive
#define		AT91C_AIC_SRCTYPE_EXT_POSITIVE_EDGE    (0x3u <<  5) // (AIC) External Sources Code Label Positive Edge triggered
// -------- AIC_CISR : (AIC Offset: 0x114) AIC Core Interrupt Status Register -------- 
#define	AT91C_AIC_NFIQ        (0x1u <<  0) // (AIC) NFIQ Status
#define	AT91C_AIC_NIRQ        (0x1u <<  1) // (AIC) NIRQ Status
// -------- AIC_DCR : (AIC Offset: 0x138) AIC Debug Control Register (Protect) -------- 
#define	AT91C_AIC_DCR_PROT    (0x1u <<  0) // (AIC) Protection Mode
#define	AT91C_AIC_DCR_GMSK    (0x1u <<  1) // (AIC) General Mask

// *****************************************************************************
//              SOFTWARE API DEFINITION  FOR Serial Parallel Interface
// *****************************************************************************
typedef struct _AT91S_SPI {
	AT91_REG	 SPI_CR; 	// Control Register
	AT91_REG	 SPI_MR; 	// Mode Register
	AT91_REG	 SPI_RDR; 	// Receive Data Register
	AT91_REG	 SPI_TDR; 	// Transmit Data Register
	AT91_REG	 SPI_SR; 	// Status Register
	AT91_REG	 SPI_IER; 	// Interrupt Enable Register
	AT91_REG	 SPI_IDR; 	// Interrupt Disable Register
	AT91_REG	 SPI_IMR; 	// Interrupt Mask Register
	AT91_REG	 Reserved0[4]; 	// 
	AT91_REG	 SPI_CSR[4]; 	// Chip Select Register
	AT91_REG	 Reserved1[48]; 	// 
	AT91_REG	 SPI_RPR; 	// Receive Pointer Register
	AT91_REG	 SPI_RCR; 	// Receive Counter Register
	AT91_REG	 SPI_TPR; 	// Transmit Pointer Register
	AT91_REG	 SPI_TCR; 	// Transmit Counter Register
	AT91_REG	 SPI_RNPR; 	// Receive Next Pointer Register
	AT91_REG	 SPI_RNCR; 	// Receive Next Counter Register
	AT91_REG	 SPI_TNPR; 	// Transmit Next Pointer Register
	AT91_REG	 SPI_TNCR; 	// Transmit Next Counter Register
	AT91_REG	 SPI_PTCR; 	// PDC Transfer Control Register
	AT91_REG	 SPI_PTSR; 	// PDC Transfer Status Register
} AT91S_SPI, *AT91PS_SPI;

// -------- SPI_CR : (SPI Offset: 0x0) SPI Control Register -------- 
#define	AT91C_SPI_SPIEN       (0x1u <<  0) // (SPI) SPI Enable
#define	AT91C_SPI_SPIDIS      (0x1u <<  1) // (SPI) SPI Disable
#define	AT91C_SPI_SWRST       (0x1u <<  7) // (SPI) SPI Software reset
// -------- SPI_MR : (SPI Offset: 0x4) SPI Mode Register -------- 
#define	AT91C_SPI_MSTR        (0x1u <<  0) // (SPI) Master/Slave Mode
#define	AT91C_SPI_PS          (0x1u <<  1) // (SPI) Peripheral Select
#define		AT91C_SPI_PS_FIXED                (0x0u <<  1) // (SPI) Fixed Peripheral Select
#define		AT91C_SPI_PS_VARIABLE             (0x1u <<  1) // (SPI) Variable Peripheral Select
#define	AT91C_SPI_PCSDEC      (0x1u <<  2) // (SPI) Chip Select Decode
#define	AT91C_SPI_DIV32       (0x1u <<  3) // (SPI) Clock Selection
#define	AT91C_SPI_MODFDIS     (0x1u <<  4) // (SPI) Mode Fault Detection
#define	AT91C_SPI_LLB         (0x1u <<  7) // (SPI) Clock Selection
#define	AT91C_SPI_PCS         (0xFu << 16) // (SPI) Peripheral Chip Select
#define	AT91C_SPI_DLYBCS      (0xFFu << 24) // (SPI) Delay Between Chip Selects
// -------- SPI_RDR : (SPI Offset: 0x8) Receive Data Register -------- 
#define	AT91C_SPI_RD          (0xFFFFu <<  0) // (SPI) Receive Data
#define	AT91C_SPI_RPCS        (0xFu << 16) // (SPI) Peripheral Chip Select Status
// -------- SPI_TDR : (SPI Offset: 0xc) Transmit Data Register -------- 
#define	AT91C_SPI_TD          (0xFFFFu <<  0) // (SPI) Transmit Data
#define	AT91C_SPI_TPCS        (0xFu << 16) // (SPI) Peripheral Chip Select Status
// -------- SPI_SR : (SPI Offset: 0x10) Status Register -------- 
#define	AT91C_SPI_RDRF        (0x1u <<  0) // (SPI) Receive Data Register Full
#define	AT91C_SPI_TDRE        (0x1u <<  1) // (SPI) Transmit Data Register Empty
#define	AT91C_SPI_MODF        (0x1u <<  2) // (SPI) Mode Fault Error
#define	AT91C_SPI_OVRES       (0x1u <<  3) // (SPI) Overrun Error Status
#define	AT91C_SPI_SPENDRX     (0x1u <<  4) // (SPI) End of Receiver Transfer
#define	AT91C_SPI_SPENDTX     (0x1u <<  5) // (SPI) End of Receiver Transfer
#define	AT91C_SPI_RXBUFF      (0x1u <<  6) // (SPI) RXBUFF Interrupt
#define	AT91C_SPI_TXBUFE      (0x1u <<  7) // (SPI) TXBUFE Interrupt
#define	AT91C_SPI_SPIENS      (0x1u << 16) // (SPI) Enable Status
// -------- SPI_IER : (SPI Offset: 0x14) Interrupt Enable Register -------- 
// -------- SPI_IDR : (SPI Offset: 0x18) Interrupt Disable Register -------- 
// -------- SPI_IMR : (SPI Offset: 0x1c) Interrupt Mask Register -------- 
// -------- SPI_CSR : (SPI Offset: 0x30) Chip Select Register -------- 
#define	AT91C_SPI_CPOL        (0x1u <<  0) // (SPI) Clock Polarity
#define	AT91C_SPI_NCPHA       (0x1u <<  1) // (SPI) Clock Phase
#define	AT91C_SPI_BITS        (0xFu <<  4) // (SPI) Bits Per Transfer
#define		AT91C_SPI_BITS_8                    (0x0u <<  4) // (SPI) 8 Bits Per transfer
#define		AT91C_SPI_BITS_9                    (0x1u <<  4) // (SPI) 9 Bits Per transfer
#define		AT91C_SPI_BITS_10                   (0x2u <<  4) // (SPI) 10 Bits Per transfer
#define		AT91C_SPI_BITS_11                   (0x3u <<  4) // (SPI) 11 Bits Per transfer
#define		AT91C_SPI_BITS_12                   (0x4u <<  4) // (SPI) 12 Bits Per transfer
#define		AT91C_SPI_BITS_13                   (0x5u <<  4) // (SPI) 13 Bits Per transfer
#define		AT91C_SPI_BITS_14                   (0x6u <<  4) // (SPI) 14 Bits Per transfer
#define		AT91C_SPI_BITS_15                   (0x7u <<  4) // (SPI) 15 Bits Per transfer
#define		AT91C_SPI_BITS_16                   (0x8u <<  4) // (SPI) 16 Bits Per transfer
#define	AT91C_SPI_SCBR        (0xFFu <<  8) // (SPI) Serial Clock Baud Rate
#define	AT91C_SPI_DLYBS       (0xFFu << 16) // (SPI) Serial Clock Baud Rate
#define	AT91C_SPI_DLYBCT      (0xFFu << 24) // (SPI) Delay Between Consecutive Transfers

// *****************************************************************************
//              SOFTWARE API DEFINITION  FOR Synchronous Serial Controller Interface
// *****************************************************************************
typedef struct _AT91S_SSC {
	AT91_REG	 SSC_CR; 	// Control Register
	AT91_REG	 SSC_CMR; 	// Clock Mode Register
	AT91_REG	 Reserved0[2]; 	// 
	AT91_REG	 SSC_RCMR; 	// Receive Clock ModeRegister
	AT91_REG	 SSC_RFMR; 	// Receive Frame Mode Register
	AT91_REG	 SSC_TCMR; 	// Transmit Clock Mode Register
	AT91_REG	 SSC_TFMR; 	// Transmit Frame Mode Register
	AT91_REG	 SSC_RHR; 	// Receive Holding Register
	AT91_REG	 SSC_THR; 	// Transmit Holding Register
	AT91_REG	 Reserved1[2]; 	// 
	AT91_REG	 SSC_RSHR; 	// Receive Sync Holding Register
	AT91_REG	 SSC_TSHR; 	// Transmit Sync Holding Register
	AT91_REG	 SSC_RC0R; 	// Receive Compare 0 Register
	AT91_REG	 SSC_RC1R; 	// Receive Compare 1 Register
	AT91_REG	 SSC_SR; 	// Status Register
	AT91_REG	 SSC_IER; 	// Interrupt Enable Register
	AT91_REG	 SSC_IDR; 	// Interrupt Disable Register
	AT91_REG	 SSC_IMR; 	// Interrupt Mask Register
	AT91_REG	 Reserved2[44]; 	// 
	AT91_REG	 SSC_RPR; 	// Receive Pointer Register
	AT91_REG	 SSC_RCR; 	// Receive Counter Register
	AT91_REG	 SSC_TPR; 	// Transmit Pointer Register
	AT91_REG	 SSC_TCR; 	// Transmit Counter Register
	AT91_REG	 SSC_RNPR; 	// Receive Next Pointer Register
	AT91_REG	 SSC_RNCR; 	// Receive Next Counter Register
	AT91_REG	 SSC_TNPR; 	// Transmit Next Pointer Register
	AT91_REG	 SSC_TNCR; 	// Transmit Next Counter Register
	AT91_REG	 SSC_PTCR; 	// PDC Transfer Control Register
	AT91_REG	 SSC_PTSR; 	// PDC Transfer Status Register
} AT91S_SSC, *AT91PS_SSC;

// -------- SSC_CR : (SSC Offset: 0x0) SSC Control Register -------- 
#define	AT91C_SSC_RXEN        (0x1u <<  0) // (SSC) Receive Enable
#define	AT91C_SSC_RXDIS       (0x1u <<  1) // (SSC) Receive Disable
#define	AT91C_SSC_TXEN        (0x1u <<  8) // (SSC) Transmit Enable
#define	AT91C_SSC_TXDIS       (0x1u <<  9) // (SSC) Transmit Disable
#define	AT91C_SSC_SWRST       (0x1u << 15) // (SSC) Software Reset
// -------- SSC_RCMR : (SSC Offset: 0x10) SSC Receive Clock Mode Register -------- 
#define	AT91C_SSC_CKS         (0x3u <<  0) // (SSC) Receive/Transmit Clock Selection
#define		AT91C_SSC_CKS_DIV                  0x0u // (SSC) Divided Clock
#define		AT91C_SSC_CKS_TK                   0x1u // (SSC) TK Clock signal
#define		AT91C_SSC_CKS_RK                   0x2u // (SSC) RK pin
#define	AT91C_SSC_CKO         (0x7u <<  2) // (SSC) Receive/Transmit Clock Output Mode Selection
#define		AT91C_SSC_CKO_NONE                 (0x0u <<  2) // (SSC) Receive/Transmit Clock Output Mode: None RK pin: Input-only
#define		AT91C_SSC_CKO_CONTINOUS            (0x1u <<  2) // (SSC) Continuous Receive/Transmit Clock RK pin: Output
#define		AT91C_SSC_CKO_DATA_TX              (0x2u <<  2) // (SSC) Receive/Transmit Clock only during data transfers RK pin: Output
#define	AT91C_SSC_CKI         (0x1u <<  5) // (SSC) Receive/Transmit Clock Inversion
#define	AT91C_SSC_CKG         (0x3u <<  6) // (SSC) Receive/Transmit Clock Gating Selection
#define		AT91C_SSC_CKG_NONE                 (0x0u <<  6) // (SSC) Receive/Transmit Clock Gating: None, continuous clock
#define		AT91C_SSC_CKG_LOW                  (0x1u <<  6) // (SSC) Receive/Transmit Clock enabled only if RF Low
#define		AT91C_SSC_CKG_HIGH                 (0x2u <<  6) // (SSC) Receive/Transmit Clock enabled only if RF High
#define	AT91C_SSC_START       (0xFu <<  8) // (SSC) Receive/Transmit Start Selection
#define		AT91C_SSC_START_CONTINOUS            (0x0u <<  8) // (SSC) Continuous, as soon as the receiver is enabled, and immediately after the end of transfer of the previous data.
#define		AT91C_SSC_START_TX                   (0x1u <<  8) // (SSC) Transmit/Receive start
#define		AT91C_SSC_START_LOW_RF               (0x2u <<  8) // (SSC) Detection of a low level on RF input
#define		AT91C_SSC_START_HIGH_RF              (0x3u <<  8) // (SSC) Detection of a high level on RF input
#define		AT91C_SSC_START_FALL_RF              (0x4u <<  8) // (SSC) Detection of a falling edge on RF input
#define		AT91C_SSC_START_RISE_RF              (0x5u <<  8) // (SSC) Detection of a rising edge on RF input
#define		AT91C_SSC_START_LEVEL_RF             (0x6u <<  8) // (SSC) Detection of any level change on RF input
#define		AT91C_SSC_START_EDGE_RF              (0x7u <<  8) // (SSC) Detection of any edge on RF input
#define		AT91C_SSC_START_0                    (0x8u <<  8) // (SSC) Compare 0
#define	AT91C_SSC_STOP        (0x1u << 12) // (SSC) Receive Stop Selection
#define	AT91C_SSC_STTOUT      (0x1u << 15) // (SSC) Receive/Transmit Start Output Selection
#define	AT91C_SSC_STTDLY      (0xFFu << 16) // (SSC) Receive/Transmit Start Delay
#define	AT91C_SSC_PERIOD      (0xFFu << 24) // (SSC) Receive/Transmit Period Divider Selection
// -------- SSC_RFMR : (SSC Offset: 0x14) SSC Receive Frame Mode Register -------- 
#define	AT91C_SSC_DATLEN      (0x1Fu <<  0) // (SSC) Data Length
#define	AT91C_SSC_LOOP        (0x1u <<  5) // (SSC) Loop Mode
#define	AT91C_SSC_MSBF        (0x1u <<  7) // (SSC) Most Significant Bit First
#define	AT91C_SSC_DATNB       (0xFu <<  8) // (SSC) Data Number per Frame
#define	AT91C_SSC_FSLEN       (0xFu << 16) // (SSC) Receive/Transmit Frame Sync length
#define	AT91C_SSC_FSOS        (0x7u << 20) // (SSC) Receive/Transmit Frame Sync Output Selection
#define		AT91C_SSC_FSOS_NONE                 (0x0u << 20) // (SSC) Selected Receive/Transmit Frame Sync Signal: None RK pin Input-only
#define		AT91C_SSC_FSOS_NEGATIVE             (0x1u << 20) // (SSC) Selected Receive/Transmit Frame Sync Signal: Negative Pulse
#define		AT91C_SSC_FSOS_POSITIVE             (0x2u << 20) // (SSC) Selected Receive/Transmit Frame Sync Signal: Positive Pulse
#define		AT91C_SSC_FSOS_LOW                  (0x3u << 20) // (SSC) Selected Receive/Transmit Frame Sync Signal: Driver Low during data transfer
#define		AT91C_SSC_FSOS_HIGH                 (0x4u << 20) // (SSC) Selected Receive/Transmit Frame Sync Signal: Driver High during data transfer
#define		AT91C_SSC_FSOS_TOGGLE               (0x5u << 20) // (SSC) Selected Receive/Transmit Frame Sync Signal: Toggling at each start of data transfer
#define	AT91C_SSC_FSEDGE      (0x1u << 24) // (SSC) Frame Sync Edge Detection
// -------- SSC_TCMR : (SSC Offset: 0x18) SSC Transmit Clock Mode Register -------- 
// -------- SSC_TFMR : (SSC Offset: 0x1c) SSC Transmit Frame Mode Register -------- 
#define	AT91C_SSC_DATDEF      (0x1u <<  5) // (SSC) Data Default Value
#define	AT91C_SSC_FSDEN       (0x1u << 23) // (SSC) Frame Sync Data Enable
// -------- SSC_SR : (SSC Offset: 0x40) SSC Status Register -------- 
#define	AT91C_SSC_TXRDY       (0x1u <<  0) // (SSC) Transmit Ready
#define	AT91C_SSC_TXEMPTY     (0x1u <<  1) // (SSC) Transmit Empty
#define	AT91C_SSC_ENDTX       (0x1u <<  2) // (SSC) End Of Transmission
#define	AT91C_SSC_TXBUFE      (0x1u <<  3) // (SSC) Transmit Buffer Empty
#define	AT91C_SSC_RXRDY       (0x1u <<  4) // (SSC) Receive Ready
#define	AT91C_SSC_OVRUN       (0x1u <<  5) // (SSC) Receive Overrun
#define	AT91C_SSC_ENDRX       (0x1u <<  6) // (SSC) End of Reception
#define	AT91C_SSC_RXBUFF      (0x1u <<  7) // (SSC) Receive Buffer Full
#define	AT91C_SSC_CP0         (0x1u <<  8) // (SSC) Compare 0
#define	AT91C_SSC_CP1         (0x1u <<  9) // (SSC) Compare 1
#define	AT91C_SSC_TXSYN       (0x1u << 10) // (SSC) Transmit Sync
#define	AT91C_SSC_RXSYN       (0x1u << 11) // (SSC) Receive Sync
#define	AT91C_SSC_TXENA       (0x1u << 16) // (SSC) Transmit Enable
#define	AT91C_SSC_RXENA       (0x1u << 17) // (SSC) Receive Enable
// -------- SSC_IER : (SSC Offset: 0x44) SSC Interrupt Enable Register -------- 
// -------- SSC_IDR : (SSC Offset: 0x48) SSC Interrupt Disable Register -------- 
// -------- SSC_IMR : (SSC Offset: 0x4c) SSC Interrupt Mask Register -------- 

// *****************************************************************************
//              SOFTWARE API DEFINITION  FOR Usart
// *****************************************************************************
typedef struct _AT91S_USART {
	AT91_REG	 US_CR; 	// Control Register
	AT91_REG	 US_MR; 	// Mode Register
	AT91_REG	 US_IER; 	// Interrupt Enable Register
	AT91_REG	 US_IDR; 	// Interrupt Disable Register
	AT91_REG	 US_IMR; 	// Interrupt Mask Register
	AT91_REG	 US_CSR; 	// Channel Status Register
	AT91_REG	 US_RHR; 	// Receiver Holding Register
	AT91_REG	 US_THR; 	// Transmitter Holding Register
	AT91_REG	 US_BRGR; 	// Baud Rate Generator Register
	AT91_REG	 US_RTOR; 	// Receiver Time-out Register
	AT91_REG	 US_TTGR; 	// Transmitter Time-guard Register
	AT91_REG	 Reserved0[5]; 	// 
	AT91_REG	 US_FIDI; 	// FI_DI_Ratio Register
	AT91_REG	 US_NER; 	// Nb Errors Register
	AT91_REG	 US_XXR; 	// XON_XOFF Register
	AT91_REG	 US_IF; 	// IRDA_FILTER Register
	AT91_REG	 Reserved1[44]; 	// 
	AT91_REG	 US_RPR; 	// Receive Pointer Register
	AT91_REG	 US_RCR; 	// Receive Counter Register
	AT91_REG	 US_TPR; 	// Transmit Pointer Register
	AT91_REG	 US_TCR; 	// Transmit Counter Register
	AT91_REG	 US_RNPR; 	// Receive Next Pointer Register
	AT91_REG	 US_RNCR; 	// Receive Next Counter Register
	AT91_REG	 US_TNPR; 	// Transmit Next Pointer Register
	AT91_REG	 US_TNCR; 	// Transmit Next Counter Register
	AT91_REG	 US_PTCR; 	// PDC Transfer Control Register
	AT91_REG	 US_PTSR; 	// PDC Transfer Status Register
} AT91S_USART, *AT91PS_USART;

// -------- US_CR : (USART Offset: 0x0) Debug Unit Control Register -------- 
#define	AT91C_US_RSTSTA       (0x1u <<  8) // (USART) Reset Status Bits
#define	AT91C_US_STTBRK       (0x1u <<  9) // (USART) Start Break
#define	AT91C_US_STPBRK       (0x1u << 10) // (USART) Stop Break
#define	AT91C_US_STTTO        (0x1u << 11) // (USART) Start Time-out
#define	AT91C_US_SENDA        (0x1u << 12) // (USART) Send Address
#define	AT91C_US_RSTIT        (0x1u << 13) // (USART) Reset Iterations
#define	AT91C_US_RSTNACK      (0x1u << 14) // (USART) Reset Non Acknowledge
#define	AT91C_US_RETTO        (0x1u << 15) // (USART) Rearm Time-out
#define	AT91C_US_DTREN        (0x1u << 16) // (USART) Data Terminal ready Enable
#define	AT91C_US_DTRDIS       (0x1u << 17) // (USART) Data Terminal ready Disable
#define	AT91C_US_RTSEN        (0x1u << 18) // (USART) Request to Send enable
#define	AT91C_US_RTSDIS       (0x1u << 19) // (USART) Request to Send Disable
// -------- US_MR : (USART Offset: 0x4) Debug Unit Mode Register -------- 
#define	AT91C_US_USMODE       (0xFu <<  0) // (USART) Usart mode
#define		AT91C_US_USMODE_NORMAL               0x0u // (USART) Normal
#define		AT91C_US_USMODE_RS485                0x1u // (USART) RS485
#define		AT91C_US_USMODE_HWHSH                0x2u // (USART) Hardware Handshaking
#define		AT91C_US_USMODE_MODEM                0x3u // (USART) Modem
#define		AT91C_US_USMODE_ISO7816_0            0x4u // (USART) ISO7816 protocol: T = 0
#define		AT91C_US_USMODE_ISO7816_1            0x6u // (USART) ISO7816 protocol: T = 1
#define		AT91C_US_USMODE_IRDA                 0x8u // (USART) IrDA
#define		AT91C_US_USMODE_SWHSH                0xCu // (USART) Software Handshaking
#define	AT91C_US_CLKS         (0x3u <<  4) // (USART) Clock Selection (Baud Rate generator Input Clock
#define		AT91C_US_CLKS_CLOCK                (0x0u <<  4) // (USART) Clock
#define		AT91C_US_CLKS_FDIV1                (0x1u <<  4) // (USART) fdiv1
#define		AT91C_US_CLKS_SLOW                 (0x2u <<  4) // (USART) slow_clock (ARM)
#define		AT91C_US_CLKS_EXT                  (0x3u <<  4) // (USART) External (SCK)
#define	AT91C_US_CHRL         (0x3u <<  6) // (USART) Clock Selection (Baud Rate generator Input Clock
#define		AT91C_US_CHRL_5_BITS               (0x0u <<  6) // (USART) Character Length: 5 bits
#define		AT91C_US_CHRL_6_BITS               (0x1u <<  6) // (USART) Character Length: 6 bits
#define		AT91C_US_CHRL_7_BITS               (0x2u <<  6) // (USART) Character Length: 7 bits
#define		AT91C_US_CHRL_8_BITS               (0x3u <<  6) // (USART) Character Length: 8 bits
#define	AT91C_US_SYNC         (0x1u <<  8) // (USART) Synchronous Mode Select
#define	AT91C_US_NBSTOP       (0x3u << 12) // (USART) Number of Stop bits
#define		AT91C_US_NBSTOP_1_BIT                (0x0u << 12) // (USART) 1 stop bit
#define		AT91C_US_NBSTOP_15_BIT               (0x1u << 12) // (USART) Asynchronous (SYNC=0) 2 stop bits Synchronous (SYNC=1) 2 stop bits
#define		AT91C_US_NBSTOP_2_BIT                (0x2u << 12) // (USART) 2 stop bits
#define	AT91C_US_MSBF         (0x1u << 16) // (USART) Bit Order
#define	AT91C_US_MODE9        (0x1u << 17) // (USART) 9-bit Character length
#define	AT91C_US_CKLO         (0x1u << 18) // (USART) Clock Output Select
#define	AT91C_US_OVER         (0x1u << 19) // (USART) Over Sampling Mode
#define	AT91C_US_INACK        (0x1u << 20) // (USART) Inhibit Non Acknowledge
#define	AT91C_US_DSNACK       (0x1u << 21) // (USART) Disable Successive NACK
#define	AT91C_US_MAX_ITER     (0x1u << 24) // (USART) Number of Repetitions
#define	AT91C_US_FILTER       (0x1u << 28) // (USART) Receive Line Filter
// -------- US_IER : (USART Offset: 0x8) Debug Unit Interrupt Enable Register -------- 
#define	AT91C_US_RXBRK        (0x1u <<  2) // (USART) Break Received/End of Break
#define	AT91C_US_TIMEOUT      (0x1u <<  8) // (USART) Receiver Time-out
#define	AT91C_US_ITERATION    (0x1u << 10) // (USART) Max number of Repetitions Reached
#define	AT91C_US_NACK         (0x1u << 13) // (USART) Non Acknowledge
#define	AT91C_US_RIIC         (0x1u << 16) // (USART) Ring INdicator Input Change Flag
#define	AT91C_US_DSRIC        (0x1u << 17) // (USART) Data Set Ready Input Change Flag
#define	AT91C_US_DCDIC        (0x1u << 18) // (USART) Data Carrier Flag
#define	AT91C_US_CTSIC        (0x1u << 19) // (USART) Clear To Send Input Change Flag
// -------- US_IDR : (USART Offset: 0xc) Debug Unit Interrupt Disable Register -------- 
// -------- US_IMR : (USART Offset: 0x10) Debug Unit Interrupt Mask Register -------- 
// -------- US_CSR : (USART Offset: 0x14) Debug Unit Channel Status Register -------- 
#define	AT91C_US_RI           (0x1u << 20) // (USART) Image of RI Input
#define	AT91C_US_DSR          (0x1u << 21) // (USART) Image of DSR Input
#define	AT91C_US_DCD          (0x1u << 22) // (USART) Image of DCD Input
#define	AT91C_US_CTS          (0x1u << 23) // (USART) Image of CTS Input

// *****************************************************************************
//              SOFTWARE API DEFINITION  FOR Two-wire Interface
// *****************************************************************************
typedef struct _AT91S_TWI {
	AT91_REG	 TWI_CR; 	// Control Register
	AT91_REG	 TWI_MMR; 	// Master Mode Register
	AT91_REG	 TWI_SMR; 	// Slave Mode Register
	AT91_REG	 TWI_IADR; 	// Internal Address Register
	AT91_REG	 TWI_CWGR; 	// Clock Waveform Generator Register
	AT91_REG	 Reserved0[3]; 	// 
	AT91_REG	 TWI_SR; 	// Status Register
	AT91_REG	 TWI_IER; 	// Interrupt Enable Register
	AT91_REG	 TWI_IDR; 	// Interrupt Disable Register
	AT91_REG	 TWI_IMR; 	// Interrupt Mask Register
	AT91_REG	 TWI_RHR; 	// Receive Holding Register
	AT91_REG	 TWI_THR; 	// Transmit Holding Register
} AT91S_TWI, *AT91PS_TWI;

// -------- TWI_CR : (TWI Offset: 0x0) TWI Control Register -------- 
#define	AT91C_TWI_START       (0x1u <<  0) // (TWI) Send a START Condition
#define	AT91C_TWI_STOP        (0x1u <<  1) // (TWI) Send a STOP Condition
#define	AT91C_TWI_MSEN        (0x1u <<  2) // (TWI) TWI Master Transfer Enabled
#define	AT91C_TWI_MSDIS       (0x1u <<  3) // (TWI) TWI Master Transfer Disabled
#define	AT91C_TWI_SVEN        (0x1u <<  4) // (TWI) TWI Slave Transfer Enabled
#define	AT91C_TWI_SVDIS       (0x1u <<  5) // (TWI) TWI Slave Transfer Disabled
#define	AT91C_TWI_SWRST       (0x1u <<  7) // (TWI) Software Reset
// -------- TWI_MMR : (TWI Offset: 0x4) TWI Master Mode Register -------- 
#define	AT91C_TWI_IADRSZ      (0x3u <<  8) // (TWI) Internal Device Address Size
#define		AT91C_TWI_IADRSZ_NO                   (0x0u <<  8) // (TWI) No internal device address
#define		AT91C_TWI_IADRSZ_1_BYTE               (0x1u <<  8) // (TWI) One-byte internal device address
#define		AT91C_TWI_IADRSZ_2_BYTE               (0x2u <<  8) // (TWI) Two-byte internal device address
#define		AT91C_TWI_IADRSZ_3_BYTE               (0x3u <<  8) // (TWI) Three-byte internal device address
#define	AT91C_TWI_MREAD       (0x1u << 12) // (TWI) Master Read Direction
#define	AT91C_TWI_DADR        (0x7Fu << 16) // (TWI) Device Address
// -------- TWI_SMR : (TWI Offset: 0x8) TWI Slave Mode Register -------- 
#define	AT91C_TWI_SADR        (0x7Fu << 16) // (TWI) Slave Device Address
// -------- TWI_CWGR : (TWI Offset: 0x10) TWI Clock Waveform Generator Register -------- 
#define	AT91C_TWI_CLDIV       (0xFFu <<  0) // (TWI) Clock Low Divider
#define	AT91C_TWI_CHDIV       (0xFFu <<  8) // (TWI) Clock High Divider
#define	AT91C_TWI_CKDIV       (0x7u << 16) // (TWI) Clock Divider
// -------- TWI_SR : (TWI Offset: 0x20) TWI Status Register -------- 
#define	AT91C_TWI_TXCOMP      (0x1u <<  0) // (TWI) Transmission Completed
#define	AT91C_TWI_RXRDY       (0x1u <<  1) // (TWI) Receive holding register ReaDY
#define	AT91C_TWI_TXRDY       (0x1u <<  2) // (TWI) Transmit holding register ReaDY
#define	AT91C_TWI_SVREAD      (0x1u <<  3) // (TWI) Slave Read
#define	AT91C_TWI_SVACC       (0x1u <<  4) // (TWI) Slave Access
#define	AT91C_TWI_GCACC       (0x1u <<  5) // (TWI) General Call Access
#define	AT91C_TWI_OVRE        (0x1u <<  6) // (TWI) Overrun Error
#define	AT91C_TWI_UNRE        (0x1u <<  7) // (TWI) Underrun Error
#define	AT91C_TWI_NACK        (0x1u <<  8) // (TWI) Not Acknowledged
#define	AT91C_TWI_ARBLST      (0x1u <<  9) // (TWI) Arbitration Lost
// -------- TWI_IER : (TWI Offset: 0x24) TWI Interrupt Enable Register -------- 
// -------- TWI_IDR : (TWI Offset: 0x28) TWI Interrupt Disable Register -------- 
// -------- TWI_IMR : (TWI Offset: 0x2c) TWI Interrupt Mask Register -------- 

// *****************************************************************************
//              SOFTWARE API DEFINITION  FOR Multimedia Card Interface
// *****************************************************************************
typedef struct _AT91S_MCI {
	AT91_REG	 MCI_CR; 	// MCI Control Register
	AT91_REG	 MCI_MR; 	// MCI Mode Register
	AT91_REG	 MCI_DTOR; 	// MCI Data Timeout Register
	AT91_REG	 MCI_SDCR; 	// MCI SD Card Register
	AT91_REG	 MCI_ARGR; 	// MCI Argument Register
	AT91_REG	 MCI_CMDR; 	// MCI Command Register
	AT91_REG	 Reserved0[2]; 	// 
	AT91_REG	 MCI_RSPR[4]; 	// MCI Response Register
	AT91_REG	 MCI_RDR; 	// MCI Receive Data Register
	AT91_REG	 MCI_TDR; 	// MCI Transmit Data Register
	AT91_REG	 Reserved1[2]; 	// 
	AT91_REG	 MCI_SR; 	// MCI Status Register
	AT91_REG	 MCI_IER; 	// MCI Interrupt Enable Register
	AT91_REG	 MCI_IDR; 	// MCI Interrupt Disable Register
	AT91_REG	 MCI_IMR; 	// MCI Interrupt Mask Register
	AT91_REG	 Reserved2[44]; 	// 
	AT91_REG	 MCI_RPR; 	// Receive Pointer Register
	AT91_REG	 MCI_RCR; 	// Receive Counter Register
	AT91_REG	 MCI_TPR; 	// Transmit Pointer Register
	AT91_REG	 MCI_TCR; 	// Transmit Counter Register
	AT91_REG	 MCI_RNPR; 	// Receive Next Pointer Register
	AT91_REG	 MCI_RNCR; 	// Receive Next Counter Register
	AT91_REG	 MCI_TNPR; 	// Transmit Next Pointer Register
	AT91_REG	 MCI_TNCR; 	// Transmit Next Counter Register
	AT91_REG	 MCI_PTCR; 	// PDC Transfer Control Register
	AT91_REG	 MCI_PTSR; 	// PDC Transfer Status Register
} AT91S_MCI, *AT91PS_MCI;

// -------- MCI_CR : (MCI Offset: 0x0) MCI Control Register -------- 
#define	AT91C_MCI_MCIEN       (0x1u <<  0) // (MCI) Multimedia Interface Enable
#define	AT91C_MCI_MCIDIS      (0x1u <<  1) // (MCI) Multimedia Interface Disable
#define	AT91C_MCI_PWSEN       (0x1u <<  2) // (MCI) Power Save Mode Enable
#define	AT91C_MCI_PWSDIS      (0x1u <<  3) // (MCI) Power Save Mode Disable
// -------- MCI_MR : (MCI Offset: 0x4) MCI Mode Register -------- 
#define	AT91C_MCI_CLKDIV      (0x1u <<  0) // (MCI) Clock Divider
#define	AT91C_MCI_PWSDIV      (0x1u <<  8) // (MCI) Power Saving Divider
#define	AT91C_MCI_PDCPADV     (0x1u << 14) // (MCI) PDC Padding Value
#define	AT91C_MCI_PDCMODE     (0x1u << 15) // (MCI) PDC Oriented Mode
#define	AT91C_MCI_BLKLEN      (0x1u << 18) // (MCI) Data Block Length
// -------- MCI_DTOR : (MCI Offset: 0x8) MCI Data Timeout Register -------- 
#define	AT91C_MCI_DTOCYC      (0x1u <<  0) // (MCI) Data Timeout Cycle Number
#define	AT91C_MCI_DTOMUL      (0x7u <<  4) // (MCI) Data Timeout Multiplier
#define		AT91C_MCI_DTOMUL_1                    (0x0u <<  4) // (MCI) DTOCYC x 1
#define		AT91C_MCI_DTOMUL_16                   (0x1u <<  4) // (MCI) DTOCYC x 16
#define		AT91C_MCI_DTOMUL_128                  (0x2u <<  4) // (MCI) DTOCYC x 128
#define		AT91C_MCI_DTOMUL_256                  (0x3u <<  4) // (MCI) DTOCYC x 256
#define		AT91C_MCI_DTOMUL_1024                 (0x4u <<  4) // (MCI) DTOCYC x 1024
#define		AT91C_MCI_DTOMUL_4096                 (0x5u <<  4) // (MCI) DTOCYC x 4096
#define		AT91C_MCI_DTOMUL_65536                (0x6u <<  4) // (MCI) DTOCYC x 65536
#define		AT91C_MCI_DTOMUL_1048576              (0x7u <<  4) // (MCI) DTOCYC x 1048576
// -------- MCI_SDCR : (MCI Offset: 0xc) MCI SD Card Register -------- 
#define	AT91C_MCI_SCDSEL      (0x1u <<  0) // (MCI) SD Card Selector
#define	AT91C_MCI_SCDBUS      (0x1u <<  7) // (MCI) SD Card Bus Width
// -------- MCI_CMDR : (MCI Offset: 0x14) MCI Command Register -------- 
// -------- MCI_SR : (MCI Offset: 0x40) MCI Status Register -------- 
#define	AT91C_MCI_CMDRDY      (0x1u <<  0) // (MCI) Command Ready flag
#define	AT91C_MCI_RXRDY       (0x1u <<  1) // (MCI) RX Ready flag
#define	AT91C_MCI_TXRDY       (0x1u <<  2) // (MCI) TX Ready flag
#define	AT91C_MCI_BLKE        (0x1u <<  3) // (MCI) Data Block Transfer Ended flag
#define	AT91C_MCI_DTIP        (0x1u <<  4) // (MCI) Data Transfer in Progress flag
#define	AT91C_MCI_NOTBUSY     (0x1u <<  5) // (MCI) Data Line Not Busy flag
#define	AT91C_MCI_ENDRX       (0x1u <<  6) // (MCI) End of RX Buffer flag
#define	AT91C_MCI_ENDTX       (0x1u <<  7) // (MCI) End of TX Buffer flag
#define	AT91C_MCI_RXBUFF      (0x1u << 14) // (MCI) RX Buffer Full flag
#define	AT91C_MCI_TXBUFE      (0x1u << 15) // (MCI) TX Buffer Empty flag
#define	AT91C_MCI_RINDE       (0x1u << 16) // (MCI) Response Index Error flag
#define	AT91C_MCI_RDIRE       (0x1u << 17) // (MCI) Response Direction Error flag
#define	AT91C_MCI_RCRCE       (0x1u << 18) // (MCI) Response CRC Error flag
#define	AT91C_MCI_RENDE       (0x1u << 19) // (MCI) Response End Bit Error flag
#define	AT91C_MCI_RTOE        (0x1u << 20) // (MCI) Response Time-out Error flag
#define	AT91C_MCI_DCRCE       (0x1u << 21) // (MCI) data CRC Error flag
#define	AT91C_MCI_DTOE        (0x1u << 22) // (MCI) Data timeout Error flag
#define	AT91C_MCI_OVRE        (0x1u << 30) // (MCI) Overrun flag
#define	AT91C_MCI_UNRE        (0x1u << 31) // (MCI) Underrun flag
// -------- MCI_IER : (MCI Offset: 0x44) MCI Interrupt Enable Register -------- 
// -------- MCI_IDR : (MCI Offset: 0x48) MCI Interrupt Disable Register -------- 
// -------- MCI_IMR : (MCI Offset: 0x4c) MCI Interrupt Mask Register -------- 

// *****************************************************************************
//              SOFTWARE API DEFINITION  FOR USB Device Interface
// *****************************************************************************
typedef struct _AT91S_UDP {
	AT91_REG	 UDP_NUM; 	// Frame Number Register
	AT91_REG	 UDP_GLBSTATE; 	// Global State Register
	AT91_REG	 UDP_FADDR; 	// Function Address Register
	AT91_REG	 Reserved0[1]; 	// 
	AT91_REG	 UDP_IER; 	// Interrupt Enable Register
	AT91_REG	 UDP_IDR; 	// Interrupt Disable Register
	AT91_REG	 UDP_IMR; 	// Interrupt Mask Register
	AT91_REG	 UDP_ISR; 	// Interrupt Status Register
	AT91_REG	 UDP_ICR; 	// Interrupt Clear Register
	AT91_REG	 Reserved1[1]; 	// 
	AT91_REG	 UDP_RSTEP; 	// Reset Endpoint Register
	AT91_REG	 Reserved2[1]; 	// 
	AT91_REG	 UDP_CSR[8]; 	// Endpoint Control and Status Register
	AT91_REG	 UDP_FDR[8]; 	// Endpoint FIFO Data Register
} AT91S_UDP, *AT91PS_UDP;

// -------- UDP_FRM_NUM : (UDP Offset: 0x0) USB Frame Number Register -------- 
#define	AT91C_UDP_FRM_NUM     (0x7FFu <<  0) // (UDP) Frame Number as Defined in the Packet Field Formats
#define	AT91C_UDP_FRM_ERR     (0x1u << 16) // (UDP) Frame Error
#define	AT91C_UDP_FRM_OK      (0x1u << 17) // (UDP) Frame OK
// -------- UDP_GLB_STATE : (UDP Offset: 0x4) USB Global State Register -------- 
#define	AT91C_UDP_FADDEN      (0x1u <<  0) // (UDP) Function Address Enable
#define	AT91C_UDP_CONFG       (0x1u <<  1) // (UDP) Configured
#define	AT91C_UDP_RMWUPE      (0x1u <<  2) // (UDP) Remote Wake Up Enable
#define	AT91C_UDP_RSMINPR     (0x1u <<  3) // (UDP) A Resume Has Been Sent to the Host
// -------- UDP_FADDR : (UDP Offset: 0x8) USB Function Address Register -------- 
#define	AT91C_UDP_FADD        (0xFFu <<  0) // (UDP) Function Address Value
#define	AT91C_UDP_FEN         (0x1u <<  8) // (UDP) Function Enable
// -------- UDP_IER : (UDP Offset: 0x10) USB Interrupt Enable Register -------- 
#define	AT91C_UDP_EPINT0      (0x1u <<  0) // (UDP) Endpoint 0 Interrupt
#define	AT91C_UDP_EPINT1      (0x1u <<  1) // (UDP) Endpoint 0 Interrupt
#define	AT91C_UDP_EPINT2      (0x1u <<  2) // (UDP) Endpoint 2 Interrupt
#define	AT91C_UDP_EPINT3      (0x1u <<  3) // (UDP) Endpoint 3 Interrupt
#define	AT91C_UDP_EPINT4      (0x1u <<  4) // (UDP) Endpoint 4 Interrupt
#define	AT91C_UDP_EPINT5      (0x1u <<  5) // (UDP) Endpoint 5 Interrupt
#define	AT91C_UDP_EPINT6      (0x1u <<  6) // (UDP) Endpoint 6 Interrupt
#define	AT91C_UDP_EPINT7      (0x1u <<  7) // (UDP) Endpoint 7 Interrupt
#define	AT91C_UDP_RXSUSP      (0x1u <<  8) // (UDP) USB Suspend Interrupt
#define	AT91C_UDP_RXRSM       (0x1u <<  9) // (UDP) USB Resume Interrupt
#define	AT91C_UDP_EXTRSM      (0x1u << 10) // (UDP) USB External Resume Interrupt
#define	AT91C_UDP_SOFINT      (0x1u << 11) // (UDP) USB Start Of frame Interrupt
#define	AT91C_UDP_WAKEUP      (0x1u << 13) // (UDP) USB Resume Interrupt
// -------- UDP_IDR : (UDP Offset: 0x14) USB Interrupt Disable Register -------- 
// -------- UDP_IMR : (UDP Offset: 0x18) USB Interrupt Mask Register -------- 
// -------- UDP_ISR : (UDP Offset: 0x1c) USB Interrupt Status Register -------- 
#define	AT91C_UDP_ENDBUSRES   (0x1u << 12) // (UDP) USB End Of Bus Reset Interrupt
// -------- UDP_ICR : (UDP Offset: 0x20) USB Interrupt Clear Register -------- 
// -------- UDP_RST_EP : (UDP Offset: 0x28) USB Reset Endpoint Register -------- 
#define	AT91C_UDP_EP0         (0x1u <<  0) // (UDP) Reset Endpoint 0
#define	AT91C_UDP_EP1         (0x1u <<  1) // (UDP) Reset Endpoint 1
#define	AT91C_UDP_EP2         (0x1u <<  2) // (UDP) Reset Endpoint 2
#define	AT91C_UDP_EP3         (0x1u <<  3) // (UDP) Reset Endpoint 3
#define	AT91C_UDP_EP4         (0x1u <<  4) // (UDP) Reset Endpoint 4
#define	AT91C_UDP_EP5         (0x1u <<  5) // (UDP) Reset Endpoint 5
#define	AT91C_UDP_EP6         (0x1u <<  6) // (UDP) Reset Endpoint 6
#define	AT91C_UDP_EP7         (0x1u <<  7) // (UDP) Reset Endpoint 7
// -------- UDP_CSR : (UDP Offset: 0x30) USB Endpoint Control and Status Register -------- 
#define	AT91C_UDP_TXCOMP      (0x1u <<  0) // (UDP) Generates an IN packet with data previously written in the DPR
#define	AT91C_UDP_RX_DATA_BK0 (0x1u <<  1) // (UDP) Receive Data Bank 0
#define	AT91C_UDP_RXSETUP     (0x1u <<  2) // (UDP) Sends STALL to the Host (Control endpoints)
#define	AT91C_UDP_ISOERROR    (0x1u <<  3) // (UDP) Isochronous error (Isochronous endpoints)
#define	AT91C_UDP_TXPKTRDY    (0x1u <<  4) // (UDP) Transmit Packet Ready
#define	AT91C_UDP_FORCESTALL  (0x1u <<  5) // (UDP) Force Stall (used by Control, Bulk and Isochronous endpoints).
#define	AT91C_UDP_RX_DATA_BK1 (0x1u <<  6) // (UDP) Receive Data Bank 1 (only used by endpoints with ping-pong attributes).
#define	AT91C_UDP_DIR         (0x1u <<  7) // (UDP) Transfer Direction
#define	AT91C_UDP_EPTYPE      (0x7u <<  8) // (UDP) Endpoint type
#define		AT91C_UDP_EPTYPE_CTRL                 (0x0u <<  8) // (UDP) Control
#define		AT91C_UDP_EPTYPE_ISO_OUT              (0x1u <<  8) // (UDP) Isochronous OUT
#define		AT91C_UDP_EPTYPE_BULK_OUT             (0x2u <<  8) // (UDP) Bulk OUT
#define		AT91C_UDP_EPTYPE_INT_OUT              (0x3u <<  8) // (UDP) Interrupt OUT
#define		AT91C_UDP_EPTYPE_ISO_IN               (0x5u <<  8) // (UDP) Isochronous IN
#define		AT91C_UDP_EPTYPE_BULK_IN              (0x6u <<  8) // (UDP) Bulk IN
#define		AT91C_UDP_EPTYPE_INT_IN               (0x7u <<  8) // (UDP) Interrupt IN
#define	AT91C_UDP_DTGLE       (0x1u << 11) // (UDP) Data Toggle
#define	AT91C_UDP_EPEDS       (0x1u << 15) // (UDP) Endpoint Enable Disable
#define	AT91C_UDP_RXBYTECNT   (0x7FFu << 16) // (UDP) Number Of Bytes Available in the FIFO

// *****************************************************************************
//              SOFTWARE API DEFINITION  FOR Timer Counter Channel Interface
// *****************************************************************************
typedef struct _AT91S_TC {
	AT91_REG	 TC_CCR; 	// Channel Control Register
	AT91_REG	 TC_CMR; 	// Channel Mode Register
	AT91_REG	 Reserved0[2]; 	// 
	AT91_REG	 TC_CV; 	// Counter Value
	AT91_REG	 TC_RA; 	// Register A
	AT91_REG	 TC_RB; 	// Register B
	AT91_REG	 TC_RC; 	// Register C
	AT91_REG	 TC_SR; 	// Status Register
	AT91_REG	 TC_IER; 	// Interrupt Enable Register
	AT91_REG	 TC_IDR; 	// Interrupt Disable Register
	AT91_REG	 TC_IMR; 	// Interrupt Mask Register
} AT91S_TC, *AT91PS_TC;

// -------- TC_CCR : (TC Offset: 0x0) TC Channel Control Register -------- 
#define	AT91C_TC_CLKEN        (0x1u <<  0) // (TC) Counter Clock Enable Command
#define	AT91C_TC_CLKDIS       (0x1u <<  1) // (TC) Counter Clock Disable Command
#define	AT91C_TC_SWTRG        (0x1u <<  2) // (TC) Software Trigger Command
// -------- TC_CMR : (TC Offset: 0x4) TC Channel Mode Register: Capture Mode / Waveform Mode -------- 
#define	AT91C_TC_CPCSTOP      (0x1u <<  6) // (TC) Counter Clock Stopped with RC Compare
#define	AT91C_TC_CPCDIS       (0x1u <<  7) // (TC) Counter Clock Disable with RC Compare
#define	AT91C_TC_EEVTEDG      (0x3u <<  8) // (TC) External Event Edge Selection
#define		AT91C_TC_EEVTEDG_NONE                 (0x0u <<  8) // (TC) Edge: None
#define		AT91C_TC_EEVTEDG_RISING               (0x1u <<  8) // (TC) Edge: rising edge
#define		AT91C_TC_EEVTEDG_FALLING              (0x2u <<  8) // (TC) Edge: falling edge
#define		AT91C_TC_EEVTEDG_BOTH                 (0x3u <<  8) // (TC) Edge: each edge
#define	AT91C_TC_EEVT         (0x3u << 10) // (TC) External Event  Selection
#define		AT91C_TC_EEVT_NONE                 (0x0u << 10) // (TC) Signal selected as external event: TIOB TIOB direction: input
#define		AT91C_TC_EEVT_RISING               (0x1u << 10) // (TC) Signal selected as external event: XC0 TIOB direction: output
#define		AT91C_TC_EEVT_FALLING              (0x2u << 10) // (TC) Signal selected as external event: XC1 TIOB direction: output
#define		AT91C_TC_EEVT_BOTH                 (0x3u << 10) // (TC) Signal selected as external event: XC2 TIOB direction: output
#define	AT91C_TC_ENETRG       (0x1u << 12) // (TC) External Event Trigger enable
#define	AT91C_TC_WAVESEL      (0x3u << 13) // (TC) Waveform  Selection
#define		AT91C_TC_WAVESEL_UP                   (0x0u << 13) // (TC) UP mode without atomatic trigger on RC Compare
#define		AT91C_TC_WAVESEL_UP_AUTO              (0x1u << 13) // (TC) UP mode with automatic trigger on RC Compare
#define		AT91C_TC_WAVESEL_UPDOWN               (0x2u << 13) // (TC) UPDOWN mode without automatic trigger on RC Compare
#define		AT91C_TC_WAVESEL_UPDOWN_AUTO          (0x3u << 13) // (TC) UPDOWN mode with automatic trigger on RC Compare
#define	AT91C_TC_CPCTRG       (0x1u << 14) // (TC) RC Compare Trigger Enable
#define	AT91C_TC_WAVE         (0x1u << 15) // (TC) 
#define	AT91C_TC_ACPA         (0x3u << 16) // (TC) RA Compare Effect on TIOA
#define		AT91C_TC_ACPA_NONE                 (0x0u << 16) // (TC) Effect: none
#define		AT91C_TC_ACPA_SET                  (0x1u << 16) // (TC) Effect: set
#define		AT91C_TC_ACPA_CLEAR                (0x2u << 16) // (TC) Effect: clear
#define		AT91C_TC_ACPA_TOGGLE               (0x3u << 16) // (TC) Effect: toggle
#define	AT91C_TC_ACPC         (0x3u << 18) // (TC) RC Compare Effect on TIOA
#define		AT91C_TC_ACPC_NONE                 (0x0u << 18) // (TC) Effect: none
#define		AT91C_TC_ACPC_SET                  (0x1u << 18) // (TC) Effect: set
#define		AT91C_TC_ACPC_CLEAR                (0x2u << 18) // (TC) Effect: clear
#define		AT91C_TC_ACPC_TOGGLE               (0x3u << 18) // (TC) Effect: toggle
#define	AT91C_TC_AEEVT        (0x3u << 20) // (TC) External Event Effect on TIOA
#define		AT91C_TC_AEEVT_NONE                 (0x0u << 20) // (TC) Effect: none
#define		AT91C_TC_AEEVT_SET                  (0x1u << 20) // (TC) Effect: set
#define		AT91C_TC_AEEVT_CLEAR                (0x2u << 20) // (TC) Effect: clear
#define		AT91C_TC_AEEVT_TOGGLE               (0x3u << 20) // (TC) Effect: toggle
#define	AT91C_TC_ASWTRG       (0x3u << 22) // (TC) Software Trigger Effect on TIOA
#define		AT91C_TC_ASWTRG_NONE                 (0x0u << 22) // (TC) Effect: none
#define		AT91C_TC_ASWTRG_SET                  (0x1u << 22) // (TC) Effect: set
#define		AT91C_TC_ASWTRG_CLEAR                (0x2u << 22) // (TC) Effect: clear
#define		AT91C_TC_ASWTRG_TOGGLE               (0x3u << 22) // (TC) Effect: toggle
#define	AT91C_TC_BCPB         (0x3u << 24) // (TC) RB Compare Effect on TIOB
#define		AT91C_TC_BCPB_NONE                 (0x0u << 24) // (TC) Effect: none
#define		AT91C_TC_BCPB_SET                  (0x1u << 24) // (TC) Effect: set
#define		AT91C_TC_BCPB_CLEAR                (0x2u << 24) // (TC) Effect: clear
#define		AT91C_TC_BCPB_TOGGLE               (0x3u << 24) // (TC) Effect: toggle
#define	AT91C_TC_BCPC         (0x3u << 26) // (TC) RC Compare Effect on TIOB
#define		AT91C_TC_BCPC_NONE                 (0x0u << 26) // (TC) Effect: none
#define		AT91C_TC_BCPC_SET                  (0x1u << 26) // (TC) Effect: set
#define		AT91C_TC_BCPC_CLEAR                (0x2u << 26) // (TC) Effect: clear
#define		AT91C_TC_BCPC_TOGGLE               (0x3u << 26) // (TC) Effect: toggle
#define	AT91C_TC_BEEVT        (0x3u << 28) // (TC) External Event Effect on TIOB
#define		AT91C_TC_BEEVT_NONE                 (0x0u << 28) // (TC) Effect: none
#define		AT91C_TC_BEEVT_SET                  (0x1u << 28) // (TC) Effect: set
#define		AT91C_TC_BEEVT_CLEAR                (0x2u << 28) // (TC) Effect: clear
#define		AT91C_TC_BEEVT_TOGGLE               (0x3u << 28) // (TC) Effect: toggle
#define	AT91C_TC_BSWTRG       (0x3u << 30) // (TC) Software Trigger Effect on TIOB
#define		AT91C_TC_BSWTRG_NONE                 (0x0u << 30) // (TC) Effect: none
#define		AT91C_TC_BSWTRG_SET                  (0x1u << 30) // (TC) Effect: set
#define		AT91C_TC_BSWTRG_CLEAR                (0x2u << 30) // (TC) Effect: clear
#define		AT91C_TC_BSWTRG_TOGGLE               (0x3u << 30) // (TC) Effect: toggle
// -------- TC_SR : (TC Offset: 0x20) TC Channel Status Register -------- 
#define	AT91C_TC_COVFS        (0x1u <<  0) // (TC) Counter Overflow
#define	AT91C_TC_LOVRS        (0x1u <<  1) // (TC) Load Overrun
#define	AT91C_TC_CPAS         (0x1u <<  2) // (TC) RA Compare
#define	AT91C_TC_CPBS         (0x1u <<  3) // (TC) RB Compare
#define	AT91C_TC_CPCS         (0x1u <<  4) // (TC) RC Compare
#define	AT91C_TC_LDRAS        (0x1u <<  5) // (TC) RA Loading
#define	AT91C_TC_LDRBS        (0x1u <<  6) // (TC) RB Loading
#define	AT91C_TC_ETRCS        (0x1u <<  7) // (TC) External Trigger
#define	AT91C_TC_ETRGS        (0x1u << 16) // (TC) Clock Enabling
#define	AT91C_TC_MTIOA        (0x1u << 17) // (TC) TIOA Mirror
#define	AT91C_TC_MTIOB        (0x1u << 18) // (TC) TIOA Mirror
// -------- TC_IER : (TC Offset: 0x24) TC Channel Interrupt Enable Register -------- 
// -------- TC_IDR : (TC Offset: 0x28) TC Channel Interrupt Disable Register -------- 
// -------- TC_IMR : (TC Offset: 0x2c) TC Channel Interrupt Mask Register -------- 

// *****************************************************************************
//              SOFTWARE API DEFINITION  FOR Timer Counter Interface
// *****************************************************************************
typedef struct _AT91S_TCB {
	AT91S_TC	 TCB_TC0; 	// TC Channel 0
	AT91_REG	 Reserved0[4]; 	// 
	AT91S_TC	 TCB_TC1; 	// TC Channel 1
	AT91_REG	 Reserved1[4]; 	// 
	AT91S_TC	 TCB_TC2; 	// TC Channel 2
	AT91_REG	 Reserved2[4]; 	// 
	AT91_REG	 TCB_BCR; 	// TC Block Control Register
	AT91_REG	 TCB_BMR; 	// TC Block Mode Register
} AT91S_TCB, *AT91PS_TCB;

// -------- TCB_BCR : (TCB Offset: 0xc0) TC Block Control Register -------- 
#define	AT91C_TCB_SYNC        (0x1u <<  0) // (TCB) Synchro Command
// -------- TCB_BMR : (TCB Offset: 0xc4) TC Block Mode Register -------- 
#define	AT91C_TCB_TC0XC0S     (0x1u <<  0) // (TCB) External Clock Signal 0 Selection
#define		AT91C_TCB_TC0XC0S_TCLK0                0x0u // (TCB) TCLK0 connected to XC0
#define		AT91C_TCB_TC0XC0S_NONE                 0x1u // (TCB) None signal connected to XC0
#define		AT91C_TCB_TC0XC0S_TIOA1                0x2u // (TCB) TIOA1 connected to XC0
#define		AT91C_TCB_TC0XC0S_TIOA2                0x3u // (TCB) TIOA2 connected to XC0
#define	AT91C_TCB_TC1XC1S     (0x1u <<  2) // (TCB) External Clock Signal 1 Selection
#define		AT91C_TCB_TC1XC1S_TCLK1                (0x0u <<  2) // (TCB) TCLK1 connected to XC1
#define		AT91C_TCB_TC1XC1S_NONE                 (0x1u <<  2) // (TCB) None signal connected to XC1
#define		AT91C_TCB_TC1XC1S_TIOA0                (0x2u <<  2) // (TCB) TIOA0 connected to XC1
#define		AT91C_TCB_TC1XC1S_TIOA2                (0x3u <<  2) // (TCB) TIOA2 connected to XC1
#define	AT91C_TCB_TC2XC2S     (0x1u <<  4) // (TCB) External Clock Signal 2 Selection
#define		AT91C_TCB_TC2XC2S_TCLK2                (0x0u <<  4) // (TCB) TCLK2 connected to XC2
#define		AT91C_TCB_TC2XC2S_NONE                 (0x1u <<  4) // (TCB) None signal connected to XC2
#define		AT91C_TCB_TC2XC2S_TIOA0                (0x2u <<  4) // (TCB) TIOA0 connected to XC2
#define		AT91C_TCB_TC2XC2S_TIOA2                (0x3u <<  4) // (TCB) TIOA2 connected to XC2

// *****************************************************************************
//              SOFTWARE API DEFINITION  FOR USB Host Interface
// *****************************************************************************
typedef struct _AT91S_UHP {
	AT91_REG	 UHP_HcRevision; 	// Revision
	AT91_REG	 UHP_HcControl; 	// Operating modes for the Host Controller
	AT91_REG	 UHP_HcCommandStatus; 	// Command & status Register
	AT91_REG	 UHP_HcInterruptStatus; 	// Interrupt Status Register
	AT91_REG	 UHP_HcInterruptEnable; 	// Interrupt Enable Register
	AT91_REG	 UHP_HcInterruptDisable; 	// Interrupt Disable Register
	AT91_REG	 UHP_HcHCCA; 	// Pointer to the Host Controller Communication Area
	AT91_REG	 UHP_HcPeriodCurrentED; 	// Current Isochronous or Interrupt Endpoint Descriptor
	AT91_REG	 UHP_HcControlHeadED; 	// First Endpoint Descriptor of the Control list
	AT91_REG	 UHP_HcControlCurrentED; 	// Endpoint Control and Status Register
	AT91_REG	 UHP_HcBulkHeadED; 	// First endpoint register of the Bulk list
	AT91_REG	 UHP_HcBulkCurrentED; 	// Current endpoint of the Bulk list
	AT91_REG	 UHP_HcBulkDoneHead; 	// Last completed transfer descriptor
	AT91_REG	 UHP_HcFmInterval; 	// Bit time between 2 consecutive SOFs
	AT91_REG	 UHP_HcFmRemaining; 	// Bit time remaining in the current Frame
	AT91_REG	 UHP_HcFmNumber; 	// Frame number
	AT91_REG	 UHP_HcPeriodicStart; 	// Periodic Start
	AT91_REG	 UHP_HcLSThreshold; 	// LS Threshold
	AT91_REG	 UHP_HcRhDescriptorA; 	// Root Hub characteristics A
	AT91_REG	 UHP_HcRhDescriptorB; 	// Root Hub characteristics B
	AT91_REG	 UHP_HcRhStatus; 	// Root Hub Status register
	AT91_REG	 UHP_HcRhPortStatus[2]; 	// Root Hub Port Status Register
} AT91S_UHP, *AT91PS_UHP;


// *****************************************************************************
//              SOFTWARE API DEFINITION  FOR Ethernet MAC
// *****************************************************************************
typedef struct _AT91S_EMAC {
	AT91_REG	 EMAC_CTL; 	// Network Control Register
	AT91_REG	 EMAC_CFG; 	// Network Configuration Register
	AT91_REG	 EMAC_SR; 	// Network Status Register
	AT91_REG	 EMAC_TAR; 	// Transmit Address Register
	AT91_REG	 EMAC_TCR; 	// Transmit Control Register
	AT91_REG	 EMAC_TSR; 	// Transmit Status Register
	AT91_REG	 EMAC_RBQP; 	// Receive Buffer Queue Pointer
	AT91_REG	 Reserved0[1]; 	// 
	AT91_REG	 EMAC_RSR; 	// Receive Status Register
	AT91_REG	 EMAC_ISR; 	// Interrupt Status Register
	AT91_REG	 EMAC_IER; 	// Interrupt Enable Register
	AT91_REG	 EMAC_IDR; 	// Interrupt Disable Register
	AT91_REG	 EMAC_IMR; 	// Interrupt Mask Register
	AT91_REG	 EMAC_MAN; 	// PHY Maintenance Register
	AT91_REG	 Reserved1[2]; 	// 
	AT91_REG	 EMAC_FRA; 	// Frames Transmitted OK Register
	AT91_REG	 EMAC_SCOL; 	// Single Collision Frame Register
	AT91_REG	 EMAC_MCOL; 	// Multiple Collision Frame Register
	AT91_REG	 EMAC_OK; 	// Frames Received OK Register
	AT91_REG	 EMAC_SEQE; 	// Frame Check Sequence Error Register
	AT91_REG	 EMAC_ALE; 	// Alignment Error Register
	AT91_REG	 EMAC_DTE; 	// Deferred Transmission Frame Register
	AT91_REG	 EMAC_LCOL; 	// Late Collision Register
	AT91_REG	 EMAC_ECOL; 	// Excessive Collision Register
	AT91_REG	 EMAC_CSE; 	// Carrier Sense Error Register
	AT91_REG	 EMAC_TUE; 	// Transmit Underrun Error Register
	AT91_REG	 EMAC_CDE; 	// Code Error Register
	AT91_REG	 EMAC_ELR; 	// Excessive Length Error Register
	AT91_REG	 EMAC_RJB; 	// Receive Jabber Register
	AT91_REG	 EMAC_USF; 	// Undersize Frame Register
	AT91_REG	 EMAC_SQEE; 	// SQE Test Error Register
	AT91_REG	 EMAC_DRFC; 	// Discarded RX Frame Register
	AT91_REG	 Reserved2[3]; 	// 
	AT91_REG	 EMAC_HSH; 	// Hash Address High[63:32]
	AT91_REG	 EMAC_HSL; 	// Hash Address Low[31:0]
	AT91_REG	 EMAC_SA1L; 	// Specific Address 1 Low, First 4 bytes
	AT91_REG	 EMAC_SA1H; 	// Specific Address 1 High, Last 2 bytes
	AT91_REG	 EMAC_SA2L; 	// Specific Address 2 Low, First 4 bytes
	AT91_REG	 EMAC_SA2H; 	// Specific Address 2 High, Last 2 bytes
	AT91_REG	 EMAC_SA3L; 	// Specific Address 3 Low, First 4 bytes
	AT91_REG	 EMAC_SA3H; 	// Specific Address 3 High, Last 2 bytes
	AT91_REG	 EMAC_SA4L; 	// Specific Address 4 Low, First 4 bytes
	AT91_REG	 EMAC_SA4H; 	// Specific Address 4 High, Last 2 bytesr
} AT91S_EMAC, *AT91PS_EMAC;

// -------- EMAC_CTL : (EMAC Offset: 0x0)  -------- 
#define	AT91C_EMAC_LB         (0x1u <<  0) // (EMAC) Loopback. Optional. When set, loopback signal is at high level.
#define	AT91C_EMAC_LBL        (0x1u <<  1) // (EMAC) Loopback local. 
#define	AT91C_EMAC_RE         (0x1u <<  2) // (EMAC) Receive enable. 
#define	AT91C_EMAC_TE         (0x1u <<  3) // (EMAC) Transmit enable. 
#define	AT91C_EMAC_MPE        (0x1u <<  4) // (EMAC) Management port enable. 
#define	AT91C_EMAC_CSR        (0x1u <<  5) // (EMAC) Clear statistics registers. 
#define	AT91C_EMAC_ISR        (0x1u <<  6) // (EMAC) Increment statistics registers. 
#define	AT91C_EMAC_WES        (0x1u <<  7) // (EMAC) Write enable for statistics registers. 
#define	AT91C_EMAC_BP         (0x1u <<  8) // (EMAC) Back pressure. 
// -------- EMAC_CFG : (EMAC Offset: 0x4) Network Configuration Register -------- 
#define	AT91C_EMAC_SPD        (0x1u <<  0) // (EMAC) Speed. 
#define	AT91C_EMAC_FD         (0x1u <<  1) // (EMAC) Full duplex. 
#define	AT91C_EMAC_BR         (0x1u <<  2) // (EMAC) Bit rate. 
#define	AT91C_EMAC_CAF        (0x1u <<  4) // (EMAC) Copy all frames. 
#define	AT91C_EMAC_NBC        (0x1u <<  5) // (EMAC) No broadcast. 
#define	AT91C_EMAC_MTI        (0x1u <<  6) // (EMAC) Multicast hash enable
#define	AT91C_EMAC_UNI        (0x1u <<  7) // (EMAC) Unicast hash enable. 
#define	AT91C_EMAC_BIG        (0x1u <<  8) // (EMAC) Receive 1522 bytes. 
#define	AT91C_EMAC_EAE        (0x1u <<  9) // (EMAC) External address match enable. 
#define	AT91C_EMAC_CLK        (0x3u << 10) // (EMAC) 
#define		AT91C_EMAC_CLK_HCLK_8               (0x0u << 10) // (EMAC) HCLK divided by 8
#define		AT91C_EMAC_CLK_HCLK_16              (0x1u << 10) // (EMAC) HCLK divided by 16
#define		AT91C_EMAC_CLK_HCLK_32              (0x2u << 10) // (EMAC) HCLK divided by 32
#define		AT91C_EMAC_CLK_HCLK_64              (0x3u << 10) // (EMAC) HCLK divided by 64
#define	AT91C_EMAC_RTY        (0x1u << 12) // (EMAC) 
#define	AT91C_EMAC_RMII       (0x1u << 13) // (EMAC) 
// -------- EMAC_SR : (EMAC Offset: 0x8) Network Status Register -------- 
#define	AT91C_EMAC_MDIO       (0x1u <<  1) // (EMAC) 
#define	AT91C_EMAC_IDLE       (0x1u <<  2) // (EMAC) 
// -------- EMAC_TCR : (EMAC Offset: 0x10) Transmit Control Register -------- 
#define	AT91C_EMAC_LEN        (0x7FFu <<  0) // (EMAC) 
#define	AT91C_EMAC_NCRC       (0x1u << 15) // (EMAC) 
// -------- EMAC_TSR : (EMAC Offset: 0x14) Transmit Control Register -------- 
#define	AT91C_EMAC_OVR        (0x1u <<  0) // (EMAC) 
#define	AT91C_EMAC_COL        (0x1u <<  1) // (EMAC) 
#define	AT91C_EMAC_RLE        (0x1u <<  2) // (EMAC) 
#define	AT91C_EMAC_TXIDLE     (0x1u <<  3) // (EMAC) 
#define	AT91C_EMAC_BNQ        (0x1u <<  4) // (EMAC) 
#define	AT91C_EMAC_COMP       (0x1u <<  5) // (EMAC) 
#define	AT91C_EMAC_UND        (0x1u <<  6) // (EMAC) 
// -------- EMAC_RSR : (EMAC Offset: 0x20) Receive Status Register -------- 
#define	AT91C_EMAC_BNA        (0x1u <<  0) // (EMAC) 
#define	AT91C_EMAC_REC        (0x1u <<  1) // (EMAC) 
// -------- EMAC_ISR : (EMAC Offset: 0x24) Interrupt Status Register -------- 
#define	AT91C_EMAC_DONE       (0x1u <<  0) // (EMAC) 
#define	AT91C_EMAC_RCOM       (0x1u <<  1) // (EMAC) 
#define	AT91C_EMAC_RBNA       (0x1u <<  2) // (EMAC) 
#define	AT91C_EMAC_TOVR       (0x1u <<  3) // (EMAC) 
#define	AT91C_EMAC_TUND       (0x1u <<  4) // (EMAC) 
#define	AT91C_EMAC_RTRY       (0x1u <<  5) // (EMAC) 
#define	AT91C_EMAC_TBRE       (0x1u <<  6) // (EMAC) 
#define	AT91C_EMAC_TCOM       (0x1u <<  7) // (EMAC) 
#define	AT91C_EMAC_TIDLE      (0x1u <<  8) // (EMAC) 
#define	AT91C_EMAC_LINK       (0x1u <<  9) // (EMAC) 
#define	AT91C_EMAC_ROVR       (0x1u << 10) // (EMAC) 
#define	AT91C_EMAC_HRESP      (0x1u << 11) // (EMAC) 
// -------- EMAC_IER : (EMAC Offset: 0x28) Interrupt Enable Register -------- 
// -------- EMAC_IDR : (EMAC Offset: 0x2c) Interrupt Disable Register -------- 
// -------- EMAC_IMR : (EMAC Offset: 0x30) Interrupt Mask Register -------- 
// -------- EMAC_MAN : (EMAC Offset: 0x34) PHY Maintenance Register -------- 
#define	AT91C_EMAC_DATA       (0xFFFFu <<  0) // (EMAC) 
#define	AT91C_EMAC_CODE       (0x3u << 16) // (EMAC) 
#define	AT91C_EMAC_REGA       (0x1Fu << 18) // (EMAC) 
#define	AT91C_EMAC_PHYA       (0x1Fu << 23) // (EMAC) 
#define	AT91C_EMAC_RW         (0x3u << 28) // (EMAC) 
#define	AT91C_EMAC_HIGH       (0x1u << 30) // (EMAC) 
#define	AT91C_EMAC_LOW        (0x1u << 31) // (EMAC) 

// *****************************************************************************
//              SOFTWARE API DEFINITION  FOR External Bus Interface
// *****************************************************************************
typedef struct _AT91S_EBI {
	AT91_REG	 EBI_CSA; 	// Chip Select Assignment Register
	AT91_REG	 EBI_CFGR; 	// Configuration Register
} AT91S_EBI, *AT91PS_EBI;

// -------- EBI_CSA : (EBI Offset: 0x0) Chip Select Assignment Register -------- 
#define	AT91C_EBI_CS0A        (0x1u <<  0) // (EBI) Chip Select 0 Assignment
#define		AT91C_EBI_CS0A_SMC                  0x0u // (EBI) Chip Select 0 is assigned to the Static Memory Controller.
#define		AT91C_EBI_CS0A_BFC                  0x1u // (EBI) Chip Select 0 is assigned to the Burst Flash Controller.
#define	AT91C_EBI_CS1A        (0x1u <<  1) // (EBI) Chip Select 1 Assignment
#define		AT91C_EBI_CS1A_SMC                  (0x0u <<  1) // (EBI) Chip Select 1 is assigned to the Static Memory Controller.
#define		AT91C_EBI_CS1A_SDRAMC               (0x1u <<  1) // (EBI) Chip Select 1 is assigned to the SDRAM Controller.
#define	AT91C_EBI_CS3A        (0x1u <<  3) // (EBI) Chip Select 3 Assignment
#define		AT91C_EBI_CS3A_SMC                  (0x0u <<  3) // (EBI) Chip Select 3 is only assigned to the Static Memory Controller and NCS3 behaves as defined by the SMC2.
#define		AT91C_EBI_CS3A_SMC_SmartMedia       (0x1u <<  3) // (EBI) Chip Select 3 is assigned to the Static Memory Controller and the SmartMedia Logic is activated.
#define	AT91C_EBI_CS4A        (0x1u <<  4) // (EBI) Chip Select 4 Assignment
#define		AT91C_EBI_CS4A_SMC                  (0x0u <<  4) // (EBI) Chip Select 4 is assigned to the Static Memory Controller and NCS4,NCS5 and NCS6 behave as defined by the SMC2.
#define		AT91C_EBI_CS4A_SMC_CompactFlash     (0x1u <<  4) // (EBI) Chip Select 4 is assigned to the Static Memory Controller and the CompactFlash Logic is activated.
// -------- EBI_CFGR : (EBI Offset: 0x4) Configuration Register -------- 
#define	AT91C_EBI_DBPUC       (0x1u <<  0) // (EBI) Data Bus Pull-Up Configuration
#define	AT91C_EBI_EBSEN       (0x1u <<  1) // (EBI) Bus Sharing Enable

// *****************************************************************************
//              SOFTWARE API DEFINITION  FOR Static Memory Controller 2 Interface
// *****************************************************************************
typedef struct _AT91S_SMC2 {
	AT91_REG	 SMC2_CSR[8]; 	// SMC2 Chip Select Register
} AT91S_SMC2, *AT91PS_SMC2;

// -------- SMC2_CSR : (SMC2 Offset: 0x0) SMC2 Chip Select Register -------- 
#define	AT91C_SMC2_NWS        (0x7Fu <<  0) // (SMC2) Number of Wait States
#define	AT91C_SMC2_WSEN       (0x1u <<  7) // (SMC2) Wait State Enable
#define	AT91C_SMC2_TDF        (0xFu <<  8) // (SMC2) Data Float Time
#define	AT91C_SMC2_BAT        (0x1u << 12) // (SMC2) Byte Access Type
#define	AT91C_SMC2_DBW        (0x1u << 13) // (SMC2) Data Bus Width
#define		AT91C_SMC2_DBW_16                   (0x1u << 13) // (SMC2) 16-bit.
#define		AT91C_SMC2_DBW_8                    (0x2u << 13) // (SMC2) 8-bit.
#define	AT91C_SMC2_DRP        (0x1u << 15) // (SMC2) Data Read Protocol
#define	AT91C_SMC2_ACSS       (0x3u << 16) // (SMC2) Address to Chip Select Setup
#define		AT91C_SMC2_ACSS_STANDARD             (0x0u << 16) // (SMC2) Standard, asserted at the beginning of the access and deasserted at the end.
#define		AT91C_SMC2_ACSS_1_CYCLE              (0x1u << 16) // (SMC2) One cycle less at the beginning and the end of the access.
#define		AT91C_SMC2_ACSS_2_CYCLES             (0x2u << 16) // (SMC2) Two cycles less at the beginning and the end of the access.
#define		AT91C_SMC2_ACSS_3_CYCLES             (0x3u << 16) // (SMC2) Three cycles less at the beginning and the end of the access.
#define	AT91C_SMC2_RWSETUP    (0x7u << 24) // (SMC2) Read and Write Signal Setup Time
#define	AT91C_SMC2_RWHOLD     (0x7u << 29) // (SMC2) Read and Write Signal Hold Time

// *****************************************************************************
//              SOFTWARE API DEFINITION  FOR SDRAM Controller Interface
// *****************************************************************************
typedef struct _AT91S_SDRC {
	AT91_REG	 SDRC_MR; 	// SDRAM Controller Mode Register
	AT91_REG	 SDRC_TR; 	// SDRAM Controller Refresh Timer Register
	AT91_REG	 SDRC_CR; 	// SDRAM Controller Configuration Register
	AT91_REG	 SDRC_SRR; 	// SDRAM Controller Self Refresh Register
	AT91_REG	 SDRC_LPR; 	// SDRAM Controller Low Power Register
	AT91_REG	 SDRC_IER; 	// SDRAM Controller Interrupt Enable Register
	AT91_REG	 SDRC_IDR; 	// SDRAM Controller Interrupt Disable Register
	AT91_REG	 SDRC_IMR; 	// SDRAM Controller Interrupt Mask Register
	AT91_REG	 SDRC_ISR; 	// SDRAM Controller Interrupt Mask Register
} AT91S_SDRC, *AT91PS_SDRC;

// -------- SDRC_MR : (SDRC Offset: 0x0) SDRAM Controller Mode Register -------- 
#define	AT91C_SDRC_MODE       (0xFu <<  0) // (SDRC) Mode
#define		AT91C_SDRC_MODE_NORMAL_CMD           0x0u // (SDRC) Normal Mode
#define		AT91C_SDRC_MODE_NOP_CMD              0x1u // (SDRC) NOP Command
#define		AT91C_SDRC_MODE_PRCGALL_CMD          0x2u // (SDRC) All Banks Precharge Command
#define		AT91C_SDRC_MODE_LMR_CMD              0x3u // (SDRC) Load Mode Register Command
#define		AT91C_SDRC_MODE_RFSH_CMD             0x4u // (SDRC) Refresh Command
#define	AT91C_SDRC_DBW        (0x1u <<  4) // (SDRC) Data Bus Width
#define		AT91C_SDRC_DBW_32_BITS              (0x0u <<  4) // (SDRC) 32 Bits datas bus
#define		AT91C_SDRC_DBW_16_BITS              (0x1u <<  4) // (SDRC) 16 Bits datas bus
// -------- SDRC_TR : (SDRC Offset: 0x4) SDRC Refresh Timer Register -------- 
#define	AT91C_SDRC_COUNT      (0xFFFu <<  0) // (SDRC) Refresh Counter
// -------- SDRC_CR : (SDRC Offset: 0x8) SDRAM Configuration Register -------- 
#define	AT91C_SDRC_NC         (0x3u <<  0) // (SDRC) Number of Column Bits
#define		AT91C_SDRC_NC_8                    0x0u // (SDRC) 8 Bits
#define		AT91C_SDRC_NC_9                    0x1u // (SDRC) 9 Bits
#define		AT91C_SDRC_NC_10                   0x2u // (SDRC) 10 Bits
#define		AT91C_SDRC_NC_11                   0x3u // (SDRC) 11 Bits
#define	AT91C_SDRC_NR         (0x3u <<  2) // (SDRC) Number of Row Bits
#define		AT91C_SDRC_NR_11                   (0x0u <<  2) // (SDRC) 11 Bits
#define		AT91C_SDRC_NR_12                   (0x1u <<  2) // (SDRC) 12 Bits
#define		AT91C_SDRC_NR_13                   (0x2u <<  2) // (SDRC) 13 Bits
#define	AT91C_SDRC_NB         (0x1u <<  4) // (SDRC) Number of Banks
#define		AT91C_SDRC_NB_2_BANKS              (0x0u <<  4) // (SDRC) 2 banks
#define		AT91C_SDRC_NB_4_BANKS              (0x1u <<  4) // (SDRC) 4 banks
#define	AT91C_SDRC_CAS        (0x3u <<  5) // (SDRC) CAS Latency
#define		AT91C_SDRC_CAS_2                    (0x2u <<  5) // (SDRC) 2 cycles
#define	AT91C_SDRC_TWR        (0xFu <<  7) // (SDRC) Number of Write Recovery Time Cycles
#define	AT91C_SDRC_TRC        (0xFu << 11) // (SDRC) Number of RAS Cycle Time Cycles
#define	AT91C_SDRC_TRP        (0xFu << 15) // (SDRC) Number of RAS Precharge Time Cycles
#define	AT91C_SDRC_TRCD       (0xFu << 19) // (SDRC) Number of RAS to CAS Delay Cycles
#define	AT91C_SDRC_TRAS       (0xFu << 23) // (SDRC) Number of RAS Active Time Cycles
#define	AT91C_SDRC_TXSR       (0xFu << 27) // (SDRC) Number of Command Recovery Time Cycles
// -------- SDRC_SRR : (SDRC Offset: 0xc) SDRAM Controller Self-refresh Register -------- 
#define	AT91C_SDRC_SRCB       (0x1u <<  0) // (SDRC) Self-refresh Command Bit
// -------- SDRC_LPR : (SDRC Offset: 0x10) SDRAM Controller Low-power Register -------- 
#define	AT91C_SDRC_LPCB       (0x1u <<  0) // (SDRC) Low-power Command Bit
// -------- SDRC_IER : (SDRC Offset: 0x14) SDRAM Controller Interrupt Enable Register -------- 
#define	AT91C_SDRC_RES        (0x1u <<  0) // (SDRC) Refresh Error Status
// -------- SDRC_IDR : (SDRC Offset: 0x18) SDRAM Controller Interrupt Disable Register -------- 
// -------- SDRC_IMR : (SDRC Offset: 0x1c) SDRAM Controller Interrupt Mask Register -------- 
// -------- SDRC_ISR : (SDRC Offset: 0x20) SDRAM Controller Interrupt Status Register -------- 

// *****************************************************************************
//              SOFTWARE API DEFINITION  FOR Burst Flash Controller Interface
// *****************************************************************************
typedef struct _AT91S_BFC {
	AT91_REG	 BFC_MR; 	// BFC Mode Register
} AT91S_BFC, *AT91PS_BFC;

// -------- BFC_MR : (BFC Offset: 0x0) BFC Mode Register -------- 
#define	AT91C_BFC_BFCOM       (0x3u <<  0) // (BFC) Burst Flash Controller Operating Mode
#define		AT91C_BFC_BFCOM_DISABLED             0x0u // (BFC) NPCS0 is driven by the SMC or remains high.
#define		AT91C_BFC_BFCOM_ASYNC                0x1u // (BFC) Asynchronous
#define		AT91C_BFC_BFCOM_BURST_READ           0x2u // (BFC) Burst Read
#define	AT91C_BFC_BFCC        (0x3u <<  2) // (BFC) Burst Flash Controller Operating Mode
#define		AT91C_BFC_BFCC_MCK                  (0x1u <<  2) // (BFC) Master Clock.
#define		AT91C_BFC_BFCC_MCK_DIV_2            (0x2u <<  2) // (BFC) Master Clock divided by 2.
#define		AT91C_BFC_BFCC_MCK_DIV_4            (0x3u <<  2) // (BFC) Master Clock divided by 4.
#define	AT91C_BFC_AVL         (0xFu <<  4) // (BFC) Address Valid Latency
#define	AT91C_BFC_PAGES       (0x7u <<  8) // (BFC) Page Size
#define		AT91C_BFC_PAGES_NO_PAGE              (0x0u <<  8) // (BFC) No page handling.
#define		AT91C_BFC_PAGES_16                   (0x1u <<  8) // (BFC) 16 bytes page size.
#define		AT91C_BFC_PAGES_32                   (0x2u <<  8) // (BFC) 32 bytes page size.
#define		AT91C_BFC_PAGES_64                   (0x3u <<  8) // (BFC) 64 bytes page size.
#define		AT91C_BFC_PAGES_128                  (0x4u <<  8) // (BFC) 128 bytes page size.
#define		AT91C_BFC_PAGES_256                  (0x5u <<  8) // (BFC) 256 bytes page size.
#define		AT91C_BFC_PAGES_512                  (0x6u <<  8) // (BFC) 512 bytes page size.
#define		AT91C_BFC_PAGES_1024                 (0x7u <<  8) // (BFC) 1024 bytes page size.
#define	AT91C_BFC_OEL         (0x3u << 12) // (BFC) Output Enable Latency
#define	AT91C_BFC_BAAEN       (0x1u << 16) // (BFC) Burst Address Advance Enable
#define	AT91C_BFC_BFOEH       (0x1u << 17) // (BFC) Burst Flash Output Enable Handling
#define	AT91C_BFC_MUXEN       (0x1u << 18) // (BFC) Multiplexed Bus Enable
#define	AT91C_BFC_RDYEN       (0x1u << 19) // (BFC) Ready Enable Mode

// *****************************************************************************
//               REGISTER ADDRESS DEFINITION FOR AT91RM9200
// *****************************************************************************
// ========== Register definition for SYS peripheral ========== 
// ========== Register definition for MC peripheral ========== 
#define	AT91C_MC_PUER   ((AT91_REG *) 	0xFFFFFF54) // (MC) MC Protection Unit Enable Register
#define	AT91C_MC_ASR    ((AT91_REG *) 	0xFFFFFF04) // (MC) MC Abort Status Register
#define	AT91C_MC_PUP    ((AT91_REG *) 	0xFFFFFF50) // (MC) MC Protection Unit Peripherals
#define	AT91C_MC_PUIA   ((AT91_REG *) 	0xFFFFFF10) // (MC) MC Protection Unit Area
#define	AT91C_MC_AASR   ((AT91_REG *) 	0xFFFFFF08) // (MC) MC Abort Address Status Register
#define	AT91C_MC_RCR    ((AT91_REG *) 	0xFFFFFF00) // (MC) MC Remap Control Register
// ========== Register definition for RTC peripheral ========== 
#define	AT91C_RTC_IMR   ((AT91_REG *) 	0xFFFFFE28) // (RTC) Interrupt Mask Register
#define	AT91C_RTC_IER   ((AT91_REG *) 	0xFFFFFE20) // (RTC) Interrupt Enable Register
#define	AT91C_RTC_SR    ((AT91_REG *) 	0xFFFFFE18) // (RTC) Status Register
#define	AT91C_RTC_TIMALR ((AT91_REG *) 	0xFFFFFE10) // (RTC) Time Alarm Register
#define	AT91C_RTC_TIMR  ((AT91_REG *) 	0xFFFFFE08) // (RTC) Time Register
#define	AT91C_RTC_CR    ((AT91_REG *) 	0xFFFFFE00) // (RTC) Control Register
#define	AT91C_RTC_VER   ((AT91_REG *) 	0xFFFFFE2C) // (RTC) Valid Entry Register
#define	AT91C_RTC_IDR   ((AT91_REG *) 	0xFFFFFE24) // (RTC) Interrupt Disable Register
#define	AT91C_RTC_SCCR  ((AT91_REG *) 	0xFFFFFE1C) // (RTC) Status Clear Command Register
#define	AT91C_RTC_CALALR ((AT91_REG *) 	0xFFFFFE14) // (RTC) Calendar Alarm Register
#define	AT91C_RTC_CALR  ((AT91_REG *) 	0xFFFFFE0C) // (RTC) Calendar Register
#define	AT91C_RTC_MR    ((AT91_REG *) 	0xFFFFFE04) // (RTC) Mode Register
// ========== Register definition for ST peripheral ========== 
#define	AT91C_ST_CRTR   ((AT91_REG *) 	0xFFFFFD24) // (ST) Current Real-time Register
#define	AT91C_ST_IMR    ((AT91_REG *) 	0xFFFFFD1C) // (ST) Interrupt Mask Register
#define	AT91C_ST_IER    ((AT91_REG *) 	0xFFFFFD14) // (ST) Interrupt Enable Register
#define	AT91C_ST_RTMR   ((AT91_REG *) 	0xFFFFFD0C) // (ST) Real-time Mode Register
#define	AT91C_ST_PIMR   ((AT91_REG *) 	0xFFFFFD04) // (ST) Period Interval Mode Register
#define	AT91C_ST_RTAR   ((AT91_REG *) 	0xFFFFFD20) // (ST) Real-time Alarm Register
#define	AT91C_ST_IDR    ((AT91_REG *) 	0xFFFFFD18) // (ST) Interrupt Disable Register
#define	AT91C_ST_SR     ((AT91_REG *) 	0xFFFFFD10) // (ST) Status Register
#define	AT91C_ST_WDMR   ((AT91_REG *) 	0xFFFFFD08) // (ST) Watchdog Mode Register
#define	AT91C_ST_CR     ((AT91_REG *) 	0xFFFFFD00) // (ST) Control Register
// ========== Register definition for PMC peripheral ========== 
#define	AT91C_PMC_SCSR  ((AT91_REG *) 	0xFFFFFC08) // (PMC) System Clock Status Register
#define	AT91C_PMC_SCER  ((AT91_REG *) 	0xFFFFFC00) // (PMC) System Clock Enable Register
#define	AT91C_PMC_IMR   ((AT91_REG *) 	0xFFFFFC6C) // (PMC) Interrupt Mask Register
#define	AT91C_PMC_IDR   ((AT91_REG *) 	0xFFFFFC64) // (PMC) Interrupt Disable Register
#define	AT91C_PMC_PCDR  ((AT91_REG *) 	0xFFFFFC14) // (PMC) Peripheral Clock Disable Register
#define	AT91C_PMC_SCDR  ((AT91_REG *) 	0xFFFFFC04) // (PMC) System Clock Disable Register
#define	AT91C_PMC_SR    ((AT91_REG *) 	0xFFFFFC68) // (PMC) Status Register
#define	AT91C_PMC_IER   ((AT91_REG *) 	0xFFFFFC60) // (PMC) Interrupt Enable Register
#define	AT91C_PMC_MCKR  ((AT91_REG *) 	0xFFFFFC30) // (PMC) Master Clock Register
#define	AT91C_PMC_PCER  ((AT91_REG *) 	0xFFFFFC10) // (PMC) Peripheral Clock Enable Register
#define	AT91C_PMC_PCSR  ((AT91_REG *) 	0xFFFFFC18) // (PMC) Peripheral Clock Status Register
#define	AT91C_PMC_PCKR  ((AT91_REG *) 	0xFFFFFC40) // (PMC) Programmable Clock Register
// ========== Register definition for CKGR peripheral ========== 
#define	AT91C_CKGR_PLLBR ((AT91_REG *) 	0xFFFFFC2C) // (CKGR) PLL B Register
#define	AT91C_CKGR_MCFR ((AT91_REG *) 	0xFFFFFC24) // (CKGR) Main Clock  Frequency Register
#define	AT91C_CKGR_PLLAR ((AT91_REG *) 	0xFFFFFC28) // (CKGR) PLL A Register
#define	AT91C_CKGR_MOR  ((AT91_REG *) 	0xFFFFFC20) // (CKGR) Main Oscillator Register
// ========== Register definition for PIOD peripheral ========== 
#define	AT91C_PIOD_PDSR ((AT91_REG *) 	0xFFFFFA3C) // (PIOD) Pin Data Status Register
#define	AT91C_PIOD_CODR ((AT91_REG *) 	0xFFFFFA34) // (PIOD) Clear Output Data Register
#define	AT91C_PIOD_OWER ((AT91_REG *) 	0xFFFFFAA0) // (PIOD) Output Write Enable Register
#define	AT91C_PIOD_MDER ((AT91_REG *) 	0xFFFFFA50) // (PIOD) Multi-driver Enable Register
#define	AT91C_PIOD_IMR  ((AT91_REG *) 	0xFFFFFA48) // (PIOD) Interrupt Mask Register
#define	AT91C_PIOD_IER  ((AT91_REG *) 	0xFFFFFA40) // (PIOD) Interrupt Enable Register
#define	AT91C_PIOD_ODSR ((AT91_REG *) 	0xFFFFFA38) // (PIOD) Output Data Status Register
#define	AT91C_PIOD_SODR ((AT91_REG *) 	0xFFFFFA30) // (PIOD) Set Output Data Register
#define	AT91C_PIOD_PER  ((AT91_REG *) 	0xFFFFFA00) // (PIOD) PIO Enable Register
#define	AT91C_PIOD_OWDR ((AT91_REG *) 	0xFFFFFAA4) // (PIOD) Output Write Disable Register
#define	AT91C_PIOD_PPUER ((AT91_REG *) 	0xFFFFFA64) // (PIOD) Pull-up Enable Register
#define	AT91C_PIOD_MDDR ((AT91_REG *) 	0xFFFFFA54) // (PIOD) Multi-driver Disable Register
#define	AT91C_PIOD_ISR  ((AT91_REG *) 	0xFFFFFA4C) // (PIOD) Interrupt Status Register
#define	AT91C_PIOD_IDR  ((AT91_REG *) 	0xFFFFFA44) // (PIOD) Interrupt Disable Register
#define	AT91C_PIOD_PDR  ((AT91_REG *) 	0xFFFFFA04) // (PIOD) PIO Disable Register
#define	AT91C_PIOD_ODR  ((AT91_REG *) 	0xFFFFFA14) // (PIOD) Output Disable Registerr
#define	AT91C_PIOD_OWSR ((AT91_REG *) 	0xFFFFFAA8) // (PIOD) Output Write Status Register
#define	AT91C_PIOD_ABSR ((AT91_REG *) 	0xFFFFFA78) // (PIOD) AB Select Status Register
#define	AT91C_PIOD_ASR  ((AT91_REG *) 	0xFFFFFA70) // (PIOD) Select A Register
#define	AT91C_PIOD_PPUSR ((AT91_REG *) 	0xFFFFFA68) // (PIOD) Pad Pull-up Status Register
#define	AT91C_PIOD_PPUDR ((AT91_REG *) 	0xFFFFFA60) // (PIOD) Pull-up Disable Register
#define	AT91C_PIOD_MDSR ((AT91_REG *) 	0xFFFFFA58) // (PIOD) Multi-driver Status Register
#define	AT91C_PIOD_PSR  ((AT91_REG *) 	0xFFFFFA08) // (PIOD) PIO Status Register
#define	AT91C_PIOD_OER  ((AT91_REG *) 	0xFFFFFA10) // (PIOD) Output Enable Register
#define	AT91C_PIOD_OSR  ((AT91_REG *) 	0xFFFFFA18) // (PIOD) Output Status Register
#define	AT91C_PIOD_IFER ((AT91_REG *) 	0xFFFFFA20) // (PIOD) Input Filter Enable Register
#define	AT91C_PIOD_BSR  ((AT91_REG *) 	0xFFFFFA74) // (PIOD) Select B Register
#define	AT91C_PIOD_IFDR ((AT91_REG *) 	0xFFFFFA24) // (PIOD) Input Filter Disable Register
#define	AT91C_PIOD_IFSR ((AT91_REG *) 	0xFFFFFA28) // (PIOD) Input Filter Status Register
// ========== Register definition for PIOC peripheral ========== 
#define	AT91C_PIOC_IFDR ((AT91_REG *) 	0xFFFFF824) // (PIOC) Input Filter Disable Register
#define	AT91C_PIOC_ODR  ((AT91_REG *) 	0xFFFFF814) // (PIOC) Output Disable Registerr
#define	AT91C_PIOC_ABSR ((AT91_REG *) 	0xFFFFF878) // (PIOC) AB Select Status Register
#define	AT91C_PIOC_SODR ((AT91_REG *) 	0xFFFFF830) // (PIOC) Set Output Data Register
#define	AT91C_PIOC_IFSR ((AT91_REG *) 	0xFFFFF828) // (PIOC) Input Filter Status Register
#define	AT91C_PIOC_CODR ((AT91_REG *) 	0xFFFFF834) // (PIOC) Clear Output Data Register
#define	AT91C_PIOC_ODSR ((AT91_REG *) 	0xFFFFF838) // (PIOC) Output Data Status Register
#define	AT91C_PIOC_IER  ((AT91_REG *) 	0xFFFFF840) // (PIOC) Interrupt Enable Register
#define	AT91C_PIOC_IMR  ((AT91_REG *) 	0xFFFFF848) // (PIOC) Interrupt Mask Register
#define	AT91C_PIOC_OWDR ((AT91_REG *) 	0xFFFFF8A4) // (PIOC) Output Write Disable Register
#define	AT91C_PIOC_MDDR ((AT91_REG *) 	0xFFFFF854) // (PIOC) Multi-driver Disable Register
#define	AT91C_PIOC_PDSR ((AT91_REG *) 	0xFFFFF83C) // (PIOC) Pin Data Status Register
#define	AT91C_PIOC_IDR  ((AT91_REG *) 	0xFFFFF844) // (PIOC) Interrupt Disable Register
#define	AT91C_PIOC_ISR  ((AT91_REG *) 	0xFFFFF84C) // (PIOC) Interrupt Status Register
#define	AT91C_PIOC_PDR  ((AT91_REG *) 	0xFFFFF804) // (PIOC) PIO Disable Register
#define	AT91C_PIOC_OWSR ((AT91_REG *) 	0xFFFFF8A8) // (PIOC) Output Write Status Register
#define	AT91C_PIOC_OWER ((AT91_REG *) 	0xFFFFF8A0) // (PIOC) Output Write Enable Register
#define	AT91C_PIOC_ASR  ((AT91_REG *) 	0xFFFFF870) // (PIOC) Select A Register
#define	AT91C_PIOC_PPUSR ((AT91_REG *) 	0xFFFFF868) // (PIOC) Pad Pull-up Status Register
#define	AT91C_PIOC_PPUDR ((AT91_REG *) 	0xFFFFF860) // (PIOC) Pull-up Disable Register
#define	AT91C_PIOC_MDSR ((AT91_REG *) 	0xFFFFF858) // (PIOC) Multi-driver Status Register
#define	AT91C_PIOC_MDER ((AT91_REG *) 	0xFFFFF850) // (PIOC) Multi-driver Enable Register
#define	AT91C_PIOC_IFER ((AT91_REG *) 	0xFFFFF820) // (PIOC) Input Filter Enable Register
#define	AT91C_PIOC_OSR  ((AT91_REG *) 	0xFFFFF818) // (PIOC) Output Status Register
#define	AT91C_PIOC_OER  ((AT91_REG *) 	0xFFFFF810) // (PIOC) Output Enable Register
#define	AT91C_PIOC_PSR  ((AT91_REG *) 	0xFFFFF808) // (PIOC) PIO Status Register
#define	AT91C_PIOC_PER  ((AT91_REG *) 	0xFFFFF800) // (PIOC) PIO Enable Register
#define	AT91C_PIOC_BSR  ((AT91_REG *) 	0xFFFFF874) // (PIOC) Select B Register
#define	AT91C_PIOC_PPUER ((AT91_REG *) 	0xFFFFF864) // (PIOC) Pull-up Enable Register
// ========== Register definition for PIOB peripheral ========== 
#define	AT91C_PIOB_OWSR ((AT91_REG *) 	0xFFFFF6A8) // (PIOB) Output Write Status Register
#define	AT91C_PIOB_PPUSR ((AT91_REG *) 	0xFFFFF668) // (PIOB) Pad Pull-up Status Register
#define	AT91C_PIOB_PPUDR ((AT91_REG *) 	0xFFFFF660) // (PIOB) Pull-up Disable Register
#define	AT91C_PIOB_MDSR ((AT91_REG *) 	0xFFFFF658) // (PIOB) Multi-driver Status Register
#define	AT91C_PIOB_MDER ((AT91_REG *) 	0xFFFFF650) // (PIOB) Multi-driver Enable Register
#define	AT91C_PIOB_IMR  ((AT91_REG *) 	0xFFFFF648) // (PIOB) Interrupt Mask Register
#define	AT91C_PIOB_OSR  ((AT91_REG *) 	0xFFFFF618) // (PIOB) Output Status Register
#define	AT91C_PIOB_OER  ((AT91_REG *) 	0xFFFFF610) // (PIOB) Output Enable Register
#define	AT91C_PIOB_PSR  ((AT91_REG *) 	0xFFFFF608) // (PIOB) PIO Status Register
#define	AT91C_PIOB_PER  ((AT91_REG *) 	0xFFFFF600) // (PIOB) PIO Enable Register
#define	AT91C_PIOB_BSR  ((AT91_REG *) 	0xFFFFF674) // (PIOB) Select B Register
#define	AT91C_PIOB_PPUER ((AT91_REG *) 	0xFFFFF664) // (PIOB) Pull-up Enable Register
#define	AT91C_PIOB_IFDR ((AT91_REG *) 	0xFFFFF624) // (PIOB) Input Filter Disable Register
#define	AT91C_PIOB_ODR  ((AT91_REG *) 	0xFFFFF614) // (PIOB) Output Disable Registerr
#define	AT91C_PIOB_ABSR ((AT91_REG *) 	0xFFFFF678) // (PIOB) AB Select Status Register
#define	AT91C_PIOB_ASR  ((AT91_REG *) 	0xFFFFF670) // (PIOB) Select A Register
#define	AT91C_PIOB_IFER ((AT91_REG *) 	0xFFFFF620) // (PIOB) Input Filter Enable Register
#define	AT91C_PIOB_IFSR ((AT91_REG *) 	0xFFFFF628) // (PIOB) Input Filter Status Register
#define	AT91C_PIOB_SODR ((AT91_REG *) 	0xFFFFF630) // (PIOB) Set Output Data Register
#define	AT91C_PIOB_ODSR ((AT91_REG *) 	0xFFFFF638) // (PIOB) Output Data Status Register
#define	AT91C_PIOB_CODR ((AT91_REG *) 	0xFFFFF634) // (PIOB) Clear Output Data Register
#define	AT91C_PIOB_PDSR ((AT91_REG *) 	0xFFFFF63C) // (PIOB) Pin Data Status Register
#define	AT91C_PIOB_OWER ((AT91_REG *) 	0xFFFFF6A0) // (PIOB) Output Write Enable Register
#define	AT91C_PIOB_IER  ((AT91_REG *) 	0xFFFFF640) // (PIOB) Interrupt Enable Register
#define	AT91C_PIOB_OWDR ((AT91_REG *) 	0xFFFFF6A4) // (PIOB) Output Write Disable Register
#define	AT91C_PIOB_MDDR ((AT91_REG *) 	0xFFFFF654) // (PIOB) Multi-driver Disable Register
#define	AT91C_PIOB_ISR  ((AT91_REG *) 	0xFFFFF64C) // (PIOB) Interrupt Status Register
#define	AT91C_PIOB_IDR  ((AT91_REG *) 	0xFFFFF644) // (PIOB) Interrupt Disable Register
#define	AT91C_PIOB_PDR  ((AT91_REG *) 	0xFFFFF604) // (PIOB) PIO Disable Register
// ========== Register definition for PIOA peripheral ========== 
#define	AT91C_PIOA_IMR  ((AT91_REG *) 	0xFFFFF448) // (PIOA) Interrupt Mask Register
#define	AT91C_PIOA_IER  ((AT91_REG *) 	0xFFFFF440) // (PIOA) Interrupt Enable Register
#define	AT91C_PIOA_OWDR ((AT91_REG *) 	0xFFFFF4A4) // (PIOA) Output Write Disable Register
#define	AT91C_PIOA_ISR  ((AT91_REG *) 	0xFFFFF44C) // (PIOA) Interrupt Status Register
#define	AT91C_PIOA_PPUDR ((AT91_REG *) 	0xFFFFF460) // (PIOA) Pull-up Disable Register
#define	AT91C_PIOA_MDSR ((AT91_REG *) 	0xFFFFF458) // (PIOA) Multi-driver Status Register
#define	AT91C_PIOA_MDER ((AT91_REG *) 	0xFFFFF450) // (PIOA) Multi-driver Enable Register
#define	AT91C_PIOA_PER  ((AT91_REG *) 	0xFFFFF400) // (PIOA) PIO Enable Register
#define	AT91C_PIOA_PSR  ((AT91_REG *) 	0xFFFFF408) // (PIOA) PIO Status Register
#define	AT91C_PIOA_OER  ((AT91_REG *) 	0xFFFFF410) // (PIOA) Output Enable Register
#define	AT91C_PIOA_BSR  ((AT91_REG *) 	0xFFFFF474) // (PIOA) Select B Register
#define	AT91C_PIOA_PPUER ((AT91_REG *) 	0xFFFFF464) // (PIOA) Pull-up Enable Register
#define	AT91C_PIOA_MDDR ((AT91_REG *) 	0xFFFFF454) // (PIOA) Multi-driver Disable Register
#define	AT91C_PIOA_PDR  ((AT91_REG *) 	0xFFFFF404) // (PIOA) PIO Disable Register
#define	AT91C_PIOA_ODR  ((AT91_REG *) 	0xFFFFF414) // (PIOA) Output Disable Registerr
#define	AT91C_PIOA_IFDR ((AT91_REG *) 	0xFFFFF424) // (PIOA) Input Filter Disable Register
#define	AT91C_PIOA_ABSR ((AT91_REG *) 	0xFFFFF478) // (PIOA) AB Select Status Register
#define	AT91C_PIOA_ASR  ((AT91_REG *) 	0xFFFFF470) // (PIOA) Select A Register
#define	AT91C_PIOA_PPUSR ((AT91_REG *) 	0xFFFFF468) // (PIOA) Pad Pull-up Status Register
#define	AT91C_PIOA_ODSR ((AT91_REG *) 	0xFFFFF438) // (PIOA) Output Data Status Register
#define	AT91C_PIOA_SODR ((AT91_REG *) 	0xFFFFF430) // (PIOA) Set Output Data Register
#define	AT91C_PIOA_IFSR ((AT91_REG *) 	0xFFFFF428) // (PIOA) Input Filter Status Register
#define	AT91C_PIOA_IFER ((AT91_REG *) 	0xFFFFF420) // (PIOA) Input Filter Enable Register
#define	AT91C_PIOA_OSR  ((AT91_REG *) 	0xFFFFF418) // (PIOA) Output Status Register
#define	AT91C_PIOA_IDR  ((AT91_REG *) 	0xFFFFF444) // (PIOA) Interrupt Disable Register
#define	AT91C_PIOA_PDSR ((AT91_REG *) 	0xFFFFF43C) // (PIOA) Pin Data Status Register
#define	AT91C_PIOA_CODR ((AT91_REG *) 	0xFFFFF434) // (PIOA) Clear Output Data Register
#define	AT91C_PIOA_OWSR ((AT91_REG *) 	0xFFFFF4A8) // (PIOA) Output Write Status Register
#define	AT91C_PIOA_OWER ((AT91_REG *) 	0xFFFFF4A0) // (PIOA) Output Write Enable Register
// ========== Register definition for DBGU peripheral ========== 
#define	AT91C_DBGU_C2R  ((AT91_REG *) 	0xFFFFF244) // (DBGU) Chip ID2 Register
#define	AT91C_DBGU_THR  ((AT91_REG *) 	0xFFFFF21C) // (DBGU) Transmitter Holding Register
#define	AT91C_DBGU_CSR  ((AT91_REG *) 	0xFFFFF214) // (DBGU) Channel Status Register
#define	AT91C_DBGU_IDR  ((AT91_REG *) 	0xFFFFF20C) // (DBGU) Interrupt Disable Register
#define	AT91C_DBGU_MR   ((AT91_REG *) 	0xFFFFF204) // (DBGU) Mode Register
#define	AT91C_DBGU_FNTR ((AT91_REG *) 	0xFFFFF248) // (DBGU) Force NTRST Register
#define	AT91C_DBGU_C1R  ((AT91_REG *) 	0xFFFFF240) // (DBGU) Chip ID1 Register
#define	AT91C_DBGU_BRGR ((AT91_REG *) 	0xFFFFF220) // (DBGU) Baud Rate Generator Register
#define	AT91C_DBGU_RHR  ((AT91_REG *) 	0xFFFFF218) // (DBGU) Receiver Holding Register
#define	AT91C_DBGU_IMR  ((AT91_REG *) 	0xFFFFF210) // (DBGU) Interrupt Mask Register
#define	AT91C_DBGU_IER  ((AT91_REG *) 	0xFFFFF208) // (DBGU) Interrupt Enable Register
#define	AT91C_DBGU_CR   ((AT91_REG *) 	0xFFFFF200) // (DBGU) Control Register
// ========== Register definition for PDC_DBGU peripheral ========== 
#define	AT91C_DBGU_TNCR ((AT91_REG *) 	0xFFFFF31C) // (PDC_DBGU) Transmit Next Counter Register
#define	AT91C_DBGU_RNCR ((AT91_REG *) 	0xFFFFF314) // (PDC_DBGU) Receive Next Counter Register
#define	AT91C_DBGU_PTCR ((AT91_REG *) 	0xFFFFF320) // (PDC_DBGU) PDC Transfer Control Register
#define	AT91C_DBGU_PTSR ((AT91_REG *) 	0xFFFFF324) // (PDC_DBGU) PDC Transfer Status Register
#define	AT91C_DBGU_RCR  ((AT91_REG *) 	0xFFFFF304) // (PDC_DBGU) Receive Counter Register
#define	AT91C_DBGU_TCR  ((AT91_REG *) 	0xFFFFF30C) // (PDC_DBGU) Transmit Counter Register
#define	AT91C_DBGU_RPR  ((AT91_REG *) 	0xFFFFF300) // (PDC_DBGU) Receive Pointer Register
#define	AT91C_DBGU_TPR  ((AT91_REG *) 	0xFFFFF308) // (PDC_DBGU) Transmit Pointer Register
#define	AT91C_DBGU_RNPR ((AT91_REG *) 	0xFFFFF310) // (PDC_DBGU) Receive Next Pointer Register
#define	AT91C_DBGU_TNPR ((AT91_REG *) 	0xFFFFF318) // (PDC_DBGU) Transmit Next Pointer Register
// ========== Register definition for AIC peripheral ========== 
#define	AT91C_AIC_ICCR  ((AT91_REG *) 	0xFFFFF128) // (AIC) Interrupt Clear Command Register
#define	AT91C_AIC_IECR  ((AT91_REG *) 	0xFFFFF120) // (AIC) Interrupt Enable Command Register
#define	AT91C_AIC_SMR   ((AT91_REG *) 	0xFFFFF000) // (AIC) Source Mode Register
#define	AT91C_AIC_ISCR  ((AT91_REG *) 	0xFFFFF12C) // (AIC) Interrupt Set Command Register
#define	AT91C_AIC_EOICR ((AT91_REG *) 	0xFFFFF130) // (AIC) End of Interrupt Command Register
#define	AT91C_AIC_DCR   ((AT91_REG *) 	0xFFFFF138) // (AIC) Debug Control Register (Protect)
#define	AT91C_AIC_FFER  ((AT91_REG *) 	0xFFFFF140) // (AIC) Fast Forcing Enable Register
#define	AT91C_AIC_SVR   ((AT91_REG *) 	0xFFFFF080) // (AIC) Source Vector Register
#define	AT91C_AIC_SPU   ((AT91_REG *) 	0xFFFFF134) // (AIC) Spurious Vector Register
#define	AT91C_AIC_FFDR  ((AT91_REG *) 	0xFFFFF144) // (AIC) Fast Forcing Disable Register
#define	AT91C_AIC_FVR   ((AT91_REG *) 	0xFFFFF104) // (AIC) FIQ Vector Register
#define	AT91C_AIC_FFSR  ((AT91_REG *) 	0xFFFFF148) // (AIC) Fast Forcing Status Register
#define	AT91C_AIC_IMR   ((AT91_REG *) 	0xFFFFF110) // (AIC) Interrupt Mask Register
#define	AT91C_AIC_ISR   ((AT91_REG *) 	0xFFFFF108) // (AIC) Interrupt Status Register
#define	AT91C_AIC_IVR   ((AT91_REG *) 	0xFFFFF100) // (AIC) IRQ Vector Register
#define	AT91C_AIC_IDCR  ((AT91_REG *) 	0xFFFFF124) // (AIC) Interrupt Disable Command Register
#define	AT91C_AIC_CISR  ((AT91_REG *) 	0xFFFFF114) // (AIC) Core Interrupt Status Register
#define	AT91C_AIC_IPR   ((AT91_REG *) 	0xFFFFF10C) // (AIC) Interrupt Pending Register
// ========== Register definition for PDC_SPI peripheral ========== 
#define	AT91C_SPI_PTCR  ((AT91_REG *) 	0xFFFE0120) // (PDC_SPI) PDC Transfer Control Register
#define	AT91C_SPI_TNPR  ((AT91_REG *) 	0xFFFE0118) // (PDC_SPI) Transmit Next Pointer Register
#define	AT91C_SPI_RNPR  ((AT91_REG *) 	0xFFFE0110) // (PDC_SPI) Receive Next Pointer Register
#define	AT91C_SPI_TPR   ((AT91_REG *) 	0xFFFE0108) // (PDC_SPI) Transmit Pointer Register
#define	AT91C_SPI_RPR   ((AT91_REG *) 	0xFFFE0100) // (PDC_SPI) Receive Pointer Register
#define	AT91C_SPI_PTSR  ((AT91_REG *) 	0xFFFE0124) // (PDC_SPI) PDC Transfer Status Register
#define	AT91C_SPI_TNCR  ((AT91_REG *) 	0xFFFE011C) // (PDC_SPI) Transmit Next Counter Register
#define	AT91C_SPI_RNCR  ((AT91_REG *) 	0xFFFE0114) // (PDC_SPI) Receive Next Counter Register
#define	AT91C_SPI_TCR   ((AT91_REG *) 	0xFFFE010C) // (PDC_SPI) Transmit Counter Register
#define	AT91C_SPI_RCR   ((AT91_REG *) 	0xFFFE0104) // (PDC_SPI) Receive Counter Register
// ========== Register definition for SPI peripheral ========== 
#define	AT91C_SPI_CSR   ((AT91_REG *) 	0xFFFE0030) // (SPI) Chip Select Register
#define	AT91C_SPI_IDR   ((AT91_REG *) 	0xFFFE0018) // (SPI) Interrupt Disable Register
#define	AT91C_SPI_SR    ((AT91_REG *) 	0xFFFE0010) // (SPI) Status Register
#define	AT91C_SPI_RDR   ((AT91_REG *) 	0xFFFE0008) // (SPI) Receive Data Register
#define	AT91C_SPI_CR    ((AT91_REG *) 	0xFFFE0000) // (SPI) Control Register
#define	AT91C_SPI_IMR   ((AT91_REG *) 	0xFFFE001C) // (SPI) Interrupt Mask Register
#define	AT91C_SPI_IER   ((AT91_REG *) 	0xFFFE0014) // (SPI) Interrupt Enable Register
#define	AT91C_SPI_TDR   ((AT91_REG *) 	0xFFFE000C) // (SPI) Transmit Data Register
#define	AT91C_SPI_MR    ((AT91_REG *) 	0xFFFE0004) // (SPI) Mode Register
// ========== Register definition for PDC_SSC2 peripheral ========== 
#define	AT91C_SSC2_PTCR ((AT91_REG *) 	0xFFFD8120) // (PDC_SSC2) PDC Transfer Control Register
#define	AT91C_SSC2_TNPR ((AT91_REG *) 	0xFFFD8118) // (PDC_SSC2) Transmit Next Pointer Register
#define	AT91C_SSC2_RNPR ((AT91_REG *) 	0xFFFD8110) // (PDC_SSC2) Receive Next Pointer Register
#define	AT91C_SSC2_TPR  ((AT91_REG *) 	0xFFFD8108) // (PDC_SSC2) Transmit Pointer Register
#define	AT91C_SSC2_RPR  ((AT91_REG *) 	0xFFFD8100) // (PDC_SSC2) Receive Pointer Register
#define	AT91C_SSC2_PTSR ((AT91_REG *) 	0xFFFD8124) // (PDC_SSC2) PDC Transfer Status Register
#define	AT91C_SSC2_TNCR ((AT91_REG *) 	0xFFFD811C) // (PDC_SSC2) Transmit Next Counter Register
#define	AT91C_SSC2_RNCR ((AT91_REG *) 	0xFFFD8114) // (PDC_SSC2) Receive Next Counter Register
#define	AT91C_SSC2_TCR  ((AT91_REG *) 	0xFFFD810C) // (PDC_SSC2) Transmit Counter Register
#define	AT91C_SSC2_RCR  ((AT91_REG *) 	0xFFFD8104) // (PDC_SSC2) Receive Counter Register
// ========== Register definition for SSC2 peripheral ========== 
#define	AT91C_SSC2_IMR  ((AT91_REG *) 	0xFFFD804C) // (SSC2) Interrupt Mask Register
#define	AT91C_SSC2_IER  ((AT91_REG *) 	0xFFFD8044) // (SSC2) Interrupt Enable Register
#define	AT91C_SSC2_RC1R ((AT91_REG *) 	0xFFFD803C) // (SSC2) Receive Compare 1 Register
#define	AT91C_SSC2_TSHR ((AT91_REG *) 	0xFFFD8034) // (SSC2) Transmit Sync Holding Register
#define	AT91C_SSC2_CMR  ((AT91_REG *) 	0xFFFD8004) // (SSC2) Clock Mode Register
#define	AT91C_SSC2_IDR  ((AT91_REG *) 	0xFFFD8048) // (SSC2) Interrupt Disable Register
#define	AT91C_SSC2_TCMR ((AT91_REG *) 	0xFFFD8018) // (SSC2) Transmit Clock Mode Register
#define	AT91C_SSC2_RCMR ((AT91_REG *) 	0xFFFD8010) // (SSC2) Receive Clock ModeRegister
#define	AT91C_SSC2_CR   ((AT91_REG *) 	0xFFFD8000) // (SSC2) Control Register
#define	AT91C_SSC2_RFMR ((AT91_REG *) 	0xFFFD8014) // (SSC2) Receive Frame Mode Register
#define	AT91C_SSC2_TFMR ((AT91_REG *) 	0xFFFD801C) // (SSC2) Transmit Frame Mode Register
#define	AT91C_SSC2_THR  ((AT91_REG *) 	0xFFFD8024) // (SSC2) Transmit Holding Register
#define	AT91C_SSC2_SR   ((AT91_REG *) 	0xFFFD8040) // (SSC2) Status Register
#define	AT91C_SSC2_RC0R ((AT91_REG *) 	0xFFFD8038) // (SSC2) Receive Compare 0 Register
#define	AT91C_SSC2_RSHR ((AT91_REG *) 	0xFFFD8030) // (SSC2) Receive Sync Holding Register
#define	AT91C_SSC2_RHR  ((AT91_REG *) 	0xFFFD8020) // (SSC2) Receive Holding Register
// ========== Register definition for PDC_SSC1 peripheral ========== 
#define	AT91C_SSC1_PTCR ((AT91_REG *) 	0xFFFD4120) // (PDC_SSC1) PDC Transfer Control Register
#define	AT91C_SSC1_TNPR ((AT91_REG *) 	0xFFFD4118) // (PDC_SSC1) Transmit Next Pointer Register
#define	AT91C_SSC1_RNPR ((AT91_REG *) 	0xFFFD4110) // (PDC_SSC1) Receive Next Pointer Register
#define	AT91C_SSC1_TPR  ((AT91_REG *) 	0xFFFD4108) // (PDC_SSC1) Transmit Pointer Register
#define	AT91C_SSC1_RPR  ((AT91_REG *) 	0xFFFD4100) // (PDC_SSC1) Receive Pointer Register
#define	AT91C_SSC1_PTSR ((AT91_REG *) 	0xFFFD4124) // (PDC_SSC1) PDC Transfer Status Register
#define	AT91C_SSC1_TNCR ((AT91_REG *) 	0xFFFD411C) // (PDC_SSC1) Transmit Next Counter Register
#define	AT91C_SSC1_RNCR ((AT91_REG *) 	0xFFFD4114) // (PDC_SSC1) Receive Next Counter Register
#define	AT91C_SSC1_TCR  ((AT91_REG *) 	0xFFFD410C) // (PDC_SSC1) Transmit Counter Register
#define	AT91C_SSC1_RCR  ((AT91_REG *) 	0xFFFD4104) // (PDC_SSC1) Receive Counter Register
// ========== Register definition for SSC1 peripheral ========== 
#define	AT91C_SSC1_RFMR ((AT91_REG *) 	0xFFFD4014) // (SSC1) Receive Frame Mode Register
#define	AT91C_SSC1_CMR  ((AT91_REG *) 	0xFFFD4004) // (SSC1) Clock Mode Register
#define	AT91C_SSC1_IDR  ((AT91_REG *) 	0xFFFD4048) // (SSC1) Interrupt Disable Register
#define	AT91C_SSC1_SR   ((AT91_REG *) 	0xFFFD4040) // (SSC1) Status Register
#define	AT91C_SSC1_RC0R ((AT91_REG *) 	0xFFFD4038) // (SSC1) Receive Compare 0 Register
#define	AT91C_SSC1_RSHR ((AT91_REG *) 	0xFFFD4030) // (SSC1) Receive Sync Holding Register
#define	AT91C_SSC1_RHR  ((AT91_REG *) 	0xFFFD4020) // (SSC1) Receive Holding Register
#define	AT91C_SSC1_TCMR ((AT91_REG *) 	0xFFFD4018) // (SSC1) Transmit Clock Mode Register
#define	AT91C_SSC1_RCMR ((AT91_REG *) 	0xFFFD4010) // (SSC1) Receive Clock ModeRegister
#define	AT91C_SSC1_CR   ((AT91_REG *) 	0xFFFD4000) // (SSC1) Control Register
#define	AT91C_SSC1_IMR  ((AT91_REG *) 	0xFFFD404C) // (SSC1) Interrupt Mask Register
#define	AT91C_SSC1_IER  ((AT91_REG *) 	0xFFFD4044) // (SSC1) Interrupt Enable Register
#define	AT91C_SSC1_RC1R ((AT91_REG *) 	0xFFFD403C) // (SSC1) Receive Compare 1 Register
#define	AT91C_SSC1_TSHR ((AT91_REG *) 	0xFFFD4034) // (SSC1) Transmit Sync Holding Register
#define	AT91C_SSC1_THR  ((AT91_REG *) 	0xFFFD4024) // (SSC1) Transmit Holding Register
#define	AT91C_SSC1_TFMR ((AT91_REG *) 	0xFFFD401C) // (SSC1) Transmit Frame Mode Register
// ========== Register definition for PDC_SSC0 peripheral ========== 
#define	AT91C_SSC0_PTCR ((AT91_REG *) 	0xFFFD0120) // (PDC_SSC0) PDC Transfer Control Register
#define	AT91C_SSC0_TNPR ((AT91_REG *) 	0xFFFD0118) // (PDC_SSC0) Transmit Next Pointer Register
#define	AT91C_SSC0_RNPR ((AT91_REG *) 	0xFFFD0110) // (PDC_SSC0) Receive Next Pointer Register
#define	AT91C_SSC0_TPR  ((AT91_REG *) 	0xFFFD0108) // (PDC_SSC0) Transmit Pointer Register
#define	AT91C_SSC0_RPR  ((AT91_REG *) 	0xFFFD0100) // (PDC_SSC0) Receive Pointer Register
#define	AT91C_SSC0_PTSR ((AT91_REG *) 	0xFFFD0124) // (PDC_SSC0) PDC Transfer Status Register
#define	AT91C_SSC0_TNCR ((AT91_REG *) 	0xFFFD011C) // (PDC_SSC0) Transmit Next Counter Register
#define	AT91C_SSC0_RNCR ((AT91_REG *) 	0xFFFD0114) // (PDC_SSC0) Receive Next Counter Register
#define	AT91C_SSC0_TCR  ((AT91_REG *) 	0xFFFD010C) // (PDC_SSC0) Transmit Counter Register
#define	AT91C_SSC0_RCR  ((AT91_REG *) 	0xFFFD0104) // (PDC_SSC0) Receive Counter Register
// ========== Register definition for SSC0 peripheral ========== 
#define	AT91C_SSC0_IMR  ((AT91_REG *) 	0xFFFD004C) // (SSC0) Interrupt Mask Register
#define	AT91C_SSC0_IER  ((AT91_REG *) 	0xFFFD0044) // (SSC0) Interrupt Enable Register
#define	AT91C_SSC0_RC1R ((AT91_REG *) 	0xFFFD003C) // (SSC0) Receive Compare 1 Register
#define	AT91C_SSC0_TSHR ((AT91_REG *) 	0xFFFD0034) // (SSC0) Transmit Sync Holding Register
#define	AT91C_SSC0_THR  ((AT91_REG *) 	0xFFFD0024) // (SSC0) Transmit Holding Register
#define	AT91C_SSC0_TFMR ((AT91_REG *) 	0xFFFD001C) // (SSC0) Transmit Frame Mode Register
#define	AT91C_SSC0_RFMR ((AT91_REG *) 	0xFFFD0014) // (SSC0) Receive Frame Mode Register
#define	AT91C_SSC0_CMR  ((AT91_REG *) 	0xFFFD0004) // (SSC0) Clock Mode Register
#define	AT91C_SSC0_IDR  ((AT91_REG *) 	0xFFFD0048) // (SSC0) Interrupt Disable Register
#define	AT91C_SSC0_SR   ((AT91_REG *) 	0xFFFD0040) // (SSC0) Status Register
#define	AT91C_SSC0_RC0R ((AT91_REG *) 	0xFFFD0038) // (SSC0) Receive Compare 0 Register
#define	AT91C_SSC0_RSHR ((AT91_REG *) 	0xFFFD0030) // (SSC0) Receive Sync Holding Register
#define	AT91C_SSC0_RHR  ((AT91_REG *) 	0xFFFD0020) // (SSC0) Receive Holding Register
#define	AT91C_SSC0_TCMR ((AT91_REG *) 	0xFFFD0018) // (SSC0) Transmit Clock Mode Register
#define	AT91C_SSC0_RCMR ((AT91_REG *) 	0xFFFD0010) // (SSC0) Receive Clock ModeRegister
#define	AT91C_SSC0_CR   ((AT91_REG *) 	0xFFFD0000) // (SSC0) Control Register
// ========== Register definition for PDC_US3 peripheral ========== 
#define	AT91C_US3_PTSR  ((AT91_REG *) 	0xFFFCC124) // (PDC_US3) PDC Transfer Status Register
#define	AT91C_US3_TNCR  ((AT91_REG *) 	0xFFFCC11C) // (PDC_US3) Transmit Next Counter Register
#define	AT91C_US3_RNCR  ((AT91_REG *) 	0xFFFCC114) // (PDC_US3) Receive Next Counter Register
#define	AT91C_US3_TCR   ((AT91_REG *) 	0xFFFCC10C) // (PDC_US3) Transmit Counter Register
#define	AT91C_US3_RCR   ((AT91_REG *) 	0xFFFCC104) // (PDC_US3) Receive Counter Register
#define	AT91C_US3_PTCR  ((AT91_REG *) 	0xFFFCC120) // (PDC_US3) PDC Transfer Control Register
#define	AT91C_US3_TNPR  ((AT91_REG *) 	0xFFFCC118) // (PDC_US3) Transmit Next Pointer Register
#define	AT91C_US3_RNPR  ((AT91_REG *) 	0xFFFCC110) // (PDC_US3) Receive Next Pointer Register
#define	AT91C_US3_TPR   ((AT91_REG *) 	0xFFFCC108) // (PDC_US3) Transmit Pointer Register
#define	AT91C_US3_RPR   ((AT91_REG *) 	0xFFFCC100) // (PDC_US3) Receive Pointer Register
// ========== Register definition for US3 peripheral ========== 
#define	AT91C_US3_IF    ((AT91_REG *) 	0xFFFCC04C) // (US3) IRDA_FILTER Register
#define	AT91C_US3_NER   ((AT91_REG *) 	0xFFFCC044) // (US3) Nb Errors Register
#define	AT91C_US3_RTOR  ((AT91_REG *) 	0xFFFCC024) // (US3) Receiver Time-out Register
#define	AT91C_US3_THR   ((AT91_REG *) 	0xFFFCC01C) // (US3) Transmitter Holding Register
#define	AT91C_US3_CSR   ((AT91_REG *) 	0xFFFCC014) // (US3) Channel Status Register
#define	AT91C_US3_IDR   ((AT91_REG *) 	0xFFFCC00C) // (US3) Interrupt Disable Register
#define	AT91C_US3_MR    ((AT91_REG *) 	0xFFFCC004) // (US3) Mode Register
#define	AT91C_US3_XXR   ((AT91_REG *) 	0xFFFCC048) // (US3) XON_XOFF Register
#define	AT91C_US3_FIDI  ((AT91_REG *) 	0xFFFCC040) // (US3) FI_DI_Ratio Register
#define	AT91C_US3_TTGR  ((AT91_REG *) 	0xFFFCC028) // (US3) Transmitter Time-guard Register
#define	AT91C_US3_BRGR  ((AT91_REG *) 	0xFFFCC020) // (US3) Baud Rate Generator Register
#define	AT91C_US3_RHR   ((AT91_REG *) 	0xFFFCC018) // (US3) Receiver Holding Register
#define	AT91C_US3_IMR   ((AT91_REG *) 	0xFFFCC010) // (US3) Interrupt Mask Register
#define	AT91C_US3_IER   ((AT91_REG *) 	0xFFFCC008) // (US3) Interrupt Enable Register
#define	AT91C_US3_CR    ((AT91_REG *) 	0xFFFCC000) // (US3) Control Register
// ========== Register definition for PDC_US2 peripheral ========== 
#define	AT91C_US2_PTSR  ((AT91_REG *) 	0xFFFC8124) // (PDC_US2) PDC Transfer Status Register
#define	AT91C_US2_TNCR  ((AT91_REG *) 	0xFFFC811C) // (PDC_US2) Transmit Next Counter Register
#define	AT91C_US2_RNCR  ((AT91_REG *) 	0xFFFC8114) // (PDC_US2) Receive Next Counter Register
#define	AT91C_US2_TCR   ((AT91_REG *) 	0xFFFC810C) // (PDC_US2) Transmit Counter Register
#define	AT91C_US2_PTCR  ((AT91_REG *) 	0xFFFC8120) // (PDC_US2) PDC Transfer Control Register
#define	AT91C_US2_RCR   ((AT91_REG *) 	0xFFFC8104) // (PDC_US2) Receive Counter Register
#define	AT91C_US2_TNPR  ((AT91_REG *) 	0xFFFC8118) // (PDC_US2) Transmit Next Pointer Register
#define	AT91C_US2_RPR   ((AT91_REG *) 	0xFFFC8100) // (PDC_US2) Receive Pointer Register
#define	AT91C_US2_TPR   ((AT91_REG *) 	0xFFFC8108) // (PDC_US2) Transmit Pointer Register
#define	AT91C_US2_RNPR  ((AT91_REG *) 	0xFFFC8110) // (PDC_US2) Receive Next Pointer Register
// ========== Register definition for US2 peripheral ========== 
#define	AT91C_US2_XXR   ((AT91_REG *) 	0xFFFC8048) // (US2) XON_XOFF Register
#define	AT91C_US2_FIDI  ((AT91_REG *) 	0xFFFC8040) // (US2) FI_DI_Ratio Register
#define	AT91C_US2_TTGR  ((AT91_REG *) 	0xFFFC8028) // (US2) Transmitter Time-guard Register
#define	AT91C_US2_BRGR  ((AT91_REG *) 	0xFFFC8020) // (US2) Baud Rate Generator Register
#define	AT91C_US2_RHR   ((AT91_REG *) 	0xFFFC8018) // (US2) Receiver Holding Register
#define	AT91C_US2_IMR   ((AT91_REG *) 	0xFFFC8010) // (US2) Interrupt Mask Register
#define	AT91C_US2_IER   ((AT91_REG *) 	0xFFFC8008) // (US2) Interrupt Enable Register
#define	AT91C_US2_CR    ((AT91_REG *) 	0xFFFC8000) // (US2) Control Register
#define	AT91C_US2_IF    ((AT91_REG *) 	0xFFFC804C) // (US2) IRDA_FILTER Register
#define	AT91C_US2_NER   ((AT91_REG *) 	0xFFFC8044) // (US2) Nb Errors Register
#define	AT91C_US2_RTOR  ((AT91_REG *) 	0xFFFC8024) // (US2) Receiver Time-out Register
#define	AT91C_US2_THR   ((AT91_REG *) 	0xFFFC801C) // (US2) Transmitter Holding Register
#define	AT91C_US2_CSR   ((AT91_REG *) 	0xFFFC8014) // (US2) Channel Status Register
#define	AT91C_US2_IDR   ((AT91_REG *) 	0xFFFC800C) // (US2) Interrupt Disable Register
#define	AT91C_US2_MR    ((AT91_REG *) 	0xFFFC8004) // (US2) Mode Register
// ========== Register definition for PDC_US1 peripheral ========== 
#define	AT91C_US1_PTSR  ((AT91_REG *) 	0xFFFC4124) // (PDC_US1) PDC Transfer Status Register
#define	AT91C_US1_TNCR  ((AT91_REG *) 	0xFFFC411C) // (PDC_US1) Transmit Next Counter Register
#define	AT91C_US1_RNCR  ((AT91_REG *) 	0xFFFC4114) // (PDC_US1) Receive Next Counter Register
#define	AT91C_US1_TCR   ((AT91_REG *) 	0xFFFC410C) // (PDC_US1) Transmit Counter Register
#define	AT91C_US1_RCR   ((AT91_REG *) 	0xFFFC4104) // (PDC_US1) Receive Counter Register
#define	AT91C_US1_PTCR  ((AT91_REG *) 	0xFFFC4120) // (PDC_US1) PDC Transfer Control Register
#define	AT91C_US1_TNPR  ((AT91_REG *) 	0xFFFC4118) // (PDC_US1) Transmit Next Pointer Register
#define	AT91C_US1_RNPR  ((AT91_REG *) 	0xFFFC4110) // (PDC_US1) Receive Next Pointer Register
#define	AT91C_US1_TPR   ((AT91_REG *) 	0xFFFC4108) // (PDC_US1) Transmit Pointer Register
#define	AT91C_US1_RPR   ((AT91_REG *) 	0xFFFC4100) // (PDC_US1) Receive Pointer Register
// ========== Register definition for US1 peripheral ========== 
#define	AT91C_US1_XXR   ((AT91_REG *) 	0xFFFC4048) // (US1) XON_XOFF Register
#define	AT91C_US1_RHR   ((AT91_REG *) 	0xFFFC4018) // (US1) Receiver Holding Register
#define	AT91C_US1_IMR   ((AT91_REG *) 	0xFFFC4010) // (US1) Interrupt Mask Register
#define	AT91C_US1_IER   ((AT91_REG *) 	0xFFFC4008) // (US1) Interrupt Enable Register
#define	AT91C_US1_CR    ((AT91_REG *) 	0xFFFC4000) // (US1) Control Register
#define	AT91C_US1_RTOR  ((AT91_REG *) 	0xFFFC4024) // (US1) Receiver Time-out Register
#define	AT91C_US1_THR   ((AT91_REG *) 	0xFFFC401C) // (US1) Transmitter Holding Register
#define	AT91C_US1_CSR   ((AT91_REG *) 	0xFFFC4014) // (US1) Channel Status Register
#define	AT91C_US1_IDR   ((AT91_REG *) 	0xFFFC400C) // (US1) Interrupt Disable Register
#define	AT91C_US1_FIDI  ((AT91_REG *) 	0xFFFC4040) // (US1) FI_DI_Ratio Register
#define	AT91C_US1_BRGR  ((AT91_REG *) 	0xFFFC4020) // (US1) Baud Rate Generator Register
#define	AT91C_US1_TTGR  ((AT91_REG *) 	0xFFFC4028) // (US1) Transmitter Time-guard Register
#define	AT91C_US1_IF    ((AT91_REG *) 	0xFFFC404C) // (US1) IRDA_FILTER Register
#define	AT91C_US1_NER   ((AT91_REG *) 	0xFFFC4044) // (US1) Nb Errors Register
#define	AT91C_US1_MR    ((AT91_REG *) 	0xFFFC4004) // (US1) Mode Register
// ========== Register definition for PDC_US0 peripheral ========== 
#define	AT91C_US0_PTCR  ((AT91_REG *) 	0xFFFC0120) // (PDC_US0) PDC Transfer Control Register
#define	AT91C_US0_TNPR  ((AT91_REG *) 	0xFFFC0118) // (PDC_US0) Transmit Next Pointer Register
#define	AT91C_US0_RNPR  ((AT91_REG *) 	0xFFFC0110) // (PDC_US0) Receive Next Pointer Register
#define	AT91C_US0_TPR   ((AT91_REG *) 	0xFFFC0108) // (PDC_US0) Transmit Pointer Register
#define	AT91C_US0_RPR   ((AT91_REG *) 	0xFFFC0100) // (PDC_US0) Receive Pointer Register
#define	AT91C_US0_PTSR  ((AT91_REG *) 	0xFFFC0124) // (PDC_US0) PDC Transfer Status Register
#define	AT91C_US0_TNCR  ((AT91_REG *) 	0xFFFC011C) // (PDC_US0) Transmit Next Counter Register
#define	AT91C_US0_RNCR  ((AT91_REG *) 	0xFFFC0114) // (PDC_US0) Receive Next Counter Register
#define	AT91C_US0_TCR   ((AT91_REG *) 	0xFFFC010C) // (PDC_US0) Transmit Counter Register
#define	AT91C_US0_RCR   ((AT91_REG *) 	0xFFFC0104) // (PDC_US0) Receive Counter Register
// ========== Register definition for US0 peripheral ========== 
#define	AT91C_US0_TTGR  ((AT91_REG *) 	0xFFFC0028) // (US0) Transmitter Time-guard Register
#define	AT91C_US0_BRGR  ((AT91_REG *) 	0xFFFC0020) // (US0) Baud Rate Generator Register
#define	AT91C_US0_RHR   ((AT91_REG *) 	0xFFFC0018) // (US0) Receiver Holding Register
#define	AT91C_US0_IMR   ((AT91_REG *) 	0xFFFC0010) // (US0) Interrupt Mask Register
#define	AT91C_US0_NER   ((AT91_REG *) 	0xFFFC0044) // (US0) Nb Errors Register
#define	AT91C_US0_RTOR  ((AT91_REG *) 	0xFFFC0024) // (US0) Receiver Time-out Register
#define	AT91C_US0_XXR   ((AT91_REG *) 	0xFFFC0048) // (US0) XON_XOFF Register
#define	AT91C_US0_FIDI  ((AT91_REG *) 	0xFFFC0040) // (US0) FI_DI_Ratio Register
#define	AT91C_US0_CR    ((AT91_REG *) 	0xFFFC0000) // (US0) Control Register
#define	AT91C_US0_IER   ((AT91_REG *) 	0xFFFC0008) // (US0) Interrupt Enable Register
#define	AT91C_US0_IF    ((AT91_REG *) 	0xFFFC004C) // (US0) IRDA_FILTER Register
#define	AT91C_US0_MR    ((AT91_REG *) 	0xFFFC0004) // (US0) Mode Register
#define	AT91C_US0_IDR   ((AT91_REG *) 	0xFFFC000C) // (US0) Interrupt Disable Register
#define	AT91C_US0_CSR   ((AT91_REG *) 	0xFFFC0014) // (US0) Channel Status Register
#define	AT91C_US0_THR   ((AT91_REG *) 	0xFFFC001C) // (US0) Transmitter Holding Register
// ========== Register definition for TWI peripheral ========== 
#define	AT91C_TWI_RHR   ((AT91_REG *) 	0xFFFB8030) // (TWI) Receive Holding Register
#define	AT91C_TWI_IDR   ((AT91_REG *) 	0xFFFB8028) // (TWI) Interrupt Disable Register
#define	AT91C_TWI_SR    ((AT91_REG *) 	0xFFFB8020) // (TWI) Status Register
#define	AT91C_TWI_CWGR  ((AT91_REG *) 	0xFFFB8010) // (TWI) Clock Waveform Generator Register
#define	AT91C_TWI_SMR   ((AT91_REG *) 	0xFFFB8008) // (TWI) Slave Mode Register
#define	AT91C_TWI_CR    ((AT91_REG *) 	0xFFFB8000) // (TWI) Control Register
#define	AT91C_TWI_THR   ((AT91_REG *) 	0xFFFB8034) // (TWI) Transmit Holding Register
#define	AT91C_TWI_IMR   ((AT91_REG *) 	0xFFFB802C) // (TWI) Interrupt Mask Register
#define	AT91C_TWI_IER   ((AT91_REG *) 	0xFFFB8024) // (TWI) Interrupt Enable Register
#define	AT91C_TWI_IADR  ((AT91_REG *) 	0xFFFB800C) // (TWI) Internal Address Register
#define	AT91C_TWI_MMR   ((AT91_REG *) 	0xFFFB8004) // (TWI) Master Mode Register
// ========== Register definition for PDC_MCI peripheral ========== 
#define	AT91C_MCI_PTCR  ((AT91_REG *) 	0xFFFB4120) // (PDC_MCI) PDC Transfer Control Register
#define	AT91C_MCI_TNPR  ((AT91_REG *) 	0xFFFB4118) // (PDC_MCI) Transmit Next Pointer Register
#define	AT91C_MCI_RNPR  ((AT91_REG *) 	0xFFFB4110) // (PDC_MCI) Receive Next Pointer Register
#define	AT91C_MCI_TPR   ((AT91_REG *) 	0xFFFB4108) // (PDC_MCI) Transmit Pointer Register
#define	AT91C_MCI_RPR   ((AT91_REG *) 	0xFFFB4100) // (PDC_MCI) Receive Pointer Register
#define	AT91C_MCI_PTSR  ((AT91_REG *) 	0xFFFB4124) // (PDC_MCI) PDC Transfer Status Register
#define	AT91C_MCI_TNCR  ((AT91_REG *) 	0xFFFB411C) // (PDC_MCI) Transmit Next Counter Register
#define	AT91C_MCI_RNCR  ((AT91_REG *) 	0xFFFB4114) // (PDC_MCI) Receive Next Counter Register
#define	AT91C_MCI_TCR   ((AT91_REG *) 	0xFFFB410C) // (PDC_MCI) Transmit Counter Register
#define	AT91C_MCI_RCR   ((AT91_REG *) 	0xFFFB4104) // (PDC_MCI) Receive Counter Register
// ========== Register definition for MCI peripheral ========== 
#define	AT91C_MCI_IDR   ((AT91_REG *) 	0xFFFB4048) // (MCI) MCI Interrupt Disable Register
#define	AT91C_MCI_SR    ((AT91_REG *) 	0xFFFB4040) // (MCI) MCI Status Register
#define	AT91C_MCI_RDR   ((AT91_REG *) 	0xFFFB4030) // (MCI) MCI Receive Data Register
#define	AT91C_MCI_RSPR  ((AT91_REG *) 	0xFFFB4020) // (MCI) MCI Response Register
#define	AT91C_MCI_ARGR  ((AT91_REG *) 	0xFFFB4010) // (MCI) MCI Argument Register
#define	AT91C_MCI_DTOR  ((AT91_REG *) 	0xFFFB4008) // (MCI) MCI Data Timeout Register
#define	AT91C_MCI_CR    ((AT91_REG *) 	0xFFFB4000) // (MCI) MCI Control Register
#define	AT91C_MCI_IMR   ((AT91_REG *) 	0xFFFB404C) // (MCI) MCI Interrupt Mask Register
#define	AT91C_MCI_IER   ((AT91_REG *) 	0xFFFB4044) // (MCI) MCI Interrupt Enable Register
#define	AT91C_MCI_TDR   ((AT91_REG *) 	0xFFFB4034) // (MCI) MCI Transmit Data Register
#define	AT91C_MCI_CMDR  ((AT91_REG *) 	0xFFFB4014) // (MCI) MCI Command Register
#define	AT91C_MCI_SDCR  ((AT91_REG *) 	0xFFFB400C) // (MCI) MCI SD Card Register
#define	AT91C_MCI_MR    ((AT91_REG *) 	0xFFFB4004) // (MCI) MCI Mode Register
// ========== Register definition for UDP peripheral ========== 
#define	AT91C_UDP_ISR   ((AT91_REG *) 	0xFFFB001C) // (UDP) Interrupt Status Register
#define	AT91C_UDP_IDR   ((AT91_REG *) 	0xFFFB0014) // (UDP) Interrupt Disable Register
#define	AT91C_UDP_GLBSTATE ((AT91_REG *) 	0xFFFB0004) // (UDP) Global State Register
#define	AT91C_UDP_FDR   ((AT91_REG *) 	0xFFFB0050) // (UDP) Endpoint FIFO Data Register
#define	AT91C_UDP_CSR   ((AT91_REG *) 	0xFFFB0030) // (UDP) Endpoint Control and Status Register
#define	AT91C_UDP_RSTEP ((AT91_REG *) 	0xFFFB0028) // (UDP) Reset Endpoint Register
#define	AT91C_UDP_ICR   ((AT91_REG *) 	0xFFFB0020) // (UDP) Interrupt Clear Register
#define	AT91C_UDP_IMR   ((AT91_REG *) 	0xFFFB0018) // (UDP) Interrupt Mask Register
#define	AT91C_UDP_IER   ((AT91_REG *) 	0xFFFB0010) // (UDP) Interrupt Enable Register
#define	AT91C_UDP_FADDR ((AT91_REG *) 	0xFFFB0008) // (UDP) Function Address Register
#define	AT91C_UDP_NUM   ((AT91_REG *) 	0xFFFB0000) // (UDP) Frame Number Register
// ========== Register definition for TC5 peripheral ========== 
#define	AT91C_TC5_CMR   ((AT91_REG *) 	0xFFFA4084) // (TC5) Channel Mode Register
#define	AT91C_TC5_IDR   ((AT91_REG *) 	0xFFFA40A8) // (TC5) Interrupt Disable Register
#define	AT91C_TC5_SR    ((AT91_REG *) 	0xFFFA40A0) // (TC5) Status Register
#define	AT91C_TC5_RB    ((AT91_REG *) 	0xFFFA4098) // (TC5) Register B
#define	AT91C_TC5_CV    ((AT91_REG *) 	0xFFFA4090) // (TC5) Counter Value
#define	AT91C_TC5_CCR   ((AT91_REG *) 	0xFFFA4080) // (TC5) Channel Control Register
#define	AT91C_TC5_IMR   ((AT91_REG *) 	0xFFFA40AC) // (TC5) Interrupt Mask Register
#define	AT91C_TC5_IER   ((AT91_REG *) 	0xFFFA40A4) // (TC5) Interrupt Enable Register
#define	AT91C_TC5_RC    ((AT91_REG *) 	0xFFFA409C) // (TC5) Register C
#define	AT91C_TC5_RA    ((AT91_REG *) 	0xFFFA4094) // (TC5) Register A
// ========== Register definition for TC4 peripheral ========== 
#define	AT91C_TC4_IMR   ((AT91_REG *) 	0xFFFA406C) // (TC4) Interrupt Mask Register
#define	AT91C_TC4_IER   ((AT91_REG *) 	0xFFFA4064) // (TC4) Interrupt Enable Register
#define	AT91C_TC4_RC    ((AT91_REG *) 	0xFFFA405C) // (TC4) Register C
#define	AT91C_TC4_RA    ((AT91_REG *) 	0xFFFA4054) // (TC4) Register A
#define	AT91C_TC4_CMR   ((AT91_REG *) 	0xFFFA4044) // (TC4) Channel Mode Register
#define	AT91C_TC4_IDR   ((AT91_REG *) 	0xFFFA4068) // (TC4) Interrupt Disable Register
#define	AT91C_TC4_SR    ((AT91_REG *) 	0xFFFA4060) // (TC4) Status Register
#define	AT91C_TC4_RB    ((AT91_REG *) 	0xFFFA4058) // (TC4) Register B
#define	AT91C_TC4_CV    ((AT91_REG *) 	0xFFFA4050) // (TC4) Counter Value
#define	AT91C_TC4_CCR   ((AT91_REG *) 	0xFFFA4040) // (TC4) Channel Control Register
// ========== Register definition for TC3 peripheral ========== 
#define	AT91C_TC3_IMR   ((AT91_REG *) 	0xFFFA402C) // (TC3) Interrupt Mask Register
#define	AT91C_TC3_CV    ((AT91_REG *) 	0xFFFA4010) // (TC3) Counter Value
#define	AT91C_TC3_CCR   ((AT91_REG *) 	0xFFFA4000) // (TC3) Channel Control Register
#define	AT91C_TC3_IER   ((AT91_REG *) 	0xFFFA4024) // (TC3) Interrupt Enable Register
#define	AT91C_TC3_CMR   ((AT91_REG *) 	0xFFFA4004) // (TC3) Channel Mode Register
#define	AT91C_TC3_RA    ((AT91_REG *) 	0xFFFA4014) // (TC3) Register A
#define	AT91C_TC3_RC    ((AT91_REG *) 	0xFFFA401C) // (TC3) Register C
#define	AT91C_TC3_IDR   ((AT91_REG *) 	0xFFFA4028) // (TC3) Interrupt Disable Register
#define	AT91C_TC3_RB    ((AT91_REG *) 	0xFFFA4018) // (TC3) Register B
#define	AT91C_TC3_SR    ((AT91_REG *) 	0xFFFA4020) // (TC3) Status Register
// ========== Register definition for TCB1 peripheral ========== 
#define	AT91C_TCB1_BCR  ((AT91_REG *) 	0xFFFA4140) // (TCB1) TC Block Control Register
#define	AT91C_TCB1_BMR  ((AT91_REG *) 	0xFFFA4144) // (TCB1) TC Block Mode Register
// ========== Register definition for TC2 peripheral ========== 
#define	AT91C_TC2_IMR   ((AT91_REG *) 	0xFFFA00AC) // (TC2) Interrupt Mask Register
#define	AT91C_TC2_IER   ((AT91_REG *) 	0xFFFA00A4) // (TC2) Interrupt Enable Register
#define	AT91C_TC2_RC    ((AT91_REG *) 	0xFFFA009C) // (TC2) Register C
#define	AT91C_TC2_RA    ((AT91_REG *) 	0xFFFA0094) // (TC2) Register A
#define	AT91C_TC2_CMR   ((AT91_REG *) 	0xFFFA0084) // (TC2) Channel Mode Register
#define	AT91C_TC2_IDR   ((AT91_REG *) 	0xFFFA00A8) // (TC2) Interrupt Disable Register
#define	AT91C_TC2_SR    ((AT91_REG *) 	0xFFFA00A0) // (TC2) Status Register
#define	AT91C_TC2_RB    ((AT91_REG *) 	0xFFFA0098) // (TC2) Register B
#define	AT91C_TC2_CV    ((AT91_REG *) 	0xFFFA0090) // (TC2) Counter Value
#define	AT91C_TC2_CCR   ((AT91_REG *) 	0xFFFA0080) // (TC2) Channel Control Register
// ========== Register definition for TC1 peripheral ========== 
#define	AT91C_TC1_IMR   ((AT91_REG *) 	0xFFFA006C) // (TC1) Interrupt Mask Register
#define	AT91C_TC1_IER   ((AT91_REG *) 	0xFFFA0064) // (TC1) Interrupt Enable Register
#define	AT91C_TC1_RC    ((AT91_REG *) 	0xFFFA005C) // (TC1) Register C
#define	AT91C_TC1_RA    ((AT91_REG *) 	0xFFFA0054) // (TC1) Register A
#define	AT91C_TC1_CMR   ((AT91_REG *) 	0xFFFA0044) // (TC1) Channel Mode Register
#define	AT91C_TC1_IDR   ((AT91_REG *) 	0xFFFA0068) // (TC1) Interrupt Disable Register
#define	AT91C_TC1_SR    ((AT91_REG *) 	0xFFFA0060) // (TC1) Status Register
#define	AT91C_TC1_RB    ((AT91_REG *) 	0xFFFA0058) // (TC1) Register B
#define	AT91C_TC1_CV    ((AT91_REG *) 	0xFFFA0050) // (TC1) Counter Value
#define	AT91C_TC1_CCR   ((AT91_REG *) 	0xFFFA0040) // (TC1) Channel Control Register
// ========== Register definition for TC0 peripheral ========== 
#define	AT91C_TC0_IMR   ((AT91_REG *) 	0xFFFA002C) // (TC0) Interrupt Mask Register
#define	AT91C_TC0_IER   ((AT91_REG *) 	0xFFFA0024) // (TC0) Interrupt Enable Register
#define	AT91C_TC0_RC    ((AT91_REG *) 	0xFFFA001C) // (TC0) Register C
#define	AT91C_TC0_RA    ((AT91_REG *) 	0xFFFA0014) // (TC0) Register A
#define	AT91C_TC0_CMR   ((AT91_REG *) 	0xFFFA0004) // (TC0) Channel Mode Register
#define	AT91C_TC0_IDR   ((AT91_REG *) 	0xFFFA0028) // (TC0) Interrupt Disable Register
#define	AT91C_TC0_SR    ((AT91_REG *) 	0xFFFA0020) // (TC0) Status Register
#define	AT91C_TC0_RB    ((AT91_REG *) 	0xFFFA0018) // (TC0) Register B
#define	AT91C_TC0_CV    ((AT91_REG *) 	0xFFFA0010) // (TC0) Counter Value
#define	AT91C_TC0_CCR   ((AT91_REG *) 	0xFFFA0000) // (TC0) Channel Control Register
// ========== Register definition for TCB0 peripheral ========== 
#define	AT91C_TCB0_BMR  ((AT91_REG *) 	0xFFFA00C4) // (TCB0) TC Block Mode Register
#define	AT91C_TCB0_BCR  ((AT91_REG *) 	0xFFFA00C0) // (TCB0) TC Block Control Register
// ========== Register definition for UHP peripheral ========== 
#define	AT91C_UHP_HcRhDescriptorA ((AT91_REG *) 	0x00300048) // (UHP) Root Hub characteristics A
#define	AT91C_UHP_HcRhPortStatus ((AT91_REG *) 	0x00300054) // (UHP) Root Hub Port Status Register
#define	AT91C_UHP_HcRhDescriptorB ((AT91_REG *) 	0x0030004C) // (UHP) Root Hub characteristics B
#define	AT91C_UHP_HcControl ((AT91_REG *) 	0x00300004) // (UHP) Operating modes for the Host Controller
#define	AT91C_UHP_HcInterruptStatus ((AT91_REG *) 	0x0030000C) // (UHP) Interrupt Status Register
#define	AT91C_UHP_HcRhStatus ((AT91_REG *) 	0x00300050) // (UHP) Root Hub Status register
#define	AT91C_UHP_HcRevision ((AT91_REG *) 	0x00300000) // (UHP) Revision
#define	AT91C_UHP_HcCommandStatus ((AT91_REG *) 	0x00300008) // (UHP) Command & status Register
#define	AT91C_UHP_HcInterruptEnable ((AT91_REG *) 	0x00300010) // (UHP) Interrupt Enable Register
#define	AT91C_UHP_HcHCCA ((AT91_REG *) 	0x00300018) // (UHP) Pointer to the Host Controller Communication Area
#define	AT91C_UHP_HcControlHeadED ((AT91_REG *) 	0x00300020) // (UHP) First Endpoint Descriptor of the Control list
#define	AT91C_UHP_HcInterruptDisable ((AT91_REG *) 	0x00300014) // (UHP) Interrupt Disable Register
#define	AT91C_UHP_HcPeriodCurrentED ((AT91_REG *) 	0x0030001C) // (UHP) Current Isochronous or Interrupt Endpoint Descriptor
#define	AT91C_UHP_HcControlCurrentED ((AT91_REG *) 	0x00300024) // (UHP) Endpoint Control and Status Register
#define	AT91C_UHP_HcBulkCurrentED ((AT91_REG *) 	0x0030002C) // (UHP) Current endpoint of the Bulk list
#define	AT91C_UHP_HcFmInterval ((AT91_REG *) 	0x00300034) // (UHP) Bit time between 2 consecutive SOFs
#define	AT91C_UHP_HcBulkHeadED ((AT91_REG *) 	0x00300028) // (UHP) First endpoint register of the Bulk list
#define	AT91C_UHP_HcBulkDoneHead ((AT91_REG *) 	0x00300030) // (UHP) Last completed transfer descriptor
#define	AT91C_UHP_HcFmRemaining ((AT91_REG *) 	0x00300038) // (UHP) Bit time remaining in the current Frame
#define	AT91C_UHP_HcPeriodicStart ((AT91_REG *) 	0x00300040) // (UHP) Periodic Start
#define	AT91C_UHP_HcLSThreshold ((AT91_REG *) 	0x00300044) // (UHP) LS Threshold
#define	AT91C_UHP_HcFmNumber ((AT91_REG *) 	0x0030003C) // (UHP) Frame number
// ========== Register definition for EMAC peripheral ========== 
#define	AT91C_EMAC_RSR  ((AT91_REG *) 	0xFFFBC020) // (EMAC) Receive Status Register
#define	AT91C_EMAC_MAN  ((AT91_REG *) 	0xFFFBC034) // (EMAC) PHY Maintenance Register
#define	AT91C_EMAC_HSH  ((AT91_REG *) 	0xFFFBC090) // (EMAC) Hash Address High[63:32]
#define	AT91C_EMAC_MCOL ((AT91_REG *) 	0xFFFBC048) // (EMAC) Multiple Collision Frame Register
#define	AT91C_EMAC_IER  ((AT91_REG *) 	0xFFFBC028) // (EMAC) Interrupt Enable Register
#define	AT91C_EMAC_SA2H ((AT91_REG *) 	0xFFFBC0A4) // (EMAC) Specific Address 2 High, Last 2 bytes
#define	AT91C_EMAC_HSL  ((AT91_REG *) 	0xFFFBC094) // (EMAC) Hash Address Low[31:0]
#define	AT91C_EMAC_LCOL ((AT91_REG *) 	0xFFFBC05C) // (EMAC) Late Collision Register
#define	AT91C_EMAC_OK   ((AT91_REG *) 	0xFFFBC04C) // (EMAC) Frames Received OK Register
#define	AT91C_EMAC_CFG  ((AT91_REG *) 	0xFFFBC004) // (EMAC) Network Configuration Register
#define	AT91C_EMAC_SA3L ((AT91_REG *) 	0xFFFBC0A8) // (EMAC) Specific Address 3 Low, First 4 bytes
#define	AT91C_EMAC_SEQE ((AT91_REG *) 	0xFFFBC050) // (EMAC) Frame Check Sequence Error Register
#define	AT91C_EMAC_ECOL ((AT91_REG *) 	0xFFFBC060) // (EMAC) Excessive Collision Register
#define	AT91C_EMAC_ELR  ((AT91_REG *) 	0xFFFBC070) // (EMAC) Excessive Length Error Register
#define	AT91C_EMAC_SR   ((AT91_REG *) 	0xFFFBC008) // (EMAC) Network Status Register
#define	AT91C_EMAC_RBQP ((AT91_REG *) 	0xFFFBC018) // (EMAC) Receive Buffer Queue Pointer
#define	AT91C_EMAC_CSE  ((AT91_REG *) 	0xFFFBC064) // (EMAC) Carrier Sense Error Register
#define	AT91C_EMAC_RJB  ((AT91_REG *) 	0xFFFBC074) // (EMAC) Receive Jabber Register
#define	AT91C_EMAC_USF  ((AT91_REG *) 	0xFFFBC078) // (EMAC) Undersize Frame Register
#define	AT91C_EMAC_IDR  ((AT91_REG *) 	0xFFFBC02C) // (EMAC) Interrupt Disable Register
#define	AT91C_EMAC_SA1L ((AT91_REG *) 	0xFFFBC098) // (EMAC) Specific Address 1 Low, First 4 bytes
#define	AT91C_EMAC_IMR  ((AT91_REG *) 	0xFFFBC030) // (EMAC) Interrupt Mask Register
#define	AT91C_EMAC_FRA  ((AT91_REG *) 	0xFFFBC040) // (EMAC) Frames Transmitted OK Register
#define	AT91C_EMAC_SA3H ((AT91_REG *) 	0xFFFBC0AC) // (EMAC) Specific Address 3 High, Last 2 bytes
#define	AT91C_EMAC_SA1H ((AT91_REG *) 	0xFFFBC09C) // (EMAC) Specific Address 1 High, Last 2 bytes
#define	AT91C_EMAC_SCOL ((AT91_REG *) 	0xFFFBC044) // (EMAC) Single Collision Frame Register
#define	AT91C_EMAC_ALE  ((AT91_REG *) 	0xFFFBC054) // (EMAC) Alignment Error Register
#define	AT91C_EMAC_TAR  ((AT91_REG *) 	0xFFFBC00C) // (EMAC) Transmit Address Register
#define	AT91C_EMAC_SA4L ((AT91_REG *) 	0xFFFBC0B0) // (EMAC) Specific Address 4 Low, First 4 bytes
#define	AT91C_EMAC_SA2L ((AT91_REG *) 	0xFFFBC0A0) // (EMAC) Specific Address 2 Low, First 4 bytes
#define	AT91C_EMAC_TUE  ((AT91_REG *) 	0xFFFBC068) // (EMAC) Transmit Underrun Error Register
#define	AT91C_EMAC_DTE  ((AT91_REG *) 	0xFFFBC058) // (EMAC) Deferred Transmission Frame Register
#define	AT91C_EMAC_TCR  ((AT91_REG *) 	0xFFFBC010) // (EMAC) Transmit Control Register
#define	AT91C_EMAC_CTL  ((AT91_REG *) 	0xFFFBC000) // (EMAC) Network Control Register
#define	AT91C_EMAC_SA4H ((AT91_REG *) 	0xFFFBC0B4) // (EMAC) Specific Address 4 High, Last 2 bytesr
#define	AT91C_EMAC_CDE  ((AT91_REG *) 	0xFFFBC06C) // (EMAC) Code Error Register
#define	AT91C_EMAC_SQEE ((AT91_REG *) 	0xFFFBC07C) // (EMAC) SQE Test Error Register
#define	AT91C_EMAC_TSR  ((AT91_REG *) 	0xFFFBC014) // (EMAC) Transmit Status Register
#define	AT91C_EMAC_DRFC ((AT91_REG *) 	0xFFFBC080) // (EMAC) Discarded RX Frame Register
// ========== Register definition for EBI peripheral ========== 
#define	AT91C_EBI_CFGR  ((AT91_REG *) 	0xFFFFFF64) // (EBI) Configuration Register
#define	AT91C_EBI_CSA   ((AT91_REG *) 	0xFFFFFF60) // (EBI) Chip Select Assignment Register
// ========== Register definition for SMC2 peripheral ========== 
#define	AT91C_SMC2_CSR  ((AT91_REG *) 	0xFFFFFF70) // (SMC2) SMC2 Chip Select Register
// ========== Register definition for SDRC peripheral ========== 
#define	AT91C_SDRC_IMR  ((AT91_REG *) 	0xFFFFFFAC) // (SDRC) SDRAM Controller Interrupt Mask Register
#define	AT91C_SDRC_IER  ((AT91_REG *) 	0xFFFFFFA4) // (SDRC) SDRAM Controller Interrupt Enable Register
#define	AT91C_SDRC_SRR  ((AT91_REG *) 	0xFFFFFF9C) // (SDRC) SDRAM Controller Self Refresh Register
#define	AT91C_SDRC_TR   ((AT91_REG *) 	0xFFFFFF94) // (SDRC) SDRAM Controller Refresh Timer Register
#define	AT91C_SDRC_ISR  ((AT91_REG *) 	0xFFFFFFB0) // (SDRC) SDRAM Controller Interrupt Mask Register
#define	AT91C_SDRC_IDR  ((AT91_REG *) 	0xFFFFFFA8) // (SDRC) SDRAM Controller Interrupt Disable Register
#define	AT91C_SDRC_LPR  ((AT91_REG *) 	0xFFFFFFA0) // (SDRC) SDRAM Controller Low Power Register
#define	AT91C_SDRC_CR   ((AT91_REG *) 	0xFFFFFF98) // (SDRC) SDRAM Controller Configuration Register
#define	AT91C_SDRC_MR   ((AT91_REG *) 	0xFFFFFF90) // (SDRC) SDRAM Controller Mode Register
// ========== Register definition for BFC peripheral ========== 
#define	AT91C_BFC_MR    ((AT91_REG *) 	0xFFFFFFC0) // (BFC) BFC Mode Register

#include <at91/at91_pio_rm9200.h>

// *****************************************************************************
//               PERIPHERAL ID DEFINITIONS FOR AT91RM9200
// *****************************************************************************
#define	AT91C_ID_FIQ    0u // Advanced Interrupt Controller (FIQ)
#define	AT91C_ID_SYS    1u // System Peripheral
#define	AT91C_ID_PIOA   2u // Parallel IO Controller A 
#define	AT91C_ID_PIOB   3u // Parallel IO Controller B
#define	AT91C_ID_PIOC   4u // Parallel IO Controller C
#define	AT91C_ID_PIOD   5u // Parallel IO Controller D
#define	AT91C_ID_US0    6u // USART 0
#define	AT91C_ID_US1    7u // USART 1
#define	AT91C_ID_US2    8u // USART 2
#define	AT91C_ID_US3    9u // USART 3
#define	AT91C_ID_MCI    10u // Multimedia Card Interface
#define	AT91C_ID_UDP    11u // USB Device Port
#define	AT91C_ID_TWI    12u // Two-Wire Interface
#define	AT91C_ID_SPI    13u // Serial Peripheral Interface
#define	AT91C_ID_SSC0   14u // Serial Synchronous Controller 0
#define	AT91C_ID_SSC1   15u // Serial Synchronous Controller 1
#define	AT91C_ID_SSC2   16u // Serial Synchronous Controller 2
#define	AT91C_ID_TC0    17u // Timer Counter 0
#define	AT91C_ID_TC1    18u // Timer Counter 1
#define	AT91C_ID_TC2    19u // Timer Counter 2
#define	AT91C_ID_TC3    20u // Timer Counter 3
#define	AT91C_ID_TC4    21u // Timer Counter 4
#define	AT91C_ID_TC5    22u // Timer Counter 5
#define	AT91C_ID_UHP    23u // USB Host port
#define	AT91C_ID_EMAC   24u // Ethernet MAC
#define	AT91C_ID_IRQ0   25u // Advanced Interrupt Controller (IRQ0)
#define	AT91C_ID_IRQ1   26u // Advanced Interrupt Controller (IRQ1)
#define	AT91C_ID_IRQ2   27u // Advanced Interrupt Controller (IRQ2)
#define	AT91C_ID_IRQ3   28u // Advanced Interrupt Controller (IRQ3)
#define	AT91C_ID_IRQ4   29u // Advanced Interrupt Controller (IRQ4)
#define	AT91C_ID_IRQ5   30u // Advanced Interrupt Controller (IRQ5)
#define	AT91C_ID_IRQ6   31u // Advanced Interrupt Controller (IRQ6)

// *****************************************************************************
//               BASE ADDRESS DEFINITIONS FOR AT91RM9200
// *****************************************************************************
#define	AT91C_BASE_SYS       ((AT91PS_SYS) 	0xFFFFF000) // (SYS) Base Address
#define	AT91C_BASE_MC        ((AT91PS_MC) 	0xFFFFFF00) // (MC) Base Address
#define	AT91C_BASE_RTC       ((AT91PS_RTC) 	0xFFFFFE00) // (RTC) Base Address
#define	AT91C_BASE_ST        ((AT91PS_ST) 	0xFFFFFD00) // (ST) Base Address
#define	AT91C_BASE_PMC       ((AT91PS_PMC) 	0xFFFFFC00) // (PMC) Base Address
#define	AT91C_BASE_CKGR      ((AT91PS_CKGR) 	0xFFFFFC20) // (CKGR) Base Address
#define	AT91C_BASE_PIOD      ((AT91PS_PIO) 	0xFFFFFA00) // (PIOD) Base Address
#define	AT91C_BASE_PIOC      ((AT91PS_PIO) 	0xFFFFF800) // (PIOC) Base Address
#define	AT91C_BASE_PIOB      ((AT91PS_PIO) 	0xFFFFF600) // (PIOB) Base Address
#define	AT91C_BASE_PIOA      ((AT91PS_PIO) 	0xFFFFF400) // (PIOA) Base Address
#define	AT91C_BASE_DBGU      ((AT91PS_DBGU) 	0xFFFFF200) // (DBGU) Base Address
#define	AT91C_BASE_PDC_DBGU  ((AT91PS_PDC) 	0xFFFFF300) // (PDC_DBGU) Base Address
#define	AT91C_BASE_AIC       ((AT91PS_AIC) 	0xFFFFF000) // (AIC) Base Address
#define	AT91C_BASE_PDC_SPI   ((AT91PS_PDC) 	0xFFFE0100) // (PDC_SPI) Base Address
#define	AT91C_BASE_SPI       ((AT91PS_SPI) 	0xFFFE0000) // (SPI) Base Address
#define	AT91C_BASE_PDC_SSC2  ((AT91PS_PDC) 	0xFFFD8100) // (PDC_SSC2) Base Address
#define	AT91C_BASE_SSC2      ((AT91PS_SSC) 	0xFFFD8000) // (SSC2) Base Address
#define	AT91C_BASE_PDC_SSC1  ((AT91PS_PDC) 	0xFFFD4100) // (PDC_SSC1) Base Address
#define	AT91C_BASE_SSC1      ((AT91PS_SSC) 	0xFFFD4000) // (SSC1) Base Address
#define	AT91C_BASE_PDC_SSC0  ((AT91PS_PDC) 	0xFFFD0100) // (PDC_SSC0) Base Address
#define	AT91C_BASE_SSC0      ((AT91PS_SSC) 	0xFFFD0000) // (SSC0) Base Address
#define	AT91C_BASE_PDC_US3   ((AT91PS_PDC) 	0xFFFCC100) // (PDC_US3) Base Address
#define	AT91C_BASE_US3       ((AT91PS_USART) 	0xFFFCC000) // (US3) Base Address
#define	AT91C_BASE_PDC_US2   ((AT91PS_PDC) 	0xFFFC8100) // (PDC_US2) Base Address
#define	AT91C_BASE_US2       ((AT91PS_USART) 	0xFFFC8000) // (US2) Base Address
#define	AT91C_BASE_PDC_US1   ((AT91PS_PDC) 	0xFFFC4100) // (PDC_US1) Base Address
#define	AT91C_BASE_US1       ((AT91PS_USART) 	0xFFFC4000) // (US1) Base Address
#define	AT91C_BASE_PDC_US0   ((AT91PS_PDC) 	0xFFFC0100) // (PDC_US0) Base Address
#define	AT91C_BASE_US0       ((AT91PS_USART) 	0xFFFC0000) // (US0) Base Address
#define	AT91C_BASE_TWI       ((AT91PS_TWI) 	0xFFFB8000) // (TWI) Base Address
#define	AT91C_BASE_PDC_MCI   ((AT91PS_PDC) 	0xFFFB4100) // (PDC_MCI) Base Address
#define	AT91C_BASE_MCI       ((AT91PS_MCI) 	0xFFFB4000) // (MCI) Base Address
#define	AT91C_BASE_UDP       ((AT91PS_UDP) 	0xFFFB0000) // (UDP) Base Address
#define	AT91C_BASE_TC5       ((AT91PS_TC) 	0xFFFA4080) // (TC5) Base Address
#define	AT91C_BASE_TC4       ((AT91PS_TC) 	0xFFFA4040) // (TC4) Base Address
#define	AT91C_BASE_TC3       ((AT91PS_TC) 	0xFFFA4000) // (TC3) Base Address
#define	AT91C_BASE_TCB1      ((AT91PS_TCB) 	0xFFFA4080) // (TCB1) Base Address
#define	AT91C_BASE_TC2       ((AT91PS_TC) 	0xFFFA0080) // (TC2) Base Address
#define	AT91C_BASE_TC1       ((AT91PS_TC) 	0xFFFA0040) // (TC1) Base Address
#define	AT91C_BASE_TC0       ((AT91PS_TC) 	0xFFFA0000) // (TC0) Base Address
#define	AT91C_BASE_TCB0      ((AT91PS_TCB) 	0xFFFA0000) // (TCB0) Base Address
#define	AT91C_BASE_UHP       ((AT91PS_UHP) 	0x00300000) // (UHP) Base Address
#define	AT91C_BASE_EMAC      ((AT91PS_EMAC) 	0xFFFBC000) // (EMAC) Base Address
#define	AT91C_BASE_EBI       ((AT91PS_EBI) 	0xFFFFFF60) // (EBI) Base Address
#define	AT91C_BASE_SMC2      ((AT91PS_SMC2) 	0xFFFFFF70) // (SMC2) Base Address
#define	AT91C_BASE_SDRC      ((AT91PS_SDRC) 	0xFFFFFF90) // (SDRC) Base Address
#define	AT91C_BASE_BFC       ((AT91PS_BFC) 	0xFFFFFFC0) // (BFC) Base Address

// *****************************************************************************
//               MEMORY MAPPING DEFINITIONS FOR AT91RM9200
// *****************************************************************************
#define	AT91C_ISRAM	 ((char *) 0x00200000) // Internal SRAM base address
#define	AT91C_ISRAM_SIZE	 0x00004000u // Internal SRAM size in byte (16 Kbyte)
#define	AT91C_IROM 	 ((char *) 0x00100000) // Internal ROM base address
#define	AT91C_IROM_SIZE	 0x00020000u // Internal ROM size in byte (128 Kbyte)

#endif
