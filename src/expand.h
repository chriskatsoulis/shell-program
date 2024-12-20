#pragma once

/** tilde expansion, parameter expansion, and quote removal
 *
 * @param [in,out]word modified in place.
 * @returns *word, or null on failure
 *
 * word must refer to an object that was dynamically allocated,
 * as with malloc():
 * e.g.
 *      char *s = strdup("~/");
 *      expand(&s);    // OK
 *  
 *  vs.
 *
 *      char *s = "~/";
 *      expand(&s)     // ERROR
 *
 */
extern char *expand(char **word);
extern char *expand_prompt(char **word);

