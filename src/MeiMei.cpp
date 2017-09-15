
#include <iostream>
#include <fstream>
#include <string>

#include "windows.h"
#include "Rom.h"

using namespace std;

typedef unsigned int uint;
typedef unsigned short ushort;
typedef unsigned char uchar;

const int SPR_ADDR_LIMIT = 0x800;

#define ERR(msg) printf("ERROR: "); printf(msg); goto end;

uchar readByte(Rom* r, int addr) {
	uchar tmp;
	r->readData(&tmp, 1, addr);
	return tmp;
}

ushort readWord(Rom* r, int addr) {
	uchar tmp[2];
	r->readData(&tmp, 2, addr);
	return tmp[0]|(tmp[1]<<8);
}

uint readLong(Rom* r, int addr) {
	uchar tmp[3];
	r->readData(&tmp, 3, addr);
	return tmp[0]|(tmp[1]<<8)|(tmp[2]<<16);
}

void writeByte(Rom* r, int addr, uchar Data) {
	r->writeData(&Data, 1, addr);
}

void writeWord(Rom* r, int addr, ushort Data) {
	uchar tmp[2] = {(uchar)Data, (uchar)(Data>>8)};
	r->writeData(&tmp, 2, addr);
}

void writeLong(Rom* r, int addr, ushort Data) {
	uchar tmp[3] = {(uchar)Data, (uchar)(Data>>8), (uchar)(Data>>16)};
	r->writeData(&tmp, 3, addr);
}

int SNEStoPC(int addr) {
	return (addr&0x7FFF)+((addr&0x7F0000)>>1);
}

int PCtoSNES(int addr) {
	return ((addr&0x7FFF)+0x8000)+((addr&0x3F8000)<<1);
}

int main(int argc, char* argv[]) {
	bool debug = false;
	bool always = false;
	string romName;
	string pixiOption = "PIXI.exe";

	if(argc == 2 && (!strcmp(argv[1], "-h") || !strcmp(argv[1], "--h") || !strcmp(argv[1], "--help"))) {
		printf("MeiMei version 0.01 by Akaginite\n");
		printf("Usage: MeiMei [MeiMei's option] [PIXI's option] [Rom file]\n");
		printf("--d : Enable debug output\n");
		printf("--a : Enable always remap sprite data\n");
		printf("--p : Set PIXI's option\n\n");
	} else if(argc >= 2) {
		romName = argv[argc-1];

		for(int i=1;i<argc-1;) {
			if(!strcmp(argv[i], "--d")) {
				debug = true;
				i++;
			} else if(!strcmp(argv[i], "--a")) {
				always = true;
				i++;
			} else if(!strcmp(argv[i], "--p")) {
				for(i=i+1;i<argc-1;i++) {
					pixiOption += " ";
					pixiOption += argv[i];
				}
			} else {
				printf("Error: Invalid Argument\n");
				return 0;
			}
		}
	}
	while(true) {
		ifstream romFile;
		if(romName == "") {
			printf("Input rom name : ");
			cin >> romName;
		}
		romFile.open(romName.c_str());
		if(!romFile) {
			printf("%s is not found\n", romName.c_str());
			romName = "";
			continue;
		}
		romFile.close();
		break;
	}
	uchar prevEx[0x400];
	uchar nowEx[0x400];
	for(int i=0;i<0x400;i++) {
		prevEx[i] = 0x03;
		nowEx[i] = 0x03;
	}

	Rom* prev = new Rom(romName);
	if(readByte(prev, 0x07730F)==0x42) {
		int addr = SNEStoPC((int)readLong(prev, 0x07730C));
		prev->readData(&prevEx, 0x0400, addr);
	}

	pixiOption = pixiOption + " " + romName;
	system(pixiOption.c_str());

	Rom* now = new Rom(romName);
	if(readByte(prev, 0x07730F)==0x42) {
		int addr = SNEStoPC((int)readLong(now, 0x07730C));
		now->readData(&nowEx, 0x0400, addr);
	}

	bool changeEx = false;
	for(int i=0;i<0x400;i++) {
		if(prevEx[i] != nowEx[i]) {
			changeEx = true;
			break;
		}
	}
	bool revert = changeEx||always;
	if(changeEx) {
		printf("\nExtra bytes change detected\n");
	}
	if(changeEx||always) {
		uchar sprAllData[SPR_ADDR_LIMIT];
		uchar sprCommonData[3];
		bool remapped[0x0200];
		for(int i=0;i<0x0200;i++) {
			remapped[i] = false;
		}

		for(int lv=0;lv<0x200;lv++) {
			if(remapped[lv])	continue;
			int sprAddrSNES = (readByte(now, 0x077100+lv)<<16)+readWord(now, 0x02EC00+lv*2);
			if((sprAddrSNES&0x8000) == 0) {
				ERR("Sprite Data has invalid address.");
			}
			int sprAddrPC = SNEStoPC(sprAddrSNES);

			for(int i=0;i<SPR_ADDR_LIMIT;i++) {
				sprAllData[i] = 0;
			}
			sprAllData[0] = readByte(now, sprAddrPC);
			int prevOfs = 1;
			int nowOfs = 1;
			bool changeData = false;
			#define OF_NOW_OFS() if(nowOfs>=SPR_ADDR_LIMIT) {ERR("Sprite data is too large!")};

			while(true) {
				now->readData(&sprCommonData, 3, sprAddrPC+prevOfs);
				if(nowOfs >= SPR_ADDR_LIMIT-3) {
					ERR("Sprite data is too large!");
				}
				if(sprCommonData[0]==0xFF) {
					sprAllData[nowOfs++] = 0xFF;
					break;
				}
				sprAllData[nowOfs++] = sprCommonData[0];	// YYYYEEsy
				sprAllData[nowOfs++] = sprCommonData[1];	// XXXXSSSS
				sprAllData[nowOfs++] = sprCommonData[2];	// NNNNNNNN

				int sprNum = ((sprCommonData[0]&0x0C)<<6)|(sprCommonData[2]);

				if(nowEx[sprNum] > prevEx[sprNum]) {
					changeData = true;
					int i;
					for(i=3;i<prevEx[sprNum];i++) {
						sprAllData[nowOfs++] = readByte(now, sprAddrPC+prevOfs+i);
						OF_NOW_OFS();
					}
					for(;i<nowEx[sprNum];i++) {
						sprAllData[nowOfs++] = 0x00;
						OF_NOW_OFS();
					}
				} else if(nowEx[sprNum] < prevEx[sprNum]){
					changeData = true;
					for(int i=3;i<nowEx[sprNum];i++) {
						sprAllData[nowOfs++] = readByte(now, sprAddrPC+prevOfs+i);
						OF_NOW_OFS();
					}
				} else {
					for(int i=3;i<nowEx[sprNum];i++) {
						sprAllData[nowOfs++] = readByte(now, sprAddrPC+prevOfs+i);
						OF_NOW_OFS();
					}
				}
				prevOfs += prevEx[sprNum];
			}
			prevOfs++;
			int remappedAddrSNES = sprAddrSNES;
			int remappedAddrPC = sprAddrPC;
			if(changeData) {
				remappedAddrPC = now->findFreeSpace(0x080000, nowOfs+8);
				if(remappedAddrPC < 0) {
					ERR("Free space is not found.\n");
				}

				if((sprAddrSNES&0x7FFFFF) < 0x100000) {
					now->eraseData(sprAddrPC, prevOfs);
				} else {
					now->eraseRATSdata(sprAddrPC-8);
				}
				now->writeRATSdata(sprAllData, nowOfs, remappedAddrPC);
				remappedAddrSNES = PCtoSNES(remappedAddrPC+8);
			}
			for(int i=lv;i<0x200;i++) {
				if(remapped[i])	continue;
				if(sprAddrSNES == (readByte(now, 0x077100+i)<<16)+readWord(now, 0x02EC00+i*2)) {
					writeByte(now, 0x077100+i, (uchar)(remappedAddrSNES>>16));
					writeWord(now, 0x02EC00+i*2, ((ushort)(remappedAddrSNES)));
					remapped[i] = true;
				}
			}
			if(debug) {
				printf("Level%03X Address:$%06X (SNES:$%06X) Remap:%s\n", lv, sprAddrPC+0x0200, sprAddrSNES, changeData ? "Yes" : "No");
				if(changeData) {
					printf("Data size:$%04X -> $%04X\n", prevOfs, nowOfs);
					printf("Remapped Address:$%06X (SNES:$%06X)\n\n", remappedAddrPC+0x0200, remappedAddrSNES);
				}
//				printf("Sprite Data:\n");
//				for(int i=0;i<nowOfs;i++) {
//					printf("%02X ", sprAllData[i]);
//				}
			}
		}
		printf("Sprite data remapped successfully.\n");
		revert = false;
	}
	now->writeRomFile();
end:
	if(revert) {
		prev->writeRomFile();
		printf("\n\nError occureted. Your rom has reverted to before insert.\n");
	}
	delete prev;
	delete now;

	return 0;
}


