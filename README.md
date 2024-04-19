# ESP_IDF MQTT Radar Track.
## Environment
Visual Studio code with the esp-idf plugin. Nothing else. ALl modules are from the stock IDF.

Open the folder. Run the menuconfig. Select custom partition. Save. 
You may wish to select a board and port.
Press build, upload and monitor.

## Menu Config
Run the idf menuconfig, go to Button Configuration, at the bottom.
Set the GPIO button for your board.


## Wifi Set Up

To reset the wifi, hold down the button (defined in Button.h). On the xiao espc3 this is GPIO_9
on the lilygo display this is pin 35.

When the 'Button' class is initialised in main.cpp you can pass another GPIO.

Once the wifi is initialised you can use the ESP Touch V2 to config the wifi.
https://www.espressif.com/en/technology/esp-touch

Make sure AES and additional settings is not filled out

## Settings Set Up via REST
The rest of the config is in SettingsManager.h

	std::string mqttBrokerUri = "mqtt://mqtt2.mianos.com";
    std::string mqttUserName = "";
    std::string mqttUserPassword = "";
    std::string sensorName = "radar3";
    int tracking = 0;
    int presence = 1000;
    int detectionTimeout = 10000;
    std::string tz = "AEST-10AEDT,M10.1.0,M4.1.0/3";
    std::string ntpServer = "time.google.com";

Once the wifi is connected with esptouch, any of these can be reset with the built in web server.

	curl http://<IP_ADDRESS>/settings | jq
	  % Total    % Received % Xferd  Average Speed   Time    Time     Time  Current
	                                 Dload  Upload   Total   Spent    Left  Speed
	100   225  100   225    0     0   1792      0 --:--:-- --:--:-- --:--:--  1800
	{
	  "mqttBrokerUri": "mqtt://mqtt2.mianos.com",
	  "mqttUserName": "",
	  "mqttUserPassword": "",
	  "sensorName": "radar3",
	  "tracking": 0,
	  "presence": 3000,
	  "detectionTimeout": 10000,
	  "tz": "AEST-10AEDT,M10.1.0,M4.1.0/3",
	  "ntpServer": "time.google.com"
	}
	
and any value can be set with:

	curl -X POST http://<IP_ADDRESS>/settings -H "Content-Type: application/json" -d '{"presence": 3000}'
	

The settings are stored in nv ram so this only needs to be done once.

## Data published

When the module starts it publishes the following:

	mosquitto_sub -h localhost -v -t 'tele/radar3/+'
	
	tele/radar3/init {"version":4,"name":"radar3","time":"2024-04-18T17:16:09","gmt":"2024-04-18T07:16:09","hostname":"espressif","ip":"<IP_ADDRESS>","settings":"cmnd/radar3/settings"}
	

When it detects movement, it publishes the following:

	tele/radar3/presence {"entry":1,"type":"rng","x":0,"y":0,"speed":0,"reference":0}
	tele/radar3/presence {"entry":1,"type":"rng","x":0,"y":0,"speed":0,"reference":0}
	tele/radar3/presence {"entry":1,"type":"rng","x":0,"y":0,"speed":0,"reference":0}
	tele/radar3/presence {"entry":1,"type":"rng","x":0,"y":0,"speed":0,"reference":0}
	tele/radar3/presence {"entry":1,"type":"rng","x":0,"y":0,"speed":0,"reference":0}
	tele/radar3/presence {"entry":0}
