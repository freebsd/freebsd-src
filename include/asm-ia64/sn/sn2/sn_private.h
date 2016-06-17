/* $Id: sn_private.h,v 1.1 2002/02/28 17:31:26 marcelo Exp $
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1992 - 1997, 2000-2003 Silicon Graphics, Inc. All rights reserved.
 */
#ifndef _ASM_IA64_SN_SN2_SN_PRIVATE_H
#define _ASM_IA64_SN_SN2_SN_PRIVATE_H

#include <asm/sn/nodepda.h>
#include <asm/sn/io.h>
#include <asm/sn/xtalk/xwidget.h>
#include <asm/sn/xtalk/xtalk_private.h>

extern nasid_t master_nasid;

/* promif.c */
extern void he_arcs_set_vectors(void);
extern void mem_init(void);
extern void cpu_unenable(cpuid_t);
extern nasid_t get_lowest_nasid(void);
extern __psunsigned_t get_master_bridge_base(void);
extern void set_master_bridge_base(void);
extern int check_nasid_equiv(nasid_t, nasid_t);
extern char get_console_pcislot(void);

extern int is_master_baseio_nasid_widget(nasid_t test_nasid, xwidgetnum_t test_wid);

/* memsupport.c */
extern void poison_state_alter_range(__psunsigned_t start, int len, int poison);
extern int memory_present(paddr_t);
extern int memory_read_accessible(paddr_t);
extern int memory_write_accessible(paddr_t);
extern void memory_set_access(paddr_t, int, int);
extern void show_dir_state(paddr_t, void (*)(char *, ...));
extern void check_dir_state(nasid_t, int, void (*)(char *, ...));
extern void set_dir_owner(paddr_t, int);
extern void set_dir_state(paddr_t, int);
extern void set_dir_state_POISONED(paddr_t);
extern void set_dir_state_UNOWNED(paddr_t);
extern int is_POISONED_dir_state(paddr_t);
extern int is_UNOWNED_dir_state(paddr_t);
#ifdef LATER
extern void get_dir_ent(paddr_t paddr, int *state,
			uint64_t *vec_ptr, hubreg_t *elo);
#endif

/* intr.c */
extern int intr_reserve_level(cpuid_t cpu, int level, int err, vertex_hdl_t owner_dev, char *name);
extern void intr_unreserve_level(cpuid_t cpu, int level);
extern int intr_connect_level(cpuid_t cpu, int bit, ilvl_t mask_no, 
			intr_func_t intr_prefunc);
extern int intr_disconnect_level(cpuid_t cpu, int bit);
extern cpuid_t intr_heuristic(vertex_hdl_t dev, device_desc_t dev_desc,
			      int req_bit,int intr_resflags,vertex_hdl_t owner_dev,
			      char *intr_name,int *resp_bit);
extern void intr_block_bit(cpuid_t cpu, int bit);
extern void intr_unblock_bit(cpuid_t cpu, int bit);
extern void setrtvector(intr_func_t);
extern void install_cpuintr(cpuid_t cpu);
extern void install_dbgintr(cpuid_t cpu);
extern void install_tlbintr(cpuid_t cpu);
extern void hub_migrintr_init(cnodeid_t /*cnode*/);
extern int cause_intr_connect(int level, intr_func_t handler, uint intr_spl_mask);
extern int cause_intr_disconnect(int level);
extern void intr_dumpvec(cnodeid_t cnode, void (*pf)(char *, ...));

/* error_dump.c */
extern char *hub_rrb_err_type[];
extern char *hub_wrb_err_type[];

void nmi_dump(void);
void install_cpu_nmi_handler(int slice);

/* klclock.c */
extern void hub_rtc_init(cnodeid_t);

/* bte.c */
void bte_lateinit(void);
void bte_wait_for_xfer_completion(void *);

/* klgraph.c */
void klhwg_add_all_nodes(vertex_hdl_t);
void klhwg_add_all_modules(vertex_hdl_t);

/* klidbg.c */
void install_klidbg_functions(void);

/* klnuma.c */
extern void replicate_kernel_text(int numnodes);
extern __psunsigned_t get_freemem_start(cnodeid_t cnode);
extern void setup_replication_mask(int maxnodes);

/* init.c */
extern cnodeid_t get_compact_nodeid(void);	/* get compact node id */
extern void init_platform_nodepda(nodepda_t *npda, cnodeid_t node);
extern void per_cpu_init(void);
extern int is_fine_dirmode(void);
extern void update_node_information(cnodeid_t);
 
/* shubio.c */
extern void hubio_init(void);
extern void hub_merge_clean(nasid_t nasid);
extern void hub_set_piomode(nasid_t nasid, int conveyor);

/* shuberror.c */
extern void hub_error_init(cnodeid_t);
extern void dump_error_spool(cpuid_t cpu, void (*pf)(char *, ...));
extern void hubni_error_handler(char *, int);
extern int check_ni_errors(void);

/* Used for debugger to signal upper software a breakpoint has taken place */

extern void		*debugger_update;
extern __psunsigned_t	debugger_stopped;

/* 
 * piomap, created by shub_pio_alloc.
 * xtalk_info MUST BE FIRST, since this structure is cast to a
 * xtalk_piomap_s by generic xtalk routines.
 */
struct hub_piomap_s {
	struct xtalk_piomap_s	hpio_xtalk_info;/* standard crosstalk pio info */
	vertex_hdl_t		hpio_hub;	/* which shub's mapping registers are set up */
	short			hpio_holdcnt;	/* count of current users of bigwin mapping */
	char			hpio_bigwin_num;/* if big window map, which one */
	int 			hpio_flags;	/* defined below */
};
/* hub_piomap flags */
#define HUB_PIOMAP_IS_VALID		0x1
#define HUB_PIOMAP_IS_BIGWINDOW		0x2
#define HUB_PIOMAP_IS_FIXED		0x4

#define	hub_piomap_xt_piomap(hp)	(&hp->hpio_xtalk_info)
#define	hub_piomap_hub_v(hp)	(hp->hpio_hub)
#define	hub_piomap_winnum(hp)	(hp->hpio_bigwin_num)

/* 
 * dmamap, created by shub_pio_alloc.
 * xtalk_info MUST BE FIRST, since this structure is cast to a
 * xtalk_dmamap_s by generic xtalk routines.
 */
struct hub_dmamap_s {
	struct xtalk_dmamap_s	hdma_xtalk_info;/* standard crosstalk dma info */
	vertex_hdl_t		hdma_hub;	/* which shub we go through */
	int			hdma_flags;	/* defined below */
};
/* shub_dmamap flags */
#define HUB_DMAMAP_IS_VALID		0x1
#define HUB_DMAMAP_USED			0x2
#define	HUB_DMAMAP_IS_FIXED		0x4

/* 
 * interrupt handle, created by shub_intr_alloc.
 * xtalk_info MUST BE FIRST, since this structure is cast to a
 * xtalk_intr_s by generic xtalk routines.
 */
struct hub_intr_s {
	struct xtalk_intr_s	i_xtalk_info;	/* standard crosstalk intr info */
	ilvl_t			i_swlevel;	/* software level for blocking intr */
	cpuid_t			i_cpuid;	/* which cpu */
	int			i_bit;		/* which bit */
	int			i_flags;
};
/* flag values */
#define HUB_INTR_IS_ALLOCED	0x1	/* for debug: allocated */
#define HUB_INTR_IS_CONNECTED	0x4	/* for debug: connected to a software driver */

typedef struct hubinfo_s {
	nodepda_t			*h_nodepda;	/* pointer to node's private data area */
	cnodeid_t			h_cnodeid;	/* compact nodeid */
	nasid_t				h_nasid;	/* nasid */

	/* structures for PIO management */
	xwidgetnum_t			h_widgetid;	/* my widget # (as viewed from xbow) */
	struct hub_piomap_s		h_small_window_piomap[HUB_WIDGET_ID_MAX+1];
	sv_t				h_bwwait;	/* wait for big window to free */
	spinlock_t			h_bwlock;	/* guard big window piomap's */
	spinlock_t			h_crblock;      /* gaurd CRB error handling */
	int				h_num_big_window_fixed;	/* count number of FIXED maps */
	struct hub_piomap_s		h_big_window_piomap[HUB_NUM_BIG_WINDOW];
	hub_intr_t			hub_ii_errintr;
} *hubinfo_t;

#define hubinfo_get(vhdl, infoptr) ((void)hwgraph_info_get_LBL \
	(vhdl, INFO_LBL_NODE_INFO, (arbitrary_info_t *)infoptr))

#define hubinfo_set(vhdl, infoptr) (void)hwgraph_info_add_LBL \
	(vhdl, INFO_LBL_NODE_INFO, (arbitrary_info_t)infoptr)

#define	hubinfo_to_hubv(hinfo, hub_v)	(hinfo->h_nodepda->node_vertex)

/*
 * Hub info PIO map access functions.
 */
#define	hubinfo_bwin_piomap_get(hinfo, win) 	\
			(&hinfo->h_big_window_piomap[win])
#define	hubinfo_swin_piomap_get(hinfo, win)	\
			(&hinfo->h_small_window_piomap[win])
	
/* cpu-specific information stored under INFO_LBL_CPU_INFO */
typedef struct cpuinfo_s {
	cpuid_t		ci_cpuid;	/* CPU ID */
} *cpuinfo_t;

#define cpuinfo_get(vhdl, infoptr) ((void)hwgraph_info_get_LBL \
	(vhdl, INFO_LBL_CPU_INFO, (arbitrary_info_t *)infoptr))

#define cpuinfo_set(vhdl, infoptr) (void)hwgraph_info_add_LBL \
	(vhdl, INFO_LBL_CPU_INFO, (arbitrary_info_t)infoptr)

/* Special initialization function for xswitch vertices created during startup. */
extern void xswitch_vertex_init(vertex_hdl_t xswitch);

extern xtalk_provider_t hub_provider;

/* du.c */
int ducons_write(char *buf, int len);

/* memerror.c */

extern void install_eccintr(cpuid_t cpu);
extern void memerror_get_stats(cnodeid_t cnode,
			       int *bank_stats, int *bank_stats_max);
extern void probe_md_errors(nasid_t);
/* sysctlr.c */
extern void sysctlr_init(void);
extern void sysctlr_power_off(int sdonly);
extern void sysctlr_keepalive(void);

#define valid_cpuid(_x)		(((_x) >= 0) && ((_x) < maxcpus))

/* Useful definitions to get the memory dimm given a physical
 * address.
 */
#define paddr_dimm(_pa)		((_pa & MD_BANK_MASK) >> MD_BANK_SHFT)
#define paddr_cnode(_pa)	(NASID_TO_COMPACT_NODEID(NASID_GET(_pa)))
extern void membank_pathname_get(paddr_t,char *);

/* To redirect the output into the error buffer */
#define errbuf_print(_s)	printf("#%s",_s)

extern void crbx(nasid_t nasid, void (*pf)(char *, ...));
void bootstrap(void);

/* sndrv.c */
extern int sndrv_attach(vertex_hdl_t vertex);

#endif /* _ASM_IA64_SN_SN2_SN_PRIVATE_H */
