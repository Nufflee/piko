#pragma once

#include <cstdint>
#include <string_view>
#include <stdio.h>
#include <pico/stdlib.h>
#include <hardware/structs/scb.h>

namespace piko {

// -- Config --
constexpr uint32_t TASK_CAPACITY = 4;
constexpr uint32_t TASK_TICK_INTERVAL_US = 1 * 1000;
constexpr uint32_t TASK_STACK_SIZE = 1024;

constexpr uint32_t CORE_COUNT = 2;

constexpr uint32_t ALARM_NUM = 0;
constexpr uint32_t ALARM_IRQ = TIMER_IRQ_0 + ALARM_NUM;
// -- Config --

struct Task {
    uint32_t* sp;
    std::string_view name;
    uint32_t sleep_ticks;
};

class Scheduler {
private:
    Task tasks[TASK_CAPACITY];
    uint32_t task_count = 0;
    uint32_t current_task = 0;

public:
    Scheduler();

    /// @brief Add a task to the task scheduler
    /// @param func The task function
    /// @param name The name of the task (used for debugging)
    void add_task(void (*func)(), std::string_view name);

    /// @brief Start the task scheduler and run the first task
    void start();

    /// @brief Sleep for a given number of scheduler ticks and switch to a different task in the meantime
    /// @param ticks Number of ticks to sleep for.
    void sleep(uint32_t ticks);

    __attribute__((always_inline)) inline void yield() {
        // Trigger a PendSV interrupt
        hw_set_bits(&scb_hw->icsr, M0PLUS_ICSR_PENDSVSET_BITS);
    }

    /// @brief Get the scheduler for the current executing core. There is one scheduler per core
    /// @return The scheduler for the current core
    static Scheduler& get_current();
private:
    Task& select_next_task();

    void tick();

    static void idle_task();

    static void alarm_isr();
    static void task_switch_isr();
};

__attribute__((always_inline)) inline void sleep(uint32_t ticks) {
    auto& scheduler = Scheduler::get_current();
    // - 1 is a hack to account for the time yield takes to run
    scheduler.sleep(ticks - 1);
    scheduler.yield();
}

// TODO: implement sleep_ms

}