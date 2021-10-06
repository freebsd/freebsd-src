/*
 * *****************************************************************************
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2018-2021 Gavin D. Howard and contributors.
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

#include <unistd.h>
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

// The actual globals.
static BcDig* temps_buf[BC_VM_MAX_TEMPS];
char output_bufs[BC_VM_BUF_SIZE];
BcVm vm;

#if BC_DEBUG_CODE
BC_NORETURN void bc_vm_jmp(const char* f) {
#else // BC_DEBUG_CODE
BC_NORETURN void bc_vm_jmp(void) {
#endif

	assert(BC_SIG_EXC);

	BC_SIG_MAYLOCK;

#if BC_DEBUG_CODE
	bc_file_puts(&vm.ferr, bc_flush_none, "Longjmp: ");
	bc_file_puts(&vm.ferr, bc_flush_none, f);
	bc_file_putchar(&vm.ferr, bc_flush_none, '\n');
	bc_file_flush(&vm.ferr, bc_flush_none);
#endif // BC_DEBUG_CODE

#ifndef NDEBUG
	assert(vm.jmp_bufs.len - (size_t) vm.sig_pop);
#endif // NDEBUG

	if (vm.jmp_bufs.len == 0) abort();
	if (vm.sig_pop) bc_vec_pop(&vm.jmp_bufs);
	else vm.sig_pop = 1;

	siglongjmp(*((sigjmp_buf*) bc_vec_top(&vm.jmp_bufs)), 1);
}

#if !BC_ENABLE_LIBRARY

/**
 * Handles signals. This is the signal handler.
 * @param sig  The signal to handle.
 */
static void bc_vm_sig(int sig) {

	// There is already a signal in flight.
	if (vm.status == (sig_atomic_t) BC_STATUS_QUIT || vm.sig) {
		if (!BC_I || sig != SIGINT) vm.status = BC_STATUS_QUIT;
		return;
	}

	// Only reset under these conditions; otherwise, quit.
	if (sig == SIGINT && BC_SIGINT && BC_I) {

		int err = errno;

		// Write the message.
		if (write(STDOUT_FILENO, vm.sigmsg, vm.siglen) != (ssize_t) vm.siglen)
			vm.status = BC_STATUS_ERROR_FATAL;
		else vm.sig = 1;

		errno = err;
	}
	else vm.status = BC_STATUS_QUIT;

	assert(vm.jmp_bufs.len);

	// Only jump if signals are not locked. The jump will happen by whoever
	// unlocks signals.
	if (!vm.sig_lock) BC_JMP;
}

/**
 * Sets up signal handling.
 */
static void bc_vm_sigaction(void) {
#ifndef _WIN32

	struct sigaction sa;

	sigemptyset(&sa.sa_mask);
	sa.sa_handler = bc_vm_sig;
	sa.sa_flags = SA_NODEFER;

	sigaction(SIGTERM, &sa, NULL);
	sigaction(SIGQUIT, &sa, NULL);
	sigaction(SIGINT, &sa, NULL);

#if BC_ENABLE_HISTORY
	if (BC_TTY) sigaction(SIGHUP, &sa, NULL);
#endif // BC_ENABLE_HISTORY

#else // _WIN32

	signal(SIGTERM, bc_vm_sig);
	signal(SIGINT, bc_vm_sig);

#endif // _WIN32
}

void bc_vm_info(const char* const help) {

	BC_SIG_ASSERT_LOCKED;

	// Print the banner.
	bc_file_puts(&vm.fout, bc_flush_none, vm.name);
	bc_file_putchar(&vm.fout, bc_flush_none, ' ');
	bc_file_puts(&vm.fout, bc_flush_none, BC_VERSION);
	bc_file_putchar(&vm.fout, bc_flush_none, '\n');
	bc_file_puts(&vm.fout, bc_flush_none, bc_copyright);

	// Print the help.
	if (help) {

		bc_file_putchar(&vm.fout, bc_flush_none, '\n');

#if BC_ENABLED
		if (BC_IS_BC) {

			const char* const banner = BC_DEFAULT_BANNER ? "to" : "to not";
			const char* const sigint = BC_DEFAULT_SIGINT_RESET ? "to reset" :
			                           "to exit";
			const char* const tty = BC_DEFAULT_TTY_MODE ? "enabled" :
			                        "disabled";
			const char* const prompt = BC_DEFAULT_PROMPT ? "enabled" :
			                           "disabled";

			bc_file_printf(&vm.fout, help, vm.name, vm.name, BC_VERSION,
			               BC_BUILD_TYPE, banner, sigint, tty, prompt);
		}
#endif // BC_ENABLED

#if DC_ENABLED
		if (BC_IS_DC)
		{
			const char* const sigint = DC_DEFAULT_SIGINT_RESET ? "to reset" :
			                           "to exit";
			const char* const tty = DC_DEFAULT_TTY_MODE ? "enabled" :
			                        "disabled";
			const char* const prompt = DC_DEFAULT_PROMPT ? "enabled" :
			                           "disabled";

			bc_file_printf(&vm.fout, help, vm.name, vm.name, BC_VERSION,
			               BC_BUILD_TYPE, sigint, tty, prompt);
		}
#endif // DC_ENABLED
	}

	// Flush.
	bc_file_flush(&vm.fout, bc_flush_none);
}
#endif // !BC_ENABLE_LIBRARY

#if !BC_ENABLE_LIBRARY && !BC_ENABLE_MEMCHECK
BC_NORETURN
#endif // !BC_ENABLE_LIBRARY && !BC_ENABLE_MEMCHECK
void bc_vm_fatalError(BcErr e) {
	bc_err(e);
#if !BC_ENABLE_LIBRARY && !BC_ENABLE_MEMCHECK
	BC_UNREACHABLE
	abort();
#endif // !BC_ENABLE_LIBRARY && !BC_ENABLE_MEMCHECK
}

#if BC_ENABLE_LIBRARY
void bc_vm_handleError(BcErr e) {

	assert(e < BC_ERR_NELEMS);
	assert(!vm.sig_pop);

	BC_SIG_LOCK;

	// If we have a normal error...
	if (e <= BC_ERR_MATH_DIVIDE_BY_ZERO) {

		// Set the error.
		vm.err = (BclError) (e - BC_ERR_MATH_NEGATIVE +
		                     BCL_ERROR_MATH_NEGATIVE);
	}
	// Abort if we should.
	else if (vm.abrt) abort();
	else if (e == BC_ERR_FATAL_ALLOC_ERR) vm.err = BCL_ERROR_FATAL_ALLOC_ERR;
	else vm.err = BCL_ERROR_FATAL_UNKNOWN_ERR;

	BC_JMP;
}
#else // BC_ENABLE_LIBRARY
void bc_vm_handleError(BcErr e, size_t line, ...) {

	BcStatus s;
	va_list args;
	uchar id = bc_err_ids[e];
	const char* err_type = vm.err_ids[id];
	sig_atomic_t lock;

	assert(e < BC_ERR_NELEMS);
	assert(!vm.sig_pop);

#if BC_ENABLED
	// Figure out if the POSIX error should be an error, a warning, or nothing.
	if (!BC_S && e >= BC_ERR_POSIX_START) {
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
	s = bc_file_flushErr(&vm.fout, bc_flush_err);

	// Just jump out if the flush failed; there's nothing we can do.
	if (BC_ERR(s == BC_STATUS_ERROR_FATAL)) {
		vm.status = (sig_atomic_t) s;
		BC_JMP;
	}

	// Print the error message.
	va_start(args, line);
	bc_file_putchar(&vm.ferr, bc_flush_none, '\n');
	bc_file_puts(&vm.ferr, bc_flush_none, err_type);
	bc_file_putchar(&vm.ferr, bc_flush_none, ' ');
	bc_file_vprintf(&vm.ferr, vm.err_msgs[e], args);
	va_end(args);

	// Print the extra information if we have it.
	if (BC_NO_ERR(vm.file != NULL)) {

		// This is the condition for parsing vs runtime.
		// If line is not 0, it is parsing.
		if (line) {
			bc_file_puts(&vm.ferr, bc_flush_none, "\n    ");
			bc_file_puts(&vm.ferr, bc_flush_none, vm.file);
			bc_file_printf(&vm.ferr, bc_err_line, line);
		}
		else {

			BcInstPtr *ip = bc_vec_item_rev(&vm.prog.stack, 0);
			BcFunc *f = bc_vec_item(&vm.prog.fns, ip->func);

			bc_file_puts(&vm.ferr, bc_flush_none, "\n    ");
			bc_file_puts(&vm.ferr, bc_flush_none, vm.func_header);
			bc_file_putchar(&vm.ferr, bc_flush_none, ' ');
			bc_file_puts(&vm.ferr, bc_flush_none, f->name);

#if BC_ENABLED
			if (BC_IS_BC && ip->func != BC_PROG_MAIN &&
			    ip->func != BC_PROG_READ)
			{
				bc_file_puts(&vm.ferr, bc_flush_none, "()");
			}
#endif // BC_ENABLED
		}
	}

	bc_file_puts(&vm.ferr, bc_flush_none, "\n\n");

	s = bc_file_flushErr(&vm.ferr, bc_flush_err);

#if !BC_ENABLE_MEMCHECK
	// Because this function is called by a BC_NORETURN function when fatal
	// errors happen, we need to make sure to exit on fatal errors. This will
	// be faster anyway. This function *cannot jump when a fatal error occurs!*
	if (BC_ERR(id == BC_ERR_IDX_FATAL || s == BC_STATUS_ERROR_FATAL))
		exit(bc_vm_atexit((int) BC_STATUS_ERROR_FATAL));
#else // !BC_ENABLE_MEMCHECK
	if (BC_ERR(s == BC_STATUS_ERROR_FATAL)) vm.status = (sig_atomic_t) s;
	else
#endif // !BC_ENABLE_MEMCHECK
	{
		vm.status = (sig_atomic_t) (uchar) (id + 1);
	}

	// Only jump if there is an error.
	if (BC_ERR(vm.status)) BC_JMP;

	BC_SIG_TRYUNLOCK(lock);
}

char* bc_vm_getenv(const char* var) {

	char* ret;

#ifndef _WIN32
	ret = getenv(var);
#else // _WIN32
	_dupenv_s(&ret, NULL, var);
#endif // _WIN32

	return ret;
}

void bc_vm_getenvFree(char* val) {
	BC_UNUSED(val);
#ifdef _WIN32
	free(val);
#endif // _WIN32
}

/**
 * Sets a flag from an environment variable and the default.
 * @param var   The environment variable.
 * @param def   The default.
 * @param flag  The flag to set.
 */
static void bc_vm_setenvFlag(const char* const var, int def, uint16_t flag) {

	// Get the value.
	char* val = bc_vm_getenv(var);

	// If there is no value...
	if (val == NULL) {

		// Set the default.
		if (def) vm.flags |= flag;
		else vm.flags &= ~(flag);
	}
	// Parse the value.
	else if (strtoul(val, NULL, 0)) vm.flags |= flag;
	else vm.flags &= ~(flag);

	bc_vm_getenvFree(val);
}

/**
 * Parses the arguments in {B,D]C_ENV_ARGS.
 * @param env_args_name  The environment variable to use.
 */
static void bc_vm_envArgs(const char* const env_args_name) {

	char *env_args = bc_vm_getenv(env_args_name), *buf, *start;
	char instr = '\0';

	BC_SIG_ASSERT_LOCKED;

	if (env_args == NULL) return;

	// Windows already allocates, so we don't need to.
#ifndef _WIN32
	start = buf = vm.env_args_buffer = bc_vm_strdup(env_args);
#else // _WIN32
	start = buf = vm.env_args_buffer = env_args;
#endif // _WIN32

	assert(buf != NULL);

	// Create two buffers for parsing. These need to stay throughout the entire
	// execution of bc, unfortunately, because of filenames that might be in
	// there.
	bc_vec_init(&vm.env_args, sizeof(char*), BC_DTOR_NONE);
	bc_vec_push(&vm.env_args, &env_args_name);

	// While we haven't reached the end of the args...
	while (*buf) {

		// If we don't have whitespace...
		if (!isspace(*buf)) {

			// If we have the start of a string...
			if (*buf == '"' || *buf == '\'') {

				// Set stuff appropriately.
				instr = *buf;
				buf += 1;

				// Check for the empty string.
				if (*buf == instr) {
					instr = '\0';
					buf += 1;
					continue;
				}
			}

			// Push the pointer to the args buffer.
			bc_vec_push(&vm.env_args, &buf);

			// Parse the string.
			while (*buf && ((!instr && !isspace(*buf)) ||
			                (instr && *buf != instr)))
			{
				buf += 1;
			}

			// If we did find the end of the string...
			if (*buf) {

				if (instr) instr = '\0';

				// Reset stuff.
				*buf = '\0';
				buf += 1;
				start = buf;
			}
			else if (instr) bc_error(BC_ERR_FATAL_OPTION, 0, start);
		}
		// If we have whitespace, eat it.
		else buf += 1;
	}

	// Make sure to push a NULL pointer at the end.
	buf = NULL;
	bc_vec_push(&vm.env_args, &buf);

	// Parse the arguments.
	bc_args((int) vm.env_args.len - 1, bc_vec_item(&vm.env_args, 0), false);
}

/**
 * Gets the {B,D}C_LINE_LENGTH.
 * @param var  The environment variable to pull it from.
 * @return     The line length.
 */
static size_t bc_vm_envLen(const char *var) {

	char *lenv = bc_vm_getenv(var);
	size_t i, len = BC_NUM_PRINT_WIDTH;
	int num;

	// Return the default with none.
	if (lenv == NULL) return len;

	len = strlen(lenv);

	// Figure out if it's a number.
	for (num = 1, i = 0; num && i < len; ++i) num = isdigit(lenv[i]);

	// If it is a number...
	if (num) {

		// Parse it and clamp it if needed.
		len = (size_t) atoi(lenv) - 1;
		if (len == 1 || len >= UINT16_MAX) len = BC_NUM_PRINT_WIDTH;
	}
	// Set the default.
	else len = BC_NUM_PRINT_WIDTH;

	bc_vm_getenvFree(lenv);

	return len;
}
#endif // BC_ENABLE_LIBRARY

void bc_vm_shutdown(void) {

	BC_SIG_ASSERT_LOCKED;

#if BC_ENABLE_NLS
	if (vm.catalog != BC_VM_INVALID_CATALOG) catclose(vm.catalog);
#endif // BC_ENABLE_NLS

#if BC_ENABLE_HISTORY
	// This must always run to ensure that the terminal is back to normal, i.e.,
	// has raw mode disabled.
	if (BC_TTY) bc_history_free(&vm.history);
#endif // BC_ENABLE_HISTORY

#ifndef NDEBUG
#if !BC_ENABLE_LIBRARY
	bc_vec_free(&vm.env_args);
	free(vm.env_args_buffer);
	bc_vec_free(&vm.files);
	bc_vec_free(&vm.exprs);

	if (BC_PARSE_IS_INITED(&vm.read_prs, &vm.prog)) {
		bc_vec_free(&vm.read_buf);
		bc_parse_free(&vm.read_prs);
	}

	bc_parse_free(&vm.prs);
	bc_program_free(&vm.prog);

	bc_slabvec_free(&vm.other_slabs);
	bc_slabvec_free(&vm.main_slabs);
	bc_slabvec_free(&vm.main_const_slab);
#endif // !BC_ENABLE_LIBRARY

	bc_vm_freeTemps();
#endif // NDEBUG

#if !BC_ENABLE_LIBRARY
	// We always want to flush.
	bc_file_free(&vm.fout);
	bc_file_free(&vm.ferr);
#endif // !BC_ENABLE_LIBRARY
}

void bc_vm_addTemp(BcDig *num) {

	// If we don't have room, just free.
	if (vm.temps_len == BC_VM_MAX_TEMPS) free(num);
	else {

		// Add to the buffer and length.
		temps_buf[vm.temps_len] = num;
		vm.temps_len += 1;
	}
}

BcDig* bc_vm_takeTemp(void) {
	if (!vm.temps_len) return NULL;
	vm.temps_len -= 1;
	return temps_buf[vm.temps_len];
}

void bc_vm_freeTemps(void) {

	size_t i;

	BC_SIG_ASSERT_LOCKED;

	if (!vm.temps_len) return;

	// Free them all...
	for (i = 0; i < vm.temps_len; ++i) free(temps_buf[i]);

	vm.temps_len = 0;
}

inline size_t bc_vm_arraySize(size_t n, size_t size) {
	size_t res = n * size;
	if (BC_ERR(BC_VM_MUL_OVERFLOW(n, size, res)))
		bc_vm_fatalError(BC_ERR_FATAL_ALLOC_ERR);
	return res;
}

inline size_t bc_vm_growSize(size_t a, size_t b) {
	size_t res = a + b;
	if (BC_ERR(res >= SIZE_MAX || res < a))
		bc_vm_fatalError(BC_ERR_FATAL_ALLOC_ERR);
	return res;
}

void* bc_vm_malloc(size_t n) {

	void* ptr;

	BC_SIG_ASSERT_LOCKED;

	ptr = malloc(n);

	if (BC_ERR(ptr == NULL)) {

		bc_vm_freeTemps();

		ptr = malloc(n);

		if (BC_ERR(ptr == NULL)) bc_vm_fatalError(BC_ERR_FATAL_ALLOC_ERR);
	}

	return ptr;
}

void* bc_vm_realloc(void *ptr, size_t n) {

	void* temp;

	BC_SIG_ASSERT_LOCKED;

	temp = realloc(ptr, n);

	if (BC_ERR(temp == NULL)) {

		bc_vm_freeTemps();

		temp = realloc(ptr, n);

		if (BC_ERR(temp == NULL)) bc_vm_fatalError(BC_ERR_FATAL_ALLOC_ERR);
	}

	return temp;
}

char* bc_vm_strdup(const char *str) {

	char *s;

	BC_SIG_ASSERT_LOCKED;

	s = strdup(str);

	if (BC_ERR(s == NULL)) {

		bc_vm_freeTemps();

		s = strdup(str);

		if (BC_ERR(s == NULL)) bc_vm_fatalError(BC_ERR_FATAL_ALLOC_ERR);
	}

	return s;
}

#if !BC_ENABLE_LIBRARY
void bc_vm_printf(const char *fmt, ...) {

	va_list args;

	BC_SIG_LOCK;

	va_start(args, fmt);
	bc_file_vprintf(&vm.fout, fmt, args);
	va_end(args);

	vm.nchars = 0;

	BC_SIG_UNLOCK;
}
#endif // !BC_ENABLE_LIBRARY

void bc_vm_putchar(int c, BcFlushType type) {
#if BC_ENABLE_LIBRARY
	bc_vec_pushByte(&vm.out, (uchar) c);
#else // BC_ENABLE_LIBRARY
	bc_file_putchar(&vm.fout, type, (uchar) c);
	vm.nchars = (c == '\n' ? 0 : vm.nchars + 1);
#endif // BC_ENABLE_LIBRARY
}

#if !BC_ENABLE_LIBRARY

#ifdef __OpenBSD__

/**
 * Aborts with a message. This should never be called because I have carefully
 * made sure that the calls to pledge() and unveil() are correct, but it's here
 * just in case.
 * @param msg  The message to print.
 */
BC_NORETURN static void bc_abortm(const char* msg) {
	bc_file_puts(&vm.ferr, bc_flush_none, msg);
	bc_file_puts(&vm.ferr, bc_flush_none, "; this is a bug");
	bc_file_flush(&vm.ferr, bc_flush_none);
	abort();
}

void bc_pledge(const char *promises, const char* execpromises) {
	int r = pledge(promises, execpromises);
	if (r) bc_abortm("pledge() failed");
}

#if BC_ENABLE_EXTRA_MATH

/**
 * A convenience and portability function for OpenBSD's unveil().
 * @param path         The path.
 * @param permissions  The permissions for the path.
 */
static void bc_unveil(const char *path, const char *permissions) {
	int r = unveil(path, permissions);
	if (r) bc_abortm("unveil() failed");
}
#endif // BC_ENABLE_EXTRA_MATH

#else // __OpenBSD__

void bc_pledge(const char *promises, const char *execpromises) {
	BC_UNUSED(promises);
	BC_UNUSED(execpromises);
}

#if BC_ENABLE_EXTRA_MATH
static void bc_unveil(const char *path, const char *permissions) {
	BC_UNUSED(path);
	BC_UNUSED(permissions);
}
#endif // BC_ENABLE_EXTRA_MATH

#endif // __OpenBSD__

/**
 * Cleans unneeded variables, arrays, functions, strings, and constants when
 * done executing a line of stdin. This is to prevent memory usage growing
 * without bound. This is an idea from busybox.
 */
static void bc_vm_clean(void) {

	BcVec *fns = &vm.prog.fns;
	BcFunc *f = bc_vec_item(fns, BC_PROG_MAIN);
	BcInstPtr *ip = bc_vec_item(&vm.prog.stack, 0);
	bool good = ((vm.status && vm.status != BC_STATUS_QUIT) || vm.sig);

	// If all is good, go ahead and reset.
	if (good) bc_program_reset(&vm.prog);

#if BC_ENABLED
	// bc has this extra condition. If it not satisfied, it is in the middle of
	// a parse.
	if (good && BC_IS_BC) good = !BC_PARSE_NO_EXEC(&vm.prs);
#endif // BC_ENABLED

#if DC_ENABLED
	// For dc, it is safe only when all of the results on the results stack are
	// safe, which means that they are temporaries or other things that don't
	// need strings or constants.
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
	// constants, and code.
	if (good && vm.prog.stack.len == 1 && ip->idx == f->code.len) {

#if BC_ENABLED
		if (BC_IS_BC) {

			bc_vec_popAll(&f->labels);
			bc_vec_popAll(&f->strs);
			bc_vec_popAll(&f->consts);

			// I can't clear out the other_slabs because it has functions,
			// consts, strings, vars, and arrays. It has strings from *other*
			// functions, specifically.
			bc_slabvec_clear(&vm.main_const_slab);
			bc_slabvec_clear(&vm.main_slabs);
		}
#endif // BC_ENABLED

#if DC_ENABLED
		// Note to self: you cannot delete strings and functions. Deal with it.
		if (BC_IS_DC) {
			bc_vec_popAll(vm.prog.consts);
			bc_slabvec_clear(&vm.main_const_slab);
		}
#endif // DC_ENABLED

		bc_vec_popAll(&f->code);

		ip->idx = 0;
	}
}

/**
 * Process a bunch of text.
 * @param text      The text to process.
 * @param is_stdin  True if the text came from stdin, false otherwise.
 */
static void bc_vm_process(const char *text, bool is_stdin) {

	// Set up the parser.
	bc_parse_text(&vm.prs, text, is_stdin);

	do {

#if BC_ENABLED
		// If the first token is the keyword define, then we need to do this
		// specially because bc thinks it may not be able to parse.
		if (vm.prs.l.t == BC_LEX_KW_DEFINE) vm.parse(&vm.prs);
#endif // BC_ENABLED

		// Parse it all.
		while (BC_PARSE_CAN_PARSE(vm.prs)) vm.parse(&vm.prs);

		// Execute if possible.
		if(BC_IS_DC || !BC_PARSE_NO_EXEC(&vm.prs)) bc_program_exec(&vm.prog);

		assert(BC_IS_DC || vm.prog.results.len == 0);

		// Flush in interactive mode.
		if (BC_I) bc_file_flush(&vm.fout, bc_flush_save);

	} while (vm.prs.l.t != BC_LEX_EOF);
}

#if BC_ENABLED

/**
 * Ends a series of if statements. This is to ensure that full parses happen
 * when a file finishes or stdin has no more data. Without this, bc thinks that
 * it cannot parse any further. But if we reach the end of a file or stdin has
 * no more data, we know we can add an empty else clause.
 */
static void bc_vm_endif(void) {
	bc_parse_endif(&vm.prs);
	bc_program_exec(&vm.prog);
}
#endif // BC_ENABLED

/**
 * Processes a file.
 * @param file  The filename.
 */
static void bc_vm_file(const char *file) {

	char *data = NULL;

	assert(!vm.sig_pop);

	// Set up the lexer.
	bc_lex_file(&vm.prs.l, file);

	BC_SIG_LOCK;

	// Read the file.
	data = bc_read_file(file);

	assert(data != NULL);

	BC_SETJMP_LOCKED(err);

	BC_SIG_UNLOCK;

	// Process it.
	bc_vm_process(data, false);

#if BC_ENABLED
	// Make sure to end any open if statements.
	if (BC_IS_BC) bc_vm_endif();
#endif // BC_ENABLED

err:
	BC_SIG_MAYLOCK;

	// Cleanup.
	free(data);
	bc_vm_clean();

	// bc_program_reset(), called by bc_vm_clean(), resets the status.
	// We want it to clear the sig_pop variable in case it was set.
	if (vm.status == (sig_atomic_t) BC_STATUS_SUCCESS) BC_LONGJMP_STOP;

	BC_LONGJMP_CONT;
}

bool bc_vm_readLine(bool clear) {

	BcStatus s;
	bool good;

	// Clear the buffer if desired.
	if (clear) bc_vec_empty(&vm.buffer);

	// Empty the line buffer.
	bc_vec_empty(&vm.line_buf);

	if (vm.eof) return false;

	do {
		// bc_read_line() must always return either BC_STATUS_SUCCESS or
		// BC_STATUS_EOF. Everything else, it and whatever it calls, must jump
		// out instead.
		s = bc_read_line(&vm.line_buf, ">>> ");
		vm.eof = (s == BC_STATUS_EOF);
	} while (!(s) && !vm.eof && vm.line_buf.len < 1);

	good = (vm.line_buf.len > 1);

	// Concat if we found something.
	if (good) bc_vec_concat(&vm.buffer, vm.line_buf.v);

	return good;
}

/**
 * Processes text from stdin.
 */
static void bc_vm_stdin(void) {

	bool clear = true;

	vm.is_stdin = true;

	// Set up the lexer.
	bc_lex_file(&vm.prs.l, bc_program_stdin_name);

	// These are global so that the dc lexer can access them, but they are tied
	// to this function, really. Well, this and bc_vm_readLine(). These are the
	// reason that we have vm.is_stdin to tell the dc lexer if we are reading
	// from stdin. Well, both lexers care. And the reason they care is so that
	// if a comment or a string goes across multiple lines, the lexer can
	// request more data from stdin until the comment or string is ended.
	BC_SIG_LOCK;
	bc_vec_init(&vm.buffer, sizeof(uchar), BC_DTOR_NONE);
	bc_vec_init(&vm.line_buf, sizeof(uchar), BC_DTOR_NONE);
	BC_SETJMP_LOCKED(err);
	BC_SIG_UNLOCK;

// This label exists because errors can cause jumps to end up at the err label
// below. If that happens, and the error should be cleared and execution
// continue, then we need to jump back.
restart:

	// While we still read data from stdin.
	while (bc_vm_readLine(clear)) {

		size_t len = vm.buffer.len - 1;
		const char *str = vm.buffer.v;

		// We don't want to clear the buffer when the line ends with a backslash
		// because a backslash newline is special in bc.
		clear = (len < 2 || str[len - 2] != '\\' || str[len - 1] != '\n');
		if (!clear) continue;

		// Process the data.
		bc_vm_process(vm.buffer.v, true);

		if (vm.eof) break;
		else bc_vm_clean();
	}

#if BC_ENABLED
	// End the if statements.
	if (BC_IS_BC) bc_vm_endif();
#endif // BC_ENABLED

err:
	BC_SIG_MAYLOCK;

	// Cleanup.
	bc_vm_clean();

#if !BC_ENABLE_MEMCHECK
	assert(vm.status != BC_STATUS_ERROR_FATAL);

	vm.status = vm.status == BC_STATUS_QUIT || !BC_I ?
	            vm.status : BC_STATUS_SUCCESS;
#else // !BC_ENABLE_MEMCHECK
	vm.status = vm.status == BC_STATUS_ERROR_FATAL ||
	            vm.status == BC_STATUS_QUIT || !BC_I ?
	            vm.status : BC_STATUS_SUCCESS;
#endif // !BC_ENABLE_MEMCHECK

	if (!vm.status && !vm.eof) {
		bc_vec_empty(&vm.buffer);
		BC_LONGJMP_STOP;
		BC_SIG_UNLOCK;
		goto restart;
	}

#ifndef NDEBUG
	// Since these are tied to this function, free them here.
	bc_vec_free(&vm.line_buf);
	bc_vec_free(&vm.buffer);
#endif // NDEBUG

	BC_LONGJMP_CONT;
}

#if BC_ENABLED

/**
 * Loads a math library.
 * @param name  The name of the library.
 * @param text  The text of the source code.
 */
static void bc_vm_load(const char *name, const char *text) {

	bc_lex_file(&vm.prs.l, name);
	bc_parse_text(&vm.prs, text, false);

	while (vm.prs.l.t != BC_LEX_EOF) vm.parse(&vm.prs);
}

#endif // BC_ENABLED

/**
 * Loads the default error messages.
 */
static void bc_vm_defaultMsgs(void) {

	size_t i;

	vm.func_header = bc_err_func_header;

	// Load the error categories.
	for (i = 0; i < BC_ERR_IDX_NELEMS + BC_ENABLED; ++i)
		vm.err_ids[i] = bc_errs[i];

	// Load the error messages.
	for (i = 0; i < BC_ERR_NELEMS; ++i) vm.err_msgs[i] = bc_err_msgs[i];
}

/**
 * Loads the error messages for the locale. If NLS is disabled, this just loads
 * the default messages.
 */
static void bc_vm_gettext(void) {

#if BC_ENABLE_NLS
	uchar id = 0;
	int set = 1, msg = 1;
	size_t i;

	// If no locale, load the defaults.
	if (vm.locale == NULL) {
		vm.catalog = BC_VM_INVALID_CATALOG;
		bc_vm_defaultMsgs();
		return;
	}

	vm.catalog = catopen(BC_MAINEXEC, NL_CAT_LOCALE);

	// If no catalog, load the defaults.
	if (vm.catalog == BC_VM_INVALID_CATALOG) {
		bc_vm_defaultMsgs();
		return;
	}

	// Load the function header.
	vm.func_header = catgets(vm.catalog, set, msg, bc_err_func_header);

	// Load the error categories.
	for (set += 1; msg <= BC_ERR_IDX_NELEMS + BC_ENABLED; ++msg)
		vm.err_ids[msg - 1] = catgets(vm.catalog, set, msg, bc_errs[msg - 1]);

	i = 0;
	id = bc_err_ids[i];

	// Load the error messages. In order to understand this loop, you must know
	// the order of messages and categories in the enum and in the locale files.
	for (set = id + 3, msg = 1; i < BC_ERR_NELEMS; ++i, ++msg) {

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

/**
 * Starts execution. Really, this is a function of historical accident; it could
 * probably be combined with bc_vm_boot(), but I don't care enough. Really, this
 * function starts when execution of bc or dc source code starts.
 */
static void bc_vm_exec(void) {

	size_t i;
	bool has_file = false;
	BcVec buf;

#if BC_ENABLED
	// Load the math libraries.
	if (BC_IS_BC && (vm.flags & BC_FLAG_L)) {

		// Can't allow redefinitions in the builtin library.
		vm.no_redefine = true;

		bc_vm_load(bc_lib_name, bc_lib);

#if BC_ENABLE_EXTRA_MATH
		if (!BC_IS_POSIX) bc_vm_load(bc_lib2_name, bc_lib2);
#endif // BC_ENABLE_EXTRA_MATH

		// Make sure to clear this.
		vm.no_redefine = false;

		// Execute to ensure that all is hunky dory. Without this, scale can be
		// set improperly.
		bc_program_exec(&vm.prog);
	}
#endif // BC_ENABLED

	// If there are expressions to execute...
	if (vm.exprs.len) {

		size_t len = vm.exprs.len - 1;
		bool more;

		BC_SIG_LOCK;

		// Create this as a buffer for reading into.
		bc_vec_init(&buf, sizeof(uchar), BC_DTOR_NONE);

#ifndef NDEBUG
		BC_SETJMP_LOCKED(err);
#endif // NDEBUG

		BC_SIG_UNLOCK;

		// Prepare the lexer.
		bc_lex_file(&vm.prs.l, bc_program_exprs_name);

		// Process the expressions one at a time.
		do {

			more = bc_read_buf(&buf, vm.exprs.v, &len);
			bc_vec_pushByte(&buf, '\0');
			bc_vm_process(buf.v, false);

			bc_vec_popAll(&buf);

		} while (more);

		BC_SIG_LOCK;

		bc_vec_free(&buf);

#ifndef NDEBUG
		BC_UNSETJMP;
#endif // NDEBUG

		BC_SIG_UNLOCK;

		// Sometimes, executing expressions means we need to quit.
		if (!vm.no_exprs && vm.exit_exprs) return;
	}

	// Process files.
	for (i = 0; i < vm.files.len; ++i) {
		char *path = *((char**) bc_vec_item(&vm.files, i));
		if (!strcmp(path, "")) continue;
		has_file = true;
		bc_vm_file(path);
	}

#if BC_ENABLE_EXTRA_MATH
	// These are needed for the pseudo-random number generator.
	bc_unveil("/dev/urandom", "r");
	bc_unveil("/dev/random", "r");
	bc_unveil(NULL, NULL);
#endif // BC_ENABLE_EXTRA_MATH

#if BC_ENABLE_HISTORY

	// We need to keep tty if history is enabled, and we need to keep rpath for
	// the times when we read from /dev/urandom.
	if (BC_TTY && !vm.history.badTerm) {
		bc_pledge(bc_pledge_end_history, NULL);
	}
	else
#endif // BC_ENABLE_HISTORY
	{
		bc_pledge(bc_pledge_end, NULL);
	}

#if BC_ENABLE_AFL
	// This is the thing that makes fuzzing with AFL++ so fast. If you move this
	// back, you won't cause any problems, but fuzzing will slow down. If you
	// move this forward, you won't fuzz anything because you will be skipping
	// the reading from stdin.
	__AFL_INIT();
#endif // BC_ENABLE_AFL

	// Execute from stdin. bc always does.
	if (BC_IS_BC || !has_file) bc_vm_stdin();

// These are all protected by ifndef NDEBUG because if these are needed, bc is
// going to exit anyway, and I see no reason to include this code in a release
// build when the OS is going to free all of the resources anyway.
#ifndef NDEBUG
	return;

err:
	BC_SIG_MAYLOCK;
	bc_vec_free(&buf);
	BC_LONGJMP_CONT;
#endif // NDEBUG
}

void bc_vm_boot(int argc, char *argv[]) {

	int ttyin, ttyout, ttyerr;
	bool tty;
	const char* const env_len = BC_IS_BC ? "BC_LINE_LENGTH" : "DC_LINE_LENGTH";
	const char* const env_args = BC_IS_BC ? "BC_ENV_ARGS" : "DC_ENV_ARGS";

	// We need to know which of stdin, stdout, and stderr are tty's.
	ttyin = isatty(STDIN_FILENO);
	ttyout = isatty(STDOUT_FILENO);
	ttyerr = isatty(STDERR_FILENO);
	tty = (ttyin != 0 && ttyout != 0 && ttyerr != 0);

	vm.flags |= ttyin ? BC_FLAG_TTYIN : 0;
	vm.flags |= tty ? BC_FLAG_TTY : 0;
	vm.flags |= ttyin && ttyout ? BC_FLAG_I : 0;

	// Set up signals.
	bc_vm_sigaction();

	// Initialize some vm stuff. This is separate to make things easier for the
	// library.
	bc_vm_init();

	// Explicitly set this in case NULL isn't all zeroes.
	vm.file = NULL;

	// Set the error messages.
	bc_vm_gettext();

	// Initialize the output file buffers. They each take portions of the global
	// buffer. stdout gets more because it will probably have more data.
	bc_file_init(&vm.ferr, STDERR_FILENO, output_bufs + BC_VM_STDOUT_BUF_SIZE,
	             BC_VM_STDERR_BUF_SIZE);
	bc_file_init(&vm.fout, STDOUT_FILENO, output_bufs, BC_VM_STDOUT_BUF_SIZE);

	// Set the input buffer to the rest of the global buffer.
	vm.buf = output_bufs + BC_VM_STDOUT_BUF_SIZE + BC_VM_STDERR_BUF_SIZE;

	// Set the line length by environment variable.
	vm.line_len = (uint16_t) bc_vm_envLen(env_len);

	// Clear the files and expressions vectors, just in case. This marks them as
	// *not* allocated.
	bc_vec_clear(&vm.files);
	bc_vec_clear(&vm.exprs);

#if !BC_ENABLE_LIBRARY

	// Initialize the slab vectors.
	bc_slabvec_init(&vm.main_const_slab);
	bc_slabvec_init(&vm.main_slabs);
	bc_slabvec_init(&vm.other_slabs);

#endif // !BC_ENABLE_LIBRARY

	// Initialize the program and main parser. These have to be in this order
	// because the program has to be initialized first, since a pointer to it is
	// passed to the parser.
	bc_program_init(&vm.prog);
	bc_parse_init(&vm.prs, &vm.prog, BC_PROG_MAIN);

#if BC_ENABLED
	// bc checks this environment variable to see if it should run in standard
	// mode.
	if (BC_IS_BC) {

		char* var = bc_vm_getenv("POSIXLY_CORRECT");

		vm.flags |= BC_FLAG_S * (var != NULL);
		bc_vm_getenvFree(var);
	}
#endif // BC_ENABLED

	// Set defaults.
	vm.flags |= BC_TTY ? BC_FLAG_P | BC_FLAG_R : 0;
	vm.flags |= BC_I ? BC_FLAG_Q : 0;

#if BC_ENABLED
	if (BC_IS_BC && BC_I) {
		// Set whether we print the banner or not.
		bc_vm_setenvFlag("BC_BANNER", BC_DEFAULT_BANNER, BC_FLAG_Q);
	}
#endif // BC_ENABLED

	// Are we in TTY mode?
	if (BC_TTY) {

		const char* const env_tty = BC_IS_BC ? "BC_TTY_MODE" : "DC_TTY_MODE";
		int env_tty_def = BC_IS_BC ? BC_DEFAULT_TTY_MODE : DC_DEFAULT_TTY_MODE;
		const char* const env_prompt = BC_IS_BC ? "BC_PROMPT" : "DC_PROMPT";
		int env_prompt_def = BC_IS_BC ? BC_DEFAULT_PROMPT : DC_DEFAULT_PROMPT;

		// Set flags for TTY mode and prompt.
		bc_vm_setenvFlag(env_tty, env_tty_def, BC_FLAG_TTY);
		bc_vm_setenvFlag(env_prompt, tty ? env_prompt_def : 0, BC_FLAG_P);

#if BC_ENABLE_HISTORY
		// If TTY mode is used, activate history.
		if (BC_TTY) bc_history_init(&vm.history);
#endif // BC_ENABLE_HISTORY
	}

	// Process environment and command-line arguments.
	bc_vm_envArgs(env_args);
	bc_args(argc, argv, true);

	// If we are in interactive mode...
	if (BC_I) {

		const char* const env_sigint = BC_IS_BC ? "BC_SIGINT_RESET" :
		                                          "DC_SIGINT_RESET";
		int env_sigint_def = BC_IS_BC ? BC_DEFAULT_SIGINT_RESET :
		                                DC_DEFAULT_SIGINT_RESET;

		// Set whether we reset on SIGINT or not.
		bc_vm_setenvFlag(env_sigint, env_sigint_def, BC_FLAG_SIGINT);
	}

#if BC_ENABLED
	// Disable global stacks in POSIX mode.
	if (BC_IS_POSIX) vm.flags &= ~(BC_FLAG_G);
#endif // BC_ENABLED

#if BC_ENABLED
	// Print the banner if allowed. We have to be in bc, in interactive mode,
	// and not be quieted by command-line option or environment variable.
	if (BC_IS_BC && BC_I && (vm.flags & BC_FLAG_Q)) {
		bc_vm_info(NULL);
		bc_file_putchar(&vm.fout, bc_flush_none, '\n');
		bc_file_flush(&vm.fout, bc_flush_none);
	}
#endif // BC_ENABLED

	BC_SIG_UNLOCK;

	// Start executing.
	bc_vm_exec();
}
#endif // !BC_ENABLE_LIBRARY

void bc_vm_init(void) {

	BC_SIG_ASSERT_LOCKED;

#if !BC_ENABLE_LIBRARY
	// Set up the constant zero.
	bc_num_setup(&vm.zero, vm.zero_num, BC_VM_ONE_CAP);
#endif // !BC_ENABLE_LIBRARY

	// Set up more constant BcNum's.
	bc_num_setup(&vm.one, vm.one_num, BC_VM_ONE_CAP);
	bc_num_one(&vm.one);

	// Set up more constant BcNum's.
	memcpy(vm.max_num, bc_num_bigdigMax,
	       bc_num_bigdigMax_size * sizeof(BcDig));
	memcpy(vm.max2_num, bc_num_bigdigMax2,
	       bc_num_bigdigMax2_size * sizeof(BcDig));
	bc_num_setup(&vm.max, vm.max_num, BC_NUM_BIGDIG_LOG10);
	bc_num_setup(&vm.max2, vm.max2_num, BC_NUM_BIGDIG_LOG10);
	vm.max.len = bc_num_bigdigMax_size;
	vm.max2.len = bc_num_bigdigMax2_size;

	// Set up the maxes for the globals.
	vm.maxes[BC_PROG_GLOBALS_IBASE] = BC_NUM_MAX_POSIX_IBASE;
	vm.maxes[BC_PROG_GLOBALS_OBASE] = BC_MAX_OBASE;
	vm.maxes[BC_PROG_GLOBALS_SCALE] = BC_MAX_SCALE;

#if BC_ENABLE_EXTRA_MATH
	vm.maxes[BC_PROG_MAX_RAND] = ((BcRand) 0) - 1;
#endif // BC_ENABLE_EXTRA_MATH

#if BC_ENABLED
#if !BC_ENABLE_LIBRARY
	// bc has a higher max ibase when it's not in POSIX mode.
	if (BC_IS_BC && !BC_IS_POSIX)
#endif // !BC_ENABLE_LIBRARY
	{
		vm.maxes[BC_PROG_GLOBALS_IBASE] = BC_NUM_MAX_IBASE;
	}
#endif // BC_ENABLED
}

#if BC_ENABLE_LIBRARY
void bc_vm_atexit(void) {

	bc_vm_shutdown();

#ifndef NDEBUG
	bc_vec_free(&vm.jmp_bufs);
#endif // NDEBUG
}
#else // BC_ENABLE_LIBRARY
int bc_vm_atexit(int status) {

	// Set the status correctly.
	int s = BC_STATUS_IS_ERROR(status) ? status : BC_STATUS_SUCCESS;

	bc_vm_shutdown();

#ifndef NDEBUG
	bc_vec_free(&vm.jmp_bufs);
#endif // NDEBUG

	return s;
}
#endif // BC_ENABLE_LIBRARY
