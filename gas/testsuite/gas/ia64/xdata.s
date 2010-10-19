// Note that most of the section names used here aren't legal as operands
// to either .section or .xdata/.xreal/.xstring (quoted strings aren't in
// general), but since generic code accepts them for .section we also test
// this here for our target specific directives. This could be viewed as a
// shortcut of a pair of .section/.secalias for each of them.

.section .xdata1, "a", @progbits
.section ".xdata2", "a", @progbits
.section ",xdata3", "a", @progbits
.section ".xdata,4", "a", @progbits
.section "\".xdata5\"", "a", @progbits

.section ".xreal\\1", "a", @progbits
.section ".xreal+2", "a", @progbits
.section ".xreal(3)", "a", @progbits
.section ".xreal[4]", "a", @progbits

.section ".xstr<1>", "a", @progbits
.section ".xstr{2}", "a", @progbits

.text

.xdata1 .xdata1, 1
.xdata2 ".xdata2", 2
.xdata4 ",xdata3", 3
.xdata8 ".xdata,4", 4
.xdata16 "\".xdata5\"", @iplt(_start)

.xdata2.ua ".xdata2", 2
.xdata4.ua ",xdata3", 3
.xdata8.ua ".xdata,4", 4
.xdata16.ua "\".xdata5\"", @iplt(_start)

.xreal4 ".xreal\\1", 1
.xreal8 ".xreal+2", 2
.xreal10 ".xreal(3)", 3
.xreal16 ".xreal[4]", 4

.xreal4.ua ".xreal\\1", 1
.xreal8.ua ".xreal+2", 2
.xreal10.ua ".xreal(3)", 3
.xreal16.ua ".xreal[4]", 4

.xstring ".xstr<1>", "abc"
.xstringz ".xstr{2}", "xyz"
