
/*
 * I/O addresses
 */

#define u_Base			(gus_base + 0x000)
#define u_Mixer			u_Base
#define u_Status		(gus_base + 0x006)
#define u_TimerControl		(gus_base + 0x008)
#define u_TimerData		(gus_base + 0x009)
#define u_IRQDMAControl		(gus_base + 0x00b)
#define u_MidiControl		(gus_base + 0x100)
#define 	MIDI_RESET		0x03
#define		MIDI_ENABLE_XMIT	0x20
#define		MIDI_ENABLE_RCV		0x80
#define u_MidiStatus		u_MidiControl
#define		MIDI_RCV_FULL		0x01
#define 	MIDI_XMIT_EMPTY		0x02
#define 	MIDI_FRAME_ERR		0x10
#define 	MIDI_OVERRUN		0x20
#define 	MIDI_IRQ_PEND		0x80
#define u_MidiData		(gus_base + 0x101)
#define u_Voice			(gus_base + 0x102)
#define u_Command		(gus_base + 0x103)
#define u_DataLo		(gus_base + 0x104)
#define u_DataHi		(gus_base + 0x105)
#define u_IrqStatus		u_Status
#	define MIDI_TX_IRQ		0x01	/* pending MIDI xmit IRQ */
#	define MIDI_RX_IRQ		0x02	/* pending MIDI recv IRQ */
#	define GF1_TIMER1_IRQ		0x04	/* general purpose timer */
#	define GF1_TIMER2_IRQ		0x08	/* general purpose timer */
#	define WAVETABLE_IRQ		0x20	/* pending wavetable IRQ */
#	define ENVELOPE_IRQ		0x40	/* pending volume envelope IRQ */
#	define DMA_TC_IRQ		0x80	/* pending dma tc IRQ */
#define u_DRAMIO		(gus_base + 0x107)
