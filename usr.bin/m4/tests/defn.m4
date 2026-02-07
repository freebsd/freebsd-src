dnl Check that our defn processes its arguments in order.
define(a,1)dnl
define(b,2)dnl
define(c,3)dnl
defn(`a',`b',`c')
