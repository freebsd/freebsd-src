/* statement.h

   Definitions for executable statements... */

/*
 * Copyright (c) 1996-1999 Internet Software Consortium.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of The Internet Software Consortium nor the names
 *    of its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INTERNET SOFTWARE CONSORTIUM AND
 * CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE INTERNET SOFTWARE CONSORTIUM OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This software has been written for the Internet Software Consortium
 * by Ted Lemon in cooperation with Vixie Enterprises and Nominum, Inc.
 * To learn more about the Internet Software Consortium, see
 * ``http://www.isc.org/''.  To learn more about Vixie Enterprises,
 * see ``http://www.vix.com''.   To learn more about Nominum, Inc., see
 * ``http://www.nominum.com''.
 */

struct executable_statement {
	int refcnt;
	struct executable_statement *next;
	enum statement_op {
		null_statement,
		if_statement,
		add_statement,
		eval_statement,
		break_statement,
		default_option_statement,
		supersede_option_statement,
		append_option_statement,
		prepend_option_statement,
		send_option_statement,
		statements_statement,
		on_statement,
		switch_statement,
		case_statement,
		default_statement,
		set_statement,
		unset_statement,
		let_statement,
		define_statement,
		log_statement,
		return_statement
	} op;
	union {
		struct {
			struct executable_statement *tc, *fc;
			struct expression *expr;
		} ie;
		struct expression *eval;
		struct expression *retval;
		struct class *add;
		struct option_cache *option;
		struct option_cache *supersede;
		struct option_cache *prepend;
		struct option_cache *append;
		struct executable_statement *statements;
		struct {
			int evtypes;
#			define ON_COMMIT  1
#			define ON_EXPIRY  2
#			define ON_RELEASE 4
#			define ON_TRANSMISSION 8
			struct executable_statement *statements;
		} on;
		struct {
			struct expression *expr;
			struct executable_statement *statements;
		} s_switch;
		struct expression *c_case;
		struct {
			char *name;
			struct expression *expr;
			struct executable_statement *statements;
		} set, let;
		char *unset;
		struct {
			enum {
				log_priority_fatal,
				log_priority_error,
				log_priority_debug,
				log_priority_info
			} priority;
			struct expression *expr;
		} log;
	} data;
};

