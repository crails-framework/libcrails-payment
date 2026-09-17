#include <crails/payment/amount.hpp>
#include <crails/payment/errors.hpp>
#undef NODEBUG
#include <cassert>
#include <sstream>

using namespace std;
using namespace Crails::Payment;

int main()
{
  // Basic arithmetic within the same currency.
  {
    Amount a(1050, "EUR"), b(250, "EUR");

    assert((a + b).value == 1300);
    assert((a - b).value == 800);
    assert(a == Amount(1050, "EUR"));
    assert(a != b);
  }

  // Combining different currencies is a hard error, not silent nonsense.
  {
    Amount eur(1000, "EUR"), usd(1000, "USD");
    bool   threw = false;

    try { auto sum = eur + usd; }
    catch (const CurrencyMismatch& e) { threw = true; }
    assert(threw);
  }

  // is_zero().
  {
    assert(Amount().is_zero());
    assert(Amount(0, "EUR").is_zero());
    assert(!Amount(1, "EUR").is_zero());
  }

  // to_string() is for display: two decimals, currency suffix.
  {
    assert(Amount(1050, "EUR").to_string() == "10.50 EUR");
    assert(Amount(5, "EUR").to_string() == "0.05 EUR");
    assert(Amount(-150, "USD").to_string() == "-1.50 USD");
  }

  // operator<</operator>> are lossless and round-trip, independently of
  // to_string()'s display formatting.
  {
    Amount             original(1050, "EUR");
    ostringstream      out;

    out << original;
    assert(out.str() == "1050 EUR");

    Amount        restored;
    istringstream in(out.str());

    in >> restored;
    assert(restored == original);
  }

  return 0;
}
