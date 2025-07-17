#-
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2023 Arm Ltd
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice unmodified, this list of conditions, and the following
#    disclaimer.
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

INTERFACE scmi;

HEADER {
	struct scmi_msg;
};

METHOD int transport_init {
	device_t dev;
};

METHOD void transport_cleanup {
	device_t dev;
};

METHOD int xfer_msg {
	device_t dev;
	struct scmi_msg *msg;
};

METHOD int poll_msg {
	device_t dev;
	struct scmi_msg *msg;
	unsigned int tmo;
};

METHOD int collect_reply {
	device_t dev;
	struct scmi_msg *msg;
};

METHOD void tx_complete {
	device_t dev;
	void *chan;
};

METHOD void clear_channel {
	device_t dev;
	void *chan;
};
