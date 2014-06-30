#include <windows.h>
/*#include "wine/debug.h"*/

/*WINE_DEFAULT_DEBUG_CHANNEL(spilpt);*/

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    /*TRACE("(0x%p, %d, %p)\n", hinstDLL, fdwReason, lpvReserved);*/

    switch (fdwReason)
    {
#ifdef __WINESRC__
        case DLL_WINE_PREATTACH:
            return FALSE;    /* prefer native version */
#endif
        case DLL_PROCESS_ATTACH:
            DisableThreadLibraryCalls(hinstDLL);
            break;
        case DLL_PROCESS_DETACH:
            break;
    }

    return TRUE;
}
