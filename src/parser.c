#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "expand.h"
#include "parser.h"
#include "util/gprintf.h"
#include "vars.h"

int is_interactive = 0;

int
parser_init()
{
  if (isatty(STDIN_FILENO)) {
    is_interactive = 1;
  } else if (errno == ENOTTY) {
    errno = 0;
  } else {
    return -1;
  }
  return 0;
}

static void
command_free(struct command *cmd)
{
  if (cmd) {
    for (size_t i = 0; i < cmd->assignment_count; ++i) {
      free(cmd->assignments[i]->name);
      free(cmd->assignments[i]->value);
      free(cmd->assignments[i]);
    }
    free(cmd->assignments);

    for (size_t i = 0; i < cmd->word_count; ++i) {
      free(cmd->words[i]);
    }
    free(cmd->words);

    for (size_t i = 0; i < cmd->io_redir_count; ++i) {
      free(cmd->io_redirs[i]->filename);
      free(cmd->io_redirs[i]);
    }
    free(cmd->io_redirs);
  }
}

void
command_list_free(struct command_list *cl)
{
  for (size_t i = 0; i < cl->command_count; ++i) {
    command_free(cl->commands[i]);
    free(cl->commands[i]);
  }
  free(cl->commands);
}

char const *
command_list_strerror(int e)
{
  char const *emsg[] = {[0] = "match failure",
                        [1] = "library error",
                        [2] = "unmatched `\"`",
                        [3] = "unmatched `'`",
                        [4] = "unterminated escape",
                        [5] = "unexpected symbol"};
  if (e > 0) {
    return "Success";
  } else {
    return emsg[-e];
  }
}

static char const *
redir_op_str(enum io_operator op)
{
  switch (op) {
    case OP_GREAT:
      return ">";
    case OP_LESS:
      return "<";
    case OP_LESSGREAT:
      return "<>";
    case OP_DGREAT:
      return ">>";
    case OP_GREATAND:
      return ">&";
    case OP_LESSAND:
      return "<&";
    case OP_CLOBBER:
      return ">|";
  }
  return "<unknown redirection operator>";
}

void
command_print(struct command const *cmd, FILE *stream)
{
  for (size_t i = 0; i < cmd->assignment_count; ++i) {
    fprintf(stream,
            "%s=%s ",
            cmd->assignments[i]->name,
            cmd->assignments[i]->value);
  }

  for (size_t i = 0; i < cmd->word_count; ++i) {
    fprintf(stream, "%s ", cmd->words[i]);
  }

  for (size_t i = 0; i < cmd->io_redir_count; ++i) {
    fprintf(stream,
            "%d%s",
            cmd->io_redirs[i]->io_number,
            redir_op_str(cmd->io_redirs[i]->io_op));
    fprintf(stream, " %s ", cmd->io_redirs[i]->filename);
  }

  if (cmd->ctrl_op != '\n') {
    fputc(cmd->ctrl_op, stream);
  } else {
    fputc(';', stream);
  }
}

void
command_list_print(struct command_list const *cl, FILE *stream)
{
  for (size_t i = 0; i < cl->command_count; ++i) {
    if (i != 0) fputc(' ', stream);
    command_print(cl->commands[i], stream);
  }
}

static void
discard_whitespace(char const **s)
{
  for (; isblank(**s); ++*s) {
  }
  if (**s == '#') {
    for (; **s && **s != '\n'; ++*s);
  }
}

static int
match_num(char const **s, int *out)
{
  char const *c = *s;
  *out = 0;
  for (; isdigit(*c); ++c) {
    int digval = *c - '0';
    assert(digval >= 0 && digval < 10);
    *out = *out * 10 + digval;
  }
  int retval = c - *s;
  *s = c;
  return retval;
}

static int
match_word(char const **s, char **out)
{
  int retval = 0;
  *out = 0;
  char const *c = *s;
  char const *word = c;

  // word     : word_part
  //          | word word_part
  //          ;
  // word_part: /[^ \t&;|<>"'\\]+/
  //          | /"([^"]|\\")*"/
  //          | /'[^']*'/
  //          ;
  for (; !isblank(*c); ++c) {
    if (strchr("&;|<>\n", *c) != 0) break;

    if (*c == '"') {
      /* Double quotes */
      ++c;
      for (; *c != '"'; ++c) {
        if (!*c) {
          gprintf("unmatched double quote");
          retval = -2;
          goto err; /* Syntax error */
        }

        if (*c == '\\') {
          ++c;
          if (!*c) {
            gprintf("missing trailing character after \\");
            retval = -4;
            goto err; /* Syntax error */
          }
          continue;
        }
      }
    } else if (*c == '\'') {
      /* Single quotes */
      ++c;
      for (; *c != '\''; ++c) {
        if (!*c) {
          gprintf("unmatched single quote");
          retval = -3;
          goto err;
        }
      }
    } else if (*c == '\\') {
      /* Escape */
      ++c;
      if (!*c) {
        gprintf("missing trailing character after \\");
        retval = -4;
        goto err;
      }
    }
  }
  if (c == word) goto match_fail;

  { /* Write output */
    void *tmp = strndup(word, c - word);
    if (!tmp) {
      retval = -errno;
      goto err;
    }
    *out = tmp;
  }

  retval = c - *s;
  *s = c;
  if (0) {
  match_fail:
    retval = 0;
  }
  if (0) {
  err:;
  }
  return retval;
}

/** [0-9]*(>>|>&|<&|<>|>|<)[ \t]*{word} */
static int
match_redirect(char const **s, struct io_redir **redir)
{
  int retval = 0;
  *redir = 0;
  struct io_redir r = {0};
  char const *c = *s;

  char *filename = 0;

  /* io_number */
  retval = match_num(&c, &r.io_number);
  if (retval < 0) goto err;
  if (retval == 0) { /* Assign a default io_number if match failed */
    if (*c == '>') r.io_number = 1;      /* stdout */
    else if (*c == '<') r.io_number = 0; /* stdin */
    else goto match_fail;
  }

  // operator: /(>>|>&|<&|<>|>|<)/
  char const *op = c;
  if (strncmp(op, ">>", 2) == 0) {
    r.io_op = OP_DGREAT;
    c += 2;
  } else if (strncmp(op, ">&", 2) == 0) {
    r.io_op = OP_GREATAND;
    c += 2;
  } else if (strncmp(op, ">|", 2) == 0) {
    r.io_op = OP_CLOBBER;
    c += 2;
  } else if (strncmp(op, "<&", 2) == 0) {
    r.io_op = OP_LESSAND;
    c += 2;
  } else if (strncmp(op, "<>", 2) == 0) {
    r.io_op = OP_LESSGREAT;
    c += 2;
  } else if (strncmp(op, ">", 1) == 0) {
    r.io_op = OP_GREAT;
    c += 1;
  } else if (strncmp(op, "<", 1) == 0) {
    r.io_op = OP_LESS;
    c += 1;
  } else {
    goto match_fail;
  }

  discard_whitespace(&c);
  retval = match_word(&c, &filename);
  if (retval < 0) goto err;
  if (retval == 0) goto match_fail;
  r.filename = filename;

  { /* Write output */
    void *tmp = malloc(sizeof **redir);
    if (!tmp) {
      retval = -1;
      goto err;
    }
    *redir = tmp;
    **redir = r;
  }
  retval = c - *s;
  *s = c;
  if (0) {
  match_fail:
    retval = 0;
  err:
    free(filename);
  }
  return retval;
}

static int
match_assignment(char const **s, struct assignment **assn)
{
  int retval = 0;
  struct assignment a = {0};

  char const *c = *s;

  char const *name = c;
  // name: /[A-z_][A-z0-9_]*/
  if (!isalpha(name[0]) && name[0] != '_') goto match_fail;

  for (; isalnum(*c) || *c == '_'; ++c);
  a.name = strndup(name, c - name);
  if (!a.name) {
    retval = -1;
    goto err;
  }

  /* match "=" */
  if (*c != '=') goto match_fail;
  ++c;

  /* Get value */
  retval = match_word(&c, &a.value);
  if (retval < 0) goto err;
  if (retval == 0) a.value = strdup("");

  { /* Write output */
    void *tmp = malloc(sizeof **assn);
    if (!tmp) {
      retval = -1;
      goto err;
    }
    *assn = tmp;
    **assn = a;
  }
  retval = c - *s;
  *s = c;
  if (0) {
  match_fail:
    retval = 0;
  err:
    free(a.name);
    free(a.value);
  }
  return retval;
}

static int
add_assignment(struct command *cmd, struct assignment *assn)
{
  /* NOLINTBEGIN */
  void *tmp = realloc(cmd->assignments,
                      sizeof *cmd->assignments * (cmd->assignment_count + 1));
  /* NOLINTEND */
  if (!tmp) return -1;
  cmd->assignments = tmp;
  cmd->assignments[cmd->assignment_count++] = assn;
  return 0;
}

static int
add_word(struct command *cmd, char *word)
{
  void *tmp = realloc(cmd->words, sizeof *cmd->words * (cmd->word_count + 1));
  if (!tmp) return -1;
  cmd->words = tmp;
  cmd->words[cmd->word_count++] = word;
  return 0;
}

static int
add_redirection(struct command *cmd, struct io_redir *redir)
{
  /* NOLINTBEGIN */
  void *tmp = realloc(cmd->io_redirs,
                      sizeof *cmd->io_redirs * (cmd->io_redir_count + 1));
  /* NOLINTEND */
  if (!tmp) return -1;
  cmd->io_redirs = tmp;
  cmd->io_redirs[cmd->io_redir_count++] = redir;
  return 0;
}

static int
match_command(char const **s, struct command **command)
{
  int retval = 0;
  struct command cmd = {0};
  char const *c = *s;

  for (;;) {
    discard_whitespace(&c);
    if (cmd.word_count == 0) {
      struct assignment *assn = 0;
      retval = match_assignment(&c, &assn);
      if (retval < 0) goto err;
      if (retval > 0) {
        add_assignment(&cmd, assn);
        continue;
      }
    }

    {
      struct io_redir *redir;
      retval = match_redirect(&c, &redir);
      if (retval < 0) goto err;
      if (retval > 0) {
        add_redirection(&cmd, redir);
        continue;
      }
    }

    {
      char *word;
      retval = match_word(&c, &word);
      if (retval < 0) goto err;
      if (retval > 0) {
        add_word(&cmd, word);
        continue;
      }
    }

    break;
  }
  discard_whitespace(&c);

  /* Empty command better be a blank line */
  if (cmd.word_count == 0 && cmd.assignment_count == 0 &&
      cmd.io_redir_count == 0) {
    if (*c == '\n') {
      goto match_fail;
    } else {
      retval = -5;
      goto err;
    }
  }

  switch (*c) {
    case '&':
    case ';':
    case '|':
      cmd.ctrl_op = *c++;
      break;
    case '\n':
    case '\0':
      cmd.ctrl_op = ';';
      break;
    default:
      retval = -5;
      goto err;
  }

  if (cmd.word_count > 0) {
    add_word(&cmd, 0);
    --cmd.word_count;
  }
  { /* Write output */
    void *tmp = malloc(sizeof **command);
    if (!tmp) {
      retval = -1;
      goto err;
    }
    *command = tmp;
    **command = cmd;
  }
  retval = c - *s;
  *s = c;
  if (0) {
  match_fail:
    retval = 0;
  err:
    command_free(&cmd);
  }
  return retval;
}

static int
add_command(struct command_list *cl, struct command *cmd)
{
  /* NOLINTBEGIN */
  void *tmp =
      realloc(cl->commands, sizeof *cl->commands * (cl->command_count + 1));
  /* NOLINTEND */
  if (!tmp) return -1;
  cl->commands = tmp;
  cl->commands[cl->command_count++] = cmd;
  return 0;
}

int
command_list_parse(struct command_list **cl, FILE *stream)
{
  int count = 0;
  int retval = 0;
  char *line = 0;
  size_t n = 0;
  char const *c;
  ssize_t line_length;
  struct command *cmd = 0;
  void *tmp = malloc(sizeof **cl);
  if (!tmp) {
    retval = -1;
    goto err;
  }
  *cl = tmp;
  (*cl)->command_count = 0;
  (*cl)->commands = 0;
  do {
    if (is_interactive) {
      char const *s = 0;
      if (!line) {
        s = vars_get("PS1");
        if (!s) {
          if (getuid() == 0) s = "#";
          else s = "$";
        }
      } else {
        s = vars_get("PS2");
        if (!s) s = ">";
      }
      assert(s);
      char *s_copy = strdup(s);
      if (s_copy) {
        if (expand_prompt(&s_copy)) {
          char prefix[] = "\n=== [SHELL] ===\n";
          write(fileno(stream),
                prefix,
                sizeof prefix - (s_copy[0] != '\n' ? 1 : 2));
          write(fileno(stream), s_copy, strlen(s_copy));
        }
      }
      free(s_copy);
    }
    line_length = getline(&line, &n, stream);
    if (line_length < 0) {
      if (feof(stream)) {
        goto eof;
      }
      retval = -1;
      goto err;
    }
    c = line;
    while (*c) {
      discard_whitespace(&c);
      retval = match_command(&c, &cmd);
      gprintf("match command returned %d", retval);
      if (retval < 0) goto err;
      if (retval == 0) {
        if ((*cl)->command_count == 0) goto match_fail;
        break;
      }
      count += retval;
      add_command(*cl, cmd);
    }
  } while (cmd->ctrl_op == '|');
  retval = count;
  if (0) {
  err:
  match_fail:
  eof:
    command_list_free(*cl);
    free(*cl);
    *cl = 0;
  }
  free(line);
  return retval;
}
