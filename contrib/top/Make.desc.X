# Makefile for .desc files

# This makefile is the prototype for "Make.desc", which is used by
# top's Configure script to build .desc files and the SYNOPSIS file.
# Configure then uses these files to ask appropriate questions.

# Written by William LeFebvre, Argonne National Laboratory
#	     (formerly of Northwestern University and Rice University)

# DO NOT EDIT "Make.desc"!!!  Make changes to "Make.desc.X",
# then "make veryclean", then run "Configure".

DESCS=%descs%

.SUFFIXES: .desc

.c.desc:
	sed -e '/^$$/,$$d' -e 's,^[/ *]*,,' $< > $@

all: SYNOPSIS

SYNOPSIS: $(DESCS)
	grep SYNOPSIS: $(DESCS) | sed -e 's@^machine/m_@@' -e 's@.desc:.[^:]*: *@:@' >SYNOPSIS
