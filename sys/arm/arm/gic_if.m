#-
# Copyright (c) 2021 The FreeBSD Foundation
#
# This software was developed by Andrew Turner under
# sponsorship from the FreeBSD Foundation.
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
# $FreeBSD$
#


INTERFACE gic;

HEADER {
	struct intr_irqsrc;
};

METHOD void reserve_msi_range {
	device_t	dev;
	u_int		mbi_start;
	u_int		mbi_count;
};

METHOD int alloc_msi {
	device_t	dev;
	u_int		mbi_start;
	u_int		mbi_count;
	int		count;
	int		maxcount;
	struct intr_irqsrc **isrc;
};

METHOD int release_msi {
	device_t	dev;
	int		count;
	struct intr_irqsrc **isrc;
};

METHOD int alloc_msix {
	device_t	dev;
	u_int		mbi_start;
	u_int		mbi_count;
	struct intr_irqsrc **isrc;
};

METHOD int release_msix {
	device_t	dev;
	struct intr_irqsrc *isrc;
};

METHOD void db_show {
	device_t	dev;
};
