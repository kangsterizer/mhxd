# Makefile for hxd/src/conf

INCLUDES = -I. -I../.. -I../common -I../lib @INCLUDES@
DEFS = @DEFS@ -D_REENTRANT -D_GNU_SOURCE

bin_PROGRAMS = genconf
prefix = $(PWD)/../../run/hxd

genconf_SOURCES = genconf.c
genconf_LDADD = ../lib/xmalloc.o ../lib/getopt1_r.o ../lib/getopt_r.o libconf.a

noinst_LIBRARIES = libconf.a
libconf_a_SOURCES = conf.c
libconf_a_LIBADD = ../lib/getline.o
libconf_a_DEPENDENCIES = 
