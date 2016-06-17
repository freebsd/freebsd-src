struct promise_raid_conf {
    char                promise_id[24];

    u32             dummy_0;
    u32             magic_0;
    u32             dummy_1;
    u32             magic_1;
    u16             dummy_2;
    u8              filler1[470];
    struct {
        u32 flags;                          /* 0x200 */
        u8          dummy_0;
        u8          disk_number;
        u8          channel;
        u8          device;
        u32         magic_0;
        u32         dummy_1;
        u32         dummy_2;                /* 0x210 */
        u32         disk_secs;
        u32         dummy_3;
        u16         dummy_4;
        u8          status;
        u8          type;
        u8        total_disks;            /* 0x220 */
        u8        raid0_shift;
        u8        raid0_disks;
        u8        array_number;
        u32       total_secs;
        u16       cylinders;
        u8        heads;
        u8        sectors;
        u32         magic_1;
        u32         dummy_5;                /* 0x230 */
        struct {
            u16     dummy_0;
            u8      channel;
            u8      device;
            u32     magic_0;
            u32     disk_number;
        } disk[8];
    } raid;
    u32             filler2[346];
    u32            checksum;
};

#define PR_MAGIC        "Promise Technology, Inc."

