/*
 * Helper functions that do not fit anywhere else
 */
#include "utils.h"
#include <stdlib.h>
#include <stdarg.h>

std::string strf(const char *fmt, ...)
{
	va_list vl;
	va_start(vl, fmt);
	char *buf = NULL;
	vasprintf(&buf, fmt, vl);
	va_end(vl);
	std::string s(buf);
	free(buf);
	return s;
}

double uniform()
{
	return rand() / (RAND_MAX + 1.0) +
	       rand() / ((RAND_MAX + 1.0) * RAND_MAX);
}
