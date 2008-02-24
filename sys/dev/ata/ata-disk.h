/*-
 * Copyright (c) 1998 - 2007 Søren Schmidt <sos@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer,
 *    without modification, immediately at the beginning of the file.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD: src/sys/dev/ata/ata-disk.h,v 1.52 2007/02/21 19:07:18 sos Exp $
 */

/* structure describing an ATA disk */
struct ad_softc {  
    u_int64_t                   total_secs;     /* total # of sectors (LBA) */
    u_int8_t                    heads;
    u_int8_t                    sectors;
    u_int32_t                   transfersize;   /* size of each transfer */
    int                         num_tags;       /* number of tags supported */
    int                         flags;          /* drive flags */
#define         AD_F_LABELLING          0x0001          
#define         AD_F_CHS_USED           0x0002
#define         AD_F_32B_ENABLED        0x0004
#define         AD_F_TAG_ENABLED        0x0008
#define         AD_F_RAID_SUBDISK       0x0010

    struct disk                 *disk;          /* disklabel/slice stuff */
};

extern devclass_t ad_devclass;

