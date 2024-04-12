/*  Sistema de Sensado Versión 2.0 (SSv1.0)
 *  Copyright (C) 2023  Iván León Andrade Franco
 *  Empresa: Silica Networks SA
 *
 *  Sensa temperature, humidity relativa y current
 *  continua. Las magnitudes obtienen con SNMP v1.
 */

#include <SPI.h>
#include <Ethernet.h>
#include <Streaming.h>
#include <Flash.h>
#include <MemoryFree.h>
#include <Agentuino.h>
#include <DHT.h>

const uint8_t dht_pin = 5;
const uint8_t current_sensor_pin = 0;
const uint8_t led_pin = 7;

float current = 0.0;
float humidity = 0.0;
float temperature = 0.0;

const float cs_voltage_offset = 2475.0;
const float cs_sensitivity = 12.0;

DHT dht(dht_pin, DHT22);

static byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xEF};
IPAddress ip(192, 168, 0, 95);
IPAddress ip_dns(192, 168, 1, 1);
IPAddress ip_gateway(192, 168, 0, 1);
IPAddress subnet(255, 255, 255, 0);

char result[8];

const char sysDHT[] PROGMEM = "1.3.6.1.2.1.1.1.0";
const char sysCurrent[] PROGMEM = "1.3.6.1.2.1.1.4.0";
const char sysSNMP[] PROGMEM = "1.3.6.1.2.1.1.5.0";
const char sysLocation[] PROGMEM = "1.3.6.1.2.1.1.6.0";
const char sysServices[] PROGMEM = "1.3.6.1.2.1.1.7.0";

const char snmp_temperature[] PROGMEM = "1.3.6.1.3.2016.5.1.0";
const char snmp_humidity[] PROGMEM = "1.3.6.1.3.2016.5.1.1";
const char snmp_current[] PROGMEM = "1.3.6.1.3.2016.5.1.2";

static char locDHT[35] = " ";
static char locCurrent[50] = " ";
static char locSNMP[40] = " ";
static char locLocation[20] = "SMA 638 - CABA";
static int32_t locServices = 2;

uint32_t prevMillis = millis();
char oid[SNMP_MAX_OID_LEN];
SNMP_API_STAT_CODES api_status;
SNMP_ERR_CODES status;

void led_temperature_alarm(float cache_temperature) {
  if (cache_temperature > 24.0) {
    digitalWrite(led_pin, HIGH);
  } else {
    digitalWrite(led_pin, LOW);
  }
}

void current_sensor_diagnosis(float cache_current) {
  if (cache_current < 0) {
    strcpy(locCurrent, "Sensor invertido o error en lectura");
  } else {
    strcpy(locCurrent, "Sensor Operativo");
  }
}

void dht_reading_diagnosis(float cache_temperature, float cache_humidity) {
  if (isnan(cache_temperature) && isnan(cache_humidity)) {
    strcpy(locDHT, "Sensor desconectado o averiado");
    } else {
      strcpy(locDHT, "Sensor Operativo");
      }  
  }

/*  calculo_current
 *  cs: Current sensor
 * 
 *  @cs_sensitivity: Voltage reading ratio per Ampere [12mV/A]
 *  @cs_voltage_offset: Center the voltage reading at 0
 */
float calculate_current(){
  float cs_sum = 0.0;
  float cs_voltage = 0.0;
  for (uint8_t i = 0; i < 4; i++) {
    cs_voltage = (analogRead(current_sensor_pin) * 5000.0) / 1023.0;
    cs_sum += (cs_voltage - cs_voltage_offset) / cs_sensitivity;
    delay(250);
  }
  return cs_sum / 4.0;
}

void pduReceived() {
  SNMP_PDU pdu;
  api_status = Agentuino.requestPdu(&pdu);

  bool isSnmpGetOperation = pdu.type == SNMP_PDU_GET || pdu.type == SNMP_PDU_GET_NEXT || pdu.type == SNMP_PDU_SET;
  bool isErrorFree = pdu.error == SNMP_ERR_NO_ERROR;
  bool isSuccess = api_status == SNMP_API_STAT_SUCCESS;
  if (isSnmpGetOperation && isErrorFree && isSuccess) {
    pdu.OID.toString(oid);
    if (strcmp_P(oid, sysDHT) == 0) {
      if (pdu.type == SNMP_PDU_SET) {
        pdu.type = SNMP_PDU_RESPONSE;
        pdu.error = SNMP_ERR_READ_ONLY;
      }
      else {
        status = pdu.VALUE.encode(SNMP_SYNTAX_OCTETS, locDHT);
        pdu.type = SNMP_PDU_RESPONSE;
        pdu.error = status;
      }
    }
    else if (strcmp_P(oid, sysSNMP) == 0) {
      if (pdu.type == SNMP_PDU_SET) {
        status = pdu.VALUE.decode(locSNMP, strlen(locSNMP));
        pdu.type = SNMP_PDU_RESPONSE;
        pdu.error = status;
      }
      else {
        status = pdu.VALUE.encode(SNMP_SYNTAX_OCTETS, locSNMP);
        pdu.type = SNMP_PDU_RESPONSE;
        pdu.error = status;
      }
    }
    else if (strcmp_P(oid, sysCurrent) == 0) {
      if (pdu.type == SNMP_PDU_SET) {
        status = pdu.VALUE.decode(locCurrent, strlen(locCurrent));
        pdu.type = SNMP_PDU_RESPONSE;
        pdu.error = status;
      }
      else {
        status = pdu.VALUE.encode(SNMP_SYNTAX_OCTETS, locCurrent);
        pdu.type = SNMP_PDU_RESPONSE;
        pdu.error = status;
      }
    }
    else if (strcmp_P(oid, sysLocation) == 0) {
      if (pdu.type == SNMP_PDU_SET) {
        status = pdu.VALUE.decode(locLocation, strlen(locLocation));
        pdu.type = SNMP_PDU_RESPONSE;
        pdu.error = status;
      }
      else {
        status = pdu.VALUE.encode(SNMP_SYNTAX_OCTETS, locLocation);
        pdu.type = SNMP_PDU_RESPONSE;
        pdu.error = status;
      }
    }
    else if (strcmp_P(oid, sysServices) == 0) {
      if (pdu.type == SNMP_PDU_SET) {
        pdu.type = SNMP_PDU_RESPONSE;
        pdu.error = SNMP_ERR_READ_ONLY;
      }
      else {
        status = pdu.VALUE.encode(SNMP_SYNTAX_INT, locServices);
        pdu.type = SNMP_PDU_RESPONSE;
        pdu.error = status;
      }
    }
    else if (strcmp_P(oid, snmp_temperature) == 0) {
      if (pdu.type == SNMP_PDU_SET) {
        pdu.type = SNMP_PDU_RESPONSE;
        pdu.error = SNMP_ERR_READ_ONLY;
      }
      else {
        status = pdu.VALUE.encode(SNMP_SYNTAX_OCTETS, dtostrf(temperature, 6, 2, result));
        pdu.type = SNMP_PDU_RESPONSE;
        pdu.error = status;
      }
    }
    else if (strcmp_P(oid, snmp_humidity) == 0) {
      if (pdu.type == SNMP_PDU_SET) {
        pdu.type = SNMP_PDU_RESPONSE;
        pdu.error = SNMP_ERR_READ_ONLY;
      }
      else {
        status = pdu.VALUE.encode(SNMP_SYNTAX_OCTETS, dtostrf(humidity, 6, 2, result));
        pdu.type = SNMP_PDU_RESPONSE;
        pdu.error = status;
      }
    }
    else if (strcmp_P(oid, snmp_current) == 0) {
      if (pdu.type == SNMP_PDU_SET) {
        pdu.type = SNMP_PDU_RESPONSE;
        pdu.error = SNMP_ERR_READ_ONLY;
      }
      else {
        status = pdu.VALUE.encode(SNMP_SYNTAX_OCTETS, dtostrf(current, 6, 2, result));
        pdu.type = SNMP_PDU_RESPONSE;
        pdu.error = status;
      }
    }
    else {
      pdu.type = SNMP_PDU_RESPONSE;
      pdu.error = SNMP_ERR_NO_SUCH_NAME;
    }
    Agentuino.responsePdu(&pdu);
  }
  Agentuino.freePdu(&pdu);
}

void setup() {
  Ethernet.begin(mac, ip, ip_dns, ip_gateway, subnet);
  api_status = Agentuino.begin();
  dht.begin();
  
  pinMode(led_pin, OUTPUT);
  
  if (api_status == SNMP_API_STAT_SUCCESS) {
    Agentuino.onPduReceive(pduReceived);
    delay(10);
    return;
  }
  delay(1000);
}

void loop() {
  Agentuino.listen();
  
  if (millis() - prevMillis > 5000) {
    temperature = dht.readTemperature();
    humidity = dht.readHumidity();
    current = calculate_current();
    
    dht_reading_diagnosis(temperature, humidity);
    led_temperature_alarm(temperature);
    current_sensor_diagnosis(current);
    
    prevMillis = millis();
  }
}
