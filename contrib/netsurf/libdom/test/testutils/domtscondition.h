/*
 * This file is part of libdom test suite.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2009 Bo Yang <struggleyb.nku@gmail.com>
 */

#ifndef domtscondition_h_
#define domtscondition_h_

#include <stdbool.h>

/**
 * Just simple functions which meet the needs of DOMTS conditions
 */

static inline bool less(int excepted, int actual)
{
	return actual < excepted;
}

static inline bool less_or_equals(int excepted, int actual)
{
	return actual <= excepted;
}

static inline bool greater(int excepted, int actual)
{
	return actual > excepted;
}

static inline bool greater_or_equals(int excepted, int actual)
{
	return actual >= excepted;
}

#endif
