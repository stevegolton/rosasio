#include <boost/asio.hpp>

namespace rosasio
{
    class Timer
    {
    public:
        Timer(const std::chrono::milliseconds &interval, std::function<void()> cb, boost::asio::io_context &ioc)
            : m_interval(interval),
              m_timer(ioc, interval),
              m_cb(cb)
        {
            m_timer.async_wait(std::bind(&Timer::timer_handler, this, std::placeholders::_1));
        }

    private:
        void timer_handler(boost::system::error_code ec)
        {
            if (ec)
                return;

            m_timer.expires_at(m_timer.expiry() + m_interval);
            m_timer.async_wait(std::bind(&Timer::timer_handler, this, std::placeholders::_1));

            m_cb();
        }

        std::chrono::milliseconds m_interval;
        boost::asio::steady_timer m_timer;
        std::function<void()> m_cb;
    };
} // namespace rosasio
