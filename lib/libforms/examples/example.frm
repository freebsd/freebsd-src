# An example form file for an adduser command
!Forms Version name

Display screen1 {
	Height 1000
	Width 1000
	Type Ncurses { 
		# libdialog compatible color pairs
		ColorPairs {
			01	Cyan	Blue
			02	Black	Black
			03	Black	White
			04	Yellow	White
			05	White	White
			06	White	Blue
			07	Black	White
			08	White	Blue
			09	Red		White
			10	Yellow	Blue
			11	Black	White
			12	Black	White
			13	Black	White
			14	Black	White
			15	Yellow	White
			16	White	White
			17	Yellow	White
			18	Black	White
			19	White	White
			20	Black	White
			21	White	Blue
			22	Yellow	White
			23	Yellow	Blue
			24	Red		White
			25	Red		Blue
			26	Black	White
			27	White	White
			28	Green	White
			29	Green	White
		}
	}
	#
	# The AttrTable assosciates attribute strings with numeric id's.
	# It's up to the device dependant code to decide how to interprate an
	# attribute id. For ncurses the id is treated as a color pair number.
	# For other devices they'd likely be an index to some device specific
	# structure declared above.
	#
	AttrTable {
			screen					01
			shadow					02
			dialog					03
			title					04
			border					05
			button_active			06
			button_inactive			07
			button_key_active		08
			button_key_inactive		09
			button_label_active		10
			button_label_inactive	11
			inputbox				12
			inputbox_border			13
			searchbox				14
			searchbox_title			15
			searchbox_border		16
			position_indicator		17
			menubox					18
			menubox_border			19
			item					20
			item_selected			21
			tag						22
			tag_selected			23
			tag_key					24
			tag_key_selected		25
			check					26
			check_selected			27
			uarrow					28
			darrow					29
	}
}

template {
	Width 15
	Text "This is defined as a template and duplicated here"
}

Window adduser on screen1 at 0,0 {
	Attributes "\screen"

	window at 1,1 {
		Height 22
		Width 75
		Attributes "\dialog"
		Active username

		box {
			Attributes "\dialog"
			Highlight "\border"
			CallFunc draw_box
			shadow {
				Attributes "\shadow"
				CallFunc draw_shadow
			}
		}

		Title at 0,9 { Text " This is a title " }

		username at 5,20 {
			Height 1
			Width 30
			Attributes "\screen"
			Highlight "\tag_selected"

			Next shells

			Input "nobody"

			exp at 3,3 {
				Attributes "\dialog"
				Text "The is an input object:"
			}
			prompt at 5,3 {
				Text "Username: "
			}
		}

		shells at 9,20 {
			Attributes "\dialog"
			Highlight "\tag_selected"
			Next button
			Options {
				"sh"
				"csh"
				"tcsh"
				"bash"
			}

			exp at 7,3 {
				Attributes "\dialog"
				Text "This is a horizontal menu:"
			}
			prompt at 9,3 { Text "Select a shell: "}
		}

		button at 14,9 {
			Height 3
			Width 7
			Attributes "\tag_key_selected"
			Highlight "\tag_selected"
			Active button

			button_box at 14,9 {
				CallFunc draw_box
			}

			button at 15, 10 {
				Up username Down username
				Action User_Routine
				Label "QUIT"
			}
		}
	}
}
