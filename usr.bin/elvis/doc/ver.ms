.ds V 1.7
.if t .ds E E\s'-2'LVIS\s'0'
.if n .ds E Elvis
.\"
.\" usage: .Go <section#> <title>
.de Go
.ds LH "\\$1-\\\\n%
.ds RH "\\$1-\\\\n%
.ds CH "\\$2
.NH S \\$1
\\$2
.\"if !\\n%=1 .bp 1
.if n .ul 0
..
