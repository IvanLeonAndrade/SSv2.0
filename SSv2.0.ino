//  Sistema de Sensado (SSv1.0)
/*
    Copyright (C) 2023  Ivan Leon Andrade Franco
    Empresa: Silica Networks SA

    Sensa temperatura, humedad relativa, corriente continua,
    tension y detecta movimiento con sensor infrarojo con el
    protocolo SNMP v1
*/

#include <SPI.h>
#include <Ethernet.h>
#include <Streaming.h>
#include <Flash.h>
#include <MemoryFree.h>
#include <Agentuino.h>
#include <DHT.h>

/* Los siguientes terminos indican 
 * que variables pertenecen a cada sensor
 * CS: Current Sensor
 * VS: Voltage Sensor
 * MS: Move Sensor
*/
const uint8_t  dht_pin = 5;
const uint8_t  cs_pin = 0;
const uint8_t  vs_pin = 0;
const uint8_t  ms_pin = 3;
bool ms_state = false;


// Las magnitudes se envian con solicitudes SNMP (GET)
float corriente = 0.0;
float humedad = 0.0;
float temperatura = 0.0;


// Calibra CS
const float cs_tension_offset = 2475.0;
const float cs_sensitivity = 12.0; //  [mV]

// Tiempo alarmado del ms
const unsigned long ms_tiempo_alarmado = 360000; // Tiempo en milisegundos para mantener el valor en 1
unsigned long ms_tiempo_inicial = 0;

DHT dht(dht_pin, DHT22);

static byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};
IPAddress ip(192, 168, 0, 95);
IPAddress ip_dns(192, 168, 1, 1);
IPAddress ip_gateway(192, 168, 0, 1);
IPAddress subnet(255, 255, 255, 0);

char result[8];

const char sysDescr[] PROGMEM      = "1.3.6.1.2.1.1.1.0";
const char sysContact[] PROGMEM    = "1.3.6.1.2.1.1.4.0";
const char sysName[] PROGMEM       = "1.3.6.1.2.1.1.5.0";
const char sysLocation[] PROGMEM   = "1.3.6.1.2.1.1.6.0";
const char sysServices[] PROGMEM   = "1.3.6.1.2.1.1.7.0";

const char snmp_temperature[]     PROGMEM     = "1.3.6.1.3.2016.5.1.0";
const char snmp_humidity[]        PROGMEM     = "1.3.6.1.3.2016.5.1.1";
const char snmp_corriente[]        PROGMEM     = "1.3.6.1.3.2016.5.1.2";
const char snmp_movimiento[]        PROGMEM     = "1.3.6.1.3.2016.5.1.4";

static char locDescr[35]            = "Sistema de Sensado v1.0";
static char locContact[25]          = "Silica Networks SA";
static char locName[20]             = "Made NOC";
static char locLocation[20]         = "SMA 638 - CABA";
static int32_t locServices          = 2;

uint32_t prevMillis = millis();
char oid[SNMP_MAX_OID_LEN];
SNMP_API_STAT_CODES api_status;
SNMP_ERR_CODES status;

/**
    calculo_corriente - promedio
    @cs_sensitivity: Razon de la lectura de tension del cs por Amper (12mV/A)
    @cs_tension_offset: Centra la tension de lectura en cero

    Calcula y promedia el resultado de la corriente [A].

    cs entrega un rango de tension de 0 a 5 en [V]. Con 2.5V, 0A; 5V, 210A; 0V, -210A. Tension
    de lectura > 2.5v, es corriente positiva, caso contrario, corriente negativa.
*/
float calculo_corriente() {
  float cs_suma = 0.0;
  float cs_tension = 0.0;

  for (uint8_t i = 0; i < 4; i++) {
    cs_tension = (analogRead(cs_pin) * 5000.0) / 1023.0;
    cs_suma += (cs_tension - cs_tension_offset) / cs_sensitivity;
    delay(250);
  }

  return cs_suma / 4.0;
}

// Detecta movimiento

int detectar_movimiento() {
  if (digitalRead(ms_pin) == HIGH && !ms_state) {
      ms_state = true;
      ms_tiempo_inicial = millis();  
  } 

  if (ms_state && millis() - ms_tiempo_inicial > ms_tiempo_alarmado) { // Si ha pasado el tiempo, resetea el valor a 0
    ms_state = false;
  }
  delay(100); 
}


/** pduReceived
    Maneja solicitudes SNMP. Para los OID, responde
    con mensaje de error: READ_ONLY, si la solicitud SNMP
    es SET.

    Nota: No pueden enviarse TRAPS.
*/
void pduReceived() {
  SNMP_PDU pdu;
  api_status = Agentuino.requestPdu(&pdu);

  if ((pdu.type == SNMP_PDU_GET || pdu.type == SNMP_PDU_GET_NEXT || pdu.type == SNMP_PDU_SET)
      && pdu.error == SNMP_ERR_NO_ERROR && api_status == SNMP_API_STAT_SUCCESS ) {

    pdu.OID.toString(oid);

    if ( strcmp_P(oid, sysDescr ) == 0 ) {
      if ( pdu.type == SNMP_PDU_SET ) {

        pdu.type = SNMP_PDU_RESPONSE;
        pdu.error = SNMP_ERR_READ_ONLY;
      } else {

        status = pdu.VALUE.encode(SNMP_SYNTAX_OCTETS, locDescr);
        pdu.type = SNMP_PDU_RESPONSE;
        pdu.error = status;
      }
    } else if ( strcmp_P(oid, sysName ) == 0 ) {

      if ( pdu.type == SNMP_PDU_SET ) {

        status = pdu.VALUE.decode(locName, strlen(locName));
        pdu.type = SNMP_PDU_RESPONSE;
        pdu.error = status;
      } else {

        status = pdu.VALUE.encode(SNMP_SYNTAX_OCTETS, locName);
        pdu.type = SNMP_PDU_RESPONSE;
        pdu.error = status;
      }
    } else if ( strcmp_P(oid, sysContact ) == 0 ) {

      if ( pdu.type == SNMP_PDU_SET ) {

        status = pdu.VALUE.decode(locContact, strlen(locContact));
        pdu.type = SNMP_PDU_RESPONSE;
        pdu.error = status;
      } else {

        status = pdu.VALUE.encode(SNMP_SYNTAX_OCTETS, locContact);
        pdu.type = SNMP_PDU_RESPONSE;
        pdu.error = status;
      }
    } else if ( strcmp_P(oid, sysLocation ) == 0 ) {

      if ( pdu.type == SNMP_PDU_SET ) {

        status = pdu.VALUE.decode(locLocation, strlen(locLocation));
        pdu.type = SNMP_PDU_RESPONSE;
        pdu.error = status;
      } else {

        status = pdu.VALUE.encode(SNMP_SYNTAX_OCTETS, locLocation);
        pdu.type = SNMP_PDU_RESPONSE;
        pdu.error = status;
      }
      //
    } else if ( strcmp_P(oid, sysServices) == 0 ) {

      if ( pdu.type == SNMP_PDU_SET ) {

        pdu.type = SNMP_PDU_RESPONSE;
        pdu.error = SNMP_ERR_READ_ONLY;
      } else {

        status = pdu.VALUE.encode(SNMP_SYNTAX_INT, locServices);
        pdu.type = SNMP_PDU_RESPONSE;
        pdu.error = status;
      }
    } else if ( strcmp_P(oid, snmp_temperature ) == 0 ) {

      if ( pdu.type == SNMP_PDU_SET ) {

        pdu.type = SNMP_PDU_RESPONSE;
        pdu.error = SNMP_ERR_READ_ONLY;
      }
      else {
        status = pdu.VALUE.encode(SNMP_SYNTAX_OCTETS, dtostrf(temperatura, 6, 2, result));
        pdu.type = SNMP_PDU_RESPONSE;
        pdu.error = status;
      }
    } else if ( strcmp_P(oid, snmp_humidity ) == 0 ) {

      if ( pdu.type == SNMP_PDU_SET ) {

        pdu.type = SNMP_PDU_RESPONSE;
        pdu.error = SNMP_ERR_READ_ONLY;
      }
      else {
        status = pdu.VALUE.encode(SNMP_SYNTAX_OCTETS, dtostrf(humedad, 6, 2, result));
        pdu.type = SNMP_PDU_RESPONSE;
        pdu.error = status;
      }
    } else if ( strcmp_P(oid, snmp_corriente ) == 0 ) {
      // handle sysName (set/get) requests
      if ( pdu.type == SNMP_PDU_SET ) {

        pdu.type = SNMP_PDU_RESPONSE;
        pdu.error = SNMP_ERR_READ_ONLY;
      }
      else {
        status = pdu.VALUE.encode(SNMP_SYNTAX_OCTETS, dtostrf(corriente, 6, 2, result));
        pdu.type = SNMP_PDU_RESPONSE;
        pdu.error = status;
      }
    } else if ( strcmp_P(oid, snmp_movimiento ) == 0 ) {
      // handle sysName (set/get) requests
      if ( pdu.type == SNMP_PDU_SET ) {

        pdu.type = SNMP_PDU_RESPONSE;
        pdu.error = SNMP_ERR_READ_ONLY;
      }
      else {
        int ms_state_value = ms_state ? 1 : 0;
        status = pdu.VALUE.encode(SNMP_SYNTAX_OCTETS, dtostrf(ms_state_value, 6, 2, result));
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
  api_status = Agentuino.begin(); // Inicia agente SNMP
  dht.begin();
  
  pinMode(ms_pin, INPUT);
  // Verificación estado agente SNMP
  if ( api_status == SNMP_API_STAT_SUCCESS ) {
    Agentuino.onPduReceive(pduReceived);
    delay(10);
    return;
  }
  delay(10);
}


void loop() {
  Agentuino.listen();
  // Actualiza los valores cada 2s
  if (millis() - prevMillis > 2000) {
    temperatura = dht.readTemperature();
    humedad = dht.readHumidity();
    corriente = calculo_corriente();
    detectar_movimiento();
    
    prevMillis = millis();
  }
}
