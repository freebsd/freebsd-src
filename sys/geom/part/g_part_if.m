#-
# Copyright (c) 2006, 2007 Marcel Moolenaar
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
# IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
# OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
# IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
# INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
# NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
# THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
# $FreeBSD: src/sys/geom/part/g_part_if.m,v 1.2 2007/02/08 04:02:56 rodrigc Exp $

#include <sys/param.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/sbuf.h>
#include <sys/bus.h>
#include <machine/bus.h>
#include <sys/systm.h>
#include <geom/geom.h>
#include <geom/part/g_part.h>

# The G_PART scheme interface.

INTERFACE g_part;

# add() - scheme specific processing for the add verb.
METHOD int add {
	struct g_part_table *table;
	struct g_part_entry *entry;
	struct g_part_parms *gpp;
};

# create() - scheme specific processing for the create verb.
METHOD int create {
	struct g_part_table *table;
	struct g_part_parms *gpp;
};

# destroy() - scheme specific processing for the destroy verb.
METHOD int destroy {
	struct g_part_table *table;
	struct g_part_parms *gpp;
};

# dumpconf()
METHOD void dumpconf {
	struct g_part_table *table;
	struct g_part_entry *entry;
	struct sbuf *sb;
	const char *indent;
};

# dumpto() - return whether the partiton can be used for kernel dumps.
METHOD int dumpto {
	struct g_part_table *table;
	struct g_part_entry *entry;
};

# modify() - scheme specific processing for the modify verb.
METHOD int modify {
	struct g_part_table *table;
	struct g_part_entry *entry;
	struct g_part_parms *gpp;
};

# name() - return the name of the given partition entry.
# Typical names are "p1", "s0" or "c".
METHOD const char * name {
	struct g_part_table *table;
	struct g_part_entry *entry;
	char *buf;
	size_t bufsz;
};

# probe() - probe the provider attached to the given consumer for the
# existence of the scheme implemented by the G_PART interface handler.
METHOD int probe {
	struct g_part_table *table;
	struct g_consumer *cp;
};

# read() - read the on-disk partition table into memory.
METHOD int read {
	struct g_part_table *table;
	struct g_consumer *cp;
};

# type() - return a string representation of the partition type.
# Preferrably, the alias names.
METHOD const char * type {
        struct g_part_table *table;
        struct g_part_entry *entry;
        char *buf;
        size_t bufsz;
};

# write() - write the in-memory partition table to disk.
METHOD int write {
	struct g_part_table *table;
	struct g_consumer *cp;
};
