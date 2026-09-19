#pragma once
# include <string>
# include <string_view>

namespace Crails
{
  namespace Payment
  {
    namespace Stripe
    {
      // Verifies a Stripe webhook's Stripe-Signature header against
      // `payload` (the exact, raw request body - not re-serialized JSON)
      // and `secret` (the endpoint's signing secret, "whsec_..."), per
      // https://stripe.com/docs/webhooks/signatures:
      //
      //   Stripe-Signature: t=1614556800,v1=<hex hmac-sha256>,v1=<...>
      //
      // The signed payload is "{t}.{payload}"; the signature is
      // HMAC-SHA256 of that, hex-encoded (computed via
      // Crails::HmacDigest, from libcrails-encrypt - this file never
      // touches OpenSSL, or any other crypto library, directly). A header
      // can carry more than one v1 value during secret rotation - this
      // accepts the payload as valid if *any* of them match, compared via
      // Crails::secure_compare. Returns false (never throws) if the
      // header is missing, malformed, no v1 value matches, or the
      // timestamp is more than `tolerance_seconds` away from now (replay
      // protection).
      bool verify_webhook_signature(std::string_view payload, std::string_view signature_header, const std::string& secret, long tolerance_seconds = 300);
    }
  }
}
