/*
 *	DMA buffer calls
 */

int DMAbuf_open(int dev, int mode);
int DMAbuf_release(int dev, int mode);
int DMAbuf_getwrbuffer(int dev, char **buf, int *size, int dontblock);
int DMAbuf_getrdbuffer(int dev, char **buf, int *len, int dontblock);
int DMAbuf_rmchars(int dev, int buff_no, int c);
int DMAbuf_start_output(int dev, int buff_no, int l);
int DMAbuf_ioctl(int dev, u_int cmd, ioctl_arg arg, int local);
void DMAbuf_init(void);
int DMAbuf_start_dma (int dev, u_long physaddr, int count, int dma_mode);
int DMAbuf_open_dma (int dev);
void DMAbuf_close_dma (int dev);
void DMAbuf_reset_dma (int dev);
void DMAbuf_inputintr(int dev);
void DMAbuf_outputintr(int dev, int underflow_flag);
void DMAbuf_start_devices(u_int devmask);
#ifdef ALLOW_SELECT
int DMAbuf_select(int dev, struct fileinfo *file, int sel_type, select_table * wait);
#endif

/*
 *	System calls for /dev/dsp and /dev/audio
 */

int audio_read (int dev, struct fileinfo *file, snd_rw_buf *buf, int count);
int audio_write (int dev, struct fileinfo *file, snd_rw_buf *buf, int count);
int audio_open (int dev, struct fileinfo *file);
void audio_release (int dev, struct fileinfo *file);
int audio_ioctl (int dev, struct fileinfo *file,
	   u_int cmd, ioctl_arg arg);
int audio_lseek (int dev, struct fileinfo *file, off_t offset, int orig);
/* long audio_init (void); */

#ifdef ALLOW_SELECT
int  audio_poll(int dev, struct fileinfo *file, int events, select_table * wait);
#endif

/*
 *	System calls for the /dev/sequencer
 */

int sequencer_read (int dev, struct fileinfo *file, snd_rw_buf *buf, int count);
int sequencer_write (int dev, struct fileinfo *file, snd_rw_buf *buf, int count);
int sequencer_open (int dev, struct fileinfo *file);
void sequencer_release (int dev, struct fileinfo *file);
int sequencer_ioctl (int dev, struct fileinfo *file,
	   u_int cmd, ioctl_arg arg);
int sequencer_lseek (int dev, struct fileinfo *file, off_t offset, int orig);
void sequencer_init (void);
void sequencer_timer(void  *dummy);
int note_to_freq(int note_num);
u_long compute_finetune(u_long base_freq, int bend, int range);
void seq_input_event(u_char *event, int len);
void seq_copy_to_input (u_char *event, int len);


/*
 *	System calls for the /dev/midi
 */

int MIDIbuf_read (int dev, struct fileinfo *file, snd_rw_buf *buf, int count);
int MIDIbuf_write (int dev, struct fileinfo *file, snd_rw_buf *buf, int count);
int MIDIbuf_open (int dev, struct fileinfo *file);
void MIDIbuf_release (int dev, struct fileinfo *file);
int MIDIbuf_ioctl (int dev, struct fileinfo *file,
	   u_int cmd, ioctl_arg arg);
int MIDIbuf_lseek (int dev, struct fileinfo *file, off_t offset, int orig);
void MIDIbuf_bytes_received(int dev, u_char *buf, int count);

/*
 *
 *	Misc calls from various sources
 */

/*	From soundcard.c	*/
void soundcard_init(void);
void tenmicrosec(int);
void request_sound_timer (int count);
void sound_stop_timer(void);
int snd_ioctl_return(int *addr, int value);
int snd_set_irq_handler (int int_lvl, void(*hndlr)(int), sound_os_info *osp);
void sound_dma_malloc(int dev);
void sound_dma_free(int dev);
void conf_printf(char *name, struct address_info *hw_config);
void conf_printf2(char *name, int base, int irq, int dma, int dma2);

/*	From sound_switch.c	*/
int sound_read_sw (int dev, struct fileinfo *file, snd_rw_buf *buf, int count);
int sound_write_sw (int dev, struct fileinfo *file, snd_rw_buf *buf, int count);
int sound_open_sw (int dev, struct fileinfo *file);
void sound_release_sw (int dev, struct fileinfo *file);
int sound_ioctl_sw (int dev, struct fileinfo *file, u_int cmd, ioctl_arg arg);

/*	From sb_dsp.c	*/
int sb_dsp_detect (struct address_info *hw_config);
void sb_dsp_init (struct address_info *hw_config);
void sb_dsp_disable_midi(void);
int sb_dsp_command (u_char val);
int sb_reset_dsp (void);

/*	From sb16_dsp.c	*/
void sb16_dsp_interrupt (int irq);
void sb16_dsp_init(struct address_info *hw_config);
int sb16_dsp_detect(struct address_info *hw_config);

/*	From sb16_midi.c	*/
void sb16midiintr (int unit);
void attach_sb16midi(struct address_info * hw_config);
int probe_sb16midi(struct address_info *hw_config);
void sb_midi_interrupt(int dummy);

/*	From sb_midi.c	*/
void sb_midi_init(int model);

/*	From sb_mixer.c	*/
void sb_setmixer (u_int port, u_int value);
int sb_getmixer (u_int port);
void sb_mixer_set_stereo(int mode);
int sb_mixer_init(int major_model);

/*	From opl3.c	*/
int opl3_detect (int ioaddr, sound_os_info *osp);
void opl3_init(int ioaddr, sound_os_info *osp);

/*	From sb_card.c	*/
void attach_sb_card(struct address_info *hw_config);
int probe_sb(struct address_info *hw_config);

/*  From awe_wave.c  */
void  attach_awe_obsolete(struct address_info *hw_config);
int probe_awe_obsolete(struct address_info *hw_config);

/*	From adlib_card.c	*/
void attach_adlib_card(struct address_info *hw_config);
int probe_adlib(struct address_info *hw_config);

/*	From pas_card.c	*/
void attach_pas_card(struct address_info *hw_config);
int probe_pas(struct address_info *hw_config);
int pas_set_intr(int mask);
int pas_remove_intr(int mask);
u_char pas_read(int ioaddr);
void pas_write(u_char data, int ioaddr);

/*	From pas_audio.c */
void pas_pcm_interrupt(u_char status, int cause);
void pas_pcm_init(struct address_info *hw_config);

/*	From pas_mixer.c */
int pas_init_mixer(void);

/*	From pas_midi.c */
void pas_midi_init(void);
void pas_midi_interrupt(void);

/*	From gus_card.c */
void attach_gus_card(struct address_info * hw_config);
int probe_gus(struct address_info *hw_config);
int gus_set_midi_irq(int num);
/*void gusintr(int irq); */
void attach_gus_db16(struct address_info * hw_config);
int probe_gus_db16(struct address_info *hw_config);

/*	From gus_wave.c */
int gus_wave_detect(int baseaddr);
void gus_wave_init(struct address_info *hw_config);
void gus_voice_irq(void);
u_char gus_read8 (int reg);
void gus_write8(int reg, u_int data);
void guswave_dma_irq(void);
void gus_delay(void);
int gus_default_mixer_ioctl (int dev, u_int cmd, ioctl_arg arg);
void gus_timer_command (u_int addr, u_int val);

/*	From gus_midi.c */
void gus_midi_init(void);
void gus_midi_interrupt(int dummy);

/*	From mpu401.c */
void attach_mpu401(struct address_info * hw_config);
int probe_mpu401(struct address_info *hw_config);
void mpuintr(int irq);

/*	From uart6850.c */
void attach_uart6850(struct address_info * hw_config);
int probe_uart6850(struct address_info *hw_config);

/*	From opl3.c */
void enable_opl3_mode(int left, int right, int both);

/*	From patmgr.c */
int pmgr_open(int dev);
void pmgr_release(int dev);
int pmgr_read (int dev, struct fileinfo *file, snd_rw_buf * buf, int count);
int pmgr_write (int dev, struct fileinfo *file, snd_rw_buf * buf, int count);
int pmgr_access(int dev, struct patmgr_info *rec);
int pmgr_inform(int dev, int event, u_long parm1, u_long parm2,
				    u_long parm3, u_long parm4);

/* 	From ics2101.c */
void ics2101_mixer_init(void);

/*	From sound_timer.c */
void sound_timer_interrupt(void);
void sound_timer_syncinterval(u_int new_usecs);

/*	From ad1848.c */
void ad1848_init (char *name, int io_base, int irq, int dma_playback, int dma_capture, int share_dma, sound_os_info *osp);

int ad1848_detect (int io_base, int *flags, sound_os_info *osp);
#define AD_F_CS4231	0x0001	/* Returned if a CS4232 (or compatible) detected */
#define AD_F_CS4248	0x0001	/* Returned if a CS4248 (or compatible) detected */

void     ad1848_interrupt (int irq);
void attach_mss(struct address_info * hw_config);
int probe_mss(struct address_info *hw_config);
void attach_pnp_ad1848(struct address_info * hw_config);
int probe_pnp_ad1848(struct address_info *hw_config);

/* 	From pss.c */
int probe_pss (struct address_info *hw_config);
void attach_pss (struct address_info *hw_config);
int probe_pss_mpu (struct address_info *hw_config);
void attach_pss_mpu (struct address_info *hw_config);
int probe_pss_mss (struct address_info *hw_config);
void attach_pss_mss (struct address_info *hw_config);

/* 	From sscape.c */
int probe_sscape (struct address_info *hw_config);
void attach_sscape (struct address_info *hw_config);
int probe_ss_mss(struct address_info *hw_config);
void attach_ss_mss(struct address_info * hw_config);

int pss_read (int dev, struct fileinfo *file, snd_rw_buf *buf, int count);
int pss_write (int dev, struct fileinfo *file, snd_rw_buf *buf, int count);
int pss_open (int dev, struct fileinfo *file);
void pss_release (int dev, struct fileinfo *file);
int pss_ioctl (int dev, struct fileinfo *file,
	   u_int cmd, ioctl_arg arg);
int pss_lseek (int dev, struct fileinfo *file, off_t offset, int orig);
void pss_init(void);

/* From aedsp16.c */
int InitAEDSP16_SBPRO(struct address_info *hw_config);
int InitAEDSP16_MSS(struct address_info *hw_config);
int InitAEDSP16_MPU401(struct address_info *hw_config);

/*	From midi_synth.c	*/
void do_midi_msg (int synthno, u_char *msg, int mlen);

/*	From trix.c	*/
void attach_trix_wss (struct address_info *hw_config);
int probe_trix_wss (struct address_info *hw_config);
void attach_trix_sb (struct address_info *hw_config);
int probe_trix_sb (struct address_info *hw_config);
void attach_trix_mpu (struct address_info *hw_config);
int probe_trix_mpu (struct address_info *hw_config);

/*	From mad16.c	*/
void attach_mad16 (struct address_info *hw_config);
int probe_mad16 (struct address_info *hw_config);
void attach_mad16_mpu (struct address_info *hw_config);
int probe_mad16_mpu (struct address_info *hw_config);
int mad16_sb_dsp_detect (struct address_info *hw_config);
void mad16_sb_dsp_init (struct address_info *hw_config);

/* From cs4232.c */

int probe_cs4232 (struct address_info *hw_config);
void attach_cs4232 (struct address_info *hw_config);
int probe_cs4232_mpu (struct address_info *hw_config);
void attach_cs4232_mpu (struct address_info *hw_config);

/*	From maui.c */
void attach_maui(struct address_info * hw_config);
int probe_maui(struct address_info *hw_config);

/*	From sound_pnp.c */
void sound_pnp_init(void);
void sound_pnp_disconnect(void);
