#!/usr/sbin/dtrace -s

/* Sample use of the cam dtrace provider */

/*
 * Trace all the non I/O commands flowing through CAM
 */

dtrace:::BEGIN
{
}

/*
 * There's two choke points in CAM. We can intercept the request on the way down
 * in xpt_action, just before it's sent to the SIM. This can be a good place to
 * see what's going on before it happens. However, most I/O happens quite
 * quickly, this isn't much of an advantage. The other place is on completion
 * when the transaction is finally done. The retry mechanism is built into the
 * periph driver, which is responsible for submitting the request.
 *
 * cam::xpt_action is a single logical point that handles both xpt_action and
 * xpt_action_direct. Thie example hooks into it. The style is funky because
 * D doesn't have looping or generalized if constructs.
 *
 * The 'trace' context local variable controls printing of different types
 * of results. This is all controlled by camio.lua.
 */


/*
 * CAM queues a CCB to the SIM in xpt_action. Save interesting bits
 * for later winnowing.
 */
/* fbt::xpt_action_default:entry */
cam::xpt:action
{
	this->ccb = ((union ccb *)arg0);
	this->func = this->ccb->ccb_h.func_code & 0xff;
	this->periph = this->ccb->ccb_h.path->periph;
	this->bus = this->ccb->ccb_h.path->bus;
	this->target = this->ccb->ccb_h.path->target;
	this->device = this->ccb->ccb_h.path->device;
	this->trace = 1;
}

/*
 * Omit the I/O CCBs. Go ahead and pass the other semi I/O enclosure
 * commands, though.
 */
cam::xpt:action
/this->func == XPT_SCSI_IO || this->func == XPT_NVME_IO || this->func == XPT_NVME_ADMIN || this->func == XPT_ATA_IO/
{
	this->trace = 0;
}

/*
 * Print out the non I/O and non ASYNC commands here.
 */
cam::xpt:action
/this->trace && this->func != XPT_ASYNC/
{
	printf("(%s%d:%s%d:%d:%d:%d): %s",
	    this->periph == NULL ? "noperiph" : stringof(this->periph->periph_name),
	    this->periph == NULL ? 0 : this->periph->unit_number,
	    this->bus == NULL ? "nobus" : this->bus->sim->sim_name,
	    this->bus == NULL ? 0 : this->bus->sim->unit_number,
	    this->bus == NULL ? 0 : this->bus->sim->bus_id,
	    this->target == NULL ? 0 : this->target->target_id,
	    this->device == NULL ? 0 : this->device->lun_id,
	    xpt_action_string[this->func]);
}

/*
 * For async calls, print out the async message type.
 */
cam::xpt:action
/this->trace && this->func == XPT_ASYNC/
{
	printf("(%s%d:%s%d:%d:%d:%d): %s %s",
	    this->periph == NULL ? "noperiph" : stringof(this->periph->periph_name),
	    this->periph == NULL ? 0 : this->periph->unit_number,
	    this->bus == NULL ? "nobus" : this->bus->sim->sim_name,
	    this->bus == NULL ? 0 : this->bus->sim->unit_number,
	    this->bus == NULL ? 0 : this->bus->sim->bus_id,
	    this->target == NULL ? 0 : this->target->target_id,
	    this->device == NULL ? 0 : this->device->lun_id,
	    xpt_action_string[this->func],
	    xpt_async_string[this->ccb->casync.async_code]);
}
