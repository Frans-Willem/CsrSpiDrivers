#include <windows.h>
#include "spi.h"

typedef uint32_t (pttrans_get_version_t)(void);

/* Public variable */
uint32_t pttrans_api_version = 0;

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    HMODULE pttdll;
    pttrans_get_version_t *pttrans_get_version;

    switch (fdwReason)
    {
        case DLL_PROCESS_ATTACH:
            DisableThreadLibraryCalls(hinstDLL);

            /* Detect SPI API version by calling a function from pttransport.dll */
            if ((pttdll = GetModuleHandle("pttransport.dll"))) {
                if ((pttrans_get_version = (pttrans_get_version_t *)GetProcAddress(pttdll,
                                "pttrans_get_version")))
                {
                    pttrans_api_version = pttrans_get_version();
                    pttrans_get_version = NULL;
                    pttdll = NULL;
                }
            }
            break;
        case DLL_PROCESS_DETACH:
            spi_deinit();
            break;
    }

    return TRUE;
}
