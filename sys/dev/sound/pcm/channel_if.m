# KOBJ
#
# Copyright (c) 2000 Cameron Grant <cg@freebsd.org>
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#
# $FreeBSD$
#

#include <dev/sound/pcm/sound.h>

INTERFACE channel;

CODE {

	static int
	channel_nosetdir(kobj_t obj, void *data, int dir)
	{
		return 0;
	}

	static int
	channel_noreset(kobj_t obj, void *data)
	{
		return 0;
	}

	static int
	channel_noresetdone(kobj_t obj, void *data)
	{
		return 0;
	}

	static int
	channel_nofree(kobj_t obj, void *data)
	{
		return 1;
	}

};

METHOD void* init {
	kobj_t obj;
	void *devinfo;
	struct snd_dbuf *b;
	struct pcm_channel *c;
	int dir;
};

METHOD int free {
	kobj_t obj;
	void *data;
} DEFAULT channel_nofree;

METHOD int reset {
	kobj_t obj;
	void *data;
} DEFAULT channel_noreset;

METHOD int resetdone {
	kobj_t obj;
	void *data;
} DEFAULT channel_noresetdone;

METHOD int setdir {
	kobj_t obj;
	void *data;
	int dir;
} DEFAULT channel_nosetdir;

METHOD u_int32_t setformat {
	kobj_t obj;
	void *data;
	u_int32_t format;
};

METHOD u_int32_t setspeed {
	kobj_t obj;
	void *data;
	u_int32_t speed;
};

METHOD u_int32_t setblocksize {
	kobj_t obj;
	void *data;
	u_int32_t blocksize;
};

METHOD int trigger {
	kobj_t obj;
	void *data;
	int go;
};

METHOD u_int32_t getptr {
	kobj_t obj;
	void *data;
};

METHOD struct pcmchan_caps* getcaps {
	kobj_t obj;
	void *data;
};

