#include <stdio.h>
#include <gccore.h>
#include <string.h>
#include <malloc.h>
#include "fs.h"

static bool Identify_GenerateTik(signed_blob **outbuf, u32 *outlen)
{
	signed_blob *buffer = (signed_blob*)memalign(32, STD_SIGNED_TIK_SIZE);
	if(!buffer)
		return false;
	memset(buffer, 0, STD_SIGNED_TIK_SIZE);

	sig_rsa2048 *signature = (sig_rsa2048*)buffer;
	signature->type = ES_SIG_RSA2048;

	tik *tik_data  = (tik *)SIGNATURE_PAYLOAD(buffer);
	strcpy(tik_data->issuer, "Root-CA00000001-XS00000003");
	memset(tik_data->cidx_mask, 0xFF, 32);

	*outbuf = buffer;
	*outlen = STD_SIGNED_TIK_SIZE;

	return true;
}

bool DoESIdentify(void)
{
	u32 tikSize;
	signed_blob *tikBuffer = NULL;

	//printf("Generating fake ticket...");
	if(!Identify_GenerateTik(&tikBuffer, &tikSize))
	{
		printf("Failed generating fake ticket!\n");
		return false;
	}
	//printf("Success!\n");

	char filepath[ISFS_MAXPATH] ATTRIBUTE_ALIGN(32);
	memset(filepath, 0, ISFS_MAXPATH);
	//printf("Reading certs...");
	memset(filepath, 0, ISFS_MAXPATH);
	strcpy(filepath, "/sys/cert.sys");

	u32 certSize = 0;
	u8 *certBuffer = ISFS_GetFile(filepath, &certSize, -1);
	if (certBuffer == NULL || certSize == 0)
	{
		printf("Failed reading certs!\n");
		free(tikBuffer);
		return false;
	}
	//printf("Success!\n");

	//printf("ES_Identify\n");
	u32 keyId = 0;
	extern u8 *tmdBuffer;
	extern u32 tmdSize;
	DCFlushRange(tikBuffer, tikSize);
	DCFlushRange(certBuffer, certSize);
	s32 ret = ES_Identify((signed_blob*)certBuffer, certSize, (signed_blob*)tmdBuffer, tmdSize, tikBuffer, tikSize, &keyId);
	if(ret < 0)
	{
		switch(ret)
		{
			case ES_EINVAL:
				printf("Error! ES_Identify (ret = %li) Data invalid!\n", ret);
				break;
			case ES_EALIGN:
				printf("Error! ES_Identify (ret = %li) Data not aligned!\n", ret);
				break;
			case ES_ENOTINIT:
				printf("Error! ES_Identify (ret = %li) ES not initialized!\n", ret);
				break;
			case ES_ENOMEM:
				printf("Error! ES_Identify (ret = %li) No memory!\n", ret);
				break;
			default:
				printf("Error! ES_Identify (ret = %li)\n", ret);
				break;
		}
	}
	free(tikBuffer);
	free(certBuffer);

	return ret < 0 ? false : true;
}
