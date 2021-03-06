#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

const char help_string[] = "gba2bmp\n\
\n\
This command line utility is meant to ease the process of editing tilesets\n\
within GBA games. The tiles are an 8x8 array of indexes which are 4-bit each,\n\
ordered from top-left to bottom-right to form an image. Thus, each tile are\n\
32 bytes each and are typically stored as compressed lz77 data. Though this\n\
utility is not meant for compressing / decompressing, there are plenty of other\n\
utilities for that (such as GBAmdc or LZ77Restructor2).\n\
\n\
However, the tileset is sometimes not arranged in the order we would like them\n\
in... this makes it very troublesome to manually arrange them within an image\n\
editor and having to manually copy each tile to the correct index is error\n\
prone. One could indeed fix the code itself to have the tileset in a more\n\
logical order... but that simply means more work. Thus, this utility aims to\n\
replace manual (and tedious) labor with cold hard CPU computation.\n\
\n\
Unfortunately, we still have to generate a map of the indexes for the tiles...\n\
However, that map will be generated regardless whether you use this utility or\n\
not. Furthermore, it being a command-line utility means it can be automated\n\
through a batch script.\n\
\n\
This utility relies on a \"map\" file, which is simply a plaintext file with a\n\
list of indexes. The syntax is as such:\n\
\n\
x, y;\n\
a[1,1], a[1,2], ... , a[1,y]\n\
a[2,1], a[2,2], ... , a[2,y]\n\
 ...     ...           ...\n\
a[x,1], a[x,2], ... , a[x,y];\n\
\n\
for a 3 by 2 set of tiles:\n\
3,2;\n\
15h,11v,20hv,\n\
23,22,-;\n\
\n\
Whitespace is ignored. There are 5 control characters comma(,), semicolon(;),\n\
dash(-), h, and v. The comma separate indexes from each other. The semicolon\n\
separates the \"size\" section from the \"indexes\" section. The dash indicates\n\
to ignore that location. Finally 'h' and 'v' indicate whether to flip the tiles\n\
horizontally or vertically... or both. The indexes are ordered to form the\n\
image from top-left to bottom-right.\n\
\n\
Locations with dashes will be filled with 0s. The locations will be ignored\n\
when converting back from a bmp to the tileset.\n\
\n\
switches\n\
-b : bitmap file\n\
-B : bytes, interpret gba data as 8bit indexes\n\
-h : help, displays this message\n\
-m : map file\n\
-r : reverse, convert from bmp to gba data\n\
-t : gba tileset data file\n\
\n\
example 1\n\
---------\n\
gba2bmp -t tileset.dat -m map.txt -b out.bmp\n\
\n\
This will construct a bitmap image using indexed colors. Using a \"map\" file,\n\
which are indexes to the tiles within the gba data file, will place those tiles\n\
in any arbitrary order. This is useful when the tiles are scattered throughout\n\
the tileset\n\
\n\
example 2\n\
---------\n\
gba2bmp -r -b in.bmp -m map.txt -t tileset.dat\n\
\n\
After editing the bmp file, we can update the tileset by running the conversion\n\
in reverse.\n\
";

void fput32(int n, FILE *f) {
	fputc(n    , f);
	fputc(n>> 8, f);
	fputc(n>>16, f);
	fputc(n>>24, f);	
}

void fput16(int n, FILE *f) {
	fputc(n    , f);
	fputc(n>> 8, f);
}

int fget16(FILE *f) {
	return fgetc(f) | (fgetc(f)<<8);
}

int fget32(FILE *f) {
	return fgetc(f) | (fgetc(f)<<8) | (fgetc(f)<<16) | (fgetc(f)<<24);
}

// writes BMP header, including the palette
// file must be openned in "wb" mode
void writeBMPHeader16(FILE *f, int tile_w, int tile_h, int *palette) {
	// Header (14 bytes)
	//   signature
	fputs("BM", f);
	//   filesize
	fput32(14 + 40 + 64 + tile_w*tile_h*32, f);
	//   reserved
	fput32(0, f);
	//   dataoffset
	fput32(14 + 40 + 64, f);

	// InfoHeader (40 bytes)
	//   size
	fput32(40, f);
	//   width
	fput32(tile_w*8, f);
	//   height
	fput32(tile_h*8, f);
	//   planes
	fput16(1, f);
	//   bits per pixel
	fput16(4, f);
	//   compression
	fput32(0, f);
	//   ImageSize (compressed)
	fput32(0, f);
	//   XpixelsPerM
	fput32(11811, f); // 300 dots per inch = 118.11 dots per cms
	//   YpixelsPerM
	fput32(11811, f);
	//   Colors Used
	fput32(16, f);
	//   Important Colors
	fput32(0, f);

	// ColorTable (4 * 16 = 64 bytes)
	for (int i=0; i<16; i++) {
		// byte order : bb gg rr 00 (little endian)
		// fputc(0, f); // b
		// fputc(i, f); // g
		// fputc(i, f); // r
		// fputc(0, f); // reserved
		fput32(palette[i], f);
	}
}

// writes BMP header, including the palette
// file must be openned in "wb" mode
void writeBMPHeader256(FILE *f, int tile_w, int tile_h, int *palette) {
	// Header (14 bytes)
	//   signature
	fputs("BM", f);
	//   filesize
	fput32(14 + 40 + 1024 + tile_w*tile_h*64, f);
	//   reserved
	fput32(0, f);
	//   dataoffset
	fput32(14 + 40 + 1024, f);

	// InfoHeader (40 bytes)
	//   header size
	fput32(40, f);
	//   width
	fput32(tile_w*8, f);
	//   height
	fput32(tile_h*8, f);
	//   planes
	fput16(1, f);
	//   bits per pixel
	fput16(8, f);
	//   compression
	fput32(0, f);
	//   ImageSize (compressed)
	fput32(0, f);
	//   XpixelsPerM
	fput32(0, f);
	//   YpixelsPerM
	fput32(0, f);
	//   Colors Used
	fput32(256, f);
	//   Important Colors
	fput32(0, f);

	// ColorTable (4 * 256 = 1024 bytes)
	for (int i=0; i<256; i++) {
		// byte order : bb gg rr 00 (little endian)
		// fputc(0, f); // b
		// fputc(i, f); // g
		// fputc(i, f); // r
		// fputc(0, f); // reserved
		fput32(palette[i], f);
	}
}

int * parseMap(char *map, int *width, int *height) {
	FILE *m;

	m = fopen(map, "r");
	if (!m) {
		perror("can't open map file\n");
		return 0;
	}
	
	// get dimensions
	int w, h, n, a, c;
	bool done;
	
	w = 0;
	h = 0;

	while((c = fgetc(m)) != ',') {
		if(c >= '0' && c <= '9') {
			w = w*10 + c - '0';
		}
	}
	while((c = fgetc(m)) != ';') {
		if(c >= '0' && c <= '9') {
			h = h*10 + c - '0';
		}
	}

	*width = w;
	*height = h;

	// allocate array of indeces
	int size;
	size = w*h;

	// printf("size: %d %d\n", w, h);
	
	int *data;
	data = malloc(sizeof(int) * size);

	n = 0;
	a = 0;
	done = false;

	// parse indeces
	while(((c = fgetc(m)) != EOF) && !done) {
		switch(c) {
			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
				n = n*10 + c - '0';
				break;
			case ';':
				done = true;
			case ',':
				// printf("%d ", n);
				data[a] = n;
				a++;
				n = 0;
				break;
			case '-':
				n = -1;
				break;
			case 'h':
				n |= 1024;
				break;
			case 'v':
				n |= 2048;
				break;
		}
	}

	fclose(m);

	return data;
}

void gba2bmp(char *tileset, char *map, char *bmp) {
	FILE *t, *b;
	t = fopen(tileset, "rb");
	if (!t) {
		perror("can't open tileset file\n");
		return;
	}
	b = fopen(bmp, "wb");
	if (!b) {
		perror("can't open bmp file\n");
		return;
	}
	
	int w, h, a;
	int *data;
	data = parseMap(map, &w, &h);

	// palette
	int pal[16] = {
		0xFF00FF, 0x0000, 0x4400, 0x6000,
		0x7600, 0x8800, 0x9800, 0xA600,
		0xB400, 0xC000, 0xCC00, 0xD700,
		0xE200, 0xEC00, 0xF500, 0xFF00
	};

	// create bmp file
	writeBMPHeader16(b, w, h, pal);

	// printf("test\n");

	// write pixels
	// bmp files write pixels from bottom-left to top-right
	for(int j = h-1; j>=0; j--) {
		int j_;
		j_ = j*w;
		for(int r = 7; r>=0; r--) {
			for(int i = 0; i<w; i++) {
				int k;
				a = data[i+j_];
				// printf("%3d ", a);
				if (a <= -1){
					if ((r & 1) == 0) {
						fputc(1, b);
						fputc(1, b);
						fputc(1, b);
						fputc(1, b);
					} else {
						fputc(16, b);
						fputc(16, b);
						fputc(16, b);
						fputc(16, b);
					}
					continue;
				}

				// a = a & 1023;
				// each tile are 32 bytes long
				// each row in the tileset are 4 bytes
				// printf("%d %d %d %d, ", i, j, k, a);

				// vflip
				if (a & 2048) {
					k = (a&1023)*32 + (7-r)*4;
				} else {
					k = (a&1023)*32 + r*4;
				}
				fseek(t, k, SEEK_SET);
				
				int d;
				if (a & 1024) { // hflip
					d = fget32(t);
					fputc(d>>24, b);
					fputc(d>>16, b);
					fputc(d>> 8, b);
					fputc(d    , b);
				} else {       // no flip
					d = fgetc(t);
					fputc((d<<4)|(d>>4), b);
					d = fgetc(t);
					fputc((d<<4)|(d>>4), b);
					d = fgetc(t);
					fputc((d<<4)|(d>>4), b);
					d = fgetc(t);
					fputc((d<<4)|(d>>4), b);
				}
				/*
				for(int l=0; l<4; l++) {
					int d;
					d = fgetc(t);
					fputc((d<<4)|(d>>4), b);
				}
				*/
			}
			// printf("\n");
		}
	}

	fclose(t);
	fclose(b);
	free(data);

	return;
}

void bmp2gba (char *bmp, char *map, char *tileset) {
	FILE *b, *t;
	
	b = fopen(bmp, "rb");
	if (!b) {
		perror("can't open bmp file\n");
		return;
	}
	t = fopen(tileset, "rb+");
	if (!t) {
		perror("can't open tileset file\n");
		return;
	}

	int w, h;
	int *data;
	data = parseMap(map, &w, &h);

	// get bmp meta
	int filesize, offset, width, height, bpp;

	if ((fgetc(b) != 'B') || (fgetc(b) != 'M'))
		puts("bmp file has bad header\n");

	filesize = fget32(b);
	fget32(b);
	offset = fget32(b);
	fget32(b);
	width = fget32(b);
	height = fget32(b);
	fget16(b);
	bpp = fget16(b);

	// check bmp dimensions
	if (width/8 != w) puts("bmp file has bad width\n");
	if (height/8 != h) puts("bmp file has bad height\n");

	// buffer a bmp line, each line has w*4 bytes
	// 8 lines consist a set of w tiles
	char *buf;
	int bufsize = 32 * w;
	buf = malloc(bufsize);
	/*
	0   1   2   3   ..  w-1
	0   4   8   12  ..
	aaaabbbbccccdddd    bot
	aaaabbbbccccdddd
	aaaabbbbccccdddd
	aaaabbbbccccdddd
	aaaabbbbccccdddd
	aaaabbbbccccdddd
	aaaabbbbccccdddd
	aaaabbbbccccdddd    top
	*/

	fseek(b, offset, SEEK_SET);

	for (int j = h-1; j >= 0; j--) {
		// buffer a row of tiles, starting from the bottom
		for (int r = 7; r >= 0; r--) {
			for (int i = 0; i < w; i++) {
				int k;
				k = i*32+r*4;
				buf[k] = fgetc(b);
				buf[k+1] = fgetc(b);
				buf[k+2] = fgetc(b);
				buf[k+3] = fgetc(b);
			}
		}

		// write buffer to tileset
		for (int i = 0; i < w; i++) {
			int a, k;
			k = j*w + i;
			a = data[k];

			if (a <= -1) continue;

			// printf("%d ", a);

			// TODO: implement h v flip here

			fseek(t, (a&1023)*32, SEEK_SET);
			/*
			for (int l=0; l<32; l++) {
				int n, m;
				m = i*32 + l;
				// switch nibbles
				n = ((buf[m]&15)<<4) | ((buf[m]>>4)&15);
				// n = buf[m];
				fputc(n, t);
			}
			*/
			int m, n, p;
			if (a & 2048) { // v flip
				m = i*32 + 28;
				p = -4;
			} else {        // no flip
				m = i*32;
				p = 4;
			}
			if (a & 1024) { // h flip
				for (int l=0; l<8; l++) {
					fputc(buf[m+3], t);
					fputc(buf[m+2], t);
					fputc(buf[m+1], t);
					fputc(buf[m]  , t);
					m+=p;
				}
			} else {        // no flip
				for (int l=0; l<8; l++) {
					n = ((buf[m]&15)<<4)   | ((buf[m]>>4)&15);
					fputc(n, t);
					n = ((buf[m+1]&15)<<4) | ((buf[m+1]>>4)&15);
					fputc(n, t);
					n = ((buf[m+2]&15)<<4) | ((buf[m+2]>>4)&15);
					fputc(n, t);
					n = ((buf[m+3]&15)<<4) | ((buf[m+3]>>4)&15);
					fputc(n, t);
					m+=p;
				}
			}
		}
	}
	
	free(data);
	free(buf);
	fclose(t);
	fclose(b);
}

void gba2bmp256 (char *tileset, char *map, char *bmp) {
	FILE *t, *b;
	t = fopen(tileset, "rb");
	if (!t) {
		perror("can't open tileset file\n");
		return;
	}
	b = fopen(bmp, "wb");
	if (!b) {
		perror("can't open bmp file\n");
		return;
	}
	
	int w, h, a;
	int *data;
	data = parseMap(map, &w, &h);

	// palette
	int pal[256] = {
		0x004000,0x005A00,0x006E00,0x008000,0x008F00,0x009C00,0x00A900,0x00B400,
		0x00BF00,0x00CA00,0x00D300,0x00DD00,0x00E600,0x00EF00,0x00F700,0x00FF00,
		0x004019,0x005A23,0x006E2B,0x008031,0x008F37,0x009C3C,0x00A941,0x00B446,
		0x00BF4A,0x00CA4E,0x00D352,0x00DD55,0x00E659,0x00EF5C,0x00F75F,0x00FF62,
		0x00402F,0x005A42,0x006E51,0x00805D,0x008F68,0x009C72,0x00A97B,0x00B484,
		0x00BF8C,0x00CA94,0x00D39B,0x00DDA2,0x00E6A8,0x00EFAF,0x00F7B5,0x00FFBB,
		0x003740,0x004D5A,0x005F6E,0x006D80,0x007A8F,0x00869C,0x0091A9,0x009BB4,
		0x00A4BF,0x00ADCA,0x00B6D3,0x00BEDD,0x00C5E6,0x00CDEF,0x00D4F7,0x00DBFF,
		0x002040,0x002D5A,0x00376E,0x004080,0x00478F,0x004E9C,0x0054A9,0x005AB4,
		0x0060BF,0x0065CA,0x006AD3,0x006EDD,0x0073E6,0x0077EF,0x007BF7,0x0080FF,
		0x000940,0x000D5A,0x00106E,0x001280,0x00148F,0x00169C,0x0018A9,0x0019B4,
		0x001BBF,0x001CCA,0x001ED3,0x001FDD,0x0020E6,0x0022EF,0x0023F7,0x0024FF,
		0x110040,0x18005A,0x1E006E,0x220080,0x26008F,0x2A009C,0x2D00A9,0x3000B4,
		0x3300BF,0x3600CA,0x3900D3,0x3B00DD,0x3E00E6,0x4000EF,0x4200F7,0x4400FF,
		0x270040,0x37005A,0x44006E,0x4E0080,0x58008F,0x60009C,0x6800A9,0x6F00B4,
		0x7500BF,0x7C00CA,0x8200D3,0x8800DD,0x8D00E6,0x9200EF,0x9800F7,0x9D00FF,
		0x400040,0x5A005A,0x6E006E,0x7F0080,0x8F008F,0x9C009C,0xA900A9,0xB400B4,
		0xBF00BF,0xCA00CA,0xD300D3,0xDD00DD,0xE600E6,0xEF00EF,0xF700F7,0xFF00FF,
		0x400027,0x5A0037,0x6E0044,0x80004E,0x8F0058,0x9C0060,0xA90068,0xB4006F,
		0xBF0075,0xCA007C,0xD30082,0xDD0088,0xE6008D,0xEF0092,0xF70098,0xFF009D,
		0x400011,0x5A0018,0x6E001E,0x800022,0x8F0026,0x9C002A,0xA9002D,0xB40030,
		0xBF0033,0xCA0036,0xD30039,0xDD003B,0xE6003E,0xEF0040,0xF70042,0xFF0044,
		0x400900,0x5A0D00,0x6E1000,0x801200,0x8F1400,0x9C1600,0xA91800,0xB41900,
		0xBF1B00,0xCA1C00,0xD31E00,0xDD1F00,0xE62000,0xEF2200,0xF72300,0xFF2400,
		0x402000,0x5A2D00,0x6E3700,0x804000,0x8F4700,0x9C4E00,0xA95400,0xB45A00,
		0xBF6000,0xCA6500,0xD36A00,0xDD6E00,0xE67300,0xEF7700,0xF77B00,0xFF7F00,
		0x403700,0x5A4D00,0x6E5F00,0x806D00,0x8F7A00,0x9C8600,0xA99100,0xB49B00,
		0xBFA400,0xCAAD00,0xD3B600,0xDDBE00,0xE6C500,0xEFCD00,0xF7D400,0xFFDB00,
		0x2F4000,0x425A00,0x516E00,0x5D8000,0x688F00,0x729C00,0x7BA900,0x84B400,
		0x8CBF00,0x94CA00,0x9BD300,0xA2DD00,0xA8E600,0xAFEF00,0xB5F700,0xBBFF00,
		0x194000,0x235A00,0x2B6E00,0x318000,0x378F00,0x3C9C00,0x41A900,0x46B400,
		0x4ABF00,0x4ECA00,0x52D300,0x55DD00,0x59E600,0x5CEF00,0x5FF700,0x62FF00
	};
	/*
	int pal[256] = {
		0xFF0000,0x000000,0x000010,0x000017,0x00011B,0x00011F,0x000123,0x000225,
		0x000228,0x00032A,0x00032D,0x00042F,0x000530,0x000532,0x000634,0x000735,
		0x000737,0x000838,0x000939,0x000A3A,0x000A3C,0x000B3D,0x000C3D,0x000D3E,
		0x000E3F,0x000F3F,0x001040,0x001141,0x001241,0x001342,0x001442,0x001543,
		0x001643,0x001744,0x001844,0x001944,0x001A45,0x001B45,0x001C45,0x001E45,
		0x001F45,0x002045,0x002145,0x002246,0x002445,0x002545,0x002645,0x002746,
		0x002945,0x002A45,0x002B45,0x002D44,0x002E44,0x002F44,0x003143,0x003244,
		0x003344,0x003543,0x003643,0x003842,0x003942,0x003B41,0x003C41,0x003E40,
		0x003F40,0x00413F,0x00423F,0x00443E,0x00453E,0x00473D,0x00483D,0x004A3C,
		0x004B3C,0x004D3B,0x004F3A,0x00503A,0x005239,0x005338,0x005537,0x005736,
		0x005836,0x005A35,0x005C34,0x005E33,0x005F33,0x006132,0x006331,0x006430,
		0x00662F,0x00682E,0x006A2D,0x006C2C,0x006D2C,0x006F2A,0x007129,0x007328,
		0x007527,0x007726,0x007826,0x007A24,0x007C23,0x007E22,0x008021,0x008220,
		0x00841E,0x00861D,0x00881C,0x00891C,0x008B1B,0x008D19,0x008F18,0x009117,
		0x009316,0x009514,0x009713,0x009912,0x009B11,0x009D0F,0x009F0E,0x00A10D,
		0x00A40B,0x00A609,0x00A808,0x00AA07,0x00AC05,0x00AE04,0x00B003,0x00B202,
		0x00B400,0x00B4FF,0x00B5FD,0x00B5FC,0x00B6FA,0x00B7F9,0x00B8F7,0x00B8F6,
		0x00B9F4,0x00BAF2,0x00BAF2,0x00BBF0,0x00BCEE,0x00BCED,0x00BDEB,0x00BEE9,
		0x00BEE8,0x00BFE6,0x00C0E4,0x00C0E3,0x00C1E1,0x00C2DF,0x00C2DE,0x00C3DC,
		0x00C4DA,0x00C4D9,0x00C5D7,0x00C6D5,0x00C6D4,0x00C7D2,0x00C7D1,0x00C8CF,
		0x00C9CD,0x00C9CC,0x00CACA,0x00CBC8,0x00CBC6,0x00CCC4,0x00CDC2,0x00CDC1,
		0x00CEBF,0x00CEBD,0x00CFBB,0x00D0B9,0x00D0B8,0x00D1B5,0x00D1B4,0x00D2B2,
		0x00D3B0,0x00D3AE,0x00D4AC,0x00D4AB,0x00D5A8,0x00D6A6,0x00D6A5,0x00D7A2,
		0x00D7A1,0x00D89F,0x00D99C,0x00D99B,0x00DA98,0x00DA97,0x00DB95,0x00DC92,
		0x00DC91,0x00DD8E,0x00DD8D,0x00DE8A,0x00DE89,0x00DF86,0x00E084,0x00E082,
		0x00E180,0x00E17E,0x00E27C,0x00E27A,0x00E378,0x00E475,0x00E473,0x00E571,
		0x00E56F,0x00E66D,0x00E66B,0x00E768,0x00E767,0x00E864,0x00E961,0x00E960,
		0x00EA5D,0x00EA5B,0x00EB59,0x00EB57,0x00EC54,0x00EC53,0x00ED50,0x00ED4E,
		0x00EE4B,0x00EE4A,0x00EF47,0x00F044,0x00F042,0x00F13F,0x00F13E,0x00F23B,
		0x00F239,0x00F336,0x00F334,0x00F432,0x00F430,0x00F52D,0x00F52B,0x00F628,
		0x00F626,0x00F723,0x00F721,0x00F81F,0x00F81D,0x00F91A,0x00F918,0x00FA15,
		0x00FA13,0x00FB10,0x00FB0E,0x00FC0B,0x00FC09,0x00FD06,0x00FD04,0x00FE01
	};
	*/

	// create bmp file
	writeBMPHeader256(b, w, h, pal);

	// write pixels
	// bmp files write pixels from bottom-left to top-right
	// rows of tiles
	for(int j = h-1; j>=0; j--) {
		int j_;
		j_ = j*w;
		// row of each pixels
		for(int r = 7; r>=0; r--) {
			// columns of pixels
			for(int i=0; i<w; i++) {
				int k;
				a = data[i+j_];

				// this creates a grid pattern
				// to indicate tile in bmp is empty
				if (a <= -1){
					if ((r & 1) == 0) {
						fputc(0, b); fputc(1, b);
						fputc(0, b); fputc(1, b);
						fputc(0, b); fputc(1, b);
						fputc(0, b); fputc(1, b);
					} else {
						fputc(1, b); fputc(0, b);
						fputc(1, b); fputc(0, b);
						fputc(1, b); fputc(0, b);
						fputc(1, b); fputc(0, b);
					}
					continue;
				}

				// a = a & 1023;
				// each tile are 64 bytes long
				// each row in the tileset are 8 bytes

				// vflip
				if (a & 2048) {
					k = (a&1023)*64 + (7-r)*8;
				} else {
					k = (a&1023)*64 + r*8;
				}
				fseek(t, k, SEEK_SET);
				
				if (a & 1024) { // hflip
					int d, e;
					d = fget32(t);
					e = fget32(t);
					fputc(e>>24, b); fputc(e>>16, b);
					fputc(e>> 8, b); fputc(e    , b);
					fputc(d>>24, b); fputc(d>>16, b);
					fputc(d>> 8, b); fputc(d    , b);
				} else {       // no flip
					fputc(fgetc(t), b);	fputc(fgetc(t), b);
					fputc(fgetc(t), b);	fputc(fgetc(t), b);
					fputc(fgetc(t), b);	fputc(fgetc(t), b);
					fputc(fgetc(t), b);	fputc(fgetc(t), b);
				}
			}
		}
	}

	fclose(t);
	fclose(b);
	free(data);

	return;
}

int main (int argc, char **argv) {
	if (argc < 2) {
		puts("usage: gba2bmp -t tileset.dat -m map.txt -b out.bmp\n"
			 "for help: gba2bmp -h\n");
		return 0;
	}

	char *tileset, *map, *bmp;
	tileset = 0;
	map = 0;
	bmp = 0;

	bool reverse, bytemode, help;
	reverse = false;
	bytemode = false;
	help = false;

	for (int i=1; i<argc; i++) {
		if (argv[i][0] == '-') {
			switch(argv[i][1]) {
				case 'b': bmp = argv[i+1]; i++; break;
				case 'B': bytemode = true; break;
				case 'h': help = true; break;
				case 'm': map = argv[i+1]; i++; break;
				case 'r': reverse = true; break;
				case 't': tileset = argv[i+1]; i++; break;
				default : printf("bad option: %s\n", argv[i]);
			}
		} else {
			printf("bad argument: %d, %s\n", i, argv[i]);
		}
	}

	if (help) {
		puts(help_string);
		return 0;
	}

	if (!tileset) {
		puts("missing tileset argument\n");
		return 0;
	}
	if(!map) {
		puts("missing map argument\n");
		return 0;
	}
	if(!bmp) {
		puts("missing bmp argument\n");
		return 0;
	}

	// TODO: implement 256 color mode
	// either implement it using map file or
	// through the CLI...

	if (bytemode) {
		gba2bmp256(tileset, map, bmp);
		return 0;
	}

	if (!reverse) {
		gba2bmp(tileset, map, bmp);
	} else {
		bmp2gba(bmp, map, tileset);
	}
}
