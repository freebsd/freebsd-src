/*******************************************************************************

Copyright (c) 2006, Myricom Inc.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

 1. Redistributions of source code must retain the above copyright notice,
    this list of conditions and the following disclaimer.

 2. Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.

 3. Neither the name of the Myricom Inc, nor the names of its
    contributors may be used to endorse or promote products derived from
    this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.

$FreeBSD$
***************************************************************************/

#ifndef _myri10ge_mcp_h
#define _myri10ge_mcp_h

#ifdef MYRI10GE_MCP
typedef signed char          int8_t;
typedef signed short        int16_t;
typedef signed int          int32_t;
typedef signed long long    int64_t;
typedef unsigned char       uint8_t;
typedef unsigned short     uint16_t;
typedef unsigned int       uint32_t;
typedef unsigned long long uint64_t;
#endif

/* 8 Bytes */
typedef struct
{
  uint32_t high;
  uint32_t low;
} mcp_dma_addr_t;

/* 16 Bytes */
typedef struct
{
  uint32_t data0;
  uint32_t data1;
  uint32_t seqnum;
  uint16_t index;
  uint8_t flag;
  uint8_t type;
} mcp_slot_t;

/* 64 Bytes */
typedef struct
{
  uint32_t cmd;
  uint32_t data0;	/* will be low portion if data > 32 bits */
  /* 8 */
  uint32_t data1;	/* will be high portion if data > 32 bits */
  uint32_t data2;	/* currently unused.. */
  /* 16 */
  mcp_dma_addr_t response_addr;
  /* 24 */
  uint8_t pad[40];
} mcp_cmd_t;

/* 8 Bytes */
typedef struct
{
  uint32_t data;
  uint32_t result;
} mcp_cmd_response_t;



/* 
   flags used in mcp_kreq_ether_send_t:

   The SMALL flag is only needed in the first segment. It is raised
   for packets that are total less or equal 512 bytes.

   The CKSUM flag must be set in all segments.

   The PADDED flags is set if the packet needs to be padded, and it
   must be set for all segments.

   The  MYRI10GE_MCP_ETHER_FLAGS_ALIGN_ODD must be set if the cumulative
   length of all previous segments was odd.
*/


#define MYRI10GE_MCP_ETHER_FLAGS_VALID      0x1
#define MYRI10GE_MCP_ETHER_FLAGS_FIRST      0x2
#define MYRI10GE_MCP_ETHER_FLAGS_ALIGN_ODD  0x4
#define MYRI10GE_MCP_ETHER_FLAGS_CKSUM      0x8
#define MYRI10GE_MCP_ETHER_FLAGS_SMALL      0x10
#define MYRI10GE_MCP_ETHER_FLAGS_NOT_LAST   0x100
#define MYRI10GE_MCP_ETHER_FLAGS_TSO_HDR    0x200
#define MYRI10GE_MCP_ETHER_FLAGS_TSO  	    0x400

#define MYRI10GE_MCP_ETHER_SEND_SMALL_SIZE  1520
#define MYRI10GE_MCP_ETHER_MAX_MTU          9400

typedef union mcp_pso_or_cumlen
{
  uint16_t pseudo_hdr_offset;
  uint16_t cum_len;
} mcp_pso_or_cumlen_t;

#define	MYRI10GE_MCP_ETHER_MAX_SEND_DESC 12
#define MYRI10GE_MCP_ETHER_PAD	    2

/* 16 Bytes */
typedef struct
{
  uint32_t addr_high;
  uint32_t addr_low;
  uint16_t length;
  uint8_t  pad; 
  uint8_t  cksum_offset; 	/* where to start computing cksum */
  uint16_t pseudo_hdr_offset;
  uint16_t flags;	       	/* as defined above */
} mcp_kreq_ether_send_t;

/* 8 Bytes */
typedef struct
{
  uint32_t addr_high;
  uint32_t addr_low;
} mcp_kreq_ether_recv_t;


/* Commands */

#define MYRI10GE_MCP_CMD_OFFSET 0xf80000

typedef enum {
  MYRI10GE_MCP_CMD_NONE = 0,
  /* Reset the mcp, it is left in a safe state, waiting
     for the driver to set all its parameters */
  MYRI10GE_MCP_CMD_RESET,

  /* get the version number of the current firmware..
     (may be available in the eeprom strings..? */
  MYRI10GE_MCP_GET_MCP_VERSION,


  /* Parameters which must be set by the driver before it can
     issue MYRI10GE_MCP_CMD_ETHERNET_UP. They persist until the next
     MYRI10GE_MCP_CMD_RESET is issued */

  MYRI10GE_MCP_CMD_SET_INTRQ0_DMA,
  MYRI10GE_MCP_CMD_SET_INTRQ1_DMA,
  MYRI10GE_MCP_CMD_SET_BIG_BUFFER_SIZE,	/* in bytes, power of 2 */
  MYRI10GE_MCP_CMD_SET_SMALL_BUFFER_SIZE,	/* in bytes */
  

  /* Parameters which refer to lanai SRAM addresses where the 
     driver must issue PIO writes for various things */

  MYRI10GE_MCP_CMD_GET_SEND_OFFSET,
  MYRI10GE_MCP_CMD_GET_SMALL_RX_OFFSET,
  MYRI10GE_MCP_CMD_GET_BIG_RX_OFFSET,
  MYRI10GE_MCP_CMD_GET_IRQ_ACK_OFFSET,
  MYRI10GE_MCP_CMD_GET_IRQ_DEASSERT_OFFSET,
  MYRI10GE_MCP_CMD_GET_IRQ_ACK_DEASSERT_OFFSET,

  /* Parameters which refer to rings stored on the MCP,
     and whose size is controlled by the mcp */

  MYRI10GE_MCP_CMD_GET_SEND_RING_SIZE,	/* in bytes */
  MYRI10GE_MCP_CMD_GET_RX_RING_SIZE,		/* in bytes */

  /* Parameters which refer to rings stored in the host,
     and whose size is controlled by the host.  Note that
     all must be physically contiguous and must contain 
     a power of 2 number of entries.  */

  MYRI10GE_MCP_CMD_SET_INTRQ_SIZE, 	/* in bytes */

  /* command to bring ethernet interface up.  Above parameters
     (plus mtu & mac address) must have been exchanged prior
     to issuing this command  */
  MYRI10GE_MCP_CMD_ETHERNET_UP,

  /* command to bring ethernet interface down.  No further sends
     or receives may be processed until an MYRI10GE_MCP_CMD_ETHERNET_UP
     is issued, and all interrupt queues must be flushed prior
     to ack'ing this command */

  MYRI10GE_MCP_CMD_ETHERNET_DOWN,

  /* commands the driver may issue live, without resetting
     the nic.  Note that increasing the mtu "live" should
     only be done if the driver has already supplied buffers
     sufficiently large to handle the new mtu.  Decreasing
     the mtu live is safe */

  MYRI10GE_MCP_CMD_SET_MTU,
  MYRI10GE_MCP_CMD_SET_INTR_COAL_DELAY,  /* in microseconds */
  MYRI10GE_MCP_CMD_SET_STATS_INTERVAL,   /* in microseconds */
  MYRI10GE_MCP_CMD_SET_STATS_DMA,

  MYRI10GE_MCP_ENABLE_PROMISC,
  MYRI10GE_MCP_DISABLE_PROMISC,
  MYRI10GE_MCP_SET_MAC_ADDRESS,

  MYRI10GE_MCP_ENABLE_FLOW_CONTROL,
  MYRI10GE_MCP_DISABLE_FLOW_CONTROL
} myri10ge_mcp_cmd_type_t;


typedef enum {
  MYRI10GE_MCP_CMD_OK = 0,
  MYRI10GE_MCP_CMD_UNKNOWN,
  MYRI10GE_MCP_CMD_ERROR_RANGE,
  MYRI10GE_MCP_CMD_ERROR_BUSY,
  MYRI10GE_MCP_CMD_ERROR_EMPTY,
  MYRI10GE_MCP_CMD_ERROR_CLOSED,
  MYRI10GE_MCP_CMD_ERROR_HASH_ERROR,
  MYRI10GE_MCP_CMD_ERROR_BAD_PORT,
  MYRI10GE_MCP_CMD_ERROR_RESOURCES
} myri10ge_mcp_cmd_status_t;

typedef enum {
  MYRI10GE_MCP_INTR_NONE = 0,
  MYRI10GE_MCP_INTR_ETHER_SEND_DONE,
  MYRI10GE_MCP_INTR_ETHER_RECV_SMALL,
  MYRI10GE_MCP_INTR_ETHER_RECV_BIG,
  MYRI10GE_MCP_INTR_LINK_CHANGE,
  MYRI10GE_MCP_INTR_STATS_UPDATE,
  MYRI10GE_MCP_INTR_ETHER_DOWN
} myri10ge_mcp_intr_type_t;


/* 32 Bytes */
typedef struct
{
  uint32_t link_up;
  uint32_t dropped_link_overflow;
  uint32_t dropped_link_error_or_filtered;
  uint32_t dropped_runt;
  uint32_t dropped_overrun;
  uint32_t dropped_no_small_buffer;
  uint32_t dropped_no_big_buffer;
  uint32_t dropped_interrupt_busy;
  uint32_t rdma_tags_available;
} mcp_stats_t;


#endif /* _myri10ge_mcp_h */
