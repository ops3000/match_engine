// //match_engine/src/kafka_event_handler.h
#pragma once
#include "event_handler/event_handler.h"
#include <nlohmann/json.hpp>
#include <librdkafka/rdkafkacpp.h>

using json = nlohmann::json;
namespace RapidTrader {
class KafkaEventHandler : public EventHandler {
public:
    explicit KafkaEventHandler(std::unique_ptr<RdKafka::Producer> producer, std::string topic_name);
    ~KafkaEventHandler() override = default;
protected:
    void handleOrderAdded(const OrderAdded& event) override;
    void handleOrderDeleted(const OrderDeleted& event) override;
    void handleOrderUpdated(const OrderUpdated& event) override;
    void handleOrderExecuted(const ExecutedOrder& event) override;
    void handleSymbolAdded(const SymbolAdded& event) override;
    void handleSymbolDeleted(const SymbolDeleted& event) override;
private:
    void send_message(const std::string& key, const std::string& payload);
    json order_to_json(const Order& order);
    std::unique_ptr<RdKafka::Producer> kafka_producer;
    std::string topic_name;
};
} // namespace RapidTrader