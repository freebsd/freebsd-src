/*
 * linux/include/asm/qdio.h
 *
 * Linux for S/390 QDIO base support, Hipersocket base support
 * version 2
 *
 * Copyright 2000,2002 IBM Corporation
 * Author(s): Utz Bacher <utz.bacher@de.ibm.com>
 *
 */
#ifndef __QDIO_H__
#define __QDIO_H__

#define VERSION_QDIO_H "$Revision: 1.66 $"

/* note, that most of the typedef's are from ingo. */

#include <linux/interrupt.h>
#include <asm/irq.h>

//#define QDIO_DBF_LIKE_HELL

#define QDIO_NAME "qdio "

#define QDIO_VERBOSE_LEVEL 9

#ifndef CONFIG_ARCH_S390X
#define QDIO_32_BIT
#endif /* CONFIG_ARCH_S390X */

#define QDIO_USE_PROCESSING_STATE

#ifdef CONFIG_QDIO_PERF_STATS
#define QDIO_PERFORMANCE_STATS
#endif /* CONFIG_QDIO_PERF_STATS */

/**** CONSTANTS, that are relied on without using these symbols *****/
#define QDIO_MAX_QUEUES_PER_IRQ 32 /* used in width of unsigned int */
/************************ END of CONSTANTS **************************/
#define QDIO_MAX_BUFFERS_PER_Q 128 /* must be a power of 2 (%x=&(x-1)*/
#define QDIO_BUF_ORDER 7 /* 2**this == number of pages used for sbals in 1 q */
#define QDIO_MAX_ELEMENTS_PER_BUFFER 16
#define SBAL_SIZE 256

#define IQDIO_FILL_LEVEL_TO_POLL (QDIO_MAX_BUFFERS_PER_Q*4/3)

#define TIQDIO_THININT_ISC 3
#define TIQDIO_DELAY_TARGET 0
#define QDIO_BUSY_BIT_PATIENCE 2000 /* in microsecs */
#define IQDIO_GLOBAL_LAPS 2 /* GLOBAL_LAPS are not used as we */
#define IQDIO_GLOBAL_LAPS_INT 1 /* dont global summary */
#define IQDIO_LOCAL_LAPS 4
#define IQDIO_LOCAL_LAPS_INT 1
#define IQDIO_GLOBAL_SUMMARY_CC_MASK 2
/*#define IQDIO_IQDC_INT_PARM 0x1234*/

#define QDIO_Q_LAPS 5

#define QDIO_STORAGE_ACC_KEY get_storage_key()

#define L2_CACHELINE_SIZE 256
#define INDICATORS_PER_CACHELINE (L2_CACHELINE_SIZE/sizeof(__u32))

#define QDIO_PERF "qdio_perf"

/* must be a power of 2 */
/*#define QDIO_STATS_NUMBER 4

#define QDIO_STATS_CLASSES 2
#define QDIO_STATS_COUNT_NEEDED 2*/

#define QDIO_ACTIVATE_DELAY 5 /* according to brenton belmar and paul
				 gioquindo it can take up to 5ms before
				 queues are really active */

#define QDIO_NO_USE_COUNT_TIME 10
#define QDIO_NO_USE_COUNT_TIMEOUT 1000 /* wait for 1 sec on each q before
					  exiting without having use_count
					  of the queue to 0 */

#define QDIO_ESTABLISH_TIMEOUT 1000
#define QDIO_ACTIVATE_TIMEOUT 100
#define QDIO_CLEANUP_CLEAR_TIMEOUT 20000
#define QDIO_CLEANUP_HALT_TIMEOUT 10000

#define QDIO_BH AURORA_BH

#define QDIO_IRQ_BUCKETS 256 /* heavy..., but does only use a few bytes, but
			      be rather faster in cases of collisions
			      (if there really is a collision, it is
			      on every (traditional) interrupt and every
			      do_QDIO, so we rather are generous */
#define QDIO_QETH_QFMT 0
#define QDIO_ZFCP_QFMT 1
#define QDIO_IQDIO_QFMT 2

#define QDIO_IRQ_STATE_FRESH 0 /* must be 0 -> memset has set it to 0 */
#define QDIO_IRQ_STATE_INACTIVE 1
#define QDIO_IRQ_STATE_ESTABLISHED 2
#define QDIO_IRQ_STATE_ACTIVE 3
#define QDIO_IRQ_STATE_STOPPED 4

/* used as intparm in do_IO: */
#define QDIO_DOING_SENSEID 0
#define QDIO_DOING_ESTABLISH 1
#define QDIO_DOING_ACTIVATE 2
#define QDIO_DOING_CLEANUP 3

/************************* DEBUG FACILITY STUFF *********************/
/* #define QDIO_DBF_LIKE_HELL */

#define QDIO_DBF_HEX(ex,name,level,addr,len) \
	do { \
	if (ex) \
		debug_exception(qdio_dbf_##name,level,(void*)(addr),len); \
	else \
		debug_event(qdio_dbf_##name,level,(void*)(addr),len); \
	} while (0)
#define QDIO_DBF_TEXT(ex,name,level,text) \
	do { \
	if (ex) \
		debug_text_exception(qdio_dbf_##name,level,text); \
	else \
		debug_text_event(qdio_dbf_##name,level,text); \
	} while (0)

#define QDIO_DBF_HEX0(ex,name,addr,len) QDIO_DBF_HEX(ex,name,0,addr,len)
#define QDIO_DBF_HEX1(ex,name,addr,len) QDIO_DBF_HEX(ex,name,1,addr,len)
#define QDIO_DBF_HEX2(ex,name,addr,len) QDIO_DBF_HEX(ex,name,2,addr,len)
#define QDIO_DBF_HEX3(ex,name,addr,len) QDIO_DBF_HEX(ex,name,3,addr,len)
#define QDIO_DBF_HEX4(ex,name,addr,len) QDIO_DBF_HEX(ex,name,4,addr,len)
#define QDIO_DBF_HEX5(ex,name,addr,len) QDIO_DBF_HEX(ex,name,5,addr,len)
#define QDIO_DBF_HEX6(ex,name,addr,len) QDIO_DBF_HEX(ex,name,6,addr,len)
#ifdef QDIO_DBF_LIKE_HELL
#endif /* QDIO_DBF_LIKE_HELL */
#if 0
#define QDIO_DBF_HEX0(ex,name,addr,len) do {} while (0)
#define QDIO_DBF_HEX1(ex,name,addr,len) do {} while (0)
#define QDIO_DBF_HEX2(ex,name,addr,len) do {} while (0)
#ifndef QDIO_DBF_LIKE_HELL
#define QDIO_DBF_HEX3(ex,name,addr,len) do {} while (0)
#define QDIO_DBF_HEX4(ex,name,addr,len) do {} while (0)
#define QDIO_DBF_HEX5(ex,name,addr,len) do {} while (0)
#define QDIO_DBF_HEX6(ex,name,addr,len) do {} while (0)
#endif /* QDIO_DBF_LIKE_HELL */
#endif /* 0 */

#define QDIO_DBF_TEXT0(ex,name,text) QDIO_DBF_TEXT(ex,name,0,text)
#define QDIO_DBF_TEXT1(ex,name,text) QDIO_DBF_TEXT(ex,name,1,text)
#define QDIO_DBF_TEXT2(ex,name,text) QDIO_DBF_TEXT(ex,name,2,text)
#define QDIO_DBF_TEXT3(ex,name,text) QDIO_DBF_TEXT(ex,name,3,text)
#define QDIO_DBF_TEXT4(ex,name,text) QDIO_DBF_TEXT(ex,name,4,text)
#define QDIO_DBF_TEXT5(ex,name,text) QDIO_DBF_TEXT(ex,name,5,text)
#define QDIO_DBF_TEXT6(ex,name,text) QDIO_DBF_TEXT(ex,name,6,text)
#ifdef QDIO_DBF_LIKE_HELL
#endif /* QDIO_DBF_LIKE_HELL */
#if 0
#define QDIO_DBF_TEXT0(ex,name,text) do {} while (0)
#define QDIO_DBF_TEXT1(ex,name,text) do {} while (0)
#define QDIO_DBF_TEXT2(ex,name,text) do {} while (0)
#ifndef QDIO_DBF_LIKE_HELL
#define QDIO_DBF_TEXT3(ex,name,text) do {} while (0)
#define QDIO_DBF_TEXT4(ex,name,text) do {} while (0)
#define QDIO_DBF_TEXT5(ex,name,text) do {} while (0)
#define QDIO_DBF_TEXT6(ex,name,text) do {} while (0)
#endif /* QDIO_DBF_LIKE_HELL */
#endif /* 0 */

#define QDIO_DBF_SETUP_NAME "qdio_setup"
#define QDIO_DBF_SETUP_LEN 8
#define QDIO_DBF_SETUP_INDEX 2
#define QDIO_DBF_SETUP_NR_AREAS 1
#ifdef QDIO_DBF_LIKE_HELL
#define QDIO_DBF_SETUP_LEVEL 6
#else /* QDIO_DBF_LIKE_HELL */
#define QDIO_DBF_SETUP_LEVEL 2
#endif /* QDIO_DBF_LIKE_HELL */

#define QDIO_DBF_SBAL_NAME "qdio_labs" /* sbal */
#define QDIO_DBF_SBAL_LEN 256
#define QDIO_DBF_SBAL_INDEX 2
#define QDIO_DBF_SBAL_NR_AREAS 2
#ifdef QDIO_DBF_LIKE_HELL
#define QDIO_DBF_SBAL_LEVEL 6
#else /* QDIO_DBF_LIKE_HELL */
#define QDIO_DBF_SBAL_LEVEL 2
#endif /* QDIO_DBF_LIKE_HELL */

#define QDIO_DBF_TRACE_NAME "qdio_trace"
#define QDIO_DBF_TRACE_LEN 8
#define QDIO_DBF_TRACE_NR_AREAS 2
#ifdef QDIO_DBF_LIKE_HELL
#define QDIO_DBF_TRACE_INDEX 4
#define QDIO_DBF_TRACE_LEVEL 4 /* -------- could be even more verbose here */
#else /* QDIO_DBF_LIKE_HELL */
#define QDIO_DBF_TRACE_INDEX 2
#define QDIO_DBF_TRACE_LEVEL 2
#endif /* QDIO_DBF_LIKE_HELL */

#define QDIO_DBF_SENSE_NAME "qdio_sense"
#define QDIO_DBF_SENSE_LEN 64
#define QDIO_DBF_SENSE_INDEX 1
#define QDIO_DBF_SENSE_NR_AREAS 1
#ifdef QDIO_DBF_LIKE_HELL
#define QDIO_DBF_SENSE_LEVEL 6
#else /* QDIO_DBF_LIKE_HELL */
#define QDIO_DBF_SENSE_LEVEL 2
#endif /* QDIO_DBF_LIKE_HELL */

#ifdef QDIO_DBF_LIKE_HELL
#define QDIO_TRACE_QTYPE QDIO_ZFCP_QFMT

#define QDIO_DBF_SLSB_OUT_NAME "qdio_slsb_out"
#define QDIO_DBF_SLSB_OUT_LEN QDIO_MAX_BUFFERS_PER_Q
#define QDIO_DBF_SLSB_OUT_INDEX 8
#define QDIO_DBF_SLSB_OUT_NR_AREAS 1
#define QDIO_DBF_SLSB_OUT_LEVEL 6

#define QDIO_DBF_SLSB_IN_NAME "qdio_slsb_in"
#define QDIO_DBF_SLSB_IN_LEN QDIO_MAX_BUFFERS_PER_Q
#define QDIO_DBF_SLSB_IN_INDEX 8
#define QDIO_DBF_SLSB_IN_NR_AREAS 1
#define QDIO_DBF_SLSB_IN_LEVEL 6
#endif /* QDIO_DBF_LIKE_HELL */

/****************** END OF DEBUG FACILITY STUFF *********************/

typedef struct qdio_buffer_element_t {
	unsigned int flags;
	unsigned int length;
#ifdef QDIO_32_BIT
	void *reserved;
#endif /* QDIO_32_BIT */
	void *addr;
} __attribute__ ((packed,aligned(16))) qdio_buffer_element_t;

typedef struct qdio_buffer_t {
	volatile qdio_buffer_element_t element[16];
} __attribute__ ((packed,aligned(256))) qdio_buffer_t;


/* params are: irq, status, qdio_error, siga_error,
   queue_number, first element processed, number of elements processed,
   int_parm */
typedef void qdio_handler_t(int,unsigned int,unsigned int,unsigned int,
			    unsigned int,int,int,unsigned long);


#define QDIO_STATUS_INBOUND_INT 0x01
#define QDIO_STATUS_OUTBOUND_INT 0x02
#define QDIO_STATUS_LOOK_FOR_ERROR 0x04
#define QDIO_STATUS_MORE_THAN_ONE_QDIO_ERROR 0x08
#define QDIO_STATUS_MORE_THAN_ONE_SIGA_ERROR 0x10
#define QDIO_STATUS_ACTIVATE_CHECK_CONDITION 0x20

#define QDIO_SIGA_ERROR_ACCESS_EXCEPTION 0x10
#define QDIO_SIGA_ERROR_B_BIT_SET 0x20

/* for qdio_initialize */
#define QDIO_INBOUND_0COPY_SBALS 0x01
#define QDIO_OUTBOUND_0COPY_SBALS 0x02
#define QDIO_USE_OUTBOUND_PCIS 0x04
#define QDIO_PFIX 0x08

/* for qdio_cleanup */
#define QDIO_FLAG_CLEANUP_USING_CLEAR 0x01
#define QDIO_FLAG_CLEANUP_USING_HALT 0x02

typedef struct qdio_initialize_t {
	int irq;
	unsigned char q_format;
	unsigned char adapter_name[8];
       	unsigned int qib_param_field_format; /*adapter dependent*/
	/* pointer to 128 bytes or NULL, if no param field */
	unsigned char *qib_param_field; /* adapter dependent */
	/* pointer to no_queues*128 words of data or NULL */
	unsigned long *input_slib_elements;
	unsigned long *output_slib_elements;
	unsigned int min_input_threshold;
	unsigned int max_input_threshold;
	unsigned int min_output_threshold;
	unsigned int max_output_threshold;
	unsigned int no_input_qs;
	unsigned int no_output_qs;
	qdio_handler_t *input_handler;
	qdio_handler_t *output_handler;
	unsigned long int_parm;
	unsigned long flags;
	void **input_sbal_addr_array; /* addr of n*128 void ptrs */
	void **output_sbal_addr_array; /* addr of n*128 void ptrs */
} qdio_initialize_t;
extern int qdio_initialize(qdio_initialize_t *init_data);

extern int qdio_activate(int irq,int flags);

#define QDIO_STATE_MUST_USE_OUTB_PCI	0x00000001
#define QDIO_STATE_INACTIVE 		0x00000002 /* after qdio_cleanup */
#define QDIO_STATE_ESTABLISHED 		0x00000004 /* after qdio_initialize */
#define QDIO_STATE_ACTIVE 		0x00000008 /* after qdio_activate */
#define QDIO_STATE_STOPPED 		0x00000010 /* after queues went down */
extern unsigned long qdio_get_status(int irq);


#define QDIO_FLAG_SYNC_INPUT     0x01
#define QDIO_FLAG_SYNC_OUTPUT    0x02
#define QDIO_FLAG_UNDER_INTERRUPT 0x04
#define QDIO_FLAG_NO_INPUT_INTERRUPT_CONTEXT 0x08 /* no effect on
						     adapter interrupts */
#define QDIO_FLAG_DONT_SIGA 0x10

extern int do_QDIO(int irq,unsigned int flags, unsigned int queue_number,
		   unsigned int qidx,unsigned int count,
		   qdio_buffer_t *buffers);

extern int qdio_synchronize(int irq,unsigned int flags,
			    unsigned int queue_number);

extern int qdio_cleanup(int irq,int how);

extern unsigned char qdio_get_slsb_state(int irq,unsigned int flag,
				  unsigned int queue_number,
				  unsigned int qidx);

/*
 * QDIO device commands returned by extended Sense-ID
 */
#define DEFAULT_ESTABLISH_QS_CMD 0x1b
#define DEFAULT_ESTABLISH_QS_COUNT 0x1000
#define DEFAULT_ACTIVATE_QS_CMD 0x1f
#define DEFAULT_ACTIVATE_QS_COUNT 0
typedef struct _qdio_cmds {
	unsigned char rcd;            /* read configuration data */
	unsigned short count_rcd;
	unsigned char sii;            /* set interface identifier */
	unsigned short count_sii;
	unsigned char rni;            /* read node identifier */
	unsigned short count_rni;
	unsigned char eq;             /* establish QDIO queues */
	unsigned short count_eq;
	unsigned char aq;             /* activate QDIO queues */
	unsigned short count_aq;
} qdio_cmds_t;

/*
 * additional CIWs returned by extended Sense-ID
 */
#define CIW_TYPE_EQUEUE 0x3       /* establish QDIO queues */
#define CIW_TYPE_AQUEUE 0x4       /* activate QDIO queues */

typedef struct _qdesfmt0 {
#ifdef QDIO_32_BIT
	unsigned long res1;             /* reserved */
#endif /* QDIO_32_BIT */
	unsigned long sliba;            /* storage-list-information-block
					   address */
#ifdef QDIO_32_BIT
	unsigned long res2;             /* reserved */
#endif /* QDIO_32_BIT */
	unsigned long sla;              /* storage-list address */
#ifdef QDIO_32_BIT
	unsigned long res3;             /* reserved */
#endif /* QDIO_32_BIT */
	unsigned long slsba;            /* storage-list-state-block address */
	unsigned int  res4;		/* reserved */
	unsigned int  akey  :  4;       /* access key for DLIB */
	unsigned int  bkey  :  4;       /* access key for SL */
	unsigned int  ckey  :  4;       /* access key for SBALs */
	unsigned int  dkey  :  4;       /* access key for SLSB */
	unsigned int  res5  : 16;       /* reserved */
} __attribute__ ((packed)) qdesfmt0_t;

/*
 * Queue-Description record (QDR)
 */
typedef struct _qdr {
	unsigned int  qfmt    :  8;     /* queue format */
	unsigned int  pfmt    :  8;     /* impl. dep. parameter format */
	unsigned int  res1    :  8;     /* reserved */
	unsigned int  ac      :  8;     /* adapter characteristics */
	unsigned int  res2    :  8;     /* reserved */
	unsigned int  iqdcnt  :  8;     /* input-queue-descriptor count */
	unsigned int  res3    :  8;     /* reserved */
	unsigned int  oqdcnt  :  8;     /* output-queue-descriptor count */
	unsigned int  res4    :  8;     /* reserved */
	unsigned int  iqdsz   :  8;     /* input-queue-descriptor size */
	unsigned int  res5    :  8;     /* reserved */
	unsigned int  oqdsz   :  8;     /* output-queue-descriptor size */
	unsigned int  res6[9];          /* reserved */
#ifdef QDIO_32_BIT
	unsigned long res7;		/* reserved */
#endif /* QDIO_32_BIT */
	unsigned long qiba;             /* queue-information-block address */
	unsigned int  res8;             /* reserved */
	unsigned int  qkey    :  4;     /* queue-informatio-block key */
	unsigned int  res9    : 28;     /* reserved */
/*	union _qd {*/ /* why this? */
		qdesfmt0_t qdf0[126];
/*	} qd;*/
} __attribute__ ((packed,aligned(4096))) qdr_t;


/*
 * queue information block (QIB)
 */
#define QIB_AC_INBOUND_PCI_SUPPORTED 0x80
#define QIB_AC_OUTBOUND_PCI_SUPPORTED 0x40
typedef struct _qib {
	unsigned int  qfmt    :  8;     /* queue format */
	unsigned int  pfmt    :  8;     /* impl. dep. parameter format */
	unsigned int  res1    :  8;     /* reserved */
	unsigned int  ac      :  8;     /* adapter characteristics */
	unsigned int  res2;             /* reserved */
#ifdef QDIO_32_BIT
	unsigned long res3;             /* reserved */
#endif /* QDIO_32_BIT */
	unsigned long isliba;           /* absolute address of 1st
					   input SLIB */
#ifdef QDIO_32_BIT
	unsigned long res4;             /* reserved */
#endif /* QDIO_32_BIT */
	unsigned long osliba;           /* absolute address of 1st
					   output SLIB */
	unsigned int  res5;             /* reserved */
	unsigned int  res6;             /* reserved */
	unsigned char ebcnam[8];        /* adapter identifier in EBCDIC */
	unsigned char res7[88];         /* reserved */
	unsigned char parm[QDIO_MAX_BUFFERS_PER_Q];
					/* implementation dependent
					   parameters */
} __attribute__ ((packed,aligned(256))) qib_t;


/*
 * storage-list-information block element (SLIBE)
 */
typedef struct _slibe {
#ifdef QDIO_32_BIT
	unsigned long res;              /* reserved */
#endif /* QDIO_32_BIT */
	unsigned long parms;            /* implementation dependent
					   parameters */
} slibe_t;

/*
 * storage-list-information block (SLIB)
 */
typedef struct _slib {
#ifdef QDIO_32_BIT
	unsigned long res1;             /* reserved */
#endif /* QDIO_32_BIT */
        unsigned long nsliba;           /* next SLIB address (if any) */
#ifdef QDIO_32_BIT
	unsigned long res2;             /* reserved */
#endif /* QDIO_32_BIT */
	unsigned long sla;              /* SL address */
#ifdef QDIO_32_BIT
	unsigned long res3;             /* reserved */
#endif /* QDIO_32_BIT */
	unsigned long slsba;            /* SLSB address */
	unsigned char res4[1000];       /* reserved */
	slibe_t       slibe[QDIO_MAX_BUFFERS_PER_Q];    /* SLIB elements */
} __attribute__ ((packed,aligned(2048))) slib_t;

typedef struct _sbal_flags {
	unsigned char res1  : 1;   /* reserved */
	unsigned char last  : 1;   /* last entry */
	unsigned char cont  : 1;   /* contiguous storage */
	unsigned char res2  : 1;   /* reserved */
	unsigned char frag  : 2;   /* fragmentation (s.below) */
	unsigned char res3  : 2;   /* reserved */
} __attribute__ ((packed)) sbal_flags_t;

#define SBAL_FLAGS_FIRST_FRAG	     0x04000000UL
#define SBAL_FLAGS_MIDDLE_FRAG	     0x08000000UL
#define SBAL_FLAGS_LAST_FRAG	     0x0c000000UL
#define SBAL_FLAGS_LAST_ENTRY	     0x40000000UL
#define SBAL_FLAGS_CONTIGUOUS	     0x20000000UL

#define SBAL_FLAGS0_DATA_CONTINUATION 0x20UL

/* Awesome FCP extensions */
#define SBAL_FLAGS0_TYPE_STATUS       0x00UL
#define SBAL_FLAGS0_TYPE_WRITE        0x08UL
#define SBAL_FLAGS0_TYPE_READ         0x10UL
#define SBAL_FLAGS0_TYPE_WRITE_READ   0x18UL
#define SBAL_FLAGS0_MORE_SBALS	      0x04UL
#define SBAL_FLAGS0_COMMAND           0x02UL
#define SBAL_FLAGS0_LAST_SBAL         0x00UL
#define SBAL_FLAGS0_ONLY_SBAL         SBAL_FLAGS0_COMMAND
#define SBAL_FLAGS0_MIDDLE_SBAL       SBAL_FLAGS0_MORE_SBALS
#define SBAL_FLAGS0_FIRST_SBAL        SBAL_FLAGS0_MORE_SBALS | SBAL_FLAGS0_COMMAND
/* Naught of interest beyond this point */

#define SBAL_FLAGS0_PCI		0x40
typedef struct _sbal_sbalf_0 {
	unsigned char res1  : 1;   /* reserved */
	unsigned char pci   : 1;   /* PCI indicator */
	unsigned char cont  : 1;   /* data continuation */
	unsigned char sbtype: 2;   /* storage-block type (FCP) */
	unsigned char res2  : 3;   /* reserved */
} __attribute__ ((packed)) sbal_sbalf_0_t;

typedef struct _sbal_sbalf_1 {
	unsigned char res1  : 4;   /* reserved */
	unsigned char key   : 4;   /* storage key */
} __attribute__ ((packed)) sbal_sbalf_1_t;

typedef struct _sbal_sbalf_14 {
	unsigned char res1   : 4;  /* reserved */
	unsigned char erridx : 4;  /* error index */
} __attribute__ ((packed)) sbal_sbalf_14_t;

typedef struct _sbal_sbalf_15 {
	unsigned char reason;      /* reserved */
} __attribute__ ((packed)) sbal_sbalf_15_t;

typedef union _sbal_sbalf {
	sbal_sbalf_0_t  i0;
	sbal_sbalf_1_t  i1;
	sbal_sbalf_14_t i14;
	sbal_sbalf_15_t i15;
	unsigned char value;
} sbal_sbalf_t;

typedef struct _sbale {
	union {
		sbal_flags_t  bits;       /* flags */
		unsigned char value;
	} flags;
	unsigned int  res1  : 16;   /* reserved */
	sbal_sbalf_t  sbalf;       /* SBAL flags */
	unsigned int  res2  : 16;  /* reserved */
	unsigned int  count : 16;  /* data count */
#ifdef QDIO_32_BIT
	unsigned long res3;        /* reserved */
#endif /* QDIO_32_BIT */
	unsigned long addr;        /* absolute data address */
} __attribute__ ((packed,aligned(16))) sbal_element_t;

/*
 * strorage-block access-list (SBAL)
 */
typedef struct _sbal {
	sbal_element_t element[QDIO_MAX_ELEMENTS_PER_BUFFER];
} __attribute__ ((packed,aligned(256))) sbal_t;

/*
 * storage-list (SL)
 */
typedef struct _sl_element {
#ifdef QDIO_32_BIT
        unsigned long res;     /* reserved */
#endif /* QDIO_32_BIT */
        unsigned long sbal;    /* absolute SBAL address */
} __attribute__ ((packed)) sl_element_t;

typedef struct _sl {
	sl_element_t element[QDIO_MAX_BUFFERS_PER_Q];
} __attribute__ ((packed,aligned(1024))) sl_t;

/*
 * storage-list-state block (SLSB)
 */
/*typedef struct _slsb_val {*/
/*	unsigned char value;       */ /* SLSB entry as a single byte value */
/*} __attribute__ ((packed)) slsb_val_t;*/

typedef struct _slsb_flags {
	unsigned char owner  : 2;   /* SBAL owner */
	unsigned char type   : 1;   /* buffer type */
	unsigned char state  : 5;   /* processing state */
} __attribute__ ((packed)) slsb_flags_t;


typedef struct _slsb {
	union _acc {
		unsigned char val[QDIO_MAX_BUFFERS_PER_Q];
		slsb_flags_t flags[QDIO_MAX_BUFFERS_PER_Q];
	} acc;
} __attribute__ ((packed,aligned(256))) slsb_t;

/*
 * SLSB values
 */
#define SLSB_OWNER_PROG              1
#define SLSB_OWNER_CU                2

#define SLSB_TYPE_INPUT              0
#define SLSB_TYPE_OUTPUT             1

#define SLSB_STATE_NOT_INIT          0
#define SLSB_STATE_EMPTY             1
#define SLSB_STATE_PRIMED            2
#define SLSB_STATE_HALTED          0xe
#define SLSB_STATE_ERROR           0xf

#define SLSB_P_INPUT_NOT_INIT     0x80
#define SLSB_P_INPUT_PROCESSING	  0x81
#define SLSB_CU_INPUT_EMPTY       0x41
#define SLSB_P_INPUT_PRIMED       0x82
#define SLSB_P_INPUT_HALTED       0x8E
#define SLSB_P_INPUT_ERROR        0x8F

#define SLSB_P_OUTPUT_NOT_INIT    0xA0
#define SLSB_P_OUTPUT_EMPTY       0xA1
#define SLSB_CU_OUTPUT_PRIMED     0x62
#define SLSB_P_OUTPUT_HALTED      0xAE
#define SLSB_P_OUTPUT_ERROR       0xAF

#define SLSB_ERROR_DURING_LOOKUP  0xFF

typedef struct qdio_q_t {
	volatile slsb_t slsb;

	__u32 * volatile dev_st_chg_ind;

	int is_input_q;
	int is_0copy_sbals_q;

	unsigned int is_iqdio_q;
	unsigned int is_thinint_q;

	/* bit 0 means queue 0, bit 1 means queue 1, ... */
	unsigned int mask;
	unsigned int q_no;

	qdio_handler_t (*handler);

	/* points to the next buffer to be checked for having
	 * been processed by the card (outbound)
	 * or to the next buffer the program should check for (inbound) */
	volatile int first_to_check;
	/* and the last time it was: */
	volatile int last_move_ftc;

	atomic_t number_of_buffers_used;
	atomic_t polling;

	unsigned int siga_in;
	unsigned int siga_out;
	unsigned int siga_sync;
	unsigned int siga_sync_done_on_thinints;
	unsigned int siga_sync_done_on_outb_tis;
	unsigned int hydra_gives_outbound_pcis;

	/* used to save beginning position when calling dd_handlers */
	int first_element_to_kick;

	atomic_t use_count;
	atomic_t is_in_shutdown;

	int irq;
	void *irq_ptr;

#ifdef QDIO_USE_TIMERS_FOR_POLLING
	struct timer_list timer;
	atomic_t timer_already_set;
	spinlock_t timer_lock;
#else /* QDIO_USE_TIMERS_FOR_POLLING */
	struct tasklet_struct tasklet;
#endif /* QDIO_USE_TIMERS_FOR_POLLING */

	unsigned int state;

	/* used to store the error condition during a data transfer */
	unsigned int qdio_error;
	unsigned int siga_error;
	unsigned int error_status_flags;

	/* list of interesting queues */
	volatile struct qdio_q_t *list_next;
	volatile struct qdio_q_t *list_prev;

	slib_t *slib; /* a page is allocated under this pointer,
			 sl points into this page, offset PAGE_SIZE/2
			 (after slib) */
	sl_t *sl;
	volatile sbal_t *sbal[QDIO_MAX_BUFFERS_PER_Q];

	qdio_buffer_t *qdio_buffers[QDIO_MAX_BUFFERS_PER_Q];

	unsigned long int_parm;

	/*struct {
		int in_bh_check_limit;
		int threshold;
	} threshold_classes[QDIO_STATS_CLASSES];*/

	struct {
		/* inbound: the time to stop polling
		   outbound: the time to kick peer */
		int threshold; /* the real value */

		/* outbound: last time of do_QDIO
		   inbound: last time of noticing incoming data */
		/*__u64 last_transfer_times[QDIO_STATS_NUMBER];
		int last_transfer_index; */

		__u64 last_transfer_time;
	} timing;
        unsigned int queue_type;
} __attribute__ ((aligned(256))) qdio_q_t;

typedef struct qdio_irq_t {
	__u32 * volatile dev_st_chg_ind;

	unsigned long int_parm;
	int irq;

	unsigned int is_iqdio_irq;
	unsigned int is_thinint_irq;
	unsigned int hydra_gives_outbound_pcis;
	unsigned int sync_done_on_outb_pcis;

	unsigned int state;
	spinlock_t setting_up_lock;

	unsigned int no_input_qs;
	unsigned int no_output_qs;

	unsigned char qdioac;

	qdio_q_t *input_qs[QDIO_MAX_QUEUES_PER_IRQ];
	qdio_q_t *output_qs[QDIO_MAX_QUEUES_PER_IRQ];

	ccw1_t ccw;
	int io_result_cstat;
	int io_result_dstat;
	int io_result_flags;
	atomic_t interrupt_has_arrived;
	atomic_t interrupt_has_been_cleaned;
	wait_queue_head_t wait_q;

	qdr_t *qdr;

	qdio_cmds_t commands;

	qib_t qib;

	io_handler_func_t original_int_handler;

	unsigned long other_flags; /* e.g. QDIO_PFIX */

	struct qdio_irq_t *next;
} qdio_irq_t;

#define QDIO_CHSC_RESPONSE_CODE_OK 1
/* flags for st qdio sch data */
#define CHSC_FLAG_QDIO_CAPABILITY 0x80
#define CHSC_FLAG_VALIDITY 0x40

#define CHSC_FLAG_SIGA_INPUT_NECESSARY 0x40
#define CHSC_FLAG_SIGA_OUTPUT_NECESSARY 0x20
#define CHSC_FLAG_SIGA_SYNC_NECESSARY 0x10
#define CHSC_FLAG_SIGA_SYNC_DONE_ON_THININTS 0x08
#define CHSC_FLAG_SIGA_SYNC_DONE_ON_OUTB_PCIS 0x04

typedef struct qdio_chsc_area_t {
	struct {
		/* word 0 */
		__u16 command_code1;
		__u16 command_code2;
		/* word 1 */
		__u16 operation_code;
		__u16 first_sch;
		/* word 2 */
		__u8 reserved1;
		__u8 image_id;
		__u16 last_sch;
		/* word 3 */
		__u32 reserved2;

		/* word 4 */
		union {
			struct {
				/* word 4&5 */
				__u64 summary_indicator_addr;
				/* word 6&7 */
				__u64 subchannel_indicator_addr;
				/* word 8 */
				int ks:4;
				int kc:4;
				int reserved1:21;
				int isc:3;
				/* word 9&10 */
				__u32 reserved2[2];
				/* word 11 */
				__u32 subsystem_id;
				/* word 12-1015 */
				__u32 reserved3[1004];
			} __attribute__ ((packed,aligned(4))) set_chsc;
			struct {
				/* word 4&5 */
				__u32 reserved1[2];	
				/* word 6 */
				__u32 delay_target;
				/* word 7-1015 */
				__u32 reserved4[1009];
			} __attribute__ ((packed,aligned(4))) set_chsc_fast;
			struct {
				/* word 0 */
				__u16 length;
				__u16 response_code;
				/* word 1 */
				__u32 reserved1;
				/* words 2 to 9 for st sch qdio data */
				__u8 flags;
				__u8 reserved2;
				__u16 sch;
				__u8 qfmt;
				__u8 reserved3;
				__u8 qdioac;
				__u8 sch_class;
				__u8 reserved4;
				__u8 icnt;
				__u8 reserved5;
				__u8 ocnt;
				/* plus 5 words of reserved fields */
			} __attribute__ ((packed,aligned(8)))
			store_qdio_data_response;
		} operation_data_area;
	} __attribute__ ((packed,aligned(8))) request_block;
	struct {
		/* word 0 */
		__u16 length;
		__u16 response_code;
		/* word 1 */
		__u32 reserved1;
	} __attribute__ ((packed,aligned(8))) response_block;
} __attribute__ ((packed,aligned(PAGE_SIZE))) qdio_chsc_area_t;


#define QDIO_PRINTK_HEADER QDIO_NAME ": "

#if QDIO_VERBOSE_LEVEL>8
#define QDIO_PRINT_STUPID(x...) printk( KERN_DEBUG QDIO_PRINTK_HEADER x)
#else
#define QDIO_PRINT_STUPID(x...)
#endif

#if QDIO_VERBOSE_LEVEL>7
#define QDIO_PRINT_ALL(x...) printk( QDIO_PRINTK_HEADER x)
#else
#define QDIO_PRINT_ALL(x...)
#endif

#if QDIO_VERBOSE_LEVEL>6
#define QDIO_PRINT_INFO(x...) printk( QDIO_PRINTK_HEADER x)
#else
#define QDIO_PRINT_INFO(x...)
#endif

#if QDIO_VERBOSE_LEVEL>5
#define QDIO_PRINT_WARN(x...) printk( QDIO_PRINTK_HEADER x)
#else
#define QDIO_PRINT_WARN(x...)
#endif

#if QDIO_VERBOSE_LEVEL>4
#define QDIO_PRINT_ERR(x...) printk( QDIO_PRINTK_HEADER x)
#else
#define QDIO_PRINT_ERR(x...)
#endif

#if QDIO_VERBOSE_LEVEL>3
#define QDIO_PRINT_CRIT(x...) printk( QDIO_PRINTK_HEADER x)
#else
#define QDIO_PRINT_CRIT(x...)
#endif

#if QDIO_VERBOSE_LEVEL>2
#define QDIO_PRINT_ALERT(x...) printk( QDIO_PRINTK_HEADER x)
#else
#define QDIO_PRINT_ALERT(x...)
#endif

#if QDIO_VERBOSE_LEVEL>1
#define QDIO_PRINT_EMERG(x...) printk( QDIO_PRINTK_HEADER x)
#else
#define QDIO_PRINT_EMERG(x...)
#endif

#endif /* __QDIO_H__ */
