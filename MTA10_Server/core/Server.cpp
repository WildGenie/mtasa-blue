/*****************************************************************************
*
*  PROJECT:     Multi Theft Auto v1.0
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        core/Server.cpp
*  PURPOSE:     Server core module entry
*  DEVELOPERS:  Christian Myhre Lundheim <>
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

#include "StdInc.h"
#include "CServerImpl.h"
#define ALLOC_STATS_MODULE_NAME "core"
#include "SharedUtil.hpp"
#include "SharedUtil.Tests.hpp"
#ifdef WIN32
    #include "SharedUtil.Win32Utf8FileHooks.hpp"
#endif

#if WIN32
    #define MTAEXPORT extern "C" __declspec(dllexport)
#else
    #define MTAEXPORT extern "C"
#endif

#ifdef WIN32
CThreadCommandQueue g_CommandQueue;
#endif

MTAEXPORT int Run ( int iArgumentCount, char* szArguments [] )
{
    SharedUtil_Tests ();

    #ifdef WIN32
        // Disable critical error message boxes
        SetErrorMode ( SEM_FAILCRITICALERRORS );

        // Apply file hooks if not already done by the client
        bool bSkipFileHooks = false;
        for( int i = 1 ; i < iArgumentCount ; i++ )
            bSkipFileHooks |= SStringX( szArguments[i] ).Contains( "--clientfeedback" );
        if( !bSkipFileHooks )
            AddUtf8FileHooks();
    #endif

    // Create the server
    #ifdef WIN32
        CServerImpl Server ( &g_CommandQueue );
    #else
        CServerImpl Server;
    #endif

    // Run the main func
    int iReturn;
    do
    {
        iReturn = Server.Run ( iArgumentCount, szArguments );
    }
    while ( iReturn == SERVER_RESET_RETURN );

    // Done
    #ifdef WIN32
        RemoveUtf8FileHooks();
    #endif

    return iReturn;
}

// Threadsafe way to tell the server to run a command (like the GUI would run)
#ifdef WIN32

MTAEXPORT bool SendServerCommand ( const char* szString )
{
    g_CommandQueue.Add ( szString );
    return true;
}

#endif
