#include <crails/payment/registry.hpp>
#include <crails/tests/payment.hpp>
#undef NODEBUG
#include <cassert>

using namespace std;
using namespace Crails::Payment;
using Crails::Tests::PaymentProvider;

namespace
{
  // A registry the test can hand an arbitrary, already-built set of
  // providers to, so each test block below sets up exactly the backends
  // it needs - the same way an application lists its own backends
  // directly in its own subclass' constructor (see registry.hpp).
  class TestRegistry : public Crails::Payment::Registry
  {
    SINGLETON_IMPLEMENTATION(TestRegistry, Crails::Payment::Registry)
  public:
    TestRegistry(vector<shared_ptr<const Provider>> providers)
    {
      for (auto& provider : providers)
        add(provider);
    }
  };
}

int main()
{
  // Nothing configured at all: find() fails loudly - a misconfigured
  // application, not a "this backend doesn't exist" case.
  {
    bool threw = false;

    try { Registry::find("anything"); }
    catch (const exception&) { threw = true; }
    assert(threw);
  }

  // Configured, but empty: find() returns null, get() throws out_of_range.
  {
    SingletonInstantiator<TestRegistry> registry(vector<shared_ptr<const Provider>>{});

    assert(Registry::find("stripe") == nullptr);

    bool threw = false;
    try { Registry::get("stripe"); }
    catch (const out_of_range&) { threw = true; }
    assert(threw);
  }

  // Multiple providers, looked up by name.
  {
    auto stripe  = make_shared<PaymentProvider>("stripe");
    auto slimpay = make_shared<PaymentProvider>("slimpay");

    SingletonInstantiator<TestRegistry> registry(vector<shared_ptr<const Provider>>{stripe, slimpay});

    assert(Registry::find("stripe").get() == stripe.get());
    assert(Registry::find("slimpay").get() == slimpay.get());
    assert(&Registry::get("stripe") == stripe.get());
    assert(Registry::find("paypal") == nullptr);
  }

  // Registering a second provider under a name already in use replaces
  // the first: Registry is a name-keyed slot, not a multimap.
  {
    auto first  = make_shared<PaymentProvider>("cash");
    auto second = make_shared<PaymentProvider>("cash");

    SingletonInstantiator<TestRegistry> registry(vector<shared_ptr<const Provider>>{first, second});
    assert(Registry::find("cash").get() == second.get());
  }

  // Back to unconfigured once the last SingletonInstantiator above went
  // out of scope.
  {
    bool threw = false;

    try { Registry::find("cash"); }
    catch (const exception&) { threw = true; }
    assert(threw);
  }

  return 0;
}
