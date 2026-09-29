//--------------------------------------------------------------------
//		L-Packer
//		Amiga & Atari executable packer
//		Written by Arnaud Carré aka Leonard/Oxygene (@leonard_coder)
//		https://github.com/arnaud-carre/L-Packer
//--------------------------------------------------------------------
				
#include <stdio.h>
#include <string.h>		// memcmp
#include "bootstrap.h"
#include "bootstrap_data_gen.h"

static const Bootstrap sDataBootstrap =
{
	eAtari, kLz4, true, false, nullptr, 3 * 4		// dummy kLz4 and 12 bytes header for data mode
};

const Bootstrap* GetRightBootstrap(Platform_e platform, ePackingAlgorithm_t algo, bool flash, bool text)
{
	if (Platform_e::eData == platform)
		return &sDataBootstrap;

	const int kCount = sizeof(sBootstraps) / sizeof(sBootstraps[0]);
	const Bootstrap* it = sBootstraps;
	for (int i=0;i<kCount;i++)
	{
		if ((it->platform == platform) &&
			(it->algo == algo) &&
			(it->flash == flash) &&
			(it->text == text))
		{
			return it;
		}
		it++;
	}
	printf("ERROR: No bootstrap found!\n");
	return nullptr;
}

bool IsLPackerExecutable(const uint8_t* data, int size, Platform_e platform)
{
	struct HeaderCheck
	{
		int offset;
		int size;
	};

	static const HeaderCheck hc[eMaxPlatform] =
	{
		{ 8, 6 * 2 },
		{ 0, 14 * 2 },
		{0,0},
	};

	const HeaderCheck& h = hc[platform];
	if (size > h.offset + h.size)
	{
		const Bootstrap* bs = GetRightBootstrap(platform, kZx0, false, false);
		if (0 == memcmp(bs->data+h.offset, data+h.offset, h.size))
			return true;
	}
	return false;
}
