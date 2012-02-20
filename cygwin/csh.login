#
# Example /etc/csh.login for Cygwin
#
unsetenv TEMP
unsetenv TMP

set path=( /usr/local/bin /usr/bin /bin $path:q )

if ( ! ${?USER} ) then
  set user="`id -un`"
endif
if ( ! ${?HOME} ) then
  set home=/home/$USER
endif
if ( ! -d "$HOME" ) then
  mkdir -p "$HOME"
endif

if ( ! ${?term} || "$term" == "unknown"  || "$tty" == "conin" ) then
  set term=cygwin
endif

setenv MAKE_MODE unix

setenv SHELL /bin/tcsh

umask 022

cd
