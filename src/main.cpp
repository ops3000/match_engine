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
OrderSide string_to_side(const std::string& s) { if (s == "Bid") return OrderSide::Bid; return OrderSide::Ask; }
OrderTimeInForce string_to_tif(const std::string& s) { if (s == "IOC") return OrderTimeInForce::IOC; if (s == "FOK") return OrderTimeInForce::FOK; return OrderTimeInForce::GTC; }
int main(int argc, char **argv) {
    const char* kafka_brokers_env = std::getenv("KAFKA_BOOTSTRAP_SERVERS");
    std::string brokers = kafka_brokers_env ? kafka_brokers_env : "localhost:9092";
    std::string inbound_topic = "match_engine_inbound";
    std::string outbound_topic = "match_engine_outbound";
    std::string group_id = "match_engine_group";
    std::cout << "Connecting to Kafka at " << brokers << std::endl;
    std::string errstr;
    std::unique_ptr<RdKafka::Conf> prod_conf(RdKafka::Conf::create(RdKafka::Conf::CONF_GLOBAL));
    if (prod_conf->set("bootstrap.servers", brokers, errstr) != RdKafka::Conf::CONF_OK) { std::cerr << "% Failed to set bootstrap.servers: " << errstr << std::endl; exit(1); }
    const uint32_t num_threads = std::thread::hardware_concurrency() > 1 ? std::thread::hardware_concurrency() : 2;
    std::vector<std::unique_ptr<EventHandler>> event_handlers;
    for (uint32_t i = 0; i < num_threads; ++i) { event_handlers.push_back(std::make_unique<KafkaEventHandler>(std::unique_ptr<RdKafka::Producer>(RdKafka::Producer::create(prod_conf.get(), errstr)), outbound_topic)); }
    ConcurrentMarket market(event_handlers, num_threads);
    std::cout << "Matching Engine started with " << num_threads << " threads." << std::endl;
    market.addSymbol(1, "BTC-USDT"); market.addSymbol(2, "ETH-USDT");
    std::cout << "Added mock symbols BTC-USDT (ID: 1) and ETH-USDT (ID: 2)." << std::endl;
    std::unique_ptr<RdKafka::Conf> conf(RdKafka::Conf::create(RdKafka::Conf::CONF_GLOBAL));
    if (conf->set("bootstrap.servers", brokers, errstr) != RdKafka::Conf::CONF_OK || conf->set("group.id", group_id, errstr) != RdKafka::Conf::CONF_OK) { std::cerr << "% Failed to create consumer config: " << errstr << std::endl; exit(1); }
    std::unique_ptr<RdKafka::KafkaConsumer> consumer(RdKafka::KafkaConsumer::create(conf.get(), errstr));
    if (!consumer) { std::cerr << "Failed to create consumer: " << errstr << std::endl; exit(1); }
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
            std::string operation = j["operation"]; json payload = j["payload"];
            if (operation == "ADD_ORDER") {
                Order order = (string_to_side(payload["side"]) == OrderSide::Bid) ? Order::limitBidOrder(payload["order_id"], payload["instrument_id"], payload["price"], payload["quantity"], string_to_tif(payload["time_in_force"])) : Order::limitAskOrder(payload["order_id"], payload["instrument_id"], payload["price"], payload["quantity"], string_to_tif(payload["time_in_force"]));
                market.addOrder(order);
                std::cout << "Processed ADD_ORDER: " << payload["order_id"] << std::endl;
            } else if (operation == "CANCEL_ORDER") {
                market.deleteOrder(payload["instrument_id"], payload["order_id"]);
                std::cout << "Processed CANCEL_ORDER: " << payload["order_id"] << std::endl;
            }
        } catch (const json::parse_error& e) { std::cerr << "JSON parse error: " << e.what() << "\nPayload: " << payload_str << std::endl; } catch (const std::exception& e) { std::cerr << "Error processing message: " << e.what() << "\nPayload: " << payload_str << std::endl; }
    }
    consumer->close(); std::cout << "\nEngine shutting down..." << std::endl;
    return 0;
}