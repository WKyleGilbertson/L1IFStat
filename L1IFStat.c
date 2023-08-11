#pragma comment(lib, "lib/FTD2XX.lib")
#include "inc/FTD2XX.h"
#include <windows.h>
#include <stdbool.h>
#include <stdio.h>

#define MLEN 64
#define MemSize 32

//const BYTE SPIDATALENGTH = 32;
const BYTE AA_ECHO_CMD_1 = 0xAA;
const BYTE AB_ECHO_CMD_2 = 0xAB;
const BYTE BAD_COMMAND_RESPONSE = 0xFA;
// How to clock the data out of FTDI chip
//const BYTE MSB_RISING_EDGE_CLOCK_BYTE_OUT = 0x10;
//const BYTE MSB_FALLING_EDGE_CLOCK_BYTE_OUT = 0x11;
//const BYTE MSB_RISING_EDGE_CLOCK_BIT_OUT = 0x12;
const BYTE MSB_FALLING_EDGE_CLOCK_BIT_OUT = 0x13;
// How to clock data in to FTDI chip .... WE DON'T
//const BYTE MSB_RISING_EDGE_CLOCK_BYTE_IN = 0x20;
//const BYTE MSB_RISING_EDGE_CLOCK_BIT_IN = 0x22;
//const BYTE MSB_FALLING_EDGE_CLOCK_BYTE_IN = 0x24;
//const BYTE MSB_FALLING_EDGE_CLOCK_BIT_IN = 0x26;

WORD L1IFStat;
const BYTE loGPIOdirection = 0xCB; // 1100 1011 : 1 = Out, 0 = In 
const BYTE loGPIOdefaults = 0xCB;  // 11xx 1x11

enum gpio
{
  IDLE = 0x40,
  SHDN = 0x80
};

typedef struct
{
  BYTE MSG[MLEN];
  DWORD SZE;
  DWORD CNT;
} PKT;

void SPI_CSEnable(PKT *pkt) // Not sure these are correct for L1IF
{
  for (int loop = 0; loop < 5; loop++)
  {
    pkt->MSG[pkt->SZE++] = 0x80; // GPIO command for ADBUS
    pkt->MSG[pkt->SZE++] = 0xC0; // Value -- Chip Select is Active Low
    pkt->MSG[pkt->SZE++] = 0xCb; // Direction
  }
}

void SPI_CSDisable(PKT *pkt) // Not sure these are correct for L1IF
{
  for (int loop = 0; loop < 5; loop++) // One 0x80 command can keep 0.2 us
  {                                    // Do 5 times to stay in for 1 us
    pkt->MSG[pkt->SZE++] = 0x80; // AN 108 Section 3.6.1 Set Data bits LowByte
    pkt->MSG[pkt->SZE++] = 0xC8; // Value -- Deselect Chip
    pkt->MSG[pkt->SZE++] = 0xCb; // Direction
  }
}

bool configureSPI(FT_HANDLE ftH) /* AN 114 Section 3.1 */
{
  bool retVal = false;
  FT_STATUS ftS;
  PKT tx;
  BYTE idx = 0;
  DWORD dwClockDivisor = 29; // Value of clock divisor, SCL frequency...
                             // SCL frequency = 60/((1+29)*2) (MHz) = 1 MHz
  ////////////////////////////////////////////////////////////////////
  // Configure the MPSSE for SPI communication with EEPROM
  //////////////////////////////////////////////////////////////////
  tx.SZE = 0;
  tx.MSG[tx.SZE++] = 0x8A;                      // Ensure disable clock divide by 5 for 60Mhz master clock
  tx.MSG[tx.SZE++] = 0x97;                      // Ensure turn off adaptive clocking
  tx.MSG[tx.SZE++] = 0x8D;                      // disable 3 phase data clock
  ftS = FT_Write(ftH, tx.MSG, tx.SZE, &tx.CNT); // Send off the commands
  tx.SZE = 0;                                   // Clear output buffer

  tx.MSG[tx.SZE++] = 0x80; // Command to set directions of lower 8 pins and force value on bits set as output
                           //  tx.MSG[tx.SZE++] = 0x00; // Set SDA, SCL high, WP disabled by SK, DO at bit ��1��, GPIOL0 at bit ��0��
                           //  tx.MSG[tx.SZE++] = 0x0b; // Set SK,DO,GPIOL0 pins as output with bit ��1��, other pins as input with bit ��0��
  tx.MSG[tx.SZE++] = 0xCB; // Set SDA, SCL high, WP disabled by SK, DO at bit ��1��, GPIOL0 at bit ��0��
  tx.MSG[tx.SZE++] = 0xCB; // Set SK,DO,GPIOL0 pins as output with bit ��1��, other pins as input with bit ��0��
  // The SK clock frequency can be worked out by below algorithm with divide by 5 set as off
  // SK frequency  = 60MHz /((1 +  [(1 +0xValueH*256) OR 0xValueL])*2)
  tx.MSG[tx.SZE++] = 0x86;                          // Command to set clock divisor
  tx.MSG[tx.SZE++] = (BYTE)(dwClockDivisor & 0xFF); // Set 0xValueL of clock divisor
  tx.MSG[tx.SZE++] = (BYTE)(dwClockDivisor >> 8);   // Set 0xValueH of clock divisor
  ftS = FT_Write(ftH, tx.MSG, tx.SZE, &tx.CNT);     // Send off the commands
  tx.SZE = 0;                                       // Clear output buffer
  Sleep(20);                                        // Delay for a while

  // Turn off loop back in case
  tx.MSG[tx.SZE++] = 0x85;                      // Command to turn off loop back of TDI/TDO connection
  ftS = FT_Write(ftH, tx.MSG, tx.SZE, &tx.CNT); // Send off the commands
  tx.SZE = 0;                                   // Clear output buffer
  Sleep(30);                                    // Delay for a while
  printf("SPI initial successful\n");
  return true;
}

bool sendSPItoMAX(FT_HANDLE ftH, UINT32 DATA, BYTE ADDR)
{
  FT_STATUS ftS;
  PKT tx;
  bool retVal = false;
  tx.SZE = 0;
  // dwNumBytesSent = 0;
  DATA |= (ADDR & 0x0F);
  SPI_CSEnable(&tx);
  //
  tx.MSG[tx.SZE++] = MSB_FALLING_EDGE_CLOCK_BIT_OUT; // AN 108 Section 3.3.4
  tx.MSG[tx.SZE++] = 7;
  // OutputBuffer[dwNumBytesToSend++] = '\x9A';
  tx.MSG[tx.SZE++] = (DATA & 0xFF000000) >> 24;
  tx.MSG[tx.SZE++] = MSB_FALLING_EDGE_CLOCK_BIT_OUT;
  tx.MSG[tx.SZE++] = 7;
  // OutputBuffer[dwNumBytesToSend++] = '\xC0';
  tx.MSG[tx.SZE++] = (DATA & 0x00FF0000) >> 16;
  tx.MSG[tx.SZE++] = MSB_FALLING_EDGE_CLOCK_BIT_OUT;
  tx.MSG[tx.SZE++] = 7;
  // OutputBuffer[dwNumBytesToSend++] = '\x00';
  tx.MSG[tx.SZE++] = (DATA & 0x0000FF00) >> 8;
  tx.MSG[tx.SZE++] = MSB_FALLING_EDGE_CLOCK_BIT_OUT;
  tx.MSG[tx.SZE++] = 7;
  // OutputBuffer[dwNumBytesToSend++] = '\x83';
  tx.MSG[tx.SZE++] = (DATA & 0x000000FF);
  //
  SPI_CSDisable(&tx);
  ftS = FT_Write(ftH, tx.MSG, tx.SZE, &tx.CNT);
  tx.SZE = 0; // Clear output buffer
  if (ftS == FT_OK)
  {
    retVal = true;
  }
  else
  {
    retVal = false;
  }
  return retVal;
}

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
  tx.MSG[tx.SZE++] = 0x84; // Enable loopback AN 108 3.7
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
  tx.MSG[tx.SZE++] = cmd;                       // Add BAD command ¡®0xAA¡¯
  ftS = FT_Write(ftH, tx.MSG, tx.SZE, &tx.CNT); // Send off the BAD commands
                                                //	fprintf(stderr, "Bytes Sent: %d\n", tx.CNT);
  tx.SZE = 0;                                   // Clear output buffer
  do
  {
    ftS = FT_GetQueueStatus(ftH, &rx.SZE);   // Get the number of bytes in the device input buffer
  } while ((rx.SZE == 0) && (ftS == FT_OK)); // or Timeout

  ftS = FT_Read(ftH, rx.MSG, rx.SZE, &rx.CNT); // Read out the data from input buffer
  for (idx = 0; idx < (rx.CNT - 1); idx++)     // Check if Bad command and echo command received
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
  BYTE idx = 0;
  BYTE opCode = 0; // FTDI OPCODE AN 108 Section 3.6
  // Change scope trigger to channel 4 (TMS/CS) falling edge
  tx.SZE = 0;
  opCode = (lhB == 0) ? 0x81 : 0x83;
  tx.MSG[tx.SZE++] = opCode;
  // Get data bits - returns state of pins, either input or output
  // on low byte of MPSSE
  ftS = FT_Write(ftH, tx.MSG, tx.SZE, &tx.CNT); // Read the low GPIO byte
  tx.SZE = 0;                                   // Reset output buffer pointer
  Sleep(2);                                     // Wait for data to be transmitted and status to be returned
  // by the device driver - see latency timer above
  // Check the receive buffer - there should be one byte
  ftS = FT_GetQueueStatus(ftH, &rx.SZE);
  // Get the number of bytes in the FT2232H receive buffer
  ftS |= FT_Read(ftH, &rx.MSG, rx.SZE, &rx.CNT);
  if ((ftS != FT_OK) && (rx.SZE != 1))
  {
    fprintf(stderr, "Error - GPIO cannot be read\n");
    FT_SetBitMode(ftH, 0x0, 0x00); // Reset the port to disable MPSSE
    FT_Close(ftH);                 // Close the USB port
    exit(1);                       // Exit with error
  }
  else
  {
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
    L1IFStat = (WORD)rx.MSG[0] & 0x00FF;
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
  bool configGPSCLK = false;
  //bool configGPSCLK = true;
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
  if (configGPSCLK == true)
  {
    if (configureSPI(ftdiHandle) != true)
    {
      fprintf(stderr, "Error - SPI not configured\n");
      FT_SetBitMode(ftdiHandle, 0x00, 0x00);
      FT_Close(ftdiHandle);
      exit(1);
    }
    else
    {
      /*      // GPSConfig(ftdiHandle, 0x9AC00080, 0x03); // 4 MHz
            //GPSConfig(ftdiHandle, 0x9CC00080, 0x03); // 8 MHz
            // GPSConfig(ftdiHandle, 0x9EC00080, 0x03); // 16 MHz */
      // Notice the trailing zero on the data... that's where the address goes
      fprintf(stderr, "Sending Clock Speed Change\n");
      //sendSPItoMAX(ftdiHandle, 0x9AC00080, 0x03); // 4 MHz
      sendSPItoMAX(ftdiHandle, 0x9CC00080, 0x03); // 8 MHz
      //sendSPItoMAX(ftdiHandle, 0x9EC00080, 0x03); // 16 MHz
      Sleep(20);
    }
  }
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