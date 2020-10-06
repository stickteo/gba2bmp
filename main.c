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

	if (!reverse) {
		gba2bmp(tileset, map, bmp);
	} else {
		bmp2gba(bmp, map, tileset);
	}
}
