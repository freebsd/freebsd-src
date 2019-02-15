/*
 * Copyright 2011 Advanced Micro Devices, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE COPYRIGHT HOLDER(S) OR AUTHOR(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * Authors: Alex Deucher
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <dev/drm2/drmP.h>
#include <dev/drm2/radeon/radeon_drm.h>
#include <dev/iicbus/iic.h>
#include <dev/iicbus/iiconf.h>
#include <dev/iicbus/iicbus.h>
#include "radeon.h"
#include "atom.h"
#include "iicbus_if.h"
#include "iicbb_if.h"

#define TARGET_HW_I2C_CLOCK 50

/* these are a limitation of ProcessI2cChannelTransaction not the hw */
#define ATOM_MAX_HW_I2C_WRITE 2
#define ATOM_MAX_HW_I2C_READ  255

static int radeon_process_i2c_ch(struct radeon_i2c_chan *chan,
				 u8 slave_addr, u8 flags,
				 u8 *buf, u8 num)
{
	struct drm_device *dev = chan->dev;
	struct radeon_device *rdev = dev->dev_private;
	PROCESS_I2C_CHANNEL_TRANSACTION_PS_ALLOCATION args;
	int index = GetIndexIntoMasterTable(COMMAND, ProcessI2cChannelTransaction);
	unsigned char *base;
	u16 out;

	memset(&args, 0, sizeof(args));

	base = (unsigned char *)rdev->mode_info.atom_context->scratch;

	if (flags & HW_I2C_WRITE) {
		if (num > ATOM_MAX_HW_I2C_WRITE) {
			DRM_ERROR("hw i2c: tried to write too many bytes (%d vs 2)\n", num);
			return -EINVAL;
		}
		memcpy(&out, buf, num);
		args.lpI2CDataOut = cpu_to_le16(out);
	} else {
#ifdef FREEBSD_WIP /* FreeBSD: to please GCC 4.2. */
		if (num > ATOM_MAX_HW_I2C_READ) {
			DRM_ERROR("hw i2c: tried to read too many bytes (%d vs 255)\n", num);
			return -EINVAL;
		}
#endif
	}

	args.ucI2CSpeed = TARGET_HW_I2C_CLOCK;
	args.ucRegIndex = 0;
	args.ucTransBytes = num;
	args.ucSlaveAddr = slave_addr << 1;
	args.ucLineNumber = chan->rec.i2c_id;

	atom_execute_table(rdev->mode_info.atom_context, index, (uint32_t *)&args);

	/* error */
	if (args.ucStatus != HW_ASSISTED_I2C_STATUS_SUCCESS) {
		DRM_DEBUG_KMS("hw_i2c error\n");
		return -EIO;
	}

	if (!(flags & HW_I2C_WRITE))
		memcpy(buf, base, num);

	return 0;
}

static int
radeon_atom_hw_i2c_xfer(device_t dev, struct iic_msg *msgs, u_int num)
{
	struct radeon_i2c_chan *i2c = device_get_softc(dev);
	struct iic_msg *p;
	int i, remaining, current_count, buffer_offset, max_bytes, ret;
	u8 buf = 0, flags;

	/* check for bus probe */
	p = &msgs[0];
	if ((num == 1) && (p->len == 0)) {
		ret = radeon_process_i2c_ch(i2c,
					    p->slave, HW_I2C_WRITE,
					    &buf, 1);
		if (ret)
			return -ret; /* "ret" is returned on Linux. */
		else
			return (0); /* "num" is returned on Linux. */
	}

	for (i = 0; i < num; i++) {
		p = &msgs[i];
		remaining = p->len;
		buffer_offset = 0;
		/* max_bytes are a limitation of ProcessI2cChannelTransaction not the hw */
		if (p->flags & IIC_M_RD) {
			max_bytes = ATOM_MAX_HW_I2C_READ;
			flags = HW_I2C_READ;
		} else {
			max_bytes = ATOM_MAX_HW_I2C_WRITE;
			flags = HW_I2C_WRITE;
		}
		while (remaining) {
			if (remaining > max_bytes)
				current_count = max_bytes;
			else
				current_count = remaining;
			ret = radeon_process_i2c_ch(i2c,
						    p->slave, flags,
						    &p->buf[buffer_offset], current_count);
			if (ret)
				return -ret; /* "ret" is returned on Linux. */
			remaining -= current_count;
			buffer_offset += current_count;
		}
	}

	return (0); /* "num" is returned on Linux. */
}

static int
radeon_atom_hw_i2c_probe(device_t dev)
{

	return (BUS_PROBE_SPECIFIC);
}

static int
radeon_atom_hw_i2c_attach(device_t dev)
{
	struct radeon_i2c_chan *i2c;
	device_t iic_dev;

	i2c = device_get_softc(dev);
	device_set_desc(dev, i2c->name);

	/* add generic bit-banging code */
	iic_dev = device_add_child(dev, "iicbus", -1);
	if (iic_dev == NULL)
		return (ENXIO);
	device_quiet(iic_dev);

	/* attach and probe added child */
	bus_generic_attach(dev);

	return (0);
}

static int
radeon_atom_hw_i2c_detach(device_t dev)
{
	/* detach bit-banding code. */
	bus_generic_detach(dev);

	/* delete bit-banding code. */
	device_delete_children(dev);
	return (0);
}

static int
radeon_atom_hw_i2c_reset(device_t dev, u_char speed,
    u_char addr, u_char *oldaddr)
{

	return (0);
}

static device_method_t radeon_atom_hw_i2c_methods[] = {
	DEVMETHOD(device_probe,		radeon_atom_hw_i2c_probe),
	DEVMETHOD(device_attach,	radeon_atom_hw_i2c_attach),
	DEVMETHOD(device_detach,	radeon_atom_hw_i2c_detach),
	DEVMETHOD(iicbus_reset,		radeon_atom_hw_i2c_reset),
	DEVMETHOD(iicbus_transfer,	radeon_atom_hw_i2c_xfer),
	DEVMETHOD_END
};

static driver_t radeon_atom_hw_i2c_driver = {
	"radeon_atom_hw_i2c",
	radeon_atom_hw_i2c_methods,
	0
};

static devclass_t radeon_atom_hw_i2c_devclass;
DRIVER_MODULE_ORDERED(radeon_atom_hw_i2c, drmn, radeon_atom_hw_i2c_driver,
    radeon_atom_hw_i2c_devclass, 0, 0, SI_ORDER_ANY);
