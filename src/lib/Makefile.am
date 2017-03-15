# Makefile for hxd/src/lib

INCLUDES = -I. -I../.. @INCLUDES@
DEFS = @DEFS@ -D_REENTRANT -D_GNU_SOURCE

noinst_LIBRARIES = liblib.a
liblib_a_SOURCES = getline.c xmalloc.c
liblib_a_LIBADD = @LIBOBJS@ xmalloc.o
liblib_a_DEPENDENCIES = @LIBOBJS@
