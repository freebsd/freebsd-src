#ifndef SCSI_ASCQ_TBL_C_INCLUDED
#define SCSI_ASCQ_TBL_C_INCLUDED

/* AuToMaGiCaLlY generated from: "t10.org/asc-num.txt"
 *******************************************************************************
 * File: ASC-NUM.TXT
 * 
 * SCSI ASC/ASCQ Assignments
 * Numeric Sorted Listing
 * as of  5/18/00
 * 
 *          D - DIRECT ACCESS DEVICE (SBC-2)                   device column key
 *          .T - SEQUENTIAL ACCESS DEVICE (SSC)               -------------------
 *          . L - PRINTER DEVICE (SSC)                           blank = reserved
 *          .  P - PROCESSOR DEVICE (SPC)                     not blank = allowed
 *          .  .W - WRITE ONCE READ MULTIPLE DEVICE (SBC-2)
 *          .  . R - CD DEVICE (MMC)
 *          .  .  S - SCANNER DEVICE (SCSI-2)
 *          .  .  .O - OPTICAL MEMORY DEVICE (SBC-2)
 *          .  .  . M - MEDIA CHANGER DEVICE (SMC)
 *          .  .  .  C - COMMUNICATION DEVICE (SCSI-2)
 *          .  .  .  .A - STORAGE ARRAY DEVICE (SCC)
 *          .  .  .  . E - ENCLOSURE SERVICES DEVICE (SES)
 *          .  .  .  .  B - SIMPLIFIED DIRECT-ACCESS DEVICE (RBC)
 *          .  .  .  .  .K - OPTICAL CARD READER/WRITER DEVICE (OCRW)
 * ASC/ASCQ DTLPWRSOMCAEBK  Description
 * -------  --------------  ----------------------------------------------------
 */

static char SenseDevTypes001[] = "DTLPWRSOMCAEBK";
static char SenseDevTypes002[] = ".T............";
static char SenseDevTypes003[] = ".T....S.......";
static char SenseDevTypes004[] = ".TL...S.......";
static char SenseDevTypes005[] = ".....R........";
static char SenseDevTypes006[] = "DTL.WRSOM.AEBK";
static char SenseDevTypes007[] = "D...W..O....BK";
static char SenseDevTypes008[] = "D...WR.OM...BK";
static char SenseDevTypes009[] = "DTL.W.SO....BK";
static char SenseDevTypes010[] = "DTL..R.O....B.";
static char SenseDevTypes011[] = "DT..W..OMCA.BK";
static char SenseDevTypes012[] = "..............";
static char SenseDevTypes013[] = "DTL.WRSOMCAEBK";
static char SenseDevTypes014[] = "DTL.WRSOM...BK";
static char SenseDevTypes015[] = "DT...R.OM...BK";
static char SenseDevTypes016[] = "DTLPWRSO.C...K";
static char SenseDevTypes017[] = "DT..WR.O....B.";
static char SenseDevTypes018[] = "....WR.O.....K";
static char SenseDevTypes019[] = "....WR.O......";
static char SenseDevTypes020[] = ".T...RS.......";
static char SenseDevTypes021[] = ".............K";
static char SenseDevTypes022[] = "DT..W..O....B.";
static char SenseDevTypes023[] = "DT..WRSO....BK";
static char SenseDevTypes024[] = "DT..W.SO....BK";
static char SenseDevTypes025[] = "....WR.O....B.";
static char SenseDevTypes026[] = "....W..O....B.";
static char SenseDevTypes027[] = "DT.....O....BK";
static char SenseDevTypes028[] = "DTL.WRSO....BK";
static char SenseDevTypes029[] = "DT..WR.O....BK";
static char SenseDevTypes030[] = "DT..W..O....BK";
static char SenseDevTypes031[] = "D...WR.O....BK";
static char SenseDevTypes032[] = "D......O.....K";
static char SenseDevTypes033[] = "D......O....BK";
static char SenseDevTypes034[] = "DT..WR.OM...BK";
static char SenseDevTypes035[] = "D.............";
static char SenseDevTypes036[] = "DTLPWRSOMCAE.K";
static char SenseDevTypes037[] = "DTLPWRSOMCA.BK";
static char SenseDevTypes038[] = ".T...R........";
static char SenseDevTypes039[] = "DT..WR.OM...B.";
static char SenseDevTypes040[] = "DTL.WRSOMCAE.K";
static char SenseDevTypes041[] = "DTLPWRSOMCAE..";
static char SenseDevTypes042[] = "......S.......";
static char SenseDevTypes043[] = "............B.";
static char SenseDevTypes044[] = "DTLPWRSO.CA..K";
static char SenseDevTypes045[] = "DT...R.......K";
static char SenseDevTypes046[] = "D.L..R.O....B.";
static char SenseDevTypes047[] = "..L...........";
static char SenseDevTypes048[] = ".TL...........";
static char SenseDevTypes049[] = "DTLPWRSOMC..BK";
static char SenseDevTypes050[] = "DT..WR.OMCAEBK";
static char SenseDevTypes051[] = "DT..WR.OMCAEB.";
static char SenseDevTypes052[] = ".T...R.O......";
static char SenseDevTypes053[] = "...P..........";
static char SenseDevTypes054[] = "DTLPWRSOM.AE.K";
static char SenseDevTypes055[] = "DTLPWRSOM.AE..";
static char SenseDevTypes056[] = ".......O......";
static char SenseDevTypes057[] = "DTLPWRSOM...BK";
static char SenseDevTypes058[] = "DT..WR.O..A.BK";
static char SenseDevTypes059[] = "DTLPWRSOM....K";
static char SenseDevTypes060[] = "D......O......";
static char SenseDevTypes061[] = ".....R......B.";
static char SenseDevTypes062[] = "D...........B.";
static char SenseDevTypes063[] = "............BK";
static char SenseDevTypes064[] = "..........A...";

static ASCQ_Table_t ASCQ_Table[] = {
  {
    0x00, 0x00,
    SenseDevTypes001,
    "NO ADDITIONAL SENSE INFORMATION"
  },
  {
    0x00, 0x01,
    SenseDevTypes002,
    "FILEMARK DETECTED"
  },
  {
    0x00, 0x02,
    SenseDevTypes003,
    "END-OF-PARTITION/MEDIUM DETECTED"
  },
  {
    0x00, 0x03,
    SenseDevTypes002,
    "SETMARK DETECTED"
  },
  {
    0x00, 0x04,
    SenseDevTypes003,
    "BEGINNING-OF-PARTITION/MEDIUM DETECTED"
  },
  {
    0x00, 0x05,
    SenseDevTypes004,
    "END-OF-DATA DETECTED"
  },
  {
    0x00, 0x06,
    SenseDevTypes001,
    "I/O PROCESS TERMINATED"
  },
  {
    0x00, 0x11,
    SenseDevTypes005,
    "AUDIO PLAY OPERATION IN PROGRESS"
  },
  {
    0x00, 0x12,
    SenseDevTypes005,
    "AUDIO PLAY OPERATION PAUSED"
  },
  {
    0x00, 0x13,
    SenseDevTypes005,
    "AUDIO PLAY OPERATION SUCCESSFULLY COMPLETED"
  },
  {
    0x00, 0x14,
    SenseDevTypes005,
    "AUDIO PLAY OPERATION STOPPED DUE TO ERROR"
  },
  {
    0x00, 0x15,
    SenseDevTypes005,
    "NO CURRENT AUDIO STATUS TO RETURN"
  },
  {
    0x00, 0x16,
    SenseDevTypes001,
    "OPERATION IN PROGRESS"
  },
  {
    0x00, 0x17,
    SenseDevTypes006,
    "CLEANING REQUESTED"
  },
  {
    0x01, 0x00,
    SenseDevTypes007,
    "NO INDEX/SECTOR SIGNAL"
  },
  {
    0x02, 0x00,
    SenseDevTypes008,
    "NO SEEK COMPLETE"
  },
  {
    0x03, 0x00,
    SenseDevTypes009,
    "PERIPHERAL DEVICE WRITE FAULT"
  },
  {
    0x03, 0x01,
    SenseDevTypes002,
    "NO WRITE CURRENT"
  },
  {
    0x03, 0x02,
    SenseDevTypes002,
    "EXCESSIVE WRITE ERRORS"
  },
  {
    0x04, 0x00,
    SenseDevTypes001,
    "LOGICAL UNIT NOT READY, CAUSE NOT REPORTABLE"
  },
  {
    0x04, 0x01,
    SenseDevTypes001,
    "LOGICAL UNIT IS IN PROCESS OF BECOMING READY"
  },
  {
    0x04, 0x02,
    SenseDevTypes001,
    "LOGICAL UNIT NOT READY, INITIALIZING CMD. REQUIRED"
  },
  {
    0x04, 0x03,
    SenseDevTypes001,
    "LOGICAL UNIT NOT READY, MANUAL INTERVENTION REQUIRED"
  },
  {
    0x04, 0x04,
    SenseDevTypes010,
    "LOGICAL UNIT NOT READY, FORMAT IN PROGRESS"
  },
  {
    0x04, 0x05,
    SenseDevTypes011,
    "LOGICAL UNIT NOT READY, REBUILD IN PROGRESS"
  },
  {
    0x04, 0x06,
    SenseDevTypes011,
    "LOGICAL UNIT NOT READY, RECALCULATION IN PROGRESS"
  },
  {
    0x04, 0x07,
    SenseDevTypes001,
    "LOGICAL UNIT NOT READY, OPERATION IN PROGRESS"
  },
  {
    0x04, 0x08,
    SenseDevTypes005,
    "LOGICAL UNIT NOT READY, LONG WRITE IN PROGRESS"
  },
  {
    0x04, 0x09,
    SenseDevTypes001,
    "LOGICAL UNIT NOT READY, SELF-TEST IN PROGRESS"
  },
  {
    0x04, 0x10,
    SenseDevTypes012,
    "auxiliary memory code 2 (99-148) [proposed]"
  },
  {
    0x05, 0x00,
    SenseDevTypes013,
    "LOGICAL UNIT DOES NOT RESPOND TO SELECTION"
  },
  {
    0x06, 0x00,
    SenseDevTypes008,
    "NO REFERENCE POSITION FOUND"
  },
  {
    0x07, 0x00,
    SenseDevTypes014,
    "MULTIPLE PERIPHERAL DEVICES SELECTED"
  },
  {
    0x08, 0x00,
    SenseDevTypes013,
    "LOGICAL UNIT COMMUNICATION FAILURE"
  },
  {
    0x08, 0x01,
    SenseDevTypes013,
    "LOGICAL UNIT COMMUNICATION TIME-OUT"
  },
  {
    0x08, 0x02,
    SenseDevTypes013,
    "LOGICAL UNIT COMMUNICATION PARITY ERROR"
  },
  {
    0x08, 0x03,
    SenseDevTypes015,
    "LOGICAL UNIT COMMUNICATION CRC ERROR (ULTRA-DMA/32)"
  },
  {
    0x08, 0x04,
    SenseDevTypes016,
    "UNREACHABLE COPY TARGET"
  },
  {
    0x09, 0x00,
    SenseDevTypes017,
    "TRACK FOLLOWING ERROR"
  },
  {
    0x09, 0x01,
    SenseDevTypes018,
    "TRACKING SERVO FAILURE"
  },
  {
    0x09, 0x02,
    SenseDevTypes018,
    "FOCUS SERVO FAILURE"
  },
  {
    0x09, 0x03,
    SenseDevTypes019,
    "SPINDLE SERVO FAILURE"
  },
  {
    0x09, 0x04,
    SenseDevTypes017,
    "HEAD SELECT FAULT"
  },
  {
    0x0A, 0x00,
    SenseDevTypes001,
    "ERROR LOG OVERFLOW"
  },
  {
    0x0B, 0x00,
    SenseDevTypes001,
    "WARNING"
  },
  {
    0x0B, 0x01,
    SenseDevTypes001,
    "WARNING - SPECIFIED TEMPERATURE EXCEEDED"
  },
  {
    0x0B, 0x02,
    SenseDevTypes001,
    "WARNING - ENCLOSURE DEGRADED"
  },
  {
    0x0C, 0x00,
    SenseDevTypes020,
    "WRITE ERROR"
  },
  {
    0x0C, 0x01,
    SenseDevTypes021,
    "WRITE ERROR - RECOVERED WITH AUTO REALLOCATION"
  },
  {
    0x0C, 0x02,
    SenseDevTypes007,
    "WRITE ERROR - AUTO REALLOCATION FAILED"
  },
  {
    0x0C, 0x03,
    SenseDevTypes007,
    "WRITE ERROR - RECOMMEND REASSIGNMENT"
  },
  {
    0x0C, 0x04,
    SenseDevTypes022,
    "COMPRESSION CHECK MISCOMPARE ERROR"
  },
  {
    0x0C, 0x05,
    SenseDevTypes022,
    "DATA EXPANSION OCCURRED DURING COMPRESSION"
  },
  {
    0x0C, 0x06,
    SenseDevTypes022,
    "BLOCK NOT COMPRESSIBLE"
  },
  {
    0x0C, 0x07,
    SenseDevTypes005,
    "WRITE ERROR - RECOVERY NEEDED"
  },
  {
    0x0C, 0x08,
    SenseDevTypes005,
    "WRITE ERROR - RECOVERY FAILED"
  },
  {
    0x0C, 0x09,
    SenseDevTypes005,
    "WRITE ERROR - LOSS OF STREAMING"
  },
  {
    0x0C, 0x0A,
    SenseDevTypes005,
    "WRITE ERROR - PADDING BLOCKS ADDED"
  },
  {
    0x0C, 0x0B,
    SenseDevTypes012,
    "auxiliary memory code 4 (99-148) [proposed]"
  },
  {
    0x10, 0x00,
    SenseDevTypes007,
    "ID CRC OR ECC ERROR"
  },
  {
    0x11, 0x00,
    SenseDevTypes023,
    "UNRECOVERED READ ERROR"
  },
  {
    0x11, 0x01,
    SenseDevTypes023,
    "READ RETRIES EXHAUSTED"
  },
  {
    0x11, 0x02,
    SenseDevTypes023,
    "ERROR TOO LONG TO CORRECT"
  },
  {
    0x11, 0x03,
    SenseDevTypes024,
    "MULTIPLE READ ERRORS"
  },
  {
    0x11, 0x04,
    SenseDevTypes007,
    "UNRECOVERED READ ERROR - AUTO REALLOCATE FAILED"
  },
  {
    0x11, 0x05,
    SenseDevTypes025,
    "L-EC UNCORRECTABLE ERROR"
  },
  {
    0x11, 0x06,
    SenseDevTypes025,
    "CIRC UNRECOVERED ERROR"
  },
  {
    0x11, 0x07,
    SenseDevTypes026,
    "DATA RE-SYNCHRONIZATION ERROR"
  },
  {
    0x11, 0x08,
    SenseDevTypes002,
    "INCOMPLETE BLOCK READ"
  },
  {
    0x11, 0x09,
    SenseDevTypes002,
    "NO GAP FOUND"
  },
  {
    0x11, 0x0A,
    SenseDevTypes027,
    "MISCORRECTED ERROR"
  },
  {
    0x11, 0x0B,
    SenseDevTypes007,
    "UNRECOVERED READ ERROR - RECOMMEND REASSIGNMENT"
  },
  {
    0x11, 0x0C,
    SenseDevTypes007,
    "UNRECOVERED READ ERROR - RECOMMEND REWRITE THE DATA"
  },
  {
    0x11, 0x0D,
    SenseDevTypes017,
    "DE-COMPRESSION CRC ERROR"
  },
  {
    0x11, 0x0E,
    SenseDevTypes017,
    "CANNOT DECOMPRESS USING DECLARED ALGORITHM"
  },
  {
    0x11, 0x0F,
    SenseDevTypes005,
    "ERROR READING UPC/EAN NUMBER"
  },
  {
    0x11, 0x10,
    SenseDevTypes005,
    "ERROR READING ISRC NUMBER"
  },
  {
    0x11, 0x11,
    SenseDevTypes005,
    "READ ERROR - LOSS OF STREAMING"
  },
  {
    0x11, 0x12,
    SenseDevTypes012,
    "auxiliary memory code 3 (99-148) [proposed]"
  },
  {
    0x12, 0x00,
    SenseDevTypes007,
    "ADDRESS MARK NOT FOUND FOR ID FIELD"
  },
  {
    0x13, 0x00,
    SenseDevTypes007,
    "ADDRESS MARK NOT FOUND FOR DATA FIELD"
  },
  {
    0x14, 0x00,
    SenseDevTypes028,
    "RECORDED ENTITY NOT FOUND"
  },
  {
    0x14, 0x01,
    SenseDevTypes029,
    "RECORD NOT FOUND"
  },
  {
    0x14, 0x02,
    SenseDevTypes002,
    "FILEMARK OR SETMARK NOT FOUND"
  },
  {
    0x14, 0x03,
    SenseDevTypes002,
    "END-OF-DATA NOT FOUND"
  },
  {
    0x14, 0x04,
    SenseDevTypes002,
    "BLOCK SEQUENCE ERROR"
  },
  {
    0x14, 0x05,
    SenseDevTypes030,
    "RECORD NOT FOUND - RECOMMEND REASSIGNMENT"
  },
  {
    0x14, 0x06,
    SenseDevTypes030,
    "RECORD NOT FOUND - DATA AUTO-REALLOCATED"
  },
  {
    0x15, 0x00,
    SenseDevTypes014,
    "RANDOM POSITIONING ERROR"
  },
  {
    0x15, 0x01,
    SenseDevTypes014,
    "MECHANICAL POSITIONING ERROR"
  },
  {
    0x15, 0x02,
    SenseDevTypes029,
    "POSITIONING ERROR DETECTED BY READ OF MEDIUM"
  },
  {
    0x16, 0x00,
    SenseDevTypes007,
    "DATA SYNCHRONIZATION MARK ERROR"
  },
  {
    0x16, 0x01,
    SenseDevTypes007,
    "DATA SYNC ERROR - DATA REWRITTEN"
  },
  {
    0x16, 0x02,
    SenseDevTypes007,
    "DATA SYNC ERROR - RECOMMEND REWRITE"
  },
  {
    0x16, 0x03,
    SenseDevTypes007,
    "DATA SYNC ERROR - DATA AUTO-REALLOCATED"
  },
  {
    0x16, 0x04,
    SenseDevTypes007,
    "DATA SYNC ERROR - RECOMMEND REASSIGNMENT"
  },
  {
    0x17, 0x00,
    SenseDevTypes023,
    "RECOVERED DATA WITH NO ERROR CORRECTION APPLIED"
  },
  {
    0x17, 0x01,
    SenseDevTypes023,
    "RECOVERED DATA WITH RETRIES"
  },
  {
    0x17, 0x02,
    SenseDevTypes029,
    "RECOVERED DATA WITH POSITIVE HEAD OFFSET"
  },
  {
    0x17, 0x03,
    SenseDevTypes029,
    "RECOVERED DATA WITH NEGATIVE HEAD OFFSET"
  },
  {
    0x17, 0x04,
    SenseDevTypes025,
    "RECOVERED DATA WITH RETRIES AND/OR CIRC APPLIED"
  },
  {
    0x17, 0x05,
    SenseDevTypes031,
    "RECOVERED DATA USING PREVIOUS SECTOR ID"
  },
  {
    0x17, 0x06,
    SenseDevTypes007,
    "RECOVERED DATA WITHOUT ECC - DATA AUTO-REALLOCATED"
  },
  {
    0x17, 0x07,
    SenseDevTypes031,
    "RECOVERED DATA WITHOUT ECC - RECOMMEND REASSIGNMENT"
  },
  {
    0x17, 0x08,
    SenseDevTypes031,
    "RECOVERED DATA WITHOUT ECC - RECOMMEND REWRITE"
  },
  {
    0x17, 0x09,
    SenseDevTypes031,
    "RECOVERED DATA WITHOUT ECC - DATA REWRITTEN"
  },
  {
    0x18, 0x00,
    SenseDevTypes029,
    "RECOVERED DATA WITH ERROR CORRECTION APPLIED"
  },
  {
    0x18, 0x01,
    SenseDevTypes031,
    "RECOVERED DATA WITH ERROR CORR. & RETRIES APPLIED"
  },
  {
    0x18, 0x02,
    SenseDevTypes031,
    "RECOVERED DATA - DATA AUTO-REALLOCATED"
  },
  {
    0x18, 0x03,
    SenseDevTypes005,
    "RECOVERED DATA WITH CIRC"
  },
  {
    0x18, 0x04,
    SenseDevTypes005,
    "RECOVERED DATA WITH L-EC"
  },
  {
    0x18, 0x05,
    SenseDevTypes031,
    "RECOVERED DATA - RECOMMEND REASSIGNMENT"
  },
  {
    0x18, 0x06,
    SenseDevTypes031,
    "RECOVERED DATA - RECOMMEND REWRITE"
  },
  {
    0x18, 0x07,
    SenseDevTypes007,
    "RECOVERED DATA WITH ECC - DATA REWRITTEN"
  },
  {
    0x19, 0x00,
    SenseDevTypes032,
    "DEFECT LIST ERROR"
  },
  {
    0x19, 0x01,
    SenseDevTypes032,
    "DEFECT LIST NOT AVAILABLE"
  },
  {
    0x19, 0x02,
    SenseDevTypes032,
    "DEFECT LIST ERROR IN PRIMARY LIST"
  },
  {
    0x19, 0x03,
    SenseDevTypes032,
    "DEFECT LIST ERROR IN GROWN LIST"
  },
  {
    0x1A, 0x00,
    SenseDevTypes001,
    "PARAMETER LIST LENGTH ERROR"
  },
  {
    0x1B, 0x00,
    SenseDevTypes001,
    "SYNCHRONOUS DATA TRANSFER ERROR"
  },
  {
    0x1C, 0x00,
    SenseDevTypes033,
    "DEFECT LIST NOT FOUND"
  },
  {
    0x1C, 0x01,
    SenseDevTypes033,
    "PRIMARY DEFECT LIST NOT FOUND"
  },
  {
    0x1C, 0x02,
    SenseDevTypes033,
    "GROWN DEFECT LIST NOT FOUND"
  },
  {
    0x1D, 0x00,
    SenseDevTypes029,
    "MISCOMPARE DURING VERIFY OPERATION"
  },
  {
    0x1E, 0x00,
    SenseDevTypes007,
    "RECOVERED ID WITH ECC CORRECTION"
  },
  {
    0x1F, 0x00,
    SenseDevTypes032,
    "PARTIAL DEFECT LIST TRANSFER"
  },
  {
    0x20, 0x00,
    SenseDevTypes001,
    "INVALID COMMAND OPERATION CODE"
  },
  {
    0x20, 0x01,
    SenseDevTypes012,
    "access controls code 1 (99-314) [proposed]"
  },
  {
    0x20, 0x02,
    SenseDevTypes012,
    "access controls code 2 (99-314) [proposed]"
  },
  {
    0x20, 0x03,
    SenseDevTypes012,
    "access controls code 3 (99-314) [proposed]"
  },
  {
    0x21, 0x00,
    SenseDevTypes034,
    "LOGICAL BLOCK ADDRESS OUT OF RANGE"
  },
  {
    0x21, 0x01,
    SenseDevTypes034,
    "INVALID ELEMENT ADDRESS"
  },
  {
    0x22, 0x00,
    SenseDevTypes035,
    "ILLEGAL FUNCTION (USE 20 00, 24 00, OR 26 00)"
  },
  {
    0x24, 0x00,
    SenseDevTypes001,
    "INVALID FIELD IN CDB"
  },
  {
    0x24, 0x01,
    SenseDevTypes001,
    "CDB DECRYPTION ERROR"
  },
  {
    0x25, 0x00,
    SenseDevTypes001,
    "LOGICAL UNIT NOT SUPPORTED"
  },
  {
    0x26, 0x00,
    SenseDevTypes001,
    "INVALID FIELD IN PARAMETER LIST"
  },
  {
    0x26, 0x01,
    SenseDevTypes001,
    "PARAMETER NOT SUPPORTED"
  },
  {
    0x26, 0x02,
    SenseDevTypes001,
    "PARAMETER VALUE INVALID"
  },
  {
    0x26, 0x03,
    SenseDevTypes036,
    "THRESHOLD PARAMETERS NOT SUPPORTED"
  },
  {
    0x26, 0x04,
    SenseDevTypes001,
    "INVALID RELEASE OF PERSISTENT RESERVATION"
  },
  {
    0x26, 0x05,
    SenseDevTypes037,
    "DATA DECRYPTION ERROR"
  },
  {
    0x26, 0x06,
    SenseDevTypes016,
    "TOO MANY TARGET DESCRIPTORS"
  },
  {
    0x26, 0x07,
    SenseDevTypes016,
    "UNSUPPORTED TARGET DESCRIPTOR TYPE CODE"
  },
  {
    0x26, 0x08,
    SenseDevTypes016,
    "TOO MANY SEGMENT DESCRIPTORS"
  },
  {
    0x26, 0x09,
    SenseDevTypes016,
    "UNSUPPORTED SEGMENT DESCRIPTOR TYPE CODE"
  },
  {
    0x26, 0x0A,
    SenseDevTypes016,
    "UNEXPECTED INEXACT SEGMENT"
  },
  {
    0x26, 0x0B,
    SenseDevTypes016,
    "INLINE DATA LENGTH EXCEEDED"
  },
  {
    0x26, 0x0C,
    SenseDevTypes016,
    "INVALID OPERATION FOR COPY SOURCE OR DESTINATION"
  },
  {
    0x26, 0x0D,
    SenseDevTypes016,
    "COPY SEGMENT GRANULARITY VIOLATION"
  },
  {
    0x27, 0x00,
    SenseDevTypes029,
    "WRITE PROTECTED"
  },
  {
    0x27, 0x01,
    SenseDevTypes029,
    "HARDWARE WRITE PROTECTED"
  },
  {
    0x27, 0x02,
    SenseDevTypes029,
    "LOGICAL UNIT SOFTWARE WRITE PROTECTED"
  },
  {
    0x27, 0x03,
    SenseDevTypes038,
    "ASSOCIATED WRITE PROTECT"
  },
  {
    0x27, 0x04,
    SenseDevTypes038,
    "PERSISTENT WRITE PROTECT"
  },
  {
    0x27, 0x05,
    SenseDevTypes038,
    "PERMANENT WRITE PROTECT"
  },
  {
    0x28, 0x00,
    SenseDevTypes001,
    "NOT READY TO READY CHANGE, MEDIUM MAY HAVE CHANGED"
  },
  {
    0x28, 0x01,
    SenseDevTypes039,
    "IMPORT OR EXPORT ELEMENT ACCESSED"
  },
  {
    0x29, 0x00,
    SenseDevTypes001,
    "POWER ON, RESET, OR BUS DEVICE RESET OCCURRED"
  },
  {
    0x29, 0x01,
    SenseDevTypes001,
    "POWER ON OCCURRED"
  },
  {
    0x29, 0x02,
    SenseDevTypes001,
    "SCSI BUS RESET OCCURRED"
  },
  {
    0x29, 0x03,
    SenseDevTypes001,
    "BUS DEVICE RESET FUNCTION OCCURRED"
  },
  {
    0x29, 0x04,
    SenseDevTypes001,
    "DEVICE INTERNAL RESET"
  },
  {
    0x29, 0x05,
    SenseDevTypes001,
    "TRANSCEIVER MODE CHANGED TO SINGLE-ENDED"
  },
  {
    0x29, 0x06,
    SenseDevTypes001,
    "TRANSCEIVER MODE CHANGED TO LVD"
  },
  {
    0x2A, 0x00,
    SenseDevTypes013,
    "PARAMETERS CHANGED"
  },
  {
    0x2A, 0x01,
    SenseDevTypes013,
    "MODE PARAMETERS CHANGED"
  },
  {
    0x2A, 0x02,
    SenseDevTypes040,
    "LOG PARAMETERS CHANGED"
  },
  {
    0x2A, 0x03,
    SenseDevTypes036,
    "RESERVATIONS PREEMPTED"
  },
  {
    0x2A, 0x04,
    SenseDevTypes041,
    "RESERVATIONS RELEASED"
  },
  {
    0x2A, 0x05,
    SenseDevTypes041,
    "REGISTRATIONS PREEMPTED"
  },
  {
    0x2B, 0x00,
    SenseDevTypes016,
    "COPY CANNOT EXECUTE SINCE HOST CANNOT DISCONNECT"
  },
  {
    0x2C, 0x00,
    SenseDevTypes001,
    "COMMAND SEQUENCE ERROR"
  },
  {
    0x2C, 0x01,
    SenseDevTypes042,
    "TOO MANY WINDOWS SPECIFIED"
  },
  {
    0x2C, 0x02,
    SenseDevTypes042,
    "INVALID COMBINATION OF WINDOWS SPECIFIED"
  },
  {
    0x2C, 0x03,
    SenseDevTypes005,
    "CURRENT PROGRAM AREA IS NOT EMPTY"
  },
  {
    0x2C, 0x04,
    SenseDevTypes005,
    "CURRENT PROGRAM AREA IS EMPTY"
  },
  {
    0x2C, 0x05,
    SenseDevTypes043,
    "ILLEGAL POWER CONDITION REQUEST"
  },
  {
    0x2D, 0x00,
    SenseDevTypes002,
    "OVERWRITE ERROR ON UPDATE IN PLACE"
  },
  {
    0x2E, 0x00,
    SenseDevTypes044,
    "ERROR DETECTED BY THIRD PARTY TEMPORARY INITIATOR"
  },
  {
    0x2E, 0x01,
    SenseDevTypes044,
    "THIRD PARTY DEVICE FAILURE"
  },
  {
    0x2E, 0x02,
    SenseDevTypes044,
    "COPY TARGET DEVICE NOT REACHABLE"
  },
  {
    0x2E, 0x03,
    SenseDevTypes044,
    "INCORRECT COPY TARGET DEVICE TYPE"
  },
  {
    0x2E, 0x04,
    SenseDevTypes044,
    "COPY TARGET DEVICE DATA UNDERRUN"
  },
  {
    0x2E, 0x05,
    SenseDevTypes044,
    "COPY TARGET DEVICE DATA OVERRUN"
  },
  {
    0x2F, 0x00,
    SenseDevTypes001,
    "COMMANDS CLEARED BY ANOTHER INITIATOR"
  },
  {
    0x30, 0x00,
    SenseDevTypes034,
    "INCOMPATIBLE MEDIUM INSTALLED"
  },
  {
    0x30, 0x01,
    SenseDevTypes029,
    "CANNOT READ MEDIUM - UNKNOWN FORMAT"
  },
  {
    0x30, 0x02,
    SenseDevTypes029,
    "CANNOT READ MEDIUM - INCOMPATIBLE FORMAT"
  },
  {
    0x30, 0x03,
    SenseDevTypes045,
    "CLEANING CARTRIDGE INSTALLED"
  },
  {
    0x30, 0x04,
    SenseDevTypes029,
    "CANNOT WRITE MEDIUM - UNKNOWN FORMAT"
  },
  {
    0x30, 0x05,
    SenseDevTypes029,
    "CANNOT WRITE MEDIUM - INCOMPATIBLE FORMAT"
  },
  {
    0x30, 0x06,
    SenseDevTypes017,
    "CANNOT FORMAT MEDIUM - INCOMPATIBLE MEDIUM"
  },
  {
    0x30, 0x07,
    SenseDevTypes006,
    "CLEANING FAILURE"
  },
  {
    0x30, 0x08,
    SenseDevTypes005,
    "CANNOT WRITE - APPLICATION CODE MISMATCH"
  },
  {
    0x30, 0x09,
    SenseDevTypes005,
    "CURRENT SESSION NOT FIXATED FOR APPEND"
  },
  {
    0x31, 0x00,
    SenseDevTypes029,
    "MEDIUM FORMAT CORRUPTED"
  },
  {
    0x31, 0x01,
    SenseDevTypes046,
    "FORMAT COMMAND FAILED"
  },
  {
    0x32, 0x00,
    SenseDevTypes007,
    "NO DEFECT SPARE LOCATION AVAILABLE"
  },
  {
    0x32, 0x01,
    SenseDevTypes007,
    "DEFECT LIST UPDATE FAILURE"
  },
  {
    0x33, 0x00,
    SenseDevTypes002,
    "TAPE LENGTH ERROR"
  },
  {
    0x34, 0x00,
    SenseDevTypes001,
    "ENCLOSURE FAILURE"
  },
  {
    0x35, 0x00,
    SenseDevTypes001,
    "ENCLOSURE SERVICES FAILURE"
  },
  {
    0x35, 0x01,
    SenseDevTypes001,
    "UNSUPPORTED ENCLOSURE FUNCTION"
  },
  {
    0x35, 0x02,
    SenseDevTypes001,
    "ENCLOSURE SERVICES UNAVAILABLE"
  },
  {
    0x35, 0x03,
    SenseDevTypes001,
    "ENCLOSURE SERVICES TRANSFER FAILURE"
  },
  {
    0x35, 0x04,
    SenseDevTypes001,
    "ENCLOSURE SERVICES TRANSFER REFUSED"
  },
  {
    0x36, 0x00,
    SenseDevTypes047,
    "RIBBON, INK, OR TONER FAILURE"
  },
  {
    0x37, 0x00,
    SenseDevTypes013,
    "ROUNDED PARAMETER"
  },
  {
    0x38, 0x00,
    SenseDevTypes043,
    "EVENT STATUS NOTIFICATION"
  },
  {
    0x38, 0x02,
    SenseDevTypes043,
    "ESN - POWER MANAGEMENT CLASS EVENT"
  },
  {
    0x38, 0x04,
    SenseDevTypes043,
    "ESN - MEDIA CLASS EVENT"
  },
  {
    0x38, 0x06,
    SenseDevTypes043,
    "ESN - DEVICE BUSY CLASS EVENT"
  },
  {
    0x39, 0x00,
    SenseDevTypes040,
    "SAVING PARAMETERS NOT SUPPORTED"
  },
  {
    0x3A, 0x00,
    SenseDevTypes014,
    "MEDIUM NOT PRESENT"
  },
  {
    0x3A, 0x01,
    SenseDevTypes034,
    "MEDIUM NOT PRESENT - TRAY CLOSED"
  },
  {
    0x3A, 0x02,
    SenseDevTypes034,
    "MEDIUM NOT PRESENT - TRAY OPEN"
  },
  {
    0x3A, 0x03,
    SenseDevTypes039,
    "MEDIUM NOT PRESENT - LOADABLE"
  },
  {
    0x3A, 0x04,
    SenseDevTypes039,
    "MEDIUM NOT PRESENT - MEDIUM AUXILIARY MEMORY ACCESSIBLE"
  },
  {
    0x3B, 0x00,
    SenseDevTypes048,
    "SEQUENTIAL POSITIONING ERROR"
  },
  {
    0x3B, 0x01,
    SenseDevTypes002,
    "TAPE POSITION ERROR AT BEGINNING-OF-MEDIUM"
  },
  {
    0x3B, 0x02,
    SenseDevTypes002,
    "TAPE POSITION ERROR AT END-OF-MEDIUM"
  },
  {
    0x3B, 0x03,
    SenseDevTypes047,
    "TAPE OR ELECTRONIC VERTICAL FORMS UNIT NOT READY"
  },
  {
    0x3B, 0x04,
    SenseDevTypes047,
    "SLEW FAILURE"
  },
  {
    0x3B, 0x05,
    SenseDevTypes047,
    "PAPER JAM"
  },
  {
    0x3B, 0x06,
    SenseDevTypes047,
    "FAILED TO SENSE TOP-OF-FORM"
  },
  {
    0x3B, 0x07,
    SenseDevTypes047,
    "FAILED TO SENSE BOTTOM-OF-FORM"
  },
  {
    0x3B, 0x08,
    SenseDevTypes002,
    "REPOSITION ERROR"
  },
  {
    0x3B, 0x09,
    SenseDevTypes042,
    "READ PAST END OF MEDIUM"
  },
  {
    0x3B, 0x0A,
    SenseDevTypes042,
    "READ PAST BEGINNING OF MEDIUM"
  },
  {
    0x3B, 0x0B,
    SenseDevTypes042,
    "POSITION PAST END OF MEDIUM"
  },
  {
    0x3B, 0x0C,
    SenseDevTypes003,
    "POSITION PAST BEGINNING OF MEDIUM"
  },
  {
    0x3B, 0x0D,
    SenseDevTypes034,
    "MEDIUM DESTINATION ELEMENT FULL"
  },
  {
    0x3B, 0x0E,
    SenseDevTypes034,
    "MEDIUM SOURCE ELEMENT EMPTY"
  },
  {
    0x3B, 0x0F,
    SenseDevTypes005,
    "END OF MEDIUM REACHED"
  },
  {
    0x3B, 0x11,
    SenseDevTypes034,
    "MEDIUM MAGAZINE NOT ACCESSIBLE"
  },
  {
    0x3B, 0x12,
    SenseDevTypes034,
    "MEDIUM MAGAZINE REMOVED"
  },
  {
    0x3B, 0x13,
    SenseDevTypes034,
    "MEDIUM MAGAZINE INSERTED"
  },
  {
    0x3B, 0x14,
    SenseDevTypes034,
    "MEDIUM MAGAZINE LOCKED"
  },
  {
    0x3B, 0x15,
    SenseDevTypes034,
    "MEDIUM MAGAZINE UNLOCKED"
  },
  {
    0x3B, 0x16,
    SenseDevTypes005,
    "MECHANICAL POSITIONING OR CHANGER ERROR"
  },
  {
    0x3D, 0x00,
    SenseDevTypes036,
    "INVALID BITS IN IDENTIFY MESSAGE"
  },
  {
    0x3E, 0x00,
    SenseDevTypes001,
    "LOGICAL UNIT HAS NOT SELF-CONFIGURED YET"
  },
  {
    0x3E, 0x01,
    SenseDevTypes001,
    "LOGICAL UNIT FAILURE"
  },
  {
    0x3E, 0x02,
    SenseDevTypes001,
    "TIMEOUT ON LOGICAL UNIT"
  },
  {
    0x3E, 0x03,
    SenseDevTypes001,
    "LOGICAL UNIT FAILED SELF-TEST"
  },
  {
    0x3E, 0x04,
    SenseDevTypes001,
    "LOGICAL UNIT UNABLE TO UPDATE SELF-TEST LOG"
  },
  {
    0x3F, 0x00,
    SenseDevTypes001,
    "TARGET OPERATING CONDITIONS HAVE CHANGED"
  },
  {
    0x3F, 0x01,
    SenseDevTypes001,
    "MICROCODE HAS BEEN CHANGED"
  },
  {
    0x3F, 0x02,
    SenseDevTypes049,
    "CHANGED OPERATING DEFINITION"
  },
  {
    0x3F, 0x03,
    SenseDevTypes001,
    "INQUIRY DATA HAS CHANGED"
  },
  {
    0x3F, 0x04,
    SenseDevTypes050,
    "COMPONENT DEVICE ATTACHED"
  },
  {
    0x3F, 0x05,
    SenseDevTypes050,
    "DEVICE IDENTIFIER CHANGED"
  },
  {
    0x3F, 0x06,
    SenseDevTypes051,
    "REDUNDANCY GROUP CREATED OR MODIFIED"
  },
  {
    0x3F, 0x07,
    SenseDevTypes051,
    "REDUNDANCY GROUP DELETED"
  },
  {
    0x3F, 0x08,
    SenseDevTypes051,
    "SPARE CREATED OR MODIFIED"
  },
  {
    0x3F, 0x09,
    SenseDevTypes051,
    "SPARE DELETED"
  },
  {
    0x3F, 0x0A,
    SenseDevTypes050,
    "VOLUME SET CREATED OR MODIFIED"
  },
  {
    0x3F, 0x0B,
    SenseDevTypes050,
    "VOLUME SET DELETED"
  },
  {
    0x3F, 0x0C,
    SenseDevTypes050,
    "VOLUME SET DEASSIGNED"
  },
  {
    0x3F, 0x0D,
    SenseDevTypes050,
    "VOLUME SET REASSIGNED"
  },
  {
    0x3F, 0x0E,
    SenseDevTypes041,
    "REPORTED LUNS DATA HAS CHANGED"
  },
  {
    0x3F, 0x0F,
    SenseDevTypes001,
    "ECHO BUFFER OVERWRITTEN"
  },
  {
    0x3F, 0x10,
    SenseDevTypes039,
    "MEDIUM LOADABLE"
  },
  {
    0x3F, 0x11,
    SenseDevTypes039,
    "MEDIUM AUXILIARY MEMORY ACCESSIBLE"
  },
  {
    0x40, 0x00,
    SenseDevTypes035,
    "RAM FAILURE (SHOULD USE 40 NN)"
  },
  {
    0x40, 0xFF,
    SenseDevTypes001,
    "DIAGNOSTIC FAILURE ON COMPONENT NN (80H-FFH)"
  },
  {
    0x41, 0x00,
    SenseDevTypes035,
    "DATA PATH FAILURE (SHOULD USE 40 NN)"
  },
  {
    0x42, 0x00,
    SenseDevTypes035,
    "POWER-ON OR SELF-TEST FAILURE (SHOULD USE 40 NN)"
  },
  {
    0x43, 0x00,
    SenseDevTypes001,
    "MESSAGE ERROR"
  },
  {
    0x44, 0x00,
    SenseDevTypes001,
    "INTERNAL TARGET FAILURE"
  },
  {
    0x45, 0x00,
    SenseDevTypes001,
    "SELECT OR RESELECT FAILURE"
  },
  {
    0x46, 0x00,
    SenseDevTypes049,
    "UNSUCCESSFUL SOFT RESET"
  },
  {
    0x47, 0x00,
    SenseDevTypes001,
    "SCSI PARITY ERROR"
  },
  {
    0x47, 0x01,
    SenseDevTypes001,
    "DATA PHASE CRC ERROR DETECTED"
  },
  {
    0x47, 0x02,
    SenseDevTypes001,
    "SCSI PARITY ERROR DETECTED DURING ST DATA PHASE"
  },
  {
    0x47, 0x03,
    SenseDevTypes001,
    "INFORMATION UNIT CRC ERROR DETECTED"
  },
  {
    0x47, 0x04,
    SenseDevTypes001,
    "ASYNCHRONOUS INFORMATION PROTECTION ERROR DETECTED"
  },
  {
    0x48, 0x00,
    SenseDevTypes001,
    "INITIATOR DETECTED ERROR MESSAGE RECEIVED"
  },
  {
    0x49, 0x00,
    SenseDevTypes001,
    "INVALID MESSAGE ERROR"
  },
  {
    0x4A, 0x00,
    SenseDevTypes001,
    "COMMAND PHASE ERROR"
  },
  {
    0x4B, 0x00,
    SenseDevTypes001,
    "DATA PHASE ERROR"
  },
  {
    0x4C, 0x00,
    SenseDevTypes001,
    "LOGICAL UNIT FAILED SELF-CONFIGURATION"
  },
  {
    0x4D, 0xFF,
    SenseDevTypes001,
    "TAGGED OVERLAPPED COMMANDS (NN = QUEUE TAG)"
  },
  {
    0x4E, 0x00,
    SenseDevTypes001,
    "OVERLAPPED COMMANDS ATTEMPTED"
  },
  {
    0x50, 0x00,
    SenseDevTypes002,
    "WRITE APPEND ERROR"
  },
  {
    0x50, 0x01,
    SenseDevTypes002,
    "WRITE APPEND POSITION ERROR"
  },
  {
    0x50, 0x02,
    SenseDevTypes002,
    "POSITION ERROR RELATED TO TIMING"
  },
  {
    0x51, 0x00,
    SenseDevTypes052,
    "ERASE FAILURE"
  },
  {
    0x52, 0x00,
    SenseDevTypes002,
    "CARTRIDGE FAULT"
  },
  {
    0x53, 0x00,
    SenseDevTypes014,
    "MEDIA LOAD OR EJECT FAILED"
  },
  {
    0x53, 0x01,
    SenseDevTypes002,
    "UNLOAD TAPE FAILURE"
  },
  {
    0x53, 0x02,
    SenseDevTypes034,
    "MEDIUM REMOVAL PREVENTED"
  },
  {
    0x54, 0x00,
    SenseDevTypes053,
    "SCSI TO HOST SYSTEM INTERFACE FAILURE"
  },
  {
    0x55, 0x00,
    SenseDevTypes053,
    "SYSTEM RESOURCE FAILURE"
  },
  {
    0x55, 0x01,
    SenseDevTypes033,
    "SYSTEM BUFFER FULL"
  },
  {
    0x55, 0x02,
    SenseDevTypes054,
    "INSUFFICIENT RESERVATION RESOURCES"
  },
  {
    0x55, 0x03,
    SenseDevTypes041,
    "INSUFFICIENT RESOURCES"
  },
  {
    0x55, 0x04,
    SenseDevTypes055,
    "INSUFFICIENT REGISTRATION RESOURCES"
  },
  {
    0x55, 0x05,
    SenseDevTypes012,
    "access controls code 4 (99-314) [proposed]"
  },
  {
    0x55, 0x06,
    SenseDevTypes012,
    "auxiliary memory code 1 (99-148) [proposed]"
  },
  {
    0x57, 0x00,
    SenseDevTypes005,
    "UNABLE TO RECOVER TABLE-OF-CONTENTS"
  },
  {
    0x58, 0x00,
    SenseDevTypes056,
    "GENERATION DOES NOT EXIST"
  },
  {
    0x59, 0x00,
    SenseDevTypes056,
    "UPDATED BLOCK READ"
  },
  {
    0x5A, 0x00,
    SenseDevTypes057,
    "OPERATOR REQUEST OR STATE CHANGE INPUT"
  },
  {
    0x5A, 0x01,
    SenseDevTypes034,
    "OPERATOR MEDIUM REMOVAL REQUEST"
  },
  {
    0x5A, 0x02,
    SenseDevTypes058,
    "OPERATOR SELECTED WRITE PROTECT"
  },
  {
    0x5A, 0x03,
    SenseDevTypes058,
    "OPERATOR SELECTED WRITE PERMIT"
  },
  {
    0x5B, 0x00,
    SenseDevTypes059,
    "LOG EXCEPTION"
  },
  {
    0x5B, 0x01,
    SenseDevTypes059,
    "THRESHOLD CONDITION MET"
  },
  {
    0x5B, 0x02,
    SenseDevTypes059,
    "LOG COUNTER AT MAXIMUM"
  },
  {
    0x5B, 0x03,
    SenseDevTypes059,
    "LOG LIST CODES EXHAUSTED"
  },
  {
    0x5C, 0x00,
    SenseDevTypes060,
    "RPL STATUS CHANGE"
  },
  {
    0x5C, 0x01,
    SenseDevTypes060,
    "SPINDLES SYNCHRONIZED"
  },
  {
    0x5C, 0x02,
    SenseDevTypes060,
    "SPINDLES NOT SYNCHRONIZED"
  },
  {
    0x5D, 0x00,
    SenseDevTypes001,
    "FAILURE PREDICTION THRESHOLD EXCEEDED"
  },
  {
    0x5D, 0x01,
    SenseDevTypes061,
    "MEDIA FAILURE PREDICTION THRESHOLD EXCEEDED"
  },
  {
    0x5D, 0x02,
    SenseDevTypes005,
    "LOGICAL UNIT FAILURE PREDICTION THRESHOLD EXCEEDED"
  },
  {
    0x5D, 0x10,
    SenseDevTypes062,
    "HARDWARE IMPENDING FAILURE GENERAL HARD DRIVE FAILURE"
  },
  {
    0x5D, 0x11,
    SenseDevTypes062,
    "HARDWARE IMPENDING FAILURE DRIVE ERROR RATE TOO HIGH"
  },
  {
    0x5D, 0x12,
    SenseDevTypes062,
    "HARDWARE IMPENDING FAILURE DATA ERROR RATE TOO HIGH"
  },
  {
    0x5D, 0x13,
    SenseDevTypes062,
    "HARDWARE IMPENDING FAILURE SEEK ERROR RATE TOO HIGH"
  },
  {
    0x5D, 0x14,
    SenseDevTypes062,
    "HARDWARE IMPENDING FAILURE TOO MANY BLOCK REASSIGNS"
  },
  {
    0x5D, 0x15,
    SenseDevTypes062,
    "HARDWARE IMPENDING FAILURE ACCESS TIMES TOO HIGH"
  },
  {
    0x5D, 0x16,
    SenseDevTypes062,
    "HARDWARE IMPENDING FAILURE START UNIT TIMES TOO HIGH"
  },
  {
    0x5D, 0x17,
    SenseDevTypes062,
    "HARDWARE IMPENDING FAILURE CHANNEL PARAMETRICS"
  },
  {
    0x5D, 0x18,
    SenseDevTypes062,
    "HARDWARE IMPENDING FAILURE CONTROLLER DETECTED"
  },
  {
    0x5D, 0x19,
    SenseDevTypes062,
    "HARDWARE IMPENDING FAILURE THROUGHPUT PERFORMANCE"
  },
  {
    0x5D, 0x1A,
    SenseDevTypes062,
    "HARDWARE IMPENDING FAILURE SEEK TIME PERFORMANCE"
  },
  {
    0x5D, 0x1B,
    SenseDevTypes062,
    "HARDWARE IMPENDING FAILURE SPIN-UP RETRY COUNT"
  },
  {
    0x5D, 0x1C,
    SenseDevTypes062,
    "HARDWARE IMPENDING FAILURE DRIVE CALIBRATION RETRY COUNT"
  },
  {
    0x5D, 0x20,
    SenseDevTypes062,
    "CONTROLLER IMPENDING FAILURE GENERAL HARD DRIVE FAILURE"
  },
  {
    0x5D, 0x21,
    SenseDevTypes062,
    "CONTROLLER IMPENDING FAILURE DRIVE ERROR RATE TOO HIGH"
  },
  {
    0x5D, 0x22,
    SenseDevTypes062,
    "CONTROLLER IMPENDING FAILURE DATA ERROR RATE TOO HIGH"
  },
  {
    0x5D, 0x23,
    SenseDevTypes062,
    "CONTROLLER IMPENDING FAILURE SEEK ERROR RATE TOO HIGH"
  },
  {
    0x5D, 0x24,
    SenseDevTypes062,
    "CONTROLLER IMPENDING FAILURE TOO MANY BLOCK REASSIGNS"
  },
  {
    0x5D, 0x25,
    SenseDevTypes062,
    "CONTROLLER IMPENDING FAILURE ACCESS TIMES TOO HIGH"
  },
  {
    0x5D, 0x26,
    SenseDevTypes062,
    "CONTROLLER IMPENDING FAILURE START UNIT TIMES TOO HIGH"
  },
  {
    0x5D, 0x27,
    SenseDevTypes062,
    "CONTROLLER IMPENDING FAILURE CHANNEL PARAMETRICS"
  },
  {
    0x5D, 0x28,
    SenseDevTypes062,
    "CONTROLLER IMPENDING FAILURE CONTROLLER DETECTED"
  },
  {
    0x5D, 0x29,
    SenseDevTypes062,
    "CONTROLLER IMPENDING FAILURE THROUGHPUT PERFORMANCE"
  },
  {
    0x5D, 0x2A,
    SenseDevTypes062,
    "CONTROLLER IMPENDING FAILURE SEEK TIME PERFORMANCE"
  },
  {
    0x5D, 0x2B,
    SenseDevTypes062,
    "CONTROLLER IMPENDING FAILURE SPIN-UP RETRY COUNT"
  },
  {
    0x5D, 0x2C,
    SenseDevTypes062,
    "CONTROLLER IMPENDING FAILURE DRIVE CALIBRATION RETRY COUNT"
  },
  {
    0x5D, 0x30,
    SenseDevTypes062,
    "DATA CHANNEL IMPENDING FAILURE GENERAL HARD DRIVE FAILURE"
  },
  {
    0x5D, 0x31,
    SenseDevTypes062,
    "DATA CHANNEL IMPENDING FAILURE DRIVE ERROR RATE TOO HIGH"
  },
  {
    0x5D, 0x32,
    SenseDevTypes062,
    "DATA CHANNEL IMPENDING FAILURE DATA ERROR RATE TOO HIGH"
  },
  {
    0x5D, 0x33,
    SenseDevTypes062,
    "DATA CHANNEL IMPENDING FAILURE SEEK ERROR RATE TOO HIGH"
  },
  {
    0x5D, 0x34,
    SenseDevTypes062,
    "DATA CHANNEL IMPENDING FAILURE TOO MANY BLOCK REASSIGNS"
  },
  {
    0x5D, 0x35,
    SenseDevTypes062,
    "DATA CHANNEL IMPENDING FAILURE ACCESS TIMES TOO HIGH"
  },
  {
    0x5D, 0x36,
    SenseDevTypes062,
    "DATA CHANNEL IMPENDING FAILURE START UNIT TIMES TOO HIGH"
  },
  {
    0x5D, 0x37,
    SenseDevTypes062,
    "DATA CHANNEL IMPENDING FAILURE CHANNEL PARAMETRICS"
  },
  {
    0x5D, 0x38,
    SenseDevTypes062,
    "DATA CHANNEL IMPENDING FAILURE CONTROLLER DETECTED"
  },
  {
    0x5D, 0x39,
    SenseDevTypes062,
    "DATA CHANNEL IMPENDING FAILURE THROUGHPUT PERFORMANCE"
  },
  {
    0x5D, 0x3A,
    SenseDevTypes062,
    "DATA CHANNEL IMPENDING FAILURE SEEK TIME PERFORMANCE"
  },
  {
    0x5D, 0x3B,
    SenseDevTypes062,
    "DATA CHANNEL IMPENDING FAILURE SPIN-UP RETRY COUNT"
  },
  {
    0x5D, 0x3C,
    SenseDevTypes062,
    "DATA CHANNEL IMPENDING FAILURE DRIVE CALIBRATION RETRY COUNT"
  },
  {
    0x5D, 0x40,
    SenseDevTypes062,
    "SERVO IMPENDING FAILURE GENERAL HARD DRIVE FAILURE"
  },
  {
    0x5D, 0x41,
    SenseDevTypes062,
    "SERVO IMPENDING FAILURE DRIVE ERROR RATE TOO HIGH"
  },
  {
    0x5D, 0x42,
    SenseDevTypes062,
    "SERVO IMPENDING FAILURE DATA ERROR RATE TOO HIGH"
  },
  {
    0x5D, 0x43,
    SenseDevTypes062,
    "SERVO IMPENDING FAILURE SEEK ERROR RATE TOO HIGH"
  },
  {
    0x5D, 0x44,
    SenseDevTypes062,
    "SERVO IMPENDING FAILURE TOO MANY BLOCK REASSIGNS"
  },
  {
    0x5D, 0x45,
    SenseDevTypes062,
    "SERVO IMPENDING FAILURE ACCESS TIMES TOO HIGH"
  },
  {
    0x5D, 0x46,
    SenseDevTypes062,
    "SERVO IMPENDING FAILURE START UNIT TIMES TOO HIGH"
  },
  {
    0x5D, 0x47,
    SenseDevTypes062,
    "SERVO IMPENDING FAILURE CHANNEL PARAMETRICS"
  },
  {
    0x5D, 0x48,
    SenseDevTypes062,
    "SERVO IMPENDING FAILURE CONTROLLER DETECTED"
  },
  {
    0x5D, 0x49,
    SenseDevTypes062,
    "SERVO IMPENDING FAILURE THROUGHPUT PERFORMANCE"
  },
  {
    0x5D, 0x4A,
    SenseDevTypes062,
    "SERVO IMPENDING FAILURE SEEK TIME PERFORMANCE"
  },
  {
    0x5D, 0x4B,
    SenseDevTypes062,
    "SERVO IMPENDING FAILURE SPIN-UP RETRY COUNT"
  },
  {
    0x5D, 0x4C,
    SenseDevTypes062,
    "SERVO IMPENDING FAILURE DRIVE CALIBRATION RETRY COUNT"
  },
  {
    0x5D, 0x50,
    SenseDevTypes062,
    "SPINDLE IMPENDING FAILURE GENERAL HARD DRIVE FAILURE"
  },
  {
    0x5D, 0x51,
    SenseDevTypes062,
    "SPINDLE IMPENDING FAILURE DRIVE ERROR RATE TOO HIGH"
  },
  {
    0x5D, 0x52,
    SenseDevTypes062,
    "SPINDLE IMPENDING FAILURE DATA ERROR RATE TOO HIGH"
  },
  {
    0x5D, 0x53,
    SenseDevTypes062,
    "SPINDLE IMPENDING FAILURE SEEK ERROR RATE TOO HIGH"
  },
  {
    0x5D, 0x54,
    SenseDevTypes062,
    "SPINDLE IMPENDING FAILURE TOO MANY BLOCK REASSIGNS"
  },
  {
    0x5D, 0x55,
    SenseDevTypes062,
    "SPINDLE IMPENDING FAILURE ACCESS TIMES TOO HIGH"
  },
  {
    0x5D, 0x56,
    SenseDevTypes062,
    "SPINDLE IMPENDING FAILURE START UNIT TIMES TOO HIGH"
  },
  {
    0x5D, 0x57,
    SenseDevTypes062,
    "SPINDLE IMPENDING FAILURE CHANNEL PARAMETRICS"
  },
  {
    0x5D, 0x58,
    SenseDevTypes062,
    "SPINDLE IMPENDING FAILURE CONTROLLER DETECTED"
  },
  {
    0x5D, 0x59,
    SenseDevTypes062,
    "SPINDLE IMPENDING FAILURE THROUGHPUT PERFORMANCE"
  },
  {
    0x5D, 0x5A,
    SenseDevTypes062,
    "SPINDLE IMPENDING FAILURE SEEK TIME PERFORMANCE"
  },
  {
    0x5D, 0x5B,
    SenseDevTypes062,
    "SPINDLE IMPENDING FAILURE SPIN-UP RETRY COUNT"
  },
  {
    0x5D, 0x5C,
    SenseDevTypes062,
    "SPINDLE IMPENDING FAILURE DRIVE CALIBRATION RETRY COUNT"
  },
  {
    0x5D, 0x60,
    SenseDevTypes062,
    "FIRMWARE IMPENDING FAILURE GENERAL HARD DRIVE FAILURE"
  },
  {
    0x5D, 0x61,
    SenseDevTypes062,
    "FIRMWARE IMPENDING FAILURE DRIVE ERROR RATE TOO HIGH"
  },
  {
    0x5D, 0x62,
    SenseDevTypes062,
    "FIRMWARE IMPENDING FAILURE DATA ERROR RATE TOO HIGH"
  },
  {
    0x5D, 0x63,
    SenseDevTypes062,
    "FIRMWARE IMPENDING FAILURE SEEK ERROR RATE TOO HIGH"
  },
  {
    0x5D, 0x64,
    SenseDevTypes062,
    "FIRMWARE IMPENDING FAILURE TOO MANY BLOCK REASSIGNS"
  },
  {
    0x5D, 0x65,
    SenseDevTypes062,
    "FIRMWARE IMPENDING FAILURE ACCESS TIMES TOO HIGH"
  },
  {
    0x5D, 0x66,
    SenseDevTypes062,
    "FIRMWARE IMPENDING FAILURE START UNIT TIMES TOO HIGH"
  },
  {
    0x5D, 0x67,
    SenseDevTypes062,
    "FIRMWARE IMPENDING FAILURE CHANNEL PARAMETRICS"
  },
  {
    0x5D, 0x68,
    SenseDevTypes062,
    "FIRMWARE IMPENDING FAILURE CONTROLLER DETECTED"
  },
  {
    0x5D, 0x69,
    SenseDevTypes062,
    "FIRMWARE IMPENDING FAILURE THROUGHPUT PERFORMANCE"
  },
  {
    0x5D, 0x6A,
    SenseDevTypes062,
    "FIRMWARE IMPENDING FAILURE SEEK TIME PERFORMANCE"
  },
  {
    0x5D, 0x6B,
    SenseDevTypes062,
    "FIRMWARE IMPENDING FAILURE SPIN-UP RETRY COUNT"
  },
  {
    0x5D, 0x6C,
    SenseDevTypes062,
    "FIRMWARE IMPENDING FAILURE DRIVE CALIBRATION RETRY COUNT"
  },
  {
    0x5D, 0xFF,
    SenseDevTypes001,
    "FAILURE PREDICTION THRESHOLD EXCEEDED (FALSE)"
  },
  {
    0x5E, 0x00,
    SenseDevTypes044,
    "LOW POWER CONDITION ON"
  },
  {
    0x5E, 0x01,
    SenseDevTypes044,
    "IDLE CONDITION ACTIVATED BY TIMER"
  },
  {
    0x5E, 0x02,
    SenseDevTypes044,
    "STANDBY CONDITION ACTIVATED BY TIMER"
  },
  {
    0x5E, 0x03,
    SenseDevTypes044,
    "IDLE CONDITION ACTIVATED BY COMMAND"
  },
  {
    0x5E, 0x04,
    SenseDevTypes044,
    "STANDBY CONDITION ACTIVATED BY COMMAND"
  },
  {
    0x5E, 0x41,
    SenseDevTypes043,
    "POWER STATE CHANGE TO ACTIVE"
  },
  {
    0x5E, 0x42,
    SenseDevTypes043,
    "POWER STATE CHANGE TO IDLE"
  },
  {
    0x5E, 0x43,
    SenseDevTypes043,
    "POWER STATE CHANGE TO STANDBY"
  },
  {
    0x5E, 0x45,
    SenseDevTypes043,
    "POWER STATE CHANGE TO SLEEP"
  },
  {
    0x5E, 0x47,
    SenseDevTypes063,
    "POWER STATE CHANGE TO DEVICE CONTROL"
  },
  {
    0x60, 0x00,
    SenseDevTypes042,
    "LAMP FAILURE"
  },
  {
    0x61, 0x00,
    SenseDevTypes042,
    "VIDEO ACQUISITION ERROR"
  },
  {
    0x61, 0x01,
    SenseDevTypes042,
    "UNABLE TO ACQUIRE VIDEO"
  },
  {
    0x61, 0x02,
    SenseDevTypes042,
    "OUT OF FOCUS"
  },
  {
    0x62, 0x00,
    SenseDevTypes042,
    "SCAN HEAD POSITIONING ERROR"
  },
  {
    0x63, 0x00,
    SenseDevTypes005,
    "END OF USER AREA ENCOUNTERED ON THIS TRACK"
  },
  {
    0x63, 0x01,
    SenseDevTypes005,
    "PACKET DOES NOT FIT IN AVAILABLE SPACE"
  },
  {
    0x64, 0x00,
    SenseDevTypes005,
    "ILLEGAL MODE FOR THIS TRACK"
  },
  {
    0x64, 0x01,
    SenseDevTypes005,
    "INVALID PACKET SIZE"
  },
  {
    0x65, 0x00,
    SenseDevTypes001,
    "VOLTAGE FAULT"
  },
  {
    0x66, 0x00,
    SenseDevTypes042,
    "AUTOMATIC DOCUMENT FEEDER COVER UP"
  },
  {
    0x66, 0x01,
    SenseDevTypes042,
    "AUTOMATIC DOCUMENT FEEDER LIFT UP"
  },
  {
    0x66, 0x02,
    SenseDevTypes042,
    "DOCUMENT JAM IN AUTOMATIC DOCUMENT FEEDER"
  },
  {
    0x66, 0x03,
    SenseDevTypes042,
    "DOCUMENT MISS FEED AUTOMATIC IN DOCUMENT FEEDER"
  },
  {
    0x67, 0x00,
    SenseDevTypes064,
    "CONFIGURATION FAILURE"
  },
  {
    0x67, 0x01,
    SenseDevTypes064,
    "CONFIGURATION OF INCAPABLE LOGICAL UNITS FAILED"
  },
  {
    0x67, 0x02,
    SenseDevTypes064,
    "ADD LOGICAL UNIT FAILED"
  },
  {
    0x67, 0x03,
    SenseDevTypes064,
    "MODIFICATION OF LOGICAL UNIT FAILED"
  },
  {
    0x67, 0x04,
    SenseDevTypes064,
    "EXCHANGE OF LOGICAL UNIT FAILED"
  },
  {
    0x67, 0x05,
    SenseDevTypes064,
    "REMOVE OF LOGICAL UNIT FAILED"
  },
  {
    0x67, 0x06,
    SenseDevTypes064,
    "ATTACHMENT OF LOGICAL UNIT FAILED"
  },
  {
    0x67, 0x07,
    SenseDevTypes064,
    "CREATION OF LOGICAL UNIT FAILED"
  },
  {
    0x67, 0x08,
    SenseDevTypes064,
    "ASSIGN FAILURE OCCURRED"
  },
  {
    0x67, 0x09,
    SenseDevTypes064,
    "MULTIPLY ASSIGNED LOGICAL UNIT"
  },
  {
    0x68, 0x00,
    SenseDevTypes064,
    "LOGICAL UNIT NOT CONFIGURED"
  },
  {
    0x69, 0x00,
    SenseDevTypes064,
    "DATA LOSS ON LOGICAL UNIT"
  },
  {
    0x69, 0x01,
    SenseDevTypes064,
    "MULTIPLE LOGICAL UNIT FAILURES"
  },
  {
    0x69, 0x02,
    SenseDevTypes064,
    "PARITY/DATA MISMATCH"
  },
  {
    0x6A, 0x00,
    SenseDevTypes064,
    "INFORMATIONAL, REFER TO LOG"
  },
  {
    0x6B, 0x00,
    SenseDevTypes064,
    "STATE CHANGE HAS OCCURRED"
  },
  {
    0x6B, 0x01,
    SenseDevTypes064,
    "REDUNDANCY LEVEL GOT BETTER"
  },
  {
    0x6B, 0x02,
    SenseDevTypes064,
    "REDUNDANCY LEVEL GOT WORSE"
  },
  {
    0x6C, 0x00,
    SenseDevTypes064,
    "REBUILD FAILURE OCCURRED"
  },
  {
    0x6D, 0x00,
    SenseDevTypes064,
    "RECALCULATE FAILURE OCCURRED"
  },
  {
    0x6E, 0x00,
    SenseDevTypes064,
    "COMMAND TO LOGICAL UNIT FAILED"
  },
  {
    0x6F, 0x00,
    SenseDevTypes005,
    "COPY PROTECTION KEY EXCHANGE FAILURE - AUTHENTICATION FAILURE"
  },
  {
    0x6F, 0x01,
    SenseDevTypes005,
    "COPY PROTECTION KEY EXCHANGE FAILURE - KEY NOT PRESENT"
  },
  {
    0x6F, 0x02,
    SenseDevTypes005,
    "COPY PROTECTION KEY EXCHANGE FAILURE - KEY NOT ESTABLISHED"
  },
  {
    0x6F, 0x03,
    SenseDevTypes005,
    "READ OF SCRAMBLED SECTOR WITHOUT AUTHENTICATION"
  },
  {
    0x6F, 0x04,
    SenseDevTypes005,
    "MEDIA REGION CODE IS MISMATCHED TO LOGICAL UNIT REGION"
  },
  {
    0x6F, 0x05,
    SenseDevTypes005,
    "DRIVE REGION MUST BE PERMANENT/REGION RESET COUNT ERROR"
  },
  {
    0x70, 0xFF,
    SenseDevTypes002,
    "DECOMPRESSION EXCEPTION SHORT ALGORITHM ID OF NN"
  },
  {
    0x71, 0x00,
    SenseDevTypes002,
    "DECOMPRESSION EXCEPTION LONG ALGORITHM ID"
  },
  {
    0x72, 0x00,
    SenseDevTypes005,
    "SESSION FIXATION ERROR"
  },
  {
    0x72, 0x01,
    SenseDevTypes005,
    "SESSION FIXATION ERROR WRITING LEAD-IN"
  },
  {
    0x72, 0x02,
    SenseDevTypes005,
    "SESSION FIXATION ERROR WRITING LEAD-OUT"
  },
  {
    0x72, 0x03,
    SenseDevTypes005,
    "SESSION FIXATION ERROR - INCOMPLETE TRACK IN SESSION"
  },
  {
    0x72, 0x04,
    SenseDevTypes005,
    "EMPTY OR PARTIALLY WRITTEN RESERVED TRACK"
  },
  {
    0x72, 0x05,
    SenseDevTypes005,
    "NO MORE TRACK RESERVATIONS ALLOWED"
  },
  {
    0x73, 0x00,
    SenseDevTypes005,
    "CD CONTROL ERROR"
  },
  {
    0x73, 0x01,
    SenseDevTypes005,
    "POWER CALIBRATION AREA ALMOST FULL"
  },
  {
    0x73, 0x02,
    SenseDevTypes005,
    "POWER CALIBRATION AREA IS FULL"
  },
  {
    0x73, 0x03,
    SenseDevTypes005,
    "POWER CALIBRATION AREA ERROR"
  },
  {
    0x73, 0x04,
    SenseDevTypes005,
    "PROGRAM MEMORY AREA UPDATE FAILURE"
  },
  {
    0x73, 0x05,
    SenseDevTypes005,
    "PROGRAM MEMORY AREA IS FULL"
  },
  {
    0x73, 0x06,
    SenseDevTypes005,
    "RMA/PMA IS FULL"
  },
};

static int ASCQ_TableSize = 463;


#endif
