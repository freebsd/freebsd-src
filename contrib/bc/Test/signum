


/* From gnu@cygnus.com Wed Jul 14 13:46:44 1993
Return-Path: <gnu@cygnus.com>
To: phil@cs.wwu.edu, gnu@cygnus.com
Subject: bc/dc - no rest for the wicked
Date: Tue, 06 Jul 93 19:12:40 -0700
From: gnu@cygnus.com

GNU bc 1.02 passes all these tests.  Can you add the test to the distribution?
Putting it into a DejaGnu test case for GNU bc would be a great thing, too.
(I haven't seen the Signum paper, maybe you can dig it out.)

	John Gilmore
	Cygnus Support

------- Forwarded Message

Date: Tue, 6 Jul 93 08:45:48 PDT
From: uunet!Eng.Sun.COM!David.Hough@uunet.UU.NET (David Hough)
Message-Id: <9307061545.AA14477@dgh.Eng.Sun.COM>
To: numeric-interest@validgh.com
Subject: bc/dc - no rest for the wicked

Steve Sommars sent me a bc script which reproduces ALL the test cases from
Dittmer's paper.    Neither SunOS 5.2 on SPARC nor 5.1 on x86 come out clean.
Anybody else who has fixed all the bugs would be justified in 
bragging about it here.  */


/*Ingo Dittmer, ACM Signum, April 1993, page 8-11*/
define g(x,y,z){
	auto a
	a=x%y
	if(a!=z){
"
x=";x
	"y=";y
	"Should be ";z
	"was       ";a
	}
}

/*Table 1*/
g=g(53894380494284,9980035577,2188378484)
g=g(47907874973121,9980035577,3704203521)
g=g(76850276401922,9980035577,4002459022)
g=g(85830854846664,9980035577,2548884464)
g=g(43915353970066,9980035577,3197431266)
g=g(35930746212825,9980035577,2618135625)
g=g(51900604524715,9980035577,4419524315)
g=g(87827018005068,9980035577,2704927468)
g=g(57887902441764,9980035577,3696095164)
g=g(96810941031110,9980035577,4595934210)

/*Table 2*/
g=g(86833646827370,9980035577,7337307470)
g=g(77850880592435,9980035577,6603091835)
g=g(84836601050323,9980035577,6298645823)
g=g(85835110016211,9980035577,6804054011)
g=g(94817143459192,9980035577,6805477692)
g=g(94818870293481,9980035577,8532311981)
g=g(91823235571154,9980035577,6908262754)
g=g(59885451951796,9980035577,5238489796)
g=g(80844460893239,9980035577,6172719539)
g=g(67869195894693,9980035577,4953971093)
g=g(95813990985202,9980035577,5649446002)

/*Skip Table 3, duplicate of line 1, table 1*/

/*Table 4*/
g=g(28420950579078013018256253301,17987947258,16619542243)
g=g(12015118977201790601658257234,16687885701,8697335297)
g=g(14349070374946789715188912007,13712994561,3605141129)
g=g(61984050238512905451986475027,13337935089,5296182558)
g=g(86189707791214681859449918641,17837971389,14435206830)
g=g(66747908181102582528134773954,19462997965,8615839889)

/*Table 6*/
g=g(4999253,9998,253)
g=g(8996373,9995,873)


/* Added by Phil Nelson..... */
"end of tests
"
