BEGIN { pipe =      "cat <<EOF\n"
        pipe = pipe "select * from user\n"
        pipe = pipe "  where Name = 'O\\'Donell'\n"
        pipe = pipe "EOF\n"
        
        while ((pipe | getline) > 0)
                print
        
        exit 0
}       
