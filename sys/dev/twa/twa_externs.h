/*-
 * Copyright (c) 2003-04 3ware, Inc.
 * All rights reserved.
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
 *
 *	$FreeBSD$
 */

/*
 * 3ware driver for 9000 series storage controllers.
 *
 * Author: Vinod Kashyap
 */


/* Global data structures */
extern char			twa_fw_img[];
extern int			twa_fw_img_size;
extern struct twa_message	twa_aen_table[];
extern char			*twa_aen_severity_table[];
extern struct twa_message	twa_error_table[];


/* Functions in twa.c */
extern int	twa_setup(struct twa_softc *sc);		/* do early driver/controller setup */
extern int	twa_deinit_ctlr(struct twa_softc *sc);		/* stop controller */
extern void	twa_interrupt(struct twa_softc *sc);		/* ISR */
extern int	twa_ioctl(struct twa_softc *sc, int cmd, void *addr);/* handle user request */
extern void	twa_enable_interrupts(struct twa_softc *sc);	/* enable controller interrupts */
extern void	twa_disable_interrupts(struct twa_softc *sc);	/* disable controller interrupts */
extern void	twa_complete_io(struct twa_request *tr);	/* I/O completion callback */
extern int	twa_reset(struct twa_softc *sc);		/* (soft) reset controller */
extern int	twa_submit_io(struct twa_request *tr);		/* wrapper to twa_start */
extern int	twa_start(struct twa_request *tr);		/* submit command to controller */
extern char	*twa_find_msg_string(struct twa_message *table, u_int16_t code);/* lookup a msg */
extern struct twa_request *twa_get_request(struct twa_softc *sc);/* get a req pkt from free pool */
extern void	twa_release_request(struct twa_request *tr);	/* put a req pkt back into free pool */
extern void	twa_describe_controller(struct twa_softc *sc);	/* describe controller info */
extern void	twa_print_controller(struct twa_softc *sc);	/* print controller state */

/* Functions in twa_freebsd.c */
extern void	twa_write_pci_config(struct twa_softc *sc, u_int32_t value, int size);/* write to pci config space */
extern int	twa_alloc_req_pkts(struct twa_softc *sc, int num_reqs); /* alloc req & cmd pkts */
extern int	twa_map_request(struct twa_request *tr);	/* copy cmd pkt & data to DMA'able memory */
extern void	twa_unmap_request(struct twa_request *tr);	/* undo mapping */

/* Functions in twa_cam.c */
extern void	twa_request_bus_scan(struct twa_softc *sc);	/* request CAM for a bus scan */
extern int	twa_send_scsi_cmd(struct twa_request *tr, int cmd);/* send down a SCSI cmd */
extern void	twa_scsi_complete(struct twa_request *tr);	/* complete a SCSI cmd by calling CAM */
extern void	twa_drain_busy_queue(struct twa_softc *sc);	/* drain busy queue (during reset) */

extern int	twa_cam_setup(struct twa_softc *sc);		/* attach to CAM */
extern void	twa_cam_detach(struct twa_softc *sc);		/* detach from CAM */
extern void	twa_allow_new_requests(struct twa_softc *sc, void *ccb);/* unfreeze ccb flow from CAM */
extern void	twa_disallow_new_requests(struct twa_softc *sc);/* freeze ccb flow from CAM */
extern void	twa_set_timer(struct twa_request *tr);	/* Set a timer to time a given request */
extern void	twa_unset_timer(struct twa_request *tr);/* Unset a previously set timer */

