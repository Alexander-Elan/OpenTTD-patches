/* $Id$ */

/** @file ai_event_types.hpp The detailed types of all events. */

#ifndef AI_EVENT_TYPES_HPP
#define AI_EVENT_TYPES_HPP

#include "ai_object.hpp"
#include "ai_event.hpp"
#include "ai_town.hpp"
#include "ai_industry.hpp"
#include "ai_engine.hpp"
#include "ai_subsidy.hpp"

/**
 * Event Test: a simple test event, to see if the event system is working.
 *  Triggered via AIEventController::Test();
 */
class AIEventTest : public AIEvent {
public:
	static const char *GetClassName() { return "AIEventTest"; }

	/**
	 * @param test A test value.
	 */
	AIEventTest(uint test) :
		AIEvent(AI_ET_TEST),
		test(test)
	{}

	/**
	 * Convert an AIEvent to the real instance.
	 * @param instance The instance to convert.
	 * @return The converted instance.
	 */
	static AIEventTest *Convert(AIEvent *instance) { return (AIEventTest *)instance; }

	/**
	 * Return the test value.
	 * @return The test value.
	 */
	uint GetTest() { return this->test; }

private:
	uint test;
};

/**
 * Event Vehicle Crash, indicating a vehicle of yours is crashed.
 *  It contains both the crash site as the vehicle crashed. It has a nice
 *  helper that creates a new vehicle in a depot with the same type
 *  and orders as the crashed one. In case the vehicle type isn't available
 *  anymore, it will find the next best.
 */
class AIEventVehicleCrashed : public AIEvent {
public:
	static const char *GetClassName() { return "AIEventVehicleCrashed"; }

	/**
	 * @param vehicle The vehicle that crashed.
	 * @param crash_site Where the vehicle crashed.
	 */
	AIEventVehicleCrashed(VehicleID vehicle, TileIndex crash_site) :
		AIEvent(AI_ET_VEHICLE_CRASHED),
		crash_site(crash_site),
		vehicle(vehicle)
	{}

	/**
	 * Convert an AIEvent to the real instance.
	 * @param instance The instance to convert.
	 * @return The converted instance.
	 */
	static AIEventVehicleCrashed *Convert(AIEvent *instance) { return (AIEventVehicleCrashed *)instance; }

	/**
	 * Get the VehicleID of the crashed vehicle.
	 * @return The crashed vehicle.
	 */
	VehicleID GetVehicleID() { return vehicle; }

	/**
	 * Find the tile the vehicle crashed.
	 * @return The crash site.
	 */
	TileIndex GetCrashSite() { return crash_site; }

	/**
	 * Clone the crashed vehicle and send it on its way again.
	 * @param depot the depot to build the vehicle in.
	 * @return True when the cloning succeeded.
	 * @note This function isn't implemented yet.
	 */
	bool CloneCrashedVehicle(TileIndex depot);

private:
	TileIndex crash_site;
	VehicleID vehicle;
};

/**
 * Event Subsidy Offered, indicating someone offered a subsidy.
 */
class AIEventSubsidyOffer : public AIEvent {
public:
	static const char *GetClassName() { return "AIEventSubsidyOffer"; }

	/**
	 * @param subsidy_id The index of this subsidy in the _subsidies array.
	 */
	AIEventSubsidyOffer(SubsidyID subsidy_id) :
		AIEvent(AI_ET_SUBSIDY_OFFER),
		subsidy_id(subsidy_id)
	{}

	/**
	 * Convert an AIEvent to the real instance.
	 * @param instance The instance to convert.
	 * @return The converted instance.
	 */
	static AIEventSubsidyOffer *Convert(AIEvent *instance) { return (AIEventSubsidyOffer *)instance; }

	/**
	 * Get the SubsidyID of the subsidy.
	 * @return The subsidy id.
	 */
	SubsidyID GetSubsidyID() { return subsidy_id; }

private:
	SubsidyID subsidy_id;
};

/**
 * Event Subsidy Offer Expired, indicating a subsidy will no longer be awarded.
 */
class AIEventSubsidyOfferExpired : public AIEvent {
public:
	static const char *GetClassName() { return "AIEventSubsidyOfferExpired"; }

	/**
	 * @param subsidy_id The index of this subsidy in the _subsidies array.
	 */
	AIEventSubsidyOfferExpired(SubsidyID subsidy_id) :
		AIEvent(AI_ET_SUBSIDY_OFFER_EXPIRED),
		subsidy_id(subsidy_id)
	{}

	/**
	 * Convert an AIEvent to the real instance.
	 * @param instance The instance to convert.
	 * @return The converted instance.
	 */
	static AIEventSubsidyOfferExpired *Convert(AIEvent *instance) { return (AIEventSubsidyOfferExpired *)instance; }

	/**
	 * Get the SubsidyID of the subsidy.
	 * @return The subsidy id.
	 */
	SubsidyID GetSubsidyID() { return subsidy_id; }

private:
	SubsidyID subsidy_id;
};

/**
 * Event Subidy Awarded, indicating a subsidy is awarded to some company.
 */
class AIEventSubsidyAwarded : public AIEvent {
public:
	static const char *GetClassName() { return "AIEventSubsidyAwarded"; }

	/**
	 * @param subsidy_id The index of this subsidy in the _subsidies array.
	 */
	AIEventSubsidyAwarded(SubsidyID subsidy_id) :
		AIEvent(AI_ET_SUBSIDY_AWARDED),
		subsidy_id(subsidy_id)
	{}

	/**
	 * Convert an AIEvent to the real instance.
	 * @param instance The instance to convert.
	 * @return The converted instance.
	 */
	static AIEventSubsidyAwarded *Convert(AIEvent *instance) { return (AIEventSubsidyAwarded *)instance; }

	/**
	 * Get the SubsidyID of the subsidy.
	 * @return The subsidy id.
	 */
	SubsidyID GetSubsidyID() { return subsidy_id; }

private:
	SubsidyID subsidy_id;
};

/**
 * Event Subsidy Expired, indicating a route that was once subsidized no longer is.
 */
class AIEventSubsidyExpired : public AIEvent {
public:
	static const char *GetClassName() { return "AIEventSubsidyExpired"; }

	/**
	 * @param subsidy_id The index of this subsidy in the _subsidies array.
	 */
	AIEventSubsidyExpired(SubsidyID subsidy_id) :
		AIEvent(AI_ET_SUBSIDY_EXPIRED),
		subsidy_id(subsidy_id)
	{}

	/**
	 * Convert an AIEvent to the real instance.
	 * @param instance The instance to convert.
	 * @return The converted instance.
	 */
	static AIEventSubsidyExpired *Convert(AIEvent *instance) { return (AIEventSubsidyExpired *)instance; }

	/**
	 * Get the SubsidyID of the subsidy.
	 * @return The subsidy id.
	 */
	 SubsidyID GetSubsidyID() { return subsidy_id; }

private:
	SubsidyID subsidy_id;
};

/**
 * Event Engine Preview, indicating a manufacturer offer you to test a new engine.
 *  You can get the same information about the offered engine as a real user
 *  would see in the offer window. And you can also accept the offer.
 */
class AIEventEnginePreview : public AIEvent {
public:
	static const char *GetClassName() { return "AIEventEnginePreview"; }

	/**
	 * @param engine The engine offered to test.
	 */
	AIEventEnginePreview(EngineID engine) :
		AIEvent(AI_ET_ENGINE_PREVIEW),
		engine(engine)
	{}

	/**
	 * Convert an AIEvent to the real instance.
	 * @param instance The instance to convert.
	 * @return The converted instance.
	 */
	static AIEventEnginePreview *Convert(AIEvent *instance) { return (AIEventEnginePreview *)instance; }

	/**
	 * Get the name of the offered engine.
	 * @return The name the engine has.
	 */
	const char *GetName();

	/**
	 * Get the cargo-type of the offered engine. In case it can transport 2 cargos, it
	 *  returns the first.
	 * @return The cargo-type of the engine.
	 */
	CargoID GetCargoType();

	/**
	 * Get the capacity of the offered engine. In case it can transport 2 cargos, it
	 *  returns the first.
	 * @return The capacity of the engine.
	 */
	int32 GetCapacity();

	/**
	 * Get the maximum speed of the offered engine.
	 * @return The maximum speed the engine has.
	 * @note The speed is in km/h.
	 */
	int32 GetMaxSpeed();

	/**
	 * Get the new cost of the offered engine.
	 * @return The new cost the engine has.
	 */
	Money GetPrice();

	/**
	 * Get the running cost of the offered engine.
	 * @return The running cost of the vehicle per year.
	 * @note Cost is per year; divide by 365 to get per day.
	 */
	Money GetRunningCost();

	/**
	 * Get the type of the offered engine.
	 * @return The type the engine has.
	 */
	AIVehicle::VehicleType GetVehicleType();

	/**
	 * Accept the engine preview.
	 * @return True when the accepting succeeded.
	 */
	bool AcceptPreview();

private:
	EngineID engine;
};

/**
 * Event Company New, indicating a new company has been created.
 */
class AIEventCompanyNew : public AIEvent {
public:
	static const char *GetClassName() { return "AIEventCompanyNew"; }

	/**
	 * @param owner The new company.
	 */
	AIEventCompanyNew(Owner owner) :
		AIEvent(AI_ET_COMPANY_NEW),
		owner((AICompany::CompanyID)(byte)owner)
	{}

	/**
	 * Convert an AIEvent to the real instance.
	 * @param instance The instance to convert.
	 * @return The converted instance.
	 */
	static AIEventCompanyNew *Convert(AIEvent *instance) { return (AIEventCompanyNew *)instance; }

	/**
	 * Get the CompanyID of the company that has been created.
	 * @return The CompanyID of the company.
	 */
	AICompany::CompanyID GetCompanyID() { return owner; }

private:
	AICompany::CompanyID owner;
};

/**
 * Event Company In Trouble, indicating a company is in trouble and might go
 *  bankrupt soon.
 */
class AIEventCompanyInTrouble : public AIEvent {
public:
	static const char *GetClassName() { return "AIEventCompanyInTrouble"; }

	/**
	 * @param owner The company that is in trouble.
	 */
	AIEventCompanyInTrouble(Owner owner) :
		AIEvent(AI_ET_COMPANY_IN_TROUBLE),
		owner((AICompany::CompanyID)(byte)owner)
	{}

	/**
	 * Convert an AIEvent to the real instance.
	 * @param instance The instance to convert.
	 * @return The converted instance.
	 */
	static AIEventCompanyInTrouble *Convert(AIEvent *instance) { return (AIEventCompanyInTrouble *)instance; }

	/**
	 * Get the CompanyID of the company that is in trouble.
	 * @return The CompanyID of the company in trouble.
	 */
	AICompany::CompanyID GetCompanyID() { return owner; }

private:
	AICompany::CompanyID owner;
};

/**
 * Event Company Merger, indicating a company has been bought by another
 *  company.
 */
class AIEventCompanyMerger : public AIEvent {
public:
	static const char *GetClassName() { return "AIEventCompanyMerger"; }

	/**
	 * @param old_owner The company bought off.
	 * @param new_owner The company that bougth owner.
	 */
	AIEventCompanyMerger(Owner old_owner, Owner new_owner) :
		AIEvent(AI_ET_COMPANY_MERGER),
		old_owner((AICompany::CompanyID)(byte)old_owner),
		new_owner((AICompany::CompanyID)(byte)new_owner)
	{}

	/**
	 * Convert an AIEvent to the real instance.
	 * @param instance The instance to convert.
	 * @return The converted instance.
	 */
	static AIEventCompanyMerger *Convert(AIEvent *instance) { return (AIEventCompanyMerger *)instance; }

	/**
	 * Get the CompanyID of the company that has been bought.
	 * @return The CompanyID of the company that has been bought.
	 * @note: The value below is not valid anymore as CompanyID, and
	 *  AICompany::ResolveCompanyID will return COMPANY_COMPANY. It's
	 *  only usefull if you're keeping track of company's yourself.
	 */
	AICompany::CompanyID GetOldCompanyID() { return old_owner; }

	/**
	 * Get the CompanyID of the new owner.
	 * @return The CompanyID of the new owner.
	 */
	AICompany::CompanyID GetNewCompanyID() { return new_owner; }

private:
	AICompany::CompanyID old_owner;
	AICompany::CompanyID new_owner;
};

/**
 * Event Company Bankrupt, indicating a company has gone bankrupt.
 */
class AIEventCompanyBankrupt : public AIEvent {
public:
	static const char *GetClassName() { return "AIEventCompanyBankrupt"; }

	/**
	 * @param owner The company that has gone bankrupt.
	 */
	AIEventCompanyBankrupt(Owner owner) :
		AIEvent(AI_ET_COMPANY_BANKRUPT),
		owner((AICompany::CompanyID)(byte)owner)
	{}

	/**
	 * Convert an AIEvent to the real instance.
	 * @param instance The instance to convert.
	 * @return The converted instance.
	 */
	static AIEventCompanyBankrupt *Convert(AIEvent *instance) { return (AIEventCompanyBankrupt *)instance; }

	/**
	 * Get the CompanyID of the company that has gone bankrupt.
	 * @return The CompanyID of the company that has gone bankrupt.
	 */
	AICompany::CompanyID GetCompanyID() { return owner; }

private:
	AICompany::CompanyID owner;
};

/**
 * Event Vehicle Lost, indicating a vehicle can't find its way to its destination.
 */
class AIEventVehicleLost : public AIEvent {
public:
	static const char *GetClassName() { return "AIEventVehicleLost"; }

	/**
	 * @param vehicle_id The vehicle that is lost.
	 */
	AIEventVehicleLost(VehicleID vehicle_id) :
		AIEvent(AI_ET_VEHICLE_LOST),
		vehicle_id(vehicle_id)
	{}

	/**
	 * Convert an AIEvent to the real instance.
	 * @param instance The instance to convert.
	 * @return The converted instance.
	 */
	static AIEventVehicleLost *Convert(AIEvent *instance) { return (AIEventVehicleLost *)instance; }

	/**
	 * Get the VehicleID of the vehicle that is lost.
	 * @return The VehicleID of the vehicle that is lost.
	 */
	VehicleID GetVehicleID() { return vehicle_id; }

private:
	VehicleID vehicle_id;
};

/**
 * Event VehicleWaitingInDepot, indicating a vehicle has arrived a depot and is now waiting there.
 */
class AIEventVehicleWaitingInDepot : public AIEvent {
public:
	static const char *GetClassName() { return "AIEventVehicleWaitingInDepot"; }

	/**
	 * @param vehicle_id The vehicle that is waiting in a depot.
	 */
	AIEventVehicleWaitingInDepot(VehicleID vehicle_id) :
		AIEvent(AI_ET_VEHICLE_WAITING_IN_DEPOT),
		vehicle_id(vehicle_id)
	{}

	/**
	 * Convert an AIEvent to the real instance.
	 * @param instance The instance to convert.
	 * @return The converted instance.
	 */
	static AIEventVehicleWaitingInDepot *Convert(AIEvent *instance) { return (AIEventVehicleWaitingInDepot *)instance; }

	/**
	 * Get the VehicleID of the vehicle that is waiting in a depot.
	 * @return The VehicleID of the vehicle that is waiting in a depot.
	 */
	VehicleID GetVehicleID() { return vehicle_id; }

private:
	VehicleID vehicle_id;
};

/**
 * Event Vehicle Unprofitable, indicating a vehicle lost money last year.
 */
class AIEventVehicleUnprofitable : public AIEvent {
public:
	static const char *GetClassName() { return "AIEventVehicleUnprofitable"; }

	/**
	 * @param vehicle_id The vehicle that was unprofitable.
	 */
	AIEventVehicleUnprofitable(VehicleID vehicle_id) :
		AIEvent(AI_ET_VEHICLE_UNPROFITABLE),
		vehicle_id(vehicle_id)
	{}

	/**
	 * Convert an AIEvent to the real instance.
	 * @param instance The instance to convert.
	 * @return The converted instance.
	 */
	static AIEventVehicleUnprofitable *Convert(AIEvent *instance) { return (AIEventVehicleUnprofitable *)instance; }

	/**
	 * Get the VehicleID of the vehicle that lost money.
	 * @return The VehicleID of the vehicle that lost money.
	 */
	VehicleID GetVehicleID() { return vehicle_id; }

private:
	VehicleID vehicle_id;
};

/**
 * Event Industry Open, indicating a new industry has been created.
 */
class AIEventIndustryOpen : public AIEvent {
public:
	static const char *GetClassName() { return "AIEventIndustryOpen"; }

	/**
	 * @param industry_id The new industry.
	 */
	AIEventIndustryOpen(IndustryID industry_id) :
		AIEvent(AI_ET_INDUSTRY_OPEN),
		industry_id(industry_id)
	{}

	/**
	 * Convert an AIEvent to the real instance.
	 * @param instance The instance to convert.
	 * @return The converted instance.
	 */
	static AIEventIndustryOpen *Convert(AIEvent *instance) { return (AIEventIndustryOpen *)instance; }

	/**
	 * Get the IndustryID of the new industry.
	 * @return The IndustryID of the industry.
	 */
	IndustryID GetIndustryID() { return industry_id; }

private:
	IndustryID industry_id;
};

/**
 * Event Industry Close, indicating an industry is going to be closed.
 */
class AIEventIndustryClose : public AIEvent {
public:
	static const char *GetClassName() { return "AIEventIndustryClose"; }

	/**
	 * @param industry_id The new industry.
	 */
	AIEventIndustryClose(IndustryID industry_id) :
		AIEvent(AI_ET_INDUSTRY_CLOSE),
		industry_id(industry_id)
	{}

	/**
	 * Convert an AIEvent to the real instance.
	 * @param instance The instance to convert.
	 * @return The converted instance.
	 */
	static AIEventIndustryClose *Convert(AIEvent *instance) { return (AIEventIndustryClose *)instance; }

	/**
	 * Get the IndustryID of the closing industry.
	 * @return The IndustryID of the industry.
	 */
	IndustryID GetIndustryID() { return industry_id; }

private:
	IndustryID industry_id;
};

/**
 * Event Engine Available, indicating a new engine is available.
 */
class AIEventEngineAvailable : public AIEvent {
public:
	static const char *GetClassName() { return "AIEventEngineAvailable"; }

	/**
	 * @param engine The engine that is available.
	 */
	AIEventEngineAvailable(EngineID engine) :
		AIEvent(AI_ET_ENGINE_AVAILABLE),
		engine(engine)
	{}

	/**
	 * Convert an AIEvent to the real instance.
	 * @param instance The instance to convert.
	 * @return The converted instance.
	 */
	static AIEventEngineAvailable *Convert(AIEvent *instance) { return (AIEventEngineAvailable *)instance; }

	/**
	 * Get the EngineID of the new engine.
	 * @return The EngineID of the new engine.
	 */
	EngineID GetEngineID() { return engine; }

private:
	EngineID engine;
};

/**
 * Event Station First Vehicle, indicating a station has been visited by a vehicle for the first time.
 */
class AIEventStationFirstVehicle : public AIEvent {
public:
	static const char *GetClassName() { return "AIEventStationFirstVehicle"; }

	/**
	 * @param station The station visited for the first time.
	 * @param vehicle The vehicle visiting the station.
	 */
	AIEventStationFirstVehicle(StationID station, VehicleID vehicle) :
		AIEvent(AI_ET_STATION_FIRST_VEHICLE),
		station(station),
		vehicle(vehicle)
	{}

	/**
	 * Convert an AIEvent to the real instance.
	 * @param instance The instance to convert.
	 * @return The converted instance.
	 */
	static AIEventStationFirstVehicle *Convert(AIEvent *instance) { return (AIEventStationFirstVehicle *)instance; }

	/**
	 * Get the StationID of the visited station.
	 * @return The StationID of the visited station.
	 */
	StationID GetStationID() { return station; }

	/**
	 * Get the VehicleID of the first vehicle.
	 * @return The VehicleID of the first vehicle.
	 */
	VehicleID GetVehicleID() { return vehicle; }

private:
	StationID station;
	VehicleID vehicle;
};

#endif /* AI_EVENT_TYPES_HPP */
