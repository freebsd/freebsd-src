Colours example_colors {
	pair = red, yellow
	pair = blue, white
}

field1 { attributes = 0 text = "\standout This text is \bold bold and \blink flashy" }

field2 {
	height = 2
	width = 22
	text = "This is an input fieldwith a default"
}

field3 {
	width = 10
	default = "This is a default entry"
	limit = 30
}

field4 { text = "This is a labelled input field" }

field5 { label = "A temp. label" }

field6 { text = "Some options to choose from: " }

field7 { selected = 0 options = "Choose", "another", "of", "these" }

field8 { width = 6 action = "EXIT" function = exit_form }

field9 {
action = "CANCEL"
function = cancel_form
}

Form example at 0,0 {
	height = 25
	width = 80
	start = input1
	colortable = example
	attributes = 0

	Title  {attributes = 0 text = "A Simple Demo"} at  0,30

	field1 at  3,23
	field2 at  7, 2
	field4 at 11, 2
	field6 at 15, 2

	input1 {field3} at  7,45, next=input2
	input2 {field5} at 11,45, next=menu1
	menu1  {field7} at 15,45, next=quit
	quit   {field8} at 20,20, up = menu1, right = cancel
	cancel {field9} at 20,43, next=input1
}
