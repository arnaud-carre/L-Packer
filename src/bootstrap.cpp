//--------------------------------------------------------------------
//		L-Packer
//		Amiga & Atari executable packer
//		Written by Arnaud Carré aka Leonard/Oxygene (@leonard_coder)
//		https://github.com/arnaud-carre/L-Packer
//--------------------------------------------------------------------
				
#include <stdio.h>
#include <string.h>		// memcmp
#include <vector>
#include "bootstrap.h"
#include "bootstrap_data_gen.h"

static const Bootstrap sDataBootstrap =
{
	eAtari, kLz4, true, false, nullptr, 3 * 4		// dummy kLz4 and 12 bytes header for data mode
};

// bootstrap_builder and its assembly sources are not part of this checkout.
// Wrap the generated Amiga stubs without changing their depackers. Save the
// CLI length (D0) and pointer (A0) before text output or decompression, then
// restore them after CacheClearU, with the original entry stack intact.
static const uint8_t sAmigaEntry[] =
{
	0x48, 0xe7, 0x80, 0x80, // movem.l d0/a0,-(sp)
	0x60, 0x0a,             // bra.s original_bootstrap
	0x58, 0x8f,             // addq.l #4,sp (discard second hunk entry)
	0x22, 0x5f,             // movea.l (sp)+,a1 (first hunk entry)
	0x4c, 0xdf, 0x01, 0x01, // movem.l (sp)+,d0/a0
	0x4e, 0xd1,             // jmp (a1)
};

static void WriteWord(uint8_t* data, int value)
{
	data[0] = uint16_t(value) >> 8;
	data[1] = uint16_t(value) & 255;
}

static bool WrapAmigaBootstrap(const Bootstrap& source, std::vector<uint8_t>& data)
{
	// These offsets describe the common loader, before the algorithm-specific
	// depacker. Validate the instructions so regenerated stubs fail safely.
	const int start = source.text ? 20 : 0;
	const int reloc = start + (source.algo == kZx0 ? 42 : 44);
	const int exit = start + (source.algo == kZx0 ? 118 : 120);
	const uint8_t tail[] = {0x58, 0x4f, 0x4e, 0x75, 'L', 'P', 'K', '!'};
	const uint8_t* code = source.data;
	if (source.size < uint32_t(exit + sizeof(tail)) ||
		code[start] != 0x4d || code[start + 1] != 0xfa ||
		code[reloc] != 0x4b || code[reloc + 1] != 0xfa ||
		(code[start + 2] * 256 + code[start + 3]) != (65536 - start - 2) ||
		(code[reloc + 2] * 256 + code[reloc + 3]) != (65536 - reloc - 2) ||
		memcmp(code + exit, tail, sizeof(tail)) != 0)
		return false;

	data.assign(sAmigaEntry, sAmigaEntry + sizeof(sAmigaEntry));
	data.insert(data.end(), code, code + source.size);
	uint8_t* body = data.data() + sizeof(sAmigaEntry);
	// Both LEAs must still find the segment link immediately before the new
	// entry. All other PC-relative references, including text, move together.
	WriteWord(body + start + 2, -int(sizeof(sAmigaEntry)) - start - 2);
	WriteWord(body + reloc + 2, -int(sizeof(sAmigaEntry)) - reloc - 2);
	WriteWord(body + exit, 0x6000); // bra.w to the restore code at offset 6
	WriteWord(body + exit + 2, 6 - int(sizeof(sAmigaEntry)) - exit - 2);
	return true;
}

static const int kBootstrapCount = sizeof(sBootstraps) / sizeof(sBootstraps[0]);

struct BootstrapTable
{
	Bootstrap entries[kBootstrapCount];
	std::vector<uint8_t> data[kBootstrapCount];

	BootstrapTable()
	{
		for (int i = 0; i < kBootstrapCount; i++)
		{
			entries[i] = sBootstraps[i];
			if (entries[i].platform != eAmiga)
				continue;
			if (!WrapAmigaBootstrap(sBootstraps[i], data[i]))
			{
				entries[i].data = nullptr;
				continue;
			}
			entries[i].data = data[i].data();
			entries[i].size = uint32_t(data[i].size());
		}
	}
};

const Bootstrap* GetRightBootstrap(Platform_e platform, ePackingAlgorithm_t algo, bool flash, bool text)
{
	if (Platform_e::eData == platform)
		return &sDataBootstrap;

	static const BootstrapTable table;
	const Bootstrap* it = table.entries;
	for (int i=0;i<kBootstrapCount;i++)
	{
		if ((it->platform == platform) &&
			(it->algo == algo) &&
			(it->flash == flash) &&
			(it->text == text))
		{
			if (it->data == nullptr)
			{
				printf("ERROR: Unsupported Amiga bootstrap layout\n");
				return nullptr;
			}
			return it;
		}
		it++;
	}
	printf("ERROR: No bootstrap found!\n");
	return nullptr;
}

bool IsLPackerExecutable(const uint8_t* data, int size, Platform_e platform)
{
	if (platform == eAmiga)
	{
		// Recognize both old stubs and wrapped stubs, including -text output.
		// Skip the text prefix because its length placeholder is patched.
		for (int i = 0; i < kBootstrapCount; i++)
		{
			const Bootstrap& original = sBootstraps[i];
			if (original.platform != eAmiga)
				continue;
			const int offset = original.text ? 20 : 0;
			if (size >= offset + 28 &&
				memcmp(data + offset, original.data + offset, 28) == 0)
				return true;
			const Bootstrap* wrapped = GetRightBootstrap(eAmiga, original.algo,
				original.flash, original.text);
			const int wrappedOffset = int(sizeof(sAmigaEntry)) + offset;
			if (wrapped && size >= wrappedOffset + 28 &&
				memcmp(data, sAmigaEntry, sizeof(sAmigaEntry)) == 0 &&
				memcmp(data + wrappedOffset, wrapped->data + wrappedOffset, 28) == 0)
				return true;
		}
		return false;
	}
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
