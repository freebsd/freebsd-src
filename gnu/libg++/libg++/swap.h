/* From Ron Guillmette; apparently needed for Hansen's code */

#define swap(a,b) ({ typeof(a) temp = (a); (a) = (b); (b) = temp; })
