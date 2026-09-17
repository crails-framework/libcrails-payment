#pragma once
#include "export.hpp"
#include "amount.hpp"
#include <optional>
#include <string>

namespace Crails
{
  namespace Payment
  {
    enum class RefundStatus
    {
      Pending,
      Succeeded,
      Failed
    };

    LIBCRAILS_PAYMENT_SYMEXPORT std::string_view to_string(RefundStatus);

    struct RefundParams
    {
      std::string                payment_intent_id;
      std::optional<Amount>      amount; // absent = full refund
      std::optional<std::string> reason;
    };

    struct Refund
    {
      std::string  id;
      std::string  payment_intent_id;
      RefundStatus status = RefundStatus::Pending;
      Amount       amount;
    };
  }
}
