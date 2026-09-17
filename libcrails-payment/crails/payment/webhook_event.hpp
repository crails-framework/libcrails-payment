#pragma once
# include "export.hpp"
# include "payment_intent.hpp"
# include "mandate.hpp"
# include "refund.hpp"
# include <crails/datatree.hpp>
# include <optional>
# include <string>

namespace Crails
{
  namespace Payment
  {
    enum class WebhookEventType
    {
      PaymentSucceeded,
      PaymentFailed,
      PaymentActionRequired,
      MandateActivated,
      MandateRevoked,
      RefundUpdated,
      Unknown
    };

    LIBCRAILS_PAYMENT_SYMEXPORT std::string to_string(WebhookEventType);

    struct WebhookEvent
    {
      std::string      provider_name;
      WebhookEventType type = WebhookEventType::Unknown;
      std::string      raw_type;
      DataTree         raw;

      std::optional<PaymentIntent> payment_intent;
      std::optional<Mandate>       mandate;
      std::optional<Refund>        refund;
    };
  }
}
