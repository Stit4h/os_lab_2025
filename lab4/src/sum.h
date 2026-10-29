#ifndef LAB4_SUM_H
#define LAB4_SUM_H
#include <stdint.h>
struct SumArgs {
  const int *array;
  unsigned int begin, end;
  uint64_t result;
};
uint64_t Sum(const struct SumArgs *args);
void *ThreadSum(void *args);
#endif
