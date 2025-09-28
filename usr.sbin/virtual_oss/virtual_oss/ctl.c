/*-
 * Copyright (c) 2012-2022 Hans Petter Selasky
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
 */

#include <sys/types.h>
#include <sys/queue.h>

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <cuse.h>

#include "int.h"
#include "virtual_oss.h"

int64_t	voss_output_peak[VMAX_CHAN];
int64_t	voss_input_peak[VMAX_CHAN];

static int
vctl_open(struct cuse_dev *pdev __unused, int fflags __unused)
{
	return (0);
}

static int
vctl_close(struct cuse_dev *pdev __unused, int fflags __unused)
{
	return (0);
}

static vprofile_t *
vprofile_by_index(const vprofile_head_t *phead, int index)
{
	vprofile_t *pvp;

	TAILQ_FOREACH(pvp, phead, entry) {
		if (!index--)
			return (pvp);
	}
	return (NULL);
}

static vmonitor_t *
vmonitor_by_index(int index, vmonitor_head_t *phead)
{
	vmonitor_t *pvm;

	TAILQ_FOREACH(pvm, phead, entry) {
		if (!index--)
			return (pvm);
	}
	return (NULL);
}

static int
vctl_ioctl(struct cuse_dev *pdev __unused, int fflags __unused,
    unsigned long cmd, void *peer_data)
{
	union {
		int	val;
		struct virtual_oss_io_info io_info;
		struct virtual_oss_mon_info mon_info;
		struct virtual_oss_io_peak io_peak;
		struct virtual_oss_mon_peak mon_peak;
		struct virtual_oss_compressor out_lim;
		struct virtual_oss_io_limit io_lim;
		struct virtual_oss_master_peak master_peak;
		struct virtual_oss_audio_delay_locator ad_locator;
		struct virtual_oss_fir_filter fir_filter;
		struct virtual_oss_system_info sys_info;
		char	options[VIRTUAL_OSS_OPTIONS_MAX];
	}     data;

	vprofile_t *pvp;
	vmonitor_t *pvm;

	int chan;
	int len;
	int error;

	len = IOCPARM_LEN(cmd);

	if (len < 0 || len > (int)sizeof(data))
		return (CUSE_ERR_INVALID);

	if (cmd & IOC_IN) {
		error = cuse_copy_in(peer_data, &data, len);
		if (error)
			return (error);
	} else {
		error = 0;
	}

	atomic_lock();
	switch (cmd) {
	case VIRTUAL_OSS_GET_DEV_INFO:
	case VIRTUAL_OSS_SET_DEV_INFO:
	case VIRTUAL_OSS_GET_DEV_PEAK:
	case VIRTUAL_OSS_SET_DEV_LIMIT:
	case VIRTUAL_OSS_GET_DEV_LIMIT:
	case VIRTUAL_OSS_SET_RX_DEV_FIR_FILTER:
	case VIRTUAL_OSS_GET_RX_DEV_FIR_FILTER:
	case VIRTUAL_OSS_SET_TX_DEV_FIR_FILTER:
	case VIRTUAL_OSS_GET_TX_DEV_FIR_FILTER:
		pvp = vprofile_by_index(&virtual_profile_client_head, data.val);
		break;
	case VIRTUAL_OSS_GET_LOOP_INFO:
	case VIRTUAL_OSS_SET_LOOP_INFO:
	case VIRTUAL_OSS_GET_LOOP_PEAK:
	case VIRTUAL_OSS_SET_LOOP_LIMIT:
	case VIRTUAL_OSS_GET_LOOP_LIMIT:
	case VIRTUAL_OSS_SET_RX_LOOP_FIR_FILTER:
	case VIRTUAL_OSS_GET_RX_LOOP_FIR_FILTER:
	case VIRTUAL_OSS_SET_TX_LOOP_FIR_FILTER:
	case VIRTUAL_OSS_GET_TX_LOOP_FIR_FILTER:
		pvp = vprofile_by_index(&virtual_profile_loopback_head, data.val);
		break;
	default:
		pvp = NULL;
		break;
	}

	switch (cmd) {
	case VIRTUAL_OSS_GET_VERSION:
		data.val = VIRTUAL_OSS_VERSION;
		break;
	case VIRTUAL_OSS_GET_DEV_INFO:
	case VIRTUAL_OSS_GET_LOOP_INFO:
		if (pvp == NULL ||
		    data.io_info.channel < 0 ||
		    data.io_info.channel >= (int)pvp->channels) {
			error = CUSE_ERR_INVALID;
			break;
		}
		strlcpy(data.io_info.name, pvp->oss_name, sizeof(data.io_info.name));
		chan = data.io_info.channel;
		data.io_info.rx_amp = pvp->rx_shift[chan];
		data.io_info.tx_amp = pvp->tx_shift[chan];
		data.io_info.rx_chan = pvp->rx_src[chan];
		data.io_info.tx_chan = pvp->tx_dst[chan];
		data.io_info.rx_mute = pvp->rx_mute[chan] ? 1 : 0;
		data.io_info.tx_mute = pvp->tx_mute[chan] ? 1 : 0;
		data.io_info.rx_pol = pvp->rx_pol[chan] ? 1 : 0;
		data.io_info.tx_pol = pvp->tx_pol[chan] ? 1 : 0;
		data.io_info.bits = pvp->bits;
		data.io_info.rx_delay = pvp->rec_delay;
		data.io_info.rx_delay_limit = voss_dsp_sample_rate;
		break;
	case VIRTUAL_OSS_SET_DEV_INFO:
	case VIRTUAL_OSS_SET_LOOP_INFO:
		if (pvp == NULL ||
		    data.io_info.channel < 0 ||
		    data.io_info.channel >= (int)pvp->channels ||
		    data.io_info.rx_amp < -31 || data.io_info.rx_amp > 31 ||
		    data.io_info.tx_amp < -31 || data.io_info.tx_amp > 31 ||
		    data.io_info.rx_delay < 0 ||
		    data.io_info.rx_delay > (int)voss_dsp_sample_rate) {
			error = CUSE_ERR_INVALID;
			break;
		}
		chan = data.io_info.channel;
		pvp->rx_shift[chan] = data.io_info.rx_amp;
		pvp->tx_shift[chan] = data.io_info.tx_amp;
		pvp->rx_src[chan] = data.io_info.rx_chan;
		pvp->tx_dst[chan] = data.io_info.tx_chan;
		pvp->rx_mute[chan] = data.io_info.rx_mute ? 1 : 0;
		pvp->tx_mute[chan] = data.io_info.tx_mute ? 1 : 0;
		pvp->rx_pol[chan] = data.io_info.rx_pol ? 1 : 0;
		pvp->tx_pol[chan] = data.io_info.tx_pol ? 1 : 0;
		pvp->rec_delay = data.io_info.rx_delay;
		break;
	case VIRTUAL_OSS_GET_INPUT_MON_INFO:
		pvm = vmonitor_by_index(data.mon_info.number,
		    &virtual_monitor_input);
		if (pvm == NULL) {
			error = CUSE_ERR_INVALID;
			break;
		}
		data.mon_info.src_chan = pvm->src_chan;
		data.mon_info.dst_chan = pvm->dst_chan;
		data.mon_info.pol = pvm->pol;
		data.mon_info.mute = pvm->mute;
		data.mon_info.amp = pvm->shift;
		data.mon_info.bits = voss_dsp_bits;
		break;
	case VIRTUAL_OSS_SET_INPUT_MON_INFO:
		pvm = vmonitor_by_index(data.mon_info.number,
		    &virtual_monitor_input);
		if (pvm == NULL ||
		    data.mon_info.amp < -31 ||
		    data.mon_info.amp > 31) {
			error = CUSE_ERR_INVALID;
			break;
		}
		pvm->src_chan = data.mon_info.src_chan;
		pvm->dst_chan = data.mon_info.dst_chan;
		pvm->pol = data.mon_info.pol ? 1 : 0;
		pvm->mute = data.mon_info.mute ? 1 : 0;
		pvm->shift = data.mon_info.amp;
		break;
	case VIRTUAL_OSS_GET_OUTPUT_MON_INFO:
		pvm = vmonitor_by_index(data.mon_info.number,
		    &virtual_monitor_output);
		if (pvm == NULL) {
			error = CUSE_ERR_INVALID;
			break;
		}
		data.mon_info.src_chan = pvm->src_chan;
		data.mon_info.dst_chan = pvm->dst_chan;
		data.mon_info.pol = pvm->pol;
		data.mon_info.mute = pvm->mute;
		data.mon_info.amp = pvm->shift;
		data.mon_info.bits = voss_dsp_bits;
		break;
	case VIRTUAL_OSS_SET_OUTPUT_MON_INFO:
		pvm = vmonitor_by_index(data.mon_info.number,
		    &virtual_monitor_output);
		if (pvm == NULL ||
		    data.mon_info.amp < -31 ||
		    data.mon_info.amp > 31) {
			error = CUSE_ERR_INVALID;
			break;
		}
		pvm->src_chan = data.mon_info.src_chan;
		pvm->dst_chan = data.mon_info.dst_chan;
		pvm->pol = data.mon_info.pol ? 1 : 0;
		pvm->mute = data.mon_info.mute ? 1 : 0;
		pvm->shift = data.mon_info.amp;
		break;
	case VIRTUAL_OSS_GET_LOCAL_MON_INFO:
		pvm = vmonitor_by_index(data.mon_info.number,
		    &virtual_monitor_local);
		if (pvm == NULL) {
			error = CUSE_ERR_INVALID;
			break;
		}
		data.mon_info.src_chan = pvm->src_chan;
		data.mon_info.dst_chan = pvm->dst_chan;
		data.mon_info.pol = pvm->pol;
		data.mon_info.mute = pvm->mute;
		data.mon_info.amp = pvm->shift;
		data.mon_info.bits = voss_dsp_bits;
		break;
	case VIRTUAL_OSS_SET_LOCAL_MON_INFO:
		pvm = vmonitor_by_index(data.mon_info.number,
		    &virtual_monitor_local);
		if (pvm == NULL ||
		    data.mon_info.amp < -31 ||
		    data.mon_info.amp > 31) {
			error = CUSE_ERR_INVALID;
			break;
		}
		pvm->src_chan = data.mon_info.src_chan;
		pvm->dst_chan = data.mon_info.dst_chan;
		pvm->pol = data.mon_info.pol ? 1 : 0;
		pvm->mute = data.mon_info.mute ? 1 : 0;
		pvm->shift = data.mon_info.amp;
		break;
	case VIRTUAL_OSS_GET_DEV_PEAK:
	case VIRTUAL_OSS_GET_LOOP_PEAK:
		if (pvp == NULL ||
		    data.io_peak.channel < 0 ||
		    data.io_peak.channel >= (int)pvp->channels) {
			error = CUSE_ERR_INVALID;
			break;
		}
		strlcpy(data.io_peak.name, pvp->oss_name, sizeof(data.io_peak.name));
		chan = data.io_peak.channel;
		data.io_peak.rx_peak_value = pvp->rx_peak_value[chan];
		pvp->rx_peak_value[chan] = 0;
		data.io_peak.tx_peak_value = pvp->tx_peak_value[chan];
		pvp->tx_peak_value[chan] = 0;
		data.io_peak.bits = pvp->bits;
		break;
	case VIRTUAL_OSS_GET_INPUT_MON_PEAK:
		pvm = vmonitor_by_index(data.mon_peak.number,
		    &virtual_monitor_input);
		if (pvm == NULL) {
			error = CUSE_ERR_INVALID;
			break;
		}
		data.mon_peak.peak_value = pvm->peak_value;
		data.mon_peak.bits = voss_dsp_bits;
		pvm->peak_value = 0;
		break;
	case VIRTUAL_OSS_GET_OUTPUT_MON_PEAK:
		pvm = vmonitor_by_index(data.mon_peak.number,
		    &virtual_monitor_output);
		if (pvm == NULL) {
			error = CUSE_ERR_INVALID;
			break;
		}
		data.mon_peak.peak_value = pvm->peak_value;
		data.mon_peak.bits = voss_dsp_bits;
		pvm->peak_value = 0;
		break;
	case VIRTUAL_OSS_GET_LOCAL_MON_PEAK:
		pvm = vmonitor_by_index(data.mon_peak.number,
		    &virtual_monitor_local);
		if (pvm == NULL) {
			error = CUSE_ERR_INVALID;
			break;
		}
		data.mon_peak.peak_value = pvm->peak_value;
		data.mon_peak.bits = voss_dsp_bits;
		pvm->peak_value = 0;
		break;
	case VIRTUAL_OSS_ADD_INPUT_MON:
		pvm = vmonitor_alloc(&data.val,
		    &virtual_monitor_input);
		if (pvm == NULL)
			error = CUSE_ERR_INVALID;
		break;
	case VIRTUAL_OSS_ADD_OUTPUT_MON:
		pvm = vmonitor_alloc(&data.val,
		    &virtual_monitor_output);
		if (pvm == NULL)
			error = CUSE_ERR_INVALID;
		break;
	case VIRTUAL_OSS_ADD_LOCAL_MON:
		pvm = vmonitor_alloc(&data.val,
		    &virtual_monitor_local);
		if (pvm == NULL)
			error = CUSE_ERR_INVALID;
		break;
	case VIRTUAL_OSS_SET_OUTPUT_LIMIT:
		if (data.out_lim.enabled < 0 ||
		    data.out_lim.enabled > 1 ||
		    data.out_lim.knee < VIRTUAL_OSS_KNEE_MIN ||
		    data.out_lim.knee > VIRTUAL_OSS_KNEE_MAX ||
		    data.out_lim.attack < VIRTUAL_OSS_ATTACK_MIN ||
		    data.out_lim.attack > VIRTUAL_OSS_ATTACK_MAX ||
		    data.out_lim.decay < VIRTUAL_OSS_DECAY_MIN ||
		    data.out_lim.decay > VIRTUAL_OSS_DECAY_MAX ||
		    data.out_lim.gain != 0) {
			error = CUSE_ERR_INVALID;
			break;
		}
		voss_output_compressor_param.enabled = data.out_lim.enabled;
		voss_output_compressor_param.knee = data.out_lim.knee;
		voss_output_compressor_param.attack = data.out_lim.attack;
		voss_output_compressor_param.decay = data.out_lim.decay;
		break;
	case VIRTUAL_OSS_GET_OUTPUT_LIMIT:
		data.out_lim.enabled = voss_output_compressor_param.enabled;
		data.out_lim.knee = voss_output_compressor_param.knee;
		data.out_lim.attack = voss_output_compressor_param.attack;
		data.out_lim.decay = voss_output_compressor_param.decay;
		data.out_lim.gain = 1000;
		for (chan = 0; chan != VMAX_CHAN; chan++) {
			int gain = voss_output_compressor_gain[chan] * 1000.0;
			if (data.out_lim.gain > gain)
				data.out_lim.gain = gain;
		}
		break;
	case VIRTUAL_OSS_SET_DEV_LIMIT:
	case VIRTUAL_OSS_SET_LOOP_LIMIT:
		if (pvp == NULL ||
		    data.io_lim.param.enabled < 0 ||
		    data.io_lim.param.enabled > 1 ||
		    data.io_lim.param.knee < VIRTUAL_OSS_KNEE_MIN ||
		    data.io_lim.param.knee > VIRTUAL_OSS_KNEE_MAX ||
		    data.io_lim.param.attack < VIRTUAL_OSS_ATTACK_MIN ||
		    data.io_lim.param.attack > VIRTUAL_OSS_ATTACK_MAX ||
		    data.io_lim.param.decay < VIRTUAL_OSS_DECAY_MIN ||
		    data.io_lim.param.decay > VIRTUAL_OSS_DECAY_MAX ||
		    data.io_lim.param.gain != 0) {
			error = CUSE_ERR_INVALID;
			break;
		}
		pvp->rx_compressor_param.enabled = data.io_lim.param.enabled;
		pvp->rx_compressor_param.knee = data.io_lim.param.knee;
		pvp->rx_compressor_param.attack = data.io_lim.param.attack;
		pvp->rx_compressor_param.decay = data.io_lim.param.decay;
		break;
	case VIRTUAL_OSS_GET_DEV_LIMIT:
	case VIRTUAL_OSS_GET_LOOP_LIMIT:
		if (pvp == NULL) {
			error = CUSE_ERR_INVALID;
			break;
		}
		data.io_lim.param.enabled = pvp->rx_compressor_param.enabled;
		data.io_lim.param.knee = pvp->rx_compressor_param.knee;
		data.io_lim.param.attack = pvp->rx_compressor_param.attack;
		data.io_lim.param.decay = pvp->rx_compressor_param.decay;
		data.io_lim.param.gain = 1000;

		for (chan = 0; chan != VMAX_CHAN; chan++) {
			int gain = pvp->rx_compressor_gain[chan] * 1000.0;
			if (data.io_lim.param.gain > gain)
				data.io_lim.param.gain = gain;
		}
		break;
	case VIRTUAL_OSS_GET_OUTPUT_PEAK:
		chan = data.master_peak.channel;
		if (chan < 0 ||
		    chan >= (int)voss_max_channels) {
			error = CUSE_ERR_INVALID;
			break;
		}
		data.master_peak.bits = voss_dsp_bits;
		data.master_peak.peak_value = voss_output_peak[chan];
		voss_output_peak[chan] = 0;
		break;
	case VIRTUAL_OSS_GET_INPUT_PEAK:
		chan = data.master_peak.channel;
		if (chan < 0 ||
		    chan >= (int)voss_dsp_max_channels) {
			error = CUSE_ERR_INVALID;
			break;
		}
		data.master_peak.bits = voss_dsp_bits;
		data.master_peak.peak_value = voss_input_peak[chan];
		voss_input_peak[chan] = 0;
		break;

	case VIRTUAL_OSS_SET_RECORDING:
		voss_is_recording = data.val ? 1 : 0;
		break;

	case VIRTUAL_OSS_GET_RECORDING:
		data.val = voss_is_recording;
		break;

	case VIRTUAL_OSS_SET_AUDIO_DELAY_LOCATOR:
		if (data.ad_locator.channel_output < 0 ||
		    data.ad_locator.channel_output >= (int)voss_mix_channels) {
			error = CUSE_ERR_INVALID;
			break;
		}
		if (data.ad_locator.channel_input < 0 ||
		    data.ad_locator.channel_input >= (int)voss_mix_channels) {
			error = CUSE_ERR_INVALID;
			break;
		}
		if (data.ad_locator.signal_output_level < 0 ||
		    data.ad_locator.signal_output_level >= 64) {
			error = CUSE_ERR_INVALID;
			break;
		}
		voss_ad_enabled = (data.ad_locator.locator_enabled != 0);
		voss_ad_output_signal = data.ad_locator.signal_output_level;
		voss_ad_output_channel = data.ad_locator.channel_output;
		voss_ad_input_channel = data.ad_locator.channel_input;
		break;

	case VIRTUAL_OSS_GET_AUDIO_DELAY_LOCATOR:
		data.ad_locator.locator_enabled = voss_ad_enabled;
		data.ad_locator.signal_output_level = voss_ad_output_signal;
		data.ad_locator.channel_output = voss_ad_output_channel;
		data.ad_locator.channel_input = voss_ad_input_channel;
		data.ad_locator.channel_last = voss_mix_channels - 1;
		data.ad_locator.signal_input_delay = voss_ad_last_delay;
		data.ad_locator.signal_delay_hz = voss_dsp_sample_rate;
		break;

	case VIRTUAL_OSS_RST_AUDIO_DELAY_LOCATOR:
		voss_ad_reset();
		break;

	case VIRTUAL_OSS_ADD_OPTIONS:
		data.options[VIRTUAL_OSS_OPTIONS_MAX - 1] = 0;
		voss_add_options(data.options);
		break;

	case VIRTUAL_OSS_GET_RX_DEV_FIR_FILTER:
	case VIRTUAL_OSS_GET_RX_LOOP_FIR_FILTER:
		if (pvp == NULL ||
		    data.fir_filter.channel < 0 ||
		    data.fir_filter.channel >= (int)pvp->channels) {
			error = CUSE_ERR_INVALID;
		} else if (data.fir_filter.filter_data == NULL) {
			data.fir_filter.filter_size = pvp->rx_filter_size;
		} else if (data.fir_filter.filter_size != (int)pvp->rx_filter_size) {
			error = CUSE_ERR_INVALID;
		} else if (pvp->rx_filter_data[data.fir_filter.channel] == NULL) {
			error = CUSE_ERR_NO_MEMORY;	/* filter disabled */
		} else {
			error = cuse_copy_out(pvp->rx_filter_data[data.fir_filter.channel],
			    data.fir_filter.filter_data,
			    sizeof(pvp->rx_filter_data[0][0]) *
			    data.fir_filter.filter_size);
		}
		break;

	case VIRTUAL_OSS_GET_TX_DEV_FIR_FILTER:
	case VIRTUAL_OSS_GET_TX_LOOP_FIR_FILTER:
		if (pvp == NULL ||
		    data.fir_filter.channel < 0 ||
		    data.fir_filter.channel >= (int)pvp->channels) {
			error = CUSE_ERR_INVALID;
		} else if (data.fir_filter.filter_data == NULL) {
			data.fir_filter.filter_size = pvp->tx_filter_size;
		} else if (data.fir_filter.filter_size != (int)pvp->tx_filter_size) {
			error = CUSE_ERR_INVALID;
		} else if (pvp->tx_filter_data[data.fir_filter.channel] == NULL) {
			error = CUSE_ERR_NO_MEMORY;	/* filter disabled */
		} else {
			error = cuse_copy_out(pvp->tx_filter_data[data.fir_filter.channel],
			    data.fir_filter.filter_data,
			    sizeof(pvp->tx_filter_data[0][0]) *
			    data.fir_filter.filter_size);
		}
		break;

	case VIRTUAL_OSS_SET_RX_DEV_FIR_FILTER:
	case VIRTUAL_OSS_SET_RX_LOOP_FIR_FILTER:
		if (pvp == NULL ||
		    data.fir_filter.channel < 0 ||
		    data.fir_filter.channel >= (int)pvp->channels) {
			error = CUSE_ERR_INVALID;
		} else if (data.fir_filter.filter_data == NULL) {
			free(pvp->rx_filter_data[data.fir_filter.channel]);
			pvp->rx_filter_data[data.fir_filter.channel] = NULL;	/* disable filter */
		} else if (data.fir_filter.filter_size != (int)pvp->rx_filter_size) {
			error = CUSE_ERR_INVALID;
		} else if (pvp->rx_filter_size != 0) {
			size_t size = sizeof(pvp->rx_filter_data[0][0]) * pvp->rx_filter_size;
			if (pvp->rx_filter_data[data.fir_filter.channel] == NULL) {
				pvp->rx_filter_data[data.fir_filter.channel] = malloc(size);
				if (pvp->rx_filter_data[data.fir_filter.channel] == NULL)
					error = CUSE_ERR_NO_MEMORY;
				else
					memset(pvp->rx_filter_data[data.fir_filter.channel], 0, size);
			}
			if (pvp->rx_filter_data[data.fir_filter.channel] != NULL) {
				error = cuse_copy_in(data.fir_filter.filter_data,
				    pvp->rx_filter_data[data.fir_filter.channel], size);
			}
		}
		break;

	case VIRTUAL_OSS_SET_TX_DEV_FIR_FILTER:
	case VIRTUAL_OSS_SET_TX_LOOP_FIR_FILTER:
		if (pvp == NULL ||
		    data.fir_filter.channel < 0 ||
		    data.fir_filter.channel >= (int)pvp->channels) {
			error = CUSE_ERR_INVALID;
		} else if (data.fir_filter.filter_data == NULL) {
			free(pvp->tx_filter_data[data.fir_filter.channel]);
			pvp->tx_filter_data[data.fir_filter.channel] = NULL;	/* disable filter */
		} else if (data.fir_filter.filter_size != (int)pvp->tx_filter_size) {
			error = CUSE_ERR_INVALID;
		} else if (pvp->tx_filter_size != 0) {
			size_t size = sizeof(pvp->tx_filter_data[0][0]) * pvp->tx_filter_size;
			if (pvp->tx_filter_data[data.fir_filter.channel] == NULL) {
				pvp->tx_filter_data[data.fir_filter.channel] = malloc(size);
				if (pvp->tx_filter_data[data.fir_filter.channel] == NULL)
					error = CUSE_ERR_NO_MEMORY;
				else
					memset(pvp->tx_filter_data[data.fir_filter.channel], 0, size);
			}
			if (pvp->tx_filter_data[data.fir_filter.channel] != NULL) {
				error = cuse_copy_in(data.fir_filter.filter_data,
				    pvp->tx_filter_data[data.fir_filter.channel], size);
			}
		}
		break;

	case VIRTUAL_OSS_GET_SAMPLE_RATE:
		data.val = voss_dsp_sample_rate;
		break;

	case VIRTUAL_OSS_GET_SYSTEM_INFO:
		data.sys_info.tx_jitter_up = voss_jitter_up;
		data.sys_info.tx_jitter_down = voss_jitter_down;
		data.sys_info.sample_rate = voss_dsp_sample_rate;
		data.sys_info.sample_bits = voss_dsp_bits;
		data.sys_info.sample_channels = voss_mix_channels;
		strlcpy(data.sys_info.rx_device_name, voss_dsp_rx_device,
		    sizeof(data.sys_info.rx_device_name));
		strlcpy(data.sys_info.tx_device_name, voss_dsp_tx_device,
		    sizeof(data.sys_info.tx_device_name));
		break;

	default:
		error = CUSE_ERR_INVALID;
		break;
	}
	atomic_unlock();

	if (error == 0) {
		if (cmd & IOC_OUT)
			error = cuse_copy_out(&data, peer_data, len);
	}
	return (error);
}

const struct cuse_methods vctl_methods = {
	.cm_open = vctl_open,
	.cm_close = vctl_close,
	.cm_ioctl = vctl_ioctl,
};
