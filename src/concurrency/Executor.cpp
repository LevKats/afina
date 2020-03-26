#include <afina/concurrency/Executor.h>

namespace Afina {
namespace Concurrency {

Executor::Executor(uint32_t low_watermark, uint32_t hight_watermark, uint32_t max_queue_size, uint32_t idle_time)
    : low_watermark(low_watermark), hight_watermark(hight_watermark), max_queue_size(max_queue_size),
      idle_time(idle_time) {}

Executor::~Executor() { Stop(true); }

void Executor::Stop(bool await = false) {
    std::unique_lock<std::mutex> lock(_queue_modify);
    state = State::kStopping;
    if (await) {
        while (state != State::kStopped) {
            had_finished.wait(lock);
        }
    }
}

void Executor::Start() {
    {
        std::unique_lock<std::mutex> lock(_queue_modify);
        if (state == State::kRun) {
            return;
        }
        state = State::kRun;
    }
    for (uint32_t i = 0; i != low_watermark; ++i) {
        std::unique_lock<std::mutex> lock(_queue_modify);
        std::thread th(&Executor::perform, this);
        th.detach();
        ++_current_workers;
    }
}

void Executor::perform() {
    while (true) {
        std::function<void()> task;
        {
            std::unique_lock<std::mutex> lock(_queue_modify);
            if (!tasks.empty()) {
                task = tasks.front();
                tasks.pop_front();
            } else if (tasks.empty() && state == State::kStopping) {
                --_current_workers;
                if (!_current_workers) {
                    state = State::kStopped;
                    had_finished.notify_all();
                }
                return;
            } else {
                while (tasks.empty()) {
                    if (empty_condition.wait_for(lock, std::chrono::milliseconds(idle_time)) ==
                        std::cv_status::timeout) {
                        if (_current_workers > low_watermark) {
                            --_current_workers;
                            return;
                        }
                    }
                }
                task = tasks.front();
                tasks.pop_front();
            }
        }
        {
            std::unique_lock<std::mutex> lock(_queue_modify);
            if (!tasks.empty() && state == State::kRun && _current_workers < hight_watermark) {
                ++_current_workers;
                std::thread th(&Executor::perform, this);
                th.detach();
            }
        }
        task();
    }
}

} // namespace Concurrency
} // namespace Afina
