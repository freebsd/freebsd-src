/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The FreeBSD Foundation
 *
 * This software was developed by Olivier Certner <olce@FreeBSD.org> at Kumacom
 * SARL under sponsorship from the FreeBSD Foundation.
 */

#include <sys/sysctl.h>

#include <x86/cpufreq/hwpstate_common.h>


int hwpstate_verbose;
SYSCTL_INT(_debug, OID_AUTO, hwpstate_verbose, CTLFLAG_RWTUN,
    &hwpstate_verbose, 0, "Debug hwpstate");

bool hwpstate_pkg_ctrl_enable = true;
SYSCTL_BOOL(_machdep, OID_AUTO, hwpstate_pkg_ctrl, CTLFLAG_RDTUN,
    &hwpstate_pkg_ctrl_enable, 0,
    "Set 1 (default) to enable package-level control, 0 to disable");
