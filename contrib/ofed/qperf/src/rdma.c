/*
 * qperf - handle RDMA tests.
 *
 * Copyright (c) 2002-2009 Johann George.  All rights reserved.
 * Copyright (c) 2006-2009 QLogic Corporation.  All rights reserved.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenIB.org BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#define _GNU_SOURCE
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <rdma/rdma_cma.h>
#include <infiniband/verbs.h>
#include "qperf.h"


/*
 * RDMA parameters.
 */
#define QKEY                0x11111111  /* Q_Key */
#define NCQE                1024        /* Number of CQ entries */
#define GRH_SIZE            40          /* InfiniBand GRH size */
#define MTU_SIZE            2048        /* Default MTU Size */
#define RETRY_CNT           7           /* RC retry count */
#define RNR_RETRY_CNT       7           /* RC RNR retry count */
#define MIN_RNR_TIMER       12          /* RC Minimum RNR timer */
#define LOCAL_ACK_TIMEOUT   14          /* RC local ACK timeout */


/*
 * Work request IDs.
 */
#define WRID_SEND   1                   /* Send */
#define WRID_RECV   2                   /* Receive */
#define WRID_RDMA   3                   /* RDMA */


/*
 * Constants.
 */
#define K2      (2*1024)
#define K64     (64*1024)


/*
 * For convenience.
 */
typedef enum ibv_wr_opcode ibv_op;
typedef struct ibv_comp_channel ibv_cc;
typedef struct ibv_xrc_domain ibv_xrc;


/*
 * Atomic operations.
 */
typedef enum ATOMIC {
    COMPARE_SWAP,
    FETCH_ADD
} ATOMIC;


/*
 * IO Mode.
 */
typedef enum IOMODE {
    IO_SR,                              /* Send/Receive */
    IO_RDMA                             /* RDMA */
} IOMODE;


/*
 * Information specific to a node.
 */
typedef struct NODE {
    uint64_t    vaddr;                  /* Virtual address */
    uint32_t    lid;                    /* Local ID */
    uint32_t    qpn;                    /* Queue pair number */
    uint32_t    psn;                    /* Packet sequence number */
    uint32_t    srqn;                   /* Shared queue number */
    uint32_t    rkey;                   /* Remote key */
    uint32_t    alt_lid;                /* Alternate Path Local LID */
    uint32_t    rd_atomic;              /* Number of read/atomics supported */
} NODE;


/*
 * InfiniBand specific information.
 */
typedef struct IBINFO {
    int                 mtu;            /* MTU */
    int                 port;           /* Port */
    int                 rate;           /* Static rate */
    struct ibv_context *context;        /* Context */
    struct ibv_device **devlist;        /* Device list */
} IBINFO;


/*
 * Connection Manager specific information.
 */
typedef struct CMINFO {
    struct rdma_event_channel *channel; /* Channel */
    struct rdma_cm_id         *id;      /* RDMA id */
    struct rdma_cm_event      *event;   /* Event */
} CMINFO;


/*
 * RDMA device descriptor.
 */
typedef struct DEVICE {
    NODE             lnode;             /* Local node information */
    NODE             rnode;             /* Remote node information */
    IBINFO           ib;                /* InfiniBand information */
    CMINFO           cm;                /* Connection Manager information */
    uint32_t         qkey;              /* Q Key for UD */
    int              trans;             /* QP transport */
    int              msg_size;          /* Message size */
    int              buf_size;          /* Buffer size */
    int              max_send_wr;       /* Maximum send work requests */
    int              max_recv_wr;       /* Maximum receive work requests */
    int              max_inline;        /* Maximum amount of inline data */
    char            *buffer;            /* Buffer */
    ibv_cc          *channel;           /* Channel */
    struct ibv_pd   *pd;                /* Protection domain */
    struct ibv_mr   *mr;                /* Memory region */
    struct ibv_cq   *cq;                /* Completion queue */
    struct ibv_qp   *qp;                /* Queue Pair */
    struct ibv_ah   *ah;                /* Address handle */
    struct ibv_srq  *srq;               /* Shared receive queue */
    ibv_xrc         *xrc;               /* XRC domain */
} DEVICE;


/*
 * Names associated with a value.
 */
typedef struct NAMES {
    int     value;                       /* Value */
    char    *name;                       /* Name */
} NAMES;


/*
 * RDMA speeds and names.
 */
typedef struct RATES {
    const char *name;                   /* Name */
    uint32_t    rate;                   /* Rate */
} RATES;


/*
 * Function prototypes.
 */
static void     atomic_seq(ATOMIC atomic, int i,
                                            uint64_t *value, uint64_t *args);
static void     cm_ack_event(DEVICE *dev);
static void     cm_close(DEVICE *dev);
static char    *cm_event_name(int event, char *data, int size);
static void     cm_expect_event(DEVICE *dev, int expected);
static void     cm_init(DEVICE *dev);
static void     cm_open(DEVICE *dev);
static void     cm_open_client(DEVICE *dev);
static void     cm_open_server(DEVICE *dev);
static void     cm_prep(DEVICE *dev);
static void     cq_error(int status);
static void     dec_node(NODE *host);
static void     do_error(int status, uint64_t *errors);
static void     enc_node(NODE *host);
static void     ib_client_atomic(ATOMIC atomic);
static void     ib_client_verify_atomic(ATOMIC atomic);
static void     ib_close1(DEVICE *dev);
static void     ib_close2(DEVICE *dev);
static void     ib_migrate(DEVICE *dev);
static void     ib_open(DEVICE *dev);
static void     ib_post_atomic(DEVICE *dev, ATOMIC atomic, int wrid,
                            int offset, uint64_t compare_add, uint64_t swap);
static void     ib_prep(DEVICE *dev);
static void     rd_bi_bw(int transport);
static void     rd_client_bw(int transport);
static void     rd_client_rdma_bw(int transport, ibv_op opcode);
static void     rd_client_rdma_read_lat(int transport);
static void     rd_close(DEVICE *dev);
static void     rd_mralloc(DEVICE *dev, int size);
static void     rd_mrfree(DEVICE *dev);
static void     rd_open(DEVICE *dev, int trans, int max_send_wr, int max_recv_wr);
static void     rd_params(int transport, long msg_size, int poll, int atomic);
static int      rd_poll(DEVICE *dev, struct ibv_wc *wc, int nwc);
static void     rd_post_rdma_std(DEVICE *dev, ibv_op opcode, int n);
static void     rd_post_recv_std(DEVICE *dev, int n);
static void     rd_post_send(DEVICE *dev, int off, int len,
                                                int inc, int rep, int stat);
static void     rd_post_send_std(DEVICE *dev, int n);
static void     rd_pp_lat(int transport, IOMODE iomode);
static void     rd_pp_lat_loop(DEVICE *dev, IOMODE iomode);
static void     rd_prep(DEVICE *dev, int size);
static void     rd_rdma_write_poll_lat(int transport);
static void     rd_server_def(int transport);
static void     rd_server_nop(int transport, int size);
static int      maybe(int val, char *msg);
static char    *opcode_name(int opcode);
static void     show_node_info(DEVICE *dev);


/*
 * List of errors we can get from a CQE.
 */
NAMES CQErrors[] ={
    { IBV_WC_SUCCESS,                   "Success"                       },
    { IBV_WC_LOC_LEN_ERR,               "Local length error"            },
    { IBV_WC_LOC_QP_OP_ERR,             "Local QP operation failure"    },
    { IBV_WC_LOC_EEC_OP_ERR,            "Local EEC operation failure"   },
    { IBV_WC_LOC_PROT_ERR,              "Local protection error"        },
    { IBV_WC_WR_FLUSH_ERR,              "WR flush failure"              },
    { IBV_WC_MW_BIND_ERR,               "Memory window bind failure"    },
    { IBV_WC_BAD_RESP_ERR,              "Bad response"                  },
    { IBV_WC_LOC_ACCESS_ERR,            "Local access failure"          },
    { IBV_WC_REM_INV_REQ_ERR,           "Remote invalid request"        },
    { IBV_WC_REM_ACCESS_ERR,            "Remote access failure"         },
    { IBV_WC_REM_OP_ERR,                "Remote operation failure"      },
    { IBV_WC_RETRY_EXC_ERR,             "Retries exceeded"              },
    { IBV_WC_RNR_RETRY_EXC_ERR,         "RNR retry exceeded"            },
    { IBV_WC_LOC_RDD_VIOL_ERR,          "Local RDD violation"           },
    { IBV_WC_REM_INV_RD_REQ_ERR,        "Remote invalid read request"   },
    { IBV_WC_REM_ABORT_ERR,             "Remote abort"                  },
    { IBV_WC_INV_EECN_ERR,              "Invalid EECN"                  },
    { IBV_WC_INV_EEC_STATE_ERR,         "Invalid EEC state"             },
    { IBV_WC_FATAL_ERR,                 "Fatal error"                   },
    { IBV_WC_RESP_TIMEOUT_ERR,          "Responder timeout"             },
    { IBV_WC_GENERAL_ERR,               "General error"                 },
};


/*
 * Opcodes.
 */
NAMES Opcodes[] ={
    { IBV_WR_ATOMIC_CMP_AND_SWP,        "compare and swap"              },
    { IBV_WR_ATOMIC_FETCH_AND_ADD,      "fetch and add"                 },
    { IBV_WR_RDMA_READ,                 "rdma read"                     },
    { IBV_WR_RDMA_WRITE,                "rdma write"                    },
    { IBV_WR_RDMA_WRITE_WITH_IMM,       "rdma write with immediate"     },
    { IBV_WR_SEND,                      "send"                          },
    { IBV_WR_SEND_WITH_IMM,             "send with immediate"           },
};


/*
 * Events from the Connection Manager.
 */
NAMES CMEvents[] ={
    { RDMA_CM_EVENT_ADDR_RESOLVED,      "Address resolved"              },
    { RDMA_CM_EVENT_ADDR_ERROR,         "Address error"                 },
    { RDMA_CM_EVENT_ROUTE_RESOLVED,     "Route resolved"                },
    { RDMA_CM_EVENT_ROUTE_ERROR,        "Route error"                   },
    { RDMA_CM_EVENT_CONNECT_REQUEST,    "Connect request"               },
    { RDMA_CM_EVENT_CONNECT_RESPONSE,   "Connect response"              },
    { RDMA_CM_EVENT_CONNECT_ERROR,      "Connect error"                 },
    { RDMA_CM_EVENT_UNREACHABLE,        "Event unreachable"             },
    { RDMA_CM_EVENT_REJECTED,           "Event rejected"                },
    { RDMA_CM_EVENT_ESTABLISHED,        "Event established"             },
    { RDMA_CM_EVENT_DISCONNECTED,       "Event disconnected"            },
    { RDMA_CM_EVENT_DEVICE_REMOVAL,     "Device removal"                },
    { RDMA_CM_EVENT_MULTICAST_JOIN,     "Multicast join"                },
    { RDMA_CM_EVENT_MULTICAST_ERROR,    "Multicast error"               },
};


/*
 * Opcodes.
 */
RATES Rates[] ={
    { "",       IBV_RATE_MAX        },
    { "max",    IBV_RATE_MAX        },
    { "1xSDR",  IBV_RATE_2_5_GBPS   },
    { "1xDDR",  IBV_RATE_5_GBPS     },
    { "1xQDR",  IBV_RATE_10_GBPS    },
    { "4xSDR",  IBV_RATE_10_GBPS    },
    { "4xDDR",  IBV_RATE_20_GBPS    },
    { "4xQDR",  IBV_RATE_40_GBPS    },
    { "8xSDR",  IBV_RATE_20_GBPS    },
    { "8xDDR",  IBV_RATE_40_GBPS    },
    { "8xQDR",  IBV_RATE_80_GBPS    },
    { "2.5",    IBV_RATE_2_5_GBPS   },
    { "5",      IBV_RATE_5_GBPS     },
    { "10",     IBV_RATE_10_GBPS    },
    { "20",     IBV_RATE_20_GBPS    },
    { "30",     IBV_RATE_30_GBPS    },
    { "40",     IBV_RATE_40_GBPS    },
    { "60",     IBV_RATE_60_GBPS    },
    { "80",     IBV_RATE_80_GBPS    },
    { "120",    IBV_RATE_120_GBPS   },
};


/*
 * This routine is never called and is solely to avoid compiler warnings for
 * functions that are not currently being used.
 */
void
rdma_not_called(void)
{
    if (0)
        ib_migrate(NULL);
}


/*
 * Measure RC bi-directional bandwidth (client side).
 */
void
run_client_rc_bi_bw(void)
{
    par_use(L_ACCESS_RECV);
    par_use(R_ACCESS_RECV);
    rd_params(IBV_QPT_RC, K64, 1, 0);
    rd_bi_bw(IBV_QPT_RC);
    show_results(BANDWIDTH);
}


/*
 * Measure RC bi-directional bandwidth (server side).
 */
void
run_server_rc_bi_bw(void)
{
    rd_bi_bw(IBV_QPT_RC);
}


/*
 * Measure RC bandwidth (client side).
 */
void
run_client_rc_bw(void)
{
    par_use(L_ACCESS_RECV);
    par_use(R_ACCESS_RECV);
    par_use(L_NO_MSGS);
    par_use(R_NO_MSGS);
    rd_params(IBV_QPT_RC, K64, 1, 0);
    rd_client_bw(IBV_QPT_RC);
    show_results(BANDWIDTH);
}


/*
 * Measure RC bandwidth (server side).
 */
void
run_server_rc_bw(void)
{
    rd_server_def(IBV_QPT_RC);
}


/*
 * Measure RC compare and swap messaging rate (client side).
 */
void
run_client_rc_compare_swap_mr(void)
{
    ib_client_atomic(COMPARE_SWAP);
}


/*
 * Measure RC compare and swap messaging rate (server side).
 */
void
run_server_rc_compare_swap_mr(void)
{
    rd_server_nop(IBV_QPT_RC, sizeof(uint64_t));
}


/*
 * Measure RC fetch and add messaging rate (client side).
 */
void
run_client_rc_fetch_add_mr(void)
{
    ib_client_atomic(FETCH_ADD);
}


/*
 * Measure RC fetch and add messaging rate (server side).
 */
void
run_server_rc_fetch_add_mr(void)
{
    rd_server_nop(IBV_QPT_RC, sizeof(uint64_t));
}


/*
 * Measure RC latency (client side).
 */
void
run_client_rc_lat(void)
{
    rd_params(IBV_QPT_RC, 1, 1, 0);
    rd_pp_lat(IBV_QPT_RC, IO_SR);
}


/*
 * Measure RC latency (server side).
 */
void
run_server_rc_lat(void)
{
    rd_pp_lat(IBV_QPT_RC, IO_SR);
}


/*
 * Measure RC RDMA read bandwidth (client side).
 */
void
run_client_rc_rdma_read_bw(void)
{
    par_use(L_ACCESS_RECV);
    par_use(R_ACCESS_RECV);
    par_use(L_RD_ATOMIC);
    par_use(R_RD_ATOMIC);
    rd_params(IBV_QPT_RC, K64, 1, 0);
    rd_client_rdma_bw(IBV_QPT_RC, IBV_WR_RDMA_READ);
    show_results(BANDWIDTH);
}


/*
 * Measure RC RDMA read bandwidth (server side).
 */
void
run_server_rc_rdma_read_bw(void)
{
    rd_server_nop(IBV_QPT_RC, 0);
}


/*
 * Measure RC RDMA read latency (client side).
 */
void
run_client_rc_rdma_read_lat(void)
{
    rd_params(IBV_QPT_RC, 1, 1, 0);
    rd_client_rdma_read_lat(IBV_QPT_RC);
}


/*
 * Measure RC RDMA read latency (server side).
 */
void
run_server_rc_rdma_read_lat(void)
{
    rd_server_nop(IBV_QPT_RC, 0);
}


/*
 * Measure RC RDMA write bandwidth (client side).
 */
void
run_client_rc_rdma_write_bw(void)
{
    rd_params(IBV_QPT_RC, K64, 1, 0);
    rd_client_rdma_bw(IBV_QPT_RC, IBV_WR_RDMA_WRITE_WITH_IMM);
    show_results(BANDWIDTH);
}


/*
 * Measure RC RDMA write bandwidth (server side).
 */
void
run_server_rc_rdma_write_bw(void)
{
    rd_server_def(IBV_QPT_RC);
}


/*
 * Measure RC RDMA write latency (client side).
 */
void
run_client_rc_rdma_write_lat(void)
{
    rd_params(IBV_QPT_RC, 1, 1, 0);
    rd_pp_lat(IBV_QPT_RC, IO_RDMA);
}


/*
 * Measure RC RDMA write latency (server side).
 */
void
run_server_rc_rdma_write_lat(void)
{
    rd_pp_lat(IBV_QPT_RC, IO_RDMA);
}


/*
 * Measure RC RDMA write polling latency (client side).
 */
void
run_client_rc_rdma_write_poll_lat(void)
{
    rd_params(IBV_QPT_RC, 1, 0, 0);
    rd_rdma_write_poll_lat(IBV_QPT_RC);
    show_results(LATENCY);
}


/*
 * Measure RC RDMA write polling latency (server side).
 */
void
run_server_rc_rdma_write_poll_lat(void)
{
    rd_rdma_write_poll_lat(IBV_QPT_RC);
}


/*
 * Measure UC bi-directional bandwidth (client side).
 */
void
run_client_uc_bi_bw(void)
{
    par_use(L_ACCESS_RECV);
    par_use(R_ACCESS_RECV);
    rd_params(IBV_QPT_UC, K64, 1, 0);
    rd_bi_bw(IBV_QPT_UC);
    show_results(BANDWIDTH_SR);
}


/*
 * Measure UC bi-directional bandwidth (server side).
 */
void
run_server_uc_bi_bw(void)
{
    rd_bi_bw(IBV_QPT_UC);
}


/*
 * Measure UC bandwidth (client side).
 */
void
run_client_uc_bw(void)
{
    par_use(L_ACCESS_RECV);
    par_use(R_ACCESS_RECV);
    par_use(L_NO_MSGS);
    par_use(R_NO_MSGS);
    rd_params(IBV_QPT_UC, K64, 1, 0);
    rd_client_bw(IBV_QPT_UC);
    show_results(BANDWIDTH_SR);
}


/*
 * Measure UC bandwidth (server side).
 */
void
run_server_uc_bw(void)
{
    rd_server_def(IBV_QPT_UC);
}


/*
 * Measure UC latency (client side).
 */
void
run_client_uc_lat(void)
{
    rd_params(IBV_QPT_UC, 1, 1, 0);
    rd_pp_lat(IBV_QPT_UC, IO_SR);
}


/*
 * Measure UC latency (server side).
 */
void
run_server_uc_lat(void)
{
    rd_pp_lat(IBV_QPT_UC, IO_SR);
}


/*
 * Measure UC RDMA write bandwidth (client side).
 */
void
run_client_uc_rdma_write_bw(void)
{
    rd_params(IBV_QPT_UC, K64, 1, 0);
    rd_client_rdma_bw(IBV_QPT_UC, IBV_WR_RDMA_WRITE_WITH_IMM);
    show_results(BANDWIDTH_SR);
}


/*
 * Measure UC RDMA write bandwidth (server side).
 */
void
run_server_uc_rdma_write_bw(void)
{
    rd_server_def(IBV_QPT_UC);
}


/*
 * Measure UC RDMA write latency (client side).
 */
void
run_client_uc_rdma_write_lat(void)
{
    rd_params(IBV_QPT_UC, 1, 1, 0);
    rd_pp_lat(IBV_QPT_UC, IO_RDMA);
}


/*
 * Measure UC RDMA write latency (server side).
 */
void
run_server_uc_rdma_write_lat(void)
{
    rd_pp_lat(IBV_QPT_UC, IO_RDMA);
}


/*
 * Measure UC RDMA write polling latency (client side).
 */
void
run_client_uc_rdma_write_poll_lat(void)
{
    rd_params(IBV_QPT_UC, 1, 1, 0);
    rd_rdma_write_poll_lat(IBV_QPT_UC);
    show_results(LATENCY);
}


/*
 * Measure UC RDMA write polling latency (server side).
 */
void
run_server_uc_rdma_write_poll_lat(void)
{
    rd_rdma_write_poll_lat(IBV_QPT_UC);
}


/*
 * Measure UD bi-directional bandwidth (client side).
 */
void
run_client_ud_bi_bw(void)
{
    par_use(L_ACCESS_RECV);
    par_use(R_ACCESS_RECV);
    rd_params(IBV_QPT_UD, K2, 1, 0);
    rd_bi_bw(IBV_QPT_UD);
    show_results(BANDWIDTH_SR);
}


/*
 * Measure UD bi-directional bandwidth (server side).
 */
void
run_server_ud_bi_bw(void)
{
    rd_bi_bw(IBV_QPT_UD);
}


/*
 * Measure UD bandwidth (client side).
 */
void
run_client_ud_bw(void)
{
    par_use(L_ACCESS_RECV);
    par_use(R_ACCESS_RECV);
    par_use(L_NO_MSGS);
    par_use(R_NO_MSGS);
    rd_params(IBV_QPT_UD, K2, 1, 0);
    rd_client_bw(IBV_QPT_UD);
    show_results(BANDWIDTH_SR);
}


/*
 * Measure UD bandwidth (server side).
 */
void
run_server_ud_bw(void)
{
    rd_server_def(IBV_QPT_UD);
}


/*
 * Measure UD latency (client side).
 */
void
run_client_ud_lat(void)
{
    rd_params(IBV_QPT_UD, 1, 1, 0);
    rd_pp_lat(IBV_QPT_UD, IO_SR);
}


/*
 * Measure UD latency (server side).
 */
void
run_server_ud_lat(void)
{
    rd_pp_lat(IBV_QPT_UD, IO_SR);
}


/*
 * Measure XRC bi-directional bandwidth (client side).
 */
void
run_client_xrc_bi_bw(void)
{
    par_use(L_ACCESS_RECV);
    par_use(R_ACCESS_RECV);
    rd_params(IBV_QPT_XRC, K64, 1, 0);
    rd_bi_bw(IBV_QPT_XRC);
    show_results(BANDWIDTH);
}


/*
 * Measure XRC bi-directional bandwidth (server side).
 */
void
run_server_xrc_bi_bw(void)
{
    rd_bi_bw(IBV_QPT_XRC);
}


/*
 * Measure XRC bandwidth (client side).
 */
void
run_client_xrc_bw(void)
{
    par_use(L_ACCESS_RECV);
    par_use(R_ACCESS_RECV);
    par_use(L_NO_MSGS);
    par_use(R_NO_MSGS);
    rd_params(IBV_QPT_XRC, K64, 1, 0);
    rd_client_bw(IBV_QPT_XRC);
    show_results(BANDWIDTH);
}


/*
 * Measure XRC bandwidth (server side).
 */
void
run_server_xrc_bw(void)
{
    rd_server_def(IBV_QPT_XRC);
}


/*
 * Measure XRC latency (client side).
 */
void
run_client_xrc_lat(void)
{
    rd_params(IBV_QPT_XRC, 1, 1, 0);
    rd_pp_lat(IBV_QPT_XRC, IO_SR);
}


/*
 * Measure XRC latency (server side).
 */
void
run_server_xrc_lat(void)
{
    rd_pp_lat(IBV_QPT_XRC, IO_SR);
}

/*
 * Verify RC compare and swap (client side).
 */
void
run_client_ver_rc_compare_swap(void)
{
    ib_client_verify_atomic(COMPARE_SWAP);
}


/*
 * Verify RC compare and swap (server side).
 */
void
run_server_ver_rc_compare_swap(void)
{
    rd_server_nop(IBV_QPT_RC, sizeof(uint64_t));
}


/*
 * Verify RC fetch and add (client side).
 */
void
run_client_ver_rc_fetch_add(void)
{
    ib_client_verify_atomic(FETCH_ADD);
}


/*
 * Verify RC fetch and add (server side).
 */
void
run_server_ver_rc_fetch_add(void)
{
    rd_server_nop(IBV_QPT_RC, sizeof(uint64_t));
}


/*
 * Measure RDMA bandwidth (client side).
 */
static void
rd_client_bw(int transport)
{
    DEVICE dev;
    long sent = 0;

    rd_open(&dev, transport, NCQE, 0);
    rd_prep(&dev, 0);
    sync_test();
    rd_post_send_std(&dev, left_to_send(&sent, NCQE));
    sent = NCQE;
    while (!Finished) {
        int i;
        struct ibv_wc wc[NCQE];
        int n = rd_poll(&dev, wc, cardof(wc));

        if (n > LStat.max_cqes)
            LStat.max_cqes = n;
        if (Finished)
            break;
        for (i = 0; i < n; ++i) {
            int id = wc[i].wr_id;
            int status = wc[i].status;

            if (id != WRID_SEND)
                debug("bad WR ID %d", id);
            else if (status != IBV_WC_SUCCESS)
                do_error(status, &LStat.s.no_errs);
        }
        if (Req.no_msgs) {
            if (LStat.s.no_msgs + LStat.s.no_errs >= Req.no_msgs)
                break;
            n = left_to_send(&sent, n);
        }
        rd_post_send_std(&dev, n);
        sent += n;
    }
    stop_test_timer();
    exchange_results();
    rd_close(&dev);
}


/*
 * Default action for the server is to post receive buffers and whenever it
 * gets a completion entry, compute statistics and post more buffers.
 */
static void
rd_server_def(int transport)
{
    DEVICE dev;

    rd_open(&dev, transport, 0, NCQE);
    rd_prep(&dev, 0);
    rd_post_recv_std(&dev, NCQE);
    sync_test();
    while (!Finished) {
        int i;
        struct ibv_wc wc[NCQE];
        int n = rd_poll(&dev, wc, cardof(wc));

        if (Finished)
            break;
        if (n > LStat.max_cqes)
            LStat.max_cqes = n;
        for (i = 0; i < n; ++i) {
            int status = wc[i].status;

            if (status == IBV_WC_SUCCESS) {
                LStat.r.no_bytes += dev.msg_size;
                LStat.r.no_msgs++;
                if (Req.access_recv)
                    touch_data(dev.buffer, dev.msg_size);
            } else
                do_error(status, &LStat.r.no_errs);
        }
        if (Req.no_msgs)
            if (LStat.r.no_msgs + LStat.r.no_errs >= Req.no_msgs)
                break;
        rd_post_recv_std(&dev, n);
    }
    stop_test_timer();
    exchange_results();
    rd_close(&dev);
}


/*
 * Measure bi-directional RDMA bandwidth.
 */
static void
rd_bi_bw(int transport)
{
    DEVICE dev;

    rd_open(&dev, transport, NCQE, NCQE);
    rd_prep(&dev, 0);
    rd_post_recv_std(&dev, NCQE);
    sync_test();
    rd_post_send_std(&dev, NCQE);
    while (!Finished) {
        int i;
        struct ibv_wc wc[NCQE];
        int numSent = 0;
        int numRecv = 0;
        int n = rd_poll(&dev, wc, cardof(wc));

        if (Finished)
            break;
        if (n > LStat.max_cqes)
            LStat.max_cqes = n;
        for (i = 0; i < n; ++i) {
            int id = wc[i].wr_id;
            int status = wc[i].status;

            switch (id) {
            case WRID_SEND:
                if (status != IBV_WC_SUCCESS)
                    do_error(status, &LStat.s.no_errs);
                ++numSent;
                break;
            case WRID_RECV:
                if (status == IBV_WC_SUCCESS) {
                    LStat.r.no_bytes += dev.msg_size;
                    LStat.r.no_msgs++;
                    if (Req.access_recv)
                        touch_data(dev.buffer, dev.msg_size);
                } else
                    do_error(status, &LStat.r.no_errs);
                ++numRecv;
                break;
            default:
                debug("bad WR ID %d", id);
            }
        }
        if (numRecv)
            rd_post_recv_std(&dev, numRecv);
        if (numSent)
            rd_post_send_std(&dev, numSent);
    }
    stop_test_timer();
    exchange_results();
    rd_close(&dev);
}


/*
 * Measure ping-pong latency (client and server side).
 */
static void
rd_pp_lat(int transport, IOMODE iomode)
{
    DEVICE dev;

    rd_open(&dev, transport, 1, 1);
    rd_prep(&dev, 0);
    rd_pp_lat_loop(&dev, iomode);
    stop_test_timer();
    exchange_results();
    rd_close(&dev);
    if (is_client())
        show_results(LATENCY);
}


/*
 * Loop sending packets back and forth to measure ping-pong latency.
 */
static void
rd_pp_lat_loop(DEVICE *dev, IOMODE iomode)
{
    int done = 1;

    rd_post_recv_std(dev, 1);
    sync_test();
    if (is_client()) {
        if (iomode == IO_SR)
            rd_post_send_std(dev, 1);
        else
            rd_post_rdma_std(dev, IBV_WR_RDMA_WRITE_WITH_IMM, 1);
        done = 0;
    }

    while (!Finished) {
        int i;
        struct ibv_wc wc[2];
        int n = rd_poll(dev, wc, cardof(wc));

        if (Finished)
            break;
        for (i = 0; i < n; ++i) {
            int id = wc[i].wr_id;
            int status = wc[i].status;

            switch (id) {
            case WRID_SEND:
            case WRID_RDMA:
                if (status != IBV_WC_SUCCESS)
                    do_error(status, &LStat.s.no_errs);
                done |= 1;
                continue;
            case WRID_RECV:
                if (status == IBV_WC_SUCCESS) {
                    LStat.r.no_bytes += dev->msg_size;
                    LStat.r.no_msgs++;
                    rd_post_recv_std(dev, 1);
                } else
                    do_error(status, &LStat.r.no_errs);
                done |= 2;
                continue;
            default:
                debug("bad WR ID %d", id);
                continue;
            }
            break;
        }
        if (done == 3) {
            if (iomode == IO_SR)
                rd_post_send_std(dev, 1);
            else
                rd_post_rdma_std(dev, IBV_WR_RDMA_WRITE_WITH_IMM, 1);
            done = 0;
        }
    }
}


/*
 * Loop sending packets back and forth using RDMA Write and polling to measure
 * latency.  This is the strategy used by some of the MPIs.  Note that it does
 * not matter what characters clientid and serverid are set to as long as they
 * are different.  Note also that we must set *p and *q before calling
 * sync_test to avoid a race condition.
 */
static void
rd_rdma_write_poll_lat(int transport)
{
    DEVICE dev;
    volatile unsigned char *p, *q;
    int send, locid, remid;
    int clientid = 0x55;
    int serverid = 0xaa;

    if (is_client())
        send = 1, locid = clientid, remid = serverid;
    else
        send = 0, locid = serverid, remid = clientid;
    rd_open(&dev, transport, NCQE, 0);
    rd_prep(&dev, 0);
    p = (unsigned char *)dev.buffer;
    q = p + dev.msg_size-1;
    *p = locid;
    *q = locid;
    sync_test();
    while (!Finished) {
        if (send) {
            int i;
            int n;
            struct ibv_wc wc[2];

            rd_post_rdma_std(&dev, IBV_WR_RDMA_WRITE, 1);
            if (Finished)
                break;
            n = ibv_poll_cq(dev.cq, cardof(wc), wc);
            if (n < 0)
                error(SYS, "CQ poll failed");
            for (i = 0; i < n; ++i) {
                int id = wc[i].wr_id;
                int status = wc[i].status;

                if (id != WRID_RDMA)
                    debug("bad WR ID %d", id);
                else if (status != IBV_WC_SUCCESS)
                    do_error(status, &LStat.s.no_errs);
            }
        }
        while (!Finished)
            if (*p == remid && *q == remid)
                break;
        LStat.r.no_bytes += dev.msg_size;
        LStat.r.no_msgs++;
        *p = locid;
        *q = locid;
        send = 1;
    }
    stop_test_timer();
    exchange_results();
    rd_close(&dev);
}


/*
 * Measure RDMA Read latency (client side).
 */
static void
rd_client_rdma_read_lat(int transport)
{
    DEVICE dev;

    rd_open(&dev, transport, 1, 0);
    rd_prep(&dev, 0);
    sync_test();
    rd_post_rdma_std(&dev, IBV_WR_RDMA_READ, 1);
    while (!Finished) {
        struct ibv_wc wc;
        int n = rd_poll(&dev, &wc, 1);

        if (n == 0)
            continue;
        if (Finished)
            break;
        if (wc.wr_id != WRID_RDMA) {
            debug("bad WR ID %d", (int)wc.wr_id);
            continue;
        }
        if (wc.status == IBV_WC_SUCCESS) {
            LStat.r.no_bytes += dev.msg_size;
            LStat.r.no_msgs++;
            LStat.rem_s.no_bytes += dev.msg_size;
            LStat.rem_s.no_msgs++;
        } else
            do_error(wc.status, &LStat.s.no_errs);
        rd_post_rdma_std(&dev, IBV_WR_RDMA_READ, 1);
    }
    stop_test_timer();
    exchange_results();
    rd_close(&dev);
    show_results(LATENCY);
}


/*
 * Measure RDMA bandwidth (client side).
 */
static void
rd_client_rdma_bw(int transport, ibv_op opcode)
{
    DEVICE dev;

    rd_open(&dev, transport, NCQE, 0);
    rd_prep(&dev, 0);
    sync_test();
    rd_post_rdma_std(&dev, opcode, NCQE);
    while (!Finished) {
        int i;
        struct ibv_wc wc[NCQE];
        int n = rd_poll(&dev, wc, cardof(wc));

        if (Finished)
            break;
        if (n > LStat.max_cqes)
            LStat.max_cqes = n;
        for (i = 0; i < n; ++i) {
            int status = wc[i].status;

            if (status == IBV_WC_SUCCESS) {
                if (opcode == IBV_WR_RDMA_READ) {
                    LStat.r.no_bytes += dev.msg_size;
                    LStat.r.no_msgs++;
                    LStat.rem_s.no_bytes += dev.msg_size;
                    LStat.rem_s.no_msgs++;
                    if (Req.access_recv)
                        touch_data(dev.buffer, dev.msg_size);
                }
            } else
                do_error(status, &LStat.s.no_errs);
        }
        rd_post_rdma_std(&dev, opcode, n);
    }
    stop_test_timer();
    exchange_results();
    rd_close(&dev);
}


/*
 * Server just waits and lets driver take care of any requests.
 */
static void
rd_server_nop(int transport, int size)
{
    DEVICE dev;

    /* workaround: Size of RQ should be 0; bug in Mellanox driver */
    rd_open(&dev, transport, 0, 1);
    rd_prep(&dev, size);
    sync_test();
    while (!Finished)
        pause();
    stop_test_timer();
    exchange_results();
    rd_close(&dev);
}


/*
 * Measure messaging rate for an atomic operation.
 */
static void
ib_client_atomic(ATOMIC atomic)
{
    int i;
    DEVICE dev;

    rd_params(IBV_QPT_RC, 0, 1, 1);
    rd_open(&dev, IBV_QPT_RC, NCQE, 0);
    rd_prep(&dev, sizeof(uint64_t));
    sync_test();

    for (i = 0; i < NCQE; ++i) {
        if (Finished)
            break;
        ib_post_atomic(&dev, atomic, 0, 0, 0, 0);
    }

    while (!Finished) {
        struct ibv_wc wc[NCQE];
        int n = rd_poll(&dev, wc, cardof(wc));

        if (Finished)
            break;
        if (n > LStat.max_cqes)
            LStat.max_cqes = n;
        for (i = 0; i < n; ++i) {
            int status = wc[i].status;

            if (status == IBV_WC_SUCCESS) {
                LStat.rem_r.no_bytes += sizeof(uint64_t);
                LStat.rem_r.no_msgs++;
            } else
                do_error(status, &LStat.s.no_errs);
            ib_post_atomic(&dev, atomic, 0, 0, 0, 0);
        }
    }

    stop_test_timer();
    exchange_results();
    rd_close(&dev);
    show_results(MSG_RATE);
}


/*
 * Verify RC compare and swap (client side).
 */
static void
ib_client_verify_atomic(ATOMIC atomic)
{
    int i;
    int slots;
    DEVICE dev;
    int head = 0;
    int tail = 0;
    uint64_t args[2] = {0};

    rd_params(IBV_QPT_RC, K64, 1, 1);
    rd_open(&dev, IBV_QPT_RC, NCQE, 0);
    slots = Req.msg_size / sizeof(uint64_t);
    if (slots < 1)
        error(0, "message size must be at least %d", sizeof(uint64_t));
    if (slots > NCQE)
        slots = NCQE;
    rd_prep(&dev, 0);
    sync_test();

    for (i = 0; i < slots; ++i) {
        if (Finished)
            break;
        atomic_seq(atomic, head++, 0, args);
        ib_post_atomic(&dev, atomic, i, i*sizeof(uint64_t), args[0], args[1]);
    }

    while (!Finished) {
        struct ibv_wc wc[NCQE];
        int n = rd_poll(&dev, wc, cardof(wc));

        if (Finished)
            break;
        if (n > LStat.max_cqes)
            LStat.max_cqes = n;
        for (i = 0; i < n; ++i) {
            uint64_t seen;
            uint64_t want = 0;
            int x = wc[i].wr_id;
            int status = wc[i].status;

            if (status == IBV_WC_SUCCESS) {
                LStat.rem_r.no_bytes += sizeof(uint64_t);
                LStat.rem_r.no_msgs++;
            } else
                do_error(status, &LStat.s.no_errs);

            atomic_seq(atomic, tail++, &want, 0);
            seen = ((uint64_t *)dev.buffer)[x];
            if (seen != want) {
                error(0, "mismatch, sequence %d, expected %llx, got %llx",
                                    tail, (long long)want, (long long)seen);
            }
            atomic_seq(atomic, head++, 0, args);
            ib_post_atomic(&dev, atomic, x, x*sizeof(uint64_t),
                                                            args[0], args[1]);
        }
    }
    stop_test_timer();
    exchange_results();
    rd_close(&dev);
    show_results(MSG_RATE);
}


/*
 * Given an atomic operation and an index, return the next value associated
 * with that index and the arguments we might pass to post that atomic.
 */
static void
atomic_seq(ATOMIC atomic, int i, uint64_t *value, uint64_t *args)
{
    if (atomic == COMPARE_SWAP) {
        uint64_t v;
        uint64_t magic = 0x0123456789abcdefULL;

        v = i ? magic + i-1 : 0;
        if (value)
            *value = v;
        if (args) {
            args[0] = v;
            args[1] = magic + i;
        }
    } else if (atomic == FETCH_ADD) {
        if (value)
            *value = i;
        if (args)
            args[0] = 1;
    }
}


/*
 * Set default parameters.
 */
static void
rd_params(int transport, long msg_size, int poll, int atomic)
{
    //if (transport == IBV_QPT_RC || transport == IBV_QPT_UD) {
    if (transport == IBV_QPT_RC) {
        par_use(L_USE_CM);
        par_use(R_USE_CM);
    } else {
        setv_u32(L_USE_CM, 0);
        setv_u32(R_USE_CM, 0);
    }

    if (!Req.use_cm) {
        setp_u32(0, L_MTU_SIZE, MTU_SIZE);
        setp_u32(0, R_MTU_SIZE, MTU_SIZE);
        par_use(L_ID);
        par_use(R_ID);
        par_use(L_SL);
        par_use(R_SL);
        par_use(L_STATIC_RATE);
        par_use(R_STATIC_RATE);
        par_use(L_SRC_PATH_BITS);
        par_use(R_SRC_PATH_BITS);
    }

    if (msg_size) {
        setp_u32(0, L_MSG_SIZE, msg_size);
        setp_u32(0, R_MSG_SIZE, msg_size);
    }

    if (poll) {
        par_use(L_POLL_MODE);
        par_use(R_POLL_MODE);
    }

    if (atomic) {
        par_use(L_RD_ATOMIC);
        par_use(R_RD_ATOMIC);
    }
    opt_check();
}


/*
 * Open a RDMA device.
 */
static void
rd_open(DEVICE *dev, int trans, int max_send_wr, int max_recv_wr)
{
    /* Send request to client */
    if (is_client())
        client_send_request();

    /* Clear structure */
    memset(dev, 0, sizeof(*dev));

    /* Set transport type and maximum work request parameters */
    dev->trans = trans;
    dev->max_send_wr = max_send_wr;
    dev->max_recv_wr = max_recv_wr;

    /* Open device */
    if (Req.use_cm)
        cm_open(dev);
    else
        ib_open(dev);

    /* Get QP attributes */
    {
        struct ibv_qp_attr qp_attr;
        struct ibv_qp_init_attr qp_init_attr;

        if (ibv_query_qp(dev->qp, &qp_attr, 0, &qp_init_attr) != 0)
            error(SYS, "query QP failed");
        dev->max_inline = qp_attr.cap.max_inline_data;
    }
}


/*
 * Called after rd_open to prepare both ends.
 */
static void
rd_prep(DEVICE *dev, int size)
{
    /* Set the size of the messages we transfer */
    if (size == 0)
        dev->msg_size = Req.msg_size;

    /* Allocate memory region */
    if (size == 0)
        size = dev->msg_size;
    if (dev->trans == IBV_QPT_UD)
        size += GRH_SIZE;
    rd_mralloc(dev, size);

    /* Exchange node information */
    {
        NODE node;

        enc_init(&node);
        enc_node(&dev->lnode);
        send_mesg(&node, sizeof(node), "node information");
        recv_mesg(&node, sizeof(node), "node information");
        dec_init(&node);
        dec_node(&dev->rnode);
    }

    /* Second phase of open for devices */
    if (Req.use_cm) 
        cm_prep(dev);
    else
        ib_prep(dev);

    /* Request CQ notification if not polling */
    if (!Req.poll_mode) {
        if (ibv_req_notify_cq(dev->cq, 0) != 0)
            error(SYS, "failed to request CQ notification");
    }

    /* Show node information if debugging */
    show_node_info(dev);
}


/*
 * Show node information when debugging.
 */
static void
show_node_info(DEVICE *dev)
{
    NODE *n;

    if (!Debug)
        return;
    n = &dev->lnode;

    if (Req.use_cm) 
        debug("L: rkey=%08x vaddr=%010x", n->rkey, n->vaddr);
    else if (dev->trans == IBV_QPT_XRC) {
        debug("L: lid=%04x qpn=%06x psn=%06x rkey=%08x vaddr=%010x srqn=%08x",
                        n->lid, n->qpn, n->psn, n->rkey, n->vaddr, n->srqn);
    } else {
        debug("L: lid=%04x qpn=%06x psn=%06x rkey=%08x vaddr=%010x",
                            n->lid, n->qpn, n->psn, n->rkey, n->vaddr);
    }

    n = &dev->rnode;
    if (Req.use_cm) 
        debug("R: rkey=%08x vaddr=%010x", n->rkey, n->vaddr);
    else if (dev->trans == IBV_QPT_XRC) {
        debug("R: lid=%04x qpn=%06x psn=%06x rkey=%08x vaddr=%010x srqn=%08x",
                            n->lid, n->qpn, n->psn, n->rkey, n->vaddr);
    } else {
        debug("R: lid=%04x qpn=%06x psn=%06x rkey=%08x vaddr=%010x",
                        n->lid, n->qpn, n->psn, n->rkey, n->vaddr, n->srqn);
    }
}


/*
 * Close a RDMA device.  We must destroy the CQ before the QP otherwise the
 * ibv_destroy_qp call seems to sometimes hang.  We must also destroy the QP
 * before destroying the memory region as we cannot destroy the memory region
 * if there are references still outstanding.  Hopefully we now have things in
 * the right order.
 */
static void
rd_close(DEVICE *dev)
{
    if (Req.use_cm)
        cm_close(dev);
    else
        ib_close1(dev);

    if (dev->ah)
        ibv_destroy_ah(dev->ah);
    if (dev->cq)
        ibv_destroy_cq(dev->cq);
    if (dev->pd)
        ibv_dealloc_pd(dev->pd);
    if (dev->channel)
        ibv_destroy_comp_channel(dev->channel);
    rd_mrfree(dev);

    if (!Req.use_cm)
        ib_close2(dev);

    memset(dev, 0, sizeof(*dev));
}


/*
 * Create a queue pair.
 */
static void
rd_create_qp(DEVICE *dev, struct ibv_context *context, struct rdma_cm_id *id)
{
    /* Set up and verify rd_atomic parameters */
    {
        struct ibv_device_attr dev_attr;

        if (ibv_query_device(context, &dev_attr) != SUCCESS0)
            error(SYS, "query device failed");
        if (Req.rd_atomic == 0)
            dev->lnode.rd_atomic = dev_attr.max_qp_rd_atom;
        else if (Req.rd_atomic <= dev_attr.max_qp_rd_atom)
            dev->lnode.rd_atomic = Req.rd_atomic;
        else
            error(0, "device only supports %d (< %d) RDMA reads or atomics",
                                    dev_attr.max_qp_rd_atom, Req.rd_atomic);
    }

    /* Allocate completion channel */
    dev->channel = ibv_create_comp_channel(context);
    if (!dev->channel)
        error(SYS, "failed to create completion channel");

    /* Allocate protection domain */
    dev->pd = ibv_alloc_pd(context);
    if (!dev->pd)
        error(SYS, "failed to allocate protection domain");

    /* Create completion queue */
    dev->cq = ibv_create_cq(context,
                        dev->max_send_wr+dev->max_recv_wr, 0, dev->channel, 0);
    if (!dev->cq)
        error(SYS, "failed to create completion queue");

    /* Create queue pair */
    {
        struct ibv_qp_init_attr qp_attr ={
            .send_cq = dev->cq,
            .recv_cq = dev->cq,
            .cap     ={
                .max_send_wr     = dev->max_send_wr,
                .max_recv_wr     = dev->max_recv_wr,
                .max_send_sge    = 1,
                .max_recv_sge    = 1,
            },
            .qp_type = dev->trans
        };

        if (Req.use_cm) {
            if (rdma_create_qp(id, dev->pd, &qp_attr) != 0)
                error(SYS, "failed to create QP");
            dev->qp = id->qp;
        } else {
            if (dev->trans == IBV_QPT_XRC) {
                struct ibv_srq_init_attr srq_attr ={
                    .attr ={
                        .max_wr  = dev->max_recv_wr,
                        .max_sge = 1
                    }
                };

                dev->xrc = ibv_open_xrc_domain(context, -1, O_CREAT);
                if (!dev->xrc)
                    error(SYS, "failed to open XRC domain");

                dev->srq = ibv_create_xrc_srq(dev->pd, dev->xrc, dev->cq,
                                                                    &srq_attr);
                if (!dev->srq)
                    error(SYS, "failed to create SRQ");

                qp_attr.cap.max_recv_wr  = 0;
                qp_attr.cap.max_recv_sge = 0;
                qp_attr.xrc_domain       = dev->xrc;
            }

            dev->qp = ibv_create_qp(dev->pd, &qp_attr);
            if (!dev->qp)
                error(SYS, "failed to create QP");
        }
    }
}


/*
 * Allocate a memory region and register it.  I thought this routine should
 * never be called with a size of 0 as prior code checks for that and sets it
 * to some default value.  I appear to be wrong.  In that case, size is set to
 * 1 so other code does not break.
 */
static void
rd_mralloc(DEVICE *dev, int size)
{
    int flags;
    int pagesize;

    if (dev->buffer)
        error(BUG, "rd_mralloc: memory region already allocated");
    if (size == 0)
        size = 1;

    pagesize = sysconf(_SC_PAGESIZE);
    if (posix_memalign((void **)&dev->buffer, pagesize, size) != 0)
        error(SYS, "failed to allocate memory");
    memset(dev->buffer, 0, size);
    dev->buf_size = size;
    flags = IBV_ACCESS_LOCAL_WRITE  |
            IBV_ACCESS_REMOTE_READ  |
            IBV_ACCESS_REMOTE_WRITE |
            IBV_ACCESS_REMOTE_ATOMIC;
    dev->mr = ibv_reg_mr(dev->pd, dev->buffer, size, flags);
    if (!dev->mr)
        error(SYS, "failed to allocate memory region");
    dev->lnode.rkey = dev->mr->rkey;
    dev->lnode.vaddr = (unsigned long)dev->buffer;
}


/*
 * Free the memory region.
 */
static void
rd_mrfree(DEVICE *dev)
{
    if (dev->mr)
        ibv_dereg_mr(dev->mr);
    dev->mr = NULL;

    if (dev->buffer)
        free(dev->buffer);
    dev->buffer = NULL;
    dev->buf_size = 0;

    dev->lnode.rkey = 0;
    dev->lnode.vaddr = 0;
}


/*
 * Open a device using the Connection Manager.
 */
static void
cm_open(DEVICE *dev)
{
    cm_init(dev);
    if (is_client())
        cm_open_client(dev);
    else
        cm_open_server(dev);
}


/*
 * Open a channel to report communication events and allocate a communication
 * id.
 */
static void
cm_init(DEVICE *dev)
{
    CMINFO *cm = &dev->cm;
    int portspace = (dev->trans == IBV_QPT_RC) ? RDMA_PS_TCP : RDMA_PS_UDP;

    cm->channel = rdma_create_event_channel();
    if (!cm->channel)
        error(0, "rdma_create_event_channel failed");
    if (rdma_create_id(cm->channel, &cm->id, 0, portspace) != 0)
        error(0, "rdma_create_id failed");
}


/*
 * Open a device using the Connection Manager when we are the client.
 */
static void
cm_open_client(DEVICE *dev)
{
    AI *aip;
    uint32_t port;
    struct addrinfo hints ={
        .ai_family   = AF_INET,
        .ai_socktype = SOCK_STREAM
    };
    int timeout = Req.timeout * 1000;
    CMINFO *cm = &dev->cm;

    recv_mesg(&port, sizeof(port), "RDMA CM TCP IPv4 server port");
    port = decode_uint32(&port);
    aip = getaddrinfo_port(ServerName, port, &hints);
    cm_init(dev);

    if (rdma_resolve_addr(cm->id, 0, (SA *)aip->ai_addr, timeout) != 0)
        error(0, "rdma_resolve_addr failed");
    freeaddrinfo(aip);
    cm_expect_event(dev, RDMA_CM_EVENT_ADDR_RESOLVED);
    cm_ack_event(dev);

    if (rdma_resolve_route(cm->id, timeout) != 0)
        error(0, "rdma_resolve_route failed");
    cm_expect_event(dev, RDMA_CM_EVENT_ROUTE_RESOLVED);
    cm_ack_event(dev);
    rd_create_qp(dev, cm->id->verbs, cm->id);

    if (dev->trans == IBV_QPT_RC) {
        struct rdma_conn_param param ={
            .responder_resources = 1,
            .initiator_depth     = 1,
            .rnr_retry_count     = RNR_RETRY_CNT,
            .retry_count         = RETRY_CNT
        };

        if (rdma_connect(cm->id, &param) != 0)
            error(0, "rdma_connect failed");
        cm_expect_event(dev, RDMA_CM_EVENT_ESTABLISHED);
        cm_ack_event(dev);
    } else if (dev->trans == IBV_QPT_UD) {
        struct rdma_conn_param param ={
            .qp_num = cm->id->qp->qp_num
        };

        if (rdma_connect(cm->id, &param) != 0)
            error(0, "rdma_connect failed");
        cm_expect_event(dev, RDMA_CM_EVENT_ESTABLISHED);
        dev->qkey = cm->event->param.ud.qkey;
        dev->ah = ibv_create_ah(dev->pd, &cm->event->param.ud.ah_attr);
        if (!dev->ah)
            error(SYS, "failed to create address handle");
        cm_ack_event(dev);
    } else
        error(BUG, "cm_open_client: bad transport: %d", dev->trans);
}


/*
 * Open a device using the Connection Manager when we are the client.
 */
static void
cm_open_server(DEVICE *dev)
{
    uint32_t port;
    struct sockaddr_in saddr ={
        .sin_family      = AF_INET,
        .sin_addr.s_addr = htonl(INADDR_ANY),
        .sin_port        = htons(0)
    };
    CMINFO *cm = &dev->cm;

    if (rdma_bind_addr(cm->id, (SA *)&saddr) != 0)
        error(0, "rdma_bind_addr failed");
    port = ntohs(rdma_get_src_port(cm->id));
    encode_uint32(&port, port);
    send_mesg(&port, sizeof(port), "RDMA CM TCP IPv4 server port");

    if (rdma_listen(cm->id, 0) != 0)
        error(0, "rdma_listen failed");
    cm_expect_event(dev, RDMA_CM_EVENT_CONNECT_REQUEST);
    rd_create_qp(dev, cm->event->id->verbs, cm->event->id);

    if (dev->trans == IBV_QPT_RC) {
        struct rdma_conn_param param ={
            .responder_resources = 1,
            .initiator_depth     = 1,
            .rnr_retry_count     = RNR_RETRY_CNT,
            .retry_count         = RETRY_CNT
        };
        struct ibv_qp_attr rtr_attr ={
            .min_rnr_timer = MIN_RNR_TIMER,
        };

        if (rdma_accept(cm->event->id, &param) != 0)
            error(0, "rdma_accept failed");
        cm_ack_event(dev);
        cm_expect_event(dev, RDMA_CM_EVENT_ESTABLISHED);
        cm_ack_event(dev);

        /* Do not complain on error as we might be on a iWARP device */
        ibv_modify_qp(dev->qp, &rtr_attr, IBV_QP_MIN_RNR_TIMER);
    } else if (dev->trans == IBV_QPT_UD) {
        struct rdma_conn_param param ={
            .qp_num = cm->event->id->qp->qp_num
        };

        if (rdma_accept(cm->event->id, &param) != 0)
            error(0, "rdma_accept failed");
        dev->qkey = cm->event->param.ud.qkey;
        dev->ah = ibv_create_ah(dev->pd, &cm->event->param.ud.ah_attr);
        if (!dev->ah)
            error(SYS, "failed to create address handle");
        cm_ack_event(dev);
    } else
        error(BUG, "cm_open_server: bad transport: %d", dev->trans);
}


/*
 * Prepare a device using the Connection Manager.  Final stage of open.
 */
static void
cm_prep(DEVICE *dev)
{
}


/*
 * Close a device using the Connection Manager.
 */
static void
cm_close(DEVICE *dev)
{
    if (is_client())
        if (rdma_disconnect(dev->cm.id) != 0)
            error(SYS, "rdma_disconnect failed");
    cm_expect_event(dev, RDMA_CM_EVENT_DISCONNECTED);
    cm_ack_event(dev);
    rdma_destroy_id(dev->cm.id);
    rdma_destroy_event_channel(dev->cm.channel);
}


/*
 * Get an event from the Connection Manager.  If it is not what we expect,
 * complain.
 */
static void
cm_expect_event(DEVICE *dev, int expected)
{
    char msg1[64];
    char msg2[64];
    CMINFO *cm = &dev->cm;

    if (rdma_get_cm_event(cm->channel, &cm->event) != 0)
        error(0, "failed to receive event from RDMA CM channel");
    if (cm->event->event != expected) {
        error(0, "unexpected event from RDMA CM: %s\n    expecting: %s",
                        cm_event_name(cm->event->event, msg1, sizeof(msg1)),
                        cm_event_name(expected, msg2, sizeof(msg2)));
    }
}


/*
 * Return a name given a RDMA CM event number.  We first look at our list.  If
 * that fails, we call the standard rdma_event_str routine.
 */
static char *
cm_event_name(int event, char *data, int size)
{
    int i;

    for (i = 0; i < cardof(CMEvents); ++i)
        if (event == CMEvents[i].value)
            return CMEvents[i].name;
    strncpy(data, rdma_event_str(event), size);
    data[size-1] = '\0';
    return data;
}


/*
 * Acknowledge and free a communication event.
 */
static void
cm_ack_event(DEVICE *dev)
{
    if (rdma_ack_cm_event(dev->cm.event) != 0)
        error(0, "rdma_ack_cm_event failed");
}


/*
 * Open an InfiniBand device.
 */
static void
ib_open(DEVICE *dev)
{
    /* Determine MTU */
    {
        int mtu = Req.mtu_size;

        if (mtu == 256)
            dev->ib.mtu = IBV_MTU_256;
        else if (mtu == 512)
            dev->ib.mtu = IBV_MTU_512;
        else if (mtu == 1024)
            dev->ib.mtu = IBV_MTU_1024;
        else if (mtu == 2048)
            dev->ib.mtu = IBV_MTU_2048;
        else if (mtu == 4096)
            dev->ib.mtu = IBV_MTU_4096;
        else
            error(0, "bad MTU: %d; must be 256/512/1K/2K/4K", mtu);
    }

    /* Determine port */
    {
        int port = 1;
        char *p = index(Req.id, ':');

        if (p) {
            *p++ = '\0';
            port = atoi(p);
            if (port < 1)
                error(0, "bad IB port: %d; must be at least 1", port);
        }
        dev->ib.port = port;
    }

    /* Determine static rate */
    {
        RATES *q = Rates;
        RATES *r = q + cardof(Rates);

        for (;; ++q) {
            if (q >= r)
                error(SYS, "bad static rate: %s", Req.static_rate);
            if (streq(Req.static_rate, q->name)) {
                dev->ib.rate = q->rate;
                break;
            }
        }
    }

    /* Set up Q Key */
    dev->qkey = QKEY;

    /* Open device */
    {
        struct ibv_device *device;
        char *name = Req.id[0] ? Req.id : 0;

        dev->ib.devlist = ibv_get_device_list(0);
        if (!dev->ib.devlist)
            error(SYS, "failed to find any InfiniBand devices");
        if (!name)
            device = *dev->ib.devlist;
        else {
            struct ibv_device **d = dev->ib.devlist;
            while ((device = *d++))
                if (streq(ibv_get_device_name(device), name))
                    break;
        }
        if (!device)
            error(SYS, "failed to find InfiniBand device");
        dev->ib.context = ibv_open_device(device);
        if (!dev->ib.context) {
            const char *s = ibv_get_device_name(device);
            error(SYS, "failed to open device %s", s);
        }
    }

    /* Set up local node LID */
    {
        struct ibv_port_attr port_attr;
        int stat = ibv_query_port(dev->ib.context, dev->ib.port, &port_attr);

        if (stat != 0)
            error(SYS, "query port failed");
        srand48(getpid()*time(0));
        dev->lnode.lid = port_attr.lid;
	if (port_attr.lmc > 0)
	    dev->lnode.lid += Req.src_path_bits & ((1 << port_attr.lmc) - 1);
    }

    /* Create QP */
    rd_create_qp(dev, dev->ib.context, 0);

    /* Modify queue pair to INIT state */
    {
        struct ibv_qp_attr attr ={
            .qp_state       = IBV_QPS_INIT,
            .pkey_index     = 0,
            .port_num       = dev->ib.port
        };
        int flags = IBV_QP_STATE | IBV_QP_PKEY_INDEX | IBV_QP_PORT;

        if (dev->trans == IBV_QPT_UD) {
            flags |= IBV_QP_QKEY;
            attr.qkey = dev->qkey;
        } else if (dev->trans == IBV_QPT_RC || dev->trans == IBV_QPT_XRC) {
            flags |= IBV_QP_ACCESS_FLAGS;
            attr.qp_access_flags =
                IBV_ACCESS_REMOTE_READ  |
                IBV_ACCESS_REMOTE_WRITE |
                IBV_ACCESS_REMOTE_ATOMIC;
        } else if (dev->trans == IBV_QPT_UC) {
            flags |= IBV_QP_ACCESS_FLAGS;
            attr.qp_access_flags = IBV_ACCESS_REMOTE_WRITE;
        }
        if (ibv_modify_qp(dev->qp, &attr, flags) != SUCCESS0)
            error(SYS, "failed to modify QP to INIT state");
    }

    /* Set up local node QP number, PSN and SRQ number */
    dev->lnode.qpn = dev->qp->qp_num;
    dev->lnode.psn = lrand48() & 0xffffff;
    if (dev->trans == IBV_QPT_XRC)
        dev->lnode.srqn = dev->srq->xrc_srq_num;

    /* Set up alternate port LID */
    if (Req.alt_port) {
        struct ibv_port_attr port_attr;
        int stat = ibv_query_port(dev->ib.context, Req.alt_port, &port_attr);

        if (stat != SUCCESS0)
            error(SYS, "query port failed");
        dev->lnode.alt_lid = port_attr.lid;
	if (port_attr.lmc > 0)
	    dev->lnode.alt_lid +=
                Req.src_path_bits & ((1 << port_attr.lmc) - 1);
    }
}


/*
 * Prepare the InfiniBand device for receiving and sending.  Final stage of
 * open.
 */
static void
ib_prep(DEVICE *dev)
{
    int flags;
    struct ibv_qp_attr rtr_attr ={
        .qp_state           = IBV_QPS_RTR,
        .path_mtu           = dev->ib.mtu,
        .dest_qp_num        = dev->rnode.qpn,
        .rq_psn             = dev->rnode.psn,
        .min_rnr_timer      = MIN_RNR_TIMER,
        .max_dest_rd_atomic = dev->lnode.rd_atomic,
        .ah_attr            = {
            .dlid           = dev->rnode.lid,
            .port_num       = dev->ib.port,
            .static_rate    = dev->ib.rate,
	    .src_path_bits  = Req.src_path_bits,
            .sl             = Req.sl
        }
    };
    struct ibv_qp_attr rts_attr ={
        .qp_state          = IBV_QPS_RTS,
        .timeout           = LOCAL_ACK_TIMEOUT,
        .retry_cnt         = RETRY_CNT,
        .rnr_retry         = RNR_RETRY_CNT,
        .sq_psn            = dev->lnode.psn,
        .max_rd_atomic     = dev->rnode.rd_atomic,
        .path_mig_state    = IBV_MIG_REARM,
        .alt_port_num      = Req.alt_port,
        .alt_ah_attr       = {
            .dlid          = dev->rnode.alt_lid,
            .port_num      = Req.alt_port,
            .static_rate   = dev->ib.rate,
	    .src_path_bits = Req.src_path_bits,
            .sl            = Req.sl
        }
    };
    struct ibv_ah_attr ah_attr ={
        .dlid          = dev->rnode.lid,
        .port_num      = dev->ib.port,
        .static_rate   = dev->ib.rate,
	.src_path_bits = Req.src_path_bits,
        .sl            = Req.sl
    };

    if (dev->trans == IBV_QPT_UD) {
        /* Modify queue pair to RTR */
        flags = IBV_QP_STATE;
        if (ibv_modify_qp(dev->qp, &rtr_attr, flags) != 0)
            error(SYS, "failed to modify QP to RTR");

        /* Modify queue pair to RTS */
        flags = IBV_QP_STATE | IBV_QP_SQ_PSN;
        if (ibv_modify_qp(dev->qp, &rts_attr, flags) != 0)
            error(SYS, "failed to modify QP to RTS");

        /* Create address handle */
        dev->ah = ibv_create_ah(dev->pd, &ah_attr);
        if (!dev->ah)
            error(SYS, "failed to create address handle");
    } else if (dev->trans == IBV_QPT_RC || dev->trans == IBV_QPT_XRC) {
        /* Modify queue pair to RTR */
        flags = IBV_QP_STATE              |
                IBV_QP_AV                 |
                IBV_QP_PATH_MTU           |
                IBV_QP_DEST_QPN           |
                IBV_QP_RQ_PSN             |
                IBV_QP_MAX_DEST_RD_ATOMIC |
                IBV_QP_MIN_RNR_TIMER;
        if (ibv_modify_qp(dev->qp, &rtr_attr, flags) != 0)
            error(SYS, "failed to modify QP to RTR");

        /* Modify queue pair to RTS */
        flags = IBV_QP_STATE     |
                IBV_QP_TIMEOUT   |
                IBV_QP_RETRY_CNT |
                IBV_QP_RNR_RETRY |
                IBV_QP_SQ_PSN    |
                IBV_QP_MAX_QP_RD_ATOMIC;
        if (dev->trans == IBV_QPT_RC && dev->rnode.alt_lid)
            flags |= IBV_QP_ALT_PATH | IBV_QP_PATH_MIG_STATE;
        if (ibv_modify_qp(dev->qp, &rts_attr, flags) != 0)
            error(SYS, "failed to modify QP to RTS");
    } else if (dev->trans == IBV_QPT_UC) {
        /* Modify queue pair to RTR */
        flags = IBV_QP_STATE    |
                IBV_QP_AV       |
                IBV_QP_PATH_MTU |
                IBV_QP_DEST_QPN |
                IBV_QP_RQ_PSN;
        if (ibv_modify_qp(dev->qp, &rtr_attr, flags) != 0)
            error(SYS, "failed to modify QP to RTR");

        /* Modify queue pair to RTS */
        flags = IBV_QP_STATE |
                IBV_QP_SQ_PSN;
        if (dev->rnode.alt_lid)
            flags |= IBV_QP_ALT_PATH | IBV_QP_PATH_MIG_STATE;
        if (ibv_modify_qp(dev->qp, &rts_attr, flags) != 0)
            error(SYS, "failed to modify QP to RTS");
    }
}


/*
 * Close an InfiniBand device, part 1.
 */
static void
ib_close1(DEVICE *dev)
{
    if (dev->qp)
        ibv_destroy_qp(dev->qp);
    if (dev->srq)
        ibv_destroy_srq(dev->srq);
    if (dev->xrc)
        ibv_close_xrc_domain(dev->xrc);
}


/*
 * Close an InfiniBand device, part 2.
 */
static void
ib_close2(DEVICE *dev)
{
    if (dev->ib.context)
        ibv_close_device(dev->ib.context);
    if (dev->ib.devlist)
        free(dev->ib.devlist);
}


/*
 * Cause a path migration to happen.
 */
static void
ib_migrate(DEVICE *dev)
{
    if (!Req.alt_port)
        return;
    /* Only migrate once. */
    Req.alt_port = 0;
    if (dev->trans != IBV_QPT_RC && dev->trans != IBV_QPT_UC)
        return;

    {
        struct ibv_qp_attr attr ={
            .path_mig_state  = IBV_MIG_MIGRATED,
        };

        if (ibv_modify_qp(dev->qp, &attr, IBV_QP_PATH_MIG_STATE) != SUCCESS0)
            error(SYS, "failed to modify QP to Migrated state");
    }
}


/*
 * Post an atomic.
 */
static void
ib_post_atomic(DEVICE *dev, ATOMIC atomic, int wrid,
                            int offset, uint64_t compare_add, uint64_t swap)
{
    struct ibv_sge sge ={
        .addr   = (uintptr_t)dev->buffer + offset,
        .length = sizeof(uint64_t),
        .lkey   = dev->mr->lkey
    };
    struct ibv_send_wr wr ={
        .wr_id      = wrid,
        .sg_list    = &sge,
        .num_sge    = 1,
        .send_flags = IBV_SEND_SIGNALED,
        .wr = {
            .atomic = {
                .remote_addr = dev->rnode.vaddr,
                .rkey        = dev->rnode.rkey,
            }
        }
    };
    struct ibv_send_wr *badwr;

    if (atomic == COMPARE_SWAP) {
        wr.opcode = IBV_WR_ATOMIC_CMP_AND_SWP;
        wr.wr.atomic.compare_add = compare_add;
        wr.wr.atomic.swap = swap;
    } else if (atomic == FETCH_ADD) {
        wr.opcode = IBV_WR_ATOMIC_FETCH_AND_ADD;
        wr.wr.atomic.compare_add = compare_add;
    }

    errno = 0;
    if (ibv_post_send(dev->qp, &wr, &badwr) != SUCCESS0) {
        if (Finished && errno == EINTR)
            return;
        if (atomic == COMPARE_SWAP)
            error(SYS, "failed to post compare and swap");
        else if (atomic == FETCH_ADD)
            error(SYS, "failed to post fetch and add");
        else
            error(BUG, "bad atomic: %d", atomic);
    }

    LStat.s.no_bytes += sizeof(uint64_t);
    LStat.s.no_msgs++;
}


/*
 * The standard version to post sends that most of the test routines call.
 * Post n sends.
 */
static void
rd_post_send_std(DEVICE *dev, int n)
{
    rd_post_send(dev, 0, dev->msg_size, 0, n, 1);
}


/*
 * Post one or more sends.
 */
static void
rd_post_send(DEVICE *dev, int off, int len, int inc, int rep, int stat)
{
    struct ibv_sge sge ={
        .addr   = (uintptr_t) &dev->buffer[off],
        .length = len,
        .lkey   = dev->mr->lkey
    };
    struct ibv_send_wr wr ={
        .wr_id      = WRID_SEND,
        .sg_list    = &sge,
        .num_sge    = 1,
        .opcode     = IBV_WR_SEND,
        .send_flags = IBV_SEND_SIGNALED,
    };
    struct ibv_send_wr *badwr;

    if (dev->trans == IBV_QPT_UD) {
        wr.wr.ud.ah          = dev->ah;
        wr.wr.ud.remote_qpn  = dev->rnode.qpn;
        wr.wr.ud.remote_qkey = dev->qkey;
    } else if (dev->trans == IBV_QPT_XRC)
        wr.xrc_remote_srq_num = dev->rnode.srqn;

    if (dev->msg_size <= dev->max_inline)
        wr.send_flags |= IBV_SEND_INLINE;

    errno = 0;
    while (!Finished && rep-- > 0) {
        if (ibv_post_send(dev->qp, &wr, &badwr) != SUCCESS0) {
            if (Finished && errno == EINTR)
                return;
            error(SYS, "failed to post send");
        }
        sge.addr += inc;
        sge.length += inc;
        if (stat) {
            LStat.s.no_bytes += dev->msg_size;
            LStat.s.no_msgs++;
        }
    }
}


/*
 * Post n receives.
 */
static void
rd_post_recv_std(DEVICE *dev, int n)
{
    struct ibv_sge sge ={
        .addr   = (uintptr_t) dev->buffer,
        .length = dev->buf_size,
        .lkey   = dev->mr->lkey
    };
    struct ibv_recv_wr wr ={
        .wr_id      = WRID_RECV,
        .sg_list    = &sge,
        .num_sge    = 1,
    };
    struct ibv_recv_wr *badwr;

    errno = 0;
    while (!Finished && n-- > 0) {
        int stat;

        if (dev->srq)
            stat = ibv_post_srq_recv(dev->srq, &wr, &badwr);
        else
            stat = ibv_post_recv(dev->qp, &wr, &badwr);

        if (stat != SUCCESS0) {
            if (Finished && errno == EINTR)
                return;
            error(SYS, "failed to post receive");
        }
    }
}


/*
 * Post n RDMA requests.
 */
static void
rd_post_rdma_std(DEVICE *dev, ibv_op opcode, int n)
{
    struct ibv_sge sge ={
        .addr   = (uintptr_t) dev->buffer,
        .length = dev->msg_size,
        .lkey   = dev->mr->lkey
    };
    struct ibv_send_wr wr ={
        .wr_id      = WRID_RDMA,
        .sg_list    = &sge,
        .num_sge    = 1,
        .opcode     = opcode,
        .send_flags = IBV_SEND_SIGNALED,
        .wr = {
            .rdma = {
                .remote_addr = dev->rnode.vaddr,
                .rkey        = dev->rnode.rkey
            }
        }
    };
    struct ibv_send_wr *badwr;

    if (opcode != IBV_WR_RDMA_READ && dev->msg_size <= dev->max_inline)
        wr.send_flags |= IBV_SEND_INLINE;
    errno = 0;
    while (!Finished && n--) {
        if (ibv_post_send(dev->qp, &wr, &badwr) != SUCCESS0) {
            if (Finished && errno == EINTR)
                return;
            error(SYS, "failed to post %s", opcode_name(wr.opcode));
        }
        if (opcode != IBV_WR_RDMA_READ) {
            LStat.s.no_bytes += dev->msg_size;
            LStat.s.no_msgs++;
        }
    }
}


/*
 * Poll the completion queue.
 */
static int
rd_poll(DEVICE *dev, struct ibv_wc *wc, int nwc)
{
    int n;

    if (!Req.poll_mode && !Finished) {
        void *ectx;
        struct ibv_cq *ecq;

        if (ibv_get_cq_event(dev->channel, &ecq, &ectx) != SUCCESS0)
            return maybe(0, "failed to get CQ event");
        if (ecq != dev->cq)
            error(0, "CQ event for unknown CQ");
        if (ibv_req_notify_cq(dev->cq, 0) != SUCCESS0)
            return maybe(0, "failed to request CQ notification");
	ibv_ack_cq_events(dev->cq, 1);
    }
    n = ibv_poll_cq(dev->cq, nwc, wc);
    if (n < 0)
        return maybe(0, "CQ poll failed");
    return n;
}


/*
 * We encountered an error in a system call which might simply have been
 * interrupted by the alarm that signaled completion of the test.  Generate the
 * error if appropriate or return the requested value.  Final return is just to
 * silence the compiler.
 */
static int
maybe(int val, char *msg)
{
    if (Finished && errno == EINTR)
        return val;
    error(SYS, msg);
    return 0;
}


/*
 * Encode a NODE structure into a data stream.
 */
static void
enc_node(NODE *host)
{
    enc_int(host->vaddr,     sizeof(host->vaddr));
    enc_int(host->lid,       sizeof(host->lid));
    enc_int(host->qpn,       sizeof(host->qpn));
    enc_int(host->psn,       sizeof(host->psn));
    enc_int(host->srqn,      sizeof(host->srqn));
    enc_int(host->rkey,      sizeof(host->rkey));
    enc_int(host->alt_lid,   sizeof(host->alt_lid));
    enc_int(host->rd_atomic, sizeof(host->rd_atomic));
}


/*
 * Decode a NODE structure from a data stream.
 */
static void
dec_node(NODE *host)
{
    host->vaddr     = dec_int(sizeof(host->vaddr));
    host->lid       = dec_int(sizeof(host->lid));
    host->qpn       = dec_int(sizeof(host->qpn));
    host->psn       = dec_int(sizeof(host->psn));
    host->srqn      = dec_int(sizeof(host->srqn));
    host->rkey      = dec_int(sizeof(host->rkey));
    host->alt_lid   = dec_int(sizeof(host->alt_lid));
    host->rd_atomic = dec_int(sizeof(host->rd_atomic));
}


/*
 * Handle a CQ error and return true if it is recoverable.
 */
static void
do_error(int status, uint64_t *errors)
{
    ++*errors;
    cq_error(status);
}


/*
 * Print out a CQ error given a status.
 */
static void
cq_error(int status)
{
    int i;

    for (i = 0; i < cardof(CQErrors); ++i)
        if (CQErrors[i].value == status)
            error(0, "%s failed: %s", TestName, CQErrors[i].name);
    error(0, "%s failed: CQ error %d", TestName, status);
}


/*
 * Return the name of an opcode.
 */
static char *
opcode_name(int opcode)
{
    int i;

    for (i = 0; i < cardof(Opcodes); ++i)
        if (Opcodes[i].value == opcode)
            return Opcodes[i].name;
    return "unknown operation";
}
