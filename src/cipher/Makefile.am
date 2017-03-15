# Makefile for hxd/src/cipher

INCLUDES = -I. -I../.. -I../common -I../conf -I../lib -I../protocol @INCLUDES@ @CIPHERINCLUDES@
DEFS = @DEFS@ -D_REENTRANT -D_GNU_SOURCE

noinst_LIBRARIES = libcipher.a
libcipher_a_SOURCES = hmac.c
libcipher_a_LIBADD = @CIPHERLIBADD@
libcipher_a_DEPENDENCIES = @CIPHERLIBADD@
