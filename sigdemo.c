#include <stdio.h>
#include <signal.h>

int main()
{
  void f(int);
  int i;
  signal(SIGINT, f);
  for (i = 0; i < 5; ++i) {
    sleep(2);
    printf("for\n");
  }
}

void f(int signum)
{
  printf("int\n");
}
