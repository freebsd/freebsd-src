/*
 * Copyright (C) 1999 WIDE Project.
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
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: src/usr.sbin/pim6sd/cfparse.h,v 1.1.2.1 2000/07/15 07:36:35 kris Exp $
 */
#if defined(YIPS_DEBUG)
#  define DP(str) YIPSDEBUG(DEBUG_CONF, cfdebug_print(str, yytext, yyleng))
#  define YYD_ECHO \
    { YIPSDEBUG(DEBUG_CONF, printf("<%d>", yy_start); ECHO ; printf("\n");); }
#  define YIPSDP(cmd) YIPSDEBUG(DEBUG_CONF, cmd)
#  define PLOG printf
#else
#  define DP(str)
#  define YYD_ECHO
#  define YIPSDP(cmd)
#  define PLOG(cmd)
#endif /* defined(YIPS_DEBUG) */

/* cfparse.y */
extern void cf_init __P((int, int));
#ifdef  notyet
extern int re_cfparse __P((void));
#endif
extern int cf_post_config __P((void));
extern int yyparse __P((void));

/* cftoken.l */
extern void yyerror __P((char *, ...));
extern void yywarn __P((char *, ...));
extern int cfparse __P((int, int));
