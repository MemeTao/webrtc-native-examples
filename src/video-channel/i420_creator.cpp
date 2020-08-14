#include "i420_creator.h"
#include <future>
#include <chrono>

I420Creator::~I420Creator()
{
    running_ = false;
    if(thread_.joinable()){
        thread_.join();
    }
}

void I420Creator::run(int fps)
{
    if(running_ || fps == 0) {
        return;
    }
    running_ = true;
    std::promise<bool> promise;
    auto future = promise.get_future();
    thread_ = std::thread([this, fps, &promise]()
    {
        promise.set_value(true);
        while(running_) {
            auto duration_ms = 1000 / fps;
            using namespace std::chrono_literals;
            std::this_thread::sleep_for(1ms * duration_ms);
            if(observer_) {
                observer_(process());
            }
        }
    });
    future.wait();
}

I420Creator::I420Frame I420Creator::process()
{
    return I420Frame();
}
