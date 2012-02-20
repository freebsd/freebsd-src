# Example bindkey.tcsh which binds some of the common xterm/linux/cygwin
# terminal keys.
bindkey -e				# Force EMACS key binding

bindkey "^[[2~"	  yank			# Insert key
bindkey "^[[3~"	  delete-char		# Delete key
bindkey "^[[H"	  beginning-of-line	# Home key
bindkey "^[[F"	  end-of-line		# End key
bindkey "^[[5~"	  up-history		# Page up key
bindkey "^[[6~"	  down-history		# Page down key

bindkey "^[[C"	  forward-char		# Cursor right
bindkey "^[[D"	  backward-char		# Cursor left
bindkey "^[[A"	  up-history		# Cursor up
bindkey "^[[B"	  down-history		# Cursor down
bindkey "^[^[[D"  backward-word		# Alt Cursor left
bindkey "^[^[[C"  forward-word		# Alt Cursor right

bindkey "^?"	  backward-delete-char  # However the BS key is defined...
bindkey "^H"	  backward-delete-char  # However the BS key is defined...
