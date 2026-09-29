//--------------------------------------------------------------------
//		L-Packer
//		Amiga & Atari executable packer
//		Written by Arnaud Carré aka Leonard/Oxygene (@leonard_coder)
//		https://github.com/arnaud-carre/L-Packer
//--------------------------------------------------------------------

#pragma once
#include <stdint.h>
#include "amigaExePack.h"
#include "byteStream.h"

struct Parameters;

struct PackedBlob
{
	uint32_t originalSize = 0;
	uint32_t packedSize = 0;
	int packTimeMs = 0;
	ByteStream data;

	void	Copy(const PackedBlob& in)
	{
		originalSize = in.originalSize;
		packedSize = in.packedSize;
		packTimeMs = in.packTimeMs;
		data.AddStream(in.data);
		data.Seek(0);
	}

};

class	lpackElf
{
public:

	lpackElf();

	bool Build(const AmigaExePack& elf, const Parameters& params);
	bool Save(const char* fname, const Parameters& params);
	void Print(bool verbose);


private:
	struct lpackHunk
	{
		lpackHunk();
		~lpackHunk();
		uint32_t m_dataSize;
		uint32_t m_allocatedSize;
		int m_relocSize;
		uint8_t* m_data;
		RelocList m_relocList[2];
		void TweakOffset(uint32_t baseOffset, uint32_t otherOffset);
	};

	bool buildMergeOffset(const AmigaExePack& elf);
	void updateMergeOffsets(const AmigaExePack& elf, eHunkType t, uint32_t* lpackHunkPos, bool containsByte);
	bool hunkMerge(const AmigaExePack& elf, const Hunk& hunk);
	bool insertReloc();
	int memToSlot(eMemoryType m) const { return (m == m_codeMemType) ? 0 : 1; }
	bool slotIsChip(int slot) const { return m_slotIsChip[slot]; }
	eMemoryType slotToMem(int slot) const { return slotIsChip(slot)?eChipMemory:eAnyMemory; };

	uint32_t m_mergeOffsets[kMaxHunk];
	lpackHunk m_hunks[2];
	eMemoryType m_codeMemType;
	bool m_slotIsChip[2];
};