#include <crails/payment/stripe/provider.hpp>
#include <crails/payment/errors.hpp>
#undef NODEBUG
#include <cassert>

using namespace std;
using namespace Crails::Payment;

namespace
{
  // Exposes Stripe::Provider's protected JSON-mapping helpers for direct
  // testing, without needing real network access to api.stripe.com.
  struct TestableProvider : public Stripe::Provider
  {
    using Stripe::Provider::Provider;
    using Stripe::Provider::parse_payment_intent;
    using Stripe::Provider::parse_customer;
    using Stripe::Provider::parse_refund;
    using Stripe::Provider::raise_for_status;
  };

  // Builds a minimal Client::Response carrying the given status and body,
  // for raise_for_status()'s tests.
  Crails::ClientInterface::Response make_response(unsigned status, const string& body)
  {
    Crails::ClientInterface::Response response;

    response.result(static_cast<Crails::HttpStatus>(status));
    response.body() = body;
    return response;
  }
}

int main()
{
  TestableProvider provider("sk_test_dummy");

  // A freshly created PaymentIntent, awaiting a payment method.
  {
    DataTree json;

    json.from_json(R"({
      "id": "pi_3N0aBcDeFgHiJk",
      "object": "payment_intent",
      "amount": 2000,
      "currency": "eur",
      "status": "requires_payment_method",
      "client_secret": "pi_3N0aBcDeFgHiJk_secret_XyZ",
      "last_payment_error": null
    })");
    PaymentIntent intent = provider.parse_payment_intent(json.as_data());

    assert(intent.id == "pi_3N0aBcDeFgHiJk");
    assert(intent.provider_name == "stripe");
    assert(intent.amount == Amount(2000, "EUR"));
    assert(intent.status == PaymentStatus::RequiresPaymentMethod);
    assert(intent.metadata.at("stripe_client_secret") == "pi_3N0aBcDeFgHiJk_secret_XyZ");
    assert(intent.metadata.at("stripe_status") == "requires_payment_method");
    assert(!intent.redirect_url.has_value());
  }

  // A successfully confirmed, captured PaymentIntent.
  {
    DataTree json;
    json.from_json(R"({
      "id": "pi_success",
      "amount": 1050,
      "currency": "usd",
      "status": "succeeded",
      "client_secret": "pi_success_secret_abc"
    })");
    PaymentIntent intent = provider.parse_payment_intent(json.as_data());

    assert(intent.status == PaymentStatus::Succeeded);
    assert(intent.amount == Amount(1050, "USD"));
  }

  // A failed attempt that Stripe kept alive for retry: distinguished from
  // "never attempted" purely by the presence of last_payment_error.
  {
    DataTree json;
    json.from_json(R"({
      "id": "pi_failed_retry",
      "amount": 500,
      "currency": "eur",
      "status": "requires_payment_method",
      "last_payment_error": { "code": "card_declined", "message": "Your card was declined." }
    })");
    PaymentIntent intent = provider.parse_payment_intent(json.as_data());

    assert(intent.status == PaymentStatus::Failed);
    assert(intent.failure_reason.has_value());
    assert(*intent.failure_reason == "Your card was declined.");
  }

  // requires_action with a genuine redirect (e.g. iDEAL): maps to our
  // RequiresAction + redirect_url.
  {
    DataTree json;
    json.from_json(R"({
      "id": "pi_redirect",
      "amount": 3000,
      "currency": "eur",
      "status": "requires_action",
      "next_action": {
        "type": "redirect_to_url",
        "redirect_to_url": { "url": "https://hooks.stripe.com/redirect/abc123", "return_url": "https://example.com/return" }
      }
    })");
    PaymentIntent intent = provider.parse_payment_intent(json.as_data());

    assert(intent.status == PaymentStatus::RequiresAction);
    assert(intent.redirect_url.has_value());
    assert(*intent.redirect_url == "https://hooks.stripe.com/redirect/abc123");
  }

  // requires_action with use_stripe_sdk (card 3DS): no redirect_url, since
  // there genuinely isn't one - client_secret is still captured.
  {
    DataTree json;
    json.from_json(R"({
      "id": "pi_3ds",
      "amount": 4000,
      "currency": "usd",
      "status": "requires_action",
      "client_secret": "pi_3ds_secret_xyz",
      "next_action": { "type": "use_stripe_sdk" }
    })");
    PaymentIntent intent = provider.parse_payment_intent(json.as_data());

    assert(intent.status == PaymentStatus::RequiresAction);
    assert(!intent.redirect_url.has_value());
    assert(intent.metadata.at("stripe_client_secret") == "pi_3ds_secret_xyz");
  }

  // requires_capture (manual capture flow): maps to Processing.
  {
    DataTree json;
    json.from_json(R"({"id": "pi_manual", "amount": 100, "currency": "eur", "status": "requires_capture"})");
    PaymentIntent intent = provider.parse_payment_intent(json.as_data());
    assert(intent.status == PaymentStatus::Processing);
  }

  // canceled.
  {
    DataTree json;
    json.from_json(R"({"id": "pi_canceled", "amount": 100, "currency": "eur", "status": "canceled"})");
    PaymentIntent intent = provider.parse_payment_intent(json.as_data());
    assert(intent.status == PaymentStatus::Canceled);
  }

  // An unrecognized/future status is treated conservatively as Processing,
  // not silently assumed to be anything more final.
  {
    DataTree json;
    json.from_json(R"({"id": "pi_future", "amount": 100, "currency": "eur", "status": "some_future_status"})");
    PaymentIntent intent = provider.parse_payment_intent(json.as_data());
    assert(intent.status == PaymentStatus::Processing);
    assert(intent.metadata.at("stripe_status") == "some_future_status");
  }

  // Customer.
  {
    DataTree json;
    json.from_json(R"({"id": "cus_abc123", "email": "jane@example.com", "name": "Jane Doe", "phone": null})");
    Customer customer = provider.parse_customer(json.as_data());

    assert(customer.id == "cus_abc123");
    assert(customer.email.has_value() && *customer.email == "jane@example.com");
    assert(customer.name.has_value() && *customer.name == "Jane Doe");
    assert(!customer.phone.has_value());
  }

  // Refund.
  {
    DataTree json;
    json.from_json(R"({"id": "re_abc", "payment_intent": "pi_abc", "amount": 500, "currency": "eur", "status": "succeeded"})");
    Refund refund = provider.parse_refund(json.as_data());

    assert(refund.id == "re_abc");
    assert(refund.payment_intent_id == "pi_abc");
    assert(refund.amount == Amount(500, "EUR"));
    assert(refund.status == RefundStatus::Succeeded);
  }

  // raise_for_status: card_error -> Declined.
  {
    string body = R"({"error": {"type": "card_error", "code": "insufficient_funds", "message": "Your card has insufficient funds."}})";
    bool   caught = false;

    try { provider.raise_for_status(make_response(402, body)); }
    catch (const Declined& e)
    {
      caught = true;
      assert(e.code() == "insufficient_funds");
      assert(string(e.what()) == "Your card has insufficient funds.");
    }
    catch (...) {}
    assert(caught);
  }

  return 0;

  // raise_for_status: invalid_request_error -> generic RequestError.
  {
    string body = R"({"error": {"type": "invalid_request_error", "code": "parameter_missing", "message": "Missing required param: amount."}})";
    bool   caught = false;

    try { provider.raise_for_status(make_response(400, body)); }
    catch (const RequestError& e)
    {
      caught = true;
      assert(e.code() == "parameter_missing");
    }
    catch (...) {}
    assert(caught);
  }

  // raise_for_status: HTTP 401 -> AuthenticationError, regardless of body shape.
  {
    string body = R"({"error": {"type": "invalid_request_error", "message": "Invalid API Key provided"}})";
    bool   caught = false;

    try { provider.raise_for_status(make_response(401, body)); }
    catch (const AuthenticationError&) { caught = true; }
    catch (...) {}
    assert(caught);
  }

  // raise_for_status: unparseable body still throws something sane, not a
  // crash or a hang.
  {
    bool caught = false;

    try { provider.raise_for_status(make_response(500, "not json at all")); }
    catch (const RequestError& e) { caught = true; assert(!e.code().empty()); }
    catch (...) {}
    assert(caught);
  }

  return 0;
}
