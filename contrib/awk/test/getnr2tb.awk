#From vp@dmat.uevora.pt Thu Jun 18 09:10 EDT 1998
#Received: from mescaline.gnu.org (we-refuse-to-spy-on-our-users@mescaline.gnu.org [158.121.106.21]) by cssun.mathcs.emory.edu (8.7.5/8.6.9-940818.01cssun) with ESMTP id JAA23649 for <arnold@mathcs.emory.edu>; Thu, 18 Jun 1998 09:10:54 -0400 (EDT)
#Received: from khromeleque.dmat.uevora.pt by mescaline.gnu.org (8.8.5/8.6.12GNU) with ESMTP id JAA21732 for <arnold@gnu.ai.mit.edu>; Thu, 18 Jun 1998 09:11:19 -0400
#Received: from khromeleque.dmat.uevora.pt (vp@localhost [127.0.0.1])
#	by khromeleque.dmat.uevora.pt (8.8.8/8.8.8/Debian/GNU) with ESMTP id OAA11817
#	for <arnold@gnu.ai.mit.edu>; Thu, 18 Jun 1998 14:13:57 +0100
#Message-Id: <199806181313.OAA11817@khromeleque.dmat.uevora.pt>
#To: arnold@gnu.org
#Subject: concatenation bug in gawk 3.0.3
#Date: Thu, 18 Jun 1998 14:13:57 +0200
#From: Vasco Pedro <vp@dmat.uevora.pt>
#Content-Type: text
#Content-Length: 2285
#Status: RO
#
#Hi,
#
#The gawk program '{print NR " " 10/NR}' will print:
#
#1 10
#5 5
#3 3.33333
#2 2.5
#2 2
#1 1.66667
#
#instead of the correct:
#
#1 10
#2 5
#3 3.33333
#4 2.5
#5 2
#6 1.66667
#
#You'll notice, on the incorrect output, that the first column is
#the first digit of the second.
#
#I think the problem comes from the way builtin variables are handled.
#Since the items to be concatenated are processed in reverse order and
#the return value of tree_eval(``NR'') is a pointer to the value part
#of `NR_node', the `unref()' of `NR_node' due to its second occurrence
#will leave a dangling pointer in `strlist'. The reason that it doesn't
#reuse the freed space with objects of the same type. (Using Electric
#Fence with EF_PROTECT_FREE set confirms that freed space is being
#accessed.)
#
#The enclosed patch (hack would be a better word to describe it) is
#all I could come up with. With it installed, things seem to work ok,
#but I doubt this is the correct way to do it. (If I treated the
#case for `Node_field_spec' as the I did others, `make check' would
#fail in several places.)
#
#Regards,
#vasco
#
#*** eval.c~	Tue May  6 21:39:55 1997
#--- eval.c	Thu Jun 18 13:39:25 1998
#***************
#*** 685,697 ****
#  		return func_call(tree->rnode, tree->lnode);
#  
#  		/* unary operations */
#  	case Node_NR:
#  	case Node_FNR:
#  	case Node_NF:
#  	case Node_FIELDWIDTHS:
#  	case Node_FS:
#  	case Node_RS:
#- 	case Node_field_spec:
#  	case Node_subscript:
#  	case Node_IGNORECASE:
#  	case Node_OFS:
#--- 685,700 ----
#  		return func_call(tree->rnode, tree->lnode);
#  
#  		/* unary operations */
#+ 	case Node_field_spec:
#+ 		lhs = get_lhs(tree, (Func_ptr *) NULL);
#+ 		return *lhs;
#+ 
#  	case Node_NR:
#  	case Node_FNR:
#  	case Node_NF:
#  	case Node_FIELDWIDTHS:
#  	case Node_FS:
#  	case Node_RS:
#  	case Node_subscript:
#  	case Node_IGNORECASE:
#  	case Node_OFS:
#***************
#*** 699,705 ****
#  	case Node_OFMT:
#  	case Node_CONVFMT:
#  		lhs = get_lhs(tree, (Func_ptr *) NULL);
#! 		return *lhs;
#  
#  	case Node_var_array:
#  		fatal("attempt to use array `%s' in a scalar context",
#--- 702,710 ----
#  	case Node_OFMT:
#  	case Node_CONVFMT:
#  		lhs = get_lhs(tree, (Func_ptr *) NULL);
#! 		r = dupnode(*lhs);
#! 		r->flags |= TEMP;
#! 		return r;
#  
#  	case Node_var_array:
#  		fatal("attempt to use array `%s' in a scalar context",
#
{ print NR " " 10/NR }
