## Process this file with automake to produce Makefile.in

libgimpui = $(top_builddir)/libgimp/libgimpui-$(GIMP_API_VERSION).la
libgimpconfig = $(top_builddir)/libgimpconfig/libgimpconfig-$(GIMP_API_VERSION).la
libgimpwidgets = $(top_builddir)/libgimpwidgets/libgimpwidgets-$(GIMP_API_VERSION).la
libgimp = $(top_builddir)/libgimp/libgimp-$(GIMP_API_VERSION).la
libgimpcolor = $(top_builddir)/libgimpcolor/libgimpcolor-$(GIMP_API_VERSION).la
libgimpbase = $(top_builddir)/libgimpbase/libgimpbase-$(GIMP_API_VERSION).la
libgimpmath = $(top_builddir)/libgimpmath/libgimpmath-$(GIMP_API_VERSION).la

if OS_WIN32
mwindows = -mwindows
endif

if HAVE_WINDRES
include $(top_srcdir)/build/windows/gimprc-plug-ins.rule
plugin_pixel_art_scalers_RC = plugin_pixel_art_scalers.rc.o
endif

AM_LDFLAGS = $(mwindows)

libexecdir = $(gimpplugindir)/plug-ins

libexec_PROGRAMS = plugin_pixel_art_scalers

plugin_pixel_art_scalers_SOURCES = \
	gtk2/filter_pixel_art_scalers.c \
	gtk2/filter_dialog.c \
	gtk2/filter_scalers.c \
	gtk2/filter_utils.c \
	lib/hq3x.c \
	lib/hqx_init.c \
	lib/hq2x.c \
	lib/hq4x.c \
	lib/xbr.c \
	lib/scaler_scalex.c \
	lib/scaler_nearestneighbor.c






INCLUDES = \
	-I$(top_srcdir)	\
	$(GTK_CFLAGS)	\
	-I$(includedir)

LDADD = \
	$(libgimpui)		\
	$(libgimpwidgets)	\
	$(libgimpconfig)	\
	$(libgimp)		\
	$(libgimpcolor)		\
	$(libgimpmath)		\
	$(libgimpbase)		\
	$(GTK_LIBS)		\
	$(RT_LIBS)		\
	$(INTLLIBS)		\
	$(plugin_pixel_art_scalers_RC)
