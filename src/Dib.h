#ifndef __INCLUDE_DIB_H
#define __INCLUDE_DIB_H

#include <stdio.h>

#ifdef _VISUALC_
#include <windows.h>
#endif

/* File information header
 * provides general information about the file
 */
struct BitmapFileHeader {
	unsigned short type;
	unsigned       size;
	unsigned short reserved1;
	unsigned short reserved2;
	unsigned       offset;
};

/* Bitmap information header
 * provides information specific to the image data
 */
struct BitmapInfoHeader{
	unsigned       size;
	unsigned       width;
	unsigned       height;
	unsigned short planes;
	unsigned short bitCount;
	unsigned       compression;
	unsigned       imageSize;
	unsigned       hPixelsPerMeter;
	unsigned       vPixelsPerMeter;
	unsigned       numColors;
	unsigned       numColorsImp;
};

/* Colour palette
 */
struct RgbQuad {
	unsigned char rgbBlue;
	unsigned char rgbGreen;
	unsigned char rgbRed;
	unsigned char rgbReserved;
};

class Dib {
public:
	Dib();
	Dib(char * filename);
#ifdef _VISUAL_C
	Dib(HANDLE handle);
#endif
	~Dib();
	void readFromFile(char * filename) ;
	void writeToFile(char * filename);
	void readFromHandle(HANDLE handle);
	unsigned getHeight();
	unsigned getWidth();
	unsigned getRowPadBytes();
	unsigned getBitsPerPixel();
	unsigned char * getPixelBuffer();
	void getPixel(unsigned row, unsigned col, RgbQuad * quad);
	unsigned char getPixelGrayscale(unsigned row, unsigned col);
	void setPixel(unsigned row, unsigned col, RgbQuad * quad);
	void setPixelGrayscale(unsigned row, unsigned col, unsigned char value);
	unsigned char * getPixelsNoPadding();
	void setPixelsNoPadding(unsigned char * pixels);
	void convertGrayscale(Dib & src);
	void sobelEdgeDetectionWithMask(Dib & src, int mask1[3][3],
			int mask2[3][3]);
	void sobelEdgeDetection(Dib & src);
	void laplaceEdgeDetection(Dib & src);
	void histEqualization(Dib & src);
	void rotateImage(Dib & src);

private:
	BitmapFileHeader * fileHeader;
	BitmapInfoHeader * infoHeader;
	unsigned bytesPerPixel;
	unsigned rowPaddingBytes;
	unsigned char * pixels;

	void copyInternals(Dib & src);

};

#endif /* __INCLUDE_DIB_H */
