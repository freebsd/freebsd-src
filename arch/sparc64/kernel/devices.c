/* devices.c: Initial scan of the prom device tree for important
 *            Sparc device nodes which we need to find.
 *
 * Copyright (C) 1996 David S. Miller (davem@caip.rutgers.edu)
 */

#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/threads.h>
#include <linux/init.h>
#include <linux/ioport.h>
#include <linux/string.h>

#include <asm/page.h>
#include <asm/oplib.h>
#include <asm/system.h>
#include <asm/smp.h>
#include <asm/spitfire.h>

struct prom_cpuinfo linux_cpus[64] __initdata = { { 0 } };
unsigned prom_cpu_nodes[64];
int linux_num_cpus = 0;

extern void cpu_probe(void);
extern void central_probe(void);

void __init device_scan(void)
{
	char node_str[128];
	int nd, prom_node_cpu, thismid;
	int cpu_nds[64];  /* One node for each cpu */
	int cpu_ctr = 0;

	/* FIX ME FAST... -DaveM */
	ioport_resource.end = 0xffffffffffffffffUL;
	iomem_resource.end = 0xffffffffffffffffUL;

	prom_getstring(prom_root_node, "device_type", node_str, sizeof(node_str));

	prom_printf("Booting Linux...\n");
	if(strcmp(node_str, "cpu") == 0) {
		cpu_nds[0] = prom_root_node;
		linux_cpus[0].prom_node = prom_root_node;
		linux_cpus[0].mid = 0;
		cpu_ctr++;
	} else {
		int scan;
		scan = prom_getchild(prom_root_node);
		/* prom_printf("root child is %08x\n", (unsigned) scan); */
		nd = 0;
		while((scan = prom_getsibling(scan)) != 0) {
			prom_getstring(scan, "device_type", node_str, sizeof(node_str));
			if(strcmp(node_str, "cpu") == 0) {
				cpu_nds[cpu_ctr] = scan;
				linux_cpus[cpu_ctr].prom_node = scan;
				thismid = 0;
				if (tlb_type == spitfire) {
					prom_getproperty(scan, "upa-portid",
							 (char *) &thismid, sizeof(thismid));
				} else if (tlb_type == cheetah ||
					   tlb_type == cheetah_plus) {
					prom_getproperty(scan, "portid",
							 (char *) &thismid, sizeof(thismid));
				}
				linux_cpus[cpu_ctr].mid = thismid;
				printk("Found CPU %d (node=%08x,mid=%d)\n",
				       cpu_ctr, (unsigned) scan, thismid);
				cpu_ctr++;
			}
		};
		if(cpu_ctr == 0) {
			prom_printf("No CPU nodes found, cannot continue.\n");
			prom_halt();
		}
		printk("Found %d CPU prom device tree node(s).\n", cpu_ctr);
	}
	prom_node_cpu = cpu_nds[0];

	linux_num_cpus = cpu_ctr;
	
	prom_cpu_nodes[0] = prom_node_cpu;

#ifndef CONFIG_SMP
	{
		extern unsigned long up_clock_tick;
		up_clock_tick = prom_getintdefault(prom_node_cpu,
						   "clock-frequency",
						   0);
	}
#endif

	central_probe();

	cpu_probe();
}
