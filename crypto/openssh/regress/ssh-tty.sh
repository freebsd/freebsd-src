#	$OpenBSD: ssh-tty.sh,v 1.8 2025/10/23 06:15:26 dtucker Exp $
#	Placed in the Public Domain.

# Basic TTY smoke test

tid="ssh-tty"

# Fake home directory to avoid user shell configuration.
FAKEHOME="$OBJ/.fakehome"
rm -rf "$FAKEHOME"
mkdir -m 0700 -p "$FAKEHOME"

case "${PATH}${HOME}" in
*\ *|*\t*) skip "\$PATH or \$HOME has whitespace, not supported in this test";;
esac

# tmux stuff
TMUX=${TMUX:-tmux}
type $TMUX >/dev/null || skip "tmux not found"

if $TMUX -V >/dev/null 2>&1; then
	tver="`$TMUX -V 2>&1`"
	echo "tmux version $tver"
else
	skip "tmux version not reported"
fi

CLEANENV="env -i HOME=$HOME LOGNAME=$USER USER=$USER PATH=$PATH SHELL=$SHELL"
TMUX_TEST="$CLEANENV $TMUX -f/dev/null -Lopenssh-regress-ssh-tty"
sess="regress-ssh-tty$$"

# Multiplexing control socket.
CTL=$OBJ/ctl-sock

# Some randomish strings used for signalling back and forth.
# We use the octal variants via printf(1).
MAGIC1="XY23zzY"
MAGIC1_OCTAL="\130\131\062\063\172\172\131"
MAGIC2="99sMarT86"
MAGIC2_OCTAL="\071\071\163\115\141\162\124\070\066"
MAGIC3="woLF1701d"
MAGIC3_OCTAL="\167\157\114\106\061\067\060\061\144"
MAGIC4="lUh4thX4evR"
MAGIC4_OCTAL="\154\125\150\064\164\150\130\064\145\166\122"
MAGIC5="AllMo1000x"
MAGIC5_OCTAL="\101\154\154\115\157\061\060\060\060\170"

# Wait for a mux process to become ready.
wait_for_mux_ready()
{
	for i in 1 2 3 4 5 6 7 8 9; do
		${SSH} -F $OBJ/ssh_config -S $CTL -Ocheck otherhost \
		    >/dev/null 2>&1 && return 0
		sleep $i
	done
	fatal "mux never becomes ready"
}

# Wait for a mux process to have finished.
wait_for_mux_done()
{
	for i in 1 2 3 4 5 6 7 8 9; do
		test -S $CTL || return 0
		sleep $i
	done
	fatal "mux socket never removed"
}

# Wait for a regex to appear in terminal output.
wait_for_regex() {
	string="$1"
	errors_are_fatal="$2"
	for x in 1 2 3 4 5 6 7 8 9 10 ; do
		$TMUX_TEST capture-pane -pt $sess | grep "$string" >/dev/null
		[ $? -eq 0 ] && return
		sleep 1
	done
	if test -z "$errors_are_fatal"; then
		fail "failed to match \"$string\" in terminal output"
		return
	fi
	fatal "failed to match \"$string\" in terminal output"
}

# Check that a regex does *not* appear in terminal output
not_in_term() {
	string="$1"
	error="$2"
	errors_are_fatal="$3"
	$TMUX_TEST capture-pane -pt $sess | grep "$string" > /dev/null
	[ $? -ne 0 ] && return
	if test -z "$errors_are_fatal"; then
		fail "$error"
		return
	fi
	fatal "$error"
}

# Shut down tmux session and Wait for it to terminate.
kill_tmux() {
	$TMUX_TEST kill-session -t $sess 2>/dev/null
	for x in 1 2 3 4 5 6 7 8 9 10; do
		$TMUX_TEST has-session -t $sess >/dev/null 2>&1 || return
		sleep 1
	done
	fatal "tmux session didn't terminate"
}

trap "$TMUX_TEST kill-session -t $sess 2>/dev/null" EXIT

run_test() {
	tag="$1"
	ssh_args="$2"
	# Prepare a tmux session.
	kill_tmux
	$TMUX_TEST new-session -d -s $sess
	# echo XXXXXXXXXX $TMUX_TEST attach -t $sess; sleep 10

	# Command to start SSH; sent as keystrokes to tmux session.
	RCMD="$CLEANENV $SHELL"
	CMD="$SSH -F $OBJ/ssh_proxy $ssh_args -S $CTL x -tt $RCMD"

	verbose "${tag}: start connection"
	# arrange for the shell to print something after ssh completes.
	$TMUX_TEST send-keys -t $sess "$CMD && printf '$MAGIC1_OCTAL\n'" ENTER
	wait_for_mux_ready

	verbose "${tag}: send string"
	$TMUX_TEST send-keys -t $sess "printf '$MAGIC2_OCTAL\n'" ENTER
	wait_for_regex "$MAGIC2"

	verbose "${tag}: ^c interrupts process"
	# ^c should interrupt the sleep and prevent the magic string
	# from appearing.
	$TMUX_TEST send-keys -t $sess \
		"printf '$MAGIC3_OCTAL' ; sleep 30 || printf '$MAGIC4_OCTAL\n'"
	$TMUX_TEST send-keys -t $sess ENTER
	wait_for_regex "$MAGIC3" # Command has executed.
	$TMUX_TEST send-keys -t $sess "C-c"
	# send another string to let us know that the sleep has finished.
	$TMUX_TEST send-keys -t $sess "printf '$MAGIC5_OCTAL\n'" ENTER
	wait_for_regex "$MAGIC5"
	not_in_term "$MAGIC4" "^c did not interrupt"

	verbose "${tag}: ~? produces help"
	$TMUX_TEST send-keys -t $sess ENTER "~?"
	wait_for_regex "^Supported escape sequences:$"

	verbose "${tag}: ~. terminates session"
	$TMUX_TEST send-keys -t $sess ENTER "~."
	wait_for_mux_done
	not_in_term "$MAGIC1" "ssh unexpectedly exited successfully after ~."

	verbose "${tag}: restart session"
	$TMUX_TEST send-keys -t $sess "$CMD && printf '$MAGIC1_OCTAL\n'" ENTER
	wait_for_mux_ready

	verbose "${tag}: eof terminates session successfully"
	$TMUX_TEST send-keys -t $sess ENTER "C-d"
	wait_for_regex "$MAGIC1"
}

# Make sure tmux is working as expected before we start.
kill_tmux
$TMUX_TEST new-session -d -s $sess
# Make sure the session doesn't contain the magic strings we will use
# for signalling or any #? output.
not_in_term "$MAGIC1" "terminal already contains magic1 string" fatal
not_in_term "$MAGIC2" "terminal already contains magic2 string" fatal
not_in_term "$MAGIC3" "terminal already contains magic3 string" fatal
not_in_term "$MAGIC4" "terminal already contains magic4 string" fatal
not_in_term "$MAGIC5" "terminal already contains magic5 string" fatal
not_in_term "^Supported escape" "terminal already contains escape help" fatal
$TMUX_TEST send-keys -t $sess "printf '$MAGIC1_OCTAL\n'" ENTER
wait_for_regex "$MAGIC1" fatal
kill_tmux

run_test "basic" "-oControlMaster=yes"
run_test "ControlPersist" "-oControlMaster=auto -oControlPersist=1s"
