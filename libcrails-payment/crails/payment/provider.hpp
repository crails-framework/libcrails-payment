#pragma once
#include "export.hpp"
#include "amount.hpp"
#include "customer.hpp"
#include "payment_method.hpp"
#include "payment_intent.hpp"
#include "mandate.hpp"
#include "refund.hpp"
#include "webhook_event.hpp"
#include <boost/beast/http/fields.hpp>
#include <exception>
#include <functional>
#include <optional>
#include <string>
#include <string_view>

namespace Crails
{
  namespace Payment
  {
    // NOTE: Implementations must be thread-safe.
    template<typename T>
    using Result = std::function<void(const T& result, std::exception_ptr error)>;

    class LIBCRAILS_PAYMENT_SYMEXPORT Provider
    {
    public:
      Provider(std::string name) : provider_name(std::move(name)) {}
      virtual ~Provider() {}

      std::string_view   name() const { return std::string_view(provider_name); }

      virtual Customer      create_customer(const CustomerParams&) const = 0;
      virtual void          create_customer_async(const CustomerParams&, Result<Customer>) const noexcept = 0;
      virtual PaymentIntent create_payment(const PaymentIntentParams&) const = 0;
      virtual void          create_payment_async(const PaymentIntentParams&, Result<PaymentIntent>) const noexcept = 0;

      // `amount` lets a partial capture happen when the intent was created
      // with capture_immediately = false; leave empty to capture in full.
      virtual PaymentIntent capture_payment(const std::string& payment_intent_id, std::optional<Amount> amount = std::nullopt) const = 0;
      virtual void          capture_payment_async(const std::string& payment_intent_id, std::optional<Amount> amount, Result<PaymentIntent>) const noexcept = 0;
      virtual PaymentIntent cancel_payment(const std::string& payment_intent_id) const = 0;
      virtual void          cancel_payment_async(const std::string& payment_intent_id, Result<PaymentIntent>) const noexcept = 0;
      virtual PaymentIntent fetch_payment(const std::string& payment_intent_id) const = 0;
      virtual void          fetch_payment_async(const std::string& payment_intent_id, Result<PaymentIntent>) const noexcept = 0;
      virtual Refund        refund(const RefundParams&) const = 0;
      virtual void          refund_async(const RefundParams&, Result<Refund>) const noexcept = 0;

      virtual Mandate       create_mandate(const std::string& customer_id, const std::string& return_url) const;
      virtual void          create_mandate_async(const std::string& customer_id, const std::string& return_url, Result<Mandate>) const noexcept;
      virtual Mandate       fetch_mandate(const std::string& mandate_id) const;
      virtual void          fetch_mandate_async(const std::string& mandate_id, Result<Mandate>) const noexcept;

      virtual WebhookEvent  verify_webhook(std::string_view body, const boost::beast::http::fields& headers) const = 0;
      virtual void          verify_webhook_async(std::string body, boost::beast::http::fields headers, Result<WebhookEvent> callback) const noexcept;

    protected:
      [[noreturn]] void throw_unsupported(const std::string& operation) const;

    private:
      const std::string provider_name;
    };
  }
}
