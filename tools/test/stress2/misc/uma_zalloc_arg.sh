#!/bin/sh

#
# Copyright (c) 2016 EMC Corp.
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

# uma_zalloc_arc() fail point test scenario.
# Test scenario by Ryan Libby <rlibby@gmail.com>.

# "panic: backing_object 0xfffff8016dd74420 was somehow re-referenced during
#     collapse!" seen.
# https://people.freebsd.org/~pho/stress/log/uma_zalloc_arg.txt

# Hang seen:
# https://people.freebsd.org/~pho/stress/log/kostik869.txt

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

sysctl debug.mnowait_failure.zalloc_whitelist > /dev/null 2>&1 || exit 0

s1=`sysctl -n debug.mnowait_failure.zalloc_whitelist`
s2=`sysctl -n debug.fail_point.uma_zalloc_arg`
cleanup() {
	sysctl debug.mnowait_failure.zalloc_whitelist="$s1" > /dev/null
	sysctl debug.fail_point.uma_zalloc_arg="$s2" > /dev/null
}
trap "cleanup" EXIT INT
sysctl debug.mnowait_failure.zalloc_whitelist='RADIX NODE'
sysctl debug.fail_point.uma_zalloc_arg='1%return'

start=`date '+%s'`
while [ $((`date '+%s'` - start)) -lt 300 ]; do
	sh -c "echo | cat | cat > /dev/null"
done
exit 0

The patch from
https://github.com/freebsd/freebsd/compare/master...rlibby:mnowait-dbg

diff --git a/sys/kern/kern_malloc.c b/sys/kern/kern_malloc.c
index 01aff78..9d557a1 100644
--- a/sys/kern/kern_malloc.c
+++ b/sys/kern/kern_malloc.c
@@ -50,6 +50,7 @@ __FBSDID("$FreeBSD$");
 
 #include <sys/param.h>
 #include <sys/systm.h>
+#include <sys/fail.h>
 #include <sys/kdb.h>
 #include <sys/kernel.h>
 #include <sys/lock.h>
@@ -472,6 +473,19 @@ malloc(unsigned long size, struct malloc_type *mtp, int flags)
 		}
 	}
 #endif
+	if (flags & M_NOWAIT) {
+		KFAIL_POINT_CODE(DEBUG_FP, malloc, {
+			if (uma_dbg_nowait_fail_enabled_malloc(
+			    mtp->ks_shortdesc)) {
+				/* XXX record call stack */
+#ifdef MALLOC_MAKE_FAILURES
+				atomic_add_int(&malloc_failure_count, 1);
+				t_malloc_fail = time_uptime;
+#endif
+				return (NULL);
+			}
+		});
+	}
 	if (flags & M_WAITOK)
 		KASSERT(curthread->td_intr_nesting_level == 0,
 		   ("malloc(M_WAITOK) in interrupt context"));
diff --git a/sys/vm/uma_core.c b/sys/vm/uma_core.c
index 1f57dff..dfa18e6 100644
--- a/sys/vm/uma_core.c
+++ b/sys/vm/uma_core.c
@@ -64,6 +64,7 @@ __FBSDID("$FreeBSD$");
 #include <sys/param.h>
 #include <sys/systm.h>
 #include <sys/bitset.h>
+#include <sys/fail.h>
 #include <sys/kernel.h>
 #include <sys/types.h>
 #include <sys/queue.h>
@@ -2148,6 +2149,23 @@ uma_zalloc_arg(uma_zone_t zone, void *udata, int flags)
 	if (flags & M_WAITOK) {
 		WITNESS_WARN(WARN_GIANTOK | WARN_SLEEPOK, NULL,
 		    "uma_zalloc_arg: zone \"%s\"", zone->uz_name);
+	} else {
+		KFAIL_POINT_CODE(DEBUG_FP, uma_zalloc_arg, {
+			/*
+			 * XXX hack.  Setting the fail point to 0 (default)
+			 * causes it to ignore malloc zones, nonzero causes it
+			 * to inject failures for malloc zones regardless of
+			 * the malloc black/white lists.
+			 */
+			if (((zone->uz_flags & UMA_ZONE_MALLOC) == 0 ||
+			    RETURN_VALUE != 0) &&
+			    uma_dbg_nowait_fail_enabled_zalloc(
+			    zone->uz_name)) {
+				/* XXX record call stack */
+				atomic_add_long(&zone->uz_fails, 1);
+				return NULL;
+			}
+		});
 	}
 
 	KASSERT(curthread->td_critnest == 0,
diff --git a/sys/vm/uma_dbg.c b/sys/vm/uma_dbg.c
index 3fbd29b..bed3130 100644
--- a/sys/vm/uma_dbg.c
+++ b/sys/vm/uma_dbg.c
@@ -42,6 +42,8 @@ __FBSDID("$FreeBSD$");
 #include <sys/lock.h>
 #include <sys/mutex.h>
 #include <sys/malloc.h>
+#include <sys/rwlock.h>
+#include <sys/sysctl.h>
 
 #include <vm/vm.h>
 #include <vm/vm_object.h>
@@ -292,4 +294,143 @@ uma_dbg_free(uma_zone_t zone, uma_slab_t slab, void *item)
 	BIT_CLR_ATOMIC(SLAB_SETSIZE, freei, &slab->us_debugfree);
 }
 
+/* XXX explain */
+struct rwlock g_uma_dbg_nowait_lock;
+RW_SYSINIT(uma_dbg_nowait, &g_uma_dbg_nowait_lock, "uma dbg nowait");
+
+#define NOWAIT_FAIL_LIST_BUFSIZE 4096
+char malloc_fail_blacklist[NOWAIT_FAIL_LIST_BUFSIZE] = "kobj";
+char malloc_fail_whitelist[NOWAIT_FAIL_LIST_BUFSIZE] = "";
+char zalloc_fail_blacklist[NOWAIT_FAIL_LIST_BUFSIZE] =
+    "BUF TRIE,ata_request,sackhole";
+char zalloc_fail_whitelist[NOWAIT_FAIL_LIST_BUFSIZE] = "";
+
+static bool
+str_in_list(const char *list, char delim, const char *str)
+{
+       const char *b, *e;
+       size_t blen, slen;
+
+       b = list;
+       slen = strlen(str);
+       for (;;) {
+               e = strchr(b, delim);
+               blen = e == NULL ? strlen(b) : e - b;
+               if (blen == slen && strncmp(b, str, slen) == 0)
+                       return true;
+               if (e == NULL)
+                       break;
+               b = e + 1;
+       }
+       return false;
+}
+
+static bool
+uma_dbg_nowait_fail_enabled_internal(const char *blacklist,
+    const char *whitelist, const char *name)
+{
+	bool fail;
+
+	/* Protect ourselves from the sysctl handlers. */
+	rw_rlock(&g_uma_dbg_nowait_lock);
+	if (whitelist[0] == '\0')
+		fail = !str_in_list(blacklist, ',', name);
+	else
+		fail = str_in_list(whitelist, ',', name);
+	rw_runlock(&g_uma_dbg_nowait_lock);
+
+	return fail;
+}
+
+bool
+uma_dbg_nowait_fail_enabled_malloc(const char *name)
+{
+	return uma_dbg_nowait_fail_enabled_internal(malloc_fail_blacklist,
+	    malloc_fail_whitelist, name);
+}
+
+bool
+uma_dbg_nowait_fail_enabled_zalloc(const char *name)
+{
+	return uma_dbg_nowait_fail_enabled_internal(zalloc_fail_blacklist,
+	    zalloc_fail_whitelist, name);
+}
+
+/*
+ * XXX provide SYSCTL_STRING_LOCKED / sysctl_string_locked_handler?
+ * This is basically just a different sysctl_string_handler.  This one wraps
+ * the string manipulation in a lock and in a way that will not cause a sleep
+ * under that lock.
+ */
+static int
+sysctl_debug_mnowait_failure_list(SYSCTL_HANDLER_ARGS)
+{
+	char *newbuf = NULL;
+	int error, newlen;
+	bool have_lock = false;
+
+	if (req->newptr != NULL) {
+		newlen = req->newlen - req->newidx;
+		if (newlen >= arg2) {
+			error = EINVAL;
+			goto out;
+		}
+		newbuf = malloc(newlen, M_TEMP, M_WAITOK);
+		error = SYSCTL_IN(req, newbuf, newlen);
+		if (error != 0)
+			goto out;
+	}
+
+	error = sysctl_wire_old_buffer(req, arg2);
+	if (error != 0)
+		goto out;
+
+	rw_wlock(&g_uma_dbg_nowait_lock);
+	have_lock = true;
+
+	error = SYSCTL_OUT(req, arg1, strnlen(arg1, arg2 - 1) + 1);
+	if (error != 0)
+		goto out;
+
+	if (newbuf == NULL)
+		goto out;
+
+	bcopy(newbuf, arg1, newlen);
+	((char *)arg1)[newlen] = '\0';
+ out:
+	if (have_lock)
+		rw_wunlock(&g_uma_dbg_nowait_lock);
+	free(newbuf, M_TEMP);
+	return error;
+}
+
+SYSCTL_NODE(_debug, OID_AUTO, mnowait_failure, CTLFLAG_RW, 0,
+    "Control of M_NOWAIT memory allocation failure injection.");
+
+SYSCTL_PROC(_debug_mnowait_failure, OID_AUTO, malloc_blacklist,
+    CTLTYPE_STRING | CTLFLAG_RW | CTLFLAG_MPSAFE, malloc_fail_blacklist,
+    sizeof(malloc_fail_blacklist), sysctl_debug_mnowait_failure_list, "A",
+    "With debug.fail_point.malloc and with an empty whitelist, CSV list of "
+    "zones which remain unaffected.");
+
+SYSCTL_PROC(_debug_mnowait_failure, OID_AUTO, malloc_whitelist,
+    CTLTYPE_STRING | CTLFLAG_RW | CTLFLAG_MPSAFE, malloc_fail_whitelist,
+    sizeof(malloc_fail_whitelist), sysctl_debug_mnowait_failure_list, "A",
+    "With debug.fail_point.malloc, CSV list of zones exclusively affected.  "
+    "With an empty whitelist, all zones but those on the blacklist"
+    "are affected.");
+
+SYSCTL_PROC(_debug_mnowait_failure, OID_AUTO, zalloc_blacklist,
+    CTLTYPE_STRING | CTLFLAG_RW | CTLFLAG_MPSAFE, zalloc_fail_blacklist,
+    sizeof(zalloc_fail_blacklist), sysctl_debug_mnowait_failure_list, "A",
+    "With debug.fail_point.uma_zalloc_arg and with an empty whitelist, CSV "
+    "list of zones which remain unaffected.");
+
+SYSCTL_PROC(_debug_mnowait_failure, OID_AUTO, zalloc_whitelist,
+    CTLTYPE_STRING | CTLFLAG_RW | CTLFLAG_MPSAFE, zalloc_fail_whitelist,
+    sizeof(zalloc_fail_whitelist), sysctl_debug_mnowait_failure_list, "A",
+    "With debug.fail_point.uma_zalloc_arg, CSV list of zones exclusively "
+    "affected.  With an empty whitelist, all zones but those on the blacklist"
+    "are affected.");
+
 #endif /* INVARIANTS */
diff --git a/sys/vm/uma_int.h b/sys/vm/uma_int.h
index ad2a405..284747f 100644
--- a/sys/vm/uma_int.h
+++ b/sys/vm/uma_int.h
@@ -427,6 +427,9 @@ vsetslab(vm_offset_t va, uma_slab_t slab)
 void *uma_small_alloc(uma_zone_t zone, vm_size_t bytes, uint8_t *pflag,
     int wait);
 void uma_small_free(void *mem, vm_size_t size, uint8_t flags);
+
+bool uma_dbg_nowait_fail_enabled_malloc(const char *name);
+bool uma_dbg_nowait_fail_enabled_zalloc(const char *name);
 #endif /* _KERNEL */
 
 #endif /* VM_UMA_INT_H */
