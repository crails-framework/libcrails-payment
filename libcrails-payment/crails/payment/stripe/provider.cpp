#include "provider.hpp"
#include "webhook_signature.hpp"
#include "../errors.hpp"
#include <crails/url.hpp>
#include <crails/utils/semantics.hpp>
#include <algorithm>
#include <cctype>
#include <set>

using namespace std;
using namespace Crails;
using namespace Crails::Payment;
using namespace Crails::Payment::Stripe;

static std::string customer_params_to_form_data(const CustomerParams& params) noexcept
{
  DataTree body;

  if (params.email.has_value())
    body["email"] = *params.email;
  if (params.name.has_value())
    body["name"]  = *params.name;
  if (params.phone.has_value())
    body["phone"] = *params.phone;
  return params2cgi(body.as_data());
}

static std::string payment_intent_params_to_form_data(const PaymentIntentParams& params) noexcept
{
  DataTree body;
  Data metadata = body["metadata"];

  body["amount"]         = params.amount.value;
  body["currency"]       = Crails::lowercase(params.amount.currency_code);
  body["capture_method"] = params.capture_immediately ? "automatic" : "manual";
  if (params.customer_id.has_value())
    body["customer"]     = *params.customer_id;
  if (params.description.has_value())
    body["description"]  = *params.description;
  if (params.return_url.has_value())
    body["return_url"]   = *params.return_url;
  if (params.payment_method_id)
  {
    body["payment_method"] = *params.payment_method_id;
    body["confirm"] = "true";
  }
  if (!params.metadata.empty())
  {
    for (auto it = params.metadata.begin() ; it != params.metadata.end() ; ++it)
      metadata[it->first] = it->second;
  }
  return params2cgi(body.as_data());
}

static std::string refund_params_to_form_data(const RefundParams& params) noexcept
{
  static const set<string> stripe_reasons = {"duplicate", "fraudulent", "requested_by_customer"};
  DataTree                 body;

  body["payment_intent"] = params.payment_intent_id;
  if (params.amount)
    body["amount"] = static_cast<long long>(params.amount->value);
  if (params.reason)
  {
    if (stripe_reasons.count(*params.reason))
      body["reason"] = *params.reason;
    else
      body["metadata"]["reason"] = *params.reason;
  }
  return params2cgi(body.as_data());
}

Stripe::Provider::Provider(string secret_key, string webhook_secret, string name) :
  Payment::Provider(move(name)),
  secret_key(move(secret_key)),
  webhook_secret(move(webhook_secret))
{
}

Client::Request Stripe::Provider::build_request(HttpVerb verb, const string& target, const string& body) const
{
  Client::Request request(verb, target, 11);

  request.set(HttpHeader::host, "api.stripe.com");
  request.set(HttpHeader::authorization, "Bearer " + secret_key);
  request.set(HttpHeader::user_agent, "libcrails-payment-stripe");
  request.set(HttpHeader::accept, "application/json");
  if (!body.empty())
  {
    request.set(HttpHeader::content_type, "application/x-www-form-urlencoded");
    request.body() = body;
  }
  request.prepare_payload();
  return request;
}

Client::Response Stripe::Provider::send(HttpVerb verb, const string& target, const string& body) const
{
  Ssl::Client client("api.stripe.com", 443);
  auto        request = build_request(verb, target, body);

  client.connect();
  return client.query(request);
}

void Stripe::Provider::send_async(HttpVerb verb, const string& target, const string& body, function<void(const Client::Response&, boost::beast::error_code)> callback) const
{
  auto client  = make_shared<Ssl::Client>("api.stripe.com", 443);
  auto request = build_request(verb, target, body);

  // connect() is synchronous even on the async path - the same is true of
  // Crails::QueryController::make_async_http_query, which this mirrors.
  // Any exception it throws propagates out of send_async() itself, to be
  // caught by run_async()'s caller.
  client->connect();
  client->async_query(request, [client, callback](const Client::Response& response, boost::beast::error_code ec)
  {
    callback(response, ec);
    client->disconnect();
  });
}

void Stripe::Provider::raise_for_status(const Client::Response& response) const
{
  DataTree root;

  try { root.from_json(response.body()); }
  catch (...) { /* fall through: report status/body as-is below */ }

  // Stripe's error responses are shaped {"error": {"type", "code",
  // "message", ...}} - not flat at the root.
  Data   error   = root.as_data()["error"];
  string type    = error["type"].defaults_to<string>("");
  string code    = error["code"].defaults_to<string>("");
  string message = error["message"].defaults_to<string>("");

  if (message.empty())
    message = "Stripe returned HTTP " + std::to_string(response.result_int());
  if (response.result() == HttpStatus::unauthorized)
    throw AuthenticationError(name(), message);
  if (type == "card_error")
    throw Declined(name(), code.empty() ? "card_declined" : code, message, error);
  throw RequestError(name(), code.empty() ? (type.empty() ? "stripe_error" : type) : code, message, error);
}

template<typename RESPONSE, typename T>
static T response_parser(const RESPONSE& response, function<T(Data)> parse_result)
{
  DataTree json;

  json.from_json(response.body());
  return parse_result(json.as_data());
}

template<typename T>
T Stripe::Provider::run(HttpVerb verb, const string& target, const string& body, function<T(Data)> parse_result) const
{
  Client::Response response;

  try { response = send(verb, target, body); }
  catch (const std::exception& e) { throw NetworkError(name(), e.what()); }

  if (response.result_int() >= 200 && response.result_int() < 300)
    return response_parser(response, parse_result);
  raise_for_status(response);
}

template<typename T>
void Stripe::Provider::run_async(HttpVerb verb, const string& target, const string& body, function<T(Data)> parse_result, Result<T> callback) const noexcept
{
  try
  {
    send_async(verb, target, body, [this, parse_result, callback](const Client::Response& response, boost::beast::error_code ec)
    {
      try
      {
        if (ec)
          throw NetworkError(name(), ec.message());
        if (response.result_int() >= 200 && response.result_int() < 300)
          callback(response_parser(response, parse_result), nullptr);
        else
          raise_for_status(response);
      }
      catch (...) { callback(T(), current_exception()); }
    });
  }
  catch (...) { callback(T(), current_exception()); }
}

// Customers -----------------------------------------------------------

Customer Stripe::Provider::parse_customer(Data json) const
{
  Customer customer;
  Data     email = json["email"];
  Data     name  = json["name"];
  Data     phone = json["phone"];

  customer.id = json["id"].defaults_to<string>("");
  if (!email.is_null())
    customer.email = email.as<string>();
  if (!name.is_null())
    customer.name = name.as<string>();
  if (!phone.is_null())
    customer.phone = phone.as<string>();
  return customer;
}

Customer Stripe::Provider::create_customer(const CustomerParams& params) const
{
  return run<Customer>(
    HttpVerb::post, "/v1/customers",
    customer_params_to_form_data(params),
    [this](Data j) { return parse_customer(j); }
  );
}

void Stripe::Provider::create_customer_async(const CustomerParams& params, Result<Customer> callback) const noexcept
{
  try
  {
    auto form_data = customer_params_to_form_data(params);

    run_async<Customer>(
      HttpVerb::post,
      "/v1/customers",
      form_data,
      [this](Data j) { return parse_customer(j); },
      callback
    );
  }
  catch (...)
  {
    callback(Customer(), current_exception());
  }
}

// Payments --------------------------------------------------------------

static std::map<string_view, PaymentStatus> payment_status_map = {
  {"requires_payment_method", PaymentStatus::RequiresPaymentMethod},
  {"requires_confirmation",   PaymentStatus::RequiresPaymentMethod},
  {"requires_action",         PaymentStatus::RequiresAction},
  {"processing",              PaymentStatus::Processing},
  {"requires_capture",        PaymentStatus::Processing},
  {"canceled",                PaymentStatus::Canceled},
  {"succeeded",               PaymentStatus::Succeeded}
};

static PaymentStatus payment_status_from_string(const string_view value)
{
  auto it = payment_status_map.find(value);

  return it != payment_status_map.end()
    ? it->second
    : PaymentStatus::Processing;
}

PaymentIntent Stripe::Provider::parse_payment_intent(Data json) const
{
  PaymentIntent intent;
  string        raw_status    = json["status"].defaults_to<string>("");
  string        client_secret = json["client_secret"].defaults_to<string>("");
  Data          amount        = json["amount"];
  Data          currency      = json["currency"];

  intent.id                        = json["id"].defaults_to<string>("");
  intent.provider_name             = name();
  intent.amount.value              = amount.defaults_to<long long>(0);
  intent.amount.currency_code      = Crails::uppercase(currency.defaults_to<string>("EUR"));
  intent.metadata["stripe_status"] = raw_status;

  if (!client_secret.empty())
    intent.metadata["stripe_client_secret"] = client_secret;

  // Stripe's own status set doesn't map 1:1 onto ours: "requires_capture"
  // (authorized, awaiting our own capture_payment() call) is closest in
  // spirit to Processing (something still needs to happen before this is
  // final, but it isn't an interactive step for the payer); an
  // unrecognized/future status is conservatively treated the same way
  // rather than guessed at. "requires_payment_method" means either
  // "nothing attempted yet" or "the last attempt failed and Stripe kept
  // the intent alive for a retry" - last_payment_error tells them apart.
  if (raw_status == "requires_payment_method" && !json["last_payment_error"].is_null())
  {
    intent.status         = PaymentStatus::Failed;
    intent.failure_reason = json["last_payment_error"]["message"].defaults_to<string>("");
  }
  else
    intent.status = payment_status_from_string(raw_status);

  if (intent.status == PaymentStatus::RequiresAction)
  {
    Data next_action = json["next_action"];

    // Only "redirect_to_url" fits our redirect_url field; other
    // next_action types (chiefly "use_stripe_sdk", for card 3DS) need
    // Stripe.js client-side and have no bare URL to redirect to - the
    // client_secret captured above is what a client-side integration
    // needs for those instead.
    // TODO: do we support all next_actions we could receive ?
    if (next_action["type"].defaults_to<string>("") == "redirect_to_url")
      intent.redirect_url = next_action["redirect_to_url"]["url"].defaults_to<string>("");
  }
  return intent;
}

PaymentIntent Stripe::Provider::create_payment(const PaymentIntentParams& params) const
{
  return run<PaymentIntent>(
    HttpVerb::post, "/v1/payment_intents",
    payment_intent_params_to_form_data(params),
    [this](Data j) { return parse_payment_intent(j); }
  );
}

void Stripe::Provider::create_payment_async(const PaymentIntentParams& params, Result<PaymentIntent> callback) const noexcept
{
  try
  {
    auto form_data = payment_intent_params_to_form_data(params);

    run_async<PaymentIntent>(
      HttpVerb::post, "/v1/payment_intents",
      form_data,
      [this](Data j) { return parse_payment_intent(j); },
      callback
    );
  }
  catch (...)
  {
    callback(PaymentIntent(), current_exception());
  }
}

PaymentIntent Stripe::Provider::capture_payment(const string& payment_intent_id, optional<Amount> amount) const
{
  DataTree body;

  if (amount)
    body["amount_to_capture"] = static_cast<long long>(amount->value);
  return run<PaymentIntent>(
    HttpVerb::post, "/v1/payment_intents/" + payment_intent_id + "/capture",
    params2cgi(body.as_data()), [this](Data j) { return parse_payment_intent(j); }
  );
}

void Stripe::Provider::capture_payment_async(const string& payment_intent_id, optional<Amount> amount, Result<PaymentIntent> callback) const noexcept
{
  try
  {
    DataTree body;

    if (amount)
      body["amount_to_capture"] = static_cast<long long>(amount->value);
    run_async<PaymentIntent>(
      HttpVerb::post, "/v1/payment_intents/" + payment_intent_id + "/capture",
      params2cgi(body.as_data()),
      [this](Data j) { return parse_payment_intent(j); },
      callback
    );
  }
  catch (...)
  {
    callback(PaymentIntent(), current_exception());
  }
}

PaymentIntent Stripe::Provider::cancel_payment(const string& payment_intent_id) const
{
  return run<PaymentIntent>(HttpVerb::post, "/v1/payment_intents/" + payment_intent_id + "/cancel", "", [this](Data j) { return parse_payment_intent(j); });
}

void Stripe::Provider::cancel_payment_async(const string& payment_intent_id, Result<PaymentIntent> callback) const noexcept
{
  try
  {
    run_async<PaymentIntent>(HttpVerb::post, "/v1/payment_intents/" + payment_intent_id + "/cancel", "", [this](Data j) { return parse_payment_intent(j); }, callback);
  }
  catch (...)
  {
    callback(PaymentIntent(), current_exception());
  }
}

PaymentIntent Stripe::Provider::fetch_payment(const string& payment_intent_id) const
{
  return run<PaymentIntent>(HttpVerb::get, "/v1/payment_intents/" + payment_intent_id, "", [this](Data j) { return parse_payment_intent(j); });
}

void Stripe::Provider::fetch_payment_async(const string& payment_intent_id, Result<PaymentIntent> callback) const noexcept
{
  try
  {
    run_async<PaymentIntent>(HttpVerb::get, "/v1/payment_intents/" + payment_intent_id, "", [this](Data j) { return parse_payment_intent(j); }, callback);
  }
  catch (...)
  {
    callback(PaymentIntent(), current_exception());
  }
}

// Refunds -----------------------------------------------------------------

Refund Stripe::Provider::parse_refund(Data json) const
{
  Refund refund;
  string status = json["status"].defaults_to<string>("");

  refund.id                   = json["id"].defaults_to<string>("");
  refund.payment_intent_id    = json["payment_intent"].defaults_to<string>("");
  refund.amount.value         = json["amount"].defaults_to<long long>(0);
  refund.amount.currency_code = Crails::uppercase(json["currency"].defaults_to<string>("EUR"));

  if (status == "succeeded")
    refund.status = RefundStatus::Succeeded;
  else if (status == "failed")
    refund.status = RefundStatus::Failed;
  else
    refund.status = RefundStatus::Pending; // "pending", "requires_action", or unknown/future
  return refund;
}

Refund Stripe::Provider::refund(const RefundParams& params) const
{
  return run<Refund>(
    HttpVerb::post,
    "/v1/refunds",
    refund_params_to_form_data(params),
    [this](Data j) { return parse_refund(j); }
  );
}

void Stripe::Provider::refund_async(const RefundParams& params, Result<Refund> callback) const noexcept
{
  try
  {
    auto form_data = refund_params_to_form_data(params);

    run_async<Refund>(
      HttpVerb::post,
      "/v1/refunds",
      form_data,
      [this](Data j) { return parse_refund(j); },
      callback
    );
  }
  catch (...)
  {
    callback(Refund(), current_exception());
  }
}

// Webhooks ----------------------------------------------------------------
//
static std::map<string_view, WebhookEventType> webhook_eventtype_map = {
  {"payment_intent.succeeded",       WebhookEventType::PaymentSucceeded},
  {"payment_intent.payment_failed",  WebhookEventType::PaymentFailed},
  {"payment_intent.requires_action", WebhookEventType::PaymentActionRequired},
  {"charge.refunded",                WebhookEventType::RefundUpdated},
  {"refund.updated",                 WebhookEventType::RefundUpdated},
  {"refund.created",                 WebhookEventType::RefundUpdated}
};

static WebhookEventType webhook_event_type_from_string(const string_view value)
{
  auto it = webhook_eventtype_map.find(value);

  return it != webhook_eventtype_map.end()
    ? it->second
    : WebhookEventType::Unknown;
}


WebhookEvent Stripe::Provider::parse_webhook_event(DataTree json) const
{
  WebhookEvent event;
  Data         object      = json["data"]["object"];
  string       object_type = object["object"].defaults_to<string>("");

  event.provider_name = name();
  event.raw_type      = json["type"].defaults_to<string>("");
  if (object_type == "payment_intent")
    event.payment_intent = parse_payment_intent(object);
  else if (object_type == "refund")
    event.refund = parse_refund(object);
  event.type = webhook_event_type_from_string(event.raw_type);
  event.raw  = std::move(json);
  return event;
}

WebhookEvent Stripe::Provider::verify_webhook(string_view body, const boost::beast::http::fields& headers) const
{
  auto     it = headers.find("Stripe-Signature");
  string   signature_header;
  DataTree json;

  if (webhook_secret.empty())
    throw AuthenticationError(name(), "no webhook secret configured for this provider");
  if (it == headers.end())
    throw AuthenticationError(name(), "missing Stripe-Signature header");
  signature_header = it->value();
  if (!verify_webhook_signature(body, signature_header, webhook_secret))
    throw AuthenticationError(name(), "invalid Stripe webhook signature");
  json.from_json(string(body));
  return parse_webhook_event(std::move(json));
}
