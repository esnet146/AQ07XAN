Climate AC Samsung AQ07XAN

Only esp8266

Install:

Create dir homeassistant/esphome/include

Copy to dir files irsamsung60.h 

In YAML (config esphome) insert:

```
esphome:
  includes: 
    - include/irsamsung60.h
  libraries:
    - IRremoteESP8266

climate:
  - platform: custom
    lambda: |-
       auto samsungac = new SamsungAC();
       samsungac->set_sensor(id(current_temperature));
       App.register_component(samsungac);
       return {samsungac};
    climates:
    - name: "Samsung AC"

sensor:
  - platform: homeassistant
    name: "Temperature Sensor From Home Assistant"
    id: current_temperature
    entity_id: sensor.temperature_in_room
    internal: true
```

You can specify any sensor whose values you want to see in the climate card. 
These may be sensors of this or another esphome.
It is important to specify the correct sensor ID.

