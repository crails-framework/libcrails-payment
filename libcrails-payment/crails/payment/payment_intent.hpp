#pragma once
#include "export.hpp"
#include "amount.hpp"
#include <map>
#include <optional>
#include <string>
#include <string_view>

namespace Crails
{
  namespace Payment
  {
    enum class PaymentStatus
    {
      RequiresPaymentMethod,
      RequiresAction,        // The application should send the payer to redirect_url
      Processing,            // Waiting for a webhook
      Succeeded,
      Failed,
      Canceled               // Explicitly abandoned or unconfirmed
    };

    LIBCRAILS_PAYMENT_SYMEXPORT std::string_view to_string(PaymentStatus);

    struct PaymentIntentParams
    {
      Amount                             amount;
      std::optional<std::string>         customer_id;
      std::optional<std::string>         payment_method_id;
      std::optional<std::string>         mandate_id;
      std::optional<std::string>         description;
      std::optional<std::string>         return_url;
      std::map<std::string, std::string> metadata;
      bool                               capture_immediately = true; // false => authorize now, capture() later
    };

    struct PaymentIntent
    {
      std::string                        id;
      std::string                        provider_name;
      PaymentStatus                      status = PaymentStatus::RequiresPaymentMethod;
      Amount                             amount;
      std::optional<std::string>         redirect_url;    // set when status == RequiresAction
      std::optional<std::string>         failure_reason;  // set when status == Failed
      std::map<std::string, std::string> metadata;
    };
  }
}
