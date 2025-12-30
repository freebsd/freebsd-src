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

INTERFACE intr_event;

#include <sys/bus.h>

HEADER {
	#include <machine/interrupt.h>
}

CODE {
	static void
	empty_intr_event_hook(device_t pic, interrupt_t *intr)
	{
		panic("%s: %s missing intr_event hook",
		    device_get_nameunit(pic), __func__);
	}

	static int
	empty_assign_cpu(device_t pic, interrupt_t *intr, unsigned int cpu)
	{
		panic("%s: %s missing intr_event hook",
		    device_get_nameunit(pic), __func__);
	}
}

METHOD void pre_ithread {
	device_t	pic;
	interrupt_t	*intr;
} DEFAULT empty_intr_event_hook;

METHOD void post_ithread {
	device_t	pic;
	interrupt_t	*intr;
} DEFAULT empty_intr_event_hook;

METHOD void post_filter {
	device_t	pic;
	interrupt_t	*intr;
} DEFAULT empty_intr_event_hook;

METHOD int assign_cpu {
	device_t	pic;
	interrupt_t	*intr;
	unsigned int	cpu;
} DEFAULT empty_assign_cpu;
