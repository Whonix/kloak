#!/usr/bin/make -f

CC         ?= gcc
INSTALL    ?= install
PKG_CONFIG ?= pkg-config
RONN       ?= ronn

CFLAGS     ?= -O2 -g

# NOTE: The systemd unit and apparmor profile are hardcoded to use
#       /usr/sbin/kloak. So if you change the default install paths,
#       you will have to patch those files yourself.
prefix         ?= /usr
sbindir        ?= $(prefix)/sbin
datadir        ?= $(prefix)/share
mandir         ?= $(datadir)/man

udev_rules_dir ?= /lib/udev/rules.d
apparmor_dir   ?= /etc/apparmor.d/
systemd_dir    ?= /usr/lib/systemd/system

TARGETARCH=$(shell $(CC) -dumpmachine)
CC_VERSION=$(shell $(CC) --version)

# https://best.openssf.org/Compiler-Hardening-Guides/Compiler-Options-Hardening-Guide-for-C-and-C++.html
#
# Omitted the following flags:
# -D_GLIBCXX_ASSERTIONS  # application is not written in C++
# -fstrict-flex-arrays=3 # not supported in Debian Bookworm's GCC version (12)
# -fPIC -shared          # not a shared library
# -fexceptions           # not multithreaded
# -fhardened             # not supported in Debian Bookworm's GCC version (12)
#
# Added the following flags:
# -fsanitize=address,undefined # enable ASan/UBSan
WARN_CFLAGS := -Wall -Wformat -Wformat=2 -Wconversion -Wimplicit-fallthrough \
	-Werror=format-security -Werror=implicit -Werror=int-conversion \
	-Werror=incompatible-pointer-types

ifeq (,$(findstring clang,$(CC_VERSION))) # if not clang
WARN_CFLAGS += -Wtrampolines -Wbidi-chars=any  # clang as for 18.1.8 doesn't support this warnings
endif

FORTIFY_CFLAGS := -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=3 -fstack-clash-protection \
	-fstack-protector-strong -fno-delete-null-pointer-checks \
	-fno-strict-overflow -fno-strict-aliasing -fsanitize=undefined

ifeq (yes,$(patsubst x86_64%-linux-gnu,yes,$(TARGETARCH)))
FORTIFY_CFLAGS += -fcf-protection=full # only supported on x86_64
endif
ifeq (yes,$(patsubst aarch64%-linux-gnu,yes,$(TARGETARCH)))
FORTIFY_CFLAGS += -mbranch-protection=standard # only supported on aarch64
endif

BIN_CFLAGS := -fPIE

CFLAGS := $(WARN_CFLAGS) $(FORTIFY_CFLAGS) $(BIN_CFLAGS) $(CFLAGS)
LDFLAGS := -Wl,-z,nodlopen -Wl,-z,noexecstack -Wl,-z,relro -Wl,-z,now \
	-Wl,--as-needed -Wl,--no-copy-dt-needed-entries -pie $(LDFLAGS)

ifeq (, $(shell which $(PKG_CONFIG)))
$(error pkg-config not installed!)
endif

all : kloak eventcap

kloak : src/main.c src/keycodes.c src/keycodes.h
	$(CC) src/main.c src/keycodes.c -o kloak -lm $(shell $(PKG_CONFIG) --cflags --libs libevdev) $(shell $(PKG_CONFIG) --cflags --libs libsodium) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS)

eventcap : src/eventcap.c
	$(CC) src/eventcap.c -o eventcap $(CPPFLAGS) $(CFLAGS) $(LDFLAGS)

MANPAGES := auto-generated-man-pages/eventcap.8 auto-generated-man-pages/kloak.8

man : $(MANPAGES)

auto-generated-man-pages/% : man/%.ronn
	ronn --manual="kloak Manual" --organization="kloak" <$< >$@

clean :
	rm -f kloak eventcap

install : all lib/udev/rules.d/95-kloak.rules etc/apparmor.d/usr.sbin.kloak  usr/lib/systemd/system/kloak.service $(MANPAGES)
	$(INSTALL) -d -m 755 $(addprefix $(DESTDIR), $(sbindir) $(mandir)/man8 $(udev_rules_dir) $(apparmor_dir) $(systemd_dir))
	$(INSTALL) -m 755 kloak eventcap $(DESTDIR)$(sbindir)
	$(INSTALL) -m 644 $(MANPAGES) $(DESTDIR)$(mandir)/man8
	$(INSTALL) -m 644 lib/udev/rules.d/95-kloak.rules $(DESTDIR)$(udev_rules_dir)
	$(INSTALL) -m 644 etc/apparmor.d/usr.sbin.kloak $(DESTDIR)$(apparmor_dir)
	$(INSTALL) -m 644 usr/lib/systemd/system/kloak.service $(DESTDIR)$(systemd_dir)
