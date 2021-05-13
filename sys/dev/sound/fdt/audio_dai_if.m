#-
# Copyright (c) 2019 Oleksandr Tymoshenko <gonzo@FreeBSD.org>
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

CODE {
	#include <sys/param.h>
	#include <sys/bus.h>
	#include <dev/sound/pcm/sound.h>
}

INTERFACE audio_dai;

# set DAI format for communications between CPU/codec nodes
METHOD int init {
	device_t	dev;
	uint32_t	format;
}

# Initialize DAI and set up interrrupt handler
METHOD int setup_intr {
	device_t	dev;
	driver_intr_t	intr_handler;
	void		*intr_arg;
}

# Setup mixers for codec node
METHOD int setup_mixer {
	device_t	dev;
	device_t	ausocdev;
}

# setup clock speed
METHOD int set_sysclk {
	device_t	dev;
	uint32_t	rate;
	int		dai_dir;
}

METHOD int trigger {
	device_t	dev;
	int		go;
	int		pcm_dir;
}

METHOD struct pcmchan_caps* get_caps {
	device_t	dev;
}

METHOD uint32_t get_ptr {
	device_t	dev;
	int		pcm_dir;
}

# Set PCM channel format
METHOD uint32_t set_chanformat {
	device_t	dev;
	uint32_t	format;
}

# Set PCM channel sampling rate
METHOD uint32_t set_chanspeed {
	device_t	dev;
	uint32_t	speed;
}

# call DAI interrupt handler
# returns 1 if call to chn_intr required, 0 otherwise
METHOD int intr {
	device_t	dev;
	struct snd_dbuf	*play_buf;
	struct snd_dbuf	*rec_buf;
}
