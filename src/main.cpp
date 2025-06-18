// //match_engine/src/main.cpp
#include "matching/market/concurrent_market.h"
#include "kafka_event_handler.h"
#include <csignal>
#include <iostream>
#include <memory>
#include <nlohmann/json.hpp>
#include <rdkafkacpp.h>
#include <string>
#include <thread>
#include <vector>

using namespace RapidTrader;
using json = nlohmann::json;
static volatile sig_atomic_t run = 1;
static void sigterm(int sig) { run = 0; }
OrderSide string_to_side(const std::string& s) { if (s == "buy") return OrderSide::Bid; return OrderSide::Ask; }
OrderTimeInForce string_to_tif(const std::string& s) { if (s == "IOC") return OrderTimeInForce::IOC; if (s == "FOK") return OrderTimeInForce::FOK; return OrderTimeInForce::GTC; }

// Helper function to produce rejection messages directly
void produce_rejection(RdKafka::Producer* producer, const std::string& topic, const json& payload, const std::string& reason) {
    if (!producer) {
        std::cerr << "Cannot produce rejection, producer is null." << std::endl;
        return;
    }
    json j_event;
    j_event["event_type"] = "OrderRejected";
    
    // Ensure data field exists and copy payload
    j_event["data"] = payload;
    j_event["data"]["reason"] = reason;
    
    // Get order_id safely
    uint64_t order_id = payload.value("order_id", 0);
    std::string key = std::to_string(order_id);
    std::string message_str = j_event.dump();
    
    producer->poll(0);
    RdKafka::ErrorCode err = producer->produce(topic, RdKafka::Topic::PARTITION_UA, RdKafka::Producer::RK_MSG_COPY,
                     const_cast<char *>(message_str.c_str()), message_str.size(),
                     key.c_str(), key.size(), 0, nullptr);

    if (err != RdKafka::ERR_NO_ERROR) {
        std::cerr << "% Produce failed for rejection: " << RdKafka::err2str(err) << std::endl;
    }
}

int main(int argc, char **argv) {
    const char* kafka_brokers_env = std::getenv("KAFKA_BOOTSTRAP_SERVERS");
    std::string brokers = kafka_brokers_env ? kafka_brokers_env : "localhost:9092";
    std::string inbound_topic = "match_engine_inbound";
    std::string outbound_topic = "match_engine_outbound";
    std::string group_id = "match_engine_group";
    std::cout << "Connecting to Kafka at " << brokers << std::endl;
    
    std::string errstr_prod;
    std::unique_ptr<RdKafka::Conf> prod_conf(RdKafka::Conf::create(RdKafka::Conf::CONF_GLOBAL));
    if (prod_conf->set("bootstrap.servers", brokers, errstr_prod) != RdKafka::Conf::CONF_OK) { 
        std::cerr << "% Failed to set bootstrap.servers: " << errstr_prod << std::endl; exit(1); 
    }
    
    // Create a dedicated producer for main to send rejections
    std::unique_ptr<RdKafka::Producer> main_producer(RdKafka::Producer::create(prod_conf.get(), errstr_prod));
    if (!main_producer) {
        std::cerr << "Failed to create main producer: " << errstr_prod << std::endl; exit(1);
    }
    
    const uint32_t num_threads = std::thread::hardware_concurrency() > 1 ? std::thread::hardware_concurrency() : 2;
    std::vector<std::unique_ptr<EventHandler>> event_handlers;
    for (uint32_t i = 0; i < num_threads; ++i) { 
        std::string handler_err;
        event_handlers.push_back(std::make_unique<KafkaEventHandler>(std::unique_ptr<RdKafka::Producer>(RdKafka::Producer::create(prod_conf.get(), handler_err)), outbound_topic)); 
    }
    ConcurrentMarket market(event_handlers, num_threads);
    std::cout << "Matching Engine started with " << num_threads << " threads." << std::endl;
    market.addSymbol(1, "BTC-USDT"); market.addSymbol(2, "ETH-USDT");
    std::cout << "Added mock symbols BTC-USDT (ID: 1) and ETH-USDT (ID: 2)." << std::endl;

    std::string errstr_cons;
    std::unique_ptr<RdKafka::Conf> conf(RdKafka::Conf::create(RdKafka::Conf::CONF_GLOBAL));
    if (conf->set("bootstrap.servers", brokers, errstr_cons) != RdKafka::Conf::CONF_OK || conf->set("group.id", group_id, errstr_cons) != RdKafka::Conf::CONF_OK) { 
        std::cerr << "% Failed to create consumer config: " << errstr_cons << std::endl; exit(1); 
    }
    std::unique_ptr<RdKafka::KafkaConsumer> consumer(RdKafka::KafkaConsumer::create(conf.get(), errstr_cons));
    if (!consumer) { std::cerr << "Failed to create consumer: " << errstr_cons << std::endl; exit(1); }
    consumer->subscribe({inbound_topic});
    std::cout << "Kafka consumer subscribed to topic: " << inbound_topic << std::endl;

    signal(SIGINT, sigterm); signal(SIGTERM, sigterm);
    while (run) {
        std::unique_ptr<RdKafka::Message> msg(consumer->consume(1000));
        if (!msg) continue;
        if (msg->err()) { if (msg->err() == RdKafka::ERR__TIMED_OUT) continue; std::cerr << "% Consume error: " << msg->errstr() << std::endl; continue; }
        
        std::string payload_str(static_cast<const char *>(msg->payload()), msg->len());
        try {
            json j = json::parse(payload_str);
            std::string operation = j.value("operation", "");
            json payload = j.value("payload", json::object());

            if (operation == "ADD_ORDER") {
                std::string order_type = payload.value("order_type", "limit");
                
                if (order_type == "limit") {
                    // Existing logic for limit orders
                    Order order = (string_to_side(payload["order_side"]) == OrderSide::Bid) ? 
                        Order::limitBidOrder(payload["order_id"], std::stoull(payload["instrument_id"].get<std::string>()), std::stoull(payload["price"].get<std::string>()), std::stoull(payload["quantity_ordered"].get<std::string>()), string_to_tif(payload["time_in_force"])) : 
                        Order::limitAskOrder(payload["order_id"], std::stoull(payload["instrument_id"].get<std::string>()), std::stoull(payload["price"].get<std::string>()), std::stoull(payload["quantity_ordered"].get<std::string>()), string_to_tif(payload["time_in_force"]));
                    market.addOrder(order);
                    std::cout << "Accepted LIMIT_ORDER: " << payload["order_id"] << std::endl;
                } else {
                    // NEW: Reject unsupported order types
                    std::cout << "Rejected unsupported order type '" << order_type << "' for order ID: " << payload["order_id"] << std::endl;
                    produce_rejection(main_producer.get(), outbound_topic, payload, "Order type '" + order_type + "' not supported by match_engine.");
                }
            } else if (operation == "CANCEL_ORDER") {
                // Existing logic
                market.deleteOrder(payload["instrument_id"], payload["order_id"]);
                std::cout << "Processed CANCEL_ORDER: " << payload["order_id"] << std::endl;
            }
        } catch (const json::parse_error& e) { std::cerr << "JSON parse error: " << e.what() << "\nPayload: " << payload_str << std::endl; } catch (const std::exception& e) { std::cerr << "Error processing message: " << e.what() << "\nPayload: " << payload_str << std::endl; }
    }
    
    main_producer->flush(5000);
    consumer->close(); 
    std::cout << "\nEngine shutting down..." << std::endl;
    return 0;
}