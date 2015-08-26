#ifdef COMPAT_MINGW_MS_VSNPRINTF
/*
 * This fixes linking with precompiled libusb-1.0.18-win and
 * libusb-1.0.19-rc1-win: "undefined reference to __ms_vsnprintf". See:
 * http://sourceforge.net/p/mingw-w64/mailman/mingw-w64-public/thread/4F8CA26A.70103@users.sourceforge.net/
 */

#include <stdio.h>
#include <stdarg.h>

int __cdecl __ms_vsnprintf(char * __restrict__ d,size_t n,const char * __restrict__ format,va_list arg)
{
    return vsnprintf(d, n, format, arg);
}
#endif /* COMPAT_MINGW_MS_VSNPRINTF */
