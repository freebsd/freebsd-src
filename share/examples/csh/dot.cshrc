# Here are some example (t)csh options and configurations that you may find interesting
#
# $FreeBSD$
#

# Sets SSH_AUTH_SOCK to the user's ssh-agent socket path if running
if (${?SSH_AUTH_SOCK} != "1") then
	setenv SSH_AUTH_SOCK `sockstat | grep "${USER}" | cut -d ' ' -f 6` 
endif

# Change only root's prompt
if (`id -g` == 0)
	set prompt="root@%m# "
endif

# This maps the "Delete" key to do the right thing
# Pressing CTRL-v followed by the key of interest will print the shell's
# mapping for the key
bindkey "^[[3~" delete-char-or-list-or-eof

# Make the Ins key work
bindkey "\e[2~" overwrite-mode 

# Some common completions
complete chown          'p/1/u/'
complete man            'C/*/c/'
complete service        'n/*/`service -l`/'
complete service  	'c/-/(e l r v)/' 'p/1/`service -l`/' 'n/*/(start stop reload restart status rcvar onestart onestop)/'
complete kldunload	'n@*@`kldstat | awk \{sub\(\/\.ko\/,\"\",\$NF\)\;print\ \$NF\} | grep -v Name` @'
complete make           'n@*@`make -pn | sed -n -E "/^[#_.\/[:blank:]]+/d; /=/d; s/[[:blank:]]*:.*//gp;"`@'
complete pkg_delete     'c/-/(i v D n p d f G x X r)/' 'n@*@`ls /var/db/pkg`@'
complete pkg_info       'c/-/(a b v p q Q c d D f g i I j k K r R m L s o G O x X e E l t V P)/' 'n@*@`\ls -1 /var/db/pkg | sed s%/var/db/pkg/%%`@'
complete kill		'c/-/S/' 'c/%/j/' 'n/*/`ps -ax | awk '"'"'{print $1}'"'"'`/'
complete killall	'c/-/S/' 'c/%/j/' 'n/*/`ps -ax | awk '"'"'{print $5}'"'"'`/'
complete dd		'c/[io]f=/f/ n/*/"(if of ibs obs bs skip seek count)"/='
alias _PKGS_PkGs_PoRtS_ 'awk -F\| \{sub\(\"\/usr\/ports\/\"\,\"\"\,\$2\)\;print\ \$2\} /usr/ports/INDEX-name -r | cut -d . -f 1'
alias _PKGS_PkGs_PoRtS_ 'awk -F\| \{sub\(\"\/usr\/ports\/\"\,\"\"\,\$2\)\;print\ \$2\} /usr/ports/INDEX-`uname -r | cut -d . -f 1`&& pkg_info -E \*'
complete portmaster   'c/--/(always-fetch check-depends check-port-dbdir clean-distfiles \
    clean-packages delete-build-only delete-packages force-config help \
    index index-first index-only list-origins local-packagedir no-confirm \
    no-index-fetch no-term-title packages packages-build packages-if-newer \
    packages-local packages-only show-work update-if-newer version)/' \
    'c/-/(a b B C d D e f F g G h H i l L m n o p r R s t u v w x)/' \
    'n@*@`_PKGS_PkGs_PoRtS_`@'

# Alternate prompts
set prompt = '#'
set prompt = '%B%m%b%# '
set prompt = '%B%m%b:%c03:%# '
set prompt = '%{\033]0;%n@%m:%/\007%}%B%m%b:%c03:%# '
set prompt = "%n@%m %c04%m%# "
set prompt = "%n@%m:%c04 %# "
set prompt = "[%n@%m]%c04%# "
set ellipsis

# Color ls
alias ll	ls -lAhG
alias ls	ls -G

# Color on many system utilities
setenv CLICOLOR 1

# other autolist options
set		autolist = TAB
