/*
 * This file is part of libdom test suite.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2009 Bo Yang <struggleyb.nku@gmail.com>
 */

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include <dom/dom.h>

#include "domts.h"

void __assert2(const char *expr, const char *function,
		const char *file, int line)
{
	UNUSED(function);
	UNUSED(file);

	printf("FAIL - %s at line %d\n", expr, line);

	exit(EXIT_FAILURE);
}

/**
 * Following are the test conditions which defined in the DOMTS, please refer
 * the DOM Test Suite for details
 */

bool is_true(bool arg)
{
	return arg == true;
}

bool is_null(void *arg)
{
	return arg == NULL;
}

bool is_same(void *expected, void *actual)
{
	return expected == actual;
}

bool is_same_int(int expected, int actual)
{
	return expected == actual;
}

bool is_same_unsigned_int32_t(uint32_t expected, uint32_t actual)
{
	return expected == actual;
}

bool is_equals_int(int expected, int actual, bool dummy)
{
	UNUSED(dummy);
	
	return expected == actual;
}

bool is_equals_bool(bool expected, bool actual, bool dummy)
{
	UNUSED(dummy);

	return expected == actual;
}

bool is_equals_unsigned_int32_t(uint32_t expected, uint32_t actual, bool dummy)
{
	UNUSED(dummy);

	return expected == actual;
}

/**
 * Test whether two string are equal
 * 
 * \param expected	The expected string
 * \param actual	The actual string
 * \param ignoreCase	Whether to ignore letter case
 */
bool is_equals_string(const char *expected, dom_string *actual, 
		bool ignoreCase)
{
	dom_string *exp;
	dom_exception err;
	bool ret;

	err = dom_string_create((const uint8_t *)expected, strlen(expected),
			&exp);
	if (err != DOM_NO_ERR)
		return false;

	if (ignoreCase == true)
		ret = dom_string_caseless_isequal(exp, actual);
	else
		ret = dom_string_isequal(exp, actual);
	
	dom_string_unref(exp);
	return ret;
}

/* Compare whether two dom_string are equal */
bool is_equals_domstring(dom_string *expected, dom_string *actual, 
		bool ignoreCase)
{
	if (ignoreCase == true)
		return dom_string_caseless_isequal(expected, actual);
	else
		return dom_string_isequal(expected, actual);
}

/* The param actual should always contain dom_sting and expectd should
 * contain char * */
bool is_equals_list(list *expected, list *actual, bool ignoreCase)
{
	assert((expected->type && 0xff00) == (actual->type && 0xff00));

	comparator cmp = NULL;
	comparator rcmp = NULL;

	if (expected->type == INT)
		cmp = int_comparator;
	if (expected->type == STRING) {
		if (actual->type == DOM_STRING) {
			cmp = ignoreCase? str_icmp : str_cmp;
			rcmp = ignoreCase? str_icmp_r : str_cmp_r;
		}
	}
	if (expected->type == DOM_STRING) {
		if (actual->type == STRING) {
			cmp = ignoreCase? str_icmp_r : str_cmp_r;
			rcmp = ignoreCase? str_icmp : str_cmp;
		}
	}

	assert(cmp != NULL);

	return list_contains_all(expected, actual, cmp) && list_contains_all(actual, expected, rcmp);
}



bool is_instanceof(const char *type, dom_node *node)
{
	assert("There is no instanceOf in the test-suite" == NULL);
        
        (void)type;
        (void)node;
        
	return false;
}


bool is_size_domnamednodemap(uint32_t size, dom_namednodemap *map)
{
	uint32_t len;
	dom_exception err;

	err = dom_namednodemap_get_length(map, &len);
	if (err != DOM_NO_ERR) {
		assert("Exception occured" == NULL);
		return false;
	}

	return size == len;
}

bool is_size_domnodelist(uint32_t size, dom_nodelist *list)
{
	uint32_t len;
	dom_exception err;

	err = dom_nodelist_get_length(list, &len);
	if (err != DOM_NO_ERR) {
		assert("Exception occured" == NULL);
		return false;
	}

	return size == len;
}

bool is_size_list(uint32_t size, list *list)
{
	return size == list->size;
}


bool is_uri_equals(const char *scheme, const char *path, const char *host,
		   const char *file, const char *name, const char *query,
		   const char *fragment, const char *isAbsolute,
		   dom_string *actual)
{
	const char *_ptr = actual != NULL ? dom_string_data(actual) : NULL;
	const size_t slen = actual != NULL ? dom_string_byte_length(actual) : 0;
	char *_sptr = actual != NULL ? domts_strndup(_ptr, slen) : NULL;
	char *sptr = _sptr;
	bool result = false;
	
	/* Used farther down */
	const char *firstColon = NULL;
	const char *firstSlash = NULL;
	char *actualPath = NULL;
	char *actualScheme = NULL;
	char *actualHost = NULL;
	char *actualFile = NULL;
	char *actualName = NULL;
	
	assert(sptr != NULL);
	
	/* Note, from here on down, this is essentially a semi-direct
	 * reimplementation of assertURIEquals in the Java DOMTS.
	 */
	
	/* Attempt to check fragment */
	{
		char *fptr = strrchr(sptr, '#');
		const char *cfptr = fptr + 1;
		if (fptr != NULL) {
			*fptr = '\0'; /* Remove fragment from sptr */
		} else {
			cfptr = "";
		}
		if (fragment != NULL) {
			if (strcmp(fragment, cfptr) != 0)
				goto out;
		}
	}
	/* Attempt to check query string */
	{
		char *qptr = strrchr(sptr, '?');
		const char *cqptr = qptr + 1;
		if (qptr != NULL) {
			*qptr = '\0'; /* Remove query from sptr */
		} else {
			cqptr = "";
		}
		if (query != NULL) {
			if (strcmp(query, cqptr) != 0)
				goto out;
		}
	}
	
	/* Scheme and path */
	firstColon = strchr(sptr, ':');
	firstSlash = strchr(sptr, '/');
	actualPath = strdup(sptr);
	actualScheme = strdup("");
	if (firstColon != NULL && firstColon < firstSlash) {
		free(actualScheme);
		free(actualPath);
		actualScheme = domts_strndup(sptr, firstColon - sptr);
		actualPath = strdup(firstColon + 1);
	}
	if (scheme != NULL) {
		if (strcmp(scheme, actualScheme) != 0)
			goto out;
	}
	if (path != NULL) {
		if (strcmp(path, actualPath) != 0)
			goto out;
	}
	
	/* host */
	if (host != NULL) {
		if (actualPath[0] == '/' &&
		    actualPath[1] == '/') {
			const char *termslash = strchr(actualPath + 2, '/');
			actualHost = domts_strndup(actualPath, 
					     termslash - actualPath);
		} else {
			actualHost = strdup("");
		}
		if (strcmp(actualHost, host) != 0)
			goto out;
	}
	
	
	/* file */
	actualFile = strdup(actualPath);
	if (file != NULL || name != NULL) {
		const char *finalSlash = strrchr(actualPath, '/');
		if (finalSlash != NULL) {
			free(actualFile);
			actualFile = strdup(finalSlash + 1);
		}
		if (file != NULL) {
			if (strcmp(actualFile, file) != 0)
				goto out;
		}
	}
	
	/* name */
	if (name != NULL) {
		const char *finalPeriod = strrchr(actualFile, '.');
		if (finalPeriod != NULL) {
			actualName = domts_strndup(actualFile, 
					     finalPeriod - actualFile);
		} else {
			actualName = strdup(actualFile);
		}
		if (strcmp(actualName, name) != 0)
			goto out;
	}
	
	/* isAbsolute */
	if (isAbsolute != NULL) {
		bool startslash = *actualPath == '/';
		bool isabsolute = strcasecmp(isAbsolute, "true") == 0;
		isabsolute |= (strcasecmp(isAbsolute, "yes") == 0);
		isabsolute |= (strcmp(isAbsolute, "1") == 0);
		startslash |= (strncmp(actualPath, "file:/", 6) == 0);
		if (isabsolute != startslash)
			goto out;
	}

	result = true;
out:
	if (actualPath != NULL)
		free(actualPath);
	if (actualScheme != NULL)
		free(actualScheme);
	if (actualHost != NULL)
		free(actualHost);
	if (actualFile != NULL)
		free(actualFile);
	if (actualName != NULL)
		free(actualName);
	free(_sptr);
	return result;
}


bool is_contenttype(const char *type)
{
	/* Now, we use the libxml2 parser for DOM parsing, so the content type
	 * is always "text/xml" */
	if (strcmp(type, "text/xml") == 0)
		return true;
	else
		return false;
}

bool has_feature(const char *feature, const char *version)
{
	dom_exception err;
	bool ret;

	if (feature == NULL)
		feature = "";

	if (version == NULL)
		version = "";

	err = dom_implementation_has_feature(feature, version, &ret);
	/* Here, when we come with exception, we should return false,
	 * TODO: this need to be improved, but I can't figure out how */
	if (err != DOM_NO_ERR) {
		return false;
	}

	return ret;
}

bool implementation_attribute(char *name, bool value)
{
	/* We didnot support DOMConfigure for implementation now */
	UNUSED(name);
	UNUSED(value);

	return true;
}
