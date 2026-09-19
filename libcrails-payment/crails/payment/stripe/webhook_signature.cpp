#include "webhook_signature.hpp"
#include <crails/hmac.hpp>
#include <crails/secure_compare.hpp>
#include <chrono>
#include <cstdlib>
#include <vector>

using namespace std;
using namespace Crails::Payment::Stripe;

namespace
{
  // Splits "t=123,v1=abc,v1=def" into a timestamp and every v1 value.
  bool parse_signature_header(string_view header, long& timestamp, vector<string>& v1_signatures)
  {
    bool   has_timestamp = false;
    size_t pos = 0;

    while (pos < header.size())
    {
      size_t      comma = header.find(',', pos);
      string_view item  = header.substr(pos, comma == string_view::npos ? string_view::npos : comma - pos);
      size_t      equals = item.find('=');

      if (equals != string_view::npos)
      {
        string_view key   = item.substr(0, equals);
        string_view value = item.substr(equals + 1);

        if (key == "t")
        {
          timestamp = strtol(string(value).c_str(), nullptr, 10);
          has_timestamp = true;
        }
        else if (key == "v1")
          v1_signatures.emplace_back(value);
      }
      if (comma == string_view::npos)
        break;
      pos = comma + 1;
    }
    return has_timestamp && !v1_signatures.empty();
  }
}

bool Crails::Payment::Stripe::verify_webhook_signature(string_view payload, string_view signature_header, const string& secret, long tolerance_seconds)
{
  long           timestamp;
  vector<string> v1_signatures;

  if (!parse_signature_header(signature_header, timestamp, v1_signatures))
    return false;

  long now = static_cast<long>(chrono::duration_cast<chrono::seconds>(chrono::system_clock::now().time_since_epoch()).count());

  if (labs(now - timestamp) > tolerance_seconds)
    return false;

  string signed_payload = to_string(timestamp) + "." + string(payload);
  string expected        = Crails::HmacDigest("sha256", secret, signed_payload).to_string();

  for (const auto& candidate : v1_signatures)
  {
    if (Crails::secure_compare(candidate, expected))
      return true;
  }
  return false;
}
