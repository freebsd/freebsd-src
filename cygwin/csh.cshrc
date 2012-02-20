#
# Example /etc/csh.cshrc for Cygwin
#
onintr -

if ( -d /etc/profile.d ) then
  set nonomatch
  foreach _s ( /etc/profile.d/*.csh )
    if ( -r $_s ) then
      source $_s
    endif
  end
  unset _s nonomatch
endif

if (! ${?prompt}) goto end

# This is an interactive session

# Now read in the key bindings of the tcsh
if ($?tcsh && -r /etc/profile.d/bindkey.tcsh) then
  source /etc/profile.d/bindkey.tcsh
endif

# On Cygwin it's possible to start tcsh without having any Cygwin /bin
# path in $PATH.  This breaks complete.tcsh starting with tcsh 6.15.00.
# For that reason we add /bin to $PATH temporarily here.  We remove it
# afterwards because it's added again (and correctly so) in /etc/csh.login.
set path=( /bin $path:q )

# Source the completion extension for tcsh
if ($?tcsh && -r /etc/profile.d/complete.tcsh) then
  source /etc/profile.d/complete.tcsh
endif

# Reset $PATH.
set path=( $path[2-]:q )

# If we find $HOME/.{t}cshrc we skip our settings used for interactive sessions.
if (-r "$HOME/.cshrc" || -r "$HOME/.tcshrc") goto end

# Set prompt
if ($?tcsh) then
  set prompt='[%n@%m %c02]$ '
else
  set prompt=\[`id -un`@`hostname`\]\$\ 
endif

# Some neat default settings.
set autocorrect=1
set autolist=ambiguous
unset autologout
set complete=enhance
set correct=cmd
set echo_style=both
set ellipsis
set fignore=(.o \~)
set histdup=erase
set history=100
unset ignoreeof
set listjobs=long
set listmaxrows=23
#set noglob
set notify=1
set rmstar=1
set savehist=( $history merge )
set showdots=1
set symlinks=expand

# Some neat aliases
alias ++ pushd
alias -- popd
alias d dirs
alias h history
alias j jobs
alias l 'ls -C'
alias la 'ls -a'
alias ll 'ls -l'
alias ls 'ls --color'

end:
  onintr
