#pragma once
# include "provider.hpp"
# include <crails/context.hpp>
# include <crails/controller.hpp>
# include <exception>
# include <functional>

namespace Crails
{
  namespace Payment
  {
    template<typename SUPER = Crails::Controller>
    class PaymentController : public SUPER
    {
    public:
      PaymentController(Context& context) : SUPER(context)
      {
      }

    protected:
      template<typename T>
      void run_payment_operation(
        std::function<void(Result<T>)>          start,
        std::function<void(const T&)>           on_success,
        std::function<void(std::exception_ptr)> on_error = nullptr)
      {
        auto self = SUPER::shared_from_this();

        start([this, self, on_success, on_error](const T& result, std::exception_ptr error)
        {
          SUPER::context.protect([result, error, on_success, on_error]()
          {
            if (error)
            {
              if (on_error)
                on_error(error);
              else
                std::rethrow_exception(error);
            }
            else
              on_success(result);
          });
        });
      }
    };
  }
}
