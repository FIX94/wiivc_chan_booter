/*
 * Copyright (C) 2017 FIX94
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */
#include <gccore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wiiuse/wpad.h>
#include <ogc/isfs.h>
#include <ogc/machine/processor.h>
#include <malloc.h>
#include <unistd.h>
#include "ChannelHandler.hpp"
#include "identify.h"
#include "fs.h"
#include "wdvd.h"

//comment in if you want to boot the channel in 4:3
//#define BOOT_FORCE_4_BY_3 1
//comment in if you want to boot from a title id you edited in below
//#define BOOT_FROM_CODE_TITLEID 1

u64 title = 0x0001000100000000ULL;

//used externally
u8 *tmdBuffer = NULL;
u32 tmdSize = 0;

//patches for channel booting without ES_LaunchTitle
static const u8 isfs_perm_wiivc_old[] = { 0x42, 0x9F, 0xD1, 0x03, 0x20, 0x00, 0xBD, 0xF0, 0x09, 0x8B, 0xE7, 0xF8, 0x20, 0x66 };
static const u8 isfs_perm_wiivc_patch[] = { 0x42, 0x9F, 0x46, 0xC0, 0x20, 0x00, 0xBD, 0xF0, 0x09, 0x8B, 0xE7, 0xF8, 0x20, 0x66 };
static const u8 setuid_old[] = { 0xD1, 0x2A, 0x1C, 0x39 };
static const u8 setuid_patch[] = { 0x46, 0xC0, 0x1C, 0x39 };
static const u8 es_identify_old[] = { 0x28, 0x03, 0xD1, 0x23 };
static const u8 es_identify_patch[] = { 0x28, 0x03, 0x00, 0x00 };

#define ALIGN32(x) (((x) + 31) & ~31)
#define TITLE_UPPER(x)		((u32)((x) >> 32))
#define TITLE_LOWER(x)		((u32)(x) & 0xFFFFFFFF)

static inline bool apply_patch(const char *name, const u8 *old, const u8 *patch, u32 size)
{
	u8 i;
	u32 found = 0;
	u8 *ptr = (u8*)0x93400000;

	u32 level = IRQ_Disable();
	while((u32)ptr < (u32)0x94000000)
	{
		if(memcmp(ptr, old, size) == 0)
		{
			for(i = 0; i < size; ++i)
				*(vu8*)(ptr+i) = *(vu8*)(patch+i);
			found++;
		}
		ptr++;
	}
	IRQ_Restore(level);

	//printf("patched %s %lu times.\n", name, found);
	return (found > 0);
}

extern "C" { extern void __exception_closeall(); };
u32 AppEntrypoint = 0;

int main(int argc, char *argv[]) 
{
	void *xfb = NULL;
	GXRModeObj *rmode = NULL;
	VIDEO_Init();
	rmode = VIDEO_GetPreferredMode(NULL);
	xfb = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
	VIDEO_Configure(rmode);
	VIDEO_SetNextFramebuffer(xfb);
	VIDEO_SetBlack(FALSE);
	VIDEO_Flush();
	VIDEO_WaitVSync();
	if(rmode->viTVMode&VI_NON_INTERLACE) VIDEO_WaitVSync();
	VIDEO_Configure(rmode);
	int x = 24, y = 32, w, h;
	w = rmode->fbWidth - (32);
	h = rmode->xfbHeight - (48);
	CON_InitEx(rmode, x, y, w, h);
	VIDEO_ClearFrameBuffer(rmode, xfb, COLOR_BLACK);
#ifndef BOOT_FROM_CODE_TITLEID
	if(WDVD_Init() != 0)
	{
		printf("The Wii VC Disc could not be initialized!\n");
		sleep(3);
		return -1;
	}
	if(WDVD_OpenDataPartition() != 0)
	{
		printf("Found no Partition on Wii VC Disc!\n");
		sleep(3);
		return -2;
	}
	if(!WDVD_FST_Mount())
	{
		printf("Unable to open Partition on Wii VC Disc!\n");
		sleep(3);
		return -3;
	}
	if(WDVD_FST_Open("title.txt") != 0)
	{
		printf("No title.txt on Wii VC Disc!\n");
		sleep(3);
		return -4;
	}
	if(WDVD_FST_Read(wdvdTmpBuf, 4) != 4)
	{
		printf("Cant read title.txt!");
		sleep(3);
		return -5;
	}
	memcpy(((u8*)(&title))+4, wdvdTmpBuf, 4);
	WDVD_FST_Close();
	WDVD_FST_Unmount();
	WDVD_Close();
#endif
	apply_patch("isfs_permissions_wiivc", isfs_perm_wiivc_old, isfs_perm_wiivc_patch, sizeof(isfs_perm_wiivc_patch));
	apply_patch("es_setuid", setuid_old, setuid_patch, sizeof(setuid_patch));
	apply_patch("es_identify", es_identify_old, es_identify_patch, sizeof(es_identify_patch));

	ISFS_Initialize();
	
	u8 reqIOS = 0;

	char tmd[ISFS_MAXPATH] ATTRIBUTE_ALIGN(32);
	sprintf(tmd, "/title/%08lx/%08lx/content/title.tmd", TITLE_UPPER(title), TITLE_LOWER(title));
	tmdBuffer = ISFS_GetFile(tmd, &tmdSize, -1);

	if(tmdBuffer == NULL)
	{
		printf("No Title TMD!\n");
		ISFS_Deinitialize();
		sleep(3);
		return 0;
	}
	else if(tmdSize < 0x1E4)
	{
		free(tmdBuffer);
		printf("Too small Title TMD!\n");
		ISFS_Deinitialize();
		sleep(3);
		return 0;
	}

	reqIOS = tmdBuffer[0x18B];
	//printf("Requested Game IOS: %i\n", reqIOS);
	IOS_ReloadIOS(reqIOS);

	if(!DoESIdentify())
	{
		free(tmdBuffer);
		printf("DoESIdentify failed!\n");
		ISFS_Deinitialize();
		sleep(3);
		return 0;
	}

	AppEntrypoint = LoadChannel(title, false);
	//printf("Entrypoint: %08lx AHBPROT: %08lx\n", AppEntrypoint, *(vu32*)0xCD800064);
	//sleep(3);
	free(tmdBuffer);

#ifdef BOOT_FORCE_4_BY_3
	write32(0xd8006a0, 0x30000002);
	mask32(0xd8006a8, 0, 2);
#endif

	/* Set black and flush */
	VIDEO_SetBlack(TRUE);
	VIDEO_Flush();
	VIDEO_WaitVSync();
	if(rmode->viTVMode & VI_NON_INTERLACE)
		VIDEO_WaitVSync();
	else while(VIDEO_GetNextField())
		VIDEO_WaitVSync();

	/* Shutdown IOS subsystems */
	ISFS_Deinitialize();
	u32 level = IRQ_Disable();
	__IOS_ShutdownSubsystems();
	__exception_closeall();

	/* Originally from tueidj - taken from NeoGamma (thx) */
	*(vu32*)0xCC003024 = 1;

	if(AppEntrypoint == 0x3400)
	{
		asm volatile (
			"isync\n"
			"lis %r3, AppEntrypoint@h\n"
			"ori %r3, %r3, AppEntrypoint@l\n"
			"lwz %r3, 0(%r3)\n"
			"mtsrr0 %r3\n"
			"mfmsr %r3\n"
			"li %r4, 0x30\n"
			"andc %r3, %r3, %r4\n"
			"mtsrr1 %r3\n"
			"rfi\n"
		);
	}
	else
	{
		asm volatile (
			"lis %r3, AppEntrypoint@h\n"
			"ori %r3, %r3, AppEntrypoint@l\n"
			"lwz %r3, 0(%r3)\n"
			"mtlr %r3\n"
			"blr\n"
		);
	}
	IRQ_Restore(level);

	return 0;
}
