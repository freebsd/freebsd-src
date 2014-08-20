/*
 * This file is part of libdom.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2007 John-Mark Bell <jmb@netsurf-browser.org>
 */

#ifndef dom_functypes_h_
#define dom_functypes_h_

#include <stddef.h>
#include <inttypes.h>

/**
 * Severity levels for dom_msg function, based on syslog(3)
 */
enum {
	DOM_MSG_DEBUG,
	DOM_MSG_INFO,
	DOM_MSG_NOTICE,
	DOM_MSG_WARNING,
	DOM_MSG_ERROR,
	DOM_MSG_CRITICAL,
	DOM_MSG_ALERT,
	DOM_MSG_EMERGENCY
};

/**
 * Type of DOM message function
 */
typedef void (*dom_msg)(uint32_t severity, void *ctx, const char *msg, ...);

#endif
