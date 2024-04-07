#include <Adafruit_SSD1306.h>
#include <DHTesp.h>
#include <Wire.h> //responsible for I2C communication
#include <Adafruit_GFX.h>
#include <WiFi.h>
#include <time.h>          
#include <math.h>
#include <PubSubClient.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <iostream>
#include <ESP32Servo.h>
#include <sstream>
#include <cstdlib>

using namespace std;



#define NTP_SERVER "pool.ntp.org"
long int UTC_OFFSET = 0;
#define UTC_OFFSET_DST 0


#define BUZZER 5
#define LED_1 25
#define PB_CANCEL 33
#define PB_OK 32
#define PB_UP 34
#define PB_DOWN 35

#define DHT_PIN 15

//Parameters

const int LDR = 36;

const float GAMMA = 0.7;
const float RL10 = 50;

char LDR_chr[8];

int pos = 0;

int posOffset = 30;

float intensity;
float controlFactor = 0.75;

const int servoPin = 18;

Servo servo;

bool isScheduledON = false;
unsigned long scheduledOnTime;

#define SCREENWIDTH 128
#define SCREENHEIGHT 64
#define OLEDRESET -1 //same reset as the esp32
#define SCREEN_ADDRESS 0x3C //FROM WOKWI for i2c communication

Adafruit_SSD1306 display(SCREENWIDTH, SCREENHEIGHT, &Wire, OLEDRESET);



DHTesp dhtsensor;

WiFiClient espClient;
PubSubClient mqttClient(espClient);
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

int C = 262;
int D = 294;
int E = 330;
int F = 349;
int G = 392;
int A = 440;
int B = 494;
int C_H = 523;

//dht22 gives digital values

int notes[] = {C,D,E,F,G,A,B,C_H};

int n_notes = 8;

int days = 0;
int hours = 0;
int minutes = 0;
int seconds = 0;


unsigned long timeNow = 0;
unsigned long timeLast = 0;

boolean alarm_enabled =   true;
int n_alarms = 3;
int alarm_hours[] = {0,1,2}; 
int alarm_minutes[] = {1,10,20};


bool alarms_triggered[] = {false, false, false}; 

int current_mode = 0;
int max_modes = 5;
String modes[] = { "1 - Input Offset to UTC", "2 - Set Alarm 1", "3 - Set Alarm 2", " 4 - Set Alarm 3", "5 - Cancel All Alarms"};

char tempAr[6];
char humidityAr[6];

int pressed;
//functions

void ring_alarm();
void print_line( String text, int text_size, int row, int coloumn);
void update_time(void);
void update_time_wifi(void);
void print_time_now();
void update_time_with_check_alarms();
int wait_for_button_press();
void go_to_menu();
void run_mode(int mode);
void get_utc_offset();
void set_alarm(int alarm);
void check_temp();
void ring_buzzer();



void setupMqtt();
void connectToBroker();
void buzzerOn(bool on);
void checkSchedule();
void setupWifi();
unsigned long getTime();
void updatelux();
void servopos();


// long press buttons for functionality


void setup() {
  
  pinMode(LED_1, OUTPUT);
  pinMode(BUZZER, OUTPUT);

  pinMode(PB_CANCEL, INPUT);
  pinMode(PB_OK, INPUT);
  pinMode(PB_UP, INPUT);
  pinMode(PB_DOWN, INPUT);

  
  dhtsensor.setup(DHT_PIN, DHTesp::DHT22);

  Serial.begin(115200); //to activate serial monitor
  

  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS))
  {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }

  display.display();
  delay(500);

  display.clearDisplay();

  
  print_line("Welcome to MediBox",2,0,0);
  TempAndHumidity data = dhtsensor.getTempAndHumidity();

  delay(3000);

  WiFi.begin("Wokwi-GUEST", "", 6);
  while (WiFi.status() != WL_CONNECTED) {

    delay(250);
    display.clearDisplay();
    print_line("Connecting to Wi-Fi",2,0,0);
    
  }

  display.clearDisplay();
  print_line("Connected to Wi-Fi",2,0,0);
  display.clearDisplay();

  
  
  configTime(UTC_OFFSET, UTC_OFFSET_DST, NTP_SERVER);

  setupMqtt();

  timeClient.begin();
  timeClient.setTimeOffset(5.5*3600);

  
  digitalWrite(BUZZER, LOW);
  servo.attach(servoPin, 500, 2400);
  
  
}

void loop() {
  
  if (!mqttClient.connected())
  {
    connectToBroker();
  }

  mqttClient.loop();

  update_time_with_check_alarms();

  if (digitalRead(PB_OK) == LOW)
  {
    delay(200); //to debounce the pushbutton
    display.clearDisplay();
    print_line("Menu",2,0,0);
    delay(200);
    go_to_menu();
  }

  check_temp();

  checkSchedule();
  updatelux();
  servopos();

} 

void ring_alarm() 
{
  display.clearDisplay();
  print_line("MEDICINE TIME!",2,0,0);

  digitalWrite(LED_1, HIGH);

  bool break_happened = false; 
  while (break_happened == false && digitalRead(PB_CANCEL)==HIGH)
  {
    for (int i=0; i<n_notes ; i++)
    {
      if (digitalRead(PB_CANCEL) == LOW) 
      {
        delay(200); //to stop bouncing of pushbutton
        break_happened = true;
        break; //so that user can stop midstop of the ringing tone
      }
      tone(BUZZER, notes[i]);
      delay(500);
      noTone(BUZZER);
      delay(2);
    }
  }
  delay(200);
  digitalWrite(LED_1, LOW);
  display.clearDisplay();
}

unsigned long getTime()
{
  timeClient.update();
  return timeClient.getEpochTime();

}

void setupMqtt()
{
  mqttClient.setServer("test.mosquitto.org",1883);
  mqttClient.setCallback(receiveCallback); //we are passing a function here

}

void receiveCallback(char* topic, byte* payload, unsigned int length) //parameters should be in order to pass function
{
  Serial.print("Message Arrived [");
  Serial.print(topic);
  Serial.print("]");

  //nned to save payload in an array to use later. 

  char payloadCharAr[length];

  for (int i = 0; i< length; i++)
  {
    Serial.print((char)payload[i]); //cast to chat as print doesnt support bytes
    payloadCharAr[i] = (char)payload[i];

  }

  Serial.println("");

  //if (topic == "")  cant compare strongs like this in cpp
  
  if (strcmp(topic,"MEDIBOX_200664P_ON_OFF") == 0) //equal
  {
      buzzerOn(payloadCharAr[0] == '1');
  }

  else if (strcmp(topic,"MEDIBOX_200664P_SCHEDULE") == 0) //equal
  {

      if (isdigit(payloadCharAr[0]))
      {
        isScheduledON = true;
        scheduledOnTime = atol(payloadCharAr);
        
      }

      else 
      {
        isScheduledON = false;//converts array to long
       
      }
  }

  else if ((strcmp(topic,"MEDIBOX_200664P_CF") == 0))
  {
    istringstream iss(payloadCharAr);
    iss >> controlFactor;
  }

  else if ((strcmp(topic,"MEDIBOX_200664P_MA") == 0))
  {
    istringstream iss(payloadCharAr);
    iss >> posOffset;
  }
  

}

void connectToBroker()
{
  while (!mqttClient.connected())
  {
    Serial.print("Attempting to connect to MQTT..");
    if (mqttClient.connect("ESP32-2566")) //random number here, need to add specific ID if authenticated
    {
      Serial.print("Connected");
      mqttClient.subscribe("MEDIBOX_200664P_ON_OFF"); //Trying to manually switch on off first, then schedule. stopped here. 35.37
      mqttClient.subscribe("MEDIBOX_200664P_SCHEDULE");
      mqttClient.subscribe("MEDIBOX_200664P_CF");
      mqttClient.subscribe("MEDIBOX_200664P_MA");
    }

    else{
      Serial.print("failed ");
      Serial.print(mqttClient.state()); //for info of user
      delay(5000); //to wait before trying again
    }
  }
}

void checkSchedule()
{
  if (isScheduledON)
  {
    unsigned long currentTime = getTime();
    if (currentTime > scheduledOnTime )
    {
      buzzerOn(true);
      isScheduledON = false;
      mqttClient.publish("MEDIBOX_200664P_ON_OFF_ESP", "1");
      mqttClient.publish("MEDIBOX_200664P_SCHEDULE_ESP", "0");
      print_line("Scheduled time!!",1,0,0);
    }
  }

}

void updatelux()
{

  int analogValue = analogRead(LDR);
  
  
  analogValue = map(analogValue,4095,0,1023,0);
  float voltage = (analogValue / 1024.)*5 ;
  float resistance = 2000 * voltage / (1 - voltage / 5);
  float lux = pow(RL10 * 1e3 * pow(10, GAMMA) / resistance, (1 / GAMMA));
  
  float intensity = (lux/100000);
  
  String(intensity, 6).toCharArray(LDR_chr, 8);

 
  mqttClient.publish("MEDIBOX_200664P_lux", LDR_chr);
  
}

void servopos()
{
  pos = posOffset + ((180 - posOffset)*intensity*controlFactor);
  
  servo.write(pos);
  
}

void buzzerOn(bool on)
{
  if (on)
  {
    tone(BUZZER,256);
    print_line("Check Website",1,0,0);
    delay(500);
  }
  
  else{
    noTone(BUZZER);
  }
}

void print_line( String text, int text_size, int row, int coloumn)
{
  display.setTextSize(text_size);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(coloumn, row);
  display.print(text);

  display.display();
}

void update_time_wifi(void)
{
  configTime(UTC_OFFSET, UTC_OFFSET_DST, NTP_SERVER);

  struct tm timeinfo;
  getLocalTime(&timeinfo);

  char day_str[8];
  char hour_str[8];
  char min_str[8];
  char sec_str[8];

  strftime(day_str, 8, "%d", &timeinfo);
  strftime(sec_str, 8, "%S", &timeinfo);
  strftime(hour_str, 8, "%H", &timeinfo);
  strftime(min_str, 8, "%M", &timeinfo);

  hours = atoi(hour_str);
  minutes = atoi(min_str);
  days = atoi(day_str);
  seconds = atoi(sec_str);
}

void print_time_now()
{

  display.clearDisplay();
  print_line("Date : " + String(days),1,0,40);
  print_line(String(hours),2,20,30);
  print_line(":",2,20,50);
  print_line(String(minutes),2,20,60);
  print_line(":",2,20,80);
  
  print_line(String(seconds),2,20,90);  

}


void update_time_with_check_alarms()
{
  
  update_time_wifi();
  print_time_now();

  if (alarm_enabled == true)
  {
    for (int i=0; i<n_alarms; i++)
    {
      if (alarms_triggered[i]==false && alarm_hours[i]==hours && alarm_minutes[i] == minutes )
      {
          ring_alarm();
          alarms_triggered[i] = true;
      }
    }
  }
}

int wait_for_button_press()
{
  while (true)
  {
    if (digitalRead(PB_UP)==LOW)
    {
      delay(200);
      return PB_UP;
    }

    else if (digitalRead(PB_DOWN)==LOW)
    {
      delay(200);
      return PB_DOWN;
    }

    else if (digitalRead(PB_OK)==LOW)
    {
      delay(200);
      return PB_OK;
    }

    else if (digitalRead(PB_CANCEL)==LOW)
    {
      delay(200);
      return PB_CANCEL;
    }

  }


}
void go_to_menu()           
{
  display.clearDisplay();
  while (digitalRead(PB_CANCEL) == HIGH)
  {
    
    display.clearDisplay();
    print_line(modes[current_mode],2,0,0);

    pressed = wait_for_button_press();

    if (pressed == PB_UP)
    {
      delay(200);
      current_mode += 1;
      current_mode = current_mode % max_modes;
      print_line(modes[current_mode],2,0,0);
      delay(100);
    } 

    else if (pressed == PB_DOWN)
      {
        delay(200);
        current_mode -= 1;
        if (current_mode < 0)
        {
          current_mode = max_modes - 1;
        }
        print_line(modes[current_mode],2,0,0);
        delay(100);
      } 
  

    else if (pressed == PB_OK)
      {
        delay(200);
        run_mode(current_mode);

      } 

    else if (pressed == PB_CANCEL)
      {
        delay(200);
        break;
      } 
  }
}

void run_mode(int mode)
{ 
  if (mode == 0)
  {
    get_utc_offset();
    configTime(UTC_OFFSET, UTC_OFFSET_DST, NTP_SERVER);

  } 

  else if (mode == 1 || mode == 2 || mode == 3 )
  {
    set_alarm(mode-1);
  }
  
  else if (mode == 4)
  {
    alarm_enabled = false;
    print_line("Disabled all alarms",2,0,0);
    delay(1000);
  }
}

void get_utc_offset()
{ 
  int temp_hour = 0; 
  display.clearDisplay();
  print_line("Enter hour shift:" + String(temp_hour),2,0,0); 
  

  while(true)
  {
    pressed = wait_for_button_press();
    display.clearDisplay();
    if (pressed == PB_UP)
    {
      delay(200);
      temp_hour+= 1;
      if (temp_hour > 12)
      {
        temp_hour = -12;
      }
      print_line("Enter hour shift:" + String(temp_hour),2,0,0);
      delay(100);
    } 

    else if (pressed == PB_DOWN)
      {
        delay(200);
        temp_hour -= 1;
        if (temp_hour < -12)
        {
          temp_hour = 12;
        }
        print_line("Enter hour shift:" + String(temp_hour),2,0,0);
        delay(100);
      } 

    else if (pressed == PB_OK)
      {
        delay(200);
        hours = temp_hour;
        break;
      } 

    else if (pressed == PB_CANCEL)
      {
        delay(200);
        break;
      } 
  }
   
  int temp_minutes = 0; 

  display.clearDisplay();
  print_line("Enter minutes shift:" + String(temp_minutes),2,0,0); 

  while(true)
  {
    pressed = wait_for_button_press();
    display.clearDisplay();
    if (pressed == PB_UP)
    {
      
      delay(200);
      temp_minutes += 30;
      if (temp_minutes > 30)
      {
        temp_minutes = 0;
      }
       print_line("Enter minutes shift:" + String(temp_minutes),2,0,0);
       delay(100);
    } 

    else if (pressed == PB_DOWN)
      {
        delay(200);
        temp_minutes -= 30;
        if (temp_minutes < 0)
        {
          temp_minutes = 30;
        } 
      } 

    else if (pressed == PB_OK)
      {
        delay(200);
        minutes = temp_minutes;
        break;
      } 

    else if (pressed == PB_CANCEL)
      {
        delay(200);
        break;
      } 
  } 

  if (hours >= 0)
  {
    UTC_OFFSET = (hours*3600) + (minutes*60);
  }
  else
  {
    UTC_OFFSET = ((static_cast<int>(abs(hours))*3600) + (minutes*60))*(-1);
  } 
  display.clearDisplay();
  print_line("UTC OFFSET SET "+String(hours)+" : " + String(minutes),2,0,0);
  delay(1000);

}

void set_alarm(int alarm) 
{
  int temp_hour = alarm_hours[alarm]; 

  display.clearDisplay();
  print_line("Enter alarm hour:" + String(temp_hour),2,0,0); 
  
  
  while(true)
  { 
    pressed = wait_for_button_press();
    display.clearDisplay();
    if (pressed == PB_UP)
    {
      delay(200);
      temp_hour+= 1;
      if (temp_hour > 23)
      {
        temp_hour = 0;
      }
      print_line("Enter alarm hour:" + String(temp_hour),2,0,0); 
      delay(100);
    } 

    else if (pressed == PB_DOWN)
      {
        delay(200);
        temp_hour -= 1;
        if (temp_hour < 0)
        {
          temp_hour = 23;
        } 
        print_line("Enter alarm hour:" + String(temp_hour),2,0,0); 
        delay(100);
      } 

    else if (pressed == PB_OK)
      {
        delay(200);
        alarm_hours[alarm] = temp_hour; 

        break;

      } 

    else if (pressed == PB_CANCEL)
      {
        delay(200);
        break;
      } 
  }
   
  int temp_minutes = alarm_minutes[alarm]; 
  display.clearDisplay();
  print_line("Enter alarm minutes:" + String(temp_minutes),2,0,0); 

  

  while(true)   
  {
    pressed = wait_for_button_press();
    display.clearDisplay();
    if (pressed == PB_UP)
    {
      delay(200);
      temp_minutes += 1;
      if (temp_minutes > 59)
      {
        temp_minutes = 0;
      }
      print_line("Enter alarm minutes:" + String(temp_minutes),2,0,0); 
      delay(100);
    } 

    else if (pressed == PB_DOWN)
      {
        delay(200);
        temp_minutes -= 1;
        if (temp_minutes < 0)
        {
          temp_minutes = 59;
        } 
        print_line("Enter alarm minutes:" + String(temp_minutes),2,0,0); 
        delay(100);
      } 

    else if (pressed == PB_OK)
      {
        delay(200);
        alarm_minutes[alarm] = temp_minutes;
        break;
      } 

    else if (pressed == PB_CANCEL)
      {
        delay(200);
        break;
      } 
  } 
  
  display.clearDisplay();
  print_line("Alarm set to " + String(temp_hour) +  " : " + String(temp_minutes),1,0,0);
  delay(1000);
  print_line("Alarm " + String(alarm+1) + " is set",2,0,0);
  delay(1000);
  
}

void ring_buzzer()
{
  bool break_happened_buzzer = false; 
  while (break_happened_buzzer == false && digitalRead(PB_CANCEL)==HIGH)
  {
    for (int i=0; i<n_notes ; i++)
    {
      if (digitalRead(PB_CANCEL) == LOW) 
      {
        delay(200); //to stop bouncing of pushbutton
        break_happened_buzzer = true;
        break; //so that user can stop midstop of the ringing tone
      }
      tone(BUZZER, notes[i]);
      delay(500);
      noTone(BUZZER);
      delay(2);
    }
  }

  digitalWrite(LED_1, LOW);

}

void check_temp()
{
  TempAndHumidity data = dhtsensor.getTempAndHumidity();

  String(data.temperature,2).toCharArray(tempAr,6);
  String(data.humidity,2).toCharArray(humidityAr,6);

  mqttClient.publish("MEDIBOX_200664P_TEMP",tempAr);
  
  bool all_good = true;
  delay(200);
  print_line( "Humidity: " + String(data.humidity) + "%" , 1,40, 0);
  print_line( "Temperature: " + String(data.temperature) + "'C" , 1,50, 0);
  

  if (data.temperature < 26)
  { 
    all_good = false;
    display.clearDisplay();
    while (digitalRead(PB_CANCEL) == HIGH) //recheck for bouncing
    { 
      
      delay(200);
      print_line("ALERT!! LOW TEMPERATURE",1,0,40);
      digitalWrite(LED_1, HIGH);
      ring_buzzer();
    }
  }
  
  else if (data.temperature > 32)
  { 
    all_good = false;
    display.clearDisplay();
    while (digitalRead(PB_CANCEL) == HIGH)
    {
      
      delay(200);
      print_line("ALERT!! HIGH TEMPERATURE",1,30,40);
      digitalWrite(LED_1, HIGH);
      ring_buzzer();
    }
  }

  else if (data.humidity < 60)
  {
    all_good = false;
    display.clearDisplay();
    while (digitalRead(PB_CANCEL) == HIGH) //recheck for bouncing
    { 
      
      delay(200);
      print_line("ALERT!! LOW HUMIDITY",1,0,40);
      digitalWrite(LED_1, HIGH);
      ring_buzzer();
    }
  }
  
  else if (data.humidity > 80)
  { 
    all_good =  false;
    display.clearDisplay();
    while (digitalRead(PB_CANCEL) == HIGH )
    {
      delay(200);
      print_line("ALERT!! HIGH HUMIDITY",1,30,40);
      digitalWrite(LED_1, HIGH);
      ring_buzzer();
    }
  
  if (all_good)
  {
    display.clearDisplay();
    digitalWrite(LED_1, LOW);
  }

  }


}
