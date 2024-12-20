#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "util/gprintf.h"
#include "vars.h"

struct var {
  struct var *next;
  bool export : 1;
  char *value;
  char name[];
};

static struct var *var_list = 0;

/* Checks if a variable name is a valid XBD name 
 *
 * @returns 1 if yes, 0 if not
 *
 * This is an internal function that does not validate its argument. It is
 * faster for internal use, but unsafe to expose as part of the API. Notice
 * we wrap it with vars_is_valid_varname() below.
 */
static int
is_valid_varname(char const *name)
{
  if (!name || !isalpha(name[0]) && name[0] != '_') return 0;
    for (int i = 1; name[i] != '\0'; i++) {
        if (!isalnum(name[i]) && name[i] != '_') return 0;
    }
    return 1;
}

/** Checks if a variable name is a valid XBD name 
 *
 * @returns 1 if yes, 0 if not
 *
 * This is an external function that validates its argument before calling the
 * internal unsafe version 
 */
int
vars_is_valid_varname(char const *name)
{
  if (!name || strlen(name) == 0) {
        errno = EINVAL;
        return 0;
    }
    return is_valid_varname(name);
}

/* Returns nullptr if not found */
static struct var *
find_var(char const *name)
{
  assert(name);
  assert(is_valid_varname(name));

  /* Search local varlist */
  struct var *v = var_list;
  for (; v; v = v->next) {
    if (strcmp(v->name, name) == 0) return v;
  }

  return 0;
}

/* Creates a new var with name and inserts into var list */
static struct var *
new_var(char const *name)
{
  assert(is_valid_varname(name));
  assert(!find_var(name));
  struct var *v = malloc(sizeof *v + strlen(name) + 1);
  if (!v) return 0;
  strcpy(v->name, name);

  char *val = getenv(name);
  if (val) {
    v->export = 1;
  } else {
    v->export = 0;
  }
  v->value = 0;
  v->next = var_list;
  var_list = v;
  return v;
}

/* Remove a var from varlist and return it */
static void
remove_var(char const *name)
{
  assert(is_valid_varname(name));
  struct var **link = &var_list;
  for (; *link; link = &((*link)->next)) {
    if (strcmp((*link)->name, name) == 0) {
      void *tmp = (*link)->next;
      free((*link)->value);
      free(*link);
      *link = tmp;
      break;
    }
  }
}

/* Return existing var, or make a new var */
static struct var *
ensure_var(char const *name)
{
  assert(is_valid_varname(name));
  struct var *v = find_var(name);
  return v ? v : new_var(name);
}

int
vars_set(char const *name, char const *value)
{
  if (!name || !value || !is_valid_varname(name)) {
    errno = EINVAL;
    return -1;
  }
  gprintf("vars_set(%s, %s)", name, value);

  struct var *v = ensure_var(name);
  if (!v) return -1;

  if (v->export) {
    gprintf("%s=%s is exported, updating env", name, value);
    return setenv(name, value, 1);
  }

  char *dupval = strdup(value);
  if (!dupval) return -1;
  v->value = dupval;
  return 0;
}

char const *
vars_get(char const *name)
{
  if (!name || !is_valid_varname(name)) {
    errno = EINVAL;
    return 0;
  }

  gprintf("searching for %s in local var list", name);
  /* Look through our local var list */
  struct var *v = find_var(name);
  if (v && !v->export) {
    gprintf("found local var %s with value %s", name, v->value);
    return v->value;
  }

  gprintf("searching for %s in environment", name);
  /* Fallback to searching environment */
  char const *value = getenv(name);
#ifndef NDEBUG
  if (value) {
    gprintf("found env var %s with value %s", name, value);
  } else {
    gprintf("did not find var %s", name);
  }
#endif
  return value;
}

int
vars_unset(char const *name)
{
  if (!name || !is_valid_varname(name)) {
    errno = EINVAL;
    return -1;
  }
  gprintf("unsetting var %s", name);
  remove_var(name);
  return unsetenv(name);
}

int
vars_export(char const *name)
{
  if (!name || !is_valid_varname(name)) {
    errno = EINVAL;
    return -1;
  }
  gprintf("marking %s for export", name);
  struct var *v = ensure_var(name);
  if (!v) return -1;

  /* Mark exported */
  v->export = 1;

  /* Only actually export to env if already set */
  if (v->value) {
    gprintf("exporting value %s for var %s", v->value, name);
    if (setenv(v->name, v->value, 1) < 0) {
      return -1;
    }
  }
  return 0;
}

void
vars_cleanup(void)
{
  while (var_list) {
    struct var *v = var_list;
    var_list = v->next;
    free(v->value);
    free(v);
  }
}
