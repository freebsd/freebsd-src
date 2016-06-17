/* Random defines and structures for the HP Lance driver.
 * Copyright (C) 05/1998 Peter Maydell <pmaydell@chiark.greenend.org.uk>
 * Based on the Sun Lance driver and the NetBSD HP Lance driver
 */

/* Registers */
struct hplance_reg
{
        u_char pad0;
        volatile u_char id;                       /* DIO register: ID byte */
        u_char pad1;
        volatile u_char status;                   /* DIO register: interrupt enable */
};

/* Control and status bits for the hplance->status register */
#define LE_IE 0x80                                /* interrupt enable */
#define LE_IR 0x40                                /* interrupt requested */
#define LE_LOCK 0x08                              /* lock status register */
#define LE_ACK 0x04                               /* ack of lock */
#define LE_JAB 0x02                               /* loss of tx clock (???) */
/* We can also extract the IPL from the status register with the standard
 * DIO_IPL(hplance) macro, or using dio_scodetoipl()
 */

/* These are the offsets for the DIO regs (hplance_reg), lance_ioreg,
 * memory and NVRAM:
 */
#define HPLANCE_IDOFF 0                           /* board baseaddr, struct hplance_reg */
#define HPLANCE_REGOFF 0x4000                     /* struct lance_regs */
#define HPLANCE_MEMOFF 0x8000                     /* struct lance_init_block */
#define HPLANCE_NVRAMOFF 0xC008                   /* etheraddress as one *nibble* per byte */
