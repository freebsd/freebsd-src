/*
 * nan_test - NAN NDP state machine test cases
 * Copyright (C) 2025 Intel Corporation
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "utils/includes.h"
#include "utils/common.h"
#include "utils/list.h"
#include "nan_i.h"
#include "nan_module_tests.h"

static u8 default_bitmap[] = { 0xfe, 0xff, 0xfe, 0xff };

static void nan_test_schedule_cb(struct nan_schedule *sched, bool ndc,
				 int freq, int center_freq1, int center_freq2,
				 int bandwidth)
{
	os_memset(sched, 0, sizeof(*sched));

	/* Only map ID 0 is used */
	sched->map_ids_bitmap = 0x1;

	sched->n_chans = 1;
	sched->chans[0].chan.freq = freq;
	sched->chans[0].chan.center_freq1 = center_freq1;
	sched->chans[0].chan.center_freq2 = center_freq2;
	sched->chans[0].chan.bandwidth = bandwidth;

	sched->chans[0].committed.duration = 0;
	sched->chans[0].committed.period = 3;
	sched->chans[0].committed.offset = 0;
	sched->chans[0].committed.len = sizeof(default_bitmap);
	os_memcpy(sched->chans[0].committed.bitmap, default_bitmap,
		  sizeof(default_bitmap));

	sched->chans[0].map_id = 0;

	if (!ndc)
		return;

	os_memset(&sched->ndc, 0, sizeof(sched->ndc));
	sched->ndc.duration = 0;
	sched->ndc.period = 3;
	sched->ndc.offset = 0;
	sched->ndc.len = 1;
	sched->ndc.bitmap[0] = 0xfe;
}


static int nan_test_schedule_cb_all_ndc(struct nan_schedule *sched)
{
	static u8 seq_id = 1;

	nan_test_schedule_cb(sched, true, 5745, 5745, true, 20);
	sched->sequence_id = ++seq_id;
	return 0;
}


static int nan_test_schedule_cb_all_no_ndc(struct nan_schedule *sched)
{
	static u8 seq_id = 1;

	nan_test_schedule_cb(sched, false, 5745, 5745, 0, 20);
	sched->sequence_id = ++seq_id;
	return 0;
}


static int nan_test_schedule_cb_all_no_ndc_period_4(struct nan_schedule *sched)
{
	static u8 seq_id = 1;

	nan_test_schedule_cb(sched, false, 5745, 5745, 0, 20);

	/*
	 * Modify the map to have a period or 1024 TUs (both halfs are
	 * identical).
	 */
	sched->chans[0].committed.period = 4;
	sched->chans[0].committed.len += sizeof(default_bitmap);
	os_memcpy(sched->chans[0].committed.bitmap + sizeof(default_bitmap),
		  default_bitmap,
		  sizeof(default_bitmap));

	sched->sequence_id = ++seq_id;
	return 0;
}


static int nan_test_schedule_cb_2ghz_no_ndc(struct nan_schedule *sched)
{
	static u8 seq_id = 1;

	nan_test_schedule_cb(sched, false, 2437, 2437, 0, 20);
	sched->sequence_id = ++seq_id;
	return 0;
}


static int nan_test_get_chans_default(struct nan_channels *chans)
{
	chans->n_chans = 2;
	chans->chans = os_zalloc(sizeof(struct nan_channel_info) *
				 chans->n_chans);
	if (!chans->chans)
		return -1;

	chans->chans[0].channel = 6;
	chans->chans[0].op_class = 81;
	chans->chans[0].pref = 255;

	chans->chans[1].channel = 149;
	chans->chans[1].op_class = 126;
	chans->chans[1].pref = 254;

	return 0;
}


static int nan_test_get_chans_default_reverse(struct nan_channels *chans)
{
	chans->n_chans = 2;
	chans->chans = os_zalloc(sizeof(struct nan_channel_info) *
				 chans->n_chans);
	if (!chans->chans)
		return -1;

	chans->chans[0].channel = 149;
	chans->chans[0].op_class = 126;
	chans->chans[0].pref = 255;

	chans->chans[1].channel = 6;
	chans->chans[1].op_class = 81;
	chans->chans[1].pref = 254;

	return 0;
}


static int nan_test_get_chans_default_24g(struct nan_channels *chans)
{
	chans->n_chans = 1;
	chans->chans = os_zalloc(sizeof(struct nan_channel_info) *
				 chans->n_chans);
	if (!chans->chans)
		return -1;

	chans->chans[0].channel = 6;
	chans->chans[0].op_class = 81;
	chans->chans[0].pref = 255;

	return 0;
}


static int nan_test_get_chans_default_52g(struct nan_channels *chans)
{
	chans->n_chans = 1;
	chans->chans = os_zalloc(sizeof(struct nan_channel_info) *
				 chans->n_chans);
	if (!chans->chans)
		return -1;

	chans->chans[0].channel = 149;
	chans->chans[0].op_class = 126;
	chans->chans[0].pref = 255;

	return 0;
}


static struct nan_test_case three_way_ndp_two_way_ndl_chan_149 = {
	.name = "Three way NDP and two way NDL channel 149",
	.pub_conf = {
		.schedule_cb = nan_test_schedule_cb_all_ndc,
		.get_chans_cb = nan_test_get_chans_default,
		.n_ndps = 1,
		.ndp_confs = {
			{
				.accept_request = 1,
				.term_once_connected = 0,
				.expected_result =
					NAN_TEST_NDP_NOTIFY_CONNECTED,
			},
		},
		.pot_avail = {
			0x12, 0x0a, 0x00, 0x01, 0x00, 0x00, 0x05, 0x00,
			0xba, 0x02, 0x20, 0x02, 0x04
		},
		.pot_avail_len = 13,
	},
	.sub_conf = {
		.schedule_cb = nan_test_schedule_cb_all_no_ndc,
		.get_chans_cb = nan_test_get_chans_default,
		.n_ndps = 1,
		.ndp_confs = {
			{
				.accept_request = 1,
				.term_once_connected = 1,
				.expected_result =
					NAN_TEST_NDP_NOTIFY_CONNECTED,
			},
		},
	}
};


static struct nan_test_case three_way_ndp_two_way_ndl_diff_period = {
	.name = "Three way NDP and two way NDL channel 149. Subscriber period=4",
	.pub_conf = {
		.schedule_cb = nan_test_schedule_cb_all_ndc,
		.get_chans_cb = nan_test_get_chans_default,
		.n_ndps = 1,
		.ndp_confs = {
			{
				.accept_request = 1,
				.term_once_connected = 0,
				.expected_result =
					NAN_TEST_NDP_NOTIFY_CONNECTED,
			},
		},
		.pot_avail = {
			0x12, 0x0a, 0x00, 0x01, 0x00, 0x00, 0x05, 0x00,
			0xba, 0x02, 0x20, 0x02, 0x04
		},
		.pot_avail_len = 13,
	},
	.sub_conf = {
		.schedule_cb = nan_test_schedule_cb_all_no_ndc_period_4,
		.get_chans_cb = nan_test_get_chans_default,
		.n_ndps = 1,
		.ndp_confs = {
			{
				.accept_request = 1,
				.term_once_connected = 1,
				.expected_result =
					NAN_TEST_NDP_NOTIFY_CONNECTED,
			},
		},
	}
};

static struct nan_test_case three_way_ndp_two_way_ndl_chan_6 = {
	.name = "Three way NDP and two way NDL channel 6",
	.pub_conf = {
		.schedule_cb = nan_test_schedule_cb_all_ndc,
		.get_chans_cb = nan_test_get_chans_default,
		.n_ndps = 1,
		.ndp_confs = {
			{
				.accept_request = 1,
				.term_once_connected = 1,
				.expected_result =
					NAN_TEST_NDP_NOTIFY_CONNECTED,
			},
		},
		.pot_avail = {
			0x12, 0x0a, 0x00, 0x01, 0x00, 0x00, 0x05, 0x00,
			0xba, 0x02, 0x20, 0x02, 0x04
		},
		.pot_avail_len = 13,
	},
	.sub_conf = {
		.schedule_cb = nan_test_schedule_cb_all_no_ndc,
		.get_chans_cb = nan_test_get_chans_default_24g,
		.n_ndps = 1,
		.ndp_confs = {
			{
				.accept_request = 1,
				.term_once_connected = 0,
				.expected_result =
					NAN_TEST_NDP_NOTIFY_CONNECTED,
			},
		},
	}
};

static struct nan_test_case three_way_ndp_two_way_ndl_chan_mis = {
	.name = "Three way NDP and two way NDL channel mismatch",
	.pub_conf = {
		.schedule_cb = nan_test_schedule_cb_all_ndc,
		.get_chans_cb = nan_test_get_chans_default_52g,
		.n_ndps = 1,
		.ndp_confs = {
			{
				.accept_request = 1,
				.expected_result =
					NAN_TEST_NDP_NOTIFY_DISCONNECTED,
			},
		},
		.pot_avail = {
			0x12, 0x0a, 0x00, 0x01, 0x00, 0x00, 0x05, 0x00,
			0xba, 0x02, 0x20, 0x02, 0x04
		},
		.pot_avail_len = 13,
	},
	.sub_conf = {
		.schedule_cb = nan_test_schedule_cb_2ghz_no_ndc,
		.get_chans_cb = nan_test_get_chans_default_24g,
		.n_ndps = 1,
		.ndp_confs = {
			{
				.accept_request = 0,
				.expected_result =
					NAN_TEST_NDP_NOTIFY_DISCONNECTED,
			},
		},
	}
};

static struct nan_test_case three_way_ndp_three_way_ndl = {
	.name = "Three way NDP and three way NDL",
	.pub_conf = {
		.schedule_cb = nan_test_schedule_cb_all_ndc,
		.get_chans_cb = nan_test_get_chans_default,
		.n_ndps = 1,
		.ndp_confs = {
			{
				.accept_request = 1,
				.expected_result =
					NAN_TEST_NDP_NOTIFY_CONNECTED,
			},
		},
		.pot_avail = {
			0x12, 0x0a, 0x00, 0x01, 0x00, 0x00, 0x05, 0x00,
			0xba, 0x02, 0x20, 0x02, 0x04
		},
		.pot_avail_len = 13,
	},
	.sub_conf = {
		.schedule_cb = nan_test_schedule_cb_2ghz_no_ndc,
		.schedule_conf_cb = nan_test_schedule_cb_all_ndc,
		.get_chans_cb = nan_test_get_chans_default_reverse,
		.n_ndps = 1,
		.ndp_confs = {
			{
				.accept_request = 1,
				.expected_result =
					NAN_TEST_NDP_NOTIFY_CONNECTED,
				.term_once_connected = 1,
			},
		},
	}
};

static struct nan_test_case three_way_ndp_two_way_ndl_reject = {
	.name = "Three way NDP and two way NDL rejected",
	.pub_conf = {
		.schedule_cb = nan_test_schedule_cb_all_ndc,
		.get_chans_cb = nan_test_get_chans_default,
		.n_ndps = 1,
		.ndp_confs = {
			{
				.accept_request = 0,
				.expected_result =
					NAN_TEST_NDP_NOTIFY_DISCONNECTED,
				.reason = NAN_REASON_NDP_REJECTED,
			},
		},
		.pot_avail = {
			0x12, 0x0a, 0x00, 0x01, 0x00, 0x00, 0x05, 0x00,
			0xba, 0x02, 0x20, 0x02, 0x04
		},
		.pot_avail_len = 13,
	},
	.sub_conf = {
		.schedule_cb = nan_test_schedule_cb_all_no_ndc,
		.get_chans_cb = nan_test_get_chans_default,
		.n_ndps = 1,
		.ndp_confs = {
			{
				.accept_request = 0,
				.expected_result =
					NAN_TEST_NDP_NOTIFY_DISCONNECTED,
			},
		},
	}
};


static struct nan_test_case four_way_ndp_two_way_ndl_chan_149_ccm_128 = {
	.name = "Four way NDP and two way NDL channel 149 with CCMP 128",
	.pub_conf = {
		.schedule_cb = nan_test_schedule_cb_all_ndc,
		.get_chans_cb = nan_test_get_chans_default,
		.n_ndps = 1,
		.ndp_confs = {
			{
				.accept_request = 1,
				.expected_result =
					NAN_TEST_NDP_NOTIFY_CONNECTED,
				.csid = NAN_CS_SK_CCM_128,
				.expected_csid = NAN_CS_SK_CCM_128,
			},
		},
		.pot_avail = {
			0x12, 0x0a, 0x00, 0x01, 0x00, 0x00, 0x05, 0x00,
			0xba, 0x02, 0x20, 0x02, 0x04
		},
		.pot_avail_len = 13,
		.pmk = {
			0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7,
			0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7,
			0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7,
			0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7,
		},
	},
	.sub_conf = {
		.schedule_cb = nan_test_schedule_cb_all_no_ndc,
		.get_chans_cb = nan_test_get_chans_default,
		.n_ndps = 1,
		.ndp_confs = {
			{
				.accept_request = 1,
				.expected_result =
					NAN_TEST_NDP_NOTIFY_CONNECTED,
				.csid = NAN_CS_SK_CCM_128,
				.expected_csid = NAN_CS_SK_CCM_128,
				.term_once_connected = 1,
			},
		},
		.pmk = {
			0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7,
			0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7,
			0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7,
			0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7,
		},
	}
};

static struct nan_test_case four_way_ndp_two_way_ndl_chan_149_gcm_256 = {
	.name = "Four way NDP and three way NDL with GCMP 256",
	.pub_conf = {
		.schedule_cb = nan_test_schedule_cb_all_ndc,
		.get_chans_cb = nan_test_get_chans_default,
		.n_ndps = 1,
		.ndp_confs = {
			{
				.accept_request = 1,
				.expected_result =
					NAN_TEST_NDP_NOTIFY_CONNECTED,
				.csid = NAN_CS_SK_GCM_256,
				.expected_csid = NAN_CS_SK_GCM_256,
			},
		},
		.pot_avail = {
			0x12, 0x0a, 0x00, 0x01, 0x00, 0x00, 0x05, 0x00,
			0xba, 0x02, 0x20, 0x02, 0x04
		},
		.pot_avail_len = 13,
		.pmk = {
			0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7,
			0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7,
			0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7,
			0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7,
		},
	},
	.sub_conf = {
		.schedule_cb = nan_test_schedule_cb_2ghz_no_ndc,
		.schedule_conf_cb = nan_test_schedule_cb_all_ndc,
		.get_chans_cb = nan_test_get_chans_default_reverse,
		.n_ndps = 1,
		.ndp_confs = {
			{
				.accept_request = 1,
				.expected_result =
					NAN_TEST_NDP_NOTIFY_CONNECTED,
				.csid = NAN_CS_SK_GCM_256,
				.expected_csid = NAN_CS_SK_GCM_256,
				.term_once_connected = 1,
			},
		},
		.pmk = {
			0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7,
			0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7,
			0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7,
			0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7,
		},
	}
};


static struct nan_test_case pmk_mismatch = {
	.name = "PMK mismatch test case",
	.pub_conf = {
		.schedule_cb = nan_test_schedule_cb_all_ndc,
		.get_chans_cb = nan_test_get_chans_default,
		.n_ndps = 1,
		.ndp_confs = {
			{
				.accept_request = 1,
				.expected_result =
					NAN_TEST_NDP_NOTIFY_DISCONNECTED,
				.csid = NAN_CS_SK_GCM_256,
			},
		},
		.pot_avail = {
			0x12, 0x0a, 0x00, 0x01, 0x00, 0x00, 0x05, 0x00,
			0xba, 0x02, 0x20, 0x02, 0x04
		},
		.pot_avail_len = 13,
		.pmk = {
			0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7,
			0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7,
			0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7,
			0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7,
		},
	},
	.sub_conf = {
		.schedule_cb = nan_test_schedule_cb_all_no_ndc,
		.get_chans_cb = nan_test_get_chans_default,
		.n_ndps = 1,
		.ndp_confs = {
			{
				.accept_request = 1,
				.expected_result =
					NAN_TEST_NDP_NOTIFY_DISCONNECTED,
				.csid = NAN_CS_SK_GCM_256,
			},
		},
		.pmk = {
			0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7,
			0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7,
			0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7,
			0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x0,
		},
	}
};


static struct nan_test_case two_ndps_no_security = {
	.name = "Two NDPs: no security. Both accepted",
	.pub_conf = {
		.schedule_cb = nan_test_schedule_cb_all_ndc,
		.get_chans_cb = nan_test_get_chans_default,
		.n_ndps = 2,
		.ndp_confs = {
			{
				.accept_request = 1,
				.term_once_connected = 0,
				.expected_result =
					NAN_TEST_NDP_NOTIFY_CONNECTED,
			},
			{
				.accept_request = 1,
				.term_once_connected = 0,
				.expected_result =
					NAN_TEST_NDP_NOTIFY_CONNECTED,
			},
		},
		.pot_avail = {
			0x12, 0x0a, 0x00, 0x01, 0x00, 0x00, 0x05, 0x00,
			0xba, 0x02, 0x20, 0x02, 0x04
		},
		.pot_avail_len = 13,
	},
	.sub_conf = {
		.schedule_cb = nan_test_schedule_cb_all_no_ndc,
		.get_chans_cb = nan_test_get_chans_default,
		.n_ndps = 2,
		.ndp_confs = {
			{
				.accept_request = 1,
				.term_once_connected = 0,
				.expected_result =
					NAN_TEST_NDP_NOTIFY_CONNECTED,
			},
			{
				.accept_request = 1,
				.term_once_connected = 1,
				.expected_result =
					NAN_TEST_NDP_NOTIFY_CONNECTED,
			},
		},
	}
};


static struct nan_test_case three_ndps_no_security_middle_one_rejected = {
	.name = "Three NDPs: no security. Middle one rejected",
	.pub_conf = {
		.schedule_cb = nan_test_schedule_cb_all_ndc,
		.get_chans_cb = nan_test_get_chans_default,
		.n_ndps = 3,
		.ndp_confs = {
			{
				.accept_request = 1,
				.expected_result =
					NAN_TEST_NDP_NOTIFY_CONNECTED,
			},
			{
				.accept_request = 0,
				.expected_result =
					NAN_TEST_NDP_NOTIFY_DISCONNECTED,
			},
			{
				.accept_request = 1,
				.expected_result =
					NAN_TEST_NDP_NOTIFY_CONNECTED,
			},
		},
		.pot_avail = {
			0x12, 0x0a, 0x00, 0x01, 0x00, 0x00, 0x05, 0x00,
			0xba, 0x02, 0x20, 0x02, 0x04
		},
		.pot_avail_len = 13,
	},
	.sub_conf = {
		.schedule_cb = nan_test_schedule_cb_all_no_ndc,
		.get_chans_cb = nan_test_get_chans_default,
		.n_ndps = 3,
		.ndp_confs = {
			{
				.accept_request = 1,
				.expected_result =
					NAN_TEST_NDP_NOTIFY_CONNECTED,
			},
			{
				.accept_request = 1,
				.expected_result =
					NAN_TEST_NDP_NOTIFY_DISCONNECTED,
			},
			{
				.accept_request = 1,
				.term_once_connected = 1,
				.expected_result =
					NAN_TEST_NDP_NOTIFY_CONNECTED,
			},
		},
	}
};


static struct nan_test_case three_ndps_increasing_security = {
	.name = "Three NDPs: increasing security levels",
	.pub_conf = {
		.schedule_cb = nan_test_schedule_cb_all_ndc,
		.get_chans_cb = nan_test_get_chans_default,
		.n_ndps = 3,
		.ndp_confs = {
			{
				.accept_request = 1,
				.expected_result =
					NAN_TEST_NDP_NOTIFY_CONNECTED,
			},
			{
				.accept_request = 1,
				.expected_result =
					NAN_TEST_NDP_NOTIFY_CONNECTED,
				.csid = NAN_CS_SK_CCM_128,
				.expected_csid = NAN_CS_SK_CCM_128,
			},
			{
				.accept_request = 1,
				.expected_result =
					NAN_TEST_NDP_NOTIFY_CONNECTED,
				.csid = NAN_CS_SK_GCM_256,
				.expected_csid = NAN_CS_SK_GCM_256,
			},
		},
		.pot_avail = {
			0x12, 0x0a, 0x00, 0x01, 0x00, 0x00, 0x05, 0x00,
			0xba, 0x02, 0x20, 0x02, 0x04
		},
		.pot_avail_len = 13,
		.pmk = {
			0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7,
			0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7,
			0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7,
			0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7,
		},
	},
	.sub_conf = {
		.schedule_cb = nan_test_schedule_cb_all_no_ndc,
		.get_chans_cb = nan_test_get_chans_default,
		.n_ndps = 3,
		.ndp_confs = {
			{
				.accept_request = 1,
				.expected_result =
					NAN_TEST_NDP_NOTIFY_CONNECTED,
			},
			{
				.accept_request = 1,
				.expected_result =
					NAN_TEST_NDP_NOTIFY_CONNECTED,
				.csid = NAN_CS_SK_CCM_128,
				.expected_csid = NAN_CS_SK_CCM_128,
			},
			{
				.accept_request = 1,
				.term_once_connected = 1,
				.expected_result =
					NAN_TEST_NDP_NOTIFY_CONNECTED,
				.csid = NAN_CS_SK_GCM_256,
				.expected_csid = NAN_CS_SK_GCM_256,
			},
		},
		.pmk = {
			0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7,
			0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7,
			0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7,
			0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7,
		},
	}
};


static struct nan_test_case three_ndps_deacreasing_security = {
	.name = "Three NDPs: decreasing security levels",
	.pub_conf = {
		.schedule_cb = nan_test_schedule_cb_all_ndc,
		.get_chans_cb = nan_test_get_chans_default,
		.n_ndps = 3,
		.ndp_confs = {
			{
				.accept_request = 1,
				.expected_result =
					NAN_TEST_NDP_NOTIFY_CONNECTED,
				.csid = NAN_CS_SK_GCM_256,
				.expected_csid = NAN_CS_SK_GCM_256,
			},
			{
				.accept_request = 1,
				.expected_result =
					NAN_TEST_NDP_NOTIFY_CONNECTED,
				.csid = NAN_CS_SK_CCM_128,
				.expected_csid = NAN_CS_SK_GCM_256,
			},
			{
				.accept_request = 1,
				.expected_result =
					NAN_TEST_NDP_NOTIFY_CONNECTED,
				.expected_csid = NAN_CS_SK_GCM_256,
			},
		},
		.pot_avail = {
			0x12, 0x0a, 0x00, 0x01, 0x00, 0x00, 0x05, 0x00,
			0xba, 0x02, 0x20, 0x02, 0x04
		},
		.pot_avail_len = 13,
		.pmk = {
			0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7,
			0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7,
			0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7,
			0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7,
		},
	},
	.sub_conf = {
		.schedule_cb = nan_test_schedule_cb_all_no_ndc,
		.get_chans_cb = nan_test_get_chans_default,
		.n_ndps = 3,
		.ndp_confs = {
			{
				.accept_request = 1,
				.expected_result =
					NAN_TEST_NDP_NOTIFY_CONNECTED,
				.csid = NAN_CS_SK_GCM_256,
				.expected_csid = NAN_CS_SK_GCM_256,
			},
			{
				.accept_request = 1,
				.expected_result =
					NAN_TEST_NDP_NOTIFY_CONNECTED,
				.csid = NAN_CS_SK_CCM_128,
				.expected_csid = NAN_CS_SK_GCM_256,
			},
			{
				.accept_request = 1,
				.term_once_connected = 1,
				.expected_result =
					NAN_TEST_NDP_NOTIFY_CONNECTED,
				.expected_csid = NAN_CS_SK_GCM_256,
			},
		},
		.pmk = {
			0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7,
			0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7,
			0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7,
			0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7,
		},
	}
};


static const struct nan_test_case *g_nan_test_cases[] = {
	&three_way_ndp_two_way_ndl_chan_149,
	&three_way_ndp_two_way_ndl_diff_period,
	&three_way_ndp_two_way_ndl_chan_6,
	&three_way_ndp_two_way_ndl_chan_mis,
	&three_way_ndp_two_way_ndl_reject,
	&three_way_ndp_three_way_ndl,
	&four_way_ndp_two_way_ndl_chan_149_ccm_128,
	&four_way_ndp_two_way_ndl_chan_149_gcm_256,
	&pmk_mismatch,
	&two_ndps_no_security,
	&three_ndps_no_security_middle_one_rejected,
	&three_ndps_increasing_security,
	&three_ndps_deacreasing_security,
	NULL
};


const struct nan_test_case * nan_test_case_get_next(void)
{
	static u32 nan_test_case_idx = 0;
	const struct nan_test_case *curr = g_nan_test_cases[nan_test_case_idx];

	if (curr)
		nan_test_case_idx++;

	return curr;
}
