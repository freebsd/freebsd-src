/*
 * dz.h: Serial port driver for DECStations equiped 
 *       with the DZ chipset.
 *
 * Copyright (C) 1998 Olivier A. D. Lebaillif 
 *             
 * Email: olivier.lebaillif@ifrsys.com
 *
 */
#ifndef DZ_SERIAL_H
#define DZ_SERIAL_H

#define SERIAL_MAGIC 0x5301

/*
 * Definitions for the Control and Status Received.
 */
#define DZ_TRDY        0x8000                 /* Transmitter empty */
#define DZ_TIE         0x4000                 /* Transmitter Interrupt Enable */
#define DZ_RDONE       0x0080                 /* Receiver data ready */
#define DZ_RIE         0x0040                 /* Receive Interrupt Enable */
#define DZ_MSE         0x0020                 /* Master Scan Enable */
#define DZ_CLR         0x0010                 /* Master reset */
#define DZ_MAINT       0x0008                 /* Loop Back Mode */

/*
 * Definitions for the Received buffer. 
 */
#define DZ_RBUF_MASK   0x00FF                 /* Data Mask in the Receive Buffer */
#define DZ_LINE_MASK   0x0300                 /* Line Mask in the Receive Buffer */
#define DZ_DVAL        0x8000                 /* Valid Data indicator */
#define DZ_OERR        0x4000                 /* Overrun error indicator */
#define DZ_FERR        0x2000                 /* Frame error indicator */
#define DZ_PERR        0x1000                 /* Parity error indicator */

#define LINE(x) (x & DZ_LINE_MASK) >> 8       /* Get the line number from the input buffer */
#define UCHAR(x) (unsigned char)(x & DZ_RBUF_MASK)

/*
 * Definitions for the Transmit Register.
 */
#define DZ_LINE_KEYBOARD 0x0001
#define DZ_LINE_MOUSE    0x0002
#define DZ_LINE_MODEM    0x0004
#define DZ_LINE_PRINTER  0x0008

#define DZ_MODEM_DTR     0x0400               /* DTR for the modem line (2) */

/*
 * Definitions for the Modem Status Register.
 */
#define DZ_MODEM_DSR     0x0200               /* DSR for the modem line (2) */

/*
 * Definitions for the Transmit Data Register.
 */
#define DZ_BRK0          0x0100               /* Break assertion for line 0 */
#define DZ_BRK1          0x0200               /* Break assertion for line 1 */
#define DZ_BRK2          0x0400               /* Break assertion for line 2 */
#define DZ_BRK3          0x0800               /* Break assertion for line 3 */

/*
 * Definitions for the Line Parameter Register.
 */
#define DZ_KEYBOARD      0x0000               /* line 0 = keyboard */
#define DZ_MOUSE         0x0001               /* line 1 = mouse */
#define DZ_MODEM         0x0002               /* line 2 = modem */
#define DZ_PRINTER       0x0003               /* line 3 = printer */

#define DZ_CSIZE         0x0018               /* Number of bits per byte (mask) */
#define DZ_CS5           0x0000               /* 5 bits per byte */
#define DZ_CS6           0x0008               /* 6 bits per byte */
#define DZ_CS7           0x0010               /* 7 bits per byte */
#define DZ_CS8           0x0018               /* 8 bits per byte */

#define DZ_CSTOPB        0x0020               /* 2 stop bits instead of one */ 

#define DZ_PARENB        0x0040               /* Parity enable */
#define DZ_PARODD        0x0080               /* Odd parity instead of even */

#define DZ_CBAUD         0x0E00               /* Baud Rate (mask) */
#define DZ_B50           0x0000
#define DZ_B75           0x0100
#define DZ_B110          0x0200
#define DZ_B134          0x0300
#define DZ_B150          0x0400
#define DZ_B300          0x0500
#define DZ_B600          0x0600
#define DZ_B1200         0x0700 
#define DZ_B1800         0x0800
#define DZ_B2000         0x0900
#define DZ_B2400         0x0A00
#define DZ_B3600         0x0B00
#define DZ_B4800         0x0C00
#define DZ_B7200         0x0D00
#define DZ_B9600         0x0E00

#define DZ_CREAD         0x1000               /* Enable receiver */
#define DZ_RXENAB        0x1000               /* enable receive char */
/*
 * Addresses for the DZ registers
 */
#define DZ_CSR       0x00            /* Control and Status Register */
#define DZ_RBUF      0x08            /* Receive Buffer */
#define DZ_LPR       0x08            /* Line Parameters Register */
#define DZ_TCR       0x10            /* Transmitter Control Register */
#define DZ_MSR       0x18            /* Modem Status Register */
#define DZ_TDR       0x18            /* Transmit Data Register */


#define DZ_NB_PORT 4

#define DZ_XMIT_SIZE   4096                 /* buffer size */
#define WAKEUP_CHARS   DZ_XMIT_SIZE/4

#define DZ_EVENT_WRITE_WAKEUP   0

#ifndef MIN
#define MIN(a,b)        ((a) < (b) ? (a) : (b))

#define DZ_INITIALIZED       0x80000000 /* Serial port was initialized */
#define DZ_CALLOUT_ACTIVE    0x40000000 /* Call out device is active */
#define DZ_NORMAL_ACTIVE     0x20000000 /* Normal device is active */
#define DZ_BOOT_AUTOCONF     0x10000000 /* Autoconfigure port on bootup */
#define DZ_CLOSING           0x08000000 /* Serial port is closing */
#define DZ_CTS_FLOW          0x04000000 /* Do CTS flow control */
#define DZ_CHECK_CD          0x02000000 /* i.e., CLOCAL */

#define DZ_CLOSING_WAIT_INF  0
#define DZ_CLOSING_WAIT_NONE 65535

#define DZ_SPLIT_TERMIOS   0x0008 /* Separate termios for dialin/callout */
#define DZ_SESSION_LOCKOUT 0x0100 /* Lock out cua opens based on session */
#define DZ_PGRP_LOCKOUT    0x0200 /* Lock out cua opens based on pgrp */

struct dz_serial {
  unsigned                port;                /* base address for the port */
  int                     type;
  int                     flags; 
  int                     baud_base;
  int                     blocked_open;
  unsigned short          close_delay;
  unsigned short          closing_wait;
  unsigned short          line;                /* port/line number */
  unsigned short          cflags;              /* line configuration flag */
  unsigned short          x_char;              /* xon/xoff character */
  unsigned short          read_status_mask;    /* mask for read condition */
  unsigned short          ignore_status_mask;  /* mask for ignore condition */
  unsigned long           event;               /* mask used in BH */
  unsigned char           *xmit_buf;           /* Transmit buffer */
  int                     xmit_head;           /* Position of the head */
  int                     xmit_tail;           /* Position of the tail */
  int                     xmit_cnt;            /* Count of the chars in the buffer */
  int                     count;               /* indicates how many times it has been opened */
  int                     magic;

  struct async_icount     icount;              /* keep track of things ... */
  struct tty_struct       *tty;                /* tty associated */
  struct tq_struct        tqueue;              /* Queue for BH */
  struct tq_struct        tqueue_hangup;
  struct termios          normal_termios;
  struct termios          callout_termios;
  wait_queue_head_t       open_wait;
  wait_queue_head_t       close_wait;

  long                    session;             /* Session of opening process */
  long                    pgrp;                /* pgrp of opening process */

  unsigned char           is_console;          /* flag indicating a serial console */
  unsigned char           is_initialized;
};

static struct dz_serial multi[DZ_NB_PORT];    /* Four serial lines in the DZ chip */
static struct dz_serial *dz_console;
static struct tty_driver serial_driver, callout_driver;

static struct tty_struct *serial_table[DZ_NB_PORT];
static struct termios *serial_termios[DZ_NB_PORT];
static struct termios *serial_termios_locked[DZ_NB_PORT];

static int serial_refcount;

/*
 * tmp_buf is used as a temporary buffer by serial_write.  We need to
 * lock it in case the copy_from_user blocks while swapping in a page,
 * and some other program tries to do a serial write at the same time.
 * Since the lock will only come under contention when the system is
 * swapping and available memory is low, it makes sense to share one
 * buffer across all the serial ports, since it significantly saves
 * memory if large numbers of serial ports are open.
 */
static unsigned char *tmp_buf;
static DECLARE_MUTEX(tmp_buf_sem);

static char *dz_name = "DECstation DZ serial driver version ";
static char *dz_version = "1.02";

static inline unsigned short dz_in (struct dz_serial *, unsigned);
static inline void dz_out (struct dz_serial *, unsigned, unsigned short);

static inline void dz_sched_event (struct dz_serial *, int);
static inline void receive_chars (struct dz_serial *);
static inline void transmit_chars (struct dz_serial *);
static inline void check_modem_status (struct dz_serial *);

static void dz_stop (struct tty_struct *);
static void dz_start (struct tty_struct *);
static void dz_interrupt (int, void *, struct pt_regs *);
static void do_serial_bh (void);
static void do_softint (void *);
static void do_serial_hangup (void *);
static void change_speed (struct dz_serial *);
static void dz_flush_chars (struct tty_struct *);
static void dz_console_print (struct console *, const char *, unsigned int);
static void dz_flush_buffer (struct tty_struct *);
static void dz_throttle (struct tty_struct *);
static void dz_unthrottle (struct tty_struct *);
static void dz_send_xchar (struct tty_struct *, char);
static void shutdown (struct dz_serial *);
static void send_break (struct dz_serial *, int);
static void dz_set_termios (struct tty_struct *, struct termios *);
static void dz_close (struct tty_struct *, struct file *);
static void dz_hangup (struct tty_struct *);
static void show_serial_version (void);

static int dz_write (struct tty_struct *, int, const unsigned char *, int);
static int dz_write_room (struct tty_struct *);
static int dz_chars_in_buffer (struct tty_struct *);
static int startup (struct dz_serial *);
static int get_serial_info (struct dz_serial *, struct serial_struct *);
static int set_serial_info (struct dz_serial *, struct serial_struct *);
static int get_lsr_info (struct dz_serial *, unsigned int *);
static int dz_ioctl (struct tty_struct *, struct file *, unsigned int, unsigned long);
static int block_til_ready (struct tty_struct *, struct file *, struct dz_serial *);
static int dz_open (struct tty_struct *, struct file *);

#ifdef MODULE
int init_module (void)
void cleanup_module (void)
#endif

#endif

#endif /* DZ_SERIAL_H */
