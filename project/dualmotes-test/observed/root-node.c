/*
 * Copyright (c) 2012, Thingsquare, www.thingsquare.com.
 * All rights reserved.
 *
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
 */


#include "contiki.h"
#include "lib/random.h"
#include "sys/ctimer.h"
#include "sys/etimer.h"
#include "net/ipv6/uip.h"
#include "net/ipv6/uip-ds6.h"
#include "net/ipv6/uip-debug.h"
#include "net/routing/routing.h"
#include "net/ipv6/simple-udp.h"

#include "net/netstack.h"
#include "project-conf.h"

#include <stdio.h>
#include <string.h>

#include "sys/rtimer.h"
#include "sys/energest.h"
#include "dev/gpio.h"
#include "dev/ioc.h"

#include "../dualmotes.h"

#define UDP_PORT 1234
#define SERVICE_ID 190

#define SEND_INTERVAL		(10 * CLOCK_SECOND)
#define SEND_TIME		(random_rand() % (SEND_INTERVAL))

static struct simple_udp_connection unicast_connection;

/*Log configuration*/
#include "sys/log.h"

#define LOG_MODULE "App"
#define LOG_LEVEL LOG_LEVEL_INFO

#if ENERGEST_CONF_ON 
const linkaddr_t energest_addr = {{0x00, 0x12, 0x4b, 0x00, 0x00, 0x12, 0x4b, 0x00}}
#endif

uint16_t seqno = 0;
struct energestmsg prev_energest_vals;

/*---------------------------------------------------------------------------*/
PROCESS(unicast_receiver_process, "Unicast receiver example process");
AUTOSTART_PROCESSES(&unicast_receiver_process);
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
int state=0;
static void
receiver(struct simple_udp_connection *c,
         const uip_ipaddr_t *sender_addr,
         uint16_t sender_port,
         const uip_ipaddr_t *receiver_addr,
         uint16_t receiver_port,
         const uint8_t *data,
         uint16_t datalen)
{
  struct testmsg msg;
    memcpy(&msg, data, datalen);

    seqno++;

    msg.seqno=seqno;

    LOG_INFO("Received data from ");
    LOG_INFO_6ADDR(sender_addr);
    LOG_INFO_("\n");

    dualmotes_send(msg.seqno);

#if ENERGEST_CONF_ON
    if (seqno%ENERGEST_FREQ==0)
		send_energest();
#endif
 
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(unicast_receiver_process, ev, data)
{
  PROCESS_BEGIN();

  prev_energest_vals.cpu = 0;
  prev_energest_vals.lpm = 0;
  prev_energest_vals.transmit = 0;
  prev_energest_vals.listen = 0;
  prev_energest_vals.seqno = 0;
	prev_energest_vals.totaltime = 0;

  dualmotes_init();

  NETSTACK_ROUTING.root_start();

  simple_udp_register(&unicast_connection, UDP_PORT,
                      NULL, UDP_PORT, receiver);

  while(1) {
    PROCESS_YIELD();
  }
  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
