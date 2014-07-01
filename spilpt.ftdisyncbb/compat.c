#ifdef COMPAT_USLEEP
/*
 * http://stackoverflow.com/questions/5801813/c-usleep-is-obsolete-workarounds-for-windows-mingw
 */
#include <windows.h>
#include <stdint.h>

void usleep(int64_t usec) 
{ 
    HANDLE timer; 
    LARGE_INTEGER ft; 

    ft.QuadPart = -(10*usec); /* Convert to 100 nanosecond interval, negative value indicates relative time */

    timer = CreateWaitableTimer(NULL, TRUE, NULL); 
    SetWaitableTimer(timer, &ft, 0, NULL, NULL, 0); 
    WaitForSingleObject(timer, INFINITE); 
    CloseHandle(timer); 
}
#endif /* COMPAT_USLEEP */


#ifdef COMPAT_MINGW_MS_VSNPRINTF
/*
 * This fixes linking with pre-compiled libusb-1.0.19-rc1-win: "undefined
 * reference to __ms_vsnprintf". See:
 * http://sourceforge.net/p/mingw-w64/mailman/mingw-w64-public/thread/4F8CA26A.70103@users.sourceforge.net/
 */

#include <stdio.h>
#include <stdarg.h>

int __ms_vsnprintf(char *str, size_t size, const char *format, va_list ap)
{
    return vsnprintf(str, size, format, ap);
}
#endif /* COMPAT_MINGW_MS_VSNPRINTF */
