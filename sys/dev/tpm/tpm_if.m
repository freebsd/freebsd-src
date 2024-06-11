#-
# Copyright (c) 2023 Juniper Networks, Inc.
# All Rights Reserved
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
#

#include <sys/bus.h>
#include <dev/tpm/tpm20.h>

INTERFACE tpm;

#
# Transfer data to the TPM data buffer
#
METHOD int transmit {
	device_t dev;
	size_t length;
};


METHOD uint64_t read_8 {
	device_t dev;
	bus_size_t addr;
}

#
# Read 4 bytes (host endian) from a TPM register
#
METHOD uint32_t read_4 {
	device_t dev;
	bus_size_t addr;
};

METHOD uint8_t read_1 {
	device_t dev;
	bus_size_t addr;
};

METHOD void write_4 {
	device_t dev;
	bus_size_t addr;
	uint32_t value;
};

METHOD void write_1 {
	device_t dev;
	bus_size_t addr;
	uint8_t value;
};

METHOD void write_barrier {
	device_t dev;
	bus_size_t off;
	bus_size_t length;
}
