#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

#include "parser.h"
#include "builtins.h"
#include "exit.h"
#include "jobs.h"
#include "params.h"
#include "vars.h"
#include "wait.h"

/* Gets the real fd of a pseudo redirect */
static int
get_pseudo_fd(struct builtin_redir const *redir_list, int fd)
{
  for (; redir_list; redir_list = redir_list->next) {
    if (redir_list->realfd == fd) return -1;
    if (redir_list->pseudofd == fd) return redir_list->realfd;
  }
  return fd;
}

/**
 *
 *  This function exists to solve the edge case of
 *  a command that consists only of redirections
 *  and assignments.
 *
 */
static int
builtin_null(struct command *cmd, struct builtin_redir const *redir_list)
{
  return 0;
}

/* Change directory
 *
 * @returns 0 on success, -1 on error
 *
 * cd [path]
 *
 * if path is omitted, change to $HOME directory.
 *
 * Updates $PWD shell variable
 *
 * It is an error if too many arguments are provided, or if the chdir operation
 * fails
 */
static int
builtin_cd(struct command *cmd, struct builtin_redir const *redir_list)
{
  char const *target_dir = 0;
  if (cmd->word_count == 1) {
    target_dir = vars_get("HOME");
    if (!target_dir) {
      dprintf(get_pseudo_fd(redir_list, STDERR_FILENO), "cd: HOME not set\n");
      return -1;
    }
  }
  
  else if (cmd->word_count == 2) {
    target_dir = cmd->words[1];
  }
  
  else {
    dprintf(get_pseudo_fd(redir_list, STDERR_FILENO), "cd: too many arguments\n");
    return -1;
  }

  if (chdir(target_dir) != 0) {
    dprintf(get_pseudo_fd(redir_list, STDERR_FILENO), "cd: %s: No such file or directory\n", target_dir);
    return -1;
  }

  char *pwd = getcwd(NULL, 0);
  if (pwd) {
    vars_set("PWD", pwd);
    free(pwd);
  } else {
    dprintf(get_pseudo_fd(redir_list, STDERR_FILENO), "cd: failed to get current working directory\n");
    return -1;
  }

  return 0;
}

/** Exits smallsh
 *
 * @returns -1 on failure, otherwise exits the program
 *
 * exit [n]
 *
 * If [n] is omitted, exits with the exit status of the most recently terminated
 * foreground command.
 *
 * It is an error if too many arguments, or a non-numeric argument, is provided
 */
static int
builtin_exit(struct command *cmd, struct builtin_redir const *redir_list)
{
  if (cmd->word_count == 1) {
    shell_exit();
    return -1;
  }

  else if (cmd->word_count == 2) {
    char *endptr;
    long exit_code = strtol(cmd->words[1], &endptr, 10);

    if (*endptr != '\0' || exit_code < 0 || exit_code > 255) {
      dprintf(get_pseudo_fd(redir_list, STDERR_FILENO), "exit: %s: numeric argument required\n", cmd->words[1]);
      return -1; 
    }

    params.status = (int) exit_code;
    shell_exit();
    return -1;
  }

  else {
    dprintf(get_pseudo_fd(redir_list, STDERR_FILENO), "exit: too many arguments\n");
    return -1;
  }
}

/** Exports variables to the environment
 *
 * @returns 0 on success, -1 on failure
 *
 * export name[=value]...
 *
 */
static int
builtin_export(struct command *cmd, struct builtin_redir const *redir_list)
{
  for (size_t i = 1; i < cmd->word_count; ++i) {
    char *word = cmd->words[i];
    char *v = strchr(word, '=');
    if (v) {
      *v = '\0';
      if (vars_set(word, v + 1) < 0) return -1;
      if (vars_export(word) < 0) return -1;
      *v = '=';
    } else {
      if (vars_export(word) < 0) return -1;
    }
  }
  return 0;
}

/** Unsets list of shell variables
 *
 * @returns 0 (always succeeds)
 *
 * Unsetting nonexistent variables is not an error.
 */
static int
builtin_unset(struct command *cmd, struct builtin_redir const *redir_list)
{
  for (size_t i = 1; i < cmd->word_count; ++i) {
    char *var_name = cmd->words[i];

    if (vars_unset(var_name) != 0) {
      dprintf(get_pseudo_fd(redir_list, STDERR_FILENO), "unset: failed to unset variable %s\n", var_name);
    }
  }
  return 0;
}

/* Places the specified (backround) job in the foreground */
static int
builtin_fg(struct command *cmd, struct builtin_redir const *redir_list)
{
  jid_t job_id = -1;
  if (cmd->word_count == 1) {
    size_t job_count = jobs_get_joblist_size();
    if (job_count == 0) {
      dprintf(get_pseudo_fd(redir_list, STDERR_FILENO), "No jobs\n");
      return -1;
    }
    job_id = jobs_get_joblist()[0].jid;
  } else if (cmd->word_count == 2) {
    char *end = cmd->words[1];
    long val = strtol(cmd->words[1], &end, 10);
    if (*end || !cmd->words[1][0] || val < 0 || val > INT_MAX) {
      dprintf(get_pseudo_fd(redir_list, STDERR_FILENO),
              "fg: `%s': %s\n",
              cmd->words[1],
              strerror(EINVAL));
      return -1;
    }
    job_id = val;
  } else {
    dprintf(get_pseudo_fd(redir_list, STDERR_FILENO),
            "fg: `%s': %s\n",
            cmd->words[2],
            strerror(EINVAL));
    return -1;
  }

  pid_t pgid = jobs_get_pgid(job_id);
  if (pgid < 0) {
    errno = EINVAL;
    goto err;
  }
  kill(-pgid, SIGCONT);

  if (wait_on_fg_job(job_id) < 0) goto err;

  return 0;
err:
  dprintf(get_pseudo_fd(redir_list, STDERR_FILENO), "fg: %s", strerror(errno));
  return -1;
}

/* Places a (stopped) bg process in the background */
static int
builtin_bg(struct command *cmd, struct builtin_redir const *redir_list)
{
  jid_t job_id = -1;
  if (cmd->word_count == 1) {
    size_t job_count = jobs_get_joblist_size();
    if (job_count == 0) return -1;
    job_id = jobs_get_joblist()[0].jid;
  } else if (cmd->word_count == 2) {
    char *end = cmd->words[1];
    long val = strtol(cmd->words[1], &end, 10);
    if (*end || !cmd->words[1][0] || val < 0 || val > INT_MAX) {
      dprintf(get_pseudo_fd(redir_list, STDERR_FILENO),
              "fg: `%s': %s\n",
              cmd->words[1],
              strerror(EINVAL));
      return -1;
    }
    job_id = val;
  } else {
    dprintf(get_pseudo_fd(redir_list, STDERR_FILENO),
            "fg: `%s': %s\n",
            cmd->words[2],
            strerror(EINVAL));
    return -1;
  }

  pid_t pgid = jobs_get_pgid(job_id);
  if (pgid < 0) {
    errno = EINVAL;
    goto err;
  }
  kill(-pgid, SIGCONT);

  return 0;
err:
  dprintf(get_pseudo_fd(redir_list, STDERR_FILENO), "bg: %s", strerror(errno));
  return -1;
}

/* Prints a list of background jobs
 *
 * @returns 0 (always succeeds)
 *
 */
static int
builtin_jobs(struct command *cmd, struct builtin_redir const *redir_list)
{
  size_t job_count = jobs_get_joblist_size();
  struct job const *jobs = jobs_get_joblist();
  for (size_t i = 0; i < job_count; ++i) {
    dprintf(get_pseudo_fd(redir_list, STDERR_FILENO),
            "[%jd] %jd\n",
            (intmax_t)jobs[i].jid,
            (intmax_t)jobs[i].pgid);
  }
  return 0;
}

/** Built-in function selector method
 *
 * @param cmd the command under consideration
 *
 * @returns pointer to built-in function corresponding to cmd
 * @returns null pointer if not found
 */
builtin_fn
get_builtin(struct command *cmd)
{
  if (cmd->word_count == 0) return builtin_null;
  else if (strcmp(cmd->words[0], "cd") == 0) return builtin_cd;
  else if (strcmp(cmd->words[0], "exit") == 0) return builtin_exit;
  else if (strcmp(cmd->words[0], "fg") == 0) return builtin_fg;
  else if (strcmp(cmd->words[0], "bg") == 0) return builtin_bg;
  else if (strcmp(cmd->words[0], "jobs") == 0) return builtin_jobs;
  else if (strcmp(cmd->words[0], "unset") == 0) return builtin_unset;
  else if (strcmp(cmd->words[0], "export") == 0) return builtin_export;
  else return 0;
}
