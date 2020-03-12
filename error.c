#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include "snprintf.h"

void Error(const char *format, ...)
{
	va_list args;
	char *buf;

	if (!format) return;

	va_start(args, format);
	vasprintf(&buf, format, args);

	if (buf) {
			puts(buf);
			free(buf);
	}

}
