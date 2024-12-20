#pragma once
#include <stdio.h>

/* This is the main command list structure returned by command_list_parse. */
struct command_list {
  struct command {
    /* Assignment name, value pairs.
     * e.g. name=value
     */
    struct assignment { 
      char *name;
      char *value;
    } **assignments;
    size_t assignment_count;

    /* Command words
     * This is the name of the command, and its arguments (if any)
     */
    char **words;
    size_t word_count;

    /* I/O redirection operators 
     */
    struct io_redir {
      int io_number; /* Left-hand file descriptor operand  */

      /* The particular operator encountered */
      enum io_operator {
        OP_GREAT,     /* >  */
        OP_LESS,      /* <  */
        OP_LESSGREAT, /* <> */
        OP_DGREAT,    /* >> */
        OP_GREATAND,  /* >& */
        OP_LESSAND,   /* <& */
        OP_CLOBBER,   /* >| */
      } io_op;

      /* Right-hand filename operand */
      char *filename;
    } **io_redirs;
    size_t io_redir_count;

    /* The control operator ending this particular command
     *
     * one of '&' (background), '|' (pipeline), or ';' (foreground)
     */
    char ctrl_op;
  } **commands;

  size_t command_count;
};

extern int is_interactive;

int parser_init(void);

/** Receives input and parses it into a command list */
int command_list_parse(struct command_list **cl, FILE *stream);

/** Returns a descriptive error of any parse errors encountered during parsing
 */
char const *command_list_strerror(int e);

/** Frees a parsed command list structure */
void command_list_free(struct command_list *cl);

/** Prints a parsed command list */
void command_list_print(struct command_list const *cl, FILE *stream);

/** Prints an individual parsed command */
void command_print(struct command const *cmd, FILE *stream);
