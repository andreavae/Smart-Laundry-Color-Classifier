#include <Smart-Laundry-Color-Classifier_inferencing.h>
#include <Arduino_APDS9960.h>
#include <ArduinoBLE.h>
#include <Wire.h>

// Configurazione servizio e caratteristica BLE
BLEService laundryService("19B10000-E8F2-537E-4F6C-D104768A1214");
BLEStringCharacteristic colorResultChar("19B10001-E8F2-537E-4F6C-D104768A1214", BLERead | BLENotify, 64);

// Buffer dinamico per le feature di Edge Impulse
static float features[EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE];
static unsigned long last_status_time = 0;

static void print_apds_i2c_diagnostic()
{
    Serial.println("Scansione bus I2C:");
    for (uint8_t address = 0x08; address <= 0x77; address++) {
        Wire.beginTransmission(address);
        if (Wire.endTransmission() == 0) {
            Serial.print("  Dispositivo trovato a 0x");
            if (address < 0x10) Serial.print('0');
            Serial.println(address, HEX);
        }
    }

    Wire.beginTransmission(0x39);
    Wire.write(0x92);
    if (Wire.endTransmission() != 0 || Wire.requestFrom((uint8_t)0x39, (uint8_t)1) != 1) {
        Serial.println("APDS9960: indirizzo 0x39 o registro ID non raggiungibile.");
        return;
    }

    uint8_t sensor_id = Wire.read();
    Serial.print("APDS9960 ID letto: 0x");
    if (sensor_id < 0x10) Serial.print('0');
    Serial.println(sensor_id, HEX);
    if (sensor_id != 0xAB) {
        Serial.println("ID inatteso: atteso 0xAB.");
    }
}

/**
 * @brief Copy raw feature data in out_ptr
 */
int raw_feature_get_data(size_t offset, size_t length, float *out_ptr) {
    memcpy(out_ptr, features + offset, length * sizeof(float));
    return 0;
}

void setup()
{
    Serial.begin(115200);

    // Inizializzazione Sensore APDS9960, con retry per l'avvio del sensore.
    bool apds_ready = false;
    for (uint8_t attempt = 1; attempt <= 5 && !apds_ready; attempt++) {
        Serial.print("Inizializzazione APDS9960, tentativo ");
        Serial.println(attempt);
        apds_ready = APDS.begin();
        if (!apds_ready) delay(250);
    }

    if (!apds_ready) {
        Serial.println("ERRORE: Inizializzazione APDS9960 fallita.");
        print_apds_i2c_diagnostic();
        while (1) {
            Serial.println("APDS9960 non disponibile; controllare i risultati I2C sopra.");
            delay(5000);
        }
    }

    // Inizializzazione Modulo Bluetooth BLE
    if (!BLE.begin()) {
        while (1) {
            Serial.println("ERRORE: Inizializzazione BLE fallita!");
            delay(1000);
        }
    }

    // Configurazione del servizio e della caratteristica BLE
    BLE.setLocalName("SmartLaundry-Nano33");
    BLE.setDeviceName("SmartLaundry-Nano33");
    BLE.setAdvertisedService(laundryService);
    laundryService.addCharacteristic(colorResultChar);
    BLE.addService(laundryService);

    // Avvio della trasmissione beacon BLE
    BLE.advertise();
    Serial.println("Dispositivo BLE attivo e in attesa di connessione...");
}

void loop()
{
    BLEDevice central = BLE.central();
    bool proximity_available = APDS.proximityAvailable();
    int proximity = -1;

    // 1. Controllo di prossimità (Trigger di presenza del capo)
    if (proximity_available) {
        proximity = APDS.readProximity();
    }

    if (millis() - last_status_time >= 2000) {
        last_status_time = millis();
        Serial.print("Sistema attivo | APDS9960: ");
        Serial.print(proximity_available ? "OK" : "in attesa");
        if (proximity_available) {
            Serial.print(" | prossimita: ");
            Serial.print(proximity);
        }
        Serial.print(" | BLE: ");
        Serial.println(central && central.connected() ? "connesso" : "in attesa");
    }

    // Se l'oggetto è più lontano di ~2 cm (nessun capo rilevato)
    if (proximity_available && proximity < 30) {
        if (central && central.connected()) {
            colorResultChar.writeValue("NESSUN_CAPO");
        }
        delay(500);
        return;
    }

    // 2. Acquisizione coordinate cromatiche dal sensore APDS9960
    if (APDS.colorAvailable()) {
        int r_raw, g_raw, b_raw, c_raw;
        APDS.readColor(r_raw, g_raw, b_raw, c_raw);

        // Normalizzazione delle coordinate (r_norm, g_norm, b_norm)
        float sum = (float)(r_raw + g_raw + b_raw);
        if (sum == 0.0f) sum = 1.0f;

        float r_norm = (float)r_raw / sum;
        float g_norm = (float)g_raw / sum;
        float b_norm = (float)b_raw / sum;

        // Popolamento del buffer di ingresso per l'inferenza
        for (size_t ix = 0; ix < EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE; ix += 3) {
            features[ix + 0] = r_norm;
            features[ix + 1] = g_norm;
            features[ix + 2] = b_norm;
        }

        // 3. Esecuzione dell'inferenza TinyML (Edge Impulse EON Compiler)
        signal_t features_signal;
        features_signal.total_length = EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE;
        features_signal.get_data = &raw_feature_get_data;

        ei_impulse_result_t result = { 0 };
        EI_IMPULSE_ERROR res = run_classifier(&features_signal, &result, false);

        if (res == EI_IMPULSE_OK) {
            // Ricerca della classe con massima confidenza
            float max_val = 0.0f;
            int max_idx = 0;
            for (uint16_t i = 0; i < EI_CLASSIFIER_LABEL_COUNT; i++) {
                if (result.classification[i].value > max_val) {
                    max_val = result.classification[i].value;
                    max_idx = i;
                }
            }

            // Formattazione stringa di output
            String top_label = String(ei_classifier_inferencing_categories[max_idx]);
            int confidence = (int)(max_val * 100.0f);
            String ble_payload = top_label + " (" + String(confidence) + "%)";

            Serial.print("Colore Riconosciuto: ");
            Serial.println(ble_payload);

            // 4. Trasmissione del dato via BLE
            if (central && central.connected()) {
                colorResultChar.writeValue(ble_payload.c_str());
            }
        }
    }

    delay(500);
}
