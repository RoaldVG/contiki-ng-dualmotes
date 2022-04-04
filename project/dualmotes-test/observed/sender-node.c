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
#include "net/netstack.h"
#include "net/routing/routing.h"
#include "lib/random.h"
#include "sys/ctimer.h"
#include "sys/etimer.h"
#include "net/ipv6/uip.h"
#include "net/ipv6/uip-ds6.h"
#include "net/ipv6/uip-debug.h"

#include "project-conf.h"

#include "simple-udp.h"

#include "sys/etimer.h"
#include "sys/rtimer.h"
#include "sys/energest.h"
#include "dev/gpio.h"
#include "dev/ioc.h"

#include <stdio.h>
#include <string.h>

#include "../dualmotes.h"

/* Log configuration */
#include "sys/log.h"
#define LOG_MODULE "App"
#define LOG_LEVEL LOG_LEVEL_INFO

#define UDP_PORT 1234

#define AVERAGE_SEND_INTERVAL CLOCK_SECOND*60
#define RANDOM 0
#define MIN_SEND_INTERVAL 1
#define INTERVAL_RANGE (AVERAGE_SEND_INTERVAL - MIN_SEND_INTERVAL) * 2 

#if RANDOM
#define SEND_INTERVAL  ((INTERVAL_RANGE + random_rand()) % INTERVAL_RANGE) + MIN_SEND_INTERVAL
#else
#define SEND_INTERVAL  AVERAGE_SEND_INTERVAL
#endif

static struct simple_udp_connection unicast_connection;

uint16_t seqno=0;
struct energestmsg prev_energest_vals;

/*---------------------------------------------------------------------------*/
PROCESS(sender_node_process, "Sender node process");
AUTOSTART_PROCESSES(&sender_node_process);
/*---------------------------------------------------------------------------*/
static void
receiver(struct simple_udp_connection *c,
         const uip_ipaddr_t *sender_addr,
         uint16_t sender_port,
         const uip_ipaddr_t *receiver_addr,
         uint16_t receiver_port,
         const uint8_t *data,
         uint16_t datalen)
{
  LOG_INFO("Received data from ");
  LOG_INFO_6ADDR(sender_addr);
  LOG_INFO_("\n");
}
/*---------------------------------------------------------------------------*/
static void
set_global_address(void)
{
  uip_ipaddr_t ipaddr;
  int i;
  uint8_t state;
  const uip_ipaddr_t *default_prefix = uip_ds6_default_prefix();

  uip_ip6addr_copy(&ipaddr, default_prefix);
  uip_ds6_set_addr_iid(&ipaddr, &uip_lladdr);
  uip_ds6_addr_add(&ipaddr, 0, ADDR_AUTOCONF);

  printf("IPv6 addresses: ");
  for(i = 0; i < UIP_DS6_ADDR_NB; i++) {
    state = uip_ds6_if.addr_list[i].state;
    if(uip_ds6_if.addr_list[i].isused &&
       (state == ADDR_TENTATIVE || state == ADDR_PREFERRED)) {
      uip_debug_ipaddr_print(&uip_ds6_if.addr_list[i].ipaddr);
      printf("\n");
    }
  }
}
/*---------------------------------------------------------------------------*/
int state = 0;
static void
send_packet()
{
	struct testmsg msg;

  uip_ipaddr_t addr;
  static unsigned count;
 
	seqno++;

	/*Set general info*/
	msg.seqno=seqno;		
	msg.timestamp_app= clock_time();
	printf("called dualmotes, seqno %d\n",seqno);
	dualmotes_send(seqno);

  uint8_t *buf = (uint8_t*) &msg;

  if(NETSTACK_ROUTING.node_is_reachable() && NETSTACK_ROUTING.get_root_ipaddr(&addr)) {
      /* Send to DAG root */
      LOG_INFO("Sending request %u to ", count);
      LOG_INFO_6ADDR(&addr);
      LOG_INFO_("\n");
      
      simple_udp_sendto(&unicast_connection, buf, sizeof(buf), &addr); 
      count++;

      LOG_INFO("DATA sent to ");
	    LOG_INFO_6ADDR(&addr);
      LOG_INFO_("\n");

    } else {
      LOG_INFO("Not reachable yet\n");
    }

}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(sender_node_process, ev, data)
{
  static struct etimer periodic_timer;
  
  prev_energest_vals.cpu = 0;
  prev_energest_vals.lpm = 0;
  prev_energest_vals.transmit = 0;
  prev_energest_vals.listen = 0;
  prev_energest_vals.seqno = 0;
	prev_energest_vals.totaltime = 0;
  
  PROCESS_BEGIN();

  dualmotes_init();

  set_global_address();

  simple_udp_register(&unicast_connection, UDP_PORT,
                      NULL, UDP_PORT, receiver);

  etimer_set(&periodic_timer, SEND_INTERVAL);
  while(1) {

    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&periodic_timer));
    etimer_reset(&periodic_timer);
   	send_packet();
  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
