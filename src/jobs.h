#pragma once
#include <sys/types.h>

/* Job id type */
typedef long jid_t;

struct job {
  jid_t jid;  /* Job id */
  pid_t pgid; /* Process group id */
  int status;
};

/** Gets a list of all jobs
 *
 * Invalidated by a call to jobs_add or jobs_remove
 */
extern struct job const *jobs_get_joblist(void);

/** Gets the size of the job list
 *
 * Invalidated by a call to jobs_add or jobs_remove
 */
extern size_t jobs_get_joblist_size(void);

/** Add a process group to the jobs list
 *
 * @param [in]pgid the process group id to add to the job list
 * @returns the new job id, or -1 on failure
 */
extern jid_t jobs_add(pid_t pgid);

/** Removes a process group from the jobs list
 *
 * @param [in]pgid the process group id to remove from the job list
 * @returns 0 on success, -1 on failure
 */
extern int jobs_remove_pgid(pid_t pgid);

/** Removes a job from the jobs list
 *
 * @param [in]jobid the job id to remove from the job list
 * @returns 0 on success, -1 on failure
 */
extern int jobs_remove_jid(jid_t jobid);

/** Looks up a job's job id
 *
 * @param [in]pgid the process group id to look up
 * @returns The job id on success, -1 on failure
 */
extern jid_t jobs_get_jid(pid_t pgid);

/** Looks up a job's process group id
 *
 * @param [in]jobid the job id to look up
 * @returns The process group id on success, -1 on failure
 */
extern pid_t jobs_get_pgid(jid_t jobid);

/** Sets a job's status value
 *
 * @params [in]jobid the job id of the job
 * @params [in]status the status to set
 *
 * @returns 0 on success, -1 on error
 */
extern int jobs_set_status(jid_t jobid, int status);

/** Gets a job's status value
 *
 * @params [in]jobid the job id of the job
 * @params [out]status
 *
 * @returns 0 on success, -1 on error
 */
extern int jobs_get_status(jid_t jobid, int *status);

/** Cleans up any resources associated with jobs tracking */
extern void jobs_cleanup(void);
