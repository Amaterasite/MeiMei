
#ifndef MT_UZUME_SRC_ROM_H_
#define MT_UZUME_SRC_ROM_H_

#include <fstream>
#include <string>
#include <iostream>

using namespace std;

typedef unsigned char	uchar;
typedef unsigned short	ushort;
typedef unsigned int	uint;

// 四角い車輪の再発明～
class Rom {

public:
	// romの種類
	enum ERomType {
		LOROM,HIROM,EXLOROM,EXHIROM,
	};

	// headerの有無
	enum EHeader {
		NO_HEADER,HEADER
	};

	// bank境界を跨いでいいか
	enum EBankBorder {
		NG,OK
	};


	Rom();
	Rom(string romName);
	Rom(string romName,Rom::ERomType romType,Rom::EHeader header);
	~Rom();

	// romを開く
	bool open(string romName);
	bool open(string romName,Rom::ERomType romType,Rom::EHeader header);

	// romの空き領域を探す
	// RATSタグも考慮する
	// 戻り値 空き領域の先頭アドレス -1で失敗
	int findFreeSpace(int startAddr,int size);
	int findFreeSpace(int startAddr,int endAddr,int size);
	int findFreeSpace(int startAddr,int endAddr,int size,Rom::EBankBorder border);

	// 指定されたデータ列を探す
	// 戻り値 データ列の先頭アドレス -1で失敗
	int findData(void* data,int size,int startAddr);
	int findData(void* data,int size,int startAddr,int endAddr);

	// 指定されたアドレスにRATSタグ及びデータを書き込む
	// 戻り値 非ゼロで成功
	int writeRATSdata(void* data,int size,int addr);

	// 指定されたアドレスがRATSタグ先頭なら保護されているデータを削除
	// 戻り値 削除した容量 0で失敗
	int eraseRATSdata(int addr);

	// 指定されたアドレスがRATSタグ先頭か確認
	// 戻り値 保護領域の大きさ
	// 0以下の場合RATSタグではない
	int checkRATSdata(int addr);

	// 指定した領域を削除
	// 非ゼロで成功
	int eraseData(int addr,int size);
	int eraseData(int addr,int size,bool alwaysMode);

	// 指定されたアドレスにデータを書き込む
	// 非ゼロで成功
	int writeData(void* data,int size,int addr);

	// 指定されたアドレスにデータを繰り返し書き込む
	// 非ゼロで成功
	int writeReptData(void* data,int dataSize,int size,int addr);

	// 指定されたアドレスからデータを読み込む
	// 非ゼロで成功
	int readData(void* data,int size,int addr);

	// 編集したデータを保存
	// 非ゼロで成功
	int writeRomFile();

	// アクセッサ周り

	bool isOpen();						// ファイルが開けたか確認
	void* getRomDataPtr();				// ファイルバッファのポインタを取得
	int getRomSize();					// rom容量取得
	void setFreeSpaceNum(uchar num);	// 空き領域とする値を変更

	int getRomData(int addr);			// romデータ読み込み
	int setRomData(int addr,uchar num);// romデータ書き込み

private:
	// 事実上のコンストラクタ
	void init (string romName,Rom::ERomType romType,Rom::EHeader header);

	bool inline checkAddr(int addr,int size);

	fstream romFile;
	string romPath;

	// romを開けたか
	bool fileOpen = false;

	// romの容量
	int romSize = 0;

	// 空き領域とする値 デフォルトではゼロ
	// データ削除時はこの値で領域を埋める
	uchar freeSpaceNum = 0x00;

	// romのデータ
	uchar* romData = (uchar*)0;

	// romの種類
	// SMWの場合LOROM
	ERomType romType = LOROM;

	// .smcファイルの512bytesヘッダの有無
	// 通常はヘッダあり
	EHeader header = HEADER;

};

#endif /* MT_UZUME_SRC_ROM_H_ */
