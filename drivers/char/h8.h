/*
 */

#ifndef __H8_H__
#define __H8_H__

/*
 * Register address and offsets
 */
#define H8_BASE_ADDR                   0x170            /* default */
#define H8_IRQ			       9                /* default */
#define H8_STATUS_REG_OFF              0x4              
#define H8_CMD_REG_OFF                 0x4
#define H8_DATA_REG_OFF                0x0


/* H8 register bit definitions */
/* status register */
#define H8_OFULL                       0x1              /* output data register full */
#define H8_IFULL                       0x2              /* input data register full */
#define H8_CMD                         0x8              /* command / not data */

#define H8_INTR                        0xfa
#define H8_NACK                        0xfc
#define H8_BYTE_LEVEL_ACK              0xfd
#define H8_CMD_ACK                     0xfe
#define H8_SYNC_BYTE                   0x99

/*
 * H8 command definitions
 */
/* System info commands */
#define H8_SYNC                         0x0
#define H8_RD_SN                        0x1
#define H8_RD_ENET_ADDR                 0x2
#define H8_RD_HW_VER                    0x3
#define H8_RD_MIC_VER                   0x4
#define H8_RD_MAX_TEMP                  0x5
#define H8_RD_MIN_TEMP                  0x6
#define H8_RD_CURR_TEMP                 0x7
#define H8_RD_SYS_VARIENT               0x8
#define H8_RD_PWR_ON_CYCLES             0x9
#define H8_RD_PWR_ON_SECS               0xa
#define H8_RD_RESET_STATUS              0xb
#define H8_RD_PWR_DN_STATUS             0xc
#define H8_RD_EVENT_STATUS              0xd
#define H8_RD_ROM_CKSM                  0xe
#define H8_RD_EXT_STATUS                0xf
#define H8_RD_USER_CFG                  0x10
#define H8_RD_INT_BATT_VOLT             0x11
#define H8_RD_DC_INPUT_VOLT             0x12
#define H8_RD_HORIZ_PTR_VOLT            0x13
#define H8_RD_VERT_PTR_VOLT             0x14
#define H8_RD_EEPROM_STATUS             0x15
#define H8_RD_ERR_STATUS                0x16
#define H8_RD_NEW_BUSY_SPEED            0x17
#define H8_RD_CONFIG_INTERFACE          0x18
#define H8_RD_INT_BATT_STATUS           0x19
#define H8_RD_EXT_BATT_STATUS           0x1a
#define H8_RD_PWR_UP_STATUS             0x1b
#define H8_RD_EVENT_STATUS_MASK         0x56

/* Read/write/modify commands */
#define H8_CTL_EMU_BITPORT              0x32
#define H8_DEVICE_CONTROL               0x21
#define H8_CTL_TFT_BRT_DC               0x22
#define H8_CTL_WATCHDOG                 0x23
#define H8_CTL_MIC_PROT                 0x24
#define H8_CTL_INT_BATT_CHG             0x25
#define H8_CTL_EXT_BATT_CHG             0x26
#define H8_CTL_MARK_SPACE               0x27
#define H8_CTL_MOUSE_SENSITIVITY        0x28
#define H8_CTL_DIAG_MODE                0x29
#define H8_CTL_IDLE_AND_BUSY_SPDS       0x2a
#define H8_CTL_TFT_BRT_BATT             0x2b
#define H8_CTL_UPPER_TEMP               0x2c
#define H8_CTL_LOWER_TEMP               0x2d
#define H8_CTL_TEMP_CUTOUT              0x2e
#define H8_CTL_WAKEUP                   0x2f
#define H8_CTL_CHG_THRESHOLD            0x30
#define H8_CTL_TURBO_MODE               0x31
#define H8_SET_DIAG_STATUS              0x40
#define H8_SOFTWARE_RESET               0x41
#define H8_RECAL_PTR                    0x42
#define H8_SET_INT_BATT_PERCENT         0x43
#define H8_WRT_CFG_INTERFACE_REG        0x45
#define H8_WRT_EVENT_STATUS_MASK        0x57
#define H8_ENTER_POST_MODE              0x46
#define H8_EXIT_POST_MODE               0x47

/* Block transfer commands */
#define H8_RD_EEPROM                    0x50
#define H8_WRT_EEPROM                   0x51
#define H8_WRT_TO_STATUS_DISP           0x52
#define H8_DEFINE_SPC_CHAR              0x53
 
/* Generic commands */
#define H8_DEFINE_TABLE_STRING_ENTRY    0x60

/* Battery control commands */
#define H8_PERFORM_EMU_CMD              0x70
#define H8_EMU_RD_REG                   0x71
#define H8_EMU_WRT_REG                  0x72
#define H8_EMU_RD_RAM                   0x73
#define H8_EMU_WRT_RAM                  0x74
#define H8_BQ_RD_REG                    0x75
#define H8_BQ_WRT_REG                   0x76

/* System admin commands */
#define H8_PWR_OFF                      0x80

/*
 * H8 command related definitions
 */

/* device control argument bits */
#define H8_ENAB_EXTSMI                  0x1
#define H8_DISAB_IRQ                    0x2
#define H8_ENAB_FLASH_WRT               0x4
#define H8_ENAB_THERM                   0x8
#define H8_ENAB_INT_PTR                 0x10
#define H8_ENAB_LOW_SPD_IND             0x20
#define H8_ENAB_EXT_PTR                 0x40
#define H8_DISAB_PWR_OFF_SW             0x80
#define H8_POWER_OFF			0x80

/* H8 read event status bits */
#define H8_DC_CHANGE                    0x1
#define H8_INT_BATT_LOW                 0x2
#define H8_INT_BATT_CHARGE_THRESHOLD    0x4
#define H8_INT_BATT_CHARGE_STATE        0x8
#define H8_INT_BATT_STATUS              0x10
#define H8_EXT_BATT_CHARGE_STATE        0x20
#define H8_EXT_BATT_LOW                 0x40
#define H8_EXT_BATT_STATUS              0x80
#define H8_THERMAL_THRESHOLD            0x100
#define H8_WATCHDOG                     0x200
#define H8_DOCKING_STATION_STATUS       0x400
#define H8_EXT_MOUSE_OR_CASE_SWITCH     0x800
#define H8_KEYBOARD                     0x1000
#define H8_BATT_CHANGE_OVER             0x2000
#define H8_POWER_BUTTON                 0x4000
#define H8_SHUTDOWN                     0x8000

/* H8 control idle and busy speeds */
#define H8_SPEED_LOW                    0x1
#define H8_SPEED_MED                    0x2
#define H8_SPEED_HI                     0x3
#define H8_SPEED_LOCKED                 0x80

#define H8_MAX_CMD_SIZE                 18      
#define H8_Q_ALLOC_AMOUNT               10      

/* H8 state field values */
#define H8_IDLE                         1
#define H8_XMIT                         2
#define H8_RCV                          3
#define H8_RESYNC                       4
#define H8_INTR_MODE                    5

/* Mask values for control functions */
#define UTH_HYSTERESIS                  5
#define DEFAULT_UTHERMAL_THRESHOLD      115
#define H8_TIMEOUT_INTERVAL		30
#define H8_RUN                          4

#define H8_GET_MAX_TEMP                 0x1
#define H8_GET_CURR_TEMP                0x2
#define H8_GET_UPPR_THRMAL_THOLD        0x4
#define H8_GET_ETHERNET_ADDR            0x8
#define H8_SYNC_OP                      0x10 
#define H8_SET_UPPR_THRMAL_THOLD        0x20
#define H8_GET_INT_BATT_STAT            0x40
#define H8_GET_CPU_SPD                  0x80
#define H8_MANAGE_UTHERM                0x100 
#define H8_MANAGE_LTHERM                0x200 
#define H8_HALT                         0x400 
#define H8_CRASH                        0x800 
#define H8_GET_EXT_STATUS               0x10000
#define H8_MANAGE_QUIET                 0x20000
#define H8_MANAGE_SPEEDUP               0x40000
#define H8_MANAGE_BATTERY               0x80000
#define H8_SYSTEM_DELAY_TEST            0x100000
#define H8_POWER_SWITCH_TEST            0x200000

/* CPU speeds and clock divisor values */
#define MHZ_14                           5
#define MHZ_28                           4
#define MHZ_57                           3
#define MHZ_115                          2
#define MHZ_230                          0 

/*
 * H8 data
 */
struct h8_data {
        u_int           ser_num;
        u_char          ether_add[6];
        u_short         hw_ver;
        u_short         mic_ver;
        u_short         max_tmp;
        u_short         min_tmp;
        u_short         cur_tmp;
        u_int           sys_var;
        u_int           pow_on;
        u_int           pow_on_secs;
        u_char          reset_status;
        u_char          pwr_dn_status;
        u_short         event_status;
        u_short         rom_cksm;
        u_short         ext_status;
        u_short         u_cfg;
        u_char          ibatt_volt;
        u_char          dc_volt;
        u_char          ptr_horiz;
        u_char          ptr_vert;
        u_char          eeprom_status;
        u_char          error_status;
        u_char          new_busy_speed;
        u_char          cfg_interface;
        u_short         int_batt_status;
        u_short         ext_batt_status;
        u_char          pow_up_status;
        u_char          event_status_mask;
};


/*
 * H8 command buffers
 */
typedef struct h8_cmd_q {
        struct list_head link;          /* double linked list */
        int             ncmd;           /* number of bytes in command */
        int             nrsp;           /* number of bytes in response */
        int             cnt;            /* number of bytes sent/received */
        int             nacks;          /* number of byte level acks */
        u_char          cmdbuf[H8_MAX_CMD_SIZE]; /* buffer to store command */
        u_char          rcvbuf[H8_MAX_CMD_SIZE]; /* buffer to store response */
} h8_cmd_q_t;

union   intr_buf {
        u_char  byte[2];
        u_int   word;
};

#endif /* __H8_H_ */
