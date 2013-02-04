/*-
 * Copyright (c) 2012 Robert N. M. Watson
 * Copyright (c) 2012 SRI International
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract (FA8750-10-C-0237)
 * ("CTSRD"), as part of the DARPA CRASH research programme.
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/module.h>
#include <sys/kernel.h>
#include <sys/systm.h>

#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/endian.h>
#include <sys/bio.h>
#include <sys/kthread.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/rman.h>
#include <sys/taskqueue.h>

#include <machine/bus.h>
#include <machine/resource.h>

#include <geom/geom_disk.h>

#include <dev/isf/isf.h>

/* Read Mode */
#define	ISF_CMD_RA	0xFF	/* Read Array mode */
#define	ISF_CMD_RSR	0x70	/* Read Status Register mode */
#define	ISF_CMD_RDI	0x90	/* Read Device ID/Config Reg mode */
#define	ISF_CMD_RQ	0x98	/* Read Query mode */
#define	ISF_CMD_CSR	0x50	/* Clear Status Register */

/* Write Mode */
#define	ISF_CMD_WPS	0x40	/* Word Program Setup */
#define ISF_CMD_BPS	0xE8	/* Buffered Program Setup */
#define	ISF_CMD_BPC	0xD0	/* Buffered Program Confirm */

/* Erase Mode */
#define	ISF_CMD_BES	0x20	/* Block Erase Setup */
#define ISF_CMD_BEC	0xD0	/* Block Erase Confirm */

/* Block Locking/Unlocking */
#define	ISF_CMD_LBS	0x60	/* Lock Block Setup */
#define	ISF_CMD_LB	0x01	/* Lock Block */
#define	ISF_CMD_UB	0xD0	/* Unlock Block */

/*
 * Read Device Identifier registers.
 *
 * NOTE: ISF_RDIR_BLC is relative to the block base address.
 */
#define	ISF_REG_MC	0x00	/* Manufacture Code */
#define ISF_REG_ID	0x01	/* Device ID Code */
#define	ISF_REG_BLC	0x02	/* Block Lock Configuration */
#define ISF_REG_RCR	0x05	/* Read Configuration Register */

/*
 * Protection Registers
 */
#define ISF_REG_L0	0x80	/* Lock Register 0 */
#define ISF_REG_FPP	0x81	/* 64-bit Factory Protection Register */
#define ISF_REG_UPP	0x85	/* 64-bit User Protection Register */
#define ISF_REG_L1	0x89	/* Lock Register 1 */
#define ISF_REG_PP1	0x8A	/* 128-bit Protection Register 1 */
#define ISF_REG_PP2	0x92	/* 128-bit Protection Register 2 */
#define ISF_REG_PP3	0x9A	/* 128-bit Protection Register 3 */
#define ISF_REG_PP4	0xA2	/* 128-bit Protection Register 4 */
#define ISF_REG_PP5	0xAA	/* 128-bit Protection Register 5 */
#define ISF_REG_PP6	0xB2	/* 128-bit Protection Register 6 */
#define ISF_REG_PP7	0xBA	/* 128-bit Protection Register 7 */
#define ISF_REG_PP8	0xC2	/* 128-bit Protection Register 8 */
#define ISF_REG_PP9	0xCA	/* 128-bit Protection Register 9 */
#define ISF_REG_PP10	0xD2	/* 128-bit Protection Register 10 */
#define ISF_REG_PP11	0xDA	/* 128-bit Protection Register 11 */
#define ISF_REG_PP12	0xE2	/* 128-bit Protection Register 12 */
#define ISF_REG_PP13	0xEA	/* 128-bit Protection Register 13 */
#define ISF_REG_PP14	0xF2	/* 128-bit Protection Register 14 */
#define ISF_REG_PP15	0xFA	/* 128-bit Protection Register 15 */
#define ISF_REG_PP16	0x102	/* 128-bit Protection Register 16 */

#define	ISF_SR_BWS	(1 << 0)	/* BEFP Status */
#define	ISF_SR_BLS	(1 << 1)	/* Block-Locked Status */
#define ISF_SR_PSS	(1 << 2)	/* Program Suspend Status */
#define	ISF_SR_VPPS	(1 << 3)	/* Vpp Status */
#define	ISF_SR_PS	(1 << 4)	/* Program Status */
#define ISF_SR_ES	(1 << 5)	/* Erase Status */
#define ISF_SR_ESS	(1 << 6)	/* Erase Suspend Status */
#define ISF_SR_DWS	(1 << 7)	/* Device Write Status */
#define ISF_SR_FSC_MASK	(ISF_SR_VPPS | ISF_SR_PS | ISF_SR_BLS)

#define ISF_BUFFER_PROGRAM

MALLOC_DEFINE(M_ISF, "isf_data", "Intel StrateFlash driver");
static int	isf_debug = 0;

static struct isf_chips {
	uint16_t	 chip_id;
	size_t		 chip_size;
	const char	*chip_desc;
} chip_ids[] = {
	{ 0x8817, 0x0800000, "64-Mbit Top Parameter" },
	{ 0x881A, 0x0800000, "64-Mbit Bottom Parameter" },
	{ 0x8818, 0x1000000, "128-Mbit Top Parameter" },
	{ 0x881B, 0x1000000, "128-Mbit Bottom Parameter" },
	{ 0x8919, 0x2000000, "256-Mbit Top Parameter" },
	{ 0x891C, 0x2000000, "256-Mbit Bottom Parameter" },
	{ 0x8961, 0x2000000, "512-Mbit package (half)" },
	{ 0x0000, 0x0000000, NULL }
};

static void	isf_task(void *arg);

/* 
 * Device driver for the Intel StrataFlash NOR flash device.  This
 * implementation is known to work with 256Mb instances of the device, but may
 * also work with other 64/128/512Mb parts without much work.  Multiple
 * device instances should be used when multiple parts are in the same
 * physical package, due to variable block size support in the StrataFlash
 * part.
 */
devclass_t	isf_devclass;

static uint16_t
isf_read_reg(struct isf_softc *sc, uint16_t reg)
{

	if (isf_debug)
		device_printf(sc->isf_dev, "isf_read_reg(0x%02x)\n", reg);
	return (le16toh(bus_read_2(sc->isf_res, reg * 2)));
}

static uint64_t
isf_read_reg64(struct isf_softc *sc, uint16_t reg)
{
	uint64_t val;
	uint16_t *val16 = (uint16_t *)&val;

	if (isf_debug)
		device_printf(sc->isf_dev, "isf_read_reg64(0x%02x)\n", reg);
	val16[0] = bus_read_2(sc->isf_res, reg * 2);
	val16[1] = bus_read_2(sc->isf_res, (reg+1) * 2);
	val16[2] = bus_read_2(sc->isf_res, (reg+2) * 2);
	val16[3] = bus_read_2(sc->isf_res, (reg+3) * 2);

	return(le64toh(val));
}

static uint16_t
isf_read_off(struct isf_softc *sc, off_t off)
{

	KASSERT(off >= 0, ("%s: negative offset\n", __func__));
	KASSERT(off < sc->isf_disk->d_mediasize,
	    ("%s: offset out side address space 0x%08jx \n", __func__,
	    (intmax_t)off));

	if (isf_debug)
		device_printf(sc->isf_dev, "isf_read_off(0x%08jx)\n",
		    (intmax_t)off);
	return (le16toh(bus_read_2(sc->isf_res, off)));
}

static void
isf_write_cmd(struct isf_softc *sc, off_t off, uint16_t cmd)
{
	
	if (isf_debug)
		device_printf(sc->isf_dev, "isf_write_cmd(0x%08jx, 0x%02x)\n",
		    off, cmd);
	bus_write_2(sc->isf_res, off, htole16(cmd));
}

static uint16_t
isf_read_status(struct isf_softc *sc, off_t off)
{
	
	isf_write_cmd(sc, off/2, ISF_CMD_RSR);
	return isf_read_off(sc, off);
}

static void
isf_clear_status(struct isf_softc *sc)
{
	
	isf_write_cmd(sc, 0, ISF_CMD_CSR);
}

static int
isf_full_status_check(struct isf_softc *sc, off_t off)
{
	int		error = 0;
	uint16_t	status;
	
	status = isf_read_status(sc, off);
	if (status & ISF_SR_VPPS) {
		device_printf(sc->isf_dev, "Vpp Range Error\n");
		error = EIO;
	} else if (status & ISF_SR_PS) {
		device_printf(sc->isf_dev, "Program Error\n");
		error = EIO;
	} else if (status & ISF_SR_BLS) {
		device_printf(sc->isf_dev, "Device Protect Error\n");
		error = EIO;
	}
	isf_clear_status(sc);

	return(error);
}

static int
isf_full_erase_status_check(struct isf_softc *sc, off_t off)
{
	int		error = 0;
	uint16_t	status;
	
	status = isf_read_status(sc, off);
	if (status & ISF_SR_VPPS) {
		device_printf(sc->isf_dev, "Vpp Range Error\n");
		error = EIO;
	} else if (status & (ISF_SR_PS|ISF_SR_ES)) {
		device_printf(sc->isf_dev, "Command Sequence Error\n");
		error = EIO;
	} else if (status & ISF_SR_ES) {
		device_printf(sc->isf_dev, "Block Erase Error\n");
		error = EIO;
	} else if (status & ISF_SR_BLS) {
		device_printf(sc->isf_dev, "Block Locked Error\n");
		error = EIO;
	}
	isf_clear_status(sc);

	return(error);
}

static void
isf_unlock_block(struct isf_softc *sc, off_t off)
{

	isf_write_cmd(sc, off, ISF_CMD_LBS);
	isf_write_cmd(sc, off, ISF_CMD_UB);
	isf_write_cmd(sc, off, ISF_CMD_RA);
}

static void
isf_lock_block(struct isf_softc *sc, off_t off)
{

	isf_write_cmd(sc, off, ISF_CMD_LBS);
	isf_write_cmd(sc, off, ISF_CMD_LB);
	isf_write_cmd(sc, off, ISF_CMD_RA);
}

static void
isf_read(struct isf_softc *sc, off_t off, void *data, size_t len)
{

	KASSERT((uintptr_t)data % 2 == 0,
	    ("%s: unaligned data %p", __func__, data));
	KASSERT((len <= ISF_SECTORSIZE) && (len % 2 == 0),
	    ("%s: invalid length %ju", __func__, len));
	KASSERT(off % ISF_SECTORSIZE == 0,
	    ("%s: invalid offset %ju\n", __func__, off));

	/*
	 * It is not permitted to read blocks that are in the process of
	 * being erased, but we know they will be all 1's after the
	 * erase so just report that value if asked about a block that
	 * is being erased.
	 */
	if (sc->isf_bstate[off / ISF_ERASE_BLOCK] == BS_ERASING)
		memset(data, 0xFF, len);
	else
		bus_read_region_2(sc->isf_res, off, (uint16_t *)data, len / 2);
}

static int
isf_write(struct isf_softc *sc, off_t off, void *data, size_t len)
{
	int		 cycles, error = 0;
	uint16_t	*dp;
	uint16_t	 status;
	off_t		 coff;

	KASSERT((uintptr_t)data % 2 == 0,
	    ("%s: unaligned data %p", __func__, data));
	KASSERT((len <= ISF_SECTORSIZE) && (len % 2 == 0),
	    ("%s: invalid length %ju", __func__, len));
	KASSERT(off % ISF_SECTORSIZE == 0,
	    ("%s: invalid offset %ju\n", __func__, off));
	KASSERT(!sc->isf_erasing,
	    ("%s: trying to write while erasing\n", __func__));
	KASSERT(sc->isf_bstate[off / ISF_ERASE_BLOCK] != BS_ERASING,
	    ("%s: block being erased at %ju\n", __func__, off));

	isf_unlock_block(sc, off);

#ifdef ISF_BUFFER_PROGRAM
	for (dp = data, coff = off; dp - (uint16_t *)data < len / 2;
	    dp += 32, coff += 64) {
		isf_clear_status(sc);
		isf_write_cmd(sc, coff, ISF_CMD_BPS);
		cycles = 0xFFFF;
		while ( !(isf_read_off(sc, coff) & ISF_SR_DWS) ) {
			if (cycles-- == 0) {
				device_printf(sc->isf_dev, "timeout waiting"
				    " for write to start at 0x08%jx\n",
				    (intmax_t)coff);
				return (EIO);
			}
			isf_write_cmd(sc, coff, ISF_CMD_BPS);
		}

		/* When writing N blocks, send N-1 as the count */
		isf_write_cmd(sc, coff, 31);
		bus_write_region_2(sc->isf_res, coff, dp, 32);

		isf_write_cmd(sc, coff, ISF_CMD_BPC);

		status = isf_read_off(sc, coff);
		cycles = 0xFFFFF;
		while ( !(status & ISF_SR_DWS) ) {
			if (cycles-- == 0) {
				device_printf(sc->isf_dev, "timeout waiting"
				    " for write to complete at 0x08%jx\n",
				    (intmax_t)coff);
				error = EIO;
				break;
			}
			status = isf_read_off(sc, coff);
		}
		isf_full_status_check(sc, off);

		isf_write_cmd(sc, coff, ISF_CMD_RA);
	}
#else
	for (dp = data, coff = off; dp - (uint16_t *)data < len / 2;
	    dp++, coff += 2) {
		isf_write_cmd(sc, coff, ISF_CMD_WPS);
		bus_write_2(sc->isf_res, coff, *dp);
		status = isf_read_off(sc, coff);
		cycles=0xFFFFF;
		while ( !(status & ISF_SR_DWS) ) {
			if (cycles-- == 0) {
				device_printf(sc->isf_dev, "timeout waiting"
				    " for write to complete at 0x08%jx\n",
				    (intmax_t)coff);
				error = EIO;
				break;
			}
			status = isf_read_off(sc, coff);
		}

	}
	isf_full_status_check(sc, off);
	isf_write_cmd(sc, coff, ISF_CMD_RA);
#endif

	isf_lock_block(sc, off);

	return error;
}

static void
isf_erase_at(struct isf_softc *sc, off_t off)
{
	int		cycles;
	uint16_t	status;

	isf_unlock_block(sc, off);
	isf_clear_status(sc);

	isf_write_cmd(sc, off, ISF_CMD_BES);
	isf_write_cmd(sc, off, ISF_CMD_BEC);

	cycles=0xFFFFFF;
	status = isf_read_off(sc, off);
	while ( !(status & ISF_SR_DWS) ) {
#ifdef NOTYET
		ISF_SLEEP(sc, sc, hz);
#endif
		if (cycles-- == 0) {
			device_printf(sc->isf_dev,
			    "Giving up on erase\n");
			break;
		}
		status = isf_read_off(sc, off);
	}

	isf_full_erase_status_check(sc, off);

	isf_lock_block(sc, off);

	isf_write_cmd(sc, off, ISF_CMD_RA);
}

static void
isf_erase_range(struct isf_softc *sc, off_t blk_off, size_t size)
{
	off_t		off;
	off_t		ms = sc->isf_disk->d_mediasize;

	KASSERT(blk_off % ISF_ERASE_BLOCK == 0,
	    ("%s: invalid offset %ju\n", __func__, blk_off));

	ISF_LOCK_ASSERT(sc);

	for (off = blk_off; off < blk_off + size; off += ISF_ERASE_BLOCK) {
		sc->isf_bstate[off / ISF_ERASE_BLOCK] = BS_ERASING;

		/*
		 * The first or last 128K is four blocks depending which
		 * part this is.  For now, just assume both are and
		 * erase four times.
		 */
		if (off == 0 || ms - off == ISF_ERASE_BLOCK) {
			isf_erase_at(sc, off);
			isf_erase_at(sc, off + 0x08000);
			isf_erase_at(sc, off + 0x10000);
			isf_erase_at(sc, off + 0x18000);
		} else
			isf_erase_at(sc, off);

		sc->isf_bstate[off / ISF_ERASE_BLOCK] = BS_STEADY;
	}
}

/*
 * disk(9) methods.
 */
static int
isf_disk_ioctl(struct disk *disk, u_long cmd, void *data, int fflag,
    struct thread *td)
{
	int			error = 0;
	struct isf_softc	*sc = disk->d_drv1;
	struct isf_range	*ir;

	switch (cmd) {
	case ISF_ERASE:
		ir = data;
		if (ir->ir_off % ISF_ERASE_BLOCK != 0 ||
		    ir->ir_off >= disk->d_mediasize ||
		    ir->ir_size == 0 ||
		    ir->ir_size % ISF_ERASE_BLOCK != 0 ||
		    ir->ir_off + ir->ir_size > disk->d_mediasize) {
			error = EINVAL;
			break;
		}
		ISF_LOCK(sc);
		if (sc->isf_erasing) {
			ISF_UNLOCK(sc);
			error = EBUSY;
			break;
		}
		sc->isf_erasing = 1;
		isf_erase_range(sc, ir->ir_off, ir->ir_size);
		sc->isf_erasing = 0;
		ISF_UNLOCK(sc);
		break;
	default:
		error = EINVAL;
	}

	return (error);
}

static void
isf_disk_strategy(struct bio *bp)
{
	struct isf_softc *sc = bp->bio_disk->d_drv1;

	/*
	 * We advertise a block size and maximum I/O size up the stack; catch
	 * any attempts to not follow the rules.
	 */
	KASSERT(bp->bio_bcount == ISF_SECTORSIZE,
	    ("%s: I/O size not %d", __func__, ISF_SECTORSIZE));

	ISF_LOCK(sc);
	bioq_disksort(&sc->isf_bioq, bp);
	ISF_WAKEUP(sc);
	ISF_UNLOCK(sc);
}

static void
isf_task(void *arg)
{
	struct isf_softc	*sc = arg;
	struct bio		*bp;
	int			ss = sc->isf_disk->d_sectorsize;
	int			error, i;

	for (;;) {
		ISF_LOCK(sc);
		do {
			bp = bioq_first(&sc->isf_bioq);
			if (bp == NULL) {
				if (sc->isf_doomed)
					kproc_exit(0);
				else
					ISF_SLEEP(sc, sc, 0);
			}
		} while (bp == NULL);
		bioq_remove(&sc->isf_bioq, bp);

		error = 0;
		switch (bp->bio_cmd) {
		case BIO_READ:
			isf_read(sc, bp->bio_pblkno * ss, bp->bio_data,
			    bp->bio_bcount);
			break;

		case BIO_WRITE:
			/*
			 * In principle one could suspend the in-progress
			 * erase, process any pending writes to other
			 * blocks and then proceed, but that seems
			 * overly complex for the likely usage modes.
			 */
			if (sc->isf_erasing) {
				error = EBUSY;
				break;
			}

			/*
			 * Read in the block we want to write and check that
			 * we're only setting bits to 0.  If an erase would
			 * be required return an I/O error.
			 */
			isf_read(sc, bp->bio_pblkno * ss, sc->isf_rbuf,
			    bp->bio_bcount);
			for (i = 0; i < bp->bio_bcount / 2; i++)
				if ((sc->isf_rbuf[i] &
				    ((uint16_t *)bp->bio_data)[i]) !=
				    ((uint16_t *)bp->bio_data)[i]) {
					device_printf(sc->isf_dev, "write"
					    " requires erase at 0x%08jx\n",
					    bp->bio_pblkno * ss);
					error = EIO;
					break;
				}
			if (error != 0)
				break;

			error = isf_write(sc, bp->bio_pblkno * ss,
			    bp->bio_data, bp->bio_bcount);
			break;

		default:
			panic("%s: unsupported I/O operation %d", __func__,
			    bp->bio_cmd);
		}
		if (error == 0)
			biodone(bp);
		else
			biofinish(bp, NULL, error);
		ISF_UNLOCK(sc);
	}
}

static void
isf_dump_info(struct isf_softc *sc)
{
	int i;
	int32_t reg;
	
	isf_write_cmd(sc, 0, ISF_CMD_RDI);
	device_printf(sc->isf_dev, "manufacturer code: 0x%04x\n",
	    isf_read_reg(sc, ISF_REG_MC));
	device_printf(sc->isf_dev, "device id code: 0x%04x\n",
	    isf_read_reg(sc, ISF_REG_ID));
	device_printf(sc->isf_dev, "read config register: 0x%04x\n",
	    isf_read_reg(sc, ISF_REG_RCR));

	device_printf(sc->isf_dev, "lock register 0: 0x%04x\n",
	    isf_read_reg(sc, ISF_REG_L0));
	device_printf(sc->isf_dev, "lock register 1: 0x%04x\n",
	    isf_read_reg(sc, ISF_REG_L1));

	device_printf(sc->isf_dev, "factory PPR: 0x%016jx\n",
	    (uintmax_t)isf_read_reg64(sc, ISF_REG_FPP));
	device_printf(sc->isf_dev, "user PPR (64-bit): 0x%016jx\n",
	    (uintmax_t)isf_read_reg64(sc, ISF_REG_UPP));

	for (reg = ISF_REG_PP1, i = 1; reg <= ISF_REG_PP16; reg += 8, i++) {
		/* XXX: big-endian ordering of uint64_t's */
		device_printf(sc->isf_dev,
		    "user PPR [%02d]: 0x%016jx%016jx\n", i,
		    (uintmax_t)isf_read_reg64(sc, reg+4),
		    (uintmax_t)isf_read_reg64(sc, reg));
	}

	isf_write_cmd(sc, 0, ISF_CMD_RA);
}

static void
isf_disk_insert(struct isf_softc *sc, off_t mediasize)
{
	struct disk *disk;

	sc->isf_doomed = 0;
	sc->isf_erasing = 0;
	sc->isf_bstate = malloc(sizeof(*sc->isf_bstate) *
	    (mediasize / ISF_ERASE_BLOCK), M_ISF, M_ZERO | M_WAITOK);
	kproc_create(&isf_task, sc, &sc->isf_proc, 0, 0, "isf");

	disk = disk_alloc();
	disk->d_drv1 = sc;
	disk->d_name = "isf";
	disk->d_unit = sc->isf_unit;
	disk->d_strategy = isf_disk_strategy;
	disk->d_ioctl = isf_disk_ioctl;
	disk->d_sectorsize = ISF_SECTORSIZE;
	disk->d_mediasize = mediasize;
	disk->d_maxsize = ISF_SECTORSIZE;
	sc->isf_disk = disk;

	if (bootverbose)
		isf_dump_info(sc);

	disk_create(disk, DISK_VERSION);
	device_printf(sc->isf_dev, "%juM flash device\n",
	    (uintmax_t)disk->d_mediasize / (1024 * 1024));

}

static void
isf_disk_remove(struct isf_softc *sc)
{
	struct disk *disk;

	ISF_LOCK_ASSERT(sc);
	KASSERT(sc->isf_disk != NULL, ("%s: isf_disk NULL", __func__));

	sc->isf_doomed = 1;
	ISF_WAKEUP(sc);
	ISF_SLEEP(sc, sc->isf_proc, 0);

	/*
	 * XXXRW: Is it OK to call disk_destroy() under the mutex, or should
	 * we be deferring that to the calling context once it is released?
	 */
	disk = sc->isf_disk;
	disk_gone(disk);
	disk_destroy(disk);
	sc->isf_disk = NULL;
	free(sc->isf_bstate, M_ISF);
	device_printf(sc->isf_dev, "flash device removed\n");
}

int
isf_attach(struct isf_softc *sc)
{
	uint16_t		id;
	u_long			start, size;
	struct isf_chips	*cp = chip_ids;

	start = rman_get_start(sc->isf_res);
	if (start % 2 != 0) {
		device_printf(sc->isf_dev,
		    "Unsupported flash start alignment %lu\n",
		    start);
		return (ENXIO);
	}

	isf_write_cmd(sc, 0, ISF_CMD_RDI);
	id = isf_read_reg(sc, ISF_REG_ID);
	while (cp->chip_id != id)
		cp++;
	if (cp->chip_desc == NULL) {
		device_printf(sc->isf_dev,
		    "Unsupported device ID 0x%04x\n", id);
		return (ENXIO);
	}
	isf_write_cmd(sc, 0, ISF_CMD_RA);

	size = rman_get_size(sc->isf_res);
	if (size != cp->chip_size) {
		device_printf(sc->isf_dev,
		    "Unsupported flash size %lu\n", size);
		return (ENXIO);
	}

	bioq_init(&sc->isf_bioq);
	ISF_LOCK_INIT(sc);
	sc->isf_disk = NULL;
	isf_disk_insert(sc, size);
	return(0);
}

void
isf_detach(struct isf_softc *sc)
{

	/*
	 * Simulate a disk removal if one is present to deal with any pending
	 * or queued I/O.  This will occur as a result of a device driver
	 * detach -- the Intel StrataFlash has no notion of removal itself.
	 *
	 * XXXRW: Is the locking here right?
	 */
	ISF_LOCK(sc);
	isf_disk_remove(sc);
	bioq_flush(&sc->isf_bioq, NULL, ENXIO);
	KASSERT(bioq_first(&sc->isf_bioq) == NULL,
	    ("%s: non-empty bioq", __func__));
	ISF_UNLOCK(sc);
	ISF_LOCK_DESTROY(sc);
}
