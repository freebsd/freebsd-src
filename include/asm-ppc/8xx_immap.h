/*
 * MPC8xx Internal Memory Map
 * Copyright (c) 1997 Dan Malek (dmalek@jlc.net)
 *
 * The I/O on the MPC860 is comprised of blocks of special registers
 * and the dual port ram for the Communication Processor Module.
 * Within this space are functional units such as the SIU, memory
 * controller, system timers, and other control functions.  It is
 * a combination that I found difficult to separate into logical
 * functional files.....but anyone else is welcome to try.  -- Dan
 */
#ifdef __KERNEL__
#ifndef __IMMAP_8XX__
#define __IMMAP_8XX__

/* System configuration registers.
*/
typedef	struct sys_conf {
	uint	sc_siumcr;
	uint	sc_sypcr;
	uint	sc_swt;
	char	res1[2];
	ushort	sc_swsr;
	uint	sc_sipend;
	uint	sc_simask;
	uint	sc_siel;
	uint	sc_sivec;
	uint	sc_tesr;
	char	res2[0xc];
	uint	sc_sdcr;
	char	res3[0x4c];
} sysconf8xx_t;

/* PCMCIA configuration registers.
*/
typedef struct pcmcia_conf {
	uint	pcmc_pbr0;
	uint	pcmc_por0;
	uint	pcmc_pbr1;
	uint	pcmc_por1;
	uint	pcmc_pbr2;
	uint	pcmc_por2;
	uint	pcmc_pbr3;
	uint	pcmc_por3;
	uint	pcmc_pbr4;
	uint	pcmc_por4;
	uint	pcmc_pbr5;
	uint	pcmc_por5;
	uint	pcmc_pbr6;
	uint	pcmc_por6;
	uint	pcmc_pbr7;
	uint	pcmc_por7;
	char	res1[0x20];
	uint	pcmc_pgcra;
	uint	pcmc_pgcrb;
	uint	pcmc_pscr;
	char	res2[4];
	uint	pcmc_pipr;
	char	res3[4];
	uint	pcmc_per;
	char	res4[4];
} pcmconf8xx_t;

/* Memory controller registers.
*/
typedef struct	mem_ctlr {
	uint	memc_br0;
	uint	memc_or0;
	uint	memc_br1;
	uint	memc_or1;
	uint	memc_br2;
	uint	memc_or2;
	uint	memc_br3;
	uint	memc_or3;
	uint	memc_br4;
	uint	memc_or4;
	uint	memc_br5;
	uint	memc_or5;
	uint	memc_br6;
	uint	memc_or6;
	uint	memc_br7;
	uint	memc_or7;
	char	res1[0x24];
	uint	memc_mar;
	uint	memc_mcr;
	char	res2[4];
	uint	memc_mamr;
	uint	memc_mbmr;
	ushort	memc_mstat;
	ushort	memc_mptpr;
	uint	memc_mdr;
	char	res3[0x80];
} memctl8xx_t;

/* System Integration Timers.
*/
typedef struct	sys_int_timers {
	ushort	sit_tbscr;
	uint	sit_tbreff0;
	uint	sit_tbreff1;
	char	res1[0x14];
	ushort	sit_rtcsc;
	uint	sit_rtc;
	uint	sit_rtsec;
	uint	sit_rtcal;
	char	res2[0x10];
	ushort	sit_piscr;
	char	res3[2];
	uint	sit_pitc;
	uint	sit_pitr;
	char	res4[0x34];
} sit8xx_t;

#define TBSCR_TBIRQ_MASK	((ushort)0xff00)
#define TBSCR_REFA		((ushort)0x0080)
#define TBSCR_REFB		((ushort)0x0040)
#define TBSCR_REFAE		((ushort)0x0008)
#define TBSCR_REFBE		((ushort)0x0004)
#define TBSCR_TBF		((ushort)0x0002)
#define TBSCR_TBE		((ushort)0x0001)

#define RTCSC_RTCIRQ_MASK	((ushort)0xff00)
#define RTCSC_SEC		((ushort)0x0080)
#define RTCSC_ALR		((ushort)0x0040)
#define RTCSC_38K		((ushort)0x0010)
#define RTCSC_SIE		((ushort)0x0008)
#define RTCSC_ALE		((ushort)0x0004)
#define RTCSC_RTF		((ushort)0x0002)
#define RTCSC_RTE		((ushort)0x0001)

#define PISCR_PIRQ_MASK		((ushort)0xff00)
#define PISCR_PS		((ushort)0x0080)
#define PISCR_PIE		((ushort)0x0004)
#define PISCR_PTF		((ushort)0x0002)
#define PISCR_PTE		((ushort)0x0001)

/* Clocks and Reset.
*/
typedef struct clk_and_reset {
	uint	car_sccr;
	uint	car_plprcr;
	uint	car_rsr;
	char	res[0x74];        /* Reserved area                  */
} car8xx_t;

/* System Integration Timers keys.
*/
typedef struct sitk {
	uint	sitk_tbscrk;
	uint	sitk_tbreff0k;
	uint	sitk_tbreff1k;
	uint	sitk_tbk;
	char	res1[0x10];
	uint	sitk_rtcsck;
	uint	sitk_rtck;
	uint	sitk_rtseck;
	uint	sitk_rtcalk;
	char	res2[0x10];
	uint	sitk_piscrk;
	uint	sitk_pitck;
	char	res3[0x38];
} sitk8xx_t;

/* Clocks and reset keys.
*/
typedef struct cark {
	uint	cark_sccrk;
	uint	cark_plprcrk;
	uint	cark_rsrk;
	char	res[0x474];
} cark8xx_t;

/* The key to unlock registers maintained by keep-alive power.
*/
#define KAPWR_KEY	((unsigned int)0x55ccaa33)

/* LCD interface.  MPC821 Only.
*/
typedef struct lcd {
	ushort	lcd_lcolr[16];
	char	res[0x20];
	uint	lcd_lccr;
	uint	lcd_lchcr;
	uint	lcd_lcvcr;
	char	res2[4];
	uint	lcd_lcfaa;
	uint	lcd_lcfba;
	char	lcd_lcsr;
	char	res3[0x7];
} lcd8xx_t;

/* I2C
*/
typedef struct i2c {
	u_char	i2c_i2mod;
	char	res1[3];
	u_char	i2c_i2add;
	char	res2[3];
	u_char	i2c_i2brg;
	char	res3[3];
	u_char	i2c_i2com;
	char	res4[3];
	u_char	i2c_i2cer;
	char	res5[3];
	u_char	i2c_i2cmr;
	char	res6[0x8b];
} i2c8xx_t;

/* DMA control/status registers.
*/
typedef struct sdma_csr {
	char	res1[4];
	uint	sdma_sdar;
	u_char	sdma_sdsr;
	char	res3[3];
	u_char	sdma_sdmr;
	char	res4[3];
	u_char	sdma_idsr1;
	char	res5[3];
	u_char	sdma_idmr1;
	char	res6[3];
	u_char	sdma_idsr2;
	char	res7[3];
	u_char	sdma_idmr2;
	char	res8[0x13];
} sdma8xx_t;

/* Communication Processor Module Interrupt Controller.
*/
typedef struct cpm_ic {
	ushort	cpic_civr;
	char	res[0xe];
	uint	cpic_cicr;
	uint	cpic_cipr;
	uint	cpic_cimr;
	uint	cpic_cisr;
} cpic8xx_t;

/* Input/Output Port control/status registers.
*/
typedef struct io_port {
	ushort	iop_padir;
	ushort	iop_papar;
	ushort	iop_paodr;
	ushort	iop_padat;
	char	res1[8];
	ushort	iop_pcdir;
	ushort	iop_pcpar;
	ushort	iop_pcso;
	ushort	iop_pcdat;
	ushort	iop_pcint;
	char	res2[6];
	ushort	iop_pddir;
	ushort	iop_pdpar;
	char	res3[2];
	ushort	iop_pddat;
	char	res4[8];
} iop8xx_t;

/* Communication Processor Module Timers
*/
typedef struct cpm_timers {
	ushort	cpmt_tgcr;
	char	res1[0xe];
	ushort	cpmt_tmr1;
	ushort	cpmt_tmr2;
	ushort	cpmt_trr1;
	ushort	cpmt_trr2;
	ushort	cpmt_tcr1;
	ushort	cpmt_tcr2;
	ushort	cpmt_tcn1;
	ushort	cpmt_tcn2;
	ushort	cpmt_tmr3;
	ushort	cpmt_tmr4;
	ushort	cpmt_trr3;
	ushort	cpmt_trr4;
	ushort	cpmt_tcr3;
	ushort	cpmt_tcr4;
	ushort	cpmt_tcn3;
	ushort	cpmt_tcn4;
	ushort	cpmt_ter1;
	ushort	cpmt_ter2;
	ushort	cpmt_ter3;
	ushort	cpmt_ter4;
	char	res2[8];
} cpmtimer8xx_t;

/* Finally, the Communication Processor stuff.....
*/
typedef struct scc {		/* Serial communication channels */
	uint	scc_gsmrl;
	uint	scc_gsmrh;
	ushort	scc_pmsr;
	char	res1[2];
	ushort	scc_todr;
	ushort	scc_dsr;
	ushort	scc_scce;
	char	res2[2];
	ushort	scc_sccm;
	char	res3;
	u_char	scc_sccs;
	char	res4[8];
} scc_t;

typedef struct smc {		/* Serial management channels */
	char	res1[2];
	ushort	smc_smcmr;
	char	res2[2];
	u_char	smc_smce;
	char	res3[3];
	u_char	smc_smcm;
	char	res4[5];
} smc_t;

/* MPC860T Fast Ethernet Controller.  It isn't part of the CPM, but
 * it fits within the address space.
 */
typedef struct fec {
	uint	fec_addr_low;		/* LS 32 bits of station address */
	ushort	fec_addr_high;		/* MS 16 bits of address */
	ushort	res1;
	uint	fec_hash_table_high;
	uint	fec_hash_table_low;
	uint	fec_r_des_start;
	uint	fec_x_des_start;
	uint	fec_r_buff_size;
	uint	res2[9];
	uint	fec_ecntrl;
	uint	fec_ievent;
	uint	fec_imask;
	uint	fec_ivec;
	uint	fec_r_des_active;
	uint	fec_x_des_active;
	uint	res3[10];
	uint	fec_mii_data;
	uint	fec_mii_speed;
	uint	res4[17];
	uint	fec_r_bound;
	uint	fec_r_fstart;
	uint	res5[6];
	uint	fec_x_fstart;
	uint	res6[17];
	uint	fec_fun_code;
	uint	res7[3];
	uint	fec_r_cntrl;
	uint	fec_r_hash;
	uint	res8[14];
	uint	fec_x_cntrl;
	uint	res9[0x1e];
} fec_t;

typedef struct comm_proc {
	/* General control and status registers.
	*/
	ushort	cp_cpcr;
	char	res1[2];
	ushort	cp_rccr;
	char	res2[6];
	ushort	cp_cpmcr1;
	ushort	cp_cpmcr2;
	ushort	cp_cpmcr3;
	ushort	cp_cpmcr4;
	char	res3[2];
	ushort	cp_rter;
	char	res4[2];
	ushort	cp_rtmr;
	char	res5[0x14];

	/* Baud rate generators.
	*/
	uint	cp_brgc1;
	uint	cp_brgc2;
	uint	cp_brgc3;
	uint	cp_brgc4;

	/* Serial Communication Channels.
	*/
	scc_t	cp_scc[4];

	/* Serial Management Channels.
	*/
	smc_t	cp_smc[2];

	/* Serial Peripheral Interface.
	*/
	ushort	cp_spmode;
	char	res6[4];
	u_char	cp_spie;
	char	res7[3];
	u_char	cp_spim;
	char	res8[2];
	u_char	cp_spcom;
	char	res9[2];

	/* Parallel Interface Port.
	*/
	char	res10[2];
	ushort	cp_pipc;
	char	res11[2];
	ushort	cp_ptpr;
	uint	cp_pbdir;
	uint	cp_pbpar;
	char	res12[2];
	ushort	cp_pbodr;
	uint	cp_pbdat;
	char	res13[0x18];

	/* Serial Interface and Time Slot Assignment.
	*/
	uint	cp_simode;
	u_char	cp_sigmr;
	char	res14;
	u_char	cp_sistr;
	u_char	cp_sicmr;
	char	res15[4];
	uint	cp_sicr;
	uint	cp_sirp;
	char	res16[0x10c];
	u_char	cp_siram[0x200];

	/* The fast ethernet controller is not really part of the CPM,
	 * but it resides in the address space.
	 */
	fec_t	cp_fec;
	char	res18[0x1000];

	/* Dual Ported RAM follows.
	 * There are many different formats for this memory area
	 * depending upon the devices used and options chosen.
	 */
	u_char	cp_dpmem[0x1000];	/* BD / Data / ucode */
	u_char	res19[0xc00];
	u_char	cp_dparam[0x400];	/* Parameter RAM */
} cpm8xx_t;

/* Internal memory map.
*/
typedef struct immap {
	sysconf8xx_t	im_siu_conf;	/* SIU Configuration */
	pcmconf8xx_t	im_pcmcia;	/* PCMCIA Configuration */
	memctl8xx_t	im_memctl;	/* Memory Controller */
	sit8xx_t	im_sit;		/* System integration timers */
	car8xx_t	im_clkrst;	/* Clocks and reset */
	sitk8xx_t	im_sitk;	/* Sys int timer keys */
	cark8xx_t	im_clkrstk;	/* Clocks and reset keys */
	lcd8xx_t	im_lcd;		/* LCD (821 only) */
	i2c8xx_t	im_i2c;		/* I2C control/status */
	sdma8xx_t	im_sdma;	/* SDMA control/status */
	cpic8xx_t	im_cpic;	/* CPM Interrupt Controller */
	iop8xx_t	im_ioport;	/* IO Port control/status */
	cpmtimer8xx_t	im_cpmtimer;	/* CPM timers */
	cpm8xx_t	im_cpm;		/* Communication processor */
} immap_t;

#endif /* __IMMAP_8XX__ */
#endif /* __KERNEL__ */
