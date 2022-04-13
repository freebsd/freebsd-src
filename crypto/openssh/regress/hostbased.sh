#	$OpenBSD: hostbased.sh,v 1.3 2022/01/08 07:55:26 dtucker Exp $
#	Placed in the Public Domain.

# This test requires external setup and thus is skipped unless
# TEST_SSH_HOSTBASED_AUTH and SUDO are set to "yes".
# Since ssh-keysign has key paths hard coded, unlike the other tests it
# needs to use the real host keys. It requires:
# - ssh-keysign must be installed and setuid.
# - "EnableSSHKeysign yes" must be in the system ssh_config.
# - the system's own real FQDN the system-wide shosts.equiv.
# - the system's real public key fingerprints must me in global ssh_known_hosts.
#
tid="hostbased"

if [ -z "${TEST_SSH_HOSTBASED_AUTH}" ]; then
	skip "TEST_SSH_HOSTBASED_AUTH not set."
elif [ -z "${SUDO}" ]; then
	skip "SUDO not set"
fi

# Enable all supported hostkey algos (but no others)
hostkeyalgos=`${SSH} -Q HostKeyAlgorithms | tr '\n' , | sed 's/,$//'`

cat >>$OBJ/sshd_proxy <<EOD
HostbasedAuthentication yes
HostbasedAcceptedAlgorithms $hostkeyalgos
HostbasedUsesNameFromPacketOnly yes
HostKeyAlgorithms $hostkeyalgos
EOD

cat >>$OBJ/ssh_proxy <<EOD
HostbasedAuthentication yes
HostKeyAlgorithms $hostkeyalgos
HostbasedAcceptedAlgorithms $hostkeyalgos
PreferredAuthentications hostbased
EOD

algos=""
for key in `${SUDO} ${SSHD} -T | awk '$1=="hostkey"{print $2}'`; do
	case "`$SSHKEYGEN -l -f ${key}.pub`" in
	256*ECDSA*)	algos="$algos ecdsa-sha2-nistp256" ;;
	384*ECDSA*)	algos="$algos ecdsa-sha2-nistp384" ;;
	521*ECDSA*)	algos="$algos ecdsa-sha2-nistp521" ;;
	*RSA*)		algos="$algos ssh-rsa rsa-sha2-256 rsa-sha2-512" ;;
	*ED25519*)	algos="$algos ssh-ed25519" ;;
	*DSA*)		algos="$algos ssh-dss" ;;
	*) verbose "unknown host key type $key" ;;
	esac
done

for algo in $algos; do
	trace "hostbased algo $algo"
	opts="-F $OBJ/ssh_proxy"
	if [ "x$algo" != "xdefault" ]; then
		opts="$opts -oHostbasedAcceptedAlgorithms=$algo"
	fi
	SSH_CONNECTION=`${SSH} $opts localhost 'echo $SSH_CONNECTION'`
	if [ $? -ne 0 ]; then
		fail "connect failed, hostbased algo $algo"
	elif [ "$SSH_CONNECTION" != "UNKNOWN 65535 UNKNOWN 65535" ]; then
		fail "hostbased algo $algo bad SSH_CONNECTION" \
		    "$SSH_CONNECTION"
	else
		verbose "ok hostbased algo $algo"
	fi
done
