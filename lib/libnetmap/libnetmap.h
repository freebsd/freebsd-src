/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (C) 2018 Universita` di Pisa
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   1. Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef LIBNETMAP_H_
#define LIBNETMAP_H_
/* if thread-safety is not needed, define LIBNETMAP_NOTHREADSAFE before including
 * this file.
 */

/* NOTE: we include net/netmap_user.h without defining NETMAP_WITH_LIBS, which
 * is deprecated. If you still need it, please define NETMAP_WITH_LIBS and
 * include net/netmap_user.h before including this file.
 */
#include <net/netmap_user.h>

struct nmctx;
struct nmport_d;
struct nmem_d;

/*
 * A port open specification (portspec for brevity) has the following syntax
 * (square brackets delimit optional parts):
 *
 *     subsystem:vpname[mode][options]
 *
 *  The "subsystem" is denoted by a prefix, possibly followed by an identifier.
 *  There can be several kinds of subsystems, each one selected by a unique
 *  prefix.  Currently defined subsystems are:
 *
 *  netmap 		(no id allowed)
 *  			the standard subsystem
 *
 *  vale 		(followed by a possibly empty id)
 *  			the vpname is connected to a VALE switch identified by
 *  			the id (an empty id selects the default switch)
 *
 *  The "vpname" has the following syntax:
 *
 *     identifier			or
 *     identifier1{identifier2		or
 *     identifier1}identifier2
 *
 *  Identifiers are sequences of alphanumeric characters. The part that begins
 *  with either '{' or '}', when present, denotes a netmap pipe opened in the
 *  same memory region as the subsystem:indentifier1 port.
 *
 * The "mode" can be one of the following:
 *
 *	^		bind all host (sw) ring pairs
 *	^NN		bind individual host ring pair
 *	*		bind host and NIC ring pairs
 *	-NN		bind individual NIC ring pair
 *	@NN		open the port in the NN memory region
 *	a suffix starting with / and the following flags,
 *	in any order:
 *	x		exclusive access
 *	z		zero copy monitor (both tx and rx)
 *	t		monitor tx side (copy monitor)
 *	r		monitor rx side (copy monitor)
 *	R		bind only RX ring(s)
 *	T		bind only TX ring(s)
 *
 *  The "options" start at the first '@' character not followed by a number.
 *  Each option starts with '@' and has the following syntax:
 *
 *      option					(flag option)
 *      option=value				(single key option)
 *      option:key1=value1,key2=value2,...	(multi-key option)
 *
 *  For multi-key options, the keys can be assigned in any order, but they
 *  cannot be assigned more than once. It is not necessary to assign all the
 *  option keys: unmentioned keys will receive default values.  Some multi-key
 *  options define a default key and also accept the single-key syntax, by
 *  assigning the value to this key.
 *
 *  NOTE: Options may be silently ignored if the port is already open by some
 *  other process.
 *
 *  The currently available options are (default keys, when defined, are marked
 *  with '*'):
 *
 *  share (single-key)
 *  			open the port in the same memory region used by the
 *  			given port name (the port name must be given in
 *  			subsystem:vpname form)
 *
 *  conf  (multi-key)
 *  			specify the rings/slots numbers (effective only on
 *  			ports that are created by the open operation itself,
 *  			and ignored otherwise).
 *
 *			The keys are:
 *
 *  		       *rings		number of tx and rx rings
 *  			tx-rings	number of tx rings
 *  			rx-rings	number of rx rings
 *			host-rings	number of tx and rx host rings
 *  			host-tx-rings	number of host tx rings
 *  			host-rx-rings	number of host rx rings
 *  			slots		number of slots in each tx and rx
 *  					ring
 *  			tx-slots	number of slots in each tx ring
 *  			rx-slots	number of slots in each rx ring
 *
 *  			(more specific keys override the less specific ones)
 *			All keys default to zero if not assigned, and the
 *			corresponding value will be chosen by netmap.
 *
 *  extmem (multi-key)
 *			open the port in the memory region obtained by
 *			mmap()ing the given file.
 *
 *			The keys are:
 *
 *		       *file		the file to mmap
 *			if-num		number of pre-allocated netmap_if's
 *			if-size		size of each netmap_if
 *			ring-num	number of pre-allocated netmap_ring's
 *			ring-size	size of each netmap_ring
 *			buf-num		number of pre-allocated buffers
 *			buf-size	size of each buffer
 *
 *			file must be assigned. The other keys default to zero,
 *			causing netmap to take the corresponding values from
 *			the priv_{if,ring,buf}_{num,size} sysctls.
 *
 *  offset (multi-key)
 *			reserve (part of) the ptr fields as an offset field
 *			and write an initial offset into them.
 *
 *			The keys are:
 *
 *		        bits		number of bits of ptr to use
 *		       *initial		initial offset value
 *
 *		        initial must be assigned. If bits is omitted, it
 *		        defaults to the entire ptr field. The max offset is set
 *		        at the same value as the initial offset. Note that the
 *		        actual values may be increased by the kernel.
 *
 *		        This option is disabled by default (see
 *			nmport_enable_option() below)
 */


/* nmport manipulation */

/* struct nmport_d - describes a netmap port */
struct nmport_d {
	/* see net/netmap.h for the definition of these fields */
	struct nmreq_header hdr;
	struct nmreq_register reg;

	/* all the fields below should be considered read-only */

	/* if the same context is used throughout the program, d1->mem ==
	 * d2->mem iff d1 and d2 are using the memory region (i.e., zero
	 * copy is possible between the two ports)
	 */
	struct nmem_d *mem;

	/* the nmctx used when this nmport_d was created */
	struct nmctx *ctx;

	int register_done;	/* nmport_register() has been called */
	int mmap_done;		/* nmport_mmap() has been called */
	/* pointer to the extmem option contained in the hdr options, if any */
	struct nmreq_opt_extmem *extmem;

	/* the fields below are compatible with nm_open() */
	int fd;				/* "/dev/netmap", -1 if not open */
	struct netmap_if *nifp;		/* pointer to the netmap_if */
	uint16_t first_tx_ring;
	uint16_t last_tx_ring;
	uint16_t first_rx_ring;
	uint16_t last_rx_ring;
	uint16_t cur_tx_ring;		/* used by nmport_inject */
	uint16_t cur_rx_ring;

	/* LIFO list of cleanup functions (used internally) */
	struct nmport_cleanup_d *clist;
};

/* nmport_open - opens a port from a portspec
 * @portspec	the port opening specification
 *
 * If successful, the function returns a new nmport_d describing a netmap
 * port, opened according to the port specification, ready to be used for rx
 * and/or tx.
 *
 * The rings available for tx are in the [first_tx_ring, last_tx_ring]
 * interval, and similarly for rx. One or both intervals may be empty.
 *
 * When done using it, the nmport_d descriptor must be closed using
 * nmport_close().
 *
 * In case of error, NULL is returned, errno is set to some error, and an
 * error message is sent through the error() method of the current context.
 */
struct nmport_d * nmport_open(const char *portspec);

/* nport_close - close a netmap port
 * @d		the port we want to close
 *
 * Undoes the actions performed by the nmport_open that created d, then
 * frees the descriptor.
 */
void nmport_close(struct nmport_d *d);

/* nmport_inject - sends a packet
 * @d		the port through which we want to send
 * @buf		base address of the packet
 * @size	its size in bytes
 *
 * Sends a packet using the cur_tx_ring and updates the index
 * to use all available tx rings in turn. Note: the packet is copied.
 *
 * Returns 0 on success an -1 on error.
 */
int nmport_inject(struct nmport_d *d, const void *buf, size_t size);

/*
 * the functions below can be used to split the functionality of
 * nmport_open when special features (e.g., extra buffers) are needed
 *
 * The relation among the functions is as follows:
 *
 *				   |nmport_new
 * 		|nmport_prepare	 = |
 *		|		   |nmport_parse
 * nmport_open =|
 *		|		   |nmport_register
 *		|nmport_open_desc =|
 *				   |nmport_mmap
 *
 */

/* nmport_new - create a new nmport_d
 *
 * Creates a new nmport_d using the malloc() method of the current default
 * context. Returns NULL on error, setting errno to an error value.
 */
struct nmport_d *nmport_new(void);

/* nmport_parse - fills the nmport_d netmap-register request
 * @d		the nmport to be filled
 * @portspec	the port opening specification
 *
 * This function parses the portspec and initizalizes the @d->hdr and @d->reg
 * fields. It may need to allocate a list of options. If an extmem option is
 * found, it may also mmap() the corresponding file.
 *
 * It returns 0 on success. On failure it returns -1, sets errno to an error
 * value and sends an error message to the error() method of the context used
 * when @d was created. Moreover, *@d is left unchanged.
 */
int nmport_parse(struct nmport_d *d, const char *portspec);

/* nmport_register - registers the port with netmap
 * @d		the nmport to be registered
 *
 * This function obtains a netmap file descriptor and registers the port with
 * netmap. The @d->hdr and @d->reg data structures must have been previously
 * initialized (via nmport_parse() or otherwise).
 *
 * It returns 0 on success. On failure it returns -1, sets errno to an error
 * value and sends an error message to the error() method of the context used
 * when @d was created. Moreover, *@d is left unchanged.
 */
int nmport_register(struct nmport_d *);

/* nmport_mmap - maps the port resources into the process memory
 * @d		the nmport to be mapped
 *
 * The port must have been previously been registered using nmport_register.
 *
 * Note that if extmem is used (either via an option or by calling an
 * nmport_extmem_* function before nmport_register()), no new mmap() is issued.
 *
 * It returns 0 on success. On failure it returns -1, sets errno to an error
 * value and sends an error message to the error() method of the context used
 * when @d was created. Moreover, *@d is left unchanged.
 */
int nmport_mmap(struct nmport_d *);

/* the following functions undo the actions of nmport_new(), nmport_parse(),
 * nmport_register() and nmport_mmap(), respectively.
 */
void nmport_delete(struct nmport_d *);
void nmport_undo_parse(struct nmport_d *);
void nmport_undo_register(struct nmport_d *);
void nmport_undo_mmap(struct nmport_d *);

/* nmport_prepare - create a port descriptor, but do not open it
 * @portspec	the port opening specification
 *
 * This functions creates a new nmport_d and initializes it according to
 * @portspec. It is equivalent to nmport_new() followed by nmport_parse().
 *
 * It returns 0 on success. On failure it returns -1, sets errno to an error
 * value and sends an error message to the error() method of the context used
 * when @d was created. Moreover, *@d is left unchanged.
 */
struct nmport_d *nmport_prepare(const char *portspec);

/* nmport_open_desc - open an initialized port descriptor
 * @d		the descriptor we want to open
 *
 * Registers the port with netmap and maps the rings and buffers into the
 * process memory. It is equivalent to nmport_register() followed by
 * nmport_mmap().
 *
 * It returns 0 on success. On failure it returns -1, sets errno to an error
 * value and sends an error message to the error() method of the context used
 * when @d was created. Moreover, *@d is left unchanged.
 */
int nmport_open_desc(struct nmport_d *d);

/* the following functions undo the actions of nmport_prepare()
 * and nmport_open_desc(), respectively.
 */
void nmport_undo_prepare(struct nmport_d *);
void nmport_undo_open_desc(struct nmport_d *);

/* nmport_clone - copy an nmport_d
 * @d		the nmport_d we want to copy
 *
 * Copying an nmport_d by hand should be avoided, since adjustments are needed
 * and some part of the state cannot be easily duplicated. This function
 * creates a copy of @d in a safe way. The returned nmport_d contains
 * nmreq_header and nmreq_register structures equivalent to those contained in
 * @d, except for the option list, which is ignored. The returned nmport_d is
 * already nmport_prepare()d, but it must still be nmport_open_desc()ed. The
 * new nmport_d uses the same nmctx as @d.
 *
 * If extmem was used for @d, then @d cannot be nmport_clone()d until it has
 * been nmport_register()ed.
 *
 * In case of error, the function returns NULL, sets errno to an error value
 * and sends an error message to the nmctx error() method.
 */
struct nmport_d *nmport_clone(struct nmport_d *);

/* nmport_extmem - use extmem for this port
 * @d		the port we want to use the extmem for
 * @base	the base address of the extmem region
 * @size	the size in bytes of the extmem region
 *
 * the memory that contains the netmap ifs, rings and buffers is usually
 * allocated by netmap and later mmap()ed by the applications. It is sometimes
 * useful to reverse this process, by having the applications allocate some
 * memory (through mmap() or otherwise) and then let netmap use it.  The extmem
 * option can be used to implement this latter strategy. The option can be
 * passed through the portspec using the '@extmem:...' syntax, or
 * programmatically by calling nmport_extmem() or nmport_extmem_from_file()
 * between nmport_parse() and nmport_register() (or between nmport_prepare()
 * and nmport_open_desc()).
 *
 * It returns 0 on success. On failure it returns -1, sets errno to an error
 * value and sends an error message to the error() method of the context used
 * when @d was created. Moreover, *@d is left unchanged.
 */
int nmport_extmem(struct nmport_d *d, void *base, size_t size);

/* nmport_extmem_from_file - use the extmem obtained by mapping a file
 * @d		the port we want to use the extmem for
 * @fname	path of the file we want to map
 *
 * This works like nmport_extmem, but the extmem memory is obtained by
 * mmap()ping @fname. nmport_close() will also automatically munmap() the file.
 *
 * It returns 0 on success. On failure it returns -1, sets errno to an error
 * value and sends an error message to the error() method of the context used
 * when @d was created. Moreover, *@d is left unchanged.
 */
int nmport_extmem_from_file(struct nmport_d *d, const char *fname);

/* nmport_extmem_getinfo - opbtai a pointer to the extmem configuration
 * @d		the port we want to obtain the pointer from
 *
 * Returns a pointer to the nmreq_pools_info structure containing the
 * configuration of the extmem attached to port @d, or NULL if no extmem
 * is attached. This can be used to set the desired configuration before
 * registering the port, or to read the actual configuration after
 * registration.
 */
struct nmreq_pools_info* nmport_extmem_getinfo(struct nmport_d *d);


/* nmport_offset - use offsets for this port
 * @initial	the initial offset for all the slots
 * @maxoff	the maximum offset
 * @bits	the number of bits of slot->ptr to use for the offsets
 * @mingap	the minimum gap between offsets (in shared buffers)
 *
 * With this option the lower @bits bits of the ptr field in the netmap_slot
 * can be used to specify an offset into the buffer.  All offsets will be set
 * to the @initial value by netmap.
 *
 * The offset field can be read and updated using the bitmask found in
 * ring->offset_mask after a successful register.  netmap_user.h contains
 * some helper macros (NETMAP_ROFFSET, NETMAP_WOFFSET and NETMAP_BUF_OFFSET).
 *
 * For RX rings, the user writes the offset o in an empty slot before passing
 * it to netmap; then, netmap will write the incoming packet at an offset o' >=
 * o in the buffer. o' may be larger than o because of, e.g., alignment
 * constrains.  If o' > o netmap will also update the offset field in the slot.
 * Note that large offsets may cause the port to split the packet over several
 * slots, setting the NS_MOREFRAG flag accordingly.
 *
 * For TX rings, the user may prepare the packet to send at an offset o into
 * the buffer and write o in the offset field. Netmap will send the packets
 * starting o bytes in the buffer. Note that the address of the packet must
 * comply with any alignment constraints that the port may have, or the result
 * will be undefined. The user may read the alignment constraint in the new
 * ring->buf_align field.  It is also possible that empty slots already come
 * with a non-zero offset o specified in the offset field. In this case, the
 * user will have to write the packet at an offset o' >= o.
 *
 * The user must also declare the @maxoff offset that she is going to use. Any
 * offset larger than this will be truncated.
 *
 * The user may also declare a @mingap (ignored if zero) if she plans to use
 * offsets to share the same buffer among several slots. Netmap will guarantee
 * that it will never write more than @mingap bytes for each slot, irrespective
 * of the buffer length.
 */
int nmport_offset(struct nmport_d *d, uint64_t initial, uint64_t maxoff,
		uint64_t bits, uint64_t mingap);

/* enable/disable options
 *
 * These functions can be used to disable options that the application cannot
 * or doesn't want to handle, or to enable options that require special support
 * from the application and are, therefore, disabled by default. Disabled
 * options will cause an error if encountered during option parsing.
 *
 * If the option is unknown, nmport_disable_option is a NOP, while
 * nmport_enable_option returns -1 and sets errno to EOPNOTSUPP.
 *
 * These functions are not threadsafe and are meant to be used at the beginning
 * of the program.
 */
void nmport_disable_option(const char *opt);
int nmport_enable_option(const char *opt);

/* nmreq manipulation
 *
 * nmreq_header_init - initialize an nmreq_header
 * @hdr		the nmreq_header to initialize
 * @reqtype	the kind of netmap request
 * @body	the body of the request
 *
 * Initialize the nr_version, nr_reqtype and nr_body fields of *@hdr.
 * The other fields are set to zero.
 */
void nmreq_header_init(struct nmreq_header *hdr, uint16_t reqtype, void *body);

/*
 * These functions allow for finer grained parsing of portspecs.  They are used
 * internally by nmport_parse().
 */

/* nmreq_header_decode - initialize an nmreq_header
 * @ppspec:	(in/out) pointer to a pointer to the portspec
 * @hdr:	pointer to the nmreq_header to be initialized
 * @ctx:	pointer to the nmctx to use (for errors)
 *
 * This function fills the @hdr the nr_name field with the port name extracted
 * from *@pifname.  The other fields of *@hdr are unchanged. The @pifname is
 * updated to point at the first char past the port name.
 *
 * Returns 0 on success.  In case of error, -1 is returned with errno set to
 * EINVAL, @pifname is unchanged, *@hdr is also unchanged, and an error message
 * is sent through @ctx->error().
 */
int nmreq_header_decode(const char **ppspec, struct nmreq_header *hdr,
		struct nmctx *ctx);

/* nmreq_regiter_decode - initialize an nmreq_register
 * @pmode:	(in/out) pointer to a pointer to an opening mode
 * @reg:	pointer to the nmreq_register to be initialized
 * @ctx:	pointer to the nmctx to use (for errors)
 *
 * This function fills the nr_mode, nr_ringid, nr_flags and nr_mem_id fields of
 * the structure pointed by @reg, according to the opening mode specified by
 * *@pmode. The other fields of *@reg are unchanged.  The @pmode is updated to
 * point at the first char past the opening mode.
 *
 * If a '@' is encountered followed by something which is not a number, parsing
 * stops (without error) and @pmode is left pointing at the '@' char. The
 * nr_mode, nr_ringid and nr_flags fields are still updated, but nr_mem_id is
 * not touched and the interpretation of the '@' field is left to the caller.
 *
 * Returns 0 on success.  In case of error, -1 is returned with errno set to
 * EINVAL, @pmode is unchanged, *@reg is also unchanged, and an error message
 * is sent through @ctx->error().
 */
int nmreq_register_decode(const char **pmode, struct nmreq_register *reg,
		struct nmctx *ctx);

/* nmreq_options_decode - parse the "options" part of the portspec
 * @opt:	pointer to the option list
 * @parsers:	list of option parsers
 * @token:	token to pass to each parser
 * @ctx:	pointer to the nmctx to use (for errors and malloc/free)
 *
 * This function parses each option in @opt. Each option is matched (based on
 * the "option" prefix) to a corresponding parser in @parsers. The function
 * checks that the syntax is appropriate for the parser and it assigns all the
 * keys mentioned in the option. It then passes control to the parser, to
 * interpret the keys values.
 *
 * Returns 0 on success. In case of error, -1 is returned, errno is set to an
 * error value and a message is sent to @ctx->error(). The effects of partially
 * interpreted options may not be undone.
 */
struct nmreq_opt_parser;
int nmreq_options_decode(const char *opt, struct nmreq_opt_parser *parsers,
		void *token, struct nmctx *ctx);

struct nmreq_parse_ctx;
/* type of the option-parsers callbacks */
typedef int (*nmreq_opt_parser_cb)(struct nmreq_parse_ctx *);

#define NMREQ_OPT_MAXKEYS 16	/* max nr of recognized keys per option */

/* struct nmreq_opt_key - describes an option key */
struct nmreq_opt_key {
	const char *key;	/* the key name */
	int id;			/* its position in the parse context */
	unsigned int flags;
#define NMREQ_OPTK_ALLOWEMPTY 	(1U << 0) /* =value may be omitted */
#define NMREQ_OPTK_MUSTSET	(1U << 1) /* the key is mandatory */
#define NMREQ_OPTK_DEFAULT	(1U << 2) /* this is the default key */
};

/* struct nmreq_opt_parser - describes an option parser */
struct nmreq_opt_parser {
	const char *prefix;	/* matches one option prefix */
	nmreq_opt_parser_cb parse;	/* the parse callback */
	int default_key;	/* which option is the default if the
				   parser is multi-key (-1 if none) */
	int nr_keys;
	unsigned int flags;
#define NMREQ_OPTF_DISABLED     (1U << 0)
#define NMREQ_OPTF_ALLOWEMPTY	(1U << 1)	/* =value can be omitted */

	struct nmreq_opt_parser *next;	/* list of options */

	/* recognized keys */
	struct nmreq_opt_key keys[NMREQ_OPT_MAXKEYS];
} __attribute__((aligned(16)));

/* struct nmreq_parse_ctx - the parse context received by the parse callback */
struct nmreq_parse_ctx {
	struct nmctx *ctx;	/* the nmctx for errors and malloc/free */
	void *token;		/* the token passed to nmreq_options_parse */

	/* the value (i.e., the part after the = sign) of each recognized key
	 * is assigned to the corresponding entry in this array, based on the
	 * key id. Unassigned keys are left at NULL.
	 */
	const char *keys[NMREQ_OPT_MAXKEYS];
};

/* nmreq_get_mem_id - get the mem_id of the given port
 * @portname	pointer to a pointer to the portname
 * @ctx		pointer to the nmctx to use (for errors)
 *
 * *@portname must point to a substem:vpname porname, possibly followed by
 * something else.
 *
 * If successful, returns the mem_id of *@portname and moves @portname past the
 * subsystem:vpname part of the input. In case of error it returns -1, sets
 * errno to an error value and sends an error message to ctx->error().
 */
int32_t nmreq_get_mem_id(const char **portname, struct nmctx *ctx);

/* option list manipulation */
void nmreq_push_option(struct nmreq_header *, struct nmreq_option *);
void nmreq_remove_option(struct nmreq_header *, struct nmreq_option *);
struct nmreq_option *nmreq_find_option(struct nmreq_header *, uint32_t);
void nmreq_free_options(struct nmreq_header *);
const char* nmreq_option_name(uint32_t);
#define nmreq_foreach_option(h_, o_) \
	for ((o_) = (struct nmreq_option *)((uintptr_t)((h_)->nr_options));\
	     (o_) != NULL;\
	     (o_) = (struct nmreq_option *)((uintptr_t)((o_)->nro_next)))

/* nmctx manipulation */

/* the nmctx serves a few purposes:
 *
 * - maintain a list of all memory regions open by the program, so that two
 *   ports that are using the same region (as identified by the mem_id) will
 *   point to the same nmem_d instance.
 *
 * - allow the user to specify how to lock accesses to the above list, if
 *   needed (lock() callback)
 *
 * - allow the user to specify how error messages should be delivered (error()
 *   callback)
 *
 * - select the verbosity of the library (verbose field); if verbose==0, no
 *   errors are sent to the error() callback
 *
 * - allow the user to override the malloc/free functions used by the library
 *   (malloc() and free() callbacks)
 *
 */
typedef void  (*nmctx_error_cb)(struct nmctx *, const char *);
typedef void *(*nmctx_malloc_cb)(struct nmctx *,size_t);
typedef void  (*nmctx_free_cb)(struct nmctx *,void *);
typedef void  (*nmctx_lock_cb)(struct nmctx *, int);

struct nmctx {
	int verbose;
	nmctx_error_cb 	error;
	nmctx_malloc_cb	malloc;
	nmctx_free_cb	free;
	nmctx_lock_cb	lock;

	struct nmem_d  *mem_descs;
};

/* nmctx_get - obtain a pointer to the current default context */
struct nmctx *nmctx_get(void);

/* nmctx_set_default - change the default context
 * @ctx		pointer to the new context
 *
 * Returns a pointer to the previous default context.
 */
struct nmctx *nmctx_set_default(struct nmctx *ctx);

/* internal functions and data structures */

/* struct nmem_d - describes a memory region currently used */
struct nmem_d {
	uint16_t mem_id;	/* the region netmap identifier */
	int refcount;		/* how many nmport_d's point here */
	void *mem;		/* memory region base address */
	size_t size;		/* memory region size */
	int is_extmem;		/* was it obtained via extmem? */

	/* pointers for the circular list implementation.
	 * The list head is the mem_descs filed in the nmctx
	 */
	struct nmem_d *next;
	struct nmem_d *prev;
};

/* a trick to force the inclusion of libpthread only if requested. If
 * LIBNETMAP_NOTHREADSAFE is defined, no pthread symbol is imported.
 *
 * There is no need to actually call this function: the ((used)) attribute is
 * sufficient to include it in the image.
 */
static  __attribute__((used)) void libnetmap_init(void)
{
#ifndef LIBNETMAP_NOTHREADSAFE
	extern int nmctx_threadsafe;
	/* dummy assignment to link-in the nmctx-pthread.o object.  The proper
	 * inizialization is performed only once in the library constructor
	 * defined there.
	 */
	nmctx_threadsafe = 1;
#endif /* LIBNETMAP_NOTHREADSAFE */
}

/* nmctx_set_threadsafe - install a threadsafe default context
 *
 * called by the constructor in nmctx-pthread.o to initialize a lock and install
 * the lock() callback in the default context.
 */
void nmctx_set_threadsafe(void);

/* nmctx_ferror - format and send an error message */
void nmctx_ferror(struct nmctx *, const char *, ...);
/* nmctx_malloc - allocate memory */
void *nmctx_malloc(struct nmctx *, size_t);
/* nmctx_free - free memory allocated via nmctx_malloc */
void nmctx_free(struct nmctx *, void *);
/* nmctx_lock - lock the list of nmem_d */
void nmctx_lock(struct nmctx *);
/* nmctx_unlock - unlock the list of nmem_d */
void nmctx_unlock(struct nmctx *);

#endif /* LIBNETMAP_H_ */
