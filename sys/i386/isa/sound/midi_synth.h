int midi_synth_ioctl (int dev,
	    unsigned int cmd, ioctl_arg arg);
int midi_synth_kill_note (int dev, int channel, int note, int velocity);
int midi_synth_set_instr (int dev, int channel, int instr_no);
int midi_synth_start_note (int dev, int channel, int note, int volume);
void midi_synth_reset (int dev);
int midi_synth_open (int dev, int mode);
void midi_synth_close (int dev);
void midi_synth_hw_control (int dev, unsigned char *event);
int midi_synth_load_patch (int dev, int format, snd_rw_buf * addr,
		 int offs, int count, int pmgr_flag);
void midi_synth_panning (int dev, int channel, int pressure);
void midi_synth_aftertouch (int dev, int channel, int pressure);
void midi_synth_controller (int dev, int channel, int ctrl_num, int value);
int midi_synth_patchmgr (int dev, struct patmgr_info *rec);
void midi_synth_bender (int dev, int chn, int value);
void midi_synth_setup_voice (int dev, int voice, int chn);
int midi_synth_send_sysex(int dev, unsigned char *bytes,int len);

#ifndef _MIDI_SYNTH_C_
static struct synth_info std_synth_info =
{MIDI_SYNTH_NAME, 0, SYNTH_TYPE_MIDI, 0, 0, 128, 0, 128, MIDI_SYNTH_CAPS};

static struct synth_operations std_midi_synth =
{
  &std_synth_info,
  0,
  SYNTH_TYPE_MIDI,
  0,
  midi_synth_open,
  midi_synth_close,
  midi_synth_ioctl,
  midi_synth_kill_note,
  midi_synth_start_note,
  midi_synth_set_instr,
  midi_synth_reset,
  midi_synth_hw_control,
  midi_synth_load_patch,
  midi_synth_aftertouch,
  midi_synth_controller,
  midi_synth_panning,
  NULL,
  midi_synth_patchmgr,
  midi_synth_bender,
  NULL,	/* alloc_voice */
  midi_synth_setup_voice,
  midi_synth_send_sysex
};
#endif
