/*-
 * Copyright (c) 1999 Takanori Watanabe <takawata@jp.freebsd.org>
 * Copyright (c) 1999 Mitsuru IWASAKI <iwasaki@FreeBSD.org>
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
 * $FreeBSD$
 */

#ifndef _ACPIIO_H_
#define _ACPIIO_H_

/*
 * Core ACPI subsystem ioctls
 */
#define ACPIIO_SETSLPSTATE	_IOW('P', 3, int)

struct acpi_battdesc {
    int	 type;				/* battery type */
    int	 phys_unit;			/* physical unit of devclass */
};

#define ACPI_BATT_TYPE_CMBAT		0x0000
#define ACPI_BATT_TYPE_SMBAT		0x0001

struct acpi_battinfo {
    int	 cap;				/* percent */
    int	 min;				/* remaining time (in minutes) */
    int	 state;				/* battery state */
};

#define ACPI_CMBAT_MAXSTRLEN 32
struct acpi_bif {
    u_int32_t units;			/* 0 for mWh, 1 for mAh */
    u_int32_t dcap;			/* Design Capacity */
    u_int32_t lfcap;			/* Last Full capacity */
    u_int32_t btech;			/* Battery Technology */
    u_int32_t dvol;			/* Design voltage (mV) */
    u_int32_t wcap;			/* WARN capacity */
    u_int32_t lcap;			/* Low capacity */
    u_int32_t gra1;			/* Granularity 1 (Warn to Low) */
    u_int32_t gra2;			/* Granularity 2 (Full to Warn) */
    char model[ACPI_CMBAT_MAXSTRLEN];	/* model identifier */
    char serial[ACPI_CMBAT_MAXSTRLEN];	/* Serial number */
    char type[ACPI_CMBAT_MAXSTRLEN];	/* Type */
    char oeminfo[ACPI_CMBAT_MAXSTRLEN];	/* OEM infomation */
};

struct acpi_bst {
    u_int32_t state;			/* Battery State */
    u_int32_t rate;			/* Present Rate */
    u_int32_t cap;			/* Remaining Capacity */
    u_int32_t volt;			/* Present Voltage */
};

#define ACPI_BATT_STAT_DISCHARG		0x0001
#define ACPI_BATT_STAT_CHARGING		0x0002
#define ACPI_BATT_STAT_CRITICAL		0x0004
#define ACPI_BATT_STAT_NOT_PRESENT	0x0007
#define ACPI_BATT_STAT_MAX		0x0007

union acpi_battery_ioctl_arg {
    int			 unit;	/* argument: logical unit (-1 = overall) */

    struct acpi_battdesc battdesc; 
    struct acpi_battinfo battinfo; 

    struct acpi_bif	 bif;
    struct acpi_bst	 bst;
};

/* Common battery ioctls */
#define ACPIIO_BATT_GET_UNITS	  _IOR('B', 0x01, int)
#define ACPIIO_BATT_GET_TYPE	  _IOR('B', 0x02, union acpi_battery_ioctl_arg)
#define ACPIIO_BATT_GET_BATTINFO _IOWR('B', 0x03, union acpi_battery_ioctl_arg)
#define ACPIIO_BATT_GET_BATTDESC _IOWR('B', 0x04, union acpi_battery_ioctl_arg)
#define ACPIIO_BATT_GET_BIF	 _IOWR('B', 0x10, union acpi_battery_ioctl_arg)
#define ACPIIO_BATT_GET_BST	 _IOWR('B', 0x11, union acpi_battery_ioctl_arg)

/* Control Method battery ioctls (deprecated) */
#define ACPIIO_CMBAT_GET_BIF	 ACPIIO_BATT_GET_BIF
#define ACPIIO_CMBAT_GET_BST	 ACPIIO_BATT_GET_BST

/* Get AC adapter status. */
#define ACPIIO_ACAD_GET_STATUS	  _IOR('A', 1, int)

#ifdef _KERNEL
typedef int	(*acpi_ioctl_fn)(u_long cmd, caddr_t addr, void *arg);
extern int	acpi_register_ioctl(u_long cmd, acpi_ioctl_fn fn, void *arg);
extern void	acpi_deregister_ioctl(u_long cmd, acpi_ioctl_fn fn);
#endif

#endif /* !_ACPIIO_H_ */
