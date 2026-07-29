#pragma once

#include <queue>
#include "LinBusListener.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace truma_inetbox {
class LinBusProtocol : public LinBusListener {
 public:
  virtual const std::array<uint8_t, 4> lin_identifier() = 0;
  virtual void lin_heartbeat() = 0;
  virtual void lin_reset_device();

 protected:
  const std::array<uint8_t, 8> lin_empty_response_ = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

  bool answer_lin_order_(const uint8_t pid) override;
  void lin_message_recieved_(const uint8_t pid, const uint8_t *message, uint8_t length) override;

  virtual bool lin_read_field_by_identifier_(uint8_t identifier, std::array<uint8_t, 5> *response) = 0;
  virtual const uint8_t *lin_multiframe_recieved(const uint8_t *message, const uint8_t message_len,
                                                  uint8_t *return_len) = 0;

  // LOCAL PATCH (thread safety): `updates_to_send_` is pushed from the main
  // loop (HA actions, protocol responses) and popped from the LIN reader
  // task. All access goes through these mutex-guarded helpers; never touch
  // the queue directly.
  size_t updates_to_send_pending_();
  bool updates_to_send_pop_(std::array<uint8_t, 8> *out, size_t *depth_after);

 private:
  uint8_t lin_node_address_ = /*LIN initial node address*/ 0x03;

  std::queue<std::array<uint8_t, 8>> updates_to_send_ = {};
  Mutex updates_to_send_mutex_;

  void prepare_update_msg_(const std::array<uint8_t, 8> message) {
    LockGuard lock(this->updates_to_send_mutex_);
    this->updates_to_send_.push(std::move(message));
  }
  bool is_matching_identifier_(const uint8_t *message);

  u_int16_t multi_pdu_message_expected_size_ = 0;
  uint8_t multi_pdu_message_len_ = 0;
  uint8_t multi_pdu_message_frame_counter_ = 0;
  uint8_t multi_pdu_message_[64];
  void lin_msg_diag_single_(const uint8_t *message, uint8_t length);
  void lin_msg_diag_first_(const uint8_t *message, uint8_t length);
  bool lin_msg_diag_consecutive_(const uint8_t *message, uint8_t length);
  void lin_msg_diag_multi_();
};

}  // namespace truma_inetbox
}  // namespace esphome
