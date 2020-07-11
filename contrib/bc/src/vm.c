/*
 * *****************************************************************************
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2018-2020 Gavin D. Howard and contributors.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice, this
 *   list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * *****************************************************************************
 *
 * Code common to all of bc and dc.
 *
 */

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <string.h>

#include <signal.h>

#include <setjmp.h>

#ifndef _WIN32

#include <sys/types.h>
#include <unistd.h>

#else // _WIN32

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <io.h>

#endif // _WIN32

#include <status.h>
#include <vector.h>
#include <args.h>
#include <vm.h>
#include <read.h>
#include <bc.h>

#if BC_DEBUG_CODE
BC_NORETURN void bc_vm_jmp(const char* f) {
#else // BC_DEBUG_CODE
BC_NORETURN void bc_vm_jmp(void) {
#endif

	assert(BC_SIG_EXC);

	BC_SIG_MAYLOCK;

#if BC_DEBUG_CODE
	bc_file_puts(&vm.ferr, "Longjmp: ");
	bc_file_puts(&vm.ferr, f);
	bc_file_putchar(&vm.ferr, '\n');
	bc_file_flush(&vm.ferr);
#endif // BC_DEBUG_CODE

#ifndef NDEBUG
	assert(vm.jmp_bufs.len - (size_t) vm.sig_pop);
#endif // NDEBUG

	if (vm.sig_pop) bc_vec_pop(&vm.jmp_bufs);
	else vm.sig_pop = 1;

	siglongjmp(*((sigjmp_buf*) bc_vec_top(&vm.jmp_bufs)), 1);
}

static void bc_vm_sig(int sig) {

	// There is already a signal in flight.
	if (vm.status == (sig_atomic_t) BC_STATUS_QUIT || vm.sig) {
		if (!BC_TTY || sig != SIGINT) vm.status = BC_STATUS_QUIT;
		return;
	}

	if (BC_TTY && sig == SIGINT) {

		int err = errno;

		if (write(STDOUT_FILENO, vm.sigmsg, vm.siglen) != (ssize_t) vm.siglen)
			vm.status = BC_STATUS_ERROR_FATAL;
		else vm.sig = 1;

		errno = err;
	}
	else vm.status = BC_STATUS_QUIT;

	assert(vm.jmp_bufs.len);

	if (!vm.sig_lock) BC_VM_JMP;
}

void bc_vm_info(const char* const help) {

	BC_SIG_ASSERT_LOCKED;

	bc_file_puts(&vm.fout, vm.name);
	bc_file_putchar(&vm.fout, ' ');
	bc_file_puts(&vm.fout, BC_VERSION);
	bc_file_putchar(&vm.fout, '\n');
	bc_file_puts(&vm.fout, bc_copyright);

	if (help) {
		bc_file_putchar(&vm.fout, '\n');
		bc_file_printf(&vm.fout, help, vm.name, vm.name);
	}

	bc_file_flush(&vm.fout);
}

void bc_vm_error(BcError e, size_t line, ...) {

	BcStatus s;
	va_list args;
	uchar id = bc_err_ids[e];
	const char* err_type = vm.err_ids[id];
	sig_atomic_t lock;

	assert(e < BC_ERROR_NELEMS);
	assert(!vm.sig_pop);

#if BC_ENABLED
	if (!BC_S && e >= BC_ERROR_POSIX_START) {
		if (BC_W) {
			// Make sure to not return an error.
			id = UCHAR_MAX;
			err_type = vm.err_ids[BC_ERR_IDX_WARN];
		}
		else return;
	}
#endif // BC_ENABLED

	BC_SIG_TRYLOCK(lock);

	// Make sure all of stdout is written first.
	s = bc_file_flushErr(&vm.fout);

	if (BC_ERR(s == BC_STATUS_ERROR_FATAL)) {
		vm.status = (sig_atomic_t) s;
		BC_VM_JMP;
	}

	va_start(args, line);
	bc_file_putchar(&vm.ferr, '\n');
	bc_file_puts(&vm.ferr, err_type);
	bc_file_putchar(&vm.ferr, ' ');
	bc_file_vprintf(&vm.ferr, vm.err_msgs[e], args);
	va_end(args);

	if (BC_NO_ERR(vm.file)) {

		// This is the condition for parsing vs runtime.
		// If line is not 0, it is parsing.
		if (line) {
			bc_file_puts(&vm.ferr, "\n    ");
			bc_file_puts(&vm.ferr, vm.file);
			bc_file_printf(&vm.ferr, bc_err_line, line);
		}
		else {

			BcInstPtr *ip = bc_vec_item_rev(&vm.prog.stack, 0);
			BcFunc *f = bc_vec_item(&vm.prog.fns, ip->func);

			bc_file_puts(&vm.ferr, "\n    ");
			bc_file_puts(&vm.ferr, vm.func_header);
			bc_file_putchar(&vm.ferr, ' ');
			bc_file_puts(&vm.ferr, f->name);

#if BC_ENABLED
			if (BC_IS_BC && ip->func != BC_PROG_MAIN &&
			    ip->func != BC_PROG_READ)
			{
				bc_file_puts(&vm.ferr, "()");
			}
#endif // BC_ENABLED
		}
	}

	bc_file_puts(&vm.ferr, "\n\n");

	s = bc_file_flushErr(&vm.ferr);

	vm.status = s == BC_STATUS_ERROR_FATAL ?
	    (sig_atomic_t) s : (sig_atomic_t) (uchar) (id + 1);

	if (BC_ERR(vm.status)) BC_VM_JMP;

	BC_SIG_TRYUNLOCK(lock);
}

static void bc_vm_envArgs(const char* const env_args_name) {

	char *env_args = getenv(env_args_name), *buf, *start;
	char instr = '\0';

	BC_SIG_ASSERT_LOCKED;

	if (env_args == NULL) return;

	start = buf = vm.env_args_buffer = bc_vm_strdup(env_args);

	assert(buf != NULL);

	bc_vec_init(&vm.env_args, sizeof(char*), NULL);
	bc_vec_push(&vm.env_args, &env_args_name);

	while (*buf) {

		if (!isspace(*buf)) {

			if (*buf == '"' || *buf == '\'') {

				instr = *buf;
				buf += 1;

				if (*buf == instr) {
					instr = '\0';
					buf += 1;
					continue;
				}
			}

			bc_vec_push(&vm.env_args, &buf);

			while (*buf && ((!instr && !isspace(*buf)) ||
			                (instr && *buf != instr)))
			{
				buf += 1;
			}

			if (*buf) {

				if (instr) instr = '\0';

				*buf = '\0';
				buf += 1;
				start = buf;
			}
			else if (instr) bc_vm_error(BC_ERROR_FATAL_OPTION, 0, start);
		}
		else buf += 1;
	}

	// Make sure to push a NULL pointer at the end.
	buf = NULL;
	bc_vec_push(&vm.env_args, &buf);

	bc_args((int) vm.env_args.len - 1, bc_vec_item(&vm.env_args, 0));
}

static size_t bc_vm_envLen(const char *var) {

	char *lenv = getenv(var);
	size_t i, len = BC_NUM_PRINT_WIDTH;
	int num;

	if (lenv == NULL) return len;

	len = strlen(lenv);

	for (num = 1, i = 0; num && i < len; ++i) num = isdigit(lenv[i]);

	if (num) {
		len = (size_t) atoi(lenv) - 1;
		if (len < 2 || len >= UINT16_MAX) len = BC_NUM_PRINT_WIDTH;
	}
	else len = BC_NUM_PRINT_WIDTH;

	return len;
}

void bc_vm_shutdown(void) {

	BC_SIG_ASSERT_LOCKED;

#if BC_ENABLE_NLS
	if (vm.catalog != BC_VM_INVALID_CATALOG) catclose(vm.catalog);
#endif // BC_ENABLE_NLS

#if BC_ENABLE_HISTORY
	// This must always run to ensure that the terminal is back to normal.
	if (BC_TTY) bc_history_free(&vm.history);
#endif // BC_ENABLE_HISTORY

#ifndef NDEBUG
	bc_vec_free(&vm.env_args);
	free(vm.env_args_buffer);
	bc_vec_free(&vm.files);
	bc_vec_free(&vm.exprs);

	bc_program_free(&vm.prog);
	bc_parse_free(&vm.prs);

	{
		size_t i;
		for (i = 0; i < vm.temps.len; ++i)
			free(((BcNum*) bc_vec_item(&vm.temps, i))->num);

		bc_vec_free(&vm.temps);
	}
#endif // NDEBUG

	bc_file_free(&vm.fout);
	bc_file_free(&vm.ferr);
}

inline size_t bc_vm_arraySize(size_t n, size_t size) {
	size_t res = n * size;
	if (BC_ERR(res >= SIZE_MAX || (n != 0 && res / n != size)))
		bc_vm_err(BC_ERROR_FATAL_ALLOC_ERR);
	return res;
}

inline size_t bc_vm_growSize(size_t a, size_t b) {
	size_t res = a + b;
	if (BC_ERR(res >= SIZE_MAX || res < a || res < b))
		bc_vm_err(BC_ERROR_FATAL_ALLOC_ERR);
	return res;
}

void* bc_vm_malloc(size_t n) {

	void* ptr;

	BC_SIG_ASSERT_LOCKED;

	ptr = malloc(n);

	if (BC_ERR(ptr == NULL)) bc_vm_err(BC_ERROR_FATAL_ALLOC_ERR);

	return ptr;
}

void* bc_vm_realloc(void *ptr, size_t n) {

	void* temp;

	BC_SIG_ASSERT_LOCKED;

	temp = realloc(ptr, n);

	if (BC_ERR(temp == NULL)) bc_vm_err(BC_ERROR_FATAL_ALLOC_ERR);

	return temp;
}

char* bc_vm_strdup(const char *str) {

	char *s;

	BC_SIG_ASSERT_LOCKED;

	s = strdup(str);

	if (BC_ERR(!s)) bc_vm_err(BC_ERROR_FATAL_ALLOC_ERR);

	return s;
}

void bc_vm_printf(const char *fmt, ...) {

	va_list args;

	BC_SIG_LOCK;

	va_start(args, fmt);
	bc_file_vprintf(&vm.fout, fmt, args);
	va_end(args);

	vm.nchars = 0;

	BC_SIG_UNLOCK;
}

void bc_vm_putchar(int c) {
	bc_file_putchar(&vm.fout, (uchar) c);
	vm.nchars = (c == '\n' ? 0 : vm.nchars + 1);
}

static void bc_vm_clean(void) {

	BcVec *fns = &vm.prog.fns;
	BcFunc *f = bc_vec_item(fns, BC_PROG_MAIN);
	BcInstPtr *ip = bc_vec_item(&vm.prog.stack, 0);
	bool good = ((vm.status && vm.status != BC_STATUS_QUIT) || vm.sig);

	if (good) bc_program_reset(&vm.prog);

#if BC_ENABLED
	if (good && BC_IS_BC) good = !BC_PARSE_NO_EXEC(&vm.prs);
#endif // BC_ENABLED

#if DC_ENABLED
	if (BC_IS_DC) {

		size_t i;

		good = true;

		for (i = 0; good && i < vm.prog.results.len; ++i) {
			BcResult *r = (BcResult*) bc_vec_item(&vm.prog.results, i);
			good = BC_VM_SAFE_RESULT(r);
		}
	}
#endif // DC_ENABLED

	// If this condition is true, we can get rid of strings,
	// constants, and code. This is an idea from busybox.
	if (good && vm.prog.stack.len == 1 && ip->idx == f->code.len) {

#if BC_ENABLED
		if (BC_IS_BC) {
			bc_vec_npop(&f->labels, f->labels.len);
			bc_vec_npop(&f->strs, f->strs.len);
			bc_vec_npop(&f->consts, f->consts.len);
		}
#endif // BC_ENABLED

#if DC_ENABLED
		// Note to self: you cannot delete strings and functions. Deal with it.
		if (BC_IS_DC) bc_vec_npop(vm.prog.consts, vm.prog.consts->len);
#endif // DC_ENABLED

		bc_vec_npop(&f->code, f->code.len);

		ip->idx = 0;
	}
}

static void bc_vm_process(const char *text, bool is_stdin) {

	bc_parse_text(&vm.prs, text);

	do {

#if BC_ENABLED
		if (vm.prs.l.t == BC_LEX_KW_DEFINE) vm.parse(&vm.prs);
#endif // BC_ENABLED

		while (BC_PARSE_CAN_PARSE(vm.prs)) vm.parse(&vm.prs);

#if BC_ENABLED
		if (BC_IS_BC) {

			uint16_t *flags = BC_PARSE_TOP_FLAG_PTR(&vm.prs);

			if (!is_stdin && vm.prs.flags.len == 1 &&
			    *flags == BC_PARSE_FLAG_IF_END)
			{
				bc_parse_noElse(&vm.prs);
			}

			if (BC_PARSE_NO_EXEC(&vm.prs)) return;
		}
#endif // BC_ENABLED

		bc_program_exec(&vm.prog);

		assert(BC_IS_DC || vm.prog.results.len == 0);

		if (BC_I) bc_file_flush(&vm.fout);

	} while (vm.prs.l.t != BC_LEX_EOF);
}

static void bc_vm_file(const char *file) {

	char *data = NULL;

	assert(!vm.sig_pop);

	bc_lex_file(&vm.prs.l, file);

	BC_SIG_LOCK;

	bc_read_file(file, &data);

	BC_SETJMP_LOCKED(err);

	BC_SIG_UNLOCK;

	bc_vm_process(data, false);

#if BC_ENABLED
	if (BC_IS_BC && BC_ERR(BC_PARSE_NO_EXEC(&vm.prs)))
		bc_parse_err(&vm.prs, BC_ERROR_PARSE_BLOCK);
#endif // BC_ENABLED

err:
	BC_SIG_MAYLOCK;

	free(data);
	bc_vm_clean();

	// bc_program_reset(), called by bc_vm_clean(), resets the status.
	// We want it to clear the sig_pop variable in case it was set.
	if (vm.status == (sig_atomic_t) BC_STATUS_SUCCESS) BC_LONGJMP_STOP;

	BC_LONGJMP_CONT;
}

static void bc_vm_stdin(void) {

	BcStatus s;
	BcVec buf, buffer;
	size_t string = 0;
	bool comment = false, hash = false;

	bc_lex_file(&vm.prs.l, bc_program_stdin_name);

	BC_SIG_LOCK;
	bc_vec_init(&buffer, sizeof(uchar), NULL);
	bc_vec_init(&buf, sizeof(uchar), NULL);
	bc_vec_pushByte(&buffer, '\0');
	BC_SETJMP_LOCKED(err);
	BC_SIG_UNLOCK;

restart:

	// This loop is complex because the vm tries not to send any lines that end
	// with a backslash to the parser. The reason for that is because the parser
	// treats a backslash+newline combo as whitespace, per the bc spec. In that
	// case, and for strings and comments, the parser will expect more stuff.
	while ((!(s = bc_read_line(&buf, ">>> ")) ||
	        (vm.eof = (s == BC_STATUS_EOF))) && buf.len > 1)
	{
		char c2, *str = buf.v;
		size_t i, len = buf.len - 1;

		for (i = 0; i < len; ++i) {

			bool notend = len > i + 1;
			uchar c = (uchar) str[i];

			hash = (!comment && !string && ((hash && c != '\n') ||
			                                (!hash && c == '#')));

			if (!hash && !comment && (i - 1 > len || str[i - 1] != '\\')) {
				if (BC_IS_BC) string ^= (c == '"');
				else if (c == ']') string -= 1;
				else if (c == '[') string += 1;
			}

			if (BC_IS_BC && !hash && !string && notend) {

				c2 = str[i + 1];

				if (c == '/' && !comment && c2 == '*') {
					comment = true;
					i += 1;
				}
				else if (c == '*' && comment && c2 == '/') {
					comment = false;
					i += 1;
				}
			}
		}

		bc_vec_concat(&buffer, buf.v);

		if (string || comment) continue;
		if (len >= 2 && str[len - 2] == '\\' && str[len - 1] == '\n') continue;
#if BC_ENABLE_HISTORY
		if (vm.history.stdin_has_data) continue;
#endif // BC_ENABLE_HISTORY

		bc_vm_process(buffer.v, true);
		bc_vec_empty(&buffer);

		if (vm.eof) break;
		else bc_vm_clean();
	}

	if (!BC_STATUS_IS_ERROR(s)) {
		if (BC_ERR(comment))
			bc_parse_err(&vm.prs, BC_ERROR_PARSE_COMMENT);
		else if (BC_ERR(string))
			bc_parse_err(&vm.prs, BC_ERROR_PARSE_STRING);
#if BC_ENABLED
		else if (BC_IS_BC && BC_ERR(BC_PARSE_NO_EXEC(&vm.prs))) {

			size_t i;
			bool good = true;

			for (i = 0; good && i < vm.prs.flags.len; ++i) {
				uint16_t flag = *((uint16_t*) bc_vec_item(&vm.prs.flags, i));
				good = ((flag & BC_PARSE_FLAG_BRACE) != BC_PARSE_FLAG_BRACE);
			}

			if (good) {
				while (BC_PARSE_IF_END(&vm.prs)) bc_vm_process("else {}", true);
			}
			else bc_parse_err(&vm.prs, BC_ERROR_PARSE_BLOCK);
		}
#endif // BC_ENABLED
	}

err:
	BC_SIG_MAYLOCK;

	bc_vm_clean();

	vm.status = vm.status == BC_STATUS_ERROR_FATAL ||
	            vm.status == BC_STATUS_QUIT || !BC_I ?
	            vm.status : BC_STATUS_SUCCESS;

	if (!vm.status && !vm.eof) {
		bc_vec_empty(&buffer);
		BC_LONGJMP_STOP;
		BC_SIG_UNLOCK;
		goto restart;
	}

	bc_vec_free(&buf);
	bc_vec_free(&buffer);

	BC_LONGJMP_CONT;
}

#if BC_ENABLED
static void bc_vm_load(const char *name, const char *text) {

	bc_lex_file(&vm.prs.l, name);
	bc_parse_text(&vm.prs, text);

	while (vm.prs.l.t != BC_LEX_EOF) vm.parse(&vm.prs);
}
#endif // BC_ENABLED

static void bc_vm_defaultMsgs(void) {

	size_t i;

	vm.func_header = bc_err_func_header;

	for (i = 0; i < BC_ERR_IDX_NELEMS + BC_ENABLED; ++i)
		vm.err_ids[i] = bc_errs[i];
	for (i = 0; i < BC_ERROR_NELEMS; ++i) vm.err_msgs[i] = bc_err_msgs[i];
}

static void bc_vm_gettext(void) {

#if BC_ENABLE_NLS
	uchar id = 0;
	int set = 1, msg = 1;
	size_t i;

	if (vm.locale == NULL) {
		vm.catalog = BC_VM_INVALID_CATALOG;
		bc_vm_defaultMsgs();
		return;
	}

	vm.catalog = catopen(BC_MAINEXEC, NL_CAT_LOCALE);

	if (vm.catalog == BC_VM_INVALID_CATALOG) {
		bc_vm_defaultMsgs();
		return;
	}

	vm.func_header = catgets(vm.catalog, set, msg, bc_err_func_header);

	for (set += 1; msg <= BC_ERR_IDX_NELEMS + BC_ENABLED; ++msg)
		vm.err_ids[msg - 1] = catgets(vm.catalog, set, msg, bc_errs[msg - 1]);

	i = 0;
	id = bc_err_ids[i];

	for (set = id + 3, msg = 1; i < BC_ERROR_NELEMS; ++i, ++msg) {

		if (id != bc_err_ids[i]) {
			msg = 1;
			id = bc_err_ids[i];
			set = id + 3;
		}

		vm.err_msgs[i] = catgets(vm.catalog, set, msg, bc_err_msgs[i]);
	}
#else // BC_ENABLE_NLS
	bc_vm_defaultMsgs();
#endif // BC_ENABLE_NLS
}

static void bc_vm_exec(const char* env_exp_exit) {

	size_t i;
	bool has_file = false;
	BcVec buf;

#if BC_ENABLED
	if (BC_IS_BC && (vm.flags & BC_FLAG_L)) {

		bc_vm_load(bc_lib_name, bc_lib);

#if BC_ENABLE_EXTRA_MATH
		if (!BC_IS_POSIX) bc_vm_load(bc_lib2_name, bc_lib2);
#endif // BC_ENABLE_EXTRA_MATH
	}
#endif // BC_ENABLED

	if (vm.exprs.len) {

		size_t len = vm.exprs.len - 1;
		bool more;

		BC_SIG_LOCK;
		bc_vec_init(&buf, sizeof(uchar), NULL);

#ifndef NDEBUG
		BC_SETJMP_LOCKED(err);
#endif // NDEBUG

		BC_SIG_UNLOCK;

		bc_lex_file(&vm.prs.l, bc_program_exprs_name);

		do {

			more = bc_read_buf(&buf, vm.exprs.v, &len);
			bc_vec_pushByte(&buf, '\0');
			bc_vm_process(buf.v, false);

			bc_vec_npop(&buf, buf.len);

		} while (more);

		BC_SIG_LOCK;
		bc_vec_free(&buf);

#ifndef NDEBUG
		BC_UNSETJMP;
#endif // NDEBUG

		BC_SIG_UNLOCK;

		if (getenv(env_exp_exit) != NULL) return;
	}

	for (i = 0; i < vm.files.len; ++i) {
		char *path = *((char**) bc_vec_item(&vm.files, i));
		if (!strcmp(path, "")) continue;
		has_file = true;
		bc_vm_file(path);
	}

	if (BC_IS_BC || !has_file) bc_vm_stdin();

// These are all protected by ifndef NDEBUG because if these are needed, bc is
// goingi to exit anyway, and I see no reason to include this code in a release
// build when the OS is going to free all of the resources anyway.
#ifndef NDEBUG
	return;

err:
	BC_SIG_MAYLOCK;
	bc_vec_free(&buf);
	BC_LONGJMP_CONT;
#endif // NDEBUG
}

void  bc_vm_boot(int argc, char *argv[], const char *env_len,
                 const char* const env_args, const char* env_exp_exit)
{
	int ttyin, ttyout, ttyerr;
	struct sigaction sa;

	BC_SIG_ASSERT_LOCKED;

	ttyin = isatty(STDIN_FILENO);
	ttyout = isatty(STDOUT_FILENO);
	ttyerr = isatty(STDERR_FILENO);

	vm.flags |= ttyin ? BC_FLAG_TTYIN : 0;
	vm.flags |= (ttyin != 0 && ttyout != 0 && ttyerr != 0) ? BC_FLAG_TTY : 0;
	vm.flags |= ttyin && ttyout ? BC_FLAG_I : 0;

	sigemptyset(&sa.sa_mask);
	sa.sa_handler = bc_vm_sig;
	sa.sa_flags = SA_NODEFER;

	sigaction(SIGTERM, &sa, NULL);
	sigaction(SIGQUIT, &sa, NULL);
	sigaction(SIGINT, &sa, NULL);

#if BC_ENABLE_HISTORY
	if (BC_TTY) sigaction(SIGHUP, &sa, NULL);
#endif // BC_ENABLE_HISTORY

	memcpy(vm.max_num, bc_num_bigdigMax,
	       bc_num_bigdigMax_size * sizeof(BcDig));
	bc_num_setup(&vm.max, vm.max_num, BC_NUM_BIGDIG_LOG10);
	vm.max.len = bc_num_bigdigMax_size;

	vm.file = NULL;

	bc_vm_gettext();

	bc_file_init(&vm.ferr, STDERR_FILENO, output_bufs + BC_VM_STDOUT_BUF_SIZE,
	             BC_VM_STDERR_BUF_SIZE);
	bc_file_init(&vm.fout, STDOUT_FILENO, output_bufs, BC_VM_STDOUT_BUF_SIZE);
	vm.buf = output_bufs + BC_VM_STDOUT_BUF_SIZE + BC_VM_STDERR_BUF_SIZE;

	vm.line_len = (uint16_t) bc_vm_envLen(env_len);

	bc_vec_clear(&vm.files);
	bc_vec_clear(&vm.exprs);

	bc_vec_init(&vm.temps, sizeof(BcNum), NULL);

	bc_program_init(&vm.prog);
	bc_parse_init(&vm.prs, &vm.prog, BC_PROG_MAIN);

#if BC_ENABLE_HISTORY
	if (BC_TTY) bc_history_init(&vm.history);
#endif // BC_ENABLE_HISTORY

#if BC_ENABLED
	if (BC_IS_BC) vm.flags |= BC_FLAG_S * (getenv("POSIXLY_CORRECT") != NULL);
#endif // BC_ENABLED

	bc_vm_envArgs(env_args);
	bc_args(argc, argv);

#if BC_ENABLED
	if (BC_IS_POSIX) vm.flags &= ~(BC_FLAG_G);
#endif // BC_ENABLED

	vm.maxes[BC_PROG_GLOBALS_IBASE] = BC_NUM_MAX_POSIX_IBASE;
	vm.maxes[BC_PROG_GLOBALS_OBASE] = BC_MAX_OBASE;
	vm.maxes[BC_PROG_GLOBALS_SCALE] = BC_MAX_SCALE;

#if BC_ENABLE_EXTRA_MATH && BC_ENABLE_RAND
	vm.maxes[BC_PROG_MAX_RAND] = ((BcRand) 0) - 1;
#endif // BC_ENABLE_EXTRA_MATH && BC_ENABLE_RAND

#if BC_ENABLED
	if (BC_IS_BC && !BC_IS_POSIX)
		vm.maxes[BC_PROG_GLOBALS_IBASE] = BC_NUM_MAX_IBASE;
#endif // BC_ENABLED

	if (BC_IS_BC && BC_I && !(vm.flags & BC_FLAG_Q)) bc_vm_info(NULL);

	BC_SIG_UNLOCK;

	bc_vm_exec(env_exp_exit);
}
