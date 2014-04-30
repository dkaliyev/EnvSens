/**
 * \file
 *         Implements sensor node
 * \author
 *         Daniyar Kaliyev
 */


#include "contiki.h"
#include "lib/list.h"
#include "lib/memb.h"
#include "lib/random.h"
#include "net/rime.h"
#include "io-pins.h"
#include "dev/leds.h"
#include "RTC.h"
#include <string.h>
#include <stdio.h>

 #define INIT_REQ 0
 #define DATA_REQ 1
 #define INIT_RESP 2

 #define FLASH_LED(l) {leds_on(l); clock_delay_msec(500); leds_off(l); clock_delay_msec(50);}

struct sensor_data {
	uint8_t lraw[60];
  	uint8_t hraw[60];
};

/* This is the structure of broadcast messages. */
struct broadcast_message {
  	struct RTC_time time;
	uint8_t type;
	uint8_t cmd;
	uint8_t node_id;
	uint8_t sender_id;
};

/* This is the structure of unicast ping messages. */
struct unicast_message {
  char name[20];
  uint8_t id;
  struct sensor_data data;
  uint8_t numSampl;
  uint8_t type;
  struct RTC_time time;
};


/* This structure holds information about neighbors. */
struct neighbor {
  /* The ->next pointer is needed since we are placing these on a
     Contiki list. */
  struct neighbor *next;

  /* The ->addr field holds the Rime address of the neighbor. */
  rimeaddr_t addr;

  uint8_t id;
};

/* This #define defines the maximum amount of neighbors we can remember. */
#define MAX_NEIGHBORS 16

/* This MEMB() definition defines a memory pool from which we allocate
   neighbor entries. */
MEMB(neighbors_memb, struct neighbor, MAX_NEIGHBORS);

/* The neighbors_list is a Contiki list that holds the neighbors we
   have seen thus far. */
LIST(neighbors_list);

/* These hold the broadcast and unicast structures, respectively. */
static struct broadcast_conn broadcast;
static struct unicast_conn unicast;
//static rimeaddr_t from;
static rimeaddr_t nmaddr;
static uint8_t id = 0;

static struct RTC_alarm q = {
		10,		// seconds
		35,		// minutes
		12,		// hours
		30,		// day
		01		// month
};

static uint8_t status = 0; // 1 waiting for response, 0 - response received

/*---------------------------------------------------------------------------*/
/* We first declare our three processes. */
PROCESS(unicast_process, "Unicast process");
PROCESS(broadcast_process, "Broadcast process");
PROCESS(time_counter, "RTC counter");


AUTOSTART_PROCESSES(&unicast_process, &broadcast_process/*, &time_counter*/);
/*---------------------------------------------------------------------------*/

static void alarmCallback()
{
	static struct RTC_time t_now;
	static struct unicast_message *tmp, sMsg;
	static struct sensor_data sens_data;
	static uint16_t samplingDelay = 280;
	static uint16_t delay = 40; // ADC requires minimum 20 us of sampling time.
	static uint16_t sleepDelay = 9680;
	static uint16_t avg;
	static uint8_t count = 0;
	if(count==0)
	RTC_getTime(&t_now);
	int len;
	uint8_t i, numSampl = 20;

	static uint16_t v = 0, v1;

	ioPins_configurePin(8, USEGPIO, OUTPUT, NOPULLUP, HYSTERESIS_OFF);
	ioPins_configurePin(7, USEGPIO, OUTPUT, NOPULLUP, HYSTERESIS_OFF);
	ioPins_setValue(8, 1);
	ioPins_setValue(7, 1);
	

	avg = 0;
	for(i=0;i<numSampl;i++)
	{
		ioPins_setValue(8, 0);
		clock_delay_usec(samplingDelay);
		v = ioPins_getValue(16);
		avg+=v;
		clock_delay_usec(delay);
		ioPins_setValue(8, 1);
		clock_delay_usec(sleepDelay);
		
	}
	
	sens_data.lraw[count] = avg&0xFF;
	sens_data.hraw[count] = (avg&0xFF00)>>8;
	
  	if(count==59)
  	{
  		sMsg.id = id;
	  	sMsg.type = DATA_REQ;
	  	sMsg.time = t_now;
	  	strcpy(sMsg.name, "Dust sensor\0");
	  	sMsg.data = sens_data;
	  	sMsg.numSampl = numSampl;
	  	packetbuf_copyfrom(&sMsg, sizeof(struct unicast_message));
		unicast_send(&unicast, &nmaddr);
		count = 0;
	}
	else
	{
		count++;
	}
}

/* This function is called whenever a broadcast message is received. */
static void
broadcast_recv(struct broadcast_conn *c, const rimeaddr_t *from)
{
  struct broadcast_message *m;
  struct unicast_message sMsg;
  struct RTC_time t, t_now;
  rimeaddr_t *add;
  struct sensor_data tk;
  
  m = (struct broadcast_message *)packetbuf_dataptr();

  t = m->time;

  if(id==0&&status==0)
  {
  	rimeaddr_copy(&nmaddr, from);
  	sMsg.type = INIT_REQ;
  	sMsg.time = t;
  	sMsg.numSampl = 20;
  	strcpy(sMsg.name, "Dust sensor\0");
  	packetbuf_copyfrom(&sMsg, sizeof(struct unicast_message));
  	RTC_getTime(&t_now);
  	unicast_send(&unicast, &nmaddr);
  	broadcast_close(&broadcast);
  	status = 1;
  	return;
  }

  if(m->type == 1)
  {
  	t = m->time;
  }
}

static const struct broadcast_callbacks broadcast_call = {broadcast_recv};

static void startReading()
{
  struct unicast_message msg, *tmp;
  rimeaddr_t *add;
  struct RTC_time time;

  while(1)
  {
  	 msg.id = id;
  	 msg.type = DATA_REQ;
  	 RTC_getTime(&time);
  	 msg.time = time;
  	 packetbuf_copyfrom(&msg, sizeof(struct unicast_message));
  	 unicast_send(&unicast, &nmaddr);
  	 clock_delay_msec(1000);
  }
}
/*---------------------------------------------------------------------------*/
/* This function is called for every incoming unicast packet. */
static void
recv_uc(struct unicast_conn *c, const rimeaddr_t *from)
{
  struct RTC_time t_now;
  struct unicast_message *msg, *tmp, sMsg;
  rimeaddr_t *add;
  /* The packetbuf_dataptr() returns a pointer to the first data byte
     in the received packet. */
  msg = (struct unicast_message *)packetbuf_dataptr();
  if(msg->type==INIT_RESP)
  {
  	rimeaddr_copy(&nmaddr, from);
  	id = msg->id;
  	t_now = msg->time;
  	RTC_setTime(&t_now);
  	q.month = t_now.month;
	q.day = t_now.day;
	q.hours = 18;
	q.minutes = 30;
	q.seconds = 0;
	ioPins_init();
	RTC_setAlarm(&q, alarmCallback, RPT_SECOND);
  }
}


static const struct unicast_callbacks unicast_callbacks = {recv_uc};
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(broadcast_process, ev, data)
{
  static struct etimer et;
  struct broadcast_message msg;
  struct RTC_time t;

  PROCESS_EXITHANDLER(broadcast_close(&broadcast);)

  PROCESS_BEGIN();

  // Initialize clock with random values
  t.hundredths = 5;
  t.tenths = 9;
  t.seconds =	0,
  t.minutes =	0,
  t.hours = 19;
  t.day =	5;
  t.month = 03;
  t.year = 14;

  RTC_setTime(&t);

  broadcast_open(&broadcast, 129, &broadcast_call);
  while(0) {
    
    etimer_set(&et, CLOCK_SECOND * 7);

    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
    
    RTC_getTime(&t);

    msg.time = t;
    msg.type = 1; // type 1 is sending current time;
    msg.node_id = 0; // set node to 0 to send everyone
    msg.sender_id = id; // if 0, sender is node manager
    msg.cmd = 0; 
    packetbuf_copyfrom(&msg, sizeof(struct broadcast_message));
    broadcast_send(&broadcast);
    process_exit(&broadcast_process);
  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(unicast_process, ev, data)
{
  PROCESS_EXITHANDLER(unicast_close(&unicast);)
    
  PROCESS_BEGIN();

  unicast_open(&unicast, 146, &unicast_callbacks);

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/

// Helper process to visualize time synchronization
PROCESS_THREAD(time_counter, ev, data)
{
	PROCESS_BEGIN();
	static struct etimer et;
	struct RTC_time t;
	while(1)
	{
		etimer_set(&et, CLOCK_SECOND);
		PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
		RTC_getTime(&t);
    	printf("Time counter: %02d/%02d/%02d %02d:%02d:%02d.%d%d\n", t.day, t.month, t.year, t.hours, t.minutes, t.seconds, t.tenths, t.hundredths);
	}
	PROCESS_END();
}