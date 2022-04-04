/**
 * \file
 *          Unicast receiving test program
 *          Accepts messages from everybody
 * \author
 *         Marie-Paule Uwase
 *         August 13, 2012
 *         Tested and modified on September 8, 2012. Includes a useless but necessary timer!
 *         Writes with a fixed format in the serialdump to facilitate transfer of data to excel.
 *         Ack power is 0dBm, regardless of the sender settings (???).
 *         Version used for measurements on May 18, 2013 in Poix.
 *         Adapted by Maite Bezunartea to serve in the white (observer) sink of a dual network.
 *         Version used by JT in Poix on April 26, 2015.
 *         Refactored by JT on May 5, 2015
 *         
 *          
 */

/**
 * \file
 *          Unicast receiving test program
 *          Accepts messages from everybody
 * \author
 *         Marie-Paule Uwase
 *         August 13, 2012
 *         Roald Van Glabbeek
 * 		     March 3, 2020
 * 
 *         Updated for newer contiki release en Zolertia Zoul (firefly) and IPv6
 */

#include "contiki.h"
#include "sys/rtimer.h"
#include "project-conf.h"
#include "net/netstack.h"
#include "net/nullnet/nullnet.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

struct testmsg {
    uint16_t  observed_seqno;
    uint16_t monitor_seqno;
    uint32_t energy;
    uint16_t counter_ADC;
    rtimer_clock_t timestamp_app;
    rtimer_clock_t timestamp_mac;
};

/* Log configuration */
#include "sys/log.h"
#define LOG_MODULE "App"
#define LOG_LEVEL LOG_LEVEL_INFO

PROCESS(monitor_sink_process, "Monitor sink process");
AUTOSTART_PROCESSES(&monitor_sink_process);

// sender power
// possible values =  0dBm = 31;  -1dBm = 27;  -3dBm = 23;  -5dBm = 19; 
//                   -7dBm = 15; -10dBm = 11; -15dBm =  7; -25dBM =  3; 
//uint8_t power = 31;

// message counters
uint16_t received = 0 ;

// This is the receiver function
/*---------------------------------------------------------------------------*/
void input_callback(const void *data, uint16_t len,
  const linkaddr_t *src, const linkaddr_t *dest)
{
    rtimer_clock_t timestamp_app = RTIMER_NOW(); //received time (for the latency)
    rtimer_clock_t timestamp_mac;
    struct testmsg message;
    		
    memcpy(&message, data, len);
    received ++;
    NETSTACK_RADIO.get_object(RADIO_PARAM_LAST_PACKET_TIMESTAMP, &timestamp_mac, sizeof(rtimer_clock_t));
    //LOG_INFO_LLADDR(src);
    
    printf("%x%x,%" PRIu16 ",%" PRIu16 ",%" PRIu16 
            ",%" PRIu32 ",%" PRIu32 ",%" PRIu32 ",%" PRIu32 "\n\r", 
            src->u8[sizeof(src->u8)-2], src->u8[sizeof(src->u8)-1],
            received, message.observed_seqno, message.monitor_seqno,
            message.timestamp_app, message.timestamp_mac, timestamp_app, timestamp_mac);     
    
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(monitor_sink_process, ev, data)
{
    PROCESS_BEGIN();

    nullnet_set_input_callback(input_callback);

    while(1) {
        PROCESS_YIELD();
    }
    PROCESS_END();
}
/*---------------------------------------------------------------------------*/

