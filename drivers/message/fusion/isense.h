#ifndef ISENSE_H_INCLUDED
#define ISENSE_H_INCLUDED
/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/

#ifdef __KERNEL__
#include <linux/types.h>		/* needed for u8, etc. */
#include <linux/string.h>		/* needed for strcat   */
#include <linux/kernel.h>		/* needed for sprintf  */
#else
    #ifndef U_STUFF_DEFINED
    #define U_STUFF_DEFINED
    typedef unsigned char u8;
    typedef unsigned short u16;
    typedef unsigned int u32;
    #endif
#endif

#include "scsi3.h"			/* needed for all things SCSI */

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
/*
 *  Defines and typedefs...
 */

#ifdef __KERNEL__
#define PrintF(x) printk x
#else
#define PrintF(x) printf x
#endif

#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif

#define RETRY_STATUS  ((int) 1)
#define PUT_STATUS    ((int) 0)

/*
 *    A generic structure to hold info about IO request that caused
 *    a Request Sense to be performed, and the resulting Sense Data.
 */
typedef struct IO_Info
{
    char *DevIDStr;   /* String of chars which identifies the device.       */
    u8   *cdbPtr;     /* Pointer (Virtual/Logical addr) to CDB bytes of
                           IO request that caused ContAllegianceCond.       */
    u8   *sensePtr;   /* Pointer (Virtual/Logical addr) to Sense Data
                           returned by Request Sense operation.             */
    u8   *dataPtr;    /* Pointer (Virtual/Logical addr) to Data buffer
                           of IO request caused ContAllegianceCondition.    */
    u8   *inqPtr;     /* Pointer (Virtual/Logical addr) to Inquiry Data for
                           IO *Device* that caused ContAllegianceCondition. */
    u8    SCSIStatus; /* SCSI status byte of IO request that caused
                           Contingent Allegiance Condition.                 */
    u8    DoDisplay;  /* Shall we display any messages?                     */
    u16   rsvd_align1;
    u32   ComplCode;  /* Four-byte OS-dependent completion code.            */
    u32   NotifyL;    /* Four-byte OS-dependent notification field.         */
} IO_Info_t;

/*
 *  SCSI Additional Sense Code and Additional Sense Code Qualifier table.
 */
typedef struct ASCQ_Table
{
    u8     ASC;
    u8     ASCQ;
    char  *DevTypes;
    char  *Description;
} ASCQ_Table_t;

#if 0
/*
 *  SCSI Opcodes table.
 */
typedef struct SCSI_OPS_Table
{
    u8     OpCode;
    char  *DevTypes;
    char  *ScsiCmndStr;
} SCSI_OPS_Table_t;
#endif

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
/*
 *  Public entry point prototypes
 */

/* in scsiherr.c, needed by mptscsih.c */
extern int	 mpt_ScsiHost_ErrorReport(IO_Info_t *ioop);

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
#endif

