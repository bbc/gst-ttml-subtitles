plugin_LTLIBRARIES = libgstebuttdparse.la

# sources used to compile this plug-in
libgstebuttdparse_la_SOURCES = \
	gstsubparse.c \
	gstsubparse.h \
	samiparse.c \
	samiparse.h \
	tmplayerparse.c \
	tmplayerparse.h \
	mpl2parse.c \
	mpl2parse.h \
	qttextparse.c \
	qttextparse.h \
	ebuttxmlparse.c \
	ebuttxmlparse.h


# compiler and linker flags used to compile this plugin, set in configure.ac
libgstebuttdparse_la_CFLAGS = $(GST_CFLAGS) $(LIBXML2_CFLAGS)
libgstebuttdparse_la_LIBADD = $(GST_LIBS) $(LIBXML2_LIBS)
libgstebuttdparse_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS) $(LIBXML2_LDFLAGS)
libgstebuttdparse_la_LIBTOOLFLAGS = $(GST_PLUGIN_LIBTOOLFLAGS)

# headers we need but don't want installed
noinst_HEADERS = \
	samiparse.h \
	tmplayerparse.h \
	mpl2parse.h \
	qttextparse.h \
	ebuttxmlparse.h

