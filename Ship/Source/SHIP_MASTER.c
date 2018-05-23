/****************************************************************************
 Module
   Communication state machine - main state machine for the SHIP

 Revision
   1.0.1

 Description
   This is a template file for implementing flat state machines under the
   Gen2 Events and Services Framework.

 Notes

 History
 When           Who     What/Why
 -------------- ---     --------
 01/15/12 11:12 jec      revisions for Gen2 framework
 11/07/11 11:26 jec      made the queue static
 10/30/11 17:59 jec      fixed references to CurrentEvent in RunTemplateSM()
 10/23/11 18:20 jec      began conversion from SMTemplate.c (02/20/07 rev)
****************************************************************************/
/*----------------------------- Include Files -----------------------------*/
/* include header files for this state machine as well as any machines at the
   next lower level in the hierarchy that are sub-machines to this machine
*/
#include "ES_Configure.h"
#include "ES_Framework.h"
#include "SHIP_MASTER.h"
#include "MotorModule.h" 
#include "SHIP_RX.h"
#include "SHIP_TX.h"
#include "SHIP_PIC_RX.h"
#include "SHIP_PIC_TX.h"

/*----------------------------- Module Defines ----------------------------*/
#define ONE_SEC 1000
#define BLUE_TEAM 1
#define RED_TEAM 0
#define PAIR_ATTEMPT_TIME 200
#define PAIR_TIMEOUT_TIME ONE_SEC

/*---------------------------- Module Functions ---------------------------*/
/* prototypes for private functions for this machine.They should be functions
   relevant to the behavior of this state machine
*/
static bool getHomeTeamColor(void);

/*FOR TESTING ONLYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYY*/
static void sendPairAck(void); 
static void sendStatusPacket(void); 
static void executeControlPacketCommands(void); 


/*---------------------------- Module Variables ---------------------------*/
// everybody needs a state variable, you may need others as well.
// type of state variable should match htat of enum in header file
static shipState_t CurrentState;
static uint8_t MyPriority;
static uint32_t lastAnsAddr = 0; 
static bool homeTeamColor; 

// Control Data
static uint8_t Control_FB;
static uint8_t Control_LR; 
static uint8_t Control_TurretR;
static uint8_t Control_TurretP;
static uint8_t Control_CTRL; 

/*------------------------------ Module Code ------------------------------*/
/****************************************************************************
 Function
     InitTemplateFSM

 Parameters
     uint8_t : the priorty of this service

 Returns
     bool, false if error in initialization, true otherwise

 Description
     Saves away the priority, sets up the initial transition and does any
     other required initialization for this state machine
 Notes

 Author
     J. Edward Carryer, 10/23/11, 18:55
****************************************************************************/
bool InitSHIP_MASTER(uint8_t Priority)
{
  ES_Event_t ThisEvent;

  MyPriority = Priority;
  // put us into the Initial PseudoState
  CurrentState = Waiting2Pair;
  printf("\r\nIn Waiting2Pair now yee");
  
  HWREG(SYSCTL_RCGCGPIO) |= SYSCTL_RCGCGPIO_R0;
  while((HWREG(SYSCTL_PRGPIO) & SYSCTL_PRGPIO_R0)!= SYSCTL_PRGPIO_R0)
  {
  }  
  
  // Set PE4 and PE5 to digital 
  HWREG(GPIO_PORTA_BASE + GPIO_O_DEN) |= (BIT2HI); 
  
  // PE4 input (RX), PE5 output (TX) 
  HWREG(GPIO_PORTA_BASE + GPIO_O_DIR) &= ~BIT2HI; 
  
  //initialize all hw necessairy for the SHIP 
  homeTeamColor = getHomeTeamColor(); 
  
  // post the initial transition event
  ThisEvent.EventType = ES_INIT;
  if (ES_PostToService(MyPriority, ThisEvent) == true)
  {
    return true;
  }
  else
  {
    return false;
  }
}

/****************************************************************************
 Function
     PostTemplateFSM

 Parameters
     EF_Event ThisEvent , the event to post to the queue

 Returns
     boolean False if the Enqueue operation failed, True otherwise

 Description
     Posts an event to this state machine's queue
 Notes

 Author
     J. Edward Carryer, 10/23/11, 19:25
****************************************************************************/
bool PostSHIP_MASTER(ES_Event_t ThisEvent)
{
  return ES_PostToService(MyPriority, ThisEvent);
}

/****************************************************************************
 Function
    RunTemplateFSM

 Parameters
   ES_Event : the event to process

 Returns
   ES_Event, ES_NO_EVENT if no error ES_ERROR otherwise

 Description
   add your description here
 Notes
   uses nested switch/case to implement the machine.
 Author
   J. Edward Carryer, 01/15/12, 15:23
****************************************************************************/
ES_Event_t RunSHIP_MASTER(ES_Event_t ThisEvent)
{
  ES_Event_t ReturnEvent;
  ReturnEvent.EventType = ES_NO_EVENT; // assume no errors

  switch (CurrentState)
  {
    case Waiting2Pair:      
    {
      if(ThisEvent.EventType == ES_PAIR_REQUEST){  /*received 0x01 packet*/
        //guard: if fueled, then only the home team can connect 
        if(QueryFuelEmpty() && (Query_ANSIBLEColour() == homeTeamColor)){
          //start pairing timer (1sec)
          ES_Timer_InitTimer(PAIR_TIMEOUT_SHIP_TIMER, PAIR_TIMEOUT_TIME);
          //start attempt timer (200ms)
          ES_Timer_InitTimer(PAIR_ATTEMPT_SHIP_TIMER, PAIR_ATTEMPT_TIME);          
          //send Pair_Ack Packet (0x02) 
          sendPairAck(); 
          CurrentState = Trying2Pair; 
          printf("\r\nIn Trying2Pair now"); 
        }
        else if(!QueryFuelEmpty() && (lastAnsAddr != QuerySourceAddress())){
          //start pairing timer (1sec)
          ES_Timer_InitTimer(PAIR_TIMEOUT_SHIP_TIMER, PAIR_TIMEOUT_TIME);
          //start attempt timer (200ms) 
          ES_Timer_InitTimer(PAIR_ATTEMPT_SHIP_TIMER, PAIR_ATTEMPT_TIME);
          //send Pair_Ack Packet (0x02)
          sendPairAck(); 
          CurrentState = Trying2Pair; 
          printf("\r\nIn Trying2Pair now"); 
        }
      }
    }
    break;

    case Trying2Pair:  //make 4 more attempts at sending 0x02 packet in case first one not read 
    {
      if(ThisEvent.EventType == ES_TIMEOUT){
        if(ThisEvent.EventParam == PAIR_ATTEMPT_SHIP_TIMER){
          //200 ms timer has timeout, just resend 0x02 packet and restart 200ms timer 
          ES_Timer_InitTimer(PAIR_ATTEMPT_SHIP_TIMER, PAIR_ATTEMPT_TIME);
          sendPairAck();
        }
        else if(ThisEvent.EventParam == PAIR_TIMEOUT_SHIP_TIMER){
          CurrentState = Waiting2Pair; 
          printf("\r\nIn Waiting2Pair now"); 
        }
        
      }
      else if(ThisEvent.EventType == ES_CONTROL_PACKET){  /*Control packet 0x03 received*/
        sendStatusPacket(); //0x04
        //restart 1 sec pairing timeout timer
        ES_Timer_InitTimer(PAIR_TIMEOUT_SHIP_TIMER, PAIR_TIMEOUT_TIME);
        executeControlPacketCommands(); 
        
        CurrentState = Communicating;
        printf("\r\nIn Communicating now"); 
      }
    }
    break;
    
    case Communicating:  //regular paired state
    {
      if(ThisEvent.EventType == ES_CONTROL_PACKET){  /*Control packet 0x03 received*/
        sendStatusPacket(); //0x04
        //restart 1 sec pairing timeout timer
        ES_Timer_InitTimer(PAIR_TIMEOUT_SHIP_TIMER, PAIR_TIMEOUT_TIME);
        executeControlPacketCommands();         
      }
      else if(ThisEvent.EventType == ES_TIMEOUT && ThisEvent.EventParam == PAIR_TIMEOUT_SHIP_TIMER){
        //one sec pairing timer has timed out 
        StopFanMotors(); 
        CurrentState = Waiting2Pair;
        printf("\r\nONE_SEC Timer Timeout");
        printf("\r\nIn Waiting2Pair now");
      } 
      else if(ThisEvent.EventType == ES_OUT_OF_FUEL){  /*Out of fuel event*/
        StopFanMotors();
        lastAnsAddr = Query_ANSIBLEColour(); 
        CurrentState = Waiting2Pair; 
        printf("\r\nOut of Fuel");
        printf("\r\nIn Waiting2Pair now");
      }
      else if(ThisEvent.EventType == ES_REFUELED && (Query_ANSIBLEColour() != homeTeamColor)){  /*Refueled Event*/
        StopFanMotors(); 
        CurrentState = Waiting2Pair; 
        printf("\r\nOpposite team kicked out"); 
        printf("\r\nIn Waiting2Pair now");
      }      
    }
    break;
        
  }                                  
  return ReturnEvent;
}




/****************************************************************************
 Function
     QueryTemplateSM

 Parameters
     None

 Returns
     TemplateState_t The current state of the Template state machine

 Description
     returns the current state of the Template state machine
 Notes

 Author
     J. Edward Carryer, 10/23/11, 19:21
****************************************************************************/
shipState_t QueryCommunicationSM(void)
{
  return CurrentState;
}


/***************************************************************************
 private functions
 ***************************************************************************/

bool getHomeTeamColor(void)
{
  uint8_t CurrentButtonState = (HWREG(GPIO_PORTA_BASE+ ALL_BITS) & BIT2HI);
      
  if(CurrentButtonState)
  {
    //printf("\r\nTeam = RED");
    return 0x01; 
  }
  else
  {
    //printf("\r\nTeam = BLUE");
    return 0x00;
  }    
}

static void sendPairAck(void)
{
  ES_Event_t ThisEvent;
  
  printf("\r\nACK packet send"); 
  ThisEvent.EventType = BEGIN_TX;
  ThisEvent.EventParam = PAIR_ACK_EVENT;
  PostSHIP_TX(ThisEvent);
}

static void sendStatusPacket(void)
{
  ES_Event_t ThisEvent;
  
  printf("\r\nStatus packet sent"); 
  ThisEvent.EventType = BEGIN_TX;
  ThisEvent.EventParam = STATUS_EVENT;
  PostSHIP_TX(ThisEvent);
}

static void executeControlPacketCommands(void)
{
  printf("\r\nControl commands executed");
  Control_FB = Query_FB();
  Control_LR = Query_LR();
  Control_TurretR = Query_TurretR();
  Control_TurretP = Query_TurretP();
  Control_CTRL = Query_CTRL();
  
  // TODO: USE VARIABLES ABOVE TO MOVE BOAT
  
}

