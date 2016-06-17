/*
 *	linux/arch/alpha/kernel/err_impl.h
 *
 *	Copyright (C) 2000 Jeff Wiedemeier (Compaq Computer Corporation)
 *
 *	Contains declarations and macros to support Alpha error handling
 * 	implementations.
 */

/*
 * SCB Vector definitions
 */
#define SCB_Q_SYSERR	0x620
#define SCB_Q_PROCERR	0x630
#define SCB_Q_SYSMCHK	0x660
#define SCB_Q_PROCMCHK	0x670
#define SCB_Q_SYSEVENT	0x680

/*
 * Disposition definitions for logout frame parser
 */
#define MCHK_DISPOSITION_UNKNOWN_ERROR		0x00
#define MCHK_DISPOSITION_REPORT			0x01
#define MCHK_DISPOSITION_DISMISS		0x02

/*
 * Error Log definitions
 */
/*
 * Types
 */

#define EL_CLASS__TERMINATION		(0)
#  define EL_TYPE__TERMINATION__TERMINATION		(0)
#define EL_CLASS__HEADER		(5)
#  define EL_TYPE__HEADER__SYSTEM_ERROR_FRAME		(1)
#  define EL_TYPE__HEADER__SYSTEM_EVENT_FRAME		(2)
#  define EL_TYPE__HEADER__HALT_FRAME			(3)
#  define EL_TYPE__HEADER__LOGOUT_FRAME			(19)
#define EL_CLASS__GENERAL_NOTIFICATION	(9)
#define EL_CLASS__PCI_ERROR_FRAME	(11)
#define EL_CLASS__REGATTA_FAMILY	(12)
#  define EL_TYPE__REGATTA__PROCESSOR_ERROR_FRAME	(1)
#  define EL_TYPE__REGATTA__SYSTEM_ERROR_FRAME		(2)
#  define EL_TYPE__REGATTA__ENVIRONMENTAL_FRAME		(3)
#  define EL_TYPE__REGATTA__TITAN_PCHIP0_EXTENDED	(8)
#  define EL_TYPE__REGATTA__TITAN_PCHIP1_EXTENDED	(9)
#  define EL_TYPE__REGATTA__TITAN_MEMORY_EXTENDED	(10)
#  define EL_TYPE__REGATTA__PROCESSOR_DBL_ERROR_HALT	(11)
#  define EL_TYPE__REGATTA__SYSTEM_DBL_ERROR_HALT	(12)
#define EL_CLASS__PAL                   (14)
#  define EL_TYPE__PAL__LOGOUT_FRAME                    (1)
#  define EL_TYPE__PAL__EV7_PROCESSOR			(4)
#  define EL_TYPE__PAL__EV7_ZBOX			(5)
#  define EL_TYPE__PAL__EV7_RBOX			(6)
#  define EL_TYPE__PAL__EV7_IO				(7)

union el_timestamp {
	struct {
		u8 second;
		u8 minute;
		u8 hour;
		u8 day;
		u8 month;
		u8 year;
	} b;
	u64 as_int;
};

struct el_subpacket {
	u16 length;		/* length of header (in bytes)	*/
	u16 class;		/* header class and type...   	*/
	u16 type;		/* ...determine content     	*/
	u16 revision;		/* header revision 		*/
	union {
		struct {	/* Class 5, Type 1 - System Error	*/
			u32 frame_length;
			u32 frame_packet_count;			
		} sys_err;			
		struct {	/* Class 5, Type 2 - System Event 	*/
			union el_timestamp timestamp;
			u32 frame_length;
			u32 frame_packet_count;			
		} sys_event;
		struct {	/* Class 5, Type 3 - Double Error Halt	*/
			u16 halt_code;
			u16 reserved;
			union el_timestamp timestamp;
			u32 frame_length;
			u32 frame_packet_count;
		} err_halt;
		struct {	/* Clasee 5, Type 19 - Logout Frame Header */
			u32 frame_length;
			u32 frame_flags;
			u32 cpu_offset;	
			u32 system_offset;
		} logout_header;
		struct {	/* Class 12 - Regatta			*/
			u64 cpuid;
			u64 data_start[1];
		} regatta_frame;
		struct {	/* Raw 				        */
			u64 data_start[1];
		} raw;
	} by_type;
};

struct el_subpacket_annotation {
	struct el_subpacket_annotation *next;
	u16 class;
	u16 type;
	u16 revision;
	char *description;
	char **annotation;
};
#define SUBPACKET_ANNOTATION(c, t, r, d, a) {NULL, (c), (t), (r), (d), (a)}

struct el_subpacket_handler {
	struct el_subpacket_handler *next;
	u16 class;
	struct el_subpacket *(*handler)(struct el_subpacket *);
};
#define SUBPACKET_HANDLER_INIT(c, h) {NULL, (c), (h)}

/*
 * Extract a field from a register given it's name. defines
 * for the LSB (__S - shift count) and bitmask (__M) are required
 */
#define EXTRACT(u, f) (((u) >> f##__S) & f##__M)

/*
 * err_common.c
 */
extern char *err_print_prefix;

extern void mchk_dump_mem(void *, size_t, char **);
extern void mchk_dump_logout_frame(struct el_common *);
extern void ev7_register_error_handlers(void);
extern void ev7_machine_check(u64, u64, struct pt_regs *);
extern void ev6_register_error_handlers(void);
extern int ev6_process_logout_frame(struct el_common *, int);
extern void ev6_machine_check(u64, u64, struct pt_regs *);
extern struct el_subpacket *el_process_subpacket(struct el_subpacket *);
extern void el_annotate_subpacket(struct el_subpacket *);
extern void cdl_check_console_data_log(void);
extern int cdl_register_subpacket_annotation(struct el_subpacket_annotation *);
extern int cdl_register_subpacket_handler(struct el_subpacket_handler *);

/*
 * err_marvel.c
 */
extern void marvel_machine_check(u64, u64, struct pt_regs *);
extern void marvel_register_error_handlers(void);

/*
 * err_titan.c
 */
extern int titan_process_logout_frame(struct el_common *, int);
extern void titan_machine_check(u64, u64, struct pt_regs *);
extern void titan_register_error_handlers(void);
extern int privateer_process_logout_frame(struct el_common *, int);
extern void privateer_machine_check(u64, u64, struct pt_regs *);
