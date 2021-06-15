#pragma once

#include <boost/asio.hpp>
#include "publisher.hpp"
#include <rosasio/timer.hpp>

namespace rosasio
{
    /**
     * @brief A publisher that ensures the most recently published message is re-published at a minimum rate.
     */
    template <class MsgType>
    class IntervalPublisher : rosasio::Publisher<MsgType>
    {
    public:
        template <typename Rep, typename Period>
        IntervalPublisher(rosasio::Node &node, const std::string &topic_name, const std::chrono::duration<Rep, Period> &interval, bool latched = false)
            : rosasio::Publisher<MsgType>(node, topic_name, latched),
              m_interval_timer(node.get_ioc(), interval, std::bind(&IntervalPublisher<MsgType>::handle_interval, this)),
              m_initial_message_published(false)
        {
            //
        }

        void publish(const MsgType &msg) override
        {
            m_initial_message_published = true;
            m_latest_message = msg;

            rosasio::Publisher<MsgType>::publish(msg);
        }

    private:
        void handle_interval()
        {
            // TODO don't just blindly publish here, make sure to publish only when we haven't otherwise published for an interval
            // TODO also update the stamp in the header?
            if (m_initial_message_published)
            {
                rosasio::Publisher<MsgType>::publish(m_latest_message);
            }
        }

        rosasio::Timer m_interval_timer;
        MsgType m_latest_message;
        bool m_initial_message_published;
    };
}
