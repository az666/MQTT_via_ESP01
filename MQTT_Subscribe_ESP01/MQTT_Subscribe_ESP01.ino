/*
 * MQTT Subscriber using standard AT firmware
 *  
 * ESP8266----------ATmega
 * TX     ----------RX(D4)
 * RX     ----------TX(D5)
 * 
 * ESP8266----------STM32F103
 * TX     ----------RX2(PA3)
 * RX     ----------TX2(PA2)
 * 
 */
 
#if defined(__AVR__)
#include <SoftwareSerial.h>
#define SUB_TOPIC       "arduino/#" // You can change
//#define STOP_BUTTON     2 // 0: Disable STOP_BUTTON
#define STOP_BUTTON     0 // 0: Disable STOP_BUTTON
#define RUNNING_LED     3 // 0: Disable RUNNING_LED
#define SERIAL_RX       4
#define SERIAL_TX       5
SoftwareSerial Serial2(SERIAL_RX, SERIAL_TX); // RX, TX
#define _MODEL_         "arduino"
#endif

#if defined(__STM32F1__)
#define SUB_TOPIC       "stm32f103/#" // You can change
//#define STOP_BUTTON     PB2 // 0: Disable STOP_BUTTON
#define STOP_BUTTON     0 // 0: Disable STOP_BUTTON
#define RUNNING_LED     PB1 // 0: Disable RUNNING_LED
#define _MODEL_         "stm32f103"
#endif

#define MQTT_SERVER     "broker.hivemq.com"
//#define MQTT_SERVER     "iot.eclipse.org"
#define MQTT_PORT       1883
#define MQTT_KEEP_ALIVE 60
#define MAX_TOPIC       64
#define MAX_MESSAGE     64
#define _DEBUG_         0                      // for Debug

unsigned long lastmillis;
int swState = 0;

void interrupt()
{
  Serial.println("interrupt");
  swState = 1;
}

void putChar(char c) {
  char tmp[10];
  if ( c == 0x0a) {
    Serial.println();
  } else if (c == 0x0d) {
    
  } else if ( c < 0x20) {
    uint8_t cc = c;
    sprintf(tmp,"[0x%.2X]",cc);
    Serial.print(tmp);
  } else {
    Serial.print(c);
  }
}

//Wait for specific input string until timeout runs out
bool waitForString(char* input, int length, unsigned int timeout) {
  unsigned long end_time = millis() + timeout;
  char current_byte = 0;
  int index = 0;

   while (end_time >= millis()) {
    
      if(Serial2.available()) {
        
        //Read one byte from serial port
        current_byte = Serial2.read();
//        Serial.print(current_byte);
        if (_DEBUG_) putChar(current_byte);
        if (current_byte != -1) {
          //Search one character at a time
          if (current_byte == input[index]) {
            index++;
            
            //Found the string
            if (index == length) {              
              return true;
            }
          //Restart position of character to look for
          } else {
            index = 0;
          }
        }
      }
  }  
  //Timed out
  return false;
}

void getResponse(int timeout){
  char c;
  bool flag = false;
  char tmp[10];
  
  long int time = millis() + timeout;
  while( time > millis()) {
    if (Serial2.available()) {
      flag = true;
      c = Serial2.read();
      if (c == 0x0d) {
           
      } else if (c == 0x0a) {
        if (_DEBUG_) Serial.println();
      } else if ( c < 0x20) {
        uint8_t cc = c;
        sprintf(tmp,"[0x%.2X]",cc);
        if (_DEBUG_) Serial.print(tmp);
      } else {
        if (_DEBUG_) Serial.print(c);
      } 
    } // end if
  } // end while
  if (flag & _DEBUG_ ) Serial.println();
}

void errorDisplay(char* buff) {
  int stat = 0;
  Serial.print("Error:");
  Serial.println(buff);
  while(1) {
    if (RUNNING_LED) {
      digitalWrite(RUNNING_LED,stat);
      stat = !stat;
      delay(100);
    }
  }
}

void clearBuffer() {
  while (Serial2.available())
    Serial2.read();
//  Serial.println("");
}

int buildConnect(byte *buf, int keep_alive, char *client_id, char *will_topic, char *will_msg) {
  int rlen = 12;
  int pos = 14;

  int client_id_len = strlen(client_id);
//  Serial.println(client_id_len);
  buf[pos++] = 0x00;
  buf[pos++] = client_id_len;
  for(int i=0;i<client_id_len;i++) {
    buf[pos++] = client_id[i];
  }
  rlen = rlen + 2 + client_id_len;
  
  int will_topic_len = strlen(will_topic);
//  Serial.println(will_topic_len);
  int will_msg_len = strlen(will_msg);
//  Serial.println(will_msg_len);

  if (will_topic_len > 0 && will_msg_len > 0) {
    buf[pos++] = 0x00;
    buf[pos++] = will_topic_len;
    for(int i=0;i<will_topic_len;i++) {
      buf[pos++] = will_topic[i];
    }
    buf[pos++] = 0x00;
    buf[pos++] = will_msg_len;
    for(int i=0;i<will_msg_len;i++) {
      buf[pos++] = will_msg[i];
    }
    rlen = rlen + 2 + will_topic_len + 2 + will_msg_len;  
  }

  buf[0] = 0x10;
  buf[1] = rlen;
  buf[2] = 0x00;
  buf[3] = 0x06;
  buf[4] = 'M';
  buf[5] = 'Q';
  buf[6] = 'I';
  buf[7] = 's';
  buf[8] = 'd';
  buf[9] = 'p';
  buf[10] = 0x03;
  buf[11] = 0x02;
  if (will_topic_len > 0 && will_msg_len > 0) buf[11] = 0x06;
  buf[12] = 0x00;
  buf[13] = keep_alive;
  return buf[1] + 2;  
}

int buildPublish(byte *buf, char *topic, char *msg) {
  int tlen = strlen(topic);
  for(int i=0;i<tlen;i++) {
    buf[4+i] = topic[i];
  }
  int mlen = strlen(msg);
  for(int i=0;i<mlen;i++) {
    buf[4+tlen+i] = msg[i];
  }
  buf[0] = 0x30;
  buf[1] = tlen + mlen + 2;
  buf[2] = 0x00;
  buf[3] = tlen;
  return buf[1] + 2;   
}

int buildSubscribe(byte *buf, char *topic) {
  int tlen = strlen(topic);
  for(int i=0;i<tlen;i++) {
    buf[6+i] = topic[i];
  }
  buf[0] = 0x82;
  buf[1] = tlen + 5;
  buf[2] = 0x00;
  buf[3] = 0x01;
  buf[4] = 0x00;
  buf[5] = tlen;
  buf[tlen+6] = 0x00;
  return buf[1] + 2;   
}


void hexDump(byte *buf, int msize) {
  Serial.print("\nmsize=");
  Serial.println(msize);
  for(int i=0;i<msize;i++) {
    if (buf[i] < 0x10) Serial.print("0");
    Serial.print(buf[i],HEX);
    Serial.print(" ");
  }
  Serial.println();
}

int getIpAddress(char *buf, int szbuf, int timeout) {
  int len=0;
  int pos=0;
  char line[128];
    
  long int time = millis();

  Serial2.print("AT+CIPSTA?\r\n");

  while( (time+timeout) > millis()) {
    while(Serial2.available())  {
      char c = Serial2.read(); // read the next character.
      if (c == 0x0d) {
          
      } else if (c == 0x0a) {
        if (_DEBUG_) {
          Serial.print("Read=[");
          Serial.print(line);
          Serial.println("]");
        }
        int offset;
        for(offset=0;offset<pos;offset++) {
          if(line[offset] == '+') break;
        }
        if (strncmp(&line[offset],"+CIPSTA:ip:",11) == 0) {
          strcpy(buf,&line[12+offset]);
          len = strlen(buf) - 1;
          buf[len] = 0;
        }
        if (strcmp(line,"OK") == 0) return len;
        pos=0;
        line[pos]=0;
      } else {
        line[pos++]=c;
        line[pos]=0;
      }
    }  
  }
  return len;
}

int getMacAddress(char *buf, int szbuf, int timeout) {
  int len=0;
  int pos=0;
  char line[128];
    
  long int time = millis();

  Serial2.print("AT+CIPSTAMAC?\r\n");

  while( (time+timeout) > millis()) {
    while(Serial2.available())  {
      char c = Serial2.read(); // read the next character.
      if (c == 0x0d) {
          
      } else if (c == 0x0a) {
        if (_DEBUG_) {
          Serial.print("Read=[");
          Serial.print(line);
          Serial.println("]");
        }
        if (strncmp(line,"+CIPSTAMAC:",11) == 0) {
          strcpy(buf,&line[12]);
          len = strlen(buf) - 1;
          buf[len] = 0;
        }
        if (strcmp(line,"OK") == 0) return len;
        pos=0;
        line[pos]=0;
      } else {
        line[pos++]=c;
        line[pos]=0;
      }
    }  
  }
  return len;
}

void setup(){
  char at[128];
  byte buf[128];
  int msize;

  Serial.begin(9600);
  Serial2.begin(4800); 
  while(!Serial2);

  if (RUNNING_LED) {
    pinMode(RUNNING_LED,OUTPUT);
    digitalWrite(RUNNING_LED,LOW);
  }
  if (STOP_BUTTON) attachInterrupt(0, interrupt, FALLING);

  //Enable autoconnect
  Serial2.print("AT+CWAUTOCONN=1\r\n");
  if (!waitForString("OK", 2, 1000)) {
    errorDisplay("AT+CWAUTOCONN Fail");
  }
  clearBuffer();
 
  Serial2.print("AT+RST\r\n");
  if (!waitForString("WIFI GOT IP", 11, 10000)) {
    errorDisplay("AT+RST Fail");
  }
  clearBuffer();

  //Establishes TCP Connection
  sprintf(at,"AT+CIPSTART=\"TCP\",\"%s\",%d\r\n",MQTT_SERVER,MQTT_PORT);
  Serial2.print(at);
  if (!waitForString("OK", 2, 5000)) {
    errorDisplay("AT+CIPSTART Fail");
  }
  clearBuffer();

  //Get My IP Address
  char IPaddress[64];
  getIpAddress(IPaddress,sizeof(IPaddress),2000);
  Serial.print("IPaddress=[");
  Serial.print(IPaddress);
  Serial.println("]");
  
  //Get My MAC Address
  char MACaddress[64];
  getMacAddress(MACaddress,sizeof(MACaddress),2000);
  Serial.print("MACaddress=[");
  Serial.print(MACaddress);
  Serial.println("]");

  //Client requests a connection to a server
  Serial.print("MQTT CONNECTT.....");
  msize =buildConnect(buf,MQTT_KEEP_ALIVE,MACaddress,"","");
  if (_DEBUG_) hexDump(buf,msize);
  sprintf(at,"AT+CIPSEND=%02d\r\n",msize);
  Serial2.print(at);
  if (!waitForString("OK", 2, 5000)) {
    errorDisplay("AT+CIPSEND Fail");
  }
  if (!waitForString(">", 1, 5000)) {
    errorDisplay("Server Not Response");
  }
  clearBuffer();
  for (int i=0;i<msize;i++)Serial2.write(buf[i]);
  if (!waitForString("SEND OK", 7, 5000)) {
    errorDisplay("Server Not Receive my data");
  }
  //Wait for CONNACK
  if (!waitForString("+IPD", 4, 5000)) {
    errorDisplay("CONNACK Fail");
  }
  getResponse(1000);
  Serial.println("OK");

  //Client requests a subscribe to a server
  Serial.print("MQTT SUBSCRIBE.....");
  msize =buildSubscribe(buf,SUB_TOPIC);
  if (_DEBUG_) hexDump(buf,msize);
  sprintf(at,"AT+CIPSEND=%02d\r\n",msize);
  Serial2.print(at);
  if (!waitForString("OK", 2, 5000)) {
    errorDisplay("AT+CIPSEND Fail");
  }
  if (!waitForString(">", 1, 5000)) {
    errorDisplay("Server Not Response");
  }
  clearBuffer();
  for (int i=0;i<msize;i++)Serial2.write(buf[i]);
  if (!waitForString("SEND OK", 7, 5000)) {
    errorDisplay("Server Not Receive my data");
  }
  //Wait for SUBACK
  if (!waitForString("+IPD", 4, 5000)) {
    errorDisplay("SUBACK Fail");
  }
  getResponse(1000);
  Serial.println("OK");

  Serial.println("Start MQTT Subscribe [" + String(_MODEL_) + "] from " + String(MQTT_SERVER));
  lastmillis = millis();
}

void loop(){
  static int timer = 0;
  static int running_state = 1;
  byte ack[] = {0x40,0x02};
  char c;
  int rflag = 0;
  static int step = 0;
  static int len0 = 0;
  static int len1 = 0;
  static int len2 = 0;
  static int pos1 = 0;
  static int pos2;
  static char buf1[MAX_TOPIC];
  static char buf2[MAX_MESSAGE];
  byte pingreq[] = {0xc0,0x00};
  byte disconnect[] = {0xe0,0x00};

  if (swState == 1) {
    Serial.println("Sending DISCONNEC");
    Serial2.print("AT+CIPSEND=02\r\n");
    if (!waitForString("OK", 2, 5000)) {
      errorDisplay("AT+CIPSEND Fail");
    }
    clearBuffer();

    if (!waitForString(">", 1, 5000)) {
      errorDisplay("Server Not Response");
    }
    clearBuffer();
    for (int i=0;i<2;i++)Serial2.write(disconnect[i]); 
    if (!waitForString("SEND OK", 7, 5000)) {
      errorDisplay("Server Not Receive my data");
    }

    if (!waitForString("CLOSE", 5, 5000)) {
      errorDisplay("CLOSE Fail");
    }
    clearBuffer();
    Serial.println();
    Serial.println("Subscribe end");
    if (RUNNING_LED) digitalWrite(RUNNING_LED,LOW);
    while(1) { }
  }

/*
 * Subscribe Message Structure
 * 
 * [LF][CR]+IPD,AA:BCDEfffffffffggggg
 * 
 * C:{number of f} ＋ {number of g} ＋ 2
 * E:{number of f}
 * {number of g} = C-E-2
 * f:Topic
 * g:Payload
 * 
 * Example
 * +IPD,XX:[0x00][0x1E][0x00][0x18][24 byte Topic][4 byte Payload]
 * C=0x1E=30
 * {number of f}=0x18=24
 * {number of g}=0x1E-0x18=30-24-2=4
 */
  while (Serial2.available()) {
    c = Serial2.read();
    if (_DEBUG_) putChar(c);
    if (c == 0x0d) {
//      Serial.println("LF");
    } else if (c == 0x0a) {
//      Serial.println("CR");
      step = 1;
      pos1 = 0;
    } else {
      if(step < 3) {
        if (step && c == ',') step++;
        if (step && c == ':') step++;
      } else if (step < 7) {
        step++;
        if (step == 5) len0 = c;
        if (step == 7) len1 = c;
      } else if (step == 7) {
        if (pos1 == MAX_TOPIC) {
          Serial.println("Topic is too large!!");
          continue;
        }
        buf1[pos1++]=c;
        buf1[pos1]=0;
//        Serial.print("pos1=");
//        Serial.println(pos1);
//        Serial.print("buf1=");
//        Serial.println(buf1);
        if (pos1 == len1) {
          step++;
          len2 = len0 - len1 -2;
          pos2 = 0;
        }
      } else if (step == 8) {
        if (pos2 == MAX_MESSAGE) {
          Serial.println("Message is too large!!");
          continue;
        }
        buf2[pos2++]=c;
        buf2[pos2]=0;
//        Serial.print("pos2=");
//        Serial.println(pos2);
//        Serial.print("buf2=");
//        Serial.println(buf2);
        if (pos2 == len2) {
          rflag++;
        }
      }
#if 0
      Serial.print("step=");
      Serial.print(step);
      Serial.print(" len0=");
      Serial.print(len0);
      Serial.print(" len1=");
      Serial.print(len1);
      Serial.print(" len2=");
      Serial.print(len2);
      Serial.print(" pos1=");
      Serial.print(pos1);
      Serial.print(" pos2=");
      Serial.println(pos2);
#endif
    }
  }

  if (rflag) {  //Publish acknowledgment
    Serial.println();
    Serial.println("Publish acknowledgment");
    Serial2.print("AT+CIPSEND=02\r\n");
    if (!waitForString("OK", 2, 5000)) {
      errorDisplay("AT+CIPCLOSE Fail");
    }
    clearBuffer();

    for (int i=0;i<2;i++)Serial2.write(ack[i]);
    if (!waitForString("SEND OK", 7, 5000)) {
      errorDisplay("Server Not Receive my data");
    }
    clearBuffer();

    Serial.println();
    for(int i=0;i<40;i++) Serial.print("-");
    Serial.println();
    Serial.print("Topic=[");
    Serial.print(buf1);
    Serial.println("]");
    Serial.print("Payload=[");
    Serial.print(buf2);
    Serial.println("]");
    for(int i=0;i<40;i++) Serial.print("-");
    Serial.println();
    rflag = 0;
  }

  unsigned long now = millis();
  if ( (now - lastmillis) < 0) {
    lastmillis = now;
  }
  if ( (now - lastmillis) > 1000) {
    lastmillis = now;
    timer++;
    if (RUNNING_LED) digitalWrite(RUNNING_LED,running_state);
    running_state = !running_state;
//    Serial.print("running_state=");
//    Serial.print(running_state);
//    Serial.print(" timer=");
//    Serial.println(timer);
    if ( (timer % 10) == 0) Serial.print("+");
    if ( (timer % 10) != 0) Serial.print(".");

    if (timer == MQTT_KEEP_ALIVE) { //  PingReq
      Serial.println("Sending PINGREQ");
      Serial2.print("AT+CIPSEND=02\r\n");
      if (!waitForString("OK", 2, 5000)) {
        errorDisplay("AT+CIPSEND Fail");
      }
//      clearBuffer();
  
      if (!waitForString(">", 1, 5000)) {
        errorDisplay("Server Not Response");
      }
      clearBuffer();
      for (int i=0;i<2;i++)Serial2.write(pingreq[i]); 
      if (!waitForString("SEND OK", 7, 5000)) {
        errorDisplay("Server Not Receive my data");
      }
      //Wait for PINGRESP
      waitForString("+IPD", 4, 1000);
#if 0
      if (!waitForString("+IPD", 4, 5000)) {
        errorDisplay("Server Not Receive my data");
      }
      getResponse(1000);
#endif
      timer = 0;
    }
  }

}

