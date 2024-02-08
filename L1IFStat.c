#pragma comment(lib, "lib/FTD2XX.lib")
#include "inc/ftd2xx.h"
#include "inc/version.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#if !defined(_WIN32) && (defined(__UNIX__) || (defined(__APPLE)))
#include <time.h>
#include <unistd.h>
#include "inc/WinTypes.h"
#endif

#define MLEN 64

// const BYTE AA_ECHO_CMD_1 = 0xAA;
// const BYTE AB_ECHO_CMD_2 = 0xAB;
// const BYTE BAD_COMMAND_RESPONSE = 0xFA;
const uint8_t MSB_FALLING_EDGE_CLOCK_BIT_OUT = 0x13; // AN 108 Section 3.3.4

uint16_t L1IFStat;
const uint8_t loGPIOdirection = 0xCB; // 1100 1011 : 1 = Out, 0 = In
const uint8_t loGPIOdefaults = 0xCB;  // 11xx 1x11

enum gpio
{
  IDLE = 0x40,
  SHDN = 0x80,
  LED = 0x80
};

enum maxreg
{
  CONF1 = 0x00,
  CONF2 = 0x01,
  CONF3 = 0x02,
  PLLCONF = 0x03,
  DIV = 0x04,
  FDIV = 0x05,
  STRM = 0x06,
  CLK = 0x07,
  TEST1 = 0x08,
  TEST2 = 0x09
};

typedef struct
{
  uint8_t MSG[MLEN];
  uint32_t SZE;
  uint32_t CNT;
} PKT;

int8_t msPause(uint32_t ms)
{
  int8_t retVal = 0;
#if defined(_WIN32)
  Sleep(ms);
#elif !defined(_WIN32)
  struct timespec remaining, request;
  request.tv_sec = 0;
  request.tv_nsec = ms * 1000000;
  retVal = nanosleep(&request, &remaining);
#endif
  return retVal;
}

void SPI_CSEnable(PKT *pkt) // Not sure these are correct for L1IF
{
  uint8_t idx;
  for (idx = 0; idx < 5; idx++)
  {
    pkt->MSG[pkt->SZE++] = 0x80; // GPIO command for ADBUS
    pkt->MSG[pkt->SZE++] = 0xC0; // Value -- Chip Select is Active Low
    pkt->MSG[pkt->SZE++] = 0xCB; // Direction
  }
}

void SPI_CSDisable(PKT *pkt) // Not sure these are correct for L1IF
{
  uint8_t idx;
  for (idx = 0; idx < 5; idx++)  // One 0x80 command can keep 0.2 us
  {                              // Do 5 times to stay in for 1 us
    pkt->MSG[pkt->SZE++] = 0x80; // AN 108 Section 3.6.1 Set Data bits LowByte
    pkt->MSG[pkt->SZE++] = 0xC8; // Value -- Deselect Chip
    pkt->MSG[pkt->SZE++] = 0xCB; // Direction
  }
}

bool sendSPItoMAX(FT_HANDLE ftH, uint32_t DATA, uint8_t ADDR)
{
  FT_STATUS ftS;
  PKT tx;
  bool retVal = false;

  tx.SZE = 0;
  DATA |= (ADDR & 0x0F);

  SPI_CSEnable(&tx);
  tx.MSG[tx.SZE++] = MSB_FALLING_EDGE_CLOCK_BIT_OUT; // AN 108 Section 3.3.4
  tx.MSG[tx.SZE++] = 7;
  tx.MSG[tx.SZE++] = (DATA & 0xFF000000) >> 24;
  tx.MSG[tx.SZE++] = MSB_FALLING_EDGE_CLOCK_BIT_OUT;
  tx.MSG[tx.SZE++] = 7;
  tx.MSG[tx.SZE++] = (DATA & 0x00FF0000) >> 16;
  tx.MSG[tx.SZE++] = MSB_FALLING_EDGE_CLOCK_BIT_OUT;
  tx.MSG[tx.SZE++] = 7;
  tx.MSG[tx.SZE++] = (DATA & 0x0000FF00) >> 8;
  tx.MSG[tx.SZE++] = MSB_FALLING_EDGE_CLOCK_BIT_OUT;
  tx.MSG[tx.SZE++] = 7;
  tx.MSG[tx.SZE++] = (DATA & 0x000000FF);
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

bool configureSPI(FT_HANDLE ftH) /* AN 114 Section 3.1 */
{
  bool retVal = false;
  FT_STATUS ftS;
  PKT tx;
  uint8_t idx = 0;
  uint32_t dwClockDivisor = 29; // Value of clock divisor, SCL frequency...
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
                           //  tx.MSG[tx.SZE++] = 0x00;  Set SDA, SCL high, WP disabled by SK, DO at bit ��1��, GPIOL0 at bit ��0��
                           //  tx.MSG[tx.SZE++] = 0x0b;  Set SK,DO,GPIOL0 pins as output with bit ��1��, other pins as input with bit ��0��
  tx.MSG[tx.SZE++] = 0xCB; // Set SDA, SCL high, WP disabled by SK, DO at bit ��1��, GPIOL0 at bit ��0��
  tx.MSG[tx.SZE++] = 0xCB; // Set SK,DO,GPIOL0 pins as output with bit ��1��, other pins as input with bit ��0��
  // The SK clock frequency can be worked out by below algorithm with divide by 5 set as off
  // SK frequency  = 60MHz /((1 +  [(1 +0xValueH*256) OR 0xValueL])*2)
  tx.MSG[tx.SZE++] = 0x86;                          // Command to set clock divisor
  tx.MSG[tx.SZE++] = (BYTE)(dwClockDivisor & 0xFF); // Set 0xValueL of clock divisor
  tx.MSG[tx.SZE++] = (BYTE)(dwClockDivisor >> 8);   // Set 0xValueH of clock divisor
  ftS = FT_Write(ftH, tx.MSG, tx.SZE, &tx.CNT);     // Send off the commands
  tx.SZE = 0;                                       // Clear output buffer
  msPause(20);
  // Turn off loop back in case
  tx.MSG[tx.SZE++] = 0x85;                      // Command to turn off loop back of TDI/TDO connection
  ftS = FT_Write(ftH, tx.MSG, tx.SZE, &tx.CNT); // Send off the commands
  tx.SZE = 0;                                   // Clear output buffer
  msPause(30);
  //  fprintf(stderr, "SPI initial successful\n");
  return true;
}

bool configureMPSSE(FT_HANDLE ftH)
{ /* AN 135 Section 4.2*/
  // AN 135 Section 5.2 "Configure FTDI Port for MPSSE Use"
  FT_STATUS ftS;
  PKT rx;

  ftS = FT_ResetDevice(ftH); // Reset peripheral side of FTDI port
  ftS |= FT_GetQueueStatus(ftH, &rx.SZE);
  if ((ftS == FT_OK) && (rx.CNT > 0))
    ftS |= FT_Read(ftH, rx.MSG, rx.SZE, &rx.CNT); // Read it to throw it away
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
    msPause(50);
  }
  return true;
} /* End AN 135 Section 4.2 */

bool testBadCommand(FT_HANDLE ftH, uint8_t cmd)
{ /* AN 135 Section 5.3 "Configure the FTDI MPSSE" */
  //////////////////////////////////////////////////////////////////
  // Synchronize the MPSSE interface by sending bad command ¡®0xAA¡¯
  //////////////////////////////////////////////////////////////////
  bool retVal = false;
  FT_STATUS ftS;
  PKT tx, rx;
  uint8_t idx = 0;

  // Enable internal loop-back AN 135 Section 5.3.1
  tx.SZE = 0;
  tx.MSG[tx.SZE++] = 0x84; // Enable loopback AN 108 3.7
  ftS = FT_Write(ftH, tx.MSG, tx.SZE, &tx.CNT);
  tx.SZE = 0;
  ftS = FT_GetQueueStatus(ftH, &rx.CNT);
  if (rx.CNT != 0)
  {
    fprintf(stderr, "Error - MPSSE receive buffer should be empty %d\n", ftS);
    printf("rx.CNT = %d\n", rx.CNT);
    FT_SetBitMode(ftH, 0x00, 0x00);
    FT_Close(ftH);
    // retVal = false;
    return retVal;
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
    fprintf(stderr, "Error - MPSSE receive buffer should be empty %d\n", ftS);
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

uint8_t readGPIObyte(FT_HANDLE ftH, uint8_t lhB) // Low byte = 0, High byte = 1
{                                                /* AN 135 Section 5.5 GPIO Read*/
  uint8_t retVal = 0x00;
  FT_STATUS ftS;
  PKT tx, rx;
  uint8_t idx = 0;
  uint8_t opCode = 0; // FTDI OPCODE AN 108 Section 3.6
  // Change scope trigger to channel 4 (TMS/CS) falling edge
  tx.SZE = 0;
  opCode = (lhB == 0) ? 0x81 : 0x83;
  tx.MSG[tx.SZE++] = opCode;
  // Get data bits - returns state of pins, either input or output
  // on low byte of MPSSE
  ftS = FT_Write(ftH, tx.MSG, tx.SZE, &tx.CNT); // Read the low GPIO byte
  tx.SZE = 0;                                   // Reset output buffer pointer
  msPause(2);
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

void toggleGPIOHighByte(FT_HANDLE ftH, uint8_t bits)
{
  FT_STATUS ftS;
  PKT tx, rx;
  uint8_t idx = 0;
  uint8_t opCode = 0x82; // FTDI OPCODE 0x82 = Write High Byte
  uint8_t Value = (uint8_t)((L1IFStat & 0xFF00) >> 8);
  //  uint8_t Value = (bits);
  // uint8_t direction = loGPIOdirection;
  uint8_t direction = 0xFF;
  //  uint8_t loGPIOdirection = 0xCB; // 1100 1011
  //  uint8_t loGPIOdefaults  = 0xCB; // 11xx 1x11

  switch (bits)
  {
  case 0x80:
    Value ^= LED;
    break;
  default:
    fprintf(stderr, "LED only\n");
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
  msPause(2);
  opCode = 0x83; // FTDI OPCODE 0x81 = Read Lower Byte
  tx.SZE = 0;
  tx.MSG[tx.SZE++] = opCode;
  ftS = FT_Write(ftH, tx.MSG, tx.SZE, &tx.CNT);
  tx.SZE = 0;
  msPause(2);
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
    fprintf(stderr, "GPIO high-byte:0x%.2X\n", rx.MSG[0]);
    L1IFStat = (uint16_t)rx.MSG[0] | 0xFF00;
  }
}

void toggleGPIO(FT_HANDLE ftH, uint8_t bits)
{
  FT_STATUS ftS;
  PKT tx, rx;
  uint8_t idx = 0;
  uint8_t opCode = 0x80; // FTDI OPCODE 0x80 = WRite Lower Byte
  uint8_t Value = (uint8_t)(L1IFStat & 0x00FF);
  uint8_t direction = loGPIOdirection;
  //  uint8_t loGPIOdirection = 0xCB; // 1100 1011
  //  uint8_t loGPIOdefaults  = 0xCB; // 11xx 1x11

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
  msPause(2);
  opCode = 0x81; // FTDI OPCODE 0x81 = Read Lower Byte
  tx.SZE = 0;
  tx.MSG[tx.SZE++] = opCode;
  ftS = FT_Write(ftH, tx.MSG, tx.SZE, &tx.CNT);
  tx.SZE = 0;
  msPause(2);
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
    L1IFStat = (uint16_t)rx.MSG[0] | 0x00FF;
  }
}

void displayDevInfo(FT_DEVICE_LIST_INFO_NODE *dInfo, uint32_t numD)
{
  uint16_t i;
  for (i = 0; i < numD; i++)
  {
    printf("Dev %d: ", i);
    printf("  Flags=0x%x ", dInfo[i].Flags);
    printf("  Type=0x%x ", dInfo[i].Type);
    printf("  ID=0x%x\n", dInfo[i].ID);
    printf("  LocId=0x%x", dInfo[i].LocId);
    printf("  SerialNumber=%s ", dInfo[i].SerialNumber);
    printf("  Description=%s ", dInfo[i].Description);
    printf("  ftHandle=0x%x\n", (uint32_t)dInfo[i].ftHandle);
  }
}

void displayL1IFStatus(uint16_t boardStatus)
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
  FT_PROGRAM_DATA ftData;
  uint8_t GPIOdata = 0;
  uint8_t LEDState = 0;
  int8_t ch = ' ', trash = ' ';
  bool devMPSSEConfig = false;
  bool devSPIConfig = false;
  bool antennaConnected = false;
  bool noPause = false;
  uint32_t numDevs;
  FT_DEVICE_LIST_INFO_NODE *devInfo;
  SWV V;

  V.Major = MAJOR_VERSION;
  V.Minor = MINOR_VERSION;
  V.Patch = PATCH_VERSION;
#ifdef CURRENT_HASH
  strncpy(V.GitCI, CURRENT_HASH, 40);
  V.GitCI[40] = '\0';
#endif
#ifdef CURRENT_DATE
  strncpy(V.BuildDate, CURRENT_DATE, 16);
  V.BuildDate[16] = '\0';
#endif
#ifdef CURRENT_NAME
  strncpy(V.Name, CURRENT_NAME, 10);
#endif

  fprintf(stdout, "%s: GitCI:%s %s v%.1d.%.1d.%.1d\n",
          V.Name, V.GitCI, V.BuildDate,
          V.Major, V.Minor, V.Patch);

  if ((argc == 2) && (argv[1][0] == '!'))
  {
    noPause = true;
  }
  else if ((argc != 1) && (argv[1][0] != '!'))
  {
    fprintf(stdout, "usage: L1IFStat [!]\n");
    fprintf(stdout, "       ! option skips interactive mode.\n");
    fprintf(stdout, "  default: Interactive mode for device query.\n");
    exit(0);
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

  //   ftStatus = FT_Open(1, &ftdiHandle);
  ftStatus = FT_OpenEx("USB<->GPS B", FT_OPEN_BY_DESCRIPTION, &ftdiHandle);
  // ftStatus = FT_OpenEx(devInfo[1].Description, FT_OPEN_BY_DESCRIPTION, &ftdiHandle);
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
  devSPIConfig = configureSPI(ftdiHandle);
  if (devSPIConfig != true)
  {
    fprintf(stderr, "Error - SPI not configured\n");
    FT_SetBitMode(ftdiHandle, 0x00, 0x00);
    FT_Close(ftdiHandle);
    exit(1);
  }
  else
  {
    fprintf(stderr, "SPI Initialized succesfully!\n");
  }

  /* Now READ the GPIO to See if Antenna is attached*/
  GPIOdata = readGPIObyte(ftdiHandle, 0);
  displayL1IFStatus(L1IFStat);

  antennaConnected = ((GPIOdata & 0x20) >> 5) == 1 ? true : false;
  printf("Antenna connected? %s\n", (antennaConnected) == true ? "yes" : "no");

  // GPIO retains its setting until MPSSE reset
  if (noPause == false)
  {
    while (ch != 0x0A) // 0x0A = Line Feed (LF)
    // while (ch != 0x0D) // 0x0D = Carriage Feed (CR)
    {
      printf("Press <Enter> or 'x' to exit, or 's'for shutdown,");
      printf(" 'i'dle, 'l'ED, or 'p'rogram.\n");
      printf("Press <Enter> after selection key. >");
      ch = getchar(); // wait for a carriage return, or don't
      ch = tolower(ch);
      //      printf("ch:%.2X\n", ch); // For debugging selections
      if (ch != 0x0A)
        trash = getchar(); // to grab the enter
      else
        break;
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
      case 'p':
        // sendSPItoMAX(ftdiHandle, 0x9AC00080, 0x03); // 4 MHz
        //sendSPItoMAX(ftdiHandle, 0x9CC00080, PLLCONF); // 8 MHz
                                                       // sendSPItoMAX(ftdiHandle, 0x9EC00080, 0x03); // 16 MHz
                                                       // Notice the trailing zero on the data... that's where the address goes
        sendSPItoMAX(ftdiHandle, 0xA2919A70, CONF1); //
        msPause(20);
        sendSPItoMAX(ftdiHandle, 0x85512881, CONF2); //
        msPause(20);
        //sendSPItoMAX(ftdiHandle, 0xEAFE1DC0, CONF3); //
        //msPause(200);
        //sendSPItoMAX(ftdiHandle, 0x9EC00080, PLLCONF); // 16.368 MHz
        //msPause(200);
        //sendSPItoMAX(ftdiHandle, 0x0C000800, DIV); //
        //msPause(200);
        //sendSPItoMAX(ftdiHandle, 0x80000700, FDIV); //
        //msPause(200);
        //sendSPItoMAX(ftdiHandle, 0x80000000, STRM); //
        //msPause(20);
        //sendSPItoMAX(ftdiHandle, 0x10061B20, CLK); //
        //msPause(200);
        //sendSPItoMAX(ftdiHandle, 0x1E0F4010, TEST1); //
        //msPause(200);
        //sendSPItoMAX(ftdiHandle, 0x28C04020, TEST2); //
        //msPause(200);
        break;
      case 'l':
        LEDState = readGPIObyte(ftdiHandle, 1);
        toggleGPIOHighByte(ftdiHandle, LED); // LED Active Low
        LEDState = readGPIObyte(ftdiHandle, 1);
        break;
      default:
        if (ch == 'x')
          ch = 0x0A;
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
