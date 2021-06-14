#pragma once

#include <boost/asio.hpp>
#include <rosasio/timer.hpp>
#include "publisher.hpp"

namespace rosasio
{
    /**
     * @brief A publisher that messages can only be published no faster than a given interval regardless of the actual publish rate.
     */
    template <class MsgType>
    class ThrottledPublisher : rosasio::Publisher<MsgType>
    {
    public:
        template <typename Rep, typename Period>
        ThrottledPublisher(rosasio::Node &node, const std::string &topic_name, const std::chrono::duration<Rep, Period> &min_interval, bool latched = false)
            : rosasio::Publisher<MsgType>(node, topic_name, latched),
              m_interval_timer(node.get_ioc(), min_interval, std::bind(&ThrottledPublisher<MsgType>::handle_interval, this)),
              m_message_is_dirty(false)
        {
            //
        }

        void publish(const MsgType &msg) override
        {
            m_latest_message = msg;
            m_message_is_dirty = true;
        }

    private:
        void handle_interval()
        {
            if (m_message_is_dirty)
            {
                rosasio::Publisher<MsgType>::publish(m_latest_message);
                m_message_is_dirty = false;
            }
        }

        rosasio::Timer m_interval_timer;
        MsgType m_latest_message;
        bool m_message_is_dirty;
    };
}
