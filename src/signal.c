#define _POSIX_C_SOURCE 200809L
#include <signal.h>
#include <errno.h>
#include <stddef.h>

#include "signal.h"

static void
interrupting_signal_handler(int signo)
{
  /* Its only job is to interrupt system calls--like read()--when
   * certain signals arrive--like Ctrl-C.
   */
}

static struct sigaction ignore_action = {.sa_handler = SIG_IGN},
                        interrupt_action = {.sa_handler =
                                                interrupting_signal_handler},
                        old_sigtstp, old_sigint, old_sigttou;

/* Ignore certain signals.
 * 
 * @returns 0 on success, -1 on failure
 *
 * The list of signals to ignore:
 *   - SIGTSTP
 *   - SIGINT
 *   - SIGTTOU
 *
 * Should be called immediately on entry to main() 
 *
 * Saves old signal dispositions for a later call to signal_restore()
 */
int
signal_init(void)
{
  /* Initialize signals, store old actions
   * Ignore SIGTSTP, SIGINT, SIGTTOU signals and save their old actions.
   */
  
  if (sigaction(SIGTSTP, &ignore_action, &old_sigtstp) == -1)
    return -1;

  if (sigaction(SIGINT, &ignore_action, &old_sigint) == -1)
    return -1;

  if (sigaction(SIGTTOU, &ignore_action, &old_sigttou) == -1)
    return -1;

  return 0;
}

/* Enable signal to interrupt blocking syscalls (read/getline, etc) 
 *
 * @returns 0 on success, -1 on failure
 *
 * does not save old signal disposition
 */
int
signal_enable_interrupt(int sig)
{
  /* Set the signal disposition for signal to interrupt. 
   * This will use the interrupt_action to handle the signal.
   */
  struct sigaction action;
  action.sa_handler = interrupting_signal_handler;
  sigemptyset(&action.sa_mask); 
  action.sa_flags = 0;  
  
  if (sigaction(sig, &action, NULL) == -1)
    return -1;

  return 0;
}

/* Ignore a signal
 *
 * @returns 0 on success, -1 on failure
 *
 * does not save old signal disposition
 */
int
signal_ignore(int sig)
{
  /* Set the signal disposition for signal back to its old state. 
   * This will use the saved old signal disposition.
   */
  struct sigaction action;
  if (sigaction(sig, &ignore_action, &action) == -1)
    return -1;

  return 0;
}

/* Restores signal dispositions to what they were when shell was invoked
 *
 * @returns 0 on success, -1 on failure
 *
 */
int
signal_restore(void)
{
  /* Restore old actions for SIGTSTP, SIGINT, SIGTTOU signals */
  if (sigaction(SIGTSTP, &old_sigtstp, NULL) == -1)
    return -1;

  if (sigaction(SIGINT, &old_sigint, NULL) == -1)
    return -1;

  if (sigaction(SIGTTOU, &old_sigttou, NULL) == -1)
    return -1;

  return 0;
}
