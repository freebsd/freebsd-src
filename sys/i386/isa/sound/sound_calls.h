/*
 *	DMA buffer calls
 */

int DMAbuf_open(int dev, int mode);
int DMAbuf_release(int dev, int mode);
int DMAbuf_read (int dev, snd_rw_buf *user_buf, int count);
int DMAbuf_getwrbuffer(int dev, char **buf, int *size);
int DMAbuf_getrdbuffer(int dev, char **buf, int *len);
int DMAbuf_rmchars(int dev, int buff_no, int c);
int DMAbuf_start_output(int dev, int buff_no, int l);
int DMAbuf_ioctl(int dev, unsigned int cmd, unsigned int arg, int local);
long DMAbuf_init(long mem_start);
int DMAbuf_start_dma (int dev, unsigned long physaddr, int count, int dma_mode);
int DMAbuf_open_dma (int chan);
void DMAbuf_close_dma (int chan);
void DMAbuf_reset_dma (int chan);
void DMAbuf_inputintr(int dev);
void DMAbuf_outputintr(int dev);

/*
 *	System calls for the /dev/dsp
 */

int dsp_read (int dev, struct fileinfo *file, snd_rw_buf *buf, int count);
int dsp_write (int dev, struct fileinfo *file, snd_rw_buf *buf, int count);
int dsp_open (int dev, struct fileinfo *file, int bits);
void dsp_release (int dev, struct fileinfo *file);
int dsp_ioctl (int dev, struct fileinfo *file,
	   unsigned int cmd, unsigned int arg);
int dsp_lseek (int dev, struct fileinfo *file, off_t offset, int orig);
long dsp_init (long mem_start);

/*
 *	System calls for the /dev/audio
 */

int audio_read (int dev, struct fileinfo *file, snd_rw_buf *buf, int count);
int audio_write (int dev, struct fileinfo *file, snd_rw_buf *buf, int count);
int audio_open (int dev, struct fileinfo *file);
void audio_release (int dev, struct fileinfo *file);
int audio_ioctl (int dev, struct fileinfo *file,
	   unsigned int cmd, unsigned int arg);
int audio_lseek (int dev, struct fileinfo *file, off_t offset, int orig);
long audio_init (long mem_start);

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

/* 	From pro_midi.c 	*/

long pro_midi_attach(long mem_start);
int  pro_midi_open(int dev, int mode);
void pro_midi_close(int dev);
int pro_midi_write(int dev, snd_rw_buf *uio);
int pro_midi_read(int dev, snd_rw_buf *uio);

/*	From soundcard.c	*/
long soundcard_init(long mem_start);
void tenmicrosec(void);
void request_sound_timer (int count);
void sound_stop_timer(void);
int snd_ioctl_return(int *addr, int value);

/*	From sb_dsp.c	*/
int sb_dsp_detect (struct address_info *hw_config);
long sb_dsp_init (long mem_start, struct address_info *hw_config);
void sb_dsp_disable_midi(void);

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
void gusintr(int);

/*	From gus_wave.c */
int gus_wave_detect(int baseaddr);
long gus_wave_init(long mem_start, int irq, int dma);
void gus_voice_irq(void);
unsigned char gus_read8 (int reg);
void gus_write8(int reg, unsigned char data);
void guswave_dma_irq(void);
void gus_delay(void);

/*	From gus_midi.c */
long gus_midi_init(long mem_start);
void gus_midi_interrupt(int dummy);

/*	From mpu401.c */
long attach_mpu401(long mem_start, struct address_info * hw_config);
int probe_mpu401(struct address_info *hw_config);

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
