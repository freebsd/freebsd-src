#!/bin/make
#
# Makefile for NetSurf buildsystem
#
# Copyright 2009-1013 John-Mark Bell <jmb@netsurf-browser.org>

# Component settings
COMPONENT := buildsystem
COMPONENT_VERSION := 1.1

.PHONY:all usage install dist clean distclean test

all: usage

usage:
	@echo "make install PREFIX=/somewhere"
	@echo ""
	@echo "Will install the makefiles, and test tools into \$$PREFIX/share/netsurf-buildsystem"
	@echo ""
	@echo "Which is where the libraries etc look for it."

PREFIX?=/opt/netsurf

BASE=$(DESTDIR)$(PREFIX)/share/netsurf-buildsystem

MAKEFILES := $(patsubst %,Makefile.%,	\
	top tools subdir pkgconfig 	\
	clang gcc norcroft open64 	\
)

TESTTOOLS := testrunner.pl

CITOOLS := jenkins-build.sh

install:
	mkdir -p $(BASE)/makefiles $(BASE)/testtools $(BASE)/citools
	for M in $(MAKEFILES); do \
		cp makefiles/$$M $(BASE)/makefiles/; \
	done
	for T in $(TESTTOOLS); do \
		cp testtools/$$T $(BASE)/testtools/; \
	done
	for C in $(CITOOLS); do \
		cp citools/$$C $(BASE)/citools/; \
	done

# Distribution
# This constructs a distribution tar from the last git tag. It ensures
#   the Makefile component version matches the git tag version and the 
#   tar is apropriately named for the component being generated
dist:
	$(eval GIT_TAG := $(shell git describe --abbrev=0 --match "release/*"))
	$(eval GIT_VER := $(shell x="$(GIT_TAG)"; echo "$${x#release/}"))
	$(if $(subst $(GIT_VER),,$(COMPONENT_VERSION)), $(error Component Version "$(COMPONENT_VERSION)" and GIT tag version "$(GIT_VER)" do not match))
	$(eval DIST_FILE := $(COMPONENT)-${GIT_VER})
	$(Q)git archive --format=tar.gz --prefix=$(DIST_FILE)/ -o $(DIST_FILE).tar.gz $(GIT_TAG)

# dummy clean target
clean:

# dummy distribution clean target
distclean:

# dummy test target
test:
