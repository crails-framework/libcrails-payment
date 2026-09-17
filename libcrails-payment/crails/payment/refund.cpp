#include "refund.hpp"

using namespace std;

string_view Crails::Payment::to_string(RefundStatus status)
{
  switch (status)
  {
  case RefundStatus::Pending:   return "pending";
  case RefundStatus::Succeeded: return "succeeded";
  case RefundStatus::Failed:    return "failed";
  }
  return "unknown";
}
