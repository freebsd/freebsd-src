#! /bin/bash
#
# Copyright (c) 2015-2018, Intel Corporation
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
#  * Redistributions of source code must retain the above copyright notice,
#    this list of conditions and the following disclaimer.
#  * Redistributions in binary form must reproduce the above copyright notice,
#    this list of conditions and the following disclaimer in the documentation
#    and/or other materials provided with the distribution.
#  * Neither the name of Intel Corporation nor the names of its contributors
#    may be used to endorse or promote products derived from this software
#    without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
# LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.

set -e

prog=`basename $0`

usage() {
    cat <<EOF
usage: $prog [<options>] <perf.data-file>

Extract the sideband records from a perf data file.

options:
  -h  this text
  -d  print commands, don't execute them

<perf.data-file> defaults to perf.data.
EOF
}

dry_run=0
while getopts "hd" opt; do
    case $opt in
        h)
            usage
            exit 0
            ;;
        d)
            dry_run=1
            ;;
    esac
done

shift $(($OPTIND-1))


if [[ $# == 0 ]]; then
    file="perf.data"
elif [[ $# == 1 ]]; then
    file="$1"
    shift
else
    usage
    exit 1
fi

base="$(basename $file)"

if [[ "$dry_run" == 0 ]]; then
    nofiles=0

    for ofile in $base-sideband-cpu*.pevent $base-sideband.pevent; do
        if [[ -w $ofile ]]; then
            echo "$prog: $ofile is in the way."
            nofiles+=1
        fi
    done

    if [[ "$nofiles" > 0 ]]; then
        exit 1
    fi
fi


perf script --no-itrace -i "$file" -D | gawk -F' ' -- '
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
    ofile = sprintf("%s-sideband.pevent", base)

    handle_record(ofile, offset, size)
  }

  function handle_cpu_record(cpu, offset, size) {
    # (uint32_t) -1 = 4294967295
    #
    if (cpu == -1 || cpu == 4294967295) {
      handle_global_record(offset, size);
    }
    else {
      ofile = sprintf("%s-sideband-cpu%d.pevent", base, cpu)

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
' file="$file" base="$base" dry_run="$dry_run"
