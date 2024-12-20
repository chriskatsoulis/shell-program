/* This code handles all variable and other expansions that a shell is supposed
 * to do, for you. Refer to expand.h for the interface.
 */
#define _POSIX_C_SOURCE 200809L
#include <ctype.h>
#include <err.h>
#include <limits.h>
#include <pwd.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include "params.h"
#include "util/asprintf.h"
#include "vars.h"

#include "expand.h"

static char *
strchrnul(char const *s, int c)
{
  for (; *s && *s != c; ++s);
  return (char *)s;
}

static char *
expand_substr(char **word, char **start, char **stop, char const *expansion)
{
  char *end = *stop;
  for (; *end; ++end);

  size_t wlen = *start - *word + end - *stop;
  size_t elen = strlen(expansion);

  char *w = malloc(wlen + elen + 1);
  if (!w) goto out;

  memcpy(w, *word, *start - *word);
  memcpy(w + (*start - *word), expansion, elen);
  memcpy(w + (*start - *word) + elen, *stop, end - *stop + 1);
  *stop = w + (*start - *word) + elen;
  *start = w + (*start - *word);
  free(*word);
  *word = w;
out:
  return w;
}

char *
expand_tilde(char **word)
{
  char *w = *word;
  if (*w != '~') return w;

  // start with string end. this covers "~", "~<user>"
  char *end = strchr(w, '\0');

  // if there's a slash, use that as expansion end instead
  // this covers "~/hello/cs374" and "~<user>/hello/cs374"
  char *slash = strchr(w, '/');
  if (slash) {
    end = slash;
  }

  char const *path = 0;
  if (end == w + 1) {
    /* Special case use HOME env variable */
    path = vars_get("HOME");
    if (!path) {
      struct passwd *pw = getpwuid(getuid());
      if (!pw) goto out; /* we tried */
      path = pw->pw_dir;
    }
  } else {
    /* General case, ~<username>/... */
    char *nam = strndup(w + 1, end - w - 1);
    if (!nam) err(1, 0);
    struct passwd *pw = getpwnam(nam);
    free(nam);
    if (!pw) goto out;
    path = pw->pw_dir;
  }
  w = expand_substr(word, &w, &end, path);
out:
  return w;
}

static char *
find_unquoted(char const *haystack, int needle)
{
  char const *c = haystack;
  for (; *c; (void)(*c && ++c)) {
    if (*c == needle) return (char *)c;

    if (*c == '\\') {
      ++c;
      continue;
    }

    if (*c == '\'') {
      c = strchrnul(c + 1, '\'');
      continue;
    }
    if (*c == '\"') {
      for (; *c; (void)(*c && ++c)) {
        if (*c == '\"') break;
        if (*c == '\\') {
          if (needle == '\\') return (char *)c;
          ++c;
        }
        if (*c == '$') {
          if (needle == '$') return (char *)c;
        }
      }
    }
    continue;
  }
  return 0;
}

static char *
expand_parameters(char **word)
{
  char *scan = *word;
  char *w = *word;
  for (;;) {
    scan = find_unquoted(scan, '$');
    if (!scan) break;

    char *expand_start = scan;
    ++scan;
    char *param;
    if (*scan == '$') {
      ++scan;
      char *val = 0;
      asprintf(&val, "%jd", (intmax_t)getpid());
      if (val) {
        w = expand_substr(word, &expand_start, &scan, val);
        free(val);
      }
    } else if (*scan == '!') {
      ++scan;
      char *val = 0;
      asprintf(&val, "%jd", (intmax_t)params.bg_pid);
      if (val) {
        w = expand_substr(word, &expand_start, &scan, val);
        free(val);
      }
      ++scan;
    } else if (*scan == '?') {
      ++scan;
      char *val = 0;
      asprintf(&val, "%d", params.status);
      if (val) {
        w = expand_substr(word, &expand_start, &scan, val);
        free(val);
      }
      ++scan;
    } else {
      if (*scan == '{') {
        param = scan + 1;
        for (; *scan && *scan != '}'; ++scan);
        if (*scan != '}') return *word;
        param = strndup(param, scan - param);
        ++scan;
        if (!param) err(1, 0);
      } else {
        param = scan;
        for (; *scan && (isalpha(*scan) || isdigit(*scan) || *scan == '_');
             ++scan);
        if (scan == param) continue; 
        param = strndup(param, scan - param);
        if (!param) err(1, 0);
      }

      char *expand_end = scan;
      char const *val = vars_get(param);
      if (!val) val = "";
      w = expand_substr(word, &expand_start, &expand_end, val);
      scan = expand_end;
      free(param);
    }
    if (!w) break;
  }
  return w;
}

static char *
remove_quotes(char **word)
{
  char *in = *word;
  char *out = *word;

  for (; *in; (void)(*in && ++in)) {
    if (*in == '\\') {
      ++in;
      if (in) {
        *out++ = *in;
      }
      continue;
    }
    if (*in == '\'') {
      ++in;
      for (; *in && *in != '\''; ++in) {
        *out++ = *in;
      }
      ++in;
      continue;
    }
    if (*in == '"') {
      ++in;
      for (; *in && *in != '"'; ++in) {
        if (*in == '\\') {
          ++in;
        }
        *out++ = *in;
      }
      ++in;
      continue;
    }
    *out++ = *in;
  }
  *out = 0;
  return *word;
}

char *
expand(char **word)
{
  if (!expand_tilde(word) || !expand_parameters(word) || !remove_quotes(word))
    return 0;
  return *word;
}

static char *
remove_prefix(char const *s, char const *pre)
{
  char const *scan = s;
  for (; *pre && *pre == *scan; ++scan, ++pre);
  if (!*pre) s = scan;
  return (char *)s;
}

char *
expand_prompt(char **prompt)
{
  char *p = *prompt;
  p = expand_parameters(prompt);
  if (!p) return 0;
  for (char *start = *prompt; *(start = strchrnul(start, '\\'));) {
    char *stop = start + 2;
    switch (start[1]) {
      case 'a':
        p = expand_substr(prompt, &start, &stop, "\a");
        break;
      case 'd':
      case 'D':
        break;
      case 'e':
        p = expand_substr(prompt, &start, &stop, "\033");
        break;
      case 'h': {
        char hn[HOST_NAME_MAX + 1] = {0};
        if (gethostname(hn, HOST_NAME_MAX + 1) == 0) {
          *strchrnul(hn, '.') = '\0';
          p = expand_substr(prompt, &start, &stop, hn);
        }
        break;
      }
      case 'H': {
        char hn[HOST_NAME_MAX + 1] = {0};
        if (gethostname(hn, HOST_NAME_MAX + 1) == 0) {
          p = expand_substr(prompt, &start, &stop, hn);
        }
        break;
      }
      case 'n':
        p = expand_substr(prompt, &start, &stop, "\n");
        break;
      case 'u': {
        struct passwd *pw = getpwuid(getuid());
        if (pw) {
          p = expand_substr(prompt, &start, &stop, pw->pw_name);
        }
        break;
      }
      case 'w': {
        char const *pwd = vars_get("PWD");
        char const *home = vars_get("HOME");
        if (pwd) {
          if (home) {
            // If $PWD starts with $HOME, compress it into ~
            if (strncmp(pwd, home, strlen(home)) == 0) {
              pwd = remove_prefix(pwd, home);
              p = expand_substr(prompt, &start, &stop, "~");
              ++start;
              if (!p) break;
            }
          }
          p = expand_substr(prompt, &start, &stop, pwd);
        }
        break;
      }
      case '$':
        if (geteuid() == 0) {
          p = expand_substr(prompt, &start, &stop, "#");
        } else {
          p = expand_substr(prompt, &start, &stop, "$");
        }
        break;
      case '\\':
        p = expand_substr(prompt, &start, &stop, "\\");
        break;
      case '[':
      case ']':
        p = expand_substr(prompt, &start, &stop, "");
        break;
    }
    start = stop;
    if (!p) break;
  }
  return p;
}
