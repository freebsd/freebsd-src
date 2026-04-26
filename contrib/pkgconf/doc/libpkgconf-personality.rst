
libpkgconf `personality` module
=========================

.. c:function:: const pkgconf_cross_personality_t *pkgconf_cross_personality_default(void)

   Returns the default cross-compile personality.

   Not thread safe.

   :rtype: pkgconf_cross_personality_t*
   :return: the default cross-compile personality

.. c:function:: void pkgconf_cross_personality_deinit(pkgconf_cross_personality_t *)

   Decrements the count of default cross personality instances.

   Not thread safe.

   :rtype: void

.. c:function:: pkgconf_cross_personality_t *pkgconf_cross_personality_find(const char *triplet)

   Attempts to find a cross-compile personality given a triplet.

   :rtype: pkgconf_cross_personality_t*
   :return: the default cross-compile personality
