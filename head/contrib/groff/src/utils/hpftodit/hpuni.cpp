// -*- C++ -*-
/* Copyright (C) 2003, 2004 Free Software Foundation, Inc.
     Written by Jeff Conrad (jeff_conrad@msn.com)

This file is part of groff.

groff is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free
Software Foundation; either version 2, or (at your option) any later
version.

groff is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License along
with groff; see the file COPYING.  If not, write to the Free Software
Foundation, 51 Franklin St - Fifth Floor, Boston, MA 02110-1301, USA. */

#include "lib.h"
#include "stringclass.h"
#include "ptable.h"

#include "unicode.h"

struct hp_msl_to_unicode {
  char *value;
};

declare_ptable(hp_msl_to_unicode)
implement_ptable(hp_msl_to_unicode)

PTABLE(hp_msl_to_unicode) hp_msl_to_unicode_table;

struct S {
  const char *key;
  const char *value;
} hp_msl_to_unicode_list[] = {
  { "1", "0021", },	// Exclamation Mark
  { "2", "0022", },	// Neutral Double Quote
  { "3", "0023", },	// Number Sign
  { "4", "0024", },	// Dollar Sign
  { "5", "0025", },	// Per Cent Sign
  { "6", "0026", },	// Ampersand
  { "8", "2019", },	// Single Close Quote (9)
  { "9", "0028", },	// Left Parenthesis
  { "10", "0029", },	// Right Parenthesis
  { "11", "002A", },	// Asterisk
  { "12", "002B", },	// Plus Sign
  { "13", "002C", },	// Comma, or Decimal Separator
  { "14", "002D", },	// Hyphen
  { "15", "002E", },	// Period, or Full Stop
  { "16", "002F", },	// Solidus, or Slash
  { "17", "0030", },	// Numeral Zero
  { "18", "0031", },	// Numeral One
  { "19", "0032", },	// Numeral Two
  { "20", "0033", },	// Numeral Three
  { "21", "0034", },	// Numeral Four
  { "22", "0035", },	// Numeral Five
  { "23", "0036", },	// Numeral Six
  { "24", "0037", },	// Numeral Seven
  { "25", "0038", },	// Numeral Eight
  { "26", "0039", },	// Numeral Nine
  { "27", "003A", },	// Colon
  { "28", "003B", },	// Semicolon
  { "29", "003C", },	// Less Than Sign
  { "30", "003D", },	// Equals Sign
  { "31", "003E", },	// Greater Than Sign
  { "32", "003F", },	// Question Mark
  { "33", "0040", },	// Commercial At
  { "34", "0041", },	// Uppercase A
  { "35", "0042", },	// Uppercase B
  { "36", "0043", },	// Uppercase C
  { "37", "0044", },	// Uppercase D
  { "38", "0045", },	// Uppercase E
  { "39", "0046", },	// Uppercase F
  { "40", "0047", },	// Uppercase G
  { "41", "0048", },	// Uppercase H
  { "42", "0049", },	// Uppercase I
  { "43", "004A", },	// Uppercase J
  { "44", "004B", },	// Uppercase K
  { "45", "004C", },	// Uppercase L
  { "46", "004D", },	// Uppercase M
  { "47", "004E", },	// Uppercase N
  { "48", "004F", },	// Uppercase O
  { "49", "0050", },	// Uppercase P
  { "50", "0051", },	// Uppercase Q
  { "51", "0052", },	// Uppercase R
  { "52", "0053", },	// Uppercase S
  { "53", "0054", },	// Uppercase T
  { "54", "0055", },	// Uppercase U
  { "55", "0056", },	// Uppercase V
  { "56", "0057", },	// Uppercase W
  { "57", "0058", },	// Uppercase X
  { "58", "0059", },	// Uppercase Y
  { "59", "005A", },	// Uppercase Z
  { "60", "005B", },	// Left Bracket
  { "61", "005C", },	// Reverse Solidus, or Backslash
  { "62", "005D", },	// Right Bracket
  { "63", "005E", },	// Circumflex, Exponent, or Pointer
  { "64", "005F", },	// Underline or Underscore Character
  { "66", "2018", },	// Single Open Quote (6)
  { "67", "0061", },	// Lowercase A
  { "68", "0062", },	// Lowercase B
  { "69", "0063", },	// Lowercase C
  { "70", "0064", },	// Lowercase D
  { "71", "0065", },	// Lowercase E
  { "72", "0066", },	// Lowercase F
  { "73", "0067", },	// Lowercase G
  { "74", "0068", },	// Lowercase H
  { "75", "0069", },	// Lowercase I
  { "76", "006A", },	// Lowercase J
  { "77", "006B", },	// Lowercase K
  { "78", "006C", },	// Lowercase L
  { "79", "006D", },	// Lowercase M
  { "80", "006E", },	// Lowercase N
  { "81", "006F", },	// Lowercase O
  { "82", "0070", },	// Lowercase P
  { "83", "0071", },	// Lowercase Q
  { "84", "0072", },	// Lowercase R
  { "85", "0073", },	// Lowercase S
  { "86", "0074", },	// Lowercase T
  { "87", "0075", },	// Lowercase U
  { "88", "0076", },	// Lowercase V
  { "89", "0077", },	// Lowercase W
  { "90", "0078", },	// Lowercase X
  { "91", "0079", },	// Lowercase Y
  { "92", "007A", },	// Lowercase Z
  { "93", "007B", },	// Left Brace
  { "94", "007C", },	// Long Vertical Mark
  { "95", "007D", },	// Right Brace
  { "96", "007E", },	// One Wavy Line Approximate
  { "97", "2592", },	// Medium Shading Character
  { "99", "00C0", },	// Uppercase A Grave
  { "100", "00C2", },	// Uppercase A Circumflex
  { "101", "00C8", },	// Uppercase E Grave
  { "102", "00CA", },	// Uppercase E Circumflex
  { "103", "00CB", },	// Uppercase E Dieresis
  { "104", "00CE", },	// Uppercase I Circumflex
  { "105", "00CF", },	// Uppercase I Dieresis
  { "106", "00B4", },	// Lowercase Acute Accent (Spacing)
  { "107", "0060", },	// Lowercase Grave Accent (Spacing)
  { "108", "02C6", },	// Lowercase Circumflex Accent (Spacing)
  { "109", "00A8", },	// Lowercase Dieresis Accent (Spacing)
  { "110", "02DC", },	// Lowercase Tilde Accent (Spacing)
  { "111", "00D9", },	// Uppercase U Grave
  { "112", "00DB", },	// Uppercase U Circumflex
  { "113", "00AF", },	// Overline, or Overscore Character
  { "114", "00DD", },	// Uppercase Y Acute
  { "115", "00FD", },	// Lowercase Y Acute
  { "116", "00B0", },	// Degree Sign
  { "117", "00C7", },	// Uppercase C Cedilla
  { "118", "00E7", },	// Lowercase C Cedilla
  { "119", "00D1", },	// Uppercase N Tilde
  { "120", "00F1", },	// Lowercase N Tilde
  { "121", "00A1", },	// Inverted Exclamation
  { "122", "00BF", },	// Inverted Question Mark
  { "123", "00A4", },	// Currency Symbol
  { "124", "00A3", },	// Pound Sterling Sign
  { "125", "00A5", },	// Yen Sign
  { "126", "00A7", },	// Section Mark
  { "127", "0192", },	// Florin Sign
  { "128", "00A2", },	// Cent Sign
  { "129", "00E2", },	// Lowercase A Circumflex
  { "130", "00EA", },	// Lowercase E Circumflex
  { "131", "00F4", },	// Lowercase O Circumflex
  { "132", "00FB", },	// Lowercase U Circumflex
  { "133", "00E1", },	// Lowercase A Acute
  { "134", "00E9", },	// Lowercase E Acute
  { "135", "00F3", },	// Lowercase O Acute
  { "136", "00FA", },	// Lowercase U Acute
  { "137", "00E0", },	// Lowercase A Grave
  { "138", "00E8", },	// Lowercase E Grave
  { "139", "00F2", },	// Lowercase O Grave
  { "140", "00F9", },	// Lowercase U Grave
  { "141", "00E4", },	// Lowercase A Dieresis
  { "142", "00EB", },	// Lowercase E Dieresis
  { "143", "00F6", },	// Lowercase O Dieresis
  { "144", "00FC", },	// Lowercase U Dieresis
  { "145", "00C5", },	// Uppercase A Ring
  { "146", "00EE", },	// Lowercase I Circumflex
  { "147", "00D8", },	// Uppercase O Oblique
  { "148", "00C6", },	// Uppercase AE Diphthong
  { "149", "00E5", },	// Lowercase A Ring
  { "150", "00ED", },	// Lowercase I Acute
  { "151", "00F8", },	// Lowercase O Oblique
  { "152", "00E6", },	// Lowercase AE Diphthong
  { "153", "00C4", },	// Uppercase A Dieresis
  { "154", "00EC", },	// Lowercase I Grave
  { "155", "00D6", },	// Uppercase O Dieresis
  { "156", "00DC", },	// Uppercase U Dieresis
  { "157", "00C9", },	// Uppercase E Acute
  { "158", "00EF", },	// Lowercase I Dieresis
  { "159", "00DF", },	// Lowercase Es-zet Ligature
  { "160", "00D4", },	// Uppercase O Circumflex
  { "161", "00C1", },	// Uppercase A Acute
  { "162", "00C3", },	// Uppercase A Tilde
  { "163", "00E3", },	// Lowercase A Tilde
  { "164", "00D0", },	// Uppercase Eth
//{ "164", "0110", },	// Uppercase D-Stroke
  { "165", "00F0", },	// Lowercase Eth
  { "166", "00CD", },	// Uppercase I Acute
  { "167", "00CC", },	// Uppercase I Grave
  { "168", "00D3", },	// Uppercase O Acute
  { "169", "00D2", },	// Uppercase O Grave
  { "170", "00D5", },	// Uppercase O Tilde
  { "171", "00F5", },	// Lowercase O Tilde
  { "172", "0160", },	// Uppercase S Hacek
  { "173", "0161", },	// Lowercase S Hacek
  { "174", "00DA", },	// Uppercase U Acute
  { "175", "0178", },	// Uppercase Y Dieresis
  { "176", "00FF", },	// Lowercase Y Dieresis
  { "177", "00DE", },	// Uppercase Thorn
  { "178", "00FE", },	// Lowercase Thorn
  { "180", "00B5", },	// Lowercase Greek Mu, or Micro
  { "181", "00B6", },	// Pilcrow, or Paragraph Sign
  { "182", "00BE", },	// Vulgar Fraction 3/4
  { "183", "2212", },	// Minus Sign
  { "184", "00BC", },	// Vulgar Fraction 1/4
  { "185", "00BD", },	// Vulgar Fraction 1/2
  { "186", "00AA", },	// Female Ordinal
  { "187", "00BA", },	// Male Ordinal
  { "188", "00AB", },	// Left Pointing Double Angle Quote
  { "189", "25A0", },	// Medium Solid Square Box
  { "190", "00BB", },	// Right Pointing Double Angle Quote
  { "191", "00B1", },	// Plus Over Minus Sign
  { "192", "00A6", },	// Broken Vertical Mark
  { "193", "00A9", },	// Copyright Sign
  { "194", "00AC", },	// Not Sign
  { "195", "00AD", },	// Soft Hyphen
  { "196", "00AE", },	// Registered Sign
  { "197", "00B2", },	// Superior Numeral 2
  { "198", "00B3", },	// Superior Numeral 3
  { "199", "00B8", },	// Lowercase Cedilla (Spacing)
  { "200", "00B9", },	// Superior Numeral 1
  { "201", "00D7", },	// Multiply Sign
  { "202", "00F7", },	// Divide Sign
  { "203", "263A", },	// Open Smiling Face
  { "204", "263B", },	// Solid Smiling Face
  { "205", "2665", },	// Solid Heart, Card Suit
  { "206", "2666", },	// Solid Diamond, Card Suit
  { "207", "2663", },	// Solid Club, Card Suit
  { "208", "2660", },	// Solid Spade, Card Suit
  { "209", "25CF", },	// Medium Solid Round Bullet
  { "210", "25D8", },	// Large Solid square with White Dot
  { "211", "EFFD", },	// Large Open Round Bullet
  { "212", "25D9", },	// Large Solid square with White Circle
  { "213", "2642", },	// Male Symbol
  { "214", "2640", },	// Female Symbol
  { "215", "266A", },	// Musical Note
  { "216", "266B", },	// Pair Of Musical Notes
  { "217", "263C", },	// Compass, or Eight Pointed Sun
  { "218", "25BA", },	// Right Solid Arrowhead
  { "219", "25C4", },	// Left Solid Arrowhead
  { "220", "2195", },	// Up/Down Arrow
  { "221", "203C", },	// Double Exclamation Mark
  { "222", "25AC", },	// Thick Horizontal Mark
  { "223", "21A8", },	// Up/Down Arrow Baseline
  { "224", "2191", },	// Up Arrow
  { "225", "2193", },	// Down Arrow
  { "226", "2192", },	// Right Arrow
  { "227", "2190", },	// Left Arrow
  { "229", "2194", },	// Left/Right Arrow
  { "230", "25B2", },	// Up Solid Arrowhead
  { "231", "25BC", },	// Down Solid Arrowhead
  { "232", "20A7", },	// Pesetas Sign
  { "233", "2310", },	// Reversed Not Sign
  { "234", "2591", },	// Light Shading Character
  { "235", "2593", },	// Dark Shading Character
  { "236", "2502", },	// Box Draw Line, Vert. 1
  { "237", "2524", },	// Box Draw Right Tee, Vert. 1 Horiz. 1
  { "238", "2561", },	// Box Draw Right Tee, Vert. 1 Horiz. 2
  { "239", "2562", },	// Box Draw Right Tee, Vert. 2 Horiz. 1
  { "240", "2556", },	// Box Draw Upper Right Corner, Vert. 2 Horiz. 1
  { "241", "2555", },	// Box Draw Upper Right Corner, Vert. 1 Horiz. 2
  { "242", "2563", },	// Box Draw Right Tee, Vert. 2 Horiz. 2
  { "243", "2551", },	// Box Draw Lines, Vert. 2
  { "244", "2557", },	// Box Draw Upper Right Corner, Vert. 2 Horiz. 2
  { "245", "255D", },	// Box Draw Lower Right Corner, Vert. 2 Horiz. 2
  { "246", "255C", },	// Box Draw Lower Right Corner, Vert. 2 Horiz. 1
  { "247", "255B", },	// Box Draw Lower Right Corner, Vert. 1 Horiz. 2
  { "248", "2510", },	// Box Draw Upper Right Corner, Vert. 1, Horiz. 1
  { "249", "2514", },	// Box Draw Lower Left Corner, Vert. 1, Horiz. 1
  { "250", "2534", },	// Box Draw Bottom Tee, Vert. 1 Horiz. 1
  { "251", "252C", },	// Box Draw Top Tee, Vert. 1 Horiz. 1
  { "252", "251C", },	// Box Draw Left Tee, Vert. 1 Horiz. 1
  { "253", "2500", },	// Box Draw Line, Horiz. 1
  { "254", "253C", },	// Box Draw Cross, Vert. 1 Horiz. 1
  { "255", "255E", },	// Box Draw Left Tee, Vert. 1 Horiz. 2
  { "256", "255F", },	// Box Draw Left Tee, Vert. 2 Horz. 1
  { "257", "255A", },	// Box Draw Lower Left Corner, Vert. 2 Horiz. 2
  { "258", "2554", },	// Box Draw Upper Left Corner, Vert. 2 Horiz. 2
  { "259", "2569", },	// Box Draw Bottom Tee, Vert. 2 Horiz. 2
  { "260", "2566", },	// Box Draw Top Tee, Vert. 2 Horiz. 2
  { "261", "2560", },	// Box Draw Left Tee, Vert. 2 Horiz. 2
  { "262", "2550", },	// Box Draw Lines, Horiz. 2
  { "263", "256C", },	// Box Draw Cross Open Center, Vert. 2 Horiz. 2
  { "264", "2567", },	// Box Draw Bottom Tee, Vert. 1 Horiz. 2
  { "265", "2568", },	// Box Draw Bottom Tee, Vert. 2 Horiz. 1
  { "266", "2564", },	// Box Draw Top Tee, Vert. 1 Horiz. 2
  { "267", "2565", },	// Box Draw Top Tee, Vert. 2 Horiz. 1
  { "268", "2559", },	// Box Draw Lower Left Corner, Vert. 2 Horiz. 1
  { "269", "2558", },	// Box Draw Lower Left Corner, Vert. 1 Horiz. 2
  { "270", "2552", },	// Box Draw Upper Left Corner, Vert. 1 Horiz. 2
  { "271", "2553", },	// Box Draw Upper Left Corner, Vert. 2 Horiz. 1
  { "272", "256B", },	// Box Draw Cross, Vert. 2 Horiz. 1
  { "273", "256A", },	// Box Draw Cross, Vert. 1 Horiz. 2
  { "274", "2518", },	// Box Draw Lower Right Corner, Vert. 1 Horiz. 1
  { "275", "250C", },	// Box Draw Upper Left Corner, Vert. 1, Horiz. 1
  { "276", "2588", },	// Solid Full High/Wide
  { "277", "2584", },	// Bottom Half Solid Rectangle
  { "278", "258C", },	// Left Half Solid Rectangle
  { "279", "2590", },	// Right Half Solid Rectangle
  { "280", "2580", },	// Top Half Solid Rectangle
  { "290", "2126", },	// Uppercase Greek Omega, or Ohms
  { "292", "221E", },	// Infinity Symbol
  { "295", "2229", },	// Set Intersection Symbol
  { "296", "2261", },	// Exactly Equals Sign
  { "297", "2265", },	// Greater Than or Equal Sign
  { "298", "2264", },	// Less Than or Equal Sign
  { "299", "2320", },	// Top Integral
  { "300", "2321", },	// Bottom Integral
  { "301", "2248", },	// Two Wavy Line Approximate Sign
//{ "302", "00B7", },	// Middle Dot, or Centered Period (see 2219)
//{ "302", "2219", },	// Centered Period, Middle Dot
  { "302", "2219", },	// Math Dot, Centered Period
  { "303", "221A", },	// Radical Symbol, Standalone Diagonal
  { "305", "25AA", },	// Small Solid Square Box
  { "306", "013F", },	// Uppercase L-Dot
  { "307", "0140", },	// Lowercase L-Dot
  { "308", "2113", },	// Litre Symbol
  { "309", "0149", },	// Lowercase Apostrophe-N
  { "310", "2032", },	// Prime, Minutes, or Feet Symbol
  { "311", "2033", },	// Double Prime, Seconds, or Inches Symbol
  { "312", "2020", },	// Dagger Symbol
  { "313", "2122", },	// Trademark Sign
  { "314", "2017", },	// Double Underline Character
  { "315", "02C7", },	// Lowercase Hacek Accent (Spacing)
  { "316", "02DA", },	// Lowercase Ring Accent (Spacing)
  { "317", "EFF9", },	// Uppercase Acute Accent (Spacing)
  { "318", "EFF8", },	// Uppercase Grave Accent (Spacing)
  { "319", "EFF7", },	// Uppercase Circumflex Accent (Spacing)
  { "320", "EFF6", },	// Uppercase Dieresis Accent (Spacing)
  { "321", "EFF5", },	// Uppercase Tilde Accent (Spacing)
  { "322", "EFF4", },	// Uppercase Hacek Accent (Spacing)
  { "323", "EFF3", },	// Uppercase Ring Accent (Spacing)
  { "324", "2215", },	// Vulgar Fraction Bar
  { "325", "2014", },	// Em Dash
  { "326", "2013", },	// En Dash
  { "327", "2021", },	// Double Dagger Symbol
  { "328", "0131", },	// Lowercase Undotted I
  { "329", "0027", },	// Neutral Single Quote
  { "330", "EFF2", },	// Uppercase Cedilla (Spacing)
  { "331", "2022", },	// Small Solid Round Bullet
  { "332", "207F", },	// Superior Lowercase N
  { "333", "2302", },	// Home Plate
  { "335", "0138", },	// Lowercase Kra
  { "338", "0166", },	// Uppercase T-Stroke
  { "339", "0167", },	// Lowercase T-Stroke
  { "340", "014A", },	// Uppercase Eng
  { "341", "014B", },	// Lowercase Eng
  { "342", "0111", },	// Lowercase D-Stroke
  { "400", "0102", },	// Uppercase A Breve
  { "401", "0103", },	// Lowercase A Breve
  { "402", "0100", },	// Uppercase A Macron
  { "403", "0101", },	// Lowercase A Macron
  { "404", "0104", },	// Uppercase A Ogonek
  { "405", "0105", },	// Lowercase A Ogonek
  { "406", "0106", },	// Uppercase C Acute
  { "407", "0107", },	// Lowercase C Acute
  { "410", "010C", },	// Uppercase C Hacek
  { "411", "010D", },	// Lowercase C Hacek
  { "414", "010E", },	// Uppercase D Hacek
  { "415", "010F", },	// Lowercase D Hacek
  { "416", "011A", },	// Uppercase E Hacek
  { "417", "011B", },	// Lowercase E Hacek
  { "418", "0116", },	// Uppercase E Overdot
  { "419", "0117", },	// Lowercase E Overdot
  { "420", "0112", },	// Uppercase E Macron
  { "421", "0113", },	// Lowercase E Macron
  { "422", "0118", },	// Uppercase E Ogonek
  { "423", "0119", },	// Lowercase E Ogonek
  { "428", "0122", },	// Uppercase G Cedilla
  { "429", "0123", },	// Lowercase G Cedilla
  { "432", "012E", },	// Uppercase I Ogonek
  { "433", "012F", },	// Lowercase I Ogonek
  { "434", "012A", },	// Uppercase I Macron
  { "435", "012B", },	// Lowercase I Macron
  { "438", "0136", },	// Uppercase K Cedilla
  { "439", "0137", },	// Lowercase K Cedilla
  { "440", "0139", },	// Uppercase L Acute
  { "441", "013A", },	// Lowercase L Acute
  { "442", "013D", },	// Uppercase L Hacek
  { "443", "013E", },	// Lowercase L Hacek
  { "444", "013B", },	// Uppercase L Cedilla
  { "445", "013C", },	// Lowercase L Cedilla
  { "446", "0143", },	// Uppercase N Acute
  { "447", "0144", },	// Lowercase N Acute
  { "448", "0147", },	// Uppercase N Hacek
  { "449", "0148", },	// Lowercase N Hacek
  { "450", "0145", },	// Uppercase N Cedilla
  { "451", "0146", },	// Lowercase N Cedilla
  { "452", "0150", },	// Uppercase O Double Acute
  { "453", "0151", },	// Lowercase O Double Acute
  { "454", "014C", },	// Uppercase O Macron
  { "455", "014D", },	// Lowercase O Macron
  { "456", "0154", },	// Uppercase R Acute
  { "457", "0155", },	// Lowercase R Acute
  { "458", "0158", },	// Uppercase R Hacek
  { "459", "0159", },	// Lowercase R Hacek
  { "460", "0156", },	// Uppercase R Cedilla
  { "461", "0157", },	// Lowercase R Cedilla
  { "462", "015A", },	// Uppercase S Acute
  { "463", "015B", },	// Lowercase S Acute
  { "466", "0164", },	// Uppercase T Hacek
  { "467", "0165", },	// Lowercase T Hacek
  { "468", "0162", },	// Uppercase T Cedilla
  { "469", "0163", },	// Lowercase T Cedilla
  { "470", "0168", },	// Uppercase U Tilde
  { "471", "0169", },	// Lowercase U Tilde
  { "474", "0170", },	// Uppercase U Double Acute
  { "475", "0171", },	// Lowercase U Double Acute
  { "476", "016E", },	// Uppercase U Ring
  { "477", "016F", },	// Lowercase U Ring
  { "478", "016A", },	// Uppercase U Macron
  { "479", "016B", },	// Lowercase U Macron
  { "480", "0172", },	// Uppercase U Ogonek
  { "481", "0173", },	// Lowercase U Ogonek
  { "482", "0179", },	// Uppercase Z Acute
  { "483", "017A", },	// Lowercase Z Acute
  { "484", "017B", },	// Uppercase Z Overdot
  { "485", "017C", },	// Lowercase Z Overdot
  { "486", "0128", },	// Uppercase I Tilde
  { "487", "0129", },	// Lowercase I Tilde
  { "500", "EFBF", },	// Radical, Diagonal, Composite
  { "501", "221D", },	// Proportional To Symbol
  { "502", "212F", },	// Napierian (italic e)
  { "503", "03F5", },	// Alternate Lowercase Greek Epsilon
//{ "503", "EFEC", },	// Alternate Lowercase Greek Epsilon
  { "504", "2234", },	// Therefore Symbol
  { "505", "0393", },	// Uppercase Greek Gamma
  { "506", "2206", },	// Increment Symbol (Delta)
  { "507", "0398", },	// Uppercase Greek Theta
  { "508", "039B", },	// Uppercase Greek Lambda
  { "509", "039E", },	// Uppercase Greek Xi
  { "510", "03A0", },	// Uppercase Greek Pi
  { "511", "03A3", },	// Uppercase Greek Sigma
  { "512", "03A5", },	// Uppercase Greek Upsilon
  { "513", "03A6", },	// Uppercase Greek Phi
  { "514", "03A8", },	// Uppercase Greek Psi
  { "515", "03A9", },	// Uppercase Greek Omega
  { "516", "2207", },	// Nabla Symbol (inverted Delta)
  { "517", "2202", },	// Partial Differential Delta Symbol
  { "518", "03C2", },	// Lowercase Sigma, Terminal
  { "519", "2260", },	// Not Equal To Symbol
  { "520", "EFEB", },	// Underline, Composite
  { "521", "2235", },	// Because Symbol
  { "522", "03B1", },	// Lowercase Greek Alpha
  { "523", "03B2", },	// Lowercase Greek Beta
  { "524", "03B3", },	// Lowercase Greek Gamma
  { "525", "03B4", },	// Lowercase Greek Delta
  { "526", "03B5", },	// Lowercase Greek Epsilon
  { "527", "03B6", },	// Lowercase Greek Zeta
  { "528", "03B7", },	// Lowercase Greek Eta
  { "529", "03B8", },	// Lowercase Greek Theta
  { "530", "03B9", },	// Lowercase Greek Iota
  { "531", "03BA", },	// Lowercase Greek Kappa
  { "532", "03BB", },	// Lowercase Greek Lambda
  { "533", "03BC", },	// Lowercase Greek Mu
  { "534", "03BD", },	// Lowercase Greek Nu
  { "535", "03BE", },	// Lowercase Greek Xi
  { "536", "03BF", },	// Lowercase Greek Omicron
  { "537", "03C0", },	// Lowercase Greek Pi
  { "538", "03C1", },	// Lowercase Greek Rho
  { "539", "03C3", },	// Lowercase Greek Sigma
  { "540", "03C4", },	// Lowercase Greek Tau
  { "541", "03C5", },	// Lowercase Greek Upsilon
  { "542", "03C6", },	// Lowercase Greek Phi
  { "543", "03C7", },	// Lowercase Greek Chi
  { "544", "03C8", },	// Lowercase Greek Psi
  { "545", "03C9", },	// Lowercase Greek Omega
  { "546", "03D1", },	// Lowercase Greek Theta, Open
  { "547", "03D5", },	// Lowercase Greek Phi, Open
  { "548", "03D6", },	// Lowercase Pi, Alternate
  { "549", "2243", },	// Wavy Over Straight Approximate Symbol
  { "550", "2262", },	// Not Exactly Equal To Symbol
  { "551", "21D1", },	// Up Arrow Double Stroke
  { "552", "21D2", },	// Right Arrow Double Stroke
  { "553", "21D3", },	// Down Arrow Double Stroke
  { "554", "21D0", },	// Left Arrow Double Stroke
  { "555", "21D5", },	// Up/Down Arrow Double Stroke
  { "556", "21D4", },	// Left/Right Arrow Double Stroke
  { "557", "21C4", },	// Right Over Left Arrow
  { "558", "21C6", },	// Left Over Right Arrow
  { "559", "EFE9", },	// Vector Symbol
  { "560", "0305", },	// Overline, Composite
  { "561", "2200", },	// For All Symbol, or Universal (inverted A)
  { "562", "2203", },	// There Exists Symbol, or Existential (inverted E)
  { "563", "22A4", },	// Top Symbol
  { "564", "22A5", },	// Bottom Symbol
  { "565", "222A", },	// Set Union Symbol
  { "566", "2208", },	// Element-Of Symbol
  { "567", "220B", },	// Contains Symbol
  { "568", "2209", },	// Not-Element-Of Symbol
  { "569", "2282", },	// Proper Subset Symbol
  { "570", "2283", },	// Proper Superset Symbol
  { "571", "2284", },	// Not Proper Subset Symbol
  { "572", "2285", },	// Not Proper Superset Symbol
  { "573", "2286", },	// Subset Symbol
  { "574", "2287", },	// Superset Symbol
  { "575", "2295", },	// Plus In Circle Symbol
  { "576", "2299", },	// Dot In Circle Symbol
  { "577", "2297", },	// Times In Circle Symbol
  { "578", "2296", },	// Minus In Circle Symbol
  { "579", "2298", },	// Slash In Circle Symbol
  { "580", "2227", },	// Logical And Symbol
  { "581", "2228", },	// Logical Or Symbol
  { "582", "22BB", },	// Exclusive Or Symbol
  { "583", "2218", },	// Functional Composition Symbol
  { "584", "20DD", },	// Large Open Circle
  { "585", "22A3", },	// Assertion Symbol
  { "586", "22A2", },	// Backwards Assertion Symbol
  { "587", "222B", },	// Integral Symbol
  { "588", "222E", },	// Curvilinear Integral Symbol
  { "589", "2220", },	// Angle Symbol
  { "590", "2205", },	// Empty Set Symbol
  { "591", "2135", },	// Hebrew Aleph
  { "592", "2136", },	// Hebrew Beth
  { "593", "2137", },	// Hebrew Gimmel
  { "594", "212D", },	// Fraktur Uppercase C
  { "595", "2111", },	// Fraktur Uppercase I
  { "596", "211C", },	// Fraktur Uppercase R
  { "597", "2128", },	// Fraktur Uppercase Z
  { "598", "23A1", },	// Top Segment Left Bracket (Left Square Bracket Upper Corner)
  { "599", "23A3", },	// Bottom Segment Left Bracket (Left Square Bracket Lower Corner)
  { "600", "239B", },	// Top Segment Left Brace (Left Parenthesis Upper Hook)
//{ "600", "23A7", },	// Top Segment Left Brace (Right Curly Bracket Upper Hook)
  { "601", "23A8", },	// Middle Segment Left Brace (Right Curly Bracket Middle Piece)
  { "602", "239D", },	// Bottom Segment LeftBrace (Left Parenthesis Lower Hook)
//{ "602", "23A9", },	// Bottom Segment Left Brace (Right Curly Bracket Lower Hook)
  { "603", "EFD4", },	// Middle Segment Curvilinear Integral
  { "604", "EFD3", },	// Top Left Segment Summation
  { "605", "2225", },	// Double Vertical Line, Composite
  { "606", "EFD2", },	// Bottom Left Segment Summation
  { "607", "EFD1", },	// Bottom Diagonal Summation
  { "608", "23A4", },	// Top Segment Right Bracket (Right Square Bracket Upper Corner)
  { "609", "23A6", },	// Bottom Segment Right Bracket (Right Square Bracket Lower Corner)
  { "610", "239E", },	// Top Segment Right Brace (Right Parenthesis Upper Hook)
//{ "610", "23AB", },	// Top Segment Right Brace (Right Curly Bracket Upper Hook)
  { "611", "23AC", },	// Middle Segment Right Brace (Right Curly Bracket Middle Piece)
  { "612", "23A0", },	// Bottom Segment Right ( Right Parenthesis Lower Hook)
//{ "612", "23AD", },	// Bottom Segment Right Brace (Right Curly Bracket Lower Hook)
  { "613", "239C", },	// Thick Vertical Line, Composite (Left Parenthesis Extension)
//{ "613", "239F", },	// Thick Vertical Line, Composite (Right Parenthesis Extension)
//{ "613", "23AA", },	// Thick Vertical Line, Composite (Curly Bracket Extension)
//{ "613", "23AE", },	// Thick Vertical Line, Composite (Integral Extension)
  { "614", "2223", },	// Thin Vertical Line, Composite
  { "615", "EFDC", },	// Bottom Segment of Vertical Radical
  { "616", "EFD0", },	// Top Right Segment Summation
  { "617", "EFCF", },	// Middle Segment Summation
  { "618", "EFCE", },	// Bottom Right Segment Summation
  { "619", "EFCD", },	// Top Diagonal Summation
  { "620", "2213", },	// Minus Over Plus Sign
  { "621", "2329", },	// Left Angle Bracket
  { "622", "232A", },	// Right Angle Bracket
  { "623", "EFFF", },	// Mask Symbol
  { "624", "2245", },	// Wavy Over Two Straight Approximate Symbol
  { "625", "2197", },	// 45 Degree Arrow
  { "626", "2198", },	// -45 Degree Arrow
  { "627", "2199", },	// -135 Degree Arrow
  { "628", "2196", },	// 135 Degree Arrow
  { "629", "25B5", },	// Up Open Triangle
  { "630", "25B9", },	// Right Open Triangle
  { "631", "25BF", },	// Down Open Triangle
  { "632", "25C3", },	// Left Open Triangle
  { "633", "226A", },	// Much Less Than Sign
  { "634", "226B", },	// Much Greater Than Sign
  { "635", "2237", },	// Proportional To Symbol (4 dots)
  { "636", "225C", },	// Defined As Symbol
  { "637", "03DD", },	// Lowercase Greek Digamma
  { "638", "210F", },	// Planck's Constant divided by 2 pi
  { "639", "2112", },	// Laplace Transform Symbol
  { "640", "EFFE", },	// Power Set
  { "641", "2118", },	// Weierstrassian Symbol
  { "642", "2211", },	// Summation Symbol (large Sigma)
  { "643", "301A", },	// Left Double Bracket
  { "644", "EFC9", },	// Middle Segment Double Bracket
  { "645", "301B", },	// Right Double Bracket
  { "646", "256D", },	// Box Draw Left Top Round Corner
  { "647", "2570", },	// Box Draw Left Bottom Round Corner
  { "648", "EFC8", },	// Extender Large Union/Product
  { "649", "EFC7", },	// Bottom Segment Large Union
  { "650", "EFC6", },	// Top Segment Large Intersection
  { "651", "EFC5", },	// Top Segment Left Double Bracket
  { "652", "EFC4", },	// Bottom Segment Left Double Bracket
  { "653", "EFFC", },	// Large Open Square Box
  { "654", "25C7", },	// Open Diamond
  { "655", "256E", },	// Box Draw Right Top Round Corner
  { "656", "256F", },	// Box Draw Right Bottom Round Corner
  { "657", "EFC3", },	// Bottom Segment Large Bottom Product
  { "658", "EFC2", },	// Top Segment Large Top Product
  { "659", "EFC1", },	// Top Segment Right Double Bracket
  { "660", "EFC0", },	// Bottom Segment Right Double Bracket
  { "661", "EFFB", },	// Large Solid Square Box
  { "662", "25C6", },	// Solid Diamond
  { "663", "220D", },	// Such That Symbol (rotated lc epsilon)
  { "664", "2217", },	// Math Asterisk
  { "665", "23AF", },	// Horizontal Arrow Extender (Horizontal Line Extension)
  { "666", "EFCB", },	// Double Horizontal Arrow Extender
  { "667", "EFCC", },	// Inverted Complement of 0xEFCF or MSL 617
  { "668", "221F", },	// Right Angle Symbol
  { "669", "220F", },	// Product Symbol (large Pi)
  { "684", "25CA", },	// Lozenge, Diamond
  { "1000", "2070", },	// Superior Numeral 0
  { "1001", "2074", },	// Superior Numeral 4
  { "1002", "2075", },	// Superior Numeral 5
  { "1003", "2076", },	// Superior Numeral 6
  { "1004", "2077", },	// Superior Numeral 7
  { "1005", "2078", },	// Superior Numeral 8
  { "1006", "2079", },	// Superior Numeral 9
  { "1017", "201C", },	// Double Open Quote (6)
  { "1018", "201D", },	// Double Close Quote (9)
  { "1019", "201E", },	// Double Baseline Quote (9)
  { "1020", "2003", },	// Em Space
  { "1021", "2002", },	// En Space
  { "1023", "2009", },	// Thin Space
  { "1028", "2026", },	// Ellipsis
  { "1030", "EFF1", },	// Uppercase Ogonek (Spacing)
  { "1031", "017E", },	// Lowercase Z Hacek
  { "1034", "2120", },	// Service Mark
  { "1036", "211E", },	// Prescription Sign
//{ "1040", "F001", },	// Lowercase FI Ligature
  { "1040", "FB01", },	// Lowercase FI Ligature
//{ "1041", "F002", },	// Lowercase FL Ligature
  { "1041", "FB02", },	// Lowercase FL Ligature
  { "1042", "FB00", },	// Lowercase FF Ligature
  { "1043", "FB03", },	// Lowercase FFI Ligature
  { "1044", "FB04", },	// Lowercase FFL Ligature
  { "1045", "EFF0", },	// Uppercase Double Acute Accent (Spacing)
  { "1047", "0133", },	// Lowercase IJ Ligature
  { "1060", "2105", },	// Care Of Symbol
  { "1061", "011E", },	// Uppercase G Breve
  { "1062", "011F", },	// Lowercase G Breve
  { "1063", "015E", },	// Uppercase S Cedilla
  { "1064", "015F", },	// Lowercase S Cedilla
  { "1065", "0130", },	// Uppercase I Overdot
  { "1067", "201A", },	// Single Baseline Quote (9)
  { "1068", "2030", },	// Per Mill Sign
  { "1069", "20AC", },	// Euro
  { "1084", "02C9", },	// Lowercase Macron Accent (Spacing)
  { "1086", "02D8", },	// Lowercase Breve Accent (Spacing)
  { "1088", "02D9", },	// Lowercase Overdot Accent (Spacing)
  { "1090", "0153", },	// Lowercase OE Ligature
  { "1091", "0152", },	// Uppercase OE Ligature
  { "1092", "2039", },	// Left Pointing Single Angle Quote
  { "1093", "203A", },	// Right Pointing Single Angle Quote
  { "1094", "25A1", },	// Medium Open Square Box
  { "1095", "0141", },	// Uppercase L-Stroke
  { "1096", "0142", },	// Lowercase L-Stroke
  { "1097", "02DD", },	// Lowercase Double Acute Accent (Spacing)
  { "1098", "02DB", },	// Lowercase Ogonek (Spacing)
  { "1099", "21B5", },	// Carriage Return Symbol
  { "1100", "EFDB", },	// Full Size Serif Registered
  { "1101", "EFDA", },	// Full Size Serif Copyright
  { "1102", "EFD9", },	// Full Size Serif Trademark
  { "1103", "EFD8", },	// Full Size Sans Registered
  { "1104", "EFD7", },	// Full Size Sans Copyright
  { "1105", "EFD6", },	// Full Size Sans Trademark
  { "1106", "017D", },	// Uppercase Z Hacek
  { "1107", "0132", },	// Uppercase IJ Ligature
  { "1108", "25AB", },	// Small Open Square Box
  { "1109", "25E6", },	// Small Open Round Bullet
  { "1110", "25CB", },	// Medium Open Round Bullet
  { "1111", "EFFA", },	// Large Solid Round Bullet
  { "3812", "F000", },	// Ornament, Apple
};

// global constructor
static struct hp_msl_to_unicode_init {
  hp_msl_to_unicode_init();
} _hp_msl_to_unicode_init;

hp_msl_to_unicode_init::hp_msl_to_unicode_init() {
  for (unsigned int i = 0;
       i < sizeof(hp_msl_to_unicode_list)/sizeof(hp_msl_to_unicode_list[0]);
       i++) {
    hp_msl_to_unicode *ptu = new hp_msl_to_unicode[1];
    ptu->value = (char *)hp_msl_to_unicode_list[i].value;
    hp_msl_to_unicode_table.define(hp_msl_to_unicode_list[i].key, ptu);
  }
}

const char *hp_msl_to_unicode_code(const char *s)
{
  hp_msl_to_unicode *result = hp_msl_to_unicode_table.lookup(s);
  return result ? result->value : 0;
}
