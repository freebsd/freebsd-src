/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026, Netflix, Inc.
 *
 * This software was developed by Ali Mashtizadeh under the sponsorship from
 * Netflix, Inc.
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
 */
#ifndef __VIEW_HH__
#define __VIEW_HH__

#include <libdwarf.h>
#include <libelf.h>
#include <libutil.h>
#include <gelf.h>

#include <vector>

#include "headers.hh"
#include "util.hh"

/*
 * PMC counter state.
 */
struct pmcinfo
{
	pmcinfo() : name(), rate(-1), event(0), count(0) { }
	pmcinfo(struct pmclog_ev_pmcallocate &p) {
		name = p.pl_evname;
		rate = p.pl_rate;
		event = p.pl_event;
		count = 0;
	}
	pmcinfo(struct pmclog_ev_pmcallocatedyn &p) {
		name = p.pl_evname;
		rate = 0;
		event = p.pl_event;
		count = 0;
	}
	~pmcinfo() { }
	std::string			name;
	int				rate;
	uint32_t			event;
	uint64_t			count; // Available for application use
};

/*
 * Extended pmcinfo structure stores the complete event description passed to
 * libpmc.
 */
struct pmcinfox
{
	pmcinfox() : rate(0), event() { }
	pmcinfox(uint32_t rate_, std::string event_) : rate(rate_), event(event_) { }
	~pmcinfox() { }
	uint32_t			rate;
	std::string			event;
};

/*
 * CPUID Leafs for X86 Logs
 */
struct cpuidleaf
{
	uint32_t			eax;
	uint32_t			ebx;
	uint32_t			ecx;
	uint32_t			edx;
};

/*
 * Stores the address range for an executable segment in the process.
 */
struct vmmap
{
	vmmap() : lowpc(0), highpc(0), image("") { }
	~vmmap() { }
	uint64_t			lowpc;
	uint64_t			highpc;
	std::string			image;
};

struct threadinfo
{
	threadinfo() : name("") { }
	threadinfo(const std::string &name_) : name(name_) { }
	~threadinfo() { }
	std::string			name;
};

struct procinfo
{
	procinfo() : name(""), fullpath(""), map(), threads(),
	    baseaddr(0), dynaddr(0) { }
	procinfo(struct pmclog_ev_proccreate &p) {
		std::string pname = p.pl_pcomm;

		if ((p.pl_flags & P_KPROC) != 0)
			name = "[" + pname + "]";
		else
			name = pname;
		map = std::map<uint64_t, vmmap>();
		threads = std::map<pid_t, threadinfo>();
		baseaddr = 0;
		dynaddr = 0;
	}
	~procinfo() { }
	std::string			name;
	std::string			fullpath;
	std::map<uint64_t, vmmap>	map;
	std::map<pid_t, threadinfo>	threads;
	uint64_t			baseaddr;
	uint64_t			dynaddr;
};

/*
 * Symbol information
 */
struct syminfo
{
	syminfo() : binary(""), name(""), offset(0), length(0), funcoff(0),
	    line(0), column(0) { }
	~syminfo() { }
	std::string to_string(bool show_line = true);
	std::string			binary;
	std::string			name;
	uint64_t			offset;
	uint64_t			length;
	uint64_t			funcoff;
	uint32_t			line;
	uint32_t			column;
};

struct image
{
	image() : isdynamic(false), vaddr(0), start(0), end(0),
	    offset(0), name(""), binary(""), path(""), loader(""),
	    dwarf(""), symbols(), dbg_fd(-1), dbg_elf(NULL) { }
	~image() { }
	bool				isdynamic;
	uint64_t			vaddr;
	uint64_t			start;
	uint64_t			end;
	uint64_t			offset;
	std::string			name;
	std::string			binary;
	std::string			path;
	std::string			loader;
	std::string			dwarf;
	std::map<uint64_t, syminfo>	symbols;
	int				dbg_fd;
	Elf				*dbg_elf;
	Dwarf_Debug			dbg_dwarf;
};

struct pmcfilter
{
	pmcfilter() : filterpid(false), filtertid(false),
	    filterprogram(false), filterthread(false),
	    filterevent(false), filtercpu(false),
	    kernelonly(false), useronly(false),
	    pids(), tids(), programs(), threads(),
	    ibs_ldlat(0), ibs_oplat(0) { CPU_ZERO(&cpus); }
	~pmcfilter() { }
	bool				filterpid;
	bool				filtertid;
	bool				filterprogram;
	bool				filterthread;
	bool				filterevent;
	bool				filtercpu;
	bool				kernelonly;
	bool				useronly;
	std::unordered_set<int>		pids;
	std::unordered_set<int>		tids;
	std::unordered_set<std::string>	programs;
	std::unordered_set<std::string>	threads;
	std::unordered_set<std::string>	events;
	cpuset_t			cpus;
	/*
	 * Advanced filters for AMD IBS but should be generalized to support
	 * other processors.
	 */
	uint64_t			ibs_ldlat;
	uint64_t			ibs_oplat;
	bool				ibs_mmio;
	void addpids(const char *ids) {
		split_and_insert(&pids, ids);
		filterpid = true;
	}
	void addtids(const char *ids) {
		split_and_insert(&tids, ids);
		filtertid = true;
	}
	void addthreads(const char *ids) {
		split_and_insert(&threads, ids);
		filterthread = true;
	}
	void addprograms(const char *ids) {
		split_and_insert(&programs, ids);
		filterprogram = true;
	}
	void addevents(const char *ids) {
		split_and_insert(&events, ids);
		filterevent = true;
	}
	void addcpus(const char *ids) {
		if (cpuset_parselist(ids, &cpus) == CPUSET_PARSE_OK)
			filtercpu = true;
		else
			fprintf(stderr, "Cannot parse cpulist");
	}
};

/*
 * These macros help provide a uniform filter facility across all passes.  
 * Filter options that are specific to a special feature, e.g., IBS, should 
 * only be accessible through long options, and we reserve option number 0-9 
 * for tool specific arguments.
 */
#define PMCFILTER_PRINTOPTS()							\
	do {									\
		printf("Filter Options:\n");					\
		printf("\t-t,--lwps         Filter by thread id\n");		\
		printf("\t-p,--pids         Filter by process id\n");		\
		printf("\t-T,--threads      Filter by thread name\n");		\
		printf("\t-P,--processes    Filter by process name\n");		\
		printf("\t-e,--event        Filter by event\n");		\
		printf("\t-c,--cpus         Filter by CPU id\n");		\
		printf("\t-k,--kernel       Show only kernel events\n");	\
		printf("\t-u,--user         Show only user events\n");		\
		printf("\nIBS Fetch Options:\n");				\
		printf("\t--ldlat           Filter by load latency\n");		\
		printf("\nIBS Op Options:\n");					\
		printf("\t--ldlat           Filter by load latency\n");		\
		printf("\t--oplat           Filter by operation latency\n");	\
		printf("\t--mmio            Show only mmio events\n");		\
	} while (0)
#define PMCFILTER_LOPTS						\
	{ "lwps",	required_argument,	NULL,	't' },	\
	{ "pids",	required_argument,	NULL,	'p' },	\
	{ "threads",	required_argument,	NULL,	'T' },	\
	{ "processes",	required_argument,	NULL,	'P' },	\
	{ "events",	required_argument,	NULL,	'e' },	\
	{ "cpus",	required_argument,	NULL,	'c' },	\
	{ "kernel",	no_argument,		NULL,	'k' },	\
	{ "user",	no_argument,		NULL,	'u' },	\
	{ "ldlat",	required_argument,	NULL,	10  },	\
	{ "oplat",	required_argument,	NULL,	11  },	\
	{ "mmio",	no_argument,		NULL,	12  }
#define PMCFILTER_SOPTS "t:p:T:P:e:c:ku"
#define PMCFILTER_CASE(_filt) \
	case 't':						\
		_filt.addtids(optarg);				\
		break;						\
	case 'p':						\
		_filt.addpids(optarg);				\
		break;						\
	case 'T':						\
		_filt.addthreads(optarg);			\
		break;						\
	case 'P':						\
		_filt.addprograms(optarg);			\
		break;						\
	case 'e':						\
		_filt.addevents(optarg);			\
		break;						\
	case 'c':						\
		_filt.addcpus(optarg);				\
		break;						\
	case 'k':						\
		_filt.kernelonly = true;			\
		break;						\
	case 'u':						\
		_filt.useronly = true;			\
		break;						\
	case 10:						\
		_filt.ibs_ldlat = atoi(optarg);			\
		break;						\
	case 11:						\
		_filt.ibs_oplat = atoi(optarg);			\
		break;						\
	case 12:						\
		_filt.ibs_mmio = true;				\
		break;

struct ibsfetchinfo
{
	int				len;
	uint64_t			ctl;
	uint64_t			extctl;
	uint64_t			linaddr;
	uint64_t			physaddr;
};

struct ibsopinfo
{
	int				len;
	uint64_t			ctl;
	uint64_t			rip;
	uint64_t			data;
	uint64_t			data2;
	uint64_t			data3;
	uint64_t			linaddr;
	uint64_t			physaddr;
	uint64_t			tgtrip;
	uint64_t			data4;
};

class pmcview
{
protected:
	pmcview();
	virtual ~pmcview();
public:
	// Filter
	void setfilter(const pmcfilter &pmcfilter);
	// View Hooks
	virtual void proccreate(__unused pid_t pid) { };
	virtual void procexec(__unused pid_t pid) { };
	virtual void procexit(__unused pid_t pid) { };
	virtual void callchain(struct pmclog_ev_callchain &p,
	    __unused ibsfetchinfo &f, uintfptr_t *cc, int len) {
		callchain(p, cc, len);
	}
	virtual void callchain(struct pmclog_ev_callchain &p,
	    __unused ibsopinfo &o, uintfptr_t *cc, int len) {
		callchain(p, cc, len);
	}
	virtual void callchain(struct pmclog_ev_callchain &p,
	    __unused uintfptr_t *cc, __unused int len) {
		callchain(p);
	}
	virtual void callchain(__unused struct pmclog_ev_callchain &p) { }
	// Console/File output
	virtual void print() { };
	virtual int process_sysinfo(int logfd, const pmchdr_infohdr &infohdr);
	virtual int process_pmcinfo(int logfd, const pmchdr_infohdr &infohdr);
	virtual int process_cpuidinfo(int logfd, const pmchdr_infohdr &infohdr);
	// Process log
	virtual void process(int logfd);
	virtual void process(struct pmclog_ev &p);
	virtual void process(struct pmclog_parse_state *ps);
	// Generic events
	virtual void process(struct pmclog_ev_initialize &p);
	virtual void process(struct pmclog_ev_closelog &p);
	virtual void process(struct pmclog_ev_pmcallocate &p);
	virtual void process(struct pmclog_ev_pmcallocatedyn &p);
	// Process events
	virtual void process(struct pmclog_ev_proccreate &p);
	virtual void process(struct pmclog_ev_procexit &p);
	virtual void process(struct pmclog_ev_procexec &p);
	virtual void process(struct pmclog_ev_procfork &p);
	virtual void process(struct pmclog_ev_sysexit &p);
	// Thread events
	virtual void process(struct pmclog_ev_threadcreate &p);
	virtual void process(struct pmclog_ev_threadexit &p);
	// Map events
	virtual void process(struct pmclog_ev_map_in &p);
	virtual void process(struct pmclog_ev_map_out &p);
	// Callchain events
	virtual void process(struct pmclog_ev_callchain &p);
protected:
	// Helper functions for views
	uint32_t pmcidtoeventid(uint32_t pmcid);
	std::string pidtoname(pid_t pid);
	syminfo addrtosymbol(pid_t pid, uint64_t addr);
	void printvm(pid_t pid);
	// Fields available to views
	uint64_t				tscfreq;
	std::unordered_map<uint32_t, uint32_t>	pmcid;
	std::unordered_map<uint32_t, struct pmcinfo> pmcinfo;
	std::unordered_map<pid_t, struct procinfo> procs;
	std::unordered_map<pid_t, pid_t>	tidtopid;
	std::unordered_map<std::string, image>	images;
	std::string				sysroot;
	pmcfilter				filter;
	// Extended info
	int					arch;
	std::string				cpumodel;
	std::string				osrelease;
	std::string				buildid;
	std::vector<struct pmcinfox>		extpmcinfo;
	std::map<uint32_t, struct cpuidleaf>	cpuid; // x86 Only
private:
	image loadimage(const std::string &path);
	void mapimage(pid_t pid, const image &im, uint64_t linkaddr);
	void loadsymboltable(image *im, Elf *e, Elf_Scn *scn, GElf_Shdr *sh);
	void loadsymbols(image *im);
};

#endif /* __VIEW_HH__ */
