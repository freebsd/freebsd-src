/*
 * Reset a Jazz machine.
 */
#include <linux/sched.h>
#include <asm/jazz.h>
#include <asm/io.h>
#include <asm/system.h>
#include <asm/reboot.h>
#include <asm/delay.h>
#include <asm/keyboard.h>

static inline void kb_wait(void)
{
	unsigned long start = jiffies;

	do {
		if (! (kbd_read_status() & 0x02))
			return;
	} while (jiffies - start < 50);
}

void jazz_machine_restart(char *command)
{
    while (1) {
	kb_wait ();
	kbd_write_command (0xd1);
	kb_wait ();
	kbd_write_output (0x00);
    }
}

void jazz_machine_halt(void)
{
}

void jazz_machine_power_off(void)
{
	/* Jazz machines don't have a software power switch */
}
