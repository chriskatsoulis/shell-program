#define _POSIX_C_SOURCE 200809
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

#include "exit.h"
#include "params.h"
#include "parser.h"
#include "runner.h"
#include "signal.h"
#include "util/gprintf.h"
#include "wait.h"

/** Main shell loop
 */
int
main(int argc, char *argv[])
{
  struct command_list *cl = 0;

  /* Program initialization routines */
  if (parser_init() < 0) goto err;
  if (signal_init() < 0) goto err;

  /* Main Event Loop: REPL -- Read Evaluate Print Loop */
  for (;;) {
prompt:
    /* Check on backround jobs */
    if (wait_on_bg_jobs() < 0) goto err;

    /* Read input and parse it into a list of commands */
    if (signal_enable_interrupt(SIGINT) < 0) goto err;
    
    int res = command_list_parse(&cl, stdin);
    
    if (signal_ignore(SIGINT) < 0) goto err;

    if (res == -1) { /* System library errors */
      switch (errno) { /* Handle specific errors */
        case EINTR:
          clearerr(stdin);
          errno = 0;
          fputc('\n', stderr);
          goto prompt;
        default:
          goto err; /* Unrecoverable errors */
      }
    } else if (res < 0) { /* Parser syntax errors */
      fprintf(stderr, "Syntax error: %s\n", command_list_strerror(res));
      errno = 0;
      goto prompt;
    } else if (res == 0) { /* No commands parsed */
      if (feof(stdin)) shell_exit(); /* Exit on eof */
      goto prompt; /* Blank line */
    } else {
      gprintf("Parsed command list to execute:");
#ifndef NDEBUG
      command_list_print(cl, stderr);
      fputc('\n', stderr);
#endif
      gprintf("executing command list with %zu commands", cl->command_count);

      /* Execute commands */
      run_command_list(cl);

      /* Cleanup */
      command_list_free(cl);
      free(cl);
      cl = 0;
    }
  }

err:
  if (cl) command_list_free(cl);
  free(cl);
  params.status = 127;
  warn(0);
  shell_exit();
}
