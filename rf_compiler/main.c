#include <stdint.h>
#include <stdio.h>
struct mapping {
  uint64_t from, to;
};

extern uint64_t rollforward_table_size;
extern struct mapping rollforward_table[];
extern struct mapping rollback_table[];

int __attribute__((preserve_all, noinline)) handler(int x) {
  printf("%d\n", x * x);
  return 0;
}

int main(int argc, char **argv) {
  printf("there are %lu rollforward entries\n", rollforward_table_size);
  for (int i = 0; i < argc; i++) {
    handler(i);
  }
  return 0;
}
