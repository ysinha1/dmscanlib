/*
 * Decoder.cpp
 *
 *  Created on: 22-May-2009
 *      Author: loyola
 */

#include "Decoder.h"
#include "UaLogger.h"
#include "UaAssert.h"
#include "Dib.h"
#include "Util.h"
#include "BarcodeInfo.h"
#include "BinRegion.h"

#include <iostream>
#include <math.h>
#include <string>
#include <limits>
#include <vector>
#include <cmath>






#if defined(USE_NVWA)
#   include "debug_new.h"
#endif

#ifdef _VISUALC_
// disable fopen warnings
#pragma warning(disable : 4996)
#endif

using namespace std;

Decoder::Decoder(double g, unsigned s, unsigned t, unsigned c, double dist) {
	ua::Logger::Instance().subSysHeaderSet(3, "Decoder");
	scanGap = g;
	squareDev = s;
	edgeThresh = t;
	corrections = c;
	imageBuf = NULL;
	cellDistance = dist;
}

Decoder::~Decoder() {
	BarcodeInfo * b;
	BinRegion * c;

	while (barcodeInfos.size() > 0) {
		b = barcodeInfos.back();
		barcodeInfos.pop_back();
		UA_ASSERT_NOT_NULL(b);
		delete b;
	}

	while (rowBinRegions.size() > 0) {
		c = rowBinRegions.back();
		rowBinRegions.pop_back();
		UA_ASSERT_NOT_NULL(c);
		delete c;
	}
	while (colBinRegions.size() > 0) {
		c = colBinRegions.back();
		colBinRegions.pop_back();
		UA_ASSERT_NOT_NULL(c);
		delete c;
	}

	if (imageBuf != NULL) {
		free(imageBuf);
	}
}

void Decoder::initCells(unsigned maxRows, unsigned maxCols) {
	cells.resize(maxRows);
	for (unsigned row = 0; row < maxRows; ++row) {
		cells[row].resize(maxCols);
	}
}



Decoder::ProcessResult Decoder::processImageRegions(unsigned plateNum,
		Dib & dib, vector<vector<string> > & cellsRef) {

	CvRect noRect;
	noRect.height=0;
	noRect.width =0;

	if (!processImage(dib,noRect))
		return IMG_INVALID;

	calcRowsAndColumns();
	Decoder::ProcessResult calcSlotResult = calculateSlots(
			static_cast<double> (dib.getDpi()));
	if (calcSlotResult != OK) {
		return calcSlotResult;
	}
	cellsRef = cells;
	return OK;
}

void applyPlateFilters(IplImage * img, int rounds){
	for(int i=0; i < rounds; i++){
		cvSmooth(img,img,CV_GAUSSIAN,11,11);
	}
}

vector<CvRect> getTubeBlobs(IplImage *original,int threshold, int blobsize, int rounds, int border)
{
	IplImage* originalThr;
	IplImage* filtered;

	vector<CvRect> blobVector;
	CBlobResult blobs;

	filtered = cvCreateImage(cvGetSize(original), original->depth,original->nChannels);
	cvCopy(original, filtered, NULL);
	applyPlateFilters(filtered,rounds);

	originalThr = cvCreateImage(cvGetSize(filtered), IPL_DEPTH_8U,1);
	cvThreshold( filtered, originalThr, threshold, 255, CV_THRESH_BINARY );

	blobs = CBlobResult( originalThr, NULL, 0 );
	blobs.Filter( blobs, B_EXCLUDE, CBlobGetArea(), B_LESS, blobsize );

	for (int i = 0; i < blobs.GetNumBlobs(); i++ )
	{
		CvRect box = blobs.GetBlob(i)->GetBoundingBox();
		
		UA_ASSERTS(box.x < filtered->width && box.y < filtered->height,"blob is out of bounds");

		box.x -= border;
		box.y -= border;
		box.width += border*2;
		box.height += border*2;
		
		box.x = box.x < 0 ? 0 : box.x;
		box.y = box.y < 0 ? 0 : box.y;


		if(box.x + box.width >= filtered->width)
			box.width = (filtered->width-1)-box.x;

		if(box.y + box.height >= filtered->height)
			box.height = (filtered->height-1)-box.y;


		blobVector.push_back(box);
	}
	
	cvReleaseImage( &originalThr );
	cvReleaseImage( &filtered );

	return blobVector;
}



Decoder::ProcessResult Decoder::superProcessImageRegions(Dib & dib,IplImage *opencvImg, vector<vector<string> > & cellsRef, bool nuk) {
	Dib tmp;

	vector<CvRect> blobVector;

	if(!nuk){
		switch(dib.getDpi()){
			case 600:
				blobVector = getTubeBlobs(opencvImg,54,2400,4,14);
				break;

			case 400:
				blobVector = getTubeBlobs(opencvImg,50,1900,3,4);
				break;

			case 300:
			default:
				blobVector = getTubeBlobs(opencvImg,55,840,2,3);
				break;
		}
	}
	// nuk
	else{
		switch(dib.getDpi()){
			case 600:
				blobVector = getTubeBlobs(opencvImg,120,5000,10,0);
				break;

			case 400:
				blobVector = getTubeBlobs(opencvImg,120,3000,10,0);
				break;

			case 300:
			default:
				blobVector = getTubeBlobs(opencvImg,110,2000,8,0);
				break;
		}
		
	}

	#ifdef _DEBUG
	Dib blobRegions(dib);
	RgbQuad white(255,255,255);
	#endif

	for (int i =0 ;i<(int)blobVector.size();i++){
		tmp.crop(dib,blobVector[i].x,blobVector[i].y,blobVector[i].x+blobVector[i].width,blobVector[i].y+blobVector[i].height);
		processImage(tmp,blobVector[i]);

		#ifdef _DEBUG
		blobRegions.rectangle(blobVector[i].x,blobVector[i].y,blobVector[i].width,blobVector[i].height,white);
		#endif
	}
	

	#ifdef _DEBUG
	blobRegions.writeToFile("blobRegions.bmp");
	#endif
	
	this->width = dib.getWidth();
	this->height = dib.getHeight();
	
	calcRowsAndColumns();

	Decoder::ProcessResult calcSlotResult = calculateSlots(static_cast<double> (dib.getDpi()));

	if (calcSlotResult != OK) 
		return calcSlotResult;
	
	cellsRef = this->cells;

	return OK;
}

bool Decoder::decode(DmtxDecode *& dec, unsigned attempts,
		vector<BarcodeInfo *> & barcodeInfos, CvRect croppedOffset) {
	DmtxRegion * reg = NULL;
	BarcodeInfo * info = NULL;

	reg = dmtxRegionFindNext(dec, NULL);
	if (reg == NULL)
		return false;

	DmtxMessage * msg = dmtxDecodeMatrixRegion(dec, reg, corrections);
	if (msg != NULL) {
		info = new BarcodeInfo(dec, reg, msg);
		UA_ASSERT_NOT_NULL(info);

		//image is not aligned when croppedOffset width or height equal 0
		if(croppedOffset.width !=0 && croppedOffset.height !=0)
			info->alignCoordinates(croppedOffset.x,croppedOffset.y);

		barcodeInfos.push_back(info);

		DmtxPixelLoc & tlCorner = info->getTopLeftCorner();
		DmtxPixelLoc & brCorner = info->getBotRightCorner();

		UA_DOUT(3, 5, "message " << barcodeInfos.size() - 1
				<< ": " << info->getMsg()
				<< " : tlCorner/" << tlCorner.X << "," << tlCorner.Y
				<< "  brCorner/" << brCorner.X << "," << brCorner.Y);
		//showStats(dec, reg, msg);
		dmtxMessageDestroy(&msg);
	}
	dmtxRegionDestroy(&reg);
	return true;
}

bool Decoder::processImage(Dib & dib, CvRect croppedOffset) {
	height = dib.getHeight();
	width = dib.getWidth();
	dpi = dib.getDpi();

	DmtxImage * image = createDmtxImageFromDib(dib);
	DmtxDecode * dec = NULL;

	UA_DOUT(3, 5, "processImage: image width/" << width
			<< " image height/" << height
			<< " row padding/" << dmtxImageGetProp(image, DmtxPropRowPadBytes)
			<< " image bits per pixel/"
			<< dmtxImageGetProp(image, DmtxPropBitsPerPixel)
			<< " image row size bytes/"
			<< dmtxImageGetProp(image, DmtxPropRowSizeBytes));

	dec = dmtxDecodeCreate(image, 1);
	UA_ASSERT_NOT_NULL(dec);

	// slightly smaller than the new tube edge
	int minEdgeSize = static_cast<unsigned> (0.08 * dpi);

	// slightly bigger than the Nunc edge
	int maxEdgeSize = static_cast<unsigned> (0.18 * dpi);

	dmtxDecodeSetProp(dec, DmtxPropEdgeMin, minEdgeSize);
	dmtxDecodeSetProp(dec, DmtxPropEdgeMax, maxEdgeSize);
	dmtxDecodeSetProp(dec, DmtxPropSymbolSize, DmtxSymbolSquareAuto);
	dmtxDecodeSetProp(dec, DmtxPropScanGap, static_cast<unsigned> (scanGap
			* dpi));
	dmtxDecodeSetProp(dec, DmtxPropSquareDevn, squareDev);
	dmtxDecodeSetProp(dec, DmtxPropEdgeThresh, edgeThresh);

	unsigned regionCount = 0;
	while (1) {
		if (!decode(dec, 1, barcodeInfos,croppedOffset)) {
			break;
		}
		UA_DOUT(3, 5, "retrieved message from region " << regionCount++);
	}

	
	if(croppedOffset.width !=0 && croppedOffset.height !=0){
		// save image to a PNM file
		UA_DEBUG(
				FILE * fh;
				unsigned char *pnm;
				int totalBytes;
				int headerBytes;

				pnm = dmtxDecodeCreateDiagnostic(dec, &totalBytes, &headerBytes, 0);
				if (pnm != NULL) {
					fh = fopen("out.pnm", "wb");
					fwrite(pnm, sizeof(unsigned char), totalBytes, fh);
					fclose(fh);
					free(pnm);
				}
		);
	}
	

	dmtxDecodeDestroy(&dec);
	dmtxImageDestroy(&image);

	if (barcodeInfos.size() == 0) {
		UA_DOUT(3, 1, "processImage: no barcodes found");
		return false;
	}

	return true;
}

void Decoder::calcRowsAndColumns() {
	UA_ASSERTS(barcodeInfos.size() != 0, "no barcodes in barcodeInfos vector");

	bool insideRowBin;
	bool insideColBin;

	for (unsigned i = 0, n = barcodeInfos.size(); i < n; ++i) {
		insideRowBin = false;
		insideColBin = false;

		DmtxPixelLoc & tlCorner = barcodeInfos[i]->getTopLeftCorner();
		DmtxPixelLoc & brCorner = barcodeInfos[i]->getBotRightCorner();

		UA_DOUT(3, 9, "tag " << i << " : tlCorner/" << tlCorner.X << "," << tlCorner.Y
				<< "  brCorner/" << brCorner.X << "," << brCorner.Y);

		if (tlCorner.X == 669) {
			UA_DOUT(3, 1, "here");
		}

		for (unsigned c = 0, cn = colBinRegions.size(); c < cn; ++c) {
			BinRegion & bin = *colBinRegions[c];

			unsigned min = bin.getMin();
			unsigned max = bin.getMax();
			unsigned left = static_cast<unsigned> (tlCorner.X);
			unsigned right = static_cast<unsigned> (brCorner.X);

			if (((min - BIN_THRESH <= left) && (left <= max + BIN_THRESH))
					|| ((min - BIN_THRESH <= right) && (right <= max
							+ BIN_THRESH))) {
				insideColBin = true;
				barcodeInfos[i]->setColBinRegion(&bin);
				UA_DOUT(3, 9, "overlaps col " << c);
				if (left < min) {
					bin.setMin(left);
					UA_DOUT(3, 9, "col " << c << " update min " << bin.getMin());
				}
				if (left > max) {
					bin.setMax(left);
					UA_DOUT(3, 9, "col " << c << " update max " << bin.getMax());
				}
				if (right < min) {
					bin.setMin(right);
					UA_DOUT(3, 9, "col " << c << " update min " << bin.getMin());
				}
				if (right > max) {
					bin.setMax(right);
					UA_DOUT(3, 9, "col " << c << " update max " << bin.getMax());
				}
				break;
			}
		}

		for (unsigned r = 0, rn = rowBinRegions.size(); r < rn; ++r) {
			BinRegion & bin = *rowBinRegions[r];

			unsigned min = bin.getMin();
			unsigned max = bin.getMax();
			unsigned top = static_cast<unsigned> (tlCorner.Y);
			unsigned bottom = static_cast<unsigned> (brCorner.Y);

			if (((min - BIN_THRESH <= top) && (top <= max + BIN_THRESH))
					|| ((min - BIN_THRESH <= bottom) && (bottom <= max
							+ BIN_THRESH))) {
				insideRowBin = true;
				barcodeInfos[i]->setRowBinRegion(&bin);
				UA_DOUT(3, 9, "overlaps row " << r);
				if (top < min) {
					bin.setMin(top);
					UA_DOUT(3, 9, "row " << r << " update min " << bin.getMin());
				}
				if (top > max) {
					bin.setMax(top);
					UA_DOUT(3, 9, "row " << r << " update max " << bin.getMax());
				}
				if (bottom < min) {
					bin.setMin(bottom);
					UA_DOUT(3, 9, "row " << r << " update min " << bin.getMin());
				}
				if (bottom > max) {
					bin.setMax(bottom);
					UA_DOUT(3, 9, "row " << r << " update max " << bin.getMax());
				}
				break;
			}
		}

		if (!insideColBin) {
			BinRegion * newBinRegion = new BinRegion(
					BinRegion::ORIENTATION_VER,
					static_cast<unsigned> (tlCorner.X),
					static_cast<unsigned> (brCorner.X));
			UA_ASSERT_NOT_NULL(newBinRegion);
			UA_DOUT(3, 9, "new col " << colBinRegions.size() << ": " << *newBinRegion);
			colBinRegions.push_back(newBinRegion);
			barcodeInfos[i]->setColBinRegion(newBinRegion);
		}

		if (!insideRowBin) {
			BinRegion * newBinRegion = new BinRegion(
					BinRegion::ORIENTATION_HOR,
					static_cast<unsigned> (tlCorner.Y),
					static_cast<unsigned> (brCorner.Y));
			UA_ASSERT_NOT_NULL(newBinRegion);
			UA_DOUT(3, 9, "new row " << rowBinRegions.size() << ": " << *newBinRegion);
			rowBinRegions.push_back(newBinRegion);
			barcodeInfos[i]->setRowBinRegion(newBinRegion);
		}

		ostringstream msg;
		for (unsigned c = 0, n = colBinRegions.size(); c < n; ++c) {
			BinRegion & region = *colBinRegions[c];
			msg << c << " (" << region.getMin() << ", " << region.getMax()
					<< "), ";
		}
		UA_DOUT(3, 9, "columns " << msg.str());

		msg.str("");
		for (unsigned r = 0, rn = rowBinRegions.size(); r < rn; ++r) {
			BinRegion & region = *rowBinRegions[r];
			msg << r << " (" << region.getMin() << ", " << region.getMax()
					<< "), ";
		}
		UA_DOUT(3, 9, "rows " << msg.str());
	}

	sort(rowBinRegions.begin(), rowBinRegions.end(), BinRegionSort());
	sort(colBinRegions.begin(), colBinRegions.end(), BinRegionSort());

	// assign ranks now and add threshold
	for (unsigned i = 0, n = colBinRegions.size(); i < n; ++i) {
		BinRegion & c = *colBinRegions[i];

		unsigned min = c.getMin();
		c.setMin(min > BIN_MARGIN ? min - BIN_MARGIN : 0);

		unsigned max = c.getMax();
		c.setMax(max < width - BIN_MARGIN - 1 ? max + BIN_MARGIN : width - 1);

		c.setRank(i);
		UA_DOUT(3, 5, "col BinRegion " << i << ": " << c);
	}
	for (unsigned i = 0, n = rowBinRegions.size(); i < n; ++i) {
		BinRegion & c = *rowBinRegions[i];

		unsigned min = c.getMin();
		c.setMin(min > BIN_MARGIN ? min - BIN_MARGIN : 0);

		unsigned max = c.getMax();
		c.setMax(max < height - BIN_MARGIN - 1 ? max + BIN_MARGIN : height - 1);

		c.setRank(i);
		UA_DOUT(3, 5, "row BinRegion " << i << ": " << c);
	}

	UA_DOUT(3, 3, "number of columns: " << colBinRegions.size());
	UA_DOUT(3, 3, "number of rows: " << rowBinRegions.size());

	sort(barcodeInfos.begin(), barcodeInfos.end(), BarcodeInfoSort());
}

Decoder::ProcessResult Decoder::calculateSlots(double dpi) {
	// for columns the one with largest rank is column 1
	unsigned numCols = colBinRegions.size();
	unsigned numRows = rowBinRegions.size();

	if (numCols > 0) {
		BinRegion & region = *colBinRegions[numCols - 1];

		// Calculate the distance of the 12'th column to check that it is within
		// the bounds of the image. If not then the column is not really the
		// first one.
		double edgeDist = 11.0 * cellDistance * dpi;
		UA_DOUT(3, 5, "first_col_center/" << region.getCenter()
				<< " edge_distance/" << edgeDist);
		if (region.getCenter() <= static_cast<unsigned> (edgeDist)) {
			UA_DOUT(3, 5, "out of bounds");
			return POS_CALC_ERROR;
		}

		region.setId(0);
	}

	if (numRows > 0) {
		BinRegion & region = *rowBinRegions[0];

		// Calculate the distance of the 8'th row to check that it is within
		// the bounds of the image. If not then the column is not really the
		// first one.
		double edgeDist = 7.0 * cellDistance * dpi;
		UA_DOUT(3, 5, "first_row_center/" << region.getCenter()
				<< " edge_distance/" << edgeDist << " height/" << height);
		if (region.getCenter() + static_cast<unsigned> (edgeDist) >= height) {
			UA_DOUT(3, 5, "out of bounds");
			return POS_CALC_ERROR;
		}
		rowBinRegions[0]->setId(0);
	}

	unsigned interval;
	double cellDistError = cellDistance * 0.4;

	UA_DOUT(3, 5, "cellDistError/" << cellDistError);

	if (numCols > 1) {
		for (int c = numCols - 1; c > 0; --c) {
			BinRegion & region1 = *colBinRegions[c - 1];
			BinRegion & region2 = *colBinRegions[c];

			double dist = static_cast<double> (region2.getCenter()
					- region1.getCenter()) / static_cast<double> (dpi);

			interval = 0;
			for (unsigned i = 1; i < 12; ++i) {
				double diff = abs(dist - i * cellDistance);
				UA_DOUT(3, 5, "col region " << c << "-" << c - 1 << " distance/"
						<< dist << " diff/" << diff << " inteval/" << i);
				if (diff < cellDistError) {
					interval = i;
					break;
				}
			}
			if (interval == 0) {
				UA_DOUT(3, 1, "could not determine column intervals");
				return POS_CALC_ERROR;
			}

			UA_DOUT(3, 5, "col region " << c << "-" << c - 1 << " distance/"
					<< dist << " inteval/" << interval);

			region1.setId(region2.getId() + interval);
		}
	}

	if (numRows > 1) {
		for (unsigned r = 1; r < numRows; ++r) {
			BinRegion & region1 = *rowBinRegions[r - 1];
			BinRegion & region2 = *rowBinRegions[r];

			double dist = static_cast<double> (region2.getCenter()
					- region1.getCenter()) / static_cast<double> (dpi);

			interval = 0;
			for (unsigned i = 1; i < 8; ++i) {
				double diff = abs(dist - i * cellDistance);
				UA_DOUT(3, 5, "row region " << r << "-" << r - 1 << " distance/"
						<< dist << " diff/" << diff << " inteval/" << i);
				if (abs(dist - i * cellDistance) < cellDistError) {
					interval = i;
					break;
				}
			}
			if (interval == 0) {
				UA_DOUT(3, 1, "could not determine row intervals");
				return POS_CALC_ERROR;
			}

			UA_DOUT(3, 5, "row region " << r << "-" << r - 1 << " distance/"
					<< dist << " inteval/" << interval);

			region2.setId(region1.getId() + interval);
		}
	}

	// get max rows and max cols
	unsigned maxRow = 0;
	unsigned maxCol = 0;
	for (unsigned i = 0, n = barcodeInfos.size(); i < n; ++i) {
		BarcodeInfo & info = *barcodeInfos[i];
		unsigned row = info.getRowBinRegion().getId();
		unsigned col = info.getColBinRegion().getId();

		if (row > maxRow) {
			maxRow = row;
		}

		if (col > maxCol) {
			maxCol = col;
		}
	}

	if ((maxRow >= 8) || (maxCol >= 12)) {
		return POS_CALC_ERROR;
	}

	// make sure no barcodes are in the same cells
	initCells(maxRow + 1, maxCol + 1);
	for (unsigned i = 0, n = barcodeInfos.size(); i < n; ++i) {
		BarcodeInfo & info = *barcodeInfos[i];
		unsigned row = info.getRowBinRegion().getId();
		unsigned col = info.getColBinRegion().getId();

		UA_DOUT(3, 5, "barcode " << i << " (" << (char)('A' + row) << ", "
				<< col + 1 << "): " << info.getMsg());

		if (cells[row][col].length() > 0) {
			UA_DOUT(3, 5, "position (" << (char)('A' + row) << ", "
					<< col + 1 << ") already occupied");
			return POS_CALC_ERROR;
		}

		cells[row][col] = info.getMsg();
	}
	return OK;
}

void Decoder::showStats(DmtxDecode *dec, DmtxRegion *reg, DmtxMessage *msg) {
	int height;
	int dataWordLength;
	int rotateInt;
	double rotate;
	DmtxVector2 p00, p10, p11, p01;

	height = dmtxDecodeGetProp(dec, DmtxPropHeight);

	p00.X = p00.Y = p10.Y = p01.X = 0.0;
	p10.X = p01.Y = p11.X = p11.Y = 1.0;
	dmtxMatrix3VMultiplyBy(&p00, reg->fit2raw);
	dmtxMatrix3VMultiplyBy(&p10, reg->fit2raw);
	dmtxMatrix3VMultiplyBy(&p11, reg->fit2raw);
	dmtxMatrix3VMultiplyBy(&p01, reg->fit2raw);

	dataWordLength = dmtxGetSymbolAttribute(DmtxSymAttribSymbolDataWords,
			reg->sizeIdx);

	rotate = (2 * M_PI) + (atan2(reg->fit2raw[0][1], reg->fit2raw[1][1])
			- atan2(reg->fit2raw[1][0], reg->fit2raw[0][0])) / 2.0;

	rotateInt = (int) (rotate * 180 / M_PI + 0.5);
	if (rotateInt >= 360)
		rotateInt -= 360;

	fprintf(stdout, "--------------------------------------------------\n");
	fprintf(stdout, "       Matrix Size: %d x %d\n", dmtxGetSymbolAttribute(
			DmtxSymAttribSymbolRows, reg->sizeIdx), dmtxGetSymbolAttribute(
			DmtxSymAttribSymbolCols, reg->sizeIdx));
	fprintf(stdout, "    Data Codewords: %d (capacity %d)\n", dataWordLength
			- msg->padCount, dataWordLength);
	fprintf(stdout, "   Error Codewords: %d\n", dmtxGetSymbolAttribute(
			DmtxSymAttribSymbolErrorWords, reg->sizeIdx));
	fprintf(stdout, "      Data Regions: %d x %d\n", dmtxGetSymbolAttribute(
			DmtxSymAttribHorizDataRegions, reg->sizeIdx),
			dmtxGetSymbolAttribute(DmtxSymAttribVertDataRegions, reg->sizeIdx));
	fprintf(stdout, "Interleaved Blocks: %d\n", dmtxGetSymbolAttribute(
			DmtxSymAttribInterleavedBlocks, reg->sizeIdx));
	fprintf(stdout, "    Rotation Angle: %d\n", rotateInt);
	fprintf(stdout, "          Corner 0: (%0.1f, %0.1f)\n", p00.X, height - 1
			- p00.Y);
	fprintf(stdout, "          Corner 1: (%0.1f, %0.1f)\n", p10.X, height - 1
			- p10.Y);
	fprintf(stdout, "          Corner 2: (%0.1f, %0.1f)\n", p11.X, height - 1
			- p11.Y);
	fprintf(stdout, "          Corner 3: (%0.1f, %0.1f)\n", p01.X, height - 1
			- p01.Y);
	fprintf(stdout, "--------------------------------------------------\n");
}

/*
 *	createDmtxImageFromDib
 *
 */
DmtxImage * Decoder::createDmtxImageFromDib(Dib & dib) {
	int pack = DmtxPackCustom;
	unsigned padding = dib.getRowPadBytes();

	switch (dib.getBitsPerPixel()) {
	case 8:
		pack = DmtxPack8bppK;
		break;
	case 24:
		pack = DmtxPack24bppRGB;
		break;
	case 32:
		pack = DmtxPack32bppXRGB;
		break;
	}

	DmtxImage * image = dmtxImageCreate(dib.getPixelBuffer(), dib.getWidth(),
			dib.getHeight(), pack);

	//set the properties (pad bytes, flip)
	dmtxImageSetProp(image, DmtxPropRowPadBytes, padding);
	dmtxImageSetProp(image, DmtxPropImageFlip, DmtxFlipY); // DIBs are flipped in Y
	return image;
}

void Decoder::imageShowBarcodes(Dib & dib, bool regions) {
	UA_DOUT(3, 3, "marking tags ");

	RgbQuad quadWhite(255, 255, 255); // change to white (shows up better in grayscale)
	RgbQuad quadGreen(0, 255, 0); // change to white (shows up better in grayscale)
	RgbQuad quadRed(255, 0, 0);

	RgbQuad & highlightQuad = (dib.getBitsPerPixel() == 8 ? quadWhite : quadGreen);

	for (unsigned i = 0, n = barcodeInfos.size(); i < n; ++i) {
		BarcodeInfo & info = *barcodeInfos[i];
		DmtxPixelLoc & tlCorner = info.getTopLeftCorner();
		DmtxPixelLoc & brCorner = info.getBotRightCorner();

		dib.line(tlCorner.X, tlCorner.Y, tlCorner.X, brCorner.Y, highlightQuad);
		dib.line(tlCorner.X, brCorner.Y, brCorner.X, brCorner.Y, highlightQuad);
		dib.line(brCorner.X, brCorner.Y, brCorner.X, tlCorner.Y, highlightQuad);
		dib.line(brCorner.X, tlCorner.Y, tlCorner.X, tlCorner.Y, highlightQuad);
	}

	unsigned logLevel = ua::Logger::Instance().levelGet(3);

	if (logLevel == 0 || regions == 0)
		return;
	

	unsigned height = dib.getHeight() - 1;
	unsigned width = dib.getWidth() - 1;

	for (unsigned r = 0, rn = rowBinRegions.size(); r < rn; ++r) {
		BinRegion & region = *rowBinRegions[r];

		unsigned min = region.getMin();
		unsigned max = region.getMax();
		//unsigned center = region.getCenter();

		dib.line(0, min, width, min, highlightQuad);
		dib.line(0, min, 0, max, highlightQuad);
		dib.line(0, max, width, max, highlightQuad);
		dib.line(width, min, width, max, highlightQuad);
		//dib.line(0, center, width, center, quadRed);
	}

	for (unsigned c = 0, n = colBinRegions.size(); c < n; ++c) {
		BinRegion & region = *colBinRegions[c];

		unsigned min = region.getMin();
		unsigned max = region.getMax();
		//unsigned center = region.getCenter();

		dib.line(min, 0, max, 0, highlightQuad);
		dib.line(min, height, max, height, highlightQuad);
		dib.line(min, 0, min, height, highlightQuad);
		dib.line(max, 0, max, height, highlightQuad);
		//dib.line(center, 0, center, height, quadRed);
	}
}

