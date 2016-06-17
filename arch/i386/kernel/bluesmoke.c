
#include <linux/init.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/config.h>
#include <asm/processor.h> 
#include <asm/msr.h>

#ifdef CONFIG_X86_MCE

static int mce_disabled __initdata = 0;

/*
 *	Machine Check Handler For PII/PIII
 */

static int banks;

static void intel_machine_check(struct pt_regs * regs, long error_code)
{
	int recover=1;
	u32 alow, ahigh, high, low;
	u32 mcgstl, mcgsth;
	int i;
	
	rdmsr(MSR_IA32_MCG_STATUS, mcgstl, mcgsth);
	if(mcgstl&(1<<0))	/* Recoverable ? */
		recover=0;

	printk(KERN_EMERG "CPU %d: Machine Check Exception: %08x%08x\n", smp_processor_id(), mcgsth, mcgstl);
	
	for(i=0;i<banks;i++)
	{
		rdmsr(MSR_IA32_MC0_STATUS+i*4,low, high);
		if(high&(1<<31))
		{
			if(high&(1<<29))
				recover|=1;
			if(high&(1<<25))
				recover|=2;
			printk(KERN_EMERG "Bank %d: %08x%08x", i, high, low);
			high&=~(1<<31);
			if(high&(1<<27))
			{
				rdmsr(MSR_IA32_MC0_MISC+i*4, alow, ahigh);
				printk("[%08x%08x]", ahigh, alow);
			}
			if(high&(1<<26))
			{
				rdmsr(MSR_IA32_MC0_ADDR+i*4, alow, ahigh);
				printk(" at %08x%08x", ahigh, alow);
			}
			printk("\n");
			/* Clear it */
			wrmsr(MSR_IA32_MC0_STATUS+i*4, 0UL, 0UL);
			/* Serialize */
			wmb();
		}
	}
	
	if(recover&2)
		panic("CPU context corrupt");
	if(recover&1)
		panic("Unable to continue");
	printk(KERN_EMERG "Attempting to continue.\n");
	mcgstl&=~(1<<2);
	wrmsr(MSR_IA32_MCG_STATUS,mcgstl, mcgsth);
}

/*
 *	Machine check handler for Pentium class Intel
 */
 
static void pentium_machine_check(struct pt_regs * regs, long error_code)
{
	u32 loaddr, hi, lotype;
	rdmsr(MSR_IA32_P5_MC_ADDR, loaddr, hi);
	rdmsr(MSR_IA32_P5_MC_TYPE, lotype, hi);
	printk(KERN_EMERG "CPU#%d: Machine Check Exception:  0x%8X (type 0x%8X).\n", smp_processor_id(), loaddr, lotype);
	if(lotype&(1<<5))
		printk(KERN_EMERG "CPU#%d: Possible thermal failure (CPU on fire ?).\n", smp_processor_id());
}

/*
 *	Machine check handler for WinChip C6
 */
 
static void winchip_machine_check(struct pt_regs * regs, long error_code)
{
	printk(KERN_EMERG "CPU#%d: Machine Check Exception.\n", smp_processor_id());
}

/*
 *	Handle unconfigured int18 (should never happen)
 */

static void unexpected_machine_check(struct pt_regs * regs, long error_code)
{	
	printk(KERN_ERR "CPU#%d: Unexpected int18 (Machine Check).\n", smp_processor_id());
}

/*
 *	Call the installed machine check handler for this CPU setup.
 */

static void (*machine_check_vector)(struct pt_regs *, long error_code) = unexpected_machine_check;

asmlinkage void do_machine_check(struct pt_regs * regs, long error_code)
{
	machine_check_vector(regs, error_code);
}

/*
 *	Set up machine check reporting for Intel processors
 */

static void __init intel_mcheck_init(struct cpuinfo_x86 *c)
{
	u32 l, h;
	int i;
	static int done;
	
	/*
	 *	Check for MCE support
	 */

	if( !test_bit(X86_FEATURE_MCE, &c->x86_capability) )
		return;	
	
	/*
	 *	Pentium machine check
	 */
	
	if(c->x86 == 5)
	{
		/* Default P5 to off as its often misconnected */
		if(mce_disabled != -1)
			return;
		machine_check_vector = pentium_machine_check;
		wmb();
		/* Read registers before enabling */
		rdmsr(MSR_IA32_P5_MC_ADDR, l, h);
		rdmsr(MSR_IA32_P5_MC_TYPE, l, h);
		if(done==0)
			printk(KERN_INFO "Intel old style machine check architecture supported.\n");
 		/* Enable MCE */
		set_in_cr4(X86_CR4_MCE);
		printk(KERN_INFO "Intel old style machine check reporting enabled on CPU#%d.\n", smp_processor_id());
		return;
	}
	

	/*
	 *	Check for PPro style MCA
	 */
	 		
	if( !test_bit(X86_FEATURE_MCA, &c->x86_capability) )
		return;
		
	/* Ok machine check is available */
	
	machine_check_vector = intel_machine_check;
	wmb();
	
	if(done==0)
		printk(KERN_INFO "Intel machine check architecture supported.\n");
	rdmsr(MSR_IA32_MCG_CAP, l, h);
	if(l&(1<<8))
		wrmsr(MSR_IA32_MCG_CTL, 0xffffffff, 0xffffffff);
	banks = l&0xff;
	for(i=1;i<banks;i++)
	{
		wrmsr(MSR_IA32_MC0_CTL+4*i, 0xffffffff, 0xffffffff);
	}
	for(i=0;i<banks;i++)
	{
		wrmsr(MSR_IA32_MC0_STATUS+4*i, 0x0, 0x0);
	}
	set_in_cr4(X86_CR4_MCE);
	printk(KERN_INFO "Intel machine check reporting enabled on CPU#%d.\n", smp_processor_id());
	done=1;
}

/*
 *	Set up machine check reporting on the Winchip C6 series
 */
 
static void __init winchip_mcheck_init(struct cpuinfo_x86 *c)
{
	u32 lo, hi;
	/* Not supported on C3 */
	if(c->x86 != 5)
		return;
	/* Winchip C6 */
	machine_check_vector = winchip_machine_check;
	wmb();
	rdmsr(MSR_IDT_FCR1, lo, hi);
	lo|= (1<<2);	/* Enable EIERRINT (int 18 MCE) */
	lo&= ~(1<<4);	/* Enable MCE */
	wrmsr(MSR_IDT_FCR1, lo, hi);
	set_in_cr4(X86_CR4_MCE);
	printk(KERN_INFO "Winchip machine check reporting enabled on CPU#%d.\n", smp_processor_id());
}


/*
 *	This has to be run for each processor
 */



void __init mcheck_init(struct cpuinfo_x86 *c)
{
	if(mce_disabled==1)
		return;
		
	switch(c->x86_vendor)
	{
		case X86_VENDOR_AMD:
			/*
			 *	AMD K7 machine check is Intel like
			 */
			if(c->x86 == 6 || c->x86 == 15)
				intel_mcheck_init(c);
			break;
		case X86_VENDOR_INTEL:
			intel_mcheck_init(c);
			break;
		case X86_VENDOR_CENTAUR:
			winchip_mcheck_init(c);
			break;
		default:
			break;
	}
}

static int __init mcheck_disable(char *str)
{
	mce_disabled = 1;
	return 0;
}

static int __init mcheck_enable(char *str)
{
	mce_disabled = -1;
	return 0;
}

__setup("nomce", mcheck_disable);
__setup("mce", mcheck_enable);

#else
asmlinkage void do_machine_check(struct pt_regs * regs, long error_code) {}
void __init mcheck_init(struct cpuinfo_x86 *c) {}
#endif
