/*-
 * Copyright (c) 2002 Adaptec Inc.
 * All rights reserved.
 *
 * Written by: David Jeffery
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
 * $FreeBSD$
 */


#include <dev/ips/ips.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <machine/clock.h>

static d_open_t ips_open;
static d_close_t ips_close;
static d_ioctl_t ips_ioctl;

#define IPS_CDEV_MAJOR 175
static struct cdevsw ips_cdevsw = {
	.d_open = ips_open,
	.d_close = ips_close,
	.d_ioctl = ips_ioctl,
	.d_name = "ips",
	.d_maj = IPS_CDEV_MAJOR,
};


static int ips_open(dev_t dev, int flags, int fmt, struct thread *td)
{
	ips_softc_t *sc = dev->si_drv1;
	sc->state |= IPS_DEV_OPEN;
        return 0;
}

static int ips_close(dev_t dev, int flags, int fmt, struct thread *td)
{
	ips_softc_t *sc = dev->si_drv1;
	sc->state &= ~IPS_DEV_OPEN;

        return 0;
}

static int ips_ioctl(dev_t dev, u_long command, caddr_t addr, int32_t flags, struct thread *td)
{
	ips_softc_t *sc;

	sc = dev->si_drv1;
	return ips_ioctl_request(sc, command, addr, flags);
}

static void ips_cmd_dmaload(void *cmdptr, bus_dma_segment_t *segments,int segnum, int error)
{
	ips_command_t *command = cmdptr;
	PRINTF(10, "ips: in ips_cmd_dmaload\n");
	if(!error)
		command->command_phys_addr = segments[0].ds_addr;

}

/* is locking needed? what locking guarentees are there on removal? */
static __inline__ int ips_cmdqueue_free(ips_softc_t *sc)
{
	int i, error = -1;
	intrmask_t mask = splbio();
	if(!sc->used_commands){
		for(i = 0; i < sc->max_cmds; i++){
			if(!(sc->commandarray[i].command_phys_addr))
				continue;
			bus_dmamap_unload(sc->command_dmatag, 
					  sc->commandarray[i].command_dmamap);
			bus_dmamem_free(sc->command_dmatag, 
					sc->commandarray[i].command_buffer,
					sc->commandarray[i].command_dmamap);
		}
		error = 0;
		sc->state |= IPS_OFFLINE;
	}
	splx(mask);
	return error;
}

/* places all ips command structs on the free command queue.  No locking as if someone else tries
 * to access this during init, we have bigger problems */
static __inline__ int ips_cmdqueue_init(ips_softc_t *sc)
{
	int i;
	ips_command_t *command;
	SLIST_INIT(&sc->free_cmd_list);
	STAILQ_INIT(&sc->cmd_wait_list);
	for(i = 0; i < sc->max_cmds; i++){
		sc->commandarray[i].id = i;
		sc->commandarray[i].sc = sc;
		SLIST_INSERT_HEAD(&sc->free_cmd_list, &sc->commandarray[i], 
				  next);	
	}
	for(i = 0; i < sc->max_cmds; i++){
		command = &sc->commandarray[i];
		if(bus_dmamem_alloc(sc->command_dmatag,&command->command_buffer,
		    BUS_DMA_NOWAIT, &command->command_dmamap))
			goto error;
		bus_dmamap_load(sc->command_dmatag, command->command_dmamap, 
				command->command_buffer,IPS_COMMAND_LEN, 
				ips_cmd_dmaload, command, BUS_DMA_NOWAIT);
		if(!command->command_phys_addr){
			bus_dmamem_free(sc->command_dmatag, 
			    command->command_buffer, command->command_dmamap);
			goto error;
		}
	}
	sc->state &= ~IPS_OFFLINE;
	return 0;
error:
		ips_cmdqueue_free(sc);
		return ENOMEM;
}

static int ips_add_waiting_command(ips_softc_t *sc, int (*callback)(ips_command_t *), void *data, unsigned long flags)
{
	intrmask_t mask;
	ips_command_t *command;
	ips_wait_list_t *waiter;
	unsigned long memflags = 0;
	if(IPS_NOWAIT_FLAG & flags)
		memflags = M_NOWAIT;
	waiter = malloc(sizeof(ips_wait_list_t), M_DEVBUF, memflags);
	if(!waiter)
		return ENOMEM;
	mask = splbio();
	if(sc->state & IPS_OFFLINE){
		splx(mask);
		return EIO;
	}
	command = SLIST_FIRST(&sc->free_cmd_list);
	if(command && !(sc->state & IPS_TIMEOUT)){
		SLIST_REMOVE_HEAD(&sc->free_cmd_list, next);
		(sc->used_commands)++;
		splx(mask);
		clear_ips_command(command);
		bzero(command->command_buffer, IPS_COMMAND_LEN);
		free(waiter, M_DEVBUF);
		command->arg = data;
		return callback(command);
	}
	DEVICE_PRINTF(1, sc->dev, "adding command to the wait queue\n"); 
	waiter->callback = callback; 
	waiter->data = data;
	STAILQ_INSERT_TAIL(&sc->cmd_wait_list, waiter, next);
	splx(mask);
	return 0;
}

static void ips_run_waiting_command(ips_softc_t *sc)
{
	ips_wait_list_t *waiter;
	ips_command_t	*command;
	int (*callback)(ips_command_t*);
	intrmask_t mask;

	mask = splbio();
	waiter = STAILQ_FIRST(&sc->cmd_wait_list);
	command = SLIST_FIRST(&sc->free_cmd_list);
	if(!waiter || !command){
		splx(mask);
		return;
	}
	DEVICE_PRINTF(1, sc->dev, "removing command from wait queue\n");
	SLIST_REMOVE_HEAD(&sc->free_cmd_list, next);
	STAILQ_REMOVE_HEAD(&sc->cmd_wait_list, next);
	(sc->used_commands)++;
	splx(mask);
	clear_ips_command(command);
	bzero(command->command_buffer, IPS_COMMAND_LEN);
	command->arg = waiter->data;
	callback = waiter->callback;
	free(waiter, M_DEVBUF);
	callback(command);
	return;	
}
/* returns a free command struct if one is available. 
 * It also blanks out anything that may be a wild pointer/value.
 * Also, command buffers are not freed.  They are
 * small so they are saved and kept dmamapped and loaded.
 */
int ips_get_free_cmd(ips_softc_t *sc, int (*callback)(ips_command_t *), void *data, unsigned long flags)
{
	intrmask_t mask;
	ips_command_t *command;
	mask = splbio();

	if(sc->state & IPS_OFFLINE){
		splx(mask);
		return EIO;
	}
	command = SLIST_FIRST(&sc->free_cmd_list);
	if(!command || (sc->state & IPS_TIMEOUT)){
		splx(mask);
		if(flags & IPS_NOWAIT_FLAG)
			return EAGAIN;
		return ips_add_waiting_command(sc, callback, data, flags);
	}
	SLIST_REMOVE_HEAD(&sc->free_cmd_list, next);
	(sc->used_commands)++;
	splx(mask);
	clear_ips_command(command);
	bzero(command->command_buffer, IPS_COMMAND_LEN);
	command->arg = data;
	return callback(command);
}

/* adds a command back to the free command queue */
void ips_insert_free_cmd(ips_softc_t *sc, ips_command_t *command)
{
	intrmask_t mask;
	mask = splbio();
	SLIST_INSERT_HEAD(&sc->free_cmd_list, command, next);
	(sc->used_commands)--;
	splx(mask);
	if(!(sc->state & IPS_TIMEOUT))
		ips_run_waiting_command(sc);
}

static int ips_diskdev_init(ips_softc_t *sc)
{
	int i;
	for(i=0; i < IPS_MAX_NUM_DRIVES; i++){
		if(sc->drives[i].state & IPS_LD_OKAY){
			sc->diskdev[i] = device_add_child(sc->dev, NULL, -1);
			device_set_ivars(sc->diskdev[i],(void *)(uintptr_t) i);
		}
	}
	if(bus_generic_attach(sc->dev)){
		device_printf(sc->dev, "Attaching bus failed\n");
	}
	return 0;
}

static int ips_diskdev_free(ips_softc_t *sc)
{
	int i;
	int error = 0;
	for(i = 0; i < IPS_MAX_NUM_DRIVES; i++){
		if(sc->diskdev[i])
			error = device_delete_child(sc->dev, sc->diskdev[i]);
			if(error)
				return error;
	}
	bus_generic_detach(sc->dev);
	return 0;
}

/* ips_timeout is periodically called to make sure no commands sent
 * to the card have become stuck.  If it finds a stuck command, it
 * sets a flag so the driver won't start any more commands and then
 * is periodically called to see if all outstanding commands have
 * either finished or timed out.  Once timed out, an attempt to
 * reinitialize the card is made.  If that fails, the driver gives
 * up and declares the card dead. */
static void ips_timeout(void *arg)
{
	intrmask_t mask;
	ips_softc_t *sc = arg;
	int i, state = 0;
	ips_command_t *command;
	command = &sc->commandarray[0];
	mask = splbio();
	for(i = 0; i < sc->max_cmds; i++){
		if(!command[i].timeout){
			continue;
		}
		command[i].timeout--;
		if(!command[i].timeout){
			if(!(sc->state & IPS_TIMEOUT)){
				sc->state |= IPS_TIMEOUT;
				device_printf(sc->dev, "WARNING: command timeout. Adapter is in toaster mode, resetting to known state\n");
			}
			command[i].status.value = IPS_ERROR_STATUS;
			command[i].callback(&command[i]);
			/* hmm, this should be enough cleanup */
		} else
			state = 1;
	}
	if(!state && (sc->state & IPS_TIMEOUT)){
		if(sc->ips_adapter_reinit(sc, 1)){
			device_printf(sc->dev, "AIEE! adapter reset failed, giving up and going home! Have a nice day.\n");
			sc->state |= IPS_OFFLINE;
			sc->state &= ~IPS_TIMEOUT;
			/* Grr, I hate this solution. I run waiting commands
			   one at a time and error them out just before they 
			   would go to the card. This sucks. */
		} else
			sc->state &= ~IPS_TIMEOUT;
		ips_run_waiting_command(sc);
	}
	if (sc->state != IPS_OFFLINE)
		sc->timer = timeout(ips_timeout, sc, 10*hz);
	splx(mask);
}

/* check card and initialize it */
int ips_adapter_init(ips_softc_t *sc)
{
        int i;
        DEVICE_PRINTF(1,sc->dev, "initializing\n");
        if (bus_dma_tag_create(	/* parent    */	sc->adapter_dmatag,
				/* alignemnt */	1,
				/* boundary  */	0,
				/* lowaddr   */	BUS_SPACE_MAXADDR_32BIT,
				/* highaddr  */	BUS_SPACE_MAXADDR,
				/* filter    */	NULL,
				/* filterarg */	NULL,
				/* maxsize   */	IPS_COMMAND_LEN + 
						    IPS_MAX_SG_LEN,
				/* numsegs   */	1,
				/* maxsegsize*/	IPS_COMMAND_LEN + 
						    IPS_MAX_SG_LEN,
				/* flags     */	0,
				&sc->command_dmatag) != 0) {
                device_printf(sc->dev, "can't alloc command dma tag\n");
		goto error;
        }
	if (bus_dma_tag_create(	/* parent    */	sc->adapter_dmatag,
				/* alignemnt */	1,
				/* boundary  */	0,
				/* lowaddr   */	BUS_SPACE_MAXADDR_32BIT,
				/* highaddr  */	BUS_SPACE_MAXADDR,
				/* filter    */	NULL,
				/* filterarg */	NULL,
				/* maxsize   */	IPS_MAX_IOBUF_SIZE,
				/* numsegs   */	IPS_MAX_SG_ELEMENTS,
				/* maxsegsize*/	IPS_MAX_IOBUF_SIZE,
				/* flags     */	0,
				&sc->sg_dmatag) != 0) {
		device_printf(sc->dev, "can't alloc SG dma tag\n");
		goto error;
	}
	/* create one command buffer until we know how many commands this card
           can handle */
	sc->max_cmds = 1;
	ips_cmdqueue_init(sc);
	callout_handle_init(&sc->timer);

	if(sc->ips_adapter_reinit(sc, 0))
		goto error;

	mtx_init(&sc->cmd_mtx, "ips command mutex", NULL, MTX_DEF);
	if ((i = ips_get_adapter_info(sc)) != 0) {
		device_printf(sc->dev, "failed to get adapter configuration data from device (%d)\n", i);
		goto error;
	}
 	if ((i = ips_get_drive_info(sc)) != 0) {
		device_printf(sc->dev, "failed to get drive configuration data from device (%d)\n", i);
		goto error;
	}
	ips_update_nvram(sc); /* no error check as failure doesn't matter */

        ips_cmdqueue_free(sc);
	if(sc->adapter_info.max_concurrent_cmds)
        	sc->max_cmds = min(128, sc->adapter_info.max_concurrent_cmds);
	else
		sc->max_cmds = 32;
        if(ips_cmdqueue_init(sc)){
		device_printf(sc->dev, "failed to initialize command buffers\n");
		goto error;
	}
        sc->device_file = make_dev(&ips_cdevsw, device_get_unit(sc->dev), UID_ROOT, GID_OPERATOR,
                                        S_IRUSR | S_IWUSR, "ips%d", device_get_unit(sc->dev));
	sc->device_file->si_drv1 = sc;
	ips_diskdev_init(sc);
	sc->timer = timeout(ips_timeout, sc, 10*hz);
        return 0;

error:
	ips_adapter_free(sc);
	return ENXIO;
}

/* see if we should reinitialize the card and wait for it to timeout or complete initialization */
int ips_morpheus_reinit(ips_softc_t *sc, int force)
{
        u_int32_t tmp;
	int i;

	tmp = ips_read_4(sc, MORPHEUS_REG_OISR);
	if(!force && (ips_read_4(sc, MORPHEUS_REG_OMR0) >= IPS_POST1_OK) &&
	    (ips_read_4(sc, MORPHEUS_REG_OMR1) != 0xdeadbeef) && !tmp){
		ips_write_4(sc, MORPHEUS_REG_OIMR, 0);
		return 0;
	}
	ips_write_4(sc, MORPHEUS_REG_OIMR, 0xff);
	ips_read_4(sc, MORPHEUS_REG_OIMR);

	device_printf(sc->dev, "resetting adapter, this may take up to 5 minutes\n");
	ips_write_4(sc, MORPHEUS_REG_IDR, 0x80000000);
	DELAY(5000000);
	pci_read_config(sc->dev, 0, 4);

	tmp = ips_read_4(sc, MORPHEUS_REG_OISR);
	for(i = 0; i < 45 && !(tmp & MORPHEUS_BIT_POST1); i++){
		DELAY(1000000);
		DEVICE_PRINTF(2, sc->dev, "post1: %d\n", i);
		tmp = ips_read_4(sc, MORPHEUS_REG_OISR);
	}
	if(tmp & MORPHEUS_BIT_POST1)
		ips_write_4(sc, MORPHEUS_REG_OISR, MORPHEUS_BIT_POST1);

        if( i == 45 || ips_read_4(sc, MORPHEUS_REG_OMR0) < IPS_POST1_OK){
                device_printf(sc->dev,"Adapter error during initialization.\n");
		return 1;
        }
	for(i = 0; i < 240 && !(tmp & MORPHEUS_BIT_POST2); i++){
		DELAY(1000000);	
		DEVICE_PRINTF(2, sc->dev, "post2: %d\n", i);
		tmp = ips_read_4(sc, MORPHEUS_REG_OISR);
	}
	if(tmp & MORPHEUS_BIT_POST2)
		ips_write_4(sc, MORPHEUS_REG_OISR, MORPHEUS_BIT_POST2);

	if(i == 240 || !ips_read_4(sc, MORPHEUS_REG_OMR1)){
		device_printf(sc->dev, "adapter failed config check\n");
		return 1;
        }
	ips_write_4(sc, MORPHEUS_REG_OIMR, 0);
	if(force && ips_clear_adapter(sc)){
		device_printf(sc->dev, "adapter clear failed\n");
		return 1;
	}
	return 0;
}

/* clean up so we can unload the driver. */
int ips_adapter_free(ips_softc_t *sc)
{
	int error = 0;
	intrmask_t mask;
	if(sc->state & IPS_DEV_OPEN)
		return EBUSY;
	if((error = ips_diskdev_free(sc)))
		return error;
	if(ips_cmdqueue_free(sc)){
		device_printf(sc->dev,
		     "trying to exit when command queue is not empty!\n");
		return EBUSY;
	}
	DEVICE_PRINTF(1, sc->dev, "free\n");
	mask = splbio();
	untimeout(ips_timeout, sc, sc->timer);
	splx(mask);
	if (mtx_initialized(&sc->cmd_mtx))
		mtx_destroy(&sc->cmd_mtx);

	if(sc->sg_dmatag)
		bus_dma_tag_destroy(sc->sg_dmatag);
	if(sc->command_dmatag)
		bus_dma_tag_destroy(sc->command_dmatag);
	if(sc->device_file)
	        destroy_dev(sc->device_file);
        return 0;
}

void ips_morpheus_intr(void *void_sc)
{
        ips_softc_t *sc = (ips_softc_t *)void_sc;
	u_int32_t oisr, iisr;
	int cmdnumber;
	ips_cmd_status_t status;

	iisr =ips_read_4(sc, MORPHEUS_REG_IISR);
	oisr =ips_read_4(sc, MORPHEUS_REG_OISR);
        PRINTF(9,"interrupt registers in:%x out:%x\n",iisr, oisr);
	if(!(oisr & MORPHEUS_BIT_CMD_IRQ)){
		DEVICE_PRINTF(2,sc->dev, "got a non-command irq\n");
		return;	
	}
	while((status.value = ips_read_4(sc, MORPHEUS_REG_OQPR)) != 0xffffffff){
		cmdnumber = status.fields.command_id;
		sc->commandarray[cmdnumber].status.value = status.value;
		sc->commandarray[cmdnumber].timeout = 0;
		sc->commandarray[cmdnumber].callback(&(sc->commandarray[cmdnumber]));
		

		DEVICE_PRINTF(9,sc->dev, "got command %d\n", cmdnumber);
	}
        return;
}

void ips_issue_morpheus_cmd(ips_command_t *command)
{
	intrmask_t mask = splbio();
	/* hmmm, is there a cleaner way to do this? */
	if(command->sc->state & IPS_OFFLINE){
		splx(mask);
		command->status.value = IPS_ERROR_STATUS;
		command->callback(command);
		return;
	}
	command->timeout = 10;
	ips_write_4(command->sc, MORPHEUS_REG_IQPR, command->command_phys_addr);
	splx(mask);
}

static void ips_copperhead_queue_callback(void *queueptr, bus_dma_segment_t *segments,int segnum, int error)
{
	ips_copper_queue_t *queue = queueptr;
	if(error){
		return;
	}
	queue->base_phys_addr = segments[0].ds_addr;
}

static int ips_copperhead_queue_init(ips_softc_t *sc)
{
	int error;
	bus_dma_tag_t dmatag;
	bus_dmamap_t dmamap;
       	if (bus_dma_tag_create(	/* parent    */	sc->adapter_dmatag,
				/* alignemnt */	1,
				/* boundary  */	0,
				/* lowaddr   */	BUS_SPACE_MAXADDR_32BIT,
				/* highaddr  */	BUS_SPACE_MAXADDR,
				/* filter    */	NULL,
				/* filterarg */	NULL,
				/* maxsize   */	sizeof(ips_copper_queue_t),
				/* numsegs   */	1,
				/* maxsegsize*/	sizeof(ips_copper_queue_t),
				/* flags     */	0,
				&dmatag) != 0) {
                device_printf(sc->dev, "can't alloc dma tag for statue queue\n");
		error = ENOMEM;
		goto exit;
        }
	if(bus_dmamem_alloc(dmatag, (void *)&(sc->copper_queue), 
	   		    BUS_DMA_NOWAIT, &dmamap)){
		error = ENOMEM;
		goto exit;
	}
	bzero(sc->copper_queue, sizeof(ips_copper_queue_t));
	sc->copper_queue->dmatag = dmatag;
	sc->copper_queue->dmamap = dmamap;
	sc->copper_queue->nextstatus = 1;
	bus_dmamap_load(dmatag, dmamap, 
			&(sc->copper_queue->status[0]), IPS_MAX_CMD_NUM * 4, 
			ips_copperhead_queue_callback, sc->copper_queue, 
			BUS_DMA_NOWAIT);
	if(sc->copper_queue->base_phys_addr == 0){
		error = ENOMEM;
		goto exit;
	}
	ips_write_4(sc, COPPER_REG_SQSR, sc->copper_queue->base_phys_addr);
	ips_write_4(sc, COPPER_REG_SQER, sc->copper_queue->base_phys_addr + 
		    IPS_MAX_CMD_NUM * 4);
	ips_write_4(sc, COPPER_REG_SQHR, sc->copper_queue->base_phys_addr + 4);
	ips_write_4(sc, COPPER_REG_SQTR, sc->copper_queue->base_phys_addr);

	
	return 0;
exit:
	bus_dmamem_free(dmatag, sc->copper_queue, dmamap);
	bus_dma_tag_destroy(dmatag);
	return error;
}

/* see if we should reinitialize the card and wait for it to timeout or complete initialization FIXME */
int ips_copperhead_reinit(ips_softc_t *sc, int force)
{
	int i, j;
	u_int32_t postcode = 0, configstatus = 0;
	ips_write_1(sc, COPPER_REG_SCPR, 0x80);
	ips_write_1(sc, COPPER_REG_SCPR, 0);
	device_printf(sc->dev, "reinitializing adapter, this could take several minutes.\n");
	for(j = 0; j < 2; j++){
		postcode <<= 8;
		for(i = 0; i < 45; i++){
			if(ips_read_1(sc, COPPER_REG_HISR) & COPPER_GHI_BIT){
				postcode |= ips_read_1(sc, COPPER_REG_ISPR);
				ips_write_1(sc, COPPER_REG_HISR, 
					    COPPER_GHI_BIT);
				break;
			} else
				DELAY(1000000);
		}
		if(i == 45)
			return 1;
	}
	for(j = 0; j < 2; j++){
		configstatus <<= 8;
		for(i = 0; i < 240; i++){
			if(ips_read_1(sc, COPPER_REG_HISR) & COPPER_GHI_BIT){
				configstatus |= ips_read_1(sc, COPPER_REG_ISPR);
				ips_write_1(sc, COPPER_REG_HISR, 
					    COPPER_GHI_BIT);
				break;
			} else
				DELAY(1000000);
		}
		if(i == 240)
			return 1;
	}
	for(i = 0; i < 240; i++){
		if(!(ips_read_1(sc, COPPER_REG_CBSP) & COPPER_OP_BIT)){
			break;
		} else
			DELAY(1000000);
	}
	if(i == 240)
		return 1;
	ips_write_2(sc, COPPER_REG_CCCR, 0x1000 | COPPER_ILE_BIT);
	ips_write_1(sc, COPPER_REG_SCPR, COPPER_EBM_BIT);
	ips_copperhead_queue_init(sc);
	ips_write_1(sc, COPPER_REG_HISR, COPPER_GHI_BIT);
	i = ips_read_1(sc, COPPER_REG_SCPR);
	ips_write_1(sc, COPPER_REG_HISR, COPPER_EI_BIT);
	if(!configstatus){
		device_printf(sc->dev, "adapter initialization failed\n");
		return 1;
	}
	if(force && ips_clear_adapter(sc)){
		device_printf(sc->dev, "adapter clear failed\n");
		return 1;
	}
	return 0;
}
static u_int32_t ips_copperhead_cmd_status(ips_softc_t *sc)
{
	intrmask_t mask;
	u_int32_t value;
	int statnum = sc->copper_queue->nextstatus++;
	if(sc->copper_queue->nextstatus == IPS_MAX_CMD_NUM)
		sc->copper_queue->nextstatus = 0;
	mask = splbio();
	value = sc->copper_queue->status[statnum];
	ips_write_4(sc, COPPER_REG_SQTR, sc->copper_queue->base_phys_addr + 
		    4 * statnum);
	splx(mask);
	return value;
}


void ips_copperhead_intr(void *void_sc)
{
        ips_softc_t *sc = (ips_softc_t *)void_sc;
	int cmdnumber;
	ips_cmd_status_t status;

	while(ips_read_1(sc, COPPER_REG_HISR) & COPPER_SCE_BIT){
		status.value = ips_copperhead_cmd_status(sc);
		cmdnumber = status.fields.command_id;
		sc->commandarray[cmdnumber].status.value = status.value;
		sc->commandarray[cmdnumber].timeout = 0;
		sc->commandarray[cmdnumber].callback(&(sc->commandarray[cmdnumber]));
		PRINTF(9, "ips: got command %d\n", cmdnumber);
	}
        return;
}

void ips_issue_copperhead_cmd(ips_command_t *command)
{
	int i;
	intrmask_t mask = splbio();
	/* hmmm, is there a cleaner way to do this? */
	if(command->sc->state & IPS_OFFLINE){
		splx(mask);
		command->status.value = IPS_ERROR_STATUS;
		command->callback(command);
		return;
	}
	command->timeout = 10;
	for(i = 0; ips_read_4(command->sc, COPPER_REG_CCCR) & COPPER_SEM_BIT;
	    i++ ){
		if( i == 20){
printf("sem bit still set, can't send a command\n");
			splx(mask);
			return;
		}
		DELAY(500);/* need to do a delay here */
	}
	ips_write_4(command->sc, COPPER_REG_CCSAR, command->command_phys_addr);
	ips_write_2(command->sc, COPPER_REG_CCCR, COPPER_CMD_START);
	splx(mask);
}

