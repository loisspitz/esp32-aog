// MIT License
//
// Copyright (c) 2019 Christian Riggenbach
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include <stdio.h>
#include <string.h>

#include "main.hpp"

#if defined(ESP32)
#include <WiFi.h>
#else
#include <ESP8266WiFi.h>
#endif

#include <DNSServer.h>
#include <ESPUI.h>
#include <EEPROM32_Rotate.h>

///////////////////////////////////////////////////////////////////////////
// global data
///////////////////////////////////////////////////////////////////////////
SteerConfig steerConfig;
Initialisation initialisation;

portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
SemaphoreHandle_t i2cMutex;

const byte DNS_PORT = 53;
IPAddress apIP( 192, 168, 1, 1 );

uint16_t labelLoad;
uint16_t labelHeading;
uint16_t labelRoll;
uint16_t labelWheelAngle;
uint16_t buttonReset;
uint16_t textNmeaToSend;

uint16_t labelStatusOutput;
uint16_t labelStatusAdc;
uint16_t labelStatusImu;
uint16_t labelStatusInclino;
uint16_t labelStatusGps;
uint16_t labelStatusNtrip;

///////////////////////////////////////////////////////////////////////////
// external Libraries
///////////////////////////////////////////////////////////////////////////
EEPROM32_Rotate EEPROM;

ESPUIClass ESPUI( Verbosity::Quiet );
DNSServer dnsServer;

///////////////////////////////////////////////////////////////////////////
// helper functions
///////////////////////////////////////////////////////////////////////////
void setResetButtonToRed() {
  ESPUI.getControl( buttonReset )->color = ControlColor::Alizarin;
  ESPUI.updateControl( buttonReset );
}

void writeEeprom() {
  EEPROM.writeUChar( ( uint16_t )EepromAddresses::Validator, 0 );
  EEPROM.writeUShort( ( uint16_t )EepromAddresses::SizeOfConfig, ( uint16_t )sizeof( SteerConfig ) );
  EEPROM.put( ( uint16_t )EepromAddresses::SteerSettings, steerConfig );
  EEPROM.put( ( uint16_t )EepromAddresses::Bno055CalibrationData, bno055CalibrationData );
  EEPROM.commit();
}

void addGpioOutput( uint16_t parent ) {
  ESPUI.addControl( ControlType::Option, "ESP32 GPIO 4",        String( ( uint8_t )SteerConfig::Gpio::Esp32Gpio4 ),  ControlColor::Alizarin, parent );
  ESPUI.addControl( ControlType::Option, "ESP32 GPIO 12",       String( ( uint8_t )SteerConfig::Gpio::Esp32Gpio12 ), ControlColor::Alizarin, parent );
  ESPUI.addControl( ControlType::Option, "ESP32 GPIO 13 / A12", String( ( uint8_t )SteerConfig::Gpio::Esp32Gpio13 ), ControlColor::Alizarin, parent );
  ESPUI.addControl( ControlType::Option, "ESP32 GPIO 14",       String( ( uint8_t )SteerConfig::Gpio::Esp32Gpio14 ), ControlColor::Alizarin, parent );
  ESPUI.addControl( ControlType::Option, "ESP32 GPIO 15",       String( ( uint8_t )SteerConfig::Gpio::Esp32Gpio15 ), ControlColor::Alizarin, parent );
  ESPUI.addControl( ControlType::Option, "ESP32 GPIO 21",       String( ( uint8_t )SteerConfig::Gpio::Esp32Gpio21 ), ControlColor::Alizarin, parent );
  ESPUI.addControl( ControlType::Option, "ESP32 GPIO 25",       String( ( uint8_t )SteerConfig::Gpio::Esp32Gpio25 ), ControlColor::Alizarin, parent );
  ESPUI.addControl( ControlType::Option, "ESP32 GPIO 26",       String( ( uint8_t )SteerConfig::Gpio::Esp32Gpio26 ), ControlColor::Alizarin, parent );
  ESPUI.addControl( ControlType::Option, "ESP32 GPIO 27",       String( ( uint8_t )SteerConfig::Gpio::Esp32Gpio27 ), ControlColor::Alizarin, parent );
  ESPUI.addControl( ControlType::Option, "ESP32 GPIO 32 / A7",  String( ( uint8_t )SteerConfig::Gpio::Esp32Gpio32 ), ControlColor::Alizarin, parent );
  ESPUI.addControl( ControlType::Option, "ESP32 GPIO 33 / A9",  String( ( uint8_t )SteerConfig::Gpio::Esp32Gpio33 ), ControlColor::Alizarin, parent );
}
void addGpioInput( uint16_t parent ) {
  ESPUI.addControl( ControlType::Option, "ESP32 GPIO 34 / A2 (only input)", String( ( uint8_t )SteerConfig::Gpio::Esp32Gpio34 ), ControlColor::Alizarin, parent );
  ESPUI.addControl( ControlType::Option, "ESP32 GPIO 36 / A4 (only input)", String( ( uint8_t )SteerConfig::Gpio::Esp32Gpio36 ), ControlColor::Alizarin, parent );
  ESPUI.addControl( ControlType::Option, "ESP32 GPIO 39 / A3 (only input)", String( ( uint8_t )SteerConfig::Gpio::Esp32Gpio39 ), ControlColor::Alizarin, parent );
}
void addAnalogInput( uint16_t parent ) {
  ESPUI.addControl( ControlType::Option, "ESP32 A2",  String( ( uint8_t )SteerConfig::AnalogIn::Esp32GpioA2 ),  ControlColor::Alizarin, parent );
  ESPUI.addControl( ControlType::Option, "ESP32 A3",  String( ( uint8_t )SteerConfig::AnalogIn::Esp32GpioA3 ),  ControlColor::Alizarin, parent );
  ESPUI.addControl( ControlType::Option, "ESP32 A4",  String( ( uint8_t )SteerConfig::AnalogIn::Esp32GpioA4 ),  ControlColor::Alizarin, parent );
  ESPUI.addControl( ControlType::Option, "ESP32 A7",  String( ( uint8_t )SteerConfig::AnalogIn::Esp32GpioA7 ),  ControlColor::Alizarin, parent );
  ESPUI.addControl( ControlType::Option, "ESP32 A9",  String( ( uint8_t )SteerConfig::AnalogIn::Esp32GpioA9 ),  ControlColor::Alizarin, parent );
  ESPUI.addControl( ControlType::Option, "ESP32 A12", String( ( uint8_t )SteerConfig::AnalogIn::Esp32GpioA12 ), ControlColor::Alizarin, parent );
}
void addAnalogInputADS1115( uint16_t parent ) {
  ESPUI.addControl( ControlType::Option, "ADS1115 A0 Single",       String( ( uint8_t )SteerConfig::AnalogIn::ADS1115A0Single ), ControlColor::Alizarin, parent );
  ESPUI.addControl( ControlType::Option, "ADS1115 A1 Single",       String( ( uint8_t )SteerConfig::AnalogIn::ADS1115A1Single ), ControlColor::Alizarin, parent );
  ESPUI.addControl( ControlType::Option, "ADS1115 A2 Single",       String( ( uint8_t )SteerConfig::AnalogIn::ADS1115A2Single ), ControlColor::Alizarin, parent );
  ESPUI.addControl( ControlType::Option, "ADS1115 A3 Single",       String( ( uint8_t )SteerConfig::AnalogIn::ADS1115A3Single ), ControlColor::Alizarin, parent );
  ESPUI.addControl( ControlType::Option, "ADS1115 A0 Differential", String( ( uint8_t )SteerConfig::AnalogIn::ADS1115A0A1Differential ), ControlColor::Alizarin, parent );
  ESPUI.addControl( ControlType::Option, "ADS1115 A2 Differential", String( ( uint8_t )SteerConfig::AnalogIn::ADS1115A2A3Differential ), ControlColor::Alizarin, parent );
}

///////////////////////////////////////////////////////////////////////////
// Application
///////////////////////////////////////////////////////////////////////////
void setup( void ) {
//   Serial.begin( 921600 );
  Serial.begin( 115200 );
  Serial.println( "Setup()" );

  // add all the partitions for the EEPROM emulation
  EEPROM.add_by_subtype( 0x99 );
  EEPROM.begin( 4096 );

  // restore the settings from EEPROM
  if ( ( EEPROM.readUChar( ( uint16_t )EepromAddresses::Validator ) != 0xff ) &&
       ( EEPROM.readUShort( ( uint16_t )EepromAddresses::SizeOfConfig ) == sizeof( SteerConfig ) ) ) {
    Serial.println( "Read from EEPROM" );
    EEPROM.get( ( uint16_t )EepromAddresses::SteerSettings, steerConfig );
    EEPROM.get( ( uint16_t )EepromAddresses::Bno055CalibrationData, bno055CalibrationData );
  } else {
    Serial.println( "Not read from EEPROM" );
    writeEeprom();
  }

#if defined(ESP32)
  WiFi.setHostname( steerConfig.hostname );
#else
  WiFi.hostname( steerConfig.hostname );
#endif

// try to connect to existing network
  WiFi.begin( steerConfig.ssid, steerConfig.password );
  Serial.print( "\n\nTry to connect to existing network " );
  Serial.print( steerConfig.ssid );
  Serial.print( " with password " );
  Serial.print( steerConfig.password );


  {
    uint8_t timeout = 5;

    // Wait for connection, 2.5s timeout
    do {
      delay( 500 );
      Serial.print( "." );
      timeout--;
    } while ( timeout && WiFi.status() != WL_CONNECTED );

    // not connected -> create hotspot
    if ( WiFi.status() != WL_CONNECTED ) {
      Serial.print( "\n\nCreating hotspot" );

      WiFi.mode( WIFI_AP );
      WiFi.softAPConfig( apIP, apIP, IPAddress( 255, 255, 255, 0 ) );
      WiFi.softAP( steerConfig.ssid );

      timeout = 5;

      do {
        delay( 500 );
        Serial.print( "." );
        timeout--;
      } while ( timeout );
    }
  }

  dnsServer.start( DNS_PORT, "*", apIP );

  Serial.println( "\n\nWiFi parameters:" );
  Serial.print( "Mode: " );
  Serial.println( WiFi.getMode() == WIFI_AP ? "Station" : "Client" );
  Serial.print( "IP address: " );
  Serial.println( WiFi.getMode() == WIFI_AP ? WiFi.softAPIP() : WiFi.localIP() );

// Comment in to update the Files in SPIFFS

  labelLoad       = ESPUI.addControl( ControlType::Label, "Load:", "", ControlColor::Turquoise );
  labelHeading    = ESPUI.addControl( ControlType::Label, "Heading:", "0°", ControlColor::Emerald );
  labelRoll       = ESPUI.addControl( ControlType::Label, "Roll:", "0°", ControlColor::Emerald );
  labelWheelAngle = ESPUI.addControl( ControlType::Label, "Wheel Angle:", "0°", ControlColor::Emerald );

  buttonReset = ESPUI.addControl( ControlType::Button, "If this turn red, you have to", "Apply & Reset", ControlColor::Emerald, Control::noParent,
  []( Control * control, int id ) {
    EEPROM.writeByte( ( uint16_t )EepromAddresses::Validator, 0 );
    EEPROM.put( ( uint16_t )EepromAddresses::SteerSettings, steerConfig );
//     EEPROM.dump( Serial );
    EEPROM.commit();
    ESP.restart();
  } );


// Network Tab
  {
    uint16_t tab = ESPUI.addControl( ControlType::Tab, "Network", "Network" );

    ESPUI.addControl( ControlType::Text, "SSID*", String( steerConfig.ssid ), ControlColor::Wetasphalt, tab,
    []( Control * control, int id ) {
      control->value.toCharArray( steerConfig.ssid, sizeof( steerConfig.ssid ) );
      setResetButtonToRed();
    } );
    ESPUI.addControl( ControlType::Text, "Password*", String( steerConfig.password ), ControlColor::Wetasphalt, tab,
    []( Control * control, int id ) {
      control->value.toCharArray( steerConfig.password, sizeof( steerConfig.password ) );
      setResetButtonToRed();
    } );
    ESPUI.addControl( ControlType::Text, "Hostname*", String( steerConfig.hostname ), ControlColor::Wetasphalt, tab,
    []( Control * control, int id ) {
      control->value.toCharArray( steerConfig.hostname, sizeof( steerConfig.hostname ) );
      setResetButtonToRed();
    } );
    ESPUI.addControl( ControlType::Number, "Port to send from*", String( steerConfig.portSendFrom ), ControlColor::Wetasphalt, tab,
    []( Control * control, int id ) {
      steerConfig.portSendFrom = control->value.toInt();
      setResetButtonToRed();
    } );
    ESPUI.addControl( ControlType::Number, "Port to send to*", String( steerConfig.portSendTo ), ControlColor::Wetasphalt, tab,
    []( Control * control, int id ) {
      steerConfig.portSendTo = control->value.toInt();
      setResetButtonToRed();
    } );
    ESPUI.addControl( ControlType::Number, "Port to listen to*", String( steerConfig.portListenTo ), ControlColor::Wetasphalt, tab,
    []( Control * control, int id ) {
      steerConfig.portListenTo = control->value.toInt();
      setResetButtonToRed();
    } );
  }


// Output Tab
  {
    uint16_t tab  = ESPUI.addControl( ControlType::Tab, "Steering", "Steering" );

    {
      uint16_t sel = ESPUI.addControl( ControlType::Select, "Output Type*", String( ( int )steerConfig.outputType ), ControlColor::Wetasphalt, tab,
      []( Control * control, int id ) {
        steerConfig.outputType = ( SteerConfig::OutputType )control->value.toInt();
        setResetButtonToRed();
      } );
      ESPUI.addControl( ControlType::Option, "None", "0", ControlColor::Alizarin, sel );
      ESPUI.addControl( ControlType::Option, "Motor: Cytron MD30C", "1", ControlColor::Alizarin, sel );
      ESPUI.addControl( ControlType::Option, "Motor: IBT 2", "2", ControlColor::Alizarin, sel );
      ESPUI.addControl( ControlType::Option, "Hydraulic: IBT 2 + PWM 2-Coil Valve", "3", ControlColor::Alizarin, sel );
      ESPUI.addControl( ControlType::Option, "Hydraulic: IBT 2 + Danfoss Valve PVE A/H/M", "4", ControlColor::Alizarin, sel );
    }

    {
      uint16_t sel = ESPUI.addControl( ControlType::Select, "Output Pin for PWM (or right coil)*", String( ( int )steerConfig.gpioPwm ), ControlColor::Wetasphalt, tab,
      []( Control * control, int id ) {
        steerConfig.gpioPwm = ( SteerConfig::Gpio )control->value.toInt();
        setResetButtonToRed();
      } );
      ESPUI.addControl( ControlType::Option, "None", "0", ControlColor::Alizarin, sel );
      addGpioOutput( sel );
    }
    {
      uint16_t sel = ESPUI.addControl( ControlType::Select, "Output Pin for Dir (or left coil)*", String( ( int )steerConfig.gpioDir ), ControlColor::Wetasphalt, tab,
      []( Control * control, int id ) {
        steerConfig.gpioDir = ( SteerConfig::Gpio )control->value.toInt();
        setResetButtonToRed();
      } );
      ESPUI.addControl( ControlType::Option, "None", "0", ControlColor::Alizarin, sel );
      addGpioOutput( sel );
    }
    {
      uint16_t sel = ESPUI.addControl( ControlType::Select, "Output Pin for Enable*", String( ( int )steerConfig.gpioEn ), ControlColor::Wetasphalt, tab,
      []( Control * control, int id ) {
        steerConfig.gpioEn = ( SteerConfig::Gpio )control->value.toInt();
        setResetButtonToRed();
      } );
      ESPUI.addControl( ControlType::Option, "None", "0", ControlColor::Alizarin, sel );
      addGpioOutput( sel );
    }

    ESPUI.addControl( ControlType::Switcher, "Invert Output", String( steerConfig.invertOutput ? "1" : "0" ), ControlColor::Peterriver, tab,
    []( Control * control, int id ) {
      steerConfig.invertOutput = control->value.toInt() == 1;
    } );

    {
      uint16_t sel = ESPUI.addControl( ControlType::Slider, "PWM Frequency", String( steerConfig.pwmFrequency ), ControlColor::Peterriver, tab,
      []( Control * control, int id ) {
        steerConfig.pwmFrequency = control->value.toInt();

        switch ( initialisation.outputType ) {
          case SteerConfig::OutputType::SteeringMotorIBT2: {
            ledcWriteTone( 0, steerConfig.pwmFrequency );
            ledcWriteTone( 1, steerConfig.pwmFrequency );
          }
          break;

          default:
            break;
        }
      } );
      ESPUI.addControl( ControlType::Min, "Min", String( "1000" ), ControlColor::Peterriver, sel );
      ESPUI.addControl( ControlType::Max, "Max", String( "5000" ), ControlColor::Peterriver, sel );
      ESPUI.addControl( ControlType::Step, "Step", String( "100" ), ControlColor::Peterriver, sel );
    }
  }
  
  {
    uint16_t tab  = ESPUI.addControl( ControlType::Tab, "Steering PID", "Steering PID" );

    ESPUI.addControl( ControlType::Switcher, "Allow AgOpenGPS to overwrite PID values", String( steerConfig.allowPidOverwrite ? "1" : "0" ), ControlColor::Peterriver, tab,
    []( Control * control, int id ) {
      steerConfig.allowPidOverwrite = control->value.toInt() == 1;
    } );
    
    {
      uint16_t wheel = ESPUI.addControl( ControlType::Number, "PID Kp", String( steerConfig.steeringPidKp ), ControlColor::Peterriver, tab,
      []( Control * control, int id ) {
        steerConfig.steeringPidKp = control->value.toDouble();
      } );
      ESPUI.addControl( ControlType::Min, "Min", String( "0" ), ControlColor::Peterriver, wheel );
      ESPUI.addControl( ControlType::Max, "Max", String( "50" ), ControlColor::Peterriver, wheel );
      ESPUI.addControl( ControlType::Step, "Step", String( ".01" ), ControlColor::Peterriver, wheel );
    }
    {
      uint16_t wheel = ESPUI.addControl( ControlType::Number, "PID Ki", String( steerConfig.steeringPidKi ), ControlColor::Peterriver, tab,
      []( Control * control, int id ) {
        steerConfig.steeringPidKi = control->value.toDouble();
      } );
      ESPUI.addControl( ControlType::Min, "Min", String( "0" ), ControlColor::Peterriver, wheel );
      ESPUI.addControl( ControlType::Max, "Max", String( "50" ), ControlColor::Peterriver, wheel );
      ESPUI.addControl( ControlType::Step, "Step", String( ".0001" ), ControlColor::Peterriver, wheel );
    }
    {
      uint16_t wheel = ESPUI.addControl( ControlType::Number, "PID Kd", String( steerConfig.steeringPidKd ), ControlColor::Peterriver, tab,
      []( Control * control, int id ) {
        steerConfig.steeringPidKd = control->value.toDouble();
      } );
      ESPUI.addControl( ControlType::Min, "Min", String( "0" ), ControlColor::Peterriver, wheel );
      ESPUI.addControl( ControlType::Max, "Max", String( "50" ), ControlColor::Peterriver, wheel );
      ESPUI.addControl( ControlType::Step, "Step", String( ".0001" ), ControlColor::Peterriver, wheel );
    }
    {
      uint16_t wheel = ESPUI.addControl( ControlType::Number, "Turn Output on if error is greater (BangOn)", String( steerConfig.steeringPidBangOn ), ControlColor::Peterriver, tab,
      []( Control * control, int id ) {
        steerConfig.steeringPidBangOn = control->value.toDouble();
      } );
      ESPUI.addControl( ControlType::Min, "Min", String( "0" ), ControlColor::Peterriver, wheel );
      ESPUI.addControl( ControlType::Max, "Max", String( "50" ), ControlColor::Peterriver, wheel );
      ESPUI.addControl( ControlType::Step, "Step", String( ".1" ), ControlColor::Peterriver, wheel );
    }
    {
      uint16_t wheel = ESPUI.addControl( ControlType::Number, "Turn Output off if error is smaller (BangOff)", String( steerConfig.steeringPidBangOff ), ControlColor::Peterriver, tab,
      []( Control * control, int id ) {
        steerConfig.steeringPidBangOff = control->value.toDouble();
      } );
      ESPUI.addControl( ControlType::Min, "Min", String( "0" ), ControlColor::Peterriver, wheel );
      ESPUI.addControl( ControlType::Max, "Max", String( "50" ), ControlColor::Peterriver, wheel );
      ESPUI.addControl( ControlType::Step, "Step", String( ".1" ), ControlColor::Peterriver, wheel );
    }
    
    {
      uint16_t wheel = ESPUI.addControl( ControlType::Number, "Distance from Line to turn Integral/Derivative of PID off", String( steerConfig.steeringPidDflTurnIdOff ), ControlColor::Peterriver, tab,
      []( Control * control, int id ) {
        steerConfig.steeringPidDflTurnIdOff = control->value.toInt();
      } );
      ESPUI.addControl( ControlType::Min, "Min", String( "0" ), ControlColor::Peterriver, wheel );
      ESPUI.addControl( ControlType::Max, "Max", String( "200" ), ControlColor::Peterriver, wheel );
      ESPUI.addControl( ControlType::Step, "Step", String( "1" ), ControlColor::Peterriver, wheel );
    }
    
  }

  {
    uint16_t tab  = ESPUI.addControl( ControlType::Tab, "Switches/Buttons", "Switches/Buttons" );

    {
      uint16_t sel = ESPUI.addControl( ControlType::Select, "Input Workswitch*", String( ( int )steerConfig.gpioWorkswitch ), ControlColor::Wetasphalt, tab,
      []( Control * control, int id ) {
        steerConfig.gpioWorkswitch = ( SteerConfig::Gpio )control->value.toInt();
        setResetButtonToRed();
      } );
      ESPUI.addControl( ControlType::Option, "None", "0", ControlColor::Alizarin, sel );
      addGpioInput( sel );
      addGpioOutput( sel );
    }
    {
      uint16_t sel = ESPUI.addControl( ControlType::Select, "Workswitch LED*", String( ( int )steerConfig.gpioWorkswitch ), ControlColor::Wetasphalt, tab,
      []( Control * control, int id ) {
        steerConfig.gpioWorkswitch = ( SteerConfig::Gpio )control->value.toInt();
        setResetButtonToRed();
      } );
      ESPUI.addControl( ControlType::Option, "None", "0", ControlColor::Alizarin, sel );
      addGpioOutput( sel );
    }

    ESPUI.addControl( ControlType::Switcher, "Autosteer input as Button*", String( steerConfig.autosteerButton ? "1" : "0" ), ControlColor::Wetasphalt, tab,
    []( Control * control, int id ) {
      steerConfig.autosteerButton = control->value.toInt() == 1;
    } );
    {
      uint16_t sel = ESPUI.addControl( ControlType::Select, "Input Autosteer switch/button*", String( ( int )steerConfig.gpioSteerswitch ), ControlColor::Wetasphalt, tab,
      []( Control * control, int id ) {
        steerConfig.gpioSteerswitch = ( SteerConfig::Gpio )control->value.toInt();
        setResetButtonToRed();
      } );
      ESPUI.addControl( ControlType::Option, "None", "0", ControlColor::Alizarin, sel );
      addGpioInput( sel );
      addGpioOutput( sel );
    }
    {
      uint16_t sel = ESPUI.addControl( ControlType::Select, "Autosteer LED*", String( ( int )steerConfig.gpioSteerswitch ), ControlColor::Wetasphalt, tab,
      []( Control * control, int id ) {
        steerConfig.gpioSteerswitch = ( SteerConfig::Gpio )control->value.toInt();
        setResetButtonToRed();
      } );
      ESPUI.addControl( ControlType::Option, "None", "0", ControlColor::Alizarin, sel );
      addGpioOutput( sel );
    }
  }

  {
    uint16_t tab  = ESPUI.addControl( ControlType::Tab, "Steering Wheel Encoder", "Steering Wheel Encoder" );

    ESPUI.addControl( ControlType::Switcher, "Steering Wheel Encoder*", String( steerConfig.steeringWheelEncoder ? "1" : "0" ), ControlColor::Wetasphalt, tab,
    []( Control * control, int id ) {
      steerConfig.steeringWheelEncoder = control->value.toInt() == 1;
      setResetButtonToRed();
    } );

    {
      uint16_t sel = ESPUI.addControl( ControlType::Select, "Steering Wheel Encoder Input A*", String( ( int )steerConfig.gpioWheelencoderA ), ControlColor::Wetasphalt, tab,
      []( Control * control, int id ) {
        steerConfig.gpioWheelencoderA = ( SteerConfig::Gpio )control->value.toInt();
        setResetButtonToRed();
      } );
      ESPUI.addControl( ControlType::Option, "None", "0", ControlColor::Alizarin, sel );
      addGpioInput( sel );
      addGpioOutput( sel );
    }
    {
      uint16_t sel = ESPUI.addControl( ControlType::Select, "Steering Wheel Encoder Input B*", String( ( int )steerConfig.gpioWheelencoderB ), ControlColor::Wetasphalt, tab,
      []( Control * control, int id ) {
        steerConfig.gpioWheelencoderB = ( SteerConfig::Gpio )control->value.toInt();
        setResetButtonToRed();
      } );
      ESPUI.addControl( ControlType::Option, "None", "0", ControlColor::Alizarin, sel );
      addGpioInput( sel );
      addGpioOutput( sel );
    }

    ESPUI.addControl( ControlType::Number, "Steering Wheel Encoder max Counts", String( steerConfig.pulseCountMax ), ControlColor::Peterriver, tab,
    []( Control * control, int id ) {
      steerConfig.pulseCountMax = control->value.toInt();
    } );
  }

// Sensors Tab
  {
    uint16_t tab = ESPUI.addControl( ControlType::Tab, "Wheel Angle Sensor", "Wheel Angle Sensor" );

    {
      uint16_t sel = ESPUI.addControl( ControlType::Select, "Wheel Angle Sensor*", String( ( int )steerConfig.analogInWheelAngleSensor ), ControlColor::Wetasphalt, tab,
      []( Control * control, int id ) {
        steerConfig.analogInWheelAngleSensor = ( SteerConfig::AnalogIn )control->value.toInt();
        setResetButtonToRed();
      } );
      ESPUI.addControl( ControlType::Option, "None", "0", ControlColor::Alizarin, sel );
      addAnalogInputADS1115( sel );
      addAnalogInput( sel );
    }

    ESPUI.addControl( ControlType::Switcher, "Allow AgOpenGPS to overwrite Counts per Degree and Steer Angle Center", String( steerConfig.allowWheelAngleCenterAndCountsOverwrite ? "1" : "0" ), ControlColor::Peterriver, tab,
    []( Control * control, int id ) {
      steerConfig.allowWheelAngleCenterAndCountsOverwrite = control->value.toInt() == 1;
      Serial.println( ", test" );
    } );

    {
      uint16_t wheel = ESPUI.addControl( ControlType::Number, "Wheel Angle Sensor Center", String( steerConfig.steeringPositionZero ), ControlColor::Peterriver, tab,
      []( Control * control, int id ) {
        steerConfig.steeringPositionZero = control->value.toInt();
      } );
      ESPUI.addControl( ControlType::Min, "Min", String( "0" ), ControlColor::Peterriver, wheel );
      ESPUI.addControl( ControlType::Max, "Max", String( "26000" ), ControlColor::Peterriver, wheel );
      ESPUI.addControl( ControlType::Step, "Step", String( "1" ), ControlColor::Peterriver, wheel );
    }

    {
      uint16_t wheel = ESPUI.addControl( ControlType::Number, "Wheel Angle Counts per Degree", String( steerConfig.steerSensorCountsPerDegree ), ControlColor::Peterriver, tab,
      []( Control * control, int id ) {
        steerConfig.steerSensorCountsPerDegree = control->value.toFloat();
      } );
      ESPUI.addControl( ControlType::Min, "Min", String( "0" ), ControlColor::Peterriver, wheel );
      ESPUI.addControl( ControlType::Max, "Max", String( "250" ), ControlColor::Peterriver, wheel );
      ESPUI.addControl( ControlType::Step, "Step", String( "0.1" ), ControlColor::Peterriver, wheel );
    }

    ESPUI.addControl( ControlType::Switcher, "Invert Wheel Angle Sensor", String( steerConfig.invertWheelAngleSensor ? "1" : "0" ), ControlColor::Peterriver, tab,
    []( Control * control, int id ) {
      steerConfig.invertWheelAngleSensor = control->value.toInt() == 1;
    } );
  }

  // Sensors Tab
  {
    uint16_t tab = ESPUI.addControl( ControlType::Tab, "Sensors", "Sensors" );

    {
      uint16_t sel = ESPUI.addControl( ControlType::Select, "IMU*", String( ( int )steerConfig.imuType ), ControlColor::Wetasphalt, tab,
      []( Control * control, int id ) {
        steerConfig.imuType = ( SteerConfig::ImuType )control->value.toInt();
        setResetButtonToRed();
      } );
      ESPUI.addControl( ControlType::Option, "No IMU", "0", ControlColor::Alizarin, sel );
      ESPUI.addControl( ControlType::Option, "BNO055", "1", ControlColor::Alizarin, sel );
    }

    {
      uint16_t sel = ESPUI.addControl( ControlType::Select, "Inclinometer*", String( ( int )steerConfig.inclinoType ), ControlColor::Wetasphalt, tab,
      []( Control * control, int id ) {
        steerConfig.inclinoType = ( SteerConfig::InclinoType )control->value.toInt();
        setResetButtonToRed();
      } );
      ESPUI.addControl( ControlType::Option, "No Inclinometer", "0", ControlColor::Alizarin, sel );
      ESPUI.addControl( ControlType::Option, "MMA8451", "1", ControlColor::Alizarin, sel );
//       ESPUI.addControl( ControlType::Option, "DOGS2", "2", ControlColor::Alizarin, sel );
    }
    {
      uint16_t rollOffset = ESPUI.addControl( ControlType::Slider, "Roll Offset", String( steerConfig.rollOffset ), ControlColor::Peterriver, tab,
      []( Control * control, int id ) {
        Serial.print( "rollOffset: " );
        Serial.print( control->value );

        steerConfig.rollOffset = control->value.toFloat();

        Serial.print( ", " );
        Serial.println( steerConfig.rollOffset );
      } );
      ESPUI.addControl( ControlType::Min, "Roll Min", String( "-10" ), ControlColor::Peterriver, rollOffset );
      ESPUI.addControl( ControlType::Max, "Roll Max", String( "10" ), ControlColor::Peterriver, rollOffset );
      ESPUI.addControl( ControlType::Step, "Roll Step", String( "0.01" ), ControlColor::Peterriver, rollOffset );
    }

  }

// GPS Tab
  {
    uint16_t tab     = ESPUI.addControl( ControlType::Tab, "NTRIP/GPS", "NTRIP/GPS" );

    {
      uint16_t sel = ESPUI.addControl( ControlType::Select, "RTK Correction*", String( ( int )steerConfig.rtkCorrectionType ), ControlColor::Wetasphalt, tab,
      []( Control * control, int id ) {
        steerConfig.rtkCorrectionType = ( SteerConfig::RtkCorrectionType )control->value.toInt();
        setResetButtonToRed();
      } );
      ESPUI.addControl( ControlType::Option, "No Correction", "0", ControlColor::Alizarin, sel );
      ESPUI.addControl( ControlType::Option, "NTRIP", "1", ControlColor::Alizarin, sel );
//       ESPUI.addControl( ControlType::Option, "UDP", "2", ControlColor::Alizarin, sel );
//       ESPUI.addControl( ControlType::Option, "TCP", "3", ControlColor::Alizarin, sel );
    }

    ESPUI.addControl( ControlType::Text, "URL*", String( steerConfig.rtkCorrectionURL ), ControlColor::Wetasphalt, tab,
    []( Control * control, int id ) {
      control->value.toCharArray( steerConfig.rtkCorrectionURL, sizeof( steerConfig.rtkCorrectionURL ) );
      setResetButtonToRed();
    } );

    {
      uint16_t baudrate = ESPUI.addControl( ControlType::Select, "Baudrate*", String( steerConfig.rtkCorrectionBaudrate ), ControlColor::Peterriver, tab,
      []( Control * control, int id ) {
        uint32_t baudrate = control->value.toInt();
        steerConfig.rtkCorrectionBaudrate = baudrate;
        Serial2.begin( baudrate );
      } );
      ESPUI.addControl( ControlType::Option, "9600", "9600", ControlColor::Alizarin, baudrate );
      ESPUI.addControl( ControlType::Option, "19200", "19200", ControlColor::Alizarin, baudrate );
      ESPUI.addControl( ControlType::Option, "38400", "38400", ControlColor::Alizarin, baudrate );
      ESPUI.addControl( ControlType::Option, "57600", "57600", ControlColor::Alizarin, baudrate );
      ESPUI.addControl( ControlType::Option, "115200", "115200", ControlColor::Alizarin, baudrate );
    }

    ESPUI.addControl( ControlType::Number, "Intervall to send Position", String( steerConfig.ntripPositionSendIntervall ), ControlColor::Peterriver, tab,
    []( Control * control, int id ) {
      steerConfig.ntripPositionSendIntervall = control->value.toInt();
    } );

    textNmeaToSend = ESPUI.addControl( ControlType::Text, "NMEA-String to send (leave empty to send live position)", String( steerConfig.rtkCorrectionNmeaToSend ), ControlColor::Peterriver, tab,
    []( Control * control, int id ) {
      control->value.toCharArray( steerConfig.rtkCorrectionNmeaToSend, sizeof( steerConfig.rtkCorrectionNmeaToSend ) );
    } );

    {
      uint16_t sel = ESPUI.addControl( ControlType::Select, "Send NMEA-data to", String( ( int )steerConfig.sendNmeaDataTo ), ControlColor::Peterriver, tab,
      []( Control * control, int id ) {
        steerConfig.sendNmeaDataTo = ( SteerConfig::SendNmeaDataTo )control->value.toInt();
      } );
      ESPUI.addControl( ControlType::Option, "Nowhere", "0", ControlColor::Alizarin, sel );
      ESPUI.addControl( ControlType::Option, "UDP", "1", ControlColor::Alizarin, sel );
//       ESPUI.addControl( ControlType::Option, "TCP", "2", ControlColor::Alizarin, sel );
      ESPUI.addControl( ControlType::Option, "Serial", "3", ControlColor::Alizarin, sel );
      ESPUI.addControl( ControlType::Option, "Serial1", "4", ControlColor::Alizarin, sel );
      ESPUI.addControl( ControlType::Option, "Serial2", "5", ControlColor::Alizarin, sel );
//       ESPUI.addControl( ControlType::Option, "Bluetooth", "6", ControlColor::Alizarin, sel );
    }
  }

// Status Tab
  {
    uint16_t tab  = ESPUI.addControl( ControlType::Tab, "Status", "Status" );

    labelStatusOutput = ESPUI.addControl( ControlType::Label, "Output:", "No Output configured", ControlColor::Turquoise, tab );
    labelStatusAdc = ESPUI.addControl( ControlType::Label, "ADC:", "No ADC configured", ControlColor::Turquoise, tab );
    labelStatusImu = ESPUI.addControl( ControlType::Label, "IMU:", "No IMU configured", ControlColor::Turquoise, tab );
    labelStatusInclino = ESPUI.addControl( ControlType::Label, "Inclinometer:", "No Inclinometer configured", ControlColor::Turquoise, tab );
    labelStatusGps = ESPUI.addControl( ControlType::Label, "GPS:", "Not configured", ControlColor::Turquoise, tab );
    labelStatusNtrip = ESPUI.addControl( ControlType::Label, "NTRIP:", "Not configured", ControlColor::Turquoise, tab );
  }


  i2cMutex = xSemaphoreCreateMutex();

  /*
   * .begin loads and serves all files from PROGMEM directly.
   * If you want to serve the files from SPIFFS use ESPUI.beginSPIFFS
   * (.prepareFileSystem has to be run in an empty sketch before)
   */

  /*
   * Optionally you can use HTTP BasicAuth. Keep in mind that this is NOT a
   * SECURE way of limiting access.
   * Anyone who is able to sniff traffic will be able to intercept your password
   * since it is transmitted in cleartext. Just add a username and password,
   * for example begin("ESPUI Control", "username", "password")
   */
  ESPUI.begin( "ESPUI Control" );

  initIdleStats();

  initSensors();
  initRtkCorrection();

  initAutosteer();
}

void loop( void ) {
  dnsServer.processNextRequest();
  vTaskDelay( 100 );
}
