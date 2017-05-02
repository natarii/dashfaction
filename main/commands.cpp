#include "stdafx.h"
#include "commands.h"
#include "rf.h"
#include "utils.h"
#include "hud.h"
#include "lazyban.h"
#include "main.h"
#include "BuildConfig.h"
#include "spectate_mode.h"
#include "main.h"
#include "misc.h"
#include "packfile.h"
#include "hooks/MemChange.h"

using namespace rf;

// Note: limit should fit in int8_t
constexpr int CMD_LIMIT = 127;

static DcCommand *g_CommandsBuffer[CMD_LIMIT];

auto DcAutoCompleteInput_Hook = makeFunHook(DcAutoCompleteInput);

#if SPLITSCREEN_ENABLE

static void SplitScreenCmdHandler(void)
{
    if (*g_pbDcRun)
    {
        if (*g_pbNetworkGame)
            SplitScreenStart(); /* FIXME: set player 2 controls */
        else
            DcPrintf("Works only in multiplayer game!");
    }
}

#endif // SPLITSCREEN_ENABLE

static void MaxFpsCmdHandler(void)
{
    if (*g_pbDcRun)
    {
        rf::DcGetArg(DC_ARG_NONE | DC_ARG_FLOAT, 0);

        if ((*g_pDcArgType & DC_ARG_FLOAT))
        {
#ifdef NDEBUG
            float newLimit = std::min(std::max(*g_pfDcArg, (float)MIN_FPS_LIMIT), (float)MAX_FPS_LIMIT);
#else
            float newLimit = *g_pfDcArg;
#endif
            g_gameConfig.maxFps = (unsigned)newLimit;
            g_gameConfig.save();
            *g_pfMinFramerate = 1.0f / newLimit;
        }
        else
            DcPrintf("Maximal FPS: %.1f", 1.0f / *g_pfMinFramerate);
    }
    
    if (*g_pbDcHelp)
    {
        DcPrint(g_ppszStringsTable[STR_USAGE], NULL);
        DcPrint("     maxfps <limit>", NULL);
    }
}

#ifndef NDEBUG
static void DebugCmdHandler(void)
{
    bool bDbg = !g_pbDbgFlagsArray[0];
    memset((char*)g_pbDbgFlagsArray, bDbg, 9);
    *g_pbRenderEventIcons = bDbg;
    *g_pbDbgNetwork = bDbg;
    *g_pbDbgWeapon = bDbg;
    *g_pbDbgRenderTriggers = bDbg;
}
#endif

#if SPECTATE_MODE_ENABLE

static void SpectateCmdHandler(void)
{
    if (*g_pbDcRun)
    {
        if (*g_pbNetworkGame)
        {
            rf::CPlayer *pPlayer;
            rf::DcGetArg(DC_ARG_NONE | DC_ARG_STR, 0);
            if (*g_pDcArgType & DC_ARG_STR)
            {
                pPlayer = FindPlayer(g_pszDcArg);
                if (!pPlayer)
                    DcPrintf("Cannot find player: %s", g_pszDcArg);
            }
            else {
                DcPrintf("Expected player name.");
                pPlayer = *g_ppLocalPlayer;
            }
            
            if (pPlayer)
                SpectateModeSetTargetPlayer(pPlayer);
        } else
            DcPrint("Works only in multiplayer game!", NULL);
    }
    
    if (*g_pbDcHelp)
    {
        DcPrint(g_ppszStringsTable[STR_USAGE], NULL);
        DcPrintf("     spectate <%s>", g_ppszStringsTable[STR_PLAYER_NAME]);
    }
}

#endif // SPECTATE_MODE_ENABLE

#if MULTISAMPLING_SUPPORT
static void AntiAliasingCmdHandler(void)
{
    if (*g_pbDcRun)
    {
        if (!g_gameConfig.msaa)
            DcPrintf("Anti-aliasing is not supported");
        else
        {
            DWORD Enabled = FALSE;
            IDirect3DDevice8_GetRenderState(*g_ppGrDevice, D3DRS_MULTISAMPLEANTIALIAS, &Enabled);
            Enabled = !Enabled;
            IDirect3DDevice8_SetRenderState(*g_ppGrDevice, D3DRS_MULTISAMPLEANTIALIAS, Enabled);
            DcPrintf("Anti-aliasing is %s", Enabled ? "enabled" : "disabled");
        }
    }
}
#endif // MULTISAMPLING_SUPPORT

#if DIRECTINPUT_SUPPORT
static void InputModeCmdHandler(void)
{
    if (*g_pbDcRun)
    {
        *g_pbDirectInputDisabled = !*g_pbDirectInputDisabled;
        if (*g_pbDirectInputDisabled)
            DcPrintf("DirectInput is disabled");
        else
            DcPrintf("DirectInput is enabled");
    }
}
#endif // DIRECTINPUT_SUPPORT

#if CAMERA_1_3_COMMANDS

static int CanPlayerFireHook(CPlayer *pPlayer)
{
    if (!(pPlayer->Flags & 0x10))
        return 0;
    if (*g_pbNetworkGame && (pPlayer->pCamera->Type == rf::CAM_FREELOOK || pPlayer->pCamera->pPlayer != pPlayer))
        return 0;
    return 1;
}

#endif // if CAMERA_1_3_COMMANDS

static void MouseSensitivityCmdHandler(void)
{
    if (*g_pbDcRun)
    {
        rf::DcGetArg(DC_ARG_NONE | DC_ARG_FLOAT, 0);

        if (*g_pDcArgType & DC_ARG_FLOAT)
        {
            float fValue = *g_pfDcArg;
            fValue = clamp(fValue, 0.0f, 1.0f);
            (*g_ppLocalPlayer)->Config.Controls.fMouseSensitivity = fValue;
        }
        DcPrintf("Mouse sensitivity: %.2f", (*g_ppLocalPlayer)->Config.Controls.fMouseSensitivity);
    }

    if (*g_pbDcHelp)
    {
        DcPrint(g_ppszStringsTable[STR_USAGE], NULL);
        DcPrint("     ms <value>", NULL);
    }
}

static void VolumeLightsCmdHandler(void)
{
    if (*g_pbDcRun)
    {
        static MemChange vliCallChange(0x0043233E);
        if (vliCallChange.IsApplied())
            vliCallChange.Revert();
        else
            vliCallChange.Write("\x90\x90\x90\x90\x90", 5);
        DcPrintf("Volumetric lightining is %s.", vliCallChange.IsApplied() ? "disabled" : "enabled");
    }
}

static void LevelSpCmdHandler(void)
{
    if (*g_pbDcRun)
    {
        rf::DcGetArg(DC_ARG_STR, 0);

        if ((*g_pDcArgType & DC_ARG_STR))
        {
            if (*g_pbNetworkGame)
            {
                DcPrintf("You cannot use it in multiplayer game!");
                return;
            }
            CString strUnk, strLevel;
            CString_Init(&strUnk, "");
            CString_Init(&strLevel, g_pszDcArg);
            DcPrintf("Loading level");
            SetNextLevelFilename(strLevel, strUnk);
            SwitchMenu(5, 0);
        }
    }

    if (*g_pbDcHelp)
    {
        DcPrint(g_ppszStringsTable[STR_USAGE], NULL);
        DcPrint("     <rfl_name>", NULL);
    }
}

static void LevelSoundsCmdHandler(void)
{
    if (*g_pbDcRun)
    {
        rf::DcGetArg(DC_ARG_FLOAT | DC_ARG_NONE, 0);

        if ((*g_pDcArgType & DC_ARG_FLOAT))
        {
            float fVolScale = clamp(*g_pfDcArg, 0.0f, 1.0f);
            SetLevelSoundVolumeScale(fVolScale);

            g_gameConfig.levelSoundVolume = fVolScale;
            g_gameConfig.save();
        }
        DcPrintf("Level sound volume: %.1f", g_gameConfig.levelSoundVolume);
    }

    if (*g_pbDcHelp)
    {
        DcPrint(g_ppszStringsTable[STR_USAGE], NULL);
        DcPrint("     <volume>", NULL);
    }
}

static void DcfFindMap()
{
    if (*g_pbDcRun)
    {
        rf::DcGetArg(DC_ARG_STR, 0);

        if (*g_pDcArgType & DC_ARG_STR)
        {
            PackfileFindMatchingFiles(g_pszDcArg, ".rfl");
        }
    }

    if (*g_pbDcHelp)
    {
        DcPrint(g_ppszStringsTable[STR_USAGE], NULL);
        DcPrint("     <query>", NULL);
    }
}

DcCommand g_Commands[] = {
#if SPLITSCREEN_ENABLE
    {"splitscreen", "Starts split screen mode", SplitScreenCmdHandler},
#endif
    {"maxfps", "Sets maximal FPS", MaxFpsCmdHandler},
    {"hud", "Show and hide HUD", HudCmdHandler},
#ifndef NDEBUG
    {"debug", "Switches debugging in RF", DebugCmdHandler},
#endif
#if SPECTATE_MODE_ENABLE
    {"spectate", "Starts spectating mode", SpectateCmdHandler},
#endif
#if MULTISAMPLING_SUPPORT
    { "antialiasing", "Toggles anti-aliasing", AntiAliasingCmdHandler },
#endif
#if DIRECTINPUT_SUPPORT
    { "inputmode", "Toggles input mode", InputModeCmdHandler },
#endif
    { "unban_last", "Unbans last banned player", UnbanLastCmdHandler },
    { "ms", "Sets mouse sensitivity", MouseSensitivityCmdHandler },
    { "vli", "Toggles volumetric lightining", VolumeLightsCmdHandler },
    { "levelsp", "Loads single player level", LevelSpCmdHandler },
    { "levelsounds", "Sets level sounds volume scale", LevelSoundsCmdHandler },
};

void DcAutoCompleteInput_New()
{
    const char *pszArgs = strchr(g_szDcCmdLine, ' ');
    size_t CmdLen = pszArgs ? pszArgs - g_szDcCmdLine : *g_pcchDcCmdLineLen;

    std::vector<DcCommand*> MatchingCmds;
    for (unsigned i = 0; i < *g_pDcNumCommands; ++i)
    {
        DcCommand *pCmd = g_CommandsBuffer[i];
        if (!strnicmp(pCmd->pszCmd, g_szDcCmdLine, CmdLen) && (!pszArgs || !pCmd->pszCmd[CmdLen]))
            MatchingCmds.push_back(pCmd);
    }

    if (pszArgs)
    {
        if (MatchingCmds.size() == 1)
        {
            *g_pbDcRun = 0;
            *g_pbDcHelp = 1;
            *g_pbDcStatus = 0;
            MatchingCmds[0]->pfnHandler();
        }
    }
    else if (MatchingCmds.size() > 1 && *g_pcchDcCmdLineLen > 0)
    {
        for (auto *pCmd : MatchingCmds)
            DcPrintf("%s - %s", pCmd->pszCmd, pCmd->pszDescr);
    }
    else if (MatchingCmds.size() == 1)
        *g_pcchDcCmdLineLen = sprintf(g_szDcCmdLine, "%s ", MatchingCmds[0]->pszCmd);
}

void CommandsInit(void)
{
#if CAMERA_1_3_COMMANDS
    /* Enable camera1-3 in multiplayer and hook CanPlayerFire to disable shooting in camera2 */
    WriteMemUInt8(0x00431280, ASM_NOP, 2);
    WriteMemUInt8(0x004312E0, ASM_NOP, 2);
    WriteMemUInt8(0x00431340, ASM_NOP, 2);
    WriteMemUInt8(0x004A68D0, ASM_LONG_JMP_REL);
    WriteMemUInt32(0x004A68D0 + 1, (uintptr_t)CanPlayerFireHook - (0x004A68D0 + 0x5));
#endif // if CAMERA_1_3_COMMANDS

    // Change limit of commands
    ASSERT(*g_pDcNumCommands == 0);
    WriteMemPtr(0x005099AC + 1, g_CommandsBuffer);
    WriteMemUInt8(0x00509A78 + 2, CMD_LIMIT);
    WriteMemPtr(0x00509A8A + 1, g_CommandsBuffer);
    WriteMemPtr(0x00509AB0 + 3, g_CommandsBuffer);
    WriteMemPtr(0x00509AE1 + 3, g_CommandsBuffer);
    WriteMemPtr(0x00509AF5 + 3, g_CommandsBuffer);
    WriteMemPtr(0x00509C8F + 1, g_CommandsBuffer);
    WriteMemPtr(0x00509DB4 + 3, g_CommandsBuffer);
    WriteMemPtr(0x00509E6F + 1, g_CommandsBuffer);
    WriteMemPtr(0x0050A648 + 4, g_CommandsBuffer);
    WriteMemPtr(0x0050A6A0 + 3, g_CommandsBuffer);

    // Better console autocomplete
    DcAutoCompleteInput_Hook.hook(DcAutoCompleteInput_New);
}

void CommandRegister(DcCommand *pCmd)
{
    if (*g_pDcNumCommands < CMD_LIMIT)
        g_CommandsBuffer[(*g_pDcNumCommands)++] = pCmd;
    else
        ASSERT(false);

    // Register some unused builtin commands
    DC_REGISTER_CMD(kill_limit, "Sets kill limit", DcfKillLimit);
    DC_REGISTER_CMD(time_limit, "Sets time limit", DcfTimeLimit);
    DC_REGISTER_CMD(geomod_limit, "Sets geomod limit", DcfGeomodLimit);
    DC_REGISTER_CMD(capture_limit, "Sets capture limit", DcfCaptureLimit);

    DC_REGISTER_CMD(sound, "Toggle sound", DcfSound);
    DC_REGISTER_CMD(difficulty, "Set game difficulty", DcfDifficulty);
    //DC_REGISTER_CMD(ms, "Set mouse sensitivity", DcfMouseSensitivity);
    DC_REGISTER_CMD(level_info, "Show level info", DcfLevelInfo);
    DC_REGISTER_CMD(verify_level, "Verify level", DcfVerifyLevel);
    DC_REGISTER_CMD(player_names, "Toggle player names on HUD", DcfPlayerNames);
    DC_REGISTER_CMD(clients_count, "Show number of connected clients", DcfClientsCount);
    DC_REGISTER_CMD(kick_all, "Kick all clients", DcfKickAll);
    DC_REGISTER_CMD(timedemo, "Start timedemo", DcfTimedemo);
    DC_REGISTER_CMD(frameratetest, "Start frame rate test", DcfFramerateTest);
    DC_REGISTER_CMD(system_info, "Show system information", DcfSystemInfo);
    DC_REGISTER_CMD(trilinear_filtering, "Toggle trilinear filtering", DcfTrilinearFiltering);
    DC_REGISTER_CMD(detail_textures, "Toggle detail textures", DcfDetailTextures);

    // New commands
    DC_REGISTER_CMD(findmap, "Find map by filename fragment", DcfFindMap);

    
}

void CommandsAfterGameInit()
{
    unsigned i;

    /* Add commands */
    for (i = 0; i < COUNTOF(g_Commands); ++i)
        CommandRegister(&g_Commands[i]);
}
