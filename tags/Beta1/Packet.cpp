#include "Packet.h"


int Packet::Checksum()
{
	int headerChecksum;
	int dataChecksum;
	int i;
	int* data = (&header.checksum)+1;

	// Checksum the pre-checksum fields
	headerChecksum = header.sourcePlayerNetID + header.destPlayerNetID;
	headerChecksum += ((int)header.sizeOfPayload | ((int)header.type << 8));
	headerChecksum ^= 0xFDE24ACB;
	// Checksum the post-checksum fields
	dataChecksum = 0;
	// Handle DWORD at a time
	i = header.sizeOfPayload / 4;
	for (; i > 0; --i)
	{
		dataChecksum += *data;
		data++;
	}
	// Checksum remaining bytes
	i = header.sizeOfPayload & 3;
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
