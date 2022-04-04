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
 *         Unicast sending test program
 * \author
 *         Marie-Paule Uwase
 *         August 7, 2012
 *         Roald Van Glabbeek
 * 		   March 3, 2020
 * 
 *         Updated for newer contiki release en Zolertia Zoul (firefly) and IPv6
 */
#include "contiki.h"
#include "net/netstack.h"
#include "net/nullnet/nullnet.h"
#include "project-conf.h"
#include "lib/random.h"
#include <stdio.h>
#include <string.h>

#include "sys/etimer.h"
#include "sys/rtimer.h"
#include "sys/energest.h"
#include "dev/gpio.h"
#include "dev/ioc.h"

#include "../dualmotes.h"

#if MAC_CONF_WITH_TSCH
#include "net/mac/tsch/tsch.h"
#endif /* MAC_CONF_WITH_TSCH */

/* Log configuration */
#include "sys/log.h"
#define LOG_MODULE "App"
#define LOG_LEVEL LOG_LEVEL_INFO

#define AVERAGE_SEND_INTERVAL CLOCK_SECOND
#define RANDOM 0
#define MIN_SEND_INTERVAL 1
#define INTERVAL_RANGE (AVERAGE_SEND_INTERVAL - MIN_SEND_INTERVAL) * 2 

#if RANDOM
#define SEND_INTERVAL  ((INTERVAL_RANGE + random_rand()) % INTERVAL_RANGE) + MIN_SEND_INTERVAL
#else
#define SEND_INTERVAL  AVERAGE_SEND_INTERVAL
#endif

const linkaddr_t transm_addr = {{ 0x00, 0x12, 0x4b, 0x00, 0x18, 0xe6, 0x9d, 0x78 }};
const linkaddr_t recv_addr = {{ 0x00, 0x12, 0x4b, 0x00, 0x19, 0x32, 0xe5, 0x92}};


uint16_t seqno=0;
struct energestmsg prev_energest_vals;
/*---------------------------------------------------------------------------*/
PROCESS(observed_sender_process, "Observed sender process");
AUTOSTART_PROCESSES(&observed_sender_process);
/*---------------------------------------------------------------------------*/
void input_callback(const void *data, uint16_t len,
  const linkaddr_t *src, const linkaddr_t *dest)
{
    LOG_INFO("Received data from ");
    LOG_INFO_LLADDR(src);
    LOG_INFO_("\n");
}
/*---------------------------------------------------------------------------*/
int state = 0;
static void
send_packet()
{
	struct testmsg msg;

	seqno++;

	/*Set general info*/
	msg.seqno=seqno;		
	msg.timestamp_app= clock_time();
	printf("called dualmotes, seqno %d\n",seqno);
	dualmotes_send(seqno);

  	LOG_INFO("DATA sent to ");
	LOG_INFO_LLADDR(&recv_addr);
  	LOG_INFO_("\n");

 	nullnet_buf = (uint8_t *) &msg;
	nullnet_len = sizeof(msg);
#if BC
	NETSTACK_NETWORK.output(NULL);
#else
	NETSTACK_NETWORK.output(&recv_addr);
#endif
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(observed_sender_process, ev, data)
{
  	static struct etimer periodic;

  	prev_energest_vals.cpu = 0;
    prev_energest_vals.lpm = 0;
    prev_energest_vals.transmit = 0;
    prev_energest_vals.listen = 0;
    prev_energest_vals.seqno = 0;
	prev_energest_vals.totaltime = 0;
	
  	PROCESS_BEGIN();

#if MAC_CONF_WITH_TSCH
  	tsch_set_coordinator(0);
#endif /* MAC_CONF_WITH_TSCH */

	dualmotes_init();
  	nullnet_set_input_callback(input_callback);

	etimer_set(&periodic, SEND_INTERVAL);
	while(1) {
		PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&periodic));
		etimer_reset(&periodic);
		send_packet();
	}

  	PROCESS_END();
}
/*---------------------------------------------------------------------------*/
