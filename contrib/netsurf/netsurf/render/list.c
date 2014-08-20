/*
 * Copyright 2005 Richard Wilson <info@tinct.net>
 *
 * This file is part of NetSurf, http://www.netsurf-browser.org/
 *
 * NetSurf is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * NetSurf is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/** \file
 * HTML lists (implementation).
 */
#include <assert.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "css/css.h"
#include "render/list.h"
#include "utils/log.h"


struct list_counter {
  	char *name;				/** Counter name */
  	struct list_counter_state *first;	/** First counter state */
  	struct list_counter_state *state;	/** Current counter state */
	struct list_counter *next;		/** Next counter */
};

struct list_counter_state {
	int count;				/** Current count */
	struct list_counter_state *parent;	/** Parent counter, or NULL */
	struct list_counter_state *next;	/** Next counter, or NULL */
};

static struct list_counter *list_counter_pool = NULL;
static char list_counter_workspace[16];

static const char *list_counter_roman[] = { "I", "IV", "V", "IX",
					    "X", "XL", "L", "XC",
					    "C", "CD", "D", "CM",
					    "M"};
static const int list_counter_decimal[] = {    1,   4,   5,   9,
					      10,  40,  50,  90,
					     100, 400, 500, 900,
					     1000};
#define ROMAN_DECIMAL_CONVERSIONS (sizeof(list_counter_decimal) \
		/ sizeof(list_counter_decimal[0]))


static struct list_counter *render_list_find_counter(const char *name);
static char *render_list_encode_counter(struct list_counter_state *state,
		enum css_list_style_type_e style);
static char *render_list_encode_roman(int value);

/*
static void render_list_counter_output(char *name);
*/

/**
 * Finds a counter from the current pool, or adds a new one.
 *
 * \param name  the name of the counter to find
 * \return the counter, or NULL if it couldn't be found/created.
 */
static struct list_counter *render_list_find_counter(const char *name) {
	struct list_counter *counter;

	assert(name);
	/* find a current counter */
	for (counter = list_counter_pool; counter; counter = counter->next)
		if (!strcasecmp(name, counter->name))
			return counter;

	/* create a new counter */
	counter = calloc(1, sizeof(struct list_counter));
	if (!counter) {
		LOG(("No memory for calloc()"));
		return NULL;
	}
	counter->name = malloc(strlen(name) + 1);
	if (!counter->name) {
		LOG(("No memory for malloc()"));
		free(counter);
		return NULL;
	}
	strcpy(counter->name, name);
	counter->next = list_counter_pool;
	list_counter_pool = counter;
	return counter;
}


/**
 * Removes all counters from the current pool.
 */
void render_list_destroy_counters(void) {
	struct list_counter *counter = list_counter_pool;
	struct list_counter *next_counter;
	struct list_counter_state *state;
	struct list_counter_state *next_state;

	while (counter) {
		next_counter = counter->next;
		free(counter->name);
		state = counter->first;
		free(counter);
		counter = next_counter;
		while (state) {
			next_state = state->next;
			free(state);
			state = next_state;
		}
	}
	list_counter_pool = NULL;
}


/**
 * Resets a counter in accordance with counter-reset (CSS 2.1/12.4).
 *
 * \param name	 the name of the counter to reset
 * \param value  the value to reset the counter to
 * \return true on success, false on failure.
 */
bool render_list_counter_reset(const char *name, int value) {
	struct list_counter *counter;
	struct list_counter_state *state;
	struct list_counter_state *link;

	assert(name);
	counter = render_list_find_counter(name);
	if (!counter)
		return false;
	state = calloc(1, sizeof(struct list_counter_state));
	if (!state) {
		LOG(("No memory for calloc()"));
		return false;
	}
	state->count = value;
	state->parent = counter->state;
	counter->state = state;
	if (!counter->first) {
		counter->first = state;
	} else {
	  	for (link = counter->first; link->next; link = link->next);
	  	link->next = state;
	}
/*	render_list_counter_output(name);
*/	return true;
}


/**
 * Increments a counter in accordance with counter-increment (CSS 2.1/12.4).
 *
 * \param name	 the name of the counter to reset
 * \param value  the value to increment the counter by
 * \return true on success, false on failure.
 */
bool render_list_counter_increment(const char *name, int value) {
	struct list_counter *counter;

	assert(name);
	counter = render_list_find_counter(name);
	if (!counter)
		return false;
	/* if no counter-reset used, it is assumed the counter has been reset
	 * to 0 by the root element. */
	if (!counter->state) {
	  	if (counter->first) {
	  	  	counter->state = counter->first;
	  	  	counter->state->count = 0;
	  	} else {
			render_list_counter_reset(name, 0);
		}
	}
	if (counter->state)
		counter->state->count += value;
/*	render_list_counter_output(name);
*/	return counter->state != NULL;
}


/**
 * Ends the scope of a counter.
 *
 * \param name	 the name of the counter to end the scope for
 * \return true on success, false on failure.
 */
bool render_list_counter_end_scope(const char *name) {
	struct list_counter *counter;

	assert(name);
	counter = render_list_find_counter(name);
	if ((!counter) || (!counter->state))
		return false;
	counter->state = counter->state->parent;
/*	render_list_counter_output(name);
*/	return true;
}


/**
 * Returns a textual representation of a counter for counter() or counters()
 * (CSS 2.1/12.2).
 *
 * \param css_counter  the counter to convert
 * \return a textual representation of the counter, or NULL on failure
 */
char *render_list_counter(const css_computed_content_item *css_counter) {
	struct list_counter *counter;
	struct list_counter_state *state;
	char *compound = NULL;
	char *merge, *extend;
	lwc_string *name = NULL, *sep = NULL;
	uint8_t style;

	assert(css_counter);

	if (css_counter->type == CSS_COMPUTED_CONTENT_COUNTER) {
		name = css_counter->data.counter.name;
		style = css_counter->data.counter.style;
	} else {
		assert(css_counter->type == CSS_COMPUTED_CONTENT_COUNTERS);

		name = css_counter->data.counters.name;
		sep = css_counter->data.counters.sep;
		style = css_counter->data.counters.style;
	}

	counter = render_list_find_counter(lwc_string_data(name));
	if (!counter) {
	  	LOG(("Failed to find/create counter for conversion"));
	  	return NULL;
	}

	/* handle counter() first */
	if (sep == NULL)
		return render_list_encode_counter(counter->state, style);

	/* loop through all states for counters() */
	for (state = counter->first; state; state = state->next) {
	  	merge = render_list_encode_counter(state, style);
	  	if (!merge) {
	  	  	free(compound);
	  	  	return NULL;
	  	}
	  	if (!compound) {
	  		compound = merge;
	  	} else {
	  		extend = realloc(compound, strlen(compound) +
	  				strlen(merge) + 1);
	  		if (!extend) {
	  		  	LOG(("No memory for realloc()"));
	  		  	free(compound);
	  		  	free(merge);
	  		  	return NULL;
	  		}
	  		compound = extend;
	  		strcat(compound, merge);
	  	}
		if (state->next) {
			merge = realloc(compound, strlen(compound) +
					lwc_string_length(sep) + 1);
			if (!merge) {
	  		  	LOG(("No memory for realloc()"));
				free(compound);
				return NULL;
			}
			compound = merge;
			strcat(compound, lwc_string_data(sep));
		}
	}
	return compound;
}


/**
 * Returns a textual representation of a counter state in a specified style.
 *
 * \param state  the counter state to represent
 * \param style  the counter style to use
 * \return a textual representation of the counter state, or NULL on failure
 */
static char *render_list_encode_counter(struct list_counter_state *state,
		enum css_list_style_type_e style) {
	char *result = NULL;
	int i;

	/* no counter state means that the counter is currently out of scope */
	if (!state) {
		result = malloc(1);
		if (!result)
			return NULL;
		result[0] = '\0';
		return result;
	}

	/* perform the relevant encoding to upper case where applicable */
	switch (style) {
		case CSS_LIST_STYLE_TYPE_LOWER_ALPHA:
		case CSS_LIST_STYLE_TYPE_UPPER_ALPHA:
			if (state->count <= 0)
			  	result = calloc(1, 1);
			else
				result = malloc(2);
			if (!result)
				return NULL;
			if (state->count > 0) {
				result[0] = 'A' + (state->count - 1) % 26;
				result[1] = '\0';
			}
			break;
		case CSS_LIST_STYLE_TYPE_DISC:
		case CSS_LIST_STYLE_TYPE_CIRCLE:
		case CSS_LIST_STYLE_TYPE_SQUARE:
			result = malloc(2);
			if (!result)
				return NULL;
			result[0] = '?';
			result[1] = '\0';
			break;
		case CSS_LIST_STYLE_TYPE_LOWER_ROMAN:
		case CSS_LIST_STYLE_TYPE_UPPER_ROMAN:
			result = render_list_encode_roman(state->count);
			if (!result)
				return NULL;
			break;
		case CSS_LIST_STYLE_TYPE_DECIMAL:
			snprintf(list_counter_workspace,
					sizeof list_counter_workspace,
					"%i", state->count);
			result = malloc(strlen(list_counter_workspace) + 1);
			if (!result)
				return NULL;
			strcpy(result, list_counter_workspace);
			break;
		case CSS_LIST_STYLE_TYPE_NONE:
			result = malloc(1);
			if (!result)
				return NULL;
			result[0] = '\0';
			break;
		default:
			break;
	}

	/* perform case conversion */
	if ((style == CSS_LIST_STYLE_TYPE_LOWER_ALPHA) ||
			(style == CSS_LIST_STYLE_TYPE_LOWER_ROMAN))
		for (i = 0; result[i]; i++)
			result[i] = tolower(result[i]);

	return result;
}


/**
 * Encodes a value in roman numerals.
 * For values that cannot be represented (ie <=0) an empty string is returned.
 *
 * \param value  the value to represent
 * \return a string containing the representation, or NULL on failure
 */
static char *render_list_encode_roman(int value) {
	int i, overflow, p = 0;
	char temp[10];
	char *result;

	/* zero and below is returned as an empty string and not erred as
	 * if it is counters() will fail to complete other scopes. */
	if (value <= 0) {
		result = malloc(1);
		if (!result)
			return NULL;
		result[0] = '\0';
		return result;
	}

	/* we only calculate 1->999 and then add a M for each thousand */
	overflow = value / 1000;
	value = value % 1000;

	/* work backwards through the array */
	for (i = ROMAN_DECIMAL_CONVERSIONS - 1; i >= 0; i--) {
		while (value >= list_counter_decimal[0]) {
			if (value - list_counter_decimal[i] <
					list_counter_decimal[0] - 1)
				break;
			value -= list_counter_decimal[i];
			temp[p++] = list_counter_roman[i][0];
			if (i & 0x1)
				temp[p++] = list_counter_roman[i][1];
		}
	}
	temp[p] = '\0';

	/* create a copy for the caller including thousands */
	result = malloc(p + overflow + 1);
	if (!result)
		return NULL;
	for (i = 0; i < overflow; i++)
		result[i] = 'M';
	strcpy(&result[overflow], temp);
	return result;
}








void render_list_test(void) {
  	/* example given in CSS2.1/12.4.1 */
/*	render_list_counter_reset("item", 0);
	render_list_counter_increment("item", 1);
	render_list_counter_increment("item", 1);
	render_list_counter_reset("item", 0);
	render_list_counter_increment("item", 1);
	render_list_counter_increment("item", 1);
	render_list_counter_increment("item", 1);
	render_list_counter_reset("item", 0);
	render_list_counter_increment("item", 1);
	render_list_counter_end_scope("item");
	render_list_counter_reset("item", 0);
	render_list_counter_increment("item", 1);
	render_list_counter_end_scope("item");
	render_list_counter_increment("item", 1);
	render_list_counter_end_scope("item");
	render_list_counter_increment("item", 1);
	render_list_counter_increment("item", 1);
	render_list_counter_end_scope("item");
	render_list_counter_reset("item", 0);
	render_list_counter_increment("item", 1);
	render_list_counter_increment("item", 1);
	render_list_counter_end_scope("item");
*/
}
/*
static void render_list_counter_output(char *name) {
	struct list_counter *counter;
	char *result;
	struct css_counter css_counter;

	assert(name);
	counter = render_list_find_counter(name);
	if (!counter) {
	  	fprintf(stderr, "Unable to create counter '%s'\n", name);
		return;
	}

	css_counter.name = name;
	css_counter.style = CSS_LIST_STYLE_TYPE_LOWER_ALPHA;
	css_counter.separator = NULL;
	result = render_list_counter(&css_counter);
	if (!result) {
		fprintf(stderr, "Failed to output counter('%s')\n", name);
	} else {
		fprintf(stderr, "counter('%s') is '%s'\n", name, result);
		free(result);
	}
	css_counter.separator = ".";
	result = render_list_counter(&css_counter);
	if (!result) {
		fprintf(stderr, "Failed to output counters('%s', '.')\n", name);
	} else {
		fprintf(stderr, "counters('%s', '.') is '%s'\n", name, result);
		free(result);
	}
}
*/
