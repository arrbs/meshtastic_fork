/**
 * @file SensorTelemetryModule.h
 * @brief Custom sensor telemetry module for Meshtastic Sensor Network
 * 
 * This module broadcasts self-describing sensor telemetry data over the Meshtastic mesh.
 * In this POC version, it generates random temperature data for testing without physical sensors.
 * 
 * Features:
 * - Broadcasts schema on boot and every 30 minutes (configurable)
 * - Sends telemetry every 5 minutes (configurable)
 * - Self-describing JSON format (<200 bytes)
 * - Comprehensive error handling and logging
 * 
 * @author Meshtastic Sensor Network Project
 * @date November 7, 2025
 * @version 0.1.0
 */

#pragma once
#include "SinglePortModule.h"
#include "concurrency/OSThread.h"

/**
 * @brief Sensor telemetry module for broadcasting sensor data over mesh
 * 
 * This module extends SinglePortModule to handle custom sensor telemetry messages.
 * It runs as a separate thread (OSThread) with periodic execution.
 * 
 * POC Version: Generates random temperature data for testing
 * Production: Will read from BME280/DHT22 sensors
 * 
 * TODO: Add support for real BME280 sensor via I2C
 * TODO: Implement configuration via Meshtastic admin messages
 * TODO: Add low-battery alert threshold
 * TODO: Implement exponential backoff for message failures
 * TODO: Add watchdog timer for sensor hangs
 */
class SensorTelemetryModule : public SinglePortModule, private concurrency::OSThread
{
  public:
    /**
     * @brief Constructor for SensorTelemetryModule
     * 
     * Initializes the module with:
     * - Module name: "sensor-telemetry"
     * - Port number: PRIVATE_APP (for custom applications)
     * - Thread name: "SensorTelem"
     */
    SensorTelemetryModule();

  protected:
    /**
     * @brief Main execution loop - called periodically by OSThread
     * 
     * This method is called repeatedly by the thread scheduler.
     * It handles:
     * 1. Schema broadcast (on boot and every 30 minutes)
     * 2. Telemetry broadcast (every 5 minutes)
     * 3. Deep sleep management
     * 
     * @return int32_t Sleep time in milliseconds until next execution
     *                 Returns the time until the next scheduled event
     */
    virtual int32_t runOnce() override;

    /**
     * @brief Process incoming mesh packets addressed to this module
     * 
     * Handles incoming messages such as:
     * - Configuration commands
     * - Acknowledgments
     * 
     * @param mp Incoming MeshPacket
     * @return ProcessMessage Action to take (CONTINUE, STOP)
     * 
     * TODO: Implement configuration message handling
     */
    virtual ProcessMessage handleReceived(const meshtastic_MeshPacket &mp) override;

  private:
    // ========================================================================
    // Configuration (will eventually come from meshtastic config)
    // ========================================================================
    
    /** @brief Telemetry broadcast interval in milliseconds (default: 5 minutes) */
    uint32_t telemetryInterval = 5 * 60 * 1000;  // 5 minutes
    
    /** @brief Schema broadcast interval in milliseconds (default: 30 minutes) */
    uint32_t schemaInterval = 30 * 60 * 1000;    // 30 minutes
    
    /** @brief Maximum message size in bytes (Meshtastic/LoRa limit) */
    static const uint16_t MAX_MESSAGE_SIZE = 200;

    // ========================================================================
    // State tracking
    // ========================================================================
    
    /** @brief Timestamp of last telemetry broadcast */
    uint32_t lastTelemetryTime = 0;
    
    /** @brief Timestamp of last schema broadcast */
    uint32_t lastSchemaTime = 0;
    
    /** @brief Flag indicating if this is the first run (triggers immediate schema broadcast) */
    bool firstRun = true;
    
    /** @brief Count of consecutive message failures (for exponential backoff) */
    uint8_t consecutiveFailures = 0;

    // ========================================================================
    // Sensor simulation (POC - will be replaced with real sensor reading)
    // ========================================================================
    
    /** @brief Last simulated temperature value (for smooth changes) */
    float lastTemperature = 20.0;
    
    /** @brief Last simulated battery voltage (for smooth changes) */
    float lastBatteryVoltage = 3.85;

    // ========================================================================
    // Private methods
    // ========================================================================
    
    /**
     * @brief Read simulated sensor data (POC version)
     * 
     * Generates random temperature and battery readings for testing.
     * In production, this will read from real I2C/SPI sensors.
     * 
     * Temperature: Random walk between 15-30°C
     * Battery: Random walk between 3.3-4.2V
     * 
     * @param[out] temp Temperature in Celsius
     * @param[out] batteryVoltage Battery voltage in volts
     * @return bool True if read successful (always true for simulation)
     * 
     * TODO: Replace with real BME280 I2C sensor reading
     * TODO: Add real battery ADC reading
     * TODO: Add sensor failure detection and recovery
     */
    bool readSensors(float &temp, float &batteryVoltage);

    /**
     * @brief Generate self-describing schema packet (JSON format)
     * 
     * Creates a JSON schema describing:
     * - Node ID and type
     * - Firmware version
     * - Available sensors (temperature, battery)
     * - Configurable parameters
     * 
     * Example output:
     * {
     *   "v":1,"id":"a1b2c3d4","t":"temp-sensor",
     *   "n":"TempSensor v0.1.0","fw":"0.1.0",
     *   "s":[{"id":"temp","n":"Temperature","u":"C","t":"f","r":[-40,85]},
     *        {"id":"batt","n":"Battery","u":"V","t":"f","r":[3.0,4.2]}],
     *   "c":[{"id":"int","n":"Interval","u":"s","t":"i","d":300,"r":[60,3600]}]
     * }
     * 
     * @return String JSON schema (<200 bytes)
     * 
     * TODO: Add GPS location if available
     * TODO: Make schema configurable per hardware variant
     */
    String generateSchema();

    /**
     * @brief Generate telemetry data packet (JSON format)
     * 
     * Creates a compact JSON telemetry message with:
     * - Version
     * - Node ID
     * - Timestamp
     * - Sensor readings
     * 
     * Example output:
     * {"v":1,"id":"a1b2c3d4","t":1699387200,"d":{"temp":23.5,"batt":3.85}}
     * 
     * @param temp Temperature reading in Celsius
     * @param batteryVoltage Battery voltage in volts
     * @return String JSON telemetry (<200 bytes)
     */
    String generateTelemetry(float temp, float batteryVoltage);

    /**
     * @brief Send message to Meshtastic mesh network
     * 
     * Wraps the message in a MeshPacket and sends via Meshtastic radio.
     * 
     * @param payload JSON string to send
     * @param wantAck Request acknowledgment from recipients
     * @return bool True if sent successfully, false on error
     * 
     * TODO: Add retry logic with exponential backoff
     * TODO: Implement message queuing for failed sends
     */
    bool sendToMesh(const String &payload, bool wantAck = false);

    /**
     * @brief Get current Unix timestamp in seconds
     * 
     * @return uint32_t Current time in seconds since epoch
     */
    uint32_t getCurrentTimestamp();

    /**
     * @brief Get Meshtastic node ID as hex string
     * 
     * @return String Node ID (e.g., "a1b2c3d4")
     */
    String getNodeId();

    /**
     * @brief Ensure the "sensetastic" channel exists and is configured
     * 
     * Creates or updates the sensetastic channel if it doesn't exist.
     * Channel will be unencrypted (for POC) with standard settings.
     * 
     * @return uint8_t Channel index (0-7) of sensetastic channel, or 0xFF if failed
     * 
     * TODO: Add encryption in production
     * TODO: Make channel name configurable
     */
    uint8_t ensureSensetasticChannel();

    /**
     * @brief Find channel index by name
     * 
     * @param channelName Name of the channel to find
     * @return uint8_t Channel index (0-7), or 0xFF if not found
     */
    uint8_t findChannelByName(const char *channelName);
};

/** @brief Global instance of the sensor telemetry module */
extern SensorTelemetryModule *sensorTelemetryModule;
