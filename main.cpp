#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <cstring>
#include <pico/stdlib.h>
#include <hardware/exception.h>
#include <hardware/structs/systick.h>
#include "src/piko.hpp"

constexpr uint8_t PERF_PIN = 2;

void hardfault_handler();

void task1() {
    static uint64_t last_time;

    while (true) {
        asm volatile("cpsid i");
        uint64_t time = get_absolute_time();
        printf("%f: Task 1 says hi!\n", (time - last_time) / 1e6);
        last_time = time;
        asm volatile("cpsie i");

        piko::sleep(500);
    }
}

void task2() {
    static uint64_t last_time;

    while (true) {
        asm volatile("cpsid i");
        uint64_t time = get_absolute_time();
        printf("%f: Task 2 says hi!\n", (time - last_time) / 1e6);
        last_time = time;
        asm volatile("cpsie i");

        piko::sleep(500);
    }
}

void task3() {
    static uint64_t last_time;

    while (true) {
        asm volatile("cpsid i");
        uint64_t time = get_absolute_time();
        printf("%f: Task 3 says hi!\n", (time - last_time) / 1e6);
        last_time = time;
        asm volatile("cpsie i");

        piko::sleep(500);
    }
}

int main() {
    // Set up SysTick timer
    systick_hw->csr = 1 << 2; // Set SysTick source to clk_sys
    systick_hw->rvr = 0xFFFFFF; // Set reload value to 2^24 (max)
    systick_hw->csr |= 1 << 0; // Enable SysTick

    stdio_init_all();

    exception_set_exclusive_handler(HARDFAULT_EXCEPTION, hardfault_handler);

    gpio_init(PERF_PIN);
    gpio_set_dir(PERF_PIN, GPIO_OUT);

    auto& scheduler = piko::Scheduler::get_current();

    scheduler.add_task(task1, "task1");
    scheduler.add_task(task2, "task2");
    scheduler.add_task(task3, "task3");

    scheduler.start();

    printf("you shouldn't ever see this\n");

    while (1) {
        sleep_ms(1000);
    }

    return 0;
}

void hardfault_handler() {
    printf("Hardfault!\n");

    while (1) {
        __breakpoint();
    }
}