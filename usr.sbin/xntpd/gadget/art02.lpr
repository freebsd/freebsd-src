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
(gadget.job - Fri Aug 21 03:35:02 1992) show
gsave
Init
8000 10500 Clipto
4000 2800 translate
-1 1 scale
0 rotate
1 1 div dup scale
75 sg
50 sg
25 sg
0 sg
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
0 0 60 /PRndPad SetFlash
-1100 1450 Flash
-1100 1150 Flash
300 3300 Flash
-100 3300 Flash
-100 3100 Flash
300 3100 Flash
300 3500 Flash
-100 3500 Flash
-400 3700 Flash
-1200 3700 Flash
-100 1300 Flash
-900 1300 Flash
-200 2800 Flash
600 2800 Flash
-1200 2800 Flash
-400 2800 Flash
0 2500 Flash
0 2100 Flash
-1200 3400 Flash
-1200 3000 Flash
-900 2300 Flash
-1200 2300 Flash
-1200 2500 Flash
-900 2500 Flash
800 2800 Flash
1100 2800 Flash
1250 1900 Flash
450 1900 Flash
-100 900 Flash
-1200 900 Flash
-700 4000 Flash
-1100 4000 Flash
1100 3000 Flash
700 3000 Flash
-300 3700 Flash
0 3700 Flash
0 0 60 /PSqrPad SetFlash
100 900 Flash
0 0 60 /PRndPad SetFlash
600 900 Flash
0 0 60 /PSqrPad SetFlash
700 3700 Flash
0 0 60 /PRndPad SetFlash
200 3700 Flash
0 0 70 /PRndPad SetFlash
-750 550 Flash
-750 450 Flash
0 550 Flash
0 450 Flash
750 550 Flash
750 450 Flash
-648 4479 Flash
-540 4479 Flash
-432 4479 Flash
-324 4479 Flash
-216 4479 Flash
-108 4479 Flash
0 4479 Flash
108 4479 Flash
216 4479 Flash
324 4479 Flash
432 4479 Flash
540 4479 Flash
648 4479 Flash
-594 4593 Flash
-486 4593 Flash
-378 4593 Flash
-270 4593 Flash
-162 4593 Flash
-54 4593 Flash
54 4593 Flash
162 4593 Flash
270 4593 Flash
378 4593 Flash
486 4593 Flash
594 4593 Flash
0 0 177 /PRndPad SetFlash
940 4536 Flash
-940 4536 Flash
0 0 60 /PSqrPad SetFlash
950 150 Flash
0 0 60 /PRndPad SetFlash
1050 150 Flash
0 0 60 /PSqrPad SetFlash
-50 150 Flash
0 0 60 /PRndPad SetFlash
50 150 Flash
0 0 60 /PSqrPad SetFlash
-1050 150 Flash
0 0 60 /PRndPad SetFlash
-950 150 Flash
0 0 50 /PRndPad SetFlash
950 3524 Flash
1026 3612 Flash
950 3700 Flash
0 0 60 /PSqrPad SetFlash
-1200 1600 Flash
0 0 60 /PRndPad SetFlash
-1100 1700 Flash
-1200 1800 Flash
300 1700 Flash
-100 1700 Flash
200 1300 Flash
600 1300 Flash
-700 2300 Flash
-300 2300 Flash
-700 4200 Flash
-1100 4200 Flash
1100 3200 Flash
700 3200 Flash
-700 2500 Flash
-300 2500 Flash
-700 2100 Flash
-300 2100 Flash
-800 2100 Flash
-1200 2100 Flash
600 1100 Flash
200 1100 Flash
0 0 60 /PSqrPad SetFlash
1200 2450 Flash
0 0 60 /PRndPad SetFlash
1100 2350 Flash
1200 2250 Flash
-100 1900 Flash
300 1900 Flash
0 1500 Flash
400 1500 Flash
0 0 60 /PSqrPad SetFlash
-900 1600 Flash
0 0 60 /PRndPad SetFlash
-800 1600 Flash
-700 1600 Flash
-600 1600 Flash
-500 1600 Flash
-400 1600 Flash
-300 1600 Flash
-300 1900 Flash
-400 1900 Flash
-500 1900 Flash
-600 1900 Flash
-700 1900 Flash
-800 1900 Flash
-900 1900 Flash
0 0 60 /PSqrPad SetFlash
200 2200 Flash
0 0 60 /PRndPad SetFlash
300 2200 Flash
400 2200 Flash
500 2200 Flash
600 2200 Flash
700 2200 Flash
800 2200 Flash
900 2200 Flash
900 2500 Flash
800 2500 Flash
700 2500 Flash
600 2500 Flash
500 2500 Flash
400 2500 Flash
300 2500 Flash
200 2500 Flash
0 0 60 /PSqrPad SetFlash
200 3900 Flash
0 0 60 /PRndPad SetFlash
300 3900 Flash
400 3900 Flash
500 3900 Flash
600 3900 Flash
700 3900 Flash
800 3900 Flash
900 3900 Flash
1000 3900 Flash
1100 3900 Flash
1100 4200 Flash
1000 4200 Flash
900 4200 Flash
800 4200 Flash
700 4200 Flash
600 4200 Flash
500 4200 Flash
400 4200 Flash
300 4200 Flash
200 4200 Flash
0 0 60 /PSqrPad SetFlash
-1000 3100 Flash
0 0 60 /PRndPad SetFlash
-900 3100 Flash
-800 3100 Flash
-700 3100 Flash
-600 3100 Flash
-500 3100 Flash
-400 3100 Flash
-300 3100 Flash
-300 3400 Flash
-400 3400 Flash
-500 3400 Flash
-600 3400 Flash
-700 3400 Flash
-800 3400 Flash
-900 3400 Flash
-1000 3400 Flash
0 0 70 /PRndPad SetFlash
900 800 Flash
1100 800 Flash
1000 800 Flash
0 0 177 /PRndPad SetFlash
1000 1550 Flash
0 0 60 /PRndPad SetFlash
0 4000 Flash
0 4200 Flash
0 0 250 /PRndPad SetFlash
-1100 450 Flash
1100 450 Flash
0 0 60 /PRndPad SetFlash
1100 3400 Flash
700 3400 Flash
10 SetText2
0 300 4725 [ [ -31 56 -27 62 -20 65 -11 65 -4 62 0 56 0 50 -2 43 -4 40 -9 37 -22 31 -27 28 -29 25 -31 18 -31 9 -27 3 -20 0 -11 0 -4 3 0 9 ] ] Char
0 248 4725 [ [ 0 65 0 0 ] ] Char
0 228 4725 [ [ 0 65 0 0 ] [ 0 65 -15 65 -22 62 -27 56 -29 50 -31 40 -31 25 -29 15 -27 9 -22 3 -15 0 0 0 ] ] Char
0 176 4725 [ [ 0 65 0 0 ] [ 0 65 -29 65 ] [ 0 34 -18 34 ] [ 0 0 -29 0 ] ] Char
0 74 4725 [ [ -2 50 -2 53 -4 59 -6 62 -11 65 -20 65 -25 62 -27 59 -29 53 -29 46 -27 40 -22 31 0 0 -31 0 ] ] Char
12 SetLine
700 3700 [ 750 3650 ] PLine
750 3650 [ 750 800 ] PLine
750 800 [ 900 800 ] PLine
0 550 [ 300 550 ] PLine
300 550 [ 300 1100 ] PLine
300 2200 [ 250 2150 ] PLine
250 2150 [ 250 1600 ] PLine
250 1600 [ 300 1550 ] PLine
300 1550 [ 300 1100 ] PLine
-700 2500 [ -550 2500 ] PLine
-550 2500 [ -550 1700 ] PLine
-550 1700 [ -700 1700 ] PLine
-700 1700 [ -700 1600 ] PLine
300 3500 [ 175 3500 ] PLine
175 3500 [ 175 3100 ] PLine
175 3100 [ 300 3100 ] PLine
300 4200 [ 250 4150 ] PLine
250 4150 [ 250 3800 ] PLine
250 3800 [ 300 3750 ] PLine
300 3750 [ 300 3500 ] PLine
-300 2500 [ -250 2550 ] PLine
-250 2550 [ -250 3300 ] PLine
-250 3300 [ -100 3300 ] PLine
300 4200 [ 400 4200 ] PLine
-900 1600 [ -800 1600 ] PLine
-800 1600 [ -800 1500 ] PLine
-800 1500 [ -500 1500 ] PLine
-500 1500 [ -500 1600 ] PLine
1000 2650 [ 1000 2250 ] PLine
1000 2250 [ 1200 2250 ] PLine
400 1500 [ 400 2200 ] PLine
400 2200 [ 400 2300 ] PLine
400 2300 [ 700 2300 ] PLine
700 2300 [ 700 2500 ] PLine
-450 2100 [ -450 1650 ] PLine
-450 1650 [ -400 1600 ] PLine
-500 3400 [ -500 3150 ] PLine
-500 3150 [ -650 3150 ] PLine
-650 3150 [ -650 2600 ] PLine
-100 2600 [ -100 2250 ] PLine
-1200 2500 [ -1200 2300 ] PLine
-300 2100 [ -250 2050 ] PLine
-250 2050 [ -250 1650 ] PLine
-250 1650 [ -300 1600 ] PLine
800 1800 [ 800 2200 ] PLine
600 900 [ 600 550 ] PLine
600 550 [ 750 550 ] PLine
700 2200 [ 700 1900 ] PLine
600 1700 [ 600 2200 ] PLine
1050 150 [ 1050 250 ] PLine
1050 250 [ 500 250 ] PLine
500 250 [ 500 1600 ] PLine
0 4200 [ 0 4100 ] PLine
900 4100 [ 900 3900 ] PLine
800 3900 [ 800 3000 ] PLine
600 3700 [ 600 3900 ] PLine
600 4200 [ 600 4075 ] PLine
600 4075 [ 450 4075 ] PLine
450 4075 [ 450 2700 ] PLine
-400 2700 [ -400 2300 ] PLine
300 3300 [ 350 3350 ] PLine
350 3350 [ 350 3800 ] PLine
-1200 2800 [ -1000 2800 ] PLine
-1000 2800 [ -1000 3100 ] PLine
-900 3100 [ -850 3150 ] PLine
-850 3150 [ -850 3700 ] PLine
50 150 [ 400 150 ] PLine
400 150 [ 400 1300 ] PLine
-500 3100 [ -500 3000 ] PLine
-500 3000 [ -350 3000 ] PLine
-350 3000 [ -350 1300 ] PLine
-350 1300 [ -100 1300 ] PLine
200 3700 [ 150 3750 ] PLine
150 3750 [ 150 4425 ] PLine
150 4425 [ 108 4479 ] PLine
108 4479 [ 54 4593 ] PLine
-108 4479 [ 50 4600 ] PLine
50 4600 [ 54 4593 ] PLine
-324 4479 [ -216 4479 ] PLine
-1100 1700 [ -1100 1450 ] PLine
-1050 1050 [ -1050 1800 ] PLine
-1050 1800 [ -1200 1800 ] PLine
950 3524 [ 950 3200 ] PLine
950 3700 [ 850 3700 ] PLine
850 3700 [ 850 1200 ] PLine
-600 1200 [ -600 150 ] PLine
-600 150 [ -950 150 ] PLine
-540 4479 [ -550 4479 ] PLine
-550 4479 [ -550 3900 ] PLine
-350 3900 [ -350 3150 ] PLine
-350 3150 [ -300 3100 ] PLine
-432 4479 [ -450 4479 ] PLine
-450 4479 [ -450 3150 ] PLine
-450 3150 [ -382 3100 ] PLine
-382 3100 [ -400 3100 ] PLine
-800 4300 [ -800 3400 ] PLine
-750 4350 [ -750 3450 ] PLine
-750 3450 [ -700 3400 ] PLine
400 3900 [ 400 3400 ] PLine
1100 3400 [ 1100 3600 ] PLine
1100 3600 [ 1026 3600 ] PLine
1026 3600 [ 1026 3612 ] PLine
50 SetLine
-100 3100 [ 0 3100 ] PLine
0 3100 [ 0 2500 ] PLine
-200 2350 [ -200 1900 ] PLine
-200 1900 [ -100 1900 ] PLine
-200 1900 [ -200 1500 ] PLine
-200 1500 [ 0 1500 ] PLine
0 1500 [ 200 1500 ] PLine
200 1500 [ 200 1300 ] PLine
800 4200 [ 800 4050 ] PLine
800 4050 [ 700 4050 ] PLine
700 4050 [ 700 3900 ] PLine
-750 450 [ -900 450 ] PLine
-900 450 [ -900 1300 ] PLine
-1200 1600 [ -1275 1600 ] PLine
-1275 1600 [ -1275 900 ] PLine
-900 900 [ -1275 900 ] PLine
-600 1900 [ -600 1750 ] PLine
-600 1750 [ -975 1750 ] PLine
-975 1750 [ -975 1300 ] PLine
-975 1300 [ -900 1300 ] PLine
-1200 2100 [ -1275 2100 ] PLine
-1275 2100 [ -1275 1600 ] PLine
-1200 3000 [ -1275 3000 ] PLine
-1275 3000 [ -1275 2100 ] PLine
-900 3400 [ -900 3525 ] PLine
-900 3525 [ -1275 3525 ] PLine
-1100 4000 [ -1275 4000 ] PLine
-1275 4000 [ -1275 3000 ] PLine
75 3500 [ 75 2100 ] PLine
75 2100 [ 0 2100 ] PLine
75 2200 [ 200 2200 ] PLine
0 4479 [ 0 4400 ] PLine
0 4400 [ 75 4400 ] PLine
75 4400 [ 75 3500 ] PLine
-300 3700 [ -300 3500 ] PLine
-300 3500 [ 75 3500 ] PLine
900 2500 [ 900 2200 ] PLine
1000 4000 [ 1000 4200 ] PLine
0 0 55 /PRndPad SetFlash
-200 2350 Flash
-200 1750 Flash
200 1400 Flash
0 0 55 /PRndPad SetFlash
300 1100 Flash
0 0 55 /PRndPad SetFlash
175 3300 Flash
0 0 55 /PRndPad SetFlash
1000 2650 Flash
0 0 55 /PRndPad SetFlash
-450 2100 Flash
0 0 55 /PRndPad SetFlash
-650 2600 Flash
-100 2600 Flash
-100 2250 Flash
0 0 55 /PRndPad SetFlash
800 1800 Flash
0 0 55 /PRndPad SetFlash
700 1900 Flash
0 0 55 /PRndPad SetFlash
600 1700 Flash
0 0 55 /PRndPad SetFlash
500 1600 Flash
0 0 55 /PRndPad SetFlash
0 4100 Flash
900 4100 Flash
0 0 55 /PRndPad SetFlash
800 3000 Flash
0 0 55 /PRndPad SetFlash
600 3700 Flash
0 0 55 /PRndPad SetFlash
450 2700 Flash
-400 2700 Flash
-400 2300 Flash
0 0 55 /PRndPad SetFlash
350 3800 Flash
0 0 55 /PRndPad SetFlash
-850 3700 Flash
0 0 55 /PRndPad SetFlash
400 1300 Flash
0 0 55 /PRndPad SetFlash
-1050 1050 Flash
0 0 55 /PRndPad SetFlash
0 4475 Flash
75 4000 Flash
1000 4000 Flash
0 0 55 /PRndPad SetFlash
950 3200 Flash
0 0 55 /PRndPad SetFlash
850 1200 Flash
-600 1200 Flash
0 0 55 /PRndPad SetFlash
-550 3900 Flash
-350 3900 Flash
0 0 55 /PRndPad SetFlash
-800 4300 Flash
0 0 55 /PRndPad SetFlash
-750 4350 Flash
0 0 55 /PRndPad SetFlash
400 3400 Flash
grestore
showpage
