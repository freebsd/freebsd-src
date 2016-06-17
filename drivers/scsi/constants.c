/* 
 * ASCII values for a number of symbolic constants, printing functions,
 * etc.
 * Additions for SCSI 2 and Linux 2.2.x by D. Gilbert (990422)
 *
 */

#define __NO_VERSION__
#include <linux/module.h>

#include <linux/config.h>
#include <linux/blk.h>
#include <linux/kernel.h>
#include "scsi.h"
#include "hosts.h"

#define CONST_COMMAND   0x01
#define CONST_STATUS    0x02
#define CONST_SENSE     0x04
#define CONST_XSENSE    0x08
#define CONST_CMND      0x10
#define CONST_MSG       0x20
#define CONST_HOST	0x40
#define CONST_DRIVER	0x80

static const char unknown[] = "UNKNOWN";

#ifdef CONFIG_SCSI_CONSTANTS
#ifdef CONSTANTS
#undef CONSTANTS
#endif
#define CONSTANTS (CONST_COMMAND | CONST_STATUS | CONST_SENSE | CONST_XSENSE \
		   | CONST_CMND | CONST_MSG | CONST_HOST | CONST_DRIVER)
#endif

#if (CONSTANTS & CONST_COMMAND)
static const char * group_0_commands[] = {
/* 00-03 */ "Test Unit Ready", "Rezero Unit", unknown, "Request Sense",
/* 04-07 */ "Format Unit", "Read Block Limits", unknown, "Reasssign Blocks",
/* 08-0d */ "Read (6)", unknown, "Write (6)", "Seek (6)", unknown, unknown,
/* 0e-12 */ unknown, "Read Reverse", "Write Filemarks", "Space", "Inquiry",  
/* 13-16 */ "Verify", "Recover Buffered Data", "Mode Select", "Reserve",
/* 17-1b */ "Release", "Copy", "Erase", "Mode Sense", "Start/Stop Unit",
/* 1c-1d */ "Receive Diagnostic", "Send Diagnostic", 
/* 1e-1f */ "Prevent/Allow Medium Removal", unknown,
};


static const char *group_1_commands[] = {
/* 20-22 */  unknown, unknown, unknown,
/* 23-28 */ unknown, "Define window parameters", "Read Capacity", 
            unknown, unknown, "Read (10)", 
/* 29-2d */ "Read Generation", "Write (10)", "Seek (10)", "Erase", 
            "Read updated block", 
/* 2e-31 */ "Write Verify","Verify", "Search High", "Search Equal", 
/* 32-34 */ "Search Low", "Set Limits", "Prefetch or Read Position", 
/* 35-37 */ "Synchronize Cache","Lock/Unlock Cache", "Read Defect Data", 
/* 38-3c */ "Medium Scan", "Compare", "Copy Verify", "Write Buffer", 
            "Read Buffer", 
/* 3d-3f */ "Update Block", "Read Long",  "Write Long",
};


static const char *group_2_commands[] = {
/* 40-41 */ "Change Definition", "Write Same", 
/* 42-48 */ "Read sub-channel", "Read TOC", "Read header", 
            "Play audio (10)", unknown, "Play audio msf",
            "Play audio track/index", 
/* 49-4f */ "Play track relative (10)", unknown, "Pause/resume", 
            "Log Select", "Log Sense", unknown, unknown,
/* 50-55 */ unknown, unknown, unknown, unknown, unknown, "Mode Select (10)",
/* 56-5b */ unknown, unknown, unknown, unknown, "Mode Sense (10)", unknown,
/* 5c-5f */ unknown, unknown, unknown,
};


/* The following are 16 byte commands in group 4 */
static const char *group_4_commands[] = {
/* 80-84 */ unknown, unknown, unknown, unknown, unknown,
/* 85-89 */ "Memory Export In (16)", unknown, unknown, unknown,
            "Memory Export Out (16)",
/* 8a-8f */ unknown, unknown, unknown, unknown, unknown, unknown,
/* 90-94 */ unknown, unknown, unknown, unknown, unknown,
/* 95-99 */ unknown, unknown, unknown, unknown, unknown,
/* 9a-9f */ unknown, unknown, unknown, unknown, unknown, unknown,
};


/* The following are 12 byte commands in group 5 */
static const char *group_5_commands[] = {
/* a0-a5 */ unknown, unknown, unknown, unknown, unknown,
            "Move medium/play audio(12)",
/* a6-a9 */ "Exchange medium", unknown, "Read(12)", "Play track relative(12)", 
/* aa-ae */ "Write(12)", unknown, "Erase(12)", unknown, 
            "Write and verify(12)", 
/* af-b1 */ "Verify(12)", "Search data high(12)", "Search data equal(12)",
/* b2-b4 */ "Search data low(12)", "Set limits(12)", unknown,
/* b5-b6 */ "Request volume element address", "Send volume tag",
/* b7-b9 */ "Read defect data(12)", "Read element status", unknown,
/* ba-bf */ unknown, unknown, unknown, unknown, unknown, unknown,
};



#define group(opcode) (((opcode) >> 5) & 7)

#define RESERVED_GROUP  0
#define VENDOR_GROUP    1

static const char **commands[] = {
    group_0_commands, group_1_commands, group_2_commands, 
    (const char **) RESERVED_GROUP, group_4_commands, 
    group_5_commands, (const char **) VENDOR_GROUP, 
    (const char **) VENDOR_GROUP
};

static const char reserved[] = "RESERVED";
static const char vendor[] = "VENDOR SPECIFIC";

static void print_opcode(int opcode) {
    const char **table = commands[ group(opcode) ];
    switch ((unsigned long) table) {
    case RESERVED_GROUP:
	printk("%s(0x%02x) ", reserved, opcode); 
	break;
    case VENDOR_GROUP:
	printk("%s(0x%02x) ", vendor, opcode); 
	break;
    default:
	if (table[opcode & 0x1f] != unknown)
	    printk("%s ",table[opcode & 0x1f]);
	else
	    printk("%s(0x%02x) ", unknown, opcode);
	break;
    }
}
#else /* CONST & CONST_COMMAND */
static void print_opcode(int opcode) {
    printk("0x%02x ", opcode);
}
#endif  

void print_command (unsigned char *command) {
    int i,s;
    print_opcode(command[0]);
    for ( i = 1, s = COMMAND_SIZE(command[0]); i < s; ++i) 
	printk("%02x ", command[i]);
    printk("\n");
}

#if (CONSTANTS & CONST_STATUS)
static const char * statuses[] = {
/* 0-4 */ "Good", "Check Condition", "Condition Met", unknown, "Busy", 
/* 5-9 */ unknown, unknown, unknown, "Intermediate", unknown, 
/* a-c */ "Intermediate-Condition Met", unknown, "Reservation Conflict",
/* d-10 */ unknown, unknown, unknown, unknown,
/* 11-14 */ "Command Terminated", unknown, unknown, "Queue Full",
/* 15-1a */ unknown, unknown, unknown, unknown, unknown, unknown,
/* 1b-1f */ unknown, unknown, unknown, unknown, unknown,
};
#endif

void print_status (int status) {
    status = (status >> 1) & 0x1f;
#if (CONSTANTS & CONST_STATUS)
    printk("%s ",statuses[status]);
#else
    printk("0x%0x ", status); 
#endif 
}

#if (CONSTANTS & CONST_XSENSE)
#define D 0x0001  /* DIRECT ACCESS DEVICE (disk) */
#define T 0x0002  /* SEQUENTIAL ACCESS DEVICE (tape) */
#define L 0x0004  /* PRINTER DEVICE */
#define P 0x0008  /* PROCESSOR DEVICE */
#define W 0x0010  /* WRITE ONCE READ MULTIPLE DEVICE */
#define R 0x0020  /* READ ONLY (CD-ROM) DEVICE */
#define S 0x0040  /* SCANNER DEVICE */
#define O 0x0080  /* OPTICAL MEMORY DEVICE */
#define M 0x0100  /* MEDIA CHANGER DEVICE */
#define C 0x0200  /* COMMUNICATION DEVICE */
#define A 0x0400  /* ARRAY STORAGE */
#define E 0x0800  /* ENCLOSURE SERVICES DEVICE */
#define B 0x1000  /* SIMPLIFIED DIRECT ACCESS DEVICE */
#define K 0x2000  /* OPTICAL CARD READER/WRITER DEVICE */

struct error_info{
    unsigned char code1, code2;
    unsigned short int devices;
    const char * text;
};

struct error_info2{
    unsigned char code1, code2_min, code2_max;
    unsigned short int devices;
    const char * text;
};

static struct error_info2 additional2[] =
{
  {0x40,0x00,0x7f,D,"Ram failure (%x)"},
  {0x40,0x80,0xff,D|T|L|P|W|R|S|O|M|C,"Diagnostic failure on component (%x)"},
  {0x41,0x00,0xff,D,"Data path failure (%x)"},
  {0x42,0x00,0xff,D,"Power-on or self-test failure (%x)"},
  {0, 0, 0, 0, NULL}
};

static struct error_info additional[] =
{
  {0x00,0x00,D|T|L|P|W|R|S|O|M|C|A|E|B|K,"No additional sense information"},
  {0x00,0x01,T,"Filemark detected"},
  {0x00,0x02,T|S,"End-of-partition/medium detected"},
  {0x00,0x03,T,"Setmark detected"},
  {0x00,0x04,T|S,"Beginning-of-partition/medium detected"},
  {0x00,0x05,T|L|S,"End-of-data detected"},
  {0x00,0x06,D|T|L|P|W|R|S|O|M|C|A|E|B|K,"I/O process terminated"},
  {0x00,0x11,R,"Audio play operation in progress"},
  {0x00,0x12,R,"Audio play operation paused"},
  {0x00,0x13,R,"Audio play operation successfully completed"},
  {0x00,0x14,R,"Audio play operation stopped due to error"},
  {0x00,0x15,R,"No current audio status to return"},
  {0x00,0x16,D|T|L|P|W|R|S|O|M|C|A|E|B|K,"Operation in progress"},
  {0x00,0x17,D|T|L|W|R|S|O|M|A|E|B|K,"Cleaning requested"},
  {0x01,0x00,D|W|O|B|K,"No index/sector signal"},
  {0x02,0x00,D|W|R|O|M|B|K,"No seek complete"},
  {0x03,0x00,D|T|L|W|S|O|B|K,"Peripheral device write fault"},
  {0x03,0x01,T,"No write current"},
  {0x03,0x02,T,"Excessive write errors"},
  {0x04,0x00,D|T|L|P|W|R|S|O|M|C|A|E|B|K,"Logical unit not ready,cause not reportable"},
  {0x04,0x01,D|T|L|P|W|R|S|O|M|C|A|E|B|K,"Logical unit is in process of becoming ready"},
  {0x04,0x02,D|T|L|P|W|R|S|O|M|C|A|E|B|K,"Logical unit not ready,initializing cmd. required"},
  {0x04,0x03,D|T|L|P|W|R|S|O|M|C|A|E|B|K,"Logical unit not ready,manual intervention required"},
  {0x04,0x04,D|T|L|R|O|B,"Logical unit not ready,format in progress"},
  {0x04,0x05,D|T|W|O|M|C|A|B|K,"Logical unit not ready,rebuild in progress"},
  {0x04,0x06,D|T|W|O|M|C|A|B|K,"Logical unit not ready,recalculation in progress"},
  {0x04,0x07,D|T|L|P|W|R|S|O|M|C|A|E|B|K,"Logical unit not ready,operation in progress"},
  {0x04,0x08,R,"Logical unit not ready,long write in progress"},
  {0x04,0x09,D|T|L|P|W|R|S|O|M|C|A|E|B|K,"Logical unit not ready,self-test in progress"},
  {0x05,0x00,D|T|L|W|R|S|O|M|C|A|E|B|K,"Logical unit does not respond to selection"},
  {0x06,0x00,D|W|R|O|M|B|K,"No reference position found"},
  {0x07,0x00,D|T|L|W|R|S|O|M|B|K,"Multiple peripheral devices selected"},
  {0x08,0x00,D|T|L|W|R|S|O|M|C|A|E|B|K,"Logical unit communication failure"},
  {0x08,0x01,D|T|L|W|R|S|O|M|C|A|E|B|K,"Logical unit communication time-out"},
  {0x08,0x02,D|T|L|W|R|S|O|M|C|A|E|B|K,"Logical unit communication parity error"},
  {0x08,0x03,D|T|R|O|M|B|K,"Logical unit communication CRC error (Ultra-DMA/32)"},
  {0x08,0x04,D|T|L|P|W|R|S|O|C|K,"Unreachable copy target"},
  {0x09,0x00,D|T|W|R|O|B,"Track following error"},
  {0x09,0x01,W|R|O|K,"Tracking servo failure"},
  {0x09,0x02,W|R|O|K,"Focus servo failure"},
  {0x09,0x03,W|R|O,"Spindle servo failure"},
  {0x09,0x04,D|T|W|R|O|B,"Head select fault"},
  {0x0A,0x00,D|T|L|P|W|R|S|O|M|C|A|E|B|K,"Error log overflow"},
  {0x0B,0x00,D|T|L|P|W|R|S|O|M|C|A|E|B|K,"Warning"},
  {0x0B,0x01,D|T|L|P|W|R|S|O|M|C|A|E|B|K,"Warning - specified temperature exceeded"},
  {0x0B,0x02,D|T|L|P|W|R|S|O|M|C|A|E|B|K,"Warning - enclosure degraded"},
  {0x0C,0x00,T|R|S,"Write error"},
  {0x0C,0x01,K,"Write error - recovered with auto reallocation"},
  {0x0C,0x02,D|W|O|B|K,"Write error - auto reallocation failed"},
  {0x0C,0x03,D|W|O|B|K,"Write error - recommend reassignment"},
  {0x0C,0x04,D|T|W|O|B,"Compression check miscompare error"},
  {0x0C,0x05,D|T|W|O|B,"Data expansion occurred during compression"},
  {0x0C,0x06,D|T|W|O|B,"Block not compressible"},
  {0x0C,0x07,R,"Write error - recovery needed"},
  {0x0C,0x08,R,"Write error - recovery failed"},
  {0x0C,0x09,R,"Write error - loss of streaming"},
  {0x0C,0x0A,R,"Write error - padding blocks added"},
  {0x10,0x00,D|W|O|B|K,"Id CRC or ECC error"},
  {0x11,0x00,D|T|W|R|S|O|B|K,"Unrecovered read error"},
  {0x11,0x01,D|T|W|R|S|O|B|K,"Read retries exhausted"},
  {0x11,0x02,D|T|W|R|S|O|B|K,"Error too long to correct"},
  {0x11,0x03,D|T|W|S|O|B|K,"Multiple read errors"},
  {0x11,0x04,D|W|O|B|K,"Unrecovered read error - auto reallocate failed"},
  {0x11,0x05,W|R|O|B,"L-EC uncorrectable error"},
  {0x11,0x06,W|R|O|B,"CIRC unrecovered error"},
  {0x11,0x07,W|O|B,"Data re-synchronization error"},
  {0x11,0x08,T,"Incomplete block read"},
  {0x11,0x09,T,"No gap found"},
  {0x11,0x0A,D|T|O|B|K,"Miscorrected error"},
  {0x11,0x0B,D|W|O|B|K,"Unrecovered read error - recommend reassignment"},
  {0x11,0x0C,D|W|O|B|K,"Unrecovered read error - recommend rewrite the data"},
  {0x11,0x0D,D|T|W|R|O|B,"De-compression CRC error"},
  {0x11,0x0E,D|T|W|R|O|B,"Cannot decompress using declared algorithm"},
  {0x11,0x0F,R,"Error reading UPC/EAN number"},
  {0x11,0x10,R,"Error reading ISRC number"},
  {0x11,0x11,R,"Read error - loss of streaming"},
  {0x12,0x00,D|W|O|B|K,"Address mark not found for id field"},
  {0x13,0x00,D|W|O|B|K,"Address mark not found for data field"},
  {0x14,0x00,D|T|L|W|R|S|O|B|K,"Recorded entity not found"},
  {0x14,0x01,D|T|W|R|O|B|K,"Record not found"},
  {0x14,0x02,T,"Filemark or setmark not found"},
  {0x14,0x03,T,"End-of-data not found"},
  {0x14,0x04,T,"Block sequence error"},
  {0x14,0x05,D|T|W|O|B|K,"Record not found - recommend reassignment"},
  {0x14,0x06,D|T|W|O|B|K,"Record not found - data auto-reallocated"},
  {0x15,0x00,D|T|L|W|R|S|O|M|B|K,"Random positioning error"},
  {0x15,0x01,D|T|L|W|R|S|O|M|B|K,"Mechanical positioning error"},
  {0x15,0x02,D|T|W|R|O|B|K,"Positioning error detected by read of medium"},
  {0x16,0x00,D|W|O|B|K,"Data synchronization mark error"},
  {0x16,0x01,D|W|O|B|K,"Data sync error - data rewritten"},
  {0x16,0x02,D|W|O|B|K,"Data sync error - recommend rewrite"},
  {0x16,0x03,D|W|O|B|K,"Data sync error - data auto-reallocated"},
  {0x16,0x04,D|W|O|B|K,"Data sync error - recommend reassignment"},
  {0x17,0x00,D|T|W|R|S|O|B|K,"Recovered data with no error correction applied"},
  {0x17,0x01,D|T|W|R|S|O|B|K,"Recovered data with retries"},
  {0x17,0x02,D|T|W|R|O|B|K,"Recovered data with positive head offset"},
  {0x17,0x03,D|T|W|R|O|B|K,"Recovered data with negative head offset"},
  {0x17,0x04,W|R|O|B,"Recovered data with retries and/or circ applied"},
  {0x17,0x05,D|W|R|O|B|K,"Recovered data using previous sector id"},
  {0x17,0x06,D|W|O|B|K,"Recovered data without ecc - data auto-reallocated"},
  {0x17,0x07,D|W|R|O|B|K,"Recovered data without ecc - recommend reassignment"},
  {0x17,0x08,D|W|R|O|B|K,"Recovered data without ecc - recommend rewrite"},
  {0x17,0x09,D|W|R|O|B|K,"Recovered data without ecc - data rewritten"},
  {0x18,0x00,D|T|W|R|O|B|K,"Recovered data with error correction applied"},
  {0x18,0x01,D|W|R|O|B|K,"Recovered data with error corr. & retries applied"},
  {0x18,0x02,D|W|R|O|B|K,"Recovered data - data auto-reallocated"},
  {0x18,0x03,R,"Recovered data with CIRC"},
  {0x18,0x04,R,"Recovered data with L-EC"},
  {0x18,0x05,D|W|R|O|B|K,"Recovered data - recommend reassignment"},
  {0x18,0x06,D|W|R|O|B|K,"Recovered data - recommend rewrite"},
  {0x18,0x07,D|W|O|B|K,"Recovered data with ecc - data rewritten"},
  {0x19,0x00,D|O|K,"Defect list error"},
  {0x19,0x01,D|O|K,"Defect list not available"},
  {0x19,0x02,D|O|K,"Defect list error in primary list"},
  {0x19,0x03,D|O|K,"Defect list error in grown list"},
  {0x1A,0x00,D|T|L|P|W|R|S|O|M|C|A|E|B|K,"Parameter list length error"},
  {0x1B,0x00,D|T|L|P|W|R|S|O|M|C|A|E|B|K,"Synchronous data transfer error"},
  {0x1C,0x00,D|O|B|K,"Defect list not found"},
  {0x1C,0x01,D|O|B|K,"Primary defect list not found"},
  {0x1C,0x02,D|O|B|K,"Grown defect list not found"},
  {0x1D,0x00,D|T|W|R|O|B|K,"Miscompare during verify operation"},
  {0x1E,0x00,D|W|O|B|K,"Recovered id with ecc correction"},
  {0x1F,0x00,D|O|K,"Partial defect list transfer"},
  {0x20,0x00,D|T|L|P|W|R|S|O|M|C|A|E|B|K,"Invalid command operation code"},
  {0x21,0x00,D|T|W|R|O|M|B|K,"Logical block address out of range"},
  {0x21,0x01,D|T|W|R|O|M|B|K,"Invalid element address"},
  {0x22,0x00,D,"Illegal function (use 20 00,24 00,or 26 00)"},
  {0x24,0x00,D|T|L|P|W|R|S|O|M|C|A|E|B|K,"Invalid field in cdb"},
  {0x24,0x01,D|T|L|P|W|R|S|O|M|C|A|E|B|K,"CDB decryption error"},
  {0x25,0x00,D|T|L|P|W|R|S|O|M|C|A|E|B|K,"Logical unit not supported"},
  {0x26,0x00,D|T|L|P|W|R|S|O|M|C|A|E|B|K,"Invalid field in parameter list"},
  {0x26,0x01,D|T|L|P|W|R|S|O|M|C|A|E|B|K,"Parameter not supported"},
  {0x26,0x02,D|T|L|P|W|R|S|O|M|C|A|E|B|K,"Parameter value invalid"},
  {0x26,0x03,D|T|L|P|W|R|S|O|M|C|A|E|K,"Threshold parameters not supported"},
  {0x26,0x04,D|T|L|P|W|R|S|O|M|C|A|E|B|K,"Invalid release of persistent reservation"},
  {0x26,0x05,D|T|L|P|W|R|S|O|M|C|A|B|K,"Data decryption error"},
  {0x26,0x06,D|T|L|P|W|R|S|O|C|K,"Too many target descriptors"},
  {0x26,0x07,D|T|L|P|W|R|S|O|C|K,"Unsupported target descriptor type code"},
  {0x26,0x08,D|T|L|P|W|R|S|O|C|K,"Too many segment descriptors"},
  {0x26,0x09,D|T|L|P|W|R|S|O|C|K,"Unsupported segment descriptor type code"},
  {0x26,0x0A,D|T|L|P|W|R|S|O|C|K,"Unexpected inexact segment"},
  {0x26,0x0B,D|T|L|P|W|R|S|O|C|K,"Inline data length exceeded"},
  {0x26,0x0C,D|T|L|P|W|R|S|O|C|K,"Invalid operation for copy source or destination"},
  {0x26,0x0D,D|T|L|P|W|R|S|O|C|K,"Copy segment granularity violation"},
  {0x27,0x00,D|T|W|R|O|B|K,"Write protected"},
  {0x27,0x01,D|T|W|R|O|B|K,"Hardware write protected"},
  {0x27,0x02,D|T|W|R|O|B|K,"Logical unit software write protected"},
  {0x27,0x03,T|R,"Associated write protect"},
  {0x27,0x04,T|R,"Persistent write protect"},
  {0x27,0x05,T|R,"Permanent write protect"},
  {0x28,0x00,D|T|L|P|W|R|S|O|M|C|A|E|B|K,"Not ready to ready change,medium may have changed"},
  {0x28,0x01,D|T|W|R|O|M|B,"Import or export element accessed"},
  {0x29,0x00,D|T|L|P|W|R|S|O|M|C|A|E|B|K,"Power on,reset,or bus device reset occurred"},
  {0x29,0x01,D|T|L|P|W|R|S|O|M|C|A|E|B|K,"Power on occurred"},
  {0x29,0x02,D|T|L|P|W|R|S|O|M|C|A|E|B|K,"Scsi bus reset occurred"},
  {0x29,0x03,D|T|L|P|W|R|S|O|M|C|A|E|B|K,"Bus device reset function occurred"},
  {0x29,0x04,D|T|L|P|W|R|S|O|M|C|A|E|B|K,"Device internal reset"},
  {0x29,0x05,D|T|L|P|W|R|S|O|M|C|A|E|B|K,"Transceiver mode changed to single-ended"},
  {0x29,0x06,D|T|L|P|W|R|S|O|M|C|A|E|B|K,"Transceiver mode changed to lvd"},
  {0x2A,0x00,D|T|L|W|R|S|O|M|C|A|E|B|K,"Parameters changed"},
  {0x2A,0x01,D|T|L|W|R|S|O|M|C|A|E|B|K,"Mode parameters changed"},
  {0x2A,0x02,D|T|L|W|R|S|O|M|C|A|E|K,"Log parameters changed"},
  {0x2A,0x03,D|T|L|P|W|R|S|O|M|C|A|E|K,"Reservations preempted"},
  {0x2A,0x04,D|T|L|P|W|R|S|O|M|C|A|E,"Reservations released"},
  {0x2A,0x05,D|T|L|P|W|R|S|O|M|C|A|E,"Registrations preempted"},
  {0x2B,0x00,D|T|L|P|W|R|S|O|C|K,"Copy cannot execute since host cannot disconnect"},
  {0x2C,0x00,D|T|L|P|W|R|S|O|M|C|A|E|B|K,"Command sequence error"},
  {0x2C,0x01,S,"Too many windows specified"},
  {0x2C,0x02,S,"Invalid combination of windows specified"},
  {0x2C,0x03,R,"Current program area is not empty"},
  {0x2C,0x04,R,"Current program area is empty"},
  {0x2C,0x05,B,"Illegal power condition request"},
  {0x2D,0x00,T,"Overwrite error on update in place"},
  {0x2F,0x00,D|T|L|P|W|R|S|O|M|C|A|E|B|K,"Commands cleared by another initiator"},
  {0x30,0x00,D|T|W|R|O|M|B|K,"Incompatible medium installed"},
  {0x30,0x01,D|T|W|R|O|B|K,"Cannot read medium - unknown format"},
  {0x30,0x02,D|T|W|R|O|B|K,"Cannot read medium - incompatible format"},
  {0x30,0x03,D|T|R|K,"Cleaning cartridge installed"},
  {0x30,0x04,D|T|W|R|O|B|K,"Cannot write medium - unknown format"},
  {0x30,0x05,D|T|W|R|O|B|K,"Cannot write medium - incompatible format"},
  {0x30,0x06,D|T|W|R|O|B,"Cannot format medium - incompatible medium"},
  {0x30,0x07,D|T|L|W|R|S|O|M|A|E|B|K,"Cleaning failure"},
  {0x30,0x08,R,"Cannot write - application code mismatch"},
  {0x30,0x09,R,"Current session not fixated for append"},
  {0x31,0x00,D|T|W|R|O|B|K,"Medium format corrupted"},
  {0x31,0x01,D|L|R|O|B,"Format command failed"},
  {0x32,0x00,D|W|O|B|K,"No defect spare location available"},
  {0x32,0x01,D|W|O|B|K,"Defect list update failure"},
  {0x33,0x00,T,"Tape length error"},
  {0x34,0x00,D|T|L|P|W|R|S|O|M|C|A|E|B|K,"Enclosure failure"},
  {0x35,0x00,D|T|L|P|W|R|S|O|M|C|A|E|B|K,"Enclosure services failure"},
  {0x35,0x01,D|T|L|P|W|R|S|O|M|C|A|E|B|K,"Unsupported enclosure function"},
  {0x35,0x02,D|T|L|P|W|R|S|O|M|C|A|E|B|K,"Enclosure services unavailable"},
  {0x35,0x03,D|T|L|P|W|R|S|O|M|C|A|E|B|K,"Enclosure services transfer failure"},
  {0x35,0x04,D|T|L|P|W|R|S|O|M|C|A|E|B|K,"Enclosure services transfer refused"},
  {0x36,0x00,L,"Ribbon,ink,or toner failure"},
  {0x37,0x00,D|T|L|W|R|S|O|M|C|A|E|B|K,"Rounded parameter"},
  {0x38,0x00,B,"Event status notification"},
  {0x38,0x02,B,"Esn - power management class event"},
  {0x38,0x04,B,"Esn - media class event"},
  {0x38,0x06,B,"Esn - device busy class event"},
  {0x39,0x00,D|T|L|W|R|S|O|M|C|A|E|K,"Saving parameters not supported"},
  {0x3A,0x00,D|T|L|W|R|S|O|M|B|K,"Medium not present"},
  {0x3A,0x01,D|T|W|R|O|M|B|K,"Medium not present - tray closed"},
  {0x3A,0x02,D|T|W|R|O|M|B|K,"Medium not present - tray open"},
  {0x3A,0x03,D|T|W|R|O|M|B,"Medium not present - loadable"},
  {0x3A,0x04,D|T|W|R|O|M|B,"Medium not present - medium auxiliary memory accessible"},
  {0x3B,0x00,T|L,"Sequential positioning error"},
  {0x3B,0x01,T,"Tape position error at beginning-of-medium"},
  {0x3B,0x02,T,"Tape position error at end-of-medium"},
  {0x3B,0x03,L,"Tape or electronic vertical forms unit not ready"},
  {0x3B,0x04,L,"Slew failure"},
  {0x3B,0x05,L,"Paper jam"},
  {0x3B,0x06,L,"Failed to sense top-of-form"},
  {0x3B,0x07,L,"Failed to sense bottom-of-form"},
  {0x3B,0x08,T,"Reposition error"},
  {0x3B,0x09,S,"Read past end of medium"},
  {0x3B,0x0A,S,"Read past beginning of medium"},
  {0x3B,0x0B,S,"Position past end of medium"},
  {0x3B,0x0C,T|S,"Position past beginning of medium"},
  {0x3B,0x0D,D|T|W|R|O|M|B|K,"Medium destination element full"},
  {0x3B,0x0E,D|T|W|R|O|M|B|K,"Medium source element empty"},
  {0x3B,0x0F,R,"End of medium reached"},
  {0x3B,0x11,D|T|W|R|O|M|B|K,"Medium magazine not accessible"},
  {0x3B,0x12,D|T|W|R|O|M|B|K,"Medium magazine removed"},
  {0x3B,0x13,D|T|W|R|O|M|B|K,"Medium magazine inserted"},
  {0x3B,0x14,D|T|W|R|O|M|B|K,"Medium magazine locked"},
  {0x3B,0x15,D|T|W|R|O|M|B|K,"Medium magazine unlocked"},
  {0x3B,0x16,R,"Mechanical positioning or changer error"},
  {0x3D,0x00,D|T|L|P|W|R|S|O|M|C|A|E|K,"Invalid bits in identify message"},
  {0x3E,0x00,D|T|L|P|W|R|S|O|M|C|A|E|B|K,"Logical unit has not self-configured yet"},
  {0x3E,0x01,D|T|L|P|W|R|S|O|M|C|A|E|B|K,"Logical unit failure"},
  {0x3E,0x02,D|T|L|P|W|R|S|O|M|C|A|E|B|K,"Timeout on logical unit"},
  {0x3E,0x03,D|T|L|P|W|R|S|O|M|C|A|E|B|K,"Logical unit failed self-test"},
  {0x3E,0x04,D|T|L|P|W|R|S|O|M|C|A|E|B|K,"Logical unit unable to update self-test log"},
  {0x3F,0x00,D|T|L|P|W|R|S|O|M|C|A|E|B|K,"Target operating conditions have changed"},
  {0x3F,0x01,D|T|L|P|W|R|S|O|M|C|A|E|B|K,"Microcode has been changed"},
  {0x3F,0x02,D|T|L|P|W|R|S|O|M|C|B|K,"Changed operating definition"},
  {0x3F,0x03,D|T|L|P|W|R|S|O|M|C|A|E|B|K,"Inquiry data has changed"},
  {0x3F,0x04,D|T|W|R|O|M|C|A|E|B|K,"Component device attached"},
  {0x3F,0x05,D|T|W|R|O|M|C|A|E|B|K,"Device identifier changed"},
  {0x3F,0x06,D|T|W|R|O|M|C|A|E|B,"Redundancy group created or modified"},
  {0x3F,0x07,D|T|W|R|O|M|C|A|E|B,"Redundancy group deleted"},
  {0x3F,0x08,D|T|W|R|O|M|C|A|E|B,"Spare created or modified"},
  {0x3F,0x09,D|T|W|R|O|M|C|A|E|B,"Spare deleted"},
  {0x3F,0x0A,D|T|W|R|O|M|C|A|E|B|K,"Volume set created or modified"},
  {0x3F,0x0B,D|T|W|R|O|M|C|A|E|B|K,"Volume set deleted"},
  {0x3F,0x0C,D|T|W|R|O|M|C|A|E|B|K,"Volume set deassigned"},
  {0x3F,0x0D,D|T|W|R|O|M|C|A|E|B|K,"Volume set reassigned"},
  {0x3F,0x0E,D|T|L|P|W|R|S|O|M|C|A|E,"Reported luns data has changed"},
  {0x3F,0x10,D|T|W|R|O|M|B,"Medium loadable"},
  {0x3F,0x11,D|T|W|R|O|M|B,"Medium auxiliary memory accessible"},
  {0x40,0x00,D,"Ram failure (should use 40 nn)"},
  /*
   * FIXME(eric) - need a way to represent wildcards here.
   */
  {0x40,0x00,D|T|L|P|W|R|S|O|M|C|A|E|B|K,"Diagnostic failure on component nn (80h-ffh)"},
  {0x41,0x00,D,"Data path failure (should use 40 nn)"},
  {0x42,0x00,D,"Power-on or self-test failure (should use 40 nn)"},
  {0x43,0x00,D|T|L|P|W|R|S|O|M|C|A|E|B|K,"Message error"},
  {0x44,0x00,D|T|L|P|W|R|S|O|M|C|A|E|B|K,"Internal target failure"},
  {0x45,0x00,D|T|L|P|W|R|S|O|M|C|A|E|B|K,"Select or reselect failure"},
  {0x46,0x00,D|T|L|P|W|R|S|O|M|C|B|K,"Unsuccessful soft reset"},
  {0x47,0x00,D|T|L|P|W|R|S|O|M|C|A|E|B|K,"Scsi parity error"},
  {0x47,0x01,D|T|L|P|W|R|S|O|M|C|A|E|B|K,"Data phase CRC error detected"},
  {0x47,0x02,D|T|L|P|W|R|S|O|M|C|A|E|B|K,"Scsi parity error detected during st data phase"},
  {0x47,0x03,D|T|L|P|W|R|S|O|M|C|A|E|B|K,"Information unit CRC error detected"},
  {0x47,0x04,D|T|L|P|W|R|S|O|M|C|A|E|B|K,"Asynchronous information protection error detected"},
  {0x48,0x00,D|T|L|P|W|R|S|O|M|C|A|E|B|K,"Initiator detected error message received"},
  {0x49,0x00,D|T|L|P|W|R|S|O|M|C|A|E|B|K,"Invalid message error"},
  {0x4A,0x00,D|T|L|P|W|R|S|O|M|C|A|E|B|K,"Command phase error"},
  {0x4B,0x00,D|T|L|P|W|R|S|O|M|C|A|E|B|K,"Data phase error"},
  {0x4C,0x00,D|T|L|P|W|R|S|O|M|C|A|E|B|K,"Logical unit failed self-configuration"},
  /*
   * FIXME(eric) - need a way to represent wildcards here.
   */
  {0x4D,0x00,D|T|L|P|W|R|S|O|M|C|A|E|B|K,"Tagged overlapped commands (nn = queue tag)"},
  {0x4E,0x00,D|T|L|P|W|R|S|O|M|C|A|E|B|K,"Overlapped commands attempted"},
  {0x50,0x00,T,"Write append error"},
  {0x50,0x01,T,"Write append position error"},
  {0x50,0x02,T,"Position error related to timing"},
  {0x51,0x00,T|R|O,"Erase failure"},
  {0x52,0x00,T,"Cartridge fault"},
  {0x53,0x00,D|T|L|W|R|S|O|M|B|K,"Media load or eject failed"},
  {0x53,0x01,T,"Unload tape failure"},
  {0x53,0x02,D|T|W|R|O|M|B|K,"Medium removal prevented"},
  {0x54,0x00,P,"Scsi to host system interface failure"},
  {0x55,0x00,P,"System resource failure"},
  {0x55,0x01,D|O|B|K,"System buffer full"},
  {0x55,0x02,D|T|L|P|W|R|S|O|M|A|E|K,"Insufficient reservation resources"},
  {0x55,0x03,D|T|L|P|W|R|S|O|M|C|A|E,"Insufficient resources"},
  {0x55,0x04,D|T|L|P|W|R|S|O|M|A|E,"Insufficient registration resources"},
  {0x57,0x00,R,"Unable to recover table-of-contents"},
  {0x58,0x00,O,"Generation does not exist"},
  {0x59,0x00,O,"Updated block read"},
  {0x5A,0x00,D|T|L|P|W|R|S|O|M|B|K,"Operator request or state change input"},
  {0x5A,0x01,D|T|W|R|O|M|B|K,"Operator medium removal request"},
  {0x5A,0x02,D|T|W|R|O|A|B|K,"Operator selected write protect"},
  {0x5A,0x03,D|T|W|R|O|A|B|K,"Operator selected write permit"},
  {0x5B,0x00,D|T|L|P|W|R|S|O|M|K,"Log exception"},
  {0x5B,0x01,D|T|L|P|W|R|S|O|M|K,"Threshold condition met"},
  {0x5B,0x02,D|T|L|P|W|R|S|O|M|K,"Log counter at maximum"},
  {0x5B,0x03,D|T|L|P|W|R|S|O|M|K,"Log list codes exhausted"},
  {0x5C,0x00,D|O,"Rpl status change"},
  {0x5C,0x01,D|O,"Spindles synchronized"},
  {0x5C,0x02,D|O,"Spindles not synchronized"},
  {0x5D,0x00,D|T|L|P|W|R|S|O|M|C|A|E|B|K,"Failure prediction threshold exceeded"},
  {0x5D,0x01,R|B,"Media failure prediction threshold exceeded"},
  {0x5D,0x02,R,"Logical unit failure prediction threshold exceeded"},
  {0x5D,0x10,D|B,"Hardware impending failure general hard drive failure"},
  {0x5D,0x11,D|B,"Hardware impending failure drive error rate too high"},
  {0x5D,0x12,D|B,"Hardware impending failure data error rate too high"},
  {0x5D,0x13,D|B,"Hardware impending failure seek error rate too high"},
  {0x5D,0x14,D|B,"Hardware impending failure too many block reassigns"},
  {0x5D,0x15,D|B,"Hardware impending failure access times too high"},
  {0x5D,0x16,D|B,"Hardware impending failure start unit times too high"},
  {0x5D,0x17,D|B,"Hardware impending failure channel parametrics"},
  {0x5D,0x18,D|B,"Hardware impending failure controller detected"},
  {0x5D,0x19,D|B,"Hardware impending failure throughput performance"},
  {0x5D,0x1A,D|B,"Hardware impending failure seek time performance"},
  {0x5D,0x1B,D|B,"Hardware impending failure spin-up retry count"},
  {0x5D,0x1C,D|B,"Hardware impending failure drive calibration retry count"},
  {0x5D,0x20,D|B,"Controller impending failure general hard drive failure"},
  {0x5D,0x21,D|B,"Controller impending failure drive error rate too high"},
  {0x5D,0x22,D|B,"Controller impending failure data error rate too high"},
  {0x5D,0x23,D|B,"Controller impending failure seek error rate too high"},
  {0x5D,0x24,D|B,"Controller impending failure too many block reassigns"},
  {0x5D,0x25,D|B,"Controller impending failure access times too high"},
  {0x5D,0x26,D|B,"Controller impending failure start unit times too high"},
  {0x5D,0x27,D|B,"Controller impending failure channel parametrics"},
  {0x5D,0x28,D|B,"Controller impending failure controller detected"},
  {0x5D,0x29,D|B,"Controller impending failure throughput performance"},
  {0x5D,0x2A,D|B,"Controller impending failure seek time performance"},
  {0x5D,0x2B,D|B,"Controller impending failure spin-up retry count"},
  {0x5D,0x2C,D|B,"Controller impending failure drive calibration retry count"},
  {0x5D,0x30,D|B,"Data channel impending failure general hard drive failure"},
  {0x5D,0x31,D|B,"Data channel impending failure drive error rate too high"},
  {0x5D,0x32,D|B,"Data channel impending failure data error rate too high"},
  {0x5D,0x33,D|B,"Data channel impending failure seek error rate too high"},
  {0x5D,0x34,D|B,"Data channel impending failure too many block reassigns"},
  {0x5D,0x35,D|B,"Data channel impending failure access times too high"},
  {0x5D,0x36,D|B,"Data channel impending failure start unit times too high"},
  {0x5D,0x37,D|B,"Data channel impending failure channel parametrics"},
  {0x5D,0x38,D|B,"Data channel impending failure controller detected"},
  {0x5D,0x39,D|B,"Data channel impending failure throughput performance"},
  {0x5D,0x3A,D|B,"Data channel impending failure seek time performance"},
  {0x5D,0x3B,D|B,"Data channel impending failure spin-up retry count"},
  {0x5D,0x3C,D|B,"Data channel impending failure drive calibration retry count"},
  {0x5D,0x40,D|B,"Servo impending failure general hard drive failure"},
  {0x5D,0x41,D|B,"Servo impending failure drive error rate too high"},
  {0x5D,0x42,D|B,"Servo impending failure data error rate too high"},
  {0x5D,0x43,D|B,"Servo impending failure seek error rate too high"},
  {0x5D,0x44,D|B,"Servo impending failure too many block reassigns"},
  {0x5D,0x45,D|B,"Servo impending failure access times too high"},
  {0x5D,0x46,D|B,"Servo impending failure start unit times too high"},
  {0x5D,0x47,D|B,"Servo impending failure channel parametrics"},
  {0x5D,0x48,D|B,"Servo impending failure controller detected"},
  {0x5D,0x49,D|B,"Servo impending failure throughput performance"},
  {0x5D,0x4A,D|B,"Servo impending failure seek time performance"},
  {0x5D,0x4B,D|B,"Servo impending failure spin-up retry count"},
  {0x5D,0x4C,D|B,"Servo impending failure drive calibration retry count"},
  {0x5D,0x50,D|B,"Spindle impending failure general hard drive failure"},
  {0x5D,0x51,D|B,"Spindle impending failure drive error rate too high"},
  {0x5D,0x52,D|B,"Spindle impending failure data error rate too high"},
  {0x5D,0x53,D|B,"Spindle impending failure seek error rate too high"},
  {0x5D,0x54,D|B,"Spindle impending failure too many block reassigns"},
  {0x5D,0x55,D|B,"Spindle impending failure access times too high"},
  {0x5D,0x56,D|B,"Spindle impending failure start unit times too high"},
  {0x5D,0x57,D|B,"Spindle impending failure channel parametrics"},
  {0x5D,0x58,D|B,"Spindle impending failure controller detected"},
  {0x5D,0x59,D|B,"Spindle impending failure throughput performance"},
  {0x5D,0x5A,D|B,"Spindle impending failure seek time performance"},
  {0x5D,0x5B,D|B,"Spindle impending failure spin-up retry count"},
  {0x5D,0x5C,D|B,"Spindle impending failure drive calibration retry count"},
  {0x5D,0x60,D|B,"Firmware impending failure general hard drive failure"},
  {0x5D,0x61,D|B,"Firmware impending failure drive error rate too high"},
  {0x5D,0x62,D|B,"Firmware impending failure data error rate too high"},
  {0x5D,0x63,D|B,"Firmware impending failure seek error rate too high"},
  {0x5D,0x64,D|B,"Firmware impending failure too many block reassigns"},
  {0x5D,0x65,D|B,"Firmware impending failure access times too high"},
  {0x5D,0x66,D|B,"Firmware impending failure start unit times too high"},
  {0x5D,0x67,D|B,"Firmware impending failure channel parametrics"},
  {0x5D,0x68,D|B,"Firmware impending failure controller detected"},
  {0x5D,0x69,D|B,"Firmware impending failure throughput performance"},
  {0x5D,0x6A,D|B,"Firmware impending failure seek time performance"},
  {0x5D,0x6B,D|B,"Firmware impending failure spin-up retry count"},
  {0x5D,0x6C,D|B,"Firmware impending failure drive calibration retry count"},
  {0x5D,0xFF,D|T|L|P|W|R|S|O|M|C|A|E|B|K,"Failure prediction threshold exceeded (false)"},
  {0x5E,0x00,D|T|L|P|W|R|S|O|C|A|K,"Low power condition on"},
  {0x5E,0x01,D|T|L|P|W|R|S|O|C|A|K,"Idle condition activated by timer"},
  {0x5E,0x02,D|T|L|P|W|R|S|O|C|A|K,"Standby condition activated by timer"},
  {0x5E,0x03,D|T|L|P|W|R|S|O|C|A|K,"Idle condition activated by command"},
  {0x5E,0x04,D|T|L|P|W|R|S|O|C|A|K,"Standby condition activated by command"},
  {0x5E,0x41,B,"Power state change to active"},
  {0x5E,0x42,B,"Power state change to idle"},
  {0x5E,0x43,B,"Power state change to standby"},
  {0x5E,0x45,B,"Power state change to sleep"},
  {0x5E,0x47,B|K,"Power state change to device control"},
  {0x60,0x00,S,"Lamp failure"},
  {0x61,0x00,S,"Video acquisition error"},
  {0x61,0x01,S,"Unable to acquire video"},
  {0x61,0x02,S,"Out of focus"},
  {0x62,0x00,S,"Scan head positioning error"},
  {0x63,0x00,R,"End of user area encountered on this track"},
  {0x63,0x01,R,"Packet does not fit in available space"},
  {0x64,0x00,R,"Illegal mode for this track"},
  {0x64,0x01,R,"Invalid packet size"},
  {0x65,0x00,D|T|L|P|W|R|S|O|M|C|A|E|B|K,"Voltage fault"},
  {0x66,0x00,S,"Automatic document feeder cover up"},
  {0x66,0x01,S,"Automatic document feeder lift up"},
  {0x66,0x02,S,"Document jam in automatic document feeder"},
  {0x66,0x03,S,"Document miss feed automatic in document feeder"},
  {0x67,0x00,A,"Configuration failure"},
  {0x67,0x01,A,"Configuration of incapable logical units failed"},
  {0x67,0x02,A,"Add logical unit failed"},
  {0x67,0x03,A,"Modification of logical unit failed"},
  {0x67,0x04,A,"Exchange of logical unit failed"},
  {0x67,0x05,A,"Remove of logical unit failed"},
  {0x67,0x06,A,"Attachment of logical unit failed"},
  {0x67,0x07,A,"Creation of logical unit failed"},
  {0x67,0x08,A,"Assign failure occurred"},
  {0x67,0x09,A,"Multiply assigned logical unit"},
  {0x68,0x00,A,"Logical unit not configured"},
  {0x69,0x00,A,"Data loss on logical unit"},
  {0x69,0x01,A,"Multiple logical unit failures"},
  {0x69,0x02,A,"Parity/data mismatch"},
  {0x6A,0x00,A,"Informational,refer to log"},
  {0x6B,0x00,A,"State change has occurred"},
  {0x6B,0x01,A,"Redundancy level got better"},
  {0x6B,0x02,A,"Redundancy level got worse"},
  {0x6C,0x00,A,"Rebuild failure occurred"},
  {0x6D,0x00,A,"Recalculate failure occurred"},
  {0x6E,0x00,A,"Command to logical unit failed"},
  {0x6F,0x00,R,"Copy protection key exchange failure - authentication failure"},
  {0x6F,0x01,R,"Copy protection key exchange failure - key not present"},
  {0x6F,0x02,R,"Copy protection key exchange failure - key not established"},
  {0x6F,0x03,R,"Read of scrambled sector without authentication"},
  {0x6F,0x04,R,"Media region code is mismatched to logical unit region"},
  {0x6F,0x05,R,"Drive region must be permanent/region reset count error"},
  /*
   * FIXME(eric) - need a way to represent wildcards here.
   */
  {0x70,0x00,T,"Decompression exception short algorithm id of nn"},
  {0x71,0x00,T,"Decompression exception long algorithm id"},
  {0x72,0x00,R,"Session fixation error"},
  {0x72,0x01,R,"Session fixation error writing lead-in"},
  {0x72,0x02,R,"Session fixation error writing lead-out"},
  {0x72,0x03,R,"Session fixation error - incomplete track in session"},
  {0x72,0x04,R,"Empty or partially written reserved track"},
  {0x72,0x05,R,"No more track reservations allowed"},
  {0x73,0x00,R,"Cd control error"},
  {0x73,0x01,R,"Power calibration area almost full"},
  {0x73,0x02,R,"Power calibration area is full"},
  {0x73,0x03,R,"Power calibration area error"},
  {0x73,0x04,R,"Program memory area update failure"},
  {0x73,0x05,R,"Program memory area is full"},
  {0x73,0x06,R,"RMA/PMA is full"},
  {0, 0, 0, NULL}
};
#endif

#if (CONSTANTS & CONST_SENSE)
static const char *snstext[] = {
    "None",                     /* There is no sense information */
    "Recovered Error",          /* The last command completed successfully
                                   but used error correction */
    "Not Ready",                /* The addressed target is not ready */
    "Medium Error",             /* Data error detected on the medium */
    "Hardware Error",           /* Controller or device failure */
    "Illegal Request",
    "Unit Attention",           /* Removable medium was changed, or
                                   the target has been reset */
    "Data Protect",             /* Access to the data is blocked */
    "Blank Check",              /* Reached unexpected written or unwritten
                                   region of the medium */
    "Key=9",                    /* Vendor specific */
    "Copy Aborted",             /* COPY or COMPARE was aborted */
    "Aborted Command",          /* The target aborted the command */
    "Equal",                    /* A SEARCH DATA command found data equal */
    "Volume Overflow",          /* Medium full with still data to be written */
    "Miscompare",               /* Source data and data on the medium
                                   do not agree */
    "Key=15"                    /* Reserved */
};
#endif

/* Print sense information */
static 
void print_sense_internal(const char * devclass, 
			  const unsigned char * sense_buffer,
			  kdev_t dev)
{
    int i, s;
    int sense_class, valid, code, info;
    const char * error = NULL;
    
    sense_class = (sense_buffer[0] >> 4) & 0x07;
    code = sense_buffer[0] & 0xf;
    valid = sense_buffer[0] & 0x80;
    
    if (sense_class == 7) {	/* extended sense data */
	s = sense_buffer[7] + 8;
	if(s > SCSI_SENSE_BUFFERSIZE)
	   s = SCSI_SENSE_BUFFERSIZE;
	
	info = ((sense_buffer[3] << 24) | (sense_buffer[4] << 16) |
		(sense_buffer[5] << 8) | sense_buffer[6]);
	if (info || valid) {
		printk("Info fld=0x%x", info);
		if (!valid)	/* info data not according to standard */
			printk(" (nonstd)");
		printk(", ");
	}
	if (sense_buffer[2] & 0x80)
           printk( "FMK ");	/* current command has read a filemark */
	if (sense_buffer[2] & 0x40)
           printk( "EOM ");	/* end-of-medium condition exists */
	if (sense_buffer[2] & 0x20)
           printk( "ILI ");	/* incorrect block length requested */
	
	switch (code) {
	case 0x0:
	    error = "Current";	/* error concerns current command */
	    break;
	case 0x1:
	    error = "Deferred";	/* error concerns some earlier command */
            	/* e.g., an earlier write to disk cache succeeded, but
                   now the disk discovers that it cannot write the data */
	    break;
	default:
	    error = "Invalid";
	}
	
	printk("%s ", error);
	
#if (CONSTANTS & CONST_SENSE)
	printk( "%s%s: sense key %s\n", devclass,
	       kdevname(dev), snstext[sense_buffer[2] & 0x0f]);
#else
	printk("%s%s: sns = %2x %2x\n", devclass,
	       kdevname(dev), sense_buffer[0], sense_buffer[2]);
#endif
	
	/* Check to see if additional sense information is available */
	if(sense_buffer[7] + 7 < 13 ||
	   (sense_buffer[12] == 0  && sense_buffer[13] ==  0)) goto done;
	
#if (CONSTANTS & CONST_XSENSE)
	for(i=0; additional[i].text; i++)
	    if(additional[i].code1 == sense_buffer[12] &&
	       additional[i].code2 == sense_buffer[13])
		printk("Additional sense indicates %s\n", additional[i].text);
	
	for(i=0; additional2[i].text; i++)
	    if(additional2[i].code1 == sense_buffer[12] &&
	       additional2[i].code2_min >= sense_buffer[13]  &&
	       additional2[i].code2_max <= sense_buffer[13]) {
		printk("Additional sense indicates ");
		printk(additional2[i].text, sense_buffer[13]);
		printk("\n");
	    };
#else
	printk("ASC=%2x ASCQ=%2x\n", sense_buffer[12], sense_buffer[13]);
#endif
    } else {	/* non-extended sense data */

         /*
          * Standard says:
          *    sense_buffer[0] & 0200 : address valid
          *    sense_buffer[0] & 0177 : vendor-specific error code
          *    sense_buffer[1] & 0340 : vendor-specific
          *    sense_buffer[1..3] : 21-bit logical block address
          */
	
#if (CONSTANTS & CONST_SENSE)
	if (sense_buffer[0] < 15)
	    printk("%s%s: old sense key %s\n", devclass,
	      kdevname(dev), snstext[sense_buffer[0] & 0x0f]);
	else
#endif
	    printk("%s%s: sns = %2x %2x\n", devclass,
	      kdevname(dev), sense_buffer[0], sense_buffer[2]);
	
	printk("Non-extended sense class %d code 0x%0x\n", sense_class, code);
	s = 4;
    }
    
 done:
#if !(CONSTANTS & CONST_SENSE)
    printk("Raw sense data:");
    for (i = 0; i < s; ++i) 
	printk("0x%02x ", sense_buffer[i]);
    printk("\n");
#endif
    return;
}

void print_sense(const char * devclass, Scsi_Cmnd * SCpnt)
{
	print_sense_internal(devclass, SCpnt->sense_buffer,
			     SCpnt->request.rq_dev);
}

void print_req_sense(const char * devclass, Scsi_Request * SRpnt)
{
	print_sense_internal(devclass, SRpnt->sr_sense_buffer,
			     SRpnt->sr_request.rq_dev);
}

#if (CONSTANTS & CONST_MSG) 
static const char *one_byte_msgs[] = {
/* 0x00 */ "Command Complete", NULL, "Save Pointers",
/* 0x03 */ "Restore Pointers", "Disconnect", "Initiator Error", 
/* 0x06 */ "Abort", "Message Reject", "Nop", "Message Parity Error",
/* 0x0a */ "Linked Command Complete", "Linked Command Complete w/flag",
/* 0x0c */ "Bus device reset", "Abort Tag", "Clear Queue", 
/* 0x0f */ "Initiate Recovery", "Release Recovery"
};

#define NO_ONE_BYTE_MSGS (sizeof(one_byte_msgs)  / sizeof (const char *))

static const char *two_byte_msgs[] = {
/* 0x20 */ "Simple Queue Tag", "Head of Queue Tag", "Ordered Queue Tag"
/* 0x23 */ "Ignore Wide Residue"
};

#define NO_TWO_BYTE_MSGS (sizeof(two_byte_msgs)  / sizeof (const char *))

static const char *extended_msgs[] = {
/* 0x00 */ "Modify Data Pointer", "Synchronous Data Transfer Request",
/* 0x02 */ "SCSI-I Extended Identify", "Wide Data Transfer Request"
};

#define NO_EXTENDED_MSGS (sizeof(two_byte_msgs)  / sizeof (const char *))
#endif /* (CONSTANTS & CONST_MSG) */

int print_msg (const unsigned char *msg) {
    int len = 0, i;
    if (msg[0] == EXTENDED_MESSAGE) {
	len = 3 + msg[1];
#if (CONSTANTS & CONST_MSG)
	if (msg[2] < NO_EXTENDED_MSGS)
	    printk ("%s ", extended_msgs[msg[2]]); 
	else 
	    printk ("Extended Message, reserved code (0x%02x) ", (int) msg[2]);
	switch (msg[2]) {
	case EXTENDED_MODIFY_DATA_POINTER:
	    printk("pointer = %d", (int) (msg[3] << 24) | (msg[4] << 16) | 
		   (msg[5] << 8) | msg[6]);
	    break;
	case EXTENDED_SDTR:
	    printk("period = %d ns, offset = %d", (int) msg[3] * 4, (int) 
		   msg[4]);
	    break;
	case EXTENDED_WDTR:
	    printk("width = 2^%d bytes", msg[3]);
	    break;
	default:
	    for (i = 2; i < len; ++i) 
		printk("%02x ", msg[i]);
	}
#else
	for (i = 0; i < len; ++i)
	    printk("%02x ", msg[i]);
#endif
	/* Identify */
    } else if (msg[0] & 0x80) {
#if (CONSTANTS & CONST_MSG)
	printk("Identify disconnect %sallowed %s %d ",
	       (msg[0] & 0x40) ? "" : "not ",
	       (msg[0] & 0x20) ? "target routine" : "lun",
	       msg[0] & 0x7);
#else
	printk("%02x ", msg[0]);
#endif
	len = 1;
	/* Normal One byte */
    } else if (msg[0] < 0x1f) {
#if (CONSTANTS & CONST_MSG)
	if (msg[0] < NO_ONE_BYTE_MSGS)
	    printk(one_byte_msgs[msg[0]]);
	else
	    printk("reserved (%02x) ", msg[0]);
#else
	printk("%02x ", msg[0]);
#endif
	len = 1;
	/* Two byte */
    } else if (msg[0] <= 0x2f) {
#if (CONSTANTS & CONST_MSG)
	if ((msg[0] - 0x20) < NO_TWO_BYTE_MSGS)
	    printk("%s %02x ", two_byte_msgs[msg[0] - 0x20], 
		   msg[1]);
	else 
	    printk("reserved two byte (%02x %02x) ", 
		   msg[0], msg[1]);
#else
	printk("%02x %02x", msg[0], msg[1]);
#endif
	len = 2;
    } else 
#if (CONSTANTS & CONST_MSG)
	printk(reserved);
#else
    printk("%02x ", msg[0]);
#endif
    return len;
}

void print_Scsi_Cmnd (Scsi_Cmnd *cmd) {
    printk("scsi%d : destination target %d, lun %d\n", 
	   cmd->host->host_no, 
	   cmd->target, 
	   cmd->lun);
    printk("        command = ");
    print_command (cmd->cmnd);
}

#if (CONSTANTS & CONST_HOST)
static const char * hostbyte_table[]={
"DID_OK", "DID_NO_CONNECT", "DID_BUS_BUSY", "DID_TIME_OUT", "DID_BAD_TARGET", 
"DID_ABORT", "DID_PARITY", "DID_ERROR", "DID_RESET", "DID_BAD_INTR",
"DID_PASSTHROUGH", "DID_SOFT_ERROR", NULL};

void print_hostbyte(int scsiresult)
{   static int maxcode=0;
    int i;
   
    if(!maxcode) {
	for(i=0;hostbyte_table[i];i++) ;
	maxcode=i-1;
    }
    printk("Hostbyte=0x%02x",host_byte(scsiresult));
    if(host_byte(scsiresult)>maxcode) {
	printk("is invalid "); 
	return;
    }
    printk("(%s) ",hostbyte_table[host_byte(scsiresult)]);
}
#else
void print_hostbyte(int scsiresult)
{   printk("Hostbyte=0x%02x ",host_byte(scsiresult));
}
#endif

#if (CONSTANTS & CONST_DRIVER)
static const char * driverbyte_table[]={
"DRIVER_OK", "DRIVER_BUSY", "DRIVER_SOFT",  "DRIVER_MEDIA", "DRIVER_ERROR", 
"DRIVER_INVALID", "DRIVER_TIMEOUT", "DRIVER_HARD",NULL };

static const char * driversuggest_table[]={"SUGGEST_OK",
"SUGGEST_RETRY", "SUGGEST_ABORT", "SUGGEST_REMAP", "SUGGEST_DIE",
unknown,unknown,unknown, "SUGGEST_SENSE",NULL};


void print_driverbyte(int scsiresult)
{   static int driver_max=0,suggest_max=0;
    int i,dr=driver_byte(scsiresult)&DRIVER_MASK, 
	su=(driver_byte(scsiresult)&SUGGEST_MASK)>>4;

    if(!driver_max) {
        for(i=0;driverbyte_table[i];i++) ;
        driver_max=i;
	for(i=0;driversuggest_table[i];i++) ;
	suggest_max=i;
    }
    printk("Driverbyte=0x%02x",driver_byte(scsiresult));
    printk("(%s,%s) ",
	dr<driver_max  ? driverbyte_table[dr]:"invalid",
	su<suggest_max ? driversuggest_table[su]:"invalid");
}
#else
void print_driverbyte(int scsiresult)
{   printk("Driverbyte=0x%02x ",driver_byte(scsiresult));
}
#endif

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
