/*
 * DTC controller, taken from T128 driver by...
 * Copyright 1993, Drew Eckhardt
 *	Visionary Computing
 *	(Unix and Linux consulting and custom programming)
 *	drew@colorado.edu
 *      +1 (303) 440-4894
 *
 * DISTRIBUTION RELEASE 2. 
 *
 * For more information, please consult 
 *
 * 
 * 
 * and 
 *
 * NCR 5380 Family
 * SCSI Protocol Controller
 * Databook
 *
 * NCR Microelectronics
 * 1635 Aeroplaza Drive
 * Colorado Springs, CO 80916
 * 1+ (719) 578-3400
 * 1+ (800) 334-5454
 */

#ifndef DTC3280_H
#define DTC3280_H

#ifndef ASM
int dtc_abort(Scsi_Cmnd *);
int dtc_biosparam(Disk *, kdev_t, int*);
int dtc_detect(Scsi_Host_Template *);
int dtc_queue_command(Scsi_Cmnd *, void (*done)(Scsi_Cmnd *));
int dtc_reset(Scsi_Cmnd *, unsigned int reset_flags);
int dtc_proc_info (char *buffer, char **start, off_t offset,
		   int length, int hostno, int inout);

#ifndef NULL
#define NULL 0
#endif

#ifndef CMD_PER_LUN
#define CMD_PER_LUN 2
#endif

#ifndef CAN_QUEUE
#define CAN_QUEUE 32 
#endif

/* 
 * I hadn't thought of this with the earlier drivers - but to prevent
 * macro definition conflicts, we shouldn't define all of the internal
 * macros when this is being used solely for the host stub.
 */

#define DTC3x80 {				\
	name:           "DTC 3180/3280 ",	\
	detect:         dtc_detect,		\
	queuecommand:   dtc_queue_command,	\
	abort:          dtc_abort,		\
	reset:          dtc_reset,		\
	bios_param:     dtc_biosparam,		\
	can_queue:      CAN_QUEUE,		\
	this_id:        7,			\
	sg_tablesize:   SG_ALL,			\
	cmd_per_lun:    CMD_PER_LUN ,		\
	use_clustering: DISABLE_CLUSTERING}

#define NCR5380_implementation_fields \
    volatile unsigned int base

#define NCR5380_local_declare() \
    volatile unsigned int base

#define NCR5380_setup(instance) \
    base = (unsigned int)(instance)->base

#define DTC_address(reg) (base + DTC_5380_OFFSET + reg)

#define dbNCR5380_read(reg)                                              \
    (rval=isa_readb(DTC_address(reg)), \
     (((unsigned char) printk("DTC : read register %d at addr %08x is: %02x\n"\
    , (reg), (int)DTC_address(reg), rval)), rval ) )

#define dbNCR5380_write(reg, value) do {                                  \
    printk("DTC : write %02x to register %d at address %08x\n",         \
            (value), (reg), (int)DTC_address(reg));     \
    isa_writeb(value, DTC_address(reg));} while(0)


#if !(DTCDEBUG & DTCDEBUG_TRANSFER) 
#define NCR5380_read(reg) (isa_readb(DTC_address(reg)))
#define NCR5380_write(reg, value) (isa_writeb(value, DTC_address(reg)))
#else
#define NCR5380_read(reg) (isa_readb(DTC_address(reg)))
#define xNCR5380_read(reg)						\
    (((unsigned char) printk("DTC : read register %d at address %08x\n"\
    , (reg), DTC_address(reg))), isa_readb(DTC_address(reg)))

#define NCR5380_write(reg, value) do {					\
    printk("DTC : write %02x to register %d at address %08x\n", 	\
	    (value), (reg), (int)DTC_address(reg));	\
    isa_writeb(value, DTC_address(reg));} while(0)
#endif

#define NCR5380_intr dtc_intr
#define do_NCR5380_intr do_dtc_intr
#define NCR5380_queue_command dtc_queue_command
#define NCR5380_abort dtc_abort
#define NCR5380_reset dtc_reset
#define NCR5380_proc_info dtc_proc_info 

/* 15 12 11 10
   1001 1100 0000 0000 */

#define DTC_IRQS 0x9c00


#endif /* ndef ASM */
#endif /* DTC3280_H */
