#ifndef BASICS_H
#define BASICS_H
#include <windows.h>
#include "spifns.h"

#ifdef DLLEXPORT
# undef DLLEXPORT
#endif

#ifdef __WINE__
# define DLLEXPORT  extern "C"
#else
# define DLLEXPORT  /* Empty */
#endif

#endif//BASICS_H
