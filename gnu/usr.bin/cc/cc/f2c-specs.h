/***** ljo's Fortran rule *****/
  {".f", "@f2c"},
  {"@f2c",
   "f2c %{checksubscripts:-C} %{I2} %{onetrip} %{honorcase:-U} %{u} %{w}\
        %{ANSIC:-A} %{a} %{C++}\
        %{c} %{E} %{ec} %{ext} %{f} %{72} %{g} %{h} %{i2} %{kr} %{krd}\
        %{P} %{p} %{r} %{r8} %{s} %{w8} %{z} %{N*}\
        %i %{!pipe: -o %g.c} %{pipe:-o -}|\n",
   "cpp -lang-c %{nostdinc*} %{C} %{v} %{A*} %{I*} %{P} %I\
	%{C:%{!E:%eGNU C does not support -C without using -E}}\
	%{M} %{MM} %{MD:-MD %b.d} %{MMD:-MMD %b.d} %{MG}\
        -undef -D__GNUC__=%v1 -D__GNUC_MINOR__=%v2\
	%{ansi:-trigraphs -$ -D__STRICT_ANSI__}\
	%{!undef:%{!ansi:%p} %P} %{trigraphs} \
        %c %{O*:%{!O0:-D__OPTIMIZE__}} %{traditional} %{ftraditional:-traditional}\
        %{traditional-cpp:-traditional}\
	%{g*} %{W*} %{w} %{pedantic*} %{H} %{d*} %C %{D*} %{U*} %{i*}\
        %{pipe:-} %{!pipe:%g.c} %{!M:%{!MM:%{!E:%{!pipe:%g.i}}}}%{E:%W{o*}}%{M:%W{o*}}%{MM:%W{o*}} |\n",
   "%{!M:%{!MM:%{!E:cc1 %{!pipe:%g.i} %1 \
		   %{!Q:-quiet} -dumpbase %b.c %{d*} %{m*} %{a}\
		   %{g*} %{O*} %{W*} %{w} %{pedantic*} %{ansi} \
		   %{traditional} %{v:-version} %{pg:-p} %{p} %{f*}\
		   %{aux-info*}\
		   %{pg:%{fomit-frame-pointer:%e-pg and -fomit-frame-pointer are incompatible}}\
		   %{S:%W{o*}%{!o*:-o %b.s}}%{!S:-o %{|!pipe:%g.s}} |\n\
              %{!S:as %{R} %{j} %{J} %{h} %{d2} %a %Y\
		      %{c:%W{o*}%{!o*:-o %w%b.o}}%{!c:-o %d%w%u.o}\
                      %{!pipe:%g.s} %A\n }}}}"},
/***** End of ljo's Fortran rule *****/
