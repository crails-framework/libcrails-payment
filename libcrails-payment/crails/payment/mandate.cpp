#include "mandate.hpp"

using namespace std;

string_view Crails::Payment::to_string(MandateStatus status)
{
  switch (status)
  {
  case MandateStatus::PendingConfirmation: return "pending_confirmation";
  case MandateStatus::Active:              return "active";
  case MandateStatus::Revoked:             return "revoked";
  case MandateStatus::Expired:             return "expired";
  }
  return "unknown";
}
