/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2022 Intel Corporation */
/**
 *****************************************************************************
 * @file dc_header_footer.h
 *
 * @ingroup Dc_DataCompression
 *
 * @description
 *      Definition of the Data Compression header and footer parameters.
 *
 *****************************************************************************/
#ifndef DC_HEADER_FOOTER_H_
#define DC_HEADER_FOOTER_H_

/* Header and footer sizes for Zlib and Gzip */
#define DC_ZLIB_HEADER_SIZE (2)
#define DC_GZIP_HEADER_SIZE (10)
#define DC_ZLIB_FOOTER_SIZE (4)
#define DC_GZIP_FOOTER_SIZE (8)

/* Values used to build the headers for Zlib and Gzip */
#define DC_GZIP_ID1 (0x1f)
#define DC_GZIP_ID2 (0x8b)
#define DC_GZIP_FILESYSTYPE (0x03)
#define DC_ZLIB_WINDOWSIZE_OFFSET (4)
#define DC_ZLIB_FLEVEL_OFFSET (6)
#define DC_ZLIB_HEADER_OFFSET (31)

/* Compression level for Zlib */
#define DC_ZLIB_LEVEL_0 (0)
#define DC_ZLIB_LEVEL_1 (1)
#define DC_ZLIB_LEVEL_2 (2)
#define DC_ZLIB_LEVEL_3 (3)

/* CM parameter for Zlib */
#define DC_ZLIB_CM_DEFLATE (8)

/* Type of Gzip compression */
#define DC_GZIP_FAST_COMP (4)
#define DC_GZIP_MAX_COMP (2)

#endif /* DC_HEADER_FOOTER_H_ */
