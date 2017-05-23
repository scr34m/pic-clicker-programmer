/* Protocol Description.

  UHB protocol is a typical master-slave communication protocol, where
  master (PC) sends commands and slave (bootloader equiped device) executes
  them and aknowledges execution.

  * Command format.

    <STX[0]><CMD_CODE[0]><ADDRESS[0..3]><COUNT[0..1]> <DATA[0..COUNT-1]>
    |-- 1 --|---- 1 -----|------ 4 -----|----- 2 ----|------ COUNT -----|

    STX      - Command start delimiter (for future upgrades).
               Length: 1 byte. Mandatory.
    CMD_CODE - Command index (TCmd).
               Length: 1 byte. Mandatory.
    ADDRESS  - Address field. Flash start address for
               CMD_CODE command operation.
               Length: 4 bytes. Optional (command specific).
    COUNT    - Count field. Amount of data/blocks for
               CMD_CODE command operation.
               Length: 2 bytes. Optional (command specific).
    DATA     - Data array.
               Length: COUNT bytes. Optional (command specific).

  Some commands do not utilize all of these fields.
  See 'Command Table' below for details on specific command's format.

  * Command Table.
   --------------------------+---------------------------------------------------
  |       Description        |                      Format                       |
  |--------------------------+---------------------------------------------------|
  | Synchronize with PC tool |                  <STX><cmdSYNC>                   |
  |--------------------------+---------------------------------------------------|
  | Send bootloader info     |                  <STX><cmdINFO>                   |
  |--------------------------+---------------------------------------------------|
  | Go to bootloader mode    |                  <STX><cmdBOOT>                   |
  |--------------------------+---------------------------------------------------|
  | Restart MCU              |                  <STX><cmdREBOOT>                 |
  |--------------------------+---------------------------------------------------|
  | Write to MCU flash       | <STX><cmdWRITE><START_ADDR><DATA_LEN><DATA_ARRAY> |
  |--------------------------+---------------------------------------------------|
  | Erase MCU flash.         |  <STX><cmdERASE><START_ADDR><ERASE_BLOCK_COUNT>   |
   ------------------------------------------------------------------------------

  * Acknowledge format.

    <STX[0]><CMD_CODE[0]>
    |-- 1 --|---- 1 -----|

    STX      - Response start delimiter (for future upgrades).
               Length: 1 byte. Mandatory.
    CMD_CODE - Index of command (TCmd) we want to acknowledge.
               Length: 1 byte. Mandatory.

  See 'Acknowledgement Table' below for details on specific command's
  acknowledgement process.

  * Acknowledgement Table.
   --------------------------+---------------------------------------------------
  |       Description        |                   Acknowledgement                 |
  |--------------------------+---------------------------------------------------|
  | Synchronize with PC tool |                  upon reception                   |
  |--------------------------+---------------------------------------------------|
  | Send bootloader info     |          no acknowledge, just send info           |
  |--------------------------+---------------------------------------------------|
  | Go to bootloader mode    |                  upon reception                   |
  |--------------------------+---------------------------------------------------|
  | Restart MCU              |                  no acknowledge                   |
  |--------------------------+---------------------------------------------------|
  | Write to MCU flash       | upon each write of internal buffer data to flash  |
  |--------------------------+---------------------------------------------------|
  | Erase MCU flash.         |                  upon execution                   |
   ------------------------------------------------------------------------------*/

#define STX 0x0F // Start of TeXt.
#define ETX 0x04 // End of TeXt.
#define DLE 0x05 // Data Link Escape.

// Supported MCU families/types.
enum TMcuType {mtPIC16 = 1, mtPIC18, mtPIC18FJ, mtPIC24, mtDSPIC = 10, mtPIC32 = 20};

// Bootloader info field ID's.
enum TBootInfoField {bifMCUTYPE = 1, // MCU type/family.
                     bifMCUID,      // MCU ID number.
                     bifERASEBLOCK, // MCU flash erase block size.
                     bifWRITEBLOCK, // MCU flash write block size.
                     bifBOOTREV,    // Bootloader revision.
                     bifBOOTSTART,  // Bootloader start address.
                     bifDEVDSC,     // Device descriptor string.
                     bifMCUSIZE     // MCU flash size
                    };

// Suported commands.
enum TCmd {cmdNON = 0,              // 'Idle'.
           cmdSYNC,                 // Synchronize with PC tool.
           cmdINFO,                 // Send bootloader info record.
           cmdBOOT,                 // Go to bootloader mode.
           cmdREBOOT,               // Restart MCU.
           cmdWRITE = 11,           // Write to MCU flash.
           cmdERASE = 21
          };            // Erase MCU flash.

/* Bootloader info field types */

// Byte field (1 byte).
typedef struct __attribute__((packed)) {
	uint8_t fFieldType;
	uint8_t fValue;
} TCharField;

// Int field (2 bytes).
typedef struct __attribute__((packed)) {
	uint8_t fFieldType;
	uint16_t fValue;
} TUIntField;

// Long field (4 bytes).
typedef struct __attribute__((packed)) {
	uint8_t fFieldType;
	uint32_t fValue;
} TULongField;

// String field (MAX_STRING_FIELD_LENGTH bytes).
#define MAX_STRING_FIELD_LENGTH 20
typedef struct __attribute__((packed)) {
	uint8_t fFieldType;
	char fValue[MAX_STRING_FIELD_LENGTH];
} TStringField;

// Bootloader info record (device specific information).
typedef struct __attribute__((packed)) {
	uint8_t bSize;
	TCharField   bMcuType;
	TULongField  ulMcuSize;
	TUIntField   uiEraseBlock;
	TUIntField   uiWriteBlock;
	TUIntField   uiBootRev;
	TULongField  ulBootStart;
	TStringField sDevDsc;
} TBootInfo;
