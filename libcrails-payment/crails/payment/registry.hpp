#pragma once
# include "export.hpp"
# include "provider.hpp"
# include <crails/utils/singleton.hpp>
# include <map>
# include <memory>
# include <string>

namespace Crails
{
  namespace Payment
  {
    class LIBCRAILS_PAYMENT_SYMEXPORT Registry
    {
      SINGLETON(Registry)
    public:
      virtual ~Registry() {}

      static std::shared_ptr<const Provider> find(const std::string& name);
      static const Provider&                 get(const std::string& name);

    protected:
      void add(std::shared_ptr<const Provider> provider);

    private:
      std::map<std::string, std::shared_ptr<const Provider>> providers;
    };
  }
}
