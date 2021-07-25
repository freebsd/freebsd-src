/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2013 David Chisnall
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract (FA8750-10-C-0237)
 * ("CTSRD"), as part of the DARPA CRASH research programme.
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
 *
 * $FreeBSD$
 */

#include <sys/resource.h>
#include <fcntl.h>
#include <libgen.h>
#include <unistd.h>

#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

#include "fdt.hh"
#include "checking.hh"
#include "util.hh"

using namespace dtc;
using std::string;

namespace {

/**
 * The current major version of the tool.
 */
constexpr int version_major = 0;
constexpr int version_major_compatible = 1;
/**
 * The current minor version of the tool.
 */
constexpr int version_minor = 5;
constexpr int version_minor_compatible = 4;
/**
 * The current patch level of the tool.
 */
constexpr int version_patch = 0;
constexpr int version_patch_compatible = 7;

void usage(const string &argv0) noexcept
{
	std::fprintf(stderr, "Usage:\n"
		"\t%s\t[-fhsv@] [-b boot_cpu_id] [-d dependency_file]"
			"[-E [no-]checker_name]\n"
		"\t\t[-H phandle_format] [-I input_format]"
			"[-O output_format]\n"
		"\t\t[-o output_file] [-R entries] [-S bytes] [-p bytes]"
			"[-V blob_version]\n"
		"\t\t-W [no-]checker_name] input_file\n", basename(argv0).c_str());
}

/**
 * Prints the current version of this program..
 */
void version(const char* progname) noexcept
{
	std::fprintf(stdout, "Version: %s %d.%d.%d compatible with gpl dtc %d.%d.%d\n", progname,
		version_major, version_minor, version_patch,
		version_major_compatible, version_minor_compatible,
		version_patch_compatible);
}

} // Anonymous namespace

using fdt::device_tree;
using fdt::tree_write_fn_ptr;
using fdt::tree_read_fn_ptr;

int
main(int argc, char **argv)
{
	int ch;
	int outfile = fileno(stdout);
	const char *outfile_name = "-";
	const char *in_file = "-";
	FILE *depfile = 0;
	bool debug_mode = false;
	tree_write_fn_ptr write_fn = nullptr;
	tree_read_fn_ptr read_fn = nullptr;
	uint32_t boot_cpu = 0;
	bool boot_cpu_specified = false;
	bool keep_going = false;
	bool sort = false;
	clock_t c0 = clock();
	class device_tree tree;
	fdt::checking::check_manager checks;
	const char *options = "@hqI:O:o:V:d:R:S:p:b:fi:svH:W:E:DP:";

	// Don't forget to update the man page if any more options are added.
	while ((ch = getopt(argc, argv, options)) != -1)
	{
		switch (ch)
		{
		case 'h':
			usage(argv[0]);
			return EXIT_SUCCESS;
		case 'v':
			version(argv[0]);
			return EXIT_SUCCESS;
		case '@':
			tree.write_symbols = true;
			break;
		case 'I':
		{
			string arg(optarg);
			if (arg == "dtb")
			{
				read_fn = &device_tree::parse_dtb;
				if (write_fn == nullptr)
				{
					write_fn = &device_tree::write_dts;
				}
			}
			else if (arg == "dts")
			{
				read_fn = &device_tree::parse_dts;
			}
			else
			{
				std::fprintf(stderr, "Unknown input format: %s\n", optarg);
				return EXIT_FAILURE;
			}
			break;
		}
		case 'O':
		{
			string arg(optarg);
			if (arg == "dtb")
			{
				write_fn = &device_tree::write_binary;
			}
			else if (arg == "asm")
			{
				write_fn = &device_tree::write_asm;
			}
			else if (arg == "dts")
			{
				write_fn = &device_tree::write_dts;
				if (read_fn == nullptr)
				{
					read_fn = &device_tree::parse_dtb;
				}
			}
			else
			{
				std::fprintf(stderr, "Unknown output format: %s\n", optarg);
				return EXIT_FAILURE;
			}
			break;
		}
		case 'o':
		{
			outfile_name = optarg;
			if (std::strcmp(outfile_name, "-") != 0)
			{
				outfile = open(optarg, O_CREAT | O_TRUNC | O_WRONLY, 0666);
				if (outfile == -1)
				{
					perror("Unable to open output file");
					return EXIT_FAILURE;
				}
			}
			break;
		}
		case 'D':
			debug_mode = true;
			break;
		case 'V':
			if (string(optarg) != "17")
			{
				std::fprintf(stderr, "Unknown output format version: %s\n", optarg);
				return EXIT_FAILURE;
			}
			break;
		case 'd':
		{
			if (depfile != 0)
			{
				std::fclose(depfile);
			}
			if (string(optarg) == "-")
			{
				depfile = stdout;
			}
			else
			{
				depfile = fdopen(open(optarg, O_CREAT | O_TRUNC | O_WRONLY, 0666), "w");
				if (depfile == 0)
				{
					perror("Unable to open dependency file");
					return EXIT_FAILURE;
				}
			}
			break;
		}
		case 'H':
		{
			string arg(optarg);
			if (arg == "both")
			{
				tree.set_phandle_format(device_tree::BOTH);
			}
			else if (arg == "epapr")
			{
				tree.set_phandle_format(device_tree::EPAPR);
			}
			else if (arg == "linux")
			{
				tree.set_phandle_format(device_tree::LINUX);
			}
			else
			{
				std::fprintf(stderr, "Unknown phandle format: %s\n", optarg);
				return EXIT_FAILURE;
			}
			break;
		}
		case 'b':
			// Don't bother to check if strtoll fails, just
			// use the 0 it returns.
			boot_cpu = static_cast<uint32_t>(strtoll(optarg, 0, 10));
			boot_cpu_specified = true;
			break;
		case 'f':
			keep_going = true;
			break;
		case 'W':
		case 'E':
		{
			string arg(optarg);
			if ((arg.size() > 3) && (std::strncmp(optarg, "no-", 3) == 0))
			{
				arg = string(optarg+3);
				if (!checks.disable_checker(arg))
				{
					std::fprintf(stderr, "Checker %s either does not exist or is already disabled\n", optarg+3);
				}
				break;
			}
			if (!checks.enable_checker(arg))
			{
				std::fprintf(stderr, "Checker %s either does not exist or is already enabled\n", optarg);
			}
			break;
		}
		case 's':
		{
			sort = true;
			break;
		}
		case 'i':
		{
			tree.add_include_path(optarg);
			break;
		}
		// Should quiet warnings, but for now is silently ignored.
		case 'q':
			break;
		case 'R':
			tree.set_empty_reserve_map_entries(strtoll(optarg, 0, 10));
			break;
		case 'S':
			tree.set_blob_minimum_size(strtoll(optarg, 0, 10));
			break;
		case 'p':
			tree.set_blob_padding(strtoll(optarg, 0, 10));
			break;
		case 'P':
			if (!tree.parse_define(optarg))
			{
				std::fprintf(stderr, "Invalid predefine value %s\n",
				        optarg);
			}
			break;
		default:
			/* 
			 * Since opterr is non-zero, getopt will have
			 * already printed an error message.
			 */
			return EXIT_FAILURE;
		}
	}
	if (read_fn == nullptr)
	{
		read_fn = &device_tree::parse_dts;
	}
	if (write_fn == nullptr)
	{
		write_fn = &device_tree::write_binary;
	}
	if (optind < argc)
	{
		in_file = argv[optind];
	}
	if (depfile != 0)
	{
		std::fputs(outfile_name, depfile);
		std::fputs(": ", depfile);
		std::fputs(in_file, depfile);
	}
	clock_t c1 = clock();
	(tree.*read_fn)(in_file, depfile);
	// Override the boot CPU found in the header, if we're loading from dtb
	if (boot_cpu_specified)
	{
		tree.set_boot_cpu(boot_cpu);
	}
	if (sort)
	{
		tree.sort();
	}
	if (depfile != 0)
	{
		std::putc('\n', depfile);
		std::fclose(depfile);
	}
	if (!(tree.is_valid() || keep_going))
	{
		std::fprintf(stderr, "Failed to parse tree.\n");
		return EXIT_FAILURE;
	}
	clock_t c2 = clock();
	if (!(checks.run_checks(&tree, true) || keep_going))
	{
		return EXIT_FAILURE;
	}
	clock_t c3 = clock();
	(tree.*write_fn)(outfile);
	close(outfile);
	clock_t c4 = clock();

	if (debug_mode)
	{
		struct rusage r;

		getrusage(RUSAGE_SELF, &r);
		std::fprintf(stderr, "Peak memory usage: %ld bytes\n", r.ru_maxrss);
		std::fprintf(stderr, "Setup and option parsing took %f seconds\n",
				((double)(c1-c0))/CLOCKS_PER_SEC);
		std::fprintf(stderr, "Parsing took %f seconds\n",
				((double)(c2-c1))/CLOCKS_PER_SEC);
		std::fprintf(stderr, "Checking took %f seconds\n",
				((double)(c3-c2))/CLOCKS_PER_SEC);
		std::fprintf(stderr, "Generating output took %f seconds\n",
				((double)(c4-c3))/CLOCKS_PER_SEC);
		std::fprintf(stderr, "Total time: %f seconds\n",
				((double)(c4-c0))/CLOCKS_PER_SEC);
	}
	return EXIT_SUCCESS;
}

