if test -n "$VXWORKS_BASE_EM_FILE" ; then
. "${srcdir}/emultempl/${VXWORKS_BASE_EM_FILE}.em"
fi

cat >>e${EMULATION_NAME}.c <<EOF

static int force_dynamic;

static void
vxworks_before_parse (void)
{
  ${LDEMUL_BEFORE_PARSE-gld${EMULATION_NAME}_before_parse} ();
  config.rpath_separator = ';';
}

static void
vxworks_after_open (void)
{
  ${LDEMUL_AFTER_OPEN-gld${EMULATION_NAME}_after_open} ();

  if (force_dynamic
      && link_info.input_bfds
      && output_bfd->xvec->flavour == bfd_target_elf_flavour
      && !_bfd_elf_link_create_dynamic_sections (link_info.input_bfds,
						 &link_info))
    einfo ("%X%P: Cannot create dynamic sections %E\n");

  if (!force_dynamic
      && !link_info.shared
      && output_bfd->xvec->flavour == bfd_target_elf_flavour
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

# Hook in our routines above.  There are three possibilities:
#
#   (1) VXWORKS_BASE_EM_FILE did not set the hook's LDEMUL_FOO variable.
#	We want to define LDEMUL_FOO to vxworks_foo in that case,
#
#   (2) VXWORKS_BASE_EM_FILE set the hook's LDEMUL_FOO variable to
#	gld${EMULATION_NAME}_foo.  This means that the file has
#	replaced elf32.em's default definition, so we simply #define
#	the current value of LDEMUL_FOO to vxworks_foo.
#
#   (3) VXWORKS_BASE_EM_FILE set the hook's LDEMUL_FOO variable to
#	something other than gld${EMULATION_NAME}_foo.  We handle
#	this case in the same way as (1).
for override in before_parse after_open; do
  var="LDEMUL_`echo ${override} | tr a-z A-Z`"
  eval value=\$${var}
  if test "${value}" = "gld${EMULATION_NAME}_${override}"; then
    cat >>e${EMULATION_NAME}.c <<EOF
#define ${value} vxworks_${override}
EOF
  else
    eval $var=vxworks_${override}
  fi
done
