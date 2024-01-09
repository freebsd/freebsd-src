#-
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2023 Arm Ltd
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
# ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#

INTERFACE vgic;

HEADER {
	struct hyp;
	struct hypctx;
	struct vm_vgic_descr;
};

METHOD void init {
	device_t dev;
}

METHOD int attach_to_vm {
	device_t dev;
	struct hyp *hyp;
	struct vm_vgic_descr *descr;
};

METHOD void detach_from_vm {
	device_t dev;
	struct hyp *hyp;
}

METHOD void vminit {
	device_t dev;
	struct hyp *hyp;
}

METHOD void cpuinit {
	device_t dev;
	struct hypctx *hypctx;
}

METHOD void cpucleanup {
	device_t dev;
	struct hypctx *hypctx;
}

METHOD void vmcleanup {
	device_t dev;
	struct hyp *hyp;
}

METHOD int max_cpu_count {
	device_t dev;
	struct hyp *hyp;
}

METHOD bool has_pending_irq {
	device_t dev;
	struct hypctx *hypctx;
}

METHOD int inject_irq {
	device_t dev;
	struct hyp *hyp;
	int vcpuid;
	uint32_t irqid;
	bool level;
}

METHOD int inject_msi {
	device_t dev;
	struct hyp *hyp;
	uint64_t msg;
	uint64_t addr;
}

METHOD void flush_hwstate {
	device_t dev;
	struct hypctx *hypctx;
}

METHOD void sync_hwstate {
	device_t dev;
	struct hypctx *hypctx;
}
