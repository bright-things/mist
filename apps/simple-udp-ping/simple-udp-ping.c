/*
 * Copyright (c) 2012, Thingsquare
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
 *
 * This file is part of the Contiki operating system.
 *
 */

#include "contiki.h"
#include "lib/random.h"
#include "sys/ctimer.h"
#include "sys/etimer.h"
#include "net/uip.h"
#include "net/uip-debug.h"
#include "net/uip-ds6.h"
#include "dev/leds.h"

#include "simple-udp.h"
#include "simple-udp-ping.h"

#include <stdio.h>
#include <string.h>

#define UDP_PORT 3117

static struct simple_udp_connection ping_connection;

#define MAX_DESTINATIONS UIP_DS6_NBR_NB

#define DEBUG 1

#define DATALEN 4

struct pingconn_t {
  uint8_t in_use;
  uint8_t waiting;
  uint8_t sent;
  uint8_t replied;
  rtimer_clock_t echo_time;
  clock_time_t echo_time2;
  uint32_t delay;
  uip_ipaddr_t host;
};
static struct pingconn_t pingconns[MAX_DESTINATIONS];

/*---------------------------------------------------------------------------*/
PROCESS(simple_udp_ping_process, "Simple ping over UDP");
/*---------------------------------------------------------------------------*/
static struct pingconn_t*
get_pingconn(const uip_ipaddr_t *addr)
{
  int i;
  for(i = 0; i < MAX_DESTINATIONS; i++) {
    if(!pingconns[i].in_use) {
      continue;
    }
    if(uip_ipaddr_cmp(&pingconns[i].host, addr)) {
      return &pingconns[i];
    }
  }
  return NULL;
}
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
#if DEBUG
  printf("simple-udp-ping: receiver: len %d\n", datalen);
#endif /* DEBUG */
  if(datalen == DATALEN && memcmp(data, "ping", 4) == 0) {
    /* Send back echo */
#if DEBUG
    printf("Sending echo to ");
    uip_debug_ipaddr_print(sender_addr);
    printf("\n");
#endif
    leds_toggle(LEDS_ALL);

    simple_udp_sendto(&ping_connection, "echo", DATALEN, sender_addr);
  } else if(datalen == DATALEN && memcmp(data, "echo", 4) == 0) {

    struct pingconn_t* pingconn = get_pingconn(sender_addr);

    if(pingconn != NULL) {
      pingconn->replied = 1;
      pingconn->sent = 0;
      if (clock_time() - pingconn->echo_time2 > CLOCK_SECOND) {
        pingconn->delay = clock_time() - pingconn->echo_time2;
        pingconn->delay *= RTIMER_SECOND;
        pingconn->delay /= CLOCK_SECOND;
      } else {
        pingconn->delay = RTIMER_NOW() - pingconn->echo_time;
      }
#if DEBUG
      printf("Received echo from ");
      uip_debug_ipaddr_print(sender_addr);
      printf(", delay ticks %lu\n", pingconn->delay);
#endif
    } else {
      printf("warning: received echo from unknown host\n");
    }
  } else {
    printf(
        "Error, unknown data  received on port %d from port %d with length %d\n",
        receiver_port, sender_port, datalen);
  }
}
/*---------------------------------------------------------------------------*/
static struct pingconn_t*
allocate_pingconn(uip_ipaddr_t *addr)
{
  static int last = MAX_DESTINATIONS;
  int i;

  for(i = 0; i < MAX_DESTINATIONS; i++) {
    if(!pingconns[i].in_use) {
      pingconns[i].in_use = 1;
      uip_ipaddr_copy(&pingconns[i].host, addr);
      return &pingconns[i];
    }
  }

  last++;
  if(last > MAX_DESTINATIONS) {
    last = 0;
  }

  pingconns[last].in_use = 1;
  uip_ipaddr_copy(&pingconns[last].host, addr);
  return &pingconns[last];
}
/*---------------------------------------------------------------------------*/
static void
free_pingconn(uip_ipaddr_t *addr)
{
  struct pingconn_t *pingconn = get_pingconn(addr);
  if(pingconn != NULL) {
    pingconn->in_use = 0;
  }
}
/*---------------------------------------------------------------------------*/
int
simple_udp_ping_send_ping(uip_ipaddr_t *addr)
{
  /* Get ping connection */
  struct pingconn_t* pingconn = get_pingconn(addr);
  if(pingconn == NULL) {
    pingconn = allocate_pingconn(addr);
  }

  pingconn->replied = 0;
  pingconn->waiting = 1;
  return -1;
}
/*---------------------------------------------------------------------------*/
void
simple_udp_ping_clear_conn(uip_ipaddr_t *addr)
{
  /* Get ping connection */
  struct pingconn_t* pingconn = get_pingconn(addr);
  if(pingconn == NULL) {
    return;
  }

  pingconn->replied = 0;
  pingconn->sent = 0;
}
/*---------------------------------------------------------------------------*/
int
simple_udp_ping_has_reply(uip_ipaddr_t *addr)
{
  struct pingconn_t *pingconn = get_pingconn(addr);
  if(pingconn == NULL) {
    return 0;
  }
  if(!pingconn->replied) {
    return 0;
  }
  return 1;
}
/*---------------------------------------------------------------------------*/
int
simple_udp_ping_has_sent(uip_ipaddr_t *addr)
{
  struct pingconn_t *pingconn = get_pingconn(addr);
  if(pingconn == NULL) {
    return 0;
  }
  if(pingconn->waiting) {
    return 1;
  }
  if(pingconn->sent) {
    return 1;
  }
  return 0;
}
/*---------------------------------------------------------------------------*/
int
simple_udp_ping_get_delay(uip_ipaddr_t *addr)
{
  uint32_t ms;

  struct pingconn_t *pingconn = get_pingconn(addr);
  if(pingconn == NULL) {
    return -1;
  }
  if(!pingconn->replied) {
    return -1;
  }
  ms = (uint32_t)1000 * (uint32_t)pingconn->delay;
  ms /= RTIMER_SECOND;
  return (int)ms;
}
/*---------------------------------------------------------------------------*/
void
simple_udp_ping_init(void)
{
  process_start(&simple_udp_ping_process, NULL);
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(simple_udp_ping_process, ev, data)
{
  static struct etimer et;
  int i;

  PROCESS_BEGIN();

  for(i = 0; i < MAX_DESTINATIONS; i++) {
    pingconns[i].in_use = 0;
  }

  simple_udp_register(&ping_connection, UDP_PORT, NULL, UDP_PORT, receiver);

  while(1) {
#define PERIOD (3*CLOCK_SECOND)
    etimer_set(&et, PERIOD);
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));

    for(i = 0; i < MAX_DESTINATIONS; i++) {
      if (pingconns[i].in_use && pingconns[i].waiting) {
        struct pingconn_t* pingconn = &pingconns[i];

        pingconn->waiting = 0;

        /* Send ping */
#if DEBUG
        printf("Sending ping to ");
        uip_debug_ipaddr_print(&pingconn->host);
        printf("\n");
#endif
        simple_udp_sendto(&ping_connection, "ping", DATALEN, &pingconn->host);
        pingconn->echo_time = RTIMER_NOW();
        pingconn->echo_time2 = clock_time();
        pingconn->sent = 1;
        pingconn->replied = 0;
        break;
      }
    }

  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
