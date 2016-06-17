/************************************************************
 * EFI GUID Partition Table
 * Per Intel EFI Specification v1.02
 * http://developer.intel.com/technology/efi/efi.htm
 *
 * By Matt Domsch <Matt_Domsch@dell.com>  Fri Sep 22 22:15:56 CDT 2000  
 *   Copyright 2000,2001 Dell Computer Corporation
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 * 
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 * 
 ************************************************************/

#ifndef FS_PART_EFI_H_INCLUDED
#define FS_PART_EFI_H_INCLUDED

#include <linux/types.h>
#include <linux/config.h>
#include <linux/fs.h>
#include <linux/genhd.h>
#include <linux/kernel.h>
#include <linux/major.h>
#include <linux/string.h>
#include <linux/blk.h>
#include <linux/efi.h>

#define MSDOS_MBR_SIGNATURE 0xaa55
#define EFI_PMBR_OSTYPE_EFI 0xEF
#define EFI_PMBR_OSTYPE_EFI_GPT 0xEE

#define GPT_BLOCK_SIZE 512
#define GPT_HEADER_SIGNATURE 0x5452415020494645ULL
#define GPT_HEADER_REVISION_V1 0x00010000
#define GPT_PRIMARY_PARTITION_TABLE_LBA 1

#define PARTITION_SYSTEM_GUID \
    EFI_GUID( 0xC12A7328, 0xF81F, 0x11d2, \
              0xBA, 0x4B, 0x00, 0xA0, 0xC9, 0x3E, 0xC9, 0x3B) 
#define LEGACY_MBR_PARTITION_GUID \
    EFI_GUID( 0x024DEE41, 0x33E7, 0x11d3, \
              0x9D, 0x69, 0x00, 0x08, 0xC7, 0x81, 0xF3, 0x9F)
#define PARTITION_MSFT_RESERVED_GUID \
    EFI_GUID( 0xE3C9E316, 0x0B5C, 0x4DB8, \
              0x81, 0x7D, 0xF9, 0x2D, 0xF0, 0x02, 0x15, 0xAE)
#define PARTITION_BASIC_DATA_GUID \
    EFI_GUID( 0xEBD0A0A2, 0xB9E5, 0x4433, \
              0x87, 0xC0, 0x68, 0xB6, 0xB7, 0x26, 0x99, 0xC7)
#define PARTITION_LINUX_RAID_GUID \
    EFI_GUID( 0xa19d880f, 0x05fc, 0x4d3b, \
              0xa0, 0x06, 0x74, 0x3f, 0x0f, 0x84, 0x91, 0x1e)
#define PARTITION_LINUX_SWAP_GUID \
    EFI_GUID( 0x0657fd6d, 0xa4ab, 0x43c4, \
              0x84, 0xe5, 0x09, 0x33, 0xc8, 0x4b, 0x4f, 0x4f)
#define PARTITION_LINUX_LVM_GUID \
    EFI_GUID( 0xe6d6d379, 0xf507, 0x44c2, \
              0xa2, 0x3c, 0x23, 0x8f, 0x2a, 0x3d, 0xf9, 0x28)

typedef struct _gpt_header {
	u64 signature;
	u32 revision;
	u32 header_size;
	u32 header_crc32;
	u32 reserved1;
	u64 my_lba;
	u64 alternate_lba;
	u64 first_usable_lba;
	u64 last_usable_lba;
	efi_guid_t disk_guid;
	u64 partition_entry_lba;
	u32 num_partition_entries;
	u32 sizeof_partition_entry;
	u32 partition_entry_array_crc32;
	u8 reserved2[GPT_BLOCK_SIZE - 92];
} __attribute__ ((packed)) gpt_header;

typedef struct _gpt_entry_attributes {
	u64 required_to_function:1;
	u64 reserved:47;
        u64 type_guid_specific:16;
} __attribute__ ((packed)) gpt_entry_attributes;

typedef struct _gpt_entry {
	efi_guid_t partition_type_guid;
	efi_guid_t unique_partition_guid;
	u64 starting_lba;
	u64 ending_lba;
	gpt_entry_attributes attributes;
	efi_char16_t partition_name[72 / sizeof (efi_char16_t)];
} __attribute__ ((packed)) gpt_entry;

typedef struct _legacy_mbr {
	u8 boot_code[440];
	u32 unique_mbr_signature;
	u16 unknown;
	struct partition partition_record[4];
	u16 signature;
} __attribute__ ((packed)) legacy_mbr;

/* Functions */
extern int
 efi_partition(struct gendisk *hd, struct block_device *bdev,
	      unsigned long first_sector, int first_part_minor);

#endif
