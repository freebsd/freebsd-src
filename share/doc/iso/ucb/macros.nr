.\"	
.\" Macro to initialize chapter macros
.\"	
.de IC
.nr CN 0 1
..
.\"	
.\" Macro to begin new chapter
.\"	
.de NC
.he 'ARGO user\'s Guide, Berkeley Hack Job''Chapter \\n+(CN'
.bp
.sh 0 "_" 1 1 1 1 1 1
.sz +2
.(l C
CHAPTER \\n(CN

\fB\\$1\fR
.)l
.sp 1
.(x
Chapter \\n(CN     \\$1
.)x
.sz -2
..
.\"	
.\"	Figure conventions:
.\"	1) do .so of figure source - figure reg incremented here
.\"	2) make references to figure via CF
.\"	
.\"	
.\"	Macro to initialize figure register
.\"	
.de IF
.nr FG 0 1
..
.\"	
.\"	Macro for current figure number
.\"	
.de CF
Figure \\n(FG
..
.\"
.\"	Define this macro to include section headings in table of contents
.\"
.de $0
.(x
Section \\$2 \\$1
.)x
..
