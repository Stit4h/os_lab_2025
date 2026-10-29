#ifndef LAB4_CLI_H
#define LAB4_CLI_H
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
static int ParseNumber(const char *text, unsigned int *out, unsigned int max) {
  if (!text || *text < '0' || *text > '9') return -1;
  errno = 0;
  char *tail;
  unsigned long value = strtoul(text, &tail, 10);
  if (errno || *tail || value > max) return -1;
  *out = (unsigned int)value;
  return 0;
}
#endif
