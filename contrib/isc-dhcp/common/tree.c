/* tree.c

   Routines for manipulating parse trees... */

/*
 * Copyright (c) 1995-2002 Internet Software Consortium.
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

#ifndef lint
static char copyright[] =
"$Id: tree.c,v 1.101.2.7 2002/11/17 02:27:00 dhankins Exp $ Copyright (c) 1995-2002 The Internet Software Consortium.  All rights reserved.\n";
#endif /* not lint */

#include "dhcpd.h"
#include <omapip/omapip_p.h>

struct binding_scope *global_scope;

static int do_host_lookup PROTO ((struct data_string *,
				  struct dns_host_entry *));

#ifdef NSUPDATE
struct __res_state resolver_state;
int resolver_inited = 0;
#endif

pair cons (car, cdr)
	caddr_t car;
	pair cdr;
{
	pair foo = (pair)dmalloc (sizeof *foo, MDL);
	if (!foo)
		log_fatal ("no memory for cons.");
	foo -> car = car;
	foo -> cdr = cdr;
	return foo;
}

int make_const_option_cache (oc, buffer, data, len, option, file, line)
	struct option_cache **oc;
	struct buffer **buffer;
	u_int8_t *data;
	unsigned len;
	struct option *option;
	const char *file;
	int line;
{
	struct buffer *bp;

	if (buffer) {
		bp = *buffer;
		*buffer = 0;
	} else {
		bp = (struct buffer *)0;
		if (!buffer_allocate (&bp, len, file, line)) {
			log_error ("%s(%d): can't allocate buffer.",
				   file, line);
			return 0;
		}
	}

	if (!option_cache_allocate (oc, file, line)) {
		log_error ("%s(%d): can't allocate option cache.", file, line);
		buffer_dereference (&bp, file, line);
		return 0;
	}

	(*oc) -> data.len = len;
	(*oc) -> data.buffer = bp;
	(*oc) -> data.data = &bp -> data [0];
	(*oc) -> data.terminated = 0;
	if (data)
		memcpy (&bp -> data [0], data, len);
	(*oc) -> option = option;
	return 1;
}

int make_host_lookup (expr, name)
	struct expression **expr;
	const char *name;
{
	if (!expression_allocate (expr, MDL)) {
		log_error ("No memory for host lookup tree node.");
		return 0;
	}
	(*expr) -> op = expr_host_lookup;
	if (!enter_dns_host (&((*expr) -> data.host_lookup), name)) {
		expression_dereference (expr, MDL);
		return 0;
	}
	return 1;
}

int enter_dns_host (dh, name)
	struct dns_host_entry **dh;
	const char *name;
{
	/* XXX This should really keep a hash table of hostnames
	   XXX and just add a new reference to a hostname that
	   XXX already exists, if possible, rather than creating
	   XXX a new structure. */
	if (!dns_host_entry_allocate (dh, name, MDL)) {
		log_error ("Can't allocate space for new host.");
		return 0;
	}
	return 1;
}

int make_const_data (struct expression **expr, const unsigned char *data,
		     unsigned len, int terminated, int allocate,
		     const char *file, int line)
{
	struct expression *nt;

	if (!expression_allocate (expr, file, line)) {
		log_error ("No memory for make_const_data tree node.");
		return 0;
	}
	nt = *expr;

	if (len) {
		if (allocate) {
			if (!buffer_allocate (&nt -> data.const_data.buffer,
					      len + terminated, file, line)) {
				log_error ("Can't allocate const_data buffer");
				expression_dereference (expr, file, line);
				return 0;
			}
			nt -> data.const_data.data =
				&nt -> data.const_data.buffer -> data [0];
			memcpy (nt -> data.const_data.buffer -> data,
				data, len + terminated);
		} else 
			nt -> data.const_data.data = data;
		nt -> data.const_data.terminated = terminated;
	} else
		nt -> data.const_data.data = 0;

	nt -> op = expr_const_data;
	nt -> data.const_data.len = len;
	return 1;
}

int make_const_int (expr, val)
	struct expression **expr;
	unsigned long val;
{
	if (!expression_allocate (expr, MDL)) {
		log_error ("No memory for make_const_int tree node.");
		return 0;
	}

	(*expr) -> op = expr_const_int;
	(*expr) -> data.const_int = val;
	return 1;
}

int make_concat (expr, left, right)
	struct expression **expr;
	struct expression *left, *right;
{
	/* If we're concatenating a null tree to a non-null tree, just
	   return the non-null tree; if both trees are null, return
	   a null tree. */
	if (!left) {
		if (!right)
			return 0;
		expression_reference (expr, right, MDL);
		return 1;
	}
	if (!right) {
		expression_reference (expr, left, MDL);
		return 1;
	}
			
	/* Otherwise, allocate a new node to concatenate the two. */
	if (!expression_allocate (expr, MDL)) {
		log_error ("No memory for concatenation expression node.");
		return 0;
	}
		
	(*expr) -> op = expr_concat;
	expression_reference (&(*expr) -> data.concat [0], left, MDL);
	expression_reference (&(*expr) -> data.concat [1], right, MDL);
	return 1;
}

int make_encapsulation (expr, name)
	struct expression **expr;
	struct data_string *name;
{
	/* Allocate a new node to store the encapsulation. */
	if (!expression_allocate (expr, MDL)) {
		log_error ("No memory for encapsulation expression node.");
		return 0;
	}
		
	(*expr) -> op = expr_encapsulate;
	data_string_copy (&(*expr) -> data.encapsulate, name, MDL);
	return 1;
}

int make_substring (new, expr, offset, length)
	struct expression **new;
	struct expression *expr;
	struct expression *offset;
	struct expression *length;
{
	/* Allocate an expression node to compute the substring. */
	if (!expression_allocate (new, MDL)) {
		log_error ("no memory for substring expression.");
		return 0;
	}
	(*new) -> op = expr_substring;
	expression_reference (&(*new) -> data.substring.expr, expr, MDL);
	expression_reference (&(*new) -> data.substring.offset, offset, MDL);
	expression_reference (&(*new) -> data.substring.len, length, MDL);
	return 1;
}

int make_limit (new, expr, limit)
	struct expression **new;
	struct expression *expr;
	int limit;
{
	struct expression *rv;

	/* Allocate a node to enforce a limit on evaluation. */
	if (!expression_allocate (new, MDL))
		log_error ("no memory for limit expression");
	(*new) -> op = expr_substring;
	expression_reference (&(*new) -> data.substring.expr, expr, MDL);

	/* Offset is a constant 0. */
	if (!expression_allocate (&(*new) -> data.substring.offset, MDL)) {
		log_error ("no memory for limit offset expression");
		expression_dereference (new, MDL);
		return 0;
	}
	(*new) -> data.substring.offset -> op = expr_const_int;
	(*new) -> data.substring.offset -> data.const_int = 0;

	/* Length is a constant: the specified limit. */
	if (!expression_allocate (&(*new) -> data.substring.len, MDL)) {
		log_error ("no memory for limit length expression");
		expression_dereference (new, MDL);
		return 0;
	}
	(*new) -> data.substring.len -> op = expr_const_int;
	(*new) -> data.substring.len -> data.const_int = limit;

	return 1;
}

int option_cache (struct option_cache **oc, struct data_string *dp,
		  struct expression *expr, struct option *option,
		  const char *file, int line)
{
	if (!option_cache_allocate (oc, file, line))
		return 0;
	if (dp)
		data_string_copy (&(*oc) -> data, dp, file, line);
	if (expr)
		expression_reference (&(*oc) -> expression, expr, file, line);
	(*oc) -> option = option;
	return 1;
}

int make_let (result, name)
	struct executable_statement **result;
	const char *name;
{
	if (!(executable_statement_allocate (result, MDL)))
		return 0;
	
	(*result) -> op = let_statement;
	(*result) -> data.let.name = dmalloc (strlen (name) + 1, MDL);
	if (!(*result) -> data.let.name) {
		executable_statement_dereference (result, MDL);
		return 0;
	}
	strcpy ((*result) -> data.let.name, name);
	return 1;
}
		
static int do_host_lookup (result, dns)
	struct data_string *result;
	struct dns_host_entry *dns;
{
	struct hostent *h;
	unsigned i, count;
	unsigned new_len;

#ifdef DEBUG_EVAL
	log_debug ("time: now = %d  dns = %d  diff = %d",
	       cur_time, dns -> timeout, cur_time - dns -> timeout);
#endif

	/* If the record hasn't timed out, just copy the data and return. */
	if (cur_time <= dns -> timeout) {
#ifdef DEBUG_EVAL
		log_debug ("easy copy: %d %s",
		       dns -> data.len,
		       (dns -> data.len > 4
			? inet_ntoa (*(struct in_addr *)(dns -> data.data))
			: 0));
#endif
		data_string_copy (result, &dns -> data, MDL);
		return 1;
	}
#ifdef DEBUG_EVAL
	log_debug ("Looking up %s", dns -> hostname);
#endif

	/* Otherwise, look it up... */
	h = gethostbyname (dns -> hostname);
	if (!h) {
#ifndef NO_H_ERRNO
		switch (h_errno) {
		      case HOST_NOT_FOUND:
#endif
			log_error ("%s: host unknown.", dns -> hostname);
#ifndef NO_H_ERRNO
			break;
		      case TRY_AGAIN:
			log_error ("%s: temporary name server failure",
				   dns -> hostname);
			break;
		      case NO_RECOVERY:
			log_error ("%s: name server failed", dns -> hostname);
			break;
		      case NO_DATA:
			log_error ("%s: no A record associated with address",
				   dns -> hostname);
		}
#endif /* !NO_H_ERRNO */

		/* Okay to try again after a minute. */
		dns -> timeout = cur_time + 60;
		data_string_forget (&dns -> data, MDL);
		return 0;
	}

#ifdef DEBUG_EVAL
	log_debug ("Lookup succeeded; first address is %s",
	       inet_ntoa (h -> h_addr_list [0]));
#endif

	/* Count the number of addresses we got... */
	for (count = 0; h -> h_addr_list [count]; count++)
		;
	
	/* Dereference the old data, if any. */
	data_string_forget (&dns -> data, MDL);

	/* Do we need to allocate more memory? */
	new_len = count * h -> h_length;
	if (!buffer_allocate (&dns -> data.buffer, new_len, MDL))
	{
		log_error ("No memory for %s.", dns -> hostname);
		return 0;
	}

	dns -> data.data = &dns -> data.buffer -> data [0];
	dns -> data.len = new_len;
	dns -> data.terminated = 0;

	/* Addresses are conveniently stored one to the buffer, so we
	   have to copy them out one at a time... :'( */
	for (i = 0; i < count; i++) {
		memcpy (&dns -> data.buffer -> data [h -> h_length * i],
			h -> h_addr_list [i], (unsigned)(h -> h_length));
	}
#ifdef DEBUG_EVAL
	log_debug ("dns -> data: %x  h -> h_addr_list [0]: %x",
		   *(int *)(dns -> buffer), h -> h_addr_list [0]);
#endif

	/* XXX Set the timeout for an hour from now.
	   XXX This should really use the time on the DNS reply. */
	dns -> timeout = cur_time + 3600;

#ifdef DEBUG_EVAL
	log_debug ("hard copy: %d %s", dns -> data.len,
	       (dns -> data.len > 4
		? inet_ntoa (*(struct in_addr *)(dns -> data.data)) : 0));
#endif
	data_string_copy (result, &dns -> data, MDL);
	return 1;
}

int evaluate_expression (result, packet, lease, client_state,
			 in_options, cfg_options, scope, expr, file, line)
	struct binding_value **result;
	struct packet *packet;
	struct lease *lease;
	struct client_state *client_state;
	struct option_state *in_options;
	struct option_state *cfg_options;
	struct binding_scope **scope;
	struct expression *expr;
	const char *file;
	int line;
{
	struct binding_value *bv;
	int status;
	struct binding *binding;

	bv = (struct binding_value *)0;

	if (expr -> op == expr_variable_reference) {
		if (!scope || !*scope)
			return 0;

		binding = find_binding (*scope, expr -> data.variable);

		if (binding && binding -> value) {
			if (result)
				binding_value_reference (result,
							 binding -> value,
							 file, line);
			return 1;
		} else
			return 0;
	} else if (expr -> op == expr_funcall) {
		struct string_list *s;
		struct expression *arg;
		struct binding_scope *ns;
		struct binding *nb;

		if (!scope || !*scope) {
			log_error ("%s: no such function.",
				   expr -> data.funcall.name);
			return 0;
		}

		binding = find_binding (*scope, expr -> data.funcall.name);

		if (!binding || !binding -> value) {
			log_error ("%s: no such function.",
				   expr -> data.funcall.name);
			return 0;
		}
		if (binding -> value -> type != binding_function) {
			log_error ("%s: not a function.",
				   expr -> data.funcall.name);
			return 0;
		}

		/* Create a new binding scope in which to define
		   the arguments to the function. */
		ns = (struct binding_scope *)0;
		if (!binding_scope_allocate (&ns, MDL)) {
			log_error ("%s: can't allocate argument scope.",
				   expr -> data.funcall.name);
			return 0;
		}

		arg = expr -> data.funcall.arglist;
		s = binding -> value -> value.fundef -> args;
		while (arg && s) {
			nb = dmalloc (sizeof *nb, MDL);
			if (!nb) {
			      blb:
				binding_scope_dereference (&ns, MDL);
				return 0;
			} else {
				memset (nb, 0, sizeof *nb);
				nb -> name = dmalloc (strlen (s -> string) + 1,
						      MDL);
				if (nb -> name)
					strcpy (nb -> name, s -> string);
				else {
					dfree (nb, MDL);
					nb = (struct binding *)0;
					goto blb;
				}
			}
			evaluate_expression (&nb -> value, packet, lease,
					     client_state,
					     in_options, cfg_options, scope,
					     arg -> data.arg.val, file, line);
			nb -> next = ns -> bindings;
			ns -> bindings = nb;
			arg = arg -> data.arg.next;
			s = s -> next;
		}
		if (arg) {
			log_error ("%s: too many arguments.",
				   expr -> data.funcall.name);
			binding_scope_dereference (&ns, MDL);
			return 0;
		}
		if (s) {
			log_error ("%s: too few arguments.",
				   expr -> data.funcall.name);
			binding_scope_dereference (&ns, MDL);
			return 0;
		}

		if (scope && *scope)
			binding_scope_reference (&ns -> outer, *scope, MDL);

		status = (execute_statements
			  (&bv, packet,
			   lease, client_state, in_options, cfg_options, &ns,
			   binding -> value -> value.fundef -> statements));
		binding_scope_dereference (&ns, MDL);

		if (!bv)
			return 1;
        } else if (is_boolean_expression (expr)) {
		if (!binding_value_allocate (&bv, MDL))
			return 0;
		bv -> type = binding_boolean;
		status = (evaluate_boolean_expression
			  (&bv -> value.boolean, packet, lease, client_state,
			   in_options, cfg_options, scope, expr));
	} else if (is_numeric_expression (expr)) {
		if (!binding_value_allocate (&bv, MDL))
			return 0;
		bv -> type = binding_numeric;
		status = (evaluate_numeric_expression
			  (&bv -> value.intval, packet, lease, client_state,
			   in_options, cfg_options, scope, expr));
	} else if (is_data_expression  (expr)) {
		if (!binding_value_allocate (&bv, MDL))
			return 0;
		bv -> type = binding_data;
		status = (evaluate_data_expression
			  (&bv -> value.data, packet, lease, client_state,
			   in_options, cfg_options, scope, expr, MDL));
	} else if (is_dns_expression (expr)) {
#if defined (NSUPDATE)
		if (!binding_value_allocate (&bv, MDL))
			return 0;
		bv -> type = binding_dns;
		status = (evaluate_dns_expression
			  (&bv -> value.dns, packet, lease, client_state,
			   in_options, cfg_options, scope, expr));
#endif
	} else {
		log_error ("%s: invalid expression type: %d",
			   "evaluate_expression", expr -> op);
		return 0;
	}
	if (result && status)
		binding_value_reference (result, bv, file, line);
	binding_value_dereference (&bv, MDL);

	return status;
}

int binding_value_dereference (struct binding_value **v,
			       const char *file, int line)
{
	struct binding_value *bv = *v;

	*v = (struct binding_value *)0;

	/* Decrement the reference count.   If it's nonzero, we're
	   done. */
	--(bv -> refcnt);
	rc_register (file, line, v, bv, bv -> refcnt, 1, RC_MISC);
	if (bv -> refcnt > 0)
		return 1;
	if (bv -> refcnt < 0) {
		log_error ("%s(%d): negative refcnt!", file, line);
#if defined (DEBUG_RC_HISTORY)
		dump_rc_history (bv);
#endif
#if defined (POINTER_DEBUG)
		abort ();
#else
		return 0;
#endif
	}

	switch (bv -> type) {
	      case binding_boolean:
	      case binding_numeric:
		break;
	      case binding_data:
		if (bv -> value.data.buffer)
			data_string_forget (&bv -> value.data, file, line);
		break;
	      case binding_dns:
#if defined (NSUPDATE)
		if (bv -> value.dns) {
			if (bv -> value.dns -> r_data) {
				dfree (bv -> value.dns -> r_data_ephem, MDL);
				bv -> value.dns -> r_data = (unsigned char *)0;
				bv -> value.dns -> r_data_ephem =
					(unsigned char *)0;
			}
			minires_freeupdrec (bv -> value.dns);
		}
		break;
#endif
	      default:
		log_error ("%s(%d): invalid binding type: %d",
			   file, line, bv -> type);
		return 0;
	}
	dfree (bv, file, line);
	return 1;
}

#if defined (NSUPDATE)
int evaluate_dns_expression (result, packet, lease, client_state, in_options,
			     cfg_options, scope, expr)
	ns_updrec **result;
	struct packet *packet;
	struct lease *lease;
	struct client_state *client_state;
	struct option_state *in_options;
	struct option_state *cfg_options;
	struct binding_scope **scope;
	struct expression *expr;
{
	ns_updrec *foo;
	unsigned long ttl = 0;
	char *tname;
	struct data_string name, data;
	int r0, r1, r2, r3;

	if (!result || *result) {
		log_error ("evaluate_dns_expression called with non-null %s",
			   "result pointer");
#if defined (POINTER_DEBUG)
		abort ();
#else
		return 0;
#endif
	}
		
	switch (expr -> op) {
#if defined (NSUPDATE)
	      case expr_ns_add:
		r0 = evaluate_numeric_expression (&ttl, packet, lease,
						  client_state,
						  in_options, cfg_options,
						  scope,
						  expr -> data.ns_add.ttl);
		goto nsfinish;

	      case expr_ns_exists:
		ttl = 1;

	      case expr_ns_delete:
	      case expr_ns_not_exists:
		r0 = 1;
	      nsfinish:
		memset (&name, 0, sizeof name);
		r1 = evaluate_data_expression (&name, packet, lease,
					       client_state,
					       in_options, cfg_options, scope,
					       expr -> data.ns_add.rrname,
					       MDL);
		if (r1) {
			/* The result of the evaluation may or may not
			   be NUL-terminated, but we need it
			   terminated for sure, so we have to allocate
			   a buffer and terminate it. */
			tname = dmalloc (name.len + 1, MDL);
			if (!tname) {
				r2 = 0;
				r1 = 0;
				data_string_forget (&name, MDL);
			} else {
				memcpy (tname, name.data, name.len);
				tname [name.len] = 0;
				memset (&data, 0, sizeof data);
				r2 = evaluate_data_expression
					(&data, packet, lease, client_state,
					 in_options, cfg_options, scope,
					 expr -> data.ns_add.rrdata, MDL);
			}
		} else
			r2 = 0;
		if (r0 && r1 && (r2 || expr -> op != expr_ns_add)) {
		    *result = minires_mkupdrec (((expr -> op == expr_ns_add ||
						  expr -> op == expr_ns_delete)
						 ? S_UPDATE : S_PREREQ),
						tname,
						expr -> data.ns_add.rrclass,
						expr -> data.ns_add.rrtype,
						ttl);
		    if (!*result) {
			  ngood:
			    if (r2) {
				data_string_forget (&data, MDL);
				r2 = 0;
			    }
		    } else {
			if (data.len) {
				/* As a special case, if we get exactly
				   four bytes of data, it's an IP address
				   represented as a 32-bit quantity, which
				   is actually what we *should* be getting
				   here.   Because res_mkupdrec is currently
				   broken and expects a dotted quad, convert
				   it.   This should be fixed when the new
				   resolver is merged. */
				if (data.len == 4) {
				    (*result) -> r_data_ephem =
					    dmalloc (16, MDL);
				    if (!(*result) -> r_data_ephem)
					goto dpngood;
				    (*result) -> r_data =
					    (*result) -> r_data_ephem;
				    sprintf ((char *)(*result) -> r_data_ephem,
					     "%d.%d.%d.%d",
					     data.data [0], data.data [1],
					     data.data [2], data.data [3]);
				    (*result) -> r_size = 
					    strlen ((const char *)
						    (*result) -> r_data);
				} else {
				    (*result) -> r_size = data.len;
				    (*result) -> r_data_ephem =
					    dmalloc (data.len, MDL);
				    if (!(*result) -> r_data_ephem) {
				      dpngood: /* double plus ungood. */
					minires_freeupdrec (*result);
					*result = 0;
					goto ngood;
				    }
				    (*result) -> r_data =
					    (*result) -> r_data_ephem;
				    memcpy ((*result) -> r_data_ephem,
					    data.data, data.len);
				}
			} else {
				(*result) -> r_data = 0;
				(*result) -> r_size = 0;
			}
			switch (expr -> op) {
			      case expr_ns_add:
				(*result) -> r_opcode = ADD;
				break;
			      case expr_ns_delete:
				(*result) -> r_opcode = DELETE;
				break;
			      case expr_ns_exists:
				(*result) -> r_opcode = YXRRSET;
				break;
			      case expr_ns_not_exists:
				(*result) -> r_opcode = NXRRSET;
				break;

				/* Can't happen, but satisfy gcc. */
			      default:
				break;
			}
		    }
		}
		if (r1) {
			data_string_forget (&name, MDL);
			dfree (tname, MDL);
		}
		if (r2)
			data_string_forget (&data, MDL);
		/* One flaw in the thinking here: an IP address and an
		   ASCII string both look like data expressions, but
		   for A records, we want an ASCII string, not a
		   binary IP address.  Do I need to turn binary IP
		   addresses into a seperate type?  */
		return (r0 && r1 &&
			(r2 || expr -> op != expr_ns_add) && *result);

#else
	      case expr_ns_add:
	      case expr_ns_delete:
	      case expr_ns_exists:
	      case expr_ns_not_exists:
		return 0;
#endif
	      case expr_funcall:
		log_error ("%s: dns values for functions not supported.",
			   expr -> data.funcall.name);
		break;

	      case expr_variable_reference:
		log_error ("%s: dns values for variables not supported.",
			   expr -> data.variable);
		break;

	      case expr_check:
	      case expr_equal:
	      case expr_not_equal:
	      case expr_and:
	      case expr_or:
	      case expr_not:
	      case expr_match:
	      case expr_static:
	      case expr_known:
	      case expr_exists:
	      case expr_variable_exists:
		log_error ("Boolean opcode in evaluate_dns_expression: %d",
		      expr -> op);
		return 0;

	      case expr_none:
	      case expr_substring:
	      case expr_suffix:
	      case expr_option:
	      case expr_hardware:
	      case expr_const_data:
	      case expr_packet:
	      case expr_concat:
	      case expr_encapsulate:
	      case expr_host_lookup:
	      case expr_encode_int8:
	      case expr_encode_int16:
	      case expr_encode_int32:
	      case expr_binary_to_ascii:
	      case expr_reverse:
	      case expr_filename:
	      case expr_sname:
	      case expr_pick_first_value:
	      case expr_host_decl_name:
	      case expr_config_option:
	      case expr_leased_address:
	      case expr_null:
		log_error ("Data opcode in evaluate_dns_expression: %d",
		      expr -> op);
		return 0;

	      case expr_extract_int8:
	      case expr_extract_int16:
	      case expr_extract_int32:
	      case expr_const_int:
	      case expr_lease_time:
	      case expr_dns_transaction:
	      case expr_add:
	      case expr_subtract:
	      case expr_multiply:
	      case expr_divide:
	      case expr_remainder:
	      case expr_binary_and:
	      case expr_binary_or:
	      case expr_binary_xor:
	      case expr_client_state:
		log_error ("Numeric opcode in evaluate_dns_expression: %d",
		      expr -> op);
		return 0;

	      case expr_function:
		log_error ("Function opcode in evaluate_dns_expression: %d",
		      expr -> op);
		return 0;

	      case expr_arg:
		break;
	}

	log_error ("Bogus opcode in evaluate_dns_expression: %d",
		   expr -> op);
	return 0;
}
#endif /* defined (NSUPDATE) */

int evaluate_boolean_expression (result, packet, lease, client_state,
				 in_options, cfg_options, scope, expr)
	int *result;
	struct packet *packet;
	struct lease *lease;
	struct client_state *client_state;
	struct option_state *in_options;
	struct option_state *cfg_options;
	struct binding_scope **scope;
	struct expression *expr;
{
	struct data_string left, right;
	struct data_string rrtype, rrname, rrdata;
	unsigned long ttl;
	int srrtype, srrname, srrdata, sttl;
	int bleft, bright;
	int sleft, sright;
	struct binding *binding;
	struct binding_value *bv, *obv;

	switch (expr -> op) {
	      case expr_check:
		*result = check_collection (packet, lease,
					    expr -> data.check);
#if defined (DEBUG_EXPRESSIONS)
		log_debug ("bool: check (%s) returns %s",
			   expr -> data.check -> name,
			   *result ? "true" : "false");
#endif
		return 1;

	      case expr_equal:
	      case expr_not_equal:
		bv = obv = (struct binding_value *)0;
		sleft = evaluate_expression (&bv, packet, lease, client_state,
					     in_options, cfg_options, scope,
					     expr -> data.equal [0], MDL);
		sright = evaluate_expression (&obv, packet, lease,
					      client_state, in_options,
					      cfg_options, scope,
					      expr -> data.equal [1], MDL);
		if (sleft && sright) {
		    if (bv -> type != obv -> type)
			*result = expr -> op == expr_not_equal;
		    else {
			switch (obv -> type) {
			  case binding_boolean:
			    if (bv -> value.boolean == obv -> value.boolean)
				*result = expr -> op == expr_equal;
			    else
				*result = expr -> op == expr_not_equal;
			    break;

			  case binding_data:
			    if ((bv -> value.data.len ==
				 obv -> value.data.len) &&
				!memcmp (bv -> value.data.data,
					 obv -> value.data.data,
					 obv -> value.data.len))
				*result = expr -> op == expr_equal;
			    else
				*result = expr -> op == expr_not_equal;
			    break;

			  case binding_numeric:
			    if (bv -> value.intval == obv -> value.intval)
				*result = expr -> op == expr_equal;
			    else
				*result = expr -> op == expr_not_equal;
			    break;

			  case binding_dns:
#if defined (NSUPDATE)
			    /* XXX This should be a comparison for equal
			       XXX values, not for identity. */
			    if (bv -> value.dns == obv -> value.dns)
				*result = expr -> op == expr_equal;
			    else
				*result = expr -> op == expr_not_equal;
#else
				*result = expr -> op == expr_not_equal;
#endif
			    break;

			  case binding_function:
			    if (bv -> value.fundef == obv -> value.fundef)
				*result = expr -> op == expr_equal;
			    else
				*result = expr -> op == expr_not_equal;
			    break;
			  default:
			    *result = expr -> op == expr_not_equal;
			    break;
			}
		    }
		} else if (!sleft && !sright)
		    *result = expr -> op == expr_equal;
		else
		    *result = expr -> op == expr_not_equal;

#if defined (DEBUG_EXPRESSIONS)
		log_debug ("bool: %sequal = %s",
			   expr -> op == expr_not_equal ? "not" : "",
			   (*result ? "true" : "false"));
#endif
		if (sleft)
			binding_value_dereference (&bv, MDL);
		if (sright)
			binding_value_dereference (&obv, MDL);
		return 1;

	      case expr_and:
		sleft = evaluate_boolean_expression (&bleft, packet, lease,
						     client_state,
						     in_options, cfg_options,
						     scope,
						     expr -> data.and [0]);
		if (sleft && bleft)
			sright = evaluate_boolean_expression
				(&bright, packet, lease, client_state,
				 in_options, cfg_options,
				 scope, expr -> data.and [1]);
		else
			sright = bright = 0;

#if defined (DEBUG_EXPRESSIONS)
		log_debug ("bool: and (%s, %s) = %s",
		      sleft ? (bleft ? "true" : "false") : "NULL",
		      sright ? (bright ? "true" : "false") : "NULL",
		      ((sleft && sright)
		       ? (bleft && bright ? "true" : "false") : "NULL"));
#endif
		if (sleft && sright) {
			*result = bleft && bright;
			return 1;
		}
		return 0;

	      case expr_or:
		bleft = bright = 0;
		sleft = evaluate_boolean_expression (&bleft, packet, lease,
						     client_state,
						     in_options, cfg_options,
						     scope,
						     expr -> data.or [0]);
		if (!sleft || !bleft)
			sright = evaluate_boolean_expression
				(&bright, packet, lease, client_state,
				 in_options, cfg_options,
				 scope, expr -> data.or [1]);
		else
			sright = 0;
#if defined (DEBUG_EXPRESSIONS)
		log_debug ("bool: or (%s, %s) = %s",
		      sleft ? (bleft ? "true" : "false") : "NULL",
		      sright ? (bright ? "true" : "false") : "NULL",
		      ((sleft || sright)
		       ? (bleft || bright ? "true" : "false") : "NULL"));
#endif
		if (sleft || sright) {
			*result = bleft || bright;
			return 1;
		}
		return 0;

	      case expr_not:
		sleft = evaluate_boolean_expression (&bleft, packet, lease,
						     client_state,
						     in_options, cfg_options,
						     scope,
						     expr -> data.not);
#if defined (DEBUG_EXPRESSIONS)
		log_debug ("bool: not (%s) = %s",
		      sleft ? (bleft ? "true" : "false") : "NULL",
		      ((sleft && sright)
		       ? (!bleft ? "true" : "false") : "NULL"));

#endif
		if (sleft) {
			*result = !bleft;
			return 1;
		}
		return 0;

	      case expr_exists:
		memset (&left, 0, sizeof left);
		if (!in_options ||
		    !get_option (&left, expr -> data.exists -> universe,
				 packet, lease, client_state,
				 in_options, cfg_options, in_options,
				 scope, expr -> data.exists -> code, MDL))
			*result = 0;
		else {
			*result = 1;
			data_string_forget (&left, MDL);
		}
#if defined (DEBUG_EXPRESSIONS)
		log_debug ("bool: exists %s.%s = %s",
			   expr -> data.option -> universe -> name,
			   expr -> data.option -> name,
			   *result ? "true" : "false");
#endif
		return 1;

	      case expr_known:
		if (!packet) {
#if defined (DEBUG_EXPRESSIONS)
			log_debug ("bool: known = NULL");
#endif
			return 0;
		}
#if defined (DEBUG_EXPRESSIONS)
		log_debug ("bool: known = %s",
			  packet -> known ? "true" : "false");
#endif
		*result = packet -> known;
		return 1;

	      case expr_static:
		if (!lease || !(lease -> flags & STATIC_LEASE)) {
#if defined (DEBUG_EXPRESSIONS)
			log_debug ("bool: static = false (%s %s %s %d)",
				   lease ? "y" : "n",
				   (lease && (lease -> flags & STATIC_LEASE)
				    ? "y" : "n"),
				   piaddr (lease -> ip_addr),
				   lease ? lease -> flags : 0);
#endif
			*result = 0;
			return 1;
		}
#if defined (DEBUG_EXPRESSIONS)
		log_debug ("bool: static = true");
#endif
		*result = 1;
		return 1;

	      case expr_variable_exists:
		if (scope && *scope) {
			binding = find_binding (*scope, expr -> data.variable);

			if (binding) {
				if (binding -> value)
					*result = 1;
				else
					*result = 0;
			} else
				*result = 0;
		} else
			*result = 0;
#if defined (DEBUG_EXPRESSIONS)
		log_debug ("boolean: %s? = %s", expr -> data.variable,
			   *result ? "true" : "false");
#endif
		return 1;

	      case expr_variable_reference:
		if (scope && *scope) {
		    binding = find_binding (*scope, expr -> data.variable);

		    if (binding && binding -> value) {
			if (binding -> value -> type ==
			    binding_boolean) {
				*result = binding -> value -> value.boolean;
				sleft = 1;
			} else {
				log_error ("binding type %d in %s.",
					   binding -> value -> type,
					   "evaluate_boolean_expression");
				sleft = 0;
			}
		    } else
			    sleft = 0;
		} else
			sleft = 0;
#if defined (DEBUG_EXPRESSIONS)
		log_debug ("boolean: %s = %s", expr -> data.variable,
			   sleft ? (*result ? "true" : "false") : "NULL");
#endif
		return sleft;

	      case expr_funcall:
		bv = (struct binding_value *)0;
		sleft = evaluate_expression (&bv, packet, lease, client_state,
					  in_options, cfg_options,
					  scope, expr, MDL);
		if (sleft) {
			if (bv -> type != binding_boolean)
				log_error ("%s() returned type %d in %s.",
					   expr -> data.funcall.name,
					   bv -> type,
					   "evaluate_boolean_expression");
			else
				*result = bv -> value.boolean;
			binding_value_dereference (&bv, MDL);
		}
#if defined (DEBUG_EXPRESSIONS)
		log_debug ("boolean: %s() = %s", expr -> data.funcall.name,
			   sleft ? (*result ? "true" : "false") : "NULL");
#endif
		break;

	      case expr_none:
	      case expr_match:
	      case expr_substring:
	      case expr_suffix:
	      case expr_option:
	      case expr_hardware:
	      case expr_const_data:
	      case expr_packet:
	      case expr_concat:
	      case expr_encapsulate:
	      case expr_host_lookup:
	      case expr_encode_int8:
	      case expr_encode_int16:
	      case expr_encode_int32:
	      case expr_binary_to_ascii:
	      case expr_reverse:
	      case expr_pick_first_value:
	      case expr_host_decl_name:
	      case expr_config_option:
	      case expr_leased_address:
	      case expr_null:
	      case expr_filename:
	      case expr_sname:
		log_error ("Data opcode in evaluate_boolean_expression: %d",
		      expr -> op);
		return 0;

	      case expr_extract_int8:
	      case expr_extract_int16:
	      case expr_extract_int32:
	      case expr_const_int:
	      case expr_lease_time:
	      case expr_dns_transaction:
	      case expr_add:
	      case expr_subtract:
	      case expr_multiply:
	      case expr_divide:
	      case expr_remainder:
	      case expr_binary_and:
	      case expr_binary_or:
	      case expr_binary_xor:
	      case expr_client_state:
		log_error ("Numeric opcode in evaluate_boolean_expression: %d",
		      expr -> op);
		return 0;

	      case expr_ns_add:
	      case expr_ns_delete:
	      case expr_ns_exists:
	      case expr_ns_not_exists:
		log_error ("dns opcode in evaluate_boolean_expression: %d",
		      expr -> op);
		return 0;

	      case expr_function:
		log_error ("function definition in evaluate_boolean_expr");
		return 0;

	      case expr_arg:
		break;
	}

	log_error ("Bogus opcode in evaluate_boolean_expression: %d",
		   expr -> op);
	return 0;
}

int evaluate_data_expression (result, packet, lease, client_state,
			      in_options, cfg_options, scope, expr, file, line)
	struct data_string *result;
	struct packet *packet;
	struct lease *lease;
	struct client_state *client_state;
	struct option_state *in_options;
	struct option_state *cfg_options;
	struct binding_scope **scope;
	struct expression *expr;
	const char *file;
	int line;
{
	struct data_string data, other;
	unsigned long offset, len, i;
	int s0, s1, s2, s3;
	int status;
	struct binding *binding;
	char *s;
	struct binding_value *bv;

	switch (expr -> op) {
		/* Extract N bytes starting at byte M of a data string. */
	      case expr_substring:
		memset (&data, 0, sizeof data);
		s0 = evaluate_data_expression (&data, packet, lease,
					       client_state,
					       in_options, cfg_options, scope,
					       expr -> data.substring.expr,
					       MDL);

		/* Evaluate the offset and length. */
		s1 = evaluate_numeric_expression
			(&offset, packet, lease, client_state, in_options,
			 cfg_options, scope, expr -> data.substring.offset);
		s2 = evaluate_numeric_expression (&len, packet, lease,
						  client_state,
						  in_options, cfg_options,
						  scope,
						  expr -> data.substring.len);

		if (s0 && s1 && s2) {
			/* If the offset is after end of the string,
			   return an empty string.  Otherwise, do the
			   adjustments and return what's left. */
			if (data.len > offset) {
				data_string_copy (result, &data, file, line);
				result -> len -= offset;
				if (result -> len > len) {
					result -> len = len;
					result -> terminated = 0;
				}
				result -> data += offset;
			}
			s3 = 1;
		} else
			s3 = 0;

#if defined (DEBUG_EXPRESSIONS)
		log_debug ("data: substring (%s, %s, %s) = %s",
		      s0 ? print_hex_1 (data.len, data.data, 30) : "NULL",
		      s1 ? print_dec_1 (offset) : "NULL",
		      s2 ? print_dec_2 (len) : "NULL",
		      (s3 ? print_hex_2 (result -> len, result -> data, 30)
		          : "NULL"));
#endif
		if (s0)
			data_string_forget (&data, MDL);
		if (s3)
			return 1;
		return 0;


		/* Extract the last N bytes of a data string. */
	      case expr_suffix:
		memset (&data, 0, sizeof data);
		s0 = evaluate_data_expression (&data, packet, lease,
					       client_state,
					       in_options, cfg_options, scope,
					       expr -> data.suffix.expr, MDL);
		/* Evaluate the length. */
		s1 = evaluate_numeric_expression (&len, packet, lease,
						  client_state,
						  in_options, cfg_options,
						  scope,
						  expr -> data.suffix.len);
		if (s0 && s1) {
			data_string_copy (result, &data, file, line);

			/* If we are returning the last N bytes of a
			   string whose length is <= N, just return
			   the string - otherwise, compute a new
			   starting address and decrease the
			   length. */
			if (data.len > len) {
				result -> data += data.len - len;
				result -> len = len;
			}
			data_string_forget (&data, MDL);
		}

#if defined (DEBUG_EXPRESSIONS)
		log_debug ("data: suffix (%s, %s) = %s",
		      s0 ? print_hex_1 (data.len, data.data, 30) : "NULL",
		      s1 ? print_dec_1 (len) : "NULL",
		      ((s0 && s1)
		       ? print_hex_2 (result -> len, result -> data, 30)
		       : "NULL"));
#endif
		return s0 && s1;

		/* Extract an option. */
	      case expr_option:
		if (in_options)
		    s0 = get_option (result,
				     expr -> data.option -> universe,
				     packet, lease, client_state,
				     in_options, cfg_options, in_options,
				     scope, expr -> data.option -> code,
				     file, line);
		else
			s0 = 0;

#if defined (DEBUG_EXPRESSIONS)
		log_debug ("data: option %s.%s = %s",
		      expr -> data.option -> universe -> name,
		      expr -> data.option -> name,
		      s0 ? print_hex_1 (result -> len, result -> data, 60)
		      : "NULL");
#endif
		return s0;

	      case expr_config_option:
		if (cfg_options)
		    s0 = get_option (result,
				     expr -> data.option -> universe,
				     packet, lease, client_state,
				     in_options, cfg_options, cfg_options,
				     scope, expr -> data.option -> code,
				     file, line);
		else
			s0 = 0;

#if defined (DEBUG_EXPRESSIONS)
		log_debug ("data: config-option %s.%s = %s",
		      expr -> data.option -> universe -> name,
		      expr -> data.option -> name,
		      s0 ? print_hex_1 (result -> len, result -> data, 60)
		      : "NULL");
#endif
		return s0;

		/* Combine the hardware type and address. */
	      case expr_hardware:
		/* On the client, hardware is our hardware. */
		if (client_state) {
			memset (result, 0, sizeof *result);
			result -> data =
				client_state -> interface -> hw_address.hbuf;
			result -> len =
				client_state -> interface -> hw_address.hlen;
#if defined (DEBUG_EXPRESSIONS)
			log_debug ("data: hardware = %s",
				   print_hex_1 (result -> len,
						result -> data, 60));
#endif
			return 1;
		}

		/* The server cares about the client's hardware address,
		   so only in the case where we are examining a packet can
		   we return anything. */
		if (!packet || !packet -> raw) {
			log_error ("data: hardware: raw packet not available");
			return 0;
		}
		if (packet -> raw -> hlen > sizeof packet -> raw -> chaddr) {
			log_error ("data: hardware: invalid hlen (%d)\n",
				   packet -> raw -> hlen);
			return 0;
		}
		result -> len = packet -> raw -> hlen + 1;
		if (buffer_allocate (&result -> buffer, result -> len,
				     file, line)) {
			result -> data = &result -> buffer -> data [0];
			result -> buffer -> data [0] = packet -> raw -> htype;
			memcpy (&result -> buffer -> data [1],
				packet -> raw -> chaddr,
				packet -> raw -> hlen);
			result -> terminated = 0;
		} else {
			log_error ("data: hardware: no memory for buffer.");
			return 0;
		}
#if defined (DEBUG_EXPRESSIONS)
		log_debug ("data: hardware = %s",
		      print_hex_1 (result -> len, result -> data, 60));
#endif
		return 1;

		/* Extract part of the raw packet. */
	      case expr_packet:
		if (!packet || !packet -> raw) {
			log_error ("data: packet: raw packet not available");
			return 0;
		}

		s0 = evaluate_numeric_expression (&offset, packet, lease,
						  client_state,
						  in_options, cfg_options,
						  scope,
						  expr -> data.packet.offset);
		s1 = evaluate_numeric_expression (&len,
						  packet, lease, client_state,
						  in_options, cfg_options,
						  scope,
						  expr -> data.packet.len);
		if (s0 && s1 && offset < packet -> packet_length) {
			if (offset + len > packet -> packet_length)
				result -> len =
					packet -> packet_length - offset;
			else
				result -> len = len;
			if (buffer_allocate (&result -> buffer,
					     result -> len, file, line)) {
				result -> data = &result -> buffer -> data [0];
				memcpy (result -> buffer -> data,
					(((unsigned char *)(packet -> raw))
					 + offset), result -> len);
				result -> terminated = 0;
			} else {
				log_error ("data: packet: no buffer memory.");
				return 0;
			}
			s2 = 1;
		} else
			s2 = 0;
#if defined (DEBUG_EXPRESSIONS)
		log_debug ("data: packet (%ld, %ld) = %s",
		      offset, len,
		      s2 ? print_hex_1 (result -> len,
					result -> data, 60) : NULL);
#endif
		return s2;

		/* The encapsulation of all defined options in an
		   option space... */
	      case expr_encapsulate:
		if (cfg_options)
			s0 = option_space_encapsulate
				(result, packet, lease, client_state,
				 in_options, cfg_options, scope,
				 &expr -> data.encapsulate);
		else
			s0 = 0;

#if defined (DEBUG_EXPRESSIONS)
		log_debug ("data: encapsulate (%s) = %s",
			  expr -> data.encapsulate.data,
			  s0 ? print_hex_1 (result -> len,
					    result -> data, 60) : "NULL");
#endif
		return s0;

		/* Some constant data... */
	      case expr_const_data:
#if defined (DEBUG_EXPRESSIONS)
		log_debug ("data: const = %s",
		      print_hex_1 (expr -> data.const_data.len,
				   expr -> data.const_data.data, 60));
#endif
		data_string_copy (result,
				  &expr -> data.const_data, file, line);
		return 1;

		/* Hostname lookup... */
	      case expr_host_lookup:
		s0 = do_host_lookup (result, expr -> data.host_lookup);
#if defined (DEBUG_EXPRESSIONS)
		log_debug ("data: DNS lookup (%s) = %s",
		      expr -> data.host_lookup -> hostname,
		      (s0
		       ? print_dotted_quads (result -> len, result -> data)
		       : "NULL"));
#endif
		return s0;

		/* Concatenation... */
	      case expr_concat:
		memset (&data, 0, sizeof data);
		s0 = evaluate_data_expression (&data, packet, lease,
					       client_state,
					       in_options, cfg_options, scope,
					       expr -> data.concat [0], MDL);
		memset (&other, 0, sizeof other);
		s1 = evaluate_data_expression (&other, packet, lease,
					       client_state,
					       in_options, cfg_options, scope,
					       expr -> data.concat [1], MDL);

		if (s0 && s1) {
		    result -> len = data.len + other.len;
		    if (!buffer_allocate (&result -> buffer,
					  (result -> len + other.terminated),
					  file, line)) {
				log_error ("data: concat: no memory");
				result -> len = 0;
				data_string_forget (&data, MDL);
				data_string_forget (&other, MDL);
				return 0;
			}
			result -> data = &result -> buffer -> data [0];
			memcpy (result -> buffer -> data, data.data, data.len);
			memcpy (&result -> buffer -> data [data.len],
				other.data, other.len + other.terminated);
		}

		if (s0)
			data_string_forget (&data, MDL);
		if (s1)
			data_string_forget (&other, MDL);
#if defined (DEBUG_EXPRESSIONS)
		log_debug ("data: concat (%s, %s) = %s",
		      s0 ? print_hex_1 (data.len, data.data, 20) : "NULL",
		      s1 ? print_hex_2 (other.len, other.data, 20) : "NULL",
		      ((s0 && s1)
		       ? print_hex_3 (result -> len, result -> data, 30)
		       : "NULL"));
#endif
		return s0 && s1;

	      case expr_encode_int8:
		s0 = evaluate_numeric_expression (&len, packet, lease,
						  client_state,
						  in_options, cfg_options,
						  scope,
						  expr -> data.encode_int);
		if (s0) {
			result -> len = 1;
			if (!buffer_allocate (&result -> buffer,
					      1, file, line)) {
				log_error ("data: encode_int8: no memory");
				result -> len = 0;
				s0 = 0;
			} else {
				result -> data = &result -> buffer -> data [0];
				result -> buffer -> data [0] = len;
			}
		} else
			result -> len = 0;

#if defined (DEBUG_EXPRESSIONS)
		if (!s0)
			log_debug ("data: encode_int8 (NULL) = NULL");
		else
			log_debug ("data: encode_int8 (%ld) = %s", len,
				  print_hex_2 (result -> len,
					       result -> data, 20));
#endif
		return s0;
			
		
	      case expr_encode_int16:
		s0 = evaluate_numeric_expression (&len, packet, lease,
						  client_state,
						  in_options, cfg_options,
						  scope,
						  expr -> data.encode_int);
		if (s0) {
			result -> len = 2;
			if (!buffer_allocate (&result -> buffer, 2,
					      file, line)) {
				log_error ("data: encode_int16: no memory");
				result -> len = 0;
				s0 = 0;
			} else {
				result -> data = &result -> buffer -> data [0];
				putUShort (result -> buffer -> data, len);
			}
		} else
			result -> len = 0;

#if defined (DEBUG_EXPRESSIONS)
		if (!s0)
			log_debug ("data: encode_int16 (NULL) = NULL");
		else
			log_debug ("data: encode_int16 (%ld) = %s", len,
				  print_hex_2 (result -> len,
					       result -> data, 20));
#endif
		return s0;

	      case expr_encode_int32:
		s0 = evaluate_numeric_expression (&len, packet, lease,
						  client_state,
						  in_options, cfg_options,
						  scope,
						  expr -> data.encode_int);
		if (s0) {
			result -> len = 4;
			if (!buffer_allocate (&result -> buffer, 4,
					      file, line)) {
				log_error ("data: encode_int32: no memory");
				result -> len = 0;
				s0 = 0;
			} else {
				result -> data = &result -> buffer -> data [0];
				putULong (result -> buffer -> data, len);
			}
		} else
			result -> len = 0;

#if defined (DEBUG_EXPRESSIONS)
		if (!s0)
			log_debug ("data: encode_int32 (NULL) = NULL");
		else
			log_debug ("data: encode_int32 (%ld) = %s", len,
				  print_hex_2 (result -> len,
					       result -> data, 20));
#endif
		return s0;

	      case expr_binary_to_ascii:
		/* Evaluate the base (offset) and width (len): */
		s0 = evaluate_numeric_expression
			(&offset, packet, lease, client_state, in_options,
			 cfg_options, scope, expr -> data.b2a.base);
		s1 = evaluate_numeric_expression (&len, packet, lease,
						  client_state,
						  in_options, cfg_options,
						  scope,
						  expr -> data.b2a.width);

		/* Evaluate the seperator string. */
		memset (&data, 0, sizeof data);
		s2 = evaluate_data_expression (&data, packet, lease,
					       client_state,
					       in_options, cfg_options, scope,
					       expr -> data.b2a.seperator,
					       MDL);

		/* Evaluate the data to be converted. */
		memset (&other, 0, sizeof other);
		s3 = evaluate_data_expression (&other, packet, lease,
					       client_state,
					       in_options, cfg_options, scope,
					       expr -> data.b2a.buffer, MDL);

		if (s0 && s1 && s2 && s3) {
			unsigned buflen, i;

			if (len != 8 && len != 16 && len != 32) {
				log_info ("binary_to_ascii: %s %ld!",
					  "invalid width", len);
				goto b2a_out;
			}
			len /= 8;

			/* The buffer must be a multiple of the number's
			   width. */
			if (other.len % len) {
				log_info ("binary-to-ascii: %s %d %s %ld!",
					  "length of buffer", other.len,
					  "not a multiple of width", len);
				status = 0;
				goto b2a_out;
			}

			/* Count the width of the output. */
			buflen = 0;
			for (i = 0; i < other.len; i += len) {
				if (len == 1) {
					if (offset == 8) {
						if (other.data [i] < 8)
							buflen++;
						else if (other.data [i] < 64)
							buflen += 2;
						else
							buflen += 3;
					} else if (offset == 10) {
						if (other.data [i] < 10)
							buflen++;
						else if (other.data [i] < 100)
							buflen += 2;
						else
							buflen += 3;
					} else if (offset == 16) {
						if (other.data [i] < 16)
							buflen++;
						else
							buflen += 2;
					} else
						buflen += (converted_length
							   (&other.data [i],
							    offset, 1));
				} else
					buflen += (converted_length
						   (&other.data [i],
						    offset, len));
				if (i + len != other.len)
					buflen += data.len;
			}

			if (!buffer_allocate (&result -> buffer,
					      buflen + 1, file, line)) {
				log_error ("data: binary-to-ascii: no memory");
				status = 0;
				goto b2a_out;
			}
			result -> data = &result -> buffer -> data [0];
			result -> len = buflen;
			result -> terminated = 1;

			buflen = 0;
			for (i = 0; i < other.len; i += len) {
				buflen += (binary_to_ascii
					   (&result -> buffer -> data [buflen],
					    &other.data [i], offset, len));
				if (i + len != other.len) {
					memcpy (&result ->
						buffer -> data [buflen],
						data.data, data.len);
					buflen += data.len;
				}
			}
			/* NUL terminate. */
			result -> buffer -> data [buflen] = 0;
			status = 1;
		} else
			status = 0;

	      b2a_out:
#if defined (DEBUG_EXPRESSIONS)
		log_debug ("data: binary-to-ascii (%s, %s, %s, %s) = %s",
		      s0 ? print_dec_1 (offset) : "NULL",
		      s1 ? print_dec_2 (len) : "NULL",
		      s2 ? print_hex_1 (data.len, data.data, 30) : "NULL",
		      s3 ? print_hex_2 (other.len, other.data, 30) : "NULL",
		      (status ? print_hex_3 (result -> len, result -> data, 30)
		          : "NULL"));
#endif
		if (s2)
			data_string_forget (&data, MDL);
		if (s3)
			data_string_forget (&other, MDL);
		if (status)
			return 1;
		return 0;

	      case expr_reverse:
		/* Evaluate the width (len): */
		s0 = evaluate_numeric_expression
			(&len, packet, lease, client_state, in_options,
			 cfg_options, scope, expr -> data.reverse.width);

		/* Evaluate the data. */
		memset (&data, 0, sizeof data);
		s1 = evaluate_data_expression (&data, packet, lease,
					       client_state,
					       in_options, cfg_options, scope,
					       expr -> data.reverse.buffer,
					       MDL);

		if (s0 && s1) {
			char *upper;
			int i;

			/* The buffer must be a multiple of the number's
			   width. */
			if (data.len % len) {
				log_info ("reverse: %s %d %s %ld!",
					  "length of buffer", data.len,
					  "not a multiple of width", len);
				status = 0;
				goto reverse_out;
			}

			/* XXX reverse in place?   I don't think we can. */
			if (!buffer_allocate (&result -> buffer,
					      data.len, file, line)) {
				log_error ("data: reverse: no memory");
				status = 0;
				goto reverse_out;
			}
			result -> data = &result -> buffer -> data [0];
			result -> len = data.len;
			result -> terminated = 0;

			for (i = 0; i < data.len; i += len) {
				memcpy (&result -> buffer -> data [i],
					&data.data [data.len - i - len], len);
			}
			status = 1;
		} else
			status = 0;

	      reverse_out:
#if defined (DEBUG_EXPRESSIONS)
		log_debug ("data: reverse (%s, %s) = %s",
		      s0 ? print_dec_1 (len) : "NULL",
		      s1 ? print_hex_1 (data.len, data.data, 30) : "NULL",
		      (status ? print_hex_3 (result -> len, result -> data, 30)
		          : "NULL"));
#endif
		if (s0)
			data_string_forget (&data, MDL);
		if (status)
			return 1;
		return 0;

	      case expr_leased_address:
		if (!lease) {
			log_error ("data: leased_address: not available");
			return 0;
		}
		result -> len = lease -> ip_addr.len;
		if (buffer_allocate (&result -> buffer, result -> len,
				     file, line)) {
			result -> data = &result -> buffer -> data [0];
			memcpy (&result -> buffer -> data [0],
				lease -> ip_addr.iabuf, lease -> ip_addr.len);
			result -> terminated = 0;
		} else {
			log_error ("data: leased-address: no memory.");
			return 0;
		}
#if defined (DEBUG_EXPRESSIONS)
		log_debug ("data: leased-address = %s",
		      print_hex_1 (result -> len, result -> data, 60));
#endif
		return 1;

	      case expr_pick_first_value:
		memset (&data, 0, sizeof data);
		if ((evaluate_data_expression
		     (result, packet,
		      lease, client_state, in_options, cfg_options,
		      scope, expr -> data.pick_first_value.car, MDL))) {
#if defined (DEBUG_EXPRESSIONS)
			log_debug ("data: pick_first_value (%s, xxx)",
				   print_hex_1 (result -> len,
						result -> data, 40));
#endif
			return 1;
		}

		if (expr -> data.pick_first_value.cdr &&
		    (evaluate_data_expression
		     (result, packet,
		      lease, client_state, in_options, cfg_options,
		      scope, expr -> data.pick_first_value.cdr, MDL))) {
#if defined (DEBUG_EXPRESSIONS)
			log_debug ("data: pick_first_value (NULL, %s)",
				   print_hex_1 (result -> len,
						result -> data, 40));
#endif
			return 1;
		}

#if defined (DEBUG_EXPRESSIONS)
		log_debug ("data: pick_first_value (NULL, NULL) = NULL");
#endif
		return 0;

	      case expr_host_decl_name:
		if (!lease || !lease -> host) {
			log_error ("data: host_decl_name: not available");
			return 0;
		}
		result -> len = strlen (lease -> host -> name);
		if (buffer_allocate (&result -> buffer,
				     result -> len + 1, file, line)) {
			result -> data = &result -> buffer -> data [0];
			strcpy ((char *)&result -> buffer -> data [0],
				lease -> host -> name);
			result -> terminated = 1;
		} else {
			log_error ("data: host-decl-name: no memory.");
			return 0;
		}
#if defined (DEBUG_EXPRESSIONS)
		log_debug ("data: host-decl-name = %s", lease -> host -> name);
#endif
		return 1;

	      case expr_null:
#if defined (DEBUG_EXPRESSIONS)
		log_debug ("data: null = NULL");
#endif
		return 0;

	      case expr_variable_reference:
		if (scope && *scope) {
		    binding = find_binding (*scope, expr -> data.variable);

		    if (binding && binding -> value) {
			if (binding -> value -> type == binding_data) {
			    data_string_copy (result,
					      &binding -> value -> value.data,
					      file, line);
			    s0 = 1;
			} else if (binding -> value -> type != binding_data) {
			    log_error ("binding type %d in %s.",
				       binding -> value -> type,
				       "evaluate_data_expression");
			    s0 = 0;
			} else
			    s0 = 0;
		    } else
			s0 = 0;
		} else
		    s0 = 0;
#if defined (DEBUG_EXPRESSIONS)
		log_debug ("data: %s = %s", expr -> data.variable,
			   s0 ? print_hex_1 (result -> len,
					     result -> data, 50) : "NULL");
#endif
		return s0;

	      case expr_funcall:
		bv = (struct binding_value *)0;
		s0 = evaluate_expression (&bv, packet, lease, client_state,
					  in_options, cfg_options,
					  scope, expr, MDL);
		if (s0) {
			if (bv -> type != binding_data)
				log_error ("%s() returned type %d in %s.",
					   expr -> data.funcall.name,
					   bv -> type,
					   "evaluate_data_expression");
			else
				data_string_copy (result, &bv -> value.data,
						  file, line);
			binding_value_dereference (&bv, MDL);
		}
#if defined (DEBUG_EXPRESSIONS)
		log_debug ("data: %s = %s", expr -> data.funcall.name,
			   s0 ? print_hex_1 (result -> len,
					     result -> data, 50) : "NULL");
#endif
		break;

		/* Extract the filename. */
	      case expr_filename:
		if (packet && packet -> raw -> file [0]) {
			char *fn =
				memchr (packet -> raw -> file, 0,
					sizeof packet -> raw -> file);
			if (!fn)
				fn = ((char *)packet -> raw -> file +
				      sizeof packet -> raw -> file);
			result -> len = fn - &(packet -> raw -> file [0]);
			if (buffer_allocate (&result -> buffer,
					     result -> len + 1, file, line)) {
				result -> data = &result -> buffer -> data [0];
				memcpy (&result -> buffer -> data [0],
					packet -> raw -> file,
					result -> len);
				result -> buffer -> data [result -> len] = 0;
				result -> terminated = 1;
				s0 = 1;
			} else {
				log_error ("data: filename: no memory.");
				s0 = 0;
			}
		} else
			s0 = 0;

#if defined (DEBUG_EXPRESSIONS)
		log_info ("data: filename = \"%s\"",
			  s0 ? (const char *)(result -> data) : "NULL");
#endif
		return s0;

		/* Extract the server name. */
	      case expr_sname:
		if (packet && packet -> raw -> sname [0]) {
			char *fn =
				memchr (packet -> raw -> sname, 0,
					sizeof packet -> raw -> sname);
			if (!fn)
				fn = ((char *)packet -> raw -> sname + 
				      sizeof packet -> raw -> sname);
			result -> len = fn - &packet -> raw -> sname [0];
			if (buffer_allocate (&result -> buffer,
					     result -> len + 1, file, line)) {
				result -> data = &result -> buffer -> data [0];
				memcpy (&result -> buffer -> data [0],
					packet -> raw -> sname,
					result -> len);
				result -> buffer -> data [result -> len] = 0;
				result -> terminated = 1;
				s0 = 1;
			} else {
				log_error ("data: sname: no memory.");
				s0 = 0;
			}
		} else
			s0 = 0;

#if defined (DEBUG_EXPRESSIONS)
		log_info ("data: sname = \"%s\"",
			  s0 ? (const char *)(result -> data) : "NULL");
#endif
		return s0;

	      case expr_check:
	      case expr_equal:
	      case expr_not_equal:
	      case expr_and:
	      case expr_or:
	      case expr_not:
	      case expr_match:
	      case expr_static:
	      case expr_known:
	      case expr_none:
	      case expr_exists:
	      case expr_variable_exists:
		log_error ("Boolean opcode in evaluate_data_expression: %d",
		      expr -> op);
		return 0;

	      case expr_extract_int8:
	      case expr_extract_int16:
	      case expr_extract_int32:
	      case expr_const_int:
	      case expr_lease_time:
	      case expr_dns_transaction:
	      case expr_add:
	      case expr_subtract:
	      case expr_multiply:
	      case expr_divide:
	      case expr_remainder:
	      case expr_binary_and:
	      case expr_binary_or:
	      case expr_binary_xor:
	      case expr_client_state:
		log_error ("Numeric opcode in evaluate_data_expression: %d",
		      expr -> op);
		return 0;

	      case expr_ns_add:
	      case expr_ns_delete:
	      case expr_ns_exists:
	      case expr_ns_not_exists:
		log_error ("dns update opcode in evaluate_data_expression: %d",
		      expr -> op);
		return 0;

	      case expr_function:
		log_error ("function definition in evaluate_data_expression");
		return 0;

	      case expr_arg:
		break;
	}

	log_error ("Bogus opcode in evaluate_data_expression: %d", expr -> op);
	return 0;
}	

int evaluate_numeric_expression (result, packet, lease, client_state,
				 in_options, cfg_options, scope, expr)
	unsigned long *result;
	struct packet *packet;
	struct lease *lease;
	struct client_state *client_state;
	struct option_state *in_options;
	struct option_state *cfg_options;
	struct binding_scope **scope;
	struct expression *expr;
{
	struct data_string data;
	int status, sleft, sright;
#if defined (NSUPDATE)
	ns_updrec *nut;
	ns_updque uq;
#endif
	struct expression *cur, *next;
	struct binding *binding;
	struct binding_value *bv;
	unsigned long ileft, iright;

	switch (expr -> op) {
	      case expr_check:
	      case expr_equal:
	      case expr_not_equal:
	      case expr_and:
	      case expr_or:
	      case expr_not:
	      case expr_match:
	      case expr_static:
	      case expr_known:
	      case expr_none:
	      case expr_exists:
	      case expr_variable_exists:
		log_error ("Boolean opcode in evaluate_numeric_expression: %d",
		      expr -> op);
		return 0;

	      case expr_substring:
	      case expr_suffix:
	      case expr_option:
	      case expr_hardware:
	      case expr_const_data:
	      case expr_packet:
	      case expr_concat:
	      case expr_encapsulate:
	      case expr_host_lookup:
	      case expr_encode_int8:
	      case expr_encode_int16:
	      case expr_encode_int32:
	      case expr_binary_to_ascii:
	      case expr_reverse:
	      case expr_filename:
	      case expr_sname:
	      case expr_pick_first_value:
	      case expr_host_decl_name:
	      case expr_config_option:
	      case expr_leased_address:
	      case expr_null:
		log_error ("Data opcode in evaluate_numeric_expression: %d",
		      expr -> op);
		return 0;

	      case expr_extract_int8:
		memset (&data, 0, sizeof data);
		status = evaluate_data_expression
			(&data, packet, lease, client_state, in_options,
			 cfg_options, scope, expr -> data.extract_int, MDL);
		if (status)
			*result = data.data [0];
#if defined (DEBUG_EXPRESSIONS)
		log_debug ("num: extract_int8 (%s) = %s",
		      status ? print_hex_1 (data.len, data.data, 60) : "NULL",
		      status ? print_dec_1 (*result) : "NULL" );
#endif
		if (status) data_string_forget (&data, MDL);
		return status;

	      case expr_extract_int16:
		memset (&data, 0, sizeof data);
		status = (evaluate_data_expression
			  (&data, packet, lease, client_state, in_options,
			   cfg_options, scope, expr -> data.extract_int, MDL));
		if (status && data.len >= 2)
			*result = getUShort (data.data);
#if defined (DEBUG_EXPRESSIONS)
		log_debug ("num: extract_int16 (%s) = %ld",
		      ((status && data.len >= 2) ?
		       print_hex_1 (data.len, data.data, 60) : "NULL"),
		      *result);
#endif
		if (status) data_string_forget (&data, MDL);
		return (status && data.len >= 2);

	      case expr_extract_int32:
		memset (&data, 0, sizeof data);
		status = (evaluate_data_expression
			  (&data, packet, lease, client_state, in_options,
			   cfg_options, scope, expr -> data.extract_int, MDL));
		if (status && data.len >= 4)
			*result = getULong (data.data);
#if defined (DEBUG_EXPRESSIONS)
		log_debug ("num: extract_int32 (%s) = %ld",
		      ((status && data.len >= 4) ?
		       print_hex_1 (data.len, data.data, 60) : "NULL"),
		      *result);
#endif
		if (status) data_string_forget (&data, MDL);
		return (status && data.len >= 4);

	      case expr_const_int:
		*result = expr -> data.const_int;
#if defined (DEBUG_EXPRESSIONS)
		log_debug ("number: CONSTANT = %ld", *result);
#endif
		return 1;

	      case expr_lease_time:
		if (!lease) {
			log_error ("data: leased_lease: not available");
			return 0;
		}
		if (lease -> ends < cur_time) {
			log_error ("%s %lu when it is now %lu",
				   "data: lease_time: lease ends at",
				   (long)(lease -> ends), (long)cur_time);
			return 0;
		}
		*result = lease -> ends - cur_time;
#if defined (DEBUG_EXPRESSIONS)
		log_debug ("number: lease-time = (%lu - %lu) = %ld",
			   lease -> ends,
			   cur_time, *result);
#endif
		return 1;
 
	      case expr_dns_transaction:
#if !defined (NSUPDATE)
		return 0;
#else
		if (!resolver_inited) {
			minires_ninit (&resolver_state);
			resolver_inited = 1;
			resolver_state.retrans = 1;
			resolver_state.retry = 1;
		}
		ISC_LIST_INIT (uq);
		cur = expr;
		do {
		    next = cur -> data.dns_transaction.cdr;
		    nut = 0;
		    status = (evaluate_dns_expression
			      (&nut, packet,
			       lease, client_state, in_options, cfg_options,
			       scope, cur -> data.dns_transaction.car));
		    if (!status)
			    goto dns_bad;
		    ISC_LIST_APPEND (uq, nut, r_link);
		    cur = next;
		} while (next);

		/* Do the update and record the error code, if there was
		   an error; otherwise set it to NOERROR. */
		*result = minires_nupdate (&resolver_state,
					   ISC_LIST_HEAD (uq));
		status = 1;

		print_dns_status ((int)*result, &uq);

	      dns_bad:
		while (!ISC_LIST_EMPTY (uq)) {
			ns_updrec *tmp = ISC_LIST_HEAD (uq);
			ISC_LIST_UNLINK (uq, tmp, r_link);
			if (tmp -> r_data_ephem) {
				dfree (tmp -> r_data_ephem, MDL);
				tmp -> r_data = (unsigned char *)0;
				tmp -> r_data_ephem = (unsigned char *)0;
			}
			minires_freeupdrec (tmp);
		}
		return status;
#endif /* NSUPDATE */

	      case expr_variable_reference:
		if (scope && *scope) {
		    binding = find_binding (*scope, expr -> data.variable);

		    if (binding && binding -> value) {
			if (binding -> value -> type == binding_numeric) {
				*result = binding -> value -> value.intval;
			    status = 1;
			} else {
				log_error ("binding type %d in %s.",
					   binding -> value -> type,
					   "evaluate_numeric_expression");
				status = 0;
			}
		    } else
			status = 0;
		} else
		    status = 0;
#if defined (DEBUG_EXPRESSIONS)
		if (status)
			log_debug ("numeric: %s = %ld",
				   expr -> data.variable, *result);
		else
			log_debug ("numeric: %s = NULL",
				   expr -> data.variable);
#endif
		return status;

	      case expr_funcall:
		bv = (struct binding_value *)0;
		status = evaluate_expression (&bv, packet, lease,
					      client_state,
					      in_options, cfg_options,
					      scope, expr, MDL);
		if (status) {
			if (bv -> type != binding_numeric)
				log_error ("%s() returned type %d in %s.",
					   expr -> data.funcall.name,
					   bv -> type,
					   "evaluate_numeric_expression");
			else
				*result = bv -> value.intval;
			binding_value_dereference (&bv, MDL);
		}
#if defined (DEBUG_EXPRESSIONS)
		log_debug ("data: %s = %ld", expr -> data.funcall.name,
			   status ? *result : 0);
#endif
		break;

	      case expr_add:
		sleft = evaluate_numeric_expression (&ileft, packet, lease,
						     client_state,
						     in_options, cfg_options,
						     scope,
						     expr -> data.and [0]);
		sright = evaluate_numeric_expression (&iright, packet, lease,
						      client_state,
						      in_options, cfg_options,
						      scope,
						      expr -> data.and [1]);

#if defined (DEBUG_EXPRESSIONS)
		if (sleft && sright)
			log_debug ("num: %ld + %ld = %ld",
				   ileft, iright, ileft + iright);
		else if (sleft)
			log_debug ("num: %ld + NULL = NULL", ileft);
		else
			log_debug ("num: NULL + %ld = NULL", iright);
#endif
		if (sleft && sright) {
			*result = ileft + iright;
			return 1;
		}
		return 0;

	      case expr_subtract:
		sleft = evaluate_numeric_expression (&ileft, packet, lease,
						     client_state,
						     in_options, cfg_options,
						     scope,
						     expr -> data.and [0]);
		sright = evaluate_numeric_expression (&iright, packet, lease,
						      client_state,
						      in_options, cfg_options,
						      scope,
						      expr -> data.and [1]);

#if defined (DEBUG_EXPRESSIONS)
		if (sleft && sright)
			log_debug ("num: %ld - %ld = %ld",
				   ileft, iright, ileft - iright);
		else if (sleft)
			log_debug ("num: %ld - NULL = NULL", ileft);
		else
			log_debug ("num: NULL - %ld = NULL", iright);
#endif
		if (sleft && sright) {
			*result = ileft - iright;
			return 1;
		}
		return 0;

	      case expr_multiply:
		sleft = evaluate_numeric_expression (&ileft, packet, lease,
						     client_state,
						     in_options, cfg_options,
						     scope,
						     expr -> data.and [0]);
		sright = evaluate_numeric_expression (&iright, packet, lease,
						      client_state,
						      in_options, cfg_options,
						      scope,
						      expr -> data.and [1]);

#if defined (DEBUG_EXPRESSIONS)
		if (sleft && sright)
			log_debug ("num: %ld * %ld = %ld",
				   ileft, iright, ileft * iright);
		else if (sleft)
			log_debug ("num: %ld * NULL = NULL", ileft);
		else
			log_debug ("num: NULL * %ld = NULL", iright);
#endif
		if (sleft && sright) {
			*result = ileft * iright;
			return 1;
		}
		return 0;

	      case expr_divide:
		sleft = evaluate_numeric_expression (&ileft, packet, lease,
						     client_state,
						     in_options, cfg_options,
						     scope,
						     expr -> data.and [0]);
		sright = evaluate_numeric_expression (&iright, packet, lease,
						      client_state,
						      in_options, cfg_options,
						      scope,
						      expr -> data.and [1]);

#if defined (DEBUG_EXPRESSIONS)
		if (sleft && sright) {
			if (iright != 0)
				log_debug ("num: %ld / %ld = %ld",
					   ileft, iright, ileft / iright);
			else
				log_debug ("num: %ld / %ld = NULL",
					   ileft, iright);
		} else if (sleft)
			log_debug ("num: %ld / NULL = NULL", ileft);
		else
			log_debug ("num: NULL / %ld = NULL", iright);
#endif
		if (sleft && sright && iright) {
			*result = ileft / iright;
			return 1;
		}
		return 0;

	      case expr_remainder:
		sleft = evaluate_numeric_expression (&ileft, packet, lease,
						     client_state,
						     in_options, cfg_options,
						     scope,
						     expr -> data.and [0]);
		sright = evaluate_numeric_expression (&iright, packet, lease,
						      client_state,
						      in_options, cfg_options,
						      scope,
						      expr -> data.and [1]);

#if defined (DEBUG_EXPRESSIONS)
		if (sleft && sright) {
			if (iright != 0)
				log_debug ("num: %ld %% %ld = %ld",
					   ileft, iright, ileft % iright);
			else
				log_debug ("num: %ld %% %ld = NULL",
					   ileft, iright);
		} else if (sleft)
			log_debug ("num: %ld %% NULL = NULL", ileft);
		else
			log_debug ("num: NULL %% %ld = NULL", iright);
#endif
		if (sleft && sright && iright) {
			*result = ileft % iright;
			return 1;
		}
		return 0;

	      case expr_binary_and:
		sleft = evaluate_numeric_expression (&ileft, packet, lease,
						     client_state,
						     in_options, cfg_options,
						     scope,
						     expr -> data.and [0]);
		sright = evaluate_numeric_expression (&iright, packet, lease,
						      client_state,
						      in_options, cfg_options,
						      scope,
						      expr -> data.and [1]);

#if defined (DEBUG_EXPRESSIONS)
		if (sleft && sright)
			log_debug ("num: %ld | %ld = %ld",
				   ileft, iright, ileft & iright);
		else if (sleft)
			log_debug ("num: %ld & NULL = NULL", ileft);
		else
			log_debug ("num: NULL & %ld = NULL", iright);
#endif
		if (sleft && sright) {
			*result = ileft & iright;
			return 1;
		}
		return 0;

	      case expr_binary_or:
		sleft = evaluate_numeric_expression (&ileft, packet, lease,
						     client_state,
						     in_options, cfg_options,
						     scope,
						     expr -> data.and [0]);
		sright = evaluate_numeric_expression (&iright, packet, lease,
						      client_state,
						      in_options, cfg_options,
						      scope,
						      expr -> data.and [1]);

#if defined (DEBUG_EXPRESSIONS)
		if (sleft && sright)
			log_debug ("num: %ld | %ld = %ld",
				   ileft, iright, ileft | iright);
		else if (sleft)
			log_debug ("num: %ld | NULL = NULL", ileft);
		else
			log_debug ("num: NULL | %ld = NULL", iright);
#endif
		if (sleft && sright) {
			*result = ileft | iright;
			return 1;
		}
		return 0;

	      case expr_binary_xor:
		sleft = evaluate_numeric_expression (&ileft, packet, lease,
						     client_state,
						     in_options, cfg_options,
						     scope,
						     expr -> data.and [0]);
		sright = evaluate_numeric_expression (&iright, packet, lease,
						      client_state,
						      in_options, cfg_options,
						      scope,
						      expr -> data.and [1]);

#if defined (DEBUG_EXPRESSIONS)
		if (sleft && sright)
			log_debug ("num: %ld ^ %ld = %ld",
				   ileft, iright, ileft ^ iright);
		else if (sleft)
			log_debug ("num: %ld ^ NULL = NULL", ileft);
		else
			log_debug ("num: NULL ^ %ld = NULL", iright);
#endif
		if (sleft && sright) {
			*result = ileft ^ iright;
			return 1;
		}
		return 0;

	      case expr_client_state:
		if (client_state) {
#if defined (DEBUG_EXPRESSIONS)
			log_debug ("num: client-state = %d",
				   client_state -> state);
#endif
			*result = client_state -> state;
			return 1;
		} else {
#if defined (DEBUG_EXPRESSIONS)
			log_debug ("num: client-state = NULL");
#endif
			return 0;
		}

	      case expr_ns_add:
	      case expr_ns_delete:
	      case expr_ns_exists:
	      case expr_ns_not_exists:
		log_error ("dns opcode in evaluate_numeric_expression: %d",
		      expr -> op);
		return 0;

	      case expr_function:
		log_error ("function definition in evaluate_numeric_expr");
		return 0;

	      case expr_arg:
		break;
	}

	log_error ("evaluate_numeric_expression: bogus opcode %d", expr -> op);
	return 0;
}

/* Return data hanging off of an option cache structure, or if there
   isn't any, evaluate the expression hanging off of it and return the
   result of that evaluation.   There should never be both an expression
   and a valid data_string. */

int evaluate_option_cache (result, packet, lease, client_state,
			   in_options, cfg_options, scope, oc, file, line)
	struct data_string *result;
	struct packet *packet;
	struct lease *lease;
	struct client_state *client_state;
	struct option_state *in_options;
	struct option_state *cfg_options;
	struct binding_scope **scope;
	struct option_cache *oc;
	const char *file;
	int line;
{
	if (oc -> data.len) {
		data_string_copy (result, &oc -> data, file, line);
		return 1;
	}
	if (!oc -> expression)
		return 0;
	return evaluate_data_expression (result, packet, lease, client_state,
					 in_options, cfg_options, scope,
					 oc -> expression, file, line);
}

/* Evaluate an option cache and extract a boolean from the result,
   returning the boolean.   Return false if there is no data. */

int evaluate_boolean_option_cache (ignorep, packet,
				   lease, client_state, in_options,
				   cfg_options, scope, oc, file, line)
	int *ignorep;
	struct packet *packet;
	struct lease *lease;
	struct client_state *client_state;
	struct option_state *in_options;
	struct option_state *cfg_options;
	struct binding_scope **scope;
	struct option_cache *oc;
	const char *file;
	int line;
{
	struct data_string ds;
	int result;

	/* So that we can be called with option_lookup as an argument. */
	if (!oc || !in_options)
		return 0;
	
	memset (&ds, 0, sizeof ds);
	if (!evaluate_option_cache (&ds, packet,
				    lease, client_state, in_options,
				    cfg_options, scope, oc, file, line))
		return 0;

	if (ds.len) {
		result = ds.data [0];
		if (result == 2) {
			result = 0;
			*ignorep = 1;
		} else
			*ignorep = 0;
	} else
		result = 0;
	data_string_forget (&ds, MDL);
	return result;
}
		

/* Evaluate a boolean expression and return the result of the evaluation,
   or FALSE if it failed. */

int evaluate_boolean_expression_result (ignorep, packet, lease, client_state,
					in_options, cfg_options, scope, expr)
	int *ignorep;
	struct packet *packet;
	struct lease *lease;
	struct client_state *client_state;
	struct option_state *in_options;
	struct option_state *cfg_options;
	struct binding_scope **scope;
	struct expression *expr;
{
	int result;

	/* So that we can be called with option_lookup as an argument. */
	if (!expr)
		return 0;
	
	if (!evaluate_boolean_expression (&result, packet, lease, client_state,
					  in_options, cfg_options,
					  scope, expr))
		return 0;

	if (result == 2) {
		*ignorep = 1;
		result = 0;
	} else
		*ignorep = 0;
	return result;
}
		

/* Dereference an expression node, and if the reference count goes to zero,
   dereference any data it refers to, and then free it. */
void expression_dereference (eptr, file, line)
	struct expression **eptr;
	const char *file;
	int line;
{
	struct expression *expr = *eptr;

	/* Zero the pointer. */
	*eptr = (struct expression *)0;

	/* Decrement the reference count.   If it's nonzero, we're
	   done. */
	--(expr -> refcnt);
	rc_register (file, line, eptr, expr, expr -> refcnt, 1, RC_MISC);
	if (expr -> refcnt > 0)
		return;
	if (expr -> refcnt < 0) {
		log_error ("%s(%d): negative refcnt!", file, line);
#if defined (DEBUG_RC_HISTORY)
		dump_rc_history (expr);
#endif
#if defined (POINTER_DEBUG)
		abort ();
#else
		return;
#endif
	}

	/* Dereference subexpressions. */
	switch (expr -> op) {
		/* All the binary operators can be handled the same way. */
	      case expr_equal:
	      case expr_not_equal:
	      case expr_concat:
	      case expr_and:
	      case expr_or:
	      case expr_add:
	      case expr_subtract:
	      case expr_multiply:
	      case expr_divide:
	      case expr_remainder:
	      case expr_binary_and:
	      case expr_binary_or:
	      case expr_binary_xor:
	      case expr_client_state:
		if (expr -> data.equal [0])
			expression_dereference (&expr -> data.equal [0],
						file, line);
		if (expr -> data.equal [1])
			expression_dereference (&expr -> data.equal [1],
						file, line);
		break;

	      case expr_substring:
		if (expr -> data.substring.expr)
			expression_dereference (&expr -> data.substring.expr,
						file, line);
		if (expr -> data.substring.offset)
			expression_dereference (&expr -> data.substring.offset,
						file, line);
		if (expr -> data.substring.len)
			expression_dereference (&expr -> data.substring.len,
						file, line);
		break;

	      case expr_suffix:
		if (expr -> data.suffix.expr)
			expression_dereference (&expr -> data.suffix.expr,
						file, line);
		if (expr -> data.suffix.len)
			expression_dereference (&expr -> data.suffix.len,
						file, line);
		break;

	      case expr_not:
		if (expr -> data.not)
			expression_dereference (&expr -> data.not, file, line);
		break;

	      case expr_packet:
		if (expr -> data.packet.offset)
			expression_dereference (&expr -> data.packet.offset,
						file, line);
		if (expr -> data.packet.len)
			expression_dereference (&expr -> data.packet.len,
						file, line);
		break;

	      case expr_extract_int8:
	      case expr_extract_int16:
	      case expr_extract_int32:
		if (expr -> data.extract_int)
			expression_dereference (&expr -> data.extract_int,
						file, line);
		break;

	      case expr_encode_int8:
	      case expr_encode_int16:
	      case expr_encode_int32:
		if (expr -> data.encode_int)
			expression_dereference (&expr -> data.encode_int,
						file, line);
		break;

	      case expr_encapsulate:
	      case expr_const_data:
		data_string_forget (&expr -> data.const_data, file, line);
		break;

	      case expr_host_lookup:
		if (expr -> data.host_lookup)
			dns_host_entry_dereference (&expr -> data.host_lookup,
						    file, line);
		break;

	      case expr_binary_to_ascii:
		if (expr -> data.b2a.base)
			expression_dereference (&expr -> data.b2a.base,
						file, line);
		if (expr -> data.b2a.width)
			expression_dereference (&expr -> data.b2a.width,
						file, line);
		if (expr -> data.b2a.seperator)
			expression_dereference (&expr -> data.b2a.seperator,
						file, line);
		if (expr -> data.b2a.buffer)
			expression_dereference (&expr -> data.b2a.buffer,
						file, line);
		break;

	      case expr_pick_first_value:
		if (expr -> data.pick_first_value.car)
		    expression_dereference (&expr -> data.pick_first_value.car,
					    file, line);
		if (expr -> data.pick_first_value.cdr)
		    expression_dereference (&expr -> data.pick_first_value.cdr,
					    file, line);
		break;

	      case expr_reverse:
		if (expr -> data.reverse.width)
			expression_dereference (&expr -> data.reverse.width,
						file, line);
		if (expr -> data.reverse.buffer)
			expression_dereference
				(&expr -> data.reverse.buffer, file, line);
		break;

	      case expr_dns_transaction:
		if (expr -> data.dns_transaction.car)
		    expression_dereference (&expr -> data.dns_transaction.car,
					    file, line);
		if (expr -> data.dns_transaction.cdr)
		    expression_dereference (&expr -> data.dns_transaction.cdr,
					    file, line);
		break;

	      case expr_ns_add:
		if (expr -> data.ns_add.rrname)
		    expression_dereference (&expr -> data.ns_add.rrname,
					    file, line);
		if (expr -> data.ns_add.rrdata)
		    expression_dereference (&expr -> data.ns_add.rrdata,
					    file, line);
		if (expr -> data.ns_add.ttl)
		    expression_dereference (&expr -> data.ns_add.ttl,
					    file, line);
		break;

	      case expr_ns_delete:
	      case expr_ns_exists:
	      case expr_ns_not_exists:
		if (expr -> data.ns_delete.rrname)
		    expression_dereference (&expr -> data.ns_delete.rrname,
					    file, line);
		if (expr -> data.ns_delete.rrdata)
		    expression_dereference (&expr -> data.ns_delete.rrdata,
					    file, line);
		break;

	      case expr_variable_reference:
	      case expr_variable_exists:
		if (expr -> data.variable)
			dfree (expr -> data.variable, file, line);
		break;

	      case expr_funcall:
		if (expr -> data.funcall.name)
			dfree (expr -> data.funcall.name, file, line);
		if (expr -> data.funcall.arglist)
			expression_dereference (&expr -> data.funcall.arglist,
						file, line);
		break;

	      case expr_arg:
		if (expr -> data.arg.val)
			expression_dereference (&expr -> data.arg.val,
						file, line);
		if (expr -> data.arg.next)
			expression_dereference (&expr -> data.arg.next,
						file, line);
		break;

	      case expr_function:
		fundef_dereference (&expr -> data.func, file, line);
		break;

		/* No subexpressions. */
	      case expr_leased_address:
	      case expr_lease_time:
	      case expr_filename:
	      case expr_sname:
	      case expr_const_int:
	      case expr_check:
	      case expr_option:
	      case expr_hardware:
	      case expr_exists:
	      case expr_known:
	      case expr_null:
		break;

	      default:
		break;
	}
	free_expression (expr, MDL);
}

int is_dns_expression (expr)
	struct expression *expr;
{
      return (expr -> op == expr_ns_add ||
	      expr -> op == expr_ns_delete ||
	      expr -> op == expr_ns_exists ||
	      expr -> op == expr_ns_not_exists);
}

int is_boolean_expression (expr)
	struct expression *expr;
{
	return (expr -> op == expr_check ||
		expr -> op == expr_exists ||
		expr -> op == expr_variable_exists ||
		expr -> op == expr_equal ||
		expr -> op == expr_not_equal ||
		expr -> op == expr_and ||
		expr -> op == expr_or ||
		expr -> op == expr_not ||
		expr -> op == expr_known ||
		expr -> op == expr_static);
}

int is_data_expression (expr)
	struct expression *expr;
{
	return (expr -> op == expr_substring ||
		expr -> op == expr_suffix ||
		expr -> op == expr_option ||
		expr -> op == expr_hardware ||
		expr -> op == expr_const_data ||
		expr -> op == expr_packet ||
		expr -> op == expr_concat ||
		expr -> op == expr_encapsulate ||
		expr -> op == expr_encode_int8 ||
		expr -> op == expr_encode_int16 ||
		expr -> op == expr_encode_int32 ||
		expr -> op == expr_host_lookup ||
		expr -> op == expr_binary_to_ascii ||
		expr -> op == expr_filename ||
		expr -> op == expr_sname ||
		expr -> op == expr_reverse ||
		expr -> op == expr_pick_first_value ||
		expr -> op == expr_host_decl_name ||
		expr -> op == expr_leased_address ||
		expr -> op == expr_config_option ||
		expr -> op == expr_null);
}

int is_numeric_expression (expr)
	struct expression *expr;
{
	return (expr -> op == expr_extract_int8 ||
		expr -> op == expr_extract_int16 ||
		expr -> op == expr_extract_int32 ||
		expr -> op == expr_const_int ||
		expr -> op == expr_lease_time ||
		expr -> op == expr_dns_transaction ||
		expr -> op == expr_add ||
		expr -> op == expr_subtract ||
		expr -> op == expr_multiply ||
		expr -> op == expr_divide ||
		expr -> op == expr_remainder ||
		expr -> op == expr_binary_and ||
		expr -> op == expr_binary_or ||
		expr -> op == expr_binary_xor ||
		expr -> op == expr_client_state);
}

int is_compound_expression (expr)
	struct expression *expr;
{
	return (expr -> op == expr_ns_add ||
		expr -> op == expr_ns_delete ||
		expr -> op == expr_ns_exists ||
		expr -> op == expr_ns_not_exists ||
		expr -> op == expr_substring ||
		expr -> op == expr_suffix ||
		expr -> op == expr_option ||
		expr -> op == expr_concat ||
		expr -> op == expr_encode_int8 ||
		expr -> op == expr_encode_int16 ||
		expr -> op == expr_encode_int32 ||
		expr -> op == expr_binary_to_ascii ||
		expr -> op == expr_reverse ||
		expr -> op == expr_pick_first_value ||
		expr -> op == expr_config_option ||
		expr -> op == expr_extract_int8 ||
		expr -> op == expr_extract_int16 ||
		expr -> op == expr_extract_int32 ||
		expr -> op == expr_dns_transaction);
}

static int op_val PROTO ((enum expr_op));

static int op_val (op)
	enum expr_op op;
{
	switch (op) {
	      case expr_none:
	      case expr_match:
	      case expr_static:
	      case expr_check:
	      case expr_substring:
	      case expr_suffix:
	      case expr_concat:
	      case expr_encapsulate:
	      case expr_host_lookup:
	      case expr_not:
	      case expr_option:
	      case expr_hardware:
	      case expr_packet:
	      case expr_const_data:
	      case expr_extract_int8:
	      case expr_extract_int16:
	      case expr_extract_int32:
	      case expr_encode_int8:
	      case expr_encode_int16:
	      case expr_encode_int32:
	      case expr_const_int:
	      case expr_exists:
	      case expr_variable_exists:
	      case expr_known:
	      case expr_binary_to_ascii:
	      case expr_reverse:
	      case expr_filename:
	      case expr_sname:
	      case expr_pick_first_value:
	      case expr_host_decl_name:
	      case expr_config_option:
	      case expr_leased_address:
	      case expr_lease_time:
	      case expr_dns_transaction:
	      case expr_null:
	      case expr_variable_reference:
	      case expr_ns_add:
	      case expr_ns_delete:
	      case expr_ns_exists:
	      case expr_ns_not_exists:
	      case expr_arg:
	      case expr_funcall:
	      case expr_function:
		/* XXXDPN: Need to assign sane precedences to these. */
	      case expr_binary_and:
	      case expr_binary_or:
	      case expr_binary_xor:
	      case expr_client_state:
		return 100;

	      case expr_equal:
	      case expr_not_equal:
		return 3;

	      case expr_and:
	      case expr_multiply:
	      case expr_divide:
	      case expr_remainder:
		return 1;

	      case expr_or:
	      case expr_add:
	      case expr_subtract:
		return 2;
	}
	return 100;
}

int op_precedence (op1, op2)
	enum expr_op op1, op2;
{
	int ov1, ov2;

	return op_val (op1) - op_val (op2);
}

enum expression_context expression_context (struct expression *expr)
{
	if (is_data_expression (expr))
		return context_data;
	if (is_numeric_expression (expr))
		return context_numeric;
	if (is_boolean_expression (expr))
		return context_boolean;
	if (is_dns_expression (expr))
		return context_dns;
	return context_any;
}

enum expression_context op_context (op)
	enum expr_op op;
{
	switch (op) {
/* XXX Why aren't these specific? */
	      case expr_none:
	      case expr_match:
	      case expr_static:
	      case expr_check:
	      case expr_substring:
	      case expr_suffix:
	      case expr_concat:
	      case expr_encapsulate:
	      case expr_host_lookup:
	      case expr_not:
	      case expr_option:
	      case expr_hardware:
	      case expr_packet:
	      case expr_const_data:
	      case expr_extract_int8:
	      case expr_extract_int16:
	      case expr_extract_int32:
	      case expr_encode_int8:
	      case expr_encode_int16:
	      case expr_encode_int32:
	      case expr_const_int:
	      case expr_exists:
	      case expr_variable_exists:
	      case expr_known:
	      case expr_binary_to_ascii:
	      case expr_reverse:
	      case expr_filename:
	      case expr_sname:
	      case expr_pick_first_value:
	      case expr_host_decl_name:
	      case expr_config_option:
	      case expr_leased_address:
	      case expr_lease_time:
	      case expr_null:
	      case expr_variable_reference:
	      case expr_ns_add:
	      case expr_ns_delete:
	      case expr_ns_exists:
	      case expr_ns_not_exists:
	      case expr_dns_transaction:
	      case expr_arg:
	      case expr_funcall:
	      case expr_function:
		return context_any;

	      case expr_equal:
	      case expr_not_equal:
		return context_data;

	      case expr_and:
		return context_boolean;

	      case expr_or:
		return context_boolean;

	      case expr_add:
	      case expr_subtract:
	      case expr_multiply:
	      case expr_divide:
	      case expr_remainder:
	      case expr_binary_and:
	      case expr_binary_or:
	      case expr_binary_xor:
	      case expr_client_state:
		return context_numeric;
	}
	return context_any;
}

int write_expression (file, expr, col, indent, firstp)
	FILE *file;
	struct expression *expr;
	int col;
	int indent;
	int firstp;
{
	struct expression *e;
	const char *s;
	char obuf [65];
	int scol;
	int width;

	/* If this promises to be a fat expression, start a new line. */
	if (!firstp && is_compound_expression (expr)) {
		indent_spaces (file, indent);
		col = indent;
	}

	switch (expr -> op) {
	      case expr_none:
		col = token_print_indent (file, col, indent, "", "", "null");
		break;
		
	      case expr_check:
		col = token_print_indent (file, col, indent, "", "", "check");
		col = token_print_indent_concat (file, col, indent,
						 " ", "", "\"",
						 expr -> data.check -> name,
						 "\"", (char *)0);
		break;

	      case expr_not_equal:
		s = "!=";
		goto binary;

	      case expr_equal:
		s = "=";
	      binary:
		col = write_expression (file, expr -> data.equal [0],
					col, indent, 1);
		col = token_print_indent (file, col, indent, " ", " ", s);
		col = write_expression (file, expr -> data.equal [1],
					col, indent + 2, 0);
		break;

	      case expr_substring:
		col = token_print_indent (file, col, indent, "", "",
					  "substring");
		col = token_print_indent (file, col, indent, " ", "", "(");
		scol = col;
		col = write_expression (file, expr -> data.substring.expr,
					col, scol, 1);
		col = token_print_indent (file, col, indent, "", " ", ",");
		col = write_expression (file, expr -> data.substring.offset,
					col, indent, 0);
		col = token_print_indent (file, col, scol, "", " ", ",");
		col = write_expression (file, expr -> data.substring.len,
					col, scol, 0);
		col = token_print_indent (file, col, indent, "", "", ")");
		break;

	      case expr_suffix:
		col = token_print_indent (file, col, indent, "", "", "suffix");
		col = token_print_indent (file, col, indent, " ", "", "(");
		scol = col;
		col = write_expression (file, expr -> data.suffix.expr,
					col, scol, 1);
		col = token_print_indent (file, col, scol, "", " ", ",");
		col = write_expression (file, expr -> data.suffix.len,
					col, scol, 0);
		col = token_print_indent (file, col, indent, "", "", ")");
		break;

	      case expr_concat:
		e = expr;
		col = token_print_indent (file, col, indent, "", "",
					  "concat");
		col = token_print_indent (file, col, indent, " ", "", "(");
		scol = col;
		firstp = 1;
	      concat_again:
		col = write_expression (file, e -> data.concat [0],
					col, scol, firstp);
		firstp = 0;
		if (!e -> data.concat [1])
			goto no_concat_cdr;
		col = token_print_indent (file, col, scol, "", " ", ",");
		if (e -> data.concat [1] -> op == expr_concat) {
			e = e -> data.concat [1];
			goto concat_again;
		}
		col = write_expression (file, e -> data.concat [1],
					col, scol, 0);
	      no_concat_cdr:
		col = token_print_indent (file, col, indent, "", "", ")");
		break;

	      case expr_host_lookup:
		col = token_print_indent (file, col, indent, "", "",
					  "gethostbyname");
		col = token_print_indent (file, col, indent, " ", "", "(");
		col = token_print_indent_concat
			(file, col, indent, "", "",
			 "\"", expr -> data.host_lookup -> hostname, "\"",
			 (char *)0);
		col = token_print_indent (file, col, indent, "", "", ")");
		break;

	      case expr_add:
		s = "+";
		goto binary;

	      case expr_subtract:
		s = "-";
		goto binary;

	      case expr_multiply:
		s = "*";
		goto binary;

	      case expr_divide:
		s = "/";
		goto binary;

	      case expr_remainder:
		s = "%";
		goto binary;

	      case expr_binary_and:
		s = "&";
		goto binary;

	      case expr_binary_or:
		s = "|";
		goto binary;

	      case expr_binary_xor:
		s = "^";
		goto binary;

	      case expr_and:
		s = "and";
		goto binary;

	      case expr_or:
		s = "or";
		goto binary;

	      case expr_not:
		col = token_print_indent (file, col, indent, "", " ", "not");
		col = write_expression (file,
					expr -> data.not, col, indent + 2, 1);
		break;

	      case expr_option:
		s = "option";

	      print_option_name:
		col = token_print_indent (file, col, indent, "", "", s);

		if (expr -> data.option -> universe != &dhcp_universe) {
			col = token_print_indent (file, col, indent,
						  " ", "",
						  (expr -> data.option -> 
						   universe -> name));
			col = token_print_indent (file, col, indent, "", "",
						  ".");
			col = token_print_indent (file, col, indent, "", "",
						  expr -> data.option -> name);
		} else {
			col = token_print_indent (file, col, indent, " ", "",
						  expr -> data.option -> name);
		}
		break;

	      case expr_hardware:	
		col = token_print_indent (file, col, indent, "", "",
					  "hardware");
		break;

	      case expr_packet:
		col = token_print_indent (file, col, indent, "", "",
					  "packet");
		col = token_print_indent (file, col, indent, " ", "", "(");
		scol = col;
		col = write_expression (file, expr -> data.packet.offset,
					col, indent, 1);
		col = token_print_indent (file, col, scol, "", " ", ",");
		col = write_expression (file, expr -> data.packet.len,
					col, scol, 0);
		col = token_print_indent (file, col, indent, "", "", ")");
		break;

	      case expr_const_data:
		col = token_indent_data_string (file, col, indent, "", "",
						&expr -> data.const_data);
		break;

	      case expr_extract_int8:
		width = 8;
	      extract_int:
		col = token_print_indent (file, col, indent, "", "",
					  "extract-int");
		col = token_print_indent (file, col, indent, " ", "", "(");
		scol = col;
		col = write_expression (file, expr -> data.extract_int,
					col, indent, 1);
		col = token_print_indent (file, col, scol, "", " ", ",");
		sprintf (obuf, "%d", width);
		col = token_print_indent (file, col, scol, " ", "", obuf);
		col = token_print_indent (file, col, indent, "", "", ")");
		break;

	      case expr_extract_int16:
		width = 16;
		goto extract_int;

	      case expr_extract_int32:
		width = 32;
		goto extract_int;

	      case expr_encode_int8:
		width = 8;
	      encode_int:
		col = token_print_indent (file, col, indent, "", "",
					  "encode-int");
		col = token_print_indent (file, col, indent, " ", "", "(");
		scol = col;
		col = write_expression (file, expr -> data.extract_int,
					col, indent, 1);
		col = token_print_indent (file, col, scol, "", " ", ",");
		sprintf (obuf, "%d", width);
		col = token_print_indent (file, col, scol, " ", "", obuf);
		col = token_print_indent (file, col, indent, "", "",
					  ")");
		break;

	      case expr_encode_int16:
		width = 16;
		goto encode_int;

	      case expr_encode_int32:
		width = 32;
		goto encode_int;

	      case expr_const_int:
		sprintf (obuf, "%lu", expr -> data.const_int);
		col = token_print_indent (file, col, indent, "", "", obuf);
		break;

	      case expr_exists:
		s = "exists";
		goto print_option_name;

	      case expr_encapsulate:
		col = token_print_indent (file, col, indent, "", "",
					  "encapsulate");
		col = token_indent_data_string (file, col, indent, " ", "",
						&expr -> data.encapsulate);
		break;

	      case expr_known:
		col = token_print_indent (file, col, indent, "", "", "known");
		break;

	      case expr_reverse:
		col = token_print_indent (file, col, indent, "", "",
					  "reverse");
		col = token_print_indent (file, col, indent, " ", "", "(");
		scol = col;
		col = write_expression (file, expr -> data.reverse.width,
					col, scol, 1);
		col = token_print_indent (file, col, scol, "", " ", ",");
		col = write_expression (file, expr -> data.reverse.buffer,
					col, scol, 0);
		col = token_print_indent (file, col, indent, "", "",
					  ")");
		break;

	      case expr_leased_address:
		col = token_print_indent (file, col, indent, "", "",
					  "leased-address");
		break;

	      case expr_client_state:
		col = token_print_indent (file, col, indent, "", "",
					  "client-state");
		break;

	      case expr_binary_to_ascii:
		col = token_print_indent (file, col, indent, "", "",
					  "binary-to-ascii");
		col = token_print_indent (file, col, indent, " ", "",
					  "(");
		scol = col;
		col = write_expression (file, expr -> data.b2a.base,
					col, scol, 1);
		col = token_print_indent (file, col, scol, "", " ",
					  ",");
		col = write_expression (file, expr -> data.b2a.width,
					col, scol, 0);
		col = token_print_indent (file, col, scol, "", " ",
					  ",");
		col = write_expression (file, expr -> data.b2a.seperator,
					col, scol, 0);
		col = token_print_indent (file, col, scol, "", " ",
					  ",");
		col = write_expression (file, expr -> data.b2a.buffer,
					col, scol, 0);
		col = token_print_indent (file, col, indent, "", "",
					  ")");
		break;

	      case expr_config_option:
		s = "config-option";
		goto print_option_name;

	      case expr_host_decl_name:
		col = token_print_indent (file, col, indent, "", "",
					  "host-decl-name");
		break;

	      case expr_pick_first_value:
		e = expr;
		col = token_print_indent (file, col, indent, "", "",
					  "concat");
		col = token_print_indent (file, col, indent, " ", "",
					  "(");
		scol = col;
		firstp = 1;
	      pick_again:
		col = write_expression (file,
					e -> data.pick_first_value.car,
					col, scol, firstp);
		firstp = 0;
		/* We're being very lisp-like right now - instead of
                   representing this expression as (first middle . last) we're
                   representing it as (first middle last), which means that the
                   tail cdr is always nil.  Apologies to non-wisp-lizards - may
                   this obscure way of describing the problem motivate you to
                   learn more about the one true computing language. */
		if (!e -> data.pick_first_value.cdr)
			goto no_pick_cdr;
		col = token_print_indent (file, col, scol, "", " ",
					  ",");
		if (e -> data.pick_first_value.cdr -> op ==
		    expr_pick_first_value) {
			e = e -> data.pick_first_value.cdr;
			goto pick_again;
		}
		col = write_expression (file,
					e -> data.pick_first_value.cdr,
					col, scol, 0);
	      no_pick_cdr:
		col = token_print_indent (file, col, indent, "", "",
					  ")");
		break;

	      case expr_lease_time:
		col = token_print_indent (file, col, indent, "", "",
					  "lease-time");
		break;

	      case expr_dns_transaction:
		col = token_print_indent (file, col, indent, "", "",
					  "ns-update");
		col = token_print_indent (file, col, indent, " ", "",
					  "(");
		scol = 0;
		for (e = expr;
		     e && e -> op == expr_dns_transaction;
		     e = e -> data.dns_transaction.cdr) {
			if (!scol) {
				scol = col;
				firstp = 1;
			} else
				firstp = 0;
			col = write_expression (file,
						e -> data.dns_transaction.car,
						col, scol, firstp);
			if (e -> data.dns_transaction.cdr)
				col = token_print_indent (file, col, scol,
							  "", " ", ",");
		}
		if (e)
			col = write_expression (file, e, col, scol, 0);
		col = token_print_indent (file, col, indent, "", "", ")");
		break;

	      case expr_ns_add:
		col = token_print_indent (file, col, indent, "", "",
					  "update");
		col = token_print_indent (file, col, indent, " ", "",
					  "(");
		scol = col;
		sprintf (obuf, "%d", expr -> data.ns_add.rrclass);
		col = token_print_indent (file, col, scol, "", "", obuf);
		col = token_print_indent (file, col, scol, "", " ",
					  ",");
		sprintf (obuf, "%d", expr -> data.ns_add.rrtype);
		col = token_print_indent (file, col, scol, "", "", obuf);
		col = token_print_indent (file, col, scol, "", " ",
					  ",");
		col = write_expression (file, expr -> data.ns_add.rrname,
					col, scol, 0);
		col = token_print_indent (file, col, scol, "", " ",
					  ",");
		col = write_expression (file, expr -> data.ns_add.rrdata,
					col, scol, 0);
		col = token_print_indent (file, col, scol, "", " ",
					  ",");
		col = write_expression (file, expr -> data.ns_add.ttl,
					col, scol, 0);
		col = token_print_indent (file, col, indent, "", "",
					  ")");
		break;

	      case expr_ns_delete:
		col = token_print_indent (file, col, indent, "", "",
					  "delete");
		col = token_print_indent (file, col, indent, " ", "",
					  "(");
	      finish_ns_small:
		scol = col;
		sprintf (obuf, "%d", expr -> data.ns_add.rrclass);
		col = token_print_indent (file, col, scol, "", "", obuf);
		col = token_print_indent (file, col, scol, "", " ",
					  ",");
		sprintf (obuf, "%d", expr -> data.ns_add.rrtype);
		col = token_print_indent (file, col, scol, "", "", obuf);
		col = token_print_indent (file, col, scol, "", " ",
					  ",");
		col = write_expression (file, expr -> data.ns_add.rrname,
					col, scol, 0);
		col = token_print_indent (file, col, scol, "", " ",
					  ",");
		col = write_expression (file, expr -> data.ns_add.rrdata,
					col, scol, 0);
		col = token_print_indent (file, col, indent, "", "",
					  ")");
		break;

	      case expr_ns_exists:
		col = token_print_indent (file, col, indent, "", "",
					  "exists");
		col = token_print_indent (file, col, indent, " ", "",
					  "(");
		goto finish_ns_small;

	      case expr_ns_not_exists:
		col = token_print_indent (file, col, indent, "", "",
					  "not exists");
		col = token_print_indent (file, col, indent, " ", "",
					  "(");
		goto finish_ns_small;

	      case expr_static:
		col = token_print_indent (file, col, indent, "", "",
					  "static");
		break;

	      case expr_null:
		col = token_print_indent (file, col, indent, "", "", "null");
		break;

	      case expr_variable_reference:
		col = token_print_indent (file, indent, indent, "", "",
					  expr -> data.variable);
		break;

	      case expr_variable_exists:
		col = token_print_indent (file, indent, indent, "", "",
					  "defined");
		col = token_print_indent (file, col, indent, " ", "", "(");
		col = token_print_indent (file, col, indent, "", "",
					  expr -> data.variable);
		col = token_print_indent (file, col, indent, "", "", ")");
		break;

	      default:
		log_fatal ("invalid expression type in print_expression: %d",
			   expr -> op);
	}
	return col;
}

struct binding *find_binding (struct binding_scope *scope, const char *name)
{
	struct binding *bp;
	struct binding_scope *s;

	for (s = scope; s; s = s -> outer) {
		for (bp = s -> bindings; bp; bp = bp -> next) {
			if (!strcasecmp (name, bp -> name)) {
				return bp;
			}
		}
	}
	return (struct binding *)0;
}

int free_bindings (struct binding_scope *scope, const char *file, int line)
{
	struct binding *bp, *next;

	for (bp = scope -> bindings; bp; bp = next) {
		next = bp -> next;
		if (bp -> name)
			dfree (bp -> name, file, line);
		if (bp -> value)
			binding_value_dereference (&bp -> value, file, line);
		dfree (bp, file, line);
	}
	scope -> bindings = (struct binding *)0;
	return 1;
}

int binding_scope_dereference (ptr, file, line)
	struct binding_scope **ptr;
	const char *file;
	int line;
{
	int i;
	struct binding_scope *binding_scope;

	if (!ptr || !*ptr) {
		log_error ("%s(%d): null pointer", file, line);
#if defined (POINTER_DEBUG)
		abort ();
#else
		return 0;
#endif
	}

	binding_scope = *ptr;
	*ptr = (struct binding_scope *)0;
	--binding_scope -> refcnt;
	rc_register (file, line, ptr,
		     binding_scope, binding_scope -> refcnt, 1, RC_MISC);
	if (binding_scope -> refcnt > 0)
		return 1;

	if (binding_scope -> refcnt < 0) {
		log_error ("%s(%d): negative refcnt!", file, line);
#if defined (DEBUG_RC_HISTORY)
		dump_rc_history (binding_scope);
#endif
#if defined (POINTER_DEBUG)
		abort ();
#else
		return 0;
#endif
	}

	free_bindings (binding_scope, file, line);
	if (binding_scope -> outer)
		binding_scope_dereference (&binding_scope -> outer, MDL);
	dfree (binding_scope, file, line);
	return 1;
}

int fundef_dereference (ptr, file, line)
	struct fundef **ptr;
	const char *file;
	int line;
{
	struct fundef *bp = *ptr;
	struct string_list *sp, *next;

	if (!ptr) {
		log_error ("%s(%d): null pointer", file, line);
#if defined (POINTER_DEBUG)
		abort ();
#else
		return 0;
#endif
	}

	if (!bp) {
		log_error ("%s(%d): null pointer", file, line);
#if defined (POINTER_DEBUG)
		abort ();
#else
		return 0;
#endif
	}

	bp -> refcnt--;
	rc_register (file, line, ptr, bp, bp -> refcnt, 1, RC_MISC);
	if (bp -> refcnt < 0) {
		log_error ("%s(%d): negative refcnt!", file, line);
#if defined (DEBUG_RC_HISTORY)
		dump_rc_history (bp);
#endif
#if defined (POINTER_DEBUG)
		abort ();
#else
		return 0;
#endif
	}
	if (!bp -> refcnt) {
		for (sp = bp -> args; sp; sp = next) {
			next = sp -> next;
			dfree (sp, file, line);
		}
		if (bp -> statements)
			executable_statement_dereference (&bp -> statements,
							  file, line);
		dfree (bp, file, line);
	}
	*ptr = (struct fundef *)0;
	return 1;
}

#if defined (NOTYET)		/* Post 3.0 final. */
int data_subexpression_length (int *rv,
			       struct expression *expr)
{
	int crhs, clhs, llhs, lrhs;
	switch (expr -> op) {
	      case expr_substring:
		if (expr -> data.substring.len &&
		    expr -> data.substring.len -> op == expr_const_int) {
			(*rv =
			 (int)expr -> data.substring.len -> data.const_int);
			return 1;
		}
		return 0;

	      case expr_packet:
	      case expr_suffix:
		if (expr -> data.suffix.len &&
		    expr -> data.suffix.len -> op == expr_const_int) {
			(*rv =
			 (int)expr -> data.suffix.len -> data.const_int);
			return 1;
		}
		return 0;

	      case expr_concat:
		clhs = data_subexpression_length (&llhs,
						  expr -> data.concat [0]);
		crhs = data_subexpression_length (&lrhs,
						  expr -> data.concat [1]);
		if (crhs == 0 || clhs == 0)
			return 0;
		*rv = llhs + lrhs;
		return 1;
		break;

	      case expr_hardware:
		return 0;

	      case expr_const_data:
		*rv = expr -> data.const_data.len;
		return 2;

	      case expr_reverse:
		return data_subexpression_length (rv,
						  expr -> data.reverse.buffer);

	      case expr_leased_address:
	      case expr_lease_time:
		*rv = 4;
		return 2;

	      case expr_pick_first_value:
		clhs = data_subexpression_length (&llhs,
						  expr -> data.concat [0]);
		crhs = data_subexpression_length (&lrhs,
						  expr -> data.concat [1]);
		if (crhs == 0 || clhs == 0)
			return 0;
		if (llhs > lrhs)
			*rv = llhs;
		else
			*rv = lrhs;
		return 1;
			
	      case expr_binary_to_ascii:
	      case expr_config_option:
	      case expr_host_decl_name:
	      case expr_encapsulate:
	      case expr_filename:
	      case expr_sname:
	      case expr_host_lookup:
	      case expr_option:
	      case expr_none:
	      case expr_match:
	      case expr_check:
	      case expr_equal:
	      case expr_and:
	      case expr_or:
	      case expr_not:
	      case expr_extract_int8:
	      case expr_extract_int16:
	      case expr_extract_int32:
	      case expr_encode_int8:
	      case expr_encode_int16:
	      case expr_encode_int32:
	      case expr_const_int:
	      case expr_exists:
	      case expr_known:
	      case expr_dns_transaction:
	      case expr_static:
	      case expr_ns_add:
	      case expr_ns_delete:
	      case expr_ns_exists:
	      case expr_ns_not_exists:
	      case expr_not_equal:
	      case expr_null:
	      case expr_variable_exists:
	      case expr_variable_reference:
	      case expr_arg:
	      case expr_funcall:
	      case expr_function:
	      case expr_add:
	      case expr_subtract:
	      case expr_multiply:
	      case expr_divide:
	      case expr_remainder:
	      case expr_binary_and:
	      case expr_binary_or:
	      case expr_binary_xor:
	      case expr_client_state:
		return 0;
	}
	return 0;
}

int expr_valid_for_context (struct expression *expr,
			    enum expression_context context)
{
	/* We don't know at parse time what type of value a function may
	   return, so we can't flag an error on it. */
	if (expr -> op == expr_funcall ||
	    expr -> op == expr_variable_reference)
		return 1;

	switch (context) {
	      case context_any:
		return 1;

	      case context_boolean:
		if (is_boolean_expression (expr))
			return 1;
		return 0;

	      case context_data:
		if (is_data_expression (expr))
			return 1;
		return 0;

	      case context_numeric:
		if (is_numeric_expression (expr))
			return 1;
		return 0;

	      case context_dns:
		if (is_dns_expression (expr)) {
			return 1;
		}
		return 0;

	      case context_data_or_numeric:
		if (is_numeric_expression (expr) ||
		    is_data_expression (expr)) {
			return 1;
		}
		return 0;

	      case context_function:
		if (expr -> op == expr_function)
			return 1;
		return 0;
	}
	return 0;
}
#endif /* NOTYET */

struct binding *create_binding (struct binding_scope **scope, const char *name)
{
	struct binding *binding;

	if (!*scope) {
		if (!binding_scope_allocate (scope, MDL))
			return (struct binding *)0;
	}

	binding = find_binding (*scope, name);
	if (!binding) {
		binding = dmalloc (sizeof *binding, MDL);
		if (!binding)
			return (struct binding *)0;

		memset (binding, 0, sizeof *binding);
		binding -> name = dmalloc (strlen (name) + 1, MDL);
		if (!binding -> name) {
			dfree (binding, MDL);
			return (struct binding *)0;
		}
		strcpy (binding -> name, name);

		binding -> next = (*scope) -> bindings;
		(*scope) -> bindings = binding;
	}

	return binding;
}


int bind_ds_value (struct binding_scope **scope,
		   const char *name,
		   struct data_string *value)
{
	struct binding *binding;

	binding = create_binding (scope, name);
	if (!binding)
		return 0;

	if (binding -> value)
		binding_value_dereference (&binding -> value, MDL);

	if (!binding_value_allocate (&binding -> value, MDL))
		return 0;

	data_string_copy (&binding -> value -> value.data, value, MDL);
	binding -> value -> type = binding_data;

	return 1;
}


int find_bound_string (struct data_string *value,
		       struct binding_scope *scope,
		       const char *name)
{
	struct binding *binding;

	binding = find_binding (scope, name);
	if (!binding ||
	    !binding -> value ||
	    binding -> value -> type != binding_data)
		return 0;

	if (binding -> value -> value.data.terminated) {
		data_string_copy (value, &binding -> value -> value.data, MDL);
	} else {
		buffer_allocate (&value -> buffer,
				 binding -> value -> value.data.len,
				 MDL);
		if (!value -> buffer)
			return 0;

		memcpy (value -> buffer -> data,
			binding -> value -> value.data.data,
			binding -> value -> value.data.len);
		value -> data = value -> buffer -> data;
		value -> len = binding -> value -> value.data.len;
	}

	return 1;
}

int unset (struct binding_scope *scope, const char *name)
{
	struct binding *binding;

	binding = find_binding (scope, name);
	if (binding) {
		if (binding -> value)
			binding_value_dereference
				(&binding -> value, MDL);
		return 1;
	}
	return 0;
}

/* vim: set tabstop=8: */
