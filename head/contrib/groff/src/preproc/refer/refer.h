// -*- C++ -*-
/* Copyright (C) 1989, 1990, 1991, 1992, 2001, 2004
   Free Software Foundation, Inc.
     Written by James Clark (jjc@jclark.com)

This file is part of groff.

groff is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free
Software Foundation; either version 2, or (at your option) any later
version.

groff is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License along
with groff; see the file COPYING.  If not, write to the Free Software
Foundation, 51 Franklin St - Fifth Floor, Boston, MA 02110-1301, USA. */

#include "lib.h"

#include <stdlib.h>
#include <assert.h>
#include <errno.h>

#include "errarg.h"
#include "error.h"
#include "stringclass.h"
#include "cset.h"
#include "cmap.h"

#include "defs.h"

unsigned hash_string(const char *, int);
int next_size(int);

extern string capitalize_fields;
extern string reverse_fields;
extern string abbreviate_fields;
extern string period_before_last_name;
extern string period_before_initial;
extern string period_before_hyphen;
extern string period_before_other;
extern string sort_fields;
extern int annotation_field;
extern string annotation_macro;
extern string discard_fields;
extern string articles;
extern int abbreviate_label_ranges;
extern string label_range_indicator;
extern int date_as_label;
extern string join_authors_exactly_two;
extern string join_authors_last_two;
extern string join_authors_default;
extern string separate_label_second_parts;
extern string et_al;
extern int et_al_min_elide;
extern int et_al_min_total;

extern int compatible_flag;

extern int set_label_spec(const char *);
extern int set_date_label_spec(const char *);
extern int set_short_label_spec(const char *);

extern int short_label_flag;

void clear_labels();
void command_error(const char *,
		   const errarg &arg1 = empty_errarg,
		   const errarg &arg2 = empty_errarg,
		   const errarg &arg3 = empty_errarg);

class reference;

void compute_labels(reference **, int);
