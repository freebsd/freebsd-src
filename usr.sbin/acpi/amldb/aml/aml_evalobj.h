/*-
 * Copyright (c) 1999 Takanori Watanabe
 * All rights reserved.
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
 *
 *	$Id: aml_evalobj.h,v 1.11 2000/08/16 18:14:53 iwasaki Exp $
 *	$FreeBSD$
 */

#ifndef _AML_EVALOBJ_H_
#define _AML_EVALOBJ_H_

#include <machine/stdarg.h>

union aml_object	*aml_eval_objref(struct aml_environ *,
					 union aml_object *);
union aml_object	*aml_eval_name(struct aml_environ *,
				       struct aml_name *);
int			 aml_eval_name_simple(struct aml_name *, va_list);
int			 aml_objtonum(struct aml_environ *,
				      union aml_object *);
struct aml_name		*aml_execute_method(struct aml_environ *);
union aml_object	*aml_invoke_method(struct aml_name *,
					   int, union aml_object *);
union aml_object	*aml_invoke_method_by_name(char *,
						   int, union aml_object *);

#endif /* !_AML_EVALOBJ_H_ */
