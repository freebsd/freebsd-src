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

#include <sys/types.h>
#include <sys/param.h>

#include <assert.h>
#include <err.h>
#include <fcntl.h>
#include <gelf.h>
#include <inttypes.h>
#include <libelf.h>
#include <pmclog.h>
#include <sysexits.h>
#include <unistd.h>

#include <cxxabi.h>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

#include <dev/hwpmc/hwpmc_ibs.h>
#include "util.hh"
#include "view.hh"

std::string
syminfo::to_string(bool show_line)
{
	int status;
	char *demangled;
	std::stringstream ss;

	if (name == "") {
		ss << "0x" << std::hex << offset;
	} else if (name.length() >= 2 && name[0] == '_' && name[1] == 'Z') {
		demangled = abi::__cxa_demangle(name.c_str(), nullptr, nullptr, &status);
		if (status == 0) {
			ss << demangled;
			free(demangled);
		} else {
			ss << name;
		}
	} else {
		ss << name;
	}

	if (!show_line)
		return ss.str();

	if (line) {
		ss << ":" << std::dec << line;
	} else if (funcoff) {
		ss << "+0x" << std::hex << funcoff; 
	}

	return ss.str();
}

pmcview::pmcview() : tscfreq(0), pmcid(), pmcinfo(), procs(), tidtopid(),
    images(), sysroot(""), filter()
{
	char *root;

	root = getenv("SYSROOT");
	if (root) {
		sysroot = root;
	}
}

pmcview::~pmcview()
{
}

void
pmcview::setfilter(const pmcfilter &pmcfilter)
{
	filter = pmcfilter;
}

static int
readlog(int logfd, void *buf, size_t len)
{
	int status;
	char *cur = (char *)buf;
	size_t left = len;

	while (left != 0) {
		status = read(logfd, cur, left);
		if (status < 0) {
			if (errno == EINTR || errno == EAGAIN)
				continue;
			else
				return status;
		}
		if (status == 0)
			return status;

		cur += status;
		left -= status;
	}

	return len;
}

int
pmcview::process_sysinfo(int logfd, const pmchdr_infohdr &infohdr)
{
	int status;
	pmchdr_sysinfo sysinfo;

	if (infohdr.length != sizeof(sysinfo))
		errx(EX_IOERR, "PMC log headers have an unexpected size");

	status = readlog(logfd, &sysinfo, sizeof(sysinfo));
	if (status < 0 || status != infohdr.length)
		errx(EX_IOERR, "readlog");

	cpumodel = sysinfo.cpumodel;
	osrelease = sysinfo.osrelease;
	buildid = sysinfo.buildid;

	return 0;
}

int
pmcview::process_pmcinfo(int logfd, const pmchdr_infohdr &infohdr)
{
	int status;
	union {
		pmchdr_pmcinfo pmcinfo;
		__unused char reserve_max_size[256];
	};

	status = readlog(logfd, &pmcinfo, infohdr.length);
	if (status < 0 || status != infohdr.length)
		errx(EX_IOERR, "readlog");

	extpmcinfo.emplace_back(pmcinfo.rate, std::string(pmcinfo.pmc));

	return 0;
}

int
pmcview::process_cpuidinfo(int logfd, const pmchdr_infohdr &infohdr)
{
	int status;
	pmchdr_cpuidinfo *cpuidinfo;
	int offset, len;
	uint32_t root, maxleaf, count;

	len = infohdr.length / 4;
	cpuidinfo = (pmchdr_cpuidinfo *)new uint32_t[len];

	status = readlog(logfd, cpuidinfo, infohdr.length);
	if (status < 0 || status != infohdr.length)
		errx(EX_IOERR, "readlog");

	offset = 0;

	while (offset < len) {
		maxleaf = cpuidinfo->cpuid[offset];

		/*
		 * In x86 the roots contains the maximum leaf number present.
		 */
		root = maxleaf & 0xFFFF0000;
		count = maxleaf & 0x0000FFFF;

		for (uint32_t i = 0; i <= count; i++) {
			cpuid[root + i] = { cpuidinfo->cpuid[4 * i + offset],
			    cpuidinfo->cpuid[4 * i + 1 + offset],
			    cpuidinfo->cpuid[4 * i + 2 + offset],
			    cpuidinfo->cpuid[4 * i + 3 + offset] };
		}

		offset += 4 * (count + 1);
	}

	delete[] cpuidinfo;

	return 0;
}

void
pmcview::process(int logfd)
{
	int status;
	pmchdr_header hdr;
	struct pmclog_parse_state *ps;

	// Read header
	status = readlog(logfd, &hdr, sizeof(hdr));
	if (status < 0 || status != sizeof(hdr))
		err(EX_IOERR, "readlog");
	if (hdr.magic != PMC_HEADER_MAGIC)
		errx(EX_DATAERR, "PMC log magic mismatch!");
	if (hdr.version > PMC_HEADER_VERSION)
		errx(EX_DATAERR, "PMC log version newer than supported!");

	// Save architecture
	arch = hdr.arch;

	while (1) {
		pmchdr_infohdr infohdr;

		status = readlog(logfd, &infohdr, sizeof(infohdr));
		if (status < 0 || status != sizeof(infohdr))
			errx(EX_IOERR, "readlog");

		if (infohdr.type == INFOHDR_TYPE_DONE) {
			break;
		} else if (infohdr.type == INFOHDR_TYPE_SYSINFO) {
			process_sysinfo(logfd, infohdr);
		} else if (infohdr.type == INFOHDR_TYPE_PMCINFO) {
			process_pmcinfo(logfd, infohdr);
		} else if (infohdr.type == INFOHDR_TYPE_CPUID) {
			process_cpuidinfo(logfd, infohdr);
		}
	}

	ps = static_cast<struct pmclog_parse_state*>(pmclog_open(logfd));
	if (ps == NULL) {
		errx(EX_OSERR, "ERROR: Cannot allocate pmclog parse state: %s\n",
		    strerror(errno));
	}

	process(ps);

	pmclog_close(ps);
}

void
pmcview::process(struct pmclog_ev &p)
{
	switch (p.pl_type) {
	case PMCLOG_TYPE_INITIALIZE:
		process(p.pl_u.pl_i);
		return;
	case PMCLOG_TYPE_CLOSELOG:
		process(p.pl_u.pl_cl);
		return;
	case PMCLOG_TYPE_PMCALLOCATE:
		process(p.pl_u.pl_a);
		return;
	case PMCLOG_TYPE_PMCALLOCATEDYN:
		process(p.pl_u.pl_ad);
		return;
	case PMCLOG_TYPE_PROC_CREATE:
		process(p.pl_u.pl_pc);
		return;
	case PMCLOG_TYPE_PROCEXIT:
		process(p.pl_u.pl_e);
		return;
	case PMCLOG_TYPE_PROCEXEC:
		process(p.pl_u.pl_x);
		return;
	case PMCLOG_TYPE_PROCFORK:
		process(p.pl_u.pl_f);
		return;
	case PMCLOG_TYPE_SYSEXIT:
		process(p.pl_u.pl_se);
		return;
	case PMCLOG_TYPE_MAP_IN:
		process(p.pl_u.pl_mi);
		return;
	case PMCLOG_TYPE_MAP_OUT:
		process(p.pl_u.pl_mo);
		return;
	case PMCLOG_TYPE_THR_CREATE:
		process(p.pl_u.pl_tc);
		return;
	case PMCLOG_TYPE_THR_EXIT:
		process(p.pl_u.pl_te);
		return;
	case PMCLOG_TYPE_CALLCHAIN:
		process(p.pl_u.pl_cc);
		return;
	case PMCLOG_TYPE_PMCATTACH: [[fallthrough]];
	case PMCLOG_TYPE_PMCDETACH:
	case PMCLOG_TYPE_USERDATA:
	case PMCLOG_TYPE_PROCCSW:
	case PMCLOG_TYPE_DROPNOTIFY:
		return;
	};
}

void
pmcview::process(struct pmclog_parse_state *p)
{
	struct pmclog_ev ev;

	while (pmclog_read(p, &ev) == 0) {
		process(ev);
	}
}

void
pmcview::process(struct pmclog_ev_initialize &p)
{
	tscfreq = p.pl_tsc_freq;
}

void
pmcview::process(__unused struct pmclog_ev_closelog &p)
{
}

void
pmcview::process(struct pmclog_ev_pmcallocate &p)
{
	pmcid[p.pl_pmcid] = p.pl_event;
	if (pmcinfo.find(p.pl_event) == pmcinfo.end()) {
		pmcinfo.emplace(p.pl_event, p);
	}
}

void
pmcview::process(struct pmclog_ev_pmcallocatedyn &p)
{
	pmcid[p.pl_pmcid] = p.pl_event;
	if (pmcinfo.find(p.pl_event) == pmcinfo.end()) {
		pmcinfo.emplace(p.pl_event, p);
	}
}

void
pmcview::process(struct pmclog_ev_proccreate &p)
{
	procs[p.pl_pid] = procinfo(p);

	proccreate(p.pl_pid);
}

void
pmcview::process(struct pmclog_ev_procexit &p)
{
	/*
	 * XXX: Seems we can recieve samples after the process exits we need 
	 * some way to delay cleanup, then after the delayed cleanup discard 
	 * delayed events.
	 */
	procs.erase(p.pl_pid);

	procexit(p.pl_pid);
}

void
pmcview::process(struct pmclog_ev_procexec &p)
{
	procs[p.pl_pid].map.clear();
	procs[p.pl_pid].name = basename(p.pl_pathname);
	procs[p.pl_pid].fullpath = p.pl_pathname;
	procs[p.pl_pid].baseaddr = p.pl_baseaddr;
	procs[p.pl_pid].dynaddr = p.pl_dynaddr;

	image im = loadimage(p.pl_pathname);

	mapimage(p.pl_pid, im, im.vaddr + p.pl_dynaddr);

	/*
	 * Map the dynamic runtime loader
	 */
	if (im.isdynamic) {
		image rtldim = loadimage(im.loader);

		mapimage(p.pl_pid, rtldim, p.pl_baseaddr);
	}

	procexec(p.pl_pid);
}

void
pmcview::process(struct pmclog_ev_procfork &p)
{
	procs[p.pl_newpid] = procs[p.pl_oldpid];

	proccreate(p.pl_newpid);
}

void
pmcview::process(struct pmclog_ev_sysexit &p)
{
	procs.erase(p.pl_pid);
}

void
pmcview::process(struct pmclog_ev_threadcreate &p)
{
	procs[p.pl_pid].threads[p.pl_tid] = threadinfo(p.pl_tdname);
	tidtopid[p.pl_tid] = p.pl_pid;
}

void
pmcview::process(struct pmclog_ev_threadexit &p)
{
	pid_t pid = tidtopid[p.pl_tid];
	procs[pid].threads.erase(p.pl_tid);
	tidtopid.erase(p.pl_tid);
}

/*
 * Load the ELF file to compute the values needed for the memory map and check 
 * if the dwarf symbols are available.
 */
image
pmcview::loadimage(const std::string &path)
{
	std::string fullpath;
	image im;
	GElf_Ehdr eh;
	Elf *e;
	const char *elf;
	uint64_t start;
	uint64_t end;
	size_t shstrndx;
	int fd, i;
	bool foundexec;

	if (images.find(path) != images.end())
		return images[path];

	im = image();

	if (path == "unknown") {
		return (im);
	}

	fullpath = sysroot + path;

	if (access(fullpath.c_str(), R_OK) != 0)
		return (im);

	fd = open(fullpath.c_str(), O_RDONLY, 0);
	if (fd < 0) {
		warnx("WARNING: Cannot open \"%s\".",
		    fullpath.c_str());
		return (im);
	}

	e = elf_begin(fd, ELF_C_READ, NULL);
	if (e == NULL) {
		warnx("WARNING: Cannot read \"%s\".",
		    fullpath.c_str());
		close(fd);
		return (im);
	}

	if (gelf_getehdr(e, &eh) != &eh) {
		warnx("WARNING: Cannot read the ELF header for \"%s\": %s.",
		    fullpath.c_str(), elf_errmsg(-1));
		goto done;
	}

	if (eh.e_type != ET_EXEC && eh.e_type != ET_DYN && eh.e_type != ET_REL) {
		warnx("WARNING: ELF file type is unsupported for \"%s\".",
		    fullpath.c_str());;
		goto done;
	}

	elf = elf_rawfile(e, NULL);
	if (elf == NULL) {
		warnx("WARNING: Cannot read the ELF file for \"%s\": %s.",
		    fullpath.c_str(), elf_errmsg(-1));
		goto done;
	}

	if (eh.e_type != ET_REL) {
		foundexec = false;
		for (i = 0; i < eh.e_phnum; i++) {
			GElf_Phdr ph;

			if (gelf_getphdr(e, i, &ph) != &ph) {
				warnx("WARNING: Cannot read program header for \"%s\": %s.",
				    fullpath.c_str(), elf_errmsg(-1));
				goto done;
			}

			if (ph.p_type == PT_DYNAMIC) {
				im.isdynamic = 1;
				continue;
			}

			if (ph.p_type == PT_INTERP) {
				im.loader = elf + ph.p_offset;
			}

			if (ph.p_type == PT_LOAD) {
				if ((ph.p_flags & PF_X) != 0 && !foundexec) {
					im.vaddr = ph.p_vaddr & ~(ph.p_align - 1);
					foundexec = true;
				}
			}
		}
	}

	elf_getshdrstrndx(e, &shstrndx);

	start = ~(0x0ULL);
	end = 0;
	for (i = 0; i < eh.e_shnum; i++) {
		GElf_Shdr sh;
		Elf_Scn *scn;
		char *scname;

		scn = elf_getscn(e, i);
		if (scn == NULL) {
			warnx("WARNING: Could not retrieve section descriptor for \"%s\".",
			    fullpath.c_str());
			goto done;
		}

		if (gelf_getshdr(scn, &sh) != &sh) {
			warnx("WARNING: Could not retrieve section header for \"%s\".",
			    fullpath.c_str());
			goto done;
		}

		if (sh.sh_flags & SHF_EXECINSTR) {
			start = std::min(start, sh.sh_addr);
			end = std::max(end, sh.sh_addr + sh.sh_size);
		}

		// Check if dwarf is embedded
		scname = elf_strptr(e, shstrndx, sh.sh_name);
		if (scname != NULL && strcmp(scname, ".debug_info") == 0)
			im.dwarf = fullpath;
	}

	im.start = start;
	im.end = end;
	im.binary = path;
	im.path = fullpath;
	im.name = basename(fullpath);

	/*
	 * If the dwarf symbols aren't embedded check:
	 *  1. SYSROOT + /path + .debug
	 *  2. SYSROOT + /usr/lib/debug + /path + .debug
	 */
	if (im.dwarf == "") {
		std::string sympath = fullpath + ".debug";
		if (access(sympath.c_str(), R_OK) == 0) {
			im.dwarf = sympath;
		}
	}
	if (im.dwarf == "") {
		std::string sympath = sysroot + "/usr/lib/debug" + path + ".debug";
		if (access(sympath.c_str(), R_OK) == 0) {
			im.dwarf = sympath;
		}
	}

	images[path] = im;

done:
	elf_end(e);
	close(fd);

	return (im);
}

void
pmcview::loadsymboltable(image *im, Elf *e, Elf_Scn *scn, GElf_Shdr *sh)
{
	size_t n, nsyms;
	char *fname;
	GElf_Sym sym;
	Elf_Data *data;
	syminfo si;

	si = syminfo();

	if ((data = elf_getdata(scn, nullptr)) == nullptr)
		return;

	nsyms = sh->sh_size / sh->sh_entsize;

	for (n = 0; n < nsyms; n++) {
		if (gelf_getsym(data, (int) n, &sym) != &sym)
			return;

		// XXX: We should load globals as well
		if (GELF_ST_TYPE(sym.st_info) != STT_FUNC)
			continue;

		if (sym.st_shndx == STN_UNDEF)
			continue;

		if ((fname = elf_strptr(e, sh->sh_link, sym.st_name)) == NULL)
			continue;

		// XXX: Extra checks to make sure we don't get corrupted
		for (int i = 0; fname[i] != 0 && i < 32; i++) {
			if (fname[i] < 0) {
				printf("EEEK SYMBOL\n");
				printf("%s\n", fname);
				assert(false);
			}
		}

		si.offset = sym.st_value;
		si.length = sym.st_size;
		si.binary = im->name;
		si.name = fname;

		im->symbols[si.offset] = si;
        }
}

void
pmcview::loadsymbols(image *im)
{
	int i;
	int fd;
	Elf *e;
	Elf_Scn *scn;
	GElf_Ehdr eh;
	GElf_Shdr sh;

	if (access(im->path.c_str(), R_OK) != 0)
		return;

	fd = open(im->path.c_str(), O_RDONLY, 0);
	if (fd < 0) {
		warnx("WARNING: Cannot open \"%s\".",
		    im->path.c_str());
		return;
	}

	e = elf_begin(fd, ELF_C_READ, NULL);
	if (e == NULL) {
		warnx("WARNING: Cannot read \"%s\".",
		    im->path.c_str());
		close(fd);
		return;
	}

	if (gelf_getehdr(e, &eh) != &eh) {
		warnx("WARNING: Cannot read the ELF header for \"%s\": %s.",
		    im->path.c_str(), elf_errmsg(-1));
		goto done;
	}

	for (i = 0; i < eh.e_shnum; i++) {
		scn = elf_getscn(e, i);
		if (scn == NULL) {
			warnx("WARNING: Could not retrieve section descriptor for \"%s\".",
			    im->path.c_str());
			goto done;
		}

		if (gelf_getshdr(scn, &sh) != &sh) {
			warnx("WARNING: Could not retrieve section header for \"%s\".",
			    im->path.c_str());
			goto done;
		}

		if (sh.sh_type == SHT_SYMTAB || sh.sh_type == SHT_DYNSYM) {
			loadsymboltable(im, e, scn, &sh);
		}
	}

done:
	elf_end(e);
	close(fd);
}

void
pmcview::mapimage(int pid, const image &im, uint64_t start)
{
	uint64_t offset;
	vmmap map;

	// Ignore empty images
	if (im.start == 0 && im.end == 0)
		return;

	/*
	 * XXX: Need to adjust the address for the PowerPC kernel that is 
	 * dynamic. Wonder if we can fix this elsewhere, because we should need 
	 * a way to deal with this for KASLR?.
	 */

	offset = start - im.vaddr;
	map.lowpc = im.start + offset;
	map.highpc = im.end + offset;
	map.image = im.binary;

	procs[pid].map[start] = map;
}

void
pmcview::process(struct pmclog_ev_map_in &p)
{
	// Kernel map-in events should be mapped to pid 0
	pid_t pid = (p.pl_pid == -1) ? 0 : p.pl_pid;

	image im = loadimage(p.pl_pathname);

	mapimage(pid, im, p.pl_start);
}

void
pmcview::process(struct pmclog_ev_map_out &p)
{
	// Kernel map-in events should be mapped to pid 0
	pid_t pid = (p.pl_pid == -1) ? 0 : p.pl_pid;
	procinfo &proc = procs[pid];

	/*
	 * XXX: We should handle all the mmap/munmap cases but only executable 
	 * file mappings are included.
	 */
	proc.map.erase(p.pl_start);
}

void
pmcview::process(struct pmclog_ev_callchain &p)
{
	int i;
	uint8_t *hdr = (uint8_t *)&p.pl_pc[0];
	uint8_t type, len;
	uintfptr_t *cc = &p.pl_pc[1];
	ibsfetchinfo ibsf;
	ibsopinfo ibso;

	/*
	 * Callchain events are always attributed to one of the kernel
	 * processes.  Make sure nobody breaks our assumption.
	 */
	assert(p.pl_pid != ~(uint32_t)0);

	ibsf.len = 0;
	ibso.len = 0;

	if (p.pl_cpuflags & PMC_CC_F_MULTIPART) {
		for (i = 0; i < 4; i++) {
			type = hdr[2 * i];
			len = hdr[2 * i + 1];

			switch (type) {
			case PMC_CC_MULTIPART_IBS_FETCH:
				ibsf.len = len;
				ibsf.ctl = cc[PMC_MPIDX_FETCH_CTL];
				ibsf.extctl = cc[PMC_MPIDX_FETCH_EXTCTL];
				ibsf.linaddr = cc[PMC_MPIDX_FETCH_LINADDR];
				ibsf.physaddr = cc[PMC_MPIDX_FETCH_PHYSADDR];
				break;
			case PMC_CC_MULTIPART_IBS_OP:
				ibso.len = len;
				ibso.ctl = cc[PMC_MPIDX_OP_CTL];
				ibso.rip = cc[PMC_MPIDX_OP_RIP];
				ibso.data = cc[PMC_MPIDX_OP_DATA];
				ibso.data2 = cc[PMC_MPIDX_OP_DATA2];
				ibso.data3 = cc[PMC_MPIDX_OP_DATA3];
				ibso.linaddr = cc[PMC_MPIDX_OP_DC_LINADDR];
				ibso.physaddr = cc[PMC_MPIDX_OP_DC_PHYSADDR];
				ibso.tgtrip = cc[PMC_MPIDX_OP_TGT_RIP];
				ibso.data4 = cc[PMC_MPIDX_OP_DATA4];
				break;
			}

			cc += len;
		}
	}

	/*
	 * Discard delayed events so we do not get unattributed samples.
	 */
	auto pinfo = procs.find(p.pl_pid);
	if (pinfo == procs.end())
		return;

	// Filter on pid, tid, program, thread and events
	if (filter.filterpid && filter.pids.count(p.pl_pid) == 0)
		return;
	if (filter.filtertid && filter.tids.count(p.pl_tid) == 0)
		return;
	if (filter.filterprogram && filter.programs.count(pinfo->second.name) == 0)
		return;
	if (filter.filterthread) {
		auto tinfo = pinfo->second.threads.find(p.pl_tid);
		if (tinfo == pinfo->second.threads.end())
			return;
		if (filter.threads.count(tinfo->second.name) == 0)
			return;
	}
	if (filter.filterevent) {
		auto pmc = pmcid.find(p.pl_pmcid);
		if (pmc == pmcid.end())
			return;
		if (filter.events.count(pmcinfo[pmc->second].name) == 0)
			return;
	}

	// Filter on cpuset
	int cpunum = PMC_CALLCHAIN_CPUFLAGS_TO_CPU(p.pl_cpuflags);
	if (filter.filtercpu && !CPU_ISSET(cpunum, &filter.cpus)) {
		return;
	}

	// Filter on user/kernel mode
	int usermode = PMC_CALLCHAIN_CPUFLAGS_TO_USERMODE(p.pl_cpuflags);
	if (filter.useronly && usermode == 0) {
		return;
	}
	if (filter.kernelonly && usermode != 0) {
		return;
	}

	// Advanced filters for AMD IBS
	if (ibsf.len) {
		if (filter.ibs_ldlat > IBS_FETCH_CTL_TO_LAT(ibsf.ctl))
			return;
		callchain(p, ibsf, cc, len);
	} else if (ibso.len) {
		if (filter.ibs_oplat > IBS_OP_DATA_TO_COMPTORET(ibso.data))
			return;
		if (filter.ibs_ldlat > IBS_OP_DATA3_TO_DCLAT(ibso.data3))
			return;
		if (filter.ibs_mmio) {
			if (((ibso.data3 & IBS_OP_DATA3_STORE) == 0) &&
			    ((ibso.data3 & IBS_OP_DATA3_LOAD) == 0))
				return;
			if (((ibso.data3 & IBS_OP_DATA3_UCMEMACCESS) == 0) &&
			    ((ibso.data3 & IBS_OP_DATA3_WCMEMACCESS) == 0))
				return;
		}
		callchain(p, ibso, cc, len);
	} else {
		callchain(p, cc, len);
	}
}

uint32_t
pmcview::pmcidtoeventid(uint32_t id)
{
	return pmcid[id];
}

std::string
pmcview::pidtoname(pid_t pid)
{
	auto p = procs.find(pid);
	if (p == procs.end())
		return ("");
	else
		return (p->second.name);
}

syminfo
pmcview::addrtosymbol(pid_t pid, uint64_t addr)
{
	syminfo si;
	procinfo &proc = procs[pid];
	vmmap *vm;
	image *im;

	im = nullptr;
	auto v = proc.map.upper_bound(addr);
	if (v != proc.map.begin()) {
		v--;
		if (v->second.lowpc <= addr && v->second.highpc >= addr) {
			vm = &v->second;
			im = &images[v->second.image];
		}
	}

	if (im == nullptr) {
#ifdef DEBUG_VIEW
		printf("Symbol not found for 0x%lx in %s\n", addr, proc.name.c_str());
		printvm(pid);
#endif
		return syminfo();
	}

	// Adjust address into the ELF's virtual address space
	addr -= vm->lowpc - im->start;

	// Load symbols if they haven't been loaded
	if (im->symbols.size() == 0) {
		loadsymbols(im);
	}

	// Look for the first symbol
	auto s = im->symbols.upper_bound(addr);
	if (s != im->symbols.begin())
		s--;
	if (s->second.offset <= addr &&
	    (s->second.offset + s->second.length + 1) >= addr) {
		si = s->second;
		si.funcoff = addr - si.offset;
		return si;
	}

	// Special case for assembly functions in the kernel
	if (s->second.offset <= addr && s->second.length == 0 && pid == 0) {
		si = s->second;
		si.funcoff = addr - si.offset;
		return si;
	}

#ifdef DEBUG_VIEW
	printf("Symbol not found in image %lx\n", addr);
	printf("%lx %lx %s\n", s->second.offset, s->second.s_length, s->second.s_name.c_str());
	printf("%lx\n", addr);
#endif

	si = syminfo();
	si.binary = im->name;
	std::stringstream str = std::stringstream();
	str << "0x" << std::hex << addr;
	si.name = str.str();
	return si;
}

void
pmcview::printvm(pid_t pid)
{
	procinfo &proc = procs[pid];

	printf("%-18s %-18s %s\n", "Low PC", "High PC", "Image");
	for (auto &i : proc.map) {
		printf("0x%08" PRIx64 " 0x%08" PRIx64 " %s\n", i.second.lowpc,
		    i.second.highpc, i.second.image.c_str());
	}
}

