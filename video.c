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
#include <float.h>
#include "main.h"

// Screen buffer
SDL_Surface              *gpScreen           = NULL;

// Backup screen buffer
SDL_Surface              *gpScreenBak        = NULL;

// The global palette
static SDL_Palette       *gpPalette          = NULL;

#if SDL_VERSION_ATLEAST(2,0,0)
SDL_Window               *gpWindow           = NULL;
SDL_Renderer      *gpRenderer         = NULL;
SDL_Texture       *gpTexture          = NULL;
SDL_Texture       *gpTouchOverlay     = NULL;
SDL_Rect           gOverlayRect;
SDL_Rect           gTextureRect;

static struct RenderBackend {
    void (*Init)();
    void (*Setup)();
    SDL_Texture *(*CreateTexture)(int width, int height);
    void (*RenderCopy)();
} gRenderBackend;
#else
#undef PAL_HAS_GLSL
#endif

// The real screen surface
SDL_Surface       *gpScreenReal       = NULL;

volatile BOOL g_bRenderPaused = FALSE;

// Menus also set g_bRenderPaused while they draw, so that alone does not mean this.
volatile BOOL g_bAppInBackground = FALSE;

static BOOL bScaleScreen = PAL_SCALE_SCREEN;

// Shake times and level
static WORD               g_wShakeTime       = 0;
static WORD               g_wShakeLevel      = 0;

#if PAL_HAS_GLSL
#include "video_glsl.h"
#endif

#if PAL_HAS_SDL_GPU
static BOOL gfUseGPU = FALSE;
#else
#define gfUseGPU FALSE
#endif

#if SDL_VERSION_ATLEAST(3,0,0)
SDL_ScaleMode VIDEO_GetScaleMode() {
    if (gConfig.pszScaleQuality == NULL)
        return SDL_SCALEMODE_NEAREST;
    return SDL_atoi(gConfig.pszScaleQuality) ? SDL_SCALEMODE_LINEAR : SDL_SCALEMODE_NEAREST;
}
#endif

#if SDL_VERSION_ATLEAST(2, 0, 0)
void VIDEO_SetupTouchArea(int window_w, int window_h, int draw_w, int draw_h)
{
	gOverlayRect.x = (window_w - draw_w) / 2;
	gOverlayRect.y = (window_h - draw_h) / 2;
	gOverlayRect.w = draw_w;
	gOverlayRect.h = draw_h;
#if PAL_HAS_TOUCH
	PAL_SetTouchBounds(window_w, window_h, gOverlayRect);
#endif
}

#if !SDL_VERSION_ATLEAST(3, 0, 0)
#define SDL_SoftStretch SDL_UpperBlit
#endif
static SDL_Texture *VIDEO_CreateTexture(int width, int height)
{
    SDL_Texture* texture = NULL;
	int texture_width, texture_height;
	double ratio = (double)width / (double)height;
	ratio *= 1.6f * (double)gConfig.dwTextureHeight / (float)gConfig.dwTextureWidth;
	//
	// Check whether to keep the aspect ratio
	//
	if (gConfig.fKeepAspectRatio && fabs(ratio - 1.6f) > FLT_EPSILON)
	{
		if (ratio > 1.6f)
		{
			texture_height = 200;
			texture_width = (int)(200 * ratio) & ~0x3;
			ratio = (float)height / 200.0f;
		}
		else
		{
			texture_width = 320;
			texture_height = (int)(320 / ratio) & ~0x3;
			ratio = (float)width / 320.0f;
		}

		WORD w = (WORD)(ratio * 320.0f) & ~0x3;
		WORD h = (WORD)(ratio * 200.0f) & ~0x3;
		gTextureRect.x = (texture_width - 320) / 2;
		gTextureRect.y = (texture_height - 200) / 2;
		gTextureRect.w = 320; gTextureRect.h = 200;
		
		VIDEO_SetupTouchArea(width,height,w,h);
	}
	else
	{
		texture_width = 320;
		texture_height = 200;
		gTextureRect.x = gTextureRect.y = 0;
		gTextureRect.w = 320; gTextureRect.h = 200;
		
		VIDEO_SetupTouchArea(width,height,width,height);
	}

	//
	// Create texture for screen.
	//
	texture = SDL_CreateTexture(gpRenderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, texture_width, texture_height);
#if SDL_VERSION_ATLEAST(3,0,0)
    if (texture) SDL_SetTextureScaleMode(texture, VIDEO_GetScaleMode());
#endif
    return texture;
}
#endif

void NullFunc() {}

//
// Everything that reaches the player's screen goes through these two functions. The
// SDL_GPU backend keeps palette indices where gpScreenReal would hold RGB pixels.
//
static VOID
VIDEO_BlitToScreenReal(
   SDL_Surface    *lpSrc,
   SDL_Rect       *lpSrcRect,
   SDL_Rect       *lpDstRect
)
{
#if PAL_HAS_SDL_GPU
   if (gfUseGPU)
   {
      VIDEO_GPU_WriteScreen(lpSrc, lpSrcRect, lpDstRect);
      return;
   }
#endif
   SDL_SoftStretch(lpSrc, lpSrcRect, gpScreenReal, lpDstRect);
}

static VOID
VIDEO_FillScreenRealBlack(
   SDL_Rect       *lpRect
)
{
#if PAL_HAS_SDL_GPU
   if (gfUseGPU)
   {
      VIDEO_GPU_SetBlackBand(lpRect);
      return;
   }
#endif
   SDL_FillRect(gpScreenReal, lpRect, 0);
}

#if PAL_HAS_SDL_GPU
static VOID
VIDEO_GPU_SetupTouchArea(
   int             iWidth,
   int             iHeight
)
{
   SDL_Rect viewport = VIDEO_GPU_CalcViewport(iWidth, iHeight);
   VIDEO_SetupTouchArea(iWidth, iHeight, viewport.w, viewport.h);
}
#endif

INT
VIDEO_Startup(
   VOID
)
/*++
  Purpose:

    Initialze the video subsystem.

  Parameters:

    None.

  Return value:

    0 = success, -1 = fail to create the screen surface,
    -2 = fail to create screen buffer.

--*/
{
	extern SDL_Surface* STBIMG_Load(const char* file);
	extern char *dirname(char *path);
#if APPIMAGE
	SDL_Surface *surf = STBIMG_Load( PAL_va(0, "%s%s", dirname(dirname(dirname(gExecutablePath))), "/usr/share/icons/hicolor/256x256/apps/sdlpal.png" ) );
#endif

#if SDL_VERSION_ATLEAST(2,0,0)
   int render_w, render_h;

   gRenderBackend.Init = NullFunc;
   gRenderBackend.Setup = NullFunc;
   gRenderBackend.CreateTexture = VIDEO_CreateTexture;
   gRenderBackend.RenderCopy = VIDEO_RenderCopy;

#if PAL_HAS_GLSL
   if( gConfig.fEnableGLSL) {
	   gRenderBackend.Init = VIDEO_GLSL_Init;
	   gRenderBackend.Setup = VIDEO_GLSL_Setup;
	   gRenderBackend.CreateTexture = VIDEO_GLSL_CreateTexture;
	   gRenderBackend.RenderCopy = VIDEO_GLSL_RenderCopy;
   }
#endif
	
   gRenderBackend.Init();

#if PAL_HAS_SDL_GPU
   //
   // The GLSL backend is built on OpenGL, so it keeps using the SDL renderer. So does
   // any system where SDL_GPU cannot start.
   //
   gfUseGPU = gConfig.fEnableGPU && !gConfig.fEnableGLSL && bScaleScreen && VIDEO_GPU_Init();
#endif

   //
   // Before we can render anything, we need a window and a renderer.
   //
   if (gpWindow == NULL)
#if SDL_VERSION_ATLEAST(3,0,0)
       gpWindow = SDL_CreateWindow("Pal", gConfig.dwScreenWidth, gConfig.dwScreenHeight, PAL_VIDEO_INIT_FLAGS);
   //TODO: Figure Whether Borderless Support Needed?
#else
       gpWindow = SDL_CreateWindow("Pal", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                               gConfig.dwScreenWidth, gConfig.dwScreenHeight, PAL_VIDEO_INIT_FLAGS);
#endif

   if (gpWindow == NULL)
   {
      return -1;
   }

# if APPIMAGE
	if(surf){
		SDL_SetWindowIcon(gpWindow, surf);
	}
# endif

#if PAL_HAS_SDL_GPU
   if (gfUseGPU && VIDEO_GPU_ClaimWindow(gpWindow))
   {
      gpScreen = VIDEO_GPU_CreateSurface(320, 200);
      gpScreenBak = VIDEO_GPU_CreateSurface(320, 200);

      if (gpScreen != NULL && gpScreenBak != NULL)
      {
         gRenderBackend.RenderCopy = VIDEO_GPU_Present;
      }
      else
      {
         //
         // Without these there is nothing to draw on, so the SDL renderer takes over.
         //
         VIDEO_GPU_Shutdown();
         SDL_FreeSurface(gpScreen);
         SDL_FreeSurface(gpScreenBak);
         gpScreen = gpScreenBak = NULL;
         gfUseGPU = FALSE;
      }
   }
   else
   {
      gfUseGPU = FALSE;
   }
#endif

   if (!gfUseGPU)
   {
#if SDL_VERSION_ATLEAST(3,0,0)
      gpRenderer = SDL_CreateRenderer(gpWindow, NULL);
      UTIL_LogOutput(LOGLEVEL_DEBUG, "renderer backend:%s\n", SDL_GetRendererName(gpRenderer));
      UTIL_LogOutput(LOGLEVEL_DEBUG, "Trying to enable VRR\n");
      if (SDL_SetRenderVSync(gpRenderer, SDL_RENDERER_VSYNC_ADAPTIVE) != SDL_OK) {
         UTIL_LogOutput(LOGLEVEL_DEBUG, "Failed, fallback to vsync\n");
         SDL_SetRenderVSync(gpRenderer, 1);
      }
      else {
         UTIL_LogOutput(LOGLEVEL_DEBUG, "Got VRR works\n");
      }
#else
      gpRenderer = SDL_CreateRenderer(gpWindow, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
#endif

#if SDL_VERSION_ATLEAST(3,3,0)
      SDL_SetDefaultTextureScaleMode(gpRenderer, VIDEO_GetScaleMode());
#endif

      gRenderBackend.Setup();

      if (gpRenderer == NULL)
      {
         return -1;
      }
   }

#if defined (__IOS__)
   SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0);
   SDL_GL_SetAttribute(SDL_GL_RETAINED_BACKING, 1);
#endif

   //
   // Create the screen buffer and the backup screen buffer.
   //
#if PAL_HAS_SDL_GPU
   if (gfUseGPU)
   {
      //
      // The screen is on the GPU, and this one only says what size it is.
      //
      gpScreenReal = SDL_CreateRGBSurfaceFrom(NULL, 320, 200, 32, 320 * 4,
                                              0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000);
   }
   else
#endif
   {
      gpScreen = SDL_CreateRGBSurface(SDL_SWSURFACE, 320, 200, 8, 0, 0, 0, 0);
      gpScreenBak = SDL_CreateRGBSurface(SDL_SWSURFACE, 320, 200, 8, 0, 0, 0, 0);
      gpScreenReal = SDL_CreateRGBSurface(SDL_SWSURFACE, 320, 200, 32,
                                          0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000);
   }

   //
   // Create texture for screen.
   //
#if PAL_HAS_SDL_GPU
   if (gfUseGPU)
   {
      SDL_GetWindowSizeInPixels(gpWindow, &render_w, &render_h);
      VIDEO_GPU_SetupTouchArea(render_w, render_h);
   }
   else
#endif
   {
      SDL_GetRendererOutputSize(gpRenderer, &render_w, &render_h);
#if SDL_MAJOR_VERSION<3
      if(!gConfig.fEnableGLSL)
         SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, gConfig.pszScaleQuality);
#endif
      gpTexture = gRenderBackend.CreateTexture(render_w, render_h);
#if SDL_MAJOR_VERSION<3
      SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
#endif
   }

   //
   // Create palette object
   //
   gpPalette = SDL_AllocPalette(256);

   //
   // Failed?
   //
   if (gpScreen == NULL || gpScreenBak == NULL || gpScreenReal == NULL || (!gfUseGPU && gpTexture == NULL) || gpPalette == NULL)
   {
      VIDEO_Shutdown();
      return -2;
   }

   //
   // Create texture for overlay.
   //
   if (gConfig.fUseTouchOverlay)
   {
      extern const unsigned char bmpData[];
      extern unsigned int bmpLen;

      void *bmp = UTIL_malloc(bmpLen);
      YJ1_Decompress(bmpData, bmp, bmpLen);
      SDL_Surface *overlay = SDL_LoadBMP_RW(SDL_RWFromConstMem(bmp, bmpLen), 1);
      free(bmp);

      if (overlay != NULL)
      {
#if SDL_VERSION_ATLEAST(3,0,0)
         SDL_SetSurfaceColorKey(overlay, true, SDL_MapRGB(SDL_GetPixelFormatDetails(overlay->format), gpPalette, 255, 0, 255));
#else
         SDL_SetColorKey(overlay, SDL_RLEACCEL, SDL_MapRGB(overlay->format, 255, 0, 255));
#endif
#if PAL_HAS_SDL_GPU
         if (gfUseGPU)
         {
            VIDEO_GPU_SetOverlay(overlay, TOUCHOVERLAY_ALPHAMOD);
         }
         else
#endif
         {
#if SDL_MAJOR_VERSION<3
            SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, gConfig.pszScaleQuality);
#endif
            gpTouchOverlay = SDL_CreateTextureFromSurface(gpRenderer, overlay);
#if SDL_VERSION_ATLEAST(3,0,0) && SDL_MINOR_VERSION < 3
            SDL_SetTextureScaleMode(gpTouchOverlay, VIDEO_GetScaleMode());
#endif
#if SDL_MAJOR_VERSION<3
            SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
#endif
            SDL_SetTextureAlphaMod(gpTouchOverlay, TOUCHOVERLAY_ALPHAMOD );
         }
         SDL_FreeSurface(overlay);
      }
   }
# if PAL_HAS_GLSL
	// notice: power of 2
#  define PIXELS 1
	// We need a total empty texture in case of not using touch overlay.
	// Or GL runtime will pick the previous texture - the main screen itself
	// and reuse it - that makes color seems overexposed
	else if( gConfig.fEnableGLSL )
	{
		BYTE pixels[4*PIXELS*PIXELS];
		memset(pixels, 0, sizeof(pixels));
		SDL_Surface *temp = SDL_CreateRGBSurfaceFrom(pixels, PIXELS, PIXELS, 32, 4*PIXELS, 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000);
		gpTouchOverlay = SDL_CreateTextureFromSurface(gpRenderer, temp);
		SDL_FreeSurface(temp);
	}
# endif
#else

# if APPIMAGE
	if(surf){
		SDL_WM_SetIcon(surf, NULL);
	}
# endif

   //
   // Create the screen surface.
   //
   gpScreenReal = SDL_SetVideoMode(gConfig.dwScreenWidth, gConfig.dwScreenHeight, 8, PAL_VIDEO_INIT_FLAGS);

   if (gpScreenReal == NULL)
   {
      //
      // Fall back to 640x480 software mode.
      //
      gpScreenReal = SDL_SetVideoMode(640, 480, 8,
         SDL_SWSURFACE | (gConfig.fFullScreen ? SDL_FULLSCREEN : 0));
   }

   //
   // Still fail?
   //
   if (gpScreenReal == NULL)
   {
      return -1;
   }

   gpPalette = gpScreenReal->format->palette;

   //
   // Create the screen buffer and the backup screen buffer.
   //
   gpScreen = SDL_CreateRGBSurface(gpScreenReal->flags & ~SDL_HWSURFACE, 320, 200, 8,
      gpScreenReal->format->Rmask, gpScreenReal->format->Gmask,
      gpScreenReal->format->Bmask, gpScreenReal->format->Amask);

   gpScreenBak = SDL_CreateRGBSurface(gpScreenReal->flags & ~SDL_HWSURFACE, 320, 200, 8,
      gpScreenReal->format->Rmask, gpScreenReal->format->Gmask,
      gpScreenReal->format->Bmask, gpScreenReal->format->Amask);

   //
   // Failed?
   //
   if (gpScreen == NULL || gpScreenBak == NULL)
   {
      VIDEO_Shutdown();
      return -2;
   }

   if (gConfig.fFullScreen)
   {
      SDL_ShowCursor(FALSE);
   }

#endif

#if APPIMAGE
	if(surf)
		SDL_FreeSurface(surf);
#endif

   return 0;
}

#if defined(__IOS__) && PAL_HAS_TOUCH && SDL_VERSION_ATLEAST(2,0,0)

#define MAX_MASKED_SURFACES 32
typedef struct tagPALSURFMASK
{
   SDL_Surface *surf;
   uint8_t     *mask;
} PALSURFMASK;

static PALSURFMASK g_rgSurfMasks[MAX_MASKED_SURFACES] = {{NULL, NULL}};

uint8_t *
VIDEO_GetSurfaceUiMask(
   SDL_Surface *lpSurf,
   BOOL         fCreate
)
{
   if (lpSurf == NULL)
   {
      return NULL;
   }
   int freeIdx = -1;
   for (int i = 0; i < MAX_MASKED_SURFACES; i++)
   {
      if (g_rgSurfMasks[i].surf == lpSurf)
      {
         return g_rgSurfMasks[i].mask;
      }
      if (freeIdx < 0 && g_rgSurfMasks[i].surf == NULL)
      {
         freeIdx = i;
      }
   }
   if (!fCreate || freeIdx < 0 || lpSurf->w <= 0 || lpSurf->h <= 0 || lpSurf->w > 320 || lpSurf->h > 200)
   {
      return NULL;
   }
   uint8_t *buf = (uint8_t *)calloc((size_t)lpSurf->w * (size_t)lpSurf->h, 1);
   if (buf != NULL)
   {
      g_rgSurfMasks[freeIdx].surf = lpSurf;
      g_rgSurfMasks[freeIdx].mask = buf;
   }
   return buf;
}

static VOID
VIDEO_FreeSurfaceUiMask(
   SDL_Surface *lpSurf
)
{
   if (lpSurf == NULL) return;
   for (int i = 0; i < MAX_MASKED_SURFACES; i++)
   {
      if (g_rgSurfMasks[i].surf == lpSurf)
      {
         free(g_rgSurfMasks[i].mask);
         g_rgSurfMasks[i].mask = NULL;
         g_rgSurfMasks[i].surf = NULL;
         return;
      }
   }
}

static VOID
VIDEO_FreeAllUiMasks(
   VOID
)
{
   for (int i = 0; i < MAX_MASKED_SURFACES; i++)
   {
      if (g_rgSurfMasks[i].mask != NULL)
      {
         free(g_rgSurfMasks[i].mask);
         g_rgSurfMasks[i].mask = NULL;
      }
      g_rgSurfMasks[i].surf = NULL;
   }
}

VOID
VIDEO_ClearScreenUiMask(
   VOID
)
{
   if (gpScreen != NULL)
   {
      uint8_t *m = VIDEO_GetSurfaceUiMask(gpScreen, TRUE);
      if (m != NULL)
      {
         memset(m, 0, (size_t)gpScreen->w * (size_t)gpScreen->h);
      }
   }
}

#else

uint8_t *
VIDEO_GetSurfaceUiMask(
   SDL_Surface *lpSurf,
   BOOL         fCreate
)
{
   (void)lpSurf;
   (void)fCreate;
   return NULL;
}

VOID
VIDEO_ClearScreenUiMask(
   VOID
)
{
}

#endif

#if SDL_VERSION_ATLEAST(2,0,0)

#if defined(__IOS__) && PAL_HAS_TOUCH
#define ZOOM_WORLD_MIN_X  (-64)
#define ZOOM_WORLD_MAX_X  2112
#define ZOOM_WORLD_MIN_Y  (-128)
#define ZOOM_WORLD_MAX_Y  2112
#define ZOOM_MAX_TEX_W    (ZOOM_WORLD_MAX_X - ZOOM_WORLD_MIN_X)
#define ZOOM_MAX_TEX_H    (ZOOM_WORLD_MAX_Y - ZOOM_WORLD_MIN_Y)

static SDL_Texture *g_rgHudBtnTex[kHudActionCount][2] = {{NULL}};
static SDL_Texture *g_pStopAutoTex[2] = {NULL, NULL};
static SDL_Texture *g_pSpeedTex[5] = {NULL, NULL, NULL, NULL, NULL};
static SDL_Texture *g_pZoomOutTex[10] = {NULL};
static SDL_Texture *g_pDpadTex[5] = {NULL, NULL, NULL, NULL, NULL};
static SDL_Texture *g_rgEnemyHudTex[MAX_ENEMIES_IN_TEAM] = {NULL};
static char         g_rgEnemyHudKey[MAX_ENEMIES_IN_TEAM][128] = {{0}};
static WORD         g_rgEnemyPrevObj[MAX_ENEMIES_IN_TEAM] = {0};
static int          g_rgEnemyPeakHP[MAX_ENEMIES_IN_TEAM] = {0};
static SDL_Texture *g_rgHeroCardTex[4] = {NULL};
static char         g_rgHeroCardKey[4][128] = {{0}};
static SDL_Texture *g_rgBattleSkillOrbTex[4][2][2] = {{{NULL}}};
static BOOL         g_bInCrossFade = FALSE;

#define MAX_SEARCH_TAG_CACHE 64
typedef struct tagSEARCHTAGCACHE
{
   WORD         itemID;
   int          cashAmt;
   int          category;
   SDL_Texture *tex;
} SEARCHTAGCACHE;
static SEARCHTAGCACHE g_rgSearchTagCache[MAX_SEARCH_TAG_CACHE] = {{0, 0, -1, NULL}};
static int            g_iSearchTagCacheNext = 0;

static uint8_t     *g_pZoomMapBuf = NULL;
static SDL_Texture *g_pZoomMapTexture = NULL;
static SDL_Texture *g_pUiOverlayTexture = NULL;
static SDL_Texture *g_pPerfHudTexture = NULL;
static char         g_szLastPerfHudText[160] = "";
static BOOL         g_bHudTexInitialized = FALSE;

static SDL_Texture *
VIDEO_CreateRGBATexture(
   SDL_Renderer  *renderer,
   unsigned char *pixels,
   int            w,
   int            h
)
{
   if (!pixels || !renderer) return NULL;
   SDL_Surface *surf = SDL_CreateRGBSurfaceFrom(
      pixels, w, h, 32, w * 4,
      0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000
   );
   SDL_Texture *tex = NULL;
   if (surf)
   {
      tex = SDL_CreateTextureFromSurface(renderer, surf);
      if (tex)
      {
         SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
#if SDL_VERSION_ATLEAST(3,0,0)
         SDL_SetTextureScaleMode(tex, SDL_SCALEMODE_LINEAR);
#endif
      }
      SDL_FreeSurface(surf);
   }
   free(pixels);
   return tex;
}

static void
VIDEO_EnsureTouchHUDTextures(
   SDL_Renderer *renderer
)
{
   if (g_bHudTexInitialized || !renderer) return;
   g_bHudTexInitialized = TRUE;

   static const char *labels[kHudActionCount] = {
      "",
      "状态",
      "仙术",
      "道具",
      "装备",
      "系统",
      "调查",
      "返回",
      "围攻",
      "重复",
      "防御",
      "道具",
      "逃跑",
      "跳过",
      "加速 1x",
      "缩小 1x",
      "放大",
      "探查",
      "设置"
   };

   for (int a = 1; a < kHudActionCount; a++)
   {
      int isCircle = (a == kHudActionSearch) ? 1 : 0;
      int tw = isCircle ? 256 : 248;
      int th = isCircle ? 256 : 136;
      for (int hi = 0; hi < 2; hi++)
      {
         const char *txt = labels[a];
         if (a == kHudActionSearch && hi == 1)
         {
            txt = "调查✦";
         }
         else if (a == kHudActionReveal && hi == 1)
         {
            txt = "探查✦";
         }
         unsigned char *rgba = UTIL_RenderHUDButtonRGBA(txt, tw, th, isCircle, hi);
         g_rgHudBtnTex[a][hi] = VIDEO_CreateRGBATexture(renderer, rgba, tw, th);
      }
   }

   for (int orb = 0; orb < 4; orb++)
   {
      for (int sel = 0; sel < 2; sel++)
      {
         for (int en = 0; en < 2; en++)
         {
            unsigned char *orbRgba = UTIL_RenderBattleSkillOrbRGBA(192, orb, sel, en);
            g_rgBattleSkillOrbTex[orb][sel][en] = VIDEO_CreateRGBATexture(renderer, orbRgba, 192, 192);
         }
      }
   }

   for (int hi = 0; hi < 2; hi++)
   {
      unsigned char *rgba = UTIL_RenderHUDButtonRGBA("停围攻", 260, 136, 0, 1);
      g_pStopAutoTex[hi] = VIDEO_CreateRGBATexture(renderer, rgba, 260, 136);
   }

   g_pSpeedTex[0] = VIDEO_CreateRGBATexture(renderer, UTIL_RenderHUDButtonRGBA("加速 1x", 260, 136, 0, 0), 260, 136);
   g_pSpeedTex[1] = VIDEO_CreateRGBATexture(renderer, UTIL_RenderHUDButtonRGBA("加速1.25x", 260, 136, 0, 1), 260, 136);
   g_pSpeedTex[2] = VIDEO_CreateRGBATexture(renderer, UTIL_RenderHUDButtonRGBA("加速1.5x", 260, 136, 0, 1), 260, 136);
   g_pSpeedTex[3] = VIDEO_CreateRGBATexture(renderer, UTIL_RenderHUDButtonRGBA("加速 2x", 260, 136, 0, 1), 260, 136);
   g_pSpeedTex[4] = VIDEO_CreateRGBATexture(renderer, UTIL_RenderHUDButtonRGBA("极速 3x", 260, 136, 0, 1), 260, 136);

   g_pZoomOutTex[0] = VIDEO_CreateRGBATexture(renderer, UTIL_RenderHUDButtonRGBA("缩小 1x", 260, 136, 0, 0), 260, 136);
   g_pZoomOutTex[1] = VIDEO_CreateRGBATexture(renderer, UTIL_RenderHUDButtonRGBA("缩小 2x", 260, 136, 0, 1), 260, 136);
   g_pZoomOutTex[2] = VIDEO_CreateRGBATexture(renderer, UTIL_RenderHUDButtonRGBA("缩小 3x", 260, 136, 0, 1), 260, 136);
   g_pZoomOutTex[3] = VIDEO_CreateRGBATexture(renderer, UTIL_RenderHUDButtonRGBA("缩小 4x", 260, 136, 0, 1), 260, 136);
   g_pZoomOutTex[4] = VIDEO_CreateRGBATexture(renderer, UTIL_RenderHUDButtonRGBA("缩小 6x", 260, 136, 0, 1), 260, 136);
   g_pZoomOutTex[5] = VIDEO_CreateRGBATexture(renderer, UTIL_RenderHUDButtonRGBA("缩小 8x", 260, 136, 0, 1), 260, 136);
   g_pZoomOutTex[6] = VIDEO_CreateRGBATexture(renderer, UTIL_RenderHUDButtonRGBA("缩小 12x", 260, 136, 0, 1), 260, 136);
   g_pZoomOutTex[7] = VIDEO_CreateRGBATexture(renderer, UTIL_RenderHUDButtonRGBA("缩小 16x", 260, 136, 0, 1), 260, 136);
   g_pZoomOutTex[8] = VIDEO_CreateRGBATexture(renderer, UTIL_RenderHUDButtonRGBA("缩小 24x", 260, 136, 0, 1), 260, 136);
   g_pZoomOutTex[9] = VIDEO_CreateRGBATexture(renderer, UTIL_RenderHUDButtonRGBA("缩小 32x", 260, 136, 0, 1), 260, 136);

   for (int d = 0; d < 4; d++)
   {
      g_pDpadTex[d] = VIDEO_CreateRGBATexture(renderer, UTIL_RenderHUDDpadRGBA(384, d), 384, 384);
   }
   g_pDpadTex[4] = VIDEO_CreateRGBATexture(renderer, UTIL_RenderHUDDpadRGBA(384, -1), 384, 384);

   // Sync Apple Metal Performance HUD state onto the live CAMetalLayer immediately on first frame!
   UTIL_SetPerfHUDEnabled(UTIL_IsPerfHUDEnabled());
}
#endif

VOID
VIDEO_DestroyTouchHUD(
   VOID
)
{
#if defined(__IOS__) && PAL_HAS_TOUCH
   for (int a = 0; a < kHudActionCount; a++)
   {
      for (int hi = 0; hi < 2; hi++)
      {
         if (g_rgHudBtnTex[a][hi])
         {
            SDL_DestroyTexture(g_rgHudBtnTex[a][hi]);
            g_rgHudBtnTex[a][hi] = NULL;
         }
      }
   }
   for (int hi = 0; hi < 2; hi++)
   {
      if (g_pStopAutoTex[hi])
      {
         SDL_DestroyTexture(g_pStopAutoTex[hi]);
         g_pStopAutoTex[hi] = NULL;
      }
   }
   for (int s = 0; s < 5; s++)
   {
      if (g_pSpeedTex[s])
      {
         SDL_DestroyTexture(g_pSpeedTex[s]);
         g_pSpeedTex[s] = NULL;
      }
   }
   for (int z = 0; z < 10; z++)
   {
      if (g_pZoomOutTex[z])
      {
         SDL_DestroyTexture(g_pZoomOutTex[z]);
         g_pZoomOutTex[z] = NULL;
      }
   }
   for (int d = 0; d < 5; d++)
   {
      if (g_pDpadTex[d])
      {
         SDL_DestroyTexture(g_pDpadTex[d]);
         g_pDpadTex[d] = NULL;
      }
   }
   for (int e = 0; e < MAX_ENEMIES_IN_TEAM; e++)
   {
      if (g_rgEnemyHudTex[e])
      {
         SDL_DestroyTexture(g_rgEnemyHudTex[e]);
         g_rgEnemyHudTex[e] = NULL;
      }
      g_rgEnemyHudKey[e][0] = '\0';
      g_rgEnemyPrevObj[e] = 0;
      g_rgEnemyPeakHP[e] = 0;
   }
   for (int h = 0; h < 4; h++)
   {
      if (g_rgHeroCardTex[h])
      {
         SDL_DestroyTexture(g_rgHeroCardTex[h]);
         g_rgHeroCardTex[h] = NULL;
      }
      g_rgHeroCardKey[h][0] = '\0';
   }
   for (int orb = 0; orb < 4; orb++)
   {
      for (int sel = 0; sel < 2; sel++)
      {
         for (int en = 0; en < 2; en++)
         {
            if (g_rgBattleSkillOrbTex[orb][sel][en])
            {
               SDL_DestroyTexture(g_rgBattleSkillOrbTex[orb][sel][en]);
               g_rgBattleSkillOrbTex[orb][sel][en] = NULL;
            }
         }
      }
   }
   for (int t = 0; t < MAX_SEARCH_TAG_CACHE; t++)
   {
      if (g_rgSearchTagCache[t].tex)
      {
         SDL_DestroyTexture(g_rgSearchTagCache[t].tex);
         g_rgSearchTagCache[t].tex = NULL;
      }
      g_rgSearchTagCache[t].category = -1;
   }
   g_iSearchTagCacheNext = 0;
   if (g_pZoomMapTexture)
   {
      SDL_DestroyTexture(g_pZoomMapTexture);
      g_pZoomMapTexture = NULL;
   }
   if (g_pZoomMapBuf)
   {
      free(g_pZoomMapBuf);
      g_pZoomMapBuf = NULL;
   }
   if (g_pUiOverlayTexture)
   {
      SDL_DestroyTexture(g_pUiOverlayTexture);
      g_pUiOverlayTexture = NULL;
   }
   if (g_pPerfHudTexture)
   {
      SDL_DestroyTexture(g_pPerfHudTexture);
      g_pPerfHudTexture = NULL;
   }
   g_szLastPerfHudText[0] = '\0';
   g_bHudTexInitialized = FALSE;
#endif
}

#if defined(__IOS__) && PAL_HAS_TOUCH
static SDL_Texture *
VIDEO_GetSearchTagTexture(
   SDL_Renderer *renderer,
   WORD          itemID,
   int           cashAmt,
   int           category
)
{
   for (int i = 0; i < MAX_SEARCH_TAG_CACHE; i++)
   {
      if (g_rgSearchTagCache[i].tex != NULL &&
          g_rgSearchTagCache[i].category == category &&
          g_rgSearchTagCache[i].itemID == itemID &&
          g_rgSearchTagCache[i].cashAmt == cashAmt)
      {
         return g_rgSearchTagCache[i].tex;
      }
   }

   int slot = g_iSearchTagCacheNext;
   g_iSearchTagCacheNext = (g_iSearchTagCacheNext + 1) % MAX_SEARCH_TAG_CACHE;
   if (g_rgSearchTagCache[slot].tex != NULL)
   {
      SDL_DestroyTexture(g_rgSearchTagCache[slot].tex);
      g_rgSearchTagCache[slot].tex = NULL;
   }

   LPCWSTR itemName = (category == 0 && itemID != 0) ? PAL_GetWord(itemID) : NULL;
   int tw = (category == 0) ? 236 : 164;
   int th = 68;
   unsigned char *rgba = UTIL_RenderSearchTagRGBA(tw, th, itemName, cashAmt, category);
   g_rgSearchTagCache[slot].tex = VIDEO_CreateRGBATexture(renderer, rgba, tw, th);
   g_rgSearchTagCache[slot].itemID = itemID;
   g_rgSearchTagCache[slot].cashAmt = cashAmt;
   g_rgSearchTagCache[slot].category = category;
   return g_rgSearchTagCache[slot].tex;
}

static VOID
VIDEO_ClassifySearchableScript(
   LPEVENTOBJECT pEvt,
   int          *pOutCategory,
   WORD         *pOutItemID,
   int          *pOutCash
)
{
   *pOutItemID = 0;
   *pOutCash = 0;
   *pOutCategory = (pEvt->nSpriteFrames > 0) ? 2 : 1;

   if (gpGlobals == NULL || gpGlobals->g.lprgScriptEntry == NULL)
   {
      return;
   }

   WORD pc = pEvt->wTriggerScript;
   BOOL bSawChoicePrompt = FALSE;
   for (int step = 0; step < 16 && pc < gpGlobals->g.nScriptEntry; step++, pc++)
   {
      LPSCRIPTENTRY se = &(gpGlobals->g.lprgScriptEntry[pc]);
      WORD op = se->wOperation;
      if (op == 0x0000 || op == 0x0001 || op == 0x0002 || op == 0x0003)
      {
         break;
      }
      if (op == 0x000A)
      {
         bSawChoicePrompt = TRUE;
      }
      else if (op == 0x001F)
      {
         SHORT qty = (SHORT)se->rgwOperand[1];
         if (se->rgwOperand[0] != 0 && qty >= 0)
         {
            *pOutCategory = 0;
            *pOutItemID = se->rgwOperand[0];
            return;
         }
      }
      else if (op == 0x001E && !bSawChoicePrompt)
      {
         SHORT amt = (SHORT)se->rgwOperand[0];
         if (amt > 0)
         {
            *pOutCategory = 0;
            *pOutCash = (int)amt;
            return;
         }
      }
   }
}

static VOID
VIDEO_RenderSearchableHighlights(
   SDL_Renderer           *renderer,
   const PALTOUCHHUDSTATE *pHud
)
{
   if (!renderer || !pHud || pHud->mode != kTouchModeMap || !pHud->fRevealSearchables ||
       gpGlobals == NULL || gpGlobals->wNumScene == 0 || !gpGlobals->fInMainGame ||
       gpGlobals->fInBattle || gOverlayRect.w <= 0 || gOverlayRect.h <= 0)
   {
      return;
   }

   int camX = PAL_X(gpGlobals->viewport);
   int camY = PAL_Y(gpGlobals->viewport);
   int viewW = 320;
   int viewH = 200;
   if (pHud->fMapZoom > 1.001f || pHud->fMapPanDetached)
   {
      PAL_TouchGetZoomViewport(&camX, &camY, &viewW, &viewH);
   }

   typedef struct {
      float        footGX, footGY; // in [0..320] x [0..200] viewport units
      float        tx, ty, tw, th; // tag pill rect in [0..320] x [0..200]
      int          category;       // 0 = loot, 1 = mechanism/inspect, 2 = NPC
      SDL_Texture *tex;
   } SEARCHMARKER;

   static SEARCHMARKER s_markers[128];
   int nMarkers = 0;

   float pillBaseH = (pHud->fMapZoom >= 7.5f) ? 8.2f : ((pHud->fMapZoom >= 3.5f) ? 9.2f : 10.5f);

   for (int k = gpGlobals->g.rgScene[gpGlobals->wNumScene - 1].wEventObjectIndex;
        k < gpGlobals->g.rgScene[gpGlobals->wNumScene].wEventObjectIndex && nMarkers < 128; k++)
   {
      LPEVENTOBJECT p = &(gpGlobals->g.lprgEventObject[k]);
      if (p->sState <= 0 || p->sVanishTime != 0 ||
          p->wTriggerMode < kTriggerSearchNear || p->wTriggerMode >= kTriggerTouchNear ||
          p->wTriggerScript == 0 ||
          gpGlobals->g.lprgScriptEntry[p->wTriggerScript].wOperation == 0x0000)
      {
         continue;
      }

      int wx = (SHORT)p->x;
      int wy = (SHORT)p->y;
      if (wx < camX - 48 || wx > camX + viewW + 48 ||
          wy < camY - 48 || wy > camY + viewH + 64)
      {
         continue;
      }

      int category = 1;
      WORD itemID = 0;
      int cashAmt = 0;
      VIDEO_ClassifySearchableScript(p, &category, &itemID, &cashAmt);

      float fgx = ((float)(wx - camX) / (float)viewW) * 320.0f;
      float fgy = ((float)(wy - camY) / (float)viewH) * 200.0f;

      int worldLift = (p->nSpriteFrames > 0) ? 38 : ((p->wSpriteNum > 0) ? 20 : 8);
      float headGY = ((float)(wy - worldLift - camY) / (float)viewH) * 200.0f;

      float tw = (category == 0) ? (pillBaseH * 3.35f) : (pillBaseH * 2.35f);
      float th = pillBaseH;

      s_markers[nMarkers].footGX = fgx;
      s_markers[nMarkers].footGY = fgy;
      s_markers[nMarkers].tw = tw;
      s_markers[nMarkers].th = th;
      s_markers[nMarkers].tx = fgx - tw * 0.5f;
      s_markers[nMarkers].ty = headGY - th - 2.0f;
      s_markers[nMarkers].category = category;
      s_markers[nMarkers].tex = VIDEO_GetSearchTagTexture(renderer, itemID, cashAmt, category);
      nMarkers++;
   }

   if (nMarkers <= 0)
   {
      return;
   }

   // Light pairwise de-overlapping so adjacent kitchen jars / chests stack cleanly
   for (int iter = 0; iter < 5; iter++)
   {
      for (int a = 0; a < nMarkers; a++)
      {
         for (int b = a + 1; b < nMarkers; b++)
         {
            float ox = fminf(s_markers[a].tx + s_markers[a].tw + 1.2f, s_markers[b].tx + s_markers[b].tw + 1.2f)
                     - fmaxf(s_markers[a].tx, s_markers[b].tx);
            float oy = fminf(s_markers[a].ty + s_markers[a].th + 1.0f, s_markers[b].ty + s_markers[b].th + 1.0f)
                     - fmaxf(s_markers[a].ty, s_markers[b].ty);
            if (ox > 0.0f && oy > 0.0f)
            {
               float dir = (s_markers[a].ty <= s_markers[b].ty) ? -1.0f : 1.0f;
               if (fabsf(s_markers[a].ty - s_markers[b].ty) < 0.3f)
                  dir = (a < b) ? -1.0f : 1.0f;
               s_markers[a].ty += dir * (oy * 0.52f);
               s_markers[b].ty -= dir * (oy * 0.52f);
            }
         }
      }
   }

#if SDL_VERSION_ATLEAST(3,0,0)
   SDL_SetRenderClipRect(renderer, &gOverlayRect);
   SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

   float scaleX = (float)gOverlayRect.w / 320.0f;
   float scaleY = (float)gOverlayRect.h / 200.0f;
   float pulse = 0.80f + 0.20f * sinf((float)SDL_GetTicks() * 0.008f);

   // Pass 1: Draw glowing isometric diamonds at each searchable object's world anchor + leader ticks
   for (int i = 0; i < nMarkers; i++)
   {
      float sx = gOverlayRect.x + s_markers[i].footGX * scaleX;
      float sy = gOverlayRect.y + s_markers[i].footGY * scaleY;
      float baseDW = (pHud->iMapZoom >= 8) ? 6.5f : 9.5f;
      float dw = baseDW * pulse * scaleX;
      float dh = (baseDW * 0.5f) * pulse * scaleY;

      Uint8 r = 85, g = 250, b = 175;
      if (s_markers[i].category == 0)      { r = 255; g = 216; b = 68;  }
      else if (s_markers[i].category == 2) { r = 105; g = 222; b = 255; }

      SDL_FPoint outerPts[5] = {
         { sx,             sy - dh * 1.32f },
         { sx + dw * 1.32f, sy             },
         { sx,             sy + dh * 1.32f },
         { sx - dw * 1.32f, sy             },
         { sx,             sy - dh * 1.32f }
      };
      SDL_FPoint innerPts[5] = {
         { sx,      sy - dh },
         { sx + dw, sy      },
         { sx,      sy + dh },
         { sx - dw, sy      },
         { sx,      sy - dh }
      };
      SDL_SetRenderDrawColor(renderer, r, g, b, 115);
      SDL_RenderLines(renderer, outerPts, 5);
      SDL_SetRenderDrawColor(renderer, r, g, b, 235);
      SDL_RenderLines(renderer, innerPts, 5);

      float tagMidX = gOverlayRect.x + (s_markers[i].tx + s_markers[i].tw * 0.5f) * scaleX;
      float tagBotY = gOverlayRect.y + (s_markers[i].ty + s_markers[i].th) * scaleY;
      if (sy - dh > tagBotY + 2.0f)
      {
         SDL_SetRenderDrawColor(renderer, r, g, b, 150);
         SDL_RenderLine(renderer, tagMidX, tagBotY, sx, sy - dh);
      }
   }
   SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);

   // Pass 2: Draw floating Retina tags (NPCs/props first, Treasure Gold on top!)
   for (int pass = 2; pass >= 0; pass--)
   {
      for (int i = 0; i < nMarkers; i++)
      {
         if (s_markers[i].category != pass || !s_markers[i].tex)
            continue;
         SDL_FRect dst = {
            gOverlayRect.x + s_markers[i].tx * scaleX,
            gOverlayRect.y + s_markers[i].ty * scaleY,
            s_markers[i].tw * scaleX,
            s_markers[i].th * scaleY
         };
         SDL_SetTextureAlphaMod(s_markers[i].tex, 245);
         SDL_RenderTexture(renderer, s_markers[i].tex, NULL, &dst);
      }
   }

   SDL_SetRenderClipRect(renderer, NULL);
#endif
}

static VOID
VIDEO_RenderBattleEnemyStatsHUD(
   SDL_Renderer           *renderer,
   const PALTOUCHHUDSTATE *pHud
)
{
   if (!renderer || !pHud || gpGlobals == NULL || !gpGlobals->fInBattle ||
       g_Battle.BattleResult != kBattleResultOnGoing ||
       !UTIL_IsEnemyStatsHUDEnabled() ||
       pHud->mode == kTouchModeMenu ||
       gOverlayRect.w <= 0 || gOverlayRect.h <= 0)
   {
      return;
   }

   if (g_Battle.UI.state == kBattleUISelectMove &&
       (g_Battle.UI.MenuState == kBattleMenuMagicSelect ||
        g_Battle.UI.MenuState == kBattleMenuUseItemSelect ||
        g_Battle.UI.MenuState == kBattleMenuThrowItemSelect))
   {
      return;
   }

   typedef struct {
      int   slot;
      int   isTargeted;
      float headGX, headGY;
      float cx, cy, cw, ch;
   } ENEMYCARDITEM;

   ENEMYCARDITEM items[MAX_ENEMIES_IN_TEAM];
   int nAlive = 0;
   int highlightedSlot = PAL_BattleGetHighlightedEnemy();

   int bfEffect[5] = { 0, 0, 0, 0, 0 };
   if (gpGlobals->g.lprgBattleField != NULL)
   {
      for (int e = 0; e < 5; e++)
      {
         bfEffect[e] = (int)gpGlobals->g.lprgBattleField[gpGlobals->wNumBattleField].rgsMagicEffect[e];
      }
   }

   for (int i = 0; i <= (int)g_Battle.wMaxEnemyIndex && i < MAX_ENEMIES_IN_TEAM; i++)
   {
      WORD wObj = g_Battle.rgEnemy[i].wObjectID;
      int hp = (int)(SHORT)g_Battle.rgEnemy[i].e.wHealth;
      if (wObj == 0 || hp <= 0)
      {
         continue;
      }

      int phys = (int)(SHORT)g_Battle.rgEnemy[i].e.wPhysicalResistance;
      int sorc = (int)(SHORT)gpGlobals->g.rgObject[wObj].enemy.wResistanceToSorcery;
      int pois = (int)(SHORT)g_Battle.rgEnemy[i].e.wPoisonResistance;
      int elem[5];
      for (int e = 0; e < 5; e++)
      {
         elem[e] = (int)(SHORT)g_Battle.rgEnemy[i].e.wElemResistance[e];
      }
      WORD wStealItem = g_Battle.rgEnemy[i].e.wStealItem;
      int  nStealCount = (int)(USHORT)g_Battle.rgEnemy[i].e.nStealItem;
      int  dropCash = (int)(USHORT)g_Battle.rgEnemy[i].e.wCash;
      int  level = (int)(USHORT)g_Battle.rgEnemy[i].e.wLevel;
      WORD wEnemyID = gpGlobals->g.rgObject[wObj].enemy.wEnemyID;
      int  hadInitSteal = 0;
      if (g_rgEnemyPrevObj[i] != wObj || hp > g_rgEnemyPeakHP[i])
      {
         g_rgEnemyPrevObj[i] = wObj;
         g_rgEnemyPeakHP[i] = hp;
      }
      if (gpGlobals->g.lprgEnemy != NULL && wEnemyID < gpGlobals->g.nEnemy)
      {
         int tplHP = (int)(SHORT)gpGlobals->g.lprgEnemy[wEnemyID].wHealth;
         if (tplHP > g_rgEnemyPeakHP[i])
         {
            g_rgEnemyPeakHP[i] = tplHP;
         }
         WORD initItem = gpGlobals->g.lprgEnemy[wEnemyID].wStealItem;
         WORD initCnt  = gpGlobals->g.lprgEnemy[wEnemyID].nStealItem;
         if ((initItem != 0 && initCnt > 0) || (initItem == 0 && initCnt >= 2))
         {
            hadInitSteal = 1;
         }
      }
      int maxHP = (g_rgEnemyPeakHP[i] >= hp && g_rgEnemyPeakHP[i] > 0) ? g_rgEnemyPeakHP[i] : hp;
      LPCWSTR stealItemName = (wStealItem != 0) ? PAL_GetWord(wStealItem) : NULL;
      int isTargeted = (highlightedSlot == i) ? 1 : 0;

      char key[128];
      snprintf(key, sizeof(key), "%u:%d/%d:%d:%d:%d:%d:%d,%d,%d,%d,%d:%d,%d,%d,%d,%d:%u:%d:%d:%d:%d",
               (unsigned)wObj, hp, maxHP, level, phys, sorc, pois,
               elem[0], elem[1], elem[2], elem[3], elem[4],
               bfEffect[0], bfEffect[1], bfEffect[2], bfEffect[3], bfEffect[4],
               (unsigned)wStealItem, nStealCount, hadInitSteal, dropCash,
               isTargeted);

      if (g_rgEnemyHudTex[i] == NULL || strcmp(key, g_rgEnemyHudKey[i]) != 0)
      {
         if (g_rgEnemyHudTex[i] != NULL)
         {
            SDL_DestroyTexture(g_rgEnemyHudTex[i]);
            g_rgEnemyHudTex[i] = NULL;
         }
         unsigned char *rgba = UTIL_RenderEnemyStatsCardRGBA(
            436, 158, hp, maxHP, level, phys, sorc, pois, elem, bfEffect,
            stealItemName, nStealCount, hadInitSteal, dropCash, isTargeted
         );
         g_rgEnemyHudTex[i] = VIDEO_CreateRGBATexture(renderer, rgba, 436, 158);
         strncpy(g_rgEnemyHudKey[i], key, sizeof(g_rgEnemyHudKey[i]) - 1);
         g_rgEnemyHudKey[i][sizeof(g_rgEnemyHudKey[i]) - 1] = '\0';
      }

      PAL_POS basePos = g_Battle.rgEnemy[i].posOriginal;
      LPCBITMAPRLE frame0 = (g_Battle.rgEnemy[i].lpSprite != NULL)
         ? PAL_SpriteGetFrame(g_Battle.rgEnemy[i].lpSprite, 0) : NULL;
      int sprH = (frame0 != NULL) ? PAL_RLEGetHeight(frame0) : 56;
      if (sprH < 24) sprH = 24;

      items[nAlive].slot = i;
      items[nAlive].isTargeted = isTargeted;
      items[nAlive].headGX = (float)PAL_X(basePos);
      items[nAlive].headGY = (float)PAL_Y(basePos) - (float)sprH;
      nAlive++;
   }

   if (nAlive <= 0)
   {
      return;
   }

   float cardGW = (nAlive <= 2) ? 84.0f : ((nAlive == 3) ? 76.0f : 69.0f);
   float cardGH = (nAlive <= 2) ? 28.0f : ((nAlive == 3) ? 25.5f : 23.0f);

   for (int k = 0; k < nAlive; k++)
   {
      items[k].cw = cardGW;
      items[k].ch = cardGH;
      items[k].cx = items[k].headGX - cardGW * 0.5f;
      items[k].cy = items[k].headGY - cardGH - 3.0f;
      if (items[k].cx < 2.0f) items[k].cx = 2.0f;
      if (items[k].cx + cardGW > 318.0f) items[k].cx = 318.0f - cardGW;
      if (items[k].cy < 2.5f) items[k].cy = 2.5f;
      if (items[k].cy + cardGH > 178.0f) items[k].cy = 178.0f - cardGH;
   }

   BOOL bTopTargetBoxActive = (highlightedSlot >= 0) ||
      (g_Battle.UI.state == kBattleUISelectTargetPlayer);

   // Iterative AABB relaxation so 4-5 crowded enemies NEVER have overlapping stat cards!
   for (int iter = 0; iter < 10; iter++)
   {
      if (bTopTargetBoxActive)
      {
         const float bx0 = 94.0f, by0 = 2.0f, bx1 = 230.0f, by1 = 35.0f;
         for (int k = 0; k < nAlive; k++)
         {
            float ox = fminf(items[k].cx + cardGW, bx1) - fmaxf(items[k].cx, bx0);
            float oy = fminf(items[k].cy + cardGH, by1) - fmaxf(items[k].cy, by0);
            if (ox > 0.0f && oy > 0.0f)
            {
               if (items[k].cx + cardGW * 0.5f < 115.0f && items[k].cx - ox >= 2.0f)
                  items[k].cx -= ox;
               else
                  items[k].cy = by1 + 1.5f;
            }
         }
      }

      for (int a = 0; a < nAlive; a++)
      {
         for (int b = a + 1; b < nAlive; b++)
         {
            float padX = 2.0f, padY = 1.5f;
            float ox = fminf(items[a].cx + cardGW + padX, items[b].cx + cardGW + padX) - fmaxf(items[a].cx, items[b].cx);
            float oy = fminf(items[a].cy + cardGH + padY, items[b].cy + cardGH + padY) - fmaxf(items[a].cy, items[b].cy);
            if (ox > 0.0f && oy > 0.0f)
            {
               if (oy * 1.45f <= ox)
               {
                  float dir = (items[a].cy <= items[b].cy) ? -1.0f : 1.0f;
                  if (fabsf(items[a].cy - items[b].cy) < 0.5f)
                     dir = (items[a].headGY <= items[b].headGY) ? -1.0f : 1.0f;
                  items[a].cy += dir * (oy * 0.52f);
                  items[b].cy -= dir * (oy * 0.52f);
               }
               else
               {
                  float dir = (items[a].cx <= items[b].cx) ? -1.0f : 1.0f;
                  if (fabsf(items[a].cx - items[b].cx) < 0.5f)
                     dir = (items[a].headGX <= items[b].headGX) ? -1.0f : 1.0f;
                  items[a].cx += dir * (ox * 0.52f);
                  items[b].cx -= dir * (ox * 0.52f);
               }
            }
         }
      }

      for (int k = 0; k < nAlive; k++)
      {
         if (items[k].cx < 2.0f) items[k].cx = 2.0f;
         if (items[k].cx + cardGW > 318.0f) items[k].cx = 318.0f - cardGW;
         if (items[k].cy < 3.0f) items[k].cy = 3.0f;
         if (items[k].cy + cardGH > 180.0f) items[k].cy = 180.0f - cardGH;
      }
   }

#if SDL_VERSION_ATLEAST(3,0,0)
   SDL_SetRenderClipRect(renderer, &gOverlayRect);
   SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

   float scaleX = (float)gOverlayRect.w / 320.0f;
   float scaleY = (float)gOverlayRect.h / 200.0f;

   // Pass 1: Subtle leader lines connecting each card to its monster's crown
   for (int k = 0; k < nAlive; k++)
   {
      float crownGX = items[k].headGX;
      float crownGY = items[k].headGY;
      if (crownGY < 6.0f) crownGY = 6.0f;
      if (crownGY > 194.0f) crownGY = 194.0f;

      float cardMidGX = items[k].cx + items[k].cw * 0.5f;
      float cardBotGY = items[k].cy + items[k].ch;
      if (fabsf(cardMidGX - crownGX) > 4.0f || fabsf(cardBotGY - crownGY) > 6.0f)
      {
         float x1 = gOverlayRect.x + cardMidGX * scaleX;
         float y1 = gOverlayRect.y + cardBotGY * scaleY;
         float x2 = gOverlayRect.x + crownGX * scaleX;
         float y2 = gOverlayRect.y + crownGY * scaleY;
         if (items[k].isTargeted)
            SDL_SetRenderDrawColor(renderer, 255, 224, 110, 210);
         else
            SDL_SetRenderDrawColor(renderer, 215, 188, 125, 130);
         SDL_RenderLine(renderer, x1, y1, x2, y2);
      }
   }
   SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);

   // Pass 2: Render non-targeted cards first, then targeted card on top
   for (int pass = 0; pass < 2; pass++)
   {
      for (int k = 0; k < nAlive; k++)
      {
         if ((items[k].isTargeted ? 1 : 0) != pass)
            continue;
         SDL_Texture *tex = g_rgEnemyHudTex[items[k].slot];
         if (!tex) continue;
         SDL_FRect dst = {
            gOverlayRect.x + items[k].cx * scaleX,
            gOverlayRect.y + items[k].cy * scaleY,
            items[k].cw * scaleX,
            items[k].ch * scaleY
         };
         SDL_SetTextureAlphaMod(tex, 255);
         SDL_RenderTexture(renderer, tex, NULL, &dst);
      }
   }

   SDL_SetRenderClipRect(renderer, NULL);
#endif
}

static VOID
VIDEO_RenderBattlePartyAndOrbsHUD(
   SDL_Renderer           *renderer,
   const PALTOUCHHUDSTATE *pHud
)
{
   if (!renderer || !pHud || gpGlobals == NULL || !gpGlobals->fInBattle ||
       g_Battle.BattleResult != kBattleResultOnGoing ||
       gpGlobals->fAutoBattle ||
       pHud->mode == kTouchModeMenu ||
       gOverlayRect.w <= 0 || gOverlayRect.h <= 0)
   {
      return;
   }

#ifdef PAL_CLASSIC
   if (g_Battle.Phase == kBattlePhasePerformAction)
   {
      return;
   }
#endif

   if (g_Battle.UI.state == kBattleUISelectMove &&
       (g_Battle.UI.MenuState == kBattleMenuMagicSelect ||
        g_Battle.UI.MenuState == kBattleMenuUseItemSelect ||
        g_Battle.UI.MenuState == kBattleMenuThrowItemSelect))
   {
      return;
   }

   if (g_Battle.UI.fAutoAttack)
   {
      return;
   }

#if SDL_VERSION_ATLEAST(3,0,0)
   float scaleX = (float)gOverlayRect.w / 320.0f;
   float scaleY = (float)gOverlayRect.h / 200.0f;

   SDL_SetRenderClipRect(renderer, &gOverlayRect);
   SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

   // 1. Retina MOBA Party Hero Cards (bottom-right: 1..3 party slots)
   int hiPlayer = PAL_BattleGetHighlightedPlayer();
   for (int i = 0; i <= (int)gpGlobals->wMaxPartyMemberIndex && i < 3; i++)
   {
      WORD wRole  = gpGlobals->rgParty[i].wPlayerRole;
      int  hp     = (int)(USHORT)gpGlobals->g.PlayerRoles.rgwHP[wRole];
      int  maxHP  = (int)(USHORT)gpGlobals->g.PlayerRoles.rgwMaxHP[wRole];
      int  mp     = (int)(USHORT)gpGlobals->g.PlayerRoles.rgwMP[wRole];
      int  maxMP  = (int)(USHORT)gpGlobals->g.PlayerRoles.rgwMaxMP[wRole];
      int  level  = (int)(USHORT)gpGlobals->g.PlayerRoles.rgwLevel[wRole];

      int turnState = 0;
      if (g_Battle.UI.state != kBattleUIWait)
      {
         if ((int)g_Battle.UI.wCurPlayerIndex == i)
            turnState = 1;
         else if (i < (int)g_Battle.UI.wCurPlayerIndex)
            turnState = 2;
      }

      int statusFlags = 0;
      for (int p = 0; p < MAX_POISONS; p++)
      {
         if (gpGlobals->rgPoisonStatus[p][i].wPoisonID != 0)
         {
            statusFlags |= 1;
            break;
         }
      }
      if (gpGlobals->rgPlayerStatus[wRole][kStatusSleep] != 0)
         statusFlags |= 2;
      if (gpGlobals->rgPlayerStatus[wRole][kStatusSilence] != 0)
         statusFlags |= 4;
      if (gpGlobals->rgPlayerStatus[wRole][kStatusConfused] != 0 ||
          gpGlobals->rgPlayerStatus[wRole][kStatusPuppet] != 0)
         statusFlags |= 8;
      if (gpGlobals->rgPlayerStatus[wRole][kStatusBravery] != 0 ||
          gpGlobals->rgPlayerStatus[wRole][kStatusProtect] != 0 ||
          gpGlobals->rgPlayerStatus[wRole][kStatusHaste] != 0 ||
          gpGlobals->rgPlayerStatus[wRole][kStatusDualAttack] != 0)
         statusFlags |= 16;

      int isTargeted = (hiPlayer == i || hiPlayer == -2) ? 1 : 0;

      char key[128];
      snprintf(key, sizeof(key), "%u:%d:%d/%d:%d/%d:%d:%d:%d",
               (unsigned)wRole, level, hp, maxHP, mp, maxMP, turnState, statusFlags, isTargeted);

      if (g_rgHeroCardTex[i] == NULL || strcmp(key, g_rgHeroCardKey[i]) != 0)
      {
         if (g_rgHeroCardTex[i] != NULL)
         {
            SDL_DestroyTexture(g_rgHeroCardTex[i]);
            g_rgHeroCardTex[i] = NULL;
         }

         unsigned char faceRGBA[64 * 64 * 4];
         int fw = 0, fh = 0;
         if (gpSpriteUI != NULL && gpPalette != NULL)
         {
            LPCBITMAPRLE lpFace = PAL_SpriteGetFrame(gpSpriteUI, SPRITENUM_PLAYERFACE_FIRST + wRole);
            if (lpFace != NULL)
            {
               fw = PAL_RLEGetWidth(lpFace);
               fh = PAL_RLEGetHeight(lpFace);
               if (fw > 64) fw = 64;
               if (fh > 64) fh = 64;
               if (fw > 0 && fh > 0)
               {
                  uint8_t buf0[64 * 64];
                  uint8_t buf1[64 * 64];
                  memset(buf0, 0x00, sizeof(buf0));
                  memset(buf1, 0xFF, sizeof(buf1));
                  SDL_Surface *s0 = SDL_CreateRGBSurfaceFrom(buf0, fw, fh, 8, fw, 0, 0, 0, 0);
                  SDL_Surface *s1 = SDL_CreateRGBSurfaceFrom(buf1, fw, fh, 8, fw, 0, 0, 0, 0);
                  if (s0 && s1)
                  {
                     PAL_RLEBlitToSurface(lpFace, s0, 0);
                     PAL_RLEBlitToSurface(lpFace, s1, 0);
                     const SDL_Color *cols = gpPalette->colors;
                     for (int k = 0; k < fw * fh; k++)
                     {
                        if (buf0[k] == 0x00 && buf1[k] == 0xFF)
                        {
                           faceRGBA[k * 4 + 0] = 0;
                           faceRGBA[k * 4 + 1] = 0;
                           faceRGBA[k * 4 + 2] = 0;
                           faceRGBA[k * 4 + 3] = 0;
                        }
                        else
                        {
                           uint8_t idx = buf0[k];
                           faceRGBA[k * 4 + 0] = cols[idx].r;
                           faceRGBA[k * 4 + 1] = cols[idx].g;
                           faceRGBA[k * 4 + 2] = cols[idx].b;
                           faceRGBA[k * 4 + 3] = 255;
                        }
                     }
                  }
                  if (s0) SDL_FreeSurface(s0);
                  if (s1) SDL_FreeSurface(s1);
               }
            }
         }

         unsigned char *cardRgba = UTIL_RenderHeroBattleCardRGBA(
            348, 176,
            (fw > 0 && fh > 0) ? faceRGBA : NULL, fw, fh,
            PAL_GetWord(gpGlobals->g.PlayerRoles.rgwName[wRole]),
            level, hp, maxHP, mp, maxMP,
            turnState, statusFlags, isTargeted
         );
         g_rgHeroCardTex[i] = VIDEO_CreateRGBATexture(renderer, cardRgba, 348, 176);
         strncpy(g_rgHeroCardKey[i], key, sizeof(g_rgHeroCardKey[i]) - 1);
         g_rgHeroCardKey[i][sizeof(g_rgHeroCardKey[i]) - 1] = '\0';
      }

      if (g_rgHeroCardTex[i] != NULL)
      {
         float gx = 89.0f + 77.0f * (float)i;
         float gy = (turnState == 1 && hp > 0) ? 159.0f : 160.5f;
         float gw = 75.5f;
         float gh = 38.0f;
         SDL_FRect dst = {
            gOverlayRect.x + gx * scaleX,
            gOverlayRect.y + gy * scaleY,
            gw * scaleX,
            gh * scaleY
         };
         SDL_SetTextureAlphaMod(g_rgHeroCardTex[i], 255);
         SDL_RenderTexture(renderer, g_rgHeroCardTex[i], NULL, &dst);
      }
   }

   // 2. 4 Retina MOBA Battle Skill Orbs (普攻 / 仙术 / 合击 / 战术)
   if (g_Battle.UI.state == kBattleUISelectMove || g_Battle.UI.state == kBattleUISelectTargetPlayer)
   {
      static const float orbPos[4][2] = {
         { 27.0f, 140.0f }, // Attack
         {  0.0f, 155.0f }, // Magic
         { 54.0f, 155.0f }, // Coop Magic
         { 27.0f, 170.0f }  // Misc / Tactics
      };
      for (int k = 0; k < 4; k++)
      {
         int isSel = (g_Battle.UI.state == kBattleUISelectMove && (int)g_Battle.UI.wSelectedAction == k) ? 1 : 0;
         int isEn  = (g_Battle.UI.state == kBattleUISelectTargetPlayer) ? 0 : (PAL_BattleIsActionValidForUI(k) ? 1 : 0);
         SDL_Texture *orbTex = g_rgBattleSkillOrbTex[k][isSel][isEn];
         if (!orbTex) continue;

         float sz = isSel ? 30.5f : 28.5f;
         float ox = orbPos[k][0] + 14.0f - sz * 0.5f;
         float oy = orbPos[k][1] + 14.0f - sz * 0.5f;
         SDL_FRect dst = {
            gOverlayRect.x + ox * scaleX,
            gOverlayRect.y + oy * scaleY,
            sz * scaleX,
            sz * scaleY
         };
         SDL_SetTextureAlphaMod(orbTex, 255);
         SDL_RenderTexture(renderer, orbTex, NULL, &dst);
      }
   }

   SDL_SetRenderClipRect(renderer, NULL);
#endif
}
#endif

VOID
VIDEO_RenderTouchHUD(
   SDL_Renderer *renderer,
   int           winW,
   int           winH
)
{
#if defined(__IOS__) && PAL_HAS_TOUCH
   if (!renderer || winW <= 0 || winH <= 0)
      return;

   VIDEO_EnsureTouchHUDTextures(renderer);

   PALTOUCHHUDSTATE hud;
   PAL_TouchGetHUDState(&hud);
   float aspect = (float)winW / (float)winH;

   // 0. Draw Searchable Object Highlights ([探查✦] mode: hidden items, chests, switches, NPCs)
   VIDEO_RenderSearchableHighlights(renderer, &hud);

   // 1. Draw A* maze path trail + pulsing golden target diamond on map (supports 1x..32x zoom)
   if (hud.mode == kTouchModeMap && hud.fMapTargetActive && gpGlobals != NULL)
   {
      int camX = PAL_X(gpGlobals->viewport);
      int camY = PAL_Y(gpGlobals->viewport);
      int viewW = 320;
      int viewH = 200;
      if (hud.fMapZoom > 1.001f || hud.fMapPanDetached)
      {
         PAL_TouchGetZoomViewport(&camX, &camY, &viewW, &viewH);
      }

#if SDL_VERSION_ATLEAST(3,0,0)
      SDL_SetRenderClipRect(renderer, &gOverlayRect);
      SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

      const SHORT *pWpWX = NULL;
      const SHORT *pWpWY = NULL;
      int nWp = PAL_TouchGetMapPathWaypoints(&pWpWX, &pWpWY);
      int destWX = hud.iMapTargetWorldX;
      int destWY = hud.iMapTargetWorldY;

      if (nWp > 0 && pWpWX != NULL && pWpWY != NULL)
      {
         static SDL_FPoint s_pathPts[1026];
         static SDL_FPoint s_glowPts[1026];
         if (nWp > 1024) nWp = 1024;

         int partyWX = PAL_X(gpGlobals->viewport) + PAL_X(gpGlobals->partyoffset);
         int partyWY = PAL_Y(gpGlobals->viewport) + PAL_Y(gpGlobals->partyoffset);
         s_pathPts[0].x = gOverlayRect.x + ((float)(partyWX - camX) / (float)viewW) * gOverlayRect.w;
         s_pathPts[0].y = gOverlayRect.y + ((float)(partyWY - camY) / (float)viewH) * gOverlayRect.h;
         s_glowPts[0].x = s_pathPts[0].x;
         s_glowPts[0].y = s_pathPts[0].y + 1.5f;

         for (int k = 0; k < nWp; k++)
         {
            float px = gOverlayRect.x + ((float)(pWpWX[k] - camX) / (float)viewW) * gOverlayRect.w;
            float py = gOverlayRect.y + ((float)(pWpWY[k] - camY) / (float)viewH) * gOverlayRect.h;
            s_pathPts[k + 1].x = px;
            s_pathPts[k + 1].y = py;
            s_glowPts[k + 1].x = px;
            s_glowPts[k + 1].y = py + 1.5f;
         }

         destWX = pWpWX[nWp - 1];
         destWY = pWpWY[nWp - 1];

         SDL_SetRenderDrawColor(renderer, 42, 24, 8, 150);
         SDL_RenderLines(renderer, s_glowPts, nWp + 1);
         SDL_SetRenderDrawColor(renderer, 242, 208, 124, 215);
         SDL_RenderLines(renderer, s_pathPts, nWp + 1);
      }

      float gx = (float)(destWX - camX);
      float gy = (float)(destWY - camY);
      if (gx >= 0.0f && gx <= (float)viewW && gy >= 0.0f && gy <= (float)viewH)
      {
         float sx = gOverlayRect.x + (gx / (float)viewW) * gOverlayRect.w;
         float sy = gOverlayRect.y + (gy / (float)viewH) * gOverlayRect.h;
         float pulse = 0.75f + 0.25f * sinf((float)SDL_GetTicks() * 0.01f);
         float dw = (10.0f * pulse / 320.0f) * gOverlayRect.w;
         float dh = (5.0f * pulse / 200.0f) * gOverlayRect.h;
         SDL_FPoint pts[5] = {
            { sx,      sy - dh },
            { sx + dw, sy      },
            { sx,      sy + dh },
            { sx - dw, sy      },
            { sx,      sy - dh }
         };
         SDL_SetRenderDrawColor(renderer, 255, 224, 115, 235);
         SDL_RenderLines(renderer, pts, 5);
      }

      SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
      SDL_SetRenderClipRect(renderer, NULL);
#endif
   }

   // 1b. Draw Overhead Enemy HP & Full Resistance Cards + Party MOBA Cards & Skill Orbs in Battle
   VIDEO_RenderBattleEnemyStatsHUD(renderer, &hud);
   VIDEO_RenderBattlePartyAndOrbsHUD(renderer, &hud);

   // 2. Draw Fixed-Position Bottom-Left Isometric D-Pad in Map Exploration Mode
   if (gConfig.fUseTouchOverlay && hud.mode == kTouchModeMap)
   {
      int dIdx = (hud.fJoyActive && (int)hud.joyDir >= 0 && (int)hud.joyDir < 4) ? (int)hud.joyDir : 4;
      SDL_Texture *dpadTex = g_pDpadTex[dIdx];
      if (dpadTex)
      {
         float dpadW = hud.fJoyRadiusX * 2.0f * winW;
         float dpadH = hud.fJoyRadiusY * 2.0f * winH;
         Uint8 alpha = hud.fJoyActive ? 248 : 198;
         SDL_SetTextureAlphaMod(dpadTex, alpha);
#if SDL_VERSION_ATLEAST(3,0,0)
         SDL_FRect dpadRect = {
            hud.fJoyCenterX * winW - dpadW * 0.5f,
            hud.fJoyCenterY * winH - dpadH * 0.5f,
            dpadW,
            dpadH
         };
         SDL_RenderTexture(renderer, dpadTex, NULL, &dpadRect);
#endif
      }
   }

   // 3. Draw Context-Sensitive Chinese RPG HUD Buttons
   PALHUDRECT rects[16];
   int nBtns = PAL_TouchGetHUDButtons(hud.mode, hud.fBattleAutoActive, aspect, rects, 16);
   for (int i = 0; i < nBtns; i++)
   {
      PALHUDACTION act = rects[i].action;
      int hi = (hud.pressedAction == act) ? 1 : 0;
      if (act == kHudActionSearch && hud.fSearchAvailable)
      {
         hi = 1;
      }
      if (act == kHudActionReveal && hud.fRevealSearchables)
      {
         hi = 1;
      }
      if (act == kHudActionZoomIn && hud.iMapZoomIndex <= 0)
      {
         hi = 0;
      }
      SDL_Texture *tex = g_rgHudBtnTex[act][hi];
      Uint8 btnAlpha = gConfig.fUseTouchOverlay ? 255 : 155;
      if (act == kHudActionSpeed)
      {
         int sIdx = hud.iSpeedIndex;
         if (sIdx < 0) sIdx = 0;
         if (sIdx > 4) sIdx = 4;
         tex = g_pSpeedTex[sIdx];
      }
      else if (act == kHudActionZoomOut)
      {
         int zIdx = hud.iMapZoomIndex;
         if (zIdx < 0) zIdx = 0;
         if (zIdx > 9) zIdx = 9;
         tex = g_pZoomOutTex[zIdx];
         if (zIdx >= 9)
         {
            btnAlpha = 145;
         }
      }
      else if (act == kHudActionZoomIn)
      {
         if (hud.iMapZoomIndex <= 0)
         {
            btnAlpha = 115;
         }
      }
      else if (hud.mode == kTouchModeBattleWait && hud.fBattleAutoActive && act == kHudActionBack)
      {
         tex = g_pStopAutoTex[hi];
      }
      if (!tex) continue;

#if SDL_VERSION_ATLEAST(3,0,0)
      SDL_FRect dst = {
         rects[i].x * winW,
         rects[i].y * winH,
         rects[i].w * winW,
         rects[i].h * winH
      };
      if (hud.pressedAction == act)
      {
         // Subtle tactile press-in scale feedback
         float shrinkW = dst.w * 0.05f;
         float shrinkH = dst.h * 0.05f;
         dst.x += shrinkW * 0.5f;
         dst.y += shrinkH * 0.5f;
         dst.w -= shrinkW;
         dst.h -= shrinkH;
      }
      SDL_SetTextureAlphaMod(tex, btnAlpha);
      SDL_RenderTexture(renderer, tex, NULL, &dst);
#endif
   }
#endif
}

#endif

VOID
VIDEO_Shutdown(
   VOID
)
{
#if PAL_HAS_GLSL
	if( gRenderBackend.Init == VIDEO_GLSL_Init ) {
		VIDEO_GLSL_Destroy();
	}
#endif

#if PAL_HAS_SDL_GPU
   if (gfUseGPU)
   {
      VIDEO_GPU_Shutdown();
      gfUseGPU = FALSE;
   }
#endif

   if (gpScreen != NULL)
   {
      SDL_FreeSurface(gpScreen);
   }
   gpScreen = NULL;

   if (gpScreenBak != NULL)
   {
      SDL_FreeSurface(gpScreenBak);
   }
   gpScreenBak = NULL;

#if SDL_VERSION_ATLEAST(2,0,0)

#if defined(__IOS__) && PAL_HAS_TOUCH
   VIDEO_FreeAllUiMasks();
#endif
   VIDEO_DestroyTouchHUD();

   if (gpTouchOverlay)
   {
      SDL_DestroyTexture(gpTouchOverlay);
   }
   gpTouchOverlay = NULL;

   if (gpTexture)
   {
	  SDL_DestroyTexture(gpTexture);
   }
   gpTexture = NULL;

   if (gpRenderer)
   {
      SDL_DestroyRenderer(gpRenderer);
   }
   gpRenderer = NULL;

   if (gpWindow)
   {
      SDL_DestroyWindow(gpWindow);
   }
   gpWindow = NULL;

   if (gpPalette)
   {
      SDL_FreePalette(gpPalette);
   }
#endif
   gpPalette = NULL;

   if (gpScreenReal != NULL)
   {
      SDL_FreeSurface(gpScreenReal);
   }
   gpScreenReal = NULL;
}

#if SDL_VERSION_ATLEAST(2,0,0)
#if defined(__IOS__) && PAL_HAS_TOUCH
static inline uint32_t
VIDEO_PackScaledARGB(
   uint8_t a,
   int     r,
   int     g,
   int     b,
   int     fadeScale
)
{
   uint32_t sr = (uint32_t)((r * fadeScale) >> 8);
   uint32_t sg = (uint32_t)((g * fadeScale) >> 8);
   uint32_t sb = (uint32_t)((b * fadeScale) >> 8);
   if (sr > 255u) sr = 255u;
   if (sg > 255u) sg = 255u;
   if (sb > 255u) sb = 255u;
   return ((uint32_t)a << 24) | (sr << 16) | (sg << 8) | sb;
}

static VOID
VIDEO_CompositeUiOverlay(
   SDL_Renderer   *renderer,
   const uint32_t  lut[256],
   int             fadeScale,
   BOOL            bOverZoomedMap
)
{
   const uint8_t *uiMask = VIDEO_GetSurfaceUiMask(gpScreen, FALSE);
   if (uiMask == NULL || gpScreen == NULL || gpScreen->pixels == NULL || renderer == NULL)
   {
      return;
   }

   BOOL bHasOverlay = FALSE;
   if (bOverZoomedMap)
   {
      const uint64_t *m64 = (const uint64_t *)uiMask;
      for (int q = 0; q < (320 * 200) / 8; q++)
      {
         if (m64[q] != 0)
         {
            bHasOverlay = TRUE;
            break;
         }
      }
   }
   else
   {
      for (int k = 0; k < 320 * 200; k++)
      {
         uint8_t mv = uiMask[k];
         if ((mv >= UIMASK_HEX_SHADOW && mv <= UIMASK_MAT_MAX) ||
             (mv >= UIMASK_TEXT_SCROLL_INK && mv <= UIMASK_TEXT_LIGHT_REMAP))
         {
            bHasOverlay = TRUE;
            break;
         }
      }
   }

   if (!bHasOverlay)
   {
      return;
   }

   if (g_pUiOverlayTexture == NULL)
   {
      g_pUiOverlayTexture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, 320, 200);
      if (g_pUiOverlayTexture != NULL)
      {
         SDL_SetTextureBlendMode(g_pUiOverlayTexture, SDL_BLENDMODE_BLEND);
#if SDL_VERSION_ATLEAST(3,0,0)
         SDL_SetTextureScaleMode(g_pUiOverlayTexture, SDL_SCALEMODE_NEAREST);
#endif
      }
   }

   if (g_pUiOverlayTexture == NULL)
   {
      return;
   }

   void *uPixels = NULL;
   int uPitch = 0;
   SDL_LockTexture(g_pUiOverlayTexture, NULL, &uPixels, &uPitch);
   if (uPixels != NULL)
   {
      const uint8_t *scrBase = (const uint8_t *)gpScreen->pixels;
      int scrPitch = gpScreen->pitch;
      // Original PAL98 Ancient-Book & Sandalwood / Rosewood / Brass-Gold Palette
      uint32_t goldBright  = VIDEO_PackScaledARGB(0xFF, 250, 235, 190, fadeScale); // Gilded brass clasp
      uint32_t darkMoat    = VIDEO_PackScaledARGB(0xF8,  30,  16,   7, fadeScale); // Deep espresso-umber groove
      uint32_t wood0Shine  = VIDEO_PackScaledARGB(0xF4, 108,  76,  46, fadeScale); // Polished sandalwood top sheen
      uint32_t wood1Shine  = VIDEO_PackScaledARGB(0xF4, 108,  40,  20, fadeScale); // Polished rosewood top sheen
      uint32_t scrollShine = VIDEO_PackScaledARGB(0xF6, 184, 168, 138, fadeScale); // Warm ancient Xuan-paper highlight
      uint32_t scrollInner = VIDEO_PackScaledARGB(0xFA,  88,  67,  40, fadeScale); // Ancient-book ink-wood rule
      uint32_t scrollInk   = VIDEO_PackScaledARGB(0xFF,  28,  18,   9, fadeScale); // Rich pine-soot ink (松烟浓墨)
      uint32_t lightText   = VIDEO_PackScaledARGB(0xFF, 246, 236, 212, fadeScale); // Warm Xuan-paper ivory

      for (int y = 0; y < 200; y++)
      {
         uint32_t *uRow = (uint32_t *)((uint8_t *)uPixels + y * uPitch);
         const uint8_t *mRow = uiMask + y * 320;
         const uint8_t *sRow = scrBase + y * scrPitch;

         // Antique brass-gold outer bevel (#E4C88E -> #967240)
         uint32_t rowGoldOuter = VIDEO_PackScaledARGB(0xFF,
            228 - (y * 78) / 200,
            200 - (y * 86) / 200,
            142 - (y * 78) / 200,
            fadeScale);
         // Warm aged bamboo-ochre inner trim (#CCA66C -> #8E6B3C)
         uint32_t rowWoodInner = VIDEO_PackScaledARGB(0xFA,
            204 - (y * 62) / 200,
            166 - (y * 59) / 200,
            108 - (y * 48) / 200,
            fadeScale);
         // Style 0: Antique Sandalwood / Bamboo-Wood (#5C3E24 -> #3A2514)
         uint32_t rowWood0Bg = VIDEO_PackScaledARGB(0xF4,
             92 - (y * 34) / 200,
             62 - (y * 25) / 200,
             36 - (y * 16) / 200,
            fadeScale);
         // Style 1: Lacquered Rosewood / Mahogany (#582010 -> #361108)
         uint32_t rowWood1Bg = VIDEO_PackScaledARGB(0xF4,
             88 - (y * 34) / 200,
             32 - (y * 15) / 200,
             16 - (y *  8) / 200,
            fadeScale);
         // Style 2: Aged Ancient Book Paper (#AE9E80 -> #968668) + Dark Bamboo-Wood Bevel
         uint32_t rowScrollOuter = VIDEO_PackScaledARGB(0xFF,
            114 - (y * 40) / 200,
             88 - (y * 34) / 200,
             54 - (y * 22) / 200,
            fadeScale);
         uint32_t rowScrollBg = VIDEO_PackScaledARGB(0xF6,
            174 - (y * 24) / 200,
            158 - (y * 24) / 200,
            128 - (y * 24) / 200,
            fadeScale);

         for (int x = 0; x < 320; x++)
         {
            uint8_t mv = mRow[x];
            if (mv == UIMASK_HEX_FG)
            {
               uRow[x] = lut[sRow[x]];
            }
            else if (mv == UIMASK_TEXT_SCROLL_INK)
            {
               uRow[x] = scrollInk;
            }
            else if (mv == UIMASK_TEXT_LIGHT_REMAP)
            {
               uRow[x] = lightText;
            }
            else if (mv == UIMASK_TEXT_SHADOW)
            {
               uRow[x] = 0xEC160C05u;
            }
            else if (mv == UIMASK_HEX_GOLD_OUTER)
            {
               uRow[x] = rowGoldOuter;
            }
            else if (mv == UIMASK_HEX_GOLD_BRIGHT)
            {
               uRow[x] = goldBright;
            }
            else if (mv == UIMASK_HEX_DARK_MOAT)
            {
               uRow[x] = darkMoat;
            }
            else if (mv == UIMASK_HEX_JADE_INNER)
            {
               uRow[x] = rowWoodInner;
            }
            else if (mv == UIMASK_HEX_GLASS_SHINE || mv == UIMASK_HEX_GLASS_BG)
            {
               uRow[x] = (mv == UIMASK_HEX_GLASS_SHINE) ? wood0Shine : rowWood0Bg;
            }
            else if (mv == UIMASK_WOOD1_SHINE || mv == UIMASK_WOOD1_BG)
            {
               uRow[x] = (mv == UIMASK_WOOD1_SHINE) ? wood1Shine : rowWood1Bg;
            }
            else if (mv == UIMASK_SCROLL_OUTER)
            {
               uRow[x] = rowScrollOuter;
            }
            else if (mv == UIMASK_SCROLL_INNER)
            {
               uRow[x] = scrollInner;
            }
            else if (mv == UIMASK_SCROLL_SHINE || mv == UIMASK_SCROLL_BG)
            {
               BOOL nearInk =
                  (x > 0 && mRow[x - 1] == UIMASK_TEXT_SCROLL_INK) ||
                  (y > 0 && mRow[x - 320] == UIMASK_TEXT_SCROLL_INK);
               if (nearInk)
               {
                  uRow[x] = scrollShine;
               }
               else
               {
                  uRow[x] = (mv == UIMASK_SCROLL_SHINE) ? scrollShine : rowScrollBg;
               }
            }
            else if (mv == UIMASK_HEX_SHADOW)
            {
               uRow[x] = 0x7C120904u;
            }
            else if (bOverZoomedMap)
            {
               if (mv == 255)
               {
                  uRow[x] = lut[sRow[x]];
               }
               else if (mv > 0)
               {
                  uRow[x] = 0x8C000000u;
               }
               else
               {
                  BOOL nearOpaque =
                     (x > 0   && mRow[x - 1] >= UIMASK_TEXT_SCROLL_INK) ||
                     (x < 319 && mRow[x + 1] >= UIMASK_TEXT_SCROLL_INK) ||
                     (y > 0   && mRow[x - 320] >= UIMASK_TEXT_SCROLL_INK) ||
                     (y < 199 && mRow[x + 320] >= UIMASK_TEXT_SCROLL_INK);
                  uRow[x] = nearOpaque ? 0xB4000000u : 0x00000000u;
               }
            }
            else
            {
               uRow[x] = 0x00000000u;
            }
         }
      }
      SDL_UnlockTexture(g_pUiOverlayTexture);
#if SDL_VERSION_ATLEAST(3,0,0)
      SDL_FRect dstFullOverlayRect = {
         (float)gOverlayRect.x,
         (float)gOverlayRect.y,
         (float)gOverlayRect.w,
         (float)gOverlayRect.h
      };
      SDL_RenderTexture(renderer, g_pUiOverlayTexture, NULL, &dstFullOverlayRect);
#else
      SDL_RenderCopy(renderer, g_pUiOverlayTexture, NULL, &gOverlayRect);
#endif
   }
}
#endif

VOID
VIDEO_RenderCopy(
   VOID
)
{
	BOOL bRenderedZoomMap = FALSE;
#if defined(__IOS__) && PAL_HAS_TOUCH
	PAL_CheckMapCameraSceneSync();
	float fZoom = PAL_GetMapZoomFloat();
	BOOL bPanDetached = PAL_IsMapPanDetached();
	if ((fZoom > 1.001f || bPanDetached) &&
	    gpGlobals != NULL &&
	    gpGlobals->fInMainGame &&
	    !gpGlobals->fInBattle &&
	    !g_bInCutscene &&
	    PAL_GetCurrentMap() != NULL)
	{
		int viewW = (int)lroundf(320.0f * fZoom);
		int viewH = (int)lroundf(200.0f * fZoom);
		if (viewW < 320) viewW = 320;
		if (viewH < 200) viewH = 200;
		if (g_pZoomMapBuf == NULL)
		{
			g_pZoomMapBuf = (uint8_t *)calloc((size_t)(ZOOM_MAX_TEX_W + 2) * (size_t)(ZOOM_MAX_TEX_H + 2), 1);
		}
		if (g_pZoomMapTexture == NULL)
		{
			g_pZoomMapTexture = SDL_CreateTexture(
				gpRenderer,
				SDL_PIXELFORMAT_ARGB8888,
				SDL_TEXTUREACCESS_STREAMING,
				ZOOM_MAX_TEX_W + 2,
				ZOOM_MAX_TEX_H + 2
			);
		}

		if (g_pZoomMapBuf != NULL && g_pZoomMapTexture != NULL && gpPalette != NULL)
		{
			int camX, camY;
			if (bPanDetached)
			{
				float panCX = 0.0f, panCY = 0.0f;
				PAL_GetMapPanCenter(&panCX, &panCY);
				camX = (int)lroundf(panCX - (float)viewW * 0.5f);
				camY = (int)lroundf(panCY - (float)viewH * 0.5f);
			}
			else
			{
				int partyWX = PAL_X(gpGlobals->viewport) + PAL_X(gpGlobals->partyoffset);
				int partyWY = PAL_Y(gpGlobals->viewport) + PAL_Y(gpGlobals->partyoffset);
				camX = partyWX - viewW / 2;
				camY = partyWY - viewH / 2 - 8;
			}
			PAL_TouchSetZoomViewport(camX, camY, viewW, viewH);

			int ds = 1;
			if (gOverlayRect.w > 0)
			{
				if (viewW > gOverlayRect.w * 4)
					ds = 4;
				else if (viewW > gOverlayRect.w * 2)
					ds = 2;
			}

			int subX0 = (camX > ZOOM_WORLD_MIN_X) ? camX : ZOOM_WORLD_MIN_X;
			int subY0 = (camY > ZOOM_WORLD_MIN_Y) ? camY : ZOOM_WORLD_MIN_Y;
			int subX1 = (camX + viewW < ZOOM_WORLD_MAX_X) ? (camX + viewW) : ZOOM_WORLD_MAX_X;
			int subY1 = (camY + viewH < ZOOM_WORLD_MAX_Y) ? (camY + viewH) : ZOOM_WORLD_MAX_Y;
			if (ds > 1)
			{
				subX0 &= ~(ds - 1);
				subY0 &= ~(ds - 1);
				subX1 = (subX1 + ds - 1) & ~(ds - 1);
				subY1 = (subY1 + ds - 1) & ~(ds - 1);
				if (subX0 < ZOOM_WORLD_MIN_X) subX0 = ZOOM_WORLD_MIN_X;
				if (subY0 < ZOOM_WORLD_MIN_Y) subY0 = ZOOM_WORLD_MIN_Y;
				if (subX1 > ZOOM_WORLD_MAX_X) subX1 = ZOOM_WORLD_MAX_X;
				if (subY1 > ZOOM_WORLD_MAX_Y) subY1 = ZOOM_WORLD_MAX_Y;
			}
			int subW = subX1 - subX0;
			int subH = subY1 - subY0;

			uint32_t lut[256];
			int maxChan = 0;
			const SDL_Color *cols = gpPalette->colors;
			for (int c = 0; c < 256; c++)
			{
				if (cols[c].r > maxChan) maxChan = cols[c].r;
				if (cols[c].g > maxChan) maxChan = cols[c].g;
				if (cols[c].b > maxChan) maxChan = cols[c].b;
				lut[c] = 0xFF000000u | ((uint32_t)cols[c].r << 16) | ((uint32_t)cols[c].g << 8) | (uint32_t)cols[c].b;
			}
			int fadeScale = (maxChan * 256) / 248;
			if (fadeScale > 256) fadeScale = 256;

			SDL_RenderClear(gpRenderer);

			if (subW > 0 && subH > 0)
			{
				SDL_Surface *subSurf = SDL_CreateRGBSurfaceFrom(g_pZoomMapBuf, subW, subH, 8, subW, 0, 0, 0, 0);
				if (subSurf != NULL)
				{
					SDL_Rect subWorldRect = { subX0, subY0, subW, subH };
					PAL_RenderZoomedMapRect(subSurf, &subWorldRect);
					SDL_FreeSurface(subSurf);
				}

#if SDL_VERSION_ATLEAST(3,0,0)
				BOOL bSmoothDownscale = (viewW * 3 >= gOverlayRect.w * 2) ||
					(gConfig.pszScaleQuality && atoi(gConfig.pszScaleQuality) == 1);
				SDL_SetTextureScaleMode(
					g_pZoomMapTexture,
					bSmoothDownscale ? SDL_SCALEMODE_LINEAR : SDL_SCALEMODE_NEAREST
				);
#endif

				int outW = subW / ds;
				int outH = subH / ds;
				SDL_Rect lockRect = { 0, 0, outW + 1, outH + 1 };
				void *zPixels = NULL;
				int zPitch = 0;
				SDL_LockTexture(g_pZoomMapTexture, &lockRect, &zPixels, &zPitch);
				if (zPixels != NULL)
				{
					uint8_t *dstRow = (uint8_t *)zPixels;
					if (ds == 1)
					{
						const uint8_t *srcRow = g_pZoomMapBuf;
						for (int y = 0; y < outH; y++)
						{
							uint32_t *dst32 = (uint32_t *)dstRow;
							for (int x = 0; x < outW; x++)
							{
								dst32[x] = lut[srcRow[x]];
							}
							dst32[outW] = 0xFF000000u;
							srcRow += subW;
							dstRow += zPitch;
						}
					}
					else if (ds == 2)
					{
						for (int y = 0; y < outH; y++)
						{
							const uint8_t *r0 = g_pZoomMapBuf + (size_t)(y * 2) * (size_t)subW;
							const uint8_t *r1 = r0 + subW;
							uint32_t *dst32 = (uint32_t *)dstRow;
							for (int x = 0; x < outW; x++)
							{
								int sx = x * 2;
								uint32_t c0 = lut[r0[sx]];
								uint32_t c1 = lut[r0[sx + 1]];
								uint32_t c2 = lut[r1[sx]];
								uint32_t c3 = lut[r1[sx + 1]];
								uint32_t rb = ((c0 & 0x00FF00FFu) + (c1 & 0x00FF00FFu) + (c2 & 0x00FF00FFu) + (c3 & 0x00FF00FFu) + 0x00020002u) >> 2;
								uint32_t g  = ((c0 & 0x0000FF00u) + (c1 & 0x0000FF00u) + (c2 & 0x0000FF00u) + (c3 & 0x0000FF00u) + 0x00000200u) >> 2;
								dst32[x] = 0xFF000000u | (rb & 0x00FF00FFu) | (g & 0x0000FF00u);
							}
							dst32[outW] = 0xFF000000u;
							dstRow += zPitch;
						}
					}
					else
					{
						for (int y = 0; y < outH; y++)
						{
							const uint8_t *r0 = g_pZoomMapBuf + (size_t)(y * 4) * (size_t)subW;
							const uint8_t *r1 = r0 + subW;
							const uint8_t *r2 = r1 + subW;
							const uint8_t *r3 = r2 + subW;
							uint32_t *dst32 = (uint32_t *)dstRow;
							for (int x = 0; x < outW; x++)
							{
								int sx = x * 4;
								uint32_t rb = 0x00080008u;
								uint32_t g  = 0x00000800u;
								for (int k = 0; k < 4; k++)
								{
									uint32_t p0 = lut[r0[sx + k]];
									uint32_t p1 = lut[r1[sx + k]];
									uint32_t p2 = lut[r2[sx + k]];
									uint32_t p3 = lut[r3[sx + k]];
									rb += (p0 & 0x00FF00FFu) + (p1 & 0x00FF00FFu) + (p2 & 0x00FF00FFu) + (p3 & 0x00FF00FFu);
									g  += (p0 & 0x0000FF00u) + (p1 & 0x0000FF00u) + (p2 & 0x0000FF00u) + (p3 & 0x0000FF00u);
								}
								dst32[x] = 0xFF000000u | ((rb >> 4) & 0x00FF00FFu) | ((g >> 4) & 0x0000FF00u);
							}
							dst32[outW] = 0xFF000000u;
							dstRow += zPitch;
						}
					}
					uint32_t *botRow = (uint32_t *)dstRow;
					for (int x = 0; x <= outW; x++)
					{
						botRow[x] = 0xFF000000u;
					}
					SDL_UnlockTexture(g_pZoomMapTexture);

#if SDL_VERSION_ATLEAST(3,0,0)
					SDL_FRect srcZoomRect = { 0.0f, 0.0f, (float)outW, (float)outH };
					SDL_FRect dstZoomRect = {
						(float)gOverlayRect.x + ((float)(subX0 - camX) / (float)viewW) * (float)gOverlayRect.w,
						(float)gOverlayRect.y + ((float)(subY0 - camY) / (float)viewH) * (float)gOverlayRect.h,
						((float)subW / (float)viewW) * (float)gOverlayRect.w,
						((float)subH / (float)viewH) * (float)gOverlayRect.h
					};
					SDL_RenderTexture(gpRenderer, g_pZoomMapTexture, &srcZoomRect, &dstZoomRect);
#else
					SDL_Rect srcZoomRect = { 0, 0, outW, outH };
					SDL_Rect dstZoomRect = {
						gOverlayRect.x + (int)(((float)(subX0 - camX) / (float)viewW) * (float)gOverlayRect.w),
						gOverlayRect.y + (int)(((float)(subY0 - camY) / (float)viewH) * (float)gOverlayRect.h),
						(int)(((float)subW / (float)viewW) * (float)gOverlayRect.w),
						(int)(((float)subH / (float)viewH) * (float)gOverlayRect.h)
					};
					SDL_RenderCopy(gpRenderer, g_pZoomMapTexture, &srcZoomRect, &dstZoomRect);
#endif
				}
			}

			// Composite crisp 1x Shushan-Hextech UI / Dialog / Menu / Shop layer right on top of the zoomed map!
			VIDEO_CompositeUiOverlay(gpRenderer, lut, fadeScale, TRUE);

			bRenderedZoomMap = TRUE;
		}
	}
#endif

	if (!bRenderedZoomMap)
	{
		void *texture_pixels;
		int texture_pitch;

		SDL_LockTexture(gpTexture, NULL, &texture_pixels, &texture_pitch);
		memset(texture_pixels, 0, gTextureRect.y * texture_pitch);
		uint8_t *pixels = (uint8_t *)texture_pixels + gTextureRect.y * texture_pitch;
		uint8_t *src = (uint8_t *)gpScreenReal->pixels;
		int left_pitch = gTextureRect.x << 2;
		int right_pitch = texture_pitch - ((gTextureRect.x + gTextureRect.w) << 2);
		for (int y = 0; y < gTextureRect.h; y++, src += gpScreenReal->pitch)
		{
			memset(pixels, 0, left_pitch); pixels += left_pitch;
			memcpy(pixels, src, 320 << 2); pixels += 320 << 2;
			memset(pixels, 0, right_pitch); pixels += right_pitch;
		}
		memset(pixels, 0, gTextureRect.y * texture_pitch);
		SDL_UnlockTexture(gpTexture);

		SDL_RenderClear(gpRenderer);
		SDL_RenderCopy(gpRenderer, gpTexture, NULL, NULL);

#if defined(__IOS__) && PAL_HAS_TOUCH
		if (!g_bInCutscene && !g_bInCrossFade && g_wShakeTime == 0 && gpPalette != NULL)
		{
			uint32_t lut[256];
			int maxChan = 0;
			const SDL_Color *cols = gpPalette->colors;
			for (int c = 0; c < 256; c++)
			{
				if (cols[c].r > maxChan) maxChan = cols[c].r;
				if (cols[c].g > maxChan) maxChan = cols[c].g;
				if (cols[c].b > maxChan) maxChan = cols[c].b;
				lut[c] = 0xFF000000u | ((uint32_t)cols[c].r << 16) | ((uint32_t)cols[c].g << 8) | (uint32_t)cols[c].b;
			}
			int fadeScale = (maxChan * 256) / 248;
			if (fadeScale > 256) fadeScale = 256;
			VIDEO_CompositeUiOverlay(gpRenderer, lut, fadeScale, FALSE);
		}
#endif
	}
#if defined(__IOS__) && PAL_HAS_TOUCH
	{
		int outW = 0, outH = 0;
		SDL_GetRendererOutputSize(gpRenderer, &outW, &outH);
		VIDEO_RenderTouchHUD(gpRenderer, outW, outH);
	}
#else
	if (gConfig.fUseTouchOverlay)
	{
#if SDL_VERSION_ATLEAST(3,0,0)
        SDL_FRect fOverlayRect = { gOverlayRect.x, gOverlayRect.y, gOverlayRect.w, gOverlayRect.h };
        SDL_RenderTexture(gpRenderer, gpTouchOverlay, NULL, &fOverlayRect);
#else
		SDL_RenderCopy(gpRenderer, gpTouchOverlay, NULL, &gOverlayRect);
#endif
	}
#endif
	SDL_RenderPresent(gpRenderer);
}
#endif

#if SDL_VERSION_ATLEAST(2,0,0)
static VOID
VIDEO_ShowFrame(
   VOID
)
{
   //
   // Rendering from the background gets an iOS app terminated. PAL_ProcessEvent() holds
   // the game there, and coming back redraws the screen, so this frame is not missed.
   //
   if (!g_bAppInBackground)
   {
      gRenderBackend.RenderCopy();
   }
}
#endif

VOID
VIDEO_UpdateScreen(
   const SDL_Rect  *lpRect
)
/*++
  Purpose:

    Update the screen area specified by lpRect.

  Parameters:

    [IN]  lpRect - Screen area to update.

  Return value:

    None.

--*/
{
   SDL_Rect        srcrect, dstrect;
   short           offset = 240 - 200;
   short           screenRealHeight = gpScreenReal->h;
   short           screenRealY = 0;

#if SDL_VERSION_ATLEAST(2,0,0)
   if (g_bRenderPaused)
   {
	   return;
   }
#endif

   //
   // Lock surface if needed
   //
   if (SDL_MUSTLOCK(gpScreenReal))
   {
      if (SDL_LockSurface(gpScreenReal) < 0)
         return;
   }

   if (!bScaleScreen)
   {
      screenRealHeight -= offset;
      screenRealY = offset / 2;
   }

   if (lpRect != NULL)
   {
      if (lpRect->x != 0 || lpRect->y != 0 || lpRect->w != 320 || lpRect->h != 200)
      {
         g_fSceneCleanForZoom = FALSE;
      }
      dstrect.x = (SHORT)((INT)(lpRect->x) * gpScreenReal->w / gpScreen->w);
      dstrect.y = (SHORT)((INT)(screenRealY + lpRect->y) * screenRealHeight / gpScreen->h);
      dstrect.w = (WORD)((DWORD)(lpRect->w) * gpScreenReal->w / gpScreen->w);
      dstrect.h = (WORD)((DWORD)(lpRect->h) * screenRealHeight / gpScreen->h);

      VIDEO_BlitToScreenReal(gpScreen, (SDL_Rect *)lpRect, &dstrect);
   }
   else if (g_wShakeTime != 0)
   {
      //
      // Shake the screen
      //
      srcrect.x = 0;
      srcrect.y = 0;
      srcrect.w = 320;
      srcrect.h = 200 - g_wShakeLevel;

      dstrect.x = 0;
      dstrect.y = screenRealY;
      dstrect.w = 320 * gpScreenReal->w / gpScreen->w;
      dstrect.h = (200 - g_wShakeLevel) * screenRealHeight / gpScreen->h;

      if (g_wShakeTime & 1)
      {
         srcrect.y = g_wShakeLevel;
      }
      else
      {
         dstrect.y = (screenRealY + g_wShakeLevel) * screenRealHeight / gpScreen->h;
      }

      VIDEO_BlitToScreenReal(gpScreen, &srcrect, &dstrect);

      if (g_wShakeTime & 1)
      {
         dstrect.y = (screenRealY + screenRealHeight - g_wShakeLevel) * screenRealHeight / gpScreen->h;
      }
      else
      {
         dstrect.y = screenRealY;
      }

      dstrect.h = g_wShakeLevel * screenRealHeight / gpScreen->h;

      VIDEO_FillScreenRealBlack(&dstrect);

#if SDL_MAJOR_VERSION == 1 && SDL_MINOR_VERSION <= 2
      dstrect.x = dstrect.y = 0;
      dstrect.w = gpScreenReal->w;
      dstrect.h = gpScreenReal->h;
#endif
      g_wShakeTime--;
   }
   else
   {
      dstrect.x = 0;
      dstrect.y = screenRealY;
      dstrect.w = gpScreenReal->w;
      dstrect.h = screenRealHeight;

      VIDEO_BlitToScreenReal(gpScreen, NULL, &dstrect);

#if SDL_MAJOR_VERSION == 1 && SDL_MINOR_VERSION <= 2
      dstrect.x = dstrect.y = 0;
      dstrect.w = gpScreenReal->w;
      dstrect.h = gpScreenReal->h;
#endif
   }

#if SDL_VERSION_ATLEAST(2,0,0)
   VIDEO_ShowFrame();
#else
   SDL_UpdateRect(gpScreenReal, dstrect.x, dstrect.y, dstrect.w, dstrect.h);
#endif

   if (SDL_MUSTLOCK(gpScreenReal))
   {
	   SDL_UnlockSurface(gpScreenReal);
   }
}

VOID
VIDEO_SetPalette(
   SDL_Color        rgPalette[256]
)
/*++
  Purpose:

    Set the palette of the screen.

  Parameters:

    [IN]  rgPalette - array of 256 colors.

  Return value:

    None.

--*/
{
#if SDL_VERSION_ATLEAST(2,0,0)
   SDL_Rect rect;

   rect.x = 0;
   rect.y = 0;
   rect.w = 320;
   rect.h = 200;

#if PAL_HAS_SDL_GPU
   if (gfUseGPU && !VIDEO_GPU_OnSetPalette(rgPalette))
   {
      VIDEO_UpdateScreen(&rect);
      return;
   }
#endif

   SDL_SetPaletteColors(gpPalette, rgPalette, 0, 256);

   SDL_SetSurfacePalette(gpScreen, gpPalette);
   SDL_SetSurfacePalette(gpScreenBak, gpPalette);

   //
   // HACKHACK: need to invalidate gpScreen->map otherwise the palette
   // would not be effective during blit
   //
   SDL_SetSurfaceColorMod(gpScreen, 0, 0, 0);
   SDL_SetSurfaceColorMod(gpScreen, 0xFF, 0xFF, 0xFF);
   SDL_SetSurfaceColorMod(gpScreenBak, 0, 0, 0);
   SDL_SetSurfaceColorMod(gpScreenBak, 0xFF, 0xFF, 0xFF);

   VIDEO_UpdateScreen(&rect);
#else
   SDL_SetPalette(gpScreen, SDL_LOGPAL | SDL_PHYSPAL, rgPalette, 0, 256);
   SDL_SetPalette(gpScreenBak, SDL_LOGPAL | SDL_PHYSPAL, rgPalette, 0, 256);
   SDL_SetPalette(gpScreenReal, SDL_LOGPAL | SDL_PHYSPAL, rgPalette, 0, 256);
# if defined(PAL_FORCE_UPDATE_ON_PALETTE_SET)
   {
      static UINT32 time = 0;
      if (SDL_GetTicks() - time > 50)
      {
	      SDL_UpdateRect(gpScreenReal, 0, 0, gpScreenReal->w, gpScreenReal->h);
	      time = SDL_GetTicks();
      }
   }
# endif
#endif
}

BOOL
VIDEO_SetPaletteFade(
   INT              iMode,
   const SDL_Color *rgPalette,
   const SDL_Color *rgPaletteTo,
   INT              iStep,
   BYTE             bColor
)
/*++
  Purpose:

    Show one step of a palette fade without working out the resulting palette, for
    backends that can do that themselves.

  Parameters:

    [IN]  iMode - one of the kPaletteFade values.

    [IN]  rgPalette - the palette to fade.

    [IN]  rgPaletteTo - the palette to fade to, for kPaletteFadeBlend.

    [IN]  iStep - how far the fade has gone; the meaning depends on the mode.

    [IN]  bColor - the palette entry to fade from or to, where the mode has one.

  Return value:

    TRUE if done. FALSE if the caller has to calculate the palette and set it.

--*/
{
#if PAL_HAS_SDL_GPU
   if (gfUseGPU && VIDEO_GPU_SetPaletteFade(iMode, rgPalette, rgPaletteTo, iStep, bColor))
   {
      SDL_Rect rect = { 0, 0, 320, 200 };
      VIDEO_UpdateScreen(&rect);
      return TRUE;
   }
#endif
   return FALSE;
}

VOID
VIDEO_Blit(
   SDL_Surface    *lpSrc,
   const SDL_Rect *lpSrcRect,
   SDL_Surface    *lpDst,
   const SDL_Rect *lpDstRect
)
/*++
  Purpose:

    Copy an area between two surfaces.

  Parameters:

    [IN]  lpSrc - the surface to copy from.

    [IN]  lpSrcRect - the area to copy, NULL for the whole surface.

    [OUT] lpDst - the surface to copy to.

    [IN]  lpDstRect - where to copy to, NULL for the top left corner.

  Return value:

    None.

--*/
{
   if (!VIDEO_GPU_Blit(lpSrc, lpSrcRect, lpDst, lpDstRect))
   {
      //
      // SDL_BlitSurface changes the destination rectangle in some versions.
      //
      SDL_Rect dstrect;

      if (lpDstRect != NULL)
      {
         dstrect = *lpDstRect;
      }
      SDL_BlitSurface(lpSrc, (SDL_Rect *)lpSrcRect, lpDst, (lpDstRect != NULL) ? &dstrect : NULL);
   }

#if defined(__IOS__) && PAL_HAS_TOUCH && SDL_VERSION_ATLEAST(2,0,0)
   if (lpSrc != NULL && lpDst != NULL)
   {
      int sx = lpSrcRect ? lpSrcRect->x : 0;
      int sy = lpSrcRect ? lpSrcRect->y : 0;
      int cw = lpSrcRect ? lpSrcRect->w : lpSrc->w;
      int ch = lpSrcRect ? lpSrcRect->h : lpSrc->h;
      int dx = lpDstRect ? lpDstRect->x : 0;
      int dy = lpDstRect ? lpDstRect->y : 0;

      if (sx < 0) { cw += sx; dx -= sx; sx = 0; }
      if (sy < 0) { ch += sy; dy -= sy; sy = 0; }
      if (dx < 0) { cw += dx; sx -= dx; dx = 0; }
      if (dy < 0) { ch += dy; sy -= dy; dy = 0; }
      if (sx + cw > lpSrc->w) cw = lpSrc->w - sx;
      if (sy + ch > lpSrc->h) ch = lpSrc->h - sy;
      if (dx + cw > lpDst->w) cw = lpDst->w - dx;
      if (dy + ch > lpDst->h) ch = lpDst->h - dy;

      if (cw > 0 && ch > 0)
      {
         const uint8_t *srcMask = VIDEO_GetSurfaceUiMask(lpSrc, FALSE);
         if (srcMask != NULL)
         {
            uint8_t *dstMask = VIDEO_GetSurfaceUiMask(lpDst, TRUE);
            if (dstMask != NULL)
            {
               for (int r = 0; r < ch; r++)
               {
                  memcpy(dstMask + (dy + r) * lpDst->w + dx,
                         srcMask + (sy + r) * lpSrc->w + sx,
                         (size_t)cw);
               }
            }
         }
         else if (lpDst == gpScreen && !g_bInsideMakeScene)
         {
            uint8_t *dstMask = VIDEO_GetSurfaceUiMask(gpScreen, TRUE);
            if (dstMask != NULL)
            {
               for (int r = 0; r < ch; r++)
               {
                  memset(dstMask + (dy + r) * lpDst->w + dx, 255, (size_t)cw);
               }
            }
         }
      }
   }
#endif
}

VOID
VIDEO_FillRect(
   SDL_Surface    *lpDst,
   const SDL_Rect *lpRect,
   BYTE            bColor
)
/*++
  Purpose:

    Fill an area of a surface with one color.

  Parameters:

    [OUT] lpDst - the surface.

    [IN]  lpRect - the area, NULL for the whole surface.

    [IN]  bColor - the color.

  Return value:

    None.

--*/
{
   if (!VIDEO_GPU_Fill(lpDst, lpRect, bColor))
   {
      SDL_FillRect(lpDst, (SDL_Rect *)lpRect, bColor);
   }

#if defined(__IOS__) && PAL_HAS_TOUCH && SDL_VERSION_ATLEAST(2,0,0)
   if (lpDst == gpScreen && !g_bInsideMakeScene && lpDst != NULL)
   {
      uint8_t *dstMask = VIDEO_GetSurfaceUiMask(gpScreen, TRUE);
      if (dstMask != NULL)
      {
         int dx = lpRect ? lpRect->x : 0;
         int dy = lpRect ? lpRect->y : 0;
         int cw = lpRect ? lpRect->w : lpDst->w;
         int ch = lpRect ? lpRect->h : lpDst->h;
         if (dx < 0) { cw += dx; dx = 0; }
         if (dy < 0) { ch += dy; dy = 0; }
         if (dx + cw > lpDst->w) cw = lpDst->w - dx;
         if (dy + ch > lpDst->h) ch = lpDst->h - dy;
         if (cw > 0 && ch > 0)
         {
            for (int r = 0; r < ch; r++)
            {
               uint8_t *mRow = dstMask + (dy + r) * lpDst->w + dx;
               for (int c = 0; c < cw; c++)
               {
                  uint8_t pm = mRow[c];
                  BOOL inHex = (pm >= UIMASK_HEX_SHADOW && pm <= UIMASK_MAT_MAX) || (pm >= UIMASK_TEXT_SCROLL_INK && pm <= UIMASK_TEXT_LIGHT_REMAP);
                  mRow[c] = inHex ? UIMASK_HEX_FG : 255;
               }
            }
         }
      }
   }
#endif
}

VOID
VIDEO_FreeSurface(
   SDL_Surface    *lpSurface
)
{
#if PAL_HAS_SDL_GPU
   VIDEO_GPU_UnregisterSurface(lpSurface);
#endif
#if defined(__IOS__) && PAL_HAS_TOUCH && SDL_VERSION_ATLEAST(2,0,0)
   VIDEO_FreeSurfaceUiMask(lpSurface);
#endif
   SDL_FreeSurface(lpSurface);
}

VOID
VIDEO_Resize(
   INT             w,
   INT             h
)
/*++
  Purpose:

    This function is called when user resized the window.

  Parameters:

    [IN]  w - width of the window after resizing.

    [IN]  h - height of the window after resizing.

  Return value:

    None.

--*/
{
#if SDL_VERSION_ATLEAST(2,0,0)
   SDL_Rect rect;

#if PAL_HAS_SDL_GPU
   if (gfUseGPU)
   {
      VIDEO_GPU_SetupTouchArea(w, h);
   }
   else
#endif
   {
      if (gpTexture)
      {
         SDL_DestroyTexture(gpTexture);
      }

      gpTexture = gRenderBackend.CreateTexture(w, h);

      if (gpTexture == NULL)
      {
         TerminateOnError("Re-creating texture failed on window resize!\n");
      }
   }

   rect.x = 0;
   rect.y = 0;
   rect.w = 320;
   rect.h = 200;

   VIDEO_UpdateScreen(&rect);
#else
   DWORD                    flags;
   PAL_LARGE SDL_Color      palette[256];
   int                      i, bpp;

   //
   // Get the original palette.
   //
   if (gpScreenReal->format->palette != NULL)
   {
      for (i = 0; i < gpScreenReal->format->palette->ncolors; i++)
      {
         palette[i] = gpScreenReal->format->palette->colors[i];
      }
   }
   else i = 0;

   //
   // Create the screen surface.
   //
   flags = gpScreenReal->flags;
   bpp = gpScreenReal->format->BitsPerPixel;

   SDL_FreeSurface(gpScreenReal);
   gpScreenReal = SDL_SetVideoMode(w, h, bpp, flags);

   if (gpScreenReal == NULL)
   {
      //
      // Fall back to software windowed mode in default size.
      //
      gpScreenReal = SDL_SetVideoMode(PAL_DEFAULT_WINDOW_WIDTH, PAL_DEFAULT_WINDOW_HEIGHT, bpp, SDL_SWSURFACE);
   }

   SDL_SetPalette(gpScreenReal, SDL_PHYSPAL | SDL_LOGPAL, palette, 0, i);
   VIDEO_UpdateScreen(NULL);

   gpPalette = gpScreenReal->format->palette;
#endif
}

SDL_Color *
VIDEO_GetPalette(
   VOID
)
/*++
  Purpose:

    Get the current palette of the screen.

  Parameters:

    None.

  Return value:

    Pointer to the current palette.

--*/
{
#if PAL_HAS_SDL_GPU
   if (gfUseGPU)
   {
      VIDEO_GPU_ResolvePalette(gpPalette->colors);
   }
#endif
   return gpPalette->colors;
}

VOID
VIDEO_ToggleScaleScreen(
   VOID
)
/*++
  Purpose:

    Toggle scalescreen mode, only used in some platforms.

  Parameters:

    None.

  Return value:

    None.

--*/
{
   //
   // The SDL_GPU backend only shows the game at full height, which is why VIDEO_Startup()
   // does not use it for an unscaled screen either.
   //
   if (gfUseGPU)
   {
      return;
   }

   bScaleScreen = !bScaleScreen;
   VIDEO_Resize(PAL_DEFAULT_WINDOW_WIDTH, PAL_DEFAULT_WINDOW_HEIGHT);
   VIDEO_UpdateScreen(NULL);
}

VOID
VIDEO_ToggleFullscreen(
   VOID
)
/*++
  Purpose:

    Toggle fullscreen mode.

  Parameters:

    None.

  Return value:

    None.

--*/
{
#if SDL_VERSION_ATLEAST(2,0,0)
	if (gConfig.fFullScreen)
	{
		SDL_SetWindowFullscreen(gpWindow, 0);
		gConfig.fFullScreen = FALSE;
	}
	else
	{
		SDL_SetWindowFullscreen(gpWindow, SDL_WINDOW_FULLSCREEN_DESKTOP);
		gConfig.fFullScreen = TRUE;
	}
#else
   DWORD                    flags;
   PAL_LARGE SDL_Color      palette[256];
   int                      i, bpp;

   //
   // Get the original palette.
   //
   if (gpScreenReal->format->palette != NULL)
   {
      for (i = 0; i < gpScreenReal->format->palette->ncolors; i++)
      {
         palette[i] = gpScreenReal->format->palette->colors[i];
      }
   }

   //
   // Get the flags and bpp of the original screen surface
   //
   flags = gpScreenReal->flags;
   bpp = gpScreenReal->format->BitsPerPixel;

   if (flags & SDL_FULLSCREEN)
   {
      //
      // Already in fullscreen mode. Remove the fullscreen flag.
      //
      flags &= ~SDL_FULLSCREEN;
      flags |= SDL_RESIZABLE;
      SDL_ShowCursor(TRUE);
   }
   else
   {
      //
      // Not in fullscreen mode. Set the fullscreen flag.
      //
      flags |= SDL_FULLSCREEN;
      SDL_ShowCursor(FALSE);
   }

   //
   // Free the original screen surface
   //
   SDL_FreeSurface(gpScreenReal);

   //
   // ... and create a new one
   //
   if (gConfig.dwScreenWidth == 640 && gConfig.dwScreenHeight == 400 && (flags & SDL_FULLSCREEN))
   {
      gpScreenReal = SDL_SetVideoMode(640, 480, bpp, flags);
   }
   else if (gConfig.dwScreenWidth == 640 && gConfig.dwScreenHeight == 480 && !(flags & SDL_FULLSCREEN))
   {
      gpScreenReal = SDL_SetVideoMode(640, 400, bpp, flags);
   }
   else
   {
      gpScreenReal = SDL_SetVideoMode(gConfig.dwScreenWidth, gConfig.dwScreenHeight, bpp, flags);
   }

   VIDEO_SetPalette(palette);

   //
   // Update the screen
   //
   VIDEO_UpdateScreen(NULL);
#endif
}

VOID
VIDEO_ChangeDepth(
   INT             bpp
)
/*++
  Purpose:

    Change screen color depth.

  Parameters:

    [IN]  bpp - bits per pixel (0 = default).

  Return value:

    None.

--*/
{
#if !SDL_VERSION_ATLEAST(2,0,0)
   DWORD                    flags;
   int                      w, h;

   //
   // Get the flags and resolution of the original screen surface
   //
   flags = gpScreenReal->flags;
   w = gpScreenReal->w;
   h = gpScreenReal->h;

   //
   // Clear the screen surface.
   //
   SDL_FillRect(gpScreenReal, NULL, 0);

   //
   // Create the screen surface.
   //
   SDL_FreeSurface(gpScreenReal);
   gpScreenReal = SDL_SetVideoMode(w, h, (bpp == 0)?8:bpp, flags);

   if (gpScreenReal == NULL)
   {
      //
      // Fall back to software windowed mode in default size.
      //
      gpScreenReal = SDL_SetVideoMode(PAL_DEFAULT_WINDOW_WIDTH, PAL_DEFAULT_WINDOW_HEIGHT, (bpp == 0)?8:bpp, SDL_SWSURFACE);
   }

   gpPalette = gpScreenReal->format->palette;
#endif
}

VOID
VIDEO_SaveScreenshot(
   VOID
)
/*++
  Purpose:

    Save the screenshot of current screen to a BMP file.

  Parameters:

    None.

  Return value:

    None.

--*/
{
	char filename[80];
#ifdef _WIN32
	SYSTEMTIME st;
	GetLocalTime(&st);
	sprintf(filename, "%04d%02d%02d%02d%02d%02d%03d.bmp", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
#elif !defined( GEKKO )
	struct timeval tv;
	struct tm *ptm;
	gettimeofday(&tv, NULL);
	ptm = localtime(&tv.tv_sec);
	sprintf(filename, "%04d%02d%02d%02d%02d%02d%03d.bmp", ptm->tm_year + 1900, ptm->tm_mon + 1, ptm->tm_mday, ptm->tm_hour, ptm->tm_min, ptm->tm_sec, (int)(tv.tv_usec / 1000));
#endif

	//
	// Save the screenshot.
	//
#if PAL_HAS_SDL_GPU
	if (gfUseGPU)
	{
		//
		// The screen is on the GPU, so the file gets a copy of its own. Palette fades on the
		// GPU leave the palette behind, so this first has to work out where it is now.
		//
		SDL_Surface *pCopy = SDL_CreateSurface(320, 200, SDL_PIXELFORMAT_INDEX8);

		if (pCopy != NULL)
		{
			VIDEO_GPU_ResolvePalette(gpPalette->colors);
			VIDEO_UpdateSurfacePalette(pCopy);
			if (VIDEO_GPU_ReadSurface(gpScreen, (BYTE *)pCopy->pixels, pCopy->pitch))
			{
				SDL_SaveBMP(pCopy, PAL_CombinePath(0, gConfig.pszSavePath, filename));
			}
			SDL_FreeSurface(pCopy);
		}
		return;
	}
#endif
#if SDL_VERSION_ATLEAST(2,0,0)
	SDL_SaveBMP(gpScreen, PAL_CombinePath(0, gConfig.pszSavePath, filename));
#else
	SDL_SaveBMP(gpScreenReal, PAL_CombinePath(0, gConfig.pszSavePath, filename));
#endif
}

VOID
VIDEO_ShakeScreen(
   WORD           wShakeTime,
   WORD           wShakeLevel
)
/*++
  Purpose:

    Set the screen shake time and level.

  Parameters:

    [IN]  wShakeTime - how many times should we shake the screen.

    [IN]  wShakeLevel - level of shaking.

  Return value:

    None.

--*/
{
   g_wShakeTime = wShakeTime;
   g_wShakeLevel = wShakeLevel;
}

VOID
VIDEO_SwitchScreen(
   WORD           wSpeed
)
/*++
  Purpose:

    Switch the screen from the backup screen buffer to the current screen buffer.
    NOTE: This will destroy the backup buffer.

  Parameters:

    [IN]  wSpeed - speed of fading (the larger value, the slower).

  Return value:

    None.

--*/
{
   int               i, j;
   const int         rgIndex[6] = {0, 3, 1, 5, 2, 4};
   SDL_Rect          dstrect;

   short             offset = 240 - 200;
   short             screenRealHeight = gpScreenReal->h;
   short             screenRealY = 0;

   if (!bScaleScreen)
   {
      screenRealHeight -= offset;
      screenRealY = offset / 2;
   }

   wSpeed++;
   wSpeed *= 10;

   for (i = 0; i < 6; i++)
   {
      if (!VIDEO_GPU_SwitchStep(gpScreen, gpScreenBak, rgIndex[i]))
      {
         const uint8_t *srcMask = VIDEO_GetSurfaceUiMask(gpScreen, FALSE);
         uint8_t *bakMask = srcMask ? VIDEO_GetSurfaceUiMask(gpScreenBak, TRUE) : NULL;
         for (j = rgIndex[i]; j < gpScreen->pitch * gpScreen->h; j += 6)
         {
            ((LPBYTE)(gpScreenBak->pixels))[j] = ((LPBYTE)(gpScreen->pixels))[j];
         }
         if (srcMask != NULL && bakMask != NULL)
         {
            for (j = rgIndex[i]; j < 320 * 200; j += 6)
            {
               bakMask[j] = srcMask[j];
            }
         }
      }

      //
      // Draw the backup buffer to the screen
      //
      dstrect.x = 0;
      dstrect.y = screenRealY;
      dstrect.w = gpScreenReal->w;
      dstrect.h = screenRealHeight;

	  if (SDL_MUSTLOCK(gpScreenReal))
	  {
		  if (SDL_LockSurface(gpScreenReal) < 0)
			  return;
	  }

      VIDEO_BlitToScreenReal(gpScreenBak, NULL, &dstrect);
#if SDL_VERSION_ATLEAST(2, 0, 0)
      VIDEO_ShowFrame();
#else
      SDL_UpdateRect(gpScreenReal, 0, 0, gpScreenReal->w, gpScreenReal->h);
#endif

	  if (SDL_MUSTLOCK(gpScreenReal))
	  {
		  SDL_UnlockSurface(gpScreenReal);
	  }

      UTIL_Delay(PAL_ScaleDelayMs(wSpeed));
   }
}

VOID
VIDEO_CrossFadeStep(
   SDL_Surface    *lpFrom,
   SDL_Surface    *lpTo,
   INT             iPhase,
   BOOL            fStep
)
/*++
  Purpose:

    Blend every sixth pixel of one 320x200 surface into another.

  Parameters:

    [IN]  lpFrom - the surface to blend from.

    [OUT] lpTo - the surface to blend into.

    [IN]  iPhase - which of the six sets of pixels to blend.

    [IN]  fStep - whether the brightness moves one step closer, or stays as it is.

  Return value:

    None.

--*/
{
   int  k;
   BYTE a, b;

   if (VIDEO_GPU_CrossFadeStep(lpFrom, lpTo, iPhase, fStep))
   {
      return;
   }

#if defined(__IOS__) && PAL_HAS_TOUCH
   if (lpTo == gpScreenBak && !fStep && iPhase == 0)
   {
      const uint8_t *fromMask = VIDEO_GetSurfaceUiMask(lpFrom, FALSE);
      uint8_t *bakMask = VIDEO_GetSurfaceUiMask(gpScreenBak, TRUE);
      if (bakMask != NULL)
      {
         if (fromMask != NULL)
            memcpy(bakMask, fromMask, 320 * 200);
         else
            memset(bakMask, 255, 320 * 200);
      }
   }
#endif

   for (k = iPhase; k < lpTo->pitch * lpTo->h; k += 6)
   {
      a = ((LPBYTE)(lpFrom->pixels))[k];
      b = ((LPBYTE)(lpTo->pixels))[k];

      if (fStep)
      {
         if ((a & 0x0F) > (b & 0x0F))
         {
            b++;
         }
         else if ((a & 0x0F) < (b & 0x0F))
         {
            b--;
         }
      }

      ((LPBYTE)(lpTo->pixels))[k] = ((a & 0xF0) | (b & 0x0F));
   }
}

VOID
VIDEO_FadeScreen(
   WORD           wSpeed
)
/*++
  Purpose:

    Fade from the backup screen buffer to the current screen buffer.
    NOTE: This will destroy the backup buffer.

  Parameters:

    [IN]  wSpeed - speed of fading (the larger value, the slower).

  Return value:

    None.

--*/
{
   int               i, j;
   DWORD             time;
   const int         rgIndex[6] = {0, 3, 1, 5, 2, 4};
   SDL_Rect          dstrect;
   short             offset = 240 - 200;
   short             screenRealHeight = gpScreenReal->h;
   short             screenRealY = 0;

   //
   // Lock surface if needed
   //
   if (SDL_MUSTLOCK(gpScreenReal))
   {
      if (SDL_LockSurface(gpScreenReal) < 0)
         return;
   }

   if (!bScaleScreen)
   {
      screenRealHeight -= offset;
      screenRealY = offset / 2;
   }

   time = SDL_GetTicks();

   wSpeed++;
   wSpeed *= 10;

#if defined(__IOS__) && PAL_HAS_TOUCH
   g_bInCrossFade = TRUE;
#endif
   for (i = 0; i < 12; i++)
   {
      for (j = 0; j < 6; j++)
      {
         PAL_ProcessEvent();
         while (!SDL_TICKS_PASSED(SDL_GetTicks(), time))
         {
            PAL_ProcessEvent();
            SDL_Delay(5);
         }
         time = SDL_GetTicks() + wSpeed;

         //
         // Blend the pixels in the 2 buffers, and put the result into the
         // backup buffer
         //
         VIDEO_CrossFadeStep(gpScreen, gpScreenBak, rgIndex[j], i > 0);

         //
         // Draw the backup buffer to the screen
         //
         if (g_wShakeTime != 0)
         {
            //
            // Shake the screen
            //
            SDL_Rect srcrect, dstrect;

            srcrect.x = 0;
            srcrect.y = 0;
            srcrect.w = 320;
            srcrect.h = 200 - g_wShakeLevel;

            dstrect.x = 0;
            dstrect.y = screenRealY;
            dstrect.w = 320 * gpScreenReal->w / gpScreen->w;
            dstrect.h = (200 - g_wShakeLevel) * screenRealHeight / gpScreen->h;

            if (g_wShakeTime & 1)
            {
               srcrect.y = g_wShakeLevel;
            }
            else
            {
               dstrect.y = (screenRealY + g_wShakeLevel) * screenRealHeight / gpScreen->h;
            }

            VIDEO_BlitToScreenReal(gpScreenBak, &srcrect, &dstrect);

            if (g_wShakeTime & 1)
            {
               dstrect.y = (screenRealY + screenRealHeight - g_wShakeLevel) * screenRealHeight / gpScreen->h;
            }
            else
            {
               dstrect.y = screenRealY;
            }

            dstrect.h = g_wShakeLevel * screenRealHeight / gpScreen->h;

            VIDEO_FillScreenRealBlack(&dstrect);
#if SDL_VERSION_ATLEAST(2, 0, 0)
            VIDEO_ShowFrame();
#else
			SDL_UpdateRect(gpScreenReal, 0, 0, gpScreenReal->w, gpScreenReal->h);
#endif
            g_wShakeTime--;
         }
         else
         {
            dstrect.x = 0;
            dstrect.y = screenRealY;
            dstrect.w = gpScreenReal->w;
            dstrect.h = screenRealHeight;

            VIDEO_BlitToScreenReal(gpScreenBak, NULL, &dstrect);
#if SDL_VERSION_ATLEAST(2, 0, 0)
            VIDEO_ShowFrame();
#else
            SDL_UpdateRect(gpScreenReal, 0, 0, gpScreenReal->w, gpScreenReal->h);
#endif
         }
      }
   }

   if (SDL_MUSTLOCK(gpScreenReal))
   {
      SDL_UnlockSurface(gpScreenReal);
   }

#if defined(__IOS__) && PAL_HAS_TOUCH
   g_bInCrossFade = FALSE;
#endif
   //
   // Draw the result buffer to the screen as the final step
   //
   VIDEO_UpdateScreen(NULL);
}

const char *
VIDEO_GetGPUDriverName(
   VOID
)
/*++
  Purpose:

    Get the name of the SDL_GPU driver drawing the game, such as "vulkan".

  Parameters:

    None.

  Return value:

    The name, or NULL if the game draws through the SDL renderer or OpenGL.

--*/
{
#if PAL_HAS_SDL_GPU
   if (gfUseGPU)
   {
      return VIDEO_GPU_GetDriverName();
   }
#endif
   return NULL;
}

void
VIDEO_SetWindowTitle(
	const char*     pszTitle
)
/*++
  Purpose:

    Set the caption of the window.

  Parameters:

    [IN]  pszTitle - the new caption of the window.

  Return value:

    None.

--*/
{
#if SDL_VERSION_ATLEAST(2,0,0)
	SDL_SetWindowTitle(gpWindow, PAL_CONVERT_UTF8(pszTitle));
#else
	SDL_WM_SetCaption(pszTitle, NULL);
#endif
}

SDL_Surface *
VIDEO_CreateCompatibleSurface(
	SDL_Surface    *pSource
)
{
	return VIDEO_CreateCompatibleSizedSurface(pSource, NULL);
}

SDL_Surface *
VIDEO_CreateCompatibleSizedSurface(
	SDL_Surface    *pSource,
	const SDL_Rect *pSize
)
/*++
  Purpose:

    Create a surface that compatible with the source surface.

  Parameters:

    [IN]  pSource   - the source surface from which attributes are taken.
    [IN]  pSize     - the size (width & height) of the created surface.

  Return value:

    None.

--*/
{
	//
	// Create the surface
	//
#if SDL_VERSION_ATLEAST(3,0,0)
    int w = pSize ? pSize->w : pSource->w;
    int h = pSize ? pSize->h : pSource->h;
    SDL_Surface* dest = gfUseGPU ? VIDEO_GPU_CreateSurface(w, h)
        : SDL_CreateSurface(w, h, SDL_GetPixelFormatDetails(pSource->format)->format);

    //
    // The GPU holds the only copy of a surface, and without one the game would go on with
    // pictures quietly missing.
    //
    if (dest == NULL && gfUseGPU)
    {
        TerminateOnError("Could not create a surface on the GPU: %s\n", SDL_GetError());
    }
#else
	SDL_Surface *dest = SDL_CreateRGBSurface(pSource->flags,
		pSize ? pSize->w : pSource->w,
		pSize ? pSize->h : pSource->h,
		pSource->format->BitsPerPixel,
		pSource->format->Rmask, pSource->format->Gmask,
		pSource->format->Bmask, pSource->format->Amask);
#endif

	if (dest)
	{
		VIDEO_UpdateSurfacePalette(dest);
	}

	return dest;
}

SDL_Surface *
VIDEO_DuplicateSurface(
	SDL_Surface    *pSource,
	const SDL_Rect *pRect
)
/*++
  Purpose:

    Duplicate the selected area from the source surface into new surface.

  Parameters:

    [IN]  pSource - the source surface.
	[IN]  pRect   - the area to be duplicated, NULL for entire surface.

  Return value:

    None.

--*/
{
	SDL_Surface* dest = VIDEO_CreateCompatibleSizedSurface(pSource, pRect);

	if (dest)
	{
		VIDEO_CopySurface(pSource, pRect, dest, NULL);
	}

	return dest;
}

void
VIDEO_UpdateSurfacePalette(
	SDL_Surface    *pSurface
)
/*++
  Purpose:

    Use the global palette to update the palette of pSurface.

  Parameters:

    [IN]  pSurface - the surface whose palette should be updated.

  Return value:

    None.

--*/
{
#if SDL_VERSION_ATLEAST(2, 0, 0)
	SDL_SetSurfacePalette(pSurface, gpPalette);
#else
	if (gpPalette != NULL)
	{
		SDL_SetPalette(pSurface, SDL_PHYSPAL | SDL_LOGPAL, gpPalette->colors, 0, 256);
	}
#endif
}

VOID
VIDEO_DrawSurfaceToScreen(
    SDL_Surface    *pSurface
)
/*++
  Purpose:

    Draw a surface directly to screen.

  Parameters:

    [IN]  pSurface - the surface which needs to be drawn to screen.

  Return value:

    None.

--*/
{
#if SDL_VERSION_ATLEAST(2, 0, 0)
   //
   // Draw the surface to screen.
   //
   if (g_bRenderPaused || g_bAppInBackground)
   {
      return;
   }
#if PAL_HAS_SDL_GPU
   if (gfUseGPU)
   {
      VIDEO_GPU_PresentSurface(pSurface);
      return;
   }
#endif
   SDL_BlitScaled(pSurface, NULL, gpScreenReal, NULL);
   VIDEO_ShowFrame();
#else
   SDL_Surface   *pCompatSurface;
   SDL_Rect       rect;

   rect.x = rect.y = 0;
   rect.w = pSurface->w;
   rect.h = pSurface->h;

   pCompatSurface = VIDEO_CreateCompatibleSizedSurface(gpScreenReal, &rect);

   //
   // First convert the surface to compatible format.
   //
   SDL_BlitSurface(pSurface, NULL, pCompatSurface, NULL);

   //
   // Draw the surface to screen.
   //
   SDL_SoftStretch(pCompatSurface, NULL, gpScreenReal, NULL);

   SDL_UpdateRect(gpScreenReal, 0, 0, gpScreenReal->w, gpScreenReal->h);
   SDL_FreeSurface(pCompatSurface);
#endif
}
