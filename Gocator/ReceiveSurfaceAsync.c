/*
* ReceiveProfileASync.c
*
* Based on "ReceiveAsync" and "ReceiveProfile" sample code from LMI
* Modified in 2018 by Martin H. Skjelvareid
*
* Gocator 2000 Sample
* Copyright (C) 2011 by LMI Technologies Inc.
*
* Licensed under The MIT License.
* Redistributions of files must retain the above copyright notice.
*
* Purpose: Connect to Gocator system, receive profile data using a callback function,
* and write data to file.
*
* Requirements: Ethernet output for the desired data must be enabled.
*
* Output files have the following format:
* char[16]				headerText			(16 bytes)	(Last 4 characters indicate version number)
* uint64				timeStamp			(8 bytes)
* uint32				surfaceWidth		(4 bytes)
* uint32				surfaceLength		(4 bytes)
* float64				xOffset				(8 bytes)
* float64				xResolution			(8 bytes)
* float64				yOffset				(8 bytes)
* float64				yResolution			(8 bytes)
* float64				zOffset				(8 bytes)
* float64				zResolution			(8 bytes)
* float64				frameRate			(8 bytes)
* float64				exposureTime		(8 bytes)
* uint16				surface				(2*surfaceWidth*surfaceLength bytes)
*
* The surface is written row-by-row.
* Note that the header text version number should be updated whenever changes are made.
*
* Gocator transmits range data as 16-bit signed integers.
* To translate 16-bit range data to metric units, the calculation for each point is:
*	X: XOffset + columnIndex * XResolution
*	Y: YOffset + rowIndex * YResolution
*	Z: ZOffset + height_map[rowIndex][columnIndex] * ZResolution
*
* Invalid data (outside surface) are given the value -2^15 = -32768
*/

#include <GoSdk/GoSdk.h>
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <time.h>		// Added
#include <Windows.h>	// Added

#define RECEIVE_TIMEOUT			(20000000)
#define INVALID_RANGE_16BIT		((signed short)0x8000)			// gocator transmits range data as 16-bit signed integers. 0x8000 signifies invalid range data.
#define DOUBLE_MAX				((k64f)1.7976931348623157e+308)	// 64-bit double - largest positive value.
#define INVALID_RANGE_DOUBLE	((k64f)-DOUBLE_MAX)				// floating point value to represent invalid range data.
#define SENSOR_IP			    "192.168.1.10"

#define NM_TO_MM(VALUE) (((k64f)(VALUE))/1000000.0)
#define UM_TO_MM(VALUE) (((k64f)(VALUE))/1000.0)

#define ROOTFOLDER          "D:\\GocatorDataOutput\\"
#define DATAFILENAMESUFFIX  "GocatorSurface.bin"
#define MEASFILENAMESUFFIX  "GocatorMeasurement.txt"
#define HEADERTEXT			"MHSKJELV VER0001"
#define HEADERTEXTSIZE		16

// Define DataContext struct - used for passing data between main() and callback func.
typedef struct
{
	k32u count;						// Variable for counting surfaces (uint32)
	k64u timeStamp;					// Variable for keeping track of timestamp
	k64f frameRate;
	k64f exposureTime;
	FILE * measFilePointer;
}DataContext;

// Declare data callback function
kStatus kCall onData(void* ctx, void* sys, void* dataset);

// Main function
void main(int argc, char **argv)
{
	kAssembly api = kNULL;
	GoSystem system = kNULL;
	GoSensor sensor = kNULL;
	kStatus status;
	kIpAddress ipAddress;
	GoSetup setup = kNULL;
	DataContext contextPointer;
	k32s scanMode;

	SYSTEMTIME str_t;
	char measurementFileName[1024];      // File name buffer

	// Construct Gocator API Library
	if ((status = GoSdk_Construct(&api)) != kOK)
	{
		printf("Error: GoSdk_Construct:%d\n", status);
		return;
	}

	// Construct GoSystem object
	if ((status = GoSystem_Construct(&system, kNULL)) != kOK)
	{
		printf("Error: GoSystem_Construct:%d\n", status);
		return;
	}

	// Parse IP address into address data structure
	kIpAddress_Parse(&ipAddress, SENSOR_IP);

	// Obtain GoSensor object by sensor IP address
	if ((status = GoSystem_FindSensorByIpAddress(system, &ipAddress, &sensor)) != kOK)
	{
		printf("Error: GoSystem_FindSensor:%d\n", status);
		return;
	}

	// Create connection to GoSystem object
	if ((status = GoSystem_Connect(system)) != kOK)
	{
		printf("Error: GoSystem_Connect:%d\n", status);
		return;
	}

	// Enable sensor data channel
	if ((status = GoSystem_EnableData(system, kTRUE)) != kOK)
	{
		printf("Error: GoSensor_EnableData:%d\n", status);
		return;
	}

	// Set data handler to receive data asynchronously
	if ((status = GoSystem_SetDataHandler(system, onData, &contextPointer)) != kOK)
	{
		printf("Error: GoSystem_SetDataHandler:%d\n", status);
		return;
	}

	// Retrieve setup handle
	if ((setup = GoSensor_Setup(sensor)) == kNULL)
	{
		printf("Error: GoSensor_Setup: Invalid Handle\n");
	}

	// Reset counter
	contextPointer.count = 0;

	// Get camera settings
	contextPointer.frameRate = GoSetup_FrameRate(setup);
	contextPointer.exposureTime = GoSetup_Exposure(setup, GoSensor_Role(sensor));


	// Check that correct scan mode is used
	if ((scanMode = GoSetup_ScanMode(setup)) != GO_MODE_SURFACE)
	{
		if ((status = GoSetup_SetScanMode(setup, GO_MODE_SURFACE)) != kOK)
		{
			printf("Error: GoSetup_SetScanMode:%d\n", status);
			return;
		}
		printf("Note: Scan mode changed to \"surface\" mode. \n\n");
	}

	// Make changes visible in web browser (if any)
	GoSensor_Flush(sensor);

	// Open measurement output file (text)
	GetSystemTime(&str_t);
	snprintf(measurementFileName, sizeof measurementFileName,
		"%s%04d-%02d-%02d_%02d%02d%02d_%s",
		ROOTFOLDER,
		str_t.wYear, str_t.wMonth, str_t.wDay, str_t.wHour, str_t.wMinute, str_t.wSecond,
		MEASFILENAMESUFFIX);
	printf("Measurement output file: %s\n\n", measurementFileName);

	if ((contextPointer.measFilePointer = fopen(measurementFileName, "w")) == NULL) {
		printf("Error opening file");
		return;
	}

	// Make measurement file header
	fprintf(contextPointer.measFilePointer, "Surface number; Measurement ID; Measurement value\r\n");

	// Intro text
	printf("******** Nofima Gocator logger ********\n\n");

	// Wait  for user to start data logging
	printf("Press ENTER key to start logging data. Press ENTER again to stop.\n");
	getchar();
	printf("Waiting for surface measurements from Gocator...\n\n");

	// Start Gocator sensor
	if ((status = GoSystem_Start(system)) != kOK)
	{
		printf("Error: GoSystem_Start:%d\n", status);
		return;
	}

	// Callback function will be executed every time a surface is sent from the sensor

	// Wait for ENTER to be pressed - stop logging
	getchar();

	// stop Gocator sensor
	if ((status = GoSystem_Stop(system)) != kOK)
	{
		printf("Error: GoSystem_Stop:%d\n", status);
		return;
	}

	// Close file pointer
	fclose(contextPointer.measFilePointer);

	// Destroy handles
	GoDestroy(system);
	GoDestroy(api);

	printf("Logging stopped - %ld surfaces logged in total. Press ENTER key to close.\n", contextPointer.count);
	getchar();
	return;
}


// Data callback function
kStatus kCall onData(void* ctx, void* sys, void* dataset)
{
	DataContext *context = ctx;
	unsigned int i, j, k;
	unsigned int fwriteStatus;
	SYSTEMTIME str_t;			// Time variable
	char filename[1024];		// File name buffer
	char headerText[16];		// Header text char array
	FILE * fptr;
	GoMeasurementData *measurementData = kNULL;

	// Loop through dataset and handle different message types
	for (i = 0; i < GoDataSet_Count(dataset); ++i)
	{
		GoDataMsg dataObj = GoDataSet_At(dataset, i);
		switch (GoDataMsg_Type(dataObj))
		{
			case GO_DATA_MESSAGE_TYPE_STAMP:
			{
				GoStampMsg stampMsg = dataObj;
				for (j = 0; j < GoStampMsg_Count(stampMsg); ++j)
				{
					GoStamp *stamp = GoStampMsg_At(stampMsg, j);	// Get stamp pointer
					context->timeStamp = stamp->timestamp;			// Copy timestamp to context
				}
			}
			break;

			case GO_DATA_MESSAGE_TYPE_SURFACE:
			{
				GoSurfaceMsg surfaceMsg = dataObj;
				unsigned int rowIdx, colIdx;

				// Get information on surface size and resolution
				k32u surfaceWidth = GoSurfaceMsg_Width(surfaceMsg);
				k32u surfaceLength = GoSurfaceMsg_Length(surfaceMsg);
				double XResolution = NM_TO_MM(GoSurfaceMsg_XResolution(surfaceMsg));
				double YResolution = NM_TO_MM(GoSurfaceMsg_YResolution(surfaceMsg));
				double ZResolution = NM_TO_MM(GoSurfaceMsg_ZResolution(surfaceMsg));
				double XOffset = UM_TO_MM(GoSurfaceMsg_XOffset(surfaceMsg));
				double YOffset = UM_TO_MM(GoSurfaceMsg_YOffset(surfaceMsg));
				double ZOffset = UM_TO_MM(GoSurfaceMsg_ZOffset(surfaceMsg));

				double widthMm = surfaceWidth * XResolution;
				double lengthMm = surfaceLength * YResolution;

				// Increment counter
				context->count++;

				// Output information on received surface
				printf("Surface %ld received. Dimensions: [%1.0f, %1.0f] mm \n", context->count, widthMm, lengthMm);

				// Open binary output file
				GetSystemTime(&str_t);
				snprintf(filename, sizeof filename, "%s%04d-%02d-%02d_%02d%02d%02d_%04ld_%s",
					ROOTFOLDER,
					str_t.wYear, str_t.wMonth, str_t.wDay, str_t.wHour, str_t.wMinute, str_t.wSecond,
					context->count, DATAFILENAMESUFFIX);

				if ((fptr = fopen(filename, "wb")) == NULL) {
					printf("Error opening file");
					return;
				}

				// Write header information
				fwrite(&HEADERTEXT, sizeof(HEADERTEXT)-1, 1, fptr);						// Note: Subtract 1 to avoid including null terminator
				fwrite(&(context->timeStamp), sizeof(context->timeStamp), 1, fptr);
				fwrite(&surfaceWidth, sizeof(surfaceWidth), 1, fptr);
				fwrite(&surfaceLength, sizeof(surfaceLength), 1, fptr);
				fwrite(&XOffset, sizeof(XOffset), 1, fptr);
				fwrite(&XResolution, sizeof(XResolution), 1, fptr);
				fwrite(&YOffset, sizeof(YOffset), 1, fptr);
				fwrite(&YResolution, sizeof(YResolution), 1, fptr);
				fwrite(&ZOffset, sizeof(XOffset), 1, fptr);
				fwrite(&ZResolution, sizeof(XOffset), 1, fptr);
				fwrite(&(context->frameRate), sizeof(context->frameRate), 1, fptr);
				fwrite(&(context->exposureTime), sizeof(context->exposureTime), 1, fptr);

				// Loop through each row of surface and write to file
				for (rowIdx = 0; rowIdx < GoSurfaceMsg_Length(surfaceMsg); rowIdx++)
				{
					k16s *data = GoSurfaceMsg_RowAt(surfaceMsg, rowIdx);

					// Write profile buffer to file
					fwriteStatus = fwrite(data, surfaceWidth * sizeof(k16s), 1, fptr);
					if (fwriteStatus != 1)
					{
						printf("WARNING: Error while writing surface to file\n");
					}
				}

				// Close file
				fclose(fptr);
				printf("Surface written to file: %s\n\n", filename);
			} // case
			break;

			case GO_DATA_MESSAGE_TYPE_MEASUREMENT:
			{
				GoMeasurementMsg measurementMsg = dataObj;
				for (k = 0; k < GoMeasurementMsg_Count(measurementMsg); ++k)
				{
					measurementData = GoMeasurementMsg_At(measurementMsg, k);

					// Write measurement data to text file
					fprintf(context->measFilePointer, "%4.0u;%4.0u; %.2f\r\n", context->count, GoMeasurementMsg_Id(measurementMsg),measurementData->value);
				}
			}
			break;
		} // switch
	} // for

	// Clean up
	GoDestroy(dataset);

	return kOK;
}
