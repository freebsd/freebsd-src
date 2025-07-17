#!/bin/sh
#
#

# PROVIDE: nfsd
# REQUIRE: mountcritremote mountd hostname gssd nfsuserd
# KEYWORD: nojailvnet shutdown

. /etc/rc.subr

name="nfsd"
desc="Remote NFS server"
rcvar="nfs_server_enable"
command="/usr/sbin/${name}"
nfs_server_vhost=""

: ${nfsd_svcj_options:="net_basic nfsd"}

load_rc_config $name
# precmd is not compatible with svcj
nfsd_svcj="NO"
start_precmd="nfsd_precmd"
sig_stop="USR1"

nfsd_precmd()
{
	local	_vhost
	rc_flags="${nfs_server_flags}"

	# Load the modules now, so that the vfs.nfsd sysctl
	# oids are available.
	load_kld nfsd || return 1

	if [ -n "${nfs_server_maxio}" ] && ! check_jail jailed; then
		if ! sysctl vfs.nfsd.srvmaxio=${nfs_server_maxio} >/dev/null; then
			warn "Failed to set server max I/O"
		fi
	fi

	if checkyesno nfs_reserved_port_only; then
		echo 'NFS on reserved port only=YES'
		sysctl vfs.nfsd.nfs_privport=1 > /dev/null
	else
		sysctl vfs.nfsd.nfs_privport=0 > /dev/null
	fi

	if checkyesno nfs_server_managegids; then
		force_depend nfsuserd || err 1 "Cannot run nfsuserd"
	fi

	if checkyesno nfsv4_server_enable; then
		sysctl vfs.nfsd.server_max_nfsvers=4 > /dev/null
	elif ! checkyesno nfsv4_server_only; then
		echo 'NFSv4 is disabled'
		sysctl vfs.nfsd.server_max_nfsvers=3 > /dev/null
	fi

	if ! checkyesno nfsv4_server_only; then
		force_depend rpcbind || return 1
	fi

	force_depend mountd || return 1
	if [ -n "${nfs_server_vhost}" ]; then
		command_args="-V \"${nfs_server_vhost}\""
	fi
}

run_rc_command "$1"
