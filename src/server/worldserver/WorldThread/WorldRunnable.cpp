/*
 * Copyright (C) 2008-2013 TrinityCore <http://www.trinitycore.org/>
 * Copyright (C) 2013 Project Cerberus <http://www.erabattle.ru/>
 * Copyright (C) 2005-2009 MaNGOS <http://getmangos.com/>
 *
 * This program is not free software; you can not redistribute it and/or modify it.
 *
 * This program is distributed only by <http://www.erabattle.ru/>!
 */

/** \file
    \ingroup Trinityd
*/

#include "Common.h"
#include "ObjectAccessor.h"
#include "World.h"
#include "WorldSocketMgr.h"
#include "Database/DatabaseEnv.h"
#include "ScriptMgr.h"
#include "BattlegroundMgr.h"
#include "MapManager.h"
#include "Timer.h"
#include "WorldRunnable.h"
#include "OutdoorPvPMgr.h"

#define WORLD_SLEEP_CONST 50

#ifdef _WIN32
#include "ServiceWin32.h"
extern int m_ServiceStatus;
#endif

/// Heartbeat for the World
void WorldRunnable::run()
{
    uint32 realCurrTime = 0;
    uint32 realPrevTime = getMSTime();

    uint32 prevSleepTime = 0;                               // used for balanced full tick time length near WORLD_SLEEP_CONST

    sScriptMgr->OnStartup();

    ///- While we have not World::m_stopEvent, update the world
    while (!World::IsStopped())
    {
        ++World::m_worldLoopCounter;
        realCurrTime = getMSTime();

        uint32 diff = getMSTimeDiff(realPrevTime, realCurrTime);

        sWorld->Update( diff );
        realPrevTime = realCurrTime;

        // diff (D0) include time of previous sleep (d0) + tick time (t0)
        // we want that next d1 + t1 == WORLD_SLEEP_CONST
        // we can't know next t1 and then can use (t0 + d1) == WORLD_SLEEP_CONST requirement
        // d1 = WORLD_SLEEP_CONST - t0 = WORLD_SLEEP_CONST - (D0 - d0) = WORLD_SLEEP_CONST + d0 - D0
        if (diff <= WORLD_SLEEP_CONST+prevSleepTime)
        {
            prevSleepTime = WORLD_SLEEP_CONST+prevSleepTime-diff;
            ACE_Based::Thread::Sleep(prevSleepTime);
        }
        else
            prevSleepTime = 0;

        #ifdef _WIN32
            if (m_ServiceStatus == 0)
                World::StopNow(SHUTDOWN_EXIT_CODE);

            while (m_ServiceStatus == 2)
                Sleep(1000);
        #endif
    }

    sScriptMgr->OnShutdown();

    sWorld->KickAll();                                       // save and kick all players
    sWorld->UpdateSessions( 1 );                             // real players unload required UpdateSessions call

    // unload battleground templates before different singletons destroyed
    sBattlegroundMgr->DeleteAllBattlegrounds();

    sWorldSocketMgr->StopNetwork();

    sMapMgr->UnloadAll();                     // unload all grids (including locked in memory)
    sObjectAccessor->UnloadAll();             // unload 'i_player2corpse' storage and remove from world
    sScriptMgr->Unload();
    sOutdoorPvPMgr->Die();
}
