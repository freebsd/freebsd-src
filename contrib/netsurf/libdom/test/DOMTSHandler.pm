# This is the PerSAX Handlers Package

package DOMTSHandler;

use Switch;

use XML::XPath;
use XML::XPath::XMLParser;

our $description = 0;
our $string_index = 0;
our $ret_index = 0;
our $condition_index = 0;
our $test_index = 0;
our $iterator_index = 0;
our $temp_index = 0;
# Sometimes, we need temp nodes
our $tnode_index = 0;
our $dom_feature = "\"XML\"";
our %bootstrap_api = (
	dom_implementation_create_document_type => "",
	dom_implementation_create_document	=> "",
);
our %native_interface = (
	DOMString => \&generate_domstring_interface,
	DOMTimeStamp => "",
	DOMUserData => "",
	DOMObject =>"",
);
our %special_type = (
	# Some of the type are not defined now!
	boolean => "bool ",
	int => "int32_t ",
	"unsigned long" => "uint32_t ",
	DOMString => "dom_string *",
	List => "list *",
	Collection => "list *",
	DOMImplementation => "dom_implementation *",
	NamedNodeMap => "dom_namednodemap *",
	NodeList => "dom_nodelist *",
        HTMLCollection => "dom_html_collection *",
        HTMLFormElement => "dom_html_form_element *",
	CharacterData => "dom_characterdata *",
	CDATASection => "dom_cdata_section *",
);
our %special_prefix = (
	DOMString => "dom_string",
	DOMImplementation => "dom_implementation",
	NamedNodeMap => "dom_namednodemap",
	NodeList => "dom_nodelist",
        HTMLCollection => "dom_html_collection",
        HTMLFormElement => "dom_html_form_element",
	CharacterData => "dom_characterdata",
	CDATASection => "dom_cdata_section *",
        HTMLHRElement => "dom_html_hr_element",
);

our %unref_prefix = (
	DOMString => "dom_string",
	NamedNodeMap => "dom_namednodemap",
	NodeList => "dom_nodelist",
        HTMLCollection => "dom_html_collection",
);

our %special_method = (
);

our %special_attribute = (
	namespaceURI => "namespace",
);

our %no_unref = (
	"boolean" => 1,
	"int" => 1,
	"unsigned int" => 1,
	"List" => 1,
	"Collection" => 1,
);

our %override_suffix = (
	boolean => "bool",
	int => "int",
	"unsigned long" => "unsigned_long",
	DOMString => "domstring",
	DOMImplementation => "domimplementation",
	NamedNodeMap => "domnamednodemap",
	NodeList => "domnodelist",
        HTMLCollection => "domhtmlcollection",
	Collection => "list",
	List => "list",
);

our %exceptions = (
	
	DOM_NO_ERR			=>  0,
	DOM_INDEX_SIZE_ERR		=>  1,
	DOM_DOMSTRING_SIZE_ERR		=>  2,
	DOM_HIERARCHY_REQUEST_ERR	=>  3,
	DOM_WRONG_DOCUMENT_ERR		=>  4,
	DOM_INVALID_CHARACTER_ERR	=>  5,
	DOM_NO_DATA_ALLOWED_ERR		=>  6,
	DOM_NO_MODIFICATION_ALLOWED_ERR	=>  7,
	DOM_NOT_FOUND_ERR		=>  8,
	DOM_NOT_SUPPORTED_ERR		=>  9,
	DOM_INUSE_ATTRIBUTE_ERR		=> 10,
	DOM_INVALID_STATE_ERR		=> 11,
	DOM_SYNTAX_ERR			=> 12,
	DOM_INVALID_MODIFICATION_ERR	=> 13,
	DOM_NAMESPACE_ERR		=> 14,
	DOM_INVALID_ACCESS_ERR		=> 15,
	DOM_VALIDATION_ERR		=> 16,
	DOM_TYPE_MISMATCH_ERR		=> 17,

	DOM_UNSPECIFIED_EVENT_TYPE_ERR  => (1<<30)+0,
	DOM_DISPATCH_REQUEST_ERR        => (1<<30)+1,

	DOM_NO_MEM_ERR			=> (1<<31)+0, 
);

our @condition = qw(same equals notEquals less lessOrEquals greater greaterOrEquals isNull notNull and or xor not instanceOf isTrue isFalse hasSize contentType hasFeature implementationAttribute);

our @exception = qw(INDEX_SIZE_ERR DOMSTRING_SIZE_ERR HIERARCHY_REQUEST_ERR WRONG_DOCUMENT_ERR INVALID_CHARACTER_ERR NO_DATA_ALLOWED_ERR NO_MODIFICATION_ALLOWED_ERR NOT_FOUND_ERR NOT_SUPPORTED_ERR INUSE_ATTRIBUTE_ERR NAMESPACE_ERR UNSPECIFIED_EVENT_TYPE_ERR DISPATCH_REQUEST_ERR);

our @assertion = qw(assertTrue assertFalse assertNull assertNotNull assertEquals assertNotEquals assertSame assertInstanceOf assertSize assertEventCount assertURIEquals);

our @assertexception = qw(assertDOMException assertEventException assertImplementationException);

our @control = qw(if while for-each else);

our @framework_statement = qw(assign increment decrement append plus subtract mult divide load implementation comment hasFeature implementationAttribute EventMonitor.setUserObj EventMonitor.getAtEvents EventMonitor.getCaptureEvents EventMonitor.getBubbleEvents EventMonitor.getAllEvents wait);

sub new {
	my $type = shift;
	my $dtd = shift;
        my $chdir = shift;
	my $dd = XML::XPath->new(filename => $dtd);
	my $self = {
			# The DTD file of the xml files
			dd => $dd,
			# To indicate whether we are in comments
			comment => 0,
			# To indicate that whether we are in <comment> element
			inline_comment => 0,
			# The stack of elements encountered utill now
			context => [],
			# The map for <var> name => type
			var => {},
			# See the comment on generate_condition2 for this member
			condition_stack => [],
			# The list for UNREF
			unref => [],
			string_unref => [],
			# The indent of current statement
			indent => "",
			# The variables for List/Collection
			# We now, declare an array for a list and then add them into a list
			# The map for all the List/Collection in one test
			# "List Name" => "Member type"
			list_map => {},
			# The name of the current List/Collection
			list_name => "",
			# The number of items of the current List/Collection
			list_num => 0,
			# Whether List/Collection has members
			list_hasmem => 0,
			# The type of the current List/Collection
			list_type => "",
			# Whether we are in exception assertion
			exception => 0,
                        # Where to chdir
                        chdir => $chdir
			};

	return bless $self, $type;
}

sub start_element {
	my ($self, $element) = @_;

	my $en = $element->{Name};

	my $dd = $self->{dd};
	my $ct = $self->{context};
	push(@$ct, $en);

	switch ($en) {
		case "test" {
			;
		}
		case "metadata" {
			# start comments here
			print "/*\n";
			$self->{comment} = 1;
		}

		# Print the var definition
		case "var" {
			$self->generate_var($element->{Attributes});
		}

		case "member" {
			if ($self->{context}->[-2] eq "var") {
				if ($self->{"list_hasmem"} eq 1) {
					print ", ";
				}
				$self->{"list_hasmem"} = 1;
				$self->{"list_num"} ++;
			}
		}


		# The framework statements
		case [@framework_statement] {
			# Because the implementationAttribute & hasFeature belongs to both 
			# framework-statement and condition, we should distinct the two 
			# situation here. Let the generate_condtion to do the work.
			if ($en eq "hasFeature" || $en eq "implementationAttribute") {
				next;
			}

			$self->generate_framework_statement($en, $element->{Attributes});
		}

		case [@control] {
			$self->generate_control_statement($en, $element->{Attributes});
		}

		# Test condition
		case [@condition] {
			$self->generate_condition($en, $element->{Attributes});
		}

		# The assertsions
		case [@assertion] {
			$self->generate_assertion($en, $element->{Attributes});
		}
		
		case [@assertexception] {
			# Indeed, nothing to do here!
		}

		# Deal with exception
		case [@exception] {
			# Just see end_element
			$self->{'exception'} = 1;
		}

		# Then catch other case
		else {
			# we don't care the comment nodes
			if ($self->{comment} eq 0) {
				$self->generate_interface($en, $element->{Attributes});
			}
		}
	}
}

sub end_element {
	my ($self, $element) = @_;

	my @ct = @{$self->{context}};
	my $name = pop(@{$self->{context}});

	switch ($name) {
		case "metadata" {
			print "*/\n";
			$self->{comment} = 0;
			$self->generate_main();
		}
		case "test" {
			$self->cleanup();
		}

		case "var" {
			$self->generate_list();
		}

		# End of condition
		case [@condition] {
			$self->complete_condition($name);
		}

		# The assertion
		case [@assertion] {
			$self->complete_assertion($name);
		}

		case [@control] {
			$self->complete_control_statement($name);
		}

		case [@exception] {
			$name = "DOM_".$name;
			print "assert(exp == $exceptions{$name});\n";
			$self->{'exception'} = 0;
		}

	}
}

sub characters {
	my ($self, $data) = @_;
	our $description;

	my $ct = $self->{context};

	if ($self->{"inline_comment"} eq 1) {
		print "$data->{Data}";
		return ;
	}

	# We print the comments here
	if ($self->{comment} eq 1) {
		# So, we are in comments state
		my $top = $ct->[$#{$ct}];
		if ($top eq "metadata") {
			return;
		}

		if ($top eq "description") {
			if ($description eq 0) {
				print "description: \n";
				$description = 1;
			}
			print "$data->{Data}";
		} else {
			print "$top: $data->{Data}\n";
		}
		return;
	}

	if ($self->{context}->[-1] eq "member") {
		# We should mark that the List/Collection has members
		$self->{"list_hasmem"} = 1;

		# Here, we should detect the characters type
		# whether it is a integer or string (now, we only take care
		# of the two types, because I did not find any other type).
		if ($self->{"list_type"} eq "") {
			if ($data->{Data} =~ /^\"/) {
				$self->{"list_type"} = "char *";
				print "const char *".$self->{"list_name"}."Array[] = \{ $data->{Data}";
			} else { 
				if ($data->{Data} =~ /^[0-9]+/) {
					$self->{"list_type"} = "int *";
					print "int ".$self->{"list_name"}."Array[] = \{ $data->{Data}";
				} else {
					die "Some data in the <member> we can't process: \"$data->{Data}\"";
				}
			}
		} else {
			# So, we must have known the type, just output the member
			print "$data->{Data}";
		}
	}
}

sub generate_main {
	my $self = shift;
	# Firstly, push a new "b" to the string_unref list
	push(@{$self->{"string_unref"}}, "b");

	print << "__EOF__"

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>

#include <dom/dom.h>
#include <dom/functypes.h>

#include <domts.h>

dom_implementation *doc_impl;

int main(int argc, char **argv)
{
	dom_exception exp;

	(void)argc;
	(void)argv;

	if (chdir("$self->{chdir}") < 0) {
		perror("chdir (\\"$self->{chdir})\\"");
		return 1;
	}
__EOF__
}

# Note that, we have not just declare variables here
# we should also define EventListener here!
# I think this should be done after the EventListener design
# is complete
sub generate_var {
	my ($self, $ats) = @_;

	my $type = "";
	my $dstring = "";

	# For the case like <var name="v" type="DOMString" value="some some"
	if ($ats->{"type"} eq "DOMString" and exists $ats->{"value"}) {
		$dstring = $self->generate_domstring($ats->{"value"});
		$ats->{"value"} = $dstring;
	}

	$type = type_to_ctype($ats->{"type"});
	if ($type eq "") {
		print "Not implement this type now\n";
		return;
	}

	print "\t$type$ats->{'name'}";
	if (exists $ats->{"value"}) {
		print " = $ats->{'value'};\n";
	} else {
		if ($type =~ m/\*/) {
			print " = NULL;\n";
		} else {
			print ";\n";
		}
	}

	my $var = $self->{"var"};
	$var->{$ats->{"name"}} = $ats->{"type"};

	# If the type is List/Collection, we should take care of it
	if ($ats->{"type"} =~ /^(List|Collection)$/) {
		$self->{"list_name"} = $ats->{"name"};
	}
}

sub generate_list {
	my $self = shift;

	# We should deal with the end of <var> when the <var> is declaring a List/Collection
	if ($self->{"list_hasmem"} eq 1) {
		# Yes, we are in List/Collection declaration
		# Firstly, enclose the Array declaration
		print "};\n";

		# Now, we should create the list * for the List/Collection
		# Note, we should deal with "int" or "string" type with different params.
		if ($self->{"list_type"} eq "char *") {
			print $self->{"list_name"}." = list_new(STRING);\n";
		}
		if ($self->{"list_type"} eq "int *") {
			print $self->{"list_name"}." = list_new(INT);\n";
		}
		if ($self->{"list_type"} eq "") {
			die "A List/Collection has children member but no type is impossible!";
		}
		for (my $i = 0; $i < $self->{"list_num"}; $i++) {
			# Use *(char **) to convert char *[] to char *
			print "list_add(".$self->{"list_name"}.", *(char **)(".$self->{"list_name"}."Array + $i));\n";
		}
	} else {
		if ($self->{"list_name"} ne "") {
			#TODO: generally, we set the list type as dom_string, but it may be dom_node
			print $self->{"list_name"}." = list_new(DOM_STRING);\n";
			$self->{"list_type"} = "DOMString";
		}
	}

	# Add the List/Collection to map
	$self->{"list_map"}->{$self->{"list_name"}} = $self->{"list_type"};

	# Reset the List/Collection member state
	$self->{"list_hasmem"} = 0;
	$self->{"list_name"} = "";
	$self->{"list_type"} = "";
	$self->{"list_num"} = 0;
}

sub generate_load {
	my ($self, $a) = @_;
	my %ats = %$a;
	my $doc = $ats{"var"};

	$test_index ++;
	# define the test file path, use HTML if there is, otherwise using XML
	# Attention: I intend to copy the test files to the program excuting dir
	print "\tconst char *test$test_index = \"$ats{'href'}.html\";\n\n";
	print "\t$doc = load_html(test$test_index, $ats{'willBeModified'});";
	print "\tif ($doc == NULL) {\n";
	$test_index ++;
	print "\t\tconst char *test$test_index = \"$ats{'href'}.xml\";\n\n";
	print "\t\t$doc = load_xml(test$test_index, $ats{'willBeModified'});\n";
	print "\t\tif ($doc == NULL)\n";
	print "\t\t\treturn 1;\n";
	print "\t\t}\n";
	print << "__EOF__";
	exp = dom_document_get_implementation($doc, &doc_impl);
	if (exp != DOM_NO_ERR)
		return exp;
__EOF__

	$self->addto_cleanup($doc);
}

sub generate_framework_statement {
	my ($self, $name, $ats) = @_;

	switch($name) {
		case "load" {
			$self->generate_load($ats);
		}

		case "assign" {
			my $var = $ats->{"var"};
			my $value = "0";
			if (exists $ats->{"value"}) {	
				$value = $ats->{"value"};
			}

			# Assign with strong-type-conversion, this is necessary in C. 
			# And we may need to do deep-copy in the future. FIXME
			my $type = type_to_ctype($self->{"var"}->{$var});
			print "$var = \($type\) $value;\n";
		}

		case "increment" {
			my $var = $ats->{"var"};
			my $value = $ats->{"value"};

			print "$var += $value;\n";
		}

		case "decrement" {
			my $var = $ats->{"var"};
			my $value = $ats->{"value"};

			print "$var -= $value;\n";
		}
		
		case "append" {
			my $col = $ats->{"collection"};
			my $obj = "";

			# God, the DTD said, there should be a "OBJ" attribute, but there may not!
			if (exists $ats->{"obj"}) {
				$obj = $ats->{"obj"};
			} else {
				$obj = $ats->{"item"}
			}

			if (not $self->{"var"}->{$col} =~ /^(List|Collection)/) {
				die "Append data to some non-list type!";
			}

			print "list_add($col, $obj);\n";
		}
		
		case [qw(plus subtract mult divide)] {
			my $var = $ats->{"var"};
			my $op1 = $ats->{"op1"};
			my $op2 = $ats->{"op2"};

			my %table = ("plus", "+", "subtract", "-", "mult", "*", "divide", "/");
			print "$var = $op1 $table{$name} $op2;\n";
		}

		case "comment" {
			print "\*";
			$self->{"inline_comment"} = 1;
		}

		case "implementation" {
			if (not exists $ats->{"obj"}) {
				my $var = $ats->{"var"};
				my $dstring = generate_domstring($self, $dom_feature);
				print "exp = dom_implregistry_get_dom_implementation($dstring, \&$var);\n";
				print "\tif (exp != DOM_NO_ERR) {\n";
				$self->cleanup_fail("\t\t");
				print "\t\treturn exp;\n\t}\n";
				last;
			}

			my $obj = $ats->{"obj"};
			my $var = $ats->{"var"};
			# Here we directly output the libDOM's get_implementation API
			print "\texp = dom_document_get_implementation($obj, \&$var);\n";
			print "\tif (exp != DOM_NO_ERR) {\n";
			$self->cleanup_fail("\t\t");
			print "\t\treturn exp;\n\t}\n";
		}

		# We deal with hasFeaturn and implementationAttribute in the generate_condition
		case "hasFeature" {
			die "No, never can be here!";
		}
		case "implementaionAttribute" {
			die "No, never can be here!";
		}
		
		# Here, we die because we did not implement other statements
		# We did not implement these statements, because there are no use of them in the W3C DOMTS now
		case [@framework_statement] {
			die "The statement \"$name\" is not implemented yet!";
		}

	}
}

sub complete_framework_statement {
	my ($self, $name) = @_;

	switch($name) {
		case "comment" {
			print "*/\n";
			$self->{"inline_comment"} = 0;
		}
	}
}

sub generate_interface {
	my ($self, $en, $a) = @_;
	my %ats = %$a;
	my $dd = $self->{dd};

	if (exists $ats{'interface'}) {
		# Firstly, test whether it is a DOM native interface
		if (exists $native_interface{$ats{'interface'}}) {
			if ($native_interface{$ats{'interface'}} eq "") {
				die "Unkown how to deal with $en of $ats{'interface'}";
			}

			return $native_interface{$ats{'interface'}}($self, $en, $a);
		}

		my $ns = $dd->find("/library/interface[\@name=\"$ats{'interface'}\"]/method[\@name=\"$en\"]");
		if ($ns->size() != 0) {
			my $node = $ns->get_node(1);
			$self->generate_method($en, $node, %ats);
		} else {
			my $ns = $dd->find("/library/interface[\@name=\"$ats{'interface'}\"]/attribute[\@name=\"$en\"]");
			if ($ns->size() != 0) {
				my $node = $ns->get_node(1);
				$self->generate_attribute_accessor($en, $node, %ats);
			}
		}
	} else {
		my $ns = $dd->find("/library/interface/method[\@name=\"$en\"]");
		if ($ns->size() != 0) {
			my $node = $ns->get_node(1);
			$self->generate_method($en, $node, %ats);
		} else {
			my $ns = $dd->find("/library/interface/attribute[\@name=\"$en\"]");
			if ($ns->size() != 0) {
				my $node = $ns->get_node(1);
				$self->generate_attribute_accessor($en, $node, %ats);
			} else {
				die "Oh, Can't find how to deal with the element $en\n";
			}
		}
	}
}

sub generate_method {
	my ($self, $en, $node, %ats) = @_;
	my $dd = $self->{dd};
	if (! exists $ats{'interface'}) {
		my $n = $node;
		while($n->getLocalName() ne "interface") {
			$n = $n->getParentNode();
		}
		$ats{'interface'} = $n->getAttribute("name");
	}

	$method = to_cmethod($ats{'interface'}, $en);
        my $cast = to_attribute_cast($ats{'interface'});
	my $ns = $dd->find("parameters/param", $node);
	my $params = "${cast}$ats{'obj'}";
	for ($count = 1; $count <= $ns->size; $count++) {
		my $n = $ns->get_node($count);
		my $p = $n->getAttribute("name");
		my $t = $n->getAttribute("type");

		# Change the raw string and the char * to dom_string
		if ($t eq "DOMString") {
			if ($ats{$p} =~ /^"/ or $self->{"var"}->{$ats{$p}} eq "char *") {
				$self->generate_domstring($ats{$p});
				$params = $params.", dstring$string_index";
				next;
			}
		}

		# For the case that the testcase did not provide the param, we just pass a NULL
		# Because we are in C, not like C++ which can overriden functions
		if (not exists $ats{$p}) {
			$params = $params.", NULL";
			next;
		}

		$params = $params.", $ats{$p}";
	}

	#$ns = $dd->find("returns", $node);
	#my $n = $ns->get_node(1);
	#my $t = $n->getAttribute("type");
	# declare the return value
	#my $tp = type_to_ctype($t);
	#print "\t$tp ret$ret_index;\n";
	my $unref = 0;
	my $temp_node = 0;
	if (exists $ats{'var'}) {
		# Add the bootstrap params
		if (exists $bootstrap_api{$method}) {
			if ($method eq "dom_implementation_create_document") {
				$params = $params.", myrealloc, NULL, NULL";
			} else {
				$params = $params.", myrealloc, NULL";
			}
		}
		# Deal with the situation like
		# 
		# dom_node_append_child(node, new_node, &node);
		# 
		# Here, we should import a tempNode, and change this expression to
		#
		# dom_node *tnode1 = NULL;
		# dom_node_append_child(node, new_node, &tnode1);
		# dom_node_unref(node);
		# node = tnode1;
		#
		# Over.
		if ($ats{'obj'} eq $ats{'var'}) {
			my $t = type_to_ctype($self->{'var'}->{$ats{'var'}});
			$tnode_index ++;
			print "$t tnode$tnode_index = NULL;";
			$params = $params.", \&tnode$tnode_index";
			# The ats{'obj'} must have been added to cleanup stack 
			$unref = 1;
			# Indicate that we have created a temp node
			$temp_node = 1;
		} else {
			$params = $params.", (void *) \&$ats{'var'}";
			$unref = $self->param_unref($ats{'var'});
		}
	}

	print "\texp = $method($params);\n";

	if ($self->{'exception'} eq 0) {
		print << "__EOF__";
	if (exp != DOM_NO_ERR) {
	fprintf(stderr, "Exception raised from %s\\n", "$method");
__EOF__

		$self->cleanup_fail("\t\t");
		print << "__EOF__";
		return exp;
	}
__EOF__
	}

	if (exists $ats{'var'} and $unref eq 0) {
		$self->addto_cleanup($ats{'var'});
	}

	if ($temp_node eq 1) {
		my $t = $self->{'var'}->{$ats{'var'}};
		if (not exists $no_unref{$t}) {
			my $prefix = "dom_node";
			if (exists $unref_prefix{$t}) {
				$prefix = $unref_prefix{$t};
			}
			print $prefix."_unref(".$ats{'obj'}.");\n";
		}
		print "$ats{'var'} = tnode$tnode_index;";
	}
}

sub generate_attribute_accessor {
	my ($self, $en, $node, %ats) = @_;

	if (defined($ats{'var'})) {
		generate_attribute_fetcher(@_);
	} else {
		if (defined($ats{'value'})) {
			generate_attribute_setter(@_);
		}
	}
}

sub generate_attribute_fetcher {
	my ($self, $en, $node, %ats) = @_;
	my $dd = $self->{dd};
	if (! exists $ats{'interface'}) {
		my $n = $node;
		while($n->getLocalName() ne "interface") {
			$n = $n->getParentNode();
		}
		$ats{'interface'} = $n->getAttribute("name");
	}

	my $fetcher = to_attribute_fetcher($ats{'interface'}, "$en");
        my $cast = to_attribute_cast($ats{'interface'});
	my $unref = 0;
	my $temp_node = 0;
	# Deal with the situation like
	# 
	# dom_node_get_next_sibling(child, &child);
	# 
	# Here, we should import a tempNode, and change this expression to
	#
	# dom_node *tnode1 = NULL;
	# dom_node_get_next_sibling(child, &tnode1);
	# dom_node_unref(child);
	# child = tnode1;
	#
	# Over.
	if ($ats{'obj'} eq $ats{'var'}) {
		my $t = type_to_ctype($self->{'var'}->{$ats{'var'}});
		$tnode_index ++;
		print "\t$t tnode$tnode_index = NULL;\n";
		print "\texp = $fetcher(${cast}$ats{'obj'}, \&tnode$tnode_index);\n";
		# The ats{'obj'} must have been added to cleanup stack 
		$unref = 1;
		# Indicate that we have created a temp node
		$temp_node = 1;
	} else {
		$unref = $self->param_unref($ats{'var'});
		print "\texp = $fetcher(${cast}$ats{'obj'}, \&$ats{'var'});\n";
	}


	if ($self->{'exception'} eq 0) {
		print << "__EOF__";
	if (exp != DOM_NO_ERR) {
		fprintf(stderr, "Exception raised when fetch attribute %s", "$en");
__EOF__
		$self->cleanup_fail("\t\t");
		print << "__EOF__";
		return exp;
	}
__EOF__
	}

	if ($temp_node eq 1) {
		my $t = $self->{'var'}->{$ats{'var'}};
		if (not exists $no_unref{$t}) {
			my $prefix = "dom_node";
			if (exists $unref_prefix{$t}) {
				$prefix = $unref_prefix{$t};
			}
			print $prefix."_unref(".$ats{'obj'}.");\n";
		}
		print "$ats{'var'} = tnode$tnode_index;";
	}

	if ($unref eq 0) {
		$self->addto_cleanup($ats{'var'});
	}
}

sub generate_attribute_setter {
	my ($self, $en, $node, %ats) = @_;
	my $dd = $self->{dd};
	if (! exists $ats{'interface'}) {
		my $n = $node;
		while($n->getLocalName() ne "interface") {
			$n = $n->getParentNode();
		}
		$ats{'interface'} = $n->getAttribute("name");
	}

	my $setter = to_attribute_setter($ats{'interface'}, "$en");
	my $param = "$ats{'obj'}";

	# For DOMString, we should deal specially
	my $lp = $ats{'value'};
	if ($node->getAttribute("type") eq "DOMString") {
		if ($ats{'value'} =~ /^"/ or $self->{"var"}->{$ats{'value'}} eq "char *") {
			$lp = $self->generate_domstring($ats{'value'});
		}
	}

	$param = $param.", $lp";

	print "exp = $setter($param);";

	if ($self->{'exception'} eq 0) {
		print << "__EOF__";
		if (exp != DOM_NO_ERR) {
			fprintf(stderr, "Exception raised when fetch attribute %s", "$en");
__EOF__
		$self->cleanup_fail("\t\t");
		print << "__EOF__";
			return exp;
		}
__EOF__
	}

}


sub generate_condition {
	my ($self, $name, $ats) = @_;

	# If we are in nested or/and/xor/not, we should put a operator before test
	my @array = @{$self->{condition_stack}};
	if ($#array ge 0) {
		switch ($array[-1]) {
			case "xor" {
				print " ^ ";
			}
			case "or" {
				print " || ";
			}
			case "and" {
				print " && ";
			}
			# It is the indicator, just pop it.
			case "new" {
				pop(@{$self->{condition_stack}});
			}
		}
	}

	switch ($name) {
		case [qw(less lessOrEquals greater greaterOrEquals)] {
			my $actual = $ats->{actual};
			my $expected = $ats->{expected};
			my $method = $name;
			$method =~ s/[A-Z]/_$&/g;
			$method = lc $method;
			print "$method($expected, $actual)";
		}

		case "same" {
			my $actual = $ats->{actual};
			my $expected = $ats->{expected};
			my $func = $self->find_override("is_same", $actual, $expected);
			print "$func($expected, $actual)";
		}

		case [qw(equals notEquals)]{
			my $actual = $ats->{actual};
			my $expected = $ats->{expected};
			my $ig;
			if (exists $ats->{ignoreCase}){
				$ig = $ats->{ignoreCase};
			} else {
				$ig = "false";
			}
			$ig = adjust_ignore($ig);

			my $func = $self->find_override("is_equals", $actual, $expected);
			if ($name =~ /not/i){
				print "(false == $func($expected, $actual, $ig))";
			} else {
				print "$func($expected, $actual, $ig)";
			}
		}

		case [qw(isNull notNull)]{
			my $obj = $ats->{obj};
			if ($name =~ /not/i) {
				print "(false == is_null($obj))";
			} else {
				print "is_null($obj)";
			}
		}

		case "isTrue" {
			my $value = $ats->{value};
			print "is_true($value)";
		}

		case "isFalse" {
			my $value = $ats->{value};
			print "(false == is_true($value))";
		}

		case "hasSize" {
			my $obj = $ats->{obj};
			my $size = $ats->{expected};
			my $func = $self->find_override("is_size", $obj, $size);
			print "$func($size, $obj)";
		}

		case "contentType" {
			my $type = $ats->{type};
			print "is_contenttype(\"$type\")";
		}

		case "instanceOf" {
			my $obj = $ats->{obj};
			my $type = $ats->{type};
			print "instanceOf(\"$type\", $obj)";
		}

		case "hasFeature" {
			if (exists $ats->{var}) {
				$self->generate_interface($name, $ats);
			} else {
				my $feature = $ats->{feature};
				if (not ($feature =~ /^"/)) {
					$feature = '"'.$feature.'"';
				}
				my $version = "NULL";
				if (exists $ats->{version}) {
					$version = $ats->{version};
					if (not ($version =~ /^"/)) {
						$version = '"'.$version.'"';
					}
					
				}

				if ($self->{context}->[-2] ne "condition") {
					# we are not in a %condition place, so we must be a statement
					# we change this to assert
					# print "assert(has_feature($feature, $version));\n"
					# do nothing if we are not in condition.
				} else {
					print "has_feature($feature, $version)";
				}
			}
		}

		case "implementationAttribute" {
			my $value = $ats->{value};
			my $name = $ats->{name};
			
			if ($self->{context}->[-2] ne "condition") {
				# print "assert(implementation_attribute(\"$name\", $value));";
				# Do nothing, and the same with hasFeature, this means we will
				# run all test cases now and try to get a result whether we support
				# such feature.
			} else {
				print "implementation_attribute(\"$name\", $value)";
			}
		}

		case [qw(and or xor)] {
			push(@{$self->{condition_stack}}, $name);
			push(@{$self->{condition_stack}}, "new");
			print "(";
		}

		case "not" {
			push(@{$self->{condition_stack}}, $name);
			print "(false == ";
		}
	}

}

sub complete_condition {
	my ($self, $name) = @_;

	if ($name =~ /^(xor|or|and)$/i) {
		print ")";
		my $top = pop(@{$self->{condition_stack}});
		die "Condition stack error! $top != $name" if $top ne $name;
	}

	if ($name eq "not") {
		my $top = pop(@{$self->{condition_stack}});
				die "Condition stack error! $top != $name" if $top ne $name;
		print ")";
	}

	# we deal with the situation that the %condition is in a control statement such as
	# <if> or <while>, and we should start a new '{' block here
	if ($self->{context}->[-1] eq "condition") {
		print ") {\n";
		pop(@{$self->{context}});
	}
}

sub generate_assertion {
	my ($self, $name, $ats) = @_;

	print "\tassert(";
	switch($name){
		# Only assertTrue & assertFalse can have nested %conditions
		case [qw(assertTrue assertFalse assertNull)] {
			my $n = $name;
			$n =~ s/assert/is/g;
			if (exists $ats->{actual}){
				my $ta = { value => $ats->{actual}, obj => $ats->{actual}};
				$self->generate_condition($n,$ta);
			}
		}

		case [qw(assertNotNull assertEquals assertNotEquals assertSame)] {
			my $n = $name;
			$n =~ s/assert//g;
			$n = lcfirst $n;
			if (exists $ats->{actual}){
				my $ta = { 	
						actual => $ats->{actual},
						value => $ats->{actual}, 
						obj => $ats->{actual},
						expected => $ats->{expected},
						ignoreCase => $ats->{ignoreCase},
						type => $ats->{type},
					 };
				$self->generate_condition($n,$ta);
			}
		}

		case "assertInstanceOf" {
			my $obj = $ats->{obj};
			my $type = $ats->{type};
			print "is_instanceof(\"$type\", $obj)";
		}

		case "assertSize" {
			my $n = $name;
			$n =~ s/assert/has/;
			if (exists $ats->{collection}){
				my $ta = { obj => $ats->{collection}, expected => $ats->{size}};
				$self->generate_condition($n,$ta);
			}
		}
	
		case "assertEventCount" {
			#todo
		}
		
		case "assertURIEquals" {
			my $actual = $ats->{actual};
			my ($scheme, $path, $host, $file, $name, $query, $fragment, $isAbsolute) = qw(NULL NULL NULL NULL NULL NULL NULL NULL);
			if (exists $ats->{scheme}) {
				$scheme = $ats->{scheme};
			}
			if (exists $ats->{path}) {
				$path = $ats->{path};
			}
			if (exists $ats->{host}) {
				$host = $ats->{host};
			}
			if (exists $ats->{file}) {
				$file = $ats->{file};
			}
			if (exists $ats->{name}) {
				$name = $ats->{name};
			}
			if (exists $ats->{query}) {
				$query = $ats->{query};
			}
			if (exists $ats->{fragment}) {
				$fragment = $ats->{fragment};
			}
			if (exists $ats->{isAbsolute}) {
				$isAbsolute = $ats->{isAbsolute};
			}

			print "is_uri_equals($scheme, $path, $host, $file, $name, $query, $fragment, $isAbsolute, $actual)"
		}
	}

}

sub complete_assertion {
	my ($self, $name) = @_;

	print ");\n";
}

sub generate_control_statement {
	my ($self, $name, $ats) = @_;

	switch($name) {
		case "if" {
			print "\tif(";
			push(@{$self->{"context"}}, "condition");
		}

		case "else" {
			$self->cleanup_block_domstring();
			print "\t} else {";
		}

		case "while" {
			print "\twhile (";
			push(@{$self->{"context"}}, "condition");
		}

		case "for-each" {
			# Detect what is the collection type, if it is "string", we
			# should also do some conversion work
			my $coll = $ats->{"collection"};
			# The default member type is dom_node
			my $type = "dom_node *";
			if (exists $self->{"list_map"}->{$coll}) {
				$type = $self->{"list_map"}->{$coll};
			}

			# Find the member variable, if it is not declared before, declare it firstly
			my $member = $ats->{"member"};
			if (not exists $self->{"var"}->{$member}) {
				print "$type  $member;\n";
				# Add the new variable to the {var} map
				$self->{"var"}->{"$member"} = $type;
			}

			# Now the member is conformed to be declared
			if ($self->{"var"}->{$coll} =~ /^(List|Collection)$/) {
				# The element in the list is not equal with the member object
				# For now, there is only one case for this, it is "char *" <=> "DOMString"
				my $conversion = 0;
				if ($self->{"var"}->{"$member"} ne $type) {
					if ($self->{"var"}->{"$member"} eq "DOMString") {
						if ($type eq "char *") {
							$conversion = 1;
						}
					}
				}

				$iterator_index++;
				print "unsigned int iterator$iterator_index = 0;";
				if ($conversion eq 1) {
					print "char *tstring$temp_index = NULL;";
				}
				print "foreach_initialise_list($coll, \&iterator$iterator_index);\n";
				print "while(get_next_list($coll, \&iterator$iterator_index, ";
				if ($conversion eq 1) {
					print "\&tstring$temp_index)) {\n";
					print "exp = dom_string_create((const uint8_t *)tstring$temp_index,";
					print "strlen(tstring$temp_index), &$member);\n";
					print "if (exp != DOM_NO_ERR) {\n";
					print "\t\tfprintf(stderr, \"Can't create DOMString\\n\");";
					$self->cleanup_fail("\t\t");
					print "\t\treturn exp;\n\t}\n";
					$temp_index ++;
				} else {
					print "\&$member)) {\n";
				}
			}

			if ($self->{"var"}->{$coll} eq "NodeList") {
				$iterator_index++;
				print "unsigned int iterator$iterator_index = 0;";
				print "foreach_initialise_domnodelist($coll, \&iterator$iterator_index);\n";
				print "while(get_next_domnodelist($coll, \&iterator$iterator_index, \&$member)) {\n";
			}

			if ($self->{"var"}->{$coll} eq "NamedNodeMap") {
				$iterator_index++;
				print "unsigned int iterator$iterator_index = 0;";
				print "foreach_initialise_domnamednodemap($coll, \&iterator$iterator_index);\n";
				print "while(get_next_domnamednodemap($coll, \&iterator$iterator_index, \&$member)) {\n";
			}

			if ($self->{"var"}->{$coll} eq "HTMLCollection") {
				$iterator_index++;
				print "unsigned int iterator$iterator_index = 0;";
				print "foreach_initialise_domhtmlcollection($coll, \&iterator$iterator_index);\n";
				print "while(get_next_domhtmlcollection($coll, \&iterator$iterator_index, \&$member)) {\n";
			}
		}
	}

	# Firstly, we enter a new block, so push a "b" into the string_unref list
	push(@{$self->{"string_unref"}}, "b");
}

sub complete_control_statement {
	my ($self, $name) = @_;

	# Note: we only print a '}' when <if> element ended but not <else> 
	# The reason is that there may be no <else> element in <if> and 
	# we when there is an <else> element, it must nested in <if>. ^_^
	switch($name) {
		case [qw(if while for-each)] {
			# Firstly, we should cleanup the dom_string in this block
			$self->cleanup_block_domstring();

			print "}\n";
		}
	}
}


###############################################################################
#
# The helper functions
#
sub generate_domstring {
	my ($self, $str) = @_;
	$string_index = $string_index + 1;

	print << "__EOF__";
	const char *string$string_index = $str;
	dom_string *dstring$string_index;
	exp = dom_string_create((const uint8_t *)string$string_index,
			strlen(string$string_index), &dstring$string_index);
	if (exp != DOM_NO_ERR) {
		fprintf(stderr, "Can't create DOMString\\n");
__EOF__
	$self->cleanup_fail("\t\t");
	print << "__EOF__";
		return exp;
	}

__EOF__

	push(@{$self->{string_unref}}, "$string_index");

	return "dstring$string_index";
}

sub cleanup_domstring {
	my ($self, $indent) = @_;

	for (my $i = 0; $i <= $#{$self->{string_unref}}; $i++) {
		if ($self->{string_unref}->[$i] ne "b") {
			print $indent."dom_string_unref(dstring$self->{string_unref}->[$i]);\n";
		}
	}
}

sub cleanup_block_domstring {
	my $self = shift;

	while ((my $num = pop(@{$self->{string_unref}})) ne "b" and $#{$self->{string_unref}} ne -1) {
		print "dom_string_unref(dstring$num);\n";
	}
}

sub type_to_ctype {
	my $type = shift;

	if (exists $special_type{$type}) {
		return $special_type{$type};
	}

	# If the type is not specially treated, we can transform it by rules
	if ($type =~ m/^HTML/) {
		# Don't deal with this now
		return "";
	}

	# The core module comes here
	$type =~ s/[A-Z]/_$&/g;
	$type = lc $type;

	# For events module
	$type =~ s/_u_i_/_ui_/g;

	return "dom".$type." *";
}

sub to_cmethod {
	my ($type, $m) = @_;
	my $prefix = get_prefix($type);
	my $ret;

	if (exists $special_method{$m}) {
		$ret = $prefix."_".$special_method{$m};
	} else {
		$m =~ s/[A-Z]/_$&/g;
		$m = lc $m;
		$ret = $prefix."_".$m;
	}

	$ret =~ s/h_t_m_l/html/;
	$ret =~ s/c_d_a_t_a/cdata/;
	$ret =~ s/_n_s$/_ns/;
	# For DOMUIEvent
	$ret =~ s/_u_i_/_ui_/;
	# For initEvent
	$ret =~ s/init_event/init/;
	return $ret;
}

sub to_attribute_fetcher {
	return to_attribute_accessor(@_, "get");
}

sub to_attribute_setter {
	return to_attribute_accessor(@_, "set");
}

sub to_attribute_accessor {
	my ($type, $af, $accessor) = @_;
	my $prefix = get_prefix($type);
	my $ret;

	if (exists $special_attribute{$af}) {
		$ret = $prefix."_".$accessor."_".$special_attribute{$af};
	} else {
		$af =~ s/[A-Z]/_$&/g;
		$af = lc $af;
		$ret = $prefix."_".$accessor."_".$af;
	}

	$ret =~ s/h_t_m_l/html/;
	return $ret;
}

sub to_attribute_cast {
	my $type = shift;
        my $ret = get_prefix($type);
        $ret =~ s/h_t_m_l/html/;
        return "(${ret} *)";
}

sub get_prefix {
	my $type = shift;

	if (exists $special_prefix{$type}) {
		$prefix = $special_prefix{$type};
	} else {
		$type =~ s/[A-Z]/_$&/g;
		$prefix = lc $type;
		$prefix = "dom".$prefix;
	}
	return $prefix;
}

# This function remain unsed
sub get_suffix {
	my $type = shift;
	my $suffix = "default";

	if (exists $override_suffix{$type}) {
		$suffix = $override_suffix{$type};
	} else {
		$type =~ s/[A-Z]/_$&/g;
		$suffix = lc $type;
		$suffix = "dom".$suffix;
	}
	return $suffix;
}

#asserttions sometimes can contain sub-statements according the DTD. Like
#<assertEquals ..>
# <stat1 />
# <stat2 />
#</assertEquals>
#
# And assertion can contains assertions too! So, I use the assertion_stack
# to deal:
#
# when we encounter an assertion, we push $assertionName, "end", "start" to 
# the stack, and when we encounter a statement, we examine the stack to see 
# the top element, if it is:
#
# 1. "start", then we are in sub-statement of that assertion, and this is the
#	the first sub-statement, so we should print a if (condtion==true) {, before
#	print the real statement.
# 2. "end", then we are in sub-statement of that assertion, and we are not the 
#	first one, just print the statement.
#
# But after searching the whole testcases, I found no use of sub-statements of assertions.
# So, this function left unsed!

sub end_half_assertion {
	my ($self, $name) = @_;

	my $top = pop(@{$self->{assertion_stack}});
	if ($top eq "end") {
		print "$self->{indent}"."}\n";
	} else {
		if ($top eq "start") {
			pop(@{$self->{assertion_stack}});
			pop(@{$self->{assertion_stack}});
		}
	}

	pop(@{$self->{assertion_stack}});
}
### Enclose an unsed function
##############################################################################################


sub cleanup_domvar {
	my ($self, $indent) = @_;

	my $str = join($indent, reverse @{$self->{unref}});
	print $indent.$str."\n";
}

sub cleanup_fail {
	my ($self, $indent) = @_;

	$self->cleanup_domstring($indent);
	$self->cleanup_domvar($indent);
}

sub cleanup {
	my $self = shift;

	print "\n\n";
	$self->cleanup_domstring("\t");
	$self->cleanup_domvar("\t");
        print "\n\tprintf(\"PASS\");\n";
	print "\n\treturn 0;\n";
	print "\n\}\n";
}

sub addto_cleanup {
	my ($self, $var) = @_;

	my $type = $self->{'var'}->{$var};
	if (not exists $no_unref{$type}) {
		my $prefix = "dom_node";
		if (exists $unref_prefix{$type}) {
			$prefix = $unref_prefix{$type};
		}
		push(@{$self->{unref}}, $prefix."_unref(".$var.");\n");
	}
}

sub adjust_ignore {
	my $ig = shift;

	if ($ig eq "auto"){
		return "true";
	}
	return $ig;
}

sub find_override {
	my ($self, $func, $var, $expected) = @_;
	my $vn = $self->{var}->{$var};

	# Deal with string types
	if ($expected eq "DOMString") {
		return $func."_domstring";
	} else {
		if ($expected =~ /^\"/ or $self->{"var"}->{$expected} eq "char *") {
			return $func."_string";
		}
	}

	if (exists $override_suffix{$vn}) {
		$func = $func."_".$override_suffix{$vn}
	}
	return $func;
}

sub param_unref {
	my ($self, $var) = @_;

	my $type = $self->{'var'}->{$var};
	if (not exists $no_unref{$type}) {
		my $prefix = "dom_node";
		if (exists $unref_prefix{$type}) {
			$prefix = $unref_prefix{$type};
		}
		print "\tif ($var != NULL) {\n";
		print "\t\t" . $prefix."_unref(".$var.");\n";
		print "\t\t$var = NULL;\n";
		print "\t}\n";
	}

	foreach my $item (@{$self->{unref}}) {
		$item =~ m/.*\((.*)\).*/;
		if ($var eq $1) {
			return 1;
		}
	}

	foreach my $item (@{$self->{string_unref}}) {
		if ($var eq $item) {
			return 1;
		}
	}

	return 0;
}

sub generate_domstring_interface {
	my ($self, $en, $a) = @_;

	switch ($en) {
		case "length" {
			print "$a->{'var'} = dom_string_length($a->{'obj'});";
		}

		else {
			die "Can't generate method/attribute $en for DOMString";
		}
	}
}

1;
