// ----------------------------------------------------------------------------
// WavPack lib for Matroska
// ----------------------------------------------------------------------------
// Copyright christophe.paris@free.fr
// Parts by David Bryant http://www.wavpack.com
// Distributed under the BSD Software License
// ----------------------------------------------------------------------------

#include "..\wavpack\wavpack.h"
#include "wavpack_common.h"

// ----------------------------------------------------------------------------

#if defined(_WIN32)

#ifdef _DEBUG

#include <stdio.h>
#include <tchar.h>

void DebugLog(const char *pFormat,...) {
    char szInfo[2000];
    
    // Format the variable length parameter list
    va_list va;
    va_start(va, pFormat);
    
    _vstprintf(szInfo, pFormat, va);
    lstrcat(szInfo, TEXT("\r\n"));
    OutputDebugString(szInfo);
    
    va_end(va);
}

void DebugMemoryDump (void *data, int length, int bytes_per_line)
{
    unsigned char inbuf [48];
    char outbuf [256];
    int address = 0;
    int i;

    if (bytes_per_line > sizeof (inbuf))
        bytes_per_line = sizeof (inbuf);

    while (length) {
        if (bytes_per_line > length)
            bytes_per_line = length;

        memcpy (inbuf, data, bytes_per_line);
        sprintf (outbuf, "%04x ", address);

        for (i = 0; i < bytes_per_line; ++i)
            sprintf (outbuf + strlen (outbuf), i < bytes_per_line ? " %02x" : "   ", inbuf [i]);

        strcat (outbuf, "  ");

        for (i = 0; i < bytes_per_line; ++i)
            sprintf (outbuf + strlen (outbuf), "%c", inbuf [i] >= ' ' && inbuf [i] <= '~' ? inbuf [i] : '.');

        DebugLog ("%s", outbuf);
        (char *) data += bytes_per_line;
        length -= bytes_per_line;
        address += bytes_per_line;
    }
}

#endif

#endif

// ----------------------------------------------------------------------------
