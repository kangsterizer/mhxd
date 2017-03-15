#ifndef __hxd_spinlock_h
#define __hxd_spinlock_h

/* testandset code from linuxthreads */

#define SL_RELEASE(spinlock) *spinlock = 0
#ifndef PT_EI
# define PT_EI extern inline
#endif

#if defined(__alpha__)

#include <asm/pal.h>

/* Spinlock implementation; required.  */
PT_EI long int
testandset (int *spinlock)
{
  long int ret, temp;

  __asm__ __volatile__(
	"/* Inline spinlock test & set */\n"
	"1:\t"
	"ldl_l %0,%3\n\t"
	"bne %0,2f\n\t"
	"or $31,1,%1\n\t"
	"stl_c %1,%2\n\t"
	"beq %1,1b\n"
	"2:\tmb\n"
	"/* End spinlock test & set */"
	: "=&r"(ret), "=&r"(temp), "=m"(*spinlock)
	: "m"(*spinlock)
        : "memory");

  return ret;
}

/* Spinlock release; default is just set to zero.  */
#undef SL_RELEASE
#define SL_RELEASE(spinlock) \
  __asm__ __volatile__("mb" : : : "memory"); \
  *spinlock = 0

#elif defined(__arm__)
/* Spinlock implementation; required.  */
PT_EI int
testandset (int *spinlock)
{
  register unsigned int ret;

  __asm__ __volatile__("swp %0, %1, [%2]"
		       : "=r"(ret)
		       : "0"(1), "r"(spinlock));

  return ret;
}
#elif defined(__i386__)
/* Spinlock implementation; required.  */
PT_EI int
testandset (int *spinlock)
{
  int ret;

  __asm__ __volatile__(
       "xchgl %0, %1"
       : "=r"(ret), "=m"(*spinlock)
       : "0"(1), "m"(*spinlock)
       : "memory");

  return ret;
}
#elif defined(__mc68000__)
/* Spinlock implementation; required.  */
PT_EI int
testandset (int *spinlock)
{
  char ret;

  __asm__ __volatile__("tas %1; sne %0"
       : "=dm"(ret), "=m"(*spinlock)
       : "m"(*spinlock)
       : "cc");

  return ret;
}
#elif defined(__mips__)
/* Spinlock implementation; required.  */
PT_EI long int
testandset (int *spinlock)
{
  long int ret, temp;

  __asm__ __volatile__(
	"# Inline spinlock test & set\n\t"
	".set\tmips2\n"
	"1:\tll\t%0,%3\n\t"
	"bnez\t%0,2f\n\t"
	".set\tnoreorder\n\t"
	"li\t%1,1\n\t"
	".set\treorder\n\t"
	"sc\t%1,%2\n\t"
	"beqz\t%1,1b\n"
	"2:\t.set\tmips0\n\t"
	"/* End spinlock test & set */"
	: "=&r"(ret), "=&r" (temp), "=m"(*spinlock)
	: "m"(*spinlock)
	: "memory");

  return ret;
}
#elif defined(__powerpc__)
/* Spinlock implementation; required.  */
static inline int testandset(int *spinlock)
{
  int ret;

  __asm__ __volatile__(
      "1:\t"
      "lwarx %0, 0, %2\n\t"
      "stwcx. %3, 0, %2\n\t"
      "bne 1b\n"
      "2:"
      : "=&r" (ret), "=m" (*spinlock)
      : "r" (spinlock), "r" (1)
      : "cr0");
  return ret;
}
#elif defined(__sparc__)
/* Spinlock implementation; required.  */
PT_EI int
testandset (int *spinlock)
{
  int ret;

  __asm__ __volatile__("ldstub %1,%0"
	: "=r"(ret), "=m"(*spinlock)
	: "m"(*spinlock));

  return ret;
}

/* Spinlock release; default is just set to zero.  */
#undef SL_RELEASE
#define SL_RELEASE(spinlock) \
  __asm__ __volatile__("stbar; stb %1,%0" : "=m"(*(spinlock)) : "r"(0));

#elif defined(__sparc_v9__)
/* Spinlock implementation; required.  */
PT_EI int
testandset (int *spinlock)
{
  int ret;

  __asm__ __volatile__("ldstub %1,%0"
	: "=r"(ret), "=m"(*spinlock) : "m"(*spinlock));

  return ret;
}
#else
#error no spin lock support for this arch
#endif

#endif
