#pragma once

#include <functional>

namespace cvi_file_recover {

template <typename T>
class EventHandler
{
public:
    using Handler = std::function<void(const T&)>;

    virtual ~EventHandler() = default;

    void setEventHandler(const Handler& handler)
    {
        this->handler = handler;
    }

    void setEventHandler(Handler&& handler)
    {
        this->handler = std::move(handler);
    }

    Handler getEventHandler()
    {
        return handler;
    }

    void handleEvent(const T& event)
    {
        if (handler) {
            handler(event);
        }
    }

private:
    Handler handler;
};

} // namespace cvi_file_recover
