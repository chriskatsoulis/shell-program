#include <stdlib.h>
#include <string.h>

#include "jobs.h"

struct job *jobs_joblist;
size_t jobs_joblist_size = 0;

struct job const *
jobs_get_joblist(void)
{
  return jobs_joblist;
}

size_t
jobs_get_joblist_size(void)
{
  return jobs_joblist_size;
}

jid_t
jobs_add(pid_t pgid)
{
  /* Allocate space for a new job record */
  if (jobs_get_jid(pgid) >= 0) return -1;
  void *tmp =
      realloc(jobs_joblist, sizeof *jobs_joblist * (jobs_joblist_size + 1));
  if (!tmp) return -1;
  jobs_joblist = tmp;

  /* Find lowest unused jobid */
  jid_t jid = 0;
  size_t insert_at = 0;
  for (; insert_at < jobs_joblist_size; ++insert_at) {
    if (jid < jobs_joblist[insert_at].jid) break;
    jid = jobs_joblist[insert_at].jid + 1;
  }

  /* Keep the list sorted */
  memmove(&jobs_joblist[insert_at + 1],
          &jobs_joblist[insert_at],
          sizeof *jobs_joblist * (jobs_joblist_size - insert_at));

  /* Assign members to new entry */
  jobs_joblist[insert_at] =
      (struct job){.jid = jid, .pgid = pgid, .status = -1};
  ++jobs_joblist_size;
  return jid;
}

jid_t
jobs_get_jid(pid_t pgid)
{
  for (size_t i = 0; i < jobs_joblist_size; ++i) {
    if (jobs_joblist[i].pgid == pgid) return jobs_joblist[i].jid;
  }
  return -1; /* DNE */
}

pid_t
jobs_get_pgid(jid_t jid)
{
  for (size_t i = 0; i < jobs_joblist_size; ++i) {
    if (jobs_joblist[i].jid == jid) return jobs_joblist[i].pgid;
  }
  return -1; /* DNE */
}

int
jobs_remove_pgid(pid_t pgid)
{
  for (size_t i = 0; i < jobs_joblist_size;) {
    if (jobs_joblist[i].pgid == pgid) {
      memmove(&jobs_joblist[i],
              &jobs_joblist[i + 1],
              sizeof *jobs_joblist * (jobs_joblist_size - i - 1));
      --jobs_joblist_size;

      if (jobs_joblist_size) {
        void *tmp =
            realloc(jobs_joblist, sizeof *jobs_joblist * (jobs_joblist_size));
        if (!tmp) return -1;
        jobs_joblist = tmp;
      } else {
        free(jobs_joblist);
        jobs_joblist = 0;
      }
    }
    if (++i == jobs_joblist_size) return -1;
  }
  return -1; /* DNE */
}

int
jobs_remove_jid(jid_t jobid)
{
  return jobs_remove_pgid(jobs_get_pgid(jobid));
}

int
jobs_set_status(jid_t jid, int status)
{
  for (size_t i = 0; i < jobs_joblist_size; ++i) {
    if (jobs_joblist[i].jid == jid) {
      jobs_joblist[i].status = status;
      return 0;
    }
  }
  return -1;
}

int
jobs_get_status(jid_t jid, int *status)
{
  for (size_t i = 0; i < jobs_joblist_size; ++i) {
    if (jobs_joblist[i].jid == jid) {
      *status = jobs_joblist[i].status;
      return 0;
    }
  }
  return -1;
}

void
jobs_cleanup(void)
{
  free(jobs_joblist);
  jobs_joblist = 0;
  jobs_joblist_size = 0;
}
