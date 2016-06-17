#define	HSCX_MTU	1600

#define	HSCX_ISTA	0x00
#define HSCX_MASK	0x00
#define HSCX_STAR	0x01
#define HSCX_CMDR	0x01
#define HSCX_MODE	0x02
#define HSCX_TIMR	0x03
#define HSCX_EXIR	0x04
#define HSCX_XAD1	0x04
#define HSCX_RBCL	0x05
#define HSCX_SAD2	0x05
#define HSCX_RAH1	0x06
#define HSCX_RSTA	0x07
#define HSCX_RAH2	0x07
#define HSCX_RAL1	0x08
#define HSCX_RCHR	0x09
#define HSCX_RAL2	0x09
#define HSCX_XBCL	0x0a
#define HSCX_BGR	0x0b
#define HSCX_CCR2	0x0c
#define HSCX_RBCH	0x0d
#define HSCX_XBCH	0x0d
#define HSCX_VSTR	0x0e
#define HSCX_RLCR	0x0e
#define HSCX_CCR1	0x0f
#define HSCX_FIFO	0x1e

#define HSCX_HSCX_CHOFFS	0x400
#define HSCX_SEROFFS	0x1000

#define HSCX_RME	0x80
#define HSCX_RPF	0x40
#define HSCX_RSC	0x20
#define HSCX_XPR	0x10
#define HSCX_TIN	0x08
#define HSCX_ICA	0x04
#define HSCX_EXA	0x02
#define HSCX_EXB	0x01

#define HSCX_XMR	0x80
#define HSCX_XDU	0x40
#define HSCX_EXE	0x40
#define HSCX_PCE	0x20
#define HSCX_RFO	0x10
#define HSCX_CSC	0x08
#define HSCX_RFS	0x04

#define HSCX_XDOV	0x80
#define HSCX_XFW	0x40
#define HSCX_XRNR	0x20
#define HSCX_RRNR	0x10
#define HSCX_RLI	0x08
#define HSCX_CEC	0x04
#define HSCX_CTS	0x02
#define HSCX_WFA	0x01

#define HSCX_RMC	0x80
#define HSCX_RHR	0x40
#define HSCX_RNR	0x20
#define HSCX_XREP	0x20
#define HSCX_STI	0x10
#define HSCX_XTF	0x08
#define HSCX_XIF	0x04
#define HSCX_XME	0x02
#define HSCX_XRES	0x01

#define HSCX_AUTO	0x00
#define HSCX_NONAUTO	0x40
#define HSCX_TRANS	0x80
#define HSCX_XTRANS	0xc0
#define HSCX_ADM16	0x20
#define HSCX_ADM8	0x00
#define HSCX_TMD_EXT	0x00
#define HSCX_TMD_INT	0x10
#define HSCX_RAC	0x08
#define HSCX_RTS	0x04
#define HSCX_TLP	0x01

#define HSCX_VFR	0x80
#define HSCX_RDO	0x40
#define HSCX_CRC	0x20
#define HSCX_RAB	0x10

#define HSCX_CIE	0x04
#define HSCX_RIE	0x02

#define HSCX_DMA	0x80
#define HSCX_NRM	0x40
#define HSCX_CAS	0x20
#define HSCX_XC	0x10

#define HSCX_OV	0x10

#define HSCX_CD	0x80

#define HSCX_RC	0x80

#define HSCX_PU	0x80
#define HSCX_NRZ	0x00
#define HSCX_NRZI	0x40
#define HSCX_ODS	0x10
#define HSCX_ITF	0x08
