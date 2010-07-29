#ifndef MESSABE_INFO_H_
#define MESSABE_INFO_H_
/*
Dmscanlib is a software library and standalone application that scans 
and decodes libdmtx compatible test-tubes. It is currently designed 
to decode 12x8 pallets that use 2D data-matrix laser etched test-tubes.
Copyright (C) 2010 Canadian Biosample Repository

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
/*
 * Calibrator.h
 *
 *  Created on: 5-Jun-2009
 *      Author: loyola
 */

#include "Decoder.h"
#include "dmtx.h"
#include "UaLogger.h"
#include "UaAssert.h"

#include <string>
#include <limits>

using namespace std;

class Dib;
class BinRegion;

class BarcodeInfo {
public:
	BarcodeInfo(DmtxDecode *dec, DmtxRegion *reg, DmtxMessage *msg);
	~BarcodeInfo();

	string & getMsg() {
		return str;
	}
	bool equals(BarcodeInfo * other){
		return strcmp(this->getMsg().c_str(),other->getMsg().c_str()) == 0;
	}

	void getCorners(DmtxVector2 & p00, DmtxVector2 & p10,
			DmtxVector2 & p11, DmtxVector2 & p01);
	DmtxPixelLoc & getTopLeftCorner();
	DmtxPixelLoc & getBotRightCorner();

	void setColBinRegion(BinRegion * c) {
		UA_ASSERT_NOT_NULL(c);
		colBinRegion = c;
	}

	BinRegion & getColBinRegion() {
		UA_ASSERT_NOT_NULL(colBinRegion);
		return *colBinRegion;
	}

	void setRowBinRegion(BinRegion * c) {
		UA_ASSERT_NOT_NULL(c);
		rowBinRegion = c;
	}

	BinRegion & getRowBinRegion() {
		UA_ASSERT_NOT_NULL(rowBinRegion);
		return *rowBinRegion;
	}
	void alignCoordinates(int x, int y);

	static void removeItems(vector<BarcodeInfo *>  & msgInfos);
	static void debugShowItems(vector<BarcodeInfo *>  & msgInfos);

private:
	string str;
	DmtxVector2 p00, p10, p11, p01;
	BinRegion * colBinRegion;
	BinRegion * rowBinRegion;
	DmtxPixelLoc topLeft;
	DmtxPixelLoc botRight;

	void getBoundingBox() ;

	friend ostream & operator<<(ostream & os, BarcodeInfo & m);
	friend struct BarcodeInfoSort;
};

ostream & operator<<(ostream &os, BarcodeInfo & m);

struct BarcodeInfoSort {
	bool operator()(BarcodeInfo* const& a, BarcodeInfo* const& b);
};

#endif /* MESSABE_INFO_H_ */

