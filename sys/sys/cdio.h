/*
 * 16 Feb 93	Julian Elischer	(julian@dialix.oz.au)
 */
/* Shared between kernel & process */
#ifndef SYS_CDIO_H
#define SYS_CDIO_H

struct cd_toc_entry {
	u_char	:8;
	u_char	control:4;
	u_char	addr_type:4;
	u_char  track;
	u_char	:8;
	u_char	addr[4];
};

struct cd_sub_channel_header {
	u_char	:8;   
	u_char	audio_status; 
#define CD_AS_AUDIO_INVALID        0x00
#define CD_AS_PLAY_IN_PROGRESS     0x11
#define CD_AS_PLAY_PAUSED          0x12
#define CD_AS_PLAY_COMPLETED       0x13
#define CD_AS_PLAY_ERROR           0x14
#define CD_AS_NO_STATUS            0x15
	u_char	data_len[2];
};

struct cd_sub_channel_position_data {
	u_char	data_format;
	u_char	control:4;
	u_char	addr_type:4;
	u_char	track_number;
	u_char	index_number;
	u_char	absaddr[4];
	u_char	reladdr[4];
};

struct cd_sub_channel_media_catalog {
        u_char  data_format;
        u_char  :8;
        u_char  :8;
        u_char  :8;
        u_char  :7;
        u_char  mc_valid:1;
        u_char  mc_number[15]; 
};

struct cd_sub_channel_track_info {
        u_char  data_format;
        u_char  :8;
        u_char  track_number;
        u_char  :8;
        u_char  :7;
        u_char  ti_valid:1;   
        u_char  ti_number[15]; 
};

struct cd_sub_channel_info {
	struct cd_sub_channel_header header;
	union {
		struct cd_sub_channel_position_data position;
		struct cd_sub_channel_media_catalog media_catalog;
		struct cd_sub_channel_track_info track_info;	
	} what;
};

/***************************************************************\
* Ioctls for the CD drive					*
\***************************************************************/
struct ioc_play_track
{
	u_char	start_track;
	u_char	start_index;
	u_char	end_track;
	u_char	end_index;
};

#define	CDIOCPLAYTRACKS	_IOW('c',1,struct ioc_play_track)
struct ioc_play_blocks
{
	int	blk;
	int	len;
};
#define	CDIOCPLAYBLOCKS	_IOW('c',2,struct ioc_play_blocks)

struct ioc_read_subchannel {
	u_char address_format;
#define CD_LBA_FORMAT	1
#define CD_MSF_FORMAT	2
	u_char data_format;
#define CD_SUBQ_DATA		0
#define CD_CURRENT_POSITION	1
#define CD_MEDIA_CATALOG	2
#define CD_TRACK_INFO		3
	u_char track;
	int	data_len;
	struct  cd_sub_channel_info *data;
};
#define CDIOCREADSUBCHANNEL _IOWR('c', 3 , struct ioc_read_subchannel )


struct ioc_toc_header {
	u_short len;
	u_char  starting_track;
	u_char  ending_track;
};

#define CDIOREADTOCHEADER _IOR('c',4,struct ioc_toc_header)

struct ioc_read_toc_entry {
	u_char	address_format;
	u_char	starting_track;
	u_short	data_len;
	struct  cd_toc_entry *data;
};
#define CDIOREADTOCENTRYS _IOWR('c',5,struct ioc_read_toc_entry)

struct	ioc_patch
{
	u_char	patch[4];	/* one for each channel */
};
#define	CDIOCSETPATCH	_IOW('c',9,struct ioc_patch)
struct	ioc_vol
{
	u_char	vol[4];	/* one for each channel */
};
#define	CDIOCGETVOL	_IOR('c',10,struct ioc_vol)
#define	CDIOCSETVOL	_IOW('c',11,struct ioc_vol)
#define	CDIOCSETMONO	_IO('c',12)
#define	CDIOCSETSTERIO	_IO('c',13)
#define	CDIOCSETMUTE	_IO('c',14)
#define	CDIOCSETLEFT	_IO('c',15)
#define	CDIOCSETRIGHT	_IO('c',16)
#define	CDIOCSETDEBUG	_IO('c',17)
#define	CDIOCCLRDEBUG	_IO('c',18)
#define	CDIOCPAUSE	_IO('c',19)
#define	CDIOCRESUME	_IO('c',20)
#define	CDIOCRESET	_IO('c',21)
#define	CDIOCSTART	_IO('c',22)
#define	CDIOCSTOP	_IO('c',23)
#define	CDIOCEJECT	_IO('c',24)

struct ioc_play_msf
{
	u_char	start_m;
	u_char	start_s;
	u_char	start_f;
	u_char	end_m;
	u_char	end_s;
	u_char	end_f;
};
#define	CDIOCPLAYMSF	_IOW('c',25,struct ioc_play_msf)



#endif /* SYS_CDIO_H */
