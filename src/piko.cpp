#include "piko.hpp"
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <string>
#include <hardware/exception.h>

namespace piko {

constexpr uint8_t OFFSET_PC = 14;
constexpr uint8_t OFFSET_xPSR = 15;

constexpr uint32_t IDLE_TASK_ID = 0;

static uint32_t get_current_core();

static Scheduler schedulers[CORE_COUNT] asm("schedulers");

Scheduler::Scheduler() {
    add_task(idle_task, "idle");
}

void Scheduler::add_task(void (*func)(), std::string_view name)
{
  // ARM requires the stack to be 8-byte aligned on procedure entry. `malloc` returns addresses aligned to
  // max_align_t which then needs to be 8 bytes or greater for the mentioend invariant to hold
  static_assert(sizeof(max_align_t) >= 8, "max_align_t must be >= 8 for the task stack to be aligned properly");

  // TODO: Names don't work??
  auto task = Task {
    .sp = (uint32_t*)malloc(TASK_STACK_SIZE),
    .name = name,
    .sleep_ticks = 0
  };

  // Initialize the stack to all zeros
  memset(task.sp, 0, TASK_STACK_SIZE);

  // The stack is full descending, so set the SP to the top of the stack
  task.sp += TASK_STACK_SIZE / sizeof(uint32_t);

  // Now it gets a little fucky.. We construct a stack frame idential to the one during a
  // task switch interrupt, including the automatically stacked exception frame

  // First, make space for the stack frame
  task.sp -= 16;

  task.sp[OFFSET_PC] = (uint32_t)func;
  // Bit 24 of xPSR has to be set to indicate that we are in Thumb mode
  task.sp[OFFSET_xPSR] = 1 << 24;

  // TODO: Set LR to task return handler (tasks should never return)

  tasks[task_count++] = task;
}

void Scheduler::start() {
    // First, set up the task switch alarm
    if (hardware_alarm_is_claimed(ALARM_NUM)) {
        panic("Alarm %ld is claimed. What the fuck..\n", ALARM_NUM);
    }

    // Enable timer alarm interrupt
    hw_set_bits(&timer_hw->inte, 1 << ALARM_NUM);
    // Set alarm IRQ handler
    irq_set_exclusive_handler(ALARM_IRQ, alarm_isr);
    // Enable alarm IRQ
    irq_set_enabled(ALARM_IRQ, true);
    // Arm the alarm by setting the target time
    timer_hw->alarm[ALARM_NUM] = timer_hw->timerawl + TASK_TICK_INTERVAL_US;

    // piko cannot start if someone else is already using the PendSV interrupt
    assert(exception_is_compile_time_default(exception_get_vtable_handler(PENDSV_EXCEPTION)) && "PendSV handler already set");

    exception_set_exclusive_handler(PENDSV_EXCEPTION, task_switch_isr);

    // Then start the first task in the array
    asm volatile(
        ".syntax unified\n"

        // Load SP from the first task struct
        "ldr r0, %[tasks]\n"
        "mov sp, r0\n"
        // TODO: Task arguments

        // Load PC and jump to it
        "ldr r0, [sp, %[offset_pc] * 4]\n"
        "bx r0\n"
        :: [tasks] "m"(tasks), [offset_pc] "i"(OFFSET_PC)
        : "r0", "pc"
    );
}

void Scheduler::sleep(uint32_t ticks) {
    assert(tasks[current_task].sleep_ticks == 0);

    tasks[current_task].sleep_ticks = ticks;
}

Scheduler& Scheduler::get_current(){
    return schedulers[get_current_core()];
}

Task& Scheduler::select_next_task()
{
    bool task_found = false;

    for (uint32_t i = 0; i < task_count; i++) {
        current_task = current_task + 1;

        if (current_task >= task_count) {
            current_task = 0;
        }

        if (tasks[current_task].sleep_ticks == 0 && current_task != IDLE_TASK_ID) {
            task_found = true;
            break;
        }
    }

    if (!task_found) {
        current_task = IDLE_TASK_ID;
    }

    return tasks[current_task];
}

void Scheduler::tick() {
    // Each tick decrement the number of sleep ticks for each task
    for (uint32_t i = 0; i < task_count; i++) {
        if (tasks[i].sleep_ticks > 0) {
            tasks[i].sleep_ticks -= 1;
        }
    }
}

void Scheduler::idle_task() {
    while (true) {
        asm volatile("wfi");
    }
}

void Scheduler::alarm_isr() {
    // Acknowledge the IRQ, clear the bit
    hw_clear_bits(&timer_hw->intr, 1 << ALARM_NUM);

    // Rearm the alarm to keep going
    timer_hw->alarm[ALARM_NUM] = timer_hw->timerawl + TASK_TICK_INTERVAL_US;

    Scheduler::get_current().tick();

    // Trigger a PendSV interrupt
    hw_set_bits(&scb_hw->icsr, M0PLUS_ICSR_PENDSVSET_BITS);
}

__attribute__((naked)) void Scheduler::task_switch_isr() {
    // The assembly code below assumes these are true
    static_assert(sizeof(Task) <= 255); // ...because mov only works with imm8
    static_assert(sizeof(Scheduler) <= 255); // ...because mov only works with imm8
    static_assert(offsetof(Scheduler, tasks) == 0); // ...because we assume tasks are the first field
    static_assert(offsetof(Task, sp) == 0); // ...and because we assume SP is the first field

    // First, save all registers in their current state by pushing them onto the current task's stack
    // NOTE: The processor pushes r0-r3, r12, LR (r14), PC+1 (r15) and xPSR onto the stack for us, so we don't have to
    // TODO: Clobbering SP is illegal, yet we do it
    asm volatile(
        ".syntax unified\n"
        // Save the remaining low registers
        "push {r4-r7}\n"

        // Save the remaining high registers
        "mov r0, r8\n"
        "mov r1, r9\n"
        "mov r2, r10\n"
        "mov r3, r11\n"
        "push {r0-r3}\n"

        // Save SP in the current task struct
        "ldr r0, =schedulers\n"
        "ldr r1, REG_CPUID\n"
        "ldr r1, [r1]\n"

        "movs r2, %[SCHEDULER_STRUCT_SIZE]\n"
        // No pointer arithmetic in assembly so we have to multiply ourselves..
        "muls r1, r2\n"
        "add r0, r1\n"

        "mov r1, r0\n"
        "adds r1, %[CURRENT_TASK_OFFSET]\n"
        "ldr r1, [r1]\n"
        "movs r2, %[TASK_STRUCT_SIZE]\n"
        // NOTE: It may be worth replacing this mul with a shift on MCUs that don't have single-cycle mul. That would require sizeof(Task) to be a power of 2.
        "muls r1, r2\n"
        "add r0, r1\n"

        // Finally, save the SP into the first field of the struct
        "mov r1, sp\n"
        "str r1, [r0]\n"
        :: [SCHEDULER_STRUCT_SIZE] "i"(sizeof(Scheduler)), [TASK_STRUCT_SIZE] "i"(sizeof(Task)), [CURRENT_TASK_OFFSET] "i"(offsetof(Scheduler, current_task))
        : "r0", "r1", "r2", "r3", "r4", /* "sp", */ "memory"
    );

    // Stack layout
    //   Manually saved:
    //     r8   <- SP
    //     r9
    //     r10
    //     r11  <- SP + 4 * 4
    //     r4
    //     r5
    //     r6
    //     r7   <- SP + 8 * 4
    //   Exception stacked frame:
    //     xPSR
    //     PC+1
    //     LR
    //     r12
    //     r3
    //     r2
    //     r1
    //     r0

    // Clear the PendSV interrupt
    hw_set_bits(&scb_hw->icsr, M0PLUS_ICSR_PENDSVCLR_BITS);

    asm volatile(
        ".syntax unified\n"

        // Load SP from the current task struct
        "ldr r0, [%[current_task_ptr]]\n"
        "mov sp, r0\n"

        "pop {r0-r3}\n"
        "mov r8, r0\n"
        "mov r9, r1\n"
        "mov r10, r2\n"
        "mov r11, r3\n"

        "pop {r4-r7}\n"

        // 0xFFFFFFF9 is a magic return address which will execute the exception return routine and return to Thread_Mode with MSP
        "ldr r0, =0xFFFFFFF9\n"
        "bx r0\n"
        :: [current_task_ptr] "r"(&Scheduler::get_current().select_next_task())
    );

    // Define the used data words
    asm volatile(
        ".syntax unified\n"

        ".align 4\n"
        "REG_CPUID: .word 0xd0000000"
    );
}

/// @brief Get the core executing the current code
/// @return Core identifier (0 or 1)
static uint32_t get_current_core() {
    return sio_hw->cpuid;
}

}