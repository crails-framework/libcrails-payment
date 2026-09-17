#pragma once
#include "export.hpp"
#include <optional>
#include <string>
#include <string_view>

namespace Crails
{
  namespace Payment
  {
    // Interpretation of the payment method allowing application
    // to branch code based on generalities (ex: recurring payment available)
    // PaymentMethod::raw_type is the provider's own label.
    enum class PaymentMethodType
    {
      Card,
      SepaDebit,
      Paypal,
      BankTransfer,
      Other
    };

    LIBCRAILS_PAYMENT_SYMEXPORT std::string_view to_string(PaymentMethodType);

    struct PaymentMethod
    {
      std::string       id;
      PaymentMethodType type = PaymentMethodType::Other;
      std::string       raw_type;

      std::optional<std::string>    brand; // ex: visa
      std::optional<std::string>    last4;
      std::optional<unsigned short> exp_month;
      std::optional<unsigned short> exp_year;
    };
  }
}
