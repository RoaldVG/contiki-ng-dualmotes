#ifndef PROJECT_CONF_H_
#define PROJECT_CONF_H_

#define LOG_LEVEL_CONF_MAC LOG_LEVEL_DBG

#include "stdint.h"

#define ENERGEST_CONF_ON 0
#define ENERGEST_FREQ 100

#define DUALMOTES_OBSERVED 1

#define BC 0 // Broadcast test or not

// UART pins are used for parallel communication, serial comm over UART overwrites some pins
#define UART_CONF_ENABLE 0
#define USB_SERIAL_CONF_ENABLE 0

/* Data structure of messages sent from sender
 *
 */
struct testmsg {       
	uint16_t  seqno;
	uint16_t  timestamp_app;
    uint16_t  timestamp_mac;
};

struct energestmsg {
    uint32_t    cpu;
    uint32_t    lpm;
    uint32_t    transmit;
    uint32_t    listen;
    uint16_t    seqno;
    uint32_t    totaltime;
};

#define TCPIP_CONF_ANNOTATE_TRANSMISSIONS 1
#define LOG_CONF_LEVEL_RPL LOG_LEVEL_INFO

#endif //PROJECT_CONF_H_
