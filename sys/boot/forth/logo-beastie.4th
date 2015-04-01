\ Copyright (c) 2003 Scott Long <scottl@FreeBSD.org>
\ Copyright (c) 2003 Aleksander Fafula <alex@fafula.com>
\ Copyright (c) 2006-2015 Devin Teske <dteske@FreeBSD.org>
\ All rights reserved.
\ 
\ Redistribution and use in source and binary forms, with or without
\ modification, are permitted provided that the following conditions
\ are met:
\ 1. Redistributions of source code must retain the above copyright
\    notice, this list of conditions and the following disclaimer.
\ 2. Redistributions in binary form must reproduce the above copyright
\    notice, this list of conditions and the following disclaimer in the
\    documentation and/or other materials provided with the distribution.
\ 
\ THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
\ ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
\ IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
\ ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
\ FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
\ DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
\ OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
\ HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
\ LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
\ OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
\ SUCH DAMAGE.
\ 
\ $FreeBSD$

46 logoX ! 4 logoY ! \ Initialize logo placement defaults

: logo ( x y -- ) \ color BSD mascot (19 rows x 34 columns)

2dup at-xy ."               [31m,        ," 1+
2dup at-xy ."              /(        )`" 1+
2dup at-xy ."              \ \___   / |" 1+
2dup at-xy ."              /- [37m_[31m  `-/  '" 1+
2dup at-xy ."             ([37m/\/ \[31m \   /\" 1+
2dup at-xy ."             [37m/ /   |[31m `    \" 1+
2dup at-xy ."             [34mO O   [37m) [31m/    |" 1+
2dup at-xy ."             [37m`-^--'[31m`<     '" 1+
2dup at-xy ."            (_.)  _  )   /" 1+
2dup at-xy ."             `.___/`    /" 1+
2dup at-xy ."               `-----' /" 1+
2dup at-xy ."  [33m<----.[31m     __ / __   \" 1+
2dup at-xy ."  [33m<----|====[31mO)))[33m==[31m) \) /[33m====|" 1+
2dup at-xy ."  [33m<----'[31m    `--' `.__,' \" 1+
2dup at-xy ."               |        |" 1+
2dup at-xy ."                \       /       /\" 1+
2dup at-xy ."           [36m______[31m( (_  / \______/" 1+
2dup at-xy ."         [36m,'  ,-----'   |" 1+
     at-xy ."         `--{__________)[37m"
;
