#ifndef AFINA_CONCURRENCY_EXECUTOR_H
#define AFINA_CONCURRENCY_EXECUTOR_H

#include <cassert>
#include <condition_variable>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>

namespace Afina {
namespace Concurrency {

/**
 * # Thread pool
 */
class Executor {
public:
    enum class State {
        // Threadpool is fully operational, tasks could be added and get executed
        kRun,

        // Threadpool is on the way to be shutdown, no ned task could be added, but existing will be
        // completed as requested
        kStopping,

        // Threadppol is stopped
        kStopped
    };
    Executor(uint32_t low_watermark, uint32_t hight_watermark, uint32_t max_queue_size, uint32_t idle_time)
        : low_watermark(low_watermark), hight_watermark(hight_watermark), max_queue_size(max_queue_size),
          idle_time(idle_time) {}

    ~Executor() {
        assert(low_watermark < hight_watermark && low_watermark > 0);
        Stop(true);
    }

    /**
     * Signal thread pool to stop, it will stop accepting new jobs and close threads just after each become
     * free. All enqueued jobs will be complete.
     *
     * In case if await flag is true, call won't return until all background jobs are done and all threads are stopped
     */
    void Stop(bool await = false) {
        std::unique_lock<std::mutex> lock(_queue_modify);
        if (state == State::kStopped) {
            return;
        }
        state = State::kStopping;
        if (tasks.empty()) {
            empty_condition.notify_all();
        }
        if (await) {
            while (state != State::kStopped) {
                had_finished.wait(lock);
            }
        }
    }

    /**
     * Signal thread pool to start. It will start low_watermark of new threads and change state to kRun
     */
    void Start() {
        {
            std::unique_lock<std::mutex> lock(_queue_modify);
            if (state == State::kRun) {
                return;
            }
            state = State::kRun;
            _current_workers = 0;
        }
        //_logger->debug("Threadpool watermarks is {} {}", low_watermark, hight_watermark);
        for (uint32_t i = 0; i != low_watermark; ++i) {
            std::unique_lock<std::mutex> lock(_queue_modify);
            std::thread th(&Executor::perform, this);
            //_logger->debug("Creating thread {}/{}, id {}", i + 1, low_watermark, th.get_id());
            th.detach();
            ++_current_workers;
        }
    }

    /**
     * Add function to be executed on the threadpool. Method returns true in case if task has been placed
     * onto execution queue, i.e scheduled for execution and false otherwise.
     *
     * That function doesn't wait for function result. Function could always be written in a way to notify caller about
     * execution finished by itself
     */
    template <typename F, typename... Types> bool Execute(F &&func, Types... args) {
        // Prepare "task"
        auto exec = std::bind(std::forward<F>(func), std::forward<Types>(args)...);

        std::unique_lock<std::mutex> lock(_queue_modify);
        if (state != State::kRun) {
            return false;
        }

        // Enqueue new task
        if (tasks.size() > max_queue_size) {
            return false;
        }
        tasks.push_back(exec);
        empty_condition.notify_one();
        return true;
    }

private:
    // No copy/move/assign allowed
    Executor(const Executor &) = delete;
    Executor(Executor &&) = delete;
    Executor &operator=(const Executor &) = delete;
    Executor &operator=(Executor &&) = delete;

    /**
     * Method that all pool threads are running. It polls internal task queue and execute tasks
     */
    void perform() {
        while (true) {
            std::function<void()> task;
            {
                std::unique_lock<std::mutex> lock(_queue_modify);
                if (!tasks.empty()) {
                    task = tasks.front();
                    tasks.pop_front();
                    //_logger->debug("Thread id {} has got task", std::this_thread::get_id());
                } else if (tasks.empty() && state == State::kStopping) {
                    --_current_workers;
                    empty_condition.notify_all();
                    if (!_current_workers) {
                        state = State::kStopped;
                        had_finished.notify_all();
                    }
                    return;
                } else {
                    while (tasks.empty()) {
                        auto now = std::chrono::system_clock::now();
                        while (tasks.empty()) {
                            if (empty_condition.wait_until(lock, now + std::chrono::milliseconds(idle_time)) ==
                                std::cv_status::timeout) {
                                if (_current_workers > low_watermark) {
                                    --_current_workers;
                                    //_logger->debug("Thread with id {} has been killed", std::this_thread::get_id());
                                    return;
                                }
                            }
                            if (tasks.empty() && state == State::kStopping) {
                                --_current_workers;
                                if (!_current_workers) {
                                    state = State::kStopped;
                                    had_finished.notify_all();
                                }
                                //_logger->debug("Thread with id {} has been killed", std::this_thread::get_id());
                                return;
                            }
                        }
                    }
                    task = tasks.front();
                    tasks.pop_front();
                    //_logger->debug("Thread with id {} has got task after wait", std::this_thread::get_id());
                }
            }
            {
                std::unique_lock<std::mutex> lock(_queue_modify);
                if (!tasks.empty() && state == State::kRun && _current_workers < hight_watermark) {
                    ++_current_workers;
                    std::thread th(&Executor::perform, this);
                    //_logger->debug("Creating thread with id {}", th.get_id());
                    th.detach();
                }
            }
            // std::cout << "task execution" << std::endl;
            //_logger->debug("Thread id {} is processing task", std::this_thread::get_id());
            task();
        }
    }

    /**
     * Mutex to protect state and tasks below from concurrent modification
     */
    std::mutex _queue_modify;

    /**
     * Conditional variable to await new data in case of empty queue
     */
    std::condition_variable empty_condition;

    uint32_t _current_workers;
    uint32_t low_watermark;
    uint32_t hight_watermark;
    uint32_t max_queue_size;
    uint32_t idle_time;

    /**
     * Conditional variable to inform thread with Stop function that all work has been done
     */
    std::condition_variable had_finished;

    /**
     * Vector of actual threads that perorm execution
     */
    std::vector<std::thread> threads;

    /**
     * Task queue
     */
    std::deque<std::function<void()>> tasks;

    /**
     * Flag to stop bg threads
     */
    State state;

    // std::shared_ptr<Logging::Service> _logger;
};

} // namespace Concurrency
} // namespace Afina

#endif // AFINA_CONCURRENCY_EXECUTOR_H
