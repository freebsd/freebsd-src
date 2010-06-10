///////////////////////////////////////////////////////////////////////////////
//
/// \file       chunk_size.c
/// \brief      Finds out the minimal reasonable chunk size for a filter chain
//
//  Author:     Lasse Collin
//
//  This file has been put into the public domain.
//  You can do whatever you want with this file.
//
///////////////////////////////////////////////////////////////////////////////

#include "common.h"


/**
 * \brief       Finds out the minimal reasonable chunk size for a filter chain
 *
 * This function helps determining the Uncompressed Sizes of the Blocks when
 * doing multi-threaded encoding.
 *
 * When compressing a large file on a system having multiple CPUs or CPU
 * cores, the file can be split into smaller chunks, that are compressed
 * independently into separate Blocks in the same .lzma Stream.
 *
 * \return      Minimum reasonable Uncompressed Size of a Block. The
 *              recommended minimum Uncompressed Size is between this value
 *              and the value times two.

 Zero if the Uncompressed Sizes of Blocks don't matter
 */
extern LZMA_API(size_t)
lzma_chunk_size(const lzma_options_filter *filters)
{
	while (filters->id != LZMA_VLI_UNKNOWN) {
		switch (filters->id) {
		// TODO LZMA_FILTER_SPARSE

		case LZMA_FILTER_COPY:
		case LZMA_FILTER_SUBBLOCK:
		case LZMA_FILTER_X86:
		case LZMA_FILTER_POWERPC:
		case LZMA_FILTER_IA64:
		case LZMA_FILTER_ARM:
		case LZMA_FILTER_ARMTHUMB:
		case LZMA_FILTER_SPARC:
			// These are very fast, thus there is no point in
			// splitting the data into smaller blocks.
			break;

		case LZMA_FILTER_LZMA1:
			// The block sizes of the possible next filters in
			// the chain are irrelevant after the LZMA filter.
			return ((lzma_options_lzma *)(filters->options))
					->dictionary_size;

		default:
			// Unknown filters
			return 0;
		}

		++filters;
	}

	// Indicate that splitting would be useless.
	return SIZE_MAX;
}
