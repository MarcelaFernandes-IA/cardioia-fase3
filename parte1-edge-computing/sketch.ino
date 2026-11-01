
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <DHTesp.h>
#include <time.h>
#include "secrets.h"

// CARDIOIA - ESP32 + EDGE + MQTT

const int PINO_DHT = 15;
const int PINO_BOTAO = 4;

const char* WIFI_NOME = "Wokwi-GUEST";
const char* WIFI_SENHA = "";

const char* MQTT_SERVIDOR =
  "le1e1b1e.ala.us-east-1.emqxsl.com";

const int MQTT_PORTA = 8883;
const char* MQTT_TOPICO =
  "cardioia/paciente01/sinais";

// Certificado CA publico do EMQX
const char* CERTIFICADO_CA = R"EOF(
-----BEGIN CERTIFICATE-----
MIIDjjCCAnagAwIBAgIQAzrx5qcRqaC7KGSxHQn65TANBgkqhkiG9w0BAQsFADBh
MQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3
d3cuZGljZXJ0LmNvbTEgMB4GA1UEAxMXRGlnaUNlcnQgR2xvYmFsIFJvb3QgRzIw
HhcNMTMwODAxMTIwMDAwWhcNMzgwMTE1MTIwMDAwWjBhMQswCQYDVQQGEwJVUzEV
MBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3d3cuZGljZXJ0LmNvbTEg
MB4GA1UEAxMXRGlnaUNlcnQgR2xvYmFsIFJvb3QgRzIwMIIBIjANBgkqhkiG9w0B
AQEFAAOCAQ8AMIIBCgKCAQEAuzfNNNx7a8myaJCtSnX/RrohCgiN9RlUyfuI
2/Ou8jqJkTx65qsGGmvPrC3oXgkkRLpimn7Wo6h+4FR1IAWsULecYxpsMNzaHxmx
1x7e/dfgy5SDN67sH0NO3Xss0r0upS/kqbitOtSZpLYl6ZtrAGCSYP9PIUkY92eQ
q2EGnI/yuum06ZIya7XzV+hdG82MHauVBJVJ8zUtluNJbd134/tJS7SsVQepj5Wz
tCO7TG1F8PapspUwtP1MVYwnSlcUfIKdzXOS0xZKBgyMUNGPHgm+F6HmIcr9g+UQ
vIOlCsRnKPZzFBQ9RnbDhxSJITRNrw9FDKZJobq7nMWxM4MphQIDAQABo0IwQDAP
BgNVHRMBAf8EBTADAQH/MA4GA1UdDwEB/wQEAwIBhjAdBgNVHQ4EFgQUTiJUIBiV
5uNu5g/6+rkS7QYXjzkwDQYJKoZIhvcNAQELBQADggEBAGBnKJRvDkhj6zHd6mcY
1Yl9PMWLSn/pvtsrF9+wX3N3KjITOYFnQoQj8kVnNeyIv/iPsGEMNKSuIEyExtv4
NeF22d+mQrvHRAiGfzZ0JFrabA0UWTW98kndth/Jsw1HKj2ZL7tcu7XUIOGZX1NG
Fdtom/DzMNU+MeKNhJ7jitralj41E6Vf8PlwUHBHQRFXGU7Aj64GxJUTFy8bJZ91
8rGOmaFvE7FBcf6IKshPECBV1/MUReXgRPTqh5Uykw7+U0b6LJ3/iyK5S9kJRaTe
pLiaWN0bfVKfjllDiIGknibVb63dDcY3fe0Dkhvld1927jyNxF1WW6LZZm6zNTfl
MrY=
-----END CERTIFICATE-----
)EOF";


DHTesp dht;
WiFiClientSecure clienteSeguro;
PubSubClient mqtt(clienteSeguro);

// Fila circular de 1000 registros
const int MAX_REGISTROS = 1000;

struct Registro {
  unsigned long tempo;
  float temperatura;
  float umidade;
  int bpm;
};

Registro fila[MAX_REGISTROS];

int inicioFila = 0;
int totalRegistros = 0;
unsigned long descartados = 0;

bool conexaoHabilitada = true;

unsigned long ultimaLeitura = 0;
unsigned long inicioJanela = 0;
unsigned long ultimoClique = 0;
unsigned long ultimaTentativa = 0;

int cliques = 0;
int bpm = 0;
bool botaoAnterior = HIGH;

void armazenar(Registro r) {
  if (totalRegistros == MAX_REGISTROS) {
    inicioFila = (inicioFila + 1) % MAX_REGISTROS;
    totalRegistros--;
    descartados++;
    Serial.println("AVISO: registro antigo descartado");
  }

  int pos = (inicioFila + totalRegistros)
            % MAX_REGISTROS;

  fila[pos] = r;
  totalRegistros++;

  Serial.print("Armazenado. Pendentes: ");
  Serial.println(totalRegistros);
}

void conectarWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;

  Serial.println("Conectando ao Wi-Fi...");
  WiFi.begin(WIFI_NOME, WIFI_SENHA);
}

void conectarMQTT() {
  if (!conexaoHabilitada) return;
  if (WiFi.status() != WL_CONNECTED) return;
  if (mqtt.connected()) return;

  Serial.println("Conectando ao EMQX...");

  String id = "cardioia-esp32-" +
              String((uint32_t)ESP.getEfuseMac(), HEX);

  if (mqtt.connect(id.c_str(),
                   MQTT_USUARIO,
                   MQTT_SENHA)) {
    Serial.println("MQTT CONECTADO!");
  } else {
    Serial.print("Falha MQTT, codigo: ");
    Serial.println(mqtt.state());
  }
}

void sincronizar() {
  if (!conexaoHabilitada || !mqtt.connected()) {
    return;
  }

  while (totalRegistros > 0 && mqtt.connected()) {
    Registro r = fila[inicioFila];

    char mensagem[256];

    bool alerta = (r.bpm > 120 ||
                   r.temperatura > 38);

    snprintf(
      mensagem,
      sizeof(mensagem),
      "{\"paciente\":\"paciente01\","
      "\"tempo_ms\":%lu,"
      "\"temperatura\":%.2f,"
      "\"umidade\":%.2f,"
      "\"bpm\":%d,"
      "\"alerta\":%s}",
      r.tempo,
      r.temperatura,
      r.umidade,
      r.bpm,
      alerta ? "true" : "false"
    );

    if (mqtt.publish(MQTT_TOPICO, mensagem)) {
      Serial.print("MQTT ENVIADO: ");
      Serial.println(mensagem);

      inicioFila = (inicioFila + 1) % MAX_REGISTROS;
      totalRegistros--;
    } else {
      Serial.println("Falha no envio; dado preservado");
      break;
    }

    mqtt.loop();
    Serial.print("Estado do botão: ");
Serial.println(digitalRead(PINO_BOTAO));
  }
}

void setup() {
  Serial.begin(115200);

  dht.setup(PINO_DHT, DHTesp::DHT22);
  pinMode(PINO_BOTAO, INPUT_PULLUP);

  inicioJanela = millis();

  clienteSeguro.setInsecure();
  mqtt.setServer(MQTT_SERVIDOR, MQTT_PORTA);
  mqtt.setBufferSize(512);

  // Sincroniza relogio para validar TLS
  configTime(0, 0, "pool.ntp.org");

  conectarWiFi();

  Serial.println("CARDIOIA INICIADO");
  Serial.println("0 = OFFLINE | 1 = ONLINE");
}

void loop() {
  unsigned long agora = millis();

  // Comandos manuais
  if (Serial.available()) {
    char comando = Serial.read();

    if (comando == '0') {
      conexaoHabilitada = false;
      mqtt.disconnect();
      Serial.println("MODO OFFLINE");
    }

    if (comando == '1') {
      conexaoHabilitada = true;
      Serial.println("MODO ONLINE");
    }
  }

  // Leitura do botao
  bool botaoAtual = digitalRead(PINO_BOTAO);

  if (botaoAnterior == HIGH &&
      botaoAtual == LOW &&
      agora - ultimoClique > 150) {
    cliques++;
    ultimoClique = agora;
    Serial.println("Batimento detectado!");
  }

  botaoAnterior = botaoAtual;

  // BPM por janela de 15 segundos
  if (agora - inicioJanela >= 15000) {
    bpm = cliques * 4;
    cliques = 0;
    inicioJanela = agora;

    Serial.print("BPM: ");
    Serial.println(bpm);
  }

  // Coleta periodica
  if (agora - ultimaLeitura >= 2000) {
    ultimaLeitura = agora;

    TempAndHumidity dados =
      dht.getTempAndHumidity();

    if (!isnan(dados.temperature) &&
        !isnan(dados.humidity)) {
      Registro r;

      r.tempo = agora;
      r.temperatura = dados.temperature;
      r.umidade = dados.humidity;
      r.bpm = bpm;

      armazenar(r);
    }
  }

  // Conexao e sincronizacao
  if (conexaoHabilitada) {
    if (WiFi.status() == WL_CONNECTED) {
      if (!mqtt.connected() &&
          agora - ultimaTentativa >= 5000) {
        ultimaTentativa = agora;

        // Espera horario valido para TLS
        if (time(nullptr) > 1700000000) {
          conectarMQTT();
        } else {
          Serial.println("Aguardando horario NTP...");
        }
      }

      if (mqtt.connected()) {
        mqtt.loop();
        sincronizar();
      }
    } else if (agora - ultimaTentativa >= 5000) {
      ultimaTentativa = agora;
      conectarWiFi();
    }
  }

  delay(10);
}
