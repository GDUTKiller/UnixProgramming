/*
 * play_again.c
 * purpose: ask if user wants another transaction
 *  method: set tty into char-by-char mode and no-echo mode
 *          set tty into no-delay mode
 *          read char, return result
 * returns: 0 => yes, 1 => no, 2 => timeout
 */
#include <stdio.h>
#include <termios.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>

#define ASK "Do you want another transaction"
#define TRIES 3            /* max tries */
#define SLEEPTIME 2       /* time per try */
#define BEEP putchar('a') /* alert user */


int main()
{
  int response;
  void ctrl_c_handler(int);
  tty_mode(0);                          /* set -icanon -echo */
  set_cr_noecho_mode();
  set_nodelay_mode();                   /* noinput => EOF */
  signal( SIGINT, ctrl_c_handler);
  signal( SIGQUIT, SIG_IGN);
  response = get_response(ASK, TRIES);
  tty_mode(1);
  return response;
}

/*
 * purpose: ask a question and wait for a y/n answer or maxtries
 *  method: use getchar and ignore non y/n answers
 * returns: 0 => yes, 1 => no, 2 => timeout
 */
int get_response(char *question, int maxtries)
{
  int input;
  printf("%s(y/n)? ", question);
  fflush(stdout);
  while (1) {
    sleep(SLEEPTIME);
    input = tolower(get_ok_char());
    if (input == 'y')
      return 0;
    if (input == 'n')
      return 1;
    if (maxtries-- <= 0 )
      return 2;    
    BEEP;
  }
}

/*
 * skip over non - lefal chars and return y,Y,n,N or EOF
 */
get_ok_char()
{
  int c;
  while ( (c = getchar()) != EOF && strchr("yYnN", c) == NULL)
    ;
  return c;
}

/*
 * purpose: put file descriptor 0(i.e. stdin) into chr-by-chr mode and noecho mode
 *  method: use bits in termios
 */
set_cr_noecho_mode()
{
  struct termios ttystate;
  tcgetattr(0, &ttystate);
  ttystate.c_lflag &= ~ICANON; /* no buffering*/
  ttystate.c_lflag &= ~ECHO; /* no echo either*/
  ttystate.c_cc[VMIN] = 1; /* get 1 char by time*/
  tcsetattr(0, TCSANOW, &ttystate); /* install setting*/
}

/*
 * purpose: put file descriptor 0 into no-delay mode
 *  method: use fcntl to set bits
 *   notes: tcsetattr() will do something similar, but it is imcomplicated
 */
set_nodelay_mode()
{
  int termflags;
  termflags = fcntl(0, F_GETFL);
  termflags |= O_NDELAY;
  fcntl(0, F_SETFL, termflags);
}

/* how == 0 => save current mode, how == 1 => restore mode */
tty_mode(int how)
{
  static struct termios original_mode;
  static int original_flags;
  static int stored = 0;
  if (how == 0) {
    tcgetattr(0, &original_mode);
    original_flags = fcntl(0, F_GETFL);
    stored = 1;
  }
  else if (stored) {
    tcsetattr(0, TCSANOW, &original_mode);
    fcntl( 0, F_SETFL, original_flags);
  }
}

void ctrl_c_handler(int signum)
{
  tty_mode(1);
  exit(1);
}
