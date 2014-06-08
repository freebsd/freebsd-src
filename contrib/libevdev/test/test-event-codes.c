/*
 * Copyright © 2013 David Herrmann <dh.herrmann@gmail.com>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that copyright
 * notice and this permission notice appear in supporting documentation, and
 * that the name of the copyright holders not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  The copyright holders make no representations
 * about the suitability of this software for any purpose.  It is provided "as
 * is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
 * OF THIS SOFTWARE.
 */

#include <config.h>
#include <libevdev/libevdev-int.h>
#include "test-common.h"

START_TEST(test_type_codes)
{
	ck_assert(libevdev_event_type_from_name("EV_SYN") == EV_SYN);
	ck_assert(libevdev_event_type_from_name("EV_KEY") == EV_KEY);
	ck_assert(libevdev_event_type_from_name("EV_REL") == EV_REL);
	ck_assert(libevdev_event_type_from_name("EV_ABS") == EV_ABS);
	ck_assert(libevdev_event_type_from_name("EV_MSC") == EV_MSC);
	ck_assert(libevdev_event_type_from_name("EV_SND") == EV_SND);
	ck_assert(libevdev_event_type_from_name("EV_SW") == EV_SW);
	ck_assert(libevdev_event_type_from_name("EV_LED") == EV_LED);
	ck_assert(libevdev_event_type_from_name("EV_REP") == EV_REP);
	ck_assert(libevdev_event_type_from_name("EV_FF") == EV_FF);
	ck_assert(libevdev_event_type_from_name("EV_FF_STATUS") == EV_FF_STATUS);
	ck_assert(libevdev_event_type_from_name("EV_MAX") == EV_MAX);

	ck_assert(libevdev_event_type_from_name_n("EV_SYNTAX", 6) == EV_SYN);
	ck_assert(libevdev_event_type_from_name_n("EV_REPTILE", 6) == EV_REP);
}
END_TEST

START_TEST(test_type_invalid)
{
	ck_assert(libevdev_event_type_from_name("EV_Syn") == -1);
	ck_assert(libevdev_event_type_from_name("ev_SYN") == -1);
	ck_assert(libevdev_event_type_from_name("SYN") == -1);
	ck_assert(libevdev_event_type_from_name("EV_SYNTAX") == -1);

	ck_assert(libevdev_event_type_from_name_n("EV_SYN", 5) == -1);
	ck_assert(libevdev_event_type_from_name_n("EV_REPTILE", 7) == -1);
}
END_TEST

START_TEST(test_key_codes)
{
	ck_assert(libevdev_event_code_from_name(EV_SYN, "SYN_REPORT") == SYN_REPORT);
	ck_assert(libevdev_event_code_from_name(EV_ABS, "ABS_X") == ABS_X);
	ck_assert(libevdev_event_code_from_name(EV_KEY, "BTN_A") == BTN_A);
	ck_assert(libevdev_event_code_from_name(EV_KEY, "KEY_A") == KEY_A);
	ck_assert(libevdev_event_code_from_name(EV_REL, "REL_X") == REL_X);
	ck_assert(libevdev_event_code_from_name(EV_MSC, "MSC_RAW") == MSC_RAW);
	ck_assert(libevdev_event_code_from_name(EV_LED, "LED_KANA") == LED_KANA);
	ck_assert(libevdev_event_code_from_name(EV_SND, "SND_BELL") == SND_BELL);
	ck_assert(libevdev_event_code_from_name(EV_REP, "REP_DELAY") == REP_DELAY);
	ck_assert(libevdev_event_code_from_name(EV_SYN, "SYN_DROPPED") == SYN_DROPPED);
	ck_assert(libevdev_event_code_from_name(EV_KEY, "KEY_RESERVED") == KEY_RESERVED);
	ck_assert(libevdev_event_code_from_name(EV_KEY, "BTN_0") == BTN_0);
	ck_assert(libevdev_event_code_from_name(EV_KEY, "KEY_0") == KEY_0);
	ck_assert(libevdev_event_code_from_name(EV_FF, "FF_GAIN") == FF_GAIN);
	ck_assert(libevdev_event_code_from_name(EV_FF_STATUS, "FF_STATUS_MAX") == FF_STATUS_MAX);
	ck_assert(libevdev_event_code_from_name(EV_SW, "SW_MAX") == SW_MAX);

	ck_assert(libevdev_event_code_from_name_n(EV_ABS, "ABS_YXZ", 5) == ABS_Y);
}
END_TEST

START_TEST(test_key_invalid)
{
	ck_assert(libevdev_event_code_from_name(EV_MAX, "MAX_FAKE") == -1);
	ck_assert(libevdev_event_code_from_name(EV_CNT, "CNT_FAKE") == -1);
	ck_assert(libevdev_event_code_from_name(EV_PWR, "PWR_SOMETHING") == -1);
	ck_assert(libevdev_event_code_from_name(EV_ABS, "EV_ABS") == -1);
	ck_assert(libevdev_event_code_from_name(EV_ABS, "ABS_XY") == -1);
	ck_assert(libevdev_event_code_from_name(EV_KEY, "BTN_GAMEPAD") == -1);
	ck_assert(libevdev_event_code_from_name(EV_KEY, "BUS_PCI") == -1);
	ck_assert(libevdev_event_code_from_name(EV_FF_STATUS, "FF_STATUS") == -1);
	ck_assert(libevdev_event_code_from_name(EV_FF_STATUS, "FF_STATUS_") == -1);
	ck_assert(libevdev_event_code_from_name(EV_FF, "FF_STATUS") == -1);
	ck_assert(libevdev_event_code_from_name(EV_FF, "FF_STATUS_") == -1);
	ck_assert(libevdev_event_code_from_name(EV_KEY, "ID_BUS") == -1);
	ck_assert(libevdev_event_code_from_name(EV_SND, "SND_CNT") == -1);
	ck_assert(libevdev_event_code_from_name(EV_SW, "SW_CNT") == -1);

	ck_assert(libevdev_event_code_from_name_n(EV_ABS, "ABS_X", 4) == -1);
}
END_TEST

Suite *
event_code_suite(void)
{
	Suite *s = suite_create("Event codes");

	TCase *tc = tcase_create("type tests");
	tcase_add_test(tc, test_type_codes);
	tcase_add_test(tc, test_type_invalid);
	suite_add_tcase(s, tc);

	tc = tcase_create("key tests");
	tcase_add_test(tc, test_key_codes);
	tcase_add_test(tc, test_key_invalid);
	suite_add_tcase(s, tc);

	return s;
}
