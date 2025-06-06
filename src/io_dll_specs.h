#pragma once
namespace AIDLL {
	constexpr int DLL_VER = 3;
	constexpr int FIELD_SIZE = 32;
	constexpr int NEXT_SIZE = 32;
	constexpr int UPCOMEATT_SIZE = 16;
	struct Field {
		int field[FIELD_SIZE];
		int width;
	};
	struct Queue {
		char next[NEXT_SIZE];
		char hold;
		char active;
		bool curCanHold;
		int x;
		int y;
		int r;
	};
	struct Status {
		int b2b;
		int combo;
		int upcomeAtt[UPCOMEATT_SIZE];
	};
	struct Config {
		int level;
		int season;
		int multiplier;
		int garbageCap;
		bool canHold;
		bool clutch;
		bool lockout;
		bool allow180;
	};
	typedef void (*CALL_INITAI)(const Config* config);
	typedef const char* (*CALL_TETRISAI)(const Field* field, const Queue* queue, const Status* status);
	typedef const char* (*CALL_AINAME)(int level);
	typedef int (*CALL_DLLVERSION)();
}