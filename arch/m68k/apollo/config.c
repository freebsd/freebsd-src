#include <linux/config.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/tty.h>
#include <linux/console.h>
#include <linux/rtc.h>
#include <linux/vt_kern.h>

#include <asm/setup.h>
#include <asm/bootinfo.h>
#include <asm/system.h>
#include <asm/pgtable.h>
#include <asm/apollohw.h>
#include <asm/irq.h>
#include <asm/rtc.h>
#include <asm/machdep.h>

u_long sio01_physaddr;
u_long sio23_physaddr;
u_long rtc_physaddr;
u_long pica_physaddr;
u_long picb_physaddr;
u_long cpuctrl_physaddr;
u_long timer_physaddr;
u_long apollo_model;

extern void dn_sched_init(void (*handler)(int,void *,struct pt_regs *));
extern int dn_keyb_init(void);
extern int dn_dummy_kbdrate(struct kbd_repeat *);
extern void dn_init_IRQ(void);
extern int dn_request_irq(unsigned int irq, void (*handler)(int, void *, struct pt_regs *), unsigned long flags, const char *devname, void *dev_id);
extern void dn_free_irq(unsigned int irq, void *dev_id);
extern void dn_enable_irq(unsigned int);
extern void dn_disable_irq(unsigned int);
extern int dn_get_irq_list(char *);
extern unsigned long dn_gettimeoffset(void);
extern void dn_gettod(int *, int *, int *, int *, int *, int *);
extern int dn_dummy_hwclk(int, struct rtc_time *);
extern int dn_dummy_set_clock_mmss(unsigned long);
extern void dn_mksound(unsigned int count, unsigned int ticks);
extern void dn_dummy_reset(void);
extern void dn_dummy_waitbut(void);
extern struct fb_info *dn_fb_init(long *);
extern void dn_dummy_debug_init(void);
extern void dn_dummy_video_setup(char *,int *);
extern void dn_process_int(int irq, struct pt_regs *fp);
#ifdef CONFIG_HEARTBEAT
static void dn_heartbeat(int on);
#endif
static void dn_timer_int(int irq,void *, struct pt_regs *);
static void (*sched_timer_handler)(int, void *, struct pt_regs *)=NULL;
static void dn_get_model(char *model);
static const char *apollo_models[] = {
	"DN3000 (Otter)",
	"DN3010 (Otter)",
	"DN3500 (Cougar II)",
	"DN4000 (Mink)",
	"DN4500 (Roadrunner)" };

int apollo_parse_bootinfo(const struct bi_record *record) {

	int unknown = 0;
	const unsigned long *data = record->data;

	switch(record->tag) {
		case BI_APOLLO_MODEL: 
			apollo_model=*data;	
			break;

		default:
			 unknown=1;
	}
	
	return unknown;
}

void dn_setup_model(void) {
	

	printk("Apollo hardware found: ");
	printk("[%s]\n", apollo_models[apollo_model - APOLLO_DN3000]);

	switch(apollo_model) {
		case APOLLO_UNKNOWN:
			panic("Unknown apollo model");
			break;
		case APOLLO_DN3000:
		case APOLLO_DN3010:
			sio01_physaddr=SAU8_SIO01_PHYSADDR;	
			rtc_physaddr=SAU8_RTC_PHYSADDR;	
			pica_physaddr=SAU8_PICA;	
			picb_physaddr=SAU8_PICB;	
			cpuctrl_physaddr=SAU8_CPUCTRL;
			timer_physaddr=SAU8_TIMER;
			break;
		case APOLLO_DN4000:
			sio01_physaddr=SAU7_SIO01_PHYSADDR;	
			sio23_physaddr=SAU7_SIO23_PHYSADDR;	
			rtc_physaddr=SAU7_RTC_PHYSADDR;	
			pica_physaddr=SAU7_PICA;	
			picb_physaddr=SAU7_PICB;	
			cpuctrl_physaddr=SAU7_CPUCTRL;
			timer_physaddr=SAU7_TIMER;
			break;
		case APOLLO_DN4500:
			panic("Apollo model not yet supported");
			break;
		case APOLLO_DN3500:
			sio01_physaddr=SAU7_SIO01_PHYSADDR;	
			sio23_physaddr=SAU7_SIO23_PHYSADDR;	
			rtc_physaddr=SAU7_RTC_PHYSADDR;	
			pica_physaddr=SAU7_PICA;	
			picb_physaddr=SAU7_PICB;	
			cpuctrl_physaddr=SAU7_CPUCTRL;
			timer_physaddr=SAU7_TIMER;
			break;
		default:
			panic("Undefined apollo model");
			break;
	}


}

int dn_serial_console_wait_key(struct console *co) {

	while(!(sio01.srb_csrb & 1))
		barrier();
	return sio01.rhrb_thrb;
}

void dn_serial_console_write (struct console *co, const char *str,unsigned int count)
{
   while(count--) {
	if (*str == '\n') { 
    	sio01.rhrb_thrb = (unsigned char)'\r';
       	while (!(sio01.srb_csrb & 0x4))
                ;
 	}
    sio01.rhrb_thrb = (unsigned char)*str++;
    while (!(sio01.srb_csrb & 0x4))
            ;
  }	
}
 
void dn_serial_print (const char *str)
{
    while (*str) {
        if (*str == '\n') {
            sio01.rhrb_thrb = (unsigned char)'\r';
            while (!(sio01.srb_csrb & 0x4))
                ;
        }
        sio01.rhrb_thrb = (unsigned char)*str++;
        while (!(sio01.srb_csrb & 0x4))
            ;
    }
}

void config_apollo(void) {

	int i;

	dn_setup_model();	

	mach_sched_init=dn_sched_init; /* */
#ifdef CONFIG_VT
	mach_keyb_init=dn_keyb_init;
	mach_kbdrate=dn_dummy_kbdrate;
	kd_mksound	     = dn_mksound;
#endif
	mach_init_IRQ=dn_init_IRQ;
	mach_default_handler=NULL;
	mach_request_irq     = dn_request_irq;
	mach_free_irq        = dn_free_irq;
	enable_irq      = dn_enable_irq;
	disable_irq     = dn_disable_irq;
	mach_get_irq_list    = dn_get_irq_list;
	mach_gettimeoffset   = dn_gettimeoffset;
	mach_gettod	     = dn_gettod; /* */
	mach_max_dma_address = 0xffffffff;
	mach_hwclk           = dn_dummy_hwclk; /* */
	mach_set_clock_mmss  = dn_dummy_set_clock_mmss; /* */
	mach_process_int     = dn_process_int;
	mach_reset	     = dn_dummy_reset;  /* */
#ifdef CONFIG_DUMMY_CONSOLE
        conswitchp           = &dummy_con;
#endif
#ifdef CONFIG_HEARTBEAT
  	mach_heartbeat = dn_heartbeat;
#endif
	mach_get_model       = dn_get_model;

	cpuctrl=0xaa00;

	/* clear DMA translation table */
	for(i=0;i<0x400;i++) 
		addr_xlat_map[i]=0;

}		

void dn_timer_int(int irq, void *dev_id, struct pt_regs *fp) {

	volatile unsigned char x;

	sched_timer_handler(irq,dev_id,fp);
	
	x=*(volatile unsigned char *)(timer+3);
	x=*(volatile unsigned char *)(timer+5);

}

void dn_sched_init(void (*timer_routine)(int, void *, struct pt_regs *)) {

	/* program timer 1 */       	
	*(volatile unsigned char *)(timer+3)=0x01;
	*(volatile unsigned char *)(timer+1)=0x40;
	*(volatile unsigned char *)(timer+5)=0x09;
	*(volatile unsigned char *)(timer+7)=0xc4;

	/* enable IRQ of PIC B */
	*(volatile unsigned char *)(pica+1)&=(~8);

#if 0
	printk("*(0x10803) %02x\n",*(volatile unsigned char *)(timer+0x3));
	printk("*(0x10803) %02x\n",*(volatile unsigned char *)(timer+0x3));
#endif

	sched_timer_handler=timer_routine;
	request_irq(0,dn_timer_int,0,NULL,NULL);

}

unsigned long dn_gettimeoffset(void) {

	return 0xdeadbeef;

}

void dn_gettod(int *yearp, int *monp, int *dayp,
	       int *hourp, int *minp, int *secp) {

  *yearp=rtc->year;
  *monp=rtc->month;
  *dayp=rtc->day_of_month;
  *hourp=rtc->hours;
  *minp=rtc->minute;
  *secp=rtc->second;

printk("gettod: %d %d %d %d %d %d\n",*yearp,*monp,*dayp,*hourp,*minp,*secp);

}

int dn_dummy_hwclk(int op, struct rtc_time *t) {


  if(!op) { /* read */
    t->tm_sec=rtc->second;
    t->tm_min=rtc->minute;
    t->tm_hour=rtc->hours;
    t->tm_mday=rtc->day_of_month;
    t->tm_wday=rtc->day_of_week;
    t->tm_mon=rtc->month;
    t->tm_year=rtc->year;
  } else {
    rtc->second=t->tm_sec;
    rtc->minute=t->tm_min;
    rtc->hours=t->tm_hour;
    rtc->day_of_month=t->tm_mday;
    if(t->tm_wday!=-1)
      rtc->day_of_week=t->tm_wday;
    rtc->month=t->tm_mon;
    rtc->year=t->tm_year;
  }

  return 0;

}

int dn_dummy_set_clock_mmss(unsigned long nowtime) {

  printk("set_clock_mmss\n");

  return 0;

}

void dn_dummy_reset(void) {

  dn_serial_print("The end !\n");

  for(;;);

}
	
void dn_dummy_waitbut(void) {

  dn_serial_print("waitbut\n");

}

static void dn_get_model(char *model)
{
    strcpy(model, "Apollo ");
    if (apollo_model >= APOLLO_DN3000 && apollo_model <= APOLLO_DN4500)
        strcat(model, apollo_models[apollo_model - APOLLO_DN3000]);
}

#ifdef CONFIG_HEARTBEAT
static int dn_cpuctrl=0xff00;

static void dn_heartbeat(int on) {

	if(on) { 
		dn_cpuctrl&=~0x100;
		cpuctrl=dn_cpuctrl;
	}
	else {
		dn_cpuctrl&=~0x100;
		dn_cpuctrl|=0x100;
		cpuctrl=dn_cpuctrl;
	}
}
#endif

