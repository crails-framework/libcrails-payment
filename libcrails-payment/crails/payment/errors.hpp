#pragma once
#include "export.hpp"
#include <crails/utils/backtrace.hpp>
#include <crails/datatree.hpp>
#include <string>
#include <string_view>

namespace Crails
{
  namespace Payment
  {
    class LIBCRAILS_PAYMENT_SYMEXPORT Error : public boost_ext::runtime_error
    {
    public:
      Error(std::string_view provider, const std::string& message) :
        boost_ext::runtime_error(message), provider_name(std::move(provider))
      {
      }

      std::string_view provider() const { return provider_name; }

    private:
      std::string_view provider_name;
    };

    class LIBCRAILS_PAYMENT_SYMEXPORT CurrencyMismatch : public Error
    {
    public:
      CurrencyMismatch(const std::string& lhs, const std::string& rhs);
    };

    class LIBCRAILS_PAYMENT_SYMEXPORT RequestError : public Error
    {
    public:
      RequestError(std::string_view provider, std::string code, const std::string& message, Data raw) :
        Error(provider, message), error_code(std::move(code))
      {
        raw_response.as_data().merge(raw);
      }

      RequestError(std::string_view provider, std::string code, const std::string& message) :
        Error(provider, message), error_code(std::move(code))
      {
      }

      const std::string& code() const { return error_code; }
      Data raw() { return raw_response.as_data(); }

    private:
      std::string error_code;
      DataTree    raw_response;
    };

    class LIBCRAILS_PAYMENT_SYMEXPORT Declined : public RequestError
    {
    public:
      using RequestError::RequestError;
    };

    class LIBCRAILS_PAYMENT_SYMEXPORT NetworkError : public Error
    {
    public:
      using Error::Error;
    };

    // invalid signature on webhook payload, wrong API key
    class LIBCRAILS_PAYMENT_SYMEXPORT AuthenticationError : public Error
    {
    public:
      using Error::Error;
    };

    // logic error, requested a feature not implemented by provider
    class LIBCRAILS_PAYMENT_SYMEXPORT UnsupportedOperation : public Error
    {
    public:
      using Error::Error;
    };
  }
}
