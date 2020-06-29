/*
 * This is *not* an unused header file; instead, it allows to declare
 * parameters as being unused.
 *
 * Inspired by https://jmmv.dev/2015/02/unused-parameters-in-c-and-c.html
 * and https://github.com/jmmv/kyua/blob/244eb9/utils/defs.hpp.in#L57
 */
#define UNUSED_ATTRIBUTE	__attribute__((unused))
#define UNUSED_PARAM(name)	unused_ ## name UNUSED_ATTRIBUTE
