// -*- C++ -*-
/* Copyright (C) 1994 Free Software Foundation, Inc.
     Written by Francisco Andrés Verdú <pandres@dragonet.es> with many ideas
     taken from the other groff drivers.


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
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA. */

/*
TODO

 - Add X command to include bitmaps
*/
#define _GNU_SOURCE

#include "driver.h"
#include "lbp.h"
#include "charset.h"

#include "nonposix.h"

#ifdef HAVE_STRNCASECMP
#ifdef NEED_DECLARATION_STRNCASECMP
extern "C" {
  // SunOS's string.h fails to declare this.
  int strncasecmp(const char *, const char *, int);
}
#endif /* NEED_DECLARATION_STRNCASECMP */
#endif /* HAVE_STRNCASECMP */

static short int papersize = -1,  // papersize
		 orientation = -1 , // orientation
		 paperlength = 0, // Custom Paper size
		 paperwidth = 0,
		 ncopies = 1; // Number of copies

class lbp_font : public font {
public:
  ~lbp_font();
  void handle_unknown_font_command(const char *command, const char *arg,
				   const char *filename, int lineno);
  static lbp_font *load_lbp_font(const char *);
  char *lbpname;
  char is_scalable;
private:
  lbp_font(const char *);
};

class lbp_printer : public printer {
public:
  lbp_printer();
  ~lbp_printer();
  void set_char(int, font *, const environment *, int, const char *name);
  void draw(int code, int *p, int np, const environment *env);
  void begin_page(int);
  void end_page(int page_length);
  font *make_font(const char *);
  void end_of_line();
private:
  void set_line_thickness(int size, int dot = 0);
  void vdmstart();
  void vdmflush(); // the name vdmend was already used in lbp.h
  void setfillmode(int mode);
  void polygon( int hpos,int vpos,int np,int *p);
  char *font_name(const lbp_font *f, const int siz);

  int fill_pattern;
  int fill_mode;
  int cur_hpos;
  int cur_vpos;
  lbp_font *cur_font;
  int cur_size;
  unsigned short cur_symbol_set;
  int line_thickness;
};

//   Compatibility section.
//
//   Here we define some functions not present in some of the targets 
//   platforms
#ifndef HAVE_STRSEP
// Solaris 8 doesn't have the strsep function
static char *strsep(char **pcadena, const char *delim)
{
  char *p;

	p = strtok(*pcadena,delim);
 	*pcadena = strtok(NULL,delim);
    return p;
 
};
#endif

#ifndef HAVE_STRDUP
// Ditto with OS/390 and strdup
static char *strdup(const char *s)
{
  char *result;

  result = (char *)malloc(strlen(s)+1);
  if (result != NULL) strcpy(result,s);
  return result;
  
}; // strdup

#endif
lbp_font::lbp_font(const char *nm)
: font(nm)
{
}

lbp_font::~lbp_font()
{
}

lbp_font *lbp_font::load_lbp_font(const char *s)
{
  lbp_font *f = new lbp_font(s);
  f->lbpname = NULL;
  f->is_scalable = 1; // Default is that fonts are scalable 
  if (!f->load()) {
        delete f;
    	return 0;
      }
    return f;
}


void lbp_font::handle_unknown_font_command(const char *command,
					   const char *arg,
					   const char *filename, int lineno)
{
  if (strcmp(command, "lbpname") == 0) {
      if (arg == 0)
	fatal_with_file_and_line(filename, lineno,
				 "`%1' command requires an argument",
				 command);
      this->lbpname = new char[strlen(arg)+1];
      strcpy(this->lbpname,arg);
      // We Recongnize bitmaped fonts by the first character of it's name
      if (arg[0] == 'N') this->is_scalable = 0;
    //  fprintf(stderr,"Loading font \"%s\" \n",arg);
  }; // if (strcmp(command, "lbpname")
   //   fprintf(stderr,"Loading font  %s \"%s\" in %s at %d\n",command,arg,filename,lineno);
};

static void wp54charset()
{
  int i;
  
  lbpputs("\033[714;100;29;0;32;120.}");
  for (i = 0; i < sizeof(symset) ; i++) lbpputc(symset[i]);
  lbpputs("\033[100;0 D");
  return ;
};

lbp_printer::lbp_printer()
: fill_pattern(1),
  fill_mode(0),
  cur_hpos(-1),
  cur_font(0),
  cur_size(0),
  cur_symbol_set(0),
  line_thickness(-1)
{
#ifdef SET_BINARY
 SET_BINARY(fileno(stdout));
#endif
 lbpinit(stdout);
 lbpputs("\033c\033;\033[2&z\033[7 I\033[?32h\033[?33h\033[11h");
 wp54charset(); // Define the new symbol set
 lbpputs("\033[7 I\033[?32h\033[?33h\033[11h");
 // Paper size handling
 if (orientation < 0) orientation = 0;// Default orientation is portrait
 if (papersize < 0) papersize = 14; // Default paper size is A4
 if (papersize < 80) // standard paper
	 lbpprintf("\033[%dp",(papersize | orientation));
 else  // Custom paper
 	lbpprintf("\033[%d;%d;%dp",(papersize | orientation),\
			paperlength,paperwidth);

 // Number of copies
 lbpprintf("\033[%dv\n",ncopies);

 lbpputs("\033[0u\033[1u\033P1y Grolbp\033\\");
 lbpmoveabs(0,0);
 lbpputs("\033[0t\033[2t");
 lbpputs("\033('$2\033)' 1"); // Primary symbol set IBML
 			     // Secondary symbol set IBMR1
 cur_symbol_set = 0;
};

lbp_printer::~lbp_printer()
{
  lbpputs("\033P1y\033\\");
  lbpputs("\033c\033<");
}

void lbp_printer::begin_page(int)
{
}

void lbp_printer::end_page(int)
{
  if (vdminited()) vdmflush();
  lbpputc('\f');
  cur_hpos = -1;
}

void lbp_printer::end_of_line()
{
   cur_hpos = -1;		// force absolute motion
}

char *lbp_printer::font_name(const lbp_font *f, const int siz)
{
  static char bfont_name[255] ; // The resulting font name
  char	type, // Italic, Roman, Bold
        ori, // Normal or Rotated
	*nam; // The font name without other data.
//	nam[strlen(f->lbpname)-2]; // The font name without other data.
  int cpi; // The font size in characters per inch 
  	   // (Bitmaped fonts are monospaced).


  /*    Bitmap font selection is ugly in this printer, so don't expect 
  	this function to be elegant. */
	
  bfont_name[0] = 0x00;
  if (orientation) // Landscape
  	ori = 'R';
  else		// Portrait
  	ori = 'N';
  type = f->lbpname[strlen(f->lbpname)-1];
  nam = new char[strlen(f->lbpname)-2];
  strncpy(nam,&(f->lbpname[1]),strlen(f->lbpname)-2);
  nam[strlen(f->lbpname)-2] = 0x00;
   // fprintf(stderr,"Bitmap font '%s' %d %c %c \n",nam,siz,type,ori);
  /* Since these fonts are avaiable only at certain sizes,
    10 and 17 cpi for courier,  12 and 17 cpi for elite,
    we adjust the resulting size. */
    cpi = 17;
    // Fortunately there were only two bitmaped fonts shiped with the printer.
    if (!strcasecmp(nam,"courier"))
    { // Courier font
        if (siz >= 12) cpi = 10;
	else cpi = 17;
    };
    if (!strcasecmp(nam,"elite"))
    {  // Elite font
        if (siz >= 10) cpi = 12;
        else cpi = 17;
    };

  // Now that we have all the data, let's generate the font name.
  if ((type != 'B') && (type != 'I')) // Roman font
     	sprintf(bfont_name,"%c%s%d",ori,nam,cpi);
  else
  	sprintf(bfont_name,"%c%s%d%c",ori,nam,cpi,type);

  return bfont_name;
	
}; // lbp_printer::font_name

void lbp_printer::set_char(int index, font *f, const environment *env, int w, const char *name)
{
  int code = f->get_code(index);

  unsigned char ch = code & 0xff;
  unsigned short symbol_set = code >> 8;
  if (f != cur_font) {
    lbp_font *psf = (lbp_font *)f;
    // fprintf(stderr,"Loading font %s \"%d\" \n",psf->lbpname,env->size);
    if (psf->is_scalable)
    {  // Scalable font selection is different from bitmaped
    	lbpprintf("\033Pz%s.IBML\033\\\033[%d C",psf->lbpname,\
	(int)((env->size*300)/72));
    } else 
    {  // Bitmaped font
       lbpprintf("\033Pz%s.IBML\033\\\n",font_name(psf,env->size));
    };
    lbpputs("\033)' 1"); // Select IBML and IBMR1 symbol set
    cur_size = env->size;
    cur_font = psf;
    cur_symbol_set = 0;
  }
  if (symbol_set != cur_symbol_set) {
    if ( cur_symbol_set == 3 ) { 
       	// if current symbol set is Symbol we must restore the font
      	lbpprintf("\033Pz%s.IBML\033\\\033[%d C",cur_font->lbpname,\
						(int)((env->size*300)/72));
    }; // if ( cur_symbol_set == 3 )
    switch (symbol_set) {
    	case 0: lbpputs("\033('$2\033)' 1"); // Select IBML and IBMR1 symbol sets
		break;
	case 1:	lbpputs("\033(d\033)' 1"); // Select wp54 symbol set
		break;
	case 2:	lbpputs("\033('$2\033)'!0"); // Select IBMP symbol set
		break;
	case 3:	lbpprintf("\033PzSymbol.SYML\033\\\033[%d C",\
						(int)((env->size*300)/72));
		lbpputs("\033(\"!!0\033)\"!!1"); // Select symbol font 
		break;
	case 4:	lbpputs("\033)\"! 1\033(\"!$2"); // Select PS symbol set
		break;
    }; // switch (symbol_set)
		
//    if (symbol_set == 1) lbpputs("\033(d"); // Select wp54 symbol set
//    else lbpputs("\033('$2\033)' 1"); // Select IBML and IBMR1 symbol sets
    cur_symbol_set = symbol_set;
  }
  if (env->size != cur_size) {
  
    if (!cur_font->is_scalable)
       lbpprintf("\033Pz%s.IBML\033\\\n",font_name(cur_font,env->size));
    else
       lbpprintf("\033[%d C",(int)((env->size*300)/72));
    cur_size = env->size;
  }
 if ((env->hpos != cur_hpos) || (env->vpos != cur_vpos))
  {
  	// lbpmoveabs(env->hpos - ((5*300)/16),env->vpos );
  	lbpmoveabs(env->hpos - 64,env->vpos - 64 );
	cur_vpos = env->vpos;
   	cur_hpos = env->hpos;
   };
  if ((ch & 0x7F) < 32) lbpputs("\033[1.v");
  lbpputc(ch);
  cur_hpos += w;
};

void
lbp_printer::vdmstart()
{
  FILE *f;
  static int changed_origin = 0;
  
   errno = 0;
   f = tmpfile();
   // f = fopen("/tmp/gtmp","w+");
   if (f == NULL) perror("Openinig temp file");
   vdminit(f);
   if (!changed_origin) { // we should change the origin only one time
     changed_origin = 1;
     vdmorigin(-63,0);
     };
   vdmlinewidth(line_thickness);
   
};

void
lbp_printer::vdmflush()
{
  char buffer[1024];
  int bytes_read = 1;
 
  vdmend();
  fflush(lbpoutput);
  /* lets copy the vdm code to the output */
  rewind(vdmoutput);
  do
  {
    bytes_read = fread(buffer,1,sizeof(buffer),vdmoutput);
    bytes_read = fwrite(buffer,1,bytes_read,lbpoutput);
  } while ( bytes_read == sizeof(buffer));
  
 fclose(vdmoutput); // This will also delete the file, 
 		    // since it is created by tmpfile()
 vdmoutput = NULL;
 
}; // lbp_printer::vdmflush

inline void
lbp_printer::setfillmode(int mode)
{
  if (mode != fill_mode) {
  	if (mode != 1) vdmsetfillmode(mode,1,0);
	else vdmsetfillmode(mode,1,1); 	// To get black we must use white
				 	// inverted
  	fill_mode = mode;
  };
}; // setfillmode

inline void
lbp_printer::polygon( int hpos,int vpos,int np,int *p)
{
 //int points[np+2],i;
 int *points,i;

 points = new int[np+2];
 points[0] = hpos;
 points[1] = vpos;
/* fprintf(stderr,"Poligon (%d,%d) ", points[0],points[1]);*/
 for (i = 0; i < np; i++) points[i+2] = p[i];
/* for (i = 0; i < np; i++) fprintf(stderr," %d ",p[i]);
 fprintf(stderr,"\n"); */
 vdmpolygon((np /2) + 1,points);
};

void lbp_printer::draw(int code, int *p, int np, const environment *env)
{
   switch (code) {
   	case 't': 
      	  	if (np == 0) line_thickness = 1;
      	  	else { // troff gratuitously adds an extra 0
	    		if (np != 1 && np != 2) {
	  		error("0 or 1 argument required for thickness");
	  		break;
	  	        } // if (np != ...
			if (p[0] == 0) line_thickness = 1;
			if (p[0] < 0) // Default = 1 point
				line_thickness = (int)(env->size*30/72);
			line_thickness = (int)((abs(p[0])*env->size)/10);
			if ((line_thickness > 16 ) && (!vdminited()))
			{ /* for greater thickness we must use VDM */
			  vdmstart();
			  /* vdmlinewidth(line_thickness); already done in
			   * vdmstart() */
			};
			if (vdminited()) vdmlinewidth(line_thickness);
			// fprintf(stderr,"\nthickness: %d == %d, size %d\n",
			//         p[0],line_thickness,env->size );
		} // else
		break;

   	case 'l':  // Line
	     if (np != 2) {
	      error("2 arguments required for line");
	      break;
	     };
       	     if (!vdminited()) vdmstart();
	     vdmline(env->hpos,env->vpos,p[0],p[1]);
	     /*fprintf(stderr,"\nline: %d,%d - %d,%d thickness %d == %d\n",\
	     		env->hpos - 64,env->vpos -64, env->hpos - 64 + p[0],\
			env->vpos -64 + p[1],env->size, line_thickness);*/
	     break;
	case 'R': // Rule
	     if (np != 2) {
	      error("2 arguments required for Rule");
	      break;
	     }
	     if (vdminited()) {
	     		setfillmode(fill_pattern); // Solid Rule
		  	vdmrectangle(env->hpos,env->vpos,p[0],p[1]);
	     }
	     else {
		     lbpruleabs(env->hpos - 64,env->vpos -64 ,  p[0],  p[1]);
		     cur_vpos = p[1];
		     cur_hpos = p[0];
	     }; 
	     fprintf(stderr,"\nrule: thickness %d == %d\n", env->size, line_thickness);
	     break;
	case 'P': // Filled Polygon	     
		if (!vdminited()) vdmstart();
		setfillmode(fill_pattern);
		polygon(env->hpos,env->vpos,np,p);
		break;
	case 'p': // Empty Polygon
		if (!vdminited()) vdmstart();
		setfillmode(0);
		polygon(env->hpos,env->vpos,np,p);
		break;
	case 'C': // Filled Circle
		if (!vdminited()) vdmstart();
		// fprintf(stderr,"Circle (%d,%d) Fill %d\n",env->hpos,env->vpos,fill_pattern);
		setfillmode(fill_pattern);
		vdmcircle(env->hpos + (p[0]/2),env->vpos,p[0]/2);
		break;
	case 'c': // Empty Circle
		if (!vdminited()) vdmstart();
		setfillmode(0);
		vdmcircle(env->hpos + (p[0]/2),env->vpos,p[0]/2);
		break;
	case 'E': // Filled Ellipse
		if (!vdminited()) vdmstart();
		setfillmode(fill_pattern);
		vdmellipse(env->hpos + (p[0]/2),env->vpos,p[0]/2,p[1]/2,0);
		break;
	case 'e': // Empty Ellipse
		if (!vdminited()) vdmstart();
		setfillmode(0);
		vdmellipse(env->hpos + (p[0]/2),env->vpos,p[0]/2,p[1]/2,0);
		break;
	case 'a': // Arc
		if (!vdminited()) vdmstart();
		setfillmode(0);
		// VDM draws arcs clockwise and pic counterclockwise
		// We must compensate for that, exchanging the starting and
		// ending points
		vdmvarc(env->hpos + p[0],env->vpos+p[1],\
				int(sqrt( double((p[0]*p[0])+(p[1]*p[1])))),\
				p[2],p[3],\
				(-p[0]),(-p[1]),1,2);	
		break;
	case '~': // Spline
		if (!vdminited()) vdmstart();
		setfillmode(0);
		vdmspline(np/2,env->hpos,env->vpos,p);
		break;
  	case 'f':
    		if (np != 1 && np != 2) {
		      error("1 argument required for fill");
		      break;
    		};
		// fprintf(stderr,"Fill %d\n",p[0]);
		if ((p[0] == 1) || (p[0] >= 1000)) { // Black
			fill_pattern = 1; 
			break;
		}; // if (p[0] == 1)
		if (p[0] == 0) { // White
			fill_pattern = 0;
			break;
		};
		if ((p[0] > 1) && (p[0] < 1000))
		{
		  if (p[0] >= 990)  fill_pattern = -23; 
		  else if (p[0] >= 700)  fill_pattern = -28;
		  else if (p[0] >= 500)  fill_pattern = -27;
		  else if (p[0] >= 400)  fill_pattern = -26;
		  else if (p[0] >= 300)  fill_pattern = -25;
		  else if (p[0] >= 200)  fill_pattern = -22;
		  else if (p[0] >= 100)  fill_pattern = -24;
		  else fill_pattern = -21;
		}; // if (p[0] >= 0 && p[0] <= 1000)
		break;
     	default:
	     error("unrecognised drawing command `%1'", char(code));
	     break;
  }; // switch (code)
  return ;				
};

font *lbp_printer::make_font(const char *nm)
{
  return lbp_font::load_lbp_font(nm);
}

  

printer *make_printer()
{
  return new lbp_printer;
}

static struct 
{
    const char *name;
    int code;
} papersizes[] = 
{{ "A4", 14 },
{ "letter", 30 },
{ "legal", 32 },
{ "executive", 40 },
};


static int set_papersize(const char *papersize)
{
  int i;

  // First test for a standard (i.e. supported directly by the printer)
  // papersize
  for (i = 0 ; i < sizeof(papersizes)/sizeof(papersizes[0]); i++)
  {
  	if (strcasecmp(papersizes[i].name,papersize) == 0) 
				return papersizes[i].code; 
   };
   
   // Now test for a custom papersize
   if (strncasecmp("cust",papersize,4) == 0)
   {
     char *p , 
     	  *p1, 
	  *papsize;
	  
      p = papsize = strdup(&papersize[4]);
      if (papsize == NULL) return -1;
      p1 = strsep(&p,"x");
      if (p == NULL) 
      {  // let's test for an uppercase x
        p = papsize ;
        p1 = strsep(&p,"X");
        if (p == NULL) { free(papsize); return -1;};
      }; // if (p1 == NULL)
      paperlength = atoi(p1);
      if (paperlength == 0) { free(papsize);  return -1;};
      paperwidth = atoi(p);
      if (paperwidth == 0) { free(papsize); return -1;};
      free(papsize);
      return 82;
    }; // if (strcnasecmp("cust",papersize,4) == 0)
      
   return -1;
};

static int handle_papersize_command(const char *arg)
{
  int n = set_papersize(arg);

      if (n < 0)
      {  // If is not a standard nor custom paper size
         // let's see if it's a file (i.e /etc/papersize )
	 FILE *f = fopen(arg,"r");
	 if (f != NULL)
	 {  // the file exists and is readable
	    char psize[255],*p;
	    fgets(psize,254,f);
	    fclose(f);
	    // set_papersize doesn't like the trailing \n
	    p = psize; while (*p) p++;
	    if (*(--p) == '\n') *p = 0x00;
	    
	    n = set_papersize(psize);
	  }; // if (f != NULL)
       }; // if (n < 0)
       
   return n;
}; // handle_papersize_command


static void handle_unknown_desc_command(const char *command, const char *arg,
					const char *filename, int lineno)
{
  // papersize command
  if (strcasecmp(command, "papersize") == 0) {
    // We give priority to command line options
    if (papersize > 0) return;
    if (arg == 0)
      error_with_file_and_line(filename, lineno,
			       "`papersize' command requires an argument");
    else 
     {
       int n = handle_papersize_command(arg);
       if (n < 0)
	error_with_file_and_line(filename, lineno,
				 "unknown paper size `%1'", arg);
       else
	papersize = n;

      }; // if (arg == 0) ... else ...
    }; // if (strcasecmp(command, "papersize")

  // orientation command  
  if (strcasecmp(command, "orientation") == 0) {
    // We give priority to command line options
    if (orientation > 0) return;
    if (arg == 0)
      error_with_file_and_line(filename, lineno,
			       "`papersize' command requires an argument");
    else {
    	if (strcasecmp(arg,"portrait") == 0) orientation = 0;
	else { if (strcasecmp(arg,"landscape") == 0) orientation = 1;
	     else error_with_file_and_line(filename, lineno,
	     	  "`orientation' command requires an argument");
	     };
    }; // if (arg == 0) ... else ...
  }; // if (strcasecmp(command, "orientation") == 0) 
};

static struct option long_options[] = {
  {"orientation",1,NULL,'o'},
  {"version",0,NULL,'v'},
  {"copies",1,NULL,'c'},
  {"landscape",0,NULL,'l'},
  {"papersize",1,NULL,'p'},
  {"fontdir",1,NULL,'F'},
  {"help",0,NULL,'h'},
  {0, 0, 0, 0}
 };

static void usage()
{
  fprintf(stderr,
	  "usage: %s [-lvh] [-c n] [-p paper_size] [-F dir] [-o or] "\
	  " [files ...]\n"\
	  "          -o --orientation=[portrait|landscape]\n"\
	  "	  -v --version\n"\
	  "	  -c --copies=numcopies\n"\
	  "	  -l --landscape\n"\
	  "	  -p --papersize=paper_size\n"\
	  "	  -F --fontdir=dir\n"\
	  "	  -h --help\n",
	  program_name);
  exit(1);
}; // usage

int main(int argc, char **argv)
{
   if (program_name == NULL) program_name = strdup(argv[0]);
   
	font::set_unknown_desc_command_handler(handle_unknown_desc_command);
	// command line parsing
  	int c = 0;
	int  option_index = 0;

	while (c >= 0 ) 
	{
		c = getopt_long (argc, argv, "F:p:lvo:c:h",\
				long_options, &option_index);
		switch (c) {
    		  case 'F'  : 	font::command_line_font_dir(optarg);
	      			break;
    		  case 'p'  : {
				int n = handle_papersize_command(optarg);
				if (n < 0)
	  			   error("unknown paper size `%1'", optarg);
				else
	  			  papersize = n;
				break;
      			      };
		  case 'l'  : 	orientation = 1;
		  		break;
		  case 'v'  :	{
				extern const char *version_string;
				fprintf(stderr, "grolbp version %s\n",\
				version_string);
				fflush(stderr);
				break;
      				};
		  case 'o'  : {
    				if (strcasecmp(optarg,"portrait") == 0) 
							orientation = 0;
				else { 
				   if (strcasecmp(optarg,"landscape") == 0) 
				   			orientation = 1;
				   else 
				     error("unknown orientation '%1'", optarg);
				 }; 
                                break;
			      };
    		  case 'c'  : {
				char *ptr;
				long n = strtol(optarg, &ptr, 10);
				if ((n <= 0) && (ptr == optarg))
	  				error("argument for -c must be a positive integer");
				else if (n <= 0 || n > 32767)
	  				error("out of range argument for -c");
				     else
	  				ncopies = unsigned(n);
				break;
      			      }
    		  case 'h'  : usage();
      			      break;
				
				
		}; // switch (c)
	}; // while (c > 0 )
			
  	if (optind >= argc)
    		do_file("-");

	while (optind < argc) {	
		do_file(argv[optind++]);
	};
		
	lbpputs("\033c\033<");
	return 0;
};
