#
# Copyright (c) 1998 Nicolas Souchu
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
#	$Id: iicbus_if.m,v 1.1.2.4 1998/08/13 17:10:43 son Exp $
#

INTERFACE iicbus

#
# Interprete interrupt
#
METHOD int intr {
	device_t dev;
	int event;
	char *buf;
};

#
# Send REPEATED_START condition
#
METHOD int repeated_start {
	device_t dev;
	u_char slave;
};

#
# Send START condition
#
METHOD int start {
	device_t dev;
	u_char slave;
};

#
# Send STOP condition
#
METHOD int stop {
	device_t dev;
};

#
# Read from I2C bus
#
METHOD int read {
	device_t dev;
	char *buf;
	int len;
	int *bytes;
};

#
# Write to the I2C bus
#
METHOD int write {
	device_t dev;
	char *buf;
	int len;
	int *bytes;
};

#
# Reset I2C bus
#
METHOD int reset {
	device_t dev;
	u_char speed;
};
