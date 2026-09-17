#pragma once
#include <crails/server.hpp>
#include <crails/context.hpp>
#include <crails/request_handler.hpp>
#include <crails/request_parser.hpp>
#include <crails/payment/controller.hpp>
#include <crails/payment/registry.hpp>
#include <functional>
#include <memory>
#include <vector>

// A registry the test can hand an arbitrary, already-built set of
// providers to, so each test block sets up exactly the backend(s) it
// needs - the same way an application lists its own backends directly in
// its own subclass' constructor (see registry.hpp).
class TestRegistry : public Crails::Payment::Registry
{
  SINGLETON_IMPLEMENTATION(TestRegistry, Crails::Payment::Registry)
public:
  TestRegistry(std::vector<std::shared_ptr<const Crails::Payment::Provider>> providers)
  {
    for (auto& provider : providers)
      add(provider);
  }
};

struct PaymentTestServer : public Crails::Server
{
  SINGLETON_IMPLEMENTATION(PaymentTestServer, Crails::Server)
public:
  PaymentTestServer() { set_environment(Crails::Test); }

  template<typename LISTA, typename LISTB>
  void test_setup(const LISTA& handlers, const LISTB& parsers)
  {
    for (auto* handler : handlers) add_request_handler(handler);
    for (auto* parser : parsers)   add_request_parser(parser);
  }

  template<typename EXCEPTION>
  void register_catcher(std::function<void(Crails::Context&, const EXCEPTION&)> fn)
  {
    exception_catcher.add_exception_catcher<EXCEPTION>(fn);
  }

  void call_default_handler(Crails::Context& context, const std::string& name, const std::string& what)
  {
    exception_catcher.default_exception_handler(context, name, what, "");
  }
};

struct TestParser : public Crails::RequestParser
{
  void operator()(Crails::Context&, std::function<void(Crails::RequestParser::Status)> callback) const override
  {
    callback(Crails::RequestParser::Continue);
  }
};

// PaymentController<SUPER> only asks two things of SUPER: a Context&-taking
// constructor, and shared_from_this() - exactly what Crails::Controller (a
// much heavier FlashController stack: rendering, flash messages, CSRF,
// basic auth...) provides among everything else. None of that is relevant
// to testing run_payment_operation's Context::protect wiring, so this
// minimal stand-in is all SUPER needs to be for this test.
struct MinimalController : public std::enable_shared_from_this<MinimalController>
{
  Crails::Context& context;

  MinimalController(Crails::Context& c) : context(c) {}
  virtual ~MinimalController() {}
};

typedef Crails::Payment::PaymentController<MinimalController> TestPaymentControllerBase;

// Runs an arbitrary closure with the live Context, exactly once, as the
// request handler stage of the pipeline. Each test builds its own small
// TestPaymentControllerBase subclass (see driver.cpp) to exercise
// run_payment_operation with whatever scenario it needs, the same way
// ThrowingHandler/TestHandler let each libcrails test script its own
// behavior inline.
struct RunHandler : public Crails::RequestHandler
{
  mutable bool was_called = false;
  std::function<void(Crails::Context&, std::function<void(bool)>)> action;

  RunHandler() : Crails::RequestHandler("run") {}

  void operator()(Crails::Context& context, std::function<void(bool)> callback) const override
  {
    was_called = true;
    action(context, callback);
  }
};

struct TestContext : public Crails::Context
{
  TestContext(const PaymentTestServer& server, Crails::Connection& connection)
    : Crails::Context(server, connection)
  {
  }

  void test_run() { run(); }
};
