#include "mutelemetry/mutelemetry_logger.h"

#include <glog/logging.h>
#include <muqueue/erqperiodic.h>
#include <cassert>
#include <thread>

//#define TEST_PARSE_VALIDITY

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
    if (!on_file_error && curr_idx_->has_data()) {
      start_io_worker(curr_idx_, true);
      curr_idx_ = nullptr;
    }
    file_.close();
  }
  data_queue_ = nullptr;
}

void MutelemetryLogger::start_io_worker(DataBuffer *dbp, bool do_flush) {
  post_function<void>([dbp, do_flush, this](void) -> void {
    SerializedData result = dbp->data();
    {
      // WARNING: why do we need it if post_function calls are serialized?
      std::lock_guard<std::mutex> lock(mutex_);
      file_.write(reinterpret_cast<const char *>(result.data()), result.size());
    }
    if (file_.bad()) {
      release(true);
      LOG(ERROR) << "Error writing to " << filename_ << endl;
    } else if (do_flush)
      flush();
    dbp->clear();
    pool_stacked_index_.push(dbp);
  });
}

void MutelemetryLogger::start() {
  add_periodic<void>(([&](void) -> void {
                       while (running_ && !data_queue_->empty()) {
                         auto dp = data_queue_->dequeue();
                         assert(dp != nullptr);
                         assert(dp->size() <= DataBuffer::max_data_size_);

#ifdef TEST_PARSE_VALIDITY
                         const uint8_t *buffer = dp->data();
                         if (!check_ulog_valid(buffer)) {
                           LOG(ERROR) << "Validity check failed\n";
                           assert(0);
                         }
#endif

                         bool is_full = curr_idx_->add(dp);
                         if (is_full) {
                           start_io_worker(curr_idx_);
                           curr_idx_ = pool_stacked_index_.pop();
                           assert(curr_idx_->size() == 0);
                         }
                       }

                       if (curr_idx_->can_start_io()) {
                         start_io_worker(curr_idx_, true);
                         curr_idx_ = pool_stacked_index_.pop();
                         assert(curr_idx_->size() == 0);
                       }
                     }),
                     0.000001, 0.1);

  running_ = true;
}
