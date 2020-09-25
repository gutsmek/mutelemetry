#include "mutelemetry/mutelemetry_logger.h"

#include <glog/logging.h>
#include <muqueue/erqperiodic.h>
#include <cassert>
#include <thread>

//#define CHECK_PARSE_VALIDITY_LOGGER
//#define USE_POST_FUNCTION_ON_IO

using namespace std;
using namespace fflow;
using namespace mutelemetry_logger;
using namespace mutelemetry_tools;

MutelemetryLogger::MutelemetryLogger()
    : running_(false), data_queue_(nullptr), filename_("") {
  for (size_t i = 1; i < mem_pool_.size(); ++i)
    pool_stacked_index_.push(&mem_pool_[i]);
  curr_idx_ = &mem_pool_[0];
}

bool MutelemetryLogger::init(const std::string &filename,
                             ConcQueue<SerializedDataPtr> *data_queue) {
  if (running_ || data_queue_ != nullptr) return false;
  if (data_queue == nullptr) return false;

  bool res = false;
  filename_ = filename;
  file_ = ofstream(filename_, ios::binary);
  if (file_.is_open()) {
    assert(data_queue);
    data_queue_ = data_queue;
    res = true;
  }

  return res;
}

void MutelemetryLogger::release(bool on_file_error) {
  if (file_.is_open()) {
    DataBuffer *curr_idx = curr_idx_;
    if (!on_file_error && curr_idx->has_data()) {
      start_io_worker(curr_idx, true);
      curr_idx_ = nullptr;
    }
    file_.close();
  }
  data_queue_ = nullptr;
}

void MutelemetryLogger::start_io_worker(DataBuffer *dbp, bool do_flush) {
#ifdef USE_POST_FUNCTION_ON_IO
  post_function<void>([dbp, do_flush, this](void) -> void {
#endif
    SerializedData result = dbp->data();
    dbp->clear();
    {
      std::lock_guard<std::mutex> lock(idx_mutex_);
      pool_stacked_index_.push(dbp);
    }

    if (!write(result)) {
      release(true);
      LOG(ERROR) << "Error writing to " << filename_ << endl;
    } else if (do_flush)
      flush();
#ifdef USE_POST_FUNCTION_ON_IO
  });
#endif
}

void MutelemetryLogger::main_loop() {
  if (!running_) return;

  while (!data_queue_->empty()) {
    DataBuffer *io_idx = curr_idx_;
    if (io_idx->full()) {
      if (!idx_mutex_.try_lock()) continue;
      if (pool_stacked_index_.size() == 0) {
        idx_mutex_.unlock();
        return;
      }
      if (!curr_idx_.compare_exchange_weak(io_idx, pool_stacked_index_.top())) {
        idx_mutex_.unlock();
        return;
      }
      pool_stacked_index_.pop();
      assert(curr_idx_.load()->size() == 0);
      idx_mutex_.unlock();

      start_io_worker(io_idx);
      io_idx = curr_idx_;
    }

    auto dp = data_queue_->dequeue();
    assert(dp != nullptr);
    assert(dp->size() <= DataBuffer::max_data_size_);

#ifdef CHECK_PARSE_VALIDITY_LOGGER
    const uint8_t *buffer = dp->data();
    if (!check_ulog_valid(buffer)) {
      LOG(ERROR) << "Validity check failed";
      assert(0);
    }
#endif

    io_idx->add(dp);
  } /* end while */

  DataBuffer *io_idx = curr_idx_;
  if (io_idx->can_start_io()) {
    if (!idx_mutex_.try_lock()) return;
    if (pool_stacked_index_.size() == 0) {
      idx_mutex_.unlock();
      return;
    }

    bool result =
        curr_idx_.compare_exchange_weak(io_idx, pool_stacked_index_.top());
    if (!result) {
      idx_mutex_.unlock();
      return;
    }
    pool_stacked_index_.pop();
    assert(curr_idx_.load()->size() == 0);
    idx_mutex_.unlock();

    start_io_worker(io_idx, true);
  }
}

void MutelemetryLogger::main_loop2() {
  while (true) {
    DataBuffer *io_idx = curr_idx_;

    if (data_queue_->empty()) {
      if (io_idx->can_start_io()) {
        if (!idx_mutex_.try_lock()) {
          this_thread::sleep_for(chrono::milliseconds(100));
          continue;
        }

        if (pool_stacked_index_.size() == 0) {
          idx_mutex_.unlock();
          this_thread::sleep_for(chrono::milliseconds(100));
          continue;
        }

        if (!curr_idx_.compare_exchange_weak(io_idx,
                                             pool_stacked_index_.top())) {
          idx_mutex_.unlock();
          this_thread::sleep_for(chrono::milliseconds(100));
          continue;
        }
        pool_stacked_index_.pop();

        assert(curr_idx_.load()->size() == 0);
        idx_mutex_.unlock();

        start_io_worker(io_idx, true);
      } else
        this_thread::sleep_for(chrono::milliseconds(100));

      continue;
    }

    if (io_idx->full()) {
      if (!idx_mutex_.try_lock()) {
        this_thread::sleep_for(chrono::milliseconds(100));
        continue;
      }

      if (pool_stacked_index_.size() == 0) {
        idx_mutex_.unlock();
        this_thread::sleep_for(chrono::milliseconds(100));
        continue;
      }

      if (!curr_idx_.compare_exchange_weak(io_idx, pool_stacked_index_.top())) {
        idx_mutex_.unlock();
        this_thread::sleep_for(chrono::milliseconds(100));
        continue;
      }
      pool_stacked_index_.pop();

      assert(curr_idx_.load()->size() == 0);
      idx_mutex_.unlock();

      start_io_worker(io_idx);
      io_idx = curr_idx_;
    }

    auto dp = data_queue_->dequeue();
    assert(dp != nullptr);
    assert(dp->size() <= DataBuffer::max_data_size_);

#ifdef CHECK_PARSE_VALIDITY_LOGGER
    const uint8_t *buffer = dp->data();
    if (!check_ulog_valid(buffer)) {
      LOG(ERROR) << "Validity check failed";
      assert(0);
    }
#endif

    io_idx->add(dp);

  } /* end while */
}

void MutelemetryLogger::start() {
#if 1
  add_periodic<void>(([&](void) -> void { main_loop(); }), 0.000001, 0.1);
#else
  post_function<void>(([&](void) -> void { main_loop2(); }));
#endif
  running_ = true;
}
