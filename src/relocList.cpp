//--------------------------------------------------------------------
//		L-Packer
//		Amiga & Atari executable packer
//		Written by Arnaud Carré aka Leonard/Oxygene (@leonard_coder)
//		https://github.com/arnaud-carre/L-Packer
//--------------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include "relocList.h"

static int compare(const void* arg1, const void* arg2)
{
	const uint32_t* pA = (const uint32_t*)arg1;
	const uint32_t* pB = (const uint32_t*)arg2;
	return (*pA) - (*pB);
}

void RelocList::Sort()
{
	byteCount = 0;
	wordCount = 0;
	longCount = 0;
	if (relocCount > 0)
	{
		qsort(offsets, relocCount, sizeof(uint32_t), compare);
		uint32_t oldOffset = 0;
		for (int i=0;i<relocCount;i++)
		{
			uint32_t delta = offsets[i] - oldOffset;
			assert(0 == (delta&1));
			assert(delta >= 2);
			if ((delta<2) || (delta&1))
				printf("ERROR Delta value in RelocList::Sort (0x%x)\n", delta);
			oldOffset = offsets[i];
			delta >>= 1;
			if (delta < 256)
				byteCount++;
			else if (delta < 65536)
				wordCount++;
			else
				longCount++;
		}
	}
}

int	RelocList::ComputeOutSize( const RelocList* pLists, int listCount)
{
	int relocSize = 0;
	for (int i = 0; i < listCount; i++)
	{
		uint32_t oldOffset = 0;
		const RelocList& list = pLists[i];
		for (int j = 0; j < list.relocCount; j++)
		{
			uint32_t delta = list.offsets[j] - oldOffset;
			if (0 == delta)
			{
				printf("ERROR: 0 delta in reloc\n");
				return -1;
			}
			if (delta & 1)
			{
				printf("ERROR: odd delta reloc\n");
				return -1;
			}
			oldOffset = list.offsets[j];

			while (delta > 508)
			{
				relocSize++;		// 0x00 byte
				delta -= 508;		// this could be 510, but at the risk the last delta of the loop is 0 (invalid)
			}
			assert(delta > 0);
			delta >>= 1;
			assert(delta < 0xff);
			relocSize++;			// delta byte
		}
		relocSize++;		// 0xff (end marker)
	}
	return relocSize;
}

bool	RelocList::outputData( uint8_t* outBuffer, int bufferSize, const RelocList* pLists, int listCount)
{
	uint8_t* pWrite = outBuffer + bufferSize;

	for (int i = 0; i < listCount; i++)
	{
		uint32_t oldOffset = 0;
		const RelocList& list = pLists[i];
		for (int j = 0; j < list.relocCount; j++)
		{
			uint32_t delta = list.offsets[j] - oldOffset;
			oldOffset = list.offsets[j];
			assert(0 == (delta & 1));
			assert(delta > 0);

			while (delta > 508)
			{
				assert(pWrite > outBuffer);
				*(--pWrite) = 0x0;
				delta -= 508;		// this could be 510, but at the risk the last delta of the loop is 0 (invalid)
			}
			assert(delta > 0);
			delta >>= 1;
			assert(delta < 0xff);
			assert(pWrite > outBuffer);
			*(--pWrite) = delta;
		}
		assert(pWrite > outBuffer);
		*(--pWrite) = 0xff;
	}
	if (pWrite != outBuffer)
		printf("ERROR: (pWrite != outBuffer) during relocation build\n");
	return (pWrite == outBuffer);
}
