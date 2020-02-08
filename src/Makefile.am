AM_CPPFLAGS = $(DEPS_CFLAGS)

lib_LTLIBRARIES = libhildon-im-vkbrenderer.la

libhildon_im_vkbrenderer_la_SOURCES = \
	hildon-im-vkbrenderer.c \
	hildon-vkb-renderer-marshal.c

libhildon_im_vkbrenderer_la_CFLAGS = \
	$(HILDONIMVKBRENDERER_CFLAGS)
libhildon_im_vkbrenderer_la_LDFLAGS = \
	-Wl,--as-needed -module \
	-version-info $(LIBHILDONIMVKBRENDERER_LT_VERSION)
libhildon_im_vkbrenderer_la_LIBADD = \
	$(DEPS_LIBS)

hildon-vkb-renderer-marshal.h: hildon-vkb-renderer-marshal.list
	$(AM_V_GEN)$(GLIB_GENMARSHAL) --header --prefix=hildon_vkb_renderer_marshal \
	$< > $@

hildon-vkb-renderer-marshal.c: hildon-vkb-renderer-marshal.list
	(cd $(srcdir) && echo "#include \"config.h\"" > $@) && \
	$(AM_V_GEN)$(GLIB_GENMARSHAL) --body --prefix=hildon_vkb_renderer_marshal \
	$< >> $@

BUILT_SOURCES = \
	hildon-vkb-renderer-marshal.h \
	hildon-vkb-renderer-marshal.c

install-exec-hook:
	$(AM_V_at)$(MKDIR_P) $(DESTDIR)/$(himwidgetsdir)
	$(AM_V_at)$(LN_S) \
		$(libdir)/libhildon-im-vkbrenderer.so.$(LIBHILDONIMVKBRENDERER_LIB_VERSION) \
		$(DESTDIR)/$(himwidgetsdir)/vkbrenderer.so

CLEANFILES = $(BUILT_SOURCES)

MAINTAINERCLEANFILES = Makefile.in
