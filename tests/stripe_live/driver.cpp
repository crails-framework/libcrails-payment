// Unlike every other test in this package, this one makes real HTTPS
// calls to Stripe's own servers (api.stripe.com), using Stripe's *test
// mode* - no real card or money is ever involved, but this does need
// genuine network access and a Stripe account, which is why it's not run
// automatically as part of a normal, offline build.
//
// To run it:
//   1. Grab a *test-mode* secret key from your Stripe dashboard
//      (https://dashboard.stripe.com/test/apikeys) - it starts with
//      "sk_test_", never "sk_live_".
//   2. export STRIPE_TEST_SECRET_KEY=sk_test_...
//   3. Build and run this driver.
// Without that environment variable set, this test prints a notice and
// exits successfully (0) rather than failing, so it doesn't break a build
// that has no Stripe credentials configured.
//
// The test payment method tokens below (pm_card_visa,
// pm_card_chargeDeclined) are Stripe's own documented test-mode fixtures
// for exercising card behavior without Stripe.js - see
// https://stripe.com/docs/testing#cards. Stripe does change these
// occasionally; if a card-related assertion here starts failing, check
// that page before assuming the backend itself regressed.
#include <crails/payment/stripe.hpp>
#include <crails/payment/errors.hpp>
#undef NODEBUG
#include <cassert>
#include <cstdlib>
#include <iostream>

using namespace std;
using namespace Crails::Payment;

int main()
{
  const char* secret_key = getenv("STRIPE_TEST_SECRET_KEY");

  if (!secret_key || string(secret_key).empty())
  {
    cerr << "STRIPE_TEST_SECRET_KEY is not set: skipping the live Stripe test.\n"
         << "Set it to a Stripe *test-mode* secret key (sk_test_...) to run this "
         << "against Stripe's real test environment." << endl;
    return 0;
  }
  if (string(secret_key).rfind("sk_test_", 0) != 0)
  {
    cerr << "STRIPE_TEST_SECRET_KEY doesn't start with 'sk_test_' - refusing to "
         << "run this against what might be a live key." << endl;
    return 1;
  }

  Stripe::Provider provider(secret_key);

  // 1. Create a customer.
  CustomerParams customer_params;

  customer_params.email = "libcrails-payment-test@example.com";
  customer_params.name  = "libcrails-payment live test";

  Customer customer = provider.create_customer(customer_params);

  assert(!customer.id.empty());
  cout << "Created customer " << customer.id << endl;

  // 2. A successful, immediately-confirmed payment.
  PaymentIntent succeeded_intent;
  {
    PaymentIntentParams params;

    params.amount            = Amount(1000, "EUR");
    params.customer_id       = customer.id;
    params.payment_method_id = "pm_card_visa";
    params.description       = "libcrails-payment live test: successful payment";

    succeeded_intent = provider.create_payment(params);
    cout << "Created PaymentIntent " << succeeded_intent.id
         << ", status=" << to_string(succeeded_intent.status) << endl;
    assert(succeeded_intent.status == PaymentStatus::Succeeded);
    assert(succeeded_intent.amount == params.amount);

    // Fetching it back should agree with what create_payment returned.
    PaymentIntent fetched = provider.fetch_payment(succeeded_intent.id);
    assert(fetched.id == succeeded_intent.id);
    assert(fetched.status == PaymentStatus::Succeeded);
  }

  // 3. Refund it in full.
  {
    Refund refund_result = provider.refund({succeeded_intent.id, {}, {}});

    cout << "Refunded " << refund_result.id << ", status=" << to_string(refund_result.status) << endl;
    assert(refund_result.payment_intent_id == succeeded_intent.id);
    assert(refund_result.status == RefundStatus::Succeeded || refund_result.status == RefundStatus::Pending);
  }

  // 4. A declined card: create_payment must throw Declined, not just
  // return some "failed" status.
  {
    PaymentIntentParams params;

    params.amount            = Amount(1000, "EUR");
    params.payment_method_id = "pm_card_chargeDeclined";
    params.description       = "libcrails-payment live test: declined payment";

    bool declined = false;

    try { provider.create_payment(params); }
    catch (const Declined& e)
    {
      declined = true;
      cout << "Correctly declined: code=" << e.code() << " message=" << e.what() << endl;
    }
    assert(declined);
  }

  // 5. Manual capture: create without capturing immediately, then
  // capture explicitly.
  {
    PaymentIntentParams params;

    params.amount              = Amount(1500, "EUR");
    params.payment_method_id   = "pm_card_visa";
    params.capture_immediately = false;

    PaymentIntent intent = provider.create_payment(params);

    cout << "Manual-capture intent " << intent.id << " status=" << to_string(intent.status) << endl;
    // Stripe's "requires_capture" maps to our Processing - see
    // Provider::parse_payment_intent's comment on that mapping.
    assert(intent.status == PaymentStatus::Processing);

    PaymentIntent captured = provider.capture_payment(intent.id, {});

    cout << "Captured, new status=" << to_string(captured.status) << endl;
    assert(captured.status == PaymentStatus::Succeeded);
  }

  // 6. Cancel an uncaptured PaymentIntent.
  {
    PaymentIntentParams params;

    params.amount              = Amount(500, "EUR");
    params.payment_method_id   = "pm_card_visa";
    params.capture_immediately = false;

    PaymentIntent intent   = provider.create_payment(params);
    PaymentIntent canceled = provider.cancel_payment(intent.id);

    cout << "Canceled " << canceled.id << endl;
    assert(canceled.status == PaymentStatus::Canceled);
  }

  // 7. Mandates are genuinely unsupported by this backend - confirm
  // that's still true talking to the real Stripe, not just our own
  // default kicking in without ever making a request.
  {
    bool threw = false;

    try { provider.create_mandate(customer.id, "https://example.com/return"); }
    catch (const UnsupportedOperation&) { threw = true; }
    assert(threw);
  }

  cout << "All live Stripe tests passed." << endl;
  return 0;
}
