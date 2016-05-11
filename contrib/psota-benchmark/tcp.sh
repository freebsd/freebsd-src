#!/bin/sh
# tar comparison program
# 2007-10-25 Jan Psota

n=3                                     # number of repetitions
TAR="bsdtar gnutar star"                # Tape archivers to compare
OPT=("" "--seek" "-no-fsync")
pax="--format=pax"                      # comment out for defaults
OPN=(create list extract compare)       # operations
version="2007-10-25"
TIMEFORMAT=$'%R\t%U\t%S\t%P'
LC_ALL=C

test $# -ge 2 || {
        echo -e "usage:\t$0 source_dir where_to_place_archive 
[where_to_extract_it]

TCP, version $version
TCP stands for Tar Comparison Program here.
It currently compares: BSD tar (bsdtar), GNU tar (gnutar) and star in archive
creation, listing, extraction and archive-to-extracted comparison.
Tcp prints out best time of n=$n repetitions.

Tcp creates temporary archive named tcp.tar with $pax and some native
(--seek/-no-fsync) options and extracts it to [\$3]/tcptmp/.
If unset, third argument defaults to [\$2].
After normal exit tcp removes tarball and extracted files.
Tcp does not check filesystems destination directories are on for free space,
so make sure there is enough space (a bit more than source_dir uses) for both:
archive and extracted files.
Do not use white space in arguments.
        Jan Psota, $version"
        exit 0
}
src=$1
dst=$2/tcp.tar
dst_path=${3:-$2}/tcptmp
test -e $dst -o -e /tmp/tcp \
        && { echo "$dst or /tmp/tcp exists, exiting"; exit 1; }
mkdir $dst_path || exit 2

use_times ()
{
        awk -F"\t" -vN=$n -vL="`du -k $dst`" -vOFS="\t" -vORS="" '
                { if (NF==4) { printf "\t%s\t%10.1d KB/s\n", $0, ($1+0>0 ? 
(L+0)/($1+0) : 0) } }' \
                /tmp/tcp | sort | head -1
        > /tmp/tcp
}

test -d $src || { echo "'$src' is not a directory"; exit 3; }

# system information: type, release, memory, cpu(s), compiler and flags
echo -e "TCP, version $version\n"`uname -sr`" / "`head -1 /etc/*-release`
free -m | awk '/^Mem/ { printf "%dMB of memory, ", $2 }'
test -e /proc/cpuinfo \
        && awk -F: '/name|cache size|MHz|mips/ { if (!a) b=b $2 }
        /^$/ { a++ } END { print a" x"b" bmips" }' /proc/cpuinfo
test -e /etc/gentoo-release \
        && gcc --version | head -1 && grep ^CFLAGS /etc/make.conf

# tar versions
t=
echo
for tar in $TAR; do 
	if which $tar &> /dev/null; then
		t="$t $tar";
		echo -ne "$tar:\t"; $tar --version | head -1; 
	fi
done

TAR="$t"

echo -e "\nbest time of $n repetitions,\n"\
"       src=$src, "\
`du -sh $src | awk '{print $1}'`" in "`find $src | wc -l`" files, "\
"avg "$((`du -sk $src | awk '{print $1}'`/`find $src -type f | wc -l`))"KB/file,\n"\
"       archive=$dst, extract to $dst_path"

echo -e "program\toperation\treal\tuser\tsystem\t%CPU\t     speed"
> /tmp/tcp
let op_num=0
for op in "cf $dst $pax -C $src ." "tf $dst" "xf $dst -C $dst_path" \
        "f $dst -C $dst_path --diff"; do
        let tar_num=0
        for tar in $TAR; do
                echo -en "$tar\t${OPN[op_num]}\t"
                for ((i=1; i<=$n; i++)); do
                        echo $op | grep -q ^cf && rm -f $dst
                        echo $op | grep -q ^xf &&
                                { chmod -R u+w $dst_path
                                rm -rf $dst_path; mkdir $dst_path; }
                        sync
                        if echo $op | grep -q ^f; then  # op == compare
                                time $tar $op ${OPT[$tar_num]} > /dev/null
                        else    # op in (create | list | extract)
                                time $tar $op ${OPT[$tar_num]} > /dev/null \
                                        || break 3
                        fi 2>> /tmp/tcp
                done
                use_times
                let tar_num++
        done
        let op_num++
        echo
done
rm -rf $dst_path $dst
echo
cat /tmp/tcp
rm -f /tmp/tcp
