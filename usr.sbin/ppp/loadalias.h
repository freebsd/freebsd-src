struct aliasHandlers {
    char *(*GetNextFragmentPtr)(char *);
    void (*InitPacketAlias)();
    int (*PacketAliasIn)(char *,int);
    int (*PacketAliasOut)(char *,int);
    struct alias_link *(*PacketAliasRedirectAddr)
        (struct in_addr, struct in_addr);
    struct alias_link *(*PacketAliasRedirectPort)
        (struct in_addr, u_short, struct in_addr, u_short,
         struct in_addr, u_short, u_char);
    int (*SaveFragmentPtr)(char *);
    void (*SetPacketAliasAddress)(struct in_addr);
    unsigned (*SetPacketAliasMode)(unsigned, unsigned);
    void (*FragmentAliasIn)(char *, char *);
};

extern int loadAliasHandlers(struct aliasHandlers *);
extern void unloadAliasHandlers();
