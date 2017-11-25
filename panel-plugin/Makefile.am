plugindir = $(libdir)/xfce4/panel/plugins
plugin_LTLIBRARIES = libappletdiskspeed.la

LIBS = @LIBS@ @SOLLIBS@

libappletdiskspeed_la_SOURCES =							\
	diskspeed.c							\
	utils.c								\
	utils.h								\
	disk.h								\
	disk.c

libappletdiskspeed_la_CFLAGS =							\
	-DPACKAGE_LOCALE_DIR=\"$(localedir)\"				\
	@LIBXFCE4PANEL_CFLAGS@						\
	@LIBXFCE4UI_CFLAGS@

libappletdiskspeed_la_LDFLAGS =							\
	-avoid-version							\
	-module								\
	-no-undefined							\
	-export-symbols-regex '^xfce_panel_module_(preinit|init|construct)' \
	$(PLATFORM_LDFLAGS)

libappletdiskspeed_la_LIBADD =							\
	@SOLLIBS@							\
	@LIBXFCE4PANEL_LIBS@						\
	@LIBXFCE4UI_LIBS@

# .desktop file
#
desktop_in_files = applet-diskspeed.desktop.in

desktopdir = $(datadir)/xfce4/panel/plugins
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

DISTCLEANFILES =							\
	$(desktop_DATA) $(desktop_in_files)

# get full path into .desktop file
%.desktop: %.desktop.in
	cp $< $@

