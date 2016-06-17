/*
 *  linux/include/asm-arm/mach/amba_kmi.h
 *
 *  Copyright (C) 2000 Deep Blue Solutions Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
struct kmi_info {
	u_int			base;
	u_int			irq;
	u_char			divisor;
	u_char			type;
	u_char			state;
	u_char			prev_rx;

	u_char			last_tx;
	u_char			resend_count;
	u_short			res;

	u_char			present;
	u_char			reconnect;
	u_char			config_num;
	u_char			hotplug_state;

	wait_queue_head_t	wait_q;
	void			(*rx)(struct kmi_info *, u_int val,
				      struct pt_regs *regs);
	char			name[8];
};

#define KMI_KEYBOARD		0
#define KMI_MOUSE		1

int register_kmi(struct kmi_info *kmi);
