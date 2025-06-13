// //match_engine/include/event_handler/event_handler.h
#ifndef RAPID_TRADER_EVENT_HANDLER_H
#define RAPID_TRADER_EVENT_HANDLER_H

// Forward declare to break circular dependency
#include "event_handler/event.h"

namespace RapidTrader {
class OrderBookHandler;
class MapOrderBook;

class EventHandler
{
public:
    EventHandler(const EventHandler &) = delete;
    EventHandler(EventHandler &&) = delete;
    EventHandler &operator=(const EventHandler &) = delete;
    EventHandler &operator=(EventHandler &&) = delete;
    EventHandler() = default;
    virtual ~EventHandler() = default;

    friend class OrderBookHandler;
    friend class MapOrderBook;

protected:
    // LCOV_EXCL_START
    virtual void handleOrderAdded(const OrderAdded &event) {}
    virtual void handleOrderDeleted(const OrderDeleted &event) {}
    virtual void handleOrderUpdated(const OrderUpdated &event) {}
    virtual void handleOrderExecuted(const ExecutedOrder &event) {}
    virtual void handleSymbolAdded(const SymbolAdded &event) {}
    virtual void handleSymbolDeleted(const SymbolDeleted &event) {}
    // LCOV_EXCL_STOP
};
} // namespace RapidTrader
#endif // RAPID_TRADER_EVENT_HANDLER_H