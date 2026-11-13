# Same priority: the scalar joins the container in an implicit array
arr_int = 5;
obj_str = "bar";
arr_null = null;

# Containers still merge, including one opened by a nested key
obj_key nested {
	key = value1;
}
obj_brace {
	key1 = value2;
}

# and those after comment lines, which are skipped before the value
arr_comment =
# a comment line
[3];
obj_comment # a comment
# and another one after an empty line

# then the value
{
	key1 = value2;
}
obj_comment_ml =
/* a multiline
   comment */{
	key1 = value2;
}

# Lower priority than the existing container: the scalar is dropped
arr_lo = 5;
obj_lo = "bar";

# but a container is still merged, with whatever it holds
arr_comment_lo =
# a comment line
[[3], {key {key1 = value2;}}];
# A separator in a chain of nested keys does not stop the nesting
obj_nested_lo nested = key1 {
	key = value2;
}

# Higher priority than the existing container: the scalar replaces it
.priority 1
arr_hi = 5;
obj_hi = "bar";

# but a container is still merged
obj_comment_hi =
# a comment line
{
	key1 = value2;
}
