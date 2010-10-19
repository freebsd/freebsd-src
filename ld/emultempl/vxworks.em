if test -n "$VXWORKS_BASE_EM_FILE" ; then
. "${srcdir}/emultempl/${VXWORKS_BASE_EM_FILE}.em"
fi

cat >>e${EMULATION_NAME}.c <<EOF

static int force_dynamic;

static void
vxworks_after_open (void)
{
  ${LDEMUL_AFTER_OPEN-gld${EMULATION_NAME}_after_open} ();

  if (force_dynamic
      && link_info.input_bfds
      && !_bfd_elf_link_create_dynamic_sections (link_info.input_bfds,
						 &link_info))
    einfo ("%X%P: Cannot create dynamic sections %E\n");

  if (!force_dynamic
      && !link_info.shared
      && elf_hash_table (&link_info)->dynamic_sections_created)
    einfo ("%X%P: Dynamic sections created in non-dynamic link\n");
}

EOF

PARSE_AND_LIST_PROLOGUE=$PARSE_AND_LIST_PROLOGUE'
enum {
  OPTION_FORCE_DYNAMIC = 501
};
'

PARSE_AND_LIST_LONGOPTS=$PARSE_AND_LIST_LONGOPTS'
  {"force-dynamic", no_argument, NULL, OPTION_FORCE_DYNAMIC},
'

PARSE_AND_LIST_OPTIONS=$PARSE_AND_LIST_OPTIONS'
  fprintf (file, _("\
  --force-dynamic       Always create dynamic sections\n"));
'

PARSE_AND_LIST_ARGS_CASES=$PARSE_AND_LIST_ARGS_CASES'
    case OPTION_FORCE_DYNAMIC:
      force_dynamic = 1;
      break;
'

LDEMUL_AFTER_OPEN=vxworks_after_open
