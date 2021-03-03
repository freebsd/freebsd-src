#!/bin/sh

#
# Copyright (c) 2008 Peter Holm <pho@FreeBSD.org>
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

# Test scenario by kib@freebsd.org

# Test of patch for Giant trick in cdevsw

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1
[ -d /usr/src/sys ] || exit 0
builddir=`sysctl kern.version | grep @ | sed 's/.*://'`
[ -d "$builddir" ] && export KERNBUILDDIR=$builddir || exit 0
export SYSDIR=`echo $builddir | sed 's#/sys.*#/sys#'`
kldstat -v | grep -q pty || { kldload pty || exit 0; }

. ../default.cfg

odir=`pwd`
dir=$RUNDIR/fpclone
[ ! -d $dir ] && mkdir -p $dir

cd $dir
cat > Makefile <<EOF
KMOD= fpclone
SRCS= fpclone.c

.include <bsd.kmod.mk>
EOF

sed '1,/^EOF2/d' < $odir/$0 > fpclone.c
make
kldload $dir/fpclone.ko

cd $odir
for i in `jot 10`; do
	dd if=/dev/fpclone bs=1m count=10 > /dev/null 2>&1 &
done

export runRUNTIME=2m
cd ..; ./run.sh pty.cfg

for i in `jot 10`; do
	wait
done
kldstat
dd if=/dev/fpclone bs=1m count=1k > /dev/null 2>&1 &
kldunload $dir/fpclone.ko
rm -rf $dir
exit

EOF2
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/conf.h>
#include <sys/uio.h>
#include <sys/malloc.h>

static d_open_t		fpclone_open;
static d_close_t	fpclone_close;
static d_read_t		fpclone_read;

static struct cdevsw fpclone_cdevsw = {
	.d_open =	fpclone_open,
	.d_close =	fpclone_close,
	.d_read =	fpclone_read,
	.d_name =	"fpclone",
	.d_version =	D_VERSION,
	.d_flags =	D_TRACKCLOSE
};

MALLOC_DEFINE(M_FPCLONESC, "fpclone memory", "fpclone memory");

struct fpclone_sc
{
	int pos;
};

static struct cdev *fpclone_dev;
static struct mtx me;

static void
fpclone_cdevpriv_dtr(void *data)
{
	free(data, M_FPCLONESC);
}

static int
fpclone_open(struct cdev *dev, int oflags, int devtype, struct thread *td)
{
	struct fpclone_sc *sc;
	int error;

	sc = malloc(sizeof(struct fpclone_sc), M_FPCLONESC,
	         M_WAITOK | M_ZERO);
	error = devfs_set_cdevpriv(sc, fpclone_cdevpriv_dtr);
	if (error)
		fpclone_cdevpriv_dtr(sc);
	return (error);
}

static int
fpclone_close(struct cdev *dev, int fflag, int devtype, struct thread *td)
{

	devfs_clear_cdevpriv();
	return (0);
}

static char rdata[] = "fpclone sample data string\n";

static int
fpclone_read(struct cdev *dev, struct uio *uio, int ioflag)
{
	struct fpclone_sc *sc;
	int rv, amnt, svpos, error;

	error = devfs_get_cdevpriv((void **)&sc);
	if (error)
		return (error);

	rv = 0;
	while (uio->uio_resid > 0) {
		svpos = sc->pos;
		amnt = MIN(uio->uio_resid, sizeof(rdata) - svpos);
		rv = uiomove(rdata + svpos, amnt, uio);
		if (rv != 0)
			break;
		mtx_lock(&me);
		sc->pos += amnt;
		sc->pos %= sizeof(rdata);
		mtx_unlock(&me);
	}
	return (rv);
}

static int
fpclone_modevent(module_t mod, int what, void *arg)
{
	switch (what) {
        case MOD_LOAD:
		mtx_init(&me, "fp_ref", NULL, MTX_DEF);
		fpclone_dev = make_dev(&fpclone_cdevsw, 0, 0, 0, 0666,
		    "fpclone");
		return(0);

        case MOD_UNLOAD:
		destroy_dev(fpclone_dev);
		mtx_destroy(&me);
		return (0);

        default:
		break;
	}

	return (0);
}

moduledata_t fpclone_mdata = {
	"fpclone",
	fpclone_modevent,
	NULL
};

DECLARE_MODULE(fpclone, fpclone_mdata, SI_SUB_DRIVERS, SI_ORDER_MIDDLE);
MODULE_VERSION(fpclone, 1);
