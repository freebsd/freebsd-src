/*
 *  Native support for the Aiptek HyperPen USB Tablets
 *  (4000U/5000U/6000U/8000U/12000U)
 *
 *  Copyright (c) 2001      Chris Atenasio   <chris@crud.net>
 *  Copyright (c) 2002-2003 Bryan W. Headley <bwheadley@earthlink.net>
 *
 *  based on wacom.c by
 *     Vojtech Pavlik      <vojtech@suse.cz>
 *     Andreas Bach Aaen   <abach@stofanet.dk>
 *     Clifford Wolf       <clifford@clifford.at>
 *     Sam Mosel           <sam.mosel@computer.org>
 *     James E. Blair      <corvus@gnu.org>
 *     Daniel Egger        <egger@suse.de>
 *
 *  Many thanks to Oliver Kuechemann for his support.
 *
 *  ChangeLog:
 *      v0.1 - Initial release
 *      v0.2 - Hack to get around fake event 28's. (Bryan W. Headley)
 *      v0.3 - Make URB dynamic (Bryan W. Headley, Jun-8-2002)
 *             Released to Linux 2.4.19 and 2.5.x
 *      v0.4 - Rewrote substantial portions of the code to deal with
 *             corrected control sequences, timing, dynamic configuration,
 *             support of 6000U - 12000U, procfs, and macro key support
 *             (Jan-1-2003 - Feb-5-2003, Bryan W. Headley)
 *      v1.0 - Added support for diagnostic messages, count of messages
 *             received from URB - Mar-8-2003, Bryan W. Headley
 *
 * NOTE:
 *      This kernel driver is augmented by the "Aiptek" XFree86 input
 *      driver for your X server, as well as a GUI Front-end "Tablet Manager".
 *      These three products are highly interactive with one another, 
 *      so therefore it's easier to document them all as one subsystem.
 *      Please visit the project's "home page", located at, 
 *      http://aiptektablet.sourceforge.net.
 *
 */

/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/input.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/usb.h>
#include <linux/proc_fs.h>
#include <asm/uaccess.h>

/*
 * Version Information
 */
#define DRIVER_VERSION "v1.0 Mar-8-2003"
#define DRIVER_AUTHOR  "Bryan W. Headley/Chris Atenasio"
#define DRIVER_DESC    "Aiptek HyperPen USB Tablet Driver (Linux 2.4.x)"

MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL");

/*
 * Aiptek status packet:
 * (returned as Report 1)
 *
 *        bit7  bit6  bit5  bit4  bit3  bit2  bit1  bit0
 * byte0   0     0     0     0     0     0     1     0
 * byte1  X7    X6    X5    X4    X3    X2    X1    X0
 * byte2  X15   X14   X13   X12   X11   X10   X9    X8
 * byte3  Y7    Y6    Y5    Y4    Y3    Y2    Y1    Y0
 * byte4  Y15   Y14   Y13   Y12   Y11   Y10   Y9    Y8
 * byte5   *     *     *    BS2   BS1   Tip   DV    IR
 * byte6  P7    P6    P5    P4    P3    P2    P1    P0
 * byte7  P15   P14   P13   P12   P11   P10   P9    P8
 *
 * IR: In Range = Proximity on
 * DV = Data Valid
 * BS = Barrel Switch (as in, macro keys)
 * BS2 also referred to as Tablet Pick
 *
 * Command Summary:
 *
 * Use report_type CONTROL (3)
 * Use report_id   2
 *
 * Command/Data    Description     Return Bytes    Return Value
 * 0x10/0x00       SwitchToMouse       0
 * 0x10/0x01       SwitchToTablet      0
 * 0x18/0x04       Resolution500LPI    0
 * 0x17/0x00       FilterOn            0
 * 0x12/0xFF       AutoGainOn          0
 * 0x01/0x00       GetXExtension       2           MaxX
 * 0x01/0x01       GetYExtension       2           MaxY
 * 0x02/0x00       GetModelCode        2           ModelCode = LOBYTE
 * 0x03/0x00       GetODMCode          2           ODMCode
 * 0x08/0x00       GetPressureLevels   2           =512
 * 0x04/0x00       GetFirmwareVersion  2           Firmware Version
 * 0x11/0x02       EnableMacroKeys     0
 *
 *
 * To initialize the tablet:
 *
 * (1) Send Resolution500LPI (Command)
 * (2) Query for Model code (Option Report)
 * (3) Query for ODM code (Option Report)
 * (4) Query for firmware (Option Report)
 * (5) Query for GetXExtension (Option Report)
 * (6) Query for GetYExtension (Option Report)
 * (7) Query for GetPressureLevels (Option Report)
 * (8) SwitchToTablet for Absolute coordinates, or
 *     SwitchToMouse for Relative coordinates (Command)
 * (9) EnableMacroKeys (Command)
 * (10) FilterOn (Command)
 * (11) AutoGainOn (Command)
 *
 * (Step 9 can be omitted, but you'll then have no function keys.)
 *
 *  The procfs interface
 *  --------------------
 *
 *  This driver supports delivering configuration/status reports
 *  through {procfs}/driver/usb/aiptek. ("procfs" is normally mounted
 *  to /proc.) Said file can be found while the driver is active in
 *  memory; it will be removed when the driver is removed, either 
 *  through user intervention (rmmod aiptek) or through software
 *  such as "hotplug".
 *
 *  Reading from the Procfs interface
 *  ---------------------------------
 *
 *  The user may determine the status of the tablet by reading the
 *  report in the procfs interface, /proc/driver/usb/aiptek.
 *  The report as of driver version 1.0, looks like,
 *
 * Aiptek Tablet (3000x2250, 8.00x6.00", 202x152mm)
 * (USB VendorID 0x08ca, ProductID 0x0020, ODMCode 0x0004
 *  ModelCode: 0x64, FirmwareCode: 0x0400)
 * on /dev/input/event0
 * pointer=either
 * coordinate=absolute
 * tool=pen
 * xtilt=disable
 * ytilt=disable
 * jitter=50
 * diagnostic=none
 * eventsReceived=0
 *
 *  (spurious ", for the benefit of vim's syntax highlighting.)
 *
 *  This report indicates the tablet recognized. (Because Aiptek reuses
 *  the USB 'productID' over several tablets, it's pointless for us to
 *  guess which model you have: we'll instead tell you the size of
 *  the tablet's drawing area, which we indicate in coordinates, inches,
 *  and millimeters.) We also indicate datum read from the USB interface,
 *  such as vendorId, productId, ODMcode, etc. It's there "just in case."
 *
 *      on /dev/input/event0
 *
 *  Linux supports HID-compliant USB devices (such as this tablet) by
 *  transposing their reports to the Linux Input Event System format. Which
 *  means, if you want to data from the tablet, that's where it will be
 *  made available from. For information on the Input Event System, see
 *  the docs in ./Documentation/input, in the kernel source tree.
 *
 *  And yes, depending on the order in which other supported Input Event
 *  devices are recognized and configured, the tablet may be allocated
 *  to a different device driver name: it's all dynamic. Use of the devfs
 *  file system is a help.
 *
 *  The keyword=value part of the report mostly shows what the programmable
 *  parameters have been set to. We describe those below, and how to
 *  program/reprogram them. Note: tablet parameters are to be programmed
 *  while the tablet is attached and active. They are not set as arguments
 *  to the kernel during bootup.
 *
 *  Here are the "read-only" parameters, and what they mean:
 *
 *      diagnostic=stringValue
 *      eventsReceived=numericValue
 *
 * diagnostic: The tablet driver attempts to explain why things are not
 *      working correctly. (To the best of it's insular abilities)
 *
 *      By default, the tablet boots up in Relative Coordinate
 *      mode. This driver initially attempts to program it in Absolute
 *      Coordinate mode (and of course, the user can subsequently choose
 *      which mode they want.) So, therefore, the situation can arise
 *      where the tablet is in one mode, and the driver believes it
 *      is in the other mode. The driver, however, cannot divine
 *      this mismatch until input events are received.  
 *      Two reports indicate such mode-mismatches between the tablet
 *      and the driver, and are,
 *
 *          "tablet sending relative reports"
 *          "tablet sending absolute reports"
 *
 *      The next diagnostic operates in conjunction with the "pointer="
 *      programmable parameter. With it, you can indicate that you want
 *      the tablet to only accept reports from the stylus, or only from the 
 *      mouse. (You can also specify to allow reports from either.) What
 *      happens when you specify that you only want mouse reports, yet
 *      the tablet keeps receiving reports from the stylus? Well, first,
 *      it's a "pilot error", but secondly, it tries to diagnose the issue
 *      with the following reports,
 *
 *          "tablet seeing reports from stylus"
 *          "tablet seeing reports from mouse"
 *
 *      What if there is nothing to report? The inference in the diagnostic
 *      reports is that something is happening which shouldn't: when things
 *      appear to be working right, the report is,
 *
 *          "none"
 *
 *      The error diagnostic report is dynamic: it only reports issues
 *      that are happening, or have happened as of the last event received.
 *      It will reset following any attempt to reprogram the tablet's mode.
 *
 * eventsReceived: Occasionally, your movements on the tablet are not being
 *      reported. Usually, this indicates that your tablet is out of sync
 *      with the USB interface driver, or itself is not sending reports
 *      out. To help diagnose this, we keep an active count of events
 *      received from the tablet. So, if you move the stylus, and yet 
 *      your client application doesn't notice, make
 *      note of the eventsReceived, and then move the stylus again. If the
 *      event counter's number doesn't change, then the tablet indeed has
 *      "froze". 
 *
 *      We have found that sending the tablet a command sequence often
 *      will clear up "frozen" tablets. Which segues into the section
 *      about how to program your tablet through the procfs interface,
 *
 *  Writing to the procfs interface
 *  -------------------------------
 *
 *  The user may configure the tablet by writing ASCII
 *  commands to the /proc/driver/usb/aiptek file. Commands which are
 *  accepted are,
 *
 *      pointer=stringvalue      {stylus|mouse|either}
 *      coordinate=stringvalue   {absolute|relative}
 *      tool=stringvalue         {mouse|rubber|pen|pencil|brush|airbrush}
 *      xtilt=string_or_numeric  {disable|[-128..127]}
 *      ytilt=string_or_numeric  {disable|[-128..127]}
 *      jitter=numericvalue      {0..xxx}
 *
 *  pointer: you can specify that reports are to be excepted ONLY from the
 *      stylus, or ONLY from the mouse. 'either' allows reports from either
 *      device to be accepted, and is the default.
 *  coordinate: you can specify that either absolute or relative coordinate
 *      reports are issued by the tablet. By default, absolute reports are
 *      sent.
 *  tool: The stylus by default prepends TOOL_BTN_PEN events with it's
 *      reports. But you may decide that you want your stylus to behave
 *      like an eraser (named 'rubber', following tablet conventions,)
 *      or a pencil, etc. The behavior is dependent upon the client software
 *      consuming the tablet's events, e.g., the XFree86 tablet driver.
 *  xtilt: By default this is disabled. However, other tablets have a notion
 *      of measuring the angle at which the stylus pen is held against the
 *      drawing surface, along the X axis. Aiptek tablets cannot sense this,
 *      but if you want to send "held-at-angle" reports, specify the value,
 *      an integer between -128 and 127 (inclusive) that you want to send.
 *      This data will be sent along with regular tablet input. Obviously,
 *      the inference here is that your hand does not change angles 
 *      while drawing (until you go back to this procfs interface, and
 *      change the value)!
 *
 *      When you consider actual drawing tools (real pens, brushes),
 *      knowing the tools' tip shape and the angle that you hold the tool 
 *      becomes important, insofar as calculating the surface of the tip 
 *      that actually touches the surface of the paper. Knowledge of what 
 *      to do with xtilt reports is solely in the realm of your client 
 *      software.
 *
 *      Yes, there is a difference between xtilt=0 and xtilt=disable
 *      settings. The former sends a report that the angle is a 0;
 *      the other indicates that NO xtilt reports are to be sent at all.
 *  ytilt: By default this is disabled. This provides similar functionality
 *      to xtilt, except that we're measuring the angle the stylus pen is
 *      held against the drawing surface, along the Y axis. Same cavaets
 *      apply as for xtilt.
 *  jitter: By default, this is set to 50. When pressing a button on
 *      either the mouse or the stylus pen, you will probably notice that
 *      the tool moves slightly from it's original position, until your
 *      hand steadies it. During that period of time, the pen is "jittering",
 *      sending spurious movement events that perhaps you'd like it not to
 *      send. What we do is set a moratorium, measured in milliseconds,
 *      during which we do not send movement events. So, the default is 50ms;
 *      you obviously can set it to zero or incredibly unreasonable values
 *      (no reports for 4 seconds following the pressing of a stylus button!)
 *
 * Interesting Side-Note
 * ---------------------
 *
 *  The tablet has "frozen" and you'd like to send it a command to wake it
 *  up. But you don't want to change how the driver's currently configured.
 *
 *  1. Send a command to /proc/driver/usb/aiptek with the same setting
 *     already reported by the driver.
 *  2. Send an illegal string to procfs file ("wakeup=now" is always good)
 *  3. Because, the driver always attempts to reprogram the tablet to it's
 *     current settings following a write to the procfs interface.
 *
 *  Hmm, still does not work.
 *  -------------------------
 *
 *  This is slightly harder to diagnose. You may be receiving frame errors
 *  from the USB interface driver (see /var/log/messages for any diagnostics).
 *
 *  Alternatively, you may be running something like 'hotplug' that attempts
 *  to match discovered USB devices to it's list of device drivers. 
 *  Unfortunately, because this is a tablet that can send relative X,Y events,
 *  it "looks like" a mouse! A usb mouse driver may have possession of
 *  input from the tablet. On the other hand, the tablet also supports 
 *  absolute reports from barrel switches, which sounds a lot like a "joystick",
 *  and the software again can be fooled into loading the wrong driver for
 *  the tablet. The distinction is, USB HID devices tell you what they
 *  are capable of, rather than what they are.
 *
 *  Come visit this driver's home page at http://aiptektablet.sourceforge.net
 *  for further assistance.
 */

#define USB_VENDOR_ID_AIPTEK   0x08ca

#define AIPTEK_POINTER_ONLY_MOUSE_MODE      0
#define AIPTEK_POINTER_ONLY_STYLUS_MODE     1
#define AIPTEK_POINTER_EITHER_MODE          2

#define AIPTEK_POINTER_ALLOW_MOUSE_MODE(a) \
        (a == AIPTEK_POINTER_ONLY_MOUSE_MODE || \
         a == AIPTEK_POINTER_EITHER_MODE)
#define AIPTEK_POINTER_ALLOW_STYLUS_MODE(a) \
        (a == AIPTEK_POINTER_ONLY_STYLUS_MODE || \
         a == AIPTEK_POINTER_EITHER_MODE)

#define AIPTEK_COORDINATE_RELATIVE_MODE 0
#define AIPTEK_COORDINATE_ABSOLUTE_MODE 1

#define AIPTEK_TILT_MIN                      (-128)
#define AIPTEK_TILT_MAX                      127
#define AIPTEK_TILT_DISABLE                  (-10101)

#define AIPTEK_TOOL_BUTTON_PEN_MODE          0
#define AIPTEK_TOOL_BUTTON_PENCIL_MODE       1
#define AIPTEK_TOOL_BUTTON_BRUSH_MODE        2
#define AIPTEK_TOOL_BUTTON_AIRBRUSH_MODE     3
#define AIPTEK_TOOL_BUTTON_RUBBER_MODE       4
#define AIPTEK_TOOL_BUTTON_MOUSE_MODE        5

#define AIPTEK_DIAGNOSTIC_NA                 0
#define AIPTEK_DIAGNOSTIC_SENDING_RELATIVE_IN_ABSOLUTE  1
#define AIPTEK_DIAGNOSTIC_SENDING_ABSOLUTE_IN_RELATIVE  2
#define AIPTEK_DIAGNOSTIC_TOOL_DISALLOWED               3

    // Time to wait (in ms) to help mask hand jittering
    // when pressing the stylus buttons.
#define AIPTEK_JITTER_DELAY_DEFAULT         50

struct aiptek_features {
	char *name;
	int pktlen;
	int x_max;
	int y_max;
	int pressure_max;
	int odmCode;
	int modelCode;
	int firmwareCode;
	void (*irq) (struct urb * urb);
};

struct aiptek {
	signed char data[10];
	struct input_dev dev;
	struct usb_device *usbdev;
	struct urb *irq;
	struct aiptek_features *features;
	unsigned int ifnum;
	int open_count;
	int pointer_mode;
	int coordinate_mode;
	int tool_mode;
	int xTilt;
	int yTilt;
	int diagnostic;
	unsigned long eventCount;
	int jitterDelay;
#ifdef CONFIG_PROC_FS
	struct proc_dir_entry *usbProcfsEntry;
	struct proc_dir_entry *aiptekProcfsEntry;
#endif
};

/*
 * Permit easy lookup of keyboard events to send, versus
 * the bitmap which comes from the tablet. This hides the
 * issue that the F_keys are not sequentially numbered.
 */
static int macroKeyEvents[] = { KEY_F1, KEY_F2, KEY_F3, KEY_F4, KEY_F5, KEY_F6,
	KEY_F7, KEY_F8, KEY_F9, KEY_F10, KEY_F11, KEY_F12,
	KEY_F13, KEY_F14, KEY_F15, KEY_F16, KEY_F17, KEY_F18,
	KEY_F19, KEY_F20, KEY_F21, KEY_F22, KEY_F23, KEY_F24,
	KEY_STOP, KEY_AGAIN, KEY_PROPS, KEY_UNDO, KEY_FRONT, KEY_COPY,
	KEY_OPEN, KEY_PASTE, 0
};

#ifdef CONFIG_PROC_FS
extern struct proc_dir_entry *proc_root_driver;
#endif

static int
aiptek_convert_from_2s_complement(unsigned char c)
{
	unsigned char b = c;
	int negate = 0;
	int ret;

	if (b & 0x80) {
		b = ~b;
		b--;
		negate = 1;
	}
	ret = b;
	ret = (negate == 1) ? -ret : ret;
	return ret;
}

/*
 * aiptek_irq can receive one of six potential reports.
 * The documentation for each is in the body of the function.
 *
 * The tablet reports on several attributes per invocation of
 * aiptek_irq. Because the Linux Input Event system allows the
 * transmission of ONE attribute per input_report_xxx() call,
 * collation has to be done on the other end to reconstitute
 * a complete tablet report. Further, the number of Input Event reports
 * submitted varies, depending on what USB report type, and circumstance.
 * To deal with this, EV_MSC is used to indicate an 'end-of-report'
 * message. This has been an undocumented convention understood by the kernel
 * tablet driver and clients such as gpm and XFree86's tablet drivers.
 *
 * Of the information received from the tablet, the one piece I
 * cannot transmit is the proximity bit (without resorting to an EV_MSC
 * convention above.) I therefore have taken over REL_MISC and ABS_MISC
 * (for relative and absolute reports, respectively) for communicating
 * Proximity. Why two events? I thought it interesting to know if the
 * Proximity event occured while the tablet was in absolute or relative
 * mode.
 *
 * Other tablets use the notion of a certain minimum stylus pressure
 * to infer proximity. While that could have been done, that is yet
 * another 'by convention' behavior, the documentation for which
 * would be spread between two (or more) pieces of software.
 *
 * EV_MSC usage is terminated in Linux 2.5.x.
 */

static void
aiptek_irq(struct urb *urb)
{
	struct aiptek *aiptek = urb->context;
	unsigned char *data = aiptek->data;
	struct input_dev *dev = &aiptek->dev;
	int jitterable = 0;

	if (urb->status)
		return;

	aiptek->eventCount++;

	// Report 1 delivers relative coordinates with either a stylus
	// or the mouse. You do not know which tool generated the event.
	if (data[0] == 1) {
		if (aiptek->coordinate_mode == AIPTEK_COORDINATE_ABSOLUTE_MODE) {
			aiptek->diagnostic =
			    AIPTEK_DIAGNOSTIC_SENDING_RELATIVE_IN_ABSOLUTE;
		} else {
			int x, y, left, right, middle;

			if (aiptek->tool_mode != AIPTEK_TOOL_BUTTON_MOUSE_MODE) {
				aiptek->tool_mode =
				    AIPTEK_TOOL_BUTTON_MOUSE_MODE;
				input_report_key(dev, BTN_TOOL_MOUSE, 1);
			}
			x = aiptek_convert_from_2s_complement(data[2]);
			y = aiptek_convert_from_2s_complement(data[3]);

			left = data[5] & 0x01;
			right = data[5] & 0x02;
			middle = data[5] & 0x04;

			jitterable = left | right | middle;

			input_report_key(dev, BTN_LEFT, left);
			input_report_key(dev, BTN_MIDDLE, middle);
			input_report_key(dev, BTN_RIGHT, right);
			input_report_rel(dev, REL_X, x);
			input_report_rel(dev, REL_Y, y);
			input_report_rel(dev, REL_MISC, 1);

			input_event(dev, EV_MSC, MSC_SERIAL, 0);
		}
	}
	// Report 2 is delivered only by the stylus, and delivers
	// absolute coordinates.
	else if (data[0] == 2) {
		if (aiptek->coordinate_mode == AIPTEK_COORDINATE_RELATIVE_MODE) {
			aiptek->diagnostic =
			    AIPTEK_DIAGNOSTIC_SENDING_ABSOLUTE_IN_RELATIVE;
		} else
		    if (!AIPTEK_POINTER_ALLOW_STYLUS_MODE(aiptek->pointer_mode))
		{
			aiptek->diagnostic = AIPTEK_DIAGNOSTIC_TOOL_DISALLOWED;
		} else {
			int x = ((__u32) data[1]) | ((__u32) data[2] << 8);
			int y = ((__u32) data[3]) | ((__u32) data[4] << 8);
			int z = ((__u32) data[6]) | ((__u32) data[7] << 8);

			int p = data[5] & 0x01;
			int dv = data[5] & 0x02;
			int tip = data[5] & 0x04;
			int bs = data[5] & 0x08;
			int pck = data[5] & 0x10;

			// dv indicates 'data valid' (e.g., the tablet is in sync
			// and has delivered a "correct" report) We will ignore
			// all 'bad' reports...
			if (dv != 0) {
				switch (aiptek->tool_mode) {
				case AIPTEK_TOOL_BUTTON_PEN_MODE:
					{
						input_report_key(dev,
								 BTN_TOOL_PEN,
								 1);
					}
					break;

				case AIPTEK_TOOL_BUTTON_PENCIL_MODE:
					{
						input_report_key(dev,
								 BTN_TOOL_PENCIL,
								 1);
					}
					break;

				case AIPTEK_TOOL_BUTTON_BRUSH_MODE:
					{
						input_report_key(dev,
								 BTN_TOOL_BRUSH,
								 1);
					}
					break;

				case AIPTEK_TOOL_BUTTON_AIRBRUSH_MODE:
					{
						input_report_key(dev,
								 BTN_TOOL_AIRBRUSH,
								 1);
					}
					break;

				case AIPTEK_TOOL_BUTTON_RUBBER_MODE:
					{
						input_report_key(dev,
								 BTN_TOOL_RUBBER,
								 1);
					}
					break;

				case AIPTEK_TOOL_BUTTON_MOUSE_MODE:
					{
						input_report_key(dev,
								 BTN_TOOL_MOUSE,
								 1);
					}
					break;
				}

				input_report_abs(dev, ABS_X, x);
				input_report_abs(dev, ABS_Y, y);

				/*
				 * The user is allowed to switch from one of the
				 * stylus tools to the Mouse using the front-end GUI.
				 * An issue that will arise, however, is what happens
				 * when the user HAS issued a TOOL_BTN_MOUSE, but has not
				 * yet swapped tools. Well, we can "pretend" to be a mouse
				 * by sending overriding tip, barrelswitch and pick.
				 * This stupidity should not be used as an excuse not
				 * to physically move your Aiptek mouse into the tablet's
				 * active area -- it merely provides momentary convenience
				 * during that transition.
				 */
				if (aiptek->tool_mode ==
				    AIPTEK_TOOL_BUTTON_MOUSE_MODE) {
					input_report_key(dev, BTN_LEFT, tip);
					input_report_key(dev, BTN_RIGHT, bs);
					input_report_key(dev, BTN_MIDDLE, pck);

					jitterable = tip | bs | pck;
				} else {
					input_report_abs(dev, ABS_PRESSURE, z);

					input_report_key(dev, BTN_TOUCH, tip);
					input_report_key(dev, BTN_STYLUS, bs);
					input_report_key(dev, BTN_STYLUS2, pck);

					jitterable = tip | bs | pck;

					if (aiptek->xTilt !=
					    AIPTEK_TILT_DISABLE)
						input_report_abs(dev,
								 ABS_TILT_X,
								 aiptek->xTilt);
					if (aiptek->yTilt !=
					    AIPTEK_TILT_DISABLE)
						input_report_abs(dev,
								 ABS_TILT_Y,
								 aiptek->yTilt);
				}
				input_report_abs(dev, ABS_MISC, p);
				input_event(dev, EV_MSC, MSC_SERIAL, 0);
			}
		}
	}
	// Report 3's come from the mouse in absolute mode.
	else if (data[0] == 3) {
		if (aiptek->coordinate_mode == AIPTEK_COORDINATE_RELATIVE_MODE) {
			aiptek->diagnostic =
			    AIPTEK_DIAGNOSTIC_SENDING_ABSOLUTE_IN_RELATIVE;
		} else
		    if (!AIPTEK_POINTER_ALLOW_MOUSE_MODE(aiptek->pointer_mode))
		{
			aiptek->diagnostic = AIPTEK_DIAGNOSTIC_TOOL_DISALLOWED;
		} else {
			int x = ((__u32) data[1]) | ((__u32) data[2] << 8);
			int y = ((__u32) data[3]) | ((__u32) data[4] << 8);
			int p = data[5] & 0x01;
			int dv = data[5] & 0x02;
			int left = data[5] & 0x04;
			int right = data[5] & 0x08;
			int middle = data[5] & 0x10;

			if (dv != 0) {
				input_report_key(dev, BTN_TOOL_MOUSE, 1);
				input_report_abs(dev, ABS_X, x);
				input_report_abs(dev, ABS_Y, y);

				input_report_key(dev, BTN_LEFT, left);
				input_report_key(dev, BTN_MIDDLE, middle);
				input_report_key(dev, BTN_RIGHT, right);

				jitterable = left | middle | right;

				input_report_rel(dev, REL_MISC, p);
				input_event(dev, EV_MSC, MSC_SERIAL, 0);
			}
		}
	}
	// Report 4s come from the macro keys when pressed by stylus
	else if (data[0] == 4) {
		int p = data[1] & 0x01;
		int dv = data[1] & 0x02;
		int tip = data[1] & 0x04;
		int bs = data[1] & 0x08;
		int pck = data[1] & 0x10;

		int m = data[3];
		int z = ((__u32) data[4]) | ((__u32) data[5] << 8);

		if (dv != 0) {
			input_report_key(dev, BTN_TOUCH, tip);
			input_report_key(dev, BTN_STYLUS, bs);
			input_report_key(dev, BTN_STYLUS2, pck);

			jitterable = tip | bs | pck;

			input_report_key(dev, macroKeyEvents[m - 1], 1);
			input_report_abs(dev, ABS_PRESSURE, z);
			input_report_abs(dev, ABS_MISC, p);
			input_event(dev, EV_MSC, MSC_SERIAL, 0);
		}
	}
	// Report 5s come from the macro keys when pressed by mouse
	else if (data[0] == 5) {
		int p = data[1] & 0x01;
		int dv = data[1] & 0x02;
		int left = data[1] & 0x04;
		int right = data[1] & 0x08;
		int middle = data[1] & 0x10;
		int macro = data[3];

		if (dv != 0) {
			input_report_key(dev, BTN_LEFT, left);
			input_report_key(dev, BTN_MIDDLE, middle);
			input_report_key(dev, BTN_RIGHT, right);

			jitterable = left | middle | right;

			input_report_key(dev, macroKeyEvents[macro - 1], 1);
			input_report_rel(dev, ABS_MISC, p);
			input_event(dev, EV_MSC, MSC_SERIAL, 0);
		}
	}
	// We have no idea which tool can generate a report 6. Theoretically,
	// neither need to, having been given reports 4 & 5 for such use.
	// However, report 6 is the 'official-looking' report for macroKeys;
	// reports 4 & 5 supposively are used to support unnamed, unknown
	// hat switches (which just so happen to be the macroKeys.)
	else if (data[0] == 6) {
		int macro = ((__u32) data[1]) | ((__u32) data[2] << 8);

		input_report_key(dev, macroKeyEvents[macro - 1], 1);
		input_report_abs(dev, ABS_MISC, 1);
		input_event(dev, EV_MSC, MSC_SERIAL, 0);
	} else {
		dbg("Unknown report %d", data[0]);
	}

	// Jitter may occur when the user presses a button on the stlyus
	// or the mouse. What we do to prevent that is wait 'x' milliseconds
	// following a 'jitterable' event, which should give the hand some time
	// stabilize itself.
	if (jitterable != 0 && aiptek->jitterDelay != 0) {
		wait_ms(aiptek->jitterDelay);
	}
}

/*
 * We are not able to reliably determine the tablet featureset by
 * asking for the USB productID. Therefore, we will query the
 * tablet dynamically and populate the struct in aiptek_probe().
 */

struct aiptek_features aiptek_features[] = {
	{"Aiptek", 8, 0, 0, 0, 0, 0, 0, aiptek_irq},
	{NULL, 0}
};

/*
 * These are the USB id's known so far. We do not identify them to
 * specific Aiptek model numbers, because there has been overlaps,
 * use, and reuse of id's in existing models. Certain models have
 * been known to use more than one ID, indicative perhaps of
 * manufacturing revisions. In any event, we consider these 
 * IDs to not be model-specific nor unique.
 */

struct usb_device_id aiptek_ids[] = {
      {USB_DEVICE(USB_VENDOR_ID_AIPTEK, 0x01), driver_info:0},
      {USB_DEVICE(USB_VENDOR_ID_AIPTEK, 0x10), driver_info:0},
      {USB_DEVICE(USB_VENDOR_ID_AIPTEK, 0x20), driver_info:0},
      {USB_DEVICE(USB_VENDOR_ID_AIPTEK, 0x21), driver_info:0},
      {USB_DEVICE(USB_VENDOR_ID_AIPTEK, 0x22), driver_info:0},
      {USB_DEVICE(USB_VENDOR_ID_AIPTEK, 0x23), driver_info:0},
      {USB_DEVICE(USB_VENDOR_ID_AIPTEK, 0x24), driver_info:0},
	{}
};

MODULE_DEVICE_TABLE(usb, aiptek_ids);

static int
aiptek_open(struct input_dev *dev)
{
	struct aiptek *aiptek = dev->private;
	if (aiptek->open_count++)
		return 0;

	aiptek->irq->dev = aiptek->usbdev;
	if (usb_submit_urb(aiptek->irq))
		return -EIO;

	return 0;
}

static void
aiptek_close(struct input_dev *dev)
{
	struct aiptek *aiptek = dev->private;

	if (!--aiptek->open_count)
		usb_unlink_urb(aiptek->irq);
}

/*
 * Send a command to the tablet. No reply is expected.
 */
static void
aiptek_command(struct usb_device *dev, unsigned int ifnum,
	       unsigned char command, unsigned char data)
{
	__u8 buf[3];

	buf[0] = 2;
	buf[1] = command;
	buf[2] = data;

	if (usb_set_report(dev, ifnum, 3, 2, buf, sizeof (buf)) != sizeof (buf)) {
		dbg("aiptek_command failed, sending: 0x%02x 0x%02x", command,
		    data);
	}
}

/*
 * Send a query to the tablet. This is done by sending the query stream
 * first as a command, waiting a few milliseconds, then submitting the
 * same stream as a query.
 */
static unsigned int
aiptek_query(struct usb_device *dev, unsigned int ifnum,
	     unsigned char command, unsigned char data)
{
	unsigned int ret;
	__u8 buf[8];
	buf[0] = 2;
	buf[1] = command;
	buf[2] = data;

	aiptek_command(dev, ifnum, command, data);
	wait_ms(400);

	if (usb_get_report(dev, ifnum, 3, 2, buf, 3) < 3) {
		dbg("aiptek_query failed: returns 0x%02x 0x%02x 0x%02x",
		    buf[0], buf[1], buf[2]);
		return 0;
	}
	ret = ((__u32) buf[1]) | ((__u32) buf[2] << 8);
	return ret;
}

/*
 * Program the tablet into either absolute or relative mode.
 *
 * We also get information about the tablet's size.
 */
static void
aiptek_program_tablet(struct aiptek *aiptek)
{
	int modelCode, odmCode, firmwareCode;
	int xResolution, yResolution, zResolution;

	aiptek->diagnostic = AIPTEK_DIAGNOSTIC_NA;

	// execute Resolution500LPI
	aiptek_command(aiptek->usbdev, aiptek->ifnum, 0x18, 0x04);
	// query getModelCode
	modelCode = aiptek_query(aiptek->usbdev, aiptek->ifnum, 0x02, 0x00);
	// query getODMCode
	odmCode = aiptek_query(aiptek->usbdev, aiptek->ifnum, 0x03, 0x00);
	// query getFirmwareCode
	firmwareCode = aiptek_query(aiptek->usbdev, aiptek->ifnum, 0x04, 0x00);
	// query getXextension
	xResolution = aiptek_query(aiptek->usbdev, aiptek->ifnum, 0x01, 0x00);
	// query getYextension
	yResolution = aiptek_query(aiptek->usbdev, aiptek->ifnum, 0x01, 0x01);
	// query getPressureLevels
	zResolution = aiptek_query(aiptek->usbdev, aiptek->ifnum, 0x08, 0x00);

	// Depending on whether we are in absolute or relative mode, we will
	// do a switchToTablet(absolute) or switchToMouse(relative) command.
	if (aiptek->coordinate_mode == AIPTEK_COORDINATE_ABSOLUTE_MODE) {
		// execute switchToTablet
		aiptek_command(aiptek->usbdev, aiptek->ifnum, 0x10, 0x01);
	} else {
		// execute switchToMouse
		aiptek_command(aiptek->usbdev, aiptek->ifnum, 0x10, 0x00);
	}
	// This command enables the macro keys
	aiptek_command(aiptek->usbdev, aiptek->ifnum, 0x11, 0x02);
	// execute FilterOn
	aiptek_command(aiptek->usbdev, aiptek->ifnum, 0x17, 0x00);
	// execute AutoGainOn
	aiptek_command(aiptek->usbdev, aiptek->ifnum, 0x12, 0xff);

	aiptek->features->odmCode = odmCode;
	aiptek->features->modelCode = modelCode & 0xff;
	aiptek->features->firmwareCode = firmwareCode;
	aiptek->features->pressure_max = zResolution;
	aiptek->features->x_max = xResolution;
	aiptek->features->y_max = yResolution;

	aiptek->eventCount = 0;
}

#if defined(CONFIG_PROC_FS)
/*
 * This routine determines keywords and their associated values, and
 * maps them to supported modes in this driver. It's input comes from
 * aiptek_procfs_write().
 */
static void
aiptek_procfs_parse(struct aiptek *aiptek, char *keyword, char *value)
{
	if (strcmp(keyword, "pointer") == 0) {
		if (strcmp(value, "stylus") == 0) {
			aiptek->pointer_mode = AIPTEK_POINTER_ONLY_STYLUS_MODE;
		} else if (strcmp(value, "mouse") == 0) {
			aiptek->pointer_mode = AIPTEK_POINTER_ONLY_MOUSE_MODE;
		} else if (strcmp(value, "either") == 0) {
			aiptek->pointer_mode = AIPTEK_POINTER_EITHER_MODE;
		}
	} else if (strcmp(keyword, "coordinate") == 0) {
		if (strcmp(value, "relative") == 0) {
			aiptek->coordinate_mode =
			    AIPTEK_COORDINATE_RELATIVE_MODE;
		} else if (strcmp(value, "absolute") == 0) {
			aiptek->coordinate_mode =
			    AIPTEK_COORDINATE_ABSOLUTE_MODE;
		}
	} else if (strcmp(keyword, "xtilt") == 0) {
		if (strcmp(value, "disable") == 0) {
			aiptek->xTilt = AIPTEK_TILT_DISABLE;
		} else {
			int x = (int) simple_strtol(value, 0, 10);
			if (x >= AIPTEK_TILT_MIN && x <= AIPTEK_TILT_MAX)
				aiptek->xTilt = x;
		}
	} else if (strcmp(keyword, "ytilt") == 0) {
		if (strcmp(value, "disable") == 0) {
			aiptek->yTilt = AIPTEK_TILT_DISABLE;
		} else {
			int y = (int) simple_strtol(value, 0, 10);
			if (y >= AIPTEK_TILT_MIN && y <= AIPTEK_TILT_MAX)
				aiptek->yTilt = y;
		}
	} else if (strcmp(keyword, "jitter") == 0) {
		aiptek->jitterDelay = (int) simple_strtol(value, 0, 10);
	} else if (strcmp(keyword, "tool") == 0) {
		if (strcmp(value, "mouse") == 0) {
			aiptek->tool_mode = AIPTEK_TOOL_BUTTON_MOUSE_MODE;
		} else if (strcmp(value, "rubber") == 0) {
			aiptek->tool_mode = AIPTEK_TOOL_BUTTON_RUBBER_MODE;
		} else if (strcmp(value, "pencil") == 0) {
			aiptek->tool_mode = AIPTEK_TOOL_BUTTON_PENCIL_MODE;
		} else if (strcmp(value, "pen") == 0) {
			aiptek->tool_mode = AIPTEK_TOOL_BUTTON_PEN_MODE;
		} else if (strcmp(value, "brush") == 0) {
			aiptek->tool_mode = AIPTEK_TOOL_BUTTON_BRUSH_MODE;
		} else if (strcmp(value, "airbrush") == 0) {
			aiptek->tool_mode = AIPTEK_TOOL_BUTTON_AIRBRUSH_MODE;
		}
	}
}

/*
 * This routine reads the status of the aiptek driver, and makes it
 * available as a procfs file. The description of the procfs file
 * is at the top of this driver source code.
 */
static int
aiptek_procfs_read(char *page, char **start, off_t offset, int count,
		   int *eof, void *data)
{
	int len;
	char *out = page;
	struct aiptek *aiptek = data;

	out +=
	    sprintf(out, "Aiptek Tablet (%dx%d)\n",
		    aiptek->features->x_max, aiptek->features->y_max);

	out +=
	    sprintf(out,
		    "(USB VendorID 0x%04x, ProductID 0x%04x, ODMCode 0x%04x\n",
		    aiptek->dev.idvendor, aiptek->dev.idproduct,
		    aiptek->features->odmCode);
	out +=
	    sprintf(out, " ModelCode: 0x%02x, FirmwareCode: 0x%04x)\n",
		    aiptek->features->modelCode,
		    aiptek->features->firmwareCode);

	out += sprintf(out, "on /dev/input/event%d\n", aiptek->dev.number);
	out += sprintf(out, "pointer=%s\n",
		       (aiptek->pointer_mode == AIPTEK_POINTER_ONLY_MOUSE_MODE
			? "mouse"
			: (aiptek->pointer_mode ==
			   AIPTEK_POINTER_ONLY_STYLUS_MODE ? "stylus" :
			   "either")));
	out +=
	    sprintf(out, "coordinate=%s\n",
		    (aiptek->coordinate_mode ==
		     AIPTEK_COORDINATE_RELATIVE_MODE ? "relative" :
		     "absolute"));

	out += sprintf(out, "tool=");
	switch (aiptek->tool_mode) {
	case AIPTEK_TOOL_BUTTON_MOUSE_MODE:
		out += sprintf(out, "mouse\n");
		break;

	case AIPTEK_TOOL_BUTTON_RUBBER_MODE:
		out += sprintf(out, "rubber\n");
		break;

	case AIPTEK_TOOL_BUTTON_PEN_MODE:
		out += sprintf(out, "pen\n");
		break;

	case AIPTEK_TOOL_BUTTON_PENCIL_MODE:
		out += sprintf(out, "pencil\n");
		break;

	case AIPTEK_TOOL_BUTTON_BRUSH_MODE:
		out += sprintf(out, "brush\n");
		break;

	case AIPTEK_TOOL_BUTTON_AIRBRUSH_MODE:
		out += sprintf(out, "airbrush\n");
		break;
	}

	out += sprintf(out, "xtilt=");
	if (aiptek->xTilt == AIPTEK_TILT_DISABLE) {
		out += sprintf(out, "disable\n");
	} else {
		out += sprintf(out, "%d\n", aiptek->xTilt);
	}

	out += sprintf(out, "ytilt=");
	if (aiptek->yTilt == AIPTEK_TILT_DISABLE) {
		out += sprintf(out, "disable\n");
	} else {
		out += sprintf(out, "%d\n", aiptek->yTilt);
	}

	out += sprintf(out, "jitter=%d\n", aiptek->jitterDelay);

	out += sprintf(out, "diagnostic=");
	switch (aiptek->diagnostic) {
	case AIPTEK_DIAGNOSTIC_NA:
		out += sprintf(out, "none\n");
		break;
	case AIPTEK_DIAGNOSTIC_SENDING_RELATIVE_IN_ABSOLUTE:
		out += sprintf(out, "tablet sending relative reports\n");
		break;
	case AIPTEK_DIAGNOSTIC_SENDING_ABSOLUTE_IN_RELATIVE:
		out += sprintf(out, "tablet sending absolute reports\n");
		break;
	case AIPTEK_DIAGNOSTIC_TOOL_DISALLOWED:
		out += sprintf(out, "tablet seeing reports from ");
		if (aiptek->pointer_mode == AIPTEK_POINTER_ONLY_MOUSE_MODE)
			out += sprintf(out, "stylus\n");
		else
			out += sprintf(out, "mouse\n");
		break;
	}

	out += sprintf(out, "eventsReceived=%lu\n", aiptek->eventCount);

	len = out - page;
	len -= offset;
	if (len < count) {
		*eof = 1;
		if (len <= 0) {
			return 0;
		}
	} else {
		len = count;
	}

	*start = page + offset;

	return len;
}

/*
 * This routine permits the setting of driver parameters through a
 * procfs file. Writing to the procfs file (/proc/driver/usb/aiptek),
 * you can program the tablet's behavior. Parameters that can be programmed
 * (and their legal values) are described at the top of this driver.
 *
 *
 * This parser is order-insensitive, and supports one or many parameters
 * to be sent in one write request. As many parameters as you may fit
 * in 64 bytes; we only require that you separate them with \n's.
 *
 * Any command that is not understood by the parser is silently ignored.
 */
static int
aiptek_procfs_write(struct file *file, const char *buffer, unsigned long count,
		    void *data)
{
	char buf[64];
	char *scan;
	char *keyword = NULL;
	char *value = NULL;
	struct aiptek *aiptek = data;
	int num;

	num = (count < 64) ? count : 64;
	if (copy_from_user(buf, buffer, num))
		return -EFAULT;
	buf[num] = '\0';

	scan = buf;
	while (*scan) {
		if (*scan == '\n' || *scan == '\0') {
			if (*scan == '\n') {
				*scan = '\0';
				scan++;
			}
			if (keyword && value) {
				aiptek_procfs_parse(aiptek, keyword, value);
			}
			keyword = NULL;
			value = NULL;
			continue;
		}

		if (*scan != '=' && keyword == NULL) {
			keyword = scan;
		} else if (*scan == '=') {
			*scan++ = '\0';
			value = scan;
		}
		scan++;
	}
	// We're insensitive as to whether the buffer ended in a \n or not.
	if (keyword && value) {
		aiptek_procfs_parse(aiptek, keyword, value);
	}

	aiptek_program_tablet(aiptek);

	return num;
}

/*
 * This routine destroys our procfs device interface. This will occur
 * when you remove the driver, either through rmmod or the hotplug system.
 */
static void
destroy_procfs_file(struct aiptek *aiptek)
{
	if (aiptek->aiptekProcfsEntry)
		remove_proc_entry("aiptek", aiptek->usbProcfsEntry);
	if (aiptek->usbProcfsEntry)
		remove_proc_entry("usb", proc_root_driver);

	aiptek->usbProcfsEntry = NULL;
	aiptek->aiptekProcfsEntry = NULL;
}

/*
 * This routine builds the procfs file. The file is located at,
 *      procfs/driver/usb/aiptek.
 */
static void
create_procfs_file(struct aiptek *aiptek)
{
	// Make procfs/driver/usb directory
	aiptek->usbProcfsEntry = create_proc_entry("usb", S_IFDIR,
						   proc_root_driver);
	if (!aiptek->usbProcfsEntry) {
		dbg("create_procfs_file failed; no procfs/driver/usb control file.");
		destroy_procfs_file(aiptek);
		return;
	}
	aiptek->usbProcfsEntry->owner = THIS_MODULE;

	// Make procfs/driver/usb/aiptek file
	aiptek->aiptekProcfsEntry = create_proc_entry("aiptek",
						      S_IFREG | S_IRUGO |
						      S_IWUGO,
						      aiptek->usbProcfsEntry);
	if (!aiptek->aiptekProcfsEntry) {
		dbg("create_procfs_file failed; no procfs/driver/usb control file.");
		destroy_procfs_file(aiptek);
		return;
	}
	aiptek->aiptekProcfsEntry->owner = THIS_MODULE;
	aiptek->aiptekProcfsEntry->data = aiptek;
	aiptek->aiptekProcfsEntry->read_proc = aiptek_procfs_read;
	aiptek->aiptekProcfsEntry->write_proc = aiptek_procfs_write;
}
#endif

static void *
aiptek_probe(struct usb_device *dev, unsigned int ifnum,
	     const struct usb_device_id *id)
{
	struct usb_endpoint_descriptor *endpoint;
	struct aiptek *aiptek;
	int i;

	if (!(aiptek = kmalloc(sizeof (struct aiptek), GFP_KERNEL)))
		return NULL;

	memset(aiptek, 0, sizeof (struct aiptek));

	aiptek->irq = usb_alloc_urb(0);
	if (!aiptek->irq) {
		kfree(aiptek);
		return NULL;
	}
	// This used to be meaningful, when we had a matrix of
	// different models with statically-assigned different
	// features. Now we ask the tablet about everything.

	aiptek->features = aiptek_features;

	// Reset the tablet. The tablet boots up in 'SwitchtoMouse'
	// mode, which indicates relative coordinates. 'SwitchToTablet'
	// infers absolute coordinates. (Ergo, mice are inferred to be
	// relative-only devices, which is not true. A misnomer.)
	// The routine we use, aiptek_program_tablet, has been generalized
	// enough such that it's callable through the procfs interface.
	// This is why we use struct aiptek throughout.
	aiptek->usbdev = dev;
	aiptek->ifnum = ifnum;
	aiptek->pointer_mode = AIPTEK_POINTER_EITHER_MODE;
	aiptek->coordinate_mode = AIPTEK_COORDINATE_ABSOLUTE_MODE;
	aiptek->tool_mode = AIPTEK_TOOL_BUTTON_PEN_MODE;
	aiptek->xTilt = AIPTEK_TILT_DISABLE;
	aiptek->yTilt = AIPTEK_TILT_DISABLE;
	aiptek->jitterDelay = AIPTEK_JITTER_DELAY_DEFAULT;

#ifdef CONFIG_PROC_FS
	create_procfs_file(aiptek);
#endif

	aiptek_program_tablet(aiptek);

	aiptek->dev.evbit[0] |= BIT(EV_KEY)
	    | BIT(EV_ABS)
	    | BIT(EV_MSC);

	aiptek->dev.absbit[0] |= BIT(ABS_X)
	    | BIT(ABS_Y)
	    | BIT(ABS_PRESSURE)
	    | BIT(ABS_TILT_X)
	    | BIT(ABS_TILT_Y)
	    | BIT(ABS_MISC);

	aiptek->dev.relbit[0] |= BIT(REL_X)
	    | BIT(REL_Y)
	    | BIT(REL_MISC);

	// Set the macro keys up. They are discontiguous, so it's better
	// to set the bitmask this way.

	for (i = 0; i < sizeof (macroKeyEvents) / sizeof (macroKeyEvents[0]);
	     ++i) {
		set_bit(macroKeyEvents[i], aiptek->dev.keybit);
	}

	aiptek->dev.keybit[LONG(BTN_LEFT)] |= BIT(BTN_LEFT)
	    | BIT(BTN_RIGHT)
	    | BIT(BTN_MIDDLE);

	aiptek->dev.keybit[LONG(BTN_DIGI)] |= BIT(BTN_TOOL_PEN)
	    | BIT(BTN_TOOL_RUBBER)
	    | BIT(BTN_TOOL_PENCIL)
	    | BIT(BTN_TOOL_AIRBRUSH)
	    | BIT(BTN_TOOL_BRUSH)
	    | BIT(BTN_TOOL_MOUSE)
	    | BIT(BTN_TOUCH)
	    | BIT(BTN_STYLUS)
	    | BIT(BTN_STYLUS2);

	aiptek->dev.mscbit[0] = BIT(MSC_SERIAL);

	aiptek->dev.absmax[ABS_X] = aiptek->features->x_max;
	aiptek->dev.absmax[ABS_Y] = aiptek->features->y_max;
	aiptek->dev.absmax[ABS_PRESSURE] = aiptek->features->pressure_max;
	aiptek->dev.absmax[ABS_TILT_X] = AIPTEK_TILT_MAX;
	aiptek->dev.absmax[ABS_TILT_Y] = AIPTEK_TILT_MAX;
	aiptek->dev.absfuzz[ABS_X] = 0;
	aiptek->dev.absfuzz[ABS_Y] = 0;

	aiptek->dev.private = aiptek;
	aiptek->dev.open = aiptek_open;
	aiptek->dev.close = aiptek_close;

	aiptek->dev.name = aiptek->features->name;
	aiptek->dev.idbus = BUS_USB;
	aiptek->dev.idvendor = dev->descriptor.idVendor;
	aiptek->dev.idproduct = dev->descriptor.idProduct;
	aiptek->dev.idversion = dev->descriptor.bcdDevice;
	aiptek->usbdev = dev;

	endpoint = dev->config[0].interface[ifnum].altsetting[0].endpoint + 0;

	usb_fill_int_urb(aiptek->irq,
			 dev,
			 usb_rcvintpipe(dev, endpoint->bEndpointAddress),
			 aiptek->data,
			 aiptek->features->pktlen,
			 aiptek->features->irq, aiptek, endpoint->bInterval);

	input_register_device(&aiptek->dev);

	printk(KERN_INFO "input%d: %s on usb%d:%d.%d\n",
	       aiptek->dev.number,
	       aiptek->features->name, dev->bus->busnum, dev->devnum, ifnum);

	return aiptek;
}

static struct usb_driver aiptek_driver;

static void
aiptek_disconnect(struct usb_device *dev, void *ptr)
{
	struct aiptek *aiptek = ptr;
#ifdef CONFIG_PROC_FS
	destroy_procfs_file(aiptek);
#endif
	usb_unlink_urb(aiptek->irq);
	input_unregister_device(&aiptek->dev);
	usb_free_urb(aiptek->irq);
	kfree(aiptek);
}

static struct usb_driver aiptek_driver = {
	name:"aiptek",
	probe:aiptek_probe,
	disconnect:aiptek_disconnect,
	id_table:aiptek_ids,
};

static int __init
aiptek_init(void)
{
	usb_register(&aiptek_driver);
	info(DRIVER_VERSION ": " DRIVER_AUTHOR);
	info(DRIVER_DESC);
	return 0;
}

static void __exit
aiptek_exit(void)
{
	usb_deregister(&aiptek_driver);
}

module_init(aiptek_init);
module_exit(aiptek_exit);
