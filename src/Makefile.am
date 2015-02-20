plugin_LTLIBRARIES = libgstebuttdrender.la

# sources used to compile this plug-in
libgstebuttdrender_la_SOURCES = \
	gstbasetextoverlay.c \
	gsttextoverlay.c


# compiler and linker flags used to compile this ebuttdrender, set in configure.ac
libgstebuttdrender_la_CFLAGS = $(GST_CFLAGS) $(PANGO_CFLAGS) $(CAIRO_CFLAGS) $(PANGOCAIRO_CFLAGS)
libgstebuttdrender_la_LIBADD = $(GST_LIBS) $(PANGO_LIBS) $(CAIRO_LIBS) $(PANGOCAIRO_LIBS) -lgstvideo-@GST_API_VERSION@
libgstebuttdrender_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
libgstebuttdrender_la_LIBTOOLFLAGS = $(GST_PLUGIN_LIBTOOLFLAGS)

# headers we need but don't want installed
noinst_HEADERS = \
	gstbasetextoverlay.h \
	gsttextoverlay.h

