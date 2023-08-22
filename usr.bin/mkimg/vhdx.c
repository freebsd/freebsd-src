/*-
 * Copyright (c) 2020 Oleksandr Tymoshenko <gonzo@FreeBSD.org>
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

#include <sys/cdefs.h>
#include <sys/errno.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "endian.h"
#include "image.h"
#include "format.h"
#include "mkimg.h"

#define	PAYLOAD_BLOCK_SIZE	(16*1024*1024)
#define	SIZE_64KB		(64*1024)
#define	SIZE_1MB		(1024*1024)

#define	META_IS_REQUIRED	(1 << 2)
#define	META_IS_VIRTUAL_DISK	(1 << 1)

#define	PAYLOAD_BLOCK_FULLY_PRESENT	6
#define	SB_BLOCK_NOT_PRESENT		0
#define	BAT_ENTRY(offset, flags)	(((offset) << 20) | (flags))

/* Regions' UUIDs */
#define VHDX_REGION_BAT_GUID	\
        {0x2dc27766,0xf623,0x4200,0x9d,0x64,{0x11,0x5e,0x9b,0xfd,0x4a,0x08}}
#define VHDX_REGION_METADATA_GUID	\
        {0x8b7ca206,0x4790,0x4b9a,0xb8,0xfe,{0x57,0x5f,0x05,0x0f,0x88,0x6e}}
static mkimg_uuid_t vhdx_bat_guid = VHDX_REGION_BAT_GUID;
static mkimg_uuid_t vhdx_metadata_guid = VHDX_REGION_METADATA_GUID;

/* Metadata UUIDs */
#define VHDX_META_FILE_PARAMETERS	\
        {0xcaa16737,0xfa36,0x4d43,0xb3,0xb6,{0x33,0xf0,0xaa,0x44,0xe7,0x6b}}
#define VHDX_META_VDISK_SIZE	\
        {0x2fa54224,0xcd1b,0x4876,0xb2,0x11,{0x5d,0xbe,0xd8,0x3b,0xf4,0xb8}}
#define VHDX_META_VDISK_ID	\
        {0xbeca12ab,0xb2e6,0x4523,0x93,0xef,{0xc3,0x09,0xe0,0x00,0xc7,0x46}}
#define VHDX_META_LOGICAL_SSIZE	\
        {0x8141bf1D,0xa96f,0x4709,0xba,0x47,{0xf2,0x33,0xa8,0xfa,0xab,0x5f}}
#define VHDX_META_PHYS_SSIZE	\
        {0xcda348c7,0x445d,0x4471,0x9c,0xc9,{0xe9,0x88,0x52,0x51,0xc5,0x56}}

static mkimg_uuid_t vhdx_meta_file_parameters_guid = VHDX_META_FILE_PARAMETERS;
static mkimg_uuid_t vhdx_meta_vdisk_size_guid = VHDX_META_VDISK_SIZE;
static mkimg_uuid_t vhdx_meta_vdisk_id_guid = VHDX_META_VDISK_ID;
static mkimg_uuid_t vhdx_meta_logical_ssize_guid = VHDX_META_LOGICAL_SSIZE;
static mkimg_uuid_t vhdx_meta_phys_ssize_guid = VHDX_META_PHYS_SSIZE;

struct vhdx_filetype_identifier {
	uint64_t	signature;
#define	VHDX_FILETYPE_ID_SIGNATURE	0x656C696678646876
	uint8_t		creator[512];
};

struct vhdx_header {
	uint32_t	signature;
#define	VHDX_HEADER_SIGNATURE		0x64616568
	uint32_t	checksum;
	uint64_t	sequence_number;
	mkimg_uuid_t	file_write_guid;
	mkimg_uuid_t	data_write_guid;
	mkimg_uuid_t	log_guid;
	uint16_t	log_version;
	uint16_t	version;
	uint32_t	log_length;
	uint64_t	log_offset;
	uint8_t		_reserved[4016];
};

struct vhdx_region_table_header {
	uint32_t	signature;
#define	VHDX_REGION_TABLE_HEADER_SIGNATURE	0x69676572
	uint32_t	checksum;
	uint32_t	entry_count;
	uint32_t	_reserved;
};

struct vhdx_region_table_entry {
	mkimg_uuid_t	guid;
	uint64_t	file_offset;
	uint32_t	length;
	uint32_t	required;
};

struct vhdx_metadata_table_header {
	uint64_t	signature;
#define	VHDX_METADATA_TABLE_HEADER_SIGNATURE		0x617461646174656D
	uint16_t	_reserved;
	uint16_t	entry_count;
	uint8_t		_reserved2[20];
};

struct vhdx_metadata_table_entry {
	mkimg_uuid_t	item_id;
	uint32_t	offset;
	uint32_t	length;
	uint32_t	flags;
	uint32_t	_reserved;
};

#define CRC32C(c, d) (c = (c>>8) ^ crc_c[(c^(d))&0xFF])

static uint32_t crc_c[256] = {
	0x00000000, 0xF26B8303, 0xE13B70F7, 0x1350F3F4,
	0xC79A971F, 0x35F1141C, 0x26A1E7E8, 0xD4CA64EB,
	0x8AD958CF, 0x78B2DBCC, 0x6BE22838, 0x9989AB3B,
	0x4D43CFD0, 0xBF284CD3, 0xAC78BF27, 0x5E133C24,
	0x105EC76F, 0xE235446C, 0xF165B798, 0x030E349B,
	0xD7C45070, 0x25AFD373, 0x36FF2087, 0xC494A384,
	0x9A879FA0, 0x68EC1CA3, 0x7BBCEF57, 0x89D76C54,
	0x5D1D08BF, 0xAF768BBC, 0xBC267848, 0x4E4DFB4B,
	0x20BD8EDE, 0xD2D60DDD, 0xC186FE29, 0x33ED7D2A,
	0xE72719C1, 0x154C9AC2, 0x061C6936, 0xF477EA35,
	0xAA64D611, 0x580F5512, 0x4B5FA6E6, 0xB93425E5,
	0x6DFE410E, 0x9F95C20D, 0x8CC531F9, 0x7EAEB2FA,
	0x30E349B1, 0xC288CAB2, 0xD1D83946, 0x23B3BA45,
	0xF779DEAE, 0x05125DAD, 0x1642AE59, 0xE4292D5A,
	0xBA3A117E, 0x4851927D, 0x5B016189, 0xA96AE28A,
	0x7DA08661, 0x8FCB0562, 0x9C9BF696, 0x6EF07595,
	0x417B1DBC, 0xB3109EBF, 0xA0406D4B, 0x522BEE48,
	0x86E18AA3, 0x748A09A0, 0x67DAFA54, 0x95B17957,
	0xCBA24573, 0x39C9C670, 0x2A993584, 0xD8F2B687,
	0x0C38D26C, 0xFE53516F, 0xED03A29B, 0x1F682198,
	0x5125DAD3, 0xA34E59D0, 0xB01EAA24, 0x42752927,
	0x96BF4DCC, 0x64D4CECF, 0x77843D3B, 0x85EFBE38,
	0xDBFC821C, 0x2997011F, 0x3AC7F2EB, 0xC8AC71E8,
	0x1C661503, 0xEE0D9600, 0xFD5D65F4, 0x0F36E6F7,
	0x61C69362, 0x93AD1061, 0x80FDE395, 0x72966096,
	0xA65C047D, 0x5437877E, 0x4767748A, 0xB50CF789,
	0xEB1FCBAD, 0x197448AE, 0x0A24BB5A, 0xF84F3859,
	0x2C855CB2, 0xDEEEDFB1, 0xCDBE2C45, 0x3FD5AF46,
	0x7198540D, 0x83F3D70E, 0x90A324FA, 0x62C8A7F9,
	0xB602C312, 0x44694011, 0x5739B3E5, 0xA55230E6,
	0xFB410CC2, 0x092A8FC1, 0x1A7A7C35, 0xE811FF36,
	0x3CDB9BDD, 0xCEB018DE, 0xDDE0EB2A, 0x2F8B6829,
	0x82F63B78, 0x709DB87B, 0x63CD4B8F, 0x91A6C88C,
	0x456CAC67, 0xB7072F64, 0xA457DC90, 0x563C5F93,
	0x082F63B7, 0xFA44E0B4, 0xE9141340, 0x1B7F9043,
	0xCFB5F4A8, 0x3DDE77AB, 0x2E8E845F, 0xDCE5075C,
	0x92A8FC17, 0x60C37F14, 0x73938CE0, 0x81F80FE3,
	0x55326B08, 0xA759E80B, 0xB4091BFF, 0x466298FC,
	0x1871A4D8, 0xEA1A27DB, 0xF94AD42F, 0x0B21572C,
	0xDFEB33C7, 0x2D80B0C4, 0x3ED04330, 0xCCBBC033,
	0xA24BB5A6, 0x502036A5, 0x4370C551, 0xB11B4652,
	0x65D122B9, 0x97BAA1BA, 0x84EA524E, 0x7681D14D,
	0x2892ED69, 0xDAF96E6A, 0xC9A99D9E, 0x3BC21E9D,
	0xEF087A76, 0x1D63F975, 0x0E330A81, 0xFC588982,
	0xB21572C9, 0x407EF1CA, 0x532E023E, 0xA145813D,
	0x758FE5D6, 0x87E466D5, 0x94B49521, 0x66DF1622,
	0x38CC2A06, 0xCAA7A905, 0xD9F75AF1, 0x2B9CD9F2,
	0xFF56BD19, 0x0D3D3E1A, 0x1E6DCDEE, 0xEC064EED,
	0xC38D26C4, 0x31E6A5C7, 0x22B65633, 0xD0DDD530,
	0x0417B1DB, 0xF67C32D8, 0xE52CC12C, 0x1747422F,
	0x49547E0B, 0xBB3FFD08, 0xA86F0EFC, 0x5A048DFF,
	0x8ECEE914, 0x7CA56A17, 0x6FF599E3, 0x9D9E1AE0,
	0xD3D3E1AB, 0x21B862A8, 0x32E8915C, 0xC083125F,
	0x144976B4, 0xE622F5B7, 0xF5720643, 0x07198540,
	0x590AB964, 0xAB613A67, 0xB831C993, 0x4A5A4A90,
	0x9E902E7B, 0x6CFBAD78, 0x7FAB5E8C, 0x8DC0DD8F,
	0xE330A81A, 0x115B2B19, 0x020BD8ED, 0xF0605BEE,
	0x24AA3F05, 0xD6C1BC06, 0xC5914FF2, 0x37FACCF1,
	0x69E9F0D5, 0x9B8273D6, 0x88D28022, 0x7AB90321,
	0xAE7367CA, 0x5C18E4C9, 0x4F48173D, 0xBD23943E,
	0xF36E6F75, 0x0105EC76, 0x12551F82, 0xE03E9C81,
	0x34F4F86A, 0xC69F7B69, 0xD5CF889D, 0x27A40B9E,
	0x79B737BA, 0x8BDCB4B9, 0x988C474D, 0x6AE7C44E,
	0xBE2DA0A5, 0x4C4623A6, 0x5F16D052, 0xAD7D5351
};

static uint32_t
crc32c(const void *data, uint32_t len)
{
	uint32_t i, crc;
	const uint8_t *buf = (const uint8_t *)data;

	crc = ~0;
	for (i = 0; i < len; i++)
		CRC32C(crc, buf[i]);
	crc = ~crc;
	return crc;
}

static int
vhdx_resize(lba_t imgsz)
{
	uint64_t imagesz;

	imagesz = imgsz * secsz;
	imagesz = (imagesz + PAYLOAD_BLOCK_SIZE - 1) & ~(PAYLOAD_BLOCK_SIZE - 1);
	return (image_set_size(imagesz / secsz));
}

static int
vhdx_write_and_pad(int fd, const void *data, size_t data_size, size_t align)
{
	size_t pad_size;

	if (sparse_write(fd, data, data_size) < 0)
		return (errno);

	if (data_size % align == 0)
		return (0);

	pad_size = align - (data_size % align);
	return  image_copyout_zeroes(fd, pad_size);
}

static int
vhdx_write_headers(int fd)
{
	int error;
	struct vhdx_header header;
	uint32_t checksum;

	/* Write header 1 */
	memset(&header, 0, sizeof(header));
	le32enc(&header.signature, VHDX_HEADER_SIGNATURE);
	le32enc(&header.sequence_number, 0);
	le16enc(&header.log_version, 0);
	le16enc(&header.version, 1);
	le32enc(&header.log_length, SIZE_1MB);
	le64enc(&header.log_offset, SIZE_1MB);
	checksum = crc32c(&header, sizeof(header));
	le32enc(&header.checksum, checksum);
	error = vhdx_write_and_pad(fd, &header, sizeof(header), SIZE_64KB);
	if (error)
		return (error);

	/* Write header 2, and make it active */
	le32enc(&header.sequence_number, 1);
	header.checksum = 0;
	checksum = crc32c(&header, sizeof(header));
	le32enc(&header.checksum, checksum);
	return vhdx_write_and_pad(fd, &header, sizeof(header), SIZE_64KB);
}

static int
vhdx_write_region_tables(int fd)
{
	int error;
	uint8_t *region_table;
	struct vhdx_region_table_header header;
	struct vhdx_region_table_entry entry;
	uint32_t checksum;

	region_table = malloc(SIZE_64KB);
	if (region_table == NULL)
		return errno;
	memset(region_table, 0, SIZE_64KB);

	memset(&header, 0, sizeof(header));
	le32enc(&header.signature, VHDX_REGION_TABLE_HEADER_SIGNATURE);
	le32enc(&header.entry_count, 2);
	memcpy(region_table, &header, sizeof(header));

	/* Metadata region entry */
	mkimg_uuid_enc(&entry.guid, &vhdx_metadata_guid);
	le64enc(&entry.file_offset, 2*SIZE_1MB);
	le64enc(&entry.length, SIZE_1MB);
	memcpy(region_table + sizeof(header),
	    &entry, sizeof(entry));

	/* BAT region entry */
	mkimg_uuid_enc(&entry.guid, &vhdx_bat_guid);
	le64enc(&entry.file_offset, 3*SIZE_1MB);
	le64enc(&entry.length, SIZE_1MB);
	memcpy(region_table + sizeof(header) + sizeof(entry),
	    &entry, sizeof(entry));

	checksum = crc32c(region_table, SIZE_64KB);
	le32enc(region_table + 4, checksum);

	/* Region Table 1 */
	if (sparse_write(fd, region_table, SIZE_64KB) < 0) {
		error = errno;
		free(region_table);
		return error;
	}

	/* Region Table 2 */
	if (sparse_write(fd, region_table, SIZE_64KB) < 0) {
		error = errno;
		free(region_table);
		return error;
	}

	free(region_table);
	return (0);
}

static int
vhdx_write_metadata(int fd, uint64_t image_size)
{
	int error;
	uint8_t *metadata;
	struct vhdx_metadata_table_header header;
	struct vhdx_metadata_table_entry entry;
	int header_ptr, data_ptr;
	mkimg_uuid_t id;

	metadata = malloc(SIZE_1MB);
	if (metadata == NULL)
		return errno;
	memset(metadata, 0, SIZE_1MB);

	memset(&header, 0, sizeof(header));
	memset(&entry, 0, sizeof(entry));

	le64enc(&header.signature, VHDX_METADATA_TABLE_HEADER_SIGNATURE);
	le16enc(&header.entry_count, 5);
	memcpy(metadata, &header, sizeof(header));
	header_ptr = sizeof(header);
	data_ptr = SIZE_64KB;

	/* File parameters */
	mkimg_uuid_enc(&entry.item_id, &vhdx_meta_file_parameters_guid);
	le32enc(&entry.offset, data_ptr);
	le32enc(&entry.length, 8);
	le32enc(&entry.flags, META_IS_REQUIRED);
	memcpy(metadata + header_ptr, &entry, sizeof(entry));
	header_ptr += sizeof(entry);
	le32enc(metadata + data_ptr, PAYLOAD_BLOCK_SIZE);
	data_ptr += 4;
	le32enc(metadata + data_ptr, 0);
	data_ptr += 4;

	/* Virtual Disk Size */
	mkimg_uuid_enc(&entry.item_id, &vhdx_meta_vdisk_size_guid);
	le32enc(&entry.offset, data_ptr);
	le32enc(&entry.length, 8);
	le32enc(&entry.flags, META_IS_REQUIRED | META_IS_VIRTUAL_DISK);
	memcpy(metadata + header_ptr, &entry, sizeof(entry));
	header_ptr += sizeof(entry);
	le64enc(metadata + data_ptr, image_size);
	data_ptr += 8;

	/* Virtual Disk ID */
	mkimg_uuid_enc(&entry.item_id, &vhdx_meta_vdisk_id_guid);
	le32enc(&entry.offset, data_ptr);
	le32enc(&entry.length, 16);
	le32enc(&entry.flags, META_IS_REQUIRED | META_IS_VIRTUAL_DISK);
	memcpy(metadata + header_ptr, &entry, sizeof(entry));
	header_ptr += sizeof(entry);
	mkimg_uuid(&id);
	mkimg_uuid_enc(metadata + data_ptr, &id);
	data_ptr += 16;

	/* Logical Sector Size*/
	mkimg_uuid_enc(&entry.item_id, &vhdx_meta_logical_ssize_guid);
	le32enc(&entry.offset, data_ptr);
	le32enc(&entry.length, 4);
	le32enc(&entry.flags, META_IS_REQUIRED | META_IS_VIRTUAL_DISK);
	memcpy(metadata + header_ptr, &entry, sizeof(entry));
	header_ptr += sizeof(entry);
	le32enc(metadata + data_ptr, secsz);
	data_ptr += 4;

	/* Physical Sector Size*/
	mkimg_uuid_enc(&entry.item_id, &vhdx_meta_phys_ssize_guid);
	le32enc(&entry.offset, data_ptr);
	le32enc(&entry.length, 4);
	le32enc(&entry.flags, META_IS_REQUIRED | META_IS_VIRTUAL_DISK);
	memcpy(metadata + header_ptr, &entry, sizeof(entry));
	header_ptr += sizeof(entry);
	le32enc(metadata + data_ptr, blksz);
	data_ptr += 4;

	if (sparse_write(fd, metadata, SIZE_1MB) < 0) {
		error = errno;
		free(metadata);
		return error;
	}

	free(metadata);
	return (0);
}

static int
vhdx_write_bat(int fd, uint64_t image_size)
{
	int error;
	uint8_t *bat;
	int chunk_ratio;
	uint64_t bat_size, data_block_count, total_bat_entries;
	uint64_t idx, payload_offset, bat_ptr;

	bat = malloc(SIZE_1MB);
	if (bat == NULL)
		return errno;
	memset(bat, 0, SIZE_1MB);

	chunk_ratio = ((1024*1024*8ULL) * secsz) / PAYLOAD_BLOCK_SIZE;
	data_block_count = (image_size + PAYLOAD_BLOCK_SIZE - 1) / PAYLOAD_BLOCK_SIZE;
	total_bat_entries = data_block_count + (data_block_count - 1)/chunk_ratio;
	bat_size = total_bat_entries * 8;
	/* round it up to 1Mb */
	bat_size = (bat_size + SIZE_1MB - 1) & ~(SIZE_1MB - 1);

	/*
	 * Offset to the first payload block
	 * 1Mb of header + 1Mb of log + 1Mb of metadata + XMb BAT
	 */
	payload_offset = 3 + (bat_size / SIZE_1MB);
	bat_ptr = 0;
	for (idx = 0; idx < data_block_count; idx++) {
		le64enc(bat + bat_ptr,
		    BAT_ENTRY(payload_offset, PAYLOAD_BLOCK_FULLY_PRESENT));
		bat_ptr += 8;
		payload_offset += (PAYLOAD_BLOCK_SIZE / SIZE_1MB);

		/* Flush the BAT buffer if required */
		if (bat_ptr == SIZE_1MB) {
			if (sparse_write(fd, bat, SIZE_1MB) < 0) {
				error = errno;
				free(bat);
				return error;
			}
			memset(bat, 0, SIZE_1MB);
			bat_ptr = 0;
		}

		if (((idx + 1) % chunk_ratio) == 0 &&
		    (idx != data_block_count - 1)) {
			le64enc(bat + bat_ptr,
			    BAT_ENTRY(0, SB_BLOCK_NOT_PRESENT));
			bat_ptr += 8;

			/* Flush the BAT buffer if required */
			if (bat_ptr == SIZE_1MB) {
				if (sparse_write(fd, bat, SIZE_1MB) < 0) {
					error = errno;
					free(bat);
					return error;
				}
				memset(bat, 0, SIZE_1MB);
				bat_ptr = 0;
			}
		}
	}

	if (bat_ptr != 0) {
		if (sparse_write(fd, bat, SIZE_1MB) < 0) {
			error = errno;
			free(bat);
			return error;
		}
	}

	free(bat);
	return (0);
}

static int
vhdx_write(int fd)
{
	int error;
	uint64_t imgsz, rawsz;
	struct vhdx_filetype_identifier identifier;

	rawsz = image_get_size() * secsz;
	imgsz = (rawsz + PAYLOAD_BLOCK_SIZE - 1) & ~(PAYLOAD_BLOCK_SIZE - 1);

	memset(&identifier, 0, sizeof(identifier));
	le64enc(&identifier.signature, VHDX_FILETYPE_ID_SIGNATURE);
	error = vhdx_write_and_pad(fd, &identifier, sizeof(identifier), SIZE_64KB);
	if (error)
		return (error);

	error = vhdx_write_headers(fd);
	if (error)
		return (error);


	error = vhdx_write_region_tables(fd);
	if (error)
		return (error);

	/* Reserved area */
	error = image_copyout_zeroes(fd, SIZE_1MB - 5*SIZE_64KB);

	/* Log */
	error = image_copyout_zeroes(fd, SIZE_1MB);
	if (error)
		return (error);

	error = vhdx_write_metadata(fd, imgsz);
	if (error)
		return (error);

	error = vhdx_write_bat(fd, imgsz);
	if (error)
		return (error);

	error = image_copyout(fd);
	if (error)
		return (error);

	return (0);
}

static struct mkimg_format vhdx_format = {
	.name = "vhdx",
	.description = "Virtual Hard Disk, version 2",
	.resize = vhdx_resize,
	.write = vhdx_write,
};

FORMAT_DEFINE(vhdx_format);
