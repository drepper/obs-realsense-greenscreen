VERSION = 1.2

PROJECT = obs-realsense.so
SRCDIR = .

DISTCC =
CXX = $(DISTCC) g++
CXXSTD = -std=gnu++2b
ifeq (,$(findstring clang,$(CC)$(CXX)))
is_clang = no
else
is_clang = yes
endif

PACKAGES = realsense2
PACKAGES-testrealsense.o = gtkmm-3.0
ifeq ($(PACKAGES),)
PKGCONFIG = true
else
PKGCONFIG = pkg-config
endif
MV_F = mv -f
LN_FS = ln -fs
INSTALL = install
RPMBUILD = rpmbuild
SED = sed
TAR = tar
GAWK = gawk

ALT_LIBREALSENSE =

CPPFLAGS = -I$(SRCDIR) $(OTHERINCLUDES) $$($(PKGCONFIG) --cflags $(PACKAGES) $(PACKAGES-$@)) $(DEFINES) $(DEFINES-$@)
CXXFLAGS = $(CXXSTD) $(DIAGNOSTICS) $(RECORD_SWITCHES) $(WARN) $(WARNCXX) $(OPT) -I$(SRCDIR) $(DEBUG) $(GCOVFLAGS) $(ASANFLAGS) $(ANALYZER) $(CXXFLAGS-$@)
LDFLAGS = -Wl,-O1 -Wl,-z,relro $(LDOPT)

DEFINES = -DVERSION=\"$(VERSION)\"
OTHERINCLUDES = -I/usr/include/obs
ifneq (,$(ALT_LIBREALSENSE))
OTHERINCLUDES += -I$(ALT_LIBREALSENSE)/include
LDOPT += -L$(ALT_LIBREALSENSE) -Wl,-R,$(ALT_LIBREALSENSE)
endif
DEPS = $(foreach f,$(filter %.o,$(ALLOBJS)),$(dir $(f)).$(notdir $(f:.o=.d))) $(foreach f,$(filter %.os,$(ALLOBJS)),$(dir $(f)).$(notdir $(f:.os=.d)))

WARN = -Wall -Wextra -Wnull-dereference -Wdouble-promotion -Wshadow -Wformat=2 -Wcast-qual -Wcast-align -Wstrict-aliasing -Wpointer-arith -Winit-self -Wredundant-decls -Wundef -Wempty-body -Wdouble-promotion
WARNCXX = -Wuseless-cast -Wsuggest-override
OPT = -Og
DEBUG = -g
DIAGNOSTICS =
RECORD_SWITCHES = -frecord-gcc-switches
GCOVFLAGS =
ASANFLAGS =
ANALYZER =

AR = ar
ARlto = $(CC)-ar
ARFLAGS = r$(TARV)cs
ifeq (,$(findstring lto,$(OL)))
ARCHIVE = $(AR) $(ARFLAGS)
else
ifeq (no,$(is_clang))
ARCHIVE = $(ARlto) $(ARFLAGS)
else
ARCHIVE = $(AR) $(ARFLAGS)
endif
endif

prefix = /usr
libdir = $(prefix)/$(shell $(CXX) -print-file-name=libc.so | $(GAWK) -v FS='/' '{ print($$(NF-1)) }')

SUBDIRS =
LIBDIRS =
TESTDIRS =
SUBTARGETS = all install $(CHECKTARGETS) clean dist
CHECKTARGETS = check

LIBS = $(LIBS-$@)
LIBS-obs-realsense.so = $$($(PKGCONFIG) --libs $(PACKAGES)) -lpthread
LIBS-testplugin = -lpthread -ldl
LIBS-testrealsense = $$($(PKGCONFIG) --libs $(PACKAGES) $(PACKAGES-testrealsense.o))


CXXFILES-obs-realsense.so = obs-realsense.cc realsense-greenscreen.cc

LIBOBJS-obs-realsense.so = $(CFILES-obs-realsense.so:.c=.os) $(CXXFILES-obs-realsense.so:.cc=.os)
ALLOBJS = $(LIBOBJS-obs-realsense.so) testplugin.o testrealsense.o
TESTS = testrealsense testplugin

all: $(PROJECT)

obs-realsense.so: $(LIBOBJS-obs-realsense.so) obs-realsense.map
	$(call DE,LINK) "$@"
	$(DC)$(LINK.cc) -shared -o $@ -Wl,--whole-archive $(filter %.os,$^) -Wl,--no-whole-archive $(LIBS-obs-realsense.so) -Wl,--version-script,obs-realsense.map

testplugin: testplugin.o
	$(call DE,LINK) "$@"
	$(DC)$(LINK.cc) -rdynamic -o $@ -Wl,--whole-archive $(filter %.o,$^) -Wl,--no-whole-archive $(LIBS-testplugin)

testrealsense: testrealsense.o realsense-greenscreen.os
	$(call DE,LINK) "$@"
	$(DC)$(LINK.cc) -o $@ -Wl,--whole-archive $^ -Wl,--no-whole-archive $(LIBS-testrealsense)

obs-realsense.spec: obs-realsense.spec.in Makefile
	$(SED) 's/@VERSION@/$(VERSION)/' $< > $@-tmp
	$(MV_F) $@-tmp $@

install: $(PROJECT)
	$(call DE,INSTALL) "$<"
	$(DC)$(INSTALL) -D -c -m 755 obs-realsense.so $(DESTDIR)$(libdir)/obs-plugins/obs-realsense.so

dist: obs-realsense.spec
	$(LN_FS) . obs-realsense-greenscreen-$(VERSION)
	$(TAR) zchf obs-realsense-greenscreen-$(VERSION).tar.gz obs-realsense-greenscreen-$(VERSION)/{Makefile,README.md,obs-realsense.cc,realsense-greenscreen.cc,realsense-greenscreen.hh,testplugin.cc,testrealsense.cc,obs-realsense.spec{,.in},obs-realsense.map}
	$(RM) obs-realsense-greenscreen-$(VERSION)

srpm: dist
	$(RPMBUILD) -ts obs-realsense-greenscreen-$(VERSION).tar.gz
rpm: dist
	$(RPMBUILD) -tb obs-realsense-greenscreen-$(VERSION).tar.gz

check: $(TESTS) $(PROJECT)
	./testplugin
	./testrealsense

-include $(DEPS)

clean: $(addsuffix /clean,$(SUBDIRS))
	$(call DE,CLEAN)
	$(DC)$(RM) $(PROJECT) $(TESTS) $(ALLOBJS) $(GENERATED) $(DEPS)

$(foreach t,$(SUBTARGETS),$(addsuffix /$t,$(SUBDIRS) $(TESTDIRS))): %:
	$(call DE,SUBDIR) "$(@D)" "$(@F)"
	$(DC)$(MAKE) SRCDIR=$(shell $(REALPATH) $(SRCDIR)/$(@D)/..) -C $(@D) $(@F)

CMDCLR-CXX = \e[33m
CMDCLR-CC = \e[38;5;208m
CMDCLR-GCH = \e[38;5;128m
CMDCLR-BISON = \e[38;5;192m
CMDCLR-FLEX = \e[38;5;157m
CMDCLR-LINK = \e[38;5;196m
CMDCLR-AR = \e[38;5;139m
CMDCLR-INSTALL = \e[38;5;44m
CMDCLR-DIST = \e[38;5;76m
CMDCLR-GCOV = \e[38;5;228m
CMDCLR-CLEAN = \e[38;5;81m
OKCLR = \e[32m
OFFCLR = \e[m
ERRCLR = \e[1;31m

ifeq ($(V),)
DE = @printf "%*s$(CMDCLR-$(1))%s$(OFFCLR) %s %s\n" $$(($(MAKELEVEL) * 2)) "" $(1)
DTS = @printf "%*s$(CMDCLR)%s$(OFFCLR) %s" $$(($(MAKELEVEL) * 2)) ""
DTE = @printf " $(OKCLR)%s$(OFFCLR)\n"
DC = @
NOPRINTDIR = --no-print-directory
TARV =
else
DE = @true "--" ""
DTS = @true "--" ""
DTE = @true "--" ""
DC =
NOPRINTDIR =
TARV = v
endif

.SUFFIXES: .hh.gch .hh .os
COMPILE_ARGS = -o $@ -MMD -MF $(@D)/.$(@F:$(suffix $@)=.d) $<
.c.o:
	$(call DE,CC) "$<"
	$(DC)$(COMPILE.c) $(COMPILE_ARGS)
.cc.o:
	$(call DE,CXX) "$<"
	$(DC)$(COMPILE.cc) $(COMPILE_ARGS)
.c.os:
	$(call DE,CC) "$<"
	$(DC)$(COMPILE.c) -fpic $(COMPILE_ARGS)
.cc.os:
	$(call DE,CXX) "$<"
	$(DC)$(COMPILE.cc) -fpic $(COMPILE_ARGS)
.hh.hh.gch:
	$(call DE,GCH) "$<"
	$(DC)$(COMPILE.cc) $(COMPILE_ARGS)

.PHONY: all clean install check dist srpm rpm
