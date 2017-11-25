plugindir = $(libdir)/xfce4/panel/plugins
plugin_LTLIBRARIES = libappletnetspeed.la

LIBS = @LIBS@ @SOLLIBS@

libappletnetspeed_la_SOURCES =							\
	netspeed.c							\
	utils.c								\
	utils.h								\
	net.h								\
	net.c

libappletnetspeed_la_CFLAGS =							\
	-DPACKAGE_LOCALE_DIR=\"$(localedir)\"				\
	@LIBXFCE4PANEL_CFLAGS@						\
	@LIBXFCE4UI_CFLAGS@

libappletnetspeed_la_LDFLAGS =							\
	-avoid-version							\
	-module								\
	-no-undefined							\
	-export-symbols-regex '^xfce_panel_module_(preinit|init|construct)' \
	$(PLATFORM_LDFLAGS)

libappletnetspeed_la_LIBADD =							\
	@SOLLIBS@							\
	@LIBXFCE4PANEL_LIBS@						\
	@LIBXFCE4UI_LIBS@

# .desktop file
#
desktop_in_files = applet-netspeed.desktop.in

desktopdir = $(datadir)/xfce4/panel/plugins
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

DISTCLEANFILES =							\
	$(desktop_DATA) $(desktop_in_files)

# get full path into .desktop file
%.desktop: %.desktop.in
	cp $< $@

