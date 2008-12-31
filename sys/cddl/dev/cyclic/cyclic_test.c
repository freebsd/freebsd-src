/*-
 * Copyright 2007 John Birrell <jb@FreeBSD.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: src/sys/cddl/dev/cyclic/cyclic_test.c,v 1.1.2.1.2.1 2008/11/25 02:59:29 kensmith Exp $
 *
 */

#include <sys/cdefs.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/conf.h>
#include <sys/kthread.h>
#include <sys/module.h>
#include <sys/sysctl.h>
#include <sys/cyclic.h>
#include <sys/time.h>

static struct timespec test_001_start;

static void
cyclic_test_001_func(void *arg)
{
	struct timespec ts;

	nanotime(&ts);
	timespecsub(&ts,&test_001_start);
	printf("%s: called after %lu.%09lu on curcpu %d\n",__func__,(u_long) ts.tv_sec,(u_long) ts.tv_nsec, curcpu);
}

static void
cyclic_test_001(void)
{
	int error = 0;
	cyc_handler_t hdlr;
	cyc_time_t when;
	cyclic_id_t id;

	printf("%s: starting\n",__func__);

	hdlr.cyh_func = (cyc_func_t) cyclic_test_001_func;
        hdlr.cyh_arg = 0;
 
        when.cyt_when = 0;
        when.cyt_interval = 1000000000;

	nanotime(&test_001_start);

	mutex_enter(&cpu_lock);

        id = cyclic_add(&hdlr, &when);

	mutex_exit(&cpu_lock);

	DELAY(1200000);

	mutex_enter(&cpu_lock);

	cyclic_remove(id);

	mutex_exit(&cpu_lock);

	printf("%s: %s\n",__func__, error == 0 ? "passed":"failed");
}

static struct timespec test_002_start;

static void
cyclic_test_002_func(void *arg)
{
	struct timespec ts;

	nanotime(&ts);
	timespecsub(&ts,&test_002_start);
	printf("%s: called after %lu.%09lu on curcpu %d\n",__func__,(u_long) ts.tv_sec,(u_long) ts.tv_nsec, curcpu);
}

static void
cyclic_test_002_online(void *arg, cpu_t *c, cyc_handler_t *hdlr, cyc_time_t *t)
{
	printf("%s: online on curcpu %d\n",__func__, curcpu);
	hdlr->cyh_func = cyclic_test_002_func;
	hdlr->cyh_arg = NULL;
	t->cyt_when = 0;
	t->cyt_interval = 1000000000;
}

static void
cyclic_test_002_offline(void *arg, cpu_t *c, void *arg1)
{
	printf("%s: offline on curcpu %d\n",__func__, curcpu);
}

static void
cyclic_test_002(void)
{
	int error = 0;
	cyc_omni_handler_t hdlr;
	cyclic_id_t id;

	printf("%s: starting\n",__func__);

	hdlr.cyo_online = cyclic_test_002_online;
	hdlr.cyo_offline = cyclic_test_002_offline;
	hdlr.cyo_arg = NULL;

	nanotime(&test_002_start);

	mutex_enter(&cpu_lock);

        id = cyclic_add_omni(&hdlr);

	mutex_exit(&cpu_lock);

	DELAY(1200000);

	mutex_enter(&cpu_lock);

	cyclic_remove(id);

	mutex_exit(&cpu_lock);

	printf("%s: %s\n",__func__, error == 0 ? "passed":"failed");
}

static struct timespec test_003_start;

static void
cyclic_test_003_func(void *arg)
{
	struct timespec ts;

	nanotime(&ts);
	timespecsub(&ts,&test_003_start);
	printf("%s: called after %lu.%09lu on curcpu %d id %ju\n",__func__,(u_long) ts.tv_sec,(u_long) ts.tv_nsec, curcpu, (uintmax_t)(uintptr_t) arg);
}

static void
cyclic_test_003(void)
{
	int error = 0;
	cyc_handler_t hdlr;
	cyc_time_t when;
	cyclic_id_t id;
	cyclic_id_t id1;
	cyclic_id_t id2;
	cyclic_id_t id3;

	printf("%s: starting\n",__func__);

	hdlr.cyh_func = (cyc_func_t) cyclic_test_003_func;
 
        when.cyt_when = 0;

	nanotime(&test_003_start);

	mutex_enter(&cpu_lock);

        when.cyt_interval = 200000000;
        hdlr.cyh_arg = (void *) 0UL;
        id = cyclic_add(&hdlr, &when);

        when.cyt_interval = 400000000;
        hdlr.cyh_arg = (void *) 1UL;
        id1 = cyclic_add(&hdlr, &when);

        hdlr.cyh_arg = (void *) 2UL;
        when.cyt_interval = 1000000000;
        id2 = cyclic_add(&hdlr, &when);

        hdlr.cyh_arg = (void *) 3UL;
        when.cyt_interval = 1300000000;
        id3 = cyclic_add(&hdlr, &when);

	mutex_exit(&cpu_lock);

	DELAY(1200000);

	mutex_enter(&cpu_lock);

	cyclic_remove(id);
	cyclic_remove(id1);
	cyclic_remove(id2);
	cyclic_remove(id3);

	mutex_exit(&cpu_lock);

	printf("%s: %s\n",__func__, error == 0 ? "passed":"failed");
}

/* Kernel thread command routine. */
static void
cyclic_run_tests(void *arg)
{
	intptr_t cmd = (intptr_t) arg;

	switch (cmd) {
	case 1:
		cyclic_test_001();
		break;
	case 2:
		cyclic_test_002();
		break;
	case 3:
		cyclic_test_003();
		break;
	default:
		cyclic_test_001();
		cyclic_test_002();
		cyclic_test_003();
		break;
	}

	printf("%s: finished\n",__func__);

	kthread_exit();
}

static int
cyclic_test(SYSCTL_HANDLER_ARGS)
{
	int error, cmd = 0;

	error = sysctl_wire_old_buffer(req, sizeof(int));
	if (error == 0)
		error = sysctl_handle_int(oidp, &cmd, 0, req);
	if (error != 0 || req->newptr == NULL)
		return (error);

	/* Check for command validity. */
	switch (cmd) {
	case 1:
	case 2:
	case -1:
		/*
		 * Execute the tests in a kernel thread to avoid blocking
		 * the sysctl. Look for the results in the syslog.
		 */
		error = kthread_add(cyclic_run_tests, (void *)(uintptr_t) cmd,
		    NULL, NULL, 0, 0, "cyctest%d", cmd);
		break;
	default:
		printf("Usage: debug.cyclic.test=(1..9) or -1 for all tests\n");
		error = EINVAL;
		break;
	}

	return (error);
}

SYSCTL_NODE(_debug, OID_AUTO, cyclic, CTLFLAG_RW, NULL, "Cyclic nodes");
SYSCTL_PROC(_debug_cyclic, OID_AUTO, test, CTLTYPE_INT | CTLFLAG_RW, 0, 0,
    cyclic_test, "I", "Enables a cyclic test. Use -1 for all tests.");

static int
cyclic_test_modevent(module_t mod, int type, void *data)
{
	int error = 0;

	switch (type) {
	case MOD_LOAD:
		break;

	case MOD_UNLOAD:
		break;

	case MOD_SHUTDOWN:
		break;

	default:
		error = EOPNOTSUPP;
		break;

	}
	return (error);
}

DEV_MODULE(cyclic_test, cyclic_test_modevent, NULL);
MODULE_VERSION(cyclic_test, 1);
MODULE_DEPEND(cyclic_test, cyclic, 1, 1, 1);
MODULE_DEPEND(cyclic_test, opensolaris, 1, 1, 1);
