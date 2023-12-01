#include <vector>
#define DEFAULT 1
#define PISTOL 2
#define DMR 3
#define RIFLE 4
namespace no_recoil {
	inline int strengthX = 0;
	// neg value means left, pos means right
	inline int strengthY = 0;
	inline bool active = false;
	inline float smoothing = 1.f;
	inline POINT currentPos;
	inline POINT previousPos;
	inline int pull_delay = 0;
	inline bool smart_recoil = false;
	inline bool within_program = false;
	inline float multiplier = 1.0f;
}

namespace macros {
	inline bool bhop = false;
	inline bool rapid_fire = false;
	inline bool turbo_crouch = false;
	inline bool isActive = false;
}

namespace preset {
	inline int CurrentPreset = DEFAULT;
	inline std::vector<int> PresetList{ DEFAULT, PISTOL, DMR, RIFLE};
	inline void SetPreset(int Index)
	{
		CurrentPreset = PresetList.at(Index);
	}
}

namespace bhop {
	inline int HotKey = VK_LMENU;
	inline std::vector<int> HotKeyList{ VK_LMENU, VK_LBUTTON, VK_RBUTTON, VK_XBUTTON1, VK_XBUTTON2, VK_CAPITAL, VK_LSHIFT, VK_LCONTROL };

	inline void SetHotKey(int Index)
	{
		HotKey = HotKeyList.at(Index);
	}
}

namespace crouch {
	inline int HotKey = VK_LMENU;
	inline std::vector<int> HotKeyList{ VK_LMENU, VK_LBUTTON, VK_RBUTTON, VK_XBUTTON1, VK_XBUTTON2, VK_CAPITAL, VK_LSHIFT, VK_LCONTROL };

	inline void SetHotKey(int Index)
	{
		HotKey = HotKeyList.at(Index);
	}
}