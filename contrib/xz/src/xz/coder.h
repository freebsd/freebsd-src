// SPDX-License-Identifier: 0BSD

///////////////////////////////////////////////////////////////////////////////
//
/// \file       coder.h
/// \brief      Compresses or uncompresses a file
//
//  Authors:    Lasse Collin
//              Jia Tan
//
///////////////////////////////////////////////////////////////////////////////

enum operation_mode {
	MODE_COMPRESS,
	MODE_DECOMPRESS,
	MODE_TEST,
	MODE_LIST,
};


// NOTE: The order of these is significant in suffix.c.
enum format_type {
	FORMAT_AUTO,
	FORMAT_XZ,
	FORMAT_LZMA,
#ifdef HAVE_LZIP_DECODER
	FORMAT_LZIP,
#endif
	FORMAT_RAW,
};


/// Simple struct to track Block metadata specified through the
/// --block-list option.
typedef struct {
	/// Uncompressed size of the Block
	uint64_t size;

	/// Index into the filters[] representing the filter chain to use
	/// for this Block.
	uint32_t filters_index;
} block_list_entry;


/// Operation mode of the command line tool. This is set in args.c and read
/// in several files.
extern enum operation_mode opt_mode;

/// File format to use when encoding or what format(s) to accept when
/// decoding. This is a global because it's needed also in suffix.c.
/// This is set in args.c.
extern enum format_type opt_format;

/// If true, the compression settings are automatically adjusted down if
/// they exceed the memory usage limit.
extern bool opt_auto_adjust;

/// If true, stop after decoding the first stream.
extern bool opt_single_stream;

/// If non-zero, start a new .xz Block after every opt_block_size bytes
/// of input. This has an effect only when compressing to the .xz format.
extern uint64_t opt_block_size;

/// List of block size and filter chain pointer pairs.
extern block_list_entry *opt_block_list;

/// Set the integrity check type used when compressing
extern void coder_set_check(lzma_check check);

/// Set preset number
extern void coder_set_preset(uint32_t new_preset);

/// Enable extreme mode
extern void coder_set_extreme(void);

/// Add a filter to the custom filter chain
extern void coder_add_filter(lzma_vli id, void *options);

/// Set and partially validate compression settings. This can also be used
/// in decompression or test mode with the raw format.
extern void coder_set_compression_settings(void);

/// Compress or decompress the given file
extern void coder_run(const char *filename);

#ifndef NDEBUG
/// Free the memory allocated for the coder and kill the worker threads.
extern void coder_free(void);
#endif

/// Create filter chain from string
extern void coder_add_filters_from_str(const char *filter_str);

/// Add or overwrite a filter that can be used by the block-list.
extern void coder_add_block_filters(const char *str, size_t slot);
