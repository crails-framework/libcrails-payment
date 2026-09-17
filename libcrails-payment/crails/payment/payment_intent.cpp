#include "payment_intent.hpp"

using namespace std;

string_view Crails::Payment::to_string(PaymentStatus status)
{
  switch (status)
  {
  case PaymentStatus::RequiresPaymentMethod: return "requires_payment_method";
  case PaymentStatus::RequiresAction:        return "requires_action";
  case PaymentStatus::Processing:            return "processing";
  case PaymentStatus::Succeeded:             return "succeeded";
  case PaymentStatus::Failed:                return "failed";
  case PaymentStatus::Canceled:              return "canceled";
  }
  return "unknown";
}
