#pragma comment(lib, "lib/FTD2XX.lib")
#include "inc/FTD2XX.h"
#include <windows.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

FT_STATUS ftStatus;
WORD L1IFStat;
WORD i = 0;

bool testBadCommand(FT_HANDLE ftH, BYTE cmd)
{
  //////////////////////////////////////////////////////////////////
	// Synchronize the MPSSE interface by sending bad command ¡®0xAA¡¯
	//////////////////////////////////////////////////////////////////
  // Enable internal loop-back AN 135 Section 5.3.1
  bool retVal = false;
  FT_STATUS ftS;
  DWORD BTS = 0;  // Bytes to send
  DWORD BTR = 0;  // Bytes to read
  DWORD NBS = 0;  // Number of bytes sent
  DWORD NBR = 0;  // Number of bytes read
  BYTE oBuff[64];
  BYTE iBuff[64];
  BYTE idx = 0;

  BTS = 0;
  oBuff[BTS++] = 0x84; // Enable loopback AN 108 3.7
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
  oBuff[BTS++] = 0x85; // Disable loopback AN 108 3.7
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
/* End of testing bad command AN 135 5.3.1*/
return retVal;
}

//BYTE readGPIObyte(ftdiHandle, 0){ // Low byte = 0, High byte = 1
BYTE readGPIObyte(FT_HANDLE ftH, BYTE lhB){ // Low byte = 0, High byte = 1
  BYTE retVal = 0x00;
  FT_STATUS ftS;
  DWORD BTS = 0;  // Bytes to send
  DWORD BTR = 0;  // Bytes to read
  DWORD NBS = 0;  // Number of bytes sent
  DWORD NBR = 0;  // Number of bytes read
  BYTE opCode = 0;// FTDI OPCODE
  BYTE oBuff[64];
  BYTE iBuff[64];
  BYTE idx = 0;
// Change scope trigger to channel 4 (TMS/CS) falling edge
BTS = 0;
opCode = (lhB == 0) ? 0x81 : 0x83; 
//oBuff[BTS++] = 0x81; // AN 108 3.6
oBuff[BTS++] = opCode; // AN 108 3.6
// Get data bits - returns state of pins, either input or output
// on low byte of MPSSE
ftS = FT_Write(ftH, oBuff, BTS, &NBS);
// Read the low GPIO byte
BTS = 0; // Reset output buffer pointer
Sleep(2); // Wait for data to be transmitted and status to be returned 
// by the device driver - see latency timer above
// Check the receive buffer - there should be one byte
ftS = FT_GetQueueStatus(ftH, &BTR);
// Get the number of bytes in the FT2232H receive buffer
ftS |= FT_Read(ftH, &iBuff, BTR, &NBR);
if ((ftStatus != FT_OK) & (BTR != 1))
{
fprintf(stderr, "Error - GPIO cannot be read\n");
FT_SetBitMode(ftH, 0x0, 0x00); // Reset the port to disable MPSSE
FT_Close(ftH);                 // Close the USB port
//return 1;                             // Exit with error
exit(1);                             // Exit with error
}
else {

retVal = iBuff[0]; 

if (lhB == 0) {
L1IFStat |= retVal & 0x00FF;
}
else {
L1IFStat |= ((retVal & 0x00FF) << 8) & 0xFF00;
}
fprintf(stderr, "The GPIO low-byte = 0x%X\n", retVal);
//antennaConnected = ((InputBuffer[0] & 0x20) >> 5) == 1 ? true : false; 
//printf("Antenna connected? %s\n", (antennaConnected) == true ? "yes" : "no");
// The input buffer only contains one valid byte at location 0
return retVal;
/* End GPIO Read*/
}
}

void displayDevInfo(FT_DEVICE_LIST_INFO_NODE * dInfo, DWORD numD) {
  WORD i;
  for (i = 0; i<numD; i++) {
    printf("Dev %d:\n", i);
    printf("  Flags=0x%x\n", dInfo[i].Flags);
    printf("  Type=0x%x\n", dInfo[i].Type);
    printf("  ID=0x%x\n", dInfo[i].ID);
    printf("  LocId=0x%x\n", dInfo[i].LocId);
    printf("  SerialNumber=%s\n", dInfo[i].SerialNumber);
    printf("  Description=%s\n", dInfo[i].Description);
    printf("  ftHandle=0x%x\n", (DWORD) dInfo[i].ftHandle);
  }
}

int main()
{
  FT_HANDLE ftdiHandle;
  BYTE OutputBuffer[64]; // Buffer to hold MPSSE commands sent to FT2232H
  BYTE InputBuffer[64];  // Buffer to hold Data bytes read from FT2232H
  BYTE GPIOdata = 0;
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
   displayDevInfo(devInfo, numDevs);
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
//    fprintf(stderr, "AN 135 Section 4.2 complete\n");
  }
	Sleep(50);	// Wait for all the USB stuff to complete and work

  //////////////////////////////////////////////////////////////////
	// Synchronize the MPSSE interface by sending bad command ¡®0xAA¡¯
	//////////////////////////////////////////////////////////////////
  printf("Bad command 0xAA was echoed? %s\n",
   (testBadCommand(ftdiHandle, 0xAA) == true) ? "yes" : "no" );
  printf("Bad command 0xAB was echoed? %s\n",
   (testBadCommand(ftdiHandle, 0xAB) == true) ? "yes" : "no" );

/* End of testing bad command part, AN 135 5.3.1*/

/* Configure SPI here. Do I need to? */

/* Now READ the GPIO to See if Antenna is attached*/
/* Write function bool testAntennaConnected(ftdiHandle) */
GPIOdata = readGPIObyte(ftdiHandle, 0);
antennaConnected = ((L1IFStat & 0x0020) >> 5) == 1 ? true : false;
fprintf(stderr, "bit 0: %s\n", ((GPIOdata & 0x01) == 1) ? "HI" : "LO");
fprintf(stderr, "bit 1: %s\n", ((GPIOdata & 0x02) >> 1 == 1) ? "HI" : "LO");
fprintf(stderr, "bit 2: %s\n", ((GPIOdata & 0x04) >> 2 == 1) ? "HI" : "LO");
fprintf(stderr, "bit 3: %s\n", ((GPIOdata & 0x08) >> 3 == 1) ? "HI" : "LO");
fprintf(stderr, "bit 4: %s\n", ((GPIOdata & 0x10) >> 4 == 1) ? "HI" : "LO");
fprintf(stderr, "bit 5 (Ant): %s\n", ((GPIOdata & 0x20) >> 5 == 1) ? "HI" : "LO");
fprintf(stderr, "bit 6: %s\n", ((GPIOdata & 0x40) >> 6 == 1) ? "HI" : "LO");
fprintf(stderr, "bit 7: %s\n", ((GPIOdata & 0x80) >> 7 == 1) ? "HI" : "LO");

printf("Antenna connected? %s\n", (antennaConnected) == true ? "yes" : "no");
/* End GPIO Read*/

printf("Press <Enter> to continue\n");
getchar(); // wait for a carriage return

FT_SetBitMode(ftdiHandle, 0x00, 0x00);  // Reset MPSSE
FT_Close(ftdiHandle);                   // Close Port
free(devInfo);
}