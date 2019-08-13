// Libraries... 
#include <M5StickC.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include <HTTPClient.h>
#include <TimeLib.h>
//#include <WiFiClientSecure.h>

// sprites are 16x16x16 uint8_t 
#include "img_data.h"

#define GRIDX 80
#define GRIDY 160
#define NBUTTON 8
#define BUTTONH 40
#define DEBUG 1
#define SPRITEDIM 16
#define SPRITEDEPTH 16
#define SPRITEFRAMES 16
#define SPRITEFRMSZ SPRITEDIM*SPRITEDIM*SPRITEFRAMES/8
#define SPRITEMOD 16
#define UTCSHIFT 18000 // USA/central

/////
// Global device variables. 
unsigned long offset_days = 3;    // 3 days
unsigned long t_unix_date1, t_unix_date2;

WiFiMulti wifi_multi;
const char grab_dump_url[] = "https://api.keyvalue.xyz/";
const char grab_dump_key[] = "/M5Stack";
const char grab_dump_token[] = "40e95ed0";

uint8_t brightness = 7;
float batt_voltage=0.;
float batt_temp=0.;

int16_t accX = 0;
int16_t accY = 0;
int16_t accZ = 0;

// To avoid time flicker. 
uint8_t rtc_hour = 0; 
uint8_t rtc_min = 0; 
uint8_t rtc_sec = 0; 

uint8_t sprite_frame = 0; 

//int16_t gyroX = 0;
//int16_t gyroY = 0;
//int16_t gyroZ = 0;

unsigned long curr_time = millis();
float cursorfX = 40.0;
float cursorfY = 80.0;
float cursorVX = 0.; 
float cursorVY = 0.;
float cursorAX = 0.; 
float cursorAY = 0.;
uint16_t cursorX = (int)cursorfX; 
uint16_t cursorY = (int)cursorfY; 
const uint16_t cursorDIM = 9;
const uint16_t cursor_565[81] = {
  0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,
  0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,
  0x0000,0x0000,0x0FF0,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,
  0x0000,0x0000,0x0000,0xF81F,0x03EF,0xF81F,0x0000,0x0000,0x0000,  
  0x0000,0x0000,0x0000,0x03EF,0xFFFF,0x03EF,0x0000,0x0000,0x0000,  
  0x0000,0x0000,0x0000,0xF81F,0x03EF,0xF81F,0x0000,0x0000,0x0000,  
  0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,
  0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,
  0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,
  };
 
void cursor_verlet(){
  //
  // Both integrate the cursor 
  // and display the cursor
  //
  float dt = (float)(millis()-curr_time)/2.5;
  float ax = -0.07*((float) accX) * M5.IMU.aRes; 
  float ay = 0.08*((float) accY) * M5.IMU.aRes; 
  cursorfX += constrain(dt*(cursorVX + 0.5*dt*ax),-cursorDIM/4,cursorDIM/4); 
  cursorfY += constrain(dt*(cursorVY + 0.5*dt*ay),-cursorDIM/4,cursorDIM/4); 
  cursorVX += 0.5*dt*(cursorAX + ax)-0.08*cursorVX;
  cursorVY += 0.5*dt*(cursorAY + ay)-0.08*cursorVY;
  cursorAX = ax; 
  cursorAY = ay; 
//  Serial.printf("%.2f %.2f %.2f %.2f %.2f %.2f %.2f \n",dt, cursorfX, cursorfY, cursorVX, cursorVY, cursorAX, cursorAY);
  if (cursorfX >= GRIDX-cursorDIM)
  {
    cursorfX = (float)(GRIDX-cursorDIM);
    cursorVX = -0.5;
    cursorAX = 0;
  }
  else if (cursorfX < 1)
  {
    cursorfX = 1.;
    cursorVX = 0.5;
    cursorAX = 0;
  }
  if (cursorfY >= (GRIDY-cursorDIM))
  {
    cursorfY = (float)(GRIDY-cursorDIM);
    cursorVY = -0.5;
    cursorAY = 0;
  }
  else if (cursorfY < 1)
  {
    cursorfY = 1;
    cursorVY = 0.5;
    cursorAY = 0;    
  }
  cursorX = (uint16_t)cursorfX; 
  cursorY = (uint16_t)cursorfY;
  if (cursorX >= GRIDX-cursorDIM || cursorY >=GRIDY-cursorDIM)
    return; 
    M5.Lcd.pushImage(cursorX, cursorY, cursorDIM, cursorDIM, cursor_565);
  curr_time = millis();
  }

void read_accel(){
//    M5.IMU.getGyroAdc(&gyroX,&gyroY,&gyroZ);
    M5.IMU.getAccelAdc(&accX,&accY,&accZ);
//    Serial.printf("Acc %.1f %.1f %.1f\n",((float) accX) * M5.IMU.aRes,((float) accY) * M5.IMU.aRes,((float) accZ) * M5.IMU.aRes);
};

void read_time(struct tm* timeinfo)
{
  M5.Rtc.GetBm8563Time();
  rtc_hour = M5.Rtc.Hour;
  rtc_min = M5.Rtc.Minute;
  rtc_sec = M5.Rtc.Second; 
  timeinfo->tm_sec = M5.Rtc.Second;
  timeinfo->tm_min = M5.Rtc.Minute;
  timeinfo->tm_hour = M5.Rtc.Hour;
  timeinfo->tm_mday = M5.Rtc.Day;
  timeinfo->tm_mon = M5.Rtc.Month;
  timeinfo->tm_year = M5.Rtc.Year;
};

void read_power(){
    batt_voltage = M5.Axp.GetVbatData() * 1.1 / 1000;
    batt_temp = -144.7 + M5.Axp.GetTempData() * 0.1;
};

void cycle_brightness(){
    brightness++;
    if(brightness >= 16)
      brightness = 7;
    M5.Axp.ScreenBreath(brightness);  
    delay(500);
};

void start_sleep(){
  delay(500);
  M5.Axp.ScreenBreath(0);
  while(true)
  {
    M5.Axp.LightSleep(1000000);
    if (M5.BtnA.isPressed())
      break; 
  }
  M5.Axp.ScreenBreath(8);
  launch_main();
};

void draw_sprite(uint8_t x_, uint8_t y_,const uint8_t* root_ptr){
  sprite_frame++;
  sprite_frame = sprite_frame%(SPRITEFRAMES*SPRITEFRAMES);
  M5.Lcd.drawBitmap(x_,y_,SPRITEDIM,SPRITEDIM,(const uint16_t*)(root_ptr+(sprite_frame/SPRITEFRAMES)*SPRITEFRMSZ));
};

class region {
  public: 
  const char* title = "region";
  uint8_t x;
  uint8_t y;
  uint8_t w;
  uint8_t h;
  uint8_t n_buttons=0;
  region* buttons[NBUTTON];
  void (*click_fcn)() = NULL; 
  public:
    region(uint8_t x_=0, uint8_t y_=0,
                  uint8_t w_=80, uint8_t h_=20,
                  char* title_ = "region")
    {
      x = x_; y=y_; w=w_; h=h_;
      title = title_;
    };
    ~region()
    { 
        for (int i=0; i<n_buttons;++i)
          delete buttons[i];
    };
    void launch(region*& attention)
    {
      attention = this; 
      this->redraw(); 
      return;
    };
    bool inBounds(uint8_t x_, uint8_t y_) {
      if (x <= x_ && x_ <= x+w)
      {
        if (y <= y_ && y_ <= y+h)
          return true; 
      }
      return false; 
    };
    void addChild(region* child)
    {
      if (n_buttons >= NBUTTON-1)
        return;
      buttons[n_buttons] = child; 
      n_buttons++; 
    };
    void addClickFcn(void (*f)())
    {
       click_fcn = f;  
    };
    region* getChild(uint8_t x_, uint8_t y_)
    {
        for (int i=0; i<n_buttons;++i)
        {
          if (buttons[i]->inBounds(x_,y_))
            return buttons[i]; 
        }
        return buttons[0];
    };
    virtual void onClick(uint8_t x_, uint8_t y_) 
    {
      if (n_buttons>0)
        getChild(x_,y_)->onClick(x_,y_);
      else if (inBounds(x_, y_))
      {  
        M5.Lcd.fillRoundRect(x, y, w, h, 5, 0xFFFF);
        delay(30);
        M5.Lcd.fillRoundRect(x, y, w, h, 5, 0x0000);
        onDraw(x_,y_);
        if (!(click_fcn==NULL))
          click_fcn(); 
      }
    };
    virtual void onDraw(uint8_t x_, uint8_t y_) 
    {
      if (inBounds(x_, y_))
      {
        M5.Lcd.setTextColor(TFT_RED);
        M5.Lcd.drawString(title,x+4,y+4,2);
        M5.Lcd.drawRoundRect(x, y, w, h, 5, 0xFFFF);        
      }
      if (n_buttons>0)
      {
        getChild(x_,y_)->onDraw(x_,y_);
        drawStats();         
      }
    };
    void drawStats()
    {
      // 
      // Render a sprite
      // 
      draw_sprite(63,2,slime_map);
      
      //
      // Render time information. 
      // 
      struct tm timeinfo;
      uint8_t old_hour = rtc_hour; 
      uint8_t old_min = rtc_min; 
      uint8_t old_sec = rtc_sec; 
      read_time(&timeinfo);
      if (old_hour != rtc_hour || old_min != rtc_min || old_sec != rtc_sec)
      {
        M5.Lcd.fillRect(3, 150, 75, 8, 0x0000);
        M5.Lcd.setTextColor(TFT_GREEN);
        char timeStringBuff[10]; //50 chars should be enough
        strftime(timeStringBuff, sizeof(timeStringBuff), "%H:%M:%S", &timeinfo);
        M5.Lcd.drawString(timeStringBuff, 4, 150, 1);
      }
      read_power();
      M5.Lcd.setTextColor(TFT_BLUE);
      M5.Lcd.drawString("V:", 45, 140, 1);
      M5.Lcd.drawFloat(batt_voltage,2,60,140,1);
      if(wifi_status())
      {
        M5.Lcd.setTextColor(TFT_GREEN);
        M5.Lcd.drawString("Wifi:+",4,140,1);
      }
      else
      {
        M5.Lcd.setTextColor(TFT_RED);
        M5.Lcd.drawString("Wifi:-",4,140,1);
      }
    };
    void redraw()
    {
      if (n_buttons>0)
      {
        M5.Lcd.fillScreen(BLACK);
        onDraw(x,y);
        for (int i=0; i<n_buttons;++i)
          buttons[i]->onDraw(buttons[i]->x,buttons[i]->y);
      }
      else
        onDraw(x,y);
    };
};

void maze_game()
{
  uint8_t wins=0; 
  uint8_t losses=0; 
  while(true)
  {
   cursorfX = 9.;
   cursorfY = 9.;
   M5.Lcd.fillScreen(BLACK);
   while(true)
   {
    M5.Lcd.drawRoundRect(0, 30, 50, 30, 5, 0xFFFF); 
    M5.Lcd.drawRoundRect(30, 80, 50, 20, 5, 0xFFFF); 
    read_accel();
    cursor_verlet(); 
    M5.Lcd.setTextColor(TFT_GREEN);
    M5.Lcd.drawNumber(wins, 3,100,2);
    M5.Lcd.setTextColor(TFT_RED);
    M5.Lcd.drawNumber(losses,3,120,2);
    // loss conditions
    if (cursorY>30 && cursorY<60)
    {
      if (cursorX<=50)
      {
        losses++; 
        break;
      }
    }
    if (cursorY>80 && cursorY<90)
    {
      if (cursorX>=30)
      {
        losses++; 
        break;
      }
    }
    // win condition
    if (cursorY > 91)
    {
      wins++; 
      break; 
    }
    if (M5.BtnB.read())
    {
      return; 
    }
   }
  }
};

void wifi_init()
{
  for(uint8_t i=0; i<40;i++)
  {
    if (wifi_multi.run() == WL_CONNECTED)
      return;
  }
};

bool wifi_status()
{
  return wifi_multi.run() == WL_CONNECTED;
};

String web_req(const char* addr)
{
  if(wifi_multi.run() != WL_CONNECTED)
    return String("failure"); 
  Serial.println(WiFi.localIP());
  HTTPClient http; 
  if (http.begin(addr)) 
  {
//    M5.Lcd.drawString("Success",2,30,1);
    int httpCode = http.GET();
    if (httpCode > 0) 
    {
        Serial.printf("[HTTP] GET... code: %d\n", httpCode);
        // file found at server
        if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) 
          return http.getString();
    } 
    else 
    {
      Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
    }
    http.end();
  }
  else
  {
     Serial.printf("B. Failure");
    return String("failure");
  }
  return String("failure");
};

void grab_req()
{
  M5.Lcd.fillScreen(BLACK);
  String req_addr = String(grab_dump_url)+String(grab_dump_token)+String(grab_dump_key);
  Serial.printf("req: %s\n", req_addr.c_str());
  String payload = web_req(req_addr.c_str());
  M5.Lcd.setRotation(1);
  Serial.printf("payload: %s\n", payload.c_str());
  M5.Lcd.drawString("Web Value",2,0,2);
  M5.Lcd.drawString(payload.c_str(),2,20,1);
  delay(2000);
  M5.Lcd.setRotation(0);
  M5.Lcd.fillScreen(BLACK);
  launch_main();
};

void time_req()
{
  String payload_ = web_req("https://nist.time.gov/actualtime.cgi?lzbc=siqm9b");
  if (payload_ == "failure")
    return; 
  String payload = payload_.substring(17,17+10);
  long int epoch = payload.toInt()-UTCSHIFT;
  Serial.println(payload);
//  M5.Lcd.drawString(payload,2,40,1);
//  M5.Lcd.drawNumber(year(epoch),2,50,1);
//  M5.Lcd.drawNumber(month(epoch),2,60,1);
//  M5.Lcd.drawNumber(day(epoch),2,70,1);
//  M5.Lcd.drawNumber(hour(epoch),2,80,1);
//  M5.Lcd.drawNumber(minute(epoch),2,90,1);
//  M5.Lcd.drawNumber(second(epoch),2,100,1);
  // Set the realtime clock 
  RTC_TimeTypeDef rtc_time; 
  RTC_DateTypeDef rtc_date; 
  rtc_time.Hours = hour(epoch);
  rtc_time.Minutes = minute(epoch);
  rtc_time.Seconds = second(epoch);
  rtc_date.Month = day(epoch);
  rtc_date.Date = month(epoch);
  rtc_date.Year = year(epoch);
  M5.Rtc.SetTime(&rtc_time);
  M5.Rtc.SetData(&rtc_date);
  launch_main();
  return; 
};

region main_page = region(0,0,GRIDX,GRIDY,"Main");
region dev_region = region(0,20,GRIDX,BUTTONH,"Device");
region web_region = region(0,20+BUTTONH,GRIDX,BUTTONH,"Web");
region toys_region = region(0,20+2*BUTTONH,GRIDX,BUTTONH,"Toys");

region device_page = region(0,0,GRIDX,GRIDY,"Device");
region sleep_region = region(0,20,GRIDX,BUTTONH,"Sleep");

region web_page = region(0,0,GRIDX,GRIDY,"Web");
region grab_region = region(0,20,GRIDX,BUTTONH,"Grab value");
region req_region = region(0,20+BUTTONH,GRIDX,BUTTONH,"Nist time");

region toys_page = region(0,0,GRIDX,GRIDY,"Toys");
region maze_region = region(0,20,GRIDX,BUTTONH,"Maze");

region* this_page = &main_page; 

void launch_main()
{
  main_page.launch(this_page); 
};
void launch_device()
{
  device_page.launch(this_page); 
};
void launch_web()
{
  web_page.launch(this_page);
};
void launch_toys()
{
  toys_page.launch(this_page); 
};

void setup() {
  // Device setup. 
  M5.begin();
#ifdef DEBUG 
  Serial.begin(115200);
#endif
  M5.IMU.Init();
  M5.Axp.EnableCoulombcounter(); 
  M5.Lcd.fillScreen(BLACK);

  cycle_brightness();

  ///////////////
  // Wifi setup 
  wifi_multi.addAP("Purple", "password");
  wifi_init();

  ///////////////
  // Sync time. 
  time_req();
  
  ///////////////////////////////// 
  // App setup. 
  // main page. 
  main_page.addChild(&dev_region); 
    dev_region.addClickFcn(&launch_device); 
  main_page.addChild(&web_region);
    web_region.addClickFcn(&launch_web); 
  main_page.addChild(&toys_region);
    toys_region.addClickFcn(&launch_toys); 
  main_page.launch(this_page);

  // dev page. 
  device_page.addChild(&sleep_region);
    sleep_region.addClickFcn(&start_sleep);

  // web page. 
  web_page.addChild(&grab_region);
    grab_region.addClickFcn(&grab_req);
  web_page.addChild(&req_region);
    req_region.addClickFcn(&time_req);

  // toys page. 
  toys_page.addChild(&maze_region); 
  maze_region.addClickFcn(&maze_game);
}

void loop() {
  read_accel();
  read_power(); 
  cursor_verlet(); 
  this_page->onDraw(cursorX, cursorY);
  if(M5.BtnA.wasPressed() && !M5.BtnB.read())
    this_page->onClick(cursorX, cursorY);
  if (M5.BtnB.read() && !M5.BtnA.read())
    main_page.launch(this_page);
  if(M5.BtnA.read() && M5.BtnB.read())
    cycle_brightness();
}
  
    
