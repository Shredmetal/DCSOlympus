#include "unit.h"
#include "utils.h"
#include "logger.h"
#include "commands.h"
#include "scheduler.h"
#include "defines.h"
#include "unitsmanager.h"

#include <chrono>
using namespace std::chrono;

#include <GeographicLib/Geodesic.hpp>
using namespace GeographicLib;

extern Scheduler* scheduler;
extern UnitsManager* unitsManager;

// TODO: Make dedicated file
bool operator==(const Options::TACAN& lhs, const Options::TACAN& rhs)
{
	return lhs.isOn == rhs.isOn && lhs.channel == rhs.channel && lhs.XY == rhs.XY && lhs.callsign == rhs.callsign;
}

bool operator==(const Options::Radio& lhs, const Options::Radio& rhs)
{
	return lhs.frequency == rhs.frequency && lhs.callsign == rhs.callsign && lhs.callsignNumber == rhs.callsignNumber;
}

bool operator==(const Options::GeneralSettings& lhs, const Options::GeneralSettings& rhs)
{
	return	lhs.prohibitAA == rhs.prohibitAA && lhs.prohibitAfterburner == rhs.prohibitAfterburner && lhs.prohibitAG == rhs.prohibitAG &&
			lhs.prohibitAirWpn == rhs.prohibitAirWpn && lhs.prohibitJettison == rhs.prohibitJettison;
}

Unit::Unit(json::value json, int ID) :
	ID(ID)
{
	log("Creating unit with ID: " + to_string(ID));
}

Unit::~Unit()
{

}

void Unit::initialize(json::value json)
{
	updateExportData(json);

	if (getAI()) {
		/* Set the default IDLE state */
		setState(State::IDLE);

		/* Set the default options (these are all defaults so will only affect the export data, no DCS command will be sent) */
		setROE(L"Designated");
		setReactionToThreat(L"Evade");
		setEmissionsCountermeasures(L"Defend");
		setTACAN(TACAN);
		setRadio(radio);
		setEPLRS(EPLRS);
		setGeneralSettings(generalSettings);
	}
}

void Unit::addMeasure(wstring key, json::value value)
{
	milliseconds ms = duration_cast<milliseconds>(system_clock::now().time_since_epoch());
	if (measures.find(key) == measures.end())
		measures[key] = new Measure(value, ms.count());
	else
	{
		if (measures[key]->getValue() != value)
		{
			measures[key]->setValue(value);
			measures[key]->setTime(ms.count());
		}
	}
}

void Unit::updateExportData(json::value json)
{
	/* Compute speed (loGetWorldObjects does not provide speed, we compute it for better performance instead of relying on many lua calls) */
	if (oldPosition != NULL)
	{
		double dist = 0;
		Geodesic::WGS84().Inverse(latitude, longitude, oldPosition.lat, oldPosition.lng, dist);
		setSpeed(getSpeed() * 0.95 + (dist / UPDATE_TIME_INTERVAL) * 0.05);
	}
	oldPosition = Coords(latitude, longitude, altitude);

	if (json.has_string_field(L"Name"))
		setName(json[L"Name"].as_string());
	if (json.has_string_field(L"UnitName"))
		setUnitName(json[L"UnitName"].as_string());
	if (json.has_string_field(L"GroupName"))
		setGroupName(json[L"GroupName"].as_string());
	if (json.has_object_field(L"Type"))
		setType(json[L"Type"]);
	if (json.has_number_field(L"Country"))
		setCountry(json[L"Country"].as_number().to_int32());
	if (json.has_number_field(L"CoalitionID"))
		setCoalitionID(json[L"CoalitionID"].as_number().to_int32());
	if (json.has_object_field(L"LatLongAlt"))
	{
		setLatitude(json[L"LatLongAlt"][L"Lat"].as_number().to_double());
		setLongitude(json[L"LatLongAlt"][L"Long"].as_number().to_double());
		setAltitude(json[L"LatLongAlt"][L"Alt"].as_number().to_double());
	}
	if (json.has_number_field(L"Heading"))
		setHeading(json[L"Heading"].as_number().to_double());
	if (json.has_object_field(L"Flags"))
		setFlags(json[L"Flags"]);

	/* All units which contain the name "Olympus" are automatically under AI control */
	/* TODO: I don't really like using this method */
	setAI(getUnitName().find(L"Olympus") != wstring::npos);

	/* If the unit is alive and it is not a human, run the AI Loop that performs the requested commands and instructions (moving, attacking, etc) */
	if (getAI() && getAlive() && getFlags()[L"Human"].as_bool() == false)
		AIloop();
}

void Unit::updateMissionData(json::value json)
{
	if (json.has_number_field(L"fuel"))
		setFuel(int(json[L"fuel"].as_number().to_double() * 100));
	if (json.has_object_field(L"ammo"))
		setAmmo(json[L"ammo"]);
	if (json.has_object_field(L"targets"))
		setTargets(json[L"targets"]);
	if (json.has_boolean_field(L"hasTask"))
		setHasTask(json[L"hasTask"].as_bool());
}

json::value Unit::getData(long long time)
{
	auto json = json::value::object();

	/********** Base data **********/
	json[L"baseData"] = json::value::object();
	for (auto key : { L"AI", L"name", L"unitName", L"groupName", L"alive", L"category"})
	{
		if (measures.find(key) != measures.end() && measures[key]->getTime() > time)
			json[L"baseData"][key] = measures[key]->getValue();
	}
	if (json[L"baseData"].size() == 0)
		json.erase(L"baseData");

	if (alive) {
		/********** Flight data **********/
		json[L"flightData"] = json::value::object();
		for (auto key : { L"latitude", L"longitude", L"altitude", L"speed", L"heading" })
		{
			if (measures.find(key) != measures.end() && measures[key]->getTime() > time)
				json[L"flightData"][key] = measures[key]->getValue();
		}
		if (json[L"flightData"].size() == 0)
			json.erase(L"flightData");

		/********** Mission data **********/
		json[L"missionData"] = json::value::object();
		for (auto key : { L"fuel", L"ammo", L"targets", L"hasTask", L"coalition", L"flags" })
		{
			if (measures.find(key) != measures.end() && measures[key]->getTime() > time)
				json[L"missionData"][key] = measures[key]->getValue();
		}
		if (json[L"missionData"].size() == 0)
			json.erase(L"missionData");

		/********** Formation data **********/
		json[L"formationData"] = json::value::object();
		for (auto key : { L"leaderID" })
		{
			if (measures.find(key) != measures.end() && measures[key]->getTime() > time)
				json[L"formationData"][key] = measures[key]->getValue();
		}
		if (json[L"formationData"].size() == 0)
			json.erase(L"formationData");

		/********** Task data **********/
		json[L"taskData"] = json::value::object();
		for (auto key : { L"currentState", L"currentTask", L"targetSpeed", L"targetAltitude", L"activePath", L"isTanker", L"isAWACS" })
		{
			if (measures.find(key) != measures.end() && measures[key]->getTime() > time)
				json[L"taskData"][key] = measures[key]->getValue();
		}
		if (json[L"taskData"].size() == 0)
			json.erase(L"taskData");

		/********** Options data **********/
		json[L"optionsData"] = json::value::object();
		for (auto key : { L"ROE", L"reactionToThreat", L"emissionsCountermeasures", L"TACAN", L"radio", L"generalSettings"})
		{
			if (measures.find(key) != measures.end() && measures[key]->getTime() > time)
				json[L"optionsData"][key] = measures[key]->getValue();
		}
		if (json[L"optionsData"].size() == 0)
			json.erase(L"optionsData");
	}

	return json;
}

void Unit::setActivePath(list<Coords> newPath)
{
	activePath = newPath;
	resetActiveDestination();
	
	auto path = json::value::object();
	if (activePath.size() > 0) {
		int count = 1;
		for (auto& destination : activePath)
		{
			auto json = json::value::object();
			json[L"lat"] = destination.lat;
			json[L"lng"] = destination.lng;
			json[L"alt"] = destination.alt;
			path[to_wstring(count++)] = json;
		}
	}
	addMeasure(L"activePath", path); 
}

void Unit::clearActivePath()
{
	list<Coords> newPath;
	setActivePath(newPath);
}

void Unit::pushActivePathFront(Coords newActivePathFront)
{
	list<Coords> path = activePath;
	path.push_front(newActivePathFront);
	setActivePath(path);
}

void Unit::pushActivePathBack(Coords newActivePathBack)
{
	list<Coords> path = activePath;
	path.push_back(newActivePathBack);
	setActivePath(path);
}

void Unit::popActivePathFront()
{
	list<Coords> path = activePath;
	path.pop_front();
	setActivePath(path);
}

void Unit::setCoalitionID(int newCoalitionID) 
{ 
	if (newCoalitionID == 0)
		coalition = L"neutral";
	else if (newCoalitionID == 1)
		coalition = L"red";
	else
		coalition = L"blue";
	addMeasure(L"coalition", json::value(coalition));
} 

int Unit::getCoalitionID()
{
	if (coalition == L"neutral")
		return 0;
	else if (coalition == L"red")
		return 1;
	else
		return 2;
}

wstring Unit::getTargetName()
{
	if (isTargetAlive())
	{
		Unit* target = unitsManager->getUnit(targetID);
		if (target != nullptr)
			return target->getUnitName();
	}
	return L"";
}

bool Unit::isTargetAlive()
{
	if (targetID == NULL)
		return false;

	Unit* target = unitsManager->getUnit(targetID);
	if (target != nullptr)
		return target->alive;
	else
		return false;
}

wstring Unit::getLeaderName()
{
	if (isLeaderAlive())
	{
		Unit* leader = unitsManager->getUnit(leaderID);
		if (leader != nullptr)
			return leader->getUnitName();
	}
	return L"";
}

bool Unit::isLeaderAlive()
{
	if (leaderID == NULL)
		return false;

	Unit* leader = unitsManager->getUnit(leaderID);
	if (leader != nullptr)
		return leader->alive;
	else
		return false;
}

void Unit::resetActiveDestination()
{
	activeDestination = Coords(NULL);
}

void Unit::resetTask()
{
	Command* command = dynamic_cast<Command*>(new ResetTask(ID));
	scheduler->appendCommand(command);
}

void Unit::setFormationOffset(Offset newFormationOffset)
{
	formationOffset = newFormationOffset;
	resetTask();
}

void Unit::setROE(wstring newROE) {
	addMeasure(L"ROE", json::value(newROE));
	
	if (ROE != newROE) {
		ROE = newROE;

		int ROEEnum;
		if (ROE.compare(L"Free") == 0)
			ROEEnum = ROE::WEAPON_FREE;
		else if (ROE.compare(L"Designated free") == 0)
			ROEEnum = ROE::OPEN_FIRE_WEAPON_FREE;
		else if (ROE.compare(L"Designated") == 0)
			ROEEnum = ROE::OPEN_FIRE;
		else if (ROE.compare(L"Return") == 0)
			ROEEnum = ROE::RETURN_FIRE;
		else if (ROE.compare(L"Hold") == 0)
			ROEEnum = ROE::WEAPON_HOLD;
		else
			return;

		Command* command = dynamic_cast<Command*>(new SetOption(ID, SetCommandType::ROE, ROEEnum));
		scheduler->appendCommand(command);
	}
}

void Unit::setReactionToThreat(wstring newReactionToThreat) {
	addMeasure(L"reactionToThreat", json::value(newReactionToThreat));

	if (reactionToThreat != newReactionToThreat) {
		reactionToThreat = newReactionToThreat;

		int reactionToThreatEnum;
		if (reactionToThreat.compare(L"None") == 0)
			reactionToThreatEnum = ReactionToThreat::NO_REACTION;
		else if (reactionToThreat.compare(L"Passive") == 0)
			reactionToThreatEnum = ReactionToThreat::PASSIVE_DEFENCE;
		else if (reactionToThreat.compare(L"Evade") == 0)
			reactionToThreatEnum = ReactionToThreat::EVADE_FIRE;
		else if (reactionToThreat.compare(L"Escape") == 0)
			reactionToThreatEnum = ReactionToThreat::BYPASS_AND_ESCAPE;
		else if (reactionToThreat.compare(L"Abort") == 0)
			reactionToThreatEnum = ReactionToThreat::ALLOW_ABORT_MISSION;
		else
			return;

		Command* command = dynamic_cast<Command*>(new SetOption(ID, SetCommandType::REACTION_ON_THREAT, reactionToThreatEnum));
		scheduler->appendCommand(command);
	}
}

void Unit::setEmissionsCountermeasures(wstring newEmissionsCountermeasures) {
	addMeasure(L"emissionsCountermeasures", json::value(newEmissionsCountermeasures)); 

	if (emissionsCountermeasures != newEmissionsCountermeasures) {
		emissionsCountermeasures = newEmissionsCountermeasures;

		int radarEnum;
		int flareEnum;
		int ECMEnum;
		if (emissionsCountermeasures.compare(L"Silent") == 0)
		{
			radarEnum = RadarUse::NEVER;
			flareEnum = FlareUse::NEVER;
			ECMEnum = ECMUse::NEVER_USE;
		}
		else if (emissionsCountermeasures.compare(L"Attack") == 0)
		{
			radarEnum = RadarUse::FOR_ATTACK_ONLY;
			flareEnum = FlareUse::AGAINST_FIRED_MISSILE;
			ECMEnum = ECMUse::USE_IF_ONLY_LOCK_BY_RADAR;
		}
		else if (emissionsCountermeasures.compare(L"Defend") == 0)
		{
			radarEnum = RadarUse::FOR_SEARCH_IF_REQUIRED;
			flareEnum = FlareUse::WHEN_FLYING_IN_SAM_WEZ;
			ECMEnum = ECMUse::USE_IF_DETECTED_LOCK_BY_RADAR;
		}
		else if (emissionsCountermeasures.compare(L"Free") == 0)
		{
			radarEnum = RadarUse::FOR_CONTINUOUS_SEARCH;
			flareEnum = FlareUse::WHEN_FLYING_NEAR_ENEMIES;
			ECMEnum = ECMUse::ALWAYS_USE;
		}
		else
			return;

		Command* command;

		command = dynamic_cast<Command*>(new SetOption(ID, SetCommandType::RADAR_USING, radarEnum));
		scheduler->appendCommand(command);

		command = dynamic_cast<Command*>(new SetOption(ID, SetCommandType::FLARE_USING, flareEnum));
		scheduler->appendCommand(command);

		command = dynamic_cast<Command*>(new SetOption(ID, SetCommandType::ECM_USING, ECMEnum));
		scheduler->appendCommand(command);
	}
}

void Unit::landAt(Coords loc) {
	clearActivePath();
	pushActivePathBack(loc);
	setState(State::LAND);
}

void Unit::setIsTanker(bool newIsTanker) { 
	isTanker = newIsTanker; 
	resetTask(); 
	addMeasure(L"isTanker", json::value(newIsTanker));
}

void Unit::setIsAWACS(bool newIsAWACS) { 
	isAWACS = newIsAWACS; 
	resetTask(); 
	addMeasure(L"isAWACS", json::value(newIsAWACS)); 
	setEPLRS(isAWACS);
}

void Unit::setTACAN(Options::TACAN newTACAN) {
	auto json = json::value();
	json[L"isOn"] = json::value(newTACAN.isOn);
	json[L"channel"] = json::value(newTACAN.channel);
	json[L"XY"] = json::value(newTACAN.XY);
	json[L"callsign"] = json::value(newTACAN.callsign);
	addMeasure(L"TACAN", json);

	if (TACAN != newTACAN)
	{
		TACAN = newTACAN;
		if (TACAN.isOn) {
			std::wostringstream commandSS;
			commandSS << "{"
				<< "id = 'ActivateBeacon',"
				<< "params = {"
				<< "type = " << ((TACAN.XY.compare(L"X") == 0) ? 4 : 5) << ","
				<< "system = 3,"
				<< "name = \"Olympus_TACAN\","
				<< "callsign = \"" << TACAN.callsign << "\", "
				<< "frequency = " << TACANChannelToFrequency(TACAN.channel, TACAN.XY) << ","
				<< "}"
				<< "}";
			Command* command = dynamic_cast<Command*>(new SetCommand(ID, commandSS.str()));
			scheduler->appendCommand(command);
		}
		else {
			std::wostringstream commandSS;
			commandSS << "{"
				<< "id = 'DeactivateBeacon',"
				<< "params = {"
				<< "}"
				<< "}";
			Command* command = dynamic_cast<Command*>(new SetCommand(ID, commandSS.str()));
			scheduler->appendCommand(command);
		}
	}
}

void Unit::setRadio(Options::Radio newRadio) {

	auto json = json::value();
	json[L"frequency"] = json::value(newRadio.frequency);
	json[L"callsign"] = json::value(newRadio.callsign);
	json[L"callsignNumber"] = json::value(newRadio.callsignNumber);
	addMeasure(L"radio", json);

	if (radio != newRadio)
	{
		radio = newRadio;

		std::wostringstream commandSS;
		Command* command;

		commandSS << "{"
			<< "id = 'SetFrequency',"
			<< "params = {"
			<< "modulation = 0,"	// TODO Allow selection
			<< "frequency = " << radio.frequency << ","
			<< "}"
			<< "}";
		command = dynamic_cast<Command*>(new SetCommand(ID, commandSS.str()));
		scheduler->appendCommand(command);

		// Clear the stringstream
		commandSS.str(wstring());

		commandSS << "{"
			<< "id = 'SetCallsign',"
			<< "params = {"
			<< "callname = " << radio.callsign << ","
			<< "number = " << radio.callsignNumber << ","
			<< "}"
			<< "}";
		command = dynamic_cast<Command*>(new SetCommand(ID, commandSS.str()));
		scheduler->appendCommand(command);
	}
}

void Unit::setEPLRS(bool newEPLRS)
{
	//addMeasure(L"EPLRS", json::value(newEPLRS)); 
	//
	//if (EPLRS != newEPLRS) {
	//	EPLRS = newEPLRS;
	//
	//	std::wostringstream commandSS;
	//	commandSS << "{"
	//		<< "id = 'EPLRS',"
	//		<< "params = {"
	//		<< "value = " << (EPLRS ? "true" : "false") << ", "
	//		<< "}"
	//		<< "}";
	//	Command* command = dynamic_cast<Command*>(new SetCommand(ID, commandSS.str()));
	//	scheduler->appendCommand(command);
	//}
}

void Unit::setGeneralSettings(Options::GeneralSettings newGeneralSettings) {

	auto json = json::value();
	json[L"prohibitJettison"] = json::value(newGeneralSettings.prohibitJettison);
	json[L"prohibitAA"] = json::value(newGeneralSettings.prohibitAA);
	json[L"prohibitAG"] = json::value(newGeneralSettings.prohibitAG);
	json[L"prohibitAfterburner"] = json::value(newGeneralSettings.prohibitAfterburner);
	json[L"prohibitAirWpn"] = json::value(newGeneralSettings.prohibitAirWpn);
	addMeasure(L"generalSettings", json);

	if (generalSettings != newGeneralSettings)
	{
		generalSettings = newGeneralSettings;

		Command* command;
		command = dynamic_cast<Command*>(new SetOption(ID, SetCommandType::PROHIBIT_AA, generalSettings.prohibitAA));
		scheduler->appendCommand(command);
		command = dynamic_cast<Command*>(new SetOption(ID, SetCommandType::PROHIBIT_AG, generalSettings.prohibitAG));
		scheduler->appendCommand(command);
		command = dynamic_cast<Command*>(new SetOption(ID, SetCommandType::PROHIBIT_JETT, generalSettings.prohibitJettison));
		scheduler->appendCommand(command);
		command = dynamic_cast<Command*>(new SetOption(ID, SetCommandType::PROHIBIT_AB, generalSettings.prohibitAfterburner));
		scheduler->appendCommand(command);
		command = dynamic_cast<Command*>(new SetOption(ID, SetCommandType::ENGAGE_AIR_WEAPONS, !generalSettings.prohibitAirWpn));
		scheduler->appendCommand(command);
	}
}

