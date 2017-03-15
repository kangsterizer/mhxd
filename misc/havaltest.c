
/*
 *  havaltest.c:  specifies a test program for the HAVAL hashing library.
 *
 *  Arguments for the test program:
 *
 *      (none)    - hash input from stdin
 *      ?,-?,-h   - show help menu
 *      -c        - hash certification data
 *      -e        - test whether your machine is little-endian
 *      -mstring  - hash message (string of chars)
 *      -s        - test speed
 *      file_name - hash file
 *
 *  Makefile for the testing program:
 * 
 *         CC=acc
 *         CFLAGS=-fast
 *         
 *         haval: haval.o havaltest.o 
 *                ${CC} ${CFLAGS} haval.o havaltest.o -o $@
 *         haval.o havaltest.o: havalapp.h
 *         
 *         clean:
 *                /usr/bin/rm -f *.o haval
 *
 *  Author:     Yuliang Zheng
 *              School of Computing and Information Technology
 *              Monash University
 *              McMahons Road, Frankston 3199, Australia
 *              Email: yzheng@fcit.monash.edu.au
 *              URL:   http://pscit-www.fcit.monash.edu.au:/~yuliang/
 *              Voice: +61 3 9904 4196
 *              Fax  : +61 3 9904 4124
 *
 *  Date:
 *              First release: June  1993
 *              Revised:       April 1996
 *                       Major change:
 *                       1. now we use CPU time, instead of elapsed time,
 *                          to measure the speed of HAVAL.
 *
 *      Copyright (C) 1996 by Yuliang Zheng.  All rights reserved. 
 *      This program may not be sold or used as inducement to sell
 *      a  product without the  written  permission of the author.
 */

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <string.h>
#include "haval.h"

#define NUMBER_OF_BLOCKS 5000               /* number of test blocks */
#define BLOCK_SIZE       5000               /* number of bytes in a block */

static void haval_speed (void);             /* test the speed of HAVAL */
static void haval_cert (void);              /* hash test data set */
static void haval_print (unsigned char *, int);  /* print a fingerprint */
static int  little_endian (void);           /* test endianity */

int main (int argc, char *argv[])
{
  int           i, fptlen = 128, passes = 3;
  unsigned char fingerprint[256 >> 3];

  if (argc <= 1) {
    argv[1] = "?";
    argc++;
  }
  for (i = 1; i < argc; i++) {
    if ((argv[i][0] == '?') ||                      /* show help info */
        (argv[i][0] == '-' && argv[i][1] == '?') ||
        (argv[i][0] == '-' && argv[i][1] == 'h')) {
      printf (" (none)     hash input from stdin\n");
      printf (" ?/-?/-h    show help menu\n");
      printf (" -c         hash certification data\n");
      printf (" -e         test endianity\n");
      printf (" -mstring   hash message\n");
      printf (" -s         test speed\n");
      printf (" file_name  hash file\n");
    } else if (argv[i][0] == '-' && argv[i][1] == 'm') {  /* hash string */
      haval_string (argv[i]+2, fingerprint, fptlen, passes);
      printf ("HAVAL(\"%s\") = ", argv[i]+2);
      haval_print (fingerprint, fptlen);
      printf ("\n");
    } else if (strcmp (argv[i], "-c") == 0) {      /* hash test set */
      haval_cert ();
    } else if (strcmp (argv[i], "-s") == 0) {      /* test speed */
      haval_speed ();
    } else if (strcmp (argv[i], "-e") == 0) {      /* test endianity */
      if (little_endian()) {
        printf ("Your machine is little-endian.\n");
        printf ("You may define LITTLE_ENDIAN to speed up processing.\n");
      } else {
        printf ("Your machine is NOT little-endian.\n");
        printf ("You must NOT define LITTLE_ENDIAN.\n");
      }
    } else {                                       /* hash file */
      int fd = open(argv[i], O_RDONLY);
      if (fd < 0 || haval_fd (fd, -1, fingerprint, fptlen, passes)) {
        printf ("%s can not be opened !\n= ", argv[i]);
      } else {
        printf ("HAVAL(File %s) = ", argv[i]);
        haval_print (fingerprint, fptlen);
        printf ("\n");
      }
    }
  }
  return (0);
}

/* test the speed of HAVAL */
static void haval_speed (void)
{
  haval_state   state;
  unsigned char buff[BLOCK_SIZE];
  unsigned char fingerprint[256 >> 3];
  clock_t       clks;
  double        cpu_time;
  unsigned int  i;
  int fptlen = 128, passes = 3;

  printf ("Test the speed of HAVAL (PASS = %d, FPTLEN = %d bits).\n", passes, fptlen);
  printf ("Hashing %d %d-byte blocks ...\n", NUMBER_OF_BLOCKS, BLOCK_SIZE);

  /* initialize test block */
  for (i = 0; i < BLOCK_SIZE; i++) {
    buff[i] = ~0;
  }

  /* reset the clock */
  clock();

  /* hash */
  haval_start (&state, fptlen, passes);
  for (i = 0; i < NUMBER_OF_BLOCKS; i++) {
    haval_hash (&state, buff, BLOCK_SIZE);
  }
  haval_end (&state, fingerprint);

  /* get the number of clocks */
  clks = clock();
  /* get cpu time */
  cpu_time = (double)clks / (double)CLOCKS_PER_SEC;

  if (cpu_time > 0.0) {
    printf ("CPU Time = %3.1f seconds\n", cpu_time);
    printf ("   Speed = %4.2f MBPS (megabits/second)\n",
    (NUMBER_OF_BLOCKS * BLOCK_SIZE * 8)/(1.0E6 * cpu_time));
  } else {
    printf ("not enough blocks !\n");
  }
}

/* hash a set of certification data and print the results.  */
static void haval_cert (void)
{
  char          *str;
  unsigned char fingerprint[256 >> 3];

  printf ("\n");
  printf("        HAVAL (V.1) CERTIFICATION DATA ");
  printf ("\n");
  printf("        ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^");
  printf ("\n");

  printf ("PASS=%d, FPTLEN=%d:", 3, 128);
  str = "";
  haval_string (str, fingerprint, 128, 3);
  printf ("HAVAL(\"%s\") = ", str);
  haval_print (fingerprint, 128);
  printf ("\n");

  printf ("PASS=%d, FPTLEN=%d:", 3, 160);
  str = "a";
  haval_string (str, fingerprint, 160, 3);
  printf ("HAVAL(\"%s\") = ", str);
  haval_print (fingerprint, 160);
  printf ("\n");

  printf ("PASS=%d, FPTLEN=%d:", 4, 192);
  str = "HAVAL";
  haval_string (str, fingerprint, 192, 4);
  printf ("HAVAL(\"%s\") = ", str);
  haval_print (fingerprint, 192);
  printf ("\n");

  printf ("PASS=%d, FPTLEN=%d:", 4, 224);
  str = "0123456789";
  haval_string (str, fingerprint, 224, 4);
  printf ("HAVAL(\"%s\") = ", str);
  haval_print (fingerprint, 224);
  printf ("\n");

  printf ("PASS=%d, FPTLEN=%d:", 5, 256);
  str = "abcdefghijklmnopqrstuvwxyz";
  haval_string (str, fingerprint, 256, 5);
  printf ("HAVAL(\"%s\") = ", str);
  haval_print (fingerprint, 256);
  printf ("\n");

  printf ("PASS=%d, FPTLEN=%d:", 5, 256);
  str = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
  haval_string (str, fingerprint, 256, 5);
  printf ("HAVAL(\"%s\")\n      = ", str);
  haval_print (fingerprint, 256);
  printf ("\n");
}

/* test endianity */
static int little_endian(void)
{
  unsigned long *wp;
  unsigned char str[4] = {'A', 'B', 'C', 'D'};

  wp = (unsigned long *)str;
  if (str[0] == (unsigned char)( *wp & 0xFF)) {
    return (1);                       /* little endian */
  } else {
    return (0);                       /* big endian */
  }
}

/* print a fingerprint in hexadecimal */
static void haval_print (unsigned char *fingerprint, int fptlen)
{
  int i;

  for (i = 0; i < fptlen >> 3; i++) {
    printf ("%02X", fingerprint[i]);
  }
}



