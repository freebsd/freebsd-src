/*-
 * Copyright (c) 2006 M. Warner Losh.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * This software is derived from software provide by Kwikbyte who specifically
 * disclaimed copyright on the code.
 *
 * $FreeBSD$
 */

#ifndef __LIBAT91RM9200_H
#define __LIBAT91RM9200_H

#include "at91rm9200.h"

//*----------------------------------------------------------------------------
//* \fn    AT91F_PMC_EnablePeriphClock
//* \brief Enable peripheral clock
//*----------------------------------------------------------------------------
static inline void
AT91F_PMC_EnablePeriphClock (
	AT91PS_PMC pPMC, // \arg pointer to PMC controller
	unsigned int periphIds)  // \arg IDs of peripherals to enable
{
	pPMC->PMC_PCER = periphIds;
}

/* *****************************************************************************
                SOFTWARE API FOR PIO
   ***************************************************************************** */
//*----------------------------------------------------------------------------
//* \fn    AT91F_PIO_CfgPeriph
//* \brief Enable pins to be drived by peripheral
//*----------------------------------------------------------------------------
static inline
void AT91F_PIO_CfgPeriph(
	AT91PS_PIO pPio,             // \arg pointer to a PIO controller
	unsigned int periphAEnable,  // \arg PERIPH A to enable
	unsigned int periphBEnable)  // \arg PERIPH B to enable

{
	pPio->PIO_ASR = periphAEnable;
	pPio->PIO_BSR = periphBEnable;
	pPio->PIO_PDR = (periphAEnable | periphBEnable); // Set in Periph mode
}

/* *****************************************************************************
                SOFTWARE API FOR MCI
   ***************************************************************************** */
//* Classic MCI Mode Register Configuration with PDC mode enabled and MCK = MCI Clock
#define AT91C_MCI_MR_PDCMODE	(AT91C_MCI_CLKDIV |\
                                AT91C_MCI_PWSDIV |\
                                (AT91C_MCI_PWSDIV<<1) |\
                                AT91C_MCI_PDCMODE) 

//* Classic MCI Data Timeout Register Configuration with 1048576 MCK cycles between 2 data transfer
#define AT91C_MCI_DTOR_1MEGA_CYCLES	(AT91C_MCI_DTOCYC | AT91C_MCI_DTOMUL)

//* Classic MCI SDCard Register Configuration with 1-bit data bus on slot A
#define AT91C_MCI_MMC_SLOTA	(AT91C_MCI_SCDSEL & 0x0)

//* Classic MCI SDCard Register Configuration with 1-bit data bus on slot B
#define AT91C_MCI_MMC_SLOTB	(AT91C_MCI_SCDSEL)

//* Classic MCI SDCard Register Configuration with 4-bit data bus on slot A
#define AT91C_MCI_SDCARD_4BITS_SLOTA	( (AT91C_MCI_SCDSEL & 0x0) | AT91C_MCI_SCDBUS )

//* Classic MCI SDCard Register Configuration with 4-bit data bus on slot B
#define AT91C_MCI_SDCARD_4BITS_SLOTB	(AT91C_MCI_SCDSEL | AT91C_MCI_SCDBUS)



//*----------------------------------------------------------------------------
//* \fn    AT91F_MCI_Configure
//* \brief Configure the MCI
//*----------------------------------------------------------------------------
static inline
void AT91F_MCI_Configure(
        AT91PS_MCI pMCI,  			 // \arg pointer to a MCI controller
        unsigned int DTOR_register,  // \arg Data Timeout Register to be programmed
        unsigned int MR_register,  	 // \arg Mode Register to be programmed
        unsigned int SDCR_register)  // \arg SDCard Register to be programmed
{
    //* Reset the MCI
    pMCI->MCI_CR = AT91C_MCI_MCIEN | AT91C_MCI_PWSEN;

    //* Disable all the interrupts
    pMCI->MCI_IDR = 0xFFFFFFFF;

    //* Set the Data Timeout Register
    pMCI->MCI_DTOR = DTOR_register;

    //* Set the Mode Register
    pMCI->MCI_MR = MR_register;

    //* Set the SDCard Register
    pMCI->MCI_SDCR = SDCR_register;
}

//*----------------------------------------------------------------------------
//* \fn    AT91F_MCI_EnableIt
//* \brief Enable MCI IT
//*----------------------------------------------------------------------------
static inline
void AT91F_MCI_EnableIt(
        AT91PS_MCI pMCI, // \arg pointer to a MCI controller
        unsigned int flag)   // \arg IT to be enabled
{
    //* Write to the IER register
    pMCI->MCI_IER = flag;
}

//*----------------------------------------------------------------------------
//* \fn    AT91F_MCI_DisableIt
//* \brief Disable MCI IT
//*----------------------------------------------------------------------------
static inline
void AT91F_MCI_DisableIt(
        AT91PS_MCI pMCI, // \arg pointer to a MCI controller
        unsigned int flag)   // \arg IT to be disabled
{
    //* Write to the IDR register
    pMCI->MCI_IDR = flag;
}

//*----------------------------------------------------------------------------
//* \fn    AT91F_MCI_Enable_Interface
//* \brief Enable the MCI Interface
//*----------------------------------------------------------------------------
static inline
void AT91F_MCI_Enable_Interface(
        AT91PS_MCI pMCI)     // \arg pointer to a MCI controller
{
    //* Enable the MCI
    pMCI->MCI_CR = AT91C_MCI_MCIEN;
}

//*----------------------------------------------------------------------------
//* \fn    AT91F_MCI_Disable_Interface
//* \brief Disable the MCI Interface
//*----------------------------------------------------------------------------
static inline
void AT91F_MCI_Disable_Interface(
        AT91PS_MCI pMCI)     // \arg pointer to a MCI controller
{
    //* Disable the MCI
    pMCI->MCI_CR = AT91C_MCI_MCIDIS;
}

//*----------------------------------------------------------------------------
//* \fn    AT91F_MCI_Cfg_ModeRegister
//* \brief Configure the MCI Mode Register
//*----------------------------------------------------------------------------
static inline
void AT91F_MCI_Cfg_ModeRegister(
        AT91PS_MCI pMCI, // \arg pointer to a MCI controller
        unsigned int mode_register)   // \arg value to set in the mode register
{
    //* Configure the MCI MR
    pMCI->MCI_MR = mode_register;
}

/* *****************************************************************************
                SOFTWARE API FOR AIC
   ***************************************************************************** */
#define AT91C_AIC_BRANCH_OPCODE ((void (*) ()) 0xE51FFF20) // ldr, pc, [pc, #-&F20]

//*----------------------------------------------------------------------------
//* \fn    AT91F_AIC_ConfigureIt
//* \brief Interrupt Handler Initialization
//*----------------------------------------------------------------------------
static inline unsigned int
AT91F_AIC_ConfigureIt(
	AT91PS_AIC pAic,  // \arg pointer to the AIC registers
	unsigned int irq_id,     // \arg interrupt number to initialize
	unsigned int priority,   // \arg priority to give to the interrupt
	unsigned int src_type,   // \arg activation and sense of activation
	void (*newHandler) (void) ) // \arg address of the interrupt handler
{
	unsigned int oldHandler;
    unsigned int mask ;

    oldHandler = pAic->AIC_SVR[irq_id];

    mask = 0x1 << irq_id ;
    //* Disable the interrupt on the interrupt controller
    pAic->AIC_IDCR = mask ;
    //* Save the interrupt handler routine pointer and the interrupt priority
    pAic->AIC_SVR[irq_id] = (unsigned int) newHandler ;
    //* Store the Source Mode Register
    pAic->AIC_SMR[irq_id] = src_type | priority  ;
    //* Clear the interrupt on the interrupt controller
    pAic->AIC_ICCR = mask ;

	return oldHandler;
}

//*----------------------------------------------------------------------------
//* \fn    AT91F_AIC_EnableIt
//* \brief Enable corresponding IT number
//*----------------------------------------------------------------------------
static inline
void AT91F_AIC_EnableIt(
	AT91PS_AIC pAic,      // \arg pointer to the AIC registers
	unsigned int irq_id ) // \arg interrupt number to initialize
{
    //* Enable the interrupt on the interrupt controller
    pAic->AIC_IECR = 0x1 << irq_id ;
}

//*----------------------------------------------------------------------------
//* \fn    AT91F_AIC_DisableIt
//* \brief Disable corresponding IT number
//*----------------------------------------------------------------------------
static inline void
AT91F_AIC_DisableIt(
	AT91PS_AIC pAic,      // \arg pointer to the AIC registers
	unsigned int irq_id ) // \arg interrupt number to initialize
{
    unsigned int mask = 0x1 << irq_id;
    //* Disable the interrupt on the interrupt controller
    pAic->AIC_IDCR = mask ;
    //* Clear the interrupt on the Interrupt Controller ( if one is pending )
    pAic->AIC_ICCR = mask ;
}

//*----------------------------------------------------------------------------
//* \fn    AT91F_AIC_ClearIt
//* \brief Clear corresponding IT number
//*----------------------------------------------------------------------------
static inline void
AT91F_AIC_ClearIt(
	AT91PS_AIC pAic,     // \arg pointer to the AIC registers
	unsigned int irq_id) // \arg interrupt number to initialize
{
    //* Clear the interrupt on the Interrupt Controller ( if one is pending )
    pAic->AIC_ICCR = (0x1 << irq_id);
}

//*----------------------------------------------------------------------------
//* \fn    AT91F_AIC_AcknowledgeIt
//* \brief Acknowledge corresponding IT number
//*----------------------------------------------------------------------------
static inline void
AT91F_AIC_AcknowledgeIt(
	AT91PS_AIC pAic)     // \arg pointer to the AIC registers
{
    pAic->AIC_EOICR = pAic->AIC_EOICR;
}

//*----------------------------------------------------------------------------
//* \fn    AT91F_AIC_SetExceptionVector
//* \brief Configure vector handler
//*----------------------------------------------------------------------------
static inline unsigned int
AT91F_AIC_SetExceptionVector(
	unsigned int *pVector, // \arg pointer to the AIC registers
	void (*Handler)(void) )   // \arg Interrupt Handler
{
	unsigned int oldVector = *pVector;

	if ((unsigned int) Handler == (unsigned int) AT91C_AIC_BRANCH_OPCODE)
		*pVector = (unsigned int) AT91C_AIC_BRANCH_OPCODE;
	else
		*pVector = (((((unsigned int) Handler) - ((unsigned int) pVector) - 0x8) >> 2) & 0x00FFFFFF) | 0xEA000000;

	return oldVector;
}

//*----------------------------------------------------------------------------
//* \fn    AT91F_AIC_Trig
//* \brief Trig an IT
//*----------------------------------------------------------------------------
static inline void
AT91F_AIC_Trig(
	AT91PS_AIC pAic,     // \arg pointer to the AIC registers
	unsigned int irq_id) // \arg interrupt number
{
	pAic->AIC_ISCR = (0x1 << irq_id) ;
}

//*----------------------------------------------------------------------------
//* \fn    AT91F_AIC_IsActive
//* \brief Test if an IT is active
//*----------------------------------------------------------------------------
static inline unsigned int
AT91F_AIC_IsActive(
	AT91PS_AIC pAic,     // \arg pointer to the AIC registers
	unsigned int irq_id) // \arg Interrupt Number
{
	return (pAic->AIC_ISR & (0x1 << irq_id));
}

//*----------------------------------------------------------------------------
//* \fn    AT91F_AIC_IsPending
//* \brief Test if an IT is pending
//*----------------------------------------------------------------------------
static inline unsigned int
AT91F_AIC_IsPending(
	AT91PS_AIC pAic,     // \arg pointer to the AIC registers
	unsigned int irq_id) // \arg Interrupt Number
{
	return (pAic->AIC_IPR & (0x1 << irq_id));
}

//*----------------------------------------------------------------------------
//* \fn    AT91F_AIC_Open
//* \brief Set exception vectors and AIC registers to default values
//*----------------------------------------------------------------------------
static inline void
AT91F_AIC_Open(
	AT91PS_AIC pAic,        // \arg pointer to the AIC registers
	void (*IrqHandler)(void),  // \arg Default IRQ vector exception
	void (*FiqHandler)(void),  // \arg Default FIQ vector exception
	void (*DefaultHandler)(void), // \arg Default Handler set in ISR
	void (*SpuriousHandler)(void), // \arg Default Spurious Handler
	unsigned int protectMode)   // \arg Debug Control Register
{
	int i;

	// Disable all interrupts and set IVR to the default handler
	for (i = 0; i < 32; ++i) {
		AT91F_AIC_DisableIt(pAic, i);
		AT91F_AIC_ConfigureIt(pAic, i, AT91C_AIC_PRIOR_LOWEST, AT91C_AIC_SRCTYPE_INT_LEVEL_SENSITIVE, DefaultHandler);
	}

	// Set the IRQ exception vector
	AT91F_AIC_SetExceptionVector((unsigned int *) 0x18, IrqHandler);
	// Set the Fast Interrupt exception vector
	AT91F_AIC_SetExceptionVector((unsigned int *) 0x1C, FiqHandler);

	pAic->AIC_SPU = (unsigned int) SpuriousHandler;
	pAic->AIC_DCR = protectMode;
}

//*----------------------------------------------------------------------------
//* \fn    AT91F_MCI_CfgPMC
//* \brief Enable Peripheral clock in PMC for  MCI
//*----------------------------------------------------------------------------
static inline void
AT91F_MCI_CfgPMC(void)
{
	AT91F_PMC_EnablePeriphClock(
		AT91C_BASE_PMC, // PIO controller base address
		((unsigned int) 1 << AT91C_ID_MCI));
}

//*----------------------------------------------------------------------------
//* \fn    AT91F_MCI_CfgPIO
//* \brief Configure PIO controllers to drive MCI signals
//*----------------------------------------------------------------------------
static inline void
AT91F_MCI_CfgPIO(void)
{
	// Configure PIO controllers to periph mode
	AT91F_PIO_CfgPeriph(
		AT91C_BASE_PIOA, // PIO controller base address
		((unsigned int) AT91C_PA28_MCCDA   ) |
		((unsigned int) AT91C_PA29_MCDA0   ) |
		((unsigned int) AT91C_PA27_MCCK    ), // Peripheral A
		0); // Peripheral B
	// Configure PIO controllers to periph mode
	AT91F_PIO_CfgPeriph(
		AT91C_BASE_PIOB, // PIO controller base address
		0, // Peripheral A
		((unsigned int) AT91C_PB5_MCDA3   ) |
		((unsigned int) AT91C_PB3_MCDA1   ) |
		((unsigned int) AT91C_PB4_MCDA2   )); // Peripheral B
}


/* *****************************************************************************
                SOFTWARE API FOR PDC
   ***************************************************************************** */
//*----------------------------------------------------------------------------
//* \fn    AT91F_PDC_SetNextRx
//* \brief Set the next receive transfer descriptor
//*----------------------------------------------------------------------------
static inline void
AT91F_PDC_SetNextRx (
	AT91PS_PDC pPDC,     // \arg pointer to a PDC controller
	char *address,       // \arg address to the next bloc to be received
	unsigned int bytes)  // \arg number of bytes to be received
{
	pPDC->PDC_RNPR = (unsigned int) address;
	pPDC->PDC_RNCR = bytes;
}

//*----------------------------------------------------------------------------
//* \fn    AT91F_PDC_SetNextTx
//* \brief Set the next transmit transfer descriptor
//*----------------------------------------------------------------------------
static inline void
AT91F_PDC_SetNextTx(
	AT91PS_PDC pPDC,       // \arg pointer to a PDC controller
	char *address,         // \arg address to the next bloc to be transmitted
	unsigned int bytes)    // \arg number of bytes to be transmitted
{
	pPDC->PDC_TNPR = (unsigned int) address;
	pPDC->PDC_TNCR = bytes;
}

//*----------------------------------------------------------------------------
//* \fn    AT91F_PDC_SetRx
//* \brief Set the receive transfer descriptor
//*----------------------------------------------------------------------------
static inline void
AT91F_PDC_SetRx(
	AT91PS_PDC pPDC,       // \arg pointer to a PDC controller
	char *address,         // \arg address to the next bloc to be received
	unsigned int bytes)    // \arg number of bytes to be received
{
	pPDC->PDC_RPR = (unsigned int) address;
	pPDC->PDC_RCR = bytes;
}

//*----------------------------------------------------------------------------
//* \fn    AT91F_PDC_SetTx
//* \brief Set the transmit transfer descriptor
//*----------------------------------------------------------------------------
static inline void
AT91F_PDC_SetTx(
	AT91PS_PDC pPDC,       // \arg pointer to a PDC controller
	char *address,         // \arg address to the next bloc to be transmitted
	unsigned int bytes)    // \arg number of bytes to be transmitted
{
	pPDC->PDC_TPR = (unsigned int) address;
	pPDC->PDC_TCR = bytes;
}

//*----------------------------------------------------------------------------
//* \fn    AT91F_PDC_EnableTx
//* \brief Enable transmit
//*----------------------------------------------------------------------------
static inline void
AT91F_PDC_EnableTx(
	AT91PS_PDC pPDC )       // \arg pointer to a PDC controller
{
	pPDC->PDC_PTCR = AT91C_PDC_TXTEN;
}

//*----------------------------------------------------------------------------
//* \fn    AT91F_PDC_EnableRx
//* \brief Enable receive
//*----------------------------------------------------------------------------
static inline void
AT91F_PDC_EnableRx(
	AT91PS_PDC pPDC )       // \arg pointer to a PDC controller
{
	pPDC->PDC_PTCR = AT91C_PDC_RXTEN;
}

//*----------------------------------------------------------------------------
//* \fn    AT91F_PDC_DisableTx
//* \brief Disable transmit
//*----------------------------------------------------------------------------
static inline void
AT91F_PDC_DisableTx(
	AT91PS_PDC pPDC )       // \arg pointer to a PDC controller
{
	pPDC->PDC_PTCR = AT91C_PDC_TXTDIS;
}

//*----------------------------------------------------------------------------
//* \fn    AT91F_PDC_DisableRx
//* \brief Disable receive
//*----------------------------------------------------------------------------
static inline void
AT91F_PDC_DisableRx(
	AT91PS_PDC pPDC )       // \arg pointer to a PDC controller
{
	pPDC->PDC_PTCR = AT91C_PDC_RXTDIS;
}

//*----------------------------------------------------------------------------
//* \fn    AT91F_PDC_IsTxEmpty
//* \brief Test if the current transfer descriptor has been sent
//*----------------------------------------------------------------------------
static inline int
AT91F_PDC_IsTxEmpty( // \return return 1 if transfer is complete
	AT91PS_PDC pPDC )       // \arg pointer to a PDC controller
{
	return !(pPDC->PDC_TCR);
}

//*----------------------------------------------------------------------------
//* \fn    AT91F_PDC_IsNextTxEmpty
//* \brief Test if the next transfer descriptor has been moved to the current td
//*----------------------------------------------------------------------------
static inline int
AT91F_PDC_IsNextTxEmpty( // \return return 1 if transfer is complete
	AT91PS_PDC pPDC )       // \arg pointer to a PDC controller
{
	return !(pPDC->PDC_TNCR);
}

//*----------------------------------------------------------------------------
//* \fn    AT91F_PDC_IsRxEmpty
//* \brief Test if the current transfer descriptor has been filled
//*----------------------------------------------------------------------------
static inline int
AT91F_PDC_IsRxEmpty( // \return return 1 if transfer is complete
	AT91PS_PDC pPDC )       // \arg pointer to a PDC controller
{
	return !(pPDC->PDC_RCR);
}

//*----------------------------------------------------------------------------
//* \fn    AT91F_PDC_IsNextRxEmpty
//* \brief Test if the next transfer descriptor has been moved to the current td
//*----------------------------------------------------------------------------
static inline int
AT91F_PDC_IsNextRxEmpty( // \return return 1 if transfer is complete
	AT91PS_PDC pPDC )       // \arg pointer to a PDC controller
{
	return !(pPDC->PDC_RNCR);
}

//*----------------------------------------------------------------------------
//* \fn    AT91F_PDC_Open
//* \brief Open PDC: disable TX and RX reset transfer descriptors, re-enable RX and TX
//*----------------------------------------------------------------------------
static inline void
AT91F_PDC_Open(
	AT91PS_PDC pPDC)       // \arg pointer to a PDC controller
{
    //* Disable the RX and TX PDC transfer requests
	AT91F_PDC_DisableRx(pPDC);
	AT91F_PDC_DisableTx(pPDC);

	//* Reset all Counter register Next buffer first
	AT91F_PDC_SetNextTx(pPDC, (char *) 0, 0);
	AT91F_PDC_SetNextRx(pPDC, (char *) 0, 0);
	AT91F_PDC_SetTx(pPDC, (char *) 0, 0);
	AT91F_PDC_SetRx(pPDC, (char *) 0, 0);

    //* Enable the RX and TX PDC transfer requests
	AT91F_PDC_EnableRx(pPDC);
	AT91F_PDC_EnableTx(pPDC);
}

//*----------------------------------------------------------------------------
//* \fn    AT91F_PDC_Close
//* \brief Close PDC: disable TX and RX reset transfer descriptors
//*----------------------------------------------------------------------------
static inline void
AT91F_PDC_Close(
	AT91PS_PDC pPDC)       // \arg pointer to a PDC controller
{
    //* Disable the RX and TX PDC transfer requests
	AT91F_PDC_DisableRx(pPDC);
	AT91F_PDC_DisableTx(pPDC);

	//* Reset all Counter register Next buffer first
	AT91F_PDC_SetNextTx(pPDC, (char *) 0, 0);
	AT91F_PDC_SetNextRx(pPDC, (char *) 0, 0);
	AT91F_PDC_SetTx(pPDC, (char *) 0, 0);
	AT91F_PDC_SetRx(pPDC, (char *) 0, 0);

}

//*----------------------------------------------------------------------------
//* \fn    AT91F_PDC_SendFrame
//* \brief Close PDC: disable TX and RX reset transfer descriptors
//*----------------------------------------------------------------------------
static inline unsigned int
AT91F_PDC_SendFrame(
	AT91PS_PDC pPDC,
	char *pBuffer,
	unsigned int szBuffer,
	char *pNextBuffer,
	unsigned int szNextBuffer )
{
	if (AT91F_PDC_IsTxEmpty(pPDC)) {
		//* Buffer and next buffer can be initialized
		AT91F_PDC_SetTx(pPDC, pBuffer, szBuffer);
		AT91F_PDC_SetNextTx(pPDC, pNextBuffer, szNextBuffer);
		return 2;
	}
	else if (AT91F_PDC_IsNextTxEmpty(pPDC)) {
		//* Only one buffer can be initialized
		AT91F_PDC_SetNextTx(pPDC, pBuffer, szBuffer);
		return 1;
	}
	else {
		//* All buffer are in use...
		return 0;
	}
}

//*----------------------------------------------------------------------------
//* \fn    AT91F_PDC_ReceiveFrame
//* \brief Close PDC: disable TX and RX reset transfer descriptors
//*----------------------------------------------------------------------------
static inline unsigned int
AT91F_PDC_ReceiveFrame(
	AT91PS_PDC pPDC,
	char *pBuffer,
	unsigned int szBuffer,
	char *pNextBuffer,
	unsigned int szNextBuffer )
{
	if (AT91F_PDC_IsRxEmpty(pPDC)) {
		//* Buffer and next buffer can be initialized
		AT91F_PDC_SetRx(pPDC, pBuffer, szBuffer);
		AT91F_PDC_SetNextRx(pPDC, pNextBuffer, szNextBuffer);
		return 2;
	}
	else if (AT91F_PDC_IsNextRxEmpty(pPDC)) {
		//* Only one buffer can be initialized
		AT91F_PDC_SetNextRx(pPDC, pBuffer, szBuffer);
		return 1;
	}
	else {
		//* All buffer are in use...
		return 0;
	}
}

#endif
