#pragma once
# include "../payment/provider.hpp"
# include "../payment/errors.hpp"
# include <atomic>
# include <functional>
# include <map>
# include <mutex>
# include <sstream>
# include <string>

namespace Crails
{
  namespace Tests
  {
    class PaymentProvider : public Payment::Provider
    {
    public:
      PaymentProvider(std::string name = "test") : Payment::Provider(std::move(name)) {}

      std::function<void()> on_create_customer;
      std::function<void()> on_create_payment;
      std::function<void()> on_capture_payment;
      std::function<void()> on_cancel_payment;
      std::function<void()> on_fetch_payment;
      std::function<void()> on_refund;
      std::function<void()> on_create_mandate;
      std::function<void()> on_fetch_mandate;
      std::function<void()> on_verify_webhook;

      Payment::PaymentStatus      next_status = Payment::PaymentStatus::Succeeded;
      std::optional<std::string> next_redirect_url;
      bool                        supports_mandates    = false;
      Payment::MandateStatus      next_mandate_status  = Payment::MandateStatus::PendingConfirmation;
      Payment::WebhookEvent       next_webhook_event; // handed back verbatim by verify_webhook

      mutable unsigned int create_customer_calls = 0;
      mutable unsigned int create_payment_calls  = 0;
      mutable unsigned int capture_payment_calls = 0;
      mutable unsigned int cancel_payment_calls  = 0;
      mutable unsigned int fetch_payment_calls   = 0;
      mutable unsigned int refund_calls          = 0;
      mutable unsigned int create_mandate_calls  = 0;
      mutable unsigned int fetch_mandate_calls   = 0;
      mutable unsigned int verify_webhook_calls  = 0;

      Payment::Customer create_customer(const Payment::CustomerParams& params) const override
      {
        ++create_customer_calls;
        if (on_create_customer)
          on_create_customer();

        Payment::Customer customer;

        customer.id    = next_id("test_cus");
        customer.email = params.email;
        customer.name  = params.name;
        customer.phone = params.phone;
        return customer;
      }

      void create_customer_async(const Payment::CustomerParams& params, Payment::Result<Payment::Customer> callback) const noexcept override
      {
        run_async<Payment::Customer>([this, params]() { return create_customer(params); }, callback);
      }

      Payment::PaymentIntent create_payment(const Payment::PaymentIntentParams& params) const override
      {
        ++create_payment_calls;
        if (on_create_payment)
          on_create_payment();

        std::lock_guard<std::mutex> guard(mtx);
        Payment::PaymentIntent      intent;

        intent.id            = next_id("test_pi");
        intent.provider_name = name();
        intent.amount        = params.amount;
        intent.status        = next_status;
        intent.metadata      = params.metadata;
        if (next_status == Payment::PaymentStatus::RequiresAction)
          intent.redirect_url = next_redirect_url;
        payments[intent.id] = intent;
        return intent;
      }

      void create_payment_async(const Payment::PaymentIntentParams& params, Payment::Result<Payment::PaymentIntent> callback) const noexcept override
      {
        run_async<Payment::PaymentIntent>([this, params]() { return create_payment(params); }, callback);
      }

      Payment::PaymentIntent capture_payment(const std::string& payment_intent_id, std::optional<Payment::Amount> amount) const override
      {
        ++capture_payment_calls;
        if (on_capture_payment)
          on_capture_payment();

        std::lock_guard<std::mutex> guard(mtx);
        auto&                       intent = find_payment(payment_intent_id);

        intent.status = Payment::PaymentStatus::Succeeded;
        if (amount)
          intent.amount = *amount;
        return intent;
      }

      void capture_payment_async(const std::string& payment_intent_id, std::optional<Payment::Amount> amount, Payment::Result<Payment::PaymentIntent> callback) const noexcept override
      {
        run_async<Payment::PaymentIntent>([this, payment_intent_id, amount]() { return capture_payment(payment_intent_id, amount); }, callback);
      }

      Payment::PaymentIntent cancel_payment(const std::string& payment_intent_id) const override
      {
        ++cancel_payment_calls;
        if (on_cancel_payment)
          on_cancel_payment();

        std::lock_guard<std::mutex> guard(mtx);
        auto&                       intent = find_payment(payment_intent_id);

        intent.status = Payment::PaymentStatus::Canceled;
        return intent;
      }

      void cancel_payment_async(const std::string& payment_intent_id, Payment::Result<Payment::PaymentIntent> callback) const noexcept override
      {
        run_async<Payment::PaymentIntent>([this, payment_intent_id]() { return cancel_payment(payment_intent_id); }, callback);
      }

      Payment::PaymentIntent fetch_payment(const std::string& payment_intent_id) const override
      {
        ++fetch_payment_calls;
        if (on_fetch_payment)
          on_fetch_payment();

        std::lock_guard<std::mutex> guard(mtx);
        return find_payment(payment_intent_id);
      }

      void fetch_payment_async(const std::string& payment_intent_id, Payment::Result<Payment::PaymentIntent> callback) const noexcept override
      {
        run_async<Payment::PaymentIntent>([this, payment_intent_id]() { return fetch_payment(payment_intent_id); }, callback);
      }

      Payment::Refund refund(const Payment::RefundParams& params) const override
      {
        ++refund_calls;
        if (on_refund)
          on_refund();

        Payment::PaymentIntent intent = fetch_payment(params.payment_intent_id);
        Payment::Refund        result;

        result.id                = next_id("test_re");
        result.payment_intent_id = params.payment_intent_id;
        result.status            = Payment::RefundStatus::Succeeded;
        result.amount            = params.amount.value_or(intent.amount);
        return result;
      }

      void refund_async(const Payment::RefundParams& params, Payment::Result<Payment::Refund> callback) const noexcept override
      {
        run_async<Payment::Refund>([this, params]() { return refund(params); }, callback);
      }

      Payment::Mandate create_mandate(const std::string& customer_id, const std::string& return_url) const override
      {
        ++create_mandate_calls;
        if (on_create_mandate)
          on_create_mandate();
        if (!supports_mandates)
          return Payment::Provider::create_mandate(customer_id, return_url); // throws UnsupportedOperation

        std::lock_guard<std::mutex> guard(mtx);
        Payment::Mandate            mandate;

        mandate.id     = next_id("test_mandate");
        mandate.status = next_mandate_status;
        if (next_mandate_status == Payment::MandateStatus::PendingConfirmation)
          mandate.redirect_url = next_redirect_url;
        mandates[mandate.id] = mandate;
        return mandate;
      }

      Payment::Mandate fetch_mandate(const std::string& mandate_id) const override
      {
        ++fetch_mandate_calls;
        if (on_fetch_mandate)
          on_fetch_mandate();
        if (!supports_mandates)
          return Payment::Provider::fetch_mandate(mandate_id); // throws UnsupportedOperation

        std::lock_guard<std::mutex> guard(mtx);
        auto                        it = mandates.find(mandate_id);

        if (it == mandates.end())
          throw Payment::RequestError(name(), "not_found", "no such mandate: " + mandate_id);
        return it->second;
      }

      Payment::WebhookEvent verify_webhook(std::string_view, const boost::beast::http::fields&) const override
      {
        ++verify_webhook_calls;
        if (on_verify_webhook)
          on_verify_webhook();
        return next_webhook_event;
      }

    private:
      Payment::PaymentIntent& find_payment(const std::string& payment_intent_id) const
      {
        auto it = payments.find(payment_intent_id);

        if (it == payments.end())
          throw Payment::RequestError(name(), "not_found", "no such payment intent: " + payment_intent_id);
        return it->second;
      }

      std::string next_id(const char* prefix) const
      {
        std::ostringstream stream;

        stream << prefix << '_' << ++counter;
        return stream.str();
      }

      template<typename T, typename F>
      static void run_async(F&& fn, Payment::Result<T> callback) noexcept
      {
        try { callback(fn(), nullptr); }
        catch (...) { callback(T(), std::current_exception()); }
      }

      mutable std::mutex                                    mtx;
      mutable std::atomic<unsigned long>                    counter{0};
      mutable std::map<std::string, Payment::PaymentIntent> payments;
      mutable std::map<std::string, Payment::Mandate>       mandates;
    };
  }
}
