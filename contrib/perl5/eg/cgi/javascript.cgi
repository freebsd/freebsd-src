#!/usr/local/bin/perl

# This script illustrates how to use JavaScript to validate fill-out
# forms.
use CGI qw(:standard);

# Here's the javascript code that we include in the document.
$JSCRIPT=<<EOF;
    // validate that the user is the right age.  Return
    // false to prevent the form from being submitted.
    function validateForm() {
	var today = new Date();
	var birthday = validateDate(document.form1.birthdate);
	if (birthday == 0) {
	    document.form1.birthdate.focus()
	    document.form1.birthdate.select();
	    return false;
	}
	var milliseconds = today.getTime()-birthday;
	var years = milliseconds/(1000 * 60 * 60 * 24 * 365.25);
	if ((years > 20) || (years < 5)) {
	    alert("You must be between the ages of 5 and 20 to submit this form");
	    document.form1.birthdate.focus();
	    document.form1.birthdate.select();
	    return false;
	}
	// Since we've calculated the age in years already,
	// we might as well send it up to our CGI script.
	document.form1.age.value=Math.floor(years);
	return true;
    }

   // make sure that the contents of the supplied
   // field contain a valid date.
   function validateDate(element) {
       var date = Date.parse(element.value);
       if (0 == date) { 
	   alert("Please enter date in format MMM DD, YY");
	   element.focus();
	   element.select();
       }
       return date;
   }

   // Compliments, compliments
    function doPraise(element) {
	if (element.checked) {
	    self.status=element.value + " is an excellent choice!";
	    return true;
	} else {
	    return false;
	}
    }

    function checkColor(element) {
	var color = element.options[element.selectedIndex].text;
	if (color == "blonde") {
	    if (confirm("Is it true that blondes have more fun?"))
		alert("Darn.  That leaves me out.");
	} else
	    alert(color + " is a fine choice!");
    }
EOF
    ;

# here's where the execution begins
print header;
print start_html(-title=>'Personal Profile',-script=>$JSCRIPT);

print h1("Big Brother Wants to Know All About You"),
    strong("Note: "),"This page uses JavaScript and requires ",
    "Netscape 2.0 or higher to do anything special.";

&print_prompt();
print hr;
&print_response() if param;
print end_html;

sub print_prompt {
    print start_form(-name=>'form1',
		     -onSubmit=>"return validateForm()"),"\n";
    print "Birthdate (e.g. Jan 3, 1972): ", 
          textfield(-name=>'birthdate',
			-onBlur=>"validateDate(this)"),"<p>\n";
    print "Sex: ",radio_group(-name=>'gender',
				  -value=>[qw/male female/],
				  -onClick=>"doPraise(this)"),"<p>\n";
    print "Hair color: ",popup_menu(-name=>'color',
					-value=>[qw/brunette blonde red gray/],
					-default=>'red',
					-onChange=>"checkColor(this)"),"<p>\n";
    print hidden(-name=>'age',-value=>0);
    print submit();
    print end_form;
}

sub print_response {
    import_names('Q');
    print h2("Your profile"),
	"You claim to be a ",b($Q::age)," year old ",b($Q::color,$Q::gender),".",
	"You should be ashamed of yourself for lying so ",
	"blatantly to big brother!",
	hr;
}

