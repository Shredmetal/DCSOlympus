#pragma once
#include "framework.h"
#include "luatools.h"
#include "utils.h"
#include "logger.h"

namespace CommandPriority {
	enum CommandPriorities { LOW, MEDIUM, HIGH };
};

namespace SetCommandType {
	enum SetCommandTypes {
		ROE = 0,
		REACTION_ON_THREAT = 1,
		RADAR_USING = 3,
		FLARE_USING = 4,
		FORMATION = 5,
		RTB_ON_BINGO = 6,
		SILENCE = 7,
		RTB_ON_OUT_OF_AMMO = 10,
		ECM_USING = 13,
		PROHIBIT_AA = 14,
		PROHIBIT_JETT = 15,
		PROHIBIT_AB = 16,
		PROHIBIT_AG = 17,
		MISSILE_ATTACK = 18,
		PROHIBIT_WP_PASS_REPORT = 19,
		ENGAGE_AIR_WEAPONS = 20,
		OPTION_RADIO_USAGE_CONTACT = 21,
		OPTION_RADIO_USAGE_ENGAGE = 22,
		OPTION_RADIO_USAGE_KILL = 23,
		JETT_TANKS_IF_EMPTY = 25,
		FORCED_ATTACK = 26
	};
}

namespace ROE {
	enum ROEs {
		WEAPON_FREE = 0,
		OPEN_FIRE_WEAPON_FREE = 1,
		OPEN_FIRE = 2,
		RETURN_FIRE = 3,
		WEAPON_HOLD = 4,
	};
}

namespace ReactionToThreat {
	enum ReactionsToThreat {
		NO_REACTION = 0,
		PASSIVE_DEFENCE = 1,
		EVADE_FIRE = 2,
		BYPASS_AND_ESCAPE = 3,
		ALLOW_ABORT_MISSION = 4
	};
}

namespace RadarUse {
	enum RadarUses {
		NEVER = 0,
		FOR_ATTACK_ONLY = 1,
		FOR_SEARCH_IF_REQUIRED = 2,
		FOR_CONTINUOUS_SEARCH = 3
	};
}

namespace FlareUse {
	enum FlareUses {
		NEVER = 0,
		AGAINST_FIRED_MISSILE = 1,
		WHEN_FLYING_IN_SAM_WEZ = 2,
		WHEN_FLYING_NEAR_ENEMIES = 3
	};
}

namespace ECMUse {
	enum ECMUses {
		NEVER_USE = 0,
		USE_IF_ONLY_LOCK_BY_RADAR = 1,
		USE_IF_DETECTED_LOCK_BY_RADAR = 2,
		ALWAYS_USE = 3
	};
}



/* Base command class */
class Command
{
public:
	int getPriority() { return priority; }
	virtual wstring getString(lua_State* L) = 0;
	virtual int getLoad() = 0;

protected:
	int priority = CommandPriority::LOW;
};

/* Simple low priority move command (from user click) */
class Move : public Command
{
public:
	Move(int ID, Coords destination, double speed, double altitude, wstring taskOptions):
		ID(ID),
		destination(destination),
		speed(speed),
		altitude(altitude),
		taskOptions(taskOptions)
	{ 
		priority = CommandPriority::HIGH; 
	};
	virtual wstring getString(lua_State* L);
	virtual int getLoad() { return 5; }

private:
	const int ID;
	const Coords destination;
	const double speed;
	const double altitude;
	const wstring taskOptions;
};

/* Smoke command */
class Smoke : public Command
{
public:
	Smoke(wstring color, Coords location) : 
		color(color), 
		location(location) 
	{ 
		priority = CommandPriority::LOW; 
	};
	virtual wstring getString(lua_State* L);
	virtual int getLoad() { return 5; }

private:
	const wstring color;
	const Coords location;
};

/* Spawn ground unit command */
class SpawnGroundUnit : public Command
{
public:
	SpawnGroundUnit(wstring coalition, wstring unitType, Coords location) :  
		coalition(coalition), 
		unitType(unitType), 
		location(location) 
	{ 
		priority = CommandPriority::LOW; 
	};
	virtual wstring getString(lua_State* L);
	virtual int getLoad() { return 100; }

private:
	const wstring coalition;
	const wstring unitType;
	const Coords location;
};

/* Spawn air unit command */
class SpawnAircraft : public Command
{
public:
	SpawnAircraft(wstring coalition, wstring unitType, Coords location, wstring payloadName, wstring airbaseName) :
		coalition(coalition), 
		unitType(unitType), 
		location(location),
		payloadName(payloadName),
		airbaseName(airbaseName)
	{ 
		priority = CommandPriority::LOW; 
	};
	virtual wstring getString(lua_State* L);
	virtual int getLoad() { return 100; }

private:
	const wstring coalition;
	const wstring unitType;
	const Coords location;
	const wstring payloadName;
	const wstring airbaseName;
};

/* Clone unit command */
class Clone : public Command
{
public:
	Clone(int ID, Coords location) :
		ID(ID),
		location(location)
	{
		priority = CommandPriority::LOW;
	};
	virtual wstring getString(lua_State* L);
	virtual int getLoad() { return 100; }

private:
	const int ID;
	const Coords location;
};

/* Delete unit command */
class Delete : public Command
{
public:
	Delete(int ID) :
		ID(ID)
	{
		priority = CommandPriority::HIGH;
	};
	virtual wstring getString(lua_State* L);
	virtual int getLoad() { return 20; }

private:
	const int ID;
};

/* Follow command */
class SetTask : public Command
{
public:
	SetTask(int ID, wstring task) :
		ID(ID),
		task(task)
	{
		priority = CommandPriority::MEDIUM;
	};
	virtual wstring getString(lua_State* L);
	virtual int getLoad() { return 10; }

private:
	const int ID;
	const wstring task;
};

/* Reset task command */
class ResetTask : public Command
{
public:
	ResetTask(int ID) :
		ID(ID)
	{
		priority = CommandPriority::HIGH;
	};
	virtual wstring getString(lua_State* L);
	virtual int getLoad() { return 10; }

private:
	const int ID;
};

/* Set command */
class SetCommand : public Command
{
public:
	SetCommand(int ID, wstring command) :
		ID(ID),
		command(command)
	{
		priority = CommandPriority::HIGH;
	};
	virtual wstring getString(lua_State* L);
	virtual int getLoad() { return 10; }

private:
	const int ID;
	const wstring command;
};

/* Set option command */
class SetOption : public Command
{
public:
	SetOption(int ID, int optionID, int optionValue) :
		ID(ID),
		optionID(optionID),
		optionValue(optionValue),
		optionBool(false),
		isBoolean(false)
	{
		priority = CommandPriority::HIGH;
	};

	SetOption(int ID, int optionID, bool optionBool) :
		ID(ID),
		optionID(optionID),
		optionValue(0),
		optionBool(optionBool),
		isBoolean(true)
	{
		priority = CommandPriority::HIGH;
	};
	virtual wstring getString(lua_State* L);
	virtual int getLoad() { return 10; }

private:
	const int ID;
	const int optionID;
	const int optionValue;
	const bool optionBool;
	const bool isBoolean;
};