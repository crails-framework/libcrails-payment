#include "webhook_event.hpp"

using namespace std;

string Crails::Payment::to_string(WebhookEventType type)
{
  switch (type)
  {
  case WebhookEventType::PaymentSucceeded:      return "payment_succeeded";
  case WebhookEventType::PaymentFailed:         return "payment_failed";
  case WebhookEventType::PaymentActionRequired: return "payment_action_required";
  case WebhookEventType::MandateActivated:      return "mandate_activated";
  case WebhookEventType::MandateRevoked:        return "mandate_revoked";
  case WebhookEventType::RefundUpdated:         return "refund_updated";
  case WebhookEventType::Unknown:               break ;
  }
  return "unknown";
}
