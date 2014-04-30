/**
 * \file
 *         Implements Node manager
 * \author
 *         Daniyar Kaliyev
 */


#include "contiki.h"
#include "contiki-uart.h"
#include "mc1322x.h"
#include "board.h"
#include "lib/list.h"
#include "lib/memb.h"
#include "lib/random.h"
#include "net/rime.h"
#include "dev/leds.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "RTC.h"

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
  uint8_t type;
  struct RTC_time time; 
  uint8_t numSampl;;
};

/* These are the types of unicast messages that we can send. */


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
static uint8_t processed;
static char readings[10][50];


/*---------------------------------------------------------------------------*/
/* We first declare our two processes. */
PROCESS(broadcast_process, "Broadcast process");
PROCESS(unicast_process, "Unicast process");
PROCESS(uart_process, "Uart process");
PROCESS(time_counter, "RTC counter");

/* The AUTOSTART_PROCESSES() definition specifices what processes to
   start when this module is loaded. We put both our processes
   there. */
AUTOSTART_PROCESSES(&broadcast_process, &unicast_process, &uart_process, &time_counter);

/*---------------------------------------------------------------------------*/

/* This function returns the difference between 2 time stamps*/

static void adjust_time(struct RTC_time t_now, struct RTC_time *to_send)
{
	uint8_t carry = 1;
	if(t_now.tenths>6) carry++; // If tenths > 8 then add 1 second to time to send
  	to_send->hundredths = 0;
  	to_send->tenths = 0;
  	to_send->seconds = (t_now.seconds + (t_now.seconds - to_send->seconds) + carry) % 60;
  	to_send->minutes = t_now.minutes + (t_now.minutes - to_send->minutes);
  	to_send->hours = t_now.hours + (t_now.hours - to_send->hours);
  	to_send->day = t_now.day + (t_now.day - to_send->day);
  	to_send->month = t_now.month + (t_now.month - to_send->month);
  	to_send->year = t_now.year + (t_now.year - to_send->year);
}


static void alarmCallback()
{
	FLASH_LED(LEDS_BLUE);
}

/* This function is called whenever a broadcast message is received. */
static void
broadcast_recv(struct broadcast_conn *c, const rimeaddr_t *from)
{
  struct neighbor *n;
  struct broadcast_message *m;

  /* The packetbuf_dataptr() returns a pointer to the first data byte
     in the received packet. */
  m = (struct broadcast_message *)packetbuf_dataptr();

}
/* This is where we define what function to be called when a broadcast
   is received. We pass a pointer to this structure in the
   broadcast_open() call below. */
static const struct broadcast_callbacks broadcast_call = {broadcast_recv};
/*---------------------------------------------------------------------------*/
/* This function is called for every incoming unicast packet. */
static void
recv_uc(struct unicast_conn *c, const rimeaddr_t *from)
{
  static struct unicast_message *rMsg, sMsg;
  static struct neighbor *n;
  static struct RTC_time t_now, to_send;
  static struct sensor_data reading;
  static uint8_t count = 60;
  char date_string[30];
  char packetStr[100];
  char str[50];
 
  static uint8_t id=1;
  uint8_t j;
  uint8_t found;
  int cp, len;
  /* The packetbuf_dataptr() returns a pointer to the first data byte
     in the received packet. */
  rMsg = (struct unicast_message *)packetbuf_dataptr();

  if(rMsg->type == INIT_REQ) 
  {

    RTC_getTime(&t_now);
    to_send = rMsg->time;
   
  	adjust_time(t_now, &to_send);
  	printf("Time adjusted: %02d/%02d/%02d %02d:%02d:%02d.%d%d\n", to_send.day, to_send.month, to_send.year, to_send.hours, to_send.minutes, to_send.seconds, to_send.tenths, to_send.hundredths);
	/* Check if we already know this neighbor. */
  	for(n = list_head(neighbors_list); n != NULL; n = list_item_next(n)) {

	    /* We break out of the loop if the address of the neighbor matches
	       the address of the neighbor from which we received this
	       broadcast message. */
	    if(rimeaddr_cmp(&n->addr, from)) {
	      break;
	    }
  	}

  /* If n is NULL, this neighbor was not found in our list, and we
     allocate a new struct neighbor from the neighbors_memb memory
     pool. */
  if(n == NULL) {
    n = memb_alloc(&neighbors_memb);

    /* If we could not allocate a new neighbor entry, we give up. We
       could have reused an old neighbor entry, but we do not do this
       for now. */
    if(n == NULL) {
      printf("%s\n", "Cant allocate space! Quiting");
      return;
    }

    /* Initialize the fields. */
    rimeaddr_copy(&n->addr, from);
    n->id = id;
    id++;
    /* Place the neighbor on the neighbor list. */
    list_add(neighbors_list, n);
  }
  
  /* We can now fill in the fields in our neighbor entry. */

  
  sMsg.id = n->id;
  sMsg.type = INIT_RESP;
  sMsg.time = to_send;
  sMsg.data = rMsg->data;
  
  cp = packetbuf_copyfrom(&sMsg, sizeof(struct unicast_message));

  unicast_send(&unicast, from);
  FLASH_LED(LEDS_GREEN);
  FLASH_LED(LEDS_GREEN);
  
  }
  else
  {
  	if(rMsg->type==DATA_REQ)
  	{
  		uint8_t l, hg, numCycles;
  		uint8_t y, m, d, h, min, s;
  		uint16_t r;
  		to_send = rMsg->time;
  		reading = rMsg->data;

  		y = to_send.year;
  		m = to_send.month;
  		d = to_send.day;
  		h = to_send.hours;
  		min = to_send.minutes;
  		s = to_send.seconds;

  		for(j=0;j<count;j++)
  		{
  			//printf("Raw %u\n", rMsg->raw);
	  		l = reading.lraw[j];
	  		hg = reading.hraw[j];
	  		r = (hg<<8)|l;
	  		strcpy(date_string, '\0');
	  		strcpy(str, '\0');
	  		sprintf(date_string, "20%02d-%02d-%02dT%02d:%02d:%02dZ", y, m, d, h, min, s);
	  		strcpy(packetStr, "\0");
			sprintf(packetStr, "{\"Raw\": %u, \"numSampl\": %u, \"date\":\"%s\", \"SensId\":%u}\0", r, rMsg->numSampl;, date_string, rMsg->id);
	  		sprintf(str, "%s\n", packetStr);
			len = strlen(str);	
			for(j=0;j<len;j++)
			{
				uart2_putc(str[j]);
			}

			s = (s+1)%60;
			if(s==0) m = (min+1)%60;
			if(min==0) h = (h+1)%24;
			if(h==0)
			{
				switch(m)
				{
					case 1:
					case 3:
					case 5:
					case 7:
					case 8:
					case 10:
					case 12:
					d = (d+1)%32;
					if(d==0) d++;
					break;
					case 2:
					d = (d+1)%29;
					if(d==0) d++;
					break;
					case 4:
					case 6:
					case 9:
					case 11:
					d = (d+1)%31;
					if(d==0) d++;
				}
			}
			if(d==1) m = (m+1)%13;
			if(m==0) m++;
			if(m==1) y++; 
		}
  	}
  }

}
static const struct unicast_callbacks unicast_callbacks = {recv_uc};

static
void parse_date(const char *str, uint8_t *year, uint8_t *month, uint8_t *day, uint8_t *hours, uint8_t *minutes, uint8_t *secs, uint8_t *ms)
{
	char tmp[40];
	char *_date, *_time, *secsANDms, *tmpMs;
	char *num;
	strcpy(tmp, str);
	_date = strtok(tmp, "T");
	_time = strtok(NULL, "T");

	*year = strtol(strtok(_date, "-"), &num, 10)%100;
	*month = strtol(strtok(NULL, "-"), &num, 10);
	*day = strtol(strtok(NULL, "-"), &num, 10);

	*hours = strtol(strtok(_time, ":"), &num, 10);
	*minutes = strtol(strtok(NULL, ":"), &num, 10);
	secsANDms = strtok(NULL, ":");
	*secs = strtol(strtok(secsANDms, "."), &num, 10);
	tmpMs = strtok(NULL, ".");
	tmpMs[strlen(tmpMs)-1] = '\0';
	*ms = strtol(tmpMs, &num, 10)%10000;
}

/*---------------------------------------------------------------------------*/
PROCESS_THREAD(broadcast_process, ev, data)
{
  static struct etimer et;
  struct broadcast_message msg, *tmp;
  struct RTC_time t;

  PROCESS_EXITHANDLER(broadcast_close(&broadcast);)

  PROCESS_BEGIN();


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

  while(1) {
    /* Send a broadcast every 16 - 32 seconds */
    etimer_set(&et, CLOCK_SECOND * 7);

    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
    
    RTC_getTime(&t);
    msg.time = t;
    msg.type = 1; // type 1 is sending current time;
    msg.node_id = 0; // set node to 0 to send everyone
    msg.sender_id = 0; // if 0, sender is node manager
    msg.cmd = 0; 
    msg.nodes_to_gate = 0;
    packetbuf_copyfrom(&msg, sizeof(struct broadcast_message));
    broadcast_send(&broadcast);
  }

  PROCESS_END();
}

PROCESS_THREAD(uart_process, ev, data)
{
	PROCESS_BEGIN();

	struct RTC_alarm q = {
		10,		// seconds
		35,		// minutes
		12,		// hours
		30,		// day
		01		// month
	};

	PROCESS_EXITHANDLER(printf("%s\n", "Exiting uart_process"));
	static int i, j;
	
	int k;
	static struct etimer et;
	int len = 13;
	char str[50];
	char command_query[100];
	uint8_t year, month, day, hour, minute, second, ms;
	char *tok;
	struct RTC_time t; 
	
	uart2_init(INC, MOD, SAMP);

	i=0;
	while(1)
	{
		etimer_set(&et, CLOCK_SECOND * 3);
    	PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
		k=0;
		while(uart2_can_get()) 
		{
			printf("%d\n", k);
			i=1;
			command_query[k] = uart2_getc();
			if(atoi(&command_query[k])==0&&k==0) continue;
			k++;
			if(k==100) break;
		}
		if(i)
		{
			FLASH_LED(LEDS_BLUE);
			FLASH_LED(LEDS_BLUE);

			sprintf(str, "%s", command_query);

			parse_date(str, &year, &month, &day, &hour, &minute, &second, &ms);

			strcat(str, "\n");
			len = strlen(str);
			
			for(j=0;j<len;j++)
			{
				uart2_putc(str[j]);
			}
		  		  	
			i=0;
			k=0;

		
			t.hundredths = 0;
			t.tenths = 0;
			t.seconds =	second,
			t.minutes =	minute,
			t.hours = hour;
			t.day =	day;
			t.month = month;
			t.year = year;
			

			RTC_setTime(&t);

			q.month = t.month;
			q.day = t.day;
			q.hours = 18;
			q.minutes = 25;
			q.seconds = 0;

			RTC_setAlarm(&q, alarmCallback, RPT_MINUTE);
		}
	}

PROCESS_END();
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(unicast_process, ev, data)
{
  PROCESS_EXITHANDLER(unicast_close(&unicast);)
    
  PROCESS_BEGIN();

  

  processed = 0;
  printf("%s %d\n", "Unicast started", processed);

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