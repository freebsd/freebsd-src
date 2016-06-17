/*
 *  acpi_system.c - ACPI System Driver ($Revision: 57 $)
 *
 *  Copyright (C) 2001, 2002 Andy Grover <andrew.grover@intel.com>
 *  Copyright (C) 2001, 2002 Paul Diefenbaugh <paul.s.diefenbaugh@intel.com>
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or (at
 *  your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */

#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/spinlock.h>
#include <linux/poll.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/sysrq.h>
#include <linux/compatmac.h>
#include <linux/proc_fs.h>
#include <linux/pm.h>
#include <asm/uaccess.h>
#include <asm/acpi.h>
#include <acpi/acpi_bus.h>
#include <acpi/acpi_drivers.h>
#include <linux/sched.h>

#ifdef CONFIG_ACPI_SLEEP
#include <linux/mc146818rtc.h>
#include <linux/irq.h>
#include <asm/hw_irq.h>

acpi_status acpi_system_save_state(u32);
#else
static inline acpi_status acpi_system_save_state(u32 state)
{
	return AE_OK;
}
#endif /* !CONFIG_ACPI_SLEEP */

#define _COMPONENT		ACPI_SYSTEM_COMPONENT
ACPI_MODULE_NAME		("acpi_system")

#define PREFIX			"ACPI: "

extern FADT_DESCRIPTOR		acpi_fadt;

static int acpi_system_add (struct acpi_device *device);
static int acpi_system_remove (struct acpi_device *device, int type);

acpi_status acpi_suspend (u32 state);

static struct acpi_driver acpi_system_driver = {
	.name =		ACPI_SYSTEM_DRIVER_NAME,
	.class =	ACPI_SYSTEM_CLASS,
	.ids =		ACPI_SYSTEM_HID,
	.ops =		{
				.add =		acpi_system_add,
				.remove =	acpi_system_remove
			},
};

struct acpi_system
{
	acpi_handle		handle;
	u8			states[ACPI_S_STATE_COUNT];
};

/* Global vars for handling event proc entry */
static spinlock_t		acpi_system_event_lock = SPIN_LOCK_UNLOCKED;
int				event_is_open = 0;
extern struct list_head		acpi_bus_event_list;
extern wait_queue_head_t	acpi_bus_event_queue;

/* --------------------------------------------------------------------------
                                  System Sleep
   -------------------------------------------------------------------------- */

#ifdef CONFIG_PM

static void
acpi_power_off (void)
{
	if (unlikely(in_interrupt())) 
		BUG();
	/* Some SMP machines only can poweroff in boot CPU */
	set_cpus_allowed(current, 1UL << cpu_logical_map(0));
	acpi_system_save_state(ACPI_STATE_S5);
	acpi_enter_sleep_state_prep(ACPI_STATE_S5);
	ACPI_DISABLE_IRQS();
	acpi_enter_sleep_state(ACPI_STATE_S5);

	printk(KERN_EMERG "ACPI: can not power off machine\n");
}

#endif /*CONFIG_PM*/


#ifdef CONFIG_ACPI_SLEEP

/**
 * acpi_system_restore_state - OS-specific restoration of state
 * @state:	sleep state we're exiting
 *
 * Note that if we're coming back from S4, the memory image should have
 * already been loaded from the disk and is already in place.  (Otherwise how
 * else would we be here?).
 */
acpi_status
acpi_system_restore_state(
	u32			state)
{
	/* 
	 * We should only be here if we're coming back from STR or STD.
	 * And, in the case of the latter, the memory image should have already
	 * been loaded from disk.
	 */
	if (state > ACPI_STATE_S1) {
		acpi_restore_state_mem();

		/* Do _early_ resume for irqs.  Required by
		 * ACPI specs.
		 */
		/* TBD: call arch dependant reinitialization of the 
		 * interrupts.
		 */
#ifdef CONFIG_X86
		init_8259A(0);
#endif
		/* wait for power to come back */
		mdelay(1000);

	}

	/* Be really sure that irqs are disabled. */
	ACPI_DISABLE_IRQS();

	/* Wait a little again, just in case... */
	mdelay(1000);

	/* enable interrupts once again */
	ACPI_ENABLE_IRQS();

	/* turn all the devices back on */
	if (state > ACPI_STATE_S1)
		pm_send_all(PM_RESUME, (void *)0);

	return AE_OK;
}


/**
 * acpi_system_save_state - save OS specific state and power down devices
 * @state:	sleep state we're entering.
 *
 * This handles saving all context to memory, and possibly disk.
 * First, we call to the device driver layer to save device state.
 * Once we have that, we save whatevery processor and kernel state we
 * need to memory.
 * If we're entering S4, we then write the memory image to disk.
 *
 * Only then it is safe for us to power down devices, since we may need
 * the disks and upstream buses to write to.
 */
acpi_status
acpi_system_save_state(
	u32			state)
{
	int			error = 0;

	/* Send notification to devices that they will be suspended.
	 * If any device or driver cannot make the transition, either up
	 * or down, we'll get an error back.
	 */
	if (state > ACPI_STATE_S1) {
		error = pm_send_all(PM_SAVE_STATE, (void *)3);
		if (error)
			return AE_ERROR;
	}

	if (state <= ACPI_STATE_S5) {
		/* Tell devices to stop I/O and actually save their state.
		 * It is theoretically possible that something could fail,
		 * so handle that gracefully..
		 */
		if (state > ACPI_STATE_S1 && state != ACPI_STATE_S5) {
			error = pm_send_all(PM_SUSPEND, (void *)3);
			if (error) {
				/* Tell devices to restore state if they have
				 * it saved and to start taking I/O requests.
				 */
				pm_send_all(PM_RESUME, (void *)0);
				return error;
			}
		}
		
		/* flush caches */
		ACPI_FLUSH_CPU_CACHE();

		/* Do arch specific saving of state. */
		if (state > ACPI_STATE_S1) {
			error = acpi_save_state_mem();

			/* TBD: if no s4bios, write codes for
			 * acpi_save_state_disk()...
			 */
#if 0
			if (!error && (state == ACPI_STATE_S4))
				error = acpi_save_state_disk();
#endif
			if (error) {
				pm_send_all(PM_RESUME, (void *)0);
				return error;
			}
		}
	}
	/* disable interrupts
	 * Note that acpi_suspend -- our caller -- will do this once we return.
	 * But, we want it done early, so we don't get any suprises during
	 * the device suspend sequence.
	 */
	ACPI_DISABLE_IRQS();

	/* Unconditionally turn off devices.
	 * Obvious if we enter a sleep state.
	 * If entering S5 (soft off), this should put devices in a
	 * quiescent state.
	 */

	if (state > ACPI_STATE_S1) {
		error = pm_send_all(PM_SUSPEND, (void *)3);

		/* We're pretty screwed if we got an error from this.
		 * We try to recover by simply calling our own restore_state
		 * function; see above for definition.
		 *
		 * If it's S5 though, go through with it anyway..
		 */
		if (error && state != ACPI_STATE_S5)
			acpi_system_restore_state(state);
	}
	return error ? AE_ERROR : AE_OK;
}


/****************************************************************************
 *
 * FUNCTION:    acpi_system_suspend
 *
 * PARAMETERS:  %state: Sleep state to enter.
 *
 * RETURN:      acpi_status, whether or not we successfully entered and
 *              exited sleep.
 *
 * DESCRIPTION: Perform OS-specific action to enter sleep state.
 *              This is the final step in going to sleep, per spec.  If we
 *              know we're coming back (i.e. not entering S5), we save the
 *              processor flags. [ We'll have to save and restore them anyway,
 *              so we use the arch-agnostic save_flags and restore_flags
 *              here.]  We then set the place to return to in arch-specific
 *              globals using arch_set_return_point. Finally, we call the
 *              ACPI function to write the proper values to I/O ports.
 *
 ****************************************************************************/

acpi_status
acpi_system_suspend(
	u32		state)
{
	acpi_status		status = AE_ERROR;
	unsigned long		flags = 0;

	local_irq_save(flags);
	/* kernel_fpu_begin(); */

	switch (state) {
	case ACPI_STATE_S1:
	case ACPI_STATE_S5:
		barrier();
		status = acpi_enter_sleep_state(state);
		break;
	case ACPI_STATE_S4:
		do_suspend_lowlevel_s4bios(0);
		break;
	}

	/* kernel_fpu_end(); */
	local_irq_restore(flags);

	return status;
}



/**
 * acpi_suspend - OS-agnostic system suspend/resume support (S? states)
 * @state:	state we're entering
 *
 */
acpi_status
acpi_suspend (
	u32			state)
{
	acpi_status status;

	/* only support S1 and S5 on kernel 2.4 */
	if (state != ACPI_STATE_S1 && state != ACPI_STATE_S4
	    && state != ACPI_STATE_S5)
		return AE_ERROR;


	if (ACPI_STATE_S4 == state) {
		/* For s4bios, we need a wakeup address. */
		if (1 == acpi_gbl_FACS->S4bios_f &&
		    0 != acpi_gbl_FADT->smi_cmd) {
			if (!acpi_wakeup_address)
				return AE_ERROR;
			acpi_set_firmware_waking_vector((acpi_physical_address) acpi_wakeup_address);
		} else
			/* We don't support S4 under 2.4.  Give up */
			return AE_ERROR;
	}

	status = acpi_system_save_state(state);
	if (!ACPI_SUCCESS(status) && state != ACPI_STATE_S5)
		return status;

	acpi_enter_sleep_state_prep(state);

	/* disable interrupts and flush caches */
	ACPI_DISABLE_IRQS();
	ACPI_FLUSH_CPU_CACHE();

	/* perform OS-specific sleep actions */
	status = acpi_system_suspend(state);

	/* Even if we failed to go to sleep, all of the devices are in an suspended
	 * mode. So, we run these unconditionaly to make sure we have a usable system
	 * no matter what.
	 */
	acpi_leave_sleep_state(state);
	acpi_system_restore_state(state);

	/* make sure interrupts are enabled */
	ACPI_ENABLE_IRQS();

	/* reset firmware waking vector */
	acpi_set_firmware_waking_vector((acpi_physical_address) 0);

	return status;
}

#endif /* CONFIG_ACPI_SLEEP */


/* --------------------------------------------------------------------------
                              FS Interface (/proc)
   -------------------------------------------------------------------------- */

static int
acpi_system_read_info (
	char			*page,
	char			**start,
	off_t			off,
	int 			count,
	int 			*eof,
	void			*data)
{
	struct acpi_system	*system = (struct acpi_system *) data;
	char			*p = page;
	int			size = 0;
	u32			i = 0;

	ACPI_FUNCTION_TRACE("acpi_system_read_info");

	if (!system || (off != 0))
		goto end;

	p += sprintf(p, "version:                 %x\n", ACPI_CA_VERSION);

	p += sprintf(p, "states:                  ");
	for (i=0; i<ACPI_S_STATE_COUNT; i++) {
		if (system->states[i]) {
			p += sprintf(p, "S%d ", i);
			if (i == ACPI_STATE_S4 &&
			    acpi_gbl_FACS->S4bios_f &&
			    0 != acpi_gbl_FADT->smi_cmd)
				p += sprintf(p, "S4Bios ");
		}
	}
	p += sprintf(p, "\n");

end:
	size = (p - page);
	if (size <= off+count) *eof = 1;
	*start = page + off;
	size -= off;
	if (size>count) size = count;
	if (size<0) size = 0;

	return_VALUE(size);
}

static int acpi_system_open_event(struct inode *inode, struct file *file);
static ssize_t acpi_system_read_event (struct file*, char*, size_t, loff_t*);
static int acpi_system_close_event(struct inode *inode, struct file *file);
static unsigned int acpi_system_poll_event(struct file *file, poll_table *wait);


static struct file_operations acpi_system_event_ops = {
	.open =		acpi_system_open_event,
	.read =		acpi_system_read_event,
	.release =	acpi_system_close_event,
	.poll =		acpi_system_poll_event,
};

static int
acpi_system_open_event(struct inode *inode, struct file *file)
{
	spin_lock_irq (&acpi_system_event_lock);

	if(event_is_open)
		goto out_busy;

	event_is_open = 1;

	spin_unlock_irq (&acpi_system_event_lock);
	return 0;

out_busy:
	spin_unlock_irq (&acpi_system_event_lock);
	return -EBUSY;
}

static ssize_t
acpi_system_read_event (
	struct file		*file,
	char			*buffer,
	size_t			count,
	loff_t			*ppos)
{
	int			result = 0;
	struct acpi_bus_event	event;
	static char		str[ACPI_MAX_STRING];
	static int		chars_remaining = 0;
	static char		*ptr;


	ACPI_FUNCTION_TRACE("acpi_system_read_event");

	if (!chars_remaining) {
		memset(&event, 0, sizeof(struct acpi_bus_event));

		if ((file->f_flags & O_NONBLOCK)
		    && (list_empty(&acpi_bus_event_list)))
			return_VALUE(-EAGAIN);

		result = acpi_bus_receive_event(&event);
		if (result) {
			return_VALUE(-EIO);
		}

		chars_remaining = sprintf(str, "%s %s %08x %08x\n", 
			event.device_class?event.device_class:"<unknown>",
			event.bus_id?event.bus_id:"<unknown>", 
			event.type, event.data);
		ptr = str;
	}

	if (chars_remaining < count) {
		count = chars_remaining;
	}

	if (copy_to_user(buffer, ptr, count))
		return_VALUE(-EFAULT);

	*ppos += count;
	chars_remaining -= count;
	ptr += count;

	return_VALUE(count);
}

static int
acpi_system_close_event(struct inode *inode, struct file *file)
{
	spin_lock_irq (&acpi_system_event_lock);
	event_is_open = 0;
	spin_unlock_irq (&acpi_system_event_lock);
	return 0;
}

static unsigned int
acpi_system_poll_event(
	struct file		*file,
	poll_table		*wait)
{
	poll_wait(file, &acpi_bus_event_queue, wait);
	if (!list_empty(&acpi_bus_event_list))
		return POLLIN | POLLRDNORM;
	return 0;
}

static ssize_t acpi_system_read_dsdt (struct file*, char*, size_t, loff_t*);

static struct file_operations acpi_system_dsdt_ops = {
	.read =			acpi_system_read_dsdt,
};

static ssize_t
acpi_system_read_dsdt (
	struct file		*file,
	char			*buffer,
	size_t			count,
	loff_t			*ppos)
{
	acpi_status		status = AE_OK;
	struct acpi_buffer	dsdt = {ACPI_ALLOCATE_BUFFER, NULL};
	void			*data = 0;
	size_t			size = 0;

	ACPI_FUNCTION_TRACE("acpi_system_read_dsdt");

	status = acpi_get_table(ACPI_TABLE_DSDT, 1, &dsdt);
	if (ACPI_FAILURE(status))
		return_VALUE(-ENODEV);

	if (*ppos < dsdt.length) {
		data = dsdt.pointer + file->f_pos;
		size = dsdt.length - file->f_pos;
		if (size > count)
			size = count;
		if (copy_to_user(buffer, data, size)) {
			acpi_os_free(dsdt.pointer);
			return_VALUE(-EFAULT);
		}
	}

	acpi_os_free(dsdt.pointer);

	*ppos += size;

	return_VALUE(size);
}


static ssize_t acpi_system_read_fadt (struct file*, char*, size_t, loff_t*);

static struct file_operations acpi_system_fadt_ops = {
	.read =			acpi_system_read_fadt,
};

static ssize_t
acpi_system_read_fadt (
	struct file		*file,
	char			*buffer,
	size_t			count,
	loff_t			*ppos)
{
	acpi_status		status = AE_OK;
	struct acpi_buffer	fadt = {ACPI_ALLOCATE_BUFFER, NULL};
	void			*data = 0;
	size_t			size = 0;

	ACPI_FUNCTION_TRACE("acpi_system_read_fadt");

	status = acpi_get_table(ACPI_TABLE_FADT, 1, &fadt);
	if (ACPI_FAILURE(status))
		return_VALUE(-ENODEV);

	if (*ppos < fadt.length) {
		data = fadt.pointer + file->f_pos;
		size = fadt.length - file->f_pos;
		if (size > count)
			size = count;
		if (copy_to_user(buffer, data, size)) {
			acpi_os_free(fadt.pointer);
			return_VALUE(-EFAULT);
		}
	}

	acpi_os_free(fadt.pointer);

	*ppos += size;

	return_VALUE(size);
}


#ifdef ACPI_DEBUG_OUTPUT

static int
acpi_system_read_debug (
	char			*page,
	char			**start,
	off_t			off,
	int 			count,
	int 			*eof,
	void			*data)
{
	char			*p = page;
	int 			size = 0;

	if (off != 0)
		goto end;

	switch ((unsigned long) data) {
	case 0:
		p += sprintf(p, "0x%08x\n", acpi_dbg_layer);
		break;
	case 1:
		p += sprintf(p, "0x%08x\n", acpi_dbg_level);
		break;
	default:
		p += sprintf(p, "Invalid debug option\n");
		break;
	}
	
end:
	size = (p - page);
	if (size <= off+count) *eof = 1;
	*start = page + off;
	size -= off;
	if (size>count) size = count;
	if (size<0) size = 0;

	return size;
}


static int
acpi_system_write_debug (
	struct file             *file,
        const char              *buffer,
	unsigned long           count,
        void                    *data)
{
	char			debug_string[12] = {'\0'};

	ACPI_FUNCTION_TRACE("acpi_system_write_debug");

	if (count > sizeof(debug_string) - 1)
		return_VALUE(-EINVAL);

	if (copy_from_user(debug_string, buffer, count))
		return_VALUE(-EFAULT);

	debug_string[count] = '\0';

	switch ((unsigned long) data) {
	case 0:
		acpi_dbg_layer = simple_strtoul(debug_string, NULL, 0);
		break;
	case 1:
		acpi_dbg_level = simple_strtoul(debug_string, NULL, 0);
		break;
	default:
		return_VALUE(-EINVAL);
	}

	return_VALUE(count);
}

#endif /* ACPI_DEBUG_OUTPUT */


#ifdef CONFIG_ACPI_SLEEP

static int
acpi_system_read_sleep (
        char                    *page,
        char                    **start,
        off_t                   off,
        int                     count,
        int                     *eof,
        void                    *data)
{
	struct acpi_system	*system = (struct acpi_system *) data;
	char			*p = page;
	int			size;
	int			i;

	ACPI_FUNCTION_TRACE("acpi_system_read_sleep");

	if (!system || (off != 0))
		goto end;

	for (i = 0; i <= ACPI_STATE_S5; i++) {
		if (system->states[i]) {
			p += sprintf(p,"S%d ", i);
			if (i == ACPI_STATE_S4 && acpi_gbl_FACS->S4bios_f &&
			    acpi_gbl_FADT->smi_cmd != 0)
				p += sprintf(p, "S4Bios ");
		}
	}

	p += sprintf(p, "\n");

end:
	size = (p - page);
	if (size <= off+count) *eof = 1;
	*start = page + off;
	size -= off;
	if (size>count) size = count;
	if (size<0) size = 0;

	return_VALUE(size);
}


static int
acpi_system_write_sleep (
	struct file		*file,
	const char		*buffer,
	unsigned long		count,
	void			*data)
{
	acpi_status		status = AE_OK;
	struct acpi_system	*system = (struct acpi_system *) data;
	char			state_string[12] = {'\0'};
	u32			state = 0;

	ACPI_FUNCTION_TRACE("acpi_system_write_sleep");

	if (!system || (count > sizeof(state_string) - 1))
		return_VALUE(-EINVAL);

	if (copy_from_user(state_string, buffer, count))
		return_VALUE(-EFAULT);
	
	state_string[count] = '\0';
	
	state = simple_strtoul(state_string, NULL, 0);
	
	if (!system->states[state])
		return_VALUE(-ENODEV);

	/*
	 * If S4 is supported by the OS, then we should assume that
	 * echo 4b > /proc/acpi/sleep is for s4bios.
	 * Since we have only s4bios, we assume that acpi_suspend failed
	 * if no s4bios support.
	 */
	status = acpi_suspend(state);
	if (ACPI_FAILURE(status))
		return_VALUE(-ENODEV);
	
	return_VALUE(count);
}


static int
acpi_system_read_alarm (
	char                    *page,
	char                    **start,
	off_t                   off,
	int                     count,
	int                     *eof,
	void                    *context)
{
	char			*p = page;
	int			size = 0;
	u32			sec, min, hr;
	u32			day, mo, yr;

	ACPI_FUNCTION_TRACE("acpi_system_read_alarm");

	if (off != 0)
		goto end;

	spin_lock(&rtc_lock);

	sec = CMOS_READ(RTC_SECONDS_ALARM);
	min = CMOS_READ(RTC_MINUTES_ALARM);
	hr = CMOS_READ(RTC_HOURS_ALARM);

#if 0	/* If we ever get an FACP with proper values... */
	if (acpi_gbl_FADT->day_alrm)
		day = CMOS_READ(acpi_gbl_FADT->day_alrm);
	else
		day =  CMOS_READ(RTC_DAY_OF_MONTH);
	if (acpi_gbl_FADT->mon_alrm)
		mo = CMOS_READ(acpi_gbl_FADT->mon_alrm);
	else
		mo = CMOS_READ(RTC_MONTH);;
	if (acpi_gbl_FADT->century)
		yr = CMOS_READ(acpi_gbl_FADT->century) * 100 + CMOS_READ(RTC_YEAR);
	else
		yr = CMOS_READ(RTC_YEAR);
#else
	day = CMOS_READ(RTC_DAY_OF_MONTH);
	mo = CMOS_READ(RTC_MONTH);
	yr = CMOS_READ(RTC_YEAR);
#endif

	spin_unlock(&rtc_lock);

	BCD_TO_BIN(sec);
	BCD_TO_BIN(min);
	BCD_TO_BIN(hr);
	BCD_TO_BIN(day);
	BCD_TO_BIN(mo);
	BCD_TO_BIN(yr);

#if 0
	/* we're trusting the FADT (see above)*/
#else
	/* If we're not trusting the FADT, we should at least make it
	 * right for _this_ century... ehm, what is _this_ century?
	 *
	 * TBD:
	 *  ASAP: find piece of code in the kernel, e.g. star tracker driver,
	 *        which we can trust to determine the century correctly. Atom
	 *        watch driver would be nice, too...
	 *
	 *  if that has not happened, change for first release in 2050:
 	 *        if (yr<50)
	 *                yr += 2100;
	 *        else
	 *                yr += 2000;   // current line of code
	 *
	 *  if that has not happened either, please do on 2099/12/31:23:59:59
	 *        s/2000/2100
	 *
	 */
	yr += 2000;
#endif

	p += sprintf(p,"%4.4u-", yr);
	p += (mo > 12)  ? sprintf(p, "**-")  : sprintf(p, "%2.2u-", mo);
	p += (day > 31) ? sprintf(p, "** ")  : sprintf(p, "%2.2u ", day);
	p += (hr > 23)  ? sprintf(p, "**:")  : sprintf(p, "%2.2u:", hr);
	p += (min > 59) ? sprintf(p, "**:")  : sprintf(p, "%2.2u:", min);
	p += (sec > 59) ? sprintf(p, "**\n") : sprintf(p, "%2.2u\n", sec);

 end:
	size = p - page;
	if (size < count) *eof = 1;
	else if (size > count) size = count;
	if (size < 0) size = 0;
	*start = page;

	return_VALUE(size);
}


static int
get_date_field (
	char			**p,
	u32			*value)
{
	char			*next = NULL;
	char			*string_end = NULL;
	int			result = -EINVAL;

	/*
	 * Try to find delimeter, only to insert null.  The end of the
	 * string won't have one, but is still valid.
	 */
	next = strpbrk(*p, "- :");
	if (next)
		*next++ = '\0';

	*value = simple_strtoul(*p, &string_end, 10);

	/* Signal success if we got a good digit */
	if (string_end != *p)
		result = 0;

	if (next)
		*p = next;

	return result;
}


static int
acpi_system_write_alarm (
	struct file		*file,
	const char		*buffer,
	unsigned long		count,
	void			*data)
{
	int			result = 0;
	char			alarm_string[30] = {'\0'};
	char			*p = alarm_string;
	u32			sec, min, hr, day, mo, yr;
	int			adjust = 0;
	unsigned char		rtc_control = 0;

	ACPI_FUNCTION_TRACE("acpi_system_write_alarm");

	if (count > sizeof(alarm_string) - 1)
		return_VALUE(-EINVAL);
	
	if (copy_from_user(alarm_string, buffer, count))
		return_VALUE(-EFAULT);

	alarm_string[count] = '\0';

	/* check for time adjustment */
	if (alarm_string[0] == '+') {
		p++;
		adjust = 1;
	}

	if ((result = get_date_field(&p, &yr)))
		goto end;
	if ((result = get_date_field(&p, &mo)))
		goto end;
	if ((result = get_date_field(&p, &day)))
		goto end;
	if ((result = get_date_field(&p, &hr)))
		goto end;
	if ((result = get_date_field(&p, &min)))
		goto end;
	if ((result = get_date_field(&p, &sec)))
		goto end;

	if (sec > 59) {
		min += 1;
		sec -= 60;
	}
	if (min > 59) {
		hr += 1;
		min -= 60;
	}
	if (hr > 23) {
		day += 1;
		hr -= 24;
	}
	if (day > 31) {
		mo += 1;
		day -= 31;
	}
	if (mo > 12) {
		yr += 1;
		mo -= 12;
	}

	spin_lock_irq(&rtc_lock);

	rtc_control = CMOS_READ(RTC_CONTROL);
	if (!(rtc_control & RTC_DM_BINARY) || RTC_ALWAYS_BCD) {
		BIN_TO_BCD(yr);
		BIN_TO_BCD(mo);
		BIN_TO_BCD(day);
		BIN_TO_BCD(hr);
		BIN_TO_BCD(min);
		BIN_TO_BCD(sec);
	}

	if (adjust) {
		yr  += CMOS_READ(RTC_YEAR);
		mo  += CMOS_READ(RTC_MONTH);
		day += CMOS_READ(RTC_DAY_OF_MONTH);
		hr  += CMOS_READ(RTC_HOURS);
		min += CMOS_READ(RTC_MINUTES);
		sec += CMOS_READ(RTC_SECONDS);
	}

	spin_unlock_irq(&rtc_lock);

	if (!(rtc_control & RTC_DM_BINARY) || RTC_ALWAYS_BCD) {
		BCD_TO_BIN(yr);
		BCD_TO_BIN(mo);
		BCD_TO_BIN(day);
		BCD_TO_BIN(hr);
		BCD_TO_BIN(min);
		BCD_TO_BIN(sec);
	}

	if (sec > 59) {
		min++;
		sec -= 60;
	}
	if (min > 59) {
		hr++;
		min -= 60;
	}
	if (hr > 23) {
		day++;
		hr -= 24;
	}
	if (day > 31) {
		mo++;
		day -= 31;
	}
	if (mo > 12) {
		yr++;
		mo -= 12;
	}
	if (!(rtc_control & RTC_DM_BINARY) || RTC_ALWAYS_BCD) {
		BIN_TO_BCD(yr);
		BIN_TO_BCD(mo);
		BIN_TO_BCD(day);
		BIN_TO_BCD(hr);
		BIN_TO_BCD(min);
		BIN_TO_BCD(sec);
	}

	spin_lock_irq(&rtc_lock);

	/* write the fields the rtc knows about */
	CMOS_WRITE(hr, RTC_HOURS_ALARM);
	CMOS_WRITE(min, RTC_MINUTES_ALARM);
	CMOS_WRITE(sec, RTC_SECONDS_ALARM);

	/*
	 * If the system supports an enhanced alarm it will have non-zero
	 * offsets into the CMOS RAM here -- which for some reason are pointing
	 * to the RTC area of memory.
	 */
#if 0
	if (acpi_gbl_FADT->day_alrm)
		CMOS_WRITE(day, acpi_gbl_FADT->day_alrm);
	if (acpi_gbl_FADT->mon_alrm)
		CMOS_WRITE(mo, acpi_gbl_FADT->mon_alrm);
	if (acpi_gbl_FADT->century)
		CMOS_WRITE(yr/100, acpi_gbl_FADT->century);
#endif
	/* enable the rtc alarm interrupt */
	if (!(rtc_control & RTC_AIE)) {
		rtc_control |= RTC_AIE;
		CMOS_WRITE(rtc_control,RTC_CONTROL);
		CMOS_READ(RTC_INTR_FLAGS);
	}

	spin_unlock_irq(&rtc_lock);

	acpi_set_register(ACPI_BITREG_RT_CLOCK_ENABLE, 1, ACPI_MTX_LOCK);

	file->f_pos += count;

	result = 0;
end:
	return_VALUE(result ? result : count);
}

#endif /*CONFIG_ACPI_SLEEP*/


static int
acpi_system_add_fs (
	struct acpi_device	*device)
{
	struct proc_dir_entry	*entry = NULL;

	ACPI_FUNCTION_TRACE("acpi_system_add_fs");

	if (!device)
		return_VALUE(-EINVAL);

	/* 'info' [R] */
	entry = create_proc_entry(ACPI_SYSTEM_FILE_INFO,
		S_IRUGO, acpi_device_dir(device));
	if (!entry)
		ACPI_DEBUG_PRINT((ACPI_DB_ERROR,
			"Unable to create '%s' fs entry\n",
			ACPI_SYSTEM_FILE_INFO));
	else {
		entry->read_proc = acpi_system_read_info;
		entry->data = acpi_driver_data(device);
	}

	/* 'dsdt' [R] */
	entry = create_proc_entry(ACPI_SYSTEM_FILE_DSDT,
		S_IRUSR, acpi_device_dir(device));
	if (!entry)
		ACPI_DEBUG_PRINT((ACPI_DB_ERROR,
			"Unable to create '%s' fs entry\n",
			ACPI_SYSTEM_FILE_DSDT));
	else
		entry->proc_fops = &acpi_system_dsdt_ops;

	/* 'fadt' [R] */
	entry = create_proc_entry(ACPI_SYSTEM_FILE_FADT,
		S_IRUSR, acpi_device_dir(device));
	if (!entry)
		ACPI_DEBUG_PRINT((ACPI_DB_ERROR,
			"Unable to create '%s' fs entry\n",
			ACPI_SYSTEM_FILE_FADT));
	else
		entry->proc_fops = &acpi_system_fadt_ops;

	/* 'event' [R] */
	entry = create_proc_entry(ACPI_SYSTEM_FILE_EVENT,
		S_IRUSR, acpi_device_dir(device));
	if (!entry)
		ACPI_DEBUG_PRINT((ACPI_DB_ERROR,
			"Unable to create '%s' fs entry\n",
			ACPI_SYSTEM_FILE_EVENT));
	else
		entry->proc_fops = &acpi_system_event_ops;

#ifdef CONFIG_ACPI_SLEEP

	/* 'sleep' [R/W]*/
	entry = create_proc_entry(ACPI_SYSTEM_FILE_SLEEP,
		S_IFREG|S_IRUGO|S_IWUSR, acpi_device_dir(device));
	if (!entry)
		ACPI_DEBUG_PRINT((ACPI_DB_ERROR,
			"Unable to create '%s' fs entry\n",
			ACPI_SYSTEM_FILE_SLEEP));
	else {
		entry->read_proc = acpi_system_read_sleep;
		entry->write_proc = acpi_system_write_sleep;
		entry->data = acpi_driver_data(device);
	}

	/* 'alarm' [R/W] */
	entry = create_proc_entry(ACPI_SYSTEM_FILE_ALARM,
		S_IFREG|S_IRUGO|S_IWUSR, acpi_device_dir(device));
	if (!entry)
		ACPI_DEBUG_PRINT((ACPI_DB_ERROR,
			"Unable to create '%s' fs entry\n",
			ACPI_SYSTEM_FILE_ALARM));
	else {
		entry->read_proc = acpi_system_read_alarm;
		entry->write_proc = acpi_system_write_alarm;
		entry->data = acpi_driver_data(device);
	}

#endif /*CONFIG_ACPI_SLEEP*/

#ifdef ACPI_DEBUG_OUTPUT

	/* 'debug_layer' [R/W] */
	entry = create_proc_entry(ACPI_SYSTEM_FILE_DEBUG_LAYER,
		S_IFREG|S_IRUGO|S_IWUSR, acpi_device_dir(device));
	if (!entry)
		ACPI_DEBUG_PRINT((ACPI_DB_ERROR,
			"Unable to create '%s' fs entry\n",
			ACPI_SYSTEM_FILE_DEBUG_LAYER));
	else {
		entry->read_proc  = acpi_system_read_debug;
		entry->write_proc = acpi_system_write_debug;
		entry->data = (void *) 0;
	}

	/* 'debug_level' [R/W] */
	entry = create_proc_entry(ACPI_SYSTEM_FILE_DEBUG_LEVEL,
		S_IFREG|S_IRUGO|S_IWUSR, acpi_device_dir(device));
	if (!entry)
		ACPI_DEBUG_PRINT((ACPI_DB_ERROR,
			"Unable to create '%s' fs entry\n",
			ACPI_SYSTEM_FILE_DEBUG_LEVEL));
	else {
		entry->read_proc  = acpi_system_read_debug;
		entry->write_proc = acpi_system_write_debug;
		entry->data = (void *) 1;
	}

#endif /*ACPI_DEBUG_OUTPUT */

	return_VALUE(0);
}


static int
acpi_system_remove_fs (
	struct acpi_device	*device)
{
	ACPI_FUNCTION_TRACE("acpi_system_remove_fs");

	if (!device)
		return_VALUE(-EINVAL);

	remove_proc_entry(ACPI_SYSTEM_FILE_INFO, acpi_device_dir(device));
	remove_proc_entry(ACPI_SYSTEM_FILE_DSDT, acpi_device_dir(device));
	remove_proc_entry(ACPI_SYSTEM_FILE_EVENT, acpi_device_dir(device));
#ifdef CONFIG_ACPI_SLEEP
	remove_proc_entry(ACPI_SYSTEM_FILE_SLEEP, acpi_device_dir(device));
	remove_proc_entry(ACPI_SYSTEM_FILE_ALARM, acpi_device_dir(device));
#endif
#ifdef ACPI_DEBUG_OUTPUT
	remove_proc_entry(ACPI_SYSTEM_FILE_DEBUG_LAYER,
		acpi_device_dir(device));
	remove_proc_entry(ACPI_SYSTEM_FILE_DEBUG_LEVEL,
		acpi_device_dir(device));
#endif

	return_VALUE(0);
}


/* --------------------------------------------------------------------------
                                 Driver Interface
   -------------------------------------------------------------------------- */

#if defined(CONFIG_MAGIC_SYSRQ) && defined(CONFIG_PM)

static int po_cb_active; 

static void acpi_po_tramp(void *x)
{ 
	acpi_power_off();	
} 

/* Simple wrapper calling power down function. */
static void acpi_sysrq_power_off(int key, struct pt_regs *pt_regs,
	struct kbd_struct *kbd, struct tty_struct *tty)
{	
	static struct tq_struct tq = { .routine = acpi_po_tramp };
	if (po_cb_active++)
		return;
	schedule_task(&tq); 
}

struct sysrq_key_op sysrq_acpi_poweroff_op = {
	.handler =	&acpi_sysrq_power_off,
	.help_msg =	"Off",
	.action_msg =	"Power Off\n"
};

#endif  /* CONFIG_MAGIC_SYSRQ */

static int
acpi_system_add (
	struct acpi_device	*device)
{
	int			result = 0;
	acpi_status		status = AE_OK;
	struct acpi_system	*system = NULL;
	u8			i = 0;

	ACPI_FUNCTION_TRACE("acpi_system_add");

	if (!device)
		return_VALUE(-EINVAL);

	system = kmalloc(sizeof(struct acpi_system), GFP_KERNEL);
	if (!system)
		return_VALUE(-ENOMEM);
	memset(system, 0, sizeof(struct acpi_system));

	system->handle = device->handle;
	sprintf(acpi_device_name(device), "%s", ACPI_SYSTEM_DEVICE_NAME);
	sprintf(acpi_device_class(device), "%s", ACPI_SYSTEM_CLASS);
	acpi_driver_data(device) = system;

	result = acpi_system_add_fs(device);
	if (result)
		goto end;

	printk(KERN_INFO PREFIX "%s [%s] (supports", 
		acpi_device_name(device), acpi_device_bid(device));
	for (i=0; i<ACPI_S_STATE_COUNT; i++) {
		u8 type_a, type_b;
		status = acpi_get_sleep_type_data(i, &type_a, &type_b);
		switch (i) {
		case ACPI_STATE_S4:
			if (acpi_gbl_FACS->S4bios_f &&
			    0 != acpi_gbl_FADT->smi_cmd) {
				printk(" S4bios");
				system->states[i] = 1;
			}
			/* no break */
		default: 
			if (ACPI_SUCCESS(status)) {
				system->states[i] = 1;
				printk(" S%d", i);
			}
		}
	}
	printk(")\n");

#ifdef CONFIG_PM
	/* Install the soft-off (S5) handler. */
	if (system->states[ACPI_STATE_S5]) {
		pm_power_off = acpi_power_off;
		register_sysrq_key('o', &sysrq_acpi_poweroff_op);
	}
#endif

end:
	if (result)
		kfree(system);

	return_VALUE(result);
}


static int
acpi_system_remove (
	struct acpi_device	*device,
	int			type)
{
	struct acpi_system	*system = NULL;

	ACPI_FUNCTION_TRACE("acpi_system_remove");

	if (!device || !acpi_driver_data(device))
		return_VALUE(-EINVAL);

	system = (struct acpi_system *) acpi_driver_data(device);

#ifdef CONFIG_PM
	/* Remove the soft-off (S5) handler. */
	if (system->states[ACPI_STATE_S5]) {
		unregister_sysrq_key('o', &sysrq_acpi_poweroff_op);
		pm_power_off = NULL;
	}
#endif

	acpi_system_remove_fs(device);

	kfree(system);

	return 0;
}


int __init
acpi_system_init (void)
{
	int			result = 0;

	ACPI_FUNCTION_TRACE("acpi_system_init");

	result = acpi_bus_register_driver(&acpi_system_driver);
	if (result < 0)
		return_VALUE(-ENODEV);

	return_VALUE(0);
}


void __exit
acpi_system_exit (void)
{
	ACPI_FUNCTION_TRACE("acpi_system_exit");
	acpi_bus_unregister_driver(&acpi_system_driver);
	return_VOID;
}
