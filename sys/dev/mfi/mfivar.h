/*-
 * Copyright (c) 2006 IronPort Systems
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _MFIVAR_H
#define _MFIVAR_H

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * SCSI structures and definitions are used from here, but no linking
 * requirements are made to CAM.
 */
#include <cam/scsi/scsi_all.h>

struct mfi_hwcomms {
	uint32_t		hw_pi;
	uint32_t		hw_ci;
	uint32_t		hw_reply_q[1];
};

struct mfi_softc;

struct mfi_command {
	TAILQ_ENTRY(mfi_command) cm_link;
	struct mfi_softc	*cm_sc;
	union mfi_frame		*cm_frame;
	uint32_t		cm_frame_busaddr;
	struct mfi_sense	*cm_sense;
	uint32_t		cm_sense_busaddr;
	bus_dmamap_t		cm_dmamap;
	union mfi_sgl		*cm_sg;
	void			*cm_data;
	int			cm_len;
	int			cm_total_frame_size;
	int			cm_extra_frames;
	int			cm_flags;
#define MFI_CMD_MAPPED		(1<<0)
#define MFI_CMD_DATAIN		(1<<1)
#define MFI_CMD_DATAOUT		(1<<2)
#define MFI_CMD_COMPLETED	(1<<3)
#define MFI_CMD_POLLED		(1<<4)
#define MFI_ON_MFIQ_FREE	(1<<5)
#define MFI_ON_MFIQ_READY	(1<<6)
#define MFI_ON_MFIQ_BUSY	(1<<7)
#define MFI_ON_MFIQ_MASK	((1<<5)|(1<<6)|(1<<7))
	int			cm_aen_abort;
	void			(* cm_complete)(struct mfi_command *cm);
	void			*cm_private;
	void			*cm_private2;
};

struct mfi_ld {
	TAILQ_ENTRY(mfi_ld)	ld_link;
	device_t		ld_disk;
	struct mfi_ld_info	*ld_info;
	int			ld_id;
};

struct mfi_aen {
	TAILQ_ENTRY(mfi_aen) aen_link;
	struct proc			*p;
};

struct mfi_softc {
	device_t			mfi_dev;
	int				mfi_flags;
#define MFI_FLAGS_SG64		(1<<0)
#define MFI_FLAGS_QFRZN		(1<<1)
#define MFI_FLAGS_OPEN		(1<<2)

	struct mfi_hwcomms		*mfi_comms;
	TAILQ_HEAD(,mfi_command)	mfi_free;
	TAILQ_HEAD(,mfi_command)	mfi_ready;
	TAILQ_HEAD(,mfi_command)	mfi_busy;
	struct bio_queue_head		mfi_bioq;
	struct mfi_qstat		mfi_qstat[MFIQ_COUNT];

	struct resource			*mfi_regs_resource;
	bus_space_handle_t		mfi_bhandle;
	bus_space_tag_t			mfi_btag;
	int				mfi_regs_rid;

	bus_dma_tag_t			mfi_parent_dmat;
	bus_dma_tag_t			mfi_buffer_dmat;

	bus_dma_tag_t			mfi_comms_dmat;
	bus_dmamap_t			mfi_comms_dmamap;
	uint32_t			mfi_comms_busaddr;

	bus_dma_tag_t			mfi_frames_dmat;
	bus_dmamap_t			mfi_frames_dmamap;
	uint32_t			mfi_frames_busaddr;
	union mfi_frame			*mfi_frames;

	TAILQ_HEAD(,mfi_aen)		mfi_aen_pids;
	struct mfi_command		*mfi_aen_cm;
	uint32_t			mfi_aen_triggered;
	uint32_t			mfi_poll_waiting;
	struct selinfo			mfi_select;

	bus_dma_tag_t			mfi_sense_dmat;
	bus_dmamap_t			mfi_sense_dmamap;
	uint32_t			mfi_sense_busaddr;
	struct mfi_sense		*mfi_sense;

	struct resource			*mfi_irq;
	void				*mfi_intr;
	int				mfi_irq_rid;

	struct intr_config_hook		mfi_ich;
	eventhandler_tag		eh;

	/*
	 * Allocation for the command array.  Used as an indexable array to
	 * recover completed commands.
	 */
	struct mfi_command		*mfi_commands;
	/*
	 * How many commands were actually allocated
	 */
	int				mfi_total_cmds;
	/*
	 * How many commands the firmware can handle.  Also how big the reply
	 * queue is, minus 1.
	 */
	int				mfi_max_fw_cmds;
	/*
	 * Max number of S/G elements the firmware can handle
	 */
	int				mfi_max_fw_sgl;
	/*
	 * How many S/G elements we'll ever actually use 
	 */
	int				mfi_total_sgl;
	/*
	 * How many bytes a compound frame is, including all of the extra frames
	 * that are used for S/G elements.
	 */
	int				mfi_frame_size;
	/*
	 * How large an S/G element is.  Used to calculate the number of single
	 * frames in a command.
	 */
	int				mfi_sgsize;
	/*
	 * Max number of sectors that the firmware allows
	 */
	uint32_t			mfi_max_io;

	TAILQ_HEAD(,mfi_ld)		mfi_ld_tqh;
	eventhandler_tag		mfi_eh;
	dev_t				mfi_cdev;

};

extern int mfi_attach(struct mfi_softc *);
extern void mfi_free(struct mfi_softc *);
extern int mfi_shutdown(struct mfi_softc *);
extern void mfi_startio(struct mfi_softc *);
extern void mfi_disk_complete(struct bio *);
extern int mfi_dump_blocks(struct mfi_softc *, int id, uint64_t, void *, int);

#define MFIQ_ADD(sc, qname)					\
	do {							\
		struct mfi_qstat *qs;				\
								\
		qs = &(sc)->mfi_qstat[qname];			\
		qs->q_length++;					\
		if (qs->q_length > qs->q_max)			\
			qs->q_max = qs->q_length;		\
	} while (0)

#define MFIQ_REMOVE(sc, qname)	(sc)->mfi_qstat[qname].q_length--

#define MFIQ_INIT(sc, qname)					\
	do {							\
		sc->mfi_qstat[qname].q_length = 0;		\
		sc->mfi_qstat[qname].q_max = 0;			\
	} while (0)

#define MFIQ_COMMAND_QUEUE(name, index)					\
	static __inline void						\
	mfi_initq_ ## name (struct mfi_softc *sc)			\
	{								\
		TAILQ_INIT(&sc->mfi_ ## name);				\
		MFIQ_INIT(sc, index);					\
	}								\
	static __inline void						\
	mfi_enqueue_ ## name (struct mfi_command *cm)			\
	{								\
		int s = splbio();					\
		if ((cm->cm_flags & MFI_ON_MFIQ_MASK) != 0) {		\
			printf("command %p is on another queue, "	\
			    "flags = %#x\n", cm, cm->cm_flags);		\
			panic("command is on another queue");		\
		}							\
		TAILQ_INSERT_TAIL(&cm->cm_sc->mfi_ ## name, cm, cm_link); \
		cm->cm_flags |= MFI_ON_ ## index;			\
		MFIQ_ADD(cm->cm_sc, index);				\
		splx(s);						\
	}								\
	static __inline void						\
	mfi_requeue_ ## name (struct mfi_command *cm)			\
	{								\
		int s = splbio();					\
									\
		if ((cm->cm_flags & MFI_ON_MFIQ_MASK) != 0) {		\
			printf("command %p is on another queue, "	\
			    "flags = %#x\n", cm, cm->cm_flags);		\
			panic("command is on another queue");		\
		}							\
		TAILQ_INSERT_HEAD(&cm->cm_sc->mfi_ ## name, cm, cm_link); \
		cm->cm_flags |= MFI_ON_ ## index;			\
		MFIQ_ADD(cm->cm_sc, index);				\
		splx(s);						\
	}								\
	static __inline struct mfi_command *				\
	mfi_dequeue_ ## name (struct mfi_softc *sc)			\
	{								\
		struct mfi_command *cm;					\
		int s = splbio();					\
									\
		if ((cm = TAILQ_FIRST(&sc->mfi_ ## name)) != NULL) {	\
			if ((cm->cm_flags & MFI_ON_ ## index) == 0) {	\
				printf("command %p not in queue, "	\
				    "flags = %#x, bit = %#x\n", cm,	\
				    cm->cm_flags, MFI_ON_ ## index);	\
				panic("command not in queue");		\
			}						\
			TAILQ_REMOVE(&sc->mfi_ ## name, cm, cm_link);	\
			cm->cm_flags &= ~MFI_ON_ ## index;		\
			MFIQ_REMOVE(sc, index);				\
		}							\
		splx(s);						\
		return (cm);						\
	}								\
	static __inline void						\
	mfi_remove_ ## name (struct mfi_command *cm)			\
	{								\
		int s = splbio();					\
									\
		if ((cm->cm_flags & MFI_ON_ ## index) == 0) {		\
			printf("command %p not in queue, flags = %#x, " \
			    "bit = %#x\n", cm, cm->cm_flags,		\
			    MFI_ON_ ## index);				\
			panic("command not in queue");			\
		}							\
		TAILQ_REMOVE(&cm->cm_sc->mfi_ ## name, cm, cm_link);	\
		cm->cm_flags &= ~MFI_ON_ ## index;			\
		MFIQ_REMOVE(cm->cm_sc, index);				\
		splx(s);						\
	}								\
struct hack

MFIQ_COMMAND_QUEUE(free, MFIQ_FREE);
MFIQ_COMMAND_QUEUE(ready, MFIQ_READY);
MFIQ_COMMAND_QUEUE(busy, MFIQ_BUSY);

static __inline void
mfi_initq_bio(struct mfi_softc *sc)
{
	bioq_init(&sc->mfi_bioq);
	MFIQ_INIT(sc, MFIQ_BIO);
}

static __inline void
mfi_enqueue_bio(struct mfi_softc *sc, struct bio *bp)
{
	int s = splbio();
	bioq_insert_tail(&sc->mfi_bioq, bp);
	MFIQ_ADD(sc, MFIQ_BIO);
	splx(s);
}

static __inline struct bio *
mfi_dequeue_bio(struct mfi_softc *sc)
{
	struct bio *bp;
	int s = splbio();

	if ((bp = bioq_first(&sc->mfi_bioq)) != NULL) {
		bioq_remove(&sc->mfi_bioq, bp);
		MFIQ_REMOVE(sc, MFIQ_BIO);
	}
	splx(s);
	return (bp);
}

static __inline void
mfi_print_sense(struct mfi_softc *sc, void *sense)
{
	int error, key, asc, ascq;

	scsi_extract_sense((struct scsi_sense_data *)sense,
	    &error, &key, &asc, &ascq);
	device_printf(sc->mfi_dev, "sense error %d, sense_key %d, "
	    "asc %d, ascq %d\n", error, key, asc, ascq);
}


#define MFI_WRITE4(sc, reg, val)	bus_space_write_4((sc)->mfi_btag, \
	sc->mfi_bhandle, (reg), (val))
#define MFI_READ4(sc, reg)		bus_space_read_4((sc)->mfi_btag, \
	(sc)->mfi_bhandle, (reg))
#define MFI_WRITE2(sc, reg, val)	bus_space_write_2((sc)->mfi_btag, \
	sc->mfi_bhandle, (reg), (val))
#define MFI_READ2(sc, reg)		bus_space_read_2((sc)->mfi_btag, \
	(sc)->mfi_bhandle, (reg))
#define MFI_WRITE1(sc, reg, val)	bus_space_write_1((sc)->mfi_btag, \
	sc->mfi_bhandle, (reg), (val))
#define MFI_READ1(sc, reg)		bus_space_read_1((sc)->mfi_btag, \
	(sc)->mfi_bhandle, (reg))

MALLOC_DECLARE(M_MFIBUF);

#endif /* _MFIVAR_H */
