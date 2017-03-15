# Makefile for hxd/src/apple

INCLUDES = -I. -I../.. -I../lib @INCLUDES@
DEFS = @DEFS@ -D_REENTRANT -D_GNU_SOURCE

noinst_LIBRARIES = libapple.a
libapple_a_SOURCES = alias.c files.c unicode.c
libapple_a_LIBADD = @APPLELIBADD@
libapple_a_DEPENDENCIES = @APPLELIBADD@
