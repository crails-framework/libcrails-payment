#pragma once
#include "export.hpp"
#include "provider.hpp"
#include <crails/utils/singleton.hpp>
#include <map>
#include <memory>
#include <string_view>

namespace Crails
{
  namespace Payment
  {
    class LIBCRAILS_PAYMENT_SYMEXPORT Registry
    {
      SINGLETON(Registry)
    public:
      virtual ~Registry() {}

      static std::shared_ptr<const Provider> find(std::string_view name);
      static const Provider&                 get(std::string_view name);
      static std::vector<std::string_view>   list();

    protected:
      void add(std::shared_ptr<const Provider> provider);

    private:
      std::map<std::string_view, std::shared_ptr<const Provider>> providers;
    };
  }
}
