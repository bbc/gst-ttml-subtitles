plugin_LTLIBRARIES = libgstttmlparse.la

# sources used to compile this plug-in
libgstttmlparse_la_SOURCES = \
	gstttmlparse.c \
	gstttmlparse.h \
	samiparse.c \
	samiparse.h \
	tmplayerparse.c \
	tmplayerparse.h \
	mpl2parse.c \
	mpl2parse.h \
	qttextparse.c \
	qttextparse.h \
	ttmlparse.c \
	ttmlparse.h


# compiler and linker flags used to compile this plugin, set in configure.ac
libgstttmlparse_la_CFLAGS = \
	-I $(top_builddir)/libs/ \
	$(GST_CFLAGS) \
	$(LIBXML2_CFLAGS)
libgstttmlparse_la_LIBADD = \
	$(top_builddir)/libs/gst/subtitle/libgstsubtitle-$(GST_API_VERSION).la \
	$(GST_LIBS) \
	$(LIBXML2_LIBS) -lgstsubtitle-@GST_API_VERSION@
libgstttmlparse_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS) $(LIBXML2_LDFLAGS)
libgstttmlparse_la_LIBTOOLFLAGS = $(GST_PLUGIN_LIBTOOLFLAGS)

# headers we need but don't want installed
noinst_HEADERS = \
	gstttmlparse.h \
	samiparse.h \
	tmplayerparse.h \
	mpl2parse.h \
	qttextparse.h \
	ttmlparse.h

