Field Title {
	attributes = A_BOLD
	text = "A Simple Demo"
}

Field field1 {
	attributes = A_BLINK|A_BOLD
	text = "This text is bold and flashy"
}

Field field2 {
	text = "This is an input field with a default"
}

Field field3 {
	width = 10
	default = "This is a default entry"
	limit = 30
}

Field field4 {
	text = "This is a labelled input field"
}

Field field5 {
	label = "A temp. label"
}

Field field6 {
	text = "Some options to choose from: "
}

Field field7 {
	selected = 0
	options = "Choose", "another", "of", "these"
}

Field field8 {
	width = 6
	attributes = A_BOLD|A_REVERSE
	action = "EXIT"
	function = exit_form
}

Field field9 {
	attributes = A_BOLD|A_REVERSE
	action = "CANCEL"
	function = cancel_form
}

Link input1 as field3 {
	next = input2
	down = input2
}

Link input2 as field5 {
	next = menu1
	up = input1
	down = menu1
}

Link menu1 as field7 {
	next = quit
	up = input2
	down = quit
}

Link quit as field8 {
	up = menu1
	right = cancel
}

Link cancel as field9 {
	up = input1
	down = input1
	left = quit
	right = input1
}

Form example at 0,0 {
	height = 24
	width = 80

	Field Title  at  0,30

	Field field1 at  3,23
	Field field2 at  7, 2
	Field field4 at 11, 2
	Field field6 at 15, 2

	Field input1 at  7,45
	Field input2 at 11,45
	Field menu1  at 15,45
	Field quit   at 20,20
	Field cancel at 20,43
}
