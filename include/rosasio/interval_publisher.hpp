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
        IntervalPublisher(rosasio::Node &node, const std::string &topic_name,  const std::chrono::duration<Rep, Period> &throttle_period, const std::chrono::duration<Rep, Period> &autorepeat_period, bool latched = false)
            : rosasio::Publisher<MsgType>(node, topic_name, latched),
              m_autorepeat_timer(node.get_ioc(), autorepeat_period, std::bind(&IntervalPublisher<MsgType>::handle_autorepeat_timer_tick, this)),
              m_throttle_timer(node.get_ioc()),
              m_initial_message_published(false),
              m_dirty(false),
              m_holdoff(false),
              m_throttle_period(std::chrono::duration_cast<decltype(m_throttle_period)>(throttle_period))
        {
            //
        }

        void publish(const MsgType &msg) override
        {
            m_initial_message_published = true;
            m_latest_message = msg;
            m_dirty = true;

            if (!m_holdoff)
            {
                rosasio::Publisher<MsgType>::publish(msg);
                m_holdoff = true;

                m_throttle_timer.expires_from_now(m_throttle_period);
                m_throttle_timer.async_wait(std::bind(&IntervalPublisher::handle_throttle_timer_tick, this, std::placeholders::_1));
            }
        }

        void handle_throttle_timer_tick(const boost::system::error_code &ec)
        {
            if (ec)
            {
                return;
            }

            if (m_dirty)
            {
                rosasio::Publisher<MsgType>::publish(m_latest_message);
                m_dirty = false;
            }

            m_holdoff = false;
        }

    private:
        void handle_autorepeat_timer_tick()
        {
            // TODO don't just blindly publish here, make sure to publish only when we haven't otherwise published for an interval
            // TODO also update the stamp in the header?
            if (m_initial_message_published)
            {
                rosasio::Publisher<MsgType>::publish(m_latest_message);
            }
        }

        rosasio::Timer m_autorepeat_timer;
        boost::asio::steady_timer m_throttle_timer;
        MsgType m_latest_message;
        bool m_initial_message_published;
        bool m_dirty;
        bool m_holdoff;
        std::chrono::milliseconds m_throttle_period;
    };
}
