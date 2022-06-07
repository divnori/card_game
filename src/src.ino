#include <SPI.h>
#include <TFT_eSPI.h>
#include <WiFiClientSecure.h>
#include <WiFiClient.h>
#include <ArduinoJson.h>
#include <string.h>

TFT_eSPI tft = TFT_eSPI();  // Invoke library, pins defined in User_Setup.h
char network[] = "MIT GUEST";
char password[] = "";
uint8_t scanning = 0;
uint8_t channel = 1; //network channel on 2.4 GHz
byte bssid[] = {0x04, 0x95, 0xE6, 0xAE, 0xDB, 0x41}; //6 byte MAC address of AP you're targeting.

//Some constants and some resources:
const int RESPONSE_TIMEOUT = 6000; //ms to wait for response from host
const int GETTING_PERIOD = 2000; //periodicity of getting a number fact.
const int BUTTON_TIMEOUT = 1000; //button timeout in milliseconds
const uint16_t IN_BUFFER_SIZE = 1000; //size of buffer to hold HTTP request
const uint16_t OUT_BUFFER_SIZE = 1000; //size of buffer to hold HTTP response
char request_buffer[IN_BUFFER_SIZE]; //char array buffer to hold HTTP request
char response_buffer[OUT_BUFFER_SIZE]; //char array buffer to hold HTTP response


const int RIGHTBUTTON = 45; //pin connected to button 
const int LEFTBUTTON = 39;
const int PROCEEDBUTTON = 38;

const int BET_STATE = 0;
const int DEAL_STATE = 1;
const int THIRD_STATE = 2;
const int FINAL_STATE = 3;
const int REPLAY_STATE = 4;

int gameState = BET_STATE;

const int UNPRESSED = 0;
const int PRESSED = 1;

int leftButtonState = UNPRESSED;
int rightButtonState = UNPRESSED;
int proceedButtonState = UNPRESSED;


char player1[6];
char player2[6];
char banker1[6];
char banker2[6];
char player3[6];
char banker3[6];

char player1Suit[9];
char player2Suit[9];
char banker1Suit[9];
char banker2Suit[9];
char player3Suit[9];
char banker3Suit[9];

int player1Score;
int player2Score;
int banker1Score;
int banker2Score;
int player3Score = 0;
int banker3Score = 0;

int playerTotalScore;
int bankerTotalScore;
int bet;
char deckID[12];


void setup() {
  delay(50); //pause to make sure comms get set up

  // put your setup code here, to run once:
  tft.init();  //init screen
  tft.setRotation(1); //adjust rotation
  tft.setTextSize(1); //default font size
  tft.fillScreen(TFT_BLACK); //fill background
  tft.setTextColor(TFT_WHITE, TFT_BLACK); //set color of font to green foreground, black background
  Serial.begin(115200); //begin serial comms

    if (scanning){
      int n = WiFi.scanNetworks();
      Serial.println("scan done");
    if (n == 0) {
      Serial.println("no networks found");
    } else {
      Serial.print(n);
      Serial.println(" networks found");
      for (int i = 0; i < n; ++i) {
        Serial.printf("%d: %s, Ch:%d (%ddBm) %s ", i + 1, WiFi.SSID(i).c_str(), WiFi.channel(i), WiFi.RSSI(i), WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "open" : "");
        uint8_t* cc = WiFi.BSSID(i);
        for (int k = 0; k < 6; k++) {
          Serial.print(*cc, HEX);
          if (k != 5) Serial.print(":");
          cc++;
        }
        Serial.println("");
      }
    }
  }
  delay(100); //wait a bit (100 ms)

    //if using regular connection use line below:
  WiFi.begin(network, password);
  //if using channel/mac specification for crowded bands use the following:
  //WiFi.begin(network, password, channel, bssid);
  uint8_t count = 0; //count used for Wifi check times
  Serial.print("Attempting to connect to ");
  Serial.println(network);
  while (WiFi.status() != WL_CONNECTED && count<6) {
    delay(500);
    Serial.print(".");
    count++;
  }
  delay(2000);
  if (WiFi.isConnected()) { //if we connected then print our IP, Mac, and SSID we're on
    Serial.println("CONNECTED!");
    Serial.printf("%d:%d:%d:%d (%s) (%s)\n",WiFi.localIP()[3],WiFi.localIP()[2],
                                            WiFi.localIP()[1],WiFi.localIP()[0], 
                                          WiFi.macAddress().c_str() ,WiFi.SSID().c_str());
    delay(500);
  } else { //if we failed to connect just Try again.
    Serial.println("Failed to Connect :/  Going to restart");
    Serial.println(WiFi.status());
    ESP.restart(); // restart the ESP (proper way)
  }

  pinMode(RIGHTBUTTON, INPUT_PULLUP); //set input pin as an input!
  pinMode(LEFTBUTTON, INPUT_PULLUP);
  pinMode(PROCEEDBUTTON, INPUT_PULLUP);

}

  void dealInitialCards() {
    int i = 0; //start at 0
    do {
        request_buffer[i] = ("GET http://deckofcardsapi.com/api/deck/new/draw/?count=4 HTTP/1.1\r\n")[i]; //assign s[i] to the string literal index i
    } while(request_buffer[i++]); //continue the loop until the last char is null
    strcat(request_buffer,"Host: deckofcardsapi.com\r\n");
    strcat(request_buffer,"\r\n");
    do_http_GET("deckofcardsapi.com", request_buffer, response_buffer, OUT_BUFFER_SIZE, RESPONSE_TIMEOUT, true);

    StaticJsonDocument<800> doc;

    char* start;
    char* end;
    end = strrchr(response_buffer, '}');
    *(end+1) = '\0';
    start = strchr(response_buffer, '{');

    DeserializationError error = deserializeJson(doc, start);
    if (error) {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.f_str());
        return;
    }
    else {
      Serial.println("deserialization was a success");
    }


    strcpy(deckID, doc["deck_id"]);
    strcpy(player1, doc["cards"][0]["value"]);
    strcpy(player2, doc["cards"][1]["value"]);
    strcpy(banker1, doc["cards"][2]["value"]);
    strcpy(banker2, doc["cards"][3]["value"]);
    strcpy(player1Suit, doc["cards"][0]["suit"]);
    strcpy(player2Suit, doc["cards"][1]["suit"]);
    strcpy(banker1Suit, doc["cards"][2]["suit"]);
    strcpy(banker2Suit, doc["cards"][3]["suit"]);

    Serial.println("printing values processed from JSON");
    Serial.println(deckID);
    Serial.println(player1);
    Serial.println(player2);
    Serial.println(banker1);
    Serial.println(banker2);

    gameState = THIRD_STATE;
     
  }

  void computeScores() {
    
    if (strcmp("ACE", player1) == 0) {
      player1Score = 1;
    }
    else {
      player1Score = atoi(player1);
      //works because atoi returns 0 for any other non numeric and face cards have a value of 0
    }

    if (strcmp("ACE", player2) == 0) {
      player2Score = 1;
    }
    else {
      player2Score = atoi(player2);
      //works because atoi returns 0 for any other non numeric and face cards have a value of 0
    }

    if (strcmp("ACE", banker1) == 0) {
      banker1Score = 1;
    }
    else {
      banker1Score = atoi(banker1);
      //works because atoi returns 0 for any other non numeric and face cards have a value of 0
    }

    if (strcmp("ACE", banker2) == 0) {
      banker2Score = 1;
    }
    else {
      banker2Score = atoi(banker2);
      //works because atoi returns 0 for any other non numeric and face cards have a value of 0
    }

    if (strlen(player3)==0) {}
    else if (strcmp("ACE", player3) == 0) {
      player3Score = 1;
    }
    else {
      player3Score = atoi(player3);
      //works because atoi returns 0 for any other non numeric and face cards have a value of 0
    }

    if (strlen(banker3)==0) {}
    else if (strcmp("ACE", banker3) == 0) {
      banker3Score = 1;
    }
    else {
      banker3Score = atoi(banker3);
      //works because atoi returns 0 for any other non numeric and face cards have a value of 0
    }

    playerTotalScore = (player1Score+player2Score+player3Score)%10;
    bankerTotalScore = (banker1Score+banker2Score+banker3Score)%10;

  }

  void dealThirdCards() {
    if (playerTotalScore==8 || playerTotalScore ==9 || bankerTotalScore==8 || bankerTotalScore==9) {
      gameState = FINAL_STATE;
    } 

    if (playerTotalScore <=5) {
      int i = 0; //start at 0
      Serial.println("about to second get request");
    do {
        char request[100];
        sprintf(request, "GET http://deckofcardsapi.com/api/deck/%s/draw/?count=1 HTTP/1.1\r\n", deckID);
        request_buffer[i] = (request)[i]; //assign s[i] to the string literal index i
    } while(request_buffer[i++]); //continue the loop until the last char is null
    //need to change new to stored deck id
    strcat(request_buffer,"Host: deckofcardsapi.com\r\n");
    strcat(request_buffer,"\r\n");
    do_http_GET("deckofcardsapi.com", request_buffer, response_buffer, OUT_BUFFER_SIZE, RESPONSE_TIMEOUT, true);
    Serial.println(request_buffer);

    StaticJsonDocument<800> doc;

    char* start;
    char* end;
    end = strrchr(response_buffer, '}');
    *(end+1) = '\0';
    start = strchr(response_buffer, '{');

    DeserializationError error = deserializeJson(doc, start);
    if (error) {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.f_str());
        return;
    }
    else {
      Serial.println("deserialization was a success");
    }

    strcpy(player3, doc["cards"][0]["value"]);
    strcpy(player3Suit, doc["cards"][0]["suit"]);
    player3Score = atoi(player3);

    if (bankerTotalScore<=2 || (bankerTotalScore==3 && player3Score!=8) || (bankerTotalScore==4&& player3Score>=2 && player3Score<=7) || (bankerTotalScore==5 && player3Score>=4 && player3Score<=7) || (bankerTotalScore==6 && player3Score==6) || (bankerTotalScore==6 && player3Score==7)) {
          int i = 0; //start at 0
          Serial.println("about to second get request");
        do {
            char request[100];
            sprintf(request, "GET http://deckofcardsapi.com/api/deck/%s/draw/?count=1 HTTP/1.1\r\n", deckID);
            request_buffer[i] = (request)[i]; //assign s[i] to the string literal index i
        } while(request_buffer[i++]); //continue the loop until the last char is null
        //need to change new to stored deck id
        strcat(request_buffer,"Host: deckofcardsapi.com\r\n");
        strcat(request_buffer,"\r\n");
        do_http_GET("deckofcardsapi.com", request_buffer, response_buffer, OUT_BUFFER_SIZE, RESPONSE_TIMEOUT, true);
        Serial.println(request_buffer);

        StaticJsonDocument<800> doc;

        char* start;
        char* end;
        end = strrchr(response_buffer, '}');
        *(end+1) = '\0';
        start = strchr(response_buffer, '{');

        DeserializationError error = deserializeJson(doc, start);
        if (error) {
            Serial.print(F("deserializeJson() failed: "));
            Serial.println(error.f_str());
            return;
        }
        else {
          Serial.println("deserialization was a success");
        }

        strcpy(banker3, doc["cards"][0]["value"]);
        strcpy(banker3Suit, doc["cards"][0]["suit"]);

        tft.fillScreen(TFT_BLACK);
      tft.setCursor(0,0,2);
      tft.printf("Player: \n%s of %s \n%s of %s \n%s of %s \nBanker: \n%s of %s \n%s of %s \n%s of %s", player1, player1Suit, player2, player2Suit, player3, player3Suit, banker1, banker1Suit, banker2, banker2Suit, banker3, banker3Suit);
    }

      else{

      tft.fillScreen(TFT_BLACK);
      tft.setCursor(0,0,2);
      tft.printf("Player: \n%s of %s \n%s of %s \n%s of %s \nBanker: \n%s of %s \n%s of %s", player1, player1Suit, player2, player2Suit, player3, player3Suit, banker1, banker1Suit, banker2, banker2Suit);

      }

    }

    else if (bankerTotalScore<=5) { //if player didn't draw, only banker draws if hand is 0-5
      int i = 0; //start at 0
      Serial.println("about to second get request");
    do {
        char request[100];
        sprintf(request, "GET http://deckofcardsapi.com/api/deck/%s/draw/?count=1 HTTP/1.1\r\n", deckID);
        request_buffer[i] = (request)[i]; //assign s[i] to the string literal index i
    } while(request_buffer[i++]); //continue the loop until the last char is null
    //need to change new to stored deck id
    strcat(request_buffer,"Host: deckofcardsapi.com\r\n");
    strcat(request_buffer,"\r\n");
    do_http_GET("deckofcardsapi.com", request_buffer, response_buffer, OUT_BUFFER_SIZE, RESPONSE_TIMEOUT, true);
    Serial.println(request_buffer);

    StaticJsonDocument<800> doc;

    char* start;
    char* end;
    end = strrchr(response_buffer, '}');
    *(end+1) = '\0';
    start = strchr(response_buffer, '{');

    DeserializationError error = deserializeJson(doc, start);
    if (error) {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.f_str());
        return;
    }
    else {
      Serial.println("deserialization was a success");
    }

    strcpy(banker3, doc["cards"][0]["value"]);
    strcpy(banker3Suit, doc["cards"][0]["suit"]);

    tft.fillScreen(TFT_BLACK);
    tft.setCursor(0,0,2);
    tft.printf("Player: \n%s of %s \n%s of %s \nBanker: \n%s of %s \n%s of %s \n%s of %s", player1, player1Suit, player2, player2Suit, banker1, banker1Suit, banker2, banker2Suit, banker3, banker3Suit);


    }

    gameState = FINAL_STATE;
    
  }

  void calculateWinner() {
    if (playerTotalScore==bankerTotalScore) {
      tft.fillScreen(TFT_BLACK);
      tft.setCursor(0,0,2);
      tft.println("We have a tie!");
      tft.println("Press the bottom button to replay");
      tft.println("Press the center button to turn off");
    }
    else if (playerTotalScore>bankerTotalScore) {
      if (bet==0) {
        tft.fillScreen(TFT_BLACK);
        tft.setCursor(0,0,2);
        tft.println("You won!");
        tft.println("Press the bottom button to replay");
        tft.println("Press the center button to turn off");
        gameState = REPLAY_STATE;
      }
      else {
        tft.fillScreen(TFT_BLACK);
        tft.setCursor(0,0,2);
        tft.println("You lost!");
        tft.println("Press the bottom button to replay");
        tft.println("Press the center button to turn off");
        gameState = REPLAY_STATE;
      }
    }
    else if (bankerTotalScore>playerTotalScore) {
      if (bet==1) {
        tft.fillScreen(TFT_BLACK);
        tft.setCursor(0,0,2);
        tft.println("You won!");
        tft.println("Press the bottom button to replay");
        tft.println("Press the center button to turn off");
        gameState = REPLAY_STATE;
      }
      else {
        tft.fillScreen(TFT_BLACK);
        tft.setCursor(0,0,2);
        tft.println("You lost!");
        tft.println("Press the bottom button to replay");
        tft.println("Press the center button to turn off");
        gameState = REPLAY_STATE;
      }
    }
  }


void loop() {
  uint8_t leftButton = digitalRead(LEFTBUTTON);
  uint8_t rightButton = digitalRead(RIGHTBUTTON);
  uint8_t proceedButton = digitalRead(PROCEEDBUTTON);



  if (gameState==BET_STATE) {
      tft.setCursor(0,0,2);
      tft.println("Welcome to Baccarat! \nWould you like to bet on the player or banker?");
      tft.println("Press the center button for player or the bottom button for banker.");

      if (leftButtonState==UNPRESSED && leftButton==0) {
        leftButtonState = PRESSED;
        Serial.println("LEFT BUTTON HAS BEEN PRESSED"); 
      }      
      else if (leftButtonState==PRESSED && leftButton==1) {
        leftButtonState = UNPRESSED;
        bet = 0;
        tft.fillScreen(TFT_BLACK); //fill background
        tft.setCursor(0,0,2);
        tft.println("You have bet on the \nplayer. We are ready to deal cards. Proceed \nthrough the game by \nhitting the top button.");
        gameState = DEAL_STATE;
      }

      if (rightButtonState==UNPRESSED && rightButton==0) {
        rightButtonState = PRESSED;
        Serial.println("RIGHT BUTTON HAS BEEN PRESSED"); 
      }
      else if (rightButtonState==PRESSED && rightButton==1) {
        rightButtonState = UNPRESSED;
        bet = 1;
        tft.fillScreen(TFT_BLACK);
        tft.setCursor(0,0,2);
        tft.println("You have bet on the \nbanker. We are ready to deal cards. Proceed \nthrough the game by \nhitting the top button.");   
        gameState = DEAL_STATE; 
      }
  }

  else if (gameState==DEAL_STATE) {
    if (proceedButtonState==UNPRESSED && proceedButton ==0) {
      proceedButtonState = PRESSED;     
      Serial.println("PROCEED BUTTON HAS BEEN PRESSED"); 
    }
    else if (proceedButtonState==PRESSED && proceedButton == 1) {
      proceedButtonState = UNPRESSED;
      dealInitialCards();
      tft.fillScreen(TFT_BLACK);
      tft.setCursor(0,0,2);
      tft.printf("Player: \n%s of %s \n%s of %s \nBanker: \n%s of %s \n%s of %s", player1, player1Suit, player2, player2Suit, banker1, banker1Suit, banker2, banker2Suit);
      computeScores();
      Serial.println("player total score:");
      Serial.println(playerTotalScore);
      Serial.println("banker total score:");
      Serial.println(bankerTotalScore);
    }


  }

  else if (gameState==THIRD_STATE) {
    if (proceedButtonState==UNPRESSED && proceedButton ==0) {
      proceedButtonState = PRESSED;     
      Serial.println("PROCEED BUTTON HAS BEEN PRESSED"); 
    }
    else if (proceedButtonState==PRESSED && proceedButton == 1) {
      proceedButtonState = UNPRESSED;
      dealThirdCards();
      computeScores();
    }
  }

  else if (gameState==FINAL_STATE) {
    if (proceedButtonState==UNPRESSED && proceedButton ==0) {
      proceedButtonState = PRESSED;     
      Serial.println("PROCEED BUTTON HAS BEEN PRESSED"); 
    }
    else if (proceedButtonState==PRESSED && proceedButton == 1) {
      proceedButtonState = UNPRESSED;
      calculateWinner();
    }
  }

  else if (gameState==REPLAY_STATE) {
      if (leftButtonState==UNPRESSED && leftButton==0) {
        leftButtonState = PRESSED;
        Serial.println("LEFT BUTTON HAS BEEN PRESSED"); 
      }      
      else if (leftButtonState==PRESSED && leftButton==1) {
        tft.fillScreen(TFT_BLACK); //fill background
      }

      if (rightButtonState==UNPRESSED && rightButton==0) {
        rightButtonState = PRESSED;
        Serial.println("RIGHT BUTTON HAS BEEN PRESSED"); 
      }
      else if (rightButtonState==PRESSED && rightButton==1) {
        rightButtonState = UNPRESSED;
        tft.fillScreen(TFT_BLACK); //fill background
        gameState = BET_STATE;
        player1Score = 0;
        player2Score = 0;
        banker1Score = 0;
        banker2Score = 0;
        player3Score = 0;
        banker3Score = 0;
        playerTotalScore = 0;
        bankerTotalScore = 0;
        bet = 2;

        memset(player1, 0, 6);
        memset(player2, 0, 6);
        memset(player3, 0, 6);
        memset(banker1, 0, 6);
        memset(banker2, 0, 6);
        memset(banker3, 0, 6);
        memset(player1Suit, 0, 9);
        memset(player2Suit, 0, 9);
        memset(player3Suit, 0, 9);
        memset(banker1Suit, 0, 9);
        memset(banker2Suit, 0, 9);
        memset(banker3Suit, 0, 9);

      }

  }

}

void do_http_GET(char* host, char* request, char* response, uint16_t response_size, uint16_t response_timeout, uint8_t serial){
  WiFiClient client; //instantiate a client object
  Serial.println(host);
  Serial.println(request_buffer);
  if (client.connect(host, 80)) { //try to connect to host on port 80
    if (serial) Serial.print(request);//Can do one-line if statements in C without curly braces
    client.print(request);
    memset(response, 0, response_size); //Null out (0 is the value of the null terminator '\0') entire buffer
    uint32_t count = millis();
    while (client.connected()) { //while we remain connected read out data coming back
      client.readBytesUntil('\n',response,response_size);
      if (serial) Serial.println(response);
      if (strcmp(response,"\r")==0) { //found a blank line!
        break;
      }
      memset(response, 0, response_size);
      if (millis()-count>response_timeout) break;
    }
    memset(response, 0, response_size);  
    count = millis();
    while (client.available()) { //read out remaining text (body of response)
      char_append(response,client.read(),OUT_BUFFER_SIZE);
    }
    if (serial) Serial.println(response);
    client.stop();
    if (serial) Serial.println("-----------");  
  }else{
    if (serial) Serial.println("connection failed :/");
    if (serial) Serial.println("wait 0.5 sec...");
    client.stop();
  }
}  

/*----------------------------------
 * char_append Function:
 * Arguments:
 *    char* buff: pointer to character array which we will append a
 *    char c: 
 *    uint16_t buff_size: size of buffer buff
 *    
 * Return value: 
 *    boolean: True if character appended, False if not appended (indicating buffer full)
 */
uint8_t char_append(char* buff, char c, uint16_t buff_size) {
        int len = strlen(buff);
        if (len>buff_size) return false;
        buff[len] = c;
        buff[len+1] = '\0';
        return true;
}
