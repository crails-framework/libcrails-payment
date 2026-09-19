#pragma once
#include "../provider.hpp"
#include <crails/client.hpp>
#include <functional>
#include <string>

namespace Crails
{
  namespace Payment
  {
    namespace Stripe
    {
      // Crails::Payment::Provider backend for Stripe (https://stripe.com).
      // Talks to Stripe's classic REST API (api.stripe.com) directly over
      // HTTPS using Crails::Ssl::Client - no Stripe SDK dependency, and
      // the same fresh-client-per-call pattern Crails::QueryController
      // uses for outgoing HTTP calls.
      //
      // Scope of this first version:
      // - customers, payment intents (create/capture/cancel/fetch), and
      //   refunds are fully implemented, card-first.
      // - create_mandate/fetch_mandate are NOT implemented: they fall
      //   through to Provider's own default (throws UnsupportedOperation).
      //   Stripe does support SEPA Direct Debit mandates, but modeling
      //   them cleanly needs SetupIntents alongside PaymentIntents, which
      //   is future work - Stripe::Provider is a card/wallet backend for
      //   now, not a SEPA one.
      // - Stripe's card 3DS challenge is driven by Stripe.js client-side
      //   using a client_secret, not a bare redirect URL, so our
      //   abstraction's RequiresAction/redirect_url only fits Stripe's
      //   *redirect-based* payment methods (iDEAL, Bancontact...) - for
      //   those, redirect_url is set from next_action.redirect_to_url.url.
      //   For every PaymentIntent, Stripe's client_secret is also copied
      //   into PaymentIntent::metadata["stripe_client_secret"] regardless,
      //   so an application that does want Stripe.js/Elements client-side
      //   for card 3DS can still get at it.
      // - WebhookEvent::raw is left unset: building a real
      //   Crails::Data from Stripe's JSON would need this library to
      //   depend on whichever package actually defines Data/DataTree,
      //   which isn't available to this implementation.
      class Provider : public Payment::Provider
      {
      public:
        // `webhook_secret` may be left empty if this application doesn't
        // use Stripe webhooks; verify_webhook() throws AuthenticationError
        // if called without one configured. Every argument is captured at
        // construction time and never modified afterward - consistent
        // with every Payment::Provider operation being const.
        Provider(std::string secret_key, std::string webhook_secret = "", std::string name = "stripe");

        Customer create_customer(const CustomerParams&) const override;
        void create_customer_async(const CustomerParams&, Result<Customer>) const noexcept override;

        PaymentIntent create_payment(const PaymentIntentParams&) const override;
        void create_payment_async(const PaymentIntentParams&, Result<PaymentIntent>) const noexcept override;

        PaymentIntent capture_payment(const std::string&, std::optional<Amount>) const override;
        void capture_payment_async(const std::string&, std::optional<Amount>, Result<PaymentIntent>) const noexcept override;

        PaymentIntent cancel_payment(const std::string&) const override;
        void cancel_payment_async(const std::string&, Result<PaymentIntent>) const noexcept override;

        PaymentIntent fetch_payment(const std::string&) const override;
        void fetch_payment_async(const std::string&, Result<PaymentIntent>) const noexcept override;

        Refund refund(const RefundParams&) const override;
        void refund_async(const RefundParams&, Result<Refund>) const noexcept override;

        WebhookEvent verify_webhook(std::string_view body, const boost::beast::http::fields& headers) const override;

      protected:
        // These are `protected` rather than `private` specifically so a
        // test double can expose them (via a `using` declaration) and
        // exercise Stripe's JSON-to-our-types mapping directly, without
        // needing real network access to api.stripe.com - see
        // tests/stripe_mapping/driver.cpp.
        [[noreturn]] void raise_for_status(const Client::Response& response) const;

        PaymentIntent parse_payment_intent(Data) const;
        Customer      parse_customer(Data) const;
        Refund        parse_refund(Data) const;
        WebhookEvent  parse_webhook_event(DataTree) const;

      private:
        std::string secret_key;
        std::string webhook_secret;

        Client::Request  build_request(HttpVerb verb, const std::string& target, const std::string& body) const;
        Client::Response send(HttpVerb verb, const std::string& target, const std::string& body) const;
        void              send_async(HttpVerb verb, const std::string& target, const std::string& body, std::function<void(const Client::Response&, boost::beast::error_code)> callback) const;

        // Shared plumbing between every sync/async pair: send the
        // request, and on a 2xx response hand the parsed JSON body to
        // `parse_result`; on anything else, throw/report the right
        // Payment::Error subclass via raise_for_status.
        template<typename T>
        T run(HttpVerb verb, const std::string& target, const std::string& body, std::function<T(Data)> parse_result) const;

        template<typename T>
        void run_async(HttpVerb verb, const std::string& target, const std::string& body, std::function<T(Data)> parse_result, Result<T> callback) const noexcept;
      };
    }
  }
}
