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
(gadget.job - Fri Aug 21 03:35:14 1992) show
gsave
Init
8000 10500 Clipto
4025 2626 translate
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
10 SetLine
400 5100 [ 1350 5100 ] PLine
10 SetLine
1350 5100 [ 1300 5150 ] PLine
10 SetLine
1350 5100 [ 1300 5050 ] PLine
10 SetLine
-1525 4900 [ -1375 4900 ] PLine
10 SetLine
-1350 5100 [ -375 5100 ] PLine
10 SetLine
-1350 5100 [ -1300 5150 ] PLine
10 SetLine
-1350 5100 [ -1300 5050 ] PLine
10 SetLine
-1542 0 [ -1367 0 ] PLine
10 SetLine
-1500 0 [ -1500 1965 ] PLine
10 SetLine
-1500 0 [ -1450 50 ] PLine
10 SetLine
-1500 0 [ -1550 50 ] PLine
10 SetLine
-1500 4900 [ -1450 4850 ] PLine
10 SetLine
-1500 4900 [ -1550 4850 ] PLine
10 SetLine
1350 5150 [ 1350 4925 ] PLine
10 SetLine
-1350 5150 [ -1350 4925 ] PLine
10 SetLine
-1500 4900 [ -1500 2750 ] PLine
10 SetLine
-1050 1400 [ -1050 1200 ] PLine
-1050 1200 [ -1150 1200 ] PLine
-1150 1200 [ -1150 1400 ] PLine
-1150 1400 [ -1050 1400 ] PLine
10 SetLine
-50 3300 [ -100 3300 ] PLine
10 SetLine
250 3235 [ -50 3235 ] PLine
-50 3235 [ -50 3365 ] PLine
-50 3365 [ 250 3365 ] PLine
250 3365 [ 250 3235 ] PLine
10 SetLine
300 3300 [ 250 3300 ] PLine
10 SetLine
250 3100 [ 300 3100 ] PLine
10 SetLine
-50 3165 [ 250 3165 ] PLine
250 3165 [ 250 3035 ] PLine
250 3035 [ -50 3035 ] PLine
-50 3035 [ -50 3165 ] PLine
10 SetLine
-100 3100 [ -50 3100 ] PLine
10 SetLine
-50 3500 [ -100 3500 ] PLine
10 SetLine
250 3435 [ -50 3435 ] PLine
-50 3435 [ -50 3565 ] PLine
-50 3565 [ 250 3565 ] PLine
250 3565 [ 250 3435 ] PLine
10 SetLine
300 3500 [ 250 3500 ] PLine
10 SetLine
-1150 3700 [ -1200 3700 ] PLine
10 SetLine
-450 3575 [ -1150 3575 ] PLine
-1150 3575 [ -1150 3825 ] PLine
-1150 3825 [ -450 3825 ] PLine
-450 3825 [ -450 3575 ] PLine
10 SetLine
-400 3700 [ -450 3700 ] PLine
10 SetLine
-850 1300 [ -900 1300 ] PLine
10 SetLine
-150 1175 [ -850 1175 ] PLine
-850 1175 [ -850 1425 ] PLine
-850 1425 [ -150 1425 ] PLine
-150 1425 [ -150 1175 ] PLine
10 SetLine
-100 1300 [ -150 1300 ] PLine
10 SetLine
550 2800 [ 600 2800 ] PLine
10 SetLine
-150 2925 [ 550 2925 ] PLine
550 2925 [ 550 2675 ] PLine
550 2675 [ -150 2675 ] PLine
-150 2675 [ -150 2925 ] PLine
10 SetLine
-200 2800 [ -150 2800 ] PLine
10 SetLine
-450 2800 [ -400 2800 ] PLine
10 SetLine
-1150 2925 [ -450 2925 ] PLine
-450 2925 [ -450 2675 ] PLine
-450 2675 [ -1150 2675 ] PLine
-1150 2675 [ -1150 2925 ] PLine
10 SetLine
-1200 2800 [ -1150 2800 ] PLine
10 SetLine
0 2150 [ 0 2100 ] PLine
10 SetLine
65 2450 [ 65 2150 ] PLine
65 2150 [ -65 2150 ] PLine
-65 2150 [ -65 2450 ] PLine
-65 2450 [ 65 2450 ] PLine
10 SetLine
0 2500 [ 0 2450 ] PLine
10 SetLine
-1200 3050 [ -1200 3000 ] PLine
10 SetLine
-1135 3350 [ -1135 3050 ] PLine
-1135 3050 [ -1265 3050 ] PLine
-1265 3050 [ -1265 3350 ] PLine
-1265 3350 [ -1135 3350 ] PLine
10 SetLine
-1200 3400 [ -1200 3350 ] PLine
10 SetLine
-950 2250 [ -1150 2250 ] PLine
-1150 2250 [ -1150 2350 ] PLine
-1150 2350 [ -950 2350 ] PLine
-950 2350 [ -950 2250 ] PLine
10 SetLine
-1150 2550 [ -950 2550 ] PLine
-950 2550 [ -950 2450 ] PLine
-950 2450 [ -1150 2450 ] PLine
-1150 2450 [ -1150 2550 ] PLine
10 SetLine
850 2850 [ 1050 2850 ] PLine
1050 2850 [ 1050 2750 ] PLine
1050 2750 [ 850 2750 ] PLine
850 2750 [ 850 2850 ] PLine
10 SetLine
500 1900 [ 450 1900 ] PLine
10 SetLine
1200 1775 [ 500 1775 ] PLine
500 1775 [ 500 2025 ] PLine
500 2025 [ 1200 2025 ] PLine
1200 2025 [ 1200 1775 ] PLine
10 SetLine
1250 1900 [ 1200 1900 ] PLine
10 SetLine
-1150 900 [ -1200 900 ] PLine
10 SetLine
-150 725 [ -1150 725 ] PLine
-1150 725 [ -1150 1075 ] PLine
-1150 1075 [ -150 1075 ] PLine
-150 1075 [ -150 725 ] PLine
10 SetLine
-100 900 [ -150 900 ] PLine
10 SetLine
-1050 4000 [ -1100 4000 ] PLine
10 SetLine
-750 3935 [ -1050 3935 ] PLine
-1050 3935 [ -1050 4065 ] PLine
-1050 4065 [ -750 4065 ] PLine
-750 4065 [ -750 3935 ] PLine
10 SetLine
-700 4000 [ -750 4000 ] PLine
10 SetLine
750 3000 [ 700 3000 ] PLine
10 SetLine
1050 2935 [ 750 2935 ] PLine
750 2935 [ 750 3065 ] PLine
750 3065 [ 1050 3065 ] PLine
1050 3065 [ 1050 2935 ] PLine
10 SetLine
1100 3000 [ 1050 3000 ] PLine
10 SetLine
-250 3750 [ -50 3750 ] PLine
-50 3750 [ -50 3650 ] PLine
-50 3650 [ -250 3650 ] PLine
-250 3650 [ -250 3750 ] PLine
10 SetLine
200 900 [ 150 900 ] PLine
10 SetLine
270 950 [ 270 850 ] PLine
10 SetLine
200 850 [ 200 950 ] PLine
200 950 [ 500 950 ] PLine
500 950 [ 500 850 ] PLine
500 850 [ 200 850 ] PLine
10 SetLine
500 900 [ 550 900 ] PLine
10 SetLine
250 850 [ 250 950 ] PLine
10 SetLine
260 850 [ 260 950 ] PLine
10 SetLine
600 3700 [ 650 3700 ] PLine
10 SetLine
530 3650 [ 530 3750 ] PLine
10 SetLine
600 3750 [ 600 3650 ] PLine
600 3650 [ 300 3650 ] PLine
300 3650 [ 300 3750 ] PLine
300 3750 [ 600 3750 ] PLine
10 SetLine
300 3700 [ 250 3700 ] PLine
10 SetLine
550 3750 [ 550 3650 ] PLine
10 SetLine
540 3750 [ 540 3650 ] PLine
10 SetLine
-750 550 100 PCircle
10 SetLine
0 550 100 PCircle
10 SetLine
750 550 100 PCircle
10 SetLine
768 5000 [ 768 5248 ] PLine
768 5248 [ -768 5248 ] PLine
-768 5248 [ -768 5000 ] PLine
10 SetLine
1058 4900 [ -1058 4900 ] PLine
10 SetLine
1058 5000 [ 1058 4408 ] PLine
1058 4408 [ -1058 4408 ] PLine
-1058 4408 [ -1058 5000 ] PLine
-1058 5000 [ 1058 5000 ] PLine
10 SetLine
1058 5000 [ -1058 5000 ] PLine
10 SetLine
768 4900 [ 768 4408 ] PLine
10 SetLine
-768 4900 [ -768 4408 ] PLine
10 SetLine
900 200 [ 1100 200 ] PLine
1100 200 [ 1100 100 ] PLine
1100 100 [ 900 100 ] PLine
900 100 [ 900 200 ] PLine
10 SetLine
-100 200 [ 100 200 ] PLine
100 200 [ 100 100 ] PLine
100 100 [ -100 100 ] PLine
-100 100 [ -100 200 ] PLine
10 SetLine
-1100 200 [ -900 200 ] PLine
-900 200 [ -900 100 ] PLine
-900 100 [ -1100 100 ] PLine
-1100 100 [ -1100 200 ] PLine
10 SetLine
916 3493 [ 900 3456 ] PLine
900 3456 [ 939 3442 ] PLine
939 3442 [ 953 3477 ] PLine
10 SetLine
988 3612 140 PCircle
10 SetLine
-1000 1529 [ -1039 1490 ] PLine
10 SetLine
-1000 1490 [ -1000 1910 ] PLine
-1000 1910 [ -1300 1910 ] PLine
-1300 1910 [ -1300 1490 ] PLine
-1300 1490 [ -1000 1490 ] PLine
10 SetLine
200 1730 [ 200 1670 ] PLine
200 1670 [ 0 1670 ] PLine
0 1670 [ 0 1730 ] PLine
0 1730 [ 200 1730 ] PLine
10 SetLine
200 1700 [ 260 1700 ] PLine
10 SetLine
0 1700 [ -50 1700 ] PLine
10 SetLine
300 1270 [ 300 1330 ] PLine
300 1330 [ 500 1330 ] PLine
500 1330 [ 500 1270 ] PLine
500 1270 [ 300 1270 ] PLine
10 SetLine
300 1300 [ 240 1300 ] PLine
10 SetLine
500 1300 [ 550 1300 ] PLine
10 SetLine
-600 2270 [ -600 2330 ] PLine
-600 2330 [ -400 2330 ] PLine
-400 2330 [ -400 2270 ] PLine
-400 2270 [ -600 2270 ] PLine
10 SetLine
-600 2300 [ -660 2300 ] PLine
10 SetLine
-400 2300 [ -350 2300 ] PLine
10 SetLine
-800 4230 [ -800 4170 ] PLine
-800 4170 [ -1000 4170 ] PLine
-1000 4170 [ -1000 4230 ] PLine
-1000 4230 [ -800 4230 ] PLine
10 SetLine
-800 4200 [ -740 4200 ] PLine
10 SetLine
-1000 4200 [ -1050 4200 ] PLine
10 SetLine
1000 3230 [ 1000 3170 ] PLine
1000 3170 [ 800 3170 ] PLine
800 3170 [ 800 3230 ] PLine
800 3230 [ 1000 3230 ] PLine
10 SetLine
1000 3200 [ 1060 3200 ] PLine
10 SetLine
800 3200 [ 750 3200 ] PLine
10 SetLine
-600 2470 [ -600 2530 ] PLine
-600 2530 [ -400 2530 ] PLine
-400 2530 [ -400 2470 ] PLine
-400 2470 [ -600 2470 ] PLine
10 SetLine
-600 2500 [ -660 2500 ] PLine
10 SetLine
-400 2500 [ -350 2500 ] PLine
10 SetLine
-600 2070 [ -600 2130 ] PLine
-600 2130 [ -400 2130 ] PLine
-400 2130 [ -400 2070 ] PLine
-400 2070 [ -600 2070 ] PLine
10 SetLine
-600 2100 [ -660 2100 ] PLine
10 SetLine
-400 2100 [ -350 2100 ] PLine
10 SetLine
-900 2130 [ -900 2070 ] PLine
-900 2070 [ -1100 2070 ] PLine
-1100 2070 [ -1100 2130 ] PLine
-1100 2130 [ -900 2130 ] PLine
10 SetLine
-900 2100 [ -840 2100 ] PLine
10 SetLine
-1100 2100 [ -1150 2100 ] PLine
10 SetLine
500 1130 [ 500 1070 ] PLine
500 1070 [ 300 1070 ] PLine
300 1070 [ 300 1130 ] PLine
300 1130 [ 500 1130 ] PLine
10 SetLine
500 1100 [ 560 1100 ] PLine
10 SetLine
300 1100 [ 250 1100 ] PLine
10 SetLine
1000 2521 [ 1039 2560 ] PLine
10 SetLine
1000 2560 [ 1000 2140 ] PLine
1000 2140 [ 1300 2140 ] PLine
1300 2140 [ 1300 2560 ] PLine
1300 2560 [ 1000 2560 ] PLine
10 SetLine
0 1870 [ 0 1930 ] PLine
0 1930 [ 200 1930 ] PLine
200 1930 [ 200 1870 ] PLine
200 1870 [ 0 1870 ] PLine
10 SetLine
0 1900 [ -60 1900 ] PLine
10 SetLine
200 1900 [ 250 1900 ] PLine
10 SetLine
100 1470 [ 100 1530 ] PLine
100 1530 [ 300 1530 ] PLine
300 1530 [ 300 1470 ] PLine
300 1470 [ 100 1470 ] PLine
10 SetLine
100 1500 [ 40 1500 ] PLine
10 SetLine
300 1500 [ 350 1500 ] PLine
10 SetLine
-950 1650 [ -250 1650 ] PLine
-250 1650 [ -250 1850 ] PLine
-250 1850 [ -950 1850 ] PLine
-950 1850 [ -950 1775 ] PLine
-950 1775 [ -900 1775 ] PLine
-900 1775 [ -900 1725 ] PLine
-900 1725 [ -950 1725 ] PLine
-950 1725 [ -950 1650 ] PLine
10 SetLine
150 2250 [ 950 2250 ] PLine
950 2250 [ 950 2450 ] PLine
950 2450 [ 150 2450 ] PLine
150 2450 [ 150 2375 ] PLine
150 2375 [ 200 2375 ] PLine
200 2375 [ 200 2325 ] PLine
200 2325 [ 150 2325 ] PLine
150 2325 [ 150 2250 ] PLine
10 SetLine
150 3950 [ 1150 3950 ] PLine
1150 3950 [ 1150 4150 ] PLine
1150 4150 [ 150 4150 ] PLine
150 4150 [ 150 4075 ] PLine
150 4075 [ 200 4075 ] PLine
200 4075 [ 200 4025 ] PLine
200 4025 [ 150 4025 ] PLine
150 4025 [ 150 3950 ] PLine
10 SetLine
-1050 3150 [ -250 3150 ] PLine
-250 3150 [ -250 3350 ] PLine
-250 3350 [ -1050 3350 ] PLine
-1050 3350 [ -1050 3275 ] PLine
-1050 3275 [ -1000 3275 ] PLine
-1000 3275 [ -1000 3225 ] PLine
-1000 3225 [ -1050 3225 ] PLine
-1050 3225 [ -1050 3150 ] PLine
10 SetLine
800 1075 [ 800 1675 ] PLine
800 1675 [ 1200 1675 ] PLine
1200 1675 [ 1200 1075 ] PLine
1200 1075 [ 800 1075 ] PLine
10 SetLine
875 1075 [ 875 825 ] PLine
875 825 [ 925 825 ] PLine
925 825 [ 925 1075 ] PLine
10 SetLine
1075 1075 [ 1075 825 ] PLine
1075 825 [ 1125 825 ] PLine
1125 825 [ 1125 1075 ] PLine
10 SetLine
975 1075 [ 975 825 ] PLine
975 825 [ 1025 825 ] PLine
1025 825 [ 1025 1075 ] PLine
10 SetLine
996 1549 75 PCircle
10 SetLine
800 1425 [ 1200 1425 ] PLine
10 SetLine
-100 4200 [ -25 4200 ] PLine
10 SetLine
-100 3900 [ -100 4300 ] PLine
-100 4300 [ -500 4300 ] PLine
-500 4300 [ -500 3900 ] PLine
-500 3900 [ -100 3900 ] PLine
10 SetLine
-100 4000 [ -25 4000 ] PLine
10 SetLine
-1100 450 100 PCircle
10 SetLine
1100 450 100 PCircle
10 SetLine
1000 3430 [ 1000 3370 ] PLine
1000 3370 [ 800 3370 ] PLine
800 3370 [ 800 3430 ] PLine
800 3430 [ 1000 3430 ] PLine
10 SetLine
1000 3400 [ 1060 3400 ] PLine
10 SetLine
800 3400 [ 750 3400 ] PLine
10 SetText2
0 -1175 1225 [ [ -50 34 -56 31 -62 27 -65 22 -65 13 -62 9 -56 4 -50 2 -40 0 -25 0 -15 2 -9 4 -3 9 0 13 0 22 -3 27 -9 31 -15 34 ] ] Char
0 -1175 1279 [ [ -53 0 -56 4 -65 11 0 11 ] ] Char
0 -1175 1310 [ [ -65 29 -65 6 -37 4 -40 6 -43 13 -43 20 -40 27 -34 31 -25 34 -18 31 -9 29 -3 25 0 18 0 11 -3 4 -6 2 -12 0 ] ] Char
10 SetText2
0 75 3375 [ [ 34 50 31 56 27 62 22 65 13 65 9 62 4 56 2 50 0 40 0 25 2 15 4 9 9 3 13 0 22 0 27 3 31 9 34 15 ] ] Char
0 129 3375 [ [ 27 56 25 62 18 65 13 65 6 62 2 53 0 37 0 21 2 9 6 3 13 0 15 0 22 3 27 9 29 18 29 21 27 31 22 37 15 40 13 40 6 37 2 31 0 21 ] ] Char
10 SetText2
0 75 3175 [ [ 34 50 31 56 27 62 22 65 13 65 9 62 4 56 2 50 0 40 0 25 2 15 4 9 9 3 13 0 22 0 27 3 31 9 34 15 ] ] Char
0 129 3175 [ [ 31 65 9 0 ] [ 0 65 31 65 ] ] Char
10 SetText2
0 75 3575 [ [ 34 50 31 56 27 62 22 65 13 65 9 62 4 56 2 50 0 40 0 25 2 15 4 9 9 3 13 0 22 0 27 3 31 9 34 15 ] ] Char
0 129 3575 [ [ 29 65 6 65 4 37 6 40 13 43 20 43 27 40 31 34 34 25 31 18 29 9 25 3 18 0 11 0 4 3 2 6 0 12 ] ] Char
10 SetText2
0 -825 3850 [ [ 34 50 31 56 27 62 22 65 13 65 9 62 4 56 2 50 0 40 0 25 2 15 4 9 9 3 13 0 22 0 27 3 31 9 34 15 ] ] Char
0 -771 3850 [ [ 4 65 29 65 15 40 22 40 27 37 29 34 31 25 31 18 29 9 25 3 18 0 11 0 4 3 2 6 0 12 ] ] Char
10 SetText2
0 -575 1450 [ [ 34 50 31 56 27 62 22 65 13 65 9 62 4 56 2 50 0 40 0 25 2 15 4 9 9 3 13 0 22 0 27 3 31 9 34 15 ] ] Char
0 -521 1450 [ [ 0 53 4 56 11 65 11 0 ] ] Char
0 -490 1450 [ [ 27 56 25 62 18 65 13 65 6 62 2 53 0 37 0 21 2 9 6 3 13 0 15 0 22 3 27 9 29 18 29 21 27 31 22 37 15 40 13 40 6 37 2 31 0 21 ] ] Char
10 SetText2
0 125 2950 [ [ 34 50 31 56 27 62 22 65 13 65 9 62 4 56 2 50 0 40 0 25 2 15 4 9 9 3 13 0 22 0 27 3 31 9 34 15 ] ] Char
0 179 2950 [ [ 0 53 4 56 11 65 11 0 ] ] Char
0 210 2950 [ [ 2 50 2 53 4 59 6 62 11 65 20 65 25 62 27 59 29 53 29 46 27 40 22 31 0 0 31 0 ] ] Char
10 SetText2
0 -825 2950 [ [ 34 50 31 56 27 62 22 65 13 65 9 62 4 56 2 50 0 40 0 25 2 15 4 9 9 3 13 0 22 0 27 3 31 9 34 15 ] ] Char
0 -771 2950 [ [ 29 43 27 34 22 28 15 25 13 25 6 28 2 34 0 43 0 46 2 56 6 62 13 65 15 65 22 62 27 56 29 43 29 28 27 12 22 3 15 0 11 0 4 3 2 9 ] ] Char
10 SetText2
0 -100 2250 [ [ -50 34 -56 31 -62 27 -65 22 -65 13 -62 9 -56 4 -50 2 -40 0 -25 0 -15 2 -9 4 -3 9 0 13 0 22 -3 27 -9 31 -15 34 ] ] Char
0 -100 2304 [ [ -53 0 -56 4 -65 11 0 11 ] ] Char
0 -100 2335 [ [ -65 4 -65 29 -40 15 -40 22 -37 27 -34 29 -25 31 -18 31 -9 29 -3 25 0 18 0 11 -3 4 -6 2 -12 0 ] ] Char
10 SetText2
0 -1275 3200 [ [ -50 34 -56 31 -62 27 -65 22 -65 13 -62 9 -56 4 -50 2 -40 0 -25 0 -15 2 -9 4 -3 9 0 13 0 22 -3 27 -9 31 -15 34 ] ] Char
0 -1275 3254 [ [ -50 2 -53 2 -59 4 -62 6 -65 11 -65 20 -62 25 -59 27 -53 29 -46 29 -40 27 -31 22 0 0 0 31 ] ] Char
10 SetText2
0 -1100 2375 [ [ 34 50 31 56 27 62 22 65 13 65 9 62 4 56 2 50 0 40 0 25 2 15 4 9 9 3 13 0 22 0 27 3 31 9 34 15 ] ] Char
0 -1046 2375 [ [ 0 53 4 56 11 65 11 0 ] ] Char
0 -1015 2375 [ [ 0 53 4 56 11 65 11 0 ] ] Char
10 SetText2
0 -1100 2575 [ [ 34 50 31 56 27 62 22 65 13 65 9 62 4 56 2 50 0 40 0 25 2 15 4 9 9 3 13 0 22 0 27 3 31 9 34 15 ] ] Char
0 -1046 2575 [ [ 0 53 4 56 11 65 11 0 ] ] Char
0 -1015 2575 [ [ 13 65 6 62 2 53 0 37 0 28 2 12 6 3 13 0 18 0 25 3 29 12 31 28 31 37 29 53 25 62 18 65 13 65 ] ] Char
10 SetText2
0 900 2875 [ [ 34 50 31 56 27 62 22 65 13 65 9 62 4 56 2 50 0 40 0 25 2 15 4 9 9 3 13 0 22 0 27 3 31 9 34 15 ] ] Char
0 954 2875 [ [ 0 53 4 56 11 65 11 0 ] ] Char
0 985 2875 [ [ 22 65 0 21 34 21 ] [ 22 65 22 0 ] ] Char
10 SetText2
0 800 2050 [ [ 34 50 31 56 27 62 22 65 13 65 9 62 4 56 2 50 0 40 0 25 2 15 4 9 9 3 13 0 22 0 27 3 31 9 34 15 ] ] Char
0 854 2050 [ [ 0 53 4 56 11 65 11 0 ] ] Char
0 885 2050 [ [ 31 65 9 0 ] [ 0 65 31 65 ] ] Char
10 SetText2
0 -675 1100 [ [ 34 50 31 56 27 62 22 65 13 65 9 62 4 56 2 50 0 40 0 25 2 15 4 9 9 3 13 0 22 0 27 3 31 9 34 15 ] ] Char
0 -621 1100 [ [ 0 53 4 56 11 65 11 0 ] ] Char
0 -590 1100 [ [ 11 65 4 62 2 56 2 50 4 43 9 40 18 37 25 34 29 28 31 21 31 12 29 6 27 3 20 0 11 0 4 3 2 6 0 12 0 21 2 28 6 34 13 37 22 40 27 43 29 50 29 56 27 62 20 65 11 65 ] ] Char
10 SetText2
0 -925 4075 [ [ 34 50 31 56 27 62 22 65 13 65 9 62 4 56 2 50 0 40 0 25 2 15 4 9 9 3 13 0 22 0 27 3 31 9 34 15 ] ] Char
0 -871 4075 [ [ 0 53 4 56 11 65 11 0 ] ] Char
10 SetText2
0 875 3075 [ [ 34 50 31 56 27 62 22 65 13 65 9 62 4 56 2 50 0 40 0 25 2 15 4 9 9 3 13 0 22 0 27 3 31 9 34 15 ] ] Char
0 929 3075 [ [ 11 65 4 62 2 56 2 50 4 43 9 40 18 37 25 34 29 28 31 21 31 12 29 6 27 3 20 0 11 0 4 3 2 6 0 12 0 21 2 28 6 34 13 37 22 40 27 43 29 50 29 56 27 62 20 65 11 65 ] ] Char
10 SetText2
0 -200 3775 [ [ 34 50 31 56 27 62 22 65 13 65 9 62 4 56 2 50 0 40 0 25 2 15 4 9 9 3 13 0 22 0 27 3 31 9 34 15 ] ] Char
0 -146 3775 [ [ 22 65 0 21 34 21 ] [ 22 65 22 0 ] ] Char
10 SetText2
0 325 975 [ [ 0 65 0 0 ] [ 0 65 15 65 22 62 27 56 29 50 31 40 31 25 29 15 27 9 22 3 15 0 0 0 ] ] Char
0 377 975 [ [ 2 50 2 53 4 59 6 62 11 65 20 65 25 62 27 59 29 53 29 46 27 40 22 31 0 0 31 0 ] ] Char
10 SetText2
0 450 3775 [ [ 0 65 0 0 ] [ 0 65 15 65 22 62 27 56 29 50 31 40 31 25 29 15 27 9 22 3 15 0 0 0 ] ] Char
0 502 3775 [ [ 0 53 4 56 11 65 11 0 ] ] Char
10 SetText2
0 -775 675 [ [ 22 65 22 15 20 6 18 3 13 0 9 0 4 3 2 6 0 15 0 21 ] ] Char
0 -732 675 [ [ 2 50 2 53 4 59 6 62 11 65 20 65 25 62 27 59 29 53 29 46 27 40 22 31 0 0 31 0 ] ] Char
10 SetText2
0 -50 675 [ [ 22 65 22 15 20 6 18 3 13 0 9 0 4 3 2 6 0 15 0 21 ] ] Char
0 -7 675 [ [ 4 65 29 65 15 40 22 40 27 37 29 34 31 25 31 18 29 9 25 3 18 0 11 0 4 3 2 6 0 12 ] ] Char
10 SetText2
0 700 675 [ [ 22 65 22 15 20 6 18 3 13 0 9 0 4 3 2 6 0 15 0 21 ] ] Char
0 743 675 [ [ 22 65 0 21 34 21 ] [ 22 65 22 0 ] ] Char
10 SetText2
0 -1175 4650 [ [ 22 65 22 15 20 6 18 3 13 0 9 0 4 3 2 6 0 15 0 21 ] ] Char
0 -1132 4650 [ [ 0 53 4 56 11 65 11 0 ] ] Char
10 SetText2
0 1125 125 [ [ 0 65 0 0 27 0 ] ] Char
0 1172 125 [ [ 0 65 0 0 ] [ 0 65 29 65 ] [ 0 34 18 34 ] [ 0 0 29 0 ] ] Char
0 1222 125 [ [ 0 65 0 0 ] [ 0 65 15 65 22 62 27 56 29 50 31 40 31 25 29 15 27 9 22 3 15 0 0 0 ] ] Char
0 1274 125 [ [ 4 65 29 65 15 40 22 40 27 37 29 34 31 25 31 18 29 9 25 3 18 0 11 0 4 3 2 6 0 12 ] ] Char
10 SetText2
0 125 125 [ [ 0 65 0 0 27 0 ] ] Char
0 172 125 [ [ 0 65 0 0 ] [ 0 65 29 65 ] [ 0 34 18 34 ] [ 0 0 29 0 ] ] Char
0 222 125 [ [ 0 65 0 0 ] [ 0 65 15 65 22 62 27 56 29 50 31 40 31 25 29 15 27 9 22 3 15 0 0 0 ] ] Char
0 274 125 [ [ 2 50 2 53 4 59 6 62 11 65 20 65 25 62 27 59 29 53 29 46 27 40 22 31 0 0 31 0 ] ] Char
10 SetText2
0 -875 125 [ [ 0 65 0 0 27 0 ] ] Char
0 -828 125 [ [ 0 65 0 0 ] [ 0 65 29 65 ] [ 0 34 18 34 ] [ 0 0 29 0 ] ] Char
0 -778 125 [ [ 0 65 0 0 ] [ 0 65 15 65 22 62 27 56 29 50 31 40 31 25 29 15 27 9 22 3 15 0 0 0 ] ] Char
0 -726 125 [ [ 0 53 4 56 11 65 11 0 ] ] Char
10 SetText2
0 1075 3425 [ [ 13 65 9 62 4 56 2 50 0 40 0 25 2 15 4 9 9 3 13 0 22 0 27 3 31 9 34 15 36 25 36 40 34 50 31 56 27 62 22 65 13 65 ] [ 20 12 34 -6 ] ] Char
0 1131 3425 [ [ 0 53 4 56 11 65 11 0 ] ] Char
10 SetText2
0 -1075 1475 [ [ 0 -65 0 0 ] [ 0 -65 -20 -65 -27 -62 -29 -59 -31 -53 -31 -46 -29 -40 -27 -37 -20 -34 0 -34 ] [ -15 -34 -31 0 ] ] Char
0 -1127 1475 [ [ -11 -65 -4 -62 -2 -56 -2 -50 -4 -43 -9 -40 -18 -37 -25 -34 -29 -28 -31 -21 -31 -12 -29 -6 -27 -3 -20 0 -11 0 -4 -3 -2 -6 0 -12 0 -21 -2 -28 -6 -34 -13 -37 -22 -40 -27 -43 -29 -50 -29 -56 -27 -62 -20 -65 -11 -65 ] ] Char
10 SetText2
0 25 1750 [ [ 0 65 0 0 ] [ 0 65 20 65 27 62 29 59 31 53 31 46 29 40 27 37 20 34 0 34 ] [ 15 34 31 0 ] ] Char
0 77 1750 [ [ 0 53 4 56 11 65 11 0 ] ] Char
0 108 1750 [ [ 13 65 6 62 2 53 0 37 0 28 2 12 6 3 13 0 18 0 25 3 29 12 31 28 31 37 29 53 25 62 18 65 13 65 ] ] Char
10 SetText2
0 350 1350 [ [ 0 65 0 0 ] [ 0 65 20 65 27 62 29 59 31 53 31 46 29 40 27 37 20 34 0 34 ] [ 15 34 31 0 ] ] Char
0 402 1350 [ [ 0 53 4 56 11 65 11 0 ] ] Char
0 433 1350 [ [ 2 50 2 53 4 59 6 62 11 65 20 65 25 62 27 59 29 53 29 46 27 40 22 31 0 0 31 0 ] ] Char
10 SetText2
0 -550 2350 [ [ 0 65 0 0 ] [ 0 65 20 65 27 62 29 59 31 53 31 46 29 40 27 37 20 34 0 34 ] [ 15 34 31 0 ] ] Char
0 -498 2350 [ [ 22 65 0 21 34 21 ] [ 22 65 22 0 ] ] Char
10 SetText2
0 -925 4250 [ [ 0 65 0 0 ] [ 0 65 20 65 27 62 29 59 31 53 31 46 29 40 27 37 20 34 0 34 ] [ 15 34 31 0 ] ] Char
0 -873 4250 [ [ 0 53 4 56 11 65 11 0 ] ] Char
10 SetText2
0 850 3250 [ [ 0 65 0 0 ] [ 0 65 20 65 27 62 29 59 31 53 31 46 29 40 27 37 20 34 0 34 ] [ 15 34 31 0 ] ] Char
0 902 3250 [ [ 2 50 2 53 4 59 6 62 11 65 20 65 25 62 27 59 29 53 29 46 27 40 22 31 0 0 31 0 ] ] Char
10 SetText2
0 -550 2550 [ [ 0 65 0 0 ] [ 0 65 20 65 27 62 29 59 31 53 31 46 29 40 27 37 20 34 0 34 ] [ 15 34 31 0 ] ] Char
0 -498 2550 [ [ 4 65 29 65 15 40 22 40 27 37 29 34 31 25 31 18 29 9 25 3 18 0 11 0 4 3 2 6 0 12 ] ] Char
10 SetText2
0 -550 2150 [ [ 0 65 0 0 ] [ 0 65 20 65 27 62 29 59 31 53 31 46 29 40 27 37 20 34 0 34 ] [ 15 34 31 0 ] ] Char
0 -498 2150 [ [ 27 56 25 62 18 65 13 65 6 62 2 53 0 37 0 21 2 9 6 3 13 0 15 0 22 3 27 9 29 18 29 21 27 31 22 37 15 40 13 40 6 37 2 31 0 21 ] ] Char
10 SetText2
0 -1025 2150 [ [ 0 65 0 0 ] [ 0 65 20 65 27 62 29 59 31 53 31 46 29 40 27 37 20 34 0 34 ] [ 15 34 31 0 ] ] Char
0 -973 2150 [ [ 29 65 6 65 4 37 6 40 13 43 20 43 27 40 31 34 34 25 31 18 29 9 25 3 18 0 11 0 4 3 2 6 0 12 ] ] Char
10 SetText2
0 350 1150 [ [ 0 65 0 0 ] [ 0 65 20 65 27 62 29 59 31 53 31 46 29 40 27 37 20 34 0 34 ] [ 15 34 31 0 ] ] Char
0 402 1150 [ [ 0 53 4 56 11 65 11 0 ] ] Char
0 433 1150 [ [ 4 65 29 65 15 40 22 40 27 37 29 34 31 25 31 18 29 9 25 3 18 0 11 0 4 3 2 6 0 12 ] ] Char
10 SetText2
0 1200 2125 [ [ 0 -65 0 0 ] [ 0 -65 -20 -65 -27 -62 -29 -59 -31 -53 -31 -46 -29 -40 -27 -37 -20 -34 0 -34 ] [ -15 -34 -31 0 ] ] Char
0 1148 2125 [ [ -31 -65 -9 0 ] [ 0 -65 -31 -65 ] ] Char
10 SetText2
0 50 1950 [ [ 0 65 0 0 ] [ 0 65 20 65 27 62 29 59 31 53 31 46 29 40 27 37 20 34 0 34 ] [ 15 34 31 0 ] ] Char
0 102 1950 [ [ 29 43 27 34 22 28 15 25 13 25 6 28 2 34 0 43 0 46 2 56 6 62 13 65 15 65 22 62 27 56 29 43 29 28 27 12 22 3 15 0 11 0 4 3 2 9 ] ] Char
10 SetText2
0 150 1550 [ [ 0 65 0 0 ] [ 0 65 20 65 27 62 29 59 31 53 31 46 29 40 27 37 20 34 0 34 ] [ 15 34 31 0 ] ] Char
0 202 1550 [ [ 0 53 4 56 11 65 11 0 ] ] Char
0 233 1550 [ [ 0 53 4 56 11 65 11 0 ] ] Char
10 SetText2
0 -675 1950 [ [ 0 65 0 18 2 9 6 3 13 0 18 0 25 3 29 9 31 18 31 65 ] ] Char
0 -623 1950 [ [ 22 65 0 21 34 21 ] [ 22 65 22 0 ] ] Char
10 SetText2
0 450 2550 [ [ 0 65 0 18 2 9 6 3 13 0 18 0 25 3 29 9 31 18 31 65 ] ] Char
0 502 2550 [ [ 4 65 29 65 15 40 22 40 27 37 29 34 31 25 31 18 29 9 25 3 18 0 11 0 4 3 2 6 0 12 ] ] Char
10 SetText2
0 500 4275 [ [ 0 65 0 18 2 9 6 3 13 0 18 0 25 3 29 9 31 18 31 65 ] ] Char
0 552 4275 [ [ 2 50 2 53 4 59 6 62 11 65 20 65 25 62 27 59 29 53 29 46 27 40 22 31 0 0 31 0 ] ] Char
10 SetText2
0 -675 3450 [ [ 0 65 0 18 2 9 6 3 13 0 18 0 25 3 29 9 31 18 31 65 ] ] Char
0 -623 3450 [ [ 0 53 4 56 11 65 11 0 ] ] Char
10 SetText2
0 950 1700 [ [ 0 65 0 18 2 9 6 3 13 0 18 0 25 3 29 9 31 18 31 65 ] ] Char
0 1002 1700 [ [ 29 65 6 65 4 37 6 40 13 43 20 43 27 40 31 34 34 25 31 18 29 9 25 3 18 0 11 0 4 3 2 6 0 12 ] ] Char
10 SetText2
0 -350 4325 [ [ 0 65 31 0 ] [ 31 65 0 0 ] ] Char
0 -298 4325 [ [ 0 53 4 56 11 65 11 0 ] ] Char
10 SetText2
0 -1225 600 [ [ 0 65 0 0 ] [ 0 65 18 0 ] [ 36 65 18 0 ] [ 36 65 36 0 ] ] Char
0 -1169 600 [ [ 0 65 0 0 ] [ 31 65 31 0 ] [ 0 34 31 34 ] ] Char
0 -1117 600 [ [ 0 53 4 56 11 65 11 0 ] ] Char
10 SetText2
0 1125 600 [ [ 0 65 0 0 ] [ 0 65 18 0 ] [ 36 65 18 0 ] [ 36 65 36 0 ] ] Char
0 1181 600 [ [ 0 65 0 0 ] [ 31 65 31 0 ] [ 0 34 31 34 ] ] Char
0 1233 600 [ [ 2 50 2 53 4 59 6 62 11 65 20 65 25 62 27 59 29 53 29 46 27 40 22 31 0 0 31 0 ] ] Char
10 SetText2
0 800 3450 [ [ 0 65 0 0 ] [ 0 65 20 65 27 62 29 59 31 53 31 46 29 40 27 37 20 34 0 34 ] [ 15 34 31 0 ] ] Char
0 852 3450 [ [ 0 53 4 56 11 65 11 0 ] ] Char
0 883 3450 [ [ 22 65 0 21 34 21 ] [ 22 65 22 0 ] ] Char
10 SetText2
0 -1075 1225 [ [ -65 0 0 0 ] [ -65 0 -65 15 -62 22 -56 27 -50 29 -40 31 -25 31 -15 29 -9 27 -3 22 0 15 0 0 ] ] Char
0 -1075 1277 [ [ -50 34 -56 31 -62 27 -65 22 -65 13 -62 9 -56 4 -50 2 -40 0 -25 0 -15 2 -9 4 -3 9 0 13 0 22 -3 27 -9 31 -15 34 ] ] Char
0 -1075 1331 [ [ -65 18 0 0 ] [ -65 18 0 36 ] [ -21 6 -21 29 ] ] Char
0 -1075 1387 [ [ -65 0 0 0 ] [ -65 0 -65 20 -62 27 -59 29 -53 31 -43 31 -37 29 -34 27 -31 20 -31 0 ] ] Char
10 SetText2
0 75 3275 [ [ 0 65 0 0 ] [ 0 65 15 65 22 62 27 56 29 50 31 40 31 25 29 15 27 9 22 3 15 0 0 0 ] ] Char
0 127 3275 [ [ 34 50 31 56 27 62 22 65 13 65 9 62 4 56 2 50 0 40 0 25 2 15 4 9 9 3 13 0 22 0 27 3 31 9 34 15 ] ] Char
0 181 3275 [ [ 18 65 0 0 ] [ 18 65 36 0 ] [ 6 21 29 21 ] ] Char
0 237 3275 [ [ 0 65 0 0 ] [ 0 65 20 65 27 62 29 59 31 53 31 43 29 37 27 34 20 31 0 31 ] ] Char
0 289 3275 [ [ 0 65 0 0 ] [ 0 65 29 65 ] [ 0 34 18 34 ] [ 0 0 29 0 ] ] Char
10 SetText2
0 75 3075 [ [ 0 65 0 0 ] [ 0 65 15 65 22 62 27 56 29 50 31 40 31 25 29 15 27 9 22 3 15 0 0 0 ] ] Char
0 127 3075 [ [ 34 50 31 56 27 62 22 65 13 65 9 62 4 56 2 50 0 40 0 25 2 15 4 9 9 3 13 0 22 0 27 3 31 9 34 15 ] ] Char
0 181 3075 [ [ 18 65 0 0 ] [ 18 65 36 0 ] [ 6 21 29 21 ] ] Char
0 237 3075 [ [ 0 65 0 0 ] [ 0 65 20 65 27 62 29 59 31 53 31 43 29 37 27 34 20 31 0 31 ] ] Char
0 289 3075 [ [ 0 65 0 0 ] [ 0 65 29 65 ] [ 0 34 18 34 ] [ 0 0 29 0 ] ] Char
10 SetText2
0 75 3475 [ [ 0 65 0 0 ] [ 0 65 15 65 22 62 27 56 29 50 31 40 31 25 29 15 27 9 22 3 15 0 0 0 ] ] Char
0 127 3475 [ [ 34 50 31 56 27 62 22 65 13 65 9 62 4 56 2 50 0 40 0 25 2 15 4 9 9 3 13 0 22 0 27 3 31 9 34 15 ] ] Char
0 181 3475 [ [ 18 65 0 0 ] [ 18 65 36 0 ] [ 6 21 29 21 ] ] Char
0 237 3475 [ [ 0 65 0 0 ] [ 0 65 20 65 27 62 29 59 31 53 31 43 29 37 27 34 20 31 0 31 ] ] Char
0 289 3475 [ [ 0 65 0 0 ] [ 0 65 29 65 ] [ 0 34 18 34 ] [ 0 0 29 0 ] ] Char
10 SetText2
0 -825 3750 [ [ 34 50 31 56 27 62 22 65 13 65 9 62 4 56 2 50 0 40 0 25 2 15 4 9 9 3 13 0 22 0 27 3 31 9 34 15 ] ] Char
0 -771 3750 [ [ 18 65 0 0 ] [ 18 65 36 0 ] [ 6 21 29 21 ] ] Char
0 -715 3750 [ [ 0 65 0 0 ] [ 0 65 20 65 27 62 29 59 31 53 31 43 29 37 27 34 20 31 0 31 ] ] Char
0 -663 3750 [ [ 0 78 40 -21 ] ] Char
0 -602 3750 [ [ 0 65 0 0 ] [ 0 65 18 0 ] [ 36 65 18 0 ] [ 36 65 36 0 ] ] Char
0 -546 3750 [ [ 18 65 0 0 ] [ 18 65 36 0 ] [ 6 21 29 21 ] ] Char
0 -490 3750 [ [ 29 65 6 65 4 37 6 40 13 43 20 43 27 40 31 34 34 25 31 18 29 9 25 3 18 0 11 0 4 3 2 6 0 12 ] ] Char
0 -436 3750 [ [ 13 65 6 62 2 53 0 37 0 28 2 12 6 3 13 0 18 0 25 3 29 12 31 28 31 37 29 53 25 62 18 65 13 65 ] ] Char
10 SetText2
0 -575 1350 [ [ 34 50 31 56 27 62 22 65 13 65 9 62 4 56 2 50 0 40 0 25 2 15 4 9 9 3 13 0 22 0 27 3 31 9 34 15 ] ] Char
0 -521 1350 [ [ 18 65 0 0 ] [ 18 65 36 0 ] [ 6 21 29 21 ] ] Char
0 -465 1350 [ [ 0 65 0 0 ] [ 0 65 20 65 27 62 29 59 31 53 31 43 29 37 27 34 20 31 0 31 ] ] Char
0 -413 1350 [ [ 0 78 40 -21 ] ] Char
0 -352 1350 [ [ 0 65 0 0 ] [ 0 65 18 0 ] [ 36 65 18 0 ] [ 36 65 36 0 ] ] Char
0 -296 1350 [ [ 18 65 0 0 ] [ 18 65 36 0 ] [ 6 21 29 21 ] ] Char
0 -240 1350 [ [ 29 65 6 65 4 37 6 40 13 43 20 43 27 40 31 34 34 25 31 18 29 9 25 3 18 0 11 0 4 3 2 6 0 12 ] ] Char
0 -186 1350 [ [ 13 65 6 62 2 53 0 37 0 28 2 12 6 3 13 0 18 0 25 3 29 12 31 28 31 37 29 53 25 62 18 65 13 65 ] ] Char
10 SetText2
0 125 2850 [ [ 34 50 31 56 27 62 22 65 13 65 9 62 4 56 2 50 0 40 0 25 2 15 4 9 9 3 13 0 22 0 27 3 31 9 34 15 ] ] Char
0 179 2850 [ [ 18 65 0 0 ] [ 18 65 36 0 ] [ 6 21 29 21 ] ] Char
0 235 2850 [ [ 0 65 0 0 ] [ 0 65 20 65 27 62 29 59 31 53 31 43 29 37 27 34 20 31 0 31 ] ] Char
0 287 2850 [ [ 0 78 40 -21 ] ] Char
0 348 2850 [ [ 0 65 0 0 ] [ 0 65 18 0 ] [ 36 65 18 0 ] [ 36 65 36 0 ] ] Char
0 404 2850 [ [ 18 65 0 0 ] [ 18 65 36 0 ] [ 6 21 29 21 ] ] Char
0 460 2850 [ [ 29 65 6 65 4 37 6 40 13 43 20 43 27 40 31 34 34 25 31 18 29 9 25 3 18 0 11 0 4 3 2 6 0 12 ] ] Char
0 514 2850 [ [ 13 65 6 62 2 53 0 37 0 28 2 12 6 3 13 0 18 0 25 3 29 12 31 28 31 37 29 53 25 62 18 65 13 65 ] ] Char
10 SetText2
0 -825 2850 [ [ 34 50 31 56 27 62 22 65 13 65 9 62 4 56 2 50 0 40 0 25 2 15 4 9 9 3 13 0 22 0 27 3 31 9 34 15 ] ] Char
0 -771 2850 [ [ 18 65 0 0 ] [ 18 65 36 0 ] [ 6 21 29 21 ] ] Char
0 -715 2850 [ [ 0 65 0 0 ] [ 0 65 20 65 27 62 29 59 31 53 31 43 29 37 27 34 20 31 0 31 ] ] Char
0 -663 2850 [ [ 0 78 40 -21 ] ] Char
0 -602 2850 [ [ 0 65 0 0 ] [ 0 65 18 0 ] [ 36 65 18 0 ] [ 36 65 36 0 ] ] Char
0 -546 2850 [ [ 18 65 0 0 ] [ 18 65 36 0 ] [ 6 21 29 21 ] ] Char
0 -490 2850 [ [ 29 65 6 65 4 37 6 40 13 43 20 43 27 40 31 34 34 25 31 18 29 9 25 3 18 0 11 0 4 3 2 6 0 12 ] ] Char
0 -436 2850 [ [ 13 65 6 62 2 53 0 37 0 28 2 12 6 3 13 0 18 0 25 3 29 12 31 28 31 37 29 53 25 62 18 65 13 65 ] ] Char
10 SetText2
0 0 2250 [ [ -65 0 0 0 ] [ -65 0 -65 15 -62 22 -56 27 -50 29 -40 31 -25 31 -15 29 -9 27 -3 22 0 15 0 0 ] ] Char
0 0 2302 [ [ -50 34 -56 31 -62 27 -65 22 -65 13 -62 9 -56 4 -50 2 -40 0 -25 0 -15 2 -9 4 -3 9 0 13 0 22 -3 27 -9 31 -15 34 ] ] Char
0 0 2356 [ [ -65 18 0 0 ] [ -65 18 0 36 ] [ -21 6 -21 29 ] ] Char
0 0 2412 [ [ -65 0 0 0 ] [ -65 0 -65 20 -62 27 -59 29 -53 31 -43 31 -37 29 -34 27 -31 20 -31 0 ] ] Char
0 0 2464 [ [ -65 0 0 0 ] [ -65 0 -65 29 ] [ -34 0 -34 18 ] [ 0 0 0 29 ] ] Char
10 SetText2
0 -1175 3200 [ [ -65 0 0 0 ] [ -65 0 -65 15 -62 22 -56 27 -50 29 -40 31 -25 31 -15 29 -9 27 -3 22 0 15 0 0 ] ] Char
0 -1175 3252 [ [ -50 34 -56 31 -62 27 -65 22 -65 13 -62 9 -56 4 -50 2 -40 0 -25 0 -15 2 -9 4 -3 9 0 13 0 22 -3 27 -9 31 -15 34 ] ] Char
0 -1175 3306 [ [ -65 18 0 0 ] [ -65 18 0 36 ] [ -21 6 -21 29 ] ] Char
0 -1175 3362 [ [ -65 0 0 0 ] [ -65 0 -65 20 -62 27 -59 29 -53 31 -43 31 -37 29 -34 27 -31 20 -31 0 ] ] Char
0 -1175 3414 [ [ -65 0 0 0 ] [ -65 0 -65 29 ] [ -34 0 -34 18 ] [ 0 0 0 29 ] ] Char
10 SetText2
0 -1100 2275 [ [ 0 65 0 0 ] [ 0 65 15 65 22 62 27 56 29 50 31 40 31 25 29 15 27 9 22 3 15 0 0 0 ] ] Char
0 -1048 2275 [ [ 34 50 31 56 27 62 22 65 13 65 9 62 4 56 2 50 0 40 0 25 2 15 4 9 9 3 13 0 22 0 27 3 31 9 34 15 ] ] Char
0 -994 2275 [ [ 18 65 0 0 ] [ 18 65 36 0 ] [ 6 21 29 21 ] ] Char
0 -938 2275 [ [ 0 65 0 0 ] [ 0 65 20 65 27 62 29 59 31 53 31 43 29 37 27 34 20 31 0 31 ] ] Char
10 SetText2
0 -1100 2475 [ [ 0 65 0 0 ] [ 0 65 15 65 22 62 27 56 29 50 31 40 31 25 29 15 27 9 22 3 15 0 0 0 ] ] Char
0 -1048 2475 [ [ 34 50 31 56 27 62 22 65 13 65 9 62 4 56 2 50 0 40 0 25 2 15 4 9 9 3 13 0 22 0 27 3 31 9 34 15 ] ] Char
0 -994 2475 [ [ 18 65 0 0 ] [ 18 65 36 0 ] [ 6 21 29 21 ] ] Char
0 -938 2475 [ [ 0 65 0 0 ] [ 0 65 20 65 27 62 29 59 31 53 31 43 29 37 27 34 20 31 0 31 ] ] Char
10 SetText2
0 900 2775 [ [ 0 65 0 0 ] [ 0 65 15 65 22 62 27 56 29 50 31 40 31 25 29 15 27 9 22 3 15 0 0 0 ] ] Char
0 952 2775 [ [ 34 50 31 56 27 62 22 65 13 65 9 62 4 56 2 50 0 40 0 25 2 15 4 9 9 3 13 0 22 0 27 3 31 9 34 15 ] ] Char
0 1006 2775 [ [ 18 65 0 0 ] [ 18 65 36 0 ] [ 6 21 29 21 ] ] Char
0 1062 2775 [ [ 0 65 0 0 ] [ 0 65 20 65 27 62 29 59 31 53 31 43 29 37 27 34 20 31 0 31 ] ] Char
10 SetText2
0 800 1950 [ [ 34 50 31 56 27 62 22 65 13 65 9 62 4 56 2 50 0 40 0 25 2 15 4 9 9 3 13 0 22 0 27 3 31 9 34 15 ] ] Char
0 854 1950 [ [ 18 65 0 0 ] [ 18 65 36 0 ] [ 6 21 29 21 ] ] Char
0 910 1950 [ [ 0 65 0 0 ] [ 0 65 20 65 27 62 29 59 31 53 31 43 29 37 27 34 20 31 0 31 ] ] Char
0 962 1950 [ [ 0 78 40 -21 ] ] Char
0 1023 1950 [ [ 0 65 0 0 ] [ 0 65 18 0 ] [ 36 65 18 0 ] [ 36 65 36 0 ] ] Char
0 1079 1950 [ [ 18 65 0 0 ] [ 18 65 36 0 ] [ 6 21 29 21 ] ] Char
0 1135 1950 [ [ 29 65 6 65 4 37 6 40 13 43 20 43 27 40 31 34 34 25 31 18 29 9 25 3 18 0 11 0 4 3 2 6 0 12 ] ] Char
0 1189 1950 [ [ 13 65 6 62 2 53 0 37 0 28 2 12 6 3 13 0 18 0 25 3 29 12 31 28 31 37 29 53 25 62 18 65 13 65 ] ] Char
10 SetText2
0 -675 1000 [ [ 34 50 31 56 27 62 22 65 13 65 9 62 4 56 2 50 0 40 0 25 2 15 4 9 9 3 13 0 22 0 27 3 31 9 34 15 ] ] Char
0 -621 1000 [ [ 18 65 0 0 ] [ 18 65 36 0 ] [ 6 21 29 21 ] ] Char
0 -565 1000 [ [ 0 65 0 0 ] [ 0 65 20 65 27 62 29 59 31 53 31 43 29 37 27 34 20 31 0 31 ] ] Char
0 -513 1000 [ [ 0 78 40 -21 ] ] Char
0 -452 1000 [ [ 0 65 0 0 ] [ 0 65 18 0 ] [ 36 65 18 0 ] [ 36 65 36 0 ] ] Char
0 -396 1000 [ [ 18 65 0 0 ] [ 18 65 36 0 ] [ 6 21 29 21 ] ] Char
0 -340 1000 [ [ 27 56 25 62 18 65 13 65 6 62 2 53 0 37 0 21 2 9 6 3 13 0 15 0 22 3 27 9 29 18 29 21 27 31 22 37 15 40 13 40 6 37 2 31 0 21 ] ] Char
0 -290 1000 [ [ 13 65 6 62 2 53 0 37 0 28 2 12 6 3 13 0 18 0 25 3 29 12 31 28 31 37 29 53 25 62 18 65 13 65 ] ] Char
10 SetText2
0 -925 3975 [ [ 0 65 0 0 ] [ 0 65 15 65 22 62 27 56 29 50 31 40 31 25 29 15 27 9 22 3 15 0 0 0 ] ] Char
0 -873 3975 [ [ 34 50 31 56 27 62 22 65 13 65 9 62 4 56 2 50 0 40 0 25 2 15 4 9 9 3 13 0 22 0 27 3 31 9 34 15 ] ] Char
0 -819 3975 [ [ 18 65 0 0 ] [ 18 65 36 0 ] [ 6 21 29 21 ] ] Char
0 -763 3975 [ [ 0 65 0 0 ] [ 0 65 20 65 27 62 29 59 31 53 31 43 29 37 27 34 20 31 0 31 ] ] Char
0 -711 3975 [ [ 0 65 0 0 ] [ 0 65 29 65 ] [ 0 34 18 34 ] [ 0 0 29 0 ] ] Char
10 SetText2
0 875 2975 [ [ 0 65 0 0 ] [ 0 65 15 65 22 62 27 56 29 50 31 40 31 25 29 15 27 9 22 3 15 0 0 0 ] ] Char
0 927 2975 [ [ 34 50 31 56 27 62 22 65 13 65 9 62 4 56 2 50 0 40 0 25 2 15 4 9 9 3 13 0 22 0 27 3 31 9 34 15 ] ] Char
0 981 2975 [ [ 18 65 0 0 ] [ 18 65 36 0 ] [ 6 21 29 21 ] ] Char
0 1037 2975 [ [ 0 65 0 0 ] [ 0 65 20 65 27 62 29 59 31 53 31 43 29 37 27 34 20 31 0 31 ] ] Char
0 1089 2975 [ [ 0 65 0 0 ] [ 0 65 29 65 ] [ 0 34 18 34 ] [ 0 0 29 0 ] ] Char
10 SetText2
0 -200 3675 [ [ 0 65 0 0 ] [ 0 65 15 65 22 62 27 56 29 50 31 40 31 25 29 15 27 9 22 3 15 0 0 0 ] ] Char
0 -148 3675 [ [ 34 50 31 56 27 62 22 65 13 65 9 62 4 56 2 50 0 40 0 25 2 15 4 9 9 3 13 0 22 0 27 3 31 9 34 15 ] ] Char
0 -94 3675 [ [ 18 65 0 0 ] [ 18 65 36 0 ] [ 6 21 29 21 ] ] Char
0 -38 3675 [ [ 0 65 0 0 ] [ 0 65 20 65 27 62 29 59 31 53 31 43 29 37 27 34 20 31 0 31 ] ] Char
10 SetText2
0 325 875 [ [ 0 65 0 0 ] [ 0 65 15 65 22 62 27 56 29 50 31 40 31 25 29 15 27 9 22 3 15 0 0 0 ] ] Char
0 377 875 [ [ 0 65 0 0 ] ] Char
0 397 875 [ [ 13 65 9 62 4 56 2 50 0 40 0 25 2 15 4 9 9 3 13 0 22 0 27 3 31 9 34 15 36 25 36 40 34 50 31 56 27 62 22 65 13 65 ] ] Char
0 453 875 [ [ 0 65 0 0 ] [ 0 65 15 65 22 62 27 56 29 50 31 40 31 25 29 15 27 9 22 3 15 0 0 0 ] ] Char
0 505 875 [ [ 0 65 0 0 ] [ 0 65 29 65 ] [ 0 34 18 34 ] [ 0 0 29 0 ] ] Char
10 SetText2
0 450 3675 [ [ 0 65 0 0 ] [ 0 65 15 65 22 62 27 56 29 50 31 40 31 25 29 15 27 9 22 3 15 0 0 0 ] ] Char
0 502 3675 [ [ 0 65 0 0 ] ] Char
0 522 3675 [ [ 13 65 9 62 4 56 2 50 0 40 0 25 2 15 4 9 9 3 13 0 22 0 27 3 31 9 34 15 36 25 36 40 34 50 31 56 27 62 22 65 13 65 ] ] Char
0 578 3675 [ [ 0 65 0 0 ] [ 0 65 15 65 22 62 27 56 29 50 31 40 31 25 29 15 27 9 22 3 15 0 0 0 ] ] Char
0 630 3675 [ [ 0 65 0 0 ] [ 0 65 29 65 ] [ 0 34 18 34 ] [ 0 0 29 0 ] ] Char
10 SetText2
0 -775 575 [ [ 0 65 0 0 ] [ 0 65 15 65 22 62 27 56 29 50 31 40 31 25 29 15 27 9 22 3 15 0 0 0 ] ] Char
0 -723 575 [ [ 0 65 0 0 ] [ 0 65 20 65 27 62 29 59 31 53 31 46 29 40 27 37 20 34 ] [ 0 34 20 34 27 31 29 28 31 21 31 12 29 6 27 3 20 0 0 0 ] ] Char
0 -671 575 [ [ 0 65 0 0 ] [ 0 65 31 0 ] [ 31 65 31 0 ] ] Char
0 -619 575 [ [ 34 50 31 56 27 62 22 65 13 65 9 62 4 56 2 50 0 40 0 25 2 15 4 9 9 3 13 0 22 0 27 3 31 9 34 15 ] ] Char
10 SetText2
0 -50 575 [ [ 0 65 0 0 ] [ 0 65 15 65 22 62 27 56 29 50 31 40 31 25 29 15 27 9 22 3 15 0 0 0 ] ] Char
0 2 575 [ [ 0 65 0 0 ] [ 0 65 20 65 27 62 29 59 31 53 31 46 29 40 27 37 20 34 ] [ 0 34 20 34 27 31 29 28 31 21 31 12 29 6 27 3 20 0 0 0 ] ] Char
0 54 575 [ [ 0 65 0 0 ] [ 0 65 31 0 ] [ 31 65 31 0 ] ] Char
0 106 575 [ [ 34 50 31 56 27 62 22 65 13 65 9 62 4 56 2 50 0 40 0 25 2 15 4 9 9 3 13 0 22 0 27 3 31 9 34 15 ] ] Char
10 SetText2
0 700 575 [ [ 0 65 0 0 ] [ 0 65 15 65 22 62 27 56 29 50 31 40 31 25 29 15 27 9 22 3 15 0 0 0 ] ] Char
0 752 575 [ [ 0 65 0 0 ] [ 0 65 20 65 27 62 29 59 31 53 31 46 29 40 27 37 20 34 ] [ 0 34 20 34 27 31 29 28 31 21 31 12 29 6 27 3 20 0 0 0 ] ] Char
0 804 575 [ [ 0 65 0 0 ] [ 0 65 31 0 ] [ 31 65 31 0 ] ] Char
0 856 575 [ [ 34 50 31 56 27 62 22 65 13 65 9 62 4 56 2 50 0 40 0 25 2 15 4 9 9 3 13 0 22 0 27 3 31 9 34 15 ] ] Char
10 SetText2
0 -1175 4550 [ [ 34 50 31 56 27 62 22 65 13 65 9 62 4 56 2 50 0 40 0 25 2 15 4 9 9 3 13 0 22 0 27 3 31 9 34 15 ] ] Char
0 -1121 4550 [ [ 13 65 9 62 4 56 2 50 0 40 0 25 2 15 4 9 9 3 13 0 22 0 27 3 31 9 34 15 36 25 36 40 34 50 31 56 27 62 22 65 13 65 ] ] Char
0 -1065 4550 [ [ 0 65 0 0 ] [ 0 65 31 0 ] [ 31 65 31 0 ] ] Char
0 -1013 4550 [ [ 0 78 40 -21 ] ] Char
0 -952 4550 [ [ 0 65 0 0 ] [ 0 65 15 65 22 62 27 56 29 50 31 40 31 25 29 15 27 9 22 3 15 0 0 0 ] ] Char
0 -900 4550 [ [ 0 65 0 0 ] [ 0 65 20 65 27 62 29 59 31 53 31 46 29 40 27 37 20 34 ] [ 0 34 20 34 27 31 29 28 31 21 31 12 29 6 27 3 20 0 0 0 ] ] Char
0 -848 4550 [ [ 2 50 2 53 4 59 6 62 11 65 20 65 25 62 27 59 29 53 29 46 27 40 22 31 0 0 31 0 ] ] Char
0 -796 4550 [ [ 29 65 6 65 4 37 6 40 13 43 20 43 27 40 31 34 34 25 31 18 29 9 25 3 18 0 11 0 4 3 2 6 0 12 ] ] Char
0 -742 4550 [ [ 0 65 0 0 ] [ 31 65 31 0 ] [ 0 34 31 34 ] ] Char
0 -690 4550 [ [ 0 65 0 0 ] [ 0 65 29 65 ] [ 0 34 18 34 ] ] Char
10 SetText2
0 1125 25 [ [ 0 65 0 0 ] [ 0 65 15 65 22 62 27 56 29 50 31 40 31 25 29 15 27 9 22 3 15 0 0 0 ] ] Char
0 1177 25 [ [ 0 65 0 0 27 0 ] ] Char
0 1224 25 [ [ 0 65 0 0 ] [ 0 65 29 65 ] [ 0 34 18 34 ] [ 0 0 29 0 ] ] Char
0 1274 25 [ [ 0 65 0 0 ] [ 0 65 15 65 22 62 27 56 29 50 31 40 31 25 29 15 27 9 22 3 15 0 0 0 ] ] Char
10 SetText2
0 125 25 [ [ 0 65 0 0 ] [ 0 65 15 65 22 62 27 56 29 50 31 40 31 25 29 15 27 9 22 3 15 0 0 0 ] ] Char
0 177 25 [ [ 0 65 0 0 27 0 ] ] Char
0 224 25 [ [ 0 65 0 0 ] [ 0 65 29 65 ] [ 0 34 18 34 ] [ 0 0 29 0 ] ] Char
0 274 25 [ [ 0 65 0 0 ] [ 0 65 15 65 22 62 27 56 29 50 31 40 31 25 29 15 27 9 22 3 15 0 0 0 ] ] Char
10 SetText2
0 -875 25 [ [ 0 65 0 0 ] [ 0 65 15 65 22 62 27 56 29 50 31 40 31 25 29 15 27 9 22 3 15 0 0 0 ] ] Char
0 -823 25 [ [ 0 65 0 0 27 0 ] ] Char
0 -776 25 [ [ 0 65 0 0 ] [ 0 65 29 65 ] [ 0 34 18 34 ] [ 0 0 29 0 ] ] Char
0 -726 25 [ [ 0 65 0 0 ] [ 0 65 15 65 22 62 27 56 29 50 31 40 31 25 29 15 27 9 22 3 15 0 0 0 ] ] Char
10 SetText2
0 1075 3325 [ [ 2 50 2 53 4 59 6 62 11 65 20 65 25 62 27 59 29 53 29 46 27 40 22 31 0 0 31 0 ] ] Char
0 1127 3325 [ [ 0 65 0 0 ] [ 0 65 31 0 ] [ 31 65 31 0 ] ] Char
0 1179 3325 [ [ 2 50 2 53 4 59 6 62 11 65 20 65 25 62 27 59 29 53 29 46 27 40 22 31 0 0 31 0 ] ] Char
0 1231 3325 [ [ 29 43 27 34 22 28 15 25 13 25 6 28 2 34 0 43 0 46 2 56 6 62 13 65 15 65 22 62 27 56 29 43 29 28 27 12 22 3 15 0 11 0 4 3 2 9 ] ] Char
0 1281 3325 [ [ 13 65 6 62 2 53 0 37 0 28 2 12 6 3 13 0 18 0 25 3 29 12 31 28 31 37 29 53 25 62 18 65 13 65 ] ] Char
0 1333 3325 [ [ 31 65 9 0 ] [ 0 65 31 65 ] ] Char
10 SetText2
0 -1075 1575 [ [ 0 -65 0 0 ] [ 0 -65 -15 -65 -22 -62 -27 -56 -29 -50 -31 -40 -31 -25 -29 -15 -27 -9 -22 -3 -15 0 0 0 ] ] Char
0 -1127 1575 [ [ 0 -65 0 0 ] [ 0 -65 -20 -65 -27 -62 -29 -59 -31 -53 -31 -43 -29 -37 -27 -34 -20 -31 0 -31 ] ] Char
0 -1179 1575 [ [ -13 -65 -9 -62 -4 -56 -2 -50 0 -40 0 -25 -2 -15 -4 -9 -9 -3 -13 0 -22 0 -27 -3 -31 -9 -34 -15 -36 -25 -36 -40 -34 -50 -31 -56 -27 -62 -22 -65 -13 -65 ] ] Char
0 -1235 1575 [ [ -15 -65 -15 0 ] [ 0 -65 -31 -65 ] ] Char
10 SetText2
0 25 1650 [ [ 0 65 0 0 ] [ 0 65 15 65 22 62 27 56 29 50 31 40 31 25 29 15 27 9 22 3 15 0 0 0 ] ] Char
0 77 1650 [ [ 0 65 0 0 ] [ 0 65 20 65 27 62 29 59 31 53 31 46 29 40 27 37 20 34 0 34 ] [ 15 34 31 0 ] ] Char
0 129 1650 [ [ 0 65 0 0 ] [ 0 65 29 65 ] [ 0 34 18 34 ] [ 0 0 29 0 ] ] Char
0 179 1650 [ [ 31 56 27 62 20 65 11 65 4 62 0 56 0 50 2 43 4 40 9 37 22 31 27 28 29 25 31 18 31 9 27 3 20 0 11 0 4 3 0 9 ] ] Char
10 SetText2
0 350 1250 [ [ 0 65 0 0 ] [ 0 65 15 65 22 62 27 56 29 50 31 40 31 25 29 15 27 9 22 3 15 0 0 0 ] ] Char
0 402 1250 [ [ 0 65 0 0 ] [ 0 65 20 65 27 62 29 59 31 53 31 46 29 40 27 37 20 34 0 34 ] [ 15 34 31 0 ] ] Char
0 454 1250 [ [ 0 65 0 0 ] [ 0 65 29 65 ] [ 0 34 18 34 ] [ 0 0 29 0 ] ] Char
0 504 1250 [ [ 31 56 27 62 20 65 11 65 4 62 0 56 0 50 2 43 4 40 9 37 22 31 27 28 29 25 31 18 31 9 27 3 20 0 11 0 4 3 0 9 ] ] Char
10 SetText2
0 -550 2250 [ [ 0 65 0 0 ] [ 0 65 15 65 22 62 27 56 29 50 31 40 31 25 29 15 27 9 22 3 15 0 0 0 ] ] Char
0 -498 2250 [ [ 0 65 0 0 ] [ 0 65 20 65 27 62 29 59 31 53 31 46 29 40 27 37 20 34 0 34 ] [ 15 34 31 0 ] ] Char
0 -446 2250 [ [ 0 65 0 0 ] [ 0 65 29 65 ] [ 0 34 18 34 ] [ 0 0 29 0 ] ] Char
0 -396 2250 [ [ 31 56 27 62 20 65 11 65 4 62 0 56 0 50 2 43 4 40 9 37 22 31 27 28 29 25 31 18 31 9 27 3 20 0 11 0 4 3 0 9 ] ] Char
10 SetText2
0 -925 4150 [ [ 0 65 0 0 ] [ 0 65 15 65 22 62 27 56 29 50 31 40 31 25 29 15 27 9 22 3 15 0 0 0 ] ] Char
0 -873 4150 [ [ 0 65 0 0 ] [ 0 65 20 65 27 62 29 59 31 53 31 46 29 40 27 37 20 34 0 34 ] [ 15 34 31 0 ] ] Char
0 -821 4150 [ [ 0 65 0 0 ] [ 0 65 29 65 ] [ 0 34 18 34 ] [ 0 0 29 0 ] ] Char
0 -771 4150 [ [ 31 56 27 62 20 65 11 65 4 62 0 56 0 50 2 43 4 40 9 37 22 31 27 28 29 25 31 18 31 9 27 3 20 0 11 0 4 3 0 9 ] ] Char
10 SetText2
0 850 3150 [ [ 0 65 0 0 ] [ 0 65 15 65 22 62 27 56 29 50 31 40 31 25 29 15 27 9 22 3 15 0 0 0 ] ] Char
0 902 3150 [ [ 0 65 0 0 ] [ 0 65 20 65 27 62 29 59 31 53 31 46 29 40 27 37 20 34 0 34 ] [ 15 34 31 0 ] ] Char
0 954 3150 [ [ 0 65 0 0 ] [ 0 65 29 65 ] [ 0 34 18 34 ] [ 0 0 29 0 ] ] Char
0 1004 3150 [ [ 31 56 27 62 20 65 11 65 4 62 0 56 0 50 2 43 4 40 9 37 22 31 27 28 29 25 31 18 31 9 27 3 20 0 11 0 4 3 0 9 ] ] Char
10 SetText2
0 -550 2450 [ [ 0 65 0 0 ] [ 0 65 15 65 22 62 27 56 29 50 31 40 31 25 29 15 27 9 22 3 15 0 0 0 ] ] Char
0 -498 2450 [ [ 0 65 0 0 ] [ 0 65 20 65 27 62 29 59 31 53 31 46 29 40 27 37 20 34 0 34 ] [ 15 34 31 0 ] ] Char
0 -446 2450 [ [ 0 65 0 0 ] [ 0 65 29 65 ] [ 0 34 18 34 ] [ 0 0 29 0 ] ] Char
0 -396 2450 [ [ 31 56 27 62 20 65 11 65 4 62 0 56 0 50 2 43 4 40 9 37 22 31 27 28 29 25 31 18 31 9 27 3 20 0 11 0 4 3 0 9 ] ] Char
10 SetText2
0 -550 2050 [ [ 0 65 0 0 ] [ 0 65 15 65 22 62 27 56 29 50 31 40 31 25 29 15 27 9 22 3 15 0 0 0 ] ] Char
0 -498 2050 [ [ 0 65 0 0 ] [ 0 65 20 65 27 62 29 59 31 53 31 46 29 40 27 37 20 34 0 34 ] [ 15 34 31 0 ] ] Char
0 -446 2050 [ [ 0 65 0 0 ] [ 0 65 29 65 ] [ 0 34 18 34 ] [ 0 0 29 0 ] ] Char
0 -396 2050 [ [ 31 56 27 62 20 65 11 65 4 62 0 56 0 50 2 43 4 40 9 37 22 31 27 28 29 25 31 18 31 9 27 3 20 0 11 0 4 3 0 9 ] ] Char
10 SetText2
0 -1025 2050 [ [ 0 65 0 0 ] [ 0 65 15 65 22 62 27 56 29 50 31 40 31 25 29 15 27 9 22 3 15 0 0 0 ] ] Char
0 -973 2050 [ [ 0 65 0 0 ] [ 0 65 20 65 27 62 29 59 31 53 31 46 29 40 27 37 20 34 0 34 ] [ 15 34 31 0 ] ] Char
0 -921 2050 [ [ 0 65 0 0 ] [ 0 65 29 65 ] [ 0 34 18 34 ] [ 0 0 29 0 ] ] Char
0 -871 2050 [ [ 31 56 27 62 20 65 11 65 4 62 0 56 0 50 2 43 4 40 9 37 22 31 27 28 29 25 31 18 31 9 27 3 20 0 11 0 4 3 0 9 ] ] Char
10 SetText2
0 350 1050 [ [ 0 65 0 0 ] [ 0 65 15 65 22 62 27 56 29 50 31 40 31 25 29 15 27 9 22 3 15 0 0 0 ] ] Char
0 402 1050 [ [ 0 65 0 0 ] [ 0 65 20 65 27 62 29 59 31 53 31 46 29 40 27 37 20 34 0 34 ] [ 15 34 31 0 ] ] Char
0 454 1050 [ [ 0 65 0 0 ] [ 0 65 29 65 ] [ 0 34 18 34 ] [ 0 0 29 0 ] ] Char
0 504 1050 [ [ 31 56 27 62 20 65 11 65 4 62 0 56 0 50 2 43 4 40 9 37 22 31 27 28 29 25 31 18 31 9 27 3 20 0 11 0 4 3 0 9 ] ] Char
10 SetText2
0 1200 2225 [ [ 0 -65 0 0 ] [ 0 -65 -15 -65 -22 -62 -27 -56 -29 -50 -31 -40 -31 -25 -29 -15 -27 -9 -22 -3 -15 0 0 0 ] ] Char
0 1148 2225 [ [ 0 -65 0 0 ] [ 0 -65 -20 -65 -27 -62 -29 -59 -31 -53 -31 -43 -29 -37 -27 -34 -20 -31 0 -31 ] ] Char
0 1096 2225 [ [ -13 -65 -9 -62 -4 -56 -2 -50 0 -40 0 -25 -2 -15 -4 -9 -9 -3 -13 0 -22 0 -27 -3 -31 -9 -34 -15 -36 -25 -36 -40 -34 -50 -31 -56 -27 -62 -22 -65 -13 -65 ] ] Char
0 1040 2225 [ [ -15 -65 -15 0 ] [ 0 -65 -31 -65 ] ] Char
10 SetText2
0 50 1850 [ [ 0 65 0 0 ] [ 0 65 15 65 22 62 27 56 29 50 31 40 31 25 29 15 27 9 22 3 15 0 0 0 ] ] Char
0 102 1850 [ [ 0 65 0 0 ] [ 0 65 20 65 27 62 29 59 31 53 31 46 29 40 27 37 20 34 0 34 ] [ 15 34 31 0 ] ] Char
0 154 1850 [ [ 0 65 0 0 ] [ 0 65 29 65 ] [ 0 34 18 34 ] [ 0 0 29 0 ] ] Char
0 204 1850 [ [ 31 56 27 62 20 65 11 65 4 62 0 56 0 50 2 43 4 40 9 37 22 31 27 28 29 25 31 18 31 9 27 3 20 0 11 0 4 3 0 9 ] ] Char
10 SetText2
0 150 1450 [ [ 0 65 0 0 ] [ 0 65 15 65 22 62 27 56 29 50 31 40 31 25 29 15 27 9 22 3 15 0 0 0 ] ] Char
0 202 1450 [ [ 0 65 0 0 ] [ 0 65 20 65 27 62 29 59 31 53 31 46 29 40 27 37 20 34 0 34 ] [ 15 34 31 0 ] ] Char
0 254 1450 [ [ 0 65 0 0 ] [ 0 65 29 65 ] [ 0 34 18 34 ] [ 0 0 29 0 ] ] Char
0 304 1450 [ [ 31 56 27 62 20 65 11 65 4 62 0 56 0 50 2 43 4 40 9 37 22 31 27 28 29 25 31 18 31 9 27 3 20 0 11 0 4 3 0 9 ] ] Char
10 SetText2
0 -675 1850 [ [ 0 65 0 0 27 0 ] ] Char
0 -628 1850 [ [ 0 65 0 0 ] [ 0 65 18 0 ] [ 36 65 18 0 ] [ 36 65 36 0 ] ] Char
0 -572 1850 [ [ 4 65 29 65 15 40 22 40 27 37 29 34 31 25 31 18 29 9 25 3 18 0 11 0 4 3 2 6 0 12 ] ] Char
0 -520 1850 [ [ 2 50 2 53 4 59 6 62 11 65 20 65 25 62 27 59 29 53 29 46 27 40 22 31 0 0 31 0 ] ] Char
0 -468 1850 [ [ 22 65 0 21 34 21 ] [ 22 65 22 0 ] ] Char
10 SetText2
0 450 2450 [ [ 31 65 9 0 ] [ 0 65 31 65 ] ] Char
0 502 2450 [ [ 22 65 0 21 34 21 ] [ 22 65 22 0 ] ] Char
0 556 2450 [ [ 0 65 0 0 27 0 ] ] Char
0 603 2450 [ [ 31 56 27 62 20 65 11 65 4 62 0 56 0 50 2 43 4 40 9 37 22 31 27 28 29 25 31 18 31 9 27 3 20 0 11 0 4 3 0 9 ] ] Char
0 655 2450 [ [ 0 53 4 56 11 65 11 0 ] ] Char
0 686 2450 [ [ 2 50 2 53 4 59 6 62 11 65 20 65 25 62 27 59 29 53 29 46 27 40 22 31 0 0 31 0 ] ] Char
0 738 2450 [ [ 4 65 29 65 15 40 22 40 27 37 29 34 31 25 31 18 29 9 25 3 18 0 11 0 4 3 2 6 0 12 ] ] Char
10 SetText2
0 500 4175 [ [ 0 65 0 0 ] [ 0 65 18 0 ] [ 36 65 18 0 ] [ 36 65 36 0 ] ] Char
0 556 4175 [ [ 34 50 31 56 27 62 22 65 13 65 9 62 4 56 2 50 0 40 0 25 2 15 4 9 9 3 13 0 22 0 27 3 31 9 34 15 ] ] Char
0 610 4175 [ [ 0 53 4 56 11 65 11 0 ] ] Char
0 641 4175 [ [ 22 65 0 21 34 21 ] [ 22 65 22 0 ] ] Char
0 695 4175 [ [ 29 65 6 65 4 37 6 40 13 43 20 43 27 40 31 34 34 25 31 18 29 9 25 3 18 0 11 0 4 3 2 6 0 12 ] ] Char
0 749 4175 [ [ 22 65 0 21 34 21 ] [ 22 65 22 0 ] ] Char
0 803 4175 [ [ 22 65 0 21 34 21 ] [ 22 65 22 0 ] ] Char
0 857 4175 [ [ 4 65 29 65 15 40 22 40 27 37 29 34 31 25 31 18 29 9 25 3 18 0 11 0 4 3 2 6 0 12 ] ] Char
10 SetText2
0 -675 3350 [ [ 0 65 0 0 ] ] Char
0 -655 3350 [ [ 34 50 31 56 27 62 22 65 13 65 9 62 4 56 2 50 0 40 0 25 2 15 4 9 9 3 13 0 22 0 27 3 31 9 34 15 ] ] Char
0 -601 3350 [ [ 0 65 0 0 27 0 ] ] Char
0 -554 3350 [ [ 2 50 2 53 4 59 6 62 11 65 20 65 25 62 27 59 29 53 29 46 27 40 22 31 0 0 31 0 ] ] Char
0 -502 3350 [ [ 4 65 29 65 15 40 22 40 27 37 29 34 31 25 31 18 29 9 25 3 18 0 11 0 4 3 2 6 0 12 ] ] Char
0 -450 3350 [ [ 2 50 2 53 4 59 6 62 11 65 20 65 25 62 27 59 29 53 29 46 27 40 22 31 0 0 31 0 ] ] Char
10 SetText2
0 950 1600 [ [ 0 65 0 0 27 0 ] ] Char
0 997 1600 [ [ 0 65 0 0 ] [ 0 65 18 0 ] [ 36 65 18 0 ] [ 36 65 36 0 ] ] Char
0 1053 1600 [ [ 31 65 9 0 ] [ 0 65 31 65 ] ] Char
0 1105 1600 [ [ 11 65 4 62 2 56 2 50 4 43 9 40 18 37 25 34 29 28 31 21 31 12 29 6 27 3 20 0 11 0 4 3 2 6 0 12 0 21 2 28 6 34 13 37 22 40 27 43 29 50 29 56 27 62 20 65 11 65 ] ] Char
0 1157 1600 [ [ 13 65 6 62 2 53 0 37 0 28 2 12 6 3 13 0 18 0 25 3 29 12 31 28 31 37 29 53 25 62 18 65 13 65 ] ] Char
0 1209 1600 [ [ 29 65 6 65 4 37 6 40 13 43 20 43 27 40 31 34 34 25 31 18 29 9 25 3 18 0 11 0 4 3 2 6 0 12 ] ] Char
10 SetText2
0 -350 4225 [ [ 0 65 0 0 ] [ 0 65 15 65 22 62 27 56 29 50 31 40 31 25 29 15 27 9 22 3 15 0 0 0 ] ] Char
0 -298 4225 [ [ 0 65 31 0 ] [ 31 65 0 0 ] ] Char
0 -246 4225 [ [ 15 65 15 0 ] [ 0 65 31 65 ] ] Char
0 -194 4225 [ [ 18 65 0 0 ] [ 18 65 36 0 ] [ 6 21 29 21 ] ] Char
0 -138 4225 [ [ 0 65 0 0 27 0 ] ] Char
10 SetText2
0 -1225 500 [ [ 0 65 0 0 ] [ 0 65 18 0 ] [ 36 65 18 0 ] [ 36 65 36 0 ] ] Char
0 -1169 500 [ [ 15 65 15 0 ] [ 0 65 31 65 ] ] Char
0 -1117 500 [ [ 0 65 0 0 ] [ 31 65 31 0 ] [ 0 34 31 34 ] ] Char
0 -1065 500 [ [ 13 65 9 62 4 56 2 50 0 40 0 25 2 15 4 9 9 3 13 0 22 0 27 3 31 9 34 15 36 25 36 40 34 50 31 56 27 62 22 65 13 65 ] ] Char
0 -1009 500 [ [ 0 65 0 0 27 0 ] ] Char
0 -962 500 [ [ 0 65 0 0 ] [ 0 65 29 65 ] [ 0 34 18 34 ] [ 0 0 29 0 ] ] Char
0 -912 500 [ [ 2 50 2 53 4 59 6 62 11 65 20 65 25 62 27 59 29 53 29 46 27 40 22 31 0 0 31 0 ] ] Char
0 -860 500 [ [ 29 65 6 65 4 37 6 40 13 43 20 43 27 40 31 34 34 25 31 18 29 9 25 3 18 0 11 0 4 3 2 6 0 12 ] ] Char
10 SetText2
0 1125 500 [ [ 0 65 0 0 ] [ 0 65 18 0 ] [ 36 65 18 0 ] [ 36 65 36 0 ] ] Char
0 1181 500 [ [ 15 65 15 0 ] [ 0 65 31 65 ] ] Char
0 1233 500 [ [ 0 65 0 0 ] [ 31 65 31 0 ] [ 0 34 31 34 ] ] Char
0 1285 500 [ [ 13 65 9 62 4 56 2 50 0 40 0 25 2 15 4 9 9 3 13 0 22 0 27 3 31 9 34 15 36 25 36 40 34 50 31 56 27 62 22 65 13 65 ] ] Char
0 1341 500 [ [ 0 65 0 0 27 0 ] ] Char
0 1388 500 [ [ 0 65 0 0 ] [ 0 65 29 65 ] [ 0 34 18 34 ] [ 0 0 29 0 ] ] Char
0 1438 500 [ [ 2 50 2 53 4 59 6 62 11 65 20 65 25 62 27 59 29 53 29 46 27 40 22 31 0 0 31 0 ] ] Char
0 1490 500 [ [ 29 65 6 65 4 37 6 40 13 43 20 43 27 40 31 34 34 25 31 18 29 9 25 3 18 0 11 0 4 3 2 6 0 12 ] ] Char
10 SetText2
0 800 3350 [ [ 0 65 0 0 ] [ 0 65 15 65 22 62 27 56 29 50 31 40 31 25 29 15 27 9 22 3 15 0 0 0 ] ] Char
0 852 3350 [ [ 0 65 0 0 ] [ 0 65 20 65 27 62 29 59 31 53 31 46 29 40 27 37 20 34 0 34 ] [ 15 34 31 0 ] ] Char
0 904 3350 [ [ 0 65 0 0 ] [ 0 65 29 65 ] [ 0 34 18 34 ] [ 0 0 29 0 ] ] Char
0 954 3350 [ [ 31 56 27 62 20 65 11 65 4 62 0 56 0 50 2 43 4 40 9 37 22 31 27 28 29 25 31 18 31 9 27 3 20 0 11 0 4 3 0 9 ] ] Char
10 SetText2
0 -300 4725 [ [ 31 56 27 62 20 65 11 65 4 62 0 56 0 50 2 43 4 40 9 37 22 31 27 28 29 25 31 18 31 9 27 3 20 0 11 0 4 3 0 9 ] ] Char
0 -248 4725 [ [ 0 65 0 0 ] ] Char
0 -228 4725 [ [ 0 65 0 0 ] [ 0 65 15 65 22 62 27 56 29 50 31 40 31 25 29 15 27 9 22 3 15 0 0 0 ] ] Char
0 -176 4725 [ [ 0 65 0 0 ] [ 0 65 29 65 ] [ 0 34 18 34 ] [ 0 0 29 0 ] ] Char
0 -74 4725 [ [ 0 53 4 56 11 65 11 0 ] ] Char
10 SetText2
0 -300 5075 [ [ 2 50 2 53 4 59 6 62 11 65 20 65 25 62 27 59 29 53 29 46 27 40 22 31 0 0 31 0 ] ] Char
0 -248 5075 [ [ 2 6 0 3 2 0 4 3 2 6 ] ] Char
0 -223 5075 [ [ 31 65 9 0 ] [ 0 65 31 65 ] ] Char
0 -171 5075 [ [ 13 65 6 62 2 53 0 37 0 28 2 12 6 3 13 0 18 0 25 3 29 12 31 28 31 37 29 53 25 62 18 65 13 65 ] ] Char
0 -119 5075 [ [ 13 65 6 62 2 53 0 37 0 28 2 12 6 3 13 0 18 0 25 3 29 12 31 28 31 37 29 53 25 62 18 65 13 65 ] ] Char
0 -15 5075 [ [ 20 56 20 0 ] [ 0 28 40 28 ] ] Char
0 46 5075 [ [ 40 78 0 -21 ] ] Char
0 107 5075 [ [ 0 28 40 28 ] ] Char
0 220 5075 [ [ 2 6 0 3 2 0 4 3 2 6 ] ] Char
0 245 5075 [ [ 13 65 6 62 2 53 0 37 0 28 2 12 6 3 13 0 18 0 25 3 29 12 31 28 31 37 29 53 25 62 18 65 13 65 ] ] Char
0 297 5075 [ [ 13 65 6 62 2 53 0 37 0 28 2 12 6 3 13 0 18 0 25 3 29 12 31 28 31 37 29 53 25 62 18 65 13 65 ] ] Char
0 349 5075 [ [ 0 53 4 56 11 65 11 0 ] ] Char
10 SetText2
0 -1475 2025 [ [ -65 22 -21 0 -21 34 ] [ -65 22 0 22 ] ] Char
0 -1475 2079 [ [ -6 2 -3 0 0 2 -3 4 -6 2 ] ] Char
0 -1475 2104 [ [ -43 29 -34 27 -28 22 -25 15 -25 13 -28 6 -34 2 -43 0 -46 0 -56 2 -62 6 -65 13 -65 15 -62 22 -56 27 -43 29 -28 29 -12 27 -3 22 0 15 0 11 -3 4 -9 2 ] ] Char
0 -1475 2154 [ [ -65 13 -62 6 -53 2 -37 0 -28 0 -12 2 -3 6 0 13 0 18 -3 25 -12 29 -28 31 -37 31 -53 29 -62 25 -65 18 -65 13 ] ] Char
0 -1475 2206 [ [ -65 13 -62 6 -53 2 -37 0 -28 0 -12 2 -3 6 0 13 0 18 -3 25 -12 29 -28 31 -37 31 -53 29 -62 25 -65 18 -65 13 ] ] Char
0 -1475 2310 [ [ -56 20 0 20 ] [ -28 0 -28 40 ] ] Char
0 -1475 2371 [ [ -78 40 21 0 ] ] Char
0 -1475 2432 [ [ -28 0 -28 40 ] ] Char
0 -1475 2545 [ [ -6 2 -3 0 0 2 -3 4 -6 2 ] ] Char
0 -1475 2570 [ [ -65 13 -62 6 -53 2 -37 0 -28 0 -12 2 -3 6 0 13 0 18 -3 25 -12 29 -28 31 -37 31 -53 29 -62 25 -65 18 -65 13 ] ] Char
0 -1475 2622 [ [ -65 13 -62 6 -53 2 -37 0 -28 0 -12 2 -3 6 0 13 0 18 -3 25 -12 29 -28 31 -37 31 -53 29 -62 25 -65 18 -65 13 ] ] Char
0 -1475 2674 [ [ -53 0 -56 4 -65 11 0 11 ] ] Char
grestore
showpage
