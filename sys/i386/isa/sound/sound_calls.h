/*
 *	DMA buffer calls
 */

int DMAbuf_open(int dev, int mode);
int DMAbuf_release(int dev, int mode);
int DMAbuf_getwrbuffer(int dev, char **buf, int *size, int dontblock);
int DMAbuf_getrdbuffer(int dev, char **buf, int *len, int dontblock);
int DMAbuf_rmchars(int dev, int buff_no, int c);
int DMAbuf_start_output(int dev, int buff_no, int l);
int DMAbuf_ioctl(int dev, unsigned int cmd, unsigned int arg, int local);
long DMAbuf_init(long mem_start);
int DMAbuf_start_dma (int dev, unsigned long physaddr, int count, int dma_mode);
int DMAbuf_open_dma (int dev);
void DMAbuf_close_dma (int dev);
void DMAbuf_reset_dma (int dev);
void DMAbuf_inputintr(int dev);
void DMAbuf_outputintr(int dev, int underflow_flag);
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
	   unsigned int cmd, unsigned int arg);
int audio_lseek (int dev, struct fileinfo *file, off_t offset, int orig);
long audio_init (long mem_start);

#ifdef ALLOW_SELECT
int audio_select(int dev, struct fileinfo *file, int sel_type, select_table * wait);
#endif

/*
 *	System calls for the /dev/sequencer
 */

int sequencer_read (int dev, struct fileinfo *file, snd_rw_buf *buf, int count);
int sequencer_write (int dev, struct fileinfo *file, snd_rw_buf *buf, int count);
int sequencer_open (int dev, struct fileinfo *file);
void sequencer_release (int dev, struct fileinfo *file);
int sequencer_ioctl (int dev, struct fileinfo *file,
	   unsigned int cmd, unsigned int arg);
int sequencer_lseek (int dev, struct fileinfo *file, off_t offset, int orig);
long sequencer_init (long mem_start);
void sequencer_timer(void);
int note_to_freq(int note_num);
unsigned long compute_finetune(unsigned long base_freq, int bend, int range);
void seq_input_event(unsigned char *event, int len);
void seq_copy_to_input (unsigned char *event, int len);

#ifdef ALLOW_SELECT
int sequencer_select(int dev, struct fileinfo *file, int sel_type, select_table * wait);
#endif

/*
 *	System calls for the /dev/midi
 */

int MIDIbuf_read (int dev, struct fileinfo *file, snd_rw_buf *buf, int count);
int MIDIbuf_write (int dev, struct fileinfo *file, snd_rw_buf *buf, int count);
int MIDIbuf_open (int dev, struct fileinfo *file);
void MIDIbuf_release (int dev, struct fileinfo *file);
int MIDIbuf_ioctl (int dev, struct fileinfo *file,
	   unsigned int cmd, unsigned int arg);
int MIDIbuf_lseek (int dev, struct fileinfo *file, off_t offset, int orig);
void MIDIbuf_bytes_received(int dev, unsigned char *buf, int count);
long MIDIbuf_init(long mem_start);

#ifdef ALLOW_SELECT
int MIDIbuf_select(int dev, struct fileinfo *file, int sel_type, select_table * wait);
#endif

/*
 *	System calls for the generic midi interface.
 *
 */

long  CMIDI_init  (long mem_start);
int   CMIDI_open  (int dev, struct fileinfo *file);
int   CMIDI_write (int dev, struct fileinfo *file, snd_rw_buf *buf, int count);
int   CMIDI_read  (int dev, struct fileinfo *file, snd_rw_buf *buf, int count);
int   CMIDI_close (int dev, struct fileinfo *file); 

/*
 *
 *	Misc calls from various sources
 */

/*	From soundcard.c	*/
long soundcard_init(long mem_start);
void tenmicrosec(void);
void request_sound_timer (int count);
void sound_stop_timer(void);
int snd_ioctl_return(int *addr, int value);
int snd_set_irq_handler (int interrupt_level, INT_HANDLER_PROTO(), char *name);
void snd_release_irq(int vect);
void sound_dma_malloc(int dev);
void sound_dma_free(int dev);

/*	From sound_switch.c	*/
int sound_read_sw (int dev, struct fileinfo *file, snd_rw_buf *buf, int count);
int sound_write_sw (int dev, struct fileinfo *file, snd_rw_buf *buf, int count);
int sound_open_sw (int dev, struct fileinfo *file);
void sound_release_sw (int dev, struct fileinfo *file);
int sound_ioctl_sw (int dev, struct fileinfo *file,
	     unsigned int cmd, unsigned long arg);

/*	From sb_dsp.c	*/
int sb_dsp_detect (struct address_info *hw_config);
long sb_dsp_init (long mem_start, struct address_info *hw_config);
void sb_dsp_disable_midi(void);
int sb_get_irq(void);
void sb_free_irq(void);
int sb_dsp_command (unsigned char val);
int sb_reset_dsp (void);

/*	From sb16_dsp.c	*/
void sb16_dsp_interrupt (int irq);
long sb16_dsp_init(long mem_start, struct address_info *hw_config);
int sb16_dsp_detect(struct address_info *hw_config);

/*	From sb16_midi.c	*/
void sb16midiintr (int unit);
long attach_sb16midi(long mem_start, struct address_info * hw_config);
int probe_sb16midi(struct address_info *hw_config);
void sb_midi_interrupt(int dummy);

/*	From sb_midi.c	*/
void sb_midi_init(int model);

/*	From sb_mixer.c	*/
void sb_setmixer (unsigned int port, unsigned int value);
int sb_getmixer (unsigned int port);
void sb_mixer_set_stereo(int mode);
int sb_mixer_init(int major_model);

/*	From opl3.c	*/
int opl3_detect (int ioaddr);
long opl3_init(long mem_start);

/*	From sb_card.c	*/
long attach_sb_card(long mem_start, struct address_info *hw_config);
int probe_sb(struct address_info *hw_config);

/*	From adlib_card.c	*/
long attach_adlib_card(long mem_start, struct address_info *hw_config);
int probe_adlib(struct address_info *hw_config);

/*	From pas_card.c	*/
long attach_pas_card(long mem_start, struct address_info *hw_config);
int probe_pas(struct address_info *hw_config);
int pas_set_intr(int mask);
int pas_remove_intr(int mask);
unsigned char pas_read(int ioaddr);
void pas_write(unsigned char data, int ioaddr);

/*	From pas_audio.c */
void pas_pcm_interrupt(unsigned char status, int cause);
long pas_pcm_init(long mem_start, struct address_info *hw_config);

/*	From pas_mixer.c */
int pas_init_mixer(void);

/*	From pas_midi.c */
long pas_midi_init(long mem_start);
void pas_midi_interrupt(void);

/*	From gus_card.c */
long attach_gus_card(long mem_start, struct address_info * hw_config);
int probe_gus(struct address_info *hw_config);
int gus_set_midi_irq(int num);
void gusintr(INT_HANDLER_PARMS(irq, dummy));
long attach_gus_db16(long mem_start, struct address_info * hw_config);
int probe_gus_db16(struct address_info *hw_config);

/*	From gus_wave.c */
int gus_wave_detect(int baseaddr);
long gus_wave_init(long mem_start, int irq, int dma, int dma_read);
void gus_voice_irq(void);
unsigned char gus_read8 (int reg);
void gus_write8(int reg, unsigned int data);
void guswave_dma_irq(void);
void gus_delay(void);
int gus_default_mixer_ioctl (int dev, unsigned int cmd, unsigned int arg);

/*	From gus_midi.c */
long gus_midi_init(long mem_start);
void gus_midi_interrupt(int dummy);

/*	From mpu401.c */
long attach_mpu401(long mem_start, struct address_info * hw_config);
int probe_mpu401(struct address_info *hw_config);
void mpuintr(INT_HANDLER_PARMS(irq, dummy));

/*	From uart6850.c */
long attach_uart6850(long mem_start, struct address_info * hw_config);
int probe_uart6850(struct address_info *hw_config);

/*	From opl3.c */
void enable_opl3_mode(int left, int right, int both);

/*	From patmgr.c */
int pmgr_open(int dev);
void pmgr_release(int dev);
int pmgr_read (int dev, struct fileinfo *file, snd_rw_buf * buf, int count);
int pmgr_write (int dev, struct fileinfo *file, snd_rw_buf * buf, int count);
int pmgr_access(int dev, struct patmgr_info *rec);
int pmgr_inform(int dev, int event, unsigned long parm1, unsigned long parm2,
				    unsigned long parm3, unsigned long parm4);

/* 	From ics2101.c */
long ics2101_mixer_init(long mem_start);

/*	From sound_timer.c */
void sound_timer_init(int io_base);
void sound_timer_interrupt(void);

/*	From ad1848.c */
void ad1848_init (char *name, int io_base, int irq, int dma_playback, int dma_capture);
int ad1848_detect (int io_base);
void     ad1848_interrupt (INT_HANDLER_PARMS(irq, dummy));
long attach_ms_sound(long mem_start, struct address_info * hw_config);
int probe_ms_sound(struct address_info *hw_config);

/* 	From pss.c */
int probe_pss (struct address_info *hw_config);
long attach_pss (long mem_start, struct address_info *hw_config);
int probe_pss_mpu (struct address_info *hw_config);
long attach_pss_mpu (long mem_start, struct address_info *hw_config);
int probe_pss_mss (struct address_info *hw_config);
long attach_pss_mss (long mem_start, struct address_info *hw_config);

/* 	From sscape.c */
int probe_sscape (struct address_info *hw_config);
long attach_sscape (long mem_start, struct address_info *hw_config);
int probe_ss_ms_sound (struct address_info *hw_config);
long attach_ss_ms_sound(long mem_start, struct address_info * hw_config);

int pss_read (int dev, struct fileinfo *file, snd_rw_buf *buf, int count);
int pss_write (int dev, struct fileinfo *file, snd_rw_buf *buf, int count);
int pss_open (int dev, struct fileinfo *file);
void pss_release (int dev, struct fileinfo *file);
int pss_ioctl (int dev, struct fileinfo *file,
	   unsigned int cmd, unsigned int arg);
int pss_lseek (int dev, struct fileinfo *file, off_t offset, int orig);
long pss_init(long mem_start);

/* From aedsp16.c */
int InitAEDSP16_SBPRO(struct address_info *hw_config);
int InitAEDSP16_MSS(struct address_info *hw_config);
int InitAEDSP16_MPU401(struct address_info *hw_config);

/*	From midi_synth.c	*/
void do_midi_msg (int synthno, unsigned char *msg, int mlen);

/*	From trix.c	*/
long attach_trix_wss (long mem_start, struct address_info *hw_config);
int probe_trix_wss (struct address_info *hw_config);
long attach_trix_sb (long mem_start, struct address_info *hw_config);
int probe_trix_sb (struct address_info *hw_config);
long attach_trix_mpu (long mem_start, struct address_info *hw_config);
int probe_trix_mpu (struct address_info *hw_config);
