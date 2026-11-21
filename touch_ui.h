/* -*- mode: c; tab-width: 4; c-basic-offset: 4; c-file-style: "linux" -*- */
//
// Copyright (c) 2009-2011, Wei Mingzhi <whistler_wmz@users.sf.net>.
// Copyright (c) 2011-2026, SDLPAL development team.
// All rights reserved.
//
// This file is part of SDLPAL.
//
// SDLPAL is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version 3
// as published by the Free Software Foundation.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
// Official-iOS-style Touch Interaction & HUD System for SDLPal
//

#ifndef TOUCH_UI_H
#define TOUCH_UI_H

#include "common.h"
#include "palcommon.h"

typedef enum tagPALTOUCHMODE
{
   kTouchModeNone = 0,       // Cutscenes / AVI (subtle Skip button)
   kTouchModeTitleMenu,      // Opening menu (direct touch only, no Back button)
   kTouchModeMap,            // Map exploration (Joystick + Search + Quick Bar + Tap-to-walk)
   kTouchModeDialog,         // Dialog / WaitForKey (Tap anywhere to advance)
   kTouchModeMenu,           // Menus / Inventory / Armory / Status / Magic / Shop (Back button + direct touch)
   kTouchModeBattleMain,     // Battle main command (Battle Quick Bar + direct touch)
   kTouchModeBattleSub,      // Battle submenus / target selection (Back button + direct touch)
   kTouchModeBattleWait      // Battle waiting / Auto-attack (Cancel Auto pill if fAutoAttack)
} PALTOUCHMODE;

typedef enum tagPALHUDACTION
{
   kHudActionNone = 0,
   kHudActionStatus,
   kHudActionMagic,
   kHudActionUseItem,
   kHudActionEquipItem,
   kHudActionSystem,
   kHudActionSearch,
   kHudActionBack,
   kHudActionBattleAuto,
   kHudActionBattleRepeat,
   kHudActionBattleDefend,
   kHudActionBattleItem,
   kHudActionBattleFlee,
   kHudActionSkip,
   kHudActionSpeed,
   kHudActionZoomOut,
   kHudActionZoomIn,
   kHudActionReveal,
   kHudActionSettings,
   kHudActionCount
} PALHUDACTION;

typedef struct tagPALHUDRECT
{
   PALHUDACTION action;
   float        x, y, w, h; // Normalized [0..1] window coordinates
} PALHUDRECT;

typedef struct tagPALTOUCHHUDSTATE
{
   PALTOUCHMODE mode;
   BOOL         fSearchAvailable;
   BOOL         fBattleAutoActive;
   INT          iSpeedIndex;
   INT          iMapZoom;
   INT          iMapZoomIndex;
   float        fMapZoom;
   BOOL         fMapPanDetached;
   BOOL         fRevealSearchables;

   BOOL         fJoyActive;
   float        fJoyCenterX, fJoyCenterY; // Normalized [0..1]
   float        fJoyKnobX, fJoyKnobY;     // Normalized [0..1]
   float        fJoyRadiusX, fJoyRadiusY; // Normalized [0..1]
   PALDIRECTION joyDir;

   PALHUDACTION pressedAction;

   BOOL         fMapTargetActive;
   int          iMapTargetWorldX;
   int          iMapTargetWorldY;
   DWORD        dwMapTargetTicks;
} PALTOUCHHUDSTATE;

PAL_C_LINKAGE_BEGIN

VOID
PAL_TouchSetMode(
   PALTOUCHMODE mode
);

PALTOUCHMODE
PAL_TouchGetMode(
   VOID
);

INT
PAL_GetSpeedIndex(
   VOID
);

INT
PAL_GetSpeedPercent(
   VOID
);

VOID
PAL_CycleSpeedMultiplier(
   VOID
);

DWORD
PAL_GetFrameTime(
   VOID
);

DWORD
PAL_GetBattleFrameTime(
   VOID
);

DWORD
PAL_ScaleDelayMs(
   DWORD ms
);

INT
PAL_GetMapZoomIndex(
   VOID
);

INT
PAL_GetMapZoom(
   VOID
);

float
PAL_GetMapZoomFloat(
   VOID
);

BOOL
PAL_IsMapPanDetached(
   VOID
);

VOID
PAL_GetMapPanCenter(
   float *pOutWX,
   float *pOutWY
);

VOID
PAL_RecenterMapCamera(
   VOID
);

VOID
PAL_CheckMapCameraSceneSync(
   VOID
);

VOID
PAL_StepMapZoom(
   INT delta
);

BOOL
PAL_IsRevealSearchablesEnabled(
   VOID
);

VOID
PAL_ToggleRevealSearchables(
   VOID
);

VOID
PAL_TouchSetZoomViewport(
   int camX,
   int camY,
   int viewW,
   int viewH
);

VOID
PAL_TouchGetZoomViewport(
   int *pCamX,
   int *pCamY,
   int *pViewW,
   int *pViewH
);

VOID
PAL_RenderZoomedMapRect(
   SDL_Surface    *lpDstSurface,
   const SDL_Rect *pWorldSubRect
);

extern BOOL g_fSceneCleanForZoom;
extern BOOL g_bInsideMakeScene;
extern BOOL g_bInCutscene;
extern BOOL g_bDrawingTextShadow;

// 32-bit Ancient-Book & Sandalwood Hextech UI Material Codes stored in uiMask
#define UIMASK_HEX_SHADOW         16
#define UIMASK_HEX_GOLD_OUTER     17
#define UIMASK_HEX_GOLD_BRIGHT    18
#define UIMASK_HEX_DARK_MOAT      19
#define UIMASK_HEX_JADE_INNER     20
#define UIMASK_HEX_GLASS_BG       21
#define UIMASK_HEX_GLASS_SHINE    22
#define UIMASK_WOOD1_BG           23
#define UIMASK_WOOD1_SHINE        24
#define UIMASK_SCROLL_OUTER       25
#define UIMASK_SCROLL_INNER       26
#define UIMASK_SCROLL_BG          27
#define UIMASK_SCROLL_SHINE       28
#define UIMASK_MAT_MAX            28
#define UIMASK_TEXT_SCROLL_INK    251
#define UIMASK_HEX_FG             252
#define UIMASK_TEXT_SHADOW        253
#define UIMASK_TEXT_LIGHT_REMAP   254

VOID
PAL_DrawHextechBoxToScreen(
   INT          boxX,
   INT          boxY,
   INT          boxW,
   INT          boxH,
   INT          nShadowOffset,
   INT          iBoxStyle
);

uint8_t *
VIDEO_GetSurfaceUiMask(
   SDL_Surface *lpSurf,
   BOOL         fCreate
);

VOID
VIDEO_ClearScreenUiMask(
   VOID
);

VOID
PAL_TouchSetSearchHint(
   BOOL fAvailable
);

VOID
PAL_TouchSetBattleAutoHint(
   BOOL fAutoActive
);

BOOL
PAL_TouchHasPendingInput(
   VOID
);

BOOL
PAL_TouchPollTap(
   INT *px,
   INT *py
);

INT
PAL_TouchPollScrollRows(
   INT iRowHeight
);

INT
PAL_TouchPollHorizSwipe(
   VOID
);

VOID
PAL_TouchClearGestures(
   VOID
);

VOID
PAL_TouchCancelMapWalk(
   VOID
);

PALDIRECTION
PAL_TouchUpdateMapWalk(
   BOOL *pfTriggerSearch
);

INT
PAL_TouchGetMapPathWaypoints(
   const SHORT **ppOutWX,
   const SHORT **ppOutWY
);

VOID
PAL_TouchGetHUDState(
   PALTOUCHHUDSTATE *pOutState
);

INT
PAL_TouchGetHUDButtons(
   PALTOUCHMODE mode,
   BOOL         fBattleAutoActive,
   float        fAspect,
   PALHUDRECT  *rgOutRects,
   INT          iMaxRects
);

#if SDL_VERSION_ATLEAST(2,0,0)
VOID
VIDEO_RenderTouchHUD(
   SDL_Renderer *renderer,
   int           winW,
   int           winH
);

VOID
VIDEO_DestroyTouchHUD(
   VOID
);
#endif

INT
PAL_BattleGetHighlightedEnemy(
   VOID
);

INT
PAL_BattleGetHighlightedPlayer(
   VOID
);

BOOL
PAL_BattleIsActionValidForUI(
   INT          iActionIdx
);

#if defined(__IOS__)
unsigned char *
UTIL_RenderHUDButtonRGBA(
   const char *utf8Text,
   int         width,
   int         height,
   int         isCircle,
   int         isHighlighted
);

unsigned char *
UTIL_RenderHUDDpadRGBA(
   int         size,
   int         activeDir
);

unsigned char *
UTIL_RenderEnemyStatsCardRGBA(
   int          width,
   int          height,
   int          hp,
   int          maxHP,
   int          level,
   int          physRes,
   int          sorcRes,
   int          poisRes,
   const int    elemRes[5],
   const int    bfEffect[5],
   LPCWSTR      stealItemName,
   int          stealCount,
   int          hadInitialSteal,
   int          dropCash,
   int          isTargeted
);

unsigned char *
UTIL_RenderHeroBattleCardRGBA(
   int                  width,
   int                  height,
   const unsigned char *portraitRGBA,
   int                  portraitW,
   int                  portraitH,
   LPCWSTR              heroName,
   int                  level,
   int                  hp,
   int                  maxHP,
   int                  mp,
   int                  maxMP,
   int                  turnState,
   int                  statusFlags,
   int                  isTargeted
);

unsigned char *
UTIL_RenderBattleSkillOrbRGBA(
   int          size,
   int          orbType,
   int          isSelected,
   int          isEnabled
);

unsigned char *
UTIL_RenderSearchTagRGBA(
   int          width,
   int          height,
   LPCWSTR      itemName,
   int          cashAmount,
   int          category
);

BOOL
UTIL_IsPerfHUDEnabled(
   VOID
);

VOID
UTIL_SetPerfHUDEnabled(
   BOOL        fEnabled
);

BOOL
UTIL_IsRightHandedDpad(
   VOID
);

VOID
UTIL_SetRightHandedDpad(
   BOOL        fRightDpad
);

BOOL
UTIL_IsEnemyStatsHUDEnabled(
   VOID
);

VOID
UTIL_SetEnemyStatsHUDEnabled(
   BOOL        fEnabled
);

unsigned char *
UTIL_RenderPerfHUDBadgeRGBA(
   const char *utf8Text,
   int         width,
   int         height
);

INT
UTIL_ShowSystemSettingsModal(
   BOOL        fInGame,
   int         currentMusicNum
);
#endif

PAL_C_LINKAGE_END

#endif
