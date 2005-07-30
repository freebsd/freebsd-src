#
# $FreeBSD$
#
# This file lists old files (OLD_FILES), libraries (OLD_LIBS) and
# directories (OLD_DIRS) which should get removed at an update. Recently
# removed entries first (with the date as a comment). Dynamic libraries are
# special cased (OLD_LIBS). Static libraries or the generic links to
# the dynamic libraries (lib*.so) should (if you don't know why to make an
# exception, make this a "must") be viewed as normal files (OLD_FILES).
#
# In case of a complete directory hierarchy the sorting is in depth first
# order.
#
# The file is partitioned: OLD_FILES first, then OLD_LIBS and OLD_DIRS last.
#

# 20050721: moved to ports
OLD_FILES+=usr/sbin/vttest
OLD_FILES+=usr/share/man/man1/vttest.1.gz
# 20050610: rexecd (insecure by design)
OLD_FILES+=etc/pam.d/rexecd
OLD_FILES+=usr/share/man/man8/rexecd.8.gz
OLD_FILES+=usr/libexec/rexecd
# 20050606: OpenBSD dhclient replaces ISC one
OLD_FILES+=bin/omshell
OLD_FILES+=usr/share/man/man1/omshell.1.gz
OLD_FILES+=usr/share/man/man5/dhcp-eval.5.gz
# 200504XX: ipf tools moved from /usr to /
OLD_FILES+=usr/sbin/ipftest
OLD_FILES+=usr/sbin/ipresend
OLD_FILES+=usr/sbin/ipsend
OLD_FILES+=usr/sbin/iptest
OLD_FILES+=usr/share/man/man1/ipnat.1.gz
OLD_FILES+=usr/share/man/man1/ipsend.1.gz
OLD_FILES+=usr/share/man/man1/iptest.1.gz
OLD_FILES+=usr/share/man/man5/ipsend.5.gz
# 200503XX: bsdtar takes over gtar
OLD_FILES+=usr/bin/gtar
OLD_FILES+=usr/share/man/man1/gtar.1.gz
# 20050324: updated release infrastructure
OLD_FILES+=usr/share/man/man5/drivers.conf.5.gz
# 20050317: removed from BIND 9 distribution
OLD_FILES+=usr/share/doc/bind9/KNOWN_DEFECTS
# 2005XXXX:
OLD_FILES+=sbin/mount_autofs
OLD_FILES+=usr/lib/libautofs.a
OLD_FILES+=usr/lib/libautofs.so
OLD_FILES+=usr/share/man/man8/mount_autofs.8.gz
# 20050203: Merged with fortunes
OLD_FILES+=usr/share/games/fortune/fortunes2
OLD_FILES+=usr/share/games/fortune/fortunes2.dat
# 200411XX: gvinum replaces vinum
OLD_FILES+=bin/vinum
OLD_FILES+=usr/share/man/man8/vinum.8.gz
# 20041109: replaced by em(4)
OLD_FILES+=usr/share/man/man4/gx.4.gz
OLD_FILES+=usr/share/man/man4/if_gx.4.gz
# 20040925: bind9 import
OLD_FILES+=usr/bin/dnskeygen
OLD_FILES+=usr/bin/dnsquery
OLD_FILES+=usr/lib/libisc.a
OLD_FILEs+=usr/lib/libisc.so
OLD_FILES+=usr/lib/libisc_p.a
OLD_FILES+=usr/libexec/named-xfer
OLD_FILES+=usr/sbin/named.restart
OLD_FILES+=usr/sbin/ndc
OLD_FILES+=usr/sbin/nslookup
OLD_FILES+=usr/sbin/nsupdate
OLD_FILES+=usr/share/doc/bind/html/acl.html
OLD_FILES+=usr/share/doc/bind/html/address_list.html
OLD_FILES+=usr/share/doc/bind/html/comments.html
OLD_FILES+=usr/share/doc/bind/html/config.html
OLD_FILES+=usr/share/doc/bind/html/controls.html
OLD_FILES+=usr/share/doc/bind/html/docdef.html
OLD_FILES+=usr/share/doc/bind/html/example.html
OLD_FILES+=usr/share/doc/bind/html/include.html
OLD_FILES+=usr/share/doc/bind/html/index.html
OLD_FILES+=usr/share/doc/bind/html/key.html
OLD_FILES+=usr/share/doc/bind/html/logging.html
OLD_FILES+=usr/share/doc/bind/html/master.html
OLD_FILES+=usr/share/doc/bind/html/options.html
OLD_FILES+=usr/share/doc/bind/html/server.html
OLD_FILES+=usr/share/doc/bind/html/trusted-keys.html
OLD_FILES+=usr/share/doc/bind/html/zone.html
OLD_FILES+=usr/share/doc/bind/misc/DynamicUpdate
OLD_FILES+=usr/share/doc/bind/misc/FAQ.1of2
OLD_FILES+=usr/share/doc/bind/misc/FAQ.2of2
OLD_FILES+=usr/share/doc/bind/misc/rfc2317-notes.txt
OLD_FILES+=usr/share/doc/bind/misc/style.txt
OLD_FILES+=usr/share/man/man1/dnskeygen.1.gz
OLD_FILES+=usr/share/man/man1/dnsquery.1.gz
OLD_FILES+=usr/share/man/man8/named-bootconf.8.gz
OLD_FILES+=usr/share/man/man8/named-xfer.8.gz
OLD_FILES+=usr/share/man/man8/named.restart.8.gz
OLD_FILES+=usr/share/man/man8/ndc.8.gz
OLD_FILES+=usr/share/man/man8/nslookup.8.gz
# 200409XX
OLD_FILES+=usr/share/man/man3/ENSURE.3.gz
OLD_FILES+=usr/share/man/man3/ENSURE_ERR.3.gz
OLD_FILES+=usr/share/man/man3/INSIST.3.gz
OLD_FILES+=usr/share/man/man3/INSIST_ERR.3.gz
OLD_FILES+=usr/share/man/man3/INVARIANT.3.gz
OLD_FILES+=usr/share/man/man3/INVARIANT_ERR.3.gz
OLD_FILES+=usr/share/man/man3/REQUIRE.3.gz
OLD_FILES+=usr/share/man/man3/REQUIRE_ERR.3.gz
OLD_FILES+=usr/share/man/man3/assertion_type_to_text.3.gz
OLD_FILES+=usr/share/man/man3/assertions.3.gz
OLD_FILES+=usr/share/man/man3/bitncmp.3.gz
OLD_FILES+=usr/share/man/man3/evAddTime.3.gz
OLD_FILES+=usr/share/man/man3/evCancelConn.3.gz
OLD_FILES+=usr/share/man/man3/evCancelRW.3.gz
OLD_FILES+=usr/share/man/man3/evClearIdleTimer.3.gz
OLD_FILES+=usr/share/man/man3/evClearTimer.3.gz
OLD_FILES+=usr/share/man/man3/evCmpTime.3.gz
OLD_FILES+=usr/share/man/man3/evConnFunc.3.gz
OLD_FILES+=usr/share/man/man3/evConnect.3.gz
OLD_FILES+=usr/share/man/man3/evConsIovec.3.gz
OLD_FILES+=usr/share/man/man3/evConsTime.3.gz
OLD_FILES+=usr/share/man/man3/evCreate.3.gz
OLD_FILES+=usr/share/man/man3/evDefer.3.gz
OLD_FILES+=usr/share/man/man3/evDeselectFD.3.gz
OLD_FILES+=usr/share/man/man3/evDestroy.3.gz
OLD_FILES+=usr/share/man/man3/evDispatch.3.gz
OLD_FILES+=usr/share/man/man3/evDo.3.gz
OLD_FILES+=usr/share/man/man3/evDrop.3.gz
OLD_FILES+=usr/share/man/man3/evFileFunc.3.gz
OLD_FILES+=usr/share/man/man3/evGetNext.3.gz
OLD_FILES+=usr/share/man/man3/evHold.3.gz
OLD_FILES+=usr/share/man/man3/evInitID.3.gz
OLD_FILES+=usr/share/man/man3/evLastEventTime.3.gz
OLD_FILES+=usr/share/man/man3/evListen.3.gz
OLD_FILES+=usr/share/man/man3/evMainLoop.3.gz
OLD_FILES+=usr/share/man/man3/evNowTime.3.gz
OLD_FILES+=usr/share/man/man3/evPrintf.3.gz
OLD_FILES+=usr/share/man/man3/evRead.3.gz
OLD_FILES+=usr/share/man/man3/evResetTimer.3.gz
OLD_FILES+=usr/share/man/man3/evSelectFD.3.gz
OLD_FILES+=usr/share/man/man3/evSetDebug.3.gz
OLD_FILES+=usr/share/man/man3/evSetIdleTimer.3.gz
OLD_FILES+=usr/share/man/man3/evSetTimer.3.gz
OLD_FILES+=usr/share/man/man3/evStreamFunc.3.gz
OLD_FILES+=usr/share/man/man3/evSubTime.3.gz
OLD_FILES+=usr/share/man/man3/evTestID.3.gz
OLD_FILES+=usr/share/man/man3/evTimeRW.3.gz
OLD_FILES+=usr/share/man/man3/evTimeSpec.3.gz
OLD_FILES+=usr/share/man/man3/evTimeVal.3.gz
OLD_FILES+=usr/share/man/man3/evTimerFunc.3.gz
OLD_FILES+=usr/share/man/man3/evTouchIdleTimer.3.gz
OLD_FILES+=usr/share/man/man3/evTryAccept.3.gz
OLD_FILES+=usr/share/man/man3/evUnhold.3.gz
OLD_FILES+=usr/share/man/man3/evUntimeRW.3.gz
OLD_FILES+=usr/share/man/man3/evUnwait.3.gz
OLD_FILES+=usr/share/man/man3/evWaitFor.3.gz
OLD_FILES+=usr/share/man/man3/evWaitFunc.3.gz
OLD_FILES+=usr/share/man/man3/evWrite.3.gz
OLD_FILES+=usr/share/man/man3/eventlib.3.gz
OLD_FILES+=usr/share/man/man3/heap.3.gz
OLD_FILES+=usr/share/man/man3/heap_decreased.3.gz
OLD_FILES+=usr/share/man/man3/heap_delete.3.gz
OLD_FILES+=usr/share/man/man3/heap_element.3.gz
OLD_FILES+=usr/share/man/man3/heap_for_each.3.gz
OLD_FILES+=usr/share/man/man3/heap_free.3.gz
OLD_FILES+=usr/share/man/man3/heap_increased.3.gz
OLD_FILES+=usr/share/man/man3/heap_insert.3.gz
OLD_FILES+=usr/share/man/man3/heap_new.3.gz
OLD_FILES+=usr/share/man/man3/log_add_channel.3.gz
OLD_FILES+=usr/share/man/man3/log_category_is_active.3.gz
OLD_FILES+=usr/share/man/man3/log_close_stream.3.gz
OLD_FILES+=usr/share/man/man3/log_dec_references.3.gz
OLD_FILES+=usr/share/man/man3/log_free_channel.3.gz
OLD_FILES+=usr/share/man/man3/log_free_context.3.gz
OLD_FILES+=usr/share/man/man3/log_get_filename.3.gz
OLD_FILES+=usr/share/man/man3/log_get_stream.3.gz
OLD_FILES+=usr/share/man/man3/log_inc_references.3.gz
OLD_FILES+=usr/share/man/man3/log_new_context.3.gz
OLD_FILES+=usr/share/man/man3/log_new_file_channel.3.gz
OLD_FILES+=usr/share/man/man3/log_new_null_channel.3.gz
OLD_FILES+=usr/share/man/man3/log_new_syslog_channel.3.gz
OLD_FILES+=usr/share/man/man3/log_open_stream.3.gz
OLD_FILES+=usr/share/man/man3/log_option.3.gz
OLD_FILES+=usr/share/man/man3/log_remove_channel.3.gz
OLD_FILES+=usr/share/man/man3/log_set_file_owner.3.gz
OLD_FILES+=usr/share/man/man3/log_vwrite.3.gz
OLD_FILES+=usr/share/man/man3/log_write.3.gz
OLD_FILES+=usr/share/man/man3/logging.3.gz
OLD_FILES+=usr/share/man/man3/memcluster.3.gz
OLD_FILES+=usr/share/man/man3/memget.3.gz
OLD_FILES+=usr/share/man/man3/memput.3.gz
OLD_FILES+=usr/share/man/man3/memstats.3.gz
OLD_FILES+=usr/share/man/man3/set_assertion_failure_callback.3.
OLD_FILES+=usr/share/man/man3/sigwait.3.gz
OLD_FILES+=usr/share/man/man3/tree_add.3.gz
OLD_FILES+=usr/share/man/man3/tree_delete.3.gz
OLD_FILES+=usr/share/man/man3/tree_init.3.gz
OLD_FILES+=usr/share/man/man3/tree_mung.3.gz
OLD_FILES+=usr/share/man/man3/tree_srch.3.gz
OLD_FILES+=usr/share/man/man3/tree_trav.3.gz
# 2004XXYY: OS internal libs, no ports use them, no need to use OLD_LIBS
OLD_FILES+=lib/geom/geom_concat.so.1
OLD_FILES+=lib/geom/geom_label.so.1
OLD_FILES+=lib/geom/geom_nop.so.1
OLD_FILES+=lib/geom/geom_stripe.so.1
# 200407XX
OLD_FILES+=usr/sbin/kernbb
OLD_FILES+=usr/sbin/ntp-genkeys
OLD_FILES+=usr/sbin/ntptimeset
OLD_FILES+=usr/share/man/man8/kernbb.8.gz
OLD_FILES+=usr/share/man/man8/ntp-genkeys.8.gz
# 200406XX
OLD_FILES+=usr/bin/gasp
OLD_FILES+=usr/bin/gdbreplay
OLD_FILES+=usr/share/man/man1/gasp.1.gz
# 200405XX: arl
OLD_FILES+=usr/sbin/arlconfig
OLD_FILES+=usr/share/man/man8/arlconfig.8.gz
# 200403XX
OLD_FILES+=bin/raidctl
OLD_FILES+=usr/bin/sasc
OLD_FILES+=usr/sbin/sgsc
OLD_FILES+=usr/sbin/stlload
OLD_FILES+=usr/sbin/stlstats
OLD_FILES+=usr/share/man/man1/sasc.1.gz
OLD_FILES+=usr/share/man/man1/sgsc.1.gz
OLD_FILES+=usr/share/man/man4/i386/stl.4.gz
OLD_FILES+=usr/share/man/man8/raidctl.8.gz
.if ${MACHINE_ARCH} != "alpha" && ${MACHINE_ARCH} != "sparc64"
# 20040130: libkse renamed to libpthread
OLD_FILES+=usr/lib/libkse.a
OLD_FILES+=usr/lib/libkse.so
.endif
# 200XYYZZ: /lib transition clitches
OLD_FILES+=lib/libalias.so
OLD_FILES+=lib/libatm.so
OLD_FILES+=lib/libbsdxml.so
OLD_FILES+=lib/libc.so
OLD_FILES+=lib/libcam.so
OLD_FILES+=lib/libcrypt.so
OLD_FILES+=lib/libcrypto.so
OLD_FILES+=lib/libdevstat.so
OLD_FILES+=lib/libedit.so
OLD_FILES+=lib/libgeom.so
OLD_FILES+=lib/libipsec.so
OLD_FILES+=lib/libipx.so
OLD_FILES+=lib/libkvm.so
OLD_FILES+=lib/libm.so
OLD_FILES+=lib/libmd.so
OLD_FILES+=lib/libncurses.so
OLD_FILES+=lib/libreadline.so
OLD_FILES+=lib/libsbuf.so
OLD_FILES+=lib/libufs.so
OLD_FILES+=lib/libz.so
# 200312XX
OLD_FILES+=bin/cxconfig
OLD_FILES+=usr/share/man/man8/cxconfig.8.gz
# 200309XX
OLD_FILES+=usr/bin/symorder
OLD_FILES+=usr/share/man/man1/symorder.1.gz
# 200308XX
OLD_FILES+=usr/sbin/amldb
OLD_FILES+=usr/share/man/man8/amldb.8.gz
# 200306XX
OLD_FILES+=bin/mount_nwfs
OLD_FILES+=bin/mount_portalfs
OLD_FILES+=bin/mount_smbfs
OLD_FILES+=usr/sbin/dev_mkdb
OLD_FILES+=usr/share/man/man8/dev_mkdb.8.gz
# 200304XX
OLD_FILES+=usr/lib/libcipher.a
OLD_FILES+=usr/lib/libcipher.so
OLD_FILES+=usr/lib/libcipher_p.a
OLD_FILES+=usr/lib/libgmp.a
OLD_FILES+=usr/lib/libgmp.so
OLD_FILES+=usr/lib/libgmp_p.a
OLD_FILES+=usr/lib/libperl.a
OLD_FILES+=usr/lib/libperl.so
OLD_FILES+=usr/lib/libperl_p.a
OLD_FILES+=usr/lib/libposix1e.a
OLD_FILES+=usr/lib/libposix1e.so
OLD_FILES+=usr/lib/libposix1e_p.a
OLD_FILES+=usr/lib/libskey.a
OLD_FILES+=usr/lib/libskey.so
OLD_FILES+=usr/lib/libskey_p.a
# 200304XX: removal of xten
OLD_FILES+=usr/sbin/xten
OLD_FILES+=usr/share/man/man1/xten.1.gz
OLD_FILES+=usr/share/man/man8/xtend.8.gz
# 200303XX
OLD_FILES+=usr/lib/libacl.so
OLD_FILES+=usr/lib/libdescrypt.so
OLD_FILES+=usr/lib/libf2c.so
OLD_FILES+=usr/lib/libg++.so
OLD_FILES+=usr/lib/libkdb.so
OLD_FILES+=usr/lib/librsaINTL.so
OLD_FILES+=usr/lib/libscrypt.so
OLD_FILES+=usr/lib/libss.so
# 200302XX
OLD_FILES+=usr/lib/libacl.a
OLD_FILES+=usr/lib/libacl_p.a
OLD_FILES+=usr/lib/libkadm.a
OLD_FILES+=usr/lib/libkadm.so
OLD_FILES+=usr/lib/libkadm_p.a
OLD_FILES+=usr/lib/libkafs.a
OLD_FILES+=usr/lib/libkafs.so
OLD_FILES+=usr/lib/libkafs_p.a
OLD_FILES+=usr/lib/libkdb.a
OLD_FILES+=usr/lib/libkdb_p.a
OLD_FILES+=usr/lib/libkrb.a
OLD_FILES+=usr/lib/libkrb.so
OLD_FILES+=usr/lib/libkrb_p.a
OLD_FILES+=usr/share/man/man3/SSL_CIPHER_get_name.3.gz
OLD_FILES+=usr/share/man/man3/SSL_COMP_add_compression_method.3
OLD_FILES+=usr/share/man/man3/SSL_CTX_add_extra_chain_cert.3.gz
OLD_FILES+=usr/share/man/man3/SSL_CTX_add_session.3.gz
OLD_FILES+=usr/share/man/man3/SSL_CTX_ctrl.3.gz
OLD_FILES+=usr/share/man/man3/SSL_CTX_flush_sessions.3.gz
OLD_FILES+=usr/share/man/man3/SSL_CTX_free.3.gz
OLD_FILES+=usr/share/man/man3/SSL_CTX_get_verify_mode.3.gz
OLD_FILES+=usr/share/man/man3/SSL_CTX_load_verify_locations.3.gz
OLD_FILES+=usr/share/man/man3/SSL_CTX_new.3.gz
OLD_FILES+=usr/share/man/man3/SSL_CTX_sess_number.3.gz
OLD_FILES+=usr/share/man/man3/SSL_CTX_sess_set_cache_size.3.gz
OLD_FILES+=usr/share/man/man3/SSL_CTX_sess_set_get_cb.3.gz
OLD_FILES+=usr/share/man/man3/SSL_CTX_sessions.3.gz
OLD_FILES+=usr/share/man/man3/SSL_CTX_set_cert_store.3.gz
OLD_FILES+=usr/share/man/man3/SSL_CTX_set_cert_verify_callback.3.gz
OLD_FILES+=usr/share/man/man3/SSL_CTX_set_cipher_list.3.gz
OLD_FILES+=usr/share/man/man3/SSL_CTX_set_client_CA_list.3.gz
OLD_FILES+=usr/share/man/man3/SSL_CTX_set_client_cert_cb.3.gz
OLD_FILES+=usr/share/man/man3/SSL_CTX_set_default_passwd_cb.3.gz
OLD_FILES+=usr/share/man/man3/SSL_CTX_set_generate_session_id.3.gz
OLD_FILES+=usr/share/man/man3/SSL_CTX_set_info_callback.3.gz
OLD_FILES+=usr/share/man/man3/SSL_CTX_set_max_cert_list.3.gz
OLD_FILES+=usr/share/man/man3/SSL_CTX_set_mode.3.gz
OLD_FILES+=usr/share/man/man3/SSL_CTX_set_msg_callback.3.gz
OLD_FILES+=usr/share/man/man3/SSL_CTX_set_options.3.gz
OLD_FILES+=usr/share/man/man3/SSL_CTX_set_quiet_shutdown.3.gz
OLD_FILES+=usr/share/man/man3/SSL_CTX_set_session_cache_mode.3.gz
OLD_FILES+=usr/share/man/man3/SSL_CTX_set_session_id_context.3.gz
OLD_FILES+=usr/share/man/man3/SSL_CTX_set_ssl_version.3.gz
OLD_FILES+=usr/share/man/man3/SSL_CTX_set_timeout.3.gz
OLD_FILES+=usr/share/man/man3/SSL_CTX_set_tmp_dh_callback.3.gz
OLD_FILES+=usr/share/man/man3/SSL_CTX_set_tmp_rsa_callback.3.gz
OLD_FILES+=usr/share/man/man3/SSL_CTX_set_verify.3.gz
OLD_FILES+=usr/share/man/man3/SSL_CTX_use_certificate.3.gz
OLD_FILES+=usr/share/man/man3/SSL_SESSION_free.3.gz
OLD_FILES+=usr/share/man/man3/SSL_SESSION_get_ex_new_index.3.gz
OLD_FILES+=usr/share/man/man3/SSL_SESSION_get_time.3.gz
OLD_FILES+=usr/share/man/man3/SSL_accept.3.gz
OLD_FILES+=usr/share/man/man3/SSL_alert_type_string.3.gz
OLD_FILES+=usr/share/man/man3/SSL_clear.3.gz
OLD_FILES+=usr/share/man/man3/SSL_connect.3.gz
OLD_FILES+=usr/share/man/man3/SSL_do_handshake.3.gz
OLD_FILES+=usr/share/man/man3/SSL_free.3.gz
OLD_FILES+=usr/share/man/man3/SSL_get_SSL_CTX.3.gz
OLD_FILES+=usr/share/man/man3/SSL_get_ciphers.3.gz
OLD_FILES+=usr/share/man/man3/SSL_get_client_CA_list.3.gz
OLD_FILES+=usr/share/man/man3/SSL_get_current_cipher.3.gz
OLD_FILES+=usr/share/man/man3/SSL_get_default_timeout.3.gz
OLD_FILES+=usr/share/man/man3/SSL_get_error.3.gz
OLD_FILES+=usr/share/man/man3/SSL_get_ex_data_X509_STORE_CTX_idx.3.gz
OLD_FILES+=usr/share/man/man3/SSL_get_ex_new_index.3.gz
OLD_FILES+=usr/share/man/man3/SSL_get_fd.3.gz
OLD_FILES+=usr/share/man/man3/SSL_get_peer_cert_chain.3.gz
OLD_FILES+=usr/share/man/man3/SSL_get_peer_certificate.3.gz
OLD_FILES+=usr/share/man/man3/SSL_get_rbio.3.gz
OLD_FILES+=usr/share/man/man3/SSL_get_session.3.gz
OLD_FILES+=usr/share/man/man3/SSL_get_verify_result.3.gz
OLD_FILES+=usr/share/man/man3/SSL_get_version.3.gz
OLD_FILES+=usr/share/man/man3/SSL_library_init.3.gz
OLD_FILES+=usr/share/man/man3/SSL_load_client_CA_file.3.gz
OLD_FILES+=usr/share/man/man3/SSL_new.3.gz
OLD_FILES+=usr/share/man/man3/SSL_pending.3.gz
OLD_FILES+=usr/share/man/man3/SSL_read.3.gz
OLD_FILES+=usr/share/man/man3/SSL_rstate_string.3.gz
OLD_FILES+=usr/share/man/man3/SSL_session_reused.3.gz
OLD_FILES+=usr/share/man/man3/SSL_set_bio.3.gz
OLD_FILES+=usr/share/man/man3/SSL_set_connect_state.3.gz
OLD_FILES+=usr/share/man/man3/SSL_set_fd.3.gz
OLD_FILES+=usr/share/man/man3/SSL_set_session.3.gz
OLD_FILES+=usr/share/man/man3/SSL_set_shutdown.3.gz
OLD_FILES+=usr/share/man/man3/SSL_set_verify_result.3.gz
OLD_FILES+=usr/share/man/man3/SSL_shutdown.3.gz
OLD_FILES+=usr/share/man/man3/SSL_state_string.3.gz
OLD_FILES+=usr/share/man/man3/SSL_want.3.gz
OLD_FILES+=usr/share/man/man3/SSL_write.3.gz
OLD_FILES+=usr/share/man/man3/d2i_SSL_SESSION.3.gz
# 200301XX
OLD_FILES+=usr/share/man/man3/des_3cbc_encrypt.3.gz
OLD_FILES+=usr/share/man/man3/des_3ecb_encrypt.3.gz
OLD_FILES+=usr/share/man/man3/des_cbc_cksum.3.gz
OLD_FILES+=usr/share/man/man3/des_cbc_encrypt.3.gz
OLD_FILES+=usr/share/man/man3/des_cfb_encrypt.3.gz
OLD_FILES+=usr/share/man/man3/des_ecb_encrypt.3.gz
OLD_FILES+=usr/share/man/man3/des_enc_read.3.gz
OLD_FILES+=usr/share/man/man3/des_enc_write.3.gz
OLD_FILES+=usr/share/man/man3/des_is_weak_key.3.gz
OLD_FILES+=usr/share/man/man3/des_key_sched.3.gz
OLD_FILES+=usr/share/man/man3/des_ofb_encrypt.3.gz
OLD_FILES+=usr/share/man/man3/des_pcbc_encrypt.3.gz
OLD_FILES+=usr/share/man/man3/des_quad_cksum.3.gz
OLD_FILES+=usr/share/man/man3/des_random_key.3.gz
OLD_FILES+=usr/share/man/man3/des_read_2password.3.gz
OLD_FILES+=usr/share/man/man3/des_read_password.3.gz
OLD_FILES+=usr/share/man/man3/des_read_pw_string.3.gz
OLD_FILES+=usr/share/man/man3/des_set_key.3.gz
OLD_FILES+=usr/share/man/man3/des_set_odd_parity.3.gz
OLD_FILES+=usr/share/man/man3/des_string_to_2key.3.gz
OLD_FILES+=usr/share/man/man3/des_string_to_key.3.gz
# 200210XX
OLD_FILES+=usr/share/man/man3/All_FreeBSD.3.gz
OLD_FILES+=usr/share/man/man3/CheckRules.3.gz
OLD_FILES+=usr/share/man/man3/ChunkCanBeRoot.3.gz
OLD_FILES+=usr/share/man/man3/Clone_Disk.3.gz
OLD_FILES+=usr/share/man/man3/Collapse_Chunk.3.gz
OLD_FILES+=usr/share/man/man3/Collapse_Disk.3.gz
OLD_FILES+=usr/share/man/man3/Create_Chunk.3.gz
OLD_FILES+=usr/share/man/man3/Create_Chunk_DWIM.3.gz
OLD_FILES+=usr/share/man/man3/Cyl_Aligned.3.gz
OLD_FILES+=usr/share/man/man3/Debug_Disk.3.gz
OLD_FILES+=usr/share/man/man3/Delete_Chunk.3.gz
OLD_FILES+=usr/share/man/man3/Disk_Names.3.gz
OLD_FILES+=usr/share/man/man3/Free_Disk.3.gz
OLD_FILES+=usr/share/man/man3/MakeDev.3.gz
OLD_FILES+=usr/share/man/man3/MakeDevDisk.3.gz
OLD_FILES+=usr/share/man/man3/Next_Cyl_Aligned.3.gz
OLD_FILES+=usr/share/man/man3/Next_Track_Aligned.3.gz
OLD_FILES+=usr/share/man/man3/Open_Disk.3.gz
OLD_FILES+=usr/share/man/man3/Prev_Cyl_Aligned.3.gz
OLD_FILES+=usr/share/man/man3/Prev_Track_Aligned.3.gz
OLD_FILES+=usr/share/man/man3/Set_Bios_Geom.3.gz
OLD_FILES+=usr/share/man/man3/Set_Boot_Blocks.3.gz
OLD_FILES+=usr/share/man/man3/Set_Boot_Mgr.3.gz
OLD_FILES+=usr/share/man/man3/ShowChunkFlags.3.gz
OLD_FILES+=usr/share/man/man3/Track_Aligned.3.gz
OLD_FILES+=usr/share/man/man3/Write_Disk.3.gz
OLD_FILES+=usr/share/man/man3/slice_type_name.3.gz
# 200210XX: most games moved to ports
OLD_FILES+=usr/share/man/man6/adventure.6.gz
OLD_FILES+=usr/share/man/man6/arithmetic.6.gz
OLD_FILES+=usr/share/man/man6/atc.6.gz
OLD_FILES+=usr/share/man/man6/backgammon.6.gz
OLD_FILES+=usr/share/man/man6/battlestar.6.gz
OLD_FILES+=usr/share/man/man6/bs.6.gz
OLD_FILES+=usr/share/man/man6/canfield.6.gz
OLD_FILES+=usr/share/man/man6/cfscores.6.gz
OLD_FILES+=usr/share/man/man6/cribbage.6.gz
OLD_FILES+=usr/share/man/man6/fish.6.gz
OLD_FILES+=usr/share/man/man6/hack.6.gz
OLD_FILES+=usr/share/man/man6/hangman.6.gz
OLD_FILES+=usr/share/man/man6/larn.6.gz
OLD_FILES+=usr/share/man/man6/mille.6.gz
OLD_FILES+=usr/share/man/man6/phantasia.6.gz
OLD_FILES+=usr/share/man/man6/piano.6.gz
OLD_FILES+=usr/share/man/man6/pig.6.gz
OLD_FILES+=usr/share/man/man6/quiz.6.gz
OLD_FILES+=usr/share/man/man6/rain.6.gz
OLD_FILES+=usr/share/man/man6/robots.6.gz
OLD_FILES+=usr/share/man/man6/rogue.6.gz
OLD_FILES+=usr/share/man/man6/sail.6.gz
OLD_FILES+=usr/share/man/man6/snake.6.gz
OLD_FILES+=usr/share/man/man6/snscore.6.gz
OLD_FILES+=usr/share/man/man6/trek.6.gz
OLD_FILES+=usr/share/man/man6/wargames.6.gz
OLD_FILES+=usr/share/man/man6/worm.6.gz
OLD_FILES+=usr/share/man/man6/worms.6.gz
OLD_FILES+=usr/share/man/man6/wump.6.gz
# 200207XX
OLD_FILES+=bin/mountd
OLD_FILES+=bin/nfsd
# 200206XX
OLD_FILES+=usr/lib/libpam_ssh.a
OLD_FILES+=usr/lib/libpam_ssh_p.a
# 200203XX
OLD_FILES+=usr/lib/libss.a
OLD_FILES+=usr/lib/libss_p.a
OLD_FILES+=usr/lib/libtelnet.a
OLD_FILES+=usr/lib/libtelnet_p.a
OLD_FILES+=usr/lib/libusb.a
OLD_FILES+=usr/lib/libusb.so
OLD_FILES+=usr/lib/libusb_p.a
# 2001XXXX
OLD_FILES+=etc/security
# 200104XX
OLD_FILES+=usr/lib/libdescrypt.a
OLD_FILES+=usr/lib/libscrypt.a
OLD_FILES+=usr/lib/libscrypt_p.a
# 20001200
OLD_FILES+=usr/lib/libgcc_r_pic.a
# 200009XX
OLD_FILES+=usr/lib/libRSAglue.a
OLD_FILES+=usr/lib/libRSAglue.so
OLD_FILES+=usr/lib/librsaINTL.a
OLD_FILES+=usr/lib/librsaUSA.a
OLD_FILES+=usr/lib/librsaUSA.so
# 200002XX ?
OLD_FILES+=usr/lib/libf2c.a
OLD_FILES+=usr/lib/libf2c_p.a
OLD_FILES+=usr/lib/libg++.a
OLD_FILES+=usr/lib/libg++_p.a
# 20001006
OLD_FILES+=usr/bin/miniperl
# 20000810
OLD_FILES+=usr/bin/sperl
# 199909XX
OLD_FILES+=usr/lib/libdesrypt_p.a
## unsorted
# do we still support aout builds?
#OLD_FILES+=usr/lib/aout/c++rt0.o
#OLD_FILES+=usr/lib/aout/crt0.o
#OLD_FILES+=usr/lib/aout/gcrt0.o
#OLD_FILES+=usr/lib/aout/scrt0.o
#OLD_FILES+=usr/lib/aout/sgcrt0.o

# 20050722: bump for 6.0-RELEASE
OLD_LIBS+=lib/libalias.so.4
OLD_LIBS+=lib/libatm.so.2
OLD_LIBS+=lib/libbegemot.so.1
OLD_LIBS+=lib/libbsdxml.so.1
OLD_LIBS+=lib/libbsnmp.so.2
OLD_LIBS+=lib/libc.so.5
OLD_LIBS+=lib/libcam.so.2
OLD_LIBS+=lib/libcrypt.so.2
OLD_LIBS+=lib/libcrypto.so.3
OLD_LIBS+=lib/libdevstat.so.4
OLD_LIBS+=lib/libedit.so.4
OLD_LIBS+=lib/libgeom.so.2
OLD_LIBS+=lib/libgpib.so.0
OLD_LIBS+=lib/libipsec.so.1
OLD_LIBS+=lib/libipx.so.2
OLD_LIBS+=lib/libkiconv.so.1
OLD_LIBS+=lib/libkvm.so.2
OLD_LIBS+=lib/libm.so.3
OLD_LIBS+=lib/libmd.so.2
OLD_LIBS+=lib/libncurses.so.5
OLD_LIBS+=lib/libreadline.so.5
OLD_LIBS+=lib/libsbuf.so.2
OLD_LIBS+=lib/libufs.so.2
OLD_LIBS+=lib/libutil.so.4
OLD_LIBS+=lib/libz.so.2
OLD_LIBS+=usr/lib/libarchive.so.1
OLD_LIBS+=usr/lib/libasn1.so.7
OLD_LIBS+=usr/lib/libbluetooth.so.1
OLD_LIBS+=usr/lib/libbz2.so.1
OLD_LIBS+=usr/lib/libc_r.so.5
OLD_LIBS+=usr/lib/libcalendar.so.2
OLD_LIBS+=usr/lib/libcom_err.so.2
OLD_LIBS+=usr/lib/libdevinfo.so.2
OLD_LIBS+=usr/lib/libdialog.so.4
OLD_LIBS+=usr/lib/libfetch.so.3
OLD_LIBS+=usr/lib/libform.so.2
OLD_LIBS+=usr/lib/libftpio.so.5
OLD_LIBS+=usr/lib/libg2c.so.1
OLD_LIBS+=usr/lib/libgnuregex.so.2
OLD_LIBS+=usr/lib/libgssapi.so.7
OLD_LIBS+=usr/lib/libhdb.so.7
OLD_LIBS+=usr/lib/libhistory.so.5
OLD_LIBS+=usr/lib/libkadm5clnt.so.7
OLD_LIBS+=usr/lib/libkadm5srv.so.7
OLD_LIBS+=usr/lib/libafs5.so.7
OLD_LIBS+=usr/lib/libkrb5.so.7
OLD_LIBS+=usr/lib/libmagic.so.1
OLD_LIBS+=usr/lib/libmenu.so.2
OLD_LIBS+=usr/lib/libmilter.so.2
OLD_LIBS+=usr/lib/libmp.so.4
OLD_LIBS+=usr/lib/libncp.so.1
OLD_LIBS+=usr/lib/libnetgraph.so.1
OLD_LIBS+=usr/lib/libngatm.so.1
OLD_LIBS+=usr/lib/libobjc.so.1
OLD_LIBS+=usr/lib/libopie.so.3
OLD_LIBS+=usr/lib/libpam.so.2
OLD_LIBS+=usr/lib/libpanel.so.2
OLD_LIBS+=usr/lib/libpcap.so.3
OLD_LIBS+=usr/lib/libpmc.so.2
OLD_LIBS+=usr/lib/libpthread.so.1
OLD_LIBS+=usr/lib/libradius.so.1
OLD_LIBS+=usr/lib/libroken.so.7
OLD_LIBS+=usr/lib/librpcsvc.so.2
OLD_LIBS+=usr/lib/libsdp.so.1
OLD_LIBS+=usr/lib/libsmb.so.1
OLD_LIBS+=usr/lib/libssh.so.2
OLD_LIBS+=usr/lib/libssl.so.3
OLD_LIBS+=usr/lib/libstdc++.so.4
OLD_LIBS+=usr/lib/libtacplus.so.1
OLD_LIBS+=usr/lib/libthr.so.1
OLD_LIBS+=usr/lib/libthread_db.so.1
OLD_LIBS+=usr/lib/libugidfw.so.1
OLD_LIBS+=usr/lib/libusbhid.so.1
OLD_LIBS+=usr/lib/libvgl.so.3
OLD_LIBS+=usr/lib/libwrap.so.3
OLD_LIBS+=usr/lib/libypclnt.so.1
OLD_LIBS+=usr/lib/pam_chroot.so.2
OLD_LIBS+=usr/lib/pam_deny.so.2
OLD_LIBS+=usr/lib/pam_echo.so.2
OLD_LIBS+=usr/lib/pam_exec.so.2
OLD_LIBS+=usr/lib/pam_ftpusers.so.2
OLD_LIBS+=usr/lib/pam_group.so.2
OLD_LIBS+=usr/lib/pam_guest.so.2
OLD_LIBS+=usr/lib/pam_krb5.so.2
OLD_LIBS+=usr/lib/pam_ksu.so.2
OLD_LIBS+=usr/lib/pam_lastlog.so.2
OLD_LIBS+=usr/lib/pam_login_access.so.2
OLD_LIBS+=usr/lib/pam_nologin.so.2
OLD_LIBS+=usr/lib/pam_opie.so.2
OLD_LIBS+=usr/lib/pam_opieaccess.so.2
OLD_LIBS+=usr/lib/pam_passwdqc.so.2
OLD_LIBS+=usr/lib/pam_permit.so.2
OLD_LIBS+=usr/lib/pam_radius.so.2
OLD_LIBS+=usr/lib/pam_rhosts.so.2
OLD_LIBS+=usr/lib/pam_rootok.so.2
OLD_LIBS+=usr/lib/pam_securetty.so.2
OLD_LIBS+=usr/lib/pam_self.so.2
OLD_LIBS+=usr/lib/pam_ssh.so.2
OLD_LIBS+=usr/lib/pam_tacplus.so.2
OLD_LIBS+=usr/lib/pam_unix.so.2
OLD_LIBS+=usr/lib/snmp_atm.so.3
OLD_LIBS+=usr/lib/snmp_mibII.so.3
OLD_LIBS+=usr/lib/snmp_netgraph.so.3
OLD_LIBS+=usr/lib/snmp_pf.so.3
# 20050603: network related ABI change on 64bit systems
OLD_LIBS+=usr/lib/libpcap.so.3
# 200505XX: ?
OLD_LIBS+=usr/lib/snmp_atm.so.2
OLD_LIBS+=usr/lib/snmp_mibII.so.2
OLD_LIBS+=usr/lib/snmp_netgraph.so.2
OLD_LIBS+=usr/lib/snmp_pf.so.2
# 2005XXXX: not ready for primetime yet
OLD_LIBS+=usr/lib/libautofs.so.1
# 200410XX: libm compatibility fix
OLD_LIBS+=lib/libm.so.2
# 20041001: version bump
OLD_LIBS+=lib/libreadline.so.4
OLD_LIBS+=usr/lib/libhistory.so.4
OLD_LIBS+=usr/lib/libopie.so.2
OLD_LIBS+=usr/lib/libpcap.so.2
# 20040925: bind9 import
OLD_LIBS+=usr/lib/libisc.so.1
# 200408XX
OLD_LIBS+=usr/lib/snmp_netgraph.so.1
.if ${MACHINE_ARCH} != "alpha" && ${MACHINE_ARCH} != "sparc64"
# 20040130: libkse renamed to libpthread
OLD_LIBS+=usr/lib/libkse.so.1
.endif
# 200404XX
OLD_LIBS+=usr/lib/libsnmp.so.1
OLD_LIBS+=usr/lib/snmp_mibII.so.1
# 200309XX
OLD_LIBS+=usr/lib/libasn1.so.6
OLD_LIBS+=usr/lib/libhdb.so.6
OLD_LIBS+=usr/lib/libkadm5clnt.so.6
OLD_LIBS+=usr/lib/libkadm5srv.so.6
OLD_LIBS+=usr/lib/libkrb5.so.6
OLD_LIBS+=usr/lib/libroken.so.6
# 200304XX
OLD_LIBS+=usr/lib/libc.so.4
OLD_LIBS+=usr/lib/libc_r.so.4
OLD_LIBS+=usr/lib/libdevstat.so.2
OLD_LIBS+=usr/lib/libedit.so.3
OLD_LIBS+=usr/lib/libgmp.so.3
OLD_LIBS+=usr/lib/libmp.so.3
OLD_LIBS+=usr/lib/libpam.so.1
OLD_LIBS+=usr/lib/libposix1e.so.2
OLD_LIBS+=usr/lib/libskey.so.2
OLD_LIBS+=usr/lib/libusbhid.so.0
OLD_LIBS+=usr/lib/libvgl.so.2
# 200302XX
OLD_LIBS+=usr/lib/libacl.so.3
OLD_LIBS+=usr/lib/libasn1.so.5
OLD_LIBS+=usr/lib/libcrypto.so.2
OLD_LIBS+=usr/lib/libgssapi.so.5
OLD_LIBS+=usr/lib/libhdb.so.5
OLD_LIBS+=usr/lib/libkadm.so.3
OLD_LIBS+=usr/lib/libkadm5clnt.so.5
OLD_LIBS+=usr/lib/libkadm5srv.so.5
OLD_LIBS+=usr/lib/libkafs.so.3
OLD_LIBS+=usr/lib/libkafs5.so.5
OLD_LIBS+=usr/lib/libkdb.so.3
OLD_LIBS+=usr/lib/libkrb.so.3
OLD_LIBS+=usr/lib/libroken.so.
OLD_LIBS+=usr/lib/libssl.so.2
OLD_LIBS+=usr/lib/pam_kerberosIV.so
# 200203XX
OLD_LIBS+=usr/lib/libss.so.3
OLD_LIBS+=usr/lib/libusb.so.0
# 200112XX
OLD_LIBS+=usr/lib/libfetch.so.2
# 200104XX
OLD_LIBS+=usr/lib/libdescrypt.so.2
OLD_LIBS+=usr/lib/libscrypt.so.2
# 200102XX
OLD_LIBS+=usr/lib/libcrypto.so.1
OLD_LIBS+=usr/lib/libssl.so.1
# 200009XX
OLD_LIBS+=usr/lib/libRSAglue.so.1
OLD_LIBS+=usr/lib/librsaINTL.so.1
OLD_LIBS+=usr/lib/librsaUSA.so.1
# 200006XX
OLD_LIBS+=usr/lib/libalias.so.3
OLD_LIBS+=usr/lib/libfetch.so.1
OLD_LIBS+=usr/lib/libipsec.so.0
# 200005XX
OLD_LIBS+=usr/lib/libxpg4.so.2
# 200002XX
OLD_LIBS+=usr/lib/libc.so.3
OLD_LIBS+=usr/lib/libcurses.so.2
OLD_LIBS+=usr/lib/libdialog.so.3
OLD_LIBS+=usr/lib/libedit.so.2
OLD_LIBS+=usr/lib/libf2c.so.2
OLD_LIBS+=usr/lib/libftpio.so.4
OLD_LIBS+=usr/lib/libg++.so.4
OLD_LIBS+=usr/lib/libhistory.so.3
OLD_LIBS+=usr/lib/libmytinfo.so.2
OLD_LIBS+=usr/lib/libncurses.so.3
OLD_LIBS+=usr/lib/libreadline.so.3
OLD_LIBS+=usr/lib/libss.so.2
OLD_LIBS+=usr/lib/libtermcap.so.2
OLD_LIBS+=usr/lib/libutil.so.2
OLD_LIBS+=usr/lib/libvgl.so.1
OLD_LIBS+=usr/lib/libwrap.so.2
# 199909XX
OLD_LIBS+=usr/lib/libc_r.so.3

# 20040925: bind9 import
OLD_DIRS+=usr/share/doc/bind/html
OLD_DIRS+=usr/share/doc/bind/misc
OLD_DIRS+=usr/share/doc/bind/

