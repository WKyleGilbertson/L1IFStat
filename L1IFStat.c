#pragma comment(lib, "lib/FTD2XX.lib")
#include "inc/FTD2XX.h"
#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

FT_STATUS ftStatus;
WORD i = 0;

bool testBadCommand(FT_HANDLE ftH, BYTE cmd)
{
/* This should be a function!*/
  //////////////////////////////////////////////////////////////////
	// Synchronize the MPSSE interface by sending bad command ¡®0xAA¡¯
	//////////////////////////////////////////////////////////////////
  // Enable internal loop-back AN 135 Section 5.3.1
  bool retVal = false;
  FT_STATUS ftS;
  DWORD BTS = 0;  // Bytes to send
  DWORD NBS = 0;  // Number of bytes sent
  DWORD NBR = 0;  // Number of bytes read
  DWORD BTR = 0;  // Bytes to read
  BYTE oBuff[64];
  BYTE iBuff[64];
  BYTE idx = 0;

  BTS = 0;
  oBuff[BTS++] = 0x84; // Enable loopback
  ftS = FT_Write(ftH, oBuff, BTS, &NBS);
  NBS = 0;
  ftS = FT_GetQueueStatus(ftH, &NBR);
  if (NBR != 0) {
    fprintf(stderr, "Error - MPSSE receive buffer should be empty\n", ftS);
    FT_SetBitMode(ftH, 0x00, 0x00);
    FT_Close(ftH);
    return 1;
  }
  else {
//    fprintf(stderr, "Loopback Enabled\n");
  }

	BTS = 0;
	oBuff[BTS++] = cmd;		//Add BAD command ¡®0xAA¡¯
	ftS = FT_Write(ftH, oBuff, BTS, &NBS);	// Send off the BAD commands
//	fprintf(stderr, "Bytes Sent: %d\n", NBS);
	BTS = 0;  //Clear output buffer
	do{
		ftS = FT_GetQueueStatus(ftH, &BTR);	 // Get the number of bytes in the device input buffer
	} while ((BTR == 0) && (ftS == FT_OK));   	//or Timeout

	ftS = FT_Read(ftH, iBuff, BTR, &NBR);  //Read out the data from input buffer
	for (idx = 0; idx < (NBR - 1); idx++)	//Check if Bad command and echo command received
	{
		if ((iBuff[idx] == 0xFA) && (iBuff[idx + 1] == cmd))
		{
			retVal = true;
			break;
		}
	}
	if (retVal == false)
	{
		fprintf(stderr, "fail to synchronize MPSSE with command '0xAA' \n");
		//Error, can¡¯t receive echo command , fail to synchronize MPSSE interface;
	}
  else {
//    fprintf(stderr, "Bad command 0x%.2X was Echoed\n", cmd);
  }
  // Disable internal loop-back
  BTS = 0;
  oBuff[BTS++] = 0x85; // Disable loopback
  ftS = FT_Write(ftH, oBuff, BTS, &NBS);
  BTS = 0;
  ftS = FT_GetQueueStatus(ftH, &NBR);
  if (NBR!= 0) {
    fprintf(stderr, "Error - MPSSE receive buffer should be empty\n", ftS);
    FT_SetBitMode(ftH, 0x00, 0x00);
    FT_Close(ftH);
    retVal = false;
    //return 1; // maybe this should be exit(0)?
  }
  else {
//    fprintf(stderr, "Loopback Disabled\n");
  }
/* End of testing bad command part, 5.3.1*/
return retVal;
}

int main()
{
  FT_HANDLE ftdiHandle;
  BYTE OutputBuffer[64]; // Buffer to hold MPSSE commands sent to FT2232H
  BYTE InputBuffer[64];  // Buffer to hold Data bytes read from FT2232H
//  DWORD dwClockDivisor = 29;      // Value of clock divisor, SCL frequency...
  DWORD dwNumBytesToSend = 0;     // Index of output buffer
  DWORD dwNumBytesToRead = 0;    
  DWORD dwNumBytesSent = 0, dwNumBytesRead = 0, dwNumInputBuffer = 0;
  DWORD dwCount = 0;
// SCL frequency = 60/((1+29)*2) (MHz) = 1 MHz
//	bool bCommandEchod = false;
  bool antennaConnected = false;
  DWORD numDevs;
  FT_DEVICE_LIST_INFO_NODE *devInfo;

  ftStatus = FT_CreateDeviceInfoList(&numDevs);   // AN_135 4.1 Step 1
  if (ftStatus == FT_OK) 
    printf("Number of devices: %.1d\n", numDevs);
  else
  {
    printf("No Devices\n");
    return 1;
}
  if (numDevs > 0) { // allocate storage for list based on numDevs
    devInfo =
    (FT_DEVICE_LIST_INFO_NODE*)malloc(sizeof(FT_DEVICE_LIST_INFO_NODE)*numDevs);
// get the device information list
    ftStatus = FT_GetDeviceInfoList(devInfo, &numDevs); // AN_135 4.1 Step 2
   if (ftStatus == FT_OK) {
   for (i = 0; i < numDevs; i++) {
    printf("Dev %d:\n", i);
    printf("  Flags=0x%x\n", devInfo[i].Flags);
    printf("  Type=0x%x\n", devInfo[i].Type);
    printf("  ID=0x%x\n", devInfo[i].ID);
    printf("  LocId=0x%x\n", devInfo[i].LocId);
    printf("  SerialNumber=%s\n", devInfo[i].SerialNumber);
    printf("  Description=%s\n", devInfo[i].Description);
    printf("  ftHandle=0x%x\n", (DWORD) devInfo[i].ftHandle);
   }
  }
 }
  else {
    printf("No Devices\n");
    return 1;
  }

	ftStatus = FT_OpenEx("USB<->GPS B", FT_OPEN_BY_DESCRIPTION, &ftdiHandle);
	if (ftStatus != FT_OK) // AN_135 4.1 Step 3
	{
		printf("Can't open FT2232H device! \n");
		return 1;
	}
	else {  // Port opened successfully
		printf("Successfully opened FT2232H device! \n");
  }
  /* AN 135 Section 4.2*/
  ftStatus = FT_ResetDevice(ftdiHandle); // Reset peripheral side of FTDI port 
  //ftStatus |= FT_SetUSBParameters(ftHandle, 65535, 65535);	//Set USB request transfer size
  ftStatus |= FT_SetUSBParameters(ftdiHandle, 64, 64);	//Set USB request transfer size
  ftStatus |= FT_SetChars(ftdiHandle, FALSE, 0, FALSE, 0);	 //Disable event and error characters
  ftStatus |= FT_SetTimeouts(ftdiHandle, 3000, 3000);		//Sets the read and write timeouts in milliseconds for the FT2232H
  ftStatus |= FT_SetLatencyTimer(ftdiHandle, 1);		//Set the latency timer
  ftStatus |= FT_SetFlowControl(ftdiHandle, FT_FLOW_RTS_CTS, 0x11, 0x13);
	ftStatus |= FT_SetBitMode(ftdiHandle, 0x0, 0x00); 		//Reset controller
	ftStatus |= FT_SetBitMode(ftdiHandle, 0x0, 0x02);	 	//Enable MPSSE mode
	if (ftStatus != FT_OK)
	{
		printf("fail on initialize FT2232H device ! \n");
		return 1;
	}
  else{
    printf("AN 135 Section 4.2 complete\n");
  }
	Sleep(50);	// Wait for all the USB stuff to complete and work

  //////////////////////////////////////////////////////////////////
	// Synchronize the MPSSE interface by sending bad command ¡®0xAA¡¯
	//////////////////////////////////////////////////////////////////
  printf("Bad command 0xAA was echoed? %s\n",
   (testBadCommand(ftdiHandle, 0xAA) == true) ? "yes" : "no" );
  printf("Bad command 0xAB was echoed? %s\n",
   (testBadCommand(ftdiHandle, 0xAB) == true) ? "yes" : "no" );

  /*
  //////////////////////////////////////////////////////////////////
	// Synchronize the MPSSE interface by sending bad command ¡®0xAA¡¯
	//////////////////////////////////////////////////////////////////
  // Enable internal loop-back AN 135 Section 5.3.1
  dwNumBytesToSend = 0;
  OutputBuffer[dwNumBytesToSend++] = 0x84; // Enable loopback
  ftStatus = FT_Write(ftdiHandle, OutputBuffer, dwNumBytesToSend, &dwNumBytesSent);
  dwNumBytesToSend = 0;
  ftStatus = FT_GetQueueStatus(ftdiHandle, &dwNumBytesRead);
  if (dwNumBytesRead != 0) {
    printf("Error - MPSSE receive buffer should be empty\n", ftStatus);
    FT_SetBitMode(ftdiHandle, 0x00, 0x00);
    FT_Close(ftdiHandle);
    return 1;
  }
  else {
    printf("Loopback Enabled\n");
  }

	dwNumBytesToSend = 0;
	OutputBuffer[dwNumBytesToSend++] = 0xAA;		//Add BAD command ¡®0xAA¡¯
	ftStatus = FT_Write(ftdiHandle, OutputBuffer, dwNumBytesToSend, &dwNumBytesSent);	// Send off the BAD commands
	printf("Bytes Sent: %d\n", dwNumBytesSent);
	dwNumBytesToSend = 0;			//Clear output buffer
	do{
		ftStatus = FT_GetQueueStatus(ftdiHandle, &dwNumBytesToRead);	 // Get the number of bytes in the device input buffer
	} while ((dwNumBytesToRead == 0) && (ftStatus == FT_OK));   	//or Timeout

	ftStatus = FT_Read(ftdiHandle, InputBuffer, dwNumBytesToRead, &dwNumBytesRead);  //Read out the data from input buffer
	for (dwCount = 0; dwCount < (dwNumBytesRead - 1); dwCount++)	//Check if Bad command and echo command received
	{
		if ((InputBuffer[dwCount] == 0xFA) && (InputBuffer[dwCount + 1] == 0xAA))
		{
			bCommandEchod = true;
			break;
		}
	}
	if (bCommandEchod == false)
	{
		printf("fail to synchronize MPSSE with command '0xAA' \n");
		return false; //Error, can¡¯t receive echo command , fail to synchronize MPSSE interface;
	}
  else {
    printf("Bad command 0xAA was Echoed\n");
    bCommandEchod = false;
  }

	//////////////////////////////////////////////////////////////////
	// Synchronize the MPSSE interface by sending bad command ¡®0xAB¡¯
	//////////////////////////////////////////////////////////////////
	dwNumBytesToSend = 0;			//Clear output buffer
	OutputBuffer[dwNumBytesToSend++] = 0xAB;	//Send BAD command ¡®0xAB¡¯
	ftStatus = FT_Write(ftdiHandle, OutputBuffer, dwNumBytesToSend, &dwNumBytesSent);	// Send off the BAD commands
  printf("Bytes Sent: %d\n", dwNumBytesSent);
	dwNumBytesToSend = 0;			//Clear output buffer
	do{
		ftStatus = FT_GetQueueStatus(ftdiHandle, &dwNumInputBuffer);	//Get the number of bytes in the device input buffer
	} while ((dwNumInputBuffer == 0) && (ftStatus == FT_OK));   //or Timeout
	bCommandEchod = false;
	ftStatus = FT_Read(ftdiHandle, InputBuffer, dwNumInputBuffer, &dwNumBytesRead);  //Read out the data from input buffer
	for (dwCount = 0; dwCount < (dwNumBytesRead - 1); dwCount++)	//Check if Bad command and echo command received
	{
		if ((InputBuffer[dwCount] == 0xFA) && (InputBuffer[dwCount + 1] == 0xAB))
		{
			bCommandEchod = true;
			break;
		}
	}
	if (bCommandEchod == false)
	{
		printf("fail to synchronize MPSSE with command '0xAB' \n");
		return false;
		//Error, can't receive echo command , fail to synchronize MPSSE interface;
	}
  else {
      printf("Bad command 0xAB was echoed\n");
      bCommandEchod = false;
  }
  // Disable internal loop-back
  dwNumBytesToSend = 0;
  OutputBuffer[dwNumBytesToSend++] = 0x85; // Disable loopback
  ftStatus = FT_Write(ftdiHandle, OutputBuffer, dwNumBytesToSend, &dwNumBytesSent);
  dwNumBytesToSend = 0;
  ftStatus = FT_GetQueueStatus(ftdiHandle, &dwNumBytesRead);
  if (dwNumBytesRead != 0) {
    printf("Error - MPSSE receive buffer should be empty\n", ftStatus);
    FT_SetBitMode(ftdiHandle, 0x00, 0x00);
    FT_Close(ftdiHandle);
    return 1;
  }
  else {
    printf("Loopback Disabled\n");
  }
*/
/* End of testing bad command part, 5.3.1*/

/* Configure SPI here. Do I need to? */

/* Now READ the GPIO to See if Antenna is attached*/
// Change scope trigger to channel 4 (TMS/CS) falling edge
dwNumBytesToSend = 0;
OutputBuffer[dwNumBytesToSend++] = 0x81;
// Get data bits - returns state of pins,
// either input or output
// on low byte of MPSSE
ftStatus = FT_Write(ftdiHandle, OutputBuffer, dwNumBytesToSend, &dwNumBytesSent);
// Read the low GPIO byte
dwNumBytesToSend = 0; // Reset output buffer pointer
Sleep(2); // Wait for data to be transmitted and status
// to be returned by the device driver
// - see latency timer above
// Check the receive buffer - there should be one byte
ftStatus = FT_GetQueueStatus(ftdiHandle, &dwNumBytesToRead);
// Get the number of bytes in the
// FT2232H receive buffer
ftStatus |= FT_Read(ftdiHandle, &InputBuffer, dwNumBytesToRead, &dwNumBytesRead);
if ((ftStatus != FT_OK) & (dwNumBytesToRead != 1))
{
printf("Error - GPIO cannot be read\n");
FT_SetBitMode(ftdiHandle, 0x0, 0x00);
// Reset the port to disable MPSSE
FT_Close(ftdiHandle); // Close the USB port
return 1; // Exit with error
}
printf("The GPIO low-byte = 0x%X\n", InputBuffer[0]);
antennaConnected = ((InputBuffer[0] & 0x20) >> 5) == 1 ? true : false; 
printf("Antenna connected? %s\n", (antennaConnected) == true ? "yes" : "no");
// The inpute buffer only contains one
// valid byte at location 0
printf("Press <Enter> to continue\n");
getchar(); // wait for a carriage return
// Modify the GPIO data (TMS/CS only) and write it back
/* End GPIO Read*/

FT_SetBitMode(ftdiHandle, 0x00, 0x00);  // Reset MPSSE
FT_Close(ftdiHandle);                   // Close Port
free(devInfo);
}