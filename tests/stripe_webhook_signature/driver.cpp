#include <crails/payment/stripe/webhook_signature.hpp>
#include <crails/hmac.hpp>
#undef NODEBUG
#include <cassert>
#include <chrono>

using namespace std;
using namespace Crails::Payment::Stripe;

static long now()
{
  return static_cast<long>(chrono::duration_cast<chrono::seconds>(chrono::system_clock::now().time_since_epoch()).count());
}

int main()
{
  const string secret  = "whsec_test_secret";
  const string payload = R"({"id":"evt_test_webhook","object":"event"})";

  // Valid signature, fresh timestamp.
  {
    long   timestamp = now();
    string signed_payload = to_string(timestamp) + "." + payload;
    string signature = Crails::HmacDigest("sha256", secret, signed_payload).to_string();
    string header = "t=" + to_string(timestamp) + ",v1=" + signature;

    assert(verify_webhook_signature(payload, header, secret));
  }

  // Tampered payload: same header, different body -> rejected.
  {
    long   timestamp = now();
    string signed_payload = to_string(timestamp) + "." + payload;
    string signature = Crails::HmacDigest("sha256", secret, signed_payload).to_string();
    string header = "t=" + to_string(timestamp) + ",v1=" + signature;

    assert(!verify_webhook_signature(payload + "tampered", header, secret));
  }

  // Wrong secret -> rejected.
  {
    long   timestamp = now();
    string signed_payload = to_string(timestamp) + "." + payload;
    string signature = Crails::HmacDigest("sha256", "whsec_wrong_secret", signed_payload).to_string();
    string header = "t=" + to_string(timestamp) + ",v1=" + signature;

    assert(!verify_webhook_signature(payload, header, secret));
  }

  // Multiple v1 entries (secret rotation): matches the second one.
  {
    long   timestamp = now();
    string signed_payload = to_string(timestamp) + "." + payload;
    string real_signature = Crails::HmacDigest("sha256", secret, signed_payload).to_string();
    string header = "t=" + to_string(timestamp) + ",v1=deadbeef,v1=" + real_signature;

    assert(verify_webhook_signature(payload, header, secret));
  }

  // Timestamp too old: replay protection kicks in even with a correct signature.
  {
    long   timestamp = now() - 1000; // older than the default 300s tolerance
    string signed_payload = to_string(timestamp) + "." + payload;
    string signature = Crails::HmacDigest("sha256", secret, signed_payload).to_string();
    string header = "t=" + to_string(timestamp) + ",v1=" + signature;

    assert(!verify_webhook_signature(payload, header, secret));
    // ...but passes with an explicitly wider tolerance.
    assert(verify_webhook_signature(payload, header, secret, 2000));
  }

  // Timestamp in the future beyond tolerance is rejected too (abs()).
  {
    long   timestamp = now() + 1000;
    string signed_payload = to_string(timestamp) + "." + payload;
    string signature = Crails::HmacDigest("sha256", secret, signed_payload).to_string();
    string header = "t=" + to_string(timestamp) + ",v1=" + signature;

    assert(!verify_webhook_signature(payload, header, secret));
  }

  // Malformed headers never throw, just return false.
  {
    assert(!verify_webhook_signature(payload, "", secret));
    assert(!verify_webhook_signature(payload, "garbage", secret));
    assert(!verify_webhook_signature(payload, "t=notanumber,v1=abc", secret));
    assert(!verify_webhook_signature(payload, "v1=abc", secret)); // missing timestamp
    assert(!verify_webhook_signature(payload, "t=" + to_string(now()), secret)); // missing v1
  }

  return 0;
}
