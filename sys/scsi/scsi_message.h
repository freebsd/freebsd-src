/* Messages (1 byte) */		     /* I/T (M)andatory or (O)ptional */
#define MSG_CMDCOMPLETE		0x00 /* M/M */
#define MSG_EXTENDED		0x01 /* O/O */
#define MSG_SAVEDATAPOINTER	0x02 /* O/O */
#define MSG_RESTOREPOINTERS	0x03 /* O/O */
#define MSG_DISCONNECT		0x04 /* O/O */
#define MSG_INITIATOR_DET_ERR	0x05 /* M/M */
#define MSG_ABORT		0x06 /* O/M */
#define MSG_MESSAGE_REJECT	0x07 /* M/M */
#define MSG_NOOP		0x08 /* M/M */
#define MSG_PARITY_ERROR	0x09 /* M/M */
#define MSG_LINK_CMD_COMPLETE	0x0a /* O/O */
#define MSG_LINK_CMD_COMPLETEF	0x0b /* O/O */
#define MSG_BUS_DEV_RESET	0x0c /* O/M */
#define MSG_ABORT_TAG		0x0d /* O/O */
#define MSG_CLEAR_QUEUE		0x0e /* O/O */
#define MSG_INIT_RECOVERY	0x0f /* O/O */
#define MSG_REL_RECOVERY	0x10 /* O/O */
#define MSG_TERM_IO_PROC	0x11 /* O/O */

/* Messages (2 byte) */
#define MSG_SIMPLE_Q_TAG	0x20 /* O/O */
#define MSG_HEAD_OF_Q_TAG	0x21 /* O/O */
#define MSG_ORDERED_Q_TAG	0x22 /* O/O */
#define MSG_IGN_WIDE_RESIDUE	0x23 /* O/O */

/* Identify message */		     /* M/M */	
#define MSG_IDENTIFYFLAG	0x80 
#define MSG_IDENTIFY(lun, disc)	((disc) ? 0xc0 : MSG_IDENTIFYFLAG) | (lun))
#define MSG_ISIDENTIFY(m)	((m) & MSG_IDENTIFYFLAG)

/* Extended messages (opcode and length) */
#define MSG_EXT_SDTR		0x01
#define MSG_EXT_SDTR_LEN	0x03

#define MSG_EXT_WDTR		0x03
#define MSG_EXT_WDTR_LEN	0x02
