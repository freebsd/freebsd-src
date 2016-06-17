#include <linux/kernel.h>
#include <asm/system.h>
#include <asm/baget/baget.h>


#define R3000_RESET_VEC  0xbfc00000
typedef void vector(void);


static void baget_reboot(char *from_fun)
{
	cli();
	baget_printk("\n%s: jumping to RESET code...\n", from_fun);
	(*(vector*)R3000_RESET_VEC)();
}

/* fixme: proper functionality */

void baget_machine_restart(char *command)
{
	baget_reboot("restart");
}

void baget_machine_halt(void)
{
	baget_reboot("halt");
}

void baget_machine_power_off(void)
{
	baget_reboot("power off");
}
