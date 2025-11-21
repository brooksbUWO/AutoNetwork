#include "AutoNetworkJsonBuilder.h"
#include "AutoNetworkParameter.h"

JsonDocument AutoNetworkJsonBuilder::createDocument(size_t estimatedSize)
{
    JsonDocument doc;
    (void)estimatedSize; // Unused in v7
    return doc;
}

void AutoNetworkJsonBuilder::serialize(JsonDocument &doc, String &output, size_t estimatedSize)
{
    output.reserve(estimatedSize);
    serializeJson(doc, output);
    doc.clear();
}

void AutoNetworkJsonBuilder::buildStatusJson(
    String &output,
    uint8_t status,
    bool wifiConnected,
    const String &staSSID,
    const String &macAddress,
    const String &ipAddress,
    uint8_t portalState,
    bool portalActive)
{
    constexpr size_t STATUS_JSON_SIZE = 256;
    JsonDocument json = createDocument(STATUS_JSON_SIZE);

    json["conn"]["status"] = status;
    json["conn"]["wifiConnected"] = wifiConnected;
    json["conn"]["ssid"] = staSSID.c_str();
    json["conn"]["mac"] = macAddress.c_str();
    json["conn"]["ip"] = ipAddress.c_str();
    json["portal"]["state"] = portalState;
    json["portal"]["active"] = portalActive;

    serialize(json, output, STATUS_JSON_SIZE);
}

void AutoNetworkJsonBuilder::buildSchemaJson(
    String &output,
    AutoNetworkParameter **parameters,
    uint8_t paramCount,
    const AutoNetworkParameterTypeNames *paramTypes)
{
    constexpr size_t SCHEMA_JSON_SIZE = 512;
    JsonDocument json = createDocument(SCHEMA_JSON_SIZE);
    JsonArray arr = json.to<JsonArray>();

    for (uint8_t i = 0; i < paramCount; i++)
    {
        AutoNetworkParameter *p = parameters[i];
        JsonObject obj = arr.add<JsonObject>();
        obj["id"] = p->_id;
        obj["t"] = paramTypes[p->_type].type;
        obj["n"] = p->_name;
        obj["v"] = p->_value;
        obj["p"] = p->_placeholder;
        obj["r"] = p->_required;
    }

    serialize(json, output, SCHEMA_JSON_SIZE);
}
