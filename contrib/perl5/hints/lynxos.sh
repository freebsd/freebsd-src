#
# LynxOS hints
#
# These hints were submitted by:
#   Greg Seibert
#   seibert@Lynx.COM
# and
#   Ed Mooring
#   mooring@lynx.com
#

cc='gcc'
so='none'
usemymalloc='n'
d_union_semun='define'
ccflags="$ccflags -DEXTRA_F_IN_SEMUN_BUF -D__NO_INCLUDE_WARN__"

# When LynxOS runs a script with "#!" it sets argv[0] to the script name
toke_cflags='ccflags="$ccflags -DARG_ZERO_IS_SCRIPT"'
