/* Work in progress */

#include "contiki.h"

#include "project-conf.h"
#include "sys/rtimer.h"
#include "net/netstack.h"
#include "net/nullnet/nullnet.h"
#include "net/ipv6/uip.h"
#include "net/ipv6/uip-ds6.h"
#include "net/ipv6/uip-debug.h"

#include "dev/zoul-sensors.h"
#include "dev/adc-zoul.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "net/routing/routing.h"
#include "net/netstack.h"
#include "net/ipv6/simple-udp.h"
#include "net/ipv6/udp-socket.h"

#include "zoul_dualmote2.h"
#include "zoul_dualmote2_io.h"


#include "sys/ctimer.h"
//#include "sys/etimer.h"


/* Log configuration */
#include "sys/log.h"
#define LOG_MODULE "App"
#define LOG_LEVEL LOG_LEVEL_DBG

/*Monitor related structures */
/*---------------------------------------------------------------------------*/
/*
 * Interval between consecutive probes of the triger bit P1.0
 */

#define ADC_READ_INTERVAL (CLOCK_SECOND/128)

struct testmsg {
    uint16_t  observed_seqno;
    uint16_t monitor_seqno;
    uint32_t energy;
    uint16_t counter_ADC;
    rtimer_clock_t timestamp_app;
    rtimer_clock_t timestamp_mac;
};

/* 
 * Data structure of sent messages
 */
uint16_t  monitor_seqno=0;
uint32_t  ADCResult=0;
uint32_t  counter=0;
uint8_t   flag;

/* sender power
 * possible values =  0dBm = 31;  -1dBm = 27;  -3dBm = 23;  -5dBm = 19; 
 *                    -7dBm = 15; -10dBm = 11; -15dBm =  7; -25dBM =  3;
 */ 
uint8_t power = 31;

linkaddr_t sink_addr = {{ 0x00, 0x12, 0x4b, 0x00, 0x14, 0xb5, 0xde, 0x23 }};
/*---------------------------------------------------------------------------*/


#define WITH_SERVER_REPLY  1
#define UDP_SERVER_RESPONSE_PORT	420
#define UDP_SERVER_PORT	69

#define UDP_MONITOR_PORT 666


static struct udp_socket udp_tftp_mgmt_socket;
static struct udp_socket udp_tftp_session_socket;
static struct udp_socket udp_monitor_socket;

static uint16_t currentblocknr = 0;


//static struct ctimer timer;


/* Declare and auto-start this file's process */
PROCESS(udp_server_process, "UDP server");
PROCESS(monitor_sender_process, "Monitor sender");

AUTOSTART_PROCESSES(&udp_server_process,&monitor_sender_process);

static uint16_t tftp_ack(uint8_t * databuffer, uint16_t buffersize, uint16_t blocknr)
{
  if(buffersize < 4) {
    return 0;
  }
  databuffer[0] = 0x00;
  databuffer[1] = 0x04;
  databuffer[2] = blocknr>>8;
  databuffer[3] = blocknr&0xFF;
  return 4;
}

static uint16_t tftp_error(uint8_t * databuffer, uint16_t buffersize)
{
  if(buffersize < 5) {
    return 0;
  }
  databuffer[0] = 0x00; 
  databuffer[1] = 0x05; //0x00 05 ERROR
  databuffer[2] = 0x00;
  databuffer[3] = 0x00; //0x00 00 General error
  databuffer[4] = 0x00; //end
  return 5;
}


/*static void timer_callback(void * ptr)
{
  uint16_t blocknr = *(uint16_t *) ptr;
  static uint8_t mybuffer[10];
  uint16_t responseLength = tftp_ack(mybuffer, 10, blocknr);
  LOG_INFO("TFTP PARSER: ACKing WRQ block %d\n", blocknr);
  udp_socket_send(&udp_tftp_session_socket, mybuffer, responseLength);  
}*/


/*---------------------------------------------------------------------------*/
static void
udp_tftp_session_callback(struct udp_socket *c,
         void * nop,
         const uip_ipaddr_t *sender_addr,
         uint16_t sender_port,
         const uip_ipaddr_t *receiver_addr,
         uint16_t receiver_port,
         const uint8_t *data,
         uint16_t datalen)
{
  LOG_DBG("Received request '%.*s' from ", datalen, (char *) data);
  LOG_DBG_6ADDR(sender_addr);
  LOG_DBG_("\n");


  static uint8_t mybuffer[128];

  if(datalen < 4) {
    LOG_INFO("Receives misformed packet while parsing TFTP header. Sending error and closing socket\n");
    uint16_t responseLength = tftp_error(mybuffer, 128);
    udp_socket_send(c, mybuffer, responseLength);
    udp_socket_close(c);
  }

  if(data[0] == 0x00 && data[1] == 0x03 && 
    (data[2] != 0x00 || data[3] != 0x00))  //WRQ
  {
    static uint16_t blocknr;
    blocknr = data[2]<<8 | data[3];
    uint16_t contentlen = datalen-4;
    LOG_DBG("TFTP WRQ block %d, data len: %d\n", blocknr, contentlen);

    if(blocknr != ++currentblocknr) {
      LOG_INFO("Expected block %d but block received %d. Sending error and closing socket.\n", currentblocknr, blocknr);
      uint16_t responseLength = tftp_error(mybuffer, 128);
      udp_socket_send(c, mybuffer, responseLength);
      udp_socket_close(c);
      return;
    }

    LOG_INFO("Flashing page for block %d\n", blocknr);
    zoul_dualmote_download(blocknr, data+4, contentlen);



    uint16_t responseLength = tftp_ack(mybuffer, 128, blocknr);
    LOG_INFO("TFTP PARSER: ACKing WRQ block %d\n", blocknr);
    udp_socket_send(c, mybuffer, responseLength);
    
    //LOG_INFO("Setting a callback for %d seconds.\n", 70+blocknr);
    //ctimer_set(&timer, CLOCK_SECOND*(70+blocknr), timer_callback, (void *) &blocknr);


    if(contentlen == 512){
      LOG_DBG("Full data packet. Expecting another block.\n");
    }
    else if(data[datalen-1] == 0x00) {
      LOG_DBG("This is the last packet. Closing socket.\n");
      udp_socket_close(c);
      LOG_INFO("Restarting observed node.\n");
      zoul_dualmote_reset();
    }
    else {
      LOG_INFO("Received not full data packet of length %d, but last byte is not 0x00 but %02x .\n", contentlen, data[datalen-1]);
    }
  }

}
/*---------------------------------------------------------------------------*/
static void
udp_tftp_mgmt_callback(struct udp_socket *c,
         void * nop,
         const uip_ipaddr_t *sender_addr,
         uint16_t sender_port,
         const uip_ipaddr_t *receiver_addr,
         uint16_t receiver_port,
         const uint8_t *data,
         uint16_t datalen)
{
  LOG_INFO("Received request '%.*s' from ", datalen, (char *) data);
  LOG_INFO_6ADDR(sender_addr);
  LOG_INFO_("\n");
  // send back the same string to the client as an echo reply //
  //LOG_INFO("Sending response via TFTP port 69.\n");
  
  //Connect new socket first now content of *sender_addr hasn't changed yet!
  LOG_INFO("Registering and connecting new session socket.\n");

  int status = udp_socket_register(&udp_tftp_session_socket, NULL, udp_tftp_session_callback);
  LOG_DBG("Creating new socket. Status: %d\n", status);
  udp_socket_connect(&udp_tftp_session_socket, (uip_ipaddr_t *) sender_addr, sender_port);
  LOG_DBG("Connecting new socket to sender_addr and port. Status: %d\n", status);

  LOG_INFO("Putting observed node in bootloader mode.\n");
  
  zoul_dualmote_togglebootloader();

  currentblocknr = 0;
  static const uint8_t mydata[] = {0x00, 0x04, 0x00, 0x00};  //2B Ack 0x0004 2B block 0x0000
  int status2 = udp_socket_send(&udp_tftp_session_socket, mydata, sizeof(mydata));  
  LOG_INFO("new socket sendto status: %d\n", status2);

  zoul_dualmote_erase(); //This takes quite some time! Make sure to open new UDP socket first.
  LOG_DBG("Erase command complete\n");
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(udp_server_process, ev, data)
{
  PROCESS_BEGIN();
  
  int response = udp_socket_register(&udp_tftp_mgmt_socket, NULL, udp_tftp_mgmt_callback);
  LOG_INFO("Registering UDP TFTP socket: %d\n", response);
  response = udp_socket_bind(&udp_tftp_mgmt_socket, UDP_SERVER_PORT);
  LOG_INFO("Binding UDP TFTP socket: %d\n", response);  

  uint8_t status = zoul_dualmote_init();
  LOG_PRINT("SPI INIT STATUS: %d\n", status);

  PROCESS_END();
}

/*Monitor */
/*---------------------------------------------------------------------------*/
static void
send_packet( uint16_t seqno)
{
	static struct testmsg msg;
  
	monitor_seqno++;

  if(NETSTACK_ROUTING.node_is_reachable()) {
    NETSTACK_RADIO.get_object(RADIO_PARAM_LAST_PACKET_TIMESTAMP, &msg.timestamp_mac, sizeof(rtimer_clock_t));

    msg.observed_seqno = seqno;	
    msg.monitor_seqno = monitor_seqno;	
    msg.energy = ADCResult;
    msg.counter_ADC = counter;

    LOG_INFO("Data sent to sink ");
      LOG_INFO_LLADDR(&sink_addr);
      LOG_INFO_("\n");

    msg.timestamp_app = RTIMER_NOW();

    udp_socket_send(&udp_monitor_socket, &msg, sizeof(msg));
  }

  ADCResult=0;
  counter=0;
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
int prev_io_flag = 0;
PROCESS_THREAD(monitor_sender_process, ev, data)
{
  static struct etimer periodic;
	static struct etimer sendtimer;

  uip_ipaddr_t root_addr;

  static int sink_connected = 0;
	
  PROCESS_BEGIN();

  set_global_address();

	NETSTACK_RADIO.set_value(RADIO_PARAM_TXPOWER, power);

  counter = 0;

  int response = udp_socket_register(&udp_tftp_mgmt_socket, NULL, udp_tftp_mgmt_callback);
  LOG_INFO("Registering UDP TFTP socket: %d\n", response);
  response = udp_socket_bind(&udp_tftp_mgmt_socket, UDP_SERVER_PORT);
  LOG_INFO("Binding UDP TFTP socket: %d\n", response);  

  uint8_t status = zoul_dualmote_init();
  LOG_PRINT("SPI INIT STATUS: %d\n", status);

  int sink = udp_socket_register(&udp_monitor_socket, NULL, NULL);
  LOG_INFO("Registering UDP sink socket: %d\n", sink);
  dualmotes_init();

  etimer_set(&periodic, ADC_READ_INTERVAL);
	etimer_set(&sendtimer, CLOCK_SECOND);
	
  while(1) {
    PROCESS_YIELD();

    if(ev == PROCESS_EVENT_TIMER){
      if(data == &periodic){
        counter++;
        int ADC_val = adc_zoul.value(ZOUL_SENSORS_ADC2);
        ADCResult += ADC_val;
        etimer_reset(&periodic);	
        
      }
    }
    if(NETSTACK_ROUTING.node_is_reachable() && NETSTACK_ROUTING.get_root_ipaddr(&root_addr)) {
      if(!sink_connected) {
        sink = udp_socket_connect(&udp_monitor_socket, &root_addr, UDP_MONITOR_PORT);
        LOG_INFO("Binding UDP sink socket: %d\n", sink);  
        sink_connected = 1;
      }
      if(ev == gpio_flag_event) {
        uint16_t seqno = *(uint16_t*) data;
        send_packet(seqno);
      }
    }
  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
