#include "provider.hpp"
#include "errors.hpp"

using namespace std;
using namespace Crails::Payment;

void Provider::throw_unsupported(const string& operation) const
{
  throw UnsupportedOperation(provider_name, provider_name + " does not support " + operation);
}

Mandate Provider::create_mandate(const string&, const string&) const
{
  throw_unsupported("mandates");
}

void Provider::create_mandate_async(const string& customer_id, const string& return_url, Result<Mandate> callback) const noexcept
{
  try { callback(create_mandate(customer_id, return_url), nullptr); }
  catch (...) { callback(Mandate(), current_exception()); }
}

Mandate Provider::fetch_mandate(const string&) const
{
  throw_unsupported("mandates");
}

void Provider::fetch_mandate_async(const string& mandate_id, Result<Mandate> callback) const noexcept
{
  try { callback(fetch_mandate(mandate_id), nullptr); }
  catch (...) { callback(Mandate(), current_exception()); }
}
