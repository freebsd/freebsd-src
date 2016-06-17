// ----------------------------------------------------------------------------
//          ATMEL Microcontroller Software Support  -  ROUSSET  -
// ----------------------------------------------------------------------------
//  The software is delivered "AS IS" without warranty or condition of any
//  kind, either express, implied or statutory. This includes without
//  limitation any warranty or condition with respect to merchantability or
//  fitness for any particular purpose, or against the infringements of
//  intellectual property rights of others.
// ----------------------------------------------------------------------------
// File Name           : AT91RM9200.h
// Object              : AT91RM9200 definitions
// Generated           : AT91 SW Application Group  04/16/2003 (12:30:06)
//
// ----------------------------------------------------------------------------

#ifndef AT91RM9200_SYS_H
#define AT91RM9200_SYS_H

#ifndef __ASSEMBLY__

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

#else

/* Offsets from AT91C_BASE_SYS */
#define AIC_SMR		(0) // Source Mode Register
#define AIC_SVR		(128) // Source Vector Register
#define AIC_IVR		(256) // IRQ Vector Register
#define AIC_FVR		(260) // FIQ Vector Register
#define AIC_ISR		(264) // Interrupt Status Register
#define AIC_IPR		(268) // Interrupt Pending Register
#define AIC_IMR		(272) // Interrupt Mask Register
#define AIC_CISR	(276) // Core Interrupt Status Register
#define AIC_IECR	(288) // Interrupt Enable Command Register
#define AIC_IDCR	(292) // Interrupt Disable Command Register
#define AIC_ICCR	(296) // Interrupt Clear Command Register
#define AIC_ISCR	(300) // Interrupt Set Command Register
#define AIC_EOICR	(304) // End of Interrupt Command Register
#define AIC_SPU		(308) // Spurious Vector Register
#define AIC_DCR		(312) // Debug Control Register (Protect)
#define AIC_FFER	(320) // Fast Forcing Enable Register
#define AIC_FFDR	(324) // Fast Forcing Disable Register
#define AIC_FFSR	(328) // Fast Forcing Status Register

/* Offsets from AT91C_BASE_SYS */
#define DBGU_CR		(0x200 + 0) // Control Register
#define DBGU_MR		(0x200 + 4) // Mode Register
#define DBGU_IER	(0x200 + 8) // Interrupt Enable Register
#define DBGU_IDR	(0x200 + 12) // Interrupt Disable Register
#define DBGU_IMR	(0x200 + 16) // Interrupt Mask Register
#define DBGU_CSR	(0x200 + 20) // Channel Status Register
#define DBGU_RHR	(0x200 + 24) // Receiver Holding Register
#define DBGU_THR	(0x200 + 28) // Transmitter Holding Register
#define DBGU_BRGR	(0x200 + 32) // Baud Rate Generator Register
#define DBGU_C1R	(0x200 + 64) // Chip ID1 Register
#define DBGU_C2R	(0x200 + 68) // Chip ID2 Register
#define DBGU_FNTR	(0x200 + 72) // Force NTRST Register
#define DBGU_RPR	(0x200 + 256) // Receive Pointer Register
#define DBGU_RCR	(0x200 + 260) // Receive Counter Register
#define DBGU_TPR	(0x200 + 264) // Transmit Pointer Register
#define DBGU_TCR	(0x200 + 268) // Transmit Counter Register
#define DBGU_RNPR	(0x200 + 272) // Receive Next Pointer Register
#define DBGU_RNCR	(0x200 + 276) // Receive Next Counter Register
#define DBGU_TNPR	(0x200 + 280) // Transmit Next Pointer Register
#define DBGU_TNCR	(0x200 + 284) // Transmit Next Counter Register
#define DBGU_PTCR	(0x200 + 288) // PDC Transfer Control Register
#define DBGU_PTSR	(0x200 + 292) // PDC Transfer Status Register

#endif // __ASSEMBLY


// *****************************************************************************
//              SOFTWARE API DEFINITION  FOR Memory Controller Interface
// *****************************************************************************
// -------- MC_RCR : (MC Offset: 0x0) MC Remap Control Register --------
#define AT91C_MC_RCB          (0x1 <<  0) // (MC) Remap Command Bit
// -------- MC_ASR : (MC Offset: 0x4) MC Abort Status Register --------
#define AT91C_MC_UNDADD       (0x1 <<  0) // (MC) Undefined Addess Abort Status
#define AT91C_MC_MISADD       (0x1 <<  1) // (MC) Misaligned Addess Abort Status
#define AT91C_MC_MPU          (0x1 <<  2) // (MC) Memory protection Unit Abort Status
#define AT91C_MC_ABTSZ        (0x3 <<  8) // (MC) Abort Size Status
#define 	AT91C_MC_ABTSZ_BYTE                 (0x0 <<  8) // (MC) Byte
#define 	AT91C_MC_ABTSZ_HWORD                (0x1 <<  8) // (MC) Half-word
#define 	AT91C_MC_ABTSZ_WORD                 (0x2 <<  8) // (MC) Word
#define AT91C_MC_ABTTYP       (0x3 << 10) // (MC) Abort Type Status
#define 	AT91C_MC_ABTTYP_DATAR                (0x0 << 10) // (MC) Data Read
#define 	AT91C_MC_ABTTYP_DATAW                (0x1 << 10) // (MC) Data Write
#define 	AT91C_MC_ABTTYP_FETCH                (0x2 << 10) // (MC) Code Fetch
#define AT91C_MC_MST0         (0x1 << 16) // (MC) Master 0 Abort Source
#define AT91C_MC_MST1         (0x1 << 17) // (MC) Master 1 Abort Source
#define AT91C_MC_SVMST0       (0x1 << 24) // (MC) Saved Master 0 Abort Source
#define AT91C_MC_SVMST1       (0x1 << 25) // (MC) Saved Master 1 Abort Source
// -------- MC_PUIA : (MC Offset: 0x10) MC Protection Unit Area --------
#define AT91C_MC_PROT         (0x3 <<  0) // (MC) Protection
#define 	AT91C_MC_PROT_PNAUNA               (0x0) // (MC) Privilege: No Access, User: No Access
#define 	AT91C_MC_PROT_PRWUNA               (0x1) // (MC) Privilege: Read/Write, User: No Access
#define 	AT91C_MC_PROT_PRWURO               (0x2) // (MC) Privilege: Read/Write, User: Read Only
#define 	AT91C_MC_PROT_PRWURW               (0x3) // (MC) Privilege: Read/Write, User: Read/Write
#define AT91C_MC_SIZE         (0xF <<  4) // (MC) Internal Area Size
#define 	AT91C_MC_SIZE_1KB                  (0x0 <<  4) // (MC) Area size 1KByte
#define 	AT91C_MC_SIZE_2KB                  (0x1 <<  4) // (MC) Area size 2KByte
#define 	AT91C_MC_SIZE_4KB                  (0x2 <<  4) // (MC) Area size 4KByte
#define 	AT91C_MC_SIZE_8KB                  (0x3 <<  4) // (MC) Area size 8KByte
#define 	AT91C_MC_SIZE_16KB                 (0x4 <<  4) // (MC) Area size 16KByte
#define 	AT91C_MC_SIZE_32KB                 (0x5 <<  4) // (MC) Area size 32KByte
#define 	AT91C_MC_SIZE_64KB                 (0x6 <<  4) // (MC) Area size 64KByte
#define 	AT91C_MC_SIZE_128KB                (0x7 <<  4) // (MC) Area size 128KByte
#define 	AT91C_MC_SIZE_256KB                (0x8 <<  4) // (MC) Area size 256KByte
#define 	AT91C_MC_SIZE_512KB                (0x9 <<  4) // (MC) Area size 512KByte
#define 	AT91C_MC_SIZE_1MB                  (0xA <<  4) // (MC) Area size 1MByte
#define 	AT91C_MC_SIZE_2MB                  (0xB <<  4) // (MC) Area size 2MByte
#define 	AT91C_MC_SIZE_4MB                  (0xC <<  4) // (MC) Area size 4MByte
#define 	AT91C_MC_SIZE_8MB                  (0xD <<  4) // (MC) Area size 8MByte
#define 	AT91C_MC_SIZE_16MB                 (0xE <<  4) // (MC) Area size 16MByte
#define 	AT91C_MC_SIZE_64MB                 (0xF <<  4) // (MC) Area size 64MByte
#define AT91C_MC_BA           (0x3FFFF << 10) // (MC) Internal Area Base Address
// -------- MC_PUP : (MC Offset: 0x50) MC Protection Unit Peripheral --------
// -------- MC_PUER : (MC Offset: 0x54) MC Protection Unit Area --------
#define AT91C_MC_PUEB         (0x1 <<  0) // (MC) Protection Unit enable Bit


// *****************************************************************************
//              SOFTWARE API DEFINITION  FOR Real-time Clock Alarm and Parallel Load Interface
// *****************************************************************************
// -------- RTC_CR : (RTC Offset: 0x0) RTC Control Register --------
#define AT91C_RTC_UPDTIM      (0x1 <<  0) // (RTC) Update Request Time Register
#define AT91C_RTC_UPDCAL      (0x1 <<  1) // (RTC) Update Request Calendar Register
#define AT91C_RTC_TIMEVSEL    (0x3 <<  8) // (RTC) Time Event Selection
#define 	AT91C_RTC_TIMEVSEL_MINUTE               (0x0 <<  8) // (RTC) Minute change.
#define 	AT91C_RTC_TIMEVSEL_HOUR                 (0x1 <<  8) // (RTC) Hour change.
#define 	AT91C_RTC_TIMEVSEL_DAY24                (0x2 <<  8) // (RTC) Every day at midnight.
#define 	AT91C_RTC_TIMEVSEL_DAY12                (0x3 <<  8) // (RTC) Every day at noon.
#define AT91C_RTC_CALEVSEL    (0x3 << 16) // (RTC) Calendar Event Selection
#define 	AT91C_RTC_CALEVSEL_WEEK                 (0x0 << 16) // (RTC) Week change (every Monday at time 00:00:00).
#define 	AT91C_RTC_CALEVSEL_MONTH                (0x1 << 16) // (RTC) Month change (every 01 of each month at time 00:00:00).
#define 	AT91C_RTC_CALEVSEL_YEAR                 (0x2 << 16) // (RTC) Year change (every January 1 at time 00:00:00).
// -------- RTC_MR : (RTC Offset: 0x4) RTC Mode Register --------
#define AT91C_RTC_HRMOD       (0x1 <<  0) // (RTC) 12-24 hour Mode
// -------- RTC_TIMR : (RTC Offset: 0x8) RTC Time Register --------
#define AT91C_RTC_SEC         (0x7F <<  0) // (RTC) Current Second
#define AT91C_RTC_MIN         (0x7F <<  8) // (RTC) Current Minute
#define AT91C_RTC_HOUR        (0x3F << 16) // (RTC) Current Hour
#define AT91C_RTC_AMPM        (0x1 << 22) // (RTC) Ante Meridiem, Post Meridiem Indicator
// -------- RTC_CALR : (RTC Offset: 0xc) RTC Calendar Register --------
#define AT91C_RTC_CENT        (0x3F <<  0) // (RTC) Current Century
#define AT91C_RTC_YEAR        (0xFF <<  8) // (RTC) Current Year
#define AT91C_RTC_MONTH       (0x1F << 16) // (RTC) Current Month
#define AT91C_RTC_DAY         (0x7 << 21) // (RTC) Current Day
#define AT91C_RTC_DATE        (0x3F << 24) // (RTC) Current Date
// -------- RTC_TIMALR : (RTC Offset: 0x10) RTC Time Alarm Register --------
#define AT91C_RTC_SECEN       (0x1 <<  7) // (RTC) Second Alarm Enable
#define AT91C_RTC_MINEN       (0x1 << 15) // (RTC) Minute Alarm
#define AT91C_RTC_HOUREN      (0x1 << 23) // (RTC) Current Hour
// -------- RTC_CALALR : (RTC Offset: 0x14) RTC Calendar Alarm Register --------
#define AT91C_RTC_MONTHEN     (0x1 << 23) // (RTC) Month Alarm Enable
#define AT91C_RTC_DATEEN      (0x1 << 31) // (RTC) Date Alarm Enable
// -------- RTC_SR : (RTC Offset: 0x18) RTC Status Register --------
#define AT91C_RTC_ACKUPD      (0x1 <<  0) // (RTC) Acknowledge for Update
#define AT91C_RTC_ALARM       (0x1 <<  1) // (RTC) Alarm Flag
#define AT91C_RTC_SECEV       (0x1 <<  2) // (RTC) Second Event
#define AT91C_RTC_TIMEV       (0x1 <<  3) // (RTC) Time Event
#define AT91C_RTC_CALEV       (0x1 <<  4) // (RTC) Calendar event
// -------- RTC_SCCR : (RTC Offset: 0x1c) RTC Status Clear Command Register --------
// -------- RTC_IER : (RTC Offset: 0x20) RTC Interrupt Enable Register --------
// -------- RTC_IDR : (RTC Offset: 0x24) RTC Interrupt Disable Register --------
// -------- RTC_IMR : (RTC Offset: 0x28) RTC Interrupt Mask Register --------
// -------- RTC_VER : (RTC Offset: 0x2c) RTC Valid Entry Register --------
#define AT91C_RTC_NVTIM       (0x1 <<  0) // (RTC) Non valid Time
#define AT91C_RTC_NVCAL       (0x1 <<  1) // (RTC) Non valid Calendar
#define AT91C_RTC_NVTIMALR    (0x1 <<  2) // (RTC) Non valid time Alarm
#define AT91C_RTC_NVCALALR    (0x1 <<  3) // (RTC) Nonvalid Calendar Alarm


// *****************************************************************************
//              SOFTWARE API DEFINITION  FOR System Timer Interface
// *****************************************************************************
// -------- ST_CR : (ST Offset: 0x0) System Timer Control Register --------
#define AT91C_ST_WDRST        (0x1 <<  0) // (ST) Watchdog Timer Restart
// -------- ST_PIMR : (ST Offset: 0x4) System Timer Period Interval Mode Register --------
#define AT91C_ST_PIV          (0xFFFF <<  0) // (ST) Watchdog Timer Restart
// -------- ST_WDMR : (ST Offset: 0x8) System Timer Watchdog Mode Register --------
#define AT91C_ST_WDV          (0xFFFF <<  0) // (ST) Watchdog Timer Restart
#define AT91C_ST_RSTEN        (0x1 << 16) // (ST) Reset Enable
#define AT91C_ST_EXTEN        (0x1 << 17) // (ST) External Signal Assertion Enable
// -------- ST_RTMR : (ST Offset: 0xc) System Timer Real-time Mode Register --------
#define AT91C_ST_RTPRES       (0xFFFF <<  0) // (ST) Real-time Timer Prescaler Value
// -------- ST_SR : (ST Offset: 0x10) System Timer Status Register --------
#define AT91C_ST_PITS         (0x1 <<  0) // (ST) Period Interval Timer Interrupt
#define AT91C_ST_WDOVF        (0x1 <<  1) // (ST) Watchdog Overflow
#define AT91C_ST_RTTINC       (0x1 <<  2) // (ST) Real-time Timer Increment
#define AT91C_ST_ALMS         (0x1 <<  3) // (ST) Alarm Status
// -------- ST_IER : (ST Offset: 0x14) System Timer Interrupt Enable Register --------
// -------- ST_IDR : (ST Offset: 0x18) System Timer Interrupt Disable Register --------
// -------- ST_IMR : (ST Offset: 0x1c) System Timer Interrupt Mask Register --------
// -------- ST_RTAR : (ST Offset: 0x20) System Timer Real-time Alarm Register --------
#define AT91C_ST_ALMV         (0xFFFFF <<  0) // (ST) Alarm Value Value
// -------- ST_CRTR : (ST Offset: 0x24) System Timer Current Real-time Register --------
#define AT91C_ST_CRTV         (0xFFFFF <<  0) // (ST) Current Real-time Value


// *****************************************************************************
//              SOFTWARE API DEFINITION  FOR Power Management Controler
// *****************************************************************************
// -------- PMC_SCER : (PMC Offset: 0x0) System Clock Enable Register --------
#define AT91C_PMC_PCK         (0x1 <<  0) // (PMC) Processor Clock
#define AT91C_PMC_UDP         (0x1 <<  1) // (PMC) USB Device Port Clock
#define AT91C_PMC_MCKUDP      (0x1 <<  2) // (PMC) USB Device Port Master Clock Automatic Disable on Suspend
#define AT91C_PMC_UHP         (0x1 <<  4) // (PMC) USB Host Port Clock
#define AT91C_PMC_PCK0        (0x1 <<  8) // (PMC) Programmable Clock Output
#define AT91C_PMC_PCK1        (0x1 <<  9) // (PMC) Programmable Clock Output
#define AT91C_PMC_PCK2        (0x1 << 10) // (PMC) Programmable Clock Output
#define AT91C_PMC_PCK3        (0x1 << 11) // (PMC) Programmable Clock Output
#define AT91C_PMC_PCK4        (0x1 << 12) // (PMC) Programmable Clock Output
#define AT91C_PMC_PCK5        (0x1 << 13) // (PMC) Programmable Clock Output
#define AT91C_PMC_PCK6        (0x1 << 14) // (PMC) Programmable Clock Output
#define AT91C_PMC_PCK7        (0x1 << 15) // (PMC) Programmable Clock Output
// -------- PMC_SCDR : (PMC Offset: 0x4) System Clock Disable Register --------
// -------- PMC_SCSR : (PMC Offset: 0x8) System Clock Status Register --------
// -------- PMC_MCKR : (PMC Offset: 0x30) Master Clock Register --------
#define AT91C_PMC_CSS         (0x3 <<  0) // (PMC) Programmable Clock Selection
#define 	AT91C_PMC_CSS_SLOW_CLK             (0x0) // (PMC) Slow Clock is selected
#define 	AT91C_PMC_CSS_MAIN_CLK             (0x1) // (PMC) Main Clock is selected
#define 	AT91C_PMC_CSS_PLLA_CLK             (0x2) // (PMC) Clock from PLL A is selected
#define 	AT91C_PMC_CSS_PLLB_CLK             (0x3) // (PMC) Clock from PLL B is selected
#define AT91C_PMC_PRES        (0x7 <<  2) // (PMC) Programmable Clock Prescaler
#define 	AT91C_PMC_PRES_CLK                  (0x0 <<  2) // (PMC) Selected clock
#define 	AT91C_PMC_PRES_CLK_2                (0x1 <<  2) // (PMC) Selected clock divided by 2
#define 	AT91C_PMC_PRES_CLK_4                (0x2 <<  2) // (PMC) Selected clock divided by 4
#define 	AT91C_PMC_PRES_CLK_8                (0x3 <<  2) // (PMC) Selected clock divided by 8
#define 	AT91C_PMC_PRES_CLK_16               (0x4 <<  2) // (PMC) Selected clock divided by 16
#define 	AT91C_PMC_PRES_CLK_32               (0x5 <<  2) // (PMC) Selected clock divided by 32
#define 	AT91C_PMC_PRES_CLK_64               (0x6 <<  2) // (PMC) Selected clock divided by 64
#define AT91C_PMC_MDIV        (0x3 <<  8) // (PMC) Master Clock Division
#define 	AT91C_PMC_MDIV_1                    (0x0 <<  8) // (PMC) The master clock and the processor clock are the same
#define 	AT91C_PMC_MDIV_2                    (0x1 <<  8) // (PMC) The processor clock is twice as fast as the master clock
#define 	AT91C_PMC_MDIV_3                    (0x2 <<  8) // (PMC) The processor clock is three times faster than the master clock
#define 	AT91C_PMC_MDIV_4                    (0x3 <<  8) // (PMC) The processor clock is four times faster than the master clock
// -------- PMC_PCKR : (PMC Offset: 0x40) Programmable Clock Register --------
// -------- PMC_IER : (PMC Offset: 0x60) PMC Interrupt Enable Register --------
#define AT91C_PMC_MOSCS       (0x1 <<  0) // (PMC) MOSC Status/Enable/Disable/Mask
#define AT91C_PMC_LOCKA       (0x1 <<  1) // (PMC) PLL A Status/Enable/Disable/Mask
#define AT91C_PMC_LOCKB       (0x1 <<  2) // (PMC) PLL B Status/Enable/Disable/Mask
#define AT91C_PMC_MCKRDY      (0x1 <<  3) // (PMC) MCK_RDY Status/Enable/Disable/Mask
#define AT91C_PMC_PCK0RDY     (0x1 <<  8) // (PMC) PCK0_RDY Status/Enable/Disable/Mask
#define AT91C_PMC_PCK1RDY     (0x1 <<  9) // (PMC) PCK1_RDY Status/Enable/Disable/Mask
#define AT91C_PMC_PCK2RDY     (0x1 << 10) // (PMC) PCK2_RDY Status/Enable/Disable/Mask
#define AT91C_PMC_PCK3RDY     (0x1 << 11) // (PMC) PCK3_RDY Status/Enable/Disable/Mask
#define AT91C_PMC_PCK4RDY     (0x1 << 12) // (PMC) PCK4_RDY Status/Enable/Disable/Mask
#define AT91C_PMC_PCK5RDY     (0x1 << 13) // (PMC) PCK5_RDY Status/Enable/Disable/Mask
#define AT91C_PMC_PCK6RDY     (0x1 << 14) // (PMC) PCK6_RDY Status/Enable/Disable/Mask
#define AT91C_PMC_PCK7RDY     (0x1 << 15) // (PMC) PCK7_RDY Status/Enable/Disable/Mask
// -------- PMC_IDR : (PMC Offset: 0x64) PMC Interrupt Disable Register --------
// -------- PMC_SR : (PMC Offset: 0x68) PMC Status Register --------
// -------- PMC_IMR : (PMC Offset: 0x6c) PMC Interrupt Mask Register --------


// *****************************************************************************
//              SOFTWARE API DEFINITION  FOR Clock Generator Controler
// *****************************************************************************
// -------- CKGR_MOR : (CKGR Offset: 0x0) Main Oscillator Register --------
#define AT91C_CKGR_MOSCEN     (0x1 <<  0) // (CKGR) Main Oscillator Enable
#define AT91C_CKGR_OSCTEST    (0x1 <<  1) // (CKGR) Oscillator Test
#define AT91C_CKGR_OSCOUNT    (0xFF <<  8) // (CKGR) Main Oscillator Start-up Time
// -------- CKGR_MCFR : (CKGR Offset: 0x4) Main Clock Frequency Register --------
#define AT91C_CKGR_MAINF      (0xFFFF <<  0) // (CKGR) Main Clock Frequency
#define AT91C_CKGR_MAINRDY    (0x1 << 16) // (CKGR) Main Clock Ready
// -------- CKGR_PLLAR : (CKGR Offset: 0x8) PLL A Register --------
#define AT91C_CKGR_DIVA       (0xFF <<  0) // (CKGR) Divider Selected
#define 	AT91C_CKGR_DIVA_0                    (0x0) // (CKGR) Divider output is 0
#define 	AT91C_CKGR_DIVA_BYPASS               (0x1) // (CKGR) Divider is bypassed
#define AT91C_CKGR_PLLACOUNT  (0x3F <<  8) // (CKGR) PLL A Counter
#define AT91C_CKGR_OUTA       (0x3 << 14) // (CKGR) PLL A Output Frequency Range
#define 	AT91C_CKGR_OUTA_0                    (0x0 << 14) // (CKGR) Please refer to the PLLA datasheet
#define 	AT91C_CKGR_OUTA_1                    (0x1 << 14) // (CKGR) Please refer to the PLLA datasheet
#define 	AT91C_CKGR_OUTA_2                    (0x2 << 14) // (CKGR) Please refer to the PLLA datasheet
#define 	AT91C_CKGR_OUTA_3                    (0x3 << 14) // (CKGR) Please refer to the PLLA datasheet
#define AT91C_CKGR_MULA       (0x7FF << 16) // (CKGR) PLL A Multiplier
#define AT91C_CKGR_SRCA       (0x1 << 29) // (CKGR) PLL A Source
// -------- CKGR_PLLBR : (CKGR Offset: 0xc) PLL B Register --------
#define AT91C_CKGR_DIVB       (0xFF <<  0) // (CKGR) Divider Selected
#define 	AT91C_CKGR_DIVB_0                    (0x0) // (CKGR) Divider output is 0
#define 	AT91C_CKGR_DIVB_BYPASS               (0x1) // (CKGR) Divider is bypassed
#define AT91C_CKGR_PLLBCOUNT  (0x3F <<  8) // (CKGR) PLL B Counter
#define AT91C_CKGR_OUTB       (0x3 << 14) // (CKGR) PLL B Output Frequency Range
#define 	AT91C_CKGR_OUTB_0                    (0x0 << 14) // (CKGR) Please refer to the PLLB datasheet
#define 	AT91C_CKGR_OUTB_1                    (0x1 << 14) // (CKGR) Please refer to the PLLB datasheet
#define 	AT91C_CKGR_OUTB_2                    (0x2 << 14) // (CKGR) Please refer to the PLLB datasheet
#define 	AT91C_CKGR_OUTB_3                    (0x3 << 14) // (CKGR) Please refer to the PLLB datasheet
#define AT91C_CKGR_MULB       (0x7FF << 16) // (CKGR) PLL B Multiplier
#define AT91C_CKGR_USB_96M    (0x1 << 28) // (CKGR) Divider for USB Ports
#define AT91C_CKGR_USB_PLL    (0x1 << 29) // (CKGR) PLL Use


// *****************************************************************************
//              SOFTWARE API DEFINITION  FOR Debug Unit
// *****************************************************************************
// -------- DBGU_CR : (DBGU Offset: 0x0) Debug Unit Control Register --------
// -------- DBGU_MR : (DBGU Offset: 0x4) Debug Unit Mode Register --------
// -------- DBGU_IER : (DBGU Offset: 0x8) Debug Unit Interrupt Enable Register --------
#define AT91C_DBGU_TXRDY	(0x1 <<  1) // (DBGU) TXRDY Interrupt
#define AT91C_DBGU_TXEMPTY	(0x1 <<  9) // (DBGU) TXEMPTY Interrupt
// -------- DBGU_IDR : (DBGU Offset: 0xc) Debug Unit Interrupt Disable Register --------
// -------- DBGU_IMR : (DBGU Offset: 0x10) Debug Unit Interrupt Mask Register --------
// -------- DBGU_CSR : (DBGU Offset: 0x14) Debug Unit Channel Status Register --------
// -------- DBGU_FNTR : (DBGU Offset: 0x48) Debug Unit FORCE_NTRST Register --------
#define AT91C_DBGU_FORCE_NTRST  (0x1 <<  0) // (DBGU) Force NTRST in JTAG


// *****************************************************************************
//              SOFTWARE API DEFINITION  FOR Peripheral Data Controller
// *****************************************************************************
// -------- PDC_PTCR : (PDC Offset: 0x20) PDC Transfer Control Register --------
#define AT91C_PDC_RXTEN       (0x1 <<  0) // (PDC) Receiver Transfer Enable
#define AT91C_PDC_RXTDIS      (0x1 <<  1) // (PDC) Receiver Transfer Disable
#define AT91C_PDC_TXTEN       (0x1 <<  8) // (PDC) Transmitter Transfer Enable
#define AT91C_PDC_TXTDIS      (0x1 <<  9) // (PDC) Transmitter Transfer Disable
// -------- PDC_PTSR : (PDC Offset: 0x24) PDC Transfer Status Register --------


// *****************************************************************************
//              SOFTWARE API DEFINITION  FOR Advanced Interrupt Controller
// *****************************************************************************
// -------- AIC_SMR : (AIC Offset: 0x0) Control Register --------
#define AT91C_AIC_PRIOR       (0x7 <<  0) // (AIC) Priority Level
#define 	AT91C_AIC_PRIOR_LOWEST               (0x0) // (AIC) Lowest priority level
#define 	AT91C_AIC_PRIOR_HIGHEST              (0x7) // (AIC) Highest priority level
#define AT91C_AIC_SRCTYPE     (0x3 <<  5) // (AIC) Interrupt Source Type
#define 	AT91C_AIC_SRCTYPE_INT_LEVEL_SENSITIVE  (0x0 <<  5) // (AIC) Internal Sources Code Label Level Sensitive
#define 	AT91C_AIC_SRCTYPE_INT_EDGE_TRIGGERED   (0x1 <<  5) // (AIC) Internal Sources Code Label Edge triggered
#define 	AT91C_AIC_SRCTYPE_EXT_HIGH_LEVEL       (0x2 <<  5) // (AIC) External Sources Code Label High-level Sensitive
#define 	AT91C_AIC_SRCTYPE_EXT_POSITIVE_EDGE    (0x3 <<  5) // (AIC) External Sources Code Label Positive Edge triggered
// -------- AIC_CISR : (AIC Offset: 0x114) AIC Core Interrupt Status Register --------
#define AT91C_AIC_NFIQ        (0x1 <<  0) // (AIC) NFIQ Status
#define AT91C_AIC_NIRQ        (0x1 <<  1) // (AIC) NIRQ Status
// -------- AIC_DCR : (AIC Offset: 0x138) AIC Debug Control Register (Protect) --------
#define AT91C_AIC_DCR_PROT    (0x1 <<  0) // (AIC) Protection Mode
#define AT91C_AIC_DCR_GMSK    (0x1 <<  1) // (AIC) General Mask


// *****************************************************************************
//              SOFTWARE API DEFINITION  FOR External Bus Interface
// *****************************************************************************
// -------- EBI_CSA : (EBI Offset: 0x0) Chip Select Assignment Register --------
#define AT91C_EBI_CS0A        (0x1 <<  0) // (EBI) Chip Select 0 Assignment
#define 	AT91C_EBI_CS0A_SMC                  (0x0) // (EBI) Chip Select 0 is assigned to the Static Memory Controller.
#define 	AT91C_EBI_CS0A_BFC                  (0x1) // (EBI) Chip Select 0 is assigned to the Burst Flash Controller.
#define AT91C_EBI_CS1A        (0x1 <<  1) // (EBI) Chip Select 1 Assignment
#define 	AT91C_EBI_CS1A_SMC                  (0x0 <<  1) // (EBI) Chip Select 1 is assigned to the Static Memory Controller.
#define 	AT91C_EBI_CS1A_SDRAMC               (0x1 <<  1) // (EBI) Chip Select 1 is assigned to the SDRAM Controller.
#define AT91C_EBI_CS3A        (0x1 <<  3) // (EBI) Chip Select 3 Assignment
#define 	AT91C_EBI_CS3A_SMC                  (0x0 <<  3) // (EBI) Chip Select 3 is only assigned to the Static Memory Controller and NCS3 behaves as defined by the SMC2.
#define 	AT91C_EBI_CS3A_SMC_SmartMedia       (0x1 <<  3) // (EBI) Chip Select 3 is assigned to the Static Memory Controller and the SmartMedia Logic is activated.
#define AT91C_EBI_CS4A        (0x1 <<  4) // (EBI) Chip Select 4 Assignment
#define 	AT91C_EBI_CS4A_SMC                  (0x0 <<  4) // (EBI) Chip Select 4 is assigned to the Static Memory Controller and NCS4,NCS5 and NCS6 behave as defined by the SMC2.
#define 	AT91C_EBI_CS4A_SMC_CompactFlash     (0x1 <<  4) // (EBI) Chip Select 4 is assigned to the Static Memory Controller and the CompactFlash Logic is activated.
// -------- EBI_CFGR : (EBI Offset: 0x4) Configuration Register --------
#define AT91C_EBI_DBPUC       (0x1 <<  0) // (EBI) Data Bus Pull-Up Configuration
#define AT91C_EBI_EBSEN       (0x1 <<  1) // (EBI) Bus Sharing Enable


// *****************************************************************************
//              SOFTWARE API DEFINITION  FOR Static Memory Controller 2 Interface
// *****************************************************************************

// -------- SMC2_CSR : (SMC2 Offset: 0x0) SMC2 Chip Select Register --------
#define AT91C_SMC2_NWS        (0x7F <<  0) // (SMC2) Number of Wait States
#define AT91C_SMC2_WSEN       (0x1 <<  7) // (SMC2) Wait State Enable
#define AT91C_SMC2_TDF        (0xF <<  8) // (SMC2) Data Float Time
#define AT91C_SMC2_BAT        (0x1 << 12) // (SMC2) Byte Access Type
#define AT91C_SMC2_DBW        (0x1 << 13) // (SMC2) Data Bus Width
#define 	AT91C_SMC2_DBW_16                   (0x1 << 13) // (SMC2) 16-bit.
#define 	AT91C_SMC2_DBW_8                    (0x2 << 13) // (SMC2) 8-bit.
#define AT91C_SMC2_DRP        (0x1 << 15) // (SMC2) Data Read Protocol
#define AT91C_SMC2_ACSS       (0x3 << 16) // (SMC2) Address to Chip Select Setup
#define 	AT91C_SMC2_ACSS_STANDARD             (0x0 << 16) // (SMC2) Standard, asserted at the beginning of the access and deasserted at the end.
#define 	AT91C_SMC2_ACSS_1_CYCLE              (0x1 << 16) // (SMC2) One cycle less at the beginning and the end of the access.
#define 	AT91C_SMC2_ACSS_2_CYCLES             (0x2 << 16) // (SMC2) Two cycles less at the beginning and the end of the access.
#define 	AT91C_SMC2_ACSS_3_CYCLES             (0x3 << 16) // (SMC2) Three cycles less at the beginning and the end of the access.
#define AT91C_SMC2_RWSETUP    (0x7 << 24) // (SMC2) Read and Write Signal Setup Time
#define AT91C_SMC2_RWHOLD     (0x7 << 29) // (SMC2) Read and Write Signal Hold Time


// *****************************************************************************
//              SOFTWARE API DEFINITION  FOR SDRAM Controller Interface
// *****************************************************************************
// -------- SDRC_MR : (SDRC Offset: 0x0) SDRAM Controller Mode Register --------
#define AT91C_SDRC_MODE       (0xF <<  0) // (SDRC) Mode
#define 	AT91C_SDRC_MODE_NORMAL_CMD           (0x0) // (SDRC) Normal Mode
#define 	AT91C_SDRC_MODE_NOP_CMD              (0x1) // (SDRC) NOP Command
#define 	AT91C_SDRC_MODE_PRCGALL_CMD          (0x2) // (SDRC) All Banks Precharge Command
#define 	AT91C_SDRC_MODE_LMR_CMD              (0x3) // (SDRC) Load Mode Register Command
#define 	AT91C_SDRC_MODE_RFSH_CMD             (0x4) // (SDRC) Refresh Command
#define AT91C_SDRC_DBW        (0x1 <<  4) // (SDRC) Data Bus Width
#define 	AT91C_SDRC_DBW_32_BITS              (0x0 <<  4) // (SDRC) 32 Bits datas bus
#define 	AT91C_SDRC_DBW_16_BITS              (0x1 <<  4) // (SDRC) 16 Bits datas bus
// -------- SDRC_TR : (SDRC Offset: 0x4) SDRC Refresh Timer Register --------
#define AT91C_SDRC_COUNT      (0xFFF <<  0) // (SDRC) Refresh Counter
// -------- SDRC_CR : (SDRC Offset: 0x8) SDRAM Configuration Register --------
#define AT91C_SDRC_NC         (0x3 <<  0) // (SDRC) Number of Column Bits
#define 	AT91C_SDRC_NC_8                    (0x0) // (SDRC) 8 Bits
#define 	AT91C_SDRC_NC_9                    (0x1) // (SDRC) 9 Bits
#define 	AT91C_SDRC_NC_10                   (0x2) // (SDRC) 10 Bits
#define 	AT91C_SDRC_NC_11                   (0x3) // (SDRC) 11 Bits
#define AT91C_SDRC_NR         (0x3 <<  2) // (SDRC) Number of Row Bits
#define 	AT91C_SDRC_NR_11                   (0x0 <<  2) // (SDRC) 11 Bits
#define 	AT91C_SDRC_NR_12                   (0x1 <<  2) // (SDRC) 12 Bits
#define 	AT91C_SDRC_NR_13                   (0x2 <<  2) // (SDRC) 13 Bits
#define AT91C_SDRC_NB         (0x1 <<  4) // (SDRC) Number of Banks
#define 	AT91C_SDRC_NB_2_BANKS              (0x0 <<  4) // (SDRC) 2 banks
#define 	AT91C_SDRC_NB_4_BANKS              (0x1 <<  4) // (SDRC) 4 banks
#define AT91C_SDRC_CAS        (0x3 <<  5) // (SDRC) CAS Latency
#define 	AT91C_SDRC_CAS_2                    (0x2 <<  5) // (SDRC) 2 cycles
#define AT91C_SDRC_TWR        (0xF <<  7) // (SDRC) Number of Write Recovery Time Cycles
#define AT91C_SDRC_TRC        (0xF << 11) // (SDRC) Number of RAS Cycle Time Cycles
#define AT91C_SDRC_TRP        (0xF << 15) // (SDRC) Number of RAS Precharge Time Cycles
#define AT91C_SDRC_TRCD       (0xF << 19) // (SDRC) Number of RAS to CAS Delay Cycles
#define AT91C_SDRC_TRAS       (0xF << 23) // (SDRC) Number of RAS Active Time Cycles
#define AT91C_SDRC_TXSR       (0xF << 27) // (SDRC) Number of Command Recovery Time Cycles
// -------- SDRC_SRR : (SDRC Offset: 0xc) SDRAM Controller Self-refresh Register --------
#define AT91C_SDRC_SRCB       (0x1 <<  0) // (SDRC) Self-refresh Command Bit
// -------- SDRC_LPR : (SDRC Offset: 0x10) SDRAM Controller Low-power Register --------
#define AT91C_SDRC_LPCB       (0x1 <<  0) // (SDRC) Low-power Command Bit
// -------- SDRC_IER : (SDRC Offset: 0x14) SDRAM Controller Interrupt Enable Register --------
#define AT91C_SDRC_RES        (0x1 <<  0) // (SDRC) Refresh Error Status
// -------- SDRC_IDR : (SDRC Offset: 0x18) SDRAM Controller Interrupt Disable Register --------
// -------- SDRC_IMR : (SDRC Offset: 0x1c) SDRAM Controller Interrupt Mask Register --------
// -------- SDRC_ISR : (SDRC Offset: 0x20) SDRAM Controller Interrupt Status Register --------


// *****************************************************************************
//              SOFTWARE API DEFINITION  FOR Burst Flash Controller Interface
// *****************************************************************************
// -------- BFC_MR : (BFC Offset: 0x0) BFC Mode Register --------
#define AT91C_BFC_BFCOM       (0x3 <<  0) // (BFC) Burst Flash Controller Operating Mode
#define 	AT91C_BFC_BFCOM_DISABLED             (0x0) // (BFC) NPCS0 is driven by the SMC or remains high.
#define 	AT91C_BFC_BFCOM_ASYNC                (0x1) // (BFC) Asynchronous
#define 	AT91C_BFC_BFCOM_BURST_READ           (0x2) // (BFC) Burst Read
#define AT91C_BFC_BFCC        (0x3 <<  2) // (BFC) Burst Flash Controller Operating Mode
#define 	AT91C_BFC_BFCC_MCK                  (0x1 <<  2) // (BFC) Master Clock.
#define 	AT91C_BFC_BFCC_MCK_DIV_2            (0x2 <<  2) // (BFC) Master Clock divided by 2.
#define 	AT91C_BFC_BFCC_MCK_DIV_4            (0x3 <<  2) // (BFC) Master Clock divided by 4.
#define AT91C_BFC_AVL         (0xF <<  4) // (BFC) Address Valid Latency
#define AT91C_BFC_PAGES       (0x7 <<  8) // (BFC) Page Size
#define 	AT91C_BFC_PAGES_NO_PAGE              (0x0 <<  8) // (BFC) No page handling.
#define 	AT91C_BFC_PAGES_16                   (0x1 <<  8) // (BFC) 16 bytes page size.
#define 	AT91C_BFC_PAGES_32                   (0x2 <<  8) // (BFC) 32 bytes page size.
#define 	AT91C_BFC_PAGES_64                   (0x3 <<  8) // (BFC) 64 bytes page size.
#define 	AT91C_BFC_PAGES_128                  (0x4 <<  8) // (BFC) 128 bytes page size.
#define 	AT91C_BFC_PAGES_256                  (0x5 <<  8) // (BFC) 256 bytes page size.
#define 	AT91C_BFC_PAGES_512                  (0x6 <<  8) // (BFC) 512 bytes page size.
#define 	AT91C_BFC_PAGES_1024                 (0x7 <<  8) // (BFC) 1024 bytes page size.
#define AT91C_BFC_OEL         (0x3 << 12) // (BFC) Output Enable Latency
#define AT91C_BFC_BAAEN       (0x1 << 16) // (BFC) Burst Address Advance Enable
#define AT91C_BFC_BFOEH       (0x1 << 17) // (BFC) Burst Flash Output Enable Handling
#define AT91C_BFC_MUXEN       (0x1 << 18) // (BFC) Multiplexed Bus Enable
#define AT91C_BFC_RDYEN       (0x1 << 19) // (BFC) Ready Enable Mode

#endif
