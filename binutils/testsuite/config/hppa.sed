s/# Old OSF sed blows up if you have a sed command starting with "#"//
s/# Avoid it by putting the comments within real sed commands.//
s/# Fix the definition of common_symbol to be correct for the PA assebmlers.//
s/	\.comm common_symbol,4/common_symbol	.comm 4/
