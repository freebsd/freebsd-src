/*-
 * Copyright (c) 2026 Stéphane Rochoy <sheda@FreeBSD.org>
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <err.h>
#include <fcntl.h>
#include <ucl.h>

#include "cprofile.h"

static int
store_lo_duty(const ucl_object_t *lo_duty)
{
	cprofile.lo_duty = (unsigned int)ucl_object_toint(lo_duty);
	return (0);
}

static int
store_step(const ucl_object_t *step)
{
	const ucl_object_t *prop;
	ucl_object_iter_t   it;
	float               temp = 0.0;
	unsigned int        duty = 0;
	bool                temp_found = false, duty_found = false;
	int                 ret = -1;

	it = ucl_object_iterate_new(step);
	for (prop = ucl_object_iterate_safe(it, true);
	     prop != NULL;
	     prop = ucl_object_iterate_safe(it, true)) {
		const char *key;

		key = ucl_object_key(prop);
		if (strcmp(key, "temp") == 0) {
			if (temp_found) {
				warnx("cprofile: Duplicated temp property"
				    " at step %s", ucl_object_key(step));
				goto done;
			}
			temp = (float)ucl_object_todouble(prop);
			temp_found = true;
		} else if (strcmp(key, "duty") == 0) {
			if (duty_found) {
				warnx("cprofile: Duplicated duty property"
				    " at step %s", ucl_object_key(step));
				goto done;
			}
			duty = (unsigned int)ucl_object_toint(prop);
			duty_found = true;
		} else {
			warnx("cprofile: Unknown property at step %s",
			    ucl_object_key(step));
			goto done;
		}
	}
	if (!temp_found) {
		warnx("cprofile: Missing temp property from step %s",
		    ucl_object_key(step));
		goto done;
	}
	if (cprofile.step_count > 0) {
		float prev_raw_temp =
		    cprofile.steps[cprofile.step_count - 1].raw_temp;

		if (prev_raw_temp >= temp) {
			warnx("cprofile: Invalid temp property from step %s;"
			    " must be greater than %.2f",
			    ucl_object_key(step), prev_raw_temp);
			goto done;
		}
	}
	if (!duty_found) {
		warnx("cprofile: Missing duty property from step %s",
		    ucl_object_key(step));
		goto done;
	}
	cprofile.steps[cprofile.step_count].raw_temp = temp;
	cprofile.steps[cprofile.step_count].duty     = duty;
	cprofile.step_count++;
	ret = 0;
  done:
	ucl_object_iterate_free(it);
	return (ret);
}

static int
store_steps(const ucl_object_t *steps)
{
	const ucl_object_t *step;
	ucl_object_iter_t it;
	int ret;

	ret = -1;
	it  = ucl_object_iterate_new(steps);
	while ((step = ucl_object_iterate_safe(it, true)) != NULL &&
	       cprofile.step_count < MAX_CSTEPS) {
		if (store_step(step) == -1)
			goto done;
	}
	if (cprofile.step_count == MAX_CSTEPS) {
		warnx("cprofile: Maximum number of steps exceeded");
		goto done;
	}
	ret = 0;
  done:
	ucl_object_iterate_free(it);
	return (ret);
}

int
parse_cprofile(const char *path)
{
	struct ucl_parser *parser;
	const ucl_object_t *top, *obj;
	ucl_object_iter_t it;
	bool steps_found, loduty_found;
	int fd, oflags, ret;

	parser = ucl_parser_new(0);
	if (parser == NULL) {
		warnx("cprofile: Unable to allocate parser");
		return (-1);
	}

	oflags = O_RDONLY;
#ifdef USE_VERIEXEC
	oflags |= O_VERIFY;
#endif
	fd = open(path, oflags);
	if (fd == -1) {
		warn("cprofile: open");
		return (-1);
	}

	ucl_parser_add_fd(parser, fd);
	if (ucl_parser_get_error(parser)) {
		warnx("cprofile: %s", ucl_parser_get_error(parser));
		return (-1);
	}

	top = ucl_parser_get_object(parser);
	if (ucl_parser_get_error(parser)) {
		warnx("cprofile: %s", ucl_parser_get_error(parser));
		return (-1);
	}

	steps_found	= false;
	loduty_found	= false;
	ret		= -1;
	it		= ucl_object_iterate_new(top);
	while ((obj = ucl_object_iterate_safe(it, true)) != NULL) {
		const char *key;

		key = ucl_object_key(obj);

		if (strcmp(key, "steps") == 0) {
			steps_found = true;
			if (store_steps(obj) == -1)
				goto done;
		} else if (strcmp(key, "lo_duty") == 0) {
			loduty_found = true;
			if (store_lo_duty(obj) == -1)
				goto done;
		} else {
			warnx("cprofile: Unknown property: %s", key);
			goto done;
		}
	}
	if (!loduty_found) {
		warnx("cprofile: Missing lo_duty property");
		goto done;
	}
	if (!steps_found) {
		warnx("cprofile: Missing steps property");
		goto done;
	}
	if (ucl_object_iter_chk_excpn(it)) {
		warnx("cprofile: Iterator exception");
		goto done;
	}
	ret = 0;
  done:
	ucl_object_iterate_free(it);
	return (ret);
}
