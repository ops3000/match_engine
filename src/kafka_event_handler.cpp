// //match_engine/src/kafka_event_handler.cpp
#include "kafka_event_handler.h"
#include "event_handler/event.h"
#include <iostream>

// All previous code from this file is correct. No change needed.
// The provided previous version is fine.
namespace RapidTrader {
KafkaEventHandler::KafkaEventHandler(std::unique_ptr<RdKafka::Producer> producer, std::string topic)
    : kafka_producer(std::move(producer)), topic_name(std::move(topic)) {}
void KafkaEventHandler::send_message(const std::string& key, const std::string& payload) {
    if (!kafka_producer) { std::cerr << "Kafka producer is not initialized." << std::endl; return; }
    kafka_producer->poll(0);
    RdKafka::ErrorCode err = kafka_producer->produce( topic_name, RdKafka::Topic::PARTITION_UA, RdKafka::Producer::RK_MSG_COPY, const_cast<char*>(payload.c_str()), payload.size(), key.c_str(), key.size(), 0, nullptr );
    if (err != RdKafka::ERR_NO_ERROR) { std::cerr << "% Produce failed for topic " << topic_name << ": " << RdKafka::err2str(err) << std::endl; }
}
json KafkaEventHandler::order_to_json(const Order& order) {
    json j; j["order_id"] = order.getOrderID(); j["symbol_id"] = order.getSymbolID(); j["price"] = order.getPrice(); j["quantity"] = order.getQuantity(); j["open_quantity"] = order.getOpenQuantity(); j["executed_quantity"] = order.getExecutedQuantity(); j["last_executed_price"] = order.getLastExecutedPrice(); j["last_executed_quantity"] = order.getLastExecutedQuantity(); j["side"] = order.isBid() ? "Bid" : "Ask";
    switch(order.getType()) { case OrderType::Limit: j["type"] = "Limit"; break; case OrderType::Market: j["type"] = "Market"; break; case OrderType::Stop: j["type"] = "Stop"; break; case OrderType::StopLimit: j["type"] = "StopLimit"; break; case OrderType::TrailingStop: j["type"] = "TrailingStop"; break; case OrderType::TrailingStopLimit: j["type"] = "TrailingStopLimit"; break; default: j["type"] = "Unknown"; break; }
    return j;
}
void KafkaEventHandler::handleOrderAdded(const OrderAdded& event) { json j_event; j_event["event_type"] = "OrderAdded"; j_event["data"] = order_to_json(event.order); send_message(std::to_string(event.order.getOrderID()), j_event.dump()); }
void KafkaEventHandler::handleOrderDeleted(const OrderDeleted& event) { json j_event; j_event["event_type"] = "OrderDeleted"; j_event["data"] = order_to_json(event.order); send_message(std::to_string(event.order.getOrderID()), j_event.dump()); }
void KafkaEventHandler::handleOrderUpdated(const OrderUpdated& event) { json j_event; j_event["event_type"] = "OrderUpdated"; j_event["data"] = order_to_json(event.order); send_message(std::to_string(event.order.getOrderID()), j_event.dump()); }
void KafkaEventHandler::handleOrderExecuted(const ExecutedOrder& event) { json j_event; j_event["event_type"] = "ExecutedOrder"; j_event["data"] = order_to_json(event.order); send_message(std::to_string(event.order.getOrderID()), j_event.dump()); }
void KafkaEventHandler::handleSymbolAdded(const SymbolAdded& event) { /* NO-OP */ }
void KafkaEventHandler::handleSymbolDeleted(const SymbolDeleted& event) { /* NO-OP */ }
} // namespace RapidTrader