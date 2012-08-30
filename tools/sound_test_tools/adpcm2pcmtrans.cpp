#include <iostream>
#include <fstream>
#include <string.h>
using namespace std;

typedef unsigned char byte;

const int stepTable[] = { 7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19,
		21, 23, 25, 28, 31, 34, 37, 41, 45, 50, 55, 60, 66, 73, 80, 88, 97,
		107, 118, 130, 143, 157, 173, 190, 209, 230, 253, 279, 307, 337,
		371, 408, 449, 494, 544, 598, 658, 724, 796, 876, 963, 1060, 1166,
		1282, 1411, 1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024, 3327,
		3660, 4026, 4428, 4871, 5358, 5894, 6484, 7132, 7845, 8630, 9493,
		10442, 11487, 12635, 13899, 15289, 16818, 18500, 20350, 22385,
		24623, 27086, 29794, 32767 };

const int indexAdjust[] = { -1, -1, -1, -1, 2, 4, 6, 8 };

static byte *encodePCMToADPCM(byte *adpcm, short *raw, int len, int &sample,int &index) 
{
	short *pcm = raw;
	int cur_sample;
	int i;
	int delta;
	int sb;
	int code;

	// Log.d("wild3", "len:"+len+",pcmlen:"+pcm.length);
	len >>= 1;

	// int pre_sample = refsample.getValue();
	// int index = refindex.getValue();

	for (i = 0; i < len; i++) {
		cur_sample = pcm[i]; //
		// Log.d("wild3", "cur_sample:"+cur_sample);
		delta = cur_sample - sample; //
		if (delta < 0) {
			delta = -delta;
			sb = 8; //
		} else {
			sb = 0;
		} //
		code = 4 * delta / stepTable[index]; //
		if (code > 7)
			code = 7; //

		delta = (stepTable[index] * code) / 4 + stepTable[index] / 8; //
		if (sb > 0)
			delta = -delta;
		sample += delta; //
		if (sample > 32767)
			sample = 32767;
		else if (sample < -32768)
			sample = -32768;

		index += indexAdjust[code]; //
		if (index < 0)
			index = 0; //
		else if (index > 88)
			index = 88;

		if ((i & 0x01) == 0x01)
			adpcm[(i >> 1)] |= code | sb;
		else
			adpcm[(i >> 1)] = (byte) ((code | sb) << 4); //
	}
	return adpcm;
}

static short *decodeADPCMToPCM(short *raw, byte *adpcm, int len, int &sample,int &index)
{
	int i;
	int code;
	int sb;
	int delta;
	// short[] pcm = new short[len * 2];
	len <<= 1;

	for (i = 0; i < len; i++) {
		if ((i & 0x01) != 0)
			code = adpcm[i >> 1] & 0x0f;
		else
			code = adpcm[i >> 1] >> 4;
		if ((code & 8) != 0)
			sb = 1;
		else
			sb = 0;
		code &= 7;

		delta = (stepTable[index] * code) / 4 + stepTable[index] / 8;
		if (sb != 0)
			delta = -delta;
		sample += delta;
		if (sample > 32767)
			sample = 32767;
		else if (sample < -32768)
			sample = -32768;
		raw[i] = (short)sample;
		index += indexAdjust[code];
		if (index < 0)
			index = 0;
		if (index > 88)
			index = 88;
	}

	return raw;
}
int main( int argn, char *argc[])
{
	int mode;

	if( argn < 4 )
		return -1;

	if( strcmp(argc[1],"-d" ) == 0 ) mode = 0;	// decode
	else if( strcmp(argc[1], "-e" ) == 0 ) mode = 1; //encode
	else mode = 0;

	char *in_filename = argc[2];
	char *out_filename = argc[3];

	ifstream f(in_filename,ios::binary|ios::in);
	ofstream out(out_filename,ios::binary|ios::out);

	if( !f || !out )
	{
		cout << "Can not open input or output file!" << endl;
		return -1;
	}

	short data[2];
	byte adpcm[1];
	int sample = 0, index = 0;
	int cnt = 0;
	if( mode )
	{
		while( !f.eof() )
		{
			f.read( (char *)data, 4 );
			encodePCMToADPCM( adpcm, data, 4, sample, index );
			out.write((char *)adpcm, 1); 
			cnt++;
		}
	}
	else
	{
		while( !f.eof() )
		{
			f.read((char *)adpcm, 1 );
			decodeADPCMToPCM( data, adpcm, 1, sample, index );
			out.write((char *)data, 4); 
			cnt++;
		}
	}
	cout << cnt << endl;

	f.close();
	out.close();
	
	return 0;
}
