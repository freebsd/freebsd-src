;;
;; Etrax100 slave network<->parport forwarder
;;
;; Copyright (c) 1999, 2000, 2001, 2002, 2003 Axis Communications AB
;;
;; We got 784 bytes (par loader size) to do DMA forwarding
;; between DMA0/1 (ethernet) and DMA3/4 (par port 0 RX/1 TX)
;;

#include <linux/config.h>
#if 0
#define ASSEMBLER_MACROS_ONLY
#endif
#include <asm/sv_addr_ag.h>

#define BUFSIZE 0x600

	;; R_IRQ_READ2

#define DMA1EOPBIT 3
#define DMA0EOPBIT 1
#define DMA3EOPBIT 7
#define DMA4DESCBIT 8

	;; R_IRQ_READ0

#define PAR0ECPCMDBIT 11

	;; get host CMDs

#include "e100lpslave.h"

start:
	;; disable interrupts. we are not going to use them at all.

	di

	;; setup DMA connections and port configuration

	movu.w	0x84, r0	; DMA2/3/4/5 to par ports
	move.d	r0, [R_GEN_CONFIG]

	;; setup port PA dirs and turn on the LED to show were alive

	movu.w	0x0cfb, r0	; PA2-PA3 out, PA2 inactive
	move.d	r0, [R_PORT_PA_SET]

	;; enable MDIO output pin
	moveq IO_STATE(R_NETWORK_MGM_CTRL, mdoe, enable), r0
	move.d	r0, [R_NETWORK_MGM_CTRL]

	;; accept broadcast frames, and enable station address 0
	moveq	IO_STATE(R_NETWORK_REC_CONFIG, broadcast, receive) | \
		IO_STATE(R_NETWORK_REC_CONFIG, ma0, enable), r0
	move.d	r0, [R_NETWORK_REC_CONFIG]

	;; use MII CLK mode, and enable the controller
	moveq	IO_STATE(R_NETWORK_GEN_CONFIG, phy, mii_clk) | \
		IO_STATE(R_NETWORK_GEN_CONFIG, enable, on), r0
	move.d	r0, [R_NETWORK_GEN_CONFIG]

	move.d	IO_STATE(R_PAR0_CONFIG, ioe,     noninv)    |  \
		IO_STATE(R_PAR0_CONFIG, iseli,   noninv)    |  \
		IO_STATE(R_PAR0_CONFIG, iautofd, noninv)    |  \
		IO_STATE(R_PAR0_CONFIG, istrb,   noninv)    |  \
		IO_STATE(R_PAR0_CONFIG, iinit,   noninv)    |  \
		IO_STATE(R_PAR0_CONFIG, iperr,   noninv)    |  \
		IO_STATE(R_PAR0_CONFIG, iack,    noninv)    |  \
		IO_STATE(R_PAR0_CONFIG, ibusy,   noninv)    |  \
		IO_STATE(R_PAR0_CONFIG, ifault,  noninv)    |  \
		IO_STATE(R_PAR0_CONFIG, isel,    noninv)    |  \
		IO_STATE(R_PAR0_CONFIG, dma, enable)        |  \
		IO_STATE(R_PAR0_CONFIG, rle_in, disable)    |  \
		IO_STATE(R_PAR0_CONFIG, rle_out, disable)   |  \
		IO_STATE(R_PAR0_CONFIG, enable, on)         |  \
		IO_STATE(R_PAR0_CONFIG, force, on)          |  \
		IO_STATE(R_PAR0_CONFIG, mode, ecp_rev), r0	; Reverse ECP - PAR0 is RX

	move.d	r0, [R_PAR0_CONFIG]

	move.d	IO_STATE(R_PAR1_CONFIG, ioe,     noninv)    |  \
		IO_STATE(R_PAR1_CONFIG, iseli,   noninv)    |  \
		IO_STATE(R_PAR1_CONFIG, iautofd, noninv)    |  \
		IO_STATE(R_PAR1_CONFIG, istrb,   noninv)    |  \
		IO_STATE(R_PAR1_CONFIG, iinit,   noninv)    |  \
		IO_STATE(R_PAR1_CONFIG, iperr,   inv)       |  \
		IO_STATE(R_PAR1_CONFIG, iack,    noninv)    |  \
		IO_STATE(R_PAR1_CONFIG, ibusy,   noninv)    |  \
		IO_STATE(R_PAR1_CONFIG, ifault,  noninv)    |  \
		IO_STATE(R_PAR1_CONFIG, isel,    noninv)    |  \
		IO_STATE(R_PAR1_CONFIG, dma, enable)        |  \
		IO_STATE(R_PAR1_CONFIG, rle_in, disable)    |  \
		IO_STATE(R_PAR1_CONFIG, rle_out, disable)   |  \
		IO_STATE(R_PAR1_CONFIG, enable, on)         |  \
		IO_STATE(R_PAR1_CONFIG, force, on)          |  \
		IO_STATE(R_PAR1_CONFIG, mode, ecp_fwd), r0	; Forward ECP - PAR1 is TX

	move.d	r0, [R_PAR1_CONFIG]

	moveq	IO_FIELD(R_PAR1_DELAY, setup, 0), r0    ; setup time of value * 160 + 20 == 20 ns
	move.d	r0, [R_PAR1_DELAY]

	;; we got four descriptors, that can be active at the same time:
	;; 1) from network
	;; 2) to parport
	;; 3) from parport
	;; 4) to network
	;;
	;; we got four buffers, each can hold a max packet (we use 1536 bytes)
	;; buffers 1 and 2 are used from network to parport, while
	;; buffers 3 and 4 are used from parport to network.
	;; 
	;; a double buffering scheme is used, so that new data can be read
	;; into a buffer pair while the last data is written out from the
	;; last buffer. if the read buffer is done before the write buffer,
	;; the reading will halt until the writing is done, at which point
	;; writing starts from the newly read and reading can start with
	;; the newly written.
	;; 

	move.d	R_DMA_CH0_FIRST, r1   ; we use this as base for subsequent DMA ops
	moveq	IO_STATE(R_DMA_CH1_CMD, cmd, start), r6
	move.d	FN1desc, r7
	move.d	R_IRQ_READ0, r9

	;; start receiving from network

	jsr	startdmaFPTN
	jsr	startdmaFNTP

	

	;; ------------------- MAIN LOOP

	;; IRQ bits:	parport rcv is par0_ecp_cmd, then dma3_eop
	;;              network rcv is dma1_eop
	;;              parport tx  is dma4_desc
	;;              network tx  is dma0_eop

mainloop:

	;; ------- first handle the parport -> network link

	;; check if we got something from the parport

	move.d	[r9], r0	; r0 <- *R_IRQ_READ0
	btstq	PAR0ECPCMDBIT, r0
	bpl	noparecp
	nop

	;; ack it by reading PAR0_STATUS_DATA

	move.d	[R_PAR0_STATUS_DATA], r0

	;; trigger EOP on DMA3 (par0 incoming channel)

	moveq	IO_STATE(R_SET_EOP, ch3_eop, set), r0
	move.d	r0, [R_SET_EOP]

noparecp:

	;; if we simultaneously have parport rx EOP and
	;; network TX eop, we can swap buffers and start a new RX/TX

	move.d	[r9 + (R_IRQ_READ2 - R_IRQ_READ0)], r0
	btstq	DMA3EOPBIT, r0	; check parport rx
	bpl	noswap1
	btstq	DMA0EOPBIT, r0	; check network tx
	bpl	noswap1
	nop

	;; prepare to swap buffer ptrs (FN3b <-> TN4b)

	move.d	[r4 = r7 + 56], r0; FP3b
	move.d	[r3 = r7 + 72], r2; TN4b

	;; but first check if this was a Host Command Packet

	move.d	[r0], r5	; r5 <- first 4 bytes in PAR-received packet
	bne	handle_command	; if non-zero, it was a host command
	addq	4, r0		; skip command (in delay slot - handle_command requires this)
	move.d	r0, [r3]	; write to To Network descriptor
	subq	4, r2		; undo the skipping done last swap
	move.d	r2, [r4]	; write to From Parport descriptor

	;; clear the interrupts

	moveq	IO_STATE(R_DMA_CH0_CLR_INTR, clr_eop, do), r0
	move.b	r0, [r1 + (R_DMA_CH0_CLR_INTR - R_DMA_CH0_FIRST)]
	move.b	r0, [r1 + (R_DMA_CH3_CLR_INTR - R_DMA_CH0_FIRST)]

	;; copy received length to outgoing network length

	move.w	[r7 + 60], r0	; FPhlen
	subq	4, r0		; skip command
	move.w	r0, [r7 + 64]	; TN4desc

	;; restart DMAs

	jsr	startdmaFPTN

#ifdef CONFIG_ETRAX_ETHERNET_LPSLAVE_HAS_LEDS
#if defined(CONFIG_ETRAX_NETWORK_LED_ON_WHEN_LINK)
	;; Turn off the LED signaling an outgoing network packet
	movu.b	[LEDOff], r0
#elif defined(CONFIG_ETRAX_NETWORK_LED_ON_WHEN_ACTIVITY)
	;; Light the LED signaling an outgoing network packet
	movu.b	[LEDAmber], r0
#else
#error "Define either CONFIG_ETRAX_NETWORK_LED_ON_WHEN_LINK or CONFIG_ETRAX_NETWORK_LED_ON_WHEN_ACTIVITY"	
#endif 
	move.b	r0, [R_PORT_PA_DATA]
	move.d	0x00011000, r0
	move.d	r0,[LEDCount]
#endif

noswap1:
	;; ----- now check the network -> parport link


	;; if we simultaneously have network rx EOP and
	;; parport TX desc, we can swap buffers and start a new RX/TX

	move.d	[r9 + (R_IRQ_READ2 - R_IRQ_READ0)], r0
	btstq	DMA1EOPBIT, r0	; check network rx
	bpl	noswap2
	btstq	DMA4DESCBIT, r0	; check parport tx
	bpl	noswap2
	nop

	;; prepare to swap buffer ptrs (FP1b <-> TP2b)

	move.d	[r4 = r7 +  8], r0; FN1b
	move.d	[r3 = r7 + 24], r2; TP2b
	move.d	r0, [r3]	; write to To Parport descriptor
	move.d	r2, [r4]	; write to From Network descriptor

	;; clear the interrupts

	moveq	IO_STATE(R_DMA_CH1_CLR_INTR, clr_eop, do) | \
		IO_STATE(R_DMA_CH1_CLR_INTR, clr_descr, do), r0
	move.b	r0, [r1 + (R_DMA_CH1_CLR_INTR - R_DMA_CH0_FIRST)]
	move.b	r0, [r1 + (R_DMA_CH4_CLR_INTR - R_DMA_CH0_FIRST)]

	;; copy received network length to outgoing parport length

	move.w	[r7 + 12], r0	; FNhlen
	move.w	r0, [r7 + 16]	; TP2desc

	;; restart DMAs

	jsr	startdmaFNTP
#if 0
#ifdef CONFIG_ETRAX_ETHERNET_LPSLAVE_HAS_LEDS
	;; Light the LED signaling an incoming networkpacket
	movu.b	0xFB, r0
	move.b	r0, [R_PORT_PA_DATA]
	move.d	0x00010000, r0
	move.d	r0,[LEDCount]
#endif
#endif

noswap2:
#ifdef CONFIG_ETRAX_ETHERNET_LPSLAVE_HAS_LEDS

	;; Count down LED counter, and turn off the network LED if required
	move.d	[LEDCount], r0
	beq	mainloop
	nop

	subq	1, r0
	move.d	r0, [LEDCount]	
	bne	mainloop
	nop

#if defined(CONFIG_ETRAX_NETWORK_LED_ON_WHEN_LINK)	
	;; Light the network LED , and start over the main loop
	movu.b	[LEDAmber], r0
#elif defined(CONFIG_ETRAX_NETWORK_LED_ON_WHEN_ACTIVITY)		
	;; Turn off the network LED, and start over the main loop
	movu.b	[LEDOff], r0
#else
#error "Define either CONFIG_ETRAX_NETWORK_LED_ON_WHEN_LINK or CONFIG_ETRAX_NETWORK_LED_ON_WHEN_ACTIVITY"	
#endif
	move.b	r0, [R_PORT_PA_DATA]
#endif

	ba	mainloop
	nop

	;; --- some useful subroutines.

handle_command:
	;; handle command. we also need to clear the PAR0 RX EOP IRQ, and 
	;; restart the PAR0 dma. command is in R5, packet after cmd is in R0

	moveq	IO_STATE(R_DMA_CH3_CLR_INTR, clr_eop, do), r2
	move.b	r2, [r1 + (R_DMA_CH3_CLR_INTR - R_DMA_CH0_FIRST)]

	cmpq	HOST_CMD_SETMAC, r5
	bne	no_setmac
	nop

	;; copy station address (6 bytes) from packet to hardware

	move.d	[r0+], r2
	move.d	R_NETWORK_SA_0, r3
	move.d	r2, [r3]
	move.w	[r0], r2
	move.w	r2, [r3 + 4] 

no_setmac:
	move	noswap1, SRP
	ba	startdmaFP
	nop

	;; start DMAs, from parport and to network

startdmaFPTN:

	;; start transmitting to the network (CH0)

	move.d	TN4desc, r8
	move.d	r8, [r1]					; TN4desc -> FIRST0
	move.b	r6, [r1 + (R_DMA_CH0_CMD - R_DMA_CH0_FIRST)]	; start -> CMD0

startdmaFP:

	;; start receiving from parport (CH3)

	move.d	FP3desc, r8
	move.d	r8, [r1 + (R_DMA_CH3_FIRST - R_DMA_CH0_FIRST)]  ; FP3desc -> FIRST3
	move.b	r6, [r1 + (R_DMA_CH3_CMD - R_DMA_CH0_FIRST)]	; start -> CMD3

	ret
	nop

	;; start DMAs, from network and to parport

startdmaFNTP:

	;; start transmitting to the parport (CH4)

	move.d	TP2desc, r8
	move.d	r8, [r1 + (R_DMA_CH4_FIRST - R_DMA_CH0_FIRST)]	; TP2desc -> FIRST4
	move.b	r6, [r1 + (R_DMA_CH4_CMD - R_DMA_CH0_FIRST)]	; start -> CMD4

	;; start receiving from network (CH1) (r7 already contains FN1desc)

	move.d	r7, [r1 + (R_DMA_CH1_FIRST - R_DMA_CH0_FIRST)]  ; FN1desc -> FIRST1
	move.b	r6, [r1 + (R_DMA_CH1_CMD - R_DMA_CH0_FIRST)]	; start -> CMD1

	ret
	nop

	;; --- DMA descriptors - each descriptor is 4 longwords (16 bytes)
	;; DONT MOVE THESE AROUND. Due to the as/ld "hole-in-the-head",
	;; we cant write stuff like (TP2b - TP2desc) but the offsets
	;; have to be hardcoded.

	.data

	;; 0 from network
FN1desc:
	.word	BUFSIZE		; sw_len
	.word	0x0001		; ctrl, d_eol is only flag we need
	.dword	0		; next
FN1b:	.dword	buffers		; buffer 1 8
	.word	0		; hw_len
	.word	0		; status

	;; 16 to parport
TP2desc:
	.word	2		; sw_len, filled in by code 
	.word	0x0004		; ctrl, d_wait because ecp cmd in next
	.dword	TP2desc2	; next
TP2b:	.dword	buffers + BUFSIZE ; buffer 2 24
	.word	0		; hw_len
	.word	0		; status

	;; 32 to parport second descriptor, for the ECP command
TP2desc2:
	.word	0x0001		; sw_len, 1 byte (ecp command) 
	.word	0x0019		; ctrl, d_ecp | d_eol | d_int
	.dword	0		; next
	.dword	TP2desc2	; buffer, dont care
	.word	0		; hw_len
	.word	0		; status

	;; 48 from parport
FP3desc:
	.word	BUFSIZE		; sw_len
	.word	0x0001		; ctrl, d_eol is only flag we need
	.dword	0		; next
FP3b:	.dword	buffers + BUFSIZE * 2 ; 56 buffer 3
FPhlen:	.word	0		; 60 hw_len
	.word	0		; status

	;; 64 to network
TN4desc:
	.word	2		; sw_len, filled in by code 
	.word	0x0007		; ctrl, d_eop | d_eol | d_wait
	.dword	0		; next
TN4b:	.dword	buffers + BUFSIZE * 3 + 4	; 72 buffer 4 (the +4 is to offset the anti-skipping)
	.word	0		; hw_len
	.word	0		; status

#ifdef CONFIG_ETRAX_ETHERNET_LPSLAVE_HAS_LEDS
LEDCount:
	.dword	0
LEDOff:	
	.word	0xff
LEDGreen:
	.word	0xfb
LEDRed:
	.word	0xf7
LEDAmber:
	.word	0xf3
LED:
	.word	0xf7
#endif

	;; after the prog we put the buffers. not in the asm program, we just use
	;; the address generated

buffers:

	;; END
