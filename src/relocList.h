//--------------------------------------------------------------------
//		L-Packer
//		Amiga & Atari executable packer
//		Written by Arnaud Carré aka Leonard/Oxygene (@leonard_coder)
//		https://github.com/arnaud-carre/L-Packer
//--------------------------------------------------------------------

#pragma once
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>

static const uint32_t kMaxReloc = 16384;

struct RelocList
{
	RelocList()
	{
		relocCount = 0;
		offsets = (uint32_t*)malloc(kMaxReloc * sizeof(uint32_t));
	};
	~RelocList()
	{
		free(offsets);
	}

	void Sort();

	int relocCount;
	uint32_t referToHunkId;
	uint32_t* offsets;

	bool Push(uint32_t v)
	{
		assert(relocCount < kMaxReloc);
		assert(offsets);
		if (relocCount < kMaxReloc)
		{
			offsets[relocCount] = v;
			relocCount++;
			return true;
		}
		else
			return false;
	}

	static int ComputeOutSize(const RelocList* pLists, int listCount);
	static bool outputData(uint8_t* outBuffer, int bufferSize, const RelocList* pLists, int listCount);

	uint32_t byteCount;
	uint32_t wordCount;
	uint32_t longCount;
};

