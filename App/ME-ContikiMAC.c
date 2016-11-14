/*
 *
 *   ME-ContikiMAC
 *  
 */

#include "project-conf.h"
#include "contiki.h"
#include "dev/leds.h"
#include "net/rime.h"
#include "net/netstack.h"
#include "sys/ctimer.h"
#include "dev/cc2420.h"
#include "dev/serial-line.h"
#include "lib/random.h"


#include "net/mac/contikimac.h"


#include "net/mac/cxmac.h"
//#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>



// Ta dika mou defines

#define SINK 0
#define STATIC 1
#define MOBILE 2

#define NUM_SINK 1
#define NUM_MOBILE 8

#define UNICAST 0
#define BROADCAST 1
#define ANYCAST	2




#define GRAND_DELAY 150   
// 150
#define MAX_PACKETS 1024
#define CBR_RATE 120  
//90
#define FIX_CBR_RATE 30

#define BUFF_SIZE 25
#define BURST_SIZE 32


// ----- Debug -----------------

#define DEBUG 0
#if DEBUG
  #define PRINTF(...) printf(__VA_ARGS__)
#else
  #define PRINTF(...)  
#endif


// --- George's defines
// Message Type
#define BEACON 1
#define NOTIFGRAD 3    // gradient propagation
#define RETGRAD 4
#define CBR 5
//To differentiate the LPL duration,in order to not starting every time when packet from burst arrives, but only from the first CBR emitting packet till the end
#define CBR_NEG 6


#define DATA 7  // node sends data to Sink
#define ANY_ACK 8
#define PROBE 9

#define MAX_ID 99




// --- Functions  ----
static int ID();
static void send_gradient();
static void sending_packet(uint8_t p_type,uint8_t addr0,uint8_t addr1, uint8_t data1,uint8_t data2, int tp,rimeaddr_t rcv);
static void emit_data();
static void my_random();


static void packet_sent(void *ptr, int status, int transmissions);

// --- Variables ----
static int My_Rank =100;
static int my_ID;
static unsigned int fatherID;
static unsigned int seq_no = 0; // seq number of data 
static unsigned int seq1=0;
static unsigned int seq2=0;
static int cast_type=UNICAST;
static int got_any_ack=0;
static int burst_counter=0; // frame[3]

static int first=0;
static int retrans=0;



static int rcv=0;
//Packet to frame
static uint8_t frame[BUFF_SIZE];


// ---- Energy -----
#define NRJ_SAMPLE 5


struct energy_time 
{
	long cpu;
	long lpm;
	long tx;
	long rx;
};
//Energy consumed by the sensor
static struct energy_time nrj_tab;
static struct energy_time nrj_dif;

/*---------------------------------------------------------------------------*/
PROCESS(example_gradient_routing, "Example Gradient Routing");
PROCESS(nrj_process, "Evaluates energy using Energest");

AUTOSTART_PROCESSES(&example_gradient_routing, &nrj_process);
//AUTOSTART_PROCESSES(&example_gradient_routing);
/*---------------------------------------------------------------------------*/


PROCESS_THREAD(example_gradient_routing, ev, data)
{

    static struct ctimer timerCBR;
    
    PROCESS_BEGIN();

  
    printf("Starting ................... \n");
    // seting power
    cc2420_set_txpower(7);
    //for having different seeds the nodes
    long t =(rtimer_arch_now());
  //  printf(" Time %ld \n", t);
  random_init(rimeaddr_node_addr.u8[0]* 5);
        
    my_ID=ID();
    
    if (my_ID == SINK ) {
	printf("Sink propagates gradients\n");
	My_Rank=0;    
	
	// Delay
	static struct etimer et; 
	etimer_set(&et, CLOCK_SECOND*2);
	PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
	
	PRINTF("[GRAD] Sink %d.%d launch the gradient construction sequence\n", rimeaddr_node_addr.u8[0], rimeaddr_node_addr.u8[1]);
	send_gradient();
      
    } else if (my_ID==STATIC){     // Static Nodes
	  unsigned int cnt = random_rand() ;
    
	  cnt %= FIX_CBR_RATE ;  //:) Lathos
	  cnt++;
	  ctimer_set(&timerCBR, CLOCK_SECOND *(GRAND_DELAY +cnt), emit_data, NULL);
	  
    }else if (my_ID == MOBILE) {
	
	  My_Rank=100;
	  fatherID =0;						// !!!
	  
	  ctimer_set(&timerCBR, CLOCK_SECOND *(GRAND_DELAY), my_random, NULL);
	 
    }
      
    while (1)	{
		
		//Wait here for an event to happen
		PROCESS_WAIT_EVENT();
				

		// and loop
	}

	    
  PROCESS_END();
}

void my_random() {
      static struct ctimer timerCBR,timer2;
      unsigned int cnt = random_rand() ;
    
      cnt %= 15 ;
      cnt++;
 
      printf("Retransmisions %d \n",retrans);
      printf ("Mobile Emit %d   %d \n",cnt,cnt+(rimeaddr_node_addr.u8[0]-1)*15);
      ctimer_set(&timerCBR, CLOCK_SECOND *(cnt+(rimeaddr_node_addr.u8[0]-1)*15), emit_data, NULL);
      
  
}


// Synarthsh ston sender meta apo thn apostolh mallon
// ti kanoyme ean den to lavoyme k.o
static void packet_sent(void *ptr, int status, int transmissions)
{
char *packet;	
	rimeaddr_t from;
	rimeaddr_copy(&from, packetbuf_addr(PACKETBUF_ADDR_RECEIVER));
	
	if (my_ID==MOBILE)	{
	  retrans=retrans+transmissions;
	  if (seq_no==128) {
	    printf("My Retransmisions %d \n",retrans);
	  }
	}
	packet = (char *)packetbuf_dataptr();
	
}

static int ID() {
int m_id=STATIC;
      if (rimeaddr_node_addr.u8[0]<=NUM_MOBILE) {
	  m_id=MOBILE;
	  set_mobile_radio();
	  set_mobile_mac();
	  
      }else if (rimeaddr_node_addr.u8[0]> NUM_MOBILE && rimeaddr_node_addr.u8[0]<= NUM_SINK+NUM_MOBILE) {
	  m_id=SINK;
      }
       
  
      
      return m_id;
}

static void sending_packet(uint8_t p_type,uint8_t addr0,uint8_t addr1, uint8_t data1,uint8_t data2, int tp, rimeaddr_t rcv)
{

      // packet prepare
	packetbuf_clear();
	frame[0] = p_type;
	frame[1] = addr0;    
	frame[2] = addr1;
	frame[3] = data1;
	frame[4] = data2;
	frame[5] = 0;
	frame[6] = 0;  
		
	packetbuf_copyfrom(&frame, BUFF_SIZE);
	
	//send_type=p_type;
	
	if (tp==ANYCAST) {
	    rimeaddr_t receiver;
	    receiver.u8[0] = 99;
	    receiver.u8[1] = 0;
	    
	    packetbuf_set_addr(PACKETBUF_ADDR_RECEIVER, &receiver);
	    packetbuf_set_addr(PACKETBUF_ADDR_SENDER, &rimeaddr_node_addr);
	    NETSTACK_MAC.send(packet_sent, &receiver);
	}
	
	packetbuf_set_addr(PACKETBUF_ADDR_SENDER, &rimeaddr_node_addr);
	if (tp==UNICAST) {
	    packetbuf_set_addr(PACKETBUF_ADDR_RECEIVER, &rcv); //rimeaddr_t addr
	    NETSTACK_MAC.send(packet_sent, &rcv);
	}else if (tp==BROADCAST){
	    NETSTACK_MAC.send(packet_sent, NULL);  // broadcast
	}
	    
  
}
void send_gradient() {
   rimeaddr_t receiver;
 
    //PRINTF("BroadCasting ------------------\n");  
   
    // delay
   PRINTF("[GRAD] %d.%d propagates its rank %d\n", rimeaddr_node_addr.u8[0], rimeaddr_node_addr.u8[1], My_Rank);
    sending_packet(NOTIFGRAD,rimeaddr_node_addr.u8[0],rimeaddr_node_addr.u8[1], My_Rank,0,BROADCAST,receiver); 
  
}

void emit_data() {
rimeaddr_t receiver;
  
	if(seq_no >=MAX_PACKETS) {
	   return;
	}

	//Emit CBR again later
	static struct ctimer timerCBR;
	if (my_ID==STATIC) {
	  ctimer_set(&timerCBR, CLOCK_SECOND*FIX_CBR_RATE, emit_data, NULL);
	}else {
	   ctimer_set(&timerCBR, CLOCK_SECOND*CBR_RATE, emit_data, NULL);
	}
	
	
	if (my_ID==STATIC) {  
	      
	      receiver.u8[0] = fatherID;
	      receiver.u8[1] = 0;	
	      
	      int i;
	      for (i=0;i<1 ; ++i) {
		   
			seq_no++;
			
			seq1= seq_no %256 ;
			if (seq1==0) {
			  ++seq2;
			}
				
			printf ("SEND %d %d %d %d\n",rimeaddr_node_addr.u8[0], fatherID,rimeaddr_node_addr.u8[0], seq_no);
			sending_packet(DATA,rimeaddr_node_addr.u8[0],rimeaddr_node_addr.u8[1], seq1,seq2,UNICAST,receiver);
		    
	      }      
		    
	} else {
	  
	   							// Na dw pws tha to k;anv burst
		  printf("Sending Burst %d  counter %d \n",seq_no+1,burst_counter);
		      
		  
		  int i=0 ;					// burst
		  for (i=0;i<BURST_SIZE ; ++i) {
		   
			seq_no++;
			
			seq1= seq_no %256 ;
			if (seq1==0) {
			  ++seq2;
			}
			
			printf ("EMT %d %d %d %d\n",rimeaddr_node_addr.u8[0], 99,rimeaddr_node_addr.u8[0], seq_no);
			sending_packet(DATA,rimeaddr_node_addr.u8[0],rimeaddr_node_addr.u8[1], seq1,seq2,ANYCAST,receiver);
		    
		      ++burst_counter;
		    
	   }
	//  printf("Retransmisions %d \n",retrans);
	}
	
	     
}


// ekteleitai ston receiver 

static void forward_msg(void *p) {
rimeaddr_t receiver;
char *packet;
 	
	
	receiver.u8[0] = fatherID;
	receiver.u8[1] = 0;									// Na dw pws tha to k;anv burst

	packet =(char *)p;
	
	uint8_t s1=packet[3];
	uint8_t s2=packet[4];
	unsigned int seq =s2*256+s1;
	unsigned int k1,k2;
	
	 sending_packet(DATA,packet[1],packet[2],packet[3],packet[4],cast_type,receiver); 
	
}

static void input(void)
{

	rimeaddr_t from,to;
	char *packet;
	static struct ctimer timerGRADACK,timerFW;
	
	rimeaddr_copy(&from, packetbuf_addr(PACKETBUF_ADDR_SENDER));
	rimeaddr_copy(&to, packetbuf_addr(PACKETBUF_ADDR_RECEIVER));

	packet = (char *)packetbuf_dataptr();
	
	static unsigned int counter = 0;
	
	if (my_ID != MOBILE) {
		  if (packet[0]==NOTIFGRAD ) {
		  //  printf("Receiving grand \n");
		    
		    int rcv_rank=(int)packet[3];
		  
		    if (rcv_rank+1 < My_Rank ) {
			My_Rank =rcv_rank+1;
			fatherID = from.u8[0];  // Updates fatherID
			
			//printf("%d\n",fatherID);						
			
			counter = random_rand();
			counter %= 60;
			counter += 1;

			//Will propagate gradient after a random time
			PRINTF("[GRAD] Node %d.%d takes rank %d in the gradient (from %d.%d)\n", rimeaddr_node_addr.u8[0], rimeaddr_node_addr.u8[1], My_Rank, from.u8[0], from.u8[1]);
			printf(" RANK %d\n",My_Rank);
			ctimer_set(&timerGRADACK, CLOCK_SECOND * (counter)/2, send_gradient, NULL);
			
		      

		    }
		  } else if (packet[0]==DATA || packet[0]==PROBE) {
			
			uint8_t s1=packet[3];
			uint8_t s2=packet[4];
			unsigned int seq =s2*256+s1;
			
			    if (my_ID!=SINK) {
				//PRINTF("[CBR-RX] Node %d [%d] got data from %d (seq=%d) via %d  father ID %d\n", rimeaddr_node_addr.u8[0], 1, packet[1], seq, from.u8[0],fatherID);
				
				if (packet[0]==PROBE) {
				   printf("PRB %d %d %d %d \n",from.u8[0],rimeaddr_node_addr.u8[0], packet[1],seq); 
				  
				}else {
				  printf("RCV %d %d %d %d %d\n",from.u8[0],rimeaddr_node_addr.u8[0], packet[1],seq,fatherID);
				  forward_msg(packet);
				}
				
				
				
			    }else {
			 
			      if (packet[0]==PROBE) {
				printf("PRB %d %d %d %d \n",from.u8[0],rimeaddr_node_addr.u8[0], packet[1],seq);
			      } else {
				printf("SNK %d %d %d %d %d\n",from.u8[0],rimeaddr_node_addr.u8[0],packet[1],seq,++rcv);
			      }
			   }
			    
		  
		  }
	}
	else {
	  if (first==0) {
	  long int m, k=rtimer_arch_now();
	
	   random_init(k);
	    ++first;
	  }
	  
	}
}

/*---------------------------------- Energy ------------------------------------*/



PROCESS_THREAD(nrj_process, ev, data)
{
    //stop-start ongoing time measurements to retrieve the diffs
    //ENERGEST_OFF(ENERGEST_TYPE_CPU);
    //ENERGEST_ON(ENERGEST_TYPE_CPU);
    
    static struct etimer et;
    
    PROCESS_BEGIN();
    
    etimer_set(&et, NRJ_SAMPLE*CLOCK_SECOND);
  //  printf(" ------------------------------------------\n");
    double voltage = 3;
    double power_cpu = 1.8 * voltage; //mW
    double power_lpm = 0.0545 * voltage; //mW
    double power_tx = 17.0 * voltage; //mW
    double power_rx = 14.7 * voltage; //mW
    //double power_tx = 17.7 * voltage; //mW
    //double power_rx = 20.0 * voltage; //mW
    
    //Power = (timer / ticks_per_sec) * power / Update_time
    long  powerCPU = 0;
    double powerLPM = 0;
    double powerTX = 0;
    double powerRX = 0;
    
    nrj_tab.cpu = energest_type_time(ENERGEST_TYPE_CPU);
    nrj_tab.lpm = energest_type_time(ENERGEST_TYPE_LPM);
    nrj_tab.tx = energest_type_time(ENERGEST_TYPE_TRANSMIT);
    nrj_tab.rx = energest_type_time(ENERGEST_TYPE_LISTEN);
    
    while(1)
    {
        PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
        
        //Retrieve periods in each state (CPU, LPM, TX, RX)
        nrj_dif.cpu = energest_type_time(ENERGEST_TYPE_CPU) - nrj_tab.cpu;
        nrj_dif.lpm = energest_type_time(ENERGEST_TYPE_LPM) - nrj_tab.lpm;
        nrj_dif.tx = energest_type_time(ENERGEST_TYPE_TRANSMIT) - nrj_tab.tx;
        nrj_dif.rx = energest_type_time(ENERGEST_TYPE_LISTEN) - nrj_tab.rx;
        
        nrj_tab.cpu = energest_type_time(ENERGEST_TYPE_CPU);
        nrj_tab.lpm = energest_type_time(ENERGEST_TYPE_LPM);
        nrj_tab.tx = energest_type_time(ENERGEST_TYPE_TRANSMIT);
        nrj_tab.rx = energest_type_time(ENERGEST_TYPE_LISTEN);
        
    //printf("[NRJ_TIME];%d.%d;CPU;%ld;%ld;LPM;%ld;%ld;TX;%ld;%ld;RX;%ld;%ld \n", rimeaddr_node_addr.u8[0], rimeaddr_node_addr.u8[1], nrj_tab.cpu, nrj_dif.cpu, nrj_tab.lpm, nrj_dif.lpm, nrj_tab.tx, nrj_dif.tx, nrj_tab.rx, nrj_dif.rx);
        
        //Calculate NRJ consumption
        //Power = (timer / ticks_per_sec) * power / Update_time
	double k;
        k =((double)nrj_dif.cpu / 4096) * power_cpu / NRJ_SAMPLE; //RTIMER_SECOND
        powerLPM = ((double)nrj_dif.lpm / 4096) * power_lpm / NRJ_SAMPLE; //mW
        powerTX = ((double)nrj_dif.tx / 4096) * power_tx / NRJ_SAMPLE;
        powerRX = ((double)nrj_dif.rx / 4096) * power_rx / NRJ_SAMPLE;
	

	//powerCPU=k;
	//printf(" Power CPU %ld \n",powerCPU);
        

	
	//printf("[NRJ_MW];%d.%d;CPU;%ld;LPM;%ld;TX;%ld;RX;%ld;\n", rimeaddr_node_addr.u8[0], rimeaddr_node_addr.u8[1], nrj_dif.cpu * 100, nrj_dif.lpm * 100,nrj_dif.tx * 100, nrj_dif.rx * 100);
	printf("[NRJ_MW];%d.%d;CPU;%ld;LPM;%ld;TX;%ld;RX;%ld;\n", rimeaddr_node_addr.u8[0], rimeaddr_node_addr.u8[1], (long)(powerCPU * 100), (long)(powerLPM * 100), (long)(powerTX * 100), (long)(powerRX * 100));
        
        etimer_reset(&et);
    }
    
    PROCESS_END();
}
static void init(void)
{
	printf("Anycast ver 3.0 \n");
	printf("Mobile Nodes %d BurstSize %d PacketNum %d CBR %d FIX_CBR_RATE %d,Power %d NRJ %d \n", NUM_MOBILE, BURST_SIZE, MAX_PACKETS, CBR_RATE,FIX_CBR_RATE,  11, NRJ_SAMPLE);
}

const struct network_driver my_driver = {
  "My driver",
  init,
  input
};
/*---------------------------------------------------------------------------*/
