// for Samsung AQ07XAN
#include "esphome.h"
#include "IRremoteESP8266.h"
#include "IRsend.h"
#include "ir_Samsung.h"

const float const_min = 16;
const float const_max = 30;
const float const_target = 25;

const uint16_t kIrLed = 14; // IR LED (OUTPUT) PIN D5 ( ИК светодиод)

const uint16_t kOnLed = 13; // "ON" LED (INPUT) PIN D7 (Импульсы со светодиода при включении, через оптопару)

IRSamsungAc ac(kIrLed);     // ONLI ESP8266

const uint16_t anti_pulse = 5000;  // таймаут, миллисекунды
boolean PinState;                  // статус со светодиода состояния
int buttonPushCounterOn = 0;       // счётчик нажатия кнопки
int buttonPushCounterOff = 0;      // счётчик нажатия кнопки
boolean PowerState = false;        // состояние вкл - выкл

class SamsungAC : public Component, public Climate {
  public: 

    sensor::Sensor *sensor_{nullptr};

    void set_sensor(sensor::Sensor *sensor) { this->sensor_ = sensor; }

    void setup() override
    {
      pinMode(kOnLed, INPUT_PULLUP);

      if (this->sensor_) {
        this->sensor_->add_on_state_callback([this](float state) {
          this->current_temperature = state;
          this->publish_state();
        });
        this->current_temperature = this->sensor_->state;
      } else {
        this->current_temperature = NAN;
      }

      auto restore = this->restore_state_();
      if (restore.has_value()) {
        restore->apply(this);
      } else {
        this->mode = climate::CLIMATE_MODE_OFF;
        this->target_temperature = roundf(clamp(this->current_temperature, const_min, const_max));
        this->fan_mode = climate::CLIMATE_FAN_AUTO;
        this->swing_mode = climate::CLIMATE_SWING_OFF;
      }

      if (isnan(this->target_temperature)) {
        this->target_temperature = const_target;
      }

      ac.begin();
      ac.on();
      if (this->mode == CLIMATE_MODE_OFF) {
        ac.off();
      } else if (this->mode == CLIMATE_MODE_COOL) {
        ac.setMode(kSamsungAcCool);
      } else if (this->mode == CLIMATE_MODE_DRY) {
        ac.setMode(kSamsungAcDry);
      } else if (this->mode == CLIMATE_MODE_FAN_ONLY) {
        ac.setMode(kSamsungAcFan);
      } else if (this->mode == CLIMATE_MODE_HEAT) {
        ac.setMode(kSamsungAcHeat);
      } else if (this->mode == CLIMATE_MODE_AUTO) {
        ac.setMode(kSamsungAcFanAuto);
      } 
      ac.setTemp(this->target_temperature);
      if (this->fan_mode == CLIMATE_FAN_AUTO) {
        ac.setFan(kSamsungAcFanAuto);
      } else if (this->fan_mode == CLIMATE_FAN_LOW) {
        ac.setFan(kSamsungAcFanLow);
      } else if (this->fan_mode == CLIMATE_FAN_MEDIUM) {
        ac.setFan(kSamsungAcFanMed);
      } else if (this->fan_mode == CLIMATE_FAN_HIGH) {
        ac.setFan(kSamsungAcFanHigh);
      }
      if (this->swing_mode == CLIMATE_SWING_OFF) {
        ac.setSwing(false);
      } else if (this->swing_mode == CLIMATE_SWING_VERTICAL) {
        ac.setSwing(true);
      }
      if (this->mode == CLIMATE_MODE_OFF) {
        ac.sendOff();
      } else {
        ac.send();
      }

      ESP_LOGD("DEBUG", "Samsung A/C remote is in the following state:");
      ESP_LOGD("DEBUG", "  %s\n", ac.toString().c_str());
    }

    void loop() override
    {
      PinState = !digitalRead(kOnLed);  // читаем состояние пина. 1 - on, 0 - off 
      // если нули
      if ( !PinState && PowerState ) {
        buttonPushCounterOff++;
      }    
      // если единицы
      if ( PinState && !PowerState ) {
        buttonPushCounterOn++;
        buttonPushCounterOff=0;
      }    
      // сбрасываем счетчик если ипульсы поступают
      if ( PinState && PowerState && buttonPushCounterOff!=0 ) {
      buttonPushCounterOff=0;
      }    
      // выключяем если во включенном состоянии
      if (buttonPushCounterOff > 2000 && PowerState) {
        ESP_LOGD("DEBUG", "PowerOff led");
        PowerState = false;
        buttonPushCounterOff=0;
        if ( this->mode != CLIMATE_MODE_OFF ) {
          this->mode = climate::CLIMATE_MODE_OFF;
          ESP_LOGD("DEBUG", "Sync'd HA with LED. AC OFF");
          this->publish_state();
        }
      }
      // включяем если в выключенном состоянии
      if (buttonPushCounterOn > 300 && !PowerState) {
        ESP_LOGD("DEBUG", "PowerOn led");
        PowerState = true;
        buttonPushCounterOn=0;
        if ( this->mode == CLIMATE_MODE_OFF ) {
          this->mode = climate::CLIMATE_MODE_AUTO;
          ESP_LOGD("DEBUG", "Sync'd HA with LED AC AUTO");
          this->publish_state();
        }
        
      }
    }

    climate::ClimateTraits traits() {
      auto traits = climate::ClimateTraits();
      traits.set_visual_min_temperature(const_min);
      traits.set_visual_max_temperature(const_max);
      traits.set_visual_temperature_step(1);
      traits.set_supported_modes({
          climate::CLIMATE_MODE_OFF,
          climate::CLIMATE_MODE_HEAT_COOL,
          climate::CLIMATE_MODE_COOL,
          climate::CLIMATE_MODE_DRY,
          climate::CLIMATE_MODE_HEAT,
          climate::CLIMATE_MODE_FAN_ONLY,
      });
      traits.set_supported_fan_modes({
          climate::CLIMATE_FAN_AUTO,
          climate::CLIMATE_FAN_LOW,
          climate::CLIMATE_FAN_MEDIUM,
          climate::CLIMATE_FAN_HIGH,
      });
      traits.set_supported_swing_modes({
          climate::CLIMATE_SWING_OFF,
          climate::CLIMATE_SWING_VERTICAL,
      });
      traits.set_supports_current_temperature(true);
      return traits;
    }
    
  void control(const ClimateCall &call) override {

    if (call.get_mode().has_value()) {
      ClimateMode mode = *call.get_mode();
      if (mode == CLIMATE_MODE_OFF) {
        ac.off();
      } else if (mode == CLIMATE_MODE_COOL) {
        ac.on();
        ac.setMode(kSamsungAcCool);
      } else if (mode == CLIMATE_MODE_DRY) {
        ac.on();
        ac.setMode(kSamsungAcDry);
      } else if (mode == CLIMATE_MODE_FAN_ONLY) {
        ac.on();
        ac.setMode(kSamsungAcFan);
      } else if (mode == CLIMATE_MODE_HEAT) {
        ac.on();
        ac.setMode(kSamsungAcHeat);
      } else if (mode == CLIMATE_MODE_HEAT_COOL) {
        ac.on();
        ac.setMode(kSamsungAcFanAuto);
      }
      this->mode = mode;
    }

    if (call.get_target_temperature().has_value()) {
      float temp = *call.get_target_temperature();
      ac.setTemp(temp);
      this->target_temperature = temp;
    }

    if (call.get_fan_mode().has_value()) {
      ClimateFanMode fan_mode = *call.get_fan_mode();
      if (fan_mode == CLIMATE_FAN_AUTO) {
        ac.setFan(kSamsungAcFanAuto);
      } else if (fan_mode == CLIMATE_FAN_LOW) {
        ac.setFan(kSamsungAcFanLow);
      } else if (fan_mode == CLIMATE_FAN_MEDIUM) {
        ac.setFan(kSamsungAcFanMed);
      } else if (fan_mode == CLIMATE_FAN_HIGH) {
        ac.setFan(kSamsungAcFanHigh);
      }
      this->fan_mode = fan_mode;
    }

    if (call.get_swing_mode().has_value()) {
      ClimateSwingMode swing_mode = *call.get_swing_mode();
      if (swing_mode == CLIMATE_SWING_OFF) {
        ac.setSwing(false);
      } else if (swing_mode == CLIMATE_SWING_VERTICAL) {
        ac.setSwing(true);
      }
      this->swing_mode = swing_mode;
    }

    if (this->mode == CLIMATE_MODE_OFF) {
      ac.sendOff();
    } else {
      ac.send();
    }

    this->publish_state();

    ESP_LOGD("DEBUG", "Samsung A/C remote is in the following state:");
    ESP_LOGD("DEBUG", "  %s\n", ac.toString().c_str());
  }
};
