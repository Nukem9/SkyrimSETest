#pragma once

class MemoryContextTracker
{
private:
	uint32_t& m_Id;
	uint32_t m_OldId;

public:
	enum
	{
		CORE_SYSTEM,
		CORE_STATIC_VARIABLES,
		CORE_UNKOWN,
		CORE_POOLS,
		CORE_TASK,
		CORE_SMALL_BLOCK,
		DEBUG_SYSTEM,
		DEBUG_DATA,
		FILE_SYSTEM,
		FILE_STREAM,
		FILE_BUFFER,
		FILE_ZIP,
		FILE_DATABASE_OVERHEAD,
		FILE_MODEL_DATABASE,
		FILE_TEXTURE_DATABASE,
		FILE_JSON,
		THREAD_SYSTEM,
		THREAD_JOBS,
		BETHEDA_NET_SYSTEM,
		UNDEFINED_1,
		VM_SYSTEM,
		VM_TASKLET,
		VM_OBJECT,
		VM_TYPES,
		VM_BINDINGS,
		VM_GAME,
		RENDER_SYSTEM,
		RENDER_SHADER,
		RENDER_SHADER_SYSTEM,
		RENDER_SHADOWS,
		RENDER_PROPERTY,
		RENDER_GEOMETRY,
		RENDER_ACCUMULATOR,
		RENDER_IMAGESPACE,
		RENDER_GRASS,
		RENDER_DECAL,
		RENDER_WATER,
		RENDER_TREES,
		RENDER_MULTI_INDEX,
		RENDER_TARGET,
		RENDER_TEXTURE,
		RENDER_PRT,
		RENDER_RSX,
		AUDIO_SYSTEM,
		AUDIO_SOUND,
		AUDIO_VOICE,
		AUDIO_MUSIC,
		HAVOK_SYSTEM,
		HAVOK_WORLD,
		HAVOK_ACTION,
		HAVOK_CONSTRAINT,
		HAVOK_RIGIDBODY,
		HAVOK_PHANTOM,
		HAVOK_SHAPE,
		HAVOK_CONTROLLER,
		HAVOK_COLLECTION,
		HAVOK_LISTENER,
		HAVOK_MOPP,
		HAVOK_BEHAVIOR,
		HAVOK_KEYFRAME,
		HAVOK_POSE,
		GAMEBRYO_SYSTEM,
		GAMEBRYO_EXTRA_DATA,
		GAMEBRYO_ANIMATION,
		GAMEBRYO_SKIN,
		GAMEBRYO_SCENEGRAPH,
		GAMEBRYO_PARTICLES,
		GAMEBRYO_MESH,
		GAMEBRYO_TEXTURE,
		GAMEBRYO_COLLISION,
		USER_INTERFACE_SYSTEM,
		USER_INTERFACE_FILE,
		USER_INTERFACE_SCALEFORM,
		USER_INTERFACE_MOVIE,
		USER_INTERFACE_KINECT,
		NAVMESH_SYSTEM,
		NAVMESH_DATA,
		NAVMESH_METADATA,
		NAVMESH_PATH,
		NAVMESH_OBSTACLE,
		NAVMESH_MOVEMENT,
		FACEGEN_SYSTEM,
		FACEGEN_TEXTURE,
		FACEGEN_MESH,
		FACEGEN_ANIM,
		LOD_SYSTEM,
		LOD_LAND,
		LOD_TREE,
		LOD_OBJECTS,
		GAME_SYSTEM,
		GAME_MISC,
		GAME_SAVELOAD,
		GAME_SCREENSHOT,
		GAME_SKY,
		GAME_HAZARD,
		GAME_EFFECTS,
		GAME_EXPLOSION,
		GAME_EXTRA_DATA,
		GAME_INVENTORY,
		GAME_MAP,
		MASTERFILE_DATA,
		GAME_FORMS,
		GAME_SETTINGS,
		GAME_REFERENCE,
		GAME_ACTOR,
		GAME_PLAYER,
		GAME_CELL,
		GAME_WORLD,
		GAME_TERRAIN,
		GAME_PROJECTILE,
		GAME_SCENE_DATA,
		GAME_QUESTS,
		AI_HIGH,
		AI_MIDDLE_HIGH,
		AI_LOW,
		AI_PROCESS,
		AI_COMBAT,
		AI_DIALOGUE,
		SCRATCH_ONE,
		SCRATCH_TWO,
		SCRATCH_THREE,
		SCRATCH_FOUR,
		HEAP_ZEROOVERHEAD,
		HEAP_BSSYSTEMPHYS,
		HEAP_BSBLOCKMEM,
		MODULES,
		BSRESOURCE,
		FACEGEN,
		GAME_OVERHEAD,
		GAMEBRYO_OVERHEAD,
		MASTERFILE,
		SAVE_DATA,
		SYSTEM,
		BETHESDA_NET,
		UNKNOWN_SYSTEM,
		UNTRACKED,
		SCRATCH,
		TOTALS,
	};

	inline MemoryContextTracker(uint32_t Id, const char *File) : m_Id(GAME_TLS(uint32_t, 0x768))
	{
		m_OldId = m_Id;
		m_Id = Id;
	}

	inline ~MemoryContextTracker()
	{
		m_Id = m_OldId;
	}
};