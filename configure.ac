AC_INIT([hildon-im-vkbrenderer3], [3.3.22])

libhildon_im_vkbrenderer_lt_current=3
libhildon_im_vkbrenderer_lt_rev=3
libhildon_im_vkbrenderer_lt_age=0

AC_SUBST([LIBHILDONIMVKBRENDERER_LT_VERSION],[$libhildon_im_vkbrenderer_lt_current:$libhildon_im_vkbrenderer_lt_rev:$libhildon_im_vkbrenderer_lt_age])
AC_SUBST([LIBHILDONIMVKBRENDERER_LIB_VERSION],[$libhildon_im_vkbrenderer_lt_current.$libhildon_im_vkbrenderer_lt_age.$libhildon_im_vkbrenderer_lt_rev])

AM_INIT_AUTOMAKE

AM_CONFIG_HEADER(config.h)

# Specify dir for local macros
AC_CONFIG_MACRO_DIR([m4])
AC_DISABLE_STATIC

AC_ISC_POSIX
AC_PROG_CC
AC_PROG_MAKE_SET
AM_PROG_CC_STDC
AC_PROG_LIBTOOL
AC_HEADER_STDC
AC_PROG_LN_S
AC_PROG_MKDIR_P

PKG_CHECK_VAR([GLIB_GENMARSHAL], [glib-2.0], [glib_genmarshal])

PKG_CHECK_MODULES(DEPS, gtk+-2.0 >= 2.4.0 gconf-2.0 > 2.0 glib-2.0 >= 2.20.3 x11 imlayouts)
AC_SUBST(DEPS_CFLAGS)
AC_SUBST(DEPS_LIBS)

PKG_CHECK_MODULES(HIM, hildon-input-method-ui-3.0)

himwidgetsdir="`$PKG_CONFIG --variable=himwidgetsdir hildon-input-method-ui-3.0`"
AC_SUBST(himwidgetsdir)

AC_OUTPUT([ Makefile src/Makefile hildon-im-vkbrenderer3.pc])

