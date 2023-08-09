#pragma comment(lib, "lib/FTD2XX.lib")
#include "inc/FTD2XX.h"
#include <windows.h>
#include <stdbool.h>
//#include <stdlib.h>
//#include <stdint.h>
#include <stdio.h>

#define MLEN 64

WORD L1IFStat;
BYTE loGPIOdirection = 0xCB; // 1100 1011
BYTE loGPIOdefaults = 0xCB;  // 11xx 1x11

enum gpio
{
  IDLE = 0x40,
  SHDN = 0x80
};

typedef struct {
BYTE MSG[MLEN];
DWORD SZE;
DWORD CNT;
} PKT;

bool configureMPSSE(FT_HANDLE ftH)
{ /* AN 135 Section 4.2*/
  // AN 135 Section 5.2 "Configure FTDI Port for MPSSE Use"
  FT_STATUS ftS;
  ftS = FT_ResetDevice(ftH); // Reset peripheral side of FTDI port
  // ftStatus |= FT_SetUSBParameters(ftHandle, 65535, 65535);	//Set USB request transfer size
  ftS |= FT_SetUSBParameters(ftH, 64, 64);     // Set USB request transfer size
  ftS |= FT_SetChars(ftH, FALSE, 0, FALSE, 0); // Disable event and error characters
  ftS |= FT_SetTimeouts(ftH, 3000, 3000);      // Sets the read and write timeouts in milliseconds for the FT2232H
  ftS |= FT_SetLatencyTimer(ftH, 1);           // Set the latency timer
  ftS |= FT_SetFlowControl(ftH, FT_FLOW_RTS_CTS, 0x11, 0x13);
  ftS |= FT_SetBitMode(ftH, 0x0, 0x00); // Reset controller
  ftS |= FT_SetBitMode(ftH, 0x0, 0x02); // Enable MPSSE mode
  if (ftS != FT_OK)
  {
    fprintf(stderr, "fail on initialize FT2232H device ! \n");
    return false;
  }
  else
  {
    Sleep(50); // Wait for all the USB stuff to complete and work
  }
  return true;
} /* End AN 135 Section 4.2 */

bool testBadCommand(FT_HANDLE ftH, BYTE cmd)
{ /* AN 135 Section 5.3 "Configure the FTDI MPSSE" */
  //////////////////////////////////////////////////////////////////
  // Synchronize the MPSSE interface by sending bad command ¡®0xAA¡¯
  //////////////////////////////////////////////////////////////////
  bool retVal = false;
  FT_STATUS ftS;
  PKT tx, rx;
  BYTE idx = 0;

  // Enable internal loop-back AN 135 Section 5.3.1
  tx.SZE = 0;
  tx.MSG[tx.SZE++] = 0x84;// Enable loopback AN 108 3.7
  ftS = FT_Write(ftH, tx.MSG, tx.SZE, &tx.CNT);
  tx.SZE = 0;
  ftS = FT_GetQueueStatus(ftH, &rx.CNT);
  if (rx.CNT != 0)
  {
    fprintf(stderr, "Error - MPSSE receive buffer should be empty\n", ftS);
    FT_SetBitMode(ftH, 0x00, 0x00);
    FT_Close(ftH);
    return 1;
  }
  else
  {
    //    fprintf(stderr, "Loopback Enabled\n");
  }

  tx.SZE = 0;
  tx.MSG[tx.SZE++] = cmd;// Add BAD command ¡®0xAA¡¯
  ftS = FT_Write(ftH, tx.MSG, tx.SZE, &tx.CNT); // Send off the BAD commands
//	fprintf(stderr, "Bytes Sent: %d\n", tx.CNT);
  tx.SZE = 0;// Clear output buffer
  do
  {
    ftS = FT_GetQueueStatus(ftH, &rx.SZE);   // Get the number of bytes in the device input buffer
  } while ((rx.SZE == 0) && (ftS == FT_OK)); // or Timeout

  ftS = FT_Read(ftH, rx.MSG, rx.SZE, &rx.CNT); // Read out the data from input buffer
  for (idx = 0; idx < (rx.CNT - 1); idx++) // Check if Bad command and echo command received
  {
    if ((rx.MSG[idx] == 0xFA) && (rx.MSG[idx + 1] == cmd))
    {
      retVal = true;
      break;
    }
  }
  if (retVal == false)
  {
    fprintf(stderr, "fail to synchronize MPSSE with command '0x%.2X' \n", cmd);
    // Error, can¡¯t receive echo command , fail to synchronize MPSSE interface;
  }
  else
  {
    //    fprintf(stderr, "Bad command 0x%.2X was Echoed\n", cmd);
  }

  // Disable internal loop-back
  tx.SZE = 0;
  tx.MSG[tx.SZE++] = 0x85; // Disable loopback AN 108 3.7
  ftS = FT_Write(ftH, tx.MSG, tx.SZE, &tx.CNT);
  tx.SZE = 0;
  ftS = FT_GetQueueStatus(ftH, &rx.CNT);
  if (rx.CNT != 0)
  {
    fprintf(stderr, "Error - MPSSE receive buffer should be empty\n", ftS);
    FT_SetBitMode(ftH, 0x00, 0x00);
    FT_Close(ftH);
    retVal = false;
    // return 1; // maybe this should be exit(0)?
  }
  else
  {
    //    fprintf(stderr, "Loopback Disabled\n");
  }
  /* End of testing bad command AN 135 5.3.1*/
  return retVal;
} /* End AN 135 Section 5.3 "Configure the FTDI MPSSE" */

BYTE readGPIObyte(FT_HANDLE ftH, BYTE lhB) // Low byte = 0, High byte = 1
{                                          /* AN 135 Section 5.5 GPIO Read*/
  BYTE retVal = 0x00;
  FT_STATUS ftS;
  PKT tx, rx;
  BYTE opCode = 0; // FTDI OPCODE
  BYTE idx = 0;
/*  DWORD BTS = 0;   // Bytes to send
  DWORD BTR = 0;   // Bytes to read
  DWORD NBS = 0;   // Number of bytes sent
  DWORD NBR = 0;   // Number of bytes read
  BYTE oBuff[64];
  BYTE iBuff[64]; */
  // Change scope trigger to channel 4 (TMS/CS) falling edge
//  BTS = 0;
  tx.SZE = 0;
  opCode = (lhB == 0) ? 0x81 : 0x83;
//  oBuff[BTS++] = opCode; // AN 108 3.6
  tx.MSG[tx.SZE++] = opCode; // AN 108 3.6
  // Get data bits - returns state of pins, either input or output
  // on low byte of MPSSE
//  ftS = FT_Write(ftH, oBuff, BTS, &NBS);
  ftS = FT_Write(ftH, tx.MSG, tx.SZE, &tx.CNT);
  // Read the low GPIO byte
//  BTS = 0;  // Reset output buffer pointer
  tx.SZE = 0;  // Reset output buffer pointer
  Sleep(2); // Wait for data to be transmitted and status to be returned
  // by the device driver - see latency timer above
  // Check the receive buffer - there should be one byte
//  ftS = FT_GetQueueStatus(ftH, &BTR);
  ftS = FT_GetQueueStatus(ftH, &rx.SZE);
  // Get the number of bytes in the FT2232H receive buffer
//  ftS |= FT_Read(ftH, &iBuff, BTR, &NBR);
  ftS |= FT_Read(ftH, &rx.MSG, rx.SZE, &rx.CNT);
//  if ((ftS != FT_OK) && (BTR != 1))
  if ((ftS != FT_OK) && (rx.SZE != 1))
  {
    fprintf(stderr, "Error - GPIO cannot be read\n");
    FT_SetBitMode(ftH, 0x0, 0x00); // Reset the port to disable MPSSE
    FT_Close(ftH);                 // Close the USB port
    // return 1;                             // Exit with error
    exit(1); // Exit with error
  }
  else
  {
//    retVal = iBuff[0];
    retVal = rx.MSG[0];
    if (lhB == 0)
    {
      L1IFStat = retVal & 0x00FF;
    }
    else
    {
      L1IFStat = ((retVal & 0x00FF) << 8) & 0xFF00;
    }
    // fprintf(stderr, "The GPIO low-byte = 0x%X\n", retVal);
    // The input buffer only contains one valid byte at location 0
    return retVal;
  }
} /* End AN 135 Section 5.5 GPIO Read*/

void toggleGPIO(FT_HANDLE ftH, BYTE bits)
{
  FT_STATUS ftS;
  PKT tx, rx;
  BYTE idx = 0;
  BYTE opCode = 0x80; // FTDI OPCODE 0x80 = WRite Lower Byte
  BYTE Value = (BYTE)(L1IFStat & 0x00FF);
  BYTE direction = loGPIOdirection;
  //  BYTE loGPIOdirection = 0xCB; // 1100 1011
  //  BYTE loGPIOdefaults  = 0xCB; // 11xx 1x11

  switch (bits)
  {
  case 0x40:
    Value ^= IDLE;
    break;
  case 0x80:
    Value ^= SHDN;
    break;
  default:
    fprintf(stderr, "Idle or shutdown only\n");
    break;
  }
  tx.SZE = 0;
  tx.MSG[tx.SZE++] = opCode;
  tx.MSG[tx.SZE++] = Value;
  tx.MSG[tx.SZE++] = direction;

  ftS = FT_Write(ftH, tx.MSG, tx.SZE, &tx.CNT);
  if ((ftS != FT_OK) && (tx.CNT != 3))
  {
    fprintf(stderr, "Error - GPIO not written\n");
    FT_SetBitMode(ftH, 0x0, 0x00); // Reset the port to disable MPSSE
    FT_Close(ftH);                 // Close the USB port
    exit(1);                       // Exit with error
  }
  Sleep(2);      // Wait for data to be transmitted and status to be returned
  opCode = 0x81; // FTDI OPCODE 0x81 = Read Lower Byte
  tx.SZE = 0;
  tx.MSG[tx.SZE++] = opCode;
  ftS = FT_Write(ftH, tx.MSG, tx.SZE, &tx.CNT);
  tx.SZE = 0;
  Sleep(2);
  ftS = FT_GetQueueStatus(ftH, &rx.SZE);
  ftS |= FT_Read(ftH, &rx.MSG, rx.SZE, &rx.CNT);
  if ((ftS != FT_OK) && (rx.SZE != 1))
//  if ((ftS != FT_OK) && (rx.CNT != 1)) // I think this should be NBR
  {
    fprintf(stderr, "Error - GPIO cannot be read\n");
    FT_SetBitMode(ftH, 0x00, 0x00);
    FT_Close(ftH);
    exit(1);
  }
  else
  {
    fprintf(stderr, "GPIO low-byte:0x%.2X\n", rx.MSG[0]);
    L1IFStat = (WORD) rx.MSG[0] & 0x00FF;
  }
}

void displayDevInfo(FT_DEVICE_LIST_INFO_NODE *dInfo, DWORD numD)
{
  WORD i;
  for (i = 0; i < numD; i++)
  {
    printf("Dev %d:\n", i);
    printf("  Flags=0x%x\n", dInfo[i].Flags);
    printf("  Type=0x%x\n", dInfo[i].Type);
    printf("  ID=0x%x\n", dInfo[i].ID);
    printf("  LocId=0x%x\n", dInfo[i].LocId);
    printf("  SerialNumber=%s\n", dInfo[i].SerialNumber);
    printf("  Description=%s\n", dInfo[i].Description);
    printf("  ftHandle=0x%x\n", (DWORD)dInfo[i].ftHandle);
  }
}

void displayL1IFStatus(WORD boardStatus)
{
  fprintf(stderr, "Bit 0 BDBUS0 (SPI CLK)->: %s\n",
          ((boardStatus & 0x0001) == 1) ? "HI" : "LO");
  fprintf(stderr, "Bit 1 BDBUS1 (SPI  DO)->: %s\n",
          ((boardStatus & 0x0002) >> 1 == 1) ? "HI" : "LO");
  fprintf(stderr, "Bit 2 BDBUS2 (SPI  DI)<-: %s\n",
          ((boardStatus & 0x0004) >> 2 == 1) ? "HI" : "LO");
  fprintf(stderr, "Bit 3 BDBUS3 (SPI ~CS)->: %s\n",
          ((boardStatus & 0x0008) >> 3 == 1) ? "HI" : "LO");
  fprintf(stderr, "Bit 4 BDBUS4 (LD{PLL})<-: %s\n",
          ((boardStatus & 0x0010) >> 4 == 1) ? "HI" : "LO");
  fprintf(stderr, "Bit 5 BDBUS5 (AntFlag)<-: %s\n",
          ((boardStatus & 0x0020) >> 5 == 1) ? "HI" : "LO");
  fprintf(stderr, "Bit 6 BDBUS6   (~IDLE)->: %s\n",
          ((boardStatus & 0x0040) >> 6 == 1) ? "HI" : "LO");
  fprintf(stderr, "Bit 7 BDBUS7   (~SHDN)->: %s\n",
          ((boardStatus & 0x0080) >> 7 == 1) ? "HI" : "LO");
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
int main(int argc, char *argv[])
{
  FT_STATUS ftStatus;
  FT_HANDLE ftdiHandle;
  BYTE GPIOdata = 0;
  BYTE ch = ' ';
  bool devMPSSEConfig = false;
  bool antennaConnected = false;
  bool noPause = false;
  DWORD numDevs;
  FT_DEVICE_LIST_INFO_NODE *devInfo;

  if ((argc == 2) && (argv[1][0] == '~'))
  {
    noPause = true;
  }

  ftStatus = FT_CreateDeviceInfoList(&numDevs); // AN_135 4.1 Step 1
  if (ftStatus == FT_OK)
    printf("Number of devices: %.1d\n", numDevs);
  else
  {
    printf("No Devices\n");
    return 1;
  }
  if (numDevs > 0)
  { // allocate storage for list based on numDevs
    devInfo =
        (FT_DEVICE_LIST_INFO_NODE *)malloc(sizeof(FT_DEVICE_LIST_INFO_NODE) * numDevs);
    // get the device information list
    ftStatus = FT_GetDeviceInfoList(devInfo, &numDevs); // AN_135 4.1 Step 2
    if (ftStatus == FT_OK)
    {
      displayDevInfo(devInfo, numDevs);
    }
  }
  else
  {
    fprintf(stderr, "No Devices\n");
    return 1;
  }

  // ftStatus = FT_OpenEx("USB<->GPS B", FT_OPEN_BY_DESCRIPTION, &ftdiHandle);
  ftStatus = FT_OpenEx(devInfo[1].Description, FT_OPEN_BY_DESCRIPTION, &ftdiHandle);
  if (ftStatus != FT_OK) // AN_135 4.1 Step 3
  {
    fprintf(stderr, "Can't open FT2232H device! \n");
    return 1;
  }
  else
  { // Port opened successfully
    fprintf(stderr, "Successfully opened FT2232H device \"%s\" for MPSSE! \n",
            devInfo[1].Description);
  }
  /* AN 135 Section 4.2*/
  devMPSSEConfig = configureMPSSE(ftdiHandle);
  if (devMPSSEConfig == false)
  {
    printf("fail on initialize FT2232H device ! \n");
    return 1;
  }
  else
  {
    //    fprintf(stderr, "AN 135 Section 4.2 complete\n");
  }

  // Synchronize MPSSE with Bad Command AN 135 5.3.1
  if (testBadCommand(ftdiHandle, 0xAA) != true)
  {
    fprintf(stderr, "Failed to sync with Bad Command 0xAA!\n");
    return 1;
  }
  else
  {
    if (testBadCommand(ftdiHandle, 0xAB) != true)
    {
      fprintf(stderr, "Failed to sync with Bad Command 0xAB!\n");
      return 1;
    }
  }

  /* Configure SPI here. Do I need to? */

  /* Now READ the GPIO to See if Antenna is attached*/
  GPIOdata = readGPIObyte(ftdiHandle, 0);
  displayL1IFStatus(L1IFStat);

  antennaConnected = ((GPIOdata & 0x20) >> 5) == 1 ? true : false;
  printf("Antenna connected? %s\n", (antennaConnected) == true ? "yes" : "no");

  // GPIO retains its setting until its reset
  if (noPause == false)
  {
    while (ch != 0x0D)
    {
      printf("Press <Enter> to continue, 's' for shutdown, or 'i' for idle\n");
      ch = getch(); // wait for a carriage return, or don't
      switch (ch)
      {
      case 's':
        toggleGPIO(ftdiHandle, SHDN);
        GPIOdata = readGPIObyte(ftdiHandle, 0);
        displayL1IFStatus(L1IFStat);
        break;
      case 'i':
        toggleGPIO(ftdiHandle, IDLE);
        GPIOdata = readGPIObyte(ftdiHandle, 0);
        displayL1IFStatus(L1IFStat);
        break;
      default:
        break;
      }
      // printf("Got it? 0x%.2X ", ch);
      // ch = getch(); // wait for a carriage return, or don't
    }
  }

  FT_SetBitMode(ftdiHandle, 0x00, 0x00); // Reset MPSSE
  FT_Close(ftdiHandle);                  // Close Port
  free(devInfo);
}