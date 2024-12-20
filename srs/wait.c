#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <err.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>

#include "jobs.h"
#include "params.h"
#include "parser.h"
#include "wait.h"

int
wait_on_fg_pgid(pid_t const pgid)
{
  if (pgid < 0) return -1;
  jid_t const jid = jobs_get_jid(pgid);
  if (jid < 0) return -1;
  /* Make sure the foreground group is running */
  /* Send the "continue" signal to the process group 'pgid'
   */
  if (kill(-pgid, SIGCONT) < 0) return -1;

  if (is_interactive) {
    /* Make 'pgid' the foreground process group */
     if (tcsetpgrp(STDIN_FILENO, pgid) < 0) return -1;
  }

  /* From this point on, all exit paths must account for setting shell
   * back to the foreground process group--no naked return statements */
  int retval = 0;

  /* Note, we loop until ECHILD and we use the status of
   * the last child process that terminated (in the previous iteration).
   * Consider a pipeline,
   *        cmd1 | cmd2 | cmd3
   *
   * Loop exactly 4 times, once for each child process, and a
   * fourth time to see ECHILD.
   */
  for (;;) {
    /* Wait on ALL processes in the process group 'pgid' */
    int status;
    pid_t res = waitpid(-pgid, &status, WUNTRACED);
    if (res < 0) {
      /* Error occurred (some errors are ok, see below)
       *
       * status may have a garbage value, use last_status from the
       * previous loop iteration */
      if (errno == ECHILD) {
        /* No unwaited-for children. The job is done! */
        errno = 0;
        if (jobs_get_status(jid, &status) < 0) goto err;
        if (WIFEXITED(status)) {
          /* Set params.status to the correct value */
          params.status = WEXITSTATUS(status);
        } else if (WIFSIGNALED(status)) {
          /* Set params.status to the correct value */
          params.status = WTERMSIG(status) + 128;
        }

        /* Remove the job for this group from the job list */
        jobs_remove_pgid(pgid);
        goto out;
      }
      goto err; /* An actual error occurred */
    }
    assert(res > 0);
    /* status is valid */

    /* Record status for reporting later when we see ECHILD */
    if (jobs_set_status(jid, status) < 0) goto err;

    /* Handle case where a child process is stopped
     *  The entire process group is placed in the background
     */
    if (WIFSTOPPED(status)) {
      fprintf(stderr, "[%jd] Stopped\n", (intmax_t)jid);
      jobs_set_status(jid, status);
      goto out;
    }

    /* A child exited, but others remain. Loop! */
  }

out:
  if (0) {
  err:
    retval = -1;
  }

  if (is_interactive) {
    /* Make shell the foreground process group again */
    if (tcsetpgrp(STDIN_FILENO, getpgrp()) < 0) retval = -1;
  }
  return retval;
}

int
wait_on_fg_job(jid_t jid)
{
  pid_t pgid = jobs_get_pgid(jid);
  if (pgid < 0) return -1;
  return wait_on_fg_pgid(pgid);
}

int
wait_on_bg_jobs()
{
  size_t job_count = jobs_get_joblist_size();
  struct job const *jobs = jobs_get_joblist();
  for (size_t i = 0; i < job_count; ++i) {
    pid_t pgid = jobs[i].pgid;
    jid_t jid = jobs[i].jid;
    for (;;) {
      int status;
      pid_t pid = waitpid(-pgid, &status, WNOHANG | WUNTRACED);
      if (pid == 0) {
        /* Unwaited children that haven't exited */
        break;
      } else if (pid < 0) {
        /* Error -- some errors are ok though! */
        if (errno == ECHILD) {
          /* No children -- print saved exit status */
          errno = 0;
          if (jobs_get_status(jid, &status) < 0) return -1;
          if (WIFEXITED(status)) {
            fprintf(stderr, "[%jd] Done\n", (intmax_t)jid);
          } else if (WIFSIGNALED(status)) {
            fprintf(stderr, "[%jd] Terminated\n", (intmax_t)jid);
          }
          jobs_remove_pgid(pgid);
          job_count = jobs_get_joblist_size();
          jobs = jobs_get_joblist();
          break;
        }
        return -1; /* Other errors are not ok */
      }

      /* Record status for reporting later when we see ECHILD */
      if (jobs_set_status(jid, status) < 0) return -1;

      /* Handle case where a process in the group is stopped */
      if (WIFSTOPPED(status)) {
        fprintf(stderr, "[%jd] Stopped\n", (intmax_t)jid);
        break;
      }
    }
  }
  return 0;
}
