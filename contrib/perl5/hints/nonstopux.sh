# tom_bates@att.net
# mips-compaq-nonstopux

. $src/hints/svr4.sh

case "$cc" in
        *gcc*)
                ccflags='-fno-strict-aliasing'
                lddlflags='-shared'
                ldflags=''
		;;
        *)
                cc="cc -Xa -Olimit 4096"
                malloctype="void *"
		;;
esac

