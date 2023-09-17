/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright 2021 Lutz Donnerhacke
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
 * CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <netgraph.h>

void
_ng_connect(char const *path1, char const *hook1,
	    char const *path2, char const *hook2,
	    char const *file, size_t line);
#define ng_connect(p1,h1,p2,h2)	\
   _ng_connect(p1,h1,p2,h2,__FILE__,__LINE__)

void
_ng_mkpeer(char const *path1, char const *hook1,
	   char const *type, char const *hook2,
	   char const *file, size_t line);
#define ng_mkpeer(p1,h1,t,h2)	\
   _ng_mkpeer(p1,h1,t,h2,__FILE__,__LINE__)

void
_ng_shutdown(char const *path,
	     char const *file, size_t line);
#define ng_shutdown(p)	\
   _ng_shutdown(p,__FILE__,__LINE__)

void
_ng_rmhook(char const *path, char const *hook,
	   char const *file, size_t line);
#define ng_rmhook(p,h)	\
   _ng_rmhook(p,h,__FILE__,__LINE__)

void
_ng_name(char const *path, char const *name,
	 char const *file, size_t line);
#define ng_name(p,n)	\
   _ng_name(p,n,__FILE__,__LINE__)


typedef void (*ng_data_handler_t)(void *, size_t, void *ctx);
void		ng_register_data(char const *hook, ng_data_handler_t proc);
void
_ng_send_data(char const *hook, void const *, size_t,
	      char const *file, size_t line);
#define ng_send_data(h,d,l)	\
   _ng_send_data(h,d,l,__FILE__,__LINE__)

typedef void (*ng_msg_handler_t)(char const *, struct ng_mesg *, void *);
void		ng_register_msg(ng_msg_handler_t proc);
int
_ng_send_msg(char const *path, char const *msg,
	     char const *file, size_t line);
#define ng_send_msg(p,m)	\
   _ng_send_msg(p,m,__FILE__,__LINE__)

int		ng_handle_event(unsigned int ms, void *ctx);
void		ng_handle_events(unsigned int ms, void *ctx);

typedef enum
{
	FAIL, PASS
} ng_error_t;
ng_error_t	ng_errors(ng_error_t);

void		_ng_init(char const *file, size_t line);
#define ng_init()	\
   _ng_init(__FILE__,__LINE__)

/* Helper function to count received data */

typedef int ng_counter_t[10];
#define ng_counter_clear(x)\
   bzero((x), sizeof(x))

void		get_data0(void *data, size_t len, void *ctx);
void		get_data1(void *data, size_t len, void *ctx);
void		get_data2(void *data, size_t len, void *ctx);
void		get_data3(void *data, size_t len, void *ctx);
void		get_data4(void *data, size_t len, void *ctx);
void		get_data5(void *data, size_t len, void *ctx);
void		get_data6(void *data, size_t len, void *ctx);
void		get_data7(void *data, size_t len, void *ctx);
void		get_data8(void *data, size_t len, void *ctx);
void		get_data9(void *data, size_t len, void *ctx);
