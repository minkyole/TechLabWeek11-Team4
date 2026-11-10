#pragma once

// 논리 이름(Lua에서 쓰는 이름)과 실제 FBX 클립 키를 한 곳에 고정해 팀원 모두 같은 클립을 보게 한다.
struct FCommonAnimClip
{
	const char* Alias;
	const char* AssetKey; // AssetManager의 FBX 키 + "#Anim/" + AnimStack 이름
};

namespace CommonAnimClips
{
	inline constexpr FCommonAnimClip Clips[] =
	{
		{ "Idle", "Models/UAL1_Standard.fbx#Anim/Armature|Idle_Loop" },
		{ "Walk", "Models/UAL1_Standard.fbx#Anim/Armature|Walk_Loop" },
		{ "Run", "Models/UAL1_Standard.fbx#Anim/Armature|Jog_Fwd_Loop" },
		{ "Attack", "Models/UAL1_Standard.fbx#Anim/Armature|Sword_Attack" },
		{ "Hit", "Models/UAL1_Standard.fbx#Anim/Armature|Hit_Chest" },
		{ "JumpStart", "Models/UAL1_Standard.fbx#Anim/Armature|Jump_Start" },
		{ "JumpLoop", "Models/UAL1_Standard.fbx#Anim/Armature|Jump_Loop" },
		{ "JumpLand", "Models/UAL1_Standard.fbx#Anim/Armature|Jump_Land" },
	};
}
