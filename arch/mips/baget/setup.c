/*
 * setup.c: Baget/MIPS specific setup, including init of the feature struct.
 *
 * Copyright (C) 1998 Gleb Raiko & Vladimir Roganov
 */
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <asm/irq.h>
#include <asm/addrspace.h>
#include <asm/reboot.h>

#include <asm/baget/baget.h>

long int vac_memory_upper;

#define CACHEABLE_STR(val) ((val) ? "not cached" : "cached")
#define MIN(a,b)           (((a)<(b)) ? (a):(b))

static void __init vac_show(void)
{
	int i;
	unsigned short val, decode = vac_inw(VAC_DECODE_CTRL);
	unsigned short a24_base = vac_inw(VAC_A24_BASE);
	unsigned long  a24_addr = ((unsigned long)
					   (a24_base & VAC_A24_MASK)) << 16;
	char *decode_mode[]  = { "eprom", "vsb", "shared", "dram" };
	char *address_mode[] = { "", ", A16", ", A32/A24", ", A32/A24/A16" };
	char *state[] = { "", " on write", " on read", " on read/write", };
	char *region_mode[] = { "inactive", "shared", "vsb", "vme" };
	char *asiz[]        = { "user", "A32", "A16", "A24" };
	unsigned short regs[] = { VAC_REG1,     VAC_REG2, VAC_REG3  };
	unsigned short bndr[] = { VAC_DRAM_MASK,VAC_BNDR2,VAC_BNDR3 };
	unsigned short io_sels[] = { VAC_IOSEL0_CTRL,
				     VAC_IOSEL1_CTRL,
				     VAC_IOSEL2_CTRL,
				     VAC_IOSEL3_CTRL,
				     VAC_IOSEL4_CTRL,
				     VAC_IOSEL5_CTRL };

	printk("[DSACKi %s, DRAMCS%s qualified, boundary%s qualified%s]\n",
	       (decode & VAC_DECODE_DSACKI)     ? "on" : "off",
	       (decode & VAC_DECODE_QFY_DRAMCS) ? ""   : " not",
	       (decode & VAC_DECODE_QFY_BNDR)   ? ""   : " not",
	       (decode & VAC_DECODE_FPUCS)      ? ", fpu" : "");

	printk("slave0 ");
	if (decode & VAC_DECODE_RDR_SLSEL0)
		printk("at %08lx (%d MB)\t[dram %s]\n",
		       ((unsigned long)vac_inw(VAC_SLSEL0_BASE))<<16,
		       ((0xffff ^ vac_inw(VAC_SLSEL0_MASK)) + 1) >> 4,
		       (decode & VAC_DECODE_QFY_SLSEL0) ? "qualified" : "");
	else
		printk("off\n");

	printk("slave1 ");
	if (decode & VAC_DECODE_RDR_SLSEL1)
		printk("at %08lx (%d MB)\t[%s%s, %s]\n",
		       ((unsigned long)vac_inw(VAC_SLSEL1_BASE))<<16,
		       ((0xffff ^ vac_inw(VAC_SLSEL1_MASK)) + 1) >> 4,
		       decode_mode[VAC_DECODE_MODE_VAL(decode)],
		       address_mode[VAC_DECODE_CMP_SLSEL1_VAL(decode)],
		       (decode & VAC_DECODE_QFY_SLSEL1) ? "qualified" : "");
	else
		printk("off\n");

	printk("icf global at %04x, module at %04x [%s]\n",
		       ((unsigned int)
			VAC_ICFSEL_GLOBAL_VAL(vac_inw(VAC_ICFSEL_BASE)))<<4,
		       ((unsigned int)
			VAC_ICFSEL_MODULE_VAL(vac_inw(VAC_ICFSEL_BASE)))<<4,
		       (decode & VAC_DECODE_QFY_ICFSEL) ? "qualified" : "");


	printk("region0 at 00000000 (%dMB)\t[dram, %s, delay %d cpuclk"
	       ", cached]\n",
	       (vac_inw(VAC_DRAM_MASK)+1)>>4,
	       (decode & VAC_DECODE_DSACK) ? "D32" : "3state",
	       VAC_DECODE_CPUCLK_VAL(decode));

	for (i = 0; i < sizeof(regs)/sizeof(regs[0]); i++) {
		unsigned long from =
			((unsigned long)vac_inw(bndr[i]))<<16;
		unsigned long to   =
			((unsigned long)
			 ((i+1 == sizeof(bndr)/sizeof(bndr[0])) ?
			  0xff00 : vac_inw(bndr[i+1])))<<16;


		val = vac_inw(regs[i]);
		printk("region%d at %08lx (%dMB)\t[%s %s/%s, %s]\n",
		       i+1,
		       from,
		       (unsigned int)((to - from) >> 20),
		       region_mode[VAC_REG_MODE(val)],
		       asiz[VAC_REG_ASIZ_VAL(val)],
		       ((val & VAC_REG_WORD) ?  "D16" : "D32"),
		       CACHEABLE_STR(val&VAC_A24_A24_CACHINH));

		if (a24_addr >= from && a24_addr < to)
			printk("\ta24 at %08lx (%dMB)\t[vme, A24/%s, %s]\n",
			       a24_addr,
			       MIN((unsigned int)(a24_addr - from)>>20, 32),
			       (a24_base & VAC_A24_DATAPATH) ?  "user" :
			       ((a24_base & VAC_A24_D32_ENABLE)  ?
				"D32" : "D16"),
			       CACHEABLE_STR(a24_base & VAC_A24_A24_CACHINH));
	}

	printk("region4 at ff000000 (15MB)\t[eprom]\n");
	val = vac_inw(VAC_EPROMCS_CTRL);
	printk("\t[ack %d cpuclk%s, %s%srecovery %d cpuclk, "
	       "read %d%s, write %d%s, assert %d%s]\n",
	       VAC_CTRL_DELAY_DSACKI_VAL(val),
	       state[val & (VAC_CTRL_IORD|VAC_CTRL_IOWR)],
	       (val & VAC_CTRL_DSACK0) ? "dsack0*, " : "",
	       (val & VAC_CTRL_DSACK1) ? "dsack1*, " : "",
	       VAC_CTRL_RECOVERY_IOSELI_VAL(val),
	       VAC_CTRL_DELAY_IORD_VAL(val)/2,
	       (VAC_CTRL_DELAY_IORD_VAL(val)&1) ? ".5" : "",
	       VAC_CTRL_DELAY_IOWR_VAL(val)/2,
	       (VAC_CTRL_DELAY_IOWR_VAL(val)&1) ? ".5" : "",
	       VAC_CTRL_DELAY_IOSELI_VAL(val)/2,
	       (VAC_CTRL_DELAY_IOSELI_VAL(val)&1) ? ".5" : "");

	printk("region5 at fff00000 (896KB)\t[local io, %s]\n",
	       CACHEABLE_STR(vac_inw(VAC_A24_BASE) & VAC_A24_IO_CACHINH));

	for (i = 0; i < sizeof(io_sels)/sizeof(io_sels[0]); i++) {
		val = vac_inw(io_sels[i]);
		printk("\tio%d[ack %d cpuclk%s, %s%srecovery %d cpuclk, "
		       "\n\t read %d%s cpuclk, write %d%s cpuclk, "
		       "assert %d%s%s cpuclk]\n",
		       i,
		       VAC_CTRL_DELAY_DSACKI_VAL(val),
		       state[val & (VAC_CTRL_IORD|VAC_CTRL_IOWR)],
		       (val & VAC_CTRL_DSACK0) ? "dsack0*, " : "",
		       (val & VAC_CTRL_DSACK1) ? "dsack1*, " : "",
		       VAC_CTRL_RECOVERY_IOSELI_VAL(val),
		       VAC_CTRL_DELAY_IORD_VAL(val)/2,
		       (VAC_CTRL_DELAY_IORD_VAL(val)&1) ? ".5" : "",
		       VAC_CTRL_DELAY_IOWR_VAL(val)/2,
		       (VAC_CTRL_DELAY_IOWR_VAL(val)&1) ? ".5" : "",
		       VAC_CTRL_DELAY_IOSELI_VAL(val)/2,
		       (VAC_CTRL_DELAY_IOSELI_VAL(val)&1) ? ".5" : "",
		       (vac_inw(VAC_DEV_LOC) & VAC_DEV_LOC_IOSEL(i)) ?
		          ", id" : "");
	}

	printk("region6 at fffe0000 (128KB)\t[vme, A16/%s, "
	       "not cached]\n",
	       (a24_base & VAC_A24_A16D32_ENABLE) ?
	       ((a24_base & VAC_A24_A16D32) ? "D32" : "D16") : "user");

	val = vac_inw(VAC_SHRCS_CTRL);
	printk("shared[ack %d cpuclk%s, %s%srecovery %d cpuclk, "
	       "read %d%s, write %d%s, assert %d%s]\n",
	       VAC_CTRL_DELAY_DSACKI_VAL(val),
	       state[val & (VAC_CTRL_IORD|VAC_CTRL_IOWR)],
	       (val & VAC_CTRL_DSACK0) ? "dsack0*, " : "",
	       (val & VAC_CTRL_DSACK1) ? "dsack1*, " : "",
	       VAC_CTRL_RECOVERY_IOSELI_VAL(val),
	       VAC_CTRL_DELAY_IORD_VAL(val)/2,
	       (VAC_CTRL_DELAY_IORD_VAL(val)&1) ? ".5" : "",
	       VAC_CTRL_DELAY_IOWR_VAL(val)/2,
	       (VAC_CTRL_DELAY_IOWR_VAL(val)&1) ? ".5" : "",
	       VAC_CTRL_DELAY_IOSELI_VAL(val)/2,
	       (VAC_CTRL_DELAY_IOSELI_VAL(val)&1) ? ".5" : "");
}

static void __init vac_init(void)
{
	unsigned short mem_limit = (vac_memory_upper >> 16);

	switch(vac_inw(VAC_ID)) {
	case 0x1AC0:
		printk("VAC068-F5: ");
		break;
	case 0x1AC1:
		printk("VAC068A: ");
		break;
	default:
		panic("Unknown VAC revision number");
	}

	vac_outw(mem_limit-1, VAC_DRAM_MASK);
	vac_outw(mem_limit, VAC_BNDR2);
	vac_outw(mem_limit, VAC_BNDR3);
	vac_outw(((BAGET_A24M_BASE>>16)&~VAC_A24_D32_ENABLE)|VAC_A24_DATAPATH,
		 VAC_A24_BASE);
	vac_outw(VAC_REG_INACTIVE|VAC_REG_ASIZ0,VAC_REG1);
	vac_outw(VAC_REG_INACTIVE|VAC_REG_ASIZ0,VAC_REG2);
	vac_outw(VAC_REG_MWB|VAC_REG_ASIZ1,VAC_REG3);
	vac_outw(BAGET_A24S_BASE>>16,VAC_SLSEL0_BASE);
	vac_outw(BAGET_A24S_MASK>>16,VAC_SLSEL0_MASK);
	vac_outw(BAGET_A24S_BASE>>16,VAC_SLSEL1_BASE);
	vac_outw(BAGET_A24S_MASK>>16,VAC_SLSEL1_MASK);
	vac_outw(BAGET_GSW_BASE|BAGET_MSW_BASE(0),VAC_ICFSEL_BASE);
	vac_outw(VAC_DECODE_FPUCS|
		 VAC_DECODE_CPUCLK(3)|
		 VAC_DECODE_RDR_SLSEL0|VAC_DECODE_RDR_SLSEL1|
		 VAC_DECODE_DSACK|
		 VAC_DECODE_QFY_BNDR|
		 VAC_DECODE_QFY_ICFSEL|
		 VAC_DECODE_QFY_SLSEL1|VAC_DECODE_QFY_SLSEL0|
		 VAC_DECODE_CMP_SLSEL1_HI|
		 VAC_DECODE_DRAMCS|
		 VAC_DECODE_QFY_DRAMCS|
		 VAC_DECODE_DSACKI,VAC_DECODE_CTRL);
	vac_outw(VAC_PIO_FUNC_UART_A_TX|VAC_PIO_FUNC_UART_A_RX|
		 VAC_PIO_FUNC_UART_B_TX|VAC_PIO_FUNC_UART_B_RX|
		 VAC_PIO_FUNC_IOWR|
		 VAC_PIO_FUNC_IOSEL3|
		 VAC_PIO_FUNC_IRQ7|VAC_PIO_FUNC_IRQ10|VAC_PIO_FUNC_IRQ11|
		 VAC_PIO_FUNC_IOSEL2|
		 VAC_PIO_FUNC_FCIACK,VAC_PIO_FUNC);
	vac_outw(VAC_PIO_DIR_FCIACK |
		 VAC_PIO_DIR_OUT(0) |
		 VAC_PIO_DIR_OUT(1) |
		 VAC_PIO_DIR_OUT(2) |
		 VAC_PIO_DIR_OUT(3) |
		 VAC_PIO_DIR_IN(4)  |
		 VAC_PIO_DIR_OUT(5) |
		 VAC_PIO_DIR_OUT(6) |
		 VAC_PIO_DIR_OUT(7) |
		 VAC_PIO_DIR_OUT(8) |
		 VAC_PIO_DIR_IN(9)  |
		 VAC_PIO_DIR_OUT(10)|
		 VAC_PIO_DIR_OUT(11)|
		 VAC_PIO_DIR_OUT(12)|
		 VAC_PIO_DIR_OUT(13),VAC_PIO_DIRECTION);
	vac_outw(VAC_DEV_LOC_IOSEL(2),VAC_DEV_LOC);
	vac_outw(VAC_CTRL_IOWR|
		 VAC_CTRL_DELAY_IOWR(3)|
		 VAC_CTRL_DELAY_IORD(3)|
		 VAC_CTRL_RECOVERY_IOSELI(1)|
		 VAC_CTRL_DELAY_DSACKI(8),VAC_SHRCS_CTRL);
	vac_outw(VAC_CTRL_IOWR|
		 VAC_CTRL_DELAY_IOWR(3)|
		 VAC_CTRL_DELAY_IORD(3)|
		 VAC_CTRL_RECOVERY_IOSELI(1)|
		 VAC_CTRL_DSACK0|VAC_CTRL_DSACK1|
		 VAC_CTRL_DELAY_DSACKI(8),VAC_EPROMCS_CTRL);
	vac_outw(VAC_CTRL_IOWR|
		 VAC_CTRL_DELAY_IOWR(3)|
		 VAC_CTRL_DELAY_IORD(3)|
		 VAC_CTRL_RECOVERY_IOSELI(2)|
		 VAC_CTRL_DSACK0|VAC_CTRL_DSACK1|
		 VAC_CTRL_DELAY_DSACKI(8),VAC_IOSEL0_CTRL);
	vac_outw(VAC_CTRL_IOWR|
		 VAC_CTRL_DELAY_IOWR(3)|
		 VAC_CTRL_DELAY_IORD(3)|
		 VAC_CTRL_RECOVERY_IOSELI(2)|
		 VAC_CTRL_DSACK0|VAC_CTRL_DSACK1|
		 VAC_CTRL_DELAY_DSACKI(8),VAC_IOSEL1_CTRL);
	vac_outw(VAC_CTRL_IOWR|
		 VAC_CTRL_DELAY_IOWR(3)|
		 VAC_CTRL_DELAY_IORD(3)|
		 VAC_CTRL_RECOVERY_IOSELI(2)|
		 VAC_CTRL_DSACK0|VAC_CTRL_DSACK1|
		 VAC_CTRL_DELAY_DSACKI(8),VAC_IOSEL2_CTRL);
	vac_outw(VAC_CTRL_IOWR|
		 VAC_CTRL_DELAY_IOWR(3)|
		 VAC_CTRL_DELAY_IORD(3)|
		 VAC_CTRL_RECOVERY_IOSELI(2)|
		 VAC_CTRL_DSACK0|VAC_CTRL_DSACK1|
		 VAC_CTRL_DELAY_DSACKI(8),VAC_IOSEL3_CTRL);
	vac_outw(VAC_CTRL_IOWR|
		 VAC_CTRL_DELAY_IOWR(3)|
		 VAC_CTRL_DELAY_IORD(3)|
		 VAC_CTRL_RECOVERY_IOSELI(2)|
		 VAC_CTRL_DELAY_DSACKI(8),VAC_IOSEL4_CTRL);
	vac_outw(VAC_CTRL_IOWR|
		 VAC_CTRL_DELAY_IOWR(3)|
		 VAC_CTRL_DELAY_IORD(3)|
		 VAC_CTRL_RECOVERY_IOSELI(2)|
		 VAC_CTRL_DELAY_DSACKI(8),VAC_IOSEL5_CTRL);

        vac_show();
}

static void __init vac_start(void)
{
	vac_outw(0, VAC_ID);
	vac_outw(VAC_INT_CTRL_TIMER_DISABLE|
		 VAC_INT_CTRL_UART_B_DISABLE|
		 VAC_INT_CTRL_UART_A_DISABLE|
		 VAC_INT_CTRL_MBOX_DISABLE|
		 VAC_INT_CTRL_PIO4_DISABLE|
		 VAC_INT_CTRL_PIO7_DISABLE|
		 VAC_INT_CTRL_PIO8_DISABLE|
		 VAC_INT_CTRL_PIO9_DISABLE,VAC_INT_CTRL);
	vac_outw(VAC_INT_CTRL_TIMER_PIO10|
		 VAC_INT_CTRL_UART_B_PIO7|
		 VAC_INT_CTRL_UART_A_PIO7,VAC_INT_CTRL);
	/*
	 *  Set quadro speed for both UARTs.
	 *  To do it we need use formulae from VIC/VAC manual,
	 *  keeping in mind Baget's 50MHz frequency...
	 */
	vac_outw((500000/(384*16))<<8,VAC_CPU_CLK_DIV);
}

static void __init vic_show(void)
{
	unsigned char val;
	char *timeout[]  = { "4", "16", "32", "64", "128", "256", "disabled" };
	char *deadlock[] = { "[dedlk only]", "[dedlk only]",
			     "[dedlk], [halt w/ rmc], [lberr]",
			     "[dedlk], [halt w/o rmc], [lberr]" };

	val = vic_inb(VIC_IFACE_CFG);
	if (val & VIC_IFACE_CFG_VME)
		printk("VMEbus controller ");
	if (val & VIC_IFACE_CFG_TURBO)
		printk("turbo ");
	if (val & VIC_IFACE_CFG_MSTAB)
		printk("metastability delay ");
	printk("%s ",
	       deadlock[VIC_IFACE_CFG_DEADLOCK_VAL(val)]);


	printk("interrupts: ");
	val = vic_inb(VIC_ERR_INT);
	if (!(val & VIC_ERR_INT_SYSFAIL))
		printk("[sysfail]");
	if (!(val & VIC_ERR_INT_TIMO))
		printk("[timeout]");
	if (!(val & VIC_ERR_INT_WRPOST))
		printk("[write post]");
	if (!(val & VIC_ERR_INT_ACFAIL))
		printk("[acfail] ");
	printk("\n");

	printk("timeouts: ");
	val = vic_inb(VIC_XFER_TIMO);
	printk("local %s, vme %s ",
	       timeout[VIC_XFER_TIMO_LOCAL_PERIOD_VAL(val)],
	       timeout[VIC_XFER_TIMO_VME_PERIOD_VAL(val)]);
	if (val & VIC_XFER_TIMO_VME)
		printk("acquisition ");
	if (val & VIC_XFER_TIMO_ARB)
		printk("arbitration ");
	printk("\n");

	val = vic_inb(VIC_LOCAL_TIM);
	printk("pas time: (%d,%d), ds time: %d\n",
	       VIC_LOCAL_TIM_PAS_ASSERT_VAL(val),
	       VIC_LOCAL_TIM_PAS_DEASSERT_VAL(val),
	       VIC_LOCAT_TIM_DS_DEASSERT_VAL(val));

	val = vic_inb(VIC_BXFER_DEF);
	printk("dma: ");
	if (val & VIC_BXFER_DEF_DUAL)
		printk("[dual path]");
	if (val & VIC_BXFER_DEF_LOCAL_CROSS)
		printk("[local boundary cross]");
	if (val & VIC_BXFER_DEF_VME_CROSS)
		printk("[vme boundary cross]");

}

static void __init vic_init(void)
{
	 unsigned char id = vic_inb(VIC_ID);
	 if ((id & 0xf0) != 0xf0)
		 panic("VIC not found");
	 printk(" VIC068A Rev. %X: ", id & 0x0f);

	 vic_outb(VIC_INT_IPL(3)|VIC_INT_DISABLE,VIC_VME_II);
	 vic_outb(VIC_INT_IPL(3)|VIC_INT_DISABLE,VIC_VME_INT1);
	 vic_outb(VIC_INT_IPL(3)|VIC_INT_DISABLE,VIC_VME_INT2);
	 vic_outb(VIC_INT_IPL(3)|VIC_INT_DISABLE,VIC_VME_INT3);
	 vic_outb(VIC_INT_IPL(3)|VIC_INT_DISABLE,VIC_VME_INT4);
/*
	 vic_outb(VIC_INT_IPL(3)|VIC_INT_DISABLE, VIC_VME_INT5);
*/
	 vic_outb(VIC_INT_IPL(3)|VIC_INT_DISABLE, VIC_VME_INT6);

	 vic_outb(VIC_INT_IPL(3)|VIC_INT_DISABLE, VIC_VME_INT7);
	 vic_outb(VIC_INT_IPL(3)|VIC_INT_DISABLE, VIC_DMA_INT);
	 vic_outb(VIC_INT_IPL(3)|VIC_INT_NOAUTO|VIC_INT_EDGE|
		  VIC_INT_LOW|VIC_INT_DISABLE, VIC_LINT1);
	 vic_outb(VIC_INT_IPL(3)|VIC_INT_NOAUTO|VIC_INT_EDGE|
		  VIC_INT_HIGH|VIC_INT_DISABLE, VIC_LINT2);
	 vic_outb(VIC_INT_IPL(3)|VIC_INT_NOAUTO|VIC_INT_EDGE|
		  VIC_INT_HIGH|VIC_INT_DISABLE, VIC_LINT3);
	 vic_outb(VIC_INT_IPL(3)|VIC_INT_NOAUTO|VIC_INT_EDGE|
		  VIC_INT_LOW|VIC_INT_DISABLE, VIC_LINT4);
/*
	 vic_outb(VIC_INT_IPL(3)|VIC_INT_NOAUTO|VIC_INT_LEVEL|
		  VIC_INT_LOW|VIC_INT_DISABLE, VIC_LINT5);
*/
	 vic_outb(VIC_INT_IPL(6)|VIC_INT_NOAUTO|VIC_INT_EDGE|
		  VIC_INT_LOW|VIC_INT_DISABLE, VIC_LINT6);
	 vic_outb(VIC_INT_IPL(6)|VIC_INT_NOAUTO|VIC_INT_EDGE|
		  VIC_INT_LOW|VIC_INT_DISABLE, VIC_LINT7);

	 vic_outb(VIC_INT_IPL(3)|
		  VIC_INT_SWITCH(0)|
		  VIC_INT_SWITCH(1)|
		  VIC_INT_SWITCH(2)|
		  VIC_INT_SWITCH(3), VIC_ICGS_INT);
	 vic_outb(VIC_INT_IPL(3)|
		  VIC_INT_SWITCH(0)|
		  VIC_INT_SWITCH(1)|
		  VIC_INT_SWITCH(2)|
		  VIC_INT_SWITCH(3), VIC_ICMS_INT);
	 vic_outb(VIC_INT_IPL(6)|
		  VIC_ERR_INT_SYSFAIL|
		  VIC_ERR_INT_TIMO|
		  VIC_ERR_INT_WRPOST|
		  VIC_ERR_INT_ACFAIL, VIC_ERR_INT);
	 vic_outb(VIC_ICxS_BASE_ID(0xf), VIC_ICGS_BASE);
	 vic_outb(VIC_ICxS_BASE_ID(0xe), VIC_ICMS_BASE);
	 vic_outb(VIC_LOCAL_BASE_ID(0x6), VIC_LOCAL_BASE);
	 vic_outb(VIC_ERR_BASE_ID(0x3), VIC_ERR_BASE);
	 vic_outb(VIC_XFER_TIMO_VME_PERIOD_32|
		  VIC_XFER_TIMO_LOCAL_PERIOD_32, VIC_XFER_TIMO);
	 vic_outb(VIC_LOCAL_TIM_PAS_ASSERT(2)|
		  VIC_LOCAT_TIM_DS_DEASSERT(1)|
		  VIC_LOCAL_TIM_PAS_DEASSERT(1), VIC_LOCAL_TIM);
	 vic_outb(VIC_BXFER_DEF_VME_CROSS|
		  VIC_BXFER_DEF_LOCAL_CROSS|
		  VIC_BXFER_DEF_AMSR|
		  VIC_BXFER_DEF_DUAL, VIC_BXFER_DEF);
	 vic_outb(VIC_SSxCR0_LOCAL_XFER_SINGLE|
		  VIC_SSxCR0_A32|VIC_SSxCR0_D32|
		  VIC_SS0CR0_TIMER_FREQ_NONE, VIC_SS0CR0);
	 vic_outb(VIC_SSxCR1_TF1(0xf)|
		  VIC_SSxCR1_TF2(0xf), VIC_SS0CR1);
	 vic_outb(VIC_SSxCR0_LOCAL_XFER_SINGLE|
		  VIC_SSxCR0_A24|VIC_SSxCR0_D32, VIC_SS1CR0);
	 vic_outb(VIC_SSxCR1_TF1(0xf)|
		  VIC_SSxCR1_TF2(0xf), VIC_SS1CR1);
         vic_outb(VIC_IFACE_CFG_NOHALT|
		  VIC_IFACE_CFG_NOTURBO, VIC_IFACE_CFG);
	 vic_outb(VIC_AMS_CODE(0), VIC_AMS);
	 vic_outb(VIC_BXFER_CTRL_INTERLEAVE(0), VIC_BXFER_CTRL);
	 vic_outb(0, VIC_BXFER_LEN_LO);
	 vic_outb(0, VIC_BXFER_LEN_HI);
	 vic_outb(VIC_REQ_CFG_FAIRNESS_DISABLED|
		  VIC_REQ_CFG_LEVEL(3)|
		  VIC_REQ_CFG_RR_ARBITRATION, VIC_REQ_CFG);
	 vic_outb(VIC_RELEASE_BLKXFER_BLEN(0)|
		  VIC_RELEASE_RWD, VIC_RELEASE);
	 vic_outb(VIC_IC6_RUN, VIC_IC6);
	 vic_outb(0, VIC_IC7);

	 vic_show();
}

static void vic_start(void)
{
	vic_outb(VIC_INT_IPL(3)|
		 VIC_INT_NOAUTO|
		 VIC_INT_EDGE|
		 VIC_INT_HIGH|
		 VIC_INT_ENABLE, VIC_LINT7);
}

void __init baget_irq_setup(void)
{
	extern void bagetIRQ(void);

        /* Now, it's safe to set the exception vector. */
	set_except_vector(0, bagetIRQ);
}

extern void baget_machine_restart(char *command);
extern void baget_machine_halt(void);
extern void baget_machine_power_off(void);

void __init baget_setup(void)
{
	printk("BT23/63-201n found.\n");
	*BAGET_WRERR_ACK = 0;
	irq_setup = baget_irq_setup;

        _machine_restart   = baget_machine_restart;
        _machine_halt      = baget_machine_halt;
        _machine_power_off = baget_machine_power_off;

	vac_init();
	vic_init();
	vac_start();
	vic_start();
}
