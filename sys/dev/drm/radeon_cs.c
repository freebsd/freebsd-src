/*-
 * Copyright 2008 Jerome Glisse.
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * PRECISION INSIGHT AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *    Jerome Glisse <glisse@freedesktop.org>
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");
#include "dev/drm/drmP.h"
#include "dev/drm/radeon_drm.h"
#include "dev/drm/radeon_drv.h"

/* regs */
#define AVIVO_D1MODE_VLINE_START_END                           0x6538
#define AVIVO_D2MODE_VLINE_START_END                           0x6d38
#define R600_CP_COHER_BASE                                     0x85f8
#define R600_DB_DEPTH_BASE                                     0x2800c
#define R600_CB_COLOR0_BASE                                    0x28040
#define R600_CB_COLOR1_BASE                                    0x28044
#define R600_CB_COLOR2_BASE                                    0x28048
#define R600_CB_COLOR3_BASE                                    0x2804c
#define R600_CB_COLOR4_BASE                                    0x28050
#define R600_CB_COLOR5_BASE                                    0x28054
#define R600_CB_COLOR6_BASE                                    0x28058
#define R600_CB_COLOR7_BASE                                    0x2805c
#define R600_SQ_PGM_START_FS                                   0x28894
#define R600_SQ_PGM_START_ES                                   0x28880
#define R600_SQ_PGM_START_VS                                   0x28858
#define R600_SQ_PGM_START_GS                                   0x2886c
#define R600_SQ_PGM_START_PS                                   0x28840
#define R600_VGT_DMA_BASE                                      0x287e8
#define R600_VGT_DMA_BASE_HI                                   0x287e4
#define R600_VGT_STRMOUT_BASE_OFFSET_0                         0x28b10
#define R600_VGT_STRMOUT_BASE_OFFSET_1                         0x28b14
#define R600_VGT_STRMOUT_BASE_OFFSET_2                         0x28b18
#define R600_VGT_STRMOUT_BASE_OFFSET_3                         0x28b1c
#define R600_VGT_STRMOUT_BASE_OFFSET_HI_0                      0x28b44
#define R600_VGT_STRMOUT_BASE_OFFSET_HI_1                      0x28b48
#define R600_VGT_STRMOUT_BASE_OFFSET_HI_2                      0x28b4c
#define R600_VGT_STRMOUT_BASE_OFFSET_HI_3                      0x28b50
#define R600_VGT_STRMOUT_BUFFER_BASE_0                         0x28ad8
#define R600_VGT_STRMOUT_BUFFER_BASE_1                         0x28ae8
#define R600_VGT_STRMOUT_BUFFER_BASE_2                         0x28af8
#define R600_VGT_STRMOUT_BUFFER_BASE_3                         0x28b08
#define R600_VGT_STRMOUT_BUFFER_OFFSET_0                       0x28adc
#define R600_VGT_STRMOUT_BUFFER_OFFSET_1                       0x28aec
#define R600_VGT_STRMOUT_BUFFER_OFFSET_2                       0x28afc
#define R600_VGT_STRMOUT_BUFFER_OFFSET_3                       0x28b0c

/* resource type */
#define R600_SQ_TEX_VTX_INVALID_TEXTURE                        0x0
#define R600_SQ_TEX_VTX_INVALID_BUFFER                         0x1
#define R600_SQ_TEX_VTX_VALID_TEXTURE                          0x2
#define R600_SQ_TEX_VTX_VALID_BUFFER                           0x3

/* packet 3 type offsets */
#define R600_SET_CONFIG_REG_OFFSET                             0x00008000
#define R600_SET_CONFIG_REG_END                                0x0000ac00
#define R600_SET_CONTEXT_REG_OFFSET                            0x00028000
#define R600_SET_CONTEXT_REG_END                               0x00029000
#define R600_SET_ALU_CONST_OFFSET                              0x00030000
#define R600_SET_ALU_CONST_END                                 0x00032000
#define R600_SET_RESOURCE_OFFSET                               0x00038000
#define R600_SET_RESOURCE_END                                  0x0003c000
#define R600_SET_SAMPLER_OFFSET                                0x0003c000
#define R600_SET_SAMPLER_END                                   0x0003cff0
#define R600_SET_CTL_CONST_OFFSET                              0x0003cff0
#define R600_SET_CTL_CONST_END                                 0x0003e200
#define R600_SET_LOOP_CONST_OFFSET                             0x0003e200
#define R600_SET_LOOP_CONST_END                                0x0003e380
#define R600_SET_BOOL_CONST_OFFSET                             0x0003e380
#define R600_SET_BOOL_CONST_END                                0x00040000

/* Packet 3 types */
#define R600_IT_INDIRECT_BUFFER_END               0x00001700
#define R600_IT_SET_PREDICATION                   0x00002000
#define R600_IT_REG_RMW                           0x00002100
#define R600_IT_COND_EXEC                         0x00002200
#define R600_IT_PRED_EXEC                         0x00002300
#define R600_IT_START_3D_CMDBUF                   0x00002400
#define R600_IT_DRAW_INDEX_2                      0x00002700
#define R600_IT_CONTEXT_CONTROL                   0x00002800
#define R600_IT_DRAW_INDEX_IMMD_BE                0x00002900
#define R600_IT_INDEX_TYPE                        0x00002A00
#define R600_IT_DRAW_INDEX                        0x00002B00
#define R600_IT_DRAW_INDEX_AUTO                   0x00002D00
#define R600_IT_DRAW_INDEX_IMMD                   0x00002E00
#define R600_IT_NUM_INSTANCES                     0x00002F00
#define R600_IT_STRMOUT_BUFFER_UPDATE             0x00003400
#define R600_IT_INDIRECT_BUFFER_MP                0x00003800
#define R600_IT_MEM_SEMAPHORE                     0x00003900
#define R600_IT_MPEG_INDEX                        0x00003A00
#define R600_IT_WAIT_REG_MEM                      0x00003C00
#define R600_IT_MEM_WRITE                         0x00003D00
#define R600_IT_INDIRECT_BUFFER                   0x00003200
#define R600_IT_CP_INTERRUPT                      0x00004000
#define R600_IT_SURFACE_SYNC                      0x00004300
#define R600_IT_ME_INITIALIZE                     0x00004400
#define R600_IT_COND_WRITE                        0x00004500
#define R600_IT_EVENT_WRITE                       0x00004600
#define R600_IT_EVENT_WRITE_EOP                   0x00004700
#define R600_IT_ONE_REG_WRITE                     0x00005700
#define R600_IT_SET_CONFIG_REG                    0x00006800
#define R600_IT_SET_CONTEXT_REG                   0x00006900
#define R600_IT_SET_ALU_CONST                     0x00006A00
#define R600_IT_SET_BOOL_CONST                    0x00006B00
#define R600_IT_SET_LOOP_CONST                    0x00006C00
#define R600_IT_SET_RESOURCE                      0x00006D00
#define R600_IT_SET_SAMPLER                       0x00006E00
#define R600_IT_SET_CTL_CONST                     0x00006F00
#define R600_IT_SURFACE_BASE_UPDATE               0x00007300

int radeon_cs_ioctl(struct drm_device *dev, void *data, struct drm_file *fpriv)
{
	struct drm_radeon_cs_parser parser;
	struct drm_radeon_private *dev_priv = dev->dev_private;
	struct drm_radeon_cs *cs = data;
	uint32_t cs_id;
	struct drm_radeon_cs_chunk __user **chunk_ptr = NULL;
	uint64_t *chunk_array;
	uint64_t *chunk_array_ptr;
	long size;
	int r, i;

	mtx_lock(&dev_priv->cs.cs_mutex);
	/* set command stream id to 0 which is fake id */
	cs_id = 0;
	cs->cs_id = cs_id;

	if (dev_priv == NULL) {
		DRM_ERROR("called with no initialization\n");
		mtx_unlock(&dev_priv->cs.cs_mutex);
		return -EINVAL;
	}
	if (!cs->num_chunks) {
		mtx_unlock(&dev_priv->cs.cs_mutex);
		return 0;
	}


	chunk_array = drm_calloc(cs->num_chunks, sizeof(uint64_t), DRM_MEM_DRIVER);
	if (!chunk_array) {
		mtx_unlock(&dev_priv->cs.cs_mutex);
		return -ENOMEM;
	}

	chunk_array_ptr = (uint64_t *)(unsigned long)(cs->chunks);

	if (DRM_COPY_FROM_USER(chunk_array, chunk_array_ptr, sizeof(uint64_t)*cs->num_chunks)) {
		r = -EFAULT;
		goto out;
	}

	parser.dev = dev;
	parser.file_priv = fpriv;
	parser.reloc_index = -1;
	parser.ib_index = -1;
	parser.num_chunks = cs->num_chunks;
	/* copy out the chunk headers */
	parser.chunks = drm_calloc(parser.num_chunks, sizeof(struct drm_radeon_kernel_chunk), DRM_MEM_DRIVER);
	if (!parser.chunks) {
		r = -ENOMEM;
		goto out;
	}

	for (i = 0; i < parser.num_chunks; i++) {
		struct drm_radeon_cs_chunk user_chunk;

		chunk_ptr = (void __user *)(unsigned long)chunk_array[i];

		if (DRM_COPY_FROM_USER(&user_chunk, chunk_ptr, sizeof(struct drm_radeon_cs_chunk))){
			r = -EFAULT;
			goto out;
		}
		parser.chunks[i].chunk_id = user_chunk.chunk_id;

		if (parser.chunks[i].chunk_id == RADEON_CHUNK_ID_RELOCS)
			parser.reloc_index = i;

		if (parser.chunks[i].chunk_id == RADEON_CHUNK_ID_IB)
			parser.ib_index = i;

		if (parser.chunks[i].chunk_id == RADEON_CHUNK_ID_OLD) {
			parser.ib_index = i;
			parser.reloc_index = -1;
		}

		parser.chunks[i].length_dw = user_chunk.length_dw;
		parser.chunks[i].chunk_data = (uint32_t *)(unsigned long)user_chunk.chunk_data;

		parser.chunks[i].kdata = NULL;
		size = parser.chunks[i].length_dw * sizeof(uint32_t);

		switch(parser.chunks[i].chunk_id) {
		case RADEON_CHUNK_ID_IB:
		case RADEON_CHUNK_ID_OLD:
			if (size == 0) {
				r = -EINVAL;
				goto out;
			}
		case RADEON_CHUNK_ID_RELOCS:
			if (size) {
				parser.chunks[i].kdata = drm_alloc(size, DRM_MEM_DRIVER);
				if (!parser.chunks[i].kdata) {
					r = -ENOMEM;
					goto out;
				}

				if (DRM_COPY_FROM_USER(parser.chunks[i].kdata, parser.chunks[i].chunk_data, size)) {
					r = -EFAULT;
					goto out;
				}
			} else
				parser.chunks[i].kdata = NULL;
			break;
		default:
			break;
		}
		DRM_DEBUG("chunk %d %d %d %p\n", i, parser.chunks[i].chunk_id, parser.chunks[i].length_dw,
			  parser.chunks[i].chunk_data);
	}

	if (parser.chunks[parser.ib_index].length_dw > (16 * 1024)) {
		DRM_ERROR("cs->dwords too big: %d\n", parser.chunks[parser.ib_index].length_dw);
		r = -EINVAL;
		goto out;
	}

	/* get ib */
	r = dev_priv->cs.ib_get(&parser);
	if (r) {
		DRM_ERROR("ib_get failed\n");
		goto out;
	}

	/* now parse command stream */
	r = dev_priv->cs.parse(&parser);
	if (r) {
		goto out;
	}

out:
	dev_priv->cs.ib_free(&parser, r);

	/* emit cs id sequence */
	dev_priv->cs.id_emit(&parser, &cs_id);

	cs->cs_id = cs_id;

	mtx_unlock(&dev_priv->cs.cs_mutex);

	for (i = 0; i < parser.num_chunks; i++) {
		if (parser.chunks[i].kdata)
			drm_free(parser.chunks[i].kdata, parser.chunks[i].length_dw * sizeof(uint32_t), DRM_MEM_DRIVER);
	}

	drm_free(parser.chunks, sizeof(struct drm_radeon_kernel_chunk)*parser.num_chunks, DRM_MEM_DRIVER);
	drm_free(chunk_array, sizeof(uint64_t)*parser.num_chunks, DRM_MEM_DRIVER);

	return r;
}

/* for non-mm */
static int r600_nomm_relocate(struct drm_radeon_cs_parser *parser, uint32_t *reloc, uint64_t *offset)
{
	struct drm_device *dev = parser->dev;
	drm_radeon_private_t *dev_priv = dev->dev_private;
	struct drm_radeon_kernel_chunk *reloc_chunk = &parser->chunks[parser->reloc_index];
	uint32_t offset_dw = reloc[1];

	//DRM_INFO("reloc: 0x%08x 0x%08x\n", reloc[0], reloc[1]);
	//DRM_INFO("length: %d\n", reloc_chunk->length_dw);

	if (!reloc_chunk->kdata)
		return -EINVAL;

	if (offset_dw > reloc_chunk->length_dw) {
		DRM_ERROR("Offset larger than chunk 0x%x %d\n", offset_dw, reloc_chunk->length_dw);
		return -EINVAL;
	}

	/* 40 bit addr */
	*offset = reloc_chunk->kdata[offset_dw + 3];
	*offset <<= 32;
	*offset |= reloc_chunk->kdata[offset_dw + 0];

	//DRM_INFO("offset 0x%lx\n", *offset);

	if (!radeon_check_offset(dev_priv, *offset)) {
		DRM_ERROR("bad offset! 0x%lx\n", (unsigned long)*offset);
		return -EINVAL;
	}

	return 0;
}

static inline int r600_cs_packet0(struct drm_radeon_cs_parser *parser, uint32_t *offset_dw_p)
{
	uint32_t hdr, num_dw, reg;
	int count_dw = 1;
	int ret = 0;
	uint32_t offset_dw = *offset_dw_p;
	int incr = 2;

	hdr = parser->chunks[parser->ib_index].kdata[offset_dw];
	num_dw = ((hdr & RADEON_CP_PACKET_COUNT_MASK) >> 16) + 2;
	reg = (hdr & 0xffff) << 2;

	while (count_dw < num_dw) {
		switch (reg) {
		case AVIVO_D1MODE_VLINE_START_END:
		case AVIVO_D2MODE_VLINE_START_END:
			break;
		default:
			ret = -EINVAL;
			DRM_ERROR("bad packet 0 reg: 0x%08x\n", reg);
			break;
		}
		if (ret)
			break;
		count_dw++;
		reg += 4;
	}
	*offset_dw_p += incr;
	return ret;
}

static inline int r600_cs_packet3(struct drm_radeon_cs_parser *parser, uint32_t *offset_dw_p)
{
	struct drm_device *dev = parser->dev;
	drm_radeon_private_t *dev_priv = dev->dev_private;
	uint32_t hdr, num_dw, start_reg, end_reg, reg;
	uint32_t *reloc;
	uint64_t offset;
	int ret = 0;
	uint32_t offset_dw = *offset_dw_p;
	int incr = 2;
	int i;
	struct drm_radeon_kernel_chunk *ib_chunk;

	ib_chunk = &parser->chunks[parser->ib_index];

	hdr = ib_chunk->kdata[offset_dw];
	num_dw = ((hdr & RADEON_CP_PACKET_COUNT_MASK) >> 16) + 2;

	/* just the ones we use for now, add more later */
	switch (hdr & 0xff00) {
	case R600_IT_START_3D_CMDBUF:
		//DRM_INFO("R600_IT_START_3D_CMDBUF\n");
		if ((dev_priv->flags & RADEON_FAMILY_MASK) >= CHIP_RV770)
			ret = -EINVAL;
		if (num_dw != 2)
			ret = -EINVAL;
		if (ret)
			DRM_ERROR("bad START_3D\n");
		break;
	case R600_IT_CONTEXT_CONTROL:
		//DRM_INFO("R600_IT_CONTEXT_CONTROL\n");
		if (num_dw != 3)
			ret = -EINVAL;
		if (ret)
			DRM_ERROR("bad CONTEXT_CONTROL\n");
		break;
	case R600_IT_INDEX_TYPE:
	case R600_IT_NUM_INSTANCES:
		//DRM_INFO("R600_IT_INDEX_TYPE/R600_IT_NUM_INSTANCES\n");
		if (num_dw != 2)
			ret = -EINVAL;
		if (ret)
			DRM_ERROR("bad INDEX_TYPE/NUM_INSTANCES\n");
		break;
	case R600_IT_DRAW_INDEX:
		//DRM_INFO("R600_IT_DRAW_INDEX\n");
		if (num_dw != 5) {
			ret = -EINVAL;
			DRM_ERROR("bad DRAW_INDEX\n");
			break;
		}
		reloc = ib_chunk->kdata + offset_dw + num_dw;
		ret = dev_priv->cs.relocate(parser, reloc, &offset);
		if (ret) {
			DRM_ERROR("bad DRAW_INDEX\n");
			break;
		}
		ib_chunk->kdata[offset_dw + 1] += (offset & 0xffffffff);
		ib_chunk->kdata[offset_dw + 2] += (upper_32_bits(offset) & 0xff);
		break;
	case R600_IT_DRAW_INDEX_AUTO:
		//DRM_INFO("R600_IT_DRAW_INDEX_AUTO\n");
		if (num_dw != 3)
			ret = -EINVAL;
		if (ret)
			DRM_ERROR("bad DRAW_INDEX_AUTO\n");
		break;
	case R600_IT_DRAW_INDEX_IMMD_BE:
	case R600_IT_DRAW_INDEX_IMMD:
		//DRM_INFO("R600_IT_DRAW_INDEX_IMMD\n");
		if (num_dw < 4)
			ret = -EINVAL;
		if (ret)
			DRM_ERROR("bad DRAW_INDEX_IMMD\n");
		break;
	case R600_IT_WAIT_REG_MEM:
		//DRM_INFO("R600_IT_WAIT_REG_MEM\n");
		if (num_dw != 7)
			ret = -EINVAL;
		/* bit 4 is reg (0) or mem (1) */
		if (ib_chunk->kdata[offset_dw + 1] & 0x10) {
			reloc = ib_chunk->kdata + offset_dw + num_dw;
			ret = dev_priv->cs.relocate(parser, reloc, &offset);
			if (ret) {
				DRM_ERROR("bad WAIT_REG_MEM\n");
				break;
			}
			ib_chunk->kdata[offset_dw + 2] += (offset & 0xffffffff);
			ib_chunk->kdata[offset_dw + 3] += (upper_32_bits(offset) & 0xff);
		}
		if (ret)
			DRM_ERROR("bad WAIT_REG_MEM\n");
		break;
	case R600_IT_SURFACE_SYNC:
		//DRM_INFO("R600_IT_SURFACE_SYNC\n");
		if (num_dw != 5)
			ret = -EINVAL;
		/* 0xffffffff/0x0 is flush all cache flag */
		else if ((ib_chunk->kdata[offset_dw + 2] == 0xffffffff) &&
			 (ib_chunk->kdata[offset_dw + 3] == 0))
			ret = 0;
		else {
			reloc = ib_chunk->kdata + offset_dw + num_dw;
			ret = dev_priv->cs.relocate(parser, reloc, &offset);
			if (ret) {
				DRM_ERROR("bad SURFACE_SYNC\n");
				break;
			}
			ib_chunk->kdata[offset_dw + 3] += ((offset >> 8) & 0xffffffff);
		}
		break;
	case R600_IT_EVENT_WRITE:
		//DRM_INFO("R600_IT_EVENT_WRITE\n");
		if ((num_dw != 4) && (num_dw != 2))
			ret = -EINVAL;
		if (num_dw > 2) {
			reloc = ib_chunk->kdata + offset_dw + num_dw;
			ret = dev_priv->cs.relocate(parser, reloc, &offset);
			if (ret) {
				DRM_ERROR("bad EVENT_WRITE\n");
				break;
			}
			ib_chunk->kdata[offset_dw + 2] += (offset & 0xffffffff);
			ib_chunk->kdata[offset_dw + 3] += (upper_32_bits(offset) & 0xff);
		}
		if (ret)
			DRM_ERROR("bad EVENT_WRITE\n");
		break;
	case R600_IT_EVENT_WRITE_EOP:
		//DRM_INFO("R600_IT_EVENT_WRITE_EOP\n");
		if (num_dw != 6) {
			ret = -EINVAL;
			DRM_ERROR("bad EVENT_WRITE_EOP\n");
			break;
		}
		reloc = ib_chunk->kdata + offset_dw + num_dw;
		ret = dev_priv->cs.relocate(parser, reloc, &offset);
		if (ret) {
			DRM_ERROR("bad EVENT_WRITE_EOP\n");
			break;
		}
		ib_chunk->kdata[offset_dw + 2] += (offset & 0xffffffff);
		ib_chunk->kdata[offset_dw + 3] += (upper_32_bits(offset) & 0xff);
		break;
	case R600_IT_SET_CONFIG_REG:
		//DRM_INFO("R600_IT_SET_CONFIG_REG\n");
		start_reg = (ib_chunk->kdata[offset_dw + 1] << 2) + R600_SET_CONFIG_REG_OFFSET;
		end_reg = 4 * (num_dw - 2) + start_reg - 4;
		if ((start_reg < R600_SET_CONFIG_REG_OFFSET) ||
		    (start_reg >= R600_SET_CONFIG_REG_END) ||
		    (end_reg >= R600_SET_CONFIG_REG_END))
			ret = -EINVAL;
		else {
			for (i = 0; i < (num_dw - 2); i++) {
				reg = start_reg + (4 * i);
				switch (reg) {
				case R600_CP_COHER_BASE:
					/* use R600_IT_SURFACE_SYNC */
					ret = -EINVAL;
					break;
				default:
					break;
				}
				if (ret)
					break;
			}
		}
		if (ret)
			DRM_ERROR("bad SET_CONFIG_REG\n");
		break;
	case R600_IT_SET_CONTEXT_REG:
		//DRM_INFO("R600_IT_SET_CONTEXT_REG\n");
		start_reg = ib_chunk->kdata[offset_dw + 1] << 2;
		start_reg += R600_SET_CONTEXT_REG_OFFSET;
		end_reg = 4 * (num_dw - 2) + start_reg - 4;
		if ((start_reg < R600_SET_CONTEXT_REG_OFFSET) ||
		    (start_reg >= R600_SET_CONTEXT_REG_END) ||
		    (end_reg >= R600_SET_CONTEXT_REG_END))
			ret = -EINVAL;
		else {
			for (i = 0; i < (num_dw - 2); i++) {
				reg = start_reg + (4 * i);
				switch (reg) {
				case R600_DB_DEPTH_BASE:
				case R600_CB_COLOR0_BASE:
				case R600_CB_COLOR1_BASE:
				case R600_CB_COLOR2_BASE:
				case R600_CB_COLOR3_BASE:
				case R600_CB_COLOR4_BASE:
				case R600_CB_COLOR5_BASE:
				case R600_CB_COLOR6_BASE:
				case R600_CB_COLOR7_BASE:
				case R600_SQ_PGM_START_FS:
				case R600_SQ_PGM_START_ES:
				case R600_SQ_PGM_START_VS:
				case R600_SQ_PGM_START_GS:
				case R600_SQ_PGM_START_PS:
					//DRM_INFO("reg: 0x%08x\n", reg);
					reloc = ib_chunk->kdata + offset_dw + num_dw + (i * 2);
					ret = dev_priv->cs.relocate(parser, reloc, &offset);
					if (ret) {
						DRM_ERROR("bad SET_CONTEXT_REG\n");
						break;
					}
					ib_chunk->kdata[offset_dw + 2 + i] +=
						((offset >> 8) & 0xffffffff);
					break;
				case R600_VGT_DMA_BASE:
				case R600_VGT_DMA_BASE_HI:
					/* These should be handled by DRAW_INDEX packet 3 */
				case R600_VGT_STRMOUT_BASE_OFFSET_0:
				case R600_VGT_STRMOUT_BASE_OFFSET_1:
				case R600_VGT_STRMOUT_BASE_OFFSET_2:
				case R600_VGT_STRMOUT_BASE_OFFSET_3:
				case R600_VGT_STRMOUT_BASE_OFFSET_HI_0:
				case R600_VGT_STRMOUT_BASE_OFFSET_HI_1:
				case R600_VGT_STRMOUT_BASE_OFFSET_HI_2:
				case R600_VGT_STRMOUT_BASE_OFFSET_HI_3:
				case R600_VGT_STRMOUT_BUFFER_BASE_0:
				case R600_VGT_STRMOUT_BUFFER_BASE_1:
				case R600_VGT_STRMOUT_BUFFER_BASE_2:
				case R600_VGT_STRMOUT_BUFFER_BASE_3:
				case R600_VGT_STRMOUT_BUFFER_OFFSET_0:
				case R600_VGT_STRMOUT_BUFFER_OFFSET_1:
				case R600_VGT_STRMOUT_BUFFER_OFFSET_2:
				case R600_VGT_STRMOUT_BUFFER_OFFSET_3:
					/* These should be handled by STRMOUT_BUFFER packet 3 */
					DRM_ERROR("bad context reg: 0x%08x\n", reg);
					ret = -EINVAL;
					break;
				default:
					break;
				}
				if (ret)
					break;
			}
		}
		if (ret)
			DRM_ERROR("bad SET_CONTEXT_REG\n");
		break;
	case R600_IT_SET_RESOURCE:
		//DRM_INFO("R600_IT_SET_RESOURCE\n");
		if ((num_dw - 2) % 7)
			ret = -EINVAL;
		start_reg = ib_chunk->kdata[offset_dw + 1] << 2;
		start_reg += R600_SET_RESOURCE_OFFSET;
		end_reg = 4 * (num_dw - 2) + start_reg - 4;
		if ((start_reg < R600_SET_RESOURCE_OFFSET) ||
		    (start_reg >= R600_SET_RESOURCE_END) ||
		    (end_reg >= R600_SET_RESOURCE_END))
			ret = -EINVAL;
		else {
			for (i = 0; i < ((num_dw - 2) / 7); i++) {
				switch ((ib_chunk->kdata[offset_dw + (i * 7) + 6 + 2] & 0xc0000000) >> 30) {
				case R600_SQ_TEX_VTX_INVALID_TEXTURE:
				case R600_SQ_TEX_VTX_INVALID_BUFFER:
				default:
					ret = -EINVAL;
					break;
				case R600_SQ_TEX_VTX_VALID_TEXTURE:
					/* tex base */
					reloc = ib_chunk->kdata + offset_dw + num_dw + (i * 4);
					ret = dev_priv->cs.relocate(parser, reloc, &offset);
					if (ret)
						break;
					ib_chunk->kdata[offset_dw + (i * 7) + 2 + 2] +=
						((offset >> 8) & 0xffffffff);
					/* tex mip base */
					reloc = ib_chunk->kdata + offset_dw + num_dw + (i * 4) + 2;
					ret = dev_priv->cs.relocate(parser, reloc, &offset);
					if (ret)
						break;
					ib_chunk->kdata[offset_dw + (i * 7) + 3 + 2] +=
						((offset >> 8) & 0xffffffff);
					break;
				case R600_SQ_TEX_VTX_VALID_BUFFER:
					/* vtx base */
					reloc = ib_chunk->kdata + offset_dw + num_dw + (i * 2);
					ret = dev_priv->cs.relocate(parser, reloc, &offset);
					if (ret)
						break;
					ib_chunk->kdata[offset_dw + (i * 7) + 0 + 2] += (offset & 0xffffffff);
					ib_chunk->kdata[offset_dw + (i * 7) + 2 + 2] += (upper_32_bits(offset) & 0xff);
					break;
				}
				if (ret)
					break;
			}
		}
		if (ret)
			DRM_ERROR("bad SET_RESOURCE\n");
		break;
	case R600_IT_SET_ALU_CONST:
		//DRM_INFO("R600_IT_SET_ALU_CONST\n");
		start_reg = ib_chunk->kdata[offset_dw + 1] << 2;
		start_reg += R600_SET_ALU_CONST_OFFSET;
		end_reg = 4 * (num_dw - 2) + start_reg - 4;
		if ((start_reg < R600_SET_ALU_CONST_OFFSET) ||
		    (start_reg >= R600_SET_ALU_CONST_END) ||
		    (end_reg >= R600_SET_ALU_CONST_END))
			ret = -EINVAL;
		if (ret)
			DRM_ERROR("bad SET_ALU_CONST\n");
		break;
	case R600_IT_SET_BOOL_CONST:
		//DRM_INFO("R600_IT_SET_BOOL_CONST\n");
		start_reg = ib_chunk->kdata[offset_dw + 1] << 2;
		start_reg += R600_SET_BOOL_CONST_OFFSET;
		end_reg = 4 * (num_dw - 2) + start_reg - 4;
		if ((start_reg < R600_SET_BOOL_CONST_OFFSET) ||
		    (start_reg >= R600_SET_BOOL_CONST_END) ||
		    (end_reg >= R600_SET_BOOL_CONST_END))
			ret = -EINVAL;
		if (ret)
			DRM_ERROR("bad SET_BOOL_CONST\n");
		break;
	case R600_IT_SET_LOOP_CONST:
		//DRM_INFO("R600_IT_SET_LOOP_CONST\n");
		start_reg = ib_chunk->kdata[offset_dw + 1] << 2;
		start_reg += R600_SET_LOOP_CONST_OFFSET;
		end_reg = 4 * (num_dw - 2) + start_reg - 4;
		if ((start_reg < R600_SET_LOOP_CONST_OFFSET) ||
		    (start_reg >= R600_SET_LOOP_CONST_END) ||
		    (end_reg >= R600_SET_LOOP_CONST_END))
			ret = -EINVAL;
		if (ret)
			DRM_ERROR("bad SET_LOOP_CONST\n");
		break;
	case R600_IT_SET_CTL_CONST:
		//DRM_INFO("R600_IT_SET_CTL_CONST\n");
		start_reg = ib_chunk->kdata[offset_dw + 1] << 2;
		start_reg += R600_SET_CTL_CONST_OFFSET;
		end_reg = 4 * (num_dw - 2) + start_reg - 4;
		if ((start_reg < R600_SET_CTL_CONST_OFFSET) ||
		    (start_reg >= R600_SET_CTL_CONST_END) ||
		    (end_reg >= R600_SET_CTL_CONST_END))
			ret = -EINVAL;
		if (ret)
			DRM_ERROR("bad SET_CTL_CONST\n");
		break;
	case R600_IT_SET_SAMPLER:
		//DRM_INFO("R600_IT_SET_SAMPLER\n");
		if ((num_dw - 2) % 3)
			ret = -EINVAL;
		start_reg = ib_chunk->kdata[offset_dw + 1] << 2;
		start_reg += R600_SET_SAMPLER_OFFSET;
		end_reg = 4 * (num_dw - 2) + start_reg - 4;
		if ((start_reg < R600_SET_SAMPLER_OFFSET) ||
		    (start_reg >= R600_SET_SAMPLER_END) ||
		    (end_reg >= R600_SET_SAMPLER_END))
			ret = -EINVAL;
		if (ret)
			DRM_ERROR("bad SET_SAMPLER\n");
		break;
	case R600_IT_SURFACE_BASE_UPDATE:
		//DRM_INFO("R600_IT_SURFACE_BASE_UPDATE\n");
		if (((dev_priv->flags & RADEON_FAMILY_MASK) >= CHIP_RV770) ||
		    ((dev_priv->flags & RADEON_FAMILY_MASK) == CHIP_R600))
			ret = -EINVAL;
		if (num_dw != 2)
			ret = -EINVAL;
		if (ret)
			DRM_ERROR("bad SURFACE_BASE_UPDATE\n");
		break;
	case RADEON_CP_NOP:
		//DRM_INFO("NOP: %d\n", ib_chunk->kdata[offset_dw + 1]);
		break;
	default:
		DRM_ERROR("invalid packet 3 0x%08x\n", 0xff00);
		ret = -EINVAL;
		break;
	}

	*offset_dw_p += incr;
	return ret;
}

static int r600_cs_parse(struct drm_radeon_cs_parser *parser)
{
	volatile int rb;
	struct drm_radeon_kernel_chunk *ib_chunk;
	/* scan the packet for various things */
	int count_dw = 0, size_dw;
	int ret = 0;

	ib_chunk = &parser->chunks[parser->ib_index];
	size_dw = ib_chunk->length_dw;

	while (count_dw < size_dw && ret == 0) {
		int hdr = ib_chunk->kdata[count_dw];
		int num_dw = (hdr & RADEON_CP_PACKET_COUNT_MASK) >> 16;

		switch (hdr & RADEON_CP_PACKET_MASK) {
		case RADEON_CP_PACKET0:
			ret = r600_cs_packet0(parser, &count_dw);
			break;
		case RADEON_CP_PACKET1:
			ret = -EINVAL;
			break;
		case RADEON_CP_PACKET2:
			DRM_DEBUG("Packet 2\n");
			num_dw += 1;
			break;
		case RADEON_CP_PACKET3:
			ret = r600_cs_packet3(parser, &count_dw);
			break;
		}

		count_dw += num_dw;
	}

	if (ret)
		return ret;


	/* copy the packet into the IB */
	memcpy(parser->ib, ib_chunk->kdata, ib_chunk->length_dw * sizeof(uint32_t));

	/* read back last byte to flush WC buffers */
	rb = readl(((vm_offset_t)parser->ib + (ib_chunk->length_dw-1) * sizeof(uint32_t)));

	return 0;
}

static uint32_t radeon_cs_id_get(struct drm_radeon_private *radeon)
{
	/* FIXME: protect with a spinlock */
	/* FIXME: check if wrap affect last reported wrap & sequence */
	radeon->cs.id_scnt = (radeon->cs.id_scnt + 1) & 0x00FFFFFF;
	if (!radeon->cs.id_scnt) {
		/* increment wrap counter */
		radeon->cs.id_wcnt += 0x01000000;
		/* valid sequence counter start at 1 */
		radeon->cs.id_scnt = 1;
	}
	return (radeon->cs.id_scnt | radeon->cs.id_wcnt);
}

static void r600_cs_id_emit(struct drm_radeon_cs_parser *parser, uint32_t *id)
{
	drm_radeon_private_t *dev_priv = parser->dev->dev_private;
	RING_LOCALS;

	//dev_priv->irq_emitted = radeon_update_breadcrumb(parser->dev);

	*id = radeon_cs_id_get(dev_priv);

	/* SCRATCH 2 */
	BEGIN_RING(3);
	R600_CLEAR_AGE(*id);
	ADVANCE_RING();
	COMMIT_RING();
}

static uint32_t r600_cs_id_last_get(struct drm_device *dev)
{
	//drm_radeon_private_t *dev_priv = dev->dev_private;

	//return GET_R600_SCRATCH(dev_priv, 2);
	return 0;
}

static int r600_ib_get(struct drm_radeon_cs_parser *parser)
{
	struct drm_device *dev = parser->dev;
	drm_radeon_private_t *dev_priv = dev->dev_private;
	struct drm_buf *buf;

	buf = radeon_freelist_get(dev);
	if (!buf) {
		dev_priv->cs_buf = NULL;
		return -EBUSY;
	}
	buf->file_priv = parser->file_priv;
	dev_priv->cs_buf = buf;
	parser->ib = (void *)((vm_offset_t)dev->agp_buffer_map->handle +
	    buf->offset);

	return 0;
}

static void r600_ib_free(struct drm_radeon_cs_parser *parser, int error)
{
	struct drm_device *dev = parser->dev;
	drm_radeon_private_t *dev_priv = dev->dev_private;
	struct drm_buf *buf = dev_priv->cs_buf;

	if (buf) {
		if (!error)
			r600_cp_dispatch_indirect(dev, buf, 0,
						  parser->chunks[parser->ib_index].length_dw * sizeof(uint32_t));
		radeon_cp_discard_buffer(dev, buf);
		COMMIT_RING();
	}
}

int r600_cs_init(struct drm_device *dev)
{
	drm_radeon_private_t *dev_priv = dev->dev_private;

	dev_priv->cs.ib_get = r600_ib_get;
	dev_priv->cs.ib_free = r600_ib_free;
	dev_priv->cs.id_emit = r600_cs_id_emit;
	dev_priv->cs.id_last_get = r600_cs_id_last_get;
	dev_priv->cs.parse = r600_cs_parse;
	dev_priv->cs.relocate = r600_nomm_relocate;
	return 0;
}
