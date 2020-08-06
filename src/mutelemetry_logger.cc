#include "mutelemetry/mutelemetry_logger.h"

#include <glog/logging.h>
#include <muqueue/erqperiodic.h>
#include <cassert>
#include <thread>

//#define CHECK_PARSE_VALIDITY_LOGGER

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
  post_function<void>([dbp, do_flush, this](void) -> void {
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
  });
}

void MutelemetryLogger::main_loop() {
  if (!running_) return;

  while (!data_queue_->empty()) {
#if 0
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

    DataBuffer *io_idx = curr_idx_;
    bool is_full = io_idx->add(dp);
    if (is_full) {
      if (!idx_mutex_.try_lock()) continue;
      if (pool_stacked_index_.size() == 0) {
        idx_mutex_.unlock();
        return;
      }

      bool result =
          curr_idx_.compare_exchange_weak(io_idx, pool_stacked_index_.top());
      if (!result) {
        idx_mutex_.unlock();
        assert(0);
        return;
      }
      assert(curr_idx_.load()->size() == 0);
      pool_stacked_index_.pop();
      idx_mutex_.unlock();

      start_io_worker(io_idx);
    }
#else
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
    io_idx->add(dp);
#endif
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

void MutelemetryLogger::start() {
  add_periodic<void>(([&](void) -> void { main_loop(); }), 0.000001, 0.1);
  running_ = true;
}
