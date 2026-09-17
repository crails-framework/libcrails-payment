#include <crails/tests/payment.hpp>
#undef NODEBUG
#include <cassert>

using namespace std;
using namespace Crails::Payment;
using Crails::Tests::PaymentProvider;

int main()
{
  // Default (card-only) provider: mandates unsupported, sync AND async.
  {
    PaymentProvider provider;
    bool sync_threw = false, async_threw = false;

    try { provider.create_mandate("cus_1", "https://example.com/return"); }
    catch (const UnsupportedOperation&) { sync_threw = true; }
    assert(sync_threw);

    // Regression test for the provider.cpp fix: async must reflect the
    // same "unsupported" behavior as sync, via delegation - not have its
    // own hardcoded copy of the throw.
    provider.create_mandate_async("cus_1", "https://example.com/return",
      [&](const Mandate&, exception_ptr error)
      {
        try { rethrow_exception(error); }
        catch (const UnsupportedOperation&) { async_threw = true; }
      });
    assert(async_threw);
    assert(provider.create_mandate_calls == 2);
  }

  // Happy path: create -> fetch -> capture -> refund.
  {
    PaymentProvider provider;
    PaymentIntentParams params;
    params.amount = Amount(1000, "EUR");

    PaymentIntent intent = provider.create_payment(params);
    assert(intent.status == PaymentStatus::Succeeded);
    assert(intent.amount == params.amount);

    PaymentIntent fetched = provider.fetch_payment(intent.id);
    assert(fetched.id == intent.id);

    Refund refund_result = provider.refund({intent.id, {}, {}});
    assert(refund_result.status == RefundStatus::Succeeded);
    assert(refund_result.amount == intent.amount);

    assert(provider.create_payment_calls == 1);
    // one explicit fetch_payment() call, plus one made internally by refund()
    assert(provider.fetch_payment_calls == 2);
    assert(provider.refund_calls == 1);
  }

  // Scripted failure via on_create_payment.
  {
    PaymentProvider provider;
    bool caught = false;

    provider.on_create_payment = []() { throw NetworkError("test", "connection reset"); };
    try { provider.create_payment({}); }
    catch (const NetworkError&) { caught = true; }
    assert(caught);
    assert(provider.create_payment_calls == 1);
  }

  // RequiresAction / redirect_url flow.
  {
    PaymentProvider provider;
    provider.next_status = PaymentStatus::RequiresAction;
    provider.next_redirect_url = "https://processor.example/3ds/abc123";

    PaymentIntent intent = provider.create_payment({});
    assert(intent.status == PaymentStatus::RequiresAction);
    assert(intent.redirect_url.has_value());
    assert(*intent.redirect_url == "https://processor.example/3ds/abc123");
  }

  // Mandate-capable configuration.
  {
    PaymentProvider provider;
    provider.supports_mandates = true;
    provider.next_mandate_status = MandateStatus::Active;

    Mandate mandate = provider.create_mandate("cus_1", "https://example.com/return");
    assert(mandate.status == MandateStatus::Active);

    Mandate fetched = provider.fetch_mandate(mandate.id);
    assert(fetched.id == mandate.id);

    bool threw = false;
    try { provider.fetch_mandate("does_not_exist"); }
    catch (const RequestError&) { threw = true; }
    assert(threw);
  }

  // fetch_payment on an unknown id.
  {
    PaymentProvider provider;
    bool threw = false;

    try { provider.fetch_payment("nope"); }
    catch (const RequestError& e) { threw = true; assert(e.code() == "not_found"); }
    assert(threw);
  }

  // Async wrapping: success and failure both funnel through Result<T> correctly.
  {
    PaymentProvider provider;
    bool success_seen = false;

    provider.create_payment_async({}, [&](const PaymentIntent& intent, exception_ptr error)
    {
      assert(!error);
      assert(intent.status == PaymentStatus::Succeeded);
      success_seen = true;
    });
    assert(success_seen);
  }

  // Every operation is genuinely callable through a const Provider&,
  // exactly how Crails::Payment::Registry hands one out. Configuration
  // (on_*, next_status...) still happens through non-const access before
  // the object is used this way, matching how a real backend would be
  // fully configured (API key, endpoint...) at construction time, then
  // only ever called through const references afterward.
  {
    PaymentProvider    mutable_provider;
    PaymentIntentParams params;

    params.amount = Amount(2000, "USD");
    mutable_provider.next_status = PaymentStatus::Succeeded;

    const Provider& provider = mutable_provider;
    shared_ptr<const Provider> shared_view = make_shared<PaymentProvider>();

    PaymentIntent intent = provider.create_payment(params);
    assert(intent.amount == params.amount);

    bool async_success = false;
    shared_view->create_payment_async(params, [&](const PaymentIntent& i, exception_ptr error)
    {
      assert(!error);
      async_success = true;
    });
    assert(async_success);
  }

  return 0;
}
