#ifndef _COMPAT_H
#define _COMPAT_H

#ifdef __WINE__
# define stricmp strcasecmp
#endif

#ifdef COMPAT_TIMER_MACROS
/* MinGW has timerisset(), timercmp(), timerclear() defined, but not timeradd()
 * and timersub(). */

/* From glibc's sys/time.h */
/* Convenience macros for operations on timevals.
   NOTE: `timercmp' does not work for >= or <=.  */
#ifndef timerisset
# define timerisset(tvp)    ((tvp)->tv_sec || (tvp)->tv_usec)
#endif
#ifndef timerclear
# define timerclear(tvp)    ((tvp)->tv_sec = (tvp)->tv_usec = 0)
#endif
#ifndef timercmp
# define timercmp(a, b, CMP)                              \
  (((a)->tv_sec == (b)->tv_sec) ?                         \
   ((a)->tv_usec CMP (b)->tv_usec) :                          \
   ((a)->tv_sec CMP (b)->tv_sec))
#endif
#ifndef timeradd
# define timeradd(a, b, result)                           \
  do {                                        \
    (result)->tv_sec = (a)->tv_sec + (b)->tv_sec;                 \
    (result)->tv_usec = (a)->tv_usec + (b)->tv_usec;                  \
    if ((result)->tv_usec >= 1000000)                         \
      {                                       \
    ++(result)->tv_sec;                           \
    (result)->tv_usec -= 1000000;                         \
      }                                       \
  } while (0)
#endif
#ifndef timersub
# define timersub(a, b, result)                           \
  do {                                        \
    (result)->tv_sec = (a)->tv_sec - (b)->tv_sec;                 \
    (result)->tv_usec = (a)->tv_usec - (b)->tv_usec;                  \
    if ((result)->tv_usec < 0) {                          \
      --(result)->tv_sec;                             \
      (result)->tv_usec += 1000000;                       \
    }                                         \
  } while (0)
#endif
#endif /* COMPAT_TIMER_MACROS */

#endif /* _COMPAT_H */
