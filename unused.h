// Inspired by http://julio.meroh.net/2015/02/unused-parameters-in-c-and-c.html
// and https://github.com/jmmv/kyua/blob/master/utils/defs.hpp.in
#define UNUSED_ATTRIBUTE	__attribute__((unused))
#define UNUSED_PARAM(name)	unused_ ## name UNUSED_ATTRIBUTE
