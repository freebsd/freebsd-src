/*
 * Copyright (c) 2001 Cubical Solutions Ltd. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * capi/iavc/iavc.h	The AVM ISDN controllers' common declarations.
 *
 * $FreeBSD$
 */

#ifndef _CAPI_IAVC_H_
#define _CAPI_IAVC_H_

/* max 4 units supported per machine */

#define IAVC_MAXUNIT 4

/*
//  iavc_softc_t
//      The software context of one AVM T1 controller.
*/

#define IAVC_IO_BASES 1

typedef struct i4b_info {
    struct resource * io_base[IAVC_IO_BASES];
    int               io_rid [IAVC_IO_BASES];
    struct resource * irq;
    int               irq_rid;
    struct resource * mem;
    int               mem_rid;
} i4b_info_t;

typedef struct iavc_softc {
    capi_softc_t        sc_capi;
    int		        sc_unit;
    int		        sc_cardtyp;

    u_int32_t           sc_membase;
    bus_space_handle_t  sc_mem_bh;
    bus_space_tag_t     sc_mem_bt;
    u_int32_t	        sc_iobase;
    bus_space_handle_t  sc_io_bh;
    bus_space_tag_t     sc_io_bt;

    int                 sc_state;
#define IAVC_DOWN       0
#define IAVC_POLL       1
#define IAVC_INIT       2
#define IAVC_UP         3
    int                 sc_blocked;
    int                 sc_dma;
    int                 sc_t1;
    int                 sc_intr;

    u_int32_t           sc_csr;

    char                sc_sendbuf[128+2048];
    char                sc_recvbuf[128+2048];
    int                 sc_recvlen;

    struct ifqueue      sc_txq;

    i4b_info_t	        sc_resources;
} iavc_softc_t;

extern iavc_softc_t iavc_sc[];

#define iavc_find_sc(unit)	(&iavc_sc[(unit)])

/*
//  {b1,b1dma,t1}_{detect,reset}
//      Routines to detect and manage the specific type of card.
*/

extern int      b1_detect(iavc_softc_t *sc);
extern void     b1_disable_irq(iavc_softc_t *sc);
extern void     b1_reset(iavc_softc_t *sc);

extern int      b1dma_detect(iavc_softc_t *sc);
extern void     b1dma_reset(iavc_softc_t *sc);

extern int      t1_detect(iavc_softc_t *sc);
extern void     t1_disable_irq(iavc_softc_t *sc);
extern void     t1_reset(iavc_softc_t *sc);

/*
//  AMCC_{READ,WRITE}
//      Routines to access the memory mapped registers of the
//      S5933 DMA controller.
*/

static __inline u_int32_t AMCC_READ(iavc_softc_t *sc, int off)
{
    return bus_space_read_4(sc->sc_mem_bt, sc->sc_mem_bh, off);
}

static __inline void AMCC_WRITE(iavc_softc_t *sc, int off, u_int32_t value)
{
    bus_space_write_4(sc->sc_mem_bt, sc->sc_mem_bh, off, value);
}

/*
//  amcc_{put,get}_{byte,word}
//      Routines to access the DMA buffers byte- or wordwise.
*/

static __inline u_int8_t* amcc_put_byte(u_int8_t *buf, u_int8_t value)
{
    *buf++ = value;
    return buf;
}

static __inline u_int8_t* amcc_get_byte(u_int8_t *buf, u_int8_t *value)
{
    *value = *buf++;
    return buf;
}

static __inline u_int8_t* amcc_put_word(u_int8_t *buf, u_int32_t value)
{
    *buf++ = (value & 0xff);
    *buf++ = (value >> 8) & 0xff;
    *buf++ = (value >> 16) & 0xff;
    *buf++ = (value >> 24) & 0xff;
    return buf;
}

static __inline u_int8_t* amcc_get_word(u_int8_t *buf, u_int32_t *value)
{
    *value = *buf++;
    *value |= (*buf++ << 8);
    *value |= (*buf++ << 16);
    *value |= (*buf++ << 24);
    return buf;
}

/*
//  Controller LLI message numbers.
*/

#define SEND_POLL           0x72
#define SEND_INIT           0x11
#define SEND_REGISTER       0x12
#define SEND_DATA_B3_REQ    0x13
#define SEND_RELEASE        0x14
#define SEND_MESSAGE        0x15
#define SEND_CONFIG         0x71
#define SEND_POLLACK        0x73

#define RECEIVE_POLL        0x32
#define RECEIVE_INIT        0x27
#define RECEIVE_MESSAGE     0x21
#define RECEIVE_DATA_B3_IND 0x22
#define RECEIVE_START       0x23
#define RECEIVE_STOP        0x24
#define RECEIVE_NEW_NCCI    0x25
#define RECEIVE_FREE_NCCI   0x26
#define RECEIVE_RELEASE     0x26
#define RECEIVE_TASK_READY  0x31
#define RECEIVE_DEBUGMSG    0x71
#define RECEIVE_POLLDWORD   0x75

/* Operation constants */

#define WRITE_REGISTER      0x00
#define READ_REGISTER       0x01

/* Port offsets in I/O space */

#define B1_READ             0x00
#define B1_WRITE            0x01
#define B1_INSTAT           0x02
#define B1_OUTSTAT          0x03
#define B1_ANALYSE          0x04
#define B1_REVISION         0x05
#define B1_RESET            0x10

#define T1_FASTLINK         0x00
#define T1_SLOWLINK         0x08

#define T1_READ             B1_READ
#define T1_WRITE            B1_WRITE
#define T1_INSTAT           B1_INSTAT
#define T1_OUTSTAT          B1_OUTSTAT
#define T1_IRQENABLE        0x05
#define T1_FIFOSTAT         0x06
#define T1_RESETLINK        0x10
#define T1_ANALYSE          0x11
#define T1_IRQMASTER        0x12
#define T1_IDENT            0x17
#define T1_RESETBOARD       0x1f

#define T1F_IREADY          0x01
#define T1F_IHALF           0x02
#define T1F_IFULL           0x04
#define T1F_IEMPTY          0x08
#define T1F_IFLAGS          0xf0

#define T1F_OREADY          0x10
#define T1F_OHALF           0x20
#define T1F_OEMPTY          0x40
#define T1F_OFULL           0x80
#define T1F_OFLAGS          0xf0

#define FIFO_OUTBSIZE       256
#define FIFO_INPBSIZE       512

#define HEMA_VERSION_ID     0
#define HEMA_PAL_ID         0

/*
//  S5933 DMA controller register offsets in memory, and bitmasks.
*/

#define AMCC_RXPTR       0x24
#define AMCC_RXLEN       0x28
#define AMCC_TXPTR       0x2c
#define AMCC_TXLEN       0x30

#define AMCC_INTCSR      0x38
#define EN_READ_TC_INT   0x00008000
#define EN_WRITE_TC_INT  0x00004000
#define EN_TX_TC_INT     EN_READ_TC_INT
#define EN_RX_TC_INT     EN_WRITE_TC_INT
#define AVM_FLAG         0x30000000

#define ANY_S5933_INT    0x00800000
#define READ_TC_INT      0x00080000
#define WRITE_TC_INT     0x00040000
#define TX_TC_INT        READ_TC_INT
#define RX_TC_INT        WRITE_TC_INT
#define MASTER_ABORT_INT 0x00100000
#define TARGET_ABORT_INT 0x00200000
#define BUS_MASTER_INT   0x00200000
#define ALL_INT          0x000c0000

#define AMCC_MCSR        0x3c
#define A2P_HI_PRIORITY  0x00000100
#define EN_A2P_TRANSFERS 0x00000400
#define P2A_HI_PRIORITY  0x00001000
#define EN_P2A_TRANSFERS 0x00004000
#define RESET_A2P_FLAGS  0x04000000
#define RESET_P2A_FLAGS  0x02000000

/*
//  (B1IO_WAIT_MAX * B1IO_WAIT_DLY) is the max wait in us for the card
//  to become ready after an I/O operation. The default is 1 ms.
*/

#define B1IO_WAIT_MAX    1000
#define B1IO_WAIT_DLY    1

/*
//  b1io_outp
//      Diagnostic output routine, returns the written value via
//      the device's analysis register.
//
//  b1io_rx_full
//      Returns nonzero if data is readable from the card via the
//      I/O ports.
//
//  b1io_tx_empty
//      Returns nonzero if data can be written to the card via the
//      I/O ports.
*/

static __inline u_int8_t b1io_outp(iavc_softc_t *sc, int off, u_int8_t val)
{
    bus_space_write_1(sc->sc_io_bt, sc->sc_io_bh, off, val);
    DELAY(1);
    return bus_space_read_1(sc->sc_io_bt, sc->sc_io_bh, B1_ANALYSE);
}

static __inline int b1io_rx_full(iavc_softc_t *sc)
{
    u_int8_t val = bus_space_read_1(sc->sc_io_bt, sc->sc_io_bh, B1_INSTAT);
    return (val & 0x01);
}

static __inline int b1io_tx_empty(iavc_softc_t *sc)
{
    u_int8_t val = bus_space_read_1(sc->sc_io_bt, sc->sc_io_bh, B1_OUTSTAT);
    return  (val & 0x01);
}

/*
//  b1io_{get,put}_{byte,word}
//      Routines to read and write the device I/O registers byte- or
//      wordwise.
//
//  b1io_{get,put}_slice
//      Routines to read and write sequential bytes to the device
//      I/O registers.
*/

static __inline u_int8_t b1io_get_byte(iavc_softc_t *sc)
{
    int spin = 0;
    while (!b1io_rx_full(sc) && spin < B1IO_WAIT_MAX) {
	spin++; DELAY(B1IO_WAIT_DLY);
    }
    if (b1io_rx_full(sc))
	return bus_space_read_1(sc->sc_io_bt, sc->sc_io_bh, B1_READ);
    printf("iavc%d: rx not completed\n", sc->sc_unit);
    return 0xff;
}

static __inline int b1io_put_byte(iavc_softc_t *sc, u_int8_t val)
{
    int spin = 0;
    while (!b1io_tx_empty(sc) && spin < B1IO_WAIT_MAX) {
	spin++; DELAY(B1IO_WAIT_DLY);
    }
    if (b1io_tx_empty(sc)) {
	bus_space_write_1(sc->sc_io_bt, sc->sc_io_bh, B1_WRITE, val);
	return 0;
    }
    printf("iavc%d: tx not emptied\n", sc->sc_unit);
    return -1;
}

static __inline int b1io_save_put_byte(iavc_softc_t *sc, u_int8_t val)
{
    int spin = 0;
    while (!b1io_tx_empty(sc) && spin < B1IO_WAIT_MAX) {
	spin++; DELAY(B1IO_WAIT_DLY);
    }
    if (b1io_tx_empty(sc)) {
	b1io_outp(sc, B1_WRITE, val);
	return 0;
    }
    printf("iavc%d: tx not emptied\n", sc->sc_unit);
    return -1;
}

static __inline u_int32_t b1io_get_word(iavc_softc_t *sc)
{
    u_int32_t val = 0;
    val |= b1io_get_byte(sc);
    val |= (b1io_get_byte(sc) << 8);
    val |= (b1io_get_byte(sc) << 16);
    val |= (b1io_get_byte(sc) << 24);
    return val;
}

static __inline void b1io_put_word(iavc_softc_t *sc, u_int32_t val)
{
    b1io_put_byte(sc, (val & 0xff));
    b1io_put_byte(sc, (val >> 8) & 0xff);
    b1io_put_byte(sc, (val >> 16) & 0xff);
    b1io_put_byte(sc, (val >> 24) & 0xff);
}

static __inline int b1io_get_slice(iavc_softc_t *sc, u_int8_t *dp)
{
    int len, i;
    len = i = b1io_get_word(sc);
    while (i--) *dp++ = b1io_get_byte(sc);
    return len;
}

static __inline void b1io_put_slice(iavc_softc_t *sc, u_int8_t *dp, int len)
{
    b1io_put_word(sc, len);
    while (len--) b1io_put_byte(sc, *dp++);
}

/*
//  b1io_{read,write}_reg
//      Routines to read and write the device registers via the I/O
//      ports.
*/

static __inline u_int32_t b1io_read_reg(iavc_softc_t *sc, int reg)
{
    b1io_put_byte(sc, READ_REGISTER);
    b1io_put_word(sc, reg);
    return b1io_get_word(sc);
}

static __inline u_int32_t b1io_write_reg(iavc_softc_t *sc, int reg, u_int32_t val)
{
    b1io_put_byte(sc, WRITE_REGISTER);
    b1io_put_word(sc, reg);
    b1io_put_word(sc, val);
    return b1io_get_word(sc);
}

/*
//  t1io_outp
//      I/O port write operation for the T1, which does not seem
//      to have the analysis port.
*/

static __inline void t1io_outp(iavc_softc_t *sc, int off, u_int8_t val)
{
    bus_space_write_1(sc->sc_io_bt, sc->sc_io_bh, off, val);
}

static __inline u_int8_t t1io_inp(iavc_softc_t *sc, int off)
{
    return bus_space_read_1(sc->sc_io_bt, sc->sc_io_bh, off);
}

static __inline int t1io_isfastlink(iavc_softc_t *sc)
{
    return ((bus_space_read_1(sc->sc_io_bt, sc->sc_io_bh, T1_IDENT) & ~0x82) == 1);
}

static __inline u_int8_t t1io_fifostatus(iavc_softc_t *sc)
{
    return bus_space_read_1(sc->sc_io_bt, sc->sc_io_bh, T1_FIFOSTAT);
}

static __inline int t1io_get_slice(iavc_softc_t *sc, u_int8_t *dp)
{
    int len, i;
    len = i = b1io_get_word(sc);
    if (t1io_isfastlink(sc)) {
	int status;
	while (i) {
	    status = t1io_fifostatus(sc) & (T1F_IREADY|T1F_IHALF);
	    if (i >= FIFO_INPBSIZE) status |= T1F_IFULL;

	    switch (status) {
	    case T1F_IREADY|T1F_IHALF|T1F_IFULL:
		bus_space_read_multi_1(sc->sc_io_bt, sc->sc_io_bh,
				       T1_READ, dp, FIFO_INPBSIZE);
		dp += FIFO_INPBSIZE;
		i -= FIFO_INPBSIZE;
		break;

	    case T1F_IREADY|T1F_IHALF:
		bus_space_read_multi_1(sc->sc_io_bt, sc->sc_io_bh,
				       T1_READ, dp, i);
		dp += i;
		i = 0;
		break;

	    default:
		*dp++ = b1io_get_byte(sc);
		i--;
	    }
	}
    } else { /* not fastlink */
	if (i--) *dp++ = b1io_get_byte(sc);
    }
    return len;
}

static __inline void t1io_put_slice(iavc_softc_t *sc, u_int8_t *dp, int len)
{
    int i = len;
    b1io_put_word(sc, i);
    if (t1io_isfastlink(sc)) {
	int status;
	while (i) {
	    status = t1io_fifostatus(sc) & (T1F_OREADY|T1F_OHALF);
	    if (i >= FIFO_OUTBSIZE) status |= T1F_OFULL;

	    switch (status) {
	    case T1F_OREADY|T1F_OHALF|T1F_OFULL:
		bus_space_write_multi_1(sc->sc_io_bt, sc->sc_io_bh,
					T1_WRITE, dp, FIFO_OUTBSIZE);
		dp += FIFO_OUTBSIZE;
		i -= FIFO_OUTBSIZE;
		break;

	    case T1F_OREADY|T1F_OHALF:
		bus_space_write_multi_1(sc->sc_io_bt, sc->sc_io_bh,
					T1_WRITE, dp, i);
		dp += i;
		i = 0;
		break;

	    default:
		b1io_put_byte(sc, *dp++);
		i--;
	    }
	}
    } else {
	while (i--) b1io_put_byte(sc, *dp++);
    }
}

/*
//  An attempt to bring it all together:
//  ------------------------------------
//
//  iavc_{read,write}_reg
//      Routines to access the device registers via the I/O port.
//
//  iavc_{read,write}_port
//      Routines to access the device I/O ports.
//
//  iavc_tx_empty, iavc_rx_full
//      Routines to check when the device has drained the last written
//      byte, or produced a full byte to read.
//
//  iavc_{get,put}_byte
//      Routines to read/write byte values to the device via the I/O port.
//
//  iavc_{get,put}_word
//      Routines to read/write 32-bit words to the device via the I/O port.
//
//  iavc_{get,put}_slice
//      Routines to read/write {length, data} pairs to the device via the
//      ubiquituous I/O port. Uses the HEMA FIFO on a T1.
*/

#define iavc_read_reg(sc, reg) b1io_read_reg(sc, reg)
#define iavc_write_reg(sc, reg, val) b1io_write_reg(sc, reg, val)

#define iavc_read_port(sc, port) \
        bus_space_read_1(sc->sc_io_bt, sc->sc_io_bh, (port))
#define iavc_write_port(sc, port, val) \
        bus_space_write_1(sc->sc_io_bt, sc->sc_io_bh, (port), (val))

#define iavc_tx_empty(sc)      b1io_tx_empty(sc)
#define iavc_rx_full(sc)       b1io_rx_full(sc)

#define iavc_get_byte(sc)      b1io_get_byte(sc)
#define iavc_put_byte(sc, val) b1io_put_byte(sc, val)
#define iavc_get_word(sc)      b1io_get_word(sc)
#define iavc_put_word(sc, val) b1io_put_word(sc, val)

static __inline u_int32_t iavc_get_slice(iavc_softc_t *sc, u_int8_t *dp)
{
    if (sc->sc_t1) return t1io_get_slice(sc, dp);
    else return b1io_get_slice(sc, dp);
}

static __inline void iavc_put_slice(iavc_softc_t *sc, u_int8_t *dp, int len)
{
    if (sc->sc_t1) t1io_put_slice(sc, dp, len);
    else b1io_put_slice(sc, dp, len);
}

/*
//  iavc_handle_intr
//      Interrupt handler, called by the bus specific interrupt routine
//      in iavc_<bustype>.c module.
//
//  iavc_load
//      CAPI callback. Resets device and loads firmware.
//
//  iavc_register
//      CAPI callback. Registers an application id.
//
//  iavc_release
//      CAPI callback. Releases an application id.
//
//  iavc_send
//      CAPI callback. Sends a CAPI message. A B3_DATA_REQ message has
//      m_next point to a data mbuf.
*/

extern void iavc_handle_intr(iavc_softc_t *);
extern int iavc_load(capi_softc_t *, int, u_int8_t *);
extern int iavc_register(capi_softc_t *, int, int);
extern int iavc_release(capi_softc_t *, int);
extern int iavc_send(capi_softc_t *, struct mbuf *);

extern void b1isa_setup_irq(struct iavc_softc *sc);

#endif /* _CAPI_IAVC_H_ */
