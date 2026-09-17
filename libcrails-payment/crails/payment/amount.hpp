#pragma once
#include "export.hpp"
#include <cstdint>
#include <iosfwd>
#include <string>
#include <string_view>

namespace Crails
{
  namespace Payment
  {
    class LIBCRAILS_PAYMENT_SYMEXPORT Amount
    {
    public:
      Amount() = default;
      Amount(std::int64_t value, std::string_view currency) : value(value), currency_code(currency)
      {
      }

      std::int64_t value = 0;      // e.g. 1050 for "10.50 EUR"
      std::string  currency_code;  // ISO 4217, uppercase, e.g. "EUR", "USD"

      bool is_zero() const { return value == 0; }
      bool operator==(const Amount& other) const { return value == other.value && currency_code == other.currency_code; }
      bool operator!=(const Amount& other) const { return !(*this == other); }

      Amount operator+(const Amount& other) const;
      Amount operator-(const Amount& other) const;

      std::string to_string() const;
    };
  }
}

LIBCRAILS_PAYMENT_SYMEXPORT std::ostream& operator<<(std::ostream&, const Crails::Payment::Amount&);
LIBCRAILS_PAYMENT_SYMEXPORT std::istream& operator>>(std::istream&, Crails::Payment::Amount&);
