/**********************************************************************
 * Defines for the Tachyon Fibre Channel Controller and the Interphase
 * (i)chip TPI. 
 *********************************************************************/

#ifndef _TACH_H
#define _TACH_H

#define MY_PAGE_SIZE       4096
#define REPLICATE          0xFF
#define MAX_NODES          127
#define BROADCAST          0xFFFFFF
#define BROADCAST_ADDR     0xFFFFFFFFFFFF
#define LOGIN_COMPLETED     2
#define LOGIN_ATTEMPTED     1
#define LOGIN_NOT_ATTEMPTED 0
#define TRUE                1
#define FALSE               0

#define TACHYON_LIMIT       0x01EF
#define TACHYON_OFFSET      0x200

/* Offsets to the (i) chip */
#define ICHIP_HW_CONTROL_REG_OFF    (0x080 - TACHYON_OFFSET)
#define ICHIP_HW_STATUS_REG_OFF     (0x084 - TACHYON_OFFSET)
#define ICHIP_HW_ADDR_MASK_REG_OFF  (0x090 - TACHYON_OFFSET)

/* (i)chip Hardware Control Register defines */
#define ICHIP_HCR_RESET         0x01
#define ICHIP_HCR_DERESET       0x0
#define ICHIP_HCR_ENABLE_INTA   0x0000003E 
#define ICHIP_HCR_ENABLE_INTB   0x003E0000
#define ICHIP_HCR_IWDATA_FIFO   0x800000

/* (i)chip Hardware Status Register defines */
#define ICHIP_HSR_INT_LATCH     0x02

/* (i)chip Hardware Address Mask Register defines */
#define ICHIP_HAMR_BYTE_SWAP_ADDR_TR    0x08
#define ICHIP_HAMR_BYTE_SWAP_NO_ADDR_TR 0x04

/* NOVRAM defines */
#define IPH5526_NOVRAM_SIZE 64


/* Offsets for the registers that correspond to the 
 * Qs on the Tachyon (As defined in the Tachyon Manual).
 */

/* Outbound Command Queue (OCQ).
 */
#define OCQ_BASE_REGISTER_OFFSET	0x000  
#define OCQ_LENGTH_REGISTER_OFFSET	0x004
#define OCQ_PRODUCER_REGISTER_OFFSET	0x008  
#define OCQ_CONSUMER_REGISTER_OFFSET	0x00C 

/* Inbound Message Queue (IMQ).
 */
#define IMQ_BASE_REGISTER_OFFSET	0x080
#define IMQ_LENGTH_REGISTER_OFFSET	0x084
#define IMQ_CONSUMER_REGISTER_OFFSET	0x088
#define IMQ_PRODUCER_REGISTER_OFFSET	0x08C

/* Multiframe Sequence Buffer Queue (MFSBQ)
 */
#define MFSBQ_BASE_REGISTER_OFFSET	0x0C0
#define MFSBQ_LENGTH_REGISTER_OFFSET	0x0C4
#define MFSBQ_PRODUCER_REGISTER_OFFSET	0x0C8
#define MFSBQ_CONSUMER_REGISTER_OFFSET	0x0CC  
#define MFS_LENGTH_REGISTER_OFFSET	0x0D0

/* Single Frame Sequence Buffer Queue (SFSBQ)
 */
#define SFSBQ_BASE_REGISTER_OFFSET	0x100
#define SFSBQ_LENGTH_REGISTER_OFFSET	0x104
#define SFSBQ_PRODUCER_REGISTER_OFFSET	0x108
#define SFSBQ_CONSUMER_REGISTER_OFFSET	0x10C  
#define SFS_LENGTH_REGISTER_OFFSET	0x110

/* SCSI Exchange State Table (SEST)
 */
#define SEST_BASE_REGISTER_OFFSET	0x140
#define SEST_LENGTH_REGISTER_OFFSET	0x144
#define SCSI_LENGTH_REGISTER_OFFSET	0x148

/*  Length of the various Qs 
 */
#define NO_OF_ENTRIES		8
#define OCQ_LENGTH		(MY_PAGE_SIZE/32)
#define IMQ_LENGTH		(MY_PAGE_SIZE/32)
#define MFSBQ_LENGTH		8
#define SFSBQ_LENGTH		8
#define SEST_LENGTH		MY_PAGE_SIZE

/* Size of the various buffers.
 */
#define TACH_FRAME_SIZE         2048
#define MFS_BUFFER_SIZE         TACH_FRAME_SIZE
#define SFS_BUFFER_SIZE         (TACH_FRAME_SIZE + TACHYON_HEADER_LEN)
#define SEST_BUFFER_SIZE        512
#define TACH_HEADER_SIZE        64
#define NO_OF_TACH_HEADERS      ((MY_PAGE_SIZE)/TACH_HEADER_SIZE)

#define NO_OF_FCP_CMNDS         (MY_PAGE_SIZE/32)
#define SDB_SIZE                2048
#define NO_OF_SDB_ENTRIES       ((32*MY_PAGE_SIZE)/SDB_SIZE)


/* Offsets to the other Tachyon registers.
 * (As defined in the Tachyon manual)
 */
#define TACHYON_CONFIG_REGISTER_OFFSET          0x184
#define TACHYON_CONTROL_REGISTER_OFFSET         0x188
#define TACHYON_STATUS_REGISTER_OFFSET          0x18C
#define TACHYON_FLUSH_SEST_REGISTER_OFFSET      0x190

/* Defines for the Tachyon Configuration register.
 */
#define SCSI_ENABLE             0x40000000     
#define WRITE_STREAM_SIZE       0x800	/* size = 16 */         
#define READ_STREAM_SIZE        0x300	/* size = 64 */      
#define PARITY_EVEN             0x2         
#define OOO_REASSEMBLY_DISABLE  0x40

/* Defines for the Tachyon Control register.
 */
#define SOFTWARE_RESET	0x80000000
#define OCQ_RESET	0x4
#define ERROR_RELEASE	0x2

/* Defines for the Tachyon Status register.
 */
#define RECEIVE_FIFO_EMPTY      0x10
#define OSM_FROZEN              0x1
#define OCQ_RESET_STATUS        0x20
#define SCSI_FREEZE_STATUS      0x40


/* Offsets to the Frame Manager registers.
 */
#define FMGR_CONFIG_REGISTER_OFFSET 0x1C0
#define FMGR_CONTROL_REGISTER_OFFSET 0x1C4
#define FMGR_STATUS_REGISTER_OFFSET 0x1C8
#define FMGR_TIMER_REGISTER_OFFSET 0x1CC
#define FMGR_WWN_HI_REGISTER_OFFSET 0x1E0
#define FMGR_WWN_LO_REGISTER_OFFSET 0x1E4
#define FMGR_RCVD_ALPA_REGISTER_OFFSET 0x1E8

/* Defines for the Frame Manager Configuration register.
 */
#define BB_CREDIT                    0x10000
#define NPORT                        0x8000 
#define LOOP_INIT_FABRIC_ADDRESS     0x400  
#define LOOP_INIT_PREVIOUS_ADDRESS   0x200  
#define LOOP_INIT_SOFT_ADDRESS       0x80  

/* Defines for the Frame Manager Control register.
 */
#define HOST_CONTROL                 0x02   
#define EXIT_HOST_CONTROL            0x03  
#define OFFLINE                      0x05 
#define INITIALIZE                   0x06 
#define CLEAR_LF                     0x07

/* Defines for the Frame Manager Status register.
 */
#define LOOP_UP                 0x80000000
#define TRANSMIT_PARITY_ERROR   0x40000000
#define NON_PARTICIPATING       0x20000000
#define OUT_OF_SYNC             0x02000000
#define LOSS_OF_SIGNAL          0x01000000
#define NOS_OLS_RECEIVED        0x00080000
#define LOOP_STATE_TIMEOUT      0x00040000
#define LIPF_RECEIVED           0x00020000
#define BAD_ALPA                0x00010000
#define LINK_FAILURE            0x00001000
#define ELASTIC_STORE_ERROR     0x00000400
#define LINK_UP                 0x00000200
#define LINK_DOWN               0x00000100
#define ARBITRATING             0x00000010
#define ARB_WON                 0x00000020
#define OPEN                    0x00000030
#define OPENED                  0x00000040
#define TX_CLS                  0x00000050
#define RX_CLS                  0x00000060
#define TRANSFER                0x00000070
#define INITIALIZING            0x00000080
#define LOOP_FAIL               0x000000D0
#define OLD_PORT                0x000000F0
#define PORT_STATE_ACTIVE       0x0000000F
#define PORT_STATE_OFFLINE      0x00000000
#define PORT_STATE_LF1          0x00000009
#define PORT_STATE_LF2          0x0000000A

/* Completion Message Types 
 * (defined in P.177 of the Tachyon manual)
 */
#define OUTBOUND_COMPLETION             0x000
#define OUTBOUND_COMPLETION_I           0x100
#define OUT_HI_PRI_COMPLETION           0x001
#define OUT_HI_PRI_COMPLETION_I         0x101
#define INBOUND_MFS_COMPLETION          0x102
#define INBOUND_OOO_COMPLETION          0x003
#define INBOUND_SFS_COMPLETION          0x104
#define INBOUND_C1_TIMEOUT              0x105
#define INBOUND_UNKNOWN_FRAME_I         0x106
#define INBOUND_BUSIED_FRAME            0x006
#define SFS_BUF_WARN                    0x107
#define MFS_BUF_WARN                    0x108
#define IMQ_BUF_WARN                    0x109
#define FRAME_MGR_INTERRUPT             0x10A
#define READ_STATUS                     0x10B
#define INBOUND_SCSI_DATA_COMPLETION    0x10C
#define INBOUND_SCSI_COMMAND            0x10D
#define BAD_SCSI_FRAME                  0x10E
#define INB_SCSI_STATUS_COMPLETION      0x10F

/* One of the things that we care about when we receive an
 * Outbound Completion Message (OCM).
 */
#define OCM_TIMEOUT_OR_BAD_ALPA         0x0800

/* Defines for the Tachyon Header structure.
 */
#define SOFI3                0x70
#define SOFN3                0xB0
#define EOFN                 0x5

/* R_CTL */
#define FC4_DEVICE_DATA      0
#define EXTENDED_LINK_DATA   0x20000000
#define FC4_LINK_DATA        0x30000000
#define BASIC_LINK_DATA      0x80000000
#define LINK_CONTROL         0xC0000000
#define SOLICITED_DATA       0x1000000
#define UNSOLICITED_CONTROL  0x2000000
#define SOLICITED_CONTROL    0x3000000
#define UNSOLICITED_DATA     0x4000000
#define DATA_DESCRIPTOR      0x5000000
#define UNSOLICITED_COMMAND  0x6000000

#define RCTL_ELS_UCTL          0x22000000
#define RCTL_ELS_SCTL          0x23000000
#define RCTL_BASIC_ABTS        0x81000000
#define RCTL_BASIC_ACC         0x84000000
#define RCTL_BASIC_RJT         0x85000000

/* TYPE */
#define TYPE_BLS               0x00000000
#define TYPE_ELS               0x01000000
#define TYPE_FC_SERVICES       0x20000000
#define TYPE_LLC_SNAP          0x05000000
#define TYPE_FCP               0x08000000

/* F_CTL */
#define EXCHANGE_RESPONDER     0x800000
#define SEQUENCE_RESPONDER     0x400000
#define FIRST_SEQUENCE         0x200000
#define LAST_SEQUENCE          0x100000
#define SEQUENCE_INITIATIVE    0x10000
#define RELATIVE_OFF_PRESENT   0x8
#define END_SEQUENCE           0x80000

#define TACHYON_HEADER_LEN     32
#define NW_HEADER_LEN          16
/* Defines for the Outbound Descriptor Block (ODB).
 */
#define ODB_CLASS_3          0xC000
#define ODB_NO_COMP          0x400
#define ODB_NO_INT           0x200
#define ODB_EE_CREDIT        0xF

/* Defines for the Extended Descriptor Block (EDB).
 */
#define EDB_LEN              ((32*MY_PAGE_SIZE)/8) 
#define EDB_END              0x8000
#define EDB_FREE             0
#define EDB_BUSY             1

/* Command Codes */
#define ELS_LS_RJT          0x01000000
#define ELS_ACC             0x02000000
#define ELS_PLOGI           0x03000000
#define ELS_FLOGI           0x04000000
#define ELS_LOGO            0x05000000
#define ELS_TPRLO           0x24000000
#define ELS_ADISC           0x52000000
#define ELS_PDISC           0x50000000
#define ELS_PRLI            0x20000000 
#define ELS_PRLO            0x21000000
#define ELS_SCR             0x62000000
#define ELS_RSCN            0x61000000
#define ELS_FARP_REQ        0x54000000
#define ELS_ABTX            0x06000000
#define ELS_ADVC            0x0D000000
#define ELS_ECHO            0x10000000
#define ELS_ESTC            0x0C000000
#define ELS_ESTS            0x0B000000
#define ELS_RCS             0x07000000
#define ELS_RES             0x08000000
#define ELS_RLS             0x0F000000
#define ELS_RRQ             0x12000000
#define ELS_RSS             0x09000000
#define ELS_RTV             0x0E000000
#define ELS_RSI             0x0A000000
#define ELS_TEST            0x11000000
#define ELS_RNC             0x53000000
#define ELS_RVCS            0x41000000
#define ELS_TPLS            0x23000000
#define ELS_GAID            0x30000000
#define ELS_FACT            0x31000000
#define ELS_FAN             0x60000000
#define ELS_FDACT           0x32000000
#define ELS_NACT            0x33000000
#define ELS_NDACT           0x34000000
#define ELS_QoSR            0x40000000
#define ELS_FDISC           0x51000000

#define ELS_NS_PLOGI        0x03FFFFFC 

/* LS_RJT reason codes.
 */
#define INV_LS_CMND_CODE                0x0001
#define LOGICAL_ERR                     0x0003
#define LOGICAL_BUSY                    0x0005
#define PROTOCOL_ERR                    0x0007
#define UNABLE_TO_PERFORM               0x0009
#define CMND_NOT_SUPP                   0x000B

/* LS_RJT explanation codes.
 */
#define NO_EXPLN                        0x0000
#define RECV_FIELD_SIZE                 0x0700
#define CONC_SEQ                        0x0900
#define REQ_NOT_SUPPORTED               0x2C00
#define INV_PAYLOAD_LEN                 0x2D00

/* Payload Length defines. 
 */
#define PLOGI_LEN				116

#define CONCURRENT_SEQUENCES 0x01
#define RO_INFO_CATEGORY     0xFE
#define E_D_TOV              0x07D0 /* 2 Secs */
#define AL_TIME	             0x0010 /* ~15 msec */
#define TOV_VALUES           (AL_TIME << 16) | E_D_TOV
#define RT_TOV               0x64   /* 100 msec */
#define PTP_TOV_VALUES       (RT_TOV << 16) | E_D_TOV
#define SERVICE_VALID        0x8000
#define SEQUENCE_DELIVERY	 0x0800
#define CLASS3_CONCURRENT_SEQUENCE    0x01
#define CLASS3_OPEN_SEQUENCE          0x01

/* These are retrieved from the NOVRAM.
 */
#define WORLD_WIDE_NAME_LOW     fi->g.my_port_name_low
#define WORLD_WIDE_NAME_HIGH    fi->g.my_port_name_high
#define N_PORT_NAME_HIGH        fi->g.my_port_name_high
#define N_PORT_NAME_LOW         fi->g.my_port_name_low
#define NODE_NAME_HIGH          fi->g.my_node_name_high
#define NODE_NAME_LOW           fi->g.my_node_name_low

#define PORT_NAME_LEN           8
#define NODE_NAME_LEN           8


#define PH_VERSION        0x0909

#define LOOP_BB_CREDIT  0x00
#define PT2PT_BB_CREDIT 0x01
#define FLOGI_C_F       0x0800 /* Alternate BB_Credit Mgmnt */ 
#define PLOGI_C_F       0x8800 /* Continuously Increasing + Alternate BB_Credit Management */

/* Fabric defines */
#define DIRECTORY_SERVER        0xFFFFFC
#define FABRIC_CONTROLLER       0xFFFFFD
#define F_PORT                  0xFFFFFE

#define FLOGI_DID				0xFFFE
#define NS_PLOGI_DID			0xFFFC

/* Fibre Channel Services defines */
#define FCS_RFC_4           0x02170000
#define FCS_GP_ID4          0x01A10000
#define FCS_ACC             0x8002
#define FCS_REJECT          0x8001

/* CT Header defines */
#define FC_CT_REV               0x01000000
#define DIRECTORY_SERVER_APP    0xFC
#define NAME_SERVICE            0x02

/* Port Type defines */
#define PORT_TYPE_IP            0x05000000
#define PORT_TYPE_NX_PORTS      0x7F000000

/* SCR defines */
#define FABRIC_DETECTED_REG		0x00000001
#define N_PORT_DETECTED_REG		0x00000002
#define FULL_REGISTRATION		0x00000003
#define CLEAR_REGISTRATION		0x000000FF

/* Command structure has only one byte to address targets 
 */
#define MAX_SCSI_TARGETS		0xFF 

#define FC_SCSI_READ                    0x80
#define FC_SCSI_WRITE                   0x81
#define FC_ELS                          0x01
#define FC_BLS                          0x00
#define FC_IP                           0x05
#define FC_BROADCAST                    0xFF

/* SEST defines.
 */
#define SEST_V                          0x80000000 /* V = 1 */
#define INB_SEST_VED                    0xA0000000 /* V = 1, D = 1 */
#define SEST_INV                        0x7FFFFFFF 
#define OUTB_SEST_VED                   0x80000000 /* V = 1 */
#define INV_SEQ_LEN                     0xFFFFFFFF
#define OUTB_SEST_LINK                  0xFFFF

/* PRLI defines. 
 */
#define PAGE_LEN                0x100000 /* 3rd byte - 0x10 */
#define PRLI_LEN                0x0014 /* 20 bytes */
#define FCP_TYPE_CODE           0x0800 /* FCP-SCSI */
#define IMAGE_PAIR              0x2000 /* establish image pair */
#define INITIATOR_FUNC          0x00000020
#define TARGET_FUNC             0x00000010
#define READ_XFER_RDY_DISABLED  0x00000002

#define NODE_PROCESS_LOGGED_IN  0x3
#define NODE_NOT_PRESENT        0x2
#define NODE_LOGGED_IN          0x1
#define NODE_LOGGED_OUT         0x0

/* Defines to determine what should be returned when a SCSI frame
 * times out.
 */
#define FC_SCSI_BAD_TARGET		0xFFFE0000

/* RSCN Address formats */
#define PORT_ADDRESS_FORMAT             0x00
#define AREA_ADDRESS_FORMAT             0x01
#define DOMAIN_ADDRESS_FORMAT           0x02

/* Defines used to determine whether a frame transmission should
 * be indicated by an interrupt or not.
 */
#define NO_COMP_AND_INT			0
#define INT_AND_COMP_REQ		1
#define NO_INT_COMP_REQ			2

/* Other junk...
 */
#define SDB_FREE             0
#define SDB_BUSY             1
#define MAX_PENDING_FRAMES   15
#define RX_ID_FIRST_SEQUENCE 0xFFFF
#define OX_ID_FIRST_SEQUENCE 0xFFFF
#define NOT_SCSI_XID            0x8000
#define MAX_SCSI_XID            0x0FFF /* X_IDs are from 0-4095 */
#define SCSI_READ_BIT           0x4000 
#define MAX_SCSI_OXID           0x4FFF
#define OXID_AVAILABLE          0
#define OXID_INUSE              1
#define MAX_SEQ_ID              0xFF

#define INITIATOR             2
#define TARGET                1
#define DELETE_ENTRY          1
#define ADD_ENTRY             2

#endif /* _TACH_H */
