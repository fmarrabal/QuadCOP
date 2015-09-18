/***********************************************************
	Raspberry Pi Flight System (RPFS)
	Written 2015 Joey Thompson
	Sponspored by Element 14 "Sci Fi Your Pi" contest

	Requires: gps.cpp,heading.cpp,i2c.cpp,screen.cpp
	Uses objects from WiringPI library and TinyGPS++
	See individual cpp files for more requirements

	Compiles on Raspberry Pi 2 using GCC G++

************************************************************/	

	

//Standard CPP includes
#include <iostream>
#include <string>
#include <fstream>
#include <sys/time.h>
#include <sstream>
using namespace std;

//Custom Includes
#include "gps.h"
#include "i2c.h"
#include "screen.h"
#include "heading.h"


#define VERSION		"BETA VERSION .93"



//Define Global Objects
WayPoint *recordWayPoints =  NULL;
WayPoint *wayPoints = NULL;
OledScreen screen;


//Pin assignments, these are Digital IN from ChipKit PI
#define PIAUTOMODE		12
#define PIMACRORECORD	1



//In HZ (samples per second).  Currenly Microstack GPS only sends at 1hz so sampling faster is pointless.
#define MACROREADPERIOD .5 


//Default 7000, which is 7000 seconds or 116 minutes or nearly 2 hours if sample rate is 1HZ.  
//1 HZ is the default microstack sample rate and currently does not appear to accept changes.
#define MAXRECORDWAYPOINTS 7000

//Error Defines
#define ERR_HEARTBEAT 1
#define ERR_CONTROLBYTE 2

//Sensor Defines, commands
#define REGPING1	1
#define REGPING2	2
#define REGPING3	3
#define REGPING4	4
#define REGFIRE	5
#define REGLED	6
#define CLEARQ	7
#define LEDON		1
#define LEDOFF	2



//Global Vars


//Mode Flags
bool autoMode = false;
bool macroRecordMode = false;
bool forceManual = false;
bool macroInProgress = false;
bool autoModeInProgress = false;
bool holdWayPoint = false;

//control Bytes
int currentControlByte = 65535;
int lastControlByte = 65535;
int nextControlByte = 65535;

bool climbRequest = false;
bool diveRequest = false;
bool requestRotateLeft = false;
bool requestRotateRight = false;

double distanceTraveled = 0.00;
double distanceTraveledABS = 0.00;


//File Descriptors for I2C communication
int controlSwitch = -1;
int sensorArray = -1;
int memsBoard = -1;


//GPS Vars
double currentLat;
double currentLong;
double previousLat;
double previousLong;
double targetLat;
double targetLong;
double velocityLat = 0;
double velocityLong = 0;
double currentHeading;
double previousHeading;
double headingOffset = 0;
double minAlt = 2;
double maxAlt = 10;

GPS *gps;
Heading *magHeading;

//These vars are used for timing
long currentMS;
long previousMS = 0;
long currentS;
long previousS = 0;
double lastLapsed = 0;
int loopCounter = 0;
double speed = 0;
long lastControlByteSent = 0;
double lastHeartBeat = 0; 
timeval currentTime;
long lastScreenUpdate = 0;
long bootup = 0;

double lastMacroRead;



//Macro Vars
int currentStep = 0;
int maxSteps = 0;
int recordCounter;
int currentWayPoint;


//Starts a global timer
void StartTimer()
{
	gettimeofday(&currentTime,NULL);
	previousMS  = currentTime.tv_usec;
        previousS = currentTime.tv_sec;
}

//Used as part of the timer functionality
double GetTimeStamp()
{
	double ts;
	gettimeofday(&currentTime,NULL);
	ts =  currentTime.tv_sec;
	ts +=  (double)currentTime.tv_usec / 1000000;
	return ts;
	
}

//Returns time lapsed in seconds
double GetLapsedTime(double timeStamp)
{
	double ts;
	gettimeofday(&currentTime,NULL);
        ts =  currentTime.tv_sec;
        ts +=  (double)currentTime.tv_usec / 1000000;
	return ts - timeStamp;
}

//Tells how much time has passed on the global timer since last set
double GetTimerLapse()
{
	gettimeofday(&currentTime,NULL);
   	currentMS = currentTime.tv_usec ;
        currentS = currentTime.tv_sec;
     	lastLapsed = (currentS - previousS);
        lastLapsed += (double)(currentMS - previousMS) / 1000000;
	return lastLapsed;
}


//Gets current GPS and heading information
WayPoint *GetCurrentLocation()
{
	WayPoint *wp = new WayPoint;

	wp->lat = gps->GetLat();
        wp->lng = gps->GetLong();
        wp->alt = gps->GetAlt();
        wp->heading = magHeading -> GetHeading();

	return wp;
	
}

//A basic function for logging, simply uses STOUT for now
void Logger(const char* function, const char* toLog)
{
        cout << function << "(): " << toLog << endl;
}

//Main OLED display function
void DisplayOLED()
{
	//Only update screen every .3 seconds
	if(GetLapsedTime(lastScreenUpdate) > .3)
	{
	cout.precision( 10 );

	double lat = gps->GetLat();
	double lng = gps->GetLong();
	double alt = gps->GetAlt();

	double lastTraveled = fabs(gps->DistanceBetween(lat,lng,gps->previousLat,gps->previousLong));

	if(lastTraveled >= GPSINCHESDEADBAND)
		distanceTraveled += fabs(gps->DistanceBetween(lat,lng,gps->previousLat,gps->previousLong));


	double head = magHeading-> GetHeading();
	double headGPS = gps->GetHeading();

	std::ostringstream sLat;
	std::ostringstream sLng;
	std::ostringstream sAlt;
	std::ostringstream sHeading;
	std::ostringstream sHeadingGPS;
	std::ostringstream sDistance;
	std::ostringstream sDistanceABS;

	sLat.precision(10);
	sLng.precision(10);
	sAlt.precision(10);
	sDistance.precision(10);
	sDistanceABS.precision(10);
	sHeading.precision(5);
	sHeadingGPS.precision(5);



	string d("");
	
	//Top bar is yellow, 21 chars here
	//6
	if(autoMode)
		d = "Auto   ";
	else
		d = "Manual ";
	//12
	if(macroRecordMode)
		d += "      ";
	else
		d += "RECORD";

	//Assume GPS FIXED
	d += "  FIXED";	

	d += "\n\n";

	sLat << lat;
//	sLat << magHeading->fx;
cout << "x: " << magHeading->fx;
	d += "LAT: ";
	d += sLat.str();
	d += "\n";

	sLng << lng;
//	sLng << magHeading->fy;
cout <<"y: " <<  magHeading->fy;

	d += "LNG: ";
	d += sLng.str();
	d += "\n";
        
	sAlt << alt;
//	sAlt << magHeading->fz;
cout <<"z: " <<  magHeading->fz;

	d += "ALT: ";
	d += sAlt.str();
	d += "\n"; 

	sHeading << head;
	d += "Head: ";
	d += sHeading.str();
	d += "\n";

        sHeadingGPS << headGPS;
        d += "Head: ";
        d += sHeadingGPS.str();
        d += "\n";


	
	sDistance << distanceTraveled;
	d += "Distance: ";
	d += sDistance.str();

		
	screen.WriteText(d);
	cout << d << endl;
	//cout << "course valid: " << gps->tinyGPS.course.isValid() << endl;
	lastScreenUpdate = GetTimeStamp();	
	}
	
	


}




//Another anciallary logger function, dumps to STDOUT
void Logger(const char* function, double speed)
{
        cout << function << ": " << speed << " loop/sec" << endl;
}

//Save the waypoints that were collected
bool SaveWayPoints(WayPoint *toSave)
{
	fstream oFile;
	oFile.open("/home/pi/waypoints/waypoints.txt",std::fstream::out);  	
	for(int i=0;i<recordCounter;i++)
	{
		oFile << toSave[i].lat << ";";
                oFile << toSave[i].lng << ";";
                oFile << toSave[i].alt << ";";
		oFile << toSave[i].heading;
		oFile << endl;

	}

	oFile.close();
	return true;
}



//an absolute direction move that is relative to the quadcopter orientation
//Allows the ControlSwitch to use the defaults speed
int MakeControlByte(int forward,int reverse,int left,int right,int climb, int dive, int rright,int rleft)
{
        unsigned int cb = 0;
        unsigned int m = 0;
        if(forward)
                cb = cb | 1;
        cb = cb << 1;
        if(reverse)
                cb = cb | 1;
        cb = cb << 1;
        if(left)
                cb = cb | 1;
        cb = cb << 1;
        if(right)
                cb = cb | 1;
        cb = cb << 1;
        if(climb)
                cb = cb | 1;
        cb = cb << 1;
        if(dive)
                cb = cb | 1;
        cb = cb << 1;
        if(rright)
                cb = cb | 1;
        cb = cb << 1;
        if(rleft)
                cb = cb | 1;
        m =  cb % 17;
        cb = cb << 8;
        cb = cb | m;


}


//This function is to handle errors, however I choose to do that eventually
void ErrorOut(int code)
{
	switch(code)
	{
		case ERR_HEARTBEAT: break;
		case ERR_CONTROLBYTE: break;
		default: break;
	}


}


//Main startup function, sets up devices, and I2c.
int Setup()
{
        
	string d("");

	Logger("setup",VERSION);
       wiringPiSetup();
       Logger("setup","Initializing pins");
       pinMode(PIAUTOMODE, INPUT);
       pinMode(PIMACRORECORD, INPUT);
	pullUpDnControl     (PIAUTOMODE,PUD_DOWN);
	pullUpDnControl     (PIMACRORECORD,PUD_DOWN);
	
	Logger("setup","Starting I2C");
	//Setup I2C
	controlSwitch = wiringPiI2CSetup(I2C_CONTROLSWITCH_ID);
	sensorArray = wiringPiI2CSetup(I2C_SENSORARRAY_ID);
	memsBoard = wiringPiI2CSetup(I2C_MEMS_ID);
	Logger("setup","Starting GPS");
	gps = new GPS();
	gps->Initialize();
	gps->Start();



        d = "QUADCOP ";
        d += VERSION;
	d += "\n\n";
	d += "INITIALIZING\n\n";
	

	screen.WriteText(d);

	float lng, lat;

	lng = gps->GetLong();
	lat = gps->GetLat();

	int c = 0;
/*
	while(lng ==0 && lat == 0)
	{
		usleep(1000000);
	        lng = gps->GetLong();
        	lat = gps->GetLat();
	        d = "QUADCOP ";

	        d += VERSION;
	        d += "\n\n";
       	 	d += "INITIALIZING GPS \n\n";
		d += c++;

		screen.WriteText(d);
	} 
*/
	bootup = GetTimeStamp();

        d = "QUADCOP ";
        d += VERSION;
        d += "\n\n";
        d += "INITIALIZING\n\n";

	magHeading = new Heading(HEADINGADDRESS);
       int t = magHeading -> Initialize();

 while(t < 0)
        {
                usleep(1000000);
		t = magHeading -> Initialize();
                d = "QUADCOP ";

                d += VERSION;
                d += "\n\n";
                d += "INITIALIZING MAG \n\n";
                d += c++;

                screen.WriteText(d);
        }
        d = "QUADCOP ";
        d += VERSION;
        d += "\n\n";
        d += "Warming up GPS\n\n";

	long l;






	while(l<10)
	{
	  	std::ostringstream sL;
       		d = "QUADCOP ";
		d += VERSION;
       		d += "\n\n";
        	d += "Warming up GPS\n\n";
		l = GetLapsedTime(bootup);

		sL << (10-l);

		d+= sL.str();
		screen.WriteText(d);	
		cout << d << endl;
		usleep(1000000);


	}

	gps->CalibrateAltitude();

	

}

//Splits a string by a deliminator
int split(string toSplit,char delim,string *r)
{
	int l = toSplit.length();
	int i = 0;
	int j = 0;

	while(i < l && j <10)
	{
		if(toSplit[i] != delim)
			r[j] += toSplit[i];
		else
			j++;	
		i++;
	}
	return j+1;
}


//Load the waypoints from the standard location
bool LoadMacro()
{
	
/*	recordWayPoints = new WayPoint[1];
	recordWayPoints = GetCurrentLocation();
	recordCounter = 1;
	return true; */
	


	ifstream iFile("/home/pi/waypoints/waypoints.txt");
	string *r = new string[10];
	int i = 0;
	string t;
	string *a;
	if(iFile.is_open())
	{
		recordWayPoints = new WayPoint[MAXRECORDWAYPOINTS];
        	while(getline(iFile,t))
		{
			cout << "." << endl;
			if(t.length() > 0)
				cout << split(t,';',r) << endl;
			i++;
			
		}
        }
	else
		cout << "ERROR" << endl;
	recordCounter = i;

        iFile.close();
        return true;

}



//Checks the Automode pin which is connected to the control switch
inline void GetAutoMode()
{
	//autoMode = true;
	//return;
	if(digitalRead(PIAUTOMODE) == HIGH)
		autoMode = true;
	else
		autoMode = false;


}

//Checks the macromode pin which is connected to the control switch
inline void GetMacroMode()
{
	//Cant go into macro mode while in automode
	//macroRecordMode = true;
        if(digitalRead(PIMACRORECORD) == HIGH && !autoMode)
                macroRecordMode = true;
        else
                macroRecordMode = false;


}


//Calculates the LOOP speed, not quad speed.  used for optimization
inline double CalculateSpeed(long lapsed, int loops)
{
	double r;
	double secs;


	r = (double)loops / lapsed;
	return r;
}


//Sends a heatbeat check to the control switch
bool CheckHeartBeat()
{
	return true;
	//Send the heartbeack value and see what we get back
	int r;

	if(GetLapsedTime(lastHeartBeat) >=  3)
        {
       		Logger("Manual Heartbeat","Checking control switch heartbeat");
		r = wiringPiI2CWriteReg16 (controlSwitch,I2C_HEARTBEAT_REGISTER, I2C_HEARTBEAT_VALUE);
		if(r != 0)
		{               		
			Logger("Manual Heartbeat","Heartbeat check failed, trying again");
                        sleep(1);
		 	r = wiringPiI2CWriteReg16 (controlSwitch,I2C_HEARTBEAT_REGISTER, I2C_HEARTBEAT_VALUE);
			if(r != 0)
				return false;
		}
               	else
		{
                	Logger("Manual Heartbeat","Heart Beat Check Good");
                        lastHeartBeat = GetTimeStamp();
                }
	}

		return true;
}


//Sends a controlbyte to stop all motion 
//Note:  It does not shut off motors since the quadCOP be hovering.
void AllStop()
{
	SendControlByte(MakeControlByte(0,0,0,0,0,0,0,0));

}


//This function determines which direction we need to rotate to get to the desired heading
//This function does not set the heading, but set vars that are used in the main loop
//The ratation is then combinded with other motiion
bool SetHeadingRequest(double toHeading)
{
	if(!magHeading->HeadingReached(toHeading))
	{
		double heading = gps->GetHeading();
		requestRotateRight = false;
		requestRotateLeft = false;
                if(toHeading > heading)
                {
                        if(toHeading - heading > 180)
                        {
                                requestRotateRight = false;
                                requestRotateLeft= true;
                        }
                        else
                        {
                                requestRotateRight = true;
                                requestRotateLeft = false;
                        }
                }
                else
                {
                        if(heading - toHeading > 180)
                        {
                                requestRotateRight = true;
                                requestRotateLeft= false;
                        }
                        else
                        {
                                requestRotateRight = false;
                                requestRotateLeft= true;
                        }
                }
        }
}

//This functions stops all motion and sets the heading
bool  SetHeading(double toHeading)
{
	double start = GetTimeStamp();
	double heading = gps->GetHeading();
	bool rotateRight = true;
	//Determine which way to rotate
	while(!magHeading->HeadingReached(toHeading) && GetLapsedTime(start) < 3)
	{
		nextControlByte = 0;

		if(toHeading > heading)
                {
                    //Need to rotate right, but left may be quicker
                        if(toHeading - heading > 180)
                        {
                                rotateRight = false;
                                
                        }
                        else
                        {
                                rotateRight = true;
                                
                        }
                }
                else
                {
                        if(heading - toHeading > 180)
                        {
                                rotateRight = true;
                        }
                        else
                        {
                                rotateRight = false;
                        }
                }	
		if(rotateRight)
		{
			nextControlByte = currentControlByte;
			nextControlByte >>= 2;
			nextControlByte <<= 2;
			nextControlByte |= 1;

		}
		else
		{
                        nextControlByte = currentControlByte;
                        nextControlByte >>= 2;
                        nextControlByte <<= 2;
                        nextControlByte |= 2;

		}
		SendControlByte(nextControlByte);

	}
	if(!magHeading->HeadingReached(toHeading))
		return true;
	else
		return false;

}


//This function checks the altitude then sets vars that are used in the main loop
//the climb or dive is combined with other needed motions.
bool CheckAltitude()
{
	double alt = gps->GetAlt();
	if(alt < 0)
		gps -> CalibrateAltitude();

	alt = gps->GetAlt();

	if(alt < minAlt)
		climbRequest = true;	
	else
		climbRequest = false;
	
	
	if(alt > maxAlt)
		diveRequest = true;
	else
		diveRequest = false;
	
	return climbRequest || diveRequest;
}



//a temp prototype for moving to a waypoint, but actually tries to hold it.
 
bool HoldWayPoint(WayPoint *wp)
{
	if(gps->WayPointReached(wp))
		return true;
	else
		return false;
}


//A Generic function that sends a command to the Sensor Array and returns result
int SendSensorCommand(int command,int param)
{
	int data[1];
	bool error = false;
	
	//send the command useing the
	if(!SendBlock(command,data,1))
		if(!SendBlock(command,data,1))
			error = true;

	return 0;	
	
		
}


//Main Loop that never ends.
int main(void)
{
	Setup();
	Logger("main","Starting main control loop");
	StartTimer();
	GetAutoMode();

	if(autoMode)
		cout << "Entering Auto Mode" << endl;
	else
		cout << "Entering Manual Mode" << endl;
	
	while(1)
	{
		DisplayOLED();

		//if not in automode or macro mode, the computer just waits or records macros when told as we assume manual control mode.
		//A heartbeat pulse is read through I2c every 3 seconds though.
		loopCounter++;

		//Every 20 seconds a bench mark is logged.
		if(GetTimerLapse() >= 20 )
		{
			Logger("main loop operating at ", CalculateSpeed(lastLapsed,loopCounter));
			loopCounter = 0;	
			StartTimer();
				
		} 

		//Anything outside these two inner wile loops is manual control	
		//Manual control here.

		

		
		GetAutoMode();
		GetMacroMode();
		CheckHeartBeat();

		//This inner loop is for autocontrol mode and the RPFS is flying the quad.
		while(autoMode)
		{
			DisplayOLED();
			if(!autoModeInProgress)
			{
				Logger("AutoLoop","Entering auto flight mode");
				autoModeInProgress = true;

				/*
				if(wayPoints == NULL)
				{
						
					Logger("AutoLoop","load waypoint macros");
					LoadMacro();
					cout << recordCounter << " waypoints loaded" << endl;
					currentWayPoint = 0;
				}
				else
					cout << "Resuming ways points " << endl;
				*/

				//hard coded waypoint here
				//Waypoints consist of current location only

				if(wayPoints == NULL)
					wayPoints = new WayPoint[1];
				wayPoints[0].lng = gps->GetLong();
				wayPoints[0].lat = gps->GetLat();
				wayPoints[0].alt = gps->GetAlt();



			}

			//HARD CODED var here for testing
			// will be removed

			holdWayPoint = true;
			if(holdWayPoint)
			{
				if(!HoldWayPoint(&wayPoints[0]))
				{
					SetHeading(wayPoints[0].heading);
					CheckAltitude();
					if(SendControlByte(MakeControlByte(true,false,false,false,climbRequest, diveRequest, false,false)) < 0)
					{
						SendControlByte(MakeControlByte(true,false,false,false,climbRequest, diveRequest, false,false));
					}

				}
			}
			//fly the Quad here
			/*
			if(gps->WayPointReached(&wayPoints[currentWayPoint]))
			{
				currentWayPoint++;
				currentWayPoint %= recordCounter;
			}
			else
			{
				//SetHeading(wayPoints[currentWayPoint].heading);

			}	
			*/
			

			 GetAutoMode();

			
		}
		if(autoModeInProgress)
		{
			autoModeInProgress = false;
			Logger("AutoLoop","Exiting auto flight mode");
		}	

		//This inner loop is still for manual control, but here the RPFS is recording waypoint macros.

		while(macroRecordMode)
		{
                       DisplayOLED();

			if(!macroInProgress)
			{
				Logger("MacroRecordLoop","Entering macro record mode");
				macroInProgress = true;
				lastMacroRead = GetTimeStamp();
				recordWayPoints = new WayPoint[MAXRECORDWAYPOINTS];
				recordCounter = 0;
			}
			//Record Macros Here
			//Record a macro every MACROREADPERIOD (in seconds)
			if(GetLapsedTime(lastMacroRead) > MACROREADPERIOD)
			{
				recordWayPoints[recordCounter].lat = gps->GetLat();
                                recordWayPoints[recordCounter].lng = gps->GetLong();
                                recordWayPoints[recordCounter].alt = gps->GetAlt();
				recordWayPoints[recordCounter].heading = gps->GetHeading();
				recordCounter++;
				//Here we ensure no buffer overflow
				//If we fill up the buffer the buffer starts getting overwritten at the beginning
				//Need work here to inform the user and stop recording.
				if(recordCounter >= MAXRECORDWAYPOINTS)
				{	
					recordCounter %= MAXRECORDWAYPOINTS;
					cout << "WAYPOINT RECORD LOOOPING" << endl;
				}
				lastMacroRead = GetTimeStamp();
			}

			
			GetAutoMode();
			GetMacroMode();
			CheckHeartBeat();
		}
		if(macroInProgress)
		{
			macroInProgress = false;
			Logger("MacroRecordLoop","Exiting macro record mode");
			SaveWayPoints(recordWayPoints);	
			delete recordWayPoints;
		}
	}	
	return 0;
}


