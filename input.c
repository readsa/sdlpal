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

#include "main.h"
#include <math.h>

volatile PALINPUTSTATE   g_InputState;
#if PAL_HAS_JOYSTICKS
static SDL_Joystick     *g_pJoy = NULL;
#endif

#if !SDL_VERSION_ATLEAST(2,0,0)
# define SDLK_KP_1     SDLK_KP1
# define SDLK_KP_2     SDLK_KP2
# define SDLK_KP_3     SDLK_KP3
# define SDLK_KP_4     SDLK_KP4
# define SDLK_KP_5     SDLK_KP5
# define SDLK_KP_6     SDLK_KP6
# define SDLK_KP_7     SDLK_KP7
# define SDLK_KP_8     SDLK_KP8
# define SDLK_KP_9     SDLK_KP9
# define SDLK_KP_0     SDLK_KP0

# define SDL_JoystickNameForIndex    SDL_JoystickName
# define SDL_GetKeyboardState        SDL_GetKeyState
# define SDL_GetScancodeFromKey(x)   (x)
#endif

BOOL                     g_fUseJoystick = TRUE;

static void _default_init_filter() {}
static int _default_input_event_filter(const SDL_Event *event, volatile PALINPUTSTATE *state) { return 0; }
static void _default_input_shutdown_filter() {}

static void (*input_init_filter)() = _default_init_filter;
static int (*input_event_filter)(const SDL_Event *, volatile PALINPUTSTATE *) = _default_input_event_filter;
static void (*input_shutdown_filter)() = _default_input_shutdown_filter;

static const int g_KeyMap[][2] = {
   { SDLK_UP,        kKeyUp },
   { SDLK_KP_8,      kKeyUp },
   { SDLK_DOWN,      kKeyDown },
   { SDLK_KP_2,      kKeyDown },
   { SDLK_LEFT,      kKeyLeft },
   { SDLK_KP_4,      kKeyLeft },
   { SDLK_RIGHT,     kKeyRight },
   { SDLK_KP_6,      kKeyRight },
   { SDLK_ESCAPE,    kKeyMenu },
   { SDLK_INSERT,    kKeyMenu },
   { SDLK_LALT,      kKeyMenu },
   { SDLK_RALT,      kKeyMenu },
   { SDLK_KP_0,      kKeyMenu },
   { SDLK_RETURN,    kKeySearch },
   { SDLK_SPACE,     kKeySearch },
   { SDLK_KP_ENTER,  kKeySearch },
   { SDLK_LCTRL,     kKeySearch },
   { SDLK_PAGEUP,    kKeyPgUp },
   { SDLK_KP_9,      kKeyPgUp },
   { SDLK_PAGEDOWN,  kKeyPgDn },
   { SDLK_KP_3,      kKeyPgDn },
   { SDLK_HOME,      kKeyHome },
   { SDLK_KP_7,      kKeyHome },
   { SDLK_END,       kKeyEnd },
   { SDLK_KP_1,      kKeyEnd },
   { SDLK_r,         kKeyRepeat },
   { SDLK_a,         kKeyAuto },
   { SDLK_d,         kKeyDefend },
   { SDLK_e,         kKeyUseItem },
   { SDLK_w,         kKeyThrowItem },
   { SDLK_q,         kKeyFlee },
   { SDLK_f,         kKeyForce },
   { SDLK_s,         kKeyStatus }
};

#if PAL_HAS_JOYSTICKS
static VOID
PAL_DetectJoystick(
    VOID
)
/*++
  Purpose:

    Detect the joystick.

  Parameters:

    None.

  Return value:

    None.

--*/
{
#if SDL_VERSION_ATLEAST(3,0,0)
    int numjoysticks;
    SDL_JoystickID* joysticks = SDL_GetJoysticks(&numjoysticks);
    if (numjoysticks > 0 && g_fUseJoystick)
    {
        int i;
        for (i = 0; i < numjoysticks; i++)
        {
            SDL_JoystickID instance_id = joysticks[i];
            if (PAL_IS_VALID_JOYSTICK(SDL_GetJoystickNameForID(instance_id)))
            {
                g_pJoy = SDL_OpenJoystick(instance_id);
                break;
            }
        }
        UTIL_LogOutput(LOGLEVEL_DEBUG, "PAL_DetectJoystick: %d joysticks found; choose %s\n", numjoysticks, SDL_GetJoystickName(g_pJoy));
        UTIL_LogOutput(LOGLEVEL_DEBUG, "This joystick has %d axes, %d hats, %d balls, and %d buttons\n",
            SDL_GetNumJoystickAxes(g_pJoy), SDL_GetNumJoystickHats(g_pJoy),
            SDL_GetNumJoystickBalls(g_pJoy), SDL_GetNumJoystickButtons(g_pJoy));
    }
#else
    if (SDL_NumJoysticks() > 0 && g_fUseJoystick)
    {
        int i;
        for (i = 0; i < SDL_NumJoysticks(); i++)
        {
            if (PAL_IS_VALID_JOYSTICK(SDL_JoystickNameForIndex(i)))
            {
                g_pJoy = SDL_JoystickOpen(i);
                break;
            }
        }

        if (g_pJoy != NULL)
        {
            //
            //! CANNOT BE DISABLED OR HOTPLUG STOPS WORK
            //
            SDL_JoystickEventState(SDL_ENABLE);
        }
    }
#endif
    else
    {
        g_pJoy = NULL;
    }
}
#endif

static INT
PAL_GetCurrDirection(
   VOID
)
/*++
  Purpose:

    Get the current walking direction.

  Parameters:

    None.

  Return value:

    None.

--*/
{
   INT i, iCurrDir = kDirSouth;

   for (i = 1; i < sizeof(g_InputState.dwKeyOrder) / sizeof(g_InputState.dwKeyOrder[0]); i++)
      if (g_InputState.dwKeyOrder[iCurrDir] < g_InputState.dwKeyOrder[i]) iCurrDir = i;

   if (!g_InputState.dwKeyOrder[iCurrDir]) iCurrDir = kDirUnknown;

   return iCurrDir;
}

static VOID
PAL_KeyDown(
   INT         key,
   BOOL        fRepeat
)
/*++
  Purpose:

    Called when user pressed a key.

  Parameters:

    [IN]  key - keycode of the pressed key.

  Return value:

    None.

--*/
{
   INT iCurrDir = kDirUnknown;

   if (!fRepeat)
   {
      if (key & kKeyDown)
      {
         iCurrDir = kDirSouth;
      }
      else if (key & kKeyLeft)
      {
         iCurrDir = kDirWest;
      }
      else if (key & kKeyUp)
      {
         iCurrDir = kDirNorth;
      }
      else if (key & kKeyRight)
      {
         iCurrDir = kDirEast;
      }

      if (iCurrDir != kDirUnknown)
      {
         g_InputState.dwKeyMaxCount++;
         g_InputState.dwKeyOrder[iCurrDir] = g_InputState.dwKeyMaxCount;
         g_InputState.dir = PAL_GetCurrDirection();
      }
   }

   g_InputState.dwKeyPress |= key;
}

static VOID
PAL_KeyUp(
   INT         key
)
/*++
  Purpose:

    Called when user released a key.

  Parameters:

    [IN]  key - keycode of the released key.

  Return value:

    None.

--*/
{
   INT iCurrDir = kDirUnknown;

   if (key & kKeyDown)
   {
      iCurrDir = kDirSouth;
   }
   else if (key & kKeyLeft)
   {
      iCurrDir = kDirWest;
   }
   else if (key & kKeyUp)
   {
      iCurrDir = kDirNorth;
   }
   else if (key & kKeyRight)
   {
      iCurrDir = kDirEast;
   }

   if (iCurrDir != kDirUnknown)
   {
      g_InputState.dwKeyOrder[iCurrDir] = 0;
      iCurrDir = PAL_GetCurrDirection();
      g_InputState.dwKeyMaxCount = (iCurrDir == kDirUnknown) ? 0 : g_InputState.dwKeyOrder[iCurrDir];
      g_InputState.dir = iCurrDir;
   }
}

static VOID
PAL_UpdateKeyboardState(
   VOID
)
/*++
  Purpose:

    Poll & update keyboard state.

  Parameters:

    None.

  Return value:

    None.

--*/
{
   static DWORD   rgdwKeyLastTime[sizeof(g_KeyMap) / sizeof(g_KeyMap[0])] = {0};
   LPCBYTE        keyState = (LPCBYTE)SDL_GetKeyboardState(NULL);
   int            i;
   DWORD          dwCurrentTime = SDL_GetTicks();
#if SDL_VERSION_ATLEAST(3,0,0)
   SDL_Keymod     mod;
#endif

   for (i = 0; i < sizeof(g_KeyMap) / sizeof(g_KeyMap[0]); i++)
   {
#if SDL_VERSION_ATLEAST(3,0,0)
      if (keyState[SDL_GetScancodeFromKey(g_KeyMap[i][0], &mod)])
#else
      if (keyState[SDL_GetScancodeFromKey(g_KeyMap[i][0])])
#endif
      {
         if (dwCurrentTime > rgdwKeyLastTime[i])
         {
            PAL_KeyDown(g_KeyMap[i][1], (rgdwKeyLastTime[i] != 0));
            if (gConfig.fEnableKeyRepeat)
            {
               rgdwKeyLastTime[i] = dwCurrentTime + (rgdwKeyLastTime[i] == 0 ? 200 : 75);
            }
            else
            {
               rgdwKeyLastTime[i] = 0xFFFFFFFF;
            }
         }
      }
      else
      {
         if (rgdwKeyLastTime[i] != 0)
         {
            PAL_KeyUp(g_KeyMap[i][1]);
            rgdwKeyLastTime[i] = 0;
         }
      }
   }
}

static VOID
PAL_KeyboardEventFilter(
#if SDL_VERSION_ATLEAST(3,0,0)
   const SDL_Event       *lpEvent3
#else
   const SDL_Event       *lpEvent
#endif
)
/*++
  Purpose:

    Handle keyboard events.

  Parameters:

    [IN]  lpEvent - pointer to the event.

  Return value:

    None.

--*/
{
#if SDL_VERSION_ATLEAST(3,0,0)
   typedef struct {
       SDL_Keycode sym;
       Uint16 mod;
   }FAKE_KEYSYM;
   typedef struct {
       FAKE_KEYSYM keysym;
   }FAKE_KEY;
   typedef struct {
       Uint32 type;
       FAKE_KEY key;
   }Fake_Event;
   Fake_Event event = { lpEvent3->type, {lpEvent3->key.key, lpEvent3->key.mod} };
   Fake_Event *lpEvent = &event;
#endif
   if (lpEvent->type == SDL_KEYDOWN)
   {
      //
      // Pressed a key
      //
      if (lpEvent->key.keysym.mod & KMOD_ALT)
      {
         if (lpEvent->key.keysym.sym == SDLK_RETURN)
         {
            //
            // Pressed Alt+Enter (toggle fullscreen)...
            //
            VIDEO_ToggleFullscreen();
            return;
         }
         else if (lpEvent->key.keysym.sym == SDLK_F4)
         {
            //
            // Pressed Alt+F4 (Exit program)...
            //
            PAL_Shutdown(0);
         }
      }
      else if (lpEvent->key.keysym.sym == SDLK_p)
      {
         VIDEO_SaveScreenshot();
      }
#if PAL_HAS_GLSL
      else if (lpEvent->key.keysym.sym == SDLK_z)
      {
         Filter_StepParamSlot(1);
      }
      else if (lpEvent->key.keysym.sym == SDLK_x)
      {
         Filter_StepParamSlot(-1);
      }
      else if (lpEvent->key.keysym.sym == SDLK_COMMA)
      {
         Filter_StepCurrentParam(1);
      }
      else if (lpEvent->key.keysym.sym == SDLK_PERIOD)
      {
         Filter_StepCurrentParam(-1);
      }
#endif
   }
}

static VOID
PAL_MouseEventFilter(
   const SDL_Event *lpEvent
)
/*++
  Purpose:

    Handle mouse events.

  Parameters:

    [IN]  lpEvent - pointer to the event.

  Return value:

    None.

--*/
{
#if PAL_HAS_MOUSE
   static short hitTest = 0; // Double click detect;   
   const SDL_VideoInfo *vi;

   double       screenWidth, gridWidth;
   double       screenHeight, gridHeight;
   double       mx, my;
   double       thumbx;
   double       thumby;
   INT          gridIndex;
   BOOL         isLeftMouseDBClick = FALSE;
   BOOL         isLeftMouseClick = FALSE;
   BOOL         isRightMouseClick = FALSE;
   static INT   lastReleaseButtonTime, lastPressButtonTime, betweenTime;
   static INT   lastPressx = 0;
   static INT   lastPressy = 0;
   static INT   lastReleasex = 0;
   static INT   lastReleasey = 0;

   if (lpEvent->type!= SDL_MOUSEBUTTONDOWN && lpEvent->type != SDL_MOUSEBUTTONUP)
      return;

   vi = SDL_GetVideoInfo();
   screenWidth = vi->current_w;
   screenHeight = vi->current_h;
   gridWidth = screenWidth / 3;
   gridHeight = screenHeight / 3;
   mx = lpEvent->button.x;
   my = lpEvent->button.y;
   thumbx = ceil(mx / gridWidth);
   thumby = floor(my / gridHeight);
   gridIndex = thumbx + thumby * 3 - 1;
   
   switch (lpEvent->type)
   {
   case SDL_MOUSEBUTTONDOWN:
      lastPressButtonTime = SDL_GetTicks();
      lastPressx = lpEvent->button.x;
      lastPressy = lpEvent->button.y;
      switch (gridIndex)
      {
      case 2:
         g_InputState.prevdir = g_InputState.dir;
         g_InputState.dir = kDirNorth;
         break;
      case 6:
         g_InputState.prevdir = g_InputState.dir;
         g_InputState.dir = kDirSouth;
         break;
      case 0:
         g_InputState.prevdir = g_InputState.dir;
         g_InputState.dir = kDirWest;
         break;
      case 8:
         g_InputState.prevdir = g_InputState.dir;
         g_InputState.dir = kDirEast;
         break;
      case 1:
        //g_InputState.prevdir = g_InputState.dir;
        //g_InputState.dir = kDirNorth;
         g_InputState.dwKeyPress |= kKeyUp;
         break;
      case 7:
        //g_InputState.prevdir = g_InputState.dir;
        //g_InputState.dir = kDirSouth; 
         g_InputState.dwKeyPress |= kKeyDown;
         break;
      case 3:
        //g_InputState.prevdir = g_InputState.dir;
        //g_InputState.dir = kDirWest;
        g_InputState.dwKeyPress |= kKeyLeft;
         break;
      case 5:
         //g_InputState.prevdir = g_InputState.dir;
         //g_InputState.dir = kDirEast;
         g_InputState.dwKeyPress |= kKeyRight;
         break;
      }
      break;
   case SDL_MOUSEBUTTONUP:
      lastReleaseButtonTime = SDL_GetTicks();
      lastReleasex = lpEvent->button.x;
      lastReleasey = lpEvent->button.y;
      hitTest ++;
      if (abs(lastPressx - lastReleasex) < 25 &&
                     abs(lastPressy - lastReleasey) < 25)
      {
        betweenTime = lastReleaseButtonTime - lastPressButtonTime;
        if (betweenTime >500)
        {
           isRightMouseClick = TRUE;
        }
        else if (betweenTime >=0)
        {
           if((betweenTime < 100) && (hitTest >= 2))
           {
              isLeftMouseClick = TRUE;
                hitTest = 0;  
           }
           else
           {  
              isLeftMouseClick = TRUE;
              if(betweenTime > 100)
              {
                 hitTest = 0;
              }
              
           }
        }
      }
      switch (gridIndex)
      {
      case 2:
        if( isLeftMouseDBClick )
       {
          AUDIO_IncreaseVolume();
          break;
       }
      case 6:
      case 0:
        if( isLeftMouseDBClick )
       {
          AUDIO_DecreaseVolume();
          break;
       }
      case 7:
         if (isRightMouseClick) //repeat attack
         {
            g_InputState.dwKeyPress |= kKeyRepeat;
            break;
         }
      case 8:
         g_InputState.dir = kDirUnknown;
         g_InputState.prevdir = kDirUnknown;
         break;
      case 1:
        if( isRightMouseClick )
       {
          g_InputState.dwKeyPress |= kKeyForce;
       }
        break;
      case 3:
        if( isRightMouseClick )
       {
          g_InputState.dwKeyPress |= kKeyAuto;
       }
        break;
      case 5:
        if( isRightMouseClick )
       {
          g_InputState.dwKeyPress |= kKeyDefend;
       }
       break;
      case 4:
      if (isRightMouseClick) // menu
      {
         g_InputState.dwKeyPress |= kKeyMenu;
      }
      else if (isLeftMouseClick) // search
      {
         g_InputState.dwKeyPress |= kKeySearch;
      }
      
        break;
      }
      break;
   }
#endif
}

static VOID
PAL_JoystickEventFilter(
   const SDL_Event       *lpEvent
)
/*++
  Purpose:

    Handle joystick events.

  Parameters:

    [IN]  lpEvent - pointer to the event.

  Return value:

    None.

--*/
{
#if PAL_HAS_JOYSTICKS
   switch (lpEvent->type)
   {
#if SDL_VERSION_ATLEAST(2,0,0)
   case SDL_JOYDEVICEADDED:
   case SDL_JOYDEVICEREMOVED:
       PAL_DetectJoystick();
       break;
#endif
   case SDL_JOYAXISMOTION:
      //
      // Moved an axis on joystick
      //
      g_InputState.joystickNeedUpdate = TRUE;
      switch (lpEvent->jaxis.axis)
      {
      case 0:
         //
         // X axis
         //
         if (lpEvent->jaxis.value > 3200)
         {
            g_InputState.axisX = 1;
         }
         else if (lpEvent->jaxis.value < -3200)
         {
            g_InputState.axisX = -1;
         }
         else
         {
            g_InputState.axisX = 0;
         }
         break;

      case 1:
         //
         // Y axis
         //
         if (lpEvent->jaxis.value > 3200)
         {
            g_InputState.axisY = 1;
         }
         else if (lpEvent->jaxis.value < -3200)
         {
            g_InputState.axisY = -1;
         }
         else
         {
            g_InputState.axisY = 0;
         }
         break;
      }
      break;

   case SDL_JOYHATMOTION:
   {
       static int lastKeyMask = 0;
       int newKeyMask = 0;

       switch (lpEvent->jhat.value)
       {
       case SDL_HAT_LEFT:       newKeyMask = kKeyLeft; break;
       case SDL_HAT_LEFTUP:     newKeyMask = kKeyLeft | kKeyUp; break;
       case SDL_HAT_UP:         newKeyMask = kKeyUp; break;
       case SDL_HAT_RIGHTUP:    newKeyMask = kKeyRight | kKeyUp; break;
       case SDL_HAT_RIGHT:      newKeyMask = kKeyRight; break;
       case SDL_HAT_RIGHTDOWN:  newKeyMask = kKeyRight | kKeyDown; break;
       case SDL_HAT_DOWN:       newKeyMask = kKeyDown; break;
       case SDL_HAT_LEFTDOWN:   newKeyMask = kKeyLeft | kKeyDown; break;
       case SDL_HAT_CENTERED:   newKeyMask = 0; break;
       }

       int releaseMask = lastKeyMask & ~newKeyMask;
       int pressMask = newKeyMask & ~lastKeyMask;

       static const int keys[] = { kKeyUp, kKeyDown, kKeyLeft, kKeyRight };
       for (int i = 0; i < 4; i++) {
           if (releaseMask & keys[i]) PAL_KeyUp(keys[i]);
           if (pressMask & keys[i])   PAL_KeyDown(keys[i], FALSE);
       }

       lastKeyMask = newKeyMask;

       g_InputState.prevdir = g_InputState.dir;
       break;
   }

   case SDL_JOYBUTTONDOWN:
      //
      // Pressed the joystick button
      //
      switch (lpEvent->jbutton.button & 1)
      {
      case 0:
         g_InputState.dwKeyPress |= kKeyMenu;
         break;

      case 1:
         g_InputState.dwKeyPress |= kKeySearch;
         break;
      }
      break;
   }
#endif
}

#if PAL_HAS_JOYSTICKS

static VOID
PAL_UpdateJoyStickState(
VOID
)
/*++
 Purpose:
 
 Poll & update joystick state.
 
 Parameters:
 
 None.
 
 Return value:
 
 None.
 
 --*/
{
   static PALDIRECTION lastAxisDir = kDirUnknown;
   int newDir = kDirUnknown;

   static const int dirToKey[] = {
      [kDirEast] = kKeyRight,
      [kDirWest] = kKeyLeft,
      [kDirSouth] = kKeyDown,
      [kDirNorth] = kKeyUp
   };

   if( g_InputState.axisX == 1 && g_InputState.axisY >= 0 )
   {
      newDir = kDirEast;
   }
   else if( g_InputState.axisX == -1 && g_InputState.axisY <= 0 )
   {
      newDir = kDirWest;
   }
   else if( g_InputState.axisY == 1 && g_InputState.axisX <= 0 )
   {
      newDir = kDirSouth;
   }
   else if( g_InputState.axisY == -1 && g_InputState.axisX >= 0 )
   {
      newDir = kDirNorth;
   }

   if (newDir != lastAxisDir) {
      if (lastAxisDir != kDirUnknown) {
         g_InputState.dwKeyPress &= ~dirToKey[lastAxisDir];
      }
      if (newDir != kDirUnknown) {
         g_InputState.dwKeyPress |= dirToKey[newDir];
      }
      g_InputState.prevdir = g_InputState.dir;
      g_InputState.dir = newDir;
      lastAxisDir = newDir;
   }
   // Uncomment to enable joystick hold-to-scroll in menus.
   //else if (newDir != kDirUnknown) {
   //   g_InputState.dwKeyPress |= dirToKey[newDir];
   //}
}

#endif

#if PAL_HAS_TOUCH

static float gfTouchXMin = 0.0f;
static float gfTouchXMax = 1.0f;
static float gfTouchYMin = 0.0f;
static float gfTouchYMax = 1.0f;
static float gfWinAspect = 1.6f;

static PALTOUCHMODE g_TouchMode = kTouchModeTitleMenu;
static BOOL         g_fSearchAvailable = FALSE;
static BOOL         g_fBattleAutoActive = FALSE;
#define PAL_MAP_ZOOM_COUNT 10
static INT          g_iSpeedIndex = 0; // 0:1x, 1:1.25x, 2:1.5x, 3:2x, 4:3x
static INT          g_iMapZoomIndex = 0;
static float        g_fMapZoom = 1.0f; // Continuous linear pinch zoom [1.0f .. 32.0f]
static BOOL         g_bMapPanDetached = FALSE;
static float        g_fPanCenterWX = 0.0f;
static float        g_fPanCenterWY = 0.0f;
static WORD         g_wLastCamScene = 0;
static BOOL         g_bPinchPanActive = FALSE;
static Uint64       g_uPinchId0 = 0;
static Uint64       g_uPinchId1 = 0;
static float        g_fPinchPrevMidUVX = 0.5f;
static float        g_fPinchPrevMidUVY = 0.5f;
static float        g_fPinchPrevDist = 0.1f;
static BOOL         g_bPinchPanDirty = FALSE;

typedef struct tagPALTOUCHSLOT
{
   BOOL   active;
   Uint64 id;
   float  nx;
   float  ny;
   BOOL   isMapFinger;
} PALTOUCHSLOT;
#define PAL_MAX_TOUCH_SLOTS 8
static PALTOUCHSLOT g_rgTouchSlots[PAL_MAX_TOUCH_SLOTS];

static BOOL         g_bRevealSearchables = FALSE;
static BOOL         g_fPendingOpenSettings = FALSE;
static int          g_nActiveFingers = 0;
static int          g_iZoomCamX = 0;
static int          g_iZoomCamY = 0;
static int          g_iZoomViewW = 320;
static int          g_iZoomViewH = 200;

// Fixed-position D-Pad state (Map mode)
static BOOL         g_bJoyTracking = FALSE;
static Uint64       g_uJoyFinger = 0;
static float        g_fJoyDefaultCX = 0.066f;
static float        g_fJoyDefaultCY = 0.815f;
static float        g_fJoyCenterX = 0.066f;
static float        g_fJoyCenterY = 0.815f;
static float        g_fJoyKnobX = 0.066f;
static float        g_fJoyKnobY = 0.815f;
static float        g_fJoyRadiusH = 0.130f; // compact, fits in left margin without covering map
static PALDIRECTION g_JoyDir = kDirUnknown;

// HUD button press tracking
static BOOL         g_bHudTracking = FALSE;
static Uint64       g_uHudFinger = 0;
static PALHUDACTION g_PressedHudAction = kHudActionNone;

// Map tap-to-walk / A* maze pathfinding state
#define PAL_MAX_PATH_STEPS 1024
static BOOL         g_bMapSinglePending = FALSE;
static float        g_fMapSingleStartNX = 0.0f;
static float        g_fMapSingleStartNY = 0.0f;
static DWORD        g_dwMapSingleDownTicks = 0;
static BOOL         g_bMapFingerTracking = FALSE;
static Uint64       g_uMapFinger = 0;
static BOOL         g_fMapTargetActive = FALSE;
static int          g_iMapTargetWorldX = 0;
static int          g_iMapTargetWorldY = 0;
static DWORD        g_dwMapTargetTicks = 0;
static PALDIRECTION g_rgPathDirs[PAL_MAX_PATH_STEPS];
static SHORT        g_rgPathWX[PAL_MAX_PATH_STEPS];
static SHORT        g_rgPathWY[PAL_MAX_PATH_STEPS];
static int          g_nPathSteps = 0;
static int          g_iPathCurStep = 0;
static WORD         g_wPathSceneNum = 0;
static int          g_iPathExpectedWX = 0;
static int          g_iPathExpectedWY = 0;

// UI direct touch / drag scroll / swipe tracking
static BOOL         g_bUiTracking = FALSE;
static Uint64       g_uUiFinger = 0;
static int          g_iUiStartGX = 0;
static int          g_iUiStartGY = 0;
static float        g_fUiLastGY = 0.0f;
static BOOL         g_fUiDragged = FALSE;
static float        g_fScrollAccumY = 0.0f;
static BOOL         g_fTapPending = FALSE;
static int          g_iPendingTapX = 0;
static int          g_iPendingTapY = 0;
static int          g_iPendingHorizSwipe = 0;

static BOOL
PAL_UseRightSideDpad(
   VOID
)
{
#if defined(__IOS__)
   return UTIL_IsRightHandedDpad();
#else
   return TRUE;
#endif
}

static void
PAL_UpdateDefaultJoystickCenter(
   VOID
)
{
   if (PAL_UseRightSideDpad())
   {
      float rightMargin = 1.0f - gfTouchXMax;
      if (rightMargin >= 0.095f)
      {
         // Right-handed layout: D-Pad inside right black margin at upper-middle height
         float availX = rightMargin - 0.012f;
         float radX = availX * 0.50f;
         if (radX > 0.062f) radX = 0.062f;
         g_fJoyRadiusH = radX * gfWinAspect;
         g_fJoyDefaultCX = gfTouchXMax + 0.005f + radX;
         g_fJoyDefaultCY = 0.365f;
      }
      else
      {
         g_fJoyRadiusH = 0.132f;
         float radX = g_fJoyRadiusH / gfWinAspect;
         g_fJoyDefaultCX = 1.0f - 0.014f - radX;
         g_fJoyDefaultCY = 0.365f;
      }
   }
   else
   {
      float leftMargin = gfTouchXMin;
      if (leftMargin >= 0.095f)
      {
         // Left-handed layout: D-Pad inside left black margin at upper-middle height
         float availX = leftMargin - 0.012f;
         float radX = availX * 0.50f;
         if (radX > 0.062f) radX = 0.062f;
         g_fJoyRadiusH = radX * gfWinAspect;
         g_fJoyDefaultCX = gfTouchXMin - 0.005f - radX;
         g_fJoyDefaultCY = 0.365f;
      }
      else
      {
         g_fJoyRadiusH = 0.132f;
         float radX = g_fJoyRadiusH / gfWinAspect;
         g_fJoyDefaultCX = 0.014f + radX;
         g_fJoyDefaultCY = 0.365f;
      }
   }
   g_fJoyCenterX = g_fJoyDefaultCX;
   g_fJoyCenterY = g_fJoyDefaultCY;
   if (!g_bJoyTracking)
   {
      g_fJoyKnobX = g_fJoyDefaultCX;
      g_fJoyKnobY = g_fJoyDefaultCY;
   }
}

VOID
PAL_SetTouchBounds(
   DWORD dwScreenWidth,
   DWORD dwScreenHeight,
   SDL_Rect renderRect
)
{
   if (dwScreenWidth > 0 && dwScreenHeight > 0)
   {
      gfTouchXMin = (float)renderRect.x / (float)dwScreenWidth;
      gfTouchXMax = (float)(renderRect.x + renderRect.w) / (float)dwScreenWidth;
      gfTouchYMin = (float)renderRect.y / (float)dwScreenHeight;
      gfTouchYMax = (float)(renderRect.y + renderRect.h) / (float)dwScreenHeight;
      gfWinAspect = (float)dwScreenWidth / (float)dwScreenHeight;
      if (gfWinAspect < 0.5f) gfWinAspect = 1.6f;
      PAL_UpdateDefaultJoystickCenter();
   }
}

static void
PAL_NormToGameCoords(
   float  nx,
   float  ny,
   float *pfx,
   float *pfy
)
{
   float w = gfTouchXMax - gfTouchXMin;
   float h = gfTouchYMax - gfTouchYMin;
   if (w <= 0.001f) w = 1.0f;
   if (h <= 0.001f) h = 1.0f;
   *pfx = ((nx - gfTouchXMin) / w) * 320.0f;
   *pfy = ((ny - gfTouchYMin) / h) * 200.0f;
}

VOID
PAL_TouchClearGestures(
   VOID
)
{
   g_bUiTracking = FALSE;
   g_fUiDragged = FALSE;
   g_fScrollAccumY = 0.0f;
   g_fTapPending = FALSE;
   g_iPendingHorizSwipe = 0;
}

VOID
PAL_TouchCancelMapWalk(
   VOID
)
{
   g_fMapTargetActive = FALSE;
   g_bMapSinglePending = FALSE;
   g_bMapFingerTracking = FALSE;
   g_nPathSteps = 0;
   g_iPathCurStep = 0;
}

VOID
PAL_TouchSetMode(
   PALTOUCHMODE mode
)
{
   if (g_TouchMode != mode)
   {
      if (mode != kTouchModeMap)
      {
         if (g_bJoyTracking)
         {
            g_bJoyTracking = FALSE;
            g_JoyDir = kDirUnknown;
            g_InputState.dir = kDirUnknown;
            PAL_UpdateDefaultJoystickCenter();
         }
         g_bPinchPanActive = FALSE;
         for (int s = 0; s < PAL_MAX_TOUCH_SLOTS; s++)
         {
            g_rgTouchSlots[s].isMapFinger = FALSE;
         }
         PAL_TouchCancelMapWalk();
      }
      PAL_TouchClearGestures();
      g_bHudTracking = FALSE;
      g_PressedHudAction = kHudActionNone;
      g_TouchMode = mode;
   }
}

PALTOUCHMODE
PAL_TouchGetMode(
   VOID
)
{
   return g_TouchMode;
}

static const int g_rgSpeedPct[5] = { 100, 125, 150, 200, 300 };

INT
PAL_GetSpeedIndex(
   VOID
)
{
   if (g_iSpeedIndex < 0) return 0;
   if (g_iSpeedIndex > 4) return 4;
   return g_iSpeedIndex;
}

INT
PAL_GetSpeedPercent(
   VOID
)
{
   return g_rgSpeedPct[PAL_GetSpeedIndex()];
}

VOID
PAL_CycleSpeedMultiplier(
   VOID
)
{
   g_iSpeedIndex = (g_iSpeedIndex + 1) % 5;
}

DWORD
PAL_GetFrameTime(
   VOID
)
{
   int pct = PAL_GetSpeedPercent();
   return (DWORD)((100 * 100 + pct / 2) / pct);
}

DWORD
PAL_GetBattleFrameTime(
   VOID
)
{
   int pct = PAL_GetSpeedPercent();
   return (DWORD)((40 * 100 + pct / 2) / pct);
}

DWORD
PAL_ScaleDelayMs(
   DWORD ms
)
{
   int pct = PAL_GetSpeedPercent();
   if (ms == 0 || pct <= 100) return ms;
   DWORD scaled = (ms * 100 + (DWORD)(pct / 2)) / (DWORD)pct;
   return scaled > 0 ? scaled : 1;
}

INT
PAL_GetMapZoomIndex(
   VOID
)
{
   return g_iMapZoomIndex;
}

float
PAL_GetMapZoomFloat(
   VOID
)
{
   if (g_fMapZoom < 1.0f) return 1.0f;
   if (g_fMapZoom > 32.0f) return 32.0f;
   return g_fMapZoom;
}

INT
PAL_GetMapZoom(
   VOID
)
{
   int z = (int)lroundf(PAL_GetMapZoomFloat());
   if (z < 1) z = 1;
   if (z > 32) z = 32;
   return z;
}

BOOL
PAL_IsMapPanDetached(
   VOID
)
{
   return g_bMapPanDetached;
}

VOID
PAL_GetMapPanCenter(
   float *pOutWX,
   float *pOutWY
)
{
   if (pOutWX) *pOutWX = g_fPanCenterWX;
   if (pOutWY) *pOutWY = g_fPanCenterWY;
}

VOID
PAL_RecenterMapCamera(
   VOID
)
{
   if (g_bMapPanDetached)
   {
      g_bMapPanDetached = FALSE;
      g_bPinchPanDirty = TRUE;
   }
}

VOID
PAL_CheckMapCameraSceneSync(
   VOID
)
{
   if (gpGlobals != NULL && gpGlobals->wNumScene != g_wLastCamScene)
   {
      g_wLastCamScene = gpGlobals->wNumScene;
      g_bMapPanDetached = FALSE;
   }
}

VOID
PAL_StepMapZoom(
   INT delta
)
{
   float next = g_fMapZoom + (float)delta;
   if (next < 1.0f) next = 1.0f;
   if (next > 32.0f) next = 32.0f;
   g_fMapZoom = next;
}

BOOL
PAL_IsRevealSearchablesEnabled(
   VOID
)
{
   return g_bRevealSearchables;
}

VOID
PAL_ToggleRevealSearchables(
   VOID
)
{
   g_bRevealSearchables = !g_bRevealSearchables;
}

VOID
PAL_TouchSetZoomViewport(
   int camX,
   int camY,
   int viewW,
   int viewH
)
{
   g_iZoomCamX = camX;
   g_iZoomCamY = camY;
   g_iZoomViewW = viewW > 0 ? viewW : 320;
   g_iZoomViewH = viewH > 0 ? viewH : 200;
}

VOID
PAL_TouchGetZoomViewport(
   int *pCamX,
   int *pCamY,
   int *pViewW,
   int *pViewH
)
{
   if (pCamX) *pCamX = g_iZoomCamX;
   if (pCamY) *pCamY = g_iZoomCamY;
   if (pViewW) *pViewW = g_iZoomViewW;
   if (pViewH) *pViewH = g_iZoomViewH;
}

VOID
PAL_TouchSetSearchHint(
   BOOL fAvailable
)
{
   g_fSearchAvailable = fAvailable;
}

VOID
PAL_TouchSetBattleAutoHint(
   BOOL fAutoActive
)
{
   g_fBattleAutoActive = fAutoActive;
}

BOOL
PAL_TouchHasPendingInput(
   VOID
)
{
   return (g_fTapPending || fabsf(g_fScrollAccumY) >= 12.0f || g_iPendingHorizSwipe != 0) ? TRUE : FALSE;
}

BOOL
PAL_TouchPollTap(
   INT *px,
   INT *py
)
{
   if (!g_fTapPending)
   {
      return FALSE;
   }
   if (px) *px = g_iPendingTapX;
   if (py) *py = g_iPendingTapY;
   g_fTapPending = FALSE;
   return TRUE;
}

INT
PAL_TouchPollScrollRows(
   INT iRowHeight
)
{
   if (iRowHeight <= 0) iRowHeight = 16;
   int rows = (int)(g_fScrollAccumY / (float)iRowHeight);
   if (rows != 0)
   {
      g_fScrollAccumY -= (float)(rows * iRowHeight);
   }
   return rows;
}

INT
PAL_TouchPollHorizSwipe(
   VOID
)
{
   int s = g_iPendingHorizSwipe;
   g_iPendingHorizSwipe = 0;
   return s;
}

INT
PAL_TouchGetHUDButtons(
   PALTOUCHMODE mode,
   BOOL         fBattleAutoActive,
   float        fAspect,
   PALHUDRECT  *rgOutRects,
   INT          iMaxRects
)
{
   if (!rgOutRects || iMaxRects <= 0) return 0;
   if (fAspect <= 0.5f) fAspect = 1.6f;

   float safeX = (fAspect > 1.85f) ? 0.052f : 0.022f;
   float topY = 0.026f;
   float pillH = 0.078f;
   float pillW = (pillH * 1.62f) / fAspect;
   float gapX = (0.012f) / (fAspect * 0.6f);
   int count = 0;

   // When full virtual keypad HUD is turned OFF, keep ONLY a subtle 1-tap [设置] safety button
   // (and [返回] inside menus) so the player can NEVER get locked out!
   if (!gConfig.fUseTouchOverlay)
   {
      if (mode == kTouchModeMenu || mode == kTouchModeBattleSub)
      {
         rgOutRects[count].action = kHudActionBack;
         rgOutRects[count].x = 1.0f - safeX - pillW;
         rgOutRects[count].y = topY;
         rgOutRects[count].w = pillW;
         rgOutRects[count].h = pillH;
         count++;
      }
      else if (mode == kTouchModeTitleMenu || mode == kTouchModeMap)
      {
         rgOutRects[count].action = kHudActionSettings;
         rgOutRects[count].x = safeX;
         rgOutRects[count].y = topY;
         rgOutRects[count].w = pillW;
         rgOutRects[count].h = pillH;
         count++;
      }
      return count;
   }

   // Top-left persistent Speed Toggle pill (1x -> 2x -> 3x -> 1x)
   if (mode == kTouchModeMap || mode == kTouchModeBattleMain ||
       mode == kTouchModeBattleSub || mode == kTouchModeBattleWait ||
       mode == kTouchModeDialog)
   {
      rgOutRects[count].action = kHudActionSpeed;
      rgOutRects[count].x = safeX;
      rgOutRects[count].y = topY;
      rgOutRects[count].w = pillW * 1.18f;
      rgOutRects[count].h = pillH;
      count++;
   }

   // Top-left Searchable Object Highlight Toggle ([探查]):
   // Map zoom & pan are now controlled directly via two-finger pinch & drag!
   if (mode == kTouchModeMap && count < iMaxRects)
   {
      rgOutRects[count].action = kHudActionReveal;
      rgOutRects[count].x = safeX + pillW * 1.18f + gapX;
      rgOutRects[count].y = topY;
      rgOutRects[count].w = pillW * 1.05f;
      rgOutRects[count].h = pillH;
      count++;
   }

   if (mode == kTouchModeMap)
   {
      const PALHUDACTION mapBtns[5] = {
         kHudActionStatus, kHudActionMagic, kHudActionUseItem, kHudActionEquipItem, kHudActionSystem
      };
      float totalW = 5.0f * pillW + 4.0f * gapX;
      float startX = 1.0f - safeX - totalW;
      for (int i = 0; i < 5 && count < iMaxRects; i++)
      {
         rgOutRects[count].action = mapBtns[i];
         rgOutRects[count].x = startX + i * (pillW + gapX);
         rgOutRects[count].y = topY;
         rgOutRects[count].w = pillW;
         rgOutRects[count].h = pillH;
         count++;
      }
      if (count < iMaxRects)
      {
         if (PAL_UseRightSideDpad())
         {
            // Right D-Pad -> Place [调查] on the left side, upper position
            if (gfTouchXMin >= 0.095f)
            {
               float actW = gfTouchXMin - 0.018f;
               if (actW > 0.082f) actW = 0.082f;
               float actH = actW * fAspect;
               rgOutRects[count].action = kHudActionSearch;
               rgOutRects[count].x = (gfTouchXMin - actW) * 0.5f;
               rgOutRects[count].y = 0.270f - actH * 0.5f;
               rgOutRects[count].w = actW;
               rgOutRects[count].h = actH;
            }
            else
            {
               float actH = 0.165f;
               float actW = actH / fAspect;
               rgOutRects[count].action = kHudActionSearch;
               rgOutRects[count].x = safeX;
               rgOutRects[count].y = 0.270f - actH * 0.5f;
               rgOutRects[count].w = actW;
               rgOutRects[count].h = actH;
            }
         }
         else
         {
            // Left D-Pad -> Place [调查] on the right side, upper position
            float rightMargin = 1.0f - gfTouchXMax;
            if (rightMargin >= 0.095f)
            {
               float actW = rightMargin - 0.018f;
               if (actW > 0.082f) actW = 0.082f;
               float actH = actW * fAspect;
               rgOutRects[count].action = kHudActionSearch;
               rgOutRects[count].x = gfTouchXMax + (rightMargin - actW) * 0.5f;
               rgOutRects[count].y = 0.270f - actH * 0.5f;
               rgOutRects[count].w = actW;
               rgOutRects[count].h = actH;
            }
            else
            {
               float actH = 0.165f;
               float actW = actH / fAspect;
               rgOutRects[count].action = kHudActionSearch;
               rgOutRects[count].x = 1.0f - safeX - actW;
               rgOutRects[count].y = 0.270f - actH * 0.5f;
               rgOutRects[count].w = actW;
               rgOutRects[count].h = actH;
            }
         }
         count++;
      }
   }
   else if (mode == kTouchModeTitleMenu)
   {
      rgOutRects[count].action = kHudActionSettings;
      rgOutRects[count].x = 1.0f - safeX - pillW * 1.12f;
      rgOutRects[count].y = topY;
      rgOutRects[count].w = pillW * 1.12f;
      rgOutRects[count].h = pillH;
      count++;
   }
   else if (mode == kTouchModeBattleMain)
   {
      const PALHUDACTION batBtns[5] = {
         kHudActionBattleAuto, kHudActionBattleRepeat, kHudActionBattleDefend, kHudActionBattleItem, kHudActionBattleFlee
      };
      float totalW = 5.0f * pillW + 4.0f * gapX;
      float startX = 1.0f - safeX - totalW;
      for (int i = 0; i < 5 && count < iMaxRects; i++)
      {
         rgOutRects[count].action = batBtns[i];
         rgOutRects[count].x = startX + i * (pillW + gapX);
         rgOutRects[count].y = topY;
         rgOutRects[count].w = pillW;
         rgOutRects[count].h = pillH;
         count++;
      }
   }
   else if (mode == kTouchModeMenu || mode == kTouchModeBattleSub)
   {
      rgOutRects[count].action = kHudActionBack;
      rgOutRects[count].x = 1.0f - safeX - pillW;
      rgOutRects[count].y = topY;
      rgOutRects[count].w = pillW;
      rgOutRects[count].h = pillH;
      count++;
   }
   else if (mode == kTouchModeBattleWait && fBattleAutoActive)
   {
      float w = pillW * 1.18f;
      rgOutRects[count].action = kHudActionBack;
      rgOutRects[count].x = 1.0f - safeX - w;
      rgOutRects[count].y = topY;
      rgOutRects[count].w = w;
      rgOutRects[count].h = pillH;
      count++;
   }
   else if (mode == kTouchModeNone)
   {
      rgOutRects[count].action = kHudActionSkip;
      rgOutRects[count].x = 1.0f - safeX - pillW;
      rgOutRects[count].y = topY;
      rgOutRects[count].w = pillW;
      rgOutRects[count].h = pillH;
      count++;
   }

   return count;
}

static PALHUDACTION
PAL_HitTestHUDButtons(
   float nx,
   float ny
)
{
   PALHUDRECT rects[16];
   int n = PAL_TouchGetHUDButtons(g_TouchMode, g_fBattleAutoActive, gfWinAspect, rects, 16);
   float padX = 0.006f;
   float padY = 0.010f;
   for (int i = 0; i < n; i++)
   {
      if (nx >= rects[i].x - padX && nx <= rects[i].x + rects[i].w + padX &&
          ny >= rects[i].y - padY && ny <= rects[i].y + rects[i].h + padY)
      {
         return rects[i].action;
      }
   }
   return kHudActionNone;
}

static void
PAL_TriggerHudAction(
   PALHUDACTION action
)
{
   switch (action)
   {
   case kHudActionStatus:
      PAL_TouchCancelMapWalk();
      g_InputState.dwKeyPress |= kKeyStatus;
      break;
   case kHudActionMagic:
      PAL_TouchCancelMapWalk();
      g_InputState.dwKeyPress |= kKeyForce;
      break;
   case kHudActionUseItem:
      PAL_TouchCancelMapWalk();
      g_InputState.dwKeyPress |= kKeyUseItem;
      break;
   case kHudActionEquipItem:
      PAL_TouchCancelMapWalk();
      g_InputState.dwKeyPress |= kKeyThrowItem;
      break;
   case kHudActionSystem:
      PAL_TouchCancelMapWalk();
      g_InputState.dwKeyPress |= kKeySystem;
      break;
   case kHudActionSearch:
      PAL_TouchCancelMapWalk();
      g_InputState.dwKeyPress |= kKeySearch;
      break;
   case kHudActionBack:
   case kHudActionSkip:
      PAL_TouchClearGestures();
      g_InputState.dwKeyPress |= kKeyMenu;
      break;
   case kHudActionBattleAuto:
      g_InputState.dwKeyPress |= kKeyAuto;
      break;
   case kHudActionBattleRepeat:
      g_InputState.dwKeyPress |= kKeyRepeat;
      break;
   case kHudActionBattleDefend:
      g_InputState.dwKeyPress |= kKeyDefend;
      break;
   case kHudActionBattleItem:
      g_InputState.dwKeyPress |= kKeyUseItem;
      break;
   case kHudActionBattleFlee:
      g_InputState.dwKeyPress |= kKeyFlee;
      break;
   case kHudActionSpeed:
      PAL_CycleSpeedMultiplier();
      if (!g_bRenderPaused && gpScreen != NULL)
      {
         VIDEO_UpdateScreen(NULL);
      }
      break;
   case kHudActionZoomOut:
      // 缩小: 往大倍率走 (1x -> 2x -> 3x -> 4x -> 6x -> 8x -> 12x -> 16x -> 24x -> 32x)
      PAL_StepMapZoom(1);
      if (!g_bRenderPaused && gpScreen != NULL)
      {
         VIDEO_UpdateScreen(NULL);
      }
      break;
   case kHudActionZoomIn:
      // 放大: 往小倍率走 (32x -> 24x -> 16x -> 12x -> 8x -> 6x -> 4x -> 3x -> 2x -> 1x)
      PAL_StepMapZoom(-1);
      if (!g_bRenderPaused && gpScreen != NULL)
      {
         VIDEO_UpdateScreen(NULL);
      }
      break;
   case kHudActionReveal:
      PAL_ToggleRevealSearchables();
      if (!g_bRenderPaused && gpScreen != NULL)
      {
         VIDEO_UpdateScreen(NULL);
      }
      break;
   case kHudActionSettings:
      g_fPendingOpenSettings = TRUE;
      break;
   default:
      break;
   }
}

static void
PAL_UpdateJoystickFromFinger(
   float nx,
   float ny
)
{
   // Using the D-Pad always immediately snaps the camera back to center on the player!
   PAL_RecenterMapCamera();
   PAL_UpdateDefaultJoystickCenter();
   float dx = (nx - g_fJoyCenterX) * gfWinAspect;
   float dy = (ny - g_fJoyCenterY);
   float dist = sqrtf(dx * dx + dy * dy);
   // Minimal center-pin deadzone so touching ANY part of a D-pad wedge moves immediately!
   float deadzone = g_fJoyRadiusH * 0.055f;

   g_fJoyKnobX = nx;
   g_fJoyKnobY = ny;

   if (dist < deadzone)
   {
      g_JoyDir = kDirUnknown;
      g_InputState.dir = kDirUnknown;
   }
   else
   {
      // Fixed 4-quadrant isometric D-Pad matching PAL1's 45-degree corridors:
      // Up-Left -> West (-16,-8), Up-Right -> North (+16,-8),
      // Down-Left -> South (-16,+8), Down-Right -> East (+16,+8)
      if (dx < 0.0f && dy < 0.0f)
         g_JoyDir = kDirWest;
      else if (dx >= 0.0f && dy < 0.0f)
         g_JoyDir = kDirNorth;
      else if (dx < 0.0f && dy >= 0.0f)
         g_JoyDir = kDirSouth;
      else
         g_JoyDir = kDirEast;

      g_InputState.prevdir = g_InputState.dir;
      g_InputState.dir = g_JoyDir;
   }
}

#define ASTAR_GRID_W     128
#define ASTAR_GRID_H     256
#define ASTAR_MAX_NODES  (ASTAR_GRID_W * ASTAR_GRID_H)
#define ASTAR_HEAP_CAP   65536

static VOID
PAL_ComputeMazePath(
   int startWX,
   int startWY,
   int targetWX,
   int targetWY
)
{
   static uint32_t s_gCost[ASTAR_MAX_NODES];
   static uint16_t s_parent[ASTAR_MAX_NODES];
   static uint8_t  s_dir[ASTAR_MAX_NODES];
   static uint8_t  s_obsCache[ASTAR_MAX_NODES]; // 0 = unknown, 1 = free, 2 = blocked
   static uint32_t s_heapF[ASTAR_HEAP_CAP];
   static uint16_t s_heapN[ASTAR_HEAP_CAP];
   static uint16_t s_revNodes[PAL_MAX_PATH_STEPS];

   g_nPathSteps = 0;
   g_iPathCurStep = 0;
   g_iPathExpectedWX = startWX;
   g_iPathExpectedWY = startWY;

   if (gpGlobals == NULL || PAL_GetCurrentMap() == NULL)
   {
      return;
   }

   g_wPathSceneNum = gpGlobals->wNumScene;

   int rx = ((startWX % 16) + 16) % 16;
   int ry = ((startWY % 8) + 8) % 8;
   int startIX = (startWX - rx) / 16;
   int startIY = (startWY - ry) / 8;

   if (startIX < 0 || startIX >= ASTAR_GRID_W || startIY < 0 || startIY >= ASTAR_GRID_H)
   {
      return;
   }

   BOOL bTargetBlocked = PAL_CheckObstacleWithRange(PAL_XY(targetWX, targetWY), TRUE, 0, TRUE);

   memset(s_gCost, 0xFF, sizeof(s_gCost));
   memset(s_obsCache, 0, sizeof(s_obsCache));

   uint16_t startNode = (uint16_t)((startIY << 7) | startIX);
   s_gCost[startNode] = 0;
   s_parent[startNode] = startNode;
   s_dir[startNode] = (uint8_t)gpGlobals->wPartyDirection;
   s_obsCache[startNode] = 1;

   int initDX = abs(targetWX - startWX);
   int initDY2 = abs(targetWY - startWY) * 2;
   int initCheb = (initDX > initDY2) ? initDX : initDY2;

   uint16_t bestNode = startNode;
   int      bestTargetDist = initDX + initDY2;
   uint32_t bestGCost = 0;

   int heapSize = 0;
   s_heapF[0] = (uint32_t)((initCheb * 10) / 16);
   s_heapN[0] = startNode;
   heapSize = 1;

   while (heapSize > 0)
   {
      uint32_t curF = s_heapF[0];
      uint16_t u = s_heapN[0];
      heapSize--;
      if (heapSize > 0)
      {
         uint32_t lastF = s_heapF[heapSize];
         uint16_t lastN = s_heapN[heapSize];
         int idx = 0;
         while (TRUE)
         {
            int left = (idx << 1) + 1;
            if (left >= heapSize) break;
            int right = left + 1;
            int smallest = (right < heapSize && s_heapF[right] < s_heapF[left]) ? right : left;
            if (lastF <= s_heapF[smallest]) break;
            s_heapF[idx] = s_heapF[smallest];
            s_heapN[idx] = s_heapN[smallest];
            idx = smallest;
         }
         s_heapF[idx] = lastF;
         s_heapN[idx] = lastN;
      }

      uint32_t curG = s_gCost[u];
      int ix = u & 127;
      int iy = u >> 7;
      int wx = rx + ix * 16;
      int wy = ry + iy * 8;

      int dx = abs(targetWX - wx);
      int dy2 = abs(targetWY - wy) * 2;
      int cheb = (dx > dy2) ? dx : dy2;
      uint32_t hVal = (uint32_t)((cheb * 10) / 16);

      if (curF > curG + hVal)
      {
         continue; // stale heap entry
      }

      int dTarget = dx + dy2;
      if (dTarget < bestTargetDist || (dTarget == bestTargetDist && curG < bestGCost))
      {
         bestNode = u;
         bestTargetDist = dTarget;
         bestGCost = curG;
      }

      if ((!bTargetBlocked && cheb < 16) || (bTargetBlocked && cheb <= 20 && u != startNode))
      {
         bestNode = u;
         break;
      }

      for (int d = 0; d < 4; d++)
      {
         int nx = ix + ((d == kDirWest || d == kDirSouth) ? -1 : 1);
         int ny = iy + ((d == kDirWest || d == kDirNorth) ? -1 : 1);
         if (nx < 0 || nx >= ASTAR_GRID_W || ny < 0 || ny >= ASTAR_GRID_H)
         {
            continue;
         }

         uint16_t v = (uint16_t)((ny << 7) | nx);
         int nwx = rx + nx * 16;
         int nwy = ry + ny * 8;

         if (s_obsCache[v] == 0)
         {
            BOOL blocked = PAL_CheckObstacleWithRange(PAL_XY(nwx, nwy), TRUE, 0, TRUE);
            s_obsCache[v] = blocked ? 2 : 1;
         }
         if (s_obsCache[v] == 2)
         {
            continue;
         }

         // Small +1 turn penalty eliminates needless zig-zag sprite jitter while preserving shortest path!
         uint32_t stepCost = (s_dir[u] == (uint8_t)d) ? 10u : 11u;
         uint32_t newG = curG + stepCost;
         if (newG < s_gCost[v])
         {
            s_gCost[v] = newG;
            s_parent[v] = u;
            s_dir[v] = (uint8_t)d;

            if (heapSize < ASTAR_HEAP_CAP)
            {
               int ndx = abs(targetWX - nwx);
               int ndy2 = abs(targetWY - nwy) * 2;
               int nCheb = (ndx > ndy2) ? ndx : ndy2;
               uint32_t newF = newG + (uint32_t)((nCheb * 10) / 16);

               int idx = heapSize++;
               while (idx > 0)
               {
                  int pIdx = (idx - 1) >> 1;
                  if (s_heapF[pIdx] <= newF) break;
                  s_heapF[idx] = s_heapF[pIdx];
                  s_heapN[idx] = s_heapN[pIdx];
                  idx = pIdx;
               }
               s_heapF[idx] = newF;
               s_heapN[idx] = v;
            }
         }
      }
   }

   if (bestNode == startNode)
   {
      return;
   }

   int revCount = 0;
   uint16_t curr = bestNode;
   while (curr != startNode && revCount < PAL_MAX_PATH_STEPS)
   {
      s_revNodes[revCount++] = curr;
      curr = s_parent[curr];
   }

   if (curr != startNode)
   {
      // Path longer than PAL_MAX_PATH_STEPS: keep the first PAL_MAX_PATH_STEPS steps from startNode!
      // Walk all the way back to startNode and record the prefix.
      uint16_t trace = bestNode;
      int totalLen = 0;
      while (trace != startNode && totalLen < ASTAR_MAX_NODES)
      {
         trace = s_parent[trace];
         totalLen++;
      }
      trace = bestNode;
      int skip = totalLen - PAL_MAX_PATH_STEPS;
      while (skip > 0 && trace != startNode)
      {
         trace = s_parent[trace];
         skip--;
      }
      revCount = 0;
      while (trace != startNode && revCount < PAL_MAX_PATH_STEPS)
      {
         s_revNodes[revCount++] = trace;
         trace = s_parent[trace];
      }
   }

   for (int i = 0; i < revCount; i++)
   {
      uint16_t node = s_revNodes[revCount - 1 - i];
      int ix = node & 127;
      int iy = node >> 7;
      g_rgPathDirs[i] = (PALDIRECTION)s_dir[node];
      g_rgPathWX[i] = (SHORT)(rx + ix * 16);
      g_rgPathWY[i] = (SHORT)(ry + iy * 8);
   }
   g_nPathSteps = revCount;
   g_iPathCurStep = 0;
}

static VOID
PAL_SetMapWalkTarget(
   int tapWX,
   int tapWY
)
{
   if (gpGlobals == NULL) return;

   int targetWX = tapWX;
   int targetWY = tapWY;

   // If the player tapped on a searchable NPC, chest, or highlighted hidden item, snap to its anchor
   int zoom = PAL_GetMapZoom();
   int snapRX = g_bRevealSearchables ? (14 + zoom * 6) : 18;
   if (snapRX > 110) snapRX = 110;
   int bestSnapDist = 999999;

   for (int i = gpGlobals->g.rgScene[gpGlobals->wNumScene - 1].wEventObjectIndex;
        i < gpGlobals->g.rgScene[gpGlobals->wNumScene].wEventObjectIndex; i++)
   {
      LPEVENTOBJECT p = &(gpGlobals->g.lprgEventObject[i]);
      if (p->sState > 0 && p->sVanishTime == 0 && p->wTriggerScript != 0 &&
          p->wTriggerMode >= kTriggerSearchNear && p->wTriggerMode < kTriggerTouchNear &&
          gpGlobals->g.lprgScriptEntry[p->wTriggerScript].wOperation != 0x0000)
      {
         int px = (SHORT)p->x;
         int py = (SHORT)p->y;
         int dx = abs(tapWX - px);
         int dyTop = g_bRevealSearchables ? (28 + zoom * 5) : 34;
         int dyBot = g_bRevealSearchables ? (12 + zoom * 3) : 10;
         if (dx <= snapRX && tapWY >= py - dyTop && tapWY <= py + dyBot)
         {
            int d = dx + abs(tapWY - py) * 2;
            if (d < bestSnapDist)
            {
               bestSnapDist = d;
               targetWX = px;
               targetWY = py;
            }
         }
      }
   }

   int startWX = PAL_X(gpGlobals->viewport) + PAL_X(gpGlobals->partyoffset);
   int startWY = PAL_Y(gpGlobals->viewport) + PAL_Y(gpGlobals->partyoffset);

   g_iMapTargetWorldX = targetWX;
   g_iMapTargetWorldY = targetWY;
   g_fMapTargetActive = TRUE;
   g_dwMapTargetTicks = SDL_GetTicks();

   PAL_ComputeMazePath(startWX, startWY, targetWX, targetWY);
}

INT
PAL_TouchGetMapPathWaypoints(
   const SHORT **ppOutWX,
   const SHORT **ppOutWY
)
{
   if (!g_fMapTargetActive || g_iPathCurStep >= g_nPathSteps)
   {
      if (ppOutWX) *ppOutWX = NULL;
      if (ppOutWY) *ppOutWY = NULL;
      return 0;
   }
   if (ppOutWX) *ppOutWX = &g_rgPathWX[g_iPathCurStep];
   if (ppOutWY) *ppOutWY = &g_rgPathWY[g_iPathCurStep];
   return g_nPathSteps - g_iPathCurStep;
}

PALDIRECTION
PAL_TouchUpdateMapWalk(
   BOOL *pfTriggerSearch
)
{
   if (pfTriggerSearch) *pfTriggerSearch = FALSE;
   if (g_bJoyTracking || !g_fMapTargetActive || gpGlobals == NULL)
   {
      return kDirUnknown;
   }

   if (gpGlobals->wNumScene != g_wPathSceneNum)
   {
      PAL_TouchCancelMapWalk();
      return kDirUnknown;
   }

   int wx = PAL_X(gpGlobals->viewport) + PAL_X(gpGlobals->partyoffset);
   int wy = PAL_Y(gpGlobals->viewport) + PAL_Y(gpGlobals->partyoffset);

   // Detect script warp / portal teleportation inside the same scene
   int drift = abs(wx - g_iPathExpectedWX) + abs(wy - g_iPathExpectedWY) * 2;
   if (drift > 64)
   {
      PAL_TouchCancelMapWalk();
      return kDirUnknown;
   }
   else if (drift > 0)
   {
      // Pushed aside or blocked by a moving NPC: replan dynamically!
      PAL_ComputeMazePath(wx, wy, g_iMapTargetWorldX, g_iMapTargetWorldY);
   }

   while (g_iPathCurStep < g_nPathSteps &&
          wx == (int)g_rgPathWX[g_iPathCurStep] &&
          wy == (int)g_rgPathWY[g_iPathCurStep])
   {
      g_iPathCurStep++;
   }

   if (g_iPathCurStep < g_nPathSteps)
   {
      PALDIRECTION d = g_rgPathDirs[g_iPathCurStep];
      int ox = (d == kDirWest || d == kDirSouth) ? -16 : 16;
      int oy = (d == kDirWest || d == kDirNorth) ? -8 : 8;

      if ((wx + ox != (int)g_rgPathWX[g_iPathCurStep]) ||
          (wy + oy != (int)g_rgPathWY[g_iPathCurStep]) ||
          PAL_CheckObstacleWithRange(PAL_XY(wx + ox, wy + oy), TRUE, 0, TRUE))
      {
         // Moving NPC/monster stepped onto our planned path: replan around it!
         PAL_ComputeMazePath(wx, wy, g_iMapTargetWorldX, g_iMapTargetWorldY);
      }

      if (g_iPathCurStep < g_nPathSteps)
      {
         d = g_rgPathDirs[g_iPathCurStep];
         ox = (d == kDirWest || d == kDirSouth) ? -16 : 16;
         oy = (d == kDirWest || d == kDirNorth) ? -8 : 8;
         if (!PAL_CheckObstacleWithRange(PAL_XY(wx + ox, wy + oy), TRUE, 0, TRUE))
         {
            g_iPathExpectedWX = wx + ox;
            g_iPathExpectedWY = wy + oy;
            return d;
         }
      }
   }

   // Arrived at destination (or closest reachable tile adjacent to tapped chest/NPC/door)
   int tdx = g_iMapTargetWorldX - wx;
   int tdy = g_iMapTargetWorldY - wy;
   int distToTap = abs(tdx) + abs(tdy) * 2;
   if (distToTap >= 12 && distToTap <= 76)
   {
      if (tdx < 0 && tdy < 0)
         gpGlobals->wPartyDirection = kDirWest;
      else if (tdx >= 0 && tdy < 0)
         gpGlobals->wPartyDirection = kDirNorth;
      else if (tdx < 0 && tdy >= 0)
         gpGlobals->wPartyDirection = kDirSouth;
      else
         gpGlobals->wPartyDirection = kDirEast;
   }

   if (!g_bMapFingerTracking)
   {
      PAL_TouchCancelMapWalk();
      if (distToTap <= 76 && pfTriggerSearch)
      {
         *pfTriggerSearch = TRUE;
      }
   }
   return kDirUnknown;
}

static void
PAL_SyncCustomMapViewport(
   VOID
)
{
   if (gpGlobals == NULL) return;
   PAL_CheckMapCameraSceneSync();
   float fz = PAL_GetMapZoomFloat();
   int vw = (int)lroundf(320.0f * fz);
   int vh = (int)lroundf(200.0f * fz);
   if (vw < 320) vw = 320;
   if (vh < 200) vh = 200;

   int cx, cy;
   if (g_bMapPanDetached)
   {
      cx = (int)lroundf(g_fPanCenterWX - (float)vw * 0.5f);
      cy = (int)lroundf(g_fPanCenterWY - (float)vh * 0.5f);
   }
   else
   {
      int partyWX = PAL_X(gpGlobals->viewport) + PAL_X(gpGlobals->partyoffset);
      int partyWY = PAL_Y(gpGlobals->viewport) + PAL_Y(gpGlobals->partyoffset);
      cx = partyWX - vw / 2;
      cy = partyWY - vh / 2 - 8;
   }
   g_iZoomCamX = cx;
   g_iZoomCamY = cy;
   g_iZoomViewW = vw;
   g_iZoomViewH = vh;
}

static void
PAL_GameCoordsToMapWorld(
   float fgx,
   float fgy,
   int  *pOutWX,
   int  *pOutWY
)
{
   if (gpGlobals == NULL)
   {
      if (pOutWX) *pOutWX = 0;
      if (pOutWY) *pOutWY = 0;
      return;
   }
   PAL_CheckMapCameraSceneSync();
   if (PAL_GetMapZoomFloat() > 1.001f || g_bMapPanDetached)
   {
      PAL_SyncCustomMapViewport();
      if (pOutWX) *pOutWX = g_iZoomCamX + (int)lroundf((fgx / 320.0f) * (float)g_iZoomViewW);
      if (pOutWY) *pOutWY = g_iZoomCamY + (int)lroundf((fgy / 200.0f) * (float)g_iZoomViewH);
   }
   else
   {
      if (pOutWX) *pOutWX = PAL_X(gpGlobals->viewport) + (int)floorf(fgx);
      if (pOutWY) *pOutWY = PAL_Y(gpGlobals->viewport) + (int)floorf(fgy);
   }
}

static void
PAL_ExecuteSingleMapTapOrStartWalk(
   float  nx,
   float  ny,
   Uint64 fid,
   BOOL   fFingerStillDown
)
{
   if (gpGlobals == NULL) return;
   float fgx = 0.0f, fgy = 0.0f;
   PAL_NormToGameCoords(nx, ny, &fgx, &fgy);
   if (fgx < 0.0f || fgx >= 320.0f || fgy < 0.0f || fgy >= 200.0f) return;

   int partyWX = PAL_X(gpGlobals->viewport) + PAL_X(gpGlobals->partyoffset);
   int partyWY = PAL_Y(gpGlobals->viewport) + PAL_Y(gpGlobals->partyoffset);
   int tapWX = 0, tapWY = 0;
   PAL_GameCoordsToMapWorld(fgx, fgy, &tapWX, &tapWY);

   if (!fFingerStillDown && abs(tapWX - partyWX) <= 20 && tapWY >= partyWY - 38 && tapWY <= partyWY + 12)
   {
      PAL_TouchCancelMapWalk();
      g_InputState.dwKeyPress |= kKeySearch;
   }
   else
   {
      if (fFingerStillDown)
      {
         g_bMapFingerTracking = TRUE;
         g_uMapFinger = fid;
      }
      PAL_SetMapWalkTarget(tapWX, tapWY);
   }
}

static int
PAL_CountActiveTouchSlots(
   VOID
)
{
   int c = 0;
   for (int i = 0; i < PAL_MAX_TOUCH_SLOTS; i++)
   {
      if (g_rgTouchSlots[i].active) c++;
   }
   return c;
}

static int
PAL_CountMapTouchSlots(
   int *pIdx0,
   int *pIdx1
)
{
   int c = 0;
   if (pIdx0) *pIdx0 = -1;
   if (pIdx1) *pIdx1 = -1;
   for (int i = 0; i < PAL_MAX_TOUCH_SLOTS; i++)
   {
      if (g_rgTouchSlots[i].active && g_rgTouchSlots[i].isMapFinger)
      {
         if (c == 0 && pIdx0) *pIdx0 = i;
         else if (c == 1 && pIdx1) *pIdx1 = i;
         c++;
      }
   }
   return c;
}

static void
PAL_NormPairToMidUVAndDist(
   float  nx0,
   float  ny0,
   float  nx1,
   float  ny1,
   float *pMidUVX,
   float *pMidUVY,
   float *pDist
)
{
   float w = gfTouchXMax - gfTouchXMin;
   float h = gfTouchYMax - gfTouchYMin;
   if (w <= 0.001f) w = 1.0f;
   if (h <= 0.001f) h = 1.0f;
   float uvx0 = (nx0 - gfTouchXMin) / w;
   float uvy0 = (ny0 - gfTouchYMin) / h;
   float uvx1 = (nx1 - gfTouchXMin) / w;
   float uvy1 = (ny1 - gfTouchYMin) / h;
   if (pMidUVX) *pMidUVX = 0.5f * (uvx0 + uvx1);
   if (pMidUVY) *pMidUVY = 0.5f * (uvy0 + uvy1);
   float dx = (nx1 - nx0) * gfWinAspect;
   float dy = (ny1 - ny0);
   if (pDist) *pDist = hypotf(dx, dy);
}

static void
PAL_LatchPinchPanReference(
   int idx0,
   int idx1
)
{
   if (idx0 < 0 || idx1 < 0 || gpGlobals == NULL) return;
   PAL_CheckMapCameraSceneSync();
   if (!g_bMapPanDetached)
   {
      if (PAL_GetMapZoomFloat() <= 1.001f)
      {
         g_fPanCenterWX = (float)PAL_X(gpGlobals->viewport) + 160.0f;
         g_fPanCenterWY = (float)PAL_Y(gpGlobals->viewport) + 100.0f;
      }
      else
      {
         g_fPanCenterWX = (float)(PAL_X(gpGlobals->viewport) + PAL_X(gpGlobals->partyoffset));
         g_fPanCenterWY = (float)(PAL_Y(gpGlobals->viewport) + PAL_Y(gpGlobals->partyoffset) - 8);
      }
   }
   g_uPinchId0 = g_rgTouchSlots[idx0].id;
   g_uPinchId1 = g_rgTouchSlots[idx1].id;
   PAL_NormPairToMidUVAndDist(
      g_rgTouchSlots[idx0].nx, g_rgTouchSlots[idx0].ny,
      g_rgTouchSlots[idx1].nx, g_rgTouchSlots[idx1].ny,
      &g_fPinchPrevMidUVX, &g_fPinchPrevMidUVY, &g_fPinchPrevDist
   );
   g_bPinchPanActive = TRUE;
}

VOID
PAL_TouchGetHUDState(
   PALTOUCHHUDSTATE *pOutState
)
{
   if (!pOutState) return;
   PAL_UpdateDefaultJoystickCenter();
   pOutState->mode = g_TouchMode;
   pOutState->fSearchAvailable = g_fSearchAvailable;
   pOutState->fBattleAutoActive = g_fBattleAutoActive;
   pOutState->iSpeedIndex = PAL_GetSpeedIndex();
   pOutState->iMapZoomIndex = PAL_GetMapZoomIndex();
   pOutState->iMapZoom = PAL_GetMapZoom();
   pOutState->fMapZoom = PAL_GetMapZoomFloat();
   pOutState->fMapPanDetached = PAL_IsMapPanDetached();
   pOutState->fRevealSearchables = g_bRevealSearchables;
   pOutState->fJoyActive = g_bJoyTracking;
   pOutState->fJoyCenterX = g_fJoyCenterX;
   pOutState->fJoyCenterY = g_fJoyCenterY;
   pOutState->fJoyKnobX = g_fJoyKnobX;
   pOutState->fJoyKnobY = g_fJoyKnobY;
   pOutState->fJoyRadiusX = g_fJoyRadiusH / gfWinAspect;
   pOutState->fJoyRadiusY = g_fJoyRadiusH;
   pOutState->joyDir = g_JoyDir;
   pOutState->pressedAction = g_PressedHudAction;
   pOutState->fMapTargetActive = g_fMapTargetActive;
   pOutState->iMapTargetWorldX = g_iMapTargetWorldX;
   pOutState->iMapTargetWorldY = g_iMapTargetWorldY;
   pOutState->dwMapTargetTicks = g_dwMapTargetTicks;
}

static VOID
PAL_TouchRepeatCheck(
   VOID
)
{
#if defined(__IOS__)
   static BOOL s_bOpeningSettings = FALSE;
   static DWORD s_dwLastPinchRender = 0;

   if (g_TouchMode == kTouchModeMap && g_bMapSinglePending && !g_bPinchPanActive)
   {
      if (SDL_GetTicks() - g_dwMapSingleDownTicks >= 105)
      {
         g_bMapSinglePending = FALSE;
         PAL_ExecuteSingleMapTapOrStartWalk(g_fMapSingleStartNX, g_fMapSingleStartNY, g_uMapFinger, TRUE);
      }
   }

   if (g_bPinchPanDirty && g_TouchMode == kTouchModeMap && !g_bRenderPaused && gpScreen != NULL)
   {
      DWORD now = SDL_GetTicks();
      if (now - s_dwLastPinchRender >= 14)
      {
         g_bPinchPanDirty = FALSE;
         s_dwLastPinchRender = now;
         VIDEO_UpdateScreen(NULL);
      }
   }

   if (g_fPendingOpenSettings && !s_bOpeningSettings)
   {
      g_fPendingOpenSettings = FALSE;
      s_bOpeningSettings = TRUE;
      g_bHudTracking = FALSE;
      g_PressedHudAction = kHudActionNone;
      g_bJoyTracking = FALSE;
      g_JoyDir = kDirUnknown;
      g_InputState.dir = kDirUnknown;
      g_nActiveFingers = 0;
      g_bPinchPanActive = FALSE;
      memset(g_rgTouchSlots, 0, sizeof(g_rgTouchSlots));
      PAL_TouchClearGestures();

      BOOL fInGame = (gpGlobals != NULL && gpGlobals->fInMainGame);
      int curMusic = (gpGlobals != NULL) ? (int)gpGlobals->wNumMusic : 0;
      if (!fInGame && g_TouchMode == kTouchModeTitleMenu)
      {
         curMusic = RIX_NUM_OPENINGMENU;
      }
      INT restartReq = UTIL_ShowSystemSettingsModal(fInGame, curMusic);
      s_bOpeningSettings = FALSE;

      if (restartReq == 1)
      {
         PAL_SaveConfig();
         AUDIO_PlayMusic(0, FALSE, 1);
         PAL_Shutdown(0);
         return;
      }
      if (!g_bRenderPaused && gpScreen != NULL)
      {
         VIDEO_UpdateScreen(NULL);
      }
   }
#endif
}

#else /* !PAL_HAS_TOUCH */

VOID
PAL_TouchSetMode(
   PALTOUCHMODE mode
)
{
   (void)mode;
}

PALTOUCHMODE
PAL_TouchGetMode(
   VOID
)
{
   return kTouchModeNone;
}

INT
PAL_GetSpeedIndex(
   VOID
)
{
   return 0;
}

INT
PAL_GetSpeedPercent(
   VOID
)
{
   return 100;
}

VOID
PAL_CycleSpeedMultiplier(
   VOID
)
{
}

DWORD
PAL_GetFrameTime(
   VOID
)
{
   return (1000 / FPS);
}

DWORD
PAL_GetBattleFrameTime(
   VOID
)
{
   return (1000 / BATTLE_FPS);
}

DWORD
PAL_ScaleDelayMs(
   DWORD ms
)
{
   return ms;
}

INT
PAL_GetMapZoomIndex(
   VOID
)
{
   return 0;
}

INT
PAL_GetMapZoom(
   VOID
)
{
   return 1;
}

float
PAL_GetMapZoomFloat(
   VOID
)
{
   return 1.0f;
}

BOOL
PAL_IsMapPanDetached(
   VOID
)
{
   return FALSE;
}

VOID
PAL_GetMapPanCenter(
   float *pOutWX,
   float *pOutWY
)
{
   if (pOutWX) *pOutWX = 0.0f;
   if (pOutWY) *pOutWY = 0.0f;
}

VOID
PAL_RecenterMapCamera(
   VOID
)
{
}

VOID
PAL_CheckMapCameraSceneSync(
   VOID
)
{
}

VOID
PAL_StepMapZoom(
   INT delta
)
{
   (void)delta;
}

BOOL
PAL_IsRevealSearchablesEnabled(
   VOID
)
{
   return FALSE;
}

VOID
PAL_ToggleRevealSearchables(
   VOID
)
{
}

VOID
PAL_TouchSetZoomViewport(
   int camX,
   int camY,
   int viewW,
   int viewH
)
{
   (void)camX;
   (void)camY;
   (void)viewW;
   (void)viewH;
}

VOID
PAL_TouchGetZoomViewport(
   int *pCamX,
   int *pCamY,
   int *pViewW,
   int *pViewH
)
{
   if (pCamX) *pCamX = 0;
   if (pCamY) *pCamY = 0;
   if (pViewW) *pViewW = 320;
   if (pViewH) *pViewH = 200;
}

VOID
PAL_TouchSetSearchHint(
   BOOL fAvailable
)
{
   (void)fAvailable;
}

VOID
PAL_TouchSetBattleAutoHint(
   BOOL fAutoActive
)
{
   (void)fAutoActive;
}

BOOL
PAL_TouchHasPendingInput(
   VOID
)
{
   return FALSE;
}

BOOL
PAL_TouchPollTap(
   INT *px,
   INT *py
)
{
   (void)px;
   (void)py;
   return FALSE;
}

INT
PAL_TouchPollScrollRows(
   INT iRowHeight
)
{
   (void)iRowHeight;
   return 0;
}

INT
PAL_TouchPollHorizSwipe(
   VOID
)
{
   return 0;
}

VOID
PAL_TouchClearGestures(
   VOID
)
{
}

VOID
PAL_TouchCancelMapWalk(
   VOID
)
{
}

PALDIRECTION
PAL_TouchUpdateMapWalk(
   BOOL *pfTriggerSearch
)
{
   if (pfTriggerSearch) *pfTriggerSearch = FALSE;
   return kDirUnknown;
}

INT
PAL_TouchGetMapPathWaypoints(
   const SHORT **ppOutWX,
   const SHORT **ppOutWY
)
{
   if (ppOutWX) *ppOutWX = NULL;
   if (ppOutWY) *ppOutWY = NULL;
   return 0;
}

VOID
PAL_TouchGetHUDState(
   PALTOUCHHUDSTATE *pOutState
)
{
   if (pOutState) memset(pOutState, 0, sizeof(*pOutState));
}

INT
PAL_TouchGetHUDButtons(
   PALTOUCHMODE mode,
   BOOL         fBattleAutoActive,
   float        fAspect,
   PALHUDRECT  *rgOutRects,
   INT          iMaxRects
)
{
   (void)mode;
   (void)fBattleAutoActive;
   (void)fAspect;
   (void)rgOutRects;
   (void)iMaxRects;
   return 0;
}

#endif

static VOID
PAL_TouchEventFilter(
#if SDL_VERSION_ATLEAST(3,0,0)
   const SDL_Event       *lpEvent3
#else
   const SDL_Event       *lpEvent
#endif
)
{
#if PAL_HAS_TOUCH
#if SDL_VERSION_ATLEAST(3,0,0)
   typedef struct {
      float x, y;
      Uint64 fingerId;
   } FAKE_TFINGER;
   typedef struct {
      Uint32 type;
      FAKE_TFINGER tfinger;
   } Fake_Event;
   Fake_Event event = { lpEvent3->type, { lpEvent3->tfinger.x, lpEvent3->tfinger.y, (Uint64)lpEvent3->tfinger.fingerID } };
   Fake_Event *lpEvent = &event;
#endif

   float nx = lpEvent->tfinger.x;
   float ny = lpEvent->tfinger.y;
   Uint64 fid = (Uint64)lpEvent->tfinger.fingerId;
   float fgx = 0.0f, fgy = 0.0f;
   PAL_NormToGameCoords(nx, ny, &fgx, &fgy);
   int gx = (int)floorf(fgx);
   int gy = (int)floorf(fgy);

   switch (lpEvent->type)
   {
   case SDL_FINGERDOWN:
   {
      int slotIdx = -1;
      for (int s = 0; s < PAL_MAX_TOUCH_SLOTS; s++)
      {
         if (g_rgTouchSlots[s].active && g_rgTouchSlots[s].id == fid)
         {
            slotIdx = s;
            break;
         }
         if (!g_rgTouchSlots[s].active && slotIdx < 0)
         {
            slotIdx = s;
         }
      }
      if (slotIdx >= 0)
      {
         g_rgTouchSlots[slotIdx].active = TRUE;
         g_rgTouchSlots[slotIdx].id = fid;
         g_rgTouchSlots[slotIdx].nx = nx;
         g_rgTouchSlots[slotIdx].ny = ny;
         g_rgTouchSlots[slotIdx].isMapFinger = FALSE;
      }

      g_nActiveFingers = PAL_CountActiveTouchSlots();
      if (g_nActiveFingers >= 3)
      {
         g_bMapSinglePending = FALSE;
         g_bMapFingerTracking = FALSE;
         g_bPinchPanActive = FALSE;
         g_fPendingOpenSettings = TRUE;
         break;
      }

      // 1. Always check active HUD buttons first (includes safety [设置]/[返回] when HUD is hidden)
      PALHUDACTION hitBtn = PAL_HitTestHUDButtons(nx, ny);
      if (hitBtn != kHudActionNone && !g_bHudTracking)
      {
         g_bHudTracking = TRUE;
         g_uHudFinger = fid;
         g_PressedHudAction = hitBtn;
         PAL_TriggerHudAction(hitBtn);
         break;
      }

      // 2. Mode-specific touch handling
      if (g_TouchMode == kTouchModeDialog)
      {
         g_InputState.dwKeyPress |= kKeySearch;
         break;
      }
      else if (g_TouchMode == kTouchModeBattleWait)
      {
         if (g_fBattleAutoActive)
         {
            g_InputState.dwKeyPress |= kKeyMenu;
         }
         break;
      }
      else if (g_TouchMode == kTouchModeMap)
      {
         // Fixed-position left/right swappable upper-middle D-Pad
         if (gConfig.fUseTouchOverlay)
         {
            PAL_UpdateDefaultJoystickCenter();
            float dpadDX = (nx - g_fJoyCenterX) * gfWinAspect;
            float dpadDY = (ny - g_fJoyCenterY);
            float dpadDist = sqrtf(dpadDX * dpadDX + dpadDY * dpadDY);
            BOOL fRightDpad = PAL_UseRightSideDpad();
            BOOL fInPadZone = fRightDpad
               ? ((1.0f - gfTouchXMax < 0.095f) || (nx > gfTouchXMax))
               : ((gfTouchXMin < 0.095f) || (nx < gfTouchXMin));
            if (!g_bJoyTracking && fInPadZone && dpadDist <= g_fJoyRadiusH * 1.05f)
            {
               g_bJoyTracking = TRUE;
               g_uJoyFinger = fid;
               PAL_RecenterMapCamera();
               PAL_TouchCancelMapWalk();
               PAL_UpdateJoystickFromFinger(nx, ny);
               break;
            }
         }

         if (slotIdx >= 0)
         {
            g_rgTouchSlots[slotIdx].isMapFinger = TRUE;
         }

         int idx0 = -1, idx1 = -1;
         int nMap = PAL_CountMapTouchSlots(&idx0, &idx1);
         if (nMap >= 2)
         {
            // Two fingers on map: enter continuous linear pinch-to-zoom (1x..32x) & free two-finger pan!
            g_bMapSinglePending = FALSE;
            g_bMapFingerTracking = FALSE;
            PAL_LatchPinchPanReference(idx0, idx1);
         }
         else if (nMap == 1 && gx >= 0 && gx < 320 && gy >= 0 && gy < 200)
         {
            // Wait briefly to disambiguate 1-finger tap/walk from 2-finger pinch/pan
            g_bMapSinglePending = TRUE;
            g_uMapFinger = fid;
            g_fMapSingleStartNX = nx;
            g_fMapSingleStartNY = ny;
            g_dwMapSingleDownTicks = SDL_GetTicks();
         }
         break;
      }
      else
      {
         // Menu / Title / BattleMain / BattleSub: track tap, vertical scroll, or horizontal swipe
         if (!g_bUiTracking)
         {
            g_bUiTracking = TRUE;
            g_uUiFinger = fid;
            g_iUiStartGX = gx;
            g_iUiStartGY = gy;
            g_fUiLastGY = fgy;
            g_fUiDragged = FALSE;
         }
         else if (g_TouchMode == kTouchModeMenu || g_TouchMode == kTouchModeBattleSub)
         {
            // Two-finger touch in menu acts as quick Cancel/Back
            PAL_TouchClearGestures();
            g_InputState.dwKeyPress |= kKeyMenu;
         }
      }
      break;
   }

   case SDL_FINGERMOTION:
   {
      for (int s = 0; s < PAL_MAX_TOUCH_SLOTS; s++)
      {
         if (g_rgTouchSlots[s].active && g_rgTouchSlots[s].id == fid)
         {
            g_rgTouchSlots[s].nx = nx;
            g_rgTouchSlots[s].ny = ny;
            break;
         }
      }

      if (g_bJoyTracking && fid == g_uJoyFinger)
      {
         PAL_UpdateJoystickFromFinger(nx, ny);
      }
      else if (g_TouchMode == kTouchModeMap && gpGlobals != NULL)
      {
         int idx0 = -1, idx1 = -1;
         int nMap = PAL_CountMapTouchSlots(&idx0, &idx1);
         if (nMap >= 2 && idx0 >= 0 && idx1 >= 0)
         {
            g_bMapSinglePending = FALSE;
            g_bMapFingerTracking = FALSE;
            if (!g_bPinchPanActive ||
                g_rgTouchSlots[idx0].id != g_uPinchId0 ||
                g_rgTouchSlots[idx1].id != g_uPinchId1)
            {
               PAL_LatchPinchPanReference(idx0, idx1);
            }
            else
            {
               float midUVX = 0.5f, midUVY = 0.5f, curDist = 0.1f;
               PAL_NormPairToMidUVAndDist(
                  g_rgTouchSlots[idx0].nx, g_rgTouchSlots[idx0].ny,
                  g_rgTouchSlots[idx1].nx, g_rgTouchSlots[idx1].ny,
                  &midUVX, &midUVY, &curDist
               );

               float oldZoom = PAL_GetMapZoomFloat();
               float oldViewW = 320.0f * oldZoom;
               float oldViewH = 200.0f * oldZoom;
               float anchorWX = g_fPanCenterWX + (g_fPinchPrevMidUVX - 0.5f) * oldViewW;
               float anchorWY = g_fPanCenterWY + (g_fPinchPrevMidUVY - 0.5f) * oldViewH;

               float newZoom = oldZoom;
               if (g_fPinchPrevDist > 0.012f && curDist > 0.012f)
               {
                  newZoom = oldZoom * (g_fPinchPrevDist / curDist);
               }
               if (newZoom < 1.0f) newZoom = 1.0f;
               if (newZoom > 32.0f) newZoom = 32.0f;

               float newViewW = 320.0f * newZoom;
               float newViewH = 200.0f * newZoom;
               g_fPanCenterWX = anchorWX - (midUVX - 0.5f) * newViewW;
               g_fPanCenterWY = anchorWY - (midUVY - 0.5f) * newViewH;

               if (g_fPanCenterWX < -320.0f) g_fPanCenterWX = -320.0f;
               if (g_fPanCenterWX > 2368.0f) g_fPanCenterWX = 2368.0f;
               if (g_fPanCenterWY < -320.0f) g_fPanCenterWY = -320.0f;
               if (g_fPanCenterWY > 2368.0f) g_fPanCenterWY = 2368.0f;

               g_fMapZoom = newZoom;
               g_bMapPanDetached = TRUE;
               g_fPinchPrevMidUVX = midUVX;
               g_fPinchPrevMidUVY = midUVY;
               g_fPinchPrevDist = curDist;
               PAL_SyncCustomMapViewport();
               g_bPinchPanDirty = TRUE;
            }
         }
         else if (nMap == 1)
         {
            if (g_bMapSinglePending && fid == g_uMapFinger)
            {
               if (SDL_GetTicks() - g_dwMapSingleDownTicks >= 95)
               {
                  g_bMapSinglePending = FALSE;
                  PAL_ExecuteSingleMapTapOrStartWalk(nx, ny, fid, TRUE);
               }
            }
            else if (g_bMapFingerTracking && fid == g_uMapFinger)
            {
               float clampedFGX = fgx < 0.0f ? 0.0f : (fgx > 319.0f ? 319.0f : fgx);
               float clampedFGY = fgy < 0.0f ? 0.0f : (fgy > 199.0f ? 199.0f : fgy);
               int dragWX = 0, dragWY = 0;
               PAL_GameCoordsToMapWorld(clampedFGX, clampedFGY, &dragWX, &dragWY);
               if (abs(dragWX - g_iMapTargetWorldX) + abs(dragWY - g_iMapTargetWorldY) * 2 >= 20)
               {
                  PAL_SetMapWalkTarget(dragWX, dragWY);
               }
            }
         }
      }
      else if (g_bUiTracking && fid == g_uUiFinger)
      {
         int totalDX = gx - g_iUiStartGX;
         int totalDY = gy - g_iUiStartGY;
         if (abs(totalDY) >= 7 || abs(totalDX) >= 10)
         {
            g_fUiDragged = TRUE;
         }
         if (g_fUiDragged)
         {
            float dy = fgy - g_fUiLastGY;
            // Natural touch scrolling: dragging finger UP (dy < 0) moves list down (+rows)
            g_fScrollAccumY -= dy;
         }
         g_fUiLastGY = fgy;
      }
      break;
   }

#if SDL_VERSION_ATLEAST(3,0,0)
   case SDL_EVENT_FINGER_CANCELED:
#endif
   case SDL_FINGERUP:
   {
      BOOL bWasCanceled = FALSE;
#if SDL_VERSION_ATLEAST(3,0,0)
      if (lpEvent->type == SDL_EVENT_FINGER_CANCELED)
      {
         bWasCanceled = TRUE;
      }
#endif
      for (int s = 0; s < PAL_MAX_TOUCH_SLOTS; s++)
      {
         if (g_rgTouchSlots[s].active && g_rgTouchSlots[s].id == fid)
         {
            g_rgTouchSlots[s].active = FALSE;
            g_rgTouchSlots[s].isMapFinger = FALSE;
            break;
         }
      }
      g_nActiveFingers = PAL_CountActiveTouchSlots();

      int idx0 = -1, idx1 = -1;
      if (PAL_CountMapTouchSlots(&idx0, &idx1) < 2)
      {
         g_bPinchPanActive = FALSE;
      }

      if (g_bHudTracking && fid == g_uHudFinger)
      {
         g_bHudTracking = FALSE;
         g_PressedHudAction = kHudActionNone;
      }
      if (g_bJoyTracking && fid == g_uJoyFinger)
      {
         g_bJoyTracking = FALSE;
         g_JoyDir = kDirUnknown;
         g_InputState.dir = kDirUnknown;
         PAL_UpdateDefaultJoystickCenter();
      }
      if (g_bMapSinglePending && fid == g_uMapFinger)
      {
         g_bMapSinglePending = FALSE;
         if (!bWasCanceled && g_TouchMode == kTouchModeMap)
         {
            PAL_ExecuteSingleMapTapOrStartWalk(nx, ny, fid, FALSE);
         }
      }
      if (g_bMapFingerTracking && fid == g_uMapFinger)
      {
         g_bMapFingerTracking = FALSE;
      }
      if (g_bUiTracking && fid == g_uUiFinger)
      {
         g_bUiTracking = FALSE;
         int totalDX = gx - g_iUiStartGX;
         int totalDY = gy - g_iUiStartGY;
         if (!bWasCanceled && !g_fUiDragged)
         {
            g_iPendingTapX = gx;
            g_iPendingTapY = gy;
            g_fTapPending = TRUE;
         }
         else if (!bWasCanceled && abs(totalDX) >= 34 && abs(totalDY) < 28)
         {
            g_iPendingHorizSwipe = (totalDX < 0) ? -1 : 1;
         }
      }
      break;
   }
   }
#endif
}

#if SDL_VERSION_ATLEAST(2,0,0)
static VOID
PAL_SetAppInBackground(
   BOOL                   fBackground
)
{
   g_bRenderPaused = fBackground;
   g_bAppInBackground = fBackground;

   if (!fBackground)
   {
      VIDEO_UpdateScreen(NULL);
   }
}
#endif

#if SDL_VERSION_ATLEAST(3,0,0)
static bool SDLCALL
PAL_AppEventWatch(
   void                  *lpUserData,
   SDL_Event             *lpEvent
)
/*++
  Purpose:

    SDL 3 never queues the events about going to the background and coming back. They
    only reach event watchers, during the call that notices them. On iOS and Android
    that is SDL_PumpEvents() on the thread running the game, as with any queued event.

  Parameters:

    [IN]  lpUserData - not used.

    [IN]  lpEvent - pointer to the event.

  Return value:

    Always true; the return value of an event watcher is ignored.

--*/
{
   if (lpEvent->type == SDL_EVENT_WILL_ENTER_BACKGROUND)
   {
      PAL_SetAppInBackground(TRUE);
   }
   else if (lpEvent->type == SDL_EVENT_DID_ENTER_FOREGROUND)
   {
      PAL_SetAppInBackground(FALSE);
   }
   return true;
}
#endif

static int SDLCALL
PAL_EventFilter(
   const SDL_Event       *lpEvent
)

/*++
  Purpose:

    SDL event filter function. A filter to process all events.

  Parameters:

    [IN]  lpEvent - pointer to the event.

  Return value:

    1 = the event will be added to the internal queue.
    0 = the event will be dropped from the queue.

--*/
{
   switch (lpEvent->type)
   {
#if SDL_VERSION_ATLEAST(2,0,0)
# if SDL_VERSION_ATLEAST(3,0,0)
   case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
       VIDEO_Resize(lpEvent->window.data1, lpEvent->window.data2);
       break;
#else
   case SDL_WINDOWEVENT:
      if (lpEvent->window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
      {
         //
         // resized the window
         //
         VIDEO_Resize(lpEvent->window.data1, lpEvent->window.data2);
      }
      break;

   case SDL_APP_WILLENTERBACKGROUND:
      PAL_SetAppInBackground(TRUE);
      break;

   case SDL_APP_DIDENTERFOREGROUND:
      PAL_SetAppInBackground(FALSE);
      break;
#endif
#else
   case SDL_VIDEORESIZE:
      //
      // resized the window
      //
      VIDEO_Resize(lpEvent->resize.w, lpEvent->resize.h);
      break;
#endif

   case SDL_QUIT:
      //
      // clicked on the close button of the window. Quit immediately.
      //
      PAL_Shutdown(0);
   }

   PAL_KeyboardEventFilter(lpEvent);
   PAL_MouseEventFilter(lpEvent);
   PAL_JoystickEventFilter(lpEvent);
   PAL_TouchEventFilter(lpEvent);

   //
   // All events are handled here; don't put anything to the internal queue
   //
   return 0;
}

VOID
PAL_ClearKeyState(
   VOID
)
/*++
  Purpose:

    Clear the record of pressed keys.

  Parameters:

    None.

  Return value:

    None.

--*/
{
   g_InputState.dwKeyPress = 0;
}

VOID
PAL_InitInput(
   VOID
)
/*++
  Purpose:

    Initialize the input subsystem.

  Parameters:

    None.

  Return value:

    None.

--*/
{
   memset((void *)&g_InputState, 0, sizeof(g_InputState));
   g_InputState.dir = kDirUnknown;
   g_InputState.prevdir = kDirUnknown;

   //
   // Check for joystick
   // MUST FOR PLATFORMS THAT DOES NOT SUPPORT JOYSTICKS HOTPLUG
   //
#if PAL_HAS_JOYSTICKS
   PAL_DetectJoystick();
#endif

   input_init_filter();

#if SDL_VERSION_ATLEAST(3,0,0)
   SDL_AddEventWatch(PAL_AppEventWatch, NULL);
#endif
}

VOID
PAL_ShutdownInput(
   VOID
)
/*++
  Purpose:

    Shutdown the input subsystem.

  Parameters:

    None.

  Return value:

    None.

--*/
{
#if SDL_VERSION_ATLEAST(3,0,0)
   SDL_RemoveEventWatch(PAL_AppEventWatch, NULL);
#endif

#if PAL_HAS_JOYSTICKS
   if (g_pJoy != NULL)
   {
      SDL_JoystickClose(g_pJoy);
      g_pJoy = NULL;
   }
#endif
   input_shutdown_filter();
}

static int
PAL_PollEvent(
   SDL_Event *event
)
/*++
  Purpose:

    Poll and process one event.

  Parameters:

    [OUT] event - Events polled from SDL.

  Return value:

    Return value of PAL_PollEvent.

--*/
{
   SDL_Event evt;

   int ret = SDL_PollEvent(&evt);
   if (ret != 0 && !input_event_filter(&evt, &g_InputState))
   {
      PAL_EventFilter(&evt);
   }

   if (event != NULL)
   {
      *event = evt;
   }

   return ret;
}

VOID
PAL_ProcessEvent(
   VOID
)
/*++
  Purpose:

    Process all events.

  Parameters:

    None.

  Return value:

    None.

--*/
{
#if PAL_HAS_JOYSTICKS
   g_InputState.joystickNeedUpdate = FALSE;
#endif
   while (PAL_PollEvent(NULL));

   //
   // The game waits here while it is in the background, without drawing anything. That also
   // keeps work for the GPU from piling up, as none of it can be submitted from there.
   //
   while (g_bAppInBackground)
   {
      SDL_Delay(50);
      while (PAL_PollEvent(NULL));
   }

   PAL_UpdateKeyboardState();
#if PAL_HAS_JOYSTICKS
   if(g_InputState.joystickNeedUpdate)
      PAL_UpdateJoyStickState();
#endif
#if PAL_HAS_TOUCH
   PAL_TouchRepeatCheck();
#endif
}

VOID
PAL_RegisterInputFilter(
   void (*init_filter)(),
   int (*event_filter)(const SDL_Event *, volatile PALINPUTSTATE *),
   void (*shutdown_filter)()
)
/*++
  Purpose:

    Register caller-defined input event filter.

  Parameters:

    [IN] init_filter - Filter that will be called inside PAL_InitInput
	[IN] event_filter - Filter that will be called inside PAL_PollEvent, 
	                    return non-zero value from this filter disables
						further internal event processing.
	[IN] shutdown_filter - Filter that will be called inside PAL_ShutdownInput

	Passing NULL to either parameter means the caller does not provide such filter.

  Return value:

    None.

--*/
{
	if (init_filter)
		input_init_filter = init_filter;
	if (event_filter)
		input_event_filter = event_filter;
	if (shutdown_filter)
		input_shutdown_filter = shutdown_filter;
}
