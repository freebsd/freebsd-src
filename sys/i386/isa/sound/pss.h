/******************************************************************************

	def.h

	Version 1.3	11/2/93

	Copyright (c) 1993 Analog Devices Inc. All rights reserved

******************************************************************************/
/* Port offsets from base port for Sound Blaster DSP */
#define DSP_PORT_CMSD0      0x00  /* C/MS music voice 1-6 data port, write only */
#define DSP_PORT_CMSR0      0x01  /* C/MS music voice 1-6 register port, write only */
#define DSP_PORT_CMSD1      0x02  /* C/MS music voice 7-12 data port, write only */
#define DSP_PORT_CMSR1      0x03  /* C/MS music voice 7-12 register port, write only */

#define DSP_PORT_STATUS     0x04  /* DSP Status bits, read only */
#define DSP_PORT_CONTROL    0x04  /* DSP Control bits, write only */
#define DSP_PORT_DATA_LSB   0x05  /* Read or write LSB of 16 bit data */


#define DSP_PORT_RESET      0x06  /* DSP Reset, write only */
#define DSP_PORT_07h        0x07  /* reserved port */

#define DSP_PORT_FMD0       0x08  /* FM music data/status port, read/write  */
#define DSP_PORT_FMR0       0x09  /* FM music data/status port, write only */

#define DSP_PORT_RDDATA     0x0A  /* DSP Read data, read only reading signals DSP */
#define DSP_PORT_0Bh        0x0B  /* reserved port */
#define DSP_PORT_WRDATA     0x0C  /* DSP Write data or command, write */
#define DSP_PORT_WRBUSY     0x0C  /* DSP Write buffer status (bit 7), read */
#define DSP_PORT_0Dh        0x0D  /* reserved port */
#define DSP_PORT_DATAAVAIL  0x0E  /* DSP Data available status (bit 7), read only */
#define DSP_PORT_INTERFACE  0x0E  /* Sets DMA Channel and Interrupt, write only */
#define DSP_PORT_0Fh        0x0F  /* reserved port (used on Pro cards) */

#define ADDR_MASK   0x003f

#define INT_MASK    0xffc7
#define INT_3_BITS  0x0008
#define INT_5_BITS  0x0010
#define INT_7_BITS  0x0018
#define INT_9_BITS  0x0020
#define INT_10_BITS 0x0028
#define INT_11_BITS 0x0030
#define INT_12_BITS 0x0038

#define GAME_BIT      0x0400
#define GAME_BIT_MASK 0xfbff

#define INT_TEST_BIT 0x0200
#define INT_TEST_PASS 0x0100
#define INT_TEST_BIT_MASK 0xFDFF

#define DMA_MASK    0xfff8
#define DMA_0_BITS  0x0001
#define DMA_1_BITS  0x0002
#define DMA_3_BITS  0x0003
#define DMA_5_BITS  0x0004
#define DMA_6_BITS  0x0005
#define DMA_7_BITS  0x0006

#define DMA_TEST_BIT  0x0080
#define DMA_TEST_PASS 0x0040
#define DMA_TEST_BIT_MASK 0xFF7F


/* Echo DSP Flags */

#define DSP_FLAG3     0x10
#define DSP_FLAG2     0x08
#define DSP_FLAG1     0x80
#define DSP_FLAG0     0x40

#define PSS_CONFIG    0x10
#define PSS_WSS_CONFIG    0x12
#define SB_CONFIG     0x14
#define MIDI_CONFIG   0x18
#define CD_CONFIG     0x16
#define UART_CONFIG   0x1a

#define PSS_DATA      0x00
#define PSS_STATUS    0x02
#define PSS_CONTROL   0x02
#define PSS_ID_VERS   0x04

#define PSS_FLAG3     0x0800
#define PSS_FLAG2     0x0400
#define PSS_FLAG1     0x1000
#define PSS_FLAG0     0x0800

/*_____ WSS defines */
#define WSS_BASE_ADDRESS 0x530
#define WSS_CONFIG       0x0
#define WSS_VERSION      0x03
#define WSS_SP0          0x04
#define WSS_SP1          0x05
#define WSS_SP2          0x06
#define WSS_SP3          0x07

/*_____ SoundPort register addresses */

#define SP_LIN_SOURCE_CTRL   0x00
#define SP_RIN_SOURCE_CTRL   0x01
#define SP_LIN_GAIN_CTRL   0x10
#define SP_RIN_GAIN_CTRL   0x11
#define SP_LAUX1_CTRL      0x02
#define SP_RAUX1_CTRL      0x03
#define SP_LAUX2_CTRL      0x04
#define SP_RAUX2_CTRL      0x05
#define SP_LOUT_CTRL       0x06
#define SP_ROUT_CTRL       0x07
#define SP_CLK_FORMAT      0x48
#define SP_INT_CONF        0x09
#define SP_INT_CONF_MCE    0x49
#define SP_PIN_CTRL        0x0a
#define SP_TEST_INIT       0x0b
#define SP_MISC_CTRL       0x0c
#define SP_MIX_CTRL        0x0d
#define SP_DMA_UCNT        0x0e
#define SP_DMA_LCNT        0x0f

/*_____ Gain constants  */

#define GAIN_0      0x00
#define GAIN_1_5    0x01
#define GAIN_3      0x02
#define GAIN_4_5    0x03
#define GAIN_6      0x04
#define GAIN_7_5    0x05
#define GAIN_9      0x06
#define GAIN_10_5   0x07
#define GAIN_12     0x08
#define GAIN_13_5   0x09
#define GAIN_15     0x0a
#define GAIN_16_5   0x0b
#define GAIN_18     0x0c
#define GAIN_19_5   0x0d
#define GAIN_21     0x0e
#define GAIN_22_5   0x0f
#define MUTE       0XFFFF

/*_____ Attenuation constants  */

#define ATTEN_0      0x00
#define ATTEN_1_5    0x01
#define ATTEN_3      0x02
#define ATTEN_4_5    0x03
#define ATTEN_6      0x04
#define ATTEN_7_5    0x05
#define ATTEN_9      0x06
#define ATTEN_10_5   0x07
#define ATTEN_12     0x08
#define ATTEN_13_5   0x09
#define ATTEN_15     0x0a
#define ATTEN_16_5   0x0b
#define ATTEN_18     0x0c
#define ATTEN_19_5   0x0d
#define ATTEN_21     0x0e
#define ATTEN_22_5   0x0f


#define PSS_WRITE_EMPTY 0x8000

#define CD_POL_MASK 0xFFBF
#define CD_POL_BIT  0x0040



/******************************************************************************

	host.h

	Version 1.2	9/27/93

	Copyright (c) 1993 Analog Devices Inc. All rights reserved

******************************************************************************/
#define SB_WRITE_FULL    0x80
#define SB_READ_FULL     0x80
#define SB_WRITE_STATUS  0x0C
#define SB_READ_STATUS   0x0E
#define SB_READ_DATA     0x0A
#define SB_WRITE_DATA    0x0C

#define PSS_DATA_REG     0x00
#define PSS_STATUS_REG   0x02
#define PSS_WRITE_EMPTY  0x8000
#define PSS_READ_FULL    0x4000

/*_____ 1848 Sound Port bit defines */

#define SP_IN_INIT            0x80
#define MODE_CHANGE_ENABLE    0x40
#define MODE_CHANGE_MASK      0xbf
#define TRANSFER_DISABLE      0x20
#define TRANSFER_DISABLE_MASK 0xdf
#define ADDRESS_MASK          0xf0

/*_____ Status bits */
#define INTERRUPT_STATUS      0x01
#define PLAYBACK_READY        0x02
#define PLAYBACK_LEFT         0x04
/*_____ pbright is not left */
#define PLAYBACK_UPPER        0x08
/*_____ bplower is not upper */

#define SAMPLE_OVERRUN        0x10
#define SAMPLE_UNDERRUN       0x10
#define CAPTURE_READY         0x20
#define CAPTURE_LEFT          0x40
/*_____ cpright is not left */
#define CAPTURE_UPPER         0x08
/*_____ cplower is not upper */

/*_____ Input & Output regs bits */
#define LINE_INPUT            0x80
#define AUX_INPUT             0x40
#define MIC_INPUT             0x80
#define MIXED_DAC_INPUT       0xC0
#define INPUT_GAIN_MASK       0xf0
#define INPUT_MIC_GAIN_ENABLE 0x20
#define INPUT_MIC_GAIN_MASK   0xdf
#define INPUT_SOURCE_MASK     0x3f
#define AUX_INPUT_ATTEN_MASK  0xf0
#define AUX_INPUT_MUTE        0x80
#define AUX_INPUT_MUTE_MASK   0x7f
#define OUTPUT_MUTE           0x80
#define OUTPUT_MUTE_MASK      0x7f
#define OUTPUT_ATTEN_MASK     0xc0

/*_____ Clock and Data format reg bits */
#define CLOCK_SELECT_MASK     0xfe
#define CLOCK_XTAL2           0x01
#define CLOCK_XTAL1           0x00
#define CLOCK_FREQ_MASK       0xf1
#define STEREO_MONO_MASK      0xef
#define STEREO                0x10
#define AUDIO_MONO            0x00
#define LINEAR_COMP_MASK      0xdf
#define LINEAR                0x00
#define COMPANDED             0x20
#define FORMAT_MASK           0xbf
#define PCM                   0x00
#define ULAW                  0x00
#define TWOS_COMP             0x40
#define ALAW                  0x40

/*_____ Interface Configuration reg bits */
#define PLAYBACK_ENABLE       0x01
#define PLAYBACK_ENABLE_MASK  0xfe
#define CAPTURE_ENABLE        0x02
#define CAPTURE_ENABLE_MASK   0xfd
#define SINGLE_DMA            0x04
#define SINGLE_DMA_MASK       0xfb
#define DUAL_DMA              0x00
#define AUTO_CAL_ENABLE       0x08
#define AUTO_CAL_DISABLE_MASK 0xf7
#define PLAYBACK_PIO_ENABLE   0x40
#define PLAYBACK_DMA_MASK     0xbf
#define CAPTURE_PIO_ENABLE    0x80
#define CAPTURE_DMA_MASK      0x7f

/*_____ Pin control bits */
#define INTERRUPT_ENABLE      0x02
#define INTERRUPT_MASK        0xfd

/*_____ Test and init reg bits */
#define OVERRANGE_LEFT_MASK   0xfc
#define OVERRANGE_RIGHT_MASK  0xf3
#define DATA_REQUEST_STATUS   0x10
#define AUTO_CAL_IN_PROG      0x20
#define PLAYBACK_UNDERRUN     0x40
#define CAPTURE_UNDERRUN      0x80

/*_____ Miscellaneous Control reg bits */
#define ID_MASK               0xf0

/*_____ Digital Mix Control reg bits */
#define DIGITAL_MIX1_MUTE_MASK 0xfe
#define MIX_ATTEN_MASK         0x03

/*_____ 1848 Sound Port reg defines */

#define SP_LEFT_INPUT_CONTROL    0x0
#define SP_RIGHT_INPUT_CONTROL   0x1
#define SP_LEFT_AUX1_CONTROL     0x2
#define SP_RIGHT_AUX1_CONTROL    0x3
#define SP_LEFT_AUX2_CONTROL     0x4
#define SP_RIGHT_AUX2_CONTROL    0x5
#define SP_LEFT_OUTPUT_CONTROL   0x6
#define SP_RIGHT_OUTPUT_CONTROL  0x7
#define SP_CLOCK_DATA_FORMAT     0x8
#define SP_INTERFACE_CONFIG      0x9
#define SP_PIN_CONTROL           0xA
#define SP_TEST_AND_INIT         0xB
#define SP_MISC_INFO             0xC
#define SP_DIGITAL_MIX           0xD
#define SP_UPPER_BASE_COUNT      0xE
#define SP_LOWER_BASE_COUNT      0xF

#define HOST_SP_ADDR (0x534)
#define HOST_SP_DATA (0x535)


/******************************************************************************

	phillips.h

	Version 1.2	9/27/93

	Copyright (c) 1993 Analog Devices Inc. All rights reserved

******************************************************************************/
/*_____ Phillips control SW defines */

/*_____ Settings and ranges */
#define VOLUME_MAX   6
#define VOLUME_MIN (-64)
#define VOLUME_RANGE 70
#define VOLUME_STEP  2
#define BASS_MAX    15
#define BASS_MIN   (-12)
#define BASS_STEP    2
#define BASS_RANGE 27
#define TREBLE_MAX  12
#define TREBLE_MIN (-12)
#define TREBLE_STEP  2
#define TREBLE_RANGE 24

#define VOLUME_CONSTANT 252
#define BASS_CONSTANT   246
#define TREBLE_CONSTANT 246

/*_____ Software commands */
#define SET_MASTER_COMMAND  0x0010
#define MASTER_VOLUME_LEFT  0x0000
#define MASTER_VOLUME_RIGHT 0x0100
#define MASTER_BASS         0x0200
#define MASTER_TREBLE       0x0300
#define MASTER_SWITCH       0x0800

#define STEREO_MODE  0x00ce
#define PSEUDO_MODE  0x00d6
#define SPATIAL_MODE 0x00de
#define MONO_MODE    0x00c6


#define PSS_STEREO           0x00ce
#define PSS_PSEUDO           0x00d6
#define PSS_SPATIAL          0x00de
#define PSS_MONO             0x00c6

#define PHILLIPS_VOL_MIN          -64
#define PHILLIPS_VOL_MAX            6
#define PHILLIPS_VOL_DELTA         70
#define PHILLIPS_VOL_INITIAL      -20
#define PHILLIPS_VOL_CONSTANT     252
#define PHILLIPS_VOL_STEP           2
#define PHILLIPS_BASS_MIN         -12
#define PHILLIPS_BASS_MAX          15
#define PHILLIPS_BASS_DELTA        27
#define PHILLIPS_BASS_INITIAL       0
#define PHILLIPS_BASS_CONSTANT    246
#define PHILLIPS_BASS_STEP          2
#define PHILLIPS_TREBLE_MIN       -12
#define PHILLIPS_TREBLE_MAX        12
#define PHILLIPS_TREBLE_DELTA      24
#define PHILLIPS_TREBLE_INITIAL     0
#define PHILLIPS_TREBLE_CONSTANT  246
#define PHILLIPS_TREBLE_STEP        2

