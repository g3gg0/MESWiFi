#ifndef __MACROS_H__
#define __MACROS_H__

#define MIN(a, b) ({ __typeof__ (a) _a = (a); __typeof__ (b) _b = (b); _a < _b ? _a : _b; })
#define MAX(a, b) ({ __typeof__ (a) _a = (a); __typeof__ (b) _b = (b); _a > _b ? _a : _b; })
#define COERCE(val, min, max) (MIN(max,MIN(min,val)))

#define xstr(s) str(s)
#define str(s) #s

#endif
