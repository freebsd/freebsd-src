#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/kernel_stat.h>

#include <asm/system.h>
#include <asm/irq.h>
#include <asm/traps.h>
#include <asm/page.h>
#include <asm/machdep.h>
#include <asm/apollohw.h>

static irq_handler_t dn_irqs[16];

void dn_process_int(int irq, struct pt_regs *fp) {


  if(dn_irqs[irq-160].handler) {
    dn_irqs[irq-160].handler(irq,dn_irqs[irq-160].dev_id,fp);
  }
  else {
    printk("spurious irq %d occurred\n",irq);
  }

  *(volatile unsigned char *)(pica)=0x20;
  *(volatile unsigned char *)(picb)=0x20;

}

void dn_init_IRQ(void) {

  int i;

  for(i=0;i<16;i++) {
    dn_irqs[i].handler=NULL;
    dn_irqs[i].flags=IRQ_FLG_STD;
    dn_irqs[i].dev_id=NULL;
    dn_irqs[i].devname=NULL;
  }
  
}

int dn_request_irq(unsigned int irq, void (*handler)(int, void *, struct pt_regs *), unsigned long flags, const char *devname, void *dev_id) {

  if((irq<0) || (irq>15)) {
    printk("Trying to request invalid IRQ\n");
    return -ENXIO;
  }

  if(!dn_irqs[irq].handler) {
    dn_irqs[irq].handler=handler;
    dn_irqs[irq].flags=IRQ_FLG_STD;
    dn_irqs[irq].dev_id=dev_id;
    dn_irqs[irq].devname=devname;
    if(irq<8)
      *(volatile unsigned char *)(pica+1)&=~(1<<irq);
    else
      *(volatile unsigned char *)(picb+1)&=~(1<<(irq-8));

    return 0;
  }
  else {
    printk("Trying to request already assigned irq %d\n",irq);
    return -ENXIO;
  }

}

void dn_free_irq(unsigned int irq, void *dev_id) {

  if((irq<0) || (irq>15)) {
    printk("Trying to free invalid IRQ\n");
    return ;
  }

  if(irq<8)
    *(volatile unsigned char *)(pica+1)|=(1<<irq);
  else
    *(volatile unsigned char *)(picb+1)|=(1<<(irq-8));  

  dn_irqs[irq].handler=NULL;
  dn_irqs[irq].flags=IRQ_FLG_STD;
  dn_irqs[irq].dev_id=NULL;
  dn_irqs[irq].devname=NULL;

  return ;

}

void dn_enable_irq(unsigned int irq) {

  printk("dn enable irq\n");

}

void dn_disable_irq(unsigned int irq) {

  printk("dn disable irq\n");

}

int dn_get_irq_list(char *buf) {

  printk("dn get irq list\n");

  return 0;

}

struct fb_info *dn_dummy_fb_init(long *mem_start) {

  printk("fb init\n");

  return NULL;

}

#ifdef CONFIG_VT
extern void write_keyb_cmd(u_short length, u_char *cmd);
static char BellOnCommand[] =  { 0xFF, 0x21, 0x81 },
		    BellOffCommand[] = { 0xFF, 0x21, 0x82 };

static void dn_nosound (unsigned long ignored) {

	write_keyb_cmd(sizeof(BellOffCommand),BellOffCommand);

}

void dn_mksound( unsigned int count, unsigned int ticks ) {

	static struct timer_list sound_timer = { function: dn_nosound };

	del_timer( &sound_timer );
	if(count) {
		write_keyb_cmd(sizeof(BellOnCommand),BellOnCommand);
		if (ticks) {
       		sound_timer.expires = jiffies + ticks;
			add_timer( &sound_timer );
		}
	}
	else
		write_keyb_cmd(sizeof(BellOffCommand),BellOffCommand);
}
#endif /* CONFIG_VT */


void dn_dummy_video_setup(char *options,int *ints) {

  printk("no video yet\n");

}
