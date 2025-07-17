#!/bin/sh

# Regression test for D33416 vm_fault: Fix vm_fault_populate()'s handling of VM_FAULT_WIRE
# Bug report: https://bugs.freebsd.org/bugzilla/show_bug.cgi?id=260347

# Test scenario by: martin

# Fixed by 88642d978a99

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1
[ -d /usr/src/sys ] || exit 0
builddir=`sysctl kern.version | grep @ | sed 's/.*://'`
[ -d "$builddir" ] && export KERNBUILDDIR=$builddir || exit 0
export SYSDIR=`echo $builddir | sed 's#/sys.*#/sys#'`

. ../default.cfg

odir=`pwd`
dir=$RUNDIR/skeleton
mkdir -p $dir

cd $dir
cat > skeleton.c <<EOF
/*
 * KLD Skeleton
 * Inspired by Andrew Reiter's Daemonnews article
 */

#include <sys/param.h>
#include <sys/module.h>
#include <sys/systm.h>  /* uprintf */
#include <sys/errno.h>
#include <sys/param.h>  /* defines used in kernel.h */
#include <sys/kernel.h> /* types used in module initialization */

#define BUFFER_SIZE 10*1000*1024 // 10 MB
static char gBuffer[BUFFER_SIZE];

static int demo_init (void)
{
  for (int i=0; i<BUFFER_SIZE; i++)
  {
    gBuffer[i] = 'A';
  }
  return 0;
}

static void demo_exit (void)
{
  for (int i=0; i<BUFFER_SIZE; i++)
  {
    gBuffer[i] += 1;
  }
}

/*
 * Load handler that deals with the loading and unloading of a KLD.
 */

static int
skel_loader(struct module *m, int what, void *arg)
{
  int err = 0;

  switch (what) {
  case MOD_LOAD:                /* kldload */
    uprintf("Skeleton KLD loaded.\n");
    demo_init();
    break;
  case MOD_UNLOAD:
    uprintf("Skeleton KLD unloaded.\n");
    demo_exit();
    break;
  default:
    err = EOPNOTSUPP;
    break;
  }
  return(err);
}

/* Declare this module to the rest of the kernel */

static moduledata_t skel_mod = {
  "skel",
  skel_loader,
  NULL
};

DECLARE_MODULE(skeleton, skel_mod, SI_SUB_KLD, SI_ORDER_ANY);
EOF

cat > Makefile <<EOF
KMOD= skeleton
SRCS= skeleton.c

.include <bsd.kmod.mk>
EOF

make
old=`sysctl -n vm.stats.vm.v_wire_count`
kldload $dir/skeleton.ko
kldunload $dir/skeleton.ko
leak=$((`sysctl -n vm.stats.vm.v_wire_count` - old))
if [ $leak -gt 25 ]; then
	echo "Test leaked $leak pages in the skeleton.ko module"
	s=1
else
	s=0
fi

cd $odir
rm -rf $dir
exit $s
