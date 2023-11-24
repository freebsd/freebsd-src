@ nostorederror_nostoredlen @
 expression __src, __dst, __len;
 statement S1;
@@

 S1
-copystr(__src, __dst, __len, NULL);
+strlcpy(__dst, __src, __len);

@ ifcondition_nostoredlen @
 expression __src, __dst, __len;
 statement S1;
@@
 if (
(
-copystr(__src, __dst, __len, NULL) == ENAMETOOLONG
|
-copystr(__src, __dst, __len, NULL) != 0
|
-copystr(__src, __dst, __len, NULL)
)
+strlcpy(__dst, __src, __len) >= __len
 ) S1

@ nostorederror_storedlen1 @
 expression __src, __dst, __len;
 identifier __done;
 statement S1;
@@
 S1
(
-copystr(__src, __dst, __len, &__done);
+__done = strlcpy(__dst, __src, __len);
+__done = MIN(__done, __len);
|
-copystr(__src, __dst, __len, __done);
+ *__done = strlcpy(__dst, __src, __len);
+ *__done = MIN(*__done, __len);
)
