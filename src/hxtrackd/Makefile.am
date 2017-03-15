# Makefile for hxd/src/hxtrackd

bin_PROGRAMS = hxtrackd
exec_prefix = $(PWD)/../../run/hxtrackd

DEFS = @DEFS@ -DONLY_TRACKER
INCLUDES = -I../.. -I. -I../lib -I../common -I../conf -I../cipher -I../compress -I../protocol @INCLUDES@
COMMONLDADD = ../common/fd.o ../common/hxd_config.o ../common/version.o
LIBLDADD = @LIBLDADD@ @HXTRACKDLIBLDADD@ ../lib/xmalloc.o
LIBS = @HXTRACKDLIBS@
LDADD = $(COMMONLDADD) $(LIBLDADD) ../conf/libconf.a

ONLY_SRC = hxd.c
hxtrackd_SOURCES = tracker.c $(ONLY_SRC)
