.if !targets(__<${_this:T}>__)
__<${_this:T}>__:	.NOTMAIN

_ALL_LIBCOMPATS:=	32

_ALL_libcompats:=	${_ALL_LIBCOMPATS:tl}

# List of LIBCOMPAT libcompat pairs to avoid repeating this ugly expression.
# Can be used as: .for LIBCOMPAT libcompat in ${_ALL_LIBCOMPATS_libcompats}
_ALL_LIBCOMPATS_libcompats:= \
	${_ALL_LIBCOMPATS:range:@i@${_ALL_LIBCOMPATS:[$i]} ${_ALL_libcompats:[$i]}@}

.endif
