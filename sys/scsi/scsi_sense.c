#include <sys/types.h>

/* XXX There should be a way for a type driver to have its own
 *     private senses and add them when it is added.
 */

#if !defined(NO_SCSI_SENSE)

#include "sd.h"
#include "st.h"
#define NSPRINT 0
#include "pt.h"
#include "worm.h"
#include "cd.h"
#define NSCAN 0
#include "od.h"
#include "ch.h"
#define NCOMM 0

static struct
{
	u_char asc;
	u_char ascq;
	char *desc;
} tab[] = {
#if (NCH > 0)
	{0x28, 0x01, "Import or export element accessed" },
	{0x21, 0x01, "Invalid element address" },
	{0x3b, 0x0d, "Medium destination element full" },
	{0x3b, 0x0e, "Medium source element empty" },
#endif
#if (NOD > 0)
	{0x58, 0x00, "Generation does not exist" },
	{0x59, 0x00, "Updated block read" },
#endif
#if (NSCAN > 0)
	{0x2c, 0x02, "Invalid combination of windows specified" },
	{0x60, 0x00, "Lamp failure" },
	{0x61, 0x02, "Out of focus" },
	{0x3b, 0x0c, "Position past beginning of medium" },
	{0x3b, 0x0b, "Position past end of medium" },
	{0x3b, 0x0a, "Read past beginning of medium" },
	{0x3b, 0x09, "Read past end of medium" },
	{0x62, 0x00, "Scan head positioning error" },
	{0x2c, 0x01, "Too many windows specified" },
	{0x61, 0x01, "Unable to acquire video" },
	{0x61, 0x00, "Video acquisition error" },
#endif
#if (NCD > 0)
	{0x00, 0x11, "Audio play operation in progress" },
	{0x00, 0x12, "Audio play operation paused" },
	{0x00, 0x14, "Audio play operation stopped due to error" },
	{0x00, 0x13, "Audio play operation successfully completed" },
	{0x63, 0x00, "End of user area encountered on this track" },
	{0x64, 0x00, "Illegal mode for this track" },
	{0x00, 0x15, "No current audio status to return" },
	{0x18, 0x03, "Recovered data with CIRC" },
	{0x18, 0x04, "Recovered data with L-EC" },
	{0x57, 0x00, "Unable to recover table-of-contents" },
#endif
#if (NWORM > 0)||(NOD > 0)
	{0x11, 0x07, "Data resynchronization error" },
#endif
#if (NWORM > 0)||(NCD > 0)||(NOD > 0)
	{0x11, 0x06, "Circ unrecovered error" },
	{0x09, 0x02, "Focus servo failure" },
	{0x11, 0x05, "L-EC uncorrectable error" },
	{0x17, 0x04, "Recovered data with retries and/or CIRC applied" },
	{0x09, 0x03, "Spindle servo failure" },
	{0x09, 0x01, "Tracking servo failure" },
#endif
#if (NPT > 0)
	{0x54, 0x00, "SCSI to host system interface failure" },
	{0x55, 0x00, "System resource failure" },
#endif
#if (NSPRINT > 0)
	{0x3b, 0x07, "Failed to sense bottom-of-form" },
	{0x3b, 0x06, "Failed to sense top-of-form" },
	{0x3b, 0x05, "Paper jam" },
	{0x36, 0x00, "Ribbon, ink, or toner failure" },
	{0x3b, 0x04, "Slew failure" },
	{0x3b, 0x03, "Tape or electronic vertical forms unit not ready" },
#endif
#if (NST > 0)
	{0x14, 0x04, "Block sequence error" },
	{0x52, 0x00, "Cartridge fault" },
	{0x14, 0x03, "End-of-data not found" },
	{0x03, 0x02, "Excessive write errors" },
	{0x00, 0x01, "Filemark detected" },
	{0x14, 0x02, "Filemark or setmark not found" },
	{0x11, 0x08, "Incomplete block read" },
	{0x11, 0x09, "No gap found" },
	{0x03, 0x01, "No write current" },
	{0x2d, 0x00, "Overwrite error on update in place" },
	{0x50, 0x02, "Position error related to timing" },
	{0x3b, 0x08, "Reposition error" },
	{0x00, 0x03, "Setmark detected" },
	{0x33, 0x00, "Tape length error" },
	{0x3b, 0x01, "Tape position error at beginning-of-medium" },
	{0x3b, 0x02, "Tape position error at end-of-medium" },
	{0x53, 0x01, "Unload tape failure" },
	{0x50, 0x00, "Write append error" },
	{0x50, 0x01, "Write append position error" },
#endif
#if (NST > 0)||(NOD > 0)
	{0x51, 0x00, "Erase failure" },
#endif
#if (NST > 0)||(NSCAN > 0)
	{0x00, 0x04, "Beginning-of-partition/medium detected" },
	{0x00, 0x05, "End-of-data detected" },
	{0x00, 0x02, "End-of-partition/medium detected" },
	{0x0c, 0x00, "Write error" },
#endif
#if (NST > 0)||(NSPRINT > 0)
	{0x3b, 0x00, "Sequential positioning error" },
#endif
#if (NSD > 0)
	{0x41, 0x00, "Data path failure (should use 40 nn)" },
	{0x22, 0x00, "Illegal function (should use 20 00, 24 00, or 26 00)" },
	{0x42, 0x00, "Power-on or self-test failure (should use 40 nn)" },
	{0x40, 0x00, "Ram failure (should use 40 nn)" },
#endif
#if (NSD > 0)||(NOD > 0)
	{0x19, 0x00, "Defect list error" },
	{0x19, 0x03, "Defect list error in grown list" },
	{0x19, 0x02, "Defect list error in primary list" },
	{0x19, 0x01, "Defect list not available" },
	{0x1c, 0x00, "Defect list not found" },
	{0x1c, 0x02, "Grown defect list not found" },
	{0x1c, 0x01, "Primary defect list not found" },
	{0x5c, 0x00, "RPL status change" },
	{0x5c, 0x02, "Spindles not synchronized" },
	{0x5c, 0x01, "Spindles synchronized" },
#endif
#if (NSD > 0)||(NWORM > 0)||(NOD > 0)
	{0x13, 0x00, "Address mark not found for data field" },
	{0x12, 0x00, "Address mark not found for id field" },
	{0x16, 0x00, "Data synchronization mark error" },
	{0x32, 0x01, "Defect list update failure" },
	{0x10, 0x00, "Id CRC or ECC error" },
	{0x1d, 0x00, "Miscompare during verify operation" },
	{0x32, 0x00, "No defect spare location available" },
	{0x01, 0x00, "No index/sector signal" },
	{0x17, 0x06, "Recovered data without ECC - data auto-reallocated" },
	{0x17, 0x07, "Recovered data without ECC - recommend reassignment" },
	{0x17, 0x08, "Recovered data without ECC - recommend rewrite" },
	{0x1e, 0x00, "Recovered ID with ECC correction" },
	{0x11, 0x04, "Unrecovered read error - auto reallocate failed" },
	{0x11, 0x0b, "Unrecovered read error - recommend reassignment" },
	{0x11, 0x0c, "Unrecovered read error - recommend rewrite the data" },
	{0x0c, 0x02, "Write error - auto reallocation failed" },
	{0x0c, 0x01, "Write error recovered with auto reallocation" },
#endif
#if (NSD > 0)||(NWORM > 0)||(NCD > 0)||(NOD > 0)
	{0x18, 0x02, "Recovered data - data auto-reallocated" },
	{0x18, 0x05, "Recovered data - recommend reassignment" },
	{0x18, 0x06, "Recovered data - recommend rewrite" },
	{0x17, 0x05, "Recovered data using previous sector id" },
	{0x18, 0x01, "Recovered data with error correction & retries applied" },
#endif
#if (NSD > 0)||(NWORM > 0)||(NCD > 0)||(NOD > 0)||(NCH > 0)
	{0x06, 0x00, "No reference position found" },
	{0x02, 0x00, "No seek complete" },
#endif
#if (NSD > 0)||(NSPRINT > 0)||(NOD > 0)
	{0x31, 0x01, "Format command failed" },
#endif
#if (NSD > 0)||(NST > 0)
	{0x30, 0x03, "Cleaning cartridge installed" },
#endif
#if (NSD > 0)||(NST > 0)||(NOD > 0)
	{0x11, 0x0a, "Miscorrected error" },
#endif
#if (NSD > 0)||(NST > 0)||(NWORM > 0)||(NOD > 0)
	{0x31, 0x00, "Medium format corrupted" },
	{0x5a, 0x03, "Operator selected write permit" },
	{0x5a, 0x02, "Operator selected write protect" },
	{0x27, 0x00, "Write protected" },
#endif
#if (NSD > 0)||(NST > 0)||(NWORM > 0)||(NSCAN > 0)||(NOD > 0)
	{0x11, 0x02, "Error too long to correct" },
	{0x11, 0x03, "Multiple read errors" },
	{0x11, 0x01, "Read retries exhausted" },
#endif
#if (NSD > 0)||(NST > 0)||(NWORM > 0)||(NCD > 0)||(NOD > 0)
	{0x30, 0x02, "Cannot read medium - incompatible format" },
	{0x30, 0x01, "Cannot read medium - unknown format" },
	{0x15, 0x02, "Positioning error detected by read of medium" },
	{0x14, 0x01, "Record not found" },
	{0x18, 0x00, "Recovered data with error correction applied" },
	{0x17, 0x03, "Recovered data with negative head offset" },
	{0x17, 0x02, "Recovered data with positive head offset" },
	{0x09, 0x00, "Track following error" },
#endif
#if (NSD > 0)||(NST > 0)||(NWORM > 0)||(NCD > 0)||(NOD > 0)||(NCH > 0)
	{0x30, 0x00, "Incompatible medium installed" },
	{0x21, 0x00, "Logical block address out of range" },
	{0x53, 0x02, "Medium removal prevented" },
	{0x5a, 0x01, "Operator medium removal request" },
#endif
#if (NSD > 0)||(NST > 0)||(NWORM > 0)||(NCD > 0)||(NSCAN > 0)||(NOD > 0)
	{0x17, 0x00, "Recovered data with no error correction applied" },
	{0x17, 0x01, "Recovered data with retries" },
	{0x11, 0x00, "Unrecovered read error" },
#endif
#if (NSD > 0)||(NST > 0)||(NSPRINT > 0)||(NOD > 0)
	{0x04, 0x04, "Logical unit not ready, format in progress" },
#endif
#if (NSD > 0)||(NST > 0)||(NSPRINT > 0)||(NWORM > 0)||(NSCAN > 0)||(NOD > 0)
	{0x03, 0x00, "Peripheral device write fault" },
#endif
#if (NSD > 0)||(NST > 0)||(NSPRINT > 0)||(NWORM > 0)||(NCD > 0)||(NSCAN > 0)||(NOD > 0)
	{0x14, 0x00, "Recorded entity not found" },
#endif
#if (NSD > 0)||(NST > 0)||(NSPRINT > 0)||(NWORM > 0)||(NCD > 0)||(NSCAN > 0)||(NOD > 0)||(NCH > 0)
	{0x15, 0x01, "Mechanical positioning error" },
	{0x53, 0x00, "Media load or eject failed" },
	{0x3a, 0x00, "Medium not present" },
	{0x07, 0x00, "Multiple peripheral devices selected" },
	{0x15, 0x00, "Random positioning error" },
#endif
#if (NSD > 0)||(NST > 0)||(NSPRINT > 0)||(NWORM > 0)||(NCD > 0)||(NSCAN > 0)||(NOD > 0)||(NCH > 0)||(ncomm > 0)
	{0x2a, 0x02, "Log parameters changed" },
	{0x08, 0x00, "Logical unit communication failure" },
	{0x08, 0x02, "Logical unit communication parity error" },
	{0x08, 0x01, "Logical unit communication time-out" },
	{0x2a, 0x01, "Mode parameters changed" },
	{0x2a, 0x00, "Parameters changed" },
	{0x37, 0x00, "Rounded parameter" },
	{0x39, 0x00, "Saving parameters not supported" },
#endif
#if (NSD > 0)||(NST > 0)||(NSPRINT > 0)||(NPT > 0)||(NWORM > 0)||(NCD > 0)||(NSCAN > 0)||(NOD > 0)||(ncomm > 0)
	{0x2b, 0x00, "Copy cannot execute since host cannot disconnect" },
#endif
#if (NSD > 0)||(NST > 0)||(NSPRINT > 0)||(NPT > 0)||(NWORM > 0)||(NCD > 0)||(NSCAN > 0)||(NOD > 0)||(NCH > 0)
	{0x5b, 0x02, "Log counter at maximum" },
	{0x5b, 0x00, "Log exception" },
	{0x5b, 0x03, "Log list codes exhausted" },
	{0x5a, 0x00, "Operator request or state change input (unspecified)" },
	{0x5b, 0x01, "Threshold condition met" },
#endif
	{0x3f, 0x02, "Changed operating definition" },
	{0x4a, 0x00, "Command phase error" },
	{0x2c, 0x00, "Command sequence error" },
	{0x2f, 0x00, "Commands cleared by another initiator" },
	{0x4b, 0x00, "Data phase error" },
/*	{0x40, 0xnn, "Diagnostic failure on component nn (80h-ffh)" }, */
	{0x0a, 0x00, "Error log overflow" },
	{0x00, 0x06, "I/O process terminated" },
	{0x48, 0x00, "Initiator detected error message received" },
	{0x3f, 0x03, "Inquiry data has changed" },
	{0x44, 0x00, "Internal target failure" },
	{0x3d, 0x00, "Invalid bits in identify message" },
	{0x20, 0x00, "Invalid command operation code" },
	{0x24, 0x00, "Invalid field in CDB" },
	{0x26, 0x00, "Invalid field in parameter list" },
	{0x49, 0x00, "Invalid message error" },
	{0x05, 0x00, "Logical unit does not respond to selection" },
	{0x4c, 0x00, "Logical unit failed self-configuration" },
	{0x3e, 0x00, "Logical unit has not self-configured yet" },
	{0x04, 0x01, "Logical unit is in process of becoming ready" },
	{0x04, 0x00, "Logical unit not ready, cause not reportable" },
	{0x04, 0x02, "Logical unit not ready, initializing command required" },
	{0x04, 0x03, "Logical unit not ready, manual intervention required" },
	{0x25, 0x00, "Logical unit not supported" },
	{0x43, 0x00, "Message error" },
	{0x3f, 0x01, "Microcode has been changed" },
	{0x00, 0x00, "No additional sense information" },
	{0x28, 0x00, "Not ready to ready transition, medium may have changed" },
	{0x4e, 0x00, "Overlapped commands attempted" },
	{0x1a, 0x00, "Parameter list length error" },
	{0x26, 0x01, "Parameter not supported" },
	{0x26, 0x02, "Parameter value invalid" },
	{0x29, 0x00, "Power on, reset, or bus device reset occurred" },
	{0x47, 0x00, "SCSI parity error" },
	{0x45, 0x00, "Select or reselect failure" },
	{0x1b, 0x00, "Synchronous data transfer error" },
	{0x3f, 0x00, "Target operating conditions have changed" },
	{0x26, 0x03, "Threshold parameters not supported" },
	{0x46, 0x00, "Unsuccessful soft reset" },
};

char *scsi_sense_desc(int asc, int ascq)
{
	int i;

	if (asc >= 0x80 && asc <= 0xff)
		return "Vendor Specific ASC";

	if (ascq >= 0x80 && ascq <= 0xff)
		return "Vendor Specific ASCQ";

	for (i = 0; i < sizeof(tab) / sizeof(tab[0]); i++)
		if (tab[i].asc == asc && tab[i].ascq == ascq)
			return tab[i].desc;

	return "";
}

#else /* NO_SCSI_SENSE */
char *scsi_sense_desc(int asc, int ascq)
{
	return "";
}
#endif
