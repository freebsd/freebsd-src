/********************************************************
* Header file for eata_dma.c Linux EATA-DMA SCSI driver *
* (c) 1993-96 Michael Neuffer                           *
*             mike@i-Connect.Net                        *
*             neuffer@mail.uni-mainz.de                 *
*********************************************************
* last change: 96/10/14                                 *
********************************************************/

#ifndef _EATA_DMA_H
#define _EATA_DMA_H

#include "eata_generic.h"


#define VER_MAJOR 2
#define VER_MINOR 5
#define VER_SUB   "9b"


/************************************************************************
 * Here you can switch parts of the code on and of                      *
 ************************************************************************/

#define CHECKPAL        0        /* EISA pal checking on/off            */
#define CHECK_BLINK     1        /* Switch Blink state check off, might *
                                  * be nessessary for some MIPS machines*/ 
#define CRIPPLE_QUEUE   0        /* Only enable this if the interrupt 
                                  * controller on your motherboard is 
                                  * broken and you are experiencing 
                                  * massive interrupt losses */

/************************************************************************
 * Debug options.                                                       * 
 * Enable DEBUG and whichever options you require.                      *
 ************************************************************************/
#define DEBUG_EATA      1       /* Enable debug code.                   */
#define DPT_DEBUG       0       /* Bobs special                         */
#define DBG_DELAY       0       /* Build in delays so debug messages can be
				 * be read before they vanish of the top of
				 * the screen!                          */
#define DBG_PROBE       0       /* Debug probe routines.                */
#define DBG_PCI         0       /* Trace PCI routines                   */
#define DBG_EISA        0       /* Trace EISA routines                  */
#define DBG_ISA         0       /* Trace ISA routines                   */ 
#define DBG_BLINK       0       /* Trace Blink check                    */
#define DBG_PIO         0       /* Trace get_config_PIO                 */
#define DBG_COM         0       /* Trace command call                   */
#define DBG_QUEUE       0       /* Trace command queueing.              */
#define DBG_QUEUE2      0       /* Trace command queueing SG.           */
#define DBG_INTR        0       /* Trace interrupt service routine.     */
#define DBG_INTR2       0       /* Trace interrupt service routine.     */
#define DBG_INTR3       0       /* Trace get_board_data interrupts.     */
#define DBG_REQSENSE    0       /* Trace request sense commands         */     
#define DBG_RESET       0       /* Trace reset calls                    */     
#define DBG_STATUS      0       /* Trace status generation              */
#define DBG_PROC        0       /* Debug proc-fs related statistics     */
#define DBG_PROC_WRITE  0
#define DBG_REGISTER    0       /* */
#define DBG_ABNORM      1       /* Debug abnormal actions (reset, abort)*/

#if DEBUG_EATA 
#define DBG(x, y)   if ((x)) {y;} 
#else
#define DBG(x, y)
#endif

int eata_detect(Scsi_Host_Template *);
const char *eata_info(struct Scsi_Host *);
int eata_command(Scsi_Cmnd *);
int eata_queue(Scsi_Cmnd *, void (* done)(Scsi_Cmnd *));
int eata_abort(Scsi_Cmnd *);
int eata_reset(Scsi_Cmnd *, unsigned int);
int eata_proc_info(char *, char **, off_t, int, int, int);
#ifdef MODULE
int eata_release(struct Scsi_Host *);
#else
#define eata_release NULL  
#endif

#include <scsi/scsicam.h>

#define EATA_DMA {                                      \
        proc_info:         eata_proc_info,     /* procinfo       */ \
        name:              "EATA (Extended Attachment) HBA driver", \
        detect:            eata_detect,                 \
        release:           eata_release,                \
	queuecommand:      eata_queue,                  \
	abort:             eata_abort,                  \
	reset:             eata_reset,                  \
	bios_param:        scsicam_bios_param,          \
	unchecked_isa_dma: 1,      /* True if ISA  */   \
	use_clustering:    ENABLE_CLUSTERING }


#endif /* _EATA_DMA_H */

/*
 * Overrides for Emacs so that we almost follow Linus's tabbing style.
 * Emacs will notice this stuff at the end of the file and automatically
 * adjust the settings for this buffer only.  This must remain at the end
 * of the file.
 * ---------------------------------------------------------------------------
 * Local variables:
 * c-indent-level: 4
 * c-brace-imaginary-offset: 0
 * c-brace-offset: -4
 * c-argdecl-indent: 4
 * c-label-offset: -4
 * c-continued-statement-offset: 4
 * c-continued-brace-offset: 0
 * indent-tabs-mode: nil
 * tab-width: 8
 * End:
 */
