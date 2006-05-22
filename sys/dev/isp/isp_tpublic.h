/* $FreeBSD$ */
/*-
 * Qlogic ISP Host Adapter Public Target Interface Structures && Routines
 *
 * Copyright (c) 1997-2006 by Matthew Jacob
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice immediately at the beginning of the file, without modification,
 *    this list of conditions, and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
/*
 * Host Adapter Public Target Interface Structures && Routines
 */

#ifndef    _ISP_TPUBLIC_H
#define    _ISP_TPUBLIC_H    1

/*
 * Action codes set by the MD target driver for
 * the external layer to figure out what to do with.
 */
typedef enum {
    QOUT_HBA_REG=0,     /* the argument is a pointer to a hba_register_t */
    QOUT_ENABLE,        /* the argument is a pointer to a enadis_t */
    QOUT_DISABLE,       /* the argument is a pointer to a enadis_t */
    QOUT_TMD_START,     /* the argument is a pointer to a tmd_cmd_t */
    QOUT_TMD_DONE,      /* the argument is a pointer to a tmd_cmd_t */
    QOUT_NOTIFY,        /* the argument is a pointer to a tmd_notify_t */
    QOUT_HBA_UNREG      /* the argument is a pointer to a hba_register_t */
} tact_e;

/*
 * Action codes set by the external layer for the
 * MD driver to figure out what to do with.
 */
typedef enum {
    QIN_HBA_REG=99,     /* the argument is a pointer to a hba_register_t */
    QIN_ENABLE,         /* the argument is a pointer to a enadis_t */
    QIN_DISABLE,        /* the argument is a pointer to a enadis_t */
    QIN_TMD_CONT,       /* the argument is a pointer to a tmd_cmd_t */
    QIN_TMD_FIN,        /* the argument is a pointer to a tmd_cmd_t */
    QIN_NOTIFY_ACK,     /* the argument is a pointer to a tmd_notify_t */
    QIN_HBA_UNREG,      /* the argument is a pointer to a hba_register_t */
} qact_e;

/*
 * This structure is used to register to other software modules the
 * binding of an HBA identifier, driver name and instance and the
 * lun width capapbilities of this target driver. It's up to each
 * platform to figure out how it wants to do this, but a typical
 * sequence would be for the MD layer to find some external module's
 * entry point and start by sending a QOUT_HBA_REG with info filled
 * in, and the external module to call back with a QIN_HBA_REG that
 * passes back the corresponding information.
 */
#define    QR_VERSION    10
typedef struct {
    void *                  r_identity;
    void                    (*r_action)(qact_e, void *);
    char                    r_name[8];
    int                     r_inst;
    int                     r_version;
    enum { R_FC, R_SCSI }   r_type;
} hba_register_t;

/*
 * Notify structure
 */
typedef enum {
    NT_ABORT_TASK=0x1000,
    NT_ABORT_TASK_SET,
    NT_CLEAR_ACA,
    NT_CLEAR_TASK_SET,
    NT_LUN_RESET,
    NT_TARGET_RESET,
    NT_BUS_RESET,
    NT_LIP_RESET,
    NT_LINK_UP,
    NT_LINK_DOWN,
    NT_LOGOUT,
    NT_HBA_RESET
} tmd_ncode_t;

typedef struct tmd_notify {
    void *      nt_hba;         /* HBA tag */
    uint64_t    nt_iid;         /* inititator id */
    uint64_t    nt_tgt;         /* target id */
    uint16_t    nt_lun;         /* logical unit */
    uint16_t    nt_padding;     /* padding */
    uint32_t    nt_tagval;      /* tag value */
    tmd_ncode_t nt_ncode;       /* action */
    void *      nt_lreserved;
    void *      nt_hreserved;
} tmd_notify_t;
#define LUN_ANY     0xffff
#define TGT_ANY     ((uint64_t) -1)
#define INI_ANY     ((uint64_t) -1)
#define TAG_ANY     0
#define MATCH_TMD(tmd, iid, lun, tag)                   \
    (                                                   \
        (tmd) &&                                        \
        (iid == INI_ANY || iid == tmd->cd_iid) &&       \
        (lun == LUN_ANY || lun == tmd->cd_lun) &&       \
        (tag == TAG_ANY || tag == tmd->cd_tagval)       \
    )

/*
 * A word about ENABLE/DISABLE: the argument is a pointer to a enadis_t
 * with en_hba, en_iid, en_chan, en_tgt and en_lun filled out.
 *
 * If an error occurs in either enabling or disabling the described lun
 * cd_error is set with an appropriate non-zero value.
 */
typedef struct {
    void *          en_private;     /* for outer layer usage */
    void *          en_hba;         /* HBA tag */
    uint64_t        en_iid;         /* initiator ID */
    uint64_t        en_tgt;         /* target id */
    uint16_t        en_lun;         /* logical unit */
    uint16_t        en_chan;        /* channel on card */
    int             en_error;
} enadis_t;

/*
 * Suggested Software Target Mode Command Handling structure.
 *
 * A note about terminology:
 *
 *   MD stands for "Machine Dependent".
 *
 *    This driver is structured in three layers: Outer MD, core, and inner MD.
 *    The latter also is bus dependent (i.e., is cognizant of PCI bus issues
 *    as well as platform issues).
 *
 *
 *   "Outer Layer" means "Other Module"
 *
 *    Some additional module that actually implements SCSI target command
 *    policy is the recipient of incoming commands and the source of the
 *    disposition for them.
 *
 * The command structure below is one suggested possible MD command structure,
 * but since the handling of thbis is entirely in the MD layer, there is
 * no explicit or implicit requirement that it be used.
 *
 * The cd_private tag should be used by the MD layer to keep a free list
 * of these structures. Code outside of this driver can then use this
 * to identify it's own unit structures. That is, when not on the MD
 * layer's freelist, the MD layer should shove into it the identifier
 * that the outer layer has for it- passed in on an initial QIN_HBA_REG
 * call (see below).
 *
 * The cd_hba tag is a tag that uniquely identifies the HBA this target
 * mode command is coming from. The outer layer has to pass this back
 * unchanged to avoid chaos.
 *
 * The cd_iid, cd_tgt, cd_lun and cd_port tags are used to identify the
 * id of the initiator who sent us a command, the target claim to be, the
 * lun on the target we claim to be, and the port instance (for multiple
 * port host adapters) that this applies to (consider it an extra port
 * parameter). The iid, tgt and lun values are deliberately chosen to be
 * fat so that, for example, World Wide Names can be used instead of
 * the units that the firmware uses (in the case where the MD
 * layer maintains a port database, for example).
 *
 * The cd_tagtype field specifies what kind of command tag type, if
 * any, has been sent with the command. Note that the Outer Layer
 * still needs to pass the tag handle through unchanged even
 * if the tag type is CD_UNTAGGED.
 *
 * The cd_cdb contains storage for the passed in command descriptor block.
 * There is no need to define length as the callee should be able to
 * figure this out.
 *
 * The tag cd_lflags are the flags set by the MD driver when it gets
 * command incoming or when it needs to inform any outside entities
 * that the last requested action failed.
 *
 * The tag cd_hflags should be set by any outside software to indicate
 * the validity of sense and status fields (defined below) and to indicate
 * the direction data is expected to move. It is an error to have both
 * CDFH_DATA_IN and CDFH_DATA_OUT set.
 *
 * If the CDFH_STSVALID flag is set, the command should be completed (after
 * sending any data and/or status). If CDFH_SNSVALID is set and the MD layer
 * can also handle sending the associated sense data (either back with an
 * FCP RESPONSE IU for Fibre Channel or otherwise automatically handling a
 * REQUEST SENSE from the initator for this target/lun), the MD layer will
 * set the CDFL_SENTSENSE flag on successful transmission of the sense data.
 * It is an error for the CDFH_SNSVALID bit to be set and CDFH_STSVALID not
 * to be set. It is an error for the CDFH_SNSVALID be set and the associated
 * SCSI status (cd_scsi_status) not be set to CHECK CONDITON.
 * 
 * The tag cd_data points to a data segment to either be filled or
 * read from depending on the direction of data movement. The tag
 * is undefined if no data direction is set. The MD layer and outer
 * layers must agree on the meaning of cd_data and it is specifically
 * not defined here.
 *
 * The tag cd_totlen is the total data amount expected to be moved
 * over the life of the command. It may be set by the MD layer, possibly
 * from the datalen field of an FCP CMND IU unit. If it shows up in the outer
 * layers set to zero and the CDB indicates data should be moved, the outer
 * layer should set it to the amount expected to be moved.
 *
 * The tag cd_resid should be the total residual of data not transferred.
 * The outer layers need to set this at the begining of command processing
 * to equal cd_totlen. As data is successfully moved, this value is decreased.
 * At the end of a command, any nonzero residual indicates the number of bytes
 * requested by the command but not moved.
 *
 * The tag cd_xfrlen is the length of the currently active data transfer.
 * This allows several interations between any outside software and the
 * MD layer to move data.
 *
 * The reason that total length and total residual have to be tracked
 * is to keep track of relative offset.
 *
 * The tags cd_sense and cd_scsi_status are pretty obvious.
 *
 * The tag cd_error is to communicate between the MD layer and outer software
 * the current error conditions.
 *
 * The tag cd_lreserved, cd_hreserved are scratch areas for use for the MD
 * and outer layers respectively.
 * 
 */

#ifndef    TMD_CDBLEN
#define    TMD_CDBLEN       16
#endif
#ifndef    TMD_SENSELEN
#define    TMD_SENSELEN     18
#endif
#ifndef    QCDS
#define    QCDS             8
#endif

typedef struct tmd_cmd {
    void *              cd_private; /* private data pointer */
    void *              cd_hba;     /* HBA tag */
    void *              cd_data;    /* 'pointer' to data */
    uint64_t            cd_iid;     /* initiator ID */
    uint64_t            cd_tgt;     /* target id */
    uint64_t            cd_lun;     /* logical unit */
    uint32_t            cd_tagval;  /* tag value */
    uint32_t            cd_lflags;  /* flags lower level sets */
    uint32_t            cd_hflags;  /* flags higher level sets */
    uint32_t            cd_totlen;  /* total data load */
    uint32_t            cd_resid;   /* total data residual */
    uint32_t            cd_xfrlen;  /* current data load */
    int32_t             cd_error;   /* current error */
    uint8_t     cd_tagtype      : 4,
                cd_port         : 4;    /* port number on HBA */
    uint8_t             cd_scsi_status;
    uint8_t             cd_sense[TMD_SENSELEN];
    uint8_t             cd_cdb[TMD_CDBLEN];
    union {
        void *          ptrs[QCDS / sizeof (void *)];
        uint64_t        llongs[QCDS / sizeof (uint64_t)];
        uint32_t        longs[QCDS / sizeof (uint32_t)];
        uint16_t        shorts[QCDS / sizeof (uint16_t)];
        uint8_t         bytes[QCDS];
    } cd_lreserved[2], cd_hreserved[2];
} tmd_cmd_t;

/* defined tags */
#define CD_UNTAGGED     0
#define CD_SIMPLE_TAG   1
#define CD_ORDERED_TAG  2
#define CD_HEAD_TAG     3
#define CD_ACA_TAG      4

#ifndef    TMD_SIZE
#define    TMD_SIZE     (sizeof (tmd_cmd_t))
#endif

/*
 * Note that NODISC (obviously) doesn't apply to non-SPI transport.
 *
 * Note that knowing the data direction and lengh at the time of receipt of
 * a command from the initiator is a feature only of Fibre Channel.
 *
 * The CDFL_BIDIR is in anticipation of the adoption of some newer
 * features required by OSD.
 *
 * The principle selector for MD layer to know whether data is to
 * be transferred in any QOUT_TMD_CONT call is cd_xfrlen- the
 * flags CDFH_DATA_IN and CDFH_DATA_OUT define which direction.
 */
#define    CDFL_SNSVALID        0x01            /* sense data (from f/w) good */
#define    CDFL_SENTSTATUS      0x02            /* last action sent status */
#define    CDFL_DATA_IN         0x04            /* target (us) -> initiator (them) */
#define    CDFL_DATA_OUT        0x08            /* initiator (them) -> target (us) */
#define    CDFL_BIDIR           0x0C            /* bidirectional data */
#define    CDFL_ERROR           0x10            /* last action ended in error */
#define    CDFL_NODISC          0x20            /* disconnects disabled */
#define    CDFL_SENTSENSE       0x40            /* last action sent sense data */
#define    CDFL_BUSY            0x80            /* this command is not on a free list */
#define    CDFL_PRIVATE         0xFF000000      /* private layer flags */

#define    CDFH_SNSVALID        0x01            /* sense data (from outer layer) good */
#define    CDFH_STSVALID        0x02            /* status valid */
#define    CDFH_DATA_IN         0x04            /* target (us) -> initiator (them) */
#define    CDFH_DATA_OUT        0x08            /* initiator (them) -> target (us) */
#define    CDFH_DATA_MASK       0x0C            /* mask to cover data direction */
#define    CDFH_PRIVATE         0xFF000000      /* private layer flags */


/*
 * A word about the START/CONT/DONE/FIN dance:
 *
 *    When the HBA is enabled for receiving commands, one may show up
 *    without notice. When that happens, the MD target mode driver
 *    gets a tmd_cmd_t, fills it with the info that just arrived, and
 *    calls the outer layer with a QOUT_TMD_START code and pointer to
 *    the tmd_cmd_t.
 *
 *    The outer layer decodes the command, fetches data, prepares stuff,
 *    whatever, and starts by passing back the pointer with a QIN_TMD_CONT
 *    code which causes the MD target mode driver to generate CTIOs to
 *    satisfy whatever action needs to be taken. When those CTIOs complete,
 *    the MD target driver sends the pointer to the cmd_tmd_t back with
 *    a QOUT_TMD_DONE code. This repeats for as long as necessary. These
 *    may not be done in parallel- they are sequential operations.
 *
 *    The outer layer signals it wants to end the command by settings within
 *    the tmd_cmd_t itself. When the final QIN_TMD_CONT is reported completed,
 *    the outer layer frees the tmd_cmd_t by sending the pointer to it
 *    back with a QIN_TMD_FIN code.
 *
 *    The graph looks like:
 *
 *    QOUT_TMD_START -> [ QIN_TMD_CONT -> QOUT_TMD_DONE ] * -> QIN_TMD_FIN.
 *
 */

/*
 * Target handler functions.
 *
 * The MD target handler function (the outer layer calls this)
 * should be be prototyped like:
 *
 *    void target_action(qact_e, void *arg)
 *
 * The outer layer target handler function (the MD layer calls this)
 * should be be prototyped like:
 *
 *    void scsi_target_handler(tact_e, void *arg)
 */
#endif    /* _ISP_TPUBLIC_H */
/*
 * vim:ts=4:sw=4:expandtab
 */
