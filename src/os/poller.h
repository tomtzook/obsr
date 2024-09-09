#pragma once

#include <chrono>

#include "os/io.h"
#include "events/events.h"

namespace obsr::os {

class resource_poller : resource, public events::poller {
public:
    resource_poller();
    ~resource_poller() override;

    void add(resource& resource, events::event_types events) override;
    void set(resource& resource, events::event_types events) override;
    void remove(resource& resource) override;

    events::polled_events poll(size_t max_events, std::chrono::milliseconds timeout) override;

private:
    class event_data : public events::event_data {
    public:
        explicit event_data(void* events);

        size_t count() const override;
        void set_count(size_t count);

        descriptor get_descriptor(size_t index) const override;
        events::event_types get_events(size_t index) const override;

    private:
        void* m_events;
        size_t m_count;
    };

    void handle_error();

    void* m_events;
    event_data m_data;
};

}
