#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* These linker symbols denote boundaries, not ordinary int variables. */
extern char etext, edata, end;
char *cptr = "This message is output by showit()";
char buffer1[25];
static void showit(const char *text) {
  char *buffer2 = malloc(strlen(text) + 1);
  printf("&buffer2 (stack): %p\n", (void *)&buffer2);
  if (!buffer2) { perror("malloc"); exit(1); }
  printf("buffer2 (heap):   %p\n", (void *)buffer2);
  strcpy(buffer2, text);
  puts(buffer2);
  free(buffer2);
}
int main(void) {
  int i = 0;
  printf("etext (end text): %p\n", (void *)&etext);
  printf("edata (end data): %p\n", (void *)&edata);
  printf("end (end BSS):    %p\n", (void *)&end);
  printf("&cptr (data):     %p\n", (void *)&cptr);
  printf("cptr (literal):   %p\n", (void *)cptr);
  printf("buffer1 (BSS):    %p\n", (void *)buffer1);
  printf("&i (stack):       %p\n", (void *)&i);
  strcpy(buffer1, "A demonstration");
  puts(buffer1);
  showit(cptr);
  return 0;
}
