# Copyright (c) 2025 Ronald Klop <ronald@FreeBSD.org>
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#
#

atf_test_case attach_mdX cleanup
attach_mdX_head()
{
	atf_set "descr" "mdX device should attach"
}
attach_mdX_body()
{
	# if the swapfile is too small (like 1k) then mdconfig hangs looking up the md
	# but need a swapfile bigger than one page kernel page size
	pagesize=$(sysctl -n hw.pagesize)
	minsize=$(( pagesize * 2 ))
	atf_check -s exit:0 -x "truncate -s $minsize swapfile"
	atf_check -s exit:0 -o save:fstab.out -x "echo 'md31    none    swap    sw,file=swapfile  0       0'"
	atf_check -s exit:0 -o match:"swapon: adding /dev/md31 as swap device" -x "swapon -F fstab.out -a"
}
attach_mdX_cleanup()
{
	swapoff -F fstab.out -a
}

###
atf_test_case attach_dev_mdX cleanup
attach_dev_mdX_head()
{
	atf_set "descr" "/dev/mdX device should attach"
}
attach_dev_mdX_body()
{
	# if the swapfile is too small (like 1k) then mdconfig hangs looking up the md
	# but need a swapfile bigger than one page kernel page size
	pagesize=$(sysctl -n hw.pagesize)
	minsize=$(( pagesize * 2 ))
	atf_check -s exit:0 -x "truncate -s $minsize swapfile"
	atf_check -s exit:0 -o save:fstab.out -x "echo '/dev/md32    none    swap    sw,file=swapfile  0       0'"
	atf_check -s exit:0 -o match:"swapon: adding /dev/md32 as swap device" -x "swapon -F fstab.out -a"
}
attach_dev_mdX_cleanup()
{
	swapoff -F fstab.out -a
}

###
atf_test_case attach_md cleanup
attach_md_head()
{
	atf_set "descr" "md device should attach"
}
attach_md_body()
{
	# if the swapfile is too small (like 1k) then mdconfig hangs looking up the md
	# but need a swapfile bigger than one page kernel page size
	pagesize=$(sysctl -n hw.pagesize)
	minsize=$(( pagesize * 2 ))
	atf_check -s exit:0 -x "truncate -s $minsize swapfile"
	atf_check -s exit:0 -o save:fstab.out -x "echo 'md    none    swap    sw,file=swapfile  0       0'"
	atf_check -s exit:0 -o match:"swapon: adding /dev/md[0-9][0-9]* as swap device" -x "swapon -F fstab.out -a"
}
attach_md_cleanup()
{
	swapoff -F fstab.out -a
}

###
atf_test_case attach_dev_md cleanup
attach_dev_md_head()
{
	atf_set "descr" "/dev/md device should attach"
}
attach_dev_md_body()
{
	# if the swapfile is too small (like 1k) then mdconfig hangs looking up the md
	# but need a swapfile bigger than one page kernel page size
	pagesize=$(sysctl -n hw.pagesize)
	minsize=$(( pagesize * 2 ))
	atf_check -s exit:0 -x "truncate -s $minsize swapfile"
	atf_check -s exit:0 -o save:fstab.out -x "echo '/dev/md    none    swap    sw,file=swapfile  0       0'"
	atf_check -s exit:0 -o match:"swapon: adding /dev/md[0-9][0-9]* as swap device" -x "swapon -F fstab.out -a"
}
attach_dev_md_cleanup()
{
	swapoff -F fstab.out -a
}

###
atf_test_case attach_mdX_eli cleanup
attach_mdX_eli_head()
{
	atf_set "descr" "mdX.eli device should attach"
}
attach_mdX_eli_body()
{
	# if the swapfile is too small (like 1k) then mdconfig hangs looking up the md
	# but need a swapfile bigger than one page kernel page size
	pagesize=$(sysctl -n hw.pagesize)
	minsize=$(( pagesize * 2 ))
	atf_check -s exit:0 -x "truncate -s $minsize swapfile"
	atf_check -s exit:0 -o save:fstab.out -x "echo 'md33.eli    none    swap    sw,file=swapfile  0       0'"
	atf_check -s exit:0 -o match:"swapon: adding /dev/md33.eli as swap device" -x "swapon -F fstab.out -a"
}
attach_mdX_eli_cleanup()
{
	swapoff -F fstab.out -a
}

###
atf_test_case attach_dev_mdX_eli cleanup
attach_dev_mdX_eli_head()
{
	atf_set "descr" "/dev/mdX.eli device should attach"
}
attach_dev_mdX_eli_body()
{
	# if the swapfile is too small (like 1k) then mdconfig hangs looking up the md
	# but need a swapfile bigger than one page kernel page size
	pagesize=$(sysctl -n hw.pagesize)
	minsize=$(( pagesize * 2 ))
	atf_check -s exit:0 -x "truncate -s $minsize swapfile"
	atf_check -s exit:0 -o save:fstab.out -x "echo '/dev/md34.eli    none    swap    sw,file=swapfile  0       0'"
	atf_check -s exit:0 -o match:"swapon: adding /dev/md34.eli as swap device" -x "swapon -F fstab.out -a"
}
attach_dev_mdX_eli_cleanup()
{
	swapoff -F fstab.out -a
}

###
atf_test_case attach_md_eli cleanup
attach_md_eli_head()
{
	atf_set "descr" "md.eli device should attach"
}
attach_md_eli_body()
{
	# if the swapfile is too small (like 1k) then mdconfig hangs looking up the md
	# but need a swapfile bigger than one page kernel page size
	pagesize=$(sysctl -n hw.pagesize)
	minsize=$(( pagesize * 2 ))
	atf_check -s exit:0 -x "truncate -s $minsize swapfile"
	atf_check -s exit:0 -o save:fstab.out -x "echo 'md.eli    none    swap    sw,file=swapfile  0       0'"
	atf_check -s exit:0 -o match:"swapon: adding /dev/md[0-9][0-9]*.eli as swap device" -x "swapon -F fstab.out -a"
}
attach_md_eli_cleanup()
{
	swapoff -F fstab.out -a
}

###
atf_test_case attach_dev_md_eli cleanup
attach_dev_md_eli_head()
{
	atf_set "descr" "/dev/md.eli device should attach"
}
attach_dev_md_eli_body()
{
	# if the swapfile is too small (like 1k) then mdconfig hangs looking up the md
	# but need a swapfile bigger than one page kernel page size
	pagesize=$(sysctl -n hw.pagesize)
	minsize=$(( pagesize * 2 ))
	atf_check -s exit:0 -x "truncate -s $minsize swapfile"
	atf_check -s exit:0 -o save:fstab.out -x "echo '/dev/md.eli    none    swap    sw,file=swapfile  0       0'"
	atf_check -s exit:0 -o match:"swapon: adding /dev/md[0-9][0-9]*.eli as swap device" -x "swapon -F fstab.out -a"
}
attach_dev_md_eli_cleanup()
{
	swapoff -F fstab.out -a
}

###

atf_test_case attach_too_small
attach_too_small_head()
{
	atf_set "descr" "should refuse to attach if smaller than one kernel page size"
}
attach_too_small_body()
{
	# Need to use smaller than kernel page size
	pagesize=$(sysctl -n hw.pagesize)
	minsize=$(( pagesize / 2 ))
	atf_check -s exit:0 -x "truncate -s $minsize swapfile"
	atf_check -s exit:0 -o save:fstab.out -x "echo 'md35    none    swap    sw,file=swapfile  0       0'"
	atf_check -s exit:1 -e match:"swapon: /dev/md35: NSWAPDEV limit reached" -x "swapon -F fstab.out -a"
	atf_check -s exit:0 -x "mdconfig -d -u 35"
}

###
atf_init_test_cases()
{
	atf_add_test_case attach_mdX
	atf_add_test_case attach_dev_mdX
	atf_add_test_case attach_md
	atf_add_test_case attach_dev_md

	atf_add_test_case attach_mdX_eli
	atf_add_test_case attach_dev_mdX_eli
	atf_add_test_case attach_md_eli
	atf_add_test_case attach_dev_md_eli

	atf_add_test_case attach_too_small
}
