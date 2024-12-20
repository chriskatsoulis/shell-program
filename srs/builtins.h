#pragma once

#include "parser.h"

struct builtin_redir {
  int pseudofd;
  int realfd;
  struct builtin_redir *next;
};

/* This is a function pointer typedef, representing functions with type
 * signature: int f(struct command *, struct builtin_redir const *redir_list)
 */
typedef int (*builtin_fn)(struct command *, struct builtin_redir const *redir);

/* Look up corresponding builtin function for a given command */
extern builtin_fn get_builtin(struct command *cmd);

