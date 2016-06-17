/*
 * linux/arch/mips/tx4927/toshiba_rbtx4927/toshiba_rbtx4927_led.c
 *
 * RBTX4927 Status LED toggle
 *
 * Copyright (C) 2003 TimeSys Corp.
 *                    S. James Hill (James.Hill@timesys.com)
 *                                  (sjhill@realitydiluted.com)
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the
 *  Free Software Foundation; either version 2 of the License, or (at your
 *  option) any later version.
 *
 *  THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED
 *  WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 *  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 *  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 *  OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 *  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 *  TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 *  USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  675 Mass Ave, Cambridge, MA 02139, USA.
 */
#include <linux/init.h>
#include <asm/io.h>
#include <asm/timex.h>
#include <asm/tx4927/toshiba_rbtx4927.h>

static struct led_state {
	struct timer_list timer;
	unsigned char val123;
	unsigned long val45;
} led_state;

void led_toggle (unsigned long v)
{
	struct led_state *l = (struct led_state *) v;

	writeb(l->val123, RBTX4927_STATUS_LED_123);
	writel(l->val45, RBTX4927_STATUS_LED_45);

	l->val123 = ~l->val123;
	l->val45 = ~l->val45;

	l->timer.expires = jiffies + HZ;
	add_timer(&l->timer);
}

int __init led_setup (void)
{
	led_state.val123 = 0xfd;
#ifdef __MIPSEB__
	led_state.val45 = 0x5a000000;
#else
	led_state.val45 = 0x0000005a;
#endif

	led_state.timer.data = (unsigned long) &led_state;
	led_state.timer.expires = jiffies + HZ;
	init_timer(&led_state.timer);
	led_state.timer.function = led_toggle;
	add_timer(&led_state.timer);

	return 0;
}

__initcall (led_setup);
