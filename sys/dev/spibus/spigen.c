/*-
 * Copyright (c) 2015 Brian Fundakowski Feldman.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_platform.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mman.h>
#include <sys/mutex.h>
#include <sys/module.h>
#include <sys/proc.h>
#include <sys/rwlock.h>
#include <sys/spigenio.h>
#include <sys/sysctl.h>
#include <sys/types.h>
 
#include <vm/vm.h>
#include <vm/vm_extern.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_pager.h>

#include <dev/spibus/spi.h>
#include <dev/spibus/spibusvar.h>

#ifdef FDT
#include <dev/ofw/ofw_bus_subr.h>
#endif

#include "spibus_if.h"

#define	SPIGEN_OPEN		(1 << 0)
#define	SPIGEN_MMAP_BUSY	(1 << 1)

struct spigen_softc {
	device_t sc_dev;
	struct cdev *sc_cdev;
	struct mtx sc_mtx;
	uint32_t sc_command_length_max; /* cannot change while mmapped */
	uint32_t sc_data_length_max;    /* cannot change while mmapped */
	vm_object_t sc_mmap_buffer;     /* command, then data */
	vm_offset_t sc_mmap_kvaddr;
	size_t sc_mmap_buffer_size;
	int sc_debug;
	int sc_flags;
};

static int
spigen_probe(device_t dev)
{
	int rv;

#ifdef FDT
	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (!ofw_bus_is_compatible(dev, "freebsd,spigen"))
		return (ENXIO);
	rv = BUS_PROBE_DEFAULT;
#else
	rv = BUS_PROBE_NOWILDCARD;
#endif
	device_set_desc(dev, "SPI Generic IO");

	return (rv);
}

static int spigen_open(struct cdev *, int, int, struct thread *);
static int spigen_ioctl(struct cdev *, u_long, caddr_t, int, struct thread *);
static int spigen_close(struct cdev *, int, int, struct thread *);
static d_mmap_single_t spigen_mmap_single;

static struct cdevsw spigen_cdevsw = {
	.d_version =     D_VERSION,
	.d_name =        "spigen",
	.d_open =        spigen_open,
	.d_ioctl =       spigen_ioctl,
	.d_mmap_single = spigen_mmap_single,
	.d_close =       spigen_close
};

static int
spigen_command_length_max_proc(SYSCTL_HANDLER_ARGS)
{
	struct spigen_softc *sc = (struct spigen_softc *)arg1;
	uint32_t command_length_max;
	int error;

	mtx_lock(&sc->sc_mtx);
	command_length_max = sc->sc_command_length_max;
	mtx_unlock(&sc->sc_mtx);
	error = sysctl_handle_int(oidp, &command_length_max,
	    sizeof(command_length_max), req);
	if (error == 0 && req->newptr != NULL) {
		mtx_lock(&sc->sc_mtx);
		if (sc->sc_mmap_buffer != NULL)
			error = EBUSY;
		else
			sc->sc_command_length_max = command_length_max;
		mtx_unlock(&sc->sc_mtx);
	}
	return (error);
}

static int
spigen_data_length_max_proc(SYSCTL_HANDLER_ARGS)
{
	struct spigen_softc *sc = (struct spigen_softc *)arg1;
	uint32_t data_length_max;
	int error;

	mtx_lock(&sc->sc_mtx);
	data_length_max = sc->sc_data_length_max;
	mtx_unlock(&sc->sc_mtx);
	error = sysctl_handle_int(oidp, &data_length_max,
	    sizeof(data_length_max), req);
	if (error == 0 && req->newptr != NULL) {
		mtx_lock(&sc->sc_mtx);
		if (sc->sc_mmap_buffer != NULL)
			error = EBUSY;
		else
			sc->sc_data_length_max = data_length_max;
		mtx_unlock(&sc->sc_mtx);
	}
	return (error);
}

static void
spigen_sysctl_init(struct spigen_softc *sc)
{
	struct sysctl_ctx_list *ctx;
	struct sysctl_oid *tree_node;
	struct sysctl_oid_list *tree;

	/*
	 * Add system sysctl tree/handlers.
	 */
	ctx = device_get_sysctl_ctx(sc->sc_dev);
	tree_node = device_get_sysctl_tree(sc->sc_dev);
	tree = SYSCTL_CHILDREN(tree_node);
	SYSCTL_ADD_PROC(ctx, tree, OID_AUTO, "command_length_max",
	    CTLFLAG_MPSAFE | CTLFLAG_RW | CTLTYPE_UINT, sc, sizeof(*sc),
	    spigen_command_length_max_proc, "IU", "SPI command header portion (octets)");
	SYSCTL_ADD_PROC(ctx, tree, OID_AUTO, "data_length_max",
	    CTLFLAG_MPSAFE | CTLFLAG_RW | CTLTYPE_UINT, sc, sizeof(*sc),
	    spigen_data_length_max_proc, "IU", "SPI data trailer portion (octets)");
	SYSCTL_ADD_INT(ctx, tree, OID_AUTO, "data", CTLFLAG_RW,
	    &sc->sc_debug, 0, "debug flags");

}

static int
spigen_attach(device_t dev)
{
	struct spigen_softc *sc;
	const int unit = device_get_unit(dev);

	sc = device_get_softc(dev);
	sc->sc_dev = dev;
	sc->sc_cdev = make_dev(&spigen_cdevsw, unit,
	    UID_ROOT, GID_OPERATOR, 0660, "spigen%d", unit);
	sc->sc_cdev->si_drv1 = dev;
	sc->sc_command_length_max = PAGE_SIZE;
	sc->sc_data_length_max = PAGE_SIZE;
	mtx_init(&sc->sc_mtx, device_get_nameunit(dev), NULL, MTX_DEF);
	spigen_sysctl_init(sc);

	return (0);
}

static int 
spigen_open(struct cdev *cdev, int oflags, int devtype, struct thread *td)
{
	int error;
	device_t dev;
	struct spigen_softc *sc;

	error = 0;
	dev = cdev->si_drv1;
	sc = device_get_softc(dev);

	mtx_lock(&sc->sc_mtx);
	if (sc->sc_flags & SPIGEN_OPEN)
		error = EBUSY;
	else
		sc->sc_flags |= SPIGEN_OPEN;
	mtx_unlock(&sc->sc_mtx);

	return (error);
}

static int
spigen_transfer(struct cdev *cdev, struct spigen_transfer *st)
{
	struct spi_command transfer = SPI_COMMAND_INITIALIZER;
	device_t dev = cdev->si_drv1;
	struct spigen_softc *sc = device_get_softc(dev);
	int error = 0;

	mtx_lock(&sc->sc_mtx);
	if (st->st_command.iov_len == 0)
		error = EINVAL;
	else if (st->st_command.iov_len > sc->sc_command_length_max ||
	    st->st_data.iov_len > sc->sc_data_length_max)
		error = ENOMEM;
	mtx_unlock(&sc->sc_mtx);
	if (error)
		return (error);
	
#if 0
	device_printf(dev, "cmd %p %u data %p %u\n", st->st_command.iov_base,
	    st->st_command.iov_len, st->st_data.iov_base, st->st_data.iov_len);
#endif
	transfer.tx_cmd = transfer.rx_cmd = malloc(st->st_command.iov_len,
	    M_DEVBUF, M_WAITOK);
	if (transfer.tx_cmd == NULL)
		return (ENOMEM);
	if (st->st_data.iov_len > 0) {
		transfer.tx_data = transfer.rx_data = malloc(st->st_data.iov_len,
		    M_DEVBUF, M_WAITOK);
		if (transfer.tx_data == NULL) {
			free(transfer.tx_cmd, M_DEVBUF);
			return (ENOMEM);
		}
	}
	else
		transfer.tx_data = transfer.rx_data = NULL;

	error = copyin(st->st_command.iov_base, transfer.tx_cmd,
	    transfer.tx_cmd_sz = transfer.rx_cmd_sz = st->st_command.iov_len);	
	if ((error == 0) && (st->st_data.iov_len > 0))
		error = copyin(st->st_data.iov_base, transfer.tx_data,
		    transfer.tx_data_sz = transfer.rx_data_sz =
		                          st->st_data.iov_len);	
	if (error == 0)
		error = SPIBUS_TRANSFER(device_get_parent(dev), dev, &transfer);
	if (error == 0) {
		error = copyout(transfer.rx_cmd, st->st_command.iov_base,
		    transfer.rx_cmd_sz);
		if ((error == 0) && (st->st_data.iov_len > 0))
			error = copyout(transfer.rx_data, st->st_data.iov_base,
			    transfer.rx_data_sz);
	}

	free(transfer.tx_cmd, M_DEVBUF);
	free(transfer.tx_data, M_DEVBUF);
	return (error);
}

static int
spigen_transfer_mmapped(struct cdev *cdev, struct spigen_transfer_mmapped *stm)
{
	struct spi_command transfer = SPI_COMMAND_INITIALIZER;
	device_t dev = cdev->si_drv1;
	struct spigen_softc *sc = device_get_softc(dev);
	int error = 0;

	mtx_lock(&sc->sc_mtx);
	if (sc->sc_flags & SPIGEN_MMAP_BUSY)
		error = EBUSY;
	else if (stm->stm_command_length > sc->sc_command_length_max ||
	    stm->stm_data_length > sc->sc_data_length_max)
		error = E2BIG;
	else if (sc->sc_mmap_buffer == NULL)
		error = EINVAL;
	else if (sc->sc_mmap_buffer_size <
	    stm->stm_command_length + stm->stm_data_length)
		error = ENOMEM;
	if (error == 0)
		sc->sc_flags |= SPIGEN_MMAP_BUSY;
	mtx_unlock(&sc->sc_mtx);
	if (error)
		return (error);
	
	transfer.tx_cmd = transfer.rx_cmd = (void *)sc->sc_mmap_kvaddr;
	transfer.tx_cmd_sz = transfer.rx_cmd_sz = stm->stm_command_length;
	transfer.tx_data = transfer.rx_data =
	    (void *)(sc->sc_mmap_kvaddr + stm->stm_command_length);
	transfer.tx_data_sz = transfer.rx_data_sz = stm->stm_data_length;
	error = SPIBUS_TRANSFER(device_get_parent(dev), dev, &transfer);

	mtx_lock(&sc->sc_mtx);
	KASSERT((sc->sc_flags & SPIGEN_MMAP_BUSY), ("mmap no longer marked busy"));
	sc->sc_flags &= ~(SPIGEN_MMAP_BUSY);
	mtx_unlock(&sc->sc_mtx);
	return (error);
}

static int
spigen_ioctl(struct cdev *cdev, u_long cmd, caddr_t data, int fflag,
    struct thread *td)
{
	device_t dev = cdev->si_drv1;
	int error;

	switch (cmd) {
	case SPIGENIOC_TRANSFER:
		error = spigen_transfer(cdev, (struct spigen_transfer *)data);
		break;
	case SPIGENIOC_TRANSFER_MMAPPED:
		error = spigen_transfer_mmapped(cdev, (struct spigen_transfer_mmapped *)data);
		break;
	case SPIGENIOC_GET_CLOCK_SPEED:
		error = spibus_get_clock(dev, (uintptr_t *)data);
		break;
	case SPIGENIOC_SET_CLOCK_SPEED:
		error = spibus_set_clock(dev, *(uint32_t *)data);
		break;
	case SPIGENIOC_GET_SPI_MODE:
		error = spibus_get_mode(dev, (uintptr_t *)data);
		break;
	case SPIGENIOC_SET_SPI_MODE:
		error = spibus_set_mode(dev, *(uint32_t *)data);
		break;
	default:
		error = ENOTTY;
		break;
	}
	return (error);
}

static int
spigen_mmap_single(struct cdev *cdev, vm_ooffset_t *offset,
    vm_size_t size, struct vm_object **object, int nprot)
{
	device_t dev = cdev->si_drv1;
	struct spigen_softc *sc = device_get_softc(dev);
	vm_page_t *m;
	size_t n, pages;

	if (size == 0 ||
	    (nprot & (PROT_EXEC | PROT_READ | PROT_WRITE))
	    != (PROT_READ | PROT_WRITE))
		return (EINVAL);
	size = roundup2(size, PAGE_SIZE);
	pages = size / PAGE_SIZE;

	mtx_lock(&sc->sc_mtx);
	if (sc->sc_mmap_buffer != NULL) {
		mtx_unlock(&sc->sc_mtx);
		return (EBUSY);
	} else if (size > sc->sc_command_length_max + sc->sc_data_length_max) {
		mtx_unlock(&sc->sc_mtx);
		return (E2BIG);
	}
	sc->sc_mmap_buffer_size = size;
	*offset = 0;
	sc->sc_mmap_buffer = *object = vm_pager_allocate(OBJT_PHYS, 0, size,
	    nprot, *offset, curthread->td_ucred);
	m = malloc(sizeof(*m) * pages, M_TEMP, M_WAITOK);
	VM_OBJECT_WLOCK(*object);
	vm_object_reference_locked(*object); // kernel and userland both
	for (n = 0; n < pages; n++) {
		m[n] = vm_page_grab(*object, n,
		    VM_ALLOC_NOBUSY | VM_ALLOC_ZERO | VM_ALLOC_WIRED);
		m[n]->valid = VM_PAGE_BITS_ALL;
	}
	VM_OBJECT_WUNLOCK(*object);
	sc->sc_mmap_kvaddr = kva_alloc(size);
	pmap_qenter(sc->sc_mmap_kvaddr, m, pages);
	free(m, M_TEMP);
	mtx_unlock(&sc->sc_mtx);

	if (*object == NULL)
		 return (EINVAL);
	return (0);
}

static int 
spigen_close(struct cdev *cdev, int fflag, int devtype, struct thread *td)
{
	device_t dev = cdev->si_drv1;
	struct spigen_softc *sc = device_get_softc(dev);

	mtx_lock(&sc->sc_mtx);
	if (sc->sc_mmap_buffer != NULL) {
		pmap_qremove(sc->sc_mmap_kvaddr,
		    sc->sc_mmap_buffer_size / PAGE_SIZE);
		kva_free(sc->sc_mmap_kvaddr, sc->sc_mmap_buffer_size);
		sc->sc_mmap_kvaddr = 0;
		vm_object_deallocate(sc->sc_mmap_buffer);
		sc->sc_mmap_buffer = NULL;
		sc->sc_mmap_buffer_size = 0;
	}
	sc->sc_flags &= ~(SPIGEN_OPEN);
	mtx_unlock(&sc->sc_mtx);
	return (0);
}

static int
spigen_detach(device_t dev)
{
	struct spigen_softc *sc;

	sc = device_get_softc(dev);

	mtx_lock(&sc->sc_mtx);
	if (sc->sc_flags & SPIGEN_OPEN) {
		mtx_unlock(&sc->sc_mtx);
		return (EBUSY);
	}
	mtx_unlock(&sc->sc_mtx);

	mtx_destroy(&sc->sc_mtx);

        if (sc->sc_cdev)
                destroy_dev(sc->sc_cdev);
	
	return (0);
}

static devclass_t spigen_devclass;

static device_method_t spigen_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		spigen_probe),
	DEVMETHOD(device_attach,	spigen_attach),
	DEVMETHOD(device_detach,	spigen_detach),

	{ 0, 0 }
};

static driver_t spigen_driver = {
	"spigen",
	spigen_methods,
	sizeof(struct spigen_softc),
};

DRIVER_MODULE(spigen, spibus, spigen_driver, spigen_devclass, 0, 0);
MODULE_DEPEND(spigen, spibus, 1, 1, 1);
