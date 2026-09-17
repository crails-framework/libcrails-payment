#include "errors.hpp"

using namespace std;
using namespace Crails::Payment;

static string currency_mismatch_message(const string& lhs, const string& rhs)
{
  return "cannot combine amounts in '" + lhs + "' and '" + rhs + "'";
}

CurrencyMismatch::CurrencyMismatch(const string& lhs, const string& rhs) :
  Error("crails-payment", currency_mismatch_message(lhs, rhs))
{
}
