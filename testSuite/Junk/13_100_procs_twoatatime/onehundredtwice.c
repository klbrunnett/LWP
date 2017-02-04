#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <lwp.h>

#define STACKSIZE 8096

static void counter(int num)
{
  if ( num ) {
    lwp_create((lwpfun)counter,(void*)num-1,STACKSIZE);
    printf("Counter is %d.  Exiting\n",num);
  } else {
    printf("Bye\n");
  }
  lwp_exit();
}


int main() {
  lwp_create((lwpfun)counter,(void*)50,STACKSIZE);
  lwp_create((lwpfun)counter,(void*)50,STACKSIZE);
  lwp_start();
  exit(0);
}
