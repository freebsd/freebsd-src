#-
# Copyright © 2024 Elliott Mitchell
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

INTERFACE pic;

HEADER {
	#include <machine/intr_machdep.h>
};

CODE {
	static void
	null_pic_generic(device_t pic)
	{
	}

	static int
	false_source_pending(device_t pic, struct intsrc *isrc)
	{
		return (0);
	}

	static void
	null_pic_resume(device_t pic, bool suspend_cancelled)
	{
	}

	static int
	nodev_config_intr(device_t pic, struct intsrc *isrc,
	    enum intr_trigger trigger, enum intr_polarity polarity)
	{
		return (ENODEV);
	}

	static void
	null_pic_reprogram_pin(device_t pic, struct intsrc *isrc)
	{
	}
}

METHOD void register_sources {
	device_t	pic;
} DEFAULT null_pic_generic;

METHOD void enable_source {
	device_t	pic;
	struct intsrc	*isrc;
};

METHOD void disable_source {
	device_t	pic;
	struct intsrc	*isrc;
	int		eoi;
};

METHOD void eoi_source {
	device_t	pic;
	struct intsrc	*isrc;
};

METHOD void enable_intr {
	device_t	pic;
	struct intsrc	*isrc;
};

METHOD void disable_intr {
	device_t	pic;
	struct intsrc	*isrc;
};

METHOD int source_pending {
	device_t	pic;
	struct intsrc	*isrc;
} DEFAULT false_source_pending;

METHOD void suspend {
	device_t	pic;
} DEFAULT null_pic_generic;

METHOD void resume {
	device_t	pic;
	bool suspend_cancelled;
} DEFAULT null_pic_resume;

METHOD int config_intr {
	device_t	pic;
	struct intsrc	*isrc;
	enum intr_trigger	trigger;
	enum intr_polarity	polarity;
} DEFAULT nodev_config_intr;

METHOD int assign_cpu {
	device_t	pic;
	struct intsrc	*isrc;
	u_int		apic_id;
};

METHOD void reprogram_pin {
	device_t	pic;
	struct intsrc	*isrc;
} DEFAULT null_pic_reprogram_pin;
