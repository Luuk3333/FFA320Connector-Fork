/*
* FFA320-Connector by mokny
*
* This is a plugin for the FlightFactor A320 Ultimate
* It allows you to map any internal A320 variable to a command or Dataref.
*
*/

#pragma warning(disable: 4996)

#include "XPLMDataAccess.h"
#include "XPLMPlugin.h"
#include "XPLMUtilities.h"
#include "XPLMProcessing.h"
#include "XPLMMenus.h"
#include "XPLMPlanes.h"
#include "XPLMDataAccess.h"
#include <stdio.h>
#include <algorithm>
#include <string.h>
#include <string>
#include <math.h>
#include <list>
#include <string>
#include <iostream>
#include <fstream>
#include "SharedValue.h"
#include <regex> 
#include "dirent.h"

using namespace std;

string					pluginversion = "1.1.7";																			// Plugin-Version

string					pluginpath;
string					aircraftpath;
string					defaultconfigpath;
string					customconfigpath;

#define					XPLM200 = 1;																						// SDK 2 Version
#define					MSG_ADD_DATAREF 0x01000000																			// Add dataref to DRE message

const int				OBJECT_TYPE_COMMAND = 1;																			// Command Object
const int				OBJECT_TYPE_DATAREF = 2;																			// Dataref Object
const int				OBJECT_TYPE_COMMANDTODATAREF = 3;																	// Command to Dataref Object
const int				VALUE_TYPE_INT = 1;																					// Integer Value														
const int				VALUE_TYPE_FLOAT = 2;																				// Float Value
const int				WORK_MODE_SET = 1;																					// Workmode Definitions
const int				WORK_MODE_STEP = 2;
const int				WORK_MODE_CYCLE = 3;
const int				WORK_MODE_CLICK = 4;
const int				WORK_MODE_ROTATE = 5;
const int				WORK_MODE_DOWN = 6;
const int				WORK_MODE_UP = 7;
const int				CONDITION_NONE = 0;
const int				CONDITION_EQUALS = 9;
const int				CONDITION_GREATER = 10;
const int				CONDITION_LOWER = 11;
const int				CONDITION_NOTEQUAL = 12;
const int				CONDITION_GREATER_EQUAL = 13;
const int				CONDITION_LOWER_EQUAL = 14;

bool					plugindisabled = false;																				// True if plugin is disabled
bool					plugininitialized = false;																			// Plugin Initialized? Set when Flightloop was called.

vector<string> filenames;

XPLMPluginID			ffPluginID = XPLM_NO_PLUGIN_ID;
SharedValuesInterface	ffAPI;
int						g_menu_container_idx;																				// Menu Stuff
XPLMMenuID				g_menu_id;																							// The menu container we'll append all our menu items to
void					menu_handler(void *, void *);
int						ffAPIdataversion = 0;
bool					debugmode = false;																					// Enable extensive logging?
void*					tag;																			// ffAPI Tag (shall we change this?!)
double					last_step;
float					PluginCustomFlightLoopCallback(float elapsedMe, float elapsedSim, int counter, void * refcon);		// FlightLoop, only called once for init
int						UniversalCommandHandler(XPLMCommandRef inCommand, XPLMCommandPhase inPhase, void * inRefcon);		// Handles all Commands
int						UniversalDataRefGET_INT(void* inRefcon);															// Handles all DataRef GET-Requests for Integer-Values
void					UniversalDataRefSET_INT(void* inRefcon, int inValue);												// Handles all DataRef SET-Requests for Integer-Values
float					UniversalDataRefGET_FLOAT(void* inRefcon);															// Handles all DataRef GET-Requests for Integer-Values
void					UniversalDataRefSET_FLOAT(void* inRefcon, float inValue);											// Handles all DataRef SET-Requests for Integer-Values
int						DrefValueInt[2000];																					// Stores the Dataref-Values (inRefcon points to here)
float					DrefValueFloat[2000];																				// Stores the Dataref-Values (inRefcon points to here)
bool					InternalDatarefUpdate = false;																		// For recognition if it was an internal Dref update or not

bool					DumpObjectsToLogActive = false;
void					DumpObjectsToLog();																					//Constructor
void					DumpCommandsToLog();																				//Constructor
void					DumpDatarefsToLog();																				//Constructor

string					trim(const string& str);																			// Trims string

/*
* StringToObjectType
*
* Converts a string to the defined Object-Type
*
*/
int StringToObjectType(string s) {
	transform(s.begin(), s.end(), s.begin(), ::toupper);
	if (s == "COMMAND") return OBJECT_TYPE_COMMAND;
	if (s == "DATAREF") return OBJECT_TYPE_DATAREF;
	if (s == "COMDEF") return OBJECT_TYPE_COMMANDTODATAREF;
	return 0;
}

/*
* StringToValueType
*
* Converts a string to the defined Value-Type
*
*/
int StringToValueType(string s) {
	transform(s.begin(), s.end(), s.begin(), ::toupper);
	if (s == "INT") return VALUE_TYPE_INT;
	if (s == "FLOAT") return VALUE_TYPE_FLOAT;
	return 0;
}

/*
* StringToWorkMode
*
* Converts a string to the defined Work-Mode
*
*/
int StringToWorkMode(string s) {
	transform(s.begin(), s.end(), s.begin(), ::toupper);
	if (s == "SET") return WORK_MODE_SET;
	if (s == "STEP") return WORK_MODE_STEP;
	if (s == "CYCLE") return WORK_MODE_CYCLE;
	if (s == "CLICK") return WORK_MODE_CLICK;
	if (s == "ROTATE") return WORK_MODE_ROTATE;
	if (s == "DOWN") return WORK_MODE_DOWN;
	if (s == "UP") return WORK_MODE_UP;
	return 0;
}

/*
* StringToCondition
*
* Converts a string to the defined Condition
*
*/
int StringToCondition(string s) {
	transform(s.begin(), s.end(), s.begin(), ::toupper);
	if (s == "=") return CONDITION_EQUALS;
	if (s == ">") return CONDITION_GREATER;
	if (s == ">=") return CONDITION_GREATER_EQUAL;
	if (s == "<=") return CONDITION_LOWER_EQUAL;
	if (s == "<") return CONDITION_LOWER;
	if (s == "!=") return CONDITION_NOTEQUAL;
	return CONDITION_NONE;
}


/*
* DebugOut
*
* Writes lines to the Log.txt if debugging was enabled
*
*/
void DebugOut(string text) {
	if (debugmode == true) {
		string line = "FFA320Connector DEBUG: " + text + "\n";
		XPLMDebugString(line.c_str());
	}
}

/*
* LogWrite
*
* Writes lines to the Log.txt - no matter if debugging is on or off
*
*/
void LogWrite(string text) {
	string line = "FFA320Connector: " + text + "\n";
	XPLMDebugString(line.c_str());
}

/*
* file_exists
*
* Checks if a file exists
*
*/
inline bool file_exists(const std::string& name) {
	ifstream f(name.c_str());
	return f.good();
}

/*
* get_paths
*
* Set the Path-Variables
*
*/
void get_paths() {
	LogWrite("Fetching Paths");

	defaultconfigpath = "";
	customconfigpath = "";
	aircraftpath = "";

	/* Getting the Aircraft Directory */
	char FileNamePath[512];

	char cacfilename[256] = { 0 };
	char cacpath[1024] = { 0 };

	string configFileDirectory;

	// Get config file directory
	XPLMGetNthAircraftModel(0, cacfilename, cacpath);
	XPLMExtractFileAndPath(cacpath);
	strcpy(FileNamePath, cacpath);
	strcat(FileNamePath, XPLMGetDirectorySeparator());
	strcat(FileNamePath, "plugins");
	strcat(FileNamePath, XPLMGetDirectorySeparator());
	strcat(FileNamePath, "FFA320Connector");
	configFileDirectory = string(FileNamePath);

	// Search .cfg config files
	LogWrite("-> Searching for config files in the directory '" + configFileDirectory + "'.");

	DIR *dir;
	struct dirent *ent;
	if ((dir = opendir(FileNamePath)) != NULL) {
		/* Get all files and directories */
		regex re_config_file("\\.cfg$");
		smatch matches;

		while ((ent = readdir(dir)) != NULL) {
			// Match all .cfg files
			if (regex_search(string(ent->d_name), matches, re_config_file)) {
				// Config file found
				filenames.push_back(configFileDirectory + XPLMGetDirectorySeparator() + string(ent->d_name));
			}
		}

		closedir(dir);
	}
	else {
		/* Could not open directory */
		LogWrite("#####################################################");
		LogWrite("# Could not open directory '" + configFileDirectory + "' to search for .cfg config files.");
		LogWrite("#####################################################");
	}

	switch (filenames.size()) {
	case 0:
		// Check if there is at least one .cfg file
		LogWrite("#####################################################");
		LogWrite("# MISSING CONFIG FILES!                             #");
		LogWrite("# No .cfg config files have been found in the directory '" + configFileDirectory + "'.");
		LogWrite("#####################################################");
		break;
	case 1:
		LogWrite("Found 1 config file.");
		break;
	default:
		LogWrite("Found " + to_string(filenames.size()) + " config files.");
		break;
	}
}

/*
* DataObject
*
* Main Data-Object for Commands and Datarefs
*
*/
class DataObject {

	public:
		string		FFVar;					// FlightFactor - Object
		bool		SyntaxError;			// SyntaxError
		int			FFID;					// Object ID
		int			Type;					// COMMAND or DATAREF
		int			WorkMode;				// SET/CYCLE/STEP/CLICK
		int			ValueType;				// INT
		string		Command;				// Command-Identifier
		string		CommandName;			// Command Name
		string		FFReference;			// FlightFactor Reference Object of increment/decrement
		int			FFReferenceID;			// FlightFactor Reference ID
		string		DataRef;				// Dataref
		int			DataRefValueType = 0;	// INT,FLOAT
		bool		IsExistingDataRef;		// Is this a custom or foreign Dataref?
		bool		IgnoreExistingDataRef;	// Ignore existing DataRef and create Handlers anyway
		int			Value;					// Value or Change-Value for COMMAND
		float		ValueFloat;				// Float Value for COMMAND
		int			MinValue;				// MinValue for CYCLE, STEP -- or Reset-Value for CLICK
		int			MaxValue;				// MaxValue for CYCLE, STEP 
		float		MinValueFloat;			// MinValueFloat for CYCLE, STEP -- or Reset-Value for CLICK
		float		MaxValueFloat;			// MaxValueFloat for CYCLE, STEP 
		bool		Cycle;					// Cycle?
		bool		NeedsUpdate;			// Tells the Flightloop if this object needs to be updated
		bool		NeedsClickUpdate;		// After a Click, this must be true
		int			ClickTimer;				// How many cycles before resetting to MinValue?
		int			SpeedRef;				// How fast increment/Decrement STEP/CYCLE
		int			NextUpdateCycle;		// Internal
		int			Phase;					// Button-Phase
		int			RefConID;				// RefconID - The link between the DREF and UniversalGET
		float		DataRefMultiplier;		// Dataref Multiplier
		int			DataRefOffset = -1;

		int			VarArrValues = 0;			
		int			VarArrI[100];
		float		VarArrF[100];

		int			DatarefCondition = 0;
		float		DatarefConditionValue = 0;
		
		int*		pAdress;				// Pointer to the Integer Refcon
		float*		pAdressf;				// Pointer to the Float Refcon

		XPLMCommandRef	CMD = NULL;			// The command 
		XPLMDataRef		DREF = NULL;		// The dataref

		void initialize() {
			/* Create the command */
			if (Type == OBJECT_TYPE_COMMAND) {
				DebugOut("Creating Command " + Command + " / " + to_string(WorkMode) + " / " + FFVar);
				CMD = XPLMCreateCommand(Command.c_str(), CommandName.c_str());
				XPLMRegisterCommandHandler(CMD, UniversalCommandHandler, 1, &Value);
				NextUpdateCycle = 0;
			}

			/* Create the command to dataref */
			if (Type == OBJECT_TYPE_COMMANDTODATAREF) {
				DebugOut("Creating Comdef " + Command + " / " + to_string(WorkMode) + " / " + FFVar);
				CMD = XPLMCreateCommand(Command.c_str(), CommandName.c_str());
				DREF = XPLMFindDataRef(DataRef.c_str());
				XPLMRegisterCommandHandler(CMD, UniversalCommandHandler, 1, &Value);
				NextUpdateCycle = 0;
			}

			/* Create the dataref */
			if (Type == OBJECT_TYPE_DATAREF) {

				if (DataRefValueType < 1) DataRefValueType = ValueType;				// If no ValueType is set in the config, use the default one

				DREF = XPLMFindDataRef(DataRef.c_str());							// Check if the Dataref already exists

				if ((DREF == NULL) || (IgnoreExistingDataRef == true)) {
					IsExistingDataRef = false;
					DebugOut("Creating Dataref " + DataRef + " / " + to_string(ValueType) + " / #" + to_string(RefConID) + " / " + FFVar);
					if (DataRefValueType == VALUE_TYPE_INT) {
						DREF = XPLMRegisterDataAccessor(DataRef.c_str(),
							xplmType_Int,											// The types we support
							1,														// Writable
							UniversalDataRefGET_INT, UniversalDataRefSET_INT,		// Integer accessors
							NULL, NULL,												// Float accessors
							NULL, NULL,												// Doubles accessors
							NULL, NULL,												// Int array accessors
							NULL, NULL,												// Float array accessors
							NULL, NULL,												// Raw data accessors
							&DrefValueInt[RefConID], &DrefValueInt[RefConID]);      // Refcons				
						pAdress = &DrefValueInt[RefConID];
					}
					if (DataRefValueType == VALUE_TYPE_FLOAT) {
						DREF = XPLMRegisterDataAccessor(DataRef.c_str(),
							xplmType_Float,											// The types we support
							1,														// Writable
							NULL, NULL,												// Integer accessors
							UniversalDataRefGET_FLOAT, UniversalDataRefSET_FLOAT,   // Float accessors
							NULL, NULL,												// Doubles accessors
							NULL, NULL,												// Int array accessors
							NULL, NULL,												// Float array accessors
							NULL, NULL,												// Raw data accessors
							&DrefValueFloat[RefConID], &DrefValueFloat[RefConID]);  // Refcons 				
						pAdressf = &DrefValueFloat[RefConID];
					}

					/*Report the Dataref to the Datarefeditor (This will only worke at this point, if the plugin resides in the
					  Aircraft's Plugin Directory. Otherwise the DRE may not be loaded here. That's why we do it twice... */

					XPLMPluginID PluginID = XPLMFindPluginBySignature("xplanesdk.examples.DataRefEditor");
					if ((PluginID != XPLM_NO_PLUGIN_ID)) XPLMSendMessageToPlugin(PluginID, MSG_ADD_DATAREF, (void*)DataRef.c_str());

				} else {
					IsExistingDataRef = true;
					DebugOut("Using existing Dataref (READONLY) " + DataRef + " / " + to_string(ValueType) + " / #" + to_string(RefConID) + " / " + FFVar);
				}

				NextUpdateCycle = 0;
			}
		}
		
		void destroy() {
			/* Remove the CommandHandler + Dataref */
			if (CMD != NULL) XPLMUnregisterCommandHandler(CMD, UniversalCommandHandler, 0, 0);
			if (DREF != NULL) XPLMUnregisterDataAccessor(DREF);
		}

};



list<DataObject>	DataObjects;													// This list contains all Data-Objects
void				ReadConfigs();
void				ReadConfig(string filename);
void				ffAPIUpdateCallback(double step, void *tag);					// FFAPI Constructor




/*
* XPluginStart
*
* Our start routine registers our window and does any other initialization we
* must do.
*
*/
PLUGIN_API int XPluginStart(
	char *		outName,
	char *		outSig,
	char *		outDesc)
{
	strcpy(outName, "FFA320-Connector");
	strcpy(outSig, "mokny.a320connector");
	strcpy(outDesc, "Plugin to supply Commands and Datarefs for the FlightFactor A320");
	
	XPLMEnableFeature("XPLM_USE_NATIVE_PATHS", 1); // MacOS requires this 

	string menu_title = string("FFA320-Connector " + pluginversion);

	/* Menu Stuff */
	g_menu_container_idx = XPLMAppendMenuItem(XPLMFindPluginsMenu(), menu_title.c_str(), 0, 0);
	g_menu_id = XPLMCreateMenu(menu_title.c_str(), XPLMFindPluginsMenu(), g_menu_container_idx, menu_handler, NULL);
	XPLMAppendMenuItem(g_menu_id, "Reload Config", (void *)"Reload Config", 1);
	XPLMAppendMenuSeparator(g_menu_id);
	XPLMAppendMenuItem(g_menu_id, "Debug logging", (void *)"Debug logging", 1);
	XPLMAppendMenuItem(g_menu_id, "Dump A320-Objects to Log.txt", (void *)"Dump A320-Objects to Log.txt", 1);
	XPLMAppendMenuItem(g_menu_id, "Dump Commands to Log.txt", (void *)"Dump Commands to Log.txt", 1);
	XPLMAppendMenuItem(g_menu_id, "Dump Datarefs to Log.txt", (void *)"Dump Datarefs to Log.txt", 1);

	/* Initial Load */
	LogWrite("==== FFA320 Connector loaded - Version " + pluginversion + " by mokny ====");

	/* Register the Flightloop */
	XPLMRegisterFlightLoopCallback(PluginCustomFlightLoopCallback, 1, NULL); 

	/* Read the Config */
	ReadConfigs();

	/* We are not disabled, right? */
	plugindisabled = false;

	return 1;
}

/*
* menu_handler
*
* Handles our Menu-Clicks
*
*/
void menu_handler(void * in_menu_ref, void * in_item_ref)
{
	/* Leave if plugin was disabled */
	if (plugindisabled == true) return;

	if (!strcmp((const char *)in_item_ref, "Reload Config"))
	{
		LogWrite("==== FFA320 Connector / Reloaded Config ====");
		ReadConfigs();
	}
	if (!strcmp((const char *)in_item_ref, "Debug logging"))
	{
		if (debugmode) {
			LogWrite("==== FFA320 Connector / Debug logging DISABLED ====");
			debugmode = false;
			XPLMCheckMenuItem(g_menu_id, 2 /* third menu item */, xplm_Menu_Unchecked);	// Uncheck menu item
		}
		else {
			LogWrite("==== FFA320 Connector / Debug logging ENABLED ====");
			debugmode = true;
			XPLMCheckMenuItem(g_menu_id, 2 /* third menu item */, xplm_Menu_Checked);	// Check menu item
		}
	}
	if (!strcmp((const char *)in_item_ref, "Dump A320-Objects to Log.txt"))
	{
		DumpObjectsToLogActive = true;
	}
	if (!strcmp((const char *)in_item_ref, "Dump Commands to Log.txt"))
	{
		DumpCommandsToLog();
	}
	if (!strcmp((const char *)in_item_ref, "Dump Datarefs to Log.txt"))
	{
		DumpDatarefsToLog();
	}
}

/*
* UniversalCommandHandler
*
* Handles all incoming Commands and updates the respective DataObject
*
*/
int UniversalCommandHandler(XPLMCommandRef inCommand, XPLMCommandPhase inPhase, void * inRefcon)
{
	if (!plugininitialized) return 0;

	list<DataObject>::iterator  iDataObjects;

	for (iDataObjects = DataObjects.begin(); iDataObjects != DataObjects.end(); ++iDataObjects) {
		if (inCommand == iDataObjects->CMD) {
			if (inPhase == iDataObjects->Phase)
			{
				iDataObjects->NextUpdateCycle = iDataObjects->NextUpdateCycle - 1;
				if ((iDataObjects->SpeedRef == 0) || (iDataObjects->NextUpdateCycle <= 0)) {
					iDataObjects->NextUpdateCycle = iDataObjects->SpeedRef;
					iDataObjects->NeedsUpdate = true;
				}
				
			}
		}
	}

	return 0;
}

/*********************************************************
	INTEGER DATAREFS
*********************************************************/

/* Universal Dataref GET Handler */
int UniversalDataRefGET_INT(void* inRefcon)
{
	int * my_var = (int *)inRefcon;
	return *my_var;
}

/* Universal Dataref SET Handler */
void UniversalDataRefSET_INT(void* inRefcon, int inValue)
{
	if (!plugininitialized) return;

	int * my_var = (int *)inRefcon;

	if (InternalDatarefUpdate == false) {
		DebugOut("========== DATAREF SET RECEIVED - Searching for the internal A320 Object");
		/*Here the A320 Internal Var gets updated in case that it was not an internal
		  Dataref-Update */
		list<DataObject>::iterator  iDataObjects;

		for (iDataObjects = DataObjects.begin(); iDataObjects != DataObjects.end(); ++iDataObjects) {
			if (iDataObjects->pAdress == inRefcon) {
				
				DebugOut("========== Found " + iDataObjects->DataRef);

				if (iDataObjects->FFID <= 0) {
					iDataObjects->FFID = ffAPI.ValueIdByName(iDataObjects->FFVar.c_str());
				}

				ffAPI.ValueSet(iDataObjects->FFID, &inValue);

				break;
			}
		}
		DebugOut("========== DATAREF SET Done.");

	}


	*my_var = inValue;
}

/*********************************************************
	FLOAT DATAREFS
*********************************************************/

/* Universal Dataref GET Handler */
float UniversalDataRefGET_FLOAT(void* inRefcon)
{
	float * my_var = (float *)inRefcon;
	return *my_var;
}

/* Universal Dataref SET Handler */
void UniversalDataRefSET_FLOAT(void* inRefcon, float inValue)
{
	if (!plugininitialized) return;

	float * my_var = (float *)inRefcon;

	if (InternalDatarefUpdate == false) {
		DebugOut("========== DATAREF SET RECEIVED - Searching for the internal A320 Object");
		/*Here the A320 Internal Var gets updated in case that it was not an internal
		Dataref-Update */
		list<DataObject>::iterator  iDataObjects;

		for (iDataObjects = DataObjects.begin(); iDataObjects != DataObjects.end(); ++iDataObjects) {
			if (iDataObjects->pAdressf == inRefcon) {

				DebugOut("========== Found " + iDataObjects->DataRef);

				if (iDataObjects->FFID <= 0) {
					iDataObjects->FFID = ffAPI.ValueIdByName(iDataObjects->FFVar.c_str());
				}

				ffAPI.ValueSet(iDataObjects->FFID, &inValue);

				break;
			}
		}
		DebugOut("========== DATAREF SET Done.");

	}


	*my_var = inValue;
}



/*
* DumpCommandsToLog
*
* Dumps all commands to log.txt
*
*/
void DumpCommandsToLog()
{
	LogWrite("=============== DUMP OF ALL COMMANDS =================");
	list<DataObject>::iterator  iDataObjects;

	for (iDataObjects = DataObjects.begin(); iDataObjects != DataObjects.end(); ++iDataObjects) {

		if ((iDataObjects->Type == OBJECT_TYPE_COMMAND) || (iDataObjects->Type == OBJECT_TYPE_COMMANDTODATAREF)) {
			LogWrite(iDataObjects->CommandName + " (" + iDataObjects->Command + ")");
		}
	}
	LogWrite("=============== DUMP END =================");
}


/*
* DumpDatarefsToLog
*
* Dumps all commands to log.txt
*
*/
void DumpDatarefsToLog()
{
	LogWrite("=============== DUMP OF ALL DATAREFS =================");
	list<DataObject>::iterator  iDataObjects;

	for (iDataObjects = DataObjects.begin(); iDataObjects != DataObjects.end(); ++iDataObjects) {

		if (iDataObjects->Type == OBJECT_TYPE_DATAREF) {
			LogWrite(iDataObjects->DataRef + " / " + to_string(iDataObjects->ValueType));
		}
	}
	LogWrite("=============== DUMP END =================");
}

/*
* DumpObjectsToLog
*
* Dumps all Objects and Parameters to the Log.txt
*
*/
void DumpObjectsToLog() {
	LogWrite("=============== DUMP OF ALL A320 OBJECTS AND PARAMETERS =================");
	unsigned int valuesCount = ffAPI.ValuesCount();
	int valueID = -1;

	unsigned int ii = 0;
	for (ii = 0; ii < valuesCount; ii++) {
		int TmpParentID = -1;
		int TmpValueID = -1;
		string FullObjectName = "";

		char *valueName, *valueDescription;

		valueID = ffAPI.ValueIdByIndex(ii);

		if (valueID >= 0) { 
			valueName = (char *)ffAPI.ValueName(valueID);
			valueDescription = (char *)ffAPI.ValueDesc(valueID);

			unsigned int valueType = ffAPI.ValueType(valueID);
			unsigned int valueFlag = ffAPI.ValueFlags(valueID);

			int parentValueID = ffAPI.ValueParent(valueID);

			// Here we get all the parents to get the full name of the object
			TmpValueID = valueID;
			TmpParentID = parentValueID;
			while ((TmpParentID > 0) && (TmpValueID > 0)) {
				TmpParentID = ffAPI.ValueParent(TmpValueID);
				if ((TmpParentID >= 0) && (TmpValueID >= 0)) FullObjectName = string((char *)ffAPI.ValueName(TmpParentID)) + string(".") + FullObjectName;
				TmpValueID = TmpParentID;
			}
			FullObjectName += string(valueName);

			char *valueTypeString;

			if (valueType == Value_Type_Deleted) {
				valueTypeString = "Deleted";
			}
			else if (valueType == Value_Type_Object) {
				valueTypeString = "Object";
			}
			else if (valueType == Value_Type_sint8) {
				valueTypeString = "sint8";
			}
			else if (valueType == Value_Type_uint8) {
				valueTypeString = "uint8";
			}
			else if (valueType == Value_Type_sint16) {
				valueTypeString = "sint16";
			}
			else if (valueType == Value_Type_uint16) {
				valueTypeString = "uint16";
			}
			else if (valueType == Value_Type_sint32) {
				valueTypeString = "sint32";
			}
			else if (valueType == Value_Type_uint32) {
				valueTypeString = "uint32";
			}
			else if (valueType == Value_Type_float32) {
				valueTypeString = "float32";
			}
			else if (valueType == Value_Type_float64) {
				valueTypeString = "float64";
			}
			else if (valueType == Value_Type_String) {
				valueTypeString = "String";
			}
			else if (valueType == Value_Type_Time) {
				valueTypeString = "Time";
			}
			else {
				valueTypeString = "UNKNOWN";
			}

			LogWrite("#" + to_string(valueID) + ": " + FullObjectName + " - " + string(valueDescription) + " (" + valueTypeString + ")" + " Value-Flag: " + to_string(valueFlag));

		}

	}
	LogWrite("=============== DUMP END =================");

	DumpObjectsToLogActive = false;
}




/*
* ReadConfigs
*
* Reads the config files
*
*/
void ReadConfigs() {
	LogWrite("Starting Reload.");

	/* Leave if plugin was disabled */
	if (plugindisabled == true) return;

	/* Set the paths */
	get_paths();

	/* First destroy and clear all Objects */
	list<DataObject>::iterator  iDataObjects;

	for (iDataObjects = DataObjects.begin(); iDataObjects != DataObjects.end(); ++iDataObjects) {
		iDataObjects->destroy();
	}
	DataObjects.clear();

	// Read each config file
	for_each(filenames.begin(), filenames.end(), &ReadConfig);

	LogWrite("Reload complete.");
}


/*
* ReadConfig
*
* Reads the cfg from plugin or aircraft directory and initializes the DataObjects
*
*/
void ReadConfig(string filename) {
	
	ifstream input(filename.c_str());
	ifstream input2(filename.c_str());	// used for old format

	if (!input || !input2)
	{
		LogWrite(" -> ==== ERROR: COULD NOT READ " + filename + " ===");
		return;
	}
	else {
		LogWrite("-> Parsing " + filename);
	}

	string line;
	int objcounter = 0;
	int datarefcounter = 0;
	int resultCount[] = {0, 0, 0};	// Keep track on how many commands, comdefs and datarefs have been added.
	regex re_category("^\\[([a-zA-z\\.]*)\\]$"); // https://regex101.com/r/y1plMJ/1
	regex re_setting("^([a-zA-Z\\._-]*)=(.*)$"); // https://regex101.com/r/tDwwrN/1
	bool entryNeedsToBeAdded = false;
	DataObject NewObj;
	int lineNumber = 0;

	while (std::getline(input, line))
	{
		lineNumber++;
		smatch matches;

		// Format introduced with version 1.1.7.
		// Match anything between square brackets (start of an entry), for example [DATAREF] or [COMMAND].
		if (line.empty()) {
			// Add entry (new line ends previous entry)
			if (NewObj.Type != -1) {
				if (!NewObj.SyntaxError) {
					if (NewObj.Type == OBJECT_TYPE_COMMAND)
					{
						resultCount[0]++;

						if (NewObj.FFReference == "" && NewObj.FFReferenceID == -1) {
							NewObj.FFReference = NewObj.FFVar;
						}
					}
					else if (NewObj.Type == OBJECT_TYPE_COMMANDTODATAREF) resultCount[1]++;
					else if (NewObj.Type == OBJECT_TYPE_DATAREF)
					{
						resultCount[2]++;

						if (NewObj.DataRef == "NORM") {
							string normdref = NewObj.FFVar;
							replace(normdref.begin(), normdref.end(), '.', '/');
							NewObj.DataRef = "FFA320Connector/" + normdref;
						}
					}

					// Add entry
					NewObj.RefConID = datarefcounter++;
					NewObj.initialize();
					DataObjects.push_back(NewObj);
				}

				NewObj.Type = -1;
			}

		}
		else if (regex_search(line, matches, re_category)) {
			// Add entry (new entry ends previous entry)
			if (NewObj.Type != -1) {
				if (!NewObj.SyntaxError) {
					if (NewObj.Type == OBJECT_TYPE_COMMAND)
					{
						resultCount[0]++;

						if (NewObj.FFReference == "" && NewObj.FFReferenceID == -1) {
							NewObj.FFReference = NewObj.FFVar;
						}
					}
					else if (NewObj.Type == OBJECT_TYPE_COMMANDTODATAREF) resultCount[1]++;
					else if (NewObj.Type == OBJECT_TYPE_DATAREF)
					{
						resultCount[2]++;

						if (NewObj.DataRef == "NORM") {
							string normdref = NewObj.FFVar;
							replace(normdref.begin(), normdref.end(), '.', '/');
							NewObj.DataRef = "FFA320Connector/" + normdref;
						}
					}

					// Add entry
					NewObj.RefConID = datarefcounter++;
					NewObj.initialize();
					DataObjects.push_back(NewObj);
				}

				NewObj.Type = -1;
			}

			NewObj = DataObject();	// Create a new Data Object for the Command / Dataref
			NewObj.Type = StringToObjectType(matches[1].str());
			entryNeedsToBeAdded = true;

			// Set default values
			NewObj.DataRefMultiplier = 1;
			NewObj.SyntaxError = false;
			NewObj.IgnoreExistingDataRef = false;
			NewObj.DataRef = "NORM";
			NewObj.Phase = 0;

			NewObj.FFVar = "";
			NewObj.FFID = -1;
			NewObj.FFReference = "";
			NewObj.FFReferenceID = -1;

			NewObj.ValueType = StringToValueType("INT");
			NewObj.Value = 1;
			NewObj.MinValue = 0;
			NewObj.MaxValue = 0;

			NewObj.SpeedRef = 0;
		}

		// Get a setting from command, dataref or comdef entry if it matches
		if (regex_search(line, matches, re_setting)) {
			string key = trim(matches[1].str());
			string value = trim(matches[2].str());

			if (key.length() == 0 || value.length() == 0) {
				LogWrite("Error: Invalid syntax. (File: '" + filename + "', line: " + to_string(lineNumber) + ")");
				NewObj.SyntaxError = true;
			}

			if (NewObj.Type == OBJECT_TYPE_COMMAND)
			{
				if (key == "MODE") {
					NewObj.WorkMode = StringToWorkMode(value);
				}
				else if (key == "TYPE") {
					NewObj.ValueType = StringToValueType(value);
				}
				else if (key == "OBJECT") {
					NewObj.FFVar = value;
				}
				else if (key == "COMMAND") {
					NewObj.Command = value;
				}
				else if (key == "COMMAND_NAME") {
					NewObj.CommandName = value;
				}
				else if (key == "VALUE") {
					if (NewObj.ValueType == VALUE_TYPE_INT) NewObj.Value = stoi(value);
					if (NewObj.ValueType == VALUE_TYPE_FLOAT) NewObj.ValueFloat = stof(value);
				}
				else if (key == "OBJECT_ID") {
					NewObj.FFID = stoi(value); // FlightFactor Object ID
				}
				else if (key == "OBJECT_REFERENCE") {
					NewObj.FFReference = value; // FlightFactor Object Reference
				}
				else if (key == "OBJECT_REFERENCE_ID") {
					NewObj.FFReferenceID = stoi(value); // FlightFactor Object ReferenceID
				}
				else if (key == "VALUE_MINIMUM") {
					if (NewObj.ValueType == VALUE_TYPE_INT) NewObj.MinValue = stoi(value);
					if (NewObj.ValueType == VALUE_TYPE_FLOAT) NewObj.MinValueFloat = stof(value);
				}
				else if (key == "VALUE_MAXIMUM") {
					if (NewObj.ValueType == VALUE_TYPE_INT) NewObj.MaxValue = stoi(value);
					if (NewObj.ValueType == VALUE_TYPE_FLOAT) NewObj.MaxValueFloat = stof(value);
				}
				else if (key == "SPEED") {
					NewObj.SpeedRef = stoi(value);
				}
				else if (key == "PHASE") {
					if (value == "SINGLE_CLICK") NewObj.Phase = 0;
					if (value == "CONTINUOUS") NewObj.Phase = 1;
				}

				NewObj.NeedsUpdate = false;
			}
			else if (NewObj.Type == OBJECT_TYPE_COMMANDTODATAREF)
			{
				if (key == "MODE") {
					NewObj.WorkMode = StringToWorkMode(value);
				}
				else if (key == "TYPE") {
					NewObj.ValueType = StringToValueType(value);
				}
				else if (key == "PHASE") {
					if (value == "SINGLE_CLICK") NewObj.Phase = 0;
					if (value == "CONTINUOUS") NewObj.Phase = 1;
				}
				else if (key == "COMMAND") {
					NewObj.Command = value;
				}
				else if (key == "COMMAND_NAME") {
					NewObj.CommandName = value;
				}
				else if (key == "DATAREF") {
					NewObj.DataRef = value;
				}
				else if (key == "VALUES") {

					// Split value by comma using regex.
					regex re("([^,]+)");

					int j = 0;

					for (sregex_iterator i = sregex_iterator(value.begin(), value.end(), re);
						i != sregex_iterator();
						++i)
					{
						smatch match = *i;

						if (j == 0) {
							if (NewObj.ValueType == VALUE_TYPE_INT) NewObj.Value = stoi(match.str());
							if (NewObj.ValueType == VALUE_TYPE_FLOAT) NewObj.ValueFloat = stof(match.str());
						}

						if (NewObj.ValueType == VALUE_TYPE_INT) NewObj.VarArrI[j] = stoi(match.str());
						if (NewObj.ValueType == VALUE_TYPE_FLOAT) NewObj.VarArrF[j] = stof(match.str());

						NewObj.VarArrValues++;
						j++;
					}
				}

				NewObj.NeedsUpdate = false;
			}
			else if (NewObj.Type == OBJECT_TYPE_DATAREF)
			{
				if (key == "TYPE") {
					NewObj.ValueType = StringToValueType(value);
				}
				else if (key == "OBJECT") {
					NewObj.FFVar = value;
				}
				else if (key == "OBJECT_ID") {
					NewObj.FFID = stoi(value); // FlightFactor Object ID
				}
				else if (key == "DATAREF") {
					size_t first = value.find("[");
					size_t last = value.find("]");

					if ((first != string::npos) && (first != string::npos)) {
						NewObj.DataRefOffset = stoi(value.substr(first + 1, last - first - 1));
						NewObj.DataRef = value.substr(0, first);
						DebugOut("ARRAY: " + NewObj.DataRef + " -> " + to_string(NewObj.DataRefOffset) + " -> " + to_string(first) + " " + to_string(last));
					}
					else {
						NewObj.DataRef = value;
					}
				}
				else if (key == "DATAREF_TYPE") {
					NewObj.DataRefValueType = StringToValueType(value);
				}
				else if (key == "IGNORE_EXISTING") {
					if (value == "TRUE") NewObj.IgnoreExistingDataRef = true;
				}
				else if (key == "MULTIPLIER" && value != "") {
					NewObj.DataRefMultiplier = stof(value);
				}
				else if (key == "CONDITION")  {
					NewObj.DatarefCondition = StringToCondition(value);
				}
				else if (key == "CONDITION_VALUE") {
					NewObj.DatarefConditionValue = stof(value);
				}
				else if (key == "CONDITION_RESULT") {
					if (NewObj.DataRefValueType == VALUE_TYPE_INT) NewObj.Value = stoi(value);
					if (NewObj.DataRefValueType == VALUE_TYPE_FLOAT) NewObj.ValueFloat = stof(value);
				}

				NewObj.NeedsUpdate = true;
			}
		}
	}

	// Make sure the last entry will be added in case there's no new line next.
	if (NewObj.Type != -1) {
		if (!NewObj.SyntaxError) {
			if (NewObj.Type == OBJECT_TYPE_COMMAND)
			{
				resultCount[0]++;

				if (NewObj.FFReference == "" && NewObj.FFReferenceID == -1) {
					NewObj.FFReference = NewObj.FFVar;
				}
			}
			else if (NewObj.Type == OBJECT_TYPE_COMMANDTODATAREF) resultCount[1]++;
			else if (NewObj.Type == OBJECT_TYPE_DATAREF)
			{
				resultCount[2]++;

				if (NewObj.DataRef == "NORM") {
					string normdref = NewObj.FFVar;
					replace(normdref.begin(), normdref.end(), '.', '/');
					NewObj.DataRef = "FFA320Connector/" + normdref;
				}
			}

			// Add entry
			NewObj.RefConID = datarefcounter++;
			NewObj.initialize();
			DataObjects.push_back(NewObj);
		}

		NewObj.Type = -1;
	}

	// format version <= 1.1.6
	lineNumber = 0;
	while (std::getline(input2, line))
	{
		lineNumber++;
		objcounter++;
		int i = 0;

		// If line is not a comment
		if (line.substr(0, 1) != "#") {
			if (line.find(";") > 4) {
				string s = line;
				string delimiter = ";";

				size_t pos = 0;
				string token;	// Each token between ";" (this is the variable 'value' in the new format)

				DataObject NewObj = DataObject();		// Create a new Data Object for the Command / Dataref

				NewObj.DataRefMultiplier = 1;	// Sets the Multiplier to 1 - may be changed later on
				NewObj.SyntaxError = false;

				while ((pos = s.find(delimiter)) != std::string::npos) {
					try {
						token = s.substr(0, pos);

						if (i == 0)	NewObj.Type = StringToObjectType(token);

						//Command
						if (NewObj.Type == OBJECT_TYPE_COMMAND) {
							if (i == 1) NewObj.WorkMode = StringToWorkMode(token);
							if (i == 2) NewObj.ValueType = StringToValueType(token);
							if (i == 3) NewObj.FFVar = token;
							if (i == 4) NewObj.Command = token;
							if (i == 5) NewObj.CommandName = token;
							if (i == 6) {
								if (NewObj.ValueType == VALUE_TYPE_INT) NewObj.Value = stoi(token);
								if (NewObj.ValueType == VALUE_TYPE_FLOAT) NewObj.ValueFloat = stof(token);
							}
							if (i == 7) NewObj.FFID = stoi(token);
							if (i == 8) NewObj.FFReference = token;
							if (i == 9) NewObj.FFReferenceID = stoi(token);
							if (i == 10) {
								if (NewObj.ValueType == VALUE_TYPE_INT) NewObj.MinValue = stoi(token);
								if (NewObj.ValueType == VALUE_TYPE_FLOAT) NewObj.MinValueFloat = stof(token);
							}
							if (i == 11) {
								if (NewObj.ValueType == VALUE_TYPE_INT) NewObj.MaxValue = stoi(token);
								if (NewObj.ValueType == VALUE_TYPE_FLOAT) NewObj.MaxValueFloat = stof(token);
							}
							if (i == 12) NewObj.SpeedRef = stoi(token);
							if (i == 13) NewObj.Phase = stoi(token);

							NewObj.NeedsUpdate = false;
						}

						//Command to Dataref
						if (NewObj.Type == OBJECT_TYPE_COMMANDTODATAREF) {
							if (i == 1) NewObj.WorkMode = StringToWorkMode(token);
							if (i == 2) NewObj.ValueType = StringToValueType(token);
							if (i == 3) NewObj.Phase = stoi(token);
							if (i == 4) NewObj.Command = token;
							if (i == 5) NewObj.CommandName = token;
							if (i == 6) NewObj.DataRef = token;

							if (i == 7) {
								if (NewObj.ValueType == VALUE_TYPE_INT) NewObj.Value = stoi(token);
								if (NewObj.ValueType == VALUE_TYPE_FLOAT) NewObj.ValueFloat = stof(token);
							}
							if (i >= 7){
								if (token != "") {
									if (NewObj.ValueType == VALUE_TYPE_INT) NewObj.VarArrI[i - 7] = stoi(token);
									if (NewObj.ValueType == VALUE_TYPE_FLOAT) NewObj.VarArrF[i - 7] = stof(token);
									NewObj.VarArrValues++;
								}
							}

							NewObj.NeedsUpdate = false;
						}

						//Dataref
						if (NewObj.Type == OBJECT_TYPE_DATAREF) {
							if (i == 1) NewObj.ValueType = StringToValueType(token);
							if (i == 2) NewObj.FFVar = token;
							if (i == 3) NewObj.FFID = stoi(token);
							if (i == 4) {
								size_t first = token.find("[");
								size_t last = token.find("]");
								if ((first != string::npos) && (first != string::npos)) {
									NewObj.DataRefOffset = stoi(token.substr(first + 1, last - first - 1));
									NewObj.DataRef = token.substr(0, first);
									DebugOut("ARRAY: " + NewObj.DataRef + " -> " + to_string(NewObj.DataRefOffset) + " -> " + to_string(first) + " " + to_string(last));
								}
								else {
									NewObj.DataRef = token;
								}
							}
							if (i == 5) NewObj.DataRefValueType = StringToValueType(token);
							if ((i == 6) && (token == "IGNOREEXISTING")) NewObj.IgnoreExistingDataRef = true;
							if ((i == 7) && (token != "")) NewObj.DataRefMultiplier = stof(token);

							if (NewObj.DataRef == "NORM") {
								string normdref = NewObj.FFVar;
								replace(normdref.begin(), normdref.end(), '.', '/');
								NewObj.DataRef = "FFA320Connector/" + normdref;
							}

							if (i == 8) NewObj.DatarefCondition = StringToCondition(token);
							if (i == 9) NewObj.DatarefConditionValue = stof(token);
							if (i == 10) {
								if (NewObj.DataRefValueType == VALUE_TYPE_INT) NewObj.Value = stoi(token);
								if (NewObj.DataRefValueType == VALUE_TYPE_FLOAT) NewObj.ValueFloat = stof(token);
							}

							NewObj.NeedsUpdate = true;
						}


						s.erase(0, pos + delimiter.length());
						i++;
					}
					catch (...) {
						s.erase(0, pos + delimiter.length());
						i++;
						NewObj.SyntaxError = true;
						LogWrite("++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++");
						LogWrite("+ SYNTAX ERROR: Line: " + to_string(objcounter) + " Token: " + token);
						LogWrite("++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++");
					}

				}

				if (i > 1) {
					if (NewObj.SyntaxError == false) {
						NewObj.RefConID = datarefcounter++;
						NewObj.initialize();
						DataObjects.push_back(NewObj);

						// Deprecated warning message
						LogWrite("Warning: This entry looks like the previous format which is now deprecated and will be removed in the future. Please use the format introduced with version 1.1.7. (File: '" + filename + "', line: " + to_string(lineNumber) + ")");
					
						if (NewObj.Type == OBJECT_TYPE_COMMAND) resultCount[0]++;
						if (NewObj.Type == OBJECT_TYPE_COMMANDTODATAREF) resultCount[1]++;
						if (NewObj.Type == OBJECT_TYPE_DATAREF) resultCount[2]++;
					}
				}
			}
		}
	}

	LogWrite("Found " + to_string(resultCount[0]) + " COMMANDs, " + to_string(resultCount[1]) + " COMDEFs and " + to_string(resultCount[2]) + " DATAREFs in the file '" + filename + "'.");
}

// Trim string
string trim(const string& str)
{
	size_t first = str.find_first_not_of(' ');
	if (string::npos == first) {
		return str;
	}
	size_t last = str.find_last_not_of(' ');
	return str.substr(first, (last - first + 1));
}

/*
* ffAPIUpdateCallback
*
* FlightFactor Callback. Here we process the DataObject thingy
*
*/
void ffAPIUpdateCallback(double step, void *tag) {

	/* Leave if plugin was disabled */
	if (plugindisabled == true) return;

	/* When this is called, we know that the plugin is initialized correctly  */
	if (!plugininitialized) {
		LogWrite("Initialized.");
		plugininitialized = true;
	}
	

	if (DumpObjectsToLogActive == true) DumpObjectsToLog();

	/* Iterate thru the Objects and see what object needs to be updated */
	list<DataObject>::iterator  iDataObjects;

	for (iDataObjects = DataObjects.begin(); iDataObjects != DataObjects.end(); ++iDataObjects) {
		
		/* Resetting the Click-Commands to the MinValue */
		if (iDataObjects->NeedsClickUpdate == true) {
			if (iDataObjects->ClickTimer <= 0) {
				if (iDataObjects->ValueType == VALUE_TYPE_INT) ffAPI.ValueSet(iDataObjects->FFID, &iDataObjects->MinValue);
				if (iDataObjects->ValueType == VALUE_TYPE_FLOAT) ffAPI.ValueSet(iDataObjects->FFID, &iDataObjects->MinValueFloat);
				iDataObjects->NeedsClickUpdate = false;
			}
			else {
				iDataObjects->ClickTimer--;
			}
		}

		/* The main interface between XP and the A320 */
		if (iDataObjects->NeedsUpdate == true) {
			
			if ((iDataObjects->Type == OBJECT_TYPE_COMMAND) || (iDataObjects->Type == OBJECT_TYPE_DATAREF)) {
				/* First find and set the Object-ID - This is only done once per Object (if needed) */
				if (iDataObjects->FFID <= 0) {
					iDataObjects->FFID = ffAPI.ValueIdByName(iDataObjects->FFVar.c_str());
				}

				/* First find and set the Object-Reference-ID - This is only done once per Object (if needed) */
				if (iDataObjects->FFReferenceID <= 0) {
					iDataObjects->FFReferenceID = ffAPI.ValueIdByName(iDataObjects->FFReference.c_str());
				}
			}

			/* Executed Commands are ported to the A320 here */
			if (iDataObjects->Type == OBJECT_TYPE_COMMAND) {

				DebugOut("Updating COMMAND " + iDataObjects->Command + " - " + iDataObjects->CommandName);

				/* Workmode SET */
				if (iDataObjects->WorkMode == WORK_MODE_SET) {
					if (iDataObjects->ValueType == VALUE_TYPE_INT) ffAPI.ValueSet(iDataObjects->FFID, &iDataObjects->Value);
					if (iDataObjects->ValueType == VALUE_TYPE_FLOAT) ffAPI.ValueSet(iDataObjects->FFID, &iDataObjects->ValueFloat);
				}

				/* Workmode STEP */
				if (iDataObjects->WorkMode == WORK_MODE_STEP) {
					if (iDataObjects->ValueType == VALUE_TYPE_INT) {
						int curval;
						ffAPI.ValueGet(iDataObjects->FFReferenceID, &curval);

						if ((curval + iDataObjects->Value <= iDataObjects->MaxValue) && (curval + iDataObjects->Value >= iDataObjects->MinValue)) {
							int newval = curval + iDataObjects->Value;
							ffAPI.ValueSet(iDataObjects->FFID, &newval);
						}
					}
					if (iDataObjects->ValueType == VALUE_TYPE_FLOAT) {
						float curval;
						ffAPI.ValueGet(iDataObjects->FFReferenceID, &curval);

						if ((curval + iDataObjects->ValueFloat <= iDataObjects->MaxValueFloat) && (curval + iDataObjects->ValueFloat >= iDataObjects->MinValueFloat)) {
							float newval = curval + iDataObjects->ValueFloat;
							ffAPI.ValueSet(iDataObjects->FFID, &newval);
						}
					}
				}

				/* Workmode CYCLE */
				if (iDataObjects->WorkMode == WORK_MODE_CYCLE) {
					if (iDataObjects->ValueType == VALUE_TYPE_INT) {
						int curval;
						ffAPI.ValueGet(iDataObjects->FFReferenceID, &curval);

						int newval = curval + iDataObjects->Value;
						if (newval > iDataObjects->MaxValue) newval = iDataObjects->MinValue;
						if (newval < iDataObjects->MinValue) newval = iDataObjects->MaxValue;

						ffAPI.ValueSet(iDataObjects->FFID, &newval);
					}
					if (iDataObjects->ValueType == VALUE_TYPE_FLOAT) {
						float curval;
						ffAPI.ValueGet(iDataObjects->FFReferenceID, &curval);

						float newval = curval + iDataObjects->ValueFloat;
						if (newval > iDataObjects->MaxValueFloat) newval = iDataObjects->MinValueFloat;
						if (newval < iDataObjects->MinValueFloat) newval = iDataObjects->MaxValueFloat;

						ffAPI.ValueSet(iDataObjects->FFID, &newval);
					}
				}

				/* Workmode ROTATE */
				if (iDataObjects->WorkMode == WORK_MODE_ROTATE) {
					if (iDataObjects->ValueType == VALUE_TYPE_INT) {
						int curval;
						ffAPI.ValueGet(iDataObjects->FFReferenceID, &curval);
						int newval = curval + iDataObjects->Value;
						ffAPI.ValueSet(iDataObjects->FFID, &newval);
					}
					if (iDataObjects->ValueType == VALUE_TYPE_FLOAT) {
						float curval;
						ffAPI.ValueGet(iDataObjects->FFReferenceID, &curval);
						float newval = curval + iDataObjects->ValueFloat;
						ffAPI.ValueSet(iDataObjects->FFID, &newval);
					}
				}

				/* Workmode CLICK */
				if (iDataObjects->WorkMode == WORK_MODE_CLICK) {
					if (iDataObjects->ValueType == VALUE_TYPE_INT) {
						ffAPI.ValueSet(iDataObjects->FFID, &iDataObjects->Value);
						iDataObjects->NeedsClickUpdate = true;
						iDataObjects->ClickTimer = 5;
					}
					if (iDataObjects->ValueType == VALUE_TYPE_FLOAT) {
						ffAPI.ValueSet(iDataObjects->FFID, &iDataObjects->ValueFloat);
						iDataObjects->NeedsClickUpdate = true;
						iDataObjects->ClickTimer = 5;
					}
				}
				iDataObjects->NeedsUpdate = false;
			}


			/* Executed Commands 2 Datarefs are processed here */
			if (iDataObjects->Type == OBJECT_TYPE_COMMANDTODATAREF) {
				
				//Integer
				if (iDataObjects->ValueType == VALUE_TYPE_INT) {
					int curval = XPLMGetDatai(iDataObjects->DREF);

					if (iDataObjects->WorkMode == WORK_MODE_UP) {
						for (int i = 0; i < iDataObjects->VarArrValues; ++i) {
							if ((curval >= iDataObjects->VarArrI[i]) && (i + 1 <= iDataObjects->VarArrValues - 1)) {
								if (curval < iDataObjects->VarArrI[i + 1]) {
									XPLMSetDatai(iDataObjects->DREF, iDataObjects->VarArrI[i + 1]);
									break;
								}
							}
						}
					}

					if (iDataObjects->WorkMode == WORK_MODE_DOWN) {
						for (int i = 0; i < iDataObjects->VarArrValues; ++i) {
							if ((curval <= iDataObjects->VarArrI[i]) && (i - 1 >= 0)) {
								if (curval > iDataObjects->VarArrI[i - 1]) {
									XPLMSetDatai(iDataObjects->DREF, iDataObjects->VarArrI[i - 1]);
									break;
								}
							}
						}
					}

					if (iDataObjects->WorkMode == WORK_MODE_SET) {
						XPLMSetDatai(iDataObjects->DREF, iDataObjects->Value);
					}
				}

				//Float
				if (iDataObjects->ValueType == VALUE_TYPE_FLOAT) {
					float curval = XPLMGetDataf(iDataObjects->DREF);

					if (iDataObjects->WorkMode == WORK_MODE_UP) {
						for (int i = 0; i < iDataObjects->VarArrValues; ++i) {
							if ((curval >= iDataObjects->VarArrF[i]) && (i + 1 <= iDataObjects->VarArrValues - 1)) {
								if (curval < iDataObjects->VarArrF[i+1]) {
									XPLMSetDataf(iDataObjects->DREF, iDataObjects->VarArrF[i + 1]);
									break;
								}
							}
						}
					}

					if (iDataObjects->WorkMode == WORK_MODE_DOWN) {
						for (int i = 0; i < iDataObjects->VarArrValues; ++i) {
							if ((curval <= iDataObjects->VarArrF[i]) && (i - 1 >= 0)) {
								if (curval > iDataObjects->VarArrF[i - 1]) {
									XPLMSetDataf(iDataObjects->DREF, iDataObjects->VarArrF[i - 1]);
									break;
								}
							}
						}
					}

					if (iDataObjects->WorkMode == WORK_MODE_SET) {
						XPLMSetDataf(iDataObjects->DREF, iDataObjects->ValueFloat);
					}

				}

				iDataObjects->SpeedRef = 0;
				iDataObjects->NextUpdateCycle = 0;
				iDataObjects->NeedsUpdate = false;
			}


			/* Datarefs are handled here */
			if (iDataObjects->Type == OBJECT_TYPE_DATAREF) {
				DebugOut("Updating DATAREF " + iDataObjects->DataRef  + " - " + to_string(iDataObjects->ValueType) + " - " + iDataObjects->FFVar);
				
				InternalDatarefUpdate = true;	// While this is true, no external DataRef-Updates are triggered. Otherwise we can not decide if the update
												// comes from an external source or from this plugin.

				// No conditions
				if (iDataObjects->DatarefCondition == CONDITION_NONE) {
					// Set the Dataref according to the Dataref Value Type INT / FLOAT
					if (iDataObjects->DataRefValueType == VALUE_TYPE_INT)  {
						DebugOut(" -> INT TO INT");
						void* curval;
						ffAPI.ValueGet(iDataObjects->FFID, &curval);
						if (iDataObjects->DataRefOffset < 0) {
							XPLMSetDatai(iDataObjects->DREF, (int)(size_t)curval * iDataObjects->DataRefMultiplier);
						}
						else {
							int idata = (int)(size_t)curval * iDataObjects->DataRefMultiplier;
							XPLMSetDatavi(iDataObjects->DREF, &idata, iDataObjects->DataRefOffset, 1);
						}
					}

					if (iDataObjects->DataRefValueType == VALUE_TYPE_FLOAT) {

						//Read the A320 Data in the specific format
						if (iDataObjects->ValueType == VALUE_TYPE_INT) {
							DebugOut(" -> INT TO FLOAT");
							int icurval;
							ffAPI.ValueGet(iDataObjects->FFID, &icurval);
							if (iDataObjects->DataRefOffset < 0) {
								XPLMSetDataf(iDataObjects->DREF, (float)icurval * iDataObjects->DataRefMultiplier);
							}
							else {
								float idata = (float)(size_t)icurval * iDataObjects->DataRefMultiplier;
								XPLMSetDatavf(iDataObjects->DREF, &idata, iDataObjects->DataRefOffset, 1);
							}
						}
						else if (iDataObjects->ValueType == VALUE_TYPE_FLOAT) {
							DebugOut(" -> FLOAT TO FLOAT");
							float fcurval;
							ffAPI.ValueGet(iDataObjects->FFID, &fcurval);
							if (iDataObjects->DataRefOffset < 0) {
								XPLMSetDataf(iDataObjects->DREF, fcurval * iDataObjects->DataRefMultiplier);
							}
							else {
								float idata = fcurval * iDataObjects->DataRefMultiplier;
								XPLMSetDatavf(iDataObjects->DREF, &idata, iDataObjects->DataRefOffset, 1);
							}
						}

					}

				}
				// With condition
				else {

					void* curval;
					ffAPI.ValueGet(iDataObjects->FFID, &curval);

					if (iDataObjects->DatarefCondition == CONDITION_EQUALS) {
						if ((int)(size_t)curval == iDataObjects->DatarefConditionValue) {
							if (iDataObjects->DataRefValueType == VALUE_TYPE_INT) {
								if (iDataObjects->DataRefOffset < 0) {
									XPLMSetDatai(iDataObjects->DREF, iDataObjects->Value * iDataObjects->DataRefMultiplier);
								}
								else {
									int idata = iDataObjects->Value * iDataObjects->DataRefMultiplier;
									XPLMSetDatavi(iDataObjects->DREF, &idata, iDataObjects->DataRefOffset, 1);
								}
							}
							if (iDataObjects->DataRefValueType == VALUE_TYPE_FLOAT) {
								if (iDataObjects->DataRefOffset < 0) {
									XPLMSetDataf(iDataObjects->DREF, iDataObjects->ValueFloat * iDataObjects->DataRefMultiplier);
								}
								else {
									float idata = iDataObjects->ValueFloat * iDataObjects->DataRefMultiplier;
									XPLMSetDatavf(iDataObjects->DREF, &idata, iDataObjects->DataRefOffset, 1);
								}
							}
						}
					}

					if (iDataObjects->DatarefCondition == CONDITION_GREATER) {
						if ((int)(size_t)curval > iDataObjects->DatarefConditionValue) {
							if (iDataObjects->DataRefValueType == VALUE_TYPE_INT) {
								if (iDataObjects->DataRefOffset < 0) {
									XPLMSetDatai(iDataObjects->DREF, iDataObjects->Value * iDataObjects->DataRefMultiplier);
								}
								else {
									int idata = iDataObjects->Value * iDataObjects->DataRefMultiplier;
									XPLMSetDatavi(iDataObjects->DREF, &idata, iDataObjects->DataRefOffset, 1);
								}
							}
							if (iDataObjects->DataRefValueType == VALUE_TYPE_FLOAT) {
								if (iDataObjects->DataRefOffset < 0) {
									XPLMSetDataf(iDataObjects->DREF, iDataObjects->ValueFloat * iDataObjects->DataRefMultiplier);
								}
								else {
									float idata = iDataObjects->ValueFloat * iDataObjects->DataRefMultiplier;
									XPLMSetDatavf(iDataObjects->DREF, &idata, iDataObjects->DataRefOffset, 1);
								}
							}
						}
					}

					if (iDataObjects->DatarefCondition == CONDITION_LOWER) {
						if ((int)(size_t)curval < iDataObjects->DatarefConditionValue) {
							if (iDataObjects->DataRefValueType == VALUE_TYPE_INT) {
								if (iDataObjects->DataRefOffset < 0) {
									XPLMSetDatai(iDataObjects->DREF, iDataObjects->Value * iDataObjects->DataRefMultiplier);
								}
								else {
									int idata = iDataObjects->Value * iDataObjects->DataRefMultiplier;
									XPLMSetDatavi(iDataObjects->DREF, &idata, iDataObjects->DataRefOffset, 1);
								}
							}
							if (iDataObjects->DataRefValueType == VALUE_TYPE_FLOAT) {
								if (iDataObjects->DataRefOffset < 0) {
									XPLMSetDataf(iDataObjects->DREF, iDataObjects->ValueFloat * iDataObjects->DataRefMultiplier);
								}
								else {
									float idata = iDataObjects->ValueFloat * iDataObjects->DataRefMultiplier;
									XPLMSetDatavf(iDataObjects->DREF, &idata, iDataObjects->DataRefOffset, 1);
								}
							}
						}
					}

					if (iDataObjects->DatarefCondition == CONDITION_NOTEQUAL) {
						if ((int)(size_t)curval != iDataObjects->DatarefConditionValue) {
							if (iDataObjects->DataRefValueType == VALUE_TYPE_INT) {
								if (iDataObjects->DataRefOffset < 0) {
									XPLMSetDatai(iDataObjects->DREF, iDataObjects->Value * iDataObjects->DataRefMultiplier);
								}
								else {
									int idata = iDataObjects->Value * iDataObjects->DataRefMultiplier;
									XPLMSetDatavi(iDataObjects->DREF, &idata, iDataObjects->DataRefOffset, 1);
								}
							}
							if (iDataObjects->DataRefValueType == VALUE_TYPE_FLOAT) {
								if (iDataObjects->DataRefOffset < 0) {
									XPLMSetDataf(iDataObjects->DREF, iDataObjects->ValueFloat * iDataObjects->DataRefMultiplier);
								}
								else {
									float idata = iDataObjects->ValueFloat * iDataObjects->DataRefMultiplier;
									XPLMSetDatavf(iDataObjects->DREF, &idata, iDataObjects->DataRefOffset, 1);
								}
							}
						}
					}

					if (iDataObjects->DatarefCondition == CONDITION_GREATER_EQUAL) {
						if ((int)(size_t)curval >= iDataObjects->DatarefConditionValue) {
							if (iDataObjects->DataRefValueType == VALUE_TYPE_INT) {
								if (iDataObjects->DataRefOffset < 0) {
									XPLMSetDatai(iDataObjects->DREF, iDataObjects->Value * iDataObjects->DataRefMultiplier);
								}
								else {
									int idata = iDataObjects->Value * iDataObjects->DataRefMultiplier;
									XPLMSetDatavi(iDataObjects->DREF, &idata, iDataObjects->DataRefOffset, 1);
								}
							}
							if (iDataObjects->DataRefValueType == VALUE_TYPE_FLOAT) {
								if (iDataObjects->DataRefOffset < 0) {
									XPLMSetDataf(iDataObjects->DREF, iDataObjects->ValueFloat * iDataObjects->DataRefMultiplier);
								}
								else {
									float idata = iDataObjects->ValueFloat * iDataObjects->DataRefMultiplier;
									XPLMSetDatavf(iDataObjects->DREF, &idata, iDataObjects->DataRefOffset, 1);
								}
							}
						}
					}

					if (iDataObjects->DatarefCondition == CONDITION_LOWER_EQUAL) {
						if ((int)(size_t)curval <= iDataObjects->DatarefConditionValue) {
							if (iDataObjects->DataRefValueType == VALUE_TYPE_INT) {
								if (iDataObjects->DataRefOffset < 0) {
									XPLMSetDatai(iDataObjects->DREF, iDataObjects->Value * iDataObjects->DataRefMultiplier);
								}
								else {
									int idata = iDataObjects->Value * iDataObjects->DataRefMultiplier;
									XPLMSetDatavi(iDataObjects->DREF, &idata, iDataObjects->DataRefOffset, 1);
								}
							}
							if (iDataObjects->DataRefValueType == VALUE_TYPE_FLOAT) {
								if (iDataObjects->DataRefOffset < 0) {
									XPLMSetDataf(iDataObjects->DREF, iDataObjects->ValueFloat * iDataObjects->DataRefMultiplier);
								}
								else {
									float idata = iDataObjects->ValueFloat * iDataObjects->DataRefMultiplier;
									XPLMSetDatavf(iDataObjects->DREF, &idata, iDataObjects->DataRefOffset, 1);
								}
							}
						}
					}
					
				}

				InternalDatarefUpdate = false;

			}

		}



	}

}

/*
* PluginCustomFlightLoopCallback
*
* This is called once for initialization
*
*/
float PluginCustomFlightLoopCallback(float elapsedMe, float elapsedSim, int counter, void * refcon)
{
	
	/* Export the Datarefs to the DRE */
	list<DataObject>::iterator  iDataObjects;
	XPLMPluginID PluginID = XPLMFindPluginBySignature("xplanesdk.examples.DataRefEditor");
	for (iDataObjects = DataObjects.begin(); iDataObjects != DataObjects.end(); ++iDataObjects) {
		if (iDataObjects->Type == OBJECT_TYPE_DATAREF) {
			if ((PluginID != XPLM_NO_PLUGIN_ID)){
				XPLMSendMessageToPlugin(PluginID, MSG_ADD_DATAREF, (void*)iDataObjects->DataRef.c_str());
			}
		}
	}

	/* Finding the FF A320 API */
	if (ffPluginID == XPLM_NO_PLUGIN_ID) {
		ffPluginID = XPLMFindPluginBySignature(XPLM_FF_SIGNATURE);
		return -1.0;
	}

	/* Initializing the FF A320 API */
	XPLMSendMessageToPlugin(ffPluginID, XPLM_FF_MSG_GET_SHARED_INTERFACE, &ffAPI);

	if (ffAPI.DataVersion != NULL) {
		ffAPIdataversion = ffAPI.DataVersion();
	}

	if (ffAPI.DataAddUpdate != NULL) {
		tag = (void*)"ffa320connector";
		ffAPI.DataAddUpdate(&ffAPIUpdateCallback, tag);
		LogWrite("Callback registered.");
		return 0;
	}

	return -1.0;
}








/**********************************************/

/*
* XPluginStop
*
* Our cleanup routine.
*
*/
PLUGIN_API void	XPluginStop(void)
{
	LogWrite("Stopped.");

	plugininitialized = false;
	/*********
		I had to uncomment the following lines, otherwise the
		plugin would crash XP when exiting from the initial menu or
		from a different A/C than the A320.
	***********/

	/*try {
		ffAPI.DataDelUpdate(&ffAPIUpdateCallback, tag);
		ffPluginID = XPLM_NO_PLUGIN_ID;
	}
	catch (int e) {

	}*/



}


/*
* XPluginDisable
*
* We do not need to do anything when we are disabled, but we must provide the handler.
*
*/
PLUGIN_API void XPluginDisable(void)
{
	LogWrite("Disabled.");
	plugindisabled = true;
}

/*
* XPluginEnable.
*
* We don't do any enable-specific initialization, but we must return 1 to indicate
* that we may be enabled at this time.
*
*/
PLUGIN_API int XPluginEnable(void)
{
	LogWrite("Enabled.");
	plugindisabled = false;
	return 1;
}

/*
* XPluginReceiveMessage
*
* We don't have to do anything in our receive message handler, but we must provide one.
*
*/
PLUGIN_API void XPluginReceiveMessage(
	XPLMPluginID	inFromWho,
	int				inMessage,
	void *			inParam)
{
	DebugOut("Message Received.");
}