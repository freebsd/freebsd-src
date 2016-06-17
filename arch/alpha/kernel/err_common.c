/*
 *	linux/arch/alpha/kernel/err_common.c
 *
 *	Copyright (C) 2000 Jeff Wiedemeier (Compaq Computer Corporation)
 *
 *	Error handling code supporting Alpha systems
 */

#include <linux/init.h>
#include <linux/pci.h>
#include <linux/sched.h>

#include <asm/io.h>
#include <asm/hwrpb.h>
#include <asm/smp.h>

#include "err_impl.h"
#include "proto.h"

/*
 * err_print_prefix -- error handling print routines should prefix
 * all prints with this
 */
char *err_print_prefix = KERN_NOTICE;

/*
 * Forward references
 */
static void el_print_timestamp(union el_timestamp);
static void el_process_subpackets(struct el_subpacket *, int);


/*
 * Generic
 */
void
mchk_dump_mem(void *data, size_t length, char **annotation)
{
	unsigned long *ldata = data;
	size_t i;
	
	for (i = 0; (i * sizeof(*ldata)) < length; i++) {
		if (annotation && !annotation[i]) 
			annotation = NULL;
		printk("%s    %08x: %016lx    %s\n",
		       err_print_prefix,
		       (unsigned)(i * sizeof(*ldata)), ldata[i],
		       annotation ? annotation[i] : "");
	}
}

void
mchk_dump_logout_frame(struct el_common *mchk_header)
{
	printk("%s  -- Frame Header --\n"
	         "    Frame Size:   %d (0x%x) bytes\n"
	         "    Flags:        %s%s\n"
	         "    MCHK Code:    0x%x\n"
	         "    Frame Rev:    %d\n"
	         "    Proc Offset:  0x%08x\n"
	         "    Sys Offset:   0x%08x\n"
  	         "  -- Processor Region --\n",
	       err_print_prefix, 
	       mchk_header->size, mchk_header->size,
	       mchk_header->retry ? "RETRY " : "", 
  	         mchk_header->err2 ? "SECOND_ERR " : "",
	       mchk_header->code,
	       mchk_header->frame_rev,
	       mchk_header->proc_offset,
	       mchk_header->sys_offset);

	mchk_dump_mem((void *)
		      ((unsigned long)mchk_header + mchk_header->proc_offset),
		      mchk_header->sys_offset - mchk_header->proc_offset,
		      NULL);
	
	printk("%s  -- System Region --\n", err_print_prefix);
	mchk_dump_mem((void *)
		      ((unsigned long)mchk_header + mchk_header->sys_offset),
		      mchk_header->size - mchk_header->sys_offset,
		      NULL);
	printk("%s  -- End of Frame --\n", err_print_prefix);
}


/*
 * EV7 generic
 */
#if defined(CONFIG_ALPHA_GENERIC) || defined(CONFIG_ALPHA_EV7)

void
ev7_machine_check(u64 vector, u64 la_ptr, struct pt_regs *regs)
{
	/*
	 * Sync the processor
	 */
	mb();
	draina();

	/*
	 * Parse the logout frame without printing first. If the only error(s)
	 * found are have a disposition of "dismiss", then just dismiss them
	 * and don't print any message
	 */
	printk("%sEV7 MACHINE CHECK vector %lx\n", err_print_prefix, vector);

	/* 
	 * Release the logout frame 
	 */
	wrmces(0x7);
	mb();
}

struct ev7_pal_subpacket {
	union {
		struct {
			u32 mchk_code;
			u32 subpacket_count;
			u64 whami;
			u64 rbox_whami;
			u64 rbox_int;
			u64 exc_addr;
			union el_timestamp timestamp;
			u64 halt_code;
			u64 reserved;
		} logout;
	} by_type;
};

static char *el_ev7_processor_subpacket_annotation[] = {
	"Subpacket Header",	"I_STAT",	"DC_STAT",
	"C_ADDR",		"C_SYNDROME_1",	"C_SYNDROME_0",
	"C_STAT",		"C_STS",	"MM_STAT",
	"EXC_ADDR",		"IER_CM",	"ISUM",
	"PAL_BASE",		"I_CTL",	"PROCESS_CONTEXT",
	"CBOX_CTL",		"CBOX_STP_CTL",	"CBOX_ACC_CTL",
	"CBOX_LCL_SET",		"CBOX_GLB_SET",	"BBOX_CTL",
	"BBOX_ERR_STS",		"BBOX_ERR_IDX",	"CBOX_DDP_ERR_STS",
	"BBOX_DAT_RMP",		NULL
};

static char *el_ev7_zbox_subpacket_annotation[] = {
	"Subpacket Header", 	
	"ZBOX(0): DRAM_ERR_STATUS_2 / DRAM_ERR_STATUS_1",
	"ZBOX(0): DRAM_ERROR_CTL    / DRAM_ERR_STATUS_3",
	"ZBOX(0): DIFT_TIMEOUT      / DRAM_ERR_ADR",
	"ZBOX(0): FRC_ERR_ADR       / DRAM_MAPPER_CTL",
	"ZBOX(0): reserved          / DIFT_ERR_STATUS",
	"ZBOX(1): DRAM_ERR_STATUS_2 / DRAM_ERR_STATUS_1",
	"ZBOX(1): DRAM_ERROR_CTL    / DRAM_ERR_STATUS_3",
	"ZBOX(1): DIFT_TIMEOUT      / DRAM_ERR_ADR",
	"ZBOX(1): FRC_ERR_ADR       / DRAM_MAPPER_CTL",
	"ZBOX(1): reserved          / DIFT_ERR_STATUS",
	"CBOX_CTL",		"CBOX_STP_CTL",
	"ZBOX(0)_ERROR_PA",	"ZBOX(1)_ERROR_PA",
	"ZBOX(0)_ORED_SYNDROME","ZBOX(1)_ORED_SYNDROME",
	NULL
};

static char *el_ev7_rbox_subpacket_annotation[] = {
	"Subpacket Header",	"RBOX_CFG",	"RBOX_N_CFG",
	"RBOX_S_CFG",		"RBOX_E_CFG",	"RBOX_W_CFG",
	"RBOX_N_ERR",		"RBOX_S_ERR",	"RBOX_E_ERR",
	"RBOX_W_ERR",		"RBOX_IO_CFG",	"RBOX_IO_ERR",
	"RBOX_L_ERR",		"RBOX_WHOAMI",	"RBOX_IMASL",
	"RBOX_INTQ",		"RBOX_INT",	NULL
};

static char *el_ev7_io_subpacket_annotation[] = {
	"Subpacket Header",	"IO_ASIC_REV",	"IO_SYS_REV",
	"IO7_UPH",		"HPI_CTL",	"CRD_CTL",
	"HEI_CTL",		"PO7_ERROR_SUM","PO7_UNCRR_SYM",
	"PO7_CRRCT_SYM",	"PO7_UGBGE_SYM","PO7_ERR_PKT0",
	"PO7_ERR_PKT1",		"reserved",	"reserved",
	"PO0_ERR_SUM",		"PO0_TLB_ERR",	"PO0_SPL_COMPLT",
	"PO0_TRANS_SUM",	"PO0_FIRST_ERR","PO0_MULT_ERR",
	"DM CSR PH",		"DM CSR PH",	"DM CSR PH",
	"DM CSR PH",		"reserved",
	"PO1_ERR_SUM",		"PO1_TLB_ERR",	"PO1_SPL_COMPLT",
	"PO1_TRANS_SUM",	"PO1_FIRST_ERR","PO1_MULT_ERR",
	"DM CSR PH",		"DM CSR PH",	"DM CSR PH",
	"DM CSR PH",		"reserved",
	"PO2_ERR_SUM",		"PO2_TLB_ERR",	"PO2_SPL_COMPLT",
	"PO2_TRANS_SUM",	"PO2_FIRST_ERR","PO2_MULT_ERR",
	"DM CSR PH",		"DM CSR PH",	"DM CSR PH",
	"DM CSR PH",		"reserved",
	"PO3_ERR_SUM",		"PO3_TLB_ERR",	"PO3_SPL_COMPLT",
	"PO3_TRANS_SUM",	"PO3_FIRST_ERR","PO3_MULT_ERR",
	"DM CSR PH",		"DM CSR PH",	"DM CSR PH",
	"DM CSR PH",		"reserved",	
	NULL
};
	
static struct el_subpacket_annotation el_ev7_pal_annotations[] = {
	SUBPACKET_ANNOTATION(EL_CLASS__PAL,
			     EL_TYPE__PAL__EV7_PROCESSOR,
			     1,
			     "EV7 Processor Subpacket",
			     el_ev7_processor_subpacket_annotation),
	SUBPACKET_ANNOTATION(EL_CLASS__PAL,
			     EL_TYPE__PAL__EV7_ZBOX,
			     1,
			     "EV7 ZBOX Subpacket",
			     el_ev7_zbox_subpacket_annotation),
	SUBPACKET_ANNOTATION(EL_CLASS__PAL,
			     EL_TYPE__PAL__EV7_RBOX,
			     1,
			     "EV7 RBOX Subpacket",
			     el_ev7_rbox_subpacket_annotation),
	SUBPACKET_ANNOTATION(EL_CLASS__PAL,
			     EL_TYPE__PAL__EV7_IO,
			     1,
			     "EV7 IO Subpacket",
			     el_ev7_io_subpacket_annotation)
};

static struct el_subpacket *
ev7_process_pal_subpacket(struct el_subpacket *header)
{
	struct ev7_pal_subpacket *packet;

	if (header->class != EL_CLASS__PAL) {
		printk("%s  ** Unexpected header CLASS %d TYPE %d, aborting\n",
		       err_print_prefix,
		       header->class, header->type);
		return NULL;
	}

	packet = (struct ev7_pal_subpacket *)header->by_type.raw.data_start;

	switch(header->type) {
	case EL_TYPE__PAL__LOGOUT_FRAME:
		printk("%s*** MCHK occurred on LPID %ld (RBOX %lx)\n",
		       err_print_prefix,
		       packet->by_type.logout.whami, 
		       packet->by_type.logout.rbox_whami);
		el_print_timestamp(packet->by_type.logout.timestamp);
		printk("%s  EXC_ADDR: %016lx\n"
		         "  HALT_CODE: %lx\n", 
		       err_print_prefix,
		       packet->by_type.logout.exc_addr,
		       packet->by_type.logout.halt_code);
		el_process_subpackets(header,
                                      packet->by_type.logout.subpacket_count);
		break;
	default:
		printk("%s  ** PAL TYPE %d SUBPACKET\n", 
		       err_print_prefix,
		       header->type);
		el_annotate_subpacket(header);
		break;
	}
	
	return (struct el_subpacket *)((unsigned long)header + header->length);
}

struct el_subpacket_handler ev7_pal_subpacket_handler =
	SUBPACKET_HANDLER_INIT(EL_CLASS__PAL, ev7_process_pal_subpacket);

void 
ev7_register_error_handlers(void)
{
	int i;

	for(i = 0;
	    i<sizeof(el_ev7_pal_annotations)/sizeof(el_ev7_pal_annotations[1]);
	    i++) {
		cdl_register_subpacket_annotation(&el_ev7_pal_annotations[i]);
	}	
	cdl_register_subpacket_handler(&ev7_pal_subpacket_handler);
}

#endif /* CONFIG_ALPHA_GENERIC || CONFIG_ALPHA_EV7 */


/*
 * EV6 generic
 */

#if defined(CONFIG_ALPHA_GENERIC) || defined(CONFIG_ALPHA_EV6)

static int
ev6_parse_ibox(u64 i_stat, int print)
{
	int status = MCHK_DISPOSITION_REPORT;

#define EV6__I_STAT__PAR	(1UL << 29)
#define EV6__I_STAT__ERRMASK	(EV6__I_STAT__PAR)

	if (!(i_stat & EV6__I_STAT__ERRMASK))
		return MCHK_DISPOSITION_UNKNOWN_ERROR;

	if (!print)
		return status;

	if (i_stat & EV6__I_STAT__PAR)
		printk("%s    Icache parity error\n", err_print_prefix);

	return status;
}

static int
ev6_parse_mbox(u64 mm_stat, u64 d_stat, u64 c_stat, int print)
{
	int status = MCHK_DISPOSITION_REPORT;

#define EV6__MM_STAT__DC_TAG_PERR	(1UL << 10)
#define EV6__MM_STAT__ERRMASK		(EV6__MM_STAT__DC_TAG_PERR)
#define EV6__D_STAT__TPERR_P0		(1UL << 0)
#define EV6__D_STAT__TPERR_P1		(1UL << 1)
#define EV6__D_STAT__ECC_ERR_ST		(1UL << 2)
#define EV6__D_STAT__ECC_ERR_LD		(1UL << 3)
#define EV6__D_STAT__SEO		(1UL << 4)
#define EV6__D_STAT__ERRMASK		(EV6__D_STAT__TPERR_P0 |	\
                                         EV6__D_STAT__TPERR_P1 | 	\
                                         EV6__D_STAT__ECC_ERR_ST | 	\
                                         EV6__D_STAT__ECC_ERR_LD | 	\
                                         EV6__D_STAT__SEO)

	if (!(d_stat & EV6__D_STAT__ERRMASK) && 
	    !(mm_stat & EV6__MM_STAT__ERRMASK))
		return MCHK_DISPOSITION_UNKNOWN_ERROR;

	if (!print)
		return status;

	if (mm_stat & EV6__MM_STAT__DC_TAG_PERR)
		printk("%s    Dcache tag parity error on probe\n",
		       err_print_prefix);
	if (d_stat & EV6__D_STAT__TPERR_P0)
		printk("%s    Dcache tag parity error - pipe 0\n",
		       err_print_prefix);
	if (d_stat & EV6__D_STAT__TPERR_P1)
		printk("%s    Dcache tag parity error - pipe 1\n",
		       err_print_prefix);
	if (d_stat & EV6__D_STAT__ECC_ERR_ST)
		printk("%s    ECC error occurred on a store\n", 
		       err_print_prefix);
	if (d_stat & EV6__D_STAT__ECC_ERR_LD)
		printk("%s    ECC error occurred on a %s load\n",
		       err_print_prefix,
		       c_stat ? "" : "speculative ");
	if (d_stat & EV6__D_STAT__SEO)
		printk("%s    Dcache second error\n", err_print_prefix);

	return status;
}

static int
ev6_parse_cbox(u64 c_addr, u64 c1_syn, u64 c2_syn, 
	       u64 c_stat, u64 c_sts, int print)
{
	char *sourcename[] = { "UNKNOWN", "UNKNOWN", "UNKNOWN",
			       "MEMORY", "BCACHE", "DCACHE", 
			       "BCACHE PROBE", "BCACHE PROBE" };
	char *streamname[] = { "D", "I" };
	char *bitsname[] = { "SINGLE", "DOUBLE" };
	int status = MCHK_DISPOSITION_REPORT;
	int source = -1, stream = -1, bits = -1;

#define EV6__C_STAT__BC_PERR		(0x01)
#define EV6__C_STAT__DC_PERR		(0x02)
#define EV6__C_STAT__DSTREAM_MEM_ERR	(0x03)
#define EV6__C_STAT__DSTREAM_BC_ERR	(0x04)
#define EV6__C_STAT__DSTREAM_DC_ERR	(0x05)
#define EV6__C_STAT__PROBE_BC_ERR0	(0x06)	/* both 6 and 7 indicate... */
#define EV6__C_STAT__PROBE_BC_ERR1	(0x07)	/* ...probe bc error.       */
#define EV6__C_STAT__ISTREAM_MEM_ERR	(0x0B)
#define EV6__C_STAT__ISTREAM_BC_ERR	(0x0C)
#define EV6__C_STAT__DSTREAM_MEM_DBL	(0x13)
#define EV6__C_STAT__DSTREAM_BC_DBL	(0x14)
#define EV6__C_STAT__ISTREAM_MEM_DBL	(0x1B)
#define EV6__C_STAT__ISTREAM_BC_DBL	(0x1C)
#define EV6__C_STAT__SOURCE_MEMORY	(0x03)
#define EV6__C_STAT__SOURCE_BCACHE	(0x04)
#define EV6__C_STAT__SOURCE__S		(0)
#define EV6__C_STAT__SOURCE__M 		(0x07)
#define EV6__C_STAT__ISTREAM__S		(3)
#define EV6__C_STAT__ISTREAM__M		(0x01)
#define EV6__C_STAT__DOUBLE__S		(4)
#define EV6__C_STAT__DOUBLE__M		(0x01)
#define EV6__C_STAT__ERRMASK		(0x1F)
#define EV6__C_STS__SHARED		(1 << 0)
#define EV6__C_STS__DIRTY		(1 << 1)
#define EV6__C_STS__VALID		(1 << 2)
#define EV6__C_STS__PARITY		(1 << 3)

	if (!(c_stat & EV6__C_STAT__ERRMASK))
		return MCHK_DISPOSITION_UNKNOWN_ERROR;

	if (!print)
		return status;

	source = EXTRACT(c_stat, EV6__C_STAT__SOURCE);
	stream = EXTRACT(c_stat, EV6__C_STAT__ISTREAM);
	bits = EXTRACT(c_stat, EV6__C_STAT__DOUBLE);

	if (c_stat & EV6__C_STAT__BC_PERR) {
		printk("%s    Bcache tag parity error\n", err_print_prefix);
		source = -1;
	}

	if (c_stat & EV6__C_STAT__DC_PERR) {
		printk("%s    Dcache tag parity error\n", err_print_prefix);
		source = -1;
	}

	if (c_stat == EV6__C_STAT__PROBE_BC_ERR0 ||
	    c_stat == EV6__C_STAT__PROBE_BC_ERR1) {
		printk("%s    Bcache single-bit error on a probe hit\n",
		       err_print_prefix);
		source = -1;
	}

	if (source != -1) 
		printk("%s    %s-STREAM %s-BIT ECC error from %s\n",
		       err_print_prefix,
		       streamname[stream], bitsname[bits], sourcename[source]);

	printk("%s    Address: 0x%016lx\n"
	         "    Syndrome[upper.lower]: %02lx.%02lx\n", 
	       err_print_prefix,
	       c_addr,
	       c2_syn, c1_syn);

	if (source == EV6__C_STAT__SOURCE_MEMORY ||
	    source == EV6__C_STAT__SOURCE_BCACHE) 
		printk("%s    Block status: %s%s%s%s\n",
		       err_print_prefix,
		       (c_sts & EV6__C_STS__SHARED) ? "SHARED " : "",
		       (c_sts & EV6__C_STS__DIRTY)  ? "DIRTY "  : "",
		       (c_sts & EV6__C_STS__VALID)  ? "VALID "  : "",
		       (c_sts & EV6__C_STS__PARITY) ? "PARITY " : "");
		
	return status;
}

int
ev6_process_logout_frame(struct el_common *mchk_header, int print)
{
	struct el_common_EV6_mcheck *ev6mchk = 
		(struct el_common_EV6_mcheck *)mchk_header;
	int status = MCHK_DISPOSITION_UNKNOWN_ERROR;

	status |= ev6_parse_ibox(ev6mchk->I_STAT, print);
	status |= ev6_parse_mbox(ev6mchk->MM_STAT, ev6mchk->DC_STAT, 
				 ev6mchk->C_STAT, print);
	status |= ev6_parse_cbox(ev6mchk->C_ADDR, ev6mchk->DC1_SYNDROME,
				 ev6mchk->DC0_SYNDROME, ev6mchk->C_STAT,
				 ev6mchk->C_STS, print);

	if (!print)
		return status;

	if (status != MCHK_DISPOSITION_DISMISS) {
		char *saved_err_prefix = err_print_prefix;

		/*
		 * Dump some additional information from the frame
		 */
		printk("%s    EXC_ADDR: 0x%016lx   IER_CM: 0x%016lx"
		            "   ISUM: 0x%016lx\n"
		         "    PAL_BASE: 0x%016lx   I_CTL:  0x%016lx"
		            "   PCTX: 0x%016lx\n",
		       err_print_prefix,
		       ev6mchk->EXC_ADDR, ev6mchk->IER_CM, ev6mchk->ISUM,
		       ev6mchk->PAL_BASE, ev6mchk->I_CTL, ev6mchk->PCTX);

		if (status == MCHK_DISPOSITION_UNKNOWN_ERROR) {
			printk("%s    UNKNOWN error, frame follows:\n",
			       err_print_prefix);
		} else {
			/* had decode -- downgrade print level for frame */
			err_print_prefix = KERN_NOTICE;
		}

		mchk_dump_logout_frame(mchk_header);

		err_print_prefix = saved_err_prefix;
	}

	return status;
}

void
ev6_machine_check(u64 vector, u64 la_ptr, struct pt_regs *regs)
{
	struct el_common *mchk_header = (struct el_common *)la_ptr;

	/*
	 * Sync the processor
	 */
	mb();
	draina();

	/*
	 * Parse the logout frame without printing first. If the only error(s)
	 * found are have a disposition of "dismiss", then just dismiss them
	 * and don't print any message
	 */
	if (ev6_process_logout_frame(mchk_header, 0) != 
	    MCHK_DISPOSITION_DISMISS) {
		char *saved_err_prefix = err_print_prefix;
		err_print_prefix = KERN_CRIT;

		/*
		 * Either a nondismissable error was detected or no
		 * recognized error was detected  in the logout frame 
		 * -- report the error in either case
		 */
		printk("%s*CPU %s Error (Vector 0x%x) reported on CPU %d:\n", 
		       err_print_prefix,
		       (vector == SCB_Q_PROCERR)?"Correctable":"Uncorrectable",
		       (unsigned int)vector, (int)smp_processor_id());
		
		ev6_process_logout_frame(mchk_header, 1);
		dik_show_regs(regs, NULL);

		err_print_prefix = saved_err_prefix;
	}

	/* 
	 * Release the logout frame 
	 */
	wrmces(0x7);
	mb();
}

#endif /* CONFIG_ALPHA_GENERIC || CONFIG_ALPHA_EV6 */


/*
 * Console Data Log
 */
/* Data */
static struct el_subpacket_handler *subpacket_handler_list = NULL;
static struct el_subpacket_annotation *subpacket_annotation_list = NULL;

static void
el_print_timestamp(union el_timestamp timestamp)
{
	if (timestamp.as_int)
		printk("%s  TIMESTAMP: %d/%d/%02d %d:%02d:%0d\n", 
		       err_print_prefix,
		       timestamp.b.month, timestamp.b.day,
		       timestamp.b.year, timestamp.b.hour,
		       timestamp.b.minute, timestamp.b.second);
}

static struct el_subpacket *
el_process_header_subpacket(struct el_subpacket *header)
{
	union el_timestamp timestamp;
	char *name = "UNKNOWN EVENT";
	int packet_count = 0;
	int length = 0;

	if (header->class != EL_CLASS__HEADER) {
		printk("%s** Unexpected header CLASS %d TYPE %d, aborting\n",
		       err_print_prefix,
		       header->class, header->type);
		return NULL;
	}

	switch(header->type) {
	case EL_TYPE__HEADER__SYSTEM_ERROR_FRAME:
		name = "SYSTEM ERROR";
		length = header->by_type.sys_err.frame_length;
		packet_count = 
			header->by_type.sys_err.frame_packet_count;
		timestamp.as_int = 0;
		break;
	case EL_TYPE__HEADER__SYSTEM_EVENT_FRAME:
		name = "SYSTEM EVENT";
		length = header->by_type.sys_event.frame_length;
		packet_count = 
			header->by_type.sys_event.frame_packet_count;
		timestamp = header->by_type.sys_event.timestamp;
		break;
	case EL_TYPE__HEADER__HALT_FRAME:
		name = "ERROR HALT";
		length = header->by_type.err_halt.frame_length;
		packet_count = 
			header->by_type.err_halt.frame_packet_count;
		timestamp = header->by_type.err_halt.timestamp;
		break;
	case EL_TYPE__HEADER__LOGOUT_FRAME:
		name = "LOGOUT FRAME";
		length = header->by_type.logout_header.frame_length;
		packet_count = 1;
		timestamp.as_int = 0;
		break;
	default: /* Unknown */
		printk("%s** Unknown header - CLASS %d TYPE %d, aborting\n",
		       err_print_prefix,
		       header->class, header->type);
		return NULL;		
	}

	printk("%s*** %s:\n"
	         "  CLASS %d, TYPE %d\n", 
	       err_print_prefix,
	       name,
	       header->class, header->type);
	el_print_timestamp(timestamp);
	
	/*
	 * Process the subpackets
	 */
	el_process_subpackets(header, packet_count);

	/* return the next header */
	header = (struct el_subpacket *)
		((unsigned long)header + header->length + length);
	return header;
}

static void
el_process_subpackets(struct el_subpacket *header, int packet_count)
{
	struct el_subpacket *subpacket;
	int i;

	subpacket = (struct el_subpacket *)
		((unsigned long)header + header->length);

	for (i = 0; subpacket && i < packet_count; i++) {
		printk("%sPROCESSING SUBPACKET %d\n", err_print_prefix, i);
		subpacket = el_process_subpacket(subpacket);
	}
}

static struct el_subpacket *
el_process_subpacket_reg(struct el_subpacket *header)
{
	struct el_subpacket *next = NULL;
	struct el_subpacket_handler *h = subpacket_handler_list;

	for (; h && h->class != header->class; h = h->next);
	if (h) next = h->handler(header);

	return next;
}

struct el_subpacket *
el_process_subpacket(struct el_subpacket *header)
{
	struct el_subpacket *next = NULL;

	switch(header->class) {
	case EL_CLASS__TERMINATION:
		/* Termination packet, there are no more */
		break;
	case EL_CLASS__HEADER: 
		next = el_process_header_subpacket(header);
		break;
	default:
		if (NULL == (next = el_process_subpacket_reg(header))) {
			printk("%s** Unexpected header CLASS %d TYPE %d"
			       " -- aborting.\n",
			       err_print_prefix,
			       header->class, header->type);
		}
		break;
	}

	return next;
}

void 
el_annotate_subpacket(struct el_subpacket *header)
{
	struct el_subpacket_annotation *a;
	char **annotation = NULL;

	for (a = subpacket_annotation_list; a; a = a->next) {
		if (a->class == header->class &&
		    a->type == header->type &&
		    a->revision == header->revision) {
			/*
			 * We found the annotation
			 */
			annotation = a->annotation;
			printk("%s  %s\n", err_print_prefix, a->description);
			break;
		}
	}

	mchk_dump_mem(header, header->length, annotation);
}

static void __init
cdl_process_console_data_log(int cpu, struct percpu_struct *pcpu)
{
	struct el_subpacket *header = (struct el_subpacket *)
		(IDENT_ADDR | pcpu->console_data_log_pa);
	int err;

	printk("%s******* CONSOLE DATA LOG FOR CPU %d. *******\n"
	         "*** Error(s) were logged on a previous boot\n",
	       err_print_prefix, cpu);
	
	for (err = 0; header && (header->class != EL_CLASS__TERMINATION); err++)
		header = el_process_subpacket(header);

	/* let the console know it's ok to clear the error(s) at restart */
	pcpu->console_data_log_pa = 0;

	printk("%s*** %d total error(s) logged\n"
	         "**** END OF CONSOLE DATA LOG FOR CPU %d ****\n", 
	       err_print_prefix, err, cpu);
}

void __init
cdl_check_console_data_log(void)
{
	struct percpu_struct *pcpu;
	unsigned long cpu;

	for (cpu = 0; cpu < hwrpb->nr_processors; cpu++) {
		pcpu = (struct percpu_struct *)
			((unsigned long)hwrpb + hwrpb->processor_offset 
			 + cpu * hwrpb->processor_size);
		if (pcpu->console_data_log_pa)
			cdl_process_console_data_log(cpu, pcpu);
	}

}

int __init
cdl_register_subpacket_annotation(struct el_subpacket_annotation *new)
{
	struct el_subpacket_annotation *a = subpacket_annotation_list;

	if (a == NULL) subpacket_annotation_list = new;
	else {
		for (; a->next != NULL; a = a->next) {
			if ((a->class == new->class && a->type == new->type) ||
			    a == new) {
				printk("Attempted to re-register "
				       "subpacket annotation\n");
				return -EINVAL;
			}
		}
		a->next = new;
	}
	new->next = NULL;

	return 0;
}

int __init
cdl_register_subpacket_handler(struct el_subpacket_handler *new)
{
	struct el_subpacket_handler *h = subpacket_handler_list;

	if (h == NULL) subpacket_handler_list = new;
	else {
		for (; h->next != NULL; h = h->next) {
			if (h->class == new->class || h == new) {
				printk("Attempted to re-register "
				       "subpacket handler\n");
				return -EINVAL;
			}
		}
		h->next = new;
	}
	new->next = NULL;

	return 0;
}

