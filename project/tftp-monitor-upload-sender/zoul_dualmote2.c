#include "zoul_dualmote2.h"
#include "contiki.h"
#include "dev/gpio.h"
#include "dev/spi.h"

#include "sys/log.h"
/* Log configuration */
#define LOG_MODULE "ZOUL DUALMOTE"
#define LOG_LEVEL LOG_LEVEL_DBG

/* Zoul dualmote addresses and sizes */
#define ZOUL_DUALMOTE_START_ADDRESS               0x00200000
#define ZOUL_DUALMOTE_MEMORY_SIZE                 0x00078F00  //Do not erase the entire 0x00080000 space to keep flags
#define ZOUL_DUALMOTE_CODE_START_ADDRESS          0x00202000
#define ZOUL_DUALMOTE_ADDR_IEEE_ADDRESS_PRIMARY   0x00280028
#define ZOUL_DUALMOTE_TRANSFERSIZE 248


/**
 *  DUALMOTE SPI PIN CONFIG
 * */
#define RESET_GPIO_NUM (GPIO_A_NUM)
#define RESET_GPIO_PIN 5
#define CS_GPIO_NUM (GPIO_A_NUM)
#define CS_GPIO_PIN 3
static const spi_device_t observedSPI = {
.pin_spi_sck = GPIO_PORT_PIN_TO_GPIO_HAL_PIN(SPI1_CLK_PORT, SPI1_CLK_PIN), 
.pin_spi_miso = GPIO_PORT_PIN_TO_GPIO_HAL_PIN(SPI1_RX_PORT, SPI1_RX_PIN),
.pin_spi_mosi = GPIO_PORT_PIN_TO_GPIO_HAL_PIN(SPI1_TX_PORT, SPI1_TX_PIN),
.pin_spi_cs = GPIO_PORT_PIN_TO_GPIO_HAL_PIN(CS_GPIO_NUM,CS_GPIO_PIN),
.spi_bit_rate = 1e6, //higher than this results in transmission errors
.spi_pha = 0,
.spi_pol = 0,
.spi_controller = 1
};


//Small helpers to go from uint32_t to 4x uint8_t
#define CHCKSM32(x) (x.b0 + x.b1 + x.b2 + x.b3) 
union addr32 {
    uint32_t  b32;
    struct {
        uint8_t b3;
        uint8_t b2;
        uint8_t b1;
        uint8_t b0;
    };
};

#define ZOUL_DUALMOTE_COMMAND_PING 0x20
#define ZOUL_DUALMOTE_COMMAND_DOWNLOAD 0x21
#define ZOUL_DUALMOTE_COMMAND_RUN 0x22      //Not implemented
#define ZOUL_DUALMOTE_COMMAND_GET_STATUS  0x23
#define ZOUL_DUALMOTE_COMMAND_SEND_DATA 0x24
#define ZOUL_DUALMOTE_COMMAND_RESET 0x25
#define ZOUL_DUALMOTE_COMMAND_MEM_ERASE 0x26
#define ZOUL_DUALMOTE_COMMAND_CRC32 0x27    //Not implemented, CRC32 on Flash
#define ZOUL_DUALMOTE_COMMAND_GET_CHIP_ID 0x28
#define ZOUL_DUALMOTE_COMMAND_SET_OSC 0x29  //Not implemented
#define ZOUL_DUALMOTE_COMMAND_MEM_READ 0x2A
#define ZOUL_DUALMOTE_COMMAND_MEM_WRITE 0x2B

//Private functions
uint8_t zoul_dualmote_getack();
uint8_t zoul_dualmote_getack_cycle(uint32_t cycles);
uint8_t zoul_dualmote_calulatechecksum(uint8_t * buffer, uint8_t len);
uint8_t zoul_dualmote_getresponse(uint8_t * buffer, uint8_t buffersize);
uint8_t zoul_dualmote_get_status();
void zoul_dualmote_cmd_download(uint32_t startaddr, uint32_t len);
uint8_t zoul_dualmote_cmd_send(const uint8_t * data, uint8_t len);


//Public functions
//*******************************************

zoul_dualmote_response zoul_dualmote_init() {    
    if(spi_acquire(&observedSPI) == SPI_DEV_STATUS_OK) {
        return ZOUL_DUALMOTE_OK; 
    } else {
        return ZOUL_DUALMOTE_ERROR;
    }
}

void zoul_dualmote_release() {
    spi_deselect(&observedSPI); //Releases Chip Select
    spi_release(&observedSPI);  //Releases SPI
}

zoul_dualmote_response zoul_dualmote_reset() {
    LOG_INFO("RESET command triggered\n");

    const uint8_t reset [] = {0x03, ZOUL_DUALMOTE_COMMAND_RESET, ZOUL_DUALMOTE_COMMAND_RESET};
    uint8_t result;
    uint8_t attempts = 0;
    
    do {
        spi_write(&observedSPI, (const uint8_t *) &reset, 3);
        result = zoul_dualmote_getack();
        if(++attempts > 1) {
            LOG_DBG("RESET command: Not acked. Status code %02x, attempt %u\n", result, attempts);
        }
    } while (result != ZOUL_DUALMOTE_BOOTLOADER_ACK && attempts < 5);

    if(result != ZOUL_DUALMOTE_BOOTLOADER_ACK) {
        LOG_ERR("RESET command: Not acked after %u attempts. Status code %02x.\n", attempts, result);
    }
    LOG_DBG("RESET command: Status code: %02x. Releasing SPI interface and CS pin.\n", result);

    zoul_dualmote_release();

    return (result==ZOUL_DUALMOTE_BOOTLOADER_ACK) ? ZOUL_DUALMOTE_OK: ZOUL_DUALMOTE_ERROR;
}

zoul_dualmote_response zoul_dualmote_erase() {
    LOG_INFO("ERASE command triggered\n");

    union addr32 addr, size;
    addr.b32 = ZOUL_DUALMOTE_START_ADDRESS;
    size.b32 = ZOUL_DUALMOTE_MEMORY_SIZE;  //Full erase

	uint8_t checksum = ZOUL_DUALMOTE_COMMAND_MEM_ERASE + CHCKSM32(addr) + CHCKSM32(size);
    const uint8_t data[] = {11, checksum, ZOUL_DUALMOTE_COMMAND_MEM_ERASE, addr.b0, addr.b1, addr.b2, addr.b3, size.b0, size.b1, size.b2, size.b3};

    spi_write(&observedSPI, (const uint8_t *) &data, 11);

    //Takes lots of time to process! 10M cycles on 2MHz SPI = 5 second
    //TODO: make asynchronous? Add way to yield process?

    uint8_t result = zoul_dualmote_getack_cycle(10e6);  
    LOG_DBG("ERASE command: Status code: %02x\n", result);

    return (result==ZOUL_DUALMOTE_BOOTLOADER_ACK) ? ZOUL_DUALMOTE_OK: ZOUL_DUALMOTE_ERROR;
}

zoul_dualmote_response zoul_dualmote_download(uint16_t block, const uint8_t * data, uint16_t len) {
    LOG_INFO("DOWNLOAD function triggered for block %u\n", block);
    //Address based on TFTP block number (block size 512bytes)
    uint32_t startaddr = ZOUL_DUALMOTE_CODE_START_ADDRESS + (block-1)*512;

    uint16_t processedlength = 0;
    uint16_t remaininglength = len;
    while(len - processedlength >= ZOUL_DUALMOTE_TRANSFERSIZE) {
        zoul_dualmote_cmd_download(startaddr + processedlength, ZOUL_DUALMOTE_TRANSFERSIZE);

        zoul_dualmote_cmd_send(data, ZOUL_DUALMOTE_TRANSFERSIZE);
        data += ZOUL_DUALMOTE_TRANSFERSIZE;    //increase pointer position
        processedlength += ZOUL_DUALMOTE_TRANSFERSIZE;
        remaininglength -= ZOUL_DUALMOTE_TRANSFERSIZE;
    }

    if(remaininglength > 0){
        if((remaininglength)%4 != 0){
           LOG_ERR("Remaining length not a multiple of 4: %d\n", remaininglength);
           return ZOUL_DUALMOTE_ERROR;
        }
        zoul_dualmote_cmd_download(startaddr + processedlength, remaininglength);
        zoul_dualmote_cmd_send(data, remaininglength);
    }
    
    //TODO: make sure to prohibit writing near address 0x0027FFF8F00

    return ZOUL_DUALMOTE_OK; //TODO: add control features based on cmd_download and cmd_send!
}

zoul_dualmote_response zoul_dualmote_ping() {
    LOG_INFO("PING command triggered\n");

    const uint8_t ping [] = {0x03, ZOUL_DUALMOTE_COMMAND_PING, ZOUL_DUALMOTE_COMMAND_PING};
    spi_write(&observedSPI, (const uint8_t *) &ping, 3);
    uint8_t result = zoul_dualmote_getack();
    LOG_DBG("PING command: Status code: %02x\n", result);

    return (result==ZOUL_DUALMOTE_BOOTLOADER_ACK) ? ZOUL_DUALMOTE_OK: ZOUL_DUALMOTE_ERROR;
}

zoul_dualmote_response zoul_dualmote_togglebootloader() {
    LOG_INFO("Togglebootloader function triggered\n");

    if(!spi_has_bus(&observedSPI)) {
        if(spi_acquire(&observedSPI) != SPI_DEV_STATUS_OK) return ZOUL_DUALMOTE_ERROR;
    }
    spi_select(&observedSPI);  //Set CS pin

    GPIO_SET_OUTPUT(GPIO_PORT_TO_BASE(RESET_GPIO_NUM), GPIO_PIN_MASK(RESET_GPIO_PIN)); //
    GPIO_CLR_PIN(GPIO_PORT_TO_BASE(RESET_GPIO_NUM), GPIO_PIN_MASK(RESET_GPIO_PIN)); //Observed Reset low  -> Trigger reset
    LOG_DBG("SCS and RST pins triggered. Waiting 100ms to release RST.\n");

    clock_wait(CLOCK_SECOND/20); //wait 50ms
    GPIO_SET_PIN(GPIO_PORT_TO_BASE(RESET_GPIO_NUM), GPIO_PIN_MASK(RESET_GPIO_PIN)); //Observed Reset high   -> allows booting
    LOG_DBG("RST pin released. Observed node should be in bootloader mode.\n");

    return zoul_dualmote_ping();
}












//Private functions
//*******************************************

uint8_t zoul_dualmote_getack() {
    return zoul_dualmote_getack_cycle(50);
}

uint8_t zoul_dualmote_getack_cycle(uint32_t cycles) {
    uint8_t recv;
    uint32_t i = 0;
    do { 
    spi_read_byte(&observedSPI, &recv);
    i++;
    if(i%10000==0) watchdog_periodic();
    } while (recv == 0x00 && i < cycles);

  return recv;
}

uint8_t zoul_dualmote_calulatechecksum(uint8_t * buffer, uint8_t len){
    uint8_t checksum = 0;
    for (uint8_t i=0; i<len; i++){
        checksum += buffer[i];
    }
    return checksum & 0xFF;
}

uint8_t zoul_dualmote_getresponse(uint8_t * buffer, uint8_t buffersize) {
    uint8_t status;    
    uint8_t len;
    uint8_t checksum;

    status = spi_read_byte(&observedSPI, &len);
    if(status != SPI_DEV_STATUS_OK || len > buffersize) return 0;

    spi_read_byte(&observedSPI, &checksum);
    
    status = spi_read(&observedSPI, buffer, len-2);

    if(status == SPI_DEV_STATUS_OK && checksum == zoul_dualmote_calulatechecksum(buffer, len-2)) {
        LOG_DBG("GetResponse checksum matches. Sending ACK.\n");
        spi_write_byte(&observedSPI, ZOUL_DUALMOTE_BOOTLOADER_ACK);
        return len-2;
    } else {
        LOG_DBG("GetResponse checksum mismatch or other error. Sending NACK.\n");
        spi_write_byte(&observedSPI, ZOUL_DUALMOTE_BOOTLOADER_NACK);
        return 0;
    }
}

uint8_t zoul_dualmote_get_status() {
    
	const uint8_t status[] = {0x03, ZOUL_DUALMOTE_COMMAND_GET_STATUS, ZOUL_DUALMOTE_COMMAND_GET_STATUS};
	for(int i=0; i<=4;i++)
	{
        spi_write(&observedSPI, (const uint8_t *) &status, 3);
    }
    uint8_t result = zoul_dualmote_getack();
    LOG_DBG("Get Status ACK response: %02x\n", result);

	if(result == ZOUL_DUALMOTE_COMMAND_RET_SUCCESS)
		LOG_DBG("STATUS: Successful command.\n");
	else if(result == ZOUL_DUALMOTE_COMMAND_RET_UNKNOWN_CMD)
		LOG_DBG("STATUS: Unknown command.\n");
    else if(result == ZOUL_DUALMOTE_COMMAND_RET_INVALID_CMD)
		LOG_DBG("STATUS: Invalid command.\n");
	else if(result == ZOUL_DUALMOTE_COMMAND_RET_INVALID_ADR)
		LOG_DBG("STATUS: Invalid address.\n");
    else if(result == ZOUL_DUALMOTE_COMMAND_RET_FLASH_FAIL)
		LOG_DBG("STATUS: Flash fail.\n");
    else
		LOG_DBG("STATUS: Unknown status code.\n");

    return result;
}

void zoul_dualmote_cmd_download(uint32_t startaddr, uint32_t len) {
    LOG_DBG("CMD_DOWNLOAD for startaddr %08lx, length %lu\n", startaddr, len);

    union addr32 addr, s;
    addr.b32 = startaddr;
    s.b32 = len;

    uint8_t checksum = ZOUL_DUALMOTE_COMMAND_DOWNLOAD + CHCKSM32(addr) + CHCKSM32(s);
    uint8_t data[] = {11, checksum, ZOUL_DUALMOTE_COMMAND_DOWNLOAD, addr.b0, addr.b1, addr.b2, addr.b3, s.b0, s.b1, s.b2, s.b3};
    
    spi_write(&observedSPI, (const uint8_t *) &data, 11);
    uint8_t result = zoul_dualmote_getack();
    LOG_DBG("CMD_DOWNLOAD status: %02x\n", result);

    //zoul_dualmote_get_status();
}

uint8_t zoul_dualmote_cmd_send(const uint8_t * data, uint8_t len) {
    LOG_DBG("CMD_SEND for %p, length %d\n", data, len);
    if(len > 252){
        return 0;
    }

    uint8_t checksum = ZOUL_DUALMOTE_COMMAND_SEND_DATA;
    for(uint8_t i=0; i<len; i++){
        checksum += data[i];
    }

    uint8_t header[] = {len+3, checksum, ZOUL_DUALMOTE_COMMAND_SEND_DATA};
    spi_write(&observedSPI, (const uint8_t *) &header, 3);
    spi_write(&observedSPI, (const uint8_t *) data, len);
    //clock_wait(CLOCK_SECOND/10);
    uint8_t result = zoul_dualmote_getack_cycle(1e6); //1s
    LOG_DBG("CMD_SEND status: %02x\n", result);

    if(result == ZOUL_DUALMOTE_BOOTLOADER_ACK) {
        //zoul_dualmote_get_status();
    }
    else {
        LOG_DBG("CMD Send NACK! Retrying once...\n");

        spi_write(&observedSPI, (const uint8_t *) &header, 3);
        spi_write(&observedSPI, (const uint8_t *) data, len);
        //clock_wait(CLOCK_SECOND/10);
        result = zoul_dualmote_getack_cycle(1e6); //1s
       LOG_DBG("CMD Send retry status: %02x\n", result);

    }

    return result;
}

uint8_t zoul_dualmote_getchipid(uint8_t * buffer, uint8_t buffersize) {
    const uint8_t getchipid [] = {0x03, ZOUL_DUALMOTE_COMMAND_GET_CHIP_ID, ZOUL_DUALMOTE_COMMAND_GET_CHIP_ID};
    spi_write(&observedSPI, (const uint8_t *) &getchipid, 3);
    uint8_t result = zoul_dualmote_getack();
    LOG_DBG("GetChipID status: %02x\n", result);
    if(!result) {
        return 0;
    }
    uint8_t len = zoul_dualmote_getresponse(buffer, buffersize);
    LOG_DBG("CHIP ID: length %d   contents: ", len);
    for (int i=0; i<len; i++){
        LOG_DBG_("%02x ", buffer[i]);
    }
    LOG_DBG_("\n");

    return len;
}


// void zoul_dualmote_read() {  //Requires parameters!
// 	uint8_t checksum =0;
//     uint8_t byte0, byte1, byte2, byte3;
//     uint32_t addr = 0x00202000;
//     int j;
    
//     for(j=0; j<16; j++)
//     { 
//     byte3 = (addr >> 0) & 0xFF;
//     byte2 = (addr >> 8) & 0xFF;
//     byte1 = (addr >> 16) & 0xFF;
//     byte0 = (addr >> 24) & 0xFF;
     
//     checksum = (ZOUL_DUALMOTE_COMMAND_MEM_READ + byte0  + byte1 + byte2  + byte3  + 4) & 0xff ; 
//     const uint8_t data[] = {8, checksum, ZOUL_DUALMOTE_COMMAND_MEM_READ, byte0, byte1, byte2, byte3, 4};
//     spi_write(&observedSPI, (const uint8_t *) &data, 8);
//     uint8_t result = zoul_dualmote_getack();
//     LOG_DBG("Read status: %02x\n", result);
//     //return result;    
//     uint8_t buffer[20];
// 	uint8_t len;
//     len = zoul_dualmote_getresponse((uint8_t *)buffer, 20);
//     LOG_DBG("Length: %02x\n", len);
// 	for (int i=0; i<len; i++){
//         LOG_DBG_("%02x ", buffer[i]);
//     }
//     LOG_DBG_("\n");
// 	addr = addr+4;
//     }
// }

//These functions should call zoul_dualmote_read(...) if kept ...
// uint8_t zoul_dualmote_MAC_address_1() {
//     uint8_t byte0;
//     uint8_t byte1;
//     uint8_t byte2;
//     uint8_t byte3;
//     uint8_t len;
//     uint32_t addr = ADDR_IEEE_ADDRESS_PRIMARY;
//     //uint8_t ieee_addr[4];
//     //uint8_t ieee_addr_end[4];
//     //int i;
    
//     byte3 = (addr >> 0) & 0xFF;
//     byte2 = (addr >> 8) & 0xFF;
//     byte1 = (addr >> 16) & 0xFF;
//     byte0 = (addr >> 24) & 0xFF;
    
//     uint8_t checksum =0;  
//     checksum = (ZOUL_DUALMOTE_COMMAND_MEM_READ + byte0  + byte1 + byte2  + byte3  + 4) & 0xff ;
//     const uint8_t data[] = {8, checksum, ZOUL_DUALMOTE_COMMAND_MEM_READ, byte0, byte1, byte2, byte3, 4};
//     spi_write(&observedSPI, (const uint8_t *) &data, 8);
//     //wait(0.1);
//     uint8_t result = zoul_dualmote_getack();
//     LOG_DBG("MAC Address status: %02x\n", result);
	
// 	uint8_t buffer[20];
//     len = zoul_dualmote_getresponse((uint8_t *)buffer, 20);
//     LOG_DBG("MAC: length %d   contents: ", len);
//     for (int i=0; i<len; i++){
//         LOG_DBG_("%02x ", buffer[i]);
//     }
//     LOG_DBG_("\n");

// 	return len;
// }
// uint8_t zoul_dualmote_MAC_address_2()
// {
// 	uint8_t b0, b1, b2, b3;
// 	uint32_t addr2 = ADDR_IEEE_ADDRESS_PRIMARY+4;
// 	uint8_t len2;
	
//     b3 = (addr2 >> 0) & 0xFF;
//     b2 = (addr2 >> 8) & 0xFF;
//     b1 = (addr2 >> 16) & 0xFF;
//     b0 = (addr2 >> 24) & 0xFF;
    
//     uint8_t checksum2 =0;  
//     checksum2 = (ZOUL_DUALMOTE_COMMAND_MEM_READ + b0  + b1 + b2  + b3  + 4) & 0xff; 
//     const uint8_t data2[] = {8, checksum2, ZOUL_DUALMOTE_COMMAND_MEM_READ, b0, b1, b2, b3, 4};
//     spi_write(&observedSPI, (const uint8_t *) &data2, 8);
//     uint8_t result = zoul_dualmote_getack();
//     LOG_DBG("MAC Address status: %02x\n", result);
// 	uint8_t buffer2[20];
// 	len2 = zoul_dualmote_getresponse((uint8_t *)buffer2, 20);
//     LOG_DBG("MAC: length second %d   contents: ", len2);
//     for (int i=0; i<len2; i++){
//         LOG_DBG_("%02x ", buffer2[i]);
//     }
//     LOG_DBG_("\n");

// 	return len2;
// }

