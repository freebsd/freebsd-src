%!PS-Adobe-2.0
%%Title: PADS Postscript Driver Header
%%Creator: Andy Montalvo, 18 Lupine St., Lowell, MA  01851
%%CreationDate: 06/08/90
%%For: CAD Software, Littleton, MA
%%EndComments
%%BeginProcSet: Markers 1.0 0
% marker attributes
/MAttr_Width 1 def
/MAttr_Size  0 def
/MAttr_Type /M1 def
% procedures
/M1 { %def
% draw marker 1: plus
% Stack: - M1 -
    -2 0 rmoveto
    4 0 rlineto
    -2 2 rmoveto
    0 -4 rlineto
} bind def
/M2 { %def
% draw marker 2: cross
% Stack: - M2 -
    -2 -2 rmoveto
    4 4 rlineto
    -4 0 rmoveto
    4 -4 rlineto
} bind def
/M3 { %def
% draw marker 3: square
% Stack: - M3 -
    0 2 rlineto
    2 0 rlineto
    0 -4 rlineto
    -4 0 rlineto
    0 4 rlineto
    2 0 rlineto
} bind def
/M4 { %def
% draw marker 4: diamond
% Stack: - M4 -
    0 2 rlineto
    2 -2 rlineto
    -2 -2 rlineto
    -2 2 rlineto
    2 2 rlineto
} bind def
/M5 { %def
% draw marker 5: hourglass
% Stack: - M5 -
    2 2 rlineto
    -4 0 rlineto
    4 -4 rlineto
    -4 0 rlineto
    2 2 rlineto
} bind def
/M6 { %def
% draw marker 6: bowtie
% Stack: - M6 -
    2 2 rlineto
    0 -4 rlineto
    -4 4 rlineto
    0 -4 rlineto
    2 2 rlineto
} bind def
/M7 { %def
% draw marker 7: small plus (goes with char marker)
% Stack: - M7 -
    -1 0 rmoveto
    2 0 rlineto
    -1 1 rmoveto
    0 -2 rlineto
} bind def
/Marker { %def
% Command from driver: draw marker
% STACK: x y Marker -
    MAttr_Size 0 gt
    {
        gsave
        moveto
        MAttr_Size 4 div dup scale
        MAttr_Type load exec
        4 MAttr_Size div dup scale
        MAttr_Width setlinewidth
        stroke
        grestore
    } if
} def
%%EndProcSet: Markers 1.0 0
%%BeginProcSet: Lib 1.0 0
/sg { %def
% Command from driver: set the gray scale 0 - 100
% STACK: greylevel sg
    100 div dup setgray /glev exch def
} bind def
/Circle { %def
% draw a circle
% STACK: x y radius Circle -
    0 360 arc
} bind def
/RndAper { %def
% select a round aperture
% STACK: - RndAper -
    1 setlinejoin
    1 setlinecap
} bind def
/SqrAper { %def
% select a square aperture
% STACK: - SqrAper -
    0 setlinejoin
    2 setlinecap
} bind def
/Line { %def
% draw a set of connected lines
% STACK: x1 y1 [ x2 y2 ... xn yn ] Line -
    3 1 roll
    moveto
    true
    exch
    % This pushes the x then the y then does lineto
    { exch  { false } { lineto true } ifelse } forall
    pop
} bind def
/Clipto { %def
% set clipping rectangle from 0,0 to new values
% STACK: x y Clipto -
    0 0 moveto
    dup 0 exch lineto
    2 copy lineto
    pop
    0 lineto
    closepath
    clip
    newpath
} bind def
/Clip4 { %def
% set clipping rectangle from xmin,ymin to xmax,ymax
% STACK: xmin ymin xmax ymax Clip4 -
    4 copy pop pop moveto
    4 copy pop exch lineto pop
    2 copy lineto
    exch pop exch pop lineto
    closepath
    clip
    newpath
} bind def
%%EndProcSet: Lib 1.0 0
%%BeginProcSet: Lines 1.0 0
% line attributes %
/LAttr_Width 1 def
% line procedures
/PLine { %def
% Cammand from driver: draw a set of connected lines
% STACK: x1 y1 [ x2 y2 ... xn yn ] PLine -
    Line
    LAttr_Width setlinewidth
    stroke
} bind def % PLine
/Char { %def
% Command from driver: draw a character at the current position
% STACK: type x y stroke_array Char -
%    stroke array -- [ stroke1 stroke2 ... stroken ]
%    stroke -- connected staight lines
%    type = 0 if text  1 if marker
    gsave
    4 1 roll
    translate
    0 eq { TAttr_Width } { MAttr_Width } ifelse setlinewidth
    {
        dup length 2 gt
        {
            dup dup 0 get exch 1 get % get starting point
            3 -1 roll                % put x y before array
            dup length 2 sub 2 exch getinterval % delete first items from array
            Line
            stroke
        }
        {
            aload pop currentlinewidth 2 div Circle fill
        } ifelse
    } forall
    grestore
} bind def % Char
/PArc { %def
% Command from driver: draw an arc
% STACK: x y radius startangle deltaangle Arc -
	 10 div exch 10 div exch
    2 copy pop add
    arc
    LAttr_Width setlinewidth
    stroke
} bind def
/PCircle { %def
% Command from driver: draw an circle
% STACK: x y radius PCircle -
    Circle
    LAttr_Width setlinewidth
    stroke
} bind def
%%EndProcSet: Lines 1.0 0
%%BeginProcSet: Polygon 1.0 0
% polygon attributes %
/PAttr_ExtWidth 1 def
/PAttr_IntWidth 1 def
/PAttr_Grid 1 def
% polygon procedures
/LoopSet { %def
% set up for loop condition
% STACK: start end LoopSet low gridwidth high
    2 copy lt { exch } if
    % make grid line up to absolute coordinates
    PAttr_Grid div truncate PAttr_Grid mul exch
    PAttr_Grid exch
} bind def
/Hatch { %def
% draw cross hatch pattern in current path
% STACK: - Hatch -
    pathbbox
    /ury exch def
    /urx exch def
    /lly exch def
    /llx exch def
    clip
    newpath
    llx urx LoopSet
    { % x loop
        dup lly exch ury moveto lineto
    } for
    lly ury LoopSet
    { % y loop
        llx exch dup urx exch moveto lineto
    } for
    PAttr_IntWidth setlinewidth
    stroke
} bind def
/PPoly { %def
% Command from driver: draw a plygon
% STACK: x1 y1 [ x2 y2 ... xn yn ] PLine -
    Line
    closepath
    gsave
    PAttr_IntWidth PAttr_Grid ge {fill} {Hatch} ifelse
    grestore
    PAttr_ExtWidth setlinewidth
    stroke
} bind def
%%EndProcSet: Polygon 1.0 0
%%BeginProcSet: Text 1.0 0
% text attributes %
/TAttr_Mirr 0 def
/TAttr_Orient 0 def
/TAttr_Width 1 def
% text procedures
/Text { %def
% Command from driver: Draw text
% STACK: x y width string Text -
    gsave
    4 2 roll
    translate
    TAttr_Mirr 0 gt
    {
        -1 1 scale
    } if
    TAttr_Orient rotate
    0 0 moveto
    dup length dup 1 gt
    {
        exch dup stringwidth pop
        4 -1 roll
        exch 2 copy
        lt
        {
            div 1 scale show
        }
        {
            sub
            3 -1 roll 1 sub div
            0 3 -1 roll ashow
        }
        ifelse
    }
    {
        pop
        show
    } ifelse
    grestore
} bind def
%%EndProcSet: Text 1.0 0
%%BeginProcSet: FlashSymbols 1.0 0
% flash symbol attributes %
/FAttr_Type /PRndPad def
/FAttr_Width  0 def
/FAttr_Length 1 def
/FAttr_Orient 0 def
% flash symbol procedures
/PRndPad { %def
% Command from driver: draw an circular pad
% STACK: - PCirclePad -
    FAttr_Width dup scale
    0 0 .5 Circle
    fill
} bind def
/PSqrPad { %def
% Draw an Square pad
% STACK: - PRectPad -
    FAttr_Width dup scale
    .5 .5 moveto
    -.5 .5 lineto
    -.5 -.5 lineto
    .5 -.5 lineto
    closepath
    fill
} bind def
/PRectPad { %def
% Draw an rectangular pad
% STACK: - PRectPad -
    FAttr_Length FAttr_Width scale
    .5 .5 moveto
    -.5 .5 lineto
    -.5 -.5 lineto
    .5 -.5 lineto
    closepath
    fill
} bind def
/POvalPad { %def
% Draw an oval pad
% STACK: - POvalPad -
    FAttr_Width setlinewidth
    FAttr_Length FAttr_Width sub 2 div dup
    neg 0 moveto
    0 lineto
    RndAper
    stroke
} bind def
/Anl { %def
    0 0 .5 Circle
    fill
    FAttr_Length FAttr_Width lt
    { % inner circle
        0 0
        FAttr_Length 0 gt { FAttr_Length FAttr_Width div } { .5 } ifelse
        2 div Circle
        1 setgray
        fill
        glev setgray
    } if
} bind def
/PAnlPad { %def
% Draw an annular pad
% STACK: - PAnlPad -
    FAttr_Width dup scale
    Anl
} bind def
/PRelPad { %def
% Draw an thermal relief pad
% STACK: - PRelPad -
    PAnlPad
    1 setgray
    .17 setlinewidth
    0 setlinecap   % the x
    45 rotate
    .5 0 moveto -.5 0 lineto
    0 .5 moveto  0 -.5 lineto
    stroke
    glev setgray
} bind def
/Flash { %def
% Command from driver: Flash a symbol
% STACK: x y Flash -
    FAttr_Width 0 gt
    {
        gsave
        translate
        FAttr_Orient rotate
        FAttr_Type load exec
        grestore
    } if
} def
%%EndProcSet: FlashSymbols 1.0 0
%%BeginProcSet: SetAttr 1.0 0
/SetLine { %def
% Set the width of the lines
% STACK: linewidth SetLine -
    /LAttr_Width exch def
    RndAper
} bind def
/SetPoly { %def
% Set attribute of polygon
% STACK: external_width internal_grid_width grid_spacing SetPoly -
    /PAttr_Grid exch def
    /PAttr_IntWidth exch def
    /PAttr_ExtWidth exch def
    RndAper
} bind def
/SetFlash { %def
% Set Attributed of flash pad
% STACK: orientation_angle length width aperture_type SetFlash -
    /FAttr_Type exch def
    FAttr_Type /PSqrPad eq FAttr_Type /PRectPad eq or
    { SqrAper } { RndAper } ifelse
    /FAttr_Width exch def
    /FAttr_Length exch def
    /FAttr_Orient exch 10 div def
} bind def
/SetMkr { %def
% Set attributes of markers
% STACK: linewidth size type SetMkr -
    /MAttr_Type exch def
    /MAttr_Size exch def
    /MAttr_Width exch def
    RndAper
} bind def
/SetText1 { %def
% Set attributes of text
% STACK: fontname height orient mirror SetMkr -
    /TAttr_Mirr exch def
    /TAttr_Orient exch 10 div def
    exch findfont exch scalefont setfont
    RndAper
} bind def
/SetText2 { %def
% Set attributes of text
% STACK: linewidth height mirror orient SetMkr -
    /TAttr_Width exch def
    RndAper
} bind def
%%EndProcSet: SetAttr 1.0 0
%%BeginProcSet: Initialize 1.0 0
/Init { %def
% Initialize the driver
% STACK: Init -
    72 1000 div dup scale % Scale to 1/1000 inch
    250 250 translate     % make origin 1/4 inch from bottom left
    1.5 setmiterlimit 1 RndAper                     % set line defaults
    0 setgray                                       % set color default
    /glev 0 def
} def
%%EndProcSet: Initialize 1.0 0
%%EndProlog
/Helvetica findfont 12 scalefont setfont
35 760 moveto
(gadget.job - Fri Aug 21 03:35:28 1992) show
gsave
Init
8000 10500 Clipto
4002 3763 translate
0 rotate
1 1 div dup scale
75 sg
50 sg
25 sg
0 sg
10 SetLine
-1350 0 [ -1350 4900 ] PLine
-1350 4900 [ 1350 4900 ] PLine
1350 4900 [ 1350 0 ] PLine
1350 0 [ -1350 0 ] PLine
10 SetLine
-1350 4700 [ -1350 4900 ] PLine
-1350 4900 [ -1150 4900 ] PLine
10 SetLine
1150 4900 [ 1350 4900 ] PLine
1350 4900 [ 1350 4700 ] PLine
10 SetLine
1150 0 [ 1350 0 ] PLine
1350 0 [ 1350 200 ] PLine
10 SetLine
-1350 200 [ -1350 0 ] PLine
-1350 0 [ -1150 0 ] PLine
10 80 /M4 SetMkr
-1100 1450 Marker
-1100 1150 Marker
300 3300 Marker
-100 3300 Marker
-100 3100 Marker
300 3100 Marker
300 3500 Marker
-100 3500 Marker
-400 3700 Marker
-1200 3700 Marker
-100 1300 Marker
-900 1300 Marker
-200 2800 Marker
600 2800 Marker
-1200 2800 Marker
-400 2800 Marker
0 2500 Marker
0 2100 Marker
-1200 3400 Marker
-1200 3000 Marker
-900 2300 Marker
-1200 2300 Marker
-1200 2500 Marker
-900 2500 Marker
800 2800 Marker
1100 2800 Marker
1250 1900 Marker
450 1900 Marker
-100 900 Marker
-1200 900 Marker
-700 4000 Marker
-1100 4000 Marker
1100 3000 Marker
700 3000 Marker
-300 3700 Marker
0 3700 Marker
10 80 /M7 SetMkr
100 900 Marker
1 113 913 [ [ 25 52 0 0 ] [ 0 52 25 52 ] [ 0 0 25 0 ] ] Char
600 900 Marker
1 613 913 [ [ 25 52 0 0 ] [ 0 52 25 52 ] [ 0 0 25 0 ] ] Char
700 3700 Marker
1 713 3713 [ [ 25 52 0 0 ] [ 0 52 25 52 ] [ 0 0 25 0 ] ] Char
200 3700 Marker
1 213 3713 [ [ 25 52 0 0 ] [ 0 52 25 52 ] [ 0 0 25 0 ] ] Char
10 80 /M4 SetMkr
-750 550 Marker
-750 450 Marker
0 550 Marker
0 450 Marker
750 550 Marker
750 450 Marker
-648 4479 Marker
-540 4479 Marker
-432 4479 Marker
-324 4479 Marker
-216 4479 Marker
-108 4479 Marker
0 4479 Marker
108 4479 Marker
216 4479 Marker
324 4479 Marker
432 4479 Marker
540 4479 Marker
648 4479 Marker
-594 4593 Marker
-486 4593 Marker
-378 4593 Marker
-270 4593 Marker
-162 4593 Marker
-54 4593 Marker
54 4593 Marker
162 4593 Marker
270 4593 Marker
378 4593 Marker
486 4593 Marker
594 4593 Marker
10 80 /M7 SetMkr
940 4536 Marker
1 953 4549 [ [ 0 52 14 27 14 0 ] [ 29 52 14 27 ] ] Char
-940 4536 Marker
1 -927 4549 [ [ 0 52 14 27 14 0 ] [ 29 52 14 27 ] ] Char
10 80 /M4 SetMkr
950 150 Marker
1050 150 Marker
-50 150 Marker
50 150 Marker
-1050 150 Marker
-950 150 Marker
10 80 /M7 SetMkr
950 3524 Marker
1 963 3537 [ [ 0 52 25 0 ] [ 25 52 0 0 ] ] Char
1026 3612 Marker
1 1039 3625 [ [ 0 52 25 0 ] [ 25 52 0 0 ] ] Char
950 3700 Marker
1 963 3713 [ [ 0 52 25 0 ] [ 25 52 0 0 ] ] Char
10 80 /M7 SetMkr
-1200 1600 Marker
1 -1187 1613 [ [ 0 52 9 0 ] [ 18 52 9 0 ] [ 18 52 27 0 ] [ 36 52 27 0 ] ] Char
-1100 1700 Marker
1 -1087 1713 [ [ 0 52 9 0 ] [ 18 52 9 0 ] [ 18 52 27 0 ] [ 36 52 27 0 ] ] Char
-1200 1800 Marker
1 -1187 1813 [ [ 0 52 9 0 ] [ 18 52 9 0 ] [ 18 52 27 0 ] [ 36 52 27 0 ] ] Char
10 80 /M4 SetMkr
300 1700 Marker
-100 1700 Marker
200 1300 Marker
600 1300 Marker
-700 2300 Marker
-300 2300 Marker
-700 4200 Marker
-1100 4200 Marker
1100 3200 Marker
700 3200 Marker
-700 2500 Marker
-300 2500 Marker
-700 2100 Marker
-300 2100 Marker
-800 2100 Marker
-1200 2100 Marker
600 1100 Marker
200 1100 Marker
10 80 /M7 SetMkr
1200 2450 Marker
1 1213 2463 [ [ 0 52 9 0 ] [ 18 52 9 0 ] [ 18 52 27 0 ] [ 36 52 27 0 ] ] Char
1100 2350 Marker
1 1113 2363 [ [ 0 52 9 0 ] [ 18 52 9 0 ] [ 18 52 27 0 ] [ 36 52 27 0 ] ] Char
1200 2250 Marker
1 1213 2263 [ [ 0 52 9 0 ] [ 18 52 9 0 ] [ 18 52 27 0 ] [ 36 52 27 0 ] ] Char
10 80 /M4 SetMkr
-100 1900 Marker
300 1900 Marker
0 1500 Marker
400 1500 Marker
-900 1600 Marker
-800 1600 Marker
-700 1600 Marker
-600 1600 Marker
-500 1600 Marker
-400 1600 Marker
-300 1600 Marker
-300 1900 Marker
-400 1900 Marker
-500 1900 Marker
-600 1900 Marker
-700 1900 Marker
-800 1900 Marker
-900 1900 Marker
200 2200 Marker
300 2200 Marker
400 2200 Marker
500 2200 Marker
600 2200 Marker
700 2200 Marker
800 2200 Marker
900 2200 Marker
900 2500 Marker
800 2500 Marker
700 2500 Marker
600 2500 Marker
500 2500 Marker
400 2500 Marker
300 2500 Marker
200 2500 Marker
200 3900 Marker
300 3900 Marker
400 3900 Marker
500 3900 Marker
600 3900 Marker
700 3900 Marker
800 3900 Marker
900 3900 Marker
1000 3900 Marker
1100 3900 Marker
1100 4200 Marker
1000 4200 Marker
900 4200 Marker
800 4200 Marker
700 4200 Marker
600 4200 Marker
500 4200 Marker
400 4200 Marker
300 4200 Marker
200 4200 Marker
-1000 3100 Marker
-900 3100 Marker
-800 3100 Marker
-700 3100 Marker
-600 3100 Marker
-500 3100 Marker
-400 3100 Marker
-300 3100 Marker
-300 3400 Marker
-400 3400 Marker
-500 3400 Marker
-600 3400 Marker
-700 3400 Marker
-800 3400 Marker
-900 3400 Marker
-1000 3400 Marker
900 800 Marker
1100 800 Marker
1000 800 Marker
10 80 /M7 SetMkr
1000 1550 Marker
1 1013 1563 [ [ 0 52 14 27 14 0 ] [ 29 52 14 27 ] ] Char
10 80 /M4 SetMkr
0 4000 Marker
0 4200 Marker
10 80 /M7 SetMkr
-1100 450 Marker
1 -1087 463 [ [ 0 52 14 27 14 0 ] [ 29 52 14 27 ] ] Char
1100 450 Marker
1 1113 463 [ [ 0 52 14 27 14 0 ] [ 29 52 14 27 ] ] Char
10 80 /M4 SetMkr
1100 3400 Marker
700 3400 Marker
10 80 /M4 SetMkr
-200 2350 Marker
-200 1750 Marker
200 1400 Marker
10 80 /M4 SetMkr
300 1100 Marker
10 80 /M4 SetMkr
175 3300 Marker
10 80 /M4 SetMkr
1000 2650 Marker
10 80 /M4 SetMkr
-450 2100 Marker
10 80 /M4 SetMkr
-650 2600 Marker
-100 2600 Marker
-100 2250 Marker
10 80 /M4 SetMkr
800 1800 Marker
10 80 /M4 SetMkr
700 1900 Marker
10 80 /M4 SetMkr
600 1700 Marker
10 80 /M4 SetMkr
500 1600 Marker
10 80 /M4 SetMkr
0 4100 Marker
900 4100 Marker
10 80 /M4 SetMkr
800 3000 Marker
10 80 /M4 SetMkr
600 3700 Marker
10 80 /M4 SetMkr
450 2700 Marker
-400 2700 Marker
-400 2300 Marker
10 80 /M4 SetMkr
350 3800 Marker
10 80 /M4 SetMkr
-850 3700 Marker
10 80 /M4 SetMkr
400 1300 Marker
10 80 /M4 SetMkr
-1050 1050 Marker
10 80 /M4 SetMkr
0 4475 Marker
75 4000 Marker
1000 4000 Marker
10 80 /M4 SetMkr
950 3200 Marker
10 80 /M4 SetMkr
850 1200 Marker
-600 1200 Marker
10 80 /M4 SetMkr
-550 3900 Marker
-350 3900 Marker
10 80 /M4 SetMkr
-800 4300 Marker
10 80 /M4 SetMkr
-750 4350 Marker
10 80 /M4 SetMkr
400 3400 Marker
10 SetLine
-1355 -485 [ -275 -485 ] PLine
-1355 -725 [ -275 -725 ] PLine
-1355 -965 [ -275 -965 ] PLine
-1355 -1205 [ -275 -1205 ] PLine
-1355 -1445 [ -275 -1445 ] PLine
-1355 -1685 [ -275 -1685 ] PLine
-1355 -1925 [ -275 -1925 ] PLine
-1355 -485 [ -1355 -1925 ] PLine
-995 -485 [ -995 -1925 ] PLine
-635 -485 [ -635 -1925 ] PLine
-275 -485 [ -275 -1925 ] PLine
10 SetText2
0 -1295 -665 [ [ 38 67 32 75 24 78 13 78 5 75 0 67 0 60 2 52 5 48 10 45 27 37 32 33 35 30 38 22 38 11 32 3 24 0 13 0 5 3 0 11 ] ] Char
0 -1233 -665 [ [ 0 78 0 0 ] ] Char
0 -1209 -665 [ [ 38 78 0 0 ] [ 0 78 38 78 ] [ 0 0 38 0 ] ] Char
0 -1147 -665 [ [ 0 78 0 0 ] [ 0 78 35 78 ] [ 0 41 21 41 ] [ 0 0 35 0 ] ] Char
10 SetText2
0 -873 -665 [ [ 16 78 10 75 5 67 2 60 0 48 0 30 2 18 5 11 10 3 16 0 27 0 32 3 38 11 40 18 43 30 43 48 40 60 38 67 32 75 27 78 16 78 ] [ 24 15 40 -7 ] ] Char
0 -805 -665 [ [ 19 78 19 0 ] [ 0 78 38 78 ] ] Char
0 -743 -665 [ [ 0 78 21 41 21 0 ] [ 43 78 21 41 ] ] Char
10 SetText2
0 -575 -665 [ [ 38 67 32 75 24 78 13 78 5 75 0 67 0 60 2 52 5 48 10 45 27 37 32 33 35 30 38 22 38 11 32 3 24 0 13 0 5 3 0 11 ] ] Char
0 -513 -665 [ [ 0 78 21 41 21 0 ] [ 43 78 21 41 ] ] Char
0 -445 -665 [ [ 0 78 0 0 ] [ 0 78 21 0 ] [ 43 78 21 0 ] [ 43 78 43 0 ] ] Char
10 SetText2
0 -1233 -905 [ [ 5 78 35 78 19 48 27 48 32 45 35 41 38 30 38 22 35 11 30 3 21 0 13 0 5 3 2 7 0 15 ] ] Char
0 -1171 -905 [ [ 38 78 10 0 ] [ 0 78 38 78 ] ] Char
10 SetText2
0 -873 -905 [ [ 2 60 2 63 5 71 8 75 13 78 24 78 30 75 32 71 35 63 35 56 32 48 27 37 0 0 38 0 ] ] Char
0 -811 -905 [ [ 16 78 8 75 2 63 0 45 0 33 2 15 8 3 16 0 21 0 30 3 35 15 38 33 38 45 35 63 30 75 21 78 16 78 ] ] Char
0 -749 -905 [ [ 27 78 0 26 40 26 ] [ 27 78 27 0 ] ] Char
10 120 /M4 SetMkr
-455 -845 Marker
10 SetText2
0 -1233 -1145 [ [ 27 78 0 26 40 26 ] [ 27 78 27 0 ] ] Char
0 -1168 -1145 [ [ 0 63 5 67 13 78 13 0 ] ] Char
10 SetText2
0 -749 -1145 [ [ 32 67 30 75 21 78 16 78 8 75 2 63 0 45 0 26 2 11 8 3 16 0 19 0 27 3 32 11 35 22 35 26 32 37 27 45 19 48 16 48 8 45 2 37 0 26 ] ] Char
10 SetText2
0 -515 -1145 [ [ 0 78 13 0 ] [ 27 78 13 0 ] [ 27 78 40 0 ] [ 54 78 40 0 ] ] Char
10 SetText2
0 -1233 -1385 [ [ 5 78 35 78 19 48 27 48 32 45 35 41 38 30 38 22 35 11 30 3 21 0 13 0 5 3 2 7 0 15 ] ] Char
0 -1171 -1385 [ [ 2 60 2 63 5 71 8 75 13 78 24 78 30 75 32 71 35 63 35 56 32 48 27 37 0 0 38 0 ] ] Char
10 SetText2
0 -749 -1385 [ [ 5 78 35 78 19 48 27 48 32 45 35 41 38 30 38 22 35 11 30 3 21 0 13 0 5 3 2 7 0 15 ] ] Char
10 SetText2
0 -515 -1385 [ [ 0 78 38 0 ] [ 38 78 0 0 ] ] Char
10 SetText2
0 -1295 -1625 [ [ 0 63 5 67 13 78 13 0 ] ] Char
0 -1257 -1625 [ [ 2 60 2 63 5 71 8 75 13 78 24 78 30 75 32 71 35 63 35 56 32 48 27 37 0 0 38 0 ] ] Char
0 -1195 -1625 [ [ 16 78 8 75 2 63 0 45 0 33 2 15 8 3 16 0 21 0 30 3 35 15 38 33 38 45 35 63 30 75 21 78 16 78 ] ] Char
10 SetText2
0 -749 -1625 [ [ 35 78 8 78 5 45 8 48 16 52 24 52 32 48 38 41 40 30 38 22 35 11 30 3 21 0 13 0 5 3 2 7 0 15 ] ] Char
10 SetText2
0 -515 -1625 [ [ 0 78 21 41 21 0 ] [ 43 78 21 41 ] ] Char
10 SetText2
0 -1233 -1865 [ [ 5 78 35 78 19 48 27 48 32 45 35 41 38 30 38 22 35 11 30 3 21 0 13 0 5 3 2 7 0 15 ] ] Char
0 -1171 -1865 [ [ 35 78 8 78 5 45 8 48 16 52 24 52 32 48 38 41 40 30 38 22 35 11 30 3 21 0 13 0 5 3 2 7 0 15 ] ] Char
10 SetText2
0 -749 -1865 [ [ 27 78 0 26 40 26 ] [ 27 78 27 0 ] ] Char
10 SetText2
0 -515 -1865 [ [ 38 78 0 0 ] [ 0 78 38 78 ] [ 0 0 38 0 ] ] Char
grestore
showpage
