/**
 * @file SensorTelemetryModule.cpp
 * @brief Implementation of the Sensor Telemetry Module
 */

#include "SensorTelemetryModule.h"
#include "MeshService.h"
#include "NodeDB.h"
#include "RTC.h"
#include "Router.h"
#include "configuration.h"
#include "mesh/generated/meshtastic/telemetry.pb.h"
#include <Arduino.h>

// Global instance
SensorTelemetryModule *sensorTelemetryModule;

SensorTelemetryModule::SensorTelemetryModule()
    : SinglePortModule("SensorTelemetry", meshtastic_PortNum_PRIVATE_APP),
      OSThread("SensorTelem")
{
    LOG_INFO("SensorTelemetryModule: Initializing...\n");
    
    // TODO: Load configuration from NodeDB preferences
    // TODO: Validate configuration parameters
    
    LOG_INFO("SensorTelemetryModule: Telemetry interval = %d seconds\n", telemetryInterval / 1000);
    LOG_INFO("SensorTelemetryModule: Schema interval = %d seconds\n", schemaInterval / 1000);
}

int32_t SensorTelemetryModule::runOnce()
{
    uint32_t now = millis();
    uint32_t nextRun = UINT32_MAX;
    
    // ========================================================================
    // 1. Schema broadcast (on first run and every schemaInterval)
    // ========================================================================
    if (firstRun || (now - lastSchemaTime >= schemaInterval)) {
        LOG_INFO("SensorTelemetryModule: Broadcasting schema...\n");
        
        String schema = generateSchema();
        
        // Validate message size
        if (schema.length() > MAX_MESSAGE_SIZE) {
            LOG_ERROR("SensorTelemetryModule: Schema too large! %d bytes (max %d)\n", 
                     schema.length(), MAX_MESSAGE_SIZE);
            // TODO: Implement schema compression or split into multiple packets
        } else {
            LOG_DEBUG("SensorTelemetryModule: Schema size: %d bytes\n", schema.length());
            
            if (sendToMesh(schema, false)) {
                LOG_INFO("SensorTelemetryModule: Schema broadcast SUCCESS\n");
                lastSchemaTime = now;
                consecutiveFailures = 0;
            } else {
                LOG_ERROR("SensorTelemetryModule: Schema broadcast FAILED\n");
                consecutiveFailures++;
                // TODO: Implement exponential backoff
            }
        }
        
        firstRun = false;
    }
    
    // Calculate next schema time
    uint32_t nextSchema = lastSchemaTime + schemaInterval;
    if (nextSchema > now && nextSchema - now < nextRun) {
        nextRun = nextSchema - now;
    }
    
    // ========================================================================
    // 2. Telemetry broadcast (every telemetryInterval)
    // ========================================================================
    if (now - lastTelemetryTime >= telemetryInterval) {
        LOG_INFO("SensorTelemetryModule: Reading sensors...\n");
        
        float temp, batteryVoltage;
        
        if (readSensors(temp, batteryVoltage)) {
            LOG_DEBUG("SensorTelemetryModule: Temperature = %.2f C, Battery = %.2f V\n", 
                     temp, batteryVoltage);
            
            String telemetry = generateTelemetry(temp, batteryVoltage);
            
            // Validate message size
            if (telemetry.length() > MAX_MESSAGE_SIZE) {
                LOG_ERROR("SensorTelemetryModule: Telemetry too large! %d bytes (max %d)\n", 
                         telemetry.length(), MAX_MESSAGE_SIZE);
            } else {
                LOG_DEBUG("SensorTelemetryModule: Telemetry size: %d bytes\n", telemetry.length());
                
                if (sendToMesh(telemetry, false)) {
                    LOG_INFO("SensorTelemetryModule: Telemetry broadcast SUCCESS\n");
                    lastTelemetryTime = now;
                    consecutiveFailures = 0;
                } else {
                    LOG_ERROR("SensorTelemetryModule: Telemetry broadcast FAILED\n");
                    consecutiveFailures++;
                    // TODO: Implement exponential backoff
                }
            }
        } else {
            LOG_ERROR("SensorTelemetryModule: Sensor read FAILED\n");
            consecutiveFailures++;
            // TODO: Implement sensor recovery/reset
        }
    }
    
    // Calculate next telemetry time
    uint32_t nextTelemetry = lastTelemetryTime + telemetryInterval;
    if (nextTelemetry > now && nextTelemetry - now < nextRun) {
        nextRun = nextTelemetry - now;
    }
    
    // ========================================================================
    // 3. Watchdog and error handling
    // ========================================================================
    if (consecutiveFailures >= 5) {
        LOG_WARN("SensorTelemetryModule: %d consecutive failures, resetting state...\n", 
                consecutiveFailures);
        // TODO: Implement recovery strategy
        consecutiveFailures = 0;
    }
    
    // Return next execution time (minimum 1 second to prevent busy loop)
    int32_t sleepTime = (nextRun == UINT32_MAX) ? 60000 : nextRun;
    if (sleepTime < 1000) sleepTime = 1000;
    
    LOG_DEBUG("SensorTelemetryModule: Next run in %d ms\n", sleepTime);
    return sleepTime;
}

ProcessMessage SensorTelemetryModule::handleReceived(const meshtastic_MeshPacket &mp)
{
    // TODO: Implement configuration message handling
    // For now, we don't handle incoming messages
    return ProcessMessage::CONTINUE;
}

bool SensorTelemetryModule::readSensors(float &temp, float &batteryVoltage)
{
    // ========================================================================
    // POC: Generate random sensor data for testing
    // ========================================================================
    
    // Simulate smooth temperature changes (random walk between 15-30°C)
    float tempChange = (random(-100, 100) / 100.0);  // ±1°C max change
    lastTemperature += tempChange;
    
    // Clamp to realistic range
    if (lastTemperature < 15.0) lastTemperature = 15.0;
    if (lastTemperature > 30.0) lastTemperature = 30.0;
    
    temp = lastTemperature;
    
    // Simulate smooth battery voltage changes (random walk between 3.3-4.2V)
    float batteryChange = (random(-10, 10) / 1000.0);  // ±0.01V max change
    lastBatteryVoltage += batteryChange;
    
    // Clamp to realistic range
    if (lastBatteryVoltage < 3.3) lastBatteryVoltage = 3.3;
    if (lastBatteryVoltage > 4.2) lastBatteryVoltage = 4.2;
    
    batteryVoltage = lastBatteryVoltage;
    
    // TODO: Replace with real sensor reading:
    // ========================================================================
    // BME280 Example (I2C):
    // ========================================================================
    // #include <Adafruit_BME280.h>
    // Adafruit_BME280 bme;
    // 
    // if (!bme.begin(0x76)) {
    //     LOG_ERROR("SensorTelemetryModule: BME280 not found!\n");
    //     return false;
    // }
    // 
    // temp = bme.readTemperature();  // Celsius
    // float humidity = bme.readHumidity();  // %
    // float pressure = bme.readPressure() / 100.0F;  // hPa
    // ========================================================================
    
    // TODO: Replace with real battery reading:
    // ========================================================================
    // Battery ADC Example (ESP32):
    // ========================================================================
    // #ifdef BATTERY_PIN
    //     uint32_t adcValue = analogRead(BATTERY_PIN);
    //     batteryVoltage = (adcValue / 4095.0) * 3.3 * 2.0;  // Voltage divider
    // #else
    //     batteryVoltage = 0.0;
    // #endif
    // ========================================================================
    
    return true;  // Always successful for simulation
}

String SensorTelemetryModule::generateSchema()
{
    // ========================================================================
    // Generate self-describing JSON schema
    // Format: Compact JSON optimized for <200 bytes
    // ========================================================================
    
    String nodeId = getNodeId();
    
    // Schema structure (compact):
    // v: version
    // id: node ID
    // t: type
    // n: name
    // fw: firmware version
    // s: sensors array
    // c: config array
    
    String schema = "{";
    schema += "\"v\":1,";
    schema += "\"id\":\"" + nodeId + "\",";
    schema += "\"t\":\"temp-sensor\",";
    schema += "\"n\":\"TempSensor v0.1.0\",";
    schema += "\"fw\":\"0.1.0\",";
    
    // Sensors
    schema += "\"s\":[";
    schema += "{\"id\":\"temp\",\"n\":\"Temperature\",\"u\":\"C\",\"t\":\"f\",\"r\":[-40,85]},";
    schema += "{\"id\":\"batt\",\"n\":\"Battery\",\"u\":\"V\",\"t\":\"f\",\"r\":[3.0,4.2]}";
    schema += "],";
    
    // Configuration parameters
    schema += "\"c\":[";
    schema += "{\"id\":\"int\",\"n\":\"Interval\",\"u\":\"s\",\"t\":\"i\",\"d\":300,\"r\":[60,3600]}";
    schema += "]";
    
    schema += "}";
    
    LOG_DEBUG("SensorTelemetryModule: Schema: %s\n", schema.c_str());
    
    return schema;
}

String SensorTelemetryModule::generateTelemetry(float temp, float batteryVoltage)
{
    // ========================================================================
    // Generate telemetry data packet
    // Format: Compact JSON optimized for <200 bytes
    // ========================================================================
    
    String nodeId = getNodeId();
    uint32_t timestamp = getCurrentTimestamp();
    
    // Telemetry structure (compact):
    // v: version
    // id: node ID
    // t: timestamp (Unix seconds)
    // d: data object
    
    String telemetry = "{";
    telemetry += "\"v\":1,";
    telemetry += "\"id\":\"" + nodeId + "\",";
    telemetry += "\"t\":" + String(timestamp) + ",";
    
    // Sensor data
    telemetry += "\"d\":{";
    telemetry += "\"temp\":" + String(temp, 2) + ",";
    telemetry += "\"batt\":" + String(batteryVoltage, 2);
    telemetry += "}";
    
    telemetry += "}";
    
    LOG_DEBUG("SensorTelemetryModule: Telemetry: %s\n", telemetry.c_str());
    
    return telemetry;
}

bool SensorTelemetryModule::sendToMesh(const String &payload, bool wantAck)
{
    // ========================================================================
    // Send message to Meshtastic mesh network
    // ========================================================================
    
    if (payload.length() == 0) {
        LOG_ERROR("SensorTelemetryModule: Empty payload, not sending\n");
        return false;
    }
    
    if (payload.length() > MAX_MESSAGE_SIZE) {
        LOG_ERROR("SensorTelemetryModule: Payload too large (%d > %d bytes)\n", 
                 payload.length(), MAX_MESSAGE_SIZE);
        return false;
    }
    
    // Create mesh packet
    meshtastic_MeshPacket *p = allocDataPacket();
    if (!p) {
        LOG_ERROR("SensorTelemetryModule: Failed to allocate packet\n");
        return false;
    }
    
    // Configure packet
    p->to = NODENUM_BROADCAST;  // Broadcast to all nodes
    p->decoded.portnum = meshtastic_PortNum_PRIVATE_APP;
    p->want_ack = wantAck;
    p->decoded.payload.size = payload.length();
    memcpy(p->decoded.payload.bytes, payload.c_str(), payload.length());
    
    // Send via router
    LOG_DEBUG("SensorTelemetryModule: Sending %d bytes to mesh...\n", payload.length());
    
    // Use the global service to send the mesh packet
    service->sendToMesh(p);
    
    return true;
}

uint32_t SensorTelemetryModule::getCurrentTimestamp()
{
    // Get Unix timestamp from RTC
    // If RTC not set, returns 0
    
    struct timeval tv;
    if (gettimeofday(&tv, NULL) == 0) {
        return tv.tv_sec;
    }
    
    return 0;  // RTC not set
}

String SensorTelemetryModule::getNodeId()
{
    // Get Meshtastic node ID as hex string
    uint32_t nodeNum = nodeDB->getNodeNum();
    
    char hexId[9];  // 8 hex chars + null terminator
    snprintf(hexId, sizeof(hexId), "%08x", nodeNum);
    
    return String(hexId);
}
