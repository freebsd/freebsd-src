/*
 *	AMD 7990 (LANCE) definitions
 *
 *
 */

#if defined(BYTE_ORDER) && BYTE_ORDER == BIG_ENDIAN
#define	LN_BITFIELD2(a, b)		      b, a
#define	LN_BITFIELD3(a, b, c)		   c, b, a
#define	LN_BITFIELD4(a, b, c, d)	d, c, b, a
#else
#define	LN_BITFIELD2(a, b)		a, b
#define	LN_BITFIELD3(a, b, c)		a, b, c
#define	LN_BITFIELD4(a, b, c, d)	a, b, c, d
#endif

#define	LN_ADDR_LO(addr)		((addr) & 0xFFFF)
#define	LN_ADDR_HI(addr)		(((addr) >> 16) & 0xFF)

typedef struct {
    unsigned short r_addr_lo;
    unsigned int   LN_BITFIELD3(r_addr_hi   : 8,
				            : 5,
				r_log2_size : 3);
} ln_ring_t;

#define	LN_MC_MASK		0x3F	/* Only 6 bits of the CRC */

typedef struct {
    unsigned short ln_mode;
#define	LN_MODE_RXD		0x0001	/* ( W)  Receiver Disabled */
#define	LN_MODE_TXD		0x0002	/* ( W)  Transmitter Disabled */
#define	LN_MODE_LOOP		0x0004	/* ( W)  Enable Loopback */
#define	LN_MODE_NOTXCRC		0x0008	/* ( W)  Don't Calculate TX CRCs */
#define	LN_MODE_FRCCOLL		0x0010	/* ( W)  Force Collision */
#define	LN_MODE_NORETRY		0x0020	/* ( W)  No Transmit Retries */
#define	LN_MODE_INTLOOP		0x0040	/* ( W)  Internal Loopback */
#define	LN_MODE_PROMISC		0x8000	/* ( W)  Promiscious Mode */
    unsigned short ln_physaddr[3];
    unsigned short ln_multi_mask[4];
    ln_ring_t ln_rxring;
    ln_ring_t ln_txring;
} ln_initb_t;

typedef struct {
    unsigned short d_addr_lo;
    unsigned char d_addr_hi;
    unsigned char d_flag;
#define	LN_DFLAG_EOP		0x0001	/* (RW)  End Of Packet */
#define	LN_DFLAG_SOP		0x0002	/* (RW)  Start Of Packet */
#define	LN_DFLAG_RxBUFERROR	0x0004	/* (R )  Receive  - Buffer Error */
#define	LN_DFLAG_TxDEFERRED	0x0004	/* (R )  Transmit - Initially Deferred */
#define	LN_DFLAG_RxBADCRC	0x0008	/* (R )  Receive  - Bad Checksum */
#define	LN_DFLAG_TxONECOLL	0x0008	/* (R )  Transmit - Single Collision */
#define	LN_DFLAG_RxOVERFLOW	0x0010	/* (R )  Receive  - Overflow Error */
#define	LN_DFLAG_TxMULTCOLL	0x0010	/* (R )  Transmit - Multiple Collisions */
#define	LN_DFLAG_RxFRAMING	0x0020	/* (R )  Receive  - Framing Error */
#define	LN_DFLAG_RxERRSUM	0x0040	/* (R )  Receive  - Error Summary */
#define	LN_DFLAG_TxERRSUM	0x0040	/* (R )  Transmit - Error Summary */
#define	LN_DFLAG_OWNER		0x0080	/* (RW)  Owner (1=Am7990, 0=host) */
    signed short d_buflen;		/* ( W)  Two's complement */
    unsigned short d_status;
#define	LN_DSTS_RxLENMASK	0x0FFF	/* (R )  Recieve Length */
#define	LN_DSTS_TxTDRMASK	0x03FF	/* (R )  Transmit - Time Domain Reflectometer */
#define	LN_DSTS_TxEXCCOLL	0x0400	/* (R )  Transmit - Excessive Collisions */
#define	LN_DSTS_TxCARRLOSS	0x0800	/* (R )  Transmit - Carrier Loss */
#define	LN_DSTS_TxLATECOLL	0x1000	/* (R )  Transmit - Late Collision */
#define	LN_DSTS_TxUNDERFLOW	0x4000	/* (R )  Transmit - Underflow */
#define	LN_DSTS_TxBUFERROR	0x8000	/* (R )  Transmit - Buffer Error */
} ln_desc_t;




#define	LN_CSR0			0x0000

#define	LN_CSR0_INIT		0x0001	/* (RS)  Initialize Am 7990 */
#define	LN_CSR0_START		0x0002	/* (RS)  Start Am7990 */
#define	LN_CSR0_STOP		0x0004	/* (RS)  Reset Am7990 */
#define	LN_CSR0_TXDEMAND	0x0008	/* (RS)  Transmit On Demand */
#define	LN_CSR0_TXON		0x0010	/* (R )  Transmitter Enabled */
#define	LN_CSR0_RXON		0x0020	/* (R )  Receiver Enabled */
#define	LN_CSR0_ENABINTR	0x0040	/* (RW)  Interrupt Enabled */
#define	LN_CSR0_PENDINTR	0x0080	/* (R )  Interrupt Pending */
#define	LN_CSR0_INITDONE	0x0100	/* (RC)  Initialization Done */
#define	LN_CSR0_TXINT		0x0200	/* (RC)  Transmit Interrupt */
#define	LN_CSR0_RXINT		0x0400	/* (RC)  Receive Interrupt */
#define	LN_CSR0_MEMERROR	0x0800	/* (RC)  Memory Error */
#define	LN_CSR0_MISS		0x1000	/* (RC)  No Available Receive Buffers */
#define	LN_CSR0_CERR		0x2000	/* (RC)  SQE failed */
#define	LN_CSR0_BABL		0x4000	/* (RC)  Transmit Babble */
#define	LN_CSR0_ERRSUM		0x8000	/* (R )  Error Summary (last 4) */
#define	LN_CSR0_CLEAR		0x7F00	/*       Clear Status Bit */

/*
 * CSR1 -- Init Block Address (Low 16 Bits -- Must be Word Aligned)
 * CSR2 -- Init Block Address (High 8 Bits)
 */
#define	LN_CSR1			0x0001
#define	LN_CSR2			0x0002

/*
 * CSR3 -- Hardware Control
 */

#define	LN_CSR3			0x0003
#define LN_CSR3_BCON		0x0001	/* (RW)  BM/HOLD Control */
#define LN_CSR3_ALE		0x0002	/* (RW)  ALE Control */
#define LN_CSR3_BSWP		0x0004	/* (RW)  Byte Swap */
