#To: bug-gnu-utils@gnu.org
#cc: arnold@gnu.org
#Subject: Possible bug in GNU Awk 3.0.4
#Date: Wed, 24 Nov 1999 21:47:24 +0000
#From: Daniel Elphick <de397@ecs.soton.ac.uk>
#Message-Id: <E11qkG4-0000l0-00@cameron>
#
#This is a multipart MIME message.
#
#--==_Exmh_-11192982200
#Content-Type: text/plain; charset=us-ascii
#
#
#When I use the attached awk script unique on the attached data file, it 
#reports that all 4 lines of the data are the same. Using mawk it correctly 
#reports that there are no repeats.
#
#I don't know if there are limits on the size of associative array keys for the 
#purposes of reliable indexing but if there is then it is not (obviously) 
#documented.
#
#
#--==_Exmh_-11192982200
#Content-Type: text/plain ; name="data"; charset=us-ascii
#Content-Description: data
#Content-Disposition: attachment; filename="data"
#
#322322111111112232231111
#322322111111112213223111
#322322111111112211132231
#322322111111112211113223
#
#--==_Exmh_-11192982200
#Content-Type: text/plain ; name="unique"; charset=us-ascii
#Content-Description: unique
#Content-Disposition: attachment; filename="unique"
#
{
	if($0 in a)
	{
		printf("line %d has been seen before at line %d\n",  NR, a[$0])
		repeat_count += 1
	}
	else
	{
		a[$0] = NR
	}
	count += 1
}
END {
#	printf("%d %f%%\n", repeat_count, (float)repeat_count / count * 100)
	printf("%d %f%%\n", repeat_count, repeat_count / count * 100)
}
#
#--==_Exmh_-11192982200--
