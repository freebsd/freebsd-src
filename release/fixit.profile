:
# $Id: fixit.profile,v 1.1 1995/03/15 06:14:19 phk Exp $

export BLOCKSIZE=K
export PS1="Fixit# "
export EDITOR="/mnt2/stand/vi"
export PAGER="/mnt2/stand/more"

alias ls="ls -F"
alias ll="ls -l"
alias m="more -e"

echo '+---------------------------------------------------------------+'
echo '| You are now running from a FreeBSD "fixit" floppy.            |'
echo '| ------------------------------------------------------------- |'
echo "| When you're finished with this shell, please type exit.       |"
echo '| The fixit floppy itself is mounted as /mnt2.                  |'
echo '|                                                               |'
echo '| You might want to symlink /mnt/etc/*pwd.db and /mnt/etc/group |'
echo '| to /etc after mounting a root filesystem from your disk.      |'
echo '| tar(1) will not restore all permissions correctly otherwise!  |'
echo '+---------------------------------------------------------------+'
echo
echo 'Good Luck!'
echo

# Make the arrow keys work; everybody will love this.
set -o emacs 2>/dev/null

cd /
