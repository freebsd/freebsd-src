#ifndef _LINUX_BUSMOUSE_H
#define _LINUX_BUSMOUSE_H

/*
 * linux/include/linux/busmouse.h: header file for Logitech Bus Mouse driver
 * by James Banks
 *
 * based on information gleamed from various mouse drivers on the net
 *
 * Heavily modified by David giller (rafetmad@oxy.edu)
 *
 * Minor modifications for Linux 0.96c-pl1 by Nathan Laredo
 * gt7080a@prism.gatech.edu (13JUL92)
 *
 * Microsoft BusMouse support by Teemu Rantanen (tvr@cs.hut.fi) (02AUG92)
 *
 * Microsoft Bus Mouse support modified by Derrick Cole (cole@concert.net)
 *    8/28/92
 *
 * Microsoft Bus Mouse support folded into 0.97pl4 code
 *    by Peter Cervasio (pete%q106fm.uucp@wupost.wustl.edu) (08SEP92)
 * Changes:  Logitech and Microsoft support in the same kernel.
 *           Defined new constants in busmouse.h for MS mice.
 *           Added int mse_busmouse_type to distinguish busmouse types
 *           Added a couple of new functions to handle differences in using
 *             MS vs. Logitech (where the int variable wasn't appropriate).
 *
 */

#define MOUSE_IRQ		5
#define LOGITECH_BUSMOUSE       0   /* Minor device # for Logitech  */
#define MICROSOFT_BUSMOUSE      2   /* Minor device # for Microsoft */

/*--------- LOGITECH BUSMOUSE ITEMS -------------*/

#define	LOGIBM_BASE		0x23c
#define	MSE_DATA_PORT		0x23c
#define	MSE_SIGNATURE_PORT	0x23d
#define	MSE_CONTROL_PORT	0x23e
#define	MSE_INTERRUPT_PORT	0x23e
#define	MSE_CONFIG_PORT		0x23f
#define	LOGIBM_EXTENT		0x4

#define	MSE_ENABLE_INTERRUPTS	0x00
#define	MSE_DISABLE_INTERRUPTS	0x10

#define	MSE_READ_X_LOW		0x80
#define	MSE_READ_X_HIGH		0xa0
#define	MSE_READ_Y_LOW		0xc0
#define	MSE_READ_Y_HIGH		0xe0

/* Magic number used to check if the mouse exists */
#define MSE_CONFIG_BYTE		0x91
#define MSE_DEFAULT_MODE	0x90
#define MSE_SIGNATURE_BYTE	0xa5

/* useful Logitech Mouse macros */

#define MSE_INT_OFF()	outb(MSE_DISABLE_INTERRUPTS, MSE_CONTROL_PORT)
#define MSE_INT_ON()	outb(MSE_ENABLE_INTERRUPTS, MSE_CONTROL_PORT)

/*--------- MICROSOFT BUSMOUSE ITEMS -------------*/

#define	MSBM_BASE			0x23d
#define	MS_MSE_DATA_PORT	        0x23d
#define	MS_MSE_SIGNATURE_PORT	        0x23e
#define	MS_MSE_CONTROL_PORT	        0x23c
#define	MS_MSE_CONFIG_PORT		0x23f
#define	MSBM_EXTENT			0x3

#define	MS_MSE_ENABLE_INTERRUPTS	0x11
#define	MS_MSE_DISABLE_INTERRUPTS	0x10

#define	MS_MSE_READ_BUTTONS             0x00
#define	MS_MSE_READ_X		        0x01
#define	MS_MSE_READ_Y                   0x02

#define MS_MSE_START                    0x80
#define MS_MSE_COMMAND_MODE             0x07

/* useful microsoft busmouse macros */

#define MS_MSE_INT_OFF() {outb(MS_MSE_COMMAND_MODE, MS_MSE_CONTROL_PORT); \
			    outb(MS_MSE_DISABLE_INTERRUPTS, MS_MSE_DATA_PORT);}
#define MS_MSE_INT_ON()  {outb(MS_MSE_COMMAND_MODE, MS_MSE_CONTROL_PORT); \
			    outb(MS_MSE_ENABLE_INTERRUPTS, MS_MSE_DATA_PORT);}

 
struct mouse_status {
	unsigned char	buttons;
	unsigned char	latch_buttons;
	int		dx;
	int		dy;	
	int 		present;
	int		ready;
	int		active;
	wait_queue_head_t wait;
	struct fasync_struct *fasyncptr;
};

/* Function Prototypes */

#endif

