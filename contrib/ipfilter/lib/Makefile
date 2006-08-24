INCDEP=$(TOP)/ip_compat.h $(TOP)/ip_fil.h $(TOP)/ipf.h

LIBOBJS=$(DEST)/addicmp.o \
     $(DEST)/addipopt.o \
     $(DEST)/bcopywrap.o \
     $(DEST)/binprint.o \
     $(DEST)/buildopts.o \
     $(DEST)/checkrev.o \
     $(DEST)/count6bits.o \
     $(DEST)/count4bits.o \
     $(DEST)/debug.o \
     $(DEST)/extras.o \
     $(DEST)/facpri.o \
     $(DEST)/flags.o \
     $(DEST)/fill6bits.o \
     $(DEST)/genmask.o \
     $(DEST)/gethost.o \
     $(DEST)/getifname.o \
     $(DEST)/getline.o \
     $(DEST)/getnattype.o \
     $(DEST)/getport.o \
     $(DEST)/getportproto.o \
     $(DEST)/getproto.o \
     $(DEST)/getsumd.o \
     $(DEST)/hexdump.o \
     $(DEST)/hostmask.o \
     $(DEST)/hostname.o \
     $(DEST)/hostnum.o \
     $(DEST)/icmpcode.o \
     $(DEST)/inet_addr.o \
     $(DEST)/initparse.o \
     $(DEST)/ionames.o \
     $(DEST)/ipoptsec.o \
     $(DEST)/ipf_dotuning.o \
     $(DEST)/ipft_ef.o \
     $(DEST)/ipft_hx.o \
     $(DEST)/ipft_pc.o \
     $(DEST)/ipft_sn.o \
     $(DEST)/ipft_td.o \
     $(DEST)/ipft_tx.o \
     $(DEST)/kmem.o \
     $(DEST)/kmemcpywrap.o \
     $(DEST)/kvatoname.o \
     $(DEST)/load_hash.o \
     $(DEST)/load_hashnode.o \
     $(DEST)/load_pool.o \
     $(DEST)/load_poolnode.o \
     $(DEST)/loglevel.o \
     $(DEST)/mutex_emul.o \
     $(DEST)/nametokva.o \
     $(DEST)/nat_setgroupmap.o \
     $(DEST)/ntomask.o \
     $(DEST)/optname.o \
     $(DEST)/optprint.o \
     $(DEST)/optprintv6.o \
     $(DEST)/optvalue.o \
     $(DEST)/portname.o \
     $(DEST)/portnum.o \
     $(DEST)/ports.o \
     $(DEST)/print_toif.o \
     $(DEST)/printactivenat.o \
     $(DEST)/printaps.o \
     $(DEST)/printbuf.o \
     $(DEST)/printhash.o \
     $(DEST)/printhashnode.o \
     $(DEST)/printip.o \
     $(DEST)/printpool.o \
     $(DEST)/printpoolnode.o \
     $(DEST)/printproto.o \
     $(DEST)/printfr.o \
     $(DEST)/printfraginfo.o \
     $(DEST)/printhostmap.o \
     $(DEST)/printifname.o \
     $(DEST)/printhostmask.o \
     $(DEST)/printlog.o \
     $(DEST)/printmask.o \
     $(DEST)/printnat.o \
     $(DEST)/printportcmp.o \
     $(DEST)/printpacket.o \
     $(DEST)/printpacket6.o \
     $(DEST)/printsbuf.o \
     $(DEST)/printstate.o \
     $(DEST)/printtunable.o \
     $(DEST)/ratoi.o \
     $(DEST)/ratoui.o \
     $(DEST)/remove_hash.o \
     $(DEST)/remove_hashnode.o \
     $(DEST)/remove_pool.o \
     $(DEST)/remove_poolnode.o \
     $(DEST)/resetlexer.o \
     $(DEST)/rwlock_emul.o \
     $(DEST)/tcpflags.o \
     $(DEST)/tcp_flags.o \
     $(DEST)/to_interface.o \
     $(DEST)/var.o \
     $(DEST)/verbose.o \
     $(DEST)/v6ionames.o \
     $(DEST)/v6optvalue.o

$(DEST)/libipf.a: $(LIBOBJS)
	/bin/rm -f $@
	ar $(AROPTS) $@ $(LIBOBJS)
	$(RANLIB) $@

$(DEST)/addicmp.o: $(LIBSRC)/addicmp.c $(INCDEP)
	$(CC) $(CCARGS) -c $(LIBSRC)/addicmp.c -o $@
$(DEST)/addipopt.o: $(LIBSRC)/addipopt.c $(INCDEP)
	$(CC) $(CCARGS) -c $(LIBSRC)/addipopt.c -o $@
$(DEST)/bcopywrap.o: $(LIBSRC)/bcopywrap.c $(INCDEP)
	$(CC) $(CCARGS) -c $(LIBSRC)/bcopywrap.c -o $@
$(DEST)/binprint.o: $(LIBSRC)/binprint.c $(INCDEP)
	$(CC) $(CCARGS) -c $(LIBSRC)/binprint.c -o $@
$(DEST)/buildopts.o: $(LIBSRC)/buildopts.c $(INCDEP)
	$(CC) $(CCARGS) -c $(LIBSRC)/buildopts.c -o $@
$(DEST)/count6bits.o: $(LIBSRC)/count6bits.c $(INCDEP)
	$(CC) $(CCARGS) -c $(LIBSRC)/count6bits.c -o $@
$(DEST)/checkrev.o: $(LIBSRC)/checkrev.c $(INCDEP) $(TOP)/ipl.h
	$(CC) $(CCARGS) -c $(LIBSRC)/checkrev.c -o $@
$(DEST)/count4bits.o: $(LIBSRC)/count4bits.c $(INCDEP)
	$(CC) $(CCARGS) -c $(LIBSRC)/count4bits.c -o $@
$(DEST)/debug.o: $(LIBSRC)/debug.c $(INCDEP)
	$(CC) $(CCARGS) -c $(LIBSRC)/debug.c -o $@
$(DEST)/extras.o: $(LIBSRC)/extras.c $(INCDEP)
	$(CC) $(CCARGS) -c $(LIBSRC)/extras.c -o $@
$(DEST)/facpri.o: $(LIBSRC)/facpri.c $(INCDEP)
	$(CC) $(CCARGS) -c $(LIBSRC)/facpri.c -o $@
$(DEST)/fill6bits.o: $(LIBSRC)/fill6bits.c $(INCDEP)
	$(CC) $(CCARGS) -c $(LIBSRC)/fill6bits.c -o $@
$(DEST)/flags.o: $(LIBSRC)/flags.c $(INCDEP)
	$(CC) $(CCARGS) -c $(LIBSRC)/flags.c -o $@
$(DEST)/genmask.o: $(LIBSRC)/genmask.c $(INCDEP)
	$(CC) $(CCARGS) -c $(LIBSRC)/genmask.c -o $@
$(DEST)/getline.o: $(LIBSRC)/getline.c $(INCDEP)
	$(CC) $(CCARGS) -c $(LIBSRC)/getline.c -o $@
$(DEST)/gethost.o: $(LIBSRC)/gethost.c $(INCDEP)
	$(CC) $(CCARGS) -c $(LIBSRC)/gethost.c -o $@
$(DEST)/getifname.o: $(LIBSRC)/getifname.c $(INCDEP)
	$(CC) $(CCARGS) -c $(LIBSRC)/getifname.c -o $@
$(DEST)/getnattype.o: $(LIBSRC)/getnattype.c $(INCDEP)
	$(CC) $(CCARGS) -c $(LIBSRC)/getnattype.c -o $@
$(DEST)/getport.o: $(LIBSRC)/getport.c $(INCDEP)
	$(CC) $(CCARGS) -c $(LIBSRC)/getport.c -o $@
$(DEST)/getportproto.o: $(LIBSRC)/getportproto.c $(INCDEP)
	$(CC) $(CCARGS) -c $(LIBSRC)/getportproto.c -o $@
$(DEST)/getproto.o: $(LIBSRC)/getproto.c $(INCDEP)
	$(CC) $(CCARGS) -c $(LIBSRC)/getproto.c -o $@
$(DEST)/getsumd.o: $(LIBSRC)/getsumd.c $(INCDEP)
	$(CC) $(CCARGS) -c $(LIBSRC)/getsumd.c -o $@
$(DEST)/hexdump.o: $(LIBSRC)/hexdump.c $(INCDEP)
	$(CC) $(CCARGS) -c $(LIBSRC)/hexdump.c -o $@
$(DEST)/hostmask.o: $(LIBSRC)/hostmask.c $(INCDEP)
	$(CC) $(CCARGS) -c $(LIBSRC)/hostmask.c -o $@
$(DEST)/hostname.o: $(LIBSRC)/hostname.c $(INCDEP)
	$(CC) $(CCARGS) -c $(LIBSRC)/hostname.c -o $@
$(DEST)/hostnum.o: $(LIBSRC)/hostnum.c $(INCDEP)
	$(CC) $(CCARGS) -c $(LIBSRC)/hostnum.c -o $@
$(DEST)/icmpcode.o: $(LIBSRC)/icmpcode.c $(INCDEP)
	$(CC) $(CCARGS) -c $(LIBSRC)/icmpcode.c -o $@
$(DEST)/ipoptsec.o: $(LIBSRC)/ipoptsec.c $(INCDEP)
	$(CC) $(CCARGS) -c $(LIBSRC)/ipoptsec.c -o $@
$(DEST)/inet_addr.o: $(LIBSRC)/inet_addr.c $(INCDEP)
	$(CC) $(CCARGS) -c $(LIBSRC)/inet_addr.c -o $@
$(DEST)/initparse.o: $(LIBSRC)/initparse.c $(INCDEP)
	$(CC) $(CCARGS) -c $(LIBSRC)/initparse.c -o $@
$(DEST)/ionames.o: $(LIBSRC)/ionames.c $(INCDEP)
	$(CC) $(CCARGS) -c $(LIBSRC)/ionames.c -o $@
$(DEST)/ipf_dotuning.o: $(LIBSRC)/ipf_dotuning.c $(INCDEP)
	$(CC) $(CCARGS) -c $(LIBSRC)/ipf_dotuning.c -o $@
$(DEST)/ipft_ef.o: $(LIBSRC)/ipft_ef.c $(INCDEP)
	$(CC) $(CCARGS) -c $(LIBSRC)/ipft_ef.c -o $@
$(DEST)/ipft_hx.o: $(LIBSRC)/ipft_hx.c $(INCDEP)
	$(CC) $(CCARGS) -c $(LIBSRC)/ipft_hx.c -o $@
$(DEST)/ipft_pc.o: $(LIBSRC)/ipft_pc.c $(INCDEP)
	$(CC) $(CCARGS) -c $(LIBSRC)/ipft_pc.c -o $@
$(DEST)/ipft_sn.o: $(LIBSRC)/ipft_sn.c $(TOP)/snoop.h
	$(CC) $(CCARGS) -c $(LIBSRC)/ipft_sn.c -o $@
$(DEST)/ipft_td.o: $(LIBSRC)/ipft_td.c $(INCDEP)
	$(CC) $(CCARGS) -c $(LIBSRC)/ipft_td.c -o $@
$(DEST)/ipft_tx.o: $(LIBSRC)/ipft_tx.c $(INCDEP)
	$(CC) $(CCARGS) -c $(LIBSRC)/ipft_tx.c -o $@
$(DEST)/kmem.o: $(LIBSRC)/kmem.c $(INCDEP)
	$(CC) $(CCARGS) -c $(LIBSRC)/kmem.c -o $@
$(DEST)/kmemcpywrap.o: $(LIBSRC)/kmemcpywrap.c $(INCDEP)
	$(CC) $(CCARGS) -c $(LIBSRC)/kmemcpywrap.c -o $@
$(DEST)/kvatoname.o: $(LIBSRC)/kvatoname.c $(INCDEP)
	$(CC) $(CCARGS) -c $(LIBSRC)/kvatoname.c -o $@
$(DEST)/load_hash.o: $(LIBSRC)/load_hash.c $(INCDEP) $(TOP)/ip_htable.h
	$(CC) $(CCARGS) -c $(LIBSRC)/load_hash.c -o $@
$(DEST)/load_hashnode.o: $(LIBSRC)/load_hashnode.c $(INCDEP) $(TOP)/ip_htable.h
	$(CC) $(CCARGS) -c $(LIBSRC)/load_hashnode.c -o $@
$(DEST)/load_pool.o: $(LIBSRC)/load_pool.c $(INCDEP) $(TOP)/ip_pool.h
	$(CC) $(CCARGS) -c $(LIBSRC)/load_pool.c -o $@
$(DEST)/load_poolnode.o: $(LIBSRC)/load_poolnode.c $(INCDEP) $(TOP)/ip_pool.h
	$(CC) $(CCARGS) -c $(LIBSRC)/load_poolnode.c -o $@
$(DEST)/make_range.o: $(LIBSRC)/make_range.c $(INCDEP)
	$(CC) $(CCARGS) -c $(LIBSRC)/make_range.c -o $@
$(DEST)/mutex_emul.o: $(LIBSRC)/mutex_emul.c $(INCDEP)
	$(CC) $(CCARGS) -c $(LIBSRC)/mutex_emul.c -o $@
$(DEST)/nametokva.o: $(LIBSRC)/nametokva.c $(INCDEP)
	$(CC) $(CCARGS) -c $(LIBSRC)/nametokva.c -o $@
$(DEST)/nat_setgroupmap.o: $(LIBSRC)/nat_setgroupmap.c $(TOP)/ip_compat.h \
    $(TOP)/ipf.h $(TOP)/ip_nat.h
	$(CC) $(CCARGS) -c $(LIBSRC)/nat_setgroupmap.c -o $@
$(DEST)/ntomask.o: $(LIBSRC)/ntomask.c $(TOP)/ip_compat.h
	$(CC) $(CCARGS) -c $(LIBSRC)/ntomask.c -o $@
$(DEST)/loglevel.o: $(LIBSRC)/loglevel.c $(INCDEP)
	$(CC) $(CCARGS) -c $(LIBSRC)/loglevel.c -o $@
$(DEST)/optname.o: $(LIBSRC)/optname.c $(INCDEP)
	$(CC) $(CCARGS) -c $(LIBSRC)/optname.c -o $@
$(DEST)/optprint.o: $(LIBSRC)/optprint.c $(INCDEP)
	$(CC) $(CCARGS) -c $(LIBSRC)/optprint.c -o $@
$(DEST)/optprintv6.o: $(LIBSRC)/optprintv6.c $(INCDEP)
	$(CC) $(CCARGS) -c $(LIBSRC)/optprintv6.c -o $@
$(DEST)/optvalue.o: $(LIBSRC)/optvalue.c $(INCDEP)
	$(CC) $(CCARGS) -c $(LIBSRC)/optvalue.c -o $@
$(DEST)/portname.o: $(LIBSRC)/portname.c $(INCDEP)
	$(CC) $(CCARGS) -c $(LIBSRC)/portname.c -o $@
$(DEST)/portnum.o: $(LIBSRC)/portnum.c $(INCDEP)
	$(CC) $(CCARGS) -c $(LIBSRC)/portnum.c -o $@
$(DEST)/ports.o: $(LIBSRC)/ports.c $(INCDEP)
	$(CC) $(CCARGS) -c $(LIBSRC)/ports.c -o $@
$(DEST)/print_toif.o: $(LIBSRC)/print_toif.c $(INCDEP)
	$(CC) $(CCARGS) -c $(LIBSRC)/print_toif.c -o $@
$(DEST)/printactivenat.o: $(LIBSRC)/printactivenat.c $(INCDEP)
	$(CC) $(CCARGS) -c $(LIBSRC)/printactivenat.c -o $@
$(DEST)/printaps.o: $(LIBSRC)/printaps.c $(INCDEP)
	$(CC) $(CCARGS) -c $(LIBSRC)/printaps.c -o $@
$(DEST)/printbuf.o: $(LIBSRC)/printbuf.c $(INCDEP)
	$(CC) $(CCARGS) -c $(LIBSRC)/printbuf.c -o $@
$(DEST)/printfr.o: $(LIBSRC)/printfr.c $(TOP)/ip_fil.h
	$(CC) $(CCARGS) -c $(LIBSRC)/printfr.c -o $@
$(DEST)/printfraginfo.o: $(LIBSRC)/printfraginfo.c $(TOP)/ip_fil.h
	$(CC) $(CCARGS) -c $(LIBSRC)/printfraginfo.c -o $@
$(DEST)/printhash.o: $(LIBSRC)/printhash.c $(TOP)/ip_fil.h $(TOP)/ip_htable.h
	$(CC) $(CCARGS) -c $(LIBSRC)/printhash.c -o $@
$(DEST)/printhashnode.o: $(LIBSRC)/printhashnode.c $(TOP)/ip_fil.h \
    $(TOP)/ip_htable.h $(TOP)/ip_lookup.h
	$(CC) $(CCARGS) -c $(LIBSRC)/printhashnode.c -o $@
$(DEST)/printip.o: $(LIBSRC)/printip.c $(TOP)/ip_fil.h
	$(CC) $(CCARGS) -c $(LIBSRC)/printip.c -o $@
$(DEST)/printpool.o: $(LIBSRC)/printpool.c $(TOP)/ip_fil.h $(TOP)/ip_pool.h
	$(CC) $(CCARGS) -c $(LIBSRC)/printpool.c -o $@
$(DEST)/printpoolnode.o: $(LIBSRC)/printpoolnode.c $(TOP)/ip_fil.h \
    $(TOP)/ip_pool.h $(TOP)/ip_lookup.h
	$(CC) $(CCARGS) -c $(LIBSRC)/printpoolnode.c -o $@
$(DEST)/printproto.o: $(LIBSRC)/printproto.c $(TOP)/ip_fil.h
	$(CC) $(CCARGS) -c $(LIBSRC)/printproto.c -o $@
$(DEST)/printhostmap.o: $(LIBSRC)/printhostmap.c $(TOP)/ip_fil.h
	$(CC) $(CCARGS) -c $(LIBSRC)/printhostmap.c -o $@
$(DEST)/printifname.o: $(LIBSRC)/printifname.c $(INCDEP)
	$(CC) $(CCARGS) -c $(LIBSRC)/printifname.c -o $@
$(DEST)/printmask.o: $(LIBSRC)/printmask.c $(INCDEP)
	$(CC) $(CCARGS) -c $(LIBSRC)/printmask.c -o $@
$(DEST)/printnat.o: $(LIBSRC)/printnat.c $(INCDEP)
	$(CC) $(CCARGS) -c $(LIBSRC)/printnat.c -o $@
$(DEST)/printhostmask.o: $(LIBSRC)/printhostmask.c $(INCDEP)
	$(CC) $(CCARGS) -c $(LIBSRC)/printhostmask.c -o $@
$(DEST)/printlog.o: $(LIBSRC)/printlog.c $(INCDEP)
	$(CC) $(CCARGS) -c $(LIBSRC)/printlog.c -o $@
$(DEST)/printpacket.o: $(LIBSRC)/printpacket.c $(INCDEP)
	$(CC) $(CCARGS) -c $(LIBSRC)/printpacket.c -o $@
$(DEST)/printpacket6.o: $(LIBSRC)/printpacket6.c $(INCDEP)
	$(CC) $(CCARGS) -c $(LIBSRC)/printpacket6.c -o $@
$(DEST)/printportcmp.o: $(LIBSRC)/printportcmp.c $(INCDEP)
	$(CC) $(CCARGS) -c $(LIBSRC)/printportcmp.c -o $@
$(DEST)/printsbuf.o: $(LIBSRC)/printsbuf.c $(INCDEP)
	$(CC) $(CCARGS) -c $(LIBSRC)/printsbuf.c -o $@
$(DEST)/printstate.o: $(LIBSRC)/printstate.c $(INCDEP) $(TOP)/ip_state.h
	$(CC) $(CCARGS) -c $(LIBSRC)/printstate.c -o $@
$(DEST)/printtunable.o: $(LIBSRC)/printtunable.c $(INCDEP)
	$(CC) $(CCARGS) -c $(LIBSRC)/printtunable.c -o $@
$(DEST)/ratoi.o: $(LIBSRC)/ratoi.c $(INCDEP)
	$(CC) $(CCARGS) -c $(LIBSRC)/ratoi.c -o $@
$(DEST)/ratoui.o: $(LIBSRC)/ratoui.c $(INCDEP)
	$(CC) $(CCARGS) -c $(LIBSRC)/ratoui.c -o $@
$(DEST)/remove_hash.o: $(LIBSRC)/remove_hash.c $(INCDEP) \
    $(TOP)/ip_htable.h
	$(CC) $(CCARGS) -c $(LIBSRC)/remove_hash.c -o $@
$(DEST)/remove_hashnode.o: $(LIBSRC)/remove_hashnode.c $(INCDEP) \
    $(TOP)/ip_htable.h
	$(CC) $(CCARGS) -c $(LIBSRC)/remove_hashnode.c -o $@
$(DEST)/remove_pool.o: $(LIBSRC)/remove_pool.c $(INCDEP) \
    $(TOP)/ip_htable.h
	$(CC) $(CCARGS) -c $(LIBSRC)/remove_pool.c -o $@
$(DEST)/remove_poolnode.o: $(LIBSRC)/remove_poolnode.c $(INCDEP) \
    $(TOP)/ip_htable.h
	$(CC) $(CCARGS) -c $(LIBSRC)/remove_poolnode.c -o $@
$(DEST)/resetlexer.o: $(LIBSRC)/resetlexer.c $(INCDEP)
	$(CC) $(CCARGS) -c $(LIBSRC)/resetlexer.c -o $@
$(DEST)/rwlock_emul.o: $(LIBSRC)/rwlock_emul.c $(INCDEP)
	$(CC) $(CCARGS) -c $(LIBSRC)/rwlock_emul.c -o $@
$(DEST)/to_interface.o: $(LIBSRC)/to_interface.c $(INCDEP)
	$(CC) $(CCARGS) -c $(LIBSRC)/to_interface.c -o $@
$(DEST)/tcpflags.o: $(LIBSRC)/tcpflags.c $(INCDEP)
	$(CC) $(CCARGS) -c $(LIBSRC)/tcpflags.c -o $@
$(DEST)/tcp_flags.o: $(LIBSRC)/tcp_flags.c $(INCDEP)
	$(CC) $(CCARGS) -c $(LIBSRC)/tcp_flags.c -o $@
$(DEST)/var.o: $(LIBSRC)/var.c $(INCDEP)
	$(CC) $(CCARGS) -c $(LIBSRC)/var.c -o $@
$(DEST)/verbose.o: $(LIBSRC)/verbose.c $(INCDEP)
	$(CC) $(CCARGS) -c $(LIBSRC)/verbose.c -o $@
$(DEST)/v6ionames.o: $(LIBSRC)/v6ionames.c $(INCDEP)
	$(CC) $(CCARGS) -c $(LIBSRC)/v6ionames.c -o $@
$(DEST)/v6optvalue.o: $(LIBSRC)/v6optvalue.c $(INCDEP)
	$(CC) $(CCARGS) -c $(LIBSRC)/v6optvalue.c -o $@

clean-lib:
	/bin/rm -f ${LIBOBJS} ${LIB}
