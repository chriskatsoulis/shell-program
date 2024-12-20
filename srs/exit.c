#define _POSIX_C_SOURCE 200809L
#include <signal.h>
#include <stdlib.h>

#include "exit.h"
#include "jobs.h"
#include "params.h"
#include "vars.h"

/* Cleans up and exits the shell */
void
shell_exit(void)
{
  /* Send SIGHUP (Hangup) signal to all jobs */
  size_t job_count = jobs_get_joblist_size();
  struct job const *jobs = jobs_get_joblist();
  for (size_t i = 0; i < job_count; ++i) {
    pid_t pgid = jobs[i].pgid;
    kill(-pgid, SIGHUP);
  }

  /* Call associated cleanup routines */
  jobs_cleanup();
  vars_cleanup();
  exit(params.status);
}
