10 February 2002

1) This release reformats the files according to kernel linux-c
conventions.  If you do not have kernel linux-c mode configured
for your version of emacs, add the following to your .emacs file.

(defun linux-c-mode ()
  "C mode with adjusted defaults for use with the Linux kernel."
  (interactive)
  (c-mode)
  (c-set-style "K&R")
  (setq c-basic-offset 8))

Then you will be able to manually invoke linux-c-mode as a Meta-X
command.

The line 

/* -*- linux-c -*- */

that is now found at the top of each C source file automatically puts
the buffer into linux-c-mode when emacs opens the file for editing.

2) The following network ioctl have been removed

#define SAB8253XSETMAC	 	(SIOCDEVPRIVATE + 5 + 2)
#define SAB8253XGETMAC	 	(SIOCDEVPRIVATE + 5 + 3)

along with the PSEUDOMAC structure and references to this structure.

The following standard ioctls provide the same functionality.

#define	SIOCSIFHWADDR	0x8924		/* set hardware address 	*/
#define SIOCGIFHWADDR	0x8927		/* Get hardware address		*/

The 8253xmac tool has been removed.  To start the ASLX sab8253x
network interface, you should use a command like the following with
the substitutions appropriate to your environment.

ifconfig 8253x006 hw ether 000000030405 test1

You should substitute for 8253x006 the network interface that you are
using.  For 000000030405 you should substitute the MAC address that
you desire to use.  For test1 you should substitute the host name or
host IP address that you wish to use for this interface.

3) As many functions and non-local variables as possible have been
declared static in order to prevent name space polution.

4) Support has been added for programmatic selection of signaling
interface for the Aurora WAN multiserver 3500 series extension boards.

Ports associated with this cards can be programmed via the

#define ATIS_IOCSSIGMODE	_IOW(ATIS_MAGIC_IOC,6,unsigned int)

ioctl to have either RS232, RS422, RS449, RS530, V.35 or no (=off)
physical layer signaling.

The program 8253xmode.c has been added to demonstrate the use
of this set ioctl as well as the associated get ioctl.

#define ATIS_IOCGSIGMODE	_IOW(ATIS_MAGIC_IOC,7,unsigned int)

The program is invoked as follows.

8253xmode /dev/ttyS* mode

where mode is 232, 442, 449, 530, v.35 or off.

The proc file has been modified to show information associated with
the signaling state.

martillo@ylith:~ > cat /proc/tty/driver/auraserial
serinfo:2.01N driver:1.22
TTY MAJOR = 4, CUA MAJOR = 5, STTY MAJOR = 254.
128: port 0: sab82538: v3: chip 0: ATI 8520P: bus 2: slot 10: NR: close: NOPRG
129: port 1: sab82538: v3: chip 0: ATI 8520P: bus 2: slot 10: NR: openA: NOPRG
130: port 2: sab82538: v3: chip 0: ATI 8520P: bus 2: slot 10: NR: openA: NOPRG
131: port 3: sab82538: v3: chip 0: ATI 8520P: bus 2: slot 10: NR: close: NOPRG
132: port 4: sab82538: v3: chip 0: ATI 8520P: bus 2: slot 10: NR: close: NOPRG
133: port 5: sab82538: v3: chip 0: ATI 8520P: bus 2: slot 10: NR: openS: NOPRG
134: port 6: sab82538: v3: chip 0: ATI 8520P: bus 2: slot 10: NR: close: NOPRG
135: port 7: sab82538: v3: chip 0: ATI 8520P: bus 2: slot 10: NR: close: NOPRG
136: port 0: sab82538: v2: chip 0: ATI WANMS: bus 2: slot 11: NR: openA: RS232
137: port 1: sab82538: v2: chip 0: ATI WANMS: bus 2: slot 11: NR: openA: RS232
138: port 2: sab82538: v2: chip 0: ATI WANMS: bus 2: slot 11: NR: openA: RS232
139: port 3: sab82538: v2: chip 0: ATI WANMS: bus 2: slot 11: NR: close: RS232
140: port 4: sab82538: v2: chip 0: ATI WANMS: bus 2: slot 11: NR: close: RS232
141: port 5: sab82538: v2: chip 0: ATI WANMS: bus 2: slot 11: NR: close: RS232
142: port 6: sab82538: v2: chip 0: ATI WANMS: bus 2: slot 11: NR: close: RS232
143: port 7: sab82538: v2: chip 0: ATI WANMS: bus 2: slot 11: NR: close: RS232
144: port 0: sab82538: v2: chip 1: ATI WANMS: bus 2: slot 11: NR: openA: RS232
145: port 1: sab82538: v2: chip 1: ATI WANMS: bus 2: slot 11: NR: close: RS232
146: port 2: sab82538: v2: chip 1: ATI WANMS: bus 2: slot 11: NR: close: RS232
147: port 3: sab82538: v2: chip 1: ATI WANMS: bus 2: slot 11: NR: close: RS232
148: port 4: sab82538: v2: chip 1: ATI WANMS: bus 2: slot 11: NR: close: RS530
149: port 5: sab82538: v2: chip 1: ATI WANMS: bus 2: slot 11: NR: close: RS232
150: port 6: sab82538: v2: chip 1: ATI WANMS: bus 2: slot 11: NR: close: RS232
151: port 7: sab82538: v2: chip 1: ATI WANMS: bus 2: slot 11: NR: openA: RS232
152: port 0: sab82538: v2: chip 2: ATI WANMS: bus 2: slot 11: NR: openA: RS232
153: port 1: sab82538: v2: chip 2: ATI WANMS: bus 2: slot 11: NR: close: RS232
154: port 2: sab82538: v2: chip 2: ATI WANMS: bus 2: slot 11: NR: close: RS232
155: port 3: sab82538: v2: chip 2: ATI WANMS: bus 2: slot 11: NR: close: RS232
156: port 4: sab82538: v2: chip 2: ATI WANMS: bus 2: slot 11: NR: close: RS232
157: port 5: sab82538: v2: chip 2: ATI WANMS: bus 2: slot 11: NR: close: RS232
158: port 6: sab82538: v2: chip 2: ATI WANMS: bus 2: slot 11: NR: close: RS232
159: port 7: sab82538: v2: chip 2: ATI WANMS: bus 2: slot 11: NR: openA: RS232
160: port 0: sab82538: v2: chip 3: ATI WANMS: bus 2: slot 11: NR: close: RS232
161: port 1: sab82538: v2: chip 3: ATI WANMS: bus 2: slot 11: NR: close: RS232
162: port 2: sab82538: v2: chip 3: ATI WANMS: bus 2: slot 11: NR: close: RS232
163: port 3: sab82538: v2: chip 3: ATI WANMS: bus 2: slot 11: NR: close: RS232
164: port 4: sab82538: v2: chip 3: ATI WANMS: bus 2: slot 11: NR: close: RS232
165: port 5: sab82538: v2: chip 3: ATI WANMS: bus 2: slot 11: NR: close: RS232
166: port 6: sab82538: v2: chip 3: ATI WANMS: bus 2: slot 11: NR: close: RS232
167: port 7: sab82538: v2: chip 3: ATI WANMS: bus 2: slot 11: NR: close: RS232
168: port 0: sab82538: v2: chip 4: ATI WANMS: bus 2: slot 11: NR: openA: RS232
169: port 1: sab82538: v2: chip 4: ATI WANMS: bus 2: slot 11: NR: close: RS232
170: port 2: sab82538: v2: chip 4: ATI WANMS: bus 2: slot 11: NR: close: RS232
171: port 3: sab82538: v2: chip 4: ATI WANMS: bus 2: slot 11: NR: close: RS232
172: port 4: sab82538: v2: chip 4: ATI WANMS: bus 2: slot 11: NR: close: RS232
173: port 5: sab82538: v2: chip 4: ATI WANMS: bus 2: slot 11: NR: close: RS232
174: port 6: sab82538: v2: chip 4: ATI WANMS: bus 2: slot 11: NR: close: RS232
175: port 7: sab82538: v2: chip 4: ATI WANMS: bus 2: slot 11: NR: close: RS232
176: port 0: sab82538: v2: chip 5: ATI WANMS: bus 2: slot 11: NR: close: RS232
177: port 1: sab82538: v2: chip 5: ATI WANMS: bus 2: slot 11: NR: close: RS232
178: port 2: sab82538: v2: chip 5: ATI WANMS: bus 2: slot 11: NR: close: RS232
179: port 3: sab82538: v2: chip 5: ATI WANMS: bus 2: slot 11: NR: close: RS232
180: port 4: sab82538: v2: chip 5: ATI WANMS: bus 2: slot 11: NR: close: RS232
181: port 5: sab82538: v2: chip 5: ATI WANMS: bus 2: slot 11: NR: close: RS232
182: port 6: sab82538: v2: chip 5: ATI WANMS: bus 2: slot 11: NR: close: RS232
183: port 7: sab82538: v2: chip 5: ATI WANMS: bus 2: slot 11: NR: close: RS232
184: port 0: sab82538: v2: chip 6: ATI WANMS: bus 2: slot 11: NR: openA: RS232
185: port 1: sab82538: v2: chip 6: ATI WANMS: bus 2: slot 11: NR: close: RS232
186: port 2: sab82538: v2: chip 6: ATI WANMS: bus 2: slot 11: NR: close: RS232
187: port 3: sab82538: v2: chip 6: ATI WANMS: bus 2: slot 11: NR: close: RS232
188: port 4: sab82538: v2: chip 6: ATI WANMS: bus 2: slot 11: NR: close: RS232
189: port 5: sab82538: v2: chip 6: ATI WANMS: bus 2: slot 11: NR: close: RS232
190: port 6: sab82538: v2: chip 6: ATI WANMS: bus 2: slot 11: NR: close: RS232
191: port 7: sab82538: v2: chip 6: ATI WANMS: bus 2: slot 11: NR: close: RS232
192: port 0: sab82538: v2: chip 7: ATI WANMS: bus 2: slot 11: NR: close: RS232
193: port 1: sab82538: v2: chip 7: ATI WANMS: bus 2: slot 11: NR: close: RS232
194: port 2: sab82538: v2: chip 7: ATI WANMS: bus 2: slot 11: NR: close: RS232
195: port 3: sab82538: v2: chip 7: ATI WANMS: bus 2: slot 11: NR: close: RS232
196: port 4: sab82538: v2: chip 7: ATI WANMS: bus 2: slot 11: NR: close: RS232
197: port 5: sab82538: v2: chip 7: ATI WANMS: bus 2: slot 11: NR: close: RS232
198: port 6: sab82538: v2: chip 7: ATI WANMS: bus 2: slot 11: NR: close: RS232
199: port 7: sab82538: v2: chip 7: ATI WANMS: bus 2: slot 11: NR: openA: RS232

The above file indicates the minor device number, port number relative
to chip, chip type, chip version number, chip number (meaningful for
4X20 and multichannel servers), interface type, bus number, slot
number, port availability (AO = asynchronous only, NR = No
Restrictions, or NA = Not Available), status (closed, open
Synchronous, open Asynchronous, open Character, or open Network), and
signaling type (NOPRG = not selectable programmatically; other
possibilities inclue OFF, RS232, RS442, RS449, RS530 and V.35).

Note that by default ports come up in RS232 mode.

The following module parameter has been added.

MODULE_PARM(sab8253x_default_sp502_mode, "i");

The asynchronous TTY functionality can immediately be used without
extra configuration.  [Note that immediate use of the WMS3500 products
is possible because the default value of sab8253x_default_sp502_mode
is SP502_RS232_MODE (== 1).  If a different default mode is needed, it
can be set as options in the /etc/modules.conf file.  OFF = 0.
RS232 = 1, RS422 = 2, RS485 = 3, RS449 = 4, EIA530 = 5 and V.35 = 6, as
defined in 8253xioc.h.]

5) I added a readme.txt an an overview of the driver sab8253xov.txt
and the functional and design specifications (sab8253xfs.txt and
sab8253xds.txt) to the patch.  The functional and design
specifications were blindly exported to text from the html versions.

The documents are more nicely formated at 

http://www.telfordtools.com/sab8253x/sab8253xfs.html

and

http://www.telfordtools.com/sab8253x/sab8253xfs.html

