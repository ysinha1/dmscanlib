#ifndef DECODER_H_
#define DECODER_H_

/*
 * Decoder.h
 *
 *  Created on: 22-May-2009
 *      Author: loyola
 */

#include "dmtx.h"

class Dib;
class LinkList;

class Decoder {
public:
	Decoder(Dib * dib);
	virtual ~Decoder();

	void decodeDib(Dib * dib);

	unsigned getNumTags();
	char * getTag(int tagNum);

private:
	DmtxImage* image;
	LinkList * results;

	void createDmtxImageFromDib(Dib * dib);

};

#endif /* DECODER_H_ */