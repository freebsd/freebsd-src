#-
# Copyright (c) 2010 Alexander Motin
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
# $FreeBSD$

#include <sys/param.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/sbuf.h>
#include <sys/bus.h>
#include <machine/bus.h>
#include <sys/systm.h>
#include <geom/geom.h>
#include <geom/raid/g_raid.h>

# The G_RAID metadata class interface.

INTERFACE g_raid_md;

# Default implementations of methods.
CODE {
};

HEADER {
#define G_RAID_MD_TASTE_FAIL		-1
#define G_RAID_MD_TASTE_EXISTING	 0
#define G_RAID_MD_TASTE_NEW		 1
};

# taste() - disk taste method.
METHOD int taste {
	struct g_raid_md_object *md;
	struct g_class *mp;
	struct g_consumer *cp;
	struct g_geom **gp;
};

# event() - events handling method.
METHOD int event {
	struct g_raid_md_object *tr;
	struct g_raid_disk *disk;
	u_int event;
};

# write() - metadata write method.
METHOD int write {
	struct g_raid_md_object *md;
	struct g_raid_disk *disk;
};

# free_disk() - disk destructor.
METHOD int free_disk {
	struct g_raid_md_object *md;
	struct g_raid_disk *disk;
};

# free() - destructor.
METHOD int free {
	struct g_raid_md_object *md;
};
