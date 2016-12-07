#include "wifiDrv.h"
#include "settings.h"
#include "simple_queue.h"
#include "event_manager.h"

static uint8_t dinBuf[WIFIDRV_BUFIN_SZ*2];
static ringBuf_t dinBufCtrl = {dinBuf,0,0,WIFIDRV_BUFIN_SZ};
static const uint32_t dinBufNext= (uint32_t)dinBuf + (uint32_t)WIFIDRV_BUFIN_SZ;

static wifiDrvIN_frame doutPool[WIFIDRV_BUFOUT_SZ]={0};
static uint32_t doutBuf[WIFIDRV_BUFOUT_SZ];
static uint32_t poolCount=0;
static ringBuf32_t doutBufCtrl = {doutBuf,0,WIFIDRV_BUFOUT_SZ-1,WIFIDRV_BUFOUT_SZ};

static uint32_t currLen=0;
static uint8_t *currStr=dinBuf;

static wifiStatus curr_status;

/*Function Implementations */

static int32_t getINBufFreeSpace(void)
{
  return ((dinBufCtrl.tail-dinBufCtrl.head) > 0)?
    dinBufCtrl.tail-dinBufCtrl.head-1:WIFIDRV_BUFIN_SZ+(dinBufCtrl.tail-dinBufCtrl.head)-1;
}

void wifiParse(uint8_t data){
}

int32_t wifiDrvIN_read(wifiDrvIN_frame **addr){
  uint32_t count;
  //release dinBuf
  uint8_t *str = doutPool[doutBufCtrl.tail].str;
  uint32_t len = doutPool[doutBuf[doutBufCtrl.tail]].len;
  dinBufCtrl.tail = (((uint32_t)str + len - dinBufNext + 1) >= 0)?
    (uint32_t)str + len - dinBufNext + 1 : (uint32_t)str + len - (uint32_t)dinBuf + 1;
  if(ringBufSPop32(&doutBufCtrl,&count)) return -1; //OUT buffer empty
  *addr = (wifiDrvIN_frame *) &doutPool[count];
  return 0;
}

//frame specification:
////id:20bytes
////coordinates:4+4bytes
////pass:undefined
////frame will be:
////passw(sha1(20bytes))|coordx(4bytes)|coordy(bytes)|id(C-string)|checksum(4bytes)|
//typedef enum {
//  START=0,
//  }
int32_t wifiDrvIN_write(uint8_t data){
  if(ringBufPush(&dinBufCtrl,data)) return -1; //IN buffer full
  //writes out of boundary to enable regular string manipulation
  if((uint32_t)(currStr + currLen) >= dinBufNext)
    dinBuf[(int)(currStr-dinBuf) + currLen] = data;
  if(data == '\n') //send frame to buffer
  {
    if(ringBufPush32(&doutBufCtrl,poolCount))
    {
      dinBufCtrl.head = currStr-dinBuf; //flush incomplete frame
      currLen = 0;
      return -2; //OUT buffer full
    }
    doutPool[poolCount].str = currStr;
    doutPool[poolCount++].len = currLen;
    if(poolCount >= WIFIDRV_BUFOUT_SZ)
      poolCount = 0;
    currStr = dinBuf + dinBufCtrl.head;
    currLen = 0;
    if(EM_setEvent(wifi_e) < 0) return -3; //event queue full
  }
  else
  {
    currLen++;
  }
  //return dinBufCtrl.head;
  return getINBufFreeSpace();
}

#ifdef STM32F401xx
void wifiDrv_Setup(void){
  GPIO_InitTypeDef GPIO_InitStructure;
  USART_InitTypeDef USART_InitStructure;
  NVIC_InitTypeDef NVIC_InitStructure;
  /* WIFI_PORT Periph clock enable */
  RCC_AHB1PeriphClockCmd(WIFI_GPIO_CLK, ENABLE);
  WIFI_MODULE_CLK_SETUP(WIFI_MODULE_CLK, ENABLE);
  /* WIFI Port Configuration */
  GPIO_InitStructure.GPIO_Pin = WIFI_TX_PIN | WIFI_RX_PIN;
  GPIO_InitStructure.GPIO_Speed =GPIO_Medium_Speed;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
  GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
  GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
  GPIO_Init(WIFI_PORT, &GPIO_InitStructure);

  GPIO_InitStructure.GPIO_Pin = WIFI_RST_PIN | WIFI_MODE_PIN;
  GPIO_InitStructure.GPIO_Speed =GPIO_Medium_Speed;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
  GPIO_InitStructure.GPIO_OType = GPIO_OType_OD;
  GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
  GPIO_Init(WIFI_PORT, &GPIO_InitStructure);

  GPIO_PinAFConfig(WIFI_PORT,WIFI_TX_PINSOURCE,WIFI_MODULE_AF);
  GPIO_PinAFConfig(WIFI_PORT,WIFI_RX_PINSOURCE,WIFI_MODULE_AF);

  USART_StructInit(&USART_InitStructure);
  USART_InitStructure.USART_BaudRate = 115200;
  USART_Init(WIFI_MODULE,&USART_InitStructure);

  /* Configure WIFI interrupt */
  NVIC_InitStructure.NVIC_IRQChannel = WIFI_IRQ; 
  NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
  NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
  NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
  NVIC_Init(&NVIC_InitStructure);
  USART_ITConfig(WIFI_MODULE, WIFI_RX_IT, ENABLE);
  //USART_ITConfig(WIFI_MODULE, WIFI_TX_IT, ENABLE);
  USART_Cmd(WIFI_MODULE, ENABLE);

  //Set wifi status
  wifiSetStatus(wifi_disabled);
}

int32_t wifiSetStatus(wifiStatus status){
  switch(status)
  {
    case wifi_disabled:
      WIFI_MODE_USER;
      WIFI_RST_ON; //0v
      for(int i=0;i<100000;i++);
      WIFI_RST_OFF; //3v3
      break;
    case wifi_setup:
      wifiDrvOUT_puts("=node.restart()\r\n");
      //wifiDrvOUT_puts("=file.fsinfo()\r\n");
      wifiDrvOUT_puts("do(\"setup.lua\")\r\n");
    case wifi_auth:
      wifiDrvOUT_puts("=node.restart()\r\n");
      //wifiDrvOUT_puts("=file.fsinfo()\r\n");
      wifiDrvOUT_puts("do(\"auth.lua\")\r\n");
      break;
    //default:
  }
  curr_status = status;
}

wifiStatus wifiGetStatus(void){
  return curr_status;
}
//returns number of bytes sent
int32_t wifiDrvOUT_puts(char *str){
  uint32_t i=0;
  while(*(str+i) != 0)
  {
    wifiDrvOUT_write(*(str+i));
    i++;
  }
  return i;
}

int32_t wifiDrvOUT_write(uint8_t data){
  while(SET != USART_GetFlagStatus(WIFI_MODULE, USART_FLAG_TC));
  USART_SendData(WIFI_MODULE,data);
  return 0;
}
#endif
