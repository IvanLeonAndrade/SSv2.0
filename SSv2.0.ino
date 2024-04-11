/*  Sistema de Sensado Versión 2.0 (SSv1.0)
 *  Copyright (C) 2023  Iván León Andrade Franco
 *  Empresa: Silica Networks SA
 *
 *  Sensa temperatura, humedad relativa y corriente
 *  continua. Las magnitudes obtienen con SNMP v1.
 */

#include <SPI.h>
#include <Ethernet.h>
#include <Streaming.h>
#include <Flash.h>
#include <MemoryFree.h>
#include <Agentuino.h>
// #include <DHT.h>
#include <DHT.h>

// cs: Current Sensor
const uint8_t dht_pin = 5;
const uint8_t cs_pin = 0;
const uint8_t led_dht_diagnosis = 7;

float corriente = 0.0;
float humedad = 0.0;
float temperatura = 0.0;

const float cs_tension_offset = 2475.0;
const float cs_sensitivity = 12.0;

DHT dht(dht_pin, DHT22);

static byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};
// IPAddress ip(10, 83, 64, 64);
// IPAddress ip_dns(192, 168, 1, 1);
// IPAddress ip_gateway(10, 83, 64, 253);
// IPAddress subnet(255, 255, 255, 0);

IPAddress ip(192, 168, 0, 96);
IPAddress ip_dns(192, 168, 1, 1);
IPAddress ip_gateway(192, 168, 0, 1);
IPAddress subnet(255, 255, 255, 0);

char result[8];

const char sysDescr[] PROGMEM = "1.3.6.1.2.1.1.1.0";
const char sysContact[] PROGMEM = "1.3.6.1.2.1.1.4.0";
const char sysName[] PROGMEM = "1.3.6.1.2.1.1.5.0";
const char sysLocation[] PROGMEM = "1.3.6.1.2.1.1.6.0";
const char sysServices[] PROGMEM = "1.3.6.1.2.1.1.7.0";

const char snmp_temperature[] PROGMEM = "1.3.6.1.3.2016.5.1.0";
const char snmp_humidity[] PROGMEM = "1.3.6.1.3.2016.5.1.1";
const char snmp_corriente[] PROGMEM = "1.3.6.1.3.2016.5.1.2";

static char locDescr[35] = "Sistema de Sensado v1.0";
static char locContact[25] = "Silica Networks SA";
static char locName[20] = "Made NOC";
static char locLocation[20] = "SMA 638 - CABA";
static int32_t locServices = 2;

uint32_t prevMillis = millis();
char oid[SNMP_MAX_OID_LEN];
SNMP_API_STAT_CODES api_status;
SNMP_ERR_CODES status;
/*
void cs_reading_diagnosis() {
  float cache_corriente = 0.0;
  if (cache < 0) {
    Serial.println("error: negative reading");
    delay(1500);
  }
}*/
void dht_reading_diagnosis() {
  float cache_temperatura = 0.0;
  float cache_humedad = 0.0;

  cache_temperatura = dht.readTemperature();
  cache_humedad = dht.readHumidity();

  if (isnan(cache_temperatura) && isnan(cache_humedad)) {
    Serial.println("Failed to read from DHT sensor!");
    digitalWrite(led_dht_diagnosis, HIGH);
    delay(1500);
  } else {
    digitalWrite(led_dht_diagnosis, LOW);
    Serial.println("dht initialized correctly");
    delay(1500);
    }  
}

/**
 *  calculo_corriente
 *  Ajustar valor de variables según las caracteristicas del sensor
 *
 *  @cs_sensitivity: Razon de la lectura de tension del cs x Amper [12mV/A]
 *  @cs_tension_offset: Centra la tension de lectura en 0.
 *
 *  Calcula y promedia el resultado de corriente [A].
 */
float calculo_corriente(){
  float cs_suma = 0.0;
  float cs_tension = 0.0;

  for (uint8_t i = 0; i < 4; i++)
  {
    cs_tension = (analogRead(cs_pin) * 5000.0) / 1023.0;
    cs_suma += (cs_tension - cs_tension_offset) / cs_sensitivity;
    delay(250);
  }

  return cs_suma / 4.0;
}

void pduReceived() {
  SNMP_PDU pdu;
  api_status = Agentuino.requestPdu(&pdu);

  bool isSnmpGetOperation = pdu.type == SNMP_PDU_GET || pdu.type == SNMP_PDU_GET_NEXT || pdu.type == SNMP_PDU_SET;
  bool isErrorFree = pdu.error == SNMP_ERR_NO_ERROR;
  bool isSuccess = api_status == SNMP_API_STAT_SUCCESS;

  if (isSnmpGetOperation && isErrorFree && isSuccess) {
    pdu.OID.toString(oid);
    if (strcmp_P(oid, sysDescr) == 0) {
      if (pdu.type == SNMP_PDU_SET) {
        pdu.type = SNMP_PDU_RESPONSE;
        pdu.error = SNMP_ERR_READ_ONLY;
      }
      else {
        status = pdu.VALUE.encode(SNMP_SYNTAX_OCTETS, locDescr);
        pdu.type = SNMP_PDU_RESPONSE;
        pdu.error = status;
      }
    }
    else if (strcmp_P(oid, sysName) == 0) {
      if (pdu.type == SNMP_PDU_SET) {
        status = pdu.VALUE.decode(locName, strlen(locName));
        pdu.type = SNMP_PDU_RESPONSE;
        pdu.error = status;
      }
      else {
        status = pdu.VALUE.encode(SNMP_SYNTAX_OCTETS, locName);
        pdu.type = SNMP_PDU_RESPONSE;
        pdu.error = status;
      }
    }
    else if (strcmp_P(oid, sysContact) == 0) {
      if (pdu.type == SNMP_PDU_SET) {
        status = pdu.VALUE.decode(locContact, strlen(locContact));
        pdu.type = SNMP_PDU_RESPONSE;
        pdu.error = status;
      }
      else {
        status = pdu.VALUE.encode(SNMP_SYNTAX_OCTETS, locContact);
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
        status = pdu.VALUE.encode(SNMP_SYNTAX_OCTETS, dtostrf(temperatura, 6, 2, result));
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
        status = pdu.VALUE.encode(SNMP_SYNTAX_OCTETS, dtostrf(humedad, 6, 2, result));
        pdu.type = SNMP_PDU_RESPONSE;
        pdu.error = status;
      }
    }
    else if (strcmp_P(oid, snmp_corriente) == 0) {
      if (pdu.type == SNMP_PDU_SET) {
        pdu.type = SNMP_PDU_RESPONSE;
        pdu.error = SNMP_ERR_READ_ONLY;
      }
      else {
        status = pdu.VALUE.encode(SNMP_SYNTAX_OCTETS, dtostrf(corriente, 6, 2, result));
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
  Serial.begin(9600);
  Ethernet.begin(mac, ip, ip_dns, ip_gateway, subnet);
  api_status = Agentuino.begin();
  dht.begin();
  pinMode(led_dht_diagnosis, OUTPUT);
  if (api_status == SNMP_API_STAT_SUCCESS) {
    Agentuino.onPduReceive(pduReceived);
    delay(10);
    return;
  }
  delay(1000);
}

void loop() {
  Agentuino.listen();

  dht_reading_diagnosis();
  if (millis() - prevMillis > 5000) {
    temperatura = dht.readTemperature();
    humedad = dht.readHumidity();

    prevMillis = millis();
  }
}
