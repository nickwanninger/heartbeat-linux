#include <heartbeat.h>
#include <stdlib.h>
#include <stdio.h>

volatile int done = 0;

void callback(void) {
  done = 1;
  // yo
}


int main(int argc, char **argv) {
  int res = hb_init(0);
  printf("res=%d\n", res);

  hb_oneshot(10, callback);

  while (!done) {
  }


  hb_exit();
  return 0;
}
