#pragma once
#include "export.hpp"
#include <optional>
#include <string>
#include <string_view>

namespace Crails
{
  namespace Payment
  {
    enum class MandateStatus
    {
      PendingConfirmation,
      Active,
      Revoked,
      Expired
    };

    LIBCRAILS_PAYMENT_SYMEXPORT std::string_view to_string(MandateStatus);

    // Authorization for recurring payments (SEPA thing)
    struct Mandate
    {
      std::string                 id;
      MandateStatus               status = MandateStatus::PendingConfirmation;
      std::optional<std::string>  reference;    // human-readable reference (ex: SEPA RUM)
      std::optional<std::string>  redirect_url; // where to send the payer to confirm
    };
  }
}
