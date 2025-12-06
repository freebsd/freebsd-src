@ pfs_create_dir_ret_ident @
 expression _pfn, E1, E2, E3, E4, E5, E6;
@@
-_pfn = pfs_create_dir(E1, E2, E3, E4, E5, E6);
+pfs_create_dir(E1, &_pfn, E2, E3, E4, E5, E6);

@ pfs_create_file_ret @
 expression _pfn, E1, E2, E3, E4, E5, E6, E7;
@@
-_pfn = pfs_create_file(E1, E2, E3, E4, E5, E6, E7);
+pfs_create_file(E1, &_pfn, E2, E3, E4, E5, E6, E7);

@ pfs_create_link_ret @
 expression _pfn, E1, E2, E3, E4, E5, E6, E7;
@@
-_pfn = pfs_create_link(E1, E2, E3, E4, E5, E6, E7);
+pfs_create_link(E1, &_pfn, E2, E3, E4, E5, E6, E7);

@ pfs_create_dir_noret @
 expression E1, E2, E3, E4, E5, E6;
@@
-pfs_create_dir(E1, E2, E3, E4, E5, E6);
+pfs_create_dir(E1, NULL, E2, E3, E4, E5, E6);

@ pfs_create_file_noret @
 expression E1, E2, E3, E4, E5, E6, E7;
@@
-pfs_create_file(E1, E2, E3, E4, E5, E6, E7);
+pfs_create_file(E1, NULL, E2, E3, E4, E5, E6, E7);

@ pfs_create_link_noret @
 expression E1, E2, E3, E4, E5, E6, E7;
@@
-pfs_create_link(E1, E2, E3, E4, E5, E6, E7);
+pfs_create_link(E1, NULL, E2, E3, E4, E5, E6, E7);
