/**
 * @file pipeline.hpp
 *
 * An implementation of pipeline design pattern.
 * Each pipeline stage is responsible for processing one pipeline buffer
 * and once processing of such buffer is finished,
 * buffer is passed to the next pipeline stage for further processing and so on.
 *
 * @author Lukasz Wiecaszek <lukasz.wiecaszek@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 */

#ifndef _PIPELINE_
#define _PIPELINE_

/*===========================================================================*\
 * system header files
\*===========================================================================*/
#include <memory>
#include <functional>
#include <thread>
#include <atomic>

/*===========================================================================*\
 * project header files
\*===========================================================================*/
#include "semaphore.hpp"
#include "ringbuffer.hpp"

/*===========================================================================*\
 * preprocessor #define constants and macros
\*===========================================================================*/

/*===========================================================================*\
 * global type definitions
\*===========================================================================*/
namespace ymn
{

} /* end of namespace ymn */

/*===========================================================================*\
 * inline function/variable definitions
\*===========================================================================*/
namespace ymn
{

class pipeline
{
public:
    /* All pipeline buffers must extend this tagging type */
    struct buffer
    {
        virtual ~buffer() = default;
    };

    using buffer_uptr = std::unique_ptr<buffer>;

    using stage_function = std::function<bool(iringbuffer<buffer_uptr>* irb, oringbuffer<buffer_uptr>* orb)>;

    template<std::size_t N>
    explicit pipeline(stage_function (&f)[N], std::size_t queue_capacity) :
       m_size{N},
       m_stages{std::make_unique<std::unique_ptr<stage_exec_env>[]>(N)},
       m_ringbuffers{},
       m_running{false}
    {
        if (N > 1) {
            m_ringbuffers = std::make_unique<std::unique_ptr<ringbuffer<buffer_uptr>>[]>(N - 1);
            for (std::size_t n = 0; n < (N - 1); ++n)
                m_ringbuffers[n] = std::make_unique<ringbuffer<buffer_uptr>>(
                    queue_capacity, RINGBUFFER_RD_BLOCKING_WR_NONBLOCKING);
        }

        for (std::size_t n = 0; n < N; ++n) {
            m_stages[n] = std::make_unique<stage_exec_env>(*this, f[n]);
            if (N > 1) {
                if (n == 0)
                    m_stages[n]->set_ringbuffers(
                        static_cast<iringbuffer<buffer_uptr>*>(nullptr),
                        static_cast<oringbuffer<buffer_uptr>*>(m_ringbuffers[n].get()));
                else
                if (n == (N - 1))
                    m_stages[n]->set_ringbuffers(
                        static_cast<iringbuffer<buffer_uptr>*>(m_ringbuffers[n - 1].get()),
                        static_cast<oringbuffer<buffer_uptr>*>(nullptr));
                else
                    m_stages[n]->set_ringbuffers(
                        static_cast<iringbuffer<buffer_uptr>*>(m_ringbuffers[n - 1].get()),
                        static_cast<oringbuffer<buffer_uptr>*>(m_ringbuffers[n].get()));
            }
        }
    }

    void start()
    {
        m_running = true;
        for (std::size_t n = 0; n < m_size; ++n)
            m_stages[n]->post();
    }

    void stop()
    {
        m_running = false;
        if (m_size > 1) {
            for (std::size_t n = 0; n < (m_size - 1); ++n)
                m_ringbuffers[n]->cancel(ymn::ringbuffer_role::CONSUMER);
        }
    }

    void join()
    {
        for (std::size_t n = 0; n < m_size; ++n) {
            m_stages[n]->join();
        }
    }

private:
    struct stage_exec_env
    {
        stage_exec_env(const pipeline& pipeline, const stage_function& function) :
            m_pipeline{pipeline},
            m_function{function},
            m_irb{nullptr},
            m_orb{nullptr},
            m_semaphore{0},
            m_thread{&stage_exec_env::run, this}
        {
        }

        void set_ringbuffers(iringbuffer<buffer_uptr>* irb, oringbuffer<buffer_uptr>* orb)
        {
            m_irb = irb;
            m_orb = orb;
        }

        void post() const
        {
            m_semaphore.post();
        }

        void join()
        {
            if (m_thread.joinable())
                m_thread.join();
        }

    private:
        void run() const
        {
            m_semaphore.wait();
            while ((m_pipeline.m_running) && (m_function(m_irb, m_orb) == true));
        }

        const pipeline& m_pipeline;
        stage_function m_function;
        iringbuffer<buffer_uptr>* m_irb;
        oringbuffer<buffer_uptr>* m_orb;
        mutable semaphore m_semaphore;
        std::thread m_thread;
    };

    std::size_t m_size;
    std::unique_ptr<std::unique_ptr<stage_exec_env>[]> m_stages;
    std::unique_ptr<std::unique_ptr<ringbuffer<buffer_uptr>>[]> m_ringbuffers;
    std::atomic<bool> m_running;
};

} /* end of namespace ymn */

/*===========================================================================*\
 * global object declarations
\*===========================================================================*/
namespace ymn
{

} /* end of namespace ymn */

/*===========================================================================*\
 * function forward declarations
\*===========================================================================*/
namespace ymn
{

} /* end of namespace ymn */

#endif /* _PIPELINE_ */
