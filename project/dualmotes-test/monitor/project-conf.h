#ifndef PROJECT_CONF_H_
#define PROJECT_CONF_H_

#include "stdint.h"
#include "sys/rtimer.h"

#undef ZOUL_CONF_USE_CC1200_RADIO
#define ZOUL_CONF_USE_CC1200_RADIO 1

#define DUALMOTES_OBSERVED 0

// UART pins are used for parallel communication, serial comm over UART overwrites some pins
#undef UART_CONF_ENABLE
#define UART_CONF_ENABLE 1
#undef USB_SERIAL_CONF_ENABLE
#define USB_SERIAL_CONF_ENABLE 1

#endif // PROJECT_CONF_H_
