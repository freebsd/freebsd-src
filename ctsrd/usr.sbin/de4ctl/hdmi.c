/*-
 * Copyright (c) 2012 Jonathan Woodruff
 * Copyright (c) 2012 Simon W. Moore
 * Copyright (c) 2014 SRI International
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract FA8750-10-C-0237
 * ("CTSRD"), as part of the DARPA CRASH research programme.
 *
 * @BERI_LICENSE_HEADER_START@
 *
 * Licensed to BERI Open Systems C.I.C. (BERI) under one or more contributor
 * license agreements.  See the NOTICE file distributed with this work for
 * additional information regarding copyright ownership.  BERI licenses this
 * file to you under the BERI Hardware-Software License, Version 1.0 (the
 * "License"); you may not use this file except in compliance with the
 * License.  You may obtain a copy of the License at:
 *
 *   http://www.beri-open-systems.org/legal/license-1-0.txt
 *
 * Unless required by applicable law or agreed to in writing, Work distributed
 * under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
 * CONDITIONS OF ANY KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations under the License.
 *
 * @BERI_LICENSE_HEADER_END@
 */

#include <sys/types.h>

#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>

#include "hdmi.h"

extern int qflag;

static int config_fd = -1;

/* ************************************************************************** */
// Helper functions to access I2C

// I2C device number of IT6613 HDMI chip
// note: the device number is the upper 7-bits and bit 0 is left to indicate
//       read or write
#define	HDMI_I2C_DEV	0x98

#define	HDMI_VENDOR_ID	0xca
#define	HDMI_DEVICE_ID	0x613

// clock scale factor to get target 100kHz:  scale = system_clock_kHz/(4*100)
#define	I2C_CLK_SCALE	1250

static void
i2c_write_reg(int regnum, int data)
{
	unsigned char c;

	c = data;
	if (pwrite(config_fd, &c, 1, regnum) != 1)
		err(1, "%s(%d, %d)", __func__, regnum, data);
}

static int
i2c_read_reg(int regnum)
{
	unsigned char c;

	errno = 0;
	if (pread(config_fd, &c, 1, regnum) != 1)
		err(1, "%s(%d)", __func__, regnum);

	return (c);
}

static void
i2c_write_clock_scale(int scale)  // scale is 16-bit number
{

	i2c_write_reg(0, scale & 0xff);
	i2c_write_reg(1, scale >> 8);
}

static int
i2c_read_clock_scale(void)
{

	return (i2c_read_reg(0) | (i2c_read_reg(1) << 8));
}

static void
i2c_write_control(int d)
{

	i2c_write_reg(2, d);
}

static void
i2c_write_tx_data(int d)
{

	i2c_write_reg(3, d);
}

static void
i2c_write_command(int d)
{

	i2c_write_reg(4, d);
}

static int
i2c_read_rx_data(void)
{

	return i2c_read_reg(3);
}

static int
i2c_read_status(void)
{

	return i2c_read_reg(4);
}

static int
i2c_write_data_command(int data, int command)
{
	int t, sr;

	i2c_write_tx_data(data);	/* device num + write (=0) bit */
	i2c_write_command(command);
	sr = i2c_read_status();

	for (t=100*I2C_CLK_SCALE; (t>0) && ((sr & 0x02)!=0); t--)
		sr = i2c_read_status();

	return (sr);
}

static int
hdmi_read_reg(int i2c_addr)
{
	int t, sr;
	/*
	 * write data: (7-bit address, 1-bit 0=write)
	 * command: STA (start condition, bit 7) + write (bit 4)
	 */
	sr = i2c_write_data_command(HDMI_I2C_DEV, 0x90);
	sr = i2c_write_data_command(i2c_addr, 0x10);

	/* now start the read (with STA and WR bits) */
	sr = i2c_write_data_command(HDMI_I2C_DEV | 0x01, 0x90);
	/* set RD bit, set ACK to '1' (NACK), set STO bit */
	i2c_write_command(0x20 | 0x08 | 0x40);

	for (t=100*I2C_CLK_SCALE, sr=2; (t>0) && ((sr & 0x02)!=0); t--)
		sr = i2c_read_status();
	if (t == 0)
		warnx("READ TIME OUT - sr=%x\n", sr);

	return (i2c_read_rx_data());
}

static void
hdmi_write_reg(int i2c_addr, int i2c_data_byte)
{
	int sr;
	/*
	 * write data: (7-bit address, 1-bit 0=write)
	 * command: STA (start condition, bit 7) + write (bit 4)
	 */
	sr = i2c_write_data_command(HDMI_I2C_DEV, 0x90);
	/* command=write */
	sr = i2c_write_data_command(i2c_addr, 0x10);
	/* command=write+STO (stop) */
	sr = i2c_write_data_command(i2c_data_byte & 0xff, 0x50);
}

void
brute_force_write_seq(int fd)
{
	int j;

	config_fd = fd;

	/* enable i2c device but leave interrupts off for now */
	i2c_write_control(0x80);

	/* set clock scale factor = system_clock_freq_in_Khz / 400 */
	if (!qflag)
		printf("Setting clock_scale to 0x%x\n", I2C_CLK_SCALE);
	i2c_write_clock_scale(I2C_CLK_SCALE);
	j = i2c_read_clock_scale();
	if (!qflag) {
		printf("clock scale = 0x%x", j);
		if (j == I2C_CLK_SCALE)
			printf(" - passed\n");
		else
			printf(" - FAILED\n");
	}

	/* switch to using lower register bank (needed after a reset?) */
	hdmi_write_reg(0x0f, 0);

	j = hdmi_read_reg(1);
	if (j == HDMI_VENDOR_ID) {
		if (!qflag)
			printf("Correct vendor ID\n");
	} else
		errx(1, "FAILED - Vendor ID=0x%x but should be 0x%x\n", j,
		    HDMI_VENDOR_ID);

	j = hdmi_read_reg(2) | ((hdmi_read_reg(3) & 0xf)<<8);
	if (j == HDMI_DEVICE_ID) {
		if (!qflag)
			printf("Correct device ID\n");
	} else
		errx(1, "FAILED - Device ID=0x%x but should be 0x%x\n", j,
		    HDMI_DEVICE_ID);

	hdmi_write_reg(0x5, 0x0);
	hdmi_write_reg(0x4, 0x3d);
	hdmi_write_reg(0x4, 0x1d);
	hdmi_write_reg(0x61, 0x30);
	hdmi_write_reg(0x9, 0xb2);
	hdmi_write_reg(0xa, 0xf8);
	hdmi_write_reg(0xb, 0x37);
	hdmi_write_reg(0xf, 0x0);
	hdmi_write_reg(0xc9, 0x0);
	hdmi_write_reg(0xca, 0x0);
	hdmi_write_reg(0xcb, 0x0);
	hdmi_write_reg(0xcc, 0x0);
	hdmi_write_reg(0xcd, 0x0);
	hdmi_write_reg(0xce, 0x0);
	hdmi_write_reg(0xcf, 0x0);
	hdmi_write_reg(0xd0, 0x0);
	hdmi_write_reg(0xe1, 0x0);
	hdmi_write_reg(0xf, 0x0);
	hdmi_write_reg(0xf8, 0xc3);
	hdmi_write_reg(0xf8, 0xa5);
	hdmi_write_reg(0x22, 0x60);
	hdmi_write_reg(0x1a, 0xe0);
	hdmi_write_reg(0x22, 0x48);
	hdmi_write_reg(0xf8, 0xff);
	hdmi_write_reg(0x4, 0x1d);
	hdmi_write_reg(0x61, 0x30);
	hdmi_write_reg(0xc, 0xff);
	hdmi_write_reg(0xd, 0xff);
	hdmi_write_reg(0xe, 0xcf);
	hdmi_write_reg(0xe, 0xce);
	hdmi_write_reg(0x10, 0x1);
	hdmi_write_reg(0x15, 0x9);
	hdmi_write_reg(0xf, 0x0);
	hdmi_write_reg(0x10, 0x1);
	hdmi_write_reg(0x15, 0x9);
	hdmi_write_reg(0x10, 0x1);
	hdmi_write_reg(0x11, 0xa0);
	hdmi_write_reg(0x12, 0x0);
	hdmi_write_reg(0x13, 0x20);
	hdmi_write_reg(0x14, 0x0);
	hdmi_write_reg(0x15, 0x3);
	hdmi_write_reg(0x10, 0x1);
	hdmi_write_reg(0x15, 0x9);
	hdmi_write_reg(0x10, 0x1);
	hdmi_write_reg(0x11, 0xa0);
	hdmi_write_reg(0x12, 0x20);
	hdmi_write_reg(0x13, 0x20);
	hdmi_write_reg(0x14, 0x0);
	hdmi_write_reg(0x15, 0x3);
	hdmi_write_reg(0x10, 0x1);
	hdmi_write_reg(0x15, 0x9);
	hdmi_write_reg(0x10, 0x1);
	hdmi_write_reg(0x11, 0xa0);
	hdmi_write_reg(0x12, 0x40);
	hdmi_write_reg(0x13, 0x20);
	hdmi_write_reg(0x14, 0x0);
	hdmi_write_reg(0x15, 0x3);
	hdmi_write_reg(0x10, 0x1);
	hdmi_write_reg(0x15, 0x9);
	hdmi_write_reg(0x10, 0x1);
	hdmi_write_reg(0x11, 0xa0);
	hdmi_write_reg(0x12, 0x60);
	hdmi_write_reg(0x13, 0x20);
	hdmi_write_reg(0x14, 0x0);
	hdmi_write_reg(0x15, 0x3);
	hdmi_write_reg(0x4, 0x1d);
	hdmi_write_reg(0x61, 0x30);
	hdmi_write_reg(0xf, 0x0);
	hdmi_write_reg(0xc1, 0x41);
	hdmi_write_reg(0xf, 0x1);
	hdmi_write_reg(0x58, 0x10);
	hdmi_write_reg(0x59, 0x68);
	hdmi_write_reg(0x5a, 0x0);
	hdmi_write_reg(0x5b, 0x3);
	hdmi_write_reg(0x5c, 0x0);
	hdmi_write_reg(0x5e, 0x0);
	hdmi_write_reg(0x5f, 0x0);
	hdmi_write_reg(0x60, 0x0);
	hdmi_write_reg(0x61, 0x0);
	hdmi_write_reg(0x62, 0x0);
	hdmi_write_reg(0x63, 0x0);
	hdmi_write_reg(0x64, 0x0);
	hdmi_write_reg(0x65, 0x0);
	hdmi_write_reg(0x5d, 0xf4);
	hdmi_write_reg(0xf, 0x0);
	hdmi_write_reg(0xcd, 0x3);
	hdmi_write_reg(0xf, 0x0);
	hdmi_write_reg(0xf, 0x1);
	hdmi_write_reg(0xf, 0x0);
	hdmi_write_reg(0x4, 0x1d);
	hdmi_write_reg(0x70, 0x0);
	hdmi_write_reg(0x72, 0x0);
	hdmi_write_reg(0xc0, 0x0);
	hdmi_write_reg(0x4, 0x15);
	hdmi_write_reg(0x61, 0x10);
	hdmi_write_reg(0x62, 0x18);
	hdmi_write_reg(0x63, 0x10);
	hdmi_write_reg(0x64, 0xc);
	hdmi_write_reg(0x4, 0x15);
	hdmi_write_reg(0x4, 0x15);
	hdmi_write_reg(0xc, 0x0);
	hdmi_write_reg(0xd, 0x40);
	hdmi_write_reg(0xe, 0x1);
	hdmi_write_reg(0xe, 0x0);
	hdmi_write_reg(0xf, 0x0);
	hdmi_write_reg(0x61, 0x0);
	hdmi_write_reg(0xf, 0x0);
	hdmi_write_reg(0xc1, 0x40);
	hdmi_write_reg(0xc6, 0x3);
}
