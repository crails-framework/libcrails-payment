#include "amount.hpp"
#include "errors.hpp"
#include <istream>
#include <ostream>
#include <sstream>
#include <iomanip>

using namespace std;
using namespace Crails::Payment;

Amount Amount::operator+(const Amount& other) const
{
  if (currency_code != other.currency_code)
    throw CurrencyMismatch(currency_code, other.currency_code);
  return Amount(value + other.value, currency_code);
}

Amount Amount::operator-(const Amount& other) const
{
  if (currency_code != other.currency_code)
    throw CurrencyMismatch(currency_code, other.currency_code);
  return Amount(value - other.value, currency_code);
}

string Amount::to_string() const
{
  ostringstream stream;
  const bool    negative = value < 0;
  const int64_t absolute = negative ? -value : value;

  stream << (negative ? "-" : "") << (absolute / 100) << '.'
         << setfill('0') << setw(2) << (absolute % 100) << ' ' << currency_code;
  return stream.str();
}

ostream& operator<<(ostream& stream, const Amount& amount)
{
  return stream << amount.value << ' ' << amount.currency_code;
}

istream& operator>>(istream& stream, Amount& amount)
{
  return stream >> amount.value >> amount.currency_code;
}
