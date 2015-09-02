plugin_LTLIBRARIES = libgstttmlrender.la

# sources used to compile this plug-in
libgstttmlrender_la_SOURCES = \
	gstbasetextoverlay.c \
	gsttextoverlay.c


# compiler and linker flags used to compile this plugin, set in configure.ac
libgstttmlrender_la_CFLAGS = \
	-I $(top_builddir)/libs/ \
	$(GST_CFLAGS) \
	$(PANGO_CFLAGS) \
	$(CAIRO_CFLAGS) \
	$(PANGOCAIRO_CFLAGS)

libgstttmlrender_la_LIBADD = \
	$(top_builddir)/libs/gst/subtitle/libgstsubtitle-$(GST_API_VERSION).la \
	$(GST_LIBS) \
	$(PANGO_LIBS) \
	$(CAIRO_LIBS) \
	$(PANGOCAIRO_LIBS) \
	-lgstvideo-@GST_API_VERSION@

libgstttmlrender_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
libgstttmlrender_la_LIBTOOLFLAGS = $(GST_PLUGIN_LIBTOOLFLAGS)

# headers we need but don't want installed
noinst_HEADERS = \
	gstbasetextoverlay.h \
	gsttextoverlay.h

