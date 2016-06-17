#ifndef __ASM_SH_BUGS_H
#define __ASM_SH_BUGS_H

/*
 * This is included by init/main.c to check for architecture-dependent bugs.
 *
 * Needs:
 *	void check_bugs(void);
 */

/*
 * I don't know of any Super-H bugs yet.
 */

#include <asm/processor.h>

static void __init check_bugs(void)
{
	extern unsigned long loops_per_jiffy;
	char *p= &system_utsname.machine[2]; /* "sh" */

	cpu_data->loops_per_jiffy = loops_per_jiffy;
	
	switch (cpu_data->type) {
	case CPU_SH7708:
		*p++ = '3';
		printk("CPU: SH7707/SH7708/SH7709\n");
		break;
	case CPU_SH7729:
		*p++ = '3';
		printk("CPU: SH7709A/SH7729\n");
		break;
	case CPU_SH7750:
		*p++ = '4';
		printk("CPU: SH7750/SH7751\n");
		break;
	case CPU_ST40:
		*p++ = '4';
		printk("CPU: ST40STB1/GX1\n");
		break;
	default:
		printk("CPU: ??????\n");
		break;
	}

#ifndef __LITTLE_ENDIAN__
	/* 'eb' means 'Endian Big' */
	*p++ = 'e';
	*p++ = 'b';
#endif
	*p = '\0';
}
#endif /* __ASM_SH_BUGS_H */
