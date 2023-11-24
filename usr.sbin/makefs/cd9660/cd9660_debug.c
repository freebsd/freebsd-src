/*	$NetBSD: cd9660_debug.c,v 1.11 2010/10/27 18:51:35 christos Exp $	*/

/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2005 Daniel Watt, Walter Deignan, Ryan Gabrys, Alan
 * Perez-Rathke and Ram Vedam.  All rights reserved.
 *
 * This code was written by Daniel Watt, Walter Deignan, Ryan Gabrys,
 * Alan Perez-Rathke and Ram Vedam.
 *
 * Redistribution and use in source and binary forms, with or
 * without modification, are permitted provided that the following
 * conditions are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY DANIEL WATT, WALTER DEIGNAN, RYAN
 * GABRYS, ALAN PEREZ-RATHKE AND RAM VEDAM ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL DANIEL WATT, WALTER DEIGNAN, RYAN
 * GABRYS, ALAN PEREZ-RATHKE AND RAM VEDAM BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE,DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 */

#include <sys/param.h>

#include <sys/mount.h>

#include "makefs.h"
#include "cd9660.h"
#include "iso9660_rrip.h"

static void debug_print_susp_attrs(cd9660node *, int);
static void debug_dump_to_xml_padded_hex_output(const char *, const char *,
						int);

static inline void
print_n_tabs(int n)
{
	int i;

	for (i = 1; i <= n; i ++)
		printf("\t");
}

#if 0
void
debug_print_rrip_info(cd9660node *n)
{
	struct ISO_SUSP_ATTRIBUTES *t;
	TAILQ_FOREACH(t, &node->head, rr_ll) {

	}
}
#endif

static void
debug_print_susp_attrs(cd9660node *n, int indent)
{
	struct ISO_SUSP_ATTRIBUTES *t;

	TAILQ_FOREACH(t, &n->head, rr_ll) {
		print_n_tabs(indent);
		printf("-");
		printf("%c%c: L:%i",t->attr.su_entry.SP.h.type[0],
		    t->attr.su_entry.SP.h.type[1],
		    (int)t->attr.su_entry.SP.h.length[0]);
		printf("\n");
	}
}

void
debug_print_tree(iso9660_disk *diskStructure, cd9660node *node, int level)
{
#if !HAVE_NBTOOL_CONFIG_H
	cd9660node *cn;

	print_n_tabs(level);
	if (node->type & CD9660_TYPE_DOT) {
		printf(". (%i)\n",
		    isonum_733(node->isoDirRecord->extent));
	} else if (node->type & CD9660_TYPE_DOTDOT) {
		printf("..(%i)\n",
		    isonum_733(node->isoDirRecord->extent));
	} else if (node->isoDirRecord->name[0]=='\0') {
		printf("(ROOT) (%" PRIu32 " to %" PRId64 ")\n",
		    node->fileDataSector,
		    node->fileDataSector +
			node->fileSectorsUsed - 1);
	} else {
		printf("%s (%s) (%" PRIu32 " to %" PRId64 ")\n",
		    node->isoDirRecord->name,
		    (node->isoDirRecord->flags[0]
			& ISO_FLAG_DIRECTORY) ?  "DIR" : "FILE",
		    node->fileDataSector,
		    (node->fileSectorsUsed == 0) ?
			node->fileDataSector :
			node->fileDataSector
			    + node->fileSectorsUsed - 1);
	}
	if (diskStructure->rock_ridge_enabled)
		debug_print_susp_attrs(node, level + 1);
	TAILQ_FOREACH(cn, &node->cn_children, cn_next_child)
		debug_print_tree(diskStructure, cn, level + 1);
#else
	printf("Sorry, debugging is not supported in host-tools mode.\n");
#endif
}

void
debug_print_path_tree(cd9660node *n)
{
	cd9660node *iterator = n;

	/* Only display this message when called with the root node */
	if (n->parent == NULL)
		printf("debug_print_path_table: Dumping path table contents\n");

	while (iterator != NULL) {
		if (iterator->isoDirRecord->name[0] == '\0')
			printf("0) (ROOT)\n");
		else
			printf("%i) %s\n", iterator->level,
			    iterator->isoDirRecord->name);

		iterator = iterator->ptnext;
	}
}

void
debug_print_volume_descriptor_information(iso9660_disk *diskStructure)
{
	volume_descriptor *tmp = diskStructure->firstVolumeDescriptor;
	char temp[CD9660_SECTOR_SIZE];

	printf("==Listing Volume Descriptors==\n");

	while (tmp != NULL) {
		memset(temp, 0, CD9660_SECTOR_SIZE);
		memcpy(temp, tmp->volumeDescriptorData + 1, 5);
		printf("Volume descriptor in sector %" PRId64
		    ": type %i, ID %s\n",
		    tmp->sector, tmp->volumeDescriptorData[0], temp);
		switch(tmp->volumeDescriptorData[0]) {
		case 0:/*boot record*/
			break;

		case 1:		/* PVD */
			break;

		case 2:		/* SVD */
			break;

		case 3:		/* Volume Partition Descriptor */
			break;

		case 255:	/* terminator */
			break;
		}
		tmp = tmp->next;
	}

	printf("==Done Listing Volume Descriptors==\n");
}

void
debug_dump_to_xml_ptentry(path_table_entry *pttemp, int num, int mode)
{
	printf("<ptentry num=\"%i\">\n" ,num);
	printf("<length>%i</length>\n", pttemp->length[0]);
	printf("<extended_attribute_length>%i</extended_attribute_length>\n",
	    pttemp->extended_attribute_length[0]);
	printf("<parent_number>%i</parent_number>\n",
	    debug_get_encoded_number(pttemp->parent_number,mode));
	debug_dump_to_xml_padded_hex_output("name",
	    pttemp->name, pttemp->length[0]);
	printf("</ptentry>\n");
}

void
debug_dump_to_xml_path_table(FILE *fd, off_t sector, int size, int mode)
{
	path_table_entry pttemp;
	int t = 0;
	int n = 0;

	if (fseeko(fd, CD9660_SECTOR_SIZE * sector, SEEK_SET) == -1)
		err(1, "fseeko");

	while (t < size) {
		/* Read fixed data first */
		fread(&pttemp, 1, 8, fd);
		t += 8;
		/* Read variable */
		fread(((unsigned char*)&pttemp) + 8, 1, pttemp.length[0], fd);
		t += pttemp.length[0];
		debug_dump_to_xml_ptentry(&pttemp, n, mode);
		n++;
	}

}

/*
 * XML Debug output functions
 * Dump hierarchy of CD, as well as volume info, to XML
 * Can be used later to diff against a standard,
 * or just provide easy to read detailed debug output
 */
void
debug_dump_to_xml(FILE *fd)
{
	unsigned char buf[CD9660_SECTOR_SIZE];
	off_t sector;
	int t, t2;
	struct iso_primary_descriptor primaryVD;
	struct _boot_volume_descriptor bootVD;

	printf("<cd9660dump>\n");

	/* Display Volume Descriptors */
	sector = 16;
	do {
		if (fseeko(fd, CD9660_SECTOR_SIZE * sector, SEEK_SET) == -1)
			err(1, "fseeko");
		fread(buf, 1, CD9660_SECTOR_SIZE, fd);
		t = (int)((unsigned char)buf[0]);
		switch (t) {
		case 0:
			memcpy(&bootVD, buf, CD9660_SECTOR_SIZE);
			break;
		case 1:
			memcpy(&primaryVD, buf, CD9660_SECTOR_SIZE);
			break;
		}
		debug_dump_to_xml_volume_descriptor(buf, sector);
		sector++;
	} while (t != 255);

	t = debug_get_encoded_number((u_char *)primaryVD.type_l_path_table,
	    731);
	t2 = debug_get_encoded_number((u_char *)primaryVD.path_table_size, 733);
	printf("Path table 1 located at sector %i and is %i bytes long\n",
	    t,t2);
	debug_dump_to_xml_path_table(fd, t, t2, 721);

	t = debug_get_encoded_number((u_char *)primaryVD.type_m_path_table,
	    731);
	debug_dump_to_xml_path_table(fd, t, t2, 722);

	printf("</cd9660dump>\n");
}

static void
debug_dump_to_xml_padded_hex_output(const char *element, const char *buf,
    int len)
{
	int i;
	int t;

	printf("<%s>",element);
	for (i = 0; i < len; i++) {
		t = (unsigned char)buf[i];
		if (t >= 32 && t < 127)
			printf("%c",t);
	}
	printf("</%s>\n",element);

	printf("<%s:hex>",element);
	for (i = 0; i < len; i++) {
		t = (unsigned char)buf[i];
		printf(" %x",t);
	}
	printf("</%s:hex>\n",element);
}

int
debug_get_encoded_number(const unsigned char* buf, int mode)
{
#if !HAVE_NBTOOL_CONFIG_H
	switch (mode) {
	/* 711: Single bite */
	case 711:
		return isonum_711(buf);

	/* 712: Single signed byte */
	case 712:
		return isonum_712(buf);

	/* 721: 16 bit LE */
	case 721:
		return isonum_721(buf);

	/* 731: 32 bit LE */
	case 731:
		return isonum_731(buf);

	/* 722: 16 bit BE */
	case 722:
		return isonum_722(buf);

	/* 732: 32 bit BE */
	case 732:
		return isonum_732(buf);

	/* 723: 16 bit bothE */
	case 723:
		return isonum_723(buf);

	/* 733: 32 bit bothE */
	case 733:
		return isonum_733(buf);
	}
#endif
	return 0;
}

void
debug_dump_integer(const char *element, const unsigned char* buf, int mode)
{
	printf("<%s>%i</%s>\n", element, debug_get_encoded_number(buf, mode),
	    element);
}

void
debug_dump_string(const char *element __unused, const unsigned char *buf __unused, int len __unused)
{

}

void
debug_dump_directory_record_9_1(unsigned char* buf)
{
	struct iso_directory_record *rec = (struct iso_directory_record *)buf;
	printf("<directoryrecord>\n");
	debug_dump_integer("length", rec->length, 711);
	debug_dump_integer("ext_attr_length", rec->ext_attr_length, 711);
	debug_dump_integer("extent", rec->extent, 733);
	debug_dump_integer("size", rec->size, 733);
	debug_dump_integer("flags", rec->flags, 711);
	debug_dump_integer("file_unit_size", rec->file_unit_size, 711);
	debug_dump_integer("interleave", rec->interleave, 711);
	debug_dump_integer("volume_sequence_number",
	    rec->volume_sequence_number, 723);
	debug_dump_integer("name_len", rec->name_len, 711);
	debug_dump_to_xml_padded_hex_output("name", rec->name,
	    debug_get_encoded_number(rec->length, 711));
	printf("</directoryrecord>\n");
}


void
debug_dump_to_xml_volume_descriptor(unsigned char* buf, int sector)
{
	struct iso_primary_descriptor *desc =
	    (struct iso_primary_descriptor *)buf;

	printf("<volumedescriptor sector=\"%i\">\n", sector);
	printf("<vdtype>");
	switch(buf[0]) {
	case 0:
		printf("boot");
		break;

	case 1:
		printf("primary");
		break;

	case 2:
		printf("supplementary");
		break;

	case 3:
		printf("volume partition descriptor");
		break;

	case 255:
		printf("terminator");
		break;
	}

	printf("</vdtype>\n");
	switch(buf[0]) {
	case 1:
		debug_dump_integer("type", desc->type, 711);
		debug_dump_to_xml_padded_hex_output("id", desc->id,
		    ISODCL(2, 6));
		debug_dump_integer("version", (u_char *)desc->version, 711);
		debug_dump_to_xml_padded_hex_output("system_id",
		    desc->system_id, ISODCL(9, 40));
		debug_dump_to_xml_padded_hex_output("volume_id",
		    desc->volume_id, ISODCL(41, 72));
		debug_dump_integer("volume_space_size",
		    (u_char *)desc->volume_space_size, 733);
		debug_dump_integer("volume_set_size",
		    (u_char *)desc->volume_set_size, 733);
		debug_dump_integer("volume_sequence_number",
		    (u_char *)desc->volume_sequence_number, 723);
		debug_dump_integer("logical_block_size",
		    (u_char *)desc->logical_block_size, 723);
		debug_dump_integer("path_table_size",
		    (u_char *)desc->path_table_size, 733);
		debug_dump_integer("type_l_path_table",
		    (u_char *)desc->type_l_path_table, 731);
		debug_dump_integer("opt_type_l_path_table",
		    (u_char *)desc->opt_type_l_path_table, 731);
		debug_dump_integer("type_m_path_table",
		    (u_char *)desc->type_m_path_table, 732);
		debug_dump_integer("opt_type_m_path_table",
		    (u_char *)desc->opt_type_m_path_table, 732);
		debug_dump_directory_record_9_1(
		    (u_char *)desc->root_directory_record);
		debug_dump_to_xml_padded_hex_output("volume_set_id",
		    desc->volume_set_id, ISODCL(191, 318));
		debug_dump_to_xml_padded_hex_output("publisher_id",
		    desc->publisher_id, ISODCL(319, 446));
		debug_dump_to_xml_padded_hex_output("preparer_id",
		    desc->preparer_id, ISODCL(447, 574));
		debug_dump_to_xml_padded_hex_output("application_id",
		    desc->application_id, ISODCL(575, 702));
		debug_dump_to_xml_padded_hex_output("copyright_file_id",
		    desc->copyright_file_id, ISODCL(703, 739));
		debug_dump_to_xml_padded_hex_output("abstract_file_id",
		    desc->abstract_file_id, ISODCL(740, 776));
		debug_dump_to_xml_padded_hex_output("bibliographic_file_id",
		    desc->bibliographic_file_id, ISODCL(777, 813));

		debug_dump_to_xml_padded_hex_output("creation_date",
		    desc->creation_date, ISODCL(814, 830));
		debug_dump_to_xml_padded_hex_output("modification_date",
		    desc->modification_date, ISODCL(831, 847));
		debug_dump_to_xml_padded_hex_output("expiration_date",
		    desc->expiration_date, ISODCL(848, 864));
		debug_dump_to_xml_padded_hex_output("effective_date",
		    desc->effective_date, ISODCL(865, 881));

		debug_dump_to_xml_padded_hex_output("file_structure_version",
		    desc->file_structure_version, ISODCL(882, 882));
		break;
	}
	printf("</volumedescriptor>\n");
}

