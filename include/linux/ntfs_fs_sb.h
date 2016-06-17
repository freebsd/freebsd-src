#ifndef _LINUX_NTFS_FS_SB_H
#define _LINUX_NTFS_FS_SB_H

#include <linux/ntfs_fs_i.h>

struct ntfs_sb_info{
	/* Configuration provided by user at mount time. */
	ntfs_uid_t uid;
	ntfs_gid_t gid;
	ntmode_t umask;
	void *nls_map;
	unsigned int ngt;
	char mft_zone_multiplier;
	unsigned long mft_data_pos;
	ntfs_cluster_t mft_zone_pos;
	ntfs_cluster_t mft_zone_start;
	ntfs_cluster_t mft_zone_end;
	ntfs_cluster_t data1_zone_pos;
	ntfs_cluster_t data2_zone_pos;
	/* Configuration provided by user with the ntfstools.
	 * FIXME: This is no longer possible. What is this good for? (AIA) */
	ntfs_size_t partition_bias;	/* For access to underlying device. */
	/* Attribute definitions. */
	ntfs_u32 at_standard_information;
	ntfs_u32 at_attribute_list;
	ntfs_u32 at_file_name;
	ntfs_u32 at_volume_version;
	ntfs_u32 at_security_descriptor;
	ntfs_u32 at_volume_name;
	ntfs_u32 at_volume_information;
	ntfs_u32 at_data;
	ntfs_u32 at_index_root;
	ntfs_u32 at_index_allocation;
	ntfs_u32 at_bitmap;
	ntfs_u32 at_symlink; /* aka SYMBOLIC_LINK or REPARSE_POINT */
	/* Data read / calculated from the boot file. */
	int sector_size;
	int cluster_size;
	int cluster_size_bits;
	int mft_clusters_per_record;
	int mft_record_size;
	int mft_record_size_bits;
	int index_clusters_per_record;
	int index_record_size;
	int index_record_size_bits;
	ntfs_cluster_t nr_clusters;
	ntfs_cluster_t mft_lcn;
	ntfs_cluster_t mft_mirr_lcn;
	/* Data read from special files. */
	unsigned char *mft;
	unsigned short *upcase;
	unsigned int upcase_length;
	/* Inodes we always hold onto. */
	struct ntfs_inode_info *mft_ino;
	struct ntfs_inode_info *mftmirr;
	struct ntfs_inode_info *bitmap;
	struct super_block *sb;
	unsigned char ino_flags;
};

#endif
