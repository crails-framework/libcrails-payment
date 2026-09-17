#include <crails/program_options.hpp>
#include <crails/session_store/no_session_store.hpp>
#include <crails/tests/payment.hpp>
#include "test_server.hpp"

#undef NODEBUG
#include <cassert>

using namespace Crails;
using namespace Crails::Payment;
using namespace std;
using Crails::Tests::PaymentProvider;

namespace
{
  HttpRequest make_get_request()
  {
    HttpRequest request;

    request.method(HttpVerb::get);
    request.target("/path");
    return request;
  }

  // Runs `provider`'s create_payment_async through run_payment_operation,
  // reporting outcomes into out-parameters the test can assert on.
  struct RunController : public TestPaymentControllerBase
  {
    using TestPaymentControllerBase::TestPaymentControllerBase;

    bool                                     success_seen = false;
    bool                                     error_seen   = false;
    PaymentIntent                            seen_intent;
    exception_ptr                            seen_error;
    function<void(bool)>                     done;

    // When true, run_payment_operation is called without an on_error
    // callback, so a failure re-throws and must go through Context::protect
    // / the real ExceptionCatcher instead of being swallowed here.
    bool propagate_on_error = false;

    void run(const string& provider_name, const PaymentIntentParams& params)
    {
      auto provider = Registry::find(provider_name);

      run_payment_operation<PaymentIntent>(
        [provider, params](Result<PaymentIntent> callback)
        {
          provider->create_payment_async(params, callback);
        },
        [this](const PaymentIntent& intent)
        {
          success_seen = true;
          seen_intent  = intent;
          done(true);
        },
        propagate_on_error
          ? function<void(exception_ptr)>(nullptr)
          : function<void(exception_ptr)>([this](exception_ptr error)
            {
              error_seen = true;
              seen_error = error;
              done(true);
            })
      );
    }
  };
}

int main()
{
  // Success: the success callback runs, and the response completes
  // normally - proving run_payment_operation's Context::protect wrapping
  // doesn't get in the way of the ordinary happy path.
  {
    auto parser  = new TestParser();
    auto handler = new RunHandler();
    auto request = make_get_request();
    auto provider = make_shared<PaymentProvider>();

    shared_ptr<RunController> controller;

    handler->action = [&](Context& context, function<void(bool)> callback)
    {
      controller = make_shared<RunController>(context);
      controller->done = callback;
      controller->run("test", PaymentIntentParams{});
    };

    SingletonInstantiator<TestRegistry> registry(vector<shared_ptr<const Provider>>{provider});
    SingletonInstantiator<PaymentTestServer> server;
    SingletonInstantiator<Crails::NoSessionStore::Factory> store;
    server->test_setup(vector<Crails::RequestHandler*>{handler}, vector<Crails::RequestParser*>{parser});
    auto connection = make_shared<Connection>(*server, request);
    auto context    = make_shared<TestContext>(*server, *connection);
    auto future_status = context->get_future();

    context->test_run();
    assert(future_status.get() == 200);
    assert(controller->success_seen);
    assert(controller->seen_intent.status == PaymentStatus::Succeeded);
    assert(provider->create_payment_calls == 1);
  }

  // Failure, no on_error given, no catcher registered for it: the
  // exception must propagate out of run_payment_operation's protected
  // callback and be handled by Crails' own fallback (matching the
  // "unhandled exception type" behavior of the exception_catcher tests).
  {
    auto parser  = new TestParser();
    auto handler = new RunHandler();
    auto request = make_get_request();
    auto provider = make_shared<PaymentProvider>();

    provider->on_create_payment = []() { throw Declined("test", "invalid_cvc", "the card was declined"); };

    shared_ptr<RunController> controller;

    handler->action = [&](Context& context, function<void(bool)> callback)
    {
      controller = make_shared<RunController>(context);
      controller->done = callback;
      controller->propagate_on_error = true;
      controller->run("test", PaymentIntentParams{});
    };

    SingletonInstantiator<TestRegistry> registry(vector<shared_ptr<const Provider>>{provider});
    SingletonInstantiator<PaymentTestServer> server;
    SingletonInstantiator<Crails::NoSessionStore::Factory> store;
    server->test_setup(vector<Crails::RequestHandler*>{handler}, vector<Crails::RequestParser*>{parser});
    auto connection = make_shared<Connection>(*server, request);
    auto context    = make_shared<TestContext>(*server, *connection);
    auto future_status = context->get_future();

    context->test_run();
    assert(future_status.get() == 500);
    assert(!controller->success_seen);
    assert(!controller->error_seen); // no on_error was given: it never ran here at all
  }

  // Same failure, but with a Payment::Error catcher registered up front:
  // proves an application can hook payment-specific exceptions into its
  // regular exception-handling setup exactly like any other exception.
  {
    auto parser  = new TestParser();
    auto handler = new RunHandler();
    auto request = make_get_request();
    auto provider = make_shared<PaymentProvider>();

    provider->on_create_payment = []() { throw Declined("test", "insufficient_funds", "insufficient funds"); };

    bool catcher_ran = false;
    shared_ptr<RunController> controller;

    handler->action = [&](Context& context, function<void(bool)> callback)
    {
      controller = make_shared<RunController>(context);
      controller->done = callback;
      controller->propagate_on_error = true;
      controller->run("test", PaymentIntentParams{});
    };

    SingletonInstantiator<TestRegistry> registry(vector<shared_ptr<const Provider>>{provider});
    SingletonInstantiator<PaymentTestServer> server;
    SingletonInstantiator<Crails::NoSessionStore::Factory> store;
    server->register_catcher<Payment::Error>([&](Crails::Context& context, const Payment::Error& e)
    {
      catcher_ran = true;
      server->call_default_handler(context, "Payment::Error", e.what());
    });
    server->test_setup(vector<Crails::RequestHandler*>{handler}, vector<Crails::RequestParser*>{parser});
    auto connection = make_shared<Connection>(*server, request);
    auto context    = make_shared<TestContext>(*server, *connection);
    auto future_status = context->get_future();

    context->test_run();
    assert(future_status.get() == 500);
    assert(catcher_ran);
  }

  // Failure, with an on_error given: the error is handled right there,
  // never reaches the exception pipeline, and the response completes
  // however on_error decides to complete it.
  {
    auto parser  = new TestParser();
    auto handler = new RunHandler();
    auto request = make_get_request();
    auto provider = make_shared<PaymentProvider>();

    provider->on_create_payment = []() { throw NetworkError("test", "connection reset"); };

    shared_ptr<RunController> controller;

    handler->action = [&](Context& context, function<void(bool)> callback)
    {
      controller = make_shared<RunController>(context);
      controller->done = callback;
      controller->propagate_on_error = false; // run_payment_operation gets an on_error this time
      controller->run("test", PaymentIntentParams{});
    };

    SingletonInstantiator<TestRegistry> registry(vector<shared_ptr<const Provider>>{provider});
    SingletonInstantiator<PaymentTestServer> server;
    SingletonInstantiator<Crails::NoSessionStore::Factory> store;
    server->test_setup(vector<Crails::RequestHandler*>{handler}, vector<Crails::RequestParser*>{parser});
    auto connection = make_shared<Connection>(*server, request);
    auto context    = make_shared<TestContext>(*server, *connection);
    auto future_status = context->get_future();

    context->test_run();
    assert(future_status.get() == 200); // on_error completed the response itself
    assert(controller->error_seen);
    assert(!controller->success_seen);

    bool identified = false;
    try { rethrow_exception(controller->seen_error); }
    catch (const NetworkError&) { identified = true; }
    assert(identified);
  }

  return 0;
}
