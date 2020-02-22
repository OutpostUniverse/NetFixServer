#include "Packet.h"


int Packet::Checksum()
{
	// Checksum the pre-checksum fields
	int headerChecksum = header.sourcePlayerNetID + header.destPlayerNetID;
	headerChecksum += ((int)header.sizeOfPayload | ((int)header.type << 8));
	headerChecksum ^= 0xFDE24ACB;

	// Checksum the post-checksum fields
	int dataChecksum = 0;
	int* data = (&header.checksum) + 1;

	// Handle DWORD at a time
	for (int i = header.sizeOfPayload / 4; i > 0; --i)
	{
		dataChecksum += *data;
		data++;
	}

	// Checksum remaining bytes
	int i = header.sizeOfPayload & 3;
	if (i > 0)
	{
		int temp = 0;
		char* byte = (char*)data;
		char* dest = (char*)&temp;
		for (; i > 0; --i)
		{
			*dest++ = *byte++;
		}
		dataChecksum += temp;
		dataChecksum ^= 0xFDE24ACB;
	}

	return headerChecksum + dataChecksum;
}
