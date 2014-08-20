/*
 * This file is part of libdom test suite.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2009 Bo Yang <struggleyb.nku@gmail.com>
 */

#include "comparators.h"
#include "domts.h"

#include <string.h>

#include <dom/dom.h>

/* Compare to integer, return zero if equal */
int int_comparator(const void* a, const void* b) {
	return *((const int *)a) - *((const int *)b);
}

/* Compare two string. The first one is a char * and the second
 * one is a dom_string, return zero if equal */
int str_cmp(const void *a, const void *b)
{
	const uint8_t *expected = (const uint8_t *) a;
	dom_string *actual = (dom_string *) b;
	dom_string *exp;
	dom_exception err;
	bool ret;

	err = dom_string_create(expected, strlen((const char *)expected),
			&exp);
	if (err != DOM_NO_ERR)
		return false;

	ret = dom_string_isequal(exp, actual);
	
	dom_string_unref(exp);

	if (ret == true)
		return 0;
	else
		return 1;
}

/* Similar with str_cmp but the first param is a dom_string the second 
 * param is a char *  */
int str_cmp_r(const void *a, const void *b)
{
	return str_cmp(b, a);
}

/* Similar with str_cmp but ignore the case of letters */
int str_icmp(const void *a, const void *b)
{
	const uint8_t *expected = (const uint8_t *) a;
	dom_string *actual = (dom_string *) b;
	dom_string *exp;
	dom_exception err;
	bool ret;

	err = dom_string_create(expected, strlen((const char *)expected),
			&exp);
	if (err != DOM_NO_ERR)
		return false;

	ret = dom_string_caseless_isequal(exp, actual);
	
	dom_string_unref(exp);

	if (ret == true)
		return 0;
	else
		return 1;
}

/* Similar with str_icmp, but the param order are reverse */
int str_icmp_r(const void *a, const void *b)
{
	return str_icmp(b, a);
}
