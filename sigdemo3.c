#include <stdio.h>
#include <signal.h>

#define INPUTNUM 100

int main(int ac, char *av[])
{
  void inthandler(int);
  void quithandler(int);
  char input[INPUTNUM];
  int nchars;

  signal(SIGINT, inthandler);
  signal(SIGQUIT, quithandler);

  do {
    printf("\nType a message\n");
    nchars = read(0, input, (INPUTNUM-1));
    if (nchars == -1)
      perror("read returned an error");
    else {
      input[nchars] = '\0';
      printf("You typed: %s", input);
    }
  } while(strncmp(input, "quit", 4) != 0);
  return 0;
}

void inthandler(int s)
{
  void (* prev_qhandler)();
  prev_qhandler = signal(SIGQUIT, SIG_IGN);
  printf("Received signal %d.. waiting\n", s);
  sleep(2);
  signal(SIGQUIT, prev_qhandler);
  printf("Leaving inthandler\n");
}

void quithandler(int s)
{
  printf("Received signal %d..waiting\n", s);
  sleep(3);
  printf("Leaving quithandler\n");
}
