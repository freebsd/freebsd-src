/*********************************************************************
 *                
 * Filename:      esi.c
 * Version:       1.5
 * Description:   Driver for the Extended Systems JetEye PC dongle
 * Status:        Experimental.
 * Author:        Dag Brattli <dagb@cs.uit.no>
 * Created at:    Sat Feb 21 18:54:38 1998
 * Modified at:   Fri Dec 17 09:14:04 1999
 * Modified by:   Dag Brattli <dagb@cs.uit.no>
 * 
 *     Copyright (c) 1999 Dag Brattli, <dagb@cs.uit.no>,
 *     Copyright (c) 1998 Thomas Davis, <ratbert@radiks.net>,
 *     All Rights Reserved.
 *     
 *     This program is free software; you can redistribute it and/or 
 *     modify it under the terms of the GNU General Public License as 
 *     published by the Free Software Foundation; either version 2 of 
 *     the License, or (at your option) any later version.
 * 
 *     This program is distributed in the hope that it will be useful,
 *     but WITHOUT ANY WARRANTY; without even the implied warranty of
 *     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *     GNU General Public License for more details.
 * 
 *     You should have received a copy of the GNU General Public License 
 *     along with this program; if not, write to the Free Software 
 *     Foundation, Inc., 59 Temple Place, Suite 330, Boston, 
 *     MA 02111-1307 USA
 *     
 ********************************************************************/

#include <linux/module.h>

#include <linux/delay.h>
#include <linux/tty.h>
#include <linux/sched.h>
#include <linux/init.h>

#include <net/irda/irda.h>
#include <net/irda/irmod.h>
#include <net/irda/irda_device.h>

static void esi_open(dongle_t *self, struct qos_info *qos);
static void esi_close(dongle_t *self);
static int  esi_change_speed(struct irda_task *task);
static int  esi_reset(struct irda_task *task);

static struct dongle_reg dongle = {
	Q_NULL,
	IRDA_ESI_DONGLE,
	esi_open,
	esi_close,
	esi_reset,
	esi_change_speed,
};

int __init esi_init(void)
{
	return irda_device_register_dongle(&dongle);
}

void esi_cleanup(void)
{
	irda_device_unregister_dongle(&dongle);
}

static void esi_open(dongle_t *self, struct qos_info *qos)
{
	qos->baud_rate.bits &= IR_9600|IR_19200|IR_115200;
	qos->min_turn_time.bits = 0x01; /* Needs at least 10 ms */

	MOD_INC_USE_COUNT;
}

static void esi_close(dongle_t *dongle)
{		
	/* Power off dongle */
	dongle->set_dtr_rts(dongle->dev, FALSE, FALSE);

	MOD_DEC_USE_COUNT;
}

/*
 * Function esi_change_speed (task)
 *
 *    Set the speed for the Extended Systems JetEye PC ESI-9680 type dongle
 *
 */
static int esi_change_speed(struct irda_task *task)
{
	dongle_t *self = (dongle_t *) task->instance;
	__u32 speed = (__u32) task->param;
	int dtr, rts;
	
	switch (speed) {
	case 19200:
		dtr = TRUE;
		rts = FALSE;
		break;
	case 115200:
		dtr = rts = TRUE;
		break;
	case 9600:
	default:
		dtr = FALSE;
		rts = TRUE;
		break;
	}

	/* Change speed of dongle */
	self->set_dtr_rts(self->dev, dtr, rts);
	self->speed = speed;

	irda_task_next_state(task, IRDA_TASK_DONE);

	return 0;
}

/*
 * Function esi_reset (task)
 *
 *    Reset dongle;
 *
 */
static int esi_reset(struct irda_task *task)
{
	dongle_t *self = (dongle_t *) task->instance;
	
	self->set_dtr_rts(self->dev, FALSE, FALSE);
	irda_task_next_state(task, IRDA_TASK_DONE);

	return 0;
}

#ifdef MODULE
MODULE_AUTHOR("Dag Brattli <dagb@cs.uit.no>");
MODULE_DESCRIPTION("Extended Systems JetEye PC dongle driver");
MODULE_LICENSE("GPL");

/*
 * Function init_module (void)
 *
 *    Initialize ESI module
 *
 */
int init_module(void)
{
	return esi_init();
}

/*
 * Function cleanup_module (void)
 *
 *    Cleanup ESI module
 *
 */
void cleanup_module(void)
{
        esi_cleanup();
}
#endif /* MODULE */

