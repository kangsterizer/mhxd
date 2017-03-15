# Makefile for hxd/src/protocol

prefix = $(PWD)/../../run/hxd

INCLUDES = -I. -I../.. -I../lib -I../conf -I../common -I../hxd @INCLUDES@ @COMMONINCLUDES@
DEFS = @DEFS@ -D_REENTRANT -D_GNU_SOURCE @MODULE_FLAG@

noinst_LIBRARIES = libprotocol.a
libprotocol_a_SOURCES = protocols.c
libprotocol_a_LIBADD = @PROTOLIBADD@
libprotocol_a_DEPENDENCIES = @PROTOLIBADD@

EXTRA_LTLIBRARIES = hotline.la irc.la kdx.la
lib_LTLIBRARIES = @PROTOLTLIBS@

# if module_hotline
# hotline_la_SOURCES = hotline.c
# endif
hotline_la_SOURCES =
hotline_la_LIBADD = @MODULE_HOTLINE_OBJS@
hotline_la_DEPENDENCIES = @MODULE_HOTLINE_OBJS@
hotline_la_CFLAGS =
hotline_la_LDFLAGS = -module -rpath $(prefix)/lib

# if module_irc
# irc_la_SOURCES = irc.c
# endif
irc_la_SOURCES =
irc_la_LIBADD = @MODULE_IRC_OBJS@
irc_la_DEPENDENCIES = @MODULE_IRC_OBJS@
irc_la_CFLAGS =
irc_la_LDFLAGS = -module -rpath $(prefix)/lib

# if module_kdx
# kdx_la_SOURCES = twofish.c kdx_common.c kdx.c
# endif
kdx_la_SOURCES =
kdx_la_LIBADD = @MODULE_KDX_OBJS@
kdx_la_DEPENDENCIES = @MODULE_KDX_OBJS@
kdx_la_CFLAGS =
kdx_la_LDFLAGS = -module -rpath $(prefix)/lib
