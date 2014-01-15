/*-
 * Copyright (c) 2009-2013 Kai Wang
 * All rights reserved.
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

#include "ld.h"
#include "ld_arch.h"
#include "ld_ehframe.h"
#include "ld_input.h"
#include "ld_output.h"
#include "ld_reloc.h"
#include "ld_utils.h"

ELFTC_VCSID("$Id: ld_ehframe.c 2964 2013-09-10 02:46:06Z kaiwang27 $");

struct ld_ehframe_cie {
	uint64_t cie_off;	/* offset in section */
	uint64_t cie_off_orig;	/* orignial offset (before optimze) */
	uint64_t cie_size;	/* CIE size (include length field) */
	uint8_t *cie_content;	/* CIE content */
	uint8_t cie_fde_encode; /* FDE PC start/range encode. */
	struct ld_ehframe_cie *cie_dup; /* duplicate entry */
	STAILQ_ENTRY(ld_ehframe_cie) cie_next;
};

STAILQ_HEAD(ld_ehframe_cie_head, ld_ehframe_cie);

struct ld_ehframe_fde {
	struct ld_ehframe_cie *fde_cie; /* associated CIE */
	uint64_t fde_off;	/* offset in section */
	uint64_t fde_off_pcbegin; /* section offset of "PC Begin" field */
	int32_t fde_pcrel;	/* relative offset to "PC Begin" */
	int32_t fde_datarel;	/* relative offset to FDE entry */
	STAILQ_ENTRY(ld_ehframe_fde) fde_next;
};

STAILQ_HEAD(ld_ehframe_fde_head, ld_ehframe_fde);

static int64_t _decode_sleb128(uint8_t **dp);
static uint64_t _decode_uleb128(uint8_t **dp);
static void _process_ehframe_section(struct ld *ld, struct ld_output *lo,
    struct ld_input_section *is);
static int _read_encoded(struct ld *ld, struct ld_output *lo, uint64_t *val,
    uint8_t *data, uint8_t encode, uint64_t pc);
static int _cmp_fde(struct ld_ehframe_fde *a, struct ld_ehframe_fde *b);

void
ld_ehframe_adjust(struct ld *ld, struct ld_input_section *is)
{
	struct ld_output *lo;
	uint8_t *p, *d, *end, *s;
	uint64_t length, length_size, remain, adjust;
	uint32_t cie_id;

	lo = ld->ld_output;
	assert(lo != NULL);

	/*
	 * If the .eh_frame section is unchanged, we don't need to
	 * do much.
	 */
	assert(is->is_ehframe != NULL);
	if (is->is_shrink == 0) {
		is->is_ehframe = NULL;
		return;
	}

	/*
	 * Otherwise the section is shrinked becase some FDE's are
	 * discarded. We copy the section content to a buffer while
	 * skipping those discarded FDE's.
	 */

	if ((is->is_ibuf = malloc(is->is_size - is->is_shrink)) == NULL)
		ld_fatal_std(ld, "malloc");
	d = is->is_ibuf;
	end = d + is->is_size - is->is_shrink;
	p = is->is_ehframe;
	adjust = 0;
	remain = is->is_size;
	while (remain > 0) {

		s = p;

		/* Read CIE/FDE length field. */
		READ_32(p, length);
		p += 4;
		if (length == 0xffffffff) {
			READ_64(p, length);
			p += 8;
			length_size = 8;
		} else
			length_size = 4;

		/* Check for terminator */
		if (length == 0) {
			memset(d, 0, 4);
			d += 4;
			break;
		}

		/* Read CIE ID/Pointer field. */
		READ_32(p, cie_id);

		/* Clear adjustment if CIE is found. */
		if (cie_id == 0)
			adjust = 0;

		/* Check for our special mark. */
		if (cie_id != 0xFFFFFFFF) {
			if (cie_id != 0) {
				/* Adjust FDE pointer. */
				assert(cie_id > adjust);
				cie_id -= adjust;
				WRITE_32(p, cie_id);
			}
			memcpy(d, s, length + length_size);
			d += length + length_size;
		} else {
			/* Discard FDE and increate adjustment. */
			adjust += length + length_size;
		}

		/* Next entry. */
		p += length;
		remain -= length + length_size;
	}

	is->is_size -= is->is_shrink;
	is->is_shrink = 0;
	assert(d == end);
	free(is->is_ehframe);
	is->is_ehframe = NULL;
}

void
ld_ehframe_scan(struct ld *ld)
{
	struct ld_output *lo;
	struct ld_output_section *os;
	struct ld_output_element *oe;
	struct ld_input_section *is;
	struct ld_input_section_head *islist;
	uint64_t ehframe_off;
	char ehframe_name[] = ".eh_frame";

	lo = ld->ld_output;
	assert(lo != NULL);

	/*
	 * Search for .eh_frame output section. Nothing needs to be done
	 * if .eh_frame section not exist or is empty.
	 */
	HASH_FIND_STR(lo->lo_ostbl, ehframe_name, os);
	if (os == NULL || os->os_empty)
		return;

	if ((ld->ld_cie = malloc(sizeof(*ld->ld_cie))) == NULL)
		ld_fatal_std(ld, "malloc");
	STAILQ_INIT(ld->ld_cie);

	/*
	 * Remove duplicate CIE from each input .eh_frame section.
	 */
	ehframe_off = 0;
	STAILQ_FOREACH(oe, &os->os_e, oe_next) {
		/*
		 * XXX We currently do not support .eh_frame section which
		 * contains elements other than OET_INPUT_SECTION_LIST.
		 */
		if (oe->oe_type != OET_INPUT_SECTION_LIST)
			continue;

		islist = oe->oe_islist;
		STAILQ_FOREACH(is, islist, is_next) {
			/*
			 * Process each input .eh_frame section and search
			 * for duplicate CIE's. The input section relative
			 * offset in the output section is resync'ed since
			 * the input section might be shrinked.
			 */
			is->is_reloff = ehframe_off;
			_process_ehframe_section(ld, lo, is);
			ehframe_off += is->is_size;
		}
	}

	/* Calculate the size of .eh_frame_hdr section. */
	if (ld->ld_ehframe_hdr) {
		is = ld_input_find_internal_section(ld, ".eh_frame_hdr");
		assert(is != NULL);
		if (lo->lo_fde_num > 0)
			is->is_size += 4 + lo->lo_fde_num * 8;
	}
}

void
ld_ehframe_create_hdr(struct ld *ld)
{
	struct ld_input_section *is;

	is = ld_input_add_internal_section(ld, ".eh_frame_hdr");
	is->is_type = SHT_PROGBITS;
	is->is_size = 8;	/* initial size */
	is->is_align = 4;
	is->is_entsize = 0;
}

void
ld_ehframe_finalize_hdr(struct ld *ld)
{
	struct ld_input_section *is, *hdr_is;
	struct ld_input_section_head *islist;
	struct ld_output *lo;
	struct ld_output_section *os, *hdr_os;
	struct ld_output_element *oe;
	struct ld_ehframe_fde *fde, *_fde;
	char ehframe_name[] = ".eh_frame";
	uint64_t pcbegin;
	int32_t pcrel;
	uint8_t *p, *end;

	lo = ld->ld_output;
	assert(lo != NULL);

	hdr_is = ld_input_find_internal_section(ld, ".eh_frame_hdr");
	assert(hdr_is != NULL);
	hdr_os = hdr_is->is_output;
	lo->lo_ehframe_hdr = hdr_os;

	if (hdr_is->is_discard || hdr_os == NULL)
		return;

	p = hdr_is->is_ibuf;
	end = p + hdr_is->is_size;

	/* Find .eh_frame output section. */
	HASH_FIND_STR(lo->lo_ostbl, ehframe_name, os);
	assert(os != NULL);

	/* .eh_frame_hdr version */
	*p++ = 1;

	/*
	 * eh_frame_ptr_enc: encoding format for eh_frame_ptr field.
	 * Usually a signed 4-byte PC relateive offset is used here.
	 */
	*p++ = DW_EH_PE_pcrel | DW_EH_PE_sdata4;

	/*
	 * fde_count_enc: encoding format for fde_count field. Unsigned
	 * 4 byte encoding should be used here. Note that If the binary
	 * search table is not present, DW_EH_PE_omit should be used
	 * instead.
	 */
	*p++ = lo->lo_fde_num == 0 ? DW_EH_PE_omit : DW_EH_PE_udata4;

	/*
	 * table_enc: encoding format for the binary search table entry.
	 * Signed 4 byte table relative offset is used here. Note that
	 * if the binary search table is not present, DW_EH_PE_omit should
	 * be used instaed.
	 */
	*p++ = lo->lo_fde_num == 0 ? DW_EH_PE_omit :
	    (DW_EH_PE_datarel | DW_EH_PE_sdata4);

	/* Write 4 byte PC relative offset to the .eh_frame section. */
	pcrel = os->os_addr - hdr_os->os_addr - 4;
	WRITE_32(p, pcrel);
	p += 4;

	/* Write the total number of FDE's. */
	WRITE_32(p, lo->lo_fde_num);
	p += 4;

	/* Allocate global FDE list. */
	if (ld->ld_fde == NULL) {
		if ((ld->ld_fde = calloc(1, sizeof(ld->ld_fde))) == NULL)
			ld_fatal_std(ld, "calloc");
		STAILQ_INIT(ld->ld_fde);
	}

	/* Link together the FDE's from each input object. */
	STAILQ_FOREACH(oe, &os->os_e, oe_next) {
		if (oe->oe_type != OET_INPUT_SECTION_LIST)
			continue;

		islist = oe->oe_islist;
		STAILQ_FOREACH(is, islist, is_next) {
			if (is->is_fde == NULL || STAILQ_EMPTY(is->is_fde))
				continue;
			STAILQ_FOREACH_SAFE(fde, is->is_fde, fde_next, _fde) {
				(void) _read_encoded(ld, lo, &pcbegin,
				    (uint8_t *) is->is_ibuf +
				    fde->fde_off_pcbegin,
				    fde->fde_cie->cie_fde_encode, os->os_addr);
				fde->fde_pcrel = pcbegin - hdr_os->os_addr;
				fde->fde_datarel = os->os_addr +
				    is->is_reloff + fde->fde_off -
				    hdr_os->os_addr;
				STAILQ_REMOVE(is->is_fde, fde, ld_ehframe_fde,
				    fde_next);
				STAILQ_INSERT_TAIL(ld->ld_fde, fde, fde_next);
			}
		}
	}

	/* Sort the binary search table in an increasing order by pcrel. */
	STAILQ_SORT(ld->ld_fde, ld_ehframe_fde, fde_next, _cmp_fde);

	/* Write binary search table. */
	STAILQ_FOREACH(fde, ld->ld_fde, fde_next) {
		WRITE_32(p, fde->fde_pcrel);
		p += 4;
		WRITE_32(p, fde->fde_datarel);
		p += 4;
	}

	assert(p == end);
}

static int
_cmp_fde(struct ld_ehframe_fde *a, struct ld_ehframe_fde *b)
{

	if (a->fde_pcrel < b->fde_pcrel)
		return (-1);
	else if (a->fde_pcrel == b->fde_pcrel)
		return (0);
	else
		return (1);
}

static void
_parse_cie_augment(struct ld *ld, struct ld_ehframe_cie *cie, uint8_t *aug_p,
    uint8_t *augdata_p, uint64_t auglen)
{
	uint64_t dummy;
	uint8_t encode, *augdata_end;
	int len;

	assert(aug_p != NULL && *aug_p == 'z');

	augdata_end = augdata_p + auglen;

	/*
	 * Here we're only interested in the presence of augment 'R'
	 * and associated CIE augment data, which describes the
	 * encoding scheme of FDE PC begin and range.
	 */
	aug_p++;
	while (*aug_p != '\0') {
		switch (*aug_p) {
		case 'L':
			/* Skip one augment in augment data. */
			augdata_p++;
			break;
		case 'P':
			/* Skip two augments in augment data. */
			encode = *augdata_p++;
			len = _read_encoded(ld, ld->ld_output, &dummy,
			    augdata_p, encode, 0);
			augdata_p += len;
			break;
		case 'R':
			cie->cie_fde_encode = *augdata_p++;
			break;
		default:
			ld_warn(ld, "unsupported eh_frame augmentation `%c'",
			    *aug_p);
			return;
		}
		aug_p++;
	}

	if (augdata_p > augdata_end)
		ld_warn(ld, "invalid eh_frame augmentation");
}

static void
_process_ehframe_section(struct ld *ld, struct ld_output *lo,
    struct ld_input_section *is)
{
	struct ld_input *li;
	struct ld_output_section *os;
	struct ld_ehframe_cie *cie, *_cie;
	struct ld_ehframe_cie_head cie_h;
	struct ld_ehframe_fde *fde;
	struct ld_reloc_entry *lre, *_lre;
	uint64_t length, es, off, off_orig, remain, shrink, auglen;
	uint32_t cie_id, cie_pointer, length_size;
	uint8_t *p, *et, cie_version, *augment;

	li = is->is_input;
	os = is->is_output;

	STAILQ_INIT(&cie_h);

	/*
	 * .eh_frame section content should already be preloaded
	 * in is->is_ibuf.
	 */
	assert(is->is_ibuf != NULL && is->is_size > 0);

	shrink = 0;
	p = is->is_ibuf;
	off = off_orig = 0;
	remain = is->is_size;
	while (remain > 0) {

		et = p;
		off = et - (uint8_t *) is->is_ibuf;

		/* Read CIE/FDE length field. */
		READ_32(p, length);
		p += 4;
		es = length + 4;
		if (length == 0xffffffff) {
			READ_64(p, length);
			p += 8;
			es += 8;
			length_size = 8;
		} else
			length_size = 4;

		/* Check for terminator */
		if (length == 0)
			break;

		/* Read CIE ID/Pointer field. */
		READ_32(p, cie_id);
		p += 4;

		if (cie_id == 0) {

			/* This is a Common Information Entry (CIE). */
			if ((cie = calloc(1, sizeof(*cie))) == NULL)
				ld_fatal_std(ld, "calloc");
			cie->cie_off = off;
			cie->cie_off_orig = off_orig;
			cie->cie_size = es;
			cie->cie_content = et;
			cie->cie_dup = NULL;
			STAILQ_INSERT_TAIL(&cie_h, cie, cie_next);

			/*
			 * This is a Common Information Entry (CIE). Search
			 * in the CIE list see if we can found a duplicate
			 * entry.
			 */
			STAILQ_FOREACH(_cie, ld->ld_cie, cie_next) {
				if (memcmp(et, _cie->cie_content, es) == 0) {
					cie->cie_dup = _cie;
					break;
				}
			}
			if (_cie != NULL) {
				/*
				 * We found a duplicate entry. It should be
				 * removed and the subsequent FDE's should
				 * point to the previously stored CIE.
				 */
				memmove(et, et + es, remain - es);
				shrink += es;
				p = et;
			} else {
				/*
				 * This is a new CIE entry which should be
				 * kept. Read its augmentation which is
				 * used to parse assoicated FDE's later.
				 */
				cie_version = *p++;
				if (cie_version != 1) {
					ld_warn(ld, "unsupported CIE version");
					goto ignore_cie;
				}
				augment = p;
				if (*p != 'z') {
					ld_warn(ld, "unsupported CIE "
					    "augmentation");
					goto ignore_cie;
				}
				while (*p++ != '\0')
					;

				/* Skip EH Data field. */
				if (strstr((char *)augment, "eh") != NULL)
					p += lo->lo_ec == ELFCLASS32 ? 4 : 8;

				/* Skip CAF and DAF. */
				(void) _decode_uleb128(&p);
				(void) _decode_sleb128(&p);

				/* Skip RA. */
				p++;

				/* Parse augmentation data. */
				auglen = _decode_uleb128(&p);
				_parse_cie_augment(ld, cie, augment, p,
				    auglen);

			ignore_cie:
				p = et + es;
			}

		} else {

			/*
			 * This is a Frame Description Entry (FDE). First
			 * Search for the associated CIE.
			 */
			STAILQ_FOREACH(cie, &cie_h, cie_next) {
				if (cie->cie_off_orig ==
				    off_orig + length_size - cie_id)
					break;
			}

			/*
			 * If we can not found the associated CIE, this FDE
			 * is invalid and we ignore it.
			 */
			if (cie == NULL) {
				ld_warn(ld, "%s(%s): malformed FDE",
				    li->li_name, is->is_name);
				p = et + es;
				goto next_entry;
			}

			/* Allocate new FDE entry. */
			if ((fde = calloc(1, sizeof(*fde))) == NULL)
				ld_fatal_std(ld, "calloc");
			fde->fde_off = off;
			fde->fde_off_pcbegin = off + length_size + 4;
			if (is->is_fde == NULL) {
				is->is_fde = calloc(1, sizeof(*is->is_fde));
				if (is->is_fde == NULL)
					ld_fatal_std(ld, "calloc");
				STAILQ_INIT(is->is_fde);
			}
			STAILQ_INSERT_TAIL(is->is_fde, fde, fde_next);
			lo->lo_fde_num++;

			/* Calculate the new CIE pointer value. */
			if (cie->cie_dup != NULL) {
				cie_pointer = off + length_size +
				    is->is_reloff - cie->cie_dup->cie_off;
				fde->fde_cie = cie->cie_dup;
			} else {
				cie_pointer = off + length_size - cie->cie_off;
				fde->fde_cie = cie;
			}

			/* Rewrite CIE pointer value. */
			if (cie_id != cie_pointer) {
				p -= 4;
				WRITE_32(p, cie_pointer);
			}

			p = et + es;
		}

	next_entry:
		off_orig += es;
		remain -= es;
	}

	/*
	 * Update the relocation entry offsets since we shrinked the
	 * section content.
	 */
	if (shrink > 0 && is->is_ris != NULL && is->is_ris->is_reloc != NULL) {
		STAILQ_FOREACH_SAFE(lre, is->is_ris->is_reloc, lre_next,
		    _lre) {
			STAILQ_FOREACH(cie, &cie_h, cie_next) {
				if (cie->cie_off_orig > lre->lre_offset)
					break;
				if (cie->cie_dup == NULL)
					continue;

				/*
				 * Remove relocations for the duplicated CIE
				 * entries.
				 */
				if (lre->lre_offset <
				    cie->cie_off_orig + cie->cie_size) {
					STAILQ_REMOVE(is->is_ris->is_reloc,
					    lre, ld_reloc_entry, lre_next);
					is->is_ris->is_num_reloc--;
					is->is_ris->is_size -= 
					    ld->ld_arch->reloc_entsize;
					if (os->os_r != NULL)
						os->os_r->os_size -=
						    ld->ld_arch->reloc_entsize;
					break;
				}

				/* Adjust relocation offset for FDE entries. */
				lre->lre_offset -= cie->cie_size;
			}
		}
	}

	/* Insert newly found non-duplicate CIE's to the global CIE list. */
	STAILQ_FOREACH_SAFE(cie, &cie_h, cie_next, _cie) {
		STAILQ_REMOVE(&cie_h, cie, ld_ehframe_cie, cie_next);
		if (cie->cie_dup == NULL) {
			cie->cie_off += is->is_reloff;
			STAILQ_INSERT_TAIL(ld->ld_cie, cie, cie_next);
		}
	}

	/* Update the size of input .eh_frame section */
	is->is_size -= shrink;
}

static int
_read_encoded(struct ld *ld, struct ld_output *lo, uint64_t *val,
    uint8_t *data, uint8_t encode, uint64_t pc)
{
	int16_t s16;
	int32_t s32;
	uint8_t application, *begin;
	int len;

	if (encode == DW_EH_PE_omit)
		return (0);

	application = encode & 0xf0;
	encode &= 0x0f;

	len = 0;
	begin = data;

	switch (encode) {
	case DW_EH_PE_absptr:
		if (lo->lo_ec == ELFCLASS32)
			READ_32(data, *val);
		else
			READ_64(data, *val);
		break;
	case DW_EH_PE_uleb128:
		*val = _decode_uleb128(&data);
		len = data - begin;
		break;
	case DW_EH_PE_udata2:
		READ_16(data, *val);
		len = 2;
		break;
	case DW_EH_PE_udata4:
		READ_32(data, *val);
		len = 4;
		break;
	case DW_EH_PE_udata8:
		READ_64(data, *val);
		len = 8;
		break;
	case DW_EH_PE_sleb128:
		*val = _decode_sleb128(&data);
		len = data - begin;
		break;
	case DW_EH_PE_sdata2:
		READ_16(data, s16);
		*val = s16;
		len = 2;
		break;
	case DW_EH_PE_sdata4:
		READ_32(data, s32);
		*val = s32;
		len = 4;
		break;
	case DW_EH_PE_sdata8:
		READ_64(data, *val);
		len = 8;
		break;
	default:
		ld_warn(ld, "unsupported eh_frame encoding");
		break;
	}

	if (application == DW_EH_PE_pcrel) {
		/*
		 * Value is relative to .eh_frame section virtual addr.
		 */
		switch (encode) {
		case DW_EH_PE_uleb128:
		case DW_EH_PE_udata2:
		case DW_EH_PE_udata4:
		case DW_EH_PE_udata8:
			*val += pc;
			break;
		case DW_EH_PE_sleb128:
		case DW_EH_PE_sdata2:
		case DW_EH_PE_sdata4:
		case DW_EH_PE_sdata8:
			*val = pc + (int64_t) *val;
			break;
		default:
			/* DW_EH_PE_absptr is absolute value. */
			break;
		}
	}

	/* XXX Applications other than DW_EH_PE_pcrel are not handled. */

	return (len);
}

static int64_t
_decode_sleb128(uint8_t **dp)
{
	int64_t ret = 0;
	uint8_t b;
	int shift = 0;

	uint8_t *src = *dp;

	do {
		b = *src++;
		ret |= ((b & 0x7f) << shift);
		shift += 7;
	} while ((b & 0x80) != 0);

	if (shift < 32 && (b & 0x40) != 0)
		ret |= (-1 << shift);

	*dp = src;

	return (ret);
}

static uint64_t
_decode_uleb128(uint8_t **dp)
{
	uint64_t ret = 0;
	uint8_t b;
	int shift = 0;

	uint8_t *src = *dp;

	do {
		b = *src++;
		ret |= ((b & 0x7f) << shift);
		shift += 7;
	} while ((b & 0x80) != 0);

	*dp = src;

	return (ret);
}
