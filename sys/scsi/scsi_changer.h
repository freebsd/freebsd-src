/*
 * SCSI changer interface description
 */

/*
 * Written by Stefan Grefen   (grefen@goofy.zdv.uni-mainz.de soon grefen@convex.com)
 * based on the SCSI System by written Julian Elischer (julian@tfs.com)
 * for TRW Financial Systems.
 *
 * TRW Financial Systems, in accordance with their agreement with Carnegie
 * Mellon University, makes this software available to CMU to distribute
 * or use in any manner that they see fit as long as this message is kept with 
 * the software. For this reason TFS also grants any other persons or
 * organisations permission to use or modify this software.
 *
 * TFS supplies this software to be publicly redistributed
 * on the understanding that TFS is not responsible for the correct
 * functioning of this software in any circumstances.
 *
 * Ported to run under 386BSD by Julian Elischer (julian@tfs.com) Sept 1992
 *
 *	$Id: scsi_changer.h,v 1.6 1993/11/18 05:02:53 rgrimes Exp $
 */
#ifndef _SCSI_SCSI_CHANGER_H
#define _SCSI_SCSI_CHANGER_H 1

/*
 * SCSI command format
 */
struct scsi_read_element_status
{
        u_char  op_code;
	u_char	byte2;
#define	SRES_ELEM_TYPE_CODE 	0x0F
#define	SRES_ELEM_VOLTAG 	0x10
	u_char	starting_element_addr[2];
	u_char	number_of_elements[2];
	u_char	resv1;
	u_char	allocation_length[3];
	u_char	resv2;
	u_char	control;
};
#define RE_ALL_ELEMENTS			0
#define RE_MEDIUM_TRANSPORT_ELEMENT	1
#define RE_STORAGE_ELEMENT		2
#define RE_IMPORT_EXPORT		3
#define RE_DATA_TRANSFER_ELEMENT	4

struct scsi_move_medium
{
	u_char  op_code;
	u_char	byte2;
	u_char  transport_element_address[2];
	u_char  source_address[2];
	u_char  destination_address[2];
	u_char  rsvd[2];
	u_char  invert;
	u_char	control;
};

struct scsi_position_to_element
{
	u_char  op_code;
        u_char  byte2;
	u_char  transport_element_address[2];
	u_char  source_address[2];
	u_char  rsvd[2];
	u_char  invert;
	u_char	control;
};
	
/*
 * Opcodes
 */
#define POSITION_TO_ELEMENT     0x2b
#define MOVE_MEDIUM             0xa5
#define READ_ELEMENT_STATUS     0xb8

struct scsi_element_status_data 
{
	u_char	first_element_reported[2];
	u_char	number_of_elements_reported[2];
	u_char  rsvd;
	u_char	byte_count_of_report[3];
};

struct element_status_page 
{
	u_char	element_type_code;
	u_char	flags;
#define	ESP_AVOLTAG	0x40
#define	ESP_PVOLTAG	0x80
	u_char element_descriptor_length[2];
	u_char rsvd;
	u_char byte_count_of_descriptor_data[3];
};
#endif /*_SCSI_SCSI_CHANGER_H*/

