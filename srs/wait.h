#pragma once
#include "jobs.h"

/** Place a process group in the foreground and wait on it 
 *
 * 
 * @returns 0 on success, -1 on failure
 */
int wait_on_fg_pgid(pid_t pgid);

/** Place a job  in the foreground and wait on it 
 *
 * @returns 0 on success, -1 on failure
 */
int wait_on_fg_job(jid_t jid);

/** Wait (nonblocking) on background jobs 
 * 
 * @returns 0 on success, -1 on failure
 */
int wait_on_bg_jobs();
