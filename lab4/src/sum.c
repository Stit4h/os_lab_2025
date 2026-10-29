#include "sum.h"
#include <stddef.h>
uint64_t Sum(const struct SumArgs *args) {
  uint64_t sum = 0;
  for (unsigned int i = args->begin; i < args->end; ++i)
    sum += (uint64_t)args->array[i];
  return sum;
}
void *ThreadSum(void *args) {
  struct SumArgs *part = args;
  part->result = Sum(part);
  return NULL;
}
