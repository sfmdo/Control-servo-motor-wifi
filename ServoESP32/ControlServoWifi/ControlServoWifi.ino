#include <WiFi.h>
#include <WebServer.h> // <- Librería cambiada
#include <ESP32Servo.h>

// --- CONFIGURACIÓN DE RED ---
const char* ssid = "Fam_Flores-2024";
const char* password = "ChucheRoloBeto4488";

// --- CONFIGURACIÓN DEL HARDWARE ---
#define PIN_SERVO 13
#define NUM_ANGLES 10 // Aumentado para mayor flexibilidad con el nuevo método de entrada
Servo miServo;

// --- OBJETOS GLOBALES ---
WebServer server(80); // <- Objeto del servidor cambiado

// --- ESTADO Y LÓGICA DEL SERVO ---
enum Mode { MANUAL, WIPER, SEQUENCE, PULSE, RANDOM };
Mode currentMode = MANUAL;

int actualAngle = 90;
int lastWrittenAngle = -1;
int targetAngle = 90;

int angleList[NUM_ANGLES] = {0, 45, 90, 135, 180}; // Ejemplo inicial
int numAnglesInList = 5; // Número de ángulos actualmente en la lista
int sequenceIndex = 0;

// --- VARIABLES DE TEMPORIZACIÓN (NO BLOQUEANTE) ---
unsigned long lastMoveTime = 0;
const long moveInterval = 15; // Intervalo para un movimiento suave (ms)

unsigned long lastSequenceStepTime = 0;
const long sequenceInterval = 1500; // Tiempo entre cada paso de la secuencia (ms)

// --- FUNCIÓN AUXILIAR PARA CONVERTIR MODO A TEXTO ---
String modeToString(Mode mode) {
  switch (mode) {
    case MANUAL: return "Manual";
    case WIPER: return "Limpiaparabrisas";
    case SEQUENCE: return "Secuencia";
    case PULSE: return "Pulso";
    case RANDOM: return "Aleatorio";
    default: return "Desconocido";
  }
}

void setServoAngle(int newAngle) {
  if (newAngle != lastWrittenAngle) {
    miServo.write(newAngle);
    lastWrittenAngle = newAngle; // Actualizamos el último ángulo enviado
  }
}

// --- MANEJADOR DE PETICIONES DE ESTADO (GET /status) ---
// Adaptado para WebServer
void handleStatus() {
  String json = "{";
  json += "\"angle\":" + String(actualAngle) + ",";
  json += "\"mode\":\"" + modeToString(currentMode) + "\",";
  json += "\"wifiStatus\":\"Conectado\"";
  json += "}";
  
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", json);
}

// --- MANEJADOR DE PETICIONES DE CONTROL (GET /control?...) ---
// Completamente reescrito para usar server.arg() como en tu segundo ejemplo
void handleControl() {
    server.sendHeader("Access-Control-Allow-Origin", "*");

    // Cambiar el modo si se especifica
    if (server.hasArg("mode")) {
        String modeStr = server.arg("mode");
        if (modeStr == "manual") {
            currentMode = MANUAL;
        } else if (modeStr == "wiper") {
            currentMode = WIPER;
            targetAngle = 180; // Inicia el movimiento
        } else if (modeStr == "sequence") {
            // Si se pasa una nueva lista de ángulos (ej: ?angles=0,90,180)
            if (server.hasArg("angles")) {
                String anglesStr = server.arg("angles");
                int currentIndex = 0;
                int lastComma = -1;

                // Parsear la cadena de ángulos separados por comas
                for (int i = 0; i < anglesStr.length() && currentIndex < NUM_ANGLES; i++) {
                    if (anglesStr.charAt(i) == ',') {
                        angleList[currentIndex++] = anglesStr.substring(lastComma + 1, i).toInt();
                        lastComma = i;
                    }
                }
                // Añadir el último ángulo
                if (currentIndex < NUM_ANGLES) {
                    angleList[currentIndex++] = anglesStr.substring(lastComma + 1).toInt();
                }
                numAnglesInList = currentIndex;
            }
            currentMode = SEQUENCE;
            sequenceIndex = 0;
            lastSequenceStepTime = 0; // Para ejecutar el primer paso inmediatamente
        } else if (modeStr == "pulse") {
            currentMode = PULSE;
        } else if (modeStr == "random") {
            currentMode = RANDOM;
            lastSequenceStepTime = 0;
        }
    }

    // Establecer el ángulo si se especifica y estamos en modo MANUAL
    if (server.hasArg("angle") && currentMode == MANUAL) {
        String angleStr = server.arg("angle");
        Serial.print("[DEBUG] Ángulo recibido: ");
        Serial.println(angleStr);

        // Convertimos a entero
        int newAngle = angleStr.toInt();
        targetAngle = newAngle;

        Serial.print("[DEBUG] Target Angle establecido a: ");
        Serial.println(targetAngle);
    }
    
    Serial.println("[DEBUG] Enviando respuesta 'OK' al cliente.");
    server.send(200, "text/plain", "OK");
    Serial.println("[DEBUG] Respuesta enviada correctamente.");
}

void setup() {
  Serial.begin(115200);
  
  miServo.attach(PIN_SERVO);
  miServo.write(actualAngle);

  // Conexión a WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Conectado!");
  Serial.print("Dirección IP: ");
  Serial.println(WiFi.localIP());

  // Definir Endpoints del Servidor (estilo WebServer)
  server.on("/status", HTTP_GET, handleStatus);
  server.on("/control", HTTP_GET, handleControl);

  // Manejador para peticiones pre-vuelo de CORS (OPTIONS)
  server.onNotFound([]() {
    if (server.method() == HTTP_OPTIONS) {
        server.sendHeader("Access-Control-Allow-Origin", "*");
        server.sendHeader("Access-Control-Allow-Methods", "GET,POST,OPTIONS");
        server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
        server.send(204);
    } else {
        server.send(404, "text/plain", "Not Found");
    }
  });

  server.begin();
  Serial.println("Servidor HTTP iniciado.");
}

void loop() {
  // 1. Maneja las peticiones web (requerido por WebServer)
  server.handleClient();

  // 2. Lógica de movimiento del servo (sin cambios)
  unsigned long currentTime = millis();

  switch (currentMode) {
    case WIPER:
      if (actualAngle <= 0) targetAngle = 180;
      else if (actualAngle >= 180) targetAngle = 0;
      break;

    case SEQUENCE:
      if (currentTime - lastSequenceStepTime >= sequenceInterval) {
        lastSequenceStepTime = currentTime;
        targetAngle = angleList[sequenceIndex];
        sequenceIndex = (sequenceIndex + 1) % numAnglesInList;
      }
      break;
    
    case PULSE:
      {
        float pulseRadians = (currentTime % 4000) / 4000.0 * 2.0 * PI;
        targetAngle = 90 + 45 * sin(pulseRadians);
      }
      break;

    case RANDOM:
      if (currentTime - lastSequenceStepTime >= sequenceInterval) {
        lastSequenceStepTime = currentTime;
        targetAngle = random(0, 181);
      }
      break;

    case MANUAL:
      // El targetAngle se establece vía API
      break;
  }

  // --- Motor de Movimiento Suave (común a todos los modos) ---
  if (currentTime - lastMoveTime > moveInterval) {
    if (actualAngle < targetAngle) {
      actualAngle++;
      setServoAngle(actualAngle);   
      lastMoveTime = currentTime;
    } else if (actualAngle > targetAngle) {
      actualAngle--;
      setServoAngle(actualAngle);
      lastMoveTime = currentTime;
    }
  }
}