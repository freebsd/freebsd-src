/* execute.c

   Support for executable statements. */

/*
 * Copyright (c) 1998-2002 Internet Software Consortium.
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
"$Id: execute.c,v 1.44.2.9 2002/11/17 02:26:58 dhankins Exp $ Copyright (c) 1998-2002 The Internet Software Consortium.  All rights reserved.\n";
#endif /* not lint */

#include "dhcpd.h"
#include <omapip/omapip_p.h>

int execute_statements (result, packet, lease, client_state,
			in_options, out_options, scope, statements)
	struct binding_value **result;
	struct packet *packet;
	struct lease *lease;
	struct client_state *client_state;
	struct option_state *in_options;
	struct option_state *out_options;
	struct binding_scope **scope;
	struct executable_statement *statements;
{
	struct executable_statement *r, *e, *next;
	int rc;
	int status;
	unsigned long num;
	struct binding_scope *outer;
	struct binding *binding;
	struct data_string ds;
	struct binding_scope *ns;

	if (!statements)
		return 1;

	r = (struct executable_statement *)0;
	next = (struct executable_statement *)0;
	e = (struct executable_statement *)0;
	executable_statement_reference (&r, statements, MDL);
	while (r && !(result && *result)) {
		if (r -> next)
			executable_statement_reference (&next, r -> next, MDL);
		switch (r -> op) {
		      case statements_statement:
#if defined (DEBUG_EXPRESSIONS)
			log_debug ("exec: statements");
#endif
			status = execute_statements (result, packet, lease,
						     client_state, in_options,
						     out_options, scope,
						     r -> data.statements);
#if defined (DEBUG_EXPRESSIONS)
			log_debug ("exec: statements returns %d", status);
#endif
			if (!status)
				return 0;
			break;

		      case on_statement:
			if (lease) {
			    if (r -> data.on.evtypes & ON_EXPIRY) {
#if defined (DEBUG_EXPRESSIONS)
				    log_debug ("exec: on expiry");
#endif
				if (lease -> on_expiry)
					executable_statement_dereference
						(&lease -> on_expiry, MDL);
				if (r -> data.on.statements)
					executable_statement_reference
						(&lease -> on_expiry,
						 r -> data.on.statements, MDL);
			    }
			    if (r -> data.on.evtypes & ON_RELEASE) {
#if defined (DEBUG_EXPRESSIONS)
				    log_debug ("exec: on release");
#endif
				if (lease -> on_release)
					executable_statement_dereference
						(&lease -> on_release, MDL);
				if (r -> data.on.statements)
					executable_statement_reference
						(&lease -> on_release,
						 r -> data.on.statements, MDL);
			    }
			    if (r -> data.on.evtypes & ON_COMMIT) {
#if defined (DEBUG_EXPRESSIONS)
				    log_debug ("exec: on commit");
#endif
				if (lease -> on_commit)
					executable_statement_dereference
						(&lease -> on_commit, MDL);
				if (r -> data.on.statements)
					executable_statement_reference
						(&lease -> on_commit,
						 r -> data.on.statements, MDL);
			    }
			}
			break;

		      case switch_statement:
#if defined (DEBUG_EXPRESSIONS)
			log_debug ("exec: switch");
#endif
			status = (find_matching_case
				  (&e, packet, lease, client_state,
				   in_options, out_options, scope,
				   r -> data.s_switch.expr,
				   r -> data.s_switch.statements));
#if defined (DEBUG_EXPRESSIONS)
			log_debug ("exec: switch: case %lx", (unsigned long)e);
#endif
			if (status) {
				if (!(execute_statements
				      (result, packet, lease, client_state,
				       in_options, out_options, scope, e))) {
					executable_statement_dereference
						(&e, MDL);
					return 0;
				}
				executable_statement_dereference (&e, MDL);
			}
			break;

			/* These have no effect when executed. */
		      case case_statement:
		      case default_statement:
			break;

		      case if_statement:
			status = (evaluate_boolean_expression
				  (&rc, packet,
				   lease, client_state, in_options,
				   out_options, scope, r -> data.ie.expr));
			
#if defined (DEBUG_EXPRESSIONS)
			log_debug ("exec: if %s", (status
					      ? (rc ? "true" : "false")
					      : "NULL"));
#endif
			/* XXX Treat NULL as false */
			if (!status)
				rc = 0;
			if (!execute_statements
			    (result, packet, lease, client_state,
			     in_options, out_options, scope,
			     rc ? r -> data.ie.tc : r -> data.ie.fc))
				return 0;
			break;

		      case eval_statement:
			status = evaluate_expression
				((struct binding_value **)0,
				 packet, lease, client_state, in_options,
				 out_options, scope, r -> data.eval, MDL);
#if defined (DEBUG_EXPRESSIONS)
			log_debug ("exec: evaluate: %s",
				   (status ? "succeeded" : "failed"));
#endif
			break;

		      case return_statement:
			status = evaluate_expression
				(result, packet,
				 lease, client_state, in_options,
				 out_options, scope, r -> data.retval, MDL);
#if defined (DEBUG_EXPRESSIONS)
			log_debug ("exec: return: %s",
				   (status ? "succeeded" : "failed"));
#endif
			break;

		      case add_statement:
#if defined (DEBUG_EXPRESSIONS)
			log_debug ("exec: add %s", (r -> data.add -> name
					       ? r -> data.add -> name
					       : "<unnamed class>"));
#endif
			classify (packet, r -> data.add);
			break;

		      case break_statement:
#if defined (DEBUG_EXPRESSIONS)
			log_debug ("exec: break");
#endif
			return 1;

		      case supersede_option_statement:
		      case send_option_statement:
#if defined (DEBUG_EXPRESSIONS)
			log_debug ("exec: %s option %s.%s",
			      (r -> op == supersede_option_statement
			       ? "supersede" : "send"),
			      r -> data.option -> option -> universe -> name,
			      r -> data.option -> option -> name);
			goto option_statement;
#endif
		      case default_option_statement:
#if defined (DEBUG_EXPRESSIONS)
			log_debug ("exec: default option %s.%s",
			      r -> data.option -> option -> universe -> name,
			      r -> data.option -> option -> name);
			goto option_statement;
#endif
		      case append_option_statement:
#if defined (DEBUG_EXPRESSIONS)
			log_debug ("exec: append option %s.%s",
			      r -> data.option -> option -> universe -> name,
			      r -> data.option -> option -> name);
			goto option_statement;
#endif
		      case prepend_option_statement:
#if defined (DEBUG_EXPRESSIONS)
			log_debug ("exec: prepend option %s.%s",
			      r -> data.option -> option -> universe -> name,
			      r -> data.option -> option -> name);
		      option_statement:
#endif
			set_option (r -> data.option -> option -> universe,
				    out_options, r -> data.option, r -> op);
			break;

		      case set_statement:
		      case define_statement:
			if (!scope) {
				log_error ("set %s: no scope",
					   r -> data.set.name);
				status = 0;
				break;
			}
			if (!*scope) {
			    if (!binding_scope_allocate (scope, MDL)) {
				log_error ("set %s: can't allocate scope",
					   r -> data.set.name);
				status = 0;
				break;
			    }
			}
			binding = find_binding (*scope, r -> data.set.name);
#if defined (DEBUG_EXPRESSIONS)
			log_debug ("exec: set %s", r -> data.set.name);
#endif
			if (!binding) {
				binding = dmalloc (sizeof *binding, MDL);
				if (binding) {
				    memset (binding, 0, sizeof *binding);
				    binding -> name =
					    dmalloc (strlen
						     (r -> data.set.name) + 1,
						     MDL);
				    if (binding -> name) {
					strcpy (binding -> name,
						r -> data.set.name);
					binding -> next = (*scope) -> bindings;
					(*scope) -> bindings = binding;
				    } else {
				       badalloc:
					dfree (binding, MDL);
					binding = (struct binding *)0;
				    }
				}
			}
			if (binding) {
				if (binding -> value)
					binding_value_dereference
						(&binding -> value, MDL);
				if (r -> op == set_statement) {
					status = (evaluate_expression
						  (&binding -> value, packet,
						   lease, client_state,
						   in_options, out_options,
						   scope, r -> data.set.expr,
						   MDL));
				} else {
				    if (!(binding_value_allocate
					  (&binding -> value, MDL))) {
					    dfree (binding, MDL);
					    binding = (struct binding *)0;
				    }
				    if (binding -> value) {
				        binding -> value -> type =
						binding_function;
					(fundef_reference
					 (&binding -> value -> value.fundef,
					  r -> data.set.expr -> data.func,
					  MDL));
				    }
				}
			}
#if defined (DEBUG_EXPRESSIONS)
			log_debug ("exec: set %s%s", r -> data.set.name,
				   (binding && status ? "" : " (failed)"));
#endif
			break;

		      case unset_statement:
			if (!scope || !*scope) {
				status = 0;
				break;
			}
			binding = find_binding (*scope, r -> data.unset);
			if (binding) {
				if (binding -> value)
					binding_value_dereference
						(&binding -> value, MDL);
				status = 1;
			} else
				status = 0;
#if defined (DEBUG_EXPRESSIONS)
			log_debug ("exec: unset %s: %s", r -> data.unset,
				   (status ? "found" : "not found"));
#endif
			break;

		      case let_statement:
#if defined (DEBUG_EXPRESSIONS)
			log_debug ("exec: let %s", r -> data.let.name);
#endif
			ns = (struct binding_scope *)0;
			binding_scope_allocate (&ns, MDL);
			e = r;

		      next_let:
			if (ns) {
				binding = dmalloc (sizeof *binding, MDL);
				memset (binding, 0, sizeof *binding);
				if (!binding) {
				   blb:
				    binding_scope_dereference (&ns, MDL);
				} else {
				    binding -> name =
					    dmalloc (strlen
						     (e -> data.let.name + 1),
						     MDL);
				    if (binding -> name)
					strcpy (binding -> name,
						e -> data.let.name);
				    else {
					dfree (binding, MDL);
					binding = (struct binding *)0;
					goto blb;
				    }
				}
			}
			if (ns && binding) {
				status = (evaluate_expression
					  (&binding -> value, packet, lease,
					   client_state,
					   in_options, out_options,
					   scope, e -> data.set.expr, MDL));
				binding -> next = ns -> bindings;
				ns -> bindings = binding;
			}

#if defined (DEBUG_EXPRESSIONS)
			log_debug ("exec: let %s%s", e -> data.let.name,
				   (binding && status ? "" : "failed"));
#endif
			if (!e -> data.let.statements) {
			} else if (e -> data.let.statements -> op ==
				   let_statement) {
				e = e -> data.let.statements;
				goto next_let;
			} else if (ns) {
				if (scope && *scope)
				    	binding_scope_reference (&ns -> outer,
								 *scope, MDL);
				execute_statements
				      (result, packet, lease,
				       client_state,
				       in_options, out_options,
				       &ns, e -> data.let.statements);
			}
			if (ns)
				binding_scope_dereference (&ns, MDL);
			break;

		      case log_statement:
			memset (&ds, 0, sizeof ds);
			status = (evaluate_data_expression
				  (&ds, packet,
				   lease, client_state, in_options,
				   out_options, scope, r -> data.log.expr,
				   MDL));
			
#if defined (DEBUG_EXPRESSIONS)
			log_debug ("exec: log");
#endif

			if (status) {
				switch (r -> data.log.priority) {
				case log_priority_fatal:
					log_fatal ("%.*s", (int)ds.len,
						   ds.buffer -> data);
					break;
				case log_priority_error:
					log_error ("%.*s", (int)ds.len,
						   ds.buffer -> data);
					break;
				case log_priority_debug:
					log_debug ("%.*s", (int)ds.len,
						   ds.buffer -> data);
					break;
				case log_priority_info:
					log_info ("%.*s", (int)ds.len,
						  ds.buffer -> data);
					break;
				}
				data_string_forget (&ds, MDL);
			}

			break;

		      default:
			log_error ("bogus statement type %d", r -> op);
			break;
		}
		executable_statement_dereference (&r, MDL);
		if (next) {
			executable_statement_reference (&r, next, MDL);
			executable_statement_dereference (&next, MDL);
		}
	}

	return 1;
}

/* Execute all the statements in a particular scope, and all statements in
   scopes outer from that scope, but if a particular limiting scope is
   reached, do not execute statements in that scope or in scopes outer
   from it.   More specific scopes need to take precedence over less
   specific scopes, so we recursively traverse the scope list, executing
   the most outer scope first. */

void execute_statements_in_scope (result, packet,
				  lease, client_state, in_options, out_options,
				  scope, group, limiting_group)
	struct binding_value **result;
	struct packet *packet;
	struct lease *lease;
	struct client_state *client_state;
	struct option_state *in_options;
	struct option_state *out_options;
	struct binding_scope **scope;
	struct group *group;
	struct group *limiting_group;
{
	struct group *limit;

	/* If we've recursed as far as we can, return. */
	if (!group)
		return;

	/* As soon as we get to a scope that is outer than the limiting
	   scope, we are done.   This is so that if somebody does something
	   like this, it does the expected thing:

	        domain-name "fugue.com";
		shared-network FOO {
			host bar {
				domain-name "othello.fugue.com";
				fixed-address 10.20.30.40;
			}
			subnet 10.20.30.0 netmask 255.255.255.0 {
				domain-name "manhattan.fugue.com";
			}
		}

	   The problem with the above arrangement is that the host's
	   group nesting will be host -> shared-network -> top-level,
	   and the limiting scope when we evaluate the host's scope
	   will be the subnet -> shared-network -> top-level, so we need
	   to know when we evaluate the host's scope to stop before we
	   evaluate the shared-networks scope, because it's outer than
	   the limiting scope, which means we've already evaluated it. */

	for (limit = limiting_group; limit; limit = limit -> next) {
		if (group == limit)
			return;
	}

	if (group -> next)
		execute_statements_in_scope (result, packet,
					     lease, client_state,
					     in_options, out_options, scope,
					     group -> next, limiting_group);
	execute_statements (result, packet, lease, client_state, in_options,
			    out_options, scope, group -> statements);
}

/* Dereference or free any subexpressions of a statement being freed. */

int executable_statement_dereference (ptr, file, line)
	struct executable_statement **ptr;
	const char *file;
	int line;
{
	struct executable_statement *bp;

	if (!ptr || !*ptr) {
		log_error ("%s(%d): null pointer", file, line);
#if defined (POINTER_DEBUG)
		abort ();
#else
		return 0;
#endif
	}

	(*ptr) -> refcnt--;
	rc_register (file, line, ptr, *ptr, (*ptr) -> refcnt, 1, RC_MISC);
	if ((*ptr) -> refcnt > 0) {
		*ptr = (struct executable_statement *)0;
		return 1;
	}

	if ((*ptr) -> refcnt < 0) {
		log_error ("%s(%d): negative refcnt!", file, line);
#if defined (DEBUG_RC_HISTORY)
		dump_rc_history (*ptr);
#endif
#if defined (POINTER_DEBUG)
		abort ();
#else
		return 0;
#endif
	}

	if ((*ptr) -> next)
		executable_statement_dereference (&(*ptr) -> next, file, line);

	switch ((*ptr) -> op) {
	      case statements_statement:
		if ((*ptr) -> data.statements)
			executable_statement_dereference
				(&(*ptr) -> data.statements, file, line);
		break;

	      case on_statement:
		if ((*ptr) -> data.on.statements)
			executable_statement_dereference
				(&(*ptr) -> data.on.statements, file, line);
		break;

	      case switch_statement:
		if ((*ptr) -> data.s_switch.statements)
			executable_statement_dereference
				(&(*ptr) -> data.on.statements, file, line);
		if ((*ptr) -> data.s_switch.expr)
			expression_dereference (&(*ptr) -> data.s_switch.expr,
						file, line);
		break;

	      case case_statement:
		if ((*ptr) -> data.s_switch.expr)
			expression_dereference (&(*ptr) -> data.c_case,
						file, line);
		break;

	      case if_statement:
		if ((*ptr) -> data.ie.expr)
			expression_dereference (&(*ptr) -> data.ie.expr,
						file, line);
		if ((*ptr) -> data.ie.tc)
			executable_statement_dereference
				(&(*ptr) -> data.ie.tc, file, line);
		if ((*ptr) -> data.ie.fc)
			executable_statement_dereference
				(&(*ptr) -> data.ie.fc, file, line);
		break;

	      case eval_statement:
		if ((*ptr) -> data.eval)
			expression_dereference (&(*ptr) -> data.eval,
						file, line);
		break;

	      case return_statement:
		if ((*ptr) -> data.eval)
			expression_dereference (&(*ptr) -> data.eval,
						file, line);
		break;

	      case set_statement:
		if ((*ptr)->data.set.name)
			dfree ((*ptr)->data.set.name, file, line);
		if ((*ptr)->data.set.expr)
			expression_dereference (&(*ptr) -> data.set.expr,
						file, line);
		break;

	      case unset_statement:
		if ((*ptr)->data.unset)
			dfree ((*ptr)->data.unset, file, line);
		break;

	      case supersede_option_statement:
	      case send_option_statement:
	      case default_option_statement:
	      case append_option_statement:
	      case prepend_option_statement:
		if ((*ptr) -> data.option)
			option_cache_dereference (&(*ptr) -> data.option,
						  file, line);
		break;

	      default:
		/* Nothing to do. */
		break;
	}

	dfree ((*ptr), file, line);
	*ptr = (struct executable_statement *)0;
	return 1;
}

void write_statements (file, statements, indent)
	FILE *file;
	struct executable_statement *statements;
	int indent;
{
	struct executable_statement *r, *x;
	int result;
	int status;
	const char *s, *t, *dot;
	int col;

	if (!statements)
		return;

	for (r = statements; r; r = r -> next) {
		switch (r -> op) {
		      case statements_statement:
			write_statements (file, r -> data.statements, indent);
			break;

		      case on_statement:
			indent_spaces (file, indent);
			fprintf (file, "on ");
			s = "";
			if (r -> data.on.evtypes & ON_EXPIRY) {
				fprintf (file, "%sexpiry", s);
				s = " or ";
			}
			if (r -> data.on.evtypes & ON_COMMIT) {
				fprintf (file, "%scommit", s);
				s = "or";
			}
			if (r -> data.on.evtypes & ON_RELEASE) {
				fprintf (file, "%srelease", s);
				s = "or";
			}
			if (r -> data.on.statements) {
				fprintf (file, " {");
				write_statements (file,
						  r -> data.on.statements,
						  indent + 2);
				indent_spaces (file, indent);
				fprintf (file, "}");
			} else {
				fprintf (file, ";");
			}
			break;

		      case switch_statement:
			indent_spaces (file, indent);
			fprintf (file, "switch (");
			col = write_expression (file,
						r -> data.s_switch.expr,
						indent + 7, indent + 7, 1);
			col = token_print_indent (file, col, indent + 7,
						  "", "", ")");
			token_print_indent (file,
					    col, indent, " ", "", "{");
			write_statements (file, r -> data.s_switch.statements,
					  indent + 2);
			indent_spaces (file, indent);
			fprintf (file, "}");
			break;
			
		      case case_statement:
			indent_spaces (file, indent - 1);
			fprintf (file, "case ");
			col = write_expression (file,
						r -> data.s_switch.expr,
						indent + 5, indent + 5, 1);
			token_print_indent (file, col, indent + 5,
					    "", "", ":");
			break;
			
		      case default_statement:
			indent_spaces (file, indent - 1);
			fprintf (file, "default: ");
			break;

		      case if_statement:
			indent_spaces (file, indent);
			fprintf (file, "if ");
			x = r;
			col = write_expression (file,
						x -> data.ie.expr,
						indent + 3, indent + 3, 1);
		      else_if:
			token_print_indent (file, col, indent, " ", "", "{");
			write_statements (file, x -> data.ie.tc, indent + 2);
			if (x -> data.ie.fc &&
			    x -> data.ie.fc -> op == if_statement &&
			    !x -> data.ie.fc -> next) {
				indent_spaces (file, indent);
				fprintf (file, "} elsif ");
				x = x -> data.ie.fc;
				col = write_expression (file,
							x -> data.ie.expr,
							indent + 6,
							indent + 6, 1);
				goto else_if;
			}
			if (x -> data.ie.fc) {
				indent_spaces (file, indent);
				fprintf (file, "} else {");
				write_statements (file, x -> data.ie.fc,
						  indent + 2);
			}
			indent_spaces (file, indent);
			fprintf (file, "}");
			break;

		      case eval_statement:
			indent_spaces (file, indent);
			fprintf (file, "eval ");
			col = write_expression (file, r -> data.eval,
						indent + 5, indent + 5, 1);
			fprintf (file, ";");
			break;

		      case return_statement:
			indent_spaces (file, indent);
			fprintf (file, "return;");
			break;

		      case add_statement:
			indent_spaces (file, indent);
			fprintf (file, "add \"%s\"", r -> data.add -> name);
			break;

		      case break_statement:
			indent_spaces (file, indent);
			fprintf (file, "break;");
			break;

		      case supersede_option_statement:
		      case send_option_statement:
			s = "supersede";
			goto option_statement;

		      case default_option_statement:
			s = "default";
			goto option_statement;

		      case append_option_statement:
			s = "append";
			goto option_statement;

		      case prepend_option_statement:
			s = "prepend";
		      option_statement:
			/* Note: the reason we don't try to pretty print
			   the option here is that the format of the option
			   may change in dhcpd.conf, and then when this
			   statement was read back, it would cause a syntax
			   error. */
			if (r -> data.option -> option -> universe ==
			    &dhcp_universe) {
				t = "";
				dot = "";
			} else {
				t = (r -> data.option -> option ->
				     universe -> name);
				dot = ".";
			}
			indent_spaces (file, indent);
			fprintf (file, "%s %s%s%s = ", s, t, dot,
				 r -> data.option -> option -> name);
			col = (indent + strlen (s) + strlen (t) +
			       strlen (dot) + strlen (r -> data.option ->
						      option -> name) + 4);
			if (r -> data.option -> expression)
				write_expression
					(file,
					 r -> data.option -> expression,
					 col, indent + 8, 1);
			else
				token_indent_data_string
					(file, col, indent + 8, "", "",
					 &r -> data.option -> data);
					 
			fprintf (file, ";"); /* XXX */
			break;

		      case set_statement:
			indent_spaces (file, indent);
			fprintf (file, "set ");
			col = token_print_indent (file, indent + 4, indent + 4,
						  "", "", r -> data.set.name);
			col = token_print_indent (file, col, indent + 4,
						  " ", " ", "=");
			col = write_expression (file, r -> data.set.expr,
						indent + 3, indent + 3, 0);
			col = token_print_indent (file, col, indent + 4,
						  " ", "", ";");
			break;
			
		      case unset_statement:
			indent_spaces (file, indent);
			fprintf (file, "unset ");
			col = token_print_indent (file, indent + 6, indent + 6,
						  "", "", r -> data.set.name);
			col = token_print_indent (file, col, indent + 6,
						  " ", "", ";");
			break;

		      case log_statement:
			indent_spaces (file, indent);
			fprintf (file, "log ");
			col = token_print_indent (file, col, indent + 4,
						  "", "", "(");
			switch (r -> data.log.priority) {
			case log_priority_fatal:
				col = token_print_indent
					(file, col, indent + 4, "",
					 " ", "fatal,");
				break;
			case log_priority_error:
				col = token_print_indent
					(file, col, indent + 4, "",
					 " ", "error,");
				break;
			case log_priority_debug:
				col = token_print_indent
					(file, col, indent + 4, "",
					 " ", "debug,");
				break;
			case log_priority_info:
				col = token_print_indent
					(file, col, indent + 4, "",
					 " ", "info,");
				break;
			}
			col = write_expression (file, r -> data.log.expr,
						indent + 4, indent + 4, 0);
			col = token_print_indent (file, col, indent + 4,
						  "", "", ");");

			break;
			
		      default:
			log_fatal ("bogus statement type %d\n", r -> op);
		}
	}
}

/* Find a case statement in the sequence of executable statements that
   matches the expression, and if found, return the following statement.
   If no case statement matches, try to find a default statement and
   return that (the default statement can precede all the case statements).
   Otherwise, return the null statement. */

int find_matching_case (struct executable_statement **ep,
			struct packet *packet, struct lease *lease,
			struct client_state *client_state,
			struct option_state *in_options,
			struct option_state *out_options,
			struct binding_scope **scope,
			struct expression *expr,
			struct executable_statement *stmt)
{
	int status, sub;
	struct executable_statement *s;
	unsigned long foo;

	if (is_data_expression (expr)) {
		struct executable_statement *e;
		struct data_string cd, ds;
		memset (&ds, 0, sizeof ds);
		memset (&cd, 0, sizeof cd);

		status = (evaluate_data_expression (&ds, packet, lease,
						    client_state, in_options,
						    out_options, scope, expr,
						    MDL));
		if (status) {
		    for (s = stmt; s; s = s -> next) {
			if (s -> op == case_statement) {
				sub = (evaluate_data_expression
				       (&cd, packet, lease, client_state,
					in_options, out_options,
					scope, s -> data.c_case, MDL));
				if (sub && cd.len == ds.len &&
				    !memcmp (cd.data, ds.data, cd.len))
				{
					data_string_forget (&cd, MDL);
					data_string_forget (&ds, MDL);
					executable_statement_reference
						(ep, s -> next, MDL);
					return 1;
				}
				data_string_forget (&cd, MDL);
			}
		    }
		    data_string_forget (&ds, MDL);
		}
	} else {
		unsigned long n, c;
		status = evaluate_numeric_expression (&n, packet, lease,
						      client_state,
						      in_options, out_options,
						      scope, expr);

		if (status) {
		    for (s = stmt; s; s = s -> next) {
			if (s -> op == case_statement) {
				sub = (evaluate_numeric_expression
				       (&c, packet, lease, client_state,
					in_options, out_options,
					scope, s -> data.c_case));
				if (sub && n == c) {
					executable_statement_reference
						(ep, s -> next, MDL);
					return 1;
				}
			}
		    }
		}
	}

	/* If we didn't find a matching case statement, look for a default
	   statement and return the statement following it. */
	for (s = stmt; s; s = s -> next)
		if (s -> op == default_statement)
			break;
	if (s) {
		executable_statement_reference (ep, s -> next, MDL);
		return 1;
	}
	return 0;
}

int executable_statement_foreach (struct executable_statement *stmt,
				  int (*callback) (struct
						   executable_statement *,
						   void *, int),
				  void *vp, int condp)
{
	struct executable_statement *foo;
	int ok = 0;
	int result;

	for (foo = stmt; foo; foo = foo -> next) {
	    if ((*callback) (foo, vp, condp) != 0)
		ok = 1;
	    switch (foo -> op) {
	      case null_statement:
		break;
	      case if_statement:
		if (executable_statement_foreach (foo -> data.ie.tc,
						  callback, vp, 1))
			ok = 1;
		if (executable_statement_foreach (foo -> data.ie.fc,
						  callback, vp, 1))
			ok = 1;
		break;
	      case add_statement:
		break;
	      case eval_statement:
		break;
	      case break_statement:
		break;
	      case default_option_statement:
		break;
	      case supersede_option_statement:
		break;
	      case append_option_statement:
		break;
	      case prepend_option_statement:
		break;
	      case send_option_statement:
		break;
	      case statements_statement:
		if ((executable_statement_foreach
		     (foo -> data.statements, callback, vp, condp)))
			ok = 1;
		break;
	      case on_statement:
		if ((executable_statement_foreach
		     (foo -> data.on.statements, callback, vp, 1)))
			ok = 1;
		break;
	      case switch_statement:
		if ((executable_statement_foreach
		     (foo -> data.s_switch.statements, callback, vp, 1)))
			ok = 1;
		break;
	      case case_statement:
		break;
	      case default_statement:
		break;
	      case set_statement:
		break;
	      case unset_statement:
		break;
	      case let_statement:
		if ((executable_statement_foreach
		     (foo -> data.let.statements, callback, vp, 0)))
			ok = 1;
		break;
	      case define_statement:
		break;
	      case log_statement:
	      case return_statement:
		break;
	    }
	}
	return ok;
}
