#include <stdio.h>
#include <stdlib.h>

int main() {
  int *p = NULL;
  puts("before invalid access");
  *p = 0;
  puts("after invali access");

  exit(EXIT_SUCCESS);
}