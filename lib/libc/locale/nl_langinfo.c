/*-
 * Copyright (c) 2001 Alexey Zelkin
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
 * $FreeBSD$
 */

#include "langinfo.h"
#include "../stdtime/timelocal.h"
#include "lnumeric.h"
#include "lmonetary.h"
#include "lmessages.h"

#define _REL(BASE) ((int)item-BASE)

char *
nl_langinfo(nl_item item) {

   char *ret;

   switch (item) {
	case CODESET:
		ret = "";		/* XXX: need to be implemented */
		break;
	case D_T_FMT:
		ret = (char *) __get_current_time_locale()->c_fmt;
		break;
	case D_FMT:
		ret = (char *) __get_current_time_locale()->x_fmt;
		break;
	case T_FMT:
		ret = (char *) __get_current_time_locale()->X_fmt;
		break;
	case T_FMT_AMPM:
		ret = "%r";
		break;
	case AM_STR:
		ret = (char *) __get_current_time_locale()->am;
		break;
	case PM_STR:
		ret = (char *) __get_current_time_locale()->pm;
		break;
	case DAY_1: case DAY_2: case DAY_3:
	case DAY_4: case DAY_5: case DAY_6: case DAY_7:
		ret = (char*) __get_current_time_locale()->weekday[_REL(DAY_1)];
		break;
	case ABDAY_1: case ABDAY_2: case ABDAY_3:
	case ABDAY_4: case ABDAY_5: case ABDAY_6: case ABDAY_7:
		ret = (char*) __get_current_time_locale()->wday[_REL(ABDAY_1)];
		break;
	case MON_1: case MON_2: case MON_3: case MON_4:
	case MON_5: case MON_6: case MON_7: case MON_8:
	case MON_9: case MON_10: case MON_11: case MON_12:
		ret = (char*) __get_current_time_locale()->month[_REL(MON_1)];
		break;
	case ABMON_1: case ABMON_2: case ABMON_3: case ABMON_4:
	case ABMON_5: case ABMON_6: case ABMON_7: case ABMON_8:
	case ABMON_9: case ABMON_10: case ABMON_11: case ABMON_12:
		ret = (char*) __get_current_time_locale()->mon[_REL(ABMON_1)];
		break;
	case ERA:
		/* XXX: ??? */
		ret = "";
		break;
	case ERA_D_FMT:
		/* XXX: ??? */
		ret = "";
		break;
	case ERA_D_T_FMT:
		/* XXX: ??? */
		ret = "";
		break;
	case ERA_T_FMT:
		/* XXX: ??? */
		ret = "";
		break;
	case ALT_DIGITS:
		/* XXX: ??? */
		ret = "";
		break;
	case RADIXCHAR:         /* deprecated */
		ret = (char*) __get_current_numeric_locale()->decimal_point;
		break;
	case THOUSEP:           /* deprecated */
		ret = (char*) __get_current_numeric_locale()->thousands_sep;
		break;
	case YESEXPR:
		ret = (char*) __get_current_messages_locale()->yesexpr;
		break;
	case NOEXPR:
		ret = (char*) __get_current_messages_locale()->noexpr;
		break;
	case YESSTR:            /* deprecated */
		ret = "";
		break;
	case NOSTR:             /* deprecated */
		ret = "";
		break;
	case CRNCYSTR:          /* deprecated */
		/* XXX: need to be implemented */
		/* __get_current_monetary_locale()->currency_symbol    */
		/* but requare special +-. prefixes according to SUSV2 */
		ret = "";
		break;
	default:
		ret = "";
   }
   return (ret);
}
