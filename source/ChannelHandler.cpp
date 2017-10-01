/***************************************************************************
 * Copyright (C) 2010
 * by dude
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any
 * damages arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any
 * purpose, including commercial applications, and to alter it and
 * redistribute it freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you
 * must not claim that you wrote the original software. If you use
 * this software in a product, an acknowledgment in the product
 * documentation would be appreciated but is not required.
 *
 * 2. Altered source versions must be plainly marked as such, and
 * must not be misrepresented as being the original software.
 *
 * 3. This notice may not be removed or altered from any source
 * distribution.
 *
 * Channel Launcher Class
 *
 * for WiiXplorer 2010
 ***************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ogcsys.h>
#include <unistd.h>
#include <malloc.h>

#include "ChannelHandler.hpp"
#include "fs.h"
#include "lz77.h"

using namespace std;

void *dolchunkoffset[18];
u32	dolchunksize[18];
u32	dolchunkcount;
u32 bootcontent;

char filepath[ISFS_MAXPATH] ATTRIBUTE_ALIGN(32);

#define TITLE_UPPER(x)		((u32)((x) >> 32))
#define TITLE_LOWER(x)		((u32)(x) & 0xFFFFFFFF)

static u8 *GetDol(u32 bootcontent, u64 title)
{
	memset(filepath, 0, ISFS_MAXPATH);
	sprintf(filepath, "/title/%08lx/%08lx/content/%08lx.app", TITLE_UPPER(title), TITLE_LOWER(title), bootcontent);

	u32 contentSize = 0;

	u8 *data = ISFS_GetFile(filepath, &contentSize, -1);
	if(data != NULL && contentSize != 0)
	{
		if(isLZ77compressed(data))
		{
			u8 *decompressed;
			u32 size = 0;
			if(decompressLZ77content(data, contentSize, &decompressed, &size) < 0)
			{
				free(data);
				return NULL;
			}
			free(data);
			data = decompressed;
		}
		return data;
	}
	return NULL;
}

static bool GetAppNameFromTmd(bool dol, u32 *bootcontent)
{
	bool ret = false;

	extern u8 *tmdBuffer;
	_tmd *tmd_file = (_tmd *)SIGNATURE_PAYLOAD((u32*)tmdBuffer);
	for(u16 i = 0; i < tmd_file->num_contents; ++i)
	{
		if(tmd_file->contents[i].index == (dol ? 0x01 : tmd_file->boot_index))
		{
			*bootcontent = tmd_file->contents[i].cid;
			ret = true;
			break;
		}
	}

	return ret;
}

static u32 MoveDol(u8 *buffer)
{
	dolchunkcount = 0;
	dolheader *dolfile = (dolheader *)buffer;

	if(dolfile->bss_start)
	{
		if(!(dolfile->bss_start & 0x80000000))
			dolfile->bss_start |= 0x80000000;

		memset((void *)dolfile->bss_start, 0, dolfile->bss_size);
		DCFlushRange((void *)dolfile->bss_start, dolfile->bss_size);
		ICInvalidateRange((void *)dolfile->bss_start, dolfile->bss_size);
	}

	for(u8 i = 0; i < 18; i++)
	{
		if(!dolfile->section_size[i]) 
			continue;
		if(dolfile->section_pos[i] < sizeof(dolheader)) 
			continue;
		if(!(dolfile->section_start[i] & 0x80000000)) 
			dolfile->section_start[i] |= 0x80000000;

		dolchunkoffset[dolchunkcount] = (void *)dolfile->section_start[i];
		dolchunksize[dolchunkcount] = dolfile->section_size[i];

		memmove(dolchunkoffset[dolchunkcount], buffer + dolfile->section_pos[i], dolchunksize[dolchunkcount]);
		dolchunkcount++;
	}
	return dolfile->entry_point;
}

u32 LoadChannel(u64 title, bool dol)
{
	u32 entry = 0;

	GetAppNameFromTmd(dol, &bootcontent);
	u8 *data = GetDol(bootcontent, title);
	entry = MoveDol(data);
	free(data);

	return entry;
}
