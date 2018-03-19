Capturing Intel(R) Processor Trace (Intel PT) {#capture}
=============================================

<!---
 ! Copyright (c) 2015-2018, Intel Corporation
 !
 ! Redistribution and use in source and binary forms, with or without
 ! modification, are permitted provided that the following conditions are met:
 !
 !  * Redistributions of source code must retain the above copyright notice,
 !    this list of conditions and the following disclaimer.
 !  * Redistributions in binary form must reproduce the above copyright notice,
 !    this list of conditions and the following disclaimer in the documentation
 !    and/or other materials provided with the distribution.
 !  * Neither the name of Intel Corporation nor the names of its contributors
 !    may be used to endorse or promote products derived from this software
 !    without specific prior written permission.
 !
 ! THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 ! AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 ! IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 ! ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 ! LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 ! CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 ! SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 ! INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 ! CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 ! ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 ! POSSIBILITY OF SUCH DAMAGE.
 !-->

This chapter describes how to capture Intel PT for processing with libipt.  For
illustration, we use the sample tools ptdump and ptxed.  We assume that they are
configured with:

  * PEVENT=ON
  * FEATURE_ELF=ON


## Capturing Intel PT on Linux

Starting with version 4.1, the Linux kernel supports Intel PT via the perf_event
kernel interface.  Starting with version 4.3, the perf user-space tool will
support Intel PT as well.


### Capturing Intel PT via Linux perf_event

We start with setting up a perf_event_attr object for capturing Intel PT.  The
structure is declared in `/usr/include/linux/perf_event.h`.

The Intel PT PMU type is dynamic.  Its value can be read from
`/sys/bus/event_source/devices/intel_pt/type`.

~~~{.c}
    struct perf_event_attr attr;

    memset(&attr, 0, sizeof(attr));
    attr.size = sizeof(attr);
    attr.type = <read type>();

    attr.exclude_kernel = 1;
    ...
~~~


Once all desired fields have been set, we can open a perf_event counter for
Intel PT.  See `perf_event_open(2)` for details.  In our example, we configure
it for tracing a single thread.

The system call returns a file descriptor on success, `-1` otherwise.

~~~{.c}
    int fd;

    fd = syscall(SYS_perf_event_open, &attr, <pid>, -1, -1, 0);
~~~


The Intel PT trace is captured in the AUX area, which has been introduced with
kernel 4.1.  The DATA area contains sideband information such as image changes
that are necessary for decoding the trace.

In theory, both areas can be configured as circular buffers or as linear buffers
by mapping them read-only or read-write, respectively.  When configured as
circular buffer, new data will overwrite older data.  When configured as linear
buffer, the user is expected to continuously read out the data and update the
buffer's tail pointer.  New data that do not fit into the buffer will be
dropped.

When using the AUX area, its size and offset have to be filled into the
`perf_event_mmap_page`, which is mapped together with the DATA area.  This
requires the DATA area to be mapped read-write and hence configured as linear
buffer.  In our example, we configure the AUX area as circular buffer.

Note that the size of both the AUX and the DATA area has to be a power of two
pages.  The DATA area needs one additional page to contain the
`perf_event_mmap_page`.

~~~{.c}
    struct perf_event_mmap_page *header;
    void *base, *data, *aux;

    base = mmap(NULL, (1+2**n) * PAGE_SIZE, PROT_WRITE, MAP_SHARED, fd, 0);
    if (base == MAP_FAILED)
        return <handle data mmap error>();

    header = base;
    data = base + header->data_offset;

    header->aux_offset = header->data_offset + header->data_size;
    header->aux_size   = (2**m) * PAGE_SIZE;

    aux = mmap(NULL, header->aux_size, PROT_READ, MAP_SHARED, fd,
               header->aux_offset);
    if (aux == MAP_FAILED)
        return <handle aux mmap error>();
~~~


### Capturing Intel PT via the perf user-space tool

Starting with kernel 4.3, the perf user-space tool can be used to capture Intel
PT with the `intel_pt` event.  See tools/perf/Documentation in the Linux kernel
tree for further information.  In this text, we describe how to use the captured
trace with the ptdump and ptxed sample tools.

We start with capturing some Intel PT trace using the `intel_pt` event.  Note
that when collecting system-wide (`-a`) trace, we need context switch events
(`--switch-events`) to decode the trace.  See `perf-record(1)` for details.

~~~{.sh}
    $ perf record -e intel_pt//[uk] [--per-thread] [-a --switch-events] -T -- ls
    [ perf record: Woken up 1 times to write data ]
    [ perf record: Captured and wrote 0.384 MB perf.data ]
~~~


This generates a file called `perf.data` that contains the Intel PT trace, the
sideband information, and some metadata.  To process the trace with ptxed, we
extract the Intel PT trace into one file per thread or cpu.

Looking at the raw trace dump of `perf script -D`, we notice
`PERF_RECORD_AUXTRACE` records.  The raw Intel PT trace is contained directly
after such records.  We can extract it with the `dd` command.  The arguments to
`dd` can be computed from the record's fields.  This can be done automatically,
for example with an AWK script.

~~~{.awk}
  /PERF_RECORD_AUXTRACE / {
    offset = strtonum($1)
    hsize  = strtonum(substr($2, 2))
    size   = strtonum($5)
    idx    = strtonum($11)

    ofile = sprintf("perf.data-aux-idx%d.bin", idx)
    begin = offset + hsize

    cmd = sprintf("dd if=perf.data of=%s conv=notrunc oflag=append ibs=1 \
                  skip=%d count=%d status=none", ofile, begin, size)

    system(cmd)
  }
~~~

The libipt tree contains such a script in `script/perf-read-aux.bash`.

If we recorded in snapshot mode (perf record -S), we need to extract the Intel
PT trace into one file per `PERF_RECORD_AUXTRACE` record.  This can be done with
an AWK script similar to the one above.  Use `script/perf-read-aux.bash -S` when
using the script from the libipt tree.


In addition to the Intel PT trace, we need sideband information that describes
process creation and termination, context switches, and memory image changes.
This sideband information needs to be processed together with the trace.  We
therefore extract the sideband information from `perf.data`.  This can again be
done automatically with an AWK script:

~~~{.awk}
  function handle_record(ofile, offset, size) {
    cmd = sprintf("dd if=%s of=%s conv=notrunc oflag=append ibs=1 skip=%d " \
                  "count=%d status=none", file, ofile, offset, size)

    if (dry_run != 0) {
      print cmd
    }
    else {
      system(cmd)
    }

    next
  }

  function handle_global_record(offset, size) {
    ofile = sprintf("%s-sideband.pevent", file)

    handle_record(ofile, offset, size)
  }

  function handle_cpu_record(cpu, offset, size) {
    # (uint32_t) -1 = 4294967295
    #
    if (cpu == -1 || cpu == 4294967295) {
      handle_global_record(offset, size);
    }
    else {
      ofile = sprintf("%s-sideband-cpu%d.pevent", file, cpu)

      handle_record(ofile, offset, size)
    }
  }

  /PERF_RECORD_AUXTRACE_INFO/  { next }
  /PERF_RECORD_AUXTRACE/       { next }
  /PERF_RECORD_FINISHED_ROUND/ { next }

  /^[0-9]+ [0-9]+ 0x[0-9a-f]+ \[0x[0-9a-f]+\]: PERF_RECORD_/ {
    cpu   = strtonum($1)
    begin = strtonum($3)
    size  = strtonum(substr($4, 2))

    handle_cpu_record(cpu, begin, size)
  }

  /^[0-9]+ 0x[0-9a-f]+ \[0x[0-9a-f]+\]: PERF_RECORD_/ {
    begin = strtonum($2)
    size  = strtonum(substr($3, 2))

    handle_global_record(begin, size)
  }

  /^0x[0-9a-f]+ \[0x[0-9a-f]+\]: PERF_RECORD_/ {
    begin = strtonum($1)
    size  = strtonum(substr($2, 2))

    handle_global_record(begin, size)
  }
~~~

The libipt tree contains such a script in `script/perf-read-sideband.bash`.


In Linux, sideband is implemented as a sequence of perf_event records.  Each
record can optionally be followed by one or more samples that specify the cpu on
which the record was created or a timestamp that specifies when the record was
created.  We use the timestamp sample to correlate sideband and trace.

To process those samples, we need to know exactly what was sampled so that we
can find the timestamp sample we are interested in.  This information can be
found in the `sample_type` field of `struct perf_event_attr`.  We can extract
this information from `perf.data` using the `perf evlist` command:

~~~{.sh}
    $ perf evlist -v
    intel_pt//u: [...] sample_type: IP|TID|TIME|CPU|IDENTIFIER [...]
    dummy:u: [...] sample_type: IP|TID|TIME|IDENTIFIER [...]
~~~


The command lists two items, one for the `intel_pt` perf_event counter and one
for a `dummy` counter that is used for capturing context switch events.

We translate the sample_type string using `PERF_EVENT_SAMPLE_*` enumeration
constants defined in `/usr/include/linux/perf_event.h` into a single 64-bit
integer constant.  For example, `IP|TID|TIME|CPU|IDENTIFIER` translates into
`0x10086`.  Note that the `IP` sample type is reported but will not be attached
to perf_event records.  The resulting constant is then supplied as argument to
the ptdump and ptxed option:

 * --pevent:sample-type


The translation can be done automatically using an AWK script, assuming that we
already extracted the samle_type string:

~~~{.awk}
  BEGIN         { RS = "[|\n]" }
  /^TID$/        { config += 0x00002 }
  /^TIME$/       { config += 0x00004 }
  /^ID$/         { config += 0x00040 }
  /^CPU$/        { config += 0x00080 }
  /^STREAM$/     { config += 0x00200 }
  /^IDENTIFIER$/ { config += 0x10000 }
  END           {
    if (config != 0) {
      printf(" --pevent:sample_type 0x%x", config)
    }
  }
~~~


Sideband and trace are time-correlated.  Since Intel PT and perf use different
time domains, we need a few parameters to translate between the two domains.
The parameters can be found in `struct perf_event_mmap_page`, which is declared
in `/usr/include/linux/perf_event.h`:

 * time_shift
 * time_mult
 * time_zero

The header also documents how to calculate TSC from perf_event timestamps.

The ptdump and ptxed sample tools do this translation but we need to supply the
parameters via corresponding options:

 * --pevent:time-shift
 * --pevent:time-mult
 * --pevent:time-zero

We can extract this information from the PERF_RECORD_AUXTRACE_INFO record.  This
is an artificial record that the perf tool synthesizes when capturing the trace.
We can view it using the `perf script` command:

~~~{.sh}
    $ perf script --no-itrace -D | grep -A14 PERF_RECORD_AUXTRACE_INFO
    0x1a8 [0x88]: PERF_RECORD_AUXTRACE_INFO type: 1
      PMU Type            6
      Time Shift          10
      Time Muliplier      642
      Time Zero           18446744056970350213
      Cap Time Zero       1
      TSC bit             0x400
      NoRETComp bit       0x800
      Have sched_switch   0
      Snapshot mode       0
      Per-cpu maps        1
      MTC bit             0x200
      TSC:CTC numerator   0
      TSC:CTC denominator 0
      CYC bit             0x2
~~~


This will also give us the values for `cpuid[0x15].eax` and `cpuid[0x15].ebx`
that we need for tracking time with `MTC` and `CYC` packets in `TSC:CTC
denominator` and `TSC:CTC numerator` respectively.  On processors that do not
support `MTC` and `CYC`, the values are reported as zero.

When decoding system-wide trace, we need to correlate context switch sideband
events with decoded instructions from the trace to find a suitable location for
switching the traced memory image for the scheduled-in process.  The heuristics
we use rely on sufficiently precise timing information.  If timing information
is too coarse, we might map the contex switch to the wrong location.

When tracing ring-0, we use any code in kernel space.  Since the kernel is
mapped into every process, this is good enough as long as we are not interested
in identifying processes and threads in the trace.  To allow ptxed to
distinguish kernel from user addresses, we provide the start address of the
kernel via the option:

  * --pevent:kernel-start


We can find the address in `kallsyms` and we can extract it automatically using
an AWK script:

~~~{.awk}
    function update_kernel_start(vaddr) {
      if (vaddr < kernel_start) {
        kernel_start = vaddr
      }
    }

    BEGIN                       { kernel_start = 0xffffffffffffffff }
    /^[0-9a-f]+ T _text$/       { update_kernel_start(strtonum("0x" $1)) }
    /^[0-9a-f]+ T _stext$/      { update_kernel_start(strtonum("0x" $1)) }
    END {
      if (kernel_start < 0xffffffffffffffff) {
        printf(" --pevent:kernel-start 0x%x", kernel_start)
      }
    }
~~~


When not tracing ring-0, we use a region where tracing has been disabled
assuming that tracing is disabled due to a ring transition.


To apply processor errata we need to know on which processor the trace was
collected and provide this information to ptxed using the

  * --cpu

option.  We can find this information in the `perf.data` header using the `perf
script --header-only` command:

~~~{.sh}
    $ perf script --header-only | grep cpuid
    # cpuid : GenuineIntel,6,61,4
~~~


The libipt tree contains a script in `script/perf-get-opts.bash` that computes
all the perf_event related options from `perf.data` and from previously
extracted sideband information.


The kernel uses special filenames in `PERF_RECORD_MMAP` and `PERF_RECORD_MMAP2`
records to indicate pseudo-files that can not be found directly on disk.  One
such special filename is

  * [vdso]

which corresponds to the virtual dynamic shared object that is mapped into every
process.  See `vdso(7)` for details.  Depending on the installation there may be
different vdso flavors.  We need to specify the location of each flavor that is
referenced in the trace via corresponding options:

  * --pevent:vdso-x64
  * --pevent:vdso-x32
  * --pevent:vdso-ia32

The perf tool installation may provide utilities called:

  * perf-read-vdso32
  * perf-read-vdsox32

for reading the ia32 and the x32 vdso flavors.  If the native flavor is not
specified or the specified file does not exist, ptxed will copy its own vdso
into a temporary file and use that.  This may not work for remote decode, nor
can ptxed provide other vdso flavors.


Let's put it all together.  Note that we use the `-m` option of
`script/perf-get-opts.bash` to specify the master sideband file for the cpu on
which we want to decode the trace.  We further enable tick events for finer
grain sideband correlation.

~~~{.sh}
    $ perf record -e intel_pt//u -T --switch-events -- grep -r foo /usr/include
    [ perf record: Woken up 18 times to write data ]
    [ perf record: Captured and wrote 30.240 MB perf.data ]
    $ script/perf-read-aux.bash
    $ script/perf-read-sideband.bash
    $ ptdump $(script/perf-get-opts.bash) perf.data-aux-idx0.bin
    [...]
    $ ptxed $(script/perf-get-opts.bash -m perf.data-sideband-cpu0.pevent)
        --pevent:vdso... --event:tick --pt perf.data-aux-idx0.bin
    [...]
~~~


When tracing ring-0 code, we need to use `perf-with-kcore` for recording and
supply the `perf.data` directory as additional argument after the `record` perf
sub-command.  When `perf-with-kcore` completes, the `perf.data` directory
contains `perf.data` as well as a directory `kcore_dir` that contains copies of
`/proc/kcore` and `/proc/kallsyms`.  We need to supply the path to `kcore_dir`
to `script/perf-get-opts.bash` using the `-k` option.

~~~{.sh}
    $ perf-with-kcore record dir -e intel_pt// -T -a --switch-events -- sleep 10
    [ perf record: Woken up 26 times to write data ]
    [ perf record: Captured and wrote 54.238 MB perf.data ]
    Copying kcore
    Done
    $ cd dir
    $ script/perf-read-aux.bash
    $ script/perf-read-sideband.bash
    $ ptdump $(script/perf-get-opts.bash) perf.data-aux-idx0.bin
    [...]
    $ ptxed $(script/perf-get-opts.bash -k kcore_dir
                -m perf.data-sideband-cpu0.pevent)
        --pevent:vdso... --event:tick --pt perf.data-aux-idx0.bin
    [...]
~~~


#### Remote decode

To decode the recorded trace on a different system, we copy all the files
referenced in the trace to the system on which the trace is being decoded and
point ptxed to the respective root directory using the option:

  * --pevent:sysroot


Ptxed will prepend the sysroot directory to every filename referenced in
`PERF_RECORD_MMAP` and `PERF_RECORD_MMAP2` records.

Note that like most configuration options, the `--pevent.sysroot` option needs
to precede `--pevent:primary` and `-pevent:secondary` options.


We can extract the referenced file names from `PERF_RECORD_MMAP` and
`PERF_RECORD_MMAP2` records in the output of `perf script -D` and we can
automatically copy the files using an AWK script:

~~~{.awk}
    function dirname(file) {
        items = split(file, parts, "/", seps)

        delete parts[items]

        dname = ""
        for (part in parts) {
            dname = dname seps[part-1] parts[part]
        }

        return dname
    }

    function handle_mmap(file) {
        # ignore any non-absolute filename
        #
        # this covers pseudo-files like [kallsyms] or [vdso]
        #
        if (substr(file, 0, 1) != "/") {
            return
        }

        # ignore kernel modules
        #
        # we rely on kcore
        #
        if (match(file, /\.ko$/) != 0) {
            return
        }

        # ignore //anon
        #
        if (file == "//anon") {
            return
        }

        dst = outdir file
        dir = dirname(dst)

        system("mkdir -p " dir)
        system("cp " file " " dst)
    }

    /PERF_RECORD_MMAP/     { handle_mmap($NF) }
~~~

The libipt tree contains such a script in `script/perf-copy-mapped-files.bash`.
It will also read the vdso flavors for which the perf installation provides
readers.

We use the `-s` option of `script/perf-get-opts.bash` to have it generate
options for the sysroot directory and for the vdso flavors found in that
sysroot.

For the remote decode case, we thus get (assuming kernel and user tracing on a
64-bit system):

~~~{.sh}
    [record]
    $ perf-with-kcore record dir -e intel_pt// -T -a --switch-events -- sleep 10
    [ perf record: Woken up 26 times to write data ]
    [ perf record: Captured and wrote 54.238 MB perf.data ]
    Copying kcore
    Done
    $ cd dir
    $ script/perf-copy-mapped-files.bash -o sysroot

    [copy dir to remote system]

    [decode]
    $ script/perf-read-aux.bash
    $ script/perf-read-sideband.bash
    $ ptdump $(script/perf-get-opts.bash -s sysroot) perf.data-aux-idx0.bin
    [...]
    $ ptxed $(script/perf-get-opts.bash -s sysroot -k kcore_dir
                -m perf.data-sideband-cpu0.pevent)
        --event:tick --pt perf.data-aux-idx0.bin
    [...]
~~~


#### Troubleshooting

##### Sideband correlation and `no memory mapped at this address` errors

If timing information in the trace is too coarse, we may end up applying
sideband events too late.  This typically results in `no memory mapped at this
address` errors.

Try to increase timing precision by increasing the MTC frequency or by enabling
cycle-accurate tracing.  If this does not help or is not an option, ptxed can
process sideband events earlier than timing information indicates.  Supply a
suitable value to ptxed's option:

  * --pevent:tsc-offset


This option adds its argument to the timing information in the trace and so
causes sideband events to be processed earlier.  There is logic in ptxed to
determine a suitable location in the trace for applying some sideband events.
For example, a context switch event is postponed until tracing is disabled or
enters the kernel.

Those heuristics have their limits, of course.  If the tsc offset is chosen too
big, ptxed may end up mapping a sideband event to the wrong kernel entry.


##### Sideband and trace losses leading to decode errors

The perf tool reads trace and sideband while it is being collected and stores it
in `perf.data`.  If it fails to keep up, perf_event records or trace may be
lost.  The losses are indicated in the sideband:

 * `PERF_RECORD_LOST`            indicates sideband losses
 * `PERF_RECORD_AUX.TRUNCATED`   indicates trace losses


Sideband losses may go unnoticed or may lead to decode errors.  Typical errors
are:

 * `no memory mapped at this address`
 * `decoder out of sync`
 * `trace stream does not match query`


Ptxed diagnoses sideband losses as warning both to stderr and to stdout
interleaved with the normal output.

Trace losses may go unnoticed or may lead to all kinds of errors.  Ptxed
diagnoses trace losses as warning to stderr.


### Capturing Intel PT via Simple-PT

The Simple-PT project on github supports capturing Intel PT on Linux with an
alternative kernel driver.  The spt decoder supports sideband information.

See the project's page at https://github.com/andikleen/simple-pt for more
information including examples.
