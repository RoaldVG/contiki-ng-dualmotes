/*
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This file is part of the Contiki operating system.
 *
 */

/**
 * \file
 *         Unicast receiving test program
 *         Accepts messages from everybody
 * \author
 *         Marie-Paule Uwase
 *         August 13, 2012
 *         Roald Van Glabbeek
 * 		     March 3, 2020
 * 
 *         Updated for newer contiki release en Zolertia Zoul (firefly) and IPv6
 */
#include "contiki.h"
#include "net/netstack.h"
#include "net/nullnet/nullnet.h"
#include "project-conf.h"

#include <stdio.h>
#include <string.h>

#include "sys/etimer.h"
#include "sys/rtimer.h"
#include "sys/energest.h"
#include "dev/gpio.h"
#include "dev/ioc.h"

#if MAC_CONF_WITH_TSCH
#include "net/mac/tsch/tsch.h"
#endif /* MAC_CONF_WITH_TSCH */

/* Log configuration */
#include "sys/log.h"
#define LOG_MODULE "App"
#define LOG_LEVEL LOG_LEVEL_INFO

#include "../dualmotes.h"

#if ENERGEST_CONF_ON
const linkaddr_t energest_addr = {{ 0x00, 0x12, 0x4b, 0x00, 0x14, 0xd5, 0x2d, 0xe1 }};
#endif

uint16_t seqno=0;
struct energestmsg prev_energest_vals;
/*---------------------------------------------------------------------------*/
PROCESS(observed_receiver_process, "Observed receiver process");
AUTOSTART_PROCESSES(&observed_receiver_process);
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
#if ENERGEST_CONF_ON
static void
send_energest()
{
    struct energestmsg energest_values;

    energest_values.totaltime = RTIMER_NOW() - prev_energest_vals.totaltime;

    energest_flush();
    energest_values.cpu = energest_type_time(ENERGEST_TYPE_CPU) - prev_energest_vals.cpu;
    energest_values.lpm = energest_type_time(ENERGEST_TYPE_LPM) - prev_energest_vals.lpm;
    energest_values.transmit = energest_type_time(ENERGEST_TYPE_TRANSMIT) - prev_energest_vals.transmit;
    energest_values.listen = energest_type_time(ENERGEST_TYPE_LISTEN) - prev_energest_vals.listen;
    energest_values.seqno = seqno;

    LOG_INFO("Energest data sent to ");
	LOG_INFO_LLADDR(&energest_addr);
  	LOG_INFO_("\n");

    nullnet_buf = (uint8_t *) &energest_values;
    nullnet_len = sizeof(energest_values);

	NETSTACK_NETWORK.output(&energest_addr);

    energest_flush();
    prev_energest_vals.cpu = energest_type_time(ENERGEST_TYPE_CPU);
    prev_energest_vals.lpm = energest_type_time(ENERGEST_TYPE_LPM);
    prev_energest_vals.transmit = energest_type_time(ENERGEST_TYPE_TRANSMIT);
    prev_energest_vals.listen = energest_type_time(ENERGEST_TYPE_LISTEN);
    prev_energest_vals.totaltime = RTIMER_NOW();
}
#endif /* ENERGEST_CONF_ON */
/*---------------------------------------------------------------------------*/
int state = 0;
void input_callback(const void *data, uint16_t len,
  const linkaddr_t *src, const linkaddr_t *dest)
{
    struct testmsg msg;
    memcpy(&msg, data, len);

    seqno++;

    msg.seqno=seqno;

    LOG_INFO("Received data from ");
    LOG_INFO_LLADDR(src);
    LOG_INFO_("\n");


    dualmotes_send(msg.seqno);

#if ENERGEST_CONF_ON
    if (seqno%ENERGEST_FREQ==0)
		send_energest();
#endif
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(observed_receiver_process, ev, data)
{

    PROCESS_BEGIN();

  	prev_energest_vals.cpu = 0;
    prev_energest_vals.lpm = 0;
    prev_energest_vals.transmit = 0;
    prev_energest_vals.listen = 0;
    prev_energest_vals.seqno = 0;
	prev_energest_vals.totaltime = 0;

#if MAC_CONF_WITH_TSCH
  	tsch_set_coordinator(0);
#endif /* MAC_CONF_WITH_TSCH */

    dualmotes_init();

  	nullnet_set_input_callback(input_callback);

    while(1) {
        PROCESS_YIELD();
    }
    PROCESS_END();
}
/*---------------------------------------------------------------------------*/
