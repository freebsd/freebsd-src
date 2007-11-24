/**************************************************************************

Copyright (c) 2007, Chelsio Inc.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

 1. Redistributions of source code must retain the above copyright notice,
    this list of conditions and the following disclaimer.

 2. Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.

 3. Neither the name of the Chelsio Corporation nor the names of its
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


***************************************************************************/
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <err.h>
#include <errno.h>
#include <inttypes.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/if_types.h>

#define NMTUS 16
#define TCB_SIZE 128
#define TCB_WORDS (TCB_SIZE / 4)
#define PROTO_SRAM_LINES 128
#define PROTO_SRAM_LINE_BITS 132
#define PROTO_SRAM_LINE_NIBBLES (132 / 4)
#define PROTO_SRAM_SIZE (PROTO_SRAM_LINE_NIBBLES * PROTO_SRAM_LINES / 2)
#define PROTO_SRAM_EEPROM_ADDR 4096

#include <cxgb_ioctl.h>
#include <common/cxgb_regs.h>
#include "version.h"

struct reg_info { 
        const char *name; 
        uint16_t addr; 
        uint16_t len; 
}; 
 

#include "reg_defs.c"
#if defined(CONFIG_T3_REGS)
# include "reg_defs_t3.c"
# include "reg_defs_t3b.c"
#endif

static const char *progname;

static void __attribute__((noreturn)) usage(FILE *fp)
{
	fprintf(fp, "Usage: %s <interface> [operation]\n", progname);
	fprintf(fp,
#ifdef CHELSIO_INTERNAL
		"\treg <address>[=<val>]               read/write register\n"
		"\ttpi <address>[=<val>]               read/write TPI register\n"
		"\tmdio <phy_addr> <mmd_addr>\n"
	        "\t     <reg_addr> [<val>]             read/write MDIO register\n"
#endif
		"\tmtus [<mtu0>...<mtuN>]              read/write MTU table\n"
#ifdef CHELSIO_INTERNAL
		"\tpm [<TX page spec> <RX page spec>]  read/write PM config\n"
		"\ttcam [<#serv> <#routes> <#filters>] read/write TCAM config\n"
		"\ttcb <index>                         read TCB\n"
#endif
		"\tregdump [<module>]                  dump registers\n"
#ifdef CHELSIO_INTERNAL
		"\ttcamdump <address> <count>          show TCAM contents\n"
		"\tcontext <type> <id>                 show an SGE context\n"
		"\tdesc <qset> <queue> <idx> [<cnt>]   dump SGE descriptors\n"
		"\tmemdump cm|tx|rx <addr> <len>       dump a mem range\n"
		"\tmeminfo                             show memory info\n"
#endif
		"\tup                                  activate TOE\n"
		"\tproto [<protocol image>]            read/write proto SRAM\n"
		"\tloadfw <FW image>                   download firmware\n"
		"\tqset [<index> [<param> <val>] ...]  read/write qset parameters\n"
		"\tqsets [<# of qsets>]                read/write # of qsets\n"
		"\ttrace tx|rx|all on|off [not]\n"
	        "\t      [<param> <val>[:<mask>]] ...  write trace parameters\n"
		"\tt1powersave [on|off]                enable/disable T1xx powersave mode\n"
		"\tpktsched port <idx> <min> <max>     set TX port scheduler params\n"
		"\tpktsched tunnelq <idx> <max>\n"
		"\t         <binding>                  set TX tunnelq scheduler params\n"
		);
	exit(fp == stderr ? 1 : 0);
}

/*
 * Make a TOETOOL ioctl call.
 */
static int
doit(const char *iff_name, unsigned long cmd, void *data)
{
	static int fd = 0;
	
	if (fd == 0) {
		char buf[64];
		snprintf(buf, 64, "/dev/%s", iff_name);

		if ((fd = open(buf, O_RDWR)) < 0)
			return (EINVAL);
	}
	
	return ioctl(fd, cmd, data) < 0 ? -1 : 0;
}

static int get_int_arg(const char *s, uint32_t *valp)
{
	char *p;

	*valp = strtoul(s, &p, 0);
	if (*p) {
		warnx("bad parameter \"%s\"", s);
		return -1;
	}
	return 0;
}

static uint32_t
read_reg(const char *iff_name, uint32_t addr)
{
	struct ch_reg reg;

	reg.addr = addr;
	
	if (doit(iff_name, CHELSIO_GETREG, &reg) < 0)
		err(1, "register read");
	return reg.val;
}

static void
write_reg(const char *iff_name, uint32_t addr, uint32_t val)
{
	struct ch_reg ch_reg;

	ch_reg.addr = addr;
	ch_reg.val = val;
	
	if (doit(iff_name, CHELSIO_SETREG, &ch_reg) < 0)
		err(1, "register write");
}

static int register_io(int argc, char *argv[], int start_arg,
		       const char *iff_name)
{
	char *p;
	uint32_t addr, val = 0, write = 0;

	if (argc != start_arg + 1) return -1;

	addr = strtoul(argv[start_arg], &p, 0);
	if (p == argv[start_arg]) return -1;
	if (*p == '=' && p[1]) {
		val = strtoul(p + 1, &p, 0);
		write = 1;
	}
	if (*p) {
		warnx("bad parameter \"%s\"", argv[start_arg]);
		return -1;
	}

	if (write)
		write_reg(iff_name, addr, val);
	else {
		val = read_reg(iff_name, addr);
		printf("%#x [%u]\n", val, val);
	}
	return 0;
}

static int mdio_io(int argc, char *argv[], int start_arg, const char *iff_name) 
{ 
        struct ifreq ifr; 
        struct mii_data p;
        unsigned int cmd, phy_addr, reg, mmd, val; 
 
        if (argc == start_arg + 3) 
                cmd = SIOCGMIIREG; 
        else if (argc == start_arg + 4) 
                cmd = SIOCSMIIREG; 
        else 
                return -1; 
 
        if (get_int_arg(argv[start_arg], &phy_addr) || 
            get_int_arg(argv[start_arg + 1], &mmd) || 
            get_int_arg(argv[start_arg + 2], &reg) || 
            (cmd == SIOCSMIIREG && get_int_arg(argv[start_arg + 3], &val))) 
                return -1; 

        p.phy_id  = phy_addr | (mmd << 8); 
        p.reg_num = reg; 
        p.val_in  = val; 
 
        if (doit(iff_name, cmd, &p) < 0) 
                err(1, "MDIO %s", cmd == SIOCGMIIREG ? "read" : "write"); 
        if (cmd == SIOCGMIIREG) 
                printf("%#x [%u]\n", p.val_out, p.val_out); 
        return 0; 
} 

static inline uint32_t xtract(uint32_t val, int shift, int len)
{
	return (val >> shift) & ((1 << len) - 1);
}

static int dump_block_regs(const struct reg_info *reg_array, uint32_t *regs)
{
	uint32_t reg_val = 0; // silence compiler warning

	for ( ; reg_array->name; ++reg_array)
		if (!reg_array->len) {
			reg_val = regs[reg_array->addr / 4];
			printf("[%#5x] %-40s %#-10x [%u]\n", reg_array->addr,
			       reg_array->name, reg_val, reg_val);
		} else {
			uint32_t v = xtract(reg_val, reg_array->addr,
					    reg_array->len);

			printf("        %-40s %#-10x [%u]\n", reg_array->name,
			       v, v);
		}
	return 1;
}

static int dump_regs_t2(int argc, char *argv[], int start_arg, uint32_t *regs)
{
	int match = 0;
	char *block_name = NULL;

	if (argc == start_arg + 1)
		block_name = argv[start_arg];
	else if (argc != start_arg)
		return -1;

	if (!block_name || !strcmp(block_name, "sge"))
		match += dump_block_regs(sge_regs, regs);
	if (!block_name || !strcmp(block_name, "mc3"))
		match += dump_block_regs(mc3_regs, regs);
	if (!block_name || !strcmp(block_name, "mc4"))
		match += dump_block_regs(mc4_regs, regs);
	if (!block_name || !strcmp(block_name, "tpi"))
		match += dump_block_regs(tpi_regs, regs);
	if (!block_name || !strcmp(block_name, "tp"))
		match += dump_block_regs(tp_regs, regs);
	if (!block_name || !strcmp(block_name, "rat"))
		match += dump_block_regs(rat_regs, regs);
	if (!block_name || !strcmp(block_name, "cspi"))
		match += dump_block_regs(cspi_regs, regs);
	if (!block_name || !strcmp(block_name, "espi"))
		match += dump_block_regs(espi_regs, regs);
	if (!block_name || !strcmp(block_name, "ulp"))
		match += dump_block_regs(ulp_regs, regs);
	if (!block_name || !strcmp(block_name, "pl"))
		match += dump_block_regs(pl_regs, regs);
	if (!block_name || !strcmp(block_name, "mc5"))
		match += dump_block_regs(mc5_regs, regs);
	if (!match)
		errx(1, "unknown block \"%s\"", block_name);
	return 0;
}

#if defined(CONFIG_T3_REGS)
static int dump_regs_t3(int argc, char *argv[], int start_arg, uint32_t *regs,
			int is_pcie)
{
	int match = 0;
	char *block_name = NULL;

	if (argc == start_arg + 1)
		block_name = argv[start_arg];
	else if (argc != start_arg)
		return -1;

	if (!block_name || !strcmp(block_name, "sge"))
		match += dump_block_regs(sge3_regs, regs);
	if (!block_name || !strcmp(block_name, "pci"))
		match += dump_block_regs(is_pcie ? pcie0_regs : pcix1_regs,
					 regs);
	if (!block_name || !strcmp(block_name, "t3dbg"))
		match += dump_block_regs(t3dbg_regs, regs);
	if (!block_name || !strcmp(block_name, "pmrx"))
		match += dump_block_regs(mc7_pmrx_regs, regs);
	if (!block_name || !strcmp(block_name, "pmtx"))
		match += dump_block_regs(mc7_pmtx_regs, regs);
	if (!block_name || !strcmp(block_name, "cm"))
		match += dump_block_regs(mc7_cm_regs, regs);
	if (!block_name || !strcmp(block_name, "cim"))
		match += dump_block_regs(cim_regs, regs);
	if (!block_name || !strcmp(block_name, "tp"))
		match += dump_block_regs(tp1_regs, regs);
	if (!block_name || !strcmp(block_name, "ulp_rx"))
		match += dump_block_regs(ulp2_rx_regs, regs);
	if (!block_name || !strcmp(block_name, "ulp_tx"))
		match += dump_block_regs(ulp2_tx_regs, regs);
	if (!block_name || !strcmp(block_name, "pmrx"))
		match += dump_block_regs(pm1_rx_regs, regs);
	if (!block_name || !strcmp(block_name, "pmtx"))
		match += dump_block_regs(pm1_tx_regs, regs);
	if (!block_name || !strcmp(block_name, "mps"))
		match += dump_block_regs(mps0_regs, regs);
	if (!block_name || !strcmp(block_name, "cplsw"))
		match += dump_block_regs(cpl_switch_regs, regs);
	if (!block_name || !strcmp(block_name, "smb"))
		match += dump_block_regs(smb0_regs, regs);
	if (!block_name || !strcmp(block_name, "i2c"))
		match += dump_block_regs(i2cm0_regs, regs);
	if (!block_name || !strcmp(block_name, "mi1"))
		match += dump_block_regs(mi1_regs, regs);
	if (!block_name || !strcmp(block_name, "sf"))
		match += dump_block_regs(sf1_regs, regs);
	if (!block_name || !strcmp(block_name, "pl"))
		match += dump_block_regs(pl3_regs, regs);
	if (!block_name || !strcmp(block_name, "mc5"))
		match += dump_block_regs(mc5a_regs, regs);
	if (!block_name || !strcmp(block_name, "xgmac0"))
		match += dump_block_regs(xgmac0_0_regs, regs);
	if (!block_name || !strcmp(block_name, "xgmac1"))
		match += dump_block_regs(xgmac0_1_regs, regs);
	if (!match)
		errx(1, "unknown block \"%s\"", block_name);
	return 0;
}

static int dump_regs_t3b(int argc, char *argv[], int start_arg, uint32_t *regs,
			 int is_pcie)
{
	int match = 0;
	char *block_name = NULL;

	if (argc == start_arg + 1)
		block_name = argv[start_arg];
	else if (argc != start_arg)
		return -1;

	if (!block_name || !strcmp(block_name, "sge"))
		match += dump_block_regs(t3b_sge3_regs, regs);
	if (!block_name || !strcmp(block_name, "pci"))
		match += dump_block_regs(is_pcie ? t3b_pcie0_regs :
						   t3b_pcix1_regs, regs);
	if (!block_name || !strcmp(block_name, "t3dbg"))
		match += dump_block_regs(t3b_t3dbg_regs, regs);
	if (!block_name || !strcmp(block_name, "pmrx"))
		match += dump_block_regs(t3b_mc7_pmrx_regs, regs);
	if (!block_name || !strcmp(block_name, "pmtx"))
		match += dump_block_regs(t3b_mc7_pmtx_regs, regs);
	if (!block_name || !strcmp(block_name, "cm"))
		match += dump_block_regs(t3b_mc7_cm_regs, regs);
	if (!block_name || !strcmp(block_name, "cim"))
		match += dump_block_regs(t3b_cim_regs, regs);
	if (!block_name || !strcmp(block_name, "tp"))
		match += dump_block_regs(t3b_tp1_regs, regs);
	if (!block_name || !strcmp(block_name, "ulp_rx"))
		match += dump_block_regs(t3b_ulp2_rx_regs, regs);
	if (!block_name || !strcmp(block_name, "ulp_tx"))
		match += dump_block_regs(t3b_ulp2_tx_regs, regs);
	if (!block_name || !strcmp(block_name, "pmrx"))
		match += dump_block_regs(t3b_pm1_rx_regs, regs);
	if (!block_name || !strcmp(block_name, "pmtx"))
		match += dump_block_regs(t3b_pm1_tx_regs, regs);
	if (!block_name || !strcmp(block_name, "mps"))
		match += dump_block_regs(t3b_mps0_regs, regs);
	if (!block_name || !strcmp(block_name, "cplsw"))
		match += dump_block_regs(t3b_cpl_switch_regs, regs);
	if (!block_name || !strcmp(block_name, "smb"))
		match += dump_block_regs(t3b_smb0_regs, regs);
	if (!block_name || !strcmp(block_name, "i2c"))
		match += dump_block_regs(t3b_i2cm0_regs, regs);
	if (!block_name || !strcmp(block_name, "mi1"))
		match += dump_block_regs(t3b_mi1_regs, regs);
	if (!block_name || !strcmp(block_name, "sf"))
		match += dump_block_regs(t3b_sf1_regs, regs);
	if (!block_name || !strcmp(block_name, "pl"))
		match += dump_block_regs(t3b_pl3_regs, regs);
	if (!block_name || !strcmp(block_name, "mc5"))
		match += dump_block_regs(t3b_mc5a_regs, regs);
	if (!block_name || !strcmp(block_name, "xgmac0"))
		match += dump_block_regs(t3b_xgmac0_0_regs, regs);
	if (!block_name || !strcmp(block_name, "xgmac1"))
		match += dump_block_regs(t3b_xgmac0_1_regs, regs);
	if (!match)
		errx(1, "unknown block \"%s\"", block_name);
	return 0;
}
#endif

static int
dump_regs(int argc, char *argv[], int start_arg, const char *iff_name)
{

	
	int i, vers, revision, is_pcie;
	struct ifconf_regs regs;

	regs.len = REGDUMP_SIZE;

	if ((regs.data = malloc(REGDUMP_SIZE)) == NULL)
		err(1, "can't malloc");

	if (doit(iff_name, CHELSIO_IFCONF_GETREGS, &regs))
		err(1, "can't read registers");

	vers = regs.version & 0x3ff;
	revision = (regs.version >> 10) & 0x3f;
	is_pcie = (regs.version & 0x80000000) != 0;

	if (vers <= 2)
		return dump_regs_t2(argc, argv, start_arg, (uint32_t *)regs.data);
#if defined(CONFIG_T3_REGS)
	if (vers == 3) {
		if (revision == 0)
			return dump_regs_t3(argc, argv, start_arg,
					    (uint32_t *)regs.data, is_pcie);
		if (revision == 2)
			return dump_regs_t3b(argc, argv, start_arg,
					     (uint32_t *)regs.data, is_pcie);
	}
#endif
	errx(1, "unknown card type %d", vers);
	return 0;
}

static int t3_meminfo(const uint32_t *regs)
{
	enum {
		SG_EGR_CNTX_BADDR       = 0x58,
		SG_CQ_CONTEXT_BADDR     = 0x6c,
		CIM_SDRAM_BASE_ADDR     = 0x28c,
		CIM_SDRAM_ADDR_SIZE     = 0x290,
		TP_CMM_MM_BASE          = 0x314,
		TP_CMM_TIMER_BASE       = 0x318,
		TP_CMM_MM_RX_FLST_BASE  = 0x460,
		TP_CMM_MM_TX_FLST_BASE  = 0x464,
		TP_CMM_MM_PS_FLST_BASE  = 0x468,
		ULPRX_ISCSI_LLIMIT      = 0x50c,
		ULPRX_ISCSI_ULIMIT      = 0x510,
		ULPRX_TDDP_LLIMIT       = 0x51c,
		ULPRX_TDDP_ULIMIT       = 0x520,
		ULPRX_STAG_LLIMIT       = 0x52c,
		ULPRX_STAG_ULIMIT       = 0x530,
		ULPRX_RQ_LLIMIT         = 0x534,
		ULPRX_RQ_ULIMIT         = 0x538,
		ULPRX_PBL_LLIMIT        = 0x53c,
		ULPRX_PBL_ULIMIT        = 0x540,
	};

	unsigned int egr_cntxt = regs[SG_EGR_CNTX_BADDR / 4],
		     cq_cntxt = regs[SG_CQ_CONTEXT_BADDR / 4],
		     timers = regs[TP_CMM_TIMER_BASE / 4] & 0xfffffff,
		     pstructs = regs[TP_CMM_MM_BASE / 4],
		     pstruct_fl = regs[TP_CMM_MM_PS_FLST_BASE / 4],
		     rx_fl = regs[TP_CMM_MM_RX_FLST_BASE / 4],
		     tx_fl = regs[TP_CMM_MM_TX_FLST_BASE / 4],
		     cim_base = regs[CIM_SDRAM_BASE_ADDR / 4],
		     cim_size = regs[CIM_SDRAM_ADDR_SIZE / 4];
	unsigned int iscsi_ll = regs[ULPRX_ISCSI_LLIMIT / 4],
		     iscsi_ul = regs[ULPRX_ISCSI_ULIMIT / 4],
		     tddp_ll = regs[ULPRX_TDDP_LLIMIT / 4],
		     tddp_ul = regs[ULPRX_TDDP_ULIMIT / 4],
		     stag_ll = regs[ULPRX_STAG_LLIMIT / 4],
		     stag_ul = regs[ULPRX_STAG_ULIMIT / 4],
		     rq_ll = regs[ULPRX_RQ_LLIMIT / 4],
		     rq_ul = regs[ULPRX_RQ_ULIMIT / 4],
		     pbl_ll = regs[ULPRX_PBL_LLIMIT / 4],
		     pbl_ul = regs[ULPRX_PBL_ULIMIT / 4];

	printf("CM memory map:\n");
	printf("  TCB region:      0x%08x - 0x%08x [%u]\n", 0, egr_cntxt - 1,
	       egr_cntxt);
	printf("  Egress contexts: 0x%08x - 0x%08x [%u]\n", egr_cntxt,
	       cq_cntxt - 1, cq_cntxt - egr_cntxt);
	printf("  CQ contexts:     0x%08x - 0x%08x [%u]\n", cq_cntxt,
	       timers - 1, timers - cq_cntxt);
	printf("  Timers:          0x%08x - 0x%08x [%u]\n", timers,
	       pstructs - 1, pstructs - timers);
	printf("  Pstructs:        0x%08x - 0x%08x [%u]\n", pstructs,
	       pstruct_fl - 1, pstruct_fl - pstructs);
	printf("  Pstruct FL:      0x%08x - 0x%08x [%u]\n", pstruct_fl,
	       rx_fl - 1, rx_fl - pstruct_fl);
	printf("  Rx FL:           0x%08x - 0x%08x [%u]\n", rx_fl, tx_fl - 1,
	       tx_fl - rx_fl);
	printf("  Tx FL:           0x%08x - 0x%08x [%u]\n", tx_fl, cim_base - 1,
	       cim_base - tx_fl);
	printf("  uP RAM:          0x%08x - 0x%08x [%u]\n", cim_base,
	       cim_base + cim_size - 1, cim_size);

	printf("\nPMRX memory map:\n");
	printf("  iSCSI region:    0x%08x - 0x%08x [%u]\n", iscsi_ll, iscsi_ul,
	       iscsi_ul - iscsi_ll + 1);
	printf("  TCP DDP region:  0x%08x - 0x%08x [%u]\n", tddp_ll, tddp_ul,
	       tddp_ul - tddp_ll + 1);
	printf("  TPT region:      0x%08x - 0x%08x [%u]\n", stag_ll, stag_ul,
	       stag_ul - stag_ll + 1);
	printf("  RQ region:       0x%08x - 0x%08x [%u]\n", rq_ll, rq_ul,
	       rq_ul - rq_ll + 1);
	printf("  PBL region:      0x%08x - 0x%08x [%u]\n", pbl_ll, pbl_ul,
	       pbl_ul - pbl_ll + 1);
	return 0;
}

static int meminfo(int argc, char *argv[], int start_arg, const char *iff_name)
{
	int vers;
	struct ifconf_regs regs;

	if ((regs.data = malloc(REGDUMP_SIZE)) == NULL)
		err(1, "can't malloc");
	
	if (doit(iff_name, CHELSIO_IFCONF_GETREGS, &regs))
		err(1, "can't read registers");

	vers = regs.version & 0x3ff;
	if (vers == 3)
		return t3_meminfo((uint32_t *)regs.data);

	errx(1, "unknown card type %d", vers);
	return 0;
}

#ifdef notyet
static int mtu_tab_op(int argc, char *argv[], int start_arg,
		      const char *iff_name)
{
	struct toetool_mtus op;
	int i;

	if (argc == start_arg) {
		op.cmd = TOETOOL_GETMTUTAB;
		op.nmtus = MAX_NMTUS;

		if (doit(iff_name, &op) < 0)
			err(1, "get MTU table");
		for (i = 0; i < op.nmtus; ++i)
			printf("%u ", op.mtus[i]);
		printf("\n");
	} else if (argc <= start_arg + MAX_NMTUS) {
		op.cmd = TOETOOL_SETMTUTAB;
		op.nmtus = argc - start_arg;

		for (i = 0; i < op.nmtus; ++i) {
			char *p;
			unsigned long m = strtoul(argv[start_arg + i], &p, 0);

			if (*p || m > 9600) {
				warnx("bad parameter \"%s\"",
				      argv[start_arg + i]);
				return -1;
			}
			if (i && m < op.mtus[i - 1])
				errx(1, "MTUs must be in ascending order");
			op.mtus[i] = m;
		}
		if (doit(iff_name, &op) < 0)
			err(1, "set MTU table");
	} else
		return -1;

	return 0;
}
#endif

#ifdef CHELSIO_INTERNAL
static void show_egress_cntxt(uint32_t data[])
{
	printf("credits:      %u\n", data[0] & 0x7fff);
	printf("GTS:          %u\n", (data[0] >> 15) & 1);
	printf("index:        %u\n", data[0] >> 16);
	printf("queue size:   %u\n", data[1] & 0xffff);
	printf("base address: 0x%llx\n",
	       ((data[1] >> 16) | ((uint64_t)data[2] << 16) |
	       (((uint64_t)data[3] & 0xf) << 48)) << 12);
	printf("rsp queue #:  %u\n", (data[3] >> 4) & 7);
	printf("cmd queue #:  %u\n", (data[3] >> 7) & 1);
	printf("TUN:          %u\n", (data[3] >> 8) & 1);
	printf("TOE:          %u\n", (data[3] >> 9) & 1);
	printf("generation:   %u\n", (data[3] >> 10) & 1);
	printf("uP token:     %u\n", (data[3] >> 11) & 0xfffff);
	printf("valid:        %u\n", (data[3] >> 31) & 1);
}

static void show_fl_cntxt(uint32_t data[])
{
	printf("base address: 0x%llx\n",
	       ((uint64_t)data[0] | ((uint64_t)data[1] & 0xfffff) << 32) << 12);
	printf("index:        %u\n", (data[1] >> 20) | ((data[2] & 0xf) << 12));
	printf("queue size:   %u\n", (data[2] >> 4) & 0xffff);
	printf("generation:   %u\n", (data[2] >> 20) & 1);
	printf("entry size:   %u\n",
	       ((data[2] >> 21) & 0x7ff) | (data[3] & 0x1fffff));
	printf("congest thr:  %u\n", (data[3] >> 21) & 0x3ff);
	printf("GTS:          %u\n", (data[3] >> 31) & 1);
}

static void show_response_cntxt(uint32_t data[])
{
	printf("index:        %u\n", data[0] & 0xffff);
	printf("size:         %u\n", data[0] >> 16);
	printf("base address: 0x%llx\n",
	       ((uint64_t)data[1] | ((uint64_t)data[2] & 0xfffff) << 32) << 12);
	printf("MSI-X/RspQ:   %u\n", (data[2] >> 20) & 0x3f);
	printf("intr enable:  %u\n", (data[2] >> 26) & 1);
	printf("intr armed:   %u\n", (data[2] >> 27) & 1);
	printf("generation:   %u\n", (data[2] >> 28) & 1);
	printf("CQ mode:      %u\n", (data[2] >> 31) & 1);
	printf("FL threshold: %u\n", data[3]);
}

static void show_cq_cntxt(uint32_t data[])
{
	printf("index:            %u\n", data[0] & 0xffff);
	printf("size:             %u\n", data[0] >> 16);
	printf("base address:     0x%llx\n",
	       ((uint64_t)data[1] | ((uint64_t)data[2] & 0xfffff) << 32) << 12);
	printf("rsp queue #:      %u\n", (data[2] >> 20) & 0x3f);
	printf("AN:               %u\n", (data[2] >> 26) & 1);
	printf("armed:            %u\n", (data[2] >> 27) & 1);
	printf("ANS:              %u\n", (data[2] >> 28) & 1);
	printf("generation:       %u\n", (data[2] >> 29) & 1);
	printf("overflow mode:    %u\n", (data[2] >> 31) & 1);
	printf("credits:          %u\n", data[3] & 0xffff);
	printf("credit threshold: %u\n", data[3] >> 16);
}

static int get_sge_context(int argc, char *argv[], int start_arg,
			   const char *iff_name)
{
	struct ch_cntxt ctx;

	if (argc != start_arg + 2) return -1;

	if (!strcmp(argv[start_arg], "egress"))
		ctx.cntxt_type = CNTXT_TYPE_EGRESS;
	else if (!strcmp(argv[start_arg], "fl"))
		ctx.cntxt_type = CNTXT_TYPE_FL;
	else if (!strcmp(argv[start_arg], "response"))
		ctx.cntxt_type = CNTXT_TYPE_RSP;
	else if (!strcmp(argv[start_arg], "cq"))
		ctx.cntxt_type = CNTXT_TYPE_CQ;
	else {
		warnx("unknown context type \"%s\"; known types are egress, "
		      "fl, cq, and response", argv[start_arg]);
		return -1;
	}

	if (get_int_arg(argv[start_arg + 1], &ctx.cntxt_id))
		return -1;

	if (doit(iff_name, CHELSIO_GET_SGE_CONTEXT, &ctx) < 0)
		err(1, "get SGE context");

	if (!strcmp(argv[start_arg], "egress"))
		show_egress_cntxt(ctx.data);
	else if (!strcmp(argv[start_arg], "fl"))
		show_fl_cntxt(ctx.data);
	else if (!strcmp(argv[start_arg], "response"))
		show_response_cntxt(ctx.data);
	else if (!strcmp(argv[start_arg], "cq"))
		show_cq_cntxt(ctx.data);
	return 0;
}

#if __BYTE_ORDER == __BIG_ENDIAN
# define ntohll(n) (n)
#else
# define ntohll(n) bswap_64(n)
#endif

static int get_sge_desc(int argc, char *argv[], int start_arg,
			const char *iff_name)
{
	uint64_t *p, wr_hdr;
	unsigned int n = 1, qset, qnum;
	struct ch_desc desc;

	if (argc != start_arg + 3 && argc != start_arg + 4)
		return -1;

	if (get_int_arg(argv[start_arg], &qset) ||
	    get_int_arg(argv[start_arg + 1], &qnum) ||
	    get_int_arg(argv[start_arg + 2], &desc.idx))
		return -1;

	if (argc == start_arg + 4 && get_int_arg(argv[start_arg + 3], &n))
		return -1;

	if (qnum > 5)
		errx(1, "invalid queue number %d, range is 0..5", qnum);

	desc.queue_num = qset * 6 + qnum;

	for (; n--; desc.idx++) {
		if (doit(iff_name, CHELSIO_GET_SGE_DESC, &desc) < 0)
			err(1, "get SGE descriptor");

		p = (uint64_t *)desc.data;
		wr_hdr = ntohll(*p);
		printf("Descriptor %u: cmd %u, TID %u, %s%s%s%s%u flits\n",
		       desc.idx, (unsigned int)(wr_hdr >> 56),
		       ((unsigned int)wr_hdr >> 8) & 0xfffff,
		       ((wr_hdr >> 55) & 1) ? "SOP, " : "",
		       ((wr_hdr >> 54) & 1) ? "EOP, " : "",
		       ((wr_hdr >> 53) & 1) ? "COMPL, " : "",
		       ((wr_hdr >> 52) & 1) ? "SGL, " : "",
		       (unsigned int)wr_hdr & 0xff);

		for (; desc.size; p++, desc.size -= sizeof(uint64_t))
			printf("%016" PRIx64 "%c", ntohll(*p),
			    desc.size % 32 == 8 ? '\n' : ' ');
	}
	return 0;
}
#endif

#ifdef notyet
static int get_tcb2(unsigned int tcb_idx, const char *iff_name)
{
	uint64_t *d;
	unsigned int i;
	struct toetool_mem_range *op;

	op = malloc(sizeof(*op) + TCB_SIZE);
	if (!op)
		err(1, "get TCB");

	op->cmd    = TOETOOL_GET_MEM;
	op->mem_id = MEM_CM;
	op->addr   = tcb_idx * TCB_SIZE;
	op->len    = TCB_SIZE;

	if (doit(iff_name, op) < 0)
		err(1, "get TCB");

	for (d = (uint64_t *)op->buf, i = 0; i < TCB_SIZE / 32; i++) {
		printf("%2u:", i);
		printf(" %08x %08x %08x %08x", (uint32_t)d[1],
		       (uint32_t)(d[1] >> 32), (uint32_t)d[0],
		       (uint32_t)(d[0] >> 32));
		d += 2;
		printf(" %08x %08x %08x %08x\n", (uint32_t)d[1],
		       (uint32_t)(d[1] >> 32), (uint32_t)d[0],
		       (uint32_t)(d[0] >> 32));
		d += 2;
	}
	free(op);
	return 0;
}

static int get_tcb(int argc, char *argv[], int start_arg, const char *iff_name)
{
	int i;
	uint32_t *d;
	struct toetool_tcb op;

	if (argc != start_arg + 1) return -1;

	op.cmd = TOETOOL_GET_TCB;
	if (get_int_arg(argv[start_arg], &op.tcb_index))
		return -1;

	/*
	 * If this operation isn't directly supported by the driver we may
	 * still be able to read TCBs using the generic memory dump operation.
	 */
	if (doit(iff_name, &op) < 0) {
		if (errno != EOPNOTSUPP)
			err(1, "get TCB");
		return get_tcb2(op.tcb_index, iff_name);
	}

	for (d = op.tcb_data, i = 0; i < TCB_WORDS; i += 8) {
		int j;

		printf("%2u:", 4 * i);
		for (j = 0; j < 8; ++j)
			printf(" %08x", *d++);
		printf("\n");
	}
	return 0;
}
#endif
#ifdef WRC
/*
 * The following defines, typedefs and structures are defined in the FW and
 * should be exported instead of being redefined here (and kept up in sync).
 * We'll fix this in the next round of FW cleanup.
 */
#define CM_WRCONTEXT_BASE       0x20300000
#define CM_WRCONTEXT_OFFSET	0x300000
#define WRC_SIZE                (FW_WR_SIZE * (2 + FW_WR_NUM) + 32 + 4 * 128)
#define FW_WR_SIZE	128
#define FW_WR_NUM	16
#define FBUF_SIZE	(FW_WR_SIZE * FW_WR_NUM)
#define FBUF_WRAP_SIZE	128
#define FBUF_WRAP_FSZ	(FBUF_WRAP_SZ >> 3)
#define MEM_CM_WRC_SIZE  WRC_SIZE

typedef char 			int8_t;
typedef short 			int16_t;
typedef int 			int32_t;
typedef long long 		_s64;
typedef unsigned char           _u8;
typedef unsigned short          _u16;
typedef unsigned int            _uint32_t;
typedef unsigned long long      uint64_t;

enum fw_ri_mpa_attrs {
	FW_RI_MPA_RX_MARKER_ENABLE = 0x1,
	FW_RI_MPA_TX_MARKER_ENABLE = 0x2,
	FW_RI_MPA_CRC_ENABLE	= 0x4,
	FW_RI_MPA_IETF_ENABLE	= 0x8
} __attribute__ ((packed));

enum fw_ri_qp_caps {
	FW_RI_QP_RDMA_READ_ENABLE = 0x01,
	FW_RI_QP_RDMA_WRITE_ENABLE = 0x02,
	FW_RI_QP_BIND_ENABLE	= 0x04,
	FW_RI_QP_FAST_REGISTER_ENABLE = 0x08,
	FW_RI_QP_STAG0_ENABLE	= 0x10
} __attribute__ ((packed));

enum wrc_state {
	WRC_STATE_CLOSED,
	WRC_STATE_ABORTED,
	WRC_STATE_HALFCLOSED,
	WRC_STATE_TOE_ESTABLISHED,
	WRC_STATE_RDMA_TX_DATA_PEND,
	WRC_STATE_RDMA_PEND,
	WRC_STATE_RDMA_ESTABLISHED,
};

struct _wr {
	uint32_t a;
	uint32_t b;
};

struct fbuf {
	uint32_t 	pp;			/* fifo producer pointer */
	uint32_t	cp;			/* fifo consumer pointer */
	int32_t	num_bytes;		/* num bytes stored in the fbuf */
	char	bufferb[FBUF_SIZE]; 	/* buffer space in bytes */
	char	_wrap[FBUF_WRAP_SIZE];	/* wrap buffer size*/
};
struct wrc {
	uint32_t	wrc_tid;
	_u16	wrc_flags;
	_u8	wrc_state;
	_u8	wrc_credits;

	/* IO */
	_u16	wrc_sge_ec;
	_u8	wrc_sge_respQ;
	_u8	wrc_port;
	_u8	wrc_ulp;

	_u8	wrc_coherency_counter;

	/* REASSEMBLY */
	_u8	wrc_frag_len;
	_u8	wrc_frag_credits;
	uint32_t	wrc_frag;

	union {
		struct {

			/* TOE */
			_u8	aborted;
			_u8	wrc_num_tx_pages;
			_u8	wrc_max_tx_pages;
			_u8	wrc_trace_idx;
			uint32_t 	wrc_snd_nxt;
			uint32_t 	wrc_snd_max;
			uint32_t 	wrc_snd_una;
			uint32_t	wrc_snd_iss;

			/* RI */
			uint32_t	wrc_pdid;
			uint32_t	wrc_scqid;
			uint32_t	wrc_rcqid;
			uint32_t	wrc_rq_addr_32a;
			_u16	wrc_rq_size;
			_u16	wrc_rq_wr_idx;
			enum fw_ri_mpa_attrs wrc_mpaattrs;
			enum fw_ri_qp_caps wrc_qpcaps;
			_u16	wrc_mulpdu_tagged;
			_u16	wrc_mulpdu_untagged;
			_u16	wrc_ord_max;
			_u16	wrc_ird_max;
			_u16	wrc_ord;
			_u16	wrc_ird;
			_u16	wrc_markeroffset;
			uint32_t	wrc_msn_send;
			uint32_t	wrc_msn_rdma_read;
			uint32_t	wrc_msn_rdma_read_req;
			_u16	wrc_rdma_read_req_err;
			_u8	wrc_ack_mode;
			_u8	wrc_sge_ec_credits;
			_u16	wrc_maxiolen_tagged;
			_u16	wrc_maxiolen_untagged;
			uint32_t	wrc_mo;
		} toe_ri;

		struct {

		} ipmi;

		struct {
			uint32_t	wrc_pad2[24];
		} pad;
	} u __attribute__ ((packed));

	/* BUFFERING */
	struct fbuf wrc_fbuf __attribute__ ((packed));
};
#define wrc_aborted u.toe_ri.aborted
#define wrc_num_tx_pages u.toe_ri.wrc_num_tx_pages
#define wrc_max_tx_pages u.toe_ri.wrc_max_tx_pages
#define wrc_trace_idx u.toe_ri.wrc_trace_idx
#define wrc_snd_nxt u.toe_ri.wrc_snd_nxt
#define wrc_snd_max u.toe_ri.wrc_snd_max
#define wrc_snd_una u.toe_ri.wrc_snd_una
#define wrc_snd_iss u.toe_ri.wrc_snd_iss
#define wrc_pdid u.toe_ri.wrc_pdid
#define wrc_scqid u.toe_ri.wrc_scqid
#define wrc_rcqid u.toe_ri.wrc_rcqid
#define wrc_rq_addr_32a u.toe_ri.wrc_rq_addr_32a
#define wrc_rq_size u.toe_ri.wrc_rq_size
#define wrc_rq_wr_idx u.toe_ri.wrc_rq_wr_idx
#define wrc_mpaattrs u.toe_ri.wrc_mpaattrs
#define wrc_qpcaps u.toe_ri.wrc_qpcaps
#define wrc_mulpdu_tagged u.toe_ri.wrc_mulpdu_tagged
#define wrc_mulpdu_untagged u.toe_ri.wrc_mulpdu_untagged
#define wrc_ord_max u.toe_ri.wrc_ord_max
#define wrc_ird_max u.toe_ri.wrc_ird_max
#define wrc_ord u.toe_ri.wrc_ord
#define wrc_ird u.toe_ri.wrc_ird
#define wrc_markeroffset u.toe_ri.wrc_markeroffset
#define wrc_msn_send u.toe_ri.wrc_msn_send
#define wrc_msn_rdma_read u.toe_ri.wrc_msn_rdma_read
#define wrc_msn_rdma_read_req u.toe_ri.wrc_msn_rdma_read_req
#define wrc_rdma_read_req_err u.toe_ri.wrc_rdma_read_req_err
#define wrc_ack_mode u.toe_ri.wrc_ack_mode
#define wrc_sge_ec_credits u.toe_ri.wrc_sge_ec_credits
#define wrc_maxiolen_tagged u.toe_ri.wrc_maxiolen_tagged
#define wrc_maxiolen_untagged u.toe_ri.wrc_maxiolen_untagged
#define wrc_mo u.toe_ri.wrc_mo

static void print_wrc_field(char *field, unsigned int value, unsigned int size)
{
	switch(size) {
	case 1:
		printf("  1 %s: 0x%02x (%u)\n", field, value, value);
		break;
	case 2: {
		unsigned short host_value = ntohs(value);
		printf("  2 %s: 0x%04x (%u)\n", field, host_value, host_value);
		break;
	}
	case 4: {
		unsigned int host_value = ntohl(value);
		printf("  4 %s: 0x%08x (%u)\n", field, host_value, host_value);
		break;
	}
	default:
		printf("  unknown size %u for field %s\n", size, field);
	}
}

#define P(field)  print_wrc_field(#field, p->wrc_ ## field, sizeof (p->wrc_ ## field))

static void print_wrc(unsigned int wrc_idx, struct wrc *p)
{
	u32 *buf = (u32 *)p;
	unsigned int i, j;

	printf("WRC STATE (raw)\n");
	for (i = 0; i < 32;) {
		printf("[%08x]:", 0x20300000 + wrc_idx * MEM_CM_WRC_SIZE + i * 4);
		for (j = 0; j < 8; j++) {
			printf(" %08x ", htonl(buf[i++]));
		}
		printf("\n");
	}
	printf("WRC BASIC\n");
	P(tid); P(flags); P(state); P(credits);
	printf("WRC IO\n");
	P(sge_ec); P(sge_respQ); P(port); P(ulp); P(coherency_counter);
	printf("WRC REASSEMBLY\n");
	P(frag_len); P(frag_credits); P(frag);
	printf("WRC TOE\n");
	P(aborted); P(num_tx_pages); P(max_tx_pages); P(trace_idx); P(snd_nxt);
	P(snd_max); P(snd_una); P(snd_iss);
	printf("WRC RI\n");
	P(pdid); P(scqid); P(rcqid); P(rq_addr_32a); P(rq_size); P(rq_wr_idx);
	P(mpaattrs); P(qpcaps); P(mulpdu_tagged); P(mulpdu_untagged); P(ord_max);
	P(ird_max); P(ord); P(ird); P(markeroffset); P(msn_send); P(msn_rdma_read);
	P(msn_rdma_read_req); P(rdma_read_req_err); P(ack_mode);
	P(sge_ec_credits); P(maxiolen_tagged); P(maxiolen_untagged); P(mo);
	printf("WRC BUFFERING\n");
	printf("  4 fbuf.pp: 0x%08x (%u)\n", htonl(p->wrc_fbuf.pp),  htonl(p->wrc_fbuf.pp));
	printf("  4 fbuf.cp: 0x%08x (%u)\n",  htonl(p->wrc_fbuf.cp),  htonl(p->wrc_fbuf.cp));
	printf("  4 fbuf.num_bytes: 0x%08x (%d)\n",  htonl(p->wrc_fbuf.num_bytes),  htonl(p->wrc_fbuf.num_bytes));
	printf("WRC BUFFER (raw)\n");
	for (i = 32; i < (FBUF_SIZE + FBUF_WRAP_SIZE) / 4;) {
		printf("[%08x]:", 0x20300000 + wrc_idx * MEM_CM_WRC_SIZE + i * 4);
		for (j = 0; j < 4; j++) {
			printf(" %08x%08x", htonl(buf[i++]), htonl(buf[i++]));
		}
		printf("\n");
	}
}

#undef P

#define P(field)  print_sizeof(#field, ##field, sizeof (p->##field))

struct history_e {
	uint32_t wr_addr;
	uint32_t debug;
	uint64_t wr_flit0;
	uint64_t wr_flit1;
	uint64_t wr_flit2;
};

static void print_wrc_zero(unsigned int wrc_idx, struct wrc *p)
{
	uint32_t *buf =
	   (uint32_t *)((unsigned long)p + FW_WR_SIZE * (2 + FW_WR_NUM));
	unsigned int i;

	printf("WRC ZERO\n");
	printf("[%08x]:", CM_WRCONTEXT_BASE + wrc_idx * MEM_CM_WRC_SIZE +
	       FW_WR_SIZE * (2 + FW_WR_NUM));
	for (i = 0; i < 4;)
		printf(" %08x%08x", htonl(buf[i]), htonl(buf[i++]));
	printf("\n");
}

static void print_wrc_history(struct wrc *p)
{
	unsigned int i, idx;
	struct history_e *e =
	    (struct history_e *)((unsigned long)p + FW_WR_SIZE *
				 (2 + FW_WR_NUM) + 32);
	printf("WRC WR HISTORY, idx %u\n", p->wrc_trace_idx);
	idx = p->wrc_trace_idx;
	for (i = 0; i < 16; i++) {
		printf("%02u: %08x %08x %08x%08x %08x%08x %08x%08x\n", idx,
		       htonl(e[idx].wr_addr), htonl(e[idx].debug),
		       htonl(e[idx].wr_flit0 & 0xFFFFFFFF),
		       htonl(e[idx].wr_flit0 >> 32),
		       htonl(e[idx].wr_flit1 & 0xFFFFFFFF),
		       htonl(e[idx].wr_flit1 >> 32),
		       htonl(e[idx].wr_flit2 & 0xFFFFFFFF),
		       htonl(e[idx].wr_flit2 >> 32));
		idx = (idx - 1) & 0xF;
	}
}

static int get_wrc(int argc, char *argv[], int start_arg, const char *iff_name)
{
	struct toetool_mem_range *op;
	uint64_t *p;
	uint32_t *buf;
	unsigned int idx, i = 0;

	if (argc != start_arg + 1)
		return -1;

	if (get_int_arg(argv[start_arg], &idx))
		return -1;

	op = malloc(sizeof(*op) + MEM_CM_WRC_SIZE);
	if (!op)
		err(1, "get_wrc: malloc failed");

	op->cmd    = TOETOOL_GET_MEM;
	op->mem_id = MEM_CM;
	op->addr   = read_reg(iff_name, 0x28c) + CM_WRCONTEXT_OFFSET +
			      idx * MEM_CM_WRC_SIZE;
	op->len    = MEM_CM_WRC_SIZE;
	buf = (uint32_t *)op->buf;

	if (doit(iff_name, op) < 0)
		err(1, "get_wrc");

	/* driver manges with the data... put it back into the the FW's view
	 */
	for (p = (uint64_t *)op->buf;
	         p < (uint64_t *)(op->buf + MEM_CM_WRC_SIZE); p++) {
		uint64_t flit = *p;
		buf[i++] = htonl((uint32_t)(flit >> 32));
		buf[i++] = htonl((uint32_t)flit);
	}

	print_wrc(idx, (struct wrc *)op->buf);
	print_wrc_zero(idx, (struct wrc *)op->buf);
	print_wrc_history((struct wrc *)op->buf);

	free(op);
	return 0;
}
#endif

#ifdef notyet
static int get_pm_page_spec(const char *s, unsigned int *page_size,
			    unsigned int *num_pages)
{
	char *p;
	unsigned long val;

	val = strtoul(s, &p, 0);
	if (p == s) return -1;
	if (*p == 'x' && p[1]) {
		*num_pages = val;
		*page_size = strtoul(p + 1, &p, 0);
	} else {
		*num_pages = -1;
		*page_size = val;
	}
	*page_size <<= 10;     // KB -> bytes
	return *p;
}

static int conf_pm(int argc, char *argv[], int start_arg, const char *iff_name)
{
	struct toetool_pm op;

	if (argc == start_arg) {
	 	op.cmd = TOETOOL_GET_PM;
		if (doit(iff_name, &op) < 0)
			err(1, "read pm config");
		printf("%ux%uKB TX pages, %ux%uKB RX pages, %uKB total memory\n",
		       op.tx_num_pg, op.tx_pg_sz >> 10, op.rx_num_pg,
		       op.rx_pg_sz >> 10, op.pm_total >> 10);
		return 0;
	}

	if (argc != start_arg + 2) return -1;

	if (get_pm_page_spec(argv[start_arg], &op.tx_pg_sz, &op.tx_num_pg)) {
		warnx("bad parameter \"%s\"", argv[start_arg]);
		return -1;
	}
	if (get_pm_page_spec(argv[start_arg + 1], &op.rx_pg_sz,
			     &op.rx_num_pg)) {
		warnx("bad parameter \"%s\"", argv[start_arg + 1]);
		return -1;
	}
	op.cmd = TOETOOL_SET_PM;
	if (doit(iff_name, &op) < 0)
		err(1, "pm config");
	return 0;
}

static int conf_tcam(int argc, char *argv[], int start_arg,
		     const char *iff_name)
{
	struct toetool_tcam op;

	if (argc == start_arg) {
		op.cmd = TOETOOL_GET_TCAM;
		op.nfilters = 0;
		if (doit(iff_name, &op) < 0)
			err(1, "read tcam config");
		printf("%u total entries, %u servers, %u filters, %u routes\n",
		       op.tcam_size, op.nservers, op.nfilters, op.nroutes);
		return 0;
	}

	if (argc != start_arg + 3) return -1;

	if (get_int_arg(argv[start_arg], &op.nservers) ||
	    get_int_arg(argv[start_arg + 1], &op.nroutes) ||
	    get_int_arg(argv[start_arg + 2], &op.nfilters))
		return -1;
	op.cmd = TOETOOL_SET_TCAM;
	if (doit(iff_name, &op) < 0)
		err(1, "tcam config");
	return 0;
}
#endif

#ifdef	CHELSIO_INTERNAL
#ifdef notyet
static int dump_tcam(int argc, char *argv[], int start_arg,
		     const char *iff_name)
{
	unsigned int nwords;
	struct toetool_tcam_word op;

	if (argc != start_arg + 2) return -1;

	if (get_int_arg(argv[start_arg], &op.addr) ||
	    get_int_arg(argv[start_arg + 1], &nwords))
		return -1;
	op.cmd = TOETOOL_READ_TCAM_WORD;

	while (nwords--) {
		if (doit(iff_name, &op) < 0)
			err(1, "tcam dump");

		printf("0x%08x: 0x%02x 0x%08x 0x%08x\n", op.addr,
		       op.buf[0] & 0xff, op.buf[1], op.buf[2]);
		op.addr++;
	}
	return 0;
}
#endif
static void hexdump_8b(unsigned int start, uint64_t *data, unsigned int len)
{
	int i;

	while (len) {
		printf("0x%08x:", start);
		for (i = 0; i < 4 && len; ++i, --len)
			printf(" %016llx", (unsigned long long)*data++);
		printf("\n");
		start += 32;
	}
}

static int dump_mc7(int argc, char *argv[], int start_arg,
		    const char *iff_name)
{
	struct ch_mem_range mem;
	unsigned int mem_id, addr, len;

	if (argc != start_arg + 3) return -1;

	if (!strcmp(argv[start_arg], "cm"))
		mem_id = MEM_CM;
	else if (!strcmp(argv[start_arg], "rx"))
		mem_id = MEM_PMRX;
	else if (!strcmp(argv[start_arg], "tx"))
		mem_id = MEM_PMTX;
	else
		errx(1, "unknown memory \"%s\"; must be one of \"cm\", \"tx\","
			" or \"rx\"", argv[start_arg]);

	if (get_int_arg(argv[start_arg + 1], &addr) ||
	    get_int_arg(argv[start_arg + 2], &len))
		return -1;

	mem.buf = malloc(len);
	if (!mem.buf)
		err(1, "memory dump");

	mem.mem_id = mem_id;
	mem.addr   = addr;
	mem.len    = len;

	if (doit(iff_name, CHELSIO_GET_MEM, &mem) < 0)
		err(1, "memory dump");

	hexdump_8b(mem.addr, (uint64_t *)mem.buf, mem.len >> 3);
	free(mem.buf);
	return 0;
}
#endif

#ifdef notyet
/* Max FW size is 32K including version, +4 bytes for the checksum. */
#define MAX_FW_IMAGE_SIZE (32768 + 4)

static int load_fw(int argc, char *argv[], int start_arg, const char *iff_name)
{
	int fd, len;
	struct toetool_mem_range *op;
	const char *fname = argv[start_arg];

	if (argc != start_arg + 1) return -1;

	fd = open(fname, O_RDONLY);
	if (fd < 0)
		err(1, "load firmware");

	op = malloc(sizeof(*op) + MAX_FW_IMAGE_SIZE + 1);
	if (!op)
		err(1, "load firmware");

	len = read(fd, op->buf, MAX_FW_IMAGE_SIZE + 1);
	if (len < 0)
		err(1, "load firmware");
 	if (len > MAX_FW_IMAGE_SIZE)
		errx(1, "FW image too large");

	op->cmd = TOETOOL_LOAD_FW;
	op->len = len;

	if (doit(iff_name, op) < 0)
		err(1, "load firmware");
	return 0;
}


static int write_proto_sram(const char *fname, const char *iff_name)
{
	int i;
	char c;
	struct toetool_proto op = { .cmd = TOETOOL_SET_PROTO };
	uint32_t *p = op.data;
	FILE *fp = fopen(fname, "r");

	if (!fp)
		err(1, "load protocol sram");

	for (i = 0; i < 128; i++, p += 5) {
		int n = fscanf(fp, "%1x%8x%8x%8x%8x",
			       &p[0], &p[1], &p[2], &p[3], &p[4]);
		if (n != 5)
			errx(1, "%s: bad line %d", fname, i);
	}
	if (fscanf(fp, "%1s", &c) != EOF)
		errx(1, "%s: protocol sram image has too many lines", fname);
	fclose(fp);

	if (doit(iff_name, &op) < 0)
		err(1, "load protocol sram");
	return 0;
}

static int dump_proto_sram(const char *iff_name)
{
	int i, j;
	u8 buf[sizeof(struct ethtool_eeprom) + PROTO_SRAM_SIZE];
	struct ethtool_eeprom *ee = (struct ethtool_eeprom *)buf;
	u8 *p = buf + sizeof(struct ethtool_eeprom);

	ee->cmd = ETHTOOL_GEEPROM;
	ee->len = PROTO_SRAM_SIZE;
	ee->offset = PROTO_SRAM_EEPROM_ADDR;
	if (ethtool_call(iff_name, ee))
		err(1, "show protocol sram");

	for (i = 0; i < PROTO_SRAM_LINES; i++) {
		for (j = PROTO_SRAM_LINE_NIBBLES - 1; j >= 0; j--) {
			int nibble_idx = i * PROTO_SRAM_LINE_NIBBLES + j;
			u8 nibble = p[nibble_idx / 2];

			if (nibble_idx & 1)
				nibble >>= 4;
			else
				nibble &= 0xf;
			printf("%x", nibble);
		}
		putchar('\n');
	}
	return 0;
}

static int proto_sram_op(int argc, char *argv[], int start_arg,
			 const char *iff_name)
{
	if (argc == start_arg + 1)
		return write_proto_sram(argv[start_arg], iff_name);
	if (argc == start_arg)
		return dump_proto_sram(iff_name);
	return -1;
}
#endif

static int dump_qset_params(const char *iff_name)
{
	struct ch_qset_params qp;

	qp.qset_idx = 0;

	while (doit(iff_name, CHELSIO_GET_QSET_PARAMS, &qp) == 0) {
		if (!qp.qset_idx)
			printf("Qnum   TxQ0   TxQ1   TxQ2   RspQ   RxQ0   RxQ1"
			       "  Cong  Intr Lat   Rx Mode\n");
		printf("%4u %6u %6u %6u %6u %6u %6u %5u %9u   %s    \n",
		       qp.qset_idx,
		       qp.txq_size[0], qp.txq_size[1], qp.txq_size[2],
		       qp.rspq_size, qp.fl_size[0], qp.fl_size[1],
		       qp.cong_thres, qp.intr_lat,
		       qp.polling ? "Polling" : "Interrupt");
		qp.qset_idx++;
	}
	if (!qp.qset_idx || (errno && errno != EINVAL))
		err(1, "get qset parameters");
	return 0;
}

static int qset_config(int argc, char *argv[], int start_arg,
		       const char *iff_name)
{
	struct ch_qset_params qp;

	if (argc == start_arg)
		return dump_qset_params(iff_name);

	if (get_int_arg(argv[start_arg++], &qp.qset_idx))
		return -1;

	qp.txq_size[0] = qp.txq_size[1] = qp.txq_size[2] = -1;
	qp.fl_size[0] = qp.fl_size[1] = qp.rspq_size = -1;
	qp.polling = qp.intr_lat = qp.cong_thres = -1;

	while (start_arg + 2 <= argc) {
		int32_t *param = NULL;

		if (!strcmp(argv[start_arg], "txq0"))
			param = &qp.txq_size[0];
		else if (!strcmp(argv[start_arg], "txq1"))
			param = &qp.txq_size[1];
		else if (!strcmp(argv[start_arg], "txq2"))
			param = &qp.txq_size[2];
		else if (!strcmp(argv[start_arg], "rspq"))
			param = &qp.rspq_size;
		else if (!strcmp(argv[start_arg], "fl0"))
			param = &qp.fl_size[0];
		else if (!strcmp(argv[start_arg], "fl1"))
			param = &qp.fl_size[1];
		else if (!strcmp(argv[start_arg], "lat"))
			param = &qp.intr_lat;
		else if (!strcmp(argv[start_arg], "cong"))
			param = &qp.cong_thres;
		else if (!strcmp(argv[start_arg], "mode"))
			param = &qp.polling;
		else
			errx(1, "unknown qset parameter \"%s\"\n"
			     "allowed parameters are \"txq0\", \"txq1\", "
			     "\"txq2\", \"rspq\", \"fl0\", \"fl1\", \"lat\", "
			     "\"cong\", \"mode\' and \"lro\"", argv[start_arg]);

		start_arg++;

		if (param == &qp.polling) {
			if (!strcmp(argv[start_arg], "irq"))
				qp.polling = 0;
			else if (!strcmp(argv[start_arg], "polling"))
				qp.polling = 1;
			else
				errx(1, "illegal qset mode \"%s\"\n"
				     "known modes are \"irq\" and \"polling\"",
				     argv[start_arg]);
		} else if (get_int_arg(argv[start_arg], (uint32_t *)param))
			return -1;
		start_arg++;
	}
	if (start_arg != argc)
		errx(1, "unknown parameter %s", argv[start_arg]);

#if 0
	printf("%4u %6d %6d %6d %6d %6d %6d %5d %9d   %d\n", op.qset_idx,
	       op.txq_size[0], op.txq_size[1], op.txq_size[2],
	       op.rspq_size, op.fl_size[0], op.fl_size[1], op.cong_thres,
	       op.intr_lat, op.polling);
#endif
	if (doit(iff_name, CHELSIO_SET_QSET_PARAMS, &qp) < 0)
		err(1, "set qset parameters");

	return 0;
}

static int qset_num_config(int argc, char *argv[], int start_arg,
			   const char *iff_name)
{
	struct ch_reg reg;

	if (argc == start_arg) {
		if (doit(iff_name, CHELSIO_GET_QSET_NUM, &reg) < 0)
			err(1, "get qsets");
		printf("%u\n", reg.val);
		return 0;
	}

	if (argc != start_arg + 1)
		return -1;
	if (get_int_arg(argv[start_arg], &reg.val))
		return -1;

	if (doit(iff_name, CHELSIO_SET_QSET_NUM, &reg) < 0)
		err(1, "set qsets");
	return 0;
}

/*
 * Parse a string containing an IP address with an optional network prefix.
 */
static int parse_ipaddr(const char *s, uint32_t *addr, uint32_t *mask)
{
	char *p, *slash;
	struct in_addr ia;

	*mask = 0xffffffffU;
	slash = strchr(s, '/');
	if (slash)
		*slash = 0;
	if (!inet_aton(s, &ia)) {
		if (slash)
			*slash = '/';
		*addr = 0;
		return -1;
	}
	*addr = ntohl(ia.s_addr);
	if (slash) {
		unsigned int prefix = strtoul(slash + 1, &p, 10);

		*slash = '/';
		if (p == slash + 1 || *p || prefix > 32)
			return -1;
		*mask <<= (32 - prefix);
	}
	return 0;
}

/*
 * Parse a string containing a value and an optional colon separated mask.
 */
static int parse_val_mask_param(const char *s, uint32_t *val, uint32_t *mask)
{
	char *p;

	*mask = 0xffffffffU;
	*val = strtoul(s, &p, 0);
	if (p == s)
		return -1;
	if (*p == ':' && p[1])
		*mask = strtoul(p + 1, &p, 0);
	return *p ? -1 : 0;
}

static int parse_trace_param(const char *s, uint32_t *val, uint32_t *mask)
{
	return strchr(s, '.') ? parse_ipaddr(s, val, mask) :
				parse_val_mask_param(s, val, mask);
}

static int trace_config(int argc, char *argv[], int start_arg,
			const char *iff_name)
{
	uint32_t val, mask;
	struct ch_trace trace;

	if (argc == start_arg)
		return -1;

	memset(&trace, 0, sizeof(trace));
	if (!strcmp(argv[start_arg], "tx"))
		trace.config_tx = 1;
	else if (!strcmp(argv[start_arg], "rx"))
		trace.config_rx = 1;
	else if (!strcmp(argv[start_arg], "all"))
		trace.config_tx = trace.config_rx = 1;
	else
		errx(1, "bad trace filter \"%s\"; must be one of \"rx\", "
		     "\"tx\" or \"all\"", argv[start_arg]);

	if (argc == ++start_arg)
		return -1;
	if (!strcmp(argv[start_arg], "on")) {
		trace.trace_tx = trace.config_tx;
		trace.trace_rx = trace.config_rx;
	} else if (strcmp(argv[start_arg], "off"))
		errx(1, "bad argument \"%s\"; must be \"on\" or \"off\"",
		     argv[start_arg]);

	start_arg++;
	if (start_arg < argc && !strcmp(argv[start_arg], "not")) {
		trace.invert_match = 1;
		start_arg++;
	}

	while (start_arg + 2 <= argc) {
		int ret = parse_trace_param(argv[start_arg + 1], &val, &mask);

		if (!strcmp(argv[start_arg], "interface")) {
			trace.intf = val;
			trace.intf_mask = mask;
		} else if (!strcmp(argv[start_arg], "sip")) {
			trace.sip = val;
			trace.sip_mask = mask;
		} else if (!strcmp(argv[start_arg], "dip")) {
			trace.dip = val;
			trace.dip_mask = mask;
		} else if (!strcmp(argv[start_arg], "sport")) {
			trace.sport = val;
			trace.sport_mask = mask;
		} else if (!strcmp(argv[start_arg], "dport")) {
			trace.dport = val;
			trace.dport_mask = mask;
		} else if (!strcmp(argv[start_arg], "vlan")) {
			trace.vlan = val;
			trace.vlan_mask = mask;
		} else if (!strcmp(argv[start_arg], "proto")) {
			trace.proto = val;
			trace.proto_mask = mask;
		} else
			errx(1, "unknown trace parameter \"%s\"\n"
			     "known parameters are \"interface\", \"sip\", "
			     "\"dip\", \"sport\", \"dport\", \"vlan\", "
			     "\"proto\"", argv[start_arg]);
		if (ret < 0)
			errx(1, "bad parameter \"%s\"", argv[start_arg + 1]);
		start_arg += 2;
	}
	if (start_arg != argc)
		errx(1, "unknown parameter \"%s\"", argv[start_arg]);

#if 0
	printf("sip: %x:%x, dip: %x:%x, sport: %x:%x, dport: %x:%x, "
	       "interface: %x:%x, vlan: %x:%x, tx_config: %u, rx_config: %u, "
	       "invert: %u, tx_enable: %u, rx_enable: %u\n", op.sip,
	       op.sip_mask, op.dip, op.dip_mask, op.sport, op.sport_mask,
	       op.dport, op.dport_mask, op.intf, op.intf_mask, op.vlan,
	       op.vlan_mask, op.config_tx, op.config_rx, op.invert_match,
	       op.trace_tx, op.trace_rx);
#endif
	if (doit(iff_name, CHELSIO_SET_TRACE_FILTER, &trace) < 0)
		err(1, "trace");
	return 0;
}

#ifdef notyet
static int t1_powersave(int argc, char *argv[], int start_arg,
		     const char *iff_name)
{
	struct toetool_t1powersave op = {
		.cmd  = TOETOOL_T1POWERSAVE,
		.mode = 0
	};

	if (argc == start_arg)
		op.mode = 2; /* Check powersave mode */

	else if (argc == start_arg + 1) {
		if (strcmp(argv[start_arg], "on") == 0)
			op.mode = 1;
		else if (strcmp(argv[start_arg], "off") == 0)
			op.mode = 0;
		else {
			warnx("bad parameter \"%s\"", argv[start_arg]);
			return -1;
		}
	} else {
		errx(1, "too many arguments");
		return -1;
	}

	if (doit(iff_name, &op) < 0)
		err(1, "t1powersave");

	if (op.mode & 2)
		printf("t1powersave is %s\n", (op.mode & 1) ? "on" : "off");

	return 0;
}
#endif

static int pktsched(int argc, char *argv[], int start_arg, const char *iff_name)
{
	struct ch_pktsched_params pktsched;
	unsigned int idx, min = -1, max, binding = -1;

	if (!strcmp(argv[start_arg], "port")) {
		if (argc != start_arg + 4)
			return -1;
		if (get_int_arg(argv[start_arg + 1], &idx) ||
		    get_int_arg(argv[start_arg + 2], &min) ||
		    get_int_arg(argv[start_arg + 3], &max))
			return -1;
		pktsched.sched = 0;
	} else if (!strcmp(argv[start_arg], "tunnelq")) {
		if (argc != start_arg + 4)
			return -1;
		if (get_int_arg(argv[start_arg + 1], &idx) ||
		    get_int_arg(argv[start_arg + 2], &max) ||
		    get_int_arg(argv[start_arg + 3], &binding))
			return -1;
		pktsched.sched = 1;
	} else
		errx(1, "unknown scheduler \"%s\"; must be one of \"port\""
			" or \"tunnelq\"", argv[start_arg]);

	pktsched.idx = idx;
	pktsched.min = min;
	pktsched.max = max;
	pktsched.binding = binding;
	if (doit(iff_name, CHELSIO_SET_PKTSCHED, &pktsched) < 0)
		 err(1, "pktsched");

	return 0;
}

int main(int argc, char *argv[])
{
	int r = -1;
	const char *iff_name;

	progname = argv[0];

	if (argc == 2) {
		if (!strcmp(argv[1], "-h") || !strcmp(argv[1], "--help"))
			usage(stdout);
		if (!strcmp(argv[1], "-v") || !strcmp(argv[1], "--version")) {
			printf("%s version %s\n", PROGNAME, VERSION);
			printf("%s\n", COPYRIGHT);
			exit(0);
		}
	}

	if (argc < 3) usage(stderr);

	iff_name = argv[1];
	if (!strcmp(argv[2], "reg"))
		r = register_io(argc, argv, 3, iff_name);
	else if (!strcmp(argv[2], "mdio"))
		r = mdio_io(argc, argv, 3, iff_name);
#ifdef notyet	
	else if (!strcmp(argv[2], "tpi"))
		r = tpi_io(argc, argv, 3, iff_name);
	else if (!strcmp(argv[2], "up"))
		r = device_up(argc, argv, 3, iff_name);
	else if (!strcmp(argv[2], "mtus"))
		r = mtu_tab_op(argc, argv, 3, iff_name);
	else if (!strcmp(argv[2], "pm"))
		r = conf_pm(argc, argv, 3, iff_name);
	else if (!strcmp(argv[2], "tcam"))
		r = conf_tcam(argc, argv, 3, iff_name);
	else if (!strcmp(argv[2], "tcb"))
		r = get_tcb(argc, argv, 3, iff_name);
#ifdef WRC
	else if (!strcmp(argv[2], "wrc"))
		r = get_wrc(argc, argv, 3, iff_name);
#endif
#endif	
	else if (!strcmp(argv[2], "regdump"))
		r = dump_regs(argc, argv, 3, iff_name);
#ifdef CHELSIO_INTERNAL
	else if (!strcmp(argv[2], "memdump"))
		r = dump_mc7(argc, argv, 3, iff_name);
	else if (!strcmp(argv[2], "meminfo"))
		r = meminfo(argc, argv, 3, iff_name);
	else if (!strcmp(argv[2], "context"))
		r = get_sge_context(argc, argv, 3, iff_name);
	else if (!strcmp(argv[2], "desc"))
		r = get_sge_desc(argc, argv, 3, iff_name);
#endif
	else if (!strcmp(argv[2], "qset"))
		r = qset_config(argc, argv, 3, iff_name);
	else if (!strcmp(argv[2], "qsets"))
		r = qset_num_config(argc, argv, 3, iff_name);
	else if (!strcmp(argv[2], "trace"))
		r = trace_config(argc, argv, 3, iff_name);
#ifdef notyet
	else if (!strcmp(argv[2], "tcamdump"))
		r = dump_tcam(argc, argv, 3, iff_name);
	else if (!strcmp(argv[2], "loadfw"))
		r = load_fw(argc, argv, 3, iff_name);
	else if (!strcmp(argv[2], "proto"))
		r = proto_sram_op(argc, argv, 3, iff_name);
	else if (!strcmp(argv[2], "t1powersave"))		
		r = t1_powersave(argc, argv, 3, iff_name);
#endif	
	else if (!strcmp(argv[2], "pktsched"))
		r = pktsched(argc, argv, 3, iff_name);
	if (r == -1)
		usage(stderr);
	return 0;
}
