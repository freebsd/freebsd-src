///////////////////////////////////////////////////////////////////////////////
//
/// \file       list.c
/// \brief      Listing information about .xz files
//
//  Author:     Lasse Collin
//
//  This file has been put into the public domain.
//  You can do whatever you want with this file.
//
///////////////////////////////////////////////////////////////////////////////

#include "private.h"
#include "tuklib_integer.h"


/// Totals that are displayed if there was more than one file.
/// The "files" counter is also used in print_info_adv() to show
/// the file number.
static struct {
	uint64_t files;
	uint64_t streams;
	uint64_t blocks;
	uint64_t compressed_size;
	uint64_t uncompressed_size;
	uint32_t checks;
} totals = { 0, 0, 0, 0, 0, 0 };


/// \brief      Parse the Index(es) from the given .xz file
///
/// \param      idx     If decoding is successful, *idx will be set to point
///                     to lzma_index containing the decoded information.
///                     On error, *idx is not modified.
/// \param      pair    Input file
///
/// \return     On success, false is returned. On error, true is returned.
///
// TODO: This function is pretty big. liblzma should have a function that
// takes a callback function to parse the Index(es) from a .xz file to make
// it easy for applications.
static bool
parse_indexes(lzma_index **idx, file_pair *pair)
{
	if (pair->src_st.st_size <= 0) {
		message_error(_("%s: File is empty"), pair->src_name);
		return true;
	}

	if (pair->src_st.st_size < 2 * LZMA_STREAM_HEADER_SIZE) {
		message_error(_("%s: Too small to be a valid .xz file"),
				pair->src_name);
		return true;
	}

	io_buf buf;
	lzma_stream_flags header_flags;
	lzma_stream_flags footer_flags;
	lzma_ret ret;

	// lzma_stream for the Index decoder
	lzma_stream strm = LZMA_STREAM_INIT;

	// All Indexes decoded so far
	lzma_index *combined_index = NULL;

	// The Index currently being decoded
	lzma_index *this_index = NULL;

	// Current position in the file. We parse the file backwards so
	// initialize it to point to the end of the file.
	off_t pos = pair->src_st.st_size;

	// Each loop iteration decodes one Index.
	do {
		// Check that there is enough data left to contain at least
		// the Stream Header and Stream Footer. This check cannot
		// fail in the first pass of this loop.
		if (pos < 2 * LZMA_STREAM_HEADER_SIZE) {
			message_error("%s: %s", pair->src_name,
					message_strm(LZMA_DATA_ERROR));
			goto error;
		}

		pos -= LZMA_STREAM_HEADER_SIZE;
		lzma_vli stream_padding = 0;

		// Locate the Stream Footer. There may be Stream Padding which
		// we must skip when reading backwards.
		while (true) {
			if (pos < LZMA_STREAM_HEADER_SIZE) {
				message_error("%s: %s", pair->src_name,
						message_strm(
							LZMA_DATA_ERROR));
				goto error;
			}

			if (io_pread(pair, &buf,
					LZMA_STREAM_HEADER_SIZE, pos))
				goto error;

			// Stream Padding is always a multiple of four bytes.
			int i = 2;
			if (buf.u32[i] != 0)
				break;

			// To avoid calling io_pread() for every four bytes
			// of Stream Padding, take advantage that we read
			// 12 bytes (LZMA_STREAM_HEADER_SIZE) already and
			// check them too before calling io_pread() again.
			do {
				stream_padding += 4;
				pos -= 4;
				--i;
			} while (i >= 0 && buf.u32[i] == 0);
		}

		// Decode the Stream Footer.
		ret = lzma_stream_footer_decode(&footer_flags, buf.u8);
		if (ret != LZMA_OK) {
			message_error("%s: %s", pair->src_name,
					message_strm(ret));
			goto error;
		}

		// Check that the size of the Index field looks sane.
		lzma_vli index_size = footer_flags.backward_size;
		if ((lzma_vli)(pos) < index_size + LZMA_STREAM_HEADER_SIZE) {
			message_error("%s: %s", pair->src_name,
					message_strm(LZMA_DATA_ERROR));
			goto error;
		}

		// Set pos to the beginning of the Index.
		pos -= index_size;

		// See how much memory we can use for decoding this Index.
		uint64_t memlimit = hardware_memlimit_get();
		uint64_t memused = 0;
		if (combined_index != NULL) {
			memused = lzma_index_memused(combined_index);
			if (memused > memlimit)
				message_bug();

			memlimit -= memused;
		}

		// Decode the Index.
		ret = lzma_index_decoder(&strm, &this_index, memlimit);
		if (ret != LZMA_OK) {
			message_error("%s: %s", pair->src_name,
					message_strm(ret));
			goto error;
		}

		do {
			// Don't give the decoder more input than the
			// Index size.
			strm.avail_in = MIN(IO_BUFFER_SIZE, index_size);
			if (io_pread(pair, &buf, strm.avail_in, pos))
				goto error;

			pos += strm.avail_in;
			index_size -= strm.avail_in;

			strm.next_in = buf.u8;
			ret = lzma_code(&strm, LZMA_RUN);

		} while (ret == LZMA_OK);

		// If the decoding seems to be successful, check also that
		// the Index decoder consumed as much input as indicated
		// by the Backward Size field.
		if (ret == LZMA_STREAM_END)
			if (index_size != 0 || strm.avail_in != 0)
				ret = LZMA_DATA_ERROR;

		if (ret != LZMA_STREAM_END) {
			// LZMA_BUFFER_ERROR means that the Index decoder
			// would have liked more input than what the Index
			// size should be according to Stream Footer.
			// The message for LZMA_DATA_ERROR makes more
			// sense in that case.
			if (ret == LZMA_BUF_ERROR)
				ret = LZMA_DATA_ERROR;

			message_error("%s: %s", pair->src_name,
					message_strm(ret));

			// If the error was too low memory usage limit,
			// show also how much memory would have been needed.
			if (ret == LZMA_MEMLIMIT_ERROR) {
				uint64_t needed = lzma_memusage(&strm);
				if (UINT64_MAX - needed < memused)
					needed = UINT64_MAX;
				else
					needed += memused;

				message_mem_needed(V_ERROR, needed);
			}

			goto error;
		}

		// Decode the Stream Header and check that its Stream Flags
		// match the Stream Footer.
		pos -= footer_flags.backward_size + LZMA_STREAM_HEADER_SIZE;
		if ((lzma_vli)(pos) < lzma_index_total_size(this_index)) {
			message_error("%s: %s", pair->src_name,
					message_strm(LZMA_DATA_ERROR));
			goto error;
		}

		pos -= lzma_index_total_size(this_index);
		if (io_pread(pair, &buf, LZMA_STREAM_HEADER_SIZE, pos))
			goto error;

		ret = lzma_stream_header_decode(&header_flags, buf.u8);
		if (ret != LZMA_OK) {
			message_error("%s: %s", pair->src_name,
					message_strm(ret));
			goto error;
		}

		ret = lzma_stream_flags_compare(&header_flags, &footer_flags);
		if (ret != LZMA_OK) {
			message_error("%s: %s", pair->src_name,
					message_strm(ret));
			goto error;
		}

		// Store the decoded Stream Flags into this_index. This is
		// needed so that we can print which Check is used in each
		// Stream.
		ret = lzma_index_stream_flags(this_index, &footer_flags);
		if (ret != LZMA_OK)
			message_bug();

		// Store also the size of the Stream Padding field. It is
		// needed to show the offsets of the Streams correctly.
		ret = lzma_index_stream_padding(this_index, stream_padding);
		if (ret != LZMA_OK)
			message_bug();

		if (combined_index != NULL) {
			// Append the earlier decoded Indexes
			// after this_index.
			ret = lzma_index_cat(
					this_index, combined_index, NULL);
			if (ret != LZMA_OK) {
				message_error("%s: %s", pair->src_name,
						message_strm(ret));
				goto error;
			}
		}

		combined_index = this_index;
		this_index = NULL;

	} while (pos > 0);

	lzma_end(&strm);

	// All OK. Make combined_index available to the caller.
	*idx = combined_index;
	return false;

error:
	// Something went wrong, free the allocated memory.
	lzma_end(&strm);
	lzma_index_end(combined_index, NULL);
	lzma_index_end(this_index, NULL);
	return true;
}


/// \brief      Get the compression ratio
///
/// This has slightly different format than that is used by in message.c.
static const char *
get_ratio(uint64_t compressed_size, uint64_t uncompressed_size)
{
	if (uncompressed_size == 0)
		return "---";

	const double ratio = (double)(compressed_size)
			/ (double)(uncompressed_size);
	if (ratio > 9.999)
		return "---";

	static char buf[6];
	snprintf(buf, sizeof(buf), "%.3f", ratio);
	return buf;
}


static const char check_names[LZMA_CHECK_ID_MAX + 1][12] = {
	"None",
	"CRC32",
	"Unknown-2",
	"Unknown-3",
	"CRC64",
	"Unknown-5",
	"Unknown-6",
	"Unknown-7",
	"Unknown-8",
	"Unknown-9",
	"SHA-256",
	"Unknown-11",
	"Unknown-12",
	"Unknown-13",
	"Unknown-14",
	"Unknown-15",
};


/// \brief      Get a comma-separated list of Check names
///
/// \param      checks  Bit mask of Checks to print
/// \param      space_after_comma
///                     It's better to not use spaces in table-like listings,
///                     but in more verbose formats a space after a comma
///                     is good for readability.
static const char *
get_check_names(uint32_t checks, bool space_after_comma)
{
	assert(checks != 0);

	static char buf[sizeof(check_names)];
	char *pos = buf;
	size_t left = sizeof(buf);

	const char *sep = space_after_comma ? ", " : ",";
	bool comma = false;

	for (size_t i = 0; i <= LZMA_CHECK_ID_MAX; ++i) {
		if (checks & (UINT32_C(1) << i)) {
			my_snprintf(&pos, &left, "%s%s",
					comma ? sep : "", check_names[i]);
			comma = true;
		}
	}

	return buf;
}


/// \brief      Read the Check value from the .xz file and print it
///
/// Since this requires a seek, listing all Check values for all Blocks can
/// be slow.
///
/// \param      pair    Input file
/// \param      iter    Location of the Block whose Check value should
///                     be printed.
///
/// \return     False on success, true on I/O error.
static bool
print_check_value(file_pair *pair, const lzma_index_iter *iter)
{
	// Don't read anything from the file if there is no integrity Check.
	if (iter->stream.flags->check == LZMA_CHECK_NONE) {
		printf("---");
		return false;
	}

	// Locate and read the Check field.
	const uint32_t size = lzma_check_size(iter->stream.flags->check);
	const off_t offset = iter->block.compressed_file_offset
			+ iter->block.total_size - size;
	io_buf buf;
	if (io_pread(pair, &buf, size, offset))
		return true;

	// CRC32 and CRC64 are in little endian. Guess that all the future
	// 32-bit and 64-bit Check values are little endian too. It shouldn't
	// be a too big problem if this guess is wrong.
	if (size == 4) {
		printf("%08" PRIx32, conv32le(buf.u32[0]));
	} else if (size == 8) {
		printf("%016" PRIx64, conv64le(buf.u64[0]));
	} else {
		for (size_t i = 0; i < size; ++i)
			printf("%02x", buf.u8[i]);
	}

	return false;
}


static void
print_info_basic(const lzma_index *idx, file_pair *pair)
{
	static bool headings_displayed = false;
	if (!headings_displayed) {
		headings_displayed = true;
		// TRANSLATORS: These are column titles. From Strms (Streams)
		// to Ratio, the columns are right aligned. Check and Filename
		// are left aligned. If you need longer words, it's OK to
		// use two lines here. Test with xz --list.
		puts(_("Strms  Blocks   Compressed Uncompressed  Ratio  "
				"Check   Filename"));
	}

	printf("%5s %7s  %11s  %11s  %5s  %-7s %s\n",
			uint64_to_str(lzma_index_stream_count(idx), 0),
			uint64_to_str(lzma_index_block_count(idx), 1),
			uint64_to_nicestr(lzma_index_file_size(idx),
				NICESTR_B, NICESTR_TIB, false, 2),
			uint64_to_nicestr(lzma_index_uncompressed_size(idx),
				NICESTR_B, NICESTR_TIB, false, 3),
			get_ratio(lzma_index_file_size(idx),
				lzma_index_uncompressed_size(idx)),
			get_check_names(lzma_index_checks(idx), false),
			pair->src_name);

	return;
}


static void
print_adv_helper(uint64_t stream_count, uint64_t block_count,
		uint64_t compressed_size, uint64_t uncompressed_size,
		uint32_t checks)
{
	printf(_("  Stream count:       %s\n"),
			uint64_to_str(stream_count, 0));
	printf(_("  Block count:        %s\n"),
			uint64_to_str(block_count, 0));
	printf(_("  Compressed size:    %s\n"),
			uint64_to_nicestr(compressed_size,
				NICESTR_B, NICESTR_TIB, true, 0));
	printf(_("  Uncompressed size:  %s\n"),
			uint64_to_nicestr(uncompressed_size,
				NICESTR_B, NICESTR_TIB, true, 0));
	printf(_("  Ratio:              %s\n"),
			get_ratio(compressed_size, uncompressed_size));
	printf(_("  Check:              %s\n"),
			get_check_names(checks, true));
	return;
}


static void
print_info_adv(const lzma_index *idx, file_pair *pair)
{
	// Print the overall information.
	print_adv_helper(lzma_index_stream_count(idx),
			lzma_index_block_count(idx),
			lzma_index_file_size(idx),
			lzma_index_uncompressed_size(idx),
			lzma_index_checks(idx));

	// TODO: The rest of this function needs some work. Currently
	// the offsets are not printed, which could be useful even when
	// printed in a less accurate format. On the other hand, maybe
	// this should print the information with exact byte values,
	// or maybe there should be at least an option to do that.
	//
	// We could also display some other info. E.g. it could be useful
	// to quickly see how big is the biggest Block (uncompressed size)
	// and if all Blocks have Compressed Size and Uncompressed Size
	// fields present, which can be used e.g. for multithreaded
	// decompression.

	// Avoid printing Stream and Block lists when they wouldn't be useful.
	bool show_blocks = false;
	if (lzma_index_stream_count(idx) > 1) {
		puts(_("  Streams:"));
		puts(_("      Number      Blocks    Compressed   "
				"Uncompressed   Ratio   Check"));

		lzma_index_iter iter;
		lzma_index_iter_init(&iter, idx);
		while (!lzma_index_iter_next(&iter, LZMA_INDEX_ITER_STREAM)) {
			if (iter.stream.block_count > 1)
				show_blocks = true;

			printf("    %8s  %10s   %11s    %11s   %5s   %s\n",
				uint64_to_str(iter.stream.number, 0),
				uint64_to_str(iter.stream.block_count, 1),
				uint64_to_nicestr(
					iter.stream.compressed_size,
					NICESTR_B, NICESTR_TIB, false, 2),
				uint64_to_nicestr(
					iter.stream.uncompressed_size,
					NICESTR_B, NICESTR_TIB, false, 3),
				get_ratio(iter.stream.compressed_size,
					iter.stream.uncompressed_size),
				check_names[iter.stream.flags->check]);
		}
	}

	if (show_blocks || lzma_index_block_count(idx)
				> lzma_index_stream_count(idx)
			|| message_verbosity_get() >= V_DEBUG) {
		puts(_("  Blocks:"));
		// FIXME: Number in Stream/file, which one is better?
		puts(_("      Stream      Number    Compressed   "
				"Uncompressed   Ratio   Check"));

		lzma_index_iter iter;
		lzma_index_iter_init(&iter, idx);
		while (!lzma_index_iter_next(&iter, LZMA_INDEX_ITER_BLOCK)) {
			printf("    %8s  %10s   %11s    %11s   %5s   %-7s",
				uint64_to_str(iter.stream.number, 0),
				uint64_to_str(iter.block.number_in_stream, 1),
				uint64_to_nicestr(iter.block.total_size,
					NICESTR_B, NICESTR_TIB, false, 2),
				uint64_to_nicestr(
					iter.block.uncompressed_size,
					NICESTR_B, NICESTR_TIB, false, 3),
				get_ratio(iter.block.total_size,
					iter.block.uncompressed_size),
				check_names[iter.stream.flags->check]);

			if (message_verbosity_get() >= V_DEBUG)
				if (print_check_value(pair, &iter))
					return;

			putchar('\n');
		}
	}
}


static void
print_info_robot(const lzma_index *idx, file_pair *pair)
{
	printf("file\t%" PRIu64 "\t%" PRIu64 "\t%" PRIu64 "\t%" PRIu64
			"\t%s\t%s\t%s\n",
			lzma_index_stream_count(idx),
			lzma_index_block_count(idx),
			lzma_index_file_size(idx),
			lzma_index_uncompressed_size(idx),
			get_ratio(lzma_index_file_size(idx),
				lzma_index_uncompressed_size(idx)),
			get_check_names(lzma_index_checks(idx), false),
			pair->src_name);

	if (message_verbosity_get() >= V_VERBOSE) {
		lzma_index_iter iter;
		lzma_index_iter_init(&iter, idx);

		while (!lzma_index_iter_next(&iter, LZMA_INDEX_ITER_STREAM))
			printf("stream\t%" PRIu64 "\t%" PRIu64 "\t%" PRIu64
				"\t%" PRIu64 "\t%" PRIu64
				"\t%s\t%" PRIu64 "\t%s\n",
				iter.stream.number,
				iter.stream.compressed_offset,
				iter.stream.uncompressed_offset,
				iter.stream.compressed_size,
				iter.stream.uncompressed_size,
				get_ratio(iter.stream.compressed_size,
					iter.stream.uncompressed_size),
				iter.stream.padding,
				check_names[iter.stream.flags->check]);

		lzma_index_iter_rewind(&iter);
		while (!lzma_index_iter_next(&iter, LZMA_INDEX_ITER_BLOCK)) {
			printf("block\t%" PRIu64 "\t%" PRIu64 "\t%" PRIu64
					"\t%" PRIu64 "\t%" PRIu64
					"\t%" PRIu64 "\t%" PRIu64 "\t%s\t%s",
					iter.stream.number,
					iter.block.number_in_stream,
					iter.block.number_in_file,
					iter.block.compressed_file_offset,
					iter.block.uncompressed_file_offset,
					iter.block.total_size,
					iter.block.uncompressed_size,
					get_ratio(iter.block.total_size,
						iter.block.uncompressed_size),
					check_names[iter.stream.flags->check]);

			if (message_verbosity_get() >= V_DEBUG) {
				putchar('\t');
				if (print_check_value(pair, &iter))
					return;
			}

			putchar('\n');
		}
	}

	return;
}


static void
update_totals(const lzma_index *idx)
{
	// TODO: Integer overflow checks
	++totals.files;
	totals.streams += lzma_index_stream_count(idx);
	totals.blocks += lzma_index_block_count(idx);
	totals.compressed_size += lzma_index_file_size(idx);
	totals.uncompressed_size += lzma_index_uncompressed_size(idx);
	totals.checks |= lzma_index_checks(idx);
	return;
}


static void
print_totals_basic(void)
{
	// Print a separator line.
	char line[80];
	memset(line, '-', sizeof(line));
	line[sizeof(line) - 1] = '\0';
	puts(line);

	// Print the totals except the file count, which needs
	// special handling.
	printf("%5s %7s  %11s  %11s  %5s  %-7s ",
			uint64_to_str(totals.streams, 0),
			uint64_to_str(totals.blocks, 1),
			uint64_to_nicestr(totals.compressed_size,
				NICESTR_B, NICESTR_TIB, false, 2),
			uint64_to_nicestr(totals.uncompressed_size,
				NICESTR_B, NICESTR_TIB, false, 3),
			get_ratio(totals.compressed_size,
				totals.uncompressed_size),
			get_check_names(totals.checks, false));

	// Since we print totals only when there are at least two files,
	// the English message will always use "%s files". But some other
	// languages need different forms for different plurals so we
	// have to translate this string still.
	//
	// TRANSLATORS: This simply indicates the number of files shown
	// by --list even though the format string uses %s.
	printf(N_("%s file", "%s files\n",
			totals.files <= ULONG_MAX ? totals.files
				: (totals.files % 1000000) + 1000000),
			uint64_to_str(totals.files, 0));

	return;
}


static void
print_totals_adv(void)
{
	putchar('\n');
	puts(_("Totals:"));
	printf(_("  Number of files:    %s\n"),
			uint64_to_str(totals.files, 0));
	print_adv_helper(totals.streams, totals.blocks,
			totals.compressed_size, totals.uncompressed_size,
			totals.checks);

	return;
}


static void
print_totals_robot(void)
{
	printf("totals\t%" PRIu64 "\t%" PRIu64 "\t%" PRIu64 "\t%" PRIu64
			"\t%s\t%s\t%" PRIu64 "\n",
			totals.streams,
			totals.blocks,
			totals.compressed_size,
			totals.uncompressed_size,
			get_ratio(totals.compressed_size,
				totals.uncompressed_size),
			get_check_names(totals.checks, false),
			totals.files);

	return;
}


extern void
list_totals(void)
{
	if (opt_robot) {
		// Always print totals in --robot mode. It can be convenient
		// in some cases and doesn't complicate usage of the
		// single-file case much.
		print_totals_robot();

	} else if (totals.files > 1) {
		// For non-robot mode, totals are printed only if there
		// is more than one file.
		if (message_verbosity_get() <= V_WARNING)
			print_totals_basic();
		else
			print_totals_adv();
	}

	return;
}


extern void
list_file(const char *filename)
{
	if (opt_format != FORMAT_XZ && opt_format != FORMAT_AUTO)
		message_fatal(_("--list works only on .xz files "
				"(--format=xz or --format=auto)"));

	message_filename(filename);

	if (filename == stdin_filename) {
		message_error(_("--list does not support reading from "
				"standard input"));
		return;
	}

	// Unset opt_stdout so that io_open_src() won't accept special files.
	// Set opt_force so that io_open_src() will follow symlinks.
	opt_stdout = false;
	opt_force = true;
	file_pair *pair = io_open_src(filename);
	if (pair == NULL)
		return;

	lzma_index *idx;
	if (!parse_indexes(&idx, pair)) {
		// Update the totals that are displayed after all
		// the individual files have been listed.
		update_totals(idx);

		// We have three main modes:
		//  - --robot, which has submodes if --verbose is specified
		//    once or twice
		//  - Normal --list without --verbose
		//  - --list with one or two --verbose
		if (opt_robot)
			print_info_robot(idx, pair);
		else if (message_verbosity_get() <= V_WARNING)
			print_info_basic(idx, pair);
		else
			print_info_adv(idx, pair);

		lzma_index_end(idx, NULL);
	}

	io_close(pair, false);
	return;
}
