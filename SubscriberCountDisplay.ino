// We always have to include the library
#include <ArduinoJson.h> // Used by YoutubeApi, get it here: https://github.com/bblanchon/ArduinoJson
#include <ESP8266WiFi.h> // Also install the board information: https://github.com/esp8266/Arduino
#include <WiFiClientSecure.h>
#include <YoutubeApi.h> // You will need to get this library: https://github.com/witnessmenow/arduino-youtube-api

#include "LedControl.h" // My modifications are included in the folder "LedControl"
#include "characters.h" // My own definition of ASCII characters and special stuff

const int c_DISP_COUNT = 4; // Number of diplays you have (Note: should be at least 4 and max 8)
LedControl lc = LedControl(16, c_DISP_COUNT); // create object of LedControl library

const char ssids[][20] = {
    "<SSID 1>", // your 1st network SSID (name)
    //    "<SSID 2>"                  // your 2nd network SSID (name) or more
};
const char passwords[][28] = {
    "<password 1>", // your 1st network key
    //    "<password 2>"              // your 2nd network key or more
};
const uint8_t num_of_wifis = sizeof(ssids);
const uint8_t connection_timeout = 10; // connection timeout for each network in seconds
#define API_KEY "<API Key>" // your google apps API Token
#define CHANNEL_ID "<Channel ID>" // makes up the url of channel

WiFiClientSecure client; // create a secure wifi client
YoutubeApi api(API_KEY, client); // create objecy of YoutubeApi library

unsigned long c_API_MTBS = 60000; // mean time between api requests (1 minute)
unsigned long api_lasttime; // last time api request has been done
unsigned long subscriber_count = 0; // The number of subscribers

// direction of characters displayed 0=up, 1=right, 2=down, 3=left
const uint8_t c_DIRECTION = 0;

// Lookup table and function for reversing the bitorder of a byte
const uint8_t lookup[16] = {
    0x0,
    0x8,
    0x4,
    0xc,
    0x2,
    0xa,
    0x6,
    0xe,
    0x1,
    0x9,
    0x5,
    0xd,
    0x3,
    0xb,
    0x7,
    0xf,
};

const uint8_t reverse(uint8_t n)
{
    // Reverse the top and bottom nibble then swap them.
    return (lookup[n & 0b1111] << 4) | lookup[n >> 4];
}

// Small cheat sheet to help wiht animations
// Matrix corners (row, column)
// (7,7)  (7,0)
// (0,7)  (0,0)

// Debug function that shows the current state of the matrix in the console
//! \todo Make it dynamic with the count of matrices someone has
void visualizeMatrix()
{
    uint8_t dev1[8] = {0};
    uint8_t dev2[8] = {0};
    uint8_t dev3[8] = {0};
    lc.getDeviceState(0, dev1);
    lc.getDeviceState(1, dev2);
    lc.getDeviceState(2, dev3);
    uint8_t i = 0;
    while (i < 8)
    {
        dev1[i] = reverse(dev1[i]);
        dev2[i] = reverse(dev2[i]);
        dev3[i] = reverse(dev3[i]);
        ++i;
    }

    Serial.println("____________________________________________________");
    for (int8_t i = 7; i > -1; --i)
    {
        Serial.print("|");
        for (int8_t j = 7; j > -1; --j)
        {
            Serial.print((dev1[i] >> j & 0x01) ? "* " : "  ");
        }
        Serial.print("|");
        for (int8_t j = 7; j > -1; --j)
        {
            Serial.print((dev2[i] >> j & 0x01) ? "* " : "  ");
        }
        Serial.print("|");
        for (int8_t j = 7; j > -1; --j)
        {
            Serial.print((dev3[i] >> j & 0x01) ? "* " : "  ");
        }
        Serial.println("|");
    }
    Serial.println("____________________________________________________");
}

//! \brief Function that displays a given value on a given device_count
//!
//! \param lc A reference to an object of LedControl aka the display
//! \param addr The device to show the value on
//! \param value An array containing the data to display
//! \param dp A boolean that specifies whether a decimal point is added to the data
//! \param dir The direction to orient the characters in
void setChar(LedControl& lc, int addr, const uint8_t value[8], bool dp = false, const uint8_t dir = c_DIRECTION)
{
    int row = 0;
    int offset = 0;
    switch (dir)
    {
    default:
    case 0: // needs to be translated
        // row = 0;
        offset = 7;
        while (row < 8)
        {
            lc.setRow(addr, row, reverse(value[offset]));
            --offset;
            ++row;
        }
        if (dp)
        {
            lc.setLed(addr, 0, 0, true);
        }
        break;
    case 1:
        // row = 0;
        offset = 7;
        while (row < 8)
        {
            lc.setColumn(addr, row, value[offset]);
            --offset;
            ++row;
        }
        if (dp)
        {
            lc.setLed(addr, 7, 0, true);
        }
        break;
    case 2:
        // row = 0;
        while (row < 8)
        {
            lc.setRow(addr, row, value[row]);
            ++row;
        }
        if (dp)
        {
            lc.setLed(addr, 7, 7, true);
        }
        break;
    case 3: // needs to be translated
        // row = 0;
        // offset = 7;
        while (row < 8)
        {
            lc.setColumn(addr, row, reverse(value[row]));
            --offset;
            ++row;
        }
        if (dp)
        {
            lc.setLed(addr, 0, 7, true);
        }
        break;
    }
    // visualizeMatrix(); // uncomment to see the state of the whole matrix
}

//! \brief Function that displays the given string on the display
//!
//! \param lc A reference to an object of LedControl aka the display
//! \param text A string to display
//! \param speed The delay between each transition aka how long a character is shown
//! \param dir The direction to orient the characters in
void showText(LedControl& lc, const String text, uint16_t speed = 500, const uint8_t dir = c_DIRECTION)
{
    uint8_t size = text.length();
    uint8_t i = 0;
    while (i < size)
    {
        for (uint8_t j = 0; j < c_DISP_COUNT; ++j)
        {
            uint8_t char_to_show = i + j;
            if (char_to_show > size)
            {
                lc.clearDisplay(j);
            }
            else
            {
                setChar(lc, j, ascii_chars[asciiToTable(text[char_to_show])], false, dir);
            }
        }
        ++i;
        delay(speed);
    }
    lc.clearDisplay(0);
}

//! \brief Function that animates the given string on the display
//!
//! \param lc A reference to an object of LedControl aka the display
//! \param text A string to display
//! \param speed The delay between each transition. There are 8 transitions per character
//! \param dir The direction to orient the characters in
void scrollText(LedControl& lc, const String text, uint16_t speed = 60, const uint8_t dir = c_DIRECTION)
{
    uint16_t size = text.length() * 8;
    uint16_t i = 0;
    while (i < size)
    {
        uint8_t to_shift_c1 = 8 - i % 8;
        uint8_t to_shift_c2 = i % 8;

        for (uint8_t j = 0; j < c_DISP_COUNT; ++j)
        {
            uint8_t char_to_show = i / 8 + j; // 8 animations per char + index
            if (char_to_show > size)
            {
                lc.clearDisplay(j);
            }
            else
            {
                uint8_t c1[8] = {0}; // char n-1     aka last char
                uint8_t c2[8] = {0}; // char n       aka current char
                memcpy(c1, ascii_chars[char_to_show - 1 >= 0 ? asciiToTable(text[char_to_show - 1]) : 0],
                    8 * sizeof(uint8_t));
                memcpy(c2, ascii_chars[asciiToTable(text[char_to_show])], 8 * sizeof(uint8_t));
                // shows last and current
                for (uint8_t k = 0; k < 8; ++k)
                {
                    c1[k] = (c2[k] >> to_shift_c1) | (c1[k] << to_shift_c2);
                }
                setChar(lc, j, c1, false, dir);
            }
        }
        ++i;
        delay(speed);
    }
    lc.clearDisplay(0);
}

//! \brief Function that animates the given value on a given device
//!
//! \param lc A reference to an object of LedControl aka the display
//! \param addr The device to show the value on
//! \param value An array containing the data to display
//! \param smooth A bool defining whether the current state of the display is integrated into the animation (true) or
//! not(false) \param speed The delay between each transition. There are 8 \param dir The direction to orient the
//! characters in
void appearDown(LedControl& lc, int addr, const uint8_t value[8], bool smooth = true, uint16_t speed = 60,
    const uint8_t dir = c_DIRECTION)
{
    int i = 0;
    uint8_t temp[8] = {0};
    if (smooth)
    {
        lc.getDeviceState(addr, temp);
        uint8_t i = 0;
        while (i < 8)
        {
            temp[i] = reverse(temp[i]);
            ++i;
        }
    }
    // lc.clearDisplay(addr);
    delay(speed);
    while (i < 8)
    {
        temp[i] = value[i];

        setChar(lc, addr, temp, false, dir);
        ++i;
        delay(speed);
    }
}

//! \brief Function that animates the given value on a given device
//!
//! \param lc A reference to an object of LedControl aka the display
//! \param addr The device to show the value on
//! \param value An array containing the data to display
//! \param smooth A bool defining whether the current state of the display is integrated into the animation (true) or
//! not(false) \param speed The delay between each transition. There are 8 \param dir The direction to orient the
//! characters in
void scrollDown(LedControl& lc, int addr, const uint8_t value[8], bool smooth = true, uint16_t speed = 60,
    const uint8_t dir = c_DIRECTION)
{
    int i = 0;
    uint8_t temp[8] = {0};
    if (smooth)
    {
        lc.getDeviceState(addr, temp);
        uint8_t i = 0;
        while (i < 8)
        {
            temp[i] = reverse(temp[i]);
            ++i;
        }
    }
    delay(speed);
    while (i < 8)
    {
        uint8_t k = 0;
        while (k <= i)
        {
            temp[k] = value[7 - i + k];
            ++k;
        }

        setChar(lc, addr, temp, false, dir);
        ++i;
        delay(speed);
    }
}

//! \brief Helper function that returns the number of digits of a given number
//!
//! \number The number to get the number of digits of
//! \return The number of digits
uint8_t numDigits(long number)
{
    uint8_t digits = 0;
    if (number < 0)
    {
        digits = 1; // remove this line if '-' does not count as a digit
    }
    while (number)
    {
        number /= 10;
        digits++;
    }
    return digits;
}

//! \brief Shows a given number on the matrix display
//!
//! If the number is to big to show it will scroll through it
//! otherwise the number will be shown and stays on the display, so
//! make sure to clear it.
//! \param lc An object of LedControl aka the display
//! \param number A long as the number to display
//! \param dir The direction to orient the characters in
void showNumber(LedControl lc, long number, const uint8_t dir = c_DIRECTION)
{
    int device_count = lc.getDeviceCount(); // get the count of matrices
    uint8_t digits = numDigits(number); // get the number of digits of the number to display
    char num_as_char[50]; // buffer for converted number
    ltoa(number, num_as_char, 10); // convert number to string
    if (device_count > 4 || (device_count > 3 && number > 0))
    {
        int8_t pos = device_count - digits;
        if (pos >= 0) // number fits on display
        {
            uint8_t i = 0; // counter
            while (i < pos) // clear all unused displays
            {
                lc.clearDisplay(i);
                ++i;
            }
            i = 0;
            while (pos < device_count)
            {
                setChar(lc, pos, ascii_chars[asciiToTable(num_as_char[i])], false, dir);
                ++i;
                ++pos;
            }
        }
        else // number does not fit on display
        {
            uint8_t disp_area = device_count - 1; // helper for boundaries of matrices aka address of last matrix
            uint8_t i = 0; // counter
            while (i < disp_area)
            {
                if ((digits - i - 1) % 3 == 0 && i != disp_area - 1) // if the number needs to have a point/comma
                {
                    setChar(lc, i, ascii_chars[asciiToTable(num_as_char[i])], true, dir);
                }
                else
                {
                    setChar(lc, i, ascii_chars[asciiToTable(num_as_char[i])], false, dir);
                }
                ++i;
            }
            if (number < 0) // If the number is negative we need to substract one of the digits, because the first one
                            // will be the minus sign
            {
                digits -= 1;
            }
            switch (digits)
            {
            case 4: //           1.000 k min. 2 // fits on 4
            case 5: //          10.000 k min. 3 // 10.0k               // fits on 5-8
            case 6: //         100.000 k min. 4 // 100 k - 100.0k      // fits on 6-8
                setChar(lc, disp_area, ascii_chars[75], false, dir);
                break;
            case 7: //       1.000.000 M min. 2 // 1.00M - 1.000.0M    // fits on 7 and 8
            case 8: //      10.000.000 M min. 3 // 10.0M - 10.000.0M   // fits on 8
            case 9: //     100.000.000 M min. 4 // 100 M - 100.000.0M
                setChar(lc, disp_area, ascii_chars[45], false, dir);
                break;
            case 10: //   1.000.000.000 G min. 2 // 1.00G - 1.000.000G
            case 11: //  10.000.000.000 G min. 3 // 10.0G - 10.000.00G
            case 12: // 100.000.000.000 G min. 4 // 100 G - 100.000.0G
                setChar(lc, disp_area, ascii_chars[39], false, dir);
                break;
            default:
                break;
            }
        }
    }
    else
    {
        scrollText(lc, num_as_char);
    }
}

// Setup
void setup()
{
    Serial.begin(115200);
    Serial.println();
    Serial.println(WiFi.macAddress()); // Print mac address of the esp

    client.setInsecure(); // Need this otherwise youtube api requests wont work anymore

    uint8_t i = 0;
    while (i < lc.getDeviceCount()) // Setup all matrices
    {
        lc.shutdown(i, false); // disable shutdown
        lc.setIntensity(i, 0); // set brightness to low
        lc.clearDisplay(i); // clear display
        ++i;
    }
    // scrollText(lc,"AaBbCcDdEeFfGgHhIiJjKkLlMmNnOoPpQqRrSsTtUuVvWwXxYyZz 1234567890"); // Uncomment to test the
    // display

    scrollText(lc, "Setup"); // Tell that we are in setup

    // Set WiFi to station mode and disconnect from an AP if it was Previously
    // connected
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);

    uint8_t selection = 0;
    while (true)
    {
        bool timeout = false;
        WiFi.begin(ssids[selection], passwords[selection]);
        uint32_t start_time = millis();
        i = 27;
        while (!timeout)
        {
            setChar(lc, 1, special_chars[i], false, 0);
            setChar(lc, 2, ascii_chars[16 + selection], false, 0);
            ++i;
            if (i > 30)
            {
                i = 27;
            }
            delay(500);
            timeout
                = ((WiFi.status() == WL_CONNECTED) || (millis() >= start_time + (uint32_t)connection_timeout * 1000));
        }
        if (WiFi.status() == WL_CONNECTED)
        {
            String connectionInfo = String("Connected to " + String(ssids[selection]));
            scrollText(lc, connectionInfo);
            Serial.println(connectionInfo);
            break;
        }
        else
        {
            scrollText(lc, "Timeout!");
            Serial.println("Timeout!");
            WiFi.disconnect();
        }
        if (selection < num_of_wifis)
        {
            ++selection;
        }
        else
        {
            selection = 0;
        }
    }
    //    api._debug = true; // use for debugging purposes
    updateChannelStatistics();
}

void updateChannelStatistics()
{
    Serial.println("Update statistics");
    if (api.getChannelStatistics(CHANNEL_ID))
    {
        Serial.println("Received statistics");
        if (subscriber_count != api.channelStats.subscriberCount)
        {
            if (subscriber_count < api.channelStats.subscriberCount)
            {
                scrollDown(lc, 0, special_chars[4]);
                // scrollDown(lc, 1, special_chars[4]);
                // scrollDown(lc, 2, special_chars[4]);
                scrollText(
                    lc, String("    +" + String(api.channelStats.subscriberCount - subscriber_count) + " Subscriber!"));
            }
            else
            {
                scrollDown(lc, 0, special_chars[4]);
                // scrollDown(lc, 1, special_chars[4]);
                // scrollDown(lc, 2, special_chars[4]);
                scrollText(
                    lc, String("    -" + String(subscriber_count - api.channelStats.subscriberCount) + " Subscriber!"));
            }
            subscriber_count = api.channelStats.subscriberCount;
            showNumber(lc, subscriber_count);
        }
        //    Serial.println("---------Stats---------");
        //    Serial.print("Subscriber Count: ");
        //    Serial.println(api.channelStats.subscriberCount);
        //    Serial.print("View Count: ");
        //    Serial.println(api.channelStats.viewCount);
        //    Serial.print("Comment Count: ");
        //    Serial.println(api.channelStats.commentCount);
        //    Serial.print("Video Count: ");
        //    Serial.println(api.channelStats.videoCount);
        ////     Probably not needed :)s
        //    Serial.print("hiddenSubscriberCount: ");
        //    Serial.println(api.channelStats.hiddenSubscriberCount);
        //    Serial.println("------------------------");
    }
}

// Main Loop
void loop()
{
    if (millis() - api_lasttime > c_API_MTBS) // Only get channel statistics every c_API_MTBS seconds
    {
        updateChannelStatistics();
        api_lasttime = millis(); // store last time we tried to get the statistics data
    }
}
