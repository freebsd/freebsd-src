/* ADL.H: Definitions for attribute descriptor list processing.
*/
/* N/C/SDATA external entity types for nxetype member of ne structure. */
#define ESNCDATA    1         /* External character data entity. */
#define ESNNDATA    2         /* Non-SGML data entity. */
#define ESNSDATA    3         /* External specific character data entity. */
#define ESNSUB      4         /* SGML subdocument entity. */

/* N/C/SDATA control block for AENTITY attributes and NDATA returns.*/
struct ne {                   /* N/C/SDATA entity control block. */
     UNIV neid;               /* Files for NDATA entity. */
     UNCH *nepubid;	      /* Public identifier if specified. */
     UNCH *nesysid;	      /* System identifier if specified. */
     PDCB nedcn;              /* Data content notation control block. */
     struct ad *neal;         /* Data attribute list (NULL if none). */
     UNCH *neename;           /* Ptr to entity name (length and EOS). */
     UNCH nextype;            /* Entity type: NDATA SDATA CDATA SUBDOC. */
};
#define NESZ (sizeof(struct ne))
typedef struct ne *PNE;
/* NDATA entity control block fields. */
#define NEID(p) (((PNE)p)->neid)            /* File ID of NDATA entity. */
#define NESYSID(p) (((PNE)p)->nesysid)      /* System ID of NDATA entity. */
#define NEPUBID(p) (((PNE)p)->nepubid)      /* Public ID of NDATA entity. */
#define NEDCN(p) (((PNE)p)->nedcn->ename)   /* Data content notation name. */
#define NEDCNSYSID(p) (((PNE)p)->nedcn->sysid) /* Notation system ID.*/
#define NEDCNPUBID(p) (((PNE)p)->nedcn->pubid) /* Notation public ID.*/
#define NEDCNDEFINED(p) (((PNE)p)->nedcn->defined) /* Notation defined? */
#define NEDCNADL(p) (((PNE)p)->nedcn->adl)  /* Data content notation attlist.*/
#define NEENAME(p) (((PNE)p)->neename)      /* Entity name pointer. */
#define NEXTYPE(p) (((PNE)p)->nextype)      /* External entity type. */
#define NEAL(p) (((PNE)p)->neal)            /* Data attributes (if any). */
#define NEDCNMARK(p) DCNMARK(((PNE)p)->nedcn)

/* Attribute descriptor list entry. */
struct ad {
     UNCH *adname;	      /* Attribute name with length and EOS. */
     UNCH adflags;            /* Attribute flags. */
     UNCH adtype;             /* Value type. */
     UNS adnum;               /* Group size or member pos in grp. */
     UNS adlen;               /* Length of default or value (for capacity). */
     UNCH *addef;             /* Default value (NULL if REQUIRED or IMPLIED). */
     union {
          PNE n;              /* AENTITY: NDATA control block. */
          PDCB x;             /* ANOTEGRP: DCN control block. */
     } addata;                /* Special data associated with some attributes.*/
};
#define ADSZ (sizeof(struct ad))   /* Size of an ad structure. */

/* Attribute flags for entire list adflags: ADLF. */
#define ADLREQ    0x80        /* Attribute list: 1=REQUIRED att defined. */
#define ADLNOTE   0x40        /* Attribute list: 1=NOTATION att defined. */
#define ADLCONR   0x20        /* Attribute list: 1=CONREF att defined. */

/* Attribute flags for list member adflags: ADFLAGS(n). */
#define AREQ      0x80        /* Attribute: 0=null; 1=required. */
#define ACURRENT  0x40        /* Attribute: 0=normal; 1=current. */
#define AFIXED    0x20        /* Attribute: 0=normal; 1=must equal default. */
#define AGROUP    0x10        /* Attribute: 0=single; 1=group of ad's. */
#define ACONREF   0x08        /* Attribute: 0=normal; 1=att is CONREF. */
#define AINVALID  0x04        /* Attribute: 1=value is invalid; 0=o.k. */
#define AERROR    0x02        /* Attribute: 1=error was specified; 0=o.k. */
#define ASPEC     0x01        /* Attribute: 1=value was specified; 0=default. */

/* Attribute types for adtype. */
#define ANMTGRP   0x00        /* Attribute: Name token group or member. */
#define ANOTEGRP  0x01        /* Attribute: Notation (name group). */
#define ACHARS    0x02        /* Attribute: Character string. */
#define AENTITY   0x03        /* Attribute: Data entity (name). */
#define AID       0x04        /* Attribute: ID value (name). */
#define AIDREF    0x05        /* Attribute: ID reference value (name). */
#define ANAME     0x06        /* Attribute: Name. */
#define ANMTOKE   0x07        /* Attribute: Name token. */
#define ANUMBER   0x08        /* Attribute: Number. */
#define ANUTOKE   0x09        /* Attribute: Number token. */
#define ATKNLIST  0x0A        /* Attribute: >= means value is a token list. */
#define AENTITYS  0x0A        /* Attribute: Data entities (name list). */
#define AIDREFS   0x0B        /* Attribute: ID reference value (name list). */
#define ANAMES    0x0C        /* Attribute: Name list. */
#define ANMTOKES  0x0D        /* Attribute: Name token list. */
#define ANUMBERS  0x0E        /* Attribute: Number list. */
#define ANUTOKES  0x0F        /* Attribute: Number token list. */

/* Field definitions for entries in an attribute list.
   The first argument to all of these is the list address.
*/
/* Attribute list: flags. */
#define ADLF(a) ((a)[0].adflags)
/* Attribute list: number of list members. */
#define ADN(a) ((a)[0].adtype)
/* Attribute list: number of attributes. */
#define AN(a) ((a)[0].adnum)
/* Nth attribute in list: name. */
#define ADNAME(a, n) (((a)[n].adname+1))
/* Nth att in list: number of val)ues. */
#define ADNUM(a, n) ((a)[n].adnum)
/* Nth attribute in list: flags. */
#define ADFLAGS(a, n) ((a)[n].adflags)
/* Nth attribute in list: type. */
#define ADTYPE(a, n) ((a)[n].adtype)
/* Nth attribute in list: len of def or val.*/
#define ADLEN(a, n) ((a)[n].adlen)
/* Nth attribute in list: def or value. */
#define ADVAL(a, n) ((a)[n].addef)
/* Nth attribute in list: special data. */
#define ADDATA(a, n) ((a)[n].addata)
/* Nth att: token at Pth pos in value. */
#define ADTOKEN(a, n, p)(((a)[n].addef+(p)))

#define IDHASH 101            /* Size of ID hash table.  Must be prime. */
struct id {                   /* ID attribute control block. */
     struct id *idnext;       /* Next ID in chain. */
     UNCH *idname;	      /* ID name with length prefix and EOS. */
     UNCH iddefed;	      /* Non-zero if it has been defined. */
     struct fwdref *idrl;     /* Chain of forward references to this ID. */
};
#define IDSZ sizeof(struct id)
typedef struct id *PID;       /* Ptr to ID attribute control block. */
