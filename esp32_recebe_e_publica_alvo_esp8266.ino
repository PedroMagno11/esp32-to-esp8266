#include <WiFi.h>
#include <esp_now.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

#define STATUS_BUTTON 36
#define LED_PIN 2  // Renomeado para evitar conflito com myMessage.ledPin

// Definir os endereços MAC dos dispositivos
uint8_t mac_peer1[] = {0xC4, 0x5B, 0xBE, 0x71, 0xD1, 0x1C};
uint8_t mac_peer2[] = {0x80, 0x7D, 0x3A, 0x44, 0x5A, 0xBC};
esp_now_peer_info_t peer1;
esp_now_peer_info_t peer2;

// Estrutura de dados para armazenar as informações de destino
typedef struct TargetMessage {
  int tgt_id;
  char tgt_type[20];
  struct Position {
    int posX;
    int posY;
  } tgt_position;
  bool ledState; // Renomeado para evitar conflito
} TargetMessage;

TargetMessage myMessage;

// Configuração do WiFi e MQTT
const char* ssid = "iPhone de Pedro";
const char* password = "birulinha";
const char* mqtt_server = "broker.hivemq.com"; // IP do broker MQTT
WiFiClient espClient;
PubSubClient client(espClient);

void callback(char* topic, byte* payload, unsigned int length) {
  StaticJsonDocument<200> jsonDoc;
  DeserializationError error = deserializeJson(jsonDoc, payload, length);

  if (error) {
    Serial.print("Erro ao decodificar JSON: ");
    Serial.println(error.c_str());
    return;
  }

  // Atualizar a estrutura com os dados recebidos
  myMessage.tgt_id = jsonDoc["tgt_id"];
  strncpy(myMessage.tgt_type, jsonDoc["tgt_type"], sizeof(myMessage.tgt_type));
  myMessage.tgt_position.posX = jsonDoc["tgt_position"]["posX"];
  myMessage.tgt_position.posY = jsonDoc["tgt_position"]["posY"];

  // Enviar os dados via ESP-NOW
  esp_now_send(NULL, (uint8_t*)&myMessage, sizeof(myMessage));
}

void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Conectando-se a ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi conectado");
  Serial.println("Endereço IP: ");
  Serial.println(WiFi.localIP());
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Tentando conectar ao MQTT...");
    if (client.connect("ESP32Client")) {
      Serial.println("conectado");
      client.subscribe("sinal/acompanhamento"); // Inscrever-se no tópico MQTT
    } else {
      Serial.print("falhou, rc=");
      Serial.print(client.state());
      Serial.println(" tentando novamente em 5 segundos");
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  setup_wifi();

  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);

  WiFi.mode(WIFI_STA);

  if (esp_now_init() != 0) {
    Serial.println("Problema durante a inicialização ESP-NOW");
  }

  memcpy(peer1.peer_addr, mac_peer1, 6);
  peer1.encrypt = 0;
  if (esp_now_add_peer(&peer1) == ESP_OK) {
    Serial.println("Peer 1 adicionado");
  }

  memcpy(peer2.peer_addr, mac_peer2, 6);
  peer2.encrypt = 0;
  if (esp_now_add_peer(&peer2) == ESP_OK) {
    Serial.println("Peer 2 adicionado");
  }

  pinMode(STATUS_BUTTON, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);
}

void loop() {
  // Reconectar ao MQTT, se necessário
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  // Atualizar o estado do LED baseado no botão
  if (digitalRead(STATUS_BUTTON) == LOW) {  // Verifica se o botão foi pressionado
    myMessage.ledState = !myMessage.ledState;  // Alterna o estado do LED
    digitalWrite(LED_PIN, myMessage.ledState ? HIGH : LOW);  // Atualiza o LED

    // Envia o estado atualizado do LED via ESP-NOW
    esp_now_send(NULL, (uint8_t*)&myMessage, sizeof(myMessage));
    delay(500);  // Delay para evitar leitura excessiva
  }
}
