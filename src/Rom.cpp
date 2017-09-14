
#include "Rom.h"


void Rom::init (string romPath,Rom::ERomType romType,Rom::EHeader header) {
	Rom::romType = romType;
	Rom::header = header;
	Rom::romPath = romPath;

	romFile.open(romPath.c_str(),ios::in|ios::binary);
	fileOpen = romFile;
	if(!fileOpen)	return;

	// サイズ確認
	romFile.seekg(0,ios::end);
	romSize = romFile.tellg();
	if(header == HEADER) {
		romSize -=0x0200;
		romFile.seekg(0x0200);
	}
	else {
		romFile.seekg(0);
	}

	// ファイルをバッファにコピー
	romData = new uchar[romSize];
	for(int i=0;i<romSize;i++) {
		romData[i] = romFile.get();
	}
	romFile.close();
}
Rom::Rom() {
}

Rom::Rom(string romPath) {
	init(romPath,LOROM,HEADER);
}

Rom::Rom(string romPath,Rom::ERomType romType,Rom::EHeader header) {
	init(romPath,romType,header);
}

Rom::~Rom() {
	if(!fileOpen) {
		delete[] romData;
	}
}

bool Rom::open(string romPath) {
	init(romPath,LOROM,HEADER);
	return fileOpen;
}

bool Rom::open(string romPath,Rom::ERomType romType,Rom::EHeader header) {
	init(romPath,romType,header);
	return fileOpen;
}


int Rom::eraseData(int addr,int size) {
	return eraseData(addr,size,false);
}

int Rom::eraseData(int addr,int size,bool alwaysMode) {
	// romSizeから飛び出さないか確認
	if(!checkAddr(addr,size)) return 0;

	// 強制削除モードでない場合、範囲内にRATSタグが存在するか事前に確認
	if(!alwaysMode) {
		for(int addr2 = addr;addr2 < addr+size;addr2++) {
			if(checkRATSdata(addr2)>0) return 0;
		}
	}
	// 領域を削除
	while(size>0) {
		romData[addr++] = freeSpaceNum;
		size--;
	}
	return 1;
}

int Rom::writeData(void* data,int size,int addr) {
	if(!checkAddr(addr,size)) return 0;

	uchar* dataPtr = (uchar*)data;
	for(int i=0;i<size;) {
		romData[addr++] = dataPtr[i++];
	}
	return 1;
}

int Rom::writeReptData(void* data,int dataSize,int size,int addr) {
	if(!checkAddr(addr,size)) return 0;

	uchar* dataPtr = (uchar*)data;
	for(int i=0;i<size;i++) {
		romData[addr++] = dataPtr[(i%dataSize)];
	}

	return 1;
}

int Rom::readData(void* data,int size,int addr) {
	if(!checkAddr(addr,size)) return 0;

	uchar* dataPtr = (uchar*)data;
	for(int i=0;i<size;) {
		dataPtr[i++] = romData[addr++];
	}
	return 1;
}

int Rom::checkRATSdata(int addr) {
	if(checkAddr(addr,8)) {
		// ゴリ押しは正義
		if(romData[addr]=='S' && romData[addr+1]=='T' && romData[addr+2]=='A' && romData[addr+3]=='R') {
			ushort size =		((romData[addr+5]<<8) + romData[addr+4]);
			ushort invSize =	((romData[addr+7]<<8) + romData[addr+6]);
			if((size ^ invSize) == 0xFFFF) {
				return (int)size+1;
			}
		}
	}
	return -1;
}

int Rom::eraseRATSdata(int addr) {
	int size = checkRATSdata(addr);
	if(size <= 0) return 0;
	eraseData(addr,size+8,true);
	return size+8;
}

// RATSタグ込みのデータ書き込み
// void* data - データへのポインタ nullptrを指定するとデータは書き込まない
// int   size - データのサイズ
// int   addr - 書き込み先
int Rom::writeRATSdata(void* data,int size,int addr) {
	if(addr<0 || addr+8 >= romSize || size<0 || size>0x010000) return 0;
	romData[addr]	= 'S';
	romData[addr+1]	= 'T';
	romData[addr+2]	= 'A';
	romData[addr+3]	= 'R';

	size--;
	ushort invSize = (ushort)((size)^0xFFFF);
	romData[addr+4] = (uchar)size;
	romData[addr+5] = (uchar)(size>>8);
	romData[addr+6] = (uchar)invSize;
	romData[addr+7] = (uchar)(invSize>>8);

	if(data != nullptr) {
		addr+=8;
		uchar* dataPtr = (uchar*)data;
		for(int i=0;i<=size;i++) {
			romData[addr++] = dataPtr[i];
		}
	}
	return 1;
}


int Rom::findFreeSpace(int startAddr,int endAddr,int size,Rom::EBankBorder border) {
	int addr = startAddr;
	int freeCount = 0;
	int RATSsize = 0;
	uchar buf;

	// とりまLoRomだけ対応～
	if(romType == LOROM) {
		while(addr < endAddr) {
			if(freeCount > 0) {
				// 1byte取り出し 空き領域か確認
				buf = romData[addr++];
				if(buf == freeSpaceNum) {
					// 要求分の空き領域を発見できた場合おしまい。
					if(++freeCount == size)	return (addr - size);
				}
				// 空きじゃなかったんで最初から
				else freeCount = 0;
			}
			// 空き領域探索前にやること
			else{
				// RATS保護範囲内はスルー
				RATSsize = checkRATSdata(addr);
				if(RATSsize > 0) {
					addr = addr+RATSsize+8;
					continue;
				}

				// バンク跨ぎ不許可で、現在のBankに収まり切らない事が判明した場合次のBankに移る
				if((border == NG) && ((addr&0x7FFF) + size > 0x8000)) {
					//int addr2 = (addr&0xFFFF8000) + 0x8000;
					//printf("Bank跨ぎ: %06X->%06X\n",addr,addr2);
					//addr = addr2;
					addr = (addr & 0xFFFF8000) + 0x8000;
					continue;
				}
				buf = romData[addr++];
				if(buf == freeSpaceNum) freeCount++;
			}
		}
	}
	return -1;
}

int Rom::findFreeSpace(int startAddr,int endAddr,int size) {
	return findFreeSpace(startAddr,endAddr,size,NG);
}

int Rom::findFreeSpace(int startAddr,int size) {
	return findFreeSpace(startAddr,romSize,size,NG);
}

int Rom::findData(void* data,int size,int startAddr,int endAddr) {
	if(!checkAddr(startAddr,size)) return -1;
	int count = 0;
	uchar* dataPtr = (uchar*)data;
	for(int addr=startAddr;addr<endAddr;addr++) {
		if(romData[addr] == dataPtr[count]) {
			if(++count >= size) {
				return addr - size + 1;
			}
		}
		else {
			count = 0;
		}
	}
	return -1;
}

int Rom::findData(void* data,int size,int startAddr) {
	return findData(data,size,startAddr,romSize);
}

int Rom::writeRomFile() {
	romFile.open(romPath.c_str(),ios::in|ios::out|ios::binary);
	if(!romFile) return 0;
	if(header == EHeader::HEADER)	romFile.seekg(0x0200);
	else							romFile.seekg(0x0000);
	romFile.write((const char*)romData,romSize);
	romFile.close();
	return 1;
}

bool Rom::isOpen() {
	return fileOpen;
}

void* Rom::getRomDataPtr() {
	return (void*)romData;
}

int Rom::getRomSize() {
	return romSize;
}

int Rom::getRomData(int addr) {
	if(addr<0 || addr>=romSize) return -1;
	return romData[addr];
}

void Rom::setFreeSpaceNum(uchar num) {
	freeSpaceNum = num;
}

int Rom::setRomData(int addr,uchar num) {
	if(addr<0 || addr>=romSize) return 0;
	romData[addr] = num;
	return 1;
}

bool inline Rom::checkAddr(int addr,int size) {
	return (addr >= 0 && addr + size < romSize);
}
