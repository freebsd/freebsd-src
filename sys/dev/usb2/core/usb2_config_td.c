/* $FreeBSD$ */
/*-
 * Copyright (c) 2008 Hans Petter Selasky. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <dev/usb2/include/usb2_mfunc.h>

#define	USB_DEBUG_VAR usb2_debug

#include <dev/usb2/core/usb2_core.h>
#include <dev/usb2/core/usb2_process.h>
#include <dev/usb2/core/usb2_config_td.h>
#include <dev/usb2/core/usb2_debug.h>

static void usb2_config_td_sync_cb(struct usb2_config_td_softc *sc, struct usb2_config_td_cc *cc, uint16_t ref);

static void
usb2_config_td_dispatch(struct usb2_proc_msg *pm)
{
	struct usb2_config_td_item *pi = (void *)pm;
	struct usb2_config_td *ctd = pi->p_ctd;

	DPRINTF("\n");

	(pi->command_func) (ctd->p_softc, (void *)(pi + 1), pi->command_ref);

	if (TAILQ_NEXT(pm, pm_qentry) == NULL) {
		/* last command */
		if (ctd->p_end_of_commands) {
			(ctd->p_end_of_commands) (ctd->p_softc);
		}
	}
	return;
}

/*------------------------------------------------------------------------*
 *	usb2_config_td_setup
 *
 * NOTE: the structure pointed to by "ctd" must be zeroed before calling
 * this function!
 *
 * Return values:
 *    0: success
 * Else: failure
 *------------------------------------------------------------------------*/
uint8_t
usb2_config_td_setup(struct usb2_config_td *ctd, void *priv_sc,
    struct mtx *priv_mtx,
    usb2_config_td_end_of_commands_t *p_func_eoc,
    uint16_t item_size, uint16_t item_count)
{
	struct usb2_config_td_item *pi;
	uint16_t n;

	DPRINTF(" size=%u, count=%u \n", item_size, item_count);

	if (item_count >= 256) {
		DPRINTFN(0, "too many items!\n");
		return (1);
	}
	ctd->p_softc = priv_sc;
	ctd->p_end_of_commands = p_func_eoc;
	ctd->msg_count = (2 * item_count);
	ctd->msg_size =
	    (sizeof(struct usb2_config_td_item) + item_size);
	ctd->p_msgs =
	    malloc(ctd->msg_size * ctd->msg_count, M_USBDEV, M_WAITOK | M_ZERO);
	if (ctd->p_msgs == NULL) {
		return (1);
	}
	if (usb2_proc_setup(&ctd->usb2_proc, priv_mtx, USB_PRI_MED)) {
		free(ctd->p_msgs, M_USBDEV);
		ctd->p_msgs = NULL;
		return (1);
	}
	/* initialise messages */
	pi = USB_ADD_BYTES(ctd->p_msgs, 0);
	for (n = 0; n != ctd->msg_count; n++) {
		pi->hdr.pm_callback = &usb2_config_td_dispatch;
		pi->p_ctd = ctd;
		pi = USB_ADD_BYTES(pi, ctd->msg_size);
	}
	return (0);
}

/*------------------------------------------------------------------------*
 *	usb2_config_td_drain
 *
 * This function will tear down an USB config thread, waiting for the
 * currently executing command to return.
 *
 * NOTE: If the structure pointed to by "ctd" is all zero,
 * this function does nothing.
 *------------------------------------------------------------------------*/
void
usb2_config_td_drain(struct usb2_config_td *ctd)
{
	DPRINTF("\n");
	if (ctd->p_msgs) {
		usb2_proc_drain(&ctd->usb2_proc);
	}
	return;
}

/*------------------------------------------------------------------------*
 *	usb2_config_td_unsetup
 *
 * NOTE: If the structure pointed to by "ctd" is all zero,
 * this function does nothing.
 *------------------------------------------------------------------------*/
void
usb2_config_td_unsetup(struct usb2_config_td *ctd)
{
	DPRINTF("\n");

	usb2_config_td_drain(ctd);

	if (ctd->p_msgs) {
		usb2_proc_unsetup(&ctd->usb2_proc);
		free(ctd->p_msgs, M_USBDEV);
		ctd->p_msgs = NULL;
	}
	return;
}

/*------------------------------------------------------------------------*
 *	usb2_config_td_queue_command
 *
 * This function will enter a command into the config thread queue for
 * execution. The "command_sync" field was previously used to indicate
 * the queue count which is now fixed at two elements. If the
 * "command_sync" field is equal to "USB2_CONFIG_TD_SYNC" the command
 * will be executed synchronously from the config thread.  The
 * "command_ref" argument is the reference count for the current
 * command which is passed on to the "command_post_func"
 * function. This parameter can be used to make a command
 * unique. "command_pre_func" is called from this function when we
 * have the final queue element. "command_post_func" is called from
 * the USB config thread when the command reaches the beginning of the
 * USB config thread queue. This function must be called locked.
 *------------------------------------------------------------------------*/
void
usb2_config_td_queue_command(struct usb2_config_td *ctd,
    usb2_config_td_command_t *command_pre_func,
    usb2_config_td_command_t *command_post_func,
    uint16_t command_sync,
    uint16_t command_ref)
{
	struct usb2_config_td_item *pi;
	struct usb2_config_td_item *pi_0;
	struct usb2_config_td_item *pi_1;
	uint16_t n;

	if (usb2_config_td_is_gone(ctd)) {
		DPRINTF("gone\n");
		/* nothing more to do */
		return;
	}
	DPRINTF("\n");

	pi = USB_ADD_BYTES(ctd->p_msgs, 0);
	for (n = 0;; n += 2) {
		if (n == ctd->msg_count) {
			/* should not happen */
			panic("%s:%d: out of memory!\n",
			    __FUNCTION__, __LINE__);
			return;
		}
		if (pi->command_func == NULL) {
			/* reserve our entry */
			pi->command_func = command_post_func;
			pi->command_ref = command_ref;
			pi_0 = pi;
			pi = USB_ADD_BYTES(pi, ctd->msg_size);
			pi->command_func = command_post_func;
			pi->command_ref = command_ref;
			pi_1 = pi;
			break;
		}
		if ((pi->command_func == command_post_func) &&
		    (pi->command_ref == command_ref)) {
			/* found an entry */
			pi_0 = pi;
			pi = USB_ADD_BYTES(pi, ctd->msg_size);
			pi_1 = pi;
			break;
		}
		pi = USB_ADD_BYTES(pi, (2 * ctd->msg_size));
	}

	/*
	 * We have two message structures. One of them will get
	 * queued:
	 */
	pi = usb2_proc_msignal(&ctd->usb2_proc, pi_0, pi_1);

	/*
	 * The job of the post-command function is to finish the command in
	 * a separate context to allow calls to sleeping functions
	 * basically. Queue the post command before calling the pre command.
	 * That way commands queued by the pre command will be queued after
	 * the current command.
	 */

	/*
	 * The job of the pre-command function is to copy the needed
	 * configuration to the provided structure and to execute other
	 * commands that must happen immediately
	 */
	if (command_pre_func) {
		(command_pre_func) (ctd->p_softc, (void *)(pi + 1), command_ref);
	}
	if (command_sync == USB2_CONFIG_TD_SYNC) {
		usb2_proc_mwait(&ctd->usb2_proc, pi_0, pi_1);
	}
	return;
}

/*------------------------------------------------------------------------*
 *	usb2_config_td_is_gone
 *
 * Return values:
 *    0: config thread is running
 * Else: config thread is gone
 *------------------------------------------------------------------------*/
uint8_t
usb2_config_td_is_gone(struct usb2_config_td *ctd)
{
	return (usb2_proc_is_gone(&ctd->usb2_proc));
}

/*------------------------------------------------------------------------*
 *	usb2_config_td_sleep
 *
 * NOTE: this function can only be called from the config thread
 *
 * Return values:
 *    0: normal delay
 * Else: config thread is gone
 *------------------------------------------------------------------------*/
uint8_t
usb2_config_td_sleep(struct usb2_config_td *ctd, uint32_t timeout)
{
	uint8_t is_gone = usb2_config_td_is_gone(ctd);

	if (is_gone) {
		goto done;
	}
	if (timeout == 0) {
		/*
		 * Zero means no timeout, so avoid that by setting
		 * timeout to one:
		 */
		timeout = 1;
	}
	mtx_unlock(ctd->usb2_proc.up_mtx);

	if (pause("USBWAIT", timeout)) {
		/* ignore */
	}
	mtx_lock(ctd->usb2_proc.up_mtx);

	is_gone = usb2_config_td_is_gone(ctd);
done:
	return (is_gone);
}

/*------------------------------------------------------------------------*
 *	usb2_config_td_sync
 *
 * This function will wait until all commands have been executed on
 * the config thread.  This function must be called locked and can
 * sleep.
 *
 * Return values:
 *    0: success
 * Else: config thread is gone
 *------------------------------------------------------------------------*/
uint8_t
usb2_config_td_sync(struct usb2_config_td *ctd)
{
	if (usb2_config_td_is_gone(ctd)) {
		return (1);
	}
	usb2_config_td_queue_command(ctd, NULL,
	    &usb2_config_td_sync_cb, USB2_CONFIG_TD_SYNC, 0);

	if (usb2_config_td_is_gone(ctd)) {
		return (1);
	}
	return (0);
}

static void
usb2_config_td_sync_cb(struct usb2_config_td_softc *sc,
    struct usb2_config_td_cc *cc, uint16_t ref)
{
	return;
}
