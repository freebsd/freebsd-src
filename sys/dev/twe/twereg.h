/*-
 * Copyright (c) 2000 Michael Smith
 * Copyright (c) 2000 BSDi
 * All rights reserved.
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
 *      $FreeBSD: src/sys/dev/twe/twereg.h,v 1.1.2.1 2000/05/25 01:50:48 msmith Exp $
 */

/* 
 * Register names, bit definitions, structure names and members are
 * identical with those in the Linux driver where possible and sane 
 * for simplicity's sake.  (The TW_ prefix has become TWE_)
 * Some defines that are clearly irrelevant to FreeBSD have been
 * removed.
 */

/* control register bit definitions */
#define TWE_CONTROL_CLEAR_HOST_INTERRUPT	0x00080000
#define TWE_CONTROL_CLEAR_ATTENTION_INTERRUPT	0x00040000
#define TWE_CONTROL_MASK_COMMAND_INTERRUPT	0x00020000
#define TWE_CONTROL_MASK_RESPONSE_INTERRUPT	0x00010000
#define TWE_CONTROL_UNMASK_COMMAND_INTERRUPT	0x00008000
#define TWE_CONTROL_UNMASK_RESPONSE_INTERRUPT	0x00004000
#define TWE_CONTROL_CLEAR_ERROR_STATUS		0x00000200
#define TWE_CONTROL_ISSUE_SOFT_RESET		0x00000100
#define TWE_CONTROL_ENABLE_INTERRUPTS		0x00000080
#define TWE_CONTROL_DISABLE_INTERRUPTS		0x00000040
#define TWE_CONTROL_ISSUE_HOST_INTERRUPT	0x00000020

#define TWE_SOFT_RESET(sc)	TWE_CONTROL(sc, TWE_CONTROL_ISSUE_SOFT_RESET |		\
					   TWE_CONTROL_CLEAR_HOST_INTERRUPT |		\
					   TWE_CONTROL_CLEAR_ATTENTION_INTERRUPT |	\
					   TWE_CONTROL_MASK_COMMAND_INTERRUPT |		\
					   TWE_CONTROL_MASK_RESPONSE_INTERRUPT |	\
					   TWE_CONTROL_CLEAR_ERROR_STATUS |		\
					   TWE_CONTROL_DISABLE_INTERRUPTS)

/* status register bit definitions */
#define TWE_STATUS_MAJOR_VERSION_MASK		0xF0000000
#define TWE_STATUS_MINOR_VERSION_MASK		0x0F000000
#define TWE_STATUS_PCI_PARITY_ERROR		0x00800000
#define TWE_STATUS_QUEUE_ERROR			0x00400000
#define TWE_STATUS_MICROCONTROLLER_ERROR	0x00200000
#define TWE_STATUS_PCI_ABORT			0x00100000
#define TWE_STATUS_HOST_INTERRUPT		0x00080000
#define TWE_STATUS_ATTENTION_INTERRUPT		0x00040000
#define TWE_STATUS_COMMAND_INTERRUPT		0x00020000
#define TWE_STATUS_RESPONSE_INTERRUPT		0x00010000
#define TWE_STATUS_COMMAND_QUEUE_FULL		0x00008000
#define TWE_STATUS_RESPONSE_QUEUE_EMPTY		0x00004000
#define TWE_STATUS_MICROCONTROLLER_READY	0x00002000
#define TWE_STATUS_COMMAND_QUEUE_EMPTY		0x00001000
#define TWE_STATUS_ALL_INTERRUPTS		0x000F0000
#define TWE_STATUS_CLEARABLE_BITS		0x00D00000
#define TWE_STATUS_EXPECTED_BITS		0x00002000
#define TWE_STATUS_UNEXPECTED_BITS		0x00F80000

/* for use with the %b printf format */
#define TWE_STATUS_BITS_DESCRIPTION \
	"\20\15CQEMPTY\16UCREADY\17RQEMPTY\20CQFULL\21RINTR\22CINTR\23AINTR\24HINTR\25PCIABRT\26MCERR\27QERR\30PCIPERR\n"

/* detect inconsistencies in the status register */
#define TWE_STATUS_ERRORS(x)				\
	(((x & TWE_STATUS_PCI_ABORT) 		||	\
	  (x & TWE_STATUS_PCI_PARITY_ERROR) 	||	\
	  (x & TWE_STATUS_QUEUE_ERROR)		||	\
	  (x & TWE_STATUS_MICROCONTROLLER_ERROR)) &&	\
	 (x & TWE_STATUS_MICROCONTROLLER_READY))

/* Response queue bit definitions */
#define TWE_RESPONSE_ID_MASK		0x00000FF0

/* PCI related defines */
#define TWE_IO_CONFIG_REG		0x10
#define TWE_DEVICE_NAME			"3ware Storage Controller"
#define TWE_VENDOR_ID			0x13C1
#define TWE_DEVICE_ID			0x1000

/* command packet opcodes */
#define TWE_OP_NOP			0x0
#define TWE_OP_INIT_CONNECTION		0x1
#define TWE_OP_READ			0x2
#define TWE_OP_WRITE			0x3
#define TWE_OP_VERIFY			0x4
#define TWE_OP_GET_PARAM		0x12
#define TWE_OP_SET_PARAM		0x13
#define TWE_OP_SECTOR_INFO		0x1a
#define TWE_OP_AEN_LISTEN		0x1c

/* asynchronous event notification (AEN) codes */
#define TWE_AEN_QUEUE_EMPTY		0x0000
#define TWE_AEN_SOFT_RESET		0x0001
#define TWE_AEN_DEGRADED_MIRROR		0x0002
#define TWE_AEN_CONTROLLER_ERROR	0x0003
#define TWE_AEN_REBUILD_FAIL		0x0004
#define TWE_AEN_REBUILD_DONE		0x0005
#define TWE_AEN_QUEUE_FULL		0x00ff
#define TWE_AEN_TABLE_UNDEFINED		0x15

/* misc defines */
#define TWE_ALIGNMENT			0x200
#define TWE_MAX_UNITS			16
#define TWE_COMMAND_ALIGNMENT_MASK	0x1ff
#define TWE_INIT_MESSAGE_CREDITS	0x100
#define TWE_INIT_COMMAND_PACKET_SIZE	0x3
#define TWE_MAX_SGL_LENGTH		62
#define TWE_Q_LENGTH			256
#define TWE_Q_START			0
#define TWE_MAX_RESET_TRIES		3
#define TWE_UNIT_INFORMATION_TABLE_BASE	0x300
#define TWE_BLOCK_SIZE			0x200	/* 512-byte blocks */
#define TWE_SECTOR_SIZE			0x200	/* generic I/O bufffer */
#define TWE_IOCTL			0x80
#define TWE_MAX_AEN_TRIES		100

/* wrappers for bus-space actions */
#define TWE_CONTROL(sc, val)		bus_space_write_4(sc->twe_btag, sc->twe_bhandle, 0x0, (u_int32_t)val)
#define TWE_STATUS(sc)			(u_int32_t)bus_space_read_4(sc->twe_btag, sc->twe_bhandle, 0x4)
#define TWE_COMMAND_QUEUE(sc, val)	bus_space_write_4(sc->twe_btag, sc->twe_bhandle, 0x8, (u_int32_t)val)
#define TWE_RESPONSE_QUEUE(sc)		(TWE_Response_Queue)bus_space_read_4(sc->twe_btag, sc->twe_bhandle, 0xc)

/* scatter/gather list entry */
typedef struct
{
    u_int32_t	address;
    u_int32_t	length;
} TWE_SG_Entry __attribute__ ((packed));

/* command packet - must be TWE_ALIGNMENT aligned */
typedef struct
{
    u_int8_t	opcode:5;
    u_int8_t	sgl_offset:3;
    u_int8_t	size;
    u_int8_t	request_id;
    u_int8_t	unit:4;
    u_int8_t	host_id:4;
    u_int8_t	status;
    u_int8_t	flags;
    u_int16_t	count;			/* block count, parameter count, message credits */
    union {
	struct {
	    u_int32_t	lba;
	    TWE_SG_Entry sgl[TWE_MAX_SGL_LENGTH];
	} io __attribute__ ((packed));
	struct {
	    TWE_SG_Entry sgl[TWE_MAX_SGL_LENGTH];
	} param;
	struct {
	    u_int32_t	response_queue_pointer;
	} init_connection;
    } args;
} TWE_Command __attribute__ ((packed));

/* argument to TWE_OP_GET/SET_PARAM */
typedef struct 
{
    u_int16_t	table_id;
    u_int8_t	parameter_id;
    u_int8_t	parameter_size_bytes;
    u_int8_t	data[1];
} TWE_Param __attribute__ ((packed));

/* response queue entry */
typedef union
{
    struct 
    {
	u_int32_t	undefined_1:4;
	u_int32_t	response_id:8;
	u_int32_t	undefined_2:20;
    } u;
    u_int32_t	value;
} TWE_Response_Queue;

#if 0 /* no idea what these will be useful for yet */
typedef struct
{
    int32_t	buffer;
    u_int8_t	opcode;
    u_int16_t	table_id;
    u_int8_t	parameter_id;
    u_int8_t	parameter_size_bytes;
    u_int8_t	data[1];
} TWE_Ioctl __attribute__ ((packed));

typedef struct
{
    u_int32_t	base_addr;
    u_int32_t	control_reg_addr;
    u_int32_t	status_reg_addr;
    u_int32_t	command_que_addr;
    u_int32_t	response_que_addr;
} TWE_Registers __attribute__ ((packed));

typedef struct
{
    char	*buffer;
    int32_t	length;
    int32_t	offset;
    int32_t	position;
} TWE_Info __attribute__ ((packed));
#endif


