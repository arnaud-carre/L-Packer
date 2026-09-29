//--------------------------------------------------------------------
//		L-Packer
//		Amiga & Atari executable packer
//		Written by Arnaud Carré aka Leonard/Oxygene (@leonard_coder)
//		https://github.com/arnaud-carre/L-Packer
//--------------------------------------------------------------------

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <string.h>
#include "byteStream.h"
#include "amigaExePack.h"

#define b2k(v)	(((v)+1023)>>10)
const char* sHunkTxt[2] = {"CHIP","ANY "};
const char* sHunkTypeTxt[4] = {"Unknown","Code","Data","BSS "};

AmigaExePack::AmigaExePack()
{
	m_hunkCount = 0;
}

AmigaExePack::~AmigaExePack()
{
	Release();
}

void AmigaExePack::Release()
{
	for (uint32_t h = 0; h < m_hunkCount; h++)
		m_hunks[h].Release();
	m_hunkCount = 0;
}

bool AmigaExePack::Parse(ByteStream& bs, bool verbose)
{

	uint32_t sig = bs.ReadLong();
	if (0x3f3 != sig)
	{
		printf("ERROR: Probably not an Amiga ELF file\n");
		return false;
	}

	uint32_t cc = bs.ReadLong();
	if ( cc != 0 )
	{
		printf("ERROR: L-Packer does not support resident library names\n");
		return false;
	}

	uint32_t hunkCount = bs.ReadLong();
	assert(hunkCount > 0);
	if ( hunkCount > kMaxHunk )
	{
		printf("ERROR: Too many hunks (%d)\n", hunkCount);
		return false;
	}

	m_hunkCount = hunkCount;
	uint32_t firstHunk = bs.ReadLong();
	uint32_t lastHunk = bs.ReadLong();
	if (( firstHunk != 0 ) || ( lastHunk != m_hunkCount-1))
	{
		printf("ERROR: Hunks should be consecutive\n");
		return false;
	}

	for (uint32_t h=0;h<m_hunkCount;h++)
	{
		uint32_t hs = bs.ReadLong();
		Hunk& hunk = m_hunks[h];
		hunk.m_allocSize = (hs & 0x3fffffff)*4;
		hunk.m_dataSize = 0;
		hunk.m_memType = (hs&(1 << 30))?eMemoryType::eChipMemory : eMemoryType::eAnyMemory;
		hunk.m_id = h;
	}

	uint32_t nextHunkId = 0;
	Hunk* pHunk = nullptr;

	while (!bs.eof())
	{
		uint32_t hid = bs.ReadLong()&0x3fffffff;

		switch (hid)
		{
			case 0x3e9:
			case 0x3ea:
			case 0x3eb:
			{
				// next hunk
				if (nextHunkId >= m_hunkCount)
				{
					printf("ERROR: Hunk number mismatch (too much)\n");
					return false;
				}
				uint32_t diskSize = bs.ReadLong() << 2;
				pHunk = &m_hunks[nextHunkId];
				pHunk->m_id = nextHunkId;

				if (0x3eb == hid)
				{
					pHunk->m_hunkType = eHunkBss;
				}
				else
				{
					if (hid == 0x3e9)
						pHunk->m_hunkType = eHunkCode;
					else
						pHunk->m_hunkType = eHunkData;

					if ( diskSize > pHunk->m_allocSize )
					{
						printf("ERROR: Hunk #%d disk size greater than allocated buffer\n", nextHunkId);
						return false;
					}
					pHunk->m_dataSize = diskSize;
					// Alloc
					pHunk->m_data = (uint8_t *)malloc(pHunk->m_dataSize);
					memset(pHunk->m_data, 0, pHunk->m_dataSize);

					// read from elf
					const void* dataIn = bs.GetData(diskSize);
					if ( nullptr == dataIn )
					{
						printf("ERROR: unable to read %d bytes from Amiga elf\n", diskSize);
						return false;
					}
					memcpy(pHunk->m_data, dataIn, diskSize);
					bs.SeekForward(diskSize);
				}

				nextHunkId++;
			}
			break;

			case 0x3ec:
			{
				assert(pHunk);

				if (( pHunk->m_hunkType != eHunkCode ) && ( pHunk->m_hunkType != eHunkData ))
				{
					printf("ERROR: Hunk #%d (%s) has unexpected relocation table!\n", pHunk->m_id, sHunkTypeTxt[pHunk->m_hunkType]);
					return false;
				}

				int relocList = 0;
				while (!bs.eof())
				{
					int relocCount = bs.ReadLong();
					if ( relocCount > kMaxReloc )
					{
						printf("ERROR: Too many relocation offsets (%d)\n", relocCount);
						return false;
					}
					if (0 == relocCount)
						break;

					pHunk->m_totalRelocCount += relocCount;
					assert(pHunk->m_relocListCount < kMaxHunk);
					if (pHunk->m_relocListCount >= kMaxHunk)
						return false;
					RelocList& rl = pHunk->m_relocLists[pHunk->m_relocListCount];
					assert(0 == rl.relocCount);
					rl.referToHunkId = bs.ReadLong();
					if (rl.referToHunkId >= m_hunkCount)
					{
						printf("ERROR: Unexpected reloc hunk id reference (%d)\n", rl.referToHunkId);
						return false;
					}
					for (int o = 0; o < relocCount; o++)
					{
						if (!rl.Push(bs.ReadLong()))
							return false;
					}
					pHunk->m_relocListCount++;
				}
			}
			break;
			case 0x3f0:
				if (verbose)
					printf("Skip HUNK_SYMBOL ( $3f0 )\n");
				while (!bs.eof())
				{
					uint32_t stringLen = bs.ReadLong();
					if (0 == stringLen)
						break;
					bs.SeekForward(stringLen * 4);
					bs.ReadLong(); // skip offset
				}
				break;
			case 0x3f1:
			{
				if (verbose)
					printf("Skip HUNK_DEBUG ( $3f1 )\n");
				uint32_t dwordCount = bs.ReadLong();
				bs.SeekForward(dwordCount*4);
			}
				break;
			case 0x3f2:
				assert(pHunk);
				pHunk = nullptr;
				break;
			default:
				printf("ERROR: Unknown hunk $%08x\n", hid);
				return false;
				break;

		}

	}
	if ( pHunk )
	{
		printf("ERROR: A hunk is still open during parsing\n");
		return false;
	}
	if ( nextHunkId != m_hunkCount)
	{
		printf("ERROR: Hunk number mismatch (not enough)\n");
		return false;
	}

	if ( !bs.eof())
	{
		printf("ERROR: Parsing is not at the end of the file\n");
		return false;
	}

	return true;
}

const Hunk& AmigaExePack::GetHunk(uint32_t hunkId) const
{
	assert(hunkId < m_hunkCount);
	return m_hunks[hunkId];
}

bool AmigaExePack::Load(ByteStream& binFile, bool verbose)
{
	printf("Original size.....: %dKiB (%d bytes)\n", (binFile.GetSize() + 1023) >> 10, binFile.GetSize());

	// parsing ELF
	if (!Parse(binFile, verbose))
		return false;

	return true;
}

void AmigaExePack::Print()
{
	printf("Elf is %d hunk(s)\n", m_hunkCount);
	printf("           Type  Mem      Disk    Alloc  Reloc\n");
	for (uint32_t h = 0; h < m_hunkCount; h++)
	{
		const Hunk& hunk = m_hunks[h];
		assert(hunk.m_id == h);
		printf("Hunk #%02d:  %s  %s  %7d  %7d", h, sHunkTypeTxt[int(hunk.m_hunkType)], sHunkTxt[int(hunk.m_memType)], hunk.m_dataSize, hunk.m_allocSize);
		if ( hunk.m_relocListCount > 0 )
		{
			printf("  %3d  (", hunk.m_totalRelocCount);
			for (int i=0;i<hunk.m_relocListCount;i++)
				printf("#%d ", hunk.m_relocLists[i].referToHunkId);
			printf(")");
		}
		printf("\n");
	}
}
