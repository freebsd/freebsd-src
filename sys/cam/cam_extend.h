/*
 * Written by Julian Elischer (julian@tfs.com)
 * for TRW Financial Systems for use under the MACH(2.5) operating system.
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
 * $FreeBSD$
 */

#ifndef _CAM_CAM_EXTEND_H
#define _CAM_CAM_EXTEND_H 1

#ifdef _KERNEL
struct extend_array;

void *cam_extend_get(struct extend_array *ea, int index);	
struct extend_array *cam_extend_new(void);
void *cam_extend_set(struct extend_array *ea, int index, void *value);
void cam_extend_release(struct extend_array *ea, int index);

#endif
#endif /* _CAM_CAM_EXTEND_H */
