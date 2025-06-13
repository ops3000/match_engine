// //match_engine/test/main_test.cpp
#include <gtest/gtest.h>
#include <memory>
#include <queue>

#include "event_handler/event.h"
#include "event_handler/event_handler.h"
#include "matching/market/market.h"
#include "matching/orderbook/order.h"

using namespace RapidTrader;

// A simple event handler for testing purposes that stores events in queues.
class TestEventHandler : public EventHandler
{
public:
    std::queue<SymbolAdded> symbol_added_events;
    std::queue<OrderAdded> order_added_events;
    std::queue<ExecutedOrder> order_executed_events;
    std::queue<OrderDeleted> order_deleted_events;

protected:
    void handleSymbolAdded(const SymbolAdded &event) override {
        symbol_added_events.push(event);
    }
    void handleOrderAdded(const OrderAdded &event) override {
        order_added_events.push(event);
    }
    void handleOrderExecuted(const ExecutedOrder &event) override {
        order_executed_events.push(event);
    }
    void handleOrderDeleted(const OrderDeleted &event) override {
        order_deleted_events.push(event);
    }
};

// Test fixture for setting up a Market with our TestEventHandler
class MarketTest : public ::testing::Test {
protected:
    std::unique_ptr<Market> market;
    TestEventHandler* event_handler_ptr; // Raw pointer to check events

    void SetUp() override {
        auto handler = std::make_unique<TestEventHandler>();
        event_handler_ptr = handler.get();
        market = std::make_unique<Market>(std::move(handler));
    }
};

// Test case 1: Test adding a symbol
TEST_F(MarketTest, AddSymbol) {
    const uint32_t symbol_id = 1;
    const std::string symbol_name = "BTC-USDT";

    market->addSymbol(symbol_id, symbol_name);

    ASSERT_TRUE(market->hasSymbol(symbol_id));
    ASSERT_EQ(event_handler_ptr->symbol_added_events.size(), 1);

    SymbolAdded event = event_handler_ptr->symbol_added_events.front();
    EXPECT_EQ(event.symbol_id, symbol_id);
    EXPECT_EQ(event.name, symbol_name);
}

// Test case 2: Test adding a single limit order to an empty book
TEST_F(MarketTest, AddSingleLimitOrder) {
    const uint32_t symbol_id = 1;
    market->addSymbol(symbol_id, "BTC-USDT");
    
    // Clear the symbol added event
    event_handler_ptr->symbol_added_events.pop();

    Order bid_order = Order::limitBidOrder(101, symbol_id, 50000, 10, OrderTimeInForce::GTC);
    market->addOrder(bid_order);

    ASSERT_EQ(event_handler_ptr->order_added_events.size(), 1);
    OrderAdded event = event_handler_ptr->order_added_events.front();
    EXPECT_EQ(event.order.getOrderID(), 101);
    EXPECT_EQ(event.order.getOpenQuantity(), 10);
}

// Test case 3: Test a simple match between two limit orders
TEST_F(MarketTest, SimpleLimitOrderMatch) {
    const uint32_t symbol_id = 1;
    market->addSymbol(symbol_id, "BTC-USDT");

    // Add a bid order
    Order bid_order = Order::limitBidOrder(101, symbol_id, 50000, 10, OrderTimeInForce::GTC);
    market->addOrder(bid_order);

    // Add an ask order that should match
    Order ask_order = Order::limitAskOrder(102, symbol_id, 50000, 10, OrderTimeInForce::GTC);
    market->addOrder(ask_order);

    // We expect:
    // 1. SymbolAdded event
    // 2. OrderAdded for bid
    // 3. OrderAdded for ask
    // 4. Two ExecutedOrder events (one for bid, one for ask)
    // 5. Two OrderDeleted events (since both are fully filled)
    
    ASSERT_EQ(event_handler_ptr->order_executed_events.size(), 2);
    ASSERT_EQ(event_handler_ptr->order_deleted_events.size(), 2);

    // Check bid execution
    ExecutedOrder executed_bid = event_handler_ptr->order_executed_events.front();
    event_handler_ptr->order_executed_events.pop();
    EXPECT_EQ(executed_bid.order.getOrderID(), 101);
    EXPECT_EQ(executed_bid.order.isFilled(), true);
    EXPECT_EQ(executed_bid.order.getLastExecutedPrice(), 50000);
    EXPECT_EQ(executed_bid.order.getLastExecutedQuantity(), 10);
    
    // Check ask execution
    ExecutedOrder executed_ask = event_handler_ptr->order_executed_events.front();
    event_handler_ptr->order_executed_events.pop();
    EXPECT_EQ(executed_ask.order.getOrderID(), 102);
    EXPECT_EQ(executed_ask.order.isFilled(), true);
    EXPECT_EQ(executed_ask.order.getLastExecutedPrice(), 50000);
    EXPECT_EQ(executed_ask.order.getLastExecutedQuantity(), 10);
}