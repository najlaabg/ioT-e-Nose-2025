  #include <WiFi.h>
  #include <PubSubClient.h>
  #include <DHT.h>
  #include <ArduinoJson.h>

  // --- Configurações Wi-Fi e MQTT ---
  const char* ssid = "agents";
  const char* password = "QgC9O8VucAByqvVu5Rruv1zdpqM66cd23KG4ElV7vZiJND580bzYvaHqz5k07G2";
  const char* mqtt_server = "broker.emqx.io";
  WiFiClient espClient;
  PubSubClient client(espClient);

  // --- DHT22 ---
  #define DHTPIN 5
  #define DHTTYPE DHT22
  DHT dht(DHTPIN, DHTTYPE);

  // --- Sensores MQ ---
  #define NUM_SENSORS 9
  #define NUM_READINGS 10
  #define ALPHA 0.01
  const int sensorPins[NUM_SENSORS] = { 25, 26, 27, 32, 33, 34, 35, 36, 39 };
  float filteredValues[NUM_SENSORS] = { 0 };

  // --- Setup Wi-Fi ---
  void setup_wifi() {
    Serial.print("Conectando-se a ");
    Serial.println(ssid);
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
    }
    Serial.println("\nWiFi conectado");
  }

  // --- Inicializar sensores ---
  void inicializarSensores() {
    for (int i = 0; i < NUM_SENSORS; i++) {
      pinMode(sensorPins[i], INPUT);
      filteredValues[i] = analogRead(sensorPins[i]);
    }
    dht.begin();
  }

  // --- Leitura dos sensores MQ com filtro exponencial ---
  float lerSensorFiltrado(int index) {
    long soma = 0;
    for (int i = 0; i < NUM_READINGS; i++) {
      soma += analogRead(sensorPins[index]);
    }
    float media = (float)soma / NUM_READINGS;

    if (index == 1) {  // MG811 - inverte valor
      media = 4095 - media;
    }

    filteredValues[index] = (1 - ALPHA) * filteredValues[index] + ALPHA * media;
    return filteredValues[index];
  }

  // --- Leitura do DHT22 ---
  void lerDHT(float& temp, float& umid) {
    temp = dht.readTemperature();
    umid = dht.readHumidity();
  }

  // --- Reconectar ao broker MQTT ---
  void reconnect() {
    while (!client.connected()) {
      Serial.print("Conectando ao broker MQTT...");
      String clientId = "NajlaEnose-";
      clientId += String(random(0xffff), HEX);
      if (client.connect(clientId.c_str())) {
        Serial.println("conectado");
      } else {
        Serial.print("falha, rc=");
        Serial.print(client.state());
        Serial.println(" tentando novamente em 5s...");
        delay(5000);
      }
    }
  }

  // --- Setup inicial ---
  void setup() {
    Serial.begin(115200);
    analogReadResolution(12);
    setup_wifi();
    client.setServer(mqtt_server, 1883);
    inicializarSensores();
  }

  // --- Loop principal ---
  void loop() {
    if (WiFi.status() != WL_CONNECTED) {
      WiFi.reconnect();
    }
    if (!client.connected()) {
      reconnect();
    }
    client.loop();

    float temperatura, umidade;
    lerDHT(temperatura, umidade);

    StaticJsonDocument<512> doc;
    if (!isnan(temperatura)) {
      doc["temperatura"] = temperatura;
    } else {
      doc["temperatura"].set(nullptr);
    }

    if (!isnan(umidade)) {
      doc["umidade"] = umidade;
    } else {
      doc["umidade"].set(nullptr);
    }


    JsonArray sensores = doc.createNestedArray("mq");
    for (int i = 0; i < NUM_SENSORS; i++) {
      sensores.add((int)lerSensorFiltrado(i));
    }

    char payload[512];
    serializeJson(doc, payload);

    client.publish("SustainOlive/eNose/Najla", payload);
    Serial.println(payload);

    delay(1000);  // intervalo de envio
  }
