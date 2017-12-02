#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "caffe/backend/device.hpp"

namespace caffe {

Device::Device() {}

void Device::Init() {}

string Device::name() { return "CPU"; }

void Device::MallocMemHost(void** ptr, uint_tp size) {
#ifdef USE_MKL
  *ptr = mkl_malloc(size ? size : 1, 64);
#else
#ifdef _MSC_VER
  *ptr = malloc(((size - 1) / CAFFE_MALLOC_CACHE_ALIGN + 1)
                * CAFFE_MALLOC_CACHE_ALIGN);
#else
  CHECK_EQ(0, posix_memalign(ptr, CAFFE_MALLOC_PAGE_ALIGN,
           ((size - 1) / CAFFE_MALLOC_CACHE_ALIGN + 1)
           * CAFFE_MALLOC_CACHE_ALIGN))
              << "Host memory allocation error of size: "
              << size << " b";
#endif  // _MSC_VER
#endif  // USE_MKL
  CHECK(*ptr) << "host allocation of size " << size << " failed";
}

vptr<void> Device::MallocMemDevice(uint_tp size, void** ptr,
                                               bool zero_copy) {
  return vptr<void>();
}

void Device::FreeMemHost(void* ptr) {
#ifdef USE_MKL
  mkl_free(ptr);
#else
  free(ptr);
#endif  // USE_MKL
}

void Device::FreeMemDevice(vptr<void> ptr) {
  NOT_IMPLEMENTED;
}

uint_tp Device::num_queues() {
  return 1;
}

bool Device::is_host_unified() {
  return true;
}

bool Device::CheckVendor(string vendor) {
  return "CPU";
}

bool Device::CheckCapability(string cap) {
  return false;
}


bool Device::CheckType(string type) {
  if (type.compare("CPU") == 1)
    return true;
  return false;
}

void Device::SwitchQueue(uint_tp id) {}

void Device::FinishQueues() {}

shared_ptr<DeviceProgram> Device::CreateProgram() {
  return nullptr;
}

void Device::memcpy(const uint_tp n, vptr<const void> x, vptr<void> y) {
  NOT_IMPLEMENTED;
}

void Device::memcpy(const uint_tp n, const void* x, vptr<void> y) {
  NOT_IMPLEMENTED;
}

void Device::memcpy(const uint_tp n, vptr<const void> x, void* y) {
  NOT_IMPLEMENTED;
}

Backend Device::backend() const {
  return backend_;
}

uint_tp Device::id() const {
  return id_;
}

uint_tp Device::list_id() const {
  return list_id_;
}

size_t Device::workgroup_size(uint_tp id) {
  return max_local_sizes_[id % 3];
}

bool Device::is_fast_unsafe_math() const {
  return fast_unsafe_math_;
}

uint_tp Device::current_queue_id() {
  return current_queue_id_;
}

uint_tp Device::memory_usage() {
  return memory_usage_;
}

uint_tp Device::peak_memory_usage() {
  return peak_memory_usage_;
}

void Device::increase_memory_usage(uint_tp bytes) {
  memory_usage_ += bytes;
  if (memory_usage_ > peak_memory_usage_) {
    peak_memory_usage_ = memory_usage_;
  }
}

void Device::decrease_memory_usage(uint_tp bytes) {
  memory_usage_ -= bytes;
}

void Device::reset_peak_memory_usage() {
  peak_memory_usage_ = memory_usage_;
}

template<typename Dtype>
shared_ptr<Blob<Dtype> > Device::Buffer(vector<int_tp> shape, int_tp* lock_id) {
  CHECK(lock_id);
  shared_ptr<Blob<Dtype> > blob = std::make_shared<Blob<Dtype> >(this);
  vector<int_tp> buffer_shape;
  for(size_t i = 0; i < shape.size(); ++i) {
    buffer_shape.push_back(safe_sizeof<Dtype>() * shape[i]);
  }

  // Ensure the thread safety of this function
  buffer_vec_mutex_.lock();
  size_t buffer_id = -1;
  for (size_t i = 0; i < buffers_.size(); ++i) {
    if (buffer_mutex_[i]->try_lock()) {
      buffer_id = i;
      break;
    }
  }

  // No buffers available, create a new one
  if (buffer_id == -1) {
    buffer_id = buffers_.size();
    buffers_.push_back(std::make_shared<Blob<int8_t> >(this));
    buffer_mutex_.push_back(std::make_shared<std::mutex>());
    buffer_mutex_[buffer_id]->lock();
  }

  buffer_vec_mutex_.unlock();

  Blob<int8_t>* buffer = buffers_[buffer_id].get();

  // Ensure the buffer is big enough for the request
  buffer->Reshape(buffer_shape);
  // Share data between returned Blob object and internal device buffer
  blob->ShareDataBase(buffer);
  blob->ShareDiffBase(buffer);

  if (not (lock_id == nullptr)) {
    *lock_id = buffer_id;
  }

  return blob;
}

template shared_ptr<Blob<half_fp> > Device::Buffer(vector<int_tp> shape,
                                                   int_tp* lock_id);
template shared_ptr<Blob<float> > Device::Buffer(vector<int_tp> shape,
                                                   int_tp* lock_id);
template shared_ptr<Blob<double> > Device::Buffer(vector<int_tp> shape,
                                                   int_tp* lock_id);
template shared_ptr<Blob<int8_t> > Device::Buffer(vector<int_tp> shape,
                                                   int_tp* lock_id);
template shared_ptr<Blob<int16_t> > Device::Buffer(vector<int_tp> shape,
                                                   int_tp* lock_id);
template shared_ptr<Blob<int32_t> > Device::Buffer(vector<int_tp> shape,
                                                   int_tp* lock_id);
template shared_ptr<Blob<int64_t> > Device::Buffer(vector<int_tp> shape,
                                                   int_tp* lock_id);
template shared_ptr<Blob<uint8_t> > Device::Buffer(vector<int_tp> shape,
                                                   int_tp* lock_id);
template shared_ptr<Blob<uint16_t> > Device::Buffer(vector<int_tp> shape,
                                                   int_tp* lock_id);
template shared_ptr<Blob<uint32_t> > Device::Buffer(vector<int_tp> shape,
                                                   int_tp* lock_id);
template shared_ptr<Blob<uint64_t> > Device::Buffer(vector<int_tp> shape,
                                                   int_tp* lock_id);


void Device::unlock_buffer(int_tp* lock_id) {
  if (*lock_id < buffer_mutex_.size()) {
    buffer_mutex_[*lock_id]->unlock();
  }
  *lock_id = -1;
}


}  // namespace caffe
