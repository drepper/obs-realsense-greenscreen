PROJECT = obs-realsense.so
SRCDIR = .

DISTCC =
CXX = $(DISTCC) g++
CXXSTD = -std=gnu++20
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
RM = rm
RM_F = $(RM) -f

CPPFLAGS = -I$(SRCDIR) $(OTHERINCLUDES) $$($(PKGCONFIG) --cflags $(PACKAGES) $(PACKAGES-$@)) $(DEFINES) $(DEFINES-$@)
CXXFLAGS = $(CXXSTD) $(DIAGNOSTICS) $(RECORD_SWITCHES) $(WARN) $(WARNCXX) $(OPT) -I$(SRCDIR) $(DEBUG) $(GCOVFLAGS) $(ASANFLAGS) $(ANALYZER) $(CXXFLAGS-$@)
LDFLAGS = -Wl,-O1 -Wl,-z,relro $(LDOPT)

DEFINES =
OTHERINCLUDES =
DEPS = $(foreach f,$(ALLOBJS),$(dir $(f)).$(notdir $(f:.o=.d)))

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
ALLOBJS = $(LIBOBJS-obs-realsense.so) testrealsense.o
TESTS = testrealsense testplugin

all: $(PROJECT)

obs-realsense.so: $(LIBOBJS-obs-realsense.so)
	$(call DE,LINK) "$@"
	$(DC)$(LINK.cc) -shared -o $@ -Wl,--whole-archive $(filter %.os,$^) -Wl,--no-whole-archive $(LIBS-obs-realsense.so)

testplugin: testplugin.o
	$(call DE,LINK) "$@"
	$(DC)$(LINK.cc) -rdynamic -o $@ -Wl,--whole-archive $(filter %.o,$^) -Wl,--no-whole-archive $(LIBS-testplugin)

testrealsense: testrealsense.o realsense-greenscreen.o
	$(call DE,LINK) "$@"
	$(DC)$(LINK.cc) -o $@ -Wl,--whole-archive $(filter %.o,$^) -Wl,--no-whole-archive $(LIBS-testrealsense)

check: $(TESTS) $(PROJECT)
	./testplugin
	./testrealsense

-include $(DEPS)

clean: $(addsuffix /clean,$(SUBDIRS))
	$(call DE,CLEAN)
	$(DC)$(RM_F) $(PROJECT) $(TESTS) $(ALLOBJS) $(GENERATED) $(DEPS)

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

.PHONY: all clean
