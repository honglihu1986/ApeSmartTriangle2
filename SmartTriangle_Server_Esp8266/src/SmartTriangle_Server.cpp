#include "HalfDuplexSerial.h"
#include "TriangleProtocol.h"
#include "SmartTopology.h"
#include <Vector.h>

enum STLightType
{
    STLT_WAITING_CHECK = 0,
    STLT_CHECKING = 1,
    STLT_SHOW_EFFECT
};

STNodeDef *seekNodeQueue_storage[64];
Vector<STNodeDef *> seekNodeQueue;
STNodeDef *currentNode;
STLightType stLightType;
#define HDSERIAL_PIN D6
#define SLECTED_PIN D5

HalfDuplexSerial hdSerial(HDSERIAL_PIN);

void waitingReceive()
{
  hdSerial.setMode(SMT_RECEIVE);
  TPT.tpBeginReceive();
}

void waitingTransmit()
{
  hdSerial.setMode(SMT_TRANSMIT);
}

void startSelect()
{
  pinMode(SLECTED_PIN, OUTPUT);
  digitalWrite(SLECTED_PIN, HIGH);
}

void stopSelect()
{
  pinMode(SLECTED_PIN, INPUT);
}

void seekRootNode()
{
  //寻找根节点
  stLightType = STLT_CHECKING;
  startSelect();
  TPT.tpBegin(51).tpTransmit(true);
  waitingReceive();
}

void seekNodeByQueue()
{
  if (seekNodeQueue.size() <= 0)
  {
    stLightType = STLT_SHOW_EFFECT;
    return;
  }
  STNodeDef *node = seekNodeQueue[0];
  if (node->leftChildType == STNT_WAITING_CHECK)
  {
    node->leftChildType = STNT_CHECKING;
    TPT.tpBegin(21).tpByte(node->nodeId).tpTransmit();
    delay(100);
    TPT.tpBegin(52).tpTransmit(true);
    waitingReceive();
    currentNode = node;
  }else if(node->rightChildType == STNT_WAITING_CHECK)
  {
    node->rightChildType = STNT_CHECKING;
    TPT.tpBegin(23).tpByte(node->nodeId).tpTransmit();
    delay(100);
    TPT.tpBegin(52).tpTransmit(true);
    waitingReceive();
    currentNode = node;
  }
}

void seekLeafNode()
{
  seekNodeQueue.push_back(ST.rootNode());
  seekNodeByQueue();
}

void effectLoop()
{
  int count = ST.nodeCount();
  int index = random(0,count);
  TPT.tpBegin(101).tpByte(index).tpColor(random(50,150),random(50,150),random(50,150)).tpTransmit();
  delay(100);
}

void tpCallback(byte pId, byte *payload, unsigned int length, bool isTimeout)
{
  waitingTransmit();
  Serial.println("tpCallback pId:" + String(pId) + " Timeout:" + String(isTimeout ? "True" : "False"));
  switch (pId)
  {
  case 1:
  {
  }
  break;
  case 2:
  {
  }
  break;
  case 51:
  {
    stopSelect();
    if (isTimeout)
    {
      //无根节点，拓扑结束
    }
    else
    {
      //有节点
      uint8_t nodeId = ST.creatRootNode();
      Serial.println("ROOT NODE ID:" + String(nodeId));
      TPT.tpBegin(2).tpByte(nodeId).tpTransmit();
      seekLeafNode();
    }
  }
  break;
  case 52:
  {
    if (isTimeout)
    {
      //无子节点
      if (currentNode->leftChildType == STNT_CHECKING)
      {
        TPT.tpBegin(22).tpByte(currentNode->nodeId).tpTransmit();
        currentNode->leftChildType = STNT_HAS_NO_CHILD;
      }
      else if (currentNode->rightChildType == STNT_CHECKING)
      {
        TPT.tpBegin(24).tpByte(currentNode->nodeId).tpTransmit();
        currentNode->rightChildType = STNT_HAS_NO_CHILD;
        seekNodeQueue.remove(0);
      }
      seekNodeByQueue();
    }
    else
    {
      //有子节点
      STNodeDef *node = ST.creatNode();
      seekNodeQueue.push_back(node);
      Serial.println("CHILD NODE ID:" + String(node->nodeId));
      if (currentNode->leftChildType == STNT_CHECKING)
      {
        currentNode->leftChildType = STNT_HAS_CHILD;
        currentNode->leftChild = node;
        TPT.tpBegin(22).tpByte(currentNode->nodeId).tpTransmit();
        TPT.tpBegin(2).tpByte(node->nodeId).tpTransmit();
      }
      else if (currentNode->rightChildType == STNT_CHECKING)
      {
        currentNode->rightChildType = STNT_HAS_CHILD;
        currentNode->rightChild = node;
        TPT.tpBegin(24).tpByte(currentNode->nodeId).tpTransmit();
        TPT.tpBegin(2).tpByte(node->nodeId).tpTransmit();
        seekNodeQueue.remove(0);
      }else
      {
        seekNodeQueue.pop_back();
        delete node;
      }
      seekNodeByQueue();
    }
  }
  break;
  }
}

void transmitCallback(byte *ptBuffer, unsigned int ptLength)
{
  for (unsigned int i = 0; i < ptLength; i++)
  {
    int c = ptBuffer[i];
    hdSerial.write(c);
  }
}

void setup()
{
  seekNodeQueue.setStorage(seekNodeQueue_storage);
  stLightType = STLT_WAITING_CHECK;
  Serial.begin(115200);
  pinMode(D2, INPUT);

  hdSerial.begin(9600);
  hdSerial.setMode(SMT_TRANSMIT);
  pinMode(D7, INPUT_PULLUP);
  TPT.callbackRegister(tpCallback, transmitCallback);

  delay(200);

  TPT.tpBegin(1).tpByte(5).tpTransmit(); //所有节点初始化
  seekRootNode();
}

void loop()
{
  TPT.protocolLoop();
  if (hdSerial.serialModeType() == SMT_RECEIVE)
  {
    while (hdSerial.available())
    {
      byte c = hdSerial.read();
      TPT.tpPushData(c).tpParse();
    }
  }
  if(stLightType == STLT_SHOW_EFFECT)
  {
    Serial.println("show effect");
    effectLoop();
  }
  // if(digitalRead(D2)==LOW)
  // {
  //   if(hdSerial.serialModeType() == SMT_RECEIVE)
  //   {
  //     hdSerial.setMode(SMT_TRANSMIT);
  //     Serial.println("TX");
  //   }else
  //   {
  //     hdSerial.setMode(SMT_RECEIVE);
  //     Serial.println("RX");
  //   }
  //   delay(200);
  // }
  // if (hdSerial.serialModeType() == SMT_TRANSMIT)
  // {

  //   while (Serial.available())
  //   {
  //     Serial.println("test");
  //     //TPT.tpPushData(hdSerial.read()).tpParse();
  //     hdSerial.write(Serial.read());
  //   }
  // }
}