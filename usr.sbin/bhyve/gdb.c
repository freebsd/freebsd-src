/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2017-2018 John H. Baldwin <jhb@FreeBSD.org>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#include <sys/param.h>
#ifndef WITHOUT_CAPSICUM
#include <sys/capsicum.h>
#endif
#include <sys/endian.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <machine/atomic.h>
#include <machine/specialreg.h>
#include <machine/vmm.h>
#include <netinet/in.h>
#include <assert.h>
#ifndef WITHOUT_CAPSICUM
#include <capsicum_helpers.h>
#endif
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <pthread.h>
#include <pthread_np.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>
#include <vmmapi.h>

#include "bhyverun.h"
#include "config.h"
#include "debug.h"
#include "gdb.h"
#include "mem.h"
#include "mevent.h"

/*
 * GDB_SIGNAL_* numbers are part of the GDB remote protocol.  Most stops
 * use SIGTRAP.
 */
#define	GDB_SIGNAL_TRAP		5

#define	GDB_BP_SIZE		1
#define	GDB_BP_INSTR		(uint8_t []){0xcc}
#define	GDB_PC_REGNAME		VM_REG_GUEST_RIP

_Static_assert(sizeof(GDB_BP_INSTR) == GDB_BP_SIZE,
    "GDB_BP_INSTR has wrong size");

static void gdb_resume_vcpus(void);
static void check_command(int fd);

static struct mevent *read_event, *write_event;

static cpuset_t vcpus_active, vcpus_suspended, vcpus_waiting;
static pthread_mutex_t gdb_lock;
static pthread_cond_t idle_vcpus;
static bool first_stop, report_next_stop, swbreak_enabled;

/*
 * An I/O buffer contains 'capacity' bytes of room at 'data'.  For a
 * read buffer, 'start' is unused and 'len' contains the number of
 * valid bytes in the buffer.  For a write buffer, 'start' is set to
 * the index of the next byte in 'data' to send, and 'len' contains
 * the remaining number of valid bytes to send.
 */
struct io_buffer {
	uint8_t *data;
	size_t capacity;
	size_t start;
	size_t len;
};

struct breakpoint {
	uint64_t gpa;
	uint8_t shadow_inst[GDB_BP_SIZE];
	TAILQ_ENTRY(breakpoint) link;
};

/*
 * When a vCPU stops to due to an event that should be reported to the
 * debugger, information about the event is stored in this structure.
 * The vCPU thread then sets 'stopped_vcpu' if it is not already set
 * and stops other vCPUs so the event can be reported.  The
 * report_stop() function reports the event for the 'stopped_vcpu'
 * vCPU.  When the debugger resumes execution via continue or step,
 * the event for 'stopped_vcpu' is cleared.  vCPUs will loop in their
 * event handlers until the associated event is reported or disabled.
 *
 * An idle vCPU will have all of the boolean fields set to false.
 *
 * When a vCPU is stepped, 'stepping' is set to true when the vCPU is
 * released to execute the stepped instruction.  When the vCPU reports
 * the stepping trap, 'stepped' is set.
 *
 * When a vCPU hits a breakpoint set by the debug server,
 * 'hit_swbreak' is set to true.
 */
struct vcpu_state {
	bool stepping;
	bool stepped;
	bool hit_swbreak;
};

static struct io_buffer cur_comm, cur_resp;
static uint8_t cur_csum;
static struct vmctx *ctx;
static int cur_fd = -1;
static TAILQ_HEAD(, breakpoint) breakpoints;
static struct vcpu_state *vcpu_state;
static struct vcpu **vcpus;
static int cur_vcpu, stopped_vcpu;
static bool gdb_active = false;

static const struct gdb_reg {
	enum vm_reg_name id;
	int size;
} gdb_regset[] = {
	{ .id = VM_REG_GUEST_RAX, .size = 8 },
	{ .id = VM_REG_GUEST_RBX, .size = 8 },
	{ .id = VM_REG_GUEST_RCX, .size = 8 },
	{ .id = VM_REG_GUEST_RDX, .size = 8 },
	{ .id = VM_REG_GUEST_RSI, .size = 8 },
	{ .id = VM_REG_GUEST_RDI, .size = 8 },
	{ .id = VM_REG_GUEST_RBP, .size = 8 },
	{ .id = VM_REG_GUEST_RSP, .size = 8 },
	{ .id = VM_REG_GUEST_R8, .size = 8 },
	{ .id = VM_REG_GUEST_R9, .size = 8 },
	{ .id = VM_REG_GUEST_R10, .size = 8 },
	{ .id = VM_REG_GUEST_R11, .size = 8 },
	{ .id = VM_REG_GUEST_R12, .size = 8 },
	{ .id = VM_REG_GUEST_R13, .size = 8 },
	{ .id = VM_REG_GUEST_R14, .size = 8 },
	{ .id = VM_REG_GUEST_R15, .size = 8 },
	{ .id = VM_REG_GUEST_RIP, .size = 8 },
	{ .id = VM_REG_GUEST_RFLAGS, .size = 4 },
	{ .id = VM_REG_GUEST_CS, .size = 4 },
	{ .id = VM_REG_GUEST_SS, .size = 4 },
	{ .id = VM_REG_GUEST_DS, .size = 4 },
	{ .id = VM_REG_GUEST_ES, .size = 4 },
	{ .id = VM_REG_GUEST_FS, .size = 4 },
	{ .id = VM_REG_GUEST_GS, .size = 4 },
};

#ifdef GDB_LOG
#include <stdarg.h>
#include <stdio.h>

static void __printflike(1, 2)
debug(const char *fmt, ...)
{
	static FILE *logfile;
	va_list ap;

	if (logfile == NULL) {
		logfile = fopen("/tmp/bhyve_gdb.log", "w");
		if (logfile == NULL)
			return;
#ifndef WITHOUT_CAPSICUM
		if (caph_limit_stream(fileno(logfile), CAPH_WRITE) == -1) {
			fclose(logfile);
			logfile = NULL;
			return;
		}
#endif
		setlinebuf(logfile);
	}
	va_start(ap, fmt);
	vfprintf(logfile, fmt, ap);
	va_end(ap);
}
#else
#define debug(...)
#endif

static void	remove_all_sw_breakpoints(void);

static int
guest_paging_info(struct vcpu *vcpu, struct vm_guest_paging *paging)
{
	uint64_t regs[4];
	const int regset[4] = {
		VM_REG_GUEST_CR0,
		VM_REG_GUEST_CR3,
		VM_REG_GUEST_CR4,
		VM_REG_GUEST_EFER
	};

	if (vm_get_register_set(vcpu, nitems(regset), regset, regs) == -1)
		return (-1);

	/*
	 * For the debugger, always pretend to be the kernel (CPL 0),
	 * and if long-mode is enabled, always parse addresses as if
	 * in 64-bit mode.
	 */
	paging->cr3 = regs[1];
	paging->cpl = 0;
	if (regs[3] & EFER_LMA)
		paging->cpu_mode = CPU_MODE_64BIT;
	else if (regs[0] & CR0_PE)
		paging->cpu_mode = CPU_MODE_PROTECTED;
	else
		paging->cpu_mode = CPU_MODE_REAL;
	if (!(regs[0] & CR0_PG))
		paging->paging_mode = PAGING_MODE_FLAT;
	else if (!(regs[2] & CR4_PAE))
		paging->paging_mode = PAGING_MODE_32;
	else if (regs[3] & EFER_LME)
		paging->paging_mode = (regs[2] & CR4_LA57) ?
		    PAGING_MODE_64_LA57 :  PAGING_MODE_64;
	else
		paging->paging_mode = PAGING_MODE_PAE;
	return (0);
}

/*
 * Map a guest virtual address to a physical address (for a given vcpu).
 * If a guest virtual address is valid, return 1.  If the address is
 * not valid, return 0.  If an error occurs obtaining the mapping,
 * return -1.
 */
static int
guest_vaddr2paddr(struct vcpu *vcpu, uint64_t vaddr, uint64_t *paddr)
{
	struct vm_guest_paging paging;
	int fault;

	if (guest_paging_info(vcpu, &paging) == -1)
		return (-1);

	/*
	 * Always use PROT_READ.  We really care if the VA is
	 * accessible, not if the current vCPU can write.
	 */
	if (vm_gla2gpa_nofault(vcpu, &paging, vaddr, PROT_READ, paddr,
	    &fault) == -1)
		return (-1);
	if (fault)
		return (0);
	return (1);
}

static uint64_t
guest_pc(struct vm_exit *vme)
{
	return (vme->rip);
}

static void
io_buffer_reset(struct io_buffer *io)
{

	io->start = 0;
	io->len = 0;
}

/* Available room for adding data. */
static size_t
io_buffer_avail(struct io_buffer *io)
{

	return (io->capacity - (io->start + io->len));
}

static uint8_t *
io_buffer_head(struct io_buffer *io)
{

	return (io->data + io->start);
}

static uint8_t *
io_buffer_tail(struct io_buffer *io)
{

	return (io->data + io->start + io->len);
}

static void
io_buffer_advance(struct io_buffer *io, size_t amount)
{

	assert(amount <= io->len);
	io->start += amount;
	io->len -= amount;
}

static void
io_buffer_consume(struct io_buffer *io, size_t amount)
{

	io_buffer_advance(io, amount);
	if (io->len == 0) {
		io->start = 0;
		return;
	}

	/*
	 * XXX: Consider making this move optional and compacting on a
	 * future read() before realloc().
	 */
	memmove(io->data, io_buffer_head(io), io->len);
	io->start = 0;
}

static void
io_buffer_grow(struct io_buffer *io, size_t newsize)
{
	uint8_t *new_data;
	size_t avail, new_cap;

	avail = io_buffer_avail(io);
	if (newsize <= avail)
		return;

	new_cap = io->capacity + (newsize - avail);
	new_data = realloc(io->data, new_cap);
	if (new_data == NULL)
		err(1, "Failed to grow GDB I/O buffer");
	io->data = new_data;
	io->capacity = new_cap;
}

static bool
response_pending(void)
{

	if (cur_resp.start == 0 && cur_resp.len == 0)
		return (false);
	if (cur_resp.start + cur_resp.len == 1 && cur_resp.data[0] == '+')
		return (false);
	return (true);
}

static void
close_connection(void)
{

	/*
	 * XXX: This triggers a warning because mevent does the close
	 * before the EV_DELETE.
	 */
	pthread_mutex_lock(&gdb_lock);
	mevent_delete(write_event);
	mevent_delete_close(read_event);
	write_event = NULL;
	read_event = NULL;
	io_buffer_reset(&cur_comm);
	io_buffer_reset(&cur_resp);
	cur_fd = -1;

	remove_all_sw_breakpoints();

	/* Clear any pending events. */
	memset(vcpu_state, 0, guest_ncpus * sizeof(*vcpu_state));

	/* Resume any stopped vCPUs. */
	gdb_resume_vcpus();
	pthread_mutex_unlock(&gdb_lock);
}

static uint8_t
hex_digit(uint8_t nibble)
{

	if (nibble <= 9)
		return (nibble + '0');
	else
		return (nibble + 'a' - 10);
}

static uint8_t
parse_digit(uint8_t v)
{

	if (v >= '0' && v <= '9')
		return (v - '0');
	if (v >= 'a' && v <= 'f')
		return (v - 'a' + 10);
	if (v >= 'A' && v <= 'F')
		return (v - 'A' + 10);
	return (0xF);
}

/* Parses big-endian hexadecimal. */
static uintmax_t
parse_integer(const uint8_t *p, size_t len)
{
	uintmax_t v;

	v = 0;
	while (len > 0) {
		v <<= 4;
		v |= parse_digit(*p);
		p++;
		len--;
	}
	return (v);
}

static uint8_t
parse_byte(const uint8_t *p)
{

	return (parse_digit(p[0]) << 4 | parse_digit(p[1]));
}

static void
send_pending_data(int fd)
{
	ssize_t nwritten;

	if (cur_resp.len == 0) {
		mevent_disable(write_event);
		return;
	}
	nwritten = write(fd, io_buffer_head(&cur_resp), cur_resp.len);
	if (nwritten == -1) {
		warn("Write to GDB socket failed");
		close_connection();
	} else {
		io_buffer_advance(&cur_resp, nwritten);
		if (cur_resp.len == 0)
			mevent_disable(write_event);
		else
			mevent_enable(write_event);
	}
}

/* Append a single character to the output buffer. */
static void
send_char(uint8_t data)
{
	io_buffer_grow(&cur_resp, 1);
	*io_buffer_tail(&cur_resp) = data;
	cur_resp.len++;
}

/* Append an array of bytes to the output buffer. */
static void
send_data(const uint8_t *data, size_t len)
{

	io_buffer_grow(&cur_resp, len);
	memcpy(io_buffer_tail(&cur_resp), data, len);
	cur_resp.len += len;
}

static void
format_byte(uint8_t v, uint8_t *buf)
{

	buf[0] = hex_digit(v >> 4);
	buf[1] = hex_digit(v & 0xf);
}

/*
 * Append a single byte (formatted as two hex characters) to the
 * output buffer.
 */
static void
send_byte(uint8_t v)
{
	uint8_t buf[2];

	format_byte(v, buf);
	send_data(buf, sizeof(buf));
}

static void
start_packet(void)
{

	send_char('$');
	cur_csum = 0;
}

static void
finish_packet(void)
{

	send_char('#');
	send_byte(cur_csum);
	debug("-> %.*s\n", (int)cur_resp.len, io_buffer_head(&cur_resp));
}

/*
 * Append a single character (for the packet payload) and update the
 * checksum.
 */
static void
append_char(uint8_t v)
{

	send_char(v);
	cur_csum += v;
}

/*
 * Append an array of bytes (for the packet payload) and update the
 * checksum.
 */
static void
append_packet_data(const uint8_t *data, size_t len)
{

	send_data(data, len);
	while (len > 0) {
		cur_csum += *data;
		data++;
		len--;
	}
}

static void
append_string(const char *str)
{

	append_packet_data(str, strlen(str));
}

static void
append_byte(uint8_t v)
{
	uint8_t buf[2];

	format_byte(v, buf);
	append_packet_data(buf, sizeof(buf));
}

static void
append_unsigned_native(uintmax_t value, size_t len)
{
	size_t i;

	for (i = 0; i < len; i++) {
		append_byte(value);
		value >>= 8;
	}
}

static void
append_unsigned_be(uintmax_t value, size_t len)
{
	char buf[len * 2];
	size_t i;

	for (i = 0; i < len; i++) {
		format_byte(value, buf + (len - i - 1) * 2);
		value >>= 8;
	}
	append_packet_data(buf, sizeof(buf));
}

static void
append_integer(unsigned int value)
{

	if (value == 0)
		append_char('0');
	else
		append_unsigned_be(value, (fls(value) + 7) / 8);
}

static void
append_asciihex(const char *str)
{

	while (*str != '\0') {
		append_byte(*str);
		str++;
	}
}

static void
send_empty_response(void)
{

	start_packet();
	finish_packet();
}

static void
send_error(int error)
{

	start_packet();
	append_char('E');
	append_byte(error);
	finish_packet();
}

static void
send_ok(void)
{

	start_packet();
	append_string("OK");
	finish_packet();
}

static int
parse_threadid(const uint8_t *data, size_t len)
{

	if (len == 1 && *data == '0')
		return (0);
	if (len == 2 && memcmp(data, "-1", 2) == 0)
		return (-1);
	if (len == 0)
		return (-2);
	return (parse_integer(data, len));
}

/*
 * Report the current stop event to the debugger.  If the stop is due
 * to an event triggered on a specific vCPU such as a breakpoint or
 * stepping trap, stopped_vcpu will be set to the vCPU triggering the
 * stop.  If 'set_cur_vcpu' is true, then cur_vcpu will be updated to
 * the reporting vCPU for vCPU events.
 */
static void
report_stop(bool set_cur_vcpu)
{
	struct vcpu_state *vs;

	start_packet();
	if (stopped_vcpu == -1) {
		append_char('S');
		append_byte(GDB_SIGNAL_TRAP);
	} else {
		vs = &vcpu_state[stopped_vcpu];
		if (set_cur_vcpu)
			cur_vcpu = stopped_vcpu;
		append_char('T');
		append_byte(GDB_SIGNAL_TRAP);
		append_string("thread:");
		append_integer(stopped_vcpu + 1);
		append_char(';');
		if (vs->hit_swbreak) {
			debug("$vCPU %d reporting swbreak\n", stopped_vcpu);
			if (swbreak_enabled)
				append_string("swbreak:;");
		} else if (vs->stepped)
			debug("$vCPU %d reporting step\n", stopped_vcpu);
		else
			debug("$vCPU %d reporting ???\n", stopped_vcpu);
	}
	finish_packet();
	report_next_stop = false;
}

/*
 * If this stop is due to a vCPU event, clear that event to mark it as
 * acknowledged.
 */
static void
discard_stop(void)
{
	struct vcpu_state *vs;

	if (stopped_vcpu != -1) {
		vs = &vcpu_state[stopped_vcpu];
		vs->hit_swbreak = false;
		vs->stepped = false;
		stopped_vcpu = -1;
	}
	report_next_stop = true;
}

static void
gdb_finish_suspend_vcpus(void)
{

	if (first_stop) {
		first_stop = false;
		stopped_vcpu = -1;
	} else if (report_next_stop) {
		assert(!response_pending());
		report_stop(true);
		send_pending_data(cur_fd);
	}
}

/*
 * vCPU threads invoke this function whenever the vCPU enters the
 * debug server to pause or report an event.  vCPU threads wait here
 * as long as the debug server keeps them suspended.
 */
static void
_gdb_cpu_suspend(struct vcpu *vcpu, bool report_stop)
{
	int vcpuid = vcpu_id(vcpu);

	debug("$vCPU %d suspending\n", vcpuid);
	CPU_SET(vcpuid, &vcpus_waiting);
	if (report_stop && CPU_CMP(&vcpus_waiting, &vcpus_suspended) == 0)
		gdb_finish_suspend_vcpus();
	while (CPU_ISSET(vcpuid, &vcpus_suspended))
		pthread_cond_wait(&idle_vcpus, &gdb_lock);
	CPU_CLR(vcpuid, &vcpus_waiting);
	debug("$vCPU %d resuming\n", vcpuid);
}

/*
 * Requests vCPU single-stepping using a
 * VMEXIT suitable for the host platform.
 */
static int
_gdb_set_step(struct vcpu *vcpu, int val)
{
	int error;

	/*
	 * If the MTRAP cap fails, we are running on an AMD host.
	 * In that case, we request DB exits caused by RFLAGS.TF.
	 */
	error = vm_set_capability(vcpu, VM_CAP_MTRAP_EXIT, val);
	if (error != 0)
		error = vm_set_capability(vcpu, VM_CAP_RFLAGS_TF, val);
	if (error == 0)
		(void)vm_set_capability(vcpu, VM_CAP_MASK_HWINTR, val);

	return (error);
}

/*
 * Checks whether single-stepping is enabled for a given vCPU.
 */
static int
_gdb_check_step(struct vcpu *vcpu)
{
	int val;

	if (vm_get_capability(vcpu, VM_CAP_MTRAP_EXIT, &val) != 0) {
		if (vm_get_capability(vcpu, VM_CAP_RFLAGS_TF, &val) != 0)
			return -1;
	}
	return 0;
}

/*
 * Invoked at the start of a vCPU thread's execution to inform the
 * debug server about the new thread.
 */
void
gdb_cpu_add(struct vcpu *vcpu)
{
	int vcpuid;

	if (!gdb_active)
		return;
	vcpuid = vcpu_id(vcpu);
	debug("$vCPU %d starting\n", vcpuid);
	pthread_mutex_lock(&gdb_lock);
	assert(vcpuid < guest_ncpus);
	assert(vcpus[vcpuid] == NULL);
	vcpus[vcpuid] = vcpu;
	CPU_SET(vcpuid, &vcpus_active);
	if (!TAILQ_EMPTY(&breakpoints)) {
		vm_set_capability(vcpu, VM_CAP_BPT_EXIT, 1);
		debug("$vCPU %d enabled breakpoint exits\n", vcpuid);
	}

	/*
	 * If a vcpu is added while vcpus are stopped, suspend the new
	 * vcpu so that it will pop back out with a debug exit before
	 * executing the first instruction.
	 */
	if (!CPU_EMPTY(&vcpus_suspended)) {
		CPU_SET(vcpuid, &vcpus_suspended);
		_gdb_cpu_suspend(vcpu, false);
	}
	pthread_mutex_unlock(&gdb_lock);
}

/*
 * Invoked by vCPU before resuming execution.  This enables stepping
 * if the vCPU is marked as stepping.
 */
static void
gdb_cpu_resume(struct vcpu *vcpu)
{
	struct vcpu_state *vs;
	int error;

	vs = &vcpu_state[vcpu_id(vcpu)];

	/*
	 * Any pending event should already be reported before
	 * resuming.
	 */
	assert(vs->hit_swbreak == false);
	assert(vs->stepped == false);
	if (vs->stepping) {
		error = _gdb_set_step(vcpu, 1);
		assert(error == 0);
	}
}

/*
 * Handler for VM_EXITCODE_DEBUG used to suspend a vCPU when the guest
 * has been suspended due to an event on different vCPU or in response
 * to a guest-wide suspend such as Ctrl-C or the stop on attach.
 */
void
gdb_cpu_suspend(struct vcpu *vcpu)
{

	if (!gdb_active)
		return;
	pthread_mutex_lock(&gdb_lock);
	_gdb_cpu_suspend(vcpu, true);
	gdb_cpu_resume(vcpu);
	pthread_mutex_unlock(&gdb_lock);
}

static void
gdb_suspend_vcpus(void)
{

	assert(pthread_mutex_isowned_np(&gdb_lock));
	debug("suspending all CPUs\n");
	vcpus_suspended = vcpus_active;
	vm_suspend_all_cpus(ctx);
	if (CPU_CMP(&vcpus_waiting, &vcpus_suspended) == 0)
		gdb_finish_suspend_vcpus();
}

/*
 * Invoked each time a vmexit handler needs to step a vCPU.
 * Handles MTRAP and RFLAGS.TF vmexits.
 */
static void
gdb_cpu_step(struct vcpu *vcpu)
{
	struct vcpu_state *vs;
	int vcpuid = vcpu_id(vcpu);
	int error;

	debug("$vCPU %d stepped\n", vcpuid);
	pthread_mutex_lock(&gdb_lock);
	vs = &vcpu_state[vcpuid];
	if (vs->stepping) {
		vs->stepping = false;
		vs->stepped = true;
		error = _gdb_set_step(vcpu, 0);
		assert(error == 0);

		while (vs->stepped) {
			if (stopped_vcpu == -1) {
				debug("$vCPU %d reporting step\n", vcpuid);
				stopped_vcpu = vcpuid;
				gdb_suspend_vcpus();
			}
			_gdb_cpu_suspend(vcpu, true);
		}
		gdb_cpu_resume(vcpu);
	}
	pthread_mutex_unlock(&gdb_lock);
}

/*
 * A general handler for VM_EXITCODE_DB.
 * Handles RFLAGS.TF exits on AMD SVM.
 */
void
gdb_cpu_debug(struct vcpu *vcpu, struct vm_exit *vmexit)
{
	if (!gdb_active)
		return;

	/* RFLAGS.TF exit? */
	if (vmexit->u.dbg.trace_trap) {
		gdb_cpu_step(vcpu);
	}
}

/*
 * Handler for VM_EXITCODE_MTRAP reported when a vCPU single-steps via
 * the VT-x-specific MTRAP exit.
 */
void
gdb_cpu_mtrap(struct vcpu *vcpu)
{
	if (!gdb_active)
		return;
	gdb_cpu_step(vcpu);
}

static struct breakpoint *
find_breakpoint(uint64_t gpa)
{
	struct breakpoint *bp;

	TAILQ_FOREACH(bp, &breakpoints, link) {
		if (bp->gpa == gpa)
			return (bp);
	}
	return (NULL);
}

void
gdb_cpu_breakpoint(struct vcpu *vcpu, struct vm_exit *vmexit)
{
	struct breakpoint *bp;
	struct vcpu_state *vs;
	uint64_t gpa;
	int error, vcpuid;

	if (!gdb_active) {
		EPRINTLN("vm_loop: unexpected VMEXIT_DEBUG");
		exit(4);
	}
	vcpuid = vcpu_id(vcpu);
	pthread_mutex_lock(&gdb_lock);
	error = guest_vaddr2paddr(vcpu, guest_pc(vmexit), &gpa);
	assert(error == 1);
	bp = find_breakpoint(gpa);
	if (bp != NULL) {
		vs = &vcpu_state[vcpuid];
		assert(vs->stepping == false);
		assert(vs->stepped == false);
		assert(vs->hit_swbreak == false);
		vs->hit_swbreak = true;
		vm_set_register(vcpu, GDB_PC_REGNAME, guest_pc(vmexit));
		for (;;) {
			if (stopped_vcpu == -1) {
				debug("$vCPU %d reporting breakpoint at rip %#lx\n",
				    vcpuid, guest_pc(vmexit));
				stopped_vcpu = vcpuid;
				gdb_suspend_vcpus();
			}
			_gdb_cpu_suspend(vcpu, true);
			if (!vs->hit_swbreak) {
				/* Breakpoint reported. */
				break;
			}
			bp = find_breakpoint(gpa);
			if (bp == NULL) {
				/* Breakpoint was removed. */
				vs->hit_swbreak = false;
				break;
			}
		}
		gdb_cpu_resume(vcpu);
	} else {
		debug("$vCPU %d injecting breakpoint at rip %#lx\n", vcpuid,
		    guest_pc(vmexit));
		error = vm_set_register(vcpu, VM_REG_GUEST_ENTRY_INST_LENGTH,
		    vmexit->u.bpt.inst_length);
		assert(error == 0);
		error = vm_inject_exception(vcpu, IDT_BP, 0, 0, 0);
		assert(error == 0);
	}
	pthread_mutex_unlock(&gdb_lock);
}

static bool
gdb_step_vcpu(struct vcpu *vcpu)
{
	int error, vcpuid;

	vcpuid = vcpu_id(vcpu);
	debug("$vCPU %d step\n", vcpuid);
	error = _gdb_check_step(vcpu);
	if (error < 0)
		return (false);

	discard_stop();
	vcpu_state[vcpuid].stepping = true;
	vm_resume_cpu(vcpu);
	CPU_CLR(vcpuid, &vcpus_suspended);
	pthread_cond_broadcast(&idle_vcpus);
	return (true);
}

static void
gdb_resume_vcpus(void)
{

	assert(pthread_mutex_isowned_np(&gdb_lock));
	vm_resume_all_cpus(ctx);
	debug("resuming all CPUs\n");
	CPU_ZERO(&vcpus_suspended);
	pthread_cond_broadcast(&idle_vcpus);
}

static void
gdb_read_regs(void)
{
	uint64_t regvals[nitems(gdb_regset)];
	int regnums[nitems(gdb_regset)];

	for (size_t i = 0; i < nitems(gdb_regset); i++)
		regnums[i] = gdb_regset[i].id;
	if (vm_get_register_set(vcpus[cur_vcpu], nitems(gdb_regset),
	    regnums, regvals) == -1) {
		send_error(errno);
		return;
	}
	start_packet();
	for (size_t i = 0; i < nitems(gdb_regset); i++)
		append_unsigned_native(regvals[i], gdb_regset[i].size);
	finish_packet();
}

static void
gdb_read_one_reg(const uint8_t *data, size_t len)
{
	uint64_t regval;
	uintmax_t reg;

	reg = parse_integer(data, len);
	if (reg >= nitems(gdb_regset)) {
		send_error(EINVAL);
		return;
	}

	if (vm_get_register(vcpus[cur_vcpu], gdb_regset[reg].id, &regval) ==
	    -1) {
		send_error(errno);
		return;
	}

	start_packet();
	append_unsigned_native(regval, gdb_regset[reg].size);
	finish_packet();
}

static void
gdb_read_mem(const uint8_t *data, size_t len)
{
	uint64_t gpa, gva, val;
	uint8_t *cp;
	size_t resid, todo, bytes;
	bool started;
	int error;

	assert(len >= 1);

	/* Skip 'm' */
	data += 1;
	len -= 1;

	/* Parse and consume address. */
	cp = memchr(data, ',', len);
	if (cp == NULL || cp == data) {
		send_error(EINVAL);
		return;
	}
	gva = parse_integer(data, cp - data);
	len -= (cp - data) + 1;
	data += (cp - data) + 1;

	/* Parse length. */
	resid = parse_integer(data, len);

	started = false;
	while (resid > 0) {
		error = guest_vaddr2paddr(vcpus[cur_vcpu], gva, &gpa);
		if (error == -1) {
			if (started)
				finish_packet();
			else
				send_error(errno);
			return;
		}
		if (error == 0) {
			if (started)
				finish_packet();
			else
				send_error(EFAULT);
			return;
		}

		/* Read bytes from current page. */
		todo = getpagesize() - gpa % getpagesize();
		if (todo > resid)
			todo = resid;

		cp = paddr_guest2host(ctx, gpa, todo);
		if (cp != NULL) {
			/*
			 * If this page is guest RAM, read it a byte
			 * at a time.
			 */
			if (!started) {
				start_packet();
				started = true;
			}
			while (todo > 0) {
				append_byte(*cp);
				cp++;
				gpa++;
				gva++;
				resid--;
				todo--;
			}
		} else {
			/*
			 * If this page isn't guest RAM, try to handle
			 * it via MMIO.  For MMIO requests, use
			 * aligned reads of words when possible.
			 */
			while (todo > 0) {
				if (gpa & 1 || todo == 1)
					bytes = 1;
				else if (gpa & 2 || todo == 2)
					bytes = 2;
				else
					bytes = 4;
				error = read_mem(vcpus[cur_vcpu], gpa, &val,
				    bytes);
				if (error == 0) {
					if (!started) {
						start_packet();
						started = true;
					}
					gpa += bytes;
					gva += bytes;
					resid -= bytes;
					todo -= bytes;
					while (bytes > 0) {
						append_byte(val);
						val >>= 8;
						bytes--;
					}
				} else {
					if (started)
						finish_packet();
					else
						send_error(EFAULT);
					return;
				}
			}
		}
		assert(resid == 0 || gpa % getpagesize() == 0);
	}
	if (!started)
		start_packet();
	finish_packet();
}

static void
gdb_write_mem(const uint8_t *data, size_t len)
{
	uint64_t gpa, gva, val;
	uint8_t *cp;
	size_t resid, todo, bytes;
	int error;

	assert(len >= 1);

	/* Skip 'M' */
	data += 1;
	len -= 1;

	/* Parse and consume address. */
	cp = memchr(data, ',', len);
	if (cp == NULL || cp == data) {
		send_error(EINVAL);
		return;
	}
	gva = parse_integer(data, cp - data);
	len -= (cp - data) + 1;
	data += (cp - data) + 1;

	/* Parse and consume length. */
	cp = memchr(data, ':', len);
	if (cp == NULL || cp == data) {
		send_error(EINVAL);
		return;
	}
	resid = parse_integer(data, cp - data);
	len -= (cp - data) + 1;
	data += (cp - data) + 1;

	/* Verify the available bytes match the length. */
	if (len != resid * 2) {
		send_error(EINVAL);
		return;
	}

	while (resid > 0) {
		error = guest_vaddr2paddr(vcpus[cur_vcpu], gva, &gpa);
		if (error == -1) {
			send_error(errno);
			return;
		}
		if (error == 0) {
			send_error(EFAULT);
			return;
		}

		/* Write bytes to current page. */
		todo = getpagesize() - gpa % getpagesize();
		if (todo > resid)
			todo = resid;

		cp = paddr_guest2host(ctx, gpa, todo);
		if (cp != NULL) {
			/*
			 * If this page is guest RAM, write it a byte
			 * at a time.
			 */
			while (todo > 0) {
				assert(len >= 2);
				*cp = parse_byte(data);
				data += 2;
				len -= 2;
				cp++;
				gpa++;
				gva++;
				resid--;
				todo--;
			}
		} else {
			/*
			 * If this page isn't guest RAM, try to handle
			 * it via MMIO.  For MMIO requests, use
			 * aligned writes of words when possible.
			 */
			while (todo > 0) {
				if (gpa & 1 || todo == 1) {
					bytes = 1;
					val = parse_byte(data);
				} else if (gpa & 2 || todo == 2) {
					bytes = 2;
					val = be16toh(parse_integer(data, 4));
				} else {
					bytes = 4;
					val = be32toh(parse_integer(data, 8));
				}
				error = write_mem(vcpus[cur_vcpu], gpa, val,
				    bytes);
				if (error == 0) {
					gpa += bytes;
					gva += bytes;
					resid -= bytes;
					todo -= bytes;
					data += 2 * bytes;
					len -= 2 * bytes;
				} else {
					send_error(EFAULT);
					return;
				}
			}
		}
		assert(resid == 0 || gpa % getpagesize() == 0);
	}
	assert(len == 0);
	send_ok();
}

static bool
set_breakpoint_caps(bool enable)
{
	cpuset_t mask;
	int vcpu;

	mask = vcpus_active;
	while (!CPU_EMPTY(&mask)) {
		vcpu = CPU_FFS(&mask) - 1;
		CPU_CLR(vcpu, &mask);
		if (vm_set_capability(vcpus[vcpu], VM_CAP_BPT_EXIT,
		    enable ? 1 : 0) < 0)
			return (false);
		debug("$vCPU %d %sabled breakpoint exits\n", vcpu,
		    enable ? "en" : "dis");
	}
	return (true);
}

static void
remove_all_sw_breakpoints(void)
{
	struct breakpoint *bp, *nbp;
	uint8_t *cp;

	if (TAILQ_EMPTY(&breakpoints))
		return;

	TAILQ_FOREACH_SAFE(bp, &breakpoints, link, nbp) {
		debug("remove breakpoint at %#lx\n", bp->gpa);
		cp = paddr_guest2host(ctx, bp->gpa, sizeof(bp->shadow_inst));
		memcpy(cp, bp->shadow_inst, sizeof(bp->shadow_inst));
		TAILQ_REMOVE(&breakpoints, bp, link);
		free(bp);
	}
	TAILQ_INIT(&breakpoints);
	set_breakpoint_caps(false);
}

static void
update_sw_breakpoint(uint64_t gva, int kind, bool insert)
{
	struct breakpoint *bp;
	uint64_t gpa;
	uint8_t *cp;
	int error;

	if (kind != GDB_BP_SIZE) {
		send_error(EINVAL);
		return;
	}

	error = guest_vaddr2paddr(vcpus[cur_vcpu], gva, &gpa);
	if (error == -1) {
		send_error(errno);
		return;
	}
	if (error == 0) {
		send_error(EFAULT);
		return;
	}

	cp = paddr_guest2host(ctx, gpa, sizeof(bp->shadow_inst));

	/* Only permit breakpoints in guest RAM. */
	if (cp == NULL) {
		send_error(EFAULT);
		return;
	}

	/* Find any existing breakpoint. */
	bp = find_breakpoint(gpa);

	/*
	 * Silently ignore duplicate commands since the protocol
	 * requires these packets to be idempotent.
	 */
	if (insert) {
		if (bp == NULL) {
			if (TAILQ_EMPTY(&breakpoints) &&
			    !set_breakpoint_caps(true)) {
				send_empty_response();
				return;
			}
			bp = malloc(sizeof(*bp));
			bp->gpa = gpa;
			memcpy(bp->shadow_inst, cp, sizeof(bp->shadow_inst));
			memcpy(cp, GDB_BP_INSTR, sizeof(bp->shadow_inst));
			TAILQ_INSERT_TAIL(&breakpoints, bp, link);
			debug("new breakpoint at %#lx\n", gpa);
		}
	} else {
		if (bp != NULL) {
			debug("remove breakpoint at %#lx\n", gpa);
			memcpy(cp, bp->shadow_inst, sizeof(bp->shadow_inst));
			TAILQ_REMOVE(&breakpoints, bp, link);
			free(bp);
			if (TAILQ_EMPTY(&breakpoints))
				set_breakpoint_caps(false);
		}
	}
	send_ok();
}

static void
parse_breakpoint(const uint8_t *data, size_t len)
{
	uint64_t gva;
	uint8_t *cp;
	bool insert;
	int kind, type;

	insert = data[0] == 'Z';

	/* Skip 'Z/z' */
	data += 1;
	len -= 1;

	/* Parse and consume type. */
	cp = memchr(data, ',', len);
	if (cp == NULL || cp == data) {
		send_error(EINVAL);
		return;
	}
	type = parse_integer(data, cp - data);
	len -= (cp - data) + 1;
	data += (cp - data) + 1;

	/* Parse and consume address. */
	cp = memchr(data, ',', len);
	if (cp == NULL || cp == data) {
		send_error(EINVAL);
		return;
	}
	gva = parse_integer(data, cp - data);
	len -= (cp - data) + 1;
	data += (cp - data) + 1;

	/* Parse and consume kind. */
	cp = memchr(data, ';', len);
	if (cp == data) {
		send_error(EINVAL);
		return;
	}
	if (cp != NULL) {
		/*
		 * We do not advertise support for either the
		 * ConditionalBreakpoints or BreakpointCommands
		 * features, so we should not be getting conditions or
		 * commands from the remote end.
		 */
		send_empty_response();
		return;
	}
	kind = parse_integer(data, len);
	data += len;
	len = 0;

	switch (type) {
	case 0:
		update_sw_breakpoint(gva, kind, insert);
		break;
	default:
		send_empty_response();
		break;
	}
}

static bool
command_equals(const uint8_t *data, size_t len, const char *cmd)
{

	if (strlen(cmd) > len)
		return (false);
	return (memcmp(data, cmd, strlen(cmd)) == 0);
}

static void
check_features(const uint8_t *data, size_t len)
{
	char *feature, *next_feature, *str, *value;
	bool supported;

	str = malloc(len + 1);
	memcpy(str, data, len);
	str[len] = '\0';
	next_feature = str;

	while ((feature = strsep(&next_feature, ";")) != NULL) {
		/*
		 * Null features shouldn't exist, but skip if they
		 * do.
		 */
		if (strcmp(feature, "") == 0)
			continue;

		/*
		 * Look for the value or supported / not supported
		 * flag.
		 */
		value = strchr(feature, '=');
		if (value != NULL) {
			*value = '\0';
			value++;
			supported = true;
		} else {
			value = feature + strlen(feature) - 1;
			switch (*value) {
			case '+':
				supported = true;
				break;
			case '-':
				supported = false;
				break;
			default:
				/*
				 * This is really a protocol error,
				 * but we just ignore malformed
				 * features for ease of
				 * implementation.
				 */
				continue;
			}
			value = NULL;
		}

		if (strcmp(feature, "swbreak") == 0)
			swbreak_enabled = supported;
	}
	free(str);

	start_packet();

	/* This is an arbitrary limit. */
	append_string("PacketSize=4096");
	append_string(";swbreak+");
	finish_packet();
}

static void
gdb_query(const uint8_t *data, size_t len)
{

	/*
	 * TODO:
	 * - qSearch
	 */
	if (command_equals(data, len, "qAttached")) {
		start_packet();
		append_char('1');
		finish_packet();
	} else if (command_equals(data, len, "qC")) {
		start_packet();
		append_string("QC");
		append_integer(cur_vcpu + 1);
		finish_packet();
	} else if (command_equals(data, len, "qfThreadInfo")) {
		cpuset_t mask;
		bool first;
		int vcpu;

		if (CPU_EMPTY(&vcpus_active)) {
			send_error(EINVAL);
			return;
		}
		mask = vcpus_active;
		start_packet();
		append_char('m');
		first = true;
		while (!CPU_EMPTY(&mask)) {
			vcpu = CPU_FFS(&mask) - 1;
			CPU_CLR(vcpu, &mask);
			if (first)
				first = false;
			else
				append_char(',');
			append_integer(vcpu + 1);
		}
		finish_packet();
	} else if (command_equals(data, len, "qsThreadInfo")) {
		start_packet();
		append_char('l');
		finish_packet();
	} else if (command_equals(data, len, "qSupported")) {
		data += strlen("qSupported");
		len -= strlen("qSupported");
		check_features(data, len);
	} else if (command_equals(data, len, "qThreadExtraInfo")) {
		char buf[16];
		int tid;

		data += strlen("qThreadExtraInfo");
		len -= strlen("qThreadExtraInfo");
		if (len == 0 || *data != ',') {
			send_error(EINVAL);
			return;
		}
		tid = parse_threadid(data + 1, len - 1);
		if (tid <= 0 || !CPU_ISSET(tid - 1, &vcpus_active)) {
			send_error(EINVAL);
			return;
		}

		snprintf(buf, sizeof(buf), "vCPU %d", tid - 1);
		start_packet();
		append_asciihex(buf);
		finish_packet();
	} else
		send_empty_response();
}

static void
handle_command(const uint8_t *data, size_t len)
{

	/* Reject packets with a sequence-id. */
	if (len >= 3 && data[0] >= '0' && data[0] <= '9' &&
	    data[0] >= '0' && data[0] <= '9' && data[2] == ':') {
		send_empty_response();
		return;
	}

	switch (*data) {
	case 'c':
		if (len != 1) {
			send_error(EINVAL);
			break;
		}

		discard_stop();
		gdb_resume_vcpus();
		break;
	case 'D':
		send_ok();

		/* TODO: Resume any stopped CPUs. */
		break;
	case 'g':
		gdb_read_regs();
		break;
	case 'p':
		gdb_read_one_reg(data + 1, len - 1);
		break;
	case 'H': {
		int tid;

		if (len < 2 || (data[1] != 'g' && data[1] != 'c')) {
			send_error(EINVAL);
			break;
		}
		tid = parse_threadid(data + 2, len - 2);
		if (tid == -2) {
			send_error(EINVAL);
			break;
		}

		if (CPU_EMPTY(&vcpus_active)) {
			send_error(EINVAL);
			break;
		}
		if (tid == -1 || tid == 0)
			cur_vcpu = CPU_FFS(&vcpus_active) - 1;
		else if (CPU_ISSET(tid - 1, &vcpus_active))
			cur_vcpu = tid - 1;
		else {
			send_error(EINVAL);
			break;
		}
		send_ok();
		break;
	}
	case 'm':
		gdb_read_mem(data, len);
		break;
	case 'M':
		gdb_write_mem(data, len);
		break;
	case 'T': {
		int tid;

		tid = parse_threadid(data + 1, len - 1);
		if (tid <= 0 || !CPU_ISSET(tid - 1, &vcpus_active)) {
			send_error(EINVAL);
			return;
		}
		send_ok();
		break;
	}
	case 'q':
		gdb_query(data, len);
		break;
	case 's':
		if (len != 1) {
			send_error(EINVAL);
			break;
		}

		/* Don't send a reply until a stop occurs. */
		if (!gdb_step_vcpu(vcpus[cur_vcpu])) {
			send_error(EOPNOTSUPP);
			break;
		}
		break;
	case 'z':
	case 'Z':
		parse_breakpoint(data, len);
		break;
	case '?':
		report_stop(false);
		break;
	case 'G': /* TODO */
	case 'v':
		/* Handle 'vCont' */
		/* 'vCtrlC' */
	case 'P': /* TODO */
	case 'Q': /* TODO */
	case 't': /* TODO */
	case 'X': /* TODO */
	default:
		send_empty_response();
	}
}

/* Check for a valid packet in the command buffer. */
static void
check_command(int fd)
{
	uint8_t *head, *hash, *p, sum;
	size_t avail, plen;

	for (;;) {
		avail = cur_comm.len;
		if (avail == 0)
			return;
		head = io_buffer_head(&cur_comm);
		switch (*head) {
		case 0x03:
			debug("<- Ctrl-C\n");
			io_buffer_consume(&cur_comm, 1);

			gdb_suspend_vcpus();
			break;
		case '+':
			/* ACK of previous response. */
			debug("<- +\n");
			if (response_pending())
				io_buffer_reset(&cur_resp);
			io_buffer_consume(&cur_comm, 1);
			if (stopped_vcpu != -1 && report_next_stop) {
				report_stop(true);
				send_pending_data(fd);
			}
			break;
		case '-':
			/* NACK of previous response. */
			debug("<- -\n");
			if (response_pending()) {
				cur_resp.len += cur_resp.start;
				cur_resp.start = 0;
				if (cur_resp.data[0] == '+')
					io_buffer_advance(&cur_resp, 1);
				debug("-> %.*s\n", (int)cur_resp.len,
				    io_buffer_head(&cur_resp));
			}
			io_buffer_consume(&cur_comm, 1);
			send_pending_data(fd);
			break;
		case '$':
			/* Packet. */

			if (response_pending()) {
				warnx("New GDB command while response in "
				    "progress");
				io_buffer_reset(&cur_resp);
			}

			/* Is packet complete? */
			hash = memchr(head, '#', avail);
			if (hash == NULL)
				return;
			plen = (hash - head + 1) + 2;
			if (avail < plen)
				return;
			debug("<- %.*s\n", (int)plen, head);

			/* Verify checksum. */
			for (sum = 0, p = head + 1; p < hash; p++)
				sum += *p;
			if (sum != parse_byte(hash + 1)) {
				io_buffer_consume(&cur_comm, plen);
				debug("-> -\n");
				send_char('-');
				send_pending_data(fd);
				break;
			}
			send_char('+');

			handle_command(head + 1, hash - (head + 1));
			io_buffer_consume(&cur_comm, plen);
			if (!response_pending())
				debug("-> +\n");
			send_pending_data(fd);
			break;
		default:
			/* XXX: Possibly drop connection instead. */
			debug("-> %02x\n", *head);
			io_buffer_consume(&cur_comm, 1);
			break;
		}
	}
}

static void
gdb_readable(int fd, enum ev_type event __unused, void *arg __unused)
{
	size_t pending;
	ssize_t nread;
	int n;

	if (ioctl(fd, FIONREAD, &n) == -1) {
		warn("FIONREAD on GDB socket");
		return;
	}
	assert(n >= 0);
	pending = n;

	/*
	 * 'pending' might be zero due to EOF.  We need to call read
	 * with a non-zero length to detect EOF.
	 */
	if (pending == 0)
		pending = 1;

	/* Ensure there is room in the command buffer. */
	io_buffer_grow(&cur_comm, pending);
	assert(io_buffer_avail(&cur_comm) >= pending);

	nread = read(fd, io_buffer_tail(&cur_comm), io_buffer_avail(&cur_comm));
	if (nread == 0) {
		close_connection();
	} else if (nread == -1) {
		if (errno == EAGAIN)
			return;

		warn("Read from GDB socket");
		close_connection();
	} else {
		cur_comm.len += nread;
		pthread_mutex_lock(&gdb_lock);
		check_command(fd);
		pthread_mutex_unlock(&gdb_lock);
	}
}

static void
gdb_writable(int fd, enum ev_type event __unused, void *arg __unused)
{

	send_pending_data(fd);
}

static void
new_connection(int fd, enum ev_type event __unused, void *arg)
{
	int optval, s;

	s = accept4(fd, NULL, NULL, SOCK_NONBLOCK);
	if (s == -1) {
		if (arg != NULL)
			err(1, "Failed accepting initial GDB connection");

		/* Silently ignore errors post-startup. */
		return;
	}

	optval = 1;
	if (setsockopt(s, SOL_SOCKET, SO_NOSIGPIPE, &optval, sizeof(optval)) ==
	    -1) {
		warn("Failed to disable SIGPIPE for GDB connection");
		close(s);
		return;
	}

	pthread_mutex_lock(&gdb_lock);
	if (cur_fd != -1) {
		close(s);
		warnx("Ignoring additional GDB connection.");
	}

	read_event = mevent_add(s, EVF_READ, gdb_readable, NULL);
	if (read_event == NULL) {
		if (arg != NULL)
			err(1, "Failed to setup initial GDB connection");
		pthread_mutex_unlock(&gdb_lock);
		return;
	}
	write_event = mevent_add(s, EVF_WRITE, gdb_writable, NULL);
	if (write_event == NULL) {
		if (arg != NULL)
			err(1, "Failed to setup initial GDB connection");
		mevent_delete_close(read_event);
		read_event = NULL;
	}

	cur_fd = s;
	cur_vcpu = 0;
	stopped_vcpu = -1;

	/* Break on attach. */
	first_stop = true;
	report_next_stop = false;
	gdb_suspend_vcpus();
	pthread_mutex_unlock(&gdb_lock);
}

#ifndef WITHOUT_CAPSICUM
static void
limit_gdb_socket(int s)
{
	cap_rights_t rights;
	unsigned long ioctls[] = { FIONREAD };

	cap_rights_init(&rights, CAP_ACCEPT, CAP_EVENT, CAP_READ, CAP_WRITE,
	    CAP_SETSOCKOPT, CAP_IOCTL);
	if (caph_rights_limit(s, &rights) == -1)
		errx(EX_OSERR, "Unable to apply rights for sandbox");
	if (caph_ioctls_limit(s, ioctls, nitems(ioctls)) == -1)
		errx(EX_OSERR, "Unable to apply rights for sandbox");
}
#endif

void
init_gdb(struct vmctx *_ctx)
{
	int error, flags, optval, s;
	struct addrinfo hints;
	struct addrinfo *gdbaddr;
	const char *saddr, *value;
	char *sport;
	bool wait;

	value = get_config_value("gdb.port");
	if (value == NULL)
		return;
	sport = strdup(value);
	if (sport == NULL)
		errx(4, "Failed to allocate memory");

	wait = get_config_bool_default("gdb.wait", false);

	saddr = get_config_value("gdb.address");
	if (saddr == NULL) {
		saddr = "localhost";
	}

	debug("==> starting on %s:%s, %swaiting\n",
	    saddr, sport, wait ? "" : "not ");

	error = pthread_mutex_init(&gdb_lock, NULL);
	if (error != 0)
		errc(1, error, "gdb mutex init");
	error = pthread_cond_init(&idle_vcpus, NULL);
	if (error != 0)
		errc(1, error, "gdb cv init");

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_NUMERICSERV | AI_PASSIVE;

	error = getaddrinfo(saddr, sport, &hints, &gdbaddr);
	if (error != 0)
		errx(1, "gdb address resolution: %s", gai_strerror(error));

	ctx = _ctx;
	s = socket(gdbaddr->ai_family, gdbaddr->ai_socktype, 0);
	if (s < 0)
		err(1, "gdb socket create");

	optval = 1;
	(void)setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

	if (bind(s, gdbaddr->ai_addr, gdbaddr->ai_addrlen) < 0)
		err(1, "gdb socket bind");

	if (listen(s, 1) < 0)
		err(1, "gdb socket listen");

	stopped_vcpu = -1;
	TAILQ_INIT(&breakpoints);
	vcpus = calloc(guest_ncpus, sizeof(*vcpus));
	vcpu_state = calloc(guest_ncpus, sizeof(*vcpu_state));
	if (wait) {
		/*
		 * Set vcpu 0 in vcpus_suspended.  This will trigger the
		 * logic in gdb_cpu_add() to suspend the first vcpu before
		 * it starts execution.  The vcpu will remain suspended
		 * until a debugger connects.
		 */
		CPU_SET(0, &vcpus_suspended);
		stopped_vcpu = 0;
	}

	flags = fcntl(s, F_GETFL);
	if (fcntl(s, F_SETFL, flags | O_NONBLOCK) == -1)
		err(1, "Failed to mark gdb socket non-blocking");

#ifndef WITHOUT_CAPSICUM
	limit_gdb_socket(s);
#endif
	mevent_add(s, EVF_READ, new_connection, NULL);
	gdb_active = true;
	freeaddrinfo(gdbaddr);
	free(sport);
}
