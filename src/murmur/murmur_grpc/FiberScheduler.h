// Copyright 2005-2020 The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

#ifndef MUMBLE_MURMUR_GRPC_SCHEDULE_H_
#define MUMBLE_MURMUR_GRPC_SCHEDULE_H_

#include <boost/fiber/algo/algorithm.hpp>
#include <boost/fiber/context.hpp>
#include <boost/fiber/properties.hpp>
#include <boost/fiber/scheduler.hpp>

#include <atomic>
#include <deque>
#include <mutex>

namespace MurmurRPC {
namespace Scheduler {

namespace bf = boost::fibers;

class grpc_props: public bf::fiber_properties {

	private:
		std::atomic<bool> is_main;

	public:
		grpc_props(bf::context* ctx) noexcept : bf::fiber_properties(ctx), is_main(false) { }

		bool run_in_main() noexcept { return is_main.load(); }

		void run_in_main(bool b) noexcept {
			auto p = is_main.exchange(b);
			if (p != b) {
				notify();
			}
		}

		virtual ~grpc_props() = default;
};

class grpc_scheduler : public bf::algo::algorithm_with_properties<grpc_props> {
private:
	typedef bf::scheduler::ready_queue_type rqueue_t;
	typedef std::deque<bf::context *> xferqueue_t;

	rqueue_t m_readyQueue{};
	std::mutex m_rqMtx{};
	std::condition_variable m_cnd{};
	bool flag{false};

	bool is_loop_thread;
	static xferqueue_t xferQueue;
	static std::mutex m_xferMtx;
	static std::function<void()> alertMain;


public:
	grpc_scheduler(bool is_event_loop = false);

	grpc_scheduler(grpc_scheduler const&) = delete;
	grpc_scheduler(grpc_scheduler && ) = delete;

	grpc_scheduler& operator=(grpc_scheduler const&) = delete;
	grpc_scheduler& operator=(grpc_scheduler &&) = delete;

	void awakened(bf::context *, grpc_props &) noexcept override;

	bf::context* pick_next() noexcept override;

	bool has_ready_fibers() const noexcept override;

	void suspend_until(std::chrono::steady_clock::time_point const&) noexcept override;

	void notify() noexcept override;

	void property_change(bf::context*, grpc_props&) noexcept override;
};

}}

#endif
