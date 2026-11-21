/* -*- mode: c; tab-width: 4; c-basic-offset: 4; c-file-style: "linux" -*- */
//
// Copyright (c) 2009-2011, Wei Mingzhi <whistler_wmz@users.sf.net>.
// Copyright (c) 2011-2026, SDLPAL development team.
// All rights reserved.
//
// This file is part of SDLPAL.
//
// SDLPAL is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License version 3
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

#include <Foundation/Foundation.h>
#include <UIKit/UIKit.h>
#include <CoreGraphics/CoreGraphics.h>
#include <QuartzCore/QuartzCore.h>
#include "common.h"
#include "palcfg.h"
#include "util.h"
#include "video.h"
#include "audio.h"
#include "touch_ui.h"

extern SDL_Renderer *gpRenderer;

static NSString * const kPrefKeyPerfHUD = @"SDLPal_ShowPerfHUD";
static int g_iCachedPerfHUD = -1;

static void ApplyMetalDeveloperHUDToLayer(CALayer *layer, BOOL enabled)
{
   if (!layer) return;
   if ([layer isKindOfClass:[CAMetalLayer class]])
   {
      if (@available(iOS 16.0, *))
      {
         NSDictionary *props = enabled
            ? @{ @"mode": @"default", @"logging": @"default" }
            : @{ @"mode": @"disabled", @"logging": @"disabled" };
         ((CAMetalLayer *)layer).developerHUDProperties = props;
      }
   }
   for (CALayer *sub in layer.sublayers)
   {
      ApplyMetalDeveloperHUDToLayer(sub, enabled);
   }
}

static void SyncMetalDeveloperHUD(BOOL enabled)
{
   setenv("MTL_HUD_ENABLED", enabled ? "1" : "0", 1);
   @autoreleasepool {
      if (gpRenderer != NULL)
      {
         CAMetalLayer *renderLayer = (__bridge CAMetalLayer *)SDL_GetRenderMetalLayer(gpRenderer);
         if (renderLayer != nil)
         {
            ApplyMetalDeveloperHUDToLayer(renderLayer, enabled);
         }
      }
      for (UIScene *scene in [UIApplication sharedApplication].connectedScenes)
      {
         if ([scene isKindOfClass:[UIWindowScene class]])
         {
            UIWindowScene *ws = (UIWindowScene *)scene;
            for (UIWindow *win in ws.windows)
            {
               ApplyMetalDeveloperHUDToLayer(win.layer, enabled);
            }
         }
      }
   }
}

BOOL
UTIL_IsPerfHUDEnabled(
   VOID
)
{
   if (g_iCachedPerfHUD < 0)
   {
      @autoreleasepool {
         g_iCachedPerfHUD = [[NSUserDefaults standardUserDefaults] boolForKey:kPrefKeyPerfHUD] ? 1 : 0;
      }
   }
   return g_iCachedPerfHUD ? TRUE : FALSE;
}

VOID
UTIL_SetPerfHUDEnabled(
   BOOL fEnabled
)
{
   g_iCachedPerfHUD = fEnabled ? 1 : 0;
   @autoreleasepool {
      [[NSUserDefaults standardUserDefaults] setBool:(fEnabled ? YES : NO) forKey:kPrefKeyPerfHUD];
      [[NSUserDefaults standardUserDefaults] synchronize];
   }
   SyncMetalDeveloperHUD(fEnabled);
}

static NSString * const kPrefKeyRightDpad = @"SDLPal_RightHandedDpad";
static int g_iCachedRightDpad = -1;

BOOL
UTIL_IsRightHandedDpad(
   VOID
)
{
   if (g_iCachedRightDpad < 0)
   {
      @autoreleasepool {
         NSUserDefaults *ud = [NSUserDefaults standardUserDefaults];
         if ([ud objectForKey:kPrefKeyRightDpad] == nil)
            g_iCachedRightDpad = 1; // Default: Right D-Pad + Left Search
         else
            g_iCachedRightDpad = [ud boolForKey:kPrefKeyRightDpad] ? 1 : 0;
      }
   }
   return g_iCachedRightDpad ? TRUE : FALSE;
}

VOID
UTIL_SetRightHandedDpad(
   BOOL fRightDpad
)
{
   g_iCachedRightDpad = fRightDpad ? 1 : 0;
   @autoreleasepool {
      [[NSUserDefaults standardUserDefaults] setBool:(fRightDpad ? YES : NO) forKey:kPrefKeyRightDpad];
      [[NSUserDefaults standardUserDefaults] synchronize];
   }
}

static NSString * const kPrefKeyEnemyStatsHUD = @"SDLPal_ShowEnemyStatsHUD";
static int g_iCachedEnemyStatsHUD = -1;

BOOL
UTIL_IsEnemyStatsHUDEnabled(
   VOID
)
{
   if (g_iCachedEnemyStatsHUD < 0)
   {
      @autoreleasepool {
         NSUserDefaults *ud = [NSUserDefaults standardUserDefaults];
         if ([ud objectForKey:kPrefKeyEnemyStatsHUD] == nil)
            g_iCachedEnemyStatsHUD = 1; // Default ON
         else
            g_iCachedEnemyStatsHUD = [ud boolForKey:kPrefKeyEnemyStatsHUD] ? 1 : 0;
      }
   }
   return g_iCachedEnemyStatsHUD ? TRUE : FALSE;
}

VOID
UTIL_SetEnemyStatsHUDEnabled(
   BOOL fEnabled
)
{
   g_iCachedEnemyStatsHUD = fEnabled ? 1 : 0;
   @autoreleasepool {
      [[NSUserDefaults standardUserDefaults] setBool:(fEnabled ? YES : NO) forKey:kPrefKeyEnemyStatsHUD];
      [[NSUserDefaults standardUserDefaults] synchronize];
   }
}

static char *runningPath = NULL;

LPCSTR
UTIL_BasePath(
   VOID
)
{
   static char buf[4096] = "";

   if (buf[0] == '\0')
   {
      @autoreleasepool {
         NSFileManager *fm = [NSFileManager defaultManager];
         NSString *bundleGameDir = [[[NSBundle mainBundle] bundlePath] stringByAppendingPathComponent:@"GameData"];
         NSString *bundleAbc = [bundleGameDir stringByAppendingPathComponent:@"abc.mkf"];
         if ([fm fileExistsAtPath:bundleAbc])
         {
            NSString *withSlash = [bundleGameDir stringByAppendingString:@"/"];
            strcpy(buf, [withSlash UTF8String]);
         }
         else
         {
            NSArray *paths = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES);
            NSString *documentsDirectory = [[paths objectAtIndex:0] stringByAppendingString:@"/"];
            strcpy(buf, [documentsDirectory UTF8String]);
         }
      }
   }

   return buf;
}

LPCSTR
UTIL_SavePath(
   VOID
)
{
   static char buf[4096] = "";

   if (buf[0] == '\0')
   {
      @autoreleasepool {
         NSArray *paths = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES);
         NSString *documentsDirectory = [[paths objectAtIndex:0] stringByAppendingString:@"/"];
         strcpy(buf, [documentsDirectory UTF8String]);
      }
   }

   return buf;
}

LPCSTR
UTIL_CachePath(
   VOID
)
{
   static char buf[4096] = "";

   if (buf[0] == '\0')
   {
      @autoreleasepool {
         NSArray *paths = NSSearchPathForDirectoriesInDomains(NSCachesDirectory, NSUserDomainMask, YES);
         NSString *documentsDirectory = [[paths objectAtIndex:0] stringByAppendingString:@"/"];
         strcpy(buf, [documentsDirectory UTF8String]);
      }
   }

   return buf;
}

BOOL
UTIL_GetScreenSize(
   DWORD *pdwScreenWidth,
   DWORD *pdwScreenHeight
)
{
   CGRect bounds = [[UIScreen mainScreen] nativeBounds];
   if (*pdwScreenWidth) *pdwScreenWidth = bounds.size.width;
   if (*pdwScreenHeight) *pdwScreenHeight = bounds.size.height;
   return (pdwScreenWidth && pdwScreenHeight && *pdwScreenWidth && *pdwScreenHeight);
}

BOOL
UTIL_IsAbsolutePath(
   LPCSTR lpszFileName
)
{
   return (lpszFileName && lpszFileName[0] == '/') ? TRUE : FALSE;
}

INT
UTIL_Platform_Init(
   int argc,
   char* argv[]
)
{
   UTIL_LogAddOutputCallback([](LOGLEVEL, const char* str, const char*)->void {
      NSLog(@"%s", str);
   }, PAL_DEFAULT_LOGLEVEL);
   gConfig.fLaunchSetting = NO;
   @autoreleasepool {
      NSUserDefaults *ud = [NSUserDefaults standardUserDefaults];
      if (![ud boolForKey:@"SDLPal_RecoverHUD_v3"]) {
         // One-time recovery from previous lockout & stuck Metal HUD
         [ud setBool:YES forKey:@"SDLPal_RecoverHUD_v3"];
         [ud setBool:NO forKey:kPrefKeyPerfHUD];
         [ud synchronize];
         g_iCachedPerfHUD = 0;
         gConfig.fUseTouchOverlay = TRUE;
         PAL_SaveConfig();
      }
   }
   BOOL wantHud = UTIL_IsPerfHUDEnabled() ? YES : NO;
   dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.25 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
      SyncMetalDeveloperHUD(wantHud);
   });
   dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(1.0 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
      SyncMetalDeveloperHUD(wantHud);
   });
   runningPath = strdup(PAL_va(0, "%s/running", UTIL_CachePath()));
   FILE *fp = fopen(runningPath, "w");
   if (fp) fclose(fp);
   return 0;
}

VOID
UTIL_Platform_Quit(
   VOID
)
{
   if (runningPath)
   {
      unlink(runningPath);
      free(runningPath);
      runningPath = NULL;
   }
}

static void UnpremultiplyRGBA(unsigned char *pixels, int count)
{
   for (int i = 0; i < count; i++)
   {
      unsigned char a = pixels[i * 4 + 3];
      if (a > 0 && a < 255)
      {
         int r = (pixels[i * 4 + 0] * 255) / a;
         int g = (pixels[i * 4 + 1] * 255) / a;
         int b = (pixels[i * 4 + 2] * 255) / a;
         pixels[i * 4 + 0] = (unsigned char)(r > 255 ? 255 : r);
         pixels[i * 4 + 1] = (unsigned char)(g > 255 ? 255 : g);
         pixels[i * 4 + 2] = (unsigned char)(b > 255 ? 255 : b);
      }
   }
}

// ============================================================================
// Shushan-Hextech (王者荣耀 × 英雄联盟海克斯 × 仙剑蜀山青玉) CoreGraphics UI Engine
// ============================================================================

static UIBezierPath *CreateChamferedPath(CGRect r, CGFloat chamfer)
{
   CGFloat c = MIN(chamfer, MIN(r.size.width, r.size.height) * 0.46);
   CGFloat x0 = CGRectGetMinX(r), x1 = CGRectGetMaxX(r);
   CGFloat y0 = CGRectGetMinY(r), y1 = CGRectGetMaxY(r);
   UIBezierPath *p = [UIBezierPath bezierPath];
   [p moveToPoint:CGPointMake(x0 + c, y0)];
   [p addLineToPoint:CGPointMake(x1 - c, y0)];
   [p addLineToPoint:CGPointMake(x1, y0 + c)];
   [p addLineToPoint:CGPointMake(x1, y1 - c)];
   [p addLineToPoint:CGPointMake(x1 - c, y1)];
   [p addLineToPoint:CGPointMake(x0 + c, y1)];
   [p addLineToPoint:CGPointMake(x0, y1 - c)];
   [p addLineToPoint:CGPointMake(x0, y0 + c)];
   [p closePath];
   return p;
}

static void FillPathWithVerticalGradient(CGContextRef ctx, UIBezierPath *path, UIColor *cTop, UIColor *cMid, UIColor *cBot)
{
   CGRect b = path.bounds;
   if (b.size.height <= 0.0) return;
   CGContextSaveGState(ctx);
   [path addClip];
   CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
   NSArray *colors = @[ (__bridge id)cTop.CGColor, (__bridge id)cMid.CGColor, (__bridge id)cBot.CGColor ];
   CGFloat locs[3] = { 0.0, 0.52, 1.0 };
   CGGradientRef grad = CGGradientCreateWithColors(cs, (__bridge CFArrayRef)colors, locs);
   CGContextDrawLinearGradient(ctx, grad,
      CGPointMake(CGRectGetMidX(b), CGRectGetMinY(b)),
      CGPointMake(CGRectGetMidX(b), CGRectGetMaxY(b)), 0);
   CGGradientRelease(grad);
   CGColorSpaceRelease(cs);
   CGContextRestoreGState(ctx);
}

static void FillPathWithRadialGradient(CGContextRef ctx, UIBezierPath *path, CGPoint center, CGFloat radius, UIColor *cInner, UIColor *cMid, UIColor *cOuter)
{
   if (radius <= 0.0) return;
   CGContextSaveGState(ctx);
   [path addClip];
   CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
   NSArray *colors = @[ (__bridge id)cInner.CGColor, (__bridge id)cMid.CGColor, (__bridge id)cOuter.CGColor ];
   CGFloat locs[3] = { 0.0, 0.58, 1.0 };
   CGGradientRef grad = CGGradientCreateWithColors(cs, (__bridge CFArrayRef)colors, locs);
   CGContextDrawRadialGradient(ctx, grad, center, 0.0, center, radius, kCGGradientDrawsAfterEndLocation);
   CGGradientRelease(grad);
   CGColorSpaceRelease(cs);
   CGContextRestoreGState(ctx);
}

static void StrokePathWithVerticalGradient(CGContextRef ctx, UIBezierPath *path, CGFloat lineWidth, UIColor *cTop, UIColor *cBot)
{
   CGRect b = CGRectInset(path.bounds, -lineWidth, -lineWidth);
   CGContextSaveGState(ctx);
   CGContextAddPath(ctx, path.CGPath);
   CGContextSetLineWidth(ctx, lineWidth);
   CGContextSetLineJoin(ctx, kCGLineJoinMiter);
   CGContextReplacePathWithStrokedPath(ctx);
   CGContextClip(ctx);
   CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
   NSArray *colors = @[ (__bridge id)cTop.CGColor, (__bridge id)cBot.CGColor ];
   CGFloat locs[2] = { 0.0, 1.0 };
   CGGradientRef grad = CGGradientCreateWithColors(cs, (__bridge CFArrayRef)colors, locs);
   CGContextDrawLinearGradient(ctx, grad,
      CGPointMake(CGRectGetMidX(b), CGRectGetMinY(b)),
      CGPointMake(CGRectGetMidX(b), CGRectGetMaxY(b)), 0);
   CGGradientRelease(grad);
   CGColorSpaceRelease(cs);
   CGContextRestoreGState(ctx);
}

static void DrawDiamondStud(CGPoint pt, CGFloat rx, CGFloat ry, UIColor *fillCol, UIColor *strokeCol)
{
   UIBezierPath *d = [UIBezierPath bezierPath];
   [d moveToPoint:CGPointMake(pt.x, pt.y - ry)];
   [d addLineToPoint:CGPointMake(pt.x + rx, pt.y)];
   [d addLineToPoint:CGPointMake(pt.x, pt.y + ry)];
   [d addLineToPoint:CGPointMake(pt.x - rx, pt.y)];
   [d closePath];
   if (fillCol) { [fillCol setFill]; [d fill]; }
   if (strokeCol) { d.lineWidth = 1.1; [strokeCol setStroke]; [d stroke]; }
}

static void DrawSegmentedMobaBar(
   CGContextRef ctx,
   CGRect       barRect,
   int          curVal,
   int          maxVal,
   UIColor     *cTop,
   UIColor     *cMid,
   UIColor     *cBot,
   UIColor     *cRim,
   BOOL         drawTicks
)
{
   if (maxVal <= 0) maxVal = 1;
   if (curVal < 0) curVal = 0;
   CGFloat ratio = (CGFloat)curVal / (CGFloat)maxVal;
   if (ratio > 1.0) ratio = 1.0;

   CGFloat ch = MIN(4.5, barRect.size.height * 0.24);
   UIBezierPath *barPath = CreateChamferedPath(barRect, ch);

   // Recessed warm espresso-umber MOBA bar trough
   [[UIColor colorWithRed:0.11 green:0.06 blue:0.03 alpha:0.94] setFill];
   [barPath fill];

   if (ratio > 0.005)
   {
      CGContextSaveGState(ctx);
      [barPath addClip];

      CGRect fillRect = CGRectMake(barRect.origin.x, barRect.origin.y, barRect.size.width * ratio, barRect.size.height);
      UIBezierPath *fillBox = [UIBezierPath bezierPathWithRect:fillRect];
      FillPathWithVerticalGradient(ctx, fillBox, cTop, cMid, cBot);

      // Top glass specular highlight strip
      CGRect shineRect = CGRectMake(barRect.origin.x, barRect.origin.y, fillRect.size.width, barRect.size.height * 0.34);
      [[UIColor colorWithWhite:1.0 alpha:0.22] setFill];
      [[UIBezierPath bezierPathWithRect:shineRect] fill];

      // Glowing meniscus edge when partially filled
      if (ratio < 0.985 && fillRect.size.width > 3.0)
      {
         CGRect edgeRect = CGRectMake(CGRectGetMaxX(fillRect) - 2.2, barRect.origin.y, 2.2, barRect.size.height);
         [[UIColor colorWithWhite:1.0 alpha:0.65] setFill];
         [[UIBezierPath bezierPathWithRect:edgeRect] fill];
      }

      // Authentic 王者荣耀 / LOL vertical HP segment ticks!
      if (drawTicks && maxVal >= 120)
      {
         int step = 250;
         if (maxVal <= 600) step = 100;
         else if (maxVal <= 2500) step = 250;
         else if (maxVal <= 10000) step = 500;
         else step = 2000;

         int majorEvery = step * 4;
         for (int hpTick = step; hpTick < maxVal; hpTick += step)
         {
            CGFloat tx = barRect.origin.x + ((CGFloat)hpTick / (CGFloat)maxVal) * barRect.size.width;
            BOOL isMajor = (hpTick % majorEvery) == 0;
            UIBezierPath *tick = [UIBezierPath bezierPath];
            [tick moveToPoint:CGPointMake(tx, barRect.origin.y)];
            [tick addLineToPoint:CGPointMake(tx, barRect.origin.y + barRect.size.height * (isMajor ? 1.0 : 0.56))];
            tick.lineWidth = isMajor ? 1.8 : 1.1;
            if (isMajor)
               [[UIColor colorWithRed:0.03 green:0.04 blue:0.06 alpha:0.82] setStroke];
            else
               [[UIColor colorWithRed:0.03 green:0.04 blue:0.06 alpha:0.52] setStroke];
            [tick stroke];
         }
      }

      CGContextRestoreGState(ctx);
   }

   barPath.lineWidth = 1.6;
   [cRim setStroke];
   [barPath stroke];
}

static BOOL DrawHudVectorIcon(CGContextRef ctx, NSString *label, CGRect box, UIColor *mainCol, UIColor *accentCol)
{
   if (!label || label.length == 0) return NO;
   CGFloat cx = CGRectGetMidX(box);
   CGFloat cy = CGRectGetMidY(box);
   CGFloat r  = MIN(box.size.width, box.size.height) * 0.44;

   CGContextSaveGState(ctx);
   CGContextSetLineCap(ctx, kCGLineCapRound);
   CGContextSetLineJoin(ctx, kCGLineJoinRound);

   if ([label hasPrefix:@"状态"])
   {
      // Hero Crown-Lotus Crest
      UIBezierPath *crown = [UIBezierPath bezierPath];
      [crown moveToPoint:CGPointMake(cx - r * 0.85, cy + r * 0.48)];
      [crown addLineToPoint:CGPointMake(cx - r * 0.95, cy - r * 0.30)];
      [crown addLineToPoint:CGPointMake(cx - r * 0.38, cy + r * 0.05)];
      [crown addLineToPoint:CGPointMake(cx, cy - r * 0.82)];
      [crown addLineToPoint:CGPointMake(cx + r * 0.38, cy + r * 0.05)];
      [crown addLineToPoint:CGPointMake(cx + r * 0.95, cy - r * 0.30)];
      [crown addLineToPoint:CGPointMake(cx + r * 0.85, cy + r * 0.48)];
      [crown closePath];
      [mainCol setFill];
      [crown fill];
      DrawDiamondStud(CGPointMake(cx, cy + r * 0.76), r * 0.48, r * 0.16, accentCol, nil);
      CGContextRestoreGState(ctx);
      return YES;
   }
   if ([label hasPrefix:@"仙术"])
   {
      // Shushan Four-Point Spirit Star + Jade Ring
      UIBezierPath *ring = [UIBezierPath bezierPathWithOvalInRect:CGRectMake(cx - r * 0.58, cy - r * 0.58, r * 1.16, r * 1.16)];
      ring.lineWidth = 2.0;
      [accentCol setStroke];
      [ring stroke];

      UIBezierPath *star = [UIBezierPath bezierPath];
      [star moveToPoint:CGPointMake(cx, cy - r)];
      [star addLineToPoint:CGPointMake(cx + r * 0.26, cy - r * 0.26)];
      [star addLineToPoint:CGPointMake(cx + r, cy)];
      [star addLineToPoint:CGPointMake(cx + r * 0.26, cy + r * 0.26)];
      [star addLineToPoint:CGPointMake(cx, cy + r)];
      [star addLineToPoint:CGPointMake(cx - r * 0.26, cy + r * 0.26)];
      [star addLineToPoint:CGPointMake(cx - r, cy)];
      [star addLineToPoint:CGPointMake(cx - r * 0.26, cy - r * 0.26)];
      [star closePath];
      [mainCol setFill];
      [star fill];
      CGContextRestoreGState(ctx);
      return YES;
   }
   if ([label hasPrefix:@"道具"] || [label hasPrefix:@"物品"])
   {
      // Immortal Spirit Gourd (仙葫芦)
      UIBezierPath *topBulb = [UIBezierPath bezierPathWithOvalInRect:CGRectMake(cx - r * 0.44, cy - r * 0.72, r * 0.88, r * 0.76)];
      UIBezierPath *botBulb = [UIBezierPath bezierPathWithOvalInRect:CGRectMake(cx - r * 0.66, cy - r * 0.08, r * 1.32, r * 1.04)];
      [mainCol setFill];
      [topBulb fill];
      [botBulb fill];
      UIBezierPath *neck = [UIBezierPath bezierPathWithRoundedRect:CGRectMake(cx - r * 0.16, cy - r * 0.98, r * 0.32, r * 0.30) cornerRadius:2.0];
      [accentCol setFill];
      [neck fill];
      UIBezierPath *belt = [UIBezierPath bezierPathWithRect:CGRectMake(cx - r * 0.46, cy - r * 0.08, r * 0.92, r * 0.18)];
      [accentCol setFill];
      [belt fill];
      CGContextRestoreGState(ctx);
      return YES;
   }
   if ([label hasPrefix:@"装备"])
   {
      // Diagonal Shushan Jian Flying Sword (御剑)
      UIBezierPath *blade = [UIBezierPath bezierPath];
      [blade moveToPoint:CGPointMake(cx + r * 0.88, cy - r * 0.88)];
      [blade addLineToPoint:CGPointMake(cx + r * 0.34, cy - r * 0.62)];
      [blade addLineToPoint:CGPointMake(cx - r * 0.38, cy + r * 0.10)];
      [blade addLineToPoint:CGPointMake(cx - r * 0.10, cy + r * 0.38)];
      [blade addLineToPoint:CGPointMake(cx + r * 0.62, cy - r * 0.34)];
      [blade closePath];
      [mainCol setFill];
      [blade fill];

      UIBezierPath *guard = [UIBezierPath bezierPath];
      [guard moveToPoint:CGPointMake(cx - r * 0.64, cy - r * 0.14)];
      [guard addLineToPoint:CGPointMake(cx + r * 0.14, cy + r * 0.64)];
      guard.lineWidth = 3.2;
      [accentCol setStroke];
      [guard stroke];

      UIBezierPath *hilt = [UIBezierPath bezierPath];
      [hilt moveToPoint:CGPointMake(cx - r * 0.26, cy + r * 0.26)];
      [hilt addLineToPoint:CGPointMake(cx - r * 0.82, cy + r * 0.82)];
      hilt.lineWidth = 3.0;
      [mainCol setStroke];
      [hilt stroke];
      CGContextRestoreGState(ctx);
      return YES;
   }
   if ([label hasPrefix:@"系统"] || [label hasPrefix:@"设置"])
   {
      // Hextech Astrolabe Gear
      for (int k = 0; k < 8; k++)
      {
         CGFloat a = k * (CGFloat)M_PI_4;
         UIBezierPath *spoke = [UIBezierPath bezierPath];
         [spoke moveToPoint:CGPointMake(cx + cos(a) * r * 0.56, cy + sin(a) * r * 0.56)];
         [spoke addLineToPoint:CGPointMake(cx + cos(a) * r * 0.94, cy + sin(a) * r * 0.94)];
         spoke.lineWidth = 3.2;
         [mainCol setStroke];
         [spoke stroke];
      }
      UIBezierPath *outer = [UIBezierPath bezierPathWithOvalInRect:CGRectMake(cx - r * 0.66, cy - r * 0.66, r * 1.32, r * 1.32)];
      outer.lineWidth = 3.0;
      [mainCol setStroke];
      [outer stroke];
      DrawDiamondStud(CGPointMake(cx, cy), r * 0.32, r * 0.32, accentCol, nil);
      CGContextRestoreGState(ctx);
      return YES;
   }
   if ([label hasPrefix:@"探查"])
   {
      // Spirit Eye (蜀山灵目)
      UIBezierPath *eye = [UIBezierPath bezierPath];
      [eye moveToPoint:CGPointMake(cx - r * 0.98, cy)];
      [eye addQuadCurveToPoint:CGPointMake(cx + r * 0.98, cy) controlPoint:CGPointMake(cx, cy - r * 0.88)];
      [eye addQuadCurveToPoint:CGPointMake(cx - r * 0.98, cy) controlPoint:CGPointMake(cx, cy + r * 0.88)];
      [eye closePath];
      eye.lineWidth = 2.6;
      [mainCol setStroke];
      [eye stroke];
      DrawDiamondStud(CGPointMake(cx, cy), r * 0.36, r * 0.38, accentCol, nil);
      CGContextRestoreGState(ctx);
      return YES;
   }
   if ([label hasPrefix:@"缩小"] || [label hasPrefix:@"放大"])
   {
      BOOL isZoomIn = [label hasPrefix:@"放大"];
      UIBezierPath *ret = CreateChamferedPath(CGRectMake(cx - r * 0.78, cy - r * 0.78, r * 1.56, r * 1.56), r * 0.38);
      ret.lineWidth = 2.5;
      [mainCol setStroke];
      [ret stroke];

      UIBezierPath *sym = [UIBezierPath bezierPath];
      [sym moveToPoint:CGPointMake(cx - r * 0.42, cy)];
      [sym addLineToPoint:CGPointMake(cx + r * 0.42, cy)];
      if (isZoomIn)
      {
         [sym moveToPoint:CGPointMake(cx, cy - r * 0.42)];
         [sym addLineToPoint:CGPointMake(cx, cy + r * 0.42)];
      }
      sym.lineWidth = 3.0;
      [accentCol setStroke];
      [sym stroke];
      CGContextRestoreGState(ctx);
      return YES;
   }
   if ([label hasPrefix:@"加速"] || [label hasPrefix:@"极速"])
   {
      // Twin Wind-Blade Chevrons (»»)
      for (int k = 0; k < 2; k++)
      {
         CGFloat ox = cx + (k == 0 ? -r * 0.44 : r * 0.16);
         UIBezierPath *chv = [UIBezierPath bezierPath];
         [chv moveToPoint:CGPointMake(ox - r * 0.28, cy - r * 0.62)];
         [chv addLineToPoint:CGPointMake(ox + r * 0.34, cy)];
         [chv addLineToPoint:CGPointMake(ox - r * 0.28, cy + r * 0.62)];
         chv.lineWidth = 3.2;
         [(k == 1 ? accentCol : mainCol) setStroke];
         [chv stroke];
      }
      CGContextRestoreGState(ctx);
      return YES;
   }
   if ([label hasPrefix:@"返回"])
   {
      UIBezierPath *arr = [UIBezierPath bezierPath];
      [arr moveToPoint:CGPointMake(cx - r * 0.18, cy - r * 0.66)];
      [arr addLineToPoint:CGPointMake(cx - r * 0.84, cy - r * 0.08)];
      [arr addLineToPoint:CGPointMake(cx - r * 0.18, cy + r * 0.50)];
      [arr moveToPoint:CGPointMake(cx - r * 0.78, cy - r * 0.08)];
      [arr addLineToPoint:CGPointMake(cx + r * 0.48, cy - r * 0.08)];
      [arr addQuadCurveToPoint:CGPointMake(cx + r * 0.48, cy + r * 0.66) controlPoint:CGPointMake(cx + r * 0.94, cy + r * 0.28)];
      [arr addLineToPoint:CGPointMake(cx - r * 0.10, cy + r * 0.66)];
      arr.lineWidth = 3.0;
      [mainCol setStroke];
      [arr stroke];
      CGContextRestoreGState(ctx);
      return YES;
   }
   if ([label hasPrefix:@"跳过"])
   {
      UIBezierPath *tri = [UIBezierPath bezierPath];
      [tri moveToPoint:CGPointMake(cx - r * 0.72, cy - r * 0.64)];
      [tri addLineToPoint:CGPointMake(cx + r * 0.26, cy)];
      [tri addLineToPoint:CGPointMake(cx - r * 0.72, cy + r * 0.64)];
      [tri closePath];
      [mainCol setFill];
      [tri fill];
      UIBezierPath *bar = [UIBezierPath bezierPathWithRect:CGRectMake(cx + r * 0.44, cy - r * 0.64, r * 0.24, r * 1.28)];
      [accentCol setFill];
      [bar fill];
      CGContextRestoreGState(ctx);
      return YES;
   }
   if ([label hasPrefix:@"围攻"] || [label hasPrefix:@"停围攻"])
   {
      // Crossed Dual Swords
      for (int s = -1; s <= 1; s += 2)
      {
         UIBezierPath *sw = [UIBezierPath bezierPath];
         [sw moveToPoint:CGPointMake(cx - s * r * 0.72, cy + r * 0.72)];
         [sw addLineToPoint:CGPointMake(cx + s * r * 0.72, cy - r * 0.72)];
         sw.lineWidth = 3.2;
         [mainCol setStroke];
         [sw stroke];
      }
      DrawDiamondStud(CGPointMake(cx, cy), r * 0.28, r * 0.28, accentCol, nil);
      CGContextRestoreGState(ctx);
      return YES;
   }
   if ([label hasPrefix:@"重复"])
   {
      UIBezierPath *arc = [UIBezierPath bezierPathWithArcCenter:CGPointMake(cx, cy) radius:r * 0.72 startAngle:-M_PI * 0.75 endAngle:M_PI * 0.65 clockwise:YES];
      arc.lineWidth = 3.0;
      [mainCol setStroke];
      [arc stroke];
      DrawDiamondStud(CGPointMake(cx, cy), r * 0.26, r * 0.26, accentCol, nil);
      CGContextRestoreGState(ctx);
      return YES;
   }
   if ([label hasPrefix:@"防御"])
   {
      UIBezierPath *sh = [UIBezierPath bezierPath];
      [sh moveToPoint:CGPointMake(cx, cy - r * 0.88)];
      [sh addLineToPoint:CGPointMake(cx + r * 0.78, cy - r * 0.52)];
      [sh addLineToPoint:CGPointMake(cx + r * 0.62, cy + r * 0.28)];
      [sh addLineToPoint:CGPointMake(cx, cy + r * 0.90)];
      [sh addLineToPoint:CGPointMake(cx - r * 0.62, cy + r * 0.28)];
      [sh addLineToPoint:CGPointMake(cx - r * 0.78, cy - r * 0.52)];
      [sh closePath];
      sh.lineWidth = 2.8;
      [mainCol setStroke];
      [sh stroke];
      DrawDiamondStud(CGPointMake(cx, cy), r * 0.30, r * 0.38, accentCol, nil);
      CGContextRestoreGState(ctx);
      return YES;
   }
   if ([label hasPrefix:@"逃跑"])
   {
      UIBezierPath *wing = [UIBezierPath bezierPath];
      [wing moveToPoint:CGPointMake(cx - r * 0.84, cy + r * 0.54)];
      [wing addQuadCurveToPoint:CGPointMake(cx + r * 0.86, cy - r * 0.72) controlPoint:CGPointMake(cx - r * 0.20, cy - r * 0.74)];
      [wing addQuadCurveToPoint:CGPointMake(cx - r * 0.84, cy + r * 0.54) controlPoint:CGPointMake(cx + r * 0.34, cy + r * 0.42)];
      [wing closePath];
      [mainCol setFill];
      [wing fill];
      CGContextRestoreGState(ctx);
      return YES;
   }

   CGContextRestoreGState(ctx);
   return NO;
}

unsigned char *
UTIL_RenderHUDButtonRGBA(
   const char *utf8Text,
   int         width,
   int         height,
   int         isCircle,
   int         isHighlighted
)
{
   if (width <= 0 || height <= 0) return NULL;
   unsigned char *pixels = (unsigned char *)calloc((size_t)width * (size_t)height, 4);
   if (!pixels) return NULL;

   @autoreleasepool {
      CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
      CGContextRef ctx = CGBitmapContextCreate(
         pixels, width, height, 8, width * 4, cs,
         kCGImageAlphaPremultipliedLast | kCGBitmapByteOrder32Big
      );
      CGColorSpaceRelease(cs);
      if (!ctx) return pixels;

      CGContextTranslateCTM(ctx, 0, height);
      CGContextScaleCTM(ctx, 1.0, -1.0);
      UIGraphicsPushContext(ctx);

      NSString *rawStr = (utf8Text && utf8Text[0]) ? [NSString stringWithUTF8String:utf8Text] : @"";
      BOOL hasSparkleTag = [rawStr hasSuffix:@"✦"];
      NSString *str = hasSparkleTag ? [rawStr substringToIndex:rawStr.length - 1] : rawStr;

      if (isCircle)
      {
         // 王者荣耀 / Wild Rift Primary Ability Medallion in Carved Sandalwood & Antique Brass
         CGFloat cx = width * 0.5;
         CGFloat cy = height * 0.5;
         CGFloat rOuter = MIN(width, height) * 0.445;

         if (isHighlighted || hasSparkleTag)
         {
            UIBezierPath *aura = [UIBezierPath bezierPathWithOvalInRect:CGRectMake(cx - rOuter - 5.0, cy - rOuter - 5.0, (rOuter + 5.0) * 2.0, (rOuter + 5.0) * 2.0)];
            [[UIColor colorWithRed:0.96 green:0.76 blue:0.32 alpha:0.28] setFill];
            [aura fill];
         }

         UIBezierPath *disc = [UIBezierPath bezierPathWithOvalInRect:CGRectMake(cx - rOuter, cy - rOuter, rOuter * 2.0, rOuter * 2.0)];
         if (isHighlighted || hasSparkleTag)
         {
            FillPathWithRadialGradient(ctx, disc, CGPointMake(cx, cy - rOuter * 0.2), rOuter * 1.15,
               [UIColor colorWithRed:0.52 green:0.30 blue:0.12 alpha:0.95],
               [UIColor colorWithRed:0.34 green:0.18 blue:0.07 alpha:0.94],
               [UIColor colorWithRed:0.19 green:0.10 blue:0.04 alpha:0.96]);
         }
         else
         {
            FillPathWithRadialGradient(ctx, disc, CGPointMake(cx, cy - rOuter * 0.2), rOuter * 1.15,
               [UIColor colorWithRed:0.38 green:0.25 blue:0.14 alpha:0.89],
               [UIColor colorWithRed:0.25 green:0.15 blue:0.08 alpha:0.88],
               [UIColor colorWithRed:0.15 green:0.09 blue:0.05 alpha:0.92]);
         }

         // Outer antique brass-gold compass bezel
         StrokePathWithVerticalGradient(ctx, disc, 4.8,
            isHighlighted ? [UIColor colorWithRed:1.00 green:0.94 blue:0.66 alpha:1.0]
                          : [UIColor colorWithRed:0.91 green:0.79 blue:0.54 alpha:0.93],
            isHighlighted ? [UIColor colorWithRed:0.86 green:0.60 blue:0.20 alpha:1.0]
                          : [UIColor colorWithRed:0.56 green:0.41 blue:0.20 alpha:0.90]);

         // Inner aged bamboo-parchment gold rune ring
         CGFloat rInnerRing = rOuter - 8.5;
         UIBezierPath *woodRing = [UIBezierPath bezierPathWithOvalInRect:CGRectMake(cx - rInnerRing, cy - rInnerRing, rInnerRing * 2.0, rInnerRing * 2.0)];
         woodRing.lineWidth = 2.0;
         [(isHighlighted ? [UIColor colorWithRed:1.00 green:0.88 blue:0.52 alpha:0.90]
                         : [UIColor colorWithRed:0.80 green:0.65 blue:0.42 alpha:0.56]) setStroke];
         [woodRing stroke];

         // 4 Cardinal Antique Brass Diamonds on the compass bezel
         UIColor *studFill = isHighlighted
            ? [UIColor colorWithRed:1.00 green:0.95 blue:0.70 alpha:1.0]
            : [UIColor colorWithRed:0.90 green:0.77 blue:0.50 alpha:0.95];
         UIColor *studRim = [UIColor colorWithRed:0.16 green:0.09 blue:0.04 alpha:0.95];
         DrawDiamondStud(CGPointMake(cx, cy - rOuter), 5.5, 7.5, studFill, studRim);
         DrawDiamondStud(CGPointMake(cx, cy + rOuter), 5.5, 7.5, studFill, studRim);
         DrawDiamondStud(CGPointMake(cx - rOuter, cy), 7.5, 5.5, studFill, studRim);
         DrawDiamondStud(CGPointMake(cx + rOuter, cy), 7.5, 5.5, studFill, studRim);

         // Upper Gilded Compass Star Emblem
         CGPoint starC = CGPointMake(cx, cy - rOuter * 0.34);
         CGFloat sr = rOuter * 0.20;
         UIBezierPath *star = [UIBezierPath bezierPath];
         [star moveToPoint:CGPointMake(starC.x, starC.y - sr)];
         [star addLineToPoint:CGPointMake(starC.x + sr * 0.28, starC.y - sr * 0.28)];
         [star addLineToPoint:CGPointMake(starC.x + sr, starC.y)];
         [star addLineToPoint:CGPointMake(starC.x + sr * 0.28, starC.y + sr * 0.28)];
         [star addLineToPoint:CGPointMake(starC.x, starC.y + sr)];
         [star addLineToPoint:CGPointMake(starC.x - sr * 0.28, starC.y + sr * 0.28)];
         [star addLineToPoint:CGPointMake(starC.x - sr, starC.y)];
         [star addLineToPoint:CGPointMake(starC.x - sr * 0.28, starC.y - sr * 0.28)];
         [star closePath];
         [(isHighlighted ? [UIColor colorWithRed:1.00 green:0.90 blue:0.52 alpha:1.0]
                         : [UIColor colorWithRed:0.90 green:0.76 blue:0.48 alpha:0.86]) setFill];
         [star fill];

         if (str.length > 0)
         {
            NSShadow *sh = [[NSShadow alloc] init];
            sh.shadowColor = [UIColor colorWithRed:0.08 green:0.04 blue:0.01 alpha:0.94];
            sh.shadowOffset = CGSizeMake(0, 2.0);
            sh.shadowBlurRadius = 3.0;
            UIFont *font = [UIFont fontWithName:@"PingFangSC-Semibold" size:height * 0.245];
            if (!font) font = [UIFont boldSystemFontOfSize:height * 0.245];
            UIColor *txtCol = isHighlighted
               ? [UIColor colorWithRed:1.00 green:0.95 blue:0.74 alpha:1.0]
               : [UIColor colorWithRed:0.96 green:0.91 blue:0.80 alpha:0.98];
            NSDictionary *attrs = @{ NSFontAttributeName: font, NSForegroundColorAttributeName: txtCol, NSShadowAttributeName: sh };
            CGSize ts = [str sizeWithAttributes:attrs];
            [str drawInRect:CGRectMake((width - ts.width) * 0.5, cy - ts.height * 0.22, ts.width, ts.height) withAttributes:attrs];
         }
      }
      else
      {
         // 45°-Chamfered Antique Sandalwood & Lacquered Rosewood Tactical Plate
         CGRect rect = CGRectMake(4.0, 4.0, width - 8.0, height - 8.0);
         CGFloat chamfer = rect.size.height * 0.25;
         UIBezierPath *outerPath = CreateChamferedPath(rect, chamfer);

         if (isHighlighted || hasSparkleTag)
         {
            FillPathWithVerticalGradient(ctx, outerPath,
               [UIColor colorWithRed:0.48 green:0.26 blue:0.10 alpha:0.94],
               [UIColor colorWithRed:0.34 green:0.17 blue:0.06 alpha:0.94],
               [UIColor colorWithRed:0.21 green:0.10 blue:0.04 alpha:0.96]);
         }
         else
         {
            FillPathWithVerticalGradient(ctx, outerPath,
               [UIColor colorWithRed:0.33 green:0.22 blue:0.13 alpha:0.90],
               [UIColor colorWithRed:0.23 green:0.14 blue:0.08 alpha:0.91],
               [UIColor colorWithRed:0.15 green:0.09 blue:0.05 alpha:0.93]);
         }

         // Warm polished sandalwood sheen inside top of plate
         CGContextSaveGState(ctx);
         [outerPath addClip];
         UIBezierPath *topSheen = [UIBezierPath bezierPathWithRect:CGRectMake(rect.origin.x, rect.origin.y, rect.size.width, rect.size.height * 0.36)];
         [[UIColor colorWithRed:0.90 green:0.74 blue:0.48 alpha:(isHighlighted ? 0.16 : 0.09)] setFill];
         [topSheen fill];
         CGContextRestoreGState(ctx);

         // Outer antique brass-gold gradient bevel
         StrokePathWithVerticalGradient(ctx, outerPath, 3.2,
            isHighlighted ? [UIColor colorWithRed:1.00 green:0.93 blue:0.62 alpha:1.0]
                          : [UIColor colorWithRed:0.90 green:0.78 blue:0.53 alpha:0.93],
            isHighlighted ? [UIColor colorWithRed:0.84 green:0.58 blue:0.18 alpha:1.0]
                          : [UIColor colorWithRed:0.56 green:0.41 blue:0.20 alpha:0.89]);

         // Inner warm aged bamboo-ochre hairline
         CGRect innerRect = CGRectInset(rect, 5.0, 5.0);
         UIBezierPath *innerPath = CreateChamferedPath(innerRect, MAX(3.0, chamfer - 3.5));
         innerPath.lineWidth = 1.4;
         [(isHighlighted ? [UIColor colorWithRed:0.98 green:0.86 blue:0.52 alpha:0.86]
                         : [UIColor colorWithRed:0.78 green:0.63 blue:0.40 alpha:0.54]) setStroke];
         [innerPath stroke];

         // Side antique brass diamond rivets
         UIColor *studCol = isHighlighted
            ? [UIColor colorWithRed:1.00 green:0.92 blue:0.60 alpha:1.0]
            : [UIColor colorWithRed:0.90 green:0.77 blue:0.50 alpha:0.92];
         UIColor *studRim = [UIColor colorWithRed:0.15 green:0.08 blue:0.04 alpha:0.95];
         DrawDiamondStud(CGPointMake(CGRectGetMinX(rect) + 2.0, CGRectGetMidY(rect)), 4.2, 5.5, studCol, studRim);
         DrawDiamondStud(CGPointMake(CGRectGetMaxX(rect) - 2.0, CGRectGetMidY(rect)), 4.2, 5.5, studCol, studRim);

         if (str.length > 0)
         {
            UIColor *iconMain = isHighlighted
               ? [UIColor colorWithRed:1.00 green:0.94 blue:0.68 alpha:1.0]
               : [UIColor colorWithRed:0.93 green:0.83 blue:0.60 alpha:0.96];
            UIColor *iconAcc = isHighlighted
               ? [UIColor colorWithRed:1.00 green:0.74 blue:0.32 alpha:1.0]
               : [UIColor colorWithRed:0.86 green:0.64 blue:0.32 alpha:0.95];

            CGFloat iconSize = height * 0.36;
            CGFloat fontSize = (str.length >= 5) ? (height * 0.34) : (height * 0.40);
            NSShadow *shadow = [[NSShadow alloc] init];
            shadow.shadowColor = [UIColor colorWithRed:0.08 green:0.04 blue:0.01 alpha:0.92];
            shadow.shadowOffset = CGSizeMake(0, 1.8);
            shadow.shadowBlurRadius = 2.2;

            UIColor *textColor = isHighlighted
               ? [UIColor colorWithRed:1.00 green:0.95 blue:0.76 alpha:1.0]
               : [UIColor colorWithRed:0.96 green:0.91 blue:0.80 alpha:0.98];

            CGFloat maxContentW = width - 42.0;
            CGFloat iconGap = 7.0;
            NSDictionary *attrs = nil;
            CGSize textSize = CGSizeZero;
            while (fontSize > 16.0)
            {
               UIFont *font = [UIFont fontWithName:@"PingFangSC-Semibold" size:fontSize];
               if (!font) font = [UIFont boldSystemFontOfSize:fontSize];
               attrs = @{
                  NSFontAttributeName: font,
                  NSForegroundColorAttributeName: textColor,
                  NSShadowAttributeName: shadow
               };
               textSize = [str sizeWithAttributes:attrs];
               if (textSize.width + iconSize + iconGap <= maxContentW) break;
               fontSize -= 1.5;
            }

            CGFloat totalW = iconSize + iconGap + textSize.width;
            CGFloat startX = (width - totalW) * 0.5;
            CGRect iconBox = CGRectMake(startX, (height - iconSize) * 0.5, iconSize, iconSize);
            BOOL drewIcon = DrawHudVectorIcon(ctx, str, iconBox, iconMain, iconAcc);
            CGFloat textX = drewIcon ? (startX + iconSize + iconGap) : ((width - textSize.width) * 0.5);
            [str drawInRect:CGRectMake(textX, (height - textSize.height) * 0.5, textSize.width, textSize.height) withAttributes:attrs];
         }
      }

      UIGraphicsPopContext();
      CGContextRelease(ctx);
   }

   UnpremultiplyRGBA(pixels, width * height);
   return pixels;
}

unsigned char *
UTIL_RenderHUDDpadRGBA(
   int size,
   int activeDir
)
{
   if (size <= 0) return NULL;
   unsigned char *pixels = (unsigned char *)calloc((size_t)size * (size_t)size, 4);
   if (!pixels) return NULL;

   @autoreleasepool {
      CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
      CGContextRef ctx = CGBitmapContextCreate(
         pixels, size, size, 8, size * 4, cs,
         kCGImageAlphaPremultipliedLast | kCGBitmapByteOrder32Big
      );
      CGColorSpaceRelease(cs);
      if (!ctx) return pixels;

      CGContextTranslateCTM(ctx, 0, size);
      CGContextScaleCTM(ctx, 1.0, -1.0);
      UIGraphicsPushContext(ctx);

      CGFloat c = size * 0.5;
      CGFloat rOuter = size * 0.455;
      CGFloat rMid   = size * 0.290;
      CGFloat rHub   = size * 0.110;

      // 1. Carved Sandalwood & Rosewood Luopan Compass Disc
      CGRect outerRect = CGRectMake(c - rOuter, c - rOuter, rOuter * 2.0, rOuter * 2.0);
      UIBezierPath *disc = [UIBezierPath bezierPathWithOvalInRect:outerRect];
      FillPathWithRadialGradient(ctx, disc, CGPointMake(c, c), rOuter,
         [UIColor colorWithRed:0.34 green:0.22 blue:0.12 alpha:0.80],
         [UIColor colorWithRed:0.22 green:0.13 blue:0.07 alpha:0.79],
         [UIColor colorWithRed:0.13 green:0.07 blue:0.03 alpha:0.86]);

      // 2. Engraved concentric warm bamboo-gold compass ring
      UIBezierPath *astroRing = [UIBezierPath bezierPathWithOvalInRect:CGRectMake(c - rMid, c - rMid, rMid * 2.0, rMid * 2.0)];
      astroRing.lineWidth = 1.4;
      [[UIColor colorWithRed:0.80 green:0.65 blue:0.42 alpha:0.30] setStroke];
      [astroRing stroke];

      // 3. Active Isometric Quadrant Warm Cinnabar-Amber Flare
      struct SectorDef {
         int dir;
         CGFloat a0, a1;
         CGFloat dx, dy;
      } sectors[4] = {
         { kDirWest,  (CGFloat)M_PI,         (CGFloat)(1.5 * M_PI), -0.7071f, -0.7071f },
         { kDirNorth, (CGFloat)(1.5 * M_PI), (CGFloat)(2.0 * M_PI),  0.7071f, -0.7071f },
         { kDirSouth, (CGFloat)M_PI_2,       (CGFloat)M_PI,         -0.7071f,  0.7071f },
         { kDirEast,  0.0f,                  (CGFloat)M_PI_2,        0.7071f,  0.7071f }
      };

      for (int i = 0; i < 4; i++)
      {
         if (activeDir == sectors[i].dir)
         {
            UIBezierPath *wedge = [UIBezierPath bezierPath];
            [wedge addArcWithCenter:CGPointMake(c, c) radius:rOuter - 3.0 startAngle:sectors[i].a0 endAngle:sectors[i].a1 clockwise:YES];
            [wedge addArcWithCenter:CGPointMake(c, c) radius:rHub startAngle:sectors[i].a1 endAngle:sectors[i].a0 clockwise:NO];
            [wedge closePath];
            FillPathWithRadialGradient(ctx, wedge, CGPointMake(c, c), rOuter,
               [UIColor colorWithRed:0.72 green:0.44 blue:0.16 alpha:0.46],
               [UIColor colorWithRed:0.88 green:0.62 blue:0.22 alpha:0.52],
               [UIColor colorWithRed:0.98 green:0.80 blue:0.36 alpha:0.66]);
         }
      }

      // 4. Quadrant divider crosshairs
      UIBezierPath *dividers = [UIBezierPath bezierPath];
      [dividers moveToPoint:CGPointMake(c - rOuter + 5.0, c)];
      [dividers addLineToPoint:CGPointMake(c - rHub, c)];
      [dividers moveToPoint:CGPointMake(c + rHub, c)];
      [dividers addLineToPoint:CGPointMake(c + rOuter - 5.0, c)];
      [dividers moveToPoint:CGPointMake(c, c - rOuter + 5.0)];
      [dividers addLineToPoint:CGPointMake(c, c - rHub)];
      [dividers moveToPoint:CGPointMake(c, c + rHub)];
      [dividers addLineToPoint:CGPointMake(c, c + rOuter - 5.0)];
      dividers.lineWidth = 1.8;
      [[UIColor colorWithRed:0.86 green:0.72 blue:0.46 alpha:0.48] setStroke];
      [dividers stroke];

      // 5. Dual Outer Antique Brass + Inner Sandalwood-Gold Bezel + 4 Cardinal Clasps
      StrokePathWithVerticalGradient(ctx, disc, 4.0,
         [UIColor colorWithRed:0.92 green:0.80 blue:0.54 alpha:0.92],
         [UIColor colorWithRed:0.56 green:0.40 blue:0.18 alpha:0.90]);

      CGFloat rInnerRim = rOuter - 6.5;
      UIBezierPath *innerRim = [UIBezierPath bezierPathWithOvalInRect:CGRectMake(c - rInnerRim, c - rInnerRim, rInnerRim * 2.0, rInnerRim * 2.0)];
      innerRim.lineWidth = 1.4;
      [[UIColor colorWithRed:0.78 green:0.63 blue:0.40 alpha:0.50] setStroke];
      [innerRim stroke];

      UIColor *claspFill = [UIColor colorWithRed:0.94 green:0.82 blue:0.54 alpha:0.94];
      UIColor *claspRim  = [UIColor colorWithRed:0.16 green:0.09 blue:0.04 alpha:0.95];
      DrawDiamondStud(CGPointMake(c, c - rOuter), 5.5, 8.0, claspFill, claspRim);
      DrawDiamondStud(CGPointMake(c, c + rOuter), 5.5, 8.0, claspFill, claspRim);
      DrawDiamondStud(CGPointMake(c - rOuter, c), 8.0, 5.5, claspFill, claspRim);
      DrawDiamondStud(CGPointMake(c + rOuter, c), 8.0, 5.5, claspFill, claspRim);

      // 6. Sculpted Winged Directional Chevrons
      CGFloat tipDist  = size * 0.355;
      CGFloat baseDist = size * 0.235;
      CGFloat wing     = size * 0.074;
      for (int i = 0; i < 4; i++)
      {
         BOOL lit = (activeDir == sectors[i].dir);
         CGFloat dx = sectors[i].dx, dy = sectors[i].dy;
         CGFloat px = -dy, py = dx;
         UIBezierPath *tri = [UIBezierPath bezierPath];
         [tri moveToPoint:CGPointMake(c + dx * tipDist, c + dy * tipDist)];
         [tri addLineToPoint:CGPointMake(c + dx * baseDist + px * wing, c + dy * baseDist + py * wing)];
         [tri addLineToPoint:CGPointMake(c + dx * (baseDist + size * 0.036), c + dy * (baseDist + size * 0.036))];
         [tri addLineToPoint:CGPointMake(c + dx * baseDist - px * wing, c + dy * baseDist - py * wing)];
         [tri closePath];
         if (lit)
         {
            [[UIColor colorWithRed:1.00 green:0.96 blue:0.78 alpha:1.0] setFill];
            [tri fill];
            tri.lineWidth = 1.6;
            [[UIColor colorWithRed:0.98 green:0.76 blue:0.32 alpha:0.95] setStroke];
            [tri stroke];
         }
         else
         {
            [[UIColor colorWithRed:0.92 green:0.82 blue:0.58 alpha:0.88] setFill];
            [tri fill];
         }
      }

      // 7. Central Carved Rosewood & Warm Amber-Gold Core Hub
      UIBezierPath *hub = [UIBezierPath bezierPathWithOvalInRect:CGRectMake(c - rHub, c - rHub, rHub * 2.0, rHub * 2.0)];
      [[UIColor colorWithRed:0.20 green:0.11 blue:0.05 alpha:0.96] setFill];
      [hub fill];
      hub.lineWidth = 2.6;
      [[UIColor colorWithRed:0.90 green:0.77 blue:0.48 alpha:0.90] setStroke];
      [hub stroke];

      DrawDiamondStud(CGPointMake(c, c), rHub * 0.52, rHub * 0.52,
         (activeDir >= 0) ? [UIColor colorWithRed:1.00 green:0.88 blue:0.46 alpha:1.0]
                          : [UIColor colorWithRed:0.84 green:0.64 blue:0.32 alpha:0.90],
         [UIColor colorWithRed:0.96 green:0.86 blue:0.56 alpha:0.85]);

      UIGraphicsPopContext();
      CGContextRelease(ctx);
   }

   UnpremultiplyRGBA(pixels, size * size);
   return pixels;
}

unsigned char *
UTIL_RenderPerfHUDBadgeRGBA(
   const char *utf8Text,
   int         width,
   int         height
)
{
   if (width <= 0 || height <= 0) return NULL;
   size_t totalBytes = (size_t)width * (size_t)height * 4;
   unsigned char *pixels = (unsigned char *)calloc(1, totalBytes);
   if (!pixels) return NULL;

   @autoreleasepool {
      CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
      CGContextRef ctx = CGBitmapContextCreate(
         pixels, width, height, 8, width * 4, cs,
         kCGImageAlphaPremultipliedLast | kCGBitmapByteOrder32Big
      );
      CGColorSpaceRelease(cs);
      if (!ctx) return pixels;

      CGContextTranslateCTM(ctx, 0, height);
      CGContextScaleCTM(ctx, 1.0, -1.0);
      UIGraphicsPushContext(ctx);

      CGRect rect = CGRectMake(2.0, 2.0, width - 4.0, height - 4.0);
      UIBezierPath *bg = CreateChamferedPath(rect, (height - 4.0) * 0.28);
      [[UIColor colorWithRed:0.22 green:0.13 blue:0.07 alpha:0.88] setFill];
      [bg fill];
      bg.lineWidth = 2.0;
      [[UIColor colorWithRed:0.86 green:0.72 blue:0.44 alpha:0.85] setStroke];
      [bg stroke];

      if (utf8Text && utf8Text[0] != '\0')
      {
         NSString *str = [NSString stringWithUTF8String:utf8Text];
         if (str)
         {
            UIFont *font = [UIFont monospacedDigitSystemFontOfSize:height * 0.42 weight:UIFontWeightSemibold];
            NSDictionary *attrs = @{
               NSFontAttributeName: font,
               NSForegroundColorAttributeName: [UIColor colorWithRed:0.98 green:0.90 blue:0.68 alpha:1.0]
            };
            CGSize ts = [str sizeWithAttributes:attrs];
            CGRect tr = CGRectMake((width - ts.width) * 0.5, (height - ts.height) * 0.5, ts.width, ts.height);
            [str drawInRect:tr withAttributes:attrs];
         }
      }

      UIGraphicsPopContext();
      CGContextRelease(ctx);
   }

   UnpremultiplyRGBA(pixels, width * height);
   return pixels;
}

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
)
{
   if (width <= 0 || height <= 0) return NULL;
   unsigned char *pixels = (unsigned char *)calloc((size_t)width * (size_t)height, 4);
   if (!pixels) return NULL;

   @autoreleasepool {
      CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
      CGContextRef ctx = CGBitmapContextCreate(
         pixels, width, height, 8, width * 4, cs,
         kCGImageAlphaPremultipliedLast | kCGBitmapByteOrder32Big
      );
      CGColorSpaceRelease(cs);
      if (!ctx) return pixels;

      CGContextTranslateCTM(ctx, 0, height);
      CGContextScaleCTM(ctx, 1.0, -1.0);
      UIGraphicsPushContext(ctx);

      CGRect cardRect = CGRectMake(3.0, 3.0, width - 6.0, height - 6.0);
      UIBezierPath *bg = CreateChamferedPath(cardRect, 13.0);
      if (isTargeted)
      {
         FillPathWithVerticalGradient(ctx, bg,
            [UIColor colorWithRed:0.42 green:0.25 blue:0.10 alpha:0.94],
            [UIColor colorWithRed:0.29 green:0.16 blue:0.07 alpha:0.94],
            [UIColor colorWithRed:0.18 green:0.09 blue:0.04 alpha:0.96]);
      }
      else
      {
         // Deep Espresso-Sandalwood (matching original PAL98 PlayerInfoBox #301408)
         FillPathWithVerticalGradient(ctx, bg,
            [UIColor colorWithRed:0.26 green:0.16 blue:0.09 alpha:0.90],
            [UIColor colorWithRed:0.18 green:0.10 blue:0.05 alpha:0.91],
            [UIColor colorWithRed:0.11 green:0.06 blue:0.03 alpha:0.93]);
      }

      StrokePathWithVerticalGradient(ctx, bg, isTargeted ? 3.2 : 2.0,
         isTargeted ? [UIColor colorWithRed:1.00 green:0.94 blue:0.58 alpha:1.0]
                    : [UIColor colorWithRed:0.88 green:0.76 blue:0.50 alpha:0.88],
         isTargeted ? [UIColor colorWithRed:0.86 green:0.60 blue:0.18 alpha:1.0]
                    : [UIColor colorWithRed:0.54 green:0.39 blue:0.19 alpha:0.86]);

      UIBezierPath *innerTrim = CreateChamferedPath(CGRectInset(cardRect, 4.0, 4.0), 10.0);
      innerTrim.lineWidth = 1.1;
      [(isTargeted ? [UIColor colorWithRed:1.00 green:0.88 blue:0.52 alpha:0.80]
                   : [UIColor colorWithRed:0.76 green:0.62 blue:0.39 alpha:0.42]) setStroke];
      [innerTrim stroke];

      NSShadow *shadow = [[NSShadow alloc] init];
      shadow.shadowColor = [UIColor colorWithRed:0.06 green:0.03 blue:0.01 alpha:0.94];
      shadow.shadowOffset = CGSizeMake(0, 1.3);
      shadow.shadowBlurRadius = 1.8;

      auto drawCenteredInBox = ^(NSString *text, UIColor *color, CGRect box, CGFloat baseSize) {
         CGFloat sz = baseSize;
         NSDictionary *attrs = nil;
         CGSize ts = CGSizeZero;
         while (sz > 10.5) {
            UIFont *f = [UIFont fontWithName:@"PingFangSC-Semibold" size:sz];
            if (!f) f = [UIFont boldSystemFontOfSize:sz];
            attrs = @{
               NSFontAttributeName: f,
               NSForegroundColorAttributeName: color,
               NSShadowAttributeName: shadow
            };
            ts = [text sizeWithAttributes:attrs];
            if (ts.width <= box.size.width - 3.0) break;
            sz -= 1.0;
         }
         CGRect tr = CGRectMake(
            box.origin.x + (box.size.width - ts.width) * 0.5,
            box.origin.y + (box.size.height - ts.height) * 0.5,
            ts.width,
            ts.height
         );
         [text drawInRect:tr withAttributes:attrs];
      };

      // =====================================================================
      // Band 1 (Top): Rosewood Lv.N Shield + Segmented Cinnabar MOBA HP Bar
      // =====================================================================
      CGFloat band1Y = 9.0;
      CGFloat band1H = height * 0.255;
      CGFloat lvW    = width * 0.195;
      CGRect  lvRect = CGRectMake(10.0, band1Y, lvW, band1H);
      UIBezierPath *lvBg = CreateChamferedPath(lvRect, 6.0);
      [[UIColor colorWithRed:0.32 green:0.18 blue:0.09 alpha:0.96] setFill];
      [lvBg fill];
      lvBg.lineWidth = 1.5;
      [[UIColor colorWithRed:0.88 green:0.74 blue:0.46 alpha:0.88] setStroke];
      [lvBg stroke];
      NSString *sLv = [NSString stringWithFormat:@"Lv.%d", (level > 0 ? level : 1)];
      drawCenteredInBox(sLv, [UIColor colorWithRed:1.00 green:0.90 blue:0.60 alpha:1.0], lvRect, band1H * 0.62);

      int effMaxHP = (maxHP >= hp && maxHP > 0) ? maxHP : (hp > 0 ? hp : 1);
      CGRect hpBarRect = CGRectMake(CGRectGetMaxX(lvRect) + 6.0, band1Y + 1.0, width - CGRectGetMaxX(lvRect) - 16.0, band1H - 2.0);
      DrawSegmentedMobaBar(ctx, hpBarRect, hp, effMaxHP,
         [UIColor colorWithRed:0.96 green:0.34 blue:0.26 alpha:1.0],
         [UIColor colorWithRed:0.78 green:0.14 blue:0.11 alpha:1.0],
         [UIColor colorWithRed:0.48 green:0.07 blue:0.05 alpha:1.0],
         isTargeted ? [UIColor colorWithRed:1.00 green:0.90 blue:0.48 alpha:0.98]
                    : [UIColor colorWithRed:0.84 green:0.68 blue:0.42 alpha:0.86],
         YES);

      NSString *sHP = (hp < effMaxHP)
         ? [NSString stringWithFormat:@"♥ %d / %d", hp, effMaxHP]
         : [NSString stringWithFormat:@"♥ %d", hp];
      drawCenteredInBox(sHP, [UIColor colorWithRed:1.00 green:0.96 blue:0.90 alpha:1.0], hpBarRect, band1H * 0.64);

      // =====================================================================
      // Band 2 (Middle): 5 Chamfered Bamboo-Slip Elemental Chips (风 雷 水 火 土)
      // =====================================================================
      NSArray<NSString *> *elemNames = @[ @"风", @"雷", @"水", @"火", @"土" ];
      int effMult[5];
      int maxEff = -99999;
      int minEff =  99999;
      for (int e = 0; e < 5; e++) {
         int bf = 10 + (bfEffect ? bfEffect[e] : 0);
         if (bf < 0) bf = 0;
         int er = elemRes ? elemRes[e] : 5;
         int m = (10 - er) * bf;
         if (m < 0) m = 0;
         effMult[e] = m;
         if (m > maxEff) maxEff = m;
         if (m < minEff) minEff = m;
      }

      CGFloat band2Y = band1Y + band1H + 6.0;
      CGFloat band2H = height * 0.285;
      CGFloat chipGap = 4.5;
      CGFloat chipW = (width - 20.0 - chipGap * 4.0) / 5.0;

      for (int e = 0; e < 5; e++) {
         int er = elemRes ? elemRes[e] : 5;
         int m = effMult[e];
         NSString *sElem = nil;
         UIColor *chipFill = nil;
         UIColor *chipRim  = nil;
         UIColor *chipTxt  = nil;

         if (m <= 0 || er >= 10) {
            sElem    = [NSString stringWithFormat:@"%@免", elemNames[e]];
            chipFill = [UIColor colorWithRed:0.32 green:0.09 blue:0.07 alpha:0.93];
            chipRim  = [UIColor colorWithRed:0.96 green:0.38 blue:0.32 alpha:0.88];
            chipTxt  = [UIColor colorWithRed:1.00 green:0.60 blue:0.54 alpha:1.00];
         } else if (m == maxEff && (maxEff > minEff || maxEff >= 60)) {
            sElem    = [NSString stringWithFormat:@"★%@%d", elemNames[e], er];
            chipFill = [UIColor colorWithRed:0.13 green:0.28 blue:0.12 alpha:0.95];
            chipRim  = [UIColor colorWithRed:0.46 green:0.92 blue:0.48 alpha:0.94];
            chipTxt  = [UIColor colorWithRed:0.64 green:1.00 blue:0.64 alpha:1.00];
         } else if (m >= 60) {
            sElem    = [NSString stringWithFormat:@"%@%d", elemNames[e], er];
            chipFill = [UIColor colorWithRed:0.22 green:0.24 blue:0.11 alpha:0.91];
            chipRim  = [UIColor colorWithRed:0.70 green:0.82 blue:0.42 alpha:0.74];
            chipTxt  = [UIColor colorWithRed:0.88 green:0.98 blue:0.68 alpha:1.00];
         } else if (m >= 40) {
            sElem    = [NSString stringWithFormat:@"%@%d", elemNames[e], er];
            chipFill = [UIColor colorWithRed:0.25 green:0.16 blue:0.09 alpha:0.90];
            chipRim  = [UIColor colorWithRed:0.72 green:0.58 blue:0.38 alpha:0.56];
            chipTxt  = [UIColor colorWithRed:0.95 green:0.90 blue:0.78 alpha:1.00];
         } else {
            sElem    = [NSString stringWithFormat:@"%@%d", elemNames[e], er];
            chipFill = [UIColor colorWithRed:0.16 green:0.10 blue:0.06 alpha:0.88];
            chipRim  = [UIColor colorWithRed:0.48 green:0.38 blue:0.26 alpha:0.46];
            chipTxt  = [UIColor colorWithRed:0.68 green:0.62 blue:0.52 alpha:0.95];
         }

         CGRect cRect = CGRectMake(10.0 + (chipW + chipGap) * e, band2Y, chipW, band2H);
         UIBezierPath *cp = CreateChamferedPath(cRect, 5.5);
         [chipFill setFill];
         [cp fill];
         cp.lineWidth = 1.3;
         [chipRim setStroke];
         [cp stroke];
         drawCenteredInBox(sElem, chipTxt, cRect, band2H * 0.60);
      }

      // =====================================================================
      // Band 3 (Bottom): [物 | 巫 | 毒] Sandalwood Strip + Parchment-Gold Loot
      // =====================================================================
      UIColor *colBest   = [UIColor colorWithRed:0.54 green:0.98 blue:0.58 alpha:1.0];
      UIColor *colGold   = [UIColor colorWithRed:1.00 green:0.88 blue:0.46 alpha:1.0];
      UIColor *colDone   = [UIColor colorWithRed:0.78 green:0.74 blue:0.58 alpha:0.96];
      UIColor *colNormal = [UIColor colorWithRed:0.94 green:0.89 blue:0.78 alpha:1.0];
      UIColor *colDim    = [UIColor colorWithRed:0.66 green:0.60 blue:0.50 alpha:0.95];
      UIColor *colImmune = [UIColor colorWithRed:1.00 green:0.52 blue:0.46 alpha:1.0];

      NSString *sPhys = nil; UIColor *cPhys = colNormal;
      if (physRes >= 10)      { sPhys = @"物免"; cPhys = colImmune; }
      else if (physRes <= 1)  { sPhys = [NSString stringWithFormat:@"★物%d", (physRes <= 0 ? 1 : physRes)]; cPhys = colBest; }
      else if (physRes <= 3)  { sPhys = [NSString stringWithFormat:@"物%d", physRes]; cPhys = colNormal; }
      else                    { sPhys = [NSString stringWithFormat:@"物%d", physRes]; cPhys = colDim; }

      NSString *sSorc = nil; UIColor *cSorc = colNormal;
      if (sorcRes >= 10)      { sSorc = @"巫免"; cSorc = colImmune; }
      else if (sorcRes <= 3)  { sSorc = [NSString stringWithFormat:@"★巫%d", sorcRes]; cSorc = colBest; }
      else if (sorcRes <= 6)  { sSorc = [NSString stringWithFormat:@"巫%d", sorcRes]; cSorc = colGold; }
      else                    { sSorc = [NSString stringWithFormat:@"巫%d", sorcRes]; cSorc = colDim; }

      NSString *sPois = nil; UIColor *cPois = colNormal;
      if (poisRes >= 10)      { sPois = @"毒免"; cPois = colImmune; }
      else if (poisRes <= 3)  { sPois = [NSString stringWithFormat:@"★毒%d", poisRes]; cPois = colBest; }
      else if (poisRes <= 6)  { sPois = [NSString stringWithFormat:@"毒%d", poisRes]; cPois = colNormal; }
      else                    { sPois = [NSString stringWithFormat:@"毒%d", poisRes]; cPois = colDim; }

      NSString *itemNameStr = nil;
      if (stealItemName != NULL && stealItemName[0] != 0) {
         size_t wlen = wcslen(stealItemName);
         NSString *raw = [[NSString alloc] initWithBytes:stealItemName
                                                  length:wlen * sizeof(wchar_t)
                                                encoding:NSUTF32LittleEndianStringEncoding];
         itemNameStr = [raw stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
      }

      NSString *sLoot = nil;
      UIColor  *cLoot = colGold;
      int safeCash = (dropCash > 0 ? dropCash : 0);
      if (itemNameStr.length > 0 && stealCount > 0) {
         sLoot = [NSString stringWithFormat:@"偷:%@×%d · 赏:%d文", itemNameStr, stealCount, safeCash];
         cLoot = colGold;
      } else if (itemNameStr.length == 0 && stealCount >= 2) {
         sLoot = [NSString stringWithFormat:@"偷:%d文 · 赏:%d文", stealCount, safeCash];
         cLoot = colGold;
      } else if (hadInitialSteal) {
         sLoot = [NSString stringWithFormat:@"偷:已空 · 赏:%d文", safeCash];
         cLoot = colDone;
      } else {
         sLoot = [NSString stringWithFormat:@"偷:无 · 赏:%d文", safeCash];
         cLoot = (safeCash > 0) ? [UIColor colorWithRed:0.95 green:0.87 blue:0.64 alpha:1.0] : colDim;
      }

      CGFloat band3Y = band2Y + band2H + 5.5;
      CGFloat band3H = height - band3Y - 8.5;
      CGFloat leftW  = (width - 20.0) * 0.43;
      CGFloat rightW = (width - 20.0) - leftW - 5.0;

      CGRect subStrip = CGRectMake(10.0, band3Y, leftW, band3H);
      UIBezierPath *subBg = CreateChamferedPath(subStrip, 5.0);
      [[UIColor colorWithRed:0.20 green:0.12 blue:0.06 alpha:0.92] setFill];
      [subBg fill];
      subBg.lineWidth = 1.1;
      [[UIColor colorWithRed:0.74 green:0.58 blue:0.38 alpha:0.48] setStroke];
      [subBg stroke];

      CGFloat subColW = leftW / 3.0;
      drawCenteredInBox(sPhys, cPhys, CGRectMake(10.0, band3Y, subColW, band3H), band3H * 0.62);
      drawCenteredInBox(sSorc, cSorc, CGRectMake(10.0 + subColW, band3Y, subColW, band3H), band3H * 0.62);
      drawCenteredInBox(sPois, cPois, CGRectMake(10.0 + subColW * 2.0, band3Y, subColW, band3H), band3H * 0.62);

      CGRect lootStrip = CGRectMake(10.0 + leftW + 5.0, band3Y, rightW, band3H);
      UIBezierPath *lootBg = CreateChamferedPath(lootStrip, 5.0);
      [[UIColor colorWithRed:0.28 green:0.18 blue:0.08 alpha:0.92] setFill];
      [lootBg fill];
      lootBg.lineWidth = 1.1;
      [[UIColor colorWithRed:0.90 green:0.74 blue:0.42 alpha:0.58] setStroke];
      [lootBg stroke];
      drawCenteredInBox(sLoot, cLoot, lootStrip, band3H * 0.60);

      UIGraphicsPopContext();
      CGContextRelease(ctx);
   }

   UnpremultiplyRGBA(pixels, width * height);
   return pixels;
}

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
)
{
   if (width <= 0 || height <= 0) return NULL;
   unsigned char *pixels = (unsigned char *)calloc((size_t)width * (size_t)height, 4);
   if (!pixels) return NULL;

   @autoreleasepool {
      CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
      CGContextRef ctx = CGBitmapContextCreate(
         pixels, width, height, 8, width * 4, cs,
         kCGImageAlphaPremultipliedLast | kCGBitmapByteOrder32Big
      );
      CGColorSpaceRelease(cs);
      if (!ctx) return pixels;

      CGContextTranslateCTM(ctx, 0, height);
      CGContextScaleCTM(ctx, 1.0, -1.0);
      UIGraphicsPushContext(ctx);

      BOOL isDead = (hp <= 0);
      BOOL isActing = (turnState == 1) && !isDead;

      CGRect cardRect = CGRectMake(3.5, 4.0, width - 7.0, height - 8.0);
      UIBezierPath *bg = CreateChamferedPath(cardRect, 14.0);

      if (isDead)
      {
         FillPathWithVerticalGradient(ctx, bg,
            [UIColor colorWithRed:0.20 green:0.10 blue:0.08 alpha:0.92],
            [UIColor colorWithRed:0.12 green:0.07 blue:0.05 alpha:0.93],
            [UIColor colorWithRed:0.08 green:0.04 blue:0.03 alpha:0.95]);
      }
      else if (isTargeted)
      {
         FillPathWithVerticalGradient(ctx, bg,
            [UIColor colorWithRed:0.34 green:0.32 blue:0.12 alpha:0.95],
            [UIColor colorWithRed:0.22 green:0.20 blue:0.08 alpha:0.94],
            [UIColor colorWithRed:0.13 green:0.11 blue:0.05 alpha:0.96]);
      }
      else if (isActing)
      {
         FillPathWithVerticalGradient(ctx, bg,
            [UIColor colorWithRed:0.42 green:0.24 blue:0.11 alpha:0.95],
            [UIColor colorWithRed:0.29 green:0.16 blue:0.07 alpha:0.95],
            [UIColor colorWithRed:0.18 green:0.09 blue:0.04 alpha:0.96]);
      }
      else
      {
         // Exact PAL98 PlayerInfoBox Rosewood-Ebony (#301408 / #240C04)
         FillPathWithVerticalGradient(ctx, bg,
            [UIColor colorWithRed:0.28 green:0.17 blue:0.09 alpha:0.92],
            [UIColor colorWithRed:0.19 green:0.10 blue:0.05 alpha:0.93],
            [UIColor colorWithRed:0.12 green:0.06 blue:0.03 alpha:0.95]);
      }

      StrokePathWithVerticalGradient(ctx, bg, (isActing || isTargeted) ? 3.4 : 2.2,
         isTargeted ? [UIColor colorWithRed:0.86 green:1.00 blue:0.56 alpha:1.0] :
         (isActing  ? [UIColor colorWithRed:1.00 green:0.94 blue:0.58 alpha:1.0]
                    : [UIColor colorWithRed:0.88 green:0.76 blue:0.50 alpha:0.88]),
         isTargeted ? [UIColor colorWithRed:0.46 green:0.78 blue:0.28 alpha:1.0] :
         (isActing  ? [UIColor colorWithRed:0.84 green:0.58 blue:0.16 alpha:1.0]
                    : [UIColor colorWithRed:0.54 green:0.39 blue:0.19 alpha:0.85]));

      UIBezierPath *innerTrim = CreateChamferedPath(CGRectInset(cardRect, 4.5, 4.5), 10.5);
      innerTrim.lineWidth = 1.2;
      [(isActing ? [UIColor colorWithRed:1.00 green:0.86 blue:0.48 alpha:0.82]
                 : [UIColor colorWithRed:0.76 green:0.62 blue:0.40 alpha:0.44]) setStroke];
      [innerTrim stroke];

      // Left Framed Hero Portrait (Dark Rosewood Backdrop + Antique Brass Trim)
      CGRect faceBox = CGRectMake(12.0, 14.0, 96.0, height - 28.0);
      UIBezierPath *faceFrame = CreateChamferedPath(faceBox, 9.0);
      FillPathWithVerticalGradient(ctx, faceFrame,
         [UIColor colorWithRed:0.24 green:0.14 blue:0.08 alpha:0.96],
         [UIColor colorWithRed:0.16 green:0.09 blue:0.05 alpha:0.96],
         [UIColor colorWithRed:0.10 green:0.05 blue:0.03 alpha:0.98]);

      if (portraitRGBA != NULL && portraitW > 0 && portraitH > 0)
      {
         CGContextSaveGState(ctx);
         [faceFrame addClip];

         size_t byteCount = (size_t)portraitW * (size_t)portraitH * 4;
         unsigned char *pmBuf = (unsigned char *)malloc(byteCount);
         if (pmBuf)
         {
            for (int p = 0; p < portraitW * portraitH; p++)
            {
               unsigned char r = portraitRGBA[p * 4 + 0];
               unsigned char g = portraitRGBA[p * 4 + 1];
               unsigned char b = portraitRGBA[p * 4 + 2];
               unsigned char a = portraitRGBA[p * 4 + 3];
               if (isDead && a > 0)
               {
                  unsigned char lum = (unsigned char)((r * 77 + g * 150 + b * 29) >> 8);
                  r = (unsigned char)(lum * 0.75);
                  g = (unsigned char)(lum * 0.70);
                  b = (unsigned char)(lum * 0.78);
               }
               else if ((statusFlags & 1) && a > 0)
               {
                  r = (unsigned char)MIN(255, (int)(r * 0.85 + 32));
                  g = (unsigned char)(g * 0.78);
                  b = (unsigned char)MIN(255, (int)(b * 0.90 + 44));
               }
               pmBuf[p * 4 + 0] = (unsigned char)((r * a) / 255);
               pmBuf[p * 4 + 1] = (unsigned char)((g * a) / 255);
               pmBuf[p * 4 + 2] = (unsigned char)((b * a) / 255);
               pmBuf[p * 4 + 3] = a;
            }
            CGColorSpaceRef pcs = CGColorSpaceCreateDeviceRGB();
            CGDataProviderRef prov = CGDataProviderCreateWithData(NULL, pmBuf, byteCount, NULL);
            CGImageRef cgImg = CGImageCreate(portraitW, portraitH, 8, 32, portraitW * 4, pcs,
               kCGImageAlphaPremultipliedLast | kCGBitmapByteOrder32Big, prov, NULL, false, kCGRenderingIntentDefault);
            if (cgImg)
            {
               CGFloat scale = MIN((faceBox.size.width - 6.0) / portraitW, (faceBox.size.height - 6.0) / portraitH);
               CGFloat dw = portraitW * scale;
               CGFloat dh = portraitH * scale;
               CGRect imgDst = CGRectMake(CGRectGetMidX(faceBox) - dw * 0.5, CGRectGetMaxY(faceBox) - dh - 2.0, dw, dh);
               CGContextSaveGState(ctx);
               CGContextTranslateCTM(ctx, 0, CGRectGetMinY(imgDst) + CGRectGetMaxY(imgDst));
               CGContextScaleCTM(ctx, 1.0, -1.0);
               CGContextSetInterpolationQuality(ctx, kCGInterpolationNone);
               CGContextDrawImage(ctx, imgDst, cgImg);
               CGContextRestoreGState(ctx);
               CGImageRelease(cgImg);
            }
            CGDataProviderRelease(prov);
            CGColorSpaceRelease(pcs);
            free(pmBuf);
         }
         CGContextRestoreGState(ctx);
      }

      faceFrame.lineWidth = 1.8;
      [(isActing ? [UIColor colorWithRed:1.00 green:0.88 blue:0.46 alpha:0.95]
                 : [UIColor colorWithRed:0.82 green:0.70 blue:0.42 alpha:0.78]) setStroke];
      [faceFrame stroke];

      // Right Column: Header (Lv + Name + Status Pills) + HP Bar + MP Bar
      NSShadow *sh = [[NSShadow alloc] init];
      sh.shadowColor = [UIColor colorWithWhite:0.0 alpha:0.94];
      sh.shadowOffset = CGSizeMake(0, 1.4);
      sh.shadowBlurRadius = 1.8;

      CGFloat rx = CGRectGetMaxX(faceBox) + 9.0;
      CGFloat rw = width - rx - 13.0;

      NSString *nameStr = @"侠客";
      if (heroName != NULL && heroName[0] != 0)
      {
         size_t wlen = wcslen(heroName);
         NSString *raw = [[NSString alloc] initWithBytes:heroName length:wlen * sizeof(wchar_t) encoding:NSUTF32LittleEndianStringEncoding];
         NSString *clean = [raw stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
         if (clean.length > 0) nameStr = clean;
      }

      NSString *headerLeft = [NSString stringWithFormat:@"Lv.%d %@", (level > 0 ? level : 1), nameStr];
      UIFont *nameFont = [UIFont fontWithName:@"PingFangSC-Semibold" size:24.0];
      if (!nameFont) nameFont = [UIFont boldSystemFontOfSize:24.0];
      UIColor *nameCol = isDead
         ? [UIColor colorWithRed:0.70 green:0.66 blue:0.68 alpha:0.95]
         : (isActing ? [UIColor colorWithRed:1.00 green:0.95 blue:0.74 alpha:1.0]
                     : [UIColor colorWithRed:0.95 green:0.93 blue:0.86 alpha:1.0]);
      NSDictionary *nameAttrs = @{ NSFontAttributeName: nameFont, NSForegroundColorAttributeName: nameCol, NSShadowAttributeName: sh };
      [headerLeft drawAtPoint:CGPointMake(rx, 13.0) withAttributes:nameAttrs];

      // Right-aligned Turn / Status Micro-Badge
      NSString *badgeTxt = nil;
      UIColor  *badgeBg  = nil;
      UIColor  *badgeRim = nil;
      UIColor  *badgeCol = nil;
      if (isDead) {
         badgeTxt = @"阵亡";
         badgeBg  = [UIColor colorWithRed:0.34 green:0.08 blue:0.10 alpha:0.95];
         badgeRim = [UIColor colorWithRed:1.00 green:0.38 blue:0.40 alpha:0.92];
         badgeCol = [UIColor colorWithRed:1.00 green:0.65 blue:0.66 alpha:1.0];
      } else if (isTargeted) {
         badgeTxt = @"✦目标";
         badgeBg  = [UIColor colorWithRed:0.16 green:0.32 blue:0.12 alpha:0.96];
         badgeRim = [UIColor colorWithRed:0.62 green:0.98 blue:0.52 alpha:0.96];
         badgeCol = [UIColor colorWithRed:0.80 green:1.00 blue:0.70 alpha:1.0];
      } else if (statusFlags & 8) {
         badgeTxt = @"乱";
         badgeBg  = [UIColor colorWithRed:0.36 green:0.08 blue:0.26 alpha:0.95];
         badgeRim = [UIColor colorWithRed:1.00 green:0.36 blue:0.74 alpha:0.92];
         badgeCol = [UIColor colorWithRed:1.00 green:0.66 blue:0.88 alpha:1.0];
      } else if (statusFlags & 2) {
         badgeTxt = @"眠";
         badgeBg  = [UIColor colorWithRed:0.16 green:0.18 blue:0.36 alpha:0.95];
         badgeRim = [UIColor colorWithRed:0.56 green:0.72 blue:0.98 alpha:0.92];
         badgeCol = [UIColor colorWithRed:0.78 green:0.88 blue:1.00 alpha:1.0];
      } else if (statusFlags & 4) {
         badgeTxt = @"封";
         badgeBg  = [UIColor colorWithRed:0.36 green:0.18 blue:0.05 alpha:0.95];
         badgeRim = [UIColor colorWithRed:1.00 green:0.62 blue:0.24 alpha:0.92];
         badgeCol = [UIColor colorWithRed:1.00 green:0.80 blue:0.50 alpha:1.0];
      } else if (statusFlags & 1) {
         badgeTxt = @"中毒";
         badgeBg  = [UIColor colorWithRed:0.26 green:0.08 blue:0.34 alpha:0.95];
         badgeRim = [UIColor colorWithRed:0.78 green:0.40 blue:0.98 alpha:0.92];
         badgeCol = [UIColor colorWithRed:0.90 green:0.68 blue:1.00 alpha:1.0];
      } else if (isActing) {
         badgeTxt = @"★出招";
         badgeBg  = [UIColor colorWithRed:0.36 green:0.22 blue:0.06 alpha:0.95];
         badgeRim = [UIColor colorWithRed:1.00 green:0.86 blue:0.38 alpha:0.96];
         badgeCol = [UIColor colorWithRed:1.00 green:0.94 blue:0.64 alpha:1.0];
      } else if (statusFlags & 16) {
         badgeTxt = @"增益";
         badgeBg  = [UIColor colorWithRed:0.18 green:0.30 blue:0.12 alpha:0.95];
         badgeRim = [UIColor colorWithRed:0.58 green:0.94 blue:0.48 alpha:0.90];
         badgeCol = [UIColor colorWithRed:0.78 green:1.00 blue:0.72 alpha:1.0];
      } else if (turnState == 2) {
         badgeTxt = @"✓已令";
         badgeBg  = [UIColor colorWithRed:0.24 green:0.18 blue:0.10 alpha:0.92];
         badgeRim = [UIColor colorWithRed:0.82 green:0.70 blue:0.44 alpha:0.78];
         badgeCol = [UIColor colorWithRed:0.94 green:0.86 blue:0.66 alpha:0.98];
      }

      if (badgeTxt)
      {
         UIFont *bf = [UIFont fontWithName:@"PingFangSC-Semibold" size:19.0];
         if (!bf) bf = [UIFont boldSystemFontOfSize:19.0];
         NSDictionary *ba = @{ NSFontAttributeName: bf, NSForegroundColorAttributeName: badgeCol, NSShadowAttributeName: sh };
         CGSize bts = [badgeTxt sizeWithAttributes:ba];
         CGFloat bw = bts.width + 14.0;
         CGRect bRect = CGRectMake(rx + rw - bw, 13.0, bw, 29.0);
         UIBezierPath *bp = CreateChamferedPath(bRect, 5.0);
         [badgeBg setFill]; [bp fill];
         bp.lineWidth = 1.3;
         [badgeRim setStroke]; [bp stroke];
         [badgeTxt drawAtPoint:CGPointMake(bRect.origin.x + 7.0, bRect.origin.y + (bRect.size.height - bts.height) * 0.5) withAttributes:ba];
      }

      // Glossy Segmented MOBA Ally Emerald HP Bar (shifts to amber/crimson at low HP!)
      int safeMaxHP = (maxHP > 0) ? maxHP : 1;
      CGFloat hpRatio = (CGFloat)hp / (CGFloat)safeMaxHP;
      UIColor *hpTop, *hpMid, *hpBot;
      if (hpRatio <= 0.22) {
         hpTop = [UIColor colorWithRed:1.00 green:0.38 blue:0.42 alpha:1.0];
         hpMid = [UIColor colorWithRed:0.84 green:0.14 blue:0.20 alpha:1.0];
         hpBot = [UIColor colorWithRed:0.52 green:0.06 blue:0.12 alpha:1.0];
      } else if (hpRatio <= 0.45) {
         hpTop = [UIColor colorWithRed:1.00 green:0.82 blue:0.32 alpha:1.0];
         hpMid = [UIColor colorWithRed:0.88 green:0.56 blue:0.10 alpha:1.0];
         hpBot = [UIColor colorWithRed:0.56 green:0.32 blue:0.05 alpha:1.0];
      } else {
         hpTop = [UIColor colorWithRed:0.32 green:0.98 blue:0.62 alpha:1.0];
         hpMid = [UIColor colorWithRed:0.11 green:0.74 blue:0.40 alpha:1.0];
         hpBot = [UIColor colorWithRed:0.05 green:0.45 blue:0.24 alpha:1.0];
      }

      CGRect hpRect = CGRectMake(rx, 54.0, rw, 46.0);
      DrawSegmentedMobaBar(ctx, hpRect, hp, safeMaxHP, hpTop, hpMid, hpBot,
         [UIColor colorWithRed:0.78 green:0.72 blue:0.46 alpha:0.84], YES);

      UIFont *barFont = [UIFont monospacedDigitSystemFontOfSize:23.0 weight:UIFontWeightBold];
      NSString *sHP = [NSString stringWithFormat:@"♥ %d / %d", hp, safeMaxHP];
      NSDictionary *hpAttrs = @{ NSFontAttributeName: barFont, NSForegroundColorAttributeName: [UIColor whiteColor], NSShadowAttributeName: sh };
      CGSize hpTS = [sHP sizeWithAttributes:hpAttrs];
      [sHP drawAtPoint:CGPointMake(hpRect.origin.x + (hpRect.size.width - hpTS.width) * 0.5, hpRect.origin.y + (hpRect.size.height - hpTS.height) * 0.5) withAttributes:hpAttrs];

      // Glossy Indigo-Spirit MP Bar framed in warm antique brass
      int safeMaxMP = (maxMP > 0) ? maxMP : 1;
      CGRect mpRect = CGRectMake(rx, 110.0, rw, 44.0);
      DrawSegmentedMobaBar(ctx, mpRect, mp, safeMaxMP,
         [UIColor colorWithRed:0.36 green:0.74 blue:0.96 alpha:1.0],
         [UIColor colorWithRed:0.15 green:0.45 blue:0.80 alpha:1.0],
         [UIColor colorWithRed:0.08 green:0.24 blue:0.48 alpha:1.0],
         [UIColor colorWithRed:0.76 green:0.68 blue:0.46 alpha:0.82], NO);

      NSString *sMP = [NSString stringWithFormat:@"✦ %d / %d", mp, safeMaxMP];
      NSDictionary *mpAttrs = @{ NSFontAttributeName: barFont, NSForegroundColorAttributeName: [UIColor colorWithRed:0.92 green:0.98 blue:1.00 alpha:1.0], NSShadowAttributeName: sh };
      CGSize mpTS = [sMP sizeWithAttributes:mpAttrs];
      [sMP drawAtPoint:CGPointMake(mpRect.origin.x + (mpRect.size.width - mpTS.width) * 0.5, mpRect.origin.y + (mpRect.size.height - mpTS.height) * 0.5) withAttributes:mpAttrs];

      UIGraphicsPopContext();
      CGContextRelease(ctx);
   }

   UnpremultiplyRGBA(pixels, width * height);
   return pixels;
}

unsigned char *
UTIL_RenderBattleSkillOrbRGBA(
   int          size,
   int          orbType,
   int          isSelected,
   int          isEnabled
)
{
   if (size <= 0) return NULL;
   unsigned char *pixels = (unsigned char *)calloc((size_t)size * (size_t)size, 4);
   if (!pixels) return NULL;

   @autoreleasepool {
      CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
      CGContextRef ctx = CGBitmapContextCreate(
         pixels, size, size, 8, size * 4, cs,
         kCGImageAlphaPremultipliedLast | kCGBitmapByteOrder32Big
      );
      CGColorSpaceRelease(cs);
      if (!ctx) return pixels;

      CGContextTranslateCTM(ctx, 0, size);
      CGContextScaleCTM(ctx, 1.0, -1.0);
      UIGraphicsPushContext(ctx);

      CGFloat c = size * 0.5;
      CGFloat rOuter = size * 0.435;

      if (isSelected && isEnabled)
      {
         UIBezierPath *aura = [UIBezierPath bezierPathWithOvalInRect:CGRectMake(c - rOuter - 7.0, c - rOuter - 7.0, (rOuter + 7.0) * 2.0, (rOuter + 7.0) * 2.0)];
         [[UIColor colorWithRed:1.00 green:0.88 blue:0.42 alpha:0.32] setFill];
         [aura fill];
      }

      UIColor *cIn = nil, *cMid = nil, *cOut = nil, *cRing = nil;
      NSString *title = @"普攻";
      NSString *iconKey = @"围攻";
      switch (orbType)
      {
      case 0: // 普攻 (朱漆红檀 · Cinnabar-Rosewood)
         title   = @"普攻";
         iconKey = @"围攻";
         cIn     = [UIColor colorWithRed:0.48 green:0.20 blue:0.12 alpha:0.96];
         cMid    = [UIColor colorWithRed:0.30 green:0.11 blue:0.07 alpha:0.95];
         cOut    = [UIColor colorWithRed:0.16 green:0.06 blue:0.03 alpha:0.96];
         cRing   = [UIColor colorWithRed:0.96 green:0.60 blue:0.38 alpha:0.90];
         break;
      case 1: // 仙术 (金丝楠檀 · Golden Sandalwood)
         title   = @"仙术";
         iconKey = @"仙术";
         cIn     = [UIColor colorWithRed:0.44 green:0.29 blue:0.13 alpha:0.96];
         cMid    = [UIColor colorWithRed:0.28 green:0.17 blue:0.08 alpha:0.95];
         cOut    = [UIColor colorWithRed:0.15 green:0.09 blue:0.04 alpha:0.96];
         cRing   = [UIColor colorWithRed:0.96 green:0.82 blue:0.46 alpha:0.92];
         break;
      case 2: // 合击 (紫檀沉香 · Imperial Rosewood)
         title   = @"合击";
         iconKey = @"状态";
         cIn     = [UIColor colorWithRed:0.42 green:0.18 blue:0.20 alpha:0.96];
         cMid    = [UIColor colorWithRed:0.26 green:0.10 blue:0.12 alpha:0.95];
         cOut    = [UIColor colorWithRed:0.14 green:0.05 blue:0.06 alpha:0.96];
         cRing   = [UIColor colorWithRed:0.94 green:0.68 blue:0.56 alpha:0.90];
         break;
      default: // 战术 (古竹乌木 · Aged Bamboo-Wood)
         title   = @"战术";
         iconKey = @"防御";
         cIn     = [UIColor colorWithRed:0.36 green:0.25 blue:0.14 alpha:0.96];
         cMid    = [UIColor colorWithRed:0.23 green:0.15 blue:0.08 alpha:0.95];
         cOut    = [UIColor colorWithRed:0.13 green:0.08 blue:0.04 alpha:0.96];
         cRing   = [UIColor colorWithRed:0.88 green:0.74 blue:0.48 alpha:0.90];
         break;
      }

      if (!isEnabled)
      {
         cIn   = [UIColor colorWithRed:0.19 green:0.14 blue:0.10 alpha:0.74];
         cMid  = [UIColor colorWithRed:0.13 green:0.09 blue:0.06 alpha:0.76];
         cOut  = [UIColor colorWithRed:0.08 green:0.05 blue:0.03 alpha:0.80];
         cRing = [UIColor colorWithRed:0.48 green:0.40 blue:0.30 alpha:0.45];
      }

      UIBezierPath *disc = [UIBezierPath bezierPathWithOvalInRect:CGRectMake(c - rOuter, c - rOuter, rOuter * 2.0, rOuter * 2.0)];
      FillPathWithRadialGradient(ctx, disc, CGPointMake(c, c - rOuter * 0.22), rOuter * 1.15, cIn, cMid, cOut);

      StrokePathWithVerticalGradient(ctx, disc, isSelected ? 5.2 : 3.6,
         (isSelected && isEnabled) ? [UIColor colorWithRed:1.00 green:0.96 blue:0.68 alpha:1.0]
                                   : [UIColor colorWithRed:0.90 green:0.78 blue:0.48 alpha:(isEnabled ? 0.90 : 0.45)],
         (isSelected && isEnabled) ? [UIColor colorWithRed:0.88 green:0.64 blue:0.20 alpha:1.0]
                                   : [UIColor colorWithRed:0.54 green:0.38 blue:0.16 alpha:(isEnabled ? 0.88 : 0.42)]);

      CGFloat rInner = rOuter - 7.5;
      UIBezierPath *inner = [UIBezierPath bezierPathWithOvalInRect:CGRectMake(c - rInner, c - rInner, rInner * 2.0, rInner * 2.0)];
      inner.lineWidth = 1.8;
      [cRing setStroke];
      [inner stroke];

      if (isEnabled)
      {
         UIColor *studCol = isSelected
            ? [UIColor colorWithRed:1.00 green:0.96 blue:0.72 alpha:1.0]
            : [UIColor colorWithRed:0.92 green:0.79 blue:0.48 alpha:0.90];
         UIColor *studRim = [UIColor colorWithRed:0.16 green:0.09 blue:0.04 alpha:0.95];
         DrawDiamondStud(CGPointMake(c, c - rOuter), 5.0, 7.0, studCol, studRim);
         DrawDiamondStud(CGPointMake(c, c + rOuter), 5.0, 7.0, studCol, studRim);
         DrawDiamondStud(CGPointMake(c - rOuter, c), 7.0, 5.0, studCol, studRim);
         DrawDiamondStud(CGPointMake(c + rOuter, c), 7.0, 5.0, studCol, studRim);
      }

      // Upper Vector Icon + Lower Label
      CGFloat iconSz = size * 0.28;
      CGRect iconBox = CGRectMake(c - iconSz * 0.5, c - rOuter * 0.62, iconSz, iconSz);
      DrawHudVectorIcon(ctx, iconKey, iconBox,
         isEnabled ? [UIColor colorWithRed:1.00 green:0.93 blue:0.68 alpha:1.0] : [UIColor colorWithWhite:0.55 alpha:0.65],
         isEnabled ? cRing : [UIColor colorWithWhite:0.45 alpha:0.55]);

      NSShadow *sh = [[NSShadow alloc] init];
      sh.shadowColor = [UIColor colorWithWhite:0.0 alpha:0.95];
      sh.shadowOffset = CGSizeMake(0, 1.8);
      sh.shadowBlurRadius = 2.4;
      UIFont *font = [UIFont fontWithName:@"PingFangSC-Semibold" size:size * 0.225];
      if (!font) font = [UIFont boldSystemFontOfSize:size * 0.225];
      UIColor *txtCol = !isEnabled
         ? [UIColor colorWithWhite:0.58 alpha:0.75]
         : (isSelected ? [UIColor colorWithRed:1.00 green:0.97 blue:0.80 alpha:1.0]
                       : [UIColor colorWithRed:0.95 green:0.92 blue:0.84 alpha:0.98]);
      NSDictionary *attrs = @{ NSFontAttributeName: font, NSForegroundColorAttributeName: txtCol, NSShadowAttributeName: sh };
      CGSize ts = [title sizeWithAttributes:attrs];
      [title drawAtPoint:CGPointMake(c - ts.width * 0.5, c + rOuter * 0.02) withAttributes:attrs];

      UIGraphicsPopContext();
      CGContextRelease(ctx);
   }

   UnpremultiplyRGBA(pixels, size * size);
   return pixels;
}

unsigned char *
UTIL_RenderSearchTagRGBA(
   int          width,
   int          height,
   LPCWSTR      itemName,
   int          cashAmount,
   int          category
)
{
   if (width <= 0 || height <= 0) return NULL;
   unsigned char *pixels = (unsigned char *)calloc((size_t)width * (size_t)height, 4);
   if (!pixels) return NULL;

   @autoreleasepool {
      CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
      CGContextRef ctx = CGBitmapContextCreate(
         pixels, width, height, 8, width * 4, cs,
         kCGImageAlphaPremultipliedLast | kCGBitmapByteOrder32Big
      );
      CGColorSpaceRelease(cs);
      if (!ctx) return pixels;

      CGContextTranslateCTM(ctx, 0, height);
      CGContextScaleCTM(ctx, 1.0, -1.0);
      UIGraphicsPushContext(ctx);

      CGRect rect = CGRectMake(2.5, 2.5, width - 5.0, height - 5.0);
      UIBezierPath *bg = CreateChamferedPath(rect, (height - 5.0) * 0.28);

      UIColor *fillTop = nil, *fillBot = nil, *rimCol = nil, *txtCol = nil;
      NSString *label  = nil;

      if (category == 0) {
         // Treasure / Hidden Item / Cash — Gilded Sandalwood Badge
         fillTop = [UIColor colorWithRed:0.38 green:0.24 blue:0.08 alpha:0.93];
         fillBot = [UIColor colorWithRed:0.20 green:0.12 blue:0.04 alpha:0.95];
         rimCol  = [UIColor colorWithRed:1.00 green:0.86 blue:0.38 alpha:0.96];
         txtCol  = [UIColor colorWithRed:1.00 green:0.94 blue:0.60 alpha:1.00];
         if (itemName != NULL && itemName[0] != 0) {
            size_t wlen = wcslen(itemName);
            NSString *raw = [[NSString alloc] initWithBytes:itemName
                                                     length:wlen * sizeof(wchar_t)
                                                   encoding:NSUTF32LittleEndianStringEncoding];
            NSString *clean = [raw stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
            label = (clean.length > 0) ? [NSString stringWithFormat:@"★%@", clean] : @"★宝物";
         } else if (cashAmount > 0) {
            label = [NSString stringWithFormat:@"★%d文", cashAmount];
         } else {
            label = @"★宝物";
         }
      } else if (category == 1) {
         // Mechanism / Switch — Bamboo-Celadon Wood Badge
         fillTop = [UIColor colorWithRed:0.22 green:0.27 blue:0.11 alpha:0.91];
         fillBot = [UIColor colorWithRed:0.12 green:0.15 blue:0.06 alpha:0.93];
         rimCol  = [UIColor colorWithRed:0.68 green:0.90 blue:0.46 alpha:0.94];
         txtCol  = [UIColor colorWithRed:0.84 green:0.98 blue:0.68 alpha:1.00];
         label   = @"✦调查";
      } else {
         // Talkable NPC — Ancient Parchment & Sandalwood Badge
         fillTop = [UIColor colorWithRed:0.32 green:0.22 blue:0.12 alpha:0.90];
         fillBot = [UIColor colorWithRed:0.18 green:0.12 blue:0.06 alpha:0.92];
         rimCol  = [UIColor colorWithRed:0.88 green:0.76 blue:0.52 alpha:0.90];
         txtCol  = [UIColor colorWithRed:0.97 green:0.92 blue:0.82 alpha:1.00];
         label   = @"对话";
      }

      FillPathWithVerticalGradient(ctx, bg, fillTop, fillTop, fillBot);
      bg.lineWidth = 2.0;
      [rimCol setStroke];
      [bg stroke];

      NSShadow *shadow = [[NSShadow alloc] init];
      shadow.shadowColor = [UIColor colorWithWhite:0.0 alpha:0.92];
      shadow.shadowOffset = CGSizeMake(0, 1.3);
      shadow.shadowBlurRadius = 1.8;

      CGFloat sz = height * 0.48;
      NSDictionary *attrs = nil;
      CGSize ts = CGSizeZero;
      while (sz > 12.0) {
         UIFont *f = [UIFont fontWithName:@"PingFangSC-Semibold" size:sz];
         if (!f) f = [UIFont boldSystemFontOfSize:sz];
         attrs = @{
            NSFontAttributeName: f,
            NSForegroundColorAttributeName: txtCol,
            NSShadowAttributeName: shadow
         };
         ts = [label sizeWithAttributes:attrs];
         if (ts.width <= width - 18.0) break;
         sz -= 1.5;
      }
      CGRect tr = CGRectMake((width - ts.width) * 0.5, (height - ts.height) * 0.5, ts.width, ts.height);
      [label drawInRect:tr withAttributes:attrs];

      UIGraphicsPopContext();
      CGContextRelease(ctx);
   }

   UnpremultiplyRGBA(pixels, width * height);
   return pixels;
}

// ============================================================================
// Live System Configuration Modal (Performance HUD, MIDI/RIX Synth, Display)
// ============================================================================

@interface PALSystemSettingsViewController : UITableViewController
@property (nonatomic, assign) BOOL isFinished;
@property (nonatomic, assign) INT  resultCode; // 0 = applied live / cancelled, 1 = restart engine
@property (nonatomic, assign) BOOL inGame;
@property (nonatomic, assign) int  currentMusicNum;
@end

@implementation PALSystemSettingsViewController {
   BOOL _perfHud;
   BOOL _touchOverlay;
   BOOL _enemyStatsHud;
   BOOL _rightHandedDpad;
   BOOL _keepAspect;
   BOOL _smoothScale;
   BOOL _enableGPU;
   BOOL _enableGLSL;
   BOOL _enableHDR;
   BOOL _launchSettingOnBoot;

   MUSICTYPE     _musicType;
   MIDISYNTHTYPE _midiSynth;
   OPLCORE_TYPE  _oplCore;
   OPLCHIP_TYPE  _oplChip;
   CDTYPE        _cdType;
   int           _oplRate;
   int           _sampleRate;
   int           _bufferSize;
   BOOL          _stereo;
   BOOL          _surroundOPL;
   int           _musicVol;
   int           _soundVol;
   int           _resampleQuality;
   LOGLEVEL      _logLevel;
   NSString     *_soundBank;
   NSString     *_gpuFilter;

   NSMutableArray<NSString *> *_availSoundFonts;
}

- (instancetype)initWithInGame:(BOOL)inGame musicNum:(int)musicNum {
   if (@available(iOS 13.0, *)) {
      self = [super initWithStyle:UITableViewStyleInsetGrouped];
   } else {
      self = [super initWithStyle:UITableViewStyleGrouped];
   }
   if (self) {
      _inGame = inGame;
      _currentMusicNum = musicNum;
      _isFinished = NO;
      _resultCode = 0;
      [self loadFromCurrentConfig];
      [self scanSoundFonts];
   }
   return self;
}

- (void)loadFromCurrentConfig {
   _perfHud = UTIL_IsPerfHUDEnabled() ? YES : NO;
   _touchOverlay = gConfig.fUseTouchOverlay ? YES : NO;
   _enemyStatsHud = UTIL_IsEnemyStatsHUDEnabled() ? YES : NO;
   _rightHandedDpad = UTIL_IsRightHandedDpad() ? YES : NO;
   _keepAspect = gConfig.fKeepAspectRatio ? YES : NO;
   _smoothScale = (gConfig.pszScaleQuality && atoi(gConfig.pszScaleQuality) == 1) ? YES : NO;
   _enableGPU = (gConfig.fEnableGPU && !gConfig.fEnableGLSL) ? YES : NO;
   _enableGLSL = gConfig.fEnableGLSL ? YES : NO;
   _enableHDR = gConfig.fEnableHDR ? YES : NO;
   _launchSettingOnBoot = gConfig.fLaunchSetting ? YES : NO;

   _musicType = gConfig.eMusicType;
   _midiSynth = gConfig.eMIDISynth;
   _oplCore = gConfig.eOPLCore;
   _oplChip = gConfig.eOPLChip;
   _cdType = gConfig.eCDType;
   _oplRate = gConfig.iOPLSampleRate > 0 ? gConfig.iOPLSampleRate : 49716;
   _sampleRate = gConfig.iSampleRate > 0 ? gConfig.iSampleRate : 48000;
   _bufferSize = gConfig.dwAudioBufferSize > 0 ? (int)gConfig.dwAudioBufferSize : 1024;
   _stereo = (gConfig.iAudioChannels == 2);
   _surroundOPL = gConfig.fUseSurroundOPL ? YES : NO;
   _musicVol = gConfig.iMusicVolume;
   _soundVol = gConfig.iSoundVolume;
   _resampleQuality = gConfig.iResampleQuality;
   _logLevel = gConfig.iLogLevel;
   _soundBank = (gConfig.pszSoundBank && gConfig.pszSoundBank[0]) ? [NSString stringWithUTF8String:gConfig.pszSoundBank] : @"";
   _gpuFilter = (gConfig.pszFilter && gConfig.pszFilter[0]) ? [NSString stringWithUTF8String:gConfig.pszFilter] : @"none";
}

- (void)scanSoundFonts {
   _availSoundFonts = [NSMutableArray arrayWithObject:@"内置 Roland GS (gs_instruments.dls)"];
   NSFileManager *fm = [NSFileManager defaultManager];
   NSArray<NSString *> *dirs = @[
      [NSString stringWithUTF8String:UTIL_SavePath()],
      [NSString stringWithUTF8String:UTIL_BasePath()]
   ];
   for (NSString *dir in dirs) {
      NSArray<NSString *> *sub = [fm subpathsOfDirectoryAtPath:dir error:nil];
      for (NSString *f in sub) {
         NSString *ext = [f.pathExtension lowercaseString];
         if ([ext isEqualToString:@"sf2"] || [ext isEqualToString:@"dls"] ||
             ([ext isEqualToString:@"cfg"] && ![f.lastPathComponent.lowercaseString isEqualToString:@"sdlpal.cfg"])) {
            if (![_availSoundFonts containsObject:f]) {
               [_availSoundFonts addObject:f];
            }
         }
      }
   }
}

- (void)viewDidLoad {
   [super viewDidLoad];
   self.title = @"仙剑奇侠传 · 系统底层配置";
   self.navigationItem.leftBarButtonItem = [[UIBarButtonItem alloc] initWithTitle:@"取消"
                                                                            style:UIBarButtonItemStylePlain
                                                                           target:self
                                                                           action:@selector(onCancel)];
   self.navigationItem.rightBarButtonItem = [[UIBarButtonItem alloc] initWithTitle:@"保存并生效"
                                                                             style:UIBarButtonItemStyleDone
                                                                            target:self
                                                                            action:@selector(onSaveLive)];
}

- (void)onCancel {
   self.resultCode = 0;
   self.isFinished = YES;
   [self dismissViewControllerAnimated:YES completion:nil];
}

- (void)applyConfigChanges:(BOOL)restartEngine {
   BOOL audioChanged =
      (gConfig.eMusicType != _musicType) ||
      (gConfig.eMIDISynth != _midiSynth) ||
      (gConfig.eOPLCore != _oplCore) ||
      (gConfig.eOPLChip != _oplChip) ||
      (gConfig.eCDType != _cdType) ||
      (gConfig.iOPLSampleRate != _oplRate) ||
      (gConfig.iSampleRate != _sampleRate) ||
      ((int)gConfig.dwAudioBufferSize != _bufferSize) ||
      ((gConfig.iAudioChannels == 2) != _stereo) ||
      ((gConfig.fUseSurroundOPL ? YES : NO) != _surroundOPL) ||
      (gConfig.iMusicVolume != _musicVol) ||
      (gConfig.iSoundVolume != _soundVol) ||
      (gConfig.iResampleQuality != _resampleQuality) ||
      ![(_soundBank ?: @"") isEqualToString:(gConfig.pszSoundBank ? [NSString stringWithUTF8String:gConfig.pszSoundBank] : @"")];

   BOOL videoDisplayChanged =
      ((gConfig.fKeepAspectRatio ? YES : NO) != _keepAspect) ||
      (((gConfig.pszScaleQuality && atoi(gConfig.pszScaleQuality) == 1) ? YES : NO) != _smoothScale);

   if (!restartEngine && audioChanged) {
      AUDIO_CloseDevice();
   }

   UTIL_SetPerfHUDEnabled(_perfHud ? TRUE : FALSE);
   UTIL_SetEnemyStatsHUDEnabled(_enemyStatsHud ? TRUE : FALSE);
   UTIL_SetRightHandedDpad(_rightHandedDpad ? TRUE : FALSE);
   gConfig.fUseTouchOverlay = _touchOverlay ? TRUE : FALSE;
   gConfig.fKeepAspectRatio = _keepAspect ? TRUE : FALSE;
   free(gConfig.pszScaleQuality);
   gConfig.pszScaleQuality = _smoothScale ? strdup("1") : strdup("0");

   gConfig.fEnableGPU = _enableGPU ? TRUE : FALSE;
   gConfig.fEnableGLSL = _enableGLSL ? TRUE : FALSE;
   gConfig.fEnableHDR = _enableHDR ? TRUE : FALSE;
   free(gConfig.pszFilter);
   gConfig.pszFilter = ([_gpuFilter isEqualToString:@"none"] || _gpuFilter.length == 0) ? NULL : strdup([_gpuFilter UTF8String]);

   gConfig.fLaunchSetting = _launchSettingOnBoot ? TRUE : FALSE;

   gConfig.eMusicType = _musicType;
   gConfig.eMIDISynth = _midiSynth;
   gConfig.eOPLCore = _oplCore;
   gConfig.eOPLChip = _oplChip;
   gConfig.eCDType = _cdType;
   gConfig.iOPLSampleRate = _oplRate;
   gConfig.iSampleRate = _sampleRate;
   gConfig.dwAudioBufferSize = (WORD)_bufferSize;
   gConfig.iAudioChannels = _stereo ? 2 : 1;
   gConfig.fUseSurroundOPL = _surroundOPL ? TRUE : FALSE;
   gConfig.iMusicVolume = _musicVol;
   gConfig.iSoundVolume = _soundVol;
   gConfig.iResampleQuality = _resampleQuality;
   gConfig.iLogLevel = _logLevel;

   free(gConfig.pszSoundBank);
   gConfig.pszSoundBank = (_soundBank.length > 0) ? strdup([_soundBank UTF8String]) : NULL;

   PAL_SaveConfig();

   if (!restartEngine) {
      if (audioChanged) {
         AUDIO_OpenDevice();
         if (_currentMusicNum > 0 && AUDIO_MusicEnabled()) {
            AUDIO_PlayMusic(_currentMusicNum, TRUE, 0);
         }
      }
      if (videoDisplayChanged && gpRenderer != NULL) {
         int outW = 0, outH = 0;
         SDL_GetRendererOutputSize(gpRenderer, &outW, &outH);
         if (outW > 0 && outH > 0) {
            VIDEO_Resize(outW, outH);
         }
      }
   }
}

- (void)onSaveLive {
   [self applyConfigChanges:NO];
   self.resultCode = 0;
   self.isFinished = YES;
   [self dismissViewControllerAnimated:YES completion:nil];
}

- (void)onSaveAndRestart {
   [self applyConfigChanges:YES];
   self.resultCode = 1;
   self.isFinished = YES;
   [self dismissViewControllerAnimated:YES completion:nil];
}

- (void)onResetDefaults {
   PAL_LoadConfig(NO);
   UTIL_SetPerfHUDEnabled(FALSE);
   UTIL_SetEnemyStatsHUDEnabled(TRUE);
   UTIL_SetRightHandedDpad(TRUE);
   [self loadFromCurrentConfig];
   [self.tableView reloadData];
}

#pragma mark - Table Data Source

- (NSInteger)numberOfSectionsInTableView:(UITableView *)tableView {
   return 4;
}

- (NSString *)tableView:(UITableView *)tableView titleForHeaderInSection:(NSInteger)section {
   switch (section) {
      case 0: return @"HUD 与画面显示（即时生效）";
      case 1: return @"音乐与 MIDI / OPL 音源（即时热切换）";
      case 2: return @"底层渲染后端与高级日志";
      case 3: return @"操作";
      default: return nil;
   }
}

- (NSString *)tableView:(UITableView *)tableView titleForFooterInSection:(NSInteger)section {
   if (section == 0) {
      return @"说明：1)「方向键与调查键位置」支持左右手习惯随时一键互换（左手方向盘+右侧调查，或右手方向盘+左侧调查）；2) 关闭「触屏操控按键 HUD」时仅隐藏方向盘与快捷栏，点地行走/点菜单仍正常可用，且左上角始终保留一个「设置」小按钮方便随时重新开启；3)「性能监控 HUD」直接控制右上角 Apple Metal 原生性能面板的开启与彻底隐藏。";
   }
   if (section == 1) {
      return @"本包已内置全套 87 首 MIDI 曲库与 Apple Roland GS 音色库；你也可通过 iOS「文件」App → 我的 iPhone → 仙剑奇侠传 放入自定义 .sf2 音色库供 Native / TinySoundFont 使用。";
   }
   return nil;
}

- (NSInteger)tableView:(UITableView *)tableView numberOfRowsInSection:(NSInteger)section {
   switch (section) {
      case 0: return 6;
      case 1: return 11;
      case 2: return 5;
      case 3: return 2;
      default: return 0;
   }
}

- (UISwitch *)makeSwitch:(BOOL)on tag:(NSInteger)tag {
   UISwitch *sw = [[UISwitch alloc] init];
   sw.on = on;
   sw.tag = tag;
   [sw addTarget:self action:@selector(onSwitchChanged:) forControlEvents:UIControlEventValueChanged];
   return sw;
}

- (void)onSwitchChanged:(UISwitch *)sw {
   switch (sw.tag) {
      case 100: _perfHud = sw.isOn; break;
      case 101: _touchOverlay = sw.isOn; break;
      case 104: _enemyStatsHud = sw.isOn; break;
      case 102: _keepAspect = sw.isOn; break;
      case 103: _smoothScale = sw.isOn; break;
      case 200: _stereo = sw.isOn; break;
      case 201: _surroundOPL = sw.isOn; break;
      case 300: _enableGPU = sw.isOn; if (_enableGPU) _enableGLSL = NO; [self.tableView reloadData]; break;
      case 301: _enableGLSL = sw.isOn; if (_enableGLSL) _enableGPU = NO; [self.tableView reloadData]; break;
      case 302: _launchSettingOnBoot = sw.isOn; break;
      default: break;
   }
}

- (void)onHandednessChanged:(UISegmentedControl *)seg {
   _rightHandedDpad = (seg.selectedSegmentIndex == 1);
}

- (void)onSliderChanged:(UISlider *)sl {
   int v = (int)roundf(sl.value);
   if (sl.tag == 400) _musicVol = v;
   else if (sl.tag == 401) _soundVol = v;
}

- (UITableViewCell *)tableView:(UITableView *)tableView cellForRowAtIndexPath:(NSIndexPath *)indexPath {
   UITableViewCell *cell = [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleValue1 reuseIdentifier:nil];
   cell.selectionStyle = UITableViewCellSelectionStyleDefault;

   static NSArray *musicNames = nil;
   static NSArray *midiNames = nil;
   static NSArray *oplCores = nil;
   static NSArray *oplChips = nil;
   static NSArray *cdNames = nil;
   static NSArray *logNames = nil;
   if (!musicNames) {
      musicNames = @[ @"MIDI", @"RIX (原版FM芯片)", @"MP3", @"OGG", @"OPUS" ];
      midiNames = @[ @"Native (iOS Roland GS / SF2)", @"TiMidity", @"TinySoundFont (.sf2)" ];
      oplCores = @[ @"MAME", @"DBFLT (DOSBox浮点)", @"DBINT (DOSBox定点)", @"NUKED (高精度OPL3)" ];
      oplChips = @[ @"OPL2", @"OPL3" ];
      cdNames = @[ @"NONE", @"MP3", @"OGG", @"OPUS" ];
      logNames = @[ @"VERBOSE", @"DEBUG", @"INFO", @"WARNING", @"ERROR", @"FATAL" ];
   }

   if (indexPath.section == 0) {
      cell.selectionStyle = UITableViewCellSelectionStyleNone;
      if (indexPath.row == 0) {
         cell.textLabel.text = @"性能监控 HUD 显示 (FPS / 帧耗时 / 音源)";
         cell.accessoryView = [self makeSwitch:_perfHud tag:100];
      } else if (indexPath.row == 1) {
         cell.textLabel.text = @"触屏操控按键 HUD (Touch Overlay)";
         cell.accessoryView = [self makeSwitch:_touchOverlay tag:101];
      } else if (indexPath.row == 2) {
         cell.textLabel.text = @"战斗中怪物头顶显示血量与全抗性弱点";
         cell.accessoryView = [self makeSwitch:_enemyStatsHud tag:104];
      } else if (indexPath.row == 3) {
         cell.textLabel.text = @"方向键与调查键位置 (左右互换)";
         UISegmentedControl *seg = [[UISegmentedControl alloc] initWithItems:@[ @"左方向 · 右调查", @"右方向 · 左调查" ]];
         seg.selectedSegmentIndex = _rightHandedDpad ? 1 : 0;
         [seg addTarget:self action:@selector(onHandednessChanged:) forControlEvents:UIControlEventValueChanged];
         cell.accessoryView = seg;
      } else if (indexPath.row == 4) {
         cell.textLabel.text = @"保持原始画面比例 (4:2.5)";
         cell.accessoryView = [self makeSwitch:_keepAspect tag:102];
      } else if (indexPath.row == 5) {
         cell.textLabel.text = @"双线性平滑缩放 (关闭则像素锐利)";
         cell.accessoryView = [self makeSwitch:_smoothScale tag:103];
      }
   } else if (indexPath.section == 1) {
      if (indexPath.row == 0) {
         cell.textLabel.text = @"背景音乐音源格式 (Music)";
         cell.detailTextLabel.text = (_musicType >= 0 && _musicType < musicNames.count) ? musicNames[_musicType] : @"RIX";
         cell.accessoryType = UITableViewCellAccessoryDisclosureIndicator;
      } else if (indexPath.row == 1) {
         cell.textLabel.text = @"MIDI 合成器引擎 (MIDI Synth)";
         cell.detailTextLabel.text = (_midiSynth >= 0 && _midiSynth < midiNames.count) ? midiNames[_midiSynth] : @"Native";
         cell.accessoryType = UITableViewCellAccessoryDisclosureIndicator;
      } else if (indexPath.row == 2) {
         cell.textLabel.text = @"MIDI SoundFont 音色库 (.sf2/.dls)";
         cell.detailTextLabel.text = (_soundBank.length > 0) ? _soundBank : @"内置 Roland GS";
         cell.accessoryType = UITableViewCellAccessoryDisclosureIndicator;
      } else if (indexPath.row == 3) {
         cell.textLabel.text = @"RIX OPL 合成内核 (OPL Core)";
         cell.detailTextLabel.text = (_oplCore >= 0 && _oplCore < oplCores.count) ? oplCores[_oplCore] : @"DBFLT";
         cell.accessoryType = UITableViewCellAccessoryDisclosureIndicator;
      } else if (indexPath.row == 4) {
         cell.textLabel.text = @"RIX OPL 芯片型号 (OPL Chip)";
         cell.detailTextLabel.text = (_oplChip >= 0 && _oplChip < oplChips.count) ? oplChips[_oplChip] : @"OPL2";
         cell.accessoryType = UITableViewCellAccessoryDisclosureIndicator;
      } else if (indexPath.row == 5) {
         cell.textLabel.text = @"OPL 合成采样率";
         cell.detailTextLabel.text = [NSString stringWithFormat:@"%d Hz", _oplRate];
         cell.accessoryType = UITableViewCellAccessoryDisclosureIndicator;
      } else if (indexPath.row == 6) {
         cell.textLabel.text = @"音频输出采样率 (Sample Rate)";
         cell.detailTextLabel.text = [NSString stringWithFormat:@"%d Hz", _sampleRate];
         cell.accessoryType = UITableViewCellAccessoryDisclosureIndicator;
      } else if (indexPath.row == 7) {
         cell.selectionStyle = UITableViewCellSelectionStyleNone;
         cell.textLabel.text = @"立体声输出 (Stereo)";
         cell.accessoryView = [self makeSwitch:_stereo tag:200];
      } else if (indexPath.row == 8) {
         cell.selectionStyle = UITableViewCellSelectionStyleNone;
         cell.textLabel.text = @"环绕立体声 OPL (Surround OPL)";
         cell.accessoryView = [self makeSwitch:_surroundOPL tag:201];
      } else if (indexPath.row == 9 || indexPath.row == 10) {
         cell.selectionStyle = UITableViewCellSelectionStyleNone;
         BOOL isMus = (indexPath.row == 9);
         cell.textLabel.text = isMus ? @"音乐音量" : @"音效音量";
         UISlider *sl = [[UISlider alloc] initWithFrame:CGRectMake(0, 0, 180, 30)];
         sl.minimumValue = 0;
         sl.maximumValue = 100;
         sl.value = isMus ? _musicVol : _soundVol;
         sl.tag = isMus ? 400 : 401;
         [sl addTarget:self action:@selector(onSliderChanged:) forControlEvents:UIControlEventValueChanged];
         cell.accessoryView = sl;
      }
   } else if (indexPath.section == 2) {
      if (indexPath.row == 0) {
         cell.selectionStyle = UITableViewCellSelectionStyleNone;
         cell.textLabel.text = @"启用 SDL_GPU 渲染后端 (重启生效)";
         cell.accessoryView = [self makeSwitch:_enableGPU tag:300];
      } else if (indexPath.row == 1) {
         cell.textLabel.text = @"SDL_GPU 画面滤镜";
         cell.detailTextLabel.text = _gpuFilter ?: @"none";
         cell.accessoryType = UITableViewCellAccessoryDisclosureIndicator;
      } else if (indexPath.row == 2) {
         cell.textLabel.text = @"CD 音轨音源格式";
         cell.detailTextLabel.text = (_cdType >= 0 && _cdType < cdNames.count) ? cdNames[_cdType] : @"NONE";
         cell.accessoryType = UITableViewCellAccessoryDisclosureIndicator;
      } else if (indexPath.row == 3) {
         cell.textLabel.text = @"日志输出级别 (Log Level)";
         cell.detailTextLabel.text = (_logLevel >= 0 && _logLevel < logNames.count) ? logNames[_logLevel] : @"ERROR";
         cell.accessoryType = UITableViewCellAccessoryDisclosureIndicator;
      } else if (indexPath.row == 4) {
         cell.selectionStyle = UITableViewCellSelectionStyleNone;
         cell.textLabel.text = @"每次启动 App 先打开原版启动配置页";
         cell.accessoryView = [self makeSwitch:_launchSettingOnBoot tag:302];
      }
   } else if (indexPath.section == 3) {
      cell.textLabel.textAlignment = NSTextAlignmentCenter;
      if (indexPath.row == 0) {
         cell.textLabel.text = @"保存配置并重启游戏引擎（切换 GPU 后端时使用）";
         cell.textLabel.textColor = [UIColor systemBlueColor];
      } else {
         cell.textLabel.text = @"恢复默认配置";
         cell.textLabel.textColor = [UIColor systemRedColor];
      }
   }

   return cell;
}

- (void)presentChoiceTitle:(NSString *)title
                   options:(NSArray<NSString *> *)options
                sourceCell:(UITableViewCell *)cell
                  selected:(void (^)(NSInteger idx))handler {
   UIAlertController *ac = [UIAlertController alertControllerWithTitle:title
                                                               message:nil
                                                        preferredStyle:UIAlertControllerStyleActionSheet];
   for (NSInteger i = 0; i < (NSInteger)options.count; i++) {
      NSString *opt = options[i];
      [ac addAction:[UIAlertAction actionWithTitle:opt style:UIAlertActionStyleDefault handler:^(UIAlertAction * _Nonnull action) {
         if (handler) handler(i);
         [self.tableView reloadData];
      }]];
   }
   [ac addAction:[UIAlertAction actionWithTitle:@"取消" style:UIAlertActionStyleCancel handler:nil]];
   UIPopoverPresentationController *pop = ac.popoverPresentationController;
   if (pop) {
      pop.sourceView = cell ?: self.view;
      pop.sourceRect = cell ? cell.bounds : CGRectMake(self.view.bounds.size.width * 0.5, self.view.bounds.size.height * 0.5, 1, 1);
   }
   [self presentViewController:ac animated:YES completion:nil];
}

- (void)tableView:(UITableView *)tableView didSelectRowAtIndexPath:(NSIndexPath *)indexPath {
   [tableView deselectRowAtIndexPath:indexPath animated:YES];
   UITableViewCell *cell = [tableView cellForRowAtIndexPath:indexPath];

   if (indexPath.section == 1) {
      if (indexPath.row == 0) {
         [self presentChoiceTitle:@"选择背景音乐格式"
                          options:@[ @"MIDI (使用87首MIDI曲库)", @"RIX (原版AdLib FM合成)", @"MP3", @"OGG", @"OPUS" ]
                       sourceCell:cell
                         selected:^(NSInteger idx) { self->_musicType = (MUSICTYPE)idx; }];
      } else if (indexPath.row == 1) {
         [self presentChoiceTitle:@"选择 MIDI 合成器引擎"
                          options:@[ @"Native (iOS CoreAudio Roland GS / SF2)", @"TiMidity", @"TinySoundFont (.sf2 软音源)" ]
                       sourceCell:cell
                         selected:^(NSInteger idx) { self->_midiSynth = (MIDISYNTHTYPE)idx; }];
      } else if (indexPath.row == 2) {
         [self scanSoundFonts];
         [self presentChoiceTitle:@"选择 SoundFont 音色库"
                          options:_availSoundFonts
                       sourceCell:cell
                         selected:^(NSInteger idx) {
            self->_soundBank = (idx == 0) ? @"" : self->_availSoundFonts[idx];
         }];
      } else if (indexPath.row == 3) {
         [self presentChoiceTitle:@"选择 RIX OPL 合成内核"
                          options:@[ @"MAME", @"DBFLT (DOSBox 浮点)", @"DBINT (DOSBox 定点)", @"NUKED (高精度周期级 OPL3)" ]
                       sourceCell:cell
                         selected:^(NSInteger idx) { self->_oplCore = (OPLCORE_TYPE)idx; }];
      } else if (indexPath.row == 4) {
         [self presentChoiceTitle:@"选择 RIX OPL 芯片"
                          options:@[ @"OPL2", @"OPL3" ]
                       sourceCell:cell
                         selected:^(NSInteger idx) { self->_oplChip = (OPLCHIP_TYPE)idx; }];
      } else if (indexPath.row == 5) {
         NSArray *rates = @[ @"12429", @"24858", @"44100", @"49716" ];
         [self presentChoiceTitle:@"选择 OPL 采样率"
                          options:rates
                       sourceCell:cell
                         selected:^(NSInteger idx) { self->_oplRate = [rates[idx] intValue]; }];
      } else if (indexPath.row == 6) {
         NSArray *rates = @[ @"22050", @"44100", @"48000", @"49716" ];
         [self presentChoiceTitle:@"选择音频输出采样率"
                          options:rates
                       sourceCell:cell
                         selected:^(NSInteger idx) { self->_sampleRate = [rates[idx] intValue]; }];
      }
   } else if (indexPath.section == 2) {
      if (indexPath.row == 1) {
         NSArray *filters = @[ @"none", @"crt", @"cartoon", @"reverse" ];
         [self presentChoiceTitle:@"选择 SDL_GPU 滤镜"
                          options:filters
                       sourceCell:cell
                         selected:^(NSInteger idx) { self->_gpuFilter = filters[idx]; }];
      } else if (indexPath.row == 2) {
         [self presentChoiceTitle:@"选择 CD 音轨格式"
                          options:@[ @"NONE", @"MP3", @"OGG", @"OPUS" ]
                       sourceCell:cell
                         selected:^(NSInteger idx) { self->_cdType = (CDTYPE)idx; }];
      } else if (indexPath.row == 3) {
         [self presentChoiceTitle:@"选择日志输出级别"
                          options:@[ @"VERBOSE", @"DEBUG", @"INFO", @"WARNING", @"ERROR", @"FATAL" ]
                       sourceCell:cell
                         selected:^(NSInteger idx) { self->_logLevel = (LOGLEVEL)idx; }];
      }
   } else if (indexPath.section == 3) {
      if (indexPath.row == 0) {
         [self onSaveAndRestart];
      } else if (indexPath.row == 1) {
         [self onResetDefaults];
      }
   }
}

@end

static UIViewController *FindTopPresentedViewController(void)
{
   UIWindow *targetWin = nil;
   for (UIScene *scene in [UIApplication sharedApplication].connectedScenes)
   {
      if ([scene isKindOfClass:[UIWindowScene class]])
      {
         UIWindowScene *ws = (UIWindowScene *)scene;
         for (UIWindow *w in ws.windows)
         {
            if (w.isKeyWindow) { targetWin = w; break; }
            if (!targetWin && !w.isHidden) targetWin = w;
         }
      }
      if (targetWin && targetWin.isKeyWindow) break;
   }
   UIViewController *vc = targetWin.rootViewController;
   while (vc.presentedViewController != nil)
   {
      vc = vc.presentedViewController;
   }
   return vc;
}

INT
UTIL_ShowSystemSettingsModal(
   BOOL fInGame,
   int  currentMusicNum
)
{
   __block INT result = 0;
   @autoreleasepool {
      UIViewController *topVC = FindTopPresentedViewController();
      if (!topVC) return 0;

      BOOL prevPaused = g_bRenderPaused;
      g_bRenderPaused = TRUE;

      PALSystemSettingsViewController *settingsVC =
         [[PALSystemSettingsViewController alloc] initWithInGame:fInGame musicNum:currentMusicNum];
      UINavigationController *nav = [[UINavigationController alloc] initWithRootViewController:settingsVC];
      nav.modalPresentationStyle = UIModalPresentationFormSheet;

      [topVC presentViewController:nav animated:YES completion:nil];

      while (!settingsVC.isFinished)
      {
         CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.016, false);
      }
      // Allow dismissal animation to finish cleanly
      for (int i = 0; i < 12; i++)
      {
         CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.016, false);
      }

      result = settingsVC.resultCode;
      g_bRenderPaused = prevPaused;
   }
   return result;
}
