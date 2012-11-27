/*
 * $FreeBSD$
 */
#include <rdma/ib_verbs.h>
#include <netinet/in.h>

/*
 * Krping header stuffs...
 */

struct krping_stats {
	unsigned send_bytes;
	unsigned send_msgs;
	unsigned recv_bytes;
	unsigned recv_msgs;
	unsigned write_bytes;
	unsigned write_msgs;
	unsigned read_bytes;
	unsigned read_msgs;
};


/*
 * These states are used to signal events between the completion handler
 * and the main client or server thread.
 *
 * Once CONNECTED, they cycle through RDMA_READ_ADV, RDMA_WRITE_ADV, 
 * and RDMA_WRITE_COMPLETE for each ping.
 */
enum test_state {
	IDLE = 1,
	CONNECT_REQUEST,
	ADDR_RESOLVED,
	ROUTE_RESOLVED,
	CONNECTED,
	RDMA_READ_ADV,
	RDMA_READ_COMPLETE,
	RDMA_WRITE_ADV,
	RDMA_WRITE_COMPLETE,
	ERROR
};

struct krping_rdma_info {
	uint64_t buf;
	uint32_t rkey;
	uint32_t size;
};

/*
 * Control block struct.
 */
struct krping_cb {
	int server;			/* 0 iff client */
	struct ib_cq *cq;
	struct ib_pd *pd;
	struct ib_qp *qp;
	struct ib_mr *dma_mr;
	int use_dmamr;

	struct ib_recv_wr rq_wr;	/* recv work request record */
	struct ib_sge recv_sgl;		/* recv single SGE */
	struct krping_rdma_info recv_buf;/* malloc'd buffer */
	struct ib_mr *recv_mr;

	struct ib_send_wr sq_wr;	/* send work requrest record */
	struct ib_sge send_sgl;
	struct krping_rdma_info send_buf;/* single send buf */
	struct ib_mr *send_mr;

	struct ib_send_wr rdma_sq_wr;	/* rdma work request record */
	struct ib_sge rdma_sgl;		/* rdma single SGE */
	char *rdma_buf;			/* used as rdma sink */
	u64  rdma_addr;
	struct ib_mr *rdma_mr;

	uint32_t remote_rkey;		/* remote guys RKEY */
	uint64_t remote_addr;		/* remote guys TO */
	uint32_t remote_len;		/* remote guys LEN */

	char *start_buf;		/* rdma read src */
	u64  start_addr;
	struct ib_mr *start_mr;

	enum test_state state;		/* used for cond/signalling */
	struct mtx lock;
	struct krping_stats stats;

	uint16_t port;			/* dst port in NBO */
	struct in_addr addr;		/* dst addr in NBO */
	char *addr_str;			/* dst addr string */
	int verbose;			/* verbose logging */
	int count;			/* ping count */
	int size;			/* ping data size */
	int validate;			/* validate ping data */
	uint64_t memlimit;		/* limit of the physical memory that
					   can be registered with dma_mr mode */

	/* CM stuff */
	struct rdma_cm_id *cm_id;	/* connection on client side,*/
					/* listener on service side. */
	struct rdma_cm_id *child_cm_id;	/* connection on server side */
	TAILQ_ENTRY(krping_cb) list;	
	
	int rlat;			/* run read latency test */
	int wlat;			/* run write latency test */
	int bw;				/* run write bw test */
	int duplex;			/* run write bw full duplex test */
	int poll;			/* poll vs block in rlat */
	int txdepth;
};

static __inline uint64_t
get_cycles(void)
{
	u_int32_t low, high;
	__asm __volatile("rdtsc" : "=a" (low), "=d" (high));
	return (low | ((u_int64_t)high << 32)); 
}

#define htonll(x) htobe64((x))
#define ntohll(x) be64toh((x))

typedef uint64_t cycles_t;

extern struct mtx krping_mutex;
TAILQ_HEAD(krping_cb_list, krping_cb);
extern struct krping_cb_list krping_cbs;

int krping_doit(char *cmd);
void krping_init(void);
