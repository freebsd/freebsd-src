/*
 * Copyright (c) 2026 BBOX.io
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#ifndef _ICE_FAULT_H_
#define _ICE_FAULT_H_

#include "ice_opts.h"

#ifdef DRIVER_FAILPOINTS

#include <sys/fail.h>

struct ice_softc;

SYSCTL_DECL(_debug_fail_point_ice);

bool ice_fail_point_device_matches(struct ice_softc *sc);

#define ICE_FAIL_POINT_CODE_COND(_sc, _parent, _name, _cond, _flags, _code...) \
	KFAIL_POINT_CODE_COND(_parent, _name, \
	    ice_fail_point_device_matches((_sc)) && (_cond), _flags, _code)
#define ICE_FAIL_POINT_CODE(_sc, _parent, _name, _flags, _code...) \
	ICE_FAIL_POINT_CODE_COND(_sc, _parent, _name, true, _flags, _code)

#else /* !DRIVER_FAILPOINTS */

#define ICE_FAIL_POINT_CODE_COND(_sc, _parent, _name, _cond, _flags, _code...) \
	do { } while (0)
#define ICE_FAIL_POINT_CODE(_sc, _parent, _name, _flags, _code...) \
	do { } while (0)

#endif /* DRIVER_FAILPOINTS */

#endif /* _ICE_FAULT_H_ */
