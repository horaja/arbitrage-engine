/**
 * @file spsc_queue.h
 * @brief Single-Producer-Single-Consumer lock-free queue optimized for low latency.
 *
 * @details
 * This is a high-performance, wait-free queue designed for a single producer thread
 * and a single consumer thread. It outperforms MPMC (Multi-Producer-Multi-Consumer)
 * queues for this use case by eliminating unnecessary synchronization overhead.
 *
 * Key optimizations:
 * - Cache-line aligned head/tail to prevent false sharing
 * - Power-of-2 size for fast modulo operations (bitwise AND instead of %)
 * - Lock-free using C++11 atomics with acquire/release semantics
 * - Minimal memory ordering constraints for maximum performance
 *
 * Target: 30% lower latency than BlockingConcurrentQueue for SPSC workloads
 */

#pragma once

#include <atomic>
#include <cstddef>
#include <memory>
#include <optional>
#include <thread>
#include <cassert>

// Define macro required by lightweightsemaphore.h
// This is normally provided by concurrentqueue.h, but we're using the semaphore standalone
#ifndef MOODYCAMEL_DELETE_FUNCTION
#define MOODYCAMEL_DELETE_FUNCTION = delete
#endif

#include "lightweightsemaphore.h"

/**
 * @class SPSCQueue
 * @brief Lock-free single-producer-single-consumer bounded queue.
 * @tparam T The type of elements stored in the queue.
 */
template<typename T>
class SPSCQueue {
public:
    /**
     * @brief Constructs an SPSC queue with the specified capacity.
     * @param capacity The maximum number of elements (must be power of 2).
     */
    explicit SPSCQueue(size_t capacity = 4096)
        : capacity_(round_up_power_of_2(capacity))
        , mask_(capacity_ - 1)
        , buffer_(new Slot[capacity_])
        , head_(0)
        , tail_(0) {
    }

    ~SPSCQueue() = default;

    // Non-copyable, non-movable
    SPSCQueue(const SPSCQueue&) = delete;
    SPSCQueue& operator=(const SPSCQueue&) = delete;

    /**
     * @brief Attempts to enqueue an element (non-blocking).
     * @param item The item to enqueue.
     * @return true if successful, false if queue is full.
     */
    bool try_enqueue(const T& item) {
        return enqueue_impl(item);
    }

    /**
     * @brief Attempts to enqueue an element (non-blocking, move version).
     * @param item The item to enqueue.
     * @return true if successful, false if queue is full.
     */
    bool try_enqueue(T&& item) {
        return enqueue_impl(std::move(item));
    }

    /**
     * @brief Enqueues an element (blocking version, always succeeds).
     *
     * This is for API compatibility with BlockingConcurrentQueue.
     * In practice, the queue should be sized large enough that this never blocks.
     * If the queue is full, it spins until space is available.
     *
     * @param item The item to enqueue.
     */
    void enqueue(const T& item) {
        while (!try_enqueue(item)) {
            // Spin if full (should be rare with proper sizing)
            std::this_thread::yield();
        }
        semaphore_.signal();
    }

    /**
     * @brief Enqueues an element (blocking version, move semantics).
     * @param item The item to enqueue.
     */
    void enqueue(T&& item) {
        while (!try_enqueue(std::move(item))) {
            std::this_thread::yield();
        }
        semaphore_.signal();
    }

    /**
     * @brief Attempts to dequeue an element (non-blocking).
     * @param item Reference to store the dequeued item.
     * @return true if successful, false if queue is empty.
     */
    bool try_dequeue(T& item) {
        const size_t current_tail = tail_.load(std::memory_order_relaxed);
        const size_t current_head = head_.load(std::memory_order_acquire);

        if (current_tail == current_head) {
            // Queue is empty
            return false;
        }

        Slot& slot = buffer_[current_tail & mask_];
        item = std::move(slot.value);

        // Update tail with release semantics so producer sees the change
        tail_.store(current_tail + 1, std::memory_order_release);

        return true;
    }

    /**
     * @brief Dequeues an element, blocking until one is available.
     *
     * This is the primary method for the consumer thread. It waits efficiently
     * using a semaphore if the queue is empty.
     *
     * @param item Reference to store the dequeued item.
     */
    void wait_dequeue(T& item) {
        // Fast path: try to dequeue immediately
        if (try_dequeue(item)) {
            return;
        }

        // Slow path: wait on semaphore until data is available
        while (true) {
            semaphore_.wait();
            if (try_dequeue(item)) {
                return;
            }
            // Spurious wakeup, try again
        }
    }

    /**
     * @brief Returns the number of elements currently in the queue (approximate).
     * @return The approximate size of the queue.
     */
    size_t size_approx() const {
        const size_t current_head = head_.load(std::memory_order_relaxed);
        const size_t current_tail = tail_.load(std::memory_order_relaxed);
        return current_head - current_tail;
    }

private:
    /**
     * @brief Storage slot for queue elements.
     */
    struct Slot {
        T value;
    };

    /**
     * @brief Rounds up to the nearest power of 2.
     * @param n The input number.
     * @return The smallest power of 2 >= n.
     */
    static size_t round_up_power_of_2(size_t n) {
        if (n == 0) return 1;
        n--;
        n |= n >> 1;
        n |= n >> 2;
        n |= n >> 4;
        n |= n >> 8;
        n |= n >> 16;
        n |= n >> 32;
        return n + 1;
    }

    /**
     * @brief Internal enqueue implementation.
     * @tparam U Forwarding reference type.
     * @param item The item to enqueue.
     * @return true if successful, false if full.
     */
    template<typename U>
    bool enqueue_impl(U&& item) {
        const size_t current_head = head_.load(std::memory_order_relaxed);
        const size_t next_head = current_head + 1;
        const size_t current_tail = tail_.load(std::memory_order_acquire);

        if (next_head - current_tail > capacity_) {
            // Queue is full
            return false;
        }

        Slot& slot = buffer_[current_head & mask_];
        slot.value = std::forward<U>(item);

        // Update head with release semantics so consumer sees the new item
        head_.store(next_head, std::memory_order_release);

        return true;
    }

    // Queue configuration
    const size_t capacity_;
    const size_t mask_;
    std::unique_ptr<Slot[]> buffer_;

    // Cache-line alignment to prevent false sharing between producer and consumer
    alignas(64) std::atomic<size_t> head_;  // Producer updates this
    alignas(64) std::atomic<size_t> tail_;  // Consumer updates this

    // Semaphore for blocking wait_dequeue
    moodycamel::LightweightSemaphore semaphore_;
};
