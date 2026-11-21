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

static BOOL __buymenu_firsttime_render;
static BOOL g_fSaveSlotIsSaving = FALSE;

static WORD GetSavedTimes(int iSaveSlot)
{
	FILE *fp = UTIL_OpenFileAtPath(gConfig.pszSavePath, PAL_va(0, "%d.rpg", iSaveSlot));
	WORD wSavedTimes = 0;
	if (fp != NULL)
	{
		if (fread(&wSavedTimes, sizeof(WORD), 1, fp) == 1)
			wSavedTimes = SDL_SwapLE16(wSavedTimes);
		else
			wSavedTimes = 0;
		fclose(fp);
	}
	return wSavedTimes;
}

static VOID PAL_ScanSaveSlots(int *pMaxSlot, WORD *pMaxSavedTimes)
{
	int maxSlot = 0;
	WORD maxTimes = 0;
	if (gConfig.pszSavePath != NULL)
	{
		DIR *dir = opendir(gConfig.pszSavePath);
		if (dir != NULL)
		{
			struct dirent *ent;
			while ((ent = readdir(dir)) != NULL)
			{
				const char *name = ent->d_name;
				int slot = 0;
				int pos = 0;
				while (name[pos] >= '0' && name[pos] <= '9')
				{
					slot = slot * 10 + (name[pos] - '0');
					if (slot > 99999) break;
					pos++;
				}
				if (pos > 0 && slot >= 1 && slot <= 60000 &&
				    SDL_strcasecmp(name + pos, ".rpg") == 0)
				{
					if (slot > maxSlot)
					{
						maxSlot = slot;
					}
					if (pMaxSavedTimes != NULL)
					{
						WORD t = GetSavedTimes(slot);
						if (t > maxTimes)
						{
							maxTimes = t;
						}
					}
				}
			}
			closedir(dir);
		}
	}
	if (pMaxSlot != NULL) *pMaxSlot = maxSlot;
	if (pMaxSavedTimes != NULL) *pMaxSavedTimes = maxTimes;
}

VOID
PAL_DrawOpeningMenuBackground(
   VOID
)
/*++
  Purpose:

    Draw the background of the main menu.

  Parameters:

    None.

  Return value:

    None.

--*/
{
   LPBYTE        buf;

   buf = (LPBYTE)malloc(320 * 200);
   if (buf == NULL)
   {
      return;
   }

   //
   // Read the picture from fbp.mkf.
   //
   PAL_MKFDecompressChunk(buf, 320 * 200, MAINMENU_BACKGROUND_FBPNUM, gpGlobals->f.fpFBP);

   //
   // ...and blit it to the screen buffer.
   //
   PAL_FBPBlitToSurface(buf, gpScreen);
   VIDEO_UpdateScreen(NULL);

   free(buf);
}

INT
PAL_OpeningMenu(
   VOID
)
/*++
  Purpose:

    Show the opening menu.

  Parameters:

    None.

  Return value:

    Which saved slot to load from (1-5). 0 to start a new game.

--*/
{
   INT           iItemSelected;
   WORD          wDefaultItem     = 0;
   INT           w[2] = { PAL_WordWidth(MAINMENU_LABEL_NEWGAME), PAL_WordWidth(MAINMENU_LABEL_LOADGAME) };

   MENUITEM      rgMainMenuItem[2] = {
      // value   label                     enabled   position
      {  0,      MAINMENU_LABEL_NEWGAME,   TRUE,     PAL_XY(125 - (w[0] > 4 ? (w[0] - 4) * 8 : 0), 95)  },
      {  1,      MAINMENU_LABEL_LOADGAME,  TRUE,     PAL_XY(125 - (w[1] > 4 ? (w[1] - 4) * 8 : 0), 112) }
   };

   PAL_TouchSetMode(kTouchModeTitleMenu);

   //
   // Play the background music
   //
   AUDIO_PlayMusic(RIX_NUM_OPENINGMENU, TRUE, 1);

   //
   // Draw the background
   //
   PAL_DrawOpeningMenuBackground();
   PAL_FadeIn(0, FALSE, 1);

   while (TRUE)
   {
      //
      // Activate the menu
      //
      iItemSelected = (INT)PAL_ReadMenu(NULL, rgMainMenuItem, 2, wDefaultItem, MENUITEM_COLOR);

      if (iItemSelected == 0 || iItemSelected == MENUITEM_VALUE_CANCELLED)
      {
         //
         // Start a new game
         //
         iItemSelected = 0;
         break;
      }
      else
      {
         //
         // Load game
         //
         g_fSaveSlotIsSaving = FALSE;
         VIDEO_BackupScreen(gpScreen);
         iItemSelected = PAL_SaveSlotMenu(1);
         VIDEO_RestoreScreen(gpScreen);
         VIDEO_UpdateScreen(NULL);
         if (iItemSelected != MENUITEM_VALUE_CANCELLED)
         {
            break;
         }
         wDefaultItem = 0;
      }
   }

   //
   // Fade out the screen and the music
   //
   AUDIO_PlayMusic(0, FALSE, 1);
   PAL_FadeOut(1);

   if (iItemSelected == 0)
   {
      PAL_PlayAVI("3.avi");
   }

   return iItemSelected;
}

INT
PAL_SaveSlotMenu(
   INT         wDefaultSlot
)
/*++
  Purpose:

    Show the unlimited scrollable save/load slot menu (1 .. infinity).

  Parameters:

    [IN]  wDefaultSlot - default save slot number (1..N).

  Return value:

    Selected save slot (1..N), or MENUITEM_VALUE_CANCELLED if cancelled.

--*/
{
   PALTOUCHMODE    prevTouchMode = PAL_TouchGetMode();
   SDL_Surface    *pSavedBg = VIDEO_DuplicateSurface(gpScreen, NULL);
   const SDL_Rect  panelRect = { 164, 2, 156, 198 };
   BOOL            fIsSaving = g_fSaveSlotIsSaving;
   int             maxExistingSlot = 0;
   int             nVisibleRows = fIsSaving ? 6 : 7;
   int             boxY = fIsSaving ? 40 : 8;
   int             nBoxRows = fIsSaving ? 6 : 8;
   int             boxH = 42 + nBoxRows * 18;
   int             firstRowY = fIsSaving ? 51 : 20;
   int             rowStep = fIsSaving ? 22 : 23;
   int             nTotalSlots;
   int             iCurSlot;
   int             iTopSlot;
   INT             iResult = MENUITEM_VALUE_CANCELLED;

   LPCWSTR         wSlotWord = PAL_GetWord(LOADMENU_LABEL_SLOT_FIRST);
   LPCWSTR         wNewGameWord = PAL_GetWord(MAINMENU_LABEL_NEWGAME);
   WCHAR           chSlot0 = (wSlotWord && wSlotWord[0]) ? wSlotWord[0] : L'#';
   WCHAR           chSlot1 = (wSlotWord && wSlotWord[0] && wSlotWord[1]) ? wSlotWord[1] : L' ';
   WCHAR           chNew = (wNewGameWord && wNewGameWord[0]) ? wNewGameWord[0] : L'+';

   PAL_ScanSaveSlots(&maxExistingSlot, NULL);

   if (wDefaultSlot < 1)
   {
      wDefaultSlot = 1;
   }
   iCurSlot = wDefaultSlot;

   nTotalSlots = (maxExistingSlot + 15 > 20) ? (maxExistingSlot + 15) : 20;
   if (iCurSlot + 15 > nTotalSlots)
   {
      nTotalSlots = iCurSlot + 15;
   }

   iTopSlot = iCurSlot - nVisibleRows / 2;
   if (iTopSlot < 1)
   {
      iTopSlot = 1;
   }

   PAL_TouchSetMode(kTouchModeMenu);
   PAL_TouchClearGestures();
   PAL_ClearKeyState();

   while (TRUE)
   {
      int i;

      // Auto-expand unlimited slot list whenever scrolling near the bottom
      if (iTopSlot + nVisibleRows >= nTotalSlots - 3)
      {
         nTotalSlots = iTopSlot + nVisibleRows + 20;
      }

      // Restore clean background before redrawing panel
      if (pSavedBg != NULL)
      {
         VIDEO_CopySurface(pSavedBg, NULL, gpScreen, NULL);
      }

      // 1. In Save Mode, draw 1-tap [+新进度 #N] quick button at the top
      if (fIsSaving)
      {
         WCHAR szNewBtn[32];
         int iNextFreshSlot = maxExistingSlot + 1;
         PAL_CreateSingleLineBoxWithShadow(PAL_XY(168, 5), 6, FALSE, 4);
         PAL_swprintf(szNewBtn, 32, L"+%lc%lc%lc %d", chNew, chSlot0, chSlot1, iNextFreshSlot);
         PAL_DrawText(szNewBtn, PAL_XY(180, 14), MENUITEM_COLOR_CONFIRMED, TRUE, FALSE, FALSE);
      }

      // 2. Draw main scrollable save-slot window
      PAL_CreateBoxWithShadow(PAL_XY(168, boxY), nBoxRows, 6, 0, FALSE, 4);

      for (i = 0; i < nVisibleRows; i++)
      {
         int slot = iTopSlot + i;
         int y = firstRowY + i * rowStep;
         WORD wTimes = GetSavedTimes(slot);
         WCHAR szLabel[32];
         BYTE bColor;

         if (slot == iCurSlot)
         {
            bColor = (!fIsSaving && wTimes == 0) ? MENUITEM_COLOR_SELECTED_INACTIVE : MENUITEM_COLOR_SELECTED;
         }
         else
         {
            bColor = (!fIsSaving && wTimes == 0) ? MENUITEM_COLOR_INACTIVE : MENUITEM_COLOR;
         }

         PAL_swprintf(szLabel, 32, L"%lc%lc %d", chSlot0, chSlot1, slot);
         PAL_DrawText(szLabel, PAL_XY(180, y), bColor, TRUE, FALSE, FALSE);

         if (wTimes > 0)
         {
            PAL_DrawNumber((UINT)wTimes, 4, PAL_XY(268, y + 4), kNumColorYellow, kNumAlignRight);
         }
         else
         {
            PAL_DrawText(L"---", PAL_XY(268, y), MENUITEM_COLOR_INACTIVE, TRUE, FALSE, FALSE);
         }
      }

      // 3. Draw up/down scroll indicators on right inner edge
      if (iTopSlot > 1)
      {
         LPCBITMAPRLE lpUp = PAL_SpriteGetFrame(gpSpriteUI, SPRITENUM_CURSOR_YELLOW_UP);
         if (lpUp != NULL)
         {
            PAL_RLEBlitToSurface(lpUp, gpScreen, PAL_XY(294, boxY + 10));
         }
      }
      {
         LPCBITMAPRLE lpDn = PAL_SpriteGetFrame(gpSpriteUI, SPRITENUM_CURSOR_YELLOW);
         if (lpDn != NULL)
         {
            PAL_RLEBlitToSurface(lpDn, gpScreen, PAL_XY(294, boxY + boxH - 20));
         }
      }

      VIDEO_UpdateScreen(&panelRect);

      // 4. Handle vertical finger-drag scrolling (unlimited!)
      {
         int scrollDelta = PAL_TouchPollScrollRows(20);
         if (scrollDelta != 0)
         {
            iTopSlot += scrollDelta;
            if (iTopSlot < 1)
            {
               iTopSlot = 1;
            }
            if (iCurSlot < iTopSlot)
            {
               iCurSlot = iTopSlot;
            }
            else if (iCurSlot >= iTopSlot + nVisibleRows)
            {
               iCurSlot = iTopSlot + nVisibleRows - 1;
            }
         }
      }

      // 5. Handle direct touch taps
      {
         INT tapX = 0, tapY = 0;
         if (PAL_TouchPollTap(&tapX, &tapY))
         {
            // Tap outside the right panel cancels/returns
            if (tapX < 162)
            {
               iResult = MENUITEM_VALUE_CANCELLED;
               break;
            }
            // In Save Mode, 1-tap on [+新进度 #N] saves to brand-new slot immediately!
            if (fIsSaving && tapX >= 164 && tapX <= 314 && tapY >= 2 && tapY <= 38)
            {
               iResult = maxExistingSlot + 1;
               break;
            }
            // Tap on right-edge up/down arrow zones pages up/down
            if (tapX >= 288 && tapX <= 316 && tapY >= boxY && tapY <= boxY + 28)
            {
               iTopSlot -= (nVisibleRows - 1);
               if (iTopSlot < 1) iTopSlot = 1;
               iCurSlot = iTopSlot;
            }
            else if (tapX >= 288 && tapX <= 316 && tapY >= boxY + boxH - 28 && tapY <= boxY + boxH + 4)
            {
               iTopSlot += (nVisibleRows - 1);
               iCurSlot = iTopSlot;
            }
            else if (tapX >= 164 && tapX <= 314 &&
                     tapY >= firstRowY - 5 && tapY < firstRowY + nVisibleRows * rowStep)
            {
               int hitRow = (tapY - (firstRowY - 5)) / rowStep;
               if (hitRow >= 0 && hitRow < nVisibleRows)
               {
                  int hitSlot = iTopSlot + hitRow;
                  iCurSlot = hitSlot;
                  if (fIsSaving || GetSavedTimes(hitSlot) > 0)
                  {
                     iResult = hitSlot;
                     break;
                  }
               }
            }
         }
      }

      // 6. Handle HUD [返回] & keyboard/controller keys
      if (g_InputState.dwKeyPress & kKeyMenu)
      {
         iResult = MENUITEM_VALUE_CANCELLED;
         break;
      }
      else if (g_InputState.dwKeyPress & kKeySearch)
      {
         if (fIsSaving || GetSavedTimes(iCurSlot) > 0)
         {
            iResult = iCurSlot;
            break;
         }
      }
      else if (g_InputState.dwKeyPress & (kKeyUp | kKeyLeft))
      {
         if (iCurSlot > 1)
         {
            iCurSlot--;
            if (iCurSlot < iTopSlot)
            {
               iTopSlot = iCurSlot;
            }
         }
      }
      else if (g_InputState.dwKeyPress & (kKeyDown | kKeyRight))
      {
         iCurSlot++;
         if (iCurSlot >= iTopSlot + nVisibleRows)
         {
            iTopSlot = iCurSlot - nVisibleRows + 1;
         }
      }
      else if (g_InputState.dwKeyPress & kKeyPgUp)
      {
         iCurSlot -= nVisibleRows;
         if (iCurSlot < 1) iCurSlot = 1;
         iTopSlot -= nVisibleRows;
         if (iTopSlot < 1) iTopSlot = 1;
      }
      else if (g_InputState.dwKeyPress & kKeyPgDn)
      {
         iCurSlot += nVisibleRows;
         iTopSlot += nVisibleRows;
      }

      // Clear key state BEFORE sleeping so events arriving during sleep are preserved!
      PAL_ClearKeyState();
      for (i = 0; i < 4; i++)
      {
         SDL_Delay(10);
         PAL_ProcessEvent();
         if (g_InputState.dwKeyPress != 0 || PAL_TouchHasPendingInput())
         {
            break;
         }
      }
   }

   if (pSavedBg != NULL)
   {
      VIDEO_CopySurface(pSavedBg, NULL, gpScreen, NULL);
      VIDEO_FreeSurface(pSavedBg);
   }
   VIDEO_UpdateScreen(&panelRect);

   PAL_ClearKeyState();
   PAL_TouchClearGestures();
   PAL_TouchSetMode(prevTouchMode);
   return iResult;
}

static
WORD
PAL_SelectionMenu(
	int   nWords,
	int   nDefault,
	WORD  wItems[]
)
/*++
  Purpose:

    Show a common selection box.

  Parameters:

    [IN]  nWords - number of emnu items.
	[IN]  nDefault - index of default item.
	[IN]  wItems - item word array.

  Return value:

    User-selected index.

--*/
{
	LPBOX           rgpBox[4];
	MENUITEM        rgMenuItem[4];
	int             w[4] = {
		(nWords >= 1 && wItems[0]) ? PAL_WordWidth(wItems[0]) : 1,
		(nWords >= 2 && wItems[1]) ? PAL_WordWidth(wItems[1]) : 1,
		(nWords >= 3 && wItems[2]) ? PAL_WordWidth(wItems[2]) : 1,
		(nWords >= 4 && wItems[3]) ? PAL_WordWidth(wItems[3]) : 1 };
	int             dx[4] = { (w[0] - 1) * 16, (w[1] - 1) * 16, (w[2] - 1) * 16, (w[3] - 1) * 16 }, i;
	PAL_POS         pos[4] = { PAL_XY(145, 110), PAL_XY(220 + dx[0], 110), PAL_XY(145, 160), PAL_XY(220 + dx[2], 160) };
	WORD            wReturnValue;

	const SDL_Rect  rect = { 130, 100, 125 + max(dx[0] + dx[1], dx[2] + dx[3]), 100 };

	for (i = 0; i < nWords; i++)
		if (nWords > i && !wItems[i])
			return MENUITEM_VALUE_CANCELLED;

	//
	// Create menu items
	//
	for (i = 0; i < nWords; i++)
	{
		rgMenuItem[i].fEnabled = TRUE;
		rgMenuItem[i].pos = pos[i];
		rgMenuItem[i].wValue = i;
		rgMenuItem[i].wNumWord = wItems[i];
	}

	//
	// Create the boxes
	//
	dx[1] = dx[0]; dx[3] = dx[2]; dx[0] = dx[2] = 0;
	for (i = 0; i < nWords; i++)
	{
		rgpBox[i] = PAL_CreateSingleLineBox(PAL_XY(130 + 75 * (i % 2) + dx[i], 100 + 50 * (i / 2)), w[i] + 1, TRUE);
	}

	//
	// Activate the menu
	//
	wReturnValue = PAL_ReadMenu(NULL, rgMenuItem, nWords, nDefault, MENUITEM_COLOR);

	//
	// Delete the boxes
	//
	for (i = 0; i < nWords; i++)
	{
		PAL_DeleteBox(rgpBox[i]);
	}

	VIDEO_UpdateScreen(&rect);

	return wReturnValue;
}

WORD
PAL_TripleMenu(
   WORD  wThirdWord
)
/*++
  Purpose:

    Show a triple-selection box.

  Parameters:

    None.

  Return value:

    User-selected index.

--*/
{
   WORD wItems[3] = { CONFIRMMENU_LABEL_NO, CONFIRMMENU_LABEL_YES, wThirdWord };
   return PAL_SelectionMenu(3, 0, wItems);
}

BOOL
PAL_ConfirmMenu(
   VOID
)
/*++
  Purpose:

    Show a "Yes or No?" confirm box.

  Parameters:

    None.

  Return value:

    TRUE if user selected Yes, FALSE if selected No.

--*/
{
   WORD wItems[2] = { CONFIRMMENU_LABEL_NO, CONFIRMMENU_LABEL_YES };
   WORD wReturnValue = PAL_SelectionMenu(2, 0, wItems);

   return (wReturnValue == MENUITEM_VALUE_CANCELLED || wReturnValue == 0) ? FALSE : TRUE;
}

BOOL
PAL_SwitchMenu(
   BOOL      fEnabled
)
/*++
  Purpose:

    Show a "Enable/Disable" selection box.

  Parameters:

    [IN]  fEnabled - whether the option is originally enabled or not.

  Return value:

    TRUE if user selected "Enable", FALSE if selected "Disable".

--*/
{
   WORD wItems[2] = { SWITCHMENU_LABEL_DISABLE, SWITCHMENU_LABEL_ENABLE };
   WORD wReturnValue = PAL_SelectionMenu(2, fEnabled ? 1 : 0, wItems);
   return (wReturnValue == MENUITEM_VALUE_CANCELLED) ? fEnabled : ((wReturnValue == 0) ? FALSE : TRUE);
}

#ifndef PAL_CLASSIC

static VOID
PAL_BattleSpeedMenu(
   VOID
)
/*++
  Purpose:

    Show the Battle Speed selection box.

  Parameters:

    None.

  Return value:

    None.

--*/
{
   LPBOX           lpBox;
   WORD            wReturnValue;
   const SDL_Rect  rect = {131, 100, 165, 50};

   MENUITEM        rgMenuItem[5] = {
      { 1,   BATTLESPEEDMENU_LABEL_1,       TRUE,   PAL_XY(145, 110) },
      { 2,   BATTLESPEEDMENU_LABEL_2,       TRUE,   PAL_XY(170, 110) },
      { 3,   BATTLESPEEDMENU_LABEL_3,       TRUE,   PAL_XY(195, 110) },
      { 4,   BATTLESPEEDMENU_LABEL_4,       TRUE,   PAL_XY(220, 110) },
      { 5,   BATTLESPEEDMENU_LABEL_5,       TRUE,   PAL_XY(245, 110) },
   };

   //
   // Create the boxes
   //
   lpBox = PAL_CreateSingleLineBox(PAL_XY(131, 100), 8, TRUE);

   //
   // Activate the menu
   //
   wReturnValue = PAL_ReadMenu(NULL, rgMenuItem, 5, gpGlobals->bBattleSpeed - 1,
      MENUITEM_COLOR);

   //
   // Delete the boxes
   //
   PAL_DeleteBox(lpBox);

   VIDEO_UpdateScreen(&rect);

   if (wReturnValue != MENUITEM_VALUE_CANCELLED)
   {
      gpGlobals->bBattleSpeed = wReturnValue;
   }
}

#endif

LPBOX
PAL_ShowCash(
   DWORD      dwCash
)
/*++
  Purpose:

    Show the cash amount at the top left corner of the screen.

  Parameters:

    [IN]  dwCash - amount of cash.

  Return value:

    pointer to the saved screen part.

--*/
{
   LPBOX     lpBox;

   //
   // Create the box.
   //
   lpBox = PAL_CreateSingleLineBox(PAL_XY(0, 0), 5, TRUE);
   if (lpBox == NULL)
   {
      return NULL;
   }

   //
   // Draw the text label.
   //
   PAL_DrawText(PAL_GetWord(CASH_LABEL), PAL_XY(10, 10), 0, FALSE, FALSE, FALSE);

   //
   // Draw the cash amount.
   //
   PAL_DrawNumber(dwCash, 6, PAL_XY(49, 14), kNumColorYellow, kNumAlignRight);

   return lpBox;
}

static VOID
PAL_SystemMenu_OnItemChange(
   WORD        wCurrentItem
)
/*++
  Purpose:

    Callback function when user selected another item in the system menu.

  Parameters:

    [IN]  wCurrentItem - current selected item.

  Return value:

    None.

--*/
{
   gpGlobals->iCurSystemMenuItem = wCurrentItem - 1;
}

static BOOL
PAL_SystemMenu(
   VOID
)
/*++
  Purpose:

    Show the system menu.

  Parameters:

    None.

  Return value:

    TRUE if user made some operations in the menu, FALSE if user cancelled.

--*/
{
   LPBOX               lpMenuBox;
   WORD                wReturnValue;
   int                 iSlot, i;
   const SDL_Rect      rect = {40, 54, 280, 146};

   //
   // Create menu items
   //
   const MENUITEM      rgSystemMenuItem[] =
   {
      // value  label                        enabled   pos
      { 1,      SYSMENU_LABEL_SAVE,          TRUE,     PAL_XY(53, 66) },
      { 2,      SYSMENU_LABEL_LOAD,          TRUE,     PAL_XY(53, 66 + 18) },
      { 3,      SYSMENU_LABEL_MUSIC,         TRUE,     PAL_XY(53, 66 + 36) },
      { 4,      SYSMENU_LABEL_SOUND,         TRUE,     PAL_XY(53, 66 + 54) },
      { 7,      SYSMENU_LABEL_LAUNCHSETTING, TRUE,     PAL_XY(53, 66 + 72) },
      { 5,      SYSMENU_LABEL_QUIT,          TRUE,     PAL_XY(53, 66 + 90) },
#if !defined(PAL_CLASSIC)
      { 6,      SYSMENU_LABEL_BATTLEMODE,    TRUE,     PAL_XY(53, 66 + 108) },
#endif
   };
   const int           nSystemMenuItem = sizeof(rgSystemMenuItem) / sizeof(MENUITEM);

   //
   // Create the menu box.
   //
   lpMenuBox = PAL_CreateBox(PAL_XY(40, 54), nSystemMenuItem - 1, PAL_MenuTextMaxWidth(rgSystemMenuItem, nSystemMenuItem) - 1, 0, TRUE);

   //
   // Perform the menu.
   //
   wReturnValue = PAL_ReadMenu(PAL_SystemMenu_OnItemChange, rgSystemMenuItem, nSystemMenuItem, gpGlobals->iCurSystemMenuItem, MENUITEM_COLOR);

   if (wReturnValue == MENUITEM_VALUE_CANCELLED)
   {
      //
      // User cancelled the menu
      //
      PAL_DeleteBox(lpMenuBox);
      VIDEO_UpdateScreen(&rect);
      return FALSE;
   }

   switch (wReturnValue)
   {
   case 1:
      //
      // Save game (unlimited slots)
      //
      g_fSaveSlotIsSaving = TRUE;
      iSlot = PAL_SaveSlotMenu(gpGlobals->bCurrentSaveSlot);
      g_fSaveSlotIsSaving = FALSE;

      if (iSlot != MENUITEM_VALUE_CANCELLED && iSlot >= 1)
      {
         WORD wSavedTimes = 0;
         gpGlobals->bCurrentSaveSlot = iSlot;
         PAL_ScanSaveSlots(NULL, &wSavedTimes);
         PAL_SaveGame(iSlot, wSavedTimes + 1);
      }
      break;

   case 2:
      //
      // Load game (unlimited slots)
      //
      g_fSaveSlotIsSaving = FALSE;
      iSlot = PAL_SaveSlotMenu(gpGlobals->bCurrentSaveSlot);
      if (iSlot != MENUITEM_VALUE_CANCELLED && iSlot >= 1)
      {
         AUDIO_PlayMusic(0, FALSE, 1);
         PAL_FadeOut(1);
         PAL_ReloadInNextTick(iSlot);
      }
      break;

   case 3:
      //
      // Music
      //
      AUDIO_EnableMusic(PAL_SwitchMenu(AUDIO_MusicEnabled()));
      if (gConfig.eMIDISynth == SYNTH_NATIVE && gConfig.eMusicType == MUSIC_MIDI)
      {
         AUDIO_PlayMusic(AUDIO_MusicEnabled() ? gpGlobals->wNumMusic : 0, AUDIO_MusicEnabled(), 0);
      }
      break;

   case 4:
      //
      // Sound
      //
      AUDIO_EnableSound(PAL_SwitchMenu(AUDIO_SoundEnabled()));
      break;

   case 5:
      //
      // Quit
      //
      PAL_QuitGame();
      break;

   case 7:
      //
      // Live System Configuration Page (Performance HUD, MIDI/RIX synth, Display, etc.)
      //
#if defined(__IOS__)
      {
         INT restartReq = UTIL_ShowSystemSettingsModal(TRUE, (int)gpGlobals->wNumMusic);
         if (restartReq == 1)
         {
            PAL_DeleteBox(lpMenuBox);
            PAL_SaveConfig();
            AUDIO_PlayMusic(0, FALSE, 1);
            PAL_FadeOut(1);
            PAL_Shutdown(0);
            return TRUE;
         }
      }
#else
      gConfig.fLaunchSetting = TRUE;
      PAL_SaveConfig();
      AUDIO_PlayMusic(0, FALSE, 2);
      PAL_FadeOut(2);
      PAL_Shutdown(0);
#endif
      break;

#if !defined(PAL_CLASSIC)
   case 6:
      //
      // Battle Mode
      //
      PAL_BattleSpeedMenu();
      break;
#endif
   }

   PAL_DeleteBox(lpMenuBox);
   return TRUE;
}

VOID
PAL_InGameMagicMenu(
   VOID
)
/*++
  Purpose:

    Show the magic menu.

  Parameters:

    None.

  Return value:

    None.

--*/
{
   MENUITEM         rgMenuItem[MAX_PLAYERS_IN_PARTY];
   int              i, y;
   static WORD      w;
   WORD             wMagic;

   if (gpGlobals->wMaxPartyMemberIndex == 0)
   {
      w = 0;
      goto start_magicmenu;
   }

   //
   // Draw the player info boxes
   //
   y = 45;

   for (i = 0; i <= gpGlobals->wMaxPartyMemberIndex; i++)
   {
      PAL_PlayerInfoBox(PAL_XY(y, 165), gpGlobals->rgParty[i].wPlayerRole, 100,
         TIMEMETER_COLOR_DEFAULT, TRUE);
      y += 78;
   }

   y = 75;

   //
   // Generate one menu items for each player in the party
   //
   for (i = 0; i <= gpGlobals->wMaxPartyMemberIndex; i++)
   {
      assert(i <= MAX_PLAYERS_IN_PARTY);

      rgMenuItem[i].wValue = i;
      rgMenuItem[i].wNumWord =
         gpGlobals->g.PlayerRoles.rgwName[gpGlobals->rgParty[i].wPlayerRole];
      rgMenuItem[i].fEnabled =
         (gpGlobals->g.PlayerRoles.rgwHP[gpGlobals->rgParty[i].wPlayerRole] > 0);
      rgMenuItem[i].pos = PAL_XY(48, y);

      y += 18;
   }

   //
   // Draw the box
   //
   PAL_CreateBox(PAL_XY(35, 62), gpGlobals->wMaxPartyMemberIndex, PAL_MenuTextMaxWidth(rgMenuItem, sizeof(rgMenuItem)/sizeof(MENUITEM)) - 1, 0, FALSE);

   w = PAL_ReadMenu(NULL, rgMenuItem, gpGlobals->wMaxPartyMemberIndex + 1, w, MENUITEM_COLOR);

   if (w == MENUITEM_VALUE_CANCELLED)
   {
      return;
   }

start_magicmenu:

   wMagic = 0;

   while (TRUE)
   {
      wMagic = PAL_MagicSelectionMenu(gpGlobals->rgParty[w].wPlayerRole, FALSE, wMagic);
      if (wMagic == 0)
      {
         break;
      }

      VIDEO_BackupScreen(gpScreen);

      if (gpGlobals->g.rgObject[wMagic].magic.wFlags & kMagicFlagApplyToAll)
      {
         gpGlobals->g.rgObject[wMagic].magic.wScriptOnUse =
            PAL_RunTriggerScript(gpGlobals->g.rgObject[wMagic].magic.wScriptOnUse, 0);

         if (g_fScriptSuccess)
         {
            gpGlobals->g.rgObject[wMagic].magic.wScriptOnSuccess =
               PAL_RunTriggerScript(gpGlobals->g.rgObject[wMagic].magic.wScriptOnSuccess, 0);

            if(g_fScriptSuccess)
               gpGlobals->g.PlayerRoles.rgwMP[gpGlobals->rgParty[w].wPlayerRole] -=
               gpGlobals->g.lprgMagic[gpGlobals->g.rgObject[wMagic].magic.wMagicNumber].wCostMP;
         }

         if (gpGlobals->fNeedToFadeIn)
         {
            PAL_FadeIn(gpGlobals->wNumPalette, gpGlobals->fNightPalette, 1);
            gpGlobals->fNeedToFadeIn = FALSE;
         }
      }
      else
      {
         //
         // Need to select which player to use the magic on.
         //
         WORD         wPlayer = 0;
         INT          iPrimedPlayer = (gpGlobals->wMaxPartyMemberIndex == 0) ? 0 : -1;
         SDL_Rect     rect;
         PALTOUCHMODE prevTargetMode = PAL_TouchGetMode();

         PAL_TouchSetMode(kTouchModeMenu);
         PAL_TouchClearGestures();
         PAL_ClearKeyState();

         while (wPlayer != MENUITEM_VALUE_CANCELLED)
         {
            BOOL fLocked = (iPrimedPlayer == (INT)wPlayer);
            WORD wTargetRole = gpGlobals->rgParty[wPlayer].wPlayerRole;

            rect.x = 0;
            rect.y = 158;
            rect.w = 320;
            rect.h = 6;

            VIDEO_RestoreScreen(gpScreen);

            // Redraw updated player HP/MP boxes after restoring background
            y = 45;
            for (i = 0; i <= gpGlobals->wMaxPartyMemberIndex; i++)
            {
               PAL_PlayerInfoBox(PAL_XY(y, 165), gpGlobals->rgParty[i].wPlayerRole, 100,
                  TIMEMETER_COLOR_DEFAULT, FALSE);
               y += 78;
            }

            // Draw Target Confirmation Box above party cards
            PAL_CreateSingleLineBox(PAL_XY(96, 120), 7, FALSE);
            PAL_DrawText(PAL_GetWord(gpGlobals->g.PlayerRoles.rgwName[wTargetRole]),
               PAL_XY(110, 129), fLocked ? MENUITEM_COLOR_SELECTED : MENUITEM_COLOR, TRUE, FALSE, FALSE);
            if (fLocked)
            {
               LPCBITMAPRLE lpConfCur = PAL_SpriteGetFrame(gpSpriteUI, SPRITENUM_CURSOR_YELLOW);
               if (lpConfCur != NULL)
               {
                  PAL_RLEBlitToSurface(lpConfCur, gpScreen, PAL_XY(214, 134));
               }
            }

            PAL_RLEBlitToSurface(
               PAL_SpriteGetFrame(gpSpriteUI, fLocked ? SPRITENUM_CURSOR_YELLOW_UP : SPRITENUM_CURSOR_UP),
               gpScreen, PAL_XY(75 + 78 * wPlayer, rect.y));

            VIDEO_UpdateScreen(NULL);

            while (TRUE)
            {
               INT tapX = 0, tapY = 0;
               BOOL fOnlySwitchedTarget = FALSE;
               PAL_ProcessEvent();

               if (PAL_TouchPollTap(&tapX, &tapY))
               {
                  if (fLocked && tapX >= 94 && tapX <= 242 && tapY >= 118 && tapY <= 152)
                  {
                     g_InputState.dwKeyPress |= kKeySearch;
                  }
                  else if (tapY >= 152 && tapY <= 200)
                  {
                     for (i = 0; i <= gpGlobals->wMaxPartyMemberIndex; i++)
                     {
                        int px = 45 + 78 * i;
                        if (tapX >= px - 2 && tapX <= px + 76)
                        {
                           if ((INT)i == (INT)wPlayer && iPrimedPlayer == (INT)i)
                           {
                              // 2nd tap on already-highlighted teammate -> Confirm magic!
                              g_InputState.dwKeyPress |= kKeySearch;
                           }
                           else
                           {
                              // 1st tap -> Highlight & lock onto teammate!
                              wPlayer = (WORD)i;
                              iPrimedPlayer = (INT)i;
                              fOnlySwitchedTarget = TRUE;
                           }
                           break;
                        }
                     }
                  }
                  else if (tapY < 114)
                  {
                     g_InputState.dwKeyPress |= kKeyMenu;
                  }
               }

               if (fOnlySwitchedTarget)
               {
                  PAL_ClearKeyState();
                  break;
               }

               if (g_InputState.dwKeyPress & kKeyMenu)
               {
                  wPlayer = MENUITEM_VALUE_CANCELLED;
                  break;
               }
               else if (g_InputState.dwKeyPress & kKeySearch)
               {
                  gpGlobals->g.rgObject[wMagic].magic.wScriptOnUse =
                     PAL_RunTriggerScript(gpGlobals->g.rgObject[wMagic].magic.wScriptOnUse,
                        gpGlobals->rgParty[wPlayer].wPlayerRole);

                  if (g_fScriptSuccess)
                  {
                     gpGlobals->g.rgObject[wMagic].magic.wScriptOnSuccess =
                        PAL_RunTriggerScript(gpGlobals->g.rgObject[wMagic].magic.wScriptOnSuccess,
                           gpGlobals->rgParty[wPlayer].wPlayerRole);

                     if (g_fScriptSuccess)
                     {
                        gpGlobals->g.PlayerRoles.rgwMP[gpGlobals->rgParty[w].wPlayerRole] -=
                           gpGlobals->g.lprgMagic[gpGlobals->g.rgObject[wMagic].magic.wMagicNumber].wCostMP;

                        //
                        // Check if we have run out of MP
                        //
                        if (gpGlobals->g.PlayerRoles.rgwMP[gpGlobals->rgParty[w].wPlayerRole] <
                           gpGlobals->g.lprgMagic[gpGlobals->g.rgObject[wMagic].magic.wMagicNumber].wCostMP)
                        {
                           //
                           // Don't go further if run out of MP
                           //
                           wPlayer = MENUITEM_VALUE_CANCELLED;
                        }
                     }
                  }

                  break;
               }
               else if (g_InputState.dwKeyPress & (kKeyLeft | kKeyUp))
               {
                  if (wPlayer > 0)
                  {
                     wPlayer--;
                     break;
                  }
               }
               else if (g_InputState.dwKeyPress & (kKeyRight | kKeyDown))
               {
                  if (wPlayer < gpGlobals->wMaxPartyMemberIndex)
                  {
                     wPlayer++;
                     break;
                  }
               }

               PAL_ClearKeyState();
               SDL_Delay(5);
            }
         }

         PAL_TouchSetMode(prevTargetMode);
      }

      //
      // Redraw the player info boxes
      //
      y = 45;

      for (i = 0; i <= gpGlobals->wMaxPartyMemberIndex; i++)
      {
         PAL_PlayerInfoBox(PAL_XY(y, 165), gpGlobals->rgParty[i].wPlayerRole, 100,
            TIMEMETER_COLOR_DEFAULT, TRUE);
         y += 78;
      }
   }
}

static VOID
PAL_InventoryMenu(
   VOID
)
/*++
  Purpose:

    Show the inventory menu.

  Parameters:

    None.

  Return value:

    None.

--*/
{
   static WORD      w = 0;

   MENUITEM        rgMenuItem[2] =
   {
      // value  label                     enabled   pos
      { 1,      INVMENU_LABEL_EQUIP,      TRUE,     PAL_XY(43, 73) },
      { 2,      INVMENU_LABEL_USE,        TRUE,     PAL_XY(43, 73 + 18) },
   };

   PAL_CreateBox(PAL_XY(30, 60), 1, PAL_MenuTextMaxWidth(rgMenuItem, sizeof(rgMenuItem)/sizeof(MENUITEM)) - 1, 0, FALSE);

   w = PAL_ReadMenu(NULL, rgMenuItem, 2, w - 1, MENUITEM_COLOR);

   switch (w)
   {
   case 1:
      PAL_GameEquipItem();
      break;

   case 2:
      PAL_GameUseItem();
      break;
   }
}

static VOID
PAL_InGameMenu_OnItemChange(
   WORD        wCurrentItem
)
/*++
  Purpose:

    Callback function when user selected another item in the in-game menu.

  Parameters:

    [IN]  wCurrentItem - current selected item.

  Return value:

    None.

--*/
{
   gpGlobals->iCurMainMenuItem = wCurrentItem - 1;
}

VOID
PAL_InGameMenu(
   VOID
)
/*++
  Purpose:

    Show the in-game main menu.

  Parameters:

    None.

  Return value:

    None.

--*/
{
   LPBOX                lpCashBox, lpMenuBox;
   WORD                 wReturnValue;
   
   // Fix render problem with shadow
   VIDEO_BackupScreen(gpScreen);

   //
   // Create menu items
   //
   MENUITEM        rgMainMenuItem[4] =
   {
      // value  label                      enabled   pos
      { 1,      GAMEMENU_LABEL_STATUS,     TRUE,     PAL_XY(16, 50) },
      { 2,      GAMEMENU_LABEL_MAGIC,      TRUE,     PAL_XY(16, 50 + 18) },
      { 3,      GAMEMENU_LABEL_INVENTORY,  TRUE,     PAL_XY(16, 50 + 36) },
      { 4,      GAMEMENU_LABEL_SYSTEM,     TRUE,     PAL_XY(16, 50 + 54) },
   };

   //
   // Display the cash amount.
   //
   lpCashBox = PAL_ShowCash(gpGlobals->dwCash);

   //
   // Create the menu box.
   //
   // Fix render problem with shadow
   lpMenuBox = PAL_CreateBox(PAL_XY(3, 37), 3, PAL_MenuTextMaxWidth(rgMainMenuItem, 4) - 1, 0, FALSE);

   //
   // Process the menu
   //
   while (TRUE)
   {
      wReturnValue = PAL_ReadMenu(PAL_InGameMenu_OnItemChange, rgMainMenuItem, 4,
         gpGlobals->iCurMainMenuItem, MENUITEM_COLOR);

      if (wReturnValue == MENUITEM_VALUE_CANCELLED)
      {
         break;
      }

      switch (wReturnValue)
      {
      case 1:
         //
         // Status
         //
         PAL_PlayerStatus();
         goto out;

      case 2:
         //
         // Magic
         //
         PAL_InGameMagicMenu();
         goto out;

      case 3:
         //
         // Inventory
         //
         PAL_InventoryMenu();
         goto out;

      case 4:
         //
         // System
         //
         if (PAL_SystemMenu())
         {
            goto out;
         }
         break;
      }
   }

out:
   //
   // Remove the boxes.
   //
   PAL_DeleteBox(lpCashBox);
   PAL_DeleteBox(lpMenuBox);

   // Fix render problem with shadow
   VIDEO_RestoreScreen(gpScreen);
}

VOID
PAL_InGameSystemMenu(
   VOID
)
{
   LPBOX lpCashBox;

   VIDEO_BackupScreen(gpScreen);
   lpCashBox = PAL_ShowCash(gpGlobals->dwCash);
   PAL_SystemMenu();
   PAL_DeleteBox(lpCashBox);
   VIDEO_RestoreScreen(gpScreen);
}

VOID
PAL_PlayerStatus(
   VOID
)
/*++
  Purpose:

    Show the player status.

  Parameters:

    None.

  Return value:

    None.

--*/
{
   PAL_LARGE BYTE   bufBackground[320 * 200];
   PAL_LARGE BYTE   bufImage[PAL_RLEBUFSIZE];
   PAL_LARGE BYTE   bufImageBox[50 * 49];
   int              labels0[] = {
      STATUS_LABEL_EXP, STATUS_LABEL_LEVEL, STATUS_LABEL_HP,
      STATUS_LABEL_MP
   };
   int              labels1[] = {
      STATUS_LABEL_EXP_LAYOUT, STATUS_LABEL_LEVEL_LAYOUT, STATUS_LABEL_HP_LAYOUT,
      STATUS_LABEL_MP_LAYOUT
   };
   int              labels[] = {
      STATUS_LABEL_ATTACKPOWER, STATUS_LABEL_MAGICPOWER, STATUS_LABEL_RESISTANCE,
      STATUS_LABEL_DEXTERITY, STATUS_LABEL_FLEERATE
   };
   int              iCurrent;
   int              iPlayerRole;
   int              i, j;
   WORD             w;
   PALTOUCHMODE     prevTouchMode = PAL_TouchGetMode();

   PAL_TouchSetMode(kTouchModeMenu);
   PAL_TouchClearGestures();
   PAL_MKFDecompressChunk(bufBackground, 320 * 200, STATUS_BACKGROUND_FBPNUM, gpGlobals->f.fpFBP);
   iCurrent = 0;

   if (gConfig.fUseCustomScreenLayout)
   {
      for (i = 0; i < 49; i++)
      {
         memcpy(&bufImageBox[i * 50], &bufBackground[(i + 39) * 320 + 247], 50);
      }
      for (i = 0; i < 49; i++)
      {
         memcpy(&bufBackground[(i + 125) * 320 + 81], &bufBackground[(i + 125) * 320 + 81 - 50], 50);
         memcpy(&bufBackground[(i + 141) * 320 + 141], &bufBackground[(i + 141) * 320 + 81 - 50], 50);
         memcpy(&bufBackground[(i + 133) * 320 + 201], &bufBackground[(i + 133) * 320 + 81 - 50], 50);
         memcpy(&bufBackground[(i + 101) * 320 + 251], &bufBackground[(i + 101) * 320 + 81 - 50], 50);
         memcpy(&bufBackground[(i + 39) * 320 + 247], &bufBackground[(i + 39) * 320 + 189 - 50], 50);
         if (i > 0) memcpy(&bufBackground[(i - 1) * 320 + 189], &bufBackground[(i - 1) * 320 + 189 - 50], 50);
      }
      for(i = 0; i < MAX_PLAYER_EQUIPMENTS; i++)
      {
         short x = PAL_X(gConfig.ScreenLayout.RoleEquipImageBoxes[i]);
         short y = PAL_Y(gConfig.ScreenLayout.RoleEquipImageBoxes[i]);
         short sx = (x < 0) ? -x : 0, sy = (y < 0) ? -y : 0, d = (x > 270) ? x - 270 : 0;
         if (sx >= 50 || sy >= 49 || x >= 320 || y >= 200) continue;
         for (; sy < 49 && y + sy < 200; sy++)
         {
            memcpy(&bufBackground[(y + sy) * 320 + x + sx], &bufImageBox[sy * 50 + sx], 50 - sx - d);
         }
      }
   }

   while (iCurrent >= 0 && iCurrent <= gpGlobals->wMaxPartyMemberIndex)
   {
      iPlayerRole = gpGlobals->rgParty[iCurrent].wPlayerRole;

      //
      // Draw the background image
      //
      PAL_FBPBlitToSurface(bufBackground, gpScreen);

      //
      // Draw the image of player role
      //
      if (PAL_MKFReadChunk(bufImage, PAL_RLEBUFSIZE, gpGlobals->g.PlayerRoles.rgwAvatar[iPlayerRole], gpGlobals->f.fpRGM) > 0)
      {
         PAL_RLEBlitToSurface(bufImage, gpScreen, gConfig.ScreenLayout.RoleImage);
      }

      //
      // Draw the equipments
      //
      for (i = 0; i < MAX_PLAYER_EQUIPMENTS; i++)
      {
         int offset;

         w = gpGlobals->g.PlayerRoles.rgwEquipment[i][iPlayerRole];

         if (w == 0)
         {
            continue;
         }

         //
         // Draw the image
         //
         if (PAL_MKFReadChunk(bufImage, PAL_RLEBUFSIZE,
            gpGlobals->g.rgObject[w].item.wBitmap, gpGlobals->f.fpBALL) > 0)
         {
            PAL_RLEBlitToSurface(bufImage, gpScreen,
               PAL_XY_OFFSET(gConfig.ScreenLayout.RoleEquipImageBoxes[i], 1, 1));
         }

         //
         // Draw the text label
         //
         offset = PAL_WordWidth(w) * 16;
         if (PAL_X(gConfig.ScreenLayout.RoleEquipNames[i]) + offset > 320)
         {
            offset = 320 - PAL_X(gConfig.ScreenLayout.RoleEquipNames[i]) - offset;
         }
         else
         {
            offset = 0;
         }
         int index = &gConfig.ScreenLayout.RoleEquipNames[i] - gConfig.ScreenLayoutArray;
         BOOL fShadow = (gConfig.ScreenLayoutFlag[index] & DISABLE_SHADOW) ? FALSE : TRUE;
         BOOL fUse8x8Font = (gConfig.ScreenLayoutFlag[index] & USE_8x8_FONT) ? TRUE : FALSE;
         PAL_DrawText(PAL_GetWord(w), PAL_XY_OFFSET(gConfig.ScreenLayout.RoleEquipNames[i], offset, 0), STATUS_COLOR_EQUIPMENT, fShadow, FALSE, fUse8x8Font);
      }

      //
      // Draw the text labels
      //
      for (i = 0; i < sizeof(labels0) / sizeof(int); i++)
      {
         int index = labels1[i];
         BOOL fShadow = (gConfig.ScreenLayoutFlag[index] & DISABLE_SHADOW) ? FALSE : TRUE;
         BOOL fUse8x8Font = (gConfig.ScreenLayoutFlag[index] & USE_8x8_FONT) ? TRUE : FALSE;
         PAL_DrawText(PAL_GetWord(labels0[i]), *(&gConfig.ScreenLayout.RoleExpLabel+i), MENUITEM_COLOR, fShadow, FALSE, fUse8x8Font);
      }
      for (i = 0; i < sizeof(labels) / sizeof(int); i++)
      {
         int index = &gConfig.ScreenLayout.RoleStatusLabels[i] - gConfig.ScreenLayoutArray;
         BOOL fShadow = (gConfig.ScreenLayoutFlag[index] & DISABLE_SHADOW) ? FALSE : TRUE;
         BOOL fUse8x8Font = (gConfig.ScreenLayoutFlag[index] & USE_8x8_FONT) ? TRUE : FALSE;
         PAL_DrawText(PAL_GetWord(labels[i]), gConfig.ScreenLayout.RoleStatusLabels[i], MENUITEM_COLOR, fShadow, FALSE, fUse8x8Font);
      }

      PAL_DrawText(PAL_GetWord(gpGlobals->g.PlayerRoles.rgwName[iPlayerRole]),
         gConfig.ScreenLayout.RoleName, MENUITEM_COLOR_CONFIRMED, TRUE, FALSE, FALSE);

      //
      // Draw the stats
      //
      if (gConfig.ScreenLayout.RoleExpSlash != 0)
	  {
         PAL_RLEBlitToSurface(PAL_SpriteGetFrame(gpSpriteUI, SPRITENUM_SLASH), gpScreen, gConfig.ScreenLayout.RoleExpSlash);
      }
      if (gConfig.ScreenLayout.RoleHPSlash != 0)
	  {
         PAL_RLEBlitToSurface(PAL_SpriteGetFrame(gpSpriteUI, SPRITENUM_SLASH), gpScreen, gConfig.ScreenLayout.RoleHPSlash);
      }
      if (gConfig.ScreenLayout.RoleMPSlash != 0)
	  {
         PAL_RLEBlitToSurface(PAL_SpriteGetFrame(gpSpriteUI, SPRITENUM_SLASH), gpScreen, gConfig.ScreenLayout.RoleMPSlash);
      }

      PAL_DrawNumber(gpGlobals->Exp.rgPrimaryExp[iPlayerRole].wExp, 5,
         gConfig.ScreenLayout.RoleCurrExp, kNumColorYellow, kNumAlignRight);
      PAL_DrawNumber(gpGlobals->g.rgLevelUpExp[gpGlobals->g.PlayerRoles.rgwLevel[iPlayerRole]], 5,
         gConfig.ScreenLayout.RoleNextExp, kNumColorCyan, kNumAlignRight);
      PAL_DrawNumber(gpGlobals->g.PlayerRoles.rgwLevel[iPlayerRole], 2,
         gConfig.ScreenLayout.RoleLevel, kNumColorYellow, kNumAlignRight);
      PAL_DrawNumber(gpGlobals->g.PlayerRoles.rgwHP[iPlayerRole], 4,
         gConfig.ScreenLayout.RoleCurHP, kNumColorYellow, kNumAlignRight);
      PAL_DrawNumber(gpGlobals->g.PlayerRoles.rgwMaxHP[iPlayerRole], 4,
         gConfig.ScreenLayout.RoleMaxHP, kNumColorBlue, kNumAlignRight);
      PAL_DrawNumber(gpGlobals->g.PlayerRoles.rgwMP[iPlayerRole], 4,
         gConfig.ScreenLayout.RoleCurMP, kNumColorYellow, kNumAlignRight);
      PAL_DrawNumber(gpGlobals->g.PlayerRoles.rgwMaxMP[iPlayerRole], 4,
         gConfig.ScreenLayout.RoleMaxMP, kNumColorBlue, kNumAlignRight);

      PAL_DrawNumber(PAL_GetPlayerAttackStrength(iPlayerRole), 4,
         gConfig.ScreenLayout.RoleStatusValues[0], kNumColorYellow, kNumAlignRight);
      PAL_DrawNumber(PAL_GetPlayerMagicStrength(iPlayerRole), 4,
         gConfig.ScreenLayout.RoleStatusValues[1], kNumColorYellow, kNumAlignRight);
      PAL_DrawNumber(PAL_GetPlayerDefense(iPlayerRole), 4,
         gConfig.ScreenLayout.RoleStatusValues[2], kNumColorYellow, kNumAlignRight);
      PAL_DrawNumber(PAL_GetPlayerDexterity(iPlayerRole), 4,
         gConfig.ScreenLayout.RoleStatusValues[3], kNumColorYellow, kNumAlignRight);
      PAL_DrawNumber(PAL_GetPlayerFleeRate(iPlayerRole), 4,
         gConfig.ScreenLayout.RoleStatusValues[4], kNumColorYellow, kNumAlignRight);

      //
      // Draw all poisons
      //
      for (i = j = 0; i < MAX_POISONS; i++)
      {
         w = gpGlobals->rgPoisonStatus[i][iCurrent].wPoisonID;

         if (w != 0 && gpGlobals->g.rgObject[w].poison.wPoisonLevel <= 3)
         {
            PAL_DrawText(PAL_GetWord(w), gConfig.ScreenLayout.RolePoisonNames[j++], (BYTE)(gpGlobals->g.rgObject[w].poison.wColor + 10), TRUE, FALSE, FALSE);
         }
      }

      //
      // Update the screen
      //
      VIDEO_UpdateScreen(NULL);

      //
      // Wait for input
      //
      PAL_ClearKeyState();

      while (TRUE)
      {
         INT tapX = 0, tapY = 0;
         INT swipe = 0;
         UTIL_Delay(5);

         swipe = PAL_TouchPollHorizSwipe();
         if (swipe < 0 && gpGlobals->wMaxPartyMemberIndex > 0)
         {
            iCurrent = (iCurrent + 1) % (gpGlobals->wMaxPartyMemberIndex + 1);
            break;
         }
         else if (swipe > 0 && gpGlobals->wMaxPartyMemberIndex > 0)
         {
            iCurrent = (iCurrent > 0) ? (iCurrent - 1) : gpGlobals->wMaxPartyMemberIndex;
            break;
         }

         if (PAL_TouchPollTap(&tapX, &tapY))
         {
            BOOL fHitEquipSlot = FALSE;
            if (!gpGlobals->fInBattle)
            {
               for (i = 0; i < MAX_PLAYER_EQUIPMENTS; i++)
               {
                  int bx = PAL_X(gConfig.ScreenLayout.RoleEquipImageBoxes[i]);
                  int by = PAL_Y(gConfig.ScreenLayout.RoleEquipImageBoxes[i]);
                  if (tapX >= bx - 2 && tapX <= bx + 52 && tapY >= by - 2 && tapY <= by + 52)
                  {
                     fHitEquipSlot = TRUE;
                     break;
                  }
               }
            }
            if (fHitEquipSlot)
            {
               // Direct tap on an equipment slot opens Armory/Equipment screen immediately!
               PAL_GameEquipItem();
               PAL_TouchSetMode(kTouchModeMenu);
               PAL_TouchClearGestures();
               break;
            }
            else if (gpGlobals->wMaxPartyMemberIndex > 0)
            {
               if (tapX < 95)
               {
                  iCurrent = (iCurrent > 0) ? (iCurrent - 1) : gpGlobals->wMaxPartyMemberIndex;
                  break;
               }
               else if (tapX <= 185)
               {
                  iCurrent = (iCurrent + 1) % (gpGlobals->wMaxPartyMemberIndex + 1);
                  break;
               }
            }
         }

         if (g_InputState.dwKeyPress & kKeyMenu)
         {
            iCurrent = -1;
            break;
         }
         else if (g_InputState.dwKeyPress & (kKeyLeft | kKeyUp))
         {
            iCurrent--;
            break;
         }
         else if (g_InputState.dwKeyPress & (kKeyRight | kKeyDown | kKeySearch))
         {
            iCurrent++;
            break;
         }
      }
   }

   PAL_TouchSetMode(prevTouchMode);
}

WORD
PAL_ItemUseMenu(
   WORD           wItemToUse
)
/*++
  Purpose:

    Show the use item menu.

  Parameters:

    [IN]  wItemToUse - the object ID of the item to use.

  Return value:

    The selected player to use the item onto.
    MENUITEM_VALUE_CANCELLED if user cancelled.

--*/
{
   BYTE           bColor, bSelectedColor;
   PAL_LARGE BYTE bufImage[2048];
   DWORD          dwColorChangeTime;
   static SHORT   sSelectedPlayer = 0;
   BOOL           fRolePrimed = (gpGlobals->wMaxPartyMemberIndex == 0);
   SDL_Rect       rect = {110, 2, 200, 180};
   int            i;
   PALTOUCHMODE   prevTouchMode = PAL_TouchGetMode();

   PAL_TouchSetMode(kTouchModeMenu);
   PAL_TouchClearGestures();
   bSelectedColor = MENUITEM_COLOR_SELECTED_FIRST;
   dwColorChangeTime = 0;

   while (TRUE)
   {
      if (sSelectedPlayer > gpGlobals->wMaxPartyMemberIndex)
      {
         sSelectedPlayer = 0;
      }

      //
      // Draw the box
      //
      PAL_CreateBox(PAL_XY(110, 2), 7, 9, 0, FALSE);

      //
      // Draw the stats of the selected player
      //
      PAL_DrawText(PAL_GetWord(STATUS_LABEL_LEVEL), PAL_XY(200, 16),
         ITEMUSEMENU_COLOR_STATLABEL, TRUE, FALSE, FALSE);
      PAL_DrawText(PAL_GetWord(STATUS_LABEL_HP), PAL_XY(200, 34),
         ITEMUSEMENU_COLOR_STATLABEL, TRUE, FALSE, FALSE);
      PAL_DrawText(PAL_GetWord(STATUS_LABEL_MP), PAL_XY(200, 52),
         ITEMUSEMENU_COLOR_STATLABEL, TRUE, FALSE, FALSE);
      PAL_DrawText(PAL_GetWord(STATUS_LABEL_ATTACKPOWER), PAL_XY(200, 70),
         ITEMUSEMENU_COLOR_STATLABEL, TRUE, FALSE, FALSE);
      PAL_DrawText(PAL_GetWord(STATUS_LABEL_MAGICPOWER), PAL_XY(200, 88),
         ITEMUSEMENU_COLOR_STATLABEL, TRUE, FALSE, FALSE);
      PAL_DrawText(PAL_GetWord(STATUS_LABEL_RESISTANCE), PAL_XY(200, 106),
         ITEMUSEMENU_COLOR_STATLABEL, TRUE, FALSE, FALSE);
      PAL_DrawText(PAL_GetWord(STATUS_LABEL_DEXTERITY), PAL_XY(200, 124),
         ITEMUSEMENU_COLOR_STATLABEL, TRUE, FALSE, FALSE);
      PAL_DrawText(PAL_GetWord(STATUS_LABEL_FLEERATE), PAL_XY(200, 142),
         ITEMUSEMENU_COLOR_STATLABEL, TRUE, FALSE, FALSE);

      i = gpGlobals->rgParty[sSelectedPlayer].wPlayerRole;

      PAL_DrawNumber(gpGlobals->g.PlayerRoles.rgwLevel[i], 4, PAL_XY(240, 20),
         kNumColorYellow, kNumAlignRight);

      PAL_RLEBlitToSurface(PAL_SpriteGetFrame(gpSpriteUI, SPRITENUM_SLASH), gpScreen,
         PAL_XY(263, 38));
      PAL_DrawNumber(gpGlobals->g.PlayerRoles.rgwMaxHP[i], 4,
         PAL_XY(261, 40), kNumColorBlue, kNumAlignRight);
      PAL_DrawNumber(gpGlobals->g.PlayerRoles.rgwHP[i], 4,
         PAL_XY(240, 37), kNumColorYellow, kNumAlignRight);

      PAL_RLEBlitToSurface(PAL_SpriteGetFrame(gpSpriteUI, SPRITENUM_SLASH), gpScreen,
         PAL_XY(263, 56));
      PAL_DrawNumber(gpGlobals->g.PlayerRoles.rgwMaxMP[i], 4,
         PAL_XY(261, 58), kNumColorBlue, kNumAlignRight);
      PAL_DrawNumber(gpGlobals->g.PlayerRoles.rgwMP[i], 4,
         PAL_XY(240, 55), kNumColorYellow, kNumAlignRight);

      PAL_DrawNumber(PAL_GetPlayerAttackStrength(i), 4, PAL_XY(240, 74),
         kNumColorYellow, kNumAlignRight);
      PAL_DrawNumber(PAL_GetPlayerMagicStrength(i), 4, PAL_XY(240, 92),
         kNumColorYellow, kNumAlignRight);
      PAL_DrawNumber(PAL_GetPlayerDefense(i), 4, PAL_XY(240, 110),
         kNumColorYellow, kNumAlignRight);
      PAL_DrawNumber(PAL_GetPlayerDexterity(i), 4, PAL_XY(240, 128),
         kNumColorYellow, kNumAlignRight);
      PAL_DrawNumber(PAL_GetPlayerFleeRate(i), 4, PAL_XY(240, 146),
         kNumColorYellow, kNumAlignRight);

      //
      // Draw the names of the players in the party
      //
      for (i = 0; i <= gpGlobals->wMaxPartyMemberIndex; i++)
      {
         if (i == sSelectedPlayer)
         {
            bColor = bSelectedColor;
         }
         else
         {
            bColor = MENUITEM_COLOR;
         }

         PAL_DrawText(PAL_GetWord(gpGlobals->g.PlayerRoles.rgwName[gpGlobals->rgParty[i].wPlayerRole]),
            PAL_XY(125, 16 + 20 * i), bColor, TRUE, FALSE, FALSE);

         if (i == sSelectedPlayer && fRolePrimed)
         {
            LPCBITMAPRLE lpLockCur = PAL_SpriteGetFrame(gpSpriteUI, SPRITENUM_CURSOR_YELLOW);
            if (lpLockCur != NULL)
            {
               PAL_RLEBlitToSurface(lpLockCur, gpScreen, PAL_XY(180, 21 + 20 * i));
            }
         }
      }

#if defined(__IOS__)
      {
         LPCBITMAPRLE pItemBox = PAL_SpriteGetFrame(gpSpriteUI, SPRITENUM_ITEMBOX);
         PAL_DrawHextechBoxToScreen(
            120,
            80,
            pItemBox ? PAL_RLEGetWidth(pItemBox) : 70,
            pItemBox ? PAL_RLEGetHeight(pItemBox) : 60,
            4,
            0
         );
      }
#else
      PAL_RLEBlitToSurface(PAL_SpriteGetFrame(gpSpriteUI, SPRITENUM_ITEMBOX), gpScreen,
         PAL_XY(120, 80));
#endif

      i = PAL_GetItemAmount(wItemToUse);

      if (i > 0)
      {
         //
         // Draw the picture of the item
         //
         if (PAL_MKFReadChunk(bufImage, 2048,
            gpGlobals->g.rgObject[wItemToUse].item.wBitmap, gpGlobals->f.fpBALL) > 0)
         {
            PAL_RLEBlitToSurface(bufImage, gpScreen, PAL_XY(127, 88));
         }

         //
         // Draw the amount and label of the item
         //
         PAL_DrawText(PAL_GetWord(wItemToUse), PAL_XY(116, 143), STATUS_COLOR_EQUIPMENT, TRUE, FALSE, FALSE);
         PAL_DrawNumber(i, 2, PAL_XY(170, 133), kNumColorCyan, kNumAlignRight);
      }

      //
      // Update the screen area
      //
      VIDEO_UpdateScreen(&rect);

      //
      // Wait for key
      //
      PAL_ClearKeyState();

      while (TRUE)
      {
         //
         // See if we should change the highlight color
         //
         if (SDL_TICKS_PASSED(SDL_GetTicks(), dwColorChangeTime))
         {
            if ((WORD)bSelectedColor + 1 >=
               (WORD)MENUITEM_COLOR_SELECTED_FIRST + MENUITEM_COLOR_SELECTED_TOTALNUM)
            {
               bSelectedColor = MENUITEM_COLOR_SELECTED_FIRST;
            }
            else
            {
               bSelectedColor++;
            }

            dwColorChangeTime = SDL_GetTicks() + (600 / MENUITEM_COLOR_SELECTED_TOTALNUM);

            //
            // Redraw the selected item.
            //
            PAL_DrawText(
               PAL_GetWord(gpGlobals->g.PlayerRoles.rgwName[gpGlobals->rgParty[sSelectedPlayer].wPlayerRole]),
               PAL_XY(125, 16 + 20 * sSelectedPlayer), bSelectedColor, FALSE, TRUE, FALSE);
         }

         PAL_ProcessEvent();

         {
            INT tapX = 0, tapY = 0;
            if (PAL_TouchPollTap(&tapX, &tapY))
            {
               BOOL fHandled = FALSE;
               for (int p = 0; p <= gpGlobals->wMaxPartyMemberIndex; p++)
               {
                  int py = 16 + 20 * p;
                  if (tapX >= 112 && tapX <= 198 && tapY >= py - 3 && tapY <= py + 17)
                  {
                     if (p == sSelectedPlayer && fRolePrimed)
                     {
                        // 2nd tap on highlighted player -> Confirm item use!
                        g_InputState.dwKeyPress |= kKeySearch;
                     }
                     else
                     {
                        // 1st tap -> Highlight player & preview stats!
                        sSelectedPlayer = (SHORT)p;
                        fRolePrimed = TRUE;
                        fHandled = TRUE;
                     }
                     break;
                  }
               }
               if (!fHandled && g_InputState.dwKeyPress == 0)
               {
                  if (fRolePrimed && tapX >= 114 && tapX <= 188 && tapY >= 76 && tapY <= 162)
                  {
                     g_InputState.dwKeyPress |= kKeySearch;
                  }
                  else if (tapX < 105 || tapY > 184)
                  {
                     g_InputState.dwKeyPress |= kKeyMenu;
                  }
               }
               if (fHandled)
               {
                  break;
               }
            }
         }

         if (g_InputState.dwKeyPress != 0)
         {
            break;
         }

         SDL_Delay(5);
      }

      if (i <= 0)
      {
         PAL_TouchSetMode(prevTouchMode);
         return MENUITEM_VALUE_CANCELLED;
      }

      if (g_InputState.dwKeyPress & (kKeyUp | kKeyLeft))
      {
         sSelectedPlayer--;
         if (sSelectedPlayer < 0)
         {
            sSelectedPlayer = gpGlobals->wMaxPartyMemberIndex;
         }
      }
      else if (g_InputState.dwKeyPress & (kKeyDown | kKeyRight))
      {
         sSelectedPlayer++;
         if (sSelectedPlayer > gpGlobals->wMaxPartyMemberIndex)
         {
            sSelectedPlayer = 0;
         }
      }
      else if (g_InputState.dwKeyPress & kKeyMenu)
      {
         break;
      }
      else if (g_InputState.dwKeyPress & kKeySearch)
      {
         PAL_TouchSetMode(prevTouchMode);
         return gpGlobals->rgParty[sSelectedPlayer].wPlayerRole;
      }
   }

   PAL_TouchSetMode(prevTouchMode);
   return MENUITEM_VALUE_CANCELLED;
}

static VOID
PAL_BuyMenu_OnItemChange(
   WORD           wCurrentItem
)
/*++
  Purpose:

    Callback function which is called when player selected another item
    in the buy menu.

  Parameters:

    [IN]  wCurrentItem - current item on the menu, indicates the object ID of
                         the currently selected item.

  Return value:

    None.

--*/
{
   const SDL_Rect      rect = {20, 8, 300, 175};
   int                 i, j, n, iPlayerID, x, y;
   PAL_LARGE BYTE      bufImage[2048];

   //
   // Prepare item bakcground box pos
   //
   x = 40, y = 8;

#if defined(__IOS__)
   {
      LPCBITMAPRLE pItemBox = PAL_SpriteGetFrame(gpSpriteUI, SPRITENUM_ITEMBOX);
      PAL_DrawHextechBoxToScreen(
         x,
         y,
         pItemBox ? PAL_RLEGetWidth(pItemBox) : 70,
         pItemBox ? PAL_RLEGetHeight(pItemBox) : 60,
         __buymenu_firsttime_render ? 4 : 0,
         0
      );
   }
#else
   if( __buymenu_firsttime_render )
      PAL_RLEBlitToSurfaceWithShadow(PAL_SpriteGetFrame(gpSpriteUI, SPRITENUM_ITEMBOX), gpScreen, PAL_XY(x + 6, y + 6), TRUE);
   //
   // Draw the picture of current selected item
   //
   PAL_RLEBlitToSurface(PAL_SpriteGetFrame(gpSpriteUI, SPRITENUM_ITEMBOX), gpScreen,
      PAL_XY(x, y));
#endif

   //
   // Prepare item pos
   //
   x = 48, y = 15;

   if (PAL_MKFReadChunk(bufImage, 2048,
      gpGlobals->g.rgObject[wCurrentItem].item.wBitmap, gpGlobals->f.fpBALL) > 0)
   {
      PAL_RLEBlitToSurface(bufImage, gpScreen, PAL_XY(x, y));
   }

   //
   // See how many of this item we have in the inventory
   //
   n = 0;

   for (i = 0; i < MAX_INVENTORY; i++)
   {
      if (gpGlobals->rgInventory[i].wItem == 0)
      {
         break;
      }
      else if (gpGlobals->rgInventory[i].wItem == wCurrentItem)
      {
         n = gpGlobals->rgInventory[i].nAmount;
         break;
      }
   }

   for (i = 0; i < MAX_PLAYER_EQUIPMENTS; i++)
   {
      for (j = 0; j <= gpGlobals->wMaxPartyMemberIndex; j++)
      {
         iPlayerID = gpGlobals->rgParty[j].wPlayerRole;

         if (gpGlobals->g.PlayerRoles.rgwEquipment[i][iPlayerID] == wCurrentItem) n++;
      }
   }

   //
   // Prepare inventory quantities pos
   //
   x = 20, y = 100;

   if( __buymenu_firsttime_render )
      PAL_CreateSingleLineBoxWithShadow(PAL_XY(x, y), 5, FALSE, 6);
   else
   //
   // Draw the amount of this item in the inventory
   //
   PAL_CreateSingleLineBoxWithShadow(PAL_XY(x, y), 5, FALSE, 0);
   PAL_DrawText(PAL_GetWord(BUYMENU_LABEL_CURRENT), PAL_XY(x + 10, y + 10), 0, FALSE, FALSE, FALSE);
   PAL_DrawNumber(n, 6, PAL_XY(x + 49, y + 15), kNumColorYellow, kNumAlignRight);

   //
   // Prepare inventory quantities pos
   //
   x = 20, y = 141;

   if( __buymenu_firsttime_render )
      PAL_CreateSingleLineBoxWithShadow(PAL_XY(x, y), 5, FALSE, 6);
   else
   //
   // Draw the cash amount
   //
   PAL_CreateSingleLineBoxWithShadow(PAL_XY(x, y), 5, FALSE, 0);
   PAL_DrawText(PAL_GetWord(CASH_LABEL), PAL_XY(x + 10, y + 10), 0, FALSE, FALSE, FALSE);
   PAL_DrawNumber(gpGlobals->dwCash, 6, PAL_XY(x + 49, y + 15), kNumColorYellow, kNumAlignRight);

   VIDEO_UpdateScreen(&rect);
   
   __buymenu_firsttime_render = FALSE;
}

VOID
PAL_BuyMenu(
   WORD           wStoreNum
)
/*++
  Purpose:

    Show the buy item menu.

  Parameters:

    [IN]  wStoreNum - number of the store to buy items from.

  Return value:

    None.

--*/
{
   MENUITEM        rgMenuItem[MAX_STORE_ITEM];
   int             i, y;
   WORD            w;

   //
   // create the menu items
   //
   y = 21;

   for (i = 0; i < MAX_STORE_ITEM; i++)
   {
      if (gpGlobals->g.lprgStore[wStoreNum].rgwItems[i] == 0)
      {
         break;
      }

      rgMenuItem[i].wValue = gpGlobals->g.lprgStore[wStoreNum].rgwItems[i];
      rgMenuItem[i].wNumWord = gpGlobals->g.lprgStore[wStoreNum].rgwItems[i];
      rgMenuItem[i].fEnabled = TRUE;
      rgMenuItem[i].pos = PAL_XY(150, y);

      y += 18;
   }

   //
   // Draw the box
   //
   PAL_CreateBox(PAL_XY(122, 8), 8, 8, 1, FALSE);

   //
   // Draw the number of prices
   //
   for (y = 0; y < i; y++)
   {
      w = gpGlobals->g.rgObject[rgMenuItem[y].wValue].item.wPrice;
      PAL_DrawNumber(w, 6, PAL_XY(238, 26 + y * 18), kNumColorYellow, kNumAlignRight);
   }

   w = 0;
   __buymenu_firsttime_render = TRUE;

   while (TRUE)
   {
      w = PAL_ReadMenu(PAL_BuyMenu_OnItemChange, rgMenuItem, i, w, MENUITEM_COLOR);

      if (w == MENUITEM_VALUE_CANCELLED)
      {
         break;
      }

      if (gpGlobals->g.rgObject[w].item.wPrice <= gpGlobals->dwCash)
      {
         if (PAL_ConfirmMenu())
         {
            //
            // Player bought an item
            //
            gpGlobals->dwCash -= gpGlobals->g.rgObject[w].item.wPrice;
            PAL_AddItemToInventory(w, 1);
         }
      }

      //
      // Place the cursor to the current item on next loop
      //
      for (y = 0; y < i; y++)
      {
         if (w == rgMenuItem[y].wValue)
         {
            w = y;
            break;
         }
      }
   }
}

static VOID
PAL_SellMenu_OnItemChange(
   WORD         wCurrentItem
)
/*++
  Purpose:

    Callback function which is called when player selected another item
    in the sell item menu.

  Parameters:

    [IN]  wCurrentItem - current item on the menu, indicates the object ID of
                         the currently selected item.

  Return value:

    None.

--*/
{
   WORD x = 100, y = 150;

   //
   // Draw the cash amount
   //
   PAL_CreateSingleLineBoxWithShadow(PAL_XY(x, y), 5, FALSE, 0);
   PAL_DrawText(PAL_GetWord(CASH_LABEL), PAL_XY(x + 10, y + 10), 0, FALSE, FALSE, FALSE);
   PAL_DrawNumber(gpGlobals->dwCash, 6, PAL_XY(x + 48, y + 15), kNumColorYellow, kNumAlignRight);

   x += 124;

   //
   // Draw the price
   //
   PAL_CreateSingleLineBoxWithShadow(PAL_XY(x, y), 5, FALSE, 0);

   if (gpGlobals->g.rgObject[wCurrentItem].item.wFlags & kItemFlagSellable)
   {
      PAL_DrawText(PAL_GetWord(SELLMENU_LABEL_PRICE), PAL_XY(x + 10, y + 10), 0, FALSE, FALSE, FALSE);
      PAL_DrawNumber(gpGlobals->g.rgObject[wCurrentItem].item.wPrice / 2, 6,
         PAL_XY(x + 48, y + 15), kNumColorYellow, kNumAlignRight);
   }
}

VOID
PAL_SellMenu(
   VOID
)
/*++
  Purpose:

    Show the sell item menu.

  Parameters:

    None.

  Return value:

    None.

--*/
{
   WORD      w;

   while (TRUE)
   {
      w = PAL_ItemSelectMenu(PAL_SellMenu_OnItemChange, kItemFlagSellable);
      if (w == 0)
      {
         break;
      }

      if (PAL_ConfirmMenu())
      {
         if (PAL_AddItemToInventory(w, -1))
         {
            gpGlobals->dwCash += gpGlobals->g.rgObject[w].item.wPrice / 2;
         }
      }
   }
}

VOID
PAL_EquipItemMenu(
   WORD        wItem
)
/*++
  Purpose:

    Show the menu which allow players to equip the specified item.

  Parameters:

    [IN]  wItem - the object ID of the item.

  Return value:

    None.

--*/
{
   PAL_LARGE BYTE   bufBackground[320 * 200];
   PAL_LARGE BYTE   bufImageBox[72 * 72];
   PAL_LARGE BYTE   bufImage[2048];
   WORD             w;
   int              iCurrentPlayer, i;
   BYTE             bColor, bSelectedColor;
   DWORD            dwColorChangeTime;
   PALTOUCHMODE     prevTouchMode = PAL_TouchGetMode();

   PAL_TouchSetMode(kTouchModeMenu);
   PAL_TouchClearGestures();
   gpGlobals->wLastUnequippedItem = wItem;

   PAL_MKFDecompressChunk(bufBackground, 320 * 200, EQUIPMENU_BACKGROUND_FBPNUM,
      gpGlobals->f.fpFBP);

   if (gConfig.fUseCustomScreenLayout)
   {
      int x = PAL_X(gConfig.ScreenLayout.EquipImageBox);
      int y = PAL_Y(gConfig.ScreenLayout.EquipImageBox);
      for (i = 8; i < 72; i++)
      {
         memcpy(&bufBackground[i * 320 + 92], &bufBackground[(i + 128) * 320 + 92], 32);
         memcpy(&bufBackground[(i + 64) * 320 + 92], &bufBackground[(i + 128) * 320 + 92], 32);
      }
      for (i = 9; i < 90; i++)
      {
         memcpy(&bufBackground[i * 320 + 226], &bufBackground[(i + 104) * 320 + 226], 32);
      }
      for (i = 99; i < 113; i++)
      {
         memcpy(&bufBackground[i * 320 + 226], &bufBackground[(i + 16) * 320 + 226], 32);
      }
      for (i = 8; i < 80; i++)
      {
         memcpy(&bufImageBox[(i - 8) * 72], &bufBackground[i * 320 + 8], 72);
         memcpy(&bufBackground[i * 320 + 8], &bufBackground[(i + 72) * 320 + 8], 72);
      }
      for (i = 0; i < 72; i++)
      {
         memcpy(&bufBackground[(i + y) * 320 + x], &bufImageBox[i * 72], 72);
      }
   }

   BOOL             fEquipRolePrimed = (gpGlobals->wMaxPartyMemberIndex == 0);
   iCurrentPlayer = 0;
   bSelectedColor = MENUITEM_COLOR_SELECTED_FIRST;
   dwColorChangeTime = SDL_GetTicks() + (600 / MENUITEM_COLOR_SELECTED_TOTALNUM);

   while (TRUE)
   {
      wItem = gpGlobals->wLastUnequippedItem;

      //
      // Draw the background
      //
      PAL_FBPBlitToSurface(bufBackground, gpScreen);

      //
      // Draw the item picture
      //
      if (PAL_MKFReadChunk(bufImage, 2048,
         gpGlobals->g.rgObject[wItem].item.wBitmap, gpGlobals->f.fpBALL) > 0)
      {
         PAL_RLEBlitToSurface(bufImage, gpScreen, PAL_XY_OFFSET(gConfig.ScreenLayout.EquipImageBox, 8, 8));
      }

      if (gConfig.fUseCustomScreenLayout)
      {
         int labels1[] = { STATUS_LABEL_ATTACKPOWER, STATUS_LABEL_MAGICPOWER, STATUS_LABEL_RESISTANCE, STATUS_LABEL_DEXTERITY, STATUS_LABEL_FLEERATE };
         int labels2[] = { EQUIP_LABEL_HEAD, EQUIP_LABEL_SHOULDER, EQUIP_LABEL_BODY, EQUIP_LABEL_HAND, EQUIP_LABEL_FOOT, EQUIP_LABEL_NECK };
		 for (i = 0; i < sizeof(labels1) / sizeof(int); i++)
         {
            int index = &gConfig.ScreenLayout.EquipStatusLabels[i] - gConfig.ScreenLayoutArray;
            BOOL fShadow = (gConfig.ScreenLayoutFlag[index] & DISABLE_SHADOW) ? FALSE : TRUE;
            BOOL fUse8x8Font = (gConfig.ScreenLayoutFlag[index] & USE_8x8_FONT) ? TRUE : FALSE;
            PAL_DrawText(PAL_GetWord(labels1[i]), gConfig.ScreenLayoutArray[index], MENUITEM_COLOR, fShadow, FALSE, fUse8x8Font);
         }
		 for (i = 0; i < sizeof(labels2) / sizeof(int); i++)
         {
            int index = &gConfig.ScreenLayout.EquipLabels[i] - gConfig.ScreenLayoutArray;
            BOOL fShadow = (gConfig.ScreenLayoutFlag[index] & DISABLE_SHADOW) ? FALSE : TRUE;
            BOOL fUse8x8Font = (gConfig.ScreenLayoutFlag[index] & USE_8x8_FONT) ? TRUE : FALSE;
            PAL_DrawText(PAL_GetWord(labels2[i]), gConfig.ScreenLayoutArray[index], MENUITEM_COLOR, fShadow, FALSE, fUse8x8Font);
         }
      }

      //
      // Draw the current equipment of the selected player
      //
      w = gpGlobals->rgParty[iCurrentPlayer].wPlayerRole;
      for (i = 0; i < MAX_PLAYER_EQUIPMENTS; i++)
      {
         if (gpGlobals->g.PlayerRoles.rgwEquipment[i][w] != 0)
         {
            PAL_DrawText(PAL_GetWord(gpGlobals->g.PlayerRoles.rgwEquipment[i][w]),
				gConfig.ScreenLayout.EquipNames[i], MENUITEM_COLOR, TRUE, FALSE, FALSE);
         }
      }

      //
      // Draw the stats of the currently selected player
      //
      PAL_DrawNumber(PAL_GetPlayerAttackStrength(w), 4, gConfig.ScreenLayout.EquipStatusValues[0], kNumColorCyan, kNumAlignRight);
      PAL_DrawNumber(PAL_GetPlayerMagicStrength(w), 4, gConfig.ScreenLayout.EquipStatusValues[1], kNumColorCyan, kNumAlignRight);
      PAL_DrawNumber(PAL_GetPlayerDefense(w), 4, gConfig.ScreenLayout.EquipStatusValues[2], kNumColorCyan, kNumAlignRight);
      PAL_DrawNumber(PAL_GetPlayerDexterity(w), 4, gConfig.ScreenLayout.EquipStatusValues[3], kNumColorCyan, kNumAlignRight);
      PAL_DrawNumber(PAL_GetPlayerFleeRate(w), 4, gConfig.ScreenLayout.EquipStatusValues[4], kNumColorCyan, kNumAlignRight);

      //
      // Draw a box for player selection
      //
      PAL_CreateBox(gConfig.ScreenLayout.EquipRoleListBox, gpGlobals->wMaxPartyMemberIndex, PAL_WordMaxWidth(36, 4) - 1, 0, FALSE);

      //
      // Draw the label of players
      //
      for (i = 0; i <= gpGlobals->wMaxPartyMemberIndex; i++)
      {
         w = gpGlobals->rgParty[i].wPlayerRole;

         if (iCurrentPlayer == i)
         {
            if (gpGlobals->g.rgObject[wItem].item.wFlags & (kItemFlagEquipableByPlayerRole_First << w))
            {
               bColor = bSelectedColor;
            }
            else
            {
               bColor = MENUITEM_COLOR_SELECTED_INACTIVE;
            }
         }
         else
         {
            if (gpGlobals->g.rgObject[wItem].item.wFlags & (kItemFlagEquipableByPlayerRole_First << w))
            {
               bColor = MENUITEM_COLOR;
            }
            else
            {
               bColor = MENUITEM_COLOR_INACTIVE;
            }
         }

         PAL_DrawText(PAL_GetWord(gpGlobals->g.PlayerRoles.rgwName[w]),
            PAL_XY_OFFSET(gConfig.ScreenLayout.EquipRoleListBox, 13, 13 + 18 * i), bColor, TRUE, FALSE, FALSE);

         if (i == iCurrentPlayer && fEquipRolePrimed)
         {
            LPCBITMAPRLE lpEqCur = PAL_SpriteGetFrame(gpSpriteUI, SPRITENUM_CURSOR_YELLOW);
            if (lpEqCur != NULL)
            {
               PAL_RLEBlitToSurface(lpEqCur, gpScreen,
                  PAL_XY_OFFSET(gConfig.ScreenLayout.EquipRoleListBox, 68, 18 + 18 * i));
            }
         }
      }

      //
      // Draw the text label and amount of the item
      //
      if (wItem != 0)
      {
         PAL_DrawText(PAL_GetWord(wItem), gConfig.ScreenLayout.EquipItemName, MENUITEM_COLOR_CONFIRMED, TRUE, FALSE, FALSE);
         PAL_DrawNumber(PAL_GetItemAmount(wItem), 2, gConfig.ScreenLayout.EquipItemAmount, kNumColorCyan, kNumAlignRight);
      }

      //
      // Update the screen
      //
      VIDEO_UpdateScreen(NULL);

      //
      // Accept input
      //
      PAL_ClearKeyState();

      while (TRUE)
      {
         PAL_ProcessEvent();

         //
         // See if we should change the highlight color
         //
         if (SDL_TICKS_PASSED(SDL_GetTicks(), dwColorChangeTime))
         {
            if ((WORD)bSelectedColor + 1 >=
               (WORD)MENUITEM_COLOR_SELECTED_FIRST + MENUITEM_COLOR_SELECTED_TOTALNUM)
            {
               bSelectedColor = MENUITEM_COLOR_SELECTED_FIRST;
            }
            else
            {
               bSelectedColor++;
            }

            dwColorChangeTime = SDL_GetTicks() + (600 / MENUITEM_COLOR_SELECTED_TOTALNUM);

            //
            // Redraw the selected item if needed.
            //
            w = gpGlobals->rgParty[iCurrentPlayer].wPlayerRole;

            if (gpGlobals->g.rgObject[wItem].item.wFlags & (kItemFlagEquipableByPlayerRole_First << w))
            {
               PAL_DrawText(PAL_GetWord(gpGlobals->g.PlayerRoles.rgwName[w]),
                  PAL_XY_OFFSET(gConfig.ScreenLayout.EquipRoleListBox, 13, 13 + 18 * iCurrentPlayer), bSelectedColor, TRUE, TRUE, FALSE);
            }
         }

         {
            INT tapX = 0, tapY = 0;
            INT swipe = PAL_TouchPollHorizSwipe();
            if (swipe != 0 && gpGlobals->wMaxPartyMemberIndex > 0)
            {
               if (swipe < 0)
                  iCurrentPlayer = (iCurrentPlayer + 1) % (gpGlobals->wMaxPartyMemberIndex + 1);
               else
                  iCurrentPlayer = (iCurrentPlayer > 0) ? (iCurrentPlayer - 1) : gpGlobals->wMaxPartyMemberIndex;
               break;
            }
            if (PAL_TouchPollTap(&tapX, &tapY))
            {
               int rbx = PAL_X(gConfig.ScreenLayout.EquipRoleListBox);
               int rby = PAL_Y(gConfig.ScreenLayout.EquipRoleListBox);
               BOOL fRoleSwitched = FALSE;
               for (int p = 0; p <= gpGlobals->wMaxPartyMemberIndex; p++)
               {
                  int py = rby + 13 + 18 * p;
                  if (tapX >= rbx && tapX <= rbx + 90 && tapY >= py - 4 && tapY <= py + 16)
                  {
                     if (p == iCurrentPlayer && fEquipRolePrimed)
                     {
                        g_InputState.dwKeyPress |= kKeySearch;
                     }
                     else
                     {
                        iCurrentPlayer = p;
                        fEquipRolePrimed = TRUE;
                        fRoleSwitched = TRUE;
                     }
                     break;
                  }
               }
               if (!fRoleSwitched && g_InputState.dwKeyPress == 0)
               {
                  if (fEquipRolePrimed && tapX >= 4 && tapX <= 86 && tapY >= 4 && tapY <= 88)
                  {
                     g_InputState.dwKeyPress |= kKeySearch;
                  }
               }
               if (fRoleSwitched)
               {
                  break;
               }
            }
         }

         if (g_InputState.dwKeyPress != 0)
         {
            break;
         }

         SDL_Delay(5);
      }

      if (wItem == 0)
      {
         PAL_TouchSetMode(prevTouchMode);
         return;
      }

      if (g_InputState.dwKeyPress & (kKeyUp | kKeyLeft))
      {
         iCurrentPlayer--;
         if (iCurrentPlayer < 0)
         {
            iCurrentPlayer = gpGlobals->wMaxPartyMemberIndex;
         }
      }
      else if (g_InputState.dwKeyPress & (kKeyDown | kKeyRight))
      {
         iCurrentPlayer++;
         if (iCurrentPlayer > gpGlobals->wMaxPartyMemberIndex)
         {
            iCurrentPlayer = 0;
         }
      }
      else if (g_InputState.dwKeyPress & kKeyMenu)
      {
         PAL_TouchSetMode(prevTouchMode);
         return;
      }
      else if (g_InputState.dwKeyPress & kKeySearch)
      {
         w = gpGlobals->rgParty[iCurrentPlayer].wPlayerRole;

         if (gpGlobals->g.rgObject[wItem].item.wFlags & (kItemFlagEquipableByPlayerRole_First << w))
         {
            //
            // Run the equip script
            //
            gpGlobals->g.rgObject[wItem].item.wScriptOnEquip =
               PAL_RunTriggerScript(gpGlobals->g.rgObject[wItem].item.wScriptOnEquip,
                  gpGlobals->rgParty[iCurrentPlayer].wPlayerRole);
         }
      }
   }
}

VOID
PAL_QuitGame(
   VOID
)
{
#if PAL_HAS_CONFIG_PAGE
	WORD wReturnValue = PAL_TripleMenu(SYSMENU_LABEL_LAUNCHSETTING);
#else
	WORD wReturnValue = PAL_ConfirmMenu(); // No config menu available
#endif
	if (wReturnValue == 1 || wReturnValue == 2)
	{
		if (wReturnValue == 2) gConfig.fLaunchSetting = TRUE;
		PAL_SaveConfig();		// Keep the fullscreen state
		AUDIO_PlayMusic(0, FALSE, 2);
		PAL_FadeOut(2);
		PAL_Shutdown(0);
	}
}
