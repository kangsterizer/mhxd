#include <stdarg.h>
#include <stdio.h>

void
test_varargs_short (short n, ...)
{
	va_list ap;
	unsigned short i;

	va_start(ap, n);
	while (n--) {
		i = va_arg(ap, unsigned short);
		printf("0x%04x\n", i);
	}
	va_end(ap);
}

void
test_varargs_int (int n, ...)
{
	va_list ap;
	unsigned int i;

	va_start(ap, n);
	while (n--) {
		i = va_arg(ap, unsigned int);
		printf("0x%08x\n", i);
	}
	va_end(ap);
}

void
test_varargs_long (long n, ...)
{
	va_list ap;
	unsigned long i;

	va_start(ap, n);
	while (n--) {
		i = va_arg(ap, unsigned long);
		printf("0x%016lx\n", i);
	}
	va_end(ap);
}

int
main ()
{
	test_varargs_short(4, 0x0, 0xff, 0xffff, 0xffffffff);
	test_varargs_int(4, 0x0, 0xff, 0xffff, 0xffffffff);
	test_varargs_long(8, 0x0, 0xff, 0xffff, 0xffffffff,
		0xffffffffff, 0xffffffffffff, 0xffffffffffffff, 0xffffffffffffffff);

	return 0;
}
