/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026, Netflix, Inc.
 *
 * This software was developed by Ali Mashtizadeh under the sponsorship from
 * Netflix, Inc.
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
 */

#include <sys/ioctl.h>

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <term.h>
#include <math.h>

#include <curses.h>
#include <unistd.h>

#include <cxxabi.h>
#include <cstring>
#include <iomanip>
#include <ios>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "display.hh"

enum termmode {
    TERMMODE_BASIC,
    TERMMODE_VT,
    TERMMODE_XTERM
};

static termmode tmode;
static int disp_height;
static int disp_width;

#define TITLE_TITLE		0
#define TITLE_SUBTITLE		1

static const char **title_char;

static const char *title_vt[] = {
    "\xe2\x95\x90", "\xe2\x94\x80"
};
static const char *title_basic[] = { "=", "-" };

#define ARROW_DOWN		0
#define ARROW_UP		1

static const char **arrow_char;

static const char *arrow_vt[] = {
    "\xe2\x96\xbc", "\xe2\x96\xb2"
};
static const char *arrow_basic[] = { "D", "U" };

#define BORDER_HBAR		0
#define BORDER_VBAR		1
#define BORDER_TOPRIGHT		2
#define BORDER_TOPLEFT		3
#define BORDER_BOTTOMLEFT	4
#define BORDER_BOTTOMRIGHT	5
#define BORDER_TOPSPLIT		6
#define BORDER_BOTTOMSPLIT	7
#define BORDER_RIGHTSPLIT	8
#define BORDER_LEFTSPLIT	9
#define BORDER_CROSS		10

static const char **border_char;

static const char *border_vt[] = {
    "\xe2\x94\x80", "\xe2\x94\x82",
    "\xe2\x94\x8c", "\xe2\x94\x90", "\xe2\x94\x94", "\xe2\x94\x98",
    "\xe2\x94\xac", "\xe2\x94\xb4", "\xe2\x94\xa4", "\xe2\x94\x9c",
    "\xe2\x94\xbc"
};
static const char *border_basic[] = { "-", "|", "+", "+", "+", "+", "+", "+", "+", "+" };

static char tc_boldbuf[16];
static char tc_sgr0buf[16];
static char tc_afbuf[16];
static char tc_green[16];
static char tc_yellow[16];
static char tc_red[16];

static const char *TC_BOLD;
static const char *TC_SGR0;
static const char *TC_SETAF;

void
tcemit(const char *c)
{
	if (c)
		tputs(c, 1, putchar);
}

void
tcfg(int n)
{
	if (TC_SETAF)
		tputs(tgoto(TC_SETAF, 0, n), 1, putchar);
}

static void
setup_termcap(const char *term)
{
	bool xcolor;
	char *buf;
	char *clicolor;
	char termcapbuf[4096];

	if (tgetent(termcapbuf, "xterm-256color") != 1) {
		TC_BOLD = NULL;
		TC_SGR0 = NULL;
		TC_SETAF = NULL;
		return;
	}

	buf = tc_boldbuf;
	TC_BOLD = tgetstr("md", &buf);
	buf = tc_sgr0buf;
	TC_SGR0 = tgetstr("me", &buf);
	buf = tc_afbuf;
	TC_SETAF = tgetstr("AF", &buf);

	clicolor = getenv("CLICOLOR");
	if (clicolor == NULL || strcmp(clicolor, "1") != 0) {
		return;
	}

	if (strcmp(term, "xterm-256color") == 0) {
		xcolor = true;
	} else {
		xcolor = false;
	}

	buf = tgoto(TC_SETAF, 1, xcolor ? COLOR256_GREEN : COLOR_GREEN);
	if (buf) {
		strlcpy(tc_green, buf, sizeof(tc_green));
	} else {
		tc_green[0] = '\0';
	}

	buf = tgoto(TC_SETAF, 1, xcolor ? COLOR256_YELLOW : COLOR_YELLOW);
	if (buf) {
		strlcpy(tc_yellow, buf, sizeof(tc_yellow));
	} else {
		tc_yellow[0] = '\0';
	}

	buf = tgoto(TC_SETAF, 1, xcolor ? COLOR256_RED : COLOR_RED);
	if (buf) {
		strlcpy(tc_red, buf, sizeof(tc_red));
	} else {
		tc_red[0] = '\0';
	}
}

void
setup_screen()
{
	int status;
	const char *term;
	struct winsize wsz;

	/* Sane defaults */
	tmode = TERMMODE_BASIC;
	title_char = title_basic;
	arrow_char = arrow_basic;
	border_char = border_basic;
	disp_height = 25;
	disp_width = 80;

	if (isatty(STDOUT_FILENO) == 0) {
		tmode = TERMMODE_XTERM;
		title_char = title_vt;
		arrow_char = arrow_vt;
		border_char = border_vt;
		return;
	}

	status = ioctl(STDIN_FILENO, TIOCGWINSZ, &wsz);
	if (status < 0) {
		perror("ioctl(TIOCGWINSZ)");
		return;
	}

	disp_height = wsz.ws_row;
	disp_width = wsz.ws_col;

	term = getenv("TERM");
	if (term == NULL) {
		tmode = TERMMODE_BASIC;
		title_char = title_basic;
		arrow_char = arrow_basic;
		border_char = border_basic;
		return;
	}

	if (strncmp(term, "xterm", 5) == 0) {
		tmode = TERMMODE_XTERM;
		title_char = title_vt;
		arrow_char = arrow_vt;
		border_char = border_vt;
	} else if (strncmp(term, "vt", 2) == 0) {
		tmode = TERMMODE_VT;
		title_char = title_vt;
		arrow_char = arrow_vt;
		border_char = border_vt;
	}

	setup_termcap(term);
}

void
title(const std::string &t)
{
	size_t i;

	if (tmode != TERMMODE_BASIC)
		tcemit(TC_BOLD);
	printf("%s\n", t.c_str());
	if (tmode != TERMMODE_BASIC)
		tcemit(TC_SGR0);
	for (i = 0; i < t.size(); i++)
		printf("%s", title_char[TITLE_TITLE]);
	printf("\n\n");
}

void
header(const std::string &h)
{
	size_t i;

	if (tmode != TERMMODE_BASIC)
		tcemit(TC_BOLD);
	printf("%s\n", h.c_str());
	if (tmode != TERMMODE_BASIC)
		tcemit(TC_SGR0);
	for (i = 0; i < h.size(); i++)
		printf("%s", title_char[TITLE_SUBTITLE]);
	printf("\n");
}

void
printval(const std::string &msg, uint64_t val, siunit ui)
{
	switch (ui) {
	case siunit::seconds:
		printf("%s: %" PRIu64 " s\n", msg.c_str(), val);
		break;
	case siunit::percent:
		printf("%s: %" PRIu64 "%%\n", msg.c_str(), val);
		break;
	case siunit::cycles:
		printf("%s: %" PRIu64 " cycles\n", msg.c_str(), val);
		break;
	}
}

void
printval(const std::string &msg, float val, siunit ui)
{
	switch (ui) {
	case siunit::seconds:
		printf("%s: %.2f s\n", msg.c_str(), val);
		break;
	case siunit::percent:
		printf("%s: %6.2f%%\n", msg.c_str(), val);
		break;
	case siunit::cycles:
		printf("%s: %.2f cycles\n", msg.c_str(), val);
		break;
	}
}

std::string
format_siprefix(uint64_t count)
{
	unsigned long index = 0;
	char prefix[] = " kMGTP";
	char buf[10];

	while (count > 10000 && index < sizeof(prefix)) {
		index++;
		count /= 1000;
	}

	if (index == 0) {
		snprintf(buf, sizeof(buf), "%" PRIu64, count);
	} else {
		snprintf(buf, sizeof(buf), "%" PRIu64 "%c", count, prefix[index]);
	}

	return buf;
}

std::string
format_binprefix(uint64_t count)
{
	unsigned long index = 0;
	char prefix[] = " KMGTP";
	char buf[10];

	while (count > 10000 && index < sizeof(prefix)) {
		index++;
		count /= 1000;
	}

	if (index == 0) {
		snprintf(buf, sizeof(buf), "%" PRIu64 " B", count);
	} else {
		snprintf(buf, sizeof(buf), "%" PRIu64 " %ciB", count, prefix[index]);
	}

	return buf;
}

std::string
format_sample(uint64_t count, uint64_t total)
{
	char buf[20];
	std::string fmt = format_siprefix(count);

	snprintf(buf, sizeof(buf), "%5s (%4.1f%%)", fmt.c_str(),
	    100.0 * (float)count / (float)total);

	return buf;
}

std::string
format_percent(uint64_t count, uint64_t total)
{
	char buf[20];

	if (total == 0) {
		snprintf(buf, sizeof(buf), "%6s", "");
	} else {
		snprintf(buf, sizeof(buf), "%5.1f%%",
		    100.0 * (float)count / (float)total);
	}

	return buf;
}

/* fields */

field::field(int64_t v)
{
	type = FIELD::INTEGER;
	value[0] = v;
}

field::field(float v)
{
	type = FIELD::FLOAT;
	fvalue = v;
}

field::field(const std::string &s)
{
	type = FIELD::STRING;
	svalue = s;
}

field::field(int64_t c, int64_t t, bool percent)
{
	if (percent)
		type = FIELD::PERCENT;
	else
		type = FIELD::SAMPLE;
	value[0] = c;
	value[1] = t;
}

field::field(int64_t uc, int64_t ut, int64_t ku, int64_t kt)
{
	type = FIELD::DSAMPLE;
	value[0] = uc;
	value[1] = ut;
	value[2] = ku;
	value[3] = kt;
}

bool
field::operator>(const field &b) const
{
	switch (type) {
	case FIELD::STRING:
		return (svalue > b.svalue);
	case FIELD::FLOAT:
		return (fvalue > b.fvalue);
	case FIELD::INTEGER: [[fallthrough]];
	case FIELD::SAMPLE:
		return (value[0] > b.value[0]);
	case FIELD::DSAMPLE:
		return ((value[0] + value[2]) > (b.value[0] + b.value[2]));
	case FIELD::PERCENT:
		return (((float)value[0] / (float)value[1]) >
		    ((float)b.value[0] / (float)b.value[1]));
	}
}

bool
field::operator<(const field &b) const
{
	switch (type) {
	case FIELD::STRING:
		return (svalue < b.svalue);
	case FIELD::FLOAT:
		return (fvalue < b.fvalue);
	case FIELD::INTEGER: [[fallthrough]];
	case FIELD::SAMPLE:
		return (value[0] < b.value[0]);
	case FIELD::DSAMPLE:
		return ((value[0] + value[2]) < (b.value[0] + b.value[2]));
	case FIELD::PERCENT:
		return (((float)value[0] / (float)value[1]) <
		    ((float)b.value[0] / (float)b.value[1]));
	}
}

size_t
field::length()
{
	return (to_string().length());
}

std::string
field::to_string()
{
	switch (type) {
	case FIELD::STRING:
		return (svalue);
	case FIELD::FLOAT:
		return (std::to_string(fvalue));
	case FIELD::INTEGER:
		return (std::to_string(value[0]));
	case FIELD::SAMPLE:
		return (format_sample(value[0], value[1]));
	case FIELD::DSAMPLE:
		return (format_sample(value[0], value[1]) + " / " +
		    format_sample(value[2], value[3]));
	case FIELD::PERCENT:
		return (format_percent(value[0], value[1]));
	}
}

/* table class */

table::table() : sortcol(-1), sortdir(true), cols(), align(), rows()
{
}

table::~table()
{
}

void
table::addcolumn(const std::string &c, bool alignleft)
{
	cols.push_back(c);
	align.push_back(alignleft);
}

void
table::addrow(std::vector<field> r)
{
	rows.push_back(std::move(r));
}

void
table::sort(int col, bool descending)
{
	sortcol = col;
	sortdir = descending;
	if (descending)
		std::sort(rows.begin(), rows.end(), [col](auto &a, auto &b)
		    { return (a[col] > b[col]); });
	else
		std::sort(rows.begin(), rows.end(), [col](auto &a, auto &b)
		    { return (a[col] < b[col]); });
}

void
table::print()
{
	unsigned long c;
	std::vector<unsigned long> width;

	// Compute column width
	for (auto h : cols)
		width.push_back(h.length() + 1);
	if (sortcol != -1)
		width[sortcol] += 1;
	for (auto &r : rows) {
		for (c = 0; c < r.size(); c++) {
			if (width[c] < r[c].length())
				width[c] = r[c].length();
		}
	}

	for (c = 0; c < width.size(); c++) {
		if (align[c])
			std::cout << std::left;
		std::cout << std::setw(width[c]);
		tcemit(TC_BOLD);
		if (c == (unsigned long)sortcol)
		    std::cout << cols[c] + " " + arrow_char[sortdir ? ARROW_DOWN : ARROW_UP];
		else
		    std::cout << cols[c];
		tcemit(TC_SGR0);
		if (c != (width.size() - 1))
			std::cout << " " << border_char[BORDER_VBAR] << " ";
		if (align[c])
			std::cout << std::right;
	}
	std::cout << std::endl;

	for (c = 0; c < width.size(); c++) {
		for (unsigned long i = 0; i < width[c]; i++)
		    std::cout << border_char[BORDER_HBAR];
		if (c != (width.size() - 1))
			std::cout << border_char[BORDER_HBAR] <<
			    border_char[BORDER_CROSS] << border_char[BORDER_HBAR];
	}
	std::cout << std::setfill(' ') << std::endl;

	for (auto &r : rows) {
		for (c = 0; c < width.size(); c++) {
			if (align[c])
				std::cout << std::left;
			std::cout << std::setw(width[c]) << r[c].to_string();
			if (c != (width.size() - 1))
				std::cout << " " << border_char[BORDER_VBAR] << " ";
			if (align[c])
				std::cout << std::right;
		}
		std::cout << std::endl;
	}

	std::cout << std::endl;
}

