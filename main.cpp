#include <WiFi.h> 
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>

//label the used pins to make code easier to read
#define POWER_SWITCH_PIN GPIO_NUM_13
#define STATUS_PIN GPIO_NUM_34

//WIFI login details
const String WIFI_SSID = "YOUR_SSID";
const String WIFI_PASSWORD = "YOUR_PASSWORD";

const String BOT_TOKEN = "YOUR_BOT_TOKEN";
const String CHANNEL_ID = "YOUR_CANNEL_ID";
const String ADMIN_USER_ID = "YOUR_USER_ID";

// Keeps track of the latest message, so it wil not be reacted to multiple times.
String lastMessageId = "";

//Length of time between clock syncs (1d)
const unsigned long CLOCK_SYNC_INTERVAL = 86400000;
//How often http request is sent to discord.
const unsigned long MESSAGE_CHECK_INTERVAL = 1000; 

//these are used keep track of the time when clock sync and message request will need to be performed again.
unsigned long clock_sync_last_time = 0;
unsigned long message_check_last_time = 0;

/** 
 * @enum Command
 * Used just to prevent massive if-else castle when executing commands by enabling us to use switch statement
*/
enum Command {
  ON,
  OFF,
  RESTART,
  POWER_STATUS,
  FORCE_OFF,
  INVALID // Default case
};

/**
 * Translates string command to enum value @see enum Command.
 */
Command get_command(const String& commandStr) {
  if (commandStr == "!on") return ON;
  if (commandStr == "!off") return OFF;
  if (commandStr == "!restart") return RESTART;
  if (commandStr == "!status") return POWER_STATUS;
  if (commandStr == "!force-off") return FORCE_OFF;
  return INVALID;
}

/**
 * This function powers the switch pin for 1 second activating the relay that closes te pc power switch simulating
 * 1 second 5 second power button press. this is so called normal power button that is used to turn on and of the 
 * computer under normal circumstances.
 */
void momentary_press() {
  digitalWrite(POWER_SWITCH_PIN, LOW);
  delay(1000); 
  digitalWrite(POWER_SWITCH_PIN, HIGH);
}

/**
 * This function powers the switch pin for 5 seconds activating the relay that closes te pc power switch simulating 
 * 5 second power button press. This forces the pc to close down and may cause data loss and should only be used when
 * the computer is not respo nding to the normal short power switch press.
 */
void hard_press() {
  digitalWrite(POWER_SWITCH_PIN, LOW);
  delay(5000); 
  digitalWrite(POWER_SWITCH_PIN, HIGH);
}

/**
 * Used to check if the pc is powered on. This is do ne by connecting the STATUS_PIN to some power pin in your motherboard.
 * Keep in mind that for this to work the pin should output power when the pc is powered off. I decided to use the 3.3 V 
 * pin on my motherboards TPM header since i dont use it and it has the correct voltage.
 * @return Returns true is power is on and false if it is off
 */
bool status() {
  return digitalRead(STATUS_PIN) == HIGH;
}

/**
 * @brief Synchronizes the system time with an external time source.
 * @note The function assumes that network connectivity is already established and that any necessary 
 */
void sync_time() {
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  Serial.print("\nSyncing time ");
  time_t now = time(nullptr);

  // wait for time to be greater than 24 hour after the start of the clock to indicate that the the time has been synced
  while (now < 24 * 3600 ) {
    Serial.print(".");
    delay(500);
    now = time(nullptr);
  }

  // saves the clock sync time
  clock_sync_last_time = millis();

  Serial.println("\nTime synced");
}

/**
 * This function attempts to connect the ESP 32 to the WIFI. 
 * @note This function will keep running as long as it cannot connect to the WIFI
 */
void connect_wifi() {

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);

  Serial.print("\nConnecting to Wifi SSID ");
  Serial.print(WIFI_SSID);
  Serial.print(" ");
  
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  // Waits for the WIFI to connect
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(1000);
  }

  Serial.println();

  Serial.print("WiFi connected. IP address: ");  
  Serial.println(WiFi.localIP());
}
/**
 * This function is used to get the last message sent to the discord channel used for controlling the switch.
 * This function mages http request to discords rest api to get the newest message and returns it if it has 
 * not yet been handled and is sent by the admin user.
 * @see https://discord.com/developers/docs/resources/channel#get-channel-messages
 * @return returns message as pair<String, pair<String, String>> where first is messages authors id, first of second is channel 
 * id and second of second is the message content
 */
std::pair<String, std::pair<String, String>> get_last_message() {

  std::pair<String, std::pair<String, String>> ret = std::make_pair(String(), std::make_pair(String(), String()));

  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    
    http.begin("https://discord.com/api/v10/channels/" + CHANNEL_ID + "/messages?limit=1");
    http.addHeader("Authorization", "Bot " + BOT_TOKEN);
    int httpCode = http.GET();

    if (httpCode > 0) {
      String payload = http.getString();

      DynamicJsonDocument doc(4096);
      DeserializationError error = deserializeJson(doc, payload);

      if (error) {

        Serial.print("json deserialization failed");
        Serial.println(error.c_str());

      } else {

        const String channelId = doc[0]["channel_id"]; 
        const String content = doc[0]["content"];
        const String authorId = doc[0]["author"]["id"];
        const String messageId = doc[0]["id"];

        if (messageId != lastMessageId && authorId == ADMIN_USER_ID) {
          lastMessageId = messageId;
          ret = std::make_pair(authorId, std::make_pair(channelId, content));
        }
      }

    } else {
      Serial.println("Error on HTTP request");
    }

    http.end();

  } else {
    Serial.println("WiFi not connected");
  }

  return ret;
}

/**
 * Used to add emoji reaction to discord message using discords rest api. 
 * @see https://discord.com/developers/docs/resources/channel#create-reaction
 * @param messageId Id of the message to witch we want to add the emoji reaction to.
 * @param emoji URL encoded emoji
 */
void add_reaction(const String& messageId, const String& emoji) {
  if (WiFi.status() == WL_CONNECTED) {

    HTTPClient http;
    http.begin("https://discord.com/api/v10/channels/" + CHANNEL_ID + "/messages/" + messageId + "/reactions/" + emoji + "/@me" );
    http.addHeader("Authorization", "Bot " + BOT_TOKEN);
    int httpCode = http.sendRequest("PUT", "");

    if (httpCode <= 0) {
      Serial.println("Error on sending PUT: " + http.errorToString(httpCode));
    }

    http.end();
  
  } else {
    Serial.println("WiFi not connected");
  }
}

/**
 * This Function is sed to reply to discord message. this is done by using discords rest api.
 * @see https://discord.com/developers/docs/resources/channel#get-channel-message
 * @param messageId The id of the message you want to respond.
 * @param channelID The id of channel where the reply is sent. this must be the same channel where the original message was sent.
 * @param content The text you want the response to contain.
 */
void message_reply(const String& messageId, const String& channelID, const String& content) {

  if (WiFi.status() == WL_CONNECTED) {

    HTTPClient http;
    String requestUrl = "https://discord.com/api/v10/channels/" + channelID + "/messages";
    String messageData = "{\"content\": \"" + content + "\", \"message_reference\": {\"message_id\": \"" + messageId + "\"}}";

    http.begin(requestUrl);
    http.addHeader("Authorization", "Bot " + BOT_TOKEN);
    http.addHeader("Content-Type", "application/json");

    const int httpCode = http.sendRequest("POST", messageData);

    if (httpCode <= 0) {
      Serial.println("Error on sending POST: " + http.errorToString(httpCode));
    }
    
    http.end();

  } else {
    Serial.println("WiFi not connected");
  }
}
/**
 * This function checks if the newest message on control channel contains a registered command and executes it.
 */
void handle_message() {
  const std::pair<String, std::pair<String, String>> message = get_last_message();

  if (message.first != ADMIN_USER_ID || message.second.first != CHANNEL_ID) 
    return;

  Command cmd = get_command(message.second.second);

  switch (cmd) {

    case ON:
      add_reaction(lastMessageId, "%F0%9F%91%80");
      if (!status()) {
        message_reply(lastMessageId, message.second.first, "The server is now powering on.");
        momentary_press();
      } else {
        message_reply(lastMessageId, message.second.first, "The server is already powered on.");
      }
      break;

    case OFF:
      add_reaction(lastMessageId, "%F0%9F%91%80");
      if (status()) {
        message_reply(lastMessageId, message.second.first, "The server is now powering off.");
        momentary_press();
      } else {
        message_reply(lastMessageId, message.second.first, "The server is already powered off.");
      }
      break;

    case RESTART:
      add_reaction(lastMessageId, "%F0%9F%91%80");
      if (status()) {
        unsigned int i = 0;
        message_reply(lastMessageId, message.second.first, "The server is now powering off.");
        momentary_press();
        //waits for the compute to shut off.
        while (status() && i < 30) delay(1000);
        //if power is not shut off in 30 seconds the restart process is aborted an the user is notified.
        if (i >= 30) {
          message_reply(lastMessageId, message.second.first, "The server was not powered off in time.");
          break;
        }
      }
      message_reply(lastMessageId, message.second.first, "The server is now powering on.");
      momentary_press();
      break;

    case POWER_STATUS:
      add_reaction(lastMessageId, "%F0%9F%91%80");
      message_reply(lastMessageId, message.second.first,  status() ? "The power is on." : "The power is  off.");
      break;

    case FORCE_OFF:
      message_reply(lastMessageId, message.second.first, "Forcing the server to shut down.");
      hard_press();
      break;   
  }
}

// this function is run on startup
void setup() {

  Serial.begin(9600);

  // Setup pins
  pinMode(POWER_SWITCH_PIN, OUTPUT);
  pinMode(STATUS_PIN, INPUT);

  digitalWrite(POWER_SWITCH_PIN, HIGH);
  digitalWrite(STATUS_PIN, LOW);

  connect_wifi();
  sync_time();

  get_last_message(); // get the most recent message to save its i so it will not be reacted to when the loop starts.
}

// this function is run by ESP32 continuously
void loop() {

  // syncs the clock if enough time has passed since last sync
  if (millis() - clock_sync_last_time > CLOCK_SYNC_INTERVAL) {
    sync_time();
  }

  // Handles the newest message if enough time has passed since last time
  if (millis() - message_check_last_time > MESSAGE_CHECK_INTERVAL) {
    handle_message();
    message_check_last_time = millis();
  }
}

