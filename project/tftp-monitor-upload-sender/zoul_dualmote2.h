#ifndef zoul_dualmote
#define zoul_dualmote

#include "contiki.h"

#define ZOUL_DUALMOTE_BOOTLOADER_ACK 0xCC
#define ZOUL_DUALMOTE_BOOTLOADER_NACK 0x33
#define ZOUL_DUALMOTE_BOOTLOADER_TIMEOUT 0x00

#define ZOUL_DUALMOTE_COMMAND_RET_SUCCESS 0x40
#define ZOUL_DUALMOTE_COMMAND_RET_UNKNOWN_CMD 0x41
#define ZOUL_DUALMOTE_COMMAND_RET_INVALID_CMD 0x42
#define ZOUL_DUALMOTE_COMMAND_RET_INVALID_ADR 0x43
#define ZOUL_DUALMOTE_COMMAND_RET_FLASH_FAIL 0x44

typedef enum
{
    ZOUL_DUALMOTE_OK,
    ZOUL_DUALMOTE_ERROR
} zoul_dualmote_response;


zoul_dualmote_response zoul_dualmote_init();
void zoul_dualmote_release();
zoul_dualmote_response zoul_dualmote_togglebootloader();
zoul_dualmote_response zoul_dualmote_erase();
zoul_dualmote_response zoul_dualmote_download(uint16_t block, const uint8_t * data, uint16_t len);
zoul_dualmote_response zoul_dualmote_reset();
zoul_dualmote_response zoul_dualmote_ping();


//Private or unused functions
//void zoul_dualmote_read();
//uint8_t zoul_dualmote_getchipid(uint8_t * buffer, uint8_t buffersize);

//uint8_t zoul_dualmote_get_status();
//uint8_t zoul_dualmote_MAC_address_1();
//uint8_t zoul_dualmote_MAC_address_2();
//uint8_t zoul_dualmote_getresponse(uint8_t * buffer, uint8_t buffersize);
//uint8_t zoul_dualmote_getack();
//uint8_t zoul_dualmote_calculatechecksum(uint8_t * buffer, uint8_t len);

#endif /* zoul_dualmote */
